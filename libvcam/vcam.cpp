#include <windows.h>
#include <stdio.h>
#include <obs-module.h>
#include "vcam_strm.h"
#include "../utils/tiny-nv12-scale.h"
#include "../utils/shared-memory-queue.h"
#include <exception>

#define DEFAULT_CX 1920
#define DEFAULT_CY 1080
#define DEFAULT_INTERVAL 333333ULL

static DWORD WINAPI VCameraThread(LPVOID lpParameter);

struct VCamera {
	nv12_scale_t scaler;
	uint32_t cx;
	uint32_t cy;
	uint64_t interval;

	video_queue_t *vq;
	//HANDLE mutex;
	HANDLE ccam;
	HANDLE semaphore;
	HANDLE thread;
	bool stoped;

	inline VCamera(video_queue_t *p) : vq(p)
	{
		interval = DEFAULT_INTERVAL;
		SetScale(DEFAULT_CX, DEFAULT_CY);
		//mutex = CreateMutex(NULL, FALSE, NULL);
		//if (!mutex || mutex == INVALID_HANDLE_VALUE)
			//throw "VCamera:CreateMutex failed!";
		stoped = true;
	}

	inline ~VCamera()
	{
		if (!stoped)
			CloseHandle(thread);
	}

	inline void SetScale(int w, int h)
	{
		cx = w;
		cy = h;
		nv12_scale_init(&scaler, TARGET_FORMAT_YUY2, cx, cy, cx, cy);
	}

	bool Start()
	{
		semaphore = CreateSemaphore(NULL, 0, 3, NULL);
		if (!semaphore || semaphore == INVALID_HANDLE_VALUE) {
			blog(LOG_ERROR, "VCamera:CreateSemaphore failed!");
			return false;
		}

		thread = CreateThread(NULL, 0, VCameraThread, this, 0,
				      NULL);
		if (!thread || thread == INVALID_HANDLE_VALUE) {
			blog(LOG_ERROR, "VCamera:CreateThread failed!");
			CloseHandle(semaphore);
			return false;
		}
		SetState(false);
		return true;
	}

	bool GetState()
	{
		bool state;
		//WaitForSingleObject(mutex, INFINITE);
		state = stoped;
		//ReleaseMutex(mutex);
		return state;
	}

	void SetState(bool state)
	{
		//WaitForSingleObject(mutex, INFINITE);
		stoped = state;
		//ReleaseMutex(mutex);
	}

	void Stop()
	{
		SetState(true);
		vcam_video_destroy(ccam);
		//WaitForSingleObject(thread, INFINITE);
		CloseHandle(thread);
		CloseHandle(semaphore);
	}

	void WriteFrame()
	{
		try {
			if (!ReleaseSemaphore(
				    semaphore, // handle to semaphore
				    1,         // increase count by one
				    NULL)) // not interested in previous count
			{
				//throw "ReleaseSemaphore failed!";
				blog(LOG_ERROR,
				     "WriteFrame:ReleaseSemaphore error: %ld\n",
				     GetLastError());
			}
		} catch (std::exception exp){
			blog(LOG_ERROR,
			     "WriteFrame: ReleaseSemaphore error:%s\n", exp.what());
		}
	}
};

static DWORD WINAPI VCameraThread(LPVOID lpParameter)
{
	struct VCamera *pVCam = (struct VCamera *)lpParameter;
	struct vcam_device_info_t *info = NULL;
	int count = 0;
	int unuse = 0;

	unuse = vcam_enum_video_devices(ENUM_TYPE_ALL, &info, &count);
	if (!info) {
		blog(LOG_ERROR,
		     "vcam_enum_video_devices get no devices! errmsg is: %s\n",
		     last_errmsg());
		return -1;
	}
	vcam_free_video_devices(&info); // 销毁内存，防止内存泄漏

	////查找还没被占用的设备(这里占用是指还没有被 vcam_video_create 函数打开的虚拟摄像头)
	unuse = vcam_enum_video_devices(ENUM_TYPE_UNUSED, &info, &count);
	if (!info) {
		blog(LOG_ERROR,
		     "vcam_enum_video_devices nofree devices! errmsg is: %s\n",
		     last_errmsg());
		return -1;
	}

	//遍历设备信息
	for (int i = 0; i < count; ++i) {
		blog(LOG_DEBUG, "UNUSED: name=[%s], desc=[%s], instId=[%s]\n",
		     info[i].device_name, info[i].device_desc,
		     info[i].device_instId);
	}

	///打开设备
	pVCam->ccam = vcam_video_create(info[0].device_name); ////
	if (!pVCam->ccam) {
		blog(LOG_ERROR, "vcam_video_create err=%s\n", last_errmsg());
		return -1;
	}
	////
	vcam_free_video_devices(&info); // 销毁内存，防止内存泄漏

	/// 设置摄像头的分辨率
	struct vcam_video_format_t fmts
		[1]; //其中第一项是输入的图像大小，其他尺寸是为了某些摄像头程序兼容考虑。
	memset(fmts, 0, sizeof(fmts));
	fmts[0].width = pVCam->cx;
	fmts[0].height = pVCam->cy;
	fmts[0].fps = pVCam->interval; //输入屏幕尺寸

	int r = vcam_video_set_format(pVCam->ccam, fmts, 1); ///
	if (r < 0) {
		blog(LOG_ERROR, "set format ERR : %s\n",
		     last_errmsg()); //查询失败原因
	}

	blog(LOG_INFO, "VCameraThread %d\n", GetCurrentThreadId());

	//fill vedio data
	DWORD dwWaitResult;
	while (true) {
		dwWaitResult = WaitForSingleObject(pVCam->semaphore, INFINITE);
		if (pVCam->GetState())
			break;

		uint32_t ln = pVCam->cx * pVCam->cy * 4;
		uint8_t *ptr = new uint8_t[ln];
		uint64_t temp;
		if (video_queue_read(pVCam->vq, &pVCam->scaler, ptr, &temp)) {
			//video_queue_close(pVCam->vq);
			//pVCam->vq = nullptr;
			vcam_video_fill_data(pVCam->ccam, ptr, ln);
		}
		delete[] ptr;
	}

	//vcam_video_destroy(ccam);
	return 0;
}


struct VCamera * g_vcamera;


extern "C" bool vcamera_start(void *p, int w, int h)
{
	g_vcamera = new VCamera((video_queue_t*)p);
	g_vcamera->SetScale(w, h);
	return g_vcamera->Start();
}

extern "C" void vcamera_frame_event()
{
	g_vcamera->WriteFrame();
}

extern "C" void vcamera_stop()
{
	g_vcamera->Stop();
	delete (g_vcamera);
	g_vcamera = NULL;
}

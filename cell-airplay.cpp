#include <objbase.h>

#include <obs-module.h>
#include <obs.hpp>
#include <vector>
#include "utils/netpipe.h"
#include "utils/buffer_util.h"
#include "libairplay/mediaserver.h"
#include "utils/ffmpeg-decode.h"

extern "C" {
#include <libavformat/avformat.h>
}

// airplay mirror resolution support:
// 2560 * 1440
// 1920 * 1080
// 1280 * 720
#define SCREEN_WIDTH 2560
#define SCREEN_HEIGHT 1440

#define AIR_DEVICE_NAME "Cell Receiver"
#define HEADER_SIZE 12
#define NO_PTS UINT64_C(-1)

#define PIPE_PORT 6960

/**/
static void airplay_open(void *cls, char *url, float fPosition,
			 double dPosition);
static void airplay_play(void *cls);
static void airplay_pause(void *cls);
static void airplay_stop(void *cls);
static void airplay_seek(void *cls, long fPosition);
static void airplay_setvolume(void *cls, int volume);
static void airplay_showphoto(void *cls, unsigned char *data, long long size);
static long airplay_getduration(void *cls);
static long airplay_getpostion(void *cls);
static int airplay_isplaying(void *cls);
static int airplay_ispaused(void *cls);
//...
static void audio_init(void *cls, int bits, int channels, int samplerate,
		       int isaudio);
static void audio_process(void *cls, const void *buffer, int buflen,
			  uint64_t timestamp, uint32_t seqnum);
static void audio_destory(void *cls);
static void audio_setvolume(void *cls, int volume); //1-100
static void audio_setmetadata(void *cls, const void *buffer, int buflen);
static void audio_setcoverart(void *cls, const void *buffer, int buflen);
static void audio_flush(void *cls);
//...
static void mirroring_play(void *cls, int width, int height, const void *buffer,
			   int buflen, int payloadtype, uint64_t timestamp);
static void mirroring_process(void *cls, const void *buffer, int buflen,
			      int payloadtype, uint64_t timestamp);
static void mirroring_stop(void *cls);
static void mirroring_live(void *cls);
//...
static void sdl_audio_callback(void *cls, uint8_t *stream, int len);
/**/

struct EncodedData {
	long long lastStartTime = 0;
	long long lastStopTime = 0;
	std::vector<unsigned char> bytes;
};

class Decoder {
	struct ffmpeg_decode decode;

public:
	inline Decoder() { memset(&decode, 0, sizeof(decode)); }
	inline ~Decoder() { ffmpeg_decode_free(&decode); }

	inline operator ffmpeg_decode *() { return &decode; }
	inline ffmpeg_decode *operator->() { return &decode; }
};

unsigned __stdcall threadRcv(void * para);

struct AirInput {
	obs_source_t *source;
	EncodedData encodedVideo;
	PipeServer obs_pipe;
	PipeClient air_pipe;

	video_range_type range = VIDEO_RANGE_DEFAULT;
	Decoder video_decoder;
	obs_source_frame2 frame;
	bool flip = false;

	HANDLE obs_rcv_pro;

	uint32_t width = SCREEN_WIDTH;
	uint32_t height = SCREEN_HEIGHT;

	AirInput(obs_source_t *source_, obs_data_t *settings)
		: source(source_),
		  obs_pipe(PipeServer(PIPE_PORT)),
		  air_pipe(PipeClient(PIPE_PORT))
	{
		memset(&frame, 0, sizeof(frame));
	}
	~AirInput() { Stop(); }

	void Receive(unsigned char *ptr, size_t size, long long pts)
	{
		int roll = 0;
		long long startTime, stopTime = 0;
		bool hasTime = pts != NO_PTS ? true : false;
		startTime = pts != NO_PTS ? pts : AV_NOPTS_VALUE;

		if (hasTime) {
			OnEncodedVideoData(AV_CODEC_ID_H264,
					   encodedVideo.bytes.data(),
					   encodedVideo.bytes.size(),
					   startTime);

			encodedVideo.bytes.resize(0);
			encodedVideo.lastStartTime = startTime;
			encodedVideo.lastStopTime = stopTime;
		}

		encodedVideo.bytes.insert(encodedVideo.bytes.end(),
					  (unsigned char *)ptr,
					  (unsigned char *)ptr + size);
	}

	void OnEncodedVideoData(enum AVCodecID id, unsigned char *data,
				size_t size, long long ts)
	{
		/* If format changes, free and allow it to recreate the decoder */
		if (ffmpeg_decode_valid(video_decoder) &&
		    video_decoder->codec->id != id) {
			ffmpeg_decode_free(video_decoder);
		}

		if (!ffmpeg_decode_valid(video_decoder)) {
			/* Only use MJPEG hardware decoding on resolutions higher
		 * than 1920x1080.  The reason why is because we want to strike
		 * a reasonable balance between hardware and CPU usage. */
			bool useHW = false;
			if (ffmpeg_decode_init(video_decoder, id, useHW) < 0) {
				blog(LOG_WARNING,
				     "Could not initialize video decoder");
				return;
			}
		}

		bool got_output;
		bool success = ffmpeg_decode_video(video_decoder, data, size,
						   &ts, range, &frame,
						   &got_output);
		if (!success) {
			blog(LOG_WARNING, "Error decoding video");
			return;
		}

		if (got_output) {
			frame.timestamp = (uint64_t)ts * 100;
			if (flip)
				frame.flip = !frame.flip;
#if LOG_ENCODED_VIDEO_TS
			blog(LOG_DEBUG, "video ts: %llu", frame.timestamp);
#endif
			obs_source_output_video2(source, &frame);
		}
	}

	int Start()
	{
		int ret = 0;
		if (obs_pipe.Start() < 0) {
			ret = -1;
			goto END;
		}
		if (air_pipe.Start() < 0) {
			ret = -2;
			goto STOPOBS;
		}
		if (start_airfunction() < 0) {
			ret = -3;
			goto STOPAIR;
		}

		obs_rcv_pro = (HANDLE)_beginthreadex(NULL, 0, threadRcv, this,
						     0, NULL);

		return 0;
	STOPAIR:
		air_pipe.Stop();
	STOPOBS:
		obs_pipe.Stop();
	END:
		return ret;
	}

	void Stop()
	{
		if (obs_rcv_pro) {
			WaitForSingleObject(obs_rcv_pro, INFINITE);
			CloseHandle(obs_rcv_pro);
		}
		
		stopMediaServer();
		air_pipe.Stop();
		obs_pipe.Stop();
	}

	int start_airfunction()
	{
		airplay_callbacks_t *ao = new airplay_callbacks_t();
		if (!ao) {
			blog(LOG_ERROR, "new airplay_callbacks_t failed!\n");
			return -1;
		}
		memset(ao, 0, sizeof(airplay_callbacks_t));
		ao->cls = this;
		ao->AirPlayPlayback_Open = airplay_open;
		ao->AirPlayPlayback_Play = airplay_play;
		ao->AirPlayPlayback_Pause = airplay_pause;
		ao->AirPlayPlayback_Stop = airplay_stop;
		ao->AirPlayPlayback_Seek = airplay_seek;
		ao->AirPlayPlayback_SetVolume = airplay_setvolume;
		ao->AirPlayPlayback_ShowPhoto = airplay_showphoto;
		ao->AirPlayPlayback_GetDuration = airplay_getduration;
		ao->AirPlayPlayback_GetPostion = airplay_getpostion;
		ao->AirPlayPlayback_IsPlaying = airplay_isplaying;
		ao->AirPlayPlayback_IsPaused = airplay_ispaused;
		//...
		ao->AirPlayAudio_Init = audio_init;
		ao->AirPlayAudio_Process = audio_process;
		ao->AirPlayAudio_destroy = audio_destory;
		ao->AirPlayAudio_SetVolume = audio_setvolume;
		ao->AirPlayAudio_SetMetadata = audio_setmetadata;
		ao->AirPlayAudio_SetCoverart = audio_setcoverart;
		ao->AirPlayAudio_Flush = audio_flush;
		//...
		ao->AirPlayMirroring_Play = mirroring_play;
		ao->AirPlayMirroring_Process = mirroring_process;
		ao->AirPlayMirroring_Stop = mirroring_stop;
		ao->AirPlayMirroring_Live = mirroring_live;

		int r = startMediaServer((char *)AIR_DEVICE_NAME, SCREEN_WIDTH,
					 SCREEN_HEIGHT, ao);
		if (r < 0) {
			blog(LOG_ERROR ,"start media server error code : -1\n");
			return -2;
		} else if (r > 0) {
			blog(LOG_ERROR ,"start media server error code : 1\n");
			return -3;
		}
		return 0;
	}
};

unsigned __stdcall threadRcv(void *para)
{
	AirInput *input = reinterpret_cast<AirInput *>(para);
	if (!input)
		return -1;
	uint8_t header[HEADER_SIZE];
	memset(header, 0, HEADER_SIZE);
	while (true) {
		int ret = input->obs_pipe.Recive((char *)header, HEADER_SIZE);
		if (ret != HEADER_SIZE) {
			blog(LOG_ERROR,
			     "obs pipe recive head length invalid: %d", ret);
			return -2;
		}
		uint64_t pts = buffer_read64be(header);
		uint32_t len = buffer_read32be(&header[8]);

		char *data = new char[len];
		ret = input->obs_pipe.Recive(data, len);
		int count = 0;
		while (ret < len) {
			if (count > 2) {
				blog(LOG_ERROR,
				     "obs pipe recive data length invalid. expected:%d, facted:%d",
				     len, ret);
				delete[] data;
				return -3;
			}
			blog(LOG_WARNING,
			     "obs pipe recive data continue... expected:%d, facted:%d", len, ret);
			ret += input->obs_pipe.Recive(&data[ret], len - ret);
			count++;
		}
		input->Receive((uint8_t*)data, len, pts);
		delete[] data;
	}
}

/**/
/*airplay*/
void airplay_open(void *cls, char *url, float fPosition, double dPosition)
{
	UNUSED_PARAMETER(cls);
	UNUSED_PARAMETER(url);
	UNUSED_PARAMETER(fPosition);
	UNUSED_PARAMETER(dPosition);
}

void airplay_play(void *cls)
{
	UNUSED_PARAMETER(cls);
}

void airplay_pause(void *cls)
{
	UNUSED_PARAMETER(cls);
}

void airplay_stop(void *cls)
{
	UNUSED_PARAMETER(cls);
}

void airplay_seek(void *cls, long fPosition)
{
	UNUSED_PARAMETER(cls);
	UNUSED_PARAMETER(fPosition);
}

void airplay_setvolume(void *cls, int volume)
{
	UNUSED_PARAMETER(cls);
	UNUSED_PARAMETER(volume);
}

void airplay_showphoto(void *cls, unsigned char *data, long long size)
{
	UNUSED_PARAMETER(cls);
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(size);
}

long airplay_getduration(void *cls)
{
	UNUSED_PARAMETER(cls);
	return 0;
}

long airplay_getpostion(void *cls)
{
	UNUSED_PARAMETER(cls);
	return 0;
}

int airplay_isplaying(void *cls)
{
	UNUSED_PARAMETER(cls);
	return 0;
}

int airplay_ispaused(void *cls)
{
	UNUSED_PARAMETER(cls);
	return 0;
}

/*audio*/
void audio_init(void *cls, int bits, int channels, int samplerate, int isaudio)
{
	UNUSED_PARAMETER(cls);
	UNUSED_PARAMETER(bits);
	UNUSED_PARAMETER(channels);
	UNUSED_PARAMETER(samplerate);
	UNUSED_PARAMETER(isaudio);
}

void audio_process(void *cls, const void *buffer, int buflen,
		   uint64_t timestamp, uint32_t seqnum)
{
	UNUSED_PARAMETER(cls);
	UNUSED_PARAMETER(buffer);
	UNUSED_PARAMETER(buflen);
	UNUSED_PARAMETER(timestamp);
	UNUSED_PARAMETER(seqnum);
}

void audio_destory(void *cls)
{
	UNUSED_PARAMETER(cls);
}

void audio_setvolume(void *cls, int volume)
{
	UNUSED_PARAMETER(cls);
	UNUSED_PARAMETER(volume);
}

void audio_setmetadata(void *cls, const void *buffer, int buflen)
{
	UNUSED_PARAMETER(cls);
	UNUSED_PARAMETER(buffer);
	UNUSED_PARAMETER(buflen);
}

void audio_setcoverart(void *cls, const void *buffer, int buflen)
{
	UNUSED_PARAMETER(cls);
	UNUSED_PARAMETER(buffer);
	UNUSED_PARAMETER(buflen);
}

void audio_flush(void *cls)
{
	UNUSED_PARAMETER(cls);
}

/*mirror*/
static uint64_t ptsOrigin = -1;
uint8_t *mirrbuff = nullptr;
void mirroring_play(void *cls, int width, int height, const void *buffer,
		    int buflen, int payloadtype, uint64_t timestamp)
{
	AirInput *ptr = (AirInput *)cls;
	if (!ptr) {
		blog(LOG_ERROR, "mirroring_play ptr is null!\n");
		return;
	}
		
	if (!mirrbuff) {
		const int MIRROR_BUFF_SIZE = 4 * 1024 * 1024;
		mirrbuff = new uint8_t[MIRROR_BUFF_SIZE];
	}

	int spscnt;
	int spsnalsize;
	int ppscnt;
	int ppsnalsize;

	unsigned char *head = (unsigned char *)buffer;

	spscnt = head[5] & 0x1f;
	spsnalsize = ((uint32_t)head[6] << 8) | ((uint32_t)head[7]);
	ppscnt = head[8 + spsnalsize];
	ppsnalsize = ((uint32_t)head[9 + spsnalsize] << 8) |
		     ((uint32_t)head[10 + spsnalsize]);

	unsigned char *data =
		(unsigned char *)malloc(4 + spsnalsize + 4 + ppsnalsize);
	if (!data) {
		blog(LOG_ERROR, "mirroring_process malloc failed!\n");
		return;
	}

	data[0] = 0;
	data[1] = 0;
	data[2] = 0;
	data[3] = 1;
	memcpy(data + 4, head + 8, spsnalsize);
	data[4 + spsnalsize] = 0;
	data[5 + spsnalsize] = 0;
	data[6 + spsnalsize] = 0;
	data[7 + spsnalsize] = 1;
	memcpy(data + 8 + spsnalsize, head + 11 + spsnalsize, ppsnalsize);

	ptsOrigin = timestamp;
	uint64_t pts = NO_PTS;
	uint32_t len = 4 + spsnalsize + 4 + ppsnalsize;

	uint8_t hdr[HEADER_SIZE];
	buffer_write64be(&hdr[0], pts);
	buffer_write32be(&hdr[8], len);
	int ret = 0;
	ret = ptr->air_pipe.Send((char*)hdr, HEADER_SIZE);
	if (ret < HEADER_SIZE)
		blog(LOG_ERROR, "send failed! expect:%d, facted:%d",
		     HEADER_SIZE, ret);
	ret = ptr->air_pipe.Send((char *)data, len);
	if (ret < HEADER_SIZE)
		blog(LOG_ERROR, "send failed! expect:%d, facted:%d",
		     HEADER_SIZE, ret);
	
	free(data);
}

void mirroring_process(void *cls, const void *buffer, int buflen,
		       int payloadtype, uint64_t timestamp)
{
	AirInput *ptr = (AirInput *)cls;
	if (!ptr) {
		blog(LOG_ERROR, "mirroring_play ptr is null!\n");
		return;
	}

	if (payloadtype == 0) {
		int rLen;
		unsigned char *head;

		memcpy(mirrbuff, buffer, buflen);

		rLen = 0;
		head = (unsigned char *)mirrbuff + rLen;
		while (rLen < buflen) {
			rLen += 4;
			rLen += (((uint32_t)head[0] << 24) |
				 ((uint32_t)head[1] << 16) |
				 ((uint32_t)head[2] << 8) | (uint32_t)head[3]);

			head[0] = 0;
			head[1] = 0;
			head[2] = 0;
			head[3] = 1;
			head = (unsigned char *)mirrbuff + rLen;
		}
		uint64_t pts = timestamp - ptsOrigin;
		uint32_t len = buflen;

		uint8_t hdr[12];
		buffer_write64be(&hdr[0], pts);
		buffer_write32be(&hdr[8], len);
		int ret = ptr->air_pipe.Send((char *)hdr, 12);
		if (ret < HEADER_SIZE)
			blog(LOG_ERROR, "send failed! expect:%d, facted:%d",
			     HEADER_SIZE, ret);
		ptr->air_pipe.Send((char *)mirrbuff, len);
		if (ret < HEADER_SIZE)
			blog(LOG_ERROR, "send failed! expect:%d, facted:%d",
			     HEADER_SIZE, ret);

	} else if (payloadtype == 1) {
		int spscnt;
		int spsnalsize;
		int ppscnt;
		int ppsnalsize;

		unsigned char *head = (unsigned char *)buffer;

		spscnt = head[5] & 0x1f;
		spsnalsize = ((uint32_t)head[6] << 8) | ((uint32_t)head[7]);
		ppscnt = head[8 + spsnalsize];
		ppsnalsize = ((uint32_t)head[9 + spsnalsize] << 8) |
			     ((uint32_t)head[10 + spsnalsize]);

		unsigned char *data = (unsigned char *)malloc(4 + spsnalsize +
							      4 + ppsnalsize);
		if (!data) {
			blog(LOG_ERROR, "mirroring_process malloc failed!\n");
			return;
		}
		data[0] = 0;
		data[1] = 0;
		data[2] = 0;
		data[3] = 1;
		memcpy(data + 4, head + 8, spsnalsize);
		data[4 + spsnalsize] = 0;
		data[5 + spsnalsize] = 0;
		data[6 + spsnalsize] = 0;
		data[7 + spsnalsize] = 1;
		memcpy(data + 8 + spsnalsize, head + 11 + spsnalsize,
		       ppsnalsize);

		ptsOrigin = timestamp;
		uint64_t pts = NO_PTS;
		uint32_t len = 4 + spsnalsize + 4 + ppsnalsize;

		uint8_t hdr[HEADER_SIZE];
		buffer_write64be(&hdr[0], pts);
		buffer_write32be(&hdr[8], len);
		int ret = ptr->air_pipe.Send((char *)hdr, HEADER_SIZE);
		if (ret < HEADER_SIZE)
			blog(LOG_ERROR, "send failed! expect:%d, facted:%d",
			     HEADER_SIZE, ret);
		ptr->air_pipe.Send((char *)data, len);
		if (ret < HEADER_SIZE)
			blog(LOG_ERROR, "send failed! expect:%d, facted:%d",
			     HEADER_SIZE, ret);

		free(data);
	}
}

void mirroring_stop(void *cls)
{
	if (mirrbuff) {
		delete[] mirrbuff;
		mirrbuff = nullptr;
	}
}

void mirroring_live(void *cls)
{
	UNUSED_PARAMETER(cls);
}

/******/
void sdl_audio_callback(void *cls, uint8_t *stream, int len)
{
	UNUSED_PARAMETER(cls);
	UNUSED_PARAMETER(stream);
	UNUSED_PARAMETER(len);
}

/*------------------------------------------------------------------*/

static const char *GetCellAirPlayName(void *)
{
	return obs_module_text("AirPlayDevice");
}

static void *CreateCellAirPlay(obs_data_t *settings, obs_source_t *source)
{
	AirInput *airshow = nullptr;

	try {
		airshow = new AirInput(source, settings);
		if (airshow->Start() < 0) {
			blog(LOG_ERROR, "cell airplay start failed!");
			delete (airshow);
			airshow = nullptr;
		}
	} catch (const char *error) {
		blog(LOG_ERROR, "Could not create device '%s': %s",
		     obs_source_get_name(source), error);
	}

	return airshow;
}

static void DestroyCellAirPlay(void *data)
{
	delete reinterpret_cast<AirInput *>(data);
}

static uint32_t GetWidth(void *data)
{
	AirInput* ptr = reinterpret_cast<AirInput *>(data);
	return ptr->width;
}

static uint32_t GetHeight(void *data)
{
	AirInput *ptr = reinterpret_cast<AirInput *>(data);
	return ptr->height;
}

void RegisterCellAirSource()
{
	obs_source_info info = {};
	info.id = "airplay_input";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_ASYNC;
	info.get_name = GetCellAirPlayName;
	info.create = CreateCellAirPlay;
	info.destroy = DestroyCellAirPlay;
	info.get_width = GetWidth;
	info.get_height = GetHeight;
	info.icon_type = OBS_ICON_TYPE_CAMERA;
	obs_register_source(&info);
}

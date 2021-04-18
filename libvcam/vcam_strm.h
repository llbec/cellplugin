/// by fanxiushu 2020-05-11
/// 与 虚拟摄像头通讯的接口API函数 

#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct vcam_device_info_t
{
	const char*  device_name; //此名字用于打开虚拟摄像头驱动，下面名字，用于显示的方便
	////
	const char*  device_desc;
	const char*  device_instId;
};

enum vcam_enum_type_t
{
	ENUM_TYPE_ALL    = 1,  // 枚举本驱动生成的所有摄像头实例，包括已经被调用 vcam_video_create 函数打开的摄像头。这个枚举可用来判断驱动是否已经安装。

	ENUM_TYPE_UNUSED = 2,  // 枚举摄像头，但是不包括被 vcam_video_create 函数打开的摄像头。
	                       // 这么枚举的目的是为了防止对某个摄像头的数据源重复被 vcam_video_create 函数占用，从而造成混乱。
};

///枚举所有的虚拟摄像头，此函数查找所有安装的虚拟摄像头驱动，此函数可以用于下面打开某个虚拟摄像头，或者检测是否安装了摄像头驱动。
///没有枚举到 *p_list_devices = NULL, *p_count=0; 枚举到的话，p_count返回个数，p_list_devices返回枚举到的虚拟摄像头
int vcam_enum_video_devices(enum vcam_enum_type_t enum_type , 
	     struct vcam_device_info_t** p_list_devices, int* p_count);

///销毁枚举函数分配的内存
void vcam_free_video_devices(struct vcam_device_info_t** p_list_devices);

////  打开具体的摄像头驱动,参数是 vcam_video_device_t 里边的 device_name
////  函数成功返回一个非0的void*句柄，用于下面其他函数调用。否则返回NULL。
void* vcam_video_create(const char* device_name); 

///关闭摄像头驱动，参数是 vcam_video_create 的返回值
void  vcam_video_destroy(void* handle);

//设置分辨率的数据结构，因为摄像头驱动内部固定使用YUY2图像格式，所以这里只有width和height设置
struct vcam_video_format_t
{
	int   width;
	int   height;
	int   fps;    ///设置参照FPS ，都常都是 30 ，有些程序设置成别值可能不能识别，比如WIN10自带的Camera程序，如果FPS是60就不能识别
};

///设置分辨率，第一个参数是 vcam_video_create 的返回值， 
///第2个参数是个数组，表示可以设置多少个分辨率。 总数不得超过20个
///第2个参数的第一项是输入的图像大小，其他尺寸是为了某些摄像头程序兼容考虑。
int vcam_video_set_format(void* handle, struct vcam_video_format_t* format_array, int count); 

////在摄像头驱动内部，会按照固定的时间间隔刷新图像，默认是33毫秒刷新一次，也就是 30FPS的频率，
////这个函数可以修改默认值，第一个参数是 vcam_video_create 的返回值，第二个参数是间隔时间，单位毫秒
int vcam_video_set_refresh_interval(void* handle, int sleep_msec);//

//给摄像头驱动输入RGB32 的图像数据源，第一个参数是 vcam_video_create 的返回值，
//第2个和第3个参数是图像的长和宽，第3个参数是RGB32图像数据每行占用的字节数，
//第4个参数是具体的RGB32图像数据地址。图像数据格式 RGB32 的，就是 R+B+G+X四个字节一个像素。
//一般是图像发生变化的时候才调用此函数。
//不需要按照固定频率调用此函数，因为摄像头驱动内会按照固定频率刷新图像数据，vcam_video_set_refresh_interval函数设置这个频率。
//int vcam_video_fill_data(void* handle, int width, int height, int linesize, void* rgb32_data);
int vcam_video_fill_data(void* handle, void * data, int size);

///设置图像旋转模式，一共三种选择。
enum vcam_rotate_t
{
	VCAM_ROTATE_NONE= 0,  //不旋转
	VCAM_ROTATE_90  = 1, //旋转 90度
	VCAM_ROTATE_180 = 2, //旋转180度
	VCAM_ROTATE_270 = 3, //旋转270度
};
int vcam_video_set_rotate_image(void* handle, enum vcam_rotate_t rotate);

///第二个参数代表缩放图像的时候是否锁定长和宽的比例，默认是锁定。
///第三个参数表示是否自动设置匹配实际图像分辨率，也就是vcam_video_fill_data输入图像的长和宽发生变化，是否自动重新调整摄像头分辨率，默认是开启的。
int vcam_video_set_other_param(void* handle, int is_lock_width_height, int is_auto_match_image);

///authorise enable & disable
int vcam_video_set_authorize(void* handle);
int vcam_video_set_deny(void* handle);

///set mirror
int vcam_video_set_mirror(void* handle, bool isMirror);

///获取上一次调用失败的错误信息
char* last_errmsg();

// vcam_video_set_XXXX (vcam_video_set_refresh_interval,vcam_video_set_format, vcam_video_set_rotate_image,vcam_video_set_other_param ) 
//这些函数在每次调用 vcam_video_fill_data 函数前设置，都会生效.

#ifdef __cplusplus
}
#endif


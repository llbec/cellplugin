/// by fanxiushu 2020-05-11
/// �� ��������ͷͨѶ�Ľӿ�API���� 

#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct vcam_device_info_t
{
	const char*  device_name; //���������ڴ���������ͷ�������������֣�������ʾ�ķ���
	////
	const char*  device_desc;
	const char*  device_instId;
};

enum vcam_enum_type_t
{
	ENUM_TYPE_ALL    = 1,  // ö�ٱ��������ɵ���������ͷʵ���������Ѿ������� vcam_video_create �����򿪵�����ͷ�����ö�ٿ������ж������Ƿ��Ѿ���װ��

	ENUM_TYPE_UNUSED = 2,  // ö������ͷ�����ǲ������� vcam_video_create �����򿪵�����ͷ��
	                       // ��ôö�ٵ�Ŀ����Ϊ�˷�ֹ��ĳ������ͷ������Դ�ظ��� vcam_video_create ����ռ�ã��Ӷ���ɻ��ҡ�
};

///ö�����е���������ͷ���˺����������а�װ����������ͷ�������˺����������������ĳ����������ͷ�����߼���Ƿ�װ������ͷ������
///û��ö�ٵ� *p_list_devices = NULL, *p_count=0; ö�ٵ��Ļ���p_count���ظ�����p_list_devices����ö�ٵ�����������ͷ
int vcam_enum_video_devices(enum vcam_enum_type_t enum_type , 
	     struct vcam_device_info_t** p_list_devices, int* p_count);

///����ö�ٺ���������ڴ�
void vcam_free_video_devices(struct vcam_device_info_t** p_list_devices);

////  �򿪾��������ͷ����,������ vcam_video_device_t ��ߵ� device_name
////  �����ɹ�����һ����0��void*������������������������á����򷵻�NULL��
void* vcam_video_create(const char* device_name); 

///�ر�����ͷ������������ vcam_video_create �ķ���ֵ
void  vcam_video_destroy(void* handle);

//���÷ֱ��ʵ����ݽṹ����Ϊ����ͷ�����ڲ��̶�ʹ��YUY2ͼ���ʽ����������ֻ��width��height����
struct vcam_video_format_t
{
	int   width;
	int   height;
	int   fps;    ///���ò���FPS ���������� 30 ����Щ�������óɱ�ֵ���ܲ���ʶ�𣬱���WIN10�Դ���Camera�������FPS��60�Ͳ���ʶ��
};

///���÷ֱ��ʣ���һ�������� vcam_video_create �ķ���ֵ�� 
///��2�������Ǹ����飬��ʾ�������ö��ٸ��ֱ��ʡ� �������ó���20��
///��2�������ĵ�һ���������ͼ���С�������ߴ���Ϊ��ĳЩ����ͷ������ݿ��ǡ�
int vcam_video_set_format(void* handle, struct vcam_video_format_t* format_array, int count); 

////������ͷ�����ڲ����ᰴ�չ̶���ʱ����ˢ��ͼ��Ĭ����33����ˢ��һ�Σ�Ҳ���� 30FPS��Ƶ�ʣ�
////������������޸�Ĭ��ֵ����һ�������� vcam_video_create �ķ���ֵ���ڶ��������Ǽ��ʱ�䣬��λ����
int vcam_video_set_refresh_interval(void* handle, int sleep_msec);//

//������ͷ��������RGB32 ��ͼ������Դ����һ�������� vcam_video_create �ķ���ֵ��
//��2���͵�3��������ͼ��ĳ��Ϳ���3��������RGB32ͼ������ÿ��ռ�õ��ֽ�����
//��4�������Ǿ����RGB32ͼ�����ݵ�ַ��ͼ�����ݸ�ʽ RGB32 �ģ����� R+B+G+X�ĸ��ֽ�һ�����ء�
//һ����ͼ�����仯��ʱ��ŵ��ô˺�����
//����Ҫ���չ̶�Ƶ�ʵ��ô˺�������Ϊ����ͷ�����ڻᰴ�չ̶�Ƶ��ˢ��ͼ�����ݣ�vcam_video_set_refresh_interval�����������Ƶ�ʡ�
//int vcam_video_fill_data(void* handle, int width, int height, int linesize, void* rgb32_data);
int vcam_video_fill_data(void* handle, void * data, int size);

///����ͼ����תģʽ��һ������ѡ��
enum vcam_rotate_t
{
	VCAM_ROTATE_NONE= 0,  //����ת
	VCAM_ROTATE_90  = 1, //��ת 90��
	VCAM_ROTATE_180 = 2, //��ת180��
	VCAM_ROTATE_270 = 3, //��ת270��
};
int vcam_video_set_rotate_image(void* handle, enum vcam_rotate_t rotate);

///�ڶ���������������ͼ���ʱ���Ƿ��������Ϳ�ı�����Ĭ����������
///������������ʾ�Ƿ��Զ�����ƥ��ʵ��ͼ��ֱ��ʣ�Ҳ����vcam_video_fill_data����ͼ��ĳ��Ϳ����仯���Ƿ��Զ����µ�������ͷ�ֱ��ʣ�Ĭ���ǿ����ġ�
int vcam_video_set_other_param(void* handle, int is_lock_width_height, int is_auto_match_image);

///authorise enable & disable
int vcam_video_set_authorize(void* handle);
int vcam_video_set_deny(void* handle);

///set mirror
int vcam_video_set_mirror(void* handle, bool isMirror);

///��ȡ��һ�ε���ʧ�ܵĴ�����Ϣ
char* last_errmsg();

// vcam_video_set_XXXX (vcam_video_set_refresh_interval,vcam_video_set_format, vcam_video_set_rotate_image,vcam_video_set_other_param ) 
//��Щ������ÿ�ε��� vcam_video_fill_data ����ǰ���ã�������Ч.

#ifdef __cplusplus
}
#endif


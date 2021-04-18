#include <objbase.h>

#include <obs-module.h>
#include <obs.hpp>
#include <vector>
#include "utils/netpipe.h"
#include "utils/buffer_util.h"

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

struct EncodedData {
	long long lastStartTime = 0;
	long long lastStopTime = 0;
	std::vector<unsigned char> bytes;
};

struct AirInput {
	obs_source_t *source;
	EncodedData encodedVideo;
	PipeServer obs_pipe;
	PipeClient air_pipe;

	HANDLE obs_rcv_pro;

	void Receive(unsigned char *ptr, size_t size, long long pts)
	{
		int roll = 0;
		long long startTime, stopTime = 0;
		bool hasTime = pts != NO_PTS ? true : false;
		startTime = pts != NO_PTS ? pts : AV_NOPTS_VALUE;

		if (hasTime) {
			/*SendToCallback(isVideo, encodedVideo.bytes.data(),
				       encodedVideo.bytes.size(),
				       encodedVideo.lastStartTime,
				       encodedVideo.lastStopTime, roll);*/

			encodedVideo.bytes.resize(0);
			encodedVideo.lastStartTime = startTime;
			encodedVideo.lastStopTime = stopTime;
		}

		encodedVideo.bytes.insert(encodedVideo.bytes.end(),
					  (unsigned char *)ptr,
				  (unsigned char *)ptr + size);
	}
};

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
	ptr->air_pipe.Send((char*)hdr, HEADER_SIZE);
	ptr->air_pipe.Send((char *)data, len);
	
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
		ptr->air_pipe.Send((char *)hdr, 12);
		ptr->air_pipe.Send((char *)mirrbuff, len);

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
		ptr->air_pipe.Send((char *)hdr, HEADER_SIZE);
		ptr->air_pipe.Send((char *)data, len);

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

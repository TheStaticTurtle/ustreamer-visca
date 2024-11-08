#pragma once

#include <stdatomic.h>

#include <pthread.h>

#include "../libs/types.h"
#include "../libs/queue.h"
#include "../libs/ring.h"
#include "../libs/fpsi.h"
#include "../libs/memsink.h"

#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>

#define OUT_FRAME_PX_FMT AV_PIX_FMT_BGRA

struct us_drmstream_s;

typedef struct us_capturesink_runtime_s {
	atomic_bool					stop;

	us_frame_s *				frame;
	us_memsink_s *				sink;

	const AVCodec*				codec;
	AVCodecContext* 			codec_ctx;

	AVPacket*					packet_in;				// Packet holding the incoming data from the sink
	AVFrame*					frame_in;				// Decoded frame from the packet
	struct SwsContext* 			sws_ctx;				// SWS Context from pixel format correction
	AVFrame*					frame_out_rgb_back;		// Pixel format correct output frame 1
	AVFrame*					frame_out_rgb_front;	// Pixel format correct output frame 2

	us_fpsi_s* 					fps;

	struct us_drmstream_s*		stream;

} us_capturesink_runtime_t;

typedef struct us_capturesink_s {
	char*						sink_raw_name;

	us_capturesink_runtime_t	*run;


} us_capturesink_t;


us_capturesink_t *us_capturesink_init();
void us_capturesink_destroy(us_capturesink_t *capturesink);

void us_capturesink_loop(us_capturesink_t *capturesink);
void us_capturesink_loop_break(us_capturesink_t *capturesink);

#include "capturesink.h"

#include <libavcodec/codec_id.h>
#include <libavcodec/packet.h>
#include <libavutil/error.h>
#include <libavutil/pixfmt.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <limits.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include <pthread.h>

#include "../libs/types.h"
#include "../libs/errors.h"
#include "../libs/tools.h"
#include "../libs/threading.h"
#include "../libs/process.h"
#include "../libs/logging.h"
#include "../libs/memsink.h"
#include "../libs/frame.h"
#include "../libs/memsink.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#define _LOG_ERROR(x_msg, ...)		US_LOG_ERROR("CAPTURESINK: " x_msg, ##__VA_ARGS__)
#define _LOG_PERROR(x_msg, ...)		US_LOG_PERROR("CAPTURESINK: " x_msg, ##__VA_ARGS__)
#define _LOG_WARN(x_msg, ...)		US_LOG_WARN("CAPTURESINK: " x_msg, ##__VA_ARGS__)
#define _LOG_INFO(x_msg, ...)		US_LOG_INFO("CAPTURESINK: " x_msg, ##__VA_ARGS__)
#define _LOG_VERBOSE(x_msg, ...)	US_LOG_VERBOSE("CAPTURESINK: " x_msg, ##__VA_ARGS__)
#define _LOG_DEBUG(x_msg, ...)		US_LOG_DEBUG("CAPTURESINK: " x_msg, ##__VA_ARGS__)
#define _LOG_TRACE(x_msg, ...)		US_LOG_TRACE("CAPTURESINK: " x_msg, ##__VA_ARGS__)


us_capturesink_s *us_capturesink_init() {
	us_capturesink_runtime_s *run;
	US_CALLOC(run, 1);
	atomic_init(&run->stop, false);

	run->fps = us_fpsi_init("capturesink", false);

	run->frame = us_frame_init();
	run->sink = NULL;

	run->packet_in = av_packet_alloc();
	run->frame_in = av_frame_alloc();

	us_capturesink_s *capturesink;
	US_CALLOC(capturesink, 1);
	capturesink->run = run;
	capturesink->sink_raw_name = "ustreamer:raw";

	return capturesink;
}

void us_capturesink_destroy(us_capturesink_s *capturesink) {
	us_fpsi_destroy(capturesink->run->fps);

	us_frame_destroy(capturesink->run->frame);

	av_packet_free(&capturesink->run->packet_in);
	av_frame_free(&capturesink->run->frame_in);

	free(capturesink->run);
	free(capturesink);
}



bool us_capturesink_init_av_from_packet(us_capturesink_s *capturesink, AVPacket* packet) {
	us_capturesink_runtime_s *const run = capturesink->run;

	if(run->codec_ctx != NULL) {
		return true;
	}

	run->codec = avcodec_find_decoder(AV_CODEC_ID_MJPEG);
	
	if ((run->codec_ctx = avcodec_alloc_context3(run->codec)) == NULL) {
		_LOG_ERROR("Failed to allocate codec context");
		return false;
	}

	if (avcodec_open2(run->codec_ctx, run->codec, NULL) < 0) {
		_LOG_ERROR("Failed to open codec");
		return false;
	}

	return true;
}
void us_capturesink_destroy_av(us_capturesink_s* capturesink) {
	us_capturesink_runtime_s *const run = capturesink->run;

	if(run->codec_ctx == NULL) { return; }

	avcodec_close(run->codec_ctx);

	avcodec_free_context(&run->codec_ctx);
	run->codec_ctx = NULL;

	run->codec = NULL;
}

bool us_capturesink_init_output_from_frame(us_capturesink_s *capturesink, AVFrame* frame) {
	us_capturesink_runtime_s *const run = capturesink->run;

	if(run->sws_ctx != NULL) {
		return true;
	}

	run->sws_ctx = sws_getContext(
		frame->width,
		frame->height,
		frame->format,
		frame->width,
		frame->height,
		OUT_FRAME_PX_FMT,
		SWS_BILINEAR,
		NULL,
		NULL,
		NULL
	);
	if (run->sws_ctx == NULL) {
		_LOG_ERROR("Failed to get SWS context");
		return false;
	}


	run->frame_out_rgb_back = av_frame_alloc();
	if (run->frame_out_rgb_back == NULL) {
		_LOG_ERROR("Failed to allocate output frame 1");
		return false;
	}
	run->frame_out_rgb_back->width = frame->width;
	run->frame_out_rgb_back->height = frame->height;
	run->frame_out_rgb_back->format = OUT_FRAME_PX_FMT;
	if (av_image_alloc(run->frame_out_rgb_back->data, run->frame_out_rgb_back->linesize, frame->width, frame->height, AV_PIX_FMT_BGRA, 32) < 0) {
		_LOG_ERROR("Failed to allocate buffer for output frame 1");
		return false;
	}

	run->frame_out_rgb_front = av_frame_alloc();
	if (run->frame_out_rgb_front == NULL) {
		_LOG_ERROR("Failed to allocate output frame 2");
		return false;
	}
	run->frame_out_rgb_front->width = frame->width;
	run->frame_out_rgb_front->height = frame->height;
	run->frame_out_rgb_front->format = OUT_FRAME_PX_FMT;
	if (av_image_alloc(run->frame_out_rgb_front->data, run->frame_out_rgb_front->linesize, frame->width, frame->height, AV_PIX_FMT_BGRA, 32) < 0) {
		_LOG_ERROR("Failed to allocate buffer for output frame 2");
		return false;
	}

	return true;
}
void us_capturesink_destroy_outputs(us_capturesink_s* capturesink) {
	us_capturesink_runtime_s *const run = capturesink->run;

	if(run->sws_ctx == NULL) { return; }

	av_frame_free(&run->frame_out_rgb_back);
	run->frame_out_rgb_back = NULL;
	av_frame_free(&run->frame_out_rgb_front);
	run->frame_out_rgb_front = NULL;
	
	sws_freeContext(run->sws_ctx);
	run->sws_ctx = NULL;
}


void us_capturesink_loop(us_capturesink_s *capturesink) {
	us_capturesink_runtime_s *const run = capturesink->run;

	_LOG_INFO("Starting to receive data from raw sink \"%s\"", capturesink->sink_raw_name);

	if ((run->sink = us_memsink_init_opened("input", capturesink->sink_raw_name, false, 0, false, 0, 2)) == NULL) {
		_LOG_ERROR("Failed to open sink");
		exit(1);
	}


	while (!atomic_load(&run->stop)) {
		bool key_requested = false;
		const int got = us_memsink_client_get(run->sink, run->frame, &key_requested, false);
		if (got == 0) {
			// Fill the data of the avframe from the us_frame
			run->packet_in->pts = run->frame->grab_ts * 1000;
			run->packet_in->data = run->frame->data;
			run->packet_in->size = run->frame->used;
			run->packet_in->flags = run->frame->key ? AV_PKT_FLAG_KEY : 0;
			
			// Only executed once, inits codec context
			if(!us_capturesink_init_av_from_packet(capturesink, run->packet_in)) {
				_LOG_ERROR("Failed to init context from first packet, exiting");
				break;
			}

			// Decode the incoming packet
			avcodec_send_packet(run->codec_ctx, run->packet_in);
			avcodec_receive_frame(run->codec_ctx, run->frame_in);

			// Only executed once, inits SWS and output frames
			if(!us_capturesink_init_output_from_frame(capturesink, run->frame_in)) {
				_LOG_ERROR("Failed to init output from first frame, exiting");
				break;
			}

			// Correct the pixel format
			sws_scale(
				run->sws_ctx, 
				(const uint8_t **)run->frame_in->data, 
				run->frame_in->linesize, 
				0, 
				run->frame_in->height, 
				run->frame_out_rgb_back->data, 
				run->frame_out_rgb_back->linesize
			);

			// Swap the buffers
			AVFrame* tmp = run->frame_out_rgb_front;
			run->frame_out_rgb_front = run->frame_out_rgb_back;
			run->frame_out_rgb_back = tmp;

			_LOG_TRACE(
				"Got frame %c n=%d pts=%" PRId64 " dts=%" PRId64 " is_key=%d latency=%.3Lf",
				av_get_picture_type_char(run->frame_in->pict_type),
				run->codec_ctx->frame_number,
				run->frame_in->pts,
				run->frame_in->pkt_dts,
				run->frame_in->key_frame,
				us_get_now_monotonic() - run->frame->grab_ts
			);

			us_fpsi_update(run->fps, true, NULL);

		} else if (got == US_ERROR_NO_DATA) {
			usleep(5000);
		} else {
			_LOG_ERROR("us_memsink_client_get failed");
			break;
		}
	}

	us_capturesink_destroy_outputs(capturesink);

	us_capturesink_destroy_av(capturesink);
	
	US_DELETE(capturesink->run->sink, us_memsink_destroy);


	if (!atomic_load(&run->stop)) {
		US_SEP_INFO('=');
	}
}

void us_capturesink_loop_break(us_capturesink_s *stream) {
	atomic_store(&stream->run->stop, true);
}




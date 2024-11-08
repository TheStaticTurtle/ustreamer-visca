#include <libavutil/log.h>
#include <stdio.h>
#include <stdbool.h>

#include <pthread.h>

#include "../libs/tools.h"
#include "../libs/threading.h"
#include "../libs/logging.h"
#include "../libs/capture.h"
#include "../libs/signal.h"

#include "options.h"
#include "drmstream.h"
#include "capturesink.h"
#include "audstream.h"
#include "utils.h"

static us_drmstream_t	*_g_drmstream = NULL;
static us_capturesink_t	*_g_capturesink = NULL;
static us_audstream_s	*_g_audstream = NULL;


static void _block_thread_signals(void) {
	sigset_t mask;
	assert(!sigemptyset(&mask));
	assert(!sigaddset(&mask, SIGINT));
	assert(!sigaddset(&mask, SIGTERM));
	assert(!pthread_sigmask(SIG_BLOCK, &mask, NULL));
}

static void *_drmstream_loop_thread(void *arg) {
	(void)arg;
	US_THREAD_SETTLE("drmstream");
	_block_thread_signals();
	us_drmstream_loop(_g_drmstream);
	return NULL;
}
static void *_capturesink_loop_thread(void *arg) {
	(void)arg;
	US_THREAD_SETTLE("capturesink");
	_block_thread_signals();
	us_capturesink_loop(_g_capturesink);
	return NULL;
}
static void *_audstream_loop_thread(void *arg) {
	(void)arg;
	US_THREAD_SETTLE("audstream");
	_block_thread_signals();
	us_audstream_loop(_g_audstream);
	return NULL;
}

static void _signal_handler(int signum) {
	char *const name = us_signum_to_string(signum);
	US_LOG_INFO_NOLOCK("===== Stopping by %s =====", name);
	free(name);
	us_drmstream_loop_break(_g_drmstream);
	us_capturesink_loop_break(_g_capturesink);
	us_audstream_loop_break(_g_audstream);
}

int main(int argc, char *argv[]) {
	assert(argc >= 0);
	int exit_code = 0;

	US_LOGGING_INIT;
	US_THREAD_RENAME("main");

    av_log_set_level(AV_LOG_INFO);
	us_set_libav_log_callback();
	
	us_options_s *options = us_options_init(argc, argv);
	
	_g_capturesink = us_capturesink_init();
	_g_drmstream = us_drmstream_init();

	_g_capturesink->run->stream = _g_drmstream;
	_g_drmstream->run->capture = _g_capturesink;


	_g_audstream = us_audstream_init();

	if ((exit_code = options_parse(options, _g_drmstream, _g_audstream)) == 0) {

		us_install_signals_handler(_signal_handler, true);

		pthread_t drmstream_loop_tid;
		pthread_t capturesink_loop_tid;
		pthread_t audstream_loop_tid;
		US_THREAD_CREATE(drmstream_loop_tid, _drmstream_loop_thread, NULL);
		US_THREAD_CREATE(capturesink_loop_tid, _capturesink_loop_thread, NULL);
		US_THREAD_CREATE(audstream_loop_tid, _audstream_loop_thread, NULL);
		US_THREAD_JOIN(drmstream_loop_tid);
		US_THREAD_JOIN(capturesink_loop_tid);
		US_THREAD_JOIN(audstream_loop_tid);
	}

	us_drmstream_destroy(_g_drmstream);
	us_capturesink_destroy(_g_capturesink);
	us_audstream_destroy(_g_audstream);
	us_options_destroy(options);

	if (exit_code == 0) {
		US_LOG_INFO("Bye-bye");
	}
	US_LOGGING_DESTROY;
	return (exit_code < 0 ? 1 : 0);
}

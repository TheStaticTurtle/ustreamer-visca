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
#include "drm.h"
#include "capturesink.h"
#include "utils.h"

static us_drm_t	*_g_drm = NULL;
static us_capturesink_t	*_g_capturesink = NULL;


static void _block_thread_signals(void) {
	sigset_t mask;
	assert(!sigemptyset(&mask));
	assert(!sigaddset(&mask, SIGINT));
	assert(!sigaddset(&mask, SIGTERM));
	assert(!pthread_sigmask(SIG_BLOCK, &mask, NULL));
}

static void *_capturesink_loop_thread(void *arg) {
	(void)arg;
	US_THREAD_SETTLE("capturesink");
	_block_thread_signals();
	us_capturesink_loop(_g_capturesink);
	return NULL;
}

static void _signal_handler(int signum) {
	char *const name = us_signum_to_string(signum);
	US_LOG_INFO_NOLOCK("===== Stopping by %s =====", name);
	free(name);
	us_capturesink_loop_break(_g_capturesink);
}

int main(int argc, char *argv[]) {
	assert(argc >= 0);
	int exit_code = 0;

	US_LOGGING_INIT;
	US_THREAD_RENAME("main");

    av_log_set_level(AV_LOG_INFO);
	us_set_libav_log_callback();
	
	us_options_s *options = us_options_init(argc, argv);
	
	_g_drm = us_drm_init();
	_g_capturesink = us_capturesink_init(_g_drm);

	if ((exit_code = options_parse(options, _g_drm, _g_capturesink)) == 0) {

		us_install_signals_handler(_signal_handler, true);

		pthread_t capturesink_loop_tid;
		US_THREAD_CREATE(capturesink_loop_tid, _capturesink_loop_thread, NULL);
		US_THREAD_JOIN(capturesink_loop_tid);
	}

	us_capturesink_destroy(_g_capturesink);
	us_drm_destroy(_g_drm);
	us_options_destroy(options);

	if (exit_code == 0) {
		US_LOG_INFO("Bye-bye");
	}
	US_LOGGING_DESTROY;
	return (exit_code < 0 ? 1 : 0);
}

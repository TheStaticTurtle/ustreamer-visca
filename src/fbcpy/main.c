#include <stdio.h>
#include <stdbool.h>

#include <pthread.h>

#include "../libs/tools.h"
#include "../libs/threading.h"
#include "../libs/logging.h"
#include "../libs/capture.h"
#include "../libs/signal.h"

#include "options.h"
#include "fbstream.h"
#include "audstream.h"

static us_fbstream_s	*_g_fbstream = NULL;
static us_audstream_s	*_g_audstream = NULL;


static void _block_thread_signals(void) {
	sigset_t mask;
	assert(!sigemptyset(&mask));
	assert(!sigaddset(&mask, SIGINT));
	assert(!sigaddset(&mask, SIGTERM));
	assert(!pthread_sigmask(SIG_BLOCK, &mask, NULL));
}

static void *_fbstream_loop_thread(void *arg) {
	(void)arg;
	US_THREAD_SETTLE("fbstream");
	_block_thread_signals();
	us_fbstream_loop(_g_fbstream);
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
	us_fbstream_loop_break(_g_fbstream);
	us_audstream_loop_break(_g_audstream);
}

int main(int argc, char *argv[]) {
	assert(argc >= 0);
	int exit_code = 0;

	US_LOGGING_INIT;
	US_THREAD_RENAME("main");

	us_options_s *options = us_options_init(argc, argv);
	
	_g_fbstream = us_fbstream_init();
	_g_audstream = us_audstream_init();

	if ((exit_code = options_parse(options, _g_fbstream, _g_audstream)) == 0) {

		us_install_signals_handler(_signal_handler, true);

		pthread_t fbstream_loop_tid;
		pthread_t audstream_loop_tid;
		US_THREAD_CREATE(fbstream_loop_tid, _fbstream_loop_thread, NULL);
		US_THREAD_CREATE(audstream_loop_tid, _audstream_loop_thread, NULL);
		US_THREAD_JOIN(fbstream_loop_tid);
		US_THREAD_JOIN(audstream_loop_tid);
	}

	us_fbstream_destroy(_g_fbstream);
	us_audstream_destroy(_g_audstream);
	us_options_destroy(options);

	if (exit_code == 0) {
		US_LOG_INFO("Bye-bye");
	}
	US_LOGGING_DESTROY;
	return (exit_code < 0 ? 1 : 0);
}

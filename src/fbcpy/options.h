#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <limits.h>
#include <getopt.h>
#include <errno.h>
#include <assert.h>

#include "../libs/const.h"
#include "../libs/logging.h"
#include "../libs/process.h"
#include "../libs/frame.h"
#include "../libs/memsink.h"
#include "../libs/options.h"


#include "drmstream.h"
#include "audstream.h"

#define US_VIDEO_MIN_WIDTH		((uint)160)
#define US_VIDEO_MAX_WIDTH		((uint)15360) // Remember about stream->run->http_capture_state;

#define US_VIDEO_MIN_HEIGHT		((uint)120)
#define US_VIDEO_MAX_HEIGHT		((uint)8640)


typedef struct {
	unsigned		argc;
	char			**argv;
	char			**argv_copy;
} us_options_s;


us_options_s *us_options_init(unsigned argc, char *argv[]);
void us_options_destroy(us_options_s *options);

int options_parse(us_options_s *options, us_drmstream_t *drmstream, us_audstream_s *audstream);

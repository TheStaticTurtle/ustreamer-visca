#include "options.h"


enum _US_OPT_VALUES {
	_O_SINK_PATH = 'i',

	_O_DRM_CARD = 'd',
	_O_DRM_RESOLUTION = 'r',
	_O_DRM_FPS = 'f',

	_O_HELP = 'h',
	_O_VERSION = 'v',

	// Longs only

	_O_AUDIO_DEV_IN = 10000,
	_O_AUDIO_DEV_OUT,
	_O_AUDIO_BUFFERS,

	_O_LOG_LEVEL,
	_O_PERF,
	_O_VERBOSE,
	_O_DEBUG,
	_O_TRACE,
	_O_FORCE_LOG_COLORS,
	_O_NO_LOG_COLORS,
};

static const struct option _LONG_OPTS[] = {
	{"sink",					required_argument,	NULL,	_O_SINK_PATH},

	{"drm-device",			required_argument,	NULL,	_O_DRM_CARD},
	{"drm-resolution",		required_argument,	NULL,	_O_DRM_RESOLUTION},
	{"drm-rate",				required_argument,	NULL,	_O_DRM_FPS},
	

	{"adev-in",				required_argument,	NULL,	_O_AUDIO_DEV_IN},
	{"adev-out",				required_argument,	NULL,	_O_AUDIO_DEV_OUT},
	{"abuffers",				required_argument,	NULL,	_O_AUDIO_BUFFERS},

	{"log-level",				required_argument,	NULL,	_O_LOG_LEVEL},
	{"perf",					no_argument,		NULL,	_O_PERF},
	{"verbose",				no_argument,		NULL,	_O_VERBOSE},
	{"debug",					no_argument,		NULL,	_O_DEBUG},
	{"trace",					no_argument,		NULL,	_O_TRACE},
	{"force-log-colors",		no_argument,		NULL,	_O_FORCE_LOG_COLORS},
	{"no-log-colors",		no_argument,		NULL,	_O_NO_LOG_COLORS},

	{"help",					no_argument,		NULL,	_O_HELP},
	{"version",				no_argument,		NULL,	_O_VERSION},

	{NULL, 0, NULL, 0},
};

static int _parse_resolution(const char *str, unsigned *width, unsigned *height, bool limited);

static void _help(FILE *fp, const us_drmstream_t *drmstream, const us_audstream_s* audstream);


us_options_s *us_options_init(unsigned argc, char *argv[]) {
	us_options_s *options;
	US_CALLOC(options, 1);
	options->argc = argc;
	options->argv = argv;

	US_CALLOC(options->argv_copy, argc);
	for (unsigned index = 0; index < argc; ++index) {
		options->argv_copy[index] = us_strdup(argv[index]);
	}
	return options;
}

void us_options_destroy(us_options_s *options) {
	for (unsigned index = 0; index < options->argc; ++index) {
		free(options->argv_copy[index]);
	}
	free(options->argv_copy);

	free(options);
}


int options_parse(us_options_s *options, us_drmstream_t *drmstream, us_audstream_s* audstream) {
#	define OPT_SET(x_dest, x_value) { \
			x_dest = x_value; \
			break; \
		}

#	define OPT_NUMBER(x_name, x_dest, x_min, x_max, x_base) { \
			errno = 0; char *m_end = NULL; const long long m_tmp = strtoll(optarg, &m_end, x_base); \
			if (errno || *m_end || m_tmp < x_min || m_tmp > x_max) { \
				printf("Invalid value for '%s=%s': min=%lld, max=%lld\n", x_name, optarg, (long long)x_min, (long long)x_max); \
				return -1; \
			} \
			x_dest = m_tmp; \
			break; \
		}

#	define OPT_PARSE_ENUM(x_name, x_dest, x_func, x_available) { \
			const int m_value = x_func(optarg); \
			if (m_value < 0) { \
				printf("Unknown " x_name ": %s; available: %s\n", optarg, x_available); \
				return -1; \
			} \
			x_dest = m_value; \
			break; \
		}

#	define OPT_CTL_DEFAULT_NOBREAK(x_dest) { \
			cap->ctl.x_dest.mode = CTL_MODE_DEFAULT; \
		}

#	define OPT_CTL_MANUAL(x_dest) { \
			if (!strcasecmp(optarg, "default")) { \
				OPT_CTL_DEFAULT_NOBREAK(x_dest); \
			} else { \
				cap->ctl.x_dest.mode = CTL_MODE_VALUE; \
				OPT_NUMBER("--"#x_dest, cap->ctl.x_dest.value, INT_MIN, INT_MAX, 0); \
			} \
			break; \
		}

#	define OPT_RESOLUTION(x_name, x_dest_width, x_dest_height, x_limited) { \
			switch (_parse_resolution(optarg, &x_dest_width, &x_dest_height, x_limited)) { \
				case -1: \
					printf("Invalid resolution format for '%s=%s'\n", x_name, optarg); \
					return -1; \
				case -2: \
					printf("Invalid width of '%s=%s': min=%u, max=%u\n", x_name, optarg, US_VIDEO_MIN_WIDTH, US_VIDEO_MAX_WIDTH); \
					return -1; \
				case -3: \
					printf("Invalid height of '%s=%s': min=%u, max=%u\n", x_name, optarg, US_VIDEO_MIN_HEIGHT, US_VIDEO_MAX_HEIGHT); \
					return -1; \
				case 0: break; \
				default: assert(0 && "Unknown error"); \
			} \
			break; \
		}

#	define OPT_CTL_AUTO(x_dest) { \
			if (!strcasecmp(optarg, "default")) { \
				OPT_CTL_DEFAULT_NOBREAK(x_dest); \
			} else if (!strcasecmp(optarg, "auto")) { \
				cap->ctl.x_dest.mode = CTL_MODE_AUTO; \
			} else { \
				cap->ctl.x_dest.mode = CTL_MODE_VALUE; \
				OPT_NUMBER("--"#x_dest, cap->ctl.x_dest.value, INT_MIN, INT_MAX, 0); \
			} \
			break; \
		}

	char short_opts[128];
	us_build_short_options(_LONG_OPTS, short_opts, 128);

	for (int ch; (ch = getopt_long(options->argc, options->argv_copy, short_opts, _LONG_OPTS, NULL)) >= 0;) {
		switch (ch) {

			case _O_SINK_PATH:			OPT_SET(drmstream->sink_raw_name, optarg);

			case _O_DRM_CARD:			OPT_SET(drmstream->run->drm->card_path, optarg);
			case _O_DRM_RESOLUTION:		OPT_RESOLUTION("--drm-resolution", drmstream->run->drm->requested_width, drmstream->run->drm->requested_height, true);
			case _O_DRM_FPS:			OPT_NUMBER("--drm-rate", drmstream->run->drm->requested_rate, 0, 60000, 0);

			case _O_AUDIO_DEV_IN:		OPT_SET(audstream->dev_in_name, optarg);
			case _O_AUDIO_DEV_OUT:		OPT_SET(audstream->dev_out_name, optarg);
			case _O_AUDIO_BUFFERS:		OPT_NUMBER("--abuffers", audstream->buffer_size, 1, 1024, 50)

			case _O_LOG_LEVEL:			OPT_NUMBER("--log-level", us_g_log_level, US_LOG_LEVEL_INFO, US_LOG_LEVEL_DEBUG, 0);
			case _O_PERF:				OPT_SET(us_g_log_level, US_LOG_LEVEL_PERF);
			case _O_VERBOSE:			OPT_SET(us_g_log_level, US_LOG_LEVEL_VERBOSE);
			case _O_DEBUG:				OPT_SET(us_g_log_level, US_LOG_LEVEL_DEBUG);
			case _O_TRACE:				OPT_SET(us_g_log_level, US_LOG_LEVEL_TRACE);
			case _O_FORCE_LOG_COLORS:	OPT_SET(us_g_log_colored, true);
			case _O_NO_LOG_COLORS:		OPT_SET(us_g_log_colored, false);

			case _O_HELP:		_help(stdout, drmstream, audstream); return 1;
			case _O_VERSION:	puts(US_VERSION); return 1;

			case 0:		break;
			default:	return -1;
		}
	}

	US_LOG_INFO("Starting uStreamer-FBcpy %s ...", US_VERSION);

#	undef OPT_CTL_AUTO
#	undef OPT_CTL_MANUAL
#	undef OPT_CTL_DEFAULT_NOBREAK
#	undef OPT_RESOLUTION
#	undef OPT_PARSE
#	undef OPT_NUMBER
#	undef OPT_SET
	return 0;
}

static void _help(FILE *fp, const us_drmstream_t *drmstream, const us_audstream_s* audstream) {
#	define SAY(x_msg, ...) fprintf(fp, x_msg "\n", ##__VA_ARGS__)
	SAY("\nuStreamer-FBcpy - uStreamer raw sink adapter to write to the framebuffer");
	SAY("═══════════════════════════════════════════════════");
	SAY("Version: %s; license: GPLv3", US_VERSION);
	SAY("Capture options:");
	SAY("══════════════════");
	SAY("    -i|--sink </dev/path> ──────────────── Name of the raw sink from uStreamer. Default: %s.\n", drmstream->sink_raw_name);

	SAY("Streaming options:");
	SAY("══════════════════");
	SAY("    -d|--drm-device </dev/path>  ───────── Path to darm card. Default: %s.\n", drmstream->run->drm->card_path);
	SAY("    -r|--drm-resolution <WxH>  ─────────── Resolution to use. Default: %dx%d.\n", drmstream->run->drm->requested_width, drmstream->run->drm->requested_height);
	SAY("    -f|--drm-rate mHz  ─────────────────── Refresh rate to use. Default: %d.\n", drmstream->run->drm->requested_rate);


	SAY("Audio options:");
	SAY("══════════════════");
	SAY("    -adin <name>  ─────────── Name of the input device. Default: %s.\n", audstream->dev_in_name);
	SAY("    -adout <name>  ────────── Name of the output device. Default: %s.\n", audstream->dev_out_name);
	SAY("    -abuffers <N>  ────────── Numer for frames to store in the buffer. Default: %d.\n", audstream->buffer_size);
	

	SAY("Logging options:");
	SAY("════════════════");
	SAY("    --log-level <N>  ──── Verbosity level of messages from 0 (error) to 6 (trace).");
	SAY("                          Enabling debugging messages can slow down the program.");
	SAY("                          Available levels: 0 (error), 1 (warn), 2 (info), 3 (verbose), 4 (performance), 5 (debug), 6 (trace).");
	SAY("                          Default: %d.\n", us_g_log_level);
	SAY("    --verbose  ────────── Enable verbose messages and lower (same as --log-level=3). Default: disabled.\n");
	SAY("    --perf  ───────────── Enable performance messages (same as --log-level=4). Default: disabled.\n");
	SAY("    --debug  ──────────── Enable debug messages and lower (same as --log-level=5). Default: disabled.\n");
	SAY("    --trace  ──────────── Enable trace messages and lower (same as --log-level=6). Default: disabled.\n");
	SAY("    --force-log-colors  ─ Force color logging. Default: colored if stderr is a TTY.\n");
	SAY("    --no-log-colors  ──── Disable color logging. Default: ditto.\n");
	SAY("Help options:");
	SAY("═════════════");
	SAY("    -h|--help  ─────── Print this text and exit.\n");
	SAY("    -v|--version  ──── Print version and exit.\n");
#	undef SAY
}

static int _parse_resolution(const char *str, unsigned *width, unsigned *height, bool limited) {
	unsigned tmp_width;
	unsigned tmp_height;
	if (sscanf(str, "%ux%u", &tmp_width, &tmp_height) != 2) {
		return -1;
	}
	if (limited) {
		if (tmp_width < US_VIDEO_MIN_WIDTH || tmp_width > US_VIDEO_MAX_WIDTH) {
			return -2;
		}
		if (tmp_height < US_VIDEO_MIN_HEIGHT || tmp_height > US_VIDEO_MAX_HEIGHT) {
			return -3;
		}
	}
	*width = tmp_width;
	*height = tmp_height;
	return 0;
}
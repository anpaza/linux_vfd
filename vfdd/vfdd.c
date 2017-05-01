/*
 * Main function
 */

#include <stdio.h>
#include <getopt.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include "vfdd.h"

const char *g_version = "0.1.0";
const char *g_config = "/etc/vfdd.ini";
int g_verbose = 0;
int g_daemon = 0;
const char *g_spaces = " \t";

// the global config
struct cfg_struct *g_cfg;

static void show_version ()
{
	printf ("vfd daemon version %s\n", g_version);
}

static void show_help (char *const *argv)
{
	show_version ();
	printf ("usage: %s [options] [config-file]\n", argv [0]);
	printf ("	-h  display this help\n");
	printf ("	-v  verbose info about what's cooking\n");
	printf ("	-V  display program version\n");
}

void trace (const char *format, ...)
{
	va_list argp;
	struct tm tm;
	struct timeval tv;

	if (g_verbose == 0)
		return;

	if (gettimeofday (&tv, NULL) < 0)
		return;

	localtime_r (&tv.tv_sec, &tm);

	printf ("%02d:%02d:%02d.%03d ", tm.tm_hour, tm.tm_min, tm.tm_sec, (int)(tv.tv_usec / 1000));
	va_start (argp, format);
	vprintf (format, argp);
	va_end (argp);
}

static int load_config ()
{
	int ret;

	trace ("loading config file '%s'\n", g_config);

	g_cfg = cfg_init ();

	if ((ret = cfg_load (g_cfg, g_config)) < 0) {
		fprintf (stderr, "failed to load config file '%s'\n", g_config);
		return ret;
	}

	return 0;
}

int main (int argc, char *const *argv)
{
	int ret;

	while ((ret = getopt (argc, argv, "DhvV")) >= 0)
		switch (ret) {
			case 'D':
				g_daemon = 1;
				break;

			case 'v':
				g_verbose = 1;
				break;

			case 'h':
				show_help (argv);
				return -1;

			case 'V':
				show_version ();
				return -1;
		}

	if (optind < argc)
		g_config = argv [optind++];

	// load the config file
	if ((ret = load_config ()) < 0)
		return ret;

	if ((ret = tasks_init ()) < 0)
		return ret;

	tasks_run ();

	return 0;
}

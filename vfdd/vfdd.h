/*
 * Main header file
 */

#ifndef __VFDD_H__
#define __VFDD_H__

#include "cfg_parse.h"

// default settings for various task modules

#define DEFAULT_PRIORITY	100
#define DEFAULT_DEVICE		"/sys/bus/platform/devices/meson-vfd.14"
#define DEFAULT_BRIGHTNESS	50
#define DEFAULT_TASKS		"clock"
#define DEFAULT_DISPLAY		"display"
#define DEFAULT_DISPLAY_QUANTUM	5000
#define DEFAULT_CLOCK_FORMAT	"%H%M"
#define DEFAULT_CLOCK_SEPARATOR	":"
#define DEFAULT_TEMP_VALUE	"/sys/devices/virtual/thermal/thermal_zone0/temp"
#define DEFAULT_TEMP_FORMAT	"t%02d*"
#define DEFAULT_TEMP_DIVIDER	1000
#define DEFAULT_DISK_DEVICE	"sda"
#define DEFAULT_DISK_FIELD	4
#define DEFAULT_DISK_THRESHOLD	50
#define DEFAULT_DISK_INDICATOR	"USB"
#define DEFAULT_DOT_ATTR	"/sys/class/switch/hdmi/state"
#define DEFAULT_DOT_FIELD	1
#define DEFAULT_DOT_THRESHOLD	1
#define DEFAULT_DOT_INDICATOR	"HDMI"


#define ARRAY_SIZE(x)		(sizeof (x) / sizeof (x [0]))

// the global config
extern struct cfg_struct *g_cfg;
// trace calls if non-zero
extern int g_verbose;
// set asynchronously to 1 to initiate shutdown
extern volatile int g_shutdown;

// trace calls if g_verbose != 0
extern void trace (const char *format, ...);

// just a list of spaces, used in many places
extern const char *g_spaces;

extern int tasks_init ();
extern void tasks_run ();
extern void tasks_fini ();

/* superstructure on cfg_parse */
extern const char *cfg_get_str (const char *instance, const char *key, const char *defval);
extern int cfg_get_int (const char *instance, const char *key, int defval);
extern int cfg_get_int_2 (const char *instance, const char *key, int *out);

/* helper functions for sysfs */
extern char *sysfs_read (const char *device_attr);
extern char *sysfs_get_str (const char *device, const char *attr);
extern int sysfs_get_int (const char *device, const char *attr);

extern int sysfs_write (const char *device_attr, const char *value);
extern int sysfs_set_str (const char *device, const char *attr, const char *value);
extern int sysfs_set_int (const char *device, const char *attr, int value);

extern int sysfs_exists (const char *device_attr);

#endif /* __VFDD_H__ */

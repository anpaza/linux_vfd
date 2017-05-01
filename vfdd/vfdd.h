/*
 * Main header file
 */

#ifndef __VFDD_H__
#define __VFDD_H__

#include "cfg_parse.h"

#define DEFAULT_DEVICE		"/sys/bus/platform/devices/meson-vfd.14"
#define DEFAULT_BRIGHTNESS	50
#define DEFAULT_TASKS		"clock"
#define DEFAULT_DISPLAY		"display"
#define DEFAULT_DISPLAY_QUANTUM	5000
#define DEFAULT_CLOCK_FORMAT	"%H%M"
#define DEFAULT_CLOCK_SEPARATOR	":"

// the global config
extern struct cfg_struct *g_cfg;
// trace calls if non-zero
extern int g_verbose;

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
extern char *sysfs_get_str (const char *device, const char *attr, const char *defval);
extern int sysfs_get_int (const char *device, const char *attr, int defval);

extern int sysfs_set_str (const char *device, const char *attr, const char *value);
extern int sysfs_set_int (const char *device, const char *attr, int value);

#endif /* __VFDD_H__ */

/*
 * This task manages LCD display output
 */

#ifndef __TASK_DISPLAY_H__
#define __TASK_DISPLAY_H__

#include "task.h"

#define PRIORITY_MAX		1000000

struct display_user_t {
	struct display_user_t *next;
	struct task_t *task;
	int priority;
	char *display;
	uint16_t dotled;
};

struct indicator_t {
	char *name;
	int word;
	unsigned mask;
};

struct task_display_t {
	struct task_t task;

	const char *device;
	struct indicator_t *indicators;
	int indicator_count;
	int brightness;
	int brightness_max;
	int quantum;
	int min_priority;

	uint16_t *overlay;
	int overlay_len;
	char *overlay_buff;

	struct display_user_t *users;
	/* the display user currently owning the display */
	struct display_user_t *active_user;

	/**
	 * Display a string on behalf of given task.
	 * If more than a task requests displaying something, the display is
	 * time-shared proportional to task priority.
	 * @arg self
	 *      the display task
	 * @arg source
	 *      the task on behalf of which we display the text. only one text
	 *      per task is allowed, thus, the text will replace the previous one
	 *      associated with this task
	 * @arg priority
	 *      text priority. the normal priority is 100, use larger or smaller
	 *      values if you need larger or smaller priority. For example, if a
	 *      task displays with prio=100 and another with prio=10, first task
	 *      will be displayed for 'quantum' ms and secnd for 'quantum*100/10' ms.
	 * @arg string
	 *      the string to display. if NULL, the text associated with 'source' is
	 *      deleted.
	 */
	void (*set_display) (struct task_display_t *self, struct task_t *source, int priority,
		const char *string);

	/**
	 * Enable or disable a single icon on the display
	 * @arg self
	 *	the display task
	 * @arg source
	 *      the task owning the icon. The icon is turned off if the task passes away,
	 *      and all active icons from all tasks are lighted up at the same time.
	 * @arg indicator
	 *	the name of the indicator
	 * @arg enable
	 *	0 to disable the indicator, 1 to enable
	 */
	void (*set_indicator) (struct task_display_t *self, struct task_t *source,
		const char *indicator, int enable);

	/**
	 * Change display brightness
	 * @arg self
	 *	the display task
	 * @arg value
	 *	new brightness (0-100%)
	 */
	void (*set_brightness) (struct task_display_t *self, int value);

	/**
	 * Check if given task is currently active.
	 * @arg source
	 *      the task to check.
	 * @return
	 *      0 if the task is not active, non-zero if it is currently owning the display.
	 */
	int (*is_active) (struct task_display_t *self, struct task_t *source);
};

#endif /* __TASK_DISPLAY_H__ */

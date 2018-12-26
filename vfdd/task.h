/*
 * Task management core
 */

#ifndef __TASK_H__
#define __TASK_H__

#include <stdint.h>
#include <sys/time.h>

/*
 * ANSI C OOP.
 * this is the base class for all tasks.
 */
struct task_t {
	/* next task in a chained list, or NULL if no more */
	struct task_t *next;

	/* task instance name */
	char *instance;

	/* number of milliseconds of sleep left */
	unsigned sleep_ms;

	/* this can be asynchronously set to non-zero when task suddenly requires attention */
	volatile uint8_t attention;

	/**
	 * do whatever this task does.
	 * @arg self
	 *	a pointer to this task
	 * @return
	 *	returns number of milliseconds after which dispatcher calls
	 *	this method the next time.
	 */
	unsigned (*run) (struct task_t *self);

	/**
	 * this method is invoked after all tasks were instantiated.
	 * it can be used to establish horizontal relations between tasks.
	 * @arg self
	 *	a pointer to this task
	 */
	void (*post_init) (struct task_t *self);

	/**
	 * Destroy this object, free whatever resources were claimed etc.
	 * @arg self
	 *	a pointer to this task
	 */
	void (*fini) (struct task_t *self);

	/**
	 * This function is called whenever a display task switches
	 * active display user to this (or from this) task.
	 * @arg self
	 *	a pointer to this task
	 * @arg active
	 *      1 if this task gets the display, 0 if it loses the display
	 */
	void (*display_notify) (struct task_t *self, int active);
};

/* current time, maintained by task manager */
extern struct timeval g_time;

extern void task_init (struct task_t *self, const char *instance);
extern struct task_t *task_find (const char *instance);
extern void task_fini (struct task_t *self);

#endif /* __TASK_H__ */

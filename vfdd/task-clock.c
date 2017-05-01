#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "vfdd.h"
#include "task.h"
#include "task-display.h"

struct task_clock_t {
	struct task_t task;
	const char *format;
	const char *separator;
	const char *display_task;
	int separator_always;
	int priority;
	time_t last_time;
	struct task_display_t *display;
};

static void task_clock_post_init (struct task_t *self)
{
	struct task_clock_t *self_clock = (struct task_clock_t *)self;
	self_clock->display = (struct task_display_t *)task_find (self_clock->display_task);
}

static void task_clock_fini (struct task_t *self)
{
	struct task_clock_t *self_clock = (struct task_clock_t *)self;

	task_fini (&self_clock->task);

	free (self_clock);
}

static unsigned task_clock_run (struct task_t *self, struct timeval *tv)
{
	struct task_clock_t *self_clock = (struct task_clock_t *)self;
	char buff [32];
	struct tm tm;
	unsigned ms;

	trace ("%s: run\n", self->instance);

	if (self_clock->last_time != tv->tv_sec) {
		self_clock->last_time = tv->tv_sec;

		localtime_r (&tv->tv_sec, &tm);
		strftime (buff, sizeof (buff), self_clock->format, &tm);
		if (self_clock->display)
			self_clock->display->set_display (self_clock->display,
				self, self_clock->priority, buff);
	}

	ms = (tv->tv_usec / 1000) % 1000;

	/* display flashing H:S separator every 0.5 sec */
	if (self_clock->separator && self_clock->display) {
		int ena = self_clock->separator_always ? 1 :
			(self_clock->display->display_task == self);
		self_clock->display->set_indicator (self_clock->display,
			self_clock->separator, ena && (ms < 500));
	}

	/* sleep up to next half of second */
	return ((1 + (ms / 500)) * 500) - ms;
}

static void task_clock_display_notify (struct task_t *self, struct timeval *tv, int active)
{
	struct task_clock_t *self_clock = (struct task_clock_t *)self;
	unsigned ms;

	trace ("%s: display_notify %d\n", self->instance, active);

	/* refresh the double colon indicator */
	ms = (tv->tv_usec / 1000) % 1000;

	/* display flashing H:S separator every 0.5 sec */
	if (self_clock->separator && self_clock->display) {
		int ena = self_clock->separator_always ? 1 : active;
		self_clock->display->set_indicator (self_clock->display,
			self_clock->separator, ena && (ms < 500));
	}
}

struct task_t *task_clock_new (const char *instance)
{
	struct task_clock_t *self = calloc (1, sizeof (struct task_clock_t));

	task_init (&self->task, instance);

	self->task.run = task_clock_run;
	self->task.post_init = task_clock_post_init;
	self->task.fini = task_clock_fini;
	self->task.display_notify = task_clock_display_notify;

	self->format = cfg_get_str (instance, "format", DEFAULT_CLOCK_FORMAT);
	self->separator = cfg_get_str (instance, "separator", DEFAULT_CLOCK_SEPARATOR);
	self->display_task = cfg_get_str (instance, "display", DEFAULT_DISPLAY);
	self->separator_always = cfg_get_int (instance, "separator.always", 0);
	self->priority = cfg_get_int (instance, "priority", DEFAULT_PRIORITY);

	if (!*self->separator)
		self->separator = NULL;

	trace ("	format '%s' separator '%s' (always %d) priority %d display '%s'\n",
		self->format, self->separator, self->separator_always,
		self->priority, self->display_task);

	return &self->task;
}

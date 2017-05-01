#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "vfdd.h"
#include "task.h"
#include "task-display.h"

struct task_temp_t {
	struct task_t task;
	const char *value;
	const char *format;
	const char *display_task;
	int divider;
	int priority;
	struct task_display_t *display;
};

static void task_temp_post_init (struct task_t *self)
{
	struct task_temp_t *self_temp = (struct task_temp_t *)self;
	self_temp->display = (struct task_display_t *)task_find (self_temp->display_task);
}

static void task_temp_fini (struct task_t *self)
{
	struct task_temp_t *self_temp = (struct task_temp_t *)self;

	task_fini (&self_temp->task);

	free (self_temp);
}

static unsigned task_temp_run (struct task_t *self, struct timeval *tv)
{
	struct task_temp_t *self_temp = (struct task_temp_t *)self;
	char *tmp;
	int temp;

	trace ("%s: run\n", self->instance);

	tmp = sysfs_read (self_temp->value);
	if (tmp == NULL)
		return 10000;

	temp = strtol (tmp, NULL, 0) / self_temp->divider;

	if (self_temp->display) {
		char buff [20];
		snprintf (buff, sizeof (buff), self_temp->format, temp);
		self_temp->display->set_display (self_temp->display,
			self, self_temp->priority, buff);
	}

	return 500;
}

struct task_t *task_temp_new (const char *instance)
{
	struct task_temp_t *self = calloc (1, sizeof (struct task_temp_t));

	task_init (&self->task, instance);

	self->task.run = task_temp_run;
	self->task.post_init = task_temp_post_init;
	self->task.fini = task_temp_fini;

	self->value = cfg_get_str (instance, "value", DEFAULT_TEMP_VALUE);
	self->format = cfg_get_str (instance, "format", DEFAULT_TEMP_FORMAT);
	self->display_task = cfg_get_str (instance, "display", DEFAULT_DISPLAY);
	self->divider = cfg_get_int (instance, "divider", DEFAULT_TEMP_DIVIDER);
	self->priority = cfg_get_int (instance, "priority", 0);

	trace ("	format '%s' priority %d display '%s' value '%s'\n",
		self->format, self->priority, self->display_task, self->value);

	return &self->task;
}

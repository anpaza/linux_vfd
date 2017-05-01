#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "vfdd.h"
#include "task.h"
#include "task-display.h"

struct task_disk_t {
	struct task_t task;
	const char *device;
	int field;
	int threshold;
	const char *indicator;
	const char *display_task;
	struct task_display_t *display;
	long old_value;
	int indicator_enabled;
};

static void task_disk_post_init (struct task_t *self)
{
	struct task_disk_t *self_disk = (struct task_disk_t *)self;
	self_disk->display = (struct task_display_t *)task_find (self_disk->display_task);
}

static void task_disk_fini (struct task_t *self)
{
	struct task_disk_t *self_disk = (struct task_disk_t *)self;

	task_fini (&self_disk->task);

	free (self_disk);
}

static unsigned task_disk_run (struct task_t *self, struct timeval *tv)
{
	struct task_disk_t *self_disk = (struct task_disk_t *)self;
	char buff [50];
	char *tmp;
	long stat [11];
	int enable = 0;

	if (!self_disk->display)
		return 10000;

	trace ("%s: run\n", self->instance);

	snprintf (buff, sizeof (buff), "/sys/block/%s/stat", self_disk->device);
	tmp = sysfs_read (buff);
	if (tmp == NULL)
		return 10000;

	sscanf (tmp, "%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld",
		&stat [0], &stat [1], &stat [2], &stat [3], 
		&stat [4], &stat [5], &stat [6], &stat [7], 
		&stat [8], &stat [9], &stat [10]);

	free (tmp);

	if (self_disk->old_value != -1) {
		long delta = stat [self_disk->field] - self_disk->old_value;
		if (delta < self_disk->threshold)
			goto setind;

		enable = 1;
	}

	self_disk->old_value = stat [self_disk->field];

setind:
	if (self_disk->indicator_enabled != enable) {
		self_disk->indicator_enabled = enable;
		self_disk->display->set_indicator (self_disk->display,
			self_disk->indicator, enable);
	}

	return 250;
}

struct task_t *task_disk_new (const char *instance)
{
	struct task_disk_t *self = calloc (1, sizeof (struct task_disk_t));

	task_init (&self->task, instance);

	self->task.run = task_disk_run;
	self->task.post_init = task_disk_post_init;
	self->task.fini = task_disk_fini;

	self->display_task = cfg_get_str (instance, "display", DEFAULT_DISPLAY);
	self->device = cfg_get_str (instance, "device", DEFAULT_DISK_DEVICE);
	self->field = cfg_get_int (instance, "field", DEFAULT_DISK_FIELD);
	self->threshold = cfg_get_int (instance, "threshold", DEFAULT_DISK_THRESHOLD);
	self->indicator = cfg_get_str (instance, "indicator", DEFAULT_DISK_INDICATOR);
	self->old_value = -1;
	self->indicator_enabled = -1;

	if ((self->field <= 0) || (self->field > 11)) {
		fprintf (stderr, "%s: invalid field number %d, must be 1 to 11\n",
			self->task.instance, self->field);
		free (self);
		return NULL;
	}

	trace ("	device '%s' field %d threshold %d indicator '%s'\n",
		self->device, self->field, self->threshold, self->indicator);

	self->field--;

	return &self->task;
}

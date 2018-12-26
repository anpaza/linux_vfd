#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "vfdd.h"
#include "task.h"
#include "task-display.h"

struct task_dot_t {
	struct task_t task;
	const char *attr;
	const char *display_task;
	const char *indicator;
	int field;
	int threshold;
	int priority;
	struct task_display_t *display;
	int indicator_enabled;
};

static const char *whitespace = " \t\n\r";

static void task_dot_post_init (struct task_t *self)
{
	struct task_dot_t *self_dot = (struct task_dot_t *)self;
	self_dot->display = (struct task_display_t *)task_find (self_dot->display_task);
}

static void task_dot_fini (struct task_t *self)
{
	struct task_dot_t *self_dot = (struct task_dot_t *)self;

	task_fini (&self_dot->task);

	free (self_dot);
}

static unsigned task_dot_run (struct task_t *self)
{
	struct task_dot_t *self_dot = (struct task_dot_t *)self;
	char *tmp, *cur;
	int i, val;

	trace ("%s: run\n", self->instance);

	tmp = sysfs_read (self_dot->attr);
	if (tmp == NULL)
		return 10000;

        cur = tmp + strspn (tmp, whitespace);

        // skip field-1 fields
        for (i = 1; i < self_dot->field; i++)
        {
            cur += strcspn (cur, whitespace);
            cur += strspn (cur, whitespace);
        }

	val = strtol (cur, NULL, 0);
	free (tmp);

        val = (val >= self_dot->threshold) ? 1 : 0;
	if (self_dot->display && (val != self_dot->indicator_enabled)) {
		self_dot->indicator_enabled = val;
		self_dot->display->set_indicator (self_dot->display, self,
			self_dot->indicator, val);
	}

	return 500;
}

struct task_t *task_dot_new (const char *instance)
{
	struct task_dot_t *self = calloc (1, sizeof (struct task_dot_t));

	task_init (&self->task, instance);

	self->task.run = task_dot_run;
	self->task.post_init = task_dot_post_init;
	self->task.fini = task_dot_fini;

	self->display_task = cfg_get_str (instance, "display", DEFAULT_DISPLAY);
	self->attr = cfg_get_str (instance, "attr", DEFAULT_DOT_ATTR);
	self->field = cfg_get_int (instance, "field", DEFAULT_DOT_FIELD);
	self->threshold = cfg_get_int (instance, "threshold", DEFAULT_DOT_THRESHOLD);
	self->indicator = cfg_get_str (instance, "indicator", DEFAULT_DOT_INDICATOR);
	self->priority = cfg_get_int (instance, "priority", DEFAULT_PRIORITY);

	// force indicator refresh
	self->indicator_enabled = -1;

	trace ("	if attr '%s'.%d >= %d display '%s' indicator '%s'\n",
		self->attr, self->field, self->threshold, self->display_task, self->indicator);

	return &self->task;
}

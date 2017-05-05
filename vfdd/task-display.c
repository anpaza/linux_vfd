#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "vfdd.h"
#include "task-display.h"

static unsigned task_display_run (struct task_t *self, struct timeval *tv)
{
	struct task_display_t *self_display = (struct task_display_t *)self;
	struct display_user_t *user;
	struct task_t *old_display_task = self_display->display_task;
	int min_prio = 1000000;
	int find_next = 1;

	trace ("%s: run\n", self->instance);

	/* if there are no display users, just sleeeeeeep */
	if (!self_display->users->task)
		return 1000000;

	/* switch display to next task */
	for (user = self_display->users; user; user = user->next) {
		if (min_prio > user->priority)
			min_prio = user->priority;

		if (find_next && (self_display->display_task == user->task)) {
			find_next = 0;
			if (user->next)
				self_display->display_task = user->next->task;
			else
				self_display->display_task = self_display->users->task;
		}
	}
	/* if we did not found the current display user, just start from beginning */
	if (!self_display->display_task)
		self_display->display_task = self_display->users->task;

	if (old_display_task && (old_display_task != self_display->display_task))
		if (old_display_task->display_notify)
			old_display_task->display_notify (old_display_task, tv, 0);

	for (user = self_display->users; user; user = user->next)
		if (self_display->display_task == user->task) {
			unsigned adj, sleep_time;

			trace ("%s: show [%s]\n", self_display->task.instance, user->display);

			sysfs_set_str (self_display->device, "display", user->display);

			if (self_display->display_task->display_notify)
				self_display->display_task->display_notify (self_display->display_task, tv, 1);

			sleep_time = (self_display->quantum * user->priority) / min_prio;
			// adjust sleep_time to nearest time quantum boundary
			adj = (tv->tv_usec / 1000) % self_display->quantum;
			return sleep_time > adj ? sleep_time - adj : self_display->quantum - adj;
		}

	trace ("%s: should never get here\n", self_display->task.instance);
	return 1000000;
}

static void task_display_set_display (struct task_display_t *self, struct task_t *source, int priority,
	const char *string)
{
	struct display_user_t *next;
	struct display_user_t **user;
	struct task_display_t *self_display = (struct task_display_t *)self;

	trace ("%s: set_display '%s' prio %d\n", self->task.instance, string, priority);

	for (user = &self_display->users; *user; user = &(*user)->next) {
		if ((*user)->task == source) {
			free ((*user)->display);
			if (string == NULL || (priority < 1)) {
				/* remove user from list */
				next = (*user)->next;
				free (*user);
				*user = next;
				return;
			}
			next = *user;
			goto update_display;
		}
	}

	if (string == NULL || (priority < 1))
		return;

	/* if there were no display users, schedule for running ASAP */
	if (self_display->users == NULL)
		self_display->task.sleep_ms = 0;

	/* create display user */
	next = calloc (1, sizeof (struct display_user_t));
	next->next = self_display->users;
	self_display->users = next;
	next->task = source;

update_display:
	next->display = strdup (string);
	next->priority = priority;

	if (self_display->display_task == source) {
		trace ("%s: show [%s]\n", self_display->task.instance, next->display);
		sysfs_set_str (self_display->device, "display", next->display);
	}
}

static void task_display_set_indicator (struct task_display_t *self, const char *indicator, int enable)
{
	int i;
	enable = (enable != 0);

	trace ("%s: set_indicator '%s' %s\n", self->task.instance, indicator,
		enable ? "on" : "off");

	for (i = 0; i < self->indicator_count; i++) {
		struct indicator_t *ind = &self->indicators [i];
		if (strcmp (indicator, ind->name) == 0) {
			char tmp [32];
			snprintf (tmp, sizeof (tmp), "%s %d", indicator, enable);
			sysfs_set_str (self->device, "dotled", tmp);
			return;
		}
	}

	trace ("%s: no indicator named '%s'\n", self->task.instance, indicator);
}

static void task_display_set_brightness (struct task_display_t *self, int value)
{
	int raw_value;

	if (value < 0)
		value = 0;
	else if (value > 100)
		value = 100;

	trace ("%s: set_brightness %d\n", self->task.instance, value);

	self->brightness = value;
	raw_value = (value * self->brightness_max) / 100;
	sysfs_set_int (self->device, "brightness", raw_value);
}

static void task_display_fini (struct task_t *self)
{
	struct task_display_t *self_display = (struct task_display_t *)self;
	struct display_user_t **user;
	int i;

	/* clear display */
	sysfs_set_str (self_display->device, "display", "");

	/* clear all indicators */
	for (i = 0; i < self_display->indicator_count; i++)
		task_display_set_indicator (self_display,
			self_display->indicators [i].name, 0);

	if (self_display->indicators) {
		for (i = 0; i < self_display->indicator_count; i++)
			free (self_display->indicators [i].name);
		free (self_display->indicators);
	}

	for (user = &self_display->users; *user; ) {
		struct display_user_t *next = (*user)->next;
		free ((*user)->display);
		free (*user);
		*user = next;
	}

	if (self_display->overlay)
		free (self_display->overlay);

	task_fini (&self_display->task);

	free (self_display);
}

struct task_t *task_display_new (const char *instance)
{
	char *cur, *tmp;
	struct task_display_t *self = calloc (1, sizeof (struct task_display_t));

	task_init (&self->task, instance);

	self->task.run = task_display_run;
	self->task.fini = task_display_fini;
	self->set_display = task_display_set_display;
	self->set_indicator = task_display_set_indicator;
	self->set_brightness = task_display_set_brightness;

	self->device = cfg_get_str (instance, "device", DEFAULT_DEVICE);
	if (sysfs_exists (self->device) != 0) {
		fprintf (stderr, "sysfs entry '%s' not found, aborting task '%s'\n",
			self->device, self->task.instance);
		task_display_fini (&self->task);
		return NULL;
	}

	self->brightness = cfg_get_int (instance, "brightness", DEFAULT_BRIGHTNESS);
	self->quantum = cfg_get_int (instance, "quantum", DEFAULT_DISPLAY_QUANTUM);
	trace ("	device [%s] brightness %d quantum %d\n",
		self->device, self->brightness, self->quantum);

	/* detect available dot LED indicators */
	tmp = (char *)sysfs_get_str (self->device, "dotled");
	if (tmp != NULL) {
		char *cur = tmp;
		char name [20 + 1];
		int ena, word, bit;

		for (;;) {
			int n = sscanf (cur, " %20s %d %d %d", name, &ena, &word, &bit);
			if (n != 4)
				break;

			n = self->indicator_count;
			self->indicators = realloc (self->indicators,
				(n + 1) * sizeof (struct indicator_t));

			self->indicators [n].name = strdup (name);
			self->indicators [n].on = ena;
			self->indicator_count++;
			trace ("	indicator [%s] enabled %d, word %d, bit %d\n",
				self->indicators [n].name, self->indicators [n].on, word, bit);

			cur = strchr (cur, '\n');
			if (!cur)
				break;
			cur++;
		}
	}

	/* initialize overlay */
	tmp = sysfs_get_str (self->device, "overlay");
	if (!tmp) {
		fprintf (stderr, "device '%s' is incompatible, aborting task '%s'\n",
			self->device, self->task.instance);
		task_display_fini (&self->task);
		return NULL;
	}
	cur = tmp;

	for (self->overlay_len = 0; ; ) {
		char *endp;

		while (isspace (*cur))
			cur++;

		uint16_t n = strtoul (cur, &endp, 16);
		if (endp == cur)
			break;

		self->overlay_len++;
		self->overlay = realloc (self->overlay, self->overlay_len * sizeof (uint16_t));
		self->overlay [self->overlay_len - 1] = n;

		cur = endp;
	}

	free (tmp);

	/* get max brightness */
	self->brightness_max = sysfs_get_int (self->device, "brightness_max");
	self->set_brightness (self, self->brightness);

	return &self->task;
}

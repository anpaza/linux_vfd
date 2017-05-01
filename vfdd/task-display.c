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

static void task_display_update_overlay (struct task_display_t *self)
{
	int i;
	char *buff = malloc (self->overlay_len * 5);

	for (i = 0; i < self->overlay_len; i++)
		sprintf (buff + i * 5, "%04x ", self->overlay [i]);
	buff [self->overlay_len * 5 - 1] = 0;

	sysfs_set_str (self->device, "overlay", buff);
	free (buff);
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
			uint16_t mask = (1 << ind->bit [1]);
			int ena = (self->overlay [ind->bit [0]] & mask) != 0;
			if (ena == enable)
				break;

			if (enable)
				self->overlay [ind->bit [0]] |= mask;
			else
				self->overlay [ind->bit [0]] &= ~mask;

			task_display_update_overlay (self);
			break;
		}
	}
}

static void task_display_fini (struct task_t *self)
{
	struct task_display_t *self_display = (struct task_display_t *)self;

	if (self_display->indicators)
		free (self_display->indicators);
	if (self_display->overlay)
		free (self_display->overlay);

	task_fini (&self_display->task);

	free (self_display);
}

static void task_display_set_brightness (struct task_display_t *self, int value)
{
	struct task_display_t *self_display = (struct task_display_t *)self;
	int raw_value;

	if (value < 0)
		value = 0;
	else if (value > 100)
		value = 100;

	trace ("%s: set_brightness %d\n", self_display->task.instance, value);

	self_display->brightness = value;
	raw_value = (value * self_display->brightness_max) / 100;
	sysfs_set_int (self_display->device, "brightness", raw_value);
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
	self->brightness = cfg_get_int (instance, "brightness", DEFAULT_BRIGHTNESS);
	self->quantum = cfg_get_int (instance, "quantum", DEFAULT_DISPLAY_QUANTUM);
	trace ("	device [%s] brightness %d quantum %d\n",
		self->device, self->brightness, self->quantum);

	tmp = (char *)cfg_get_str (instance, "indicators", NULL);
	if (tmp != NULL) {
		char *tok, *save;
		char indbit [20];

		tmp = strdup (tmp);
		for (tok = strtok_r (tmp, g_spaces, &save); tok != NULL; tok = strtok_r (NULL, g_spaces, &save)) {
			int n = self->indicator_count;
			self->indicators = realloc (self->indicators,
				(n + 1) * sizeof (struct indicator_t));

			snprintf (indbit, sizeof (indbit), "bit.%s", tok);
			if (cfg_get_int_2 (instance, indbit, self->indicators [n].bit) < 0) {
				fprintf (stderr, "%s not defined, ignoring indicator %s\n",
					indbit, tok);
				continue;
			}

			self->indicators [n].name = strdup (tok);
			self->indicator_count++;
			trace ("	indicator [%s] cell %d bit %d\n", tok,
				self->indicators [n].bit [0], self->indicators [n].bit [1]);
		}
		free (tmp);
	}

	/* initialize overlay */
	tmp = sysfs_get_str (self->device, "overlay", NULL);
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

	/* get max brightness */
	self->brightness_max = sysfs_get_int (self->device, "brightness_max", 1);
	self->set_brightness (self, self->brightness);

	return &self->task;
}

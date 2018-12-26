#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "vfdd.h"
#include "task-display.h"

static void task_display_update (struct task_display_t *self);

static int task_display_qualify (struct display_user_t *user)
{
	return (user != NULL) && (user->display != NULL);
}

static void task_display_set_active (struct task_display_t *self, struct display_user_t *user)
{
	if (self->active_user == user)
		return;

	trace ("%s: --------- set active display task [%s]\n",
		self->task.instance, user ? user->task->instance : "NONE");

	struct display_user_t *cur = self->active_user;
	if (cur && cur->task->display_notify)
		cur->task->display_notify (cur->task, 0);

	self->active_user = user;
	self->task.attention = 1;

	if (user && user->task->display_notify)
		user->task->display_notify (user->task, 1);
}

static void task_display_next (struct task_display_t *self)
{
	struct display_user_t *user, *next;
	int find_next = 1;
	int pass;

	/* if active task has max priority, don't switch away */
	if (self->active_user && self->active_user->priority == PRIORITY_MAX)
		return;

	next = self->active_user;

	/* switch display to next task with a non-NULL display string */
	for (pass = 0; find_next && (pass < 2); pass++) {
		/* if there's no active display user, just start from beginning */
		if (!next)
			if (task_display_qualify (next = self->users))
				find_next = 0;

		for (user = self->users; find_next && user; user = user->next) {
			if (next == user) {
				if (user->next)
					next = user->next;
				else
					next = self->users;

				if (task_display_qualify (next))
					find_next = 0;
			}
		}
	}

	if (find_next)
		next = NULL;

	task_display_set_active (self, next);
}

static unsigned task_display_run (struct task_t *self)
{
	struct task_display_t *self_display = (struct task_display_t *)self;
	struct display_user_t *user;
	unsigned adj, sleep_time;

	trace ("%s: run (%u) due to %s\n", self->instance, self->sleep_ms,
		(self->sleep_ms == 0) ? "timeout" : "attention request");

	/* if time slot ended, switch to next display user */
	if (self->sleep_ms == 0)
		task_display_next (self_display);

	if (!(user = self_display->active_user))
		return 1000000;

	task_display_update (self_display);

	sleep_time = (self_display->quantum * user->priority) / self_display->min_priority;
	// adjust sleep_time to nearest time quantum boundary
	adj = (g_time.tv_usec / 1000) % self_display->quantum;
	return sleep_time > adj ? sleep_time - adj : self_display->quantum - adj;
}

static struct display_user_t *task_display_get_user (struct task_display_t *self, struct task_t *source)
{
	struct display_user_t *user;

	for (user = self->users; user; user = user->next) {
		if (user->task == source)
			return user;
	}

	/* no display user structure, allocate a new one */
	user = calloc (1, sizeof (struct display_user_t));
	user->task = source;
	user->next = self->users;
	self->users = user;

	return user;
}

static void task_display_remove_user (struct task_display_t *self, struct task_t *source)
{
	struct display_user_t *next;
	struct display_user_t **cur;

	for (cur = &self->users; *cur; cur = &(*cur)->next) {
		if ((*cur)->task == source) {
			struct display_user_t *user = *cur;
			if (user->display)
				free (user->display);
			/* remove cur from list */
			next = user->next;
			free (user);
			*cur = next;

			/* if we're removing the active display user, switch to next */
			if (self->active_user == user) {
				self->active_user = next;
				self->task.attention = 1;
			}

			return;
		}
	}
}

static void task_display_update (struct task_display_t *self)
{
	struct display_user_t *user;
	const char *text = NULL;
	unsigned dotled = 0;
	unsigned i;

	if ((user = self->active_user) != NULL)
		text = user->display;

	if (!text)
		text = "";

	trace ("%s: show [%s]\n", self->task.instance, text);
	sysfs_set_str (self->device, "display", text);

	/* dotled is a logical OR of all display tasks */
	for (user = self->users; user; user = user->next)
		dotled |= user->dotled;

	int dotled_changed = 0;
	for (i = 0; i < self->indicator_count; i++) {
		struct indicator_t *ind = &self->indicators [i];
		unsigned mask = 1 << i;
		unsigned need_mask = ind->mask;

		if ((dotled & mask) == 0)
			need_mask = 0;
		if ((self->overlay [ind->word] & ind->mask) != need_mask) {
			self->overlay [ind->word] = (self->overlay [ind->word] & ~ind->mask) | need_mask;
			dotled_changed = 1;
		}
	}

	if (dotled_changed) {
		for (i = 0; i < self->overlay_len; i++) {
			snprintf (self->overlay_buff + i * 5, 5, "%04X", self->overlay [i]);
			if (i < self->overlay_len - 1)
				self->overlay_buff [i * 5 + 4] = ' ';
		}

		trace ("%s: overlay [%s]\n", self->task.instance, self->overlay_buff);
		sysfs_set_str (self->device, "overlay", self->overlay_buff);
	}
}

static void task_display_set_display (struct task_display_t *self, struct task_t *source, int priority,
	const char *string)
{
	struct display_user_t *user;

	trace ("%s: set_display '%s' prio %d\n", self->task.instance, string, priority);

	if ((priority < 1) || !string) {
		task_display_remove_user (self, source);
		return;
	}

	user = task_display_get_user (self, source);
	if (user->display) {
		free (user->display);
		user->display = NULL;
	}

	user->display = strdup (string);
	user->priority = priority;
	if (self->min_priority > priority)
		self->min_priority = priority;

	if (priority == PRIORITY_MAX)
		task_display_set_active (self, user);

	if (self->active_user == user)
		self->task.attention = 1;
}

static void task_display_set_indicator (struct task_display_t *self, struct task_t *source,
	const char *indicator, int enable)
{
	int i;
	struct display_user_t *user;

	trace ("%s: %s set_indicator '%s' %s\n", self->task.instance, source->instance,
		indicator, enable ? "on" : "off");

	user = task_display_get_user (self, source);

	for (i = 0; i < self->indicator_count; i++) {
		struct indicator_t *ind = &self->indicators [i];
		if (strcmp (indicator, ind->name) == 0) {
			if (enable != 0)
				user->dotled |= (1 << i);
			else
				user->dotled &= ~(1 << i);

			if (self->active_user == user)
				self->task.attention = 1;

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
	raw_value = (value * self->brightness_max + 50) / 100;
	sysfs_set_int (self->device, "brightness", raw_value);
}

static int task_display_is_active (struct task_display_t *self, struct task_t *source)
{
	if (!self->active_user)
		return 0;

	return (self->active_user->task == source);
}

static void task_display_fini (struct task_t *self)
{
	struct task_display_t *self_display = (struct task_display_t *)self;
	struct display_user_t **user;
	int i;

	/* unlock display */
	self_display->active_user = 0;

	/* free display users */
	for (user = &self_display->users; *user; ) {
		struct display_user_t *next = (*user)->next;
		free ((*user)->display);
		free (*user);
		*user = next;
	}

	/* update display (effectively clear) */
	task_display_update (self_display);

	if (self_display->indicators) {
		for (i = 0; i < self_display->indicator_count; i++)
			free (self_display->indicators [i].name);
		free (self_display->indicators);
	}

	if (self_display->overlay)
		free (self_display->overlay);

	task_fini (&self_display->task);

	free (self_display);
}

struct task_t *task_display_new (const char *instance)
{
	char *tmp;
	struct task_display_t *self = calloc (1, sizeof (struct task_display_t));

	task_init (&self->task, instance);

	self->min_priority = 1000000;

	self->task.run = task_display_run;
	self->task.fini = task_display_fini;
	self->set_display = task_display_set_display;
	self->set_indicator = task_display_set_indicator;
	self->set_brightness = task_display_set_brightness;
	self->is_active = task_display_is_active;

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
			self->indicators [n].word = word;
			self->indicators [n].mask = 1 << bit;
			self->indicator_count++;
			trace ("	indicator [%s] enabled %d, word %d, bit %d\n",
				self->indicators [n].name, ena, word, bit);

			if (word + 1 > self->overlay_len)
				self->overlay_len = word + 1;

			cur = strchr (cur, '\n');
			if (!cur)
				break;
			cur++;
		}
	}

	/* initialize overlay */
	self->overlay = calloc (self->overlay_len, sizeof (uint16_t));
	self->overlay_buff = malloc (self->overlay_len * 5);

	/* get max brightness */
	self->brightness_max = sysfs_get_int (self->device, "brightness_max");
	self->set_brightness (self, self->brightness);

	return &self->task;
}

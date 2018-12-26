/*
 * Task management core
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "vfdd.h"
#include "task.h"

static struct task_t *g_tasks = NULL;
struct timeval g_time;

/* declare task constructors below */

extern struct task_t *task_display_new (const char *instance);
extern struct task_t *task_suspend_new (const char *instance);
extern struct task_t *task_clock_new (const char *instance);
extern struct task_t *task_dot_new (const char *instance);
extern struct task_t *task_temp_new (const char *instance);
extern struct task_t *task_disk_new (const char *instance);

static struct task_module_t {
	const char *name;
	struct task_t *(*new) (const char *instance);
} task_modules [] = {
	{ "display", task_display_new },
	{ "suspend", task_suspend_new },
	{ "clock", task_clock_new },
  	{ "dot", task_dot_new },
	{ "temp", task_temp_new },
	{ "disk", task_disk_new },
};

static void task_add (struct task_t *task)
{
	if (!task)
		return;

	if (g_tasks == NULL)
		g_tasks = task;
	else {
		task->next = g_tasks;
		g_tasks = task;
	}
}

static int task_cmp (const char *tok, const char *pat)
{
	while (*pat)
		if (*tok++ != *pat++)
			return -1;
	if (*tok && *tok != '/')
		return -1;
	return 0;
}

void task_init (struct task_t *self, const char *instance)
{
	trace ("initializing '%s' plugin\n", instance);

	self->instance = strdup (instance);
}

void task_fini (struct task_t *self)
{
	trace ("finalizing '%s' plugin\n", self->instance);

	free (self->instance);
}

struct task_t *task_find (const char *instance)
{
	struct task_t *cur;

	for (cur = g_tasks; cur != NULL; cur = cur->next)
		if (strcmp (instance, cur->instance) == 0)
			return cur;

	return NULL;
}

int tasks_init ()
{
	char *tsk = strdup (cfg_get_str (NULL, "tasks", DEFAULT_TASKS));
	char *tok, *save;
	struct task_t *cur;

	for (tok = strtok_r (tsk, g_spaces, &save); tok != NULL; tok = strtok_r (NULL, g_spaces, &save)) {
		int i, ok = 0;

		for (i = 0; i < ARRAY_SIZE (task_modules); i++)
			if (task_cmp (tok, task_modules [i].name) == 0) {
				task_add (task_modules [i].new (tok));
				ok = 1;
				break;
			}

		if (ok == 0)
			fprintf (stderr, "Task '%s' unknown, ignoring\n", tok);
	}

	free (tsk);

	if (g_tasks == NULL) {
		fprintf (stderr, "No valid tasks in config, aborting\n");
		return -EINVAL;
	}

	for (cur = g_tasks; cur; cur = cur->next)
		if (cur->post_init)
			cur->post_init (cur);

	return 0;
}

void tasks_run ()
{
	struct task_t *cur;
	struct timeval tv;

	gettimeofday (&g_time, NULL);

	while (!g_shutdown) {
		/* sleep no more than 10 seconds */
		unsigned sleep_time = 10000;

		/* run all tasks which are ready to run
		 * until we're left with no ready tasks
		 */
		int ready = 1;
		while (ready) {
			ready = 0;


			/* run all ready-to-run tasks */
			for (cur = g_tasks; cur; cur = cur->next) {
				if (cur->sleep_ms == 0) {
					cur->sleep_ms = cur->run (cur);
				} else if (cur->attention) {
					cur->attention = 0;
					cur->run (cur);
				}
				if (sleep_time > cur->sleep_ms)
					sleep_time = cur->sleep_ms;
			}

			/* check if there are more ready-to-run tasks */
			for (cur = g_tasks; cur; cur = cur->next)
				if (cur->attention || (cur->sleep_ms == 0)) {
					ready = 1;
					break;
				}
		}

		usleep (sleep_time * 1000);

		/* find out how much we actually slept */
		tv = g_time;
		gettimeofday (&g_time, NULL);

		sleep_time = ((g_time.tv_sec & 255) * 1000 + g_time.tv_usec / 1000) - 
			((tv.tv_sec & 255) * 1000 + tv.tv_usec / 1000);

		for (cur = g_tasks; cur; cur = cur->next)
			if (cur->sleep_ms > sleep_time)
				cur->sleep_ms -= sleep_time;
			else
				cur->sleep_ms = 0;
	}
}

void tasks_fini ()
{
	struct task_t *next;
	struct task_t **cur;

	for (cur = &g_tasks; *cur; ) {
		next = (*cur)->next;
		(*cur)->fini (*cur);
		*cur = next;
	}
}

/*
 * This task will listen for kernel uevents from vfd driver
 * and display something special when device goes into suspend mode,
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#include "vfdd.h"
#include "task.h"
#include "task-display.h"

struct task_suspend_t {
	struct task_t task;
	const char *text;
	const char *indicators;
	const char *display_task;
	int brightness;
	struct task_display_t *display;
	int uevent_sock;
	pthread_t uevent_tid;
	volatile uint8_t suspended;
	volatile uint8_t shutdown;
};

static int uevent_open (int buf_sz);
static void *uevent_watching_thread (void *_self);

static void task_suspend_post_init (struct task_t *self)
{
	struct task_suspend_t *self_suspend = (struct task_suspend_t *)self;
	self_suspend->display = (struct task_display_t *)task_find (self_suspend->display_task);
	self_suspend->shutdown = 0;
	pthread_create (&self_suspend->uevent_tid, NULL, uevent_watching_thread, self_suspend);
}

static void task_suspend_fini (struct task_t *self)
{
	struct task_suspend_t *self_suspend = (struct task_suspend_t *)self;

	self_suspend->shutdown = 1;
	task_fini (&self_suspend->task);

	close (self_suspend->uevent_sock);

	free (self_suspend);
}

static unsigned task_suspend_run (struct task_t *self)
{
	struct task_suspend_t *self_suspend = (struct task_suspend_t *)self;

	/* check if a uevent occured */
	if (self_suspend->task.attention) {
	}

	return 10000;
}

struct task_t *task_suspend_new (const char *instance)
{
	struct task_suspend_t *self = calloc (1, sizeof (struct task_suspend_t));

	task_init (&self->task, instance);

	self->task.run = task_suspend_run;
	self->task.post_init = task_suspend_post_init;
	self->task.fini = task_suspend_fini;

	self->display_task = cfg_get_str (instance, "display", DEFAULT_DISPLAY);
	self->text = cfg_get_str (instance, "text", DEFAULT_SUSPEND_TEXT);
	self->indicators = cfg_get_str (instance, "indicators", DEFAULT_SUSPEND_INDICATORS);
	self->brightness = cfg_get_int (instance, "brightness", DEFAULT_SUSPEND_BRIGHTNESS);

	self->uevent_sock = uevent_open (16 * 1024);
	if (self->uevent_sock < 0) {
		fprintf (stderr, "%s: failed to open uevent socket\n",
			self->task.instance);
		free (self);
		return NULL;
	}

	trace ("	text '%s' indicators '%s' brightness %d display '%s'\n",
		self->text, self->indicators, self->brightness, self->display_task);

	return &self->task;
}

// crazy kernel stuff we don't want to see most of the time...

#define __USE_GNU
#include <fcntl.h>
#include <poll.h>
#include <linux/netlink.h>
#include <sys/socket.h>

#ifndef SOCK_CLOEXEC
#  define SOCK_CLOEXEC O_CLOEXEC
#endif

#define CMSG_FOREACH(cmsg, mh)                                          \
	for ((cmsg) = CMSG_FIRSTHDR(mh); (cmsg); (cmsg) = CMSG_NXTHDR((mh), (cmsg)))

static int uevent_open (int buf_sz)
{
	struct sockaddr_nl addr;
	int sock;

	memset (&addr, 0, sizeof (addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_pid = getpid ();
	addr.nl_groups = 0xffffffff;

	sock = socket (PF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC, NETLINK_KOBJECT_UEVENT);
	if (sock < 0)
		return -1;

	setsockopt (sock, SOL_SOCKET, SO_RCVBUFFORCE, &buf_sz, sizeof (buf_sz));

	int one = 1;
	setsockopt (sock, SOL_SOCKET, SO_PASSCRED, &one, sizeof (one));

	if (bind (sock, (struct sockaddr *)&addr, sizeof (addr)) < 0) {
		close (sock);
		return -1;
	}

	fcntl (sock, F_SETFL, O_NONBLOCK);

	return sock;
}

static void uevent_handle (struct task_suspend_t *self)
{
	for (;;)
	{
		char msg [1024];
		struct sockaddr_nl addr;
		struct iovec iovec = {
			.iov_base = &msg,
			.iov_len = sizeof(msg),
		};
		union {
			struct cmsghdr cmsghdr;
			uint8_t buf [CMSG_SPACE (sizeof (struct ucred))];
		} control = {};
		struct msghdr msghdr = {
			.msg_name = &addr,
			.msg_namelen = sizeof (addr),
			.msg_iov = &iovec,
			.msg_iovlen = 1,
			.msg_control = &control,
			.msg_controllen = sizeof (control),
		};

		ssize_t size = recvmsg (self->uevent_sock, &msghdr, MSG_DONTWAIT);
		if (size < 0) {
			if (errno == EAGAIN)
				return;

			continue;
		}

		struct cmsghdr *cmsg;
		struct ucred *ucred = NULL;
		CMSG_FOREACH (cmsg, &msghdr) {
			if (cmsg->cmsg_level == SOL_SOCKET &&
			    cmsg->cmsg_type == SCM_CREDENTIALS &&
			    cmsg->cmsg_len == CMSG_LEN (sizeof (struct ucred)))
				ucred = (struct ucred*) CMSG_DATA (cmsg);
		}

		if (!ucred || ucred->pid != 0 || addr.nl_pid != 0)
			continue;

		//uevent_check (msg, size);
	}
}

static void *uevent_watching_thread (void *_self)
{
	struct task_suspend_t *self = (struct task_suspend_t *)_self;
	struct pollfd pfd;
	pfd.events = POLLIN;
	pfd.fd = self->uevent_sock;

	while (!self->shutdown) {
		// wait until a new uevent comes
		pfd.revents = 0;
		int rc = poll (&pfd, 1, -1);

		if ((rc > 0) && (pfd.revents & POLLIN))
			uevent_handle (self);
	}

	return NULL;
}

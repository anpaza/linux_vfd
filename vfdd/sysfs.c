#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "vfdd.h"

char *sysfs_get_str (const char *device, const char *attr, const char *defval)
{
	int h;
	off_t fsize;
	char tmp [1000];
	snprintf (tmp, sizeof (tmp), "%s/%s", device, attr);

	h = open (tmp, O_RDONLY);
	if (h < 0)
		goto error;

	fsize = lseek (h, 0, SEEK_END);
	if (fsize < 0)
		goto error;

	if (fsize >= sizeof (tmp))
		fsize = sizeof (tmp) - 1;
	lseek (h, 0, SEEK_SET);
	if (read (h, tmp, fsize) != fsize)
		goto error;
	tmp [fsize] = 0;

	close (h);
	return strdup (tmp);

error:
	trace ("failed to read sysfs attr %s from dev %s\n", attr, device);

	if (h >= 0)
		close (h);
	return strdup (defval);
}

int sysfs_get_int (const char *device, const char *attr, int defval)
{
	char *val = sysfs_get_str (device, attr, NULL);
	if (val) {
		defval = strtol (val, NULL, 0);
		free (val);
	}

	return defval;
}

int sysfs_set_str (const char *device, const char *attr, const char *value)
{
	int h, n;
	char tmp [1000];
	snprintf (tmp, sizeof (tmp), "%s/%s", device, attr);

	h = open (tmp, O_WRONLY);
	if (h < 0)
		goto error;

	n = strlen (value);
	if (write (h, value, n) != n)
		goto error;

	close (h);
	return 0;

error:
	trace ("failed to write [%s] into sysfs attr %s from dev %s\n",
		value, attr, device);

	if (h >= 0)
		close (h);
	return -1;
}

int sysfs_set_int (const char *device, const char *attr, int value)
{
	char tmp [11];
	snprintf (tmp, sizeof (tmp), "%d", value);
	return sysfs_set_str (device, attr, tmp);
}

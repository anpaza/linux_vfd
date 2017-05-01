#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "vfdd.h"

char *sysfs_read (const char *device_attr)
{
	int h, n;
	off_t fsize;
	char tmp [1000];

	h = open (device_attr, O_RDONLY);
	if (h < 0)
		goto error;

	fsize = lseek (h, 0, SEEK_END);
	if (fsize < 0)
		goto error;

	if (fsize >= sizeof (tmp))
		fsize = sizeof (tmp) - 1;
	lseek (h, 0, SEEK_SET);
	n = read (h, tmp, fsize);
	if (n <= 0)
		goto error;
	tmp [n] = 0;

	close (h);
	return strdup (tmp);

error:
	trace ("failed to read sysfs attr from %s\n", device_attr);

	if (h >= 0)
		close (h);
	return NULL;
}

char *sysfs_get_str (const char *device, const char *attr)
{
	char tmp [200];
	snprintf (tmp, sizeof (tmp), "%s/%s", device, attr);

	return sysfs_read (tmp);
}

int sysfs_get_int (const char *device, const char *attr)
{
	int val;
	char *vals = sysfs_get_str (device, attr);
	if (!vals)
		return -1;

	val = strtol (vals, NULL, 0);
	free (vals);

	return val;
}

int sysfs_write (const char *device_attr, const char *value)
{
	int h, n;

	h = open (device_attr, O_TRUNC | O_WRONLY);
	if (h < 0)
		goto error;

	n = strlen (value);
	if (write (h, value, n) != n)
		goto error;

	close (h);
	return 0;

error:
	trace ("failed to write attr [%s] into %s\n", value, device_attr);

	if (h >= 0)
		close (h);
	return -1;
}

int sysfs_set_str (const char *device, const char *attr, const char *value)
{
	char tmp [200];
	snprintf (tmp, sizeof (tmp), "%s/%s", device, attr);
	return sysfs_write (tmp, value);
}

int sysfs_set_int (const char *device, const char *attr, int value)
{
	char tmp [11];
	snprintf (tmp, sizeof (tmp), "%d", value);
	return sysfs_set_str (device, attr, tmp);
}

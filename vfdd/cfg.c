#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "vfdd.h"

static char *cfg_make_key (const char *instance, const char *key)
{
	int n;
	char *ret;

	if (instance == NULL)
		return strdup (key);

	n = strlen (instance) + 1 + strlen (key) + 1;
	ret = malloc (n);
	snprintf (ret, n, "%s.%s", instance, key);
	return ret;
}

const char *cfg_get_str (const char *instance, const char *key, const char *defval)
{
	char *ikey = cfg_make_key (instance, key);
	const char *ret = cfg_get (g_cfg, ikey);
	free (ikey);

	if (!ret)
		return defval;
	return ret;
}

int cfg_get_int (const char *instance, const char *key, int defval)
{
	char *ikey = cfg_make_key (instance, key);
	const char *ret = cfg_get (g_cfg, ikey);
	free (ikey);

	if (!ret)
		return defval;

	return atoi (ret);
}

int cfg_get_int_2 (const char *instance, const char *key, int *out)
{
	char *ikey = cfg_make_key (instance, key);
	const char *ret = cfg_get (g_cfg, ikey);
	free (ikey);

	if (!ret)
		return -ENOKEY;

	if (sscanf (ret, "%d %d", out, out + 1) != 2)
		return -EBADMSG;

	return 0;
}

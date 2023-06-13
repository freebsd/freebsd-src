/*-
 * Copyright (c) 2023 Klara, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/limits.h>

#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <pthread_np.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include <nsswitch.h>
#include <pwd.h>
#include <taclib.h>

extern int __isthreaded;

#define	DEF_UID		65534
#define	DEF_GID		65534
#define	DEF_CLASS	""
#define	DEF_DIR		"/"
#define	DEF_SHELL	"/bin/sh"

ns_mtab *nss_module_register(const char *, unsigned int *,
    nss_module_unregister_fn *);

static void
tacplus_error(struct tac_handle *h, const char *func)
{
	if (h == NULL)
		syslog(LOG_ERR, "%s(): %m", func);
	else
		syslog(LOG_ERR, "%s(): %s", func, tac_strerror(h));
}

static pthread_key_t tacplus_key;

static void
tacplus_fini(void *p)
{
	struct tac_handle **h = p;

	tac_close(*h);
	free(h);
}

static void
tacplus_keyinit(void)
{
	(void)pthread_key_create(&tacplus_key, tacplus_fini);
}

static struct tac_handle *
tacplus_get_handle(void)
{
	static pthread_once_t keyinit = PTHREAD_ONCE_INIT;
	static struct tac_handle *sth;
	struct tac_handle **h = &sth;
	int ret;

	if (__isthreaded && !pthread_main_np()) {
		if ((ret = pthread_once(&keyinit, tacplus_keyinit)) != 0)
			return (NULL);
		if ((h = pthread_getspecific(tacplus_key)) == NULL) {
			if ((h = calloc(1, sizeof(*h))) == NULL)
				return (NULL);
			if ((pthread_setspecific(tacplus_key, h)) != 0) {
				free(h);
				return (NULL);
			}
		}
	}
	if (*h == NULL) {
		if ((*h = tac_open()) == NULL) {
			tacplus_error(*h, "tac_open");
			return (NULL);
		}
		if (tac_config(*h, NULL) != 0) {
			tacplus_error(*h, "tac_config");
			tac_close(*h);
			*h = NULL;
			return (NULL);
		}
	}
	return (*h);
}

static char *
tacplus_copystr(const char *str, char **buffer, size_t *bufsize)
{
	char *copy = *buffer;
	size_t len = strlen(str) + 1;

	if (len > *bufsize) {
		errno = ERANGE;
		return (NULL);
	}
	memcpy(copy, str, len);
	*buffer += len;
	*bufsize -= len;
	return (copy);
}

static int
tacplus_getpwnam_r(const char *name, struct passwd *pwd, char *buffer,
    size_t bufsize)
{
	struct tac_handle *h;
	char *av, *key, *value, *end;
	intmax_t num;
	int i, ret;

	if ((h = tacplus_get_handle()) == NULL)
		return (NS_UNAVAIL);
	ret = tac_create_author(h, TAC_AUTHEN_METH_NOT_SET,
	    TAC_AUTHEN_TYPE_NOT_SET, TAC_AUTHEN_SVC_LOGIN);
	if (ret < 0) {
		tacplus_error(h, "tac_create_author");
		return (NS_TRYAGAIN);
	}
	if (tac_set_user(h, name) < 0) {
		tacplus_error(h, "tac_set_user");
		return (NS_TRYAGAIN);
	}
	if (tac_set_av(h, 0, "service=shell") < 0) {
		tacplus_error(h, "tac_set_av");
		return (NS_TRYAGAIN);
	}
	ret = tac_send_author(h);
	switch (TAC_AUTHOR_STATUS(ret)) {
	case TAC_AUTHOR_STATUS_PASS_ADD:
	case TAC_AUTHOR_STATUS_PASS_REPL:
		/* found */
		break;
	case TAC_AUTHOR_STATUS_FAIL:
		return (NS_NOTFOUND);
	case TAC_AUTHOR_STATUS_ERROR:
		return (NS_UNAVAIL);
	default:
		tacplus_error(h, "tac_send_author");
		return (NS_UNAVAIL);
	}
	memset(pwd, 0, sizeof(*pwd));

	/* copy name */
	pwd->pw_name = tacplus_copystr(name, &buffer, &bufsize);
	if (pwd->pw_name == NULL)
		return (NS_RETURN);

	/* no password */
	pwd->pw_passwd = tacplus_copystr("*", &buffer, &bufsize);
	if (2 > bufsize)
		return (NS_RETURN);

	/* default uid and gid */
	pwd->pw_uid = DEF_UID;
	pwd->pw_gid = DEF_GID;

	/* get attribute-value pairs from TACACS+ response */
	for (i = 0; i < TAC_AUTHEN_AV_COUNT(ret); i++) {
		if ((av = tac_get_av(h, i)) == NULL) {
			tacplus_error(h, "tac_get_av");
			return (NS_UNAVAIL);
		}
		key = av;
		if ((value = strchr(av, '=')) == NULL) {
			free(av);
			return (NS_RETURN);
		}
		*value++ = '\0';
		if (strcasecmp(key, "uid") == 0) {
			num = strtoimax(value, &end, 10);
			if (end == value || *end != '\0' ||
			    num < 0 || num > (intmax_t)UID_MAX) {
				errno = EINVAL;
				free(av);
				return (NS_RETURN);
			}
			pwd->pw_uid = num;
		} else if (strcasecmp(key, "gid") == 0) {
			num = strtoimax(value, &end, 10);
			if (end == value || *end != '\0' ||
			    num < 0 || num > (intmax_t)GID_MAX) {
				errno = EINVAL;
				free(av);
				return (NS_RETURN);
			}
			pwd->pw_gid = num;
		} else if (strcasecmp(av, "class") == 0) {
			pwd->pw_class = tacplus_copystr(value, &buffer,
			    &bufsize);
			if (pwd->pw_class == NULL) {
				free(av);
				return (NS_RETURN);
			}
		} else if (strcasecmp(av, "gecos") == 0) {
			pwd->pw_gecos = tacplus_copystr(value, &buffer,
			    &bufsize);
			if (pwd->pw_gecos == NULL) {
				free(av);
				return (NS_RETURN);
			}
		} else if (strcasecmp(av, "home") == 0) {
			pwd->pw_dir = tacplus_copystr(value, &buffer,
			    &bufsize);
			if (pwd->pw_dir == NULL) {
				free(av);
				return (NS_RETURN);
			}
		} else if (strcasecmp(av, "shell") == 0) {
			pwd->pw_shell = tacplus_copystr(value, &buffer,
			    &bufsize);
			if (pwd->pw_shell == NULL) {
				free(av);
				return (NS_RETURN);
			}
		}
		free(av);
	}

	/* default class if none was provided */
	if (pwd->pw_class == NULL)
		pwd->pw_class = tacplus_copystr(DEF_CLASS, &buffer, &bufsize);

	/* gecos equal to name if none was provided */
	if (pwd->pw_gecos == NULL)
		pwd->pw_gecos = pwd->pw_name;

	/* default home directory if none was provided */
	if (pwd->pw_dir == NULL)
		pwd->pw_dir = tacplus_copystr(DEF_DIR, &buffer, &bufsize);
	if (pwd->pw_dir == NULL)
		return (NS_RETURN);

	/* default shell if none was provided */
	if (pwd->pw_shell == NULL)
		pwd->pw_shell = tacplus_copystr(DEF_SHELL, &buffer, &bufsize);
	if (pwd->pw_shell == NULL)
		return (NS_RETURN);

	/* done! */
	return (NS_SUCCESS);
}

static int
nss_tacplus_getpwnam_r(void *retval, void *mdata __unused, va_list ap)
{
	char *name = va_arg(ap, char *);
	struct passwd *pwd = va_arg(ap, struct passwd *);
	char *buffer = va_arg(ap, char *);
	size_t bufsize = va_arg(ap, size_t);
	int *result = va_arg(ap, int *);
	int ret;

	errno = 0;
	ret = tacplus_getpwnam_r(name, pwd, buffer, bufsize);
	if (ret == NS_SUCCESS) {
		*(void **)retval = pwd;
		*result = 0;
	} else {
		*(void **)retval = NULL;
		*result = errno;
	}
	return (ret);
}

ns_mtab *
nss_module_register(const char *name __unused, unsigned int *plen,
    nss_module_unregister_fn *unreg)
{
	static ns_mtab mtab[] = {
		{ "passwd", "getpwnam_r", &nss_tacplus_getpwnam_r, NULL },
	};

	*plen = nitems(mtab);
	*unreg = NULL;
	return (mtab);
}

/*-
 * Copyright (c) 1998 Michael Smith
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * The unified bootloader passes us a pointer to a preserved copy of
 * bootstrap/kernel environment variables.  We convert them to a
 * dynamic array of strings later when the VM subsystem is up.
 *
 * We make these available through the kenv(2) syscall for userland
 * and through getenv()/freeenv() setenv() unsetenv() testenv() for
 * the kernel.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/libkern.h>
#include <sys/kenv.h>

#include <security/mac/mac_framework.h>

static MALLOC_DEFINE(M_KENV, "kenv", "kernel environment");

#define KENV_SIZE	512	/* Maximum number of environment strings */

/* pointer to the static environment */
char		*kern_envp;
static int	env_len;
static int	env_pos;
static char	*kernenv_next(char *);

/* dynamic environment variables */
char		**kenvp;
struct mtx	kenv_lock;

/*
 * No need to protect this with a mutex since SYSINITS are single threaded.
 */
int	dynamic_kenv = 0;

#define KENV_CHECK	if (!dynamic_kenv) \
			    panic("%s: called before SI_SUB_KMEM", __func__)

int
kenv(td, uap)
	struct thread *td;
	struct kenv_args /* {
		int what;
		const char *name;
		char *value;
		int len;
	} */ *uap;
{
	char *name, *value, *buffer = NULL;
	size_t len, done, needed, buflen;
	int error, i;

	KASSERT(dynamic_kenv, ("kenv: dynamic_kenv = 0"));

	error = 0;
	if (uap->what == KENV_DUMP) {
#ifdef MAC
		error = mac_kenv_check_dump(td->td_ucred);
		if (error)
			return (error);
#endif
		done = needed = 0;
		buflen = uap->len;
		if (buflen > KENV_SIZE * (KENV_MNAMELEN + KENV_MVALLEN + 2))
			buflen = KENV_SIZE * (KENV_MNAMELEN +
			    KENV_MVALLEN + 2);
		if (uap->len > 0 && uap->value != NULL)
			buffer = malloc(buflen, M_TEMP, M_WAITOK|M_ZERO);
		mtx_lock(&kenv_lock);
		for (i = 0; kenvp[i] != NULL; i++) {
			len = strlen(kenvp[i]) + 1;
			needed += len;
			len = min(len, buflen - done);
			/*
			 * If called with a NULL or insufficiently large
			 * buffer, just keep computing the required size.
			 */
			if (uap->value != NULL && buffer != NULL && len > 0) {
				bcopy(kenvp[i], buffer + done, len);
				done += len;
			}
		}
		mtx_unlock(&kenv_lock);
		if (buffer != NULL) {
			error = copyout(buffer, uap->value, done);
			free(buffer, M_TEMP);
		}
		td->td_retval[0] = ((done == needed) ? 0 : needed);
		return (error);
	}

	switch (uap->what) {
	case KENV_SET:
		error = priv_check(td, PRIV_KENV_SET);
		if (error)
			return (error);
		break;

	case KENV_UNSET:
		error = priv_check(td, PRIV_KENV_UNSET);
		if (error)
			return (error);
		break;
	}

	name = malloc(KENV_MNAMELEN, M_TEMP, M_WAITOK);

	error = copyinstr(uap->name, name, KENV_MNAMELEN, NULL);
	if (error)
		goto done;

	switch (uap->what) {
	case KENV_GET:
#ifdef MAC
		error = mac_kenv_check_get(td->td_ucred, name);
		if (error)
			goto done;
#endif
		value = getenv(name);
		if (value == NULL) {
			error = ENOENT;
			goto done;
		}
		len = strlen(value) + 1;
		if (len > uap->len)
			len = uap->len;
		error = copyout(value, uap->value, len);
		freeenv(value);
		if (error)
			goto done;
		td->td_retval[0] = len;
		break;
	case KENV_SET:
		len = uap->len;
		if (len < 1) {
			error = EINVAL;
			goto done;
		}
		if (len > KENV_MVALLEN)
			len = KENV_MVALLEN;
		value = malloc(len, M_TEMP, M_WAITOK);
		error = copyinstr(uap->value, value, len, NULL);
		if (error) {
			free(value, M_TEMP);
			goto done;
		}
#ifdef MAC
		error = mac_kenv_check_set(td->td_ucred, name, value);
		if (error == 0)
#endif
			setenv(name, value);
		free(value, M_TEMP);
		break;
	case KENV_UNSET:
#ifdef MAC
		error = mac_kenv_check_unset(td->td_ucred, name);
		if (error)
			goto done;
#endif
		error = unsetenv(name);
		if (error)
			error = ENOENT;
		break;
	default:
		error = EINVAL;
		break;
	}
done:
	free(name, M_TEMP);
	return (error);
}

void
init_static_kenv(char *buf, size_t len)
{
	kern_envp = buf;
	env_len = len;
	env_pos = 0;
}

/*
 * Setup the dynamic kernel environment.
 */
static void
init_dynamic_kenv(void *data __unused)
{
	char *cp;
	int len, i;

	kenvp = malloc((KENV_SIZE + 1) * sizeof(char *), M_KENV,
		M_WAITOK | M_ZERO);
	i = 0;
	for (cp = kern_envp; cp != NULL; cp = kernenv_next(cp)) {
		len = strlen(cp) + 1;
		if (i < KENV_SIZE) {
			kenvp[i] = malloc(len, M_KENV, M_WAITOK);
			strcpy(kenvp[i++], cp);
		} else
			printf(
			    "WARNING: too many kenv strings, ignoring %s\n",
			    cp);
	}
	kenvp[i] = NULL;

	mtx_init(&kenv_lock, "kernel environment", NULL, MTX_DEF);
	dynamic_kenv = 1;
}
SYSINIT(kenv, SI_SUB_KMEM, SI_ORDER_ANY, init_dynamic_kenv, NULL);

void
freeenv(char *env)
{

	if (dynamic_kenv)
		free(env, M_KENV);
}

/*
 * Internal functions for string lookup.
 */
static char *
_getenv_dynamic(const char *name, int *idx)
{
	char *cp;
	int len, i;

	mtx_assert(&kenv_lock, MA_OWNED);
	len = strlen(name);
	for (cp = kenvp[0], i = 0; cp != NULL; cp = kenvp[++i]) {
		if ((strncmp(cp, name, len) == 0) &&
		    (cp[len] == '=')) {
			if (idx != NULL)
				*idx = i;
			return (cp + len + 1);
		}
	}
	return (NULL);
}

static char *
_getenv_static(const char *name)
{
	char *cp, *ep;
	int len;

	for (cp = kern_envp; cp != NULL; cp = kernenv_next(cp)) {
		for (ep = cp; (*ep != '=') && (*ep != 0); ep++)
			;
		if (*ep != '=')
			continue;
		len = ep - cp;
		ep++;
		if (!strncmp(name, cp, len) && name[len] == 0)
			return (ep);
	}
	return (NULL);
}

/*
 * Look up an environment variable by name.
 * Return a pointer to the string if found.
 * The pointer has to be freed with freeenv()
 * after use.
 */
char *
getenv(const char *name)
{
	char buf[KENV_MNAMELEN + 1 + KENV_MVALLEN + 1];
	char *ret, *cp;
	int len;

	if (dynamic_kenv) {
		mtx_lock(&kenv_lock);
		cp = _getenv_dynamic(name, NULL);
		if (cp != NULL) {
			strcpy(buf, cp);
			mtx_unlock(&kenv_lock);
			len = strlen(buf) + 1;
			ret = malloc(len, M_KENV, M_WAITOK);
			strcpy(ret, buf);
		} else {
			mtx_unlock(&kenv_lock);
			ret = NULL;
		}
	} else
		ret = _getenv_static(name);
	return (ret);
}

/*
 * Test if an environment variable is defined.
 */
int
testenv(const char *name)
{
	char *cp;

	if (dynamic_kenv) {
		mtx_lock(&kenv_lock);
		cp = _getenv_dynamic(name, NULL);
		mtx_unlock(&kenv_lock);
	} else
		cp = _getenv_static(name);
	if (cp != NULL)
		return (1);
	return (0);
}

static int
setenv_static(const char *name, const char *value)
{
	int len;

	if (env_pos >= env_len)
		return (-1);

	/* Check space for x=y and two nuls */
	len = strlen(name) + strlen(value);
	if (len + 3 < env_len - env_pos) {
		len = sprintf(&kern_envp[env_pos], "%s=%s", name, value);
		env_pos += len+1;
		kern_envp[env_pos] = '\0';
		return (0);
	} else
		return (-1);

}

/*
 * Set an environment variable by name.
 */
int
setenv(const char *name, const char *value)
{
	char *buf, *cp, *oldenv;
	int namelen, vallen, i;

	if (dynamic_kenv == 0 && env_len > 0)
		return (setenv_static(name, value));

	KENV_CHECK;

	namelen = strlen(name) + 1;
	if (namelen > KENV_MNAMELEN)
		return (-1);
	vallen = strlen(value) + 1;
	if (vallen > KENV_MVALLEN)
		return (-1);
	buf = malloc(namelen + vallen, M_KENV, M_WAITOK);
	sprintf(buf, "%s=%s", name, value);

	mtx_lock(&kenv_lock);
	cp = _getenv_dynamic(name, &i);
	if (cp != NULL) {
		oldenv = kenvp[i];
		kenvp[i] = buf;
		mtx_unlock(&kenv_lock);
		free(oldenv, M_KENV);
	} else {
		/* We add the option if it wasn't found */
		for (i = 0; (cp = kenvp[i]) != NULL; i++)
			;

		/* Bounds checking */
		if (i < 0 || i >= KENV_SIZE) {
			free(buf, M_KENV);
			mtx_unlock(&kenv_lock);
			return (-1);
		}

		kenvp[i] = buf;
		kenvp[i + 1] = NULL;
		mtx_unlock(&kenv_lock);
	}
	return (0);
}

/*
 * Unset an environment variable string.
 */
int
unsetenv(const char *name)
{
	char *cp, *oldenv;
	int i, j;

	KENV_CHECK;

	mtx_lock(&kenv_lock);
	cp = _getenv_dynamic(name, &i);
	if (cp != NULL) {
		oldenv = kenvp[i];
		for (j = i + 1; kenvp[j] != NULL; j++)
			kenvp[i++] = kenvp[j];
		kenvp[i] = NULL;
		mtx_unlock(&kenv_lock);
		free(oldenv, M_KENV);
		return (0);
	}
	mtx_unlock(&kenv_lock);
	return (-1);
}

/*
 * Return a string value from an environment variable.
 */
int
getenv_string(const char *name, char *data, int size)
{
	char *tmp;

	tmp = getenv(name);
	if (tmp != NULL) {
		strlcpy(data, tmp, size);
		freeenv(tmp);
		return (1);
	} else
		return (0);
}

/*
 * Return an integer value from an environment variable.
 */
int
getenv_int(const char *name, int *data)
{
	quad_t tmp;
	int rval;

	rval = getenv_quad(name, &tmp);
	if (rval)
		*data = (int) tmp;
	return (rval);
}

/*
 * Return an unsigned integer value from an environment variable.
 */
int
getenv_uint(const char *name, unsigned int *data)
{
	quad_t tmp;
	int rval;

	rval = getenv_quad(name, &tmp);
	if (rval)
		*data = (unsigned int) tmp;
	return (rval);
}

/*
 * Return a long value from an environment variable.
 */
int
getenv_long(const char *name, long *data)
{
	quad_t tmp;
	int rval;

	rval = getenv_quad(name, &tmp);
	if (rval)
		*data = (long) tmp;
	return (rval);
}

/*
 * Return an unsigned long value from an environment variable.
 */
int
getenv_ulong(const char *name, unsigned long *data)
{
	quad_t tmp;
	int rval;

	rval = getenv_quad(name, &tmp);
	if (rval)
		*data = (unsigned long) tmp;
	return (rval);
}

/*
 * Return a quad_t value from an environment variable.
 */
int
getenv_quad(const char *name, quad_t *data)
{
	char	*value;
	char	*vtp;
	quad_t	iv;

	value = getenv(name);
	if (value == NULL)
		return (0);
	iv = strtoq(value, &vtp, 0);
	if (vtp == value || (vtp[0] != '\0' && vtp[1] != '\0')) {
		freeenv(value);
		return (0);
	}
	switch (vtp[0]) {
	case 't': case 'T':
		iv *= 1024;
	case 'g': case 'G':
		iv *= 1024;
	case 'm': case 'M':
		iv *= 1024;
	case 'k': case 'K':
		iv *= 1024;
	case '\0':
		break;
	default:
		freeenv(value);
		return (0);
	}
	*data = iv;
	freeenv(value);
	return (1);
}

/*
 * Find the next entry after the one which (cp) falls within, return a
 * pointer to its start or NULL if there are no more.
 */
static char *
kernenv_next(char *cp)
{

	if (cp != NULL) {
		while (*cp != 0)
			cp++;
		cp++;
		if (*cp == 0)
			cp = NULL;
	}
	return (cp);
}

void
tunable_int_init(void *data)
{
	struct tunable_int *d = (struct tunable_int *)data;

	TUNABLE_INT_FETCH(d->path, d->var);
}

void
tunable_long_init(void *data)
{
	struct tunable_long *d = (struct tunable_long *)data;

	TUNABLE_LONG_FETCH(d->path, d->var);
}

void
tunable_ulong_init(void *data)
{
	struct tunable_ulong *d = (struct tunable_ulong *)data;

	TUNABLE_ULONG_FETCH(d->path, d->var);
}

void
tunable_quad_init(void *data)
{
	struct tunable_quad *d = (struct tunable_quad *)data;

	TUNABLE_QUAD_FETCH(d->path, d->var);
}

void
tunable_str_init(void *data)
{
	struct tunable_str *d = (struct tunable_str *)data;

	TUNABLE_STR_FETCH(d->path, d->var, d->size);
}

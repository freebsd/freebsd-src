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
 *
 * $FreeBSD$
 */

/*
 * The unified bootloader passes us a pointer to a preserved copy of
 * bootstrap/kernel environment variables.
 * We make these available using sysctl for both in-kernel and
 * out-of-kernel consumers.
 *
 * Note that the current sysctl infrastructure doesn't allow 
 * dynamic insertion or traversal through handled spaces.  Grr.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/libkern.h>

char	*kern_envp;

static char	*kernenv_next(char *cp);

/*
 * Look up an environment variable by name.
 */
char *
getenv(const char *name)
{
    char	*cp, *ep;
    int		len;
    
    for (cp = kern_envp; cp != NULL; cp = kernenv_next(cp)) {
	for (ep = cp; (*ep != '=') && (*ep != 0); ep++)
	    ;
	len = ep - cp;
	if (*ep == '=')
	    ep++;
	if (!strncmp(name, cp, len))
	    return(ep);
    }
    return(NULL);
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
	strncpy(data, tmp, size);
	data[size - 1] = 0;
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
    if (rval) {
	*data = (int) tmp;
    }
    return (rval);
}

/*
 * Return a quad_t value from an environment variable.
 */
int
getenv_quad(const char *name, quad_t *data)
{
    const char	*value;
    char	*vtp;
    quad_t	iv;
    
    if ((value = getenv(name)) == NULL)
	return(0);
    
    iv = strtoq(value, &vtp, 0);
    if ((vtp == value) || (*vtp != '\0'))
	return(0);
    
    *data = iv;
    return(1);
}

/*
 * Export for userland.  See kenv(1) specifically.
 */
static int
sysctl_kernenv(SYSCTL_HANDLER_ARGS)
{
    int		*name = (int *)arg1;
    u_int	namelen = arg2;
    char	*cp;
    int		i, error;

    if (kern_envp == NULL)
	return(ENOENT);
    
    name++;
    namelen--;
    
    if (namelen != 1)
	return(EINVAL);

    cp = kern_envp;
    for (i = 0; i < name[0]; i++) {
	cp = kernenv_next(cp);
	if (cp == NULL)
	    break;
    }
    
    if (cp == NULL)
	return(ENOENT);
    
    error = SYSCTL_OUT(req, cp, strlen(cp) + 1);
    return (error);
}

SYSCTL_NODE(_kern, OID_AUTO, environment, CTLFLAG_RD, sysctl_kernenv, "kernel environment space");

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
    return(cp);
}

void
tunable_int_init(void *data)
{
	struct tunable_int *d = (struct tunable_int *)data;

	TUNABLE_INT_FETCH(d->path, d->var);
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

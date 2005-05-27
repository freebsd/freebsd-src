/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stand.h>
#include <string.h>

#include "bootstrap.h"
/*
 * Core console support
 */

static int	cons_set(struct env_var *ev, int flags, const void *value);
static int	cons_find(const char *name);
static int	cons_check(const char *string);
static void	cons_change(const char *string);

/*
 * Detect possible console(s) to use.  If preferred console(s) have been
 * specified, mark them as active. Else, mark the first probed console
 * as active.  Also create the console variable.
 */
void
cons_probe(void) 
{
    int			cons;
    int			active;
    char		*prefconsole;
    
    /* Do all console probes */
    for (cons = 0; consoles[cons] != NULL; cons++) {
	consoles[cons]->c_flags = 0;
 	consoles[cons]->c_probe(consoles[cons]);
    }
    /* Now find the first working one */
    active = -1;
    for (cons = 0; consoles[cons] != NULL && active == -1; cons++) {
	consoles[cons]->c_flags = 0;
 	consoles[cons]->c_probe(consoles[cons]);
	if (consoles[cons]->c_flags == (C_PRESENTIN | C_PRESENTOUT))
	    active = cons;
    }
    /* Force a console even if all probes failed */
    if (active == -1)
	active = 0;

    /* Check to see if a console preference has already been registered */
    prefconsole = getenv("console");
    if (prefconsole != NULL)
	prefconsole = strdup(prefconsole);
    if (prefconsole != NULL) {
	unsetenv("console");		/* we want to replace this */
	cons_change(prefconsole);
    } else {
	consoles[active]->c_flags |= C_ACTIVEIN | C_ACTIVEOUT;
	consoles[active]->c_init(0);
	prefconsole = strdup(consoles[active]->c_name);
    }

    printf("Consoles: ");
    for (cons = 0; consoles[cons] != NULL; cons++)
	if (consoles[cons]->c_flags & (C_ACTIVEIN | C_ACTIVEOUT))
	    printf("%s  ", consoles[cons]->c_desc);
    printf("\n");

    if (prefconsole != NULL) {
	env_setenv("console", EV_VOLATILE, prefconsole, cons_set,
	    env_nounset);
	free(prefconsole);
    }
}

int
getchar(void)
{
    int		cons;
    int		rv;
    
    /* Loop forever polling all active consoles */
    for(;;)
	for (cons = 0; consoles[cons] != NULL; cons++)
	    if ((consoles[cons]->c_flags & C_ACTIVEIN) && 
		((rv = consoles[cons]->c_in()) != -1))
		return(rv);
}

int
ischar(void)
{
    int		cons;

    for (cons = 0; consoles[cons] != NULL; cons++)
	if ((consoles[cons]->c_flags & C_ACTIVEIN) && 
	    (consoles[cons]->c_ready() != 0))
		return(1);
    return(0);
}

void
putchar(int c)
{
    int		cons;
    
    /* Expand newlines */
    if (c == '\n')
	putchar('\r');
    
    for (cons = 0; consoles[cons] != NULL; cons++)
	if (consoles[cons]->c_flags & C_ACTIVEOUT)
	    consoles[cons]->c_out(c);
}

/*
 * Find the console with the specified name.
 */
static int
cons_find(const char *name)
{
    int		cons;

    for (cons = 0; consoles[cons] != NULL; cons++)
	if (!strcmp(consoles[cons]->c_name, name))
	    return (cons);
    return (-1);
}

/*
 * Select one or more consoles.
 */
static int
cons_set(struct env_var *ev, int flags, const void *value)
{
    int		cons;

    if ((value == NULL) || (cons_check(value) == -1)) {
	if (value != NULL) 
	    printf("no such console!\n");
	printf("Available consoles:\n");
	for (cons = 0; consoles[cons] != NULL; cons++)
	    printf("    %s\n", consoles[cons]->c_name);
	return(CMD_ERROR);
    }

    cons_change(value);

    env_setenv(ev->ev_name, flags | EV_NOHOOK, value, NULL, NULL);
    return(CMD_OK);
}

/*
 * Check that all of the consoles listed in *string are valid consoles
 */
static int
cons_check(const char *string)
{
    int		cons;
    char	*curpos, *dup, *next;

    dup = next = strdup(string);
    cons = -1;
    while (next != NULL) {
	curpos = strsep(&next, " ,");
	if (*curpos != '\0') {
	    cons = cons_find(curpos);
	    if (cons == -1)
		break;
	}
    }

    free(dup);
    return (cons);
}

/*
 * Activate all of the consoles listed in *string and disable all the others.
 */
static void
cons_change(const char *string)
{
    int		cons;
    char	*curpos, *dup, *next;

    /* Disable all consoles */
    for (cons = 0; consoles[cons] != NULL; cons++) {
	consoles[cons]->c_flags &= ~(C_ACTIVEIN | C_ACTIVEOUT);
    }

    /* Enable selected consoles */
    dup = next = strdup(string);
    while (next != NULL) {
	curpos = strsep(&next, " ,");
	if (*curpos == '\0')
		continue;
	cons = cons_find(curpos);
	if (cons > 0) {
	    consoles[cons]->c_flags |= C_ACTIVEIN | C_ACTIVEOUT;
	    consoles[cons]->c_init(0);
	}
    }

    free(dup);
}

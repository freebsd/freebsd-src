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
 *
 * $FreeBSD$
 */

#include <stand.h>
#include <string.h>

#include "bootstrap.h"
/*
 * Core console support
 */

static int	cons_set(struct env_var *ev, int flags, void *value);
static int	cons_find(char *name);

/*
 * Detect possible console(s) to use.  The first probed console
 * is marked active.  Also create the console variable.
 *	
 * XXX Add logic for multiple console support.
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

    /* Check to see if a console preference has already been registered */
    prefconsole = getenv("console");
    if (prefconsole != NULL)
	prefconsole = strdup(prefconsole);
    if (prefconsole != NULL) {
	unsetenv("console");		/* we want to replace this */
	for (cons = 0; consoles[cons] != NULL; cons++)
	    /* look for the nominated console, use it if it's functional */
	    if (!strcmp(prefconsole, consoles[cons]->c_name) &&
		(consoles[cons]->c_flags == (C_PRESENTIN | C_PRESENTOUT)))
		active = cons;
	free(prefconsole);
    }
    if (active == -1)
	active = 0;
    consoles[active]->c_flags |= (C_ACTIVEIN | C_ACTIVEOUT);
    consoles[active]->c_init(0);

    printf("Console: %s\n", consoles[active]->c_desc);
    env_setenv("console", EV_VOLATILE, consoles[active]->c_name, cons_set,
	env_nounset);
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

static int
cons_find(char *name)
{
    int		cons;
    
    for (cons = 0; consoles[cons] != NULL; cons++)
	if (!strcmp(consoles[cons]->c_name, name))
	    return(cons);
    return(-1);
}
    

/*
 * Select a console.
 *
 * XXX Note that the console system design allows for some extension
 *     here (eg. multiple consoles, input/output only, etc.)
 */
static int
cons_set(struct env_var *ev, int flags, void *value)
{
    int		cons, active;

    if ((value == NULL) || ((active = cons_find(value)) == -1)) {
	if (value != NULL) 
	    printf("no such console '%s'\n", (char *)value);
	printf("Available consoles:\n");
	for (cons = 0; consoles[cons] != NULL; cons++)
	    printf("    %s\n", consoles[cons]->c_name);
	return(CMD_ERROR);
    }

    /* disable all current consoles */
    for (cons = 0; consoles[cons] != NULL; cons++)
	consoles[cons]->c_flags &= ~(C_ACTIVEIN | C_ACTIVEOUT);
    
    /* enable selected console */
    consoles[active]->c_flags |= C_ACTIVEIN | C_ACTIVEOUT;
    consoles[active]->c_init(0);

    env_setenv(ev->ev_name, flags | EV_NOHOOK, value, NULL, NULL);
    return(CMD_OK);
}

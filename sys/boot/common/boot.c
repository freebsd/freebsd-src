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
 *	$Id: boot.c,v 1.11 1999/05/28 08:01:52 brian Exp $
 */

/*
 * Loading modules, booting the system
 */

#include <stand.h>
#include <string.h>

#include "bootstrap.h"

static char	*getbootfile(int try);

/* List of kernel names to try (may be overwritten by boot.config) XXX should move from here? */
static char *default_bootfiles = "kernel;kernel.old";

static int autoboot_tried;

/*
 * The user wants us to boot.
 */
COMMAND_SET(boot, "boot", "boot a file or loaded kernel", command_boot);

static int
command_boot(int argc, char *argv[])
{
    struct loaded_module	*km;
    char			*cp;
    int				try;
    
    /*
     * See if the user has specified an explicit kernel to boot.
     */
    if ((argc > 1) && (argv[1][0] != '-')) {
	
	/* XXX maybe we should discard everything and start again? */
	if (mod_findmodule(NULL, NULL) != NULL) {
	    sprintf(command_errbuf, "can't boot '%s', kernel module already loaded", argv[1]);
	    return(CMD_ERROR);
	}
	
	/* find/load the kernel module */
	if (mod_load(argv[1], argc - 2, argv + 2) != 0)
	    return(CMD_ERROR);
	/* we have consumed all arguments */
	argc = 1;
    }

    /*
     * See if there is a kernel module already loaded
     */
    if (mod_findmodule(NULL, NULL) == NULL) {
	for (try = 0; (cp = getbootfile(try)) != NULL; try++) {
	    if (mod_load(cp, argc - 1, argv + 1) != 0) {
		printf("can't load '%s'\n", cp);
	    } else {
		/* we have consumed all arguments */
		argc = 1;
		break;
	    }
	}
    }

    /*
     * Loaded anything yet?
     */
    if ((km = mod_findmodule(NULL, NULL)) == NULL) {
	command_errmsg = "no bootable kernel";
	return(CMD_ERROR);
    }

    /*
     * If we were given arguments, discard any previous.
     * XXX should we merge arguments?  Hard to DWIM.
     */
    if (argc > 1) {
	if (km->m_args != NULL)	
	    free(km->m_args);
	km->m_args = unargv(argc - 1, argv + 1);
    }

    /* Hook for platform-specific autoloading of modules */
    if (archsw.arch_autoload() != 0)
	return(CMD_ERROR);

    /* Call the exec handler from the loader matching the kernel */
    module_formats[km->m_loader]->l_exec(km);
    return(CMD_ERROR);
}


/*
 * Autoboot after a delay
 */

COMMAND_SET(autoboot, "autoboot", "boot automatically after a delay", command_autoboot);

static int
command_autoboot(int argc, char *argv[])
{
    int		howlong;
    char	*cp, *prompt;

    prompt = NULL;
    howlong = -1;
    switch(argc) {
    case 3:
	prompt = argv[2];
	/* FALLTHROUGH */
    case 2:
	howlong = strtol(argv[1], &cp, 0);
	if (*cp != 0) {
	    sprintf(command_errbuf, "bad delay '%s'", argv[1]);
	    return(CMD_ERROR);
	}
	/* FALLTHROUGH */
    case 1:
	return(autoboot(howlong, prompt));
    }
	
    command_errmsg = "too many arguments";
    return(CMD_ERROR);
}

/*
 * Called before we go interactive.  If we think we can autoboot, and
 * we haven't tried already, try now.
 */
void
autoboot_maybe()
{
    char	*cp;
    
    cp = getenv("autoboot_delay");
    if ((autoboot_tried == 0) && ((cp == NULL) || strcasecmp(cp, "NO")))
	autoboot(-1, NULL);		/* try to boot automatically */
}

int
autoboot(int delay, char *prompt)
{
    time_t	when, otime, ntime;
    int		c, yes;
    char	*argv[2], *cp, *ep;

    autoboot_tried = 1;

    if (delay == -1) {
	/* try to get a delay from the environment */
	if ((cp = getenv("autoboot_delay"))) {
	    delay = strtol(cp, &ep, 0);
	    if (cp == ep)
		delay = -1;
	}
    }
    if (delay == -1)		/* all else fails */
	delay = 10;

    otime = time(NULL);
    when = otime + delay;	/* when to boot */
    yes = 0;

    /* XXX could try to work out what we might boot */
    printf("%s\n", (prompt == NULL) ? "Hit [Enter] to boot immediately, or any other key for command prompt." : prompt);

    for (;;) {
	if (ischar()) {
	    c = getchar();
	    if ((c == '\r') || (c == '\n'))
		yes = 1;
	    break;
	}
	ntime = time(NULL);
	if (ntime >= when) {
	    yes = 1;
	    break;
	}
	if (ntime != otime) {
	    printf("\rBooting [%s] in %d seconds... ", getbootfile(0), (int)(when - ntime));
	    otime = ntime;
	}
    }
    if (yes)
	printf("\rBooting [%s]...               ", getbootfile(0));
    putchar('\n');
    if (yes) {
	argv[0] = "boot";
	argv[1] = NULL;
	return(command_boot(1, argv));
    }
    return(CMD_OK);
}

/*
 * Scrounge for the name of the (try)'th file we will try to boot.
 */
static char *
getbootfile(int try) 
{
    static char *name = NULL;
    char	*spec, *ep;
    int		len;
    
    /* we use dynamic storage */
    if (name != NULL) {
	free(name);
	name = NULL;
    }
    
    /* 
     * Try $bootfile, then try our builtin default
     */
    if ((spec = getenv("bootfile")) == NULL)
	spec = default_bootfiles;

    while ((try > 0) && (spec != NULL)) {
	spec = strchr(spec, ';');
	try--;
    }
    if (spec != NULL) {
	if ((ep = strchr(spec, ';')) != NULL) {
	    len = ep - spec;
	} else {
	    len = strlen(spec);
	}
	name = malloc(len + 1);
	strncpy(name, spec, len);
	name[len] = 0;
    }
    if (name[0] == 0) {
	free(name);
	name = NULL;
    }
    return(name);
}


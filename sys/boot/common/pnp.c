/*
 * mjs copyright
 */
/*
 * "Plug and Play" functionality.
 *
 * We use the PnP enumerators to obtain identifiers for installed hardware,
 * and the contents of a database to determine modules to be loaded to support
 * such hardware.
 */

#include <stand.h>
#include <bootstrap.h>

static struct pnpinfo	*pnp_devices = NULL;

static void		pnp_discard(void);

/*
 * Perform complete enumeration sweep, and load required module(s) if possible.
 */

int
pnp_autoload(void) 
{
    int			hdlr, idx;

    /* forget anything we think we knew */
    pnp_discard();

    /* iterate over all of the handlers */
    for (hdlr = 0; pnphandlers[hdlr]->pp_name != NULL; i++) {
	printf("Probing bus '%s'...\n", pnphandlers[hdlr]->pp_name);
	idx = 0;
	while ((pi = pnphandlers[hdlr]->pp_enumerate(idx++)) != NULL) {
	    printf("  %s\n", pi->pi_ident);
	    pi->pi_handler = hdlr;
	    pi->pi_next = pnp_devices;
	    pnp_devices = pi;
	}
    }
    /* find anything? */
    if (pnp_devices != NULL) {
	/* XXX hardcoded paths! should use loaddev? */
	pnp_readconf("/boot/pnpdata.local");
	pnp_readconf("/boot/pnpdata");

	pnp_reload();
    }
}

/*
 * Try to load outstanding modules (eg. after disk change)
 */
int
pnp_reload(void)
{
    struct pnpinfo	*pi;
    char		*modfname;
	
    /* try to load any modules that have been nominated */
    for (pi = pnp_devices; pi != NULL; pi = pi->pi_next) {
	/* Already loaded? */
	if ((pi->pi_module != NULL) && (mod_findmodule(pi->pi_module, NULL) == NULL)) {
	    modfname = malloc(strlen(pi->pi_module + 3));
	    sprintf(modfname, "%s.ko", pi->pi_module);	/* XXX implicit knowledge of KLD module filenames */
	    if (mod_load(pi->pi_module, pi->pi_argc, pi->pi_argv))
		printf("Could not load module '%s' for device '%s'\n", modfname, pi->pi_ident);
	    free(modfname);
	}
    }
}


/*
 * Throw away anything we think we know about PnP devices
 */
static void
pnp_discard(void)
{
    struct pnpinfo	*pi;
    
    while (pnp_devices != NULL) {
	pi = pnp_devices;
	pnp_devices = pnp_devices->pi_next;
	if (pi->pi_ident)
	    free(pi->pi_ident);
	if (pi->pi_module)
	    free(pi->pi_module);
	if (pi->pi_argv)
	    free(pi->pi_argv);
	free(pi);
    }
}

/*
 * The PnP configuration database consists of a flat text file with 
 * entries one per line.  Valid lines are:
 *
 * # <text>
 *
 * 	This line is a comment, and ignored.
 *
 * [<name>]
 *
 *	Entries following this line are for devices connected to the
 *	bus <name>, At least one such entry must be encountered
 *	before identifiers are recognised.
 *
 * ident=<identifier> rev=<revision> module=<module> args=<arguments>
 *
 *	This line describes an identifier:module mapping.  The 'ident'
 *	and 'module' fields are required; the 'rev' field is currently
 *	ignored (but should be used), and the 'args' field must come
 *	last.
 */
static void
pnp_readconf(char *path)
{
    struct pnpinfo	*pi;
    int			fd, line;
    char		lbuf[128], *currbus, *ident, *revision, *module, *args;
    char		*cp, *ep, *tp, c;

    /* try to open the file */
    if ((fd = open(path, O_RDONLY)) >= 0) {
	line = 0;
	currbus = NULL;
	
	while (fgetstr(lbuf, sizeof(lbuf), fd) > 0) {
	    line++;
	    /* Find the first non-space character on the line */
	    for (cp = lbuf; (*cp != 0) && !isspace(*cp); cp++)
		;
	    
	    /* keep/discard? */
	    if ((*cp == 0) || (*cp == '#'))
		continue;
	    
	    /* bus declaration? */
	    if (*cp == '[') {
		if (((ep = strchr(cp, ']')) == NULL) || ((ep - cp) < 2)) {
		    printf("%s line %d: bad bus specification\n", path, line);
		} else {
		    if (currbus != NULL)
			free(currbus);
		    *ep = 0;
		    currbus = strdup(cp + 1);
		}
		continue;
	    }

	    /* XXX should we complain? */
	    if (currbus == NULL)
		continue;

	    /* mapping */
	    for (ident = module = args = NULL; *cp != 0;) {

		/* discard leading whitespace */
		if (isspace(*cp)) {
		    cp++;
		    continue;
		}
		
		/* scan for terminator, separator */
		for (ep = cp; (*ep != 0) && (*ep != '=') && !isspace(ep); ep++)
		    ;

		if (*ep == '=') {
		    *ep = 0;
		    for (tp = ep + 1; (*tp != 0) && !isspace(tp); tp++)
			;
		    c = *tp;
		    *tp = 0;
		    if ((ident == NULL) && !strcmp(cp, "ident")) {
			ident = ep + 1;
		    } else if ((revision == NULL) && !strcmp(cp, "revision")) {
			revision = ep + 1;
		    } else if ((args == NULL) && !strcmp(cp, "args")) {
			*tp = c;
			while (*tp != 0)		/* skip to end of string */
			    tp++;
			args = ep + 1;
		    } else {
			/* XXX complain? */
		    }
		    cp = tp;
		    continue;
		}
		
		/* it's garbage or a keyword - ignore it for now */
		cp = ep;
	    }

	    /* we must have at least ident and module set */
	    if ((ident == NULL) || (module == NULL)) {
		printf("%s line %d: bad mapping\n", path, line);
		continue;
	    }
	    
	    /*
	     * Loop looking for module/bus that might match this 
	     * XXX no revision parse/test here yet.
	     */
	    for (pi = pnp_modules; pi != NULL; pi = pi->pi_next) {
		if (!strcmp(pnphandlers[pi->pi_handler]->pp_name, currbus) &&
		    !strcmp(pi->pi_indent, ident)) {
		    if (args != NULL)
			if (parse(&pi->pi_argc, &pi->pi_argv, args)) {
			    printf("%s line %d: bad arguments\n", path, line);
			    break;
			}
		    pi->pi_module = strdup(module);
		}
	    }
	}
	close(fd);
    }
}


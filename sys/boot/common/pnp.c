/*
 * mjs copyright
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * "Plug and Play" functionality.
 *
 * We use the PnP enumerators to obtain identifiers for installed hardware,
 * and the contents of a database to determine modules to be loaded to support
 * such hardware.
 */

#include <stand.h>
#include <string.h>
#include <bootstrap.h>

struct pnpinfo_stql	pnp_devices;
static int		pnp_devices_initted = 0;

static void		pnp_discard(void);

/*
 * Perform complete enumeration sweep
 */

COMMAND_SET(pnpscan, "pnpscan", "scan for PnP devices", pnp_scan);

static int
pnp_scan(int argc, char *argv[]) 
{
    struct pnpinfo	*pi;
    int			hdlr;
    int			verbose;
    int			ch;

    if (pnp_devices_initted == 0) {
	STAILQ_INIT(&pnp_devices);
	pnp_devices_initted = 1;
    }

    verbose = 0;
    optind = 1;
    optreset = 1;
    while ((ch = getopt(argc, argv, "v")) != -1) {
	switch(ch) {
	case 'v':
	    verbose = 1;
	    break;
	case '?':
	default:
	    /* getopt has already reported an error */
	    return(CMD_OK);
	}
    }

    /* forget anything we think we knew */
    pnp_discard();

    /* iterate over all of the handlers */
    for (hdlr = 0; pnphandlers[hdlr] != NULL; hdlr++) {
	if (verbose)
	    printf("Probing %s...\n", pnphandlers[hdlr]->pp_name);
	pnphandlers[hdlr]->pp_enumerate();
    }
    if (verbose) {
	pager_open();
	pager_output("PNP scan summary:\n");
	STAILQ_FOREACH(pi, &pnp_devices, pi_link) {
	    pager_output(STAILQ_FIRST(&pi->pi_ident)->id_ident);	/* first ident should be canonical */
	    if (pi->pi_desc != NULL) {
		pager_output(" : ");
		pager_output(pi->pi_desc);
	    }
	    pager_output("\n");
	}
	pager_close();
    }
    return(CMD_OK);
}

#if 0
/*
 * Try to load outstanding modules (eg. after disk change)
 */
COMMAND_SET(pnpload, "pnpload", "load modules for PnP devices", pnp_load);

static int
pnp_load(int argc, char *argv[])
{
    struct pnpinfo	*pi;
    char		*modfname;
	
    /* find anything? */
    if (STAILQ_FIRST(&pnp_devices) != NULL) {

	/* check for kernel, assign modules handled by static drivers there */
	if (pnp_scankernel()) {
	    command_errmsg = "cannot load drivers until kernel loaded";
	    return(CMD_ERROR);
	}
	if (fname == NULL) {
	    /* default paths */
	    pnp_readconf("/boot/pnpdata.local");
	    pnp_readconf("/boot/pnpdata");
	} else {
	    if (pnp_readconf(fname)) {
		sprintf(command_errbuf, "can't read PnP information from '%s'", fname);
		return(CMD_ERROR);
	    }
	}

	/* try to load any modules that have been nominated */
	STAILQ_FOREACH(pi, &pnp_devices, pi_link) {
	    /* Already loaded? */
	    if ((pi->pi_module != NULL) && (file_findfile(pi->pi_module, NULL) == NULL)) {
		modfname = malloc(strlen(pi->pi_module) + 4);
		sprintf(modfname, "%s.ko", pi->pi_module);	/* XXX implicit knowledge of KLD module filenames */
		if (mod_load(pi->pi_module, pi->pi_argc, pi->pi_argv))
		    printf("Could not load module '%s' for device '%s'\n", modfname, STAILQ_FIRST(&pi->pi_ident)->id_ident);
		free(modfname);
	    }
	}
    }
    return(CMD_OK);
}
#endif
/*
 * Throw away anything we think we know about PnP devices.
 */
static void
pnp_discard(void)
{
    struct pnpinfo	*pi;

    while (STAILQ_FIRST(&pnp_devices) != NULL) {
	pi = STAILQ_FIRST(&pnp_devices);
	STAILQ_REMOVE_HEAD(&pnp_devices, pi_link);
	pnp_freeinfo(pi);
    }
}
#if 0
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
 *
 * Comments may be appended to lines; any character including or following
 * '#' on a line is ignored.
 */
static int
pnp_readconf(char *path)
{
    struct pnpinfo	*pi;
    struct pnpident	*id;
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

	    /* cut trailing comment? */
	    if ((ep = strchr(cp, '#')) != NULL)
		*ep = 0;
	    
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
	    for (ident = module = args = revision = NULL; *cp != 0;) {

		/* discard leading whitespace */
		if (isspace(*cp)) {
		    cp++;
		    continue;
		}
		
		/* scan for terminator, separator */
		for (ep = cp; (*ep != 0) && (*ep != '=') && !isspace(*ep); ep++)
		    ;

		if (*ep == '=') {
		    *ep = 0;
		    for (tp = ep + 1; (*tp != 0) && !isspace(*tp); tp++)
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

	    /* we must have at least ident and module set to be interesting */
	    if ((ident == NULL) || (module == NULL))
		continue;
	    
	    /*
	     * Loop looking for module/bus that might match this, but aren't already
	     * assigned.
	     * XXX no revision parse/test here yet.
	     */
	    STAILQ_FOREACH(pi, &pnp_devices, pi_link) {

		/* no driver assigned, bus matches OK */
		if ((pi->pi_module == NULL) &&
		    !strcmp(pi->pi_handler->pp_name, currbus)) {

		    /* scan idents, take first match */
		    STAILQ_FOREACH(id, &pi->pi_ident, id_link)
			if (!strcmp(id->id_ident, ident))
			    break;
			
		    /* find a match? */
		    if (id != NULL) {
			if (args != NULL)
			    if (parse(&pi->pi_argc, &pi->pi_argv, args)) {
				printf("%s line %d: bad arguments\n", path, line);
				continue;
			    }
			pi->pi_module = strdup(module);
			printf("use module '%s' for %s:%s\n", module, pi->pi_handler->pp_name, id->id_ident);
		    }
		}
	    }
	}
	close(fd);
    }
    return(CMD_OK);
}

static int
pnp_scankernel(void)
{
    return(CMD_OK);
}
#endif
/*
 * Add a unique identifier to (pi)
 */
void
pnp_addident(struct pnpinfo *pi, char *ident)
{
    struct pnpident	*id;

    STAILQ_FOREACH(id, &pi->pi_ident, id_link)
	if (!strcmp(id->id_ident, ident))
	    return;			/* already have this one */

    id = malloc(sizeof(struct pnpident));
    id->id_ident = strdup(ident);
    STAILQ_INSERT_TAIL(&pi->pi_ident, id, id_link);
}

/*
 * Allocate a new pnpinfo struct
 */
struct pnpinfo *
pnp_allocinfo(void)
{
    struct pnpinfo	*pi;
    
    pi = malloc(sizeof(struct pnpinfo));
    bzero(pi, sizeof(struct pnpinfo));
    STAILQ_INIT(&pi->pi_ident);
    return(pi);
}

/*
 * Release storage held by a pnpinfo struct
 */
void
pnp_freeinfo(struct pnpinfo *pi)
{
    struct pnpident	*id;

    while (!STAILQ_EMPTY(&pi->pi_ident)) {
	id = STAILQ_FIRST(&pi->pi_ident);
	STAILQ_REMOVE_HEAD(&pi->pi_ident, id_link);
	free(id->id_ident);
	free(id);
    }
    if (pi->pi_desc)
	free(pi->pi_desc);
    if (pi->pi_module)
	free(pi->pi_module);
    if (pi->pi_argv)
	free(pi->pi_argv);
    free(pi);
}

/*
 * Add a new pnpinfo struct to the list.
 */
void
pnp_addinfo(struct pnpinfo *pi)
{
    STAILQ_INSERT_TAIL(&pnp_devices, pi, pi_link);
}


/*
 * Format an EISA id as a string in standard ISA PnP format, AAAIIRR
 * where 'AAA' is the EISA vendor ID, II is the product ID and RR the revision ID.
 */
char *
pnp_eisaformat(u_int8_t *data)
{
    static char	idbuf[8];
    const char	hextoascii[] = "0123456789abcdef";

    idbuf[0] = '@' + ((data[0] & 0x7c) >> 2);
    idbuf[1] = '@' + (((data[0] & 0x3) << 3) + ((data[1] & 0xe0) >> 5));
    idbuf[2] = '@' + (data[1] & 0x1f);
    idbuf[3] = hextoascii[(data[2] >> 4)];
    idbuf[4] = hextoascii[(data[2] & 0xf)];
    idbuf[5] = hextoascii[(data[3] >> 4)];
    idbuf[6] = hextoascii[(data[3] & 0xf)];
    idbuf[7] = 0;
    return(idbuf);
}


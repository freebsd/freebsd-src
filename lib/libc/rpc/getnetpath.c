/*	$NetBSD: getnetpath.c,v 1.3 2000/07/06 03:10:34 christos Exp $	*/

/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user or with the express written consent of
 * Sun Microsystems, Inc.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)getnetpath.c	1.11 91/12/19 SMI";
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Copyright (c) 1989 by Sun Microsystems, Inc.
 */

#include "namespace.h"
#include <stdio.h>
#include <errno.h>
#include <netconfig.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include "un-namespace.h"

/*
 * internal structure to keep track of a netpath "session"
 */
struct netpath_chain {
    struct netconfig *ncp;  /* an nconf entry */
    struct netpath_chain *nchain_next;	/* next nconf entry allocated */
};


struct netpath_vars {
    int   valid;	    /* token that indicates a valid netpath_vars */
    void *nc_handlep;	    /* handle for current netconfig "session" */
    char *netpath;	    /* pointer to current view-point in NETPATH */
    char *netpath_start;    /* pointer to start of our copy of NETPATH */
    struct netpath_chain *ncp_list;  /* list of nconfs allocated this session*/
};

#define NP_VALID	0xf00d
#define NP_INVALID	0

char *_get_next_token(char *, int);


/*
 * A call to setnetpath() establishes a NETPATH "session".  setnetpath()
 * must be called before the first call to getnetpath().  A "handle" is
 * returned to distinguish the session; this handle should be passed
 * subsequently to getnetpath().  (Handles are used to allow for nested calls
 * to setnetpath()).
 * If setnetpath() is unable to establish a session (due to lack of memory
 * resources, or the absence of the /etc/netconfig file), a NULL pointer is
 * returned.
 */

void *
setnetpath()
{

    struct netpath_vars *np_sessionp;   /* this session's variables */
    char *npp;				/* NETPATH env variable */

#ifdef MEM_CHK
    malloc_debug(1);
#endif

    if ((np_sessionp =
	(struct netpath_vars *)malloc(sizeof (struct netpath_vars))) == NULL) {
	return (NULL);
    }
    if ((np_sessionp->nc_handlep = setnetconfig()) == NULL) {
	free(np_sessionp);
	syslog (LOG_ERR, "rpc: failed to open " NETCONFIG);
	goto failed;
    }
    np_sessionp->valid = NP_VALID;
    np_sessionp->ncp_list = NULL;
    if ((npp = getenv(NETPATH)) == NULL) {
	np_sessionp->netpath = NULL;
    } else {
	(void) endnetconfig(np_sessionp->nc_handlep);/* won't need nc session*/
	np_sessionp->nc_handlep = NULL;
	if ((np_sessionp->netpath = malloc(strlen(npp)+1)) == NULL)
		goto failed;
	else {
	    (void) strcpy(np_sessionp->netpath, npp);
	}
    }
    np_sessionp->netpath_start = np_sessionp->netpath;
    return ((void *)np_sessionp);

failed:
    free(np_sessionp);
    return (NULL);
}

/*
 * When first called, getnetpath() returns a pointer to the netconfig
 * database entry corresponding to the first valid NETPATH component.  The
 * netconfig entry is formatted as a struct netconfig.
 * On each subsequent call, getnetpath returns a pointer to the netconfig
 * entry that corresponds to the next valid NETPATH component.  getnetpath
 * can thus be used to search the netconfig database for all networks
 * included in the NETPATH variable.
 * When NETPATH has been exhausted, getnetpath() returns NULL.  It returns
 * NULL and sets errno in case of an error (e.g., setnetpath was not called
 * previously).
 * getnetpath() silently ignores invalid NETPATH components.  A NETPATH
 * compnent is invalid if there is no corresponding entry in the netconfig
 * database.
 * If the NETPATH variable is unset, getnetpath() behaves as if NETPATH
 * were set to the sequence of default or visible networks in the netconfig
 * database, in the order in which they are listed.
 */

struct netconfig *
getnetpath(handlep)
    void *handlep;
{
    struct netpath_vars *np_sessionp = (struct netpath_vars *)handlep;
    struct netconfig *ncp = NULL;   /* temp. holds a netconfig session */
    struct netpath_chain *chainp;   /* holds chain of ncp's we alloc */
    char  *npp;		/* holds current NETPATH */

    if (np_sessionp == NULL || np_sessionp->valid != NP_VALID) {
	errno = EINVAL;
	return (NULL);
    }
    if (np_sessionp->netpath_start == NULL) {	/* NETPATH was not set */
	do {                /* select next visible network */
	    if (np_sessionp->nc_handlep == NULL) {
		np_sessionp->nc_handlep = setnetconfig();
		if (np_sessionp->nc_handlep == NULL)
		    syslog (LOG_ERR, "rpc: failed to open " NETCONFIG);
	    }
	    if ((ncp = getnetconfig(np_sessionp->nc_handlep)) == NULL) {
		return(NULL);
	    }
	} while ((ncp->nc_flag & NC_VISIBLE) == 0);
	return (ncp);
    }
    /*
     * Find first valid network ID in netpath.
     */
    while ((npp = np_sessionp->netpath) != NULL && strlen(npp) != 0) {
	np_sessionp->netpath = _get_next_token(npp, ':');
    	/*
    	 * npp is a network identifier.
	 */
	if ((ncp = getnetconfigent(npp)) != NULL) {
	    chainp = (struct netpath_chain *)	/* cobble alloc chain entry */
		    malloc(sizeof (struct netpath_chain));
	    chainp->ncp = ncp;
	    chainp->nchain_next = NULL;
	    if (np_sessionp->ncp_list == NULL) {
		np_sessionp->ncp_list = chainp;
	    } else {
		np_sessionp->ncp_list->nchain_next = chainp;
	    }
	    return (ncp);
	}
	/* couldn't find this token in the database; go to next one. */
    }
    return (NULL);
}

/*
 * endnetpath() may be called to unbind NETPATH when processing is complete,
 * releasing resources for reuse.  It returns 0 on success and -1 on failure
 * (e.g. if setnetpath() was not called previously.
 */
int
endnetpath(handlep)
    void *handlep;
{
    struct netpath_vars *np_sessionp = (struct netpath_vars *)handlep;
    struct netpath_chain *chainp, *lastp;

    if (np_sessionp == NULL || np_sessionp->valid != NP_VALID) {
	errno = EINVAL;
	return (-1);
    }
    if (np_sessionp->nc_handlep != NULL)
	endnetconfig(np_sessionp->nc_handlep);
    if (np_sessionp->netpath_start != NULL)
	free(np_sessionp->netpath_start);
    for (chainp = np_sessionp->ncp_list; chainp != NULL;
	    lastp=chainp, chainp=chainp->nchain_next, free(lastp)) {
	freenetconfigent(chainp->ncp);
    }
    free(np_sessionp);
#ifdef MEM_CHK
    if (malloc_verify() == 0) {
	fprintf(stderr, "memory heap corrupted in endnetpath\n");
	exit(1);
    }
#endif
    return (0);
}



/*
 * Returns pointer to the rest-of-the-string after the current token.
 * The token itself starts at arg, and we null terminate it.  We return NULL
 * if either the arg is empty, or if this is the last token.
 */

char *
_get_next_token(npp, token)
char *npp;		/* string */
int token;		/* char to parse string for */
{
    char  *cp;		/* char pointer */
    char  *np;		/* netpath pointer */
    char  *ep;		/* escape pointer */

    if ((cp = strchr(npp, token)) == NULL) {
	return (NULL);
    }
    /*
     * did find a token, but it might be escaped.
     */
    if ((cp > npp) && (cp[-1] == '\\')) {
        /* if slash was also escaped, carry on, otherwise find next token */
	if ((cp > npp + 1) && (cp[-2] != '\\')) {
	    /* shift r-o-s  onto the escaped token */
	    strcpy(&cp[-1], cp);    /* XXX: overlapping string copy */
	    /*
	     * Do a recursive call.
	     * We don't know how many escaped tokens there might be.
	     */
	    return (_get_next_token(cp, token));
	}
    }

    *cp++ = '\0';		/* null-terminate token */
    /* get rid of any backslash escapes */
    ep = npp;
    while ((np = strchr(ep, '\\')) != 0) {
	if (np[1] == '\\')
	    np++;
	strcpy(np, (ep = &np[1]));  /* XXX: overlapping string copy */
    }
    return (cp);		/* return ptr to r-o-s */
}

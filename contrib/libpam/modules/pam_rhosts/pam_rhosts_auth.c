/*----------------------------------------------------------------------
 * Modified for Linux-PAM by Al Longyear <longyear@netcom.com> 96/5/5
 * Modifications, Cristian Gafton 97/2/8
 * Modifications, Peter Allgeyer 97/3
 * Modifications (netgroups and fixes), Nicolai Langfeldt 97/3/21
 * Security fix: 97/10/2 - gethostbyname called repeatedly without care
 * Modification (added privategroup option) Andrew <morgan@transmeta.com>
 *----------------------------------------------------------------------
 * Copyright (c) 1983, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define _BSD_SOURCE

#define USER_RHOSTS_FILE "/.rhosts"     /* prefixed by user's home dir */

#ifdef linux
#include <endian.h>
#endif

#ifdef NEED_FSUID_H
#include <sys/fsuid.h>
#endif /* NEED_FSUID_H */

#include <sys/types.h>
#include <sys/uio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>       /* This is supposed(?) to contain the following */
int innetgr(const char *, const char *, const char *,const char *);

#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include <arpa/inet.h>

#ifndef MAXDNAME
#define MAXDNAME  256
#endif

#include <stdarg.h>
#include <ctype.h>

#include <net/if.h>
#ifdef linux
# include <linux/sockios.h>
# ifndef __USE_MISC
#  define __USE_MISC
#  include <sys/fsuid.h>
# endif /* __USE_MISC */
#endif

#include <pwd.h>
#include <grp.h>
#include <sys/file.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <syslog.h>
#ifndef _PATH_HEQUIV
#define _PATH_HEQUIV "/etc/hosts.equiv"
#endif /* _PATH_HEQUIV */

#define PAM_SM_AUTH  /* only defines this management group */

#include <security/pam_modules.h>
#include <security/_pam_macros.h>

/* to the best of my knowledge, all modern UNIX boxes have 32 bit integers */
#define U32 unsigned int


/*
 * Options for this module
 */

struct _options {
    int  opt_no_hosts_equiv;
    int  opt_hosts_equiv_rootok;
    int  opt_no_rhosts;
    int  opt_debug;
    int  opt_nowarn;
    int  opt_disallow_null_authtok;
    int  opt_silent;
    int  opt_promiscuous;
    int  opt_suppress;
    int  opt_private_group;
    int  opt_no_uid_check;
    const char *superuser;
    const char *last_error;
};

/* logging */
static void _pam_log(int err, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    openlog("pam_rhosts_auth", LOG_CONS|LOG_PID, LOG_AUTH);
    vsyslog(err, format, args);
    va_end(args);
    closelog();
}

static void set_option (struct _options *opts, const char *arg)
{
    if (strcmp(arg, "no_hosts_equiv") == 0) {
	opts->opt_no_hosts_equiv = 1;
	return;
    }

    if (strcmp(arg, "hosts_equiv_rootok") == 0) {
        opts->opt_hosts_equiv_rootok = 1;
        return;
    }

    if (strcmp(arg, "no_rhosts") == 0) {
	opts->opt_no_rhosts = 1;
	return;
    }

    if (strcmp(arg, "debug") == 0) {
	D(("debugging enabled"));
	opts->opt_debug = 1;
	return;
    }

    if (strcmp(arg, "no_warn") == 0) {
	opts->opt_nowarn = 1;
	return;
    }

    if (strcmp(arg, "promiscuous") == 0) {
	opts->opt_promiscuous = 1;   /* used to permit '+' in ...hosts file */
	return;
    }

    if (strcmp(arg, "suppress") == 0) {
	opts->opt_suppress = 1; /* used to suppress failure warning message */
	return;
    }

    if (strcmp(arg, "privategroup") == 0) {
	opts->opt_private_group = 1; /* used to permit group write on .rhosts
					file if group has same name as owner */
	return;
    }

    if (strcmp(arg, "no_uid_check") == 0) {
	opts->opt_no_uid_check = 1;  /* NIS optimization */
	return;
    }

    if (strcmp(arg, "superuser=") == 0) {
	opts->superuser = arg+sizeof("superuser=")-1;
	return;
    }
    /*
     * All other options are ignored at the present time.
     */
    _pam_log(LOG_WARNING, "unrecognized option '%s'", arg);
}

static void set_parameters (struct _options *opts, int flags,
			    int argc, const char **argv)
{
    opts->opt_silent                = flags & PAM_SILENT;
    opts->opt_disallow_null_authtok = flags & PAM_DISALLOW_NULL_AUTHTOK;

    while (argc-- > 0) {
	set_option (opts, *argv);
	++argv;
    }
}

/*
 * Obtain the name of the remote host. Currently, this is simply by
 * requesting the contents of the PAM_RHOST item.
 */

static int pam_get_rhost(pam_handle_t *pamh, const char **rhost
			 , const char *prompt)
{
    int retval;
    const char   *current;

    retval = pam_get_item (pamh, PAM_RHOST, (const void **)&current);
    if (retval != PAM_SUCCESS)
        return retval;

    if (current == NULL) {
	return PAM_AUTH_ERR;
    }
    *rhost = current;

    return retval;        /* pass on any error from conversation */
}

/*
 * Obtain the name of the remote user. Currently, this is simply by
 * requesting the contents of the PAM_RUSER item.
 */

static int pam_get_ruser(pam_handle_t *pamh, const char **ruser
			 , const char *prompt)
{
    int retval;
    const char   *current;

    retval = pam_get_item (pamh, PAM_RUSER, (const void **)&current);
    if (retval != PAM_SUCCESS)
        return retval;

    if (current == NULL) {
	return PAM_AUTH_ERR;
    }
    *ruser = current;

    return retval;        /* pass on any error from conversation */
}

/*
 * Returns 1 if positive match, 0 if no match, -1 if negative match.
 */

static int
__icheckhost (pam_handle_t *pamh, struct _options *opts, U32 raddr
	      , register char *lhost, const char *rhost)
{
    struct hostent *hp;
    U32 laddr;
    int negate=1;    /* Multiply return with this to get -1 instead of 1 */
    char **pp, *user;

    /* Check nis netgroup.  We assume that pam has done all needed
       paranoia checking before we are handed the rhost */
    if (strncmp("+@",lhost,2) == 0)
      return(innetgr(&lhost[2],rhost,NULL,NULL));

    if (strncmp("-@",lhost,2) == 0)
      return(-innetgr(&lhost[2],rhost,NULL,NULL));

    /* -host */
    if (strncmp("-",lhost,1) == 0) {
	negate=-1;
	lhost++;
    } else if (strcmp("+",lhost) == 0) {
        (void) pam_get_item(pamh, PAM_USER, (const void **)&user);
        D(("user %s has a `+' host entry", user));
	if (opts->opt_promiscuous)
	    return (1);                     /* asking for trouble, but ok.. */
	/* If not promiscuous: handle as negative */
	return (-1);
    }

    /* Try for raw ip address first. */
    if (isdigit(*lhost) && (long)(laddr = inet_addr(lhost)) != -1)
	return (negate*(! (raddr ^ laddr)));

    /* Better be a hostname. */
    hp = gethostbyname(lhost);
    if (hp == NULL)
	return (0);
    
    /* Spin through ip addresses. */
    for (pp = hp->h_addr_list; *pp; ++pp)
	if (!memcmp (&raddr, *pp, sizeof (U32)))
	    return (negate);

    /* No match. */
    return (0);
}

/* Returns 1 on positive match, 0 on no match, -1 on negative match */

static int __icheckuser(pam_handle_t *pamh, struct _options *opts
			, const char *luser, const char *ruser
			, const char *rhost)
{
    /*
      luser is user entry from .rhosts/hosts.equiv file
      ruser is user id on remote host
      rhost is the remote host name
      */
    char *user;

    /* [-+]@netgroup */
    if (strncmp("+@",luser,2) == 0)
	return (innetgr(&luser[2],NULL,ruser,NULL));

    if (strncmp("-@",luser,2) == 0)
	return (-innetgr(&luser[2],NULL,ruser,NULL));

    /* -user */
    if (strncmp("-",luser,1) == 0)
	return(-(strcmp(&luser[1],ruser) == 0));

    /* + */
    if (strcmp("+",luser) == 0) {
	(void) pam_get_item(pamh, PAM_USER, (const void **)&user);
	_pam_log(LOG_WARNING, "user %s has a `+' user entry", user);
	if (opts->opt_promiscuous)
	    return(1);
	/* If not promiscuous we handle it as a negative match */
	return(-1);
    }

    /* simple string match */
    return (strcmp(ruser, luser) == 0);
}

/*
 * Returns 1 for blank lines (or only comment lines) and 0 otherwise
 */

static int __isempty(char *p)
{
    while (*p && isspace(*p)) {
	++p;
    }

    return (*p == '\0' || *p == '#') ? 1:0 ;
}

/*
 * Returns 0 if positive match, 1 if _not_ ok.
 */

static int
__ivaliduser (pam_handle_t *pamh, struct _options *opts,
	      FILE *hostf, U32 raddr,
	      const char *luser, const char *ruser, const char *rhost)
{
    register const char *user;
    register char *p;
    int hcheck, ucheck;
    char buf[MAXHOSTNAMELEN + 128];                       /* host + login */

    buf[sizeof (buf)-1] = '\0';                 	/* terminate line */

    while (fgets(buf, sizeof(buf), hostf) != NULL) {   /* hostf file line */
        p = buf;                              /* from beginning of file.. */

	/* Skip empty or comment lines */
	if (__isempty(p)) {
	    continue;
	}

	/* Skip lines that are too long. */
	if (strchr(p, '\n') == NULL) {
	    int ch = getc(hostf);

	    while (ch != '\n' && ch != EOF)
		ch = getc(hostf);
	    continue;
	}

	/*
	 * If there is a hostname at the start of the line.  Set it to
	 * lower case. A leading ' ' or '\t' indicates no hostname
	 */

	for (;*p && !isspace(*p); ++p) {
	    *p = tolower(*p);
	}

	/*
	 * next we want to find the permitted name for the remote user
	 */

	if (*p == ' ' || *p == '\t') {

	    /* <nul> terminate hostname and skip spaces */
	    for (*p++='\0'; *p && isspace(*p); ++p);

	    user = p;                   /* this is the user's name */
	    while (*p && !isspace(*p))
		++p;                    /* find end of user's name */
	} else 
	    user = p;

	*p = '\0';              /* <nul> terminate username (+host?) */

	/* buf -> host(?) ; user -> username(?) */

	/* First check host part */
	hcheck=__icheckhost(pamh, opts, raddr, buf, rhost);

	if (hcheck<0)
	    return(1);

	if (hcheck) {
	    /* Then check user part */
	    if (! (*user))
		user = luser;

	    ucheck=__icheckuser(pamh, opts, user, ruser, rhost);

	    /* Positive 'host user' match? */
	    if (ucheck>0)
		return(0);

	    /* Negative 'host -user' match? */
	    if (ucheck<0)
		return(1);

	    /* Neither, go on looking for match */
	}
    }

    return (1);
}

/*
 * New .rhosts strategy: We are passed an ip address. We spin through
 * hosts.equiv and .rhosts looking for a match. When the .rhosts only
 * has ip addresses, we don't have to trust a nameserver.  When it
 * contains hostnames, we spin through the list of addresses the nameserver
 * gives us and look for a match.
 *
 * Returns 0 if ok, -1 if not ok.
 */

static int
pam_iruserok(pam_handle_t *pamh,
	 struct _options *opts, U32 raddr, int superuser,
	 const char *ruser, const char *luser, const char *rhost)
{
    const char *cp;
    struct stat sbuf;
    struct passwd *pwd;
    FILE *hostf;
    uid_t uid;
    int answer;
    char pbuf[MAXPATHLEN];               /* potential buffer overrun */

    if ((!superuser||opts->opt_hosts_equiv_rootok) && !opts->opt_no_hosts_equiv ) {

	/* try to open system hosts.equiv file */
	hostf = fopen (_PATH_HEQUIV, "r");
	if (hostf) {
	    answer = __ivaliduser(pamh, opts, hostf, raddr, luser
				  , ruser, rhost);
	    (void) fclose(hostf);
	    if (answer == 0)
		return 0;      /* remote host is equivalent to localhost */
	} /* else {
	    No hosts.equiv file on system.
	} */
    }
    
    if ( opts->opt_no_rhosts )
	return 1;

    /*
     * Identify user's local .rhosts file
     */

    pwd = getpwnam(luser);
    if (pwd == NULL) {
	/* 
	 * luser is assumed to be valid because of an earlier check for uid = 0
	 * we don't log this error twice. However, this shouldn't happen !
	 * --cristiang 
	 */
	return(1);
    }

    /* check for buffer overrun */
    if (strlen(pwd->pw_dir) + sizeof(USER_RHOSTS_FILE) + 2 >= MAXPATHLEN) {
	if (opts->opt_debug)
	    _pam_log(LOG_DEBUG,"home directory for `%s' is too long", luser);
	return 1;                               /* to dangerous to try */
    }

    (void) strcpy(pbuf, pwd->pw_dir);
    (void) strcat(pbuf, USER_RHOSTS_FILE);

    /*
     * Change effective uid while _reading_ .rhosts. (not just
     * opening).  If root and reading an NFS mounted file system,
     * can't read files that are 0600 as .rhosts files should be.
     */

    /* We are root, this will not fail */
#ifdef linux
    /* If we are on linux the better way is setfsuid */
    uid = setfsuid(pwd->pw_uid);
    hostf = fopen(pbuf, "r");
#else
    uid = geteuid();
    (void) seteuid(pwd->pw_uid);
    hostf = fopen(pbuf, "r");
#endif

    if (hostf == NULL) {
        if (opts->opt_debug)
	    _pam_log(LOG_DEBUG,"Could not open %s file",pbuf);
	answer = 1;
	goto exit_function;
    }

    /*
     * If not a regular file, or is owned by someone other than
     * user or root or if writeable by anyone but the owner, quit.
     */

    cp = NULL;
    if (lstat(pbuf, &sbuf) < 0 || !S_ISREG(sbuf.st_mode))
	cp = ".rhosts not regular file";
    else if (fstat(fileno(hostf), &sbuf) < 0)
	cp = ".rhosts fstat failed";
    else if (sbuf.st_uid && sbuf.st_uid != pwd->pw_uid)
	cp = "bad .rhosts owner";
    else if (sbuf.st_mode & S_IWOTH)
	cp = ".rhosts writable by other!";
    else if (sbuf.st_mode & S_IWGRP) {

	/* private group caveat */
	if (opts->opt_private_group) {
	    struct group *grp = getgrgid(sbuf.st_gid);

	    if (NULL == grp || NULL == grp->gr_name
		|| strcmp(luser,grp->gr_name)) {
		cp = ".rhosts writable by public group";
	    } else if (grp->gr_mem) {
		int gcount;

		/* require at most one member (luser) of this group */
		for (gcount=0; grp->gr_mem[gcount]; ++gcount) {
		    if (strcmp(grp->gr_mem[gcount], luser)) {
			gcount = -1;
			break;
		    }
		}
		if (gcount < 0) {
		    cp = ".rhosts writable by other members of group";
		}
	    }
	} else {
	    cp = ".rhosts writable by group";
	}

    } /* It is _NOT_ safe to append an else here...  Do so prior to
       * S_IWGRP check */

    /* If there were any problems, quit. */
    if (cp) {
	opts->last_error = cp;
	answer = 1;
	goto exit_function;
    }

    answer = __ivaliduser (pamh, opts, hostf, raddr, luser, ruser, rhost);

exit_function:
    /*
     * Go here to exit after the fsuid/euid has been adjusted so that
     * they are reset before we exit.
     */

#ifdef linux
    setfsuid(uid);
#else
    (void)seteuid(uid);
#endif

    if (hostf != NULL)
        (void) fclose(hostf);

    return answer;
}

static int
pam_ruserok (pam_handle_t *pamh,
	     struct _options *opts, const char *rhost, int superuser,
	     const char *ruser, const char *luser)
{
    struct hostent *hp;
    int answer = 1;                             /* default to failure */
    U32 *addrs;
    int n, i;

    opts->last_error = (char *) 0;
    hp               = gethostbyname(rhost);         /* identify host */

    if (hp != NULL) {
	/* First of all check the address length */
        if (hp->h_length != 4) {
            _pam_log(LOG_ALERT, "pam_rhosts module can't work with not IPv4 "
		     "addresses");
            return 1;                                    /* not allowed */
        }

        /* loop though address list */
        for (n = 0; hp->h_addr_list[n]; n++);
        D(("rhosts: %d addresses", n));

        if (n) {
            addrs = calloc (n, hp->h_length);
            for (i = 0; i < n; i++)
                memcpy (addrs+i, hp->h_addr_list[i], hp->h_length);

            for (i = 0; i < n && answer; i++) {
                D(("rhosts: address %d is %04x", i, addrs[i]));
                answer = pam_iruserok(pamh, opts, addrs[i], superuser,
				      ruser, luser, rhost);
                         /* answer == 0 means success */
            }

            free (addrs);
        }
    }

    return answer;
}

/*
 * Internal function to do authentication
 */

static int _pam_auth_rhosts (pam_handle_t *pamh,
			     int flags, 
			     int argc,
			     const char **argv) 
{
    int retval;
    const char *luser;
    const char *ruser,*rhost;
    struct _options opts;
    int as_root = 0;
    /*
     * Look at the options and set the flags accordingly.
     */
    memset (&opts, 0, sizeof (opts));
    set_parameters (&opts, flags, argc, argv);
    /*
     * Obtain the parameters for the various items
     */
    for (;;) {                         /* abuse loop to avoid goto */

	/* get the remotehost */
	retval = pam_get_rhost(pamh, &rhost, NULL);
	(void) pam_set_item(pamh, PAM_RHOST, rhost);
	if (retval != PAM_SUCCESS) {
	    if (opts.opt_debug) {
		_pam_log(LOG_DEBUG, "could not get the remote host name");
	    }
	    break;
	}

	/* get the remote user */
	retval = pam_get_ruser(pamh, &ruser, NULL);
	(void) pam_set_item(pamh, PAM_RUSER, ruser);
	if (retval != PAM_SUCCESS) {
	    if (opts.opt_debug)
		_pam_log(LOG_DEBUG, "could not get the remote username");
	    break;
	}

	/* get the local user */
	retval = pam_get_user(pamh, &luser, NULL);

	if (retval != PAM_SUCCESS) {
	    if (opts.opt_debug)
		_pam_log(LOG_DEBUG, "could not determine name of local user");
	    break;
	}

	if (opts.superuser && !strcmp(opts.superuser, luser)) {
	    as_root = 1;
	}

	/* check if the luser uid == 0... --cristiang */
	if (! opts.opt_no_uid_check) {
	    struct passwd *luser_pwd;

	    luser_pwd = getpwnam(luser);
	    if (luser_pwd == NULL) {
		if (opts.opt_debug)
		    _pam_log(LOG_DEBUG, "user '%s' unknown to this system",
			     luser);
		retval = PAM_AUTH_ERR;
		break;
	    }
	    if (luser_pwd->pw_uid == 0)
		as_root = 1;
	    luser_pwd = NULL;                                   /* forget */
	}
/*
 * Validate the account information.
 */
	if (pam_ruserok (pamh, &opts, rhost, as_root, ruser, luser) != 0) {
	    if ( !opts.opt_suppress ) {
		_pam_log(LOG_WARNING, "denied to %s@%s as %s: %s",
			 ruser, rhost, luser, (opts.last_error==NULL) ?
			 "access not allowed":opts.last_error);
	    }
	    retval = PAM_AUTH_ERR;
	} else {
	    _pam_log(LOG_NOTICE, "allowed to %s@%s as %s",
                     ruser, rhost, luser);
	}
	break;
    }

    return retval;
}

/* --- authentication management functions --- */

PAM_EXTERN
int pam_sm_authenticate (pam_handle_t *pamh, 
			 int flags,
			 int argc, 
			 const char **argv)
{
    int retval;

    if (sizeof(U32) != 4) {
        _pam_log (LOG_ALERT, "pam_rhosts module can\'t work on this hardware "
		  "(yet)");
        return PAM_AUTH_ERR;
    }
    sethostent(1);
    retval = _pam_auth_rhosts (pamh, flags, argc, argv);
    endhostent();
    return retval;
}

PAM_EXTERN
int pam_sm_setcred(pam_handle_t *pamh,int flags,int argc,
		   const char **argv)
{
    return PAM_SUCCESS;
}

/* end of module definition */


#ifdef PAM_STATIC

/* static module data */

struct pam_module _pam_rhosts_auth_modstruct = {
    "pam_rhosts_auth",
    pam_sm_authenticate,
    pam_sm_setcred,
    NULL,
    NULL,
    NULL,
    NULL,
};

#endif

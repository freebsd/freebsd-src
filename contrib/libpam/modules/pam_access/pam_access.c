/* pam_access module */

/*
 * Written by Alexei Nogin <alexei@nogin.dnttm.ru> 1997/06/15
 * (I took login_access from logdaemon-5.6 and converted it to PAM
 * using parts of pam_time code.)
 *
 ************************************************************************ 
 * Copyright message from logdaemon-5.6 (original file name DISCLAIMER)
 ************************************************************************ 
 * Copyright 1995 by Wietse Venema. All rights reserved. Individual files 
 * may be covered by other copyrights (as noted in the file itself.) 
 * 
 * This material was originally written and compiled by Wietse Venema at 
 * Eindhoven University of Technology, The Netherlands, in 1990, 1991, 
 * 1992, 1993, 1994 and 1995. 
 * 
 * Redistribution and use in source and binary forms are permitted 
 * provided that this entire copyright notice is duplicated in all such 
 * copies. 
 * 
 * This software is provided "as is" and without any expressed or implied 
 * warranties, including, without limitation, the implied warranties of 
 * merchantibility and fitness for any particular purpose. 
 *************************************************************************
 */

#include <security/_pam_aconf.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
/* man page says above file includes this... */
extern int gethostname(char *name, size_t len);

#include <stdarg.h>
#include <syslog.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <ctype.h>
#include <sys/utsname.h>

#ifndef BROKEN_NETWORK_MATCH
# include <netdb.h>
# include <sys/socket.h>
#endif

/*
 * here, we make definitions for the externally accessible functions
 * in this file (these definitions are required for static modules
 * but strongly encouraged generally) they are used to instruct the
 * modules include file to define their prototypes.
 */

#define PAM_SM_ACCOUNT

#include <security/_pam_macros.h>
#include <security/pam_modules.h>

int strcasecmp(const char *s1, const char *s2);

/* login_access.c from logdaemon-5.6 with several changes by A.Nogin: */

 /*
  * This module implements a simple but effective form of login access
  * control based on login names and on host (or domain) names, internet
  * addresses (or network numbers), or on terminal line names in case of
  * non-networked logins. Diagnostics are reported through syslog(3).
  * 
  * Author: Wietse Venema, Eindhoven University of Technology, The Netherlands.
  */

#if !defined(MAXHOSTNAMELEN) || (MAXHOSTNAMELEN < 64)
#undef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 256
#endif

#ifdef DEFAULT_CONF_FILE
# define PAM_ACCESS_CONFIG DEFAULT_CONF_FILE
#else
# define PAM_ACCESS_CONFIG "/etc/security/access.conf"
#endif

 /* Delimiters for fields and for lists of users, ttys or hosts. */

static const char fs[] = ":";			/* field separator */
static const char sep[] = ", \t";		/* list-element separator */

 /* Constants to be used in assignments only, not in comparisons... */

#define YES             1
#define NO              0

 /*
  * A structure to bundle up all login-related information to keep the
  * functional interfaces as generic as possible.
  */
struct login_info {
    struct passwd *user;
    char   *from;
    const char *config_file;
    const char *service;
};

/* --- static functions for checking whether the user should be let in --- */

static void _log_err(const char *format, ... )
{
    va_list args;

    va_start(args, format);
    openlog("pam_access", LOG_CONS|LOG_PID, LOG_AUTH);
    vsyslog(LOG_ERR, format, args);
    va_end(args);
    closelog();
}

/* Parse module config arguments */

static int parse_args(struct login_info *loginfo, int argc, const char **argv)
{
    int i;

    for (i=0; i<argc; ++i) {
	if (!strncmp("accessfile=", argv[i], 11)) {
	    FILE *fp = fopen(11 + argv[i], "r");

	    if (fp) {
		loginfo->config_file = 11 + argv[i];
		fclose(fp);
	    } else {
		_log_err("for service [%s] failed to open accessfile=[%s]"
			 , loginfo->service, 11 + argv[i]);
		return 0;
	    }
	    
	} else {
	    _log_err("unrecognized option [%s]", argv[i]);
	}
    }
    
    return 1;  /* OK */
}

typedef int match_func (char *, struct login_info *); 

static int list_match (char *, struct login_info *,
		             match_func *);
static int user_match (char *, struct login_info *);
static int from_match (char *, struct login_info *);
static int string_match (char *, char *);

/* login_access - match username/group and host/tty with access control file */

static int login_access(struct login_info *item)
{
    FILE   *fp;
    char    line[BUFSIZ];
    char   *perm;		/* becomes permission field */
    char   *users;		/* becomes list of login names */
    char   *froms;		/* becomes list of terminals or hosts */
    int     match = NO;
    int     end;
    int     lineno = 0;		/* for diagnostics */

    /*
     * Process the table one line at a time and stop at the first match.
     * Blank lines and lines that begin with a '#' character are ignored.
     * Non-comment lines are broken at the ':' character. All fields are
     * mandatory. The first field should be a "+" or "-" character. A
     * non-existing table means no access control.
     */

    if ((fp = fopen(item->config_file, "r"))!=NULL) {
	while (!match && fgets(line, sizeof(line), fp)) {
	    lineno++;
	    if (line[end = strlen(line) - 1] != '\n') {
		_log_err("%s: line %d: missing newline or line too long",
		       item->config_file, lineno);
		continue;
	    }
	    if (line[0] == '#')
		continue;			/* comment line */
	    while (end > 0 && isspace(line[end - 1]))
		end--;
	    line[end] = 0;			/* strip trailing whitespace */
	    if (line[0] == 0)			/* skip blank lines */
		continue;
	    if (!(perm = strtok(line, fs))
		|| !(users = strtok((char *) 0, fs))
		|| !(froms = strtok((char *) 0, fs))
		|| strtok((char *) 0, fs)) {
		_log_err("%s: line %d: bad field count",
			 item->config_file, lineno);
		continue;
	    }
	    if (perm[0] != '+' && perm[0] != '-') {
		_log_err("%s: line %d: bad first field",
			 item->config_file, lineno);
		continue;
	    }
	    match = (list_match(froms, item, from_match)
		     && list_match(users, item, user_match));
	}
	(void) fclose(fp);
    } else if (errno != ENOENT) {
	_log_err("cannot open %s: %m", item->config_file);
    }
    return (match == 0 || (line[0] == '+'));
}

/* list_match - match an item against a list of tokens with exceptions */

static int list_match(char *list, struct login_info *item, match_func *match_fn)
{
    char   *tok;
    int     match = NO;

    /*
     * Process tokens one at a time. We have exhausted all possible matches
     * when we reach an "EXCEPT" token or the end of the list. If we do find
     * a match, look for an "EXCEPT" list and recurse to determine whether
     * the match is affected by any exceptions.
     */

    for (tok = strtok(list, sep); tok != 0; tok = strtok((char *) 0, sep)) {
	if (strcasecmp(tok, "EXCEPT") == 0)	/* EXCEPT: give up */
	    break;
	if ((match = (*match_fn) (tok, item)))	/* YES */
	    break;
    }
    /* Process exceptions to matches. */

    if (match != NO) {
	while ((tok = strtok((char *) 0, sep)) && strcasecmp(tok, "EXCEPT"))
	     /* VOID */ ;
	if (tok == 0 || list_match((char *) 0, item, match_fn) == NO)
	    return (match);
    }
    return (NO);
}

/* myhostname - figure out local machine name */

static char * myhostname(void)
{
    static char name[MAXHOSTNAMELEN + 1];

    gethostname(name, MAXHOSTNAMELEN);
    name[MAXHOSTNAMELEN] = 0;
    return (name);
}

/* netgroup_match - match group against machine or user */

static int netgroup_match(char *group, char *machine, char *user)
{
#ifdef NIS
    static char *mydomain = 0;

    if (mydomain == 0)
	yp_get_default_domain(&mydomain);
    return (innetgr(group, machine, user, mydomain));
#else
    _log_err("NIS netgroup support not configured");
    return (NO);
#endif
}

/* user_match - match a username against one token */

static int user_match(char *tok, struct login_info *item)
{
    char   *string = item->user->pw_name;
    struct login_info fake_item;
    struct group *group;
    int     i;
    char   *at;

    /*
     * If a token has the magic value "ALL" the match always succeeds.
     * Otherwise, return YES if the token fully matches the username, if the
     * token is a group that contains the username, or if the token is the
     * name of the user's primary group.
     */

    if ((at = strchr(tok + 1, '@')) != 0) {	/* split user@host pattern */
	*at = 0;
	fake_item.from = myhostname();
	return (user_match(tok, item) && from_match(at + 1, &fake_item));
    } else if (tok[0] == '@') {			/* netgroup */
	return (netgroup_match(tok + 1, (char *) 0, string));
    } else if (string_match(tok, string)) {	/* ALL or exact match */
	return (YES);
    } else if ((group = getgrnam(tok))) {	/* try group membership */
	if (item->user->pw_gid == group->gr_gid)
	    return (YES);
	for (i = 0; group->gr_mem[i]; i++)
	    if (strcasecmp(string, group->gr_mem[i]) == 0)
		return (YES);
    }
    return (NO);
}

/* from_match - match a host or tty against a list of tokens */

static int from_match(char *tok, struct login_info *item)
{
    char   *string = item->from;
    int     tok_len;
    int     str_len;

    /*
     * If a token has the magic value "ALL" the match always succeeds. Return
     * YES if the token fully matches the string. If the token is a domain
     * name, return YES if it matches the last fields of the string. If the
     * token has the magic value "LOCAL", return YES if the string does not
     * contain a "." character. If the token is a network number, return YES
     * if it matches the head of the string.
     */

    if (tok[0] == '@') {			/* netgroup */
	return (netgroup_match(tok + 1, string, (char *) 0));
    } else if (string_match(tok, string)) {	/* ALL or exact match */
	return (YES);
    } else if (tok[0] == '.') {			/* domain: match last fields */
	if ((str_len = strlen(string)) > (tok_len = strlen(tok))
	    && strcasecmp(tok, string + str_len - tok_len) == 0)
	    return (YES);
    } else if (strcasecmp(tok, "LOCAL") == 0) {	/* local: no dots */
	if (strchr(string, '.') == 0)
	    return (YES);
#ifdef BROKEN_NETWORK_MATCH
    } else if (tok[(tok_len = strlen(tok)) - 1] == '.'	/* network */
	       && strncmp(tok, string, tok_len) == 0) {
	return (YES);
#else /*  BROKEN_NETWORK_MATCH */
    } else if (tok[(tok_len = strlen(tok)) - 1] == '.') {
        /*
           The code below does a more correct check if the address specified
           by "string" starts from "tok".
                               1998/01/27  Andrey V. Savochkin <saw@msu.ru>
         */
        struct hostent *h;
        char hn[3+1+3+1+3+1+3+1];
        int r;

        h = gethostbyname(string);
        if (h == NULL)
	    return (NO);
        if (h->h_addrtype != AF_INET)
	    return (NO);
        if (h->h_length != 4)
	    return (NO); /* only IPv4 addresses (SAW) */
        r = snprintf(hn, sizeof(hn), "%u.%u.%u.%u",
                (unsigned char)h->h_addr[0], (unsigned char)h->h_addr[1],
                (unsigned char)h->h_addr[2], (unsigned char)h->h_addr[3]);
        if (r < 0 || r >= sizeof(hn))
	    return (NO);
        if (!strncmp(tok, hn, tok_len))
	    return (YES);
#endif /*  BROKEN_NETWORK_MATCH */
    }
    return (NO);
}

/* string_match - match a string against one token */

static int string_match(char *tok, char *string)
{

    /*
     * If the token has the magic value "ALL" the match always succeeds.
     * Otherwise, return YES if the token fully matches the string.
     */

    if (strcasecmp(tok, "ALL") == 0) {		/* all: always matches */
	return (YES);
    } else if (strcasecmp(tok, string) == 0) {	/* try exact match */
	return (YES);
    }
    return (NO);
}

/* end of login_access.c */

int strcasecmp(const char *s1, const char *s2) 
{
    while ((toupper(*s1)==toupper(*s2)) && (*s1) && (*s2)) {s1++; s2++;}
    return(toupper(*s1)-toupper(*s2));
}

/* --- public account management functions --- */

PAM_EXTERN int pam_sm_acct_mgmt(pam_handle_t *pamh,int flags,int argc
		     ,const char **argv)
{
    struct login_info loginfo;
    const char *user=NULL, *service=NULL;
    char *from=NULL;
    struct passwd *user_pw;

    if ((pam_get_item(pamh, PAM_SERVICE, (const void **)&service)
	!= PAM_SUCCESS) || (service == NULL) || (*service == ' ')) {
	_log_err("cannot find the service name");
	return PAM_ABORT;
    }

    /* set username */

    if (pam_get_user(pamh, &user, NULL) != PAM_SUCCESS || user == NULL
	|| *user == '\0') {
	_log_err("cannot determine the user's name");
	return PAM_USER_UNKNOWN;
    }

    /* remote host name */

    if (pam_get_item(pamh, PAM_RHOST, (const void **)&from)
	!= PAM_SUCCESS) {
	_log_err("cannot find the remote host name");
	return PAM_ABORT;
    }

    if (from==NULL) {

        /* local login, set tty name */

        if (pam_get_item(pamh, PAM_TTY, (const void **)&from) != PAM_SUCCESS
            || from == NULL) {
            D(("PAM_TTY not set, probing stdin"));
	    from = ttyname(STDIN_FILENO);
	    if (from == NULL) {
	        _log_err("couldn't get the tty name");
	        return PAM_ABORT;
	     }
	    if (pam_set_item(pamh, PAM_TTY, from) != PAM_SUCCESS) {
	        _log_err("couldn't set tty name");
	        return PAM_ABORT;
	     }
        }
        if (strncmp("/dev/",from,5) == 0) {          /* strip leading /dev/ */
	    from += 5;
        }

    }

    if ((user_pw=getpwnam(user))==NULL) return (PAM_USER_UNKNOWN);

    /*
     * Bundle up the arguments to avoid unnecessary clumsiness later on.
     */
    loginfo.user = user_pw;
    loginfo.from = from;
    loginfo.service = service;
    loginfo.config_file = PAM_ACCESS_CONFIG;

    /* parse the argument list */

    if (!parse_args(&loginfo, argc, argv)) {
	_log_err("failed to parse the module arguments");
	return PAM_ABORT;
    }

    if (login_access(&loginfo)) {
	return (PAM_SUCCESS);
    } else {
	_log_err("access denied for user `%s' from `%s'",user,from);
	return (PAM_PERM_DENIED);
    }
}

/* end of module definition */

#ifdef PAM_STATIC

/* static module data */

struct pam_module _pam_access_modstruct = {
    "pam_access",
    NULL,
    NULL,
    pam_sm_acct_mgmt,
    NULL,
    NULL,
    NULL
};
#endif


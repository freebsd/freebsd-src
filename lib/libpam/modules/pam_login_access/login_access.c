 /*
  * This module implements a simple but effective form of login access
  * control based on login names and on host (or domain) names, internet
  * addresses (or network numbers), or on terminal line names in case of
  * non-networked logins. Diagnostics are reported through syslog(3).
  *
  * Author: Wietse Venema, Eindhoven University of Technology, The Netherlands.
  */

#if 0
#ifndef lint
static char sccsid[] = "%Z% %M% %I% %E% %U%";
#endif
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <ctype.h>
#include <errno.h>
#include <grp.h>
#include <netdb.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "pam_login_access.h"

 /* Constants to be used in assignments only, not in comparisons... */

#define YES             1
#define NO              0

static int	from_match(const char *, const char *, struct pam_login_access_options *);
static int	list_match(char *, const char *,
				int (*)(const char *, const char *,
				struct pam_login_access_options *),
				struct pam_login_access_options *);
static int	netgroup_match(const char *, const char *, const char *);
static int	string_match(const char *, const char *);
static int	user_match(const char *, const char *, struct pam_login_access_options *);
static int	group_match(const char *, const char *);

/* login_access - match username/group and host/tty with access control file */

int
login_access(const char *user, const char *from,
	struct pam_login_access_options *login_access_opts)
{
    FILE   *fp;
    char    line[BUFSIZ];
    char   *perm;			/* becomes permission field */
    char   *users;			/* becomes list of login names */
    char   *froms;			/* becomes list of terminals or hosts */
    int     match = NO;
    int     end;
    int     lineno = 0;			/* for diagnostics */
    const char *fieldsep = login_access_opts->fieldsep;

    /*
     * Process the table one line at a time and stop at the first match.
     * Blank lines and lines that begin with a '#' character are ignored.
     * Non-comment lines are broken at the ':' character. All fields are
     * mandatory. The first field should be a "+" or "-" character. A
     * non-existing table means no access control.
     */

    if ((fp = fopen(login_access_opts->accessfile, "r")) != NULL) {
	while (!match && fgets(line, sizeof(line), fp)) {
	    lineno++;
	    if (line[end = strlen(line) - 1] != '\n') {
		syslog(LOG_ERR, "%s: line %d: missing newline or line too long",
		       login_access_opts->accessfile, lineno);
		continue;
	    }
	    if (line[0] == '#')
		continue;			/* comment line */
	    while (end > 0 && isspace(line[end - 1]))
		end--;
	    line[end] = 0;			/* strip trailing whitespace */
	    if (line[0] == 0)			/* skip blank lines */
		continue;
	    if (!(perm = strtok(line, fieldsep))
		|| !(users = strtok((char *) 0, fieldsep))
		|| !(froms = strtok((char *) 0, fieldsep))
		|| strtok((char *) 0, fieldsep)) {
		syslog(LOG_ERR, "%s: line %d: bad field count", login_access_opts->accessfile,
		       lineno);
		continue;
	    }
	    if (perm[0] != '+' && perm[0] != '-') {
		syslog(LOG_ERR, "%s: line %d: bad first field", login_access_opts->accessfile,
		       lineno);
		continue;
	    }
	    match = (list_match(froms, from, from_match, login_access_opts)
		     && list_match(users, user, user_match, login_access_opts));
	}
	(void) fclose(fp);
    } else if (errno != ENOENT) {
	syslog(LOG_ERR, "cannot open %s: %m", login_access_opts->accessfile);
    }
    return (match == 0 || (line[0] == '+'));
}

/* list_match - match an item against a list of tokens with exceptions */

static int
list_match(char *list, const char *item,
    int (*match_fn)(const char *, const char *, struct pam_login_access_options *),
    struct pam_login_access_options *login_access_opts)
{
    char   *tok;
    int     match = NO;
    const char *listsep = login_access_opts->listsep;

    /*
     * Process tokens one at a time. We have exhausted all possible matches
     * when we reach an "EXCEPT" token or the end of the list. If we do find
     * a match, look for an "EXCEPT" list and recurse to determine whether
     * the match is affected by any exceptions.
     */

    for (tok = strtok(list, listsep); tok != NULL; tok = strtok((char *) 0, listsep)) {
	if (strcmp(tok, "EXCEPT") == 0)	/* EXCEPT: give up */
	    break;
	if ((match = (*match_fn)(tok, item, login_access_opts)) != 0)	/* YES */
	    break;
    }
    /* Process exceptions to matches. */

    if (match != NO) {
	while ((tok = strtok((char *) 0, listsep)) && strcmp(tok, "EXCEPT")) {
	     /* VOID */ ;
	}
	if (tok == NULL ||
	    list_match((char *) 0, item, match_fn, login_access_opts) == NO) {
		return (match);
	}
    }
    return (NO);
}

/* netgroup_match - match group against machine or user */

static int
netgroup_match(const char *group, const char *machine, const char *user)
{
    char domain[1024];
    unsigned int i;

    if (getdomainname(domain, sizeof(domain)) != 0 || *domain == '\0') {
	syslog(LOG_ERR, "NIS netgroup support disabled: no NIS domain");
	return (NO);
    }

    /* getdomainname() does not reliably terminate the string */
    for (i = 0; i < sizeof(domain); ++i)
	if (domain[i] == '\0')
	    break;
    if (i == sizeof(domain)) {
	syslog(LOG_ERR, "NIS netgroup support disabled: invalid NIS domain");
	return (NO);
    }

    if (innetgr(group, machine, user, domain) == 1)
	return (YES);
    return (NO);
}

/* group_match - match a group against one token */

int
group_match(const char *tok, const char *username)
{
    struct group *group;
    struct passwd *passwd;
    gid_t *grouplist;
    int i, ret, ngroups = NGROUPS;

    if ((passwd = getpwnam(username)) == NULL)
	return (NO);
    errno = 0;
    if ((group = getgrnam(tok)) == NULL) {
	if (errno != 0)
	    syslog(LOG_ERR, "getgrnam() failed for %s: %s", username, strerror(errno));
	else
	    syslog(LOG_NOTICE, "group not found: %s", username);
	return (NO);
    }
    if ((grouplist = calloc(ngroups, sizeof(gid_t))) == NULL) {
	syslog(LOG_ERR, "cannot allocate memory for grouplist: %s", username);
	return (NO);
    }
    ret = NO;
    if (getgrouplist(username, passwd->pw_gid, grouplist, &ngroups) != 0)
	syslog(LOG_ERR, "getgrouplist() failed for %s", username);
    for (i = 0; i < ngroups; i++)
	if (grouplist[i] == group->gr_gid)
	    ret = YES;
    free(grouplist);
    return (ret);
}

/* user_match - match a username against one token */

static int
user_match(const char *tok, const char *string,
	struct pam_login_access_options *login_access_opts)
{
    size_t stringlen;
    char *grpstr;
    int rc;

    /*
     * If a token has the magic value "ALL" the match always succeeds.
     * Otherwise, return YES if the token fully matches the username, or if
     * the token is a group that contains the username.
     */

    if (tok[0] == '@') {			/* netgroup */
	return (netgroup_match(tok + 1, (char *) 0, string));
    } else if (tok[0] == '(' && tok[(stringlen = strlen(&tok[1]))] == ')') {		/* group */
	if ((grpstr = strndup(&tok[1], stringlen - 1)) == NULL) {
	    syslog(LOG_ERR, "cannot allocate memory for %s", string);
	    return (NO);
	}
	rc = group_match(grpstr, string);
	free(grpstr);
	return (rc);
    } else if (string_match(tok, string)) {	/* ALL or exact match */
	return (YES);
    } else if (login_access_opts->defgroup == true) {/* try group membership */
	return (group_match(tok, string));
    }
    return (NO);
}

/* from_match - match a host or tty against a list of tokens */

static int
from_match(const char *tok, const char *string,
	struct pam_login_access_options *login_access_opts __unused)
{
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
    } else if (strcmp(tok, "LOCAL") == 0) {	/* local: no dots */
	if (strchr(string, '.') == NULL)
	    return (YES);
    } else if (tok[(tok_len = strlen(tok)) - 1] == '.'	/* network */
	       && strncmp(tok, string, tok_len) == 0) {
	return (YES);
    }
    return (NO);
}

/* string_match - match a string against one token */

static int
string_match(const char *tok, const char *string)
{

    /*
     * If the token has the magic value "ALL" the match always succeeds.
     * Otherwise, return YES if the token fully matches the string.
     */

    if (strcmp(tok, "ALL") == 0) {		/* all: always matches */
	return (YES);
    } else if (strcasecmp(tok, string) == 0) {	/* try exact match */
	return (YES);
    }
    return (NO);
}

/* pam_issue module - a simple /etc/issue parser to set PAM_USER_PROMPT
 *
 * Copyright 1999 by Ben Collins <bcollins@debian.org>
 *
 * Needs to be called before any other auth modules so we can setup the
 * user prompt before it's first used. Allows one argument option, which
 * is the full path to a file to be used for issue (uses /etc/issue as a
 * default) such as "issue=/etc/issue.telnet".
 *
 * We can also parse escapes within the the issue file (enabled by
 * default, but can be disabled with the "noesc" option). It's the exact
 * same parsing as util-linux's agetty program performs.
 *
 * Released under the GNU LGPL version 2 or later
 */

#define _GNU_SOURCE
#define _BSD_SOURCE

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <utmp.h>
#include <malloc.h>

#include <security/_pam_macros.h>

#define PAM_SM_AUTH

#include <security/pam_modules.h>

static int _user_prompt_set = 0;

char *do_prompt (FILE *);

/* --- authentication management functions (only) --- */

PAM_EXTERN
int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc,
                        const char **argv)
{
    int retval = PAM_SUCCESS;
    FILE *fd;
    int parse_esc = 1;
    char *prompt_tmp = NULL, *cur_prompt = NULL;
    struct stat st;
    char *issue_file = NULL;

   /* If we've already set the prompt, don't set it again */
    if(_user_prompt_set)
	return PAM_IGNORE;
    else
       /* we set this here so if we fail below, we wont get further
	  than this next time around (only one real failure) */
	_user_prompt_set = 1;

    for ( ; argc-- > 0 ; ++argv ) {
	if (!strncmp(*argv,"issue=",6)) {
	    issue_file = (char *) strdup(6+*argv);
	    if (issue_file != NULL) {
		D(("set issue_file to: %s", issue_file));
	    } else {
		D(("failed to strdup issue_file - ignored"));
		return PAM_IGNORE;
	    }
	} else if (!strcmp(*argv,"noesc")) {
	    parse_esc = 0;
	    D(("turning off escape parsing by request"));
	} else
	    D(("unknown option passed: %s", *argv));
    }

    if (issue_file == NULL)
	issue_file = strdup("/etc/issue");

    if ((fd = fopen(issue_file, "r")) != NULL) {
	int tot_size = 0;

	if (stat(issue_file, &st) < 0)
	    return PAM_IGNORE;

	retval = pam_get_item(pamh, PAM_USER_PROMPT, (const void **) &cur_prompt);
	if (retval != PAM_SUCCESS)
	    return PAM_IGNORE;

       /* first read in the issue file */

	if (parse_esc)
	    prompt_tmp = do_prompt(fd);
	else {
	    int count = 0;
	    prompt_tmp = malloc(st.st_size + 1);
	    if (prompt_tmp == NULL) return PAM_IGNORE;
	    memset (prompt_tmp, '\0', st.st_size + 1);
	    count = fread(prompt_tmp, sizeof(char *), st.st_size, fd);
	    prompt_tmp[st.st_size] = '\0';
	}

	fclose(fd);

	tot_size = strlen(prompt_tmp) + strlen(cur_prompt) + 1;

       /*
	* alloc some extra space for the original prompt
	* and postpend it to the buffer
	*/
	prompt_tmp = realloc(prompt_tmp, tot_size);
	strcpy(prompt_tmp+strlen(prompt_tmp), cur_prompt);

	prompt_tmp[tot_size] = '\0';

	retval = pam_set_item(pamh, PAM_USER_PROMPT, (const char *) prompt_tmp);

	free(issue_file);
	free(prompt_tmp);
    } else {
	D(("could not open issue_file: %s", issue_file));
	return PAM_IGNORE;
    }

    return retval;
}

PAM_EXTERN
int pam_sm_setcred(pam_handle_t *pamh, int flags, int argc,
                   const char **argv)
{
     return PAM_IGNORE;
}

char *do_prompt(FILE *fd)
{
    int c, size = 1024;
    char *issue = (char *)malloc(size);
    char buf[1024];
    struct utsname uts;

    if (issue == NULL || fd == NULL)
	return NULL;

    issue[0] = '\0'; /* zero this, for strcat to work on first buf */
    (void) uname(&uts);

    while ((c = getc(fd)) != EOF) {
	if (c == '\\') {
	    c = getc(fd);
	    switch (c) {
	      case 's':
		snprintf (buf, 1024, "%s", uts.sysname);
		break;
	      case 'n':
		snprintf (buf, 1024, "%s", uts.nodename);
		break;
	      case 'r':
		snprintf (buf, 1024, "%s", uts.release);
		break;
	      case 'v':
		snprintf (buf, 1024, "%s", uts.version);
		break;
	      case 'm':
		snprintf (buf, 1024, "%s", uts.machine);
		break;
	      case 'o':
		{
		    char domainname[256];

		    getdomainname(domainname, sizeof(domainname));
		    domainname[sizeof(domainname)-1] = '\0';
		    snprintf (buf, 1024, "%s", domainname);
		}
		break;

	      case 'd':
	      case 't':
		{
		    const char *weekday[] = {
			"Sun", "Mon", "Tue", "Wed", "Thu",
			"Fri", "Sat" };
		    const char *month[] = {
			"Jan", "Feb", "Mar", "Apr", "May",
			"Jun", "Jul", "Aug", "Sep", "Oct",
			"Nov", "Dec" };
		    time_t now;
		    struct tm *tm;

		    (void) time (&now);
		    tm = localtime(&now);

		    if (c == 'd')
			snprintf (buf, 1024, "%s %s %d  %d",
				weekday[tm->tm_wday], month[tm->tm_mon],
				tm->tm_mday, 
				tm->tm_year + 1900);
		    else
			snprintf (buf, 1024, "%02d:%02d:%02d",
				tm->tm_hour, tm->tm_min, tm->tm_sec);
		}
		break;
	      case 'l':
		{
		    char *ttyn = ttyname(1);
		    if (!strncmp(ttyn, "/dev/", 5))
			ttyn += 5;
		    snprintf (buf, 1024, "%s", ttyn);
		}
		break;
	      case 'u':
	      case 'U':
		{
		    int users = 0;
		    struct utmp *ut;
		    setutent();
		    while ((ut = getutent()))
			if (ut->ut_type == USER_PROCESS)
			users++;
		    endutent();
		    printf ("%d ", users);
		    if (c == 'U')
			snprintf (buf, 1024, "%s", (users == 1) ?
				" user" : " users");
		    break;
		}
	      default:
		buf[0] = c; buf[1] = '\0';
	    }
	    if ((strlen(issue) + strlen(buf)) < size + 1) {
		size += strlen(buf) + 1;
		issue = (char *) realloc (issue, size);
	    }
	    strcat(issue, buf);
	} else {
	    buf[0] = c; buf[1] = '\0';
	    if ((strlen(issue) + strlen(buf)) < size + 1) {
		size += strlen(buf) + 1;
		issue = (char *) realloc (issue, size);
	    }
	    strcat(issue, buf);
	}
    }
    return issue;
}

#ifdef PAM_STATIC

/* static module data */

struct pam_module _pam_issue_modstruct = {
     "pam_issue",
     pam_sm_authenticate,
     pam_sm_setcred,
     NULL,
     NULL,
     NULL,
     NULL,
};

#endif

/* end of module definition */

/*
 * pam_limits - impose resource limits when opening a user session
 *
 * 1.6 - modified for PLD (added process priority settings) 
 *       by Marcin Korzonek <mkorz@shadow.eu.org
 * 1.5 - Elliot Lee's "max system logins patch"
 * 1.4 - addressed bug in configuration file parser
 * 1.3 - modified the configuration file format
 * 1.2 - added 'debug' and 'conf=' arguments
 * 1.1 - added @group support    
 * 1.0 - initial release - Linux ONLY
 *
 * See end for Copyright information
 */

#if !(defined(linux))
#error THIS CODE IS KNOWN TO WORK ONLY ON LINUX !!!
#endif 

#include <security/_pam_aconf.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <syslog.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>

#include <utmp.h>
#ifndef UT_USER  /* some systems have ut_name instead of ut_user */
#define UT_USER ut_user
#endif

#include <grp.h>
#include <pwd.h>

/* Module defines */
#define LINE_LENGTH 1024

#define LIMITS_DEF_USER     0 /* limit was set by an user entry */
#define LIMITS_DEF_GROUP    1 /* limit was set by a group entry */
#define LIMITS_DEF_DEFAULT  2 /* limit was set by an default entry */
#define LIMITS_DEF_NONE     3 /* this limit was not set yet */

struct user_limits_struct {
    int src_soft;
    int src_hard;
    struct rlimit limit;
};

/* internal data */
struct pam_limit_s {
    int login_limit;     /* the max logins limit */
    int login_limit_def; /* which entry set the login limit */
    int flag_numsyslogins; /* whether to limit logins only for a
			      specific user or to count all logins */
    int priority;	 /* the priority to run user process with */
    struct user_limits_struct limits[RLIM_NLIMITS];
    char conf_file[BUFSIZ];
};

#define LIMIT_LOGIN RLIM_NLIMITS+1
#define LIMIT_NUMSYSLOGINS RLIM_NLIMITS+2

#define LIMIT_PRI RLIM_NLIMITS+3

#define LIMIT_SOFT  1
#define LIMIT_HARD  2

#define PAM_SM_SESSION

#include <security/pam_modules.h>
#include <security/_pam_macros.h>

/* logging */
static void _pam_log(int err, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    openlog("pam_limits", LOG_CONS|LOG_PID, LOG_AUTH);
    vsyslog(err, format, args);
    va_end(args);
    closelog();
}

/* argument parsing */

#define PAM_DEBUG_ARG       0x0001
#define PAM_DO_SETREUID     0x0002

static int _pam_parse(int argc, const char **argv, struct pam_limit_s *pl)
{
     int ctrl=0;

     /* step through arguments */
     for (ctrl=0; argc-- > 0; ++argv) {

          /* generic options */

          if (!strcmp(*argv,"debug"))
               ctrl |= PAM_DEBUG_ARG;
          else if (!strncmp(*argv,"conf=",5))
                strcpy(pl->conf_file,*argv+5);
	  else if (!strncmp(*argv,"change_uid",10))
		ctrl |= PAM_DO_SETREUID;
          else {
               _pam_log(LOG_ERR,"pam_parse: unknown option; %s",*argv);
          }
     }

     return ctrl;
}


/* limits stuff */
#ifdef DEFAULT_CONF_FILE
# define LIMITS_FILE DEFAULT_CONF_FILE
#else
# define LIMITS_FILE "/etc/security/limits.conf"
#endif

#define LIMIT_ERR 1 /* error setting a limit */
#define LOGIN_ERR 2 /* too many logins err */

/* Counts the number of user logins and check against the limit*/
static int check_logins(const char *name, int limit, int ctrl,
			struct pam_limit_s *pl)
{
    struct utmp *ut;
    unsigned int count;

    if (ctrl & PAM_DEBUG_ARG) {
	 _pam_log(LOG_DEBUG, "checking logins for '%s' / %d\n", name,limit);
    }

    if (limit < 0)
        return 0; /* no limits imposed */
    if (limit == 0) /* maximum 0 logins ? */ {
        _pam_log(LOG_WARNING, "No logins allowed for '%s'\n", name);
        return LOGIN_ERR;
    }

    setutent();
    count = 0;
    while((ut = getutent())) {
#ifdef USER_PROCESS
        if (ut->ut_type != USER_PROCESS)
            continue;
#endif
        if (ut->UT_USER[0] == '\0')
            continue;
        if (!pl->flag_numsyslogins
	    && strncmp(name, ut->UT_USER, sizeof(ut->UT_USER)) != 0)
            continue;
        if (++count >= limit)
            break;
    }
    endutent();
    if (count >= limit) {
	if (name) {
	    _pam_log(LOG_WARNING, "Too many logins (max %d) for %s",
		     limit, name);
	} else {
	    _pam_log(LOG_WARNING, "Too many system logins (max %d)", limit);
	}
        return LOGIN_ERR;
    }
    return 0;
}

/* checks if a user is on a list of members of the GID 0 group */
static int is_on_list(char * const *list, const char *member)
{
    while (*list) {
        if (strcmp(*list, member) == 0)
            return 1;
        list++;
    }
    return 0;
}

/* Checks if a user is a member of a group */
static int is_on_group(const char *user_name, const char *group_name)
{
    struct passwd *pwd;
    struct group *grp, *pgrp;
    char uname[LINE_LENGTH], gname[LINE_LENGTH];
    
    if (!strlen(user_name))
        return 0;
    if (!strlen(group_name))
        return 0;
    memset(uname, 0, sizeof(uname));
    strncpy(uname, user_name, LINE_LENGTH);
    memset(gname, 0, sizeof(gname));
    strncpy(gname, group_name, LINE_LENGTH);
        
    setpwent();
    pwd = getpwnam(uname);
    endpwent();
    if (!pwd)
        return 0;

    /* the info about this group */
    setgrent();
    grp = getgrnam(gname);
    endgrent();
    if (!grp)
        return 0;
    
    /* first check: is a member of the group_name group ? */
    if (is_on_list(grp->gr_mem, uname))
        return 1;

    /* next check: user primary group is group_name ? */
    setgrent();
    pgrp = getgrgid(pwd->pw_gid);
    endgrent();
    if (!pgrp)
        return 0;
    if (!strcmp(pgrp->gr_name, gname))
        return 1;
        
    return 0;
}
    
static int init_limits(struct pam_limit_s *pl)
{
    int i;
    int retval = PAM_SUCCESS;

    D(("called."));

    for(i = 0; i < RLIM_NLIMITS; i++) 
	retval |= getrlimit(i, &pl->limits[i].limit);
    
    pl->limits[RLIMIT_CPU].src_soft = LIMITS_DEF_NONE;
    pl->limits[RLIMIT_CPU].src_hard = LIMITS_DEF_NONE;

    pl->limits[RLIMIT_FSIZE].src_soft = LIMITS_DEF_NONE;
    pl->limits[RLIMIT_FSIZE].src_hard = LIMITS_DEF_NONE;

    pl->limits[RLIMIT_DATA].src_soft = LIMITS_DEF_NONE;
    pl->limits[RLIMIT_DATA].src_hard = LIMITS_DEF_NONE;

    pl->limits[RLIMIT_STACK].src_soft = LIMITS_DEF_NONE;
    pl->limits[RLIMIT_STACK].src_hard = LIMITS_DEF_NONE;

    pl->limits[RLIMIT_CORE].src_soft = LIMITS_DEF_NONE;
    pl->limits[RLIMIT_CORE].src_hard = LIMITS_DEF_NONE;

    pl->limits[RLIMIT_RSS].src_soft = LIMITS_DEF_NONE;
    pl->limits[RLIMIT_RSS].src_hard = LIMITS_DEF_NONE;

    pl->limits[RLIMIT_NPROC].src_soft = LIMITS_DEF_NONE;
    pl->limits[RLIMIT_NPROC].src_hard = LIMITS_DEF_NONE;

    pl->limits[RLIMIT_NOFILE].src_soft = LIMITS_DEF_NONE;
    pl->limits[RLIMIT_NOFILE].src_hard = LIMITS_DEF_NONE;

    pl->limits[RLIMIT_MEMLOCK].src_soft = LIMITS_DEF_NONE;
    pl->limits[RLIMIT_MEMLOCK].src_hard = LIMITS_DEF_NONE;

    pl->limits[RLIMIT_AS].src_soft = LIMITS_DEF_NONE;
    pl->limits[RLIMIT_AS].src_hard = LIMITS_DEF_NONE;

    pl->priority = 0;
    pl->login_limit = -2;
    pl->login_limit_def = LIMITS_DEF_NONE;

    return retval;
}    

static void process_limit(int source, const char *lim_type,
			  const char *lim_item, const char *lim_value,
			  int ctrl, struct pam_limit_s *pl)
{
    int limit_item;
    int limit_type = 0;
    long limit_value;
    const char **endptr = &lim_value;
    const char *value_orig = lim_value;
        
    if (ctrl & PAM_DEBUG_ARG)
	 _pam_log(LOG_DEBUG, "%s: processing(%d) %s %s %s\n",
		  __FUNCTION__,source,lim_type,lim_item,lim_value);

    if (strcmp(lim_item, "cpu") == 0)
        limit_item = RLIMIT_CPU;
    else if (strcmp(lim_item, "fsize") == 0)
        limit_item = RLIMIT_FSIZE;
    else if (strcmp(lim_item, "data") == 0)
	limit_item = RLIMIT_DATA;
    else if (strcmp(lim_item, "stack") == 0)
	limit_item = RLIMIT_STACK;
    else if (strcmp(lim_item, "core") == 0)
	limit_item = RLIMIT_CORE;
    else if (strcmp(lim_item, "rss") == 0)
	limit_item = RLIMIT_RSS;
    else if (strcmp(lim_item, "nproc") == 0)
	limit_item = RLIMIT_NPROC;
    else if (strcmp(lim_item, "nofile") == 0)
	limit_item = RLIMIT_NOFILE;
    else if (strcmp(lim_item, "memlock") == 0)
	limit_item = RLIMIT_MEMLOCK;
    else if (strcmp(lim_item, "as") == 0)
	limit_item = RLIMIT_AS;
    else if (strcmp(lim_item, "maxlogins") == 0) {
	limit_item = LIMIT_LOGIN;
	pl->flag_numsyslogins = 0;
    } else if (strcmp(lim_item, "maxsyslogins") == 0) {
	limit_item = LIMIT_NUMSYSLOGINS;
	pl->flag_numsyslogins = 1;
    } else if (strcmp(lim_item, "priority") == 0) {
	limit_item = LIMIT_PRI;
    } else {
        _pam_log(LOG_DEBUG,"unknown limit item '%s'", lim_item);
        return;
    }

    if (strcmp(lim_type,"soft")==0)
	limit_type=LIMIT_SOFT;
    else if (strcmp(lim_type, "hard")==0)
	limit_type=LIMIT_HARD;
    else if (strcmp(lim_type,"-")==0)
	limit_type=LIMIT_SOFT | LIMIT_HARD;
    else if (limit_item != LIMIT_LOGIN && limit_item != LIMIT_NUMSYSLOGINS) {
        _pam_log(LOG_DEBUG,"unknown limit type '%s'", lim_type);
        return;
    }
    
    limit_value = strtol(lim_value, endptr, 10);
    if (limit_value == 0 && value_orig == *endptr) { /* no chars read */
        if (strcmp(lim_value,"-") != 0) {
            _pam_log(LOG_DEBUG,"wrong limit value '%s'", lim_value);
            return;
        } else
            if (limit_item != LIMIT_LOGIN) {
                if (ctrl & PAM_DEBUG_ARG)
                    _pam_log(LOG_DEBUG,
                            "'-' limit value valid for maxlogins type only");
                return;
            } else
                limit_value = -1;
    }
    
    switch(limit_item) {
        case RLIMIT_CPU:
            limit_value *= 60;
            break;
        case RLIMIT_FSIZE:
        case RLIMIT_DATA:
        case RLIMIT_STACK:
        case RLIMIT_CORE:
        case RLIMIT_RSS:
        case RLIMIT_MEMLOCK:
        case RLIMIT_AS:
            limit_value *= 1024;
            break;
    }
    
    if (limit_item != LIMIT_LOGIN && limit_item != LIMIT_NUMSYSLOGINS 
		    && limit_item != LIMIT_PRI
		    ) {
        if (limit_type & LIMIT_SOFT) {
	    if (pl->limits[limit_item].src_soft < source) {
                return;
	    } else {
                pl->limits[limit_item].limit.rlim_cur = limit_value;
                pl->limits[limit_item].src_soft = source;
            }
	}
        if (limit_type & LIMIT_HARD) {
	    if (pl->limits[limit_item].src_hard < source) {
                return;
            } else {
                pl->limits[limit_item].limit.rlim_max = limit_value;
                pl->limits[limit_item].src_hard = source;
            }
	}
    } else 
	if (limit_item == LIMIT_PRI) {
		/* additional check */
		pl->priority = ((limit_value>0)?limit_value:0);
	} else {
	        if (pl->login_limit_def < source) {
	            return;
	        } else {
	            pl->login_limit = limit_value;
	            pl->login_limit_def = source;
        	}
	}
    return;
}

static int parse_config_file(const char *uname, int ctrl,
			     struct pam_limit_s *pl)
{
    FILE *fil;
    char buf[LINE_LENGTH];
    
#define CONF_FILE (pl->conf_file[0])?pl->conf_file:LIMITS_FILE
    /* check for the LIMITS_FILE */
    if (ctrl & PAM_DEBUG_ARG)
        _pam_log(LOG_DEBUG,"reading settings from '%s'", CONF_FILE);
    fil = fopen(CONF_FILE, "r");
    if (fil == NULL) {
        _pam_log (LOG_WARNING, "can not read settings from %s", CONF_FILE);
        return PAM_SERVICE_ERR;
    }
#undef CONF_FILE
    
    /* init things */
    memset(buf, 0, sizeof(buf));
    /* start the show */
    while (fgets(buf, LINE_LENGTH, fil) != NULL) {
        char domain[LINE_LENGTH];
        char ltype[LINE_LENGTH];
        char item[LINE_LENGTH];
        char value[LINE_LENGTH];
        int i,j;
        char *tptr;
        
        tptr = buf;
        /* skip the leading white space */
        while (*tptr && isspace(*tptr))
            tptr++;
        strcpy(buf, (const char *)tptr);
                                
        /* Rip off the comments */
        tptr = strchr(buf,'#');
        if (tptr)
            *tptr = '\0';
        /* Rip off the newline char */
        tptr = strchr(buf,'\n');
        if (tptr)
            *tptr = '\0';
        /* Anything left ? */
        if (!strlen(buf)) {
            memset(buf, 0, sizeof(buf));
            continue;
        }

        memset(domain, 0, sizeof(domain));
        memset(ltype, 0, sizeof(ltype));
        memset(item, 0, sizeof(item));
        memset(value, 0, sizeof(value));
        
        i = sscanf(buf,"%s%s%s%s", domain, ltype, item, value);
	D(("scanned line[%d]: domain[%s], ltype[%s], item[%s], value[%s]",
	   i, domain, ltype, item, value));

        for(j=0; j < strlen(domain); j++)
            domain[j]=tolower(domain[j]);
        for(j=0; j < strlen(ltype); j++)
            ltype[j]=tolower(ltype[j]);
        for(j=0; j < strlen(item); j++)
            item[j]=tolower(item[j]);
        for(j=0; j < strlen(value); j++)
            value[j]=tolower(value[j]);

        if (i == 4) { /* a complete line */
            if (strcmp(uname, domain) == 0) /* this user have a limit */
                process_limit(LIMITS_DEF_USER, ltype, item, value, ctrl, pl);
            else if (domain[0]=='@') {
                if (is_on_group(uname, domain+1))
                    process_limit(LIMITS_DEF_GROUP, ltype, item, value, ctrl,
				  pl);
            } else if (strcmp(domain, "*") == 0)
                process_limit(LIMITS_DEF_DEFAULT, ltype, item, value, ctrl,
			      pl);
	} else if (i == 2 && ltype[0] == '-') { /* Probably a no-limit line */
	    if (strcmp(uname, domain) == 0) {
		_pam_log(LOG_DEBUG, "no limits for '%s'", uname);
		fclose(fil);
		return PAM_IGNORE;
	    } else if (domain[0] == '@' && is_on_group(uname, domain+1)) {
		_pam_log(LOG_DEBUG, "no limits for '%s' in group '%s'",
			 uname, domain+1);
		fclose(fil);
		return PAM_IGNORE;
	    }
        } else {
            _pam_log(LOG_DEBUG,"invalid line '%s' - skipped", buf);
	}
    }
    fclose(fil);
    return PAM_SUCCESS;    
}

static int setup_limits(const char * uname, int ctrl, struct pam_limit_s *pl)
{
    int i;
    int retval = PAM_SUCCESS;
    
    for (i=0; i<RLIM_NLIMITS; i++) {
        if (pl->limits[i].limit.rlim_cur > pl->limits[i].limit.rlim_max)
            pl->limits[i].limit.rlim_cur = pl->limits[i].limit.rlim_max;
        retval |= setrlimit(i, &pl->limits[i].limit);
    }
    
    if (retval != PAM_SUCCESS)
        retval = LIMIT_ERR;

    retval=setpriority(PRIO_PROCESS, 0, pl->priority);
    
    if (retval != PAM_SUCCESS)
        retval = LIMIT_ERR;

    if (pl->login_limit > 0) {
        if (check_logins(uname, pl->login_limit, ctrl, pl) == LOGIN_ERR)
            retval |= LOGIN_ERR;
    } else if (pl->login_limit == 0)
        retval |= LOGIN_ERR;
    return retval;
}
            
/* now the session stuff */
PAM_EXTERN int pam_sm_open_session(pam_handle_t *pamh, int flags,
                                   int argc, const char **argv)
{
    int retval;
    char *user_name;
    struct passwd *pwd;
    int ctrl;
    struct pam_limit_s pl;

    D(("called."));

    memset(&pl, 0, sizeof(pl));

    ctrl = _pam_parse(argc, argv, &pl);
    retval = pam_get_item( pamh, PAM_USER, (void*) &user_name );
    if ( user_name == NULL || retval != PAM_SUCCESS ) {
        _pam_log(LOG_CRIT, "open_session - error recovering username");
        return PAM_SESSION_ERR;
     }
	
    setpwent();
    pwd = getpwnam(user_name);
    endpwent();
    if (!pwd) {
        if (ctrl & PAM_DEBUG_ARG)
            _pam_log(LOG_WARNING, "open_session username '%s' does not exist",
                                   user_name);
        return PAM_SESSION_ERR;
    }
                     
    /* do not impose limits on UID 0 accounts */
    if (!pwd->pw_uid) {
        if (ctrl & PAM_DEBUG_ARG)
            _pam_log(LOG_DEBUG, "user '%s' have UID 0 - no limits imposed",
                                user_name);
        return PAM_SUCCESS;
    }
        
    retval = init_limits(&pl);
    if (retval != PAM_SUCCESS) {
        _pam_log(LOG_WARNING, "cannot initialize");
        return PAM_IGNORE;
    }

    retval = parse_config_file(pwd->pw_name, ctrl, &pl);
    if (retval == PAM_IGNORE) {
	D(("the configuration file has an applicable '<domain> -' entry"));
	return PAM_SUCCESS;
    }
    if (retval != PAM_SUCCESS) {
        _pam_log(LOG_WARNING, "error parsing the configuration file");
        return PAM_IGNORE;
    }

    if (ctrl & PAM_DO_SETREUID)
	setreuid(pwd->pw_uid, -1);
    retval = setup_limits(pwd->pw_name, ctrl, &pl);
    if (retval & LOGIN_ERR) {
        printf("\nToo many logins for '%s'\n",pwd->pw_name);
        sleep(2);
        return PAM_PERM_DENIED;
    }

    return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_close_session(pam_handle_t *pamh, int flags,
                                    int argc, const char **argv)
{
     /* nothing to do */
     return PAM_SUCCESS;
}

#ifdef PAM_STATIC

/* static module data */

struct pam_module _pam_limits_modstruct = {
     "pam_limits",
     NULL,
     NULL,
     NULL,
     pam_sm_open_session,
     pam_sm_close_session,
     NULL
};
#endif

/*
 * Copyright (c) Cristian Gafton, 1996-1997, <gafton@redhat.com>
 *                                              All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 * 
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * $Id: pam_listfile.c,v 1.2 2000/11/19 23:54:04 agmorgan Exp $
 *
 */

/*
 * by Elliot Lee <sopwith@redhat.com>, Red Hat Software. July 25, 1996.
 * log refused access error christopher mccrory <chrismcc@netus.com> 1998/7/11
 *
 * This code began life as the pam_rootok module.
 */

#include <security/_pam_aconf.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <syslog.h>
#include <stdarg.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>

#ifdef DEBUG
#include <assert.h>
#endif

/*
 * here, we make a definition for the externally accessible function
 * in this file (this definition is required for static a module
 * but strongly encouraged generally) it is used to instruct the
 * modules include file to define the function prototypes.
 */

#define PAM_SM_AUTH

#include <security/pam_modules.h>
#include <security/_pam_macros.h>

/* some syslogging */

#define LOCAL_LOG_PREFIX "PAM-listfile: "

static void _pam_log(int err, const char *format, ...)
{
    va_list args;
 
    va_start(args, format);
    vsyslog(LOG_AUTH | err, format, args);
    va_end(args);
}

/* checks if a user is on a list of members */
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
    char uname[BUFSIZ], gname[BUFSIZ];
    
    if (!strlen(user_name))
        return 0;
    if (!strlen(group_name))
        return 0;
    bzero(uname, sizeof(uname));
    strncpy(uname, user_name, BUFSIZ-1);
    bzero(gname, sizeof(gname));
    strncpy(gname, group_name, BUFSIZ-1);
        
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

/* --- authentication management functions (only) --- */

/* Extended Items that are not directly available via pam_get_item() */
#define EI_GROUP (1 << 0)
#define EI_SHELL (1 << 1)

/* Constants for apply= parameter */
#define APPLY_TYPE_NULL		0
#define APPLY_TYPE_NONE		1
#define APPLY_TYPE_USER		2
#define APPLY_TYPE_GROUP	3

PAM_EXTERN
int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    int retval, i, citem=0, extitem=0, onerr=PAM_SERVICE_ERR, sense=2;
    const char *citemp;
    char *ifname=NULL;
    char aline[256];
    char mybuf[256],myval[256];
    struct stat fileinfo;
    FILE *inf;
    char apply_val[256];
    int apply_type;

    /* Stuff for "extended" items */
    struct passwd *userinfo;
    struct group *grpinfo;
    char *itemlist[256]; /* Maximum of 256 items */

    D(("called."));

    apply_type=APPLY_TYPE_NULL;
    memset(apply_val,0,sizeof(apply_val));

    for(i=0; i < argc; i++) {
	{
	    char *junk;
	    junk = (char *) malloc(strlen(argv[i])+1);
	    if (junk == NULL) {
		return PAM_BUF_ERR;
	    }
	    strcpy(junk,argv[i]);
	    strncpy(mybuf,strtok(junk,"="),255);
	    strncpy(myval,strtok(NULL,"="),255);
	    free(junk);
	}
	if(!strcmp(mybuf,"onerr"))
	    if(!strcmp(myval,"succeed"))
		onerr = PAM_SUCCESS;
	    else if(!strcmp(myval,"fail"))
		onerr = PAM_SERVICE_ERR;
	    else
		return PAM_SERVICE_ERR;
	else if(!strcmp(mybuf,"sense"))
	    if(!strcmp(myval,"allow"))
		sense=0;
	    else if(!strcmp(myval,"deny"))
		sense=1;
	    else
		return onerr;
	else if(!strcmp(mybuf,"file")) {
	    ifname = (char *)malloc(strlen(myval)+1);
	    strcpy(ifname,myval);
	} else if(!strcmp(mybuf,"item"))
	    if(!strcmp(myval,"user"))
		citem = PAM_USER;
	    else if(!strcmp(myval,"tty"))
		citem = PAM_TTY;
	    else if(!strcmp(myval,"rhost"))
		citem = PAM_RHOST;
	    else if(!strcmp(myval,"ruser"))
		citem = PAM_RUSER;
	    else { /* These items are related to the user, but are not
		      directly gettable with pam_get_item */
		citem = PAM_USER;
		if(!strcmp(myval,"group"))
		    extitem = EI_GROUP;
		else if(!strcmp(myval,"shell"))
		    extitem = EI_SHELL;
		else
		    citem = 0;
	    } else if(!strcmp(mybuf,"apply")) {
		apply_type=APPLY_TYPE_NONE;
		if (myval[0]=='@') {
		    apply_type=APPLY_TYPE_GROUP;
		    strncpy(apply_val,myval+1,sizeof(apply_val)-1);
		} else {
		    apply_type=APPLY_TYPE_USER;
		    strncpy(apply_val,myval,sizeof(apply_val)-1);
		}
	    } else {
		_pam_log(LOG_ERR,LOCAL_LOG_PREFIX "Unknown option: %s",mybuf);
		return onerr;
	    }
    }

    if(!citem) {
	_pam_log(LOG_ERR,
		 LOCAL_LOG_PREFIX "Unknown item or item not specified");
	return onerr;
    } else if(!ifname) {
	_pam_log(LOG_ERR,LOCAL_LOG_PREFIX "List filename not specified");
	return onerr;
    } else if(sense == 2) {
	_pam_log(LOG_ERR,
		 LOCAL_LOG_PREFIX "Unknown sense or sense not specified");
	return onerr;
    } else if(
	      (apply_type==APPLY_TYPE_NONE) || 
	      ((apply_type!=APPLY_TYPE_NULL) && (*apply_val=='\0'))
              ) {
	_pam_log(LOG_ERR,
		 LOCAL_LOG_PREFIX "Invalid usage for apply= parameter");
	return onerr;
    }
     
    /* Check if it makes sense to use the apply= parameter */
    if (apply_type != APPLY_TYPE_NULL) {
	if((citem==PAM_USER) || (citem==PAM_RUSER)) {
	    _pam_log(LOG_WARNING,
		     LOCAL_LOG_PREFIX "Non-sense use for apply= parameter");
	    apply_type=APPLY_TYPE_NULL;
	}
	if(extitem && (extitem==EI_GROUP)) {
	    _pam_log(LOG_WARNING,
		     LOCAL_LOG_PREFIX "Non-sense use for apply= parameter");
	    apply_type=APPLY_TYPE_NULL;
	}
    }
     
    /* Short-circuit - test if this session apply for this user */
    {
	const char *user_name;
	int rval;
       
	rval=pam_get_user(pamh,&user_name,NULL);
	if((rval==PAM_SUCCESS) && user_name[0]) {
	    /* Got it ? Valid ? */
	    if(apply_type==APPLY_TYPE_USER) {
		if(strcmp(user_name, apply_val)) {
		    /* Does not apply to this user */
#ifdef DEBUG
		    _pam_log(LOG_DEBUG,
			     LOCAL_LOG_PREFIX "don't apply: apply=%s, user=%s",
			     apply_val,user_name);
#endif /* DEBUG */
		    return PAM_IGNORE;
		}
	    } else if(apply_type==APPLY_TYPE_GROUP) {
		if(!is_on_group(user_name,apply_val)) {
		    /* Not a member of apply= group */
#ifdef DEBUG
		    _pam_log(LOG_DEBUG,
			     LOCAL_LOG_PREFIX
			     "don't apply: %s not a member of group %s",
			     user_name,apply_val);
#endif /* DEBUG */
		    return PAM_IGNORE;
		}
	    }
	}
    }

    retval = pam_get_item(pamh,citem,(const void **)&citemp);
    if(retval != PAM_SUCCESS) {
	return onerr;
    }
    if((citem == PAM_USER) && !citemp) {
	pam_get_user(pamh,&citemp,NULL);
	if (retval != PAM_SUCCESS)
	    return PAM_SERVICE_ERR;
    }

    if(!citemp || (strlen(citemp) <= 0)) {
	/* The item was NULL - we are sure not to match */
	return sense?PAM_SUCCESS:PAM_AUTH_ERR;
    }

    if(extitem) {
	switch(extitem) {
	    case EI_GROUP:
		setpwent();
		userinfo = getpwnam(citemp);
		setgrent();
		grpinfo = getgrgid(userinfo->pw_gid);
		itemlist[0] = x_strdup(grpinfo->gr_name);
		setgrent();
		for (i=1; (i < sizeof(itemlist)/sizeof(itemlist[0])-1) &&
			 (grpinfo = getgrent()); ) {
		    if (is_on_list(grpinfo->gr_mem,citemp)) {
			itemlist[i++] = x_strdup(grpinfo->gr_name);
		    }
                }
		itemlist[i] = NULL;
		endgrent();
		endpwent();
		break;
	    case EI_SHELL:
		setpwent();
		userinfo = getpwnam(citemp); /* Assume that we have already gotten
						PAM_USER in pam_get_item() - a valid
						assumption since citem gets set to
						PAM_USER in the extitem switch */
		citemp = userinfo->pw_shell;
		endpwent();
		break;
	    default:
		_pam_log(LOG_ERR,
			 LOCAL_LOG_PREFIX
			 "Internal weirdness, unknown extended item %d",
			 extitem);
		return onerr;
	}
    }
#ifdef DEBUG
    _pam_log(LOG_INFO,
	     LOCAL_LOG_PREFIX
	     "Got file = %s, item = %d, value = %s, sense = %d",
	     ifname, citem, citemp, sense);
#endif
    if(lstat(ifname,&fileinfo)) {
	_pam_log(LOG_ERR,LOCAL_LOG_PREFIX "Couldn't open %s",ifname);
	return onerr;
    }

    if((fileinfo.st_mode & S_IWOTH)
       || !S_ISREG(fileinfo.st_mode)) {
	/* If the file is world writable or is not a
	   normal file, return error */
	_pam_log(LOG_ERR,LOCAL_LOG_PREFIX 
		 "%s is either world writable or not a normal file",
		 ifname);
	return PAM_AUTH_ERR;
    }

    inf = fopen(ifname,"r");
    if(inf == NULL) { /* Check that we opened it successfully */
	if (onerr == PAM_SERVICE_ERR) {
	    /* Only report if it's an error... */
	    _pam_log(LOG_ERR,LOCAL_LOG_PREFIX  "Error opening %s", ifname);
	}
	return onerr;
    }
    /* There should be no more errors from here on */
    retval=PAM_AUTH_ERR;
    /* This loop assumes that PAM_SUCCESS == 0
       and PAM_AUTH_ERR != 0 */
#ifdef DEBUG
    assert(PAM_SUCCESS == 0);
    assert(PAM_AUTH_ERR != 0);
#endif
    if(extitem == EI_GROUP) {
	while((fgets(aline,255,inf) != NULL)
	      && retval) {
	    if(aline[strlen(aline) - 1] == '\n')
		aline[strlen(aline) - 1] = '\0';
	    for(i=0;itemlist[i];)
		/* If any of the items match, strcmp() == 0, and we get out
		   of this loop */
		retval = (strcmp(aline,itemlist[i++]) && retval);
	}
	for(i=0;itemlist[i];)
	    free(itemlist[i++]);
    } else {
	while((fgets(aline,255,inf) != NULL)
	      && retval) {
	    if(aline[strlen(aline) - 1] == '\n')
		aline[strlen(aline) - 1] = '\0';
	    retval = strcmp(aline,citemp);
	}
    }
    fclose(inf);
    free(ifname);
    if ((sense && retval) || (!sense && !retval)) {
#ifdef DEBUG
	_pam_log(LOG_INFO, LOCAL_LOG_PREFIX
		 "Returning PAM_SUCCESS, retval = %d", retval);
#endif
	return PAM_SUCCESS;
    }
    else {
	const char *service, *user_name;
#ifdef DEBUG
	_pam_log(LOG_INFO,LOCAL_LOG_PREFIX
		 "Returning PAM_AUTH_ERR, retval = %d", retval);
#endif
	(void) pam_get_item(pamh, PAM_SERVICE, (const void **)&service);
	(void) pam_get_user(pamh, &user_name, NULL);
	_pam_log(LOG_ALERT,LOCAL_LOG_PREFIX "Refused user %s for service %s",
		 user_name, service);
	return PAM_AUTH_ERR;
    }
}

PAM_EXTERN
int pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    return PAM_SUCCESS;
}

#ifdef PAM_STATIC

/* static module data */

struct pam_module _pam_listfile_modstruct = {
    "pam_listfile",
    pam_sm_authenticate,
    pam_sm_setcred,
    NULL,
    NULL,
    NULL,
    NULL,
};

#endif

/* end of module definition */


/*
 * pam_tally.c
 * 
 * $Id: pam_tally.c,v 1.5 2001/01/20 22:21:22 agmorgan Exp $
 */


/* By Tim Baverstock <warwick@mmm.co.uk>, Multi Media Machine Ltd.
 * 5 March 1997
 *
 * Stuff stolen from pam_rootok and pam_listfile
 */

#include <security/_pam_aconf.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <syslog.h>
#include <pwd.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include "faillog.h"

#ifndef TRUE
#define TRUE  1L
#define FALSE 0L
#endif

/*
 * here, we make a definition for the externally accessible function
 * in this file (this definition is required for static a module
 * but strongly encouraged generally) it is used to instruct the
 * modules include file to define the function prototypes.
 */

#define PAM_SM_AUTH
#define PAM_SM_ACCOUNT
/* #define PAM_SM_SESSION  */
/* #define PAM_SM_PASSWORD */

#include <security/pam_modules.h>

/*---------------------------------------------------------------------*/

#define DEFAULT_LOGFILE "/var/log/faillog"
#define MODULE_NAME     "pam_tally"

enum TALLY_RESET {
  TALLY_RESET_DEFAULT,
  TALLY_RESET_RESET,
  TALLY_RESET_NO_RESET
};

#define tally_t    unsigned short int
#define TALLY_FMT  "%hu"
#define TALLY_HI   ((tally_t)~0L)

#define UID_FMT    "%hu"

#ifndef FILENAME_MAX
# define FILENAME_MAX MAXPATHLEN
#endif

struct fail_s {
    struct faillog fs_faillog;
#ifndef MAIN
    time_t fs_fail_time;
#endif /* ndef MAIN */
};

/*---------------------------------------------------------------------*/

/* some syslogging */

static void _pam_log(int err, const char *format, ...)
{
    va_list args;
    va_start(args, format);

#ifdef MAIN
    vfprintf(stderr,format,args);
#else
    openlog(MODULE_NAME, LOG_CONS|LOG_PID, LOG_AUTH);
    vsyslog(err, format, args);
    closelog();
#endif
    va_end(args);
}

/*---------------------------------------------------------------------*/

/* --- Support function: get uid (and optionally username) from PAM or
        cline_user --- */

#ifdef MAIN
static char *cline_user=0;  /* cline_user is used in the administration prog */
#endif

static int pam_get_uid( pam_handle_t *pamh, uid_t *uid, const char **userp ) 
  {
    const char *user;
    struct passwd *pw;

#ifdef MAIN
    user = cline_user;
#else
    pam_get_user( pamh, &user, NULL );
#endif

    if ( !user || !*user ) {
      _pam_log(LOG_ERR, MODULE_NAME ": pam_get_uid; user?");
      return PAM_AUTH_ERR;
    }

    if ( ! ( pw = getpwnam( user ) ) ) {
      _pam_log(LOG_ERR,MODULE_NAME ": pam_get_uid; no such user %s",user);
      return PAM_USER_UNKNOWN;
    }
    
    if ( uid )   *uid   = pw->pw_uid;
    if ( userp ) *userp = user;
    return PAM_SUCCESS;
  }

/*---------------------------------------------------------------------*/

/* --- Support function: open/create tallyfile and return tally for uid --- */

/* If on entry *tally==TALLY_HI, tallyfile is opened READONLY */
/* Otherwise, if on entry tallyfile doesn't exist, creation is attempted. */

static int get_tally( tally_t *tally, 
                              uid_t uid, 
                              const char *filename, 
                              FILE **TALLY,
		              struct fail_s *fsp) 
  {
    struct stat fileinfo;
    int lstat_ret = lstat(filename,&fileinfo);

    if ( lstat_ret && *tally!=TALLY_HI ) {
      int oldmask = umask(077);
      *TALLY=fopen(filename, "a");
      /* Create file, or append-open in pathological case. */
      umask(oldmask);
      if ( !*TALLY ) {
        _pam_log(LOG_ALERT, "Couldn't create %s",filename);
        return PAM_AUTH_ERR;
      }
      lstat_ret = fstat(fileno(*TALLY),&fileinfo);
      fclose(*TALLY);
    }

    if ( lstat_ret ) {
      _pam_log(LOG_ALERT, "Couldn't stat %s",filename);
      return PAM_AUTH_ERR;
    }

    if((fileinfo.st_mode & S_IWOTH) || !S_ISREG(fileinfo.st_mode)) {
      /* If the file is world writable or is not a
         normal file, return error */
      _pam_log(LOG_ALERT,
               "%s is either world writable or not a normal file",
               filename);
      return PAM_AUTH_ERR;
    }

    if ( ! ( *TALLY = fopen(filename,(*tally!=TALLY_HI)?"r+":"r") ) ) {
      _pam_log(LOG_ALERT, "Error opening %s for update", filename);

/* Discovering why account service fails: e/uid are target user.
 *
 *      perror(MODULE_NAME);
 *      fprintf(stderr,"uid %d euid %d\n",getuid(), geteuid());
 */
      return PAM_AUTH_ERR;
    }

    if ( fseek( *TALLY, uid * sizeof(struct faillog), SEEK_SET ) ) {
          _pam_log(LOG_ALERT, "fseek failed %s", filename);
                return PAM_AUTH_ERR;
     }
                    

    if (( fread((char *) &fsp->fs_faillog,
		sizeof(struct faillog), 1, *TALLY) )==0 ) {
          *tally=0; /* Assuming a gappy filesystem */
    }
    *tally = fsp->fs_faillog.fail_cnt;    
              
    return PAM_SUCCESS;
  }

/*---------------------------------------------------------------------*/

/* --- Support function: update and close tallyfile with tally!=TALLY_HI --- */

static int set_tally( tally_t tally, 
                              uid_t uid,
                              const char *filename, 
                              FILE **TALLY,
		              struct fail_s *fsp) 
  {
    if ( tally!=TALLY_HI ) 
      {
        if ( fseek( *TALLY, uid * sizeof(struct faillog), SEEK_SET ) ) {
                  _pam_log(LOG_ALERT, "fseek failed %s", filename);
                            return PAM_AUTH_ERR;
        }
        fsp->fs_faillog.fail_cnt = tally;                                    
        if (fwrite((char *) &fsp->fs_faillog,
		   sizeof(struct faillog), 1, *TALLY)==0 ) {
          _pam_log(LOG_ALERT, "tally update (fputc) failed.", filename);
          return PAM_AUTH_ERR;
        }
      }
    
    if ( fclose(*TALLY) ) {
      _pam_log(LOG_ALERT, "tally update (fclose) failed.", filename);
      return PAM_AUTH_ERR;
    }
    *TALLY=NULL;
    return PAM_SUCCESS;
  }

/*---------------------------------------------------------------------*/

/* --- PAM bits --- */

#ifndef MAIN

#define PAM_FUNCTION(name) \
 PAM_EXTERN int name (pam_handle_t *pamh,int flags,int argc,const char **argv)

#define RETURN_ERROR(i) return ((fail_on_error)?(i):(PAM_SUCCESS))

/*---------------------------------------------------------------------*/

/* --- tally bump function: bump tally for uid by (signed) inc --- */

static int tally_bump (int inc,
                           pam_handle_t *pamh,
                           int flags,
                           int argc,
                           const char **argv) {
  uid_t uid;

  int 
    fail_on_error = FALSE;
  tally_t
    tally         = 0;  /* !TALLY_HI --> Log opened for update */

  char
    no_magic_root          = FALSE;

  char 
    filename[ FILENAME_MAX ] = DEFAULT_LOGFILE;

  /* Should probably decode the parameters before anything else. */

  {
    for ( ; argc-- > 0; ++argv ) {

      /* generic options.. um, ignored. :] */
      
      if ( ! strcmp( *argv, "no_magic_root" ) ) {
        no_magic_root = TRUE;
      }
      else if ( ! strncmp( *argv, "file=", 5 ) ) {
        char const 
          *from = (*argv)+5;
        char
          *to   = filename;
        if ( *from!='/' || strlen(from)>FILENAME_MAX-1 ) {
          _pam_log(LOG_ERR,
                   MODULE_NAME ": filename not /rooted or too long; ",
                   *argv);
          RETURN_ERROR( PAM_AUTH_ERR );
        }
        while ( ( *to++ = *from++ ) );        
      }
      else if ( ! strcmp( *argv, "onerr=fail" ) ) {
        fail_on_error=TRUE;
      }
      else if ( ! strcmp( *argv, "onerr=succeed" ) ) {
        fail_on_error=FALSE;
      }
      else {
        _pam_log(LOG_ERR, MODULE_NAME ": unknown option; %s",*argv);
      }
    } /* for() */
  }

  {
    FILE
      *TALLY = NULL;
    const char
      *user  = NULL,
      *remote_host = NULL,
      *cur_tty = NULL;
    struct fail_s fs, *fsp = &fs;

    int i=pam_get_uid(pamh, &uid, &user);
    if ( i != PAM_SUCCESS ) RETURN_ERROR( i );

    i=get_tally( &tally, uid, filename, &TALLY, fsp );

    /* to remember old fail time (for locktime) */
    fsp->fs_fail_time = fsp->fs_faillog.fail_time;
    fsp->fs_faillog.fail_time = (time_t) time( (time_t *) 0);
    (void) pam_get_item(pamh, PAM_RHOST, (const void **)&remote_host);
    if (!remote_host)
    {
    	(void) pam_get_item(pamh, PAM_TTY, (const void **)&cur_tty);
	if (!cur_tty)
    	    strcpy(fsp->fs_faillog.fail_line, "unknown");
	else {
    	    strncpy(fsp->fs_faillog.fail_line, cur_tty,
		    (size_t)sizeof(fsp->fs_faillog.fail_line));
	    fsp->fs_faillog.fail_line[sizeof(fsp->fs_faillog.fail_line)-1] = 0;
	}
    }
    else
    {
    	strncpy(fsp->fs_faillog.fail_line, remote_host,
		(size_t)sizeof(fsp->fs_faillog.fail_line));
	fsp->fs_faillog.fail_line[sizeof(fsp->fs_faillog.fail_line)-1] = 0;
    }
    if ( i != PAM_SUCCESS ) { if (TALLY) fclose(TALLY); RETURN_ERROR( i ); }
    
    if ( no_magic_root || getuid() ) {       /* no_magic_root kills uid test */

      tally+=inc;
      
      if ( tally==TALLY_HI ) { /* Overflow *and* underflow. :) */
        tally-=inc;
        _pam_log(LOG_ALERT,"Tally %sflowed for user %s",
                 (inc<0)?"under":"over",user);
      }
    }
    
    i=set_tally( tally, uid, filename, &TALLY, fsp );
    if ( i != PAM_SUCCESS ) { if (TALLY) fclose(TALLY); RETURN_ERROR( i ); }
  }

  return PAM_SUCCESS;
} 

/*---------------------------------------------------------------------*/

/* --- authentication management functions (only) --- */

#ifdef PAM_SM_AUTH

PAM_FUNCTION( pam_sm_authenticate ) {
  return tally_bump( 1, pamh, flags, argc, argv);
}

/* --- Seems to need this function. Ho hum. --- */

PAM_FUNCTION( pam_sm_setcred ) { return PAM_SUCCESS; }

#endif

/*---------------------------------------------------------------------*/

/* --- session management functions (only) --- */

/*
 *  Unavailable until .so files can be suid
 */

#ifdef PAM_SM_SESSION

/* To maintain a balance-tally of successful login/outs */

PAM_FUNCTION( pam_sm_open_session ) {
  return tally_bump( 1, pamh, flags, argc, argv);
}

PAM_FUNCTION( pam_sm_close_session ) {
  return tally_bump(-1, pamh, flags, argc, argv);
}

#endif

/*---------------------------------------------------------------------*/

/* --- authentication management functions (only) --- */

#ifdef PAM_SM_AUTH

/* To lock out a user with an unacceptably high tally */

PAM_FUNCTION( pam_sm_acct_mgmt ) {
  uid_t 
    uid;

  int 
    fail_on_error = FALSE;
  tally_t
    deny          = 0;
  tally_t
    tally         = 0;  /* !TALLY_HI --> Log opened for update */

  char
    no_magic_root          = FALSE,
    even_deny_root_account = FALSE;
  char  per_user	    = FALSE;    /* if true then deny=.fail_max for user */
  char  no_lock_time	    = FALSE;	/* if true then don't use .fail_locktime */

  const char
    *user                  = NULL;

  enum TALLY_RESET 
    reset         = TALLY_RESET_DEFAULT;

  char 
    filename[ FILENAME_MAX ] = DEFAULT_LOGFILE;

  /* Should probably decode the parameters before anything else. */

  {
    for ( ; argc-- > 0; ++argv ) {

      /* generic options.. um, ignored. :] */
      
      if ( ! strcmp( *argv, "no_magic_root" ) ) {
        no_magic_root = TRUE;
      }
      else if ( ! strcmp( *argv, "even_deny_root_account" ) ) {
        even_deny_root_account = TRUE;
      }
      else if ( ! strcmp( *argv, "reset" ) ) {
        reset = TALLY_RESET_RESET;
      }
      else if ( ! strcmp( *argv, "no_reset" ) ) {
        reset = TALLY_RESET_NO_RESET;
      }
      else if ( ! strncmp( *argv, "file=", 5 ) ) {
        char const 
          *from = (*argv)+5;
        char
          *to   = filename;
        if ( *from != '/' || strlen(from) > FILENAME_MAX-1 ) {
          _pam_log(LOG_ERR,
                   MODULE_NAME ": filename not /rooted or too long; ",
                   *argv);
          RETURN_ERROR( PAM_AUTH_ERR );
        }
        while ( ( *to++ = *from++ ) );        
      }
      else if ( ! strncmp( *argv, "deny=", 5 ) ) {
        if ( sscanf((*argv)+5,TALLY_FMT,&deny) != 1 ) {
          _pam_log(LOG_ERR,"bad number supplied; %s",*argv);
          RETURN_ERROR( PAM_AUTH_ERR );
        }
      }
      else if ( ! strcmp( *argv, "onerr=fail" ) ) {
        fail_on_error=TRUE;
      }
      else if ( ! strcmp( *argv, "onerr=succeed" ) ) {
        fail_on_error=FALSE;
      }
      else if ( ! strcmp( *argv, "per_user" ) )
      {
      	per_user = TRUE;
      }
      else if ( ! strcmp( *argv, "no_lock_time") )
      {
      	no_lock_time = TRUE;
      }
      else {
        _pam_log(LOG_ERR, MODULE_NAME ": unknown option; %s",*argv);
      }
    } /* for() */
  }
  
  {
    struct fail_s fs, *fsp = &fs;
    FILE *TALLY=0;
    int i=pam_get_uid(pamh, &uid, &user);
    if ( i != PAM_SUCCESS ) RETURN_ERROR( i );
    
    i=get_tally( &tally, uid, filename, &TALLY, fsp );
    if ( i != PAM_SUCCESS ) { if (TALLY) fclose(TALLY); RETURN_ERROR( i ); }
    
    if ( no_magic_root || getuid() ) {       /* no_magic_root kills uid test */
      
      /* To deny or not to deny; that is the question */
      
      /* if there's .fail_max entry and per_user=TRUE then deny=.fail_max */
      
      if ( (fsp->fs_faillog.fail_max) && (per_user) ) {
	  deny = fsp->fs_faillog.fail_max;
      }
      if (fsp->fs_faillog.fail_locktime && fsp->fs_fail_time
	  && (!no_lock_time) )
      {
      	if ( (fsp->fs_faillog.fail_locktime + fsp->fs_fail_time)
	     > (time_t)time((time_t)0) )
      	{ 
      		_pam_log(LOG_NOTICE,
			 "user %s ("UID_FMT") has time limit [%lds left]"
			 " since last failure.",
			 user,uid,
			 fsp->fs_fail_time+fsp->fs_faillog.fail_locktime
			 -(time_t)time((time_t)0));
      		return PAM_AUTH_ERR;
      	}
      }
      if (
        ( deny != 0 ) &&                     /* deny==0 means no deny        */
        ( tally > deny ) &&                  /* tally>deny means exceeded    */
        ( even_deny_root_account || uid )    /* even_deny stops uid check    */
        ) {
        _pam_log(LOG_NOTICE,"user %s ("UID_FMT") tally "TALLY_FMT", deny "TALLY_FMT,
                 user, uid, tally, deny);
        return PAM_AUTH_ERR;                 /* Only unconditional failure   */
      }
      
      /* resets for explicit reset
       * or by default if deny exists and not magic-root
       */
      
      if ( ( reset == TALLY_RESET_RESET ) ||
           ( reset == TALLY_RESET_DEFAULT && deny ) ) { tally=0; }
    }
    else /* is magic root */ {
      
      /* Magic root skips deny test... */
      
      /* Magic root only resets on explicit reset, regardless of deny */
      
      if ( reset == TALLY_RESET_RESET ) { tally=0; }
    }
    if (tally == 0)
    {
    	fsp->fs_faillog.fail_time = (time_t) 0;
    	strcpy(fsp->fs_faillog.fail_line, "");	
    }
    i=set_tally( tally, uid, filename, &TALLY, fsp );
    if ( i != PAM_SUCCESS ) { if (TALLY) fclose(TALLY); RETURN_ERROR( i ); }
  }
  
  return PAM_SUCCESS;
} 

#endif  /* #ifdef PAM_SM_AUTH */

/*-----------------------------------------------------------------------*/

#ifdef PAM_STATIC

/* static module data */

struct pam_module _pam_tally_modstruct = {
     MODULE_NAME,
#ifdef PAM_SM_AUTH
     pam_sm_authenticate,
     pam_sm_setcred,
#else
     NULL,
     NULL,
#endif
#ifdef PAM_SM_ACCOUNT
     pam_sm_acct_mgmt,
#else
     NULL,
#endif
#ifdef PAM_SM_SESSION
     pam_sm_open_session,
     pam_sm_close_session,
#else
     NULL,
     NULL,
#endif
#ifdef PAM_SM_PASSWORD
     pam_sm_chauthtok,
#else
     NULL,
#endif
};

#endif   /* #ifdef PAM_STATIC */

/*-----------------------------------------------------------------------*/

#else   /* #ifndef MAIN */

static const char *cline_filename = DEFAULT_LOGFILE;
static tally_t cline_reset = TALLY_HI; /* Default is `interrogate only' */
static int cline_quiet =  0;

/*
 *  Not going to link with pamlib just for these.. :)
 */

static const char * pam_errors( int i ) {
  switch (i) {
  case PAM_AUTH_ERR:     return "Authentication error";
  case PAM_SERVICE_ERR:  return "Service error";
  case PAM_USER_UNKNOWN: return "Unknown user";
  default:               return "Unknown error";
  }
}

static int getopts( int argc, char **argv ) {
  const char *pname = *argv;
  for ( ; *argv ; (void)(*argv && ++argv) ) {
    if      ( !strcmp (*argv,"--file")    ) cline_filename=*++argv;
    else if ( !strncmp(*argv,"--file=",7) ) cline_filename=*argv+7;
    else if ( !strcmp (*argv,"--user")    ) cline_user=*++argv;
    else if ( !strncmp(*argv,"--user=",7) ) cline_user=*argv+7;
    else if ( !strcmp (*argv,"--reset")   ) cline_reset=0;
    else if ( !strncmp(*argv,"--reset=",8)) {
      if ( sscanf(*argv+8,TALLY_FMT,&cline_reset) != 1 )
        fprintf(stderr,"%s: Bad number given to --reset=\n",pname), exit(0);
    }
    else if ( !strcmp (*argv,"--quiet")   ) cline_quiet=1;
    else {
      fprintf(stderr,"%s: Unrecognised option %s\n",pname,*argv);
      return FALSE;
    }
  }
  return TRUE;
}

int main ( int argc, char **argv ) {

  struct fail_s fs, *fsp = &fs;

  if ( ! getopts( argc, argv+1 ) ) {
    printf("%s: [--file rooted-filename] [--user username] "
           "[--reset[=n]] [--quiet]\n",
           *argv);
    exit(0);
  }

  umask(077);

  /* 
   * Major difference between individual user and all users:
   *  --user just handles one user, just like PAM.
   *  --user=* handles all users, sniffing cline_filename for nonzeros
   */

  if ( cline_user ) {
    uid_t uid;
    tally_t tally=cline_reset;
    FILE *TALLY=0;
    int i=pam_get_uid( NULL, &uid, NULL);
    if ( i != PAM_SUCCESS ) { 
      fprintf(stderr,"%s: %s\n",*argv,pam_errors(i));
      exit(0);
    }
    
    i=get_tally( &tally, uid, cline_filename, &TALLY, fsp );
    if ( i != PAM_SUCCESS ) { 
      if (TALLY) fclose(TALLY);       
      fprintf(stderr,"%s: %s\n",*argv,pam_errors(i));
      exit(0);
    }
    
    if ( !cline_quiet ) 
      printf("User %s\t("UID_FMT")\t%s "TALLY_FMT"\n",cline_user,uid,
             (cline_reset!=TALLY_HI)?"had":"has",tally);
    
    i=set_tally( cline_reset, uid, cline_filename, &TALLY, fsp );
    if ( i != PAM_SUCCESS ) { 
      if (TALLY) fclose(TALLY);      
      fprintf(stderr,"%s: %s\n",*argv,pam_errors(i));
      exit(0);
    }
  }
  else /* !cline_user (ie, operate on all users) */ {
    FILE *TALLY=fopen(cline_filename, "r");
    uid_t uid=0;
    if ( !TALLY ) perror(*argv), exit(0);
    
    for ( ; !feof(TALLY); uid++ ) {
      tally_t tally;
      struct passwd *pw;
      if ( ! fread((char *) &fsp->fs_faillog,
		   sizeof (struct faillog), 1, TALLY)
	   || ! fsp->fs_faillog.fail_cnt ) {
      	tally=fsp->fs_faillog.fail_cnt;
      	continue;
      	}
      tally = fsp->fs_faillog.fail_cnt;	
      
      if ( ( pw=getpwuid(uid) ) ) {
        printf("User %s\t("UID_FMT")\t%s "TALLY_FMT"\n",pw->pw_name,uid,
               (cline_reset!=TALLY_HI)?"had":"has",tally);
      }
      else {
        printf("User [NONAME]\t("UID_FMT")\t%s "TALLY_FMT"\n",uid,
               (cline_reset!=TALLY_HI)?"had":"has",tally);
      }
    }
    fclose(TALLY);
    if ( cline_reset!=0 && cline_reset!=TALLY_HI ) {
      fprintf(stderr,"%s: Can't reset all users to non-zero\n",*argv);
    }
    else if ( !cline_reset ) {
      TALLY=fopen(cline_filename, "w");
      if ( !TALLY ) perror(*argv), exit(0);
      fclose(TALLY);
    }
  }
  return 0;
}


#endif

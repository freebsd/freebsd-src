/*
 * $Id: pam_filter.c,v 1.2 2000/11/19 23:54:03 agmorgan Exp $
 *
 * written by Andrew Morgan <morgan@transmeta.com> with much help from
 * Richard Stevens' UNIX Network Programming book.
 */

#include <security/_pam_aconf.h>

#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <termio.h>

#include <signal.h>

#define PAM_SM_AUTH
#define PAM_SM_ACCOUNT
#define PAM_SM_SESSION
#define PAM_SM_PASSWORD

#include <security/pam_modules.h>
#include <security/pam_filter.h>

/* ------ some tokens used for convenience throughout this file ------- */

#define FILTER_DEBUG     01
#define FILTER_RUN1      02
#define FILTER_RUN2      04
#define NEW_TERM        010
#define NON_TERM        020

/* -------------------------------------------------------------------- */

/* log errors */

#include <stdarg.h>

static void _pam_log(int err, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    openlog("pam_filter", LOG_CONS|LOG_PID, LOG_AUTH);
    vsyslog(err, format, args);
    va_end(args);
    closelog();
}

#define TERMINAL_LEN 12

static int master(char *terminal)
/*
 * try to open all of the terminals in sequence return first free one,
 * or -1
 */
{
     const char ptys[] = "pqrs", *pty = ptys;
     const char hexs[] = "0123456789abcdef", *hex;
     struct stat tstat;
     int fd;

     strcpy(terminal, "/dev/pty??");

     while (*pty) {                   /* step through four types */
	  terminal[8] = *pty++;
	  terminal[9] = '0';
	  if (stat(terminal,&tstat) < 0) {
	       _pam_log(LOG_WARNING, "unknown pseudo terminal; %s", terminal);
	       break;
	  }
	  for (hex = hexs; *hex; ) {  /* step through 16 of these */
	       terminal[9] = *hex++;
	       if ((fd = open(terminal, O_RDWR)) >= 0) {
		    return fd;
	       }
	  }
     }

     /* no terminal found */

     return -1;
}

static int process_args(pam_handle_t *pamh
			, int argc, const char **argv, const char *type
			, char ***evp, const char **filtername)
{
    int ctrl=0;

    while (argc-- > 0) {
	if (strcmp("debug",*argv) == 0) {
	    ctrl |= FILTER_DEBUG;
	} else if (strcmp("new_term",*argv) == 0) {
	    ctrl |= NEW_TERM;
	} else if (strcmp("non_term",*argv) == 0) {
	    ctrl |= NON_TERM;
	} else if (strcmp("run1",*argv) == 0) {
	    ctrl |= FILTER_RUN1;
	    if (argc <= 0) {
		_pam_log(LOG_ALERT,"no run filter supplied");
	    } else
		break;
	} else if (strcmp("run2",*argv) == 0) {
	    ctrl |= FILTER_RUN2;
	    if (argc <= 0) {
		_pam_log(LOG_ALERT,"no run filter supplied");
	    } else
		break;
	} else {
	    _pam_log(LOG_ERR, "unrecognized option: %s (ignored)", *argv);
	}
	++argv;                   /* step along list */
    }

    if (argc < 0) {
	/* there was no reference to a filter */
	*filtername = NULL;
	*evp = NULL;
    } else {
	char **levp;
	const char *tmp;
	int i,size;

	*filtername = *++argv;
	if (ctrl & FILTER_DEBUG) {
	    _pam_log(LOG_DEBUG,"will run filter %s\n", *filtername);
	}

	levp = (char **) malloc(5*sizeof(char *));
	if (levp == NULL) {
	    _pam_log(LOG_CRIT,"no memory for environment of filter");
	    return -1;
	}

	for (size=i=0; i<argc; ++i) {
	    size += strlen(argv[i])+1;
	}

	/* the "ARGS" variable */

#define ARGS_OFFSET    5                          /*  sizeof("ARGS=");  */
#define ARGS_NAME      "ARGS="

	size += ARGS_OFFSET;

	levp[0] = (char *) malloc(size);
	if (levp[0] == NULL) {
	    _pam_log(LOG_CRIT,"no memory for filter arguments");
	    if (levp) {
		free(levp);
	    }
	    return -1;
	}

	strncpy(levp[0],ARGS_NAME,ARGS_OFFSET);
	for (i=0,size=ARGS_OFFSET; i<argc; ++i) {
	    strcpy(levp[0]+size, argv[i]);
	    size += strlen(argv[i]);
	    levp[0][size++] = ' ';
	}
	levp[0][--size] = '\0';                    /* <NUL> terminate */

	/* the "SERVICE" variable */

#define SERVICE_OFFSET    8                    /*  sizeof("SERVICE=");  */
#define SERVICE_NAME      "SERVICE="

	pam_get_item(pamh, PAM_SERVICE, (const void **)&tmp);
	size = SERVICE_OFFSET+strlen(tmp);

	levp[1] = (char *) malloc(size+1);
	if (levp[1] == NULL) {
	    _pam_log(LOG_CRIT,"no memory for service name");
	    if (levp) {
		free(levp[0]);
		free(levp);
	    }
	    return -1;
	}

	strncpy(levp[1],SERVICE_NAME,SERVICE_OFFSET);
	strcpy(levp[1]+SERVICE_OFFSET, tmp);
	levp[1][size] = '\0';                      /* <NUL> terminate */

	/* the "USER" variable */

#define USER_OFFSET    5                          /*  sizeof("USER=");  */
#define USER_NAME      "USER="

	pam_get_user(pamh, &tmp, NULL);
	if (tmp == NULL) {
	    tmp = "<unknown>";
	}
	size = USER_OFFSET+strlen(tmp);

	levp[2] = (char *) malloc(size+1);
	if (levp[2] == NULL) {
	    _pam_log(LOG_CRIT,"no memory for user's name");
	    if (levp) {
		free(levp[1]);
		free(levp[0]);
		free(levp);
	    }
	    return -1;
	}

	strncpy(levp[2],USER_NAME,USER_OFFSET);
	strcpy(levp[2]+USER_OFFSET, tmp);
	levp[2][size] = '\0';                      /* <NUL> terminate */

	/* the "USER" variable */

#define TYPE_OFFSET    5                          /*  sizeof("TYPE=");  */
#define TYPE_NAME      "TYPE="

	size = TYPE_OFFSET+strlen(type);

	levp[3] = (char *) malloc(size+1);
	if (levp[3] == NULL) {
	    _pam_log(LOG_CRIT,"no memory for type");
	    if (levp) {
		free(levp[2]);
		free(levp[1]);
		free(levp[0]);
		free(levp);
	    }
	    return -1;
	}

	strncpy(levp[3],TYPE_NAME,TYPE_OFFSET);
	strcpy(levp[3]+TYPE_OFFSET, type);
	levp[3][size] = '\0';                      /* <NUL> terminate */

	levp[4] = NULL;	                     /* end list */

	*evp = levp;
    }

    if ((ctrl & FILTER_DEBUG) && *filtername) {
	char **e;

	_pam_log(LOG_DEBUG,"filter[%s]: %s",type,*filtername);
	_pam_log(LOG_DEBUG,"environment:");
	for (e=*evp; e && *e; ++e) {
	    _pam_log(LOG_DEBUG,"  %s",*e);
	}
    }

    return ctrl;
}

static void free_evp(char *evp[])
{
    int i;

    if (evp)
	for (i=0; i<4; ++i) {
	    if (evp[i])
		free(evp[i]);
	}
    free(evp);
}

static int set_filter(pam_handle_t *pamh, int flags, int ctrl
		      , const char **evp, const char *filtername)
{
    int status=-1;
    char terminal[TERMINAL_LEN];
    struct termio stored_mode;           /* initial terminal mode settings */
    int fd[2], child=0, child2=0, aterminal;

    if (filtername == NULL || *filtername != '/') {
	_pam_log(LOG_ALERT, "filtername not permitted; require full path");
	return PAM_ABORT;
    }

    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
	aterminal = 0;
    } else {
	aterminal = 1;
    }

    if (aterminal) {

	/* open the master pseudo terminal */

	fd[0] = master(terminal);
	if (fd[0] < 0) {
	    _pam_log(LOG_CRIT,"no master terminal");
	    return PAM_AUTH_ERR;
	}

	/* set terminal into raw mode.. remember old mode so that we can
	   revert to it after the child has quit. */

	/* this is termio terminal handling... */

	if (ioctl(STDIN_FILENO, TCGETA, (char *) &stored_mode ) < 0) {
	    /* in trouble, so close down */
	    close(fd[0]);
	    _pam_log(LOG_CRIT, "couldn't copy terminal mode");
	    return PAM_ABORT;
	} else {
	    struct termio t_mode = stored_mode;

	    t_mode.c_iflag = 0;            /* no input control */
	    t_mode.c_oflag &= ~OPOST;      /* no ouput post processing */

	    /* no signals, canonical input, echoing, upper/lower output */
	    t_mode.c_lflag &= ~(ISIG|ICANON|ECHO|XCASE);
	    t_mode.c_cflag &= ~(CSIZE|PARENB);  /* no parity */
	    t_mode.c_cflag |= CS8;              /* 8 bit chars */

	    t_mode.c_cc[VMIN] = 1; /* number of chars to satisfy a read */
	    t_mode.c_cc[VTIME] = 0;          /* 0/10th second for chars */

	    if (ioctl(STDIN_FILENO, TCSETA, (char *) &t_mode) < 0) {
		close(fd[0]);
		_pam_log(LOG_WARNING, "couldn't put terminal in RAW mode");
		return PAM_ABORT;
	    }

	    /*
	     * NOTE: Unlike the stream socket case here the child
	     * opens the slave terminal as fd[1] *after* the fork...
	     */
	}
    } else {

	/*
	 * not a terminal line so just open a stream socket fd[0-1]
	 * both set...
	 */

	if ( socketpair(AF_UNIX, SOCK_STREAM, 0, fd) < 0 ) {
	    _pam_log(LOG_CRIT,"couldn't open a stream pipe");
	    return PAM_ABORT;
	}
    }

    /* start child process */

    if ( (child = fork()) < 0 ) {

	_pam_log(LOG_WARNING,"first fork failed");
	if (aterminal) {
	    (void) ioctl(STDIN_FILENO, TCSETA, (char *) &stored_mode);
	}

	return PAM_AUTH_ERR;
    }

    if ( child == 0 ) {                  /* child process *is* application */

	if (aterminal) {

	    /* close the controlling tty */

#if defined(__hpux) && defined(O_NOCTTY)
	    int t = open("/dev/tty", O_RDWR|O_NOCTTY);
#else
	    int t = open("/dev/tty",O_RDWR);
	    if (t > 0) {
		(void) ioctl(t, TIOCNOTTY, NULL);
		close(t);
	    }
#endif /* defined(__hpux) && defined(O_NOCTTY) */

	    /* make this process it's own process leader */
	    if (setsid() == -1) {
		_pam_log(LOG_WARNING,"child cannot become new session");
		return PAM_ABORT;
	    }

	    /* find slave's name */
	    terminal[5] = 't';             /* want to open slave terminal */
	    fd[1] = open(terminal, O_RDWR);
	    close(fd[0]);      /* process is the child -- uses line fd[1] */

	    if (fd[1] < 0) {
		_pam_log(LOG_WARNING,"cannot open slave terminal; %s"
			 ,terminal);
		return PAM_ABORT;
	    }

	    /* initialize the child's terminal to be the way the
	       parent's was before we set it into RAW mode */

	    if (ioctl(fd[1], TCSETA, (char *) &stored_mode) < 0) {
		_pam_log(LOG_WARNING,"cannot set slave terminal mode; %s"
			 ,terminal);
		close(fd[1]);
		return PAM_ABORT;
	    }

	} else {

	    /* nothing to do for a simple stream socket */

	}

	/* re-assign the stdin/out to fd[1] <- (talks to filter). */

	if ( dup2(fd[1],STDIN_FILENO) != STDIN_FILENO ||
	     dup2(fd[1],STDOUT_FILENO) != STDOUT_FILENO ||
	     dup2(fd[1],STDERR_FILENO) != STDERR_FILENO )  {
	    _pam_log(LOG_WARNING
		     ,"unable to re-assign STDIN/OUT/ERR...'s");
	    close(fd[1]);
	    return PAM_ABORT;
	}

	/* make sure that file descriptors survive 'exec's */

	if ( fcntl(STDIN_FILENO, F_SETFD, 0) ||
	     fcntl(STDOUT_FILENO,F_SETFD, 0) ||
	     fcntl(STDERR_FILENO,F_SETFD, 0) ) {
	    _pam_log(LOG_WARNING
		     ,"unable to re-assign STDIN/OUT/ERR...'s");
	    return PAM_ABORT;
	}

	/* now the user input is read from the parent/filter: forget fd */

	close(fd[1]);

	/* the current process is now aparently working with filtered
	   stdio/stdout/stderr --- success! */

	return PAM_SUCCESS;
    }

    /*
     * process is the parent here. So we can close the application's
     * input/output
     */

    close(fd[1]);

    /* Clear out passwords... there is a security problem here in
     * that this process never executes pam_end.  Consequently, any
     * other sensitive data in this process is *not* explicitly
     * overwritten, before the process terminates */

    (void) pam_set_item(pamh, PAM_AUTHTOK, NULL);
    (void) pam_set_item(pamh, PAM_OLDAUTHTOK, NULL);

    /* fork a copy of process to run the actual filter executable */

    if ( (child2 = fork()) < 0 ) {

	_pam_log(LOG_WARNING,"filter fork failed");
	child2 = 0;

    } else if ( child2 == 0 ) {              /* exec the child filter */

	if ( dup2(fd[0],APPIN_FILENO) != APPIN_FILENO ||
	     dup2(fd[0],APPOUT_FILENO) != APPOUT_FILENO ||
	     dup2(fd[0],APPERR_FILENO) != APPERR_FILENO )  {
	    _pam_log(LOG_WARNING
		     ,"unable to re-assign APPIN/OUT/ERR...'s");
	    close(fd[0]);
	    exit(1);
	}

	/* make sure that file descriptors survive 'exec's */

	if ( fcntl(APPIN_FILENO, F_SETFD, 0) == -1 ||
	     fcntl(APPOUT_FILENO,F_SETFD, 0) == -1 ||
	     fcntl(APPERR_FILENO,F_SETFD, 0) == -1 ) {
	    _pam_log(LOG_WARNING
		     ,"unable to retain APPIN/OUT/ERR...'s");
	    close(APPIN_FILENO);
	    close(APPOUT_FILENO);
	    close(APPERR_FILENO);
	    exit(1);
	}

	/* now the user input is read from the parent through filter */

	execle(filtername, "<pam_filter>", NULL, evp);

	/* getting to here is an error */

	_pam_log(LOG_ALERT, "filter: %s, not executable", filtername);

    } else {           /* wait for either of the two children to exit */

	while (child && child2) {    /* loop if there are two children */
	    int lstatus=0;
	    int chid;

	    chid = wait(&lstatus);
	    if (chid == child) {

		if (WIFEXITED(lstatus)) {            /* exited ? */
		    status = WEXITSTATUS(lstatus);
		} else if (WIFSIGNALED(lstatus)) {   /* killed ? */
		    status = -1;
		} else
		    continue;             /* just stopped etc.. */
		child = 0;        /* the child has exited */

	    } else if (chid == child2) {
		/*
		 * if the filter has exited. Let the child die
		 * naturally below
		 */
		if (WIFEXITED(lstatus) || WIFSIGNALED(lstatus))
		    child2 = 0;
	    } else {

		_pam_log(LOG_ALERT
			 ,"programming error <chid=%d,lstatus=%x>: "
			 __FILE__ " line %d"
			 , lstatus, __LINE__ );
		child = child2 = 0;
		status = -1;

	    }
	}
    }

    close(fd[0]);

    /* if there is something running, wait for it to exit */

    while (child || child2) {
	int lstatus=0;
	int chid;

	chid = wait(&lstatus);

	if (child && chid == child) {

	    if (WIFEXITED(lstatus)) {            /* exited ? */
		status = WEXITSTATUS(lstatus);
	    } else if (WIFSIGNALED(lstatus)) {   /* killed ? */
		status = -1;
	    } else
		continue;             /* just stopped etc.. */
	    child = 0;        /* the child has exited */

	} else if (child2 && chid == child2) {

	    if (WIFEXITED(lstatus) || WIFSIGNALED(lstatus))
		child2 = 0;

	} else {

	    _pam_log(LOG_ALERT
		     ,"programming error <chid=%d,lstatus=%x>: "
		     __FILE__ " line %d"
		     , lstatus, __LINE__ );
	    child = child2 = 0;
	    status = -1;

	}
    }

    if (aterminal) {
	/* reset to initial terminal mode */
	(void) ioctl(STDIN_FILENO, TCSETA, (char *) &stored_mode);
    }

    if (ctrl & FILTER_DEBUG) {
	_pam_log(LOG_DEBUG,"parent process exited");      /* clock off */
    }

    /* quit the parent process, returning the child's exit status */

    exit(status);
}

static int set_the_terminal(pam_handle_t *pamh)
{
    const char *tty;

    if (pam_get_item(pamh, PAM_TTY, (const void **)&tty) != PAM_SUCCESS
	|| tty == NULL) {
	tty = ttyname(STDIN_FILENO);
	if (tty == NULL) {
	    _pam_log(LOG_ERR, "couldn't get the tty name");
	    return PAM_ABORT;
	}
	if (pam_set_item(pamh, PAM_TTY, tty) != PAM_SUCCESS) {
	    _pam_log(LOG_ERR, "couldn't set tty name");
	    return PAM_ABORT;
	}
    }
    return PAM_SUCCESS;
}

static int need_a_filter(pam_handle_t *pamh
			 , int flags, int argc, const char **argv
			 , const char *name, int which_run)
{
    int ctrl;
    char **evp;
    const char *filterfile;
    int retval;

    ctrl = process_args(pamh, argc, argv, name, &evp, &filterfile);
    if (ctrl == -1) {
	return PAM_AUTHINFO_UNAVAIL;
    }

    /* set the tty to the old or the new one? */

    if (!(ctrl & NON_TERM) && !(ctrl & NEW_TERM)) {
	retval = set_the_terminal(pamh);
	if (retval != PAM_SUCCESS) {
	    _pam_log(LOG_ERR, "tried and failed to set PAM_TTY");
	}
    } else {
	retval = PAM_SUCCESS;  /* nothing to do which is always a success */
    }

    if (retval == PAM_SUCCESS && (ctrl & which_run)) {
	retval = set_filter(pamh, flags, ctrl
			    , (const char **)evp, filterfile);
    }

    if (retval == PAM_SUCCESS 
	&& !(ctrl & NON_TERM) && (ctrl & NEW_TERM)) {
	retval = set_the_terminal(pamh);
	if (retval != PAM_SUCCESS) {
	    _pam_log(LOG_ERR
		     , "tried and failed to set new terminal as PAM_TTY");
	}
    }

    free_evp(evp);

    if (ctrl & FILTER_DEBUG) {
	_pam_log(LOG_DEBUG, "filter/%s, returning %d", name, retval);
	_pam_log(LOG_DEBUG, "[%s]", pam_strerror(pamh, retval));
    }

    return retval;
}

/* ----------------- public functions ---------------- */

/*
 * here are the advertised access points ...
 */

/* ------------------ authentication ----------------- */

PAM_EXTERN int pam_sm_authenticate(pam_handle_t *pamh
				   , int flags, int argc, const char **argv)
{
    return need_a_filter(pamh, flags, argc, argv
			 , "authenticate", FILTER_RUN1);
}

PAM_EXTERN int pam_sm_setcred(pam_handle_t *pamh, int flags
			      , int argc, const char **argv)
{
    return need_a_filter(pamh, flags, argc, argv, "setcred", FILTER_RUN2);
}

/* --------------- account management ---------------- */

PAM_EXTERN int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags, int argc,
                              const char **argv)
{
    return need_a_filter(pamh, flags, argc, argv
			 , "setcred", FILTER_RUN1|FILTER_RUN2 );
}

/* --------------- session management ---------------- */

PAM_EXTERN int pam_sm_open_session(pam_handle_t *pamh, int flags
				   , int argc, const char **argv)
{
    return need_a_filter(pamh, flags, argc, argv
			 , "open_session", FILTER_RUN1);
}

PAM_EXTERN int pam_sm_close_session(pam_handle_t *pamh, int flags
			      , int argc, const char **argv)
{
    return need_a_filter(pamh, flags, argc, argv
			 , "close_session", FILTER_RUN2);
}

/* --------- updating authentication tokens --------- */


PAM_EXTERN int pam_sm_chauthtok(pam_handle_t *pamh, int flags
				, int argc, const char **argv)
{
    int runN;

    if (flags & PAM_PRELIM_CHECK)
	runN = FILTER_RUN1;
    else if (flags & PAM_UPDATE_AUTHTOK)
	runN = FILTER_RUN2;
    else {
	_pam_log(LOG_ERR, "unknown flags for chauthtok (0x%X)", flags);
	return PAM_TRY_AGAIN;
    }

    return need_a_filter(pamh, flags, argc, argv, "chauthtok", runN);
}

#ifdef PAM_STATIC

/* ------------ stuff for static modules ------------ */

struct pam_module _pam_filter_modstruct = {
    "pam_filter",
    pam_sm_authenticate,
    pam_sm_setcred,
    pam_sm_acct_mgmt,
    pam_sm_open_session,
    pam_sm_close_session,
    pam_sm_chauthtok,
};

#endif

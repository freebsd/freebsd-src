/*
 * $Id: upperLOWER.c,v 1.2 2000/11/19 23:54:03 agmorgan Exp $
 *
 * This is a sample filter program, for use with pam_filter (a module
 * provided with Linux-PAM). This filter simply transposes upper and
 * lower case letters, it is intended for demonstration purposes and
 * it serves no purpose other than to annoy the user...
 */

#include <security/_pam_aconf.h>

#include <stdio.h>
#include <syslog.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <security/pam_filter.h>

/* ---------------------------------------------------------------- */

#include <stdarg.h>
#ifdef hpux
# define log_this syslog
#else
static void log_this(int err, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    openlog("upperLOWER", LOG_CONS|LOG_PID, LOG_AUTH);
    vsyslog(err, format, args);
    va_end(args);
    closelog();
}
#endif

#include <ctype.h>

static void do_transpose(char *buffer,int len)
{
     int i;
     for (i=0; i<len; ++i) {
	  if (islower(buffer[i])) {
	       buffer[i] = toupper(buffer[i]);
	  } else {
	       buffer[i] = tolower(buffer[i]);
	  }
     }
}

extern char **environ;

int main(int argc, char **argv) 
{
     char buffer[BUFSIZ];
     fd_set readers;
     void (*before_user)(char *,int);
     void (*before_app)(char *,int);

#ifdef DEBUG
     {
	  int i;

	  fprintf(stderr,"environment :[\r\n");
	  for (i=0; environ[i]; ++i) {
	       fprintf(stderr,"-> %s\r\n",environ[i]);
	  }
	  fprintf(stderr,"]: end\r\n");
     }
#endif

     if (argc != 1) {
#ifdef DEBUG
	  fprintf(stderr,"filter invoked as conventional executable\n");
#else
	  log_this(LOG_ERR, "filter invoked as conventional executable");
#endif
	  exit(1);
     }

     before_user = before_app = do_transpose;   /* assign filter functions */

     /* enter a loop that deals with the input and output of the
        user.. passing it to and from the application */

     FD_ZERO(&readers);                    /* initialize reading mask */

     for (;;) {

	  FD_SET(APPOUT_FILENO, &readers);              /* wake for output */
	  FD_SET(APPERR_FILENO, &readers);               /* wake for error */
	  FD_SET(STDIN_FILENO, &readers);                /* wake for input */

	  if ( select(APPTOP_FILE,&readers,NULL,NULL,NULL) < 0 ) {
#ifdef DEBUG
	       fprintf(stderr,"select failed\n");
#else
	       log_this(LOG_WARNING,"select failed");
#endif
	       break;
	  }

	  /* application errors */

	  if ( FD_ISSET(APPERR_FILENO,&readers) ) {
	       int got = read(APPERR_FILENO, buffer, BUFSIZ);
	       if (got <= 0) {
		    break;
	       } else {
		    /* translate to give to real terminal */
		    if (before_user != NULL)
			 before_user(buffer, got);
		    if ( write(STDERR_FILENO, buffer, got) != got ) {
			 log_this(LOG_WARNING,"couldn't write %d bytes?!",got);
			 break;
		    }
	       }
	  } else if ( FD_ISSET(APPOUT_FILENO,&readers) ) {    /* app output */
	       int got = read(APPOUT_FILENO, buffer, BUFSIZ);
	       if (got <= 0) {
		    break;
	       } else {
		    /* translate to give to real terminal */
		    if (before_user != NULL)
			 before_user(buffer, got);
		    if ( write(STDOUT_FILENO, buffer, got) != got ) {
			 log_this(LOG_WARNING,"couldn't write %d bytes!?",got);
			 break;
		    }
	       }
	  }

	  if ( FD_ISSET(STDIN_FILENO, &readers) ) {  /* user input */
	       int got = read(STDIN_FILENO, buffer, BUFSIZ);
	       if (got < 0) {
		    log_this(LOG_WARNING,"user input junked");
		    break;
	       } else if (got) {
		    /* translate to give to application */
		    if (before_app != NULL)
			 before_app(buffer, got);
		    if ( write(APPIN_FILENO, buffer, got) != got ) {
			 log_this(LOG_WARNING,"couldn't pass %d bytes!?",got);
			 break;
		    }
	       } else {
		    /* nothing received -- an error? */
		    log_this(LOG_WARNING,"user input null?");
		    break;
	       }
	  }
     }

     exit(0);
}




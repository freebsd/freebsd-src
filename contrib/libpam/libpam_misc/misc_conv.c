/*
 * $Id: misc_conv.c,v 1.5 1997/01/04 20:16:48 morgan Exp morgan $
 * $FreeBSD$
 *
 * A generic conversation function for text based applications
 *
 * Written by Andrew Morgan <morgan@linux.kernel.org>
 *
 * $Log: misc_conv.c,v $
 * Revision 1.5  1997/01/04 20:16:48  morgan
 * removed getpass. Replaced with POSIX code for same function which
 * also observes timeouts specified by the parent application
 *
 * Revision 1.4  1996/12/01 03:26:51  morgan
 * *** empty log message ***
 *
 * Revision 1.3  1996/11/10 20:10:01  morgan
 * sgi definition
 *
 * Revision 1.2  1996/07/07 23:59:56  morgan
 * changed the name of the misc include file
 *
 * Revision 1.1  1996/05/02 05:17:06  morgan
 * Initial revision
 */

#ifdef linux
#define _GNU_SOURCE
#include <features.h>
#endif

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include <security/pam_appl.h>
#include <security/pam_misc.h>

#define INPUTSIZE PAM_MAX_MSG_SIZE           /* maximum length of input+1 */
#define CONV_ECHO_ON  1                            /* types of echo state */
#define CONV_ECHO_OFF 0

/*
 * external timeout definitions - these can be overriden by the
 * application.
 */

time_t pam_misc_conv_warn_time = 0;                  /* time when we warn */
time_t pam_misc_conv_die_time  = 0;               /* time when we timeout */

const char *pam_misc_conv_warn_line = "..\a.Time is running out...\n";
const char *pam_misc_conv_die_line  = "..\a.Sorry, your time is up!\n";

int pam_misc_conv_died=0;       /* application can probe this for timeout */

static void pam_misc_conv_delete_binary(void **delete_me)
{
    if (delete_me && *delete_me) {
	unsigned char *packet = *(unsigned char **)delete_me;
	int length;

	length = 4+(packet[0]<<24)+(packet[1]<<16)+(packet[2]<<8)+packet[3];
	memset(packet, 0, length);
	free(packet);
	*delete_me = packet = NULL;
    }
}

/* These function pointers are for application specific binary
   conversations.  One or both of the arguments to the first function
   must be non-NULL.  The first function must return PAM_SUCCESS or
   PAM_CONV_ERR.  If input is non-NULL, a response is expected, this
   response should be malloc()'d and will eventually be free()'d by
   the calling module. The structure of this malloc()'d response is as
   follows:

          { int length, char data[length] }

   For convenience, the pointer used by the two function pointer
   prototypes is 'void *'.

   The ...free() fn pointer is used to discard a binary message that
   is not of the default form.  It should be explicitly overwritten
   when using some other convention for the structure of a binary
   prompt (not recommended). */

int (*pam_binary_handler_fn)(const void *send, void **receive) = NULL;
void (*pam_binary_handler_free)(void **packet_p) = pam_misc_conv_delete_binary;

/* the following code is used to get text input */

volatile static int expired=0;

/* return to the previous signal handling */
static void reset_alarm(struct sigaction *o_ptr)
{
    (void) alarm(0);                 /* stop alarm clock - if still ticking */
    (void) sigaction(SIGALRM, o_ptr, NULL);
}

/* this is where we intercept the alarm signal */
static void time_is_up(int ignore)
{
    expired = 1;
}

/* set the new alarm to hit the time_is_up() function */
static int set_alarm(int delay, struct sigaction *o_ptr)
{
    struct sigaction new_sig;

    sigemptyset(&new_sig.sa_mask);
    new_sig.sa_flags = 0;
    new_sig.sa_handler = time_is_up;
    if ( sigaction(SIGALRM, &new_sig, o_ptr) ) {
	return 1;         /* setting signal failed */
    }
    if ( alarm(delay) ) {
	(void) sigaction(SIGALRM, o_ptr, NULL);
	return 1;         /* failed to set alarm */
    }
    return 0;             /* all seems to have worked */
}

/* return the number of seconds to next alarm. 0 = no delay, -1 = expired */
static int get_delay(void)
{
    time_t now;

    expired = 0;                                        /* reset flag */
    (void) time(&now);

    /* has the quit time past? */
    if (pam_misc_conv_die_time && now >= pam_misc_conv_die_time) {
	fprintf(stderr,"%s",pam_misc_conv_die_line);

	pam_misc_conv_died = 1;       /* note we do not reset the die_time */
	return -1;                                           /* time is up */
    }

    /* has the warning time past? */
    if (pam_misc_conv_warn_time && now >= pam_misc_conv_warn_time) {
	fprintf(stderr, "%s", pam_misc_conv_warn_line);
	pam_misc_conv_warn_time = 0;                    /* reset warn_time */

	/* indicate remaining delay - if any */

	return (pam_misc_conv_die_time ? pam_misc_conv_die_time - now:0 );
    }

    /* indicate possible warning delay */

    if (pam_misc_conv_warn_time)
	return (pam_misc_conv_warn_time - now);
    else if (pam_misc_conv_die_time)
	return (pam_misc_conv_die_time - now);
    else
	return 0;
}

/* read a line of input string, giving prompt when appropriate */
static char *read_string(int echo, const char *prompt)
{
    struct termios term_before, term_tmp;
    char line[INPUTSIZE];
    struct sigaction old_sig;
    int delay, nc, have_term=0;

    D(("called with echo='%s', prompt='%s'.", echo ? "ON":"OFF" , prompt));

    if (isatty(STDIN_FILENO)) {                      /* terminal state */

	/* is a terminal so record settings and flush it */
	if ( tcgetattr(STDIN_FILENO, &term_before) != 0 ) {
	    D(("<error: failed to get terminal settings>"));
	    return NULL;
	}
	memcpy(&term_tmp, &term_before, sizeof(term_tmp));
	if (echo) {
	    term_tmp.c_lflag |= ECHO;
	} else {
	    term_tmp.c_lflag &= ~(ECHO);
	}
	have_term = 1;

    } else if (!echo) {
	D(("<warning: cannot turn echo off>"));
    }

    /* set up the signal handling */
    delay = get_delay();

    /* reading the line */
    while (delay >= 0) {

	fprintf(stderr, "%s", prompt);
	/* this may, or may not set echo off -- drop pending input */
	if (have_term)
	    (void) tcsetattr(STDIN_FILENO, TCSAFLUSH, &term_tmp);

	if ( delay > 0 && set_alarm(delay, &old_sig) ) {
	    D(("<failed to set alarm>"));
	    break;
	} else {
	    nc = read(STDIN_FILENO, line, INPUTSIZE-1);
	    if (have_term) {
		(void) tcsetattr(STDIN_FILENO, TCSADRAIN, &term_before);
		if (!echo || expired)             /* do we need a newline? */
		    fprintf(stderr,"\n");
	    }
	    if ( delay > 0 ) {
		reset_alarm(&old_sig);
	    }
	    if (expired) {
		delay = get_delay();
	    } else if (nc > 0) {                 /* we got some user input */
		char *input;

		if (nc > 0 && line[nc-1] == '\n') {     /* <NUL> terminate */
		    line[--nc] = '\0';
		} else {
		    line[nc] = '\0';
		}
		input = x_strdup(line);
		_pam_overwrite(line);

		return input;                  /* return malloc()ed string */
	    } else if (nc == 0) {                                /* Ctrl-D */
		D(("user did not want to type anything"));
		fprintf(stderr, "\n");
		break;
	    }
	}
    }

    /* getting here implies that the timer expired */
    if (have_term)
	(void) tcsetattr(STDIN_FILENO, TCSADRAIN, &term_before);

    memset(line, 0, INPUTSIZE);                      /* clean up */
    return NULL;
}

/* end of read_string functions */

int misc_conv(int num_msg, const struct pam_message **msgm,
	      struct pam_response **response, void *appdata_ptr)
{
    int count=0;
    struct pam_response *reply;

    if (num_msg <= 0)
	return PAM_CONV_ERR;

    D(("allocating empty response structure array."));

    reply = (struct pam_response *) calloc(num_msg,
					   sizeof(struct pam_response));
    if (reply == NULL) {
	D(("no memory for responses"));
	return PAM_CONV_ERR;
    }

    D(("entering conversation function."));

    for (count=0; count < num_msg; ++count) {
	char *string=NULL;

	switch (msgm[count]->msg_style) {
	case PAM_PROMPT_ECHO_OFF:
	    string = read_string(CONV_ECHO_OFF,msgm[count]->msg);
	    if (string == NULL) {
		goto failed_conversation;
	    }
	    break;
	case PAM_PROMPT_ECHO_ON:
	    string = read_string(CONV_ECHO_ON,msgm[count]->msg);
	    if (string == NULL) {
		goto failed_conversation;
	    }
	    break;
	case PAM_ERROR_MSG:
	    if (fprintf(stderr,"%s\n",msgm[count]->msg) < 0) {
		goto failed_conversation;
	    }
	    break;
	case PAM_TEXT_INFO:
	    if (fprintf(stdout,"%s\n",msgm[count]->msg) < 0) {
		goto failed_conversation;
	    }
	    break;
	case PAM_BINARY_PROMPT:
	{
	    void *pack_out=NULL;
	    const void *pack_in = msgm[count]->msg;

	    if (!pam_binary_handler_fn
		|| pam_binary_handler_fn(pack_in, &pack_out) != PAM_SUCCESS
		|| pack_out == NULL) {
		goto failed_conversation;
	    }
	    string = (char *) pack_out;
	    pack_out = NULL;

	    break;
	}
	case PAM_BINARY_MSG:
	{
	    const void *pack_in = msgm[count]->msg;
	    if (!pam_binary_handler_fn
		|| pam_binary_handler_fn(pack_in, NULL) != PAM_SUCCESS) {
		goto failed_conversation;
	    }
	    break;
	}
	default:
	    fprintf(stderr, "erroneous conversation (%d)\n"
		    ,msgm[count]->msg_style);
	    goto failed_conversation;
	}

	if (string) {                         /* must add to reply array */
	    /* add string to list of responses */

	    reply[count].resp_retcode = 0;
	    reply[count].resp = string;
	    string = NULL;
	}
    }

    /* New (0.59+) behavior is to always have a reply - this is
       compatable with the X/Open (March 1997) spec. */
    *response = reply;
    reply = NULL;

    return PAM_SUCCESS;

failed_conversation:

    if (reply) {
	for (count=0; count<num_msg; ++count) {
	    if (reply[count].resp == NULL) {
		continue;
	    }
	    switch (msgm[count]->msg_style) {
	    case PAM_PROMPT_ECHO_ON:
	    case PAM_PROMPT_ECHO_OFF:
		_pam_overwrite(reply[count].resp);
		free(reply[count].resp);
		break;
	    case PAM_BINARY_PROMPT:
		pam_binary_handler_free((void **) &reply[count].resp);
		break;
	    case PAM_ERROR_MSG:
	    case PAM_TEXT_INFO:
	    case PAM_BINARY_MSG:
		/* should not actually be able to get here... */
		free(reply[count].resp);
	    }                                            
	    reply[count].resp = NULL;
	}
	/* forget reply too */
	free(reply);
	reply = NULL;
    }

    return PAM_CONV_ERR;
}


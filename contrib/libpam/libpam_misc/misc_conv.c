/*
 * $Id: misc_conv.c,v 1.3 2001/01/20 22:29:47 agmorgan Exp $
 * $FreeBSD$
 *
 * A generic conversation function for text based applications
 *
 * Written by Andrew Morgan <morgan@linux.kernel.org>
 */

#include <security/_pam_aconf.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include <security/pam_appl.h>
#include <security/pam_client.h>
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

/*
 * These functions are for binary prompt manipulation.
 * The manner in which a binary prompt is processed is application
 * specific, so these function pointers are provided and can be
 * initialized by the application prior to the conversation function
 * being used.
 */

static void pam_misc_conv_delete_binary(void *appdata,
					pamc_bp_t *delete_me)
{
    PAM_BP_RENEW(delete_me, 0, 0);
}

int (*pam_binary_handler_fn)(void *appdata, pamc_bp_t *prompt_p) = NULL;
void (*pam_binary_handler_free)(void *appdata, pamc_bp_t *prompt_p)
      = pam_misc_conv_delete_binary;

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
    char *input;
    char line[INPUTSIZE];
    struct sigaction old_sig;
    int delay, nc, have_term=0;
    sigset_t oset, nset;

    D(("called with echo='%s', prompt='%s'.", echo ? "ON":"OFF" , prompt));

    if (isatty(STDIN_FILENO)) {                      /* terminal state */

	/* is a terminal so record settings and flush it */
	if ( tcgetattr(STDIN_FILENO, &term_before) != 0 ) {
	    D(("<error: failed to get terminal settings>"));
	    return NULL;
	}
	memcpy(&term_tmp, &term_before, sizeof(term_tmp));
	if (!echo) {
	    term_tmp.c_lflag &= ~(ECHO);
	}
	have_term = 1;
	/*
	 * note - blocking signals isn't necessarily the
	 * right thing, but we leave it for now.
	 */
	sigemptyset(&nset);
	sigaddset(&nset, SIGINT);
	sigaddset(&nset, SIGTSTP);
	(void)_sigprocmask(SIG_BLOCK, &nset, &oset);

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
		if (nc > 0 && line[nc-1] == '\n') {     /* <NUL> terminate */
		    line[--nc] = '\0';
		} else {
		    line[nc] = '\0';
		}
		input = x_strdup(line);
		_pam_overwrite(line);

		goto cleanexit;                  /* return malloc()ed string */
	    } else if (nc == 0) {                                /* Ctrl-D */
		D(("user did not want to type anything"));
		input = x_strdup("");
		goto cleanexit;                  /* return malloc()ed string */
	    }
	}
    }

    /* getting here implies that the timer expired */
    memset(line, 0, INPUTSIZE);                      /* clean up */
    input = NULL;

cleanexit:
    if (have_term) {
	(void)_sigprocmask(SIG_SETMASK, &oset, NULL);
	(void) tcsetattr(STDIN_FILENO, TCSADRAIN, &term_before);
    }
    return input;
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
	    pamc_bp_t binary_prompt = NULL;

	    if (!msgm[count]->msg || !pam_binary_handler_fn) {
		goto failed_conversation;
	    }

	    PAM_BP_RENEW(&binary_prompt,
			 PAM_BP_RCONTROL(msgm[count]->msg),
			 PAM_BP_LENGTH(msgm[count]->msg));
	    PAM_BP_FILL(binary_prompt, 0, PAM_BP_LENGTH(msgm[count]->msg),
			PAM_BP_RDATA(msgm[count]->msg));

	    if (pam_binary_handler_fn(appdata_ptr,
				      &binary_prompt) != PAM_SUCCESS
		|| (binary_prompt == NULL)) {
		goto failed_conversation;
	    }
	    string = (char *) binary_prompt;
	    binary_prompt = NULL;

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
		pam_binary_handler_free(appdata_ptr,
					(pamc_bp_t *) &reply[count].resp);
		break;
	    case PAM_ERROR_MSG:
	    case PAM_TEXT_INFO:
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


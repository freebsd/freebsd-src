/*
 * Copyright (c) 1989 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#include <popper.h>
RCSID("$Id: popper.c,v 1.16 2002/07/04 14:09:25 joda Exp $");

int hangup = FALSE ;

static RETSIGTYPE
catchSIGHUP(int sig)
{
    hangup = TRUE ;

    /* This should not be a problem on BSD systems */
    signal(SIGHUP,  catchSIGHUP);
    signal(SIGPIPE, catchSIGHUP);
    SIGRETURN(0);
}

int     pop_timeout = POP_TIMEOUT;

jmp_buf env;

static RETSIGTYPE
ring(int sig)
{
  longjmp(env,1);
}
  
/*
 * fgets, but with a timeout
 */
static char *
tgets(char *str, int size, FILE *fp, int timeout)
{
  signal(SIGALRM, ring);
  alarm(timeout);
  if (setjmp(env))
    str = NULL;
  else
    str = fgets(str,size,fp);
  alarm(0);
  signal(SIGALRM,SIG_DFL);
  return(str);
}

/* 
 *  popper: Handle a Post Office Protocol version 3 session
 */
int
main (int argc, char **argv)
{
    POP                 p;
    state_table     *   s;
    char                message[MAXLINELEN];

    signal(SIGHUP,  catchSIGHUP);
    signal(SIGPIPE, catchSIGHUP);

    /*  Start things rolling */
    pop_init(&p,argc,argv);

    /*  Tell the user that we are listenting */
    pop_msg(&p,POP_SUCCESS, "POP3 server ready");

    /*  State loop.  The POP server is always in a particular state in 
        which a specific suite of commands can be executed.  The following 
        loop reads a line from the client, gets the command, and processes 
        it in the current context (if allowed) or rejects it.  This continues 
        until the client quits or an error occurs. */

    for (p.CurrentState=auth1;p.CurrentState!=halt&&p.CurrentState!=error;) {
        if (hangup) {
            pop_msg(&p, POP_FAILURE, "POP hangup: %s", p.myhost);
            if (p.CurrentState > auth2 && !pop_updt(&p))
                pop_msg(&p, POP_FAILURE,
			"POP mailbox update failed: %s", p.myhost);
            p.CurrentState = error;
        } else if (tgets(message, MAXLINELEN, p.input, pop_timeout) == NULL) {
	    pop_msg(&p, POP_FAILURE, "POP timeout: %s", p.myhost);
	    if (p.CurrentState > auth2 && !pop_updt(&p))
                pop_msg(&p,POP_FAILURE,
			"POP mailbox update failed: %s", p.myhost);
            p.CurrentState = error;
        }
        else {
            /*  Search for the command in the command/state table */
            if ((s = pop_get_command(&p,message)) == NULL) continue;

            /*  Call the function associated with this command in 
                the current state */
            if (s->function) p.CurrentState = s->result[(*s->function)(&p)];

            /*  Otherwise assume NOOP and send an OK message to the client */
            else {
                p.CurrentState = s->success_state;
                pop_msg(&p,POP_SUCCESS,NULL);
            }
        }       
    }

    /*  Say goodbye to the client */
    pop_msg(&p,POP_SUCCESS,"Pop server at %s signing off.",p.myhost);

    /*  Log the end of activity */
    pop_log(&p,POP_PRIORITY,
        "(v%s) Ending request from \"%s\" at %s\n",VERSION,p.client,p.ipaddr);

    /*  Stop logging */
    closelog();

    return(0);
}

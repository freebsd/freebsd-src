/*
 * Copyright 1987, 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 *	from: get_in_tkt.c,v 4.12 89/07/18 16:32:56 jtkohl Exp $
 *	$Id: get_in_tkt.c,v 1.3 1995/07/18 16:38:30 mark Exp $
 */

#if 0
#ifndef lint
static char rcsid[] =
"$Id: get_in_tkt.c,v 1.3 1995/07/18 16:38:30 mark Exp $";
#endif /* lint */
#endif

#include <krb.h>
#include <prot.h>
#include <des.h>
#include "conf.h"

#include <stdio.h>

/*
 * This file contains two routines: passwd_to_key() converts
 * a password into a DES key (prompting for the password if
 * not supplied), and krb_get_pw_in_tkt() gets an initial ticket for
 * a user.
 */

/*
 * passwd_to_key(): given a password, return a DES key.
 * There are extra arguments here which (used to be?)
 * used by srvtab_to_key().
 *
 * If the "passwd" argument is not null, generate a DES
 * key from it, using string_to_key().
 *
 * If the "passwd" argument is null, call des_read_password()
 * to prompt for a password and then convert it into a DES key.
 *
 * In either case, the resulting key is put in the "key" argument,
 * and 0 is returned.
 */

/*ARGSUSED */
static int passwd_to_key(char *user, char *instance, char *realm,
    char *passwd, des_cblock key)
{
#ifdef NOENCRYPTION
    if (!passwd)
	placebo_read_password(key, "Password: ", 0);
#else
    if (passwd)
	string_to_key(passwd,(des_cblock *)key);
    else
	des_read_password((des_cblock *)key,"Password: ",0);
#endif
    return (0);
}

/*
 * krb_get_pw_in_tkt() takes the name of the server for which the initial
 * ticket is to be obtained, the name of the principal the ticket is
 * for, the desired lifetime of the ticket, and the user's password.
 * It passes its arguments on to krb_get_in_tkt(), which contacts
 * Kerberos to get the ticket, decrypts it using the password provided,
 * and stores it away for future use.
 *
 * krb_get_pw_in_tkt() passes two additional arguments to krb_get_in_tkt():
 * the name of a routine (passwd_to_key()) to be used to get the
 * password in case the "password" argument is null and NULL for the
 * decryption procedure indicating that krb_get_in_tkt should use the
 * default method of decrypting the response from the KDC.
 *
 * The result of the call to krb_get_in_tkt() is returned.
 */

int krb_get_pw_in_tkt(char *user, char *instance, char *realm, char *service,
    char *sinstance, int life, char *password)
{
    return(krb_get_in_tkt(user,instance,realm,service,sinstance,life,
                          passwd_to_key, NULL, password));
}

#ifdef NOENCRYPTION
/*
 * $Source: /usr/cvs/src/eBones/krb/get_in_tkt.c,v $
 * $Author: mark $
 *
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 *
 * This routine prints the supplied string to standard
 * output as a prompt, and reads a password string without
 * echoing.
 */

#if 0
#ifndef	lint
static char rcsid_read_password_c[] =
"Bones$Header: /usr/cvs/src/eBones/krb/get_in_tkt.c,v 1.3 1995/07/18 16:38:30 mark Exp $";
#endif	lint
#endif


/*** Routines ****************************************************** */
int placebo_read_password(des_cblock *k, char *prompt, int verify)
{
    int ok;
    char key_string[BUFSIZ];

#ifdef BSDUNIX
    if (setjmp(env)) {
	ok = -1;
	goto lose;
    }
#endif

    ok = placebo_read_pw_string(key_string, BUFSIZ, prompt, verify);
    if (ok == 0)
	bzero(k, sizeof(C_Block));

lose:
    bzero(key_string, sizeof (key_string));
    return ok;
}

/*
 * This version just returns the string, doesn't map to key.
 *
 * Returns 0 on success, non-zero on failure.
 */

int placebo_read_pw_string(char *s, int max, char *prompt, int verify)
    char *s;
    int	max;
    char *prompt;
    int	verify;
{
    int ok = 0;
    char *ptr;

#ifdef BSDUNIX
    jmp_buf old_env;
    struct sgttyb tty_state;
#endif
    char key_string[BUFSIZ];

    if (max > BUFSIZ) {
	return -1;
    }

#ifdef	BSDUNIX
    bcopy(old_env, env, sizeof(env));
    if (setjmp(env))
	goto lose;

    /* save terminal state*/
    if (ioctl(0,TIOCGETP,&tty_state) == -1)
	return -1;

    push_signals();
    /* Turn off echo */
    tty_state.sg_flags &= ~ECHO;
    if (ioctl(0,TIOCSETP,&tty_state) == -1)
	return -1;
#endif
    while (!ok) {
	printf(prompt);
	fflush(stdout);
#ifdef	CROSSMSDOS
	h19line(s,sizeof(s),0);
	if (!strlen(s))
	    continue;
#else
	if (!fgets(s, max, stdin)) {
	    clearerr(stdin);
	    continue;
	}
	if ((ptr = index(s, '\n')))
	    *ptr = '\0';
#endif
	if (verify) {
	    printf("\nVerifying, please re-enter %s",prompt);
	    fflush(stdout);
#ifdef CROSSMSDOS
	    h19line(key_string,sizeof(key_string),0);
	    if (!strlen(key_string))
		continue;
#else
	    if (!fgets(key_string, sizeof(key_string), stdin)) {
		clearerr(stdin);
		continue;
	    }
            if ((ptr = index(key_string, '\n')))
	    *ptr = '\0';
#endif
	    if (strcmp(s,key_string)) {
		printf("\n\07\07Mismatch - try again\n");
		fflush(stdout);
		continue;
	    }
	}
	ok = 1;
    }

#ifdef	BSDUNIX
lose:
    if (!ok)
	bzero(s, max);
    printf("\n");
    /* turn echo back on */
    tty_state.sg_flags |= ECHO;
    if (ioctl(0,TIOCSETP,&tty_state))
	ok = 0;
    pop_signals();
    bcopy(env, old_env, sizeof(env));
#endif
    if (verify)
	bzero(key_string, sizeof (key_string));
    s[max-1] = 0;		/* force termination */
    return !ok;			/* return nonzero if not okay */
}

#ifdef	BSDUNIX
/*
 * this can be static since we should never have more than
 * one set saved....
 */
#ifdef POSIX
static void (*old_sigfunc[NSIG])();
#else
static int (*old_sigfunc[NSIG])();
#endif POSIX

static void push_signals()
{
    register i;
    for (i = 0; i < NSIG; i++)
	old_sigfunc[i] = signal(i,sig_restore);
}

static void pop_signals()
{
    register i;
    for (i = 0; i < NSIG; i++)
	signal(i,old_sigfunc[i]);
}

static void sig_restore(int sig, int code, struct sigcontext *scp)
{
    longjmp(env,1);
}
#endif
#endif /* NOENCRYPTION */

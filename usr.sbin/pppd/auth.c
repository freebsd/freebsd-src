/*
 * auth.c - PPP authentication and phase control.
 *
 * Copyright (c) 1993 The Australian National University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the Australian National University.  The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Copyright (c) 1989 Carnegie Mellon University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Carnegie Mellon University.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static char rcsid[] = "$Id: auth.c,v 1.7 1996/10/01 03:41:28 pst Exp $";
#endif

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <pwd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <utmp.h>

#ifdef HAS_SHADOW
#include <shadow.h>
#include <shadow/pwauth.h>
#ifndef PW_PPP
#define PW_PPP PW_LOGIN
#endif
#endif

#include "pppd.h"
#include "fsm.h"
#include "lcp.h"
#include "upap.h"
#include "chap.h"
#include "ipcp.h"
#include "ccp.h"
#include "pathnames.h"

#if defined(sun) && defined(sparc)
#include <alloca.h>
#endif /*sparc*/

/* Used for storing a sequence of words.  Usually malloced. */
struct wordlist {
    struct wordlist	*next;
    char		word[1];
};

/* Bits in scan_authfile return value */
#define NONWILD_SERVER	1
#define NONWILD_CLIENT	2

#define ISWILD(word)	(word[0] == '*' && word[1] == 0)

#define FALSE	0
#define TRUE	1

/* Records which authentication operations haven't completed yet. */
static int auth_pending[NUM_PPP];
static int logged_in;
static struct wordlist *addresses[NUM_PPP];

/* Bits in auth_pending[] */
#define UPAP_WITHPEER	1
#define UPAP_PEER	2
#define CHAP_WITHPEER	4
#define CHAP_PEER	8

/* Prototypes */
void check_access __P((FILE *, char *));

static void network_phase __P((int));
static int  ppplogin __P((char *, char *, char **, int *));
static void ppplogout __P((void));
static int  null_login __P((int));
static int  get_upap_passwd __P((void));
static int  have_upap_secret __P((void));
static int  have_chap_secret __P((char *, char *));
static int  scan_authfile __P((FILE *, char *, char *, char *,
				  struct wordlist **, char *));
static void free_wordlist __P((struct wordlist *));

/*
 * An Open on LCP has requested a change from Dead to Establish phase.
 * Do what's necessary to bring the physical layer up.
 */
void
link_required(unit)
    int unit;
{
}

/*
 * LCP has terminated the link; go to the Dead phase and take the
 * physical layer down.
 */
void
link_terminated(unit)
    int unit;
{
    if (phase == PHASE_DEAD)
	return;
    if (logged_in)
	ppplogout();
    phase = PHASE_DEAD;
    syslog(LOG_NOTICE, "Connection terminated.");
}

/*
 * LCP has gone down; it will either die or try to re-establish.
 */
void
link_down(unit)
    int unit;
{
    ipcp_close(0);
    ccp_close(0);
    phase = PHASE_TERMINATE;
}

/*
 * The link is established.
 * Proceed to the Dead, Authenticate or Network phase as appropriate.
 */
void
link_established(unit)
    int unit;
{
    int auth;
    lcp_options *wo = &lcp_wantoptions[unit];
    lcp_options *go = &lcp_gotoptions[unit];
    lcp_options *ho = &lcp_hisoptions[unit];

    if (auth_required && !(go->neg_chap || go->neg_upap)) {
	/*
	 * We wanted the peer to authenticate itself, and it refused:
	 * treat it as though it authenticated with PAP using a username
	 * of "" and a password of "".  If that's not OK, boot it out.
	 */
	if (!wo->neg_upap || !null_login(unit)) {
	    syslog(LOG_WARNING, "peer refused to authenticate");
	    lcp_close(unit);
	    phase = PHASE_TERMINATE;
	    return;
	}
    }

    phase = PHASE_AUTHENTICATE;
    auth = 0;
    if (go->neg_chap) {
	ChapAuthPeer(unit, our_name, go->chap_mdtype);
	auth |= CHAP_PEER;
    } else if (go->neg_upap) {
	upap_authpeer(unit);
	auth |= UPAP_PEER;
    }
    if (ho->neg_chap) {
	ChapAuthWithPeer(unit, our_name, ho->chap_mdtype);
	auth |= CHAP_WITHPEER;
    } else if (ho->neg_upap) {
	upap_authwithpeer(unit, user, passwd);
	auth |= UPAP_WITHPEER;
    }
    auth_pending[unit] = auth;

    if (!auth)
	network_phase(unit);
}

/*
 * Proceed to the network phase.
 */
static void
network_phase(unit)
    int unit;
{
    phase = PHASE_NETWORK;
    ipcp_open(unit);
    ccp_open(unit);
}

/*
 * The peer has failed to authenticate himself using `protocol'.
 */
void
auth_peer_fail(unit, protocol)
    int unit, protocol;
{
    /*
     * Authentication failure: take the link down
     */
    lcp_close(unit);
    phase = PHASE_TERMINATE;
}

/*
 * The peer has been successfully authenticated using `protocol'.
 */
void
auth_peer_success(unit, protocol)
    int unit, protocol;
{
    int bit;

    switch (protocol) {
    case PPP_CHAP:
	bit = CHAP_PEER;
	break;
    case PPP_PAP:
	bit = UPAP_PEER;
	break;
    default:
	syslog(LOG_WARNING, "auth_peer_success: unknown protocol %x",
	       protocol);
	return;
    }

    /*
     * If there is no more authentication still to be done,
     * proceed to the network phase.
     */
    if ((auth_pending[unit] &= ~bit) == 0) {
	phase = PHASE_NETWORK;
	ipcp_open(unit);
	ccp_open(unit);
    }
}

/*
 * We have failed to authenticate ourselves to the peer using `protocol'.
 */
void
auth_withpeer_fail(unit, protocol)
    int unit, protocol;
{
    /*
     * We've failed to authenticate ourselves to our peer.
     * He'll probably take the link down, and there's not much
     * we can do except wait for that.
     */
}

/*
 * We have successfully authenticated ourselves with the peer using `protocol'.
 */
void
auth_withpeer_success(unit, protocol)
    int unit, protocol;
{
    int bit;

    switch (protocol) {
    case PPP_CHAP:
	bit = CHAP_WITHPEER;
	break;
    case PPP_PAP:
	bit = UPAP_WITHPEER;
	break;
    default:
	syslog(LOG_WARNING, "auth_peer_success: unknown protocol %x",
	       protocol);
	bit = 0;
    }

    /*
     * If there is no more authentication still being done,
     * proceed to the network phase.
     */
    if ((auth_pending[unit] &= ~bit) == 0)
	network_phase(unit);
}


/*
 * check_auth_options - called to check authentication options.
 */
void
check_auth_options()
{
    lcp_options *wo = &lcp_wantoptions[0];
    lcp_options *ao = &lcp_allowoptions[0];

    /* Default our_name to hostname, and user to our_name */
    if (our_name[0] == 0 || usehostname)
	strcpy(our_name, hostname);
    if (user[0] == 0)
	strcpy(user, our_name);

    /* If authentication is required, ask peer for CHAP or PAP. */
    if (auth_required && !wo->neg_chap && !wo->neg_upap) {
	wo->neg_chap = 1;
	wo->neg_upap = 1;
    }

    /*
     * Check whether we have appropriate secrets to use
     * to authenticate ourselves and/or the peer.
     */
    if (ao->neg_upap && passwd[0] == 0 && !get_upap_passwd())
	ao->neg_upap = 0;
    if (wo->neg_upap && !uselogin && !have_upap_secret())
	wo->neg_upap = 0;
    if (ao->neg_chap && !have_chap_secret(our_name, remote_name))
	ao->neg_chap = 0;
    if (wo->neg_chap && !have_chap_secret(remote_name, our_name))
	wo->neg_chap = 0;

    if (auth_required && !wo->neg_chap && !wo->neg_upap) {
	fprintf(stderr, "\
pppd: peer authentication required but no authentication files accessible\n");
	exit(1);
    }

}


/*
 * check_passwd - Check the user name and passwd against the PAP secrets
 * file.  If requested, also check against the system password database,
 * and login the user if OK.
 *
 * returns:
 *	UPAP_AUTHNAK: Authentication failed.
 *	UPAP_AUTHACK: Authentication succeeded.
 * In either case, msg points to an appropriate message.
 */
int
check_passwd(unit, auser, userlen, apasswd, passwdlen, msg, msglen)
    int unit;
    char *auser;
    int userlen;
    char *apasswd;
    int passwdlen;
    char **msg;
    int *msglen;
{
    int ret;
    char *filename;
    FILE *f;
    struct wordlist *addrs;
    char passwd[256], user[256];
    char secret[MAXWORDLEN];
    static int attempts = 0;
    int len;

    /*
     * Make copies of apasswd and auser, then null-terminate them.
     */
    len = MIN(passwdlen, sizeof(passwd) - 1);
    BCOPY(apasswd, passwd, len);
    passwd[len] = '\0';
    len = MIN(userlen, sizeof(user) - 1);
    BCOPY(auser, user, len);
    user[len] = '\0';

    /*
     * Open the file of upap secrets and scan for a suitable secret
     * for authenticating this user.
     */
    filename = _PATH_UPAPFILE;
    addrs = NULL;
    ret = UPAP_AUTHACK;
    f = fopen(filename, "r");
    if (f == NULL) {
	if (!uselogin) {
	    syslog(LOG_ERR, "Can't open upap password file %s: %m", filename);
	    ret = UPAP_AUTHNAK;
	}

    } else {
	check_access(f, filename);
	if (scan_authfile(f, user, our_name, secret, &addrs, filename) < 0
	    || (secret[0] != 0 && (cryptpap || strcmp(passwd, secret) != 0)
		&& strcmp(crypt(passwd, secret), secret) != 0)) {
	    syslog(LOG_WARNING, "upap authentication failure for %s", user);
	    ret = UPAP_AUTHNAK;
	}
	fclose(f);
    }

    if (uselogin && ret == UPAP_AUTHACK) {
	ret = ppplogin(user, passwd, msg, msglen);
	if (ret == UPAP_AUTHNAK) {
	    syslog(LOG_WARNING, "upap login failure for %s", user);
	}
    }

    if (ret == UPAP_AUTHNAK) {
	*msg = "Login incorrect";
	*msglen = strlen(*msg);
	/*
	 * Frustrate passwd stealer programs.
	 * Allow 10 tries, but start backing off after 3 (stolen from login).
	 * On 10'th, drop the connection.
	 */
	if (attempts++ >= 10) {
	    syslog(LOG_WARNING, "%d LOGIN FAILURES ON %s, %s",
		   attempts, devnam, user);
	    quit();
	}
	if (attempts > 3)
	    sleep((u_int) (attempts - 3) * 5);
	if (addrs != NULL)
	    free_wordlist(addrs);

    } else {
	attempts = 0;			/* Reset count */
	*msg = "Login ok";
	*msglen = strlen(*msg);
	if (addresses[unit] != NULL)
	    free_wordlist(addresses[unit]);
	addresses[unit] = addrs;
    }

    return ret;
}


/*
 * ppplogin - Check the user name and password against the system
 * password database, and login the user if OK.
 *
 * returns:
 *	UPAP_AUTHNAK: Login failed.
 *	UPAP_AUTHACK: Login succeeded.
 * In either case, msg points to an appropriate message.
 */
static int
ppplogin(user, passwd, msg, msglen)
    char *user;
    char *passwd;
    char **msg;
    int *msglen;
{
    struct utmp utmp;
    struct passwd *pw;
    struct timeval tp;
    char *epasswd;
    char *tty;

#ifdef HAS_SHADOW
    struct spwd *spwd;
    struct spwd *getspnam();
#endif

    if ((pw = getpwnam(user)) == NULL) {
	return (UPAP_AUTHNAK);
    }

#ifdef HAS_SHADOW
    if ((spwd = getspnam(user)) == NULL) {
        pw->pw_passwd = "";
    } else {
	pw->pw_passwd = spwd->sp_pwdp;
    }
#endif

    /*
     * XXX If no passwd, let them login without one.
     */
    if (pw->pw_passwd == '\0') {
	return (UPAP_AUTHACK);
    }

#ifdef HAS_SHADOW
    if ((pw->pw_passwd && pw->pw_passwd[0] == '@'
	 && pw_auth (pw->pw_passwd+1, pw->pw_name, PW_PPP, NULL))
	|| !valid (passwd, pw)) {
	return (UPAP_AUTHNAK);
    }
#else
    epasswd = crypt(passwd, pw->pw_passwd);
    if (strcmp(epasswd, pw->pw_passwd)) {
	return (UPAP_AUTHNAK);
    }
#endif

    if (pw->pw_expire) {
	(void)gettimeofday(&tp, (struct timezone *)NULL);
	if (tp.tv_sec >= pw->pw_expire) {
	    syslog(LOG_INFO, "user %s account expired", user);
	    return (UPAP_AUTHNAK);
	}
    }
    syslog(LOG_INFO, "user %s logged in", user);

    /*
     * Write a wtmp entry for this user.
     */
    tty = devnam;
    if (strncmp(tty, "/dev/", 5) == 0)
	tty += 5;

    logwtmp(tty, user, ":PPP");		/* Add wtmp login entry */
    logged_in = TRUE;

    /* Log in utmp too */
    memset((void *)&utmp, 0, sizeof(utmp));
    (void)time(&utmp.ut_time);
    (void)strncpy(utmp.ut_name, user, sizeof(utmp.ut_name));
    (void)strncpy(utmp.ut_host, ":PPP", sizeof(utmp.ut_host));
    (void)strncpy(utmp.ut_line, tty, sizeof(utmp.ut_line));
    login(&utmp);

    return (UPAP_AUTHACK);
}

/*
 * ppplogout - Logout the user.
 */
static void
ppplogout()
{
    char *tty;

    tty = devnam;
    if (strncmp(tty, "/dev/", 5) == 0)
	tty += 5;
    logwtmp(tty, "", "");		/* Wipe out wtmp logout entry */
    logged_in = FALSE;

    logout(tty);			/* Wipe out utmp */
}


/*
 * null_login - Check if a username of "" and a password of "" are
 * acceptable, and iff so, set the list of acceptable IP addresses
 * and return 1.
 */
static int
null_login(unit)
    int unit;
{
    char *filename;
    FILE *f;
    int i, ret;
    struct wordlist *addrs;
    char secret[MAXWORDLEN];

    /*
     * Open the file of upap secrets and scan for a suitable secret.
     * We don't accept a wildcard client.
     */
    filename = _PATH_UPAPFILE;
    addrs = NULL;
    f = fopen(filename, "r");
    if (f == NULL)
	return 0;
    check_access(f, filename);

    i = scan_authfile(f, "", our_name, secret, &addrs, filename);
    ret = i >= 0 && (i & NONWILD_CLIENT) != 0 && secret[0] == 0;

    if (ret) {
	if (addresses[unit] != NULL)
	    free_wordlist(addresses[unit]);
	addresses[unit] = addrs;
    }

    fclose(f);
    return ret;
}


/*
 * get_upap_passwd - get a password for authenticating ourselves with
 * our peer using PAP.  Returns 1 on success, 0 if no suitable password
 * could be found.
 */
static int
get_upap_passwd()
{
    char *filename;
    FILE *f;
    struct wordlist *addrs;
    char secret[MAXWORDLEN];

    filename = _PATH_UPAPFILE;
    addrs = NULL;
    f = fopen(filename, "r");
    if (f == NULL)
	return 0;
    check_access(f, filename);
    if (scan_authfile(f, user, remote_name, secret, NULL, filename) < 0)
	return 0;
    strncpy(passwd, secret, MAXSECRETLEN);
    passwd[MAXSECRETLEN-1] = 0;
    return 1;
}


/*
 * have_upap_secret - check whether we have a PAP file with any
 * secrets that we could possibly use for authenticating the peer.
 */
static int
have_upap_secret()
{
    FILE *f;
    int ret;
    char *filename;

    filename = _PATH_UPAPFILE;
    f = fopen(filename, "r");
    if (f == NULL)
	return 0;

    ret = scan_authfile(f, NULL, our_name, NULL, NULL, filename);
    fclose(f);
    if (ret < 0)
	return 0;

    return 1;
}


/*
 * have_chap_secret - check whether we have a CHAP file with a
 * secret that we could possibly use for authenticating `client'
 * on `server'.  Either can be the null string, meaning we don't
 * know the identity yet.
 */
static int
have_chap_secret(client, server)
    char *client;
    char *server;
{
    FILE *f;
    int ret;
    char *filename;

    filename = _PATH_CHAPFILE;
    f = fopen(filename, "r");
    if (f == NULL)
	return 0;

    if (client[0] == 0)
	client = NULL;
    else if (server[0] == 0)
	server = NULL;

    ret = scan_authfile(f, client, server, NULL, NULL, filename);
    fclose(f);
    if (ret < 0)
	return 0;

    return 1;
}


/*
 * get_secret - open the CHAP secret file and return the secret
 * for authenticating the given client on the given server.
 * (We could be either client or server).
 */
int
get_secret(unit, client, server, secret, secret_len, save_addrs)
    int unit;
    char *client;
    char *server;
    char *secret;
    int *secret_len;
    int save_addrs;
{
    FILE *f;
    int ret, len;
    char *filename;
    struct wordlist *addrs;
    char secbuf[MAXWORDLEN];

    filename = _PATH_CHAPFILE;
    addrs = NULL;
    secbuf[0] = 0;

    f = fopen(filename, "r");
    if (f == NULL) {
	syslog(LOG_ERR, "Can't open chap secret file %s: %m", filename);
	return 0;
    }
    check_access(f, filename);

    ret = scan_authfile(f, client, server, secbuf, &addrs, filename);
    fclose(f);
    if (ret < 0)
	return 0;

    if (save_addrs) {
	if (addresses[unit] != NULL)
	    free_wordlist(addresses[unit]);
	addresses[unit] = addrs;
    }

    len = strlen(secbuf);
    if (len > MAXSECRETLEN) {
	syslog(LOG_ERR, "Secret for %s on %s is too long", client, server);
	len = MAXSECRETLEN;
    }
    BCOPY(secbuf, secret, len);
    *secret_len = len;

    return 1;
}

/*
 * auth_ip_addr - check whether the peer is authorized to use
 * a given IP address.  Returns 1 if authorized, 0 otherwise.
 */
int
auth_ip_addr(unit, addr)
    int unit;
    u_int32_t addr;
{
    u_int32_t a;
    struct hostent *hp;
    struct wordlist *addrs;

    /* don't allow loopback or multicast address */
    if (bad_ip_adrs(addr))
	return 0;

    if ((addrs = addresses[unit]) == NULL)
	return 1;		/* no restriction */

    for (; addrs != NULL; addrs = addrs->next) {
	/* "-" means no addresses authorized */
	if (strcmp(addrs->word, "-") == 0)
	    break;
	if ((a = inet_addr(addrs->word)) == -1) {
	    if ((hp = gethostbyname(addrs->word)) == NULL) {
		syslog(LOG_WARNING, "unknown host %s in auth. address list",
		       addrs->word);
		continue;
	    } else
		a = *(u_int32_t *)hp->h_addr;
	}
	if (addr == a)
	    return 1;
    }
    return 0;			/* not in list => can't have it */
}

/*
 * bad_ip_adrs - return 1 if the IP address is one we don't want
 * to use, such as an address in the loopback net or a multicast address.
 * addr is in network byte order.
 */
int
bad_ip_adrs(addr)
    u_int32_t addr;
{
    addr = ntohl(addr);
    return (addr >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET
	|| IN_MULTICAST(addr) || IN_BADCLASS(addr);
}

/*
 * check_access - complain if a secret file has too-liberal permissions.
 */
void
check_access(f, filename)
    FILE *f;
    char *filename;
{
    struct stat sbuf;

    if (fstat(fileno(f), &sbuf) < 0) {
	syslog(LOG_WARNING, "cannot stat secret file %s: %m", filename);
    } else if ((sbuf.st_mode & (S_IRWXG | S_IRWXO)) != 0) {
	syslog(LOG_WARNING, "Warning - secret file %s has world and/or group access", filename);
    }
}


/*
 * scan_authfile - Scan an authorization file for a secret suitable
 * for authenticating `client' on `server'.  The return value is -1
 * if no secret is found, otherwise >= 0.  The return value has
 * NONWILD_CLIENT set if the secret didn't have "*" for the client, and
 * NONWILD_SERVER set if the secret didn't have "*" for the server.
 * Any following words on the line (i.e. address authorization
 * info) are placed in a wordlist and returned in *addrs.  
 */
static int
scan_authfile(f, client, server, secret, addrs, filename)
    FILE *f;
    char *client;
    char *server;
    char *secret;
    struct wordlist **addrs;
    char *filename;
{
    int newline, xxx;
    int got_flag, best_flag;
    FILE *sf;
    struct wordlist *ap, *addr_list, *addr_last;
    char word[MAXWORDLEN];
    char atfile[MAXWORDLEN];

    if (addrs != NULL)
	*addrs = NULL;
    addr_list = NULL;
    if (!getword(f, word, &newline, filename))
	return -1;		/* file is empty??? */
    newline = 1;
    best_flag = -1;
    for (;;) {
	/*
	 * Skip until we find a word at the start of a line.
	 */
	while (!newline && getword(f, word, &newline, filename))
	    ;
	if (!newline)
	    break;		/* got to end of file */

	/*
	 * Got a client - check if it's a match or a wildcard.
	 */
	got_flag = 0;
	if (client != NULL && strcmp(word, client) != 0 && !ISWILD(word)) {
	    newline = 0;
	    continue;
	}
	if (!ISWILD(word))
	    got_flag = NONWILD_CLIENT;

	/*
	 * Now get a server and check if it matches.
	 */
	if (!getword(f, word, &newline, filename))
	    break;
	if (newline)
	    continue;
	if (server != NULL && strcmp(word, server) != 0 && !ISWILD(word))
	    continue;
	if (!ISWILD(word))
	    got_flag |= NONWILD_SERVER;

	/*
	 * Got some sort of a match - see if it's better than what
	 * we have already.
	 */
	if (got_flag <= best_flag)
	    continue;

	/*
	 * Get the secret.
	 */
	if (!getword(f, word, &newline, filename))
	    break;
	if (newline)
	    continue;

	/*
	 * Special syntax: @filename means read secret from file.
	 */
	if (word[0] == '@') {
	    strcpy(atfile, word+1);
	    if ((sf = fopen(atfile, "r")) == NULL) {
		syslog(LOG_WARNING, "can't open indirect secret file %s",
		       atfile);
		continue;
	    }
	    check_access(sf, atfile);
	    if (!getword(sf, word, &xxx, atfile)) {
		syslog(LOG_WARNING, "no secret in indirect secret file %s",
		       atfile);
		fclose(sf);
		continue;
	    }
	    fclose(sf);
	}
	if (secret != NULL)
	    strcpy(secret, word);
		
	best_flag = got_flag;

	/*
	 * Now read address authorization info and make a wordlist.
	 */
	if (addr_list)
	    free_wordlist(addr_list);
	addr_list = addr_last = NULL;
	for (;;) {
	    if (!getword(f, word, &newline, filename) || newline)
		break;
	    ap = (struct wordlist *) malloc(sizeof(struct wordlist)
					    + strlen(word));
	    if (ap == NULL)
		novm("authorized addresses");
	    ap->next = NULL;
	    strcpy(ap->word, word);
	    if (addr_list == NULL)
		addr_list = ap;
	    else
		addr_last->next = ap;
	    addr_last = ap;
	}
	if (!newline)
	    break;
    }

    if (addrs != NULL)
	*addrs = addr_list;
    else if (addr_list != NULL)
	free_wordlist(addr_list);

    return best_flag;
}

/*
 * free_wordlist - release memory allocated for a wordlist.
 */
static void
free_wordlist(wp)
    struct wordlist *wp;
{
    struct wordlist *next;

    while (wp != NULL) {
	next = wp->next;
	free(wp);
	wp = next;
    }
}

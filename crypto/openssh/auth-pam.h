/*
 * OpenSSH PAM authentication support.
 *
 * $FreeBSD$
 */
#ifndef AUTH_PAM_H
#define AUTH_PAM_H
#include "includes.h"
#ifdef USE_PAM

#include <pwd.h> /* For struct passwd */

void start_pam(struct passwd *pw);
void finish_pam(void);
int auth_pam_password(struct passwd *pw, const char *password);
char **fetch_pam_environment(void);
int do_pam_account(char *username, char *remote_user);
void do_pam_session(char *username, const char *ttyname);
void do_pam_setcred(void);
void print_pam_messages(void);
int pam_password_change_required(void);
void do_pam_chauthtok(void);

struct inverted_pam_cookie {
    int state;			/* Which state have we reached? */
    pid_t pid;			/* PID of child process */

    /* Only valid in state STATE_CONV */
    int num_msg;		/* Number of messages */
    struct pam_message **msg;	/* Message structures */
    struct pam_response **resp;	/* Response structures */
    struct inverted_pam_userdata *userdata;
};
void ipam_free_cookie(struct inverted_pam_cookie *cookie);
struct inverted_pam_cookie *ipam_start_auth(const char *, const char *);

#endif /* USE_PAM */
#endif /* AUTH_PAM_H */

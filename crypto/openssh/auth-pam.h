/* $Id: auth-pam.h,v 1.12 2002/04/04 19:02:28 stevesk Exp $ */

#include "includes.h"
#ifdef USE_PAM

#include <pwd.h> /* For struct passwd */

void start_pam(const char *user);
void finish_pam(void);
int auth_pam_password(Authctxt *authctxt, const char *password);
char **fetch_pam_environment(void);
int do_pam_authenticate(int flags);
int do_pam_account(char *username, char *remote_user);
void do_pam_session(char *username, const char *ttyname);
void do_pam_setcred(int init);
void print_pam_messages(void);
int is_pam_password_change_required(void);
void do_pam_chauthtok(void);
void do_pam_set_conv(struct pam_conv *);
void message_cat(char **p, const char *a);

#endif /* USE_PAM */

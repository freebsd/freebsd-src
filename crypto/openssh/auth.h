#ifndef AUTH_H
#define AUTH_H

void	do_authentication(void);
void	do_authentication2(void);

struct passwd *
auth_get_user(void);

int allowed_user(struct passwd * pw);;

#define AUTH_FAIL_MAX 6
#define AUTH_FAIL_LOG (AUTH_FAIL_MAX/2)
#define AUTH_FAIL_MSG "Too many authentication failures for %.100s"

#endif


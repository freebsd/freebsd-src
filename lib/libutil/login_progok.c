#include <sys/types.h>
#include <login_cap.h>
#include <pwd.h>
#include <string.h>

int
login_progok(uid_t uid, const char *prog)
{
  login_cap_t *lc;
  const struct passwd *pwd;
  char **data;

  pwd = getpwuid(uid);
  if (!pwd)
    return 0;			/* How did that happen ? - we can't run */

  lc = login_getpwclass(pwd);
  if (!lc)
    return 1;			/* We're missing login.conf ? - we can run */

  data = login_getcaplist(lc, "prog.allow", NULL);
  if (data) 
    for (; *data; data++)
      if (!strcmp(*data, prog)) {
        login_close(lc);
	return 1;		/* We're in prog.allow - we can run */
      }

  data = login_getcaplist(lc, "prog.deny", NULL);
  if (data) 
    for (; *data; data++)
      if (!strcmp(*data, prog)) {
        login_close(lc);
	return 0;		/* We're in prog.deny - we can't run */
      }

  login_close(lc);
  return 1;			/* We're not mentioned anywhere - we can run */
}

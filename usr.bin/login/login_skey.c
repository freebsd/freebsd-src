 /* Portions taken from the skey distribution on Oct 21 1993 */
#ifdef SKEY
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <termios.h>
#include <pwd.h>
#include <syslog.h>

#include <skey.h>

/* skey_getpass - read regular or s/key password */

char   *skey_getpass(prompt, pwd, pwok)
char   *prompt;
struct passwd *pwd;
int     pwok;
{
    static char buf[128];
    struct skey skey;
    char   *cp;
    void    rip();
    struct termios saved_ttymode;
    struct termios noecho_ttymode;
    char   *username = pwd ? pwd->pw_name : "nope";
    int     sflag;

    /* Attempt an s/key challenge. */

    if ((sflag = skeychallenge(&skey, username, buf)) == 0) {
	printf("%s\n", buf);
    }
    if (!pwok) {
	printf("(s/key required)\n");
    }
    fputs(prompt, stdout);
    fflush(stdout);

    /* Save current input modes and turn echo off. */

    tcgetattr(0, &saved_ttymode);
    tcgetattr(0, &noecho_ttymode);
    noecho_ttymode.c_lflag &= ~ECHO;
    tcsetattr(0, TCSANOW, &noecho_ttymode);

    /* Read password. */

    buf[0] = 0;
    fgets(buf, sizeof(buf), stdin);
    rip(buf);

    /* Restore previous input modes. */

    tcsetattr(0, TCSANOW, &saved_ttymode);

    /* Give S/Key users a chance to do it with echo on. */

    if (sflag == 0 && feof(stdin) == 0 && buf[0] == 0) {
	fputs(" (turning echo on)\n", stdout);
	fputs(prompt, stdout);
	fflush(stdout);
	fgets(buf, sizeof(buf), stdin);
	rip(buf);
    } else {
	putchar('\n');
    }
    return (buf);
}

/* skey_crypt - return encrypted UNIX passwd if s/key or regular password ok */

char   *skey_crypt(pp, salt, pwd, pwok)
char   *pp;
char   *salt;
struct passwd *pwd;
int     pwok;
{
    struct skey skey;
    char   *p;
    char   *crypt();

    /* Try s/key authentication even when the UNIX password is permitted. */

    if (pwd != 0 && skeylookup(&skey, pwd->pw_name) == 0
	&& skeyverify(&skey, pp) == 0) {
	/* s/key authentication succeeded */
	if (skey.n < 5)
	    printf("Warning! Change s/key password soon\n");
	return (pwd->pw_passwd);
    }

    /* When s/key authentication does not work, always invoke crypt(). */

    p = crypt(pp, salt);
    if (pwok && pwd != 0 && strcmp(p, pwd->pw_passwd) == 0)
	return (pwd->pw_passwd);

    /* The user does not exist or entered bad input. */

    return (":");
}
#endif SKEY

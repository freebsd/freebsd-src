#include <unistd.h>
#include <stdio.h>
#include <skey.h>

/* skey_getpass - read regular or s/key password */

char *skey_getpass(prompt, pwd, pwok)
const char *prompt;
struct passwd *pwd;
int     pwok;
{
    static char buf[128];
    struct skey skey;
    char   *pass;
    int     sflag;

    /* Attempt an s/key challenge. */
    sflag = (pwd == NULL || skeyinfo(&skey, pwd->pw_name, buf));
    if (!sflag) {
	printf("%s\n", buf);
	if (!pwok)
	    printf("(s/key required)\n");
    }

    pass = getpass(prompt);

    /* Give S/Key users a chance to do it with echo on. */
    if (!sflag && !feof(stdin) && *pass == '\0') {
	fputs(" (turning echo on)\n", stdout);
	fputs(prompt, stdout);
	fflush(stdout);
	fgets(buf, sizeof(buf), stdin);
	rip(buf);
	return (buf);
    } else
	return (pass);
}

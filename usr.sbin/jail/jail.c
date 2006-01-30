/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/jail.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <grp.h>
#include <login_cap.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void	usage(void);
extern char	**environ;

#define GET_USER_INFO do {						\
	pwd = getpwnam(username);					\
	if (pwd == NULL) {						\
		if (errno)						\
			err(1, "getpwnam: %s", username);		\
		else							\
			errx(1, "%s: no such user", username);		\
	}								\
	lcap = login_getpwclass(pwd);					\
	if (lcap == NULL)						\
		err(1, "getpwclass: %s", username);			\
	ngroups = NGROUPS;						\
	if (getgrouplist(username, pwd->pw_gid, groups, &ngroups) != 0)	\
		err(1, "getgrouplist: %s", username);			\
} while (0)

int
main(int argc, char **argv)
{
	login_cap_t *lcap = NULL;
	struct jail j;
	struct passwd *pwd = NULL;
	struct in_addr in;
	gid_t groups[NGROUPS];
	int ch, i, iflag, Jflag, lflag, ngroups, uflag, Uflag;
	char path[PATH_MAX], *username, *JidFile;
	static char *cleanenv;
	const char *shell, *p = NULL;
	FILE *fp;

	iflag = Jflag = lflag = uflag = Uflag = 0;
	username = JidFile = cleanenv = NULL;
	fp = NULL;

	while ((ch = getopt(argc, argv, "ilu:U:J:")) != -1) {
		switch (ch) {
		case 'i':
			iflag = 1;
			break;
		case 'J':
			JidFile = optarg;
			Jflag = 1;
			break;
		case 'u':
			username = optarg;
			uflag = 1;
			break;
		case 'U':
			username = optarg;
			Uflag = 1;
			break;
		case 'l':
			lflag = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc < 4)
		usage();
	if (uflag && Uflag)
		usage();
	if (lflag && username == NULL)
		usage();
	if (uflag)
		GET_USER_INFO;
	if (realpath(argv[0], path) == NULL)
		err(1, "realpath: %s", argv[0]);
	if (chdir(path) != 0)
		err(1, "chdir: %s", path);
	memset(&j, 0, sizeof(j));
	j.version = 0;
	j.path = path;
	j.hostname = argv[1];
	if (inet_aton(argv[2], &in) == 0)
		errx(1, "Could not make sense of ip-number: %s", argv[2]);
	j.ip_number = ntohl(in.s_addr);
	if (Jflag) {
		fp = fopen(JidFile, "w");
		if (fp == NULL)
			errx(1, "Could not create JidFile: %s", JidFile);
	}
	i = jail(&j);
	if (i == -1)
		err(1, "jail");
	if (iflag) {
		printf("%d\n", i);
		fflush(stdout);
	}
	if (Jflag) {
		if (fp != NULL) {
			fprintf(fp, "%d\t%s\t%s\t%s\t%s\n",
			    i, j.path, j.hostname, argv[2], argv[3]);
			(void)fclose(fp);
		} else {
			errx(1, "Could not write JidFile: %s", JidFile);
		}
	}
	if (username != NULL) {
		if (Uflag)
			GET_USER_INFO;
		if (lflag) {
			p = getenv("TERM");
			environ = &cleanenv;
		}
		if (setgroups(ngroups, groups) != 0)
			err(1, "setgroups");
		if (setgid(pwd->pw_gid) != 0)
			err(1, "setgid");
		if (setusercontext(lcap, pwd, pwd->pw_uid,
		    LOGIN_SETALL & ~LOGIN_SETGROUP) != 0)
			err(1, "setusercontext");
		login_close(lcap);
	}
	if (lflag) {
		if (*pwd->pw_shell)
			shell = pwd->pw_shell;
		else
			shell = _PATH_BSHELL;
		if (chdir(pwd->pw_dir) < 0)
			errx(1, "no home directory");
		setenv("HOME", pwd->pw_dir, 1);
		setenv("SHELL", shell, 1);
		setenv("USER", pwd->pw_name, 1);
		if (p)
			setenv("TERM", p, 1);
	}
	if (execv(argv[3], argv + 3) != 0)
		err(1, "execv: %s", argv[3]);
	exit(0);
}

static void
usage(void)
{

	(void)fprintf(stderr, "%s%s\n",
	     "usage: jail [-i] [-J jid_file] [-l -u username | -U username]",
	     " path hostname ip-number command ...");
	exit(1);
}

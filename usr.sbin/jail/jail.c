/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 * 
 * $FreeBSD$
 * 
 */

#include <sys/param.h>
#include <sys/jail.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <err.h>
#include <grp.h>
#include <login_cap.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void	usage(void);

int
main(int argc, char **argv)
{
	login_cap_t *lcap;
	struct jail j;
	struct passwd *pwd;
	struct in_addr in;
	int ch, groups[NGROUPS], i, ngroups;
	char *username;

	username = NULL;

	while ((ch = getopt(argc, argv, "u:")) != -1)
		switch (ch) {
		case 'u':
			username = optarg;
			break;
		default:
			usage();
			break;
		}
	argc -= optind;
	argv += optind;
	if (argc < 4)
		usage();

	if (username != NULL) {
		pwd = getpwnam(username);
		if (pwd == NULL)
			err(1, "getpwnam %s", username);
		lcap = login_getpwclass(pwd);
		if (lcap == NULL)
			err(1, "getpwclass failed", username);
		ngroups = NGROUPS;
		i = getgrouplist(username, pwd->pw_gid, groups, &ngroups);
		if (i)
			err(1, "getgrouplist %s", username);
	}
	i = chdir(argv[0]);
	if (i)
		err(1, "chdir %s", argv[0]);
	memset(&j, 0, sizeof(j));
	j.version = 0;
	j.path = argv[0];
	j.hostname = argv[1];
	i = inet_aton(argv[2], &in);
	if (!i)
		errx(1, "Couldn't make sense of ip-number\n");
	j.ip_number = ntohl(in.s_addr);
	i = jail(&j);
	if (i)
		err(1, "Imprisonment failed");
	if (username != NULL) {
		i = setgroups(ngroups, groups);
		if (i)
			err(1, "setgroups failed");
		i = setgid(pwd->pw_gid);
		if (i)
			err(1, "setgid failed");
		i = setusercontext(lcap, pwd, pwd->pw_uid,
		    LOGIN_SETALL & ~LOGIN_SETGROUP);
		if (i)
			err(1, "setusercontext failed");
	}
	i = execv(argv[3], argv + 3);
	if (i)
		err(1, "execv(%s)", argv[3]);
	exit (0);
}

static void
usage(void)
{

	errx(1,
	    "Usage: jail [-u username] path hostname ip-number command ...");
}

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
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <err.h>
#include <errno.h>
#include <grp.h>
#include <login_cap.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>

static void		usage(void);
static int		add_addresses(struct addrinfo *);
static struct in_addr	*copy_addr4(void);
#ifdef INET6
static struct in6_addr	*copy_addr6(void);
#endif

extern char	**environ;

struct addr4entry {
	STAILQ_ENTRY(addr4entry)	addr4entries;
	struct in_addr			ip4;
	int				count;
};
struct addr6entry {
	STAILQ_ENTRY(addr6entry)	addr6entries;
#ifdef INET6
	struct in6_addr			ip6;
#endif
	int				count;
};
STAILQ_HEAD(addr4head, addr4entry) addr4 = STAILQ_HEAD_INITIALIZER(addr4);
STAILQ_HEAD(addr6head, addr6entry) addr6 = STAILQ_HEAD_INITIALIZER(addr6);

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
	gid_t groups[NGROUPS];
	int ch, error, i, ngroups, securelevel;
	int hflag, iflag, Jflag, lflag, uflag, Uflag;
	char path[PATH_MAX], *jailname, *ep, *username, *JidFile, *ip;
	static char *cleanenv;
	const char *shell, *p = NULL;
	long ltmp;
	FILE *fp;
	struct addrinfo hints, *res0;

	hflag = iflag = Jflag = lflag = uflag = Uflag = 0;
	securelevel = -1;
	jailname = username = JidFile = cleanenv = NULL;
	fp = NULL;

	while ((ch = getopt(argc, argv, "hiln:s:u:U:J:")) != -1) {
		switch (ch) {
		case 'h':
			hflag = 1;
			break;
		case 'i':
			iflag = 1;
			break;
		case 'J':
			JidFile = optarg;
			Jflag = 1;
			break;
		case 'n':
			jailname = optarg;
			break;
		case 's':
			ltmp = strtol(optarg, &ep, 0);
			if (*ep || ep == optarg || ltmp > INT_MAX || !ltmp)
				errx(1, "invalid securelevel: `%s'", optarg);
			securelevel = ltmp;
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
	/* Initialize struct jail. */
	memset(&j, 0, sizeof(j));
	j.version = JAIL_API_VERSION;
	j.path = path;
	j.hostname = argv[1];
	if (jailname != NULL)
		j.jailname = jailname;

	/* Handle IP addresses. If requested resolve hostname too. */
	bzero(&hints, sizeof(struct addrinfo));
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_socktype = SOCK_STREAM;
	if (JAIL_API_VERSION < 2)
		hints.ai_family = PF_INET;
	else
		hints.ai_family = PF_UNSPEC;
	/* Handle hostname. */
	if (hflag != 0) {
		error = getaddrinfo(j.hostname, NULL, &hints, &res0);
		if (error != 0)
			errx(1, "failed to handle hostname: %s",
			    gai_strerror(error));
		error = add_addresses(res0);
		freeaddrinfo(res0);
		if (error != 0)
			errx(1, "failed to add addresses.");
	}
	/* Handle IP addresses. */
	hints.ai_flags = AI_NUMERICHOST;
	ip = strtok(argv[2], ",");
	while (ip != NULL) {
		error = getaddrinfo(ip, NULL, &hints, &res0);
		if (error != 0)
			errx(1, "failed to handle ip: %s", gai_strerror(error));
		error = add_addresses(res0);
		freeaddrinfo(res0);
		if (error != 0)
			errx(1, "failed to add addresses.");
		ip = strtok(NULL, ",");
	}
	/* Count IP addresses and add them to struct jail. */
	if (!STAILQ_EMPTY(&addr4)) {
		j.ip4s = STAILQ_FIRST(&addr4)->count;
		j.ip4 = copy_addr4();
		if (j.ip4s > 0 && j.ip4 == NULL)
			errx(1, "copy_addr4()");
	}
#ifdef INET6
	if (!STAILQ_EMPTY(&addr6)) {
		j.ip6s = STAILQ_FIRST(&addr6)->count;
		j.ip6 = copy_addr6();
		if (j.ip6s > 0 && j.ip6 == NULL)
			errx(1, "copy_addr6()");
	}
#endif 

	if (Jflag) {
		fp = fopen(JidFile, "w");
		if (fp == NULL)
			errx(1, "Could not create JidFile: %s", JidFile);
	}
	i = jail(&j);
	if (i == -1)
		err(1, "syscall failed with");
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
	if (securelevel > 0) {
		if (sysctlbyname("kern.securelevel", NULL, 0, &securelevel,
		    sizeof(securelevel)))
			err(1, "Can not set securelevel to %d", securelevel);
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
		    LOGIN_SETALL & ~LOGIN_SETGROUP & ~LOGIN_SETLOGIN) != 0)
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

	(void)fprintf(stderr, "%s%s%s\n",
	     "usage: jail [-hi] [-n jailname] [-J jid_file] ",
	     "[-s securelevel] [-l -u username | -U username] ",
	     "path hostname [ip[,..]] command ...");
	exit(1);
}

static int
add_addresses(struct addrinfo *res0)
{
	int error;
	struct addrinfo *res;
	struct addr4entry *a4p;
	struct sockaddr_in *sai;
#ifdef INET6
	struct addr6entry *a6p;
	struct sockaddr_in6 *sai6;
#endif
	int count;

	error = 0;
	for (res = res0; res && error == 0; res = res->ai_next) {
		switch (res->ai_family) {
		case AF_INET:
			sai = (struct sockaddr_in *)(void *)res->ai_addr;
			STAILQ_FOREACH(a4p, &addr4, addr4entries) {
			    if (bcmp(&sai->sin_addr, &a4p->ip4,
				sizeof(struct in_addr)) == 0) {
				    err(1, "Ignoring duplicate IPv4 address.");
				    break;
			    }
			}
			a4p = (struct addr4entry *) malloc(
			    sizeof(struct addr4entry));
			if (a4p == NULL) {
				error = 1;
				break;
			}
			bzero(a4p, sizeof(struct addr4entry));
			bcopy(&sai->sin_addr, &a4p->ip4,
			    sizeof(struct in_addr));
			if (!STAILQ_EMPTY(&addr4))
				count = STAILQ_FIRST(&addr4)->count;
			else
				count = 0;
			STAILQ_INSERT_TAIL(&addr4, a4p, addr4entries);
			STAILQ_FIRST(&addr4)->count = count + 1;
			break;
#ifdef INET6
		case AF_INET6:
			sai6 = (struct sockaddr_in6 *)(void *)res->ai_addr;
			STAILQ_FOREACH(a6p, &addr6, addr6entries) {
			    if (bcmp(&sai6->sin6_addr, &a6p->ip6,
				sizeof(struct in6_addr)) == 0) {
				    err(1, "Ignoring duplicate IPv6 address.");
				    break;
			    }
			}
			a6p = (struct addr6entry *) malloc(
			    sizeof(struct addr6entry));
			if (a6p == NULL) {
				error = 1;
				break;
			}
			bzero(a6p, sizeof(struct addr6entry));
			bcopy(&sai6->sin6_addr, &a6p->ip6,
			    sizeof(struct in6_addr));
			if (!STAILQ_EMPTY(&addr6))
				count = STAILQ_FIRST(&addr6)->count;
			else
				count = 0;
			STAILQ_INSERT_TAIL(&addr6, a6p, addr6entries);
			STAILQ_FIRST(&addr6)->count = count + 1;
			break;
#endif
		default:
			err(1, "Address family %d not supported. Ignoring.\n",
			    res->ai_family);
			break;
		}
	}

	return (error);
}

static struct in_addr *
copy_addr4(void)
{
	size_t len;
	struct in_addr *ip4s, *p, ia;
	struct addr4entry *a4p;

	if (STAILQ_EMPTY(&addr4))
		return NULL;

	len = STAILQ_FIRST(&addr4)->count * sizeof(struct in_addr);

	ip4s = p = (struct in_addr *)malloc(len);
	if (ip4s == NULL)
	return (NULL);

	bzero(p, len);

	while (!STAILQ_EMPTY(&addr4)) {
		a4p = STAILQ_FIRST(&addr4);
		STAILQ_REMOVE_HEAD(&addr4, addr4entries);
		ia.s_addr = a4p->ip4.s_addr;
		bcopy(&ia, p, sizeof(struct in_addr));
		p++;
		free(a4p);
	}

	return (ip4s);
}

#ifdef INET6
static struct in6_addr *
copy_addr6(void)
{
	size_t len;
	struct in6_addr *ip6s, *p;
	struct addr6entry *a6p;

	if (STAILQ_EMPTY(&addr6))
		return NULL;

	len = STAILQ_FIRST(&addr6)->count * sizeof(struct in6_addr);

	ip6s = p = (struct in6_addr *)malloc(len);
	if (ip6s == NULL)
		return (NULL);

	bzero(p, len);

	while (!STAILQ_EMPTY(&addr6)) {
		a6p = STAILQ_FIRST(&addr6);
		STAILQ_REMOVE_HEAD(&addr6, addr6entries);
		bcopy(&a6p->ip6, p, sizeof(struct in6_addr));
		p++;
		free(a6p);
	}

	return (ip6s);
}
#endif


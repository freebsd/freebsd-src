/*
 * Initially written by Yar Tikhiy <yar@freebsd.org> in PR 76398.
 * Bug fixes and instrumentation by kib@freebsd.org.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <md5.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

#define NDATA	1000
#define	DELAY	2

static void
hup(int signo __unused)
{
}

static int ndata, seq;

static void
setdata(int n)
{
	ndata = n;
	seq = 0;
}

static char *
getdata(void)
{
	static char databuf[256];
	static char xeof[] = "#";

	if (seq > ndata)
		return (NULL);
	if (seq == ndata) {
		seq++;
		return (xeof);
	}
	sprintf(databuf, "%08d xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n", seq++);
	return (databuf);
}

ATF_TC_WITHOUT_HEAD(eintr_test);
ATF_TC_BODY(eintr_test, tc)
{
	char c, digest0[33], digest[33], *p;
	FILE *fp;
	int i, s[2], total0, total;
	MD5_CTX md5;
	pid_t child;
	struct sigaction sa;

	MD5Init(&md5);
	setdata(NDATA);
	for (total0 = 0; (p = getdata()) != NULL; total0 += strlen(p))
		MD5Update(&md5, p, strlen(p));
	p = MD5End(&md5, digest0);

	sa.sa_handler = hup;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	ATF_REQUIRE(sigaction(SIGHUP, &sa, NULL) == 0);

	ATF_REQUIRE(socketpair(PF_UNIX, SOCK_STREAM, 0, s) == 0);

	switch (child = fork()) {
	case -1:
		atf_tc_fail("fork failed %s", strerror(errno));
		break;

	case 0:
		ATF_REQUIRE((fp = fdopen(s[0], "w")) != NULL);
		close(s[1]);
		setdata(NDATA);
		while ((p = getdata())) {
			for (; *p;) {
				if (fputc(*p, fp) == EOF) {
					if (errno == EINTR) {
						clearerr(fp);
					} else {
						atf_tc_fail("fputc errno %s",
						    strerror(errno));
					}
				} else {
					p++;
				}
			}
		}
		fclose(fp);
		break;

	default:
		close(s[0]);
		ATF_REQUIRE((fp = fdopen(s[1], "r")) != NULL);
		sleep(DELAY);
		ATF_REQUIRE(kill(child, SIGHUP) != -1);
		sleep(DELAY);
		MD5Init(&md5);
		for (total = 0;;) {
			i = fgetc(fp);
			if (i == EOF) {
				if (errno == EINTR) {
					clearerr(fp);
				} else {
					atf_tc_fail("fgetc errno %s",
					    strerror(errno));
				}
				continue;
			}
			total++;
			c = i;
			MD5Update(&md5, &c, 1);
			if (i == '#')
				break;
		}
		MD5End(&md5, digest);
		fclose(fp);
		ATF_REQUIRE_MSG(total == total0,
		    "Total number of bytes read does not match: %d %d",
		    total, total0);
		ATF_REQUIRE_MSG(strcmp(digest, digest0) == 0,
		    "Digests do not match %s %s", digest, digest0);
		break;
	}
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, eintr_test);
	return (atf_no_error());
}

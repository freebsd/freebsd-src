
/*
 * Copyright (C) 2012 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <net/if.h>

#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <signal.h>

#include "netinet/ip_compat.h"
#include "netinet/ip_fil.h"
#include "netinet/ip_state.h"
#include "netinet/ip_nat.h"
#include "netinet/ip_sync.h"

int	main(int, char *[]);
void	usage(const char *progname);

int	terminate = 0;

void usage(const char *progname) {
	fprintf(stderr,
		"Usage: %s <destination IP> <destination port> [remote IP]\n",
		progname);
}

#if 0
static void handleterm(int sig)
{
	terminate = sig;
}
#endif

#define BUFFERLEN 1400

int main(argc, argv)
	int argc;
	char *argv[];
{
	int nfd = -1 , lfd = -1;
	int n1, n2, n3, magic, len, inbuf;
	struct sockaddr_in sin;
	struct sockaddr_in in;
	char buff[BUFFERLEN];
	synclogent_t *sl;
	syncupdent_t *su;
	synchdr_t *sh;
	char *progname;

	progname = strrchr(argv[0], '/');
	if (progname) {
		progname++;
	} else {
		progname = argv[0];
	}

	if (argc < 2) {
		usage(progname);
		exit(1);
	}

#if 0
       	signal(SIGHUP, handleterm);
       	signal(SIGINT, handleterm);
       	signal(SIGTERM, handleterm);
#endif

	openlog(progname, LOG_PID, LOG_SECURITY);

	lfd = open(IPSYNC_NAME, O_WRONLY);
	if (lfd == -1) {
		syslog(LOG_ERR, "Opening %s :%m", IPSYNC_NAME);
		exit(1);
	}

	bzero((char *)&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	if (argc > 1)
		sin.sin_addr.s_addr = inet_addr(argv[1]);
	if (argc > 2)
		sin.sin_port = htons(atoi(argv[2]));
	else
		sin.sin_port = htons(43434);
	if (argc > 3)
		in.sin_addr.s_addr = inet_addr(argv[3]);
	else
		in.sin_addr.s_addr = 0;
	in.sin_port = 0;

	while(1) {

		if (lfd != -1)
			close(lfd);
		if (nfd != -1)
			close(nfd);

		lfd = open(IPSYNC_NAME, O_WRONLY);
		if (lfd == -1) {
			syslog(LOG_ERR, "Opening %s :%m", IPSYNC_NAME);
			goto tryagain;
		}

		nfd = socket(AF_INET, SOCK_DGRAM, 0);
		if (nfd == -1) {
			syslog(LOG_ERR, "Socket :%m");
			goto tryagain;
		}

	        n1 = 1;
                setsockopt(nfd, SOL_SOCKET, SO_REUSEADDR, &n1, sizeof(n1));

		if (bind(nfd, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
			syslog(LOG_ERR, "Bind: %m");
			goto tryagain;
		}

		syslog(LOG_INFO, "Listening to %s", inet_ntoa(sin.sin_addr));

		inbuf = 0;
		while (1) {


			/*
			 * XXX currently we do not check the source address
			 * of a datagram, this can be a security risk
			 */
			n1 = read(nfd, buff+inbuf, BUFFERLEN-inbuf);

			printf("header : %d bytes read (header = %d bytes)\n",
			       n1, (int) sizeof(*sh));

			if (n1 < 0) {
				syslog(LOG_ERR, "Read error (header): %m");
				goto tryagain;
			}

			if (n1 == 0) {
				/* XXX can this happen??? */
				syslog(LOG_ERR,
				       "Read error (header) : No data");
				sleep(1);
				continue;
			}

			inbuf += n1;

moreinbuf:
			if (inbuf < sizeof(*sh)) {
				continue; /* need more data */
			}

			sh = (synchdr_t *)buff;
			len = ntohl(sh->sm_len);
			magic = ntohl(sh->sm_magic);

			if (magic != SYNHDRMAGIC) {
				syslog(LOG_ERR, "Invalid header magic %x",
				       magic);
				goto tryagain;
			}

#define IPSYNC_DEBUG
#ifdef IPSYNC_DEBUG
			printf("v:%d p:%d len:%d magic:%x", sh->sm_v,
			       sh->sm_p, len, magic);

			if (sh->sm_cmd == SMC_CREATE)
				printf(" cmd:CREATE");
			else if (sh->sm_cmd == SMC_UPDATE)
				printf(" cmd:UPDATE");
			else
				printf(" cmd:Unknown(%d)", sh->sm_cmd);

			if (sh->sm_table == SMC_NAT)
				printf(" table:NAT");
			else if (sh->sm_table == SMC_STATE)
				printf(" table:STATE");
			else
				printf(" table:Unknown(%d)", sh->sm_table);

			printf(" num:%d\n", (u_32_t)ntohl(sh->sm_num));
#endif

			if (inbuf < sizeof(*sh) + len) {
				continue; /* need more data */
				goto tryagain;
			}

#ifdef IPSYNC_DEBUG
			if (sh->sm_cmd == SMC_CREATE) {
				sl = (synclogent_t *)buff;

			} else if (sh->sm_cmd == SMC_UPDATE) {
				su = (syncupdent_t *)buff;
				if (sh->sm_p == IPPROTO_TCP) {
					printf(" TCP Update: age %lu state %d/%d\n",
					       su->sup_tcp.stu_age,
					       su->sup_tcp.stu_state[0],
					       su->sup_tcp.stu_state[1]);
				}
			} else {
				printf("Unknown command\n");
			}
#endif

			n2 = sizeof(*sh) + len;
			n3 = write(lfd, buff, n2);
			if (n3 <= 0) {
				syslog(LOG_ERR, "%s: Write error: %m",
				       IPSYNC_NAME);
				goto tryagain;
			}


			if (n3 != n2) {
				syslog(LOG_ERR, "%s: Incomplete write (%d/%d)",
				       IPSYNC_NAME, n3, n2);
				goto tryagain;
			}

			/* signal received? */
			if (terminate)
				break;

			/* move buffer to the front,we might need to make
			 * this more efficient, by using a rolling pointer
			 * over the buffer and only copying it, when
			 * we are reaching the end
			 */
			inbuf -= n2;
			if (inbuf) {
				bcopy(buff+n2, buff, inbuf);
				printf("More data in buffer\n");
				goto moreinbuf;
			}
		}

		if (terminate)
			break;
tryagain:
		sleep(1);
	}


	/* terminate */
	if (lfd != -1)
		close(lfd);
	if (nfd != -1)
		close(nfd);

	syslog(LOG_ERR, "signal %d received, exiting...", terminate);

	exit(1);
}

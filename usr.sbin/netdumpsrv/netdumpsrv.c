/*-
 * Copyright (c) 2005-2011 Sandvine Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/errno.h>
#include <sys/kerneldump.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/netdump.h>

#include <fcntl.h>
#include <libutil.h>
#include <netdb.h>
#include <syslog.h>
#include <unistd.h>

#include <assert.h>
#include <inttypes.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define	MAX_DUMPS	256	/* Dumps per IP before to be cleaned out. */
#define	CLIENT_TIMEOUT	600	/* Clients timeout (secs). */
#define	CLIENT_TPASS	10	/* Clients timeout pass (secs). */

#define	PFLAGS_ABIND	0x01
#define	PFLAGS_DDIR	0x02
#define	PFLAGS_DEBUG	0x04
#define	PFLAGS_SCRIPT	0x08

#define	LOGERR(m, ...)							\
	(*phook)(LOG_ERR | LOG_DAEMON, (m), ## __VA_ARGS__)
#define	LOGERR_PERROR(m)						\
	(*phook)(LOG_ERR | LOG_DAEMON, "%s: %s\n", m, strerror(errno))
#define	LOGINFO(m, ...)							\
	(*phook)(LOG_INFO | LOG_DAEMON, (m), ## __VA_ARGS__)
#define	LOGWARN(m, ...)							\
	(*phook)(LOG_WARNING | LOG_DAEMON, (m), ## __VA_ARGS__)

#define	client_ntoa(cl)							\
	inet_ntoa((cl)->ip)
#define	client_pinfo(cl, f, ...)					\
	fprintf((cl)->infofile, (f), ## __VA_ARGS__)

struct netdump_client {
	char		infofilename[MAXPATHLEN];
	char		corefilename[MAXPATHLEN];
	char		hostname[NI_MAXHOST];
	time_t		last_msg;
	SLIST_ENTRY(netdump_client) iter;
	struct in_addr	ip;
	FILE		*infofile;
	int		corefd;
	int		sock;
	unsigned short	printed_port_warning: 1;
	unsigned short	any_data_rcvd: 1;
};

/* Clients list. */
static SLIST_HEAD(, netdump_client) clients = SLIST_HEAD_INITIALIZER(clients);

/* Program arguments handlers. */
static uint32_t pflags;
static char dumpdir[MAXPATHLEN];
static char *handler_script;
static struct in_addr bindip;

/* Miscellaneous handlers. */
static struct pidfh *pfh;
static time_t now;
static time_t last_timeout_check;
static int do_shutdown;
static int sock;

/* Daemon print functions hook. */
static void (*phook)(int, const char *, ...);

static struct netdump_client	*alloc_client(struct sockaddr_in *sip);
static void		 eventloop(void);
static void		 exec_handler(struct netdump_client *client,
			    const char *reason);
static void		 free_client(struct netdump_client *client);
static void		 handle_finish(struct netdump_client *client,
			    struct netdump_msg *msg);
static void		 handle_herald(struct sockaddr_in *from,
			    struct netdump_client *client,
			    struct netdump_msg *msg);
static void		 handle_kdh(struct netdump_client *client,
			    struct netdump_msg *msg);
static void		 handle_packet(struct netdump_client *client,
			    struct sockaddr_in *from, const char *fromstr,
			    struct netdump_msg *msg);
static void		 handle_timeout(struct netdump_client *client);
static void		 handle_vmcore(struct netdump_client *client,
			    struct netdump_msg *msg);
static void		 phook_printf(int priority, const char *message, ...);
static int		 receive_message(int isock, struct sockaddr_in *from,
			    char *fromstr, size_t fromstrlen,
			    struct netdump_msg *msg);
static void		 send_ack(struct netdump_client *client,
			    struct netdump_msg *msg);
static void		 signal_shutdown(int sig __unused);
static void		 timeout_clients(void);
static void		 usage(const char *cmd);

static void
usage(const char *cmd)
{

	fprintf(stderr, "Usage: %s [-a bind_addr] [-d dump_dir] [-i script]\n",
	    cmd);
}

static void
phook_printf(int priority, const char *message, ...)
{
	va_list ap;

	va_start(ap, message);
	if ((priority & LOG_INFO) != 0) {
		assert((priority & (LOG_WARNING | LOG_ERR)) == 0);
		vprintf(message, ap);
	} else
		vfprintf(stderr, message, ap);
}

static struct netdump_client *
alloc_client(struct sockaddr_in *sip)
{
	struct sockaddr_in saddr;
	struct netdump_client *client;
	struct in_addr *ip;
	char *firstdot;
	int i, ecode, fd, bufsz;

	assert(sip != NULL);

	client = calloc(1, sizeof(*client));
	if (client == NULL) {
		LOGERR_PERROR("calloc()");
		return (NULL);
	}
	ip = &sip->sin_addr;
	bcopy(ip, &client->ip, sizeof(*ip));
	client->corefd = -1;
	client->sock = -1;
	client->last_msg = now;

	ecode = getnameinfo((struct sockaddr *)sip, sip->sin_len,
	    client->hostname, sizeof(client->hostname), NULL, 0, NI_NAMEREQD);
	if (ecode != 0) {

		/* Can't resolve, try with a numeric IP. */
		ecode = getnameinfo((struct sockaddr *)sip, sip->sin_len,
		    client->hostname, sizeof(client->hostname), NULL, 0, 0);
		if (ecode != 0) {
			LOGERR("getnameinfo(): %s\n", gai_strerror(ecode));
			free(client);
			return (NULL);
		}
	} else {

		/* Strip off the domain name */
		firstdot = strchr(client->hostname, '.');
		if (firstdot)
			*firstdot = '\0';
	}

	client->sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (client->sock == -1) {
		LOGERR_PERROR("socket()");
		free(client);
		return (NULL);
	}
	if (fcntl(client->sock, F_SETFL, O_NONBLOCK) == -1) {
		LOGERR_PERROR("fcntl()");
		close(client->sock);
		free(client);
		return (NULL);
	}
	bzero(&saddr, sizeof(saddr));
	saddr.sin_len = sizeof(saddr);
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = bindip.s_addr;
	saddr.sin_port = htons(0);
	if (bind(client->sock, (struct sockaddr *)&saddr, sizeof(saddr))) {
		LOGERR_PERROR("bind()");
		close(client->sock);
		free(client);
		return (NULL);
	}
	bzero(&saddr, sizeof(saddr));
	saddr.sin_len = sizeof(saddr);
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = ip->s_addr;
	saddr.sin_port = htons(NETDUMP_ACKPORT);
	if (connect(client->sock, (struct sockaddr *)&saddr, sizeof(saddr))) {
		LOGERR_PERROR("connect()");
		close(client->sock);
		free(client);
		return (NULL);
	}

	/* It should be enough to hold approximatively twize the chunk size. */
	bufsz = 131072;
	if (setsockopt(client->sock, SOL_SOCKET, SO_RCVBUF, &bufsz,
	    sizeof(bufsz))) {
		LOGERR_PERROR("setsockopt()");
	LOGWARN("May drop packets from %s due to small receive buffer\n",
		    client->hostname);
	}

	/* Try info.host.0 through info.host.255 in sequence. */
	for (i = 0; i < MAX_DUMPS; i++) {
		snprintf(client->infofilename, sizeof(client->infofilename),
		    "%s/info.%s.%d", dumpdir, client->hostname, i);
		snprintf(client->corefilename, sizeof(client->corefilename),
		    "%s/vmcore.%s.%d", dumpdir, client->hostname, i);

		/* Try the info file first. */
		fd = open(client->infofilename, O_WRONLY | O_CREAT | O_EXCL,
		    DEFFILEMODE);
		if (fd == -1) {
			if (errno != EEXIST)
				LOGERR("open(\"%s\"): %s\n",
				    client->infofilename, strerror(errno));
			continue;
		}
		client->infofile = fdopen(fd, "w");
		if (client->infofile == NULL) {
			LOGERR_PERROR("fdopen()");
			close(fd);
			unlink(client->infofilename);
			continue;
		}

		/* Next make the core file. */
		fd = open(client->corefilename, O_RDWR | O_CREAT | O_EXCL,
		    DEFFILEMODE);
		if (fd == -1) {

			/* Failed. Keep the numbers in sync. */
			fclose(client->infofile);
			unlink(client->infofilename);
			client->infofile = NULL;
			if (errno != EEXIST)
				LOGERR("open(\"%s\"): %s\n",
				    client->corefilename, strerror(errno));
			continue;
		}
		client->corefd = fd;
		break;
	}

	if (client->infofile == NULL || client->corefd == -1) {
		LOGERR("Can't create output files for new client %s [%s]\n",
		    client->hostname, client_ntoa(client));
		if (client->infofile)
			fclose(client->infofile);
		if (client->corefd != -1)
			close(client->corefd);
		if (client->sock != -1)
			close(client->sock);
		free(client);
		return (NULL);
	}
	SLIST_INSERT_HEAD(&clients, client, iter);
	return (client);
}

static void
free_client(struct netdump_client *client)
{

	assert(client != NULL);

	/* Remove from the list.  Ignore errors from close() routines. */
	SLIST_REMOVE(&clients, client, netdump_client, iter);
	fclose(client->infofile);
	close(client->corefd);
	close(client->sock);
	free(client);
}

static void
exec_handler(struct netdump_client *client, const char *reason)
{
	int pid;

	assert(client != NULL);

	/* If no script is specified this is a no-op. */
	if ((pflags & PFLAGS_SCRIPT) == 0)
		return;

	pid = fork();

	/*
	 * The function is invoked in critical conditions, thus just exiting
	 * without reporting errors is fine.
	 */
	if (pid == -1) {
		LOGERR_PERROR("fork()");
		return;
	} else if (pid != 0) {
		close(sock);
		pidfile_close(pfh);
		if (execl(handler_script, handler_script, reason,
		    client_ntoa(client), client->hostname,
		    client->infofilename, client->corefilename, NULL) == -1) {
			LOGERR_PERROR("fork()");
			_exit(1);
		}
	}
}

static void
handle_timeout(struct netdump_client *client)
{

	assert(client != NULL);

	LOGINFO("Client %s timed out\n", client_ntoa(client));
	client_pinfo(client, "Dump incomplete: client timed out\n");
	exec_handler(client, "timeout");
	free_client(client);
}

static void
timeout_clients(void)
{
	struct netdump_client *client, *tmp;
    
	/* Only time out clients every 10 seconds. */
	if (now - last_timeout_check < CLIENT_TPASS)
		return;
	last_timeout_check = now;

	/* Traverse the list looking for stale clients. */
	SLIST_FOREACH_SAFE(client, &clients, iter, tmp) {
		if (client->last_msg + CLIENT_TIMEOUT < now) {
			LOGINFO("Timingout with such values: %jd + %jd < %jd\n",
			    (intmax_t)client->last_msg,
			    (intmax_t)CLIENT_TIMEOUT, (intmax_t)now);
			handle_timeout(client);
		}
	}
}

static void
send_ack(struct netdump_client *client, struct netdump_msg *msg)
{
	struct netdump_ack ack;
	int tryagain;
    
	assert(client != NULL && msg != NULL);

	bzero(&ack, sizeof(ack));
	ack.na_seqno = htonl(msg->nm_hdr.mh_seqno);
	do {
		tryagain = 0;
		if (send(client->sock, &ack, sizeof(ack), 0) == -1) {
			if (errno == EINTR) {
				tryagain = 1;
				continue;
			}

			/*
			 * XXX: On EAGAIN, we should probably queue the packet
			 * to be sent when the socket is writable but
			 * that is too much effort, since it is mostly
			 * harmless to wait for the client to retransmit.
			 */
			LOGERR_PERROR("send()");
		}
	} while (tryagain);
}

static void
handle_herald(struct sockaddr_in *from, struct netdump_client *client,
    struct netdump_msg *msg)
{

	assert(from != NULL && msg != NULL);

	if (client != NULL) {
		if (client->any_data_rcvd == 0) {

			/* Must be a retransmit of the herald packet. */
			send_ack(client, msg);
			return;
		}

		/* An old connection must have timed out. Clean it up first. */
		handle_timeout(client);
	}

	client = alloc_client(from);
	if (client == NULL) {
		LOGERR("handle_herald(): new client allocation failure\n");
		return;
	}
	client_pinfo(client, "Dump from %s [%s]\n", client->hostname,
	    client_ntoa(client));
	LOGINFO("New dump from client %s [%s] (to %s)\n", client->hostname,
	    client_ntoa(client), client->corefilename);
	send_ack(client, msg);
}

static void
handle_kdh(struct netdump_client *client, struct netdump_msg *msg)
{
	time_t t;
	uint64_t dumplen;
	struct kerneldumpheader *h;
	int parity_check;

	assert(msg != NULL);

	if (client == NULL)
		return;

	client->any_data_rcvd = 1;
	h = (struct kerneldumpheader *)msg->nm_data;
	if (msg->nm_hdr.mh_len < sizeof(struct kerneldumpheader)) {
		LOGERR("Bad KDH from %s [%s]: packet too small\n",
		    client->hostname, client_ntoa(client));
		client_pinfo(client, "Bad KDH: packet too small\n");
		fflush(client->infofile);
		send_ack(client, msg);
		return;
	}
	parity_check = kerneldump_parity(h);

	/* Make sure all the strings are null-terminated. */
	h->architecture[sizeof(h->architecture) - 1] = '\0';
	h->hostname[sizeof(h->hostname) - 1] = '\0';
	h->versionstring[sizeof(h->versionstring) - 1] = '\0';
	h->panicstring[sizeof(h->panicstring) - 1] = '\0';

	client_pinfo(client, "  Architecture: %s\n", h->architecture);
	client_pinfo(client, "  Architecture version: %d\n",
	    dtoh32(h->architectureversion));
	dumplen = dtoh64(h->dumplength);
	client_pinfo(client, "  Dump length: %lldB (%lld MB)\n",
	    (long long)dumplen, (long long)(dumplen >> 20));
	client_pinfo(client, "  blocksize: %d\n", dtoh32(h->blocksize));
	t = dtoh64(h->dumptime);
	client_pinfo(client, "  Dumptime: %s", ctime(&t));
	client_pinfo(client, "  Hostname: %s\n", h->hostname);
	client_pinfo(client, "  Versionstring: %s", h->versionstring);
	client_pinfo(client, "  Panicstring: %s\n", h->panicstring);
	client_pinfo(client, "  Header parity check: %s\n",
	    parity_check ? "Fail" : "Pass");
	fflush(client->infofile);

	LOGINFO("(KDH from %s [%s])", client->hostname, client_ntoa(client));
	send_ack(client, msg);
}

static void
handle_vmcore(struct netdump_client *client, struct netdump_msg *msg)
{

	assert(msg != NULL);

	if (client == NULL)
		return;

	client->any_data_rcvd = 1;
	if (msg->nm_hdr.mh_seqno % 11523 == 0) {

		/* Approximately every 16MB with MTU of 1500 */
		LOGINFO(".");
	}
	if (pwrite(client->corefd, msg->nm_data, msg->nm_hdr.mh_len,
	    msg->nm_hdr.mh_offset) == -1) {
		LOGERR("pwrite (for client %s [%s]): %s\n", client->hostname,
		    client_ntoa(client), strerror(errno));
		client_pinfo(client,
		    "Dump unsuccessful: write error @ offset %08"PRIx64": %s\n",
		    msg->nm_hdr.mh_offset, strerror(errno));
		exec_handler(client, "error");
		free_client(client);
		return;
	}
	send_ack(client, msg);
}

static void
handle_finish(struct netdump_client *client, struct netdump_msg *msg)
{

	assert(msg != NULL);

	if (client == NULL)
		return;

	LOGINFO("\nCompleted dump from client %s [%s]\n", client->hostname,
	    client_ntoa(client));
	client_pinfo(client, "Dump complete\n");
	send_ack(client, msg);
	exec_handler(client, "success");
	free_client(client);
}


static int
receive_message(int isock, struct sockaddr_in *from, char *fromstr,
    size_t fromstrlen, struct netdump_msg *msg)
{
	socklen_t fromlen;
	ssize_t len;

	assert(from != NULL && fromstr != NULL && msg != NULL);

	bzero(from, sizeof(*from));
	from->sin_family = AF_INET;
	from->sin_len = fromlen = sizeof(*from);
	from->sin_port = 0;
	from->sin_addr.s_addr = INADDR_ANY;

	len = recvfrom(isock, msg, sizeof(*msg), 0, (struct sockaddr *)from,
	    &fromlen);
	if (len == -1) {

		/*
		 * As long as some callers may discard the errors printing
		 * in defined circumstances, leave them the choice and avoid
		 * any error reporting.
		 */
		return (-1);
	}

	snprintf(fromstr, fromstrlen, "%s:%hu", inet_ntoa(from->sin_addr),
	    ntohs(from->sin_port));
	if ((size_t)len < sizeof(struct netdump_msg_hdr)) {
		LOGERR("Ignoring runt packet from %s (got %zu)\n", fromstr,
		    (size_t)len);
		return (0);
	}

	/* Convert byte order. */
	msg->nm_hdr.mh_type = ntohl(msg->nm_hdr.mh_type);
	msg->nm_hdr.mh_seqno = ntohl(msg->nm_hdr.mh_seqno);
	msg->nm_hdr.mh_offset = be64toh(msg->nm_hdr.mh_offset);
	msg->nm_hdr.mh_len = ntohl(msg->nm_hdr.mh_len);

	if ((size_t)len < sizeof(struct netdump_msg_hdr) + msg->nm_hdr.mh_len) {
		LOGERR("Packet too small from %s (got %zu, expected %zu)\n",
		    fromstr, (size_t)len,
		    sizeof(struct netdump_msg_hdr) + msg->nm_hdr.mh_len);
		return (0);
	}
	return (len);
}

static void
handle_packet(struct netdump_client *client, struct sockaddr_in *from,
    const char *fromstr, struct netdump_msg *msg)
{

	assert(from != NULL && fromstr != NULL && msg != NULL);

	if (client != NULL)
		client->last_msg = time(NULL);

	switch (msg->nm_hdr.mh_type) {
	case NETDUMP_HERALD:
		handle_herald(from, client, msg);
		break;
	case NETDUMP_KDH:
		handle_kdh(client, msg);
		break;
	case NETDUMP_VMCORE:
		handle_vmcore(client, msg);
		break;
	case NETDUMP_FINISHED:
		handle_finish(client, msg);
		break;
	default:
		LOGERR("Received unknown message type %d from %s\n",
		    msg->nm_hdr.mh_type, fromstr);
	}
}

static void
eventloop(void)
{
	struct netdump_msg msg;
	char fromstr[INET_ADDRSTRLEN + 6];
	fd_set readfds;
	struct sockaddr_in from;
	struct timeval tv;
	struct netdump_client *client, *tmp;
	int len, maxfd;

	while (do_shutdown == 0) {
		maxfd = sock + 1;
		FD_ZERO(&readfds);
		FD_SET(sock, &readfds);
		SLIST_FOREACH(client, &clients, iter) {
			FD_SET(client->sock, &readfds);
			if (maxfd <= client->sock)
				maxfd = client->sock+1;
		}

		/* So that we time out clients regularly. */
		tv.tv_sec = CLIENT_TPASS;
		tv.tv_usec = 0;
		if (select(maxfd, &readfds, NULL, NULL, &tv) == -1) {
			if (errno == EINTR)
				continue;
			LOGERR_PERROR("select()");

			/*
			 * Errors with select() probably will not go away
			 * with simple retrying.
			 */
			pidfile_remove(pfh);
			exit(1);
		}
		now = time(NULL);
		if (FD_ISSET(sock, &readfds)) {
			len = receive_message(sock, &from, fromstr,
			    sizeof(fromstr), &msg);
			if (len == -1) {
				if (errno == EINTR)
					continue;
				if (errno != EAGAIN) {
					pidfile_remove(pfh);
					LOGERR_PERROR("recvfrom()");
					exit(1);
				}
			} else if (len != 0) {

				/*
				 * With len == 0 the packet was rejected
				 * (probably because it was too small) so just
				 * ignore this case.
				 */

				/* Check if they are on the clients list. */
				SLIST_FOREACH(client, &clients, iter)
					if (client->ip.s_addr ==
					    from.sin_addr.s_addr)
						break;

				/*
				 * Technically, clients should not be
				 * responding on the server port, so client
				 * should be NULL, however, if they insist on
				 * doing so, it's not really going to hurt
				 * anything (except maybe fill up the server
				 * socket's receive buffer), so still
				 * accept it. The only possibly legitimate case
				 * is if there's a new dump starting and the
				 * previous one didn't finish cleanly. Handle
				 * this by suppressing the error on HERALD
				 * packets.
				 */
				if (client != NULL &&
				    msg.nm_hdr.mh_type != NETDUMP_HERALD &&
				    client->printed_port_warning == 0) {
			    LOGWARN("Client %s responding on server port\n",
					    client->hostname);
					client->printed_port_warning = 1;
				}
				handle_packet(client, &from, fromstr, &msg);
			}
		}

		/*
		 * handle_packet() and handle_timeout() may free the client,
		 * handle stale pointers.
		 */
		SLIST_FOREACH_SAFE(client, &clients, iter, tmp) {
			if (FD_ISSET(client->sock, &readfds)) {
				len = receive_message(client->sock, &from,
				    fromstr, sizeof(fromstr), &msg);
				if (len == -1) {
					if (errno == EINTR || errno == EAGAIN)
						continue;
					LOGERR_PERROR("recvfrom()");

					/*
					 * Client socket is broken for
					 * some reason.
					 */
					handle_timeout(client);
				} else if (len != 0) {

					/*
					 * With len == 0 the packet was
					 * rejected (probably because it was
					 * too small) so just ignore this case.
					 */

					FD_CLR(client->sock, &readfds);
					handle_packet(client, &from, fromstr,
					    &msg);
				}
			}
		}
		timeout_clients();
	}
	LOGINFO("Shutting down...");

	/*
	 * Clients is the head of the list, so clients != NULL iff the list
	 * is not empty. Call it a timeout so that the scripts get run.
	 */
	while (!SLIST_EMPTY(&clients))
		handle_timeout(SLIST_FIRST(&clients));
}

static void
signal_shutdown(int sig __unused)
{

    do_shutdown = 1;
}

int
main(int argc, char **argv)
{
	struct stat statbuf;
	struct sockaddr_in bindaddr;
	struct sigaction sa;
	int ch;

	pfh = pidfile_open(NULL, 0600, NULL);
	if (pfh == NULL) {
		if (errno == EEXIST)
			printf("Instance of netdump already running\n");
		else
			printf("Impossible to open the pid file\n");
		exit(1);
	}

	while ((ch = getopt(argc, argv, "a:Dd:i:")) != -1) {
		switch (ch) {
		case 'a':
			pflags |= PFLAGS_ABIND;
			if (!inet_aton(optarg, &bindip)) {
				pidfile_remove(pfh);
				fprintf(stderr, "Invalid bind IP specified\n");
				exit(1);
			}
			printf("Listening on IP %s\n", optarg);
			break;
		case 'D':
			pflags |= PFLAGS_DEBUG;
			break;
		case 'd':
			pflags |= PFLAGS_DDIR;
			assert(dumpdir[0] == '\0');
			strncpy(dumpdir, optarg, sizeof(dumpdir) - 1);
			break;
		case 'i':
			pflags |= PFLAGS_SCRIPT;

			/*
			 * When suddenly closing the process for an error,
			 * it is unuseful to take care of handler_script
			 * deallocation as long as the process will _exit(2)
			 * anyway.
			 */
			handler_script = strdup(optarg);
			if (handler_script == NULL) {
				pidfile_remove(pfh);
				perror("strdup()");
				fprintf(stderr, "Unable to set script file\n");
				exit(1);
			}
			if (access(handler_script, F_OK | X_OK)) {
				pidfile_remove(pfh);
				perror("access()");
				fprintf(stderr,
				    "Unable to access script file\n");
				exit(1);
			}
			break;
		default:
			pidfile_remove(pfh);
			usage(argv[0]);
			exit(1);
		}
	}
	if ((pflags & PFLAGS_ABIND) == 0) {
		bindip.s_addr = INADDR_ANY;
		printf("Default: listening on all interfaces\n");
	}
	if ((pflags & PFLAGS_DDIR) == 0) {
		strcpy(dumpdir, "/var/crash");
		printf("Default: dumping on /var/crash/\n");
	}
	if ((pflags & PFLAGS_DEBUG) == 0)
		phook = syslog;
	else
		phook = phook_printf;

	/* Further sanity checks on dump location. */
	if (stat(dumpdir, &statbuf)) {
		pidfile_remove(pfh);
		perror("stat()");
		fprintf(stderr, "Invalid dump location specified\n");
		exit(1);
	}
	if ((statbuf.st_mode & S_IFMT) != S_IFDIR) {
		pidfile_remove(pfh);
		fprintf(stderr, "Dump location is not a directory\n");
		exit(1);
	}
	if (access(dumpdir, F_OK | W_OK)) {
		fprintf(stderr,
		    "Warning: May be unable to write into dump location: %s\n",
		    strerror(errno));
	}

	if ((pflags & PFLAGS_DEBUG) == 0 && daemon(0, 0) == -1) {
		pidfile_remove(pfh);
		perror("daemon()");
		fprintf(stderr, "Impossible to demonize the process\n");
		exit(1);
	}
	pidfile_write(pfh);

	/* Set up the server socket. */
	sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == -1) {
		pidfile_remove(pfh);
		LOGERR_PERROR("socket()");
		exit(1);
	}
	bzero(&bindaddr, sizeof(bindaddr));
	bindaddr.sin_len = sizeof(bindaddr);
	bindaddr.sin_family = AF_INET;
	bindaddr.sin_addr.s_addr = bindip.s_addr;
	bindaddr.sin_port = htons(NETDUMP_PORT);
	if (bind(sock, (struct sockaddr *)&bindaddr, sizeof(bindaddr))) {
		pidfile_remove(pfh);
		close(sock);
		LOGERR_PERROR("bind()");
		exit(1);
	}
	if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
		pidfile_remove(pfh);
		close(sock);
		LOGERR_PERROR("fcntl()");
		exit(1);
	}

	/* Override some signal handlers. */
	bzero(&sa, sizeof(sa));
	sa.sa_handler = signal_shutdown;
	if (sigaction(SIGINT, &sa, NULL) || sigaction(SIGTERM, &sa, NULL)) {
		pidfile_remove(pfh);
		close(sock);
		LOGERR_PERROR("sigaction(SIGINT | SIGTERM)");
		exit(1);
	}
	bzero(&sa, sizeof(sa));
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = SA_NOCLDWAIT;
	if (sigaction(SIGCHLD, &sa, NULL)) {
		pidfile_remove(pfh);
		LOGERR_PERROR("sigaction(SIGCHLD)");
		close(sock);
		exit(1);
	}

	LOGINFO("Waiting for clients.\n");
	eventloop();

	pidfile_remove(pfh);
	return (0);
}

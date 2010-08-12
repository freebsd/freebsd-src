/*-
 * Copyright (c) 2005-2006 Sandvine Incorporated. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <libutil.h>
#include <netdb.h>
#include <stdbool.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/kerneldump.h>
#include <sys/queue.h>
#include <netinet/netdump.h>
#include <inttypes.h>

/* How many dumps to allow per IP before they need to be cleaned out */
#define MAX_DUMPS 256
/* Clients time out after two minutes */
#define CLIENT_TIMEOUT 120
/* Host name length (keep at least as big as INET_ADDRSTRLEN) */
#define MAXHOSTNAMELEN 256

#define	client_ntoa(cl)		inet_ntoa((cl)->ip)

struct netdump_client
{
    SLIST_ENTRY(netdump_client) iter;
    char infofilename[MAXPATHLEN];
    char corefilename[MAXPATHLEN];
    char hostname[MAXHOSTNAMELEN];
    struct in_addr ip;
    FILE *infofile;
    int corefd;
    int sock;
    time_t last_msg;
    unsigned int printed_port_warning : 1;
    unsigned int any_data_rcvd : 1;
};

SLIST_HEAD(, netdump_client) clients = SLIST_HEAD_INITIALIZER(clients);
char dumpdir[MAXPATHLEN];
char *handler_script=NULL;
time_t now;
int do_shutdown;
struct in_addr bindip;
struct pidfh *pfh;
int sock;

struct netdump_client * alloc_client(struct in_addr *ip)
{
    struct sockaddr_in saddr;
    struct netdump_client *client;
    struct hostent *hp;
    int i, fd, bufsz;

    client = calloc(1, sizeof(*client));
    if (!client)
    {
	perror("calloc");
	return NULL;
    }
    bcopy(ip, &client->ip, sizeof(*ip));
    client->corefd = -1;
    client->sock = -1;
    client->last_msg = now;

    /* XXX: To be replaced by getnameinfo(). Get the hostname */
    if ((hp = gethostbyaddr((const char *)ip, sizeof(*ip), AF_INET)) == NULL ||
	    !hp->h_name || strlen(hp->h_name) == 0)
    {
	/* Can't resolve; use IP */
	addr2ascii(AF_INET, ip, sizeof(*ip), client->hostname);
    }
    else
    {
	char *firstdot;

	/* Grab the hostname */
	strncpy(client->hostname, hp->h_name, MAXHOSTNAMELEN);
	hp->h_name[MAXHOSTNAMELEN-1]='\0';

	/* Strip off the domain name */
	firstdot = strchr(client->hostname, '.');
	if (firstdot)
	{
	    *firstdot='\0';
	}
    }

    /* Set up the client socket */
    if ((client->sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
	perror("socket");
	free(client);
	return NULL;
    }
    if (fcntl(client->sock, F_SETFL, O_NONBLOCK) == -1) {
	perror("fcntl(client->sock, F_SETFL, O_NONBLOCK)");
	close(client->sock);
	free(client);
	return NULL;
    }
    bzero(&saddr, sizeof(saddr));
    saddr.sin_len = sizeof(saddr);
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = bindip.s_addr;
    saddr.sin_port = htons(0);
    if (bind(client->sock, (struct sockaddr *)&saddr, sizeof(saddr))) {
	perror("bind");
	close(client->sock);
	free(client);
	return NULL;
    }
    bzero(&saddr, sizeof(saddr));
    saddr.sin_len = sizeof(saddr);
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = ip->s_addr;
    saddr.sin_port = htons(NETDUMP_ACKPORT);
    if (connect(client->sock, (struct sockaddr *)&saddr, sizeof(saddr))) {
	perror("connect");
	close(client->sock);
	free(client);
	return NULL;
    }
    bufsz=131072; /* XXX: Enough to hold approx twice the chunk size. Should be
		   * plenty for any 1 client. */
    if (setsockopt(client->sock, SOL_SOCKET, SO_RCVBUF, &bufsz, sizeof(bufsz)))
    {
	perror("setsockopt(SOL_SOCKET, SO_RCVBUF)");
	fprintf(stderr, "Warning: May drop packets from %s due to small receive"
		" buffer\n", client->hostname);
    }

    /* Try info.host.0 through info.host.255 in sequence */
    for (i=0; i < MAX_DUMPS; i++)
    {
	snprintf(client->infofilename, sizeof(client->infofilename),
			"%s/info.%s.%d", dumpdir, client->hostname, i);
	snprintf(client->corefilename, sizeof(client->corefilename),
			"%s/vmcore.%s.%d", dumpdir, client->hostname, i);

	/* Try the info file first */
	if ((fd = open(client->infofilename, O_WRONLY|O_CREAT|O_EXCL,
		DEFFILEMODE)) == -1)
	{
	    if (errno == EEXIST)
	    {
		continue;
	    }

	    fprintf(stderr, "open(\"%s\"): %s\n", client->infofilename,
			    strerror(errno));
	    continue;
	}
	if (!(client->infofile = fdopen(fd, "w")))
	{
	    perror("fdopen");
	    close(fd);
	    /* XXX */
	    unlink(client->infofilename);
	    continue;
	}

	/* Next make the core file */
	if ((fd = open(client->corefilename, O_RDWR|O_CREAT|O_EXCL,
		DEFFILEMODE)) == -1)
	{
	    /* Failed. Keep the numbers in sync. */
	    fclose(client->infofile);
	    unlink(client->infofilename);
	    client->infofile = NULL;

	    if (errno == EEXIST)
	    {
		continue;
	    }

	    fprintf(stderr, "open(\"%s\"): %s\n", client->corefilename,
			    strerror(errno));
	    continue;
	}
	client->corefd = fd;
	break;
    }

    if (!client->infofile || client->corefd == -1)
    {
	fprintf(stderr, "Can't create output files for new client %s [%s]\n",
		client->hostname, client_ntoa(client));
	if (client->infofile)
	{
	    fclose(client->infofile);
	}
	if (client->corefd != -1)
	{
	    close(client->corefd);
	}
	if (client->sock != -1)
	{
	    close(client->sock);
	}
	free(client);
	return NULL;
    }
    SLIST_INSERT_HEAD(&clients, client, iter);
    return client;
}

void free_client(struct netdump_client *client)
{
    /* Remove from the list */
    SLIST_REMOVE(&clients, client, netdump_client, iter);
    fclose(client->infofile);
    close(client->corefd);
    close(client->sock);
    free(client);
}

void exec_handler(struct netdump_client *client, const char *reason)
{
    int pid=fork();

    if (pid == -1)
    {
	perror("fork");
	return;
    }
    else if (pid)
    {
	return;
    }
    else
    {
	close(sock);
	pidfile_close(pfh);
	execl(handler_script, handler_script, reason, client_ntoa(client),
		client->hostname, client->infofilename, client->corefilename,
		NULL);
	perror("execl");
	_exit(1);
    }
}

void handle_timeout(struct netdump_client *client)
{
    printf("Client %s timed out\n", client_ntoa(client));
    fputs("Dump incomplete: client timed out\n", client->infofile);
    exec_handler(client, "timeout");
    free_client(client);
}

void timeout_clients()
{
    static time_t last_timeout_check;
    struct netdump_client *client, *tmp;
    
    /* Only time out clients every 10 seconds */
    if (now - last_timeout_check < 10)
    {
	return;
    }

    last_timeout_check = now;

    /* Traverse the list looking for stale clients */
    SLIST_FOREACH_SAFE(client, &clients, iter, tmp) {
	if (client->last_msg+CLIENT_TIMEOUT < now)
	{
	    handle_timeout(client);
	}
    }
}

void send_ack(struct netdump_client *client, struct netdump_msg *msg)
{
    struct netdump_ack ack;
    int tryagain;
    
    bzero(&ack, sizeof(ack));
    ack.seqno = htonl(msg->hdr.seqno);

    do
    {
	tryagain=0;

	if (send(client->sock, &ack, sizeof(ack), 0) == -1)
	{
	    if (errno == EINTR)
	    {
		tryagain=1;
		continue;
	    }

	    /* XXX: On EAGAIN, we should probably queue the packet to be sent
	     * when the socket is writable... but that's too much effort, since
	     * it's mostly harmless to wait for the client to retransmit. */
	    perror("sendto");
	}
    }
    while (tryagain);
}

int handle_herald(struct sockaddr_in *from, struct netdump_client *client,
	struct netdump_msg *msg)
{
    int freed_client=0;

    if (client)
    {
	if (!client->any_data_rcvd)
	{
	    /* Must be a retransmit of the herald packet. */
	    send_ack(client, msg);
	    return 0;
	}

	/* An old connection must have timed out. Clean it up first */
	handle_timeout(client);
	freed_client=1;
    }

    client = alloc_client(&from->sin_addr);

    if (!client)
    {
	/* alloc_client would have printed an error message already */
	return freed_client;
    }

    fprintf(client->infofile, "Dump from %s [%s]\n", client->hostname,
	    client_ntoa(client));

    printf("New dump from client %s [%s] (to %s)\n", client->hostname,
	    client_ntoa(client), client->corefilename);

    send_ack(client, msg);

    return freed_client;
}

int handle_kdh(struct netdump_client *client, struct netdump_msg *msg)
{
    struct kerneldumpheader *h=(void *)msg->data;
    uint64_t dumplen;
    FILE *f;
    time_t t;
    int parity_check;

    if (!client)
    {
	return 0;
    }

    client->any_data_rcvd = 1;

    f = client->infofile;
    
    if (msg->hdr.len < sizeof(struct kerneldumpheader))
    {
	fprintf(stderr, "Bad KDH from %s [%s]: packet too small\n",
		client->hostname, client_ntoa(client));
	fputs("Bad KDH: packet too small\n", f);
	fflush(f);
	send_ack(client, msg);

	return 0;
    }

    parity_check = kerneldump_parity(h);

    /* Make sure we null terminate all the strings */
    h->architecture[sizeof(h->architecture)-1] = '\0';
    h->hostname[sizeof(h->hostname)-1] = '\0';
    h->versionstring[sizeof(h->versionstring)-1] = '\0';
    h->panicstring[sizeof(h->panicstring)-1] = '\0';

    fprintf(f, "  Architecture: %s\n", h->architecture);
    fprintf(f, "  Architecture version: %d\n", dtoh32(h->architectureversion));
    dumplen = dtoh64(h->dumplength);
    fprintf(f, "  Dump length: %lldB (%lld MB)\n", (long long)dumplen,
	    (long long)(dumplen >> 20));
    fprintf(f, "  Blocksize: %d\n", dtoh32(h->blocksize));
    t = dtoh64(h->dumptime);
    fprintf(f, "  Dumptime: %s", ctime(&t));
    fprintf(f, "  Hostname: %s\n", h->hostname);
    fprintf(f, "  Versionstring: %s", h->versionstring);
    fprintf(f, "  Panicstring: %s\n", h->panicstring);
    fprintf(f, "  Header parity check: %s\n", parity_check ? "Fail" : "Pass");
    fflush(f);

    fprintf(stdout, "(KDH from %s [%s])", client->hostname,
	    client_ntoa(client));
    fflush(stdout);

    send_ack(client, msg);
    
    return 0;
}

int handle_vmcore(struct netdump_client *client, struct netdump_msg *msg)
{
    if (!client)
    {
	return 0;
    }

    client->any_data_rcvd = 1;

    if (msg->hdr.seqno % 11523 == 0)
    {
	/* Approximately every 16MB with MTU of 1500 */
	putc('.', stdout);
	fflush(stdout);
    }

    if (pwrite(client->corefd, msg->data, msg->hdr.len, msg->hdr.offset) == -1)
    {
	fprintf(stderr, "pwrite (for client %s [%s]): %s\n", client->hostname,
		client_ntoa(client), strerror(errno));
	fprintf(client->infofile, "Dump unsuccessful: write error at offset %08"PRIx64": %s\n",
		msg->hdr.offset, strerror(errno));
	exec_handler(client, "error");
	free_client(client);
	return 1;
    }

    send_ack(client, msg);

    return 0;
}

int handle_finish(struct netdump_client *client, struct netdump_msg *msg)
{
    if (!client)
    {
	return 0;
    }


    printf("\nCompleted dump from client %s [%s]\n", client->hostname,
	    client_ntoa(client));
    fflush(stdout);
    fputs("Dump complete\n", client->infofile);

    /* Do this before we free the client */
    send_ack(client, msg);

    exec_handler(client, "success");
    free_client(client);

    return 1;
}


int receive_message(int sock, struct sockaddr_in *from, char *fromstr,
	size_t fromstrlen, struct netdump_msg *msg)
{
    socklen_t fromlen;
    ssize_t len;

    bzero(from, sizeof(*from));
    from->sin_family = AF_INET;
    from->sin_len = fromlen = sizeof(*from);
    from->sin_port = 0;
    from->sin_addr.s_addr = INADDR_ANY;

    if ((len = recvfrom(sock, msg, sizeof(*msg), 0,
		    (struct sockaddr *)from, &fromlen)) == -1)
    {
	/* The caller can use errno to find the error */
	return -1;
    }

    snprintf(fromstr, fromstrlen, "%s:%hu", inet_ntoa(from->sin_addr),
	    ntohs(from->sin_port));

    if ((size_t)len < sizeof(struct netdump_msg_hdr))
    {
	fprintf(stderr, "Ignoring runt packet from %s (got %zu)\n", fromstr,
		(size_t)len);
	return 0;
    }

    /* Convert byte order */
    msg->hdr.type = ntohl(msg->hdr.type);
    msg->hdr.seqno = ntohl(msg->hdr.seqno);
    msg->hdr.offset = ntohll(msg->hdr.offset);
    msg->hdr.len = ntohl(msg->hdr.len);

    if ((size_t)len < sizeof(struct netdump_msg_hdr) + msg->hdr.len)
    {
	fprintf(stderr, "Packet too small from %s (got %zu, expected %zu)\n",
		fromstr, (size_t)len, sizeof(struct netdump_msg_hdr) + msg->hdr.len);
	return 0;
    }

    return len;
}

int handle_packet(struct netdump_client *client, struct sockaddr_in *from,
	const char *fromstr, struct netdump_msg *msg)
{
    int freed_client;

    if (client)
    {
	client->last_msg = time(NULL);
    }

    switch (msg->hdr.type)
    {
	case NETDUMP_HERALD:
	    freed_client = handle_herald(from, client, msg);
	    break;
	case NETDUMP_KDH:
	    freed_client = handle_kdh(client, msg);
	    break;
	case NETDUMP_VMCORE:
	    freed_client = handle_vmcore(client, msg);
	    break;
	case NETDUMP_FINISHED:
	    freed_client = handle_finish(client, msg);
	    break;
	default:
	    freed_client=0;
	    fprintf(stderr, "Received unknown message type %d from %s\n",
		    msg->hdr.type, fromstr);
    }

    return freed_client;
}

void eventloop()
{
    struct netdump_msg msg;

    while (!do_shutdown)
    {
	fd_set readfds;
	int maxfd=sock+1;
	struct netdump_client *client;
	struct timeval tv;
	struct sockaddr_in from;
	char fromstr[INET_ADDRSTRLEN+6]; /* Long enough for IP+':'+port+'\0' */
	
	FD_ZERO(&readfds);
	FD_SET(sock, &readfds);
	SLIST_FOREACH(client, &clients, iter) {
	    FD_SET(client->sock, &readfds);
	    if (maxfd <= client->sock)
	    {
		maxfd = client->sock+1;
	    }
	}

	/* So that we time out clients regularly */
	tv.tv_sec = 10;
	tv.tv_usec = 0;

	if (select(maxfd, &readfds, NULL, NULL, &tv) == -1)
	{
	    if (errno == EINTR)
	    {
		continue;
	    }

	    perror("select");
	    /* Errors with select() probably won't go away if we just try to
	     * select() again */
	    exit(1);
	}

	now = time(NULL);

	if (FD_ISSET(sock, &readfds)) {
	    int len;

	    len = receive_message(sock, &from, fromstr, sizeof(fromstr), &msg);

	    if (len == -1)
	    {
		if (errno == EINTR)
		{
		    continue;
		}

		if (errno != EAGAIN)
		{
		    /* Server socket is broken for some reason */
		    perror("recvfrom");
		    exit(1);
		}
	    }
	    else if (len == 0)
	    {
		/* The packet was rejected (probably because it was too small).
		 * Just ignore it. */
	    }
	    else
	    {
		/* Check if they're on the clients list */
		SLIST_FOREACH(client, &clients, iter) {
		    if (client->ip.s_addr == from.sin_addr.s_addr)
		    {
			break;
		    }
		}

		/* Technically, clients shouldn't be responding on the server
		 * port, so client should be NULL, however, if they insist on
		 * doing so, it's not really going to hurt anything (except
		 * maybe fill up the server socket's receive buffer), so still
		 * accept it. The only possibly legitimate case is if there's
		 * a new dump starting and the previous one didn't finish
		 * cleanly. Handle this by suppressing the error on HERALD
		 * packets.
		 */
		if (client && msg.hdr.type != NETDUMP_HERALD &&
			!client->printed_port_warning)
		{
		    printf("Warning: Client %s responding on server port\n",
			    client->hostname);
		    client->printed_port_warning = 1;
		}

		handle_packet(client, &from, fromstr, &msg);
	    }
	}

	SLIST_FOREACH(client, &clients, iter) {
	    if (FD_ISSET(client->sock, &readfds))
	    {
		int len = receive_message(client->sock, &from, fromstr,
			sizeof(fromstr), &msg);
		if (len == -1)
		{
		    if (errno == EINTR || errno == EAGAIN)
		    {
			continue;
		    }

		    perror("recvfrom");
		    /* Client socket is broken for some reason */
		    handle_timeout(client);
		    /* The client pointer is now invalid */
		    client = SLIST_FIRST(&clients);
		    if (!client)
		    {
			break;
		    }
		}
		else if (len == 0)
		{
		    /* The packet was rejected (probably because it was too
		     * small). Just ignore it. */
		}
		else
		{
		    FD_CLR(client->sock, &readfds);
		    if (handle_packet(client, &from, fromstr, &msg))
		    {
			/* Client was freed; we have a stale pointer */
		    	client = SLIST_FIRST(&clients);
			if (!client)
			{
			    break;
			}
		    }
		}
	    }
	}

	timeout_clients();
    }

    puts("Shutting down...");
    /* Clients is the head of the list, so clients != NULL iff the list isn't
     * empty. Call it a timeout so that the scripts get run. */
    while (!SLIST_EMPTY(&clients))
    {
	handle_timeout(SLIST_FIRST(&clients));
    }
}

void signal_shutdown(int sig)
{
    do_shutdown=1;
}

int main(int argc, char **argv)
{
    struct stat statbuf;
    struct sockaddr_in bindaddr;
    struct sigaction sa;

    pfh = pidfile_open(NULL, 0600, NULL);
    if (pfh == NULL) {
        if (errno == EEXIST)
            printf("Instance of netdump already running\n");
        else
            printf("Impossible to open the pid file\n");
        exit(1);
    }
    pidfile_write(pfh);

    /* Check argc and set the bindaddr and handler_script */
    switch (argc)
    {
	case 4:
	    handler_script = strdup(argv[3]);
	    if (access(handler_script, F_OK|X_OK))
	    {
                pidfile_remove(pfh);
		fputs("Warning: may be unable to execute handler script\n",
			stderr);
		free(handler_script);
		return 1;
	    }
	case 3:
	    if (!inet_aton(argv[2], &bindip))
	    {
                pidfile_remove(pfh);
		fputs("Invalid bind IP specified\n", stderr);
		return 1;
	    }
	    printf("Listening on IP %s\n", argv[2]);
	    break;
	case 2:
	    bindip.s_addr = INADDR_ANY;
	    puts("Listening on all interfaces");
	    break;
	default:
            pidfile_remove(pfh);
	    fprintf(stderr, "Usage: %s <dump location> "
		    "[bind IP [handler script]]\n", argv[0]);
	    return 1;
    }

    /* Check dump location for sanity */
    if (stat(argv[1], &statbuf))
    {
        pidfile_remove(pfh);
	perror("stat");
	fputs("Invalid dump location specified\n", stderr);
	return 1;
    }
    if ((statbuf.st_mode & S_IFMT) != S_IFDIR)
    {
        pidfile_remove(pfh);
	fputs("Dump location is not a directory\n", stderr);
	return 1;
    }
    if (access(argv[1], F_OK|W_OK))
    {
	fprintf(stderr, "Warning: May be unable to write into dump "
		"location: %s\n", strerror(errno));
    }
    strncpy(dumpdir, argv[1], sizeof(dumpdir)-1);
    dumpdir[sizeof(dumpdir)-1]='\0';

    /* Set up the server socket */
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        pidfile_remove(pfh);
	perror("socket");
	return 1;
    }
    bzero(&bindaddr, sizeof(bindaddr));
    bindaddr.sin_len = sizeof(bindaddr);
    bindaddr.sin_family = AF_INET;
    bindaddr.sin_addr.s_addr = bindip.s_addr;
    bindaddr.sin_port = htons(NETDUMP_PORT);
    if (bind(sock, (struct sockaddr *)&bindaddr, sizeof(bindaddr)))
    {
        pidfile_remove(pfh);
	perror("bind");
	close(sock);
	return 1;
    }
    if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
        pidfile_remove(pfh);
	perror("fcntl(sock, F_SETFL, O_NONBLOCK)");
	close(sock);
	return 1;
    }

    /* Signal handlers */
    bzero(&sa, sizeof(sa));
    sa.sa_handler = signal_shutdown;
    if (sigaction(SIGINT, &sa, NULL) || sigaction(SIGTERM, &sa, NULL))
    {
        pidfile_remove(pfh);
	perror("sigaction");
	close(sock);
	return 1;
    }
    bzero(&sa, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = SA_NOCLDWAIT;
    if (sigaction(SIGCHLD, &sa, NULL))
    {
        pidfile_remove(pfh);
	perror("sigaction");
	close(sock);
	return 1;
    }

    printf("Waiting for clients.\n");
    fflush(stdout);

    do_shutdown=0;
    eventloop();

    if (handler_script)
    {
	free(handler_script);
    }
    pidfile_remove(pfh);

    return 0;
}


/*
 * Copyright (c) 1995 Andrew McRae.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#define EXTERN
#include "cardd.h"

char   *config_file = "/etc/defaults/pccard.conf";
static char *pid_file = "/var/run/pccardd.pid";

/*
 * pathname of UNIX-domain socket
 */
static char	*socket_name = "/var/tmp/.pccardd";
static char	*sock = 0;
static int	server_sock;

/* SIGHUP signal handler */
static void
restart(void)
{
	bitstr_t bit_decl(io_inuse, IOPORTS);
	bitstr_t bit_decl(mem_inuse, MEMBLKS);
	int	irq_inuse[16];
	int	i;
	struct sockaddr_un sun;

	bit_nclear(io_inuse, 0, IOPORTS-1);
	bit_nclear(mem_inuse, 0, MEMBLKS-1);
	bzero(irq_inuse, sizeof(irq_inuse));

	/* compare the initial and current state of resource pool */
	for (i = 0; i < IOPORTS; i++) {
		if (bit_test(io_init, i) == 1 && bit_test(io_avail, i) == 0) {
			if (debug_level >= 1) {
				logmsg("io 0x%x seems to be in use\n", i);
			}
			bit_set(io_inuse, i);
		}
	}
	for (i = 0; i < MEMBLKS; i++) {
		if (bit_test(mem_init, i) == 1 && bit_test(mem_avail, i) == 0) {
			if (debug_level >= 1) {
				logmsg("mem 0x%x seems to be in use\n", i);
			}
			bit_set(mem_inuse, i);
		}
	}
	for (i = 0; i < 16; i++) {
		if (irq_init[i] == 1 && pool_irq[i] == 0) {
			if (debug_level >= 1) {
				logmsg("irq %d seems to be in use\n", i);
			}
			irq_inuse[i] = 1;
		}
	}

	readfile(config_file);

	/* reflect used resources to managed resource pool */
	for (i = 0; i < IOPORTS; i++) {
		if (bit_test(io_inuse, i) == 1) {
			bit_clear(io_avail, i);
		}
	}
	for (i = 0; i < MEMBLKS; i++) {
		if (bit_test(mem_inuse, i) == 1) {
			bit_clear(mem_avail, i);
		}
	}
	for (i = 0; i < 16; i++) {
		if (irq_inuse[i] == 1) {
			pool_irq[i] = 0;
		}
	}
	close(server_sock);
	if ((server_sock = socket(PF_LOCAL, SOCK_DGRAM, 0)) < 0)
		die("socket failed");
	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	if (sock) {
		socket_name = sock;
	}
	strcpy(sun.sun_path, socket_name);
	slen = SUN_LEN(&sun);
	(void)unlink(socket_name);
	if (bind(server_sock, (struct sockaddr *) & sun, slen) < 0)
		die("bind failed");
	chown(socket_name, 0, 5);	/* XXX - root.operator */
	chmod(socket_name, 0660);
	set_socket(server_sock);
}

/* SIGTERM/SIGINT signal handler */
static void
term(int sig)
{
	logmsg("pccardd terminated: signal %d received", sig);
	(void)unlink(pid_file);
	exit(0);
}

static void
write_pid()
{
	FILE *fp = fopen(pid_file, "w");

	if (fp) {
		fprintf(fp, "%d\n", getpid());
		fclose(fp);
	}
}

int doverbose = 0;
/*
 *	mainline code for cardd
 */
int
main(int argc, char *argv[])
{
	struct slot *sp;
	int count, dodebug = 0;
	int delay = 0;
	int irq_arg[16];
	int irq_specified = 0;
	int i;
	struct sockaddr_un sun;
#define	COM_OPTS	":dvf:s:i:z"

	bzero(irq_arg, sizeof(irq_arg));
	debug_level = 0;
	pccard_init_sleep = 5000000;
	cards = last_card = 0;
	while ((count = getopt(argc, argv, COM_OPTS)) != -1) {
		switch (count) {
		case 'd':
			setbuf(stdout, 0);
			setbuf(stderr, 0);
			dodebug = 1;
			break;
		case 'v':
			doverbose = 1;
			break;
		case 'f':
			config_file = optarg;
			break;
		case 'i':
			/* configure available irq */
			if (sscanf(optarg, "%d", &i) != 1) {
				fprintf(stderr, "%s: -i number\n", argv[0]);
				exit(1);
			}
			irq_arg[i] = 1;
			irq_specified = 1;
			break;
		case 's':
			sock = optarg;
			break;
		case 'z':
			delay = 1;
			break;
		case ':':
			die("no config file argument");
			break;
		case '?':
			die("illegal option");
			break;
		}
	}
#ifdef	DEBUG
	dodebug = 1;
#endif
	io_avail = bit_alloc(IOPORTS);	/* Only supports ISA ports */
	io_init  = bit_alloc(IOPORTS);

	/* Mem allocation done in MEMUNIT units. */
	mem_avail = bit_alloc(MEMBLKS);
	mem_init  = bit_alloc(MEMBLKS);
	readfile(config_file);
	if (irq_specified) {
		bcopy(irq_arg, pool_irq, sizeof(irq_arg));
		bcopy(irq_arg, irq_init, sizeof(irq_arg));
	}
	if (doverbose)
		dump_config_file();
	log_setup();
	if (!dodebug && !delay)
		if (daemon(0, 0))
			die("fork failed");
	slots = readslots();
	if (slots == 0)
		die("no PC-CARD slots");
	if (delay)
		if (daemon(0, 0))
			die("fork failed");
	logmsg("pccardd started", NULL);
	write_pid();

	if ((server_sock = socket(PF_LOCAL, SOCK_DGRAM, 0)) < 0)
		die("socket failed");
	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	if (sock) {
		socket_name = sock;
	}
	strcpy(sun.sun_path, socket_name);
	slen = SUN_LEN(&sun);
	(void)unlink(socket_name);
	if (bind(server_sock, (struct sockaddr *) & sun, slen) < 0)
		die("bind failed");
	chown(socket_name, 0, 5);	/* XXX - root.operator */
	chmod(socket_name, 0660);
	set_socket(server_sock);

	(void)signal(SIGINT, dodebug ? term : SIG_IGN);
	(void)signal(SIGTERM, term);
	(void)signal(SIGHUP, (void (*)(int))restart);

	for (;;) {
		fd_set  rmask, emask;
		FD_ZERO(&emask);
		FD_ZERO(&rmask);
		for (sp = slots; sp; sp = sp->next)
			FD_SET(sp->fd, &emask);
		FD_SET(server_sock, &rmask);
		count = select(32, &rmask, 0, &emask, 0);
		if (count == -1) {
			logerr("select");
			continue;
		}
		if (count) {
			for (sp = slots; sp; sp = sp->next)
				if (FD_ISSET(sp->fd, &emask))
					slot_change(sp);
			if (FD_ISSET(server_sock, &rmask))
				process_client();
		}
	}
}

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
	"$Id: pccardd.c,v 1.2 1998/03/09 05:18:58 hosokawa Exp $";
#endif /* not lint */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#define EXTERN
#include "cardd.h"

char   *config_file = "/etc/pccard.conf";

/*
 *	mainline code for cardd
 */
int
main(int argc, char *argv[])
{
	struct slot *slots, *sp;
	int count, dodebug = 0;
	int doverbose = 0;

	while ((count = getopt(argc, argv, ":dvf:")) != -1) {
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

	/* Mem allocation done in MEMUNIT units. */
	mem_avail = bit_alloc(MEMBLKS);
	readfile(config_file);
	if (doverbose)
		dump_config_file();
	log_setup();
	if (!dodebug)
		if (daemon(0, 0))
			die("fork failed");
	slots = readslots();
	if (slots == 0)
		die("no PC-CARD slots");
	logmsg("pccardd started", NULL);
	for (;;) {
		fd_set  mask;
		FD_ZERO(&mask);
		for (sp = slots; sp; sp = sp->next)
			FD_SET(sp->fd, &mask);
		count = select(32, 0, 0, &mask, 0);
		if (count == -1) {
			logerr("select");
			continue;
		}
		if (count)
			for (sp = slots; sp; sp = sp->next)
				if (FD_ISSET(sp->fd, &mask))
					slot_change(sp);
	}
}

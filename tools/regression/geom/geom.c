/*-
 * Copyright (c) 2002 Poul-Henning Kamp
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Poul-Henning Kamp
 * and NAI Labs, the Security Research Division of Network Associates, Inc.
 * under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
 * DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 *
 * $FreeBSD$
 */


#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <err.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/sbuf.h>
#include <geom/geom.h>
#include <geom/geom_int.h>

void
conff(char *file)
{
	FILE *f;
	char *s;
	struct sbuf *sb;

	printf(">> conf to %s\n", file);
	asprintf(&s, "_%s.conf", file);
	f = fopen(s, "w");
	if (f == NULL)
		err(1, s);
	sb = g_conf();
	fputs(sbuf_data(sb), f);
	fclose(f);
	free(s);
}




static int
thread_up(void *ptr)
{
	struct thread *tp = ptr;

	printf("Running %s\n", tp->name);
	for (;;) {
		g_io_schedule_up(tp);
		tsleep(&g_wait_up, 0, "up", 0);
	}
}

static int
thread_down(void *ptr)
{
	struct thread *tp = ptr;
	printf("Running %s\n", tp->name);
	for (;;) {
		g_io_schedule_down(tp);
		tsleep(&g_wait_down, 0, "down", 0);
	}
}

static int
thread_event(void *ptr)
{
	struct thread *tp = ptr;
	/* nice(5); */
	printf("Running %s\n", tp->name);
	for (;;) {
		usleep(100000);
		g_run_events();
		tsleep(&g_wait_event, 0, "events", 0);
	}
}


int
main(int argc __unused, char **argv __unused)
{
	int ch;

	while ((ch = getopt(argc, argv, "bt")) != -1) {
		switch (ch) {
		case 'b':
			g_debugflags |= G_T_BIO;
			break;
		case 't':
			g_debugflags |= G_T_TOPOLOGY;
			break;
		}
	}


	setbuf(stdout, NULL);
	printf("Sizeof g_class = %d\n", sizeof(struct g_class));
	printf("Sizeof g_geom = %d\n", sizeof(struct g_geom));
	printf("Sizeof g_consumer = %d\n", sizeof(struct g_consumer));
	printf("Sizeof g_provider = %d\n", sizeof(struct g_provider));
	printf("Sizeof g_event = %d\n", sizeof(struct g_event));
	g_init();
	new_thread(thread_up, "UP");
	new_thread(thread_down, "DOWN");
	new_thread(thread_event, "EVENT");
	new_thread(thread_sim, "SIM");

	while (1) {
		sleep (1);
		secrethandshake();
	}
}

void
sdumpf(char *file)
{
	FILE *f;
	char *s;
	struct sbuf *sb;

	printf(">> dump to %s\n", file);
	asprintf(&s, "_%s.dot", file);
	f = fopen(s, "w");
	if (f == NULL)
		err(1, s);
	sb = g_confdot();
	fprintf(f, "%s", sbuf_data(sb));
	fclose(f);
	free(s);
	asprintf(&s, "dot -Tps _%s.dot > _%s.ps", file, file);
	system(s);
	free(s);
}


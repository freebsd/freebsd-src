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

LIST_HEAD(class_list_head, g_class);
TAILQ_HEAD(g_tailq_head, g_geom);
TAILQ_HEAD(event_tailq_head, g_event);

extern struct event_tailq_head events;
extern int g_debugflags;
/* 1	G_T_TOPOLOGY		*/
/* 2	G_T_BIO			*/
/* 4	G_T_ACCESS		*/
/* 8	enable sanity checks	*/

/*
 * Various internal actions are tracked by tagging g_event[s] onto
 * an internal eventqueue.
 */
enum g_events {
	EV_NEW_CLASS,		/* class */
	EV_NEW_PROVIDER,	/* provider */
	EV_SPOILED,		/* provider, consumer */
	EV_CALL_ME,		/* func, arg */
	EV_LAST
};

struct g_event {
	enum g_events 		event;
	TAILQ_ENTRY(g_event)	events;
	struct g_class		*class;
	struct g_geom		*geom;
	struct g_provider	*provider;
	struct g_consumer	*consumer;
	void			*arg;
	g_call_me_t		*func;
};

/* geom_dump.c */
void g_confxml(void *);
void g_conf_specific(struct sbuf *sb, struct g_class *mp, struct g_geom *gp, struct g_provider *pp, struct g_consumer *cp);
void g_confdot(void *);


/* geom_event.c */
void g_event_init(void);
void g_post_event(enum g_events ev, struct g_class *mp, struct g_geom *gp, struct g_provider *pp, struct g_consumer *cp);
void g_run_events(void);

/* geom_subr.c */
extern struct class_list_head g_classes;
extern char *g_wait_event, *g_wait_sim, *g_wait_up, *g_wait_down;

/* geom_io.c */
void g_io_init(void);
void g_io_schedule_down(struct thread *tp);
void g_io_schedule_up(struct thread *tp);

/* geom_kern.c / geom_kernsim.c */
void g_init(void);

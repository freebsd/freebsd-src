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

#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sx.h>
#include <sys/queue.h>

#ifndef _KERNEL
/*
 * The GEOM subsystem makes a few concessions in order to be able to run as a
 * user-land simulation as well as a kernel component.
 */
#include <geom_sim.h>
#endif

struct g_class;
struct g_geom;
struct g_consumer;
struct g_provider;
struct g_event;
struct thread;
struct bio;
struct sbuf;

LIST_HEAD(class_list_head, g_class);
TAILQ_HEAD(g_tailq_head, g_geom);
TAILQ_HEAD(event_tailq_head, g_event);

extern struct g_tailq_head geoms;
extern struct event_tailq_head events;
extern int g_debugflags;


#define G_CLASS_INITSTUFF { 0, 0 }, { 0 }, 0

typedef struct g_geom * g_create_geom_t (struct g_class *mp,
    struct g_provider *pp, char *name);
typedef struct g_geom * g_taste_t (struct g_class *, struct g_provider *,
    struct thread *tp, int flags);
#define G_TF_NORMAL		0
#define G_TF_INSIST		1
#define G_TF_TRANSPARENT	2
typedef int g_access_t (struct g_provider *, int, int, int);
/* XXX: not sure about the thread arg */
typedef void g_orphan_t (struct g_consumer *, struct thread *);

typedef void g_start_t (struct bio *);
typedef void g_spoiled_t (struct g_consumer *);
typedef void g_dumpconf_t (struct sbuf *, char *indent, struct g_geom *,
    struct g_consumer *, struct g_provider *);

/*
 * The g_class structure describes a transformation class.  In other words
 * all BSD disklabel handlers share one g_class, all MBR handlers share
 * one common g_class and so on.
 * Certain operations are instantiated on the class, most notably the
 * taste and create_geom functions.
 * XXX: should access and orphan go into g_geom ?
 * XXX: would g_class be a better and less confusing name ?
 */
struct g_class {
	char			*name;
	g_taste_t		*taste;
	g_access_t		*access;
	g_orphan_t		*orphan;
	g_create_geom_t		*create_geom;
	LIST_ENTRY(g_class)	class;
	LIST_HEAD(,g_geom)	geom;
	struct g_event		*event;
};

/*
 * The g_geom is an instance of a g_class.
 */
struct g_geom {
	char			*name;
	struct g_class		*class;
	LIST_ENTRY(g_geom)	geom;
	LIST_HEAD(,g_consumer)	consumer;
	LIST_HEAD(,g_provider)	provider;
	TAILQ_ENTRY(g_geom)	geoms;	/* XXX: better name */
	int			rank;
	g_start_t		*start;
	g_spoiled_t		*spoiled;
	g_dumpconf_t		*dumpconf;
	void			*softc;
	struct g_event		*event;
	unsigned		flags;
#define	G_GEOM_WITHER		1
};

/*
 * The g_bioq is a queue of struct bio's.
 * XXX: possibly collection point for statistics.
 * XXX: should (possibly) be collapsed with sys/bio.h::bio_queue_head.
 */
struct g_bioq {
	TAILQ_HEAD(, bio)	bio_queue;
	struct mtx		bio_queue_lock;
	int			bio_queue_length;
};

/*
 * A g_consumer is an attachment point for a g_provider.  One g_consumer
 * can only be attached to one g_provider, but multiple g_consumers
 * can be attached to one g_provider.
 */

struct g_consumer {
	struct g_geom		*geom;
	LIST_ENTRY(g_consumer)	consumer;
	struct g_provider	*provider;
	LIST_ENTRY(g_consumer)	consumers;	/* XXX: better name */
	int			acr, acw, ace;
	struct g_event		*event;

	int			biocount;
	int			spoiled;
};

/*
 * A g_provider is a "logical disk".
 */
struct g_provider {
	char			*name;
	LIST_ENTRY(g_provider)	provider;
	struct g_geom		*geom;
	LIST_HEAD(,g_consumer)	consumers;
	int			acr, acw, ace;
	int			error;
	struct g_event		*event;
	TAILQ_ENTRY(g_provider)	orphan;
	int			index;
};

/*
 * Various internal actions are tracked by tagging g_event[s] onto
 * an internal eventqueue.
 */
enum g_events {
	EV_NEW_CLASS,		/* class */
	EV_NEW_PROVIDER,	/* provider */
	EV_SPOILED,		/* provider, consumer */
	EV_LAST
};

struct g_event {
	enum g_events 		event;
	TAILQ_ENTRY(g_event)	events;
	struct g_class		*class;
	struct g_geom		*geom;
	struct g_provider	*provider;
	struct g_consumer	*consumer;
};

/* geom_dump.c */
struct sbuf * g_conf(void);
struct sbuf * g_conf_specific(struct g_class *mp, struct g_geom *gp, struct g_provider *pp, struct g_consumer *cp);
struct sbuf * g_confdot(void);
void g_hexdump(void *ptr, int length);
void g_trace(int level, char *, ...);
#	define G_T_TOPOLOGY	1
#	define G_T_BIO		2
#	define G_T_ACCESS	4

/* geom_enc.c */
uint32_t g_dec_be2(u_char *p);
uint32_t g_dec_be4(u_char *p);
uint32_t g_dec_le2(u_char *p);
uint32_t g_dec_le4(u_char *p);
void g_enc_le4(u_char *p, uint32_t u);

/* geom_event.c */
void g_event_init(void);
void g_orphan_provider(struct g_provider *pp, int error);
void g_post_event(enum g_events ev, struct g_class *mp, struct g_geom *gp, struct g_provider *pp, struct g_consumer *cp);
void g_rattle(void);
void g_run_events(struct thread *tp);
void g_silence(void);

/* geom_subr.c */
int g_access_abs(struct g_consumer *cp, int read, int write, int exclusive);
int g_access_rel(struct g_consumer *cp, int read, int write, int exclusive);
void g_add_class(struct g_class *mp);
int g_attach(struct g_consumer *cp, struct g_provider *pp);
struct g_geom *g_create_geomf(char *class, struct g_provider *, char *fmt, ...);
void g_destroy_consumer(struct g_consumer *cp);
void g_destroy_geom(struct g_geom *pp);
void g_destroy_provider(struct g_provider *pp);
void g_dettach(struct g_consumer *cp);
void g_error_provider(struct g_provider *pp, int error);
int g_haveattr(struct bio *bp, char *attribute, void *val, int len);
int g_haveattr_int(struct bio *bp, char *attribute, int val);
int g_haveattr_off_t(struct bio *bp, char *attribute, off_t val);
struct g_geom * g_insert_geom(char *class, struct g_consumer *cp);
extern struct class_list_head g_classs;
struct g_consumer * g_new_consumer(struct g_geom *gp);
struct g_geom * g_new_geomf(struct g_class *mp, char *fmt, ...);
struct g_provider * g_new_providerf(struct g_geom *gp, char *fmt, ...);
void g_spoil(struct g_provider *pp, struct g_consumer *cp);
int g_std_access(struct g_provider *pp, int dr, int dw, int de);
void g_std_done(struct bio *bp);
void g_std_spoiled(struct g_consumer *cp);
extern char *g_wait_event, *g_wait_sim, *g_wait_up, *g_wait_down;

/* geom_io.c */
struct bio * g_clone_bio(struct bio *);
void g_destroy_bio(struct bio *);
void g_io_deliver(struct bio *bp);
int g_io_getattr(char *attr, struct g_consumer *cp, int *len, void *ptr, struct thread *tp);
void g_io_init(void);
void g_io_request(struct bio *bp, struct g_consumer *cp);
void g_io_schedule_down(struct thread *tp);
void g_io_schedule_up(struct thread *tp);
int g_io_setattr(char *attr, struct g_consumer *cp, int len, void *ptr, struct thread *tp);
struct bio *g_new_bio(void);
void * g_read_data(struct g_consumer *cp, off_t offset, off_t length, int *error);

/* geom_kern.c / geom_kernsim.c */
void g_init(void);

struct g_ioctl {
	u_long		cmd;
	void		*data;
	int		fflag;
	struct thread	*td;
};

#ifdef _KERNEL

MALLOC_DECLARE(M_GEOM);

static __inline void *
g_malloc(int size, int flags)
{
	void *p;

	mtx_lock(&Giant);
	p = malloc(size, M_GEOM, flags);
	mtx_unlock(&Giant);
	return (p);
}

static __inline void
g_free(void *ptr)
{
	mtx_lock(&Giant);
	free(ptr, M_GEOM);
	mtx_unlock(&Giant);
}

extern struct sx topology_lock;
#define g_topology_lock() do { mtx_assert(&Giant, MA_NOTOWNED); sx_xlock(&topology_lock); } while (0)
#define g_topology_unlock() sx_xunlock(&topology_lock)
#define g_topology_assert() sx_assert(&topology_lock, SX_XLOCKED)

#define DECLARE_GEOM_CLASS(class, name) 	\
	static void				\
	name##init(void)			\
	{					\
		mtx_unlock(&Giant);		\
		g_add_class(&class);		\
		mtx_lock(&Giant);		\
	}					\
	SYSINIT(name, SI_SUB_PSEUDO, SI_ORDER_FIRST, name##init, NULL);

#endif


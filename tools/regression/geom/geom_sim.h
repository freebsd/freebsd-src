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

#include <pthread.h>

pthread_cond_t ptc_up, ptc_down, ptc_event;
pthread_mutex_t ptm_up, ptm_down, ptm_event;

#define CTASSERT(foo)

/* bio.h */

struct bio {
	enum {
	    BIO_INVALID = 0,
	    BIO_READ = 1,
	    BIO_WRITE = 2,
	    BIO_DELETE = 4,
	    BIO_GETATTR = 8,
	    BIO_SETATTR = 16
	}			bio_cmd;
	struct { int foo; }	stats;
	TAILQ_ENTRY(bio)	bio_queue;
	TAILQ_ENTRY(bio)	bio_sort;
	struct g_consumer		*bio_from;
	struct g_provider		*bio_to;
	void			(*bio_done)(struct bio *);

	off_t			bio_offset;
	off_t			bio_length;
	off_t			bio_completed;
	void			*bio_data;
	const char		*bio_attribute;	/* BIO_GETATTR/BIO_SETATTR */
	int			bio_error;

	struct bio		*bio_linkage;
	int			bio_flags;
#define BIO_DONE		0x1
#define BIO_ERROR		0x2
};

void biodone(struct bio *bp);
int biowait(struct bio *bp, const char *wchan);

/* geom_dev.c */
void g_dev_init(void *junk);
struct g_consumer *g_dev_opendev(char *name, int w, int r, int e);
int g_dev_request(char *name, struct bio *bp);

/* geom_kernsim.c */
struct thread {
	char 	*name;
	pthread_t	tid;
	int	pid;
	void 	*wchan;
	const char    *wmesg;
	int	pipe[2];
};

void done(void);
void rattle(void);
void secrethandshake(void);
void wakeup(void *chan);
int     tsleep __P((void *chan, int pri, const char *wmesg, int timo));
#define PPAUSE 0
extern int hz;

void new_thread(void *(*func)(void *arg), char *name);

extern int bootverbose;
#define KASSERT(cond, txt) do {if (!(cond)) {printf txt; conff("err"); abort();}} while(0)
#define M_WAITOK 0
#define M_NOWAIT 1
#define M_ZERO 2

extern struct mtx Giant;
void *g_malloc(int size, int flags);
void g_free(void *ptr);

#define MTX_DEF   0
#define MTX_SPIN  1
void mtx_lock(struct mtx *);
void mtx_lock_spin(struct mtx *);
void mtx_unlock(struct mtx *);
void mtx_unlock_spin(struct mtx *);
void mtx_init(struct mtx *, const char *, const char *, int);
void mtx_destroy(struct mtx *);

#define MALLOC_DECLARE(foo)	/* */

void g_topology_lock(void);
void g_topology_unlock(void);
void g_topology_assert(void);


/* geom_simdisk.c */
void g_simdisk_init(void);
void g_simdisk_destroy(char *);
struct g_geom *g_simdisk_new(char *, char *);
struct g_geom * g_simdisk_xml_load(char *name, char *file);
void g_simdisk_xml_save(char *name, char *file);
void g_simdisk_stop(char *name);
void g_simdisk_restart(char *name);

#define DECLARE_GEOM_CLASS(class, name) 	\
	void					\
	name##_init(void)			\
	{					\
		g_add_class(&class);		\
	}

void g_pc98_init(void);
void g_sunlabel_init(void);
void g_bsd_init(void);
void g_mbr_init(void);
void g_mbrext_init(void);

void *thread_sim(void *ptr);

void dumpf(char *file);
void conff(char *file);
void sdumpf(char *file);

#define THR_MAIN	0
#define THR_UP		1
#define THR_DOWN	2
#define THR_EVENT	3


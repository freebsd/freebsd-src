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

/* These are quite, but not entirely unlike constants. */
#define G_BDE_MKEYLEN	(2048/8)
#define G_BDE_SKEYBITS	128
#define G_BDE_SKEYLEN	(G_BDE_SKEYBITS/8)
#define G_BDE_KKEYBITS	128
#define G_BDE_KKEYLEN	(G_BDE_KKEYBITS/8)
#define G_BDE_MAXKEYS	4
#define G_BDE_LOCKSIZE	384

/* This just needs to be "large enough" */
#define G_BDE_KEYBYTES	304

struct g_bde_work;
struct g_bde_softc;

struct g_bde_sector {
	struct g_bde_work	*owner;
	struct g_bde_softc	*softc;
	off_t			offset;
	u_int			size;
	u_int			ref;
	void			*data;
	TAILQ_ENTRY(g_bde_sector) list;
	u_char			valid;
	u_char			malloc;
	enum {JUNK, IO, VALID}	state;
	int			error;
};

struct g_bde_work {
	struct mtx		mutex;
	off_t			offset;
	off_t			length;
	void			*data;
        struct bio      	*bp;
	struct g_bde_softc 	*softc;
        off_t           	so;
        off_t           	kso;
        u_int           	ko;
        struct g_bde_sector   	*sp;
        struct g_bde_sector   	*ksp;
	TAILQ_ENTRY(g_bde_work) list;
	enum {SETUP, WAIT, FINISH} state;
	int			error;
};

struct g_bde_key {
	uint64_t		sector0;        
			/* Physical byte offset of first byte used */
	uint64_t		sectorN;
			/* Physical byte offset of first byte not used */
	uint64_t		keyoffset;
	uint64_t		lsector[G_BDE_MAXKEYS];
			/* Physical offsets */
	uint32_t		sectorsize;
	uint32_t		flags;
				/* 1 = lockfile in sector 0 */
	uint8_t			hash[16];
	uint8_t			salt[16];
	uint8_t			spare[32];
	uint8_t			mkey[G_BDE_MKEYLEN];
	/* Non-stored help-fields */
	uint64_t		zone_width;	/* On-disk width of zone */
	uint64_t		zone_cont;	/* Payload width of zone */
	uint64_t		media_width;	/* Non-magic width of zone */
	u_int			keys_per_sector;
};

struct g_bde_softc {
	off_t			mediasize;
	u_int			sectorsize;
	uint64_t		zone_cont;
	struct g_geom		*geom;
	struct g_consumer	*consumer;
	TAILQ_HEAD(, g_bde_sector)	freelist;
	TAILQ_HEAD(, g_bde_work) 	worklist;
	struct mtx		worklist_mutex;
	struct proc		*thread;
	struct g_bde_key	key;
	u_char			arc4_sbox[256];
	u_char			arc4_i, arc4_j;
	int			dead;
	u_int			nwork;
	u_int			nsect;
	u_int			ncache;
};

/* g_bde_crypt.c */
void g_bde_crypt_delete(struct g_bde_work *wp);
void g_bde_crypt_read(struct g_bde_work *wp);
void g_bde_crypt_write(struct g_bde_work *wp);

/* g_bde_key.c */
void g_bde_zap_key(struct g_bde_softc *sc);
int g_bde_get_key(struct g_bde_softc *sc, void *ptr, int len);
int g_bde_init_keybytes(struct g_bde_softc *sc, char *passp, int len);

/* g_bde_lock .c */
void g_bde_encode_lock(struct g_bde_key *gl, u_char *ptr);
void g_bde_decode_lock(struct g_bde_key *gl, u_char *ptr);
u_char g_bde_arc4(struct g_bde_softc *sc);
void g_bde_arc4_seq(struct g_bde_softc *sc, void *ptr, u_int len);
void g_bde_arc4_seed(struct g_bde_softc *sc, const void *ptr, u_int len);
int g_bde_keyloc_encrypt(struct g_bde_softc *sc, void *input, void *output);
int g_bde_keyloc_decrypt(struct g_bde_softc *sc, void *input, void *output);
int g_bde_decrypt_lock(struct g_bde_softc *sc, u_char *sbox, u_char *meta, off_t mediasize, u_int sectorsize, u_int *nkey);

/* g_bde_math .c */
uint64_t g_bde_max_sector(struct g_bde_key *lp);
void g_bde_map_sector(struct g_bde_key *lp, uint64_t isector, uint64_t *osector, uint64_t *ksector, u_int *koffset);

/* g_bde_work.c */
void g_bde_start1(struct bio *bp);
void g_bde_worker(void *arg);


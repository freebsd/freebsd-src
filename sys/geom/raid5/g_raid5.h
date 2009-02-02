/*-
 * Copyright (c) 2006 Arne Woerner <arne_woerner@yahoo.com>
 * testing + tuning-tricks: veronica@fluffles.net
 * derived from gstripe/gmirror (Pawel Jakub Dawidek <pjd@FreeBSD.org>)
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: g_raid5.h,v 1.12.1.34.1.23 2008/07/29 12:54:34 aw Exp aw $
 */

#ifndef	_G_RAID5_H_
#define	_G_RAID5_H_

#include <sys/endian.h>

#define	G_RAID5_CLASS_NAME	"RAID5"

#define	G_RAID5_MAGIC		"GEOM::RAID5"
/*
 * Version history:
 * 1 - Initial version number.
 * 2 - quite stable + provider size checking
 * 3 - experimental + fast combining but high kmem use
 * 4 - experimental + fast combining + read ahead
 */
#define	G_RAID5_VERSION	3

#ifdef _KERNEL
#define	G_RAID5_TYPE_MANUAL	0
#define	G_RAID5_TYPE_AUTOMATIC	1

#define	G_RAID5_DEBUG(lvl, ...)	do {		\
	if (g_raid5_debug >= (lvl)) {		\
		printf("GEOM_RAID5");		\
		if ((lvl) > 0)			\
			printf("[%u]",lvl);	\
		printf(": ");			\
		printf(__VA_ARGS__);		\
		printf("\n");			\
	}					\
} while (0)

#define	G_RAID5_LOGREQ(bp, ...)	do {		\
	if (g_raid5_debug >= 2) {		\
		printf("GEOM_RAID5[2]: ");	\
		printf(__VA_ARGS__);		\
		printf(" ");			\
		g_print_bio(bp);		\
		printf("\n");			\
	}					\
} while (0)

struct g_raid5_cache_entry {
	off_t sno;
	int dc; /* dirty areas */
	struct bio *fst; /* bio_queue.next/.prev: all bios in this stripe */
		/* dirty (bio_parent==NULL); clean (bio_parent==sc) */
	char *sd; /* space for a full stripe (sc->fsl) */
	char *sp; /* space for parity */
	struct bintime lu; /* last use */
};

struct g_raid5_softc {
	u_int		 sc_type;	/* provider type */
	struct g_geom	*sc_geom;
	struct g_provider *sc_provider;
	struct root_hold_token *sc_rootmount;
	uint32_t	 sc_id;		/* raid5 unique ID */

	u_char no_more_open;
	u_char lstdno;
	uint8_t sc_ndisks;
	uint8_t vdc; /* valid disk count */
	int newest; /* number of formerly dead disk */
	struct g_consumer **sc_disks;
	char *preremoved;

	int open_count;
	struct mtx open_mtx;

	struct proc *workerD;
	struct mtx dq_mtx;
	struct bio_queue_head dq;

	struct proc *worker;
	struct mtx sq_mtx;
	struct bio_queue_head sq;

	struct bio_queue_head wdq;
	int wqpi;
	int wqp;

	int csmo; /* cache size mem - old */
	int cso; /* cache size - old */
	int cs; /* cache size */
	struct g_raid5_cache_entry *ce; /* cs entries */
	int cc; /* cache entries */
	int cfc; /* cache deficit */
	int dc; /* dirty cache entries */

	struct mtx sleep_mtx;
	u_char no_sleep;

	u_char conf_order;
	u_char no_hot;
	u_char term;
	u_char hardcoded;
	u_char state;
	u_char destroy_on_za;
	u_char stripebits;
	uint32_t stripesize;
	uint32_t fsl;
	int64_t disksize;
	int veri_pa;
	int64_t verified; /* number of first potentially dead parity stripe */

	int mhs;
	int mhc;
	caddr_t *mhl;
	struct mtx mh_mtx;
};
#define	sc_name	sc_geom->name
#endif	/* _KERNEL */

#define G_RAID5_STATE_CALM       0
#define G_RAID5_STATE_HOT        1
#define G_RAID5_STATE_VERIFY     2
#define G_RAID5_STATE_SAFEOP     4
#define G_RAID5_STATE_COWOP      8

struct g_raid5_metadata {
	char		md_magic[16];	/* Magic value. */
	uint32_t	md_version;	/* Version number. */
	char		md_name[16];	/* Concat name. */
	uint32_t	md_id;		/* Unique ID. */
	uint8_t	md_no;		/* Disk number. */
	uint8_t	md_no_hot;	/* no hot disks flag.. */
	uint8_t	md_all;		/* Number of all disks. */
	uint8_t	md_state;	/* status code */
	uint64_t	md_provsize;	/* Provider's size. */
	uint64_t	md_verified;	/* verified. */
	uint32_t	md_stripesize;	/* stripe size. */
	int32_t	md_newest;	/* # of newest disk. */
	char		md_provider[16]; /* Hardcoded provider. */
};

#define G_RAID5_META_VERSION		16
#define G_RAID5_META_NAME		20
#define G_RAID5_META_ID			36
#define G_RAID5_META_NO			40
#define G_RAID5_META_NO_HOT		41
#define G_RAID5_META_ALL		42
#define G_RAID5_META_STATE		43
#define G_RAID5_META_PROVIDERSIZE	44
#define G_RAID5_META_VERIFIED		52
#define G_RAID5_META_STRIPESIZE		60
#define G_RAID5_META_NEWEST		64
#define G_RAID5_META_PROVIDER		68

static __inline void
raid5_metadata_encode(const struct g_raid5_metadata *md, u_char *data)
{
	bcopy(md->md_magic, data, sizeof(md->md_magic));
	le32enc(data + G_RAID5_META_VERSION, md->md_version);
	bcopy(md->md_name, data + G_RAID5_META_NAME, sizeof(md->md_name));
	le32enc(data + G_RAID5_META_ID, md->md_id);
	data[G_RAID5_META_NO] = md->md_no;
	data[G_RAID5_META_NO_HOT] = md->md_no_hot;
	data[G_RAID5_META_ALL] = md->md_all;
	data[G_RAID5_META_STATE] = md->md_state;
	le64enc(data + G_RAID5_META_PROVIDERSIZE, md->md_provsize);
	le64enc(data + G_RAID5_META_VERIFIED, md->md_verified);
	le32enc(data + G_RAID5_META_STRIPESIZE, md->md_stripesize);
	le32enc(data + G_RAID5_META_NEWEST, md->md_newest);
	bcopy(md->md_provider, data + G_RAID5_META_PROVIDER,
	    sizeof(md->md_provider));
}

static __inline void
raid5_metadata_decode(const u_char *data, struct g_raid5_metadata *md)
{

	bcopy(data, md->md_magic, sizeof(md->md_magic));
	md->md_version = le32dec(data + G_RAID5_META_VERSION);
	bcopy(data + G_RAID5_META_NAME, md->md_name, sizeof(md->md_name));
	md->md_id = le32dec(data + G_RAID5_META_ID);
	md->md_no = data[G_RAID5_META_NO];
	md->md_no_hot = data[G_RAID5_META_NO_HOT];
	md->md_all = data[G_RAID5_META_ALL];
	md->md_state = data[G_RAID5_META_STATE];
	md->md_provsize = le64dec(data + G_RAID5_META_PROVIDERSIZE);
	md->md_verified = le64dec(data + G_RAID5_META_VERIFIED);
	md->md_stripesize = le32dec(data + G_RAID5_META_STRIPESIZE);
	md->md_newest = le32dec(data + G_RAID5_META_NEWEST);
	bcopy(data + G_RAID5_META_PROVIDER, md->md_provider,
	    sizeof(md->md_provider));
}

#endif

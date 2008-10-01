/*-
 * Copyright (c) 2004 Lukas Ertl
 * Copyright (c) 1997, 1998, 1999
 *      Nan Yang Computer Services Limited.  All rights reserved.
 *
 * Parts copyright (c) 1997, 1998 Cybernet Corporation, NetMAX project.
 * Parts written by Greg Lehey.
 *
 *  This software is distributed under the so-called ``Berkeley
 *  License'':                                                                   *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Nan Yang Computer
 *      Services Limited.
 * 4. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *  
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any               * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even if
 * advised of the possibility of such damage.
 *  
 * $FreeBSD$
 */

#ifndef	_GEOM_VINUM_VAR_H_
#define	_GEOM_VINUM_VAR_H_

/*
 * Slice header
 *
 * Vinum drives start with this structure:
 *
 *\                                            Sector
 * |--------------------------------------|
 * |   PDP-11 memorial boot block         |      0
 * |--------------------------------------|
 * |   Disk label, maybe                  |      1
 * |--------------------------------------|
 * |   Slice definition  (vinum_hdr)      |      8
 * |--------------------------------------|
 * |                                      |
 * |   Configuration info, first copy     |      9
 * |                                      |
 * |--------------------------------------|
 * |                                      |
 * |   Configuration info, second copy    |      9 + size of config
 * |                                      |
 * |--------------------------------------|
 */

/* Sizes and offsets of our information. */
#define	GV_HDR_OFFSET	4096	/* Offset of vinum header. */
#define	GV_HDR_LEN	512	/* Size of vinum header. */
#define	GV_CFG_OFFSET	4608	/* Offset of first config copy. */
#define	GV_CFG_LEN	65536	/* Size of config copy. */

/* This is where the actual data starts. */
#define	GV_DATA_START	(GV_CFG_LEN * 2 + GV_CFG_OFFSET)
/* #define GV_DATA_START	(GV_CFG_LEN * 2 + GV_HDR_LEN) */

#define	GV_MAXDRIVENAME	32	/* Maximum length of a device name. */
#define	GV_MAXSDNAME	64	/* Maximum length of a subdisk name. */
#define	GV_MAXPLEXNAME	64	/* Maximum length of a plex name. */
#define	GV_MAXVOLNAME	64	/* Maximum length of a volume name. */

/* Command line flags. */
#define	GV_FLAG_R	0x01
#define	GV_FLAG_S	0x02
#define	GV_FLAG_V	0x04
#define	GV_FLAG_VV	0x08
#define	GV_FLAG_F	0x10

/* Object types. */
#define	GV_TYPE_VOL	1
#define	GV_TYPE_PLEX	2
#define	GV_TYPE_SD	3
#define	GV_TYPE_DRIVE	4

/* State changing flags. */
#define	GV_SETSTATE_FORCE	0x1
#define	GV_SETSTATE_CONFIG	0x2

/* Subdisk state bitmaps for plexes. */
#define	GV_SD_DOWNSTATE		0x01	/* Subdisk is down. */
#define	GV_SD_STALESTATE	0x02	/* Subdisk is stale. */
#define	GV_SD_INITSTATE		0x04	/* Subdisk is initializing. */
#define	GV_SD_UPSTATE		0x08	/* Subdisk is up. */

/* Synchronization/initialization request sizes. */
#define	GV_MIN_SYNCSIZE		512
#define	GV_MAX_SYNCSIZE		MAXPHYS
#define	GV_DFLT_SYNCSIZE	65536

/* Flags for BIOs, as they are processed within vinum. */
#define	GV_BIO_DONE	0x01
#define	GV_BIO_MALLOC	0x02
#define	GV_BIO_ONHOLD	0x04
#define	GV_BIO_SYNCREQ	0x08
#define	GV_BIO_SUCCEED	0x10
#define	GV_BIO_REBUILD	0x20
#define	GV_BIO_CHECK	0x40
#define	GV_BIO_PARITY	0x80
#define	GV_BIO_RETRY	0x100

/*
 * hostname is 256 bytes long, but we don't need to shlep multiple copies in
 * vinum.  We use the host name just to identify this system, and 32 bytes
 * should be ample for that purpose.
 */

#define	GV_HOSTNAME_LEN	32
struct gv_label {
	char	sysname[GV_HOSTNAME_LEN]; /* System name at creation time. */
	char	name[GV_MAXDRIVENAME];	/* Our name of the drive. */
	struct timeval	date_of_birth;	/* The time it was created ... */
	struct timeval	last_update;	/* ... and the time of last update. */
	off_t		drive_size;	/* Total size incl. headers. */
};

/* The 'header' of each valid vinum drive. */
struct gv_hdr {
	uint64_t	magic;
#define GV_OLD_MAGIC	0x494E2056494E4F00LL
#define GV_OLD_NOMAGIC	0x4E4F2056494E4F00LL
#define GV_MAGIC	0x56494E554D2D3100LL
#define GV_NOMAGIC	0x56494E554D2D2D00LL

	uint64_t	config_length;
	struct gv_label	label;
};

/* A single freelist entry of a drive. */
struct gv_freelist {
	off_t size;				/* Size of this free slot. */
	off_t offset;				/* Offset on the drive. */
	LIST_ENTRY(gv_freelist) freelist;
};

/*
 * Since we share structures between userland and kernel, we need this helper
 * struct instead of struct bio_queue_head and friends.  Maybe I find a proper
 * solution some day.
 */
struct gv_bioq {
	struct bio *bp;
	TAILQ_ENTRY(gv_bioq)	queue;
};

/* This struct contains the main vinum config. */
struct gv_softc {
	/*struct mtx config_mtx; XXX not yet */

	/* Linked lists of all objects in our setup. */
	LIST_HEAD(,gv_drive)	drives;		/* All drives. */
	LIST_HEAD(,gv_plex)	plexes;		/* All plexes. */
	LIST_HEAD(,gv_sd)	subdisks;	/* All subdisks. */
	LIST_HEAD(,gv_volume)	volumes;	/* All volumes. */

	struct g_geom		*geom;		/* Pointer to our VINUM geom. */
};

/* softc for a drive. */
struct gv_drive {
	char	name[GV_MAXDRIVENAME];		/* The name of this drive. */
	char	device[GV_MAXDRIVENAME];	/* Associated device. */
	int	state;				/* The state of this drive. */
#define	GV_DRIVE_DOWN	0
#define	GV_DRIVE_UP	1

	off_t	size;				/* Size of this drive. */
	off_t	avail;				/* Available space. */
	int	sdcount;			/* Number of subdisks. */

	int	flags;
#define	GV_DRIVE_THREAD_ACTIVE	0x01	/* Drive has an active worker thread. */
#define	GV_DRIVE_THREAD_DIE	0x02	/* Signal the worker thread to die. */
#define	GV_DRIVE_THREAD_DEAD	0x04	/* The worker thread has died. */
#define	GV_DRIVE_NEWBORN	0x08	/* The drive was just created. */

	struct gv_hdr	*hdr;			/* The drive header. */

	int freelist_entries;			/* Count of freelist entries. */
	LIST_HEAD(,gv_freelist)	freelist;	/* List of freelist entries. */
	LIST_HEAD(,gv_sd)	subdisks;	/* Subdisks on this drive. */
	LIST_ENTRY(gv_drive)	drive;		/* Entry in the vinum config. */

#ifdef _KERNEL
	struct bio_queue_head	*bqueue;	/* BIO queue of this drive. */
#else
	char			*padding;
#endif
	struct mtx		bqueue_mtx;	/* Mtx. to protect the queue. */

	struct g_geom	*geom;			/* The geom of this drive. */
	struct gv_softc	*vinumconf;		/* Pointer to the vinum conf. */
};

/* softc for a subdisk. */
struct gv_sd {
	char	name[GV_MAXSDNAME];	/* The name of this subdisk. */
	off_t	size;			/* The size of this subdisk. */
	off_t	drive_offset;		/* Offset in the underlying drive. */
	off_t	plex_offset;		/* Offset in the associated plex. */
	int	state;			/* The state of this subdisk. */
#define	GV_SD_DOWN		0
#define	GV_SD_STALE		1
#define	GV_SD_INITIALIZING	2
#define	GV_SD_REVIVING		3
#define	GV_SD_UP		4

	off_t	initialized;		/* Count of initialized bytes. */

	int	init_size;		/* Initialization read/write size. */
	int	init_error;		/* Flag error on initialization. */

	int	flags;
#define	GV_SD_NEWBORN		0x01	/* Subdisk was just created. */
#define	GV_SD_INITCANCEL	0x02	/* Cancel initialization process. */

	char drive[GV_MAXDRIVENAME];	/* Name of underlying drive. */
	char plex[GV_MAXPLEXNAME];	/* Name of associated plex. */

	struct gv_drive	*drive_sc;	/* Pointer to underlying drive. */
	struct gv_plex	*plex_sc;	/* Pointer to associated plex. */

	struct g_provider *provider;	/* The provider this sd represents. */
	struct g_consumer *consumer;	/* Consumer attached to our provider. */

	LIST_ENTRY(gv_sd) from_drive;	/* Subdisk list of underlying drive. */
	LIST_ENTRY(gv_sd) in_plex;	/* Subdisk list of associated plex. */
	LIST_ENTRY(gv_sd) sd;		/* Entry in the vinum config. */

	struct gv_softc	*vinumconf;	/* Pointer to the vinum config. */
};

/* softc for a plex. */
struct gv_plex {
	char	name[GV_MAXPLEXNAME];	/* The name of the plex. */
	off_t	size;			/* The size of the plex. */
	int	state;			/* The plex state. */
#define	GV_PLEX_DOWN		0
#define	GV_PLEX_INITIALIZING	1
#define	GV_PLEX_DEGRADED	2
#define	GV_PLEX_UP		3

	int	org;			/* The plex organisation. */
#define	GV_PLEX_DISORG	0
#define	GV_PLEX_CONCAT	1
#define	GV_PLEX_STRIPED	2
#define	GV_PLEX_RAID5	4

	int	stripesize;		/* The stripe size of the plex. */

	char	volume[GV_MAXVOLNAME];	/* Name of associated volume. */
	struct gv_volume *vol_sc;	/* Pointer to associated volume. */

	int	sdcount;		/* Number of subdisks in this plex. */
	int	sddown;			/* Number of subdisks that are down. */
	int	flags;
#define	GV_PLEX_ADDED		0x01	/* Added to an existing volume. */
#define	GV_PLEX_SYNCING		0x02	/* Plex is syncing from another plex. */
#define	GV_PLEX_THREAD_ACTIVE	0x04	/* Plex has an active RAID5 thread. */
#define	GV_PLEX_THREAD_DIE	0x08	/* Signal the RAID5 thread to die. */
#define	GV_PLEX_THREAD_DEAD	0x10	/* The RAID5 thread has died. */
#define	GV_PLEX_NEWBORN		0x20	/* The plex was just created. */

	off_t	synced;			/* Count of synced bytes. */

	struct mtx		bqueue_mtx; /* Lock for the BIO queue. */
#ifdef _KERNEL
	struct bio_queue_head	*bqueue; /* BIO queue. */
	struct bio_queue_head	*wqueue; /* Waiting BIO queue. */
#else
	char			*bpad, *wpad;
#endif
	TAILQ_HEAD(,gv_raid5_packet)	packets; /* RAID5 sub-requests. */

	LIST_HEAD(,gv_sd)   subdisks;	/* List of attached subdisks. */
	LIST_ENTRY(gv_plex) in_volume;	/* Plex list of associated volume. */
	LIST_ENTRY(gv_plex) plex;	/* Entry in the vinum config. */

	struct g_provider *provider;	/* The provider this plex represents. */
	struct g_consumer *consumer;	/* Consumer attached to our provider. */

	struct g_geom	*geom;		/* The geom of this plex. */
	struct gv_softc	*vinumconf;	/* Pointer to the vinum config. */
};

/* softc for a volume. */
struct gv_volume {
	char	name[GV_MAXVOLNAME];	/* The name of the volume. */
	off_t	size;			/* The size of the volume. */
	int	plexcount;		/* Number of plexes. */
	int	state;			/* The state of the volume. */
#define	GV_VOL_DOWN	0
#define	GV_VOL_UP	1

	int	flags;
#define	GV_VOL_THREAD_ACTIVE	0x01	/* Volume has an active thread. */
#define	GV_VOL_THREAD_DIE	0x02	/* Signal the thread to die. */
#define	GV_VOL_THREAD_DEAD	0x04	/* The thread has died. */

	struct mtx		bqueue_mtx; /* Lock for the BIO queue. */
#ifdef _KERNEL
	struct bio_queue_head	*bqueue; /* BIO queue. */
#else
	char			*padding;
#endif

	LIST_HEAD(,gv_plex)   plexes;	/* List of attached plexes. */
	LIST_ENTRY(gv_volume) volume;	/* Entry in vinum config. */

	struct gv_plex	*last_read_plex;
	struct g_geom	*geom;		/* The geom of this volume. */
	struct gv_softc	*vinumconf;	/* Pointer to the vinum config. */
};

#endif /* !_GEOM_VINUM_VAR_H */

/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 voidanix <voidanix@FreeBSD.org>
 */

#ifndef	_G_ZONED_H_
#define	_G_ZONED_H_

#include <sys/endian.h>

#define	G_ZONED_CLASS_NAME	"ZONED"
#define	G_ZONED_MAGIC		"GEOM::ZONED"
#define	G_ZONED_TABLE_MAGIC	"GEOM::ZONEDTBL"
/* Appended to the backing provider's name to name the zoned provider. */
#define	G_ZONED_SUFFIX		".zoned"

/*
 * Version history:
 * 1 - Initial version number.
 */
#define	G_ZONED_VERSION		1

/*
 * On-disk layout, carved out of the end of the backing provider:
 *
 *   [ ... usable (zoned) region ... | table header | zone entries | metadata ]
 *                                     (1 sector)     (N entries)   (last sec)
 *
 * Table header + zone entries hold the live per-zone state and are maintained
 * entirely by the kernel.
 */
/* On-disk zone-state table entry */
struct g_zoned_disk_entry {
	uint8_t		de_type;		/* Zone type. */
	uint8_t		de_condition;		/* Zone condition. */
	uint8_t		de_flags;		/* Zone attribute flags. */
	uint8_t		de_reserved[5];
	uint64_t	de_write_pointer_lba;	/* Write pointer LBA. */
};
#define	G_ZONED_ENTRY_SIZE	sizeof(struct g_zoned_disk_entry)
_Static_assert(G_ZONED_ENTRY_SIZE == 16, "on-disk zone entry layout changed");

/*
 * Maximum number of conventional-zone ranges that fit in the metadata.
 */
#define	G_ZONED_MAXCONV		16

/*
 * A run of consecutive conventional (non-sequential-write) zones. ZBC/ZAC
 * identify zones by their start LBA, a 64-bit field; zone indices get the same
 * width.
 */
struct g_zoned_convrange {
	uint64_t	cr_first;	/* First zone of the range. */
	uint64_t	cr_count;	/* Number of zones in the range. */
};

/* md_flags bits. */
#define	G_ZONED_MD_RESTRICTED_READS	0x00000001 /* URSWRZ off. */
#define	G_ZONED_MD_FLAGSMASK		G_ZONED_MD_RESTRICTED_READS

struct g_zoned_metadata {
	char		md_magic[16];	/* Magic value. */
	uint32_t	md_version;	/* Version number. */
	uint32_t	md_id;		/* Unique ID. */
	uint64_t	md_zonesize;	/* Zone size in bytes. */
	uint32_t	md_nzones;	/* Number of zones. */
	uint32_t	md_sectorsize;	/* Provider sector size in bytes. */
	uint64_t	md_provsize;	/* Provider size in bytes. */
	uint32_t	md_nconv;	/* Conventional ranges in use. */
	struct g_zoned_convrange md_conv[G_ZONED_MAXCONV];
	uint32_t	md_flags;	/* G_ZONED_MD_* flags. */
	uint32_t	md_maxopen;	/* Open zone limit, 0 = unlimited. */
};

static __inline void
zoned_metadata_encode(const struct g_zoned_metadata *md, u_char *data)
{
	u_char *p;
	u_int i;

	bcopy(md->md_magic, data, sizeof(md->md_magic));
	le32enc(data + 16, md->md_version);
	le32enc(data + 20, md->md_id);
	le64enc(data + 24, md->md_zonesize);
	le32enc(data + 32, md->md_nzones);
	le32enc(data + 36, md->md_sectorsize);
	le64enc(data + 40, md->md_provsize);
	le32enc(data + 48, md->md_nconv);
	p = data + 52;
	for (i = 0; i < G_ZONED_MAXCONV; i++, p += 16) {
		le64enc(p, md->md_conv[i].cr_first);
		le64enc(p + 8, md->md_conv[i].cr_count);
	}
	le32enc(p, md->md_flags);
	le32enc(p + 4, md->md_maxopen);
}

static __inline void
zoned_metadata_decode(const u_char *data, struct g_zoned_metadata *md)
{
	const u_char *p;
	u_int i;

	bcopy(data, md->md_magic, sizeof(md->md_magic));
	md->md_version = le32dec(data + 16);
	md->md_id = le32dec(data + 20);
	md->md_zonesize = le64dec(data + 24);
	md->md_nzones = le32dec(data + 32);
	md->md_sectorsize = le32dec(data + 36);
	md->md_provsize = le64dec(data + 40);
	md->md_nconv = le32dec(data + 48);
	p = data + 52;
	for (i = 0; i < G_ZONED_MAXCONV; i++, p += 16) {
		md->md_conv[i].cr_first = le64dec(p);
		md->md_conv[i].cr_count = le64dec(p + 8);
	}
	md->md_flags = le32dec(p);
	md->md_maxopen = le32dec(p + 4);
}

/*
 * Number of zones a provider of a given geometry can hold after reserving room
 * at the tail for the metadata sector and the zone-state table. Returns 0 if
 * the zone size does not leave room for even a single zone.
 */
static __inline uint32_t
g_zoned_nzones(off_t mediasize, off_t zonesize, u_int secsize)
{
	uint64_t nmax, reserve;

	if (zonesize <= 0 || secsize == 0 || mediasize <= 0)
		return (0);
	/* Over-reserve using the zone count that ignores the reservation. */
	nmax = (uint64_t)mediasize / (uint64_t)zonesize;
	if (nmax == 0)
		return (0);
	/* Metadata sector + table header sector + sector-aligned entries. */
	reserve = 2 * (uint64_t)secsize +
	    roundup2(nmax * G_ZONED_ENTRY_SIZE, (uint64_t)secsize);
	if ((uint64_t)mediasize <= reserve)
		return (0);
	return ((uint32_t)(((uint64_t)mediasize - reserve) /
	    (uint64_t)zonesize));
}

#ifdef _KERNEL
#define	G_ZONED_DEBUG(lvl, ...) \
    _GEOM_DEBUG("GEOM_ZONED", g_zoned_debug, (lvl), NULL, __VA_ARGS__)
#define	G_ZONED_LOGREQLVL(lvl, bp, ...) \
    _GEOM_DEBUG("GEOM_ZONED", g_zoned_debug, (lvl), (bp), __VA_ARGS__)
#define	G_ZONED_LOGREQ(bp, ...)	G_ZONED_LOGREQLVL(2, bp, __VA_ARGS__)

/*
 * Live state of one emulated zoned device. The sc_zones array mirrors the
 * on-disk zone-state table; dirty entries are re-encoded and written out
 * lazily on BIO_FLUSH. Zone states are kept in RAM.
 */
struct g_zoned_softc {
	struct mtx			 sc_lock;
	uint32_t			 sc_id;		/* Unique ID. */
	off_t				 sc_zonesize;	/* Zone bytes. */
	u_int				 sc_secsize;	/* Sector bytes. */
	uint64_t			 sc_zonesecs;	/* Zone sectors. */
	uint32_t			 sc_nzones;	/* Number of zones. */
	uint64_t			 sc_maxlba;	/* Last LBA + 1. */
	uint32_t			 sc_nconv;	/* Conv. ranges. */
	uint32_t			 sc_convzones;	/* Conv. zones. */
	uint32_t			 sc_maxopen;	/* 0 = unlimited. */
	uint32_t			 sc_nopen;	/* Open zones. */
	bool				 sc_rdrestrict;	/* URSWRZ off. */
	struct g_zoned_convrange	 sc_conv[G_ZONED_MAXCONV];
	struct disk_zone_rep_entry	*sc_zones;	/* sc_nzones long. */
	/* Zone-state table placement and dirty tracking. */
	uint32_t			 sc_tabsecs;	/* Table sectors. */
	off_t				 sc_taboff;	/* Table offset. */
	bool				 sc_dirty;	/* Pending writes. */
	bool				 sc_hdrdirty;	/* Header dirty. */
	uint32_t			 sc_dirtylo;	/* First dirty zone. */
	uint32_t			 sc_dirtyhi;	/* Last dirty zone. */
	/* Statistics. */
	uintmax_t			 sc_reads;
	uintmax_t			 sc_writes;
	uintmax_t			 sc_readbytes;
	uintmax_t			 sc_wrotebytes;
	uintmax_t			 sc_zonecmds;
};

static __inline void
g_zoned_entry_encode(const struct disk_zone_rep_entry *z, u_char *data)
{
	struct g_zoned_disk_entry *de = (struct g_zoned_disk_entry *)data;

	de->de_type = z->zone_type;
	de->de_condition = z->zone_condition;
	de->de_flags = z->zone_flags;
	memset(de->de_reserved, 0, sizeof(de->de_reserved));
	de->de_write_pointer_lba = htole64(z->write_pointer_lba);
}

static __inline void
g_zoned_entry_decode(const u_char *data, struct disk_zone_rep_entry *z)
{
	const struct g_zoned_disk_entry *de =
	    (const struct g_zoned_disk_entry *)data;

	z->zone_type = de->de_type;
	z->zone_condition = de->de_condition;
	z->zone_flags = de->de_flags;
	z->write_pointer_lba = le64toh(de->de_write_pointer_lba);
}
#endif	/* _KERNEL */

#endif	/* _G_ZONED_H_ */

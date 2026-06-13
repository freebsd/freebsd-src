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
 * Sentinel for "unlimited open/active sequential zones".
 * See SVPD_ZBDC_MAX_SEQ_UNLIMITED.
 */
#define	G_ZONED_SEQ_UNLIMITED	0xffffffff

/*
 * On-disk layout, carved out of the end of the backing provider:
 *
 *   [ ... usable (zoned) region ... | table header | zone entries | metadata ]
 *                                     (1 sector)     (N entries)   (last sec)
 *
 * Table header + zone entries hold the live per-zone state and are maintained
 * entirely by the kernel.
 */
/* On-disk zone-state table header, at the start of the table's first sector. */
struct g_zoned_table_hdr {
	char		th_magic[16];	/* G_ZONED_TABLE_MAGIC. */
	uint32_t	th_version;	/* Version number. */
	uint32_t	th_nzones;	/* Zone entries that follow. */
};
_Static_assert(sizeof(struct g_zoned_table_hdr) == 24,
    "on-disk zone table header layout changed");

/* On-disk zone-state table entry. */
struct g_zoned_disk_entry {
	uint8_t		de_type;		/* Zone type. */
	uint8_t		de_condition;		/* Zone condition. */
	uint8_t		de_flags;		/* Zone attribute flags. */
	uint8_t		de_reserved;
	uint32_t	de_write_pointer;	/* WP offset in the zone. */
};
#define	G_ZONED_ENTRY_SIZE	(sizeof(struct g_zoned_disk_entry))
_Static_assert(G_ZONED_ENTRY_SIZE == 8, "on-disk zone entry layout changed");

/*
 * write_pointer_lba for zones that have no valid write pointer.  Report all
 * ones across the whole 64-bit field, like a ZBC drive might.
 */
#define	G_ZONED_WP_NONE_LBA	0xffffffffffffffff
/* On-disk, zone-relative counterpart in de_write_pointer. */
#define	G_ZONED_WP_NONE		0xffffffff
/*
 * Zone size cap (in sectors) keeping every valid zone-relative write pointer,
 * including that of a full zone (== zone length), below G_ZONED_WP_NONE.
 */
#define	G_ZONED_MAXZONESECS	(G_ZONED_WP_NONE - 1)

/*
 * Maximum number of conventional-zone ranges that fit in the metadata.
 */
#define	G_ZONED_MAXCONV		16

/* Maximum number of zones for a single device. */
#define	G_ZONED_MAXZONES	UINT32_MAX

/*
 * A run of consecutive conventional (non-sequential-write) zones.  ZBC/ZAC
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
	uint32_t	md_sectorsize;	/* Provider sector size in bytes. */
	uint32_t	md_nconv;	/* Conventional ranges in use. */
	uint64_t	md_provsize;	/* Provider size in bytes. */
	struct g_zoned_convrange md_conv[G_ZONED_MAXCONV];
	uint32_t	md_flags;	/* G_ZONED_MD_* flags. */
	uint32_t	md_maxopen;	/* Open zone limit, 0 = unlimited. */
};
_Static_assert(sizeof(struct g_zoned_metadata) == 312,
    "on-disk metadata layout changed");
_Static_assert(__offsetof(struct g_zoned_metadata, md_conv) == 48,
    "on-disk metadata layout changed");

/*
 * The on-disk image has the same layout as the struct, so marshalling is a
 * matter of byte order.  The image is copied in and out rather than cast to,
 * as the buffers holding it are only guaranteed to be byte aligned.
 */
static __inline void
zoned_metadata_encode(const struct g_zoned_metadata *md, u_char *data)
{
	struct g_zoned_metadata d;
	u_int i;

	memset(&d, 0, sizeof(d));
	memcpy(d.md_magic, md->md_magic, sizeof(d.md_magic));
	d.md_version = htole32(md->md_version);
	d.md_id = htole32(md->md_id);
	d.md_zonesize = htole64(md->md_zonesize);
	d.md_sectorsize = htole32(md->md_sectorsize);
	d.md_nconv = htole32(md->md_nconv);
	d.md_provsize = htole64(md->md_provsize);
	for (i = 0; i < G_ZONED_MAXCONV; i++) {
		d.md_conv[i].cr_first = htole64(md->md_conv[i].cr_first);
		d.md_conv[i].cr_count = htole64(md->md_conv[i].cr_count);
	}
	d.md_flags = htole32(md->md_flags);
	d.md_maxopen = htole32(md->md_maxopen);
	memcpy(data, &d, sizeof(d));
}

static __inline void
zoned_metadata_decode(const u_char *data, struct g_zoned_metadata *md)
{
	struct g_zoned_metadata d;
	u_int i;

	memcpy(&d, data, sizeof(d));
	memcpy(md->md_magic, d.md_magic, sizeof(md->md_magic));
	md->md_version = le32toh(d.md_version);
	md->md_id = le32toh(d.md_id);
	md->md_zonesize = le64toh(d.md_zonesize);
	md->md_sectorsize = le32toh(d.md_sectorsize);
	md->md_nconv = le32toh(d.md_nconv);
	md->md_provsize = le64toh(d.md_provsize);
	for (i = 0; i < G_ZONED_MAXCONV; i++) {
		md->md_conv[i].cr_first = le64toh(d.md_conv[i].cr_first);
		md->md_conv[i].cr_count = le64toh(d.md_conv[i].cr_count);
	}
	md->md_flags = le32toh(d.md_flags);
	md->md_maxopen = le32toh(d.md_maxopen);
}

/*
 * Number of zones a provider of a given geometry can hold after reserving room
 * at the tail for the metadata sector and the zone-state table.  Returns 0 if
 * the zone size leaves room for no zone at all, or if it yields more zones
 * than G_ZONED_MAXZONES.
 */
static __inline uint32_t
g_zoned_nzones(off_t mediasize, off_t zonesize, u_int secsize)
{
	uint64_t nmax, nzones, reserve;

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
	nzones = ((uint64_t)mediasize - reserve) / (uint64_t)zonesize;
	/*
	 * Reject rather than truncate, as wrapped counts can quietly describe
	 * devices of the wrong size.  Note that nzones must be wider than the
	 * bound for this to work.
	 */
	if (nzones > G_ZONED_MAXZONES)
		return (0);
	return ((uint32_t)nzones);
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
zoned_table_hdr_encode(const struct g_zoned_table_hdr *th, u_char *data)
{
	struct g_zoned_table_hdr d;

	memset(&d, 0, sizeof(d));
	memcpy(d.th_magic, th->th_magic, sizeof(d.th_magic));
	d.th_version = htole32(th->th_version);
	d.th_nzones = htole32(th->th_nzones);
	memcpy(data, &d, sizeof(d));
}

static __inline void
zoned_table_hdr_decode(const u_char *data, struct g_zoned_table_hdr *th)
{
	struct g_zoned_table_hdr d;

	memcpy(&d, data, sizeof(d));
	memcpy(th->th_magic, d.th_magic, sizeof(th->th_magic));
	th->th_version = le32toh(d.th_version);
	th->th_nzones = le32toh(d.th_nzones);
}

static __inline void
g_zoned_entry_encode(const struct disk_zone_rep_entry *z, u_char *data)
{
	struct g_zoned_disk_entry de;

	de.de_type = z->zone_type;
	de.de_condition = z->zone_condition;
	de.de_flags = z->zone_flags;
	de.de_reserved = 0;
	de.de_write_pointer = htole32(
	    z->write_pointer_lba == G_ZONED_WP_NONE_LBA ? G_ZONED_WP_NONE :
	    (uint32_t)(z->write_pointer_lba - z->zone_start_lba));
	memcpy(data, &de, sizeof(de));
}

static __inline void
g_zoned_entry_decode(const u_char *data, struct disk_zone_rep_entry *z,
    uint64_t start_lba)
{
	struct g_zoned_disk_entry de;
	uint32_t wp;

	memcpy(&de, data, sizeof(de));
	z->zone_type = de.de_type;
	z->zone_condition = de.de_condition;
	z->zone_flags = de.de_flags;
	wp = le32toh(de.de_write_pointer);
	z->write_pointer_lba = (wp == G_ZONED_WP_NONE) ? G_ZONED_WP_NONE_LBA :
	    start_lba + wp;
}
#endif	/* _KERNEL */

#endif	/* _G_ZONED_H_ */

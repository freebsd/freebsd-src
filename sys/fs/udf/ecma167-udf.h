/*-
 * Copyright (c) 2001, 2002 Scott Long <scottl@freebsd.org>
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

/* ecma167-udf.h */
/* Structure/definitions/constants a la ECMA 167 rev. 3 */

/* Tag identifiers */
enum {
	TAGID_PRI_VOL =		1,
	TAGID_ANCHOR =		2,
	TAGID_VOL = 		3,
	TAGID_IMP_VOL =		4,
	TAGID_PARTITION =	5,
	TAGID_LOGVOL =		6,
	TAGID_UNALLOC_SPACE =	7,
	TAGID_TERM =		8,
	TAGID_LOGVOL_INTEGRITY = 9,
	TAGID_FSD =		256,
	TAGID_FID =		257,
	TAGID_FENTRY =		261
};

/* Descriptor tag [3/7.2] */
struct desc_tag {
	u_int16_t	id;
	u_int16_t	descriptor_ver;
	u_int8_t	cksum;
	u_int8_t	reserved;
	u_int16_t	serial_num;
	u_int16_t	desc_crc;
	u_int16_t	desc_crc_len;
	u_int32_t	tag_loc;
} __attribute__ ((packed));

/* Recorded Address [4/7.1] */
struct lb_addr {
	u_int32_t	lb_num;
	u_int16_t	part_num;
} __attribute__ ((packed));

/* Extent Descriptor [3/7.1] */
struct extent_ad {
	u_int32_t	len;
	u_int32_t	loc;
} __attribute__ ((packed));

/* Short Allocation Descriptor [4/14.14.1] */
struct short_ad {
	u_int32_t	len;
	u_int32_t	pos;
} __attribute__ ((packed));

/* Long Allocation Descriptor [4/14.14.2] */
struct long_ad {
	u_int32_t	len;
	struct lb_addr	loc;
	u_int16_t	ad_flags;
	u_int32_t	ad_id;
} __attribute__ ((packed));

/* Extended Allocation Descriptor [4/14.14.3] */
struct ext_ad {
	u_int32_t	ex_len;
	u_int32_t	rec_len;
	u_int32_t	inf_len;
	struct lb_addr	ex_loc;
	u_int8_t	reserved[2];
} __attribute__ ((packed));

union icb {
	struct short_ad	s_ad;
	struct long_ad	l_ad;
	struct ext_ad	e_ad;
};

/* Character set spec [1/7.2.1] */
struct charspec {
	u_int8_t	type;
	u_int8_t	inf[63];
} __attribute__ ((packed));

/* Timestamp [1/7.3] */
struct timestamp {
	u_int16_t	type_tz;
	u_int16_t	year;
	u_int8_t	month;
	u_int8_t	day;
	u_int8_t	hour;
	u_int8_t	minute;
	u_int8_t	second;
	u_int8_t	centisec;
	u_int8_t	hund_usec;
	u_int8_t	usec;
} __attribute__ ((packed));

/* Entity Identifier [1/7.4] */
#define	UDF_REGID_ID_SIZE	23
struct regid {
	u_int8_t	flags;
	u_int8_t	id[UDF_REGID_ID_SIZE];
	u_int8_t	id_suffix[8];
} __attribute__ ((packed));

/* ICB Tag [4/14.6] */
struct icb_tag {
	u_int32_t	prev_num_dirs;
	u_int16_t	strat_type;
	u_int8_t	strat_param[2];
	u_int16_t	max_num_entries;
	u_int8_t	reserved;
	u_int8_t	file_type;
	struct lb_addr	parent_icb;
	u_int16_t	flags;
} __attribute__ ((packed));
#define	UDF_ICB_TAG_FLAGS_SETUID	0x40
#define	UDF_ICB_TAG_FLAGS_SETGID	0x80
#define	UDF_ICB_TAG_FLAGS_STICKY	0x100

/* Anchor Volume Descriptor Pointer [3/10.2] */
struct anchor_vdp {
	struct desc_tag		tag;
	struct extent_ad	main_vds_ex;
	struct extent_ad	reserve_vds_ex;
} __attribute__ ((packed));

/* Volume Descriptor Pointer [3/10.3] */
struct vol_desc_ptr {
	struct desc_tag		tag;
	u_int32_t		vds_number;
	struct extent_ad	next_vds_ex;
} __attribute__ ((packed));

/* Primary Volume Descriptor [3/10.1] */
struct pri_vol_desc {
	struct desc_tag		tag;
	u_int32_t		seq_num;
	u_int32_t		pdv_num;
	char			vol_id[32];
	u_int16_t		vds_num;
	u_int16_t		max_vol_seq;
	u_int16_t		ichg_lvl;
	u_int16_t		max_ichg_lvl;
	u_int32_t		charset_list;
	u_int32_t		max_charset_list;
	char			volset_id[128];
	struct charspec		desc_charset;
	struct charspec		explanatory_charset;
	struct extent_ad	vol_abstract;
	struct extent_ad	vol_copyright;
	struct regid		app_id;
	struct timestamp	time;
	struct regid		imp_id;
	u_int8_t		imp_use[64];
	u_int32_t		prev_vds_lov;
	u_int16_t		flags;
	u_int8_t		reserved[22];
} __attribute__ ((packed));

/* Logical Volume Descriptor [3/10.6] */
struct logvol_desc {
	struct desc_tag		tag;
	u_int32_t		seq_num;
	struct charspec		desc_charset;
	char			logvol_id[128];
	u_int32_t		lb_size;
	struct regid		domain_id;
	union {
		struct long_ad	fsd_loc;
		u_int8_t	logvol_content_use[16];
	} _lvd_use;
	u_int32_t		mt_l; /* Partition map length */
	u_int32_t		n_pm; /* Number of partition maps */
	struct regid		imp_id;
	u_int8_t		imp_use[128];
	struct extent_ad	integrity_seq_id;
	u_int8_t		maps[1];
} __attribute__ ((packed));

#define	UDF_PMAP_SIZE	64

/* Type 1 Partition Map [3/10.7.2] */
struct part_map_1 {
	u_int8_t		type;
	u_int8_t		len;
	u_int16_t		vol_seq_num;
	u_int16_t		part_num;
} __attribute__ ((packed));

/* Type 2 Partition Map [3/10.7.3] */
struct part_map_2 {
	u_int8_t		type;
	u_int8_t		len;
	u_int8_t		part_id[62];
} __attribute__ ((packed));

/* Virtual Partition Map [UDF 2.01/2.2.8] */
struct part_map_virt {
	u_int8_t		type;
	u_int8_t		len;
	u_int8_t		reserved[2];
	struct regid		id;
	u_int16_t		vol_seq_num;
	u_int16_t		part_num;
	u_int8_t		reserved1[24];
} __attribute__ ((packed));

/* Sparable Partition Map [UDF 2.01/2.2.9] */
struct part_map_spare {
	u_int8_t		type;
	u_int8_t		len;
	u_int8_t		reserved[2];
	struct regid		id;
	u_int16_t		vol_seq_num;
	u_int16_t		part_num;
	u_int16_t		packet_len;
	u_int8_t		n_st;	/* Number of Sparing Tables */
	u_int8_t		reserved1;
	u_int32_t		st_size;
	u_int32_t		st_loc[1];
} __attribute__ ((packed));

union udf_pmap {
	u_int8_t		data[UDF_PMAP_SIZE];
	struct part_map_1	pm1;
	struct part_map_2	pm2;
	struct part_map_virt	pmv;
	struct part_map_spare	pms;
};

/* Sparing Map Entry [UDF 2.01/2.2.11] */
struct spare_map_entry {
	u_int32_t		org;
	u_int32_t		map;
} __attribute__ ((packed));

/* Sparing Table [UDF 2.01/2.2.11] */
struct udf_sparing_table {
	struct desc_tag		tag;
	struct regid		id;
	u_int16_t		rt_l;	/* Relocation Table len */
	u_int8_t		reserved[2];
	u_int32_t		seq_num;
	struct spare_map_entry	entries[1];
} __attribute__ ((packed));

/* Partition Descriptor [3/10.5] */
struct part_desc {
	struct desc_tag	tag;
	u_int32_t	seq_num;
	u_int16_t	flags;
	u_int16_t	part_num;
	struct regid	contents;
	u_int8_t	contents_use[128];
	u_int32_t	access_type;
	u_int32_t	start_loc;
	u_int32_t	part_len;
	struct regid	imp_id;
	u_int8_t	imp_use[128];
	u_int8_t	reserved[156];
} __attribute__ ((packed));

/* File Set Descriptor [4/14.1] */
struct fileset_desc {
	struct desc_tag		tag;
	struct timestamp	time;
	u_int16_t		ichg_lvl;
	u_int16_t		max_ichg_lvl;
	u_int32_t		charset_list;
	u_int32_t		max_charset_list;
	u_int32_t		fileset_num;
	u_int32_t		fileset_desc_num;
	struct charspec		logvol_id_charset;
	char			logvol_id[128];
	struct charspec		fileset_charset;
	char			fileset_id[32];
	char			copyright_file_id[32];
	char			abstract_file_id[32];
	struct long_ad		rootdir_icb;
	struct regid		domain_id;
	struct long_ad		next_ex;
	struct long_ad		streamdir_icb;
	u_int8_t		reserved[32];
} __attribute__ ((packed));

/* File Identifier Descriptor [4/14.4] */
struct fileid_desc {
	struct desc_tag	tag;
	u_int16_t	file_num;
	u_int8_t	file_char;
	u_int8_t	l_fi;	/* Length of file identifier area */
	struct long_ad	icb;
	u_int16_t	l_iu;	/* Length of implementaion use area */
	u_int8_t	data[1];
} __attribute__ ((packed));
#define	UDF_FID_SIZE	38

/* File Entry [4/14.9] */
struct file_entry {
	struct desc_tag		tag;
	struct icb_tag		icbtag;
	u_int32_t		uid;
	u_int32_t		gid;
	u_int32_t		perm;
	u_int16_t		link_cnt;
	u_int8_t		rec_format;
	u_int8_t		rec_disp_attr;
	u_int32_t		rec_len;
	u_int64_t		inf_len;
	u_int64_t		logblks_rec;
	struct timestamp	atime;
	struct timestamp	mtime;
	struct timestamp	attrtime;
	u_int32_t		ckpoint;
	struct long_ad		ex_attr_icb;
	struct regid		imp_id;
	u_int64_t		unique_id;
	u_int32_t		l_ea;	/* Length of extended attribute area */
	u_int32_t		l_ad;	/* Length of allocation descriptors */
	u_int8_t		data[1];
} __attribute ((packed));
#define	UDF_FENTRY_SIZE	176
#define	UDF_FENTRY_PERM_USER_MASK	0x07
#define	UDF_FENTRY_PERM_GRP_MASK	0xE0
#define	UDF_FENTRY_PERM_OWNER_MASK	0x1C00

union dscrptr {
	struct desc_tag		tag;
	struct anchor_vdp	avdp;
	struct vol_desc_ptr	vdp;
	struct pri_vol_desc	pvd;
	struct logvol_desc	lvd;
	struct part_desc	pd;
	struct fileset_desc	fsd;
	struct fileid_desc	fid;
	struct file_entry	fe;
};

/* Useful defines */

#define	GETICB(ad_type, fentry, offset)	\
	(struct ad_type *)&fentry->data[offset]

#define	GETICBLEN(ad_type, icb)	((struct ad_type *)(icb))->len

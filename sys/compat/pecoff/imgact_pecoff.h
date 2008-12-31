/* $NetBSD$	 */
/* $FreeBSD: src/sys/compat/pecoff/imgact_pecoff.h,v 1.2.18.1 2008/11/25 02:59:29 kensmith Exp $	*/
/*-
 * Copyright (c) 2000 Masaru OKI
 */

#ifndef _PECOFF_EXEC_H_
#define _PECOFF_EXEC_H_

struct pecoff_dos_filehdr {
	u_int16_t       d_magic;/* +0x00 'MZ' */
	u_int8_t        d_stub[0x3a];
	u_int32_t       d_peofs;/* +0x3c */
};

#define PECOFF_DOS_MAGIC 0x5a4d
#define PECOFF_DOS_HDR_SIZE (sizeof(struct pecoff_dos_filehdr))

#define DOS_BADMAG(dp) ((dp)->d_magic != PECOFF_DOS_MAGIC)

/*
 * COFF file header
 */

struct coff_filehdr {
	u_short         f_magic;/* magic number */
	u_short         f_nscns;/* # of sections */
	long            f_timdat;	/* timestamp */
	long            f_symptr;	/* file offset of symbol table */
	long            f_nsyms;/* # of symbol table entries */
	u_short         f_opthdr;	/* size of optional header */
	u_short         f_flags;/* flags */
};

/*
 * COFF system header
 */

struct coff_aouthdr {
	short           a_magic;
	short           a_vstamp;
	long            a_tsize;
	long            a_dsize;
	long            a_bsize;
	long            a_entry;
	long            a_tstart;
	long            a_dstart;
};

/* magic */
#define COFF_OMAGIC	0407	/* text not write-protected; data seg is
				 * contiguous with text */
#define COFF_NMAGIC	0410	/* text is write-protected; data starts at
				 * next seg following text */
#define COFF_ZMAGIC	0413	/* text and data segs are aligned for direct
				 * paging */
#define COFF_SMAGIC	0443	/* shared lib */

struct pecoff_imghdr {
	long            i_vaddr;
	long            i_size;
};

struct pecoff_opthdr {
	long            w_base;
	long            w_salign;
	long            w_falign;
	long            w_osvers;
	long            w_imgvers;
	long            w_subvers;
	long            w_rsvd;
	long            w_imgsize;
	long            w_hdrsize;
	long            w_chksum;
	u_short         w_subsys;
	u_short         w_dllflags;
	long            w_ssize;
	long            w_cssize;
	long            w_hsize;
	long            w_chsize;
	long            w_lflag;
	long            w_nimghdr;
	struct pecoff_imghdr w_imghdr[16];
};

/*
 * COFF section header
 */

struct coff_scnhdr {
	char            s_name[8];
	long            s_paddr;
	long            s_vaddr;
	long            s_size;
	long            s_scnptr;
	long            s_relptr;
	long            s_lnnoptr;
	u_short         s_nreloc;
	u_short         s_nlnno;
	long            s_flags;
};

/* s_flags */
#define COFF_STYP_REG		0x00
#define COFF_STYP_DSECT		0x01
#define COFF_STYP_NOLOAD	0x02
#define COFF_STYP_GROUP		0x04
#define COFF_STYP_PAD		0x08
#define COFF_STYP_COPY		0x10
#define COFF_STYP_TEXT		0x20
#define COFF_STYP_DATA		0x40
#define COFF_STYP_BSS		0x80
#define COFF_STYP_INFO		0x200
#define COFF_STYP_OVER		0x400
#define COFF_STYP_SHLIB		0x800
/* s_flags for PE */
#define COFF_STYP_DISCARD	0x2000000
#define COFF_STYP_EXEC		0x20000000
#define COFF_STYP_READ		0x40000000
#define COFF_STYP_WRITE		0x80000000

struct pecoff_args {
	u_long          a_base;
	u_long          a_entry;
	u_long          a_end;
	u_long          a_subsystem;
	struct pecoff_imghdr a_imghdr[16];
	u_long          a_ldbase;
	u_long          a_ldexport;
};

#define COFF_LDPGSZ 4096	
#define COFF_ALIGN(a) ((a) & ~(COFF_LDPGSZ - 1))
#define COFF_ROUND(a) COFF_ALIGN((a) + COFF_LDPGSZ - 1)

#define COFF_HDR_SIZE \
	(sizeof(struct coff_filehdr) + sizeof(struct coff_aouthdr))

#define PECOFF_HDR_SIZE (COFF_HDR_SIZE + sizeof(struct pecoff_opthdr))


#endif

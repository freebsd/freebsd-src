/*
 *	from db.h	4.16 (Berkeley) 6/1/90
 *	$Id: db_defs.h,v 8.41 2001/02/08 02:05:50 marka Exp $
 */

/*
 * Copyright (c) 1985, 1990
 *    The Regents of the University of California.  All rights reserved.
 * 
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
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * Portions Copyright (c) 1996-2000 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * Global definitions for data base routines.
 */

	/* max length of data in RR data field */
#define MAXDATA		(2*MAXDNAME + 5*INT32SZ)

	/* max length of data in a TXT RR segment */
#define MAXCHARSTRING 255

#define DB_ROOT_TIMBUF	3600
#define TIMBUF		300

#define	DICT_INDEXBITS	24
#define	DICT_MAXLENGTH	127
#define	DICT_INSERT_P	0x0001

/* Average hash chain depths. */
#define	AVGCH_MARSHAL	5
#define	AVGCH_NLOOKUP	3

/* Nonstandard maximum class to force better packing. */
#define	ZONE_BITS	24
#define	CLASS_BITS	8
#define	ZONE_MAX	((1<<ZONE_BITS)-1)
#define	CLASS_MAX	((1<<CLASS_BITS)-1)

/*
 * Hash table structures.
 */
/*
 * XXX
 * For IPv6 transport support we need a seperate reference counted
 * database of source addresses and d_addr should become a union with
 * a pointer into that database.  A bit can be robbed from d_rode to
 * indicate what the union is being used for.  This should require less
 * memory than making d_addr a union of struct in6_addr and struct in_addr.
 */
struct databuf {
	struct databuf	*d_next;	/* linked list */
	struct in_addr	d_addr;		/* NS from whence this came */
	u_int32_t	d_ttl;		/* time to live */
					/* if d_zone == DB_Z_CACHE, then
					 * d_ttl is actually the time when
					 * the record will expire.
					 * otherwise (for authoritative
					 * master and slave zones),
					 * d_ttl is the time to live.
					 */
	unsigned	d_zone :ZONE_BITS; /* zone number or 0 for the cache */
	unsigned	d_class :CLASS_BITS; /* class number (nonstandard limit) */
	unsigned	d_flags :4;	/* DB_F_{??????} */
	unsigned	d_secure :2;	/* DB_S_{??????} */
	unsigned	d_cred :3;	/* DB_C_{??????} */
	unsigned	d_clev :6;
	unsigned	d_rcode :4;	/* rcode for negative caching */
	unsigned	d_mark :3;	/* place to mark data */
	int16_t		d_type;		/* type number */
	int16_t		d_size;		/* size of data area */
	u_int32_t	d_rcnt;
#ifdef HITCOUNTS
	u_int32_t	d_hitcnt;	/* Number of requests for this data. */
#endif /* HITCOUNTS */
	u_int16_t	d_nstime;	/* NS response time, milliseconds */
	u_char		d_data[sizeof(void*)]; /* dynamic (padded) */
};
#define DATASIZE(n) (sizeof(struct databuf) - sizeof(void*) + n)

#ifdef HITCOUNTS
extern u_int32_t	db_total_hits;
#endif /* HITCOUNTS */

#ifdef BIND_UPDATE
/*
 * d_mark definitions
 */
#define D_MARK_DELETED  0x01
#define D_MARK_ADDED	0x02
#define D_MARK_FOUND	0x04
#endif

/*
 * d_flags definitions
 */
#define DB_F_HINT	0x01		/* databuf belongs to fcachetab */
#define DB_F_ACTIVE	0x02		/* databuf is linked into a cache */
#define DB_F_FREE	0x04		/* databuf has been freed */
#define DB_F_LAME	0x08		/* databuf may refer to lame server */

/*
 * d_cred definitions
 */
#define	DB_C_ZONE	4		/* authoritative zone - best */
#define	DB_C_AUTH	3		/* authoritative answer */
#define	DB_C_ANSWER	2		/* non-authoritative answer */
#define	DB_C_ADDITIONAL	1		/* additional data */
#define	DB_C_CACHE	0		/* cache - worst */

/*
 * d_secure definitions
 */
#define	DB_S_SECURE	2		/* secure (verified) data */
#define	DB_S_INSECURE	1		/* insecure data */
#define	DB_S_FAILED	0		/* data that failed a security check */

struct namebuf {
	u_int		n_hashval;	/* hash value of _n_name */
	struct namebuf	*n_next;	/* linked list */
	struct databuf	*n_data;	/* data records */
	struct namebuf	*n_parent;	/* parent domain */
	struct hashbuf	*n_hash;	/* hash table for children */
	char		_n_name[sizeof(void*)];	/* Counted str (dynamic). */
};
#define	NAMESIZE(n)	(sizeof(struct namebuf) - sizeof(void*) + 1 + n + 1)
#define	NAMELEN(nb)	(((u_char *)((nb)._n_name))[0])
#define	NAME(nb)	((nb)._n_name + 1)

struct hashbuf {
	int		h_size;		/* size of hash table */
	int		h_cnt;		/* number of entries */
	struct namebuf	*h_tab[1];	/* allocated as needed */
};
#define HASHSIZE(s) (sizeof(struct hashbuf) + (s-1) * sizeof(struct namebuf *))

#define HASHSHIFT	3
#define HASHMASK	0x1f
#define HASHROTATE(v) \
	(((v) << HASHSHIFT) | ((v) >> ((sizeof(v) * 8) - HASHSHIFT)))
#define HASHLOWER(c)	((isascii(c) && isupper(c)) ? tolower(c) : (c))
#define HASHIMILATE(v,c) ((v) = (HASHROTATE(v)) + (HASHLOWER(c) & HASHMASK))

#define TSIG_BUF_SIZE 640
#define TSIG_SIG_SIZE 20

struct tsig_record {
	u_int8_t	sig[TSIG_SIG_SIZE];
	struct dst_key	*key;
	int		siglen;
};

struct sig_record {
	u_int16_t sig_type_n;
	u_int8_t sig_alg_n, sig_labels_n;
	u_int32_t sig_ottl_n, sig_exp_n, sig_time_n;
	u_int16_t sig_keyid_n;
};

/* This is the wire format size of "struct sig_record", i.e., no padding. */
#define SIG_HDR_SIZE 18

struct dnode {
	struct databuf	*dp;
	struct dnode	*dn_next;
	int		line;
	char		*file;
};

typedef struct dnode * dlist;

struct db_rrset {
	dlist		rr_list;
	dlist		rr_sigs;
	char		*rr_name;
	int16_t		rr_class;
	int16_t		rr_type;
	struct db_rrset *rr_next;
};
#define DBHASHSIZE(s) (sizeof(struct hashbuf) + \
		       (s-1) * sizeof(struct db_rrset *))

#define SIG_COVERS(dp) (ns_get16(dp->d_data))

/*
 * Flags to updatedb
 */
#define DB_NODATA	0x01	/* data should not exist */
#define DB_MEXIST	0x02	/* data must exist */
#define DB_DELETE	0x04	/* delete data if it exists */
#define DB_NOTAUTH	0x08	/* must not update authoritative data */
#define DB_NOHINTS      0x10	/* don't reflect update in fcachetab */
#define DB_PRIMING	0x20	/* is this update the result of priming? */
#define DB_MERGE	0x40    /* make no control on rr in db_update (for ixfr) */
#define DB_REPLACE	0x80	/* replace data if it exists */

#define DB_Z_CACHE	0	/* cache-zone-only db_dump() */
#define DB_Z_ALL	65535	/* normal db_dump() */
#define	DB_Z_SPECIAL(z)	((z) == DB_Z_CACHE || (z) == DB_Z_ALL)

/*
 * Error return codes
 */
#define OK		0
#define NONAME		(-1)
#define NOCLASS		(-2)
#define NOTYPE		(-3)
#define NODATA		(-4)
#define DATAEXISTS	(-5)
#define NODBFILE	(-6)
#define TOOMANYZONES	(-7)
#define GOODDB		(-8)
#define NEWDB		(-9)
#define AUTH		(-10)
#ifdef BIND_UPDATE
#define SERIAL		(-11)
#endif
#define CNAMEANDOTHER	(-12)
#define DNSSECFAIL	(-13)	/* db_set_update */

/*
 * getnum() options
 */
#define GETNUM_NONE	0x00	/* placeholder */
#define GETNUM_SERIAL	0x01	/* treat as serial number */
#define GETNUM_SCALED	0x02	/* permit "k", "m" suffixes, scale result */

/*
 * db_load() options
 */
#define ISNOTIXFR	0
#define ISIXFR		1
#define ISAXFRIXFR	2

/*
 * Database access abstractions.
 */
#define	foreach_rr(dp, np, ty, cl, zn) \
	for ((dp) = (np)->n_data; (dp) != NULL; (dp) = (dp)->d_next) \
		if (!match(dp, (cl), (ty))) \
			continue; \
		else if (((zn) == DB_Z_CACHE) \
			 ? stale(dp) \
			 : (zn) != (dp)->d_zone) \
			continue; \
		else if ((dp)->d_rcode) \
			continue; \
		else \
			/* Caller code follows in sequence. */

#define DRCNTINC(x) \
	do { \
		if (++((x)->d_rcnt) == 0) \
			ns_panic(ns_log_db, 1, "++d_rcnt == 0"); \
	} while (0)

#define DRCNTDEC(x) \
	do { \
		if (((x)->d_rcnt)-- == 0) \
			ns_panic(ns_log_db, 1, "d_rcnt-- == 0"); \
	} while (0)

#define ISVALIDGLUE(xdp) ((xdp)->d_type == T_NS || (xdp)->d_type == T_A \
                         || (xdp)->d_type == T_AAAA || (xdp)->d_type == ns_t_a6)


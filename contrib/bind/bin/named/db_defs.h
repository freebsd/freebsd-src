/*
 *	from db.h	4.16 (Berkeley) 6/1/90
 *	$Id: db_defs.h,v 8.17 1998/02/17 17:17:43 vixie Exp $
 */

/* Copyright (c) 1985, 1990
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

/* Portions Copyright (c) 1993 by Digital Equipment Corporation.
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

/* Portions Copyright (c) 1996, 1997 by Internet Software Consortium.
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

#define INVBLKSZ	7	/* # of namebuf pointers per block */
#define INVHASHSZ	919	/* size of inverse hash table */

	/* max length of data in RR data field */
#define MAXDATA		(2*MAXDNAME + 5*INT32SZ)

#define DB_ROOT_TIMBUF	3600
#define TIMBUF		300

#define	DICT_INDEXBITS	24
#define	DICT_MAXLENGTH	127
#define	DICT_INSERT_P	0x0001

/*
 * Hash table structures.
 */
struct databuf {
	struct databuf	*d_next;	/* linked list */
	struct nameser	*d_ns;		/* NS from whence this came */
	u_int32_t	d_ttl;		/* time to live */
					/* if d_zone == DB_Z_CACHE, then
					 * d_ttl is actually the time when
					 * the record will expire.
					 * otherwise (for authoritative
					 * primary and secondary zones),
					 * d_ttl is the time to live.
					 */
	unsigned	d_flags :7;	/* see below */
	unsigned	d_cred :3;	/* DB_C_{??????} */
	unsigned	d_clev :6;
	u_int16_t	d_zone;		/* zone number or 0 for the cache */
	int16_t		d_class;	/* class number */
	int16_t		d_type;		/* type number */
	int16_t		d_size;		/* size of data area */
	u_int32_t	d_rcnt;
	unsigned	d_rcode :4;	/* rcode for negative caching */
	unsigned	d_mark :12;	/* place to mark data */
	u_int16_t       d_nstime;       /* NS response time, milliseconds */
	u_char		d_data[sizeof(void*)]; /* dynamic (padded) */
};
#define DATASIZE(n) (sizeof(struct databuf) - sizeof(void*) + n)

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

/*
 * d_cred definitions
 */
#define	DB_C_ZONE	4		/* authoritative zone - best */
#define	DB_C_AUTH	3		/* authoritative answer */
#define	DB_C_ANSWER	2		/* non-authoritative answer */
#define	DB_C_ADDITIONAL	1		/* additional data */
#define	DB_C_CACHE	0		/* cache - worst */

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

/*
 * Flags to updatedb
 */
#define DB_NODATA	0x01	/* data should not exist */
#define DB_MEXIST	0x02	/* data must exist */
#define DB_DELETE	0x04	/* delete data if it exists */
#define DB_NOTAUTH	0x08	/* must not update authoritative data */
#define DB_NOHINTS      0x10	/* don't reflect update in fcachetab */
#define DB_PRIMING	0x20	/* is this update the result of priming? */

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

/*
 * getnum() options
 */
#define GETNUM_NONE	0x00	/* placeholder */
#define GETNUM_SERIAL	0x01	/* treat as serial number */
#define GETNUM_SCALED	0x02	/* permit "k", "m" suffixes, scale result */

/*
 * Database access abstractions.
 */
#define	foreach_rr(dp, np, ty, cl, zn) \
	for ((dp) = (np)->n_data; (dp) != NULL; (dp) = (dp)->d_next) \
		if ((cl) != C_ANY && (cl) != (dp)->d_class) \
			continue; \
		else if ((ty) != T_ANY && (ty) != (dp)->d_type) \
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

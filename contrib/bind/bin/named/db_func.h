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
 * Portions Copyright (c) 1999 by Check Point Software Technologies, Inc.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Check Point Software Technologies Incorporated not be used 
 * in advertising or publicity pertaining to distribution of the document 
 * or software without specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND CHECK POINT SOFTWARE TECHNOLOGIES 
 * INCORPORATED DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.   
 * IN NO EVENT SHALL CHECK POINT SOFTWARE TECHNOLOGIES INCORPRATED
 * BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR 
 * ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT 
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* db_proc.h - prototypes for functions in db_*.c
 *
 * $Id: db_func.h,v 8.42 2000/04/21 06:54:02 vixie Exp $
 */

/* ++from db_update.c++ */
extern int		db_update(const char *name,
				  struct databuf *odp,
				  struct databuf *newdp,
				  struct databuf **savedpp,
				  int flags,
				  struct hashbuf *htp,
				  struct sockaddr_in from),
			db_cmp(const struct databuf *, const struct databuf *),
			findMyZone(struct namebuf *np, int class);
void			fixttl(struct databuf *dp);
/* --from db_update.c-- */

/* ++from db_save.c++ */
extern struct namebuf	*savename(const char *, int);
extern struct databuf	*savedata(int, int, u_int32_t, u_char *, int);
extern struct hashbuf	*savehash(struct hashbuf *);
/* --from db_save.c-- */

/* ++from db_dump.c++ */
extern int		db_dump(struct hashbuf *, FILE *, int, char *),
			zt_dump(FILE *);
extern void		doadump(void);
/* --from db_dump.c-- */

/* ++from db_load.c++ */
extern int		makename_ok(char *name, const char *origin, int class,
				    struct zoneinfo *zp,
				    enum transport transport,
				    enum context context,
				    const char *owner, const char *filename,
				    int lineno, int size);
extern void		endline(FILE *);
extern int		getword(char *, size_t, FILE *, int),
			getttl(FILE *, const char *, int, u_int32_t *, int *),
			getnum(FILE *, const char *, int),
			db_load(const char *, const char *, struct zoneinfo *,
				const char *, int);
extern int 		getnonblank(FILE *, const char *),
			getservices(int, char *, FILE *, const char *);
extern char		getprotocol(FILE *, const char *);
extern int		makename(char *, const char *, int);
extern void		db_err(int, char *, int, const char *, int);
extern int		parse_sec_rdata(char *inp, int inp_len, int inp_full,
					u_char *data, int data_len,
					FILE *fp, struct zoneinfo *zp, 
					char *domain,  u_int32_t ttl, 
					int type, enum context context,
					enum transport transport,
					char **errmsg);
/* --from db_load.c-- */

/* ++from db_glue.c++ */
extern void		buildservicelist(void),
			destroyservicelist(void),
			buildprotolist(void),
			destroyprotolist(void),
			getname(struct namebuf *, char *, int);
extern int		servicenumber(const char *),
			protocolnumber(const char *),
			get_class(const char *);
extern u_int		nhash(const char *);
extern const char	*protocolname(int),
			*servicename(u_int16_t, const char *);
#ifndef BSD
extern int		getdtablesize(void);
#endif
extern struct databuf	*rm_datum(struct databuf *,
				  struct namebuf *,
				  struct databuf *,
				  struct databuf **);
extern struct namebuf	*rm_name(struct namebuf *, 
				 struct namebuf **,
				 struct namebuf *);
extern void		rm_hash(struct hashbuf *);
extern void		db_freedata(struct databuf *);
extern void		db_lame_add(char *zone, char *server, time_t when);
extern time_t		db_lame_find(char *zone, struct databuf *dp);
extern void 		db_lame_clean(void);
extern void 		db_lame_destroy(void);
/* --from db_glue.c-- */

/* ++from db_lookup.c++ */
extern struct namebuf	*nlookup(const char *, struct hashbuf **,
				 const char **, int);
extern struct namebuf	*np_parent __P((struct namebuf *));
extern int		match(struct databuf *, int, int),
			nxtmatch(const char *, struct databuf *,
				 struct databuf *),
			rrmatch(const char *, struct databuf *,
				struct databuf *);
/* --from db_lookup.c-- */

/* ++from db_ixfr.c++ */
extern ns_deltalist *	ixfr_get_change_list(struct zoneinfo *, u_int32_t,
					     u_int32_t);
int			ixfr_have_log(struct zoneinfo *, u_int32_t,
				      u_int32_t);
/* --from db_ixfr.c++ */

/* ++from db_sec.c++ */
int			add_trusted_key(const char *name, const int flags,
					const int proto, const int alg,
					const char *str);
int			db_set_update(char *name, struct databuf *dp,
				      void **state, int flags,
				      struct hashbuf **htp,
				      struct sockaddr_in from,
				      int *rrcount, int line,
				      const char *file);
/* --from db_sec.c-- */
/* ++from db_tsig.c++ */
char *			tsig_alg_name(int value);
int			tsig_alg_value(char *name);
struct dst_key *	tsig_key_from_addr(struct in_addr addr);
struct tsig_record *	new_tsig(struct dst_key *key, u_char *sig, int siglen);
void			free_tsig(struct tsig_record *tsig);
/* --from db_tsig.c-- */

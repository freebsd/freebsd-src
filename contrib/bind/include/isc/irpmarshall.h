/*
 * Copyright (c) 1999 by Internet Software Consortium.
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
 * $Id: irpmarshall.h,v 8.2 2001/05/29 05:47:10 marka Exp $
 */

#ifndef _IRPMARSHALL_H_INCLUDED
#define _IRPMARSHALL_H_INCLUDED

/* Hide function names */
#define irp_marshall_gr __irp_marshall_gr
#define irp_marshall_ho __irp_marshall_ho
#define irp_marshall_ne __irp_marshall_ne
#define irp_marshall_ng __irp_marshall_ng
#define irp_marshall_nw __irp_marshall_nw
#define irp_marshall_pr __irp_marshall_pr
#define irp_marshall_pw __irp_marshall_pw
#define irp_marshall_sv __irp_marshall_sv
#define irp_unmarshall_gr __irp_unmarshall_gr
#define irp_unmarshall_ho __irp_unmarshall_ho
#define irp_unmarshall_ne __irp_unmarshall_ne
#define irp_unmarshall_ng __irp_unmarshall_ng
#define irp_unmarshall_nw __irp_unmarshall_nw
#define irp_unmarshall_pr __irp_unmarshall_pr
#define irp_unmarshall_pw __irp_unmarshall_pw
#define irp_unmarshall_sv __irp_unmarshall_sv

#define MAXPADDRSIZE (sizeof "255.255.255.255" + 1)
#define ADDR_T_STR(x) (x == AF_INET ? "AF_INET" :\
		       (x == AF_INET6 ? "AF_INET6" : "UNKNOWN"))

/* See comment below on usage */
int irp_marshall_pw(const struct passwd *pw, char **buffer, size_t *len);
int irp_unmarshall_pw(struct passwd *pw, char *buffer);
int irp_marshall_gr(const struct group *gr, char **buffer, size_t *len);
int irp_unmarshall_gr(struct group *gr, char *buffer);
int irp_marshall_sv(const struct servent *sv, char **buffer, size_t *len);
int irp_unmarshall_sv(struct servent *sv, char *buffer);
int irp_marshall_pr(struct protoent *pr, char **buffer, size_t *len);
int irp_unmarshall_pr(struct protoent *pr, char *buffer);
int irp_marshall_ho(struct hostent *ho, char **buffer, size_t *len);
int irp_unmarshall_ho(struct hostent *ho, char *buffer);
int irp_marshall_ng(const char *host, const char *user, const char *domain,
		    char **buffer, size_t *len);
int irp_unmarshall_ng(const char **host, const char **user,
		      const char **domain, char *buffer);
int irp_marshall_nw(struct nwent *ne, char **buffer, size_t *len);
int irp_unmarshall_nw(struct nwent *ne, char *buffer);
int irp_marshall_ne(struct netent *ne, char **buffer, size_t *len);
int irp_unmarshall_ne(struct netent *ne, char *buffer);

/*
 * Functions to marshall and unmarshall various system data structures. We
 * use a printable ascii format that is as close to various system config
 * files as reasonable (e.g. /etc/passwd format).
 *
 * We are not forgiving with unmarhsalling misformatted buffers. In
 * particular whitespace in fields is not ignored. So a formatted password
 * entry "brister  :1364:100:...." will yield a username of "brister   "
 *
 * We potentially do a lot of mallocs to fill fields that are of type
 * (char **) like a hostent h_addr field. Building (for example) the
 * h_addr field and its associated addresses all in one buffer is
 * certainly possible, but not done here.
 *
 * The following description is true for all the marshalling functions:
 *
 */

/* int irp_marshall_XX(struct yyyy *XX, char **buffer, size_t *len);
 *
 * The argument XX (of type struct passwd for example) is marshalled in the
 * buffer pointed at by *BUFFER, which is of length *LEN. Returns 0
 * on success and -1 on failure. Failure will occur if *LEN is
 * smaller than needed.
 *
 * If BUFFER is NULL, then *LEN is set to the size of the buffer
 * needed to marshall the data and no marshalling is actually done.
 *
 * If *BUFFER is NULL, then a buffer large enough will be allocated
 * with memget() and the size allocated will be stored in *LEN. An extra 2
 * bytes will be allocated for the client to append CRLF if wanted. The
 * value of *LEN will include these two bytes.
 *
 * All the marshalling functions produce a buffer with the fields
 * separated by colons (except for the hostent marshalling, which uses '@'
 * to separate fields). Fields that have multiple subfields (like the
 * gr_mem field in struct group) have their subparts separated by
 * commas.
 */

/*
 * int irp_unmarshall_XX(struct YYYYY *XX, char *buffer);
 *
 * The unmashalling functions break apart the buffer and store the
 * values in the struct pointed to by XX. All pointer values inside
 * XX are allocated with malloc. All arrays of pointers have a NULL
 * as the last element.
 */

#endif

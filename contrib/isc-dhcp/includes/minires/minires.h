/*
 * Copyright (c) 2001 by Internet Software Consortium.
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

#include "cdefs.h"
#include "osdep.h"

#define _ns_flagdata MR_ns_flagdata

#include "minires/resolv.h"
#include "minires/res_update.h"
#include "isc-dhcp/result.h"

/*
 * Based on the Dynamic DNS reference implementation by Viraj Bais
 * <viraj_bais@ccm.fm.intel.com>
 */

int minires_mkupdate (ns_updrec *, unsigned char *, unsigned);
int minires_update (ns_updrec *);
ns_updrec *minires_mkupdrec (int, const char *, unsigned int,
			     unsigned int, unsigned long);
void minires_freeupdrec (ns_updrec *);
int minires_nmkupdate (res_state, ns_updrec *, double *, unsigned *);
isc_result_t minires_nupdate (res_state, ns_updrec *);
int minires_ninit (res_state);
ns_rcode isc_rcode_to_ns (isc_result_t);

#if defined (MINIRES_LIB)
#define res_update minires_update
#define res_mkupdate minires_mkupdate
#define res_mkupdrec minires_mkupdrec
#define res_freeupdrec minires_freeupdrec
#define res_nmkupdate minires_nmkupdate
#define res_nupdate minires_nupdate
#define __p_type_syms MR__p_type_syms
#define dn_comp MRdn_comp
#define loc_aton MRloc_aton
#define sym_ston MRsym_ston
#define res_buildservicelist MRres_buildservicelist
#define res_destroyservicelist MRres_destroyservicelist
#define res_buildprotolist MRres_buildprotolist
#define res_destroyprotolist MRres_destroyprotolist
#define res_servicenumber MRres_servicenumber
#define res_protocolnumber MRres_protocolnumber
#define res_protocolname MRres_protocolname
#define res_servicename MRres_servicename
#define ns_datetosecs MRns_datetosecs
#define b64_pton MRb64_pton
#define res_ninit minires_ninit
#define res_randomid MRres_randomid
#define res_findzonecut MRres_findzonecut
#define res_nsend MRres_nsend
#define res_nsendsigned MRres_nsendsigned
#define ns_samename MRns_samename
#define res_nameinquery MRres_nameinquery
#define res_queriesmatch MRres_queriesmatch
#define dn_expand MRdn_expand
#define ns_get16 MRns_get16
#define res_close MRres_close
#define res_nclose MRres_nclose
#define res_ourserver_p MRres_ourserver_p
#define ns_sign MRns_sign
#define p_class MRp_class
#define p_section MRp_section
#define ns_makecanon MRns_makecanon
#define ns_parserr MRns_parserr
#define ns_samedomain MRns_samedomain
#define ns_name_uncompress MRns_name_uncompress
#define res_nmkquery MRres_nmkquery
#define ns_initparse MRns_initparse
#define res_nquery MRres_nquery
#define res_nsearch MRres_nsearch
#define res_hostalias MRres_hostalias
#define res_nquerydomain MRres_nquerydomain
#define ns_skiprr MRns_skiprr
#define dn_skipname MRdn_skipname
#define ns_name_ntol MRns_name_ntol
#define ns_sign_tcp_init MRns_sign_tcp_init
#define ns_sign_tcp MRns_sign_tcp
#define ns_name_ntop MRns_name_ntop
#define ns_name_pton MRns_name_pton
#define ns_name_unpack MRns_name_unpack
#define ns_name_pack MRns_name_pack
#define ns_name_compress MRns_name_compress
#define ns_name_skip MRns_name_skip
#define ns_subdomain MRns_subdomain
#define ns_find_tsig MRns_find_tsig
#define ns_verify MRns_verify
#define ns_verify_tcp_init MRns_verify_tcp_init
#define ns_verify_tcp MRns_verify_tcp
#define b64_ntop MRb64_ntop

extern const struct res_sym __p_type_syms[];
extern time_t cur_time;

int dn_comp (const char *,
	     unsigned char *, unsigned, unsigned char **, unsigned char **);
int loc_aton (const char *, u_char *);
int sym_ston (const struct res_sym *, const char *, int *);
void  res_buildservicelist (void);
void res_destroyservicelist (void);
void res_buildprotolist(void);
void res_destroyprotolist(void);
int res_servicenumber(const char *);
int res_protocolnumber(const char *);
const char *res_protocolname(int);
const char *res_servicename(u_int16_t, const char *);
u_int32_t ns_datetosecs (const char *cp, int *errp);
int b64_pton (char const *, unsigned char *, size_t);
unsigned int res_randomid (void);
isc_result_t res_findzonecut (res_state, const char *, ns_class, int, char *,
			      size_t, struct in_addr *, int, int *, void *);
isc_result_t res_nsend (res_state,
			double *, unsigned, double *, unsigned, unsigned *);
isc_result_t res_nsendsigned (res_state, double *, unsigned, ns_tsig_key *,
			      double *, unsigned, unsigned *);
int ns_samename (const char *, const char *);
int res_nameinquery (const char *, int, int,
		     const unsigned char *, const unsigned char *);
int res_queriesmatch (const unsigned char *, const unsigned char *,
		      const unsigned char *, const unsigned char *);
int dn_expand (const unsigned char *,
	       const unsigned char *, const unsigned char *, char *, unsigned);
unsigned int ns_get16 (const unsigned char *);
void res_close (void);
void res_nclose (res_state);
int res_ourserver_p (const res_state, const struct sockaddr_in *);
isc_result_t ns_sign (unsigned char *, unsigned *,
		      unsigned, int, void *, const unsigned char *,
		      unsigned, unsigned char *, unsigned *, time_t);
const char *p_class (int);
const char *p_section (int section, int opcode);
isc_result_t ns_makecanon (const char *, char *, size_t);
isc_result_t ns_parserr (ns_msg *, ns_sect, int, ns_rr *);
int ns_samedomain (const char *, const char *);
int ns_name_uncompress (const u_char *, const u_char *,
			    const u_char *, char *, size_t);
isc_result_t res_nmkquery (res_state, int, const char *, ns_class, ns_type,
			   const unsigned char *, unsigned,
			   const unsigned char *, double *,
			   unsigned, unsigned *);
isc_result_t ns_initparse (const unsigned char *, unsigned, ns_msg *);
isc_result_t res_nquery(res_state, const char *,
			ns_class, ns_type, double *, unsigned, unsigned *);
isc_result_t res_nsearch(res_state, const char *,
			 ns_class, ns_type, double *, unsigned, unsigned *);
const char *res_hostalias (const res_state, const char *, char *, size_t);
isc_result_t res_nquerydomain(res_state, const char *, const char *,
			      ns_class class, ns_type type,
			      double *, unsigned, unsigned *);

isc_result_t ns_skiprr(const unsigned char *,
		       const unsigned char *, ns_sect, int, int *);
int dn_skipname (const unsigned char *, const unsigned char *);
u_int32_t getULong (const unsigned char *);
int32_t getLong (const unsigned char *);
u_int32_t getUShort (const unsigned char *);
int32_t getShort (const unsigned char *);
u_int32_t getUChar (const unsigned char *);
void putULong (unsigned char *, u_int32_t);
void putLong (unsigned char *, int32_t);
void putUShort (unsigned char *, u_int32_t);
void putShort (unsigned char *, int32_t);
void putUChar (unsigned char *, u_int32_t);
int ns_name_ntol (const unsigned char *, unsigned char *, size_t);
isc_result_t ns_sign_tcp_init (void *, const unsigned char *,
			       unsigned, ns_tcp_tsig_state *);
isc_result_t ns_sign_tcp (unsigned char *,
			  unsigned *, unsigned, int, ns_tcp_tsig_state *, int);
int ns_name_ntop (const unsigned char *, char *, size_t);
int ns_name_pton (const char *, unsigned char *, size_t);
int ns_name_unpack (const unsigned char *, const unsigned char *,
		    const unsigned char *, unsigned char *, size_t);
int ns_name_pack (const unsigned char *, unsigned char *,
		  unsigned, const unsigned char **, const unsigned char **);
int ns_name_compress (const char *, unsigned char *,
		      size_t, const unsigned char **, const unsigned char **);
int ns_name_skip (const unsigned char **, const unsigned char *);
int ns_subdomain (const char *, const char *);
unsigned char *ns_find_tsig (unsigned char *, unsigned char *);
isc_result_t ns_verify (unsigned char *, unsigned *, void *,
			const unsigned char *,
			unsigned, unsigned char *, unsigned *, time_t *, int);
isc_result_t ns_verify_tcp_init (void *, const unsigned char *, unsigned,
				 ns_tcp_tsig_state *);
isc_result_t ns_verify_tcp (unsigned char *, unsigned *,
			    ns_tcp_tsig_state *, int);
int b64_ntop (unsigned char const *, size_t, char *, size_t);

ns_rcode find_cached_zone (const char *, ns_class, char *,
			   size_t, struct in_addr *, int, int *, void *);
int find_tsig_key (ns_tsig_key **, const char *, void *);
int forget_zone (void *);
int repudiate_zone (void *);
void cache_found_zone (ns_class, char *, struct in_addr *, int);
isc_result_t uerr2isc (int);
isc_result_t ns_rcode_to_isc (int);

#define DprintQ(a,b,c,d)
#define Dprint(a,b)
#define Perror(a, b, c, d)
#define Aerror(a, b, c, d, e)
#define DPRINTF(x)

#define USE_MD5
#endif

#if defined (TRACING)
void trace_mr_statp_setup (res_state);
#endif

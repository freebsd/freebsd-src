/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2002  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: master.h,v 1.31.2.3.2.7 2004/03/08 09:04:36 marka Exp $ */

#ifndef DNS_MASTER_H
#define DNS_MASTER_H 1

/***
 ***	Imports
 ***/

#include <stdio.h>

#include <isc/lang.h>

#include <dns/types.h>

/*
 * Flags to be passed in the 'options' argument in the functions below.
 */
#define	DNS_MASTER_AGETTL 	0x00000001	/* Age the ttl based on $DATE. */
#define DNS_MASTER_MANYERRORS 	0x00000002	/* Continue processing on errors. */
#define DNS_MASTER_NOINCLUDE 	0x00000004	/* Disallow $INCLUDE directives. */
#define DNS_MASTER_ZONE 	0x00000008	/* Loading a zone master file. */
#define DNS_MASTER_HINT 	0x00000010	/* Loading a hint master file. */
#define DNS_MASTER_SLAVE 	0x00000020	/* Loading a slave master file. */
#define DNS_MASTER_CHECKNS 	0x00000040	/* Check NS records to see if
						 * they are an address */
#define DNS_MASTER_FATALNS 	0x00000080	/* Treat DNS_MASTER_CHECKNS
						 * matches as fatal */
#define DNS_MASTER_CHECKNAMES   0x00000100
#define DNS_MASTER_CHECKNAMESFAIL 0x00000200

ISC_LANG_BEGINDECLS

/***
 ***	Function
 ***/

isc_result_t
dns_master_loadfile(const char *master_file,
		    dns_name_t *top,
		    dns_name_t *origin,
		    dns_rdataclass_t zclass,
		    unsigned int options,
		    dns_rdatacallbacks_t *callbacks,
		    isc_mem_t *mctx);

isc_result_t
dns_master_loadstream(FILE *stream,
		      dns_name_t *top,
		      dns_name_t *origin,
		      dns_rdataclass_t zclass,
		      unsigned int options,
		      dns_rdatacallbacks_t *callbacks,
		      isc_mem_t *mctx);

isc_result_t
dns_master_loadbuffer(isc_buffer_t *buffer,
		      dns_name_t *top,
		      dns_name_t *origin,
		      dns_rdataclass_t zclass,
		      unsigned int options,
		      dns_rdatacallbacks_t *callbacks,
		      isc_mem_t *mctx);

isc_result_t
dns_master_loadlexer(isc_lex_t *lex,
		     dns_name_t *top,
		     dns_name_t *origin,
		     dns_rdataclass_t zclass,
		     unsigned int options,
		     dns_rdatacallbacks_t *callbacks,
		     isc_mem_t *mctx);

isc_result_t
dns_master_loadfileinc(const char *master_file,
		       dns_name_t *top,
		       dns_name_t *origin,
		       dns_rdataclass_t zclass,
		       unsigned int options,
		       dns_rdatacallbacks_t *callbacks,
		       isc_task_t *task,
		       dns_loaddonefunc_t done, void *done_arg,
		       dns_loadctx_t **ctxp, isc_mem_t *mctx);

isc_result_t
dns_master_loadstreaminc(FILE *stream,
			 dns_name_t *top,
			 dns_name_t *origin,
			 dns_rdataclass_t zclass,
			 unsigned int options,
			 dns_rdatacallbacks_t *callbacks,
			 isc_task_t *task,
			 dns_loaddonefunc_t done, void *done_arg,
			 dns_loadctx_t **ctxp, isc_mem_t *mctx);

isc_result_t
dns_master_loadbufferinc(isc_buffer_t *buffer,
			 dns_name_t *top,
			 dns_name_t *origin,
			 dns_rdataclass_t zclass,
			 unsigned int options,
			 dns_rdatacallbacks_t *callbacks,
			 isc_task_t *task,
			 dns_loaddonefunc_t done, void *done_arg,
			 dns_loadctx_t **ctxp, isc_mem_t *mctx);

isc_result_t
dns_master_loadlexerinc(isc_lex_t *lex,
			dns_name_t *top,
			dns_name_t *origin,
			dns_rdataclass_t zclass,
			unsigned int options,
			dns_rdatacallbacks_t *callbacks,
			isc_task_t *task,
			dns_loaddonefunc_t done, void *done_arg,
			dns_loadctx_t **ctxp, isc_mem_t *mctx);

/*
 * Loads a RFC 1305 master file from a file, stream, buffer, or existing
 * lexer into rdatasets and then calls 'callbacks->commit' to commit the
 * rdatasets.  Rdata memory belongs to dns_master_load and will be
 * reused / released when the callback completes.  dns_load_master will
 * abort if callbacks->commit returns any value other than ISC_R_SUCCESS.
 *
 * If 'DNS_MASTER_AGETTL' is set and the master file contains one or more
 * $DATE directives, the TTLs of the data will be aged accordingly.
 *
 * 'callbacks->commit' is assumed to call 'callbacks->error' or
 * 'callbacks->warn' to generate any error messages required.
 *
 * 'done' is called with 'done_arg' and a result code when the loading
 * is completed or has failed.  If the initial setup fails 'done' is
 * not called.
 *
 * Requires:
 *	'master_file' points to a valid string.
 *	'lexer' points to a valid lexer.
 *	'top' points to a valid name.
 *	'origin' points to a valid name.
 *	'callbacks->commit' points to a valid function.
 *	'callbacks->error' points to a valid function.
 *	'callbacks->warn' points to a valid function.
 *	'mctx' points to a valid memory context.
 *	'task' and 'done' to be valid.
 *	'lmgr' to be valid.
 *	'ctxp != NULL && ctxp == NULL'.
 *
 * Returns:
 *	ISC_R_SUCCESS upon successfully loading the master file.
 *	ISC_R_SEENINCLUDE upon successfully loading the master file with
 *		a $INCLUDE statement.
 *	ISC_R_NOMEMORY out of memory.
 *	ISC_R_UNEXPECTEDEND expected to be able to read a input token and
 *		there was not one.
 *	ISC_R_UNEXPECTED
 *	DNS_R_NOOWNER failed to specify a ownername.
 *	DNS_R_NOTTL failed to specify a ttl.
 *	DNS_R_BADCLASS record class did not match zone class.
 *	DNS_R_CONTINUE load still in progress (dns_master_load*inc() only).
 *	Any dns_rdata_fromtext() error code.
 *	Any error code from callbacks->commit().
 */

void
dns_loadctx_detach(dns_loadctx_t **ctxp);
/*
 * Detach from the load context.
 *
 * Requires:
 *	'*ctxp' to be valid.
 *
 * Ensures:
 *	'*ctxp == NULL'
 */

void
dns_loadctx_attach(dns_loadctx_t *source, dns_loadctx_t **target);
/*
 * Attach to the load context.
 *
 * Requires:
 *	'source' to be valid.
 *	'target != NULL && *target == NULL'.
 */

void
dns_loadctx_cancel(dns_loadctx_t *ctx);
/*
 * Cancel loading the zone file associated with this load context.
 *
 * Requires:
 *	'ctx' to be valid
 */

ISC_LANG_ENDDECLS

#endif /* DNS_MASTER_H */

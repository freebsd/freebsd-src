/*
 * Copyright (C) 2005, 2007  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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

/* $Id: ns_smf_globals.h,v 1.7 2007-06-19 23:46:59 tbox Exp $ */

#ifndef NS_SMF_GLOBALS_H
#define NS_SMF_GLOBALS_H 1
 
#include <libscf.h>
 
#undef EXTERN
#undef INIT
#ifdef NS_MAIN
#define EXTERN
#define INIT(v) = (v)
#else
#define EXTERN extern
#define INIT(v)
#endif
                
EXTERN unsigned int	ns_smf_got_instance	INIT(0);
EXTERN unsigned int	ns_smf_chroot		INIT(0);
EXTERN unsigned int	ns_smf_want_disable	INIT(0);

isc_result_t ns_smf_add_message(isc_buffer_t *text);
isc_result_t ns_smf_get_instance(char **name, int debug, isc_mem_t *mctx);

#undef EXTERN
#undef INIT
 
#endif /* NS_SMF_GLOBALS_H */

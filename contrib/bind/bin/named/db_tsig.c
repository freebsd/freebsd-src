
#if !defined(lint) && !defined(SABER)
static const char rcsid[] = "$Id: db_tsig.c,v 8.7 2001/06/18 14:42:59 marka Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1986, 1990
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

#include "port_before.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <ctype.h>
#include <resolv.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

#include <isc/eventlib.h>
#include <isc/logging.h>
#include <isc/memcluster.h>
#include <isc/tree.h>

#include <isc/dst.h>

#include "port_after.h"

#include "named.h"

typedef struct {
	DST_KEY *key;
	void *ctx;
} tsig_axfr_state;

#define TSIG_ALG_MD5 "HMAC-MD5.SIG-ALG.REG.INT"
#define TSIG_ALG_MD5_SHORT "hmac-md5"

const char *
tsig_alg_name(int value) {
	if (value == KEY_HMAC_MD5)
		return(TSIG_ALG_MD5);
	else
		return(NULL);
}

int
tsig_alg_value(char *name) {
	if (ns_samename(name, TSIG_ALG_MD5) == 1 ||
	    strcasecmp(name, TSIG_ALG_MD5_SHORT) == 0)
		return (KEY_HMAC_MD5);
	else
		return (-1);
}

DST_KEY *
tsig_key_from_addr(struct in_addr addr) {
	server_info si = si = find_server(addr);
	if (si == NULL || si->key_list == NULL || si->key_list->first == NULL)
		return(NULL);
	return(si->key_list->first->key);
}

struct tsig_record *
new_tsig(DST_KEY *key, u_char *sig, int siglen) {
	struct tsig_record *tsig;

	if (siglen > TSIG_SIG_SIZE)
		return(NULL);
	tsig = memget(sizeof(struct tsig_record));
	if (tsig == NULL)
		return(NULL);
	tsig->key = key;
	tsig->siglen = siglen;
	memcpy(tsig->sig, sig, siglen);
	return(tsig);
}

void
free_tsig(struct tsig_record *tsig) {
	if (tsig == NULL)
		return;
	memput(tsig, sizeof(struct tsig_record));
}

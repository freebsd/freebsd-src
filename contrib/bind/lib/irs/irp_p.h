/*
 * Copyright (c) 1996 by Internet Software Consortium.
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
 * $Id: irp_p.h,v 8.1 1999/01/18 07:46:54 vixie Exp $
 */

#ifndef _IRP_P_H_INCLUDED
#define _IRP_P_H_INCLUDED

#include <stdio.h>

struct irp_p {
	char inbuffer[1024];
	int inlast; /* index of one past the last char in buffer */
	int incurr; /* index of the next char to be read from buffer */

	int fdCxn;
};

/*
 * Externs.
 */

extern struct irs_acc *	irs_irp_acc __P((const char *));
extern struct irs_gr *	irs_irp_gr __P((struct irs_acc *));
extern struct irs_pw *	irs_irp_pw __P((struct irs_acc *));
extern struct irs_sv *	irs_irp_sv __P((struct irs_acc *));
extern struct irs_pr *	irs_irp_pr __P((struct irs_acc *));
extern struct irs_ho *	irs_irp_ho __P((struct irs_acc *));
extern struct irs_nw *	irs_irp_nw __P((struct irs_acc *));
extern struct irs_ng *	irs_irp_ng __P((struct irs_acc *));

int irs_irp_connect(struct irp_p *pvt);
int irs_irp_is_connected(struct irp_p *pvt);
void irs_irp_disconnect(struct irp_p *pvt);
int irs_irp_read_response(struct irp_p *pvt, char *text, size_t textlen);
char *irs_irp_read_body(struct irp_p *pvt, size_t *size);
int irs_irp_get_full_response(struct irp_p *pvt, int *code,
			      char *text, size_t textlen,
			      char **body, size_t *bodylen);
int irs_irp_send_command(struct irp_p *pvt, const char *fmt, ...);


extern int irp_log_errors;

#endif

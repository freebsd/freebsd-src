/*-
 * Copyright (c) 2009 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Ed Schouten under sponsorship from the
 * FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <dev/vt/vt.h>

void
vthistory_add(struct vt_history *vh, struct vt_buf *vb, const term_rect_t *r)
{

	/* XXX! */
	if (vh->vh_offset != 0) {
		vh->vh_offset += r->tr_end.tp_row - r->tr_begin.tp_row;
		if (vh->vh_offset > 100)
			vh->vh_offset = 100;
	}
}

void
vthistory_seek(struct vt_history *vh, int offset, int whence)
{

	/* XXX! */
	switch (whence) {
	case VHS_SET:
		vh->vh_offset = offset;
		break;
	case VHS_CUR:
		vh->vh_offset += offset;
		break;
	case VHS_END:
		vh->vh_offset = 100 + offset;
		break;
	}

	if (vh->vh_offset > 100)
		vh->vh_offset = 100;
}

void
vthistory_getpos(const struct vt_history *vh, unsigned int *offset)
{

	*offset = vh->vh_offset;
}

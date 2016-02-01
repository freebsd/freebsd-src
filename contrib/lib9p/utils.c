/*
 * Copyright 2016 Jakub Klama <jceel@FreeBSD.org>
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <assert.h>
#include <inttypes.h>
#include <sys/param.h>
#include <sys/uio.h>
#if defined(__FreeBSD__)
#include <sys/sbuf.h>
#else
#include "sbuf/sbuf.h"
#endif
#include "lib9p.h"
#include "fcall.h"

static const char *ftype_names[] = {
	"Tversion",
	"Rversion",
	"Tauth",
	"Rauth",
	"Tattach",
	"Rattach",
	"Terror",
	"Rerror",
	"Tflush",
	"Rflush",
	"Twalk",
	"Rwalk",
	"Topen",
	"Ropen",
	"Tcreate",
	"Rcreate",
	"Tread",
	"Rread",
	"Twrite",
	"Rwrite",
	"Tclunk",
	"Rclunk",
	"Tremove",
	"Rremove",
	"Tstat",
	"Rstat",
	"Twstat",
	"Rwstat"
};

void
l9p_seek_iov(struct iovec *iov1, size_t niov1, struct iovec *iov2,
    size_t *niov2, size_t seek)
{
	size_t remainder = 0;
	size_t left = seek;
	size_t i, j;

	for (i = 0; i < niov1; i++) {
		size_t toseek = MIN(left, iov1[i].iov_len);
		left -= toseek;

		if (toseek == iov1[i].iov_len)
			continue;

		if (left == 0) {
			remainder = toseek;
			break;
		}
	}

	for (j = i; j < niov1; j++) {
		iov2[j - i].iov_base = (char *)iov1[j].iov_base + remainder;
		iov2[j - i].iov_len = iov1[j].iov_len - remainder;
		remainder = 0;
	}

	*niov2 = j - i;
}

size_t
l9p_truncate_iov(struct iovec *iov, size_t niov, size_t length)
{
	size_t i, done = 0;

	for (i = 0; i < niov; i++) {
		size_t toseek = MIN(length - done, iov[i].iov_len);
		done += toseek;

		if (toseek < iov[i].iov_len) {
			iov[i].iov_len = toseek;
			return (i + 1);
		}
	}

	return (niov);
}

void
l9p_describe_qid(struct l9p_qid *qid, struct sbuf *sb)
{

	assert(qid != NULL);
	assert(sb != NULL);

	sbuf_printf(sb, "<0x%02x,%u,0x%016" PRIx64 ">", qid->type, qid->version,
	    qid->path);
}

void
l9p_describe_stat(struct l9p_stat *st, struct sbuf *sb)
{

	assert(st != NULL);
	assert(sb != NULL);

	sbuf_printf(sb, "type=0x%04x dev=%d name=\"%s\" uid=\"%s\"",
	    st->type, st->dev, st->name, st->uid);
}

void
l9p_describe_fcall(union l9p_fcall *fcall, enum l9p_version version,
    struct sbuf *sb)
{
	uint8_t type;
	int i;

	assert(fcall != NULL);
	assert(sb != NULL);
	assert(version <= L9P_2000L && version >= L9P_2000);

	type = fcall->hdr.type;

	if (type < 100 || type > 127) {
		sbuf_printf(sb, "<unknown request %d> tag=%d", type,
		    fcall->hdr.tag);
		return;
	}

	sbuf_printf(sb, "%s tag=%d", ftype_names[type - L9P_TVERSION],
	    fcall->hdr.tag);

	switch (type) {
		case L9P_TVERSION:
		case L9P_RVERSION:
			sbuf_printf(sb, " version=\"%s\" msize=%d", fcall->version.version,
			    fcall->version.msize);
			return;
		case L9P_TAUTH:
			sbuf_printf(sb, "afid=%d uname=\"%s\" aname=\"%s\"", fcall->hdr.fid,
			    fcall->tauth.uname, fcall->tauth.aname);
			return;
		case L9P_TATTACH:
			sbuf_printf(sb, " fid=%d afid=%d uname=\"%s\" aname=\"%s\"",
			    fcall->hdr.fid, fcall->tattach.afid, fcall->tattach.uname,
			    fcall->tattach.aname);
			if (version >= L9P_2000U)
				sbuf_printf(sb, " n_uname=%d", fcall->tattach.n_uname);
			return;
		case L9P_RERROR:
			sbuf_printf(sb, " ename=\"%s\" errnum=%d", fcall->error.ename,
			    fcall->error.errnum);
			return;
		case L9P_TFLUSH:
			sbuf_printf(sb, " oldtag=%d", fcall->tflush.oldtag);
			return;
		case L9P_TWALK:
			sbuf_printf(sb, " fid=%d newfid=%d wname=\"",
			    fcall->hdr.fid, fcall->twalk.newfid);

			for (i = 0; i < fcall->twalk.nwname; i++) {
				sbuf_printf(sb, "%s", fcall->twalk.wname[i]);
				if (i != fcall->twalk.nwname - 1)
					sbuf_printf(sb, "/");
			}
			sbuf_printf(sb, "\"");
			return;
		case L9P_RWALK:
			sbuf_printf(sb, " wqid=[");
			for (i = 0; i < fcall->rwalk.nwqid; i++) {
				l9p_describe_qid(&fcall->rwalk.wqid[i], sb);
				if (i != fcall->rwalk.nwqid - 1)
					sbuf_printf(sb, ",");
			}
			sbuf_printf(sb, "]");
			return;
		case L9P_TOPEN:
			sbuf_printf(sb, " fid=%d mode=%d", fcall->hdr.fid,
			    fcall->tcreate.mode);

			return;
		case L9P_ROPEN:
			sbuf_printf(sb, " qid=");
			l9p_describe_qid(&fcall->ropen.qid, sb);
			sbuf_printf(sb, " iounit=%d", fcall->ropen.iounit);
			return;
		case L9P_TCREATE:
			sbuf_printf(sb, " fid=%d name=\"%s\" perm=0x%08x mode=%d",
			    fcall->hdr.fid, fcall->tcreate.name, fcall->tcreate.perm,
			    fcall->tcreate.mode);
			return;
		case L9P_RCREATE:
			return;
		case L9P_TREAD:
			sbuf_printf(sb, " fid=%d offset=%" PRIu64 " count=%u", fcall->hdr.fid,
			    fcall->io.offset, fcall->io.count);
			return;

		case L9P_RREAD:
		case L9P_RWRITE:
			sbuf_printf(sb, " count=%d", fcall->io.count);
			return;
		case L9P_TWRITE:
			sbuf_printf(sb, " fid=%d offset=%" PRIu64 " count=%u", fcall->hdr.fid,
			    fcall->io.offset, fcall->io.count);
			return;
		case L9P_TCLUNK:
			sbuf_printf(sb, " fid=%d ", fcall->hdr.fid);
			return;
		case L9P_TREMOVE:
			sbuf_printf(sb, " fid=%d", fcall->hdr.fid);
			return;
		case L9P_RREMOVE:
			return;
		case L9P_TSTAT:
			sbuf_printf(sb, " fid=%d", fcall->hdr.fid);
			return;
		case L9P_RSTAT:
			sbuf_printf(sb, " ");
			l9p_describe_stat(&fcall->rstat.stat, sb);
			return;
		case L9P_TWSTAT:
			sbuf_printf(sb, " fid=%d ", fcall->hdr.fid);
			l9p_describe_stat(&fcall->twstat.stat, sb);
			return;
		case L9P_RWSTAT:
			return;
	}
}

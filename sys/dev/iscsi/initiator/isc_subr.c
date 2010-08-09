/*-
 * Copyright (c) 2005-2010 Daniel Braniss <danny@cs.huji.ac.il>
 * All rights reserved.
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
 *
 */
/*
 | iSCSI
 | $Id: isc_subr.c 560 2009-05-07 07:37:49Z danny $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_iscsi_initiator.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/ctype.h>
#include <sys/errno.h>
#include <sys/sysctl.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/socketvar.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/proc.h>
#include <sys/ioccom.h>
#include <sys/queue.h>
#include <sys/kthread.h>
#include <sys/syslog.h>
#include <sys/mbuf.h>
#include <sys/libkern.h>

#include <dev/iscsi/initiator/iscsi.h>
#include <dev/iscsi/initiator/iscsivar.h>

MALLOC_DEFINE(M_ISC, "iSC", "iSCSI driver options");

static char *
i_strdupin(char *s, size_t maxlen)
{
     size_t	len;
     char	*p, *q;

     p = malloc(maxlen, M_ISC, M_WAITOK);
     if(copyinstr(s, p, maxlen, &len)) {
	  free(p, M_ISC);
	  return NULL;
     }
     q = malloc(len, M_ISC, M_WAITOK);
     bcopy(p, q, len);
     free(p, M_ISC);

     return q;
}

static uint32_t
i_crc32c(const void *buf, size_t size, uint32_t crc)
{
     crc = crc ^ 0xffffffff;
     crc = calculate_crc32c(crc, buf, size);
     crc = crc ^ 0xffffffff;
     return crc;
}

/*
 | XXX: not finished coding
 */
int
i_setopt(isc_session_t *sp, isc_opt_t *opt)
{
     if(opt->maxRecvDataSegmentLength > 0) {
	  sp->opt.maxRecvDataSegmentLength = opt->maxRecvDataSegmentLength;
	  sdebug(2, "maxRecvDataSegmentLength=%d", sp->opt.maxRecvDataSegmentLength);
     }
     if(opt->maxXmitDataSegmentLength > 0) {
	  // danny's RFC
	  sp->opt.maxXmitDataSegmentLength = opt->maxXmitDataSegmentLength;
	  sdebug(2, "opt.maXmitDataSegmentLength=%d", sp->opt.maxXmitDataSegmentLength);
     }
     if(opt->maxBurstLength != 0) {
	  sp->opt.maxBurstLength = opt->maxBurstLength;
	  sdebug(2, "opt.maxBurstLength=%d", sp->opt.maxBurstLength);
     }

     if(opt->targetAddress != NULL) {
	  if(sp->opt.targetAddress != NULL)
	       free(sp->opt.targetAddress, M_ISC);
	  sp->opt.targetAddress = i_strdupin(opt->targetAddress, 128);
	  sdebug(2, "opt.targetAddress='%s'", sp->opt.targetAddress);
     }
     if(opt->targetName != NULL) {
	  if(sp->opt.targetName != NULL)
	       free(sp->opt.targetName, M_ISC);
	  sp->opt.targetName = i_strdupin(opt->targetName, 128);
	  sdebug(2, "opt.targetName='%s'", sp->opt.targetName);
     }
     if(opt->initiatorName != NULL) {
	  if(sp->opt.initiatorName != NULL)
	       free(sp->opt.initiatorName, M_ISC);
	  sp->opt.initiatorName = i_strdupin(opt->initiatorName, 128);
	  sdebug(2, "opt.initiatorName='%s'", sp->opt.initiatorName);
     }

     if(opt->maxluns > 0) {
	  if(opt->maxluns > ISCSI_MAX_LUNS)
	       sp->opt.maxluns = ISCSI_MAX_LUNS; // silently chop it down ...
	  sp->opt.maxluns = opt->maxluns;
	  sdebug(2, "opt.maxluns=%d", sp->opt.maxluns);
     }

     if(opt->headerDigest != NULL) {
	  sdebug(2, "opt.headerDigest='%s'", opt->headerDigest);
	  if(strcmp(opt->headerDigest, "CRC32C") == 0) {
	       sp->hdrDigest = (digest_t *)i_crc32c;
	       sdebug(2, "opt.headerDigest set");
	  }
     }
     if(opt->dataDigest != NULL) {
	  sdebug(2, "opt.dataDigest='%s'", opt->headerDigest);
	  if(strcmp(opt->dataDigest, "CRC32C") == 0) {
	       sp->dataDigest = (digest_t *)i_crc32c;
	       sdebug(2, "opt.dataDigest set");
	  }
     }

     return 0;
}

void
i_freeopt(isc_opt_t *opt)
{
     debug_called(8);

     if(opt->targetAddress != NULL) {
	  free(opt->targetAddress, M_ISC);
	  opt->targetAddress = NULL;
     }
     if(opt->targetName != NULL) {
	  free(opt->targetName, M_ISC);
	  opt->targetName = NULL;
     }
     if(opt->initiatorName != NULL) {
	  free(opt->initiatorName, M_ISC);
	  opt->initiatorName = NULL;
     }
}

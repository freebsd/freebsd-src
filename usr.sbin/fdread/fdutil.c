/*
 * Copyright (c) 2001 Joerg Wunsch
 *
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
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPERS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE DEVELOPERS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/fdcio.h>

#include <stdio.h>
#include <string.h>

#include <dev/ic/nec765.h>

#include "fdutil.h"

void
printstatus(struct fdc_status *fdcsp, int terse)
{
	char msgbuf[100];

	if (!terse)
		fprintf(stderr,
		"\nFDC status ST0=%#x ST1=%#x ST2=%#x C=%u H=%u R=%u N=%u:\n",
			fdcsp->status[0] & 0xff,
			fdcsp->status[1] & 0xff,
			fdcsp->status[2] & 0xff,
			fdcsp->status[3] & 0xff,
			fdcsp->status[4] & 0xff,
			fdcsp->status[5] & 0xff,
			fdcsp->status[6] & 0xff);

	if ((fdcsp->status[0] & NE7_ST0_IC_RC) != NE7_ST0_IC_AT) {
		sprintf(msgbuf, "unexcpted interrupt code %#x",
			fdcsp->status[0] & NE7_ST0_IC_RC);
	} else {
		strcpy(msgbuf, "unexpected error code in ST1/ST2");

		if (fdcsp->status[1] & NE7_ST1_EN)
			strcpy(msgbuf, "end of cylinder (wrong format)");
		else if (fdcsp->status[1] & NE7_ST1_DE) {
			if (fdcsp->status[2] & NE7_ST2_DD)
				strcpy(msgbuf, "CRC error in data field");
			else
				strcpy(msgbuf, "CRC error in ID field");
		} else if (fdcsp->status[1] & NE7_ST1_MA) {
			if (fdcsp->status[2] & NE7_ST2_MD)
				strcpy(msgbuf, "no address mark in data field");
			else
				strcpy(msgbuf, "no address mark in ID field");
		} else if (fdcsp->status[2] & NE7_ST2_WC)
			strcpy(msgbuf, "wrong cylinder (format mismatch)");
		else if (fdcsp->status[1] & NE7_ST1_ND)
			strcpy(msgbuf, "no data (sector not found)");
	}
	fputs(msgbuf, stderr);
}


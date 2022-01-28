/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021-2022 Alfonso Sabato Siciliano
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

#ifndef _LIBBSDDIALOG_PROGRESSVIEW_H_
#define _LIBBSDDIALOG_PROGRESSVIEW_H_

/*
 * Undocumented API, DO NOT USE!
 * Please consider this file private: it is used by bsdinstall/distextract,
 * could be deleted or changed in the future.
 */

extern bool bsddialog_interruptprogview;
extern bool bsddialog_abortprogview;
extern int  bsddialog_total_progview;

struct bsddialog_fileminibar {
	const char *path;
	const char *label;
	int status; /* next if BSDDIALOG_MG_DONE or BSDDIALOG_MG_FAILED */
	long long size;
	long long read;
};

struct bsddialog_progviewconf {
	const char *fmtbottomstr;
	unsigned int refresh; /* in seconds */
	int (*callback)(struct bsddialog_fileminibar *minibar);
};

int
bsddialog_progressview (struct bsddialog_conf *conf, const char *text, int rows,
    int cols, struct bsddialog_progviewconf *pvconf, unsigned int nminibar,
    struct bsddialog_fileminibar *minibar);

#endif
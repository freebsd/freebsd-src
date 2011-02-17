/*-
 * Copyright (c) 2006, Maxime Henrion <mux@FreeBSD.org>
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
 * $FreeBSD$
 */
#ifndef _STATUS_H_
#define _STATUS_H_

#include <time.h>

struct coll;
struct fattr;
struct status;

#define	SR_DIRDOWN			0
#define	SR_CHECKOUTLIVE			1
#define	SR_CHECKOUTDEAD			2
#define	SR_FILELIVE			3
#define	SR_FILEDEAD			4
#define	SR_DIRUP			5

struct statusrec {
	int		sr_type;
	char		*sr_file;
	char		*sr_tag;
	char		*sr_date;
	char		*sr_revnum;
	char		*sr_revdate;

	/*
	 * "clientrttr" contains the attributes of the client's file if there
	 * is one. "serverattr" contains the attributes of the corresponding
	 * file on the server.  In CVS mode, these are identical.  But in
	 * checkout mode, "clientattr" represents the checked-out file while
	 * "serverattr" represents the corresponding RCS file on the server.
	 */
	struct fattr	*sr_serverattr;
	struct fattr	*sr_clientattr;
};

struct status	*status_open(struct coll *, time_t, char **);
int		 status_get(struct status *, char *, int, int,
		     struct statusrec **);
int		 status_put(struct status *, struct statusrec *);
int		 status_eof(struct status *);
char		*status_errmsg(struct status *);
int		 status_delete(struct status *, char *, int);
void		 status_close(struct status *, char **);

#endif /* !_STATUS_H_ */

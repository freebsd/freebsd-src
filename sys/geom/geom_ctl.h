/*-
 * Copyright (c) 2003 Poul-Henning Kamp
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
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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

#ifndef _GEOM_GEOM_CTL_H_
#define _GEOM_GEOM_CTL_H_

/*
 * Version number.  Used to check consistency between kernel and libgeom.
 */
#define GEOM_CTL_VERSION	1

/*
 * Primitives.
 */
enum geom_ctl_request {
	GEOM_INVALID_REQUEST = 0,
	GEOM_CREATE_GEOM,
	GEOM_NEW_GEOM,
	GEOM_ATTACH,
	GEOM_DETACH,
	GEOM_CREATE_PROVIDER,
	GEOM_DESTROY_PROVIDER,
	GEOM_INSERT_GEOM,
	GEOM_ELIMINATE_GEOM,
	GEOM_WRITE_META,
	GEOM_READ_META
};

#ifdef GEOM_CTL_TABLE
struct geom_ctl_req_table {
	int             	class;
	int             	geom;
	int             	provider;
	int             	consumer;
	int             	params;
	int             	meta;
	char			*name;
	enum geom_ctl_request	request;
} gcrt[] = {
/*        Cl Ge Pr Co Pa Me Name                Request			*/
	{ 1, 1, 1, 0, 1, 0, "create geom",	GEOM_CREATE_GEOM            },
	{ 1, 1, 0, 0, 1, 0, "new geom",		GEOM_NEW_GEOM               },
	{ 0, 1, 1, 0, 1, 0, "attach",		GEOM_ATTACH                 },
	{ 0, 1, 1, 0, 1, 0, "detach",		GEOM_DETACH                 },
	{ 0, 1, 0, 0, 1, 0, "create provider",	GEOM_CREATE_PROVIDER        },
	{ 0, 1, 1, 0, 1, 0, "destroy provider",	GEOM_DESTROY_PROVIDER       },
	{ 1, 1, 1, 0, 1, 0, "insert geom",	GEOM_INSERT_GEOM            },
	{ 0, 1, 0, 0, 1, 0, "eliminate geom",	GEOM_ELIMINATE_GEOM         },
	{ 0, 1, 0, 0, 1, 1, "write meta",	GEOM_WRITE_META             },
	{ 0, 1, 0, 0, 1, 1, "read meta",	GEOM_READ_META              },

	/* Terminator entry */
	{ 1, 1, 1, 1, 1, 1, "*INVALID*",	GEOM_INVALID_REQUEST        }
};

#endif /* GEOM_CTL_TABLE */

#endif /* _GEOM_GEOM_CTL_H_ */

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
#define GCTL_VERSION	1

/*
 * Primitives.
 */
enum gctl_request {
	GCTL_INVALID_REQUEST = 0,

	GCTL_CREATE_GEOM,
	GCTL_DESTROY_GEOM,

	GCTL_ATTACH,
	GCTL_DETACH,

	GCTL_CREATE_PROVIDER,
	GCTL_DESTROY_PROVIDER,

	GCTL_INSERT_GEOM,
	GCTL_ELIMINATE_GEOM,

	GCTL_CONFIG_GEOM,
};

#ifdef GCTL_TABLE
struct gctl_req_table {
	int             	class;
	int             	geom;
	int             	provider;
	int             	consumer;
	int             	params;
	char			*name;
	enum gctl_request	request;
} gcrt[] = {
/*        Cl Ge Pr Co Pa Name                Request			*/
	{ 1, 0, 1, 0, 1, "create geom",		GCTL_CREATE_GEOM            },
	{ 0, 1, 0, 0, 1, "destroy geom",	GCTL_DESTROY_GEOM           },
	{ 0, 1, 1, 0, 1, "attach",		GCTL_ATTACH                 },
	{ 0, 1, 1, 0, 1, "detach",		GCTL_DETACH                 },
	{ 0, 1, 0, 0, 1, "create provider",	GCTL_CREATE_PROVIDER        },
	{ 0, 1, 1, 0, 1, "destroy provider",	GCTL_DESTROY_PROVIDER       },
	{ 1, 1, 1, 0, 1, "insert geom",		GCTL_INSERT_GEOM            },
	{ 0, 1, 0, 0, 1, "eliminate geom",	GCTL_ELIMINATE_GEOM         },
	{ 0, 1, 0, 0, 1, "config geom",		GCTL_CONFIG_GEOM            },

	/* Terminator entry */
	{ 1, 1, 1, 1, 1, "*INVALID*",		GCTL_INVALID_REQUEST        }
};

#endif /* GCTL_TABLE */

#endif /* _GEOM_GEOM_CTL_H_ */

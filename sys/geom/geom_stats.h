/*-
 * Copyright (c) 2002 Poul-Henning Kamp
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

#ifndef _GEOM_GEOM_STATS_H_
#define _GEOM_GEOM_STATS_H_

#define GEOM_STATS_DEVICE	"geom.stats"

/*
 * A g_stat contains the statistics the kernel collect on consumers and
 * providers.  See libgeom(3) for how to get hold of these.
 */
struct g_stat {
	int			updating;
				/*
				 * If non-zero, the structure is being
				 * updated by the kernel and the contents
				 * should not be used.
				 */

	void			*id;
				/* GEOM-identifier for the consumer/provider */

	uint64_t		nop;
				/* Number of requests started */

	uint64_t		nend;
				/* Number of requests completed */

	struct bintime		bt;
				/* Accumulated busy time */

	struct bintime		wentbusy;
				/* Busy time accounted for until here */
	struct {
		uint64_t	nop;
				/* Number of requests completed */

		uint64_t	nbyte;
				/* Number of bytes completed */

		uint64_t	nmem;
				/* Number of ENOMEM request errors */

		uint64_t	nerr;
				/* Number of other request errors */

		struct bintime	dt;
				/* Accumulated request processing time */

	} ops[3];

#define G_STAT_IDX_READ		0
#define G_STAT_IDX_WRITE	1
#define G_STAT_IDX_DELETE	2

};

#endif /* _GEOM_GEOM_STATS_H_ */

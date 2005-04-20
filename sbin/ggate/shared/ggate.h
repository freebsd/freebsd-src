/*-
 * Copyright (c) 2004 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
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

#ifndef _GGATE_H_
#define	_GGATE_H_

#include <sys/endian.h>

#define	G_GATE_BUFSIZE_START	65536
#define	G_GATE_PORT		3080

#define	G_GATE_RCVBUF		131072
#define	G_GATE_SNDBUF		131072
#define	G_GATE_QUEUE_SIZE	1024
#define	G_GATE_TIMEOUT		30

extern int g_gate_devfd;
extern int g_gate_verbose;

/* Client's initial packet. */
struct g_gate_cinit {
	char	gc_path[PATH_MAX + 1];
	uint8_t	gc_flags;
};

/* Server's initial packet. */
struct g_gate_sinit {
	uint8_t		gs_flags;
	uint64_t	gs_mediasize;
	uint32_t	gs_sectorsize;
	uint16_t	gs_error;
};

/* Control struct. */
struct g_gate_hdr {
	uint8_t		gh_cmd;		/* command */
	uint64_t	gh_offset;	/* device offset */
	uint32_t	gh_length;	/* size of block */
	int16_t		gh_error;	/* error value (0 if ok) */
};

void	g_gate_vlog(int priority, const char *message, va_list ap);
void	g_gate_log(int priority, const char *message, ...);
void	g_gate_xvlog(const char *message, va_list ap);
void	g_gate_xlog(const char *message, ...);
off_t	g_gate_mediasize(int fd);
size_t	g_gate_sectorsize(int fd);
void	g_gate_open_device(void);
void	g_gate_close_device(void);
void	g_gate_ioctl(unsigned long req, void *data);
void	g_gate_destroy(int unit, int force);
int	g_gate_openflags(unsigned ggflags);
void	g_gate_load_module(void);
#ifdef LIBGEOM
void	g_gate_list(int unit, int verbose);
#endif
in_addr_t g_gate_str2ip(const char *str);

/*
 * g_gate_swap2h_* - functions swap bytes to host byte order (from big endian).
 * g_gate_swap2n_* - functions swap bytes to network byte order (actually
 *                   to big endian byte order).
 */

static __inline void
g_gate_swap2h_cinit(struct g_gate_cinit *cinit __unused)
{

	/* Nothing here for now. */
}

static __inline void
g_gate_swap2n_cinit(struct g_gate_cinit *cinit __unused)
{

	/* Nothing here for now. */
}

static __inline void
g_gate_swap2h_sinit(struct g_gate_sinit *sinit)
{

	/* Swap only used fields. */
	sinit->gs_mediasize = be64toh(sinit->gs_mediasize);
	sinit->gs_sectorsize = be32toh(sinit->gs_sectorsize);
	sinit->gs_error = be16toh(sinit->gs_error);
}

static __inline void
g_gate_swap2n_sinit(struct g_gate_sinit *sinit)
{

	/* Swap only used fields. */
	sinit->gs_mediasize = htobe64(sinit->gs_mediasize);
	sinit->gs_sectorsize = htobe32(sinit->gs_sectorsize);
	sinit->gs_error = htobe16(sinit->gs_error);
}

static __inline void
g_gate_swap2h_hdr(struct g_gate_hdr *hdr)
{

	/* Swap only used fields. */
	hdr->gh_offset = be64toh(hdr->gh_offset);
	hdr->gh_length = be32toh(hdr->gh_length);
	hdr->gh_error = be16toh(hdr->gh_error);
}

static __inline void
g_gate_swap2n_hdr(struct g_gate_hdr *hdr)
{

	/* Swap only used fields. */
	hdr->gh_offset = htobe64(hdr->gh_offset);
	hdr->gh_length = htobe32(hdr->gh_length);
	hdr->gh_error = htobe16(hdr->gh_error);
}
#endif	/* _GGATE_H_ */

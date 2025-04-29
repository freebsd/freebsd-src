/*-
 * Copyright (c) 2006 M. Warner Losh <imp@FreeBSD.org>
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

#define SPIBUS_IVAR(d) (struct spibus_ivar *) device_get_ivars(d)
#define SPIBUS_SOFTC(d) (struct spibus_softc *) device_get_softc(d)

struct spibus_softc
{
	device_t	dev;
};

#define	SPIBUS_MODE_NONE	0
#define	SPIBUS_MODE_CPHA	1
#define	SPIBUS_MODE_CPOL	2
#define	SPIBUS_MODE_CPOL_CPHA	3

struct spibus_ivar
{
	uint32_t	cs;
	uint32_t	mode;
	uint32_t	clock;
	uint32_t	cs_delay;
	struct resource_list	rl;
};

#define	SPIBUS_CS_HIGH	(1U << 31)

enum {
	SPIBUS_IVAR_CS,		/* chip select that we're on */
	SPIBUS_IVAR_MODE,	/* SPI mode (0-3) */
	SPIBUS_IVAR_CLOCK,	/* maximum clock freq for device */
	SPIBUS_IVAR_CS_DELAY,	/* delay in microseconds after toggling chip select */
};

#define SPIBUS_ACCESSOR(A, B, T)					\
static inline int							\
spibus_get_ ## A(device_t dev, T *t)					\
{									\
	return BUS_READ_IVAR(device_get_parent(dev), dev,		\
	    SPIBUS_IVAR_ ## B, (uintptr_t *) t);			\
}									\
static inline int							\
spibus_set_ ## A(device_t dev, T t)					\
{									\
	return BUS_WRITE_IVAR(device_get_parent(dev), dev,		\
	    SPIBUS_IVAR_ ## B, (uintptr_t) t);			\
}

SPIBUS_ACCESSOR(cs,		CS,		uint32_t)
SPIBUS_ACCESSOR(mode,		MODE,		uint32_t)
SPIBUS_ACCESSOR(clock,		CLOCK,		uint32_t)
SPIBUS_ACCESSOR(cs_delay,	CS_DELAY,	uint32_t)

extern driver_t spibus_driver;
extern driver_t ofw_spibus_driver;

int spibus_attach(device_t);
device_t spibus_add_child_common(device_t, u_int, const char *, int, size_t);
void spibus_child_deleted(device_t, device_t);
void spibus_probe_nomatch(device_t, device_t);
int spibus_child_location(device_t, device_t, struct sbuf *);
int spibus_read_ivar(device_t, device_t, int, uintptr_t *);
int spibus_write_ivar(device_t, device_t, int, uintptr_t);

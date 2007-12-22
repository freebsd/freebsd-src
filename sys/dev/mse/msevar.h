/*-
 * Copyright (c) 2004 M. Warner Losh
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

/*-
 * Copyright 1992 by the University of Guelph
 *
 * Permission to use, copy and modify this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting
 * documentation.
 * University of Guelph makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 */

/* driver configuration flags (config) */
#define MSE_CONFIG_ACCEL	0x00f0  /* acceleration factor */
#define MSE_CONFIG_FLAGS	(MSE_CONFIG_ACCEL)

/*
 * Software control structure for mouse. The sc_enablemouse(),
 * sc_disablemouse() and sc_getmouse() routines must be called spl'd().
 */
typedef struct mse_softc {
	int		sc_flags;
	int		sc_mousetype;
	struct selinfo	sc_selp;
	struct resource	*sc_port;
	struct resource	*sc_intr;
	bus_space_tag_t	sc_iot;
	bus_space_handle_t sc_ioh;
	void		*sc_ih;
	void		(*sc_enablemouse)(bus_space_tag_t t,
			    bus_space_handle_t h);
	void		(*sc_disablemouse)(bus_space_tag_t t,
			    bus_space_handle_t h);
	void		(*sc_getmouse)(bus_space_tag_t t, bus_space_handle_t h,
			    int *dx, int *dy, int *but);
	int		sc_deltax;
	int		sc_deltay;
	int		sc_obuttons;
	int		sc_buttons;
	int		sc_bytesread;
	u_char		sc_bytes[MOUSE_SYS_PACKETSIZE];
	struct		callout_handle sc_callout;
	int		sc_watchdog;
	struct cdev *sc_dev;
	struct cdev *sc_ndev;
	mousehw_t	hw;
	mousemode_t	mode;
	mousestatus_t	status;
} mse_softc_t;

/* Flags */
#define	MSESC_OPEN	0x1
#define	MSESC_WANT	0x2

/* and Mouse Types */
#define	MSE_NONE	0	/* don't move this! */

/* pc98 bus mouse types */
#define	MSE_98BUSMOUSE	0x1

/* isa bus mouse types */
#define	MSE_LOGITECH	0x1
#define	MSE_ATIINPORT	0x2

#define	MSE_LOGI_SIG	0xA5

/* XXX msereg.h? */
#define	MSE_PORTA	0
#define	MSE_PORTB	1
#define	MSE_PORTC	2
#define	MSE_PORTD	3
#define MSE_IOSIZE	4

/*
 * Table of mouse types.
 * Keep the Logitech last, since I haven't figured out how to probe it
 * properly yet. (Someday I'll have the documentation.)
 */
struct mse_types {
	int	m_type;		/* Type of bus mouse */
	int	(*m_probe)(device_t dev, mse_softc_t *sc);
				/* Probe routine to test for it */
	void	(*m_enable)(bus_space_tag_t t, bus_space_handle_t h);
				/* Start routine */
	void	(*m_disable)(bus_space_tag_t t, bus_space_handle_t h);
				/* Disable interrupts routine */
	void	(*m_get)(bus_space_tag_t t, bus_space_handle_t h, int *dx,
		    int *dy, int *but);
				/* and get mouse status */
	mousehw_t   m_hw;	/* buttons iftype type model hwid */
	mousemode_t m_mode;	/* proto rate res accel level size mask */
};

extern devclass_t	mse_devclass;
int mse_common_attach(device_t);

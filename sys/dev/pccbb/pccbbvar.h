/*
 * Copyright (c) 2000,2001 Jonathan Chen.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Structure definitions for the Cardbus Bridge driver
 */

struct intrhand {
	void(*func)(void*arg);
	void	*arg;
	STAILQ_ENTRY(intrhand) entries;
};

struct pccbb_reslist {
	SLIST_ENTRY(pccbb_reslist) link;
	struct	resource *res;
	int	type;
	int	rid;
		/* note: unlike the regular resource list, there can be
		 * duplicate rid's in the same list.  However, the
		 * combination of rid and res->r_dev should be unique.
		 */
	bus_addr_t cardaddr; /* for 16-bit pccard memory */
};

#define	PCCBB_AUTO_OPEN_SMALLHOLE 0x100

struct pccbb_softc {
	device_t	sc_dev;
	struct		resource *sc_base_res;
	struct		resource *sc_irq_res;
	void		*sc_intrhand;
	bus_space_tag_t sc_bst;
	bus_space_handle_t sc_bsh;
	u_int8_t	sc_secbus;
	u_int8_t	sc_subbus;
	struct		mtx sc_mtx;
	u_int32_t	sc_flags;
#define	PCCBB_PCIC_IO_RELOC	0x01
#define	PCCBB_PCIC_MEM_32	0x02
#define	PCCBB_16BIT_CARD	0x02000000
#define	PCCBB_KTHREAD_RUNNING	0x04000000
#define	PCCBB_KTHREAD_DONE	0x08000000
	int		sc_chipset;		/* chipset id */
#define	CB_UNKNOWN	0		/* NOT Cardbus-PCI bridge */
#define	CB_TI113X	1		/* TI PCI1130/1131 */
#define	CB_TI12XX	2		/* TI PCI1250/1220 */
#define	CB_RF5C47X	3		/* RICOH RF5C475/476/477 */
#define	CB_RF5C46X	4		/* RICOH RF5C465/466/467 */
#define	CB_TOPIC95	5		/* Toshiba ToPIC95 */
#define	CB_TOPIC95B	6		/* Toshiba ToPIC95B */
#define	CB_TOPIC97	7		/* Toshiba ToPIC97/100 */
#define	CB_CIRRUS	8		/* Cirrus Logic CLPD683x */
	SLIST_HEAD(, pccbb_reslist) rl;

	device_t	sc_cbdev;
	device_t	sc_pccarddev;

	/* PC Card stuff */
	int		memalloc;
	struct		pccard_mem_handle mem[PCIC_MEM_WINS];
	int		ioalloc;
	struct		pccard_io_handle io[PCIC_IO_WINS];

	/* kthread staff */
	struct		proc *event_thread;
};

/* result of detect_card */
#define	CARD_UKN_CARD	0x00
#define	CARD_5V_CARD	0x01
#define	CARD_3V_CARD	0x02
#define	CARD_XV_CARD	0x04
#define	CARD_YV_CARD	0x08

/* for power_socket */
#define	CARD_VCC_UC	0x0000
#define	CARD_VCC_3V	0x0001
#define	CARD_VCC_XV	0x0002
#define	CARD_VCC_YV	0x0003
#define	CARD_VCC_0V	0x0004
#define	CARD_VCC_5V	0x0005
#define	CARD_VCCMASK	0x000f
#define	CARD_VPP_UC	0x0000
#define	CARD_VPP_VCC	0x0010
#define	CARD_VPP_12V	0x0030
#define	CARD_VPP_0V	0x0040
#define	CARD_VPPMASK	0x00f0

/*-
 * Copyright (C) 2012 Margarida Gouveia
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_POWERPC_WII_WIIFB_H
#define	_POWERPC_WII_WIIFB_H

#define	WIIFB_FONT_HEIGHT	8

enum wiifb_format {
	WIIFB_FORMAT_NTSC  = 0,
	WIIFB_FORMAT_PAL   = 1,
	WIIFB_FORMAT_MPAL  = 2,
	WIIFB_FORMAT_DEBUG = 3
};

enum wiifb_mode {
	WIIFB_MODE_NTSC_480i = 0,
	WIIFB_MODE_NTSC_480p = 1,
	WIIFB_MODE_PAL_576i  = 2,
	WIIFB_MODE_PAL_480i  = 3,
	WIIFB_MODE_PAL_480p  = 4
};

struct wiifb_mode_desc {
	const char 	*fd_name;
	unsigned int	fd_width;
	unsigned int	fd_height;
	unsigned int	fd_lines;
	uint8_t		fd_flags;
#define WIIFB_MODE_FLAG_PROGRESSIVE	0x00
#define WIIFB_MODE_FLAG_INTERLACED	0x01
};

struct wiifb_softc {
	video_adapter_t	sc_va;
	struct cdev	*sc_si;
	int		sc_console;

	intptr_t	sc_reg_addr;
	unsigned int	sc_reg_size;

	intptr_t	sc_fb_addr;
	unsigned int	sc_fb_size;

	unsigned int	sc_height;
	unsigned int	sc_width;
	unsigned int	sc_stride;

	unsigned int	sc_xmargin;
	unsigned int	sc_ymargin;

	boolean_t	sc_component;
	enum wiifb_format sc_format;
	struct wiifb_mode_desc *sc_mode;

	unsigned int	sc_vtiming;
	unsigned int	sc_htiming;

	unsigned char	*sc_font;
	int		sc_initialized;
	int		sc_rrid;
};

/*
 * Vertical timing
 * 16 bit
 */
#define	WIIFB_REG_VTIMING	0x00
struct wiifb_vtiming {
	uint8_t		vt_eqpulse;
	uint16_t	vt_actvideo;
};

static __inline void
wiifb_vtiming_read(struct wiifb_softc *sc, struct wiifb_vtiming *vt)
{
	volatile uint16_t *reg =
	    (uint16_t *)(sc->sc_reg_addr + WIIFB_REG_VTIMING);
	
	vt->vt_eqpulse  = *reg & 0xf;
	vt->vt_actvideo = (*reg >> 4) & 0x3ff;
}

static __inline void
wiifb_vtiming_write(struct wiifb_softc *sc, struct wiifb_vtiming *vt)
{
	volatile uint16_t *reg =
	    (uint16_t *)(sc->sc_reg_addr + WIIFB_REG_VTIMING);

	*reg = ((vt->vt_actvideo & 0x3ff) << 4) |
	        (vt->vt_eqpulse & 0xf);
	powerpc_sync();
}

/*
 * Display configuration
 * 16 bit
 */
#define	WIIFB_REG_DISPCFG	0x02
struct wiifb_dispcfg {
	uint8_t		  dc_enable;
	uint8_t		  dc_reset;
	uint8_t		  dc_noninterlaced;
	uint8_t		  dc_3dmode;
	uint8_t		  dc_latchenb0;
	uint8_t		  dc_latchenb1;
	enum wiifb_format dc_format;
};

static __inline void
wiifb_dispcfg_read(struct wiifb_softc *sc, struct wiifb_dispcfg *dc)
{
	volatile uint16_t *reg =
	    (uint16_t *)(sc->sc_reg_addr + WIIFB_REG_DISPCFG);

	dc->dc_enable        = *reg & 0x1;
	dc->dc_reset         = (*reg >> 1) & 0x1;
	dc->dc_noninterlaced = (*reg >> 2) & 0x1;
	dc->dc_3dmode        = (*reg >> 3) & 0x1;
	dc->dc_latchenb0     = (*reg >> 4) & 0x3;
	dc->dc_latchenb1     = (*reg >> 6) & 0x3;
	dc->dc_format        = (*reg >> 8) & 0x3;
}

static __inline void
wiifb_dispcfg_write(struct wiifb_softc *sc, struct wiifb_dispcfg *dc)
{
	volatile uint16_t *reg =
	    (uint16_t *)(sc->sc_reg_addr + WIIFB_REG_DISPCFG);

	*reg = ((dc->dc_format & 0x3) << 8)        |
	       ((dc->dc_latchenb1 & 0x3) << 6)     |
	       ((dc->dc_latchenb0 & 0x3) << 4)     |
	       ((dc->dc_3dmode & 0x1) << 3)        |
	       ((dc->dc_noninterlaced & 0x1) << 2) |
	       ((dc->dc_reset & 0x1) << 1)         |
	        (dc->dc_enable & 0x1);
	powerpc_sync();
}

/*
 * Horizontal Timing 0
 * 32 bit
 */
#define	WIIFB_REG_HTIMING0		0x04
struct wiifb_htiming0 {
	uint16_t	ht0_hlinew;	/* half line width */
	uint8_t		ht0_hcolourend;
	uint8_t		ht0_hcolourstart;
};

static __inline void
wiifb_htiming0_read(struct wiifb_softc *sc, struct wiifb_htiming0 *ht0)
{
	volatile uint32_t *reg =
	    (uint32_t *)(sc->sc_reg_addr + WIIFB_REG_HTIMING0);

	ht0->ht0_hlinew       = *reg & 0x1ff;
	ht0->ht0_hcolourend   = (*reg >> 16) & 0x7f;
	ht0->ht0_hcolourstart = (*reg >> 24) & 0x7f;
}

static __inline void
wiifb_htiming0_write(struct wiifb_softc *sc, struct wiifb_htiming0 *ht0)
{
	volatile uint32_t *reg =
	    (uint32_t *)(sc->sc_reg_addr + WIIFB_REG_HTIMING0);

	*reg = ((ht0->ht0_hcolourstart & 0x7f) << 24) |
	       ((ht0->ht0_hcolourend & 0x7f) << 16)   |
	        (ht0->ht0_hlinew & 0x1ff);
	powerpc_sync();
}
/*
 * Horizontal Timing 1
 * 32 bit
 */
#define	WIIFB_REG_HTIMING1		0x08
struct wiifb_htiming1 {
	uint8_t		ht1_hsyncw;
	uint16_t	ht1_hblankend;
	uint16_t	ht1_hblankstart;
};

static __inline void
wiifb_htiming1_read(struct wiifb_softc *sc, struct wiifb_htiming1 *ht1)
{
	volatile uint32_t *reg =
	    (uint32_t *)(sc->sc_reg_addr + WIIFB_REG_HTIMING1);

	ht1->ht1_hsyncw      = *reg & 0x7f;
	ht1->ht1_hblankend   = (*reg >> 7) & 0x3ff;
	ht1->ht1_hblankstart = (*reg >> 17) & 0x3ff;
}

static __inline void
wiifb_htiming1_write(struct wiifb_softc *sc, struct wiifb_htiming1 *ht1)
{
	volatile uint32_t *reg =
	    (uint32_t *)(sc->sc_reg_addr + WIIFB_REG_HTIMING1);

	*reg = ((ht1->ht1_hblankstart & 0x3ff) << 17) |
	       ((ht1->ht1_hblankend & 0x3ff) << 7)    |
	        (ht1->ht1_hsyncw & 0x7f);
	powerpc_sync();
}

/*
 * Vertical Timing Odd
 * 32 bit
 */
#define	WIIFB_REG_VTIMINGODD		0x0c
struct wiifb_vtimingodd {
	uint16_t	vto_preb;	/* pre blanking */
	uint16_t	vto_postb;	/* post blanking */
};

static __inline void
wiifb_vtimingodd_read(struct wiifb_softc *sc, struct wiifb_vtimingodd *vto)
{
	volatile uint32_t *reg =
	    (uint32_t *)(sc->sc_reg_addr + WIIFB_REG_VTIMINGODD);

	vto->vto_preb  = *reg & 0x3ff;
	vto->vto_postb = (*reg >> 16) & 0x3ff;
}

static __inline void
wiifb_vtimingodd_write(struct wiifb_softc *sc, struct wiifb_vtimingodd *vto)
{
	volatile uint32_t *reg =
	    (uint32_t *)(sc->sc_reg_addr + WIIFB_REG_VTIMINGODD);

	*reg = ((vto->vto_postb & 0x3ff) << 16) | 
	        (vto->vto_preb & 0x3ff);
	powerpc_sync();
}

/*
 * Vertical Timing Even
 * 32 bit
 */
#define	WIIFB_REG_VTIMINGEVEN		0x10
struct wiifb_vtimingeven {
	uint16_t	vte_preb;	/* pre blanking */
	uint16_t	vte_postb;	/* post blanking */
};

static __inline void
wiifb_vtimingeven_read(struct wiifb_softc *sc, struct wiifb_vtimingeven *vte)
{
	volatile uint32_t *reg =
	    (uint32_t *)(sc->sc_reg_addr + WIIFB_REG_VTIMINGEVEN);

	vte->vte_preb  = *reg & 0x3ff;
	vte->vte_postb = (*reg >> 16) & 0x3ff;
}

static __inline void
wiifb_vtimingeven_write(struct wiifb_softc *sc, struct wiifb_vtimingeven *vte)
{
	volatile uint32_t *reg =
	    (uint32_t *)(sc->sc_reg_addr + WIIFB_REG_VTIMINGEVEN);

	*reg = ((vte->vte_postb & 0x3ff) << 16) | 
	        (vte->vte_preb & 0x3ff);
	powerpc_sync();
}

/*
 * Burst Blanking Odd Interval
 * 32 bit
 */
#define	WIIFB_REG_BURSTBLANKODD		0x14
struct wiifb_burstblankodd {
	uint8_t		bbo_bs1;
	uint16_t	bbo_be1;
	uint8_t		bbo_bs3;
	uint16_t	bbo_be3;
};

static __inline void
wiifb_burstblankodd_read(struct wiifb_softc *sc,
    struct wiifb_burstblankodd *bbo)
{
	volatile uint32_t *reg =
	    (uint32_t *)(sc->sc_reg_addr + WIIFB_REG_BURSTBLANKODD);

	bbo->bbo_bs1 = *reg & 0x1f;
	bbo->bbo_be1 = (*reg >> 5) & 0x7ff;
	bbo->bbo_bs3 = (*reg >> 16) & 0x1f;
	bbo->bbo_be3 = (*reg >> 21) & 0x7ff;
}

static __inline void
wiifb_burstblankodd_write(struct wiifb_softc *sc,
    struct wiifb_burstblankodd *bbo)
{
	volatile uint32_t *reg =
	    (uint32_t *)(sc->sc_reg_addr + WIIFB_REG_BURSTBLANKODD);

	*reg = ((bbo->bbo_be3 & 0x7ff) << 21) |
	       ((bbo->bbo_bs3 & 0x1f) << 16)  |
	       ((bbo->bbo_be1 & 0x7ff) << 5)  |
	        (bbo->bbo_bs1 & 0x1f);
	powerpc_sync();
}

/*
 * Burst Blanking Even Interval
 * 32 bit
 */
#define	WIIFB_REG_BURSTBLANKEVEN	0x18
struct wiifb_burstblankeven {
	uint8_t		bbe_bs2;
	uint16_t	bbe_be2;
	uint8_t		bbe_bs4;
	uint16_t	bbe_be4;
};

static __inline void
wiifb_burstblankeven_read(struct wiifb_softc *sc,
    struct wiifb_burstblankeven *bbe)
{
	volatile uint32_t *reg =
	    (uint32_t *)(sc->sc_reg_addr + WIIFB_REG_BURSTBLANKEVEN);

	bbe->bbe_bs2 = *reg & 0x1f;
	bbe->bbe_be2 = (*reg >> 5) & 0x7ff;
	bbe->bbe_bs4 = (*reg >> 16) & 0x1f;
	bbe->bbe_be4 = (*reg >> 21) & 0x7ff;
}

static __inline void
wiifb_burstblankeven_write(struct wiifb_softc *sc,
    struct wiifb_burstblankeven *bbe)
{
	volatile uint32_t *reg =
	    (uint32_t *)(sc->sc_reg_addr + WIIFB_REG_BURSTBLANKEVEN);

	*reg = ((bbe->bbe_be4 & 0x7ff) << 21) |
	       ((bbe->bbe_bs4 & 0x1f) << 16)  |
	       ((bbe->bbe_be2 & 0x7ff) << 5)  |
	        (bbe->bbe_bs2 & 0x1f);
	powerpc_sync();
}

/*
 * Top Field Base Left
 * 32 bit
 */
#define	WIIFB_REG_TOPFIELDBASEL		0x1c
struct wiifb_topfieldbasel {
	uint32_t	tfbl_fbaddr;
	uint8_t		tfbl_xoffset;
	uint8_t		tfbl_pageoffbit;
};

static __inline void
wiifb_topfieldbasel_read(struct wiifb_softc *sc,
    struct wiifb_topfieldbasel *tfbl)
{
	volatile uint32_t *reg =
	    (uint32_t *)(sc->sc_reg_addr + WIIFB_REG_TOPFIELDBASEL);

	tfbl->tfbl_fbaddr     = *reg & 0xffffff;
	tfbl->tfbl_xoffset    = (*reg >> 24) & 0xf;
	tfbl->tfbl_pageoffbit = (*reg >> 28) & 0x1;
}

static __inline void
wiifb_topfieldbasel_write(struct wiifb_softc *sc,
    struct wiifb_topfieldbasel *tfbl)
{
	volatile uint32_t *reg =
	    (uint32_t *)(sc->sc_reg_addr + WIIFB_REG_TOPFIELDBASEL);

	*reg = ((tfbl->tfbl_pageoffbit & 0x1) << 28) |
	       ((tfbl->tfbl_xoffset & 0xf) << 24)    |
	        (tfbl->tfbl_fbaddr & 0xffffff);
	powerpc_sync();
}

/*
 * Top Field Base Right
 * 32 bit
 */
#define	WIIFB_REG_TOPFIELDBASER		0x20
struct wiifb_topfieldbaser {
	uint32_t	tfbr_fbaddr;
	uint8_t		tfbr_pageoffbit;
};

static __inline void
wiifb_topfieldbaser_read(struct wiifb_softc *sc,
    struct wiifb_topfieldbaser *tfbr)
{
	volatile uint32_t *reg =
	    (uint32_t *)(sc->sc_reg_addr + WIIFB_REG_TOPFIELDBASER);

	tfbr->tfbr_fbaddr     = *reg & 0xffffff;
	tfbr->tfbr_pageoffbit = (*reg >> 28) & 0x1;
}

static __inline void
wiifb_topfieldbaser_write(struct wiifb_softc *sc,
    struct wiifb_topfieldbaser *tfbr)
{
	volatile uint32_t *reg =
	    (uint32_t *)(sc->sc_reg_addr + WIIFB_REG_TOPFIELDBASER);

	*reg  = ((tfbr->tfbr_pageoffbit & 0x1) << 28) |
		 (tfbr->tfbr_fbaddr & 0xffffff);
	powerpc_sync();
}

/*
 * Bottom Field Base Left
 * 32 bit
 */
#define	WIIFB_REG_BOTTOMFIELDBASEL	0x24
struct wiifb_bottomfieldbasel {
	uint32_t	bfbl_fbaddr;
	uint8_t		bfbl_xoffset;
	uint8_t		bfbl_pageoffbit;
};

static __inline void
wiifb_bottomfieldbasel_read(struct wiifb_softc *sc,
    struct wiifb_bottomfieldbasel *bfbl)
{
	volatile uint32_t *reg =
	    (uint32_t *)(sc->sc_reg_addr + WIIFB_REG_BOTTOMFIELDBASEL);

	bfbl->bfbl_fbaddr     = *reg & 0xffffff;
	bfbl->bfbl_xoffset    = (*reg >> 24) & 0xf;
	bfbl->bfbl_pageoffbit = (*reg >> 28) & 0x1;
}

static __inline void
wiifb_bottomfieldbasel_write(struct wiifb_softc *sc,
    struct wiifb_bottomfieldbasel *bfbl)
{
	volatile uint32_t *reg =
	    (uint32_t *)(sc->sc_reg_addr + WIIFB_REG_BOTTOMFIELDBASEL);

	*reg  = ((bfbl->bfbl_pageoffbit & 0x1) << 28) |
	        ((bfbl->bfbl_xoffset & 0xf) << 24)    |
		 (bfbl->bfbl_fbaddr & 0xffffff);
	powerpc_sync();
}

/*
 * Bottom Field Base Right
 * 32 bit
 */
#define	WIIFB_REG_BOTTOMFIELDBASER	0x28
struct wiifb_bottomfieldbaser {
	uint32_t	bfbr_fbaddr;
	uint8_t		bfbr_pageoffbit;
};

static __inline void
wiifb_bottomfieldbaser_read(struct wiifb_softc *sc,
    struct wiifb_bottomfieldbaser *bfbr)
{
	volatile uint32_t *reg =
	    (uint32_t *)(sc->sc_reg_addr + WIIFB_REG_BOTTOMFIELDBASER);

	bfbr->bfbr_fbaddr     = *reg & 0xffffff;
	bfbr->bfbr_pageoffbit = (*reg >> 28) & 0x1;
}

static __inline void
wiifb_bottomfieldbaser_write(struct wiifb_softc *sc,
    struct wiifb_bottomfieldbaser *bfbr)
{
	volatile uint32_t *reg =
	    (uint32_t *)(sc->sc_reg_addr + WIIFB_REG_BOTTOMFIELDBASER);

	*reg  = ((bfbr->bfbr_pageoffbit & 0x1) << 28) |
		 (bfbr->bfbr_fbaddr & 0xffffff);
	powerpc_sync();
}

/*
 * Display Position Vertical
 * 16 bit
 */
#define	WIIFB_REG_DISPPOSV		0x2c
static __inline uint16_t
wiifb_dispposv_read(struct wiifb_softc *sc)
{
	volatile uint32_t *reg =
	    (uint32_t *)(sc->sc_reg_addr + WIIFB_REG_DISPPOSV);

	return (*reg & 0x7ff);
}

static __inline void
wiifb_dispposv_write(struct wiifb_softc *sc, uint16_t val)
{
	volatile uint32_t *reg =
	    (uint32_t *)(sc->sc_reg_addr + WIIFB_REG_DISPPOSV);

	*reg = val & 0x7ff;
	powerpc_sync();
}

/*
 * Display Position Horizontal
 * 16 bit
 */
#define	WIIFB_REG_DISPPOSH		0x2e
static __inline uint16_t
wiifb_dispposh_read(struct wiifb_softc *sc)
{
	volatile uint32_t *reg =
	    (uint32_t *)(sc->sc_reg_addr + WIIFB_REG_DISPPOSH);

	return (*reg & 0x7ff);
}

static __inline void
wiifb_dispposh_write(struct wiifb_softc *sc, uint16_t val)
{
	volatile uint32_t *reg =
	    (uint32_t *)(sc->sc_reg_addr + WIIFB_REG_DISPPOSH);

	*reg = val & 0x7ff;
	powerpc_sync();
}

/*
 * Display Interrupts.
 * There are 4 display interrupt registers, all 32 bit.
 */
#define	WIIFB_REG_DISPINT0		0x30
#define	WIIFB_REG_DISPINT1		0x34
#define	WIIFB_REG_DISPINT2		0x38
#define	WIIFB_REG_DISPINT3		0x3c
struct wiifb_dispint {
	uint16_t	di_htiming;
	uint16_t	di_vtiming;
	uint8_t		di_enable;
	uint8_t		di_irq;
};

static __inline void
wiifb_dispint_read(struct wiifb_softc *sc, int regno, struct wiifb_dispint *di)
{
	volatile uint32_t *reg = (uint32_t *)(sc->sc_reg_addr +
	    WIIFB_REG_DISPINT0 + regno * 4);

	di->di_htiming = *reg & 0x3ff;
	di->di_vtiming = (*reg >> 16) & 0x3ff;
	di->di_enable   = (*reg >> 28) & 0x1;
	di->di_irq      = (*reg >> 31) & 0x1;
}

static __inline void
wiifb_dispint_write(struct wiifb_softc *sc, int regno, struct wiifb_dispint *di)
{
	volatile uint32_t *reg = (uint32_t *)(sc->sc_reg_addr +
	    WIIFB_REG_DISPINT0 + regno * 4);

	*reg = ((di->di_irq & 0x1) << 31)        |
	       ((di->di_enable & 0x1) << 28)     |
	       ((di->di_vtiming & 0x3ff) << 16)  |
	        (di->di_htiming & 0x3ff);
	powerpc_sync();
}

/*
 * Display Latch 0
 * 32 bit
 */
#define	WIIFB_REG_DISPLAYTCH0		0x40

/*
 * Display Latch 1
 * 32 bit
 */
#define	WIIFB_REG_DISPLAYTCH1		0x44

/*
 * Picture Configuration
 * 16 bit
 */
#define	WIIFB_REG_PICCONF		0x48
struct wiifb_picconf {
	uint8_t		pc_strides;	/* strides per line (words) */
	uint8_t		pc_reads;	/* reads per line (words */
};

static __inline void
wiifb_picconf_read(struct wiifb_softc *sc, struct wiifb_picconf *pc)
{
	volatile uint16_t *reg =
	    (uint16_t *)(sc->sc_reg_addr + WIIFB_REG_PICCONF);

	pc->pc_strides = *reg & 0xff;
	pc->pc_reads   = (*reg >> 8) & 0xff;
}

static __inline void
wiifb_picconf_write(struct wiifb_softc *sc, struct wiifb_picconf *pc)
{
	volatile uint16_t *reg =
	    (uint16_t *)(sc->sc_reg_addr + WIIFB_REG_PICCONF);

	*reg = ((pc->pc_reads & 0xff) << 8) |
	        (pc->pc_strides & 0xff);
	powerpc_sync();
}

/*
 * Horizontal Scaling
 * 16 bit
 */
#define	WIIFB_REG_HSCALING		0x4a
struct wiifb_hscaling {
	uint16_t	hs_step;
	uint8_t		hs_enable;
};

static __inline void
wiifb_hscaling_read(struct wiifb_softc *sc, struct wiifb_hscaling *hs)
{
	volatile uint16_t *reg =
	    (uint16_t *)(sc->sc_reg_addr + WIIFB_REG_HSCALING);

	hs->hs_step   = *reg & 0x1ff;
	hs->hs_enable = (*reg >> 12) & 0x1;
}

static __inline void
wiifb_hscaling_write(struct wiifb_softc *sc, struct wiifb_hscaling *hs)
{
	volatile uint16_t *reg =
	    (uint16_t *)(sc->sc_reg_addr + WIIFB_REG_HSCALING);

	*reg = ((hs->hs_step & 0x1ff) << 12) |
	        (hs->hs_enable & 0x1);
	powerpc_sync();
}

/*
 * Filter Coeficient Table 0-6
 * 32 bit
 */
#define	WIIFB_REG_FILTCOEFT0		0x4c
#define	WIIFB_REG_FILTCOEFT1		0x50
#define	WIIFB_REG_FILTCOEFT2		0x54
#define	WIIFB_REG_FILTCOEFT3		0x58
#define	WIIFB_REG_FILTCOEFT4		0x5c
#define	WIIFB_REG_FILTCOEFT5		0x60
#define	WIIFB_REG_FILTCOEFT6		0x64
static __inline void
wiifb_filtcoeft_write(struct wiifb_softc *sc, unsigned int regno,
    uint32_t coeft)
{
	volatile uint32_t *reg =
	    (uint32_t *)(sc->sc_reg_addr + WIIFB_REG_FILTCOEFT0 + 4 * regno);

	*reg = coeft;
	powerpc_sync();
}

/*
 * Anti-aliasing
 * 32 bit
 */
#define	WIIFB_REG_ANTIALIAS		0x68
static __inline void
wiifb_antialias_write(struct wiifb_softc *sc, uint32_t antialias)
{
	volatile uint32_t *reg =
	    (uint32_t *)(sc->sc_reg_addr + WIIFB_REG_ANTIALIAS);

	*reg = antialias;
	powerpc_sync();
}

/*
 * Video Clock
 * 16 bit
 */
#define	WIIFB_REG_VIDEOCLK		0x6c
static __inline uint8_t
wiifb_videoclk_read(struct wiifb_softc *sc)
{
	volatile uint16_t *reg =
	    (uint16_t *)(sc->sc_reg_addr + WIIFB_REG_VIDEOCLK);

	return (*reg & 0x1);
}

static __inline void
wiifb_videoclk_write(struct wiifb_softc *sc, uint16_t clk54mhz)
{
	volatile uint16_t *reg =
	    (uint16_t *)(sc->sc_reg_addr + WIIFB_REG_VIDEOCLK);

	*reg = clk54mhz & 0x1;
	powerpc_sync();
}

/*
 * DTV Status
 * 16 bit
 *
 * DTV is another name for the Component Cable output.
 */
#define	WIIFB_REG_DTVSTATUS		0x6e
static __inline uint16_t
wiifb_dtvstatus_read(struct wiifb_softc *sc)
{
	volatile uint16_t *reg =
	    (uint16_t *)(sc->sc_reg_addr + WIIFB_REG_DTVSTATUS);

	return (*reg & 0x1);
}

static __inline uint16_t
wiifb_component_enabled(struct wiifb_softc *sc)
{
	
	return wiifb_dtvstatus_read(sc);
}

/*
 * Horizontal Scaling Width
 * 16 bit
 */
#define	WIIFB_REG_HSCALINGW		0x70
static __inline uint16_t
wiifb_hscalingw_read(struct wiifb_softc *sc)
{
	volatile uint16_t *reg =
	    (uint16_t *)(sc->sc_reg_addr + WIIFB_REG_HSCALINGW);

	return (*reg & 0x3ff);
}

static __inline void
wiifb_hscalingw_write(struct wiifb_softc *sc, uint16_t width)
{
	volatile uint16_t *reg =
	    (uint16_t *)(sc->sc_reg_addr + WIIFB_REG_HSCALINGW);

	*reg = width & 0x3ff;
	powerpc_sync();
}

/* 
 * Horizontal Border End
 * For debug mode only. Not used by this driver.
 * 16 bit
 */
#define	WIIFB_REG_HBORDEREND		0x72
static __inline void
wiifb_hborderend_write(struct wiifb_softc *sc, uint16_t border)
{
	volatile uint16_t *reg =
	    (uint16_t *)(sc->sc_reg_addr + WIIFB_REG_HBORDEREND);

	*reg = border;
	powerpc_sync();
}

/* 
 * Horizontal Border Start
 * 16 bit
 */
#define	WIIFB_REG_HBORDERSTART		0x74
static __inline void
wiifb_hborderstart_write(struct wiifb_softc *sc, uint16_t border)
{
	volatile uint16_t *reg =
	    (uint16_t *)(sc->sc_reg_addr + WIIFB_REG_HBORDERSTART);

	*reg = border;
	powerpc_sync();
}

/*
 * Unknown register
 * 16 bit
 */
#define	WIIFB_REG_UNKNOWN1		0x76
static __inline void
wiifb_unknown1_write(struct wiifb_softc *sc, uint16_t unknown)
{
	volatile uint16_t *reg =
	    (uint16_t *)(sc->sc_reg_addr + WIIFB_REG_UNKNOWN1);

	*reg = unknown;
	powerpc_sync();
}

/* 
 * Unknown register
 * 32 bit
 */
#define	WIIFB_REG_UNKNOWN2		0x78
static __inline void
wiifb_unknown2_write(struct wiifb_softc *sc, uint32_t unknown)
{
	volatile uint32_t *reg =
	    (uint32_t *)(sc->sc_reg_addr + WIIFB_REG_UNKNOWN2);

	*reg = unknown;
	powerpc_sync();
}

/*
 * Unknown register
 * 32 bit
 */
#define	WIIFB_REG_UNKNOWN3		0x7c
static __inline void
wiifb_unknown3_write(struct wiifb_softc *sc, uint32_t unknown)
{
	volatile uint32_t *reg =
	    (uint32_t *)(sc->sc_reg_addr + WIIFB_REG_UNKNOWN3);

	*reg = unknown;
	powerpc_sync();
}

#endif /* _POWERPC_WII_WIIFB_H */

/*
 * Copyright 1996 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: ide_pci.c,v 1.13 1998/07/11 07:45:52 bde Exp $
 */

#include "pci.h"
#if NPCI > 0
#include "opt_wd.h"
#include "wd.h"

#if NWDC > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <i386/isa/wdreg.h>
#ifdef PC98
#include <pc98/pc98/pc98.h>
#else
#include <i386/isa/isa.h>
#endif
#include <i386/isa/isa_device.h>

#include <pci/pcivar.h>
#include <pci/pcireg.h>
#include <pci/ide_pcireg.h>

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#define PROMISE_ULTRA33 0x4d33105a

struct ide_pci_cookie;  /* structs vendor_fns, ide_pci_cookie are recursive */

struct vendor_fns {
	int (*vendor_dmainit)   /* initialize DMA controller and drive */
	     (struct	ide_pci_cookie *cookie,
	      struct	wdparams *wp, 
	      int	(*wdcmd)(int, void *),
	      void 	*);
	
	     void (*vendor_status) /* prints off DMA timing info */
	     (struct	ide_pci_cookie *cookie);
};

/*
 * XXX the fact that this list keeps all kinds of info on PCI controllers
 * is pretty grotty-- much of this should be replaced by a proper integration
 * of PCI probes into the wd driver.
 * XXX if we're going to support native-PCI controllers, we also need to
 * keep the address of the IDE control block register, which is something wd.c
 * needs to know, which is why this info is in the wrong place.
 */

struct ide_pci_cookie {
	LIST_ENTRY(ide_pci_cookie) le;
	int		iobase_wd;
	int		ctlr;	/* controller 0/1 on PCI IDE interface */
	int		unit;
	int		iobase_bm; /* SFF-8038 control registers */
	int		altiobase_wd;
	pcici_t		tag;
	pcidi_t		type;
	struct		ide_pci_prd *prd;
	struct		vendor_fns vs;
};

struct ide_pci_softc {
	LIST_HEAD(, ide_pci_cookie) cookies;
};

static int
generic_dmainit(struct	ide_pci_cookie *cookie, 
	struct	wdparams *wp, 
	int	(*wdcmd)(int, void *),
	void	*wdinfo);

static void
generic_status(struct ide_pci_cookie *cookie);

static void
via_571_status(struct ide_pci_cookie *cookie);

static int
via_571_dmainit(struct	ide_pci_cookie *cookie, 
	struct	wdparams *wp, 
	int	(*wdcmd)(int, void *),
	void	*wdinfo);

static void
intel_piix_dump_drive(char	*ctlr,
	int	sitre,
	int	is_piix4,
	int	word40,
	int	word44,
	int	word48,
	int	word4a,
	int	drive);

static void
intel_piix_status(struct ide_pci_cookie *cookie);
static int
intel_piix_dmainit(struct	ide_pci_cookie *cookie, 
	struct	wdparams *wp, 
	int		(*wdcmd)(int, void *),
	void		*wdinfo);

static struct ide_pci_cookie *
mkcookie(int		iobase_wd, 
	 int		ctlr,
	 int		unit, 
	 int		iobase_bm, 
	 pcici_t	tag, 
	 pcidi_t	type, 
	 struct		vendor_fns *vp,
	 int		altiobase_wd);



static void ide_pci_attach(pcici_t tag, int unit);
static void *ide_pci_candma(int, int);
static int ide_pci_dmainit(void *, 
	struct wdparams *, 
	int (*)(int, void *),
	void *);

static int ide_pci_dmaverify(void *, char *, u_long, int);
static int ide_pci_dmasetup(void *, char *, u_long, int);
static void ide_pci_dmastart(void *);
static int ide_pci_dmadone(void *);
static int ide_pci_status(void *);
static int ide_pci_iobase(void *xcp);
static int ide_pci_altiobase(void *xcp);

static struct ide_pci_softc softc;

static int ide_pci_softc_cookies_initted = 0;

extern struct isa_driver wdcdriver;

/*
 * PRD_ALLOC_SIZE should be something that will not be allocated across a 64k
 * boundary.
 * PRD_MAX_SEGS is defined to be the maximum number of segments required for
 * a transfer on an IDE drive, for an xfer that is linear in virtual memory.
 * PRD_BUF_SIZE is the size of the buffer needed for a PRD table.
 */
#define	PRD_ALLOC_SIZE		PAGE_SIZE
#define PRD_MAX_SEGS		((256 * 512 / PAGE_SIZE) + 1)
#define PRD_BUF_SIZE		PRD_MAX_SEGS * 8

static void *prdbuf = 0;
static void *prdbuf_next = 0;

/* 
 * Hardware specific IDE controller code.  All vendor-specific code
 * for handling IDE timing and other chipset peculiarities should be
 * encapsulated here.
 */

/* helper funcs */

/*
 * nnn_mode() return the highest valid mode, or -1 if the mode class is
 * not supported
 */

static __inline int
pio_mode(struct wdparams *wp)
{
	if ((wp->wdp_atavalid & 2) == 2) {
		if ((wp->wdp_eidepiomodes & 2) == 2) return 4;
		if ((wp->wdp_eidepiomodes & 1) == 1) return 3;
	}
	return -1;
}

#if 0
static __inline int
dma_mode(struct wdparams *wp)
{
	/* XXX not quite sure how to verify validity on this field */
}
#endif

static __inline int
mwdma_mode(struct wdparams *wp)
{
	/* 
	 * XXX technically, using wdp_atavalid to test for validity of
	 * this field is not quite correct
	 */
	if ((wp->wdp_atavalid & 2) == 2) {
		if ((wp->wdp_dmamword & 4) == 4) return 2;
		if ((wp->wdp_dmamword & 2) == 2) return 1;
		if ((wp->wdp_dmamword & 1) == 1) return 0;
	}
	return -1;
}

static __inline int
udma_mode(struct wdparams *wp)
{
	if ((wp->wdp_atavalid & 4) == 4) {
		if ((wp->wdp_udmamode & 4) == 4) return 2;
		if ((wp->wdp_udmamode & 2) == 2) return 1;
		if ((wp->wdp_udmamode & 1) == 1) return 0;
	}
	return -1;
}


/* Generic busmastering PCI-IDE */

static int
generic_dmainit(struct ide_pci_cookie *cookie, 
		struct wdparams *wp, 
		int(*wdcmd)(int, void *),
		void *wdinfo)
{
	/*
	 * punt on the whole timing issue by looking for either a
	 * drive programmed for both PIO4 and mDMA2 (which use similar
	 * timing) or a drive in an UltraDMA mode (hopefully all
	 * controllers have separate timing for UDMA).  one hopes that if
	 * the drive's DMA mode has been configured by the BIOS, the
	 * controller's has also.
	 *
	 * XXX there are examples where this approach is now known to be
	 * broken, at least on systems based on Intel chipsets.
	 */

	if ((pio_mode(wp) >= 4 && mwdma_mode(wp) >= 2) || 
	    (udma_mode(wp) >= 2)) {
		printf("ide_pci: generic_dmainit %04x:%d: warning, IDE controller timing not set\n",
			cookie->iobase_wd,
			cookie->unit);
		return 1;
	}
#ifdef IDE_PCI_DEBUG
	printf("pio_mode: %d, mwdma_mode(wp): %d, udma_mode(wp): %d\n",
		pio_mode(wp), mwdma_mode(wp), udma_mode(wp));
#endif
	return 0;
}

static void
generic_status(struct ide_pci_cookie *cookie)
{
	printf("generic_status: no PCI IDE timing info available\n");
}

static struct vendor_fns vs_generic = 
{ 
	generic_dmainit, 
	generic_status
};

/* VIA Technologies "82C571" PCI-IDE controller core */

static void
via_571_status(struct ide_pci_cookie *cookie)
{
	int iobase_wd;
	int ctlr, unit;
	int iobase_bm;
	pcici_t tag;
	pcidi_t type;
	u_long word40[5];
	int i, unitno;

	iobase_wd = cookie->iobase_wd;
	unit = cookie->unit;
	ctlr = cookie->ctlr;
	iobase_bm = cookie->iobase_bm;
	tag = cookie->tag;
	type = cookie->type;

	unitno = ctlr * 2 + unit;

	for (i=0; i<5; i++) {
		word40[i] = pci_conf_read(tag, i * 4 + 0x40);
	}

	if (ctlr == 0)
		printf("via_571_status: Primary IDE prefetch/postwrite %s/%s\n",
		       word40[0] & 0x8000 ? "enabled" : "disabled",
		       word40[0] & 0x4000 ? "enabled" : "disabled");
	else
		printf("via_571_status: Secondary IDE prefetch/postwrite %s/%s\n",
		       word40[0] & 0x2000 ? "enabled" : "disabled",
		       word40[0] & 0x1000 ? "enabled" : "disabled");

	printf("via_571_status: busmaster status read retry %s\n",
	       (word40[1] & 0x08) ? "enabled" : "disabled");

    
	printf("via_571_status: %s drive %d data setup=%d active=%d recovery=%d\n",
	       unitno < 2 ? "primary" : "secondary", 
	       unitno & 1,
	       ((u_int)(word40[3] >> ((3 - unitno) * 2)) & 3) + 1,
	       ((u_int)(word40[2] >> (((3 - unitno) * 8) + 4)) & 0x0f) + 1,
	       ((u_int)(word40[2] >> ((3 - unitno) * 8)) & 0x0f) + 1);
    
	if (ctlr == 0)
		printf("via_571_status: primary ctrl active=%d recovery=%d\n",
		       ((u_int)(word40[3] >> 28) & 0x0f) + 1,
		       ((u_int)(word40[2] >> 24) & 0x0f) + 1);
	else
		printf("via_571_status: secondary ctrl active=%d recovery=%d\n",
		       ((u_int)(word40[3] >> 20) & 0x0f) + 1,
		       ((u_int)(word40[2] >> 16) & 0x0f) + 1);

	/* UltraDMA dump */
	{
		int foo;

		foo = word40[4] >> ((3 - unitno) * 8);
		printf("via_571_status: %s drive %d udma method=%d enable=%d PIOmode=%d cycle=%d\n",
		       i < 2 ? "primary" : "secondary", 
		       i & 1,
		       (foo >> 7) & 1,
		       (foo >> 6) & 1,
		       (foo >> 5) & 1,
		       (foo & 3) + 2);
	}
}

/*
 * XXX timing values set here are only good for 30/33MHz buses; should deal
 * with slower ones too (BTW: you overclock-- you lose)
 */

static int
via_571_dmainit(struct ide_pci_cookie *cookie, 
		struct wdparams *wp, 
		int(*wdcmd)(int, void *),
		void *wdinfo)
{
	int r;
	u_long pci_revision;
	int unitno;

	pci_revision = pci_conf_read(cookie->tag, PCI_CLASS_REG) & 
		PCI_REVISION_MASK;

	unitno = cookie->ctlr * 2 + cookie->unit;

	/* If it's a UDMA drive on a '590, set it up */
	/* 
	 * XXX the revision number we check for is of dubious validity.
	 * it's extracted from the AMD 645 datasheet.
	 */
	if (pci_revision >= 1 && udma_mode(wp) >= 2) {
		unsigned int word50, mask, new;
		word50 = pci_conf_read(cookie->tag, 0x50);

		/* UDMA enable by SET FEATURES, DMA cycles, cycle time 2T */
		mask = 0xe3000000 >> (unitno * 8);
		new = 0x80000000 >> (unitno * 8);

		word50 &= ~mask;
		word50 |= new;

		pci_conf_write(cookie->tag, 0x50, word50);

		/*
		 * With the '590, drive configuration should come *after* the
		 * controller configuration, to make sure the controller sees 
		 * the SET FEATURES command and does the right thing.
		 */
		/* Set UDMA mode 2 on drive */
		if (bootverbose)
			printf("intel_piix_dmainit: setting ultra DMA mode 2\n");
		r = wdcmd(WDDMA_UDMA2, wdinfo);
		if (!r) {
			printf("intel_piix_dmainit: setting DMA mode failed\n");
			return 0;
		}

		if (bootverbose)
			via_571_status(cookie);
		return 1;

	}

	/* otherwise, try and program it for MW DMA mode 2 */
	else if (mwdma_mode(wp) >= 2 && pio_mode(wp) >= 4) {
		u_long workword;

		/* Set multiword DMA mode 2 on drive */
		if (bootverbose)
			printf("intel_piix_dmainit: setting multiword DMA mode 2\n");
		r = wdcmd(WDDMA_MDMA2, wdinfo);
		if (!r) {
			printf("intel_piix_dmainit: setting DMA mode failed\n");
			return 0;
		}

		/* Configure the controller appropriately for MWDMA mode 2 */

		workword = pci_conf_read(cookie->tag, 0x40);

		/* 
		 * enable prefetch/postwrite-- XXX may cause problems
		 * with CD-ROMs? 
		 */
		workword &= ~(3 << (cookie->ctlr * 2 + 12));
		workword |= 3 << (cookie->ctlr * 2 + 12);

		/* FIFO configurations-- equal split, threshold 1/2 */
		workword &= 0x90ffffff;
		workword |= 0x2a000000;

		pci_conf_write(cookie->tag, 0x40, workword);

		workword = pci_conf_read(cookie->tag, 0x44);

		/* enable status read retry */
		workword |= 8;

		/* enable FIFO flush on interrupt and end of sector */
		workword &= 0xff0cffff;
		workword |= 0x00f00000;
		pci_conf_write(cookie->tag, 0x44, workword);

		workword = pci_conf_read(cookie->tag, 0x48);
		/* set Mode2 timing */
		workword &= ~(0xff000000 >> (unitno * 8));
		workword |= 0x31000000 >> (unitno * 8);
		pci_conf_write(cookie->tag, 0x48, workword);

		/* set sector size */
		pci_conf_write(cookie->tag, cookie->ctlr ? 0x68 : 0x60, 0x200);

		if (bootverbose)
			via_571_status(cookie);

		return 1;

	}
	return 0;
}


static struct vendor_fns vs_via_571 = 
{ 
	via_571_dmainit, 
	via_571_status
};


static void
promise_status(struct ide_pci_cookie *cookie)
{
    pcici_t tag;
    int i;
    u_int32_t port0_command, port0_altstatus;
    u_int32_t port1_command, port1_altstatus;
    u_int32_t dma_block;

    u_int32_t lat_and_interrupt;
    u_int32_t drivetiming;
    int pa, pb, mb, mc;

    tag = cookie->tag;
    port0_command = pci_conf_read(tag, 0x10);
    port0_altstatus = pci_conf_read(tag, 0x14);
    port1_command = pci_conf_read(tag, 0x18);
    port1_altstatus = pci_conf_read(tag, 0x1c);

    dma_block = pci_conf_read(tag, 0x20);
    lat_and_interrupt = pci_conf_read(tag, 0x3c);

    printf("promise_status: port0: 0x%lx, port0_alt: 0x%lx, port1: 0x%lx, port1_alt: 0x%lx\n",
	(u_long)port0_command, (u_long)port0_altstatus, (u_long)port1_command,
	(u_long)port1_altstatus);
    printf(
	"promise_status: dma control blk address: 0x%lx, int: %d, irq: %d\n",
	(u_long)dma_block, (u_int)(lat_and_interrupt >> 8) & 0xff,
	(u_int)lat_and_interrupt & 0xff);

    for(i=0;i<4;i+=2) {
		drivetiming = pci_conf_read(tag, 0x60 + i * 4);
		printf("drivebits%d-%d: %b\n", i, i+1, drivetiming,
			"\020\05Prefetch\06Iordy\07Errdy\010Sync\025DmaW\026DmaR");
	pa = drivetiming & 0xf;
	pb = (drivetiming >> 8) & 0x1f;
	mb = (drivetiming >> 13) & 0x7;
	mc = (drivetiming >> 16) & 0xf;
	printf("drivetiming%d: pa: 0x%x, pb: 0x%x, mb: 0x%x, mc: 0x%x\n",
		i, pa, pb, mb, mc);

	drivetiming = pci_conf_read(tag, 0x60 + (i + 1) * 4);
	pa = drivetiming & 0xf;
	pb = (drivetiming >> 8) & 0x1f;
	mb = (drivetiming >> 13) & 0x7;
	mc = (drivetiming >> 16) & 0xf;
	printf("drivetiming%d: pa: 0x%x, pb: 0x%x, mb: 0x%x, mc: 0x%x\n",
		i + 1, pa, pb, mb, mc);
    }
}

static struct vendor_fns vs_promise = 
{ 
    generic_dmainit, 
    promise_status
};

/* Intel PIIX, PIIX3, and PIIX4 IDE controller subfunctions */
static void
intel_piix_dump_drive(char *ctlr,
		      int sitre,
		      int is_piix4,
		      int word40,
		      int word44,
		      int word48,
		      int word4a,
		      int drive)
{
	char *ms;

	if (!sitre)
		ms = "master/slave";
	else if (drive == 0)
		ms = "master";
	else
		ms = "slave";

	printf("intel_piix_status: %s %s sample = %d, %s recovery = %d\n", 
	       ctlr, 
	       ms, 
	       5 - ((sitre && drive) ?
		    ((word44 >> 2) & 3) :
		    ((word40 >> 12) & 3)),
	       ms,
	       4 - ((sitre && drive) ?
		    ((word44 >> 0) & 3) :
		    ((word40 >> 8) & 3)));

	word40 >>= (drive * 4);
	printf("intel_piix_status: %s %s fastDMAonly %s, pre/post %s,\n\
intel_piix_status:  IORDY sampling %s,\n\
intel_piix_status:  fast PIO %s%s\n", 
	       ctlr,
	       (drive == 0) ? "master" : "slave",
	       (word40 & 8) ? "enabled" : "disabled",
	       (word40 & 4) ? "enabled" : "disabled",
	       (word40 & 2) ? "enabled" : "disabled",
	       (word40 & 1) ? "enabled" : "disabled",
	       ((word40 & 9) == 9) ? " (overridden by fastDMAonly)" : "" );

	if (is_piix4)
		printf("intel_piix_status: UltraDMA %s, CT/RP = %d/%d\n",
		       word48 ? "enabled": "disabled",
		       4 - (word4a & 3),
		       6 - (word4a & 3));
}

static void
intel_piix_status(struct ide_pci_cookie *cookie)
{
	int iobase_wd;
	int unit;
	int iobase_bm;
	pcici_t tag;
	pcidi_t type;
	int ctlr;
	u_long word40, word44, word48;
	int sitre, is_piix4;

	iobase_wd = cookie->iobase_wd;
	unit = cookie->unit;
	iobase_bm = cookie->iobase_bm;
	tag = cookie->tag;
	type = cookie->type;
	ctlr = cookie->ctlr;

	word40 = pci_conf_read(tag, 0x40);
	word44 = pci_conf_read(tag, 0x44);
	word48 = pci_conf_read(tag, 0x48);

	/* 
	 * XXX will not be right for the *next* generation of upward-compatible
	 * intel IDE controllers...
	 */
	is_piix4 = pci_conf_read(tag, PCI_CLASS_REG) == 0x71118086;

	sitre = word40 & 0x4000;

	switch (ctlr * 2 + unit) {
	case 0:
		intel_piix_dump_drive("primary", 
				      sitre, 
				      is_piix4,
				      word40 & 0xffff, 
				      word44 & 0x0f, 
				      word48,
				      word48 >> 16,
				      0);
		break;
	case 1:
		intel_piix_dump_drive("primary", 
				      sitre, 
				      is_piix4,
				      word40 & 0xffff, 
				      word44 & 0x0f, 
				      word48 >> 1,
				      word48 >> 20,
				      1);
		break;
	case 2:
		intel_piix_dump_drive("secondary", 
				      sitre, 
				      is_piix4,
				      (word40 >> 16) & 0xffff, 
				      (word44 >> 4) & 0x0f,
				      word48 >> 2,
				      word48 >> 24,
				      0);
		break;
	case 3:
		intel_piix_dump_drive("secondary", 
				      sitre, 
				      is_piix4,
				      (word40 >> 16) & 0xffff, 
				      (word44 >> 4) & 0x0f,
				      word48 >> 3,
				      word48 >> 28,
				      1);
		break;
	default:
		printf("intel_piix_status: bad drive or controller number\n");
	}
}

/*
 * XXX timing values set hereare only good for 30/33MHz buses; should deal
 * with slower ones too (BTW: you overclock-- you lose)
 */

static int
intel_piix_dmainit(struct ide_pci_cookie *cookie, 
		   struct wdparams *wp, 
		   int(*wdcmd)(int, void *),
		   void *wdinfo)
{
	int r;

	/* If it's a UDMA drive and a PIIX4, set it up */
	if (cookie->type == 0x71118086 && udma_mode(wp) >= 2) {
		/* Set UDMA mode 2 on controller */
		int unitno, mask, new;

		if (bootverbose)
			printf("intel_piix_dmainit: setting ultra DMA mode 2\n");

		r = wdcmd(WDDMA_UDMA2, wdinfo);

		if (!r) {
			printf("intel_piix_dmainit: setting DMA mode failed\n");
			return 0;
		}

		unitno = cookie->ctlr * 2 + cookie->unit;

		mask = 1 << unitno + 3 << (16 + unitno * 4);
		new = 1 << unitno + 2 << (16 + unitno * 4);

		pci_conf_write(cookie->tag, 0x48, 
			(pci_conf_read(cookie->tag, 0x48) & ~mask) | new);

		if (bootverbose)
			intel_piix_status(cookie);
		return 1;
	}
	/* 
	 * if it's an 82371FB, which can't do independent programming of
	 * drive timing, we punt; we're not going to fuss with trying to
	 * coordinate timing modes between drives.  if this is you, get a
	 * new motherboard.  or contribute patches :)
	 *
	 * we do now at least see if the modes set are OK to use.  this should
	 * satisfy the majority of people, with mwdma mode2 drives.
	 */
	else if (cookie->type == 0x12308086)
	{
		u_long word40;

		/* can drive do PIO 4 and MW DMA 2? */
		if (!(mwdma_mode(wp) >= 2 && pio_mode(wp) >= 4)) 
			return 0;

		word40 = pci_conf_read(cookie->tag, 0x40);
		word40 >>= cookie->ctlr * 16;

		/* Check for timing config usable for DMA on controller */
		if (!((word40 & 0x3300) == 0x2300 && 
		      ((word40 >> (cookie->unit * 4)) & 1) == 1))
			return 0;

		/* Set multiword DMA mode 2 on drive */
		if (bootverbose)
			printf("intel_piix_dmainit: setting multiword DMA mode 2\n");
		r = wdcmd(WDDMA_MDMA2, wdinfo);
		if (!r) {
			printf("intel_piix_dmainit: setting DMA mode failed\n");
			return 0;
		}
		return 1;
	}

	/* otherwise, treat it as a PIIX3 and program it for MW DMA mode 2 */
	else if (mwdma_mode(wp) >= 2 && pio_mode(wp) >= 4) {
		u_long mask40, mask44, new40, new44;

		/* 
		 * If SITRE is not set, set it and copy the
		 * appropriate bits into the secondary registers.  Do
		 * both controllers at once.
		 */
		if (((pci_conf_read(cookie->tag, 0x40) >> (16 * cookie->ctlr)) 
		     & 0x4000) == 0) {
			unsigned int word40, word44;

			word40 = pci_conf_read(cookie->tag, 0x40);

			/* copy bits to secondary register */
			word44 = pci_conf_read(cookie->tag, 0x44);
			/*
			 * I've got a Biostar motherboard with Award
			 * BIOS that sets SITRE and secondary timing
			 * on one controller but not the other.
			 * Bizarre.
			 */
			if ((word40 & 0x4000) == 0) {
				word44 &= ~0xf;
				word44 |= ((word40 & 0x3000) >> 10) | 
					((word40 & 0x0300) >> 8);
			}
			if ((word40 & 0x40000000) == 0) {
				word44 &= ~0xf0;
				word44 |= ((word40 & 0x30000000) >> 22) | 
					((word40 & 0x03000000) >> 20);
			}
			/* set SITRE */
			word40 |= 0x40004000;

			pci_conf_write(cookie->tag, 0x40, word40);
			pci_conf_write(cookie->tag, 0x44, word44);
		}

		/* Set multiword DMA mode 2 on drive */
		if (bootverbose)
			printf("intel_piix_dmainit: setting multiword DMA mode 2\n");

		r = wdcmd(WDDMA_MDMA2, wdinfo);

		if (!r) {
			printf("intel_piix_dmainit: setting DMA mode failed\n");
			return 0;
		}

		/* 
		 * backward compatible hardware leaves us with such
		 * twisted masses of software (aka twiddle the
		 * extremely weird register layout on a PIIX3, setting
		 * PIO mode 4 and MWDMA mode 2)
		 */
		if (cookie->unit == 0) {
			mask40 = 0x330f;
			new40 = 0x2307;
			mask44 = 0;
			new44 = 0;
		} else {
			mask40 = 0x00f0;
			new40 = 0x0070;
			mask44 = 0x000f;
			new44 = 0x000b;
		}

		if (cookie->ctlr) {
			mask40 <<= 16;
			new40 <<= 16;
			mask44 <<= 4;
			new44 <<= 4;
		}

		pci_conf_write(cookie->tag, 0x40, 
			       (pci_conf_read(cookie->tag, 0x40) & ~mask40) | new40);
		pci_conf_write(cookie->tag, 0x44, 
			       (pci_conf_read(cookie->tag, 0x44) & ~mask44) | new44);

		if (bootverbose)
			intel_piix_status(cookie);
		return 1;
	}
	return 0;
}

static struct vendor_fns vs_intel_piix = 
{ 
	intel_piix_dmainit, 
	intel_piix_status
};

/* Generic SFF-8038i code-- all code below here, except for PCI probes,
 * more or less conforms to the SFF-8038i spec as extended for PCI.
 * There should be no code that goes beyond that feature set below.
 */

/* XXX mkcookie is overloaded with too many parameters */

static struct ide_pci_cookie *
mkcookie(int iobase_wd, 
	int ctlr,
	int unit, 
	int iobase_bm, 
	pcici_t tag, 
	pcidi_t type, 
	struct vendor_fns *vp,
	int altiobase_wd)
{
	struct ide_pci_cookie *cp;

	cp = malloc(sizeof *cp, M_DEVBUF, M_NOWAIT);
	if (!cp) return 0;

	cp->iobase_wd = iobase_wd;
	cp->ctlr = ctlr;
	cp->unit = unit;
	cp->tag = tag;
	cp->type = type;
	cp->iobase_bm = iobase_bm;
	cp->altiobase_wd = altiobase_wd;
	bcopy(vp, &cp->vs, sizeof(struct vendor_fns));

	if (!prdbuf) {
		prdbuf = malloc(PRD_ALLOC_SIZE, M_DEVBUF, M_NOWAIT);
		if (!prdbuf) {
			FREE(cp, M_DEVBUF);
			return 0;
		}
		if (((int)prdbuf >> PAGE_SHIFT) ^
		    (((int)prdbuf + PRD_ALLOC_SIZE - 1) >> PAGE_SHIFT)) {
			printf("ide_pci: prdbuf straddles page boundary, no DMA\n");
			FREE(cp, M_DEVBUF);
			FREE(prdbuf, M_DEVBUF);
			return 0;
		}

		prdbuf_next = prdbuf;
	}
	if (((char *)prdbuf_next + PRD_BUF_SIZE) > 
	    ((char *)prdbuf + PRD_ALLOC_SIZE)) {
		printf("ide_pci: mkcookie %04x:%d: no more space for PRDs, no DMA\n",
		       iobase_wd, unit);
		FREE(cp, M_DEVBUF);
		return 0;
	}

	cp->prd = prdbuf_next;
	(char *)prdbuf_next += PRD_BUF_SIZE;

	LIST_INSERT_HEAD(&softc.cookies, cp, le);
	return cp;
}

static char *
ide_pci_probe(pcici_t tag, pcidi_t type)
{
	u_long data;

	data = pci_conf_read(tag, PCI_CLASS_REG);

	if ((data & PCI_CLASS_MASK) == PCI_CLASS_MASS_STORAGE &&
	    ((data & PCI_SUBCLASS_MASK) == 0x00010000 ||
	    ((data & PCI_SUBCLASS_MASK) == 0x00040000))) {
		if (type == 0x71118086)
			return ("Intel PIIX4 Bus-master IDE controller");
		if (type == 0x70108086)
			return ("Intel PIIX3 Bus-master IDE controller");
		if (type == 0x12308086)
			return ("Intel PIIX Bus-master IDE controller");
		if (type == PROMISE_ULTRA33)
	    		return ("Promise Ultra/33 IDE controller");
		if (type == 0x05711106)
			return ("VIA 82C586x (Apollo) Bus-master IDE controller");
		if (data & 0x8000)
			return ("PCI IDE controller (busmaster capable)");
#ifndef CMD640
		/*
		 * XXX the CMD640B hack should be better integrated, or
		 * something.
		 */
		else
			return ("PCI IDE controller (not busmaster capable)");
#endif
	};
	return ((char*)0);
}

static void
ide_pci_attach(pcici_t tag, int unit)
{
	u_long class, cmd;
	int bmista_1, bmista_2;
	int iobase_wd_1, iobase_wd_2, iobase_bm_1, iobase_bm_2;
	int altiobase_wd_1, altiobase_wd_2;
	struct vendor_fns *vp;
	pcidi_t type;
	struct ide_pci_cookie *cookie;
	int ctlridx;

	ctlridx = unit * 2;

	/* set up vendor-specific stuff */
	type = pci_conf_read(tag, PCI_ID_REG);

	if (type != PROMISE_ULTRA33) {
	/* is it busmaster capable?  bail if not */
		class = pci_conf_read(tag, PCI_CLASS_REG);
		if (!(class & 0x8000)) {
			return;
		}

	/* is it enabled and is busmastering turned on? */
		cmd = pci_conf_read(tag, PCI_COMMAND_STATUS_REG);
		if ((cmd & 5) != 5) {
			return;
		}
	}

	switch (type) {
	case 0x71118086:
	case 0x70108086:
	case 0x12308086:
		/* Intel PIIX, PIIX3, PIIX4 */
		vp = &vs_intel_piix;
		break;

	case 0x5711106:
		/* VIA Apollo chipset family */
		vp = &vs_via_571;
		break;

	case PROMISE_ULTRA33:
		/* Promise controllers */
		vp = &vs_promise;
		break;

	default:
		/* everybody else */
		vp = &vs_generic;
		break;
	}

	if (type != PROMISE_ULTRA33) {
		if ((class & 0x100) == 0) {
			iobase_wd_1 = IO_WD1;
			altiobase_wd_1 = iobase_wd_1 + wd_altsts;
		} else {
			iobase_wd_1 = pci_conf_read(tag, 0x10) & 0xfffc;
			altiobase_wd_1 = pci_conf_read(tag, 0x14) & 0xfffc;
		}

		if ((class & 0x400) == 0) {
			iobase_wd_2 = IO_WD2;
			altiobase_wd_2 = iobase_wd_2 + wd_altsts;
		} else {
			iobase_wd_2 = pci_conf_read(tag, 0x18) & 0xfffc;
			altiobase_wd_2 = pci_conf_read(tag, 0x1c) & 0xfffc;
		}
	} else {
		iobase_wd_1 = pci_conf_read(tag, 0x10) & 0xfffc;
		altiobase_wd_1 = pci_conf_read(tag, 0x14) & 0xfffc;
		iobase_wd_2 = pci_conf_read(tag, 0x18) & 0xfffc;
		altiobase_wd_2 = pci_conf_read(tag, 0x1c) & 0xfffc;
	}

	iobase_bm_1 = pci_conf_read(tag, 0x20) & 0xfffc;
	iobase_bm_2 = iobase_bm_1 + SFF8038_CTLR_1;
	if (iobase_bm_1 == 0) {
		printf("ide_pci: BIOS has not configured busmaster I/O address,\n\
ide_pci:  giving up\n");
		return;
	}

	wddma[unit].wdd_candma = ide_pci_candma;
	wddma[unit].wdd_dmainit = ide_pci_dmainit;
	wddma[unit].wdd_dmaverify = ide_pci_dmaverify;
	wddma[unit].wdd_dmaprep = ide_pci_dmasetup;
	wddma[unit].wdd_dmastart = ide_pci_dmastart;
	wddma[unit].wdd_dmadone = ide_pci_dmadone;
	wddma[unit].wdd_dmastatus = ide_pci_status;
	wddma[unit].wdd_iobase = ide_pci_iobase;
	wddma[unit].wdd_altiobase = ide_pci_altiobase;

	/*
	 * This code below is mighty bogus.  The config entries for the
	 * isa_devtab_bio are plugged in before the standard ISA bios scan.
	 * This is our "hack" way to simulate a dynamic assignment of I/O
	 * addresses, from a PCI device to an ISA probe.  Sorry :-).
	 */
	if (iobase_wd_1 != IO_WD1) {
	    struct isa_device *dvp, *dvp1, *dvup;
	    for( dvp = isa_devtab_bio;
			dvp->id_id != 0;
			dvp++) {
				if ((dvp->id_driver == &wdcdriver) && (dvp->id_iobase == 0)) {
					int biotabunit;
					biotabunit = dvp->id_unit * 2;
					dvp->id_iobase = iobase_wd_1;
					dvp1 = dvp + 1;
					dvp1->id_iobase = iobase_wd_2;
					printf("ide_pci%d: adding drives to controller %d:",
						unit, biotabunit);
					for(dvup = isa_biotab_wdc;
						dvup->id_id != 0;
						dvup++) {
						if (dvup->id_driver != &wdcdriver)
							continue;
						if (dvup->id_unit != biotabunit)
							continue;

						dvup->id_iobase = dvp->id_iobase;
						printf(" %d", dvup->id_unit);
						dvup++;

						pci_map_int(tag, wdintr, (void *) dvp->id_unit, &bio_imask); 
						if (dvup->id_id == 0)
							break;

						if (dvup->id_unit == biotabunit + 1) {
							dvup->id_iobase = dvp->id_iobase;
							printf(" %d", dvup->id_unit);
							dvup++;
							if (dvup->id_id == 0) {
								iobase_wd_2 = 0;
								break;
			    			}
						}

						if (dvup->id_unit == biotabunit + 2) {
							pci_map_int(tag, wdintr, (void *) ((int) dvp->id_unit + 1), &bio_imask);
							dvup->id_iobase = dvp1->id_iobase;
							printf(" %d", dvup->id_unit);
							dvup++;
							if (dvup->id_id == 0) {
								break;
							}
						}

						if (dvup->id_unit == biotabunit + 3) {
							pci_map_int(tag, wdintr, (void *) ((int) dvp->id_unit + 1), &bio_imask);
							dvup->id_iobase = dvp1->id_iobase;
							printf(" %d", dvup->id_unit);
						}

						break;
		    		}
				    printf("\n");
				    break;
				}
	    }
	}


	bmista_1 = inb(iobase_bm_1 + BMISTA_PORT);
	bmista_2 = inb(iobase_bm_2 + BMISTA_PORT);
		
	if (!ide_pci_softc_cookies_initted) {
		LIST_INIT(&softc.cookies);
		ide_pci_softc_cookies_initted = 1;
	}

	if (iobase_wd_1 != 0) {
		cookie = mkcookie(iobase_wd_1,
				  ctlridx, 
				  0, 
				  iobase_bm_1, 
				  tag, 
				  type, 
				  vp,
				  altiobase_wd_1);
		if (bootverbose)
			vp->vendor_status(cookie);
		cookie = mkcookie(iobase_wd_1,
				  ctlridx,
				  1,
				  iobase_bm_1, 
				  tag,
				  type,
				  vp,
				  altiobase_wd_1);
		if (bootverbose) {
			vp->vendor_status(cookie);

			printf("ide_pci: busmaster 0 status: %02x from port: %08x\n", 
			       bmista_1, iobase_bm_1+BMISTA_PORT);

			if (bmista_1 & BMISTA_DMA0CAP)
				printf("ide_pci: ide0:0 has been configured for DMA by BIOS\n");
			if (bmista_1 & BMISTA_DMA1CAP)
				printf("ide_pci: ide0:1 has been configured for DMA by BIOS\n");
		}
	}

	if (bmista_1 & BMISTA_SIMPLEX || bmista_2 & BMISTA_SIMPLEX) {
		printf("ide_pci: controller is simplex, no DMA on secondary channel\n");
	} else if (iobase_wd_2 != 0) {
		cookie = mkcookie(iobase_wd_2,
				  ctlridx + 1,
				  0, 
				  iobase_bm_2,
				  tag,
				  type, 
				  vp,
				  altiobase_wd_2);
		if (bootverbose)
			vp->vendor_status(cookie);
		cookie = mkcookie(iobase_wd_2,
				  ctlridx + 1,
				  1, 
				  iobase_bm_2, 
				  tag, 
				  type,
				  vp,
				  altiobase_wd_2);
		if (bootverbose) {
			vp->vendor_status(cookie);

			printf("ide_pci: busmaster 1 status: %02x from port: %08x\n",
			       bmista_2, iobase_bm_2+BMISTA_PORT);

			if (bmista_2 & BMISTA_DMA0CAP)
				printf("ide_pci: ide1:0 has been configured for DMA by BIOS\n");
			if (bmista_2 & BMISTA_DMA1CAP)
				printf("ide_pci: ide1:1 has been configured for DMA by BIOS\n");
		}
	}
}

static u_long ide_pci_count;

static struct pci_device ide_pci_device = {
	"ide_pci",
	ide_pci_probe,
	ide_pci_attach,
	&ide_pci_count,
	0
};

DATA_SET(pcidevice_set, ide_pci_device);

/*
 * Return a cookie if we can do DMA on the specified (iobase_wd, unit).
 */
static void *
ide_pci_candma(int iobase_wd, int unit)
{
	struct ide_pci_cookie *cp;

	cp = softc.cookies.lh_first;
	while(cp) {
		if (cp->ctlr == unit &&
			((iobase_wd == 0) || (cp->iobase_wd == iobase_wd)))
			break;
		cp = cp->le.le_next;
	}

	return cp;
}

/*
 * Initialize controller and drive for DMA operation, including timing modes.
 * Uses data passed from the wd driver and a callback function to initialize
 * timing modes on the drive.
 */
static int
ide_pci_dmainit(void *cookie,
		struct wdparams *wp,
		int(*wdcmd)(int, void *),
		void *wdinfo)
{
	struct ide_pci_cookie *cp = cookie;
	/* 
	 * If the controller status indicates that DMA is configured already,
	 * we flounce happily away
	 */
	if (inb(cp->iobase_bm + BMISTA_PORT) & 
	    ((cp->unit == 0) ? BMISTA_DMA0CAP : BMISTA_DMA1CAP))
		return 1;
    
	/* We take a stab at it with device-dependent code */
	return(cp->vs.vendor_dmainit(cp, wp, wdcmd, wdinfo));
}

/*
 * Verify that controller can handle a dma request for cp.  Should
 * not affect any hardware or driver state.
 */
static int
ide_pci_dmaverify(void *xcp, char *vaddr, u_long count, int dir)
{
	int badfu;

	/*
	 * check for nonaligned or odd-length Stuff
	 */
	badfu = ((unsigned int)vaddr & 1) || (count & 1);
#ifdef DIAGNOSTIC
	if (badfu) {
		printf("ide_pci: dmaverify odd vaddr or length, ");
		printf("vaddr = %p length = %08lx\n", (void *)vaddr, count);
	}
#endif
	return (!badfu);
}

/*
 * Set up DMA for cp.  It is the responsibility of the caller
 * to ensure that the controller is idle before this routine
 * is called.
 */
static int
ide_pci_dmasetup(void *xcp, char *vaddr, u_long vcount, int dir)
{
	struct ide_pci_cookie *cp = xcp;
	struct ide_pci_prd *prd;
	int i;
	u_long firstpage;
	u_long prd_base, prd_count;
	u_long nbase, ncount, nend;
	int iobase_bm;
	u_long count, checkcount;

	prd = cp->prd;

	count = vcount;

	i = 0;

	iobase_bm = cp->iobase_bm;

	if (count == 0) {
		printf("ide_pci: dmasetup 0-length transfer, ");
		printf("vaddr = %p length = %08lx\n", (void *)vaddr, count);
		return 1;
	}

	/* Generate first PRD entry, which may be non-aligned. */

	firstpage = PAGE_SIZE - ((uintptr_t)vaddr & PAGE_MASK);

	prd_base = vtophys(vaddr);
	prd_count = MIN(count,  firstpage);

	vaddr += prd_count;
	count -= prd_count;

	/* Step through virtual pages, coalescing as needed. */
	while (count) {
		nbase = vtophys(vaddr);
		ncount = MIN(count, PAGE_SIZE);
		nend = nbase + ncount;

		/* 
		 * Coalesce if physically contiguous and not crossing
		 * 64k boundary. 
		 */
		if ((prd_base + prd_count == nbase) && 
		    ((((nend - 1) ^ prd_base) & ~0xffff) == 0)) {
			prd_count += ncount;
		} else {
			prd[i].prd_base = prd_base;
			prd[i].prd_count = (prd_count & 0xffff);
			i++;
			if (i >= PRD_MAX_SEGS) {
				printf("wd82371: too many segments in PRD table\n");
				return 1;
			}
			prd_base = nbase;
			prd_count = ncount;
		}
		vaddr += ncount;
		count -= ncount;
	}

	/* Write last PRD entry. */
	prd[i].prd_base = prd_base;
	prd[i].prd_count = (prd_count & 0xffff) | PRD_EOT_BIT;

#ifdef DIAGNOSTIC
	/* sanity check the transfer for length and page-alignment, at least */
	checkcount = 0;
	for (i = 0;; i++) {
		unsigned int modcount;

		modcount = prd[i].prd_count & 0xffffe;
		if (modcount == 0) modcount = 0x10000;
		checkcount += modcount;
		if (i != 0 && ((prd[i].prd_base & PAGE_MASK) != 0)) {
			printf("ide_pci: dmasetup() diagnostic fails-- unaligned page\n");
			return 1;
		}
		if (prd[i].prd_count & PRD_EOT_BIT)
			break;
	}

	if (checkcount != vcount) {
		printf("ide_pci: dmasetup() diagnostic fails-- bad length\n");
		return 1;
	}
#endif

	/* Set up PRD base register */
	outl(iobase_bm + BMIDTP_PORT, vtophys(prd));

	/* Set direction of transfer */
	outb(iobase_bm + BMICOM_PORT, (dir == B_READ) ? BMICOM_READ_WRITE : 0);

	/* Clear interrupt and error bits */
	outb(iobase_bm + BMISTA_PORT,
	     (inb(iobase_bm + BMISTA_PORT) 
	      | (BMISTA_INTERRUPT | BMISTA_DMA_ERROR)));

	return 0;
}		

static void
ide_pci_dmastart(void *xcp)
{
	struct ide_pci_cookie *cp = xcp;
	int iobase_bm;

	iobase_bm = cp->iobase_bm;

	outb(iobase_bm + BMICOM_PORT,
	     inb(iobase_bm + BMICOM_PORT) | BMICOM_STOP_START);

}

static int
ide_pci_dmadone(void *xcp)
{
	struct ide_pci_cookie *cp = xcp;
	int iobase_bm, status;

	status = ide_pci_status(xcp);
	iobase_bm = cp->iobase_bm;

	outb(iobase_bm + BMICOM_PORT,
	     inb(iobase_bm + BMICOM_PORT) & ~BMICOM_STOP_START);

	return status;
}

static int
ide_pci_status(void *xcp)
{
	struct ide_pci_cookie *cp = xcp;
	int iobase_bm, status, bmista;

	status = 0;
	iobase_bm = cp->iobase_bm;

	bmista = inb(iobase_bm + BMISTA_PORT);

	if (bmista & BMISTA_INTERRUPT)
		status |= WDDS_INTERRUPT;
	if (bmista & BMISTA_DMA_ERROR)
		status |= WDDS_ERROR;
	if (bmista & BMISTA_DMA_ACTIVE)
		status |= WDDS_ACTIVE;

	return status;
}

static int
ide_pci_altiobase(void *xcp)
{
	struct ide_pci_cookie *cp = xcp;
	if (cp == 0) {
		return 0;
	} else {
		return cp->altiobase_wd;
	}
}

static int
ide_pci_iobase(void *xcp)
{
	struct ide_pci_cookie *cp = xcp;
	if (cp == 0) {
		return 0;
	} else {
		return cp->iobase_wd;
	}
}

#endif
#endif /* NPCI > 0 */

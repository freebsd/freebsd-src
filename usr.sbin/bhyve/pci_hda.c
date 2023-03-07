/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2016 Alex Teaca <iateaca@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <time.h>

#include "pci_hda.h"
#include "bhyverun.h"
#include "config.h"
#include "pci_emul.h"
#include "hdac_reg.h"

/*
 * HDA defines
 */
#define PCIR_HDCTL		0x40
#define INTEL_VENDORID		0x8086
#define HDA_INTEL_82801G	0x27d8

#define HDA_IOSS_NO		0x08
#define HDA_OSS_NO		0x04
#define HDA_ISS_NO		0x04
#define HDA_CODEC_MAX		0x0f
#define HDA_LAST_OFFSET						\
	(0x2084 + ((HDA_ISS_NO) * 0x20) + ((HDA_OSS_NO) * 0x20))
#define HDA_CORB_ENTRY_LEN	0x04
#define HDA_RIRB_ENTRY_LEN	0x08
#define HDA_BDL_ENTRY_LEN	0x10
#define HDA_DMA_PIB_ENTRY_LEN	0x08
#define HDA_STREAM_TAGS_CNT	0x10
#define HDA_STREAM_REGS_BASE	0x80
#define HDA_STREAM_REGS_LEN	0x20

#define HDA_DMA_ACCESS_LEN	(sizeof(uint32_t))
#define HDA_BDL_MAX_LEN		0x0100

#define HDAC_SDSTS_FIFORDY	(1 << 5)

#define HDA_RIRBSTS_IRQ_MASK	(HDAC_RIRBSTS_RINTFL | HDAC_RIRBSTS_RIRBOIS)
#define HDA_STATESTS_IRQ_MASK	((1 << HDA_CODEC_MAX) - 1)
#define HDA_SDSTS_IRQ_MASK					\
	(HDAC_SDSTS_DESE | HDAC_SDSTS_FIFOE | HDAC_SDSTS_BCIS)

/*
 * HDA data structures
 */

struct hda_softc;

typedef void (*hda_set_reg_handler)(struct hda_softc *sc, uint32_t offset,
		uint32_t old);

struct hda_bdle {
	uint32_t addrl;
	uint32_t addrh;
	uint32_t len;
	uint32_t ioc;
} __packed;

struct hda_bdle_desc {
	void *addr;
	uint8_t ioc;
	uint32_t len;
};

struct hda_codec_cmd_ctl {
	const char *name;
	void *dma_vaddr;
	uint8_t run;
	uint16_t rp;
	uint16_t size;
	uint16_t wp;
};

struct hda_stream_desc {
	uint8_t dir;
	uint8_t run;
	uint8_t stream;

	/* bp is the no. of bytes transferred in the current bdle */
	uint32_t bp;
	/* be is the no. of bdles transferred in the bdl */
	uint32_t be;

	uint32_t bdl_cnt;
	struct hda_bdle_desc bdl[HDA_BDL_MAX_LEN];
};

struct hda_softc {
	struct pci_devinst *pci_dev;
	uint32_t regs[HDA_LAST_OFFSET];

	uint8_t lintr;
	uint8_t rirb_cnt;
	uint64_t wall_clock_start;

	struct hda_codec_cmd_ctl corb;
	struct hda_codec_cmd_ctl rirb;

	uint8_t codecs_no;
	struct hda_codec_inst *codecs[HDA_CODEC_MAX];

	/* Base Address of the DMA Position Buffer */
	void *dma_pib_vaddr;

	struct hda_stream_desc streams[HDA_IOSS_NO];
	/* 2 tables for output and input */
	uint8_t stream_map[2][HDA_STREAM_TAGS_CNT];
};

/*
 * HDA module function declarations
 */
static inline void hda_set_reg_by_offset(struct hda_softc *sc, uint32_t offset,
    uint32_t value);
static inline uint32_t hda_get_reg_by_offset(struct hda_softc *sc,
    uint32_t offset);
static inline void hda_set_field_by_offset(struct hda_softc *sc,
    uint32_t offset, uint32_t mask, uint32_t value);

static struct hda_softc *hda_init(nvlist_t *nvl);
static void hda_update_intr(struct hda_softc *sc);
static void hda_response_interrupt(struct hda_softc *sc);
static int hda_codec_constructor(struct hda_softc *sc,
    struct hda_codec_class *codec, const char *play, const char *rec);
static struct hda_codec_class *hda_find_codec_class(const char *name);

static int hda_send_command(struct hda_softc *sc, uint32_t verb);
static int hda_notify_codecs(struct hda_softc *sc, uint8_t run,
    uint8_t stream, uint8_t dir);
static void hda_reset(struct hda_softc *sc);
static void hda_reset_regs(struct hda_softc *sc);
static void hda_stream_reset(struct hda_softc *sc, uint8_t stream_ind);
static int hda_stream_start(struct hda_softc *sc, uint8_t stream_ind);
static int hda_stream_stop(struct hda_softc *sc, uint8_t stream_ind);
static uint32_t hda_read(struct hda_softc *sc, uint32_t offset);
static int hda_write(struct hda_softc *sc, uint32_t offset, uint8_t size,
    uint32_t value);

static inline void hda_print_cmd_ctl_data(struct hda_codec_cmd_ctl *p);
static int hda_corb_start(struct hda_softc *sc);
static int hda_corb_run(struct hda_softc *sc);
static int hda_rirb_start(struct hda_softc *sc);

static void *hda_dma_get_vaddr(struct hda_softc *sc, uint64_t dma_paddr,
    size_t len);
static void hda_dma_st_dword(void *dma_vaddr, uint32_t data);
static uint32_t hda_dma_ld_dword(void *dma_vaddr);

static inline uint8_t hda_get_stream_by_offsets(uint32_t offset,
    uint8_t reg_offset);
static inline uint32_t hda_get_offset_stream(uint8_t stream_ind);

static void hda_set_gctl(struct hda_softc *sc, uint32_t offset, uint32_t old);
static void hda_set_statests(struct hda_softc *sc, uint32_t offset,
    uint32_t old);
static void hda_set_corbwp(struct hda_softc *sc, uint32_t offset, uint32_t old);
static void hda_set_corbctl(struct hda_softc *sc, uint32_t offset,
    uint32_t old);
static void hda_set_rirbctl(struct hda_softc *sc, uint32_t offset,
    uint32_t old);
static void hda_set_rirbsts(struct hda_softc *sc, uint32_t offset,
    uint32_t old);
static void hda_set_dpiblbase(struct hda_softc *sc, uint32_t offset,
    uint32_t old);
static void hda_set_sdctl(struct hda_softc *sc, uint32_t offset, uint32_t old);
static void hda_set_sdctl2(struct hda_softc *sc, uint32_t offset, uint32_t old);
static void hda_set_sdsts(struct hda_softc *sc, uint32_t offset, uint32_t old);

static int hda_signal_state_change(struct hda_codec_inst *hci);
static int hda_response(struct hda_codec_inst *hci, uint32_t response,
    uint8_t unsol);
static int hda_transfer(struct hda_codec_inst *hci, uint8_t stream,
    uint8_t dir, uint8_t *buf, size_t count);

static void hda_set_pib(struct hda_softc *sc, uint8_t stream_ind, uint32_t pib);
static uint64_t hda_get_clock_ns(void);

/*
 * PCI HDA function declarations
 */
static int pci_hda_init(struct pci_devinst *pi, nvlist_t *nvl);
static void pci_hda_write(struct pci_devinst *pi, int baridx, uint64_t offset,
    int size, uint64_t value);
static uint64_t pci_hda_read(struct pci_devinst *pi, int baridx,
    uint64_t offset, int size);
/*
 * HDA global data
 */

static const hda_set_reg_handler hda_set_reg_table[] = {
	[HDAC_GCTL] = hda_set_gctl,
	[HDAC_STATESTS] = hda_set_statests,
	[HDAC_CORBWP] = hda_set_corbwp,
	[HDAC_CORBCTL] = hda_set_corbctl,
	[HDAC_RIRBCTL] = hda_set_rirbctl,
	[HDAC_RIRBSTS] = hda_set_rirbsts,
	[HDAC_DPIBLBASE] = hda_set_dpiblbase,

#define HDAC_ISTREAM(n, iss, oss)				\
	[_HDAC_ISDCTL(n, iss, oss)] = hda_set_sdctl,		\
	[_HDAC_ISDCTL(n, iss, oss) + 2] = hda_set_sdctl2,	\
	[_HDAC_ISDSTS(n, iss, oss)] = hda_set_sdsts,		\

#define HDAC_OSTREAM(n, iss, oss)				\
	[_HDAC_OSDCTL(n, iss, oss)] = hda_set_sdctl,		\
	[_HDAC_OSDCTL(n, iss, oss) + 2] = hda_set_sdctl2,	\
	[_HDAC_OSDSTS(n, iss, oss)] = hda_set_sdsts,		\

	HDAC_ISTREAM(0, HDA_ISS_NO, HDA_OSS_NO)
	HDAC_ISTREAM(1, HDA_ISS_NO, HDA_OSS_NO)
	HDAC_ISTREAM(2, HDA_ISS_NO, HDA_OSS_NO)
	HDAC_ISTREAM(3, HDA_ISS_NO, HDA_OSS_NO)

	HDAC_OSTREAM(0, HDA_ISS_NO, HDA_OSS_NO)
	HDAC_OSTREAM(1, HDA_ISS_NO, HDA_OSS_NO)
	HDAC_OSTREAM(2, HDA_ISS_NO, HDA_OSS_NO)
	HDAC_OSTREAM(3, HDA_ISS_NO, HDA_OSS_NO)
};

static const uint16_t hda_corb_sizes[] = {
	[HDAC_CORBSIZE_CORBSIZE_2]	= 2,
	[HDAC_CORBSIZE_CORBSIZE_16]	= 16,
	[HDAC_CORBSIZE_CORBSIZE_256]	= 256,
	[HDAC_CORBSIZE_CORBSIZE_MASK]	= 0,
};

static const uint16_t hda_rirb_sizes[] = {
	[HDAC_RIRBSIZE_RIRBSIZE_2]	= 2,
	[HDAC_RIRBSIZE_RIRBSIZE_16]	= 16,
	[HDAC_RIRBSIZE_RIRBSIZE_256]	= 256,
	[HDAC_RIRBSIZE_RIRBSIZE_MASK]	= 0,
};

static const struct hda_ops hops = {
	.signal		= hda_signal_state_change,
	.response	= hda_response,
	.transfer	= hda_transfer,
};

static const struct pci_devemu pci_de_hda = {
	.pe_emu		= "hda",
	.pe_init	= pci_hda_init,
	.pe_barwrite	= pci_hda_write,
	.pe_barread	= pci_hda_read
};
PCI_EMUL_SET(pci_de_hda);

SET_DECLARE(hda_codec_class_set, struct hda_codec_class);

#if DEBUG_HDA == 1
FILE *dbg;
#endif

/*
 * HDA module function definitions
 */

static inline void
hda_set_reg_by_offset(struct hda_softc *sc, uint32_t offset, uint32_t value)
{
	assert(offset < HDA_LAST_OFFSET);
	sc->regs[offset] = value;
}

static inline uint32_t
hda_get_reg_by_offset(struct hda_softc *sc, uint32_t offset)
{
	assert(offset < HDA_LAST_OFFSET);
	return sc->regs[offset];
}

static inline void
hda_set_field_by_offset(struct hda_softc *sc, uint32_t offset,
    uint32_t mask, uint32_t value)
{
	uint32_t reg_value = 0;

	reg_value = hda_get_reg_by_offset(sc, offset);

	reg_value &= ~mask;
	reg_value |= (value & mask);

	hda_set_reg_by_offset(sc, offset, reg_value);
}

static struct hda_softc *
hda_init(nvlist_t *nvl)
{
	struct hda_softc *sc = NULL;
	struct hda_codec_class *codec = NULL;
	const char *value;
	char *play;
	char *rec;
	int err;

#if DEBUG_HDA == 1
	dbg = fopen("/tmp/bhyve_hda.log", "w+");
#endif

	sc = calloc(1, sizeof(*sc));
	if (!sc)
		return (NULL);

	hda_reset_regs(sc);

	/*
	 * TODO search all configured codecs
	 * For now we play with one single codec
	 */
	codec = hda_find_codec_class("hda_codec");
	if (codec) {
		value = get_config_value_node(nvl, "play");
		if (value == NULL)
			play = NULL;
		else
			play = strdup(value);
		value = get_config_value_node(nvl, "rec");
		if (value == NULL)
			rec = NULL;
		else
			rec = strdup(value);
		DPRINTF("play: %s rec: %s", play, rec);
		if (play != NULL || rec != NULL) {
			err = hda_codec_constructor(sc, codec, play, rec);
			assert(!err);
		}
		free(play);
		free(rec);
	}

	return (sc);
}

static void
hda_update_intr(struct hda_softc *sc)
{
	struct pci_devinst *pi = sc->pci_dev;
	uint32_t intctl = hda_get_reg_by_offset(sc, HDAC_INTCTL);
	uint32_t intsts = 0;
	uint32_t sdsts = 0;
	uint32_t rirbsts = 0;
	uint32_t wakeen = 0;
	uint32_t statests = 0;
	uint32_t off = 0;
	int i;

	/* update the CIS bits */
	rirbsts = hda_get_reg_by_offset(sc, HDAC_RIRBSTS);
	if (rirbsts & (HDAC_RIRBSTS_RINTFL | HDAC_RIRBSTS_RIRBOIS))
		intsts |= HDAC_INTSTS_CIS;

	wakeen = hda_get_reg_by_offset(sc, HDAC_WAKEEN);
	statests = hda_get_reg_by_offset(sc, HDAC_STATESTS);
	if (statests & wakeen)
		intsts |= HDAC_INTSTS_CIS;

	/* update the SIS bits */
	for (i = 0; i < HDA_IOSS_NO; i++) {
		off = hda_get_offset_stream(i);
		sdsts = hda_get_reg_by_offset(sc, off + HDAC_SDSTS);
		if (sdsts & HDAC_SDSTS_BCIS)
			intsts |= (1 << i);
	}

	/* update the GIS bit */
	if (intsts)
		intsts |= HDAC_INTSTS_GIS;

	hda_set_reg_by_offset(sc, HDAC_INTSTS, intsts);

	if ((intctl & HDAC_INTCTL_GIE) && ((intsts &			\
		~HDAC_INTSTS_GIS) & intctl)) {
		if (!sc->lintr) {
			pci_lintr_assert(pi);
			sc->lintr = 1;
		}
	} else {
		if (sc->lintr) {
			pci_lintr_deassert(pi);
			sc->lintr = 0;
		}
	}
}

static void
hda_response_interrupt(struct hda_softc *sc)
{
	uint8_t rirbctl = hda_get_reg_by_offset(sc, HDAC_RIRBCTL);

	if ((rirbctl & HDAC_RIRBCTL_RINTCTL) && sc->rirb_cnt) {
		sc->rirb_cnt = 0;
		hda_set_field_by_offset(sc, HDAC_RIRBSTS, HDAC_RIRBSTS_RINTFL,
				HDAC_RIRBSTS_RINTFL);
		hda_update_intr(sc);
	}
}

static int
hda_codec_constructor(struct hda_softc *sc, struct hda_codec_class *codec,
    const char *play, const char *rec)
{
	struct hda_codec_inst *hci = NULL;

	if (sc->codecs_no >= HDA_CODEC_MAX)
		return (-1);

	hci = calloc(1, sizeof(struct hda_codec_inst));
	if (!hci)
		return (-1);

	hci->hda = sc;
	hci->hops = &hops;
	hci->cad = sc->codecs_no;
	hci->codec = codec;

	sc->codecs[sc->codecs_no++] = hci;

	if (!codec->init) {
		DPRINTF("This codec does not implement the init function");
		return (-1);
	}

	return (codec->init(hci, play, rec));
}

static struct hda_codec_class *
hda_find_codec_class(const char *name)
{
	struct hda_codec_class **pdpp = NULL, *pdp = NULL;

	SET_FOREACH(pdpp, hda_codec_class_set) {
		pdp = *pdpp;
		if (!strcmp(pdp->name, name)) {
			return (pdp);
		}
	}

	return (NULL);
}

static int
hda_send_command(struct hda_softc *sc, uint32_t verb)
{
	struct hda_codec_inst *hci = NULL;
	struct hda_codec_class *codec = NULL;
	uint8_t cad = (verb >> HDA_CMD_CAD_SHIFT) & 0x0f;

	if (cad >= sc->codecs_no)
		return (-1);

	DPRINTF("cad: 0x%x verb: 0x%x", cad, verb);

	hci = sc->codecs[cad];
	assert(hci);

	codec = hci->codec;
	assert(codec);

	if (!codec->command) {
		DPRINTF("This codec does not implement the command function");
		return (-1);
	}

	return (codec->command(hci, verb));
}

static int
hda_notify_codecs(struct hda_softc *sc, uint8_t run, uint8_t stream,
    uint8_t dir)
{
	struct hda_codec_inst *hci = NULL;
	struct hda_codec_class *codec = NULL;
	int err;
	int i;

	/* Notify each codec */
	for (i = 0; i < sc->codecs_no; i++) {
		hci = sc->codecs[i];
		assert(hci);

		codec = hci->codec;
		assert(codec);

		if (codec->notify) {
			err = codec->notify(hci, run, stream, dir);
			if (!err)
				break;
		}
	}

	return (i == sc->codecs_no ? (-1) : 0);
}

static void
hda_reset(struct hda_softc *sc)
{
	int i;
	struct hda_codec_inst *hci = NULL;
	struct hda_codec_class *codec = NULL;

	hda_reset_regs(sc);

	/* Reset each codec */
	for (i = 0; i < sc->codecs_no; i++) {
		hci = sc->codecs[i];
		assert(hci);

		codec = hci->codec;
		assert(codec);

		if (codec->reset)
			codec->reset(hci);
	}

	sc->wall_clock_start = hda_get_clock_ns();
}

static void
hda_reset_regs(struct hda_softc *sc)
{
	uint32_t off = 0;
	uint8_t i;

	DPRINTF("Reset the HDA controller registers ...");

	memset(sc->regs, 0, sizeof(sc->regs));

	hda_set_reg_by_offset(sc, HDAC_GCAP,
			HDAC_GCAP_64OK |
			(HDA_ISS_NO << HDAC_GCAP_ISS_SHIFT) |
			(HDA_OSS_NO << HDAC_GCAP_OSS_SHIFT));
	hda_set_reg_by_offset(sc, HDAC_VMAJ, 0x01);
	hda_set_reg_by_offset(sc, HDAC_OUTPAY, 0x3c);
	hda_set_reg_by_offset(sc, HDAC_INPAY, 0x1d);
	hda_set_reg_by_offset(sc, HDAC_CORBSIZE,
	    HDAC_CORBSIZE_CORBSZCAP_256 | HDAC_CORBSIZE_CORBSIZE_256);
	hda_set_reg_by_offset(sc, HDAC_RIRBSIZE,
	    HDAC_RIRBSIZE_RIRBSZCAP_256 | HDAC_RIRBSIZE_RIRBSIZE_256);

	for (i = 0; i < HDA_IOSS_NO; i++) {
		off = hda_get_offset_stream(i);
		hda_set_reg_by_offset(sc, off + HDAC_SDFIFOS, HDA_FIFO_SIZE);
	}
}

static void
hda_stream_reset(struct hda_softc *sc, uint8_t stream_ind)
{
	struct hda_stream_desc *st = &sc->streams[stream_ind];
	uint32_t off = hda_get_offset_stream(stream_ind);

	DPRINTF("Reset the HDA stream: 0x%x", stream_ind);

	/* Reset the Stream Descriptor registers */
	memset(sc->regs + HDA_STREAM_REGS_BASE + off, 0, HDA_STREAM_REGS_LEN);

	/* Reset the Stream Descriptor */
	memset(st, 0, sizeof(*st));

	hda_set_field_by_offset(sc, off + HDAC_SDSTS,
	    HDAC_SDSTS_FIFORDY, HDAC_SDSTS_FIFORDY);
	hda_set_field_by_offset(sc, off + HDAC_SDCTL0,
	    HDAC_SDCTL_SRST, HDAC_SDCTL_SRST);
}

static int
hda_stream_start(struct hda_softc *sc, uint8_t stream_ind)
{
	struct hda_stream_desc *st = &sc->streams[stream_ind];
	struct hda_bdle_desc *bdle_desc = NULL;
	struct hda_bdle *bdle = NULL;
	uint32_t lvi = 0;
	uint32_t bdl_cnt = 0;
	uint64_t bdpl = 0;
	uint64_t bdpu = 0;
	uint64_t bdl_paddr = 0;
	void *bdl_vaddr = NULL;
	uint32_t bdle_sz = 0;
	uint64_t bdle_addrl = 0;
	uint64_t bdle_addrh = 0;
	uint64_t bdle_paddr = 0;
	void *bdle_vaddr = NULL;
	uint32_t off = hda_get_offset_stream(stream_ind);
	uint32_t sdctl = 0;
	uint8_t strm = 0;
	uint8_t dir = 0;

	assert(!st->run);

	lvi = hda_get_reg_by_offset(sc, off + HDAC_SDLVI);
	bdpl = hda_get_reg_by_offset(sc, off + HDAC_SDBDPL);
	bdpu = hda_get_reg_by_offset(sc, off + HDAC_SDBDPU);

	bdl_cnt = lvi + 1;
	assert(bdl_cnt <= HDA_BDL_MAX_LEN);

	bdl_paddr = bdpl | (bdpu << 32);
	bdl_vaddr = hda_dma_get_vaddr(sc, bdl_paddr,
	    HDA_BDL_ENTRY_LEN * bdl_cnt);
	if (!bdl_vaddr) {
		DPRINTF("Fail to get the guest virtual address");
		return (-1);
	}

	DPRINTF("stream: 0x%x bdl_cnt: 0x%x bdl_paddr: 0x%lx",
	    stream_ind, bdl_cnt, bdl_paddr);

	st->bdl_cnt = bdl_cnt;

	bdle = (struct hda_bdle *)bdl_vaddr;
	for (size_t i = 0; i < bdl_cnt; i++, bdle++) {
		bdle_sz = bdle->len;
		assert(!(bdle_sz % HDA_DMA_ACCESS_LEN));

		bdle_addrl = bdle->addrl;
		bdle_addrh = bdle->addrh;

		bdle_paddr = bdle_addrl | (bdle_addrh << 32);
		bdle_vaddr = hda_dma_get_vaddr(sc, bdle_paddr, bdle_sz);
		if (!bdle_vaddr) {
			DPRINTF("Fail to get the guest virtual address");
			return (-1);
		}

		bdle_desc = &st->bdl[i];
		bdle_desc->addr = bdle_vaddr;
		bdle_desc->len = bdle_sz;
		bdle_desc->ioc = bdle->ioc;

		DPRINTF("bdle: 0x%zx bdle_sz: 0x%x", i, bdle_sz);
	}

	sdctl = hda_get_reg_by_offset(sc, off + HDAC_SDCTL0);
	strm = (sdctl >> 20) & 0x0f;
	dir = stream_ind >= HDA_ISS_NO;

	DPRINTF("strm: 0x%x, dir: 0x%x", strm, dir);

	sc->stream_map[dir][strm] = stream_ind;
	st->stream = strm;
	st->dir = dir;
	st->bp = 0;
	st->be = 0;

	hda_set_pib(sc, stream_ind, 0);

	st->run = 1;

	hda_notify_codecs(sc, 1, strm, dir);

	return (0);
}

static int
hda_stream_stop(struct hda_softc *sc, uint8_t stream_ind)
{
	struct hda_stream_desc *st = &sc->streams[stream_ind];
	uint8_t strm = st->stream;
	uint8_t dir = st->dir;

	DPRINTF("stream: 0x%x, strm: 0x%x, dir: 0x%x", stream_ind, strm, dir);

	st->run = 0;

	hda_notify_codecs(sc, 0, strm, dir);

	return (0);
}

static uint32_t
hda_read(struct hda_softc *sc, uint32_t offset)
{
	if (offset == HDAC_WALCLK)
		return (24 * (hda_get_clock_ns() -			\
			sc->wall_clock_start) / 1000);

	return (hda_get_reg_by_offset(sc, offset));
}

static int
hda_write(struct hda_softc *sc, uint32_t offset, uint8_t size, uint32_t value)
{
	uint32_t old = hda_get_reg_by_offset(sc, offset);
	uint32_t masks[] = {0x00000000, 0x000000ff, 0x0000ffff,
			0x00ffffff, 0xffffffff};
	hda_set_reg_handler set_reg_handler = NULL;

	if (offset < nitems(hda_set_reg_table))
		set_reg_handler = hda_set_reg_table[offset];

	hda_set_field_by_offset(sc, offset, masks[size], value);

	if (set_reg_handler)
		set_reg_handler(sc, offset, old);

	return (0);
}

static inline void
hda_print_cmd_ctl_data(struct hda_codec_cmd_ctl *p)
{
#if DEBUG_HDA == 1
	const char *name = p->name;
#endif
	DPRINTF("%s size: %d", name, p->size);
	DPRINTF("%s dma_vaddr: %p", name, p->dma_vaddr);
	DPRINTF("%s wp: 0x%x", name, p->wp);
	DPRINTF("%s rp: 0x%x", name, p->rp);
}

static int
hda_corb_start(struct hda_softc *sc)
{
	struct hda_codec_cmd_ctl *corb = &sc->corb;
	uint8_t corbsize = 0;
	uint64_t corblbase = 0;
	uint64_t corbubase = 0;
	uint64_t corbpaddr = 0;

	corb->name = "CORB";

	corbsize = hda_get_reg_by_offset(sc, HDAC_CORBSIZE) &		\
		   HDAC_CORBSIZE_CORBSIZE_MASK;
	corb->size = hda_corb_sizes[corbsize];

	if (!corb->size) {
		DPRINTF("Invalid corb size");
		return (-1);
	}

	corblbase = hda_get_reg_by_offset(sc, HDAC_CORBLBASE);
	corbubase = hda_get_reg_by_offset(sc, HDAC_CORBUBASE);

	corbpaddr = corblbase | (corbubase << 32);
	DPRINTF("CORB dma_paddr: %p", (void *)corbpaddr);

	corb->dma_vaddr = hda_dma_get_vaddr(sc, corbpaddr,
			HDA_CORB_ENTRY_LEN * corb->size);
	if (!corb->dma_vaddr) {
		DPRINTF("Fail to get the guest virtual address");
		return (-1);
	}

	corb->wp = hda_get_reg_by_offset(sc, HDAC_CORBWP);
	corb->rp = hda_get_reg_by_offset(sc, HDAC_CORBRP);

	corb->run = 1;

	hda_print_cmd_ctl_data(corb);

	return (0);
}

static int
hda_corb_run(struct hda_softc *sc)
{
	struct hda_codec_cmd_ctl *corb = &sc->corb;
	uint32_t verb = 0;
	int err;

	corb->wp = hda_get_reg_by_offset(sc, HDAC_CORBWP);

	while (corb->rp != corb->wp && corb->run) {
		corb->rp++;
		corb->rp %= corb->size;

		verb = hda_dma_ld_dword((uint8_t *)corb->dma_vaddr +
		    HDA_CORB_ENTRY_LEN * corb->rp);

		err = hda_send_command(sc, verb);
		assert(!err);
	}

	hda_set_reg_by_offset(sc, HDAC_CORBRP, corb->rp);

	if (corb->run)
		hda_response_interrupt(sc);

	return (0);
}

static int
hda_rirb_start(struct hda_softc *sc)
{
	struct hda_codec_cmd_ctl *rirb = &sc->rirb;
	uint8_t rirbsize = 0;
	uint64_t rirblbase = 0;
	uint64_t rirbubase = 0;
	uint64_t rirbpaddr = 0;

	rirb->name = "RIRB";

	rirbsize = hda_get_reg_by_offset(sc, HDAC_RIRBSIZE) &		\
		   HDAC_RIRBSIZE_RIRBSIZE_MASK;
	rirb->size = hda_rirb_sizes[rirbsize];

	if (!rirb->size) {
		DPRINTF("Invalid rirb size");
		return (-1);
	}

	rirblbase = hda_get_reg_by_offset(sc, HDAC_RIRBLBASE);
	rirbubase = hda_get_reg_by_offset(sc, HDAC_RIRBUBASE);

	rirbpaddr = rirblbase | (rirbubase << 32);
	DPRINTF("RIRB dma_paddr: %p", (void *)rirbpaddr);

	rirb->dma_vaddr = hda_dma_get_vaddr(sc, rirbpaddr,
			HDA_RIRB_ENTRY_LEN * rirb->size);
	if (!rirb->dma_vaddr) {
		DPRINTF("Fail to get the guest virtual address");
		return (-1);
	}

	rirb->wp = hda_get_reg_by_offset(sc, HDAC_RIRBWP);
	rirb->rp = 0x0000;

	rirb->run = 1;

	hda_print_cmd_ctl_data(rirb);

	return (0);
}

static void *
hda_dma_get_vaddr(struct hda_softc *sc, uint64_t dma_paddr, size_t len)
{
	struct pci_devinst *pi = sc->pci_dev;

	assert(pi);

	return (paddr_guest2host(pi->pi_vmctx, (uintptr_t)dma_paddr, len));
}

static void
hda_dma_st_dword(void *dma_vaddr, uint32_t data)
{
	*(uint32_t*)dma_vaddr = data;
}

static uint32_t
hda_dma_ld_dword(void *dma_vaddr)
{
	return (*(uint32_t*)dma_vaddr);
}

static inline uint8_t
hda_get_stream_by_offsets(uint32_t offset, uint8_t reg_offset)
{
	uint8_t stream_ind = (offset - reg_offset) >> 5;

	assert(stream_ind < HDA_IOSS_NO);

	return (stream_ind);
}

static inline uint32_t
hda_get_offset_stream(uint8_t stream_ind)
{
	return (stream_ind << 5);
}

static void
hda_set_gctl(struct hda_softc *sc, uint32_t offset, uint32_t old __unused)
{
	uint32_t value = hda_get_reg_by_offset(sc, offset);

	if (!(value & HDAC_GCTL_CRST)) {
		hda_reset(sc);
	}
}

static void
hda_set_statests(struct hda_softc *sc, uint32_t offset, uint32_t old)
{
	uint32_t value = hda_get_reg_by_offset(sc, offset);

	hda_set_reg_by_offset(sc, offset, old);

	/* clear the corresponding bits written by the software (guest) */
	hda_set_field_by_offset(sc, offset, value & HDA_STATESTS_IRQ_MASK, 0);

	hda_update_intr(sc);
}

static void
hda_set_corbwp(struct hda_softc *sc, uint32_t offset __unused,
    uint32_t old __unused)
{
	hda_corb_run(sc);
}

static void
hda_set_corbctl(struct hda_softc *sc, uint32_t offset, uint32_t old)
{
	uint32_t value = hda_get_reg_by_offset(sc, offset);
	int err;
	struct hda_codec_cmd_ctl *corb = NULL;

	if (value & HDAC_CORBCTL_CORBRUN) {
		if (!(old & HDAC_CORBCTL_CORBRUN)) {
			err = hda_corb_start(sc);
			assert(!err);
		}
	} else {
		corb = &sc->corb;
		memset(corb, 0, sizeof(*corb));
	}

	hda_corb_run(sc);
}

static void
hda_set_rirbctl(struct hda_softc *sc, uint32_t offset, uint32_t old __unused)
{
	uint32_t value = hda_get_reg_by_offset(sc, offset);
	int err;
	struct hda_codec_cmd_ctl *rirb = NULL;

	if (value & HDAC_RIRBCTL_RIRBDMAEN) {
		err = hda_rirb_start(sc);
		assert(!err);
	} else {
		rirb = &sc->rirb;
		memset(rirb, 0, sizeof(*rirb));
	}
}

static void
hda_set_rirbsts(struct hda_softc *sc, uint32_t offset, uint32_t old)
{
	uint32_t value = hda_get_reg_by_offset(sc, offset);

	hda_set_reg_by_offset(sc, offset, old);

	/* clear the corresponding bits written by the software (guest) */
	hda_set_field_by_offset(sc, offset, value & HDA_RIRBSTS_IRQ_MASK, 0);

	hda_update_intr(sc);
}

static void
hda_set_dpiblbase(struct hda_softc *sc, uint32_t offset, uint32_t old)
{
	uint32_t value = hda_get_reg_by_offset(sc, offset);
	uint64_t dpiblbase = 0;
	uint64_t dpibubase = 0;
	uint64_t dpibpaddr = 0;

	if ((value & HDAC_DPLBASE_DPLBASE_DMAPBE) != (old &		\
				HDAC_DPLBASE_DPLBASE_DMAPBE)) {
		if (value & HDAC_DPLBASE_DPLBASE_DMAPBE) {
			dpiblbase = value & HDAC_DPLBASE_DPLBASE_MASK;
			dpibubase = hda_get_reg_by_offset(sc, HDAC_DPIBUBASE);

			dpibpaddr = dpiblbase | (dpibubase << 32);
			DPRINTF("DMA Position In Buffer dma_paddr: %p",
			    (void *)dpibpaddr);

			sc->dma_pib_vaddr = hda_dma_get_vaddr(sc, dpibpaddr,
					HDA_DMA_PIB_ENTRY_LEN * HDA_IOSS_NO);
			if (!sc->dma_pib_vaddr) {
				DPRINTF("Fail to get the guest \
					 virtual address");
				assert(0);
			}
		} else {
			DPRINTF("DMA Position In Buffer Reset");
			sc->dma_pib_vaddr = NULL;
		}
	}
}

static void
hda_set_sdctl(struct hda_softc *sc, uint32_t offset, uint32_t old)
{
	uint8_t stream_ind = hda_get_stream_by_offsets(offset, HDAC_SDCTL0);
	uint32_t value = hda_get_reg_by_offset(sc, offset);
	int err;

	DPRINTF("stream_ind: 0x%x old: 0x%x value: 0x%x",
	    stream_ind, old, value);

	if (value & HDAC_SDCTL_SRST) {
		hda_stream_reset(sc, stream_ind);
	}

	if ((value & HDAC_SDCTL_RUN) != (old & HDAC_SDCTL_RUN)) {
		if (value & HDAC_SDCTL_RUN) {
			err = hda_stream_start(sc, stream_ind);
			assert(!err);
		} else {
			err = hda_stream_stop(sc, stream_ind);
			assert(!err);
		}
	}
}

static void
hda_set_sdctl2(struct hda_softc *sc, uint32_t offset, uint32_t old __unused)
{
	uint32_t value = hda_get_reg_by_offset(sc, offset);

	hda_set_field_by_offset(sc, offset - 2, 0x00ff0000, value << 16);
}

static void
hda_set_sdsts(struct hda_softc *sc, uint32_t offset, uint32_t old)
{
	uint32_t value = hda_get_reg_by_offset(sc, offset);

	hda_set_reg_by_offset(sc, offset, old);

	/* clear the corresponding bits written by the software (guest) */
	hda_set_field_by_offset(sc, offset, value & HDA_SDSTS_IRQ_MASK, 0);

	hda_update_intr(sc);
}

static int
hda_signal_state_change(struct hda_codec_inst *hci)
{
	struct hda_softc *sc = NULL;
	uint32_t sdiwake = 0;

	assert(hci);
	assert(hci->hda);

	DPRINTF("cad: 0x%x", hci->cad);

	sc = hci->hda;
	sdiwake = 1 << hci->cad;

	hda_set_field_by_offset(sc, HDAC_STATESTS, sdiwake, sdiwake);
	hda_update_intr(sc);

	return (0);
}

static int
hda_response(struct hda_codec_inst *hci, uint32_t response, uint8_t unsol)
{
	struct hda_softc *sc = NULL;
	struct hda_codec_cmd_ctl *rirb = NULL;
	uint32_t response_ex = 0;
	uint8_t rintcnt = 0;

	assert(hci);
	assert(hci->cad <= HDA_CODEC_MAX);

	response_ex = hci->cad | unsol;

	sc = hci->hda;
	assert(sc);

	rirb = &sc->rirb;

	if (rirb->run) {
		rirb->wp++;
		rirb->wp %= rirb->size;

		hda_dma_st_dword((uint8_t *)rirb->dma_vaddr +
		    HDA_RIRB_ENTRY_LEN * rirb->wp, response);
		hda_dma_st_dword((uint8_t *)rirb->dma_vaddr +
		    HDA_RIRB_ENTRY_LEN * rirb->wp + 0x04, response_ex);

		hda_set_reg_by_offset(sc, HDAC_RIRBWP, rirb->wp);

		sc->rirb_cnt++;
	}

	rintcnt = hda_get_reg_by_offset(sc, HDAC_RINTCNT);
	if (sc->rirb_cnt == rintcnt)
		hda_response_interrupt(sc);

	return (0);
}

static int
hda_transfer(struct hda_codec_inst *hci, uint8_t stream, uint8_t dir,
    uint8_t *buf, size_t count)
{
	struct hda_softc *sc = NULL;
	struct hda_stream_desc *st = NULL;
	struct hda_bdle_desc *bdl = NULL;
	struct hda_bdle_desc *bdle_desc = NULL;
	uint8_t stream_ind = 0;
	uint32_t lpib = 0;
	uint32_t off = 0;
	size_t left = 0;
	uint8_t irq = 0;

	assert(hci);
	assert(hci->hda);
	assert(buf);
	assert(!(count % HDA_DMA_ACCESS_LEN));

	if (!stream) {
		DPRINTF("Invalid stream");
		return (-1);
	}

	sc = hci->hda;

	assert(stream < HDA_STREAM_TAGS_CNT);
	stream_ind = sc->stream_map[dir][stream];

	if (!dir)
		assert(stream_ind < HDA_ISS_NO);
	else
		assert(stream_ind >= HDA_ISS_NO && stream_ind < HDA_IOSS_NO);

	st = &sc->streams[stream_ind];
	if (!st->run) {
		DPRINTF("Stream 0x%x stopped", stream);
		return (-1);
	}

	assert(st->stream == stream);

	off = hda_get_offset_stream(stream_ind);

	lpib = hda_get_reg_by_offset(sc, off + HDAC_SDLPIB);

	bdl = st->bdl;

	assert(st->be < st->bdl_cnt);
	assert(st->bp < bdl[st->be].len);

	left = count;
	while (left) {
		bdle_desc = &bdl[st->be];

		if (dir)
			*(uint32_t *)buf = hda_dma_ld_dword(
			    (uint8_t *)bdle_desc->addr + st->bp);
		else
			hda_dma_st_dword((uint8_t *)bdle_desc->addr +
			    st->bp, *(uint32_t *)buf);

		buf += HDA_DMA_ACCESS_LEN;
		st->bp += HDA_DMA_ACCESS_LEN;
		lpib += HDA_DMA_ACCESS_LEN;
		left -= HDA_DMA_ACCESS_LEN;

		if (st->bp == bdle_desc->len) {
			st->bp = 0;
			if (bdle_desc->ioc)
				irq = 1;
			st->be++;
			if (st->be == st->bdl_cnt) {
				st->be = 0;
				lpib = 0;
			}
			bdle_desc = &bdl[st->be];
		}
	}

	hda_set_pib(sc, stream_ind, lpib);

	if (irq) {
		hda_set_field_by_offset(sc, off + HDAC_SDSTS,
				HDAC_SDSTS_BCIS, HDAC_SDSTS_BCIS);
		hda_update_intr(sc);
	}

	return (0);
}

static void
hda_set_pib(struct hda_softc *sc, uint8_t stream_ind, uint32_t pib)
{
	uint32_t off = hda_get_offset_stream(stream_ind);

	hda_set_reg_by_offset(sc, off + HDAC_SDLPIB, pib);
	/* LPIB Alias */
	hda_set_reg_by_offset(sc, 0x2000 + off + HDAC_SDLPIB, pib);
	if (sc->dma_pib_vaddr)
		*(uint32_t *)((uint8_t *)sc->dma_pib_vaddr + stream_ind *
		    HDA_DMA_PIB_ENTRY_LEN) = pib;
}

static uint64_t hda_get_clock_ns(void)
{
	struct timespec ts;
	int err;

	err = clock_gettime(CLOCK_MONOTONIC, &ts);
	assert(!err);

	return (ts.tv_sec * 1000000000LL + ts.tv_nsec);
}

/*
 * PCI HDA function definitions
 */
static int
pci_hda_init(struct pci_devinst *pi, nvlist_t *nvl)
{
	struct hda_softc *sc = NULL;

	assert(pi != NULL);

	pci_set_cfgdata16(pi, PCIR_VENDOR, INTEL_VENDORID);
	pci_set_cfgdata16(pi, PCIR_DEVICE, HDA_INTEL_82801G);

	pci_set_cfgdata8(pi, PCIR_SUBCLASS, PCIS_MULTIMEDIA_HDA);
	pci_set_cfgdata8(pi, PCIR_CLASS, PCIC_MULTIMEDIA);

	/* select the Intel HDA mode */
	pci_set_cfgdata8(pi, PCIR_HDCTL, 0x01);

	/* allocate one BAR register for the Memory address offsets */
	pci_emul_alloc_bar(pi, 0, PCIBAR_MEM32, HDA_LAST_OFFSET);

	/* allocate an IRQ pin for our slot */
	pci_lintr_request(pi);

	sc = hda_init(nvl);
	if (!sc)
		return (-1);

	sc->pci_dev = pi;
	pi->pi_arg = sc;

	return (0);
}

static void
pci_hda_write(struct pci_devinst *pi, int baridx, uint64_t offset, int size,
    uint64_t value)
{
	struct hda_softc *sc = pi->pi_arg;
	int err;

	assert(sc);
	assert(baridx == 0);
	assert(size <= 4);

	DPRINTF("offset: 0x%lx value: 0x%lx", offset, value);

	err = hda_write(sc, offset, size, value);
	assert(!err);
}

static uint64_t
pci_hda_read(struct pci_devinst *pi, int baridx, uint64_t offset, int size)
{
	struct hda_softc *sc = pi->pi_arg;
	uint64_t value = 0;

	assert(sc);
	assert(baridx == 0);
	assert(size <= 4);

	value = hda_read(sc, offset);

	DPRINTF("offset: 0x%lx value: 0x%lx", offset, value);

	return (value);
}

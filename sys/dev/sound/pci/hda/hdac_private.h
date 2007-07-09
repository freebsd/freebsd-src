/*-
 * Copyright (c) 2006 Stephane E. Potvin <sepotvin@videotron.ca>
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

#ifndef _HDAC_PRIVATE_H_
#define _HDAC_PRIVATE_H_


/****************************************************************************
 * Miscellaneous defines
 ****************************************************************************/
#define HDAC_DMA_ALIGNMENT	128
#define HDAC_CODEC_MAX		16

#define HDAC_MTX_NAME		"hdac driver mutex"

/****************************************************************************
 * Helper Macros
 ****************************************************************************/
#define HDAC_READ_1(mem, offset)					\
	bus_space_read_1((mem)->mem_tag, (mem)->mem_handle, (offset))
#define HDAC_READ_2(mem, offset)					\
	bus_space_read_2((mem)->mem_tag, (mem)->mem_handle, (offset))
#define HDAC_READ_4(mem, offset)					\
	bus_space_read_4((mem)->mem_tag, (mem)->mem_handle, (offset))
#define HDAC_WRITE_1(mem, offset, value)				\
	bus_space_write_1((mem)->mem_tag, (mem)->mem_handle, (offset), (value))
#define HDAC_WRITE_2(mem, offset, value)				\
	bus_space_write_2((mem)->mem_tag, (mem)->mem_handle, (offset), (value))
#define HDAC_WRITE_4(mem, offset, value)				\
	bus_space_write_4((mem)->mem_tag, (mem)->mem_handle, (offset), (value))

#define HDAC_ISDCTL(sc, n)	(_HDAC_ISDCTL((n), (sc)->num_iss, (sc)->num_oss))
#define HDAC_ISDSTS(sc, n)	(_HDAC_ISDSTS((n), (sc)->num_iss, (sc)->num_oss))
#define HDAC_ISDPICB(sc, n)	(_HDAC_ISDPICB((n), (sc)->num_iss, (sc)->num_oss))
#define HDAC_ISDCBL(sc, n)	(_HDAC_ISDCBL((n), (sc)->num_iss, (sc)->num_oss))
#define HDAC_ISDLVI(sc, n)	(_HDAC_ISDLVI((n), (sc)->num_iss, (sc)->num_oss))
#define HDAC_ISDFIFOD(sc, n)	(_HDAC_ISDFIFOD((n), (sc)->num_iss, (sc)->num_oss))
#define HDAC_ISDFMT(sc, n)	(_HDAC_ISDFMT((n), (sc)->num_iss, (sc)->num_oss))
#define HDAC_ISDBDPL(sc, n)	(_HDAC_ISDBDPL((n), (sc)->num_iss, (sc)->num_oss))
#define HDAC_ISDBDPU(sc, n)	(_HDAC_ISDBDPU((n), (sc)->num_iss, (sc)->num_oss))

#define HDAC_OSDCTL(sc, n)	(_HDAC_OSDCTL((n), (sc)->num_iss, (sc)->num_oss))
#define HDAC_OSDSTS(sc, n)	(_HDAC_OSDSTS((n), (sc)->num_iss, (sc)->num_oss))
#define HDAC_OSDPICB(sc, n)	(_HDAC_OSDPICB((n), (sc)->num_iss, (sc)->num_oss))
#define HDAC_OSDCBL(sc, n)	(_HDAC_OSDCBL((n), (sc)->num_iss, (sc)->num_oss))
#define HDAC_OSDLVI(sc, n)	(_HDAC_OSDLVI((n), (sc)->num_iss, (sc)->num_oss))
#define HDAC_OSDFIFOD(sc, n)	(_HDAC_OSDFIFOD((n), (sc)->num_iss, (sc)->num_oss))
#define HDAC_OSDBDPL(sc, n)	(_HDAC_OSDBDPL((n), (sc)->num_iss, (sc)->num_oss))
#define HDAC_OSDBDPU(sc, n)	(_HDAC_OSDBDPU((n), (sc)->num_iss, (sc)->num_oss))

#define HDAC_BSDCTL(sc, n)	(_HDAC_BSDCTL((n), (sc)->num_iss, (sc)->num_oss))
#define HDAC_BSDSTS(sc, n)	(_HDAC_BSDSTS((n), (sc)->num_iss, (sc)->num_oss))
#define HDAC_BSDPICB(sc, n)	(_HDAC_BSDPICB((n), (sc)->num_iss, (sc)->num_oss))
#define HDAC_BSDCBL(sc, n)	(_HDAC_BSDCBL((n), (sc)->num_iss, (sc)->num_oss))
#define HDAC_BSDLVI(sc, n)	(_HDAC_BSDLVI((n), (sc)->num_iss, (sc)->num_oss))
#define HDAC_BSDFIFOD(sc, n)	(_HDAC_BSDFIFOD((n), (sc)->num_iss, (sc)->num_oss))
#define HDAC_BSDBDPL(sc, n)	(_HDAC_BSDBDPL((n), (sc)->num_iss, (sc)->num_oss))
#define HDAC_BSDBDPU(sc, n)	(_HDAC_BSDBDPU((n), (sc)->num_iss, (sc)->num_oss))


/****************************************************************************
 * Custom hdac malloc type
 ****************************************************************************/
MALLOC_DECLARE(M_HDAC);

/****************************************************************************
 * struct hdac_mem
 *
 * Holds the resources necessary to describe the physical memory associated
 * with the device.
 ****************************************************************************/
struct hdac_mem {
	struct resource		*mem_res;
	int			mem_rid;
	bus_space_tag_t		mem_tag;
	bus_space_handle_t	mem_handle;
};

/****************************************************************************
 * struct hdac_irq
 *
 * Holds the resources necessary to describe the irq associated with the
 * device.
 ****************************************************************************/
struct hdac_irq {
	struct resource		*irq_res;
	int			irq_rid;
	void			*irq_handle;
};

/****************************************************************************
 * struct hdac_dma
 *
 * This structure is used to hold all the information to manage the dma
 * states.
 ****************************************************************************/
struct hdac_dma {
	bus_dma_tag_t	dma_tag;
	bus_dmamap_t	dma_map;
	bus_addr_t	dma_paddr;
	bus_size_t	dma_size;
	caddr_t		dma_vaddr;
};

/****************************************************************************
 * struct hdac_rirb
 *
 * Hold a response from a verb sent to a codec received via the rirb.
 ****************************************************************************/
struct hdac_rirb {
	uint32_t	response;
	uint32_t	response_ex;
};

#define HDAC_RIRB_RESPONSE_EX_SDATA_IN_MASK	0x0000000f
#define HDAC_RIRB_RESPONSE_EX_SDATA_IN_OFFSET	0
#define HDAC_RIRB_RESPONSE_EX_UNSOLICITED	0x00000010

#define HDAC_RIRB_RESPONSE_EX_SDATA_IN(response_ex)			\
    (((response_ex) & HDAC_RIRB_RESPONSE_EX_SDATA_IN_MASK) >>		\
    HDAC_RIRB_RESPONSE_EX_SDATA_IN_OFFSET)

/****************************************************************************
 * struct hdac_command_list
 *
 * This structure holds the list of verbs that are to be sent to the codec
 * via the corb and the responses received via the rirb. It's allocated by
 * the codec driver and is owned by it.
 ****************************************************************************/
struct hdac_command_list {
	int		num_commands;
	uint32_t	*verbs;
	uint32_t	*responses;
};

typedef int nid_t;

struct hdac_softc;
/****************************************************************************
 * struct hdac_codec
 *
 ****************************************************************************/
struct hdac_codec {
	int	verbs_sent;
	int	responses_received;
	nid_t	cad;
	struct hdac_command_list *commands;
	struct hdac_softc *sc;
};

struct hdac_bdle {
	volatile uint32_t addrl;
	volatile uint32_t addrh;
	volatile uint32_t len;
	volatile uint32_t ioc;
} __packed;

#define HDA_MAX_CONNS	32
#define HDA_MAX_NAMELEN	32

struct hdac_devinfo;

struct hdac_widget {
	nid_t nid;
	int type;
	int enable;
	int nconns, selconn;
	uint32_t pflags, ctlflags;
	nid_t conns[HDA_MAX_CONNS];
	char name[HDA_MAX_NAMELEN];
	struct hdac_devinfo *devinfo;
	struct {
		uint32_t widget_cap;
		uint32_t outamp_cap;
		uint32_t inamp_cap;
		uint32_t supp_stream_formats;
		uint32_t supp_pcm_size_rate;
		uint32_t eapdbtl;
		int outpath;
	} param;
	union {
		struct {
			uint32_t config;
			uint32_t cap;
			uint32_t ctrl;
		} pin;
	} wclass;
};

struct hdac_audio_ctl {
	struct hdac_widget *widget, *childwidget;
	int enable;
	int index;
	int mute, step, size, offset;
	int left, right;
	uint32_t muted;
	int ossdev;
	uint32_t dir, ossmask, ossval;
};

/****************************************************************************
 * struct hdac_devinfo
 *
 * Holds all the parameters of a given codec function group. This is stored
 * in the ivar of each child of the hdac bus
 ****************************************************************************/
struct hdac_devinfo {
	device_t dev;
	uint16_t vendor_id;
	uint16_t device_id;
	uint8_t revision_id;
	uint8_t stepping_id;
	uint8_t node_type;
	nid_t nid;
	nid_t startnode, endnode;
	int nodecnt;
	struct hdac_codec *codec;
	struct hdac_widget *widget;
	union {
		struct {
			uint32_t outamp_cap;
			uint32_t inamp_cap;
			uint32_t supp_stream_formats;
			uint32_t supp_pcm_size_rate;
			int ctlcnt, pcnt, rcnt;
			struct hdac_audio_ctl *ctl;
			uint32_t mvol;
			uint32_t quirks;
			uint32_t gpio;
			int ossidx;
			int playcnt, reccnt;
			int parsing_strategy;
		} audio;
		/* XXX undefined: modem, hdmi. */
	} function;
};

#define HDAC_CHN_RUNNING	0x00000001
#define HDAC_CHN_SUSPEND	0x00000002

struct hdac_chan {
	struct snd_dbuf *b;
	struct pcm_channel *c;
	struct pcmchan_caps caps;
	struct hdac_devinfo *devinfo;
	struct hdac_dma	bdl_dma;
	uint32_t spd, fmt, fmtlist[8], pcmrates[16];
	uint32_t supp_stream_formats, supp_pcm_size_rate;
	uint32_t ptr, prevptr, blkcnt, blksz;
	uint32_t *dmapos;
	uint32_t flags;
	int dir;
	int off;
	int sid;
	int bit16, bit32;
	nid_t io[16];
};

/****************************************************************************
 * struct hdac_softc
 *
 * This structure holds the current state of the hdac driver.
 ****************************************************************************/

#define HDAC_F_DMA_NOCACHE	0x00000001
#define HDAC_F_MSI		0x00000002

struct hdac_softc {
	device_t	dev;
	device_t	hdabus;
	struct mtx	*lock;

	struct intr_config_hook intrhook;

	struct hdac_mem	mem;
	struct hdac_irq	irq;
	uint32_t pci_subvendor;

	uint32_t	flags;

	int		num_iss;
	int		num_oss;
	int		num_bss;
	int		support_64bit;
	int		streamcnt;

	int		corb_size;
	struct hdac_dma corb_dma;
	int		corb_wp;

	int		rirb_size;
	struct hdac_dma	rirb_dma;
	int		rirb_rp;

	struct hdac_dma	pos_dma;

	struct hdac_chan	play, rec;
	bus_dma_tag_t		chan_dmat;
	int			chan_size;
	int			chan_blkcnt;

	/*
	 * Polling
	 */
	int			polling;
	int			poll_ticks;
	int			poll_ival;
	struct callout		poll_hda;
	struct callout		poll_hdac;
	struct callout		poll_jack;

	struct task		unsolq_task;

#define HDAC_UNSOLQ_MAX		64
#define HDAC_UNSOLQ_READY	0
#define HDAC_UNSOLQ_BUSY	1
	int		unsolq_rp;
	int		unsolq_wp;
	int		unsolq_st;
	uint32_t	unsolq[HDAC_UNSOLQ_MAX];

	struct hdac_codec *codecs[HDAC_CODEC_MAX];

	int		registered;
};

/****************************************************************************
 * struct hdac_command flags
 ****************************************************************************/
#define HDAC_COMMAND_FLAG_WAITOK	0x0000
#define HDAC_COMMAND_FLAG_NOWAIT	0x0001

#endif

/*
 * Copyright (c) 2026 Abdelkader Boudih <freebsd@seuros.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _DEV_FIREWIRE_FWDV_H_
#define _DEV_FIREWIRE_FWDV_H_

/* FCP (Function Control Protocol) register addresses (IEC 61883-1 9.2) */
#define FCP_COMMAND_ADDR	0xfffff0000b00ULL
#define FCP_RESPONSE_ADDR	0xfffff0000d00ULL
#define FCP_MAX_FRAME_LEN	512

/* AV/C command/response type codes (ctype / response, 4-bit) */
#define AVC_CTYPE_CONTROL	0x0
#define AVC_CTYPE_STATUS	0x1
#define AVC_CTYPE_NOTIFY	0x3

#define AVC_RESP_NOT_IMPL	0x8
#define AVC_RESP_ACCEPTED	0x9
#define AVC_RESP_REJECTED	0xa
#define AVC_RESP_IN_TRANSITION	0xb
#define AVC_RESP_STABLE		0xc
#define AVC_RESP_INTERIM	0xf

/* AV/C subunit types (5-bit) */
#define AVC_SUBUNIT_TAPE	0x04
#define AVC_SUBUNIT_UNIT	0x1f

/* AV/C subunit ID (3-bit) */
#define AVC_SUBUNIT_ID_0	0x00
#define AVC_SUBUNIT_ID_IGNORE	0x07

/* AV/C address byte: subunit_type(5) | subunit_id(3) */
#define AVC_ADDR(type, id)	(((type) << 3) | ((id) & 0x07))
#define AVC_ADDR_UNIT		AVC_ADDR(AVC_SUBUNIT_UNIT, AVC_SUBUNIT_ID_IGNORE)
#define AVC_ADDR_TAPE0		AVC_ADDR(AVC_SUBUNIT_TAPE, AVC_SUBUNIT_ID_0)

/* AV/C unit commands (opcode) */
#define AVC_OP_UNIT_INFO	0x30
#define AVC_OP_SUBUNIT_INFO	0x31
#define AVC_OP_VENDOR_DEP	0x00
#define AVC_OP_POWER		0xb2

/* AV/C tape subunit opcodes */
#define AVC_OP_PLAY		0xc3
#define AVC_OP_RECORD		0xc2
#define AVC_OP_WIND		0xc4
#define AVC_OP_TRANSPORT_STATE	0xd0

/* PLAY operand[0] values */
#define AVC_PLAY_FWD		0x75
#define AVC_PLAY_STOP		0x60

/* WIND operand[0] values */
#define AVC_WIND_FFWD		0x75
#define AVC_WIND_REW		0x65
#define AVC_WIND_STOP		0x60

/* TRANSPORT_STATE operand[0] values */
#define AVC_XPORT_PLAY		0x01
#define AVC_XPORT_RECORD	0x02
#define AVC_XPORT_WIND_FF	0x03
#define AVC_XPORT_WIND_REW	0x04
#define AVC_XPORT_NO_INFO	0x7f

/* POWER operand[0] */
#define AVC_POWER_ON		0x70
#define AVC_POWER_OFF		0x60

/* IEEE 1394 CSR register space */
#define CSR_BASE_HI		0xffff
#define CSR_BASE_LO		0xf0000000

/* ISO channel limits */
#define ISO_CHANNEL_MASK	0x3f
#define ISO_CHANNEL_MAX		64

/* IEC 61883 plug control registers (CSR offsets) */
#define PCR_oMPR		0x900
#define PCR_oPCR(n)		(0x904 + (n) * 4)
#define PCR_iMPR		0x980
#define PCR_iPCR(n)		(0x984 + (n) * 4)

/*
 * oPCR register fields (IEC 61883-1 7.5)
 *
 * 31    30    29:24       23:16      15:14  13:10       9:0
 * online bcast n_p2p_conn channel    speed  overhead_id payload
 */
#define OPCR_ONLINE		(1U << 31)
#define OPCR_BCAST		(1U << 30)
#define OPCR_N_P2P_MASK		0x3f000000
#define OPCR_N_P2P_SHIFT	24
#define OPCR_CHANNEL_MASK	0x003f0000
#define OPCR_CHANNEL_SHIFT	16
#define OPCR_SPEED_MASK		0x0000c000
#define OPCR_SPEED_SHIFT	14
#define OPCR_OVERHEAD_MASK	0x00003c00
#define OPCR_OVERHEAD_SHIFT	10
#define OPCR_PAYLOAD_MASK	0x000003ff

/* DV frame sizes */
#define DV_SD_NTSC_FRAME	(10 * 150 * 80)	/* 120,000 bytes */
#define DV_SD_PAL_FRAME		(12 * 150 * 80)	/* 144,000 bytes */
#define DV_DVCPRO_HD_FRAME	(10 * 250 * 80)	/* 200,000 bytes */

/*
 * ioctl interface
 */
struct fwdv_info {
	uint8_t		state;
	uint8_t		iso_channel;
	uint8_t		dv_system;	/* 0=NTSC, 1=PAL, 0xff=unknown */
	uint8_t		_pad;
	uint32_t	frame_size;
	uint32_t	frame_count;
	uint32_t	frame_dropped;
	uint32_t	eui_hi;		/* device EUI-64 high */
	uint32_t	eui_lo;		/* device EUI-64 low */
	char		vendor[32];	/* from config ROM */
	char		model[32];	/* from config ROM */
};

#define FWDV_GINFO	_IOR('D', 1, struct fwdv_info)
#define FWDV_PLAY	_IO('D', 2)
#define FWDV_STOP	_IO('D', 3)
#define FWDV_FFWD	_IO('D', 4)
#define FWDV_REW	_IO('D', 5)

/* fwdv state values */
#define FWDV_STATE_IDLE		0	/* no AV/C device found */
#define FWDV_STATE_PROBED	1	/* device found, not streaming */
#define FWDV_STATE_STARTING	2	/* iso_start in progress */
#define FWDV_STATE_STREAMING	3	/* iso receive active */
#define FWDV_STATE_DETACHING	4

#define DV_DIF_BLOCK_SIZE	80
#define FWDV_MAX_FRAME_SIZE	DV_DVCPRO_HD_FRAME
#define FWDV_ISO_NCHUNK		256
#define FWDV_ISO_PKTSIZE	2048
#define FWDV_DEFAULT_ISO_CH	63
#define FWDV_FCP_XFERS		4

#ifdef _KERNEL

struct fwdv_softc {
	struct firewire_dev_comm fd;
	struct mtx		mtx;
	struct cdev		*cdev;

	struct fw_device	*fwdev;
	uint32_t		eui_hi;
	uint32_t		eui_lo;

	struct fw_bind		fcp_bind;
	uint8_t			fcp_resp[FCP_MAX_FRAME_LEN];
	int			fcp_resp_len;
	int			fcp_resp_ready;
	int			avc_busy;

	int			iso_active;

	char			vendor[32];
	char			model[32];

	struct task		probe_task;

	int			dma_ch;
	uint8_t			iso_channel;

	uint8_t			*frame_buf;
	uint8_t			*read_buf;
	uint32_t		frame_size;
	uint32_t		frame_offset;
	int			frame_ready;
	int			read_in_progress;
	uint32_t		frame_count;
	uint32_t		frame_dropped;
	int			open_count;
	struct selinfo		rsel;

	uint8_t			dv_system;
	int			dv_detected;

	int			state;
};

#define FWDV_LOCK(sc)		mtx_lock(&(sc)->mtx)
#define FWDV_UNLOCK(sc)		mtx_unlock(&(sc)->mtx)

#endif /* _KERNEL */

#endif /* _DEV_FIREWIRE_FWDV_H_ */

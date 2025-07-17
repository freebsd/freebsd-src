/*-
 * Copyright (c) 2012-2025 Patrick Kelsey.  All rights reserved.
 * Copyright (c) 2025 Ruslan Bukin <br@bsdpad.com>
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Portions of this software may have been developed with reference to
 * the SD Simplified Specification.  The following disclaimer may apply:
 *
 * The following conditions apply to the release of the simplified
 * specification ("Simplified Specification") by the SD Card Association and
 * the SD Group. The Simplified Specification is a subset of the complete SD
 * Specification which is owned by the SD Card Association and the SD
 * Group. This Simplified Specification is provided on a non-confidential
 * basis subject to the disclaimers below. Any implementation of the
 * Simplified Specification may require a license from the SD Card
 * Association, SD Group, SD-3C LLC or other third parties.
 *
 * Disclaimers:
 *
 * The information contained in the Simplified Specification is presented only
 * as a standard specification for SD Cards and SD Host/Ancillary products and
 * is provided "AS-IS" without any representations or warranties of any
 * kind. No responsibility is assumed by the SD Group, SD-3C LLC or the SD
 * Card Association for any damages, any infringements of patents or other
 * right of the SD Group, SD-3C LLC, the SD Card Association or any third
 * parties, which may result from its use. No license is granted by
 * implication, estoppel or otherwise under any patent or other rights of the
 * SD Group, SD-3C LLC, the SD Card Association or any third party. Nothing
 * herein shall be construed as an obligation by the SD Group, the SD-3C LLC
 * or the SD Card Association to disclose or distribute any technical
 * information, know-how or other confidential information to any third party.
 *
 *
 * CRC routines adapted from public domain code written by Lammert Bies.
 *
 *
 * This is an implementation of mmcbr that communicates with SD/MMC cards in
 * SPI mode via spibus_if.  In order to minimize changes to the existing
 * MMC/SD stack (and allow for maximal reuse of the same), the behavior of
 * the SD-bus command set is emulated as much as possible, where required.
 *
 * The SPI bus ownership behavior is to acquire the SPI bus for the entire
 * duration that the MMC host is acquired.
 *
 * CRC checking is enabled by default, but can be disabled at runtime
 * per-card via sysctl (e.g. sysctl dev.mmcspi.0.use_crc=0).
 *
 * Considered, but not implemented:
 *   - Card presence detection
 *   - Card power control
 *   - Detection of lock switch state on cards that have them
 *   - Yielding the CPU during long card busy cycles
 *
 * Originally developed and tested using a MicroTik RouterBOARD RB450G and
 * 31 microSD cards available circa 2012.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/resource.h>
#include <sys/sysctl.h>

#include <dev/mmc/bridge.h>
#include <dev/mmc/mmcreg.h>
#include <dev/mmc/mmcbrvar.h>
#include <dev/spibus/spi.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "mmcbr_if.h"
#include "spibus_if.h"

#define	MMCSPI_RETRIES		3
#define	MMCSPI_TIMEOUT_SEC	3

#define	MMCSPI_MAX_RSP_LEN	5  /* max length of an Rn response */
#define	MMCSPI_OCR_LEN		4

#define	MMCSPI_DATA_BLOCK_LEN	512
#define	MMCSPI_DATA_CRC_LEN	2

#define	MMCSPI_POLL_LEN		8  /* amount to read when searching */

#define	MMCSPI_R1_MASK	0x80 /* mask used to search for R1 tokens */
#define	MMCSPI_R1_VALUE	0x00 /* value used to search for R1 tokens */
#define	MMCSPI_DR_MASK	0x11 /* mask used to search for data resp tokens */
#define	MMCSPI_DR_VALUE	0x01 /* value used to search for data resp tokens */

#define	MMCSPI_DR_ERR_MASK	0x0e
#define	MMCSPI_DR_ERR_NONE	0x04
#define	MMCSPI_DR_ERR_CRC	0x0a
#define	MMCSPI_DR_ERR_WRITE	0x0c

#define	MMCSPI_TOKEN_SB		0xfe /* start block token for read single,
					read multi, and write single */
#define	MMCSPI_TOKEN_SB_WM	0xfc /* start block token for write multi */
#define	MMCSPI_TOKEN_ST		0xfd /* stop transmission token */
#define	MMCSPI_IS_DE_TOKEN(x)	(0 == ((x) & 0xf0)) /* detector for data
						       error token */

#define	MMCSPI_R1_IDLE		0x01
#define	MMCSPI_R1_ERASE_RST	0x02
#define	MMCSPI_R1_ILL_CMD	0x04
#define	MMCSPI_R1_CRC_ERR	0x08
#define	MMCSPI_R1_ERASE_ERR	0x10
#define	MMCSPI_R1_ADDR_ERR	0x20
#define	MMCSPI_R1_PARAM_ERR	0x40

#define	MMCSPI_R1_ERR_MASK	(MMCSPI_R1_PARAM_ERR | MMCSPI_R1_ADDR_ERR | \
				 MMCSPI_R1_ERASE_ERR | MMCSPI_R1_CRC_ERR | \
				 MMCSPI_R1_ILL_CMD)

#define	MMCSPI_R2_LOCKED	0x01
#define	MMCSPI_R2_WP_ER_LCK	0x02
#define	MMCSPI_R2_ERR		0x04
#define	MMCSPI_R2_CC_ERR	0x08
#define	MMCSPI_R2_ECC_FAIL	0x10
#define	MMCSPI_R2_WP_VIOLATE	0x20
#define	MMCSPI_R2_ERASE_PARAM	0x40
#define	MMCSPI_R2_OOR_CSD_OW	0x80

/* commands that only apply to the SPI interface */
#define	MMCSPI_READ_OCR		58
#define	MMCSPI_CRC_ON_OFF	59

static struct ofw_compat_data compat_data[] = {
	{ "mmc-spi-slot",	1 },
	{ NULL,			0 }
};

struct mmcspi_command {
	struct mmc_command *mmc_cmd;	/* command passed from mmc layer */
	uint32_t	opcode;		/* possibly translated opcode */
	uint32_t	arg;		/* possibly translated arg */
	uint32_t	flags;		/* possibly translated flags */
	uint32_t	retries;	/* possibly translated retry count */
	struct mmc_data	*data;		/* possibly redirected data segment */
	unsigned int	error_mask;	/* R1 errors check mask */
	unsigned char	use_crc;	/* do crc checking for this command */
	unsigned char	rsp_type;	/* SPI response type of this command */
#define	MMCSPI_RSP_R1	0
#define	MMCSPI_RSP_R1B	1
#define	MMCSPI_RSP_R2	2
#define	MMCSPI_RSP_R3	3
#define	MMCSPI_RSP_R7	4
	unsigned char	rsp_len;	/* response len of this command */
	unsigned char	mmc_rsp_type;	/* MMC reponse type to translate to */
#define	MMCSPI_TO_MMC_RSP_NONE	0
#define	MMCSPI_TO_MMC_RSP_R1	1
#define	MMCSPI_TO_MMC_RSP_R1B	2
#define	MMCSPI_TO_MMC_RSP_R2	3
#define	MMCSPI_TO_MMC_RSP_R3	4
#define	MMCSPI_TO_MMC_RSP_R6	5
#define	MMCSPI_TO_MMC_RSP_R7	6
	struct mmc_data	ldata;		/* local read data */
};

struct mmcspi_slot {
	struct mmcspi_softc *sc;	/* back pointer to parent bridge */
	device_t	dev;		/* mmc device for slot */
	boolean_t	bus_busy;	/* host has been acquired */
	struct mmc_host host;		/* host parameters */
	struct mtx	mtx;		/* slot mutex */
	uint8_t		last_ocr[MMCSPI_OCR_LEN]; /* ocr retrieved after CMD8 */
	uint32_t	last_opcode;	/* last opcode requested by mmc layer */
	uint32_t	last_flags;	/* last flags requested by mmc layer */
	unsigned int	crc_enabled;	/* crc checking is enabled */
	unsigned int	crc_init_done;  /* whether the initial crc setting has
					   been sent to the card */
#define	MMCSPI_MAX_LDATA_LEN 16
	uint8_t	ldata_buf[MMCSPI_MAX_LDATA_LEN];
};

struct mmcspi_softc {
	device_t		dev;		/* this mmc bridge device */
	device_t		busdev;
	struct mmcspi_slot	slot;
	unsigned int		use_crc;	/* command CRC checking */
};

#if defined(MMCSPI_ENABLE_DEBUG_FUNCS)
static void mmcspi_dump_data(device_t dev, const char *label, uint8_t *data,
    unsigned int len);
static void mmcspi_dump_spi_bus(device_t dev, unsigned int len);
#endif

#define	MMCSPI_LOCK_SLOT(_slot)			mtx_lock(&(_slot)->mtx)
#define	MMCSPI_UNLOCK_SLOT(_slot)		mtx_unlock(&(_slot)->mtx)
#define	MMCSPI_SLOT_LOCK_INIT(_slot)		mtx_init(&(_slot)->mtx, \
    "SD slot mtx", "mmcspi", MTX_DEF)
#define	MMCSPI_SLOT_LOCK_DESTROY(_slot)		mtx_destroy(&(_slot)->mtx);
#define	MMCSPI_ASSERT_SLOT_LOCKED(_slot)	mtx_assert(&(_slot)->mtx, \
    MA_OWNED);
#define	MMCSPI_ASSERT_SLOT_UNLOCKED(_slot)	mtx_assert(&(_slot)->mtx, \
    MA_NOTOWNED);

#define	TRACE_ZONE_ENABLED(zone) (trace_zone_mask & TRACE_ZONE_##zone)

#define	TRACE_ENTER(dev)					\
	if (TRACE_ZONE_ENABLED(ENTER)) {			\
		device_printf(dev, "%s: enter\n", __func__);	\
	}

#define	TRACE_EXIT(dev)						\
	if (TRACE_ZONE_ENABLED(EXIT)) {				\
		device_printf(dev, "%s: exit\n", __func__);	\
	}

#define	TRACE(dev, zone, ...)				\
	if (TRACE_ZONE_ENABLED(zone)) {			\
		device_printf(dev, __VA_ARGS__);	\
	}

#define	TRACE_ZONE_ENTER   (1ul << 0)  /* function entrance */
#define	TRACE_ZONE_EXIT    (1ul << 1)  /* function exit */
#define	TRACE_ZONE_ACTION  (1ul << 2)  /* for narrating major actions taken */
#define	TRACE_ZONE_RESULT  (1ul << 3)  /* for narrating results of actions */
#define	TRACE_ZONE_ERROR   (1ul << 4)  /* for reporting errors */
#define	TRACE_ZONE_DATA    (1ul << 5)  /* for dumping bus data */
#define	TRACE_ZONE_DETAILS (1ul << 6)  /* for narrating minor actions/results */

#define	TRACE_ZONE_NONE    0
#define	TRACE_ZONE_ALL     0xffffffff

#define	CRC7_INITIAL 0x00
#define	CRC16_INITIAL 0x0000

SYSCTL_NODE(_hw, OID_AUTO, mmcspi, CTLFLAG_RD, 0, "mmcspi driver");

static unsigned int trace_zone_mask = TRACE_ZONE_ERROR;

static uint8_t crc7tab[256];
static uint16_t crc16tab[256];
static uint8_t onesbuf[MMCSPI_DATA_BLOCK_LEN];  /* for driving the tx line
						   when receiving */
static uint8_t junkbuf[MMCSPI_DATA_BLOCK_LEN];  /* for receiving data when
						   transmitting */

static uint8_t
update_crc7(uint8_t crc, uint8_t *buf, unsigned int len)
{
	uint8_t tmp;
	int i;

	for (i = 0; i < len; i++) {
		tmp = (crc << 1) ^ buf[i];
		crc = crc7tab[tmp];
	}

	return (crc);
}

static uint16_t
update_crc16(uint16_t crc, uint8_t *buf, unsigned int len)
{
	uint16_t tmp, c16;
	int i;

	for (i = 0; i < len; i++) {
		c16  = 0x00ff & (uint16_t)buf[i];

		tmp = (crc >> 8) ^ c16;
		crc = (crc << 8) ^ crc16tab[tmp];
	}

	return (crc);
}

static void
init_crc7tab(void)
{
#define	P_CRC7 0x89

	int i, j;
	uint8_t crc, c;

	for (i = 0; i < 256; i++) {

		c = (uint8_t)i;
		crc = (c & 0x80) ? c ^ P_CRC7 : c;

		for (j=1; j<8; j++) {
			crc = crc << 1;

			if (crc & 0x80)
				crc = crc ^ P_CRC7;
		}

		crc7tab[i] = crc;
	}
}

static void
init_crc16tab(void)
{
#define	P_CCITT 0x1021

	int i, j;
	uint16_t crc, c;

	for (i = 0; i < 256; i++) {

		crc = 0;
		c   = ((uint16_t) i) << 8;

		for (j=0; j<8; j++) {

			if ((crc ^ c) & 0x8000) crc = ( crc << 1 ) ^ P_CCITT;
			else                    crc =   crc << 1;

			c = c << 1;
		}

		crc16tab[i] = crc;
	}
}

static void
mmcspi_slot_init(device_t brdev, struct mmcspi_slot *slot)
{
	struct mmcspi_softc *sc;

	TRACE_ENTER(brdev);

	sc = device_get_softc(brdev);

	slot->sc = sc;
	slot->dev = NULL;  /* will get real value when card is added */
	slot->bus_busy = false;
	slot->host.f_min = 100000; /* this should be as low as we need to go
				      for any card */
	slot->host.caps = 0;
	/* SPI mode requires 3.3V operation */
	slot->host.host_ocr = MMC_OCR_320_330 | MMC_OCR_330_340;

	MMCSPI_SLOT_LOCK_INIT(slot);

	TRACE_EXIT(brdev);
}

static void
mmcspi_slot_fini(device_t brdev, struct mmcspi_slot *slot)
{
	TRACE_ENTER(brdev);

	MMCSPI_SLOT_LOCK_DESTROY(slot);

	TRACE_EXIT(brdev);
}

static void
mmcspi_card_add(struct mmcspi_slot *slot)
{
	device_t brdev;
	device_t child;

	brdev = slot->sc->dev;

	TRACE_ENTER(brdev);

	child = device_add_child(brdev, "mmc", DEVICE_UNIT_ANY);

	MMCSPI_LOCK_SLOT(slot);
	slot->dev = child;
	device_set_ivars(slot->dev, slot);
	MMCSPI_UNLOCK_SLOT(slot);

	device_probe_and_attach(slot->dev);

	TRACE_EXIT(brdev);
}

static void
mmcspi_card_delete(struct mmcspi_slot *slot)
{
	device_t brdev;
	device_t dev;

	brdev = slot->sc->dev;

	TRACE_ENTER(brdev);

	MMCSPI_LOCK_SLOT(slot);
	dev = slot->dev;
	slot->dev = NULL;
	MMCSPI_UNLOCK_SLOT(slot);
	device_delete_child(brdev, dev);

	TRACE_EXIT(brdev);
}

static int
mmcspi_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "MMC SPI mode controller");

	return (BUS_PROBE_DEFAULT);
}

static int
mmcspi_attach(device_t dev)
{
	struct mmcspi_softc *sc;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree;
	struct sysctl_oid_list *child;

	TRACE_ENTER(dev);

	sc = device_get_softc(dev);
	ctx = device_get_sysctl_ctx(dev);
	tree = device_get_sysctl_tree(dev);
	child = SYSCTL_CHILDREN(tree);

	sc->dev = dev;
	sc->busdev = device_get_parent(dev);
	sc->use_crc = 1;

	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "use_crc", CTLFLAG_RW,
	    &sc->use_crc, sizeof(sc->use_crc), "Enable/disable crc checking");

	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "trace_mask", CTLFLAG_RW,
	    &trace_zone_mask, sizeof(trace_zone_mask), "Bitmask for adjusting "
	    "trace messages");

	mmcspi_slot_init(dev, &sc->slot);

	/* XXX trigger this from card insert detection */
	mmcspi_card_add(&sc->slot);

	TRACE_EXIT(dev);

	return (0);
}

static int
mmcspi_detach(device_t dev)
{
	struct mmcspi_softc *sc;

	TRACE_ENTER(dev);

	sc = device_get_softc(dev);

	/* XXX trigger this from card removal detection */
	mmcspi_card_delete(&sc->slot);

	mmcspi_slot_fini(dev, &sc->slot);

	TRACE_EXIT(dev);

	return (0);
}

static int
mmcspi_suspend(device_t dev)
{
	int err;

	TRACE_ENTER(dev);
	err = bus_generic_suspend(dev);
	if (err) {
		TRACE_EXIT(dev);
		return (err);
	}
	TRACE_EXIT(dev);

	return (0);
}

static int
mmcspi_resume(device_t dev)
{
	int err;

	TRACE_ENTER(dev);
	err = bus_generic_resume(dev);
	if (err) {
		TRACE_EXIT(dev);
		return (err);
	}
	TRACE_EXIT(dev);

	return (0);
}

static int
mmcspi_read_ivar(device_t bus, device_t child, int which, uintptr_t *result)
{
	struct mmcspi_slot *slot;

	TRACE_ENTER(bus);

	slot = device_get_ivars(child);

	switch (which) {
	case MMCBR_IVAR_BUS_TYPE:
		*result = bus_type_spi;
		break;
	case MMCBR_IVAR_BUS_MODE:
		*result = slot->host.ios.bus_mode;
		break;
	case MMCBR_IVAR_BUS_WIDTH:
		*result = slot->host.ios.bus_width;
		break;
	case MMCBR_IVAR_CHIP_SELECT:
		*result = slot->host.ios.chip_select;
		break;
	case MMCBR_IVAR_CLOCK:
		*result = slot->host.ios.clock;
		break;
	case MMCBR_IVAR_F_MIN:
		*result = slot->host.f_min;
		break;
	case MMCBR_IVAR_F_MAX:
		*result = slot->host.f_max;
		break;
	case MMCBR_IVAR_HOST_OCR:
		*result = slot->host.host_ocr;
		break;
	case MMCBR_IVAR_MODE:
		*result = slot->host.mode;
		break;
	case MMCBR_IVAR_OCR:
		*result = slot->host.ocr;
		break;
	case MMCBR_IVAR_POWER_MODE:
		*result = slot->host.ios.power_mode;
		break;
	case MMCBR_IVAR_VDD:
		*result = slot->host.ios.vdd;
		break;
	case MMCBR_IVAR_VCCQ:
		*result = slot->host.ios.vccq;
		break;
	case MMCBR_IVAR_CAPS:
		*result = slot->host.caps;
		break;
	case MMCBR_IVAR_TIMING:
		*result = slot->host.ios.timing;
		break;
	case MMCBR_IVAR_MAX_DATA:
		/* seems reasonable, not dictated by anything */
		*result = 64 * 1024;
		break;
	default:
		return (EINVAL);
	}

	TRACE_EXIT(bus);

	return (0);
}

static int
mmcspi_write_ivar(device_t bus, device_t child, int which, uintptr_t value)
{
	struct mmcspi_slot *slot;

	TRACE_ENTER(bus);

	slot = device_get_ivars(child);

	switch (which) {
	default:
		return (EINVAL);
	case MMCBR_IVAR_BUS_MODE:
		slot->host.ios.bus_mode = value;
		break;
	case MMCBR_IVAR_BUS_WIDTH:
		slot->host.ios.bus_width = value;
		break;
	case MMCBR_IVAR_CLOCK:
		slot->host.ios.clock = value;
		break;
	case MMCBR_IVAR_CHIP_SELECT:
		slot->host.ios.chip_select = value;
		break;
	case MMCBR_IVAR_MODE:
		slot->host.mode = value;
		break;
	case MMCBR_IVAR_OCR:
		slot->host.ocr = value;
		break;
	case MMCBR_IVAR_POWER_MODE:
		slot->host.ios.power_mode = value;
		break;
	case MMCBR_IVAR_VDD:
		slot->host.ios.vdd = value;
		break;
	case MMCBR_IVAR_VCCQ:
		slot->host.ios.vccq = value;
		break;
	case MMCBR_IVAR_TIMING:
		slot->host.ios.timing = value;
		break;
	case MMCBR_IVAR_BUS_TYPE:
	case MMCBR_IVAR_CAPS:
	case MMCBR_IVAR_HOST_OCR:
	case MMCBR_IVAR_F_MIN:
	case MMCBR_IVAR_F_MAX:
	case MMCBR_IVAR_MAX_DATA:
		return (EINVAL);
	}
	TRACE_EXIT(bus);

	return (0);
}

static unsigned int
mmcspi_do_spi_read(device_t dev, uint8_t *data, unsigned int len)
{
	struct spi_command spi_cmd;
	struct mmcspi_softc *sc;
	int err;

	TRACE_ENTER(dev);

	sc = device_get_softc(dev);

	spi_cmd.tx_cmd = onesbuf;
	spi_cmd.rx_cmd = data;
	spi_cmd.tx_cmd_sz = len;
	spi_cmd.rx_cmd_sz = len;
	spi_cmd.tx_data = NULL;
	spi_cmd.rx_data = NULL;
	spi_cmd.tx_data_sz = 0;
	spi_cmd.rx_data_sz = 0;

	err = SPIBUS_TRANSFER(sc->busdev, sc->dev, &spi_cmd);

#ifdef DEBUG_RX
	int i;
	if (err == 0) {
		printf("rx val: ");
		for (i = 0; i < len; i++)
			printf("%x ", data[i]);
		printf("\n");
	}
#endif

	TRACE_EXIT(dev);

	return (err ? MMC_ERR_FAILED : MMC_ERR_NONE);
}

static unsigned int
mmcspi_do_spi_write(device_t dev, uint8_t *cmd, unsigned int cmdlen,
    uint8_t *data, unsigned int datalen)
{
	struct mmcspi_softc *sc;
	struct spi_command spi_cmd;
	int err;

	TRACE_ENTER(dev);

	sc = device_get_softc(dev);

	spi_cmd.tx_cmd = cmd;
	spi_cmd.rx_cmd = junkbuf;
	spi_cmd.tx_cmd_sz = cmdlen;
	spi_cmd.rx_cmd_sz = cmdlen;
	spi_cmd.tx_data = data;
	spi_cmd.rx_data = junkbuf;
	spi_cmd.tx_data_sz = datalen;
	spi_cmd.rx_data_sz = datalen;

	err = SPIBUS_TRANSFER(sc->busdev, sc->dev, &spi_cmd);

	TRACE_EXIT(dev);

	return (err ? MMC_ERR_FAILED : MMC_ERR_NONE);
}

static unsigned int
mmcspi_wait_for_not_busy(device_t dev)
{
	unsigned int busy_length;
	uint8_t pollbuf[MMCSPI_POLL_LEN];
	struct bintime start, elapsed;
	unsigned int err;
	int i;

	busy_length = 0;

	TRACE_ENTER(dev);
	TRACE(dev, ACTION, "waiting for not busy\n");

	getbintime(&start);
	do {
		TRACE(dev, DETAILS, "looking for end of busy\n");
		err = mmcspi_do_spi_read(dev, pollbuf, MMCSPI_POLL_LEN);
		if (MMC_ERR_NONE != err) {
			TRACE(dev, ERROR, "spi read failed\n");
			TRACE_EXIT(dev);
			return (err);
		}

		for (i = 0; i < MMCSPI_POLL_LEN; i++) {
			if (pollbuf[i] != 0x00) {
				TRACE(dev, DETAILS,
				    "end of busy found at %d\n", i);
				break;
			}
			busy_length++;
		}

		getbintime(&elapsed);
		bintime_sub(&elapsed, &start);

		if (elapsed.sec > MMCSPI_TIMEOUT_SEC) {
			TRACE(dev, ERROR, "card busy timeout\n");
			return (MMC_ERR_TIMEOUT);
		}
	} while (MMCSPI_POLL_LEN == i);

	TRACE(dev, RESULT, "busy for %u byte slots\n", busy_length);
	TRACE_EXIT(dev);

	return (MMC_ERR_NONE);
}

static int
mmcspi_update_ios(device_t brdev, device_t reqdev)
{
	struct mmcspi_softc *sc;
	struct mmcspi_slot *slot;
	struct spi_command spi_cmd;

	TRACE_ENTER(brdev);

	sc = device_get_softc(brdev);
	slot = device_get_ivars(reqdev);

	if (power_up == slot->host.ios.power_mode) {
		/*
		 * This sequence provides the initialization steps required
		 * by the spec after card power is applied, but before any
		 * commands are issued.  These operations are harmless if
		 * applied at any other time (after a warm reset, for
		 * example).
		 */

		/*
		 * XXX Power-on portion of implementation of card power
		 * control should go here.  Should probably include a power
		 * off first to ensure card is fully reset from any previous
		 * state.
		 */

		/*
		 * Make sure power to card has ramped up.  The spec requires
		 * power to ramp up in 35ms or less.
		 */
		DELAY(35000);

		/*
		 * Provide at least 74 clocks with CS and MOSI high that the
		 * spec requires after card power stabilizes.
		 */

		spi_cmd.tx_cmd = onesbuf;
		spi_cmd.tx_cmd_sz = 10;
		spi_cmd.rx_cmd = junkbuf;
		spi_cmd.rx_cmd_sz = 10;
		spi_cmd.tx_data = NULL;
		spi_cmd.rx_data = NULL;
		spi_cmd.tx_data_sz = 0;
		spi_cmd.rx_data_sz = 0;

		SPIBUS_TRANSFER(sc->busdev, sc->dev, &spi_cmd);

		/*
		 * Perhaps this was a warm reset and the card is in the
		 * middle of a long operation.
		 */
		mmcspi_wait_for_not_busy(brdev);

		slot->last_opcode = 0xffffffff;
		slot->last_flags = 0;
		memset(slot->last_ocr, 0, MMCSPI_OCR_LEN);
		slot->crc_enabled = 0;
		slot->crc_init_done = 0;
	}

	if (power_off == slot->host.ios.power_mode) {
		/*
		 * XXX Power-off portion of implementation of card power
		 * control should go here.
		 */
	}

	TRACE_EXIT(brdev);

	return (0);
}

static unsigned int
mmcspi_shift_copy(uint8_t *dest, uint8_t *src, unsigned int dest_len,
    unsigned int shift)
{
	unsigned int i;

	if (0 == shift)
		memcpy(dest, src, dest_len);
	else {
		for (i = 0; i < dest_len; i++) {
			dest[i] =
			    (src[i] << shift) |
			    (src[i + 1] >> (8 - shift));
		}
	}

	return (dest_len);
}

static unsigned int
mmcspi_get_response_token(device_t dev, uint8_t mask, uint8_t value,
    unsigned int len, unsigned int has_busy, uint8_t *rspbuf)
{
	uint8_t pollbuf[2 * MMCSPI_MAX_RSP_LEN];
	struct bintime start, elapsed;
	boolean_t found;
	unsigned int err;
	unsigned int offset;
	unsigned int shift = 0;
	unsigned int remaining;
	uint16_t search_space;
	uint16_t search_mask;
	uint16_t search_value;
	int i;

	TRACE_ENTER(dev);

	/*
	 * This loop searches data clocked out of the card for a response
	 * token matching the given mask and value.  It will locate tokens
	 * that are not byte-aligned, as some cards send non-byte-aligned
	 * response tokens in some situations.  For example, the following
	 * card consistently sends an unaligned response token to the stop
	 * command used to terminate multi-block reads:
	 *
	 * Transcend 2GB SDSC card, cid:
	 * mid=0x1b oid=0x534d pnm="00000" prv=1.0 mdt=00.2000
	 */

	offset = 0;
	found = false;
	getbintime(&start);
	do {
		TRACE(dev, DETAILS, "looking for response token with "
		    "mask 0x%02x, value 0x%02x\n", mask, value);
		err = mmcspi_do_spi_read(dev, &pollbuf[offset], len);
		if (MMC_ERR_NONE != err) {
			TRACE(dev, ERROR, "spi read of resp token failed\n");
			TRACE_EXIT(dev);
			return (err);
		}

		for (i = 0; i < len + offset; i++) {
			if ((pollbuf[i] & mask) == value) {
				TRACE(dev, DETAILS, "response token found at "
				    "%d (0x%02x)\n", i, pollbuf[i]);
				shift = 0;
				found = true;
				break;
			} else if (i < len + offset - 1) {
				/*
				 * Not the last byte in the buffer, so check
				 * for a non-aligned response.
				 */
				search_space = ((uint16_t)pollbuf[i] << 8) |
				    pollbuf[i + 1];
				search_mask  = (uint16_t)mask << 8;
				search_value = (uint16_t)value << 8;

				TRACE(dev, DETAILS, "search: space=0x%04x "
				    " mask=0x%04x val=0x%04x\n", search_space,
				    search_mask, search_value);

				for (shift = 1; shift < 8; shift++) {
					search_space <<= 1;
					if ((search_space & search_mask) ==
					    search_value) {
						found = true;
						TRACE(dev, DETAILS, "Found mat"
						    "ch at shift %u\n", shift);
						break;
					}
				}

				if (shift < 8)
					break;
			} else {
				/*
				 * Move the last byte to the first position
				 * and go 'round again.
				 */
				pollbuf[0] = pollbuf[i];
			}
		}

		if (!found) {
			offset = 1;

			getbintime(&elapsed);
			bintime_sub(&elapsed, &start);

			if (elapsed.sec > MMCSPI_TIMEOUT_SEC) {
				TRACE(dev, ERROR, "timeout while looking for "
				    "response token\n");
				return (MMC_ERR_TIMEOUT);
			}
		}
	} while (!found);

	/*
	 * Note that if i == 0 and offset == 1, shift is always greater than
	 * zero.
	 */
	remaining = i - offset + (shift ? 1 : 0);

	TRACE(dev, DETAILS, "len=%u i=%u rem=%u shift=%u\n",
	      len, i, remaining, shift);

	if (remaining) {
		err = mmcspi_do_spi_read(dev, &pollbuf[len + offset],
		    remaining);
		if (MMC_ERR_NONE != err) {
			TRACE(dev, ERROR, "spi read of remainder of response "
			    "token failed\n");
			TRACE_EXIT(dev);
			return (err);
		}
	}

	mmcspi_shift_copy(rspbuf, &pollbuf[i], len, shift);

	if (TRACE_ZONE_ENABLED(RESULT)) {
		TRACE(dev, RESULT, "response =");
		for (i = 0; i < len; i++)
			printf(" 0x%02x", rspbuf[i]);
		printf("\n");
	}

	if (has_busy) {
		err = mmcspi_wait_for_not_busy(dev);
		if (MMC_ERR_NONE != err) {
			TRACE_EXIT(dev);
			return (err);
		}
	}

	TRACE_EXIT(dev);

	return (MMC_ERR_NONE);
}

static unsigned int
mmcspi_set_up_command(device_t dev, struct mmcspi_command *mmcspi_cmd,
    struct mmc_command *mmc_cmd)
{
	struct mmcspi_softc *sc;
	struct mmcspi_slot *slot;
	uint32_t opcode;
	uint32_t arg;
	uint32_t flags;
	uint32_t retries;
	unsigned char rsp_type;
	unsigned char rsp_len;
	unsigned char mmc_rsp_type;
	unsigned int ldata_len = 0;
	unsigned int use_crc;

	sc = device_get_softc(dev);
	slot  = &sc->slot;
	use_crc = slot->crc_enabled;

	opcode = mmc_cmd->opcode;
	arg = mmc_cmd->arg;
	flags = mmc_cmd->flags;
	retries = mmc_cmd->retries;

	if (flags & MMC_CMD_IS_APP) {
		switch (opcode) {
		case ACMD_SD_STATUS:
			rsp_type = MMCSPI_RSP_R2;
			mmc_rsp_type = MMCSPI_TO_MMC_RSP_R1;
			break;
		case ACMD_SEND_NUM_WR_BLOCKS:
		case ACMD_SET_WR_BLK_ERASE_COUNT:
		case ACMD_SET_CLR_CARD_DETECT:
		case ACMD_SEND_SCR:
			rsp_type = MMCSPI_RSP_R1;
			mmc_rsp_type = MMCSPI_TO_MMC_RSP_R1;
			break;
		case ACMD_SD_SEND_OP_COND:
			/* only HCS bit is valid in spi mode */
			arg &= 0x40000000;
			rsp_type = MMCSPI_RSP_R1;
			mmc_rsp_type = MMCSPI_TO_MMC_RSP_R3;
			break;
		default:
			TRACE(dev, ERROR, "Invalid app command opcode %u\n",
			      opcode);
			return (MMC_ERR_INVALID);
		}
	} else {
		switch (opcode) {
		case MMC_GO_IDLE_STATE:
			use_crc = 1;
			rsp_type = MMCSPI_RSP_R1;
			mmc_rsp_type = MMCSPI_TO_MMC_RSP_NONE;
			break;

		case MMC_SEND_OP_COND:
		case MMC_SWITCH_FUNC:  /* also SD_SWITCH_FUNC */
		case MMC_SET_BLOCKLEN:
		case MMC_READ_SINGLE_BLOCK:
		case MMC_READ_MULTIPLE_BLOCK:
		case MMC_WRITE_BLOCK:
		case MMC_WRITE_MULTIPLE_BLOCK:
		case MMC_PROGRAM_CSD:
		case MMC_SEND_WRITE_PROT:
		case SD_ERASE_WR_BLK_START:
		case SD_ERASE_WR_BLK_END:
		case MMC_LOCK_UNLOCK:
		case MMC_GEN_CMD:
			rsp_type = MMCSPI_RSP_R1;
			mmc_rsp_type = MMCSPI_TO_MMC_RSP_R1;
			break;
		case MMCSPI_CRC_ON_OFF:
			rsp_type = MMCSPI_RSP_R1;
			mmc_rsp_type = MMCSPI_TO_MMC_RSP_NONE;
			break;

		case MMC_SEND_CSD:
		case MMC_SEND_CID:
			arg = 0; /* no rca in spi mode */
			rsp_type = MMCSPI_RSP_R1;
			mmc_rsp_type = MMCSPI_TO_MMC_RSP_R2;
			ldata_len = 16;
			break;

		case MMC_APP_CMD:
			arg = 0; /* no rca in spi mode */
			rsp_type = MMCSPI_RSP_R1;
			mmc_rsp_type = MMCSPI_TO_MMC_RSP_R1;
			break;

		case MMC_STOP_TRANSMISSION:
		case MMC_SET_WRITE_PROT:
		case MMC_CLR_WRITE_PROT:
		case MMC_ERASE:
			rsp_type = MMCSPI_RSP_R1B;
			mmc_rsp_type = MMCSPI_TO_MMC_RSP_R1B;
			break;

		case MMC_ALL_SEND_CID:
			/* handle MMC_ALL_SEND_CID as MMC_SEND_CID */
			opcode = MMC_SEND_CID;
			rsp_type = MMCSPI_RSP_R1;
			mmc_rsp_type = MMCSPI_TO_MMC_RSP_R2;
			ldata_len = 16;
			break;

		case MMC_SEND_STATUS:
			arg = 0; /* no rca in spi mode */
			rsp_type = MMCSPI_RSP_R2;
			mmc_rsp_type = MMCSPI_TO_MMC_RSP_R1;
			break;


		case MMCSPI_READ_OCR:
			rsp_type = MMCSPI_RSP_R3;
			mmc_rsp_type = MMCSPI_TO_MMC_RSP_NONE;
			break;

		case SD_SEND_RELATIVE_ADDR:
			/*
			 * Handle SD_SEND_RELATIVE_ADDR as MMC_SEND_STATUS -
			 * the rca returned to the caller will always be 0.
			 */
			opcode = MMC_SEND_STATUS;
			rsp_type = MMCSPI_RSP_R2;
			mmc_rsp_type = MMCSPI_TO_MMC_RSP_R6;
			break;

		case SD_SEND_IF_COND:
			use_crc = 1;
			rsp_type = MMCSPI_RSP_R7;
			mmc_rsp_type = MMCSPI_TO_MMC_RSP_R7;
			break;

		default:
			TRACE(dev, ERROR, "Invalid cmd opcode %u\n", opcode);
			return (MMC_ERR_INVALID);
		}
	}

	switch (rsp_type) {
	case MMCSPI_RSP_R1:
	case MMCSPI_RSP_R1B:
		rsp_len = 1;
		break;
	case MMCSPI_RSP_R2:
		rsp_len = 2;
		break;
	case MMCSPI_RSP_R3:
	case MMCSPI_RSP_R7:
		rsp_len = 5;
		break;
	default:
		TRACE(dev, ERROR, "Unknown response type %u\n", rsp_type);
		return (MMC_ERR_INVALID);
	}

	mmcspi_cmd->mmc_cmd = mmc_cmd;
	mmcspi_cmd->opcode = opcode;
	mmcspi_cmd->arg = arg;
	mmcspi_cmd->flags = flags;
	mmcspi_cmd->retries = retries;
	mmcspi_cmd->use_crc = use_crc;
	mmcspi_cmd->error_mask = MMCSPI_R1_ERR_MASK;
	if (!mmcspi_cmd->use_crc)
		mmcspi_cmd->error_mask &= ~MMCSPI_R1_CRC_ERR;
	mmcspi_cmd->rsp_type = rsp_type;
	mmcspi_cmd->rsp_len = rsp_len;
	mmcspi_cmd->mmc_rsp_type = mmc_rsp_type;

	memset(&mmcspi_cmd->ldata, 0, sizeof(struct mmc_data));
	mmcspi_cmd->ldata.len = ldata_len;
	if (ldata_len) {
		mmcspi_cmd->ldata.data = sc->slot.ldata_buf;
		mmcspi_cmd->ldata.flags = MMC_DATA_READ;

		mmcspi_cmd->data = &mmcspi_cmd->ldata;
	} else
		mmcspi_cmd->data = mmc_cmd->data;

	return (MMC_ERR_NONE);
}

static unsigned int
mmcspi_send_cmd(device_t dev, struct mmcspi_command *cmd, uint8_t *rspbuf)
{
	unsigned int err;
	uint32_t opcode;
	uint32_t arg;
	uint8_t txbuf[8];
	uint8_t crc;

	TRACE_ENTER(dev);

	opcode = cmd->opcode;
	arg = cmd->arg;

	TRACE(dev, ACTION, "sending %sMD%u(0x%08x)\n",
	    cmd->flags & MMC_CMD_IS_APP ? "AC": "C", opcode, arg);

	/*
	 * Sending this byte ahead of each command prevents some cards from
	 * responding with unaligned data, and doesn't bother the others.
	 * Examples:
	 *
	 * Sandisk 32GB SDHC card, cid:
	 * mid=0x03 oid=0x5344 pnm="SU32G" prv=8.0 mdt=00.2000
	 */
	txbuf[0] = 0xff;

	txbuf[1] = 0x40 | (opcode & 0x3f);
	txbuf[2] = arg >> 24;
	txbuf[3] = (arg >> 16) & 0xff;
	txbuf[4] = (arg >> 8) & 0xff;
	txbuf[5] = arg & 0xff;

	if (cmd->use_crc)
		crc = update_crc7(CRC7_INITIAL, &txbuf[1], 5);
	else
		crc = 0;

	txbuf[6] = (crc << 1) | 0x01;

	 /*
	  * Some cards have garbage on the bus in the first byte slot after
	  * the last command byte.  This seems to be common with the stop
	  * command.  Clocking out an extra byte with the command will
	  * result in that data not being searched for the response token,
	  * which is ok, because no cards respond that fast.
	  */
	txbuf[7] = 0xff;

	err = mmcspi_do_spi_write(dev, txbuf, sizeof(txbuf), NULL, 0);
	if (MMC_ERR_NONE != err) {
		TRACE(dev, ERROR, "spi write of command failed\n");
		TRACE_EXIT(dev);
		return (err);
	}

	TRACE(dev, DETAILS,
	      "rx cmd bytes 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
	      junkbuf[0], junkbuf[1], junkbuf[2], junkbuf[3], junkbuf[4],
	      junkbuf[5] );
	TRACE(dev, DETAILS, "skipped response byte is 0x%02x\n", junkbuf[6]);

	err = mmcspi_get_response_token(dev, MMCSPI_R1_MASK, MMCSPI_R1_VALUE,
	    cmd->rsp_len, MMCSPI_RSP_R1B == cmd->rsp_type, rspbuf);

	if (MMC_ERR_NONE == err) {
		if (rspbuf[0] & cmd->error_mask & MMCSPI_R1_CRC_ERR)
			err = MMC_ERR_BADCRC;
		else if (rspbuf[0] & cmd->error_mask)
			err = MMC_ERR_INVALID;
	}

	TRACE_EXIT(dev);

	return (err);
}

static unsigned int
mmcspi_read_block(device_t dev, uint8_t *data, unsigned int len,
    unsigned int check_crc16, unsigned int check_crc7)
{
	struct bintime start;
	struct bintime elapsed;
	unsigned int non_token_bytes;
	unsigned int data_captured;
	unsigned int crc_captured;
	unsigned int pollbufpos;
	unsigned int crc16_mismatch;
	unsigned int err;
	uint16_t crc16, computed_crc16;
	uint8_t crc7, computed_crc7;
	uint8_t pollbuf[MMCSPI_POLL_LEN];
	uint8_t crcbuf[MMCSPI_DATA_CRC_LEN];
	int i;

	crc16_mismatch = 0;

	TRACE_ENTER(dev);
	TRACE(dev, ACTION, "read block(%u)\n", len);

	/*
	 * With this approach, we could pointlessly read up to
	 * (MMCSPI_POLL_LEN - 3 - len) bytes from the spi bus, but only in
	 * the odd situation where MMCSPI_POLL_LEN is greater than len + 3.
	 */
	getbintime(&start);
	do {
		TRACE(dev, DETAILS, "looking for read token\n");
		err = mmcspi_do_spi_read(dev, pollbuf, MMCSPI_POLL_LEN);
		if (MMC_ERR_NONE != err) {
			TRACE(dev, ERROR, "token read on spi failed\n");
			TRACE_EXIT(dev);
			return (err);
		}

		for (i = 0; i < MMCSPI_POLL_LEN; i++) {
			if (MMCSPI_TOKEN_SB == pollbuf[i]) {
				TRACE(dev, RESULT,
				      "found start block token at %d\n", i);
				break;
			} else if (MMCSPI_IS_DE_TOKEN(pollbuf[i])) {
				TRACE(dev, ERROR,
				      "found data error token at %d\n", i);
				TRACE_EXIT(dev);
				return (MMC_ERR_FAILED);
			}
		}

		getbintime(&elapsed);
		bintime_sub(&elapsed, &start);

		if (elapsed.sec > MMCSPI_TIMEOUT_SEC) {
			TRACE(dev, ERROR, "timeout while looking for read "
			    "token\n");
			return (MMC_ERR_TIMEOUT);
		}
	} while (MMCSPI_POLL_LEN == i);

	/* copy any data captured in tail of poll buf to data buf */
	non_token_bytes = MMCSPI_POLL_LEN - i - 1;
	data_captured = min(non_token_bytes, len);
	crc_captured = non_token_bytes - data_captured;
	pollbufpos = i + 1;

	TRACE(dev, DETAILS, "data bytes captured in pollbuf = %u\n",
	    data_captured);

	memcpy(data, &pollbuf[pollbufpos], data_captured);
	pollbufpos += data_captured;

	TRACE(dev, DETAILS, "data bytes to read = %u, crc_captured = %u\n",
	    len - data_captured, crc_captured);

	/* get any remaining data from the spi bus */
	if (data_captured < len) {
		err = mmcspi_do_spi_read(dev, &data[data_captured],
		    len - data_captured);
		if (MMC_ERR_NONE != err) {
			TRACE(dev, ERROR,
			      "spi read of remainder of block failed\n");
			TRACE_EXIT(dev);
			return (err);
		}
	}

	/* copy any crc captured in the poll buf to the crc buf */
	memcpy(crcbuf, &pollbuf[pollbufpos], crc_captured);

	/* get any remaining crc */
	if (crc_captured < MMCSPI_DATA_CRC_LEN) {
		TRACE(dev, DETAILS, "crc bytes to read = %u\n",
		    MMCSPI_DATA_CRC_LEN - crc_captured);

		err = mmcspi_do_spi_read(dev, &crcbuf[crc_captured],
		    MMCSPI_DATA_CRC_LEN - crc_captured);
		if (MMC_ERR_NONE != err) {
			TRACE(dev, ERROR, "spi read of crc failed\n");
			TRACE_EXIT(dev);
			return (err);
		}
	}

	/*
	 * The following crc checking code is deliberately structured to
	 * allow a passing crc-7 check to override a failing crc-16 check
	 * when both are enabled.
	 */
	if (check_crc16) {
		crc16 = ((uint16_t)crcbuf[0] << 8) | crcbuf[1];
		computed_crc16 = update_crc16(CRC16_INITIAL, data, len);
		TRACE(dev, RESULT, "sent_crc16=0x%04x computed_crc16=0x%04x\n",
		    crc16, computed_crc16);

		if (computed_crc16 != crc16) {
			crc16_mismatch = 1;

			TRACE(dev, ERROR, "crc16 mismatch, should be 0x%04x, "
			    " is 0x%04x\n", crc16, computed_crc16);

			if (!check_crc7) {
				TRACE_EXIT(dev);
				return (MMC_ERR_BADCRC);
			}
		}
	}

	if (check_crc7) {
		if (crc16_mismatch) {
			/*
			 * Let the user know something else is being checked
			 * after announcing an error above.
			 */
			TRACE(dev, ERROR, "checking crc7\n");
		}

		crc7 = data[len - 1] >> 1;
		computed_crc7 = update_crc7(CRC7_INITIAL, data, len - 1);
		TRACE(dev, RESULT, "sent_crc7=0x%02x computed_crc7=0x%02x\n",
		    crc7, computed_crc7);

		if (computed_crc7 != crc7) {
			TRACE(dev, ERROR,
			      "crc7 mismatch, should be 0x%02x, is 0x%02x\n",
			      crc7, computed_crc7);

			TRACE_EXIT(dev);
			return (MMC_ERR_BADCRC);
		}
	}

	TRACE_EXIT(dev);

	return (MMC_ERR_NONE);
}

static unsigned int
mmcspi_send_stop(device_t dev, unsigned int retries)
{
	struct mmcspi_command stop;
	struct mmc_command mmc_stop;
	uint8_t stop_response;
	unsigned int err;
	int i;

	TRACE_ENTER(dev);

	memset(&mmc_stop, 0, sizeof(mmc_stop));
	mmc_stop.opcode = MMC_STOP_TRANSMISSION;
	mmc_stop.flags = MMC_RSP_R1B | MMC_CMD_AC;

	err = mmcspi_set_up_command(dev, &stop, &mmc_stop);
	if (MMC_ERR_NONE != err) {
		TRACE_EXIT(dev);
		return (err);
	}

	/*
	 * Retry stop commands that fail due to bad crc here because having
	 * the caller retry the entire read/write command due to such a
	 * failure is pointlessly expensive.
	 */
	for (i = 0; i <= retries; i++) {
		TRACE(dev, ACTION, "sending stop message\n");

		err = mmcspi_send_cmd(dev, &stop, &stop_response);
		if (MMC_ERR_NONE != err) {
			TRACE_EXIT(dev);
			return (err);
		}

		TRACE(dev, RESULT, "stop response=0x%02x\n", stop_response);

		/* retry on crc error */
		if (stop_response & stop.error_mask & MMCSPI_R1_CRC_ERR) {
			continue;
		}
	}

	if (stop_response & stop.error_mask) {
		TRACE_EXIT(dev);

		/*
		 * Don't return MMC_ERR_BADCRC here, even if
		 * MMCSPI_R1_CRC_ERR is set, because that would trigger the
		 * caller's retry-on-crc-error mechanism, effectively
		 * squaring the maximum number of retries of the stop
		 * command.
		 */
		return (MMC_ERR_FAILED);
	}
	TRACE_EXIT(dev);

	return (MMC_ERR_NONE);
}

static unsigned int
mmcspi_read_phase(device_t dev, struct mmcspi_command *cmd)
{
	struct mmc_data *data;
	unsigned int data_offset;
	unsigned int num_blocks;
	unsigned int len;
	unsigned int err;
	uint8_t *data8;
	int i;

	TRACE_ENTER(dev);

	data = cmd->data;
	data8 = (uint8_t *)data->data;
	data_offset = 0;

	if (data->len < MMCSPI_DATA_BLOCK_LEN) {
		num_blocks = 1;
		len = data->len;
	} else {
		num_blocks = data->len / MMCSPI_DATA_BLOCK_LEN;
		len = MMCSPI_DATA_BLOCK_LEN;
	}

	for (i = 0; i < num_blocks; i++) {
		/*
		 * The CID and CSD data blocks contain both a trailing crc-7
		 * inside the data block and the standard crc-16 following
		 * the data block, so both are checked when use_crc is true.
		 *
		 * When crc checking has been enabled via CMD59, some cards
		 * send CID and CSD data blocks with correct crc-7 values
		 * but incorrect crc-16 values.  read_block will accept
		 * those responses as valid as long as the crc-7 is correct.
		 *
		 * Examples:
		 *
		 * Super Talent 1GB SDSC card, cid:
		 * mid=0x1b oid=0x534d pnm="00000" prv=1.0 mdt=02.2010
		 */
		err = mmcspi_read_block(dev, &data8[data_offset], len,
		    cmd->use_crc, cmd->use_crc && ((MMC_SEND_CID == cmd->opcode)
		    || (MMC_SEND_CSD == cmd->opcode)));

		if (MMC_ERR_NONE != err) {
			TRACE_EXIT(dev);
			return (err);
		}

		data_offset += MMCSPI_DATA_BLOCK_LEN;
	}

	/* multi-block read commands require a stop */
	if (num_blocks > 1) {
		err = mmcspi_send_stop(dev, cmd->retries);
		if (MMC_ERR_NONE != err) {
			TRACE_EXIT(dev);
			return (err);
		}
	}

	TRACE_EXIT(dev);

	return (MMC_ERR_NONE);
}

static unsigned int
mmcspi_write_block(device_t dev, uint8_t *data, unsigned int is_multi,
    unsigned char use_crc, uint8_t *status)
{
	uint8_t txbuf[max(MMCSPI_POLL_LEN, 2)];
	uint8_t response_token;
	unsigned int err;
	uint16_t crc;

	TRACE_ENTER(dev);

	if (use_crc)
		crc = update_crc16(CRC16_INITIAL, data, MMCSPI_DATA_BLOCK_LEN);
	else
		crc = 0;

	TRACE(dev, ACTION, "write block(512) crc=0x%04x\n", crc);

	txbuf[0] = is_multi ? MMCSPI_TOKEN_SB_WM : MMCSPI_TOKEN_SB;
	err = mmcspi_do_spi_write(dev, txbuf, 1, data, MMCSPI_DATA_BLOCK_LEN);
	if (MMC_ERR_NONE != err) {
		TRACE_EXIT(dev);
		return (err);
	}

	txbuf[0] = crc >> 8;
	txbuf[1] = crc & 0xff;
	err = mmcspi_do_spi_write(dev, txbuf, 2, NULL, 0);
	if (MMC_ERR_NONE != err) {
		TRACE_EXIT(dev);
		return (err);
	}

	err = mmcspi_get_response_token(dev, MMCSPI_DR_MASK, MMCSPI_DR_VALUE,
	    1, 1, &response_token);
	if (MMC_ERR_NONE != err) {
		TRACE_EXIT(dev);
		return (err);
	}

	*status = response_token & MMCSPI_DR_ERR_MASK;

	TRACE_EXIT(dev);

	return (MMC_ERR_NONE);
}

static unsigned int
mmcspi_write_phase(device_t dev, struct mmcspi_command *cmd)
{

	struct mmc_data *data;
	unsigned int data_offset;
	unsigned int num_blocks;
	unsigned int err;
	uint8_t *data8;
	uint8_t token[2];
	uint8_t status;
	int i;

	TRACE_ENTER(dev);

	data = cmd->data;

	data8 = (uint8_t *)data->data;
	data_offset = 0;
	num_blocks = data->len / MMCSPI_DATA_BLOCK_LEN;
	for (i = 0; i < num_blocks; i++) {
		err = mmcspi_write_block(dev, &data8[data_offset],
					 num_blocks > 1, cmd->use_crc, &status);

		if (MMC_ERR_NONE != err) {
			TRACE_EXIT(dev);
			return (err);
		}

		if (MMCSPI_DR_ERR_NONE != status) {
			if (num_blocks > 1) {
				/*
				 * Ignore any failure reported for the stop
				 * command, as the return status for the
				 * write phase will be whatever error was
				 * indicated in the data respone token.
				 */
				mmcspi_send_stop(dev, cmd->retries);
			}

			/*
			 * A CRC error can't be ignored here, even if crc
			 * use is disabled, as there is no way to simply
			 * carry on when a data error token has been sent.
			 */
			if (MMCSPI_DR_ERR_CRC == status) {
				TRACE_EXIT(dev);
				return (MMC_ERR_BADCRC);
			} else {
				TRACE_EXIT(dev);
				return (MMC_ERR_FAILED);
			}
		}

		data_offset += MMCSPI_DATA_BLOCK_LEN;
	}

	/* successful multi-block write commands require a stop token */
	if (num_blocks > 1) {
		TRACE(dev, ACTION, "Sending stop token\n");

		/*
		 * Most/all cards are a bit sluggish in assserting busy
		 * after receipt of the STOP_TRAN token. Clocking out an
		 * extra byte here provides a byte of dead time before
		 * looking for not busy, avoiding a premature not-busy
		 * determination with such cards.
		 */
		token[0] = MMCSPI_TOKEN_ST;
		token[1] = 0xff;

		err = mmcspi_do_spi_write(dev, token, sizeof(token), NULL, 0);
		if (MMC_ERR_NONE != err) {
			TRACE_EXIT(dev);
			return (err);
		}

		err = mmcspi_wait_for_not_busy(dev);
		if (MMC_ERR_NONE != err) {
			TRACE_EXIT(dev);
			return (err);
		}
	}

	TRACE_EXIT(dev);

	return (MMC_ERR_NONE);
}

static unsigned int
mmcspi_translate_response(device_t dev, struct mmcspi_command *cmd,
    uint8_t *rspbuf)
{
	struct mmc_command *mmc_cmd;
	uint32_t mmc_rsp_type;
	uint8_t *ldata;

	mmc_cmd = cmd->mmc_cmd;
	mmc_rsp_type = cmd->mmc_rsp_type;
	ldata = cmd->ldata.data;

	TRACE_ENTER(dev);

	TRACE(dev, ACTION, "translating SPI rsp %u to SD rsp %u\n",
	    cmd->rsp_type, mmc_rsp_type);

	if ((MMCSPI_TO_MMC_RSP_R1 == mmc_rsp_type) ||
	    (MMCSPI_TO_MMC_RSP_R1B == mmc_rsp_type)) {

		TRACE(dev, ACTION, "translating SPI-R1/2 to SD-R1\n");

		if ((MMCSPI_RSP_R1 == cmd->rsp_type) ||
		    (MMCSPI_RSP_R1B == cmd->rsp_type) ||
		    (MMCSPI_RSP_R2 == cmd->rsp_type)) {
			mmc_cmd->resp[0] = 0;

			if (rspbuf[0] & MMCSPI_R1_PARAM_ERR)
				mmc_cmd->resp[0] |= R1_OUT_OF_RANGE;

			if (rspbuf[0] & MMCSPI_R1_ADDR_ERR)
				mmc_cmd->resp[0] |= R1_ADDRESS_ERROR;

			if (rspbuf[0] & MMCSPI_R1_ERASE_ERR)
				mmc_cmd->resp[0] |= R1_ERASE_SEQ_ERROR;

			if (rspbuf[0] & MMCSPI_R1_CRC_ERR)
				mmc_cmd->resp[0] |= R1_COM_CRC_ERROR;

			if (rspbuf[0] & MMCSPI_R1_ILL_CMD)
				mmc_cmd->resp[0] |= R1_ILLEGAL_COMMAND;

			if (rspbuf[0] & MMCSPI_R1_ERASE_RST)
				mmc_cmd->resp[0] |= R1_ERASE_RESET;

			if (rspbuf[0] & MMCSPI_R1_IDLE)
				mmc_cmd->resp[0] |=
				    (uint32_t)R1_STATE_IDLE << 9;
			else
				mmc_cmd->resp[0] |=
				    (uint32_t)R1_STATE_READY << 9;

			/* When MMC_CMD_IS_APP is sent, emulate R1_APP_CMD
			   SD-bus status bit. */
			if (!(cmd->flags & MMC_CMD_IS_APP) &&
			    (MMC_APP_CMD == cmd->opcode))
				mmc_cmd->resp[0] |= R1_APP_CMD;

			if (MMCSPI_RSP_R2 == cmd->rsp_type) {
				if (rspbuf[1] & MMCSPI_R2_OOR_CSD_OW)
					mmc_cmd->resp[0] |=
					    R1_OUT_OF_RANGE |
					    R1_CSD_OVERWRITE;

				if (rspbuf[1] & MMCSPI_R2_ERASE_PARAM)
					mmc_cmd->resp[0] |= R1_ERASE_PARAM;

				if (rspbuf[1] & MMCSPI_R2_WP_VIOLATE)
					mmc_cmd->resp[0] |= R1_WP_VIOLATION;

				if (rspbuf[1] & MMCSPI_R2_ECC_FAIL)
					mmc_cmd->resp[0] |= R1_CARD_ECC_FAILED;

				if (rspbuf[1] & MMCSPI_R2_CC_ERR)
					mmc_cmd->resp[0] |= R1_CC_ERROR;

				if (rspbuf[1] & MMCSPI_R2_ERR)
					mmc_cmd->resp[0] |= R1_ERROR;

				if (rspbuf[1] & MMCSPI_R2_WP_ER_LCK)
					mmc_cmd->resp[0] |=
					    R1_LOCK_UNLOCK_FAILED |
					    R1_WP_ERASE_SKIP;

				if (rspbuf[1] & MMCSPI_R2_LOCKED)
					mmc_cmd->resp[0] |= R1_CARD_IS_LOCKED;

			}
		} else
			return (MMC_ERR_INVALID);

	} else if (MMCSPI_TO_MMC_RSP_R2 == mmc_rsp_type) {

		if (16 == cmd->ldata.len) {

			TRACE(dev, ACTION, "translating SPI-R1/ldata(16) "
			    "to SD-R2\n");

			/* ldata contains bits 127:0 of the spi response */

			mmc_cmd->resp[0] =
			    (uint32_t)ldata[0] << 24 |
			    (uint32_t)ldata[1] << 16 |
			    (uint32_t)ldata[2] << 8  |
			    (uint32_t)ldata[3];

			mmc_cmd->resp[1] =
			    (uint32_t)ldata[4] << 24 |
			    (uint32_t)ldata[5] << 16 |
			    (uint32_t)ldata[6] << 8  |
			    (uint32_t)ldata[7];

			mmc_cmd->resp[2] =
			    (uint32_t)ldata[8] << 24 |
			    (uint32_t)ldata[9] << 16 |
			    (uint32_t)ldata[10] << 8  |
			    (uint32_t)ldata[11];

			mmc_cmd->resp[3] =
			    (uint32_t)ldata[12] << 24 |
			    (uint32_t)ldata[13] << 16 |
			    (uint32_t)ldata[14] <<  8;

		} else
			return (MMC_ERR_INVALID);

	} else if (MMCSPI_TO_MMC_RSP_R3 == mmc_rsp_type) {

		if (MMCSPI_RSP_R3 == cmd->rsp_type) {

			TRACE(dev, ACTION, "translating SPI-R3 to SD-R3\n");

			/* rspbuf contains a 40-bit spi-R3 from the
			   MMCSPI_READ_OCR response, of which bits 31:0 are
			   the OCR value */

			/* spi  response bits 31:0 mapped to
			   sdhc register bits 31:0 */
			mmc_cmd->resp[0] =
			    (uint32_t)rspbuf[1] << 24 |
			    (uint32_t)rspbuf[2] << 16 |
			    (uint32_t)rspbuf[3] << 8  |
			    (uint32_t)rspbuf[4];

			/* Clear card busy bit (indicating busy) if the
			   SPI-R1 idle bit is set. */
			if (rspbuf[0] & MMCSPI_R1_IDLE) {
				mmc_cmd->resp[0] &= ~MMC_OCR_CARD_BUSY;
			} else {
				mmc_cmd->resp[0] |= MMC_OCR_CARD_BUSY;
			}

			TRACE(dev, DETAILS, "ocr=0x%08x\n", mmc_cmd->resp[0]);
		} else
			return (MMC_ERR_INVALID);

	} else if (MMCSPI_TO_MMC_RSP_R6 == mmc_rsp_type) {
		if (MMCSPI_RSP_R2 == cmd->rsp_type) {

			TRACE(dev, ACTION, "translating SPI-R2 to SD-R6\n");

			/* rca returned will always be zero */
			mmc_cmd->resp[0] = 0;

			if (rspbuf[0] & MMCSPI_R1_CRC_ERR)
				mmc_cmd->resp[0] |= 0x8000;

			if (rspbuf[0] & MMCSPI_R1_ILL_CMD)
				mmc_cmd->resp[0] |= 0x4000;

			if (rspbuf[1] & MMCSPI_R2_ERR)
				mmc_cmd->resp[0] |= 0x2000;

			if (rspbuf[0] & MMCSPI_R1_IDLE)
				mmc_cmd->resp[0] |=
				    (uint32_t)R1_STATE_IDLE << 9;
			else
				mmc_cmd->resp[0] |=
				    (uint32_t)R1_STATE_READY << 9;
		} else
			return (MMC_ERR_INVALID);

	} else if (MMCSPI_TO_MMC_RSP_R7 == mmc_rsp_type) {
		if (MMCSPI_RSP_R7 == cmd->rsp_type) {

			TRACE(dev, ACTION, "translating SPI-R7 to SD-R7\n");

			/* rsp buf contains a 40-bit spi-R7, of which bits
			   11:0 need to be transferred */

			/* spi  response bits 11:0 mapped to
			   sdhc register bits 11:0 */
			mmc_cmd->resp[0] =
			    (uint32_t)(rspbuf[3] & 0xf) << 8 |
			    (uint32_t)rspbuf[4];
		} else
			return (MMC_ERR_INVALID);

	} else if (MMCSPI_TO_MMC_RSP_NONE != mmc_rsp_type)
		return (MMC_ERR_INVALID);

	TRACE_EXIT(dev);

	return (MMC_ERR_NONE);
}

static unsigned int
mmcspi_get_ocr(device_t dev, uint8_t *ocrbuf)
{
	struct mmc_command mmc_cmd;
	struct mmcspi_command cmd;
	unsigned int err;
	uint8_t r1_status;
	uint8_t rspbuf[MMCSPI_MAX_RSP_LEN];

	TRACE_ENTER(dev);

	memset(&mmc_cmd, 0, sizeof(struct mmc_command));
	mmc_cmd.opcode = MMCSPI_READ_OCR;
	mmc_cmd.flags = MMC_RSP_R3 | MMC_CMD_AC;

	err = mmcspi_set_up_command(dev, &cmd, &mmc_cmd);
	if (MMC_ERR_NONE != err) {
		TRACE_EXIT(dev);
		return (err);
	}

	err = mmcspi_send_cmd(dev, &cmd, rspbuf);
	if (MMC_ERR_NONE != err) {
		TRACE_EXIT(dev);
		return (err);
	}

	r1_status = rspbuf[0] & cmd.error_mask;
	if (r1_status) {
		if (r1_status & MMCSPI_R1_CRC_ERR)
			err = MMC_ERR_BADCRC;
		else
			err = MMC_ERR_INVALID;

		TRACE_EXIT(dev);
		return (err);
	}

	memcpy(ocrbuf, &rspbuf[1], MMCSPI_OCR_LEN);

	TRACE_EXIT(dev);

	return (MMC_ERR_NONE);
}

static unsigned int
mmcspi_set_crc_on_off(device_t dev, unsigned int crc_on)
{
	struct mmc_command mmc_cmd;
	struct mmcspi_command cmd;
	unsigned int err;
	uint8_t r1_status;
	uint8_t rspbuf[MMCSPI_MAX_RSP_LEN];

	TRACE_ENTER(dev);

	memset(&mmc_cmd, 0, sizeof(struct mmc_command));
	mmc_cmd.opcode = MMCSPI_CRC_ON_OFF;
	mmc_cmd.arg = crc_on ? 1 : 0;
	mmc_cmd.flags = MMC_RSP_NONE | MMC_CMD_AC;

	err = mmcspi_set_up_command(dev, &cmd, &mmc_cmd);
	if (MMC_ERR_NONE != err) {
		TRACE_EXIT(dev);
		return (err);
	}

	err = mmcspi_send_cmd(dev, &cmd, rspbuf);
	if (MMC_ERR_NONE != err) {
		TRACE_EXIT(dev);
		return (err);
	}

	r1_status = rspbuf[0] & cmd.error_mask;
	if (r1_status) {
		if (r1_status & MMCSPI_R1_CRC_ERR)
			err = MMC_ERR_BADCRC;
		else
			err = MMC_ERR_INVALID;

		TRACE_EXIT(dev);
		return (err);
	}

	TRACE_EXIT(dev);
	return (MMC_ERR_NONE);
}

static unsigned int
mmcspi_update_crc_setting(device_t dev, unsigned int crc_on)
{
	struct mmcspi_softc *sc;
	struct mmcspi_slot *slot;
	unsigned int err;
	int i;

	TRACE_ENTER(dev);

	sc = device_get_softc(dev);
	slot = &sc->slot;

	for (i = 0; i <= MMCSPI_RETRIES; i++) {
		err = mmcspi_set_crc_on_off(dev, crc_on);
		if (MMC_ERR_BADCRC != err)
			break;
	}

	if (MMC_ERR_NONE != err) {
		TRACE_EXIT(dev);
		return (err);
	}

	if (crc_on)
		slot->crc_enabled = 1;
	else
		slot->crc_enabled = 0;

	TRACE_EXIT(dev);

	return (MMC_ERR_NONE);
}

static int
mmcspi_request(device_t brdev, device_t reqdev, struct mmc_request *req)
{
	TRACE_ENTER(brdev);

	struct mmcspi_softc *sc = device_get_softc(brdev);
	struct mmcspi_slot *slot = &sc->slot;
	struct mmcspi_command cmd;
	struct mmc_command *mmc_cmd = req->cmd;
	struct mmc_data *data;
	unsigned int err;
	unsigned int use_crc_sample;
	int i, j;
	uint32_t opcode;
	uint32_t flags;
	uint32_t last_opcode;
	uint32_t last_flags;
	uint8_t rspbuf[MMCSPI_MAX_RSP_LEN];

#define	IS_CMD(code, cmd, flags)	\
	(!((flags) & MMC_CMD_IS_APP) && ((code) == (cmd)))
#define	IS_ACMD(code, cmd, flags)	\
	(((flags) & MMC_CMD_IS_APP) && ((code) == (cmd)))

	if (power_on != slot->host.ios.power_mode)
		return (MMC_ERR_INVALID);

	/*
	 * Sample use_crc sysctl and adjust card setting if required and
	 * appropriate.
	 */
	use_crc_sample = sc->use_crc;
	if (slot->crc_init_done &&
	    (use_crc_sample != slot->crc_enabled)) {
		err = mmcspi_update_crc_setting(brdev, use_crc_sample);
		if (MMC_ERR_NONE != err)
			goto out;
		slot->crc_init_done = 1;
	}

	err = mmcspi_set_up_command(brdev, &cmd, mmc_cmd);
	if (MMC_ERR_NONE != err)
		goto out;

	opcode = cmd.opcode;
	flags = cmd.flags;
	data = cmd.data;

	last_opcode = slot->last_opcode;
	last_flags = slot->last_flags;

	/* enforce restrictions on request parameters */
	if (data) {
		/*
		 * All writes must be a multiple of the block length. All
		 * reads greater than the block length must be a multiple of
		 * the block length.
		 */
		if ((data->len % MMCSPI_DATA_BLOCK_LEN) &&
		    !((data->flags & MMC_DATA_READ) &&
		      (data->len < MMCSPI_DATA_BLOCK_LEN))) {
			TRACE(brdev, ERROR,
			      "requested data phase not a multiple of %u\n",
			      MMCSPI_DATA_BLOCK_LEN);
			err = MMC_ERR_INVALID;
			goto out;
		}

		if (((data->flags & MMC_DATA_READ) &&
		     (data->flags & MMC_DATA_WRITE)) ||
		    (data->flags & MMC_DATA_STREAM)) {
			TRACE(brdev, ERROR, "illegal data phase flags 0x%02x\n",
			      data->flags);
			err = MMC_ERR_INVALID;
			goto out;
		}
	}

	for (i = 0; i <= cmd.retries; i++) {
		/*
		 * On the next command following a CMD8, collect the OCR and
		 * save it off for use in the next ACMD41.
		 */
		if (IS_CMD(SD_SEND_IF_COND, last_opcode, last_flags)) {
			err = mmcspi_get_ocr(brdev, slot->last_ocr);
			if (MMC_ERR_NONE != err) {
				if (MMC_ERR_BADCRC == err)
					continue;
				goto out;
			}
		}

		err = mmcspi_send_cmd(brdev, &cmd, rspbuf);
		if (MMC_ERR_NONE != err) {
			if (MMC_ERR_BADCRC == err)
				continue;
			goto out;
		}

		if (data) {
			if (data->flags & MMC_DATA_READ)
				err = mmcspi_read_phase(brdev, &cmd);
			else /* MMC_DATA_WRITE */
				err = mmcspi_write_phase(brdev, &cmd);
			if (MMC_ERR_NONE != err) {
				if (MMC_ERR_BADCRC == err)
					continue;
				goto out;
			}
		}
		break;
	}

	if (MMC_ERR_NONE != err)
		goto out;

	/*
	 * If this was an ACMD_SD_SEND_OP_COND or MMC_SEND_OP_COND, we need
	 * to return an OCR value in the result.  If the response from the
	 * card indicates it is still in the IDLE state, supply the OCR
	 * value obtained after the last CMD8, otherwise issue an
	 * MMCSPI_READ_OCR to get the current value, which will have a valid
	 * CCS bit.
	 *
	 * This dance is required under this emulation approach because the
	 * spec stipulates that no other commands should be sent while
	 * ACMD_SD_SEND_OP_COND is being used to poll for the end of the
	 * IDLE state, and some cards do enforce that requirement.
	 */
	if (IS_ACMD(ACMD_SD_SEND_OP_COND, opcode, flags) ||
	    IS_CMD(MMC_SEND_OP_COND, opcode, flags)) {

		if (rspbuf[0] & MMCSPI_R1_IDLE)
			memcpy(&rspbuf[1], slot->last_ocr, MMCSPI_OCR_LEN);
		else {

			/*
			 * Some cards won't accept the MMCSPI_CRC_ON_OFF
			 * command until initialization is complete.
			 *
			 * Examples:
			 *
			 * Super Talent 1GB SDSC card, cid:
			 * mid=0x1b oid=0x534d pnm="00000" prv=1.0 mdt=02.2010
			 */
			if (!slot->crc_init_done) {
				err = mmcspi_update_crc_setting(brdev,
								sc->use_crc);
				if (MMC_ERR_NONE != err)
					goto out;
				slot->crc_init_done = 1;
			}

			for (j = 0; j <= cmd.retries; j++) {
				/*
				 * Note that in this case, we pass on the R1
				 * from READ_OCR.
				 */
				err = mmcspi_get_ocr(brdev, rspbuf);
				if (MMC_ERR_NONE != err) {
					if (MMC_ERR_BADCRC == err)
						continue;

					goto out;
				}

			}

			if (MMC_ERR_NONE != err)
				goto out;

		}

		/* adjust the SPI response type to include the OCR */
		cmd.rsp_type = MMCSPI_RSP_R3;
	}

	err = mmcspi_translate_response(brdev, &cmd, rspbuf);
	if (MMC_ERR_NONE != err)
		goto out;

 out:
	slot->last_opcode = mmc_cmd->opcode;
	slot->last_flags = mmc_cmd->flags;

	mmc_cmd->error = err;

	if (req->done)
		req->done(req);

	TRACE_EXIT(brdev);

	return (err);
}

static int
mmcspi_get_ro(device_t brdev, device_t reqdev)
{

	TRACE_ENTER(brdev);
	TRACE_EXIT(brdev);

	/* XXX no support for this currently */
	return (0);
}

static int
mmcspi_acquire_host(device_t brdev, device_t reqdev)
{
	struct mmcspi_slot *slot;
	int err;

	TRACE_ENTER(brdev);
	err = 0;

	slot = device_get_ivars(reqdev);

	MMCSPI_LOCK_SLOT(slot);
	while (slot->bus_busy)
		mtx_sleep(slot, &slot->mtx, 0, "mmcspiah", 0);
	slot->bus_busy++;
	MMCSPI_UNLOCK_SLOT(slot);

	TRACE_EXIT(brdev);

	return (err);
}

static int
mmcspi_release_host(device_t brdev, device_t reqdev)
{
	struct mmcspi_slot *slot;

	TRACE_ENTER(brdev);

	slot = device_get_ivars(reqdev);

	MMCSPI_LOCK_SLOT(slot);
	slot->bus_busy--;
	MMCSPI_UNLOCK_SLOT(slot);

	wakeup(slot);

	TRACE_EXIT(brdev);

	return (0);
}

static int
mmcspi_modevent_handler(module_t mod, int what, void *arg)
{

	switch (what) {
	case MOD_LOAD:
		init_crc7tab();
		init_crc16tab();
		memset(onesbuf, 0xff, sizeof(onesbuf));
		break;
	default:
		return (EOPNOTSUPP);
	}

	return (0);
}

static int
mmcspi_switch_vccq(device_t bus, device_t child)
{

	return (0);
}

#if defined(MMCSPI_ENABLE_DEBUG_FUNCS)
static void
mmcspi_dump_data(device_t dev, const char *label, uint8_t *data,
    unsigned int len)
{
	unsigned int i, j;
	unsigned int num_lines;
	unsigned int residual;

	TRACE_ENTER(dev);

	num_lines = len / 16;
	residual = len - 16 * num_lines;

	for(i = 0; i < num_lines; i++) {
		device_printf(dev, "%s:", label);
		for(j = 0; j < 16; j++)
			printf(" %02x", data[i * 16 + j]);
		printf("\n");
	}

	if (residual) {
		device_printf(dev, "%s:", label);
		for(j = 0; j < residual; j++)
			printf(" %02x", data[num_lines * 16 + j]);
		printf("\n");
	}

	TRACE_EXIT(dev);
}

static void
mmcspi_dump_spi_bus(device_t dev, unsigned int len)
{
	unsigned int num_blocks;
	unsigned int residual;
	unsigned int i;

	TRACE_ENTER(dev);

	num_blocks = len / MMCSPI_DATA_BLOCK_LEN;
	residual = len - num_blocks * MMCSPI_DATA_BLOCK_LEN;

	for (i = 0; i < num_blocks; i++) {
		if (MMC_ERR_NONE != mmcspi_do_spi_read(dev, junkbuf,
		    MMCSPI_DATA_BLOCK_LEN)) {
			device_printf(dev, "spi read failed\n");
			return;
		}

		mmcspi_dump_data(dev, "bus_data", junkbuf,
		    MMCSPI_DATA_BLOCK_LEN);
	}

	if (residual) {
		if (MMC_ERR_NONE != mmcspi_do_spi_read(dev, junkbuf,
		    residual)) {
			device_printf(dev, "spi read failed\n");
			return;
		}

		mmcspi_dump_data(dev, "bus_data", junkbuf, residual);
	}

	TRACE_EXIT(dev);
}
#endif

static device_method_t mmcspi_methods[] = {
	/* device_if */
	DEVMETHOD(device_probe,		mmcspi_probe),
	DEVMETHOD(device_attach,	mmcspi_attach),
	DEVMETHOD(device_detach,	mmcspi_detach),
	DEVMETHOD(device_suspend,	mmcspi_suspend),
	DEVMETHOD(device_resume,	mmcspi_resume),

	/* Bus interface */
	DEVMETHOD(bus_read_ivar,	mmcspi_read_ivar),
	DEVMETHOD(bus_write_ivar,	mmcspi_write_ivar),

	/* mmcbr_if */
	DEVMETHOD(mmcbr_update_ios,	mmcspi_update_ios),
	DEVMETHOD(mmcbr_request,	mmcspi_request),
	DEVMETHOD(mmcbr_get_ro,		mmcspi_get_ro),
	DEVMETHOD(mmcbr_acquire_host,	mmcspi_acquire_host),
	DEVMETHOD(mmcbr_release_host,	mmcspi_release_host),
	DEVMETHOD(mmcbr_switch_vccq,	mmcspi_switch_vccq),

	{0, 0},
};

static driver_t mmcspi_driver = {
	"mmcspi",
	mmcspi_methods,
	sizeof(struct mmcspi_softc),
};

DRIVER_MODULE(mmcspi, spibus, mmcspi_driver, mmcspi_modevent_handler, NULL);
MODULE_DEPEND(mmcspi, spibus, 1, 1, 1);
MMC_DECLARE_BRIDGE(mmcspi);
#ifdef FDT
SPIBUS_FDT_PNP_INFO(compat_data);
#endif

/*
 * Copyright (c) 2026 Abdelkader Boudih <freebsd@seuros.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * fwdv(4) - AV/C DV capture driver for FireWire camcorders
 *
 * References:
 *   AV/C General Specification 4.1 (TA Document 2001012)
 *   IEC 61883-1, IEC 61883-2
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <sys/fcntl.h>
#include <sys/poll.h>
#include <sys/selinfo.h>
#include <sys/uio.h>
#include <sys/mbuf.h>

#include <dev/firewire/firewire.h>
#include <dev/firewire/firewirereg.h>
#include <dev/firewire/iec13213.h>
#include <dev/firewire/iec68113.h>
#include <dev/firewire/fwdv.h>
#include <dev/firewire/fw_helpers.h>

static MALLOC_DEFINE(M_FWDV, "fwdv", "AV/C DV over FireWire");

static int debug = 0;
static int iso_channel = FWDV_DEFAULT_ISO_CH;
SYSCTL_DECL(_hw_firewire);
static SYSCTL_NODE(_hw_firewire, OID_AUTO, fwdv, CTLFLAG_RD | CTLFLAG_MPSAFE,
    0, "AV/C DV");
SYSCTL_INT(_hw_firewire_fwdv, OID_AUTO, debug, CTLFLAG_RWTUN, &debug, 0,
    "fwdv debug level");
SYSCTL_INT(_hw_firewire_fwdv, OID_AUTO, iso_channel, CTLFLAG_RWTUN,
    &iso_channel, 0, "ISO channel for DV receive");

#define FWDV_DEBUG(lev, fmt, ...)					\
	do {								\
		if (debug >= (lev))					\
			printf("fwdv: " fmt, ## __VA_ARGS__);		\
	} while (0)

static int	fwdv_probe(device_t);
static int	fwdv_attach(device_t);
static int	fwdv_detach(device_t);
static void	fwdv_probe_task(void *, int);

static int	fwdv_avc_command(struct fwdv_softc *, const uint8_t *, int,
		    uint8_t *, int *);
static int	fwdv_avc_unit_info(struct fwdv_softc *);
static int	fwdv_avc_play(struct fwdv_softc *);
static int	fwdv_avc_stop(struct fwdv_softc *);

static int	fwdv_read_opcr(struct fwdv_softc *, int, uint32_t *);

static int	fwdv_iso_start(struct fwdv_softc *);
static void	fwdv_iso_stop(struct fwdv_softc *);
static void	fwdv_iso_input(struct fw_xferq *);

static void	fwdv_fcp_handler(struct fw_xfer *);

static d_open_t		fwdv_cdev_open;
static d_close_t	fwdv_cdev_close;
static d_read_t		fwdv_cdev_read;
static d_poll_t		fwdv_cdev_poll;
static d_ioctl_t	fwdv_cdev_ioctl;

static struct cdevsw fwdv_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_TRACKCLOSE,
	.d_open =	fwdv_cdev_open,
	.d_close =	fwdv_cdev_close,
	.d_read =	fwdv_cdev_read,
	.d_poll =	fwdv_cdev_poll,
	.d_ioctl =	fwdv_cdev_ioctl,
	.d_name =	"fwdv",
};

static int
fwdv_read_quadlet(struct fwdv_softc *sc, uint16_t addr_hi, uint32_t addr_lo,
    uint32_t *val)
{
	uint16_t dst;
	uint8_t spd;

	FWDV_LOCK(sc);
	if (sc->fwdev == NULL) {
		FWDV_UNLOCK(sc);
		return (ENXIO);
	}
	dst = FWLOCALBUS | sc->fwdev->dst;
	spd = min(sc->fwdev->speed, FWSPD_S400);
	FWDV_UNLOCK(sc);

	return (fw_read_quadlet(sc->fd.fc, M_FWDV, dst, spd,
	    addr_hi, addr_lo, val));
}

static void
fwdv_fcp_handler(struct fw_xfer *xfer)
{
	struct fwdv_softc *sc = (struct fwdv_softc *)xfer->sc;
	struct fw_pkt *fp;
	uint16_t src_id;
	int len;

	fp = &xfer->recv.hdr;

	if (xfer->resp != 0) {
		FWDV_DEBUG(1, "FCP response error: %d\n", xfer->resp);
		goto done;
	}

	len = fp->mode.wreqb.len;
	if (len < 4 || len > FCP_MAX_FRAME_LEN) {
		FWDV_DEBUG(1, "FCP response bad len: %d\n", len);
		goto done;
	}

	if (xfer->recv.pay_len < 4) {
		FWDV_DEBUG(1, "FCP response short payload: %d\n",
		    xfer->recv.pay_len);
		goto done;
	}
	if (xfer->recv.pay_len < len)
		len = xfer->recv.pay_len;

	src_id = fp->mode.wreqb.src;
	FWDV_LOCK(sc);
	if (sc->fwdev == NULL ||
	    (FWLOCALBUS | sc->fwdev->dst) != src_id) {
		FWDV_UNLOCK(sc);
		FWDV_DEBUG(2, "FCP response from unknown node 0x%04x\n",
		    src_id);
		goto done;
	}

	if (xfer->recv.payload != NULL) {
		memcpy(sc->fcp_resp, xfer->recv.payload,
		    min(len, FCP_MAX_FRAME_LEN));
		sc->fcp_resp_len = len;
		sc->fcp_resp_ready = 1;
		wakeup(&sc->fcp_resp_ready);
	}
	FWDV_UNLOCK(sc);

done:
	STAILQ_INSERT_TAIL(&sc->fcp_bind.xferlist, xfer, link);
}

static int
fwdv_avc_command(struct fwdv_softc *sc, const uint8_t *cmd, int cmd_len,
    uint8_t *resp, int *resp_len)
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;
	uint16_t dst;
	uint8_t spd;
	int err, padded_len;

	FWDV_LOCK(sc);
	while (sc->avc_busy) {
		if (sc->state == FWDV_STATE_DETACHING) {
			FWDV_UNLOCK(sc);
			return (ENXIO);
		}
		msleep(&sc->avc_busy, &sc->mtx, PWAIT, "fwdvavc", hz);
	}
	sc->avc_busy = 1;

	if (sc->fwdev == NULL) {
		sc->avc_busy = 0;
		wakeup(&sc->avc_busy);
		FWDV_UNLOCK(sc);
		return (ENXIO);
	}
	dst = FWLOCALBUS | sc->fwdev->dst;
	spd = min(sc->fwdev->speed, FWSPD_S400);
	FWDV_UNLOCK(sc);

	padded_len = roundup2(cmd_len, 4);

	xfer = fw_xfer_alloc_buf(M_FWDV, padded_len, 0);
	if (xfer == NULL) {
		err = ENOMEM;
		goto done;
	}

	xfer->fc = sc->fd.fc;
	xfer->hand = fw_xferwake;

	fp = &xfer->send.hdr;
	fp->mode.wreqb.tcode = FWTCODE_WREQB;
	fp->mode.wreqb.dst = dst;
	fp->mode.wreqb.dest_hi = FCP_COMMAND_ADDR >> 32;
	fp->mode.wreqb.dest_lo = FCP_COMMAND_ADDR & 0xffffffff;
	fp->mode.wreqb.len = padded_len;
	fp->mode.wreqb.extcode = 0;
	xfer->send.spd = spd;
	xfer->send.pay_len = padded_len;

	memcpy(xfer->send.payload, cmd, cmd_len);
	if (padded_len > cmd_len)
		memset((uint8_t *)xfer->send.payload + cmd_len, 0,
		    padded_len - cmd_len);

	FWDV_LOCK(sc);
	sc->fcp_resp_ready = 0;
	sc->fcp_resp_len = 0;
	FWDV_UNLOCK(sc);

	FWDV_DEBUG(2, "AVC cmd: %02x %02x %02x %02x (len=%d)\n",
	    cmd[0], cmd[1], cmd_len > 2 ? cmd[2] : 0,
	    cmd_len > 3 ? cmd[3] : 0, cmd_len);

	err = fw_xfer_request_wait(xfer->fc, xfer, 2 * hz);
	if (err != 0) {
		fw_xfer_free_buf(xfer);
		goto done;
	}

	if (xfer->resp != 0 ||
	    xfer->recv.hdr.mode.wres.rtcode != FWRCODE_COMPLETE) {
		FWDV_DEBUG(1, "FCP write failed: resp=%d rtcode=%d\n",
		    xfer->resp, xfer->recv.hdr.mode.wres.rtcode);
		err = EIO;
	}
	fw_xfer_free_buf(xfer);
	if (err != 0)
		goto done;

	FWDV_LOCK(sc);
	for (int interim_retry = 0; interim_retry < 3; interim_retry++) {
		while (!sc->fcp_resp_ready) {
			if (sc->state == FWDV_STATE_DETACHING ||
			    sc->fwdev == NULL) {
				err = ENXIO;
				goto done_locked;
			}
			err = msleep(&sc->fcp_resp_ready, &sc->mtx, PCATCH,
			    "fwdvfcp", 3 * hz);
			if (err) {
				err = (err == EWOULDBLOCK) ? ETIMEDOUT : err;
				goto done_locked;
			}
		}

		if (sc->fcp_resp_len >= 1 &&
		    (sc->fcp_resp[0] >> 4) == AVC_RESP_INTERIM) {
			FWDV_DEBUG(2, "AVC INTERIM, waiting for final\n");
			sc->fcp_resp_ready = 0;
			if (interim_retry == 2) {
				FWDV_DEBUG(1, "AVC INTERIM timeout\n");
				err = ETIMEDOUT;
				goto done_locked;
			}
			continue;
		}
		break;
	}

	if (resp != NULL && resp_len != NULL) {
		int copy_len = min(sc->fcp_resp_len, *resp_len);
		memcpy(resp, sc->fcp_resp, copy_len);
		*resp_len = copy_len;
	}
	FWDV_UNLOCK(sc);

	FWDV_DEBUG(2, "AVC resp: %02x %02x %02x %02x (len=%d)\n",
	    sc->fcp_resp[0], sc->fcp_resp[1],
	    sc->fcp_resp_len > 2 ? sc->fcp_resp[2] : 0,
	    sc->fcp_resp_len > 3 ? sc->fcp_resp[3] : 0,
	    sc->fcp_resp_len);

	FWDV_LOCK(sc);
	sc->avc_busy = 0;
	wakeup(&sc->avc_busy);
	FWDV_UNLOCK(sc);

	return (0);

done_locked:
	sc->avc_busy = 0;
	wakeup(&sc->avc_busy);
	FWDV_UNLOCK(sc);
	return (err);

done:
	FWDV_LOCK(sc);
	sc->avc_busy = 0;
	wakeup(&sc->avc_busy);
	FWDV_UNLOCK(sc);
	return (err);
}

static int
fwdv_avc_unit_info(struct fwdv_softc *sc)
{
	uint8_t cmd[8], resp[8];
	int resp_len, err;

	cmd[0] = (AVC_CTYPE_STATUS << 4) | 0;
	cmd[1] = AVC_ADDR_UNIT;
	cmd[2] = AVC_OP_UNIT_INFO;
	cmd[3] = 0xff;
	cmd[4] = 0xff;
	cmd[5] = 0xff;
	cmd[6] = 0xff;
	cmd[7] = 0xff;

	resp_len = sizeof(resp);
	err = fwdv_avc_command(sc, cmd, 8, resp, &resp_len);
	if (err)
		return (err);

	if (resp_len < 8) {
		device_printf(sc->fd.dev,
		    "UNIT INFO: short response (%d bytes)\n", resp_len);
		return (EIO);
	}
	if ((resp[0] >> 4) != AVC_RESP_STABLE) {
		device_printf(sc->fd.dev,
		    "UNIT INFO: unexpected response 0x%02x\n", resp[0]);
		return (EIO);
	}

	FWDV_DEBUG(1, "UNIT INFO: unit_type=0x%02x, company_id=%02x%02x%02x\n",
	    resp[4] >> 3, resp[5], resp[6], resp[7]);

	return (0);
}

static int
fwdv_avc_tape_control(struct fwdv_softc *sc, uint8_t opcode, uint8_t operand,
    const char *name)
{
	uint8_t cmd[4], resp[4];
	int resp_len, err;

	cmd[0] = (AVC_CTYPE_CONTROL << 4) | 0;
	cmd[1] = AVC_ADDR_TAPE0;
	cmd[2] = opcode;
	cmd[3] = operand;

	resp_len = sizeof(resp);
	err = fwdv_avc_command(sc, cmd, 4, resp, &resp_len);
	if (err)
		return (err);
	if (resp_len < 1)
		return (EIO);

	if ((resp[0] >> 4) != AVC_RESP_ACCEPTED) {
		device_printf(sc->fd.dev, "%s rejected: 0x%02x\n",
		    name, resp[0]);
		return (EIO);
	}

	FWDV_DEBUG(1, "%s accepted\n", name);
	return (0);
}

static int
fwdv_avc_play(struct fwdv_softc *sc)
{

	return (fwdv_avc_tape_control(sc, AVC_OP_PLAY, AVC_PLAY_FWD, "PLAY"));
}

static int
fwdv_avc_stop(struct fwdv_softc *sc)
{

	return (fwdv_avc_tape_control(sc, AVC_OP_WIND, AVC_WIND_STOP, "STOP"));
}

static int
fwdv_read_opcr(struct fwdv_softc *sc, int plug, uint32_t *val)
{

	return (fwdv_read_quadlet(sc, CSR_BASE_HI,
	    CSR_BASE_LO | PCR_oPCR(plug), val));
}

static int
fwdv_iso_start(struct fwdv_softc *sc)
{
	struct firewire_comm *fc = sc->fd.fc;
	struct fw_xferq *xferq;
	uint32_t opcr_val;
	int dma_ch, err;
	uint8_t ch;

	mtx_assert(&sc->mtx, MA_NOTOWNED);

	FWDV_LOCK(sc);
	if (sc->dma_ch >= 0 || sc->state == FWDV_STATE_STREAMING) {
		FWDV_UNLOCK(sc);
		return (0);
	}
	if (sc->state == FWDV_STATE_DETACHING) {
		FWDV_UNLOCK(sc);
		return (ENXIO);
	}
	FWDV_UNLOCK(sc);

	ch = (uint8_t)(iso_channel & ISO_CHANNEL_MASK);
	err = fwdv_read_opcr(sc, 0, &opcr_val);
	if (err == 0 && (opcr_val & OPCR_ONLINE)) {
		uint8_t dev_ch = (opcr_val & OPCR_CHANNEL_MASK) >>
		    OPCR_CHANNEL_SHIFT;
		if (dev_ch < ISO_CHANNEL_MAX) {
			ch = dev_ch;
			FWDV_DEBUG(1, "oPCR[0] channel=%d\n", ch);
		}
	}

	dma_ch = fw_open_isodma(fc, 0);
	if (dma_ch < 0) {
		device_printf(sc->fd.dev, "no IR DMA channel available\n");
		return (EBUSY);
	}

	FWDV_LOCK(sc);
	if (sc->dma_ch >= 0) {
		FWDV_UNLOCK(sc);
		fc->ir[dma_ch]->flag &= ~FWXFERQ_OPEN;
		return (0);
	}
	FWDV_UNLOCK(sc);

	xferq = fc->ir[dma_ch];
	xferq->flag |= FWXFERQ_EXTBUF | FWXFERQ_HANDLER | FWXFERQ_STREAM;
	sc->iso_channel = ch;
	xferq->flag &= ~FWXFERQ_CHTAGMASK;
	xferq->flag |= (ch & ISO_CHANNEL_MASK) | (1 << 6);

	xferq->sc = (caddr_t)sc;
	xferq->hand = fwdv_iso_input;
	xferq->bnchunk = FWDV_ISO_NCHUNK;
	xferq->bnpacket = 1;
	xferq->psize = FWDV_ISO_PKTSIZE;
	xferq->queued = 0;
	xferq->buf = NULL;

	xferq->bulkxfer = malloc(sizeof(struct fw_bulkxfer) * xferq->bnchunk,
	    M_FWDV, M_WAITOK | M_ZERO);

	fw_iso_init_chunks(xferq);

	sc->frame_size = FWDV_MAX_FRAME_SIZE;
	sc->frame_buf = malloc(sc->frame_size, M_FWDV, M_WAITOK | M_ZERO);
	sc->read_buf = malloc(sc->frame_size, M_FWDV, M_WAITOK | M_ZERO);
	sc->frame_offset = 0;
	sc->frame_ready = 0;
	sc->frame_dropped = 0;
	sc->frame_count = 0;
	sc->dv_detected = 0;

	err = fc->irx_enable(fc, dma_ch);
	if (err) {
		device_printf(sc->fd.dev, "failed to start IR DMA: %d\n", err);
		goto fail;
	}

	FWDV_LOCK(sc);
	if (sc->state == FWDV_STATE_DETACHING || sc->fwdev == NULL) {
		FWDV_UNLOCK(sc);
		fc->irx_disable(fc, dma_ch);
		err = ENXIO;
		goto fail;
	}
	sc->dma_ch = dma_ch;
	sc->state = FWDV_STATE_STREAMING;
	wakeup(sc);
	FWDV_UNLOCK(sc);

	return (0);

fail:
	fw_iso_free_chunks(xferq, M_FWDV);
	xferq->flag &= ~(FWXFERQ_MODEMASK | FWXFERQ_OPEN | FWXFERQ_STREAM |
	    FWXFERQ_EXTBUF | FWXFERQ_HANDLER | FWXFERQ_CHTAGMASK);
	xferq->hand = NULL;

	free(sc->frame_buf, M_FWDV);
	free(sc->read_buf, M_FWDV);
	sc->frame_buf = NULL;
	sc->read_buf = NULL;
	sc->dma_ch = -1;

	return (err);
}

static void
fwdv_iso_stop(struct fwdv_softc *sc)
{
	struct firewire_comm *fc = sc->fd.fc;
	struct fw_xferq *xferq;
	int dma_ch;

	FWDV_LOCK(sc);
	dma_ch = sc->dma_ch;
	if (dma_ch < 0) {
		FWDV_UNLOCK(sc);
		return;
	}
	sc->dma_ch = -1;
	FWDV_UNLOCK(sc);

	xferq = fc->ir[dma_ch];

	if (xferq->flag & FWXFERQ_RUNNING)
		fc->irx_disable(fc, dma_ch);

	FWDV_LOCK(sc);
	fw_iso_wait_inactive_locked(&sc->mtx, &sc->iso_active, "fwdvis");
	sc->frame_ready = 0;
	while (sc->read_in_progress)
		msleep(&sc->read_in_progress, &sc->mtx, PWAIT, "fwdvst", hz);
	FWDV_UNLOCK(sc);

	xferq->flag &= ~(FWXFERQ_MODEMASK | FWXFERQ_OPEN | FWXFERQ_STREAM |
	    FWXFERQ_EXTBUF | FWXFERQ_HANDLER | FWXFERQ_CHTAGMASK);
	xferq->hand = NULL;

	fw_iso_free_chunks(xferq, M_FWDV);

	free(sc->frame_buf, M_FWDV);
	free(sc->read_buf, M_FWDV);
	sc->frame_buf = NULL;
	sc->read_buf = NULL;
}

static void
fwdv_iso_input(struct fw_xferq *xferq)
{
	struct fwdv_softc *sc = (struct fwdv_softc *)xferq->sc;
	struct fw_bulkxfer *sxfer;
	struct fw_pkt *fp;
	struct ciphdr *ciph;
	struct dvdbc *dv;
	struct mbuf *m;
	uint8_t *ptr, *end;
	uint32_t plen;
	uint8_t *tmp;
	int dma_ch;

	FWDV_LOCK(sc);
	dma_ch = sc->dma_ch;
	if (dma_ch < 0 || sc->frame_buf == NULL) {
		FWDV_UNLOCK(sc);
		return;
	}
	sc->iso_active = 1;
	FWDV_UNLOCK(sc);

	while ((sxfer = STAILQ_FIRST(&xferq->stvalid)) != NULL) {
		STAILQ_REMOVE_HEAD(&xferq->stvalid, link);

		m = fw_iso_dequeue(xferq, sxfer, sc->fd.fc);
		if (m == NULL)
			continue;

		fp = mtod(m, struct fw_pkt *);
		plen = fp->mode.stream.len;
		if (plen <= sizeof(struct ciphdr)) {
			m_freem(m);
			continue;
		}

		if (m->m_len < (int)(4 + sizeof(struct ciphdr))) {
			m_freem(m);
			continue;
		}

		ciph = (struct ciphdr *)(mtod(m, uint8_t *) + 4);

		if (ciph->fmt != CIP_FMT_DVCR) {
			m_freem(m);
			continue;
		}

		if (!sc->dv_detected) {
			sc->dv_system = ciph->fdf.dv.fs;
			sc->dv_detected = 1;
			if (ciph->fdf.dv.stype == CIP_STYPE_HD) {
				sc->frame_size = DV_DVCPRO_HD_FRAME;
			} else {
				sc->frame_size = sc->dv_system ?
				    DV_SD_PAL_FRAME : DV_SD_NTSC_FRAME;
			}
			printf("fwdv: DV stype=%d %s detected (frame=%u)\n",
			    ciph->fdf.dv.stype,
			    sc->dv_system ? "PAL" : "NTSC",
			    sc->frame_size);
		}

		ptr = (uint8_t *)(ciph + 1);
		end = mtod(m, uint8_t *) + 4 + plen;
		if (end > mtod(m, uint8_t *) + m->m_len)
			end = mtod(m, uint8_t *) + m->m_len;

		while (ptr + DV_DIF_BLOCK_SIZE <= end) {
			dv = (struct dvdbc *)ptr;

			if (dv->sct == DV_SCT_HEADER && dv->dseq == 0) {
				if (sc->frame_offset > 0) {
					if (sc->frame_offset >= sc->frame_size) {
						FWDV_LOCK(sc);
						if (sc->read_in_progress) {
							sc->frame_dropped++;
						} else {
							if (sc->frame_ready)
								sc->frame_dropped++;
							tmp = sc->read_buf;
							sc->read_buf =
							    sc->frame_buf;
							sc->frame_buf = tmp;
							sc->frame_ready = 1;
							sc->frame_count++;
							wakeup(sc);
							selwakeup(&sc->rsel);
						}
						FWDV_UNLOCK(sc);
					} else {
						sc->frame_dropped++;
					}
				}
				sc->frame_offset = 0;

				if (sc->dv_system == 1 &&
				    (dv->payload[0] & DV_DSF_12) == 0)
					dv->payload[0] |= DV_DSF_12;
			}

			if (sc->frame_offset + DV_DIF_BLOCK_SIZE <=
			    sc->frame_size) {
				memcpy(sc->frame_buf + sc->frame_offset,
				    ptr, DV_DIF_BLOCK_SIZE);
				sc->frame_offset += DV_DIF_BLOCK_SIZE;
			} else {
				sc->frame_dropped++;
				sc->frame_offset = 0;
			}

			ptr += DV_DIF_BLOCK_SIZE;
		}

		m_freem(m);
	}

	fw_iso_rearm_done(xferq, sc->fd.fc, &sc->mtx, &sc->iso_active,
	    &sc->dma_ch, dma_ch);
}

static int
fwdv_cdev_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct fwdv_softc *sc = dev->si_drv1;
	int err = 0;

	FWDV_LOCK(sc);
	if (sc->state == FWDV_STATE_DETACHING) {
		FWDV_UNLOCK(sc);
		return (ENXIO);
	}
	while (sc->state == FWDV_STATE_STARTING) {
		err = msleep(sc, &sc->mtx, PCATCH, "fwdvst", 5 * hz);
		if (err) {
			FWDV_UNLOCK(sc);
			return (err == EWOULDBLOCK ? EAGAIN : err);
		}
	}
	if (sc->state == FWDV_STATE_DETACHING) {
		FWDV_UNLOCK(sc);
		return (ENXIO);
	}
	if (sc->state != FWDV_STATE_PROBED &&
	    sc->state != FWDV_STATE_STREAMING) {
		FWDV_UNLOCK(sc);
		return (EAGAIN);
	}

	sc->open_count++;
	if (sc->open_count == 1 && sc->state == FWDV_STATE_PROBED) {
		sc->state = FWDV_STATE_STARTING;
		FWDV_UNLOCK(sc);
		err = fwdv_iso_start(sc);
		if (err) {
			FWDV_LOCK(sc);
			sc->open_count--;
			if (sc->state == FWDV_STATE_STARTING) {
				sc->state = FWDV_STATE_PROBED;
				wakeup(sc);
			}
			FWDV_UNLOCK(sc);
		}
	} else {
		FWDV_UNLOCK(sc);
	}
	return (err);
}

static int
fwdv_cdev_close(struct cdev *dev, int fflag, int devtype, struct thread *td)
{
	struct fwdv_softc *sc = dev->si_drv1;

	FWDV_LOCK(sc);
	sc->open_count--;
	if (sc->open_count <= 0) {
		sc->open_count = 0;
		if (sc->state == FWDV_STATE_STREAMING) {
			FWDV_UNLOCK(sc);
			fwdv_iso_stop(sc);
			FWDV_LOCK(sc);
			if (sc->state != FWDV_STATE_DETACHING)
				sc->state = FWDV_STATE_PROBED;
		}
	}
	FWDV_UNLOCK(sc);
	return (0);
}

static int
fwdv_cdev_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct fwdv_softc *sc = dev->si_drv1;
	int err;

	FWDV_LOCK(sc);
	while (!sc->frame_ready) {
		if (sc->state != FWDV_STATE_STREAMING) {
			FWDV_UNLOCK(sc);
			return (ENXIO);
		}
		if (ioflag & FNONBLOCK) {
			FWDV_UNLOCK(sc);
			return (EAGAIN);
		}
		err = msleep(sc, &sc->mtx, PCATCH, "fwdvrd", 0);
		if (err) {
			FWDV_UNLOCK(sc);
			return (err);
		}
	}

	sc->read_in_progress = 1;
	sc->frame_ready = 0;
	FWDV_UNLOCK(sc);

	err = uiomove(sc->read_buf, min(uio->uio_resid, sc->frame_size),
	    uio);

	FWDV_LOCK(sc);
	sc->read_in_progress = 0;
	wakeup(&sc->read_in_progress);
	FWDV_UNLOCK(sc);

	return (err);
}

static int
fwdv_cdev_poll(struct cdev *dev, int events, struct thread *td)
{
	struct fwdv_softc *sc = dev->si_drv1;
	int revents = 0;

	FWDV_LOCK(sc);
	if (sc->state != FWDV_STATE_STREAMING) {
		revents = POLLHUP;
	} else if (events & (POLLIN | POLLRDNORM)) {
		if (sc->frame_ready)
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(td, &sc->rsel);
	}
	FWDV_UNLOCK(sc);

	return (revents);
}

static int
fwdv_cdev_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag,
    struct thread *td)
{
	struct fwdv_softc *sc = dev->si_drv1;
	struct fwdv_info *info;

	switch (cmd) {
	case FWDV_GINFO:
		info = (struct fwdv_info *)data;
		memset(info, 0, sizeof(*info));
		FWDV_LOCK(sc);
		info->state = sc->state;
		info->iso_channel = sc->iso_channel;
		info->dv_system = sc->dv_detected ? sc->dv_system : 0xff;
		info->frame_size = sc->frame_size;
		info->frame_count = sc->frame_count;
		info->frame_dropped = sc->frame_dropped;
		info->eui_hi = sc->eui_hi;
		info->eui_lo = sc->eui_lo;
		strlcpy(info->vendor, sc->vendor, sizeof(info->vendor));
		strlcpy(info->model, sc->model, sizeof(info->model));
		FWDV_UNLOCK(sc);
		return (0);

	case FWDV_PLAY:
		return (fwdv_avc_play(sc));

	case FWDV_STOP:
		return (fwdv_avc_stop(sc));

	case FWDV_FFWD:
		return (fwdv_avc_tape_control(sc, AVC_OP_WIND,
		    AVC_WIND_FFWD, "FFWD"));

	case FWDV_REW:
		return (fwdv_avc_tape_control(sc, AVC_OP_WIND,
		    AVC_WIND_REW, "REW"));

	default:
		return (ENOTTY);
	}
}

static void
fwdv_parse_identity(struct fwdv_softc *sc)
{
	struct fw_device *fwdev = sc->fwdev;
	struct crom_context cc;

	sc->vendor[0] = '\0';
	sc->model[0] = '\0';

	if (fwdev == NULL)
		return;

	crom_init_context(&cc, fwdev->csrrom);

	crom_search_key(&cc, CSRKEY_VENDOR);
	crom_next(&cc);
	crom_parse_text(&cc, sc->vendor, sizeof(sc->vendor));

	crom_search_key(&cc, CSRKEY_MODEL);
	crom_next(&cc);
	crom_parse_text(&cc, sc->model, sizeof(sc->model));

}


static void
fwdv_probe_task(void *arg, int pending __unused)
{
	struct fwdv_softc *sc = (struct fwdv_softc *)arg;
	int err, restart;

	FWDV_LOCK(sc);
	if (sc->state == FWDV_STATE_DETACHING || sc->fwdev == NULL) {
		FWDV_UNLOCK(sc);
		return;
	}
	FWDV_UNLOCK(sc);

	fwdv_parse_identity(sc);
	err = fwdv_avc_unit_info(sc);
	if (err) {
		device_printf(sc->fd.dev,
		    "UNIT INFO failed (%d), device may not be ready\n", err);
	}

	FWDV_LOCK(sc);
	if (sc->state == FWDV_STATE_DETACHING || sc->fwdev == NULL) {
		FWDV_UNLOCK(sc);
		return;
	}
	sc->state = FWDV_STATE_PROBED;
	restart = (sc->open_count > 0);
	FWDV_UNLOCK(sc);

	if (restart)
		fwdv_iso_start(sc);
}



static int
fwdv_probe(device_t dev)
{
	struct fw_unit *unit;

	unit = fw_get_unit(dev);
	if (unit == NULL)
		return (ENXIO);

	if (unit->spec_id != CSRVAL_1394TA)
		return (ENXIO);
	if (unit->sw_version != CSR_PROTAVC)
		return (ENXIO);

	device_set_desc(dev, "AV/C DV Capture over FireWire");
	return (BUS_PROBE_DEFAULT);
}

static int
fwdv_attach(device_t dev)
{
	struct fwdv_softc *sc;
	struct firewire_comm *fc;
	struct fw_unit *unit;
	struct fw_device *fwdev;
	int err;

	unit = fw_get_unit(dev);
	if (unit == NULL)
		return (ENXIO);
	fwdev = unit->fwdev;
	if (fwdev == NULL)
		return (ENXIO);

	sc = device_get_softc(dev);
	sc->fd.dev = dev;
	sc->fd.fc = fw_get_comm(dev);
	fc = sc->fd.fc;
	mtx_init(&sc->mtx, "fwdv", NULL, MTX_DEF);

	sc->fwdev = fwdev;
	sc->eui_hi = fwdev->eui.hi;
	sc->eui_lo = fwdev->eui.lo;
	sc->state = FWDV_STATE_IDLE;
	sc->dma_ch = -1;
	sc->open_count = 0;
	sc->dv_system = 0;
	sc->dv_detected = 0;
	sc->avc_busy = 0;
	sc->iso_active = 0;
	knlist_init_mtx(&sc->rsel.si_note, &sc->mtx);
	TASK_INIT(&sc->probe_task, 0, fwdv_probe_task, sc);

	sc->cdev = make_dev(&fwdv_cdevsw, device_get_unit(dev),
	    UID_ROOT, GID_OPERATOR, 0660, "fwdv%d", device_get_unit(dev));
	if (sc->cdev == NULL) {
		device_printf(dev, "make_dev failed\n");
		knlist_destroy(&sc->rsel.si_note);
		mtx_destroy(&sc->mtx);
		return (ENXIO);
	}
	sc->cdev->si_drv1 = sc;

	STAILQ_INIT(&sc->fcp_bind.xferlist);
	sc->fcp_bind.start = FCP_RESPONSE_ADDR;
	sc->fcp_bind.end = FCP_RESPONSE_ADDR + FCP_MAX_FRAME_LEN - 1;
	sc->fcp_bind.sc = sc;
	if (fw_xferlist_add(&sc->fcp_bind.xferlist, M_FWDV,
	    0, FCP_MAX_FRAME_LEN, FWDV_FCP_XFERS, fc, sc,
	    fwdv_fcp_handler) < FWDV_FCP_XFERS) {
		device_printf(dev, "fw_xferlist_add: allocation failed\n");
		fw_xferlist_remove(&sc->fcp_bind.xferlist);
		destroy_dev(sc->cdev);
		knlist_destroy(&sc->rsel.si_note);
		mtx_destroy(&sc->mtx);
		return (ENOMEM);
	}

	err = fw_bindadd(fc, &sc->fcp_bind);
	if (err) {
		device_printf(dev, "fw_bindadd failed: %d\n", err);
		fw_xferlist_remove(&sc->fcp_bind.xferlist);
		destroy_dev(sc->cdev);
		knlist_destroy(&sc->rsel.si_note);
		mtx_destroy(&sc->mtx);
		return (err);
	}

	if (taskqueue_enqueue(taskqueue_thread, &sc->probe_task) != 0)
		device_printf(dev, "probe task enqueue failed\n");

	return (0);
}

static int
fwdv_detach(device_t dev)
{
	struct fwdv_softc *sc;

	sc = device_get_softc(dev);

	FWDV_LOCK(sc);
	sc->state = FWDV_STATE_DETACHING;
	wakeup(sc);
	wakeup(&sc->fcp_resp_ready);
	wakeup(&sc->avc_busy);
	FWDV_UNLOCK(sc);

	taskqueue_drain(taskqueue_thread, &sc->probe_task);
	fwdv_iso_stop(sc);

	fw_bindremove(sc->fd.fc, &sc->fcp_bind);
	fw_xferlist_remove(&sc->fcp_bind.xferlist);

	if (sc->cdev != NULL)
		destroy_dev(sc->cdev);

	seldrain(&sc->rsel);
	knlist_destroy(&sc->rsel.si_note);
	mtx_destroy(&sc->mtx);

	return (0);
}

static device_method_t fwdv_methods[] = {
	DEVMETHOD(device_probe,		fwdv_probe),
	DEVMETHOD(device_attach,	fwdv_attach),
	DEVMETHOD(device_detach,	fwdv_detach),
	DEVMETHOD_END
};

static driver_t fwdv_driver = {
	"fwdv",
	fwdv_methods,
	sizeof(struct fwdv_softc)
};

DRIVER_MODULE(fwdv, firewire, fwdv_driver, NULL, NULL);
MODULE_VERSION(fwdv, 1);
MODULE_DEPEND(fwdv, firewire, 1, 1, 1);

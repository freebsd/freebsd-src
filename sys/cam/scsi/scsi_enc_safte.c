/*-
 * Copyright (c) 2000 Matthew Jacob
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/sys/cam/scsi/scsi_ses.c 201758 2010-01-07 21:01:37Z mbr $");

#include <sys/param.h>

#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/sx.h>
#include <sys/systm.h>
#include <sys/types.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_periph.h>

#include <cam/scsi/scsi_enc.h>
#include <cam/scsi/scsi_enc_internal.h>
#include <cam/scsi/scsi_message.h>

#include <opt_enc.h>

/*
 * SAF-TE Type Device Emulation
 */

static int set_elm_status_sel(enc_softc_t *, encioc_elm_status_t *, int);
static int wrbuf16(enc_softc_t *, uint8_t, uint8_t, uint8_t, uint8_t, int);
static void wrslot_stat(enc_softc_t *, int);
static int perf_slotop(enc_softc_t *, uint8_t, uint8_t, int);

#define	ALL_ENC_STAT (SES_ENCSTAT_CRITICAL | SES_ENCSTAT_UNRECOV | \
	SES_ENCSTAT_NONCRITICAL | SES_ENCSTAT_INFO)
/*
 * SAF-TE specific defines- Mandatory ones only...
 */

/*
 * READ BUFFER ('get' commands) IDs- placed in offset 2 of cdb
 */
#define	SAFTE_RD_RDCFG	0x00	/* read enclosure configuration */
#define	SAFTE_RD_RDESTS	0x01	/* read enclosure status */
#define	SAFTE_RD_RDDSTS	0x04	/* read drive slot status */
#define	SAFTE_RD_RDGFLG	0x05	/* read global flags */

/*
 * WRITE BUFFER ('set' commands) IDs- placed in offset 0 of databuf
 */
#define	SAFTE_WT_DSTAT	0x10	/* write device slot status */
#define	SAFTE_WT_SLTOP	0x12	/* perform slot operation */
#define	SAFTE_WT_FANSPD	0x13	/* set fan speed */
#define	SAFTE_WT_ACTPWS	0x14	/* turn on/off power supply */
#define	SAFTE_WT_GLOBAL	0x15	/* send global command */

#define	SAFT_SCRATCH	64
#define	SCSZ		0x8000

typedef enum {
	SAFTE_UPDATE_NONE,
	SAFTE_UPDATE_READCONFIG,
	SAFTE_UPDATE_READGFLAGS,
	SAFTE_UPDATE_READENCSTATUS,
	SAFTE_UPDATE_READSLOTSTATUS,
	SAFTE_NUM_UPDATE_STATES
} safte_update_action;

static fsm_fill_handler_t safte_fill_read_buf_io;
static fsm_done_handler_t safte_process_config;
static fsm_done_handler_t safte_process_gflags;
static fsm_done_handler_t safte_process_status;
static fsm_done_handler_t safte_process_slotstatus;

static struct enc_fsm_state enc_fsm_states[SAFTE_NUM_UPDATE_STATES] =
{
	{ "SAFTE_UPDATE_NONE", 0, 0, 0, NULL, NULL, NULL },
	{
		"SAFTE_UPDATE_READCONFIG",
		SAFTE_RD_RDCFG,
		SAFT_SCRATCH,
		60 * 1000,
		safte_fill_read_buf_io,
		safte_process_config,
		enc_error
	},
	{
		"SAFTE_UPDATE_READGFLAGS",
		SAFTE_RD_RDGFLG,
		16,
		60 * 1000,
		safte_fill_read_buf_io,
		safte_process_gflags,
		enc_error
	},
	{
		"SAFTE_UPDATE_READENCSTATUS",
		SAFTE_RD_RDESTS,
		SCSZ,
		60 * 1000,
		safte_fill_read_buf_io,
		safte_process_status,
		enc_error
	},
	{
		"SAFTE_UPDATE_READSLOTSTATUS",
		SAFTE_RD_RDDSTS,
		SCSZ,
		60 * 1000,
		safte_fill_read_buf_io,
		safte_process_slotstatus,
		enc_error
	}
};

#define	NPSEUDO_ALARM	1
struct scfg {
	/*
	 * Cached Configuration
	 */
	uint8_t	Nfans;		/* Number of Fans */
	uint8_t	Npwr;		/* Number of Power Supplies */
	uint8_t	Nslots;		/* Number of Device Slots */
	uint8_t	DoorLock;	/* Door Lock Installed */
	uint8_t	Ntherm;		/* Number of Temperature Sensors */
	uint8_t	Nspkrs;		/* Number of Speakers */
	uint8_t Nalarm;		/* Number of Alarms (at least one) */
	uint8_t	Ntstats;	/* Number of Thermostats */
	/*
	 * Cached Flag Bytes for Global Status
	 */
	uint8_t	flag1;
	uint8_t	flag2;
	/*
	 * What object index ID is where various slots start.
	 */
	uint8_t	pwroff;
	uint8_t	slotoff;
#define	SAFT_ALARM_OFFSET(cc)	(cc)->slotoff - 1
};

#define	SAFT_FLG1_ALARM		0x1
#define	SAFT_FLG1_GLOBFAIL	0x2
#define	SAFT_FLG1_GLOBWARN	0x4
#define	SAFT_FLG1_ENCPWROFF	0x8
#define	SAFT_FLG1_ENCFANFAIL	0x10
#define	SAFT_FLG1_ENCPWRFAIL	0x20
#define	SAFT_FLG1_ENCDRVFAIL	0x40
#define	SAFT_FLG1_ENCDRVWARN	0x80

#define	SAFT_FLG2_LOCKDOOR	0x4
#define	SAFT_PRIVATE		sizeof (struct scfg)

static char *safte_2little = "Too Little Data Returned (%d) at line %d\n";
#define	SAFT_BAIL(r, x)	\
	if ((r) >= (x)) { \
		ENC_LOG(enc, safte_2little, x, __LINE__);\
		return (EIO); \
	}

static int
safte_fill_read_buf_io(enc_softc_t *enc, struct enc_fsm_state *state,
		       union ccb *ccb, uint8_t *buf)
{

	if (state->page_code != SAFTE_RD_RDCFG &&
	    enc->enc_cache.nelms == 0) {
		enc_update_request(enc, SAFTE_UPDATE_READCONFIG);
		return (-1);
	}

	if (enc->enc_type == ENC_SEMB_SAFT) {
		semb_read_buffer(&ccb->ataio, /*retries*/5,
				enc_done, MSG_SIMPLE_Q_TAG,
				state->page_code, buf, state->buf_size,
				state->timeout);
	} else {
		scsi_read_buffer(&ccb->csio, /*retries*/5,
				enc_done, MSG_SIMPLE_Q_TAG, 1,
				state->page_code, 0, buf, state->buf_size,
				SSD_FULL_SIZE, state->timeout);
	}
	return (0);
}

static int
safte_process_config(enc_softc_t *enc, struct enc_fsm_state *state,
		   union ccb *ccb, uint8_t **bufp, int xfer_len)
{
	struct scfg *cfg;
	uint8_t *buf = *bufp;
	int i, r;

	cfg = enc->enc_private;
	if (cfg == NULL)
		return (ENXIO);

	if (xfer_len < 6) {
		ENC_LOG(enc, "too little data (%d) for configuration\n",
		    xfer_len);
		return (EIO);
	}
	cfg->Nfans = buf[0];
	cfg->Npwr = buf[1];
	cfg->Nslots = buf[2];
	cfg->DoorLock = buf[3];
	cfg->Ntherm = buf[4];
	cfg->Nspkrs = buf[5];
	cfg->Nalarm = NPSEUDO_ALARM;
	if (xfer_len >= 7)
		cfg->Ntstats = buf[6] & 0x0f;
	else
		cfg->Ntstats = 0;
	ENC_VLOG(enc, "Nfans %d Npwr %d Nslots %d Lck %d Ntherm %d Nspkrs %d "
	    "Ntstats %d\n",
	    cfg->Nfans, cfg->Npwr, cfg->Nslots, cfg->DoorLock, cfg->Ntherm,
	    cfg->Nspkrs, cfg->Ntstats);

	enc->enc_cache.nelms = cfg->Nfans + cfg->Npwr + cfg->Nslots +
	    cfg->DoorLock + cfg->Ntherm + cfg->Nspkrs + cfg->Ntstats + 1 +
	    NPSEUDO_ALARM;
	ENC_FREE_AND_NULL(enc->enc_cache.elm_map);
	enc->enc_cache.elm_map =
	    ENC_MALLOCZ(enc->enc_cache.nelms * sizeof(enc_element_t));
	if (enc->enc_cache.elm_map == NULL) {
		enc->enc_cache.nelms = 0;
		return (ENOMEM);
	}

	r = 0;
	/*
	 * Note that this is all arranged for the convenience
	 * in later fetches of status.
	 */
	for (i = 0; i < cfg->Nfans; i++)
		enc->enc_cache.elm_map[r++].enctype = ELMTYP_FAN;
	cfg->pwroff = (uint8_t) r;
	for (i = 0; i < cfg->Npwr; i++)
		enc->enc_cache.elm_map[r++].enctype = ELMTYP_POWER;
	for (i = 0; i < cfg->DoorLock; i++)
		enc->enc_cache.elm_map[r++].enctype = ELMTYP_DOORLOCK;
	for (i = 0; i < cfg->Nspkrs; i++)
		enc->enc_cache.elm_map[r++].enctype = ELMTYP_ALARM;
	for (i = 0; i < cfg->Ntherm; i++)
		enc->enc_cache.elm_map[r++].enctype = ELMTYP_THERM;
	for (i = 0; i <= cfg->Ntstats; i++)
		enc->enc_cache.elm_map[r++].enctype = ELMTYP_THERM;
	enc->enc_cache.elm_map[r++].enctype = ELMTYP_ALARM;
	cfg->slotoff = (uint8_t) r;
	for (i = 0; i < cfg->Nslots; i++)
		enc->enc_cache.elm_map[r++].enctype = ELMTYP_DEVICE;

	enc_update_request(enc, SAFTE_UPDATE_READGFLAGS);
	enc_update_request(enc, SAFTE_UPDATE_READENCSTATUS);
	enc_update_request(enc, SAFTE_UPDATE_READSLOTSTATUS);

	return (0);
}

static int
safte_process_gflags(enc_softc_t *enc, struct enc_fsm_state *state,
		   union ccb *ccb, uint8_t **bufp, int xfer_len)
{
	struct scfg *cfg;
	uint8_t *buf = *bufp;

	cfg = enc->enc_private;
	if (cfg == NULL)
		return (ENXIO);

	SAFT_BAIL(3, xfer_len);
	cfg->flag1 = buf[1];
	cfg->flag2 = buf[2];

	return (0);
}

static int
safte_process_status(enc_softc_t *enc, struct enc_fsm_state *state,
		   union ccb *ccb, uint8_t **bufp, int xfer_len)
{
	struct scfg *cfg;
	uint8_t *buf = *bufp;
	int oid, r, i, nitems;
	uint16_t tempflags;
	enc_cache_t *cache = &enc->enc_cache;

	cfg = enc->enc_private;
	if (cfg == NULL)
		return (ENXIO);

	oid = r = 0;

	for (nitems = i = 0; i < cfg->Nfans; i++) {
		SAFT_BAIL(r, xfer_len);
		/*
		 * 0 = Fan Operational
		 * 1 = Fan is malfunctioning
		 * 2 = Fan is not present
		 * 0x80 = Unknown or Not Reportable Status
		 */
		cache->elm_map[oid].encstat[1] = 0;	/* resvd */
		cache->elm_map[oid].encstat[2] = 0;	/* resvd */
		switch ((int)buf[r]) {
		case 0:
			nitems++;
			cache->elm_map[oid].encstat[0] = SES_OBJSTAT_OK;
			/*
			 * We could get fancier and cache
			 * fan speeds that we have set, but
			 * that isn't done now.
			 */
			cache->elm_map[oid].encstat[3] = 7;
			break;

		case 1:
			cache->elm_map[oid].encstat[0] =
			    SES_OBJSTAT_CRIT;
			/*
			 * FAIL and FAN STOPPED synthesized
			 */
			cache->elm_map[oid].encstat[3] = 0x40;
			/*
			 * Enclosure marked with CRITICAL error
			 * if only one fan or no thermometers,
			 * else the NONCRITICAL error is set.
			 */
			if (cfg->Nfans == 1 || (cfg->Ntherm + cfg->Ntstats) == 0)
				cache->enc_status |= SES_ENCSTAT_CRITICAL;
			else
				cache->enc_status |= SES_ENCSTAT_NONCRITICAL;
			break;
		case 2:
			cache->elm_map[oid].encstat[0] =
			    SES_OBJSTAT_NOTINSTALLED;
			cache->elm_map[oid].encstat[3] = 0;
			/*
			 * Enclosure marked with CRITICAL error
			 * if only one fan or no thermometers,
			 * else the NONCRITICAL error is set.
			 */
			if (cfg->Nfans == 1)
				cache->enc_status |= SES_ENCSTAT_CRITICAL;
			else
				cache->enc_status |= SES_ENCSTAT_NONCRITICAL;
			break;
		case 0x80:
			cache->elm_map[oid].encstat[0] = SES_OBJSTAT_UNKNOWN;
			cache->elm_map[oid].encstat[3] = 0;
			cache->enc_status |= SES_ENCSTAT_INFO;
			break;
		default:
			cache->elm_map[oid].encstat[0] = SES_OBJSTAT_UNSUPPORTED;
			ENC_LOG(enc, "Unknown fan%d status 0x%x\n", i,
			    buf[r] & 0xff);
			break;
		}
		cache->elm_map[oid++].svalid = 1;
		r++;
	}

	/*
	 * No matter how you cut it, no cooling elements when there
	 * should be some there is critical.
	 */
	if (cfg->Nfans && nitems == 0) {
		cache->enc_status |= SES_ENCSTAT_CRITICAL;
	}


	for (i = 0; i < cfg->Npwr; i++) {
		SAFT_BAIL(r, xfer_len);
		cache->elm_map[oid].encstat[0] = SES_OBJSTAT_UNKNOWN;
		cache->elm_map[oid].encstat[1] = 0;	/* resvd */
		cache->elm_map[oid].encstat[2] = 0;	/* resvd */
		cache->elm_map[oid].encstat[3] = 0x20;	/* requested on */
		switch (buf[r]) {
		case 0x00:	/* pws operational and on */
			cache->elm_map[oid].encstat[0] = SES_OBJSTAT_OK;
			break;
		case 0x01:	/* pws operational and off */
			cache->elm_map[oid].encstat[0] = SES_OBJSTAT_OK;
			cache->elm_map[oid].encstat[3] = 0x10;
			cache->enc_status |= SES_ENCSTAT_INFO;
			break;
		case 0x10:	/* pws is malfunctioning and commanded on */
			cache->elm_map[oid].encstat[0] = SES_OBJSTAT_CRIT;
			cache->elm_map[oid].encstat[3] = 0x61;
			cache->enc_status |= SES_ENCSTAT_NONCRITICAL;
			break;

		case 0x11:	/* pws is malfunctioning and commanded off */
			cache->elm_map[oid].encstat[0] = SES_OBJSTAT_NONCRIT;
			cache->elm_map[oid].encstat[3] = 0x51;
			cache->enc_status |= SES_ENCSTAT_NONCRITICAL;
			break;
		case 0x20:	/* pws is not present */
			cache->elm_map[oid].encstat[0] =
			    SES_OBJSTAT_NOTINSTALLED;
			cache->elm_map[oid].encstat[3] = 0;
			cache->enc_status |= SES_ENCSTAT_INFO;
			break;
		case 0x21:	/* pws is present */
			/*
			 * This is for enclosures that cannot tell whether the
			 * device is on or malfunctioning, but know that it is
			 * present. Just fall through.
			 */
			/* FALLTHROUGH */
		case 0x80:	/* Unknown or Not Reportable Status */
			cache->elm_map[oid].encstat[0] = SES_OBJSTAT_UNKNOWN;
			cache->elm_map[oid].encstat[3] = 0;
			cache->enc_status |= SES_ENCSTAT_INFO;
			break;
		default:
			ENC_LOG(enc, "unknown power supply %d status (0x%x)\n",
			    i, buf[r] & 0xff);
			break;
		}
		enc->enc_cache.elm_map[oid++].svalid = 1;
		r++;
	}

	/*
	 * Skip over Slot SCSI IDs
	 */
	r += cfg->Nslots;

	/*
	 * We always have doorlock status, no matter what,
	 * but we only save the status if we have one.
	 */
	SAFT_BAIL(r, xfer_len);
	if (cfg->DoorLock) {
		/*
		 * 0 = Door Locked
		 * 1 = Door Unlocked, or no Lock Installed
		 * 0x80 = Unknown or Not Reportable Status
		 */
		cache->elm_map[oid].encstat[1] = 0;
		cache->elm_map[oid].encstat[2] = 0;
		switch (buf[r]) {
		case 0:
			cache->elm_map[oid].encstat[0] = SES_OBJSTAT_OK;
			cache->elm_map[oid].encstat[3] = 0;
			break;
		case 1:
			cache->elm_map[oid].encstat[0] = SES_OBJSTAT_OK;
			cache->elm_map[oid].encstat[3] = 1;
			break;
		case 0x80:
			cache->elm_map[oid].encstat[0] = SES_OBJSTAT_UNKNOWN;
			cache->elm_map[oid].encstat[3] = 0;
			cache->enc_status |= SES_ENCSTAT_INFO;
			break;
		default:
			cache->elm_map[oid].encstat[0] =
			    SES_OBJSTAT_UNSUPPORTED;
			ENC_LOG(enc, "unknown lock status 0x%x\n",
			    buf[r] & 0xff);
			break;
		}
		cache->elm_map[oid++].svalid = 1;
	}
	r++;

	/*
	 * We always have speaker status, no matter what,
	 * but we only save the status if we have one.
	 */
	SAFT_BAIL(r, xfer_len);
	if (cfg->Nspkrs) {
		cache->elm_map[oid].encstat[1] = 0;
		cache->elm_map[oid].encstat[2] = 0;
		if (buf[r] == 1) {
			/*
			 * We need to cache tone urgency indicators.
			 * Someday.
			 */
			cache->elm_map[oid].encstat[0] = SES_OBJSTAT_NONCRIT;
			cache->elm_map[oid].encstat[3] = 0x8;
			cache->enc_status |= SES_ENCSTAT_NONCRITICAL;
		} else if (buf[r] == 0) {
			cache->elm_map[oid].encstat[0] = SES_OBJSTAT_OK;
			cache->elm_map[oid].encstat[3] = 0;
		} else {
			cache->elm_map[oid].encstat[0] = SES_OBJSTAT_UNSUPPORTED;
			cache->elm_map[oid].encstat[3] = 0;
			ENC_LOG(enc, "unknown spkr status 0x%x\n",
			    buf[r] & 0xff);
		}
		cache->elm_map[oid++].svalid = 1;
	}
	r++;

	/*
	 * Now, for "pseudo" thermometers, we have two bytes
	 * of information in enclosure status- 16 bits. Actually,
	 * the MSB is a single TEMP ALERT flag indicating whether
	 * any other bits are set, but, thanks to fuzzy thinking,
	 * in the SAF-TE spec, this can also be set even if no
	 * other bits are set, thus making this really another
	 * binary temperature sensor.
	 */

	SAFT_BAIL(r + cfg->Ntherm, xfer_len);
	tempflags = buf[r + cfg->Ntherm];
	SAFT_BAIL(r + cfg->Ntherm + 1, xfer_len);
	tempflags |= (tempflags << 8) | buf[r + cfg->Ntherm + 1];

	for (i = 0; i < cfg->Ntherm; i++) {
		SAFT_BAIL(r, xfer_len);
		/*
		 * Status is a range from -10 to 245 deg Celsius,
		 * which we need to normalize to -20 to -245 according
		 * to the latest SCSI spec, which makes little
		 * sense since this would overflow an 8bit value.
		 * Well, still, the base normalization is -20,
		 * not -10, so we have to adjust.
		 *
		 * So what's over and under temperature?
		 * Hmm- we'll state that 'normal' operating
		 * is 10 to 40 deg Celsius.
		 */

		/*
		 * Actually.... All of the units that people out in the world
		 * seem to have do not come even close to setting a value that
		 * complies with this spec.
		 *
		 * The closest explanation I could find was in an
		 * LSI-Logic manual, which seemed to indicate that
		 * this value would be set by whatever the I2C code
		 * would interpolate from the output of an LM75
		 * temperature sensor.
		 *
		 * This means that it is impossible to use the actual
		 * numeric value to predict anything. But we don't want
		 * to lose the value. So, we'll propagate the *uncorrected*
		 * value and set SES_OBJSTAT_NOTAVAIL. We'll depend on the
		 * temperature flags for warnings.
		 */
		if (tempflags & (1 << i)) {
			cache->elm_map[oid].encstat[0] = SES_OBJSTAT_CRIT;
			cache->enc_status |= SES_ENCSTAT_CRITICAL;
		} else
			cache->elm_map[oid].encstat[0] = SES_OBJSTAT_OK;
		cache->elm_map[oid].encstat[1] = 0;
		cache->elm_map[oid].encstat[2] = buf[r];
		cache->elm_map[oid].encstat[3] = 0;
		cache->elm_map[oid++].svalid = 1;
		r++;
	}

	for (i = 0; i <= cfg->Ntstats; i++) {
		cache->elm_map[oid].encstat[1] = 0;
		if (tempflags & (1 <<
		    ((i == cfg->Ntstats) ? 15 : (cfg->Ntherm + i)))) {
			cache->elm_map[oid].encstat[0] = SES_OBJSTAT_CRIT;
			cache->elm_map[4].encstat[2] = 0xff;
			/*
			 * Set 'over temperature' failure.
			 */
			cache->elm_map[oid].encstat[3] = 8;
			cache->enc_status |= SES_ENCSTAT_CRITICAL;
		} else {
			/*
			 * We used to say 'not available' and synthesize a
			 * nominal 30 deg (C)- that was wrong. Actually,
			 * Just say 'OK', and use the reserved value of
			 * zero.
			 */
			if ((cfg->Ntherm + cfg->Ntstats) == 0)
				cache->elm_map[oid].encstat[0] =
				    SES_OBJSTAT_NOTAVAIL;
			else
				cache->elm_map[oid].encstat[0] =
				    SES_OBJSTAT_OK;
			cache->elm_map[oid].encstat[2] = 0;
			cache->elm_map[oid].encstat[3] = 0;
		}
		cache->elm_map[oid++].svalid = 1;
	}
	r += 2;

	/*
	 * Get alarm status.
	 */
	cache->elm_map[oid].encstat[0] = SES_OBJSTAT_OK;
	cache->elm_map[oid].encstat[3] = cache->elm_map[oid].priv;
	cache->elm_map[oid++].svalid = 1;

	return (0);
}

static int
safte_process_slotstatus(enc_softc_t *enc, struct enc_fsm_state *state,
		   union ccb *ccb, uint8_t **bufp, int xfer_len)
{
	struct scfg *cfg;
	uint8_t *buf = *bufp;
	enc_cache_t *cache = &enc->enc_cache;
	int oid, r, i;
	uint8_t status;

	cfg = enc->enc_private;
	if (cfg == NULL)
		return (ENXIO);

	oid = cfg->slotoff;
	for (r = i = 0; i < cfg->Nslots; i++, r += 4) {
		SAFT_BAIL(r+3, xfer_len);
		cache->elm_map[oid].encstat[0] = SES_OBJSTAT_UNSUPPORTED;
		cache->elm_map[oid].encstat[1] = (uint8_t) i;
		cache->elm_map[oid].encstat[2] = 0;
		cache->elm_map[oid].encstat[3] = 0;
		status = buf[r+3];
		if ((status & 0x1) == 0) {	/* no device */
			cache->elm_map[oid].encstat[0] =
			    SES_OBJSTAT_NOTINSTALLED;
		} else {
			cache->elm_map[oid].encstat[0] = SES_OBJSTAT_OK;
		}
		if (status & 0x2) {
			cache->elm_map[oid].encstat[2] = 0x8;
		}
		if ((status & 0x4) == 0) {
			cache->elm_map[oid].encstat[3] = 0x10;
		}
		cache->elm_map[oid++].svalid = 1;
	}
	return (0);
}

static int
set_elm_status_sel(enc_softc_t *enc, encioc_elm_status_t *elms, int slp)
{
	int idx;
	enc_element_t *ep;
	struct scfg *cc = enc->enc_private;

	if (cc == NULL)
		return (0);

	idx = (int)elms->elm_idx;
	ep = &enc->enc_cache.elm_map[idx];

	switch (ep->enctype) {
	case ELMTYP_DEVICE:
		if (elms->cstat[0] & SESCTL_PRDFAIL) {
			ep->priv |= 0x40;
		} else {
			ep->priv &= ~0x40;
		}
		/* SESCTL_RSTSWAP has no correspondence in SAF-TE */
		if (elms->cstat[0] & SESCTL_DISABLE) {
			ep->priv |= 0x80;
			/*
			 * Hmm. Try to set the 'No Drive' flag.
			 * Maybe that will count as a 'disable'.
			 */
		} else {
			ep->priv &= ~0x80;
		}
		if (ep->priv & 0xc6) {
			ep->priv &= ~0x1;
		} else {
			ep->priv |= 0x1;	/* no errors */
		}
		wrslot_stat(enc, slp);
		break;
	case ELMTYP_POWER:
		/*
		 * Okay- the only one that makes sense here is to
		 * do the 'disable' for a power supply.
		 */
		if (elms->cstat[0] & SESCTL_DISABLE) {
			(void) wrbuf16(enc, SAFTE_WT_ACTPWS,
				idx - cc->pwroff, 0, 0, slp);
		}
		break;
	case ELMTYP_FAN:
		/*
		 * Okay- the only one that makes sense here is to
		 * set fan speed to zero on disable.
		 */
		if (elms->cstat[0] & SESCTL_DISABLE) {
			/* remember- fans are the first items, so idx works */
			(void) wrbuf16(enc, SAFTE_WT_FANSPD, idx, 0, 0, slp);
		}
		break;
	case ELMTYP_DOORLOCK:
		/*
		 * Well, we can 'disable' the lock.
		 */
		if (elms->cstat[0] & SESCTL_DISABLE) {
			cc->flag2 &= ~SAFT_FLG2_LOCKDOOR;
			(void) wrbuf16(enc, SAFTE_WT_GLOBAL, cc->flag1,
				cc->flag2, 0, slp);
		}
		break;
	case ELMTYP_ALARM:
		/*
		 * Well, we can 'disable' the alarm.
		 */
		if (elms->cstat[0] & SESCTL_DISABLE) {
			cc->flag2 &= ~SAFT_FLG1_ALARM;
			ep->priv |= 0x40;	/* Muted */
			(void) wrbuf16(enc, SAFTE_WT_GLOBAL, cc->flag1,
				cc->flag2, 0, slp);
		}
		break;
	default:
		break;
	}
	ep->svalid = 0;
	return (0);
}

/*
 * This function handles all of the 16 byte WRITE BUFFER commands.
 */
static int
wrbuf16(enc_softc_t *enc, uint8_t op, uint8_t b1, uint8_t b2,
    uint8_t b3, int slp)
{
	int err, amt;
	char *sdata;
	struct scfg *cc = enc->enc_private;
	static char cdb[10] = { WRITE_BUFFER, 1, 0, 0, 0, 0, 0, 0, 16, 0 };

	if (cc == NULL)
		return (0);

	sdata = ENC_MALLOCZ(16);
	if (sdata == NULL)
		return (ENOMEM);

	ENC_DLOG(enc, "saf_wrbuf16 %x %x %x %x\n", op, b1, b2, b3);

	sdata[0] = op;
	sdata[1] = b1;
	sdata[2] = b2;
	sdata[3] = b3;
	amt = -16;
	err = enc_runcmd(enc, cdb, 10, sdata, &amt);
	ENC_FREE(sdata);
	return (err);
}

/*
 * This function updates the status byte for the device slot described.
 *
 * Since this is an optional SAF-TE command, there's no point in
 * returning an error.
 */
static void
wrslot_stat(enc_softc_t *enc, int slp)
{
	int i, amt;
	enc_element_t *ep;
	char cdb[10], *sdata;
	struct scfg *cc = enc->enc_private;

	if (cc == NULL)
		return;

	ENC_DLOG(enc, "saf_wrslot\n");
	cdb[0] = WRITE_BUFFER;
	cdb[1] = 1;
	cdb[2] = 0;
	cdb[3] = 0;
	cdb[4] = 0;
	cdb[5] = 0;
	cdb[6] = 0;
	cdb[7] = 0;
	cdb[8] = cc->Nslots * 3 + 1;
	cdb[9] = 0;

	sdata = ENC_MALLOCZ(cc->Nslots * 3 + 1);
	if (sdata == NULL)
		return;

	sdata[0] = SAFTE_WT_DSTAT;
	for (i = 0; i < cc->Nslots; i++) {
		ep = &enc->enc_cache.elm_map[cc->slotoff + i];
		ENC_DLOG(enc, "saf_wrslot %d <- %x\n", i, ep->priv & 0xff);
		sdata[1 + (3 * i)] = ep->priv & 0xff;
	}
	amt = -(cc->Nslots * 3 + 1);
	(void) enc_runcmd(enc, cdb, 10, sdata, &amt);
	ENC_FREE(sdata);
}

/*
 * This function issues the "PERFORM SLOT OPERATION" command.
 */
static int
perf_slotop(enc_softc_t *enc, uint8_t slot, uint8_t opflag, int slp)
{
	int err, amt;
	char *sdata;
	struct scfg *cc = enc->enc_private;
	static char cdb[10] =
	    { WRITE_BUFFER, 1, 0, 0, 0, 0, 0, 0, SAFT_SCRATCH, 0 };

	if (cc == NULL)
		return (0);

	sdata = ENC_MALLOCZ(SAFT_SCRATCH);
	if (sdata == NULL)
		return (ENOMEM);

	sdata[0] = SAFTE_WT_SLTOP;
	sdata[1] = slot;
	sdata[2] = opflag;
	ENC_DLOG(enc, "saf_slotop slot %d op %x\n", slot, opflag);
	amt = -SAFT_SCRATCH;
	err = enc_runcmd(enc, cdb, 10, sdata, &amt);
	ENC_FREE(sdata);
	return (err);
}

static void
safte_softc_cleanup(struct cam_periph *periph)
{
	enc_softc_t *enc;

	enc = periph->softc;
	ENC_FREE_AND_NULL(enc->enc_cache.elm_map);
	ENC_FREE_AND_NULL(enc->enc_private);
	enc->enc_cache.nelms = 0;
}

static int
safte_init_enc(enc_softc_t *enc)
{
	int err;
	static char cdb0[6] = { SEND_DIAGNOSTIC };

	err = enc_runcmd(enc, cdb0, 6, NULL, 0);
	if (err) {
		return (err);
	}
	DELAY(5000);
	err = wrbuf16(enc, SAFTE_WT_GLOBAL, 0, 0, 0, 1);
	return (err);
}

static int
safte_get_enc_status(enc_softc_t *enc, int slpflg)
{

	return (0);
}

static int
safte_set_enc_status(enc_softc_t *enc, uint8_t encstat, int slpflg)
{
	struct scfg *cc = enc->enc_private;
	if (cc == NULL)
		return (0);
	/*
	 * Since SAF-TE devices aren't necessarily sticky in terms
	 * of state, make our soft copy of enclosure status 'sticky'-
	 * that is, things set in enclosure status stay set (as implied
	 * by conditions set in reading object status) until cleared.
	 */
	enc->enc_cache.enc_status &= ~ALL_ENC_STAT;
	enc->enc_cache.enc_status |= (encstat & ALL_ENC_STAT);
	cc->flag1 &= ~(SAFT_FLG1_ALARM|SAFT_FLG1_GLOBFAIL|SAFT_FLG1_GLOBWARN);
	if ((encstat & (SES_ENCSTAT_CRITICAL|SES_ENCSTAT_UNRECOV)) != 0) {
		cc->flag1 |= SAFT_FLG1_ALARM|SAFT_FLG1_GLOBFAIL;
	} else if ((encstat & SES_ENCSTAT_NONCRITICAL) != 0) {
		cc->flag1 |= SAFT_FLG1_GLOBWARN;
	}
	return (wrbuf16(enc, SAFTE_WT_GLOBAL, cc->flag1, cc->flag2, 0, slpflg));
}

static int
safte_get_elm_status(enc_softc_t *enc, encioc_elm_status_t *elms, int slpflg)
{
	int i = (int)elms->elm_idx;

	elms->cstat[0] = enc->enc_cache.elm_map[i].encstat[0];
	elms->cstat[1] = enc->enc_cache.elm_map[i].encstat[1];
	elms->cstat[2] = enc->enc_cache.elm_map[i].encstat[2];
	elms->cstat[3] = enc->enc_cache.elm_map[i].encstat[3];
	return (0);
}


static int
safte_set_elm_status(enc_softc_t *enc, encioc_elm_status_t *elms, int slp)
{
	int idx, err;
	enc_element_t *ep;
	struct scfg *cc;


	ENC_DLOG(enc, "safte_set_objstat(%d): %x %x %x %x\n",
	    (int)elms->elm_idx, elms->cstat[0], elms->cstat[1], elms->cstat[2],
	    elms->cstat[3]);

	/*
	 * If this is clear, we don't do diddly.
	 */
	if ((elms->cstat[0] & SESCTL_CSEL) == 0) {
		return (0);
	}

	err = 0;
	/*
	 * Check to see if the common bits are set and do them first.
	 */
	if (elms->cstat[0] & ~SESCTL_CSEL) {
		err = set_elm_status_sel(enc, elms, slp);
		if (err)
			return (err);
	}

	cc = enc->enc_private;
	if (cc == NULL)
		return (0);

	idx = (int)elms->elm_idx;
	ep = &enc->enc_cache.elm_map[idx];

	switch (ep->enctype) {
	case ELMTYP_DEVICE:
	{
		uint8_t slotop = 0;
		/*
		 * XXX: I should probably cache the previous state
		 * XXX: of SESCTL_DEVOFF so that when it goes from
		 * XXX: true to false I can then set PREPARE FOR OPERATION
		 * XXX: flag in PERFORM SLOT OPERATION write buffer command.
		 */
		if (elms->cstat[2] & (SESCTL_RQSINS|SESCTL_RQSRMV)) {
			slotop |= 0x2;
		}
		if (elms->cstat[2] & SESCTL_RQSID) {
			slotop |= 0x4;
		}
		err = perf_slotop(enc, (uint8_t) idx - (uint8_t) cc->slotoff,
		    slotop, slp);
		if (err)
			return (err);
		if (elms->cstat[3] & SESCTL_RQSFLT) {
			ep->priv |= 0x2;
		} else {
			ep->priv &= ~0x2;
		}
		if (ep->priv & 0xc6) {
			ep->priv &= ~0x1;
		} else {
			ep->priv |= 0x1;	/* no errors */
		}
		wrslot_stat(enc, slp);
		break;
	}
	case ELMTYP_POWER:
		if (elms->cstat[3] & SESCTL_RQSTFAIL) {
			cc->flag1 |= SAFT_FLG1_ENCPWRFAIL;
		} else {
			cc->flag1 &= ~SAFT_FLG1_ENCPWRFAIL;
		}
		err = wrbuf16(enc, SAFTE_WT_GLOBAL, cc->flag1,
		    cc->flag2, 0, slp);
		if (err)
			return (err);
		if (elms->cstat[3] & SESCTL_RQSTON) {
			(void) wrbuf16(enc, SAFTE_WT_ACTPWS,
				idx - cc->pwroff, 0, 0, slp);
		} else {
			(void) wrbuf16(enc, SAFTE_WT_ACTPWS,
				idx - cc->pwroff, 0, 1, slp);
		}
		break;
	case ELMTYP_FAN:
		if (elms->cstat[3] & SESCTL_RQSTFAIL) {
			cc->flag1 |= SAFT_FLG1_ENCFANFAIL;
		} else {
			cc->flag1 &= ~SAFT_FLG1_ENCFANFAIL;
		}
		err = wrbuf16(enc, SAFTE_WT_GLOBAL, cc->flag1,
		    cc->flag2, 0, slp);
		if (err)
			return (err);
		if (elms->cstat[3] & SESCTL_RQSTON) {
			uint8_t fsp;
			if ((elms->cstat[3] & 0x7) == 7) {
				fsp = 4;
			} else if ((elms->cstat[3] & 0x7) == 6) {
				fsp = 3;
			} else if ((elms->cstat[3] & 0x7) == 4) {
				fsp = 2;
			} else {
				fsp = 1;
			}
			(void) wrbuf16(enc, SAFTE_WT_FANSPD, idx, fsp, 0, slp);
		} else {
			(void) wrbuf16(enc, SAFTE_WT_FANSPD, idx, 0, 0, slp);
		}
		break;
	case ELMTYP_DOORLOCK:
		if (elms->cstat[3] & 0x1) {
			cc->flag2 &= ~SAFT_FLG2_LOCKDOOR;
		} else {
			cc->flag2 |= SAFT_FLG2_LOCKDOOR;
		}
		(void) wrbuf16(enc, SAFTE_WT_GLOBAL, cc->flag1,
		    cc->flag2, 0, slp);
		break;
	case ELMTYP_ALARM:
		/*
		 * On all nonzero but the 'muted' bit, we turn on the alarm,
		 */
		elms->cstat[3] &= ~0xa;
		if (elms->cstat[3] & 0x40) {
			cc->flag2 &= ~SAFT_FLG1_ALARM;
		} else if (elms->cstat[3] != 0) {
			cc->flag2 |= SAFT_FLG1_ALARM;
		} else {
			cc->flag2 &= ~SAFT_FLG1_ALARM;
		}
		ep->priv = elms->cstat[3];
		(void) wrbuf16(enc, SAFTE_WT_GLOBAL, cc->flag1,
			cc->flag2, 0, slp);
		break;
	default:
		break;
	}
	ep->svalid = 0;
	return (0);
}

static void
safte_poll_status(enc_softc_t *enc)
{

	enc_update_request(enc, SAFTE_UPDATE_READENCSTATUS);
	enc_update_request(enc, SAFTE_UPDATE_READSLOTSTATUS);
}

static struct enc_vec safte_enc_vec =
{
	.softc_cleanup	= safte_softc_cleanup,
	.init_enc	= safte_init_enc,
	.get_enc_status	= safte_get_enc_status,
	.set_enc_status	= safte_set_enc_status,
	.get_elm_status	= safte_get_elm_status,
	.set_elm_status	= safte_set_elm_status,
	.poll_status	= safte_poll_status
};

int
safte_softc_init(enc_softc_t *enc, int doinit)
{
	if (doinit == 0) {
		safte_softc_cleanup(enc->periph);
		return (0);
	}

	enc->enc_vec = safte_enc_vec;
	enc->enc_fsm_states = enc_fsm_states;

	if (enc->enc_private == NULL) {
		enc->enc_private = ENC_MALLOCZ(SAFT_PRIVATE);
		if (enc->enc_private == NULL) {
			return (ENOMEM);
		}
	}

	enc->enc_cache.nelms = 0;
	enc->enc_cache.enc_status = 0;

	return (0);
}


/*-
 * Copyright (c) 2002, 2003 Sam Leffler, Errno Consulting, Atheros
 * Communications, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the following conditions are met:
 * 1. The materials contained herein are unmodified and are used
 *    unmodified.
 * 2. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following NO
 *    ''WARRANTY'' disclaimer below (''Disclaimer''), without
 *    modification.
 * 3. Redistributions in binary form must reproduce at minimum a
 *    disclaimer similar to the Disclaimer below and any redistribution
 *    must be conditioned upon including a substantially similar
 *    Disclaimer requirement for further binary redistribution.
 * 4. Neither the names of the above-listed copyright holders nor the
 *    names of any contributors may be used to endorse or promote
 *    product derived from this software without specific prior written
 *    permission.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT,
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
 * FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES.
 *
 * $Id: ah_osdep.c,v 1.28 2003/11/01 01:43:21 sam Exp $
 */
#include "opt_ah.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/proc.h>

#include <machine/stdarg.h>

#include <net/ethernet.h>		/* XXX for ether_sprintf */

#include <contrib/dev/ath/ah.h>

extern	void ath_hal_printf(struct ath_hal *, const char*, ...)
		__printflike(2,3);
extern	void ath_hal_vprintf(struct ath_hal *, const char*, __va_list)
		__printflike(2, 0);
extern	const char* ath_hal_ether_sprintf(const u_int8_t *mac);
extern	void *ath_hal_malloc(size_t);
extern	void ath_hal_free(void *);
#ifdef AH_ASSERT
extern	void ath_hal_assert_failed(const char* filename,
		int lineno, const char* msg);
#endif
#ifdef AH_DEBUG
extern	void HALDEBUG(struct ath_hal *ah, const char* fmt, ...);
extern	void HALDEBUGn(struct ath_hal *ah, u_int level, const char* fmt, ...);
#endif /* AH_DEBUG */

/* NB: put this here instead of the driver to avoid circular references */
SYSCTL_NODE(_hw, OID_AUTO, ath, CTLFLAG_RD, 0, "Atheros driver parameters");
SYSCTL_NODE(_hw_ath, OID_AUTO, hal, CTLFLAG_RD, 0, "Atheros HAL parameters");

#ifdef AH_DEBUG
static	int ath_hal_debug = 0;		/* XXX */
SYSCTL_INT(_hw_ath_hal, OID_AUTO, debug, CTLFLAG_RW, &ath_hal_debug,
	    0, "Atheros HAL debugging printfs");
#endif /* AH_DEBUG */

SYSCTL_STRING(_hw_ath_hal, OID_AUTO, version, CTLFLAG_RD, ath_hal_version, 0,
	"Atheros HAL version");

int	ath_hal_dma_beacon_response_time = 2;	/* in TU's */
SYSCTL_INT(_hw_ath_hal, OID_AUTO, dma_brt, CTLFLAG_RW,
	   &ath_hal_dma_beacon_response_time, 0,
	   "Atheros HAL DMA beacon response time");
int	ath_hal_sw_beacon_response_time = 10;	/* in TU's */
SYSCTL_INT(_hw_ath_hal, OID_AUTO, sw_brt, CTLFLAG_RW,
	   &ath_hal_sw_beacon_response_time, 0,
	   "Atheros HAL software beacon response time");
int	ath_hal_additional_swba_backoff = 0;	/* in TU's */
SYSCTL_INT(_hw_ath_hal, OID_AUTO, swba_backoff, CTLFLAG_RW,
	   &ath_hal_additional_swba_backoff, 0,
	   "Atheros HAL additional SWBA backoff time");

void*
ath_hal_malloc(size_t size)
{
	return malloc(size, M_DEVBUF, M_NOWAIT | M_ZERO);
}

void
ath_hal_free(void* p)
{
	return free(p, M_DEVBUF);
}

void
ath_hal_vprintf(struct ath_hal *ah, const char* fmt, va_list ap)
{
	vprintf(fmt, ap);
}

void
ath_hal_printf(struct ath_hal *ah, const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	ath_hal_vprintf(ah, fmt, ap);
	va_end(ap);
}

const char*
ath_hal_ether_sprintf(const u_int8_t *mac)
{
	return ether_sprintf(mac);
}

#ifdef AH_DEBUG
void
HALDEBUG(struct ath_hal *ah, const char* fmt, ...)
{
	if (ath_hal_debug) {
		__va_list ap;
		va_start(ap, fmt);
		ath_hal_vprintf(ah, fmt, ap);
		va_end(ap);
	}
}

void
HALDEBUGn(struct ath_hal *ah, u_int level, const char* fmt, ...)
{
	if (ath_hal_debug >= level) {
		__va_list ap;
		va_start(ap, fmt);
		ath_hal_vprintf(ah, fmt, ap);
		va_end(ap);
	}
}
#endif /* AH_DEBUG */

#ifdef AH_DEBUG_ALQ
/*
 * ALQ register tracing support.
 *
 * Setting hw.ath.hal.alq=1 enables tracing of all register reads and
 * writes to the file /tmp/ath_hal.log.  The file format is a simple
 * fixed-size array of records.  When done logging set hw.ath.hal.alq=0
 * and then decode the file with the arcode program (that is part of the
 * HAL).  If you start+stop tracing the data will be appended to an
 * existing file.
 *
 * NB: doesn't handle multiple devices properly; only one DEVICE record
 *     is emitted and the different devices are not identified.
 */
#include <sys/alq.h>
#include <sys/pcpu.h>
#include <contrib/dev/ath/ah_decode.h>

static	struct alq *ath_hal_alq;
static	int ath_hal_alq_emitdev;	/* need to emit DEVICE record */
static	u_int ath_hal_alq_lost;		/* count of lost records */
static	const char *ath_hal_logfile = "/tmp/ath_hal.log";
static	u_int ath_hal_alq_qsize = 64*1024;

static int
ath_hal_setlogging(int enable)
{
	int error;

	if (enable) {
		error = suser(curthread);
		if (error == 0) {
			error = alq_open(&ath_hal_alq, ath_hal_logfile,
				curthread->td_ucred,
				sizeof (struct athregrec), ath_hal_alq_qsize);
			ath_hal_alq_lost = 0;
			ath_hal_alq_emitdev = 1;
			printf("ath_hal: logging to %s enabled\n",
				ath_hal_logfile);
		}
	} else {
		if (ath_hal_alq)
			alq_close(ath_hal_alq);
		ath_hal_alq = NULL;
		printf("ath_hal: logging disabled\n");
		error = 0;
	}
	return (error);
}

static int
sysctl_hw_ath_hal_log(SYSCTL_HANDLER_ARGS)
{
	int error, enable;

	enable = (ath_hal_alq != NULL);
        error = sysctl_handle_int(oidp, &enable, 0, req);
        if (error || !req->newptr)
                return (error);
	else
		return (ath_hal_setlogging(enable));
}
SYSCTL_PROC(_hw_ath_hal, OID_AUTO, alq, CTLTYPE_INT|CTLFLAG_RW,
	0, 0, sysctl_hw_ath_hal_log, "I", "Enable HAL register logging");
SYSCTL_INT(_hw_ath_hal, OID_AUTO, alq_size, CTLFLAG_RW,
	&ath_hal_alq_qsize, 0, "In-memory log size (#records)");
SYSCTL_INT(_hw_ath_hal, OID_AUTO, alq_lost, CTLFLAG_RW,
	&ath_hal_alq_lost, 0, "Register operations not logged");

static struct ale *
ath_hal_alq_get(struct ath_hal *ah)
{
	struct ale *ale;

	if (ath_hal_alq_emitdev) {
		ale = alq_get(ath_hal_alq, ALQ_NOWAIT);
		if (ale) {
			struct athregrec *r =
				(struct athregrec *) ale->ae_data;
			r->op = OP_DEVICE;
			r->reg = 0;
			r->val = ah->ah_devid;
			alq_post(ath_hal_alq, ale);
			ath_hal_alq_emitdev = 0;
		} else
			ath_hal_alq_lost++;
	}
	ale = alq_get(ath_hal_alq, ALQ_NOWAIT);
	if (!ale)
		ath_hal_alq_lost++;
	return ale;
}

void
ath_hal_reg_write(struct ath_hal *ah, u_int32_t reg, u_int32_t val)
{
	if (ath_hal_alq) {
		struct ale *ale = ath_hal_alq_get(ah);
		if (ale) {
			struct athregrec *r = (struct athregrec *) ale->ae_data;
			r->op = OP_WRITE;
			r->reg = reg;
			r->val = val;
			alq_post(ath_hal_alq, ale);
		}
	}
#if _BYTE_ORDER == _BIG_ENDIAN
	if (reg >= 0x4000 && reg < 0x5000)
		bus_space_write_4(ah->ah_st, ah->ah_sh, reg, htole32(val));
	else
#endif
		bus_space_write_4(ah->ah_st, ah->ah_sh, reg, val);
}

u_int32_t
ath_hal_reg_read(struct ath_hal *ah, u_int32_t reg)
{
	u_int32_t val;

	val = bus_space_read_4(ah->ah_st, ah->ah_sh, reg);
#if _BYTE_ORDER == _BIG_ENDIAN
	if (reg >= 0x4000 && reg < 0x5000)
		val = le32toh(val);
#endif
	if (ath_hal_alq) {
		struct ale *ale = ath_hal_alq_get(ah);
		if (ale) {
			struct athregrec *r = (struct athregrec *) ale->ae_data;
			r->op = OP_READ;
			r->reg = reg;
			r->val = val;
			alq_post(ath_hal_alq, ale);
		}
	}
	return val;
}

void
OS_MARK(struct ath_hal *ah, u_int id, u_int32_t v)
{
	if (ath_hal_alq) {
		struct ale *ale = ath_hal_alq_get(ah);
		if (ale) {
			struct athregrec *r = (struct athregrec *) ale->ae_data;
			r->op = OP_MARK;
			r->reg = id;
			r->val = v;
			alq_post(ath_hal_alq, ale);
		}
	}
}
#elif defined(AH_DEBUG) || defined(AH_REGOPS_FUNC)
/*
 * Memory-mapped device register read/write.  These are here
 * as routines when debugging support is enabled and/or when
 * explicitly configured to use function calls.  The latter is
 * for architectures that might need to do something before
 * referencing memory (e.g. remap an i/o window).
 *
 * NB: see the comments in ah_osdep.h about byte-swapping register
 *     reads and writes to understand what's going on below.
 */

void
ath_hal_reg_write(struct ath_hal *ah, u_int32_t reg, u_int32_t val)
{
#if _BYTE_ORDER == _BIG_ENDIAN
	if (reg >= 0x4000 && reg < 0x5000)
		bus_space_write_4(ah->ah_st, ah->ah_sh, reg, htole32(val));
	else
#endif
		bus_space_write_4(ah->ah_st, ah->ah_sh, reg, val);
}

u_int32_t
ath_hal_reg_read(struct ath_hal *ah, u_int32_t reg)
{
	u_int32_t val;

	val = bus_space_read_4(ah->ah_st, ah->ah_sh, reg);
#if _BYTE_ORDER == _BIG_ENDIAN
	if (reg >= 0x4000 && reg < 0x5000)
		val = le32toh(val);
#endif
	return val;
}
#endif /* AH_DEBUG || AH_REGOPS_FUNC */

#ifdef AH_ASSERT
void
ath_hal_assert_failed(const char* filename, int lineno, const char *msg)
{
	printf("Atheros HAL assertion failure: %s: line %u: %s\n",
		filename, lineno, msg);
	panic("ath_hal_assert");
}
#endif /* AH_ASSERT */

/*
 * Delay n microseconds.
 */
void
ath_hal_delay(int n)
{
	DELAY(n);
}

u_int32_t
ath_hal_getuptime(struct ath_hal *ah)
{
	struct bintime bt;
	getbinuptime(&bt);
	return (bt.sec * 1000) +
		(((uint64_t)1000 * (uint32_t)(bt.frac >> 32)) >> 32);
}

/*
 * Module glue.
 */

static int
ath_hal_modevent(module_t mod, int type, void *unused)
{
	switch (type) {
	case MOD_LOAD:
		if (bootverbose)
			printf("ath_hal: <Atheros Hardware Access Layer>"
				"version %s\n", ath_hal_version);
		return 0;
	case MOD_UNLOAD:
		return 0;
	}
	return EINVAL;
}

static moduledata_t ath_hal_mod = {
	"ath_hal",
	ath_hal_modevent,
	0
};
DECLARE_MODULE(ath_hal, ath_hal_mod, SI_SUB_DRIVERS, SI_ORDER_ANY);
MODULE_VERSION(ath_hal, 1);

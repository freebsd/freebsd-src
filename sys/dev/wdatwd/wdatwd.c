/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Tetsuya Uemura <t_uemura@macome.co.jp>
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
 */

#include <sys/cdefs.h>
#include "opt_acpi.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/callout.h>
#include <sys/eventhandler.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <sys/rman.h>
#include <sys/sysctl.h>
#include <sys/watchdog.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/aclocal.h>
#include <contrib/dev/acpica/include/actables.h>

#include <dev/acpica/acpivar.h>

/*
 * Resource entry. Every instruction has the corresponding ACPI GAS but two or
 * more instructions may access the same or adjacent register region(s). So we
 * need to merge all the specified resources.
 *
 * res   Resource when allocated.
 * start Region start address.
 * end   Region end address + 1.
 * rid   Resource rid assigned when allocated.
 * type  ACPI resource type, SYS_RES_IOPORT or SYS_RES_MEMORY.
 * link  Next/previous resource entry.
 */
struct wdat_res {
	struct resource		*res;
	uint64_t		start;
	uint64_t		end;
	int			rid;
	int			type;
	TAILQ_ENTRY(wdat_res)	link;
};

/*
 * Instruction entry. Every instruction itself is actually a single register
 * read or write (and subsequent bit operation(s)).
 * 0 or more instructions are tied to every watchdog action and once an action
 * is kicked, the corresponding entries run sequentially.
 *
 * entry Permanent copy of ACPI_WDAT_ENTRY entry (sub-table).
 * next  Next instruction entry.
 */
struct wdat_instr {
	ACPI_WDAT_ENTRY		entry;
	STAILQ_ENTRY(wdat_instr) next;
};

/*
 * dev             Watchdog device.
 * wdat            ACPI WDAT table, can be accessed until AcpiPutTable().
 * default_timeout BIOS configured watchdog ticks to fire.
 * timeout         User configured timeout in millisecond or 0 if isn't set.
 * max             Max. supported watchdog ticks to be set.
 * min             Min. supported watchdog ticks to be set.
 * period          Milliseconds per watchdog tick.
 * running         True if this watchdog is running or false if stopped.
 * stop_in_sleep   False if this watchdog keeps counting down during sleep.
 * ev_tag          Tag for EVENTHANDLER_*().
 * action          Array of watchdog instruction sets, each indexed by action.
 */
struct wdatwd_softc {
	device_t		dev;
	ACPI_TABLE_WDAT		*wdat;
	uint64_t		default_timeout;
	uint64_t		timeout;
	u_int			max;
	u_int			min;
	u_int			period;
	bool			running;
	bool			stop_in_sleep;
	eventhandler_tag	ev_tag;
	STAILQ_HEAD(, wdat_instr) action[ACPI_WDAT_ACTION_RESERVED];
	TAILQ_HEAD(res_head, wdat_res) res;
};

#define WDATWD_VERBOSE_PRINTF(dev, ...)					\
	do {								\
		if (bootverbose)					\
			device_printf(dev, __VA_ARGS__);		\
	} while (0)

/*
 * Do requested action.
 */
static int
wdatwd_action(const struct wdatwd_softc *sc, const u_int action, const uint64_t val, uint64_t *ret)
{
	struct wdat_instr	*wdat;
	const char		*rw = NULL;
	ACPI_STATUS		status;

	if (STAILQ_EMPTY(&sc->action[action])) {
		WDATWD_VERBOSE_PRINTF(sc->dev,
		    "action not supported: 0x%02x\n", action);
		return (EOPNOTSUPP);
	}

	STAILQ_FOREACH(wdat, &sc->action[action], next) {
		ACPI_GENERIC_ADDRESS	*gas = &wdat->entry.RegisterRegion;
		uint64_t		x, y;

		switch (wdat->entry.Instruction
		    & ~ACPI_WDAT_PRESERVE_REGISTER) {
		    case ACPI_WDAT_READ_VALUE:
			status = AcpiRead(&x, gas);
			if (ACPI_FAILURE(status)) {
				rw = "AcpiRead";
				goto fail;
			}
			x >>= gas->BitOffset;
			x &= wdat->entry.Mask;
			*ret = (x == wdat->entry.Value) ? 1 : 0;
			break;
		    case ACPI_WDAT_READ_COUNTDOWN:
			status = AcpiRead(&x, gas);
			if (ACPI_FAILURE(status)) {
				rw = "AcpiRead";
				goto fail;
			}
			x >>= gas->BitOffset;
			x &= wdat->entry.Mask;
			*ret = x;
			break;
		    case ACPI_WDAT_WRITE_VALUE:
			x = wdat->entry.Value & wdat->entry.Mask;
			x <<= gas->BitOffset;
			if (wdat->entry.Instruction
			    & ACPI_WDAT_PRESERVE_REGISTER) {
				status = AcpiRead(&y, gas);
				if (ACPI_FAILURE(status)) {
					rw = "AcpiRead";
					goto fail;
				}
				y &= ~(wdat->entry.Mask << gas->BitOffset);
				x |= y;
			}
			status = AcpiWrite(x, gas);
			if (ACPI_FAILURE(status)) {
				rw = "AcpiWrite";
				goto fail;
			}
			break;
		    case ACPI_WDAT_WRITE_COUNTDOWN:
			x = val & wdat->entry.Mask;
			x <<= gas->BitOffset;
			if (wdat->entry.Instruction
			    & ACPI_WDAT_PRESERVE_REGISTER) {
				status = AcpiRead(&y, gas);
				if (ACPI_FAILURE(status)) {
					rw = "AcpiRead";
					goto fail;
				}
				y &= ~(wdat->entry.Mask << gas->BitOffset);
				x |= y;
			}
			status = AcpiWrite(x, gas);
			if (ACPI_FAILURE(status)) {
				rw = "AcpiWrite";
				goto fail;
			}
			break;
		    default:
			return (EINVAL);
		}
	}

	return (0);

fail:
	device_printf(sc->dev, "action: 0x%02x, %s() returned: %d\n",
	    action, rw, status);
	return (ENXIO);
}

/*
 * Reset the watchdog countdown.
 */
static int
wdatwd_reset_countdown(const struct wdatwd_softc *sc)
{
	return wdatwd_action(sc, ACPI_WDAT_RESET, 0, NULL);
}

/*
 * Set the watchdog countdown value. In WDAT specification, this is optional.
 */
static int
wdatwd_set_countdown(struct wdatwd_softc *sc, u_int cmd)
{
	uint64_t		timeout;
	int			e;

	cmd &= WD_INTERVAL;
	timeout = ((uint64_t) 1 << cmd) / 1000000 / sc->period;
	if (timeout > sc->max)
		timeout = sc->max;
	else if (timeout < sc->min)
		timeout = sc->min;

	e = wdatwd_action(sc, ACPI_WDAT_SET_COUNTDOWN, timeout, NULL);
	if (e == 0)
		sc->timeout = timeout * sc->period;

	return (e);
}

/*
 * Get the watchdog current countdown value.
 */
static int
wdatwd_get_current_countdown(const struct wdatwd_softc *sc, uint64_t *timeout)
{
	return wdatwd_action(sc, ACPI_WDAT_GET_CURRENT_COUNTDOWN, 0, timeout);
}

/*
 * Get the watchdog countdown value the watchdog is configured to fire.
 */
static int
wdatwd_get_countdown(const struct wdatwd_softc *sc, uint64_t *timeout)
{
	return wdatwd_action(sc, ACPI_WDAT_GET_COUNTDOWN, 0, timeout);
}

/*
 * Set the watchdog to running state.
 */
static int
wdatwd_set_running(struct wdatwd_softc *sc)
{
	int			e;

	e = wdatwd_action(sc, ACPI_WDAT_SET_RUNNING_STATE, 0, NULL);
	if (e == 0)
		sc->running = true;
	return (e);
}

/*
 * Set the watchdog to stopped state.
 */
static int
wdatwd_set_stop(struct wdatwd_softc *sc)
{
	int			e;

	e = wdatwd_action(sc, ACPI_WDAT_SET_STOPPED_STATE, 0, NULL);
	if (e == 0)
		sc->running = false;
	return (e);
}

/*
 * Clear the watchdog's boot status if the current boot was caused by the
 * watchdog firing.
 */
static int
wdatwd_clear_status(const struct wdatwd_softc *sc)
{
	return wdatwd_action(sc, ACPI_WDAT_SET_STATUS, 0, NULL);
}

/*
 * Set the watchdog to reboot when it is fired.
 */
static int
wdatwd_set_reboot(const struct wdatwd_softc *sc)
{
	return wdatwd_action(sc, ACPI_WDAT_SET_REBOOT, 0, NULL);
}

/*
 * Watchdog event handler.
 */
static void
wdatwd_event(void *private, u_int cmd, int *error)
{
	struct wdatwd_softc	*sc = private;
	uint64_t		cur[2], cnt[2];
	bool			run[2];

	if (bootverbose) {
		run[0] = sc->running;
		if (wdatwd_get_countdown(sc, &cnt[0]) != 0) 
			cnt[0] = 0;
		if (wdatwd_get_current_countdown(sc, &cur[0]) != 0)
			cur[0] = 0;
	}

	if ((cmd & WD_INTERVAL) == 0)
		wdatwd_set_stop(sc);
	else {
		if (!sc->running) {
			/* ACPI_WDAT_SET_COUNTDOWN may not be implemented. */
			wdatwd_set_countdown(sc, cmd);
			wdatwd_set_running(sc);
			/*
			 * In the first wdatwd_event() call, it sets the
			 * watchdog timeout to a considerably larger value such
			 * as 137 seconds, then kicks the watchdog to start
			 * counting down. Weirdly though, on a Dell R210 BIOS
			 * 1.12.0, a supplemental reset action must be
			 * triggered for the newly set timeout value to take
			 * effect. Without it, the watchdog fires 2.4 seconds
			 * after starting, where 2.4 seconds is its initially
			 * set timeout. This failure scenario is seen by first
			 * starting watchdogd(8) without wdatwd registered then
			 * kldload it. In steady state, watchdogd pats the
			 * watchdog every 10 or so seconds which is much longer
			 * than 2.4 seconds timeout.
			 */
		}
		wdatwd_reset_countdown(sc);
	}

	if (bootverbose) {
		run[1] = sc->running;
		if (wdatwd_get_countdown(sc, &cnt[1]) != 0)
			cnt[1] = 0;
		if (wdatwd_get_current_countdown(sc, &cur[1]) != 0)
			cur[1] = 0;
		WDATWD_VERBOSE_PRINTF(sc->dev, "cmd: %u, sc->running: "
		    "%d -> %d, cnt: %llu -> %llu, cur: %llu -> %llu\n", cmd,
				      run[0], run[1], 
				      (unsigned long long) cnt[0],
				      (unsigned long long) cnt[1],
				      (unsigned long long)cur[0],
				      (unsigned long long)cur[1]);
	}

	return;
}

static ssize_t
wdat_set_action(struct wdatwd_softc *sc, ACPI_WDAT_ENTRY *addr, ssize_t remaining)
{
	ACPI_WDAT_ENTRY		*entry = addr;
	struct wdat_instr	*wdat;

	if (remaining < sizeof(ACPI_WDAT_ENTRY))
		return (-EINVAL);

	/* Skip actions beyond specification. */
	if (entry->Action < nitems(sc->action)) {
		wdat = malloc(sizeof(*wdat), M_DEVBUF, M_WAITOK | M_ZERO);
		wdat->entry = *entry;
		STAILQ_INSERT_TAIL(&sc->action[entry->Action], wdat, next);
	}
	return sizeof(ACPI_WDAT_ENTRY);
}

/*
 * Transform every ACPI_WDAT_ENTRY to wdat_instr by calling wdat_set_action().
 */
static void
wdat_parse_action_table(struct wdatwd_softc *sc)
{
	ACPI_TABLE_WDAT		*wdat = sc->wdat;
	ssize_t			remaining, consumed;
	char			*cp;

	remaining = wdat->Header.Length - sizeof(ACPI_TABLE_WDAT);
	while (remaining > 0) {
		cp = (char *)wdat + wdat->Header.Length - remaining;
		consumed = wdat_set_action(sc, (ACPI_WDAT_ENTRY *)cp,
		    remaining);
		if (consumed < 0) {
			device_printf(sc->dev, "inconsistent WDAT table.\n");
			break;
		}
			remaining -= consumed;
	}
}

/*
 * Decode the given GAS rr and set its type, start and end (actually end + 1)
 * in the newly malloc()'ed res.
 */
static struct wdat_res *
wdat_alloc_region(ACPI_GENERIC_ADDRESS *rr)
{
	struct wdat_res *res;

	if (rr->AccessWidth < 1 || rr->AccessWidth > 4)
		return (NULL);

	res = malloc(sizeof(*res),
	    M_DEVBUF, M_WAITOK | M_ZERO);
	if (res != NULL) {
		res->start = rr->Address;
		res->end   = res->start + (1 << (rr->AccessWidth - 1));
		res->type  = rr->SpaceId;
	}
	return (res);
}

#define OVERLAP_NONE	0x0 // no overlap.
#define OVERLAP_SUBSET	0x1 // res2 is fully covered by res1.
#define OVERLAP_START	0x2 // the start of res2 is overlaped.
#define OVERLAP_END	0x4 // the end of res2 is overlapped.

/*
 * Compare the given res1 and res2, and one of the above OVERLAP_* constant, or
 * in case res2 is larger than res1 at both the start and the end,
 * OVERLAP_START | OVERLAP_END, is returned.
 */
static int
wdat_compare_region(const struct wdat_res *res1, const struct wdat_res *res2)
{
	int overlap;

	/*
	 * a) both have different resource type. == OVERLAP_NONE
	 * b) res2 and res1 have no overlap.     == OVERLAP_NONE
	 * c) res2 is fully covered by res1.     == OVERLAP_SUBSET
	 * d) res2 and res1 overlap partially.   == OVERLAP_START or
	 * 					    OVERLAP_END
	 * e) res2 fully covers res1.            == OVERLAP_START | OVERLAP_END
	 */
	overlap = 0;

	if (res1->type != res2->type || res1->start > res2->end
	    || res1->end < res2->start)
		overlap |= OVERLAP_NONE;
	else {
		if (res1->start <= res2->start && res1->end >= res2->end)
			overlap |= OVERLAP_SUBSET;
		if (res1->start > res2->start)
			overlap |= OVERLAP_START;
		if (res1->end < res2->end)
			overlap |= OVERLAP_END;
	}

	return (overlap);
}

/*
 * Try to merge the given newres with the existing sc->res.
 */
static void
wdat_merge_region(struct wdatwd_softc *sc, struct wdat_res *newres)
{
	struct wdat_res		*res1, *res2, *res_safe, *res_itr;
	int			overlap;

	if (TAILQ_EMPTY(&sc->res)) {
		TAILQ_INSERT_HEAD(&sc->res, newres, link);
		return;
	}

	overlap = OVERLAP_NONE;

	TAILQ_FOREACH_SAFE(res1, &sc->res, link, res_safe) {
		overlap = wdat_compare_region(res1, newres);

		/* Try next res if newres isn't mergeable. */
		if (overlap == OVERLAP_NONE)
			continue;

		/* This res fully covers newres. */
		if (overlap == OVERLAP_SUBSET)
			break;

		/* Newres extends the existing res res1 to lower. */
		if ((overlap & OVERLAP_START)) {
			res1->start = newres->start;
			res_itr = res1;
			/* Try to merge more res if possible. */
			while ((res2 = TAILQ_PREV(res_itr, res_head, link))) {
				if (res1->type != res2->type) {
					res_itr = res2;
					continue;
				} else if (res1->start <= res2->end) {
					res1->start = res2->start;
					TAILQ_REMOVE(&sc->res, res2, link);
					free(res2, M_DEVBUF);
				} else
					break;
			}
		}
		/* Newres extends the existing res res1 to upper. */
		if ((overlap & OVERLAP_END)) {
			res1->end = newres->end;
			res_itr = res1;
			/* Try to merge more res if possible. */
			while ((res2 = TAILQ_NEXT(res_itr, link))) {
				if (res1->type != res2->type) {
					res_itr = res2;
					continue;
				} else if (res1->end >= res2->start) {
					res1->end = res2->end;
					TAILQ_REMOVE(&sc->res, res2, link);
					free(res2, M_DEVBUF);
				} else
					break;
			}
		}
		break;
	}

	/*
	 * If newres extends the existing res, newres must be free()'ed.
	 * Otherwise insert newres into sc->res at appropriate position
	 * (the lowest address region appears first).
	 */
	if (overlap > OVERLAP_NONE)
		free(newres, M_DEVBUF);
	else {
		TAILQ_FOREACH(res1, &sc->res, link) {
			if (newres->type != res1->type)
				continue;
			if (newres->start < res1->start) {
				TAILQ_INSERT_BEFORE(res1, newres, link);
				break;
			}
		}
		if (res1 == NULL)
			TAILQ_INSERT_TAIL(&sc->res, newres, link);
	}
}

/*
 * Release the already allocated resource.
 */
static void
wdat_release_resource(device_t dev)
{
	struct wdatwd_softc	*sc;
	struct wdat_instr	*wdat;
	struct wdat_res		*res;
	int			i;

	sc = device_get_softc(dev);

	TAILQ_FOREACH(res, &sc->res, link)
		if (res->res != NULL) {
			bus_release_resource(dev, res->type,
			    res->rid, res->res);
			bus_delete_resource(dev, res->type, res->rid);
			res->res = NULL;
		}

	for (i = 0; i < nitems(sc->action); ++i)
		while (!STAILQ_EMPTY(&sc->action[i])) {
			wdat = STAILQ_FIRST(&sc->action[i]);
			STAILQ_REMOVE_HEAD(&sc->action[i], next);
			free(wdat, M_DEVBUF);
		}

	while (!TAILQ_EMPTY(&sc->res)) {
		res = TAILQ_FIRST(&sc->res);
		TAILQ_REMOVE(&sc->res, res, link);
		free(res, M_DEVBUF);
	}
}

static int
wdatwd_probe(device_t dev)
{
	ACPI_TABLE_WDAT		*wdat;
	ACPI_STATUS		status;

	/* Without WDAT table we have nothing to do. */
	status = AcpiGetTable(ACPI_SIG_WDAT, 0, (ACPI_TABLE_HEADER **)&wdat);
	if (ACPI_FAILURE(status))
		return (ENXIO);

	/* Try to allocate one resource and assume wdatwd is already attached
	 * if it fails. */
	{
		int		type, rid = 0;
		struct resource *res;

		if (acpi_bus_alloc_gas(dev, &type, &rid,
		    &((ACPI_WDAT_ENTRY *)(wdat + 1))->RegisterRegion,
		    &res, 0))
			return (ENXIO);
		bus_release_resource(dev, type, rid, res);
		bus_delete_resource(dev, type, rid);
	}

	WDATWD_VERBOSE_PRINTF(dev, "Flags: 0x%x, TimerPeriod: %d ms/cnt, "
	    "MaxCount: %d cnt (%d ms), MinCount: %d cnt (%d ms)\n",
	    (int)wdat->Flags, (int)wdat->TimerPeriod,
	    (int)wdat->MaxCount, (int)(wdat->MaxCount * wdat->TimerPeriod),
	    (int)wdat->MinCount, (int)(wdat->MinCount * wdat->TimerPeriod));
	/* WDAT timer consistency. */
	if ((wdat->TimerPeriod < 1) || (wdat->MinCount > wdat->MaxCount)) {
		device_printf(dev, "inconsistent timer variables.\n");
		return (EINVAL);
	}

	AcpiPutTable((ACPI_TABLE_HEADER *)wdat);

	device_set_desc(dev, "ACPI WDAT Watchdog Interface");
	return (BUS_PROBE_DEFAULT);
}

static int
wdatwd_attach(device_t dev)
{
	struct wdatwd_softc	*sc;
	struct wdat_instr	*wdat;
	struct wdat_res		*res;
	struct sysctl_ctx_list	*sctx;
	struct sysctl_oid	*soid;
	ACPI_STATUS		status;
	int			e, i, rid;

	sc = device_get_softc(dev);
	sc->dev = dev;

	for (i = 0; i < nitems(sc->action); ++i)
		STAILQ_INIT(&sc->action[i]);

	/* Search and parse WDAT table. */
	status = AcpiGetTable(ACPI_SIG_WDAT, 0,
	    (ACPI_TABLE_HEADER **)&sc->wdat);
	if (ACPI_FAILURE(status))
		return (ENXIO);

	/* Parse watchdog variables. */
	sc->period = sc->wdat->TimerPeriod;
	sc->max = sc->wdat->MaxCount;
	sc->min = sc->wdat->MinCount;
	sc->stop_in_sleep = (sc->wdat->Flags & ACPI_WDAT_STOPPED)
	    ? true : false;
	/* Parse defined watchdog actions. */
	wdat_parse_action_table(sc);

	AcpiPutTable((ACPI_TABLE_HEADER *)sc->wdat);

	/* Verbose logging. */
	if (bootverbose) {
		for (i = 0; i < nitems(sc->action); ++i)
			STAILQ_FOREACH(wdat, &sc->action[i], next) {
				WDATWD_VERBOSE_PRINTF(dev, "action: 0x%02x, "
				    "%s %s at 0x%llx (%d bit(s), offset %d bit(s))\n",
				    i,
				    wdat->entry.RegisterRegion.SpaceId
					== ACPI_ADR_SPACE_SYSTEM_MEMORY
					? "mem"
					: wdat->entry.RegisterRegion.SpaceId
					    == ACPI_ADR_SPACE_SYSTEM_IO
					    ? "io "
					    : "???",
				    wdat->entry.RegisterRegion.AccessWidth == 1
					? "byte "
					: wdat->entry.RegisterRegion.AccessWidth == 2
					    ? "word "
					    : wdat->entry.RegisterRegion.AccessWidth == 3
						? "dword"
						: wdat->entry.RegisterRegion.AccessWidth == 4
						    ? "qword"
						    : "undef",
				    (unsigned long long )
				    wdat->entry.RegisterRegion.Address,
				    wdat->entry.RegisterRegion.BitWidth,
				    wdat->entry.RegisterRegion.BitOffset);
		}
	}

	/* Canonicalize the requested resources. */
	TAILQ_INIT(&sc->res);
	for (i = 0; i < nitems(sc->action); ++i)
		STAILQ_FOREACH(wdat, &sc->action[i], next) {
			res = wdat_alloc_region(&wdat->entry.RegisterRegion);
			if (res == NULL)
				goto fail;
			wdat_merge_region(sc, res);
		}

	/* Resource allocation. */
	rid = 0;
	TAILQ_FOREACH(res, &sc->res, link) {
		switch (res->type) {
		    case ACPI_ADR_SPACE_SYSTEM_MEMORY:
			res->type = SYS_RES_MEMORY;
			break;
		    case ACPI_ADR_SPACE_SYSTEM_IO:
			res->type = SYS_RES_IOPORT;
			break;
		    default:
			goto fail;
		}

		res->rid = rid++;
		bus_set_resource(dev, res->type, res->rid,
		    res->start, res->end - res->start);
		res->res = bus_alloc_resource_any(
		    dev, res->type, &res->rid, RF_ACTIVE);
		if (res->res == NULL) {
			bus_delete_resource(dev, res->type, res->rid);
			device_printf(dev, "%s at 0x%llx (%lld byte(s)): "
			    "alloc' failed\n",
			    res->type == SYS_RES_MEMORY ? "mem" : "io ",
			    (unsigned long long )res->start,
			    (unsigned long long )(res->end - res->start));
			goto fail;
		}
		WDATWD_VERBOSE_PRINTF(dev, "%s at 0x%llx (%lld byte(s)): "
		    "alloc'ed\n",
		    res->type == SYS_RES_MEMORY ? "mem" : "io ",
		    (unsigned long long )res->start,
		    (unsigned long long) (res->end - res->start));
	}

	/* Initialize the watchdog hardware. */
	if (wdatwd_set_stop(sc) != 0)
		goto fail;
	if ((e = wdatwd_clear_status(sc)) && e != EOPNOTSUPP)
		goto fail;
	if ((e = wdatwd_set_reboot(sc)) && e != EOPNOTSUPP)
		goto fail;
	if ((e = wdatwd_get_countdown(sc, &sc->default_timeout))
	    && e != EOPNOTSUPP)
		goto fail;
	WDATWD_VERBOSE_PRINTF(dev, "initialized.\n");

	/* Some sysctls. Most of them should go to WDATWD_VERBOSE_PRINTF(). */
	sctx = device_get_sysctl_ctx(dev);
	soid = device_get_sysctl_tree(dev);
	SYSCTL_ADD_U64(sctx, SYSCTL_CHILDREN(soid), OID_AUTO,
	    "timeout_default", CTLFLAG_RD, SYSCTL_NULL_U64_PTR,
	    sc->default_timeout * sc->period,
	    "The default watchdog timeout in millisecond.");
	SYSCTL_ADD_BOOL(sctx, SYSCTL_CHILDREN(soid), OID_AUTO,
	    "timeout_configurable", CTLFLAG_RD, SYSCTL_NULL_BOOL_PTR,
	    STAILQ_EMPTY(&sc->action[ACPI_WDAT_SET_COUNTDOWN]) ? false : true,
	    "Whether the watchdog timeout is configurable or not.");
	SYSCTL_ADD_U64(sctx, SYSCTL_CHILDREN(soid), OID_AUTO,
	    "timeout", CTLFLAG_RD, &sc->timeout, 0,
	    "The current watchdog timeout in millisecond. "
	    "If 0, the default timeout is used.");
	SYSCTL_ADD_BOOL(sctx, SYSCTL_CHILDREN(soid), OID_AUTO,
	    "running", CTLFLAG_RD, &sc->running, 0,
	    "Whether the watchdog timer is running or not.");

	sc->ev_tag = EVENTHANDLER_REGISTER(watchdog_list, wdatwd_event, sc,
	    EVENTHANDLER_PRI_ANY);
	WDATWD_VERBOSE_PRINTF(dev, "watchdog registered.\n");

	return (0);

fail:
	wdat_release_resource(dev);

	return (ENXIO);
}

static int
wdatwd_detach(device_t dev)
{
	struct wdatwd_softc	*sc;
	int			e;

	sc = device_get_softc(dev);

	EVENTHANDLER_DEREGISTER(watchdog_list, sc->ev_tag);
	e = wdatwd_set_stop(sc);
	wdat_release_resource(dev);

	return (e);
}

static int
wdatwd_suspend(device_t dev)
{
	struct wdatwd_softc	*sc;

	sc = device_get_softc(dev);

	if (!sc->stop_in_sleep)
		return (0);

	return wdatwd_set_stop(sc);
}

static int
wdatwd_resume(device_t dev)
{
	struct wdatwd_softc	*sc;

	sc = device_get_softc(dev);

	if (!sc->stop_in_sleep)
		return (0);

	return (wdatwd_reset_countdown(sc) || wdatwd_set_running(sc));
}

static device_method_t wdatwd_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, wdatwd_probe),
	DEVMETHOD(device_attach, wdatwd_attach),
	DEVMETHOD(device_detach, wdatwd_detach),
	DEVMETHOD(device_shutdown, wdatwd_detach),
	DEVMETHOD(device_suspend, wdatwd_suspend),
	DEVMETHOD(device_resume, wdatwd_resume),
	DEVMETHOD_END
};

static driver_t	wdatwd_driver = {
	"wdatwd",
	wdatwd_methods,
	sizeof(struct wdatwd_softc),
};

DRIVER_MODULE(wdatwd, acpi, wdatwd_driver, 0, 0);
MODULE_DEPEND(wdatwd, acpi, 1, 1, 1);

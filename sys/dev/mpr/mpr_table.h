/*-
 * Copyright (c) 2009 Yahoo! Inc.
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

#ifndef _MPR_TABLE_H
#define _MPR_TABLE_H

struct mpr_table_lookup {
	char	*string;
	u_int	code;
};

char * mpr_describe_table(struct mpr_table_lookup *table, u_int code);
void mpr_describe_devinfo(uint32_t devinfo, char *string, int len);

extern struct mpr_table_lookup mpr_event_names[];
extern struct mpr_table_lookup mpr_phystatus_names[];
extern struct mpr_table_lookup mpr_linkrate_names[];

void _mpr_print_iocfacts(struct mpr_softc *, MPI2_IOC_FACTS_REPLY *);
void _mpr_print_portfacts(struct mpr_softc *, MPI2_PORT_FACTS_REPLY *);
void _mpr_print_event(struct mpr_softc *, MPI2_EVENT_NOTIFICATION_REPLY *);
void _mpr_print_sasdev0(struct mpr_softc *, MPI2_CONFIG_PAGE_SAS_DEV_0 *);
void _mpr_print_evt_sas(struct mpr_softc *, MPI2_EVENT_NOTIFICATION_REPLY *);
void _mpr_print_expander1(struct mpr_softc *, MPI2_CONFIG_PAGE_EXPANDER_1 *);
void _mpr_print_sasphy0(struct mpr_softc *, MPI2_CONFIG_PAGE_SAS_PHY_0 *);
void mpr_print_sgl(struct mpr_softc *, struct mpr_command *, int);
void mpr_print_scsiio_cmd(struct mpr_softc *, struct mpr_command *);

static __inline void
mpr_print_iocfacts(struct mpr_softc *sc, MPI2_IOC_FACTS_REPLY *facts)
{
	if (sc->mpr_debug & MPR_XINFO)
		_mpr_print_iocfacts(sc, facts);
}

static __inline void
mpr_print_portfacts(struct mpr_softc *sc, MPI2_PORT_FACTS_REPLY *facts)
{
	if (sc->mpr_debug & MPR_XINFO)
		_mpr_print_portfacts(sc, facts);
}

static __inline void
mpr_print_event(struct mpr_softc *sc, MPI2_EVENT_NOTIFICATION_REPLY *event)
{
	if (sc->mpr_debug & MPR_EVENT)
		_mpr_print_event(sc, event);
}

static __inline void
mpr_print_evt_sas(struct mpr_softc *sc, MPI2_EVENT_NOTIFICATION_REPLY *event)
{
	if (sc->mpr_debug & MPR_EVENT)
		_mpr_print_evt_sas(sc, event);
}

static __inline void
mpr_print_sasdev0(struct mpr_softc *sc, MPI2_CONFIG_PAGE_SAS_DEV_0 *buf)
{
	if (sc->mpr_debug & MPR_XINFO)
		_mpr_print_sasdev0(sc, buf);
}

static __inline void
mpr_print_expander1(struct mpr_softc *sc, MPI2_CONFIG_PAGE_EXPANDER_1 *buf)
{
	if (sc->mpr_debug & MPR_XINFO)
		_mpr_print_expander1(sc, buf);
}

static __inline void
mpr_print_sasphy0(struct mpr_softc *sc, MPI2_CONFIG_PAGE_SAS_PHY_0 *buf)
{
	if (sc->mpr_debug & MPR_XINFO)
		_mpr_print_sasphy0(sc, buf);
}

#endif

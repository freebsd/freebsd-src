/*-
 * Copyright (c) 2002 Matthew N. Dodd <winter@jurai.net>
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
 */
/*-
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <sys/bus.h>
#include <sys/conf.h>

#include <sys/module.h>
#include <machine/bus.h>
#include <machine/ofw_machdep.h>
#include <machine/resource.h>

#include <sys/rman.h> 

#include <dev/ofw/openfirm.h>

#include <sparc64/sbus/sbusreg.h>
#include <sparc64/sbus/sbusvar.h>

#include <net/if.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>

#include <dev/hfa/fore.h>
#include <dev/hfa/fore_aali.h>
#include <dev/hfa/fore_slave.h>
#include <dev/hfa/fore_stats.h>
#include <dev/hfa/fore_var.h>
#include <dev/hfa/fore_include.h>

#include <dev/hfa/hfa_freebsd.h>

static int hfa_sbus_probe(device_t);
static int hfa_sbus_attach(device_t);

static int
hfa_sbus_probe (device_t dev)
{

	return (ENXIO);
}

static int
hfa_sbus_attach (device_t dev)
{

	return (ENXIO);
}

static device_method_t hfa_sbus_methods[] = {
	DEVMETHOD(device_probe,		hfa_sbus_probe),
	DEVMETHOD(device_attach,	hfa_sbus_attach),

	DEVMETHOD(device_detach,	hfa_detach),

	{ 0, 0 }
};

static driver_t hfa_sbus_driver = {
	"hfa",
	hfa_sbus_methods,
	sizeof(struct hfa_softc)
};

DRIVER_MODULE(hfa, sbus, hfa_sbus_driver, hfa_devclass, 0, 0);
MODULE_DEPEND(hfa, hfa, 1, 1, 1);
MODULE_DEPEND(hfa, sbus, 1, 1, 1);

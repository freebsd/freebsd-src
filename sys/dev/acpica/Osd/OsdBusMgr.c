/*-
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2000 BSDi
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
 *	$FreeBSD$
 */

/*
 * OSD interfaces for the BusMgr module
 */

#include "acpi.h"
#include "bmosd.h"

#include <sys/eventhandler.h>
#include <sys/reboot.h>

#define _COMPONENT	ACPI_OS_SERVICES
MODULE_NAME("BUSMGR")

struct osd_eventhandle {
    eventhandler_tag	tag;
    OSD_IDLE_HANDLER	Function;
    void		*Context;
};

#define NUM_IDLEHANDLERS	10
struct osd_eventhandle idlehandlers[NUM_IDLEHANDLERS] = {{0, 0}};
#define NUM_SHUTDOWNHANDLERS	20
struct osd_eventhandle shutdownhandlers[NUM_SHUTDOWNHANDLERS] = {{0, 0}};

static void
osd_idlehandler(void *arg, int junk)
{
    struct osd_eventhandle	*oh = (struct osd_eventhandle *)arg;

    FUNCTION_TRACE(__func__);

    oh->Function(oh->Context);
    return_VOID();
}

static void
osd_shutdownhandler(void *arg, int howto)
{
    struct osd_eventhandle	*oh = (struct osd_eventhandle *)arg;

    FUNCTION_TRACE(__func__);

    oh->Function(oh->Context);
    return_VOID();
}

ACPI_STATUS
AcpiOsInstallIdleHandler(OSD_IDLE_HANDLER Function, void *Context)
{
    int	i;

    FUNCTION_TRACE(__func__);

    if (Function == NULL)
	return_ACPI_STATUS(AE_BAD_PARAMETER);
    for (i = 0; i < NUM_IDLEHANDLERS; i++) {
	if (idlehandlers[i].Function == NULL) {
	    idlehandlers[i].Function = Function;
	    idlehandlers[i].Context = Context;
	    idlehandlers[i].tag = EVENTHANDLER_FAST_REGISTER(idle_event, osd_idlehandler, 
							     &idlehandlers[i], IDLE_PRI_FIRST);
	    return_ACPI_STATUS(AE_OK);
	}
    }
    return_ACPI_STATUS(AE_NO_MEMORY);
}

ACPI_STATUS
AcpiOsRemoveIdleHandler(OSD_IDLE_HANDLER Function)
{
    int i;

    FUNCTION_TRACE(__func__);

    if (Function == NULL)
	return_ACPI_STATUS(AE_BAD_PARAMETER);
    for (i = 0; i < NUM_IDLEHANDLERS; i++) {
	if (idlehandlers[i].Function == Function) {
	    EVENTHANDLER_FAST_DEREGISTER(idle_event, idlehandlers[i].tag);
	    idlehandlers[i].Function = NULL;
	    return_ACPI_STATUS(AE_OK);
	}
    }
    return_ACPI_STATUS(AE_NOT_EXIST);
}

/*
 * It's not clear where exactly in the shutdown order these should be
 * queued.
 */
ACPI_STATUS
AcpiOsInstallShutdownHandler(OSD_SHUTDOWN_HANDLER Function, void *Context)
{
    int	i;

    FUNCTION_TRACE(__func__);

    if (Function == NULL)
	return_ACPI_STATUS(AE_BAD_PARAMETER);
    for (i = 0; i < NUM_SHUTDOWNHANDLERS; i++) {
	if (shutdownhandlers[i].Function == NULL) {
	    shutdownhandlers[i].Function = Function;
	    shutdownhandlers[i].Context = Context;
	    shutdownhandlers[i].tag = EVENTHANDLER_REGISTER(shutdown_final, osd_shutdownhandler, 
							     &shutdownhandlers[i], SHUTDOWN_PRI_LAST);
	    return_ACPI_STATUS(AE_OK);
	}
    }
    return_ACPI_STATUS(AE_NO_MEMORY);
}

ACPI_STATUS
AcpiOsRemoveShutdownHandler(OSD_SHUTDOWN_HANDLER Function)
{
    int i;

    FUNCTION_TRACE(__func__);

    if (Function == NULL)
	return_ACPI_STATUS(AE_BAD_PARAMETER);
    for (i = 0; i < NUM_SHUTDOWNHANDLERS; i++) {
	if (shutdownhandlers[i].Function == Function) {
	    EVENTHANDLER_DEREGISTER(shutdown_final, shutdownhandlers[i].tag);
	    shutdownhandlers[i].Function = NULL;
	    return_ACPI_STATUS(AE_OK);
	}
    }
    return_ACPI_STATUS(AE_NOT_EXIST);
}

ACPI_STATUS
AcpiOsShutdown (void)
{
    FUNCTION_TRACE(__func__);

    shutdown_nice(0);
    return_VOID();
}

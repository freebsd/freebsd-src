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
 * 6.8 : Debugging support
 */

#include "opt_ddb.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cons.h>
#include <sys/kernel.h>

#include <sys/bus.h>
#include <machine/resource.h>
#include <machine/bus.h>
#include <sys/rman.h>

#include <ddb/ddb.h>
#include <ddb/db_output.h>

#include "acpi.h"
#include "acdebug.h"
#include <dev/acpica/acpivar.h>

ACPI_STATUS
AcpiOsBreakpoint(NATIVE_CHAR *Message)
{
    Debugger(Message);
    return(AE_OK);
}

UINT32
AcpiOsGetLine(NATIVE_CHAR *Buffer)
{
#ifdef DDB
    char	*cp;

    db_readline(Buffer, 80);
    for (cp = Buffer; *cp != 0; cp++)
	if (*cp == '\n')
	    *cp = 0;
    return(AE_OK);
#else
    printf("AcpiOsGetLine called but no input support");
    return(AE_NOT_EXIST);
#endif
}

void
AcpiOsDbgAssert(void *FailedAssertion, void *FileName, UINT32 LineNumber, NATIVE_CHAR *Message)
{
    printf("ACPI: %s:%d - %s\n", (char *)FileName, LineNumber, Message);
    printf("ACPI: assertion  %s\n", (char *)FailedAssertion);
}

#ifdef ENABLE_DEBUGGER
void
acpi_EnterDebugger(void)
{
    ACPI_PARSE_OBJECT	obj;
    static int		initted = 0;

    if (!initted) {
	printf("Initialising ACPICA debugger...\n");
	AcpiDbInitialize();
	initted = 1;
    }

    printf("Entering ACPICA debugger...\n");
    AcpiDbUserCommands('A', &obj);
}
#endif

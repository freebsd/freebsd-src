/* local type decs. and macro defs.

   Copyright 1993 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "udiphcfg.h"	/* Get host specific configuration */
#include "udiptcfg.h"	/* Get target specific configuration */

/* Here are all of the CPU Families for which UDI is currently defined */
#define Am29K		1	/* AMD's Am290xx and Am292xx parts */

typedef UDIInt		UDIError;
typedef UDIInt		UDISessionId;
typedef	UDIInt		UDIPId;
typedef	UDIInt		UDIStepType;
typedef	UDIInt		UDIBreakType;
typedef	UDIUInt		UDIBreakId;
typedef UDIUInt		UDIMode;

typedef UDIStruct
{			
    CPUSpace	Space;
    CPUOffset	Offset;
} UDIResource;

typedef	UDIStruct
{
    CPUOffset	Low;
    CPUOffset	High;
} UDIRange;

typedef UDIStruct
{
    CPUSpace	Space;
    CPUOffset	Offset;
    CPUSizeT	Size;
    } UDIMemoryRange;

/* Values for UDIStepType parameters */
#define UDIStepNatural		0x0000
#define UDIStepOverTraps	0x0001
#define UDIStepOverCalls	0x0002
#define UDIStepInRange		0x0004
#define UDIStepNatural		0x0000

/* Values for UDIBreakType parameters */
#define UDIBreakFlagExecute	0x0001
#define UDIBreakFlagRead	0x0002
#define UDIBreakFlagWrite	0x0004
#define UDIBreakFlagFetch	0x0008

/* Special values for UDIWait MaxTime parameter */
#define UDIWaitForever	(UDIInt32) -1	/* Infinite time delay */

/* Special values for PId */
#define UDIProcessProcessor	-1	/* Raw Hardware, if possible */

/* Values for UDIWait StopReason */
#define UDIGrossState		0xff
#define UDITrapped		0	/* Fine state - which trap */
#define UDINotExecuting		1
#define UDIRunning		2
#define UDIStopped		3
#define UDIWarned		4
#define UDIStepped		5
#define UDIWaiting		6
#define UDIHalted		7
#define UDIStdoutReady		8	/* fine state - size */
#define UDIStderrReady		9	/* fine state - size */
#define UDIStdinNeeded		10	/* fine state - size */
#define UDIStdinModeX  		11	/* fine state - mode */
#define UDIBreak		12	/* Fine state - Breakpoint Id */
#define UDIExited		13	/* Fine state - exit code */

/* Enumerate the return values from the callback function
   for UDIEnumerateTIPs.
*/
#define UDITerminateEnumeration	0
#define UDIContinueEnumeration	1

/* Enumerate values for Terminate parameter to UDIDisconnect */
#define UDITerminateSession	1
#define UDIContinueSession	0

/* Error codes */
#define UDINoError			0	/* No error occured */
#define UDIErrorNoSuchConfiguration	1
#define UDIErrorCantHappen		2
#define UDIErrorCantConnect		3
#define UDIErrorNoSuchConnection	4
#define UDIErrorNoConnection		5
#define UDIErrorCantOpenConfigFile	6
#define UDIErrorCantStartTIP		7
#define UDIErrorConnectionUnavailable	8
#define UDIErrorTryAnotherTIP		9
#define UDIErrorExecutableNotTIP	10
#define UDIErrorInvalidTIPOption	11
#define UDIErrorCantDisconnect		12
#define UDIErrorUnknownError		13
#define UDIErrorCantCreateProcess	14
#define UDIErrorNoSuchProcess		15
#define UDIErrorUnknownResourceSpace	16
#define UDIErrorInvalidResource		17
#define UDIErrorUnsupportedStepType	18
#define UDIErrorCantSetBreakpoint	19
#define UDIErrorTooManyBreakpoints	20
#define UDIErrorInvalidBreakId		21
#define UDIErrorNoMoreBreakIds		22
#define UDIErrorUnsupportedService	23
#define UDIErrorTryAgain		24
#define UDIErrorIPCLimitation		25
#define UDIErrorIncomplete		26
#define UDIErrorAborted			27
#define UDIErrorTransDone		28
#define UDIErrorCantAccept		29
#define UDIErrorTransInputNeeded	30
#define UDIErrorTransModeX		31
#define UDIErrorInvalidSize		32
#define UDIErrorBadConfigFileEntry	33
#define UDIErrorIPCInternal		34
/* TBD */

/****************************************************************** PROCEDURES
*/

UDIError UDIConnect UDIParams((
  char		*Configuration,		/* In */
  UDISessionId	*Session		/* Out */
  ));

UDIError UDIDisconnect UDIParams((
  UDISessionId	Session,		/* In */
  UDIBool	Terminate		/* In */
  ));

UDIError UDISetCurrentConnection UDIParams((
  UDISessionId	Session			/* In */
  ));

UDIError UDICapabilities UDIParams((
  UDIUInt32	*TIPId,			/* Out */
  UDIUInt32	*TargetId,		/* Out */
  UDIUInt32	DFEId,			/* In */
  UDIUInt32	DFE,			/* In */
  UDIUInt32	*TIP,			/* Out */
  UDIUInt32	*DFEIPCId,		/* Out */
  UDIUInt32	*TIPIPCId,		/* Out */
  char		*TIPString		/* Out */
  ));

UDIError UDIEnumerateTIPs UDIParams((
  UDIInt	(*UDIETCallback)	/* In */
    UDIParams(( char *Configuration ))	/* In to callback() */
  ));

UDIError UDIGetErrorMsg UDIParams((
  UDIError	ErrorCode,		/* In */
  UDISizeT	MsgSize,		/* In */
  char		*Msg,			/* Out */
  UDISizeT	*CountDone		/* Out */
  ));

UDIError UDIGetTargetConfig UDIParams((
  UDIMemoryRange KnownMemory[],		/* Out */
  UDIInt	*NumberOfRanges,	/* In/Out */
  UDIUInt32	ChipVersions[],		/* Out */
  UDIInt	*NumberOfChips		/* In/Out */
  ));

UDIError UDICreateProcess UDIParams((
  UDIPId	*PId			/* Out */
  ));

UDIError UDISetCurrentProcess UDIParams((
  UDIPId	PId			/* In */
  ));

UDIError UDIDestroyProcess UDIParams((
  UDIPId	PId			/* In */
  ));

UDIError UDIInitializeProcess UDIParams((
  UDIMemoryRange ProcessMemory[],	/* In */
  UDIInt	NumberOfRanges,		/* In */
  UDIResource	EntryPoint,		/* In */
  CPUSizeT	StackSizes[],		/* In */
  UDIInt	NumberOfStacks,		/* In */
  char		*ArgString		/* In */
  ));

UDIError UDIRead UDIParams((
  UDIResource	From,			/* In */
  UDIHostMemPtr	To,			/* Out */
  UDICount	Count,			/* In */
  UDISizeT	Size,			/* In */
  UDICount	*CountDone,		/* Out */
  UDIBool	HostEndian		/* In */
  ));

UDIError UDIWrite UDIParams((
  UDIHostMemPtr	From,			/* In */
  UDIResource	To,			/* In */
  UDICount	Count,			/* In */
  UDISizeT	Size,			/* In */
  UDICount	*CountDone,		/* Out */
  UDIBool	HostEndian		/* In */
  ));

UDIError UDICopy UDIParams((
  UDIResource	From,			/* In */
  UDIResource	To,			/* In */
  UDICount	Count,			/* In */
  UDISizeT	Size,			/* In */
  UDICount	*CountDone,		/* Out */
  UDIBool	Direction		/* In */
  ));

UDIError UDIExecute UDIParams((
  void
  ));

UDIError UDIStep UDIParams((
  UDIUInt32	Steps,			/* In */
  UDIStepType   StepType,		/* In */
  UDIRange      Range			/* In */
  ));

UDIVoid UDIStop UDIParams((
  void
  ));

UDIError UDIWait UDIParams((
  UDIInt32	MaxTime,		/* In */
  UDIPId	*PId,			/* Out */
  UDIUInt32	*StopReason		/* Out */
  ));

UDIError UDISetBreakpoint UDIParams((
  UDIResource	Addr,			/* In */
  UDIInt32	PassCount,		/* In */
  UDIBreakType	Type,			/* In */
  UDIBreakId	*BreakId		/* Out */
  ));

UDIError UDIQueryBreakpoint UDIParams((
  UDIBreakId	BreakId,		/* In */
  UDIResource	*Addr,			/* Out */
  UDIInt32	*PassCount,		/* Out */
  UDIBreakType	*Type,			/* Out */
  UDIInt32	*CurrentCount		/* Out */
  ));

UDIError UDIClearBreakpoint UDIParams((
  UDIBreakId	BreakId			/* In */
  ));

UDIError UDIGetStdout UDIParams((
  UDIHostMemPtr	Buf,			/* Out */
  UDISizeT	BufSize,		/* In */
  UDISizeT	*CountDone		/* Out */
  ));

UDIError UDIGetStderr UDIParams((
  UDIHostMemPtr	Buf,			/* Out */
  UDISizeT	BufSize,		/* In */
  UDISizeT	*CountDone		/* Out */
  ));

UDIError UDIPutStdin UDIParams((
  UDIHostMemPtr	Buf,			/* In */
  UDISizeT	Count,			/* In */
  UDISizeT	*CountDone		/* Out */
  ));

UDIError UDIStdinMode UDIParams((
  UDIMode	*Mode			/* Out */
  ));

UDIError UDIPutTrans UDIParams((
  UDIHostMemPtr	Buf,			/* In */
  UDISizeT	Count,			/* In */
  UDISizeT	*CountDone		/* Out */
  ));

UDIError UDIGetTrans UDIParams((
  UDIHostMemPtr	Buf,			/* Out */
  UDISizeT	BufSize,		/* In */
  UDISizeT	*CountDone		/* Out */
  ));

UDIError UDITransMode UDIParams((
  UDIMode	*Mode			/* Out */
  ));

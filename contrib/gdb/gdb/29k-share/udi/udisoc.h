/* This module defines constants used in the UDI IPC modules.

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

static char udisoc_h[]="@(#)udisoc.h	2.6  Daniel Mann";
static char udisoc_h_AMD[]="@(#)udisoc.h	2.4, AMD";

#define LOCAL static
#define	company_c	1		/* AMD Company id */
#define	product_c 	1		/* socket IPC id */

/* Enumerate the UDI procedure services 
*/
#define	UDIConnect_c			0
#define	UDIDisconnect_c			1
#define	UDISetCurrentConnection_c	2
#define	UDICapabilities_c		3
#define	UDIEnumerateTIPs_c		4
#define	UDIGetErrorMsg_c		5
#define	UDIGetTargetConfig_c		6
#define	UDICreateProcess_c		7
#define	UDISetCurrentProcess_c		8
#define	UDIDestroyProcess_c		9
#define	UDIInitializeProcess_c		10
#define	UDIRead_c			11
#define	UDIWrite_c			12
#define	UDICopy_c			13
#define	UDIExecute_c			14
#define	UDIStep_c			15
#define	UDIStop_c			16
#define	UDIWait_c			17
#define	UDISetBreakpoint_c		18
#define	UDIQueryBreakpoint_c		19
#define	UDIClearBreakpoint_c		20
#define	UDIGetStdout_c			21
#define	UDIGetStderr_c			22
#define	UDIPutStdin_c			23
#define	UDIStdinMode_c			24
#define	UDIPutTrans_c			25
#define	UDIGetTrans_c			26
#define	UDITransMode_c			27
#define	UDITest_c			28
#define	UDIKill_c			29

#define	udr_UDIInt8(udrs, obj)  udr_work(udrs, obj, 1)
#define	udr_UDIInt16(udrs, obj) udr_work(udrs, obj, 2)
#define	udr_UDIInt32(udrs, obj) udr_work(udrs, obj, 4)
#define	udr_UDIInt(udrs, obj)   udr_work(udrs, obj, 4)

#define	udr_UDIUInt8(udrs, obj)  udr_work(udrs, obj, 1)
#define	udr_UDIUInt16(udrs, obj) udr_work(udrs, obj, 2)
#define	udr_UDIUInt32(udrs, obj) udr_work(udrs, obj, 4)
#define	udr_UDIUInt(udrs, obj)   udr_work(udrs, obj, 4)

#define	udr_UDIBool(udrs, obj)   udr_UDIInt32(udrs, obj)
#define	udr_UDICount(udrs, obj)  udr_UDIInt32(udrs, obj)
#define	udr_UDISize(udrs, obj)   udr_UDIUInt32(udrs, obj)
#define	udr_CPUSpace(udrs, obj)  udr_UDIInt32(udrs, obj)
#define	udr_CPUOffset(udrs, obj) udr_UDIUInt32(udrs, obj)
#define	udr_CPUSizeT(udrs, obj)  udr_UDIUInt32(udrs, obj)
#define	udr_UDIBreakId(udrs,obj) udr_UDIUInt(udrs, obj)
#define	udr_UDISizeT(udrs, obj)  udr_UDIUInt(udrs, obj)
#define	udr_UDIMode(udrs, obj)   udr_UDIUInt(udrs, obj)

#define	udr_UDIHostMemPtr(udrs, obj) udr_UDIUInt32(udrs, obj)
#define	udr_UDIVoidPtr(udrs, obj)   udr_UDIUInt32(udrs, obj)
#define	udr_UDIPId(udrs, obj)       udr_UDIUInt(udrs, obj)
#define	udr_UDISessionId(udrs, obj) udr_UDIInt32(udrs, obj)
#define	udr_UDIError(udrs, obj)     udr_UDIInt32(udrs, obj)
#define	udr_UDIStepType(udrs, obj)  udr_UDIInt32(udrs, obj)
#define	udr_UDIBreakType(udrs, obj) udr_UDIInt32(udrs, obj)

 
#define	UDR_ENCODE 1
#define	UDR_DECODE 2

typedef struct	UDR_str
{
    int		udr_op;			/* UDR operation */
    int		previous_op;
    int		sd;
    int		bufsize;
    char*	buff;
    char*	getbytes;
    char*	putbytes;
    char*	putend;
    int		domain;
    char*	soc_name;
} UDR;

/******************************************* Declare UDR suport functions */
int udr_create UDIParams((
  UDR*	udrs,
  int	sd,
  int	size
  ));

int udr_free UDIParams((
  UDR*	udrs,
  ));

int udr_signal UDIParams((
  UDR*	udrs,
  ));

int udr_sendnow UDIParams((
  UDR*	udrs
  ));

int udr_work UDIParams((
  UDR*	udrs,
  void*	object_p,
  int	size
  ));

int udr_UDIResource UDIParams((
  UDR*	udrs,
  UDIResource*	object_p
  ));

int udr_UDIRange UDIParams((
  UDR*	udrs,
  UDIRange*	object_p
  ));

int udr_UDIMemoryRange UDIParams((
  UDR*	udrs,
  UDIMemoryRange*	object_p
  ));

int udr_UDIMemoryRange UDIParams((
  UDR*	udrs,
  UDIMemoryRange* object_p
  ));

int udr_int UDIParams((
  UDR*	udrs,
  int*	int_p
  ));

int udr_bytes UDIParams((
  UDR*	udrs,
  char*	ptr,
  int	len
  ));

char* udr_inline UDIParams((
  UDR*	udrs,
  int	size
  ));

char*	udr_getpos UDIParams((
  UDR*	udrs
  ));
int	udr_setpos UDIParams((
  UDR*	udrs,
  char*	pos
  ));

int	udr_readnow UDIParams((
  UDR*	udrs,
  int	size
  ));

int udr_align UDIParams((
  UDR*	udrs,
  int	size,
  ));

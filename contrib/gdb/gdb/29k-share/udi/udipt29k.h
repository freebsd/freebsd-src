/* This file is to be used to reconfigure the UDI Procedural interface
   for a given target.

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

/* This file should be placed so that it will be
   included from udiproc.h. Everything in here will probably need to
   be changed when you change the target processor. Nothing in here
   should need to change when you change hosts or compilers.
*/

/* Select a target CPU Family */
#define TargetCPUFamily 	Am29K

/* Enumerate the processor specific values for Space in a resource */
#define UDI29KDRAMSpace		0
#define UDI29KIOSpace		1
#define UDI29KCPSpace0		2
#define UDI29KCPSpace1		3
#define UDI29KIROMSpace		4
#define UDI29KIRAMSpace		5
#define UDI29KLocalRegs		8
#define UDI29KGlobalRegs	9
#define UDI29KRealRegs		10
#define UDI29KSpecialRegs	11
#define UDI29KTLBRegs		12	/* Not Am29005 */
#define UDI29KACCRegs		13	/* Am29050 only */
#define UDI29KICacheSpace	14	/* Am2903x only */
#define UDI29KAm29027Regs	15	/* When available */
#define UDI29KPC		16
#define UDI29KDCacheSpace	17	/* When available */

/* Enumerate the Co-processor registers */
#define UDI29KCP_F		0
#define UDI29KCP_Flag		8
#define UDI29KCP_I		12
#define UDI29KCP_ITmp		16
#define UDI29KCP_R		20
#define UDI29KCP_S		28
#define UDI29KCP_RTmp		36
#define UDI29KCP_STmp		44
#define UDI29KCP_Stat		52
#define UDI29KCP_Prec		56
#define UDI29KCP_Reg0		60
#define UDI29KCP_Reg1		68
#define UDI29KCP_Reg2		76
#define UDI29KCP_Reg3		84
#define UDI29KCP_Reg4		92
#define UDI29KCP_Reg5		100
#define UDI29KCP_Reg6		108
#define UDI29KCP_Reg7		116
#define UDI29KCP_Mode		124

/* Enumerate the stacks in StackSizes array */
#define UDI29KMemoryStack	0
#define UDI29KRegisterStack	1

/* Enumerate the chips for ChipVersions array */
#define UDI29K29KVersion	0
#define UDI29K29027Version	1

/* Define special value for elements of ChipVersions array for
 * chips not present */
#define UDI29KChipNotPresent	-1

typedef	UDIInt32		UDICount;
typedef	UDIUInt32		UDISize;

typedef UDIInt			CPUSpace;
typedef UDIUInt32		CPUOffset;
typedef	UDIUInt32		CPUSizeT;

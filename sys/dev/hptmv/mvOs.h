/*-
 * Copyright (c) 2003-2004 HighPoint Technologies, Inc.
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
#ifndef __INCmvOsBsdh
#define __INCmvOsBsdh

/* Taken out of the Makefile magic */
#define __KERNEL__ 1
#define KERNEL 1
#define _KERNEL 1
#define _FREEBSD_ 1

/*
 * This binary object core for this driver is only for x86, so this constant
 * will not change.
 */
#define BITS_PER_LONG 32
#define DRIVER_VERSION "1.1"

#if DBG
#define MV_DEBUG_LOG
#endif

#define ENABLE_READ_AHEAD
#define ENABLE_WRITE_CACHE

/* Typedefs    */
#define HPTLIBAPI __attribute__((regparm(0)))
#define FAR
#ifdef FASTCALL
#undef FASTCALL
#endif
#define FASTCALL HPTLIBAPI
#define PASCAL HPTLIBAPI

typedef u_short USHORT;
typedef u_char  UCHAR;
typedef u_char *PUCHAR;
typedef u_short *PUSHORT;
typedef u_char  BOOLEAN;
typedef u_short WORD;
typedef u_int UINT, BOOL;
typedef u_char BYTE;
typedef void *PVOID, *LPVOID;
typedef void *ADDRESS;

typedef int  LONG;
typedef unsigned int ULONG, *PULONG, LBA_T;
typedef unsigned int DWORD, *LPDWORD, *PDWORD;
typedef unsigned long ULONG_PTR, UINT_PTR, BUS_ADDR;

typedef enum mvBoolean{MV_FALSE, MV_TRUE} MV_BOOLEAN;

#define FALSE 0
#define TRUE  1

/* System dependant typedefs */
typedef void		MV_VOID;
typedef uint32_t 	MV_U32;
typedef uint16_t	MV_U16;
typedef uint8_t		MV_U8;
typedef void		*MV_VOID_PTR;
typedef MV_U32		*MV_U32_PTR;
typedef MV_U16 		*MV_U16_PTR;
typedef MV_U8		*MV_U8_PTR;
typedef char		*MV_CHAR_PTR;
typedef void		*MV_BUS_ADDR_T;

/* System dependent macro for flushing CPU write cache */
#define MV_CPU_WRITE_BUFFER_FLUSH()

/* System dependent little endian from / to CPU conversions */
#define MV_CPU_TO_LE16(x)	(x)
#define MV_CPU_TO_LE32(x)	(x)

#define MV_LE16_TO_CPU(x)	(x)
#define MV_LE32_TO_CPU(x)	(x)

/* System dependent register read / write in byte/word/dword variants */
extern void HPTLIBAPI MV_REG_WRITE_BYTE(MV_BUS_ADDR_T base, MV_U32 offset,
    MV_U8 val);
extern void HPTLIBAPI MV_REG_WRITE_WORD(MV_BUS_ADDR_T base, MV_U32 offset,
    MV_U16 val);
extern void HPTLIBAPI MV_REG_WRITE_DWORD(MV_BUS_ADDR_T base, MV_U32 offset,
    MV_U32 val);
extern MV_U8  HPTLIBAPI MV_REG_READ_BYTE(MV_BUS_ADDR_T base, MV_U32 offset);
extern MV_U16 HPTLIBAPI MV_REG_READ_WORD(MV_BUS_ADDR_T base, MV_U32 offset);
extern MV_U32 HPTLIBAPI MV_REG_READ_DWORD(MV_BUS_ADDR_T base, MV_U32 offset);

/* System dependent structure */
typedef struct mvOsSemaphore
{
	int notused;
} MV_OS_SEMAPHORE;

/* Functions (User implemented)*/
ULONG_PTR HPTLIBAPI fOsPhysicalAddress(void *addr);

/* Semaphore init, take and release */
static __inline int
mvOsSemInit(MV_OS_SEMAPHORE *p)
{
	return (MV_TRUE);
}

static __inline int
mvOsSemTake(MV_OS_SEMAPHORE *p)
{
	return (MV_TRUE);
}

static __inline int
mvOsSemRelease(MV_OS_SEMAPHORE *p)
{
	return (MV_TRUE);
}

#define MV_MAX_SEGMENTS		255

/* Delay function in micro seconds resolution */
void HPTLIBAPI mvMicroSecondsDelay(MV_U32);

/* System logging function */
#ifdef MV_DEBUG_LOG
int mvLogMsg(MV_U8, MV_CHAR_PTR, ...);
#define _mvLogMsg(x) mvLogMsg x
#else
#define mvLogMsg(x...) 
#define _mvLogMsg(x)
#endif

/*************************************************************************
 * Debug support
 *************************************************************************/
#ifdef DEBUG
#define HPT_ASSERT(x)	\
	KASSERT((x), ("ASSERT fail at %s line %d", __FILE__, __LINE__))

extern int hpt_dbg_level;
#define KdPrintI(_x_) do{ if (hpt_dbg_level>2) printf _x_; }while(0)
#define KdPrintW(_x_) do{ if (hpt_dbg_level>1) printf _x_; }while(0)
#define KdPrintE(_x_) do{ if (hpt_dbg_level>0) printf _x_; }while(0)
#define KdPrint(x) KdPrintI(x)
#else
#define HPT_ASSERT(x)
#define KdPrint(x) 
#define KdPrintI(x) 
#define KdPrintW(x) 
#define KdPrintE(x) 
#endif

#endif /* __INCmvOsBsdh */

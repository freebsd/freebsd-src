/***************************************************************************\
|*                                                                           *|
|*       Copyright 2001-2004 NVIDIA Corporation.  All Rights Reserved.       *|
|*                                                                           *|
|*     THE INFORMATION CONTAINED HEREIN  IS PROPRIETARY AND CONFIDENTIAL     *|
|*     TO NVIDIA, CORPORATION.   USE,  REPRODUCTION OR DISCLOSURE TO ANY     *|
|*     THIRD PARTY IS SUBJECT TO WRITTEN PRE-APPROVAL BY NVIDIA, CORP.       *|
|*                                                                           *|
|*     THE INFORMATION CONTAINED HEREIN IS PROVIDED  "AS IS" WITHOUT         *|
|*     EXPRESS OR IMPLIED WARRANTY OF ANY KIND, INCLUDING ALL IMPLIED        *|
|*     WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A     *|
|*     PARTICULAR PURPOSE.                                                   *|
|*                                                                           *|
\***************************************************************************/ 


/*++

File:

	basetype.h


Abstract:

	This file contains the base type definitions used by the networking driver.


Revision History:

	SNo.	Date		Author				Description
	1.	2/7/2000	AJha				Created	

*/

#ifndef _BASETYPE_H_
#define _BASETYPE_H_

#ifndef IN
#define IN
#endif

#ifndef OUT
#define OUT
#endif

//
// Useful "types"

#ifndef NULL
#define NULL            0
#endif

#ifndef TRUE
#define TRUE            1
#endif

#ifndef FALSE
#define FALSE           0
#endif

#if 1
//
// Don't use as these are going to be deleted soon. Use NV_ instead
//
#define VOID                void
typedef VOID                *PVOID;

typedef unsigned char   UCHAR;
typedef UCHAR * PUCHAR;
typedef unsigned short  USHORT;
typedef USHORT * PUSHORT;
#ifdef linux
typedef unsigned int ULONG;
#else
typedef unsigned long ULONG;
#endif
typedef ULONG * PULONG;

typedef char CHAR;
typedef short SHORT;
typedef long LONG;

typedef unsigned int UINT;
typedef unsigned int *PUINT;


#endif


#define NV_VOID            	void
typedef NV_VOID            	*PNV_VOID;

typedef unsigned long		NV_BOOLEAN, *PNV_BOOLEAN;

typedef unsigned char		NV_UINT8, *PNV_UINT8;
typedef unsigned short		NV_UINT16, *PNV_UINT16;
#ifdef linux
typedef unsigned int		NV_UINT32, *PNV_UINT32;
#else
typedef unsigned long		NV_UINT32, *PNV_UINT32;
#endif

typedef signed char   		NV_SINT8,  *PNV_SINT8;
typedef signed short  		NV_SINT16, *PNV_SINT16;
typedef signed long   		NV_SINT32, *PNV_SINT32;


#if defined(linux)

    typedef unsigned long long           NV_UINT64, *PNV_UINT64;
    typedef signed long long             NV_SINT64, *PNV_SINT64;

#else
    #if _MSC_VER >= 1200         // MSVC 6.0 onwards
        typedef unsigned __int64 	NV_UINT64, *PNV_UINT64;
        typedef signed __int64 		NV_SINT64, *PNV_SINT64;
    #else
        typedef unsigned long     	NV_UINT64, *PNV_UINT64;
        typedef signed   long 		NV_SINT64, *PNV_SINT64;
    #endif

#endif

#ifndef _AMD64_
typedef unsigned int    NV_UINT;
typedef signed int      NV_INT;
#else

#if defined(linux)

typedef unsigned long long  NV_UINT;
typedef signed long long    NV_INT;

#else

typedef unsigned __int64    NV_UINT;
typedef signed __int64      NV_INT;

#endif
#endif


//
// Floating point definitions
//
typedef float                 NV_REAL32;   // 4-byte floating point
typedef double                NV_REAL64;   // 8-byte floating point



//
// Bit defintions
//
#define NV_BIT(bitpos)                  (1 << (bitpos))

// NV_BIT_SET 
// Sets the specified bit position (0..31). 
// Parameter bits can be 1 byte to 4 bytes, but the caller needs to make sure bitpos fits into it.
// x = 0xA0
// NV_BIT_SET(x, 1)
// Result: x = 0xA2
#define NV_BIT_SET(bits, bitpos)        ((bits) |= (NV_BIT(bitpos)))

// NV_BIT_CLEAR
// Clears the specified bit position (0..31)
// Parameter bits can be 1 byte to 4 bytes, but the caller needs to make sure bitpos fits into it.
// x = 0xAA
// NV_BIT_CLEAR(x, 1)
// Result: x = 0xA8
#define NV_BIT_CLEAR(bits, bitpos)      ((bits) &= (~NV_BIT(bitpos)))

// NV_BIT_GET 
// Gets the bit at the specified bit position (0..31)
// Parameter bits can be 1 byte to 4 bytes, but the caller needs to make sure bitpos fits into it.
// Result is either 1 or 0.
// x = 0xAA
// NV_BIT_GET(x, 1)
// Result: x = 1
#define NV_BIT_GET(bits, bitpos)        (((bits) >> (bitpos)) & 0x0001)


// NV_BIT_GETVALUE
// Gets the value from a 32 bit ULONG at specified bit position.
// Parameter bits needs to be 4 bytes long.
// Ex. ul32 = 0xFEDCBA98
// ulVal = NV_BIT_GETVALUE(ul32, 3, 0)  : Gets value from Bit position 3 to 0
// Result : ulVal = 8
#define NV_BIT_GETVALUE(ulOrigValue, bitposHi, bitposLow)  (((ulOrigValue) >> (bitposLow)) & (~(0xFFFFFFFF << ((bitposHi) - (bitposLow) +1))))

// NV_BIT_SETVALUE
// Set a value in a 32 bit ULONG at a specific bit position.
// Parameter bits needs to be 4 bytes long.
// Ex. ul32 = 0xFEDCBA98
// NV_BIT_SETVALUE(ul32, 0xF, 3, 0)  : Sets value at Bit position 3 to 0
// Result : ul32 becomes 0xFEDCBA9F
#define NV_BIT_SETVALUE(ulOrigValue, ulWindowValue, bitposHi, bitposLow)  \
    ((ulOrigValue) = ((((ulOrigValue) & (~ ((0xFFFFFFFF >> (31 - (bitposHi))) & (0xFFFFFFFF << (bitposLow))))) | ((ulWindowValue) << (bitposLow)))))


#define NV_BYTE(ulus, bytepos)  ((ulus >> (8 * (bytepos))) & 0xFF)


#define SWAP_U16(us) ((((us) & 0x00FF) << 8) | \
                      (((us) & 0xFF00) >> 8))

#define SWAP_U32(ul) ((((ul) & 0x000000FF) << 24) |   \
                        (((ul) & 0x0000FF00) <<  8) |	  \
                        (((ul) & 0x00FF0000) >>  8) |	  \
                        (((ul) & 0xFF000000) >> 24))

#define NV_FIELD_OFFSET(TYPE, FIELD)  ((NV_UINT32)((NV_UINT64)&((TYPE *)0)->FIELD))

#define ADDRESS_OFFSET(structure, member)       ((NV_UINT32) ((NV_UINT8 *) &(structure).member  \
                                                            - (NV_UINT8 *) &(structure)))


#define NV_MIN(a, b) ((a < b) ? a : b)
#define NV_MAX(a, b) ((a > b) ? a : b)

#ifdef AMD64
#define PNV_VOID_TO_NV_UINT64(x)    ((NV_UINT64)(x))
#define PNV_VOID_TO_NV_UINT32(x)    ((NV_UINT32)(NV_UINT64)(x))
#define NV_UINT64_TO_PNV_VOID(x)    ((PNV_VOID)(x))
#define NV_UINT32_TO_PNV_VOID(x)    ((PNV_VOID)(NV_UINT64)(x))
#else
#define PNV_VOID_TO_NV_UINT64(x)    ((NV_UINT64)(NV_UINT32)(x))
#define PNV_VOID_TO_NV_UINT32(x)    ((NV_UINT32)(x))
#define NV_UINT64_TO_PNV_VOID(x)    ((PNV_VOID)(NV_UINT32)(x))
#define NV_UINT32_TO_PNV_VOID(x)    ((PNV_VOID)(x))
#endif

#define NV_MAKE_TAG32(s)            (((NV_UINT32)((s)[3]) << 24) | ((NV_UINT32)((s)[2]) << 16) | \
                                     ((NV_UINT32)((s)[1]) <<  8) | ((NV_UINT32)((s)[0])))

#define NV_MAKE_TAG64(s)            (((NV_UINT64)((s)[7]) << 56) | ((NV_UINT64)((s)[6]) << 48) | \
                                     ((NV_UINT64)((s)[5]) << 40) | ((NV_UINT64)((s)[4]) << 32) | \
                                     ((NV_UINT64)((s)[3]) << 24) | ((NV_UINT64)((s)[2]) << 16) | \
                                     ((NV_UINT64)((s)[1]) <<  8) | ((NV_UINT64)((s)[0])))

typedef union _NVLARGE_INTEGER {

#if 0
    // NO UNNAMED UNIONS ALLOWED !@
    struct {
        NV_UINT32   LowPart;
        NV_SINT32   HighPart;
    };
#endif

    struct {
        NV_UINT32   LowPart;
        NV_SINT32   HighPart;
    } u;

    NV_SINT64       QuadPart;

} NVLARGE_INTEGER, *PNVLARGE_INTEGER;


#ifndef LINUX
typedef unsigned short NV_WCHAR;
#else
typedef unsigned long NV_WCHAR;
#endif

typedef NV_WCHAR *PNV_WSTR;

#if defined(linux)
#if !defined(NV_API_CALL)
#if defined (__i386__)
#define NV_API_CALL __attribute__ ((regparm(0)))
#else
#define NV_API_CALL
#endif
#endif
#else
#define NV_API_CALL
#endif

#endif // _BASETYPE_H_

/* $FreeBSD: src/sys/dev/asr/i2otypes.h,v 1.1.2.1 2000/09/21 20:33:50 msmith Exp $ */
/****************************************************************
 * Copyright (c) 1996-2000 Distributed Processing Technology Corporation
 * Copyright (c) 2000 Adaptec Corporation.
 * All rights reserved.
 * 
 * Copyright 1999 I2O Special Interest Group (I2O SIG).  All rights reserved.
 * All rights reserved
 * 
 * TERMS AND CONDITIONS OF USE
 * 
 * Redistribution and use in source form, with or without modification, are
 * permitted provided that redistributions of source code must retain the
 * above copyright notice, this list of conditions and the following disclaimer.
 * 
 * This software is provided `as is' by Distributed Processing Technology and
 * any express or implied warranties, including, but not limited to, the
 * implied warranties of merchantability and fitness for a particular purpose,
 * are disclaimed. In no event shall Distributed Processing Technology be
 * liable for any direct, indirect, incidental, special, exemplary or
 * consequential damages (including, but not limited to, procurement of
 * substitute goods or services; loss of use, data, or profits; or business
 * interruptions) however caused and on any theory of liability, whether in
 * contract, strict liability, or tort (including negligence or otherwise)
 * arising in any way out of the use of this driver software, even if advised
 * of the possibility of such damage.
 * 
 * This header file, and any modifications of this header file, are provided
 * contingent upon your agreement and adherence to the here-listed terms and
 * conditions.  By accepting and/or using this header file, you agree to abide
 * by these terms and conditions and that these terms and conditions will be
 * construed and governed in accordance with the laws of the State of California,
 * without reference to conflict-of-law provisions.  If you do not agree
 * to these terms and conditions, please delete this file, and any copies,
 * permanently, without making any use thereof.
 * 
 * THIS HEADER FILE IS PROVIDED FREE OF CHARGE ON AN AS-IS BASIS WITHOUT
 * WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
 * TO IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE.  I2O SIG DOES NOT WARRANT THAT THIS HEADER FILE WILL MEET THE
 * USER'S REQUIREMENTS OR THAT ITS OPERATION WILL BE UNINTERRUPTED OR
 * ERROR-FREE.
 * 
 * I2O SIG DISCLAIMS ALL LIABILITY, INCLUDING LIABILITY FOR INFRINGEMENT OF
 * ANY PROPRIETARY RIGHTS, RELATING TO THE IMPLEMENTATION OF THE I2O
 * SPECIFICATIONS.  I2O SIG DOES NOT WARRANT OR REPRESENT THAT SUCH
 * IMPLEMENTATIONS WILL NOT INFRINGE SUCH RIGHTS.
 * 
 * THE USER OF THIS HEADER FILE SHALL HAVE NO RECOURSE TO I2O SIG FOR ANY
 * ACTUAL OR CONSEQUENTIAL DAMAGES INCLUDING, BUT NOT LIMITED TO, LOST DATA
 * OR LOST PROFITS ARISING OUT OF THE USE OR INABILITY TO USE THIS PROGRAM.
 * 
 * I2O SIG grants the user of this header file a license to copy, distribute,
 * and modify it, for any purpose, under the following terms.  Any copying,
 * distribution, or modification of this header file must not delete or alter
 * the copyright notice of I2O SIG or any of these Terms and Conditions.
 * 
 * Any distribution of this header file must not include a charge for the
 * header file (unless such charges are strictly for the physical acts of
 * copying or transferring copies).  However, distribution of a product in
 * which this header file is embedded may include a charge so long as any
 * such charge does not include any charge for the header file itself.
 * 
 * Any modification of this header file constitutes a derivative work based
 * on this header file.  Any distribution of such derivative work: (1) must
 * include prominent notices that the header file has been changed from the
 * original, together with the dates of any changes; (2) automatically includes 
 * this same license to the original header file from I2O SIG, without any
 * restriction thereon from the distributing user; and (3) must include a
 * grant of license of the modified file under the same terms and conditions
 * as these Terms and Conditions.
 * 
 * The I2O SIG Web site can be found at: http://www.i2osig.org
 * 
 * The I2O SIG encourages you to deposit derivative works based on this
 * header file at the I2O SIG Web site.  Furthermore, to become a Registered
 * Developer of the I2O SIG, sign up at the Web site or call 415.750.8352
 * (United States).
 ****************************************************************/

#ifndef __INCi2otypesh
#define __INCi2otypesh

#define I2OTYPES_REV 1_5_4

/* include architecture/compiler dependencies */

#if ((defined(KERNEL) || defined(_KERNEL)) && defined(__FreeBSD__))
# if (KERN_VERSION < 3)
#  include "i386/pci/i2odep.h"
# else
#  include "dev/asr/i2odep.h"
# endif
#else
# include "i2odep.h"
#endif


/* 64 bit defines */

typedef struct _S64 {
   U32                         LowPart;
   S32                         HighPart;
} S64;

typedef struct _U64 {
   U32                         LowPart;
   U32                         HighPart;
} U64;

/* Pointer to Basics */

typedef    VOID                *PVOID;
typedef    S8                  *PS8;
typedef    S16                 *PS16;
typedef    S32                 *PS32;
typedef    S64                 *PS64;

/* Pointer to Unsigned Basics */

typedef    U8                  *PU8;
typedef    U16                 *PU16;
typedef    U32                 *PU32;
typedef    U64                 *PU64;

/* misc */

typedef S32             I2O_ARG;
typedef U32             I2O_COUNT;
typedef U32             I2O_USECS;
typedef U32             I2O_ADDR32;
typedef U32             I2O_SIZE;

#endif /* __INCi2otypesh */

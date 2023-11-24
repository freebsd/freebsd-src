/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2016-2023, Broadcom Inc. All rights reserved.
 * Support: <fbsd-storage-driver.pdl@broadcom.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 * 3. Neither the name of the Broadcom Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing
 * official policies,either expressed or implied, of the FreeBSD Project.
 *
 * Mail to: Broadcom Inc 1320 Ridder Park Dr, San Jose, CA 95131
 *
 * Broadcom Inc. (Broadcom) MPI3MR Adapter FreeBSD
 *
 */

#ifndef MPI30_TYPE_H
#define MPI30_TYPE_H     1

/*****************************************************************************
 * Define MPI3_POINTER if it has not already been defined. By default        *
 * MPI3_POINTER is defined to be a near pointer. MPI3_POINTER can be defined *
 * as a far pointer by defining MPI3_POINTER as "far *" before this header   *
 * file is included.                                                         *
 ****************************************************************************/
#ifndef MPI3_POINTER
#define MPI3_POINTER    *
#endif  /* MPI3_POINTER */

/* The basic types may have already been included by mpi_type.h or mpi2_type.h */
#if !defined(MPI_TYPE_H) && !defined(MPI2_TYPE_H)
/*****************************************************************************
 *              Basic Types                                                  *
 ****************************************************************************/
typedef int8_t      S8;
typedef uint8_t     U8;
typedef int16_t     S16;
typedef uint16_t    U16;
typedef int32_t     S32;
typedef uint32_t    U32;
typedef int64_t     S64;
typedef uint64_t    U64;

/*****************************************************************************
 *              Structure Types                                              *
 ****************************************************************************/
typedef struct _S64struct
{
    U32         Low;
    S32         High;
} S64struct;

typedef struct _U64struct
{
    U32         Low;
    U32         High;
} U64struct;

/*****************************************************************************
 *              Pointer Types                                                *
 ****************************************************************************/
typedef S8          *PS8;
typedef U8          *PU8;
typedef S16         *PS16;
typedef U16         *PU16;
typedef S32         *PS32;
typedef U32         *PU32;
typedef S64         *PS64;
typedef U64         *PU64;
typedef S64struct   *PS64struct;
typedef U64struct   *PU64struct;
#endif  /* MPI_TYPE_H && MPI2_TYPE_H */

#endif  /* MPI30_TYPE_H */

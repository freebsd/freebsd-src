/*-
 * Copyright (c) 2000 Mitsuru IWASAKI <iwasaki@FreeBSD.org>
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
 *	$Id: aml_region.h,v 1.5 2000/08/08 14:12:05 iwasaki Exp $
 *	$FreeBSD$
 */

#ifndef _AML_REGION_H_
#define _AML_REGION_H_

/*
 * Note that common part of region I/O is implemented in aml_common.c.
 */

/*
 * Debug macros for region I/O
 */

#define AML_REGION_READ_DEBUG(regtype, flags, addr, bitoffset, bitlen)	\
  AML_DEBUGPRINT("\n[aml_region_read(%d, %d, 0x%x, 0x%x, 0x%x)]\n",\
    regtype, flags, addr, bitoffset, bitlen)

#define AML_REGION_READ_INTO_BUFFER_DEBUG(regtype, flags,		\
					  addr, bitoffset, bitlen)	\
  AML_DEBUGPRINT("\n[aml_region_read_into_buffer(%d, %d, 0x%x, 0x%x, 0x%x)]\n",\
    regtype, flags, addr, bitoffset, bitlen)

#define AML_REGION_WRITE_DEBUG(regtype, flags, value,			\
			       addr, bitoffset, bitlen)			\
  AML_DEBUGPRINT("\n[aml_region_write(%d, %d, 0x%x, 0x%x, 0x%x, 0x%x)]\n",\
    regtype, flags, value, addr, bitoffset, bitlen)

#define AML_REGION_WRITE_FROM_BUFFER_DEBUG(regtype, flags,		\
					   addr, bitoffset, bitlen)	\
  AML_DEBUGPRINT("\n[aml_region_write_from_buffer(%d, %d, 0x%x, 0x%x, 0x%x)]\n",\
    regtype, flags, addr, bitoffset, bitlen)

#define AML_REGION_BCOPY_DEBUG(regtype, flags, addr, bitoffset, bitlen,	\
			       dflags, daddr, dbitoffset, dbitlen)	\
  AML_DEBUGPRINT("\n[aml_region_bcopy(%d, %d, 0x%x, 0x%x, 0x%x, %d, 0x%x, 0x%x, 0x%x)]\n",\
    regtype, flags, addr, bitoffset, bitlen,				\
    dflags, daddr, dbitoffset, dbitlen)

/*
 * Region I/O subroutine
 */

struct aml_environ;

u_int32_t	 aml_region_read(struct aml_environ *, int, u_int32_t,
				 u_int32_t, u_int32_t, u_int32_t);
int		 aml_region_write(struct aml_environ *, int, u_int32_t,
				  u_int32_t, u_int32_t, u_int32_t, u_int32_t);
int		 aml_region_read_into_buffer(struct aml_environ *, int,
					     u_int32_t, u_int32_t, u_int32_t,
					     u_int32_t, u_int8_t *);
int		 aml_region_write_from_buffer(struct aml_environ *, int,
					      u_int32_t, u_int8_t *, u_int32_t,
					      u_int32_t, u_int32_t);
int		 aml_region_bcopy(struct aml_environ *, int,
				  u_int32_t, u_int32_t, u_int32_t, u_int32_t,
				  u_int32_t, u_int32_t, u_int32_t, u_int32_t);

#ifndef _KERNEL
void	aml_simulation_regdump(const char *);
extern int	aml_debug_prompt_regoutput;
extern int	aml_debug_prompt_reginput;
#endif /* !_KERNEL */

#endif /* !_AML_REGION_H_ */

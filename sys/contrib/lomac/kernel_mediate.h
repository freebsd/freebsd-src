/*-
 * Copyright (c) 2001 Networks Associates Technology, Inc.
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

#ifndef _KERNEL_MEDIATE_H_
#define _KERNEL_MEDIATE_H_

#include "kernel_interface.h"

int mediate_subject_level_subject( const char *op_s,
			     const lomac_subject_t *p_subject_one, 
			     level_t level_one,
			     lomac_subject_t *p_subject_two );
int mediate_subject_object( const char *op_s, lomac_subject_t *p_subject,
			    const lomac_object_t *p_object );
int mediate_subject_object_open( lomac_subject_t *p_subject,
			    const lomac_object_t *p_object );
#if 0
int mediate_subject_path( const char *op_s, const lomac_subject_t *p_subject,
			  const char *path_s );
int mediate_path_path( const char *op_s, const lomac_subject_t *p_subject,
		       const char *canabsname_one_s, 
		       const char *canabsname_two_s );
#endif
int mediate_subject_at_level( const char *op_s, 
			      lomac_subject_t *p_subject,
			      const level_t target_level );
#if 0
int mediate_object_at_level( const char *op_s,
			     const lomac_subject_t *p_subject,
			     const lomac_object_t *p_object,
			     const level_t target_level );
#endif

#endif

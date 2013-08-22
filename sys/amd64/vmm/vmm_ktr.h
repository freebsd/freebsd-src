/*-
 * Copyright (c) 2011 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
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

#ifndef _VMM_KTR_H_
#define	_VMM_KTR_H_

#include <sys/ktr.h>
#include <sys/pcpu.h>

#define	KTR_VMM	KTR_GEN

#define	VMM_CTR0(vm, vcpuid, format)					\
CTR3(KTR_VMM, "vm %s-%d(%d): " format, vm_name((vm)), (vcpuid), curcpu)

#define	VMM_CTR1(vm, vcpuid, format, p1)				\
CTR4(KTR_VMM, "vm %s-%d(%d): " format, vm_name((vm)), (vcpuid), curcpu, \
			(p1))

#define	VMM_CTR2(vm, vcpuid, format, p1, p2)				\
CTR5(KTR_VMM, "vm %s-%d(%d): " format, vm_name((vm)), (vcpuid), curcpu, \
			(p1), (p2))

#define	VMM_CTR3(vm, vcpuid, format, p1, p2, p3)			\
CTR6(KTR_VMM, "vm %s-%d(%d): " format, vm_name((vm)), (vcpuid), curcpu, \
			(p1), (p2), (p3))
#endif

/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_LINUX_IDR_H_
#define	_LINUX_IDR_H_

#include <sys/kernel.h>

#define	IDR_BITS	5
#define	IDR_SIZE	(1 << IDR_BITS)
#define	IDR_MASK	(IDR_SIZE - 1)

#define	MAX_ID_SHIFT	((sizeof(int) * NBBY) - 1)
#define	MAX_ID_BIT	(1U << MAX_ID_SHIFT)
#define	MAX_ID_MASK	(MAX_ID_BIT - 1)
#define	MAX_LEVEL	(MAX_ID_SHIFT + IDR_BITS - 1) / IDR_BITS

struct idr_layer {
	unsigned long		bitmap;
	struct idr_layer	*ary[IDR_SIZE];
};

struct idr {
	struct mtx		lock;
	struct idr_layer	*top;
	struct idr_layer	*free;
	int			layers;
};

#define DEFINE_IDR(name)						\
	struct idr name;						\
	SYSINIT(name##_idr_sysinit, SI_SUB_DRIVERS, SI_ORDER_FIRST,	\
	    idr_init, &(name));

void	*idr_find(struct idr *idp, int id);
int	idr_pre_get(struct idr *idp, gfp_t gfp_mask);
int	idr_get_new(struct idr *idp, void *ptr, int *id);
int	idr_get_new_above(struct idr *idp, void *ptr, int starting_id, int *id);
void	*idr_replace(struct idr *idp, void *ptr, int id);
void	idr_remove(struct idr *idp, int id);
void	idr_remove_all(struct idr *idp);
void	idr_destroy(struct idr *idp);
void	idr_init(struct idr *idp);

#endif	/* _LINUX_IDR_H_ */

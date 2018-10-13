/*-
 * Copyright (c) 2018, Mellanox Technologies, Ltd.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS `AS IS' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
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

#ifndef _DEV_MLX5_MLX5IO_H_
#define _DEV_MLX5_MLX5IO_H_

#include <sys/ioccom.h>

struct mlx5_fwdump_reg {
	uint32_t addr;
	uint32_t val;
};

struct mlx5_fwdump_addr {
	uint32_t domain;
	uint8_t bus;
	uint8_t slot;
	uint8_t func;
};

struct mlx5_fwdump_get {
	struct mlx5_fwdump_addr devaddr;
	struct mlx5_fwdump_reg *buf;
	size_t reg_cnt;
	size_t reg_filled; /* out */
};

#define	MLX5_FWDUMP_GET		_IOWR('m', 1, struct mlx5_fwdump_get)
#define	MLX5_FWDUMP_RESET	_IOW('m', 2, struct mlx5_fwdump_addr)
#define	MLX5_FWDUMP_FORCE	_IOW('m', 3, struct mlx5_fwdump_addr)

#ifndef _KERNEL
#define	MLX5_DEV_PATH	_PATH_DEV"mlx5ctl"
#endif

#endif

/*-
 * Copyright (c) 2013-2015, Mellanox Technologies, Ltd.  All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <dev/mlx5/driver.h>
#include "mlx5_core.h"

int mlx5_core_mad_ifc(struct mlx5_core_dev *dev, void *inb, void *outb,
		      u16 opmod, u8 port)
{
	struct mlx5_mad_ifc_mbox_in *in = NULL;
	struct mlx5_mad_ifc_mbox_out *out = NULL;
	int err;

	in = kzalloc(sizeof(*in), GFP_KERNEL);

	out = kzalloc(sizeof(*out), GFP_KERNEL);

	in->hdr.opcode = cpu_to_be16(MLX5_CMD_OP_MAD_IFC);
	in->hdr.opmod = cpu_to_be16(opmod);
	in->port = port;

	memcpy(in->data, inb, sizeof(in->data));

	err = mlx5_cmd_exec(dev, in, sizeof(*in), out, sizeof(*out));
	if (err)
		goto out;

	if (out->hdr.status) {
		err = mlx5_cmd_status_to_err(&out->hdr);
		goto out;
	}

	memcpy(outb, out->data, sizeof(out->data));

out:
	kfree(out);
	kfree(in);
	return err;
}
EXPORT_SYMBOL_GPL(mlx5_core_mad_ifc);

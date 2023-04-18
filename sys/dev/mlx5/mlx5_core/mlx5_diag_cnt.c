/*-
 * Copyright (c) 2018, Mellanox Technologies, Ltd.
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
 */

#include <dev/mlx5/mlx5_core/diag_cnt.h>

static int get_supported_cnt_ids(struct mlx5_core_dev *dev);
static int enable_cnt_id(struct mlx5_core_dev *dev, u16 id);
static void reset_cnt_id(struct mlx5_core_dev *dev);
static void reset_params(struct mlx5_diag_cnt *diag_cnt);

static int
mlx5_sysctl_counter_id(SYSCTL_HANDLER_ARGS)
{
	struct mlx5_diag_cnt *diag_cnt;
	struct mlx5_core_dev *dev;
	uint16_t *ptr;
	size_t max;
	size_t num;
	size_t x;
	int err;

	dev = arg1;
	diag_cnt = &dev->diag_cnt;

	max = MLX5_CAP_GEN(dev, num_of_diagnostic_counters);

	ptr = kmalloc(sizeof(ptr[0]) * max, GFP_KERNEL);

	DIAG_LOCK(diag_cnt);

	for (x = num = 0; x != max; x++) {
		if (diag_cnt->cnt_id[x].enabled)
			ptr[num++] = diag_cnt->cnt_id[x].id;
	}

	err = SYSCTL_OUT(req, ptr, sizeof(ptr[0]) * num);
	if (err || !req->newptr)
		goto done;

	num = req->newlen / sizeof(ptr[0]);
	if (num > max) {
		err = ENOMEM;
		goto done;
	}

	err = SYSCTL_IN(req, ptr, sizeof(ptr[0]) * num);

	reset_cnt_id(dev);

	for (x = 0; x != num; x++) {
		err = enable_cnt_id(dev, ptr[x]);
		if (err)
			goto done;
	}

	diag_cnt->num_cnt_id = num;
done:
	kfree(ptr);

	if (err != 0 && req->newptr != NULL)
		reset_cnt_id(dev);

	DIAG_UNLOCK(diag_cnt);

	return (err);
}

#define	NUM_OF_DIAG_PARAMS 5

static int
mlx5_sysctl_params(SYSCTL_HANDLER_ARGS)
{
	struct mlx5_diag_cnt *diag_cnt;
	struct mlx5_core_dev *dev;
	uint32_t temp[NUM_OF_DIAG_PARAMS];
	int err;

	dev = arg1;
	diag_cnt = &dev->diag_cnt;

	DIAG_LOCK(diag_cnt);

	temp[0] = diag_cnt->log_num_of_samples;
	temp[1] = diag_cnt->log_sample_period;
	temp[2] = diag_cnt->flag;
	temp[3] = diag_cnt->num_of_samples;
	temp[4] = diag_cnt->sample_index;

	err = SYSCTL_OUT(req, temp, sizeof(temp));
	if (err || !req->newptr)
		goto done;

	err = SYSCTL_IN(req, temp, sizeof(temp));
	if (err)
		goto done;

	reset_params(&dev->diag_cnt);

	if (temp[0] > MLX5_CAP_DEBUG(dev, log_max_samples) ||
	    (1U << (MLX5_CAP_DEBUG(dev, log_max_samples) - temp[0])) <
	    diag_cnt->num_cnt_id) {
		err = ERANGE;
		goto done;
	} else if (temp[1] < MLX5_CAP_DEBUG(dev, log_min_sample_period)) {
		err = ERANGE;
		goto done;
	} else if (temp[2] >= 0x100) {
		err = ERANGE;
		goto done;
	} else if (temp[3] > (1U << diag_cnt->log_num_of_samples)) {
		err = ERANGE;
		goto done;
	} else if (temp[4] > (1U << diag_cnt->log_num_of_samples)) {
		err = ERANGE;
		goto done;
	}

	diag_cnt->log_num_of_samples = temp[0];
	diag_cnt->log_sample_period = temp[1];
	diag_cnt->flag = temp[2];
	diag_cnt->num_of_samples = temp[3];
	diag_cnt->sample_index = temp[4];
done:
	DIAG_UNLOCK(diag_cnt);

	return (err);
}

static void
decode_cnt_buffer(u32 num_of_samples, u8 *out, struct sbuf *sbuf)
{
	void *cnt;
	u64 temp;
	u32 i;

	for (i = 0; i != num_of_samples; i++) {
		cnt = MLX5_ADDR_OF(query_diagnostic_counters_out,
		    out, diag_counter[i]);
		temp = MLX5_GET(diagnostic_cntr_struct, cnt, counter_value_h);
		temp = (temp << 32) |
		    MLX5_GET(diagnostic_cntr_struct, cnt, counter_value_l);
		sbuf_printf(sbuf,
		    "0x%04x,0x%04x,0x%08x,0x%016llx\n",
		    MLX5_GET(diagnostic_cntr_struct, cnt, counter_id),
		    MLX5_GET(diagnostic_cntr_struct, cnt, sample_id),
		    MLX5_GET(diagnostic_cntr_struct, cnt, time_stamp_31_0),
		    (unsigned long long)temp);
	}
}

static int
mlx5_sysctl_dump_set(SYSCTL_HANDLER_ARGS)
{
	struct mlx5_diag_cnt *diag_cnt;
	struct mlx5_core_dev *dev;
	uint8_t temp;
	int err;

	dev = arg1;
	diag_cnt = &dev->diag_cnt;

	DIAG_LOCK(diag_cnt);

	err = SYSCTL_OUT(req, &diag_cnt->ready, sizeof(diag_cnt->ready));
	if (err || !req->newptr)
		goto done;

	err = SYSCTL_IN(req, &temp, sizeof(temp));
	if (err)
		goto done;

	diag_cnt->ready = (temp != 0);
	if (diag_cnt->ready != 0)
		err = -mlx5_diag_set_params(dev);
done:
	DIAG_UNLOCK(diag_cnt);

	return (err);
}

static int
mlx5_sysctl_dump_get(SYSCTL_HANDLER_ARGS)
{
	struct mlx5_diag_cnt *diag_cnt;
	struct mlx5_core_dev *dev;
	struct sbuf sbuf;
	u8 *out;
	int err;

	dev = arg1;
	diag_cnt = &dev->diag_cnt;

	err = sysctl_wire_old_buffer(req, 0);
	if (err != 0)
		return (err);

	DIAG_LOCK(diag_cnt);

	sbuf_new_for_sysctl(&sbuf, NULL, 65536, req);

	if (diag_cnt->ready != 0) {
		err = -mlx5_diag_query_counters(dev, &out);
		if (err) {
			sbuf_printf(&sbuf, "\nCould not query counters: %d\n", err);
		} else {
			sbuf_printf(&sbuf, "\n");
			decode_cnt_buffer(diag_cnt->num_of_samples *
			    diag_cnt->num_cnt_id, out, &sbuf);
			kfree(out);
		}
	} else {
		sbuf_printf(&sbuf, "\nDump was not set.\n");
	}

	err = sbuf_finish(&sbuf);

	sbuf_delete(&sbuf);

	DIAG_UNLOCK(diag_cnt);

	return (err);
}

static int
mlx5_sysctl_cap_read(SYSCTL_HANDLER_ARGS)
{
	struct mlx5_diag_cnt *diag_cnt;
	struct mlx5_core_dev *dev;
	struct sbuf sbuf;
	int err;
	u32 i;

	dev = arg1;
	diag_cnt = &dev->diag_cnt;

	err = sysctl_wire_old_buffer(req, 0);
	if (err != 0)
		return (err);

	DIAG_LOCK(diag_cnt);

	sbuf_new_for_sysctl(&sbuf, NULL, 8192, req);

	sbuf_printf(&sbuf, "\n");

	/* print cap */
	sbuf_printf(&sbuf, "log_max_samples=%d\n",
	    MLX5_CAP_DEBUG(dev, log_max_samples));
	sbuf_printf(&sbuf, "log_min_sample_period=%d\n",
	    MLX5_CAP_DEBUG(dev, log_min_sample_period));
	sbuf_printf(&sbuf, "repetitive=%d\n",
	    MLX5_CAP_DEBUG(dev, repetitive));
	sbuf_printf(&sbuf, "single=%d\n",
	    MLX5_CAP_DEBUG(dev, single));
	sbuf_printf(&sbuf, "num_of_diagnostic_counters=%d\n",
	    MLX5_CAP_GEN(dev, num_of_diagnostic_counters));

	/* print list of supported counter */
	sbuf_printf(&sbuf, "supported counter id:\n");
	for (i = 0; i != MLX5_CAP_GEN(dev, num_of_diagnostic_counters); i++)
		sbuf_printf(&sbuf, "0x%04x,", diag_cnt->cnt_id[i].id);
	sbuf_printf(&sbuf, "\n");

	err = sbuf_finish(&sbuf);
	sbuf_delete(&sbuf);

	DIAG_UNLOCK(diag_cnt);

	return (err);
}

static int
get_supported_cnt_ids(struct mlx5_core_dev *dev)
{
	u32 num_counters = MLX5_CAP_GEN(dev, num_of_diagnostic_counters);
	struct mlx5_diag_cnt *diag_cnt = &dev->diag_cnt;
	u32 i;

	diag_cnt->cnt_id = kzalloc(sizeof(*diag_cnt->cnt_id) * num_counters,
	    GFP_KERNEL);
	if (!diag_cnt->cnt_id)
		return (-ENOMEM);

	for (i = 0; i != num_counters; i++) {
		diag_cnt->cnt_id[i].id =
		    MLX5_CAP_DEBUG(dev, diagnostic_counter[i].counter_id);
	}
	return (0);
}

static void
reset_cnt_id(struct mlx5_core_dev *dev)
{
	struct mlx5_diag_cnt *diag_cnt = &dev->diag_cnt;
	u32 i;

	diag_cnt->num_cnt_id = 0;
	for (i = 0; i != MLX5_CAP_GEN(dev, num_of_diagnostic_counters); i++)
		diag_cnt->cnt_id[i].enabled = false;
}

static int
enable_cnt_id(struct mlx5_core_dev *dev, u16 id)
{
	struct mlx5_diag_cnt *diag_cnt = &dev->diag_cnt;
	u32 i;

	for (i = 0; i != MLX5_CAP_GEN(dev, num_of_diagnostic_counters); i++) {
		if (diag_cnt->cnt_id[i].id == id) {
			if (diag_cnt->cnt_id[i].enabled)
				return (EINVAL);

			diag_cnt->cnt_id[i].enabled = true;
			break;
		}
	}

	if (i == MLX5_CAP_GEN(dev, num_of_diagnostic_counters))
		return (ENOENT);
	else
		return (0);
}

static void
reset_params(struct mlx5_diag_cnt *diag_cnt)
{
	diag_cnt->log_num_of_samples = 0;
	diag_cnt->log_sample_period = 0;
	diag_cnt->flag = 0;
	diag_cnt->num_of_samples = 0;
	diag_cnt->sample_index = 0;
}

int
mlx5_diag_set_params(struct mlx5_core_dev *dev)
{
	u8 out[MLX5_ST_SZ_BYTES(set_diagnostic_params_out)] = {0};
	struct mlx5_diag_cnt *diag_cnt = &dev->diag_cnt;
	void *cnt_id;
	void *ctx;
	u16 in_sz;
	int err;
	u8 *in;
	u32 i;
	u32 j;

	if (!diag_cnt->num_cnt_id)
		return (-EINVAL);

	in_sz = MLX5_ST_SZ_BYTES(set_diagnostic_params_in) +
	    diag_cnt->num_cnt_id * MLX5_ST_SZ_BYTES(counter_id);
	in = kzalloc(in_sz, GFP_KERNEL);
	if (!in)
		return (-ENOMEM);

	MLX5_SET(set_diagnostic_params_in, in, opcode,
	    MLX5_CMD_OP_SET_DIAGNOSTICS);

	ctx = MLX5_ADDR_OF(set_diagnostic_params_in, in,
	    diagnostic_params_ctx);
	MLX5_SET(diagnostic_params_context, ctx, num_of_counters,
	    diag_cnt->num_cnt_id);
	MLX5_SET(diagnostic_params_context, ctx, log_num_of_samples,
	    diag_cnt->log_num_of_samples);

	MLX5_SET(diagnostic_params_context, ctx, single,
	    (diag_cnt->flag >> 7) & 1);
	MLX5_SET(diagnostic_params_context, ctx, repetitive,
	    (diag_cnt->flag >> 6) & 1);
	MLX5_SET(diagnostic_params_context, ctx, sync,
	    (diag_cnt->flag >> 5) & 1);
	MLX5_SET(diagnostic_params_context, ctx, clear,
	    (diag_cnt->flag >> 4) & 1);
	MLX5_SET(diagnostic_params_context, ctx, on_demand,
	    (diag_cnt->flag >> 3) & 1);
	MLX5_SET(diagnostic_params_context, ctx, enable,
	    (diag_cnt->flag >> 2) & 1);
	MLX5_SET(diagnostic_params_context, ctx, log_sample_period,
	    diag_cnt->log_sample_period);

	for (i = j = 0; i != MLX5_CAP_GEN(dev, num_of_diagnostic_counters); i++) {
		if (diag_cnt->cnt_id[i].enabled) {
			cnt_id = MLX5_ADDR_OF(diagnostic_params_context,
			    ctx, counter_id[j]);
			MLX5_SET(counter_id, cnt_id, counter_id,
			    diag_cnt->cnt_id[i].id);
			j++;
		}
	}

	err = mlx5_cmd_exec(dev, in, in_sz, out, sizeof(out));

	kfree(in);
	return (err);
}

/* This function is for debug purpose */
int
mlx5_diag_query_params(struct mlx5_core_dev *dev)
{
	u8 in[MLX5_ST_SZ_BYTES(query_diagnostic_params_in)] = {0};
	struct mlx5_diag_cnt *diag_cnt = &dev->diag_cnt;
	void *cnt_id;
	u16 out_sz;
	void *ctx;
	int err;
	u8 *out;
	u32 i;

	out_sz = MLX5_ST_SZ_BYTES(query_diagnostic_params_out) +
	    diag_cnt->num_cnt_id * MLX5_ST_SZ_BYTES(counter_id);

	out = kzalloc(out_sz, GFP_KERNEL);
	if (!out)
		return (-ENOMEM);

	MLX5_SET(query_diagnostic_params_in, in, opcode,
	    MLX5_CMD_OP_QUERY_DIAGNOSTIC_PARAMS);
	err = mlx5_cmd_exec(dev, in, sizeof(in), out, out_sz);
	if (err)
		goto out;

	ctx = MLX5_ADDR_OF(query_diagnostic_params_out, out,
	    diagnostic_params_ctx);
	mlx5_core_dbg(dev, "single=%x\n",
	    MLX5_GET(diagnostic_params_context, ctx, single));
	mlx5_core_dbg(dev, "repetitive=%x\n",
	    MLX5_GET(diagnostic_params_context, ctx, repetitive));
	mlx5_core_dbg(dev, "sync=%x\n",
	    MLX5_GET(diagnostic_params_context, ctx, sync));
	mlx5_core_dbg(dev, "clear=%x\n",
	    MLX5_GET(diagnostic_params_context, ctx, clear));
	mlx5_core_dbg(dev, "on_demand=%x\n",
	    MLX5_GET(diagnostic_params_context, ctx, on_demand));
	mlx5_core_dbg(dev, "enable=%x\n",
	    MLX5_GET(diagnostic_params_context, ctx, enable));
	mlx5_core_dbg(dev, "log_sample_period=%x\n",
	    MLX5_GET(diagnostic_params_context, ctx,
	    log_sample_period));

	for (i = 0; i != diag_cnt->num_cnt_id; i++) {
		cnt_id = MLX5_ADDR_OF(diagnostic_params_context,
		    ctx, counter_id[i]);
		mlx5_core_dbg(dev, "counter_id[%d]=%x\n", i,
		    MLX5_GET(counter_id, cnt_id, counter_id));
	}
out:
	kfree(out);
	return (err);
}

int
mlx5_diag_query_counters(struct mlx5_core_dev *dev, u8 **out_buffer)
{
	u8 in[MLX5_ST_SZ_BYTES(query_diagnostic_counters_in)] = {0};
	struct mlx5_diag_cnt *diag_cnt = &dev->diag_cnt;
	u16 out_sz;
	u8 *out;
	int err;

	out_sz = MLX5_ST_SZ_BYTES(query_diagnostic_counters_out) +
	    diag_cnt->num_of_samples * diag_cnt->num_cnt_id *
	    MLX5_ST_SZ_BYTES(diagnostic_cntr_struct);

	out = kzalloc(out_sz, GFP_KERNEL);
	if (!out)
		return (-ENOMEM);

	MLX5_SET(query_diagnostic_counters_in, in, opcode,
	    MLX5_CMD_OP_QUERY_DIAGNOSTICS);
	MLX5_SET(query_diagnostic_counters_in, in, num_of_samples,
	    diag_cnt->num_of_samples);
	MLX5_SET(query_diagnostic_counters_in, in, sample_index,
	    diag_cnt->sample_index);

	err = mlx5_cmd_exec(dev, in, sizeof(in), out, out_sz);

	if (!err)
		*out_buffer = out;
	else
		kfree(out);

	return (err);
}

int
mlx5_diag_cnt_init(struct mlx5_core_dev *dev)
{
	struct mlx5_diag_cnt *diag_cnt = &dev->diag_cnt;
	struct sysctl_oid *diag_cnt_sysctl_node;
	int err;

	if (!MLX5_DIAG_CNT_SUPPORTED(dev))
		return (0);

	mutex_init(&diag_cnt->lock);

	/* Build private data */
	err = get_supported_cnt_ids(dev);
	if (err)
		return (err);

	sysctl_ctx_init(&diag_cnt->sysctl_ctx);

	diag_cnt_sysctl_node = SYSCTL_ADD_NODE(&diag_cnt->sysctl_ctx,
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev->pdev->dev.bsddev)),
	    OID_AUTO, "diag_cnt", CTLFLAG_RW | CTLFLAG_MPSAFE, NULL,
	    "Diagnostics counters");

	if (diag_cnt_sysctl_node == NULL)
		return (-ENOMEM);

	SYSCTL_ADD_PROC(&diag_cnt->sysctl_ctx, SYSCTL_CHILDREN(diag_cnt_sysctl_node),
	    OID_AUTO, "counter_id", CTLTYPE_U16 | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    dev, 0, mlx5_sysctl_counter_id, "SU", "Selected counter IDs");

	SYSCTL_ADD_PROC(&diag_cnt->sysctl_ctx, SYSCTL_CHILDREN(diag_cnt_sysctl_node),
	    OID_AUTO, "params", CTLTYPE_U32 | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    dev, 0, mlx5_sysctl_params, "IU",
	    "Counter parameters: log_num_of_samples, log_sample_perios, flag, num_of_samples, sample_index");

	SYSCTL_ADD_PROC(&diag_cnt->sysctl_ctx, SYSCTL_CHILDREN(diag_cnt_sysctl_node),
	    OID_AUTO, "dump_set", CTLTYPE_U8 | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    dev, 0, mlx5_sysctl_dump_set, "CU",
	    "Set dump parameters by writing 1 and enable dump_get. Write 0 to disable dump.");

	SYSCTL_ADD_PROC(&diag_cnt->sysctl_ctx, SYSCTL_CHILDREN(diag_cnt_sysctl_node),
	    OID_AUTO, "dump_get", CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    dev, 0, mlx5_sysctl_dump_get, "A",
	    "Get dump parameters.");

	SYSCTL_ADD_PROC(&diag_cnt->sysctl_ctx, SYSCTL_CHILDREN(diag_cnt_sysctl_node),
	    OID_AUTO, "cap", CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    dev, 0, mlx5_sysctl_cap_read, "A",
	    "Read capabilities.");

	return (0);
}

void
mlx5_diag_cnt_cleanup(struct mlx5_core_dev *dev)
{
	struct mlx5_diag_cnt *diag_cnt = &dev->diag_cnt;
	void *ptr;

	if (!MLX5_DIAG_CNT_SUPPORTED(dev))
		return;

	sysctl_ctx_free(&diag_cnt->sysctl_ctx);

	ptr = diag_cnt->cnt_id;
	diag_cnt->cnt_id = NULL;

	kfree(ptr);

	reset_params(diag_cnt);
}

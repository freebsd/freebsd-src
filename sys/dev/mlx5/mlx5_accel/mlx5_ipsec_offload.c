/*-
 * Copyright (c) 2023 NVIDIA corporation & affiliates.
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
 */

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <net/pfkeyv2.h>
#include <netipsec/ipsec.h>
#include <dev/mlx5/mlx5_en/en.h>
#include <dev/mlx5/crypto.h>
#include <dev/mlx5/mlx5_accel/ipsec.h>

u32 mlx5_ipsec_device_caps(struct mlx5_core_dev *mdev)
{
	u32 caps = 0;

	if (!MLX5_CAP_GEN(mdev, ipsec_offload))
		return 0;

	if (!MLX5_CAP_GEN(mdev, log_max_dek))
		return 0;

	if (!(MLX5_CAP_GEN_64(mdev, general_obj_types) &
	    MLX5_HCA_CAP_GENERAL_OBJECT_TYPES_IPSEC))
		return 0;

	if (!MLX5_CAP_FLOWTABLE_NIC_TX(mdev, ipsec_encrypt) ||
	    !MLX5_CAP_FLOWTABLE_NIC_RX(mdev, ipsec_decrypt))
		return 0;

	if (!MLX5_CAP_IPSEC(mdev, ipsec_crypto_esp_aes_gcm_128_encrypt) ||
	    !MLX5_CAP_IPSEC(mdev, ipsec_crypto_esp_aes_gcm_128_decrypt))
		return 0;

        if (MLX5_CAP_IPSEC(mdev, ipsec_full_offload)) {
                if (MLX5_CAP_FLOWTABLE_NIC_TX(mdev,
                                              reformat_add_esp_trasport) &&
                    MLX5_CAP_FLOWTABLE_NIC_RX(mdev,
                                              reformat_del_esp_trasport) &&
                    MLX5_CAP_FLOWTABLE_NIC_RX(mdev, decap))
			caps |= MLX5_IPSEC_CAP_PACKET_OFFLOAD;

                if (MLX5_CAP_FLOWTABLE_NIC_TX(mdev, ignore_flow_level) &&
                    MLX5_CAP_FLOWTABLE_NIC_RX(mdev, ignore_flow_level))
			caps |= MLX5_IPSEC_CAP_PRIO;

		if (MLX5_CAP_FLOWTABLE_NIC_TX(mdev, reformat_add_esp_transport_over_udp) &&
		    MLX5_CAP_FLOWTABLE_NIC_RX(mdev, reformat_del_esp_transport_over_udp))
			caps |= MLX5_IPSEC_CAP_ESPINUDP;
        }

        if (!caps)
		return 0;

	if (MLX5_CAP_IPSEC(mdev, ipsec_esn))
		caps |= MLX5_IPSEC_CAP_ESN;

	return caps;
}
EXPORT_SYMBOL_GPL(mlx5_ipsec_device_caps);

static void mlx5e_ipsec_packet_setup(void *obj, u32 pdn,
				     struct mlx5_accel_esp_xfrm_attrs *attrs)
{
	void *aso_ctx;

	aso_ctx = MLX5_ADDR_OF(ipsec_obj, obj, ipsec_aso);
	/* ASO context */
	MLX5_SET(ipsec_obj, obj, ipsec_aso_access_pd, pdn);
	MLX5_SET(ipsec_obj, obj, full_offload, 1);
	MLX5_SET(ipsec_aso, aso_ctx, valid, 1);
	/* MLX5_IPSEC_ASO_REG_C_4_5 is type C register that is used
	 * in flow steering to perform matching against. Please be
	 * aware that this register was chosen arbitrary and can't
	 * be used in other places as long as IPsec packet offload
	 * active.
	 */
	MLX5_SET(ipsec_obj, obj, aso_return_reg, MLX5_IPSEC_ASO_REG_C_4_5);
	if (attrs->replay_esn.trigger) {
		MLX5_SET(ipsec_aso, aso_ctx, esn_event_arm, 1);

		if (attrs->dir == IPSEC_DIR_INBOUND) {
			MLX5_SET(ipsec_aso, aso_ctx, window_sz,
				 attrs->replay_esn.replay_window);
			if (attrs->replay_esn.replay_window != 0)
				MLX5_SET(ipsec_aso, aso_ctx, mode,
				    MLX5_IPSEC_ASO_REPLAY_PROTECTION);
			else
				MLX5_SET(ipsec_aso, aso_ctx, mode,
				    MLX5_IPSEC_ASO_MODE);
		}
		MLX5_SET(ipsec_aso, aso_ctx, mode_parameter,
			 attrs->replay_esn.esn);
	}

	switch (attrs->dir) {
	case IPSEC_DIR_OUTBOUND:
		if (attrs->replay_esn.replay_window != 0)
			MLX5_SET(ipsec_aso, aso_ctx, mode, MLX5_IPSEC_ASO_INC_SN);
		else
			MLX5_SET(ipsec_aso, aso_ctx, mode, MLX5_IPSEC_ASO_MODE);
		break;
	default:
		break;
	}
}

static int mlx5_create_ipsec_obj(struct mlx5e_ipsec_sa_entry *sa_entry)
{
	struct mlx5_accel_esp_xfrm_attrs *attrs = &sa_entry->attrs;
	struct mlx5_core_dev *mdev = mlx5e_ipsec_sa2dev(sa_entry);
	struct aes_gcm_keymat *aes_gcm = &attrs->aes_gcm;
	u32 out[MLX5_ST_SZ_DW(general_obj_out_cmd_hdr)];
	u32 in[MLX5_ST_SZ_DW(create_ipsec_obj_in)] = {};
	void *obj, *salt_p, *salt_iv_p;
	int err;

	obj = MLX5_ADDR_OF(create_ipsec_obj_in, in, ipsec_object);

	/* salt and seq_iv */
	salt_p = MLX5_ADDR_OF(ipsec_obj, obj, salt);
	memcpy(salt_p, &aes_gcm->salt, sizeof(aes_gcm->salt));

	MLX5_SET(ipsec_obj, obj, icv_length, MLX5_IPSEC_OBJECT_ICV_LEN_16B);
	salt_iv_p = MLX5_ADDR_OF(ipsec_obj, obj, implicit_iv);
	memcpy(salt_iv_p, &aes_gcm->seq_iv, sizeof(aes_gcm->seq_iv));

	/* esn */
	if (attrs->replay_esn.trigger) {
		MLX5_SET(ipsec_obj, obj, esn_en, 1);
		MLX5_SET(ipsec_obj, obj, esn_msb, attrs->replay_esn.esn_msb);
		MLX5_SET(ipsec_obj, obj, esn_overlap, attrs->replay_esn.overlap);
	}

	/* enc./dec. key */
	MLX5_SET(ipsec_obj, obj, dekn, sa_entry->enc_key_id);

	/* general object fields set */
	MLX5_SET(general_obj_in_cmd_hdr, in, opcode,
		 MLX5_CMD_OP_CREATE_GENERAL_OBJ);
	MLX5_SET(general_obj_in_cmd_hdr, in, obj_type,
		 MLX5_GENERAL_OBJECT_TYPES_IPSEC);

	mlx5e_ipsec_packet_setup(obj, sa_entry->ipsec->pdn, attrs);

	err = mlx5_cmd_exec(mdev, in, sizeof(in), out, sizeof(out));
	if (!err)
		sa_entry->ipsec_obj_id =
			MLX5_GET(general_obj_out_cmd_hdr, out, obj_id);

	return err;
}

static void mlx5_destroy_ipsec_obj(struct mlx5e_ipsec_sa_entry *sa_entry)
{
	struct mlx5_core_dev *mdev = mlx5e_ipsec_sa2dev(sa_entry);
	u32 in[MLX5_ST_SZ_DW(general_obj_in_cmd_hdr)] = {};
	u32 out[MLX5_ST_SZ_DW(general_obj_out_cmd_hdr)];

	MLX5_SET(general_obj_in_cmd_hdr, in, opcode,
		 MLX5_CMD_OP_DESTROY_GENERAL_OBJ);
	MLX5_SET(general_obj_in_cmd_hdr, in, obj_type,
		 MLX5_GENERAL_OBJECT_TYPES_IPSEC);
	MLX5_SET(general_obj_in_cmd_hdr, in, obj_id, sa_entry->ipsec_obj_id);

	mlx5_cmd_exec(mdev, in, sizeof(in), out, sizeof(out));
}

int mlx5_ipsec_create_sa_ctx(struct mlx5e_ipsec_sa_entry *sa_entry)
{
	struct aes_gcm_keymat *aes_gcm = &sa_entry->attrs.aes_gcm;
	struct mlx5_core_dev *mdev = mlx5e_ipsec_sa2dev(sa_entry);
	int err;

	/* key */
	err = mlx5_encryption_key_create(mdev, sa_entry->ipsec->pdn,
					 MLX5_GENERAL_OBJECT_TYPE_ENCRYPTION_KEY_TYPE_IPSEC,
					 aes_gcm->aes_key,
					 aes_gcm->key_len,
					 &sa_entry->enc_key_id);
	if (err) {
		mlx5_core_dbg(mdev, "Failed to create encryption key (err = %d)\n", err);
		return err;
	}

	err = mlx5_create_ipsec_obj(sa_entry);
	if (err) {
		mlx5_core_dbg(mdev, "Failed to create IPsec object (err = %d)\n", err);
		goto err_enc_key;
	}

	return 0;

err_enc_key:
	mlx5_encryption_key_destroy(mdev, sa_entry->enc_key_id);
	return err;
}

void mlx5_ipsec_free_sa_ctx(struct mlx5e_ipsec_sa_entry *sa_entry)
{
	struct mlx5_core_dev *mdev = mlx5e_ipsec_sa2dev(sa_entry);

	mlx5_destroy_ipsec_obj(sa_entry);
	mlx5_encryption_key_destroy(mdev, sa_entry->enc_key_id);
}

static void mlx5e_ipsec_aso_copy(struct mlx5_wqe_aso_ctrl_seg *ctrl,
				 struct mlx5_wqe_aso_ctrl_seg *data)
{
	if (!data)
		return;

	ctrl->data_mask_mode = data->data_mask_mode;
	ctrl->condition_1_0_operand = data->condition_1_0_operand;
	ctrl->condition_1_0_offset = data->condition_1_0_offset;
	ctrl->data_offset_condition_operand = data->data_offset_condition_operand;
	ctrl->condition_0_data = data->condition_0_data;
	ctrl->condition_0_mask = data->condition_0_mask;
	ctrl->condition_1_data = data->condition_1_data;
	ctrl->condition_1_mask = data->condition_1_mask;
	ctrl->bitwise_data = data->bitwise_data;
	ctrl->data_mask = data->data_mask;
}

static int mlx5e_ipsec_aso_query(struct mlx5e_ipsec_sa_entry *sa_entry,
				 struct mlx5_wqe_aso_ctrl_seg *data)
{
	struct mlx5e_ipsec *ipsec = sa_entry->ipsec;
	struct mlx5e_ipsec_aso *aso = ipsec->aso;
	struct mlx5_wqe_aso_ctrl_seg *ctrl;
	struct mlx5_aso_wqe *wqe;
	unsigned long expires;
	u8 ds_cnt;
	int ret;

	spin_lock_bh(&aso->lock);
	memset(aso->ctx, 0, sizeof(aso->ctx));
	wqe = mlx5_aso_get_wqe(aso->aso);
	ds_cnt = DIV_ROUND_UP(sizeof(*wqe), MLX5_SEND_WQE_DS);
	mlx5_aso_build_wqe(aso->aso, ds_cnt, wqe, sa_entry->ipsec_obj_id,
			   MLX5_ACCESS_ASO_OPC_MOD_IPSEC);

	ctrl = &wqe->aso_ctrl;
	ctrl->va_l = cpu_to_be32(lower_32_bits(aso->dma_addr) | ASO_CTRL_READ_EN);
	ctrl->va_h = cpu_to_be32(upper_32_bits(aso->dma_addr));
	ctrl->l_key = cpu_to_be32(ipsec->mkey);
	mlx5e_ipsec_aso_copy(ctrl, data);

	mlx5_aso_post_wqe(aso->aso, false, &wqe->ctrl);
	expires = jiffies + msecs_to_jiffies(10);
	do {
		ret = mlx5_aso_poll_cq(aso->aso, false);
		if (ret)
			/* We are in atomic context */
			udelay(10);
	} while (ret && time_is_after_jiffies(expires));
	spin_unlock_bh(&aso->lock);

	return ret;
}

#define MLX5E_IPSEC_ESN_SCOPE_MID 0x80000000L

static int mlx5_modify_ipsec_obj(struct mlx5e_ipsec_sa_entry *sa_entry,
                                 const struct mlx5_accel_esp_xfrm_attrs *attrs)
{
        struct mlx5_core_dev *mdev = mlx5e_ipsec_sa2dev(sa_entry);
        u32 in[MLX5_ST_SZ_DW(modify_ipsec_obj_in)] = {};
        u32 out[MLX5_ST_SZ_DW(query_ipsec_obj_out)];
        u64 modify_field_select = 0;
        u64 general_obj_types;
        void *obj;
        int err;

        general_obj_types = MLX5_CAP_GEN_64(mdev, general_obj_types);
        if (!(general_obj_types & MLX5_HCA_CAP_GENERAL_OBJECT_TYPES_IPSEC))
                return -EINVAL;

        /* general object fields set */
        MLX5_SET(general_obj_in_cmd_hdr, in, opcode, MLX5_CMD_OP_QUERY_GENERAL_OBJ);
        MLX5_SET(general_obj_in_cmd_hdr, in, obj_type, MLX5_GENERAL_OBJECT_TYPES_IPSEC);
        MLX5_SET(general_obj_in_cmd_hdr, in, obj_id, sa_entry->ipsec_obj_id);
        err = mlx5_cmd_exec(mdev, in, sizeof(in), out, sizeof(out));
        if (err) {
                mlx5_core_err(mdev, "Query IPsec object failed (Object id %d), err = %d\n",
                              sa_entry->ipsec_obj_id, err);
                return err;
        }

        obj = MLX5_ADDR_OF(query_ipsec_obj_out, out, ipsec_object);
        modify_field_select = MLX5_GET64(ipsec_obj, obj, modify_field_select);

        /* esn */
        if (!(modify_field_select & MLX5_MODIFY_IPSEC_BITMASK_ESN_OVERLAP) ||
            !(modify_field_select & MLX5_MODIFY_IPSEC_BITMASK_ESN_MSB))
                return -EOPNOTSUPP;

        obj = MLX5_ADDR_OF(modify_ipsec_obj_in, in, ipsec_object);
        MLX5_SET64(ipsec_obj, obj, modify_field_select,
                   MLX5_MODIFY_IPSEC_BITMASK_ESN_OVERLAP |
                           MLX5_MODIFY_IPSEC_BITMASK_ESN_MSB);
        MLX5_SET(ipsec_obj, obj, esn_msb, attrs->replay_esn.esn_msb);
        MLX5_SET(ipsec_obj, obj, esn_overlap, attrs->replay_esn.overlap);

        /* general object fields set */
        MLX5_SET(general_obj_in_cmd_hdr, in, opcode, MLX5_CMD_OP_MODIFY_GENERAL_OBJ);

        return mlx5_cmd_exec(mdev, in, sizeof(in), out, sizeof(out));
}

static void mlx5_accel_esp_modify_xfrm(struct mlx5e_ipsec_sa_entry *sa_entry,
				       const struct mlx5_accel_esp_xfrm_attrs *attrs)
{
	int err;

	err = mlx5_modify_ipsec_obj(sa_entry, attrs);
	if (err)
		return;

	memcpy(&sa_entry->attrs, attrs, sizeof(sa_entry->attrs));
}

static void mlx5e_ipsec_aso_update(struct mlx5e_ipsec_sa_entry *sa_entry,
				   struct mlx5_wqe_aso_ctrl_seg *data)
{
	data->data_mask_mode = MLX5_ASO_DATA_MASK_MODE_BITWISE_64BIT << 6;
	data->condition_1_0_operand = MLX5_ASO_ALWAYS_TRUE | MLX5_ASO_ALWAYS_TRUE << 4;

	mlx5e_ipsec_aso_query(sa_entry, data);
}

#define MLX5_IPSEC_ASO_REMOVE_FLOW_PKT_CNT_OFFSET 0

static void mlx5e_ipsec_update_esn_state(struct mlx5e_ipsec_sa_entry *sa_entry,
					 u32 mode_param)
{
	struct mlx5_accel_esp_xfrm_attrs attrs = {};
	struct mlx5_wqe_aso_ctrl_seg data = {};

	if (mode_param < MLX5E_IPSEC_ESN_SCOPE_MID) {
		sa_entry->esn_state.esn_msb++;
		sa_entry->esn_state.overlap = 0;
	} else {
		sa_entry->esn_state.overlap = 1;
	}

	mlx5e_ipsec_build_accel_xfrm_attrs(sa_entry, &attrs, sa_entry->attrs.dir);

	mlx5_accel_esp_modify_xfrm(sa_entry, &attrs);

	data.data_offset_condition_operand = MLX5_IPSEC_ASO_REMOVE_FLOW_PKT_CNT_OFFSET;
	data.bitwise_data = cpu_to_be64(BIT_ULL(54));
	data.data_mask = data.bitwise_data;

	mlx5e_ipsec_aso_update(sa_entry, &data);
}

static void mlx5e_ipsec_handle_event(struct work_struct *_work)
{
	struct mlx5e_ipsec_work *work =
		container_of(_work, struct mlx5e_ipsec_work, work);
	struct mlx5e_ipsec_sa_entry *sa_entry = work->data;
	struct mlx5_accel_esp_xfrm_attrs *attrs;
	struct mlx5e_ipsec_aso *aso;
	int ret;

	aso = sa_entry->ipsec->aso;
	attrs = &sa_entry->attrs;

	/* TODO: Kostia, this event should be locked/protected
	 * from concurent SA delete.
	 */
	ret = mlx5e_ipsec_aso_query(sa_entry, NULL);
	if (ret)
		goto unlock;

	if (attrs->replay_esn.trigger &&
	    !MLX5_GET(ipsec_aso, aso->ctx, esn_event_arm)) {
		u32 mode_param = MLX5_GET(ipsec_aso, aso->ctx, mode_parameter);

	        mlx5e_ipsec_update_esn_state(sa_entry, mode_param);
	}

unlock:
	kfree(work);
}

void mlx5_object_change_event(struct mlx5_core_dev *dev, struct mlx5_eqe *eqe)
{
	struct mlx5e_ipsec_sa_entry *sa_entry;
	struct mlx5_eqe_obj_change *object;
	struct mlx5e_ipsec_work *work;
	u16 type;

	object = &eqe->data.obj_change;
	type = be16_to_cpu(object->obj_type);

	if (type != MLX5_GENERAL_OBJECT_TYPES_IPSEC)
		return;

	sa_entry = xa_load(&dev->ipsec_sadb, be32_to_cpu(object->obj_id));
	if (!sa_entry)
		return;

	work = kmalloc(sizeof(*work), GFP_ATOMIC);
	if (!work)
		return;

	INIT_WORK(&work->work, mlx5e_ipsec_handle_event);
	work->data = sa_entry;

	queue_work(sa_entry->ipsec->wq, &work->work);
}

int mlx5e_ipsec_aso_init(struct mlx5e_ipsec *ipsec)
{
	struct mlx5_core_dev *mdev = ipsec->mdev;
	struct mlx5e_ipsec_aso *aso;
	struct device *pdev;
	int err;

	aso = kzalloc(sizeof(*ipsec->aso), GFP_KERNEL);
	if (!aso)
		return -ENOMEM;

	pdev = &mdev->pdev->dev;
	aso->dma_addr = dma_map_single(pdev, aso->ctx, sizeof(aso->ctx), DMA_BIDIRECTIONAL);
	err = dma_mapping_error(pdev, aso->dma_addr);
	if (err)
		goto err_dma;

	aso->aso = mlx5_aso_create(mdev, ipsec->pdn);
	if (IS_ERR(aso->aso)) {
		err = PTR_ERR(aso->aso);
		goto err_aso_create;
	}

	spin_lock_init(&aso->lock);
	ipsec->aso = aso;
	return 0;

err_aso_create:
	dma_unmap_single(pdev, aso->dma_addr, sizeof(aso->ctx), DMA_BIDIRECTIONAL);
err_dma:
	kfree(aso);
	return err;
}

void mlx5e_ipsec_aso_cleanup(struct mlx5e_ipsec *ipsec)
{
	struct mlx5_core_dev *mdev = ipsec->mdev;
	struct mlx5e_ipsec_aso *aso;
	struct device *pdev;

	aso = ipsec->aso;
	pdev = &mdev->pdev->dev;

	mlx5_aso_destroy(aso->aso);
	dma_unmap_single(pdev, aso->dma_addr, sizeof(aso->ctx), DMA_BIDIRECTIONAL);
	kfree(aso);
}

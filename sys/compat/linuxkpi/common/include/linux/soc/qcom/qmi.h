/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022-2023 Bjoern A. Zeeb
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
 */

#ifndef	_LINUXKPI_LINUX_SOC_QCOM_QMI_H
#define	_LINUXKPI_LINUX_SOC_QCOM_QMI_H

/* QMI (Qualcomm MSM Interface) */

#include <linux/qrtr.h>

enum soc_qcom_qmi_data_type {
	QMI_EOTI,
	QMI_DATA_LEN,
	QMI_OPT_FLAG,
	QMI_UNSIGNED_1_BYTE,
	QMI_UNSIGNED_2_BYTE,
	QMI_UNSIGNED_4_BYTE,
	QMI_UNSIGNED_8_BYTE,
	QMI_SIGNED_4_BYTE_ENUM,
	QMI_STRUCT,
	QMI_STRING,
};

#define	QMI_RESULT_SUCCESS_V01	__LINE__
#define	QMI_INDICATION		__LINE__

struct qmi_handle;

enum soc_qcom_qmi_array_type {
	NO_ARRAY,
	STATIC_ARRAY,
	VAR_LEN_ARRAY,
};

/* Should this become an enum? */
#define	QMI_COMMON_TLV_TYPE			0

struct qmi_elem_info {
	enum soc_qcom_qmi_data_type		data_type;
	uint32_t				elem_len;
	uint32_t				elem_size;
	enum soc_qcom_qmi_array_type		array_type;
	uint8_t					tlv_type;
	uint32_t				offset;
	const struct qmi_elem_info		*ei_array;
};

struct qmi_response_type_v01 {
	uint16_t				result;
	uint16_t				error;
};

struct qmi_txn {
};

struct qmi_service {
	uint32_t				node;
	uint32_t				port;
};

struct qmi_msg_handler {
	uint32_t				type;
	uint32_t				msg_id;
	const struct qmi_elem_info		*ei;
	size_t					decoded_size;
	void	(*fn)(struct qmi_handle *, struct sockaddr_qrtr *, struct qmi_txn *, const void *);
};

struct qmi_ops {
	int	(*new_server)(struct qmi_handle *, struct qmi_service *);
	void	(*del_server)(struct qmi_handle *, struct qmi_service *);
};

struct qmi_handle {
	int				sock;

	const struct qmi_msg_handler	*handler;
	struct qmi_ops			ops;
};


/* XXX-TODO need implementation somewhere... it is not in ath1xk* */
extern struct qmi_elem_info qmi_response_type_v01_ei[];

static inline int
qmi_handle_init(struct qmi_handle *handle, size_t resp_len_max,
    const struct qmi_ops *ops, const struct qmi_msg_handler *handler)
{

	handle->handler = handler;
	if (ops != NULL)
		handle->ops = *ops;

        /* We will find out what else to do here. */
	/* XXX TODO */

	return (0);
}

static __inline int
qmi_add_lookup(struct qmi_handle *handle, uint32_t service, uint32_t version,
    uint32_t service_ins_id)
{

	/* XXX TODO */
	return (0);
}

static __inline void
qmi_handle_release(struct qmi_handle *handle)
{

	/* XXX TODO */
}

static __inline int
qmi_send_request(struct qmi_handle *handle, void *x, struct qmi_txn *txn,
    uint32_t msd_id, size_t len, const struct qmi_elem_info *ei, void *req)
{

	/* XXX TODO */
	return (-ENXIO);
}

static __inline void
qmi_txn_cancel(struct qmi_txn *txn)
{

	/* XXX TODO */
}

static __inline int
qmi_txn_init(struct qmi_handle *handle, struct qmi_txn *txn,
    const struct qmi_elem_info *ei, void *resp)
{

	/* XXX TODO */
	return (-ENXIO);
}

static __inline int
qmi_txn_wait(struct qmi_txn *txn, uint64_t jiffies)
{

	/* XXX TODO */
	return (-ENXIO);
}

#endif	/* _LINUXKPI_LINUX_SOC_QCOM_QMI_H */

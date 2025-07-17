/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022-2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#ifndef __NVMF_TCP_H__
#define	__NVMF_TCP_H__

#ifndef _KERNEL
#define	MPASS			assert
#endif

#define	NVME_SGL_TYPE_ICD						\
	NVME_SGL_TYPE(NVME_SGL_TYPE_DATA_BLOCK, NVME_SGL_SUBTYPE_OFFSET)

#define	NVME_SGL_TYPE_COMMAND_BUFFER					\
	NVME_SGL_TYPE(NVME_SGL_TYPE_TRANSPORT_DATA_BLOCK,		\
	    NVME_SGL_SUBTYPE_TRANSPORT)

/*
 * Validate common fields in a received PDU header.  If an error is
 * detected that requires an immediate disconnect, ECONNRESET is
 * returned.  If an error is detected that should be reported, EBADMSG
 * is returned and *fes and *fei are set to the values to be used in a
 * termination request PDU.  If no error is detected, 0 is returned
 * and *data_lenp is set to the length of any included data.
 *
 * Section number references refer to NVM Express over Fabrics
 * Revision 1.1 dated October 22, 2019.
 */
static __inline int
nvmf_tcp_validate_pdu_header(const struct nvme_tcp_common_pdu_hdr *ch,
    bool controller, bool header_digests, bool data_digests, uint8_t rxpda,
    uint32_t *data_lenp, uint16_t *fes, uint32_t *fei)
{
	uint32_t data_len, plen;
	u_int expected_hlen, full_hlen;
	uint8_t digest_flags, valid_flags;

	plen = le32toh(ch->plen);
	full_hlen = ch->hlen;
	if ((ch->flags & NVME_TCP_CH_FLAGS_HDGSTF) != 0)
		full_hlen += sizeof(uint32_t);
	if (plen == full_hlen)
		data_len = 0;
	else
		data_len = plen - ch->pdo;

	/*
	 * Errors must be reported for the lowest incorrect field
	 * first, so validate fields in order.
	 */

	/* Validate pdu_type. */

	/* Controllers only receive PDUs with a PDU direction of 0. */
	if (controller != ((ch->pdu_type & 0x01) == 0)) {
		printf("NVMe/TCP: Invalid PDU type %u\n", ch->pdu_type);
		*fes = NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD;
		*fei = offsetof(struct nvme_tcp_common_pdu_hdr, pdu_type);
		return (EBADMSG);
	}

	switch (ch->pdu_type) {
	case NVME_TCP_PDU_TYPE_IC_REQ:
	case NVME_TCP_PDU_TYPE_IC_RESP:
		/* Shouldn't get these for an established connection. */
		printf("NVMe/TCP: Received Initialize Connection PDU\n");
		*fes = NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD;
		*fei = offsetof(struct nvme_tcp_common_pdu_hdr, pdu_type);
		return (EBADMSG);
	case NVME_TCP_PDU_TYPE_H2C_TERM_REQ:
	case NVME_TCP_PDU_TYPE_C2H_TERM_REQ:
		/*
		 * 7.4.7 Termination requests with invalid PDU lengths
		 * result in an immediate connection termination
		 * without reporting an error.
		 */
		if (plen < sizeof(struct nvme_tcp_term_req_hdr) ||
		    plen > NVME_TCP_TERM_REQ_PDU_MAX_SIZE) {
			printf("NVMe/TCP: Received invalid termination request\n");
			return (ECONNRESET);
		}
		break;
	case NVME_TCP_PDU_TYPE_CAPSULE_CMD:
	case NVME_TCP_PDU_TYPE_CAPSULE_RESP:
	case NVME_TCP_PDU_TYPE_H2C_DATA:
	case NVME_TCP_PDU_TYPE_C2H_DATA:
	case NVME_TCP_PDU_TYPE_R2T:
		break;
	default:
		printf("NVMe/TCP: Invalid PDU type %u\n", ch->pdu_type);
		*fes = NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD;
		*fei = offsetof(struct nvme_tcp_common_pdu_hdr, pdu_type);
		return (EBADMSG);
	}

	/* Validate flags. */
	switch (ch->pdu_type) {
	default:
		__assert_unreachable();
		break;
	case NVME_TCP_PDU_TYPE_H2C_TERM_REQ:
	case NVME_TCP_PDU_TYPE_C2H_TERM_REQ:
		valid_flags = 0;
		break;
	case NVME_TCP_PDU_TYPE_CAPSULE_CMD:
		valid_flags = NVME_TCP_CH_FLAGS_HDGSTF |
		    NVME_TCP_CH_FLAGS_DDGSTF;
		break;
	case NVME_TCP_PDU_TYPE_CAPSULE_RESP:
	case NVME_TCP_PDU_TYPE_R2T:
		valid_flags = NVME_TCP_CH_FLAGS_HDGSTF;
		break;
	case NVME_TCP_PDU_TYPE_H2C_DATA:
		valid_flags = NVME_TCP_CH_FLAGS_HDGSTF |
		    NVME_TCP_CH_FLAGS_DDGSTF | NVME_TCP_H2C_DATA_FLAGS_LAST_PDU;
		break;
	case NVME_TCP_PDU_TYPE_C2H_DATA:
		valid_flags = NVME_TCP_CH_FLAGS_HDGSTF |
		    NVME_TCP_CH_FLAGS_DDGSTF | NVME_TCP_C2H_DATA_FLAGS_LAST_PDU |
		    NVME_TCP_C2H_DATA_FLAGS_SUCCESS;
		break;
	}
	if ((ch->flags & ~valid_flags) != 0) {
		printf("NVMe/TCP: Invalid PDU header flags %#x\n", ch->flags);
		*fes = NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD;
		*fei = offsetof(struct nvme_tcp_common_pdu_hdr, flags);
		return (EBADMSG);
	}

	/*
	 * Verify that digests are present iff enabled.  Note that the
	 * data digest will not be present if there is no data
	 * payload.
	 */
	digest_flags = 0;
	if (header_digests)
		digest_flags |= NVME_TCP_CH_FLAGS_HDGSTF;
	if (data_digests && data_len != 0)
		digest_flags |= NVME_TCP_CH_FLAGS_DDGSTF;
	if ((digest_flags & valid_flags) !=
	    (ch->flags & (NVME_TCP_CH_FLAGS_HDGSTF |
	    NVME_TCP_CH_FLAGS_DDGSTF))) {
		printf("NVMe/TCP: Invalid PDU header flags %#x\n", ch->flags);
		*fes = NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD;
		*fei = offsetof(struct nvme_tcp_common_pdu_hdr, flags);
		return (EBADMSG);
	}

	/* 7.4.5.2: SUCCESS in C2H requires LAST_PDU */
	if (ch->pdu_type == NVME_TCP_PDU_TYPE_C2H_DATA &&
	    (ch->flags & (NVME_TCP_C2H_DATA_FLAGS_LAST_PDU |
	    NVME_TCP_C2H_DATA_FLAGS_SUCCESS)) ==
	    NVME_TCP_C2H_DATA_FLAGS_SUCCESS) {
		printf("NVMe/TCP: Invalid PDU header flags %#x\n", ch->flags);
		*fes = NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD;
		*fei = offsetof(struct nvme_tcp_common_pdu_hdr, flags);
		return (EBADMSG);
	}

	/* Validate hlen. */
	switch (ch->pdu_type) {
	default:
		__assert_unreachable();
		break;
	case NVME_TCP_PDU_TYPE_H2C_TERM_REQ:
	case NVME_TCP_PDU_TYPE_C2H_TERM_REQ:
		expected_hlen = sizeof(struct nvme_tcp_term_req_hdr);
		break;
	case NVME_TCP_PDU_TYPE_CAPSULE_CMD:
		expected_hlen = sizeof(struct nvme_tcp_cmd);
		break;
	case NVME_TCP_PDU_TYPE_CAPSULE_RESP:
		expected_hlen = sizeof(struct nvme_tcp_rsp);
		break;
	case NVME_TCP_PDU_TYPE_H2C_DATA:
		expected_hlen = sizeof(struct nvme_tcp_h2c_data_hdr);
		break;
	case NVME_TCP_PDU_TYPE_C2H_DATA:
		expected_hlen = sizeof(struct nvme_tcp_c2h_data_hdr);
		break;
	case NVME_TCP_PDU_TYPE_R2T:
		expected_hlen = sizeof(struct nvme_tcp_r2t_hdr);
		break;
	}
	if (ch->hlen != expected_hlen) {
		printf("NVMe/TCP: Invalid PDU header length %u\n", ch->hlen);
		*fes = NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD;
		*fei = offsetof(struct nvme_tcp_common_pdu_hdr, hlen);
		return (EBADMSG);
	}

	/* Validate pdo. */
	switch (ch->pdu_type) {
	default:
		__assert_unreachable();
		break;
	case NVME_TCP_PDU_TYPE_H2C_TERM_REQ:
	case NVME_TCP_PDU_TYPE_C2H_TERM_REQ:
	case NVME_TCP_PDU_TYPE_CAPSULE_RESP:
	case NVME_TCP_PDU_TYPE_R2T:
		if (ch->pdo != 0) {
			printf("NVMe/TCP: Invalid PDU data offset %u\n",
			    ch->pdo);
			*fes = NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD;
			*fei = offsetof(struct nvme_tcp_common_pdu_hdr, pdo);
			return (EBADMSG);
		}
		break;
	case NVME_TCP_PDU_TYPE_CAPSULE_CMD:
	case NVME_TCP_PDU_TYPE_H2C_DATA:
	case NVME_TCP_PDU_TYPE_C2H_DATA:
		/* Permit PDO of 0 if there is no data. */
		if (data_len == 0 && ch->pdo == 0)
			break;

		if (ch->pdo < full_hlen || ch->pdo > plen ||
		    ch->pdo % rxpda != 0) {
			printf("NVMe/TCP: Invalid PDU data offset %u\n",
			    ch->pdo);
			*fes = NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD;
			*fei = offsetof(struct nvme_tcp_common_pdu_hdr, pdo);
			return (EBADMSG);
		}
		break;
	}

	/* Validate plen. */
	if (plen < ch->hlen) {
		printf("NVMe/TCP: Invalid PDU length %u\n", plen);
		*fes = NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD;
		*fei = offsetof(struct nvme_tcp_common_pdu_hdr, plen);
		return (EBADMSG);
	}

	switch (ch->pdu_type) {
	default:
		__assert_unreachable();
		break;
	case NVME_TCP_PDU_TYPE_H2C_TERM_REQ:
	case NVME_TCP_PDU_TYPE_C2H_TERM_REQ:
		/* Checked above. */
		MPASS(plen <= NVME_TCP_TERM_REQ_PDU_MAX_SIZE);
		break;
	case NVME_TCP_PDU_TYPE_CAPSULE_CMD:
	case NVME_TCP_PDU_TYPE_H2C_DATA:
	case NVME_TCP_PDU_TYPE_C2H_DATA:
		if ((ch->flags & NVME_TCP_CH_FLAGS_DDGSTF) != 0 &&
		    data_len <= sizeof(uint32_t)) {
			printf("NVMe/TCP: PDU %u too short for digest\n",
			    ch->pdu_type);
			*fes = NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD;
			*fei = offsetof(struct nvme_tcp_common_pdu_hdr, plen);
			return (EBADMSG);
		}
		break;
	case NVME_TCP_PDU_TYPE_R2T:
	case NVME_TCP_PDU_TYPE_CAPSULE_RESP:
		if (data_len != 0) {
			printf("NVMe/TCP: PDU %u with data length %u\n",
			    ch->pdu_type, data_len);
			*fes = NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD;
			*fei = offsetof(struct nvme_tcp_common_pdu_hdr, plen);
			return (EBADMSG);
		}
		break;
	}

	if ((ch->flags & NVME_TCP_CH_FLAGS_DDGSTF) != 0)
		data_len -= sizeof(uint32_t);

	*data_lenp = data_len;
	return (0);
}

#endif /* !__NVMF_TCP_H__ */

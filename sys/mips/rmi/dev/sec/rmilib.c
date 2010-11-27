/*-
 * Copyright (c) 2003-2009 RMI Corporation
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
 * 3. Neither the name of RMI Corporation, nor the names of its contributors,
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 * RMI_BSD
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/md_var.h>
#include <machine/cpuregs.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <opencrypto/cryptodev.h>

#include <mips/rmi/rmi_mips_exts.h>
#include <mips/rmi/iomap.h>
#include <mips/rmi/pic.h>
#include <mips/rmi/rmi_boot_info.h>
#include <mips/rmi/msgring.h>
#include <mips/rmi/board.h>
#include <mips/rmi/dev/sec/rmilib.h>
#include <mips/rmi/dev/sec/desc.h>


/* static int msgrng_stnid_pk0 = MSGRNG_STNID_PK0; */

/* #define RMI_SEC_DEBUG */

#define SMP_CACHE_BYTES XLR_CACHELINE_SIZE
#define NUM_CHUNKS(size, bits) ( ((size)>>(bits)) + (((size)&((1<<(bits))-1))?1:0) )

static const char nib2hex[] = "0123456789ABCDEF";
symkey_desc_pt g_desc;
struct xlr_sec_command *g_cmd;

#ifdef XLR_SEC_CMD_DEBUG
static void decode_symkey_desc(symkey_desc_pt desc, uint32_t cfg_vector);
#endif

static int xlr_sec_cipher_hash_command(xlr_sec_io_pt op, symkey_desc_pt desc,
    uint8_t);
static xlr_sec_error_t xlr_sec_setup_descriptor(xlr_sec_io_pt op,
    unsigned int flags, symkey_desc_pt desc, uint32_t * cfg_vector);

static xlr_sec_error_t xlr_sec_setup_packet(xlr_sec_io_pt op,
    symkey_desc_pt desc, unsigned int flags, uint64_t * data,
    PacketDescriptor_pt pkt_desc, ControlDescriptor_pt ctl_desc,
    uint32_t vector, PacketDescriptor_pt next_pkt_desc,
    uint8_t multi_frag_flag);
static int xlr_sec_submit_message(symkey_desc_pt desc, uint32_t cfg_vector);
static xlr_sec_error_t xlr_sec_setup_cipher(xlr_sec_io_pt op,
    ControlDescriptor_pt ctl_desc, uint32_t * vector);
static xlr_sec_error_t xlr_sec_setup_digest(xlr_sec_io_pt op,
    ControlDescriptor_pt ctl_desc, uint32_t * vector);
static xlr_sec_error_t xlr_sec_setup_cksum(xlr_sec_io_pt op,
    ControlDescriptor_pt ctl_desc);
static xlr_sec_error_t xlr_sec_control_setup(xlr_sec_io_pt op,
    unsigned int flags, uint64_t * control, ControlDescriptor_pt ctl_desc,
    xlr_sec_drv_user_t * user, uint32_t vector);
static void xlr_sec_free_desc(symkey_desc_pt desc); 

void print_buf(char *desc, void *data, int len);
xlr_sec_error_t xlr_sec_submit_op(symkey_desc_pt desc);
void xlr_sec_msgring_handler(int bucket, int size, int code, int stid,
    struct msgrng_msg *msg, void *data);

void 
xlr_sec_init(struct xlr_sec_softc *sc)
{
	unsigned int i;
	xlr_reg_t *mmio;

	mmio = sc->mmio = xlr_io_mmio(XLR_IO_SECURITY_OFFSET);
	xlr_write_reg(mmio, SEC_DMA_CREDIT, SEC_DMA_CREDIT_CONFIG);
	xlr_write_reg(mmio, SEC_CONFIG2, SEC_CFG2_ROUND_ROBIN_ON);

	for (i = 0; i < 8; i++)
		xlr_write_reg(mmio,
		    SEC_MSG_BUCKET0_SIZE + i,
		    xlr_is_xls() ?
		    xls_bucket_sizes.bucket[MSGRNG_STNID_SEC + i] :
		    bucket_sizes.bucket[MSGRNG_STNID_SEC + i]);

	for (i = 0; i < 128; i++)
		xlr_write_reg(mmio,
		    SEC_CC_CPU0_0 + i,
		    xlr_is_xls() ?
		    xls_cc_table_sec.counters[i >> 3][i & 0x07] :
		    cc_table_sec.counters[i >> 3][i & 0x07]);

	/*
	 * Register a bucket handler with the phoenix messaging subsystem
	 * For now, register handler for bucket 0->5 in msg stn 0
	 */
	if (register_msgring_handler(TX_STN_SAE, xlr_sec_msgring_handler, NULL)) {
		panic("Couldn't register msgring handler 0\n");
	}
	return;
}

int 
xlr_sec_setup(struct xlr_sec_session *ses,
    struct xlr_sec_command *cmd,
    symkey_desc_pt desc)
{
	xlr_sec_io_pt op;
	int size, ret_val;
	int iv_len;

	desc->ses = ses;
	op = &cmd->op;
	if (op == NULL)
		return (-ENOMEM);

	desc->ctl_desc.instruction = 0;
	memset(&desc->ctl_desc.cipherHashInfo, 0, sizeof(CipherHashInfo_t));
	desc->control = 0;
	desc->pkt_desc[0].srcLengthIVOffUseIVNext = 0;
	desc->pkt_desc[0].dstDataSettings = 0;
	desc->pkt_desc[0].authDstNonceLow = 0;
	desc->pkt_desc[0].ckSumDstNonceHiCFBMaskLLWMask = 0;
	desc->pkt_desc[1].srcLengthIVOffUseIVNext = 0;
	desc->pkt_desc[1].dstDataSettings = 0;
	desc->pkt_desc[1].authDstNonceLow = 0;
	desc->pkt_desc[1].ckSumDstNonceHiCFBMaskLLWMask = 0;
	desc->data = 0;
	desc->ctl_result = 0;
	desc->data_result = 0;

	if (op->flags & XLR_SEC_FLAGS_HIGH_PRIORITY)
		if (!xlr_is_xls())
			desc->op_ctl.stn_id++;

	desc->user.user_src = (uint8_t *) (unsigned long)op->source_buf;
	desc->user.user_dest = (uint8_t *) (unsigned long)op->dest_buf;
	desc->user.user_auth = (uint8_t *) (unsigned long)op->auth_dest;

	if ((op->cipher_type == XLR_SEC_CIPHER_TYPE_ARC4) &&
	    (!op->rc4_state && (op->rc4_loadstate || op->rc4_savestate))) {
		printf(" ** Load/Save State and no State **");
		xlr_sec_free_desc(desc);
		return (-EINVAL);
	}
	desc->user.user_state = (uint8_t *) (unsigned long)op->rc4_state;

	switch (op->cipher_type) {
	case XLR_SEC_CIPHER_TYPE_NONE:
		iv_len = 0;
		break;
	case XLR_SEC_CIPHER_TYPE_DES:
	case XLR_SEC_CIPHER_TYPE_3DES:
		iv_len = XLR_SEC_DES_IV_LENGTH;
		break;
	case XLR_SEC_CIPHER_TYPE_AES128:
	case XLR_SEC_CIPHER_TYPE_AES192:
	case XLR_SEC_CIPHER_TYPE_AES256:
		iv_len = XLR_SEC_AES_IV_LENGTH;
		break;
	case XLR_SEC_CIPHER_TYPE_ARC4:
		iv_len = XLR_SEC_ARC4_IV_LENGTH;
		break;
	case XLR_SEC_CIPHER_TYPE_KASUMI_F8:
		iv_len = XLR_SEC_KASUMI_F8_IV_LENGTH;
		break;

	default:
		printf(" ** Undefined Cipher Type **");
		xlr_sec_free_desc(desc);
		return (-EINVAL);
	}
	size = op->source_buf_size + iv_len;

	/*
	 * make sure that there are enough bytes for aes based stream
	 * ciphers
	 */
	if (op->cipher_mode == XLR_SEC_CIPHER_MODE_F8 ||
	    op->cipher_mode == XLR_SEC_CIPHER_MODE_CTR)
		size += XLR_SEC_AES_BLOCK_SIZE - 1;

	if (op->cipher_type == XLR_SEC_CIPHER_TYPE_NONE) {
		if (op->source_buf_size != 0) {
			memcpy(desc->user.aligned_src,
			    (uint8_t *)(uintptr_t)op->source_buf,
			    op->source_buf_size);
		}
	} else {
		if (ses->multi_frag_flag) {
			/* copy IV into temporary kernel source buffer */
			memcpy(desc->user.aligned_src, &op->initial_vector[0], iv_len);

			/* copy input data to temporary kernel source buffer */
			memcpy((uint8_t *) (desc->user.aligned_src + iv_len),
			    (uint8_t *) (unsigned long)op->source_buf, SEC_MAX_FRAG_LEN);

			desc->next_src_len = op->source_buf_size - SEC_MAX_FRAG_LEN;
			memcpy((uint8_t *) (desc->next_src_buf),
			    (uint8_t *) (unsigned long)(op->source_buf + SEC_MAX_FRAG_LEN),
			    desc->next_src_len);

			op->source_buf_size = SEC_MAX_FRAG_LEN;
			op->source_buf_size += iv_len;
		} else {
			/* copy IV into temporary kernel source buffer */
			memcpy(desc->user.aligned_src, &op->initial_vector[0], iv_len);

			/* copy input data to temporary kernel source buffer */
			memcpy((uint8_t *) (desc->user.aligned_src + iv_len),
			    (uint8_t *) (unsigned long)op->source_buf, op->source_buf_size);
			op->source_buf_size += iv_len;
		}
	}

	/* Set source to new kernel space */
	op->source_buf = (uint64_t) (unsigned long)desc->user.aligned_src;

	/*
	 * Build new dest buffer, for Cipher output only
	 */
	if (op->cipher_type == XLR_SEC_CIPHER_TYPE_NONE) {
		/*
		 * Digest Engine *NEEDS* this, otherwise it will write at
		 * 0[x]
		 */
		op->dest_buf = (uint64_t) (unsigned long)desc->user.aligned_src;
	} else {
		/* DEBUG -dpk */
		XLR_SEC_CMD_DIAG("dest_buf_size = %d \n", op->dest_buf_size);
		size = op->dest_buf_size + iv_len;

		/*
		 * make sure that there are enough bytes for aes based
		 * stream ciphers
		 */
		if (op->cipher_mode == XLR_SEC_CIPHER_MODE_F8 ||
		    op->cipher_mode == XLR_SEC_CIPHER_MODE_CTR)
			size += XLR_SEC_AES_BLOCK_SIZE - 1;
		op->dest_buf = (uint64_t) (unsigned long)desc->user.aligned_dest;
	}

	ret_val = xlr_sec_cipher_hash_command(op, desc, ses->multi_frag_flag);
	return (ret_val);

}

static int
xlr_sec_cipher_hash_command(xlr_sec_io_pt op, symkey_desc_pt desc,
    uint8_t multi_frag_flag)
{
	xlr_sec_error_t err;
	uint32_t cfg_vector;
	unsigned int setup_flags = 0;

	err = XLR_SEC_ERR_NONE;
	cfg_vector = 0;

	if ((op->digest_type == XLR_SEC_DIGEST_TYPE_NONE) &&
	    (op->cipher_type != XLR_SEC_CIPHER_TYPE_ARC4) &&
	    (op->cipher_mode != XLR_SEC_CIPHER_MODE_F8) &&
	    (op->cipher_type != XLR_SEC_CIPHER_TYPE_KASUMI_F8) &&
	    (op->source_buf_size & 0x7)) {
		printf("Invalid Cipher Block Size, data len=%d\n",
		    op->source_buf_size);
		return (-EINVAL);
	}
	do {

		if ((op->cipher_type == XLR_SEC_CIPHER_TYPE_3DES) &&
		    (op->cipher_op == XLR_SEC_CIPHER_OP_DECRYPT))
			setup_flags = XLR_SEC_SETUP_OP_FLIP_3DES_KEY;

		err = xlr_sec_setup_descriptor(op,
		    setup_flags,
		    desc, &cfg_vector);
		if (err != XLR_SEC_ERR_NONE)
			break;

		err = xlr_sec_setup_packet(op,
		    desc,
		    op->digest_type != XLR_SEC_DIGEST_TYPE_NONE ?
		    XLR_SEC_SETUP_OP_CIPHER_HMAC : 0,
		    &desc->data,
		    &desc->pkt_desc[0],
		    &desc->ctl_desc,
		    cfg_vector,
		    &desc->pkt_desc[1],
		    multi_frag_flag);
		if (err != XLR_SEC_ERR_NONE)
			break;
	} while (0);
	if (err != XLR_SEC_ERR_NONE) {
		return (EINVAL);
	}
	err = xlr_sec_submit_message(desc, cfg_vector);
	return err;
}

static xlr_sec_error_t
xlr_sec_setup_descriptor(xlr_sec_io_pt op,
    unsigned int flags,
    symkey_desc_pt desc,
    uint32_t * cfg_vector)
{
	xlr_sec_error_t err;

	XLR_SEC_CMD_DIAG("xlr_sec_setup_descriptor: ENTER\n");

	if ((err = xlr_sec_setup_cipher(op, &desc->ctl_desc, cfg_vector)) != XLR_SEC_ERR_NONE) {
		XLR_SEC_CMD_DIAG("xlr_sec_setup_descriptor: xlr_sec_setup_cipher done err %d\n",
		    (int)err);
		return err;
	}
	if (op->digest_type != XLR_SEC_DIGEST_TYPE_NONE) {
		if ((err = xlr_sec_setup_digest(op, &desc->ctl_desc, cfg_vector)) != XLR_SEC_ERR_NONE) {
			XLR_SEC_CMD_DIAG("xlr_sec_setup_descriptor: xlr_sec_setup_digest done err %d\n",
			    (int)err);
			return err;
		}
	}
	if ((err = xlr_sec_setup_cksum(op, &desc->ctl_desc)) != XLR_SEC_ERR_NONE) {
		XLR_SEC_CMD_DIAG("xlr_sec_setup_descriptor: xlr_sec_setup_cksum done err %d\n",
		    (int)err);
		return err;
	}
	if ((err = xlr_sec_control_setup(op,
	    flags,
	    &desc->control,
	    &desc->ctl_desc,
	    &desc->user,
	    *cfg_vector)) != XLR_SEC_ERR_NONE) {
		XLR_SEC_CMD_DIAG("xlr_sec_setup_descriptor: xlr_sec_control_setup done err %d\n",
		    (int)err);
		return err;
	}
	XLR_SEC_CMD_DIAG("xlr_sec_setup_descriptor: DONE\n");
	return err;
}



static
xlr_sec_error_t 
xlr_sec_setup_packet(xlr_sec_io_pt op,
    symkey_desc_pt desc,
    unsigned int flags,
    uint64_t * data,
    PacketDescriptor_pt pkt_desc,
    ControlDescriptor_pt ctl_desc,
    uint32_t vector,
    PacketDescriptor_pt next_pkt_desc,
    uint8_t multi_frag_flag)
{
	uint32_t len, next_len = 0, len_dwords, last_u64_bytes;
	uint64_t addr;
	uint64_t seg_addr, next_seg_addr = 0;
	uint64_t byte_offset, global_offset;
	uint32_t cipher_offset_dwords;

	XLR_SEC_CMD_DIAG("xlr_sec_setup_packet: ENTER  vector = %04x\n", vector);

	/* physical address of the source buffer */
	addr = (uint64_t) vtophys((void *)(unsigned long)op->source_buf);
	/* cache-aligned base of the source buffer */
	seg_addr = (addr & ~(SMP_CACHE_BYTES - 1));
	/* offset in bytes to the source buffer start from the segment base */
	byte_offset = addr - seg_addr;
	/* global offset: 0-7 bytes */
	global_offset = byte_offset & 0x7;


	/*
	 * op->source_buf_size is expected to be the Nb double words to
	 * stream in (Including Segment address->CP/IV/Auth/CkSum offsets)
	 */

	/*
	 * adjusted length of the whole thing, accounting for the added
	 * head, sans global_offset (per Paul S.)
	 */

	len = op->source_buf_size + byte_offset - global_offset;
	if (multi_frag_flag) {
		next_seg_addr = (uint64_t)vtophys((void *)(uintptr_t)desc->next_src_buf);
		next_seg_addr = (next_seg_addr & ~(SMP_CACHE_BYTES - 1));
		next_len = desc->next_src_len;
	}
	/* length of the whole thing in dwords */
	len_dwords = NUM_CHUNKS(len, 3);
	/* number of bytes in the last chunk (len % 8) */
	last_u64_bytes = len & 0x07;

	if (op->cipher_offset & 0x7) {
		printf("** cipher_offset(%d) fails 64-bit word alignment **",
		    op->cipher_offset);

		return XLR_SEC_ERR_CIPHER_MODE;	/* ! fix ! */
	}
	/*
	 * global_offset is only three bits, so work the number of the whole
	 * 8-byte words into the global offset. both offset and
	 * cipher_offset are byte counts
	 */
	cipher_offset_dwords = (op->iv_offset + byte_offset) >> 3;

	if (op->cipher_mode == XLR_SEC_CIPHER_MODE_F8 ||
	    op->cipher_mode == XLR_SEC_CIPHER_MODE_CTR) {
		if (multi_frag_flag) {
			int nlhmac = ((op->source_buf_size + global_offset + 7 - op->cipher_offset) >> 3) & 1;

			pkt_desc->srcLengthIVOffUseIVNext =
			    FIELD_VALUE(PKT_DSC_HASHBYTES, len & 7) |
			    FIELD_VALUE(PKT_DSC_IVOFF, cipher_offset_dwords) |
			    FIELD_VALUE(PKT_DSC_PKTLEN, nlhmac + ((len + 7) >> 3)) |
			    FIELD_VALUE(PKT_DSC_NLHMAC, nlhmac) |
			    FIELD_VALUE(PKT_DSC_BREAK, 0) |
			    FIELD_VALUE(PKT_DSC_WAIT, 1) |
			    FIELD_VALUE(PKT_DSC_NEXT, 1) |
			    FIELD_VALUE(PKT_DSC_SEGADDR, seg_addr >> (PKT_DSC_SEGADDR_LSB)) |
			    FIELD_VALUE(PKT_DSC_SEGOFFSET, global_offset);
		} else {
			int nlhmac = ((op->source_buf_size + global_offset + 7 - op->cipher_offset) >> 3) & 1;

			pkt_desc->srcLengthIVOffUseIVNext =
			    FIELD_VALUE(PKT_DSC_HASHBYTES, len & 7) |
			    FIELD_VALUE(PKT_DSC_IVOFF, cipher_offset_dwords) |
			    FIELD_VALUE(PKT_DSC_PKTLEN, nlhmac + ((len + 7) >> 3)) |
			    FIELD_VALUE(PKT_DSC_NLHMAC, nlhmac) |
			    FIELD_VALUE(PKT_DSC_BREAK, 0) |
			    FIELD_VALUE(PKT_DSC_WAIT, 0) |
			    FIELD_VALUE(PKT_DSC_SEGADDR, seg_addr >> (PKT_DSC_SEGADDR_LSB)) |
			    FIELD_VALUE(PKT_DSC_SEGOFFSET, global_offset);

		}
	} else {
		if (multi_frag_flag) {
			pkt_desc->srcLengthIVOffUseIVNext =
			    FIELD_VALUE(PKT_DSC_HASHBYTES, len & 7) |
			    FIELD_VALUE(PKT_DSC_IVOFF, cipher_offset_dwords) |
			    FIELD_VALUE(PKT_DSC_PKTLEN, (len + 7) >> 3) |
			    FIELD_VALUE(PKT_DSC_BREAK, 0) |
			    FIELD_VALUE(PKT_DSC_WAIT, 0) |
			    FIELD_VALUE(PKT_DSC_NEXT, 1) |
			    FIELD_VALUE(PKT_DSC_SEGADDR, seg_addr >> (PKT_DSC_SEGADDR_LSB)) |
			    FIELD_VALUE(PKT_DSC_SEGOFFSET, global_offset);


			next_pkt_desc->srcLengthIVOffUseIVNext =
			    FIELD_VALUE(PKT_DSC_HASHBYTES, (next_len & 7)) |
			    FIELD_VALUE(PKT_DSC_IVOFF, 0) |
			    FIELD_VALUE(PKT_DSC_PKTLEN, (next_len + 7) >> 3) |
			    FIELD_VALUE(PKT_DSC_BREAK, 0) |
			    FIELD_VALUE(PKT_DSC_WAIT, 0) |
			    FIELD_VALUE(PKT_DSC_NEXT, 0) |
			    FIELD_VALUE(PKT_DSC_SEGADDR, next_seg_addr >> (PKT_DSC_SEGADDR_LSB)) |
			    FIELD_VALUE(PKT_DSC_SEGOFFSET, 0);


		} else {
			pkt_desc->srcLengthIVOffUseIVNext =
			    FIELD_VALUE(PKT_DSC_HASHBYTES, len & 7) |
			    FIELD_VALUE(PKT_DSC_IVOFF, cipher_offset_dwords) |
			    FIELD_VALUE(PKT_DSC_PKTLEN, (len + 7) >> 3) |
			    FIELD_VALUE(PKT_DSC_BREAK, 0) |
			    FIELD_VALUE(PKT_DSC_WAIT, 0) |
			    FIELD_VALUE(PKT_DSC_SEGADDR, seg_addr >> (PKT_DSC_SEGADDR_LSB)) |
			    FIELD_VALUE(PKT_DSC_SEGOFFSET, global_offset);


		}
	}

	switch (op->pkt_hmac) {
	case XLR_SEC_LOADHMACKEY_MODE_OLD:
		CLEAR_SET_FIELD(pkt_desc->srcLengthIVOffUseIVNext,
		    PKT_DSC_LOADHMACKEY, PKT_DSC_LOADHMACKEY_OLD);
		if (multi_frag_flag) {
			CLEAR_SET_FIELD(next_pkt_desc->srcLengthIVOffUseIVNext,
			    PKT_DSC_LOADHMACKEY, PKT_DSC_LOADHMACKEY_OLD);

		}
		break;
	case XLR_SEC_LOADHMACKEY_MODE_LOAD:
		CLEAR_SET_FIELD(pkt_desc->srcLengthIVOffUseIVNext,
		    PKT_DSC_LOADHMACKEY, PKT_DSC_LOADHMACKEY_LOAD);
		if (multi_frag_flag) {
			CLEAR_SET_FIELD(next_pkt_desc->srcLengthIVOffUseIVNext,
			    PKT_DSC_LOADHMACKEY, PKT_DSC_LOADHMACKEY_LOAD);

		}
		break;
	default:
		if (vector & (XLR_SEC_VECTOR_HMAC | XLR_SEC_VECTOR_HMAC2 | XLR_SEC_VECTOR_GCM | XLR_SEC_VECTOR_F9)) {
			XLR_SEC_CMD_DIAG("xlr_sec_setup_packet:  ERR_LOADHMACKEY_MODE EXIT\n");
			return XLR_SEC_ERR_LOADHMACKEY_MODE;
		}
		break;
	}

	switch (op->pkt_hash) {
	case XLR_SEC_PADHASH_PADDED:
		CLEAR_SET_FIELD(pkt_desc->srcLengthIVOffUseIVNext,
		    PKT_DSC_PADHASH, PKT_DSC_PADHASH_PADDED);
		if (multi_frag_flag) {
			CLEAR_SET_FIELD(next_pkt_desc->srcLengthIVOffUseIVNext,
			    PKT_DSC_PADHASH, PKT_DSC_PADHASH_PADDED);
		}
		break;
	case XLR_SEC_PADHASH_PAD:
		CLEAR_SET_FIELD(pkt_desc->srcLengthIVOffUseIVNext,
		    PKT_DSC_PADHASH, PKT_DSC_PADHASH_PAD);
		if (multi_frag_flag) {
			CLEAR_SET_FIELD(next_pkt_desc->srcLengthIVOffUseIVNext,
			    PKT_DSC_PADHASH, PKT_DSC_PADHASH_PAD);
		}
		break;
	default:
		if (vector & (XLR_SEC_VECTOR_MAC | XLR_SEC_VECTOR_HMAC | XLR_SEC_VECTOR_HMAC2)) {
			XLR_SEC_CMD_DIAG("xlr_sec_setup_packet:  ERR_PADHASH_MODE EXIT\n");
			return XLR_SEC_ERR_PADHASH_MODE;
		}
		break;
	}

	switch (op->pkt_iv) {
	case XLR_SEC_PKT_IV_OLD:
		CLEAR_SET_FIELD(pkt_desc->srcLengthIVOffUseIVNext,
		    PKT_DSC_IV, PKT_DSC_IV_OLD);
		if (multi_frag_flag) {
			CLEAR_SET_FIELD(next_pkt_desc->srcLengthIVOffUseIVNext,
			    PKT_DSC_IV, PKT_DSC_IV_OLD);

		}
		break;
	case XLR_SEC_PKT_IV_NEW:
		CLEAR_SET_FIELD(pkt_desc->srcLengthIVOffUseIVNext,
		    PKT_DSC_IV, PKT_DSC_IV_NEW);
		if (multi_frag_flag) {
			CLEAR_SET_FIELD(next_pkt_desc->srcLengthIVOffUseIVNext,
			    PKT_DSC_IV, PKT_DSC_IV_NEW);

		}
		break;
	default:
		if (vector & XLR_SEC_VECTOR_CIPHER) {
			XLR_SEC_CMD_DIAG("xlr_sec_setup_packet:  ERR_PKT_IV_MODE EXIT\n");
			return XLR_SEC_ERR_PKT_IV_MODE;
		}
		break;
	}

	XLR_SEC_CMD_DIAG("xlr_sec_setup_packet:  src_buf=%llx  phys_src_buf=%llx \n",
	    (unsigned long long)op->source_buf, (unsigned long long)addr);

	XLR_SEC_CMD_DIAG("xlr_sec_setup_packet:  seg_addr=%llx  offset=%lld\n",
	    (unsigned long long)seg_addr, (unsigned long long)byte_offset);

	XLR_SEC_CMD_DIAG("xlr_sec_setup_packet: global src offset: %d, iv_offset=%d\n",
	    cipher_offset_dwords, op->iv_offset);

	XLR_SEC_CMD_DIAG("xlr_sec_setup_packet: src_buf_sz=%d  PKT_LEN=%d\n",
	    op->source_buf_size, len_dwords);

	/*
	 * same operation with the destination. cipher offset affects this,
	 * as well
	 */
	if (multi_frag_flag) {
		next_seg_addr = (uint64_t) vtophys((void *)(unsigned long)(desc->next_dest_buf));
		next_seg_addr = (next_seg_addr & ~(SMP_CACHE_BYTES - 1));
	}
	addr = (uint64_t) vtophys((void *)(unsigned long)op->dest_buf);
	seg_addr = (addr & ~(SMP_CACHE_BYTES - 1));
	byte_offset = addr - seg_addr;
	global_offset = byte_offset & 0x7;

	XLR_SEC_CMD_DIAG("xlr_sec_setup_packet:  dest_buf=%llx  phys_dest_buf=%llx \n",
	    (unsigned long long)op->dest_buf, (unsigned long long)addr);

	XLR_SEC_CMD_DIAG("xlr_sec_setup_packet:  seg_addr=%llx  offset=%lld\n",
	    (unsigned long long)seg_addr, (unsigned long long)byte_offset);

	/*
	 * Dest Address = (Cipher Dest Address) + (Cipher Offset) + (Global
	 * Dest Data Offset)
	 * 
	 * Cipher Dest Address - Cache-line  (0xffffffffe0) Cipher Offset -
	 * Which (64-bit) Word in Cacheline  (0-3) Global Dest Data Offset -
	 * Number of Bytes in (64-bit) Word before data
	 * 
	 * It must be set for Digest-only Ops, since the Digest engine will
	 * write data to this address.
	 */
	cipher_offset_dwords = (op->cipher_offset + byte_offset) >> 3;


	pkt_desc->dstDataSettings =
	/* SYM_OP, HASHSRC */
	    FIELD_VALUE(PKT_DSC_CPHROFF, cipher_offset_dwords) |
	    FIELD_VALUE(PKT_DSC_HASHOFF, (op->digest_offset + byte_offset) >> 3) |
	    FIELD_VALUE(PKT_DSC_CPHR_DST_ADDR, seg_addr) |
	    FIELD_VALUE(PKT_DSC_CPHR_DST_DWOFFSET, 0) |
	    FIELD_VALUE(PKT_DSC_CPHR_DST_OFFSET, global_offset);

	if (multi_frag_flag) {
		next_pkt_desc->dstDataSettings =
		/* SYM_OP, HASHSRC */
		    FIELD_VALUE(PKT_DSC_CPHROFF, cipher_offset_dwords) |
		    FIELD_VALUE(PKT_DSC_HASHOFF, (op->digest_offset + byte_offset) >> 3) |
		    FIELD_VALUE(PKT_DSC_CPHR_DST_ADDR, next_seg_addr) |
		    FIELD_VALUE(PKT_DSC_CPHR_DST_DWOFFSET, 0) |
		    FIELD_VALUE(PKT_DSC_CPHR_DST_OFFSET, global_offset);

	}
	if (op->cipher_type == XLR_SEC_CIPHER_TYPE_ARC4)
		pkt_desc->dstDataSettings |= FIELD_VALUE(PKT_DSC_ARC4BYTECOUNT, last_u64_bytes);

	if (op->cipher_type != XLR_SEC_CIPHER_TYPE_NONE) {
		switch (op->cipher_op) {
		case XLR_SEC_CIPHER_OP_ENCRYPT:
			CLEAR_SET_FIELD(pkt_desc->dstDataSettings,
			    PKT_DSC_SYM_OP, PKT_DSC_SYM_OP_ENCRYPT);
			if (multi_frag_flag) {
				CLEAR_SET_FIELD(next_pkt_desc->dstDataSettings,
				    PKT_DSC_SYM_OP, PKT_DSC_SYM_OP_ENCRYPT);

			}
			break;
		case XLR_SEC_CIPHER_OP_DECRYPT:
			CLEAR_SET_FIELD(pkt_desc->dstDataSettings,
			    PKT_DSC_SYM_OP, PKT_DSC_SYM_OP_DECRYPT);
			if (multi_frag_flag) {
				CLEAR_SET_FIELD(next_pkt_desc->dstDataSettings,
				    PKT_DSC_SYM_OP, PKT_DSC_SYM_OP_DECRYPT);

			}
			break;
		default:
			XLR_SEC_CMD_DIAG("xlr_sec_setup_packet:  ERR_CIPHER_OP EXIT\n");
			return XLR_SEC_ERR_CIPHER_OP;
		}
	}
	if (flags & XLR_SEC_SETUP_OP_HMAC) {
		switch (op->digest_src) {
		case XLR_SEC_DIGEST_SRC_DMA:
			CLEAR_SET_FIELD(pkt_desc->dstDataSettings,
			    PKT_DSC_HASHSRC, PKT_DSC_HASHSRC_DMA);
			if (multi_frag_flag) {
				CLEAR_SET_FIELD(next_pkt_desc->dstDataSettings,
				    PKT_DSC_HASHSRC, PKT_DSC_HASHSRC_DMA);

			}
			break;
		case XLR_SEC_DIGEST_SRC_CPHR:
			CLEAR_SET_FIELD(pkt_desc->dstDataSettings,
			    PKT_DSC_HASHSRC, PKT_DSC_HASHSRC_CIPHER);
			if (multi_frag_flag) {
				CLEAR_SET_FIELD(next_pkt_desc->dstDataSettings,
				    PKT_DSC_HASHSRC, PKT_DSC_HASHSRC_CIPHER);
			}
			break;
		default:
			XLR_SEC_CMD_DIAG("xlr_sec_setup_packet:  ERR_DIGEST_SRC EXIT\n");
			return XLR_SEC_ERR_DIGEST_SRC;
		}
	}
	if (op->cksum_type != XLR_SEC_CKSUM_TYPE_NOP) {
		switch (op->cksum_src) {
		case XLR_SEC_CKSUM_SRC_DMA:
			CLEAR_SET_FIELD(pkt_desc->dstDataSettings,
			    PKT_DSC_CKSUMSRC, PKT_DSC_CKSUMSRC_DMA);
			if (multi_frag_flag) {
				CLEAR_SET_FIELD(next_pkt_desc->dstDataSettings,
				    PKT_DSC_CKSUMSRC, PKT_DSC_CKSUMSRC_DMA);
			}
			break;
		case XLR_SEC_CKSUM_SRC_CIPHER:
			CLEAR_SET_FIELD(next_pkt_desc->dstDataSettings,
			    PKT_DSC_CKSUMSRC, PKT_DSC_CKSUMSRC_CIPHER);
			if (multi_frag_flag) {
				CLEAR_SET_FIELD(next_pkt_desc->dstDataSettings,
				    PKT_DSC_CKSUMSRC, PKT_DSC_CKSUMSRC_CIPHER);
			}
			break;
		default:
			XLR_SEC_CMD_DIAG("xlr_sec_setup_packet:  ERR_CKSUM_SRC EXIT\n");
			return XLR_SEC_ERR_CKSUM_SRC;
		}
	}
	pkt_desc->ckSumDstNonceHiCFBMaskLLWMask =
	    FIELD_VALUE(PKT_DSC_HASH_BYTE_OFF, (op->digest_offset & 0x7)) |
	    FIELD_VALUE(PKT_DSC_PKTLEN_BYTES, 0) |
	/* NONCE_HI, PKT_DSC_LASTWORD, CFB_MASK, CKSUM_DST_ADDR */
	    FIELD_VALUE(PKT_DSC_IV_OFF_HI, 0);

	if (multi_frag_flag) {
		next_pkt_desc->ckSumDstNonceHiCFBMaskLLWMask =
		    FIELD_VALUE(PKT_DSC_HASH_BYTE_OFF, (op->digest_offset & 0x7)) |
		    FIELD_VALUE(PKT_DSC_PKTLEN_BYTES, 0) |
		/* NONCE_HI, PKT_DSC_LASTWORD, CFB_MASK, CKSUM_DST_ADDR */
		    FIELD_VALUE(PKT_DSC_IV_OFF_HI, 0);

	}
	switch (op->pkt_lastword) {
	case XLR_SEC_LASTWORD_128:
		CLEAR_SET_FIELD(pkt_desc->ckSumDstNonceHiCFBMaskLLWMask,
		    PKT_DSC_LASTWORD, PKT_DSC_LASTWORD_128);
		if (multi_frag_flag) {
			CLEAR_SET_FIELD(next_pkt_desc->ckSumDstNonceHiCFBMaskLLWMask,
			    PKT_DSC_LASTWORD, PKT_DSC_LASTWORD_128);

		}
		break;
	case XLR_SEC_LASTWORD_96MASK:
		CLEAR_SET_FIELD(pkt_desc->ckSumDstNonceHiCFBMaskLLWMask,
		    PKT_DSC_LASTWORD, PKT_DSC_LASTWORD_96MASK);
		if (multi_frag_flag) {
			CLEAR_SET_FIELD(next_pkt_desc->ckSumDstNonceHiCFBMaskLLWMask,
			    PKT_DSC_LASTWORD, PKT_DSC_LASTWORD_96MASK);
		}
		break;
	case XLR_SEC_LASTWORD_64MASK:
		CLEAR_SET_FIELD(pkt_desc->ckSumDstNonceHiCFBMaskLLWMask,
		    PKT_DSC_LASTWORD, PKT_DSC_LASTWORD_64MASK);
		if (multi_frag_flag) {
			CLEAR_SET_FIELD(next_pkt_desc->ckSumDstNonceHiCFBMaskLLWMask,
			    PKT_DSC_LASTWORD, PKT_DSC_LASTWORD_64MASK);
		}
		break;
	case XLR_SEC_LASTWORD_32MASK:
		CLEAR_SET_FIELD(pkt_desc->ckSumDstNonceHiCFBMaskLLWMask,
		    PKT_DSC_LASTWORD, PKT_DSC_LASTWORD_32MASK);
		if (multi_frag_flag) {
			CLEAR_SET_FIELD(next_pkt_desc->ckSumDstNonceHiCFBMaskLLWMask,
			    PKT_DSC_LASTWORD, PKT_DSC_LASTWORD_32MASK);
		}
		break;
	default:
		XLR_SEC_CMD_DIAG("xlr_sec_setup_packet:  ERR_LASTWORD_MODE EXIT\n");
		return XLR_SEC_ERR_LASTWORD_MODE;
	}
	CLEAR_SET_FIELD(pkt_desc->ckSumDstNonceHiCFBMaskLLWMask,
	    PKT_DSC_CFB_MASK, op->cfb_mask);
	CLEAR_SET_FIELD(pkt_desc->ckSumDstNonceHiCFBMaskLLWMask,
	    PKT_DSC_NONCE_HI, htonl(op->nonce) >> 24);
	CLEAR_SET_FIELD(pkt_desc->authDstNonceLow,
	    PKT_DSC_NONCE_LOW, htonl(op->nonce) & 0xffffff);

	if (multi_frag_flag) {
		CLEAR_SET_FIELD(next_pkt_desc->ckSumDstNonceHiCFBMaskLLWMask,
		    PKT_DSC_CFB_MASK, op->cfb_mask);
		CLEAR_SET_FIELD(next_pkt_desc->ckSumDstNonceHiCFBMaskLLWMask,
		    PKT_DSC_NONCE_HI, htonl(op->nonce) >> 24);
		CLEAR_SET_FIELD(next_pkt_desc->authDstNonceLow,
		    PKT_DSC_NONCE_LOW, htonl(op->nonce) & 0xffffff);


	}
	/* Auth Dest Address must be Cacheline aligned on input */
	if (vector & (XLR_SEC_VECTOR_MAC | XLR_SEC_VECTOR_HMAC | XLR_SEC_VECTOR_HMAC2 | XLR_SEC_VECTOR_GCM | XLR_SEC_VECTOR_F9)) {
		pkt_desc->authDstNonceLow |=
		/* NONCE_LOW */
		    FIELD_VALUE(PKT_DSC_AUTH_DST_ADDR,
		    (uint64_t) vtophys((void *)(unsigned long)op->auth_dest)) |
		    FIELD_VALUE(PKT_DSC_CIPH_OFF_HI, 0);


		if (multi_frag_flag) {
			next_pkt_desc->authDstNonceLow |=
			/* NONCE_LOW */
			    FIELD_VALUE(PKT_DSC_AUTH_DST_ADDR,
			    (uint64_t) vtophys((void *)(unsigned long)desc->next_auth_dest)) |
			    FIELD_VALUE(PKT_DSC_CIPH_OFF_HI, 0);


		}
	}
	/* CkSum Dest Address must be Cacheline aligned on input */
	if (op->cksum_type == XLR_SEC_CKSUM_TYPE_IP) {
		CLEAR_SET_FIELD(pkt_desc->ckSumDstNonceHiCFBMaskLLWMask,
		    PKT_DSC_CKSUM_DST_ADDR,
		    (uint64_t) vtophys((void *)(unsigned long)op->cksum_dest));

		if (multi_frag_flag) {
			CLEAR_SET_FIELD(next_pkt_desc->ckSumDstNonceHiCFBMaskLLWMask,
			    PKT_DSC_CKSUM_DST_ADDR,
			    (uint64_t) vtophys((void *)(unsigned long)desc->next_cksum_dest));
		}
	}
	/*
	 * XLR_SEC_CMD_DIAG (" xlr_sec_setup_packet():  pkt_desc=%llx
	 * phys_pkt_desc=%llx \n", (unsigned long long)pkt_desc, (unsigned
	 * long long)virt_to_phys(pkt_desc)); (unsigned long long)pkt_desc,
	 * (unsigned long long)vtophys(pkt_desc));
	 */
	XLR_SEC_CMD_DIAG(" xlr_sec_setup_packet():  pkt_desc=%p   phys_pkt_desc=%llx \n",
	    pkt_desc, (unsigned long long)vtophys(pkt_desc));

	CLEAR_SET_FIELD(*data, MSG_CMD_DATA_ADDR, ((uint64_t) vtophys(pkt_desc)));
	CLEAR_SET_FIELD(*data, MSG_CMD_DATA_CTL, SEC_EOP);
	CLEAR_SET_FIELD(*data, MSG_CMD_DATA_LEN, MSG_CMD_DATA_LEN_LOAD);

	XLR_SEC_CMD_DIAG("xlr_sec_setup_packet:  DONE\n");

#ifdef RMI_SEC_DEBUG
	{
		printf("data desc\n");
		printf("srcLengthIVOffUseIVNext = 0x%llx\n", pkt_desc->srcLengthIVOffUseIVNext);
		printf("dstDataSettings = 0x%llx\n", pkt_desc->dstDataSettings);
		printf("authDstNonceLow = 0x%llx\n", pkt_desc->authDstNonceLow);
		printf("ckSumDstNonceHiCFBMaskLLWMask = 0x%llx\n", pkt_desc->ckSumDstNonceHiCFBMaskLLWMask);
	}

	if (multi_frag_flag) {

		printf("next data desc\n");
		printf("srcLengthIVOffUseIVNext = 0x%llx\n", next_pkt_desc->srcLengthIVOffUseIVNext);
		printf("dstDataSettings = 0x%llx\n", next_pkt_desc->dstDataSettings);
		printf("authDstNonceLow = 0x%llx\n", next_pkt_desc->authDstNonceLow);
		printf("ckSumDstNonceHiCFBMaskLLWMask = 0x%llx\n", next_pkt_desc->ckSumDstNonceHiCFBMaskLLWMask);
	}
#endif

#ifdef SYMBOL
	if (op->cipher_type == XLR_SEC_CIPHER_TYPE_ARC4) {
		op->source_buf -= 0;
		op->source_buf_size += 0;
		op->dest_buf -= 0;
	}
#endif
	return XLR_SEC_ERR_NONE;
}


static int 
identify_symkey_ctl_error(uint32_t code, xlr_sec_error_t err)
{
	int ret_val = EINVAL;

	switch (code) {
	case CTL_ERR_NONE:
		XLR_SEC_CMD_DIAG("XLR_SEC_SEC: CTL Error:  No Error\n");
		ret_val = 0;
		break;
	case CTL_ERR_CIPHER_OP:
		XLR_SEC_CMD_DIAG("XLR_SEC_SEC: CTL Error(CTL_ERR_CIPHER_OP) - Unknown Cipher Op \n");
		break;
	case CTL_ERR_MODE:
		XLR_SEC_CMD_DIAG("XLR_SEC_SEC: CTL Error(CTL_ERR_MODE) - "
		    "Unknown or Not Allowed Mode \n");
		break;
	case CTL_ERR_CHKSUM_SRC:
		XLR_SEC_CMD_DIAG("XLR_SEC_SEC: CTL Error(CTL_ERR_CHKSUM_SRC) - Unknown CkSum Src\n");
		break;
	case CTL_ERR_CFB_MASK:
		XLR_SEC_CMD_DIAG("XLR_SEC_SEC: CTL Error(CTL_ERR_CFB_MASK) - Forbidden CFB Mask \n");
		break;
	case CTL_ERR_OP:
		XLR_SEC_CMD_DIAG("XLR_SEC_SEC: CTL Error(CTL_ERR_OP) - Unknown Ctrl Op \n");
		break;
	case CTL_ERR_DATA_READ:
		XLR_SEC_CMD_DIAG("XLR_SEC_SEC: CTL Error(CTL_ERR_DATA_READ) - Data Read Error\n");
		break;
	case CTL_ERR_DESC_CTRL:
		XLR_SEC_CMD_DIAG("XLR_SEC_SEC: CTL Error(CTL_ERR_DESC_CTRL) - "
		    "Descriptor Ctrl Field Error  \n");
		break;
	case CTL_ERR_UNDEF1:
	case CTL_ERR_UNDEF2:
	default:
		XLR_SEC_CMD_DIAG("XLR_SEC_SEC: CTL Error:  UNKNOWN CODE=%d \n", code);
		break;
	}
	return ret_val;
}

static
int 
identify_symkey_data_error(uint32_t code, xlr_sec_error_t err)
{
	int ret_val = -EINVAL;

	switch (code) {
	case DATA_ERR_NONE:
		XLR_SEC_CMD_DIAG("XLR_SEC_SEC: DATA Error  No Error\n");
		ret_val = 0;
		break;
	case DATA_ERR_LEN_CIPHER:
		XLR_SEC_CMD_DIAG("XLR_SEC_SEC: DATA Error() - Not Enough Data To Cipher\n");
		break;
	case DATA_ERR_IV_ADDR:
		XLR_SEC_CMD_DIAG("XLR_SEC_SEC: DATA Error() - Illegal IV Loacation\n");
		break;
	case DATA_ERR_WD_LEN_AES:
		XLR_SEC_CMD_DIAG("XLR_SEC_SEC: DATA Error() - Illegal Nb Words To AES\n");
		break;
	case DATA_ERR_BYTE_COUNT:
		XLR_SEC_CMD_DIAG("XLR_SEC_SEC: DATA Error() - Illegal Pad And ByteCount Spec\n");
		break;
	case DATA_ERR_LEN_CKSUM:
		XLR_SEC_CMD_DIAG("XLR_SEC_SEC: DATA Error() - Not Enough Data To CkSum\n");
		break;
	case DATA_ERR_OP:
		XLR_SEC_CMD_DIAG("XLR_SEC_SEC: DATA Error() - Unknown Data Op \n");
		break;
	case DATA_ERR_READ:
		XLR_SEC_CMD_DIAG("XLR_SEC_SEC: DATA Error() - Data Read Error \n");
		break;
	case DATA_ERR_WRITE:
		XLR_SEC_CMD_DIAG("XLR_SEC_SEC: DATA Error() - Data Write Error \n");
		break;
	case DATA_ERR_UNDEF1:
	default:
		XLR_SEC_CMD_DIAG("XLR_SEC_SEC: DATA Error - UNKNOWN CODE=%d \n", code);
		break;
	}
	return ret_val;
}


static int
xlr_sec_submit_message(symkey_desc_pt desc, uint32_t cfg_vector)
{
	xlr_sec_error_t err;
	uint32_t ctl_error, data_error;
	int ret_val = 0;

	XLR_SEC_CMD_DIAG("xlr_sec_submit_message:  ENTER\n");
	err = XLR_SEC_ERR_NONE;
	XLR_SEC_CMD_DIAG_SYM_DESC(desc, cfg_vector);

	do {
		/* For now, send message and wait for response */
		err = xlr_sec_submit_op(desc);

		XLR_SEC_CMD_DIAG("xlr_sec_submit_message:  err = %d \n", (uint32_t) err);

		if (err != XLR_SEC_ERR_NONE) {
			ret_val = (EINVAL);
			break;
		}
		ctl_error = desc->ctl_result;
		data_error = desc->data_result;

		XLR_SEC_CMD_DIAG("xlr_sec_submit_message: ctl_error = %x   data_error = %x\n",
		    ctl_error, data_error);

		if ((ret_val = identify_symkey_ctl_error(ctl_error, err)) == 0)
			ret_val = identify_symkey_data_error(data_error, err);

		XLR_SEC_CMD_DIAG("xlr_sec_submit_message: identify error = %d \n", ret_val);

	} while (0);

	XLR_SEC_CMD_DIAG("xlr_sec_submit_message:  DONE\n");
	return (ret_val);
}


static
xlr_sec_error_t 
xlr_sec_setup_cipher(xlr_sec_io_pt op,
    ControlDescriptor_pt ctl_desc,
    uint32_t * vector)
{
	uint32_t aes_flag = 0;
	uint32_t cipher_vector = 0;

	XLR_SEC_CMD_DIAG("xlr_sec_setup_cipher:  ENTER  vector = %04x\n", *vector);

	switch (op->cipher_type) {
	case XLR_SEC_CIPHER_TYPE_NONE:
		SET_FIELD(ctl_desc->instruction, CTL_DSC_CPHR, CTL_DSC_CPHR_BYPASS);
		XLR_SEC_CMD_DIAG("xlr_sec_setup_cipher:  CIPHER_TYPE_NONE EXIT\n");
		return XLR_SEC_ERR_NONE;
	case XLR_SEC_CIPHER_TYPE_DES:
		cipher_vector |= XLR_SEC_VECTOR_CIPHER_DES;
		SET_FIELD(ctl_desc->instruction, CTL_DSC_CPHR, CTL_DSC_CPHR_DES);
		break;
	case XLR_SEC_CIPHER_TYPE_3DES:
		cipher_vector |= XLR_SEC_VECTOR_CIPHER_3DES;
		SET_FIELD(ctl_desc->instruction, CTL_DSC_CPHR, CTL_DSC_CPHR_3DES);
		break;
	case XLR_SEC_CIPHER_TYPE_AES128:
		aes_flag = 1;
		cipher_vector |= XLR_SEC_VECTOR_CIPHER_AES128;
		SET_FIELD(ctl_desc->instruction, CTL_DSC_CPHR, CTL_DSC_CPHR_AES128);
		break;
	case XLR_SEC_CIPHER_TYPE_AES192:
		aes_flag = 1;
		cipher_vector |= XLR_SEC_VECTOR_CIPHER_AES192;
		SET_FIELD(ctl_desc->instruction, CTL_DSC_CPHR, CTL_DSC_CPHR_AES192);
		break;
	case XLR_SEC_CIPHER_TYPE_AES256:
		aes_flag = 1;
		cipher_vector |= XLR_SEC_VECTOR_CIPHER_AES256;
		SET_FIELD(ctl_desc->instruction, CTL_DSC_CPHR, CTL_DSC_CPHR_AES256);
		break;
	case XLR_SEC_CIPHER_TYPE_ARC4:
		cipher_vector |= XLR_SEC_VECTOR_CIPHER_ARC4;
		SET_FIELD(ctl_desc->instruction, CTL_DSC_CPHR, CTL_DSC_CPHR_ARC4);
		SET_FIELD(ctl_desc->instruction, CTL_DSC_ARC4_KEYLEN,
		    op->rc4_key_len);
		SET_FIELD(ctl_desc->instruction, CTL_DSC_ARC4_LOADSTATE,
		    op->rc4_loadstate);
		SET_FIELD(ctl_desc->instruction, CTL_DSC_ARC4_SAVESTATE,
		    op->rc4_savestate);
		if (op->rc4_loadstate || op->rc4_savestate)
			cipher_vector |= XLR_SEC_VECTOR_STATE;
		break;
	case XLR_SEC_CIPHER_TYPE_KASUMI_F8:
		aes_flag = 1;
		cipher_vector |= XLR_SEC_VECTOR_CIPHER_KASUMI_F8;
		SET_FIELD(ctl_desc->instruction, CTL_DSC_CPHR, CTL_DSC_CPHR_KASUMI_F8);
		break;
	default:
		XLR_SEC_CMD_DIAG("xlr_sec_setup_cipher:  ERR_CIPHER_TYPE EXIT\n");
		return XLR_SEC_ERR_CIPHER_TYPE;
	}

	switch (op->cipher_mode) {
	case XLR_SEC_CIPHER_MODE_ECB:
		if (aes_flag == 1)
			cipher_vector |= XLR_SEC_VECTOR_MODE_ECB_CBC_OFB;
		else
			cipher_vector |= XLR_SEC_VECTOR_MODE_ECB_CBC;
		SET_FIELD(ctl_desc->instruction, CTL_DSC_MODE, CTL_DSC_MODE_ECB);
		break;
	case XLR_SEC_CIPHER_MODE_CBC:
		if (aes_flag == 1)
			cipher_vector |= XLR_SEC_VECTOR_MODE_ECB_CBC_OFB;
		else
			cipher_vector |= XLR_SEC_VECTOR_MODE_ECB_CBC;
		SET_FIELD(ctl_desc->instruction, CTL_DSC_MODE, CTL_DSC_MODE_CBC);
		break;
	case XLR_SEC_CIPHER_MODE_OFB:
		if (aes_flag == 0) {
			XLR_SEC_CMD_DIAG("xlr_sec_setup_cipher:  ERR_CIPHER_MODE EXIT\n");
			return XLR_SEC_ERR_CIPHER_MODE;
		}
		cipher_vector |= XLR_SEC_VECTOR_MODE_ECB_CBC_OFB;
		SET_FIELD(ctl_desc->instruction, CTL_DSC_MODE, CTL_DSC_MODE_OFB);
		break;
	case XLR_SEC_CIPHER_MODE_CTR:
		if (aes_flag == 0) {
			XLR_SEC_CMD_DIAG("xlr_sec_setup_cipher:  ERR_CIPHER_MODE EXIT\n");
			return XLR_SEC_ERR_CIPHER_MODE;
		}
		cipher_vector |= XLR_SEC_VECTOR_MODE_CTR_CFB;
		SET_FIELD(ctl_desc->instruction, CTL_DSC_MODE, CTL_DSC_MODE_CTR);
		break;
	case XLR_SEC_CIPHER_MODE_CFB:
		if (aes_flag == 0) {
			XLR_SEC_CMD_DIAG("xlr_sec_setup_cipher:  ERR_CIPHER_MODE EXIT\n");
			return XLR_SEC_ERR_CIPHER_MODE;
		}
		cipher_vector |= XLR_SEC_VECTOR_MODE_CTR_CFB;
		SET_FIELD(ctl_desc->instruction, CTL_DSC_MODE, CTL_DSC_MODE_CFB);
		break;
	case XLR_SEC_CIPHER_MODE_F8:
		if (aes_flag == 0) {
			XLR_SEC_CMD_DIAG("xlr_sec_setup_cipher:  ERR_CIPHER_MODE EXIT\n");
			return XLR_SEC_ERR_CIPHER_MODE;
		}
		cipher_vector |= XLR_SEC_VECTOR_MODE_F8;
		SET_FIELD(ctl_desc->instruction, CTL_DSC_MODE, CTL_DSC_MODE_F8);
		break;
	default:
		if (!(cipher_vector & (XLR_SEC_VECTOR_CIPHER_ARC4 | XLR_SEC_VECTOR_CIPHER_KASUMI_F8))) {
			XLR_SEC_CMD_DIAG("xlr_sec_setup_cipher:  ERR_CIPHER_MODE EXIT\n");
			return XLR_SEC_ERR_CIPHER_MODE;
		}
	}

	switch (op->cipher_init) {
	case XLR_SEC_CIPHER_INIT_OK:
		SET_FIELD(ctl_desc->instruction,
		    CTL_DSC_ICPHR, CTL_DSC_ICPHR_OKY);
		break;

	case XLR_SEC_CIPHER_INIT_NK:
		SET_FIELD(ctl_desc->instruction,
		    CTL_DSC_ICPHR, CTL_DSC_ICPHR_NKY);
		break;
	default:
		XLR_SEC_CMD_DIAG("xlr_sec_setup_cipher:  ERR_CIPHER_INIT EXIT\n");
		return XLR_SEC_ERR_CIPHER_INIT;
	}

	*vector |= cipher_vector;

	XLR_SEC_CMD_DIAG("xlr_sec_setup_cipher:  EXIT  vector = %04x\n", *vector);

	return XLR_SEC_ERR_NONE;
}

static
xlr_sec_error_t 
xlr_sec_setup_digest(xlr_sec_io_pt op,
    ControlDescriptor_pt ctl_desc,
    uint32_t * vector)
{
	uint32_t hash_flag = 0;
	uint32_t hmac_flag = 0;
	uint32_t digest_vector = 0;

	XLR_SEC_CMD_DIAG("xlr_sec_setup_digest:  ENTER  vector = %04x\n", *vector);

	switch (op->digest_type) {
	case XLR_SEC_DIGEST_TYPE_MD5:
		digest_vector |= XLR_SEC_VECTOR_MAC;
		SET_FIELD(ctl_desc->instruction, CTL_DSC_HASH, CTL_DSC_HASH_MD5);
		break;
	case XLR_SEC_DIGEST_TYPE_SHA1:
		digest_vector |= XLR_SEC_VECTOR_MAC;
		SET_FIELD(ctl_desc->instruction, CTL_DSC_HASH, CTL_DSC_HASH_SHA1);
		break;
	case XLR_SEC_DIGEST_TYPE_SHA256:
		digest_vector |= XLR_SEC_VECTOR_MAC;
		SET_FIELD(ctl_desc->instruction, CTL_DSC_HASH, CTL_DSC_HASH_SHA256);
		break;
	case XLR_SEC_DIGEST_TYPE_SHA384:
		digest_vector |= XLR_SEC_VECTOR_MAC;
		SET_FIELD(ctl_desc->instruction, CTL_DSC_HASHHI, CTL_DSC_HASH_SHA384 >> 2);
		SET_FIELD(ctl_desc->instruction, CTL_DSC_HASH, CTL_DSC_HASH_SHA384);
		break;
	case XLR_SEC_DIGEST_TYPE_SHA512:
		digest_vector |= XLR_SEC_VECTOR_MAC;
		SET_FIELD(ctl_desc->instruction, CTL_DSC_HASHHI, CTL_DSC_HASH_SHA512 >> 2);
		SET_FIELD(ctl_desc->instruction, CTL_DSC_HASH, CTL_DSC_HASH_SHA512);
		break;
	case XLR_SEC_DIGEST_TYPE_GCM:
		hash_flag = 1;
		digest_vector |= XLR_SEC_VECTOR_GCM;
		SET_FIELD(ctl_desc->instruction, CTL_DSC_HASHHI, CTL_DSC_HASH_GCM >> 2);
		SET_FIELD(ctl_desc->instruction, CTL_DSC_HASH, CTL_DSC_HASH_GCM);
		break;
	case XLR_SEC_DIGEST_TYPE_KASUMI_F9:
		hash_flag = 1;
		digest_vector |= XLR_SEC_VECTOR_F9;
		SET_FIELD(ctl_desc->instruction, CTL_DSC_HASHHI, CTL_DSC_HASH_KASUMI_F9 >> 2);
		SET_FIELD(ctl_desc->instruction, CTL_DSC_HASH, CTL_DSC_HASH_KASUMI_F9);
		break;
	case XLR_SEC_DIGEST_TYPE_HMAC_MD5:
		hmac_flag = 1;
		digest_vector |= XLR_SEC_VECTOR_HMAC;
		SET_FIELD(ctl_desc->instruction, CTL_DSC_HASH, CTL_DSC_HASH_MD5);
		break;
	case XLR_SEC_DIGEST_TYPE_HMAC_SHA1:
		hmac_flag = 1;
		digest_vector |= XLR_SEC_VECTOR_HMAC;
		SET_FIELD(ctl_desc->instruction, CTL_DSC_HASH, CTL_DSC_HASH_SHA1);
		break;
	case XLR_SEC_DIGEST_TYPE_HMAC_SHA256:
		hmac_flag = 1;
		digest_vector |= XLR_SEC_VECTOR_HMAC;
		SET_FIELD(ctl_desc->instruction, CTL_DSC_HASH, CTL_DSC_HASH_SHA256);
		break;
	case XLR_SEC_DIGEST_TYPE_HMAC_SHA384:
		hmac_flag = 1;
		digest_vector |= XLR_SEC_VECTOR_HMAC2;
		SET_FIELD(ctl_desc->instruction, CTL_DSC_HASHHI, CTL_DSC_HASH_SHA384 >> 2);
		SET_FIELD(ctl_desc->instruction, CTL_DSC_HASH, CTL_DSC_HASH_SHA384);
		break;
	case XLR_SEC_DIGEST_TYPE_HMAC_SHA512:
		hmac_flag = 1;
		digest_vector |= XLR_SEC_VECTOR_HMAC2;
		SET_FIELD(ctl_desc->instruction, CTL_DSC_HASHHI, CTL_DSC_HASH_SHA512 >> 2);
		SET_FIELD(ctl_desc->instruction, CTL_DSC_HASH, CTL_DSC_HASH_SHA512);
		break;
	default:
		return XLR_SEC_ERR_DIGEST_TYPE;
	}

	if (hmac_flag == 1) {
		SET_FIELD(ctl_desc->instruction, CTL_DSC_HMAC, CTL_DSC_HMAC_ON);

	}
	if (hmac_flag || hash_flag) {
		switch (op->digest_init) {
		case XLR_SEC_DIGEST_INIT_OLDKEY:
			SET_FIELD(ctl_desc->instruction, CTL_DSC_IHASH, CTL_DSC_IHASH_OLD);
			break;
		case XLR_SEC_DIGEST_INIT_NEWKEY:
			SET_FIELD(ctl_desc->instruction, CTL_DSC_IHASH, CTL_DSC_IHASH_NEW);
			break;
		default:
			return XLR_SEC_ERR_DIGEST_INIT;
		}
	}			/* hmac_flag */
	*vector |= digest_vector;

	XLR_SEC_CMD_DIAG("xlr_sec_setup_digest: EXIT  vector = %04x\n", *vector);
	return XLR_SEC_ERR_NONE;
}

static
xlr_sec_error_t 
xlr_sec_setup_cksum(xlr_sec_io_pt op,
    ControlDescriptor_pt ctl_desc)
{
	switch (op->cksum_type) {
		case XLR_SEC_CKSUM_TYPE_NOP:
		SET_FIELD(ctl_desc->instruction, CTL_DSC_CKSUM, CTL_DSC_CKSUM_NOP);
		return XLR_SEC_ERR_NONE;
	case XLR_SEC_CKSUM_TYPE_IP:
		SET_FIELD(ctl_desc->instruction, CTL_DSC_CKSUM, CTL_DSC_CKSUM_IP);
		break;
	default:
		return XLR_SEC_ERR_CKSUM_TYPE;
	}

	return XLR_SEC_ERR_NONE;
}


static
xlr_sec_error_t 
xlr_sec_control_setup(xlr_sec_io_pt op,
    unsigned int flags,
    uint64_t * control,
    ControlDescriptor_pt ctl_desc,
    xlr_sec_drv_user_t * user,
    uint32_t vector)
{
	uint64_t *hmac_key = NULL;
	uint64_t *cipher_key = NULL;
	uint64_t *cipher_state = NULL;
	uint32_t ctl_size = 0;
	uint64_t ctl_addr = 0;
	uint32_t cipher_keylen = 0;
	uint32_t hmac_keylen = 0;
	uint32_t ctl_len;

#ifdef SYM_DEBUG
	XLR_SEC_CMD_DIAG(" ENTER  vector = %04x\n", vector);
#endif

	switch (vector) {
	case XLR_SEC_VECTOR_MAC:
		XLR_SEC_CMD_DIAG(" XLR_SEC_VECTOR_MAC \n");
		ctl_size = sizeof(HMAC_t);
		break;
	case XLR_SEC_VECTOR_HMAC:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR_HMAC  \n");
		hmac_key = &ctl_desc->cipherHashInfo.infoHMAC.hmacKey0;
		hmac_keylen = sizeof(HMAC_t);
		ctl_size = sizeof(HMAC_t);
		break;
	case XLR_SEC_VECTOR_CIPHER_ARC4:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR_CIPHER_ARC4\n");
		cipher_key = &ctl_desc->cipherHashInfo.infoARC4.cipherKey0;
		cipher_keylen = op->rc4_key_len;
		ctl_size = sizeof(ARC4_t);
		break;
	case XLR_SEC_VECTOR_CIPHER_ARC4__HMAC:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR_CIPHER_ARC4__HMAC\n");
		cipher_key = &ctl_desc->cipherHashInfo.infoARC4HMAC.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoARC4HMAC.hmacKey0;
		cipher_keylen = op->rc4_key_len;
		hmac_keylen = sizeof(HMAC_t);
		ctl_size = sizeof(ARC4HMAC_t);
		break;
	case XLR_SEC_VECTOR_CIPHER_ARC4__STATE:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR_CIPHER_ARC4__STATE\n");
		cipher_key = &ctl_desc->cipherHashInfo.infoARC4State.cipherKey0;
		cipher_state =
		    &ctl_desc->cipherHashInfo.infoARC4State.Arc4SboxData0;
		cipher_keylen = op->rc4_key_len;
		ctl_size = sizeof(ARC4State_t);
		break;
	case XLR_SEC_VECTOR_CIPHER_ARC4__HMAC__STATE:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR_CIPHER_ARC4__HMAC__STATE\n");
		cipher_key = &ctl_desc->cipherHashInfo.infoARC4StateHMAC.cipherKey0;
		cipher_state =
		    &ctl_desc->cipherHashInfo.infoARC4StateHMAC.Arc4SboxData0;
		hmac_key = &ctl_desc->cipherHashInfo.infoARC4StateHMAC.hmacKey0;
		cipher_keylen = op->rc4_key_len;
		hmac_keylen = sizeof(HMAC_t);
		ctl_size = sizeof(ARC4StateHMAC_t);
		break;
	case XLR_SEC_VECTOR_CIPHER_KASUMI_F8:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR_CIPHER_KASUMI_F8\n");
		cipher_key = &ctl_desc->cipherHashInfo.infoKASUMIF8.cipherKey0;
		cipher_keylen = XLR_SEC_KASUMI_F8_KEY_LENGTH;
		ctl_size = sizeof(KASUMIF8_t);
		break;
	case XLR_SEC_VECTOR_CIPHER_KASUMI_F8__HMAC:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR_CIPHER_KASUMI_F8__HMAC\n");
		cipher_key = &ctl_desc->cipherHashInfo.infoKASUMIF8HMAC.cipherKey0;
		cipher_keylen = XLR_SEC_KASUMI_F8_KEY_LENGTH;
		hmac_key = &ctl_desc->cipherHashInfo.infoKASUMIF8HMAC.hmacKey0;
		hmac_keylen = sizeof(HMAC_t);
		ctl_size = sizeof(KASUMIF8HMAC_t);
		break;
	case XLR_SEC_VECTOR_CIPHER_KASUMI_F8__HMAC2:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR_CIPHER_KASUMI_F8__HMAC2\n");
		cipher_key = &ctl_desc->cipherHashInfo.infoKASUMIF8HMAC2.cipherKey0;
		cipher_keylen = XLR_SEC_KASUMI_F8_KEY_LENGTH;
		hmac_key = &ctl_desc->cipherHashInfo.infoKASUMIF8HMAC2.hmacKey0;
		hmac_keylen = sizeof(HMAC2_t);
		ctl_size = sizeof(KASUMIF8HMAC2_t);
		break;
	case XLR_SEC_VECTOR_CIPHER_KASUMI_F8__GCM:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR_CIPHER_KASUMI_F8__GCM\n");
		cipher_key = &ctl_desc->cipherHashInfo.infoKASUMIF8GCM.cipherKey0;
		cipher_keylen = XLR_SEC_KASUMI_F8_KEY_LENGTH;
		hmac_key = &ctl_desc->cipherHashInfo.infoKASUMIF8GCM.GCMH0;
		hmac_keylen = sizeof(GCM_t);
		ctl_size = sizeof(KASUMIF8GCM_t);
		break;
	case XLR_SEC_VECTOR_CIPHER_KASUMI_F8__F9:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR_CIPHER_KASUMI_F8__F9\n");
		cipher_key = &ctl_desc->cipherHashInfo.infoKASUMIF8F9.cipherKey0;
		cipher_keylen = XLR_SEC_KASUMI_F8_KEY_LENGTH;
		hmac_key = &ctl_desc->cipherHashInfo.infoKASUMIF8F9.authKey0;
		hmac_keylen = sizeof(F9_t);
		ctl_size = sizeof(KASUMIF8F9_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_DES__HMAC__MODE_ECB_CBC:
		XLR_SEC_CMD_DIAG(" XLR_SEC_VECTOR__CIPHER_DES__HMAC__MODE_ECB_CBC  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoDESHMAC.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoDESHMAC.hmacKey0;
		hmac_keylen = sizeof(HMAC_t);
		cipher_keylen = XLR_SEC_DES_KEY_LENGTH;
		ctl_size = sizeof(DESHMAC_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_DES__MODE_ECB_CBC:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_DES__MODE_ECB_CBC  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoDES.cipherKey0;
		cipher_keylen = XLR_SEC_DES_KEY_LENGTH;
		ctl_size = sizeof(DES_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_3DES__HMAC__MODE_ECB_CBC:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_3DES__HMAC__MODE_ECB_CBC  \n");
		cipher_key = &ctl_desc->cipherHashInfo.info3DESHMAC.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.info3DESHMAC.hmacKey0;
		cipher_keylen = XLR_SEC_3DES_KEY_LENGTH;
		hmac_keylen = sizeof(HMAC_t);
		ctl_size = sizeof(DES3HMAC_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_3DES__MODE_ECB_CBC:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_3DES__MODE_ECB_CBC  \n");
		cipher_key = &ctl_desc->cipherHashInfo.info3DES.cipherKey0;
		cipher_keylen = XLR_SEC_3DES_KEY_LENGTH;
		ctl_size = sizeof(DES3_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES128__HMAC__MODE_CTR_CFB:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES128__HMAC__MODE_CTR_CFB  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES128HMAC.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES128HMAC.hmacKey0;
		cipher_keylen = XLR_SEC_AES128_KEY_LENGTH;
		hmac_keylen = sizeof(HMAC_t);
		ctl_size = sizeof(AES128HMAC_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES128__MODE_CTR_CFB:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES128__MODE_CTR_CFB  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES128.cipherKey0;
		cipher_keylen = XLR_SEC_AES128_KEY_LENGTH;
		ctl_size = sizeof(AES128_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES128__HMAC__MODE_ECB_CBC_OFB:
		XLR_SEC_CMD_DIAG(" XLR_SEC_VECTOR__CIPHER_AES128__HMAC__MODE_ECB_CBC_OFB  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES128HMAC.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES128HMAC.hmacKey0;
		cipher_keylen = XLR_SEC_AES128_KEY_LENGTH;
		hmac_keylen = sizeof(HMAC_t);
		ctl_size = sizeof(AES128HMAC_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES128__MODE_ECB_CBC_OFB:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES128__MODE_ECB_CBC_OFB  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES128.cipherKey0;
		cipher_keylen = XLR_SEC_AES128_KEY_LENGTH;
		ctl_size = sizeof(AES128_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES128__HMAC__MODE_F8:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES128__HMAC__MODE_F8  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES128F8HMAC.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES128F8HMAC.hmacKey0;
		cipher_keylen = XLR_SEC_AES128F8_KEY_LENGTH;
		hmac_keylen = sizeof(HMAC_t);
		ctl_size = sizeof(AES128F8HMAC_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES128__MODE_F8:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES128__MODE_F8  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES128F8.cipherKey0;
		cipher_keylen = XLR_SEC_AES128F8_KEY_LENGTH;
		ctl_size = sizeof(AES128F8_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES192__HMAC__MODE_CTR_CFB:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES192__HMAC__MODE_CTR_CFB  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES192HMAC.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES192HMAC.hmacKey0;
		cipher_keylen = XLR_SEC_AES192_KEY_LENGTH;
		hmac_keylen = sizeof(HMAC_t);
		ctl_size = sizeof(AES192HMAC_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES192__MODE_CTR_CFB:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES192__MODE_CTR_CFB  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES192.cipherKey0;
		cipher_keylen = XLR_SEC_AES192_KEY_LENGTH;
		ctl_size = sizeof(AES192_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES192__HMAC__MODE_ECB_CBC_OFB:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES192__HMAC__MODE_ECB_CBC_OFB  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES192HMAC.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES192HMAC.hmacKey0;
		cipher_keylen = XLR_SEC_AES192_KEY_LENGTH;
		hmac_keylen = sizeof(HMAC_t);
		ctl_size = sizeof(AES192HMAC_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES192__MODE_ECB_CBC_OFB:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES192__MODE_ECB_CBC_OFB  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES192.cipherKey0;
		cipher_keylen = XLR_SEC_AES192_KEY_LENGTH;
		ctl_size = sizeof(AES192_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES192__HMAC__MODE_F8:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES192__HMAC__MODE_F8  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES192F8HMAC.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES192F8HMAC.hmacKey0;
		cipher_keylen = XLR_SEC_AES192F8_KEY_LENGTH;
		hmac_keylen = sizeof(HMAC_t);
		ctl_size = sizeof(AES192F8HMAC_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES192__MODE_F8:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES192__MODE_F8  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES192F8.cipherKey0;
		cipher_keylen = XLR_SEC_AES192F8_KEY_LENGTH;
		ctl_size = sizeof(AES192F8_t);
		break;

	case XLR_SEC_VECTOR__CIPHER_AES256__HMAC__MODE_CTR_CFB:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES256__HMAC__MODE_CTR_CFB  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES256HMAC.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES256HMAC.hmacKey0;
		cipher_keylen = XLR_SEC_AES256_KEY_LENGTH;
		hmac_keylen = sizeof(HMAC_t);
		ctl_size = sizeof(AES256HMAC_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES256__MODE_CTR_CFB:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES256__MODE_CTR_CFB  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES256.cipherKey0;
		cipher_keylen = XLR_SEC_AES256_KEY_LENGTH;
		ctl_size = sizeof(AES256_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES256__HMAC__MODE_ECB_CBC_OFB:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES256__HMAC__MODE_ECB_CBC_OFB  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES256HMAC.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES256HMAC.hmacKey0;
		cipher_keylen = XLR_SEC_AES256_KEY_LENGTH;
		hmac_keylen = sizeof(HMAC_t);
		ctl_size = sizeof(AES256HMAC_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES256__MODE_ECB_CBC_OFB:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES256__MODE_ECB_CBC_OFB  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES256.cipherKey0;
		cipher_keylen = XLR_SEC_AES256_KEY_LENGTH;
		ctl_size = sizeof(AES256_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES256__HMAC__MODE_F8:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES256__HMAC__MODE_F8  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES256F8HMAC.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES256F8HMAC.hmacKey0;
		cipher_keylen = XLR_SEC_AES256F8_KEY_LENGTH;
		hmac_keylen = sizeof(HMAC_t);
		ctl_size = sizeof(AES256F8HMAC_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES256__MODE_F8:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES256__MODE_F8  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES256F8.cipherKey0;
		cipher_keylen = XLR_SEC_AES256F8_KEY_LENGTH;
		ctl_size = sizeof(AES256F8_t);
		break;
	case XLR_SEC_VECTOR_HMAC2:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR_HMAC2  \n");
		hmac_key = &ctl_desc->cipherHashInfo.infoHMAC2.hmacKey0;
		hmac_keylen = sizeof(HMAC2_t);
		ctl_size = sizeof(HMAC2_t);
		break;
	case XLR_SEC_VECTOR_CIPHER_ARC4__HMAC2:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR_CIPHER_ARC4__HMAC2\n");
		cipher_key = &ctl_desc->cipherHashInfo.infoARC4HMAC2.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoARC4HMAC2.hmacKey0;
		cipher_keylen = op->rc4_key_len;
		hmac_keylen = sizeof(HMAC2_t);
		ctl_size = sizeof(ARC4HMAC2_t);
		break;
	case XLR_SEC_VECTOR_CIPHER_ARC4__HMAC2__STATE:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR_CIPHER_ARC4__HMAC2__STATE\n");
		cipher_key = &ctl_desc->cipherHashInfo.infoARC4StateHMAC2.cipherKey0;
		cipher_state =
		    &ctl_desc->cipherHashInfo.infoARC4StateHMAC2.Arc4SboxData0;
		hmac_key = &ctl_desc->cipherHashInfo.infoARC4StateHMAC2.hmacKey0;
		cipher_keylen = op->rc4_key_len;
		hmac_keylen = sizeof(HMAC2_t);
		ctl_size = sizeof(ARC4StateHMAC2_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_DES__HMAC2__MODE_ECB_CBC:
		XLR_SEC_CMD_DIAG(" XLR_SEC_VECTOR__CIPHER_DES__HMAC2__MODE_ECB_CBC  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoDESHMAC2.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoDESHMAC2.hmacKey0;
		hmac_keylen = sizeof(HMAC2_t);
		cipher_keylen = XLR_SEC_DES_KEY_LENGTH;
		ctl_size = sizeof(DESHMAC2_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_3DES__HMAC2__MODE_ECB_CBC:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_3DES__HMAC2__MODE_ECB_CBC  \n");
		cipher_key = &ctl_desc->cipherHashInfo.info3DESHMAC2.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.info3DESHMAC2.hmacKey0;
		cipher_keylen = XLR_SEC_3DES_KEY_LENGTH;
		hmac_keylen = sizeof(HMAC2_t);
		ctl_size = sizeof(DES3HMAC2_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES128__HMAC2__MODE_CTR_CFB:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES128__HMAC2__MODE_CTR_CFB  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES128HMAC2.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES128HMAC2.hmacKey0;
		cipher_keylen = XLR_SEC_AES128_KEY_LENGTH;
		hmac_keylen = sizeof(HMAC2_t);
		ctl_size = sizeof(AES128HMAC2_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES128__HMAC2__MODE_ECB_CBC_OFB:
		XLR_SEC_CMD_DIAG(" XLR_SEC_VECTOR__CIPHER_AES128__HMAC2__MODE_ECB_CBC_OFB  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES128HMAC2.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES128HMAC2.hmacKey0;
		cipher_keylen = XLR_SEC_AES128_KEY_LENGTH;
		hmac_keylen = sizeof(HMAC2_t);
		ctl_size = sizeof(AES128HMAC2_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES128__HMAC2__MODE_F8:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES128__HMAC2__MODE_F8  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES128F8HMAC2.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES128F8HMAC2.hmacKey0;
		cipher_keylen = XLR_SEC_AES128F8_KEY_LENGTH;
		hmac_keylen = sizeof(HMAC2_t);
		ctl_size = sizeof(AES128F8HMAC2_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES192__HMAC2__MODE_CTR_CFB:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES192__HMAC2__MODE_CTR_CFB  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES192HMAC2.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES192HMAC2.hmacKey0;
		cipher_keylen = XLR_SEC_AES192_KEY_LENGTH;
		hmac_keylen = sizeof(HMAC2_t);
		ctl_size = sizeof(AES192HMAC2_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES192__HMAC2__MODE_ECB_CBC_OFB:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES192__HMAC2__MODE_ECB_CBC_OFB  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES192HMAC2.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES192HMAC2.hmacKey0;
		cipher_keylen = XLR_SEC_AES192_KEY_LENGTH;
		hmac_keylen = sizeof(HMAC2_t);
		ctl_size = sizeof(AES192HMAC2_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES192__HMAC2__MODE_F8:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES192__HMAC2__MODE_F8  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES192F8HMAC2.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES192F8HMAC2.hmacKey0;
		cipher_keylen = XLR_SEC_AES192F8_KEY_LENGTH;
		hmac_keylen = sizeof(HMAC2_t);
		ctl_size = sizeof(AES192F8HMAC2_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES256__HMAC2__MODE_CTR_CFB:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES256__HMAC2__MODE_CTR_CFB  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES256HMAC2.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES256HMAC2.hmacKey0;
		cipher_keylen = XLR_SEC_AES256_KEY_LENGTH;
		hmac_keylen = sizeof(HMAC2_t);
		ctl_size = sizeof(AES256HMAC2_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES256__HMAC2__MODE_ECB_CBC_OFB:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES256__HMAC2__MODE_ECB_CBC_OFB  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES256HMAC2.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES256HMAC2.hmacKey0;
		cipher_keylen = XLR_SEC_AES256_KEY_LENGTH;
		hmac_keylen = sizeof(HMAC2_t);
		ctl_size = sizeof(AES256HMAC2_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES256__HMAC2__MODE_F8:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES256__HMAC2__MODE_F8  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES256F8HMAC2.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES256F8HMAC2.hmacKey0;
		cipher_keylen = XLR_SEC_AES256F8_KEY_LENGTH;
		hmac_keylen = sizeof(HMAC2_t);
		ctl_size = sizeof(AES256F8HMAC2_t);
		break;
	case XLR_SEC_VECTOR_GCM:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR_GCM  \n");
		hmac_key = &ctl_desc->cipherHashInfo.infoGCM.GCMH0;
		hmac_keylen = sizeof(GCM_t);
		ctl_size = sizeof(GCM_t);
		break;
	case XLR_SEC_VECTOR_CIPHER_ARC4__GCM:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR_CIPHER_ARC4__GCM\n");
		cipher_key = &ctl_desc->cipherHashInfo.infoARC4GCM.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoARC4GCM.GCMH0;
		cipher_keylen = op->rc4_key_len;
		hmac_keylen = sizeof(GCM_t);
		ctl_size = sizeof(ARC4GCM_t);
		break;
	case XLR_SEC_VECTOR_CIPHER_ARC4__GCM__STATE:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR_CIPHER_ARC4__GCM__STATE\n");
		cipher_key = &ctl_desc->cipherHashInfo.infoARC4StateGCM.cipherKey0;
		cipher_state =
		    &ctl_desc->cipherHashInfo.infoARC4StateGCM.Arc4SboxData0;
		hmac_key = &ctl_desc->cipherHashInfo.infoARC4StateGCM.GCMH0;
		cipher_keylen = op->rc4_key_len;
		hmac_keylen = sizeof(GCM_t);
		ctl_size = sizeof(ARC4StateGCM_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_DES__GCM__MODE_ECB_CBC:
		XLR_SEC_CMD_DIAG(" XLR_SEC_VECTOR__CIPHER_DES__GCM__MODE_ECB_CBC  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoDESGCM.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoDESGCM.GCMH0;
		hmac_keylen = sizeof(GCM_t);
		cipher_keylen = XLR_SEC_DES_KEY_LENGTH;
		ctl_size = sizeof(DESGCM_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_3DES__GCM__MODE_ECB_CBC:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_3DES__GCM__MODE_ECB_CBC  \n");
		cipher_key = &ctl_desc->cipherHashInfo.info3DESGCM.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.info3DESGCM.GCMH0;
		cipher_keylen = XLR_SEC_3DES_KEY_LENGTH;
		hmac_keylen = sizeof(GCM_t);
		ctl_size = sizeof(DES3GCM_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES128__GCM__MODE_CTR_CFB:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES128__GCM__MODE_CTR_CFB  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES128GCM.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES128GCM.GCMH0;
		cipher_keylen = XLR_SEC_AES128_KEY_LENGTH;
		hmac_keylen = sizeof(GCM_t);
		ctl_size = sizeof(AES128GCM_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES128__GCM__MODE_ECB_CBC_OFB:
		XLR_SEC_CMD_DIAG(" XLR_SEC_VECTOR__CIPHER_AES128__GCM__MODE_ECB_CBC_OFB  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES128GCM.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES128GCM.GCMH0;
		cipher_keylen = XLR_SEC_AES128_KEY_LENGTH;
		hmac_keylen = sizeof(GCM_t);
		ctl_size = sizeof(AES128GCM_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES128__GCM__MODE_F8:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES128__GCM__MODE_F8  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES128F8GCM.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES128F8GCM.GCMH0;
		cipher_keylen = XLR_SEC_AES128F8_KEY_LENGTH;
		hmac_keylen = sizeof(GCM_t);
		ctl_size = sizeof(AES128F8GCM_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES192__GCM__MODE_CTR_CFB:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES192__GCM__MODE_CTR_CFB  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES192GCM.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES192GCM.GCMH0;
		cipher_keylen = XLR_SEC_AES192_KEY_LENGTH;
		hmac_keylen = sizeof(GCM_t);
		ctl_size = sizeof(AES192GCM_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES192__GCM__MODE_ECB_CBC_OFB:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES192__GCM__MODE_ECB_CBC_OFB  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES192GCM.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES192GCM.GCMH0;
		cipher_keylen = XLR_SEC_AES192_KEY_LENGTH;
		hmac_keylen = sizeof(GCM_t);
		ctl_size = sizeof(AES192GCM_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES192__GCM__MODE_F8:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES192__GCM__MODE_F8  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES192F8GCM.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES192F8GCM.GCMH0;
		cipher_keylen = XLR_SEC_AES192F8_KEY_LENGTH;
		hmac_keylen = sizeof(GCM_t);
		ctl_size = sizeof(AES192F8GCM_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES256__GCM__MODE_CTR_CFB:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES256__GCM__MODE_CTR_CFB  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES256GCM.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES256GCM.GCMH0;
		cipher_keylen = XLR_SEC_AES256_KEY_LENGTH;
		hmac_keylen = sizeof(GCM_t);
		ctl_size = sizeof(AES256GCM_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES256__GCM__MODE_ECB_CBC_OFB:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES256__GCM__MODE_ECB_CBC_OFB  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES256GCM.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES256GCM.GCMH0;
		cipher_keylen = XLR_SEC_AES256_KEY_LENGTH;
		hmac_keylen = sizeof(GCM_t);
		ctl_size = sizeof(AES256GCM_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES256__GCM__MODE_F8:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES256__GCM__MODE_F8  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES256F8GCM.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES256F8GCM.GCMH0;
		cipher_keylen = XLR_SEC_AES256F8_KEY_LENGTH;
		hmac_keylen = sizeof(GCM_t);
		ctl_size = sizeof(AES256F8GCM_t);
		break;
	case XLR_SEC_VECTOR_F9:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR_F9  \n");
		hmac_key = &ctl_desc->cipherHashInfo.infoF9.authKey0;
		hmac_keylen = sizeof(F9_t);
		ctl_size = sizeof(F9_t);
		break;
	case XLR_SEC_VECTOR_CIPHER_ARC4__F9:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR_CIPHER_ARC4__F9\n");
		cipher_key = &ctl_desc->cipherHashInfo.infoARC4F9.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoARC4F9.authKey0;
		cipher_keylen = op->rc4_key_len;
		hmac_keylen = sizeof(F9_t);
		ctl_size = sizeof(ARC4F9_t);
		break;
	case XLR_SEC_VECTOR_CIPHER_ARC4__F9__STATE:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR_CIPHER_ARC4__F9__STATE\n");
		cipher_key = &ctl_desc->cipherHashInfo.infoARC4StateF9.cipherKey0;
		cipher_state =
		    &ctl_desc->cipherHashInfo.infoARC4StateF9.Arc4SboxData0;
		hmac_key = &ctl_desc->cipherHashInfo.infoARC4StateF9.authKey0;
		cipher_keylen = op->rc4_key_len;
		hmac_keylen = sizeof(F9_t);
		ctl_size = sizeof(ARC4StateF9_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_DES__F9__MODE_ECB_CBC:
		XLR_SEC_CMD_DIAG(" XLR_SEC_VECTOR__CIPHER_DES__F9__MODE_ECB_CBC  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoDESF9.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoDESF9.authKey0;
		hmac_keylen = sizeof(F9_t);
		cipher_keylen = XLR_SEC_DES_KEY_LENGTH;
		ctl_size = sizeof(DESF9_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_3DES__F9__MODE_ECB_CBC:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_3DES__F9__MODE_ECB_CBC  \n");
		cipher_key = &ctl_desc->cipherHashInfo.info3DESF9.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.info3DESF9.authKey0;
		cipher_keylen = XLR_SEC_3DES_KEY_LENGTH;
		hmac_keylen = sizeof(F9_t);
		ctl_size = sizeof(DES3F9_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES128__F9__MODE_CTR_CFB:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES128__F9__MODE_CTR_CFB  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES128F9.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES128F9.authKey0;
		cipher_keylen = XLR_SEC_AES128_KEY_LENGTH;
		hmac_keylen = sizeof(F9_t);
		ctl_size = sizeof(AES128F9_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES128__F9__MODE_ECB_CBC_OFB:
		XLR_SEC_CMD_DIAG(" XLR_SEC_VECTOR__CIPHER_AES128__F9__MODE_ECB_CBC_OFB  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES128F9.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES128F9.authKey0;
		cipher_keylen = XLR_SEC_AES128_KEY_LENGTH;
		hmac_keylen = sizeof(F9_t);
		ctl_size = sizeof(AES128F9_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES128__F9__MODE_F8:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES128__F9__MODE_F8  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES128F8F9.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES128F8F9.authKey0;
		cipher_keylen = XLR_SEC_AES128F8_KEY_LENGTH;
		hmac_keylen = sizeof(F9_t);
		ctl_size = sizeof(AES128F8F9_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES192__F9__MODE_CTR_CFB:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES192__F9__MODE_CTR_CFB  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES192F9.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES192F9.authKey0;
		cipher_keylen = XLR_SEC_AES192_KEY_LENGTH;
		hmac_keylen = sizeof(F9_t);
		ctl_size = sizeof(AES192F9_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES192__F9__MODE_ECB_CBC_OFB:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES192__F9__MODE_ECB_CBC_OFB  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES192F9.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES192F9.authKey0;
		cipher_keylen = XLR_SEC_AES192_KEY_LENGTH;
		hmac_keylen = sizeof(F9_t);
		ctl_size = sizeof(AES192F9_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES192__F9__MODE_F8:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES192__F9__MODE_F8  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES192F8F9.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES192F8F9.authKey0;
		cipher_keylen = XLR_SEC_AES192F8_KEY_LENGTH;
		hmac_keylen = sizeof(F9_t);
		ctl_size = sizeof(AES192F8F9_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES256__F9__MODE_CTR_CFB:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES256__F9__MODE_CTR_CFB  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES256F9.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES256F9.authKey0;
		cipher_keylen = XLR_SEC_AES256_KEY_LENGTH;
		hmac_keylen = sizeof(F9_t);
		ctl_size = sizeof(AES256F9_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES256__F9__MODE_ECB_CBC_OFB:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES256__F9__MODE_ECB_CBC_OFB  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES256F9.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES256F9.authKey0;
		cipher_keylen = XLR_SEC_AES256_KEY_LENGTH;
		hmac_keylen = sizeof(F9_t);
		ctl_size = sizeof(AES256F9_t);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES256__F9__MODE_F8:
		XLR_SEC_CMD_DIAG("XLR_SEC_VECTOR__CIPHER_AES256__F9__MODE_F8  \n");
		cipher_key = &ctl_desc->cipherHashInfo.infoAES256F8F9.cipherKey0;
		hmac_key = &ctl_desc->cipherHashInfo.infoAES256F8F9.authKey0;
		cipher_keylen = XLR_SEC_AES256F8_KEY_LENGTH;
		hmac_keylen = sizeof(F9_t);
		ctl_size = sizeof(AES256F8F9_t);
		break;

	default:
		XLR_SEC_CMD_DIAG("default  \n");
		return XLR_SEC_ERR_CONTROL_VECTOR;
	}

	if ((cipher_key != NULL) && !(flags & XLR_SEC_SETUP_OP_PRESERVE_CIPHER_KEY))
		memcpy(cipher_key, &op->crypt_key[0], cipher_keylen);

	if ((hmac_key != NULL) && !(flags & XLR_SEC_SETUP_OP_PRESERVE_HMAC_KEY))
		memcpy(hmac_key, &op->mac_key[0], hmac_keylen);
	if (cipher_state) {
		if (op->rc4_loadstate)
			memcpy(cipher_state, (void *)(unsigned long)op->rc4_state,
			    XLR_SEC_MAX_RC4_STATE_SIZE);
		if (op->rc4_savestate)
			user->aligned_state = (char *)cipher_state;
	}
	if (flags & XLR_SEC_SETUP_OP_FLIP_3DES_KEY) {
		uint64_t temp;

		temp = ctl_desc->cipherHashInfo.info3DES.cipherKey0;
		ctl_desc->cipherHashInfo.info3DES.cipherKey0 =
		    ctl_desc->cipherHashInfo.info3DES.cipherKey2;
		ctl_desc->cipherHashInfo.info3DES.cipherKey2 = temp;
	}
	/*
	 * Control length is the number of control cachelines to be read so
	 * user needs to round up the control length to closest integer
	 * multiple of 32 bytes.
	 */
	ctl_size += sizeof(ctl_desc->instruction);
	ctl_len = NUM_CHUNKS(ctl_size, 5);
	XLR_SEC_CMD_DIAG("ctl_size in bytes: %u, in cachelines: %u\n", ctl_size, ctl_len);
	CLEAR_SET_FIELD(*control, MSG_CMD_CTL_LEN, ctl_len);

	ctl_addr = (uint64_t) vtophys(ctl_desc);
	CLEAR_SET_FIELD(*control, MSG_CMD_CTL_ADDR, ctl_addr);

	XLR_SEC_CMD_DIAG(" xlr_sec_control_setup():  ctl_desc=%p   ctl_addr=%llx \n",
	    ctl_desc, (unsigned long long)ctl_addr);

	CLEAR_SET_FIELD(*control, MSG_CMD_CTL_CTL, SEC_SOP);

	return XLR_SEC_ERR_NONE;
}

xlr_sec_error_t
xlr_sec_submit_op(symkey_desc_pt desc)
{
	struct msgrng_msg send_msg;

	int rsp_dest_id, cpu, hard_cpu, hard_thread;
	int code, retries;
	unsigned long msgrng_flags = 0;

	/* threads (0-3) are orthogonal to buckets 0-3 */
	cpu = xlr_cpu_id();

	hard_cpu = cpu >> 2;
	hard_thread = cpu & 0x3;/* thread id */
	rsp_dest_id = (hard_cpu << 3) + hard_thread;

	desc->op_ctl.cpu = hard_cpu;
	desc->op_ctl.flags = 0;	/* called from kernel thread */

	XLR_SEC_CMD_DIAG("[%s]:%d:  cpu=0x%x  hard_cpu=0x%x  hard_thrd=0x%x  id=0x%x \n",
	    __FUNCTION__, __LINE__, cpu, hard_cpu, hard_thread, rsp_dest_id);

	/*
	 * Set DestId in Message Control Word. This tells the Security
	 * Engine which bucket to send the reply to for this CPU
	 */
	CLEAR_SET_FIELD(desc->control, MSG_CMD_CTL_ID, rsp_dest_id);
	CLEAR_SET_FIELD(desc->data, MSG_CMD_CTL_ID, rsp_dest_id);

	CLEAR_SET_FIELD(desc->control, MSG_CTL_OP_TYPE, MSG0_CTL_OP_ENGINE_SYMKEY);
	CLEAR_SET_FIELD(desc->data, MSG_CTL_OP_TYPE, MSG1_CTL_OP_SYMKEY_PIPE0);

	send_msg.msg0 = desc->control | (1ULL << 53);
	send_msg.msg1 = desc->data | (1ULL << 53) | (1ULL << 52);
	send_msg.msg2 = send_msg.msg3 = 0;

	desc->op_ctl.flags = 1;
	//in_interrupt();	/* ipsec softirq ? */

	XLR_SEC_CMD_DIAG("[%s]: IN_IRQ=%d  msg0=0x%llx  msg1=0x%llx \n",
	    __FUNCTION__, desc->op_ctl.flags, send_msg.msg0, send_msg.msg1);

	retries = 100;
	while (retries--) {
		msgrng_flags = msgrng_access_enable();
		code = message_send(SEC_MSGRING_WORDSIZE, MSGRNG_CODE_SEC,
		    desc->op_ctl.stn_id, &send_msg);
		msgrng_restore(msgrng_flags);
		if (code == 0)
			break;
	}
	return (XLR_SEC_ERR_NONE);
}

symkey_desc_pt 
xlr_sec_allocate_desc(void *session_ptr)
{
	uint64_t addr;
	symkey_desc_pt aligned, new;

	new = (symkey_desc_pt) malloc(sizeof(symkey_desc_t),
	    M_DEVBUF, M_NOWAIT | M_ZERO);

	if (new == NULL)
		return (NULL);

	new->ses = session_ptr;

	new->user.kern_src = new->user.aligned_src =
	    (uint8_t *) contigmalloc(256 * 1024 + 1024,
	    M_DEVBUF, M_NOWAIT | M_ZERO,
	    0, 0xffffffff, XLR_CACHELINE_SIZE, 0);

	if (new->user.kern_src == NULL) {
		printf("ERROR - malloc failed for user.kern_src\n");
		return NULL;
	}
	new->user.aligned_dest = new->user.kern_dest =
	    (uint8_t *) contigmalloc(257 * 1024,
	    M_DEVBUF, M_NOWAIT | M_ZERO,
	    0, 0xffffffff, XLR_CACHELINE_SIZE, 0);

	if (new->user.aligned_dest == NULL) {
		printf("ERROR - malloc failed for user.aligned_dest\n");
		return NULL;
	}
	new->next_src_buf = (uint8_t *) contigmalloc(256 * 1024 + 1024,
	    M_DEVBUF, M_NOWAIT | M_ZERO,
	    0, 0xffffffff, XLR_CACHELINE_SIZE, 0);

	if (new->next_src_buf == NULL) {
		printf("ERROR - malloc failed for next_src_buf\n");
		return NULL;
	}
	new->next_dest_buf =
	    (uint8_t *) contigmalloc(257 * 1024,
	    M_DEVBUF, M_NOWAIT | M_ZERO,
	    0, 0xffffffff, XLR_CACHELINE_SIZE, 0);

	if (new->next_dest_buf == NULL) {
		printf("ERROR - malloc failed for next_dest_buf\n");
		return NULL;
	}
	new->user.kern_auth = new->user.user_auth = NULL;
	new->user.aligned_auth = new->user.user_auth = NULL;

	/* find cacheline alignment */
	aligned = new;
	addr = (uint64_t) vtophys(new);

	/* save for free */
	aligned->alloc = new;

	/* setup common control info */
	aligned->op_ctl.phys_self = addr;
	aligned->op_ctl.stn_id = MSGRNG_STNID_SEC0;
	aligned->op_ctl.vaddr = (uintptr_t)aligned;

	return (aligned);
}


static void 
xlr_sec_free_desc(symkey_desc_pt desc)
{
	if ((desc == NULL) || (desc->alloc == NULL)) {
		printf("%s:  NULL descriptor \n", __FUNCTION__);
		return;
	}
	contigfree(desc, sizeof(symkey_desc_t), M_DEVBUF);
	return;
}

void 
print_buf(char *desc, void *data, int len)
{
	uint8_t *dp;
	int i;

	DPRINT("%s: ", desc);	/* newline done in for-loop */
	dp = data;
	for (i = 0; i < len; i++, dp++) {
		if ((i % 16) == 0)
			DPRINT("\n");
		DPRINT(" %c%c",
		    nib2hex[(((*dp) & 0xf0) >> 4)],
		    nib2hex[((*dp) & 0x0f)]);
	}
	DPRINT("\n");
}


#ifdef XLR_SEC_CMD_DEBUG
static void
decode_symkey_desc(symkey_desc_pt desc, uint32_t cfg_vector)
{

	unsigned long long word;

	/* uint8_t   *info; */
	/* int i; */

	DPRINT("MSG - CTL: \n");
	DPRINT("\t CTRL      = %lld \n",
	    GET_FIELD(desc->control, MSG_CMD_CTL_CTL));
	DPRINT("\t CTRL LEN  = %lld \n",
	    GET_FIELD(desc->control, MSG_CMD_CTL_LEN));
	DPRINT("\t CTRL ADDR = %llx \n\n",
	    GET_FIELD(desc->control, MSG_CMD_CTL_ADDR));

	DPRINT("MSG - DATA: \n");
	DPRINT("\t CTRL      = %lld \n",
	    GET_FIELD(desc->data, MSG_CMD_DATA_CTL));
	DPRINT("\t DATA LEN  = %lld \n",
	    GET_FIELD(desc->data, MSG_CMD_DATA_LEN));
	DPRINT("\t DATA ADDR = %llx \n\n",
	    GET_FIELD(desc->data, MSG_CMD_DATA_ADDR));

	DPRINT("CONTROL DESCRIPTOR: \n");
	word = desc->ctl_desc.instruction;
	DPRINT("\tINSTRUCTION:   %llx\n", word);
	DPRINT("\t\tOVERRIDE CIPH = %lld \n", GET_FIELD(word, CTL_DSC_OVERRIDECIPHER));
	DPRINT("\t\tARC4 WAIT     = %lld \n", GET_FIELD(word, CTL_DSC_ARC4_WAIT4SAVE));
	DPRINT("\t\tARC4 SAVE     = %lld \n", GET_FIELD(word, CTL_DSC_ARC4_SAVESTATE));
	DPRINT("\t\tARC4 LOAD     = %lld \n", GET_FIELD(word, CTL_DSC_ARC4_LOADSTATE));
	DPRINT("\t\tARC4 KEYLEN   = %lld \n", GET_FIELD(word, CTL_DSC_ARC4_KEYLEN));
	DPRINT("\t\tCIPHER        = %lld \n", GET_FIELD(word, CTL_DSC_CPHR));
	DPRINT("\t\tCIPHER MODE   = %lld \n", GET_FIELD(word, CTL_DSC_MODE));
	DPRINT("\t\tINIT CIPHER   = %lld \n", GET_FIELD(word, CTL_DSC_ICPHR));
	DPRINT("\t\tHMAC          = %lld \n", GET_FIELD(word, CTL_DSC_HMAC));
	DPRINT("\t\tHASH ALG      = %lld \n", GET_FIELD(word, CTL_DSC_HASH) | (GET_FIELD(word, CTL_DSC_HASHHI) << 2));
	DPRINT("\t\tINIT HASH     = %lld \n", GET_FIELD(word, CTL_DSC_IHASH));
	DPRINT("\t\tCHKSUM        = %lld \n", GET_FIELD(word, CTL_DSC_CKSUM));
	DPRINT("\tCIPHER HASH INFO: \n");
#if 0
	info = (uint8_t *) & desc->ctl_desc->cipherHashInfo;
	for (i = 0; i < sizeof(CipherHashInfo_t); i++, info++) {
		DPRINT(" %02x", *info);
		if (i && (i % 16) == 0)
			DPRINT("\n");
	}
	DPRINT("\n\n");
#endif

	switch (cfg_vector) {
	case XLR_SEC_VECTOR_CIPHER_ARC4:
		DPRINT("VECTOR:  XLR_SEC_VECTOR_CIPHER_ARC4 \n");
		print_buf("ARC4 Key",
		    &desc->ctl_desc.cipherHashInfo.infoARC4.cipherKey0,
		    GET_FIELD(word, CTL_DSC_ARC4_KEYLEN));
		break;
	case XLR_SEC_VECTOR_CIPHER_ARC4__HMAC:
		DPRINT("VECTOR:  XLR_SEC_VECTOR_CIPHER_ARC4__HMAC \n");
		print_buf("ARC4 Key",
		    &desc->ctl_desc.cipherHashInfo.infoARC4HMAC.cipherKey0,
		    GET_FIELD(word, CTL_DSC_ARC4_KEYLEN));
		print_buf("HMAC Key",
		    &desc->ctl_desc.cipherHashInfo.infoARC4HMAC.hmacKey0,
		    sizeof(HMAC_t));
		break;
	case XLR_SEC_VECTOR_CIPHER_ARC4__STATE:
		DPRINT("VECTOR:  XLR_SEC_VECTOR_CIPHER_ARC4__STATE \n");
		print_buf("ARC4 Key",
		    &desc->ctl_desc.cipherHashInfo.infoARC4State.cipherKey0,
		    GET_FIELD(word, CTL_DSC_ARC4_KEYLEN));
		break;
	case XLR_SEC_VECTOR_CIPHER_ARC4__HMAC__STATE:
		DPRINT("VECTOR:  XLR_SEC_VECTOR_CIPHER_ARC4__HMAC__STATE \n");
		print_buf("ARC4 Key",
		    &desc->ctl_desc.cipherHashInfo.infoARC4StateHMAC.cipherKey0,
		    GET_FIELD(word, CTL_DSC_ARC4_KEYLEN));
		print_buf("HMAC Key",
		    &desc->ctl_desc.cipherHashInfo.infoARC4StateHMAC.hmacKey0,
		    sizeof(HMAC_t));
		break;
	case XLR_SEC_VECTOR_CIPHER_KASUMI_F8:
		DPRINT("VECTOR:  XLR_SEC_VECTOR_CIPHER_KASUMI_F8 \n");
		print_buf("KASUMI_F8 Key",
		    &desc->ctl_desc.cipherHashInfo.infoKASUMIF8.cipherKey0,
		    XLR_SEC_KASUMI_F8_KEY_LENGTH);
		break;
	case XLR_SEC_VECTOR_CIPHER_KASUMI_F8__HMAC:
		DPRINT("XLR_SEC_VECTOR_CIPHER_KASUMI_F8__HMAC\n");
		print_buf("KASUMI_F8 Key",
		    &desc->ctl_desc.cipherHashInfo.infoKASUMIF8HMAC.cipherKey0,
		    XLR_SEC_KASUMI_F8_KEY_LENGTH);
		print_buf("HMAC Key",
		    &desc->ctl_desc.cipherHashInfo.infoKASUMIF8HMAC.hmacKey0,
		    sizeof(HMAC_t));
		break;
	case XLR_SEC_VECTOR_CIPHER_KASUMI_F8__HMAC2:
		DPRINT("XLR_SEC_VECTOR_CIPHER_KASUMI_F8__HMAC2\n");
		print_buf("KASUMI_F8 Key",
		    &desc->ctl_desc.cipherHashInfo.infoKASUMIF8HMAC2.cipherKey0,
		    XLR_SEC_KASUMI_F8_KEY_LENGTH);
		print_buf("HMAC2 Key",
		    &desc->ctl_desc.cipherHashInfo.infoKASUMIF8HMAC2.hmacKey0,
		    sizeof(HMAC2_t));
		break;
	case XLR_SEC_VECTOR_CIPHER_KASUMI_F8__GCM:
		DPRINT("XLR_SEC_VECTOR_CIPHER_KASUMI_F8__GCM\n");
		print_buf("KASUMI_F8 Key",
		    &desc->ctl_desc.cipherHashInfo.infoKASUMIF8GCM.cipherKey0,
		    XLR_SEC_KASUMI_F8_KEY_LENGTH);
		print_buf("GCM Key",
		    &desc->ctl_desc.cipherHashInfo.infoKASUMIF8GCM.GCMH0,
		    sizeof(GCM_t));
		break;
	case XLR_SEC_VECTOR_CIPHER_KASUMI_F8__F9:
		DPRINT("XLR_SEC_VECTOR_CIPHER_KASUMI_F8__F9\n");
		print_buf("KASUMI_F8 Key",
		    &desc->ctl_desc.cipherHashInfo.infoKASUMIF8F9.cipherKey0,
		    XLR_SEC_KASUMI_F8_KEY_LENGTH);
		print_buf("F9 Key",
		    &desc->ctl_desc.cipherHashInfo.infoKASUMIF8F9.authKey0,
		    sizeof(F9_t));
		break;
	case XLR_SEC_VECTOR_MAC:
		DPRINT("VECTOR:  XLR_SEC_VECTOR_MAC \n");
		DPRINT("MAC-ONLY - No Info\n");
		break;
	case XLR_SEC_VECTOR_HMAC:
		DPRINT("VECTOR:  XLR_SEC_VECTOR_HMAC \n");
		print_buf("HMAC Key",
		    &desc->ctl_desc.cipherHashInfo.infoHMAC.hmacKey0,
		    sizeof(HMAC_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_DES__HMAC__MODE_ECB_CBC:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_DES__HMAC__MODE_ECB_CBC \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoDESHMAC.cipherKey0,
		    XLR_SEC_DES_KEY_LENGTH);
		print_buf("HMAC Key",
		    &desc->ctl_desc.cipherHashInfo.infoDESHMAC.hmacKey0,
		    sizeof(HMAC_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_DES__MODE_ECB_CBC:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_DES__MODE_ECB_CBC \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoDES.cipherKey0,
		    XLR_SEC_DES_KEY_LENGTH);
		break;
	case XLR_SEC_VECTOR__CIPHER_3DES__HMAC__MODE_ECB_CBC:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_3DES__HMAC__MODE_ECB_CBC \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.info3DESHMAC.cipherKey0,
		    XLR_SEC_3DES_KEY_LENGTH);
		print_buf("HMAC Key",
		    &desc->ctl_desc.cipherHashInfo.info3DESHMAC.hmacKey0,
		    sizeof(HMAC_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_3DES__MODE_ECB_CBC:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_3DES__MODE_ECB_CBC \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.info3DES.cipherKey0,
		    XLR_SEC_3DES_KEY_LENGTH);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES128__HMAC__MODE_CTR_CFB:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES128__HMAC__MODE_CTR_CFB \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES128HMAC.cipherKey0,
		    XLR_SEC_AES128_KEY_LENGTH);
		print_buf("HMAC Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES128HMAC.hmacKey0,
		    sizeof(HMAC_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_AES128__MODE_CTR_CFB:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES128__MODE_CTR_CFB \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES128.cipherKey0,
		    XLR_SEC_AES128_KEY_LENGTH);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES128__HMAC__MODE_ECB_CBC_OFB:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES128__HMAC__MODE_ECB_CBC_OFB\n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES128HMAC.cipherKey0,
		    XLR_SEC_AES128_KEY_LENGTH);
		print_buf("HMAC Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES128HMAC.hmacKey0,
		    sizeof(HMAC_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_AES128__MODE_ECB_CBC_OFB:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES128__MODE_ECB_CBC_OFB \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES128.cipherKey0,
		    XLR_SEC_AES128_KEY_LENGTH);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES192__HMAC__MODE_CTR_CFB:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES192__HMAC__MODE_CTR_CFB \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES192HMAC.cipherKey0,
		    XLR_SEC_AES192_KEY_LENGTH);
		print_buf("HMAC Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES192HMAC.hmacKey0,
		    sizeof(HMAC_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_AES192__MODE_CTR_CFB:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES192__MODE_CTR_CFB \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES192.cipherKey0,
		    XLR_SEC_AES192_KEY_LENGTH);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES192__HMAC__MODE_ECB_CBC_OFB:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES192__HMAC__MODE_ECB_CBC_OFB\n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES192HMAC.cipherKey0,
		    XLR_SEC_AES192_KEY_LENGTH);
		print_buf("HMAC Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES192HMAC.hmacKey0,
		    sizeof(HMAC_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_AES192__MODE_ECB_CBC_OFB:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES192__MODE_ECB_CBC_OFB \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES192.cipherKey0,
		    XLR_SEC_AES192_KEY_LENGTH);
		break;

	case XLR_SEC_VECTOR__CIPHER_AES256__HMAC__MODE_CTR_CFB:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES256__HMAC__MODE_CTR_CFB \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES256HMAC.cipherKey0,
		    XLR_SEC_AES256_KEY_LENGTH);
		print_buf("HMAC Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES256HMAC.hmacKey0,
		    sizeof(HMAC_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_AES256__MODE_CTR_CFB:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES256__MODE_CTR_CFB \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES256.cipherKey0,
		    XLR_SEC_AES256_KEY_LENGTH);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES256__HMAC__MODE_ECB_CBC_OFB:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES256__HMAC__MODE_ECB_CBC_OFB \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES256HMAC.cipherKey0,
		    XLR_SEC_AES256_KEY_LENGTH);
		print_buf("HMAC Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES256HMAC.hmacKey0,
		    sizeof(HMAC_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_AES256__MODE_ECB_CBC_OFB:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES256__MODE_ECB_CBC_OFB \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES256.cipherKey0,
		    XLR_SEC_AES256_KEY_LENGTH);
		break;
	case XLR_SEC_VECTOR_CIPHER_ARC4__HMAC2:
		DPRINT("VECTOR:  XLR_SEC_VECTOR_CIPHER_ARC4__HMAC2 \n");
		print_buf("ARC4 Key",
		    &desc->ctl_desc.cipherHashInfo.infoARC4HMAC2.cipherKey0,
		    GET_FIELD(word, CTL_DSC_ARC4_KEYLEN));
		print_buf("HMAC2 Key",
		    &desc->ctl_desc.cipherHashInfo.infoARC4HMAC2.hmacKey0,
		    sizeof(HMAC2_t));
		break;
	case XLR_SEC_VECTOR_CIPHER_ARC4__HMAC2__STATE:
		DPRINT("VECTOR:  XLR_SEC_VECTOR_CIPHER_ARC4__HMAC2__STATE \n");
		print_buf("ARC4 Key",
		    &desc->ctl_desc.cipherHashInfo.infoARC4StateHMAC2.cipherKey0,
		    GET_FIELD(word, CTL_DSC_ARC4_KEYLEN));
		print_buf("HMAC2 Key",
		    &desc->ctl_desc.cipherHashInfo.infoARC4StateHMAC2.hmacKey0,
		    sizeof(HMAC2_t));
		break;
	case XLR_SEC_VECTOR_HMAC2:
		DPRINT("VECTOR:  XLR_SEC_VECTOR_HMAC2 \n");
		print_buf("HMAC2 Key",
		    &desc->ctl_desc.cipherHashInfo.infoHMAC2.hmacKey0,
		    sizeof(HMAC2_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_DES__HMAC2__MODE_ECB_CBC:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_DES__HMAC2__MODE_ECB_CBC \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoDESHMAC2.cipherKey0,
		    XLR_SEC_DES_KEY_LENGTH);
		print_buf("HMAC2 Key",
		    &desc->ctl_desc.cipherHashInfo.infoDESHMAC2.hmacKey0,
		    sizeof(HMAC2_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_3DES__HMAC2__MODE_ECB_CBC:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_3DES__HMAC2__MODE_ECB_CBC \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.info3DESHMAC2.cipherKey0,
		    XLR_SEC_3DES_KEY_LENGTH);
		print_buf("HMAC2 Key",
		    &desc->ctl_desc.cipherHashInfo.info3DESHMAC2.hmacKey0,
		    sizeof(HMAC2_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_AES128__HMAC2__MODE_CTR_CFB:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES128__HMAC2__MODE_CTR_CFB \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES128HMAC2.cipherKey0,
		    XLR_SEC_AES128_KEY_LENGTH);
		print_buf("HMAC2 Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES128HMAC2.hmacKey0,
		    sizeof(HMAC2_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_AES128__HMAC2__MODE_ECB_CBC_OFB:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES128__HMAC2__MODE_ECB_CBC_OFB\n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES128HMAC2.cipherKey0,
		    XLR_SEC_AES128_KEY_LENGTH);
		print_buf("HMAC2 Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES128HMAC2.hmacKey0,
		    sizeof(HMAC2_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_AES192__HMAC2__MODE_CTR_CFB:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES192__HMAC2__MODE_CTR_CFB \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES192HMAC2.cipherKey0,
		    XLR_SEC_AES192_KEY_LENGTH);
		print_buf("HMAC2 Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES192HMAC2.hmacKey0,
		    sizeof(HMAC2_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_AES192__HMAC2__MODE_ECB_CBC_OFB:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES192__HMAC2__MODE_ECB_CBC_OFB\n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES192HMAC2.cipherKey0,
		    XLR_SEC_AES192_KEY_LENGTH);
		print_buf("HMAC2 Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES192HMAC2.hmacKey0,
		    sizeof(HMAC2_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_AES256__HMAC2__MODE_CTR_CFB:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES256__HMAC2__MODE_CTR_CFB \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES256HMAC2.cipherKey0,
		    XLR_SEC_AES256_KEY_LENGTH);
		print_buf("HMAC2 Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES256HMAC2.hmacKey0,
		    sizeof(HMAC2_t));
		break;

	case XLR_SEC_VECTOR__CIPHER_AES256__HMAC2__MODE_ECB_CBC_OFB:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES256__HMAC2__MODE_ECB_CBC_OFB \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES256HMAC2.cipherKey0,
		    XLR_SEC_AES256_KEY_LENGTH);
		print_buf("HMAC2 Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES256HMAC2.hmacKey0,
		    sizeof(HMAC2_t));
		break;
	case XLR_SEC_VECTOR_CIPHER_ARC4__GCM:
		DPRINT("VECTOR:  XLR_SEC_VECTOR_CIPHER_ARC4__GCM \n");
		print_buf("ARC4 Key",
		    &desc->ctl_desc.cipherHashInfo.infoARC4GCM.cipherKey0,
		    GET_FIELD(word, CTL_DSC_ARC4_KEYLEN));
		print_buf("GCM Key",
		    &desc->ctl_desc.cipherHashInfo.infoARC4GCM.GCMH0,
		    sizeof(GCM_t));
		break;
	case XLR_SEC_VECTOR_CIPHER_ARC4__GCM__STATE:
		DPRINT("VECTOR:  XLR_SEC_VECTOR_CIPHER_ARC4__GCM__STATE \n");
		print_buf("ARC4 Key",
		    &desc->ctl_desc.cipherHashInfo.infoARC4StateGCM.cipherKey0,
		    GET_FIELD(word, CTL_DSC_ARC4_KEYLEN));
		print_buf("GCM Key",
		    &desc->ctl_desc.cipherHashInfo.infoARC4StateGCM.GCMH0,
		    sizeof(GCM_t));
		break;
	case XLR_SEC_VECTOR_GCM:
		DPRINT("VECTOR:  XLR_SEC_VECTOR_GCM \n");
		print_buf("GCM Key",
		    &desc->ctl_desc.cipherHashInfo.infoGCM.GCMH0,
		    sizeof(GCM_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_DES__GCM__MODE_ECB_CBC:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_DES__GCM__MODE_ECB_CBC \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoDESGCM.cipherKey0,
		    XLR_SEC_DES_KEY_LENGTH);
		print_buf("GCM Key",
		    &desc->ctl_desc.cipherHashInfo.infoDESGCM.GCMH0,
		    sizeof(GCM_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_3DES__GCM__MODE_ECB_CBC:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_3DES__GCM__MODE_ECB_CBC \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.info3DESGCM.cipherKey0,
		    XLR_SEC_3DES_KEY_LENGTH);
		print_buf("GCM Key",
		    &desc->ctl_desc.cipherHashInfo.info3DESGCM.GCMH0,
		    sizeof(GCM_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_AES128__GCM__MODE_CTR_CFB:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES128__GCM__MODE_CTR_CFB \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES128GCM.cipherKey0,
		    XLR_SEC_AES128_KEY_LENGTH);
		print_buf("GCM Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES128GCM.GCMH0,
		    XLR_SEC_AES128_KEY_LENGTH);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES128__GCM__MODE_ECB_CBC_OFB:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES128__GCM__MODE_ECB_CBC_OFB\n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES128GCM.cipherKey0,
		    XLR_SEC_AES128_KEY_LENGTH);
		print_buf("GCM Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES128GCM.GCMH0,
		    XLR_SEC_AES128_KEY_LENGTH);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES192__GCM__MODE_CTR_CFB:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES192__GCM__MODE_CTR_CFB \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES192GCM.cipherKey0,
		    XLR_SEC_AES192_KEY_LENGTH);
		print_buf("GCM Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES192GCM.GCMH0,
		    XLR_SEC_AES192_KEY_LENGTH);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES192__GCM__MODE_ECB_CBC_OFB:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES192__GCM__MODE_ECB_CBC_OFB\n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES192GCM.cipherKey0,
		    XLR_SEC_AES192_KEY_LENGTH);
		print_buf("GCM Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES192GCM.GCMH0,
		    XLR_SEC_AES192_KEY_LENGTH);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES256__GCM__MODE_CTR_CFB:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES256__GCM__MODE_CTR_CFB \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES256GCM.cipherKey0,
		    XLR_SEC_AES256_KEY_LENGTH);
		print_buf("GCM Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES256GCM.GCMH0,
		    XLR_SEC_AES256_KEY_LENGTH);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES256__GCM__MODE_ECB_CBC_OFB:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES256__GCM__MODE_ECB_CBC_OFB \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES256GCM.cipherKey0,
		    XLR_SEC_AES256_KEY_LENGTH);
		print_buf("GCM Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES256GCM.GCMH0,
		    XLR_SEC_AES256_KEY_LENGTH);
		break;
	case XLR_SEC_VECTOR_CIPHER_ARC4__F9:
		DPRINT("VECTOR:  XLR_SEC_VECTOR_CIPHER_ARC4__F9 \n");
		print_buf("ARC4 Key",
		    &desc->ctl_desc.cipherHashInfo.infoARC4F9.cipherKey0,
		    GET_FIELD(word, CTL_DSC_ARC4_KEYLEN));
		print_buf("F9 Key",
		    &desc->ctl_desc.cipherHashInfo.infoARC4F9.authKey0,
		    sizeof(F9_t));
		break;
	case XLR_SEC_VECTOR_CIPHER_ARC4__F9__STATE:
		DPRINT("VECTOR:  XLR_SEC_VECTOR_CIPHER_ARC4__F9__STATE \n");
		print_buf("ARC4 Key",
		    &desc->ctl_desc.cipherHashInfo.infoARC4StateF9.cipherKey0,
		    GET_FIELD(word, CTL_DSC_ARC4_KEYLEN));
		print_buf("F9 Key",
		    &desc->ctl_desc.cipherHashInfo.infoARC4StateF9.authKey0,
		    sizeof(F9_t));
		break;
	case XLR_SEC_VECTOR_F9:
		DPRINT("VECTOR:  XLR_SEC_VECTOR_F9 \n");
		print_buf("F9 Key",
		    &desc->ctl_desc.cipherHashInfo.infoF9.authKey0,
		    sizeof(F9_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_DES__F9__MODE_ECB_CBC:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_DES__F9__MODE_ECB_CBC \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoDESF9.cipherKey0,
		    XLR_SEC_DES_KEY_LENGTH);
		print_buf("F9 Key",
		    &desc->ctl_desc.cipherHashInfo.infoDESF9.authKey0,
		    sizeof(F9_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_3DES__F9__MODE_ECB_CBC:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_3DES__F9__MODE_ECB_CBC \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.info3DESF9.cipherKey0,
		    XLR_SEC_3DES_KEY_LENGTH);
		print_buf("F9 Key",
		    &desc->ctl_desc.cipherHashInfo.info3DESF9.authKey0,
		    sizeof(F9_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_AES128__F9__MODE_CTR_CFB:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES128__F9__MODE_CTR_CFB \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES128F9.cipherKey0,
		    XLR_SEC_AES128_KEY_LENGTH);
		print_buf("F9 Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES128F9.authKey0,
		    sizeof(F9_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_AES128__F9__MODE_ECB_CBC_OFB:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES128__F9__MODE_ECB_CBC_OFB\n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES128F9.cipherKey0,
		    XLR_SEC_AES128_KEY_LENGTH);
		print_buf("F9 Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES128F9.authKey0,
		    sizeof(F9_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_AES192__F9__MODE_CTR_CFB:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES192__F9__MODE_CTR_CFB \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES192F9.cipherKey0,
		    XLR_SEC_AES192_KEY_LENGTH);
		print_buf("F9 Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES192F9.authKey0,
		    sizeof(F9_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_AES192__F9__MODE_ECB_CBC_OFB:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES192__F9__MODE_ECB_CBC_OFB\n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES192F9.cipherKey0,
		    XLR_SEC_AES192_KEY_LENGTH);
		print_buf("F9 Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES192F9.authKey0,
		    sizeof(F9_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_AES256__F9__MODE_CTR_CFB:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES256__F9__MODE_CTR_CFB \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES256F9.cipherKey0,
		    XLR_SEC_AES256_KEY_LENGTH);
		print_buf("F9 Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES256F9.authKey0,
		    sizeof(F9_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_AES256__F9__MODE_ECB_CBC_OFB:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES256__F9__MODE_ECB_CBC_OFB \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES256F9.cipherKey0,
		    XLR_SEC_AES256_KEY_LENGTH);
		print_buf("F9 Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES256F9.authKey0,
		    sizeof(F9_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_AES128__HMAC__MODE_F8:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES128__HMAC__MODE_F8 \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES128F8HMAC.cipherKey0,
		    XLR_SEC_AES128F8_KEY_LENGTH);
		print_buf("HMAC Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES128F8HMAC.hmacKey0,
		    sizeof(HMAC_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_AES128__MODE_F8:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES128__MODE_F8 \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES128F8.cipherKey0,
		    XLR_SEC_AES128F8_KEY_LENGTH);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES192__HMAC__MODE_F8:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES192__HMAC__MODE_F8 \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES192F8HMAC.cipherKey0,
		    XLR_SEC_AES192F8_KEY_LENGTH);
		print_buf("HMAC Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES192F8HMAC.hmacKey0,
		    sizeof(HMAC_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_AES192__MODE_F8:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES192__MODE_F8 \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES192F8.cipherKey0,
		    XLR_SEC_AES192F8_KEY_LENGTH);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES256__HMAC__MODE_F8:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES256__HMAC__MODE_F8 \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES256F8HMAC.cipherKey0,
		    XLR_SEC_AES256F8_KEY_LENGTH);
		print_buf("HMAC Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES256HMAC.hmacKey0,
		    sizeof(HMAC_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_AES256__MODE_F8:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES256__MODE_F8 \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES256F8.cipherKey0,
		    XLR_SEC_AES256F8_KEY_LENGTH);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES128__HMAC2__MODE_F8:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES128__HMAC2__MODE_F8 \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES128F8HMAC2.cipherKey0,
		    XLR_SEC_AES128F8_KEY_LENGTH);
		print_buf("HMAC2 Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES128F8HMAC2.hmacKey0,
		    sizeof(HMAC2_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_AES192__HMAC2__MODE_F8:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES192__HMAC2__MODE_F8 \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES192F8HMAC2.cipherKey0,
		    XLR_SEC_AES192F8_KEY_LENGTH);
		print_buf("HMAC2 Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES192F8HMAC2.hmacKey0,
		    sizeof(HMAC2_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_AES256__HMAC2__MODE_F8:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES256__HMAC2__MODE_F8 \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES256F8HMAC2.cipherKey0,
		    XLR_SEC_AES256F8_KEY_LENGTH);
		print_buf("HMAC2 Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES256F8HMAC2.hmacKey0,
		    sizeof(HMAC2_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_AES128__GCM__MODE_F8:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES128__GCM__MODE_F8 \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES128F8GCM.cipherKey0,
		    XLR_SEC_AES128F8_KEY_LENGTH);
		print_buf("GCM Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES128GCM.GCMH0,
		    XLR_SEC_AES128_KEY_LENGTH);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES192__GCM__MODE_F8:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES192__GCM__MODE_F8 \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES192F8GCM.cipherKey0,
		    XLR_SEC_AES192_KEY_LENGTH);
		print_buf("GCM Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES192F8GCM.GCMH0,
		    XLR_SEC_AES192_KEY_LENGTH);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES256__GCM__MODE_F8:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES256__GCM__MODE_F8 \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES256F8GCM.cipherKey0,
		    XLR_SEC_AES256F8_KEY_LENGTH);
		print_buf("GCM Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES256F8GCM.GCMH0,
		    XLR_SEC_AES256_KEY_LENGTH);
		break;
	case XLR_SEC_VECTOR__CIPHER_AES128__F9__MODE_F8:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES128__F9__MODE_F8 \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES128F8F9.cipherKey0,
		    XLR_SEC_AES128F8_KEY_LENGTH);
		print_buf("F9 Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES128F8F9.authKey0,
		    sizeof(F9_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_AES192__F9__MODE_F8:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES192__F9__MODE_F8 \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES192F8F9.cipherKey0,
		    XLR_SEC_AES192F8_KEY_LENGTH);
		print_buf("F9 Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES192F8F9.authKey0,
		    sizeof(F9_t));
		break;
	case XLR_SEC_VECTOR__CIPHER_AES256__F9__MODE_F8:
		DPRINT("VECTOR:  XLR_SEC_VECTOR__CIPHER_AES256__F9__MODE_F8 \n");
		print_buf("CIPHER Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES256F8F9.cipherKey0,
		    XLR_SEC_AES256F8_KEY_LENGTH);
		print_buf("F9 Key",
		    &desc->ctl_desc.cipherHashInfo.infoAES256F8F9.authKey0,
		    sizeof(F9_t));
		break;

	default:
		DPRINT("VECTOR:  ???? \n");
		DPRINT(">>>  WHAT THE HECK !!!  <<< \n");
		break;
	}
	DPRINT("PACKET DESCRIPTOR: \n");
	word = 0; //desc->pkt_desc.srcLengthIVOffUseIVNext;
	DPRINT("\tSrcLengthIVOffsetIVNext:   %llx\n", word);
	DPRINT("\t\tLoad HMAC         = %lld \n",
	    GET_FIELD(word, PKT_DSC_LOADHMACKEY));
	DPRINT("\t\tPad Hash          = %lld \n",
	    GET_FIELD(word, PKT_DSC_PADHASH));
	DPRINT("\t\tHash Byte Count   = %lld \n",
	    GET_FIELD(word, PKT_DSC_HASHBYTES));
	DPRINT("\t\tNext              = %lld \n",
	    GET_FIELD(word, PKT_DSC_NEXT));
	DPRINT("\t\tUse IV            = %lld \n",
	    GET_FIELD(word, PKT_DSC_IV));
	DPRINT("\t\tIV Offset         = %lld \n",
	    GET_FIELD(word, PKT_DSC_IVOFF));
	DPRINT("\t\tPacket Length     = %lld \n",
	    GET_FIELD(word, PKT_DSC_PKTLEN));
	DPRINT("\t\tNLHMAC            = %lld \n", GET_FIELD(word, PKT_DSC_NLHMAC));
	DPRINT("\t\tBreak             = %lld \n", GET_FIELD(word, PKT_DSC_BREAK));
	DPRINT("\t\tWait              = %lld \n", GET_FIELD(word, PKT_DSC_WAIT));
	DPRINT("\t\tSegment Src Addr  = %llx \n",
	    (GET_FIELD(word, PKT_DSC_SEGADDR) << 5) & 0xffffffffffULL);
	DPRINT("\t\tSRTCP             = %lld \n", GET_FIELD(word, PKT_DSC_SRTCP));
	DPRINT("\t\tGlobal Src Offset = %lld \n",
	    GET_FIELD(word, PKT_DSC_SEGOFFSET));

	word = 0; //desc->pkt_desc.dstDataSettings;
	DPRINT("\tdstDataSettings:  %llx \n", word);
	DPRINT("\t\tArc4 Byte Count   = %lld \n", GET_FIELD(word,
	    PKT_DSC_ARC4BYTECOUNT));
	DPRINT("\t\tSym Operation     = %lld \n", GET_FIELD(word, PKT_DSC_SYM_OP));
	DPRINT("\t\tCipher Offset     = %lld \n", GET_FIELD(word, PKT_DSC_CPHROFF));
	DPRINT("\t\tHash Offset       = %lld \n", GET_FIELD(word, PKT_DSC_HASHOFF));
	DPRINT("\t\tHash Source       = %lld \n", GET_FIELD(word, PKT_DSC_HASHSRC));
	DPRINT("\t\tChecksum Offset   = %lld \n", GET_FIELD(word,
	    PKT_DSC_CKSUMOFF));
	DPRINT("\t\tChecksum Source   = %lld \n", GET_FIELD(word,
	    PKT_DSC_CKSUMSRC));
	DPRINT("\t\tCipher Dest Addr  = %llx \n", GET_FIELD(word,
	    PKT_DSC_CPHR_DST_ADDR));
	DPRINT("\t\tCipher Dest Dword = %lld \n", GET_FIELD(word,
	    PKT_DSC_CPHR_DST_DWOFFSET));
	DPRINT("\t\tCipher Dest Offset= %lld \n", GET_FIELD(word,
	    PKT_DSC_CPHR_DST_OFFSET));
	word = 0; //desc->pkt_desc.authDstNonceLow;
	DPRINT("\tauthDstNonceLow:  %llx \n", word);
	DPRINT("\t\tNonce Low 24      = %lld \n", GET_FIELD(word,
	    PKT_DSC_NONCE_LOW));
	DPRINT("\t\tauthDst           = %llx \n", GET_FIELD(word,
	    PKT_DSC_AUTH_DST_ADDR));
	DPRINT("\t\tCipher Offset High= %lld \n", GET_FIELD(word,
	    PKT_DSC_CIPH_OFF_HI));
	word = 0; //desc->pkt_desc.ckSumDstNonceHiCFBMaskLLWMask;
	DPRINT("\tckSumDstNonceHiCFBMaskLLWMask:  %llx \n", word);
	DPRINT("\t\tHash Byte off     = %lld \n", GET_FIELD(word, PKT_DSC_HASH_BYTE_OFF));
	DPRINT("\t\tPacket Len bytes  = %lld \n", GET_FIELD(word, PKT_DSC_PKTLEN_BYTES));
	DPRINT("\t\tLast Long Word Mask = %lld \n", GET_FIELD(word,
	    PKT_DSC_LASTWORD));
	DPRINT("\t\tCipher Dst Address  = %llx \n", GET_FIELD(word,
	    PKT_DSC_CPHR_DST_ADDR));
	DPRINT("\t\tGlobal Dst Offset  = %lld \n", GET_FIELD(word,
	    PKT_DSC_CPHR_DST_OFFSET));

	DPRINT("CFG_VECTOR = %04x\n", cfg_vector);
	DPRINT("\n\n");
}

#endif



/* This function is called from an interrupt handler */
void 
xlr_sec_msgring_handler(int bucket, int size, int code, int stid,
    struct msgrng_msg *msg, void *data)
{
	uint64_t error;
	uint64_t addr, sec_eng, sec_pipe;
	xlr_sec_io_pt op = NULL;
	symkey_desc_pt desc = NULL;
	struct xlr_sec_session *ses = NULL;
	struct xlr_sec_command *cmd = NULL;
	uint32_t flags;

	if (code != MSGRNG_CODE_SEC) {
		panic("xlr_sec_msgring_handler: bad code = %d,"
		    " expected code = %d\n",
		    code, MSGRNG_CODE_SEC);
	}
	if ((stid < MSGRNG_STNID_SEC0) || (stid > MSGRNG_STNID_PK0)) {
		panic("xlr_sec_msgring_handler: bad stn id = %d, expect %d - %d\n",
		    stid, MSGRNG_STNID_SEC0, MSGRNG_STNID_PK0);
	}
	/*
	 * The Submit() operation encodes the engine and pipe in these two
	 * separate fields.  This allows use to verify the result type with
	 * the submitted operation type.
	 */
	sec_eng = GET_FIELD(msg->msg0, MSG_CTL_OP_TYPE);
	sec_pipe = GET_FIELD(msg->msg1, MSG_CTL_OP_TYPE);

	error = msg->msg0 >> 40 & 0x1ff;
	if (error)
		printf("ctrl error = 0x%llx\n", error);
	error = msg->msg1 >> 40 & 0x1ff;
	if (error)
		printf("data error = 0x%llx\n", error);


	XLR_SEC_CMD_DIAG("[%s]:  eng=%lld  pipe=%lld\n",
	    __FUNCTION__, sec_eng, sec_pipe);

	/* Symmetric Key Operation ? */
	if (sec_eng == MSG0_CTL_OP_ENGINE_SYMKEY) {

		/*
		 * The data descriptor address allows us to associate the
		 * response with the submitted operation. Address is 40-bit
		 * cacheline aligned address. We need to zero bit 0-4 since
		 * they are used for the engine and pipe Id.
		 */
		addr = GET_FIELD(msg->msg1, MSG_RSLT_DATA_DSC_ADDR);
		addr = addr & ~((1 << 5) - 1);
		if (!addr) {
			panic("[%s:STNID_SEC]:  NULL symkey addr!\n", __FUNCTION__);
		}

		/*
		 * The adddress points to the data descriptor. The operation
		 * descriptor is defined with the 32-byte cacheline size in
		 * mind.  It allows the code to use this address to
		 * reference the symkey descriptor.   (ref:  xlr_sec_desc.h)
		 */
		addr = addr - sizeof(OperationDescriptor_t);
		flags = xlr_enable_kx();
		desc = (symkey_desc_pt)(uintptr_t)xlr_paddr_ld(addr + 
		    offsetof(OperationDescriptor_t, vaddr));
		xlr_restore_kx(flags);

		if (!desc) {
			printf("\nerror : not getting desc back correctly \n");
			panic("[%s:STNID_SEC]:  NULL symkey data descriptor!\n", __FUNCTION__);
		}
		ses = (struct xlr_sec_session *)desc->ses;
		if (!ses) {
			printf("\n error : not getting ses back correctly \n");
			panic("[%s:STNID_SEC]:  NULL symkey data descriptor!\n", __FUNCTION__);
		}
		cmd = &ses->cmd;
		if (!cmd) {
			printf("\n error : not getting cmd back correctly \n");
			panic("[%s:STNID_SEC]:  NULL symkey data descriptor!\n", __FUNCTION__);
		}
		op = &cmd->op;
		if (!op) {
			printf("\n error : not getting op back correctly \n");
			panic("[%s:STNID_SEC]:  NULL symkey data descriptor!\n", __FUNCTION__);
		}
		XLR_SEC_CMD_DIAG("[%s:STNID_SEC]:  addr=0x%llx  desc=%p  alloc=%p \n",
		    __FUNCTION__, addr, desc, desc->alloc);

		XLR_SEC_CMD_DIAG("[%s:STNID_SEC]:  op_ctl=%p  phys_self=%llx  stn_id=%d \n",
		    __FUNCTION__, &desc->op_ctl, desc->op_ctl.phys_self,
		    desc->op_ctl.stn_id);

		if (addr != desc->op_ctl.phys_self) {
			XLR_SEC_CMD_DIAG("[%s:STNID_SEC]:  Control Descriptor fails Self-Verify !\n",
			    __FUNCTION__);
			printf("[%s:STNID_SEC]:  Control Descriptor fails Self-Verify !\n",
			    __FUNCTION__);
			printf("[%s:STNID_SEC]:  addr=0x%llx  desc=%p  alloc=%p \n",
			    __FUNCTION__, (unsigned long long)addr, desc, desc->alloc);
			printf("[%s:STNID_SEC]:  op_ctl=%p  phys_self=%llx  stn_id=%d \n",
			    __FUNCTION__, &desc->op_ctl, (unsigned long long)desc->op_ctl.phys_self,
			    desc->op_ctl.stn_id);

		}
		if (desc->op_ctl.stn_id != MSGRNG_STNID_SEC0 &&
		    desc->op_ctl.stn_id != MSGRNG_STNID_SEC1) {
			XLR_SEC_CMD_DIAG("[%s:STNID_SEC]:  Operation Type Mismatch !\n",
			    __FUNCTION__);
			printf("[%s:STNID_SEC]:  Operation Type Mismatch !\n",
			    __FUNCTION__);
			printf("[%s:STNID_SEC]:  addr=0x%llx  desc=%p  alloc=%p \n",
			    __FUNCTION__, (unsigned long long)addr, desc, desc->alloc);
			printf("[%s:STNID_SEC]:  op_ctl=%p  phys_self=%llx  stn_id=%d \n",
			    __FUNCTION__, &desc->op_ctl, (unsigned long long)desc->op_ctl.phys_self,
			    desc->op_ctl.stn_id);
		}
		desc->ctl_result = GET_FIELD(msg->msg0, MSG_RSLT_CTL_INST_ERR);
		desc->data_result = GET_FIELD(msg->msg1, MSG_RSLT_DATA_INST_ERR);

		XLR_SEC_CMD_DIAG("[%s:STNID_SEC]:  cpu=%d  ctl_result=0x%llx  data_result=%llx\n",
		    __FUNCTION__, desc->op_ctl.cpu,
		    desc->ctl_result, desc->data_result);

	}
#if 0
	else if (sec_eng == MSG0_CTL_OP_ENGINE_PUBKEY) {
		pubkey_desc_pt desc;

		if (sec_pipe != MSG1_CTL_OP_PUBKEY_PIPE0) {
			/* response to uc load */
			/*
			 * XLR_SEC_CMD_DIAG("[%s:STNID_SEC]: ecc cpu=%d
			 * ctl_result=0x%llx  data_result=%llx\n",
			 * __FUNCTION__, desc->op_ctl.cpu, desc->ctl_result,
			 * desc->data_result);
			 */
			return;
		}
		/*
		 * The data descriptor address allows us to associate the
		 * response with the submitted operation. Address is 40-bit
		 * cacheline aligned address. We need to zero bit 0-4 since
		 * they are used for the engine and pipe Id.
		 */
		addr = GET_FIELD(msg->msg0, PUBKEY_RSLT_CTL_SRCADDR);
		addr = addr & ~((1 << 5) - 1);
		if (!addr) {
			panic("[%s:STNID_SEC]:  NULL pubkey ctrl desc!\n", __FUNCTION__);
		}
		/*
		 * The adddress points to the data descriptor. The operation
		 * descriptor is defined with the 32-byte cacheline size in
		 * mind.  It allows the code to use this address to
		 * reference the symkey descriptor.   (ref:  xlr_sec_desc.h)
		 */
		addr = addr - sizeof(OperationDescriptor_t);

		/* Get pointer to pubkey Descriptor */
		desc = (pubkey_desc_pt) (unsigned long)addr;
		if (!desc) {
			panic("[%s:STNID_SEC]:  NULL pubkey data descriptor!\n", __FUNCTION__);
		}
		XLR_SEC_CMD_DIAG("[%s:STNID_PK0]:  addr=0x%llx  desc=%p  alloc=%p \n",
		    __FUNCTION__, addr, desc, desc->alloc);

		XLR_SEC_CMD_DIAG("[%s:STNID_PK0]:  op_ctl=%p  phys_self=%llx  stn_id=%d \n",
		    __FUNCTION__, &desc->op_ctl, desc->op_ctl.phys_self,
		    desc->op_ctl.stn_id);

		if (addr != desc->op_ctl.phys_self) {
			XLR_SEC_CMD_DIAG("[%s:STNID_PK0]:  Control Descriptor fails Self-Verify !\n",
			    __FUNCTION__);
		}
		if (desc->op_ctl.stn_id != msgrng_stnid_pk0) {
			XLR_SEC_CMD_DIAG("[%s:STNID_PK0]:  Operation Type Mismatch ! \n",
			    __FUNCTION__);
		}
		desc->ctl_result = GET_FIELD(msg->msg0, PUBKEY_RSLT_CTL_ERROR);
		desc->data_result = GET_FIELD(msg->msg1, PUBKEY_RSLT_DATA_ERROR);

		XLR_SEC_CMD_DIAG("[%s:STNID_PK0]:  ctl_result=0x%llx  data_result=%llx\n",
		    __FUNCTION__, desc->ctl_result, desc->data_result);

	}
#endif
	else {
		printf("[%s]: HANDLER  bad id = %d\n", __FUNCTION__, stid);
	}
#ifdef RMI_SEC_DEBUG
	if (ses->multi_frag_flag) {
		int i;
		char *ptr;

		printf("\n RETURNED DATA:  \n");

		ptr = (char *)(unsigned long)(desc->user.aligned_dest + cmd->op.cipher_offset);
		for (i = 0; i < SEC_MAX_FRAG_LEN; i++) {
			printf("%c  ", (char)*ptr++);
			if ((i % 10) == 0)
				printf("\n");
		}

		printf("second desc\n");
		ptr = (char *)(unsigned long)(desc->next_dest_buf);
		for (i = 0; i < desc->next_src_len; i++) {
			printf("%c  ", (char)*ptr++);
			if ((i % 10) == 0)
				printf("\n");
		}
	}
#endif

	/* Copy cipher-data to User-space */
	if (op->cipher_type != XLR_SEC_CIPHER_TYPE_NONE) {
		size = op->dest_buf_size;

		/* DEBUG -dpk */
		XLR_SEC_CMD_DIAG("cipher: to_addr=%p  from_addr=%p  size=%d \n",
		    desc->user.user_dest, desc->user.aligned_dest, size);

		if (ses->multi_frag_flag) {
			crypto_copyback(cmd->crp->crp_flags, cmd->crp->crp_buf, 0,
			    SEC_MAX_FRAG_LEN, (caddr_t)(long)desc->user.aligned_dest + op->cipher_offset);
			crypto_copyback(cmd->crp->crp_flags, cmd->crp->crp_buf + SEC_MAX_FRAG_LEN, 0,
			    desc->next_src_len, (caddr_t)(long)desc->next_dest_buf);
			crypto_done(cmd->crp);
		} else {
			crypto_copyback(cmd->crp->crp_flags, cmd->crp->crp_buf, 0,
			    cmd->op.dest_buf_size, (caddr_t)(long)desc->user.aligned_dest + op->cipher_offset);
			crypto_done(cmd->crp);
		}

	}

	/* Copy digest to User-space */
	if (op->digest_type != XLR_SEC_DIGEST_TYPE_NONE) {
		int offset = 0;

		switch (op->digest_type) {
		case XLR_SEC_DIGEST_TYPE_MD5:
			size = XLR_SEC_MD5_LENGTH;
			break;
		case XLR_SEC_DIGEST_TYPE_SHA1:
			size = XLR_SEC_SHA1_LENGTH;
			break;
		case XLR_SEC_DIGEST_TYPE_SHA256:
			size = XLR_SEC_SHA256_LENGTH;
			break;
		case XLR_SEC_DIGEST_TYPE_SHA384:
			size = XLR_SEC_SHA384_LENGTH;
			break;
		case XLR_SEC_DIGEST_TYPE_SHA512:
			size = XLR_SEC_SHA512_LENGTH;
			break;
		case XLR_SEC_DIGEST_TYPE_GCM:
			size = XLR_SEC_GCM_LENGTH;
			break;
		case XLR_SEC_DIGEST_TYPE_KASUMI_F9:
			offset = 4;
			size = XLR_SEC_KASUMI_F9_RESULT_LENGTH;
			break;
		default:
			size = 0;
		}

		XLR_SEC_CMD_DIAG("digest:  to_addr=%p  from_addr=%p  size=%d \n",
		    desc->user.user_auth, desc->user.aligned_auth, size);
		memcpy(desc->user.user_auth, desc->user.aligned_auth + offset, size);
		op->auth_dest = (uint64_t) (unsigned long)desc->user.user_auth;
	}
	if (op->cipher_type == XLR_SEC_CIPHER_TYPE_ARC4 &&
	    op->rc4_savestate) {
		size = XLR_SEC_MAX_RC4_STATE_SIZE;

		XLR_SEC_CMD_DIAG("state:  to_addr=%p  from_addr=%p  size=%d \n",
		    desc->user.user_state, desc->user.aligned_state, size);
		op->rc4_state = (uint64_t) (unsigned long)desc->user.user_state;
	}
	return;
}

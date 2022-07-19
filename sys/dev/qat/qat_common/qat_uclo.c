/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
#include "qat_freebsd.h"
#include "adf_cfg.h"
#include "adf_common_drv.h"
#include "adf_accel_devices.h"
#include "icp_qat_uclo.h"
#include "icp_qat_fw.h"
#include "icp_qat_fw_init_admin.h"
#include "adf_cfg_strings.h"
#include "adf_transport_access_macros.h"
#include "adf_transport_internal.h"
#include <sys/ctype.h>
#include <sys/kernel.h>
#include <linux/delay.h>
#include "adf_accel_devices.h"
#include "adf_common_drv.h"
#include "icp_qat_uclo.h"
#include "icp_qat_hal.h"
#include "icp_qat_fw_loader_handle.h"

#define UWORD_CPYBUF_SIZE 1024
#define INVLD_UWORD 0xffffffffffull
#define PID_MINOR_REV 0xf
#define PID_MAJOR_REV (0xf << 4)
#define MAX_UINT32_VAL 0xfffffffful

static int
qat_uclo_init_ae_data(struct icp_qat_uclo_objhandle *obj_handle,
		      unsigned int ae,
		      unsigned int image_num)
{
	struct icp_qat_uclo_aedata *ae_data;
	struct icp_qat_uclo_encapme *encap_image;
	struct icp_qat_uclo_page *page = NULL;
	struct icp_qat_uclo_aeslice *ae_slice = NULL;

	ae_data = &obj_handle->ae_data[ae];
	encap_image = &obj_handle->ae_uimage[image_num];
	ae_slice = &ae_data->ae_slices[ae_data->slice_num];
	ae_slice->encap_image = encap_image;

	if (encap_image->img_ptr) {
		ae_slice->ctx_mask_assigned =
		    encap_image->img_ptr->ctx_assigned;
		ae_data->shareable_ustore =
		    ICP_QAT_SHARED_USTORE_MODE(encap_image->img_ptr->ae_mode);
		ae_data->eff_ustore_size = ae_data->shareable_ustore ?
		    (obj_handle->ustore_phy_size << 1) :
		    obj_handle->ustore_phy_size;
	} else {
		ae_slice->ctx_mask_assigned = 0;
	}
	ae_slice->region =
	    malloc(sizeof(*ae_slice->region), M_QAT, M_WAITOK | M_ZERO);
	ae_slice->page =
	    malloc(sizeof(*ae_slice->page), M_QAT, M_WAITOK | M_ZERO);
	page = ae_slice->page;
	page->encap_page = encap_image->page;
	ae_slice->page->region = ae_slice->region;
	ae_data->slice_num++;
	return 0;
}

static int
qat_uclo_free_ae_data(struct icp_qat_uclo_aedata *ae_data)
{
	unsigned int i;

	if (!ae_data) {
		pr_err("QAT: bad argument, ae_data is NULL\n ");
		return EINVAL;
	}

	for (i = 0; i < ae_data->slice_num; i++) {
		free(ae_data->ae_slices[i].region, M_QAT);
		ae_data->ae_slices[i].region = NULL;
		free(ae_data->ae_slices[i].page, M_QAT);
		ae_data->ae_slices[i].page = NULL;
	}
	return 0;
}

static char *
qat_uclo_get_string(struct icp_qat_uof_strtable *str_table,
		    unsigned int str_offset)
{
	if (!str_table->table_len || str_offset > str_table->table_len)
		return NULL;
	return (char *)(((uintptr_t)(str_table->strings)) + str_offset);
}

static int
qat_uclo_check_uof_format(struct icp_qat_uof_filehdr *hdr)
{
	int maj = hdr->maj_ver & 0xff;
	int min = hdr->min_ver & 0xff;

	if (hdr->file_id != ICP_QAT_UOF_FID) {
		pr_err("QAT: Invalid header 0x%x\n", hdr->file_id);
		return EINVAL;
	}
	if (min != ICP_QAT_UOF_MINVER || maj != ICP_QAT_UOF_MAJVER) {
		pr_err("QAT: bad UOF version, major 0x%x, minor 0x%x\n",
		       maj,
		       min);
		return EINVAL;
	}
	return 0;
}

static int
qat_uclo_check_suof_format(const struct icp_qat_suof_filehdr *suof_hdr)
{
	int maj = suof_hdr->maj_ver & 0xff;
	int min = suof_hdr->min_ver & 0xff;

	if (suof_hdr->file_id != ICP_QAT_SUOF_FID) {
		pr_err("QAT: invalid header 0x%x\n", suof_hdr->file_id);
		return EINVAL;
	}
	if (suof_hdr->fw_type != 0) {
		pr_err("QAT: unsupported firmware type\n");
		return EINVAL;
	}
	if (suof_hdr->num_chunks <= 0x1) {
		pr_err("QAT: SUOF chunk amount is incorrect\n");
		return EINVAL;
	}
	if (maj != ICP_QAT_SUOF_MAJVER || min != ICP_QAT_SUOF_MINVER) {
		pr_err("QAT: bad SUOF version, major 0x%x, minor 0x%x\n",
		       maj,
		       min);
		return EINVAL;
	}
	return 0;
}

static int
qat_uclo_wr_sram_by_words(struct icp_qat_fw_loader_handle *handle,
			  unsigned int addr,
			  const unsigned int *val,
			  unsigned int num_in_bytes)
{
	unsigned int outval;
	const unsigned char *ptr = (const unsigned char *)val;

	if (num_in_bytes > handle->hal_sram_size) {
		pr_err("QAT: error, mmp size overflow %d\n", num_in_bytes);
		return EINVAL;
	}
	while (num_in_bytes) {
		memcpy(&outval, ptr, 4);
		SRAM_WRITE(handle, addr, outval);
		num_in_bytes -= 4;
		ptr += 4;
		addr += 4;
	}
	return 0;
}

static void
qat_uclo_wr_umem_by_words(struct icp_qat_fw_loader_handle *handle,
			  unsigned char ae,
			  unsigned int addr,
			  unsigned int *val,
			  unsigned int num_in_bytes)
{
	unsigned int outval;
	unsigned char *ptr = (unsigned char *)val;

	addr >>= 0x2; /* convert to uword address */

	while (num_in_bytes) {
		memcpy(&outval, ptr, 4);
		qat_hal_wr_umem(handle, ae, addr++, 1, &outval);
		num_in_bytes -= 4;
		ptr += 4;
	}
}

static void
qat_uclo_batch_wr_umem(struct icp_qat_fw_loader_handle *handle,
		       unsigned char ae,
		       struct icp_qat_uof_batch_init *umem_init_header)
{
	struct icp_qat_uof_batch_init *umem_init;

	if (!umem_init_header)
		return;
	umem_init = umem_init_header->next;
	while (umem_init) {
		unsigned int addr, *value, size;

		ae = umem_init->ae;
		addr = umem_init->addr;
		value = umem_init->value;
		size = umem_init->size;
		qat_uclo_wr_umem_by_words(handle, ae, addr, value, size);
		umem_init = umem_init->next;
	}
}

static void
qat_uclo_cleanup_batch_init_list(struct icp_qat_fw_loader_handle *handle,
				 struct icp_qat_uof_batch_init **base)
{
	struct icp_qat_uof_batch_init *umem_init;

	umem_init = *base;
	while (umem_init) {
		struct icp_qat_uof_batch_init *pre;

		pre = umem_init;
		umem_init = umem_init->next;
		free(pre, M_QAT);
	}
	*base = NULL;
}

static int
qat_uclo_parse_num(char *str, unsigned int *num)
{
	char buf[16] = { 0 };
	unsigned long ae = 0;
	int i;

	strncpy(buf, str, 15);
	for (i = 0; i < 16; i++) {
		if (!isdigit(buf[i])) {
			buf[i] = '\0';
			break;
		}
	}
	if ((compat_strtoul(buf, 10, &ae)))
		return EFAULT;

	if (ae > MAX_UINT32_VAL)
		return EFAULT;

	*num = (unsigned int)ae;
	return 0;
}

static int
qat_uclo_fetch_initmem_ae(struct icp_qat_fw_loader_handle *handle,
			  struct icp_qat_uof_initmem *init_mem,
			  unsigned int size_range,
			  unsigned int *ae)
{
	struct icp_qat_uclo_objhandle *obj_handle = handle->obj_handle;
	char *str;

	if ((init_mem->addr + init_mem->num_in_bytes) > (size_range << 0x2)) {
		pr_err("QAT: initmem is out of range");
		return EINVAL;
	}
	if (init_mem->scope != ICP_QAT_UOF_LOCAL_SCOPE) {
		pr_err("QAT: Memory scope for init_mem error\n");
		return EINVAL;
	}
	str = qat_uclo_get_string(&obj_handle->str_table, init_mem->sym_name);
	if (!str) {
		pr_err("QAT: AE name assigned in UOF init table is NULL\n");
		return EINVAL;
	}
	if (qat_uclo_parse_num(str, ae)) {
		pr_err("QAT: Parse num for AE number failed\n");
		return EINVAL;
	}
	if (*ae >= ICP_QAT_UCLO_MAX_AE) {
		pr_err("QAT: ae %d out of range\n", *ae);
		return EINVAL;
	}
	return 0;
}

static int
qat_uclo_create_batch_init_list(struct icp_qat_fw_loader_handle *handle,
				struct icp_qat_uof_initmem *init_mem,
				unsigned int ae,
				struct icp_qat_uof_batch_init **init_tab_base)
{
	struct icp_qat_uof_batch_init *init_header, *tail;
	struct icp_qat_uof_batch_init *mem_init, *tail_old;
	struct icp_qat_uof_memvar_attr *mem_val_attr;
	unsigned int i = 0;

	mem_val_attr =
	    (struct icp_qat_uof_memvar_attr *)((uintptr_t)init_mem +
					       sizeof(
						   struct icp_qat_uof_initmem));

	init_header = *init_tab_base;
	if (!init_header) {
		init_header =
		    malloc(sizeof(*init_header), M_QAT, M_WAITOK | M_ZERO);
		init_header->size = 1;
		*init_tab_base = init_header;
	}
	tail_old = init_header;
	while (tail_old->next)
		tail_old = tail_old->next;
	tail = tail_old;
	for (i = 0; i < init_mem->val_attr_num; i++) {
		mem_init = malloc(sizeof(*mem_init), M_QAT, M_WAITOK | M_ZERO);
		mem_init->ae = ae;
		mem_init->addr = init_mem->addr + mem_val_attr->offset_in_byte;
		mem_init->value = &mem_val_attr->value;
		mem_init->size = 4;
		mem_init->next = NULL;
		tail->next = mem_init;
		tail = mem_init;
		init_header->size += qat_hal_get_ins_num();
		mem_val_attr++;
	}
	return 0;
}

static int
qat_uclo_init_lmem_seg(struct icp_qat_fw_loader_handle *handle,
		       struct icp_qat_uof_initmem *init_mem)
{
	struct icp_qat_uclo_objhandle *obj_handle = handle->obj_handle;
	unsigned int ae;

	if (qat_uclo_fetch_initmem_ae(
		handle, init_mem, ICP_QAT_UCLO_MAX_LMEM_REG, &ae))
		return EINVAL;
	if (qat_uclo_create_batch_init_list(
		handle, init_mem, ae, &obj_handle->lm_init_tab[ae]))
		return EINVAL;
	return 0;
}

static int
qat_uclo_init_umem_seg(struct icp_qat_fw_loader_handle *handle,
		       struct icp_qat_uof_initmem *init_mem)
{
	struct icp_qat_uclo_objhandle *obj_handle = handle->obj_handle;
	unsigned int ae, ustore_size, uaddr, i;
	struct icp_qat_uclo_aedata *aed;

	ustore_size = obj_handle->ustore_phy_size;
	if (qat_uclo_fetch_initmem_ae(handle, init_mem, ustore_size, &ae))
		return EINVAL;
	if (qat_uclo_create_batch_init_list(
		handle, init_mem, ae, &obj_handle->umem_init_tab[ae]))
		return EINVAL;
	/* set the highest ustore address referenced */
	uaddr = (init_mem->addr + init_mem->num_in_bytes) >> 0x2;
	aed = &obj_handle->ae_data[ae];
	for (i = 0; i < aed->slice_num; i++) {
		if (aed->ae_slices[i].encap_image->uwords_num < uaddr)
			aed->ae_slices[i].encap_image->uwords_num = uaddr;
	}
	return 0;
}

#define ICP_DH895XCC_PESRAM_BAR_SIZE 0x80000
static int
qat_uclo_init_ae_memory(struct icp_qat_fw_loader_handle *handle,
			struct icp_qat_uof_initmem *init_mem)
{
	switch (init_mem->region) {
	case ICP_QAT_UOF_LMEM_REGION:
		if (qat_uclo_init_lmem_seg(handle, init_mem))
			return EINVAL;
		break;
	case ICP_QAT_UOF_UMEM_REGION:
		if (qat_uclo_init_umem_seg(handle, init_mem))
			return EINVAL;
		break;
	default:
		pr_err("QAT: initmem region error. region type=0x%x\n",
		       init_mem->region);
		return EINVAL;
	}
	return 0;
}

static int
qat_uclo_init_ustore(struct icp_qat_fw_loader_handle *handle,
		     struct icp_qat_uclo_encapme *image)
{
	unsigned int i;
	struct icp_qat_uclo_encap_page *page;
	struct icp_qat_uof_image *uof_image;
	unsigned char ae = 0;
	unsigned char neigh_ae;
	unsigned int ustore_size;
	unsigned int patt_pos;
	struct icp_qat_uclo_objhandle *obj_handle = handle->obj_handle;
	uint64_t *fill_data;
	static unsigned int init[32] = { 0 };
	unsigned long ae_mask = handle->hal_handle->ae_mask;

	uof_image = image->img_ptr;
	/*if shared CS mode, the ustore size should be 2*ustore_phy_size*/
	fill_data = malloc(obj_handle->ustore_phy_size * 2 * sizeof(uint64_t),
			   M_QAT,
			   M_WAITOK | M_ZERO);
	for (i = 0; i < obj_handle->ustore_phy_size * 2; i++)
		memcpy(&fill_data[i],
		       &uof_image->fill_pattern,
		       sizeof(uint64_t));
	page = image->page;

	for_each_set_bit(ae, &ae_mask, handle->hal_handle->ae_max_num)
	{
		unsigned long cfg_ae_mask = handle->cfg_ae_mask;
		unsigned long ae_assigned = uof_image->ae_assigned;

		if (!test_bit(ae, &cfg_ae_mask))
			continue;

		if (!test_bit(ae, &ae_assigned))
			continue;

		if (obj_handle->ae_data[ae].shareable_ustore && (ae & 1)) {
			qat_hal_get_scs_neigh_ae(ae, &neigh_ae);

			if (test_bit(neigh_ae, &ae_assigned))
				continue;
		}

		ustore_size = obj_handle->ae_data[ae].eff_ustore_size;
		patt_pos = page->beg_addr_p + page->micro_words_num;
		if (obj_handle->ae_data[ae].shareable_ustore) {
			qat_hal_get_scs_neigh_ae(ae, &neigh_ae);
			if (init[ae] == 0 && page->beg_addr_p != 0) {
				qat_hal_wr_coalesce_uwords(handle,
							   (unsigned char)ae,
							   0,
							   page->beg_addr_p,
							   &fill_data[0]);
			}
			qat_hal_wr_coalesce_uwords(
			    handle,
			    (unsigned char)ae,
			    patt_pos,
			    ustore_size - patt_pos,
			    &fill_data[page->beg_addr_p]);
			init[ae] = 1;
			init[neigh_ae] = 1;
		} else {
			qat_hal_wr_uwords(handle,
					  (unsigned char)ae,
					  0,
					  page->beg_addr_p,
					  &fill_data[0]);
			qat_hal_wr_uwords(handle,
					  (unsigned char)ae,
					  patt_pos,
					  ustore_size - patt_pos + 1,
					  &fill_data[page->beg_addr_p]);
		}
	}
	free(fill_data, M_QAT);
	return 0;
}

static int
qat_uclo_init_memory(struct icp_qat_fw_loader_handle *handle)
{
	int i;
	int ae = 0;
	struct icp_qat_uclo_objhandle *obj_handle = handle->obj_handle;
	struct icp_qat_uof_initmem *initmem = obj_handle->init_mem_tab.init_mem;
	unsigned long ae_mask = handle->hal_handle->ae_mask;

	for (i = 0; i < obj_handle->init_mem_tab.entry_num; i++) {
		if (initmem->num_in_bytes) {
			if (qat_uclo_init_ae_memory(handle, initmem))
				return EINVAL;
		}
		initmem =
		    (struct icp_qat_uof_initmem
			 *)((uintptr_t)((uintptr_t)initmem +
					sizeof(struct icp_qat_uof_initmem)) +
			    (sizeof(struct icp_qat_uof_memvar_attr) *
			     initmem->val_attr_num));
	}

	for_each_set_bit(ae, &ae_mask, handle->hal_handle->ae_max_num)
	{
		if (qat_hal_batch_wr_lm(handle,
					ae,
					obj_handle->lm_init_tab[ae])) {
			pr_err("QAT: fail to batch init lmem for AE %d\n", ae);
			return EINVAL;
		}
		qat_uclo_cleanup_batch_init_list(handle,
						 &obj_handle->lm_init_tab[ae]);
		qat_uclo_batch_wr_umem(handle,
				       ae,
				       obj_handle->umem_init_tab[ae]);
		qat_uclo_cleanup_batch_init_list(
		    handle, &obj_handle->umem_init_tab[ae]);
	}
	return 0;
}

static void *
qat_uclo_find_chunk(struct icp_qat_uof_objhdr *obj_hdr,
		    char *chunk_id,
		    void *cur)
{
	int i;
	struct icp_qat_uof_chunkhdr *chunk_hdr =
	    (struct icp_qat_uof_chunkhdr *)((uintptr_t)obj_hdr +
					    sizeof(struct icp_qat_uof_objhdr));

	for (i = 0; i < obj_hdr->num_chunks; i++) {
		if ((cur < (void *)&chunk_hdr[i]) &&
		    !strncmp(chunk_hdr[i].chunk_id,
			     chunk_id,
			     ICP_QAT_UOF_OBJID_LEN)) {
			return &chunk_hdr[i];
		}
	}
	return NULL;
}

static unsigned int
qat_uclo_calc_checksum(unsigned int reg, int ch)
{
	int i;
	unsigned int topbit = 1 << 0xF;
	unsigned int inbyte = (unsigned int)((reg >> 0x18) ^ ch);

	reg ^= inbyte << 0x8;
	for (i = 0; i < 0x8; i++) {
		if (reg & topbit)
			reg = (reg << 1) ^ 0x1021;
		else
			reg <<= 1;
	}
	return reg & 0xFFFF;
}

static unsigned int
qat_uclo_calc_str_checksum(const char *ptr, int num)
{
	unsigned int chksum = 0;

	if (ptr)
		while (num--)
			chksum = qat_uclo_calc_checksum(chksum, *ptr++);
	return chksum;
}

static struct icp_qat_uclo_objhdr *
qat_uclo_map_chunk(char *buf,
		   struct icp_qat_uof_filehdr *file_hdr,
		   char *chunk_id)
{
	struct icp_qat_uof_filechunkhdr *file_chunk;
	struct icp_qat_uclo_objhdr *obj_hdr;
	char *chunk;
	int i;

	file_chunk = (struct icp_qat_uof_filechunkhdr
			  *)(buf + sizeof(struct icp_qat_uof_filehdr));
	for (i = 0; i < file_hdr->num_chunks; i++) {
		if (!strncmp(file_chunk->chunk_id,
			     chunk_id,
			     ICP_QAT_UOF_OBJID_LEN)) {
			chunk = buf + file_chunk->offset;
			if (file_chunk->checksum !=
			    qat_uclo_calc_str_checksum(chunk, file_chunk->size))
				break;
			obj_hdr =
			    malloc(sizeof(*obj_hdr), M_QAT, M_WAITOK | M_ZERO);
			obj_hdr->file_buff = chunk;
			obj_hdr->checksum = file_chunk->checksum;
			obj_hdr->size = file_chunk->size;
			return obj_hdr;
		}
		file_chunk++;
	}
	return NULL;
}

static unsigned int
qat_uclo_check_image_compat(struct icp_qat_uof_encap_obj *encap_uof_obj,
			    struct icp_qat_uof_image *image)
{
	struct icp_qat_uof_objtable *uc_var_tab, *imp_var_tab, *imp_expr_tab;
	struct icp_qat_uof_objtable *neigh_reg_tab;
	struct icp_qat_uof_code_page *code_page;

	code_page =
	    (struct icp_qat_uof_code_page *)((char *)image +
					     sizeof(struct icp_qat_uof_image));
	uc_var_tab =
	    (struct icp_qat_uof_objtable *)(encap_uof_obj->beg_uof +
					    code_page->uc_var_tab_offset);
	imp_var_tab =
	    (struct icp_qat_uof_objtable *)(encap_uof_obj->beg_uof +
					    code_page->imp_var_tab_offset);
	imp_expr_tab =
	    (struct icp_qat_uof_objtable *)(encap_uof_obj->beg_uof +
					    code_page->imp_expr_tab_offset);
	if (uc_var_tab->entry_num || imp_var_tab->entry_num ||
	    imp_expr_tab->entry_num) {
		pr_err("QAT: UOF can't contain imported variable to be parsed");
		return EINVAL;
	}
	neigh_reg_tab =
	    (struct icp_qat_uof_objtable *)(encap_uof_obj->beg_uof +
					    code_page->neigh_reg_tab_offset);
	if (neigh_reg_tab->entry_num) {
		pr_err("QAT: UOF can't contain neighbor register table\n");
		return EINVAL;
	}
	if (image->numpages > 1) {
		pr_err("QAT: UOF can't contain multiple pages\n");
		return EINVAL;
	}
	if (RELOADABLE_CTX_SHARED_MODE(image->ae_mode)) {
		pr_err("QAT: UOF can't use reloadable feature\n");
		return EFAULT;
	}
	return 0;
}

static void
qat_uclo_map_image_page(struct icp_qat_uof_encap_obj *encap_uof_obj,
			struct icp_qat_uof_image *img,
			struct icp_qat_uclo_encap_page *page)
{
	struct icp_qat_uof_code_page *code_page;
	struct icp_qat_uof_code_area *code_area;
	struct icp_qat_uof_objtable *uword_block_tab;
	struct icp_qat_uof_uword_block *uwblock;
	int i;

	code_page =
	    (struct icp_qat_uof_code_page *)((char *)img +
					     sizeof(struct icp_qat_uof_image));
	page->def_page = code_page->def_page;
	page->page_region = code_page->page_region;
	page->beg_addr_v = code_page->beg_addr_v;
	page->beg_addr_p = code_page->beg_addr_p;
	code_area =
	    (struct icp_qat_uof_code_area *)(encap_uof_obj->beg_uof +
					     code_page->code_area_offset);
	page->micro_words_num = code_area->micro_words_num;
	uword_block_tab =
	    (struct icp_qat_uof_objtable *)(encap_uof_obj->beg_uof +
					    code_area->uword_block_tab);
	page->uwblock_num = uword_block_tab->entry_num;
	uwblock = (struct icp_qat_uof_uword_block
		       *)((char *)uword_block_tab +
			  sizeof(struct icp_qat_uof_objtable));
	page->uwblock = (struct icp_qat_uclo_encap_uwblock *)uwblock;
	for (i = 0; i < uword_block_tab->entry_num; i++)
		page->uwblock[i].micro_words =
		    (uintptr_t)encap_uof_obj->beg_uof + uwblock[i].uword_offset;
}

static int
qat_uclo_map_uimage(struct icp_qat_uclo_objhandle *obj_handle,
		    struct icp_qat_uclo_encapme *ae_uimage,
		    int max_image)
{
	int i, j;
	struct icp_qat_uof_chunkhdr *chunk_hdr = NULL;
	struct icp_qat_uof_image *image;
	struct icp_qat_uof_objtable *ae_regtab;
	struct icp_qat_uof_objtable *init_reg_sym_tab;
	struct icp_qat_uof_objtable *sbreak_tab;
	struct icp_qat_uof_encap_obj *encap_uof_obj =
	    &obj_handle->encap_uof_obj;

	for (j = 0; j < max_image; j++) {
		chunk_hdr = qat_uclo_find_chunk(encap_uof_obj->obj_hdr,
						ICP_QAT_UOF_IMAG,
						chunk_hdr);
		if (!chunk_hdr)
			break;
		image = (struct icp_qat_uof_image *)(encap_uof_obj->beg_uof +
						     chunk_hdr->offset);
		ae_regtab =
		    (struct icp_qat_uof_objtable *)(image->reg_tab_offset +
						    obj_handle->obj_hdr
							->file_buff);
		ae_uimage[j].ae_reg_num = ae_regtab->entry_num;
		ae_uimage[j].ae_reg =
		    (struct icp_qat_uof_ae_reg
			 *)(((char *)ae_regtab) +
			    sizeof(struct icp_qat_uof_objtable));
		init_reg_sym_tab =
		    (struct icp_qat_uof_objtable *)(image->init_reg_sym_tab +
						    obj_handle->obj_hdr
							->file_buff);
		ae_uimage[j].init_regsym_num = init_reg_sym_tab->entry_num;
		ae_uimage[j].init_regsym =
		    (struct icp_qat_uof_init_regsym
			 *)(((char *)init_reg_sym_tab) +
			    sizeof(struct icp_qat_uof_objtable));
		sbreak_tab = (struct icp_qat_uof_objtable *)(image->sbreak_tab +
							     obj_handle->obj_hdr
								 ->file_buff);
		ae_uimage[j].sbreak_num = sbreak_tab->entry_num;
		ae_uimage[j].sbreak =
		    (struct icp_qat_uof_sbreak
			 *)(((char *)sbreak_tab) +
			    sizeof(struct icp_qat_uof_objtable));
		ae_uimage[j].img_ptr = image;
		if (qat_uclo_check_image_compat(encap_uof_obj, image))
			goto out_err;
		ae_uimage[j].page =
		    malloc(sizeof(struct icp_qat_uclo_encap_page),
			   M_QAT,
			   M_WAITOK | M_ZERO);
		qat_uclo_map_image_page(encap_uof_obj,
					image,
					ae_uimage[j].page);
	}
	return j;
out_err:
	for (i = 0; i < j; i++)
		free(ae_uimage[i].page, M_QAT);
	return 0;
}

static int
qat_uclo_map_ae(struct icp_qat_fw_loader_handle *handle, int max_ae)
{
	int i;
	int ae = 0;
	unsigned long ae_mask = handle->hal_handle->ae_mask;
	unsigned long cfg_ae_mask = handle->cfg_ae_mask;
	int mflag = 0;
	struct icp_qat_uclo_objhandle *obj_handle = handle->obj_handle;

	for_each_set_bit(ae, &ae_mask, max_ae)
	{
		if (!test_bit(ae, &cfg_ae_mask))
			continue;

		for (i = 0; i < obj_handle->uimage_num; i++) {
			unsigned long ae_assigned =
			    obj_handle->ae_uimage[i].img_ptr->ae_assigned;
			if (!test_bit(ae, &ae_assigned))
				continue;
			mflag = 1;
			if (qat_uclo_init_ae_data(obj_handle, ae, i))
				return EINVAL;
		}
	}
	if (!mflag) {
		pr_err("QAT: uimage uses AE not set");
		return EINVAL;
	}
	return 0;
}

static struct icp_qat_uof_strtable *
qat_uclo_map_str_table(struct icp_qat_uclo_objhdr *obj_hdr,
		       char *tab_name,
		       struct icp_qat_uof_strtable *str_table)
{
	struct icp_qat_uof_chunkhdr *chunk_hdr;

	chunk_hdr =
	    qat_uclo_find_chunk((struct icp_qat_uof_objhdr *)obj_hdr->file_buff,
				tab_name,
				NULL);
	if (chunk_hdr) {
		int hdr_size;

		memcpy(&str_table->table_len,
		       obj_hdr->file_buff + chunk_hdr->offset,
		       sizeof(str_table->table_len));
		hdr_size = (char *)&str_table->strings - (char *)str_table;
		str_table->strings = (uintptr_t)obj_hdr->file_buff +
		    chunk_hdr->offset + hdr_size;
		return str_table;
	}
	return NULL;
}

static void
qat_uclo_map_initmem_table(struct icp_qat_uof_encap_obj *encap_uof_obj,
			   struct icp_qat_uclo_init_mem_table *init_mem_tab)
{
	struct icp_qat_uof_chunkhdr *chunk_hdr;

	chunk_hdr =
	    qat_uclo_find_chunk(encap_uof_obj->obj_hdr, ICP_QAT_UOF_IMEM, NULL);
	if (chunk_hdr) {
		memmove(&init_mem_tab->entry_num,
			encap_uof_obj->beg_uof + chunk_hdr->offset,
			sizeof(unsigned int));
		init_mem_tab->init_mem =
		    (struct icp_qat_uof_initmem *)(encap_uof_obj->beg_uof +
						   chunk_hdr->offset +
						   sizeof(unsigned int));
	}
}

static unsigned int
qat_uclo_get_dev_type(struct icp_qat_fw_loader_handle *handle)
{
	switch (pci_get_device(GET_DEV(handle->accel_dev))) {
	case ADF_DH895XCC_PCI_DEVICE_ID:
		return ICP_QAT_AC_895XCC_DEV_TYPE;
	case ADF_C62X_PCI_DEVICE_ID:
		return ICP_QAT_AC_C62X_DEV_TYPE;
	case ADF_C3XXX_PCI_DEVICE_ID:
		return ICP_QAT_AC_C3XXX_DEV_TYPE;
	case ADF_200XX_PCI_DEVICE_ID:
		return ICP_QAT_AC_200XX_DEV_TYPE;
	case ADF_C4XXX_PCI_DEVICE_ID:
		return ICP_QAT_AC_C4XXX_DEV_TYPE;
	default:
		pr_err("QAT: unsupported device 0x%x\n",
		       pci_get_device(GET_DEV(handle->accel_dev)));
		return 0;
	}
}

static int
qat_uclo_check_uof_compat(struct icp_qat_uclo_objhandle *obj_handle)
{
	unsigned int maj_ver, prod_type = obj_handle->prod_type;

	if (!(prod_type & obj_handle->encap_uof_obj.obj_hdr->ac_dev_type)) {
		pr_err("QAT: UOF type 0x%x doesn't match with platform 0x%x\n",
		       obj_handle->encap_uof_obj.obj_hdr->ac_dev_type,
		       prod_type);
		return EINVAL;
	}
	maj_ver = obj_handle->prod_rev & 0xff;
	if (obj_handle->encap_uof_obj.obj_hdr->max_cpu_ver < maj_ver ||
	    obj_handle->encap_uof_obj.obj_hdr->min_cpu_ver > maj_ver) {
		pr_err("QAT: UOF maj_ver 0x%x out of range\n", maj_ver);
		return EINVAL;
	}
	return 0;
}

static int
qat_uclo_init_reg(struct icp_qat_fw_loader_handle *handle,
		  unsigned char ae,
		  unsigned char ctx_mask,
		  enum icp_qat_uof_regtype reg_type,
		  unsigned short reg_addr,
		  unsigned int value)
{
	switch (reg_type) {
	case ICP_GPA_ABS:
	case ICP_GPB_ABS:
		ctx_mask = 0;
		return qat_hal_init_gpr(
		    handle, ae, ctx_mask, reg_type, reg_addr, value);
	case ICP_GPA_REL:
	case ICP_GPB_REL:
		return qat_hal_init_gpr(
		    handle, ae, ctx_mask, reg_type, reg_addr, value);
	case ICP_SR_ABS:
	case ICP_DR_ABS:
	case ICP_SR_RD_ABS:
	case ICP_DR_RD_ABS:
		ctx_mask = 0;
		return qat_hal_init_rd_xfer(
		    handle, ae, ctx_mask, reg_type, reg_addr, value);
	case ICP_SR_REL:
	case ICP_DR_REL:
	case ICP_SR_RD_REL:
	case ICP_DR_RD_REL:
		return qat_hal_init_rd_xfer(
		    handle, ae, ctx_mask, reg_type, reg_addr, value);
	case ICP_SR_WR_ABS:
	case ICP_DR_WR_ABS:
		ctx_mask = 0;
		return qat_hal_init_wr_xfer(
		    handle, ae, ctx_mask, reg_type, reg_addr, value);
	case ICP_SR_WR_REL:
	case ICP_DR_WR_REL:
		return qat_hal_init_wr_xfer(
		    handle, ae, ctx_mask, reg_type, reg_addr, value);
	case ICP_NEIGH_REL:
		return qat_hal_init_nn(handle, ae, ctx_mask, reg_addr, value);
	default:
		pr_err("QAT: UOF uses unsupported reg type 0x%x\n", reg_type);
		return EFAULT;
	}
	return 0;
}

static int
qat_uclo_init_reg_sym(struct icp_qat_fw_loader_handle *handle,
		      unsigned int ae,
		      struct icp_qat_uclo_encapme *encap_ae)
{
	unsigned int i;
	unsigned char ctx_mask;
	struct icp_qat_uof_init_regsym *init_regsym;

	if (ICP_QAT_CTX_MODE(encap_ae->img_ptr->ae_mode) ==
	    ICP_QAT_UCLO_MAX_CTX)
		ctx_mask = 0xff;
	else
		ctx_mask = 0x55;

	for (i = 0; i < encap_ae->init_regsym_num; i++) {
		unsigned int exp_res;

		init_regsym = &encap_ae->init_regsym[i];
		exp_res = init_regsym->value;
		switch (init_regsym->init_type) {
		case ICP_QAT_UOF_INIT_REG:
			qat_uclo_init_reg(handle,
					  ae,
					  ctx_mask,
					  (enum icp_qat_uof_regtype)
					      init_regsym->reg_type,
					  (unsigned short)init_regsym->reg_addr,
					  exp_res);
			break;
		case ICP_QAT_UOF_INIT_REG_CTX:
			/* check if ctx is appropriate for the ctxMode */
			if (!((1 << init_regsym->ctx) & ctx_mask)) {
				pr_err("QAT: invalid ctx num = 0x%x\n",
				       init_regsym->ctx);
				return EINVAL;
			}
			qat_uclo_init_reg(
			    handle,
			    ae,
			    (unsigned char)(1 << init_regsym->ctx),
			    (enum icp_qat_uof_regtype)init_regsym->reg_type,
			    (unsigned short)init_regsym->reg_addr,
			    exp_res);
			break;
		case ICP_QAT_UOF_INIT_EXPR:
			pr_err("QAT: INIT_EXPR feature not supported\n");
			return EINVAL;
		case ICP_QAT_UOF_INIT_EXPR_ENDIAN_SWAP:
			pr_err("QAT: INIT_EXPR_ENDIAN_SWAP not supported\n");
			return EINVAL;
		default:
			break;
		}
	}
	return 0;
}

static int
qat_uclo_init_globals(struct icp_qat_fw_loader_handle *handle)
{
	struct icp_qat_uclo_objhandle *obj_handle = handle->obj_handle;
	unsigned int s;
	unsigned int ae = 0;
	struct icp_qat_uclo_aedata *aed;
	unsigned long ae_mask = handle->hal_handle->ae_mask;

	if (obj_handle->global_inited)
		return 0;
	if (obj_handle->init_mem_tab.entry_num) {
		if (qat_uclo_init_memory(handle)) {
			pr_err("QAT: initialize memory failed\n");
			return EINVAL;
		}
	}

	for_each_set_bit(ae, &ae_mask, handle->hal_handle->ae_max_num)
	{
		aed = &obj_handle->ae_data[ae];
		for (s = 0; s < aed->slice_num; s++) {
			if (!aed->ae_slices[s].encap_image)
				continue;
			if (qat_uclo_init_reg_sym(
				handle, ae, aed->ae_slices[s].encap_image))
				return EINVAL;
		}
	}
	obj_handle->global_inited = 1;
	return 0;
}

static int
qat_hal_set_modes(struct icp_qat_fw_loader_handle *handle,
		  struct icp_qat_uclo_objhandle *obj_handle,
		  unsigned char ae,
		  struct icp_qat_uof_image *uof_image)
{
	unsigned char nn_mode;
	char ae_mode = 0;

	ae_mode = (char)ICP_QAT_CTX_MODE(uof_image->ae_mode);
	if (qat_hal_set_ae_ctx_mode(handle, ae, ae_mode)) {
		pr_err("QAT: qat_hal_set_ae_ctx_mode error\n");
		return EFAULT;
	}

	ae_mode = (char)ICP_QAT_SHARED_USTORE_MODE(uof_image->ae_mode);
	qat_hal_set_ae_scs_mode(handle, ae, ae_mode);
	nn_mode = ICP_QAT_NN_MODE(uof_image->ae_mode);

	if (qat_hal_set_ae_nn_mode(handle, ae, nn_mode)) {
		pr_err("QAT: qat_hal_set_ae_nn_mode error\n");
		return EFAULT;
	}
	ae_mode = (char)ICP_QAT_LOC_MEM0_MODE(uof_image->ae_mode);
	if (qat_hal_set_ae_lm_mode(handle, ae, ICP_LMEM0, ae_mode)) {
		pr_err("QAT: qat_hal_set_ae_lm_mode LMEM0 error\n");
		return EFAULT;
	}
	ae_mode = (char)ICP_QAT_LOC_MEM1_MODE(uof_image->ae_mode);
	if (qat_hal_set_ae_lm_mode(handle, ae, ICP_LMEM1, ae_mode)) {
		pr_err("QAT: qat_hal_set_ae_lm_mode LMEM1 error\n");
		return EFAULT;
	}
	if (obj_handle->prod_type == ICP_QAT_AC_C4XXX_DEV_TYPE) {
		ae_mode = (char)ICP_QAT_LOC_MEM2_MODE(uof_image->ae_mode);
		if (qat_hal_set_ae_lm_mode(handle, ae, ICP_LMEM2, ae_mode)) {
			pr_err("QAT: qat_hal_set_ae_lm_mode LMEM2 error\n");
			return EFAULT;
		}
		ae_mode = (char)ICP_QAT_LOC_MEM3_MODE(uof_image->ae_mode);
		if (qat_hal_set_ae_lm_mode(handle, ae, ICP_LMEM3, ae_mode)) {
			pr_err("QAT: qat_hal_set_ae_lm_mode LMEM3 error\n");
			return EFAULT;
		}
		ae_mode = (char)ICP_QAT_LOC_TINDEX_MODE(uof_image->ae_mode);
		qat_hal_set_ae_tindex_mode(handle, ae, ae_mode);
	}
	return 0;
}

static int
qat_uclo_set_ae_mode(struct icp_qat_fw_loader_handle *handle)
{
	int error;
	unsigned char s;
	unsigned char ae = 0;
	struct icp_qat_uof_image *uof_image;
	struct icp_qat_uclo_aedata *ae_data;
	struct icp_qat_uclo_objhandle *obj_handle = handle->obj_handle;
	unsigned long ae_mask = handle->hal_handle->ae_mask;

	for_each_set_bit(ae, &ae_mask, handle->hal_handle->ae_max_num)
	{
		unsigned long cfg_ae_mask = handle->cfg_ae_mask;

		if (!test_bit(ae, &cfg_ae_mask))
			continue;

		ae_data = &obj_handle->ae_data[ae];
		for (s = 0; s < min_t(unsigned int,
				      ae_data->slice_num,
				      ICP_QAT_UCLO_MAX_CTX);
		     s++) {
			if (!obj_handle->ae_data[ae].ae_slices[s].encap_image)
				continue;
			uof_image = ae_data->ae_slices[s].encap_image->img_ptr;
			error = qat_hal_set_modes(handle,
						  obj_handle,
						  ae,
						  uof_image);
			if (error)
				return error;
		}
	}
	return 0;
}

static void
qat_uclo_init_uword_num(struct icp_qat_fw_loader_handle *handle)
{
	struct icp_qat_uclo_objhandle *obj_handle = handle->obj_handle;
	struct icp_qat_uclo_encapme *image;
	int a;

	for (a = 0; a < obj_handle->uimage_num; a++) {
		image = &obj_handle->ae_uimage[a];
		image->uwords_num =
		    image->page->beg_addr_p + image->page->micro_words_num;
	}
}

static int
qat_uclo_parse_uof_obj(struct icp_qat_fw_loader_handle *handle)
{
	struct icp_qat_uclo_objhandle *obj_handle = handle->obj_handle;
	unsigned int ae;

	obj_handle->encap_uof_obj.beg_uof = obj_handle->obj_hdr->file_buff;
	obj_handle->encap_uof_obj.obj_hdr =
	    (struct icp_qat_uof_objhdr *)obj_handle->obj_hdr->file_buff;
	obj_handle->uword_in_bytes = 6;
	obj_handle->prod_type = qat_uclo_get_dev_type(handle);
	obj_handle->prod_rev =
	    PID_MAJOR_REV | (PID_MINOR_REV & handle->hal_handle->revision_id);
	if (qat_uclo_check_uof_compat(obj_handle)) {
		pr_err("QAT: UOF incompatible\n");
		return EINVAL;
	}
	obj_handle->uword_buf = malloc(UWORD_CPYBUF_SIZE * sizeof(uint64_t),
				       M_QAT,
				       M_WAITOK | M_ZERO);
	obj_handle->ustore_phy_size =
	    (obj_handle->prod_type == ICP_QAT_AC_C4XXX_DEV_TYPE) ? 0x2000 :
								   0x4000;
	if (!obj_handle->obj_hdr->file_buff ||
	    !qat_uclo_map_str_table(obj_handle->obj_hdr,
				    ICP_QAT_UOF_STRT,
				    &obj_handle->str_table)) {
		pr_err("QAT: UOF doesn't have effective images\n");
		goto out_err;
	}
	obj_handle->uimage_num =
	    qat_uclo_map_uimage(obj_handle,
				obj_handle->ae_uimage,
				ICP_QAT_UCLO_MAX_AE * ICP_QAT_UCLO_MAX_CTX);
	if (!obj_handle->uimage_num)
		goto out_err;
	if (qat_uclo_map_ae(handle, handle->hal_handle->ae_max_num)) {
		pr_err("QAT: Bad object\n");
		goto out_check_uof_aemask_err;
	}
	qat_uclo_init_uword_num(handle);
	qat_uclo_map_initmem_table(&obj_handle->encap_uof_obj,
				   &obj_handle->init_mem_tab);
	if (qat_uclo_set_ae_mode(handle))
		goto out_check_uof_aemask_err;
	return 0;
out_check_uof_aemask_err:
	for (ae = 0; ae < obj_handle->uimage_num; ae++)
		free(obj_handle->ae_uimage[ae].page, M_QAT);
out_err:
	free(obj_handle->uword_buf, M_QAT);
	obj_handle->uword_buf = NULL;
	return EFAULT;
}

static int
qat_uclo_map_suof_file_hdr(const struct icp_qat_fw_loader_handle *handle,
			   const struct icp_qat_suof_filehdr *suof_ptr,
			   int suof_size)
{
	unsigned int check_sum = 0;
	unsigned int min_ver_offset = 0;
	struct icp_qat_suof_handle *suof_handle = handle->sobj_handle;

	suof_handle->file_id = ICP_QAT_SUOF_FID;
	suof_handle->suof_buf = (const char *)suof_ptr;
	suof_handle->suof_size = suof_size;
	min_ver_offset =
	    suof_size - offsetof(struct icp_qat_suof_filehdr, min_ver);
	check_sum = qat_uclo_calc_str_checksum((const char *)&suof_ptr->min_ver,
					       min_ver_offset);
	if (check_sum != suof_ptr->check_sum) {
		pr_err("QAT: incorrect SUOF checksum\n");
		return EINVAL;
	}
	suof_handle->check_sum = suof_ptr->check_sum;
	suof_handle->min_ver = suof_ptr->min_ver;
	suof_handle->maj_ver = suof_ptr->maj_ver;
	suof_handle->fw_type = suof_ptr->fw_type;
	return 0;
}

static void
qat_uclo_map_simg(struct icp_qat_suof_handle *suof_handle,
		  struct icp_qat_suof_img_hdr *suof_img_hdr,
		  struct icp_qat_suof_chunk_hdr *suof_chunk_hdr)
{
	const struct icp_qat_simg_ae_mode *ae_mode;
	struct icp_qat_suof_objhdr *suof_objhdr;

	suof_img_hdr->simg_buf =
	    (suof_handle->suof_buf + suof_chunk_hdr->offset +
	     sizeof(*suof_objhdr));
	suof_img_hdr->simg_len =
	    ((struct icp_qat_suof_objhdr *)(uintptr_t)(suof_handle->suof_buf +
						       suof_chunk_hdr->offset))
		->img_length;

	suof_img_hdr->css_header = suof_img_hdr->simg_buf;
	suof_img_hdr->css_key =
	    (suof_img_hdr->css_header + sizeof(struct icp_qat_css_hdr));
	suof_img_hdr->css_signature = suof_img_hdr->css_key +
	    ICP_QAT_CSS_FWSK_MODULUS_LEN + ICP_QAT_CSS_FWSK_EXPONENT_LEN;
	suof_img_hdr->css_simg =
	    suof_img_hdr->css_signature + ICP_QAT_CSS_SIGNATURE_LEN;

	ae_mode = (const struct icp_qat_simg_ae_mode *)(suof_img_hdr->css_simg);
	suof_img_hdr->ae_mask = ae_mode->ae_mask;
	suof_img_hdr->simg_name = (unsigned long)&ae_mode->simg_name;
	suof_img_hdr->appmeta_data = (unsigned long)&ae_mode->appmeta_data;
	suof_img_hdr->fw_type = ae_mode->fw_type;
}

static void
qat_uclo_map_suof_symobjs(struct icp_qat_suof_handle *suof_handle,
			  struct icp_qat_suof_chunk_hdr *suof_chunk_hdr)
{
	char **sym_str = (char **)&suof_handle->sym_str;
	unsigned int *sym_size = &suof_handle->sym_size;
	struct icp_qat_suof_strtable *str_table_obj;

	*sym_size = *(unsigned int *)(uintptr_t)(suof_chunk_hdr->offset +
						 suof_handle->suof_buf);
	*sym_str =
	    (char *)(uintptr_t)(suof_handle->suof_buf + suof_chunk_hdr->offset +
				sizeof(str_table_obj->tab_length));
}

static int
qat_uclo_check_simg_compat(struct icp_qat_fw_loader_handle *handle,
			   struct icp_qat_suof_img_hdr *img_hdr)
{
	const struct icp_qat_simg_ae_mode *img_ae_mode = NULL;
	unsigned int prod_rev, maj_ver, prod_type;

	prod_type = qat_uclo_get_dev_type(handle);
	img_ae_mode = (const struct icp_qat_simg_ae_mode *)img_hdr->css_simg;
	prod_rev =
	    PID_MAJOR_REV | (PID_MINOR_REV & handle->hal_handle->revision_id);
	if (img_ae_mode->dev_type != prod_type) {
		pr_err("QAT: incompatible product type %x\n",
		       img_ae_mode->dev_type);
		return EINVAL;
	}
	maj_ver = prod_rev & 0xff;
	if (maj_ver > img_ae_mode->devmax_ver ||
	    maj_ver < img_ae_mode->devmin_ver) {
		pr_err("QAT: incompatible device maj_ver 0x%x\n", maj_ver);
		return EINVAL;
	}
	return 0;
}

static void
qat_uclo_del_suof(struct icp_qat_fw_loader_handle *handle)
{
	struct icp_qat_suof_handle *sobj_handle = handle->sobj_handle;

	free(sobj_handle->img_table.simg_hdr, M_QAT);
	sobj_handle->img_table.simg_hdr = NULL;
	free(handle->sobj_handle, M_QAT);
	handle->sobj_handle = NULL;
}

static void
qat_uclo_tail_img(struct icp_qat_suof_img_hdr *suof_img_hdr,
		  unsigned int img_id,
		  unsigned int num_simgs)
{
	struct icp_qat_suof_img_hdr img_header;

	if ((img_id != num_simgs - 1) && img_id != ICP_QAT_UCLO_MAX_AE) {
		memcpy(&img_header,
		       &suof_img_hdr[num_simgs - 1],
		       sizeof(*suof_img_hdr));
		memcpy(&suof_img_hdr[num_simgs - 1],
		       &suof_img_hdr[img_id],
		       sizeof(*suof_img_hdr));
		memcpy(&suof_img_hdr[img_id],
		       &img_header,
		       sizeof(*suof_img_hdr));
	}
}

static int
qat_uclo_map_suof(struct icp_qat_fw_loader_handle *handle,
		  const struct icp_qat_suof_filehdr *suof_ptr,
		  int suof_size)
{
	struct icp_qat_suof_handle *suof_handle = handle->sobj_handle;
	struct icp_qat_suof_chunk_hdr *suof_chunk_hdr = NULL;
	struct icp_qat_suof_img_hdr *suof_img_hdr = NULL;
	int ret = 0, ae0_img = ICP_QAT_UCLO_MAX_AE;
	unsigned int i = 0;
	struct icp_qat_suof_img_hdr img_header;

	if (!suof_ptr || suof_size == 0) {
		pr_err("QAT: input parameter SUOF pointer/size is NULL\n");
		return EINVAL;
	}
	if (qat_uclo_check_suof_format(suof_ptr))
		return EINVAL;
	ret = qat_uclo_map_suof_file_hdr(handle, suof_ptr, suof_size);
	if (ret)
		return ret;
	suof_chunk_hdr = (struct icp_qat_suof_chunk_hdr *)((uintptr_t)suof_ptr +
							   sizeof(*suof_ptr));

	qat_uclo_map_suof_symobjs(suof_handle, suof_chunk_hdr);
	suof_handle->img_table.num_simgs = suof_ptr->num_chunks - 1;

	if (suof_handle->img_table.num_simgs != 0) {
		suof_img_hdr = malloc(suof_handle->img_table.num_simgs *
					  sizeof(img_header),
				      M_QAT,
				      M_WAITOK | M_ZERO);
		suof_handle->img_table.simg_hdr = suof_img_hdr;
	}

	for (i = 0; i < suof_handle->img_table.num_simgs; i++) {
		qat_uclo_map_simg(handle->sobj_handle,
				  &suof_img_hdr[i],
				  &suof_chunk_hdr[1 + i]);
		ret = qat_uclo_check_simg_compat(handle, &suof_img_hdr[i]);
		if (ret)
			return ret;
		suof_img_hdr[i].ae_mask &= handle->cfg_ae_mask;
		if ((suof_img_hdr[i].ae_mask & 0x1) != 0)
			ae0_img = i;
	}
	qat_uclo_tail_img(suof_img_hdr,
			  ae0_img,
			  suof_handle->img_table.num_simgs);
	return 0;
}

#define ADD_ADDR(high, low) ((((uint64_t)high) << 32) + (low))
#define BITS_IN_DWORD 32

static int
qat_uclo_auth_fw(struct icp_qat_fw_loader_handle *handle,
		 struct icp_qat_fw_auth_desc *desc)
{
	unsigned int fcu_sts, mem_cfg_err, retry = 0;
	unsigned int fcu_ctl_csr, fcu_sts_csr;
	unsigned int fcu_dram_hi_csr, fcu_dram_lo_csr;
	u64 bus_addr;

	bus_addr = ADD_ADDR(desc->css_hdr_high, desc->css_hdr_low) -
	    sizeof(struct icp_qat_auth_chunk);
	if (IS_QAT_GEN3(pci_get_device(GET_DEV(handle->accel_dev)))) {
		fcu_ctl_csr = FCU_CONTROL_C4XXX;
		fcu_sts_csr = FCU_STATUS_C4XXX;
		fcu_dram_hi_csr = FCU_DRAM_ADDR_HI_C4XXX;
		fcu_dram_lo_csr = FCU_DRAM_ADDR_LO_C4XXX;
	} else {
		fcu_ctl_csr = FCU_CONTROL;
		fcu_sts_csr = FCU_STATUS;
		fcu_dram_hi_csr = FCU_DRAM_ADDR_HI;
		fcu_dram_lo_csr = FCU_DRAM_ADDR_LO;
	}
	SET_FCU_CSR(handle, fcu_dram_hi_csr, (bus_addr >> BITS_IN_DWORD));
	SET_FCU_CSR(handle, fcu_dram_lo_csr, bus_addr);
	SET_FCU_CSR(handle, fcu_ctl_csr, FCU_CTRL_CMD_AUTH);

	do {
		pause_ms("adfstop", FW_AUTH_WAIT_PERIOD);
		fcu_sts = GET_FCU_CSR(handle, fcu_sts_csr);
		if ((fcu_sts & FCU_AUTH_STS_MASK) == FCU_STS_VERI_FAIL)
			goto auth_fail;
		if (((fcu_sts >> FCU_STS_AUTHFWLD_POS) & 0x1))
			if ((fcu_sts & FCU_AUTH_STS_MASK) == FCU_STS_VERI_DONE)
				return 0;
	} while (retry++ < FW_AUTH_MAX_RETRY);
auth_fail:
	pr_err("QAT: authentication error (FCU_STATUS = 0x%x),retry = %d\n",
	       fcu_sts & FCU_AUTH_STS_MASK,
	       retry);
	if (IS_QAT_GEN3(pci_get_device(GET_DEV(handle->accel_dev)))) {
		mem_cfg_err =
		    (GET_FCU_CSR(handle, FCU_STATUS1_C4XXX) & MEM_CFG_ERR_BIT);
		if (mem_cfg_err)
			pr_err("QAT: MEM_CFG_ERR\n");
	}
	return EINVAL;
}

static int
qat_uclo_simg_alloc(struct icp_qat_fw_loader_handle *handle,
		    struct icp_firml_dram_desc *dram_desc,
		    unsigned int size)
{
	int ret;

	ret = bus_dma_mem_create(&dram_desc->dram_mem,
				 handle->accel_dev->dma_tag,
				 1,
				 BUS_SPACE_MAXADDR,
				 size,
				 0);
	if (ret != 0)
		return ret;
	dram_desc->dram_base_addr_v = dram_desc->dram_mem.dma_vaddr;
	dram_desc->dram_bus_addr = dram_desc->dram_mem.dma_baddr;
	dram_desc->dram_size = size;
	return 0;
}

static void
qat_uclo_simg_free(struct icp_qat_fw_loader_handle *handle,
		   struct icp_firml_dram_desc *dram_desc)
{
	if (handle && dram_desc && dram_desc->dram_base_addr_v)
		bus_dma_mem_free(&dram_desc->dram_mem);

	if (dram_desc)
		explicit_bzero(dram_desc, sizeof(*dram_desc));
}

static int
qat_uclo_map_auth_fw(struct icp_qat_fw_loader_handle *handle,
		     const char *image,
		     unsigned int size,
		     struct icp_firml_dram_desc *img_desc,
		     struct icp_qat_fw_auth_desc **desc)
{
	const struct icp_qat_css_hdr *css_hdr =
	    (const struct icp_qat_css_hdr *)image;
	struct icp_qat_fw_auth_desc *auth_desc;
	struct icp_qat_auth_chunk *auth_chunk;
	u64 virt_addr, bus_addr, virt_base;
	unsigned int length, simg_offset = sizeof(*auth_chunk);

	if (size > (ICP_QAT_AE_IMG_OFFSET + ICP_QAT_CSS_MAX_IMAGE_LEN)) {
		pr_err("QAT: error, input image size overflow %d\n", size);
		return EINVAL;
	}
	length = (css_hdr->fw_type == CSS_AE_FIRMWARE) ?
	    ICP_QAT_CSS_AE_SIMG_LEN + simg_offset :
	    size + ICP_QAT_CSS_FWSK_PAD_LEN + simg_offset;
	if (qat_uclo_simg_alloc(handle, img_desc, length)) {
		pr_err("QAT: error, allocate continuous dram fail\n");
		return -ENOMEM;
	}

	auth_chunk = img_desc->dram_base_addr_v;
	auth_chunk->chunk_size = img_desc->dram_size;
	auth_chunk->chunk_bus_addr = img_desc->dram_bus_addr;
	virt_base = (uintptr_t)img_desc->dram_base_addr_v + simg_offset;
	bus_addr = img_desc->dram_bus_addr + simg_offset;
	auth_desc = img_desc->dram_base_addr_v;
	auth_desc->css_hdr_high = (unsigned int)(bus_addr >> BITS_IN_DWORD);
	auth_desc->css_hdr_low = (unsigned int)bus_addr;
	virt_addr = virt_base;

	memcpy((void *)(uintptr_t)virt_addr, image, sizeof(*css_hdr));
	/* pub key */
	bus_addr = ADD_ADDR(auth_desc->css_hdr_high, auth_desc->css_hdr_low) +
	    sizeof(*css_hdr);
	virt_addr = virt_addr + sizeof(*css_hdr);

	auth_desc->fwsk_pub_high = (unsigned int)(bus_addr >> BITS_IN_DWORD);
	auth_desc->fwsk_pub_low = (unsigned int)bus_addr;

	memcpy((void *)(uintptr_t)virt_addr,
	       (const void *)(image + sizeof(*css_hdr)),
	       ICP_QAT_CSS_FWSK_MODULUS_LEN);
	/* padding */
	explicit_bzero((void *)(uintptr_t)(virt_addr +
					   ICP_QAT_CSS_FWSK_MODULUS_LEN),
		       ICP_QAT_CSS_FWSK_PAD_LEN);

	/* exponent */
	memcpy((void *)(uintptr_t)(virt_addr + ICP_QAT_CSS_FWSK_MODULUS_LEN +
				   ICP_QAT_CSS_FWSK_PAD_LEN),
	       (const void *)(image + sizeof(*css_hdr) +
			      ICP_QAT_CSS_FWSK_MODULUS_LEN),
	       sizeof(unsigned int));

	/* signature */
	bus_addr = ADD_ADDR(auth_desc->fwsk_pub_high, auth_desc->fwsk_pub_low) +
	    ICP_QAT_CSS_FWSK_PUB_LEN;
	virt_addr = virt_addr + ICP_QAT_CSS_FWSK_PUB_LEN;
	auth_desc->signature_high = (unsigned int)(bus_addr >> BITS_IN_DWORD);
	auth_desc->signature_low = (unsigned int)bus_addr;

	memcpy((void *)(uintptr_t)virt_addr,
	       (const void *)(image + sizeof(*css_hdr) +
			      ICP_QAT_CSS_FWSK_MODULUS_LEN +
			      ICP_QAT_CSS_FWSK_EXPONENT_LEN),
	       ICP_QAT_CSS_SIGNATURE_LEN);

	bus_addr =
	    ADD_ADDR(auth_desc->signature_high, auth_desc->signature_low) +
	    ICP_QAT_CSS_SIGNATURE_LEN;
	virt_addr += ICP_QAT_CSS_SIGNATURE_LEN;

	auth_desc->img_high = (unsigned int)(bus_addr >> BITS_IN_DWORD);
	auth_desc->img_low = (unsigned int)bus_addr;
	auth_desc->img_len = size - ICP_QAT_AE_IMG_OFFSET;
	memcpy((void *)(uintptr_t)virt_addr,
	       (const void *)(image + ICP_QAT_AE_IMG_OFFSET),
	       auth_desc->img_len);
	virt_addr = virt_base;
	/* AE firmware */
	if (((struct icp_qat_css_hdr *)(uintptr_t)virt_addr)->fw_type ==
	    CSS_AE_FIRMWARE) {
		auth_desc->img_ae_mode_data_high = auth_desc->img_high;
		auth_desc->img_ae_mode_data_low = auth_desc->img_low;
		bus_addr = ADD_ADDR(auth_desc->img_ae_mode_data_high,
				    auth_desc->img_ae_mode_data_low) +
		    sizeof(struct icp_qat_simg_ae_mode);

		auth_desc->img_ae_init_data_high =
		    (unsigned int)(bus_addr >> BITS_IN_DWORD);
		auth_desc->img_ae_init_data_low = (unsigned int)bus_addr;
		bus_addr += ICP_QAT_SIMG_AE_INIT_SEQ_LEN;
		auth_desc->img_ae_insts_high =
		    (unsigned int)(bus_addr >> BITS_IN_DWORD);
		auth_desc->img_ae_insts_low = (unsigned int)bus_addr;
		virt_addr += sizeof(struct icp_qat_css_hdr) +
		    ICP_QAT_CSS_FWSK_PUB_LEN + ICP_QAT_CSS_SIGNATURE_LEN;
		auth_desc->ae_mask =
		    ((struct icp_qat_simg_ae_mode *)virt_addr)->ae_mask &
		    handle->cfg_ae_mask;
	} else {
		auth_desc->img_ae_insts_high = auth_desc->img_high;
		auth_desc->img_ae_insts_low = auth_desc->img_low;
	}
	*desc = auth_desc;
	return 0;
}

static int
qat_uclo_load_fw(struct icp_qat_fw_loader_handle *handle,
		 struct icp_qat_fw_auth_desc *desc)
{
	unsigned int i = 0;
	unsigned int fcu_sts;
	unsigned int fcu_sts_csr, fcu_ctl_csr;
	unsigned int loaded_aes = FCU_LOADED_AE_POS;
	unsigned long ae_mask = handle->hal_handle->ae_mask;

	if (IS_QAT_GEN3(pci_get_device(GET_DEV(handle->accel_dev)))) {
		fcu_ctl_csr = FCU_CONTROL_C4XXX;
		fcu_sts_csr = FCU_STATUS_C4XXX;

	} else {
		fcu_ctl_csr = FCU_CONTROL;
		fcu_sts_csr = FCU_STATUS;
	}

	for_each_set_bit(i, &ae_mask, handle->hal_handle->ae_max_num)
	{
		int retry = 0;

		if (!((desc->ae_mask >> i) & 0x1))
			continue;
		if (qat_hal_check_ae_active(handle, i)) {
			pr_err("QAT: AE %d is active\n", i);
			return EINVAL;
		}
		SET_FCU_CSR(handle,
			    fcu_ctl_csr,
			    (FCU_CTRL_CMD_LOAD | (i << FCU_CTRL_AE_POS)));

		do {
			pause_ms("adfstop", FW_AUTH_WAIT_PERIOD);
			fcu_sts = GET_FCU_CSR(handle, fcu_sts_csr);
			if ((fcu_sts & FCU_AUTH_STS_MASK) ==
			    FCU_STS_LOAD_DONE) {
				loaded_aes = IS_QAT_GEN3(pci_get_device(
						 GET_DEV(handle->accel_dev))) ?
				    GET_FCU_CSR(handle, FCU_AE_LOADED_C4XXX) :
				    (fcu_sts >> FCU_LOADED_AE_POS);
				if (loaded_aes & (1 << i))
					break;
			}
		} while (retry++ < FW_AUTH_MAX_RETRY);
		if (retry > FW_AUTH_MAX_RETRY) {
			pr_err("QAT: firmware load failed timeout %x\n", retry);
			return EINVAL;
		}
	}
	return 0;
}

static int
qat_uclo_map_suof_obj(struct icp_qat_fw_loader_handle *handle,
		      const void *addr_ptr,
		      int mem_size)
{
	struct icp_qat_suof_handle *suof_handle;

	suof_handle = malloc(sizeof(*suof_handle), M_QAT, M_WAITOK | M_ZERO);
	handle->sobj_handle = suof_handle;
	if (qat_uclo_map_suof(handle, addr_ptr, mem_size)) {
		qat_uclo_del_suof(handle);
		pr_err("QAT: map SUOF failed\n");
		return EINVAL;
	}
	return 0;
}

int
qat_uclo_wr_mimage(struct icp_qat_fw_loader_handle *handle,
		   const void *addr_ptr,
		   int mem_size)
{
	struct icp_qat_fw_auth_desc *desc = NULL;
	struct icp_firml_dram_desc img_desc;
	int status = 0;

	if (handle->fw_auth) {
		status = qat_uclo_map_auth_fw(
		    handle, addr_ptr, mem_size, &img_desc, &desc);
		if (!status)
			status = qat_uclo_auth_fw(handle, desc);

		qat_uclo_simg_free(handle, &img_desc);
	} else {
		if (pci_get_device(GET_DEV(handle->accel_dev)) ==
		    ADF_C3XXX_PCI_DEVICE_ID) {
			pr_err("QAT: C3XXX doesn't support unsigned MMP\n");
			return EINVAL;
		}
		status = qat_uclo_wr_sram_by_words(handle,
						   handle->hal_sram_offset,
						   addr_ptr,
						   mem_size);
	}
	return status;
}

static int
qat_uclo_map_uof_obj(struct icp_qat_fw_loader_handle *handle,
		     const void *addr_ptr,
		     int mem_size)
{
	struct icp_qat_uof_filehdr *filehdr;
	struct icp_qat_uclo_objhandle *objhdl;

	objhdl = malloc(sizeof(*objhdl), M_QAT, M_WAITOK | M_ZERO);
	objhdl->obj_buf = malloc(mem_size, M_QAT, M_WAITOK);
	bcopy(addr_ptr, objhdl->obj_buf, mem_size);
	filehdr = (struct icp_qat_uof_filehdr *)objhdl->obj_buf;
	if (qat_uclo_check_uof_format(filehdr))
		goto out_objhdr_err;
	objhdl->obj_hdr = qat_uclo_map_chunk((char *)objhdl->obj_buf,
					     filehdr,
					     ICP_QAT_UOF_OBJS);
	if (!objhdl->obj_hdr) {
		pr_err("QAT: object file chunk is null\n");
		goto out_objhdr_err;
	}
	handle->obj_handle = objhdl;
	if (qat_uclo_parse_uof_obj(handle))
		goto out_overlay_obj_err;
	return 0;

out_overlay_obj_err:
	handle->obj_handle = NULL;
	free(objhdl->obj_hdr, M_QAT);
out_objhdr_err:
	free(objhdl->obj_buf, M_QAT);
	free(objhdl, M_QAT);
	return ENOMEM;
}

static int
qat_uclo_map_mof_file_hdr(struct icp_qat_fw_loader_handle *handle,
			  const struct icp_qat_mof_file_hdr *mof_ptr,
			  u32 mof_size)
{
	unsigned int checksum = 0;
	unsigned int min_ver_offset = 0;
	struct icp_qat_mof_handle *mobj_handle = handle->mobj_handle;

	mobj_handle->file_id = ICP_QAT_MOF_FID;
	mobj_handle->mof_buf = (const char *)mof_ptr;
	mobj_handle->mof_size = mof_size;

	min_ver_offset =
	    mof_size - offsetof(struct icp_qat_mof_file_hdr, min_ver);
	checksum = qat_uclo_calc_str_checksum((const char *)&mof_ptr->min_ver,
					      min_ver_offset);
	if (checksum != mof_ptr->checksum) {
		pr_err("QAT: incorrect MOF checksum\n");
		return EINVAL;
	}
	mobj_handle->checksum = mof_ptr->checksum;
	mobj_handle->min_ver = mof_ptr->min_ver;
	mobj_handle->maj_ver = mof_ptr->maj_ver;
	return 0;
}

void
qat_uclo_del_mof(struct icp_qat_fw_loader_handle *handle)
{
	struct icp_qat_mof_handle *mobj_handle = handle->mobj_handle;

	free(mobj_handle->obj_table.obj_hdr, M_QAT);
	mobj_handle->obj_table.obj_hdr = NULL;
	free(handle->mobj_handle, M_QAT);
	handle->mobj_handle = NULL;
}

static int
qat_uclo_seek_obj_inside_mof(struct icp_qat_mof_handle *mobj_handle,
			     const char *obj_name,
			     const char **obj_ptr,
			     unsigned int *obj_size)
{
	unsigned int i;
	struct icp_qat_mof_objhdr *obj_hdr = mobj_handle->obj_table.obj_hdr;

	for (i = 0; i < mobj_handle->obj_table.num_objs; i++) {
		if (!strncmp(obj_hdr[i].obj_name,
			     obj_name,
			     ICP_QAT_SUOF_OBJ_NAME_LEN)) {
			*obj_ptr = obj_hdr[i].obj_buf;
			*obj_size = obj_hdr[i].obj_size;
			break;
		}
	}

	if (i >= mobj_handle->obj_table.num_objs) {
		pr_err("QAT: object %s is not found inside MOF\n", obj_name);
		return EFAULT;
	}
	return 0;
}

static int
qat_uclo_map_obj_from_mof(struct icp_qat_mof_handle *mobj_handle,
			  struct icp_qat_mof_objhdr *mobj_hdr,
			  struct icp_qat_mof_obj_chunkhdr *obj_chunkhdr)
{
	if ((strncmp((char *)obj_chunkhdr->chunk_id,
		     ICP_QAT_UOF_IMAG,
		     ICP_QAT_MOF_OBJ_CHUNKID_LEN)) == 0) {
		mobj_hdr->obj_buf =
		    (const char *)((unsigned long)obj_chunkhdr->offset +
				   mobj_handle->uobjs_hdr);
	} else if ((strncmp((char *)(obj_chunkhdr->chunk_id),
			    ICP_QAT_SUOF_IMAG,
			    ICP_QAT_MOF_OBJ_CHUNKID_LEN)) == 0) {
		mobj_hdr->obj_buf =
		    (const char *)((unsigned long)obj_chunkhdr->offset +
				   mobj_handle->sobjs_hdr);

	} else {
		pr_err("QAT: unsupported chunk id\n");
		return EINVAL;
	}
	mobj_hdr->obj_size = (unsigned int)obj_chunkhdr->size;
	mobj_hdr->obj_name =
	    (char *)(obj_chunkhdr->name + mobj_handle->sym_str);
	return 0;
}

static int
qat_uclo_map_objs_from_mof(struct icp_qat_mof_handle *mobj_handle)
{
	struct icp_qat_mof_objhdr *mof_obj_hdr;
	const struct icp_qat_mof_obj_hdr *uobj_hdr;
	const struct icp_qat_mof_obj_hdr *sobj_hdr;
	struct icp_qat_mof_obj_chunkhdr *uobj_chunkhdr;
	struct icp_qat_mof_obj_chunkhdr *sobj_chunkhdr;
	unsigned int uobj_chunk_num = 0, sobj_chunk_num = 0;
	unsigned int *valid_chunks = 0;
	int ret, i;

	uobj_hdr = (const struct icp_qat_mof_obj_hdr *)mobj_handle->uobjs_hdr;
	sobj_hdr = (const struct icp_qat_mof_obj_hdr *)mobj_handle->sobjs_hdr;
	if (uobj_hdr)
		uobj_chunk_num = uobj_hdr->num_chunks;
	if (sobj_hdr)
		sobj_chunk_num = sobj_hdr->num_chunks;

	mof_obj_hdr = (struct icp_qat_mof_objhdr *)
	    malloc((uobj_chunk_num + sobj_chunk_num) * sizeof(*mof_obj_hdr),
		   M_QAT,
		   M_WAITOK | M_ZERO);

	mobj_handle->obj_table.obj_hdr = mof_obj_hdr;
	valid_chunks = &mobj_handle->obj_table.num_objs;
	uobj_chunkhdr =
	    (struct icp_qat_mof_obj_chunkhdr *)((uintptr_t)uobj_hdr +
						sizeof(*uobj_hdr));
	sobj_chunkhdr =
	    (struct icp_qat_mof_obj_chunkhdr *)((uintptr_t)sobj_hdr +
						sizeof(*sobj_hdr));

	/* map uof objects */
	for (i = 0; i < uobj_chunk_num; i++) {
		ret = qat_uclo_map_obj_from_mof(mobj_handle,
						&mof_obj_hdr[*valid_chunks],
						&uobj_chunkhdr[i]);
		if (ret)
			return ret;
		(*valid_chunks)++;
	}

	/* map suof objects */
	for (i = 0; i < sobj_chunk_num; i++) {
		ret = qat_uclo_map_obj_from_mof(mobj_handle,
						&mof_obj_hdr[*valid_chunks],
						&sobj_chunkhdr[i]);
		if (ret)
			return ret;
		(*valid_chunks)++;
	}

	if ((uobj_chunk_num + sobj_chunk_num) != *valid_chunks) {
		pr_err("QAT: inconsistent UOF/SUOF chunk amount\n");
		return EINVAL;
	}
	return 0;
}

static void
qat_uclo_map_mof_symobjs(struct icp_qat_mof_handle *mobj_handle,
			 struct icp_qat_mof_chunkhdr *mof_chunkhdr)
{
	char **sym_str = (char **)&mobj_handle->sym_str;
	unsigned int *sym_size = &mobj_handle->sym_size;
	struct icp_qat_mof_str_table *str_table_obj;

	*sym_size = *(unsigned int *)(uintptr_t)(mof_chunkhdr->offset +
						 mobj_handle->mof_buf);
	*sym_str =
	    (char *)(uintptr_t)(mobj_handle->mof_buf + mof_chunkhdr->offset +
				sizeof(str_table_obj->tab_len));
}

static void
qat_uclo_map_mof_chunk(struct icp_qat_mof_handle *mobj_handle,
		       struct icp_qat_mof_chunkhdr *mof_chunkhdr)
{
	if (!strncmp(mof_chunkhdr->chunk_id,
		     ICP_QAT_MOF_SYM_OBJS,
		     ICP_QAT_MOF_OBJ_ID_LEN))
		qat_uclo_map_mof_symobjs(mobj_handle, mof_chunkhdr);
	else if (!strncmp(mof_chunkhdr->chunk_id,
			  ICP_QAT_UOF_OBJS,
			  ICP_QAT_MOF_OBJ_ID_LEN))
		mobj_handle->uobjs_hdr =
		    mobj_handle->mof_buf + (unsigned long)mof_chunkhdr->offset;
	else if (!strncmp(mof_chunkhdr->chunk_id,
			  ICP_QAT_SUOF_OBJS,
			  ICP_QAT_MOF_OBJ_ID_LEN))
		mobj_handle->sobjs_hdr =
		    mobj_handle->mof_buf + (unsigned long)mof_chunkhdr->offset;
}

static int
qat_uclo_check_mof_format(const struct icp_qat_mof_file_hdr *mof_hdr)
{
	int maj = mof_hdr->maj_ver & 0xff;
	int min = mof_hdr->min_ver & 0xff;

	if (mof_hdr->file_id != ICP_QAT_MOF_FID) {
		pr_err("QAT: invalid header 0x%x\n", mof_hdr->file_id);
		return EINVAL;
	}

	if (mof_hdr->num_chunks <= 0x1) {
		pr_err("QAT: MOF chunk amount is incorrect\n");
		return EINVAL;
	}
	if (maj != ICP_QAT_MOF_MAJVER || min != ICP_QAT_MOF_MINVER) {
		pr_err("QAT: bad MOF version, major 0x%x, minor 0x%x\n",
		       maj,
		       min);
		return EINVAL;
	}
	return 0;
}

static int
qat_uclo_map_mof_obj(struct icp_qat_fw_loader_handle *handle,
		     const struct icp_qat_mof_file_hdr *mof_ptr,
		     u32 mof_size,
		     const char *obj_name,
		     const char **obj_ptr,
		     unsigned int *obj_size)
{
	struct icp_qat_mof_handle *mobj_handle;
	struct icp_qat_mof_chunkhdr *mof_chunkhdr;
	unsigned short chunks_num;
	int ret;
	unsigned int i;

	if (mof_ptr->file_id == ICP_QAT_UOF_FID ||
	    mof_ptr->file_id == ICP_QAT_SUOF_FID) {
		if (obj_ptr)
			*obj_ptr = (const char *)mof_ptr;
		if (obj_size)
			*obj_size = (unsigned int)mof_size;
		return 0;
	}
	if (qat_uclo_check_mof_format(mof_ptr))
		return EINVAL;
	mobj_handle = malloc(sizeof(*mobj_handle), M_QAT, M_WAITOK | M_ZERO);
	handle->mobj_handle = mobj_handle;
	ret = qat_uclo_map_mof_file_hdr(handle, mof_ptr, mof_size);
	if (ret)
		return ret;
	mof_chunkhdr = (struct icp_qat_mof_chunkhdr *)((uintptr_t)mof_ptr +
						       sizeof(*mof_ptr));
	chunks_num = mof_ptr->num_chunks;
	/*Parse MOF file chunks*/
	for (i = 0; i < chunks_num; i++)
		qat_uclo_map_mof_chunk(mobj_handle, &mof_chunkhdr[i]);
	/*All sym_objs uobjs and sobjs should be available*/
	if (!mobj_handle->sym_str ||
	    (!mobj_handle->uobjs_hdr && !mobj_handle->sobjs_hdr))
		return EINVAL;
	ret = qat_uclo_map_objs_from_mof(mobj_handle);
	if (ret)
		return ret;
	/*Seek specified uof object in MOF*/
	ret = qat_uclo_seek_obj_inside_mof(mobj_handle,
					   obj_name,
					   obj_ptr,
					   obj_size);
	if (ret)
		return ret;
	return 0;
}

int
qat_uclo_map_obj(struct icp_qat_fw_loader_handle *handle,
		 const void *addr_ptr,
		 u32 mem_size,
		 const char *obj_name)
{
	const char *obj_addr;
	u32 obj_size;
	int ret;

	BUILD_BUG_ON(ICP_QAT_UCLO_MAX_AE >
		     (sizeof(handle->hal_handle->ae_mask) * 8));

	if (!handle || !addr_ptr || mem_size < 24)
		return EINVAL;

	if (obj_name) {
		ret = qat_uclo_map_mof_obj(
		    handle, addr_ptr, mem_size, obj_name, &obj_addr, &obj_size);
		if (ret)
			return ret;
	} else {
		obj_addr = addr_ptr;
		obj_size = mem_size;
	}

	return (handle->fw_auth) ?
	    qat_uclo_map_suof_obj(handle, obj_addr, obj_size) :
	    qat_uclo_map_uof_obj(handle, obj_addr, obj_size);
}

void
qat_uclo_del_obj(struct icp_qat_fw_loader_handle *handle)
{
	struct icp_qat_uclo_objhandle *obj_handle = handle->obj_handle;
	unsigned int a;
	unsigned long ae_mask = handle->hal_handle->ae_mask;

	if (handle->mobj_handle)
		qat_uclo_del_mof(handle);
	if (handle->sobj_handle)
		qat_uclo_del_suof(handle);
	if (!obj_handle)
		return;

	free(obj_handle->uword_buf, M_QAT);
	for (a = 0; a < obj_handle->uimage_num; a++)
		free(obj_handle->ae_uimage[a].page, M_QAT);

	for_each_set_bit(a, &ae_mask, handle->hal_handle->ae_max_num)
	{
		qat_uclo_free_ae_data(&obj_handle->ae_data[a]);
	}

	free(obj_handle->obj_hdr, M_QAT);
	free(obj_handle->obj_buf, M_QAT);
	free(obj_handle, M_QAT);
	handle->obj_handle = NULL;
}

static void
qat_uclo_fill_uwords(struct icp_qat_uclo_objhandle *obj_handle,
		     struct icp_qat_uclo_encap_page *encap_page,
		     uint64_t *uword,
		     unsigned int addr_p,
		     unsigned int raddr,
		     uint64_t fill)
{
	uint64_t uwrd = 0;
	unsigned int i, addr;

	if (!encap_page) {
		*uword = fill;
		return;
	}
	addr = (encap_page->page_region) ? raddr : addr_p;
	for (i = 0; i < encap_page->uwblock_num; i++) {
		if (addr >= encap_page->uwblock[i].start_addr &&
		    addr <= encap_page->uwblock[i].start_addr +
			    encap_page->uwblock[i].words_num - 1) {
			addr -= encap_page->uwblock[i].start_addr;
			addr *= obj_handle->uword_in_bytes;
			memcpy(&uwrd,
			       (void *)(((uintptr_t)encap_page->uwblock[i]
					     .micro_words) +
					addr),
			       obj_handle->uword_in_bytes);
			uwrd = uwrd & 0xbffffffffffull;
		}
	}
	*uword = uwrd;
	if (*uword == INVLD_UWORD)
		*uword = fill;
}

static void
qat_uclo_wr_uimage_raw_page(struct icp_qat_fw_loader_handle *handle,
			    struct icp_qat_uclo_encap_page *encap_page,
			    unsigned int ae)
{
	unsigned int uw_physical_addr, uw_relative_addr, i, words_num, cpylen;
	struct icp_qat_uclo_objhandle *obj_handle = handle->obj_handle;
	uint64_t fill_pat;

	/* load the page starting at appropriate ustore address */
	/* get fill-pattern from an image -- they are all the same */
	memcpy(&fill_pat,
	       obj_handle->ae_uimage[0].img_ptr->fill_pattern,
	       sizeof(uint64_t));
	uw_physical_addr = encap_page->beg_addr_p;
	uw_relative_addr = 0;
	words_num = encap_page->micro_words_num;
	while (words_num) {
		if (words_num < UWORD_CPYBUF_SIZE)
			cpylen = words_num;
		else
			cpylen = UWORD_CPYBUF_SIZE;

		/* load the buffer */
		for (i = 0; i < cpylen; i++)
			qat_uclo_fill_uwords(obj_handle,
					     encap_page,
					     &obj_handle->uword_buf[i],
					     uw_physical_addr + i,
					     uw_relative_addr + i,
					     fill_pat);

		if (obj_handle->ae_data[ae].shareable_ustore)
			/* copy the buffer to ustore */
			qat_hal_wr_coalesce_uwords(handle,
						   (unsigned char)ae,
						   uw_physical_addr,
						   cpylen,
						   obj_handle->uword_buf);
		else
			/* copy the buffer to ustore */
			qat_hal_wr_uwords(handle,
					  (unsigned char)ae,
					  uw_physical_addr,
					  cpylen,
					  obj_handle->uword_buf);
		uw_physical_addr += cpylen;
		uw_relative_addr += cpylen;
		words_num -= cpylen;
	}
}

static void
qat_uclo_wr_uimage_page(struct icp_qat_fw_loader_handle *handle,
			struct icp_qat_uof_image *image)
{
	struct icp_qat_uclo_objhandle *obj_handle = handle->obj_handle;
	unsigned int ctx_mask, s;
	struct icp_qat_uclo_page *page;
	unsigned char ae = 0;
	int ctx;
	struct icp_qat_uclo_aedata *aed;
	unsigned long ae_mask = handle->hal_handle->ae_mask;

	if (ICP_QAT_CTX_MODE(image->ae_mode) == ICP_QAT_UCLO_MAX_CTX)
		ctx_mask = 0xff;
	else
		ctx_mask = 0x55;
	/* load the default page and set assigned CTX PC
	 * to the entrypoint address
	 */
	for_each_set_bit(ae, &ae_mask, handle->hal_handle->ae_max_num)
	{
		unsigned long cfg_ae_mask = handle->cfg_ae_mask;
		unsigned long ae_assigned = image->ae_assigned;

		if (!test_bit(ae, &cfg_ae_mask))
			continue;

		if (!test_bit(ae, &ae_assigned))
			continue;

		aed = &obj_handle->ae_data[ae];
		/* find the slice to which this image is assigned */
		for (s = 0; s < aed->slice_num; s++) {
			if (image->ctx_assigned &
			    aed->ae_slices[s].ctx_mask_assigned)
				break;
		}
		if (s >= aed->slice_num)
			continue;
		page = aed->ae_slices[s].page;
		if (!page->encap_page->def_page)
			continue;
		qat_uclo_wr_uimage_raw_page(handle, page->encap_page, ae);

		page = aed->ae_slices[s].page;
		for (ctx = 0; ctx < ICP_QAT_UCLO_MAX_CTX; ctx++)
			aed->ae_slices[s].cur_page[ctx] =
			    (ctx_mask & (1 << ctx)) ? page : NULL;
		qat_hal_set_live_ctx(handle,
				     (unsigned char)ae,
				     image->ctx_assigned);
		qat_hal_set_pc(handle,
			       (unsigned char)ae,
			       image->ctx_assigned,
			       image->entry_address);
	}
}

static int
qat_uclo_wr_suof_img(struct icp_qat_fw_loader_handle *handle)
{
	unsigned int i;
	struct icp_qat_fw_auth_desc *desc = NULL;
	struct icp_firml_dram_desc img_desc;
	struct icp_qat_suof_handle *sobj_handle = handle->sobj_handle;
	struct icp_qat_suof_img_hdr *simg_hdr = sobj_handle->img_table.simg_hdr;

	for (i = 0; i < sobj_handle->img_table.num_simgs; i++) {
		if (qat_uclo_map_auth_fw(handle,
					 (const char *)simg_hdr[i].simg_buf,
					 (unsigned int)(simg_hdr[i].simg_len),
					 &img_desc,
					 &desc))
			goto wr_err;
		if (qat_uclo_auth_fw(handle, desc))
			goto wr_err;
		if (qat_uclo_load_fw(handle, desc))
			goto wr_err;
		qat_uclo_simg_free(handle, &img_desc);
	}
	return 0;
wr_err:
	qat_uclo_simg_free(handle, &img_desc);
	return -EINVAL;
}

static int
qat_uclo_wr_uof_img(struct icp_qat_fw_loader_handle *handle)
{
	struct icp_qat_uclo_objhandle *obj_handle = handle->obj_handle;
	unsigned int i;

	if (qat_uclo_init_globals(handle))
		return EINVAL;
	for (i = 0; i < obj_handle->uimage_num; i++) {
		if (!obj_handle->ae_uimage[i].img_ptr)
			return EINVAL;
		if (qat_uclo_init_ustore(handle, &obj_handle->ae_uimage[i]))
			return EINVAL;
		qat_uclo_wr_uimage_page(handle,
					obj_handle->ae_uimage[i].img_ptr);
	}
	return 0;
}

int
qat_uclo_wr_all_uimage(struct icp_qat_fw_loader_handle *handle)
{
	return (handle->fw_auth) ? qat_uclo_wr_suof_img(handle) :
				   qat_uclo_wr_uof_img(handle);
}

int
qat_uclo_set_cfg_ae_mask(struct icp_qat_fw_loader_handle *handle,
			 unsigned int cfg_ae_mask)
{
	if (!cfg_ae_mask)
		return EINVAL;

	handle->cfg_ae_mask = cfg_ae_mask;
	return 0;
}

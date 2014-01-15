/*-
 * Copyright (c) 2010 Kai Wang
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
 * $Id: dwarf_frame.c 2084 2011-10-27 04:48:12Z jkoshy $
 */

#include <assert.h>
#include <dwarf.h>
#include <errno.h>
#include <fcntl.h>
#include <libdwarf.h>
#include <string.h>

#include "driver.h"
#include "tet_api.h"

/*
 * Test case for dwarf line informatio API.
 */
static void tp_dwarf_frame2(void);
static void tp_dwarf_frame3(void);
static struct dwarf_tp dwarf_tp_array[] = {
	{"tp_dwarf_frame2",tp_dwarf_frame2},
	{"tp_dwarf_frame3",tp_dwarf_frame3},
	{NULL, NULL},
};
static int result = TET_UNRESOLVED;
#include "driver.c"

#define	_MAX_REG_NUM	10

static void
_frame2_test(Dwarf_Debug dbg, Dwarf_Fde fde, Dwarf_Addr pc,
    Dwarf_Unsigned func_len, Dwarf_Unsigned caf)
{
	Dwarf_Signed offset_relevant, register_num, offset;
	Dwarf_Addr pc_end, row_pc;
	Dwarf_Regtable reg_table;
	Dwarf_Error de;
	int i, cnt;

	(void) dwarf_set_frame_cfa_value(dbg, DW_FRAME_CFA_COL);

	/* Sanity check for invalid table_column. */
	if (dwarf_get_fde_info_for_reg(fde, 9999, 0, &offset_relevant,
	    &register_num, &offset, &row_pc, &de) != DW_DLV_ERROR) {
		tet_infoline("dwarf_get_fde_info_for_reg didn't return"
		    " DW_DLV_ERROR when called with invalid table_column"
		    " value");
		result = TET_FAIL;
		return;
	}

	cnt = 0;
	pc_end = pc + func_len;
	while (pc < pc_end && cnt < 16) {
		tet_printf("query CFA register pc %#jx\n", (uintmax_t) pc);
		/*
		 * XXX If application want to use DW_FRAME_CFA_COL for CFA,
		 * it should call dwarf_set_frame_cfa_value() to set that
		 * explicitly. So here DW_FRAME_CFA_COL might not be refering
		 * to the CFA at all, depends on whether CFA(0) is set by
		 * dwarf_set_frame_cfa_value.
		 */
		if (dwarf_get_fde_info_for_reg(fde, DW_FRAME_CFA_COL,
		    pc, &offset_relevant, &register_num, &offset,
		    &row_pc, &de) != DW_DLV_OK) {
			tet_printf("dwarf_get_fde_info_for_reg(cfa) failed: %s",
			    dwarf_errmsg(de));
			result = TET_FAIL;
			return;
		}
		TS_CHECK_INT(offset_relevant);
		TS_CHECK_INT(offset);
		TS_CHECK_INT(register_num);
		TS_CHECK_UINT(row_pc);
		for (i = 1; i < _MAX_REG_NUM; i++) {
			tet_printf("query register %d\n", i);
			if (dwarf_get_fde_info_for_reg(fde, i, pc,
			    &offset_relevant, &register_num, &offset,
			    &row_pc, &de) != DW_DLV_OK) {
				tet_printf("dwarf_get_fde_info_for_reg(%d)"
				    " failed: %s", i, dwarf_errmsg(de));
				result = TET_FAIL;
				goto next;
			}
			TS_CHECK_INT(offset_relevant);
			TS_CHECK_INT(offset);
			TS_CHECK_INT(register_num);
			TS_CHECK_UINT(row_pc);
		}
		tet_infoline("query all register");
		if (dwarf_get_fde_info_for_all_regs(fde, pc, &reg_table,
		    &row_pc, &de) != DW_DLV_OK) {
			tet_printf("dwarf_get_fde_info_for_all_regs failed: %s",
			    dwarf_errmsg(de));
			result = TET_FAIL;
			goto next;
		}
		TS_CHECK_UINT(row_pc);
		for (i = 0; i < _MAX_REG_NUM; i++) {
			tet_printf("check reg_table[%d]\n", i);
			TS_CHECK_UINT(reg_table.rules[i].dw_offset_relevant);
			TS_CHECK_UINT(reg_table.rules[i].dw_regnum);
			TS_CHECK_UINT(reg_table.rules[i].dw_offset);
		}
		
	next:
		pc += caf;
		cnt++;
	}
}

static void
_frame3_test(Dwarf_Debug dbg, Dwarf_Fde fde, Dwarf_Addr pc,
    Dwarf_Unsigned func_len, Dwarf_Unsigned caf)
{
	Dwarf_Signed offset_relevant, register_num, offset_or_block_len;
	Dwarf_Addr pc_end, row_pc;
	Dwarf_Ptr block_ptr;
	Dwarf_Regtable3 reg_table3;
	Dwarf_Small value_type;
	Dwarf_Error de;
	int i, cnt;

	/* Initialise regster table (DWARF3). */
	reg_table3.rt3_reg_table_size = DW_REG_TABLE_SIZE;
	reg_table3.rt3_rules = calloc(reg_table3.rt3_reg_table_size,
	    sizeof(Dwarf_Regtable_Entry3));
	if (reg_table3.rt3_rules == NULL) {
		tet_infoline("calloc failed when initialising reg_table3");
		result = TET_FAIL;
		return;
	}

	/* Sanity check for invalid table_column. */
	if (dwarf_get_fde_info_for_reg3(fde, 9999, 0, &value_type,
	    &offset_relevant, &register_num, &offset_or_block_len, &block_ptr,
	    &row_pc, &de) != DW_DLV_ERROR) {
		tet_infoline("dwarf_get_fde_info_for_reg3 didn't return"
		    " DW_DLV_ERROR when called with invalid table_column"
		    " value");
		result = TET_FAIL;
		return;
	}

	cnt = 0;
	pc_end = pc + func_len;
	while (pc < pc_end && cnt < 16) {
		tet_printf("query CFA(3) register pc %#jx\n", (uintmax_t) pc);
		if (dwarf_get_fde_info_for_cfa_reg3(fde, pc, &value_type,
		    &offset_relevant, &register_num, &offset_or_block_len,
		    &block_ptr, &row_pc, &de) != DW_DLV_OK) {
			tet_printf("dwarf_get_fde_info_for_reg3(cfa) failed: %s",
			    dwarf_errmsg(de));
			result = TET_FAIL;
			return;
		}
		TS_CHECK_INT(value_type);
		TS_CHECK_INT(offset_relevant);
		TS_CHECK_INT(offset_or_block_len);
		TS_CHECK_INT(register_num);
		TS_CHECK_UINT(row_pc);
		if (value_type == DW_EXPR_EXPRESSION ||
		    value_type == DW_EXPR_VAL_EXPRESSION)
			TS_CHECK_BLOCK(block_ptr, offset_or_block_len);
		for (i = 1; i < _MAX_REG_NUM; i++) {
			tet_printf("query register(3) %d\n", i);
			if (dwarf_get_fde_info_for_reg3(fde, i, pc, &value_type,
			    &offset_relevant, &register_num,
			    &offset_or_block_len, &block_ptr,
			    &row_pc, &de) != DW_DLV_OK) {
				tet_printf("dwarf_get_fde_info_for_reg3(%d)"
				    " failed: %s", i, dwarf_errmsg(de));
				result = TET_FAIL;
				goto next;
			}
			TS_CHECK_INT(value_type);
			TS_CHECK_INT(offset_relevant);
			TS_CHECK_INT(offset_or_block_len);
			TS_CHECK_INT(register_num);
			TS_CHECK_UINT(row_pc);
			if (value_type == DW_EXPR_EXPRESSION ||
			    value_type == DW_EXPR_VAL_EXPRESSION)
				TS_CHECK_BLOCK(block_ptr, offset_or_block_len);
		}
		tet_infoline("query all register(3)");
		if (dwarf_get_fde_info_for_all_regs3(fde, pc, &reg_table3,
		    &row_pc, &de) != DW_DLV_OK) {
			tet_printf("dwarf_get_fde_info_for_all_regs failed: %s",
			    dwarf_errmsg(de));
			result = TET_FAIL;
			goto next;
		}
		TS_CHECK_UINT(row_pc);

#define	CFA3	reg_table3.rt3_cfa_rule
#define	RT3	reg_table3.rt3_rules
		TS_CHECK_UINT(CFA3.dw_offset_relevant);
		TS_CHECK_UINT(CFA3.dw_value_type);
		TS_CHECK_UINT(CFA3.dw_regnum);
		TS_CHECK_UINT(CFA3.dw_offset_or_block_len);
		if (CFA3.dw_value_type == DW_EXPR_EXPRESSION ||
		    CFA3.dw_value_type == DW_EXPR_VAL_EXPRESSION)
			TS_CHECK_BLOCK(CFA3.dw_block_ptr,
			    CFA3.dw_offset_or_block_len);
		for (i = 0; i < _MAX_REG_NUM; i++) {
			tet_printf("check reg_table3[%d]\n", i);
			TS_CHECK_UINT(RT3[i].dw_offset_relevant);
			TS_CHECK_UINT(RT3[i].dw_value_type);
			TS_CHECK_UINT(RT3[i].dw_regnum);
			TS_CHECK_UINT(RT3[i].dw_offset_or_block_len);
			if (RT3[i].dw_value_type == DW_EXPR_EXPRESSION ||
			    RT3[i].dw_value_type == DW_EXPR_VAL_EXPRESSION)
				TS_CHECK_BLOCK(RT3[i].dw_block_ptr,
				    RT3[i].dw_offset_or_block_len);
		}
#undef CFA3
#undef RT3
		
	next:
		pc += caf;
		cnt++;
	}
}

static void
_dwarf_cie_fde_test(Dwarf_Debug dbg, int eh, void (*_frame_test)(Dwarf_Debug,
    Dwarf_Fde, Dwarf_Addr, Dwarf_Unsigned, Dwarf_Unsigned))
{
	Dwarf_Cie *cielist, cie;
	Dwarf_Fde *fdelist, fde;
	Dwarf_Frame_Op *oplist;
	Dwarf_Signed ciecnt, fdecnt;
	Dwarf_Addr low_pc, high_pc;
	Dwarf_Unsigned func_len, fde_byte_len, fde_inst_len, bytes_in_cie;
	Dwarf_Unsigned cie_caf, cie_daf, cie_inst_len;
	Dwarf_Signed cie_index, opcnt;
	Dwarf_Off cie_offset, fde_offset;
	Dwarf_Ptr fde_bytes, fde_inst, cie_initinst;
	Dwarf_Half cie_ra;
	Dwarf_Small cie_version;
	Dwarf_Error de;
	const char *cfa_str;
	char *cie_augmenter;
	int i, j, r_fde_at_pc;

	if (eh) {
		if (dwarf_get_fde_list_eh(dbg, &cielist, &ciecnt, &fdelist,
		    &fdecnt, &de) != DW_DLV_OK) {
			tet_printf("dwarf_get_fde_list_eh failed: %s\n",
			    dwarf_errmsg(de));
			result = TET_FAIL;
			goto done;
		}
	} else {
		if (dwarf_get_fde_list(dbg, &cielist, &ciecnt, &fdelist,
		    &fdecnt, &de) != DW_DLV_OK) {
			tet_printf("dwarf_get_fde_list failed: %s\n",
			    dwarf_errmsg(de));
			result = TET_FAIL;
			goto done;
		}
	}
	TS_CHECK_INT(ciecnt);
	TS_CHECK_INT(fdecnt);

	/*
	 * Test dwarf_get_fde_at_pc using hard-coded PC values.
	 */

	tet_infoline("attempt to get fde at 0x08082a30");
	r_fde_at_pc = dwarf_get_fde_at_pc(fdelist, 0x08082a30, &fde, &low_pc,
	    &high_pc, &de);
	TS_CHECK_INT(r_fde_at_pc);
	if (r_fde_at_pc == DW_DLV_OK) {
		TS_CHECK_UINT(low_pc);
		TS_CHECK_UINT(high_pc);
	}

	tet_infoline("attempt to get fde at 0x08083087");
	r_fde_at_pc = dwarf_get_fde_at_pc(fdelist, 0x08083087, &fde, &low_pc,
	    &high_pc, &de);
	TS_CHECK_INT(r_fde_at_pc);
	if (r_fde_at_pc == DW_DLV_OK) {
		TS_CHECK_UINT(low_pc);
		TS_CHECK_UINT(high_pc);
	}

	tet_infoline("attempt to get fde at 0x080481f0");
	r_fde_at_pc = dwarf_get_fde_at_pc(fdelist, 0x080481f0, &fde, &low_pc,
	    &high_pc, &de);
	TS_CHECK_INT(r_fde_at_pc);
	if (r_fde_at_pc == DW_DLV_OK) {
		TS_CHECK_UINT(low_pc);
		TS_CHECK_UINT(high_pc);
	}

	tet_infoline("attempt to get fde at 0x08048564");
	r_fde_at_pc = dwarf_get_fde_at_pc(fdelist, 0x08048564, &fde, &low_pc,
	    &high_pc, &de);
	TS_CHECK_INT(r_fde_at_pc);
	if (r_fde_at_pc == DW_DLV_OK) {
		TS_CHECK_UINT(low_pc);
		TS_CHECK_UINT(high_pc);
	}

	tet_infoline("attempt to get fde at 0x00401280");
	r_fde_at_pc = dwarf_get_fde_at_pc(fdelist, 0x00401280, &fde, &low_pc,
	    &high_pc, &de);
	TS_CHECK_INT(r_fde_at_pc);
	if (r_fde_at_pc == DW_DLV_OK) {
		TS_CHECK_UINT(low_pc);
		TS_CHECK_UINT(high_pc);
	}

	tet_infoline("attempt to get fde at 0x004012b1");
	r_fde_at_pc = dwarf_get_fde_at_pc(fdelist, 0x004012b1, &fde, &low_pc,
	    &high_pc, &de);
	TS_CHECK_INT(r_fde_at_pc);
	if (r_fde_at_pc == DW_DLV_OK) {
		TS_CHECK_UINT(low_pc);
		TS_CHECK_UINT(high_pc);
	}

	/*
	 * Test each FDE contained in the FDE list.
	 */

	for (i = 0; i < fdecnt; i++) {
		if (dwarf_get_fde_n(fdelist, i, &fde, &de) != DW_DLV_OK) {
			tet_printf("dwarf_get_fde_n(%d) failed: %s\n", i,
			    dwarf_errmsg(de));
			result = TET_FAIL;
			continue;
		}
		if (dwarf_get_fde_range(fde, &low_pc, &func_len, &fde_bytes,
		    &fde_byte_len, &cie_offset, &cie_index, &fde_offset,
		    &de) == DW_DLV_ERROR) {
			tet_printf("dwarf_get_fde_range(%d) failed: %s\n", i,
			    dwarf_errmsg(de));
			result = TET_FAIL;
			continue;
		}
		TS_CHECK_UINT(low_pc);
		TS_CHECK_UINT(func_len);
		TS_CHECK_UINT(fde_byte_len);
		if (fde_byte_len > 0)
			TS_CHECK_BLOCK(fde_bytes, fde_byte_len);
		TS_CHECK_INT(cie_offset);
		TS_CHECK_INT(cie_index);
		TS_CHECK_INT(fde_offset);
		if (dwarf_get_cie_of_fde(fde, &cie, &de) != DW_DLV_OK) {
			tet_printf("dwarf_get_cie_of_fde(%d) failed: %s\n", i,
			    dwarf_errmsg(de));
			result = TET_FAIL;
			continue;
		}
		if (dwarf_get_cie_index(cie, &cie_index, &de) != DW_DLV_OK) {
			tet_printf("dwarf_get_cie_index(%d) failed: %s\n", i,
			    dwarf_errmsg(de));
			result = TET_FAIL;
			continue;
		}
		TS_CHECK_INT(cie_index);
		if (dwarf_get_cie_info(cie, &bytes_in_cie, &cie_version,
		    &cie_augmenter, &cie_caf, &cie_daf, &cie_ra, &cie_initinst,
		    &cie_inst_len, &de) != DW_DLV_OK) {
			tet_printf("dwarf_get_cie_info(%d) failed: %s\n", i,
			    dwarf_errmsg(de));
			result = TET_FAIL;
			continue;
		}
		TS_CHECK_UINT(bytes_in_cie);
		TS_CHECK_UINT(cie_version);
		TS_CHECK_STRING(cie_augmenter);
		TS_CHECK_UINT(cie_caf);
		TS_CHECK_UINT(cie_daf);
		TS_CHECK_UINT(cie_ra);
		TS_CHECK_UINT(cie_inst_len);
		if (cie_inst_len > 0)
			TS_CHECK_BLOCK(cie_initinst, cie_inst_len);
		if (dwarf_get_fde_instr_bytes(fde, &fde_inst, &fde_inst_len,
		    &de) != DW_DLV_OK) {
			tet_printf("dwarf_get_fde_instr_bytes(%d) failed: %s\n",
			    i, dwarf_errmsg(de));
			result = TET_FAIL;
			continue;
		}
		TS_CHECK_UINT(fde_inst_len);
		if (fde_inst_len > 0) {
			TS_CHECK_BLOCK(fde_inst, fde_inst_len);
			if (dwarf_expand_frame_instructions(cie, fde_inst,
			    fde_inst_len, &oplist, &opcnt, &de) != DW_DLV_OK) {
				tet_printf("dwarf_expand_frame_instructions(%d)"
				    " failed: %s\n", i, dwarf_errmsg(de));
				result = TET_FAIL;
				continue;
			}
			TS_CHECK_INT(opcnt);
			for (j = 0; j < opcnt; j++) {
				TS_CHECK_UINT(oplist[j].fp_base_op);
				if (oplist[j].fp_base_op != 0) {
					if (dwarf_get_CFA_name(
					    oplist[j].fp_base_op << 6,
					    &cfa_str) != DW_DLV_OK) {
						tet_printf("dwarf_get_CFA_name"
						    " failed\n");
						continue;
					}
					TS_CHECK_STRING(cfa_str);
				}
				TS_CHECK_UINT(oplist[j].fp_extended_op);
				if (oplist[j].fp_extended_op != 0) {
					if (dwarf_get_CFA_name(
					    oplist[j].fp_extended_op,
					    &cfa_str) != DW_DLV_OK) {
						tet_printf("dwarf_get_CFA_name"
						    " failed\n");
						continue;
					}
					TS_CHECK_STRING(cfa_str);
				}
				TS_CHECK_UINT(oplist[j].fp_register);
				TS_CHECK_INT(oplist[j].fp_offset);
				TS_CHECK_INT(oplist[j].fp_instr_offset);
			}
		}
		_frame_test(dbg, fde, low_pc, func_len, cie_caf);
	}

done:
	return;
}

static void
tp_dwarf_frame2(void)
{
	Dwarf_Debug dbg;
	Dwarf_Error de;
	int fd;

	result = TET_UNRESOLVED;

	TS_DWARF_INIT(dbg, fd, de);

	_dwarf_cie_fde_test(dbg, 0, _frame2_test);
	_dwarf_cie_fde_test(dbg, 1, _frame2_test);

	if (result == TET_UNRESOLVED)
		result = TET_PASS;
done:
	TS_DWARF_FINISH(dbg, de);
	TS_RESULT(result);
}

static void
tp_dwarf_frame3(void)
{
	Dwarf_Debug dbg;
	Dwarf_Error de;
	int fd;

	result = TET_UNRESOLVED;

	TS_DWARF_INIT(dbg, fd, de);

	_dwarf_cie_fde_test(dbg, 0, _frame3_test);
	_dwarf_cie_fde_test(dbg, 1, _frame3_test);

	if (result == TET_UNRESOLVED)
		result = TET_PASS;
done:
	TS_DWARF_FINISH(dbg, de);
	TS_RESULT(result);
}

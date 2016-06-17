/*
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2001-2003 Silicon Graphics, Inc.  All rights reserved.
 */



#ifndef _ASM_IA64_SN_SN2_SHUB_MMR_T_H
#define _ASM_IA64_SN_SN2_SHUB_MMR_T_H

#include <asm/sn/arch.h>

/* ==================================================================== */
/*                   Register "SH_FSB_BINIT_CONTROL"                    */
/*                          FSB BINIT# Control                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_fsb_binit_control_u {
	mmr_t	sh_fsb_binit_control_regval;
	struct {
		mmr_t	binit       : 1;
		mmr_t	reserved_0  : 63;
	} sh_fsb_binit_control_s;
} sh_fsb_binit_control_u_t;
#else
typedef union sh_fsb_binit_control_u {
	mmr_t	sh_fsb_binit_control_regval;
	struct {
		mmr_t	reserved_0  : 63;
		mmr_t	binit       : 1;
	} sh_fsb_binit_control_s;
} sh_fsb_binit_control_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_FSB_RESET_CONTROL"                    */
/*                          FSB Reset Control                           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_fsb_reset_control_u {
	mmr_t	sh_fsb_reset_control_regval;
	struct {
		mmr_t	reset       : 1;
		mmr_t	reserved_0  : 63;
	} sh_fsb_reset_control_s;
} sh_fsb_reset_control_u_t;
#else
typedef union sh_fsb_reset_control_u {
	mmr_t	sh_fsb_reset_control_regval;
	struct {
		mmr_t	reserved_0  : 63;
		mmr_t	reset       : 1;
	} sh_fsb_reset_control_s;
} sh_fsb_reset_control_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_FSB_SYSTEM_AGENT_CONFIG"                 */
/*                    FSB System Agent Configuration                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_fsb_system_agent_config_u {
	mmr_t	sh_fsb_system_agent_config_regval;
	struct {
		mmr_t	rcnt_scnt_en        : 1;
		mmr_t	reserved_0          : 2;
		mmr_t	berr_assert_en      : 1;
		mmr_t	berr_sampling_en    : 1;
		mmr_t	binit_assert_en     : 1;
		mmr_t	bnr_throttling_en   : 1;
		mmr_t	short_hang_en       : 1;
		mmr_t	inta_rsp_data       : 8;
		mmr_t	io_trans_rsp        : 1;
		mmr_t	xtpr_trans_rsp      : 1;
		mmr_t	inta_trans_rsp      : 1;
		mmr_t	reserved_1          : 4;
		mmr_t	tdot                : 1;
		mmr_t	serialize_fsb_en    : 1;
		mmr_t	reserved_2          : 7;
		mmr_t	binit_event_enables : 14;
		mmr_t	reserved_3          : 18;
	} sh_fsb_system_agent_config_s;
} sh_fsb_system_agent_config_u_t;
#else
typedef union sh_fsb_system_agent_config_u {
	mmr_t	sh_fsb_system_agent_config_regval;
	struct {
		mmr_t	reserved_3          : 18;
		mmr_t	binit_event_enables : 14;
		mmr_t	reserved_2          : 7;
		mmr_t	serialize_fsb_en    : 1;
		mmr_t	tdot                : 1;
		mmr_t	reserved_1          : 4;
		mmr_t	inta_trans_rsp      : 1;
		mmr_t	xtpr_trans_rsp      : 1;
		mmr_t	io_trans_rsp        : 1;
		mmr_t	inta_rsp_data       : 8;
		mmr_t	short_hang_en       : 1;
		mmr_t	bnr_throttling_en   : 1;
		mmr_t	binit_assert_en     : 1;
		mmr_t	berr_sampling_en    : 1;
		mmr_t	berr_assert_en      : 1;
		mmr_t	reserved_0          : 2;
		mmr_t	rcnt_scnt_en        : 1;
	} sh_fsb_system_agent_config_s;
} sh_fsb_system_agent_config_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_FSB_VGA_REMAP"                      */
/*                     FSB VGA Address Space Remap                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_fsb_vga_remap_u {
	mmr_t	sh_fsb_vga_remap_regval;
	struct {
		mmr_t	reserved_0            : 17;
		mmr_t	offset                : 19;
		mmr_t	asid                  : 2;
		mmr_t	nid                   : 11;
		mmr_t	reserved_1            : 13;
		mmr_t	vga_remapping_enabled : 1;
		mmr_t	reserved_2            : 1;
	} sh_fsb_vga_remap_s;
} sh_fsb_vga_remap_u_t;
#else
typedef union sh_fsb_vga_remap_u {
	mmr_t	sh_fsb_vga_remap_regval;
	struct {
		mmr_t	reserved_2            : 1;
		mmr_t	vga_remapping_enabled : 1;
		mmr_t	reserved_1            : 13;
		mmr_t	nid                   : 11;
		mmr_t	asid                  : 2;
		mmr_t	offset                : 19;
		mmr_t	reserved_0            : 17;
	} sh_fsb_vga_remap_s;
} sh_fsb_vga_remap_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_FSB_RESET_STATUS"                    */
/*                           FSB Reset Status                           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_fsb_reset_status_u {
	mmr_t	sh_fsb_reset_status_regval;
	struct {
		mmr_t	reset_in_progress : 1;
		mmr_t	reserved_0        : 63;
	} sh_fsb_reset_status_s;
} sh_fsb_reset_status_u_t;
#else
typedef union sh_fsb_reset_status_u {
	mmr_t	sh_fsb_reset_status_regval;
	struct {
		mmr_t	reserved_0        : 63;
		mmr_t	reset_in_progress : 1;
	} sh_fsb_reset_status_s;
} sh_fsb_reset_status_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_FSB_SYMMETRIC_AGENT_STATUS"               */
/*                      FSB Symmetric Agent Status                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_fsb_symmetric_agent_status_u {
	mmr_t	sh_fsb_symmetric_agent_status_regval;
	struct {
		mmr_t	cpu_0_active : 1;
		mmr_t	cpu_1_active : 1;
		mmr_t	cpus_ready   : 1;
		mmr_t	reserved_0   : 61;
	} sh_fsb_symmetric_agent_status_s;
} sh_fsb_symmetric_agent_status_u_t;
#else
typedef union sh_fsb_symmetric_agent_status_u {
	mmr_t	sh_fsb_symmetric_agent_status_regval;
	struct {
		mmr_t	reserved_0   : 61;
		mmr_t	cpus_ready   : 1;
		mmr_t	cpu_1_active : 1;
		mmr_t	cpu_0_active : 1;
	} sh_fsb_symmetric_agent_status_s;
} sh_fsb_symmetric_agent_status_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_GFX_CREDIT_COUNT_0"                   */
/*                Graphics-write Credit Count for CPU 0                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_gfx_credit_count_0_u {
	mmr_t	sh_gfx_credit_count_0_regval;
	struct {
		mmr_t	count           : 20;
		mmr_t	reserved_0      : 43;
		mmr_t	reset_gfx_state : 1;
	} sh_gfx_credit_count_0_s;
} sh_gfx_credit_count_0_u_t;
#else
typedef union sh_gfx_credit_count_0_u {
	mmr_t	sh_gfx_credit_count_0_regval;
	struct {
		mmr_t	reset_gfx_state : 1;
		mmr_t	reserved_0      : 43;
		mmr_t	count           : 20;
	} sh_gfx_credit_count_0_s;
} sh_gfx_credit_count_0_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_GFX_CREDIT_COUNT_1"                   */
/*                Graphics-write Credit Count for CPU 1                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_gfx_credit_count_1_u {
	mmr_t	sh_gfx_credit_count_1_regval;
	struct {
		mmr_t	count           : 20;
		mmr_t	reserved_0      : 43;
		mmr_t	reset_gfx_state : 1;
	} sh_gfx_credit_count_1_s;
} sh_gfx_credit_count_1_u_t;
#else
typedef union sh_gfx_credit_count_1_u {
	mmr_t	sh_gfx_credit_count_1_regval;
	struct {
		mmr_t	reset_gfx_state : 1;
		mmr_t	reserved_0      : 43;
		mmr_t	count           : 20;
	} sh_gfx_credit_count_1_s;
} sh_gfx_credit_count_1_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_GFX_MODE_CNTRL_0"                    */
/*         Graphics credit mode amd message ordering for CPU 0          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_gfx_mode_cntrl_0_u {
	mmr_t	sh_gfx_mode_cntrl_0_regval;
	struct {
		mmr_t	dword_credits      : 1;
		mmr_t	mixed_mode_credits : 1;
		mmr_t	relaxed_ordering   : 1;
		mmr_t	reserved_0         : 61;
	} sh_gfx_mode_cntrl_0_s;
} sh_gfx_mode_cntrl_0_u_t;
#else
typedef union sh_gfx_mode_cntrl_0_u {
	mmr_t	sh_gfx_mode_cntrl_0_regval;
	struct {
		mmr_t	reserved_0         : 61;
		mmr_t	relaxed_ordering   : 1;
		mmr_t	mixed_mode_credits : 1;
		mmr_t	dword_credits      : 1;
	} sh_gfx_mode_cntrl_0_s;
} sh_gfx_mode_cntrl_0_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_GFX_MODE_CNTRL_1"                    */
/*         Graphics credit mode amd message ordering for CPU 1          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_gfx_mode_cntrl_1_u {
	mmr_t	sh_gfx_mode_cntrl_1_regval;
	struct {
		mmr_t	dword_credits      : 1;
		mmr_t	mixed_mode_credits : 1;
		mmr_t	relaxed_ordering   : 1;
		mmr_t	reserved_0         : 61;
	} sh_gfx_mode_cntrl_1_s;
} sh_gfx_mode_cntrl_1_u_t;
#else
typedef union sh_gfx_mode_cntrl_1_u {
	mmr_t	sh_gfx_mode_cntrl_1_regval;
	struct {
		mmr_t	reserved_0         : 61;
		mmr_t	relaxed_ordering   : 1;
		mmr_t	mixed_mode_credits : 1;
		mmr_t	dword_credits      : 1;
	} sh_gfx_mode_cntrl_1_s;
} sh_gfx_mode_cntrl_1_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_GFX_SKID_CREDIT_COUNT_0"                 */
/*              Graphics-write Skid Credit Count for CPU 0              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_gfx_skid_credit_count_0_u {
	mmr_t	sh_gfx_skid_credit_count_0_regval;
	struct {
		mmr_t	skid        : 20;
		mmr_t	reserved_0  : 44;
	} sh_gfx_skid_credit_count_0_s;
} sh_gfx_skid_credit_count_0_u_t;
#else
typedef union sh_gfx_skid_credit_count_0_u {
	mmr_t	sh_gfx_skid_credit_count_0_regval;
	struct {
		mmr_t	reserved_0  : 44;
		mmr_t	skid        : 20;
	} sh_gfx_skid_credit_count_0_s;
} sh_gfx_skid_credit_count_0_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_GFX_SKID_CREDIT_COUNT_1"                 */
/*              Graphics-write Skid Credit Count for CPU 1              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_gfx_skid_credit_count_1_u {
	mmr_t	sh_gfx_skid_credit_count_1_regval;
	struct {
		mmr_t	skid        : 20;
		mmr_t	reserved_0  : 44;
	} sh_gfx_skid_credit_count_1_s;
} sh_gfx_skid_credit_count_1_u_t;
#else
typedef union sh_gfx_skid_credit_count_1_u {
	mmr_t	sh_gfx_skid_credit_count_1_regval;
	struct {
		mmr_t	reserved_0  : 44;
		mmr_t	skid        : 20;
	} sh_gfx_skid_credit_count_1_s;
} sh_gfx_skid_credit_count_1_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_GFX_STALL_LIMIT_0"                    */
/*                 Graphics-write Stall Limit for CPU 0                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_gfx_stall_limit_0_u {
	mmr_t	sh_gfx_stall_limit_0_regval;
	struct {
		mmr_t	limit       : 26;
		mmr_t	reserved_0  : 38;
	} sh_gfx_stall_limit_0_s;
} sh_gfx_stall_limit_0_u_t;
#else
typedef union sh_gfx_stall_limit_0_u {
	mmr_t	sh_gfx_stall_limit_0_regval;
	struct {
		mmr_t	reserved_0  : 38;
		mmr_t	limit       : 26;
	} sh_gfx_stall_limit_0_s;
} sh_gfx_stall_limit_0_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_GFX_STALL_LIMIT_1"                    */
/*                 Graphics-write Stall Limit for CPU 1                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_gfx_stall_limit_1_u {
	mmr_t	sh_gfx_stall_limit_1_regval;
	struct {
		mmr_t	limit       : 26;
		mmr_t	reserved_0  : 38;
	} sh_gfx_stall_limit_1_s;
} sh_gfx_stall_limit_1_u_t;
#else
typedef union sh_gfx_stall_limit_1_u {
	mmr_t	sh_gfx_stall_limit_1_regval;
	struct {
		mmr_t	reserved_0  : 38;
		mmr_t	limit       : 26;
	} sh_gfx_stall_limit_1_s;
} sh_gfx_stall_limit_1_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_GFX_STALL_TIMER_0"                    */
/*                 Graphics-write Stall Timer for CPU 0                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_gfx_stall_timer_0_u {
	mmr_t	sh_gfx_stall_timer_0_regval;
	struct {
		mmr_t	timer_value : 26;
		mmr_t	reserved_0  : 38;
	} sh_gfx_stall_timer_0_s;
} sh_gfx_stall_timer_0_u_t;
#else
typedef union sh_gfx_stall_timer_0_u {
	mmr_t	sh_gfx_stall_timer_0_regval;
	struct {
		mmr_t	reserved_0  : 38;
		mmr_t	timer_value : 26;
	} sh_gfx_stall_timer_0_s;
} sh_gfx_stall_timer_0_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_GFX_STALL_TIMER_1"                    */
/*                 Graphics-write Stall Timer for CPU 1                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_gfx_stall_timer_1_u {
	mmr_t	sh_gfx_stall_timer_1_regval;
	struct {
		mmr_t	timer_value : 26;
		mmr_t	reserved_0  : 38;
	} sh_gfx_stall_timer_1_s;
} sh_gfx_stall_timer_1_u_t;
#else
typedef union sh_gfx_stall_timer_1_u {
	mmr_t	sh_gfx_stall_timer_1_regval;
	struct {
		mmr_t	reserved_0  : 38;
		mmr_t	timer_value : 26;
	} sh_gfx_stall_timer_1_s;
} sh_gfx_stall_timer_1_u_t;
#endif

/* ==================================================================== */
/*                      Register "SH_GFX_WINDOW_0"                      */
/*                   Graphics-write Window for CPU 0                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_gfx_window_0_u {
	mmr_t	sh_gfx_window_0_regval;
	struct {
		mmr_t	reserved_0    : 24;
		mmr_t	base_addr     : 12;
		mmr_t	reserved_1    : 27;
		mmr_t	gfx_window_en : 1;
	} sh_gfx_window_0_s;
} sh_gfx_window_0_u_t;
#else
typedef union sh_gfx_window_0_u {
	mmr_t	sh_gfx_window_0_regval;
	struct {
		mmr_t	gfx_window_en : 1;
		mmr_t	reserved_1    : 27;
		mmr_t	base_addr     : 12;
		mmr_t	reserved_0    : 24;
	} sh_gfx_window_0_s;
} sh_gfx_window_0_u_t;
#endif

/* ==================================================================== */
/*                      Register "SH_GFX_WINDOW_1"                      */
/*                   Graphics-write Window for CPU 1                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_gfx_window_1_u {
	mmr_t	sh_gfx_window_1_regval;
	struct {
		mmr_t	reserved_0    : 24;
		mmr_t	base_addr     : 12;
		mmr_t	reserved_1    : 27;
		mmr_t	gfx_window_en : 1;
	} sh_gfx_window_1_s;
} sh_gfx_window_1_u_t;
#else
typedef union sh_gfx_window_1_u {
	mmr_t	sh_gfx_window_1_regval;
	struct {
		mmr_t	gfx_window_en : 1;
		mmr_t	reserved_1    : 27;
		mmr_t	base_addr     : 12;
		mmr_t	reserved_0    : 24;
	} sh_gfx_window_1_s;
} sh_gfx_window_1_u_t;
#endif

/* ==================================================================== */
/*              Register "SH_GFX_INTERRUPT_TIMER_LIMIT_0"               */
/*               Graphics-write Interrupt Limit for CPU 0               */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_gfx_interrupt_timer_limit_0_u {
	mmr_t	sh_gfx_interrupt_timer_limit_0_regval;
	struct {
		mmr_t	interrupt_timer_limit : 8;
		mmr_t	reserved_0            : 56;
	} sh_gfx_interrupt_timer_limit_0_s;
} sh_gfx_interrupt_timer_limit_0_u_t;
#else
typedef union sh_gfx_interrupt_timer_limit_0_u {
	mmr_t	sh_gfx_interrupt_timer_limit_0_regval;
	struct {
		mmr_t	reserved_0            : 56;
		mmr_t	interrupt_timer_limit : 8;
	} sh_gfx_interrupt_timer_limit_0_s;
} sh_gfx_interrupt_timer_limit_0_u_t;
#endif

/* ==================================================================== */
/*              Register "SH_GFX_INTERRUPT_TIMER_LIMIT_1"               */
/*               Graphics-write Interrupt Limit for CPU 1               */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_gfx_interrupt_timer_limit_1_u {
	mmr_t	sh_gfx_interrupt_timer_limit_1_regval;
	struct {
		mmr_t	interrupt_timer_limit : 8;
		mmr_t	reserved_0            : 56;
	} sh_gfx_interrupt_timer_limit_1_s;
} sh_gfx_interrupt_timer_limit_1_u_t;
#else
typedef union sh_gfx_interrupt_timer_limit_1_u {
	mmr_t	sh_gfx_interrupt_timer_limit_1_regval;
	struct {
		mmr_t	reserved_0            : 56;
		mmr_t	interrupt_timer_limit : 8;
	} sh_gfx_interrupt_timer_limit_1_s;
} sh_gfx_interrupt_timer_limit_1_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_GFX_WRITE_STATUS_0"                   */
/*                   Graphics Write Status for CPU 0                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_gfx_write_status_0_u {
	mmr_t	sh_gfx_write_status_0_regval;
	struct {
		mmr_t	busy                : 1;
		mmr_t	reserved_0          : 62;
		mmr_t	re_enable_gfx_stall : 1;
	} sh_gfx_write_status_0_s;
} sh_gfx_write_status_0_u_t;
#else
typedef union sh_gfx_write_status_0_u {
	mmr_t	sh_gfx_write_status_0_regval;
	struct {
		mmr_t	re_enable_gfx_stall : 1;
		mmr_t	reserved_0          : 62;
		mmr_t	busy                : 1;
	} sh_gfx_write_status_0_s;
} sh_gfx_write_status_0_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_GFX_WRITE_STATUS_1"                   */
/*                   Graphics Write Status for CPU 1                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_gfx_write_status_1_u {
	mmr_t	sh_gfx_write_status_1_regval;
	struct {
		mmr_t	busy                : 1;
		mmr_t	reserved_0          : 62;
		mmr_t	re_enable_gfx_stall : 1;
	} sh_gfx_write_status_1_s;
} sh_gfx_write_status_1_u_t;
#else
typedef union sh_gfx_write_status_1_u {
	mmr_t	sh_gfx_write_status_1_regval;
	struct {
		mmr_t	re_enable_gfx_stall : 1;
		mmr_t	reserved_0          : 62;
		mmr_t	busy                : 1;
	} sh_gfx_write_status_1_s;
} sh_gfx_write_status_1_u_t;
#endif

/* ==================================================================== */
/*                        Register "SH_II_INT0"                         */
/*                    SHub II Interrupt 0 Registers                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ii_int0_u {
	mmr_t	sh_ii_int0_regval;
	struct {
		mmr_t	idx         : 8;
		mmr_t	send        : 1;
		mmr_t	reserved_0  : 55;
	} sh_ii_int0_s;
} sh_ii_int0_u_t;
#else
typedef union sh_ii_int0_u {
	mmr_t	sh_ii_int0_regval;
	struct {
		mmr_t	reserved_0  : 55;
		mmr_t	send        : 1;
		mmr_t	idx         : 8;
	} sh_ii_int0_s;
} sh_ii_int0_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_II_INT0_CONFIG"                     */
/*                 SHub II Interrupt 0 Config Registers                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ii_int0_config_u {
	mmr_t	sh_ii_int0_config_regval;
	struct {
		mmr_t	type        : 3;
		mmr_t	agt         : 1;
		mmr_t	pid         : 16;
		mmr_t	reserved_0  : 1;
		mmr_t	base        : 29;
		mmr_t	reserved_1  : 14;
	} sh_ii_int0_config_s;
} sh_ii_int0_config_u_t;
#else
typedef union sh_ii_int0_config_u {
	mmr_t	sh_ii_int0_config_regval;
	struct {
		mmr_t	reserved_1  : 14;
		mmr_t	base        : 29;
		mmr_t	reserved_0  : 1;
		mmr_t	pid         : 16;
		mmr_t	agt         : 1;
		mmr_t	type        : 3;
	} sh_ii_int0_config_s;
} sh_ii_int0_config_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_II_INT0_ENABLE"                     */
/*                 SHub II Interrupt 0 Enable Registers                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ii_int0_enable_u {
	mmr_t	sh_ii_int0_enable_regval;
	struct {
		mmr_t	ii_enable   : 1;
		mmr_t	reserved_0  : 63;
	} sh_ii_int0_enable_s;
} sh_ii_int0_enable_u_t;
#else
typedef union sh_ii_int0_enable_u {
	mmr_t	sh_ii_int0_enable_regval;
	struct {
		mmr_t	reserved_0  : 63;
		mmr_t	ii_enable   : 1;
	} sh_ii_int0_enable_s;
} sh_ii_int0_enable_u_t;
#endif

/* ==================================================================== */
/*                        Register "SH_II_INT1"                         */
/*                    SHub II Interrupt 1 Registers                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ii_int1_u {
	mmr_t	sh_ii_int1_regval;
	struct {
		mmr_t	idx         : 8;
		mmr_t	send        : 1;
		mmr_t	reserved_0  : 55;
	} sh_ii_int1_s;
} sh_ii_int1_u_t;
#else
typedef union sh_ii_int1_u {
	mmr_t	sh_ii_int1_regval;
	struct {
		mmr_t	reserved_0  : 55;
		mmr_t	send        : 1;
		mmr_t	idx         : 8;
	} sh_ii_int1_s;
} sh_ii_int1_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_II_INT1_CONFIG"                     */
/*                 SHub II Interrupt 1 Config Registers                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ii_int1_config_u {
	mmr_t	sh_ii_int1_config_regval;
	struct {
		mmr_t	type        : 3;
		mmr_t	agt         : 1;
		mmr_t	pid         : 16;
		mmr_t	reserved_0  : 1;
		mmr_t	base        : 29;
		mmr_t	reserved_1  : 14;
	} sh_ii_int1_config_s;
} sh_ii_int1_config_u_t;
#else
typedef union sh_ii_int1_config_u {
	mmr_t	sh_ii_int1_config_regval;
	struct {
		mmr_t	reserved_1  : 14;
		mmr_t	base        : 29;
		mmr_t	reserved_0  : 1;
		mmr_t	pid         : 16;
		mmr_t	agt         : 1;
		mmr_t	type        : 3;
	} sh_ii_int1_config_s;
} sh_ii_int1_config_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_II_INT1_ENABLE"                     */
/*                 SHub II Interrupt 1 Enable Registers                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ii_int1_enable_u {
	mmr_t	sh_ii_int1_enable_regval;
	struct {
		mmr_t	ii_enable   : 1;
		mmr_t	reserved_0  : 63;
	} sh_ii_int1_enable_s;
} sh_ii_int1_enable_u_t;
#else
typedef union sh_ii_int1_enable_u {
	mmr_t	sh_ii_int1_enable_regval;
	struct {
		mmr_t	reserved_0  : 63;
		mmr_t	ii_enable   : 1;
	} sh_ii_int1_enable_s;
} sh_ii_int1_enable_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_INT_NODE_ID_CONFIG"                   */
/*                 SHub Interrupt Node ID Configuration                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_int_node_id_config_u {
	mmr_t	sh_int_node_id_config_regval;
	struct {
		mmr_t	node_id     : 11;
		mmr_t	id_sel      : 1;
		mmr_t	reserved_0  : 52;
	} sh_int_node_id_config_s;
} sh_int_node_id_config_u_t;
#else
typedef union sh_int_node_id_config_u {
	mmr_t	sh_int_node_id_config_regval;
	struct {
		mmr_t	reserved_0  : 52;
		mmr_t	id_sel      : 1;
		mmr_t	node_id     : 11;
	} sh_int_node_id_config_s;
} sh_int_node_id_config_u_t;
#endif

/* ==================================================================== */
/*                        Register "SH_IPI_INT"                         */
/*               SHub Inter-Processor Interrupt Registers               */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ipi_int_u {
	mmr_t	sh_ipi_int_regval;
	struct {
		mmr_t	type        : 3;
		mmr_t	agt         : 1;
		mmr_t	pid         : 16;
		mmr_t	reserved_0  : 1;
		mmr_t	base        : 29;
		mmr_t	reserved_1  : 2;
		mmr_t	idx         : 8;
		mmr_t	reserved_2  : 3;
		mmr_t	send        : 1;
	} sh_ipi_int_s;
} sh_ipi_int_u_t;
#else
typedef union sh_ipi_int_u {
	mmr_t	sh_ipi_int_regval;
	struct {
		mmr_t	send        : 1;
		mmr_t	reserved_2  : 3;
		mmr_t	idx         : 8;
		mmr_t	reserved_1  : 2;
		mmr_t	base        : 29;
		mmr_t	reserved_0  : 1;
		mmr_t	pid         : 16;
		mmr_t	agt         : 1;
		mmr_t	type        : 3;
	} sh_ipi_int_s;
} sh_ipi_int_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_IPI_INT_ENABLE"                     */
/*           SHub Inter-Processor Interrupt Enable Registers            */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ipi_int_enable_u {
	mmr_t	sh_ipi_int_enable_regval;
	struct {
		mmr_t	pio_enable  : 1;
		mmr_t	reserved_0  : 63;
	} sh_ipi_int_enable_s;
} sh_ipi_int_enable_u_t;
#else
typedef union sh_ipi_int_enable_u {
	mmr_t	sh_ipi_int_enable_regval;
	struct {
		mmr_t	reserved_0  : 63;
		mmr_t	pio_enable  : 1;
	} sh_ipi_int_enable_s;
} sh_ipi_int_enable_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_LOCAL_INT0_CONFIG"                    */
/*                   SHub Local Interrupt 0 Registers                   */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_local_int0_config_u {
	mmr_t	sh_local_int0_config_regval;
	struct {
		mmr_t	type        : 3;
		mmr_t	agt         : 1;
		mmr_t	pid         : 16;
		mmr_t	reserved_0  : 1;
		mmr_t	base        : 29;
		mmr_t	reserved_1  : 2;
		mmr_t	idx         : 8;
		mmr_t	reserved_2  : 4;
	} sh_local_int0_config_s;
} sh_local_int0_config_u_t;
#else
typedef union sh_local_int0_config_u {
	mmr_t	sh_local_int0_config_regval;
	struct {
		mmr_t	reserved_2  : 4;
		mmr_t	idx         : 8;
		mmr_t	reserved_1  : 2;
		mmr_t	base        : 29;
		mmr_t	reserved_0  : 1;
		mmr_t	pid         : 16;
		mmr_t	agt         : 1;
		mmr_t	type        : 3;
	} sh_local_int0_config_s;
} sh_local_int0_config_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_LOCAL_INT0_ENABLE"                    */
/*                    SHub Local Interrupt 0 Enable                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_local_int0_enable_u {
	mmr_t	sh_local_int0_enable_regval;
	struct {
		mmr_t	pi_hw_int           : 1;
		mmr_t	md_hw_int           : 1;
		mmr_t	xn_hw_int           : 1;
		mmr_t	lb_hw_int           : 1;
		mmr_t	ii_hw_int           : 1;
		mmr_t	pi_ce_int           : 1;
		mmr_t	md_ce_int           : 1;
		mmr_t	xn_ce_int           : 1;
		mmr_t	pi_uce_int          : 1;
		mmr_t	md_uce_int          : 1;
		mmr_t	xn_uce_int          : 1;
		mmr_t	reserved_0          : 1;
		mmr_t	system_shutdown_int : 1;
		mmr_t	uart_int            : 1;
		mmr_t	l1_nmi_int          : 1;
		mmr_t	stop_clock          : 1;
		mmr_t	reserved_1          : 48;
	} sh_local_int0_enable_s;
} sh_local_int0_enable_u_t;
#else
typedef union sh_local_int0_enable_u {
	mmr_t	sh_local_int0_enable_regval;
	struct {
		mmr_t	reserved_1          : 48;
		mmr_t	stop_clock          : 1;
		mmr_t	l1_nmi_int          : 1;
		mmr_t	uart_int            : 1;
		mmr_t	system_shutdown_int : 1;
		mmr_t	reserved_0          : 1;
		mmr_t	xn_uce_int          : 1;
		mmr_t	md_uce_int          : 1;
		mmr_t	pi_uce_int          : 1;
		mmr_t	xn_ce_int           : 1;
		mmr_t	md_ce_int           : 1;
		mmr_t	pi_ce_int           : 1;
		mmr_t	ii_hw_int           : 1;
		mmr_t	lb_hw_int           : 1;
		mmr_t	xn_hw_int           : 1;
		mmr_t	md_hw_int           : 1;
		mmr_t	pi_hw_int           : 1;
	} sh_local_int0_enable_s;
} sh_local_int0_enable_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_LOCAL_INT1_CONFIG"                    */
/*                   SHub Local Interrupt 1 Registers                   */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_local_int1_config_u {
	mmr_t	sh_local_int1_config_regval;
	struct {
		mmr_t	type        : 3;
		mmr_t	agt         : 1;
		mmr_t	pid         : 16;
		mmr_t	reserved_0  : 1;
		mmr_t	base        : 29;
		mmr_t	reserved_1  : 2;
		mmr_t	idx         : 8;
		mmr_t	reserved_2  : 4;
	} sh_local_int1_config_s;
} sh_local_int1_config_u_t;
#else
typedef union sh_local_int1_config_u {
	mmr_t	sh_local_int1_config_regval;
	struct {
		mmr_t	reserved_2  : 4;
		mmr_t	idx         : 8;
		mmr_t	reserved_1  : 2;
		mmr_t	base        : 29;
		mmr_t	reserved_0  : 1;
		mmr_t	pid         : 16;
		mmr_t	agt         : 1;
		mmr_t	type        : 3;
	} sh_local_int1_config_s;
} sh_local_int1_config_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_LOCAL_INT1_ENABLE"                    */
/*                    SHub Local Interrupt 1 Enable                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_local_int1_enable_u {
	mmr_t	sh_local_int1_enable_regval;
	struct {
		mmr_t	pi_hw_int           : 1;
		mmr_t	md_hw_int           : 1;
		mmr_t	xn_hw_int           : 1;
		mmr_t	lb_hw_int           : 1;
		mmr_t	ii_hw_int           : 1;
		mmr_t	pi_ce_int           : 1;
		mmr_t	md_ce_int           : 1;
		mmr_t	xn_ce_int           : 1;
		mmr_t	pi_uce_int          : 1;
		mmr_t	md_uce_int          : 1;
		mmr_t	xn_uce_int          : 1;
		mmr_t	reserved_0          : 1;
		mmr_t	system_shutdown_int : 1;
		mmr_t	uart_int            : 1;
		mmr_t	l1_nmi_int          : 1;
		mmr_t	stop_clock          : 1;
		mmr_t	reserved_1          : 48;
	} sh_local_int1_enable_s;
} sh_local_int1_enable_u_t;
#else
typedef union sh_local_int1_enable_u {
	mmr_t	sh_local_int1_enable_regval;
	struct {
		mmr_t	reserved_1          : 48;
		mmr_t	stop_clock          : 1;
		mmr_t	l1_nmi_int          : 1;
		mmr_t	uart_int            : 1;
		mmr_t	system_shutdown_int : 1;
		mmr_t	reserved_0          : 1;
		mmr_t	xn_uce_int          : 1;
		mmr_t	md_uce_int          : 1;
		mmr_t	pi_uce_int          : 1;
		mmr_t	xn_ce_int           : 1;
		mmr_t	md_ce_int           : 1;
		mmr_t	pi_ce_int           : 1;
		mmr_t	ii_hw_int           : 1;
		mmr_t	lb_hw_int           : 1;
		mmr_t	xn_hw_int           : 1;
		mmr_t	md_hw_int           : 1;
		mmr_t	pi_hw_int           : 1;
	} sh_local_int1_enable_s;
} sh_local_int1_enable_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_LOCAL_INT2_CONFIG"                    */
/*                   SHub Local Interrupt 2 Registers                   */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_local_int2_config_u {
	mmr_t	sh_local_int2_config_regval;
	struct {
		mmr_t	type        : 3;
		mmr_t	agt         : 1;
		mmr_t	pid         : 16;
		mmr_t	reserved_0  : 1;
		mmr_t	base        : 29;
		mmr_t	reserved_1  : 2;
		mmr_t	idx         : 8;
		mmr_t	reserved_2  : 4;
	} sh_local_int2_config_s;
} sh_local_int2_config_u_t;
#else
typedef union sh_local_int2_config_u {
	mmr_t	sh_local_int2_config_regval;
	struct {
		mmr_t	reserved_2  : 4;
		mmr_t	idx         : 8;
		mmr_t	reserved_1  : 2;
		mmr_t	base        : 29;
		mmr_t	reserved_0  : 1;
		mmr_t	pid         : 16;
		mmr_t	agt         : 1;
		mmr_t	type        : 3;
	} sh_local_int2_config_s;
} sh_local_int2_config_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_LOCAL_INT2_ENABLE"                    */
/*                    SHub Local Interrupt 2 Enable                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_local_int2_enable_u {
	mmr_t	sh_local_int2_enable_regval;
	struct {
		mmr_t	pi_hw_int           : 1;
		mmr_t	md_hw_int           : 1;
		mmr_t	xn_hw_int           : 1;
		mmr_t	lb_hw_int           : 1;
		mmr_t	ii_hw_int           : 1;
		mmr_t	pi_ce_int           : 1;
		mmr_t	md_ce_int           : 1;
		mmr_t	xn_ce_int           : 1;
		mmr_t	pi_uce_int          : 1;
		mmr_t	md_uce_int          : 1;
		mmr_t	xn_uce_int          : 1;
		mmr_t	reserved_0          : 1;
		mmr_t	system_shutdown_int : 1;
		mmr_t	uart_int            : 1;
		mmr_t	l1_nmi_int          : 1;
		mmr_t	stop_clock          : 1;
		mmr_t	reserved_1          : 48;
	} sh_local_int2_enable_s;
} sh_local_int2_enable_u_t;
#else
typedef union sh_local_int2_enable_u {
	mmr_t	sh_local_int2_enable_regval;
	struct {
		mmr_t	reserved_1          : 48;
		mmr_t	stop_clock          : 1;
		mmr_t	l1_nmi_int          : 1;
		mmr_t	uart_int            : 1;
		mmr_t	system_shutdown_int : 1;
		mmr_t	reserved_0          : 1;
		mmr_t	xn_uce_int          : 1;
		mmr_t	md_uce_int          : 1;
		mmr_t	pi_uce_int          : 1;
		mmr_t	xn_ce_int           : 1;
		mmr_t	md_ce_int           : 1;
		mmr_t	pi_ce_int           : 1;
		mmr_t	ii_hw_int           : 1;
		mmr_t	lb_hw_int           : 1;
		mmr_t	xn_hw_int           : 1;
		mmr_t	md_hw_int           : 1;
		mmr_t	pi_hw_int           : 1;
	} sh_local_int2_enable_s;
} sh_local_int2_enable_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_LOCAL_INT3_CONFIG"                    */
/*                   SHub Local Interrupt 3 Registers                   */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_local_int3_config_u {
	mmr_t	sh_local_int3_config_regval;
	struct {
		mmr_t	type        : 3;
		mmr_t	agt         : 1;
		mmr_t	pid         : 16;
		mmr_t	reserved_0  : 1;
		mmr_t	base        : 29;
		mmr_t	reserved_1  : 2;
		mmr_t	idx         : 8;
		mmr_t	reserved_2  : 4;
	} sh_local_int3_config_s;
} sh_local_int3_config_u_t;
#else
typedef union sh_local_int3_config_u {
	mmr_t	sh_local_int3_config_regval;
	struct {
		mmr_t	reserved_2  : 4;
		mmr_t	idx         : 8;
		mmr_t	reserved_1  : 2;
		mmr_t	base        : 29;
		mmr_t	reserved_0  : 1;
		mmr_t	pid         : 16;
		mmr_t	agt         : 1;
		mmr_t	type        : 3;
	} sh_local_int3_config_s;
} sh_local_int3_config_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_LOCAL_INT3_ENABLE"                    */
/*                    SHub Local Interrupt 3 Enable                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_local_int3_enable_u {
	mmr_t	sh_local_int3_enable_regval;
	struct {
		mmr_t	pi_hw_int           : 1;
		mmr_t	md_hw_int           : 1;
		mmr_t	xn_hw_int           : 1;
		mmr_t	lb_hw_int           : 1;
		mmr_t	ii_hw_int           : 1;
		mmr_t	pi_ce_int           : 1;
		mmr_t	md_ce_int           : 1;
		mmr_t	xn_ce_int           : 1;
		mmr_t	pi_uce_int          : 1;
		mmr_t	md_uce_int          : 1;
		mmr_t	xn_uce_int          : 1;
		mmr_t	reserved_0          : 1;
		mmr_t	system_shutdown_int : 1;
		mmr_t	uart_int            : 1;
		mmr_t	l1_nmi_int          : 1;
		mmr_t	stop_clock          : 1;
		mmr_t	reserved_1          : 48;
	} sh_local_int3_enable_s;
} sh_local_int3_enable_u_t;
#else
typedef union sh_local_int3_enable_u {
	mmr_t	sh_local_int3_enable_regval;
	struct {
		mmr_t	reserved_1          : 48;
		mmr_t	stop_clock          : 1;
		mmr_t	l1_nmi_int          : 1;
		mmr_t	uart_int            : 1;
		mmr_t	system_shutdown_int : 1;
		mmr_t	reserved_0          : 1;
		mmr_t	xn_uce_int          : 1;
		mmr_t	md_uce_int          : 1;
		mmr_t	pi_uce_int          : 1;
		mmr_t	xn_ce_int           : 1;
		mmr_t	md_ce_int           : 1;
		mmr_t	pi_ce_int           : 1;
		mmr_t	ii_hw_int           : 1;
		mmr_t	lb_hw_int           : 1;
		mmr_t	xn_hw_int           : 1;
		mmr_t	md_hw_int           : 1;
		mmr_t	pi_hw_int           : 1;
	} sh_local_int3_enable_s;
} sh_local_int3_enable_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_LOCAL_INT4_CONFIG"                    */
/*                   SHub Local Interrupt 4 Registers                   */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_local_int4_config_u {
	mmr_t	sh_local_int4_config_regval;
	struct {
		mmr_t	type        : 3;
		mmr_t	agt         : 1;
		mmr_t	pid         : 16;
		mmr_t	reserved_0  : 1;
		mmr_t	base        : 29;
		mmr_t	reserved_1  : 2;
		mmr_t	idx         : 8;
		mmr_t	reserved_2  : 4;
	} sh_local_int4_config_s;
} sh_local_int4_config_u_t;
#else
typedef union sh_local_int4_config_u {
	mmr_t	sh_local_int4_config_regval;
	struct {
		mmr_t	reserved_2  : 4;
		mmr_t	idx         : 8;
		mmr_t	reserved_1  : 2;
		mmr_t	base        : 29;
		mmr_t	reserved_0  : 1;
		mmr_t	pid         : 16;
		mmr_t	agt         : 1;
		mmr_t	type        : 3;
	} sh_local_int4_config_s;
} sh_local_int4_config_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_LOCAL_INT4_ENABLE"                    */
/*                    SHub Local Interrupt 4 Enable                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_local_int4_enable_u {
	mmr_t	sh_local_int4_enable_regval;
	struct {
		mmr_t	pi_hw_int           : 1;
		mmr_t	md_hw_int           : 1;
		mmr_t	xn_hw_int           : 1;
		mmr_t	lb_hw_int           : 1;
		mmr_t	ii_hw_int           : 1;
		mmr_t	pi_ce_int           : 1;
		mmr_t	md_ce_int           : 1;
		mmr_t	xn_ce_int           : 1;
		mmr_t	pi_uce_int          : 1;
		mmr_t	md_uce_int          : 1;
		mmr_t	xn_uce_int          : 1;
		mmr_t	reserved_0          : 1;
		mmr_t	system_shutdown_int : 1;
		mmr_t	uart_int            : 1;
		mmr_t	l1_nmi_int          : 1;
		mmr_t	stop_clock          : 1;
		mmr_t	reserved_1          : 48;
	} sh_local_int4_enable_s;
} sh_local_int4_enable_u_t;
#else
typedef union sh_local_int4_enable_u {
	mmr_t	sh_local_int4_enable_regval;
	struct {
		mmr_t	reserved_1          : 48;
		mmr_t	stop_clock          : 1;
		mmr_t	l1_nmi_int          : 1;
		mmr_t	uart_int            : 1;
		mmr_t	system_shutdown_int : 1;
		mmr_t	reserved_0          : 1;
		mmr_t	xn_uce_int          : 1;
		mmr_t	md_uce_int          : 1;
		mmr_t	pi_uce_int          : 1;
		mmr_t	xn_ce_int           : 1;
		mmr_t	md_ce_int           : 1;
		mmr_t	pi_ce_int           : 1;
		mmr_t	ii_hw_int           : 1;
		mmr_t	lb_hw_int           : 1;
		mmr_t	xn_hw_int           : 1;
		mmr_t	md_hw_int           : 1;
		mmr_t	pi_hw_int           : 1;
	} sh_local_int4_enable_s;
} sh_local_int4_enable_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_LOCAL_INT5_CONFIG"                    */
/*                   SHub Local Interrupt 5 Registers                   */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_local_int5_config_u {
	mmr_t	sh_local_int5_config_regval;
	struct {
		mmr_t	type        : 3;
		mmr_t	agt         : 1;
		mmr_t	pid         : 16;
		mmr_t	reserved_0  : 1;
		mmr_t	base        : 29;
		mmr_t	reserved_1  : 2;
		mmr_t	idx         : 8;
		mmr_t	reserved_2  : 4;
	} sh_local_int5_config_s;
} sh_local_int5_config_u_t;
#else
typedef union sh_local_int5_config_u {
	mmr_t	sh_local_int5_config_regval;
	struct {
		mmr_t	reserved_2  : 4;
		mmr_t	idx         : 8;
		mmr_t	reserved_1  : 2;
		mmr_t	base        : 29;
		mmr_t	reserved_0  : 1;
		mmr_t	pid         : 16;
		mmr_t	agt         : 1;
		mmr_t	type        : 3;
	} sh_local_int5_config_s;
} sh_local_int5_config_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_LOCAL_INT5_ENABLE"                    */
/*                    SHub Local Interrupt 5 Enable                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_local_int5_enable_u {
	mmr_t	sh_local_int5_enable_regval;
	struct {
		mmr_t	pi_hw_int           : 1;
		mmr_t	md_hw_int           : 1;
		mmr_t	xn_hw_int           : 1;
		mmr_t	lb_hw_int           : 1;
		mmr_t	ii_hw_int           : 1;
		mmr_t	pi_ce_int           : 1;
		mmr_t	md_ce_int           : 1;
		mmr_t	xn_ce_int           : 1;
		mmr_t	pi_uce_int          : 1;
		mmr_t	md_uce_int          : 1;
		mmr_t	xn_uce_int          : 1;
		mmr_t	reserved_0          : 1;
		mmr_t	system_shutdown_int : 1;
		mmr_t	uart_int            : 1;
		mmr_t	l1_nmi_int          : 1;
		mmr_t	stop_clock          : 1;
		mmr_t	reserved_1          : 48;
	} sh_local_int5_enable_s;
} sh_local_int5_enable_u_t;
#else
typedef union sh_local_int5_enable_u {
	mmr_t	sh_local_int5_enable_regval;
	struct {
		mmr_t	reserved_1          : 48;
		mmr_t	stop_clock          : 1;
		mmr_t	l1_nmi_int          : 1;
		mmr_t	uart_int            : 1;
		mmr_t	system_shutdown_int : 1;
		mmr_t	reserved_0          : 1;
		mmr_t	xn_uce_int          : 1;
		mmr_t	md_uce_int          : 1;
		mmr_t	pi_uce_int          : 1;
		mmr_t	xn_ce_int           : 1;
		mmr_t	md_ce_int           : 1;
		mmr_t	pi_ce_int           : 1;
		mmr_t	ii_hw_int           : 1;
		mmr_t	lb_hw_int           : 1;
		mmr_t	xn_hw_int           : 1;
		mmr_t	md_hw_int           : 1;
		mmr_t	pi_hw_int           : 1;
	} sh_local_int5_enable_s;
} sh_local_int5_enable_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PROC0_ERR_INT_CONFIG"                  */
/*              SHub Processor 0 Error Interrupt Registers              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_proc0_err_int_config_u {
	mmr_t	sh_proc0_err_int_config_regval;
	struct {
		mmr_t	type        : 3;
		mmr_t	agt         : 1;
		mmr_t	pid         : 16;
		mmr_t	reserved_0  : 1;
		mmr_t	base        : 29;
		mmr_t	reserved_1  : 2;
		mmr_t	idx         : 8;
		mmr_t	reserved_2  : 4;
	} sh_proc0_err_int_config_s;
} sh_proc0_err_int_config_u_t;
#else
typedef union sh_proc0_err_int_config_u {
	mmr_t	sh_proc0_err_int_config_regval;
	struct {
		mmr_t	reserved_2  : 4;
		mmr_t	idx         : 8;
		mmr_t	reserved_1  : 2;
		mmr_t	base        : 29;
		mmr_t	reserved_0  : 1;
		mmr_t	pid         : 16;
		mmr_t	agt         : 1;
		mmr_t	type        : 3;
	} sh_proc0_err_int_config_s;
} sh_proc0_err_int_config_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PROC1_ERR_INT_CONFIG"                  */
/*              SHub Processor 1 Error Interrupt Registers              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_proc1_err_int_config_u {
	mmr_t	sh_proc1_err_int_config_regval;
	struct {
		mmr_t	type        : 3;
		mmr_t	agt         : 1;
		mmr_t	pid         : 16;
		mmr_t	reserved_0  : 1;
		mmr_t	base        : 29;
		mmr_t	reserved_1  : 2;
		mmr_t	idx         : 8;
		mmr_t	reserved_2  : 4;
	} sh_proc1_err_int_config_s;
} sh_proc1_err_int_config_u_t;
#else
typedef union sh_proc1_err_int_config_u {
	mmr_t	sh_proc1_err_int_config_regval;
	struct {
		mmr_t	reserved_2  : 4;
		mmr_t	idx         : 8;
		mmr_t	reserved_1  : 2;
		mmr_t	base        : 29;
		mmr_t	reserved_0  : 1;
		mmr_t	pid         : 16;
		mmr_t	agt         : 1;
		mmr_t	type        : 3;
	} sh_proc1_err_int_config_s;
} sh_proc1_err_int_config_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PROC2_ERR_INT_CONFIG"                  */
/*              SHub Processor 2 Error Interrupt Registers              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_proc2_err_int_config_u {
	mmr_t	sh_proc2_err_int_config_regval;
	struct {
		mmr_t	type        : 3;
		mmr_t	agt         : 1;
		mmr_t	pid         : 16;
		mmr_t	reserved_0  : 1;
		mmr_t	base        : 29;
		mmr_t	reserved_1  : 2;
		mmr_t	idx         : 8;
		mmr_t	reserved_2  : 4;
	} sh_proc2_err_int_config_s;
} sh_proc2_err_int_config_u_t;
#else
typedef union sh_proc2_err_int_config_u {
	mmr_t	sh_proc2_err_int_config_regval;
	struct {
		mmr_t	reserved_2  : 4;
		mmr_t	idx         : 8;
		mmr_t	reserved_1  : 2;
		mmr_t	base        : 29;
		mmr_t	reserved_0  : 1;
		mmr_t	pid         : 16;
		mmr_t	agt         : 1;
		mmr_t	type        : 3;
	} sh_proc2_err_int_config_s;
} sh_proc2_err_int_config_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PROC3_ERR_INT_CONFIG"                  */
/*              SHub Processor 3 Error Interrupt Registers              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_proc3_err_int_config_u {
	mmr_t	sh_proc3_err_int_config_regval;
	struct {
		mmr_t	type        : 3;
		mmr_t	agt         : 1;
		mmr_t	pid         : 16;
		mmr_t	reserved_0  : 1;
		mmr_t	base        : 29;
		mmr_t	reserved_1  : 2;
		mmr_t	idx         : 8;
		mmr_t	reserved_2  : 4;
	} sh_proc3_err_int_config_s;
} sh_proc3_err_int_config_u_t;
#else
typedef union sh_proc3_err_int_config_u {
	mmr_t	sh_proc3_err_int_config_regval;
	struct {
		mmr_t	reserved_2  : 4;
		mmr_t	idx         : 8;
		mmr_t	reserved_1  : 2;
		mmr_t	base        : 29;
		mmr_t	reserved_0  : 1;
		mmr_t	pid         : 16;
		mmr_t	agt         : 1;
		mmr_t	type        : 3;
	} sh_proc3_err_int_config_s;
} sh_proc3_err_int_config_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PROC0_ADV_INT_CONFIG"                  */
/*            SHub Processor 0 Advisory Interrupt Registers             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_proc0_adv_int_config_u {
	mmr_t	sh_proc0_adv_int_config_regval;
	struct {
		mmr_t	type        : 3;
		mmr_t	agt         : 1;
		mmr_t	pid         : 16;
		mmr_t	reserved_0  : 1;
		mmr_t	base        : 29;
		mmr_t	reserved_1  : 2;
		mmr_t	idx         : 8;
		mmr_t	reserved_2  : 4;
	} sh_proc0_adv_int_config_s;
} sh_proc0_adv_int_config_u_t;
#else
typedef union sh_proc0_adv_int_config_u {
	mmr_t	sh_proc0_adv_int_config_regval;
	struct {
		mmr_t	reserved_2  : 4;
		mmr_t	idx         : 8;
		mmr_t	reserved_1  : 2;
		mmr_t	base        : 29;
		mmr_t	reserved_0  : 1;
		mmr_t	pid         : 16;
		mmr_t	agt         : 1;
		mmr_t	type        : 3;
	} sh_proc0_adv_int_config_s;
} sh_proc0_adv_int_config_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PROC1_ADV_INT_CONFIG"                  */
/*            SHub Processor 1 Advisory Interrupt Registers             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_proc1_adv_int_config_u {
	mmr_t	sh_proc1_adv_int_config_regval;
	struct {
		mmr_t	type        : 3;
		mmr_t	agt         : 1;
		mmr_t	pid         : 16;
		mmr_t	reserved_0  : 1;
		mmr_t	base        : 29;
		mmr_t	reserved_1  : 2;
		mmr_t	idx         : 8;
		mmr_t	reserved_2  : 4;
	} sh_proc1_adv_int_config_s;
} sh_proc1_adv_int_config_u_t;
#else
typedef union sh_proc1_adv_int_config_u {
	mmr_t	sh_proc1_adv_int_config_regval;
	struct {
		mmr_t	reserved_2  : 4;
		mmr_t	idx         : 8;
		mmr_t	reserved_1  : 2;
		mmr_t	base        : 29;
		mmr_t	reserved_0  : 1;
		mmr_t	pid         : 16;
		mmr_t	agt         : 1;
		mmr_t	type        : 3;
	} sh_proc1_adv_int_config_s;
} sh_proc1_adv_int_config_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PROC2_ADV_INT_CONFIG"                  */
/*            SHub Processor 2 Advisory Interrupt Registers             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_proc2_adv_int_config_u {
	mmr_t	sh_proc2_adv_int_config_regval;
	struct {
		mmr_t	type        : 3;
		mmr_t	agt         : 1;
		mmr_t	pid         : 16;
		mmr_t	reserved_0  : 1;
		mmr_t	base        : 29;
		mmr_t	reserved_1  : 2;
		mmr_t	idx         : 8;
		mmr_t	reserved_2  : 4;
	} sh_proc2_adv_int_config_s;
} sh_proc2_adv_int_config_u_t;
#else
typedef union sh_proc2_adv_int_config_u {
	mmr_t	sh_proc2_adv_int_config_regval;
	struct {
		mmr_t	reserved_2  : 4;
		mmr_t	idx         : 8;
		mmr_t	reserved_1  : 2;
		mmr_t	base        : 29;
		mmr_t	reserved_0  : 1;
		mmr_t	pid         : 16;
		mmr_t	agt         : 1;
		mmr_t	type        : 3;
	} sh_proc2_adv_int_config_s;
} sh_proc2_adv_int_config_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PROC3_ADV_INT_CONFIG"                  */
/*            SHub Processor 3 Advisory Interrupt Registers             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_proc3_adv_int_config_u {
	mmr_t	sh_proc3_adv_int_config_regval;
	struct {
		mmr_t	type        : 3;
		mmr_t	agt         : 1;
		mmr_t	pid         : 16;
		mmr_t	reserved_0  : 1;
		mmr_t	base        : 29;
		mmr_t	reserved_1  : 2;
		mmr_t	idx         : 8;
		mmr_t	reserved_2  : 4;
	} sh_proc3_adv_int_config_s;
} sh_proc3_adv_int_config_u_t;
#else
typedef union sh_proc3_adv_int_config_u {
	mmr_t	sh_proc3_adv_int_config_regval;
	struct {
		mmr_t	reserved_2  : 4;
		mmr_t	idx         : 8;
		mmr_t	reserved_1  : 2;
		mmr_t	base        : 29;
		mmr_t	reserved_0  : 1;
		mmr_t	pid         : 16;
		mmr_t	agt         : 1;
		mmr_t	type        : 3;
	} sh_proc3_adv_int_config_s;
} sh_proc3_adv_int_config_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PROC0_ERR_INT_ENABLE"                  */
/*          SHub Processor 0 Error Interrupt Enable Registers           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_proc0_err_int_enable_u {
	mmr_t	sh_proc0_err_int_enable_regval;
	struct {
		mmr_t	proc0_err_enable : 1;
		mmr_t	reserved_0       : 63;
	} sh_proc0_err_int_enable_s;
} sh_proc0_err_int_enable_u_t;
#else
typedef union sh_proc0_err_int_enable_u {
	mmr_t	sh_proc0_err_int_enable_regval;
	struct {
		mmr_t	reserved_0       : 63;
		mmr_t	proc0_err_enable : 1;
	} sh_proc0_err_int_enable_s;
} sh_proc0_err_int_enable_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PROC1_ERR_INT_ENABLE"                  */
/*          SHub Processor 1 Error Interrupt Enable Registers           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_proc1_err_int_enable_u {
	mmr_t	sh_proc1_err_int_enable_regval;
	struct {
		mmr_t	proc1_err_enable : 1;
		mmr_t	reserved_0       : 63;
	} sh_proc1_err_int_enable_s;
} sh_proc1_err_int_enable_u_t;
#else
typedef union sh_proc1_err_int_enable_u {
	mmr_t	sh_proc1_err_int_enable_regval;
	struct {
		mmr_t	reserved_0       : 63;
		mmr_t	proc1_err_enable : 1;
	} sh_proc1_err_int_enable_s;
} sh_proc1_err_int_enable_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PROC2_ERR_INT_ENABLE"                  */
/*          SHub Processor 2 Error Interrupt Enable Registers           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_proc2_err_int_enable_u {
	mmr_t	sh_proc2_err_int_enable_regval;
	struct {
		mmr_t	proc2_err_enable : 1;
		mmr_t	reserved_0       : 63;
	} sh_proc2_err_int_enable_s;
} sh_proc2_err_int_enable_u_t;
#else
typedef union sh_proc2_err_int_enable_u {
	mmr_t	sh_proc2_err_int_enable_regval;
	struct {
		mmr_t	reserved_0       : 63;
		mmr_t	proc2_err_enable : 1;
	} sh_proc2_err_int_enable_s;
} sh_proc2_err_int_enable_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PROC3_ERR_INT_ENABLE"                  */
/*          SHub Processor 3 Error Interrupt Enable Registers           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_proc3_err_int_enable_u {
	mmr_t	sh_proc3_err_int_enable_regval;
	struct {
		mmr_t	proc3_err_enable : 1;
		mmr_t	reserved_0       : 63;
	} sh_proc3_err_int_enable_s;
} sh_proc3_err_int_enable_u_t;
#else
typedef union sh_proc3_err_int_enable_u {
	mmr_t	sh_proc3_err_int_enable_regval;
	struct {
		mmr_t	reserved_0       : 63;
		mmr_t	proc3_err_enable : 1;
	} sh_proc3_err_int_enable_s;
} sh_proc3_err_int_enable_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PROC0_ADV_INT_ENABLE"                  */
/*         SHub Processor 0 Advisory Interrupt Enable Registers         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_proc0_adv_int_enable_u {
	mmr_t	sh_proc0_adv_int_enable_regval;
	struct {
		mmr_t	proc0_adv_enable : 1;
		mmr_t	reserved_0       : 63;
	} sh_proc0_adv_int_enable_s;
} sh_proc0_adv_int_enable_u_t;
#else
typedef union sh_proc0_adv_int_enable_u {
	mmr_t	sh_proc0_adv_int_enable_regval;
	struct {
		mmr_t	reserved_0       : 63;
		mmr_t	proc0_adv_enable : 1;
	} sh_proc0_adv_int_enable_s;
} sh_proc0_adv_int_enable_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PROC1_ADV_INT_ENABLE"                  */
/*         SHub Processor 1 Advisory Interrupt Enable Registers         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_proc1_adv_int_enable_u {
	mmr_t	sh_proc1_adv_int_enable_regval;
	struct {
		mmr_t	proc1_adv_enable : 1;
		mmr_t	reserved_0       : 63;
	} sh_proc1_adv_int_enable_s;
} sh_proc1_adv_int_enable_u_t;
#else
typedef union sh_proc1_adv_int_enable_u {
	mmr_t	sh_proc1_adv_int_enable_regval;
	struct {
		mmr_t	reserved_0       : 63;
		mmr_t	proc1_adv_enable : 1;
	} sh_proc1_adv_int_enable_s;
} sh_proc1_adv_int_enable_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PROC2_ADV_INT_ENABLE"                  */
/*         SHub Processor 2 Advisory Interrupt Enable Registers         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_proc2_adv_int_enable_u {
	mmr_t	sh_proc2_adv_int_enable_regval;
	struct {
		mmr_t	proc2_adv_enable : 1;
		mmr_t	reserved_0       : 63;
	} sh_proc2_adv_int_enable_s;
} sh_proc2_adv_int_enable_u_t;
#else
typedef union sh_proc2_adv_int_enable_u {
	mmr_t	sh_proc2_adv_int_enable_regval;
	struct {
		mmr_t	reserved_0       : 63;
		mmr_t	proc2_adv_enable : 1;
	} sh_proc2_adv_int_enable_s;
} sh_proc2_adv_int_enable_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PROC3_ADV_INT_ENABLE"                  */
/*         SHub Processor 3 Advisory Interrupt Enable Registers         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_proc3_adv_int_enable_u {
	mmr_t	sh_proc3_adv_int_enable_regval;
	struct {
		mmr_t	proc3_adv_enable : 1;
		mmr_t	reserved_0       : 63;
	} sh_proc3_adv_int_enable_s;
} sh_proc3_adv_int_enable_u_t;
#else
typedef union sh_proc3_adv_int_enable_u {
	mmr_t	sh_proc3_adv_int_enable_regval;
	struct {
		mmr_t	reserved_0       : 63;
		mmr_t	proc3_adv_enable : 1;
	} sh_proc3_adv_int_enable_s;
} sh_proc3_adv_int_enable_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_PROFILE_INT_CONFIG"                   */
/*            SHub Profile Interrupt Configuration Registers            */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_profile_int_config_u {
	mmr_t	sh_profile_int_config_regval;
	struct {
		mmr_t	type        : 3;
		mmr_t	agt         : 1;
		mmr_t	pid         : 16;
		mmr_t	reserved_0  : 1;
		mmr_t	base        : 29;
		mmr_t	reserved_1  : 2;
		mmr_t	idx         : 8;
		mmr_t	reserved_2  : 4;
	} sh_profile_int_config_s;
} sh_profile_int_config_u_t;
#else
typedef union sh_profile_int_config_u {
	mmr_t	sh_profile_int_config_regval;
	struct {
		mmr_t	reserved_2  : 4;
		mmr_t	idx         : 8;
		mmr_t	reserved_1  : 2;
		mmr_t	base        : 29;
		mmr_t	reserved_0  : 1;
		mmr_t	pid         : 16;
		mmr_t	agt         : 1;
		mmr_t	type        : 3;
	} sh_profile_int_config_s;
} sh_profile_int_config_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_PROFILE_INT_ENABLE"                   */
/*               SHub Profile Interrupt Enable Registers                */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_profile_int_enable_u {
	mmr_t	sh_profile_int_enable_regval;
	struct {
		mmr_t	profile_enable : 1;
		mmr_t	reserved_0     : 63;
	} sh_profile_int_enable_s;
} sh_profile_int_enable_u_t;
#else
typedef union sh_profile_int_enable_u {
	mmr_t	sh_profile_int_enable_regval;
	struct {
		mmr_t	reserved_0     : 63;
		mmr_t	profile_enable : 1;
	} sh_profile_int_enable_s;
} sh_profile_int_enable_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_RTC0_INT_CONFIG"                     */
/*                SHub RTC 0 Interrupt Config Registers                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_rtc0_int_config_u {
	mmr_t	sh_rtc0_int_config_regval;
	struct {
		mmr_t	type        : 3;
		mmr_t	agt         : 1;
		mmr_t	pid         : 16;
		mmr_t	reserved_0  : 1;
		mmr_t	base        : 29;
		mmr_t	reserved_1  : 2;
		mmr_t	idx         : 8;
		mmr_t	reserved_2  : 4;
	} sh_rtc0_int_config_s;
} sh_rtc0_int_config_u_t;
#else
typedef union sh_rtc0_int_config_u {
	mmr_t	sh_rtc0_int_config_regval;
	struct {
		mmr_t	reserved_2  : 4;
		mmr_t	idx         : 8;
		mmr_t	reserved_1  : 2;
		mmr_t	base        : 29;
		mmr_t	reserved_0  : 1;
		mmr_t	pid         : 16;
		mmr_t	agt         : 1;
		mmr_t	type        : 3;
	} sh_rtc0_int_config_s;
} sh_rtc0_int_config_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_RTC0_INT_ENABLE"                     */
/*                SHub RTC 0 Interrupt Enable Registers                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_rtc0_int_enable_u {
	mmr_t	sh_rtc0_int_enable_regval;
	struct {
		mmr_t	rtc0_enable : 1;
		mmr_t	reserved_0  : 63;
	} sh_rtc0_int_enable_s;
} sh_rtc0_int_enable_u_t;
#else
typedef union sh_rtc0_int_enable_u {
	mmr_t	sh_rtc0_int_enable_regval;
	struct {
		mmr_t	reserved_0  : 63;
		mmr_t	rtc0_enable : 1;
	} sh_rtc0_int_enable_s;
} sh_rtc0_int_enable_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_RTC1_INT_CONFIG"                     */
/*                SHub RTC 1 Interrupt Config Registers                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_rtc1_int_config_u {
	mmr_t	sh_rtc1_int_config_regval;
	struct {
		mmr_t	type        : 3;
		mmr_t	agt         : 1;
		mmr_t	pid         : 16;
		mmr_t	reserved_0  : 1;
		mmr_t	base        : 29;
		mmr_t	reserved_1  : 2;
		mmr_t	idx         : 8;
		mmr_t	reserved_2  : 4;
	} sh_rtc1_int_config_s;
} sh_rtc1_int_config_u_t;
#else
typedef union sh_rtc1_int_config_u {
	mmr_t	sh_rtc1_int_config_regval;
	struct {
		mmr_t	reserved_2  : 4;
		mmr_t	idx         : 8;
		mmr_t	reserved_1  : 2;
		mmr_t	base        : 29;
		mmr_t	reserved_0  : 1;
		mmr_t	pid         : 16;
		mmr_t	agt         : 1;
		mmr_t	type        : 3;
	} sh_rtc1_int_config_s;
} sh_rtc1_int_config_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_RTC1_INT_ENABLE"                     */
/*                SHub RTC 1 Interrupt Enable Registers                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_rtc1_int_enable_u {
	mmr_t	sh_rtc1_int_enable_regval;
	struct {
		mmr_t	rtc1_enable : 1;
		mmr_t	reserved_0  : 63;
	} sh_rtc1_int_enable_s;
} sh_rtc1_int_enable_u_t;
#else
typedef union sh_rtc1_int_enable_u {
	mmr_t	sh_rtc1_int_enable_regval;
	struct {
		mmr_t	reserved_0  : 63;
		mmr_t	rtc1_enable : 1;
	} sh_rtc1_int_enable_s;
} sh_rtc1_int_enable_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_RTC2_INT_CONFIG"                     */
/*                SHub RTC 2 Interrupt Config Registers                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_rtc2_int_config_u {
	mmr_t	sh_rtc2_int_config_regval;
	struct {
		mmr_t	type        : 3;
		mmr_t	agt         : 1;
		mmr_t	pid         : 16;
		mmr_t	reserved_0  : 1;
		mmr_t	base        : 29;
		mmr_t	reserved_1  : 2;
		mmr_t	idx         : 8;
		mmr_t	reserved_2  : 4;
	} sh_rtc2_int_config_s;
} sh_rtc2_int_config_u_t;
#else
typedef union sh_rtc2_int_config_u {
	mmr_t	sh_rtc2_int_config_regval;
	struct {
		mmr_t	reserved_2  : 4;
		mmr_t	idx         : 8;
		mmr_t	reserved_1  : 2;
		mmr_t	base        : 29;
		mmr_t	reserved_0  : 1;
		mmr_t	pid         : 16;
		mmr_t	agt         : 1;
		mmr_t	type        : 3;
	} sh_rtc2_int_config_s;
} sh_rtc2_int_config_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_RTC2_INT_ENABLE"                     */
/*                SHub RTC 2 Interrupt Enable Registers                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_rtc2_int_enable_u {
	mmr_t	sh_rtc2_int_enable_regval;
	struct {
		mmr_t	rtc2_enable : 1;
		mmr_t	reserved_0  : 63;
	} sh_rtc2_int_enable_s;
} sh_rtc2_int_enable_u_t;
#else
typedef union sh_rtc2_int_enable_u {
	mmr_t	sh_rtc2_int_enable_regval;
	struct {
		mmr_t	reserved_0  : 63;
		mmr_t	rtc2_enable : 1;
	} sh_rtc2_int_enable_s;
} sh_rtc2_int_enable_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_RTC3_INT_CONFIG"                     */
/*                SHub RTC 3 Interrupt Config Registers                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_rtc3_int_config_u {
	mmr_t	sh_rtc3_int_config_regval;
	struct {
		mmr_t	type        : 3;
		mmr_t	agt         : 1;
		mmr_t	pid         : 16;
		mmr_t	reserved_0  : 1;
		mmr_t	base        : 29;
		mmr_t	reserved_1  : 2;
		mmr_t	idx         : 8;
		mmr_t	reserved_2  : 4;
	} sh_rtc3_int_config_s;
} sh_rtc3_int_config_u_t;
#else
typedef union sh_rtc3_int_config_u {
	mmr_t	sh_rtc3_int_config_regval;
	struct {
		mmr_t	reserved_2  : 4;
		mmr_t	idx         : 8;
		mmr_t	reserved_1  : 2;
		mmr_t	base        : 29;
		mmr_t	reserved_0  : 1;
		mmr_t	pid         : 16;
		mmr_t	agt         : 1;
		mmr_t	type        : 3;
	} sh_rtc3_int_config_s;
} sh_rtc3_int_config_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_RTC3_INT_ENABLE"                     */
/*                SHub RTC 3 Interrupt Enable Registers                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_rtc3_int_enable_u {
	mmr_t	sh_rtc3_int_enable_regval;
	struct {
		mmr_t	rtc3_enable : 1;
		mmr_t	reserved_0  : 63;
	} sh_rtc3_int_enable_s;
} sh_rtc3_int_enable_u_t;
#else
typedef union sh_rtc3_int_enable_u {
	mmr_t	sh_rtc3_int_enable_regval;
	struct {
		mmr_t	reserved_0  : 63;
		mmr_t	rtc3_enable : 1;
	} sh_rtc3_int_enable_s;
} sh_rtc3_int_enable_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_EVENT_OCCURRED"                     */
/*                    SHub Interrupt Event Occurred                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_event_occurred_u {
	mmr_t	sh_event_occurred_regval;
	struct {
		mmr_t	pi_hw_int           : 1;
		mmr_t	md_hw_int           : 1;
		mmr_t	xn_hw_int           : 1;
		mmr_t	lb_hw_int           : 1;
		mmr_t	ii_hw_int           : 1;
		mmr_t	pi_ce_int           : 1;
		mmr_t	md_ce_int           : 1;
		mmr_t	xn_ce_int           : 1;
		mmr_t	pi_uce_int          : 1;
		mmr_t	md_uce_int          : 1;
		mmr_t	xn_uce_int          : 1;
		mmr_t	proc0_adv_int       : 1;
		mmr_t	proc1_adv_int       : 1;
		mmr_t	proc2_adv_int       : 1;
		mmr_t	proc3_adv_int       : 1;
		mmr_t	proc0_err_int       : 1;
		mmr_t	proc1_err_int       : 1;
		mmr_t	proc2_err_int       : 1;
		mmr_t	proc3_err_int       : 1;
		mmr_t	system_shutdown_int : 1;
		mmr_t	uart_int            : 1;
		mmr_t	l1_nmi_int          : 1;
		mmr_t	stop_clock          : 1;
		mmr_t	rtc0_int            : 1;
		mmr_t	rtc1_int            : 1;
		mmr_t	rtc2_int            : 1;
		mmr_t	rtc3_int            : 1;
		mmr_t	profile_int         : 1;
		mmr_t	ipi_int             : 1;
		mmr_t	ii_int0             : 1;
		mmr_t	ii_int1             : 1;
		mmr_t	reserved_0          : 33;
	} sh_event_occurred_s;
} sh_event_occurred_u_t;
#else
typedef union sh_event_occurred_u {
	mmr_t	sh_event_occurred_regval;
	struct {
		mmr_t	reserved_0          : 33;
		mmr_t	ii_int1             : 1;
		mmr_t	ii_int0             : 1;
		mmr_t	ipi_int             : 1;
		mmr_t	profile_int         : 1;
		mmr_t	rtc3_int            : 1;
		mmr_t	rtc2_int            : 1;
		mmr_t	rtc1_int            : 1;
		mmr_t	rtc0_int            : 1;
		mmr_t	stop_clock          : 1;
		mmr_t	l1_nmi_int          : 1;
		mmr_t	uart_int            : 1;
		mmr_t	system_shutdown_int : 1;
		mmr_t	proc3_err_int       : 1;
		mmr_t	proc2_err_int       : 1;
		mmr_t	proc1_err_int       : 1;
		mmr_t	proc0_err_int       : 1;
		mmr_t	proc3_adv_int       : 1;
		mmr_t	proc2_adv_int       : 1;
		mmr_t	proc1_adv_int       : 1;
		mmr_t	proc0_adv_int       : 1;
		mmr_t	xn_uce_int          : 1;
		mmr_t	md_uce_int          : 1;
		mmr_t	pi_uce_int          : 1;
		mmr_t	xn_ce_int           : 1;
		mmr_t	md_ce_int           : 1;
		mmr_t	pi_ce_int           : 1;
		mmr_t	ii_hw_int           : 1;
		mmr_t	lb_hw_int           : 1;
		mmr_t	xn_hw_int           : 1;
		mmr_t	md_hw_int           : 1;
		mmr_t	pi_hw_int           : 1;
	} sh_event_occurred_s;
} sh_event_occurred_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_EVENT_OVERFLOW"                     */
/*                SHub Interrupt Event Occurred Overflow                */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_event_overflow_u {
	mmr_t	sh_event_overflow_regval;
	struct {
		mmr_t	pi_hw_int           : 1;
		mmr_t	md_hw_int           : 1;
		mmr_t	xn_hw_int           : 1;
		mmr_t	lb_hw_int           : 1;
		mmr_t	ii_hw_int           : 1;
		mmr_t	pi_ce_int           : 1;
		mmr_t	md_ce_int           : 1;
		mmr_t	xn_ce_int           : 1;
		mmr_t	pi_uce_int          : 1;
		mmr_t	md_uce_int          : 1;
		mmr_t	xn_uce_int          : 1;
		mmr_t	proc0_adv_int       : 1;
		mmr_t	proc1_adv_int       : 1;
		mmr_t	proc2_adv_int       : 1;
		mmr_t	proc3_adv_int       : 1;
		mmr_t	proc0_err_int       : 1;
		mmr_t	proc1_err_int       : 1;
		mmr_t	proc2_err_int       : 1;
		mmr_t	proc3_err_int       : 1;
		mmr_t	system_shutdown_int : 1;
		mmr_t	uart_int            : 1;
		mmr_t	l1_nmi_int          : 1;
		mmr_t	stop_clock          : 1;
		mmr_t	rtc0_int            : 1;
		mmr_t	rtc1_int            : 1;
		mmr_t	rtc2_int            : 1;
		mmr_t	rtc3_int            : 1;
		mmr_t	profile_int         : 1;
		mmr_t	reserved_0          : 36;
	} sh_event_overflow_s;
} sh_event_overflow_u_t;
#else
typedef union sh_event_overflow_u {
	mmr_t	sh_event_overflow_regval;
	struct {
		mmr_t	reserved_0          : 36;
		mmr_t	profile_int         : 1;
		mmr_t	rtc3_int            : 1;
		mmr_t	rtc2_int            : 1;
		mmr_t	rtc1_int            : 1;
		mmr_t	rtc0_int            : 1;
		mmr_t	stop_clock          : 1;
		mmr_t	l1_nmi_int          : 1;
		mmr_t	uart_int            : 1;
		mmr_t	system_shutdown_int : 1;
		mmr_t	proc3_err_int       : 1;
		mmr_t	proc2_err_int       : 1;
		mmr_t	proc1_err_int       : 1;
		mmr_t	proc0_err_int       : 1;
		mmr_t	proc3_adv_int       : 1;
		mmr_t	proc2_adv_int       : 1;
		mmr_t	proc1_adv_int       : 1;
		mmr_t	proc0_adv_int       : 1;
		mmr_t	xn_uce_int          : 1;
		mmr_t	md_uce_int          : 1;
		mmr_t	pi_uce_int          : 1;
		mmr_t	xn_ce_int           : 1;
		mmr_t	md_ce_int           : 1;
		mmr_t	pi_ce_int           : 1;
		mmr_t	ii_hw_int           : 1;
		mmr_t	lb_hw_int           : 1;
		mmr_t	xn_hw_int           : 1;
		mmr_t	md_hw_int           : 1;
		mmr_t	pi_hw_int           : 1;
	} sh_event_overflow_s;
} sh_event_overflow_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_JUNK_BUS_TIME"                      */
/*                           Junk Bus Timing                            */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_junk_bus_time_u {
	mmr_t	sh_junk_bus_time_regval;
	struct {
		mmr_t	fprom_setup_hold : 8;
		mmr_t	fprom_enable     : 8;
		mmr_t	uart_setup_hold  : 8;
		mmr_t	uart_enable      : 8;
		mmr_t	reserved_0       : 32;
	} sh_junk_bus_time_s;
} sh_junk_bus_time_u_t;
#else
typedef union sh_junk_bus_time_u {
	mmr_t	sh_junk_bus_time_regval;
	struct {
		mmr_t	reserved_0       : 32;
		mmr_t	uart_enable      : 8;
		mmr_t	uart_setup_hold  : 8;
		mmr_t	fprom_enable     : 8;
		mmr_t	fprom_setup_hold : 8;
	} sh_junk_bus_time_s;
} sh_junk_bus_time_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_JUNK_LATCH_TIME"                     */
/*                        Junk Bus Latch Timing                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_junk_latch_time_u {
	mmr_t	sh_junk_latch_time_regval;
	struct {
		mmr_t	setup_hold  : 3;
		mmr_t	reserved_0  : 61;
	} sh_junk_latch_time_s;
} sh_junk_latch_time_u_t;
#else
typedef union sh_junk_latch_time_u {
	mmr_t	sh_junk_latch_time_regval;
	struct {
		mmr_t	reserved_0  : 61;
		mmr_t	setup_hold  : 3;
	} sh_junk_latch_time_s;
} sh_junk_latch_time_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_JUNK_NACK_RESET"                     */
/*                     Junk Bus Nack Counter Reset                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_junk_nack_reset_u {
	mmr_t	sh_junk_nack_reset_regval;
	struct {
		mmr_t	pulse       : 1;
		mmr_t	reserved_0  : 63;
	} sh_junk_nack_reset_s;
} sh_junk_nack_reset_u_t;
#else
typedef union sh_junk_nack_reset_u {
	mmr_t	sh_junk_nack_reset_regval;
	struct {
		mmr_t	reserved_0  : 63;
		mmr_t	pulse       : 1;
	} sh_junk_nack_reset_s;
} sh_junk_nack_reset_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_JUNK_BUS_LED0"                      */
/*                            Junk Bus LED0                             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_junk_bus_led0_u {
	mmr_t	sh_junk_bus_led0_regval;
	struct {
		mmr_t	led0_data   : 8;
		mmr_t	reserved_0  : 56;
	} sh_junk_bus_led0_s;
} sh_junk_bus_led0_u_t;
#else
typedef union sh_junk_bus_led0_u {
	mmr_t	sh_junk_bus_led0_regval;
	struct {
		mmr_t	reserved_0  : 56;
		mmr_t	led0_data   : 8;
	} sh_junk_bus_led0_s;
} sh_junk_bus_led0_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_JUNK_BUS_LED1"                      */
/*                            Junk Bus LED1                             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_junk_bus_led1_u {
	mmr_t	sh_junk_bus_led1_regval;
	struct {
		mmr_t	led1_data   : 8;
		mmr_t	reserved_0  : 56;
	} sh_junk_bus_led1_s;
} sh_junk_bus_led1_u_t;
#else
typedef union sh_junk_bus_led1_u {
	mmr_t	sh_junk_bus_led1_regval;
	struct {
		mmr_t	reserved_0  : 56;
		mmr_t	led1_data   : 8;
	} sh_junk_bus_led1_s;
} sh_junk_bus_led1_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_JUNK_BUS_LED2"                      */
/*                            Junk Bus LED2                             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_junk_bus_led2_u {
	mmr_t	sh_junk_bus_led2_regval;
	struct {
		mmr_t	led2_data   : 8;
		mmr_t	reserved_0  : 56;
	} sh_junk_bus_led2_s;
} sh_junk_bus_led2_u_t;
#else
typedef union sh_junk_bus_led2_u {
	mmr_t	sh_junk_bus_led2_regval;
	struct {
		mmr_t	reserved_0  : 56;
		mmr_t	led2_data   : 8;
	} sh_junk_bus_led2_s;
} sh_junk_bus_led2_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_JUNK_BUS_LED3"                      */
/*                            Junk Bus LED3                             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_junk_bus_led3_u {
	mmr_t	sh_junk_bus_led3_regval;
	struct {
		mmr_t	led3_data   : 8;
		mmr_t	reserved_0  : 56;
	} sh_junk_bus_led3_s;
} sh_junk_bus_led3_u_t;
#else
typedef union sh_junk_bus_led3_u {
	mmr_t	sh_junk_bus_led3_regval;
	struct {
		mmr_t	reserved_0  : 56;
		mmr_t	led3_data   : 8;
	} sh_junk_bus_led3_s;
} sh_junk_bus_led3_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_JUNK_ERROR_STATUS"                    */
/*                        Junk Bus Error Status                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_junk_error_status_u {
	mmr_t	sh_junk_error_status_regval;
	struct {
		mmr_t	address     : 47;
		mmr_t	reserved_0  : 1;
		mmr_t	cmd         : 8;
		mmr_t	mode        : 1;
		mmr_t	status      : 4;
		mmr_t	reserved_1  : 3;
	} sh_junk_error_status_s;
} sh_junk_error_status_u_t;
#else
typedef union sh_junk_error_status_u {
	mmr_t	sh_junk_error_status_regval;
	struct {
		mmr_t	reserved_1  : 3;
		mmr_t	status      : 4;
		mmr_t	mode        : 1;
		mmr_t	cmd         : 8;
		mmr_t	reserved_0  : 1;
		mmr_t	address     : 47;
	} sh_junk_error_status_s;
} sh_junk_error_status_u_t;
#endif

/* ==================================================================== */
/*                      Register "SH_NI0_LLP_STAT"                      */
/*               This register describes the LLP status.                */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni0_llp_stat_u {
	mmr_t	sh_ni0_llp_stat_regval;
	struct {
		mmr_t	link_reset_state : 4;
		mmr_t	reserved_0       : 60;
	} sh_ni0_llp_stat_s;
} sh_ni0_llp_stat_u_t;
#else
typedef union sh_ni0_llp_stat_u {
	mmr_t	sh_ni0_llp_stat_regval;
	struct {
		mmr_t	reserved_0       : 60;
		mmr_t	link_reset_state : 4;
	} sh_ni0_llp_stat_s;
} sh_ni0_llp_stat_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_NI0_LLP_RESET"                      */
/*           Writing issues a reset to the network interface            */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni0_llp_reset_u {
	mmr_t	sh_ni0_llp_reset_regval;
	struct {
		mmr_t	link        : 1;
		mmr_t	warm        : 1;
		mmr_t	reserved_0  : 62;
	} sh_ni0_llp_reset_s;
} sh_ni0_llp_reset_u_t;
#else
typedef union sh_ni0_llp_reset_u {
	mmr_t	sh_ni0_llp_reset_regval;
	struct {
		mmr_t	reserved_0  : 62;
		mmr_t	warm        : 1;
		mmr_t	link        : 1;
	} sh_ni0_llp_reset_s;
} sh_ni0_llp_reset_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_NI0_LLP_RESET_EN"                    */
/*                 Controls LLP warm reset propagation                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni0_llp_reset_en_u {
	mmr_t	sh_ni0_llp_reset_en_regval;
	struct {
		mmr_t	ok          : 1;
		mmr_t	reserved_0  : 63;
	} sh_ni0_llp_reset_en_s;
} sh_ni0_llp_reset_en_u_t;
#else
typedef union sh_ni0_llp_reset_en_u {
	mmr_t	sh_ni0_llp_reset_en_regval;
	struct {
		mmr_t	reserved_0  : 63;
		mmr_t	ok          : 1;
	} sh_ni0_llp_reset_en_s;
} sh_ni0_llp_reset_en_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_NI0_LLP_CHAN_MODE"                    */
/*              Sets the signaling mode of LLP and channel              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni0_llp_chan_mode_u {
	mmr_t	sh_ni0_llp_chan_mode_regval;
	struct {
		mmr_t	bitmode32         : 1;
		mmr_t	ac_encode         : 1;
		mmr_t	enable_tuning     : 1;
		mmr_t	enable_rmt_ft_upd : 1;
		mmr_t	enable_clkquad    : 1;
		mmr_t	reserved_0        : 59;
	} sh_ni0_llp_chan_mode_s;
} sh_ni0_llp_chan_mode_u_t;
#else
typedef union sh_ni0_llp_chan_mode_u {
	mmr_t	sh_ni0_llp_chan_mode_regval;
	struct {
		mmr_t	reserved_0        : 59;
		mmr_t	enable_clkquad    : 1;
		mmr_t	enable_rmt_ft_upd : 1;
		mmr_t	enable_tuning     : 1;
		mmr_t	ac_encode         : 1;
		mmr_t	bitmode32         : 1;
	} sh_ni0_llp_chan_mode_s;
} sh_ni0_llp_chan_mode_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_NI0_LLP_CONFIG"                     */
/*              Sets the configuration of LLP and channel               */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni0_llp_config_u {
	mmr_t	sh_ni0_llp_config_regval;
	struct {
		mmr_t	maxburst    : 10;
		mmr_t	maxretry    : 10;
		mmr_t	nulltimeout : 6;
		mmr_t	ftu_time    : 12;
		mmr_t	reserved_0  : 26;
	} sh_ni0_llp_config_s;
} sh_ni0_llp_config_u_t;
#else
typedef union sh_ni0_llp_config_u {
	mmr_t	sh_ni0_llp_config_regval;
	struct {
		mmr_t	reserved_0  : 26;
		mmr_t	ftu_time    : 12;
		mmr_t	nulltimeout : 6;
		mmr_t	maxretry    : 10;
		mmr_t	maxburst    : 10;
	} sh_ni0_llp_config_s;
} sh_ni0_llp_config_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_NI0_LLP_TEST_CTL"                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni0_llp_test_ctl_u {
	mmr_t	sh_ni0_llp_test_ctl_regval;
	struct {
		mmr_t	pattern        : 40;
		mmr_t	send_test_mode : 2;
		mmr_t	reserved_0     : 2;
		mmr_t	wire_sel       : 6;
		mmr_t	reserved_1     : 2;
		mmr_t	lfsr_mode      : 2;
		mmr_t	noise_mode     : 2;
		mmr_t	armcapture     : 1;
		mmr_t	capturecbonly  : 1;
		mmr_t	sendcberror    : 1;
		mmr_t	sendsnerror    : 1;
		mmr_t	fakesnerror    : 1;
		mmr_t	captured       : 1;
		mmr_t	cberror        : 1;
		mmr_t	reserved_2     : 1;
	} sh_ni0_llp_test_ctl_s;
} sh_ni0_llp_test_ctl_u_t;
#else
typedef union sh_ni0_llp_test_ctl_u {
	mmr_t	sh_ni0_llp_test_ctl_regval;
	struct {
		mmr_t	reserved_2     : 1;
		mmr_t	cberror        : 1;
		mmr_t	captured       : 1;
		mmr_t	fakesnerror    : 1;
		mmr_t	sendsnerror    : 1;
		mmr_t	sendcberror    : 1;
		mmr_t	capturecbonly  : 1;
		mmr_t	armcapture     : 1;
		mmr_t	noise_mode     : 2;
		mmr_t	lfsr_mode      : 2;
		mmr_t	reserved_1     : 2;
		mmr_t	wire_sel       : 6;
		mmr_t	reserved_0     : 2;
		mmr_t	send_test_mode : 2;
		mmr_t	pattern        : 40;
	} sh_ni0_llp_test_ctl_s;
} sh_ni0_llp_test_ctl_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_NI0_LLP_CAPT_WD1"                    */
/*                    low order 64-bit captured word                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni0_llp_capt_wd1_u {
	mmr_t	sh_ni0_llp_capt_wd1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_ni0_llp_capt_wd1_s;
} sh_ni0_llp_capt_wd1_u_t;
#else
typedef union sh_ni0_llp_capt_wd1_u {
	mmr_t	sh_ni0_llp_capt_wd1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_ni0_llp_capt_wd1_s;
} sh_ni0_llp_capt_wd1_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_NI0_LLP_CAPT_WD2"                    */
/*                   high order 64-bit captured word                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni0_llp_capt_wd2_u {
	mmr_t	sh_ni0_llp_capt_wd2_regval;
	struct {
		mmr_t	data        : 64;
	} sh_ni0_llp_capt_wd2_s;
} sh_ni0_llp_capt_wd2_u_t;
#else
typedef union sh_ni0_llp_capt_wd2_u {
	mmr_t	sh_ni0_llp_capt_wd2_regval;
	struct {
		mmr_t	data        : 64;
	} sh_ni0_llp_capt_wd2_s;
} sh_ni0_llp_capt_wd2_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_NI0_LLP_CAPT_SBCB"                    */
/*                 captured sideband, sequence, and CRC                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni0_llp_capt_sbcb_u {
	mmr_t	sh_ni0_llp_capt_sbcb_regval;
	struct {
		mmr_t	capturedrcvsbsn  : 16;
		mmr_t	capturedrcvcrc   : 16;
		mmr_t	sentallcberrors  : 1;
		mmr_t	sentallsnerrors  : 1;
		mmr_t	fakedallsnerrors : 1;
		mmr_t	chargeoverflow   : 1;
		mmr_t	chargeunderflow  : 1;
		mmr_t	reserved_0       : 27;
	} sh_ni0_llp_capt_sbcb_s;
} sh_ni0_llp_capt_sbcb_u_t;
#else
typedef union sh_ni0_llp_capt_sbcb_u {
	mmr_t	sh_ni0_llp_capt_sbcb_regval;
	struct {
		mmr_t	reserved_0       : 27;
		mmr_t	chargeunderflow  : 1;
		mmr_t	chargeoverflow   : 1;
		mmr_t	fakedallsnerrors : 1;
		mmr_t	sentallsnerrors  : 1;
		mmr_t	sentallcberrors  : 1;
		mmr_t	capturedrcvcrc   : 16;
		mmr_t	capturedrcvsbsn  : 16;
	} sh_ni0_llp_capt_sbcb_s;
} sh_ni0_llp_capt_sbcb_u_t;
#endif

/* ==================================================================== */
/*                      Register "SH_NI0_LLP_ERR"                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni0_llp_err_u {
	mmr_t	sh_ni0_llp_err_regval;
	struct {
		mmr_t	rx_sn_err_count : 8;
		mmr_t	rx_cb_err_count : 8;
		mmr_t	retry_count     : 8;
		mmr_t	retry_timeout   : 1;
		mmr_t	rcv_link_reset  : 1;
		mmr_t	squash          : 1;
		mmr_t	power_not_ok    : 1;
		mmr_t	wire_cnt        : 24;
		mmr_t	wire_overflow   : 1;
		mmr_t	reserved_0      : 11;
	} sh_ni0_llp_err_s;
} sh_ni0_llp_err_u_t;
#else
typedef union sh_ni0_llp_err_u {
	mmr_t	sh_ni0_llp_err_regval;
	struct {
		mmr_t	reserved_0      : 11;
		mmr_t	wire_overflow   : 1;
		mmr_t	wire_cnt        : 24;
		mmr_t	power_not_ok    : 1;
		mmr_t	squash          : 1;
		mmr_t	rcv_link_reset  : 1;
		mmr_t	retry_timeout   : 1;
		mmr_t	retry_count     : 8;
		mmr_t	rx_cb_err_count : 8;
		mmr_t	rx_sn_err_count : 8;
	} sh_ni0_llp_err_s;
} sh_ni0_llp_err_u_t;
#endif

/* ==================================================================== */
/*                      Register "SH_NI1_LLP_STAT"                      */
/*               This register describes the LLP status.                */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni1_llp_stat_u {
	mmr_t	sh_ni1_llp_stat_regval;
	struct {
		mmr_t	link_reset_state : 4;
		mmr_t	reserved_0       : 60;
	} sh_ni1_llp_stat_s;
} sh_ni1_llp_stat_u_t;
#else
typedef union sh_ni1_llp_stat_u {
	mmr_t	sh_ni1_llp_stat_regval;
	struct {
		mmr_t	reserved_0       : 60;
		mmr_t	link_reset_state : 4;
	} sh_ni1_llp_stat_s;
} sh_ni1_llp_stat_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_NI1_LLP_RESET"                      */
/*           Writing issues a reset to the network interface            */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni1_llp_reset_u {
	mmr_t	sh_ni1_llp_reset_regval;
	struct {
		mmr_t	link        : 1;
		mmr_t	warm        : 1;
		mmr_t	reserved_0  : 62;
	} sh_ni1_llp_reset_s;
} sh_ni1_llp_reset_u_t;
#else
typedef union sh_ni1_llp_reset_u {
	mmr_t	sh_ni1_llp_reset_regval;
	struct {
		mmr_t	reserved_0  : 62;
		mmr_t	warm        : 1;
		mmr_t	link        : 1;
	} sh_ni1_llp_reset_s;
} sh_ni1_llp_reset_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_NI1_LLP_RESET_EN"                    */
/*                 Controls LLP warm reset propagation                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni1_llp_reset_en_u {
	mmr_t	sh_ni1_llp_reset_en_regval;
	struct {
		mmr_t	ok          : 1;
		mmr_t	reserved_0  : 63;
	} sh_ni1_llp_reset_en_s;
} sh_ni1_llp_reset_en_u_t;
#else
typedef union sh_ni1_llp_reset_en_u {
	mmr_t	sh_ni1_llp_reset_en_regval;
	struct {
		mmr_t	reserved_0  : 63;
		mmr_t	ok          : 1;
	} sh_ni1_llp_reset_en_s;
} sh_ni1_llp_reset_en_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_NI1_LLP_CHAN_MODE"                    */
/*              Sets the signaling mode of LLP and channel              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni1_llp_chan_mode_u {
	mmr_t	sh_ni1_llp_chan_mode_regval;
	struct {
		mmr_t	bitmode32         : 1;
		mmr_t	ac_encode         : 1;
		mmr_t	enable_tuning     : 1;
		mmr_t	enable_rmt_ft_upd : 1;
		mmr_t	enable_clkquad    : 1;
		mmr_t	reserved_0        : 59;
	} sh_ni1_llp_chan_mode_s;
} sh_ni1_llp_chan_mode_u_t;
#else
typedef union sh_ni1_llp_chan_mode_u {
	mmr_t	sh_ni1_llp_chan_mode_regval;
	struct {
		mmr_t	reserved_0        : 59;
		mmr_t	enable_clkquad    : 1;
		mmr_t	enable_rmt_ft_upd : 1;
		mmr_t	enable_tuning     : 1;
		mmr_t	ac_encode         : 1;
		mmr_t	bitmode32         : 1;
	} sh_ni1_llp_chan_mode_s;
} sh_ni1_llp_chan_mode_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_NI1_LLP_CONFIG"                     */
/*              Sets the configuration of LLP and channel               */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni1_llp_config_u {
	mmr_t	sh_ni1_llp_config_regval;
	struct {
		mmr_t	maxburst    : 10;
		mmr_t	maxretry    : 10;
		mmr_t	nulltimeout : 6;
		mmr_t	ftu_time    : 12;
		mmr_t	reserved_0  : 26;
	} sh_ni1_llp_config_s;
} sh_ni1_llp_config_u_t;
#else
typedef union sh_ni1_llp_config_u {
	mmr_t	sh_ni1_llp_config_regval;
	struct {
		mmr_t	reserved_0  : 26;
		mmr_t	ftu_time    : 12;
		mmr_t	nulltimeout : 6;
		mmr_t	maxretry    : 10;
		mmr_t	maxburst    : 10;
	} sh_ni1_llp_config_s;
} sh_ni1_llp_config_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_NI1_LLP_TEST_CTL"                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni1_llp_test_ctl_u {
	mmr_t	sh_ni1_llp_test_ctl_regval;
	struct {
		mmr_t	pattern        : 40;
		mmr_t	send_test_mode : 2;
		mmr_t	reserved_0     : 2;
		mmr_t	wire_sel       : 6;
		mmr_t	reserved_1     : 2;
		mmr_t	lfsr_mode      : 2;
		mmr_t	noise_mode     : 2;
		mmr_t	armcapture     : 1;
		mmr_t	capturecbonly  : 1;
		mmr_t	sendcberror    : 1;
		mmr_t	sendsnerror    : 1;
		mmr_t	fakesnerror    : 1;
		mmr_t	captured       : 1;
		mmr_t	cberror        : 1;
		mmr_t	reserved_2     : 1;
	} sh_ni1_llp_test_ctl_s;
} sh_ni1_llp_test_ctl_u_t;
#else
typedef union sh_ni1_llp_test_ctl_u {
	mmr_t	sh_ni1_llp_test_ctl_regval;
	struct {
		mmr_t	reserved_2     : 1;
		mmr_t	cberror        : 1;
		mmr_t	captured       : 1;
		mmr_t	fakesnerror    : 1;
		mmr_t	sendsnerror    : 1;
		mmr_t	sendcberror    : 1;
		mmr_t	capturecbonly  : 1;
		mmr_t	armcapture     : 1;
		mmr_t	noise_mode     : 2;
		mmr_t	lfsr_mode      : 2;
		mmr_t	reserved_1     : 2;
		mmr_t	wire_sel       : 6;
		mmr_t	reserved_0     : 2;
		mmr_t	send_test_mode : 2;
		mmr_t	pattern        : 40;
	} sh_ni1_llp_test_ctl_s;
} sh_ni1_llp_test_ctl_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_NI1_LLP_CAPT_WD1"                    */
/*                    low order 64-bit captured word                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni1_llp_capt_wd1_u {
	mmr_t	sh_ni1_llp_capt_wd1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_ni1_llp_capt_wd1_s;
} sh_ni1_llp_capt_wd1_u_t;
#else
typedef union sh_ni1_llp_capt_wd1_u {
	mmr_t	sh_ni1_llp_capt_wd1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_ni1_llp_capt_wd1_s;
} sh_ni1_llp_capt_wd1_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_NI1_LLP_CAPT_WD2"                    */
/*                   high order 64-bit captured word                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni1_llp_capt_wd2_u {
	mmr_t	sh_ni1_llp_capt_wd2_regval;
	struct {
		mmr_t	data        : 64;
	} sh_ni1_llp_capt_wd2_s;
} sh_ni1_llp_capt_wd2_u_t;
#else
typedef union sh_ni1_llp_capt_wd2_u {
	mmr_t	sh_ni1_llp_capt_wd2_regval;
	struct {
		mmr_t	data        : 64;
	} sh_ni1_llp_capt_wd2_s;
} sh_ni1_llp_capt_wd2_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_NI1_LLP_CAPT_SBCB"                    */
/*                 captured sideband, sequence, and CRC                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni1_llp_capt_sbcb_u {
	mmr_t	sh_ni1_llp_capt_sbcb_regval;
	struct {
		mmr_t	capturedrcvsbsn  : 16;
		mmr_t	capturedrcvcrc   : 16;
		mmr_t	sentallcberrors  : 1;
		mmr_t	sentallsnerrors  : 1;
		mmr_t	fakedallsnerrors : 1;
		mmr_t	chargeoverflow   : 1;
		mmr_t	chargeunderflow  : 1;
		mmr_t	reserved_0       : 27;
	} sh_ni1_llp_capt_sbcb_s;
} sh_ni1_llp_capt_sbcb_u_t;
#else
typedef union sh_ni1_llp_capt_sbcb_u {
	mmr_t	sh_ni1_llp_capt_sbcb_regval;
	struct {
		mmr_t	reserved_0       : 27;
		mmr_t	chargeunderflow  : 1;
		mmr_t	chargeoverflow   : 1;
		mmr_t	fakedallsnerrors : 1;
		mmr_t	sentallsnerrors  : 1;
		mmr_t	sentallcberrors  : 1;
		mmr_t	capturedrcvcrc   : 16;
		mmr_t	capturedrcvsbsn  : 16;
	} sh_ni1_llp_capt_sbcb_s;
} sh_ni1_llp_capt_sbcb_u_t;
#endif

/* ==================================================================== */
/*                      Register "SH_NI1_LLP_ERR"                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni1_llp_err_u {
	mmr_t	sh_ni1_llp_err_regval;
	struct {
		mmr_t	rx_sn_err_count : 8;
		mmr_t	rx_cb_err_count : 8;
		mmr_t	retry_count     : 8;
		mmr_t	retry_timeout   : 1;
		mmr_t	rcv_link_reset  : 1;
		mmr_t	squash          : 1;
		mmr_t	power_not_ok    : 1;
		mmr_t	wire_cnt        : 24;
		mmr_t	wire_overflow   : 1;
		mmr_t	reserved_0      : 11;
	} sh_ni1_llp_err_s;
} sh_ni1_llp_err_u_t;
#else
typedef union sh_ni1_llp_err_u {
	mmr_t	sh_ni1_llp_err_regval;
	struct {
		mmr_t	reserved_0      : 11;
		mmr_t	wire_overflow   : 1;
		mmr_t	wire_cnt        : 24;
		mmr_t	power_not_ok    : 1;
		mmr_t	squash          : 1;
		mmr_t	rcv_link_reset  : 1;
		mmr_t	retry_timeout   : 1;
		mmr_t	retry_count     : 8;
		mmr_t	rx_cb_err_count : 8;
		mmr_t	rx_sn_err_count : 8;
	} sh_ni1_llp_err_s;
} sh_ni1_llp_err_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XNNI0_LLP_TO_FIFO02_FLOW"                */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni0_llp_to_fifo02_flow_u {
	mmr_t	sh_xnni0_llp_to_fifo02_flow_regval;
	struct {
		mmr_t	debit_vc0_withhold   : 6;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	debit_vc2_withhold   : 6;
		mmr_t	reserved_1           : 1;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	reserved_2           : 8;
		mmr_t	credit_vc0_dyn       : 6;
		mmr_t	reserved_3           : 2;
		mmr_t	credit_vc0_cap       : 6;
		mmr_t	reserved_4           : 10;
		mmr_t	credit_vc2_dyn       : 6;
		mmr_t	reserved_5           : 2;
		mmr_t	credit_vc2_cap       : 6;
		mmr_t	reserved_6           : 2;
	} sh_xnni0_llp_to_fifo02_flow_s;
} sh_xnni0_llp_to_fifo02_flow_u_t;
#else
typedef union sh_xnni0_llp_to_fifo02_flow_u {
	mmr_t	sh_xnni0_llp_to_fifo02_flow_regval;
	struct {
		mmr_t	reserved_6           : 2;
		mmr_t	credit_vc2_cap       : 6;
		mmr_t	reserved_5           : 2;
		mmr_t	credit_vc2_dyn       : 6;
		mmr_t	reserved_4           : 10;
		mmr_t	credit_vc0_cap       : 6;
		mmr_t	reserved_3           : 2;
		mmr_t	credit_vc0_dyn       : 6;
		mmr_t	reserved_2           : 8;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	reserved_1           : 1;
		mmr_t	debit_vc2_withhold   : 6;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc0_withhold   : 6;
	} sh_xnni0_llp_to_fifo02_flow_s;
} sh_xnni0_llp_to_fifo02_flow_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XNNI0_LLP_TO_FIFO13_FLOW"                */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni0_llp_to_fifo13_flow_u {
	mmr_t	sh_xnni0_llp_to_fifo13_flow_regval;
	struct {
		mmr_t	debit_vc0_withhold   : 6;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	debit_vc2_withhold   : 6;
		mmr_t	reserved_1           : 1;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	reserved_2           : 8;
		mmr_t	credit_vc0_dyn       : 6;
		mmr_t	reserved_3           : 2;
		mmr_t	credit_vc0_cap       : 6;
		mmr_t	reserved_4           : 10;
		mmr_t	credit_vc2_dyn       : 6;
		mmr_t	reserved_5           : 2;
		mmr_t	credit_vc2_cap       : 6;
		mmr_t	reserved_6           : 2;
	} sh_xnni0_llp_to_fifo13_flow_s;
} sh_xnni0_llp_to_fifo13_flow_u_t;
#else
typedef union sh_xnni0_llp_to_fifo13_flow_u {
	mmr_t	sh_xnni0_llp_to_fifo13_flow_regval;
	struct {
		mmr_t	reserved_6           : 2;
		mmr_t	credit_vc2_cap       : 6;
		mmr_t	reserved_5           : 2;
		mmr_t	credit_vc2_dyn       : 6;
		mmr_t	reserved_4           : 10;
		mmr_t	credit_vc0_cap       : 6;
		mmr_t	reserved_3           : 2;
		mmr_t	credit_vc0_dyn       : 6;
		mmr_t	reserved_2           : 8;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	reserved_1           : 1;
		mmr_t	debit_vc2_withhold   : 6;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc0_withhold   : 6;
	} sh_xnni0_llp_to_fifo13_flow_s;
} sh_xnni0_llp_to_fifo13_flow_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_XNNI0_LLP_DEBIT_FLOW"                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni0_llp_debit_flow_u {
	mmr_t	sh_xnni0_llp_debit_flow_regval;
	struct {
		mmr_t	debit_vc0_dyn : 5;
		mmr_t	reserved_0    : 3;
		mmr_t	debit_vc0_cap : 5;
		mmr_t	reserved_1    : 3;
		mmr_t	debit_vc1_dyn : 5;
		mmr_t	reserved_2    : 3;
		mmr_t	debit_vc1_cap : 5;
		mmr_t	reserved_3    : 3;
		mmr_t	debit_vc2_dyn : 5;
		mmr_t	reserved_4    : 3;
		mmr_t	debit_vc2_cap : 5;
		mmr_t	reserved_5    : 3;
		mmr_t	debit_vc3_dyn : 5;
		mmr_t	reserved_6    : 3;
		mmr_t	debit_vc3_cap : 5;
		mmr_t	reserved_7    : 3;
	} sh_xnni0_llp_debit_flow_s;
} sh_xnni0_llp_debit_flow_u_t;
#else
typedef union sh_xnni0_llp_debit_flow_u {
	mmr_t	sh_xnni0_llp_debit_flow_regval;
	struct {
		mmr_t	reserved_7    : 3;
		mmr_t	debit_vc3_cap : 5;
		mmr_t	reserved_6    : 3;
		mmr_t	debit_vc3_dyn : 5;
		mmr_t	reserved_5    : 3;
		mmr_t	debit_vc2_cap : 5;
		mmr_t	reserved_4    : 3;
		mmr_t	debit_vc2_dyn : 5;
		mmr_t	reserved_3    : 3;
		mmr_t	debit_vc1_cap : 5;
		mmr_t	reserved_2    : 3;
		mmr_t	debit_vc1_dyn : 5;
		mmr_t	reserved_1    : 3;
		mmr_t	debit_vc0_cap : 5;
		mmr_t	reserved_0    : 3;
		mmr_t	debit_vc0_dyn : 5;
	} sh_xnni0_llp_debit_flow_s;
} sh_xnni0_llp_debit_flow_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_XNNI0_LINK_0_FLOW"                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni0_link_0_flow_u {
	mmr_t	sh_xnni0_link_0_flow_regval;
	struct {
		mmr_t	debit_vc0_withhold   : 6;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	credit_vc0_test      : 7;
		mmr_t	reserved_1           : 1;
		mmr_t	credit_vc0_dyn       : 7;
		mmr_t	reserved_2           : 1;
		mmr_t	credit_vc0_cap       : 7;
		mmr_t	reserved_3           : 33;
	} sh_xnni0_link_0_flow_s;
} sh_xnni0_link_0_flow_u_t;
#else
typedef union sh_xnni0_link_0_flow_u {
	mmr_t	sh_xnni0_link_0_flow_regval;
	struct {
		mmr_t	reserved_3           : 33;
		mmr_t	credit_vc0_cap       : 7;
		mmr_t	reserved_2           : 1;
		mmr_t	credit_vc0_dyn       : 7;
		mmr_t	reserved_1           : 1;
		mmr_t	credit_vc0_test      : 7;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc0_withhold   : 6;
	} sh_xnni0_link_0_flow_s;
} sh_xnni0_link_0_flow_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_XNNI0_LINK_1_FLOW"                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni0_link_1_flow_u {
	mmr_t	sh_xnni0_link_1_flow_regval;
	struct {
		mmr_t	debit_vc1_withhold   : 6;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc1_force_cred : 1;
		mmr_t	credit_vc1_test      : 7;
		mmr_t	reserved_1           : 1;
		mmr_t	credit_vc1_dyn       : 7;
		mmr_t	reserved_2           : 1;
		mmr_t	credit_vc1_cap       : 7;
		mmr_t	reserved_3           : 33;
	} sh_xnni0_link_1_flow_s;
} sh_xnni0_link_1_flow_u_t;
#else
typedef union sh_xnni0_link_1_flow_u {
	mmr_t	sh_xnni0_link_1_flow_regval;
	struct {
		mmr_t	reserved_3           : 33;
		mmr_t	credit_vc1_cap       : 7;
		mmr_t	reserved_2           : 1;
		mmr_t	credit_vc1_dyn       : 7;
		mmr_t	reserved_1           : 1;
		mmr_t	credit_vc1_test      : 7;
		mmr_t	debit_vc1_force_cred : 1;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc1_withhold   : 6;
	} sh_xnni0_link_1_flow_s;
} sh_xnni0_link_1_flow_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_XNNI0_LINK_2_FLOW"                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni0_link_2_flow_u {
	mmr_t	sh_xnni0_link_2_flow_regval;
	struct {
		mmr_t	debit_vc2_withhold   : 6;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	credit_vc2_test      : 7;
		mmr_t	reserved_1           : 1;
		mmr_t	credit_vc2_dyn       : 7;
		mmr_t	reserved_2           : 1;
		mmr_t	credit_vc2_cap       : 7;
		mmr_t	reserved_3           : 33;
	} sh_xnni0_link_2_flow_s;
} sh_xnni0_link_2_flow_u_t;
#else
typedef union sh_xnni0_link_2_flow_u {
	mmr_t	sh_xnni0_link_2_flow_regval;
	struct {
		mmr_t	reserved_3           : 33;
		mmr_t	credit_vc2_cap       : 7;
		mmr_t	reserved_2           : 1;
		mmr_t	credit_vc2_dyn       : 7;
		mmr_t	reserved_1           : 1;
		mmr_t	credit_vc2_test      : 7;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc2_withhold   : 6;
	} sh_xnni0_link_2_flow_s;
} sh_xnni0_link_2_flow_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_XNNI0_LINK_3_FLOW"                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni0_link_3_flow_u {
	mmr_t	sh_xnni0_link_3_flow_regval;
	struct {
		mmr_t	debit_vc3_withhold   : 6;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc3_force_cred : 1;
		mmr_t	credit_vc3_test      : 7;
		mmr_t	reserved_1           : 1;
		mmr_t	credit_vc3_dyn       : 7;
		mmr_t	reserved_2           : 1;
		mmr_t	credit_vc3_cap       : 7;
		mmr_t	reserved_3           : 33;
	} sh_xnni0_link_3_flow_s;
} sh_xnni0_link_3_flow_u_t;
#else
typedef union sh_xnni0_link_3_flow_u {
	mmr_t	sh_xnni0_link_3_flow_regval;
	struct {
		mmr_t	reserved_3           : 33;
		mmr_t	credit_vc3_cap       : 7;
		mmr_t	reserved_2           : 1;
		mmr_t	credit_vc3_dyn       : 7;
		mmr_t	reserved_1           : 1;
		mmr_t	credit_vc3_test      : 7;
		mmr_t	debit_vc3_force_cred : 1;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc3_withhold   : 6;
	} sh_xnni0_link_3_flow_s;
} sh_xnni0_link_3_flow_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XNNI1_LLP_TO_FIFO02_FLOW"                */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni1_llp_to_fifo02_flow_u {
	mmr_t	sh_xnni1_llp_to_fifo02_flow_regval;
	struct {
		mmr_t	debit_vc0_withhold   : 6;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	debit_vc2_withhold   : 6;
		mmr_t	reserved_1           : 1;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	reserved_2           : 8;
		mmr_t	credit_vc0_dyn       : 6;
		mmr_t	reserved_3           : 2;
		mmr_t	credit_vc0_cap       : 6;
		mmr_t	reserved_4           : 10;
		mmr_t	credit_vc2_dyn       : 6;
		mmr_t	reserved_5           : 2;
		mmr_t	credit_vc2_cap       : 6;
		mmr_t	reserved_6           : 2;
	} sh_xnni1_llp_to_fifo02_flow_s;
} sh_xnni1_llp_to_fifo02_flow_u_t;
#else
typedef union sh_xnni1_llp_to_fifo02_flow_u {
	mmr_t	sh_xnni1_llp_to_fifo02_flow_regval;
	struct {
		mmr_t	reserved_6           : 2;
		mmr_t	credit_vc2_cap       : 6;
		mmr_t	reserved_5           : 2;
		mmr_t	credit_vc2_dyn       : 6;
		mmr_t	reserved_4           : 10;
		mmr_t	credit_vc0_cap       : 6;
		mmr_t	reserved_3           : 2;
		mmr_t	credit_vc0_dyn       : 6;
		mmr_t	reserved_2           : 8;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	reserved_1           : 1;
		mmr_t	debit_vc2_withhold   : 6;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc0_withhold   : 6;
	} sh_xnni1_llp_to_fifo02_flow_s;
} sh_xnni1_llp_to_fifo02_flow_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XNNI1_LLP_TO_FIFO13_FLOW"                */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni1_llp_to_fifo13_flow_u {
	mmr_t	sh_xnni1_llp_to_fifo13_flow_regval;
	struct {
		mmr_t	debit_vc0_withhold   : 6;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	debit_vc2_withhold   : 6;
		mmr_t	reserved_1           : 1;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	reserved_2           : 8;
		mmr_t	credit_vc0_dyn       : 6;
		mmr_t	reserved_3           : 2;
		mmr_t	credit_vc0_cap       : 6;
		mmr_t	reserved_4           : 10;
		mmr_t	credit_vc2_dyn       : 6;
		mmr_t	reserved_5           : 2;
		mmr_t	credit_vc2_cap       : 6;
		mmr_t	reserved_6           : 2;
	} sh_xnni1_llp_to_fifo13_flow_s;
} sh_xnni1_llp_to_fifo13_flow_u_t;
#else
typedef union sh_xnni1_llp_to_fifo13_flow_u {
	mmr_t	sh_xnni1_llp_to_fifo13_flow_regval;
	struct {
		mmr_t	reserved_6           : 2;
		mmr_t	credit_vc2_cap       : 6;
		mmr_t	reserved_5           : 2;
		mmr_t	credit_vc2_dyn       : 6;
		mmr_t	reserved_4           : 10;
		mmr_t	credit_vc0_cap       : 6;
		mmr_t	reserved_3           : 2;
		mmr_t	credit_vc0_dyn       : 6;
		mmr_t	reserved_2           : 8;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	reserved_1           : 1;
		mmr_t	debit_vc2_withhold   : 6;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc0_withhold   : 6;
	} sh_xnni1_llp_to_fifo13_flow_s;
} sh_xnni1_llp_to_fifo13_flow_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_XNNI1_LLP_DEBIT_FLOW"                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni1_llp_debit_flow_u {
	mmr_t	sh_xnni1_llp_debit_flow_regval;
	struct {
		mmr_t	debit_vc0_dyn : 5;
		mmr_t	reserved_0    : 3;
		mmr_t	debit_vc0_cap : 5;
		mmr_t	reserved_1    : 3;
		mmr_t	debit_vc1_dyn : 5;
		mmr_t	reserved_2    : 3;
		mmr_t	debit_vc1_cap : 5;
		mmr_t	reserved_3    : 3;
		mmr_t	debit_vc2_dyn : 5;
		mmr_t	reserved_4    : 3;
		mmr_t	debit_vc2_cap : 5;
		mmr_t	reserved_5    : 3;
		mmr_t	debit_vc3_dyn : 5;
		mmr_t	reserved_6    : 3;
		mmr_t	debit_vc3_cap : 5;
		mmr_t	reserved_7    : 3;
	} sh_xnni1_llp_debit_flow_s;
} sh_xnni1_llp_debit_flow_u_t;
#else
typedef union sh_xnni1_llp_debit_flow_u {
	mmr_t	sh_xnni1_llp_debit_flow_regval;
	struct {
		mmr_t	reserved_7    : 3;
		mmr_t	debit_vc3_cap : 5;
		mmr_t	reserved_6    : 3;
		mmr_t	debit_vc3_dyn : 5;
		mmr_t	reserved_5    : 3;
		mmr_t	debit_vc2_cap : 5;
		mmr_t	reserved_4    : 3;
		mmr_t	debit_vc2_dyn : 5;
		mmr_t	reserved_3    : 3;
		mmr_t	debit_vc1_cap : 5;
		mmr_t	reserved_2    : 3;
		mmr_t	debit_vc1_dyn : 5;
		mmr_t	reserved_1    : 3;
		mmr_t	debit_vc0_cap : 5;
		mmr_t	reserved_0    : 3;
		mmr_t	debit_vc0_dyn : 5;
	} sh_xnni1_llp_debit_flow_s;
} sh_xnni1_llp_debit_flow_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_XNNI1_LINK_0_FLOW"                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni1_link_0_flow_u {
	mmr_t	sh_xnni1_link_0_flow_regval;
	struct {
		mmr_t	debit_vc0_withhold   : 6;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	credit_vc0_test      : 7;
		mmr_t	reserved_1           : 1;
		mmr_t	credit_vc0_dyn       : 7;
		mmr_t	reserved_2           : 1;
		mmr_t	credit_vc0_cap       : 7;
		mmr_t	reserved_3           : 33;
	} sh_xnni1_link_0_flow_s;
} sh_xnni1_link_0_flow_u_t;
#else
typedef union sh_xnni1_link_0_flow_u {
	mmr_t	sh_xnni1_link_0_flow_regval;
	struct {
		mmr_t	reserved_3           : 33;
		mmr_t	credit_vc0_cap       : 7;
		mmr_t	reserved_2           : 1;
		mmr_t	credit_vc0_dyn       : 7;
		mmr_t	reserved_1           : 1;
		mmr_t	credit_vc0_test      : 7;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc0_withhold   : 6;
	} sh_xnni1_link_0_flow_s;
} sh_xnni1_link_0_flow_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_XNNI1_LINK_1_FLOW"                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni1_link_1_flow_u {
	mmr_t	sh_xnni1_link_1_flow_regval;
	struct {
		mmr_t	debit_vc1_withhold   : 6;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc1_force_cred : 1;
		mmr_t	credit_vc1_test      : 7;
		mmr_t	reserved_1           : 1;
		mmr_t	credit_vc1_dyn       : 7;
		mmr_t	reserved_2           : 1;
		mmr_t	credit_vc1_cap       : 7;
		mmr_t	reserved_3           : 33;
	} sh_xnni1_link_1_flow_s;
} sh_xnni1_link_1_flow_u_t;
#else
typedef union sh_xnni1_link_1_flow_u {
	mmr_t	sh_xnni1_link_1_flow_regval;
	struct {
		mmr_t	reserved_3           : 33;
		mmr_t	credit_vc1_cap       : 7;
		mmr_t	reserved_2           : 1;
		mmr_t	credit_vc1_dyn       : 7;
		mmr_t	reserved_1           : 1;
		mmr_t	credit_vc1_test      : 7;
		mmr_t	debit_vc1_force_cred : 1;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc1_withhold   : 6;
	} sh_xnni1_link_1_flow_s;
} sh_xnni1_link_1_flow_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_XNNI1_LINK_2_FLOW"                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni1_link_2_flow_u {
	mmr_t	sh_xnni1_link_2_flow_regval;
	struct {
		mmr_t	debit_vc2_withhold   : 6;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	credit_vc2_test      : 7;
		mmr_t	reserved_1           : 1;
		mmr_t	credit_vc2_dyn       : 7;
		mmr_t	reserved_2           : 1;
		mmr_t	credit_vc2_cap       : 7;
		mmr_t	reserved_3           : 33;
	} sh_xnni1_link_2_flow_s;
} sh_xnni1_link_2_flow_u_t;
#else
typedef union sh_xnni1_link_2_flow_u {
	mmr_t	sh_xnni1_link_2_flow_regval;
	struct {
		mmr_t	reserved_3           : 33;
		mmr_t	credit_vc2_cap       : 7;
		mmr_t	reserved_2           : 1;
		mmr_t	credit_vc2_dyn       : 7;
		mmr_t	reserved_1           : 1;
		mmr_t	credit_vc2_test      : 7;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc2_withhold   : 6;
	} sh_xnni1_link_2_flow_s;
} sh_xnni1_link_2_flow_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_XNNI1_LINK_3_FLOW"                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni1_link_3_flow_u {
	mmr_t	sh_xnni1_link_3_flow_regval;
	struct {
		mmr_t	debit_vc3_withhold   : 6;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc3_force_cred : 1;
		mmr_t	credit_vc3_test      : 7;
		mmr_t	reserved_1           : 1;
		mmr_t	credit_vc3_dyn       : 7;
		mmr_t	reserved_2           : 1;
		mmr_t	credit_vc3_cap       : 7;
		mmr_t	reserved_3           : 33;
	} sh_xnni1_link_3_flow_s;
} sh_xnni1_link_3_flow_u_t;
#else
typedef union sh_xnni1_link_3_flow_u {
	mmr_t	sh_xnni1_link_3_flow_regval;
	struct {
		mmr_t	reserved_3           : 33;
		mmr_t	credit_vc3_cap       : 7;
		mmr_t	reserved_2           : 1;
		mmr_t	credit_vc3_dyn       : 7;
		mmr_t	reserved_1           : 1;
		mmr_t	credit_vc3_test      : 7;
		mmr_t	debit_vc3_force_cred : 1;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc3_withhold   : 6;
	} sh_xnni1_link_3_flow_s;
} sh_xnni1_link_3_flow_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_IILB_LOCAL_TABLE"                    */
/*                          local lookup table                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_iilb_local_table_u {
	mmr_t	sh_iilb_local_table_regval;
	struct {
		mmr_t	dir0        : 4;
		mmr_t	v0          : 1;
		mmr_t	ni_sel0     : 1;
		mmr_t	reserved_0  : 57;
		mmr_t	valid       : 1;
	} sh_iilb_local_table_s;
} sh_iilb_local_table_u_t;
#else
typedef union sh_iilb_local_table_u {
	mmr_t	sh_iilb_local_table_regval;
	struct {
		mmr_t	valid       : 1;
		mmr_t	reserved_0  : 57;
		mmr_t	ni_sel0     : 1;
		mmr_t	v0          : 1;
		mmr_t	dir0        : 4;
	} sh_iilb_local_table_s;
} sh_iilb_local_table_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_IILB_GLOBAL_TABLE"                    */
/*                         global lookup table                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_iilb_global_table_u {
	mmr_t	sh_iilb_global_table_regval;
	struct {
		mmr_t	dir0        : 4;
		mmr_t	v0          : 1;
		mmr_t	ni_sel0     : 1;
		mmr_t	reserved_0  : 57;
		mmr_t	valid       : 1;
	} sh_iilb_global_table_s;
} sh_iilb_global_table_u_t;
#else
typedef union sh_iilb_global_table_u {
	mmr_t	sh_iilb_global_table_regval;
	struct {
		mmr_t	valid       : 1;
		mmr_t	reserved_0  : 57;
		mmr_t	ni_sel0     : 1;
		mmr_t	v0          : 1;
		mmr_t	dir0        : 4;
	} sh_iilb_global_table_s;
} sh_iilb_global_table_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_IILB_OVER_RIDE_TABLE"                  */
/*              If enabled, bypass the Global/Local tables              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_iilb_over_ride_table_u {
	mmr_t	sh_iilb_over_ride_table_regval;
	struct {
		mmr_t	dir0        : 4;
		mmr_t	v0          : 1;
		mmr_t	ni_sel0     : 1;
		mmr_t	reserved_0  : 57;
		mmr_t	enable      : 1;
	} sh_iilb_over_ride_table_s;
} sh_iilb_over_ride_table_u_t;
#else
typedef union sh_iilb_over_ride_table_u {
	mmr_t	sh_iilb_over_ride_table_regval;
	struct {
		mmr_t	enable      : 1;
		mmr_t	reserved_0  : 57;
		mmr_t	ni_sel0     : 1;
		mmr_t	v0          : 1;
		mmr_t	dir0        : 4;
	} sh_iilb_over_ride_table_s;
} sh_iilb_over_ride_table_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_IILB_RSP_PLANE_HINT"                   */
/*  If enabled, invert incoming response only plane hint bit before lo  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_iilb_rsp_plane_hint_u {
	mmr_t	sh_iilb_rsp_plane_hint_regval;
	struct {
		mmr_t	reserved_0  : 64;
	} sh_iilb_rsp_plane_hint_s;
} sh_iilb_rsp_plane_hint_u_t;
#else
typedef union sh_iilb_rsp_plane_hint_u {
	mmr_t	sh_iilb_rsp_plane_hint_regval;
	struct {
		mmr_t	reserved_0  : 64;
	} sh_iilb_rsp_plane_hint_s;
} sh_iilb_rsp_plane_hint_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_PI_LOCAL_TABLE"                     */
/*                          local lookup table                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_local_table_u {
	mmr_t	sh_pi_local_table_regval;
	struct {
		mmr_t	dir0        : 4;
		mmr_t	v0          : 1;
		mmr_t	ni_sel0     : 1;
		mmr_t	reserved_0  : 2;
		mmr_t	dir1        : 4;
		mmr_t	v1          : 1;
		mmr_t	ni_sel1     : 1;
		mmr_t	reserved_1  : 49;
		mmr_t	valid       : 1;
	} sh_pi_local_table_s;
} sh_pi_local_table_u_t;
#else
typedef union sh_pi_local_table_u {
	mmr_t	sh_pi_local_table_regval;
	struct {
		mmr_t	valid       : 1;
		mmr_t	reserved_1  : 49;
		mmr_t	ni_sel1     : 1;
		mmr_t	v1          : 1;
		mmr_t	dir1        : 4;
		mmr_t	reserved_0  : 2;
		mmr_t	ni_sel0     : 1;
		mmr_t	v0          : 1;
		mmr_t	dir0        : 4;
	} sh_pi_local_table_s;
} sh_pi_local_table_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_PI_GLOBAL_TABLE"                     */
/*                         global lookup table                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_global_table_u {
	mmr_t	sh_pi_global_table_regval;
	struct {
		mmr_t	dir0        : 4;
		mmr_t	v0          : 1;
		mmr_t	ni_sel0     : 1;
		mmr_t	reserved_0  : 2;
		mmr_t	dir1        : 4;
		mmr_t	v1          : 1;
		mmr_t	ni_sel1     : 1;
		mmr_t	reserved_1  : 49;
		mmr_t	valid       : 1;
	} sh_pi_global_table_s;
} sh_pi_global_table_u_t;
#else
typedef union sh_pi_global_table_u {
	mmr_t	sh_pi_global_table_regval;
	struct {
		mmr_t	valid       : 1;
		mmr_t	reserved_1  : 49;
		mmr_t	ni_sel1     : 1;
		mmr_t	v1          : 1;
		mmr_t	dir1        : 4;
		mmr_t	reserved_0  : 2;
		mmr_t	ni_sel0     : 1;
		mmr_t	v0          : 1;
		mmr_t	dir0        : 4;
	} sh_pi_global_table_s;
} sh_pi_global_table_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_PI_OVER_RIDE_TABLE"                   */
/*              If enabled, bypass the Global/Local tables              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_over_ride_table_u {
	mmr_t	sh_pi_over_ride_table_regval;
	struct {
		mmr_t	dir0        : 4;
		mmr_t	v0          : 1;
		mmr_t	ni_sel0     : 1;
		mmr_t	reserved_0  : 2;
		mmr_t	dir1        : 4;
		mmr_t	v1          : 1;
		mmr_t	ni_sel1     : 1;
		mmr_t	reserved_1  : 49;
		mmr_t	enable      : 1;
	} sh_pi_over_ride_table_s;
} sh_pi_over_ride_table_u_t;
#else
typedef union sh_pi_over_ride_table_u {
	mmr_t	sh_pi_over_ride_table_regval;
	struct {
		mmr_t	enable      : 1;
		mmr_t	reserved_1  : 49;
		mmr_t	ni_sel1     : 1;
		mmr_t	v1          : 1;
		mmr_t	dir1        : 4;
		mmr_t	reserved_0  : 2;
		mmr_t	ni_sel0     : 1;
		mmr_t	v0          : 1;
		mmr_t	dir0        : 4;
	} sh_pi_over_ride_table_s;
} sh_pi_over_ride_table_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_PI_RSP_PLANE_HINT"                    */
/*  If enabled, invert incoming response only plane hint bit before lo  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_rsp_plane_hint_u {
	mmr_t	sh_pi_rsp_plane_hint_regval;
	struct {
		mmr_t	invert      : 1;
		mmr_t	reserved_0  : 63;
	} sh_pi_rsp_plane_hint_s;
} sh_pi_rsp_plane_hint_u_t;
#else
typedef union sh_pi_rsp_plane_hint_u {
	mmr_t	sh_pi_rsp_plane_hint_regval;
	struct {
		mmr_t	reserved_0  : 63;
		mmr_t	invert      : 1;
	} sh_pi_rsp_plane_hint_s;
} sh_pi_rsp_plane_hint_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_NI0_LOCAL_TABLE"                     */
/*                          local lookup table                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni0_local_table_u {
	mmr_t	sh_ni0_local_table_regval;
	struct {
		mmr_t	dir0        : 4;
		mmr_t	v0          : 1;
		mmr_t	reserved_0  : 58;
		mmr_t	valid       : 1;
	} sh_ni0_local_table_s;
} sh_ni0_local_table_u_t;
#else
typedef union sh_ni0_local_table_u {
	mmr_t	sh_ni0_local_table_regval;
	struct {
		mmr_t	valid       : 1;
		mmr_t	reserved_0  : 58;
		mmr_t	v0          : 1;
		mmr_t	dir0        : 4;
	} sh_ni0_local_table_s;
} sh_ni0_local_table_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_NI0_GLOBAL_TABLE"                    */
/*                         global lookup table                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni0_global_table_u {
	mmr_t	sh_ni0_global_table_regval;
	struct {
		mmr_t	dir0        : 4;
		mmr_t	v0          : 1;
		mmr_t	reserved_0  : 58;
		mmr_t	valid       : 1;
	} sh_ni0_global_table_s;
} sh_ni0_global_table_u_t;
#else
typedef union sh_ni0_global_table_u {
	mmr_t	sh_ni0_global_table_regval;
	struct {
		mmr_t	valid       : 1;
		mmr_t	reserved_0  : 58;
		mmr_t	v0          : 1;
		mmr_t	dir0        : 4;
	} sh_ni0_global_table_s;
} sh_ni0_global_table_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_NI0_OVER_RIDE_TABLE"                   */
/*              If enabled, bypass the Global/Local tables              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni0_over_ride_table_u {
	mmr_t	sh_ni0_over_ride_table_regval;
	struct {
		mmr_t	dir0        : 4;
		mmr_t	v0          : 1;
		mmr_t	reserved_0  : 58;
		mmr_t	enable      : 1;
	} sh_ni0_over_ride_table_s;
} sh_ni0_over_ride_table_u_t;
#else
typedef union sh_ni0_over_ride_table_u {
	mmr_t	sh_ni0_over_ride_table_regval;
	struct {
		mmr_t	enable      : 1;
		mmr_t	reserved_0  : 58;
		mmr_t	v0          : 1;
		mmr_t	dir0        : 4;
	} sh_ni0_over_ride_table_s;
} sh_ni0_over_ride_table_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_NI0_RSP_PLANE_HINT"                   */
/*  If enabled, invert incoming response only plane hint bit before lo  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni0_rsp_plane_hint_u {
	mmr_t	sh_ni0_rsp_plane_hint_regval;
	struct {
		mmr_t	reserved_0  : 64;
	} sh_ni0_rsp_plane_hint_s;
} sh_ni0_rsp_plane_hint_u_t;
#else
typedef union sh_ni0_rsp_plane_hint_u {
	mmr_t	sh_ni0_rsp_plane_hint_regval;
	struct {
		mmr_t	reserved_0  : 64;
	} sh_ni0_rsp_plane_hint_s;
} sh_ni0_rsp_plane_hint_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_NI1_LOCAL_TABLE"                     */
/*                          local lookup table                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni1_local_table_u {
	mmr_t	sh_ni1_local_table_regval;
	struct {
		mmr_t	dir0        : 4;
		mmr_t	v0          : 1;
		mmr_t	reserved_0  : 58;
		mmr_t	valid       : 1;
	} sh_ni1_local_table_s;
} sh_ni1_local_table_u_t;
#else
typedef union sh_ni1_local_table_u {
	mmr_t	sh_ni1_local_table_regval;
	struct {
		mmr_t	valid       : 1;
		mmr_t	reserved_0  : 58;
		mmr_t	v0          : 1;
		mmr_t	dir0        : 4;
	} sh_ni1_local_table_s;
} sh_ni1_local_table_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_NI1_GLOBAL_TABLE"                    */
/*                         global lookup table                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni1_global_table_u {
	mmr_t	sh_ni1_global_table_regval;
	struct {
		mmr_t	dir0        : 4;
		mmr_t	v0          : 1;
		mmr_t	reserved_0  : 58;
		mmr_t	valid       : 1;
	} sh_ni1_global_table_s;
} sh_ni1_global_table_u_t;
#else
typedef union sh_ni1_global_table_u {
	mmr_t	sh_ni1_global_table_regval;
	struct {
		mmr_t	valid       : 1;
		mmr_t	reserved_0  : 58;
		mmr_t	v0          : 1;
		mmr_t	dir0        : 4;
	} sh_ni1_global_table_s;
} sh_ni1_global_table_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_NI1_OVER_RIDE_TABLE"                   */
/*              If enabled, bypass the Global/Local tables              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni1_over_ride_table_u {
	mmr_t	sh_ni1_over_ride_table_regval;
	struct {
		mmr_t	dir0        : 4;
		mmr_t	v0          : 1;
		mmr_t	reserved_0  : 58;
		mmr_t	enable      : 1;
	} sh_ni1_over_ride_table_s;
} sh_ni1_over_ride_table_u_t;
#else
typedef union sh_ni1_over_ride_table_u {
	mmr_t	sh_ni1_over_ride_table_regval;
	struct {
		mmr_t	enable      : 1;
		mmr_t	reserved_0  : 58;
		mmr_t	v0          : 1;
		mmr_t	dir0        : 4;
	} sh_ni1_over_ride_table_s;
} sh_ni1_over_ride_table_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_NI1_RSP_PLANE_HINT"                   */
/*  If enabled, invert incoming response only plane hint bit before lo  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni1_rsp_plane_hint_u {
	mmr_t	sh_ni1_rsp_plane_hint_regval;
	struct {
		mmr_t	reserved_0  : 64;
	} sh_ni1_rsp_plane_hint_s;
} sh_ni1_rsp_plane_hint_u_t;
#else
typedef union sh_ni1_rsp_plane_hint_u {
	mmr_t	sh_ni1_rsp_plane_hint_regval;
	struct {
		mmr_t	reserved_0  : 64;
	} sh_ni1_rsp_plane_hint_s;
} sh_ni1_rsp_plane_hint_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_MD_LOCAL_TABLE"                     */
/*                          local lookup table                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_local_table_u {
	mmr_t	sh_md_local_table_regval;
	struct {
		mmr_t	dir0        : 4;
		mmr_t	v0          : 1;
		mmr_t	ni_sel0     : 1;
		mmr_t	reserved_0  : 2;
		mmr_t	dir1        : 4;
		mmr_t	v1          : 1;
		mmr_t	ni_sel1     : 1;
		mmr_t	reserved_1  : 49;
		mmr_t	valid       : 1;
	} sh_md_local_table_s;
} sh_md_local_table_u_t;
#else
typedef union sh_md_local_table_u {
	mmr_t	sh_md_local_table_regval;
	struct {
		mmr_t	valid       : 1;
		mmr_t	reserved_1  : 49;
		mmr_t	ni_sel1     : 1;
		mmr_t	v1          : 1;
		mmr_t	dir1        : 4;
		mmr_t	reserved_0  : 2;
		mmr_t	ni_sel0     : 1;
		mmr_t	v0          : 1;
		mmr_t	dir0        : 4;
	} sh_md_local_table_s;
} sh_md_local_table_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_MD_GLOBAL_TABLE"                     */
/*                         global lookup table                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_global_table_u {
	mmr_t	sh_md_global_table_regval;
	struct {
		mmr_t	dir0        : 4;
		mmr_t	v0          : 1;
		mmr_t	ni_sel0     : 1;
		mmr_t	reserved_0  : 2;
		mmr_t	dir1        : 4;
		mmr_t	v1          : 1;
		mmr_t	ni_sel1     : 1;
		mmr_t	reserved_1  : 49;
		mmr_t	valid       : 1;
	} sh_md_global_table_s;
} sh_md_global_table_u_t;
#else
typedef union sh_md_global_table_u {
	mmr_t	sh_md_global_table_regval;
	struct {
		mmr_t	valid       : 1;
		mmr_t	reserved_1  : 49;
		mmr_t	ni_sel1     : 1;
		mmr_t	v1          : 1;
		mmr_t	dir1        : 4;
		mmr_t	reserved_0  : 2;
		mmr_t	ni_sel0     : 1;
		mmr_t	v0          : 1;
		mmr_t	dir0        : 4;
	} sh_md_global_table_s;
} sh_md_global_table_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_MD_OVER_RIDE_TABLE"                   */
/*              If enabled, bypass the Global/Local tables              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_over_ride_table_u {
	mmr_t	sh_md_over_ride_table_regval;
	struct {
		mmr_t	dir0        : 4;
		mmr_t	v0          : 1;
		mmr_t	ni_sel0     : 1;
		mmr_t	reserved_0  : 2;
		mmr_t	dir1        : 4;
		mmr_t	v1          : 1;
		mmr_t	ni_sel1     : 1;
		mmr_t	reserved_1  : 49;
		mmr_t	enable      : 1;
	} sh_md_over_ride_table_s;
} sh_md_over_ride_table_u_t;
#else
typedef union sh_md_over_ride_table_u {
	mmr_t	sh_md_over_ride_table_regval;
	struct {
		mmr_t	enable      : 1;
		mmr_t	reserved_1  : 49;
		mmr_t	ni_sel1     : 1;
		mmr_t	v1          : 1;
		mmr_t	dir1        : 4;
		mmr_t	reserved_0  : 2;
		mmr_t	ni_sel0     : 1;
		mmr_t	v0          : 1;
		mmr_t	dir0        : 4;
	} sh_md_over_ride_table_s;
} sh_md_over_ride_table_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_MD_RSP_PLANE_HINT"                    */
/*  If enabled, invert incoming response only plane hint bit before lo  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_rsp_plane_hint_u {
	mmr_t	sh_md_rsp_plane_hint_regval;
	struct {
		mmr_t	invert      : 1;
		mmr_t	reserved_0  : 63;
	} sh_md_rsp_plane_hint_s;
} sh_md_rsp_plane_hint_u_t;
#else
typedef union sh_md_rsp_plane_hint_u {
	mmr_t	sh_md_rsp_plane_hint_regval;
	struct {
		mmr_t	reserved_0  : 63;
		mmr_t	invert      : 1;
	} sh_md_rsp_plane_hint_s;
} sh_md_rsp_plane_hint_u_t;
#endif

/* ==================================================================== */
/*                       Register "SH_LB_LIQ_CTL"                       */
/*                       Local Block LIQ Control                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_lb_liq_ctl_u {
	mmr_t	sh_lb_liq_ctl_regval;
	struct {
		mmr_t	liq_req_ctl        : 5;
		mmr_t	reserved_0         : 3;
		mmr_t	liq_rpl_ctl        : 4;
		mmr_t	reserved_1         : 4;
		mmr_t	force_rq_credit    : 1;
		mmr_t	force_rp_credit    : 1;
		mmr_t	force_linvv_credit : 1;
		mmr_t	reserved_2         : 45;
	} sh_lb_liq_ctl_s;
} sh_lb_liq_ctl_u_t;
#else
typedef union sh_lb_liq_ctl_u {
	mmr_t	sh_lb_liq_ctl_regval;
	struct {
		mmr_t	reserved_2         : 45;
		mmr_t	force_linvv_credit : 1;
		mmr_t	force_rp_credit    : 1;
		mmr_t	force_rq_credit    : 1;
		mmr_t	reserved_1         : 4;
		mmr_t	liq_rpl_ctl        : 4;
		mmr_t	reserved_0         : 3;
		mmr_t	liq_req_ctl        : 5;
	} sh_lb_liq_ctl_s;
} sh_lb_liq_ctl_u_t;
#endif

/* ==================================================================== */
/*                       Register "SH_LB_LOQ_CTL"                       */
/*                       Local Block LOQ Control                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_lb_loq_ctl_u {
	mmr_t	sh_lb_loq_ctl_regval;
	struct {
		mmr_t	loq_req_ctl : 1;
		mmr_t	loq_rpl_ctl : 1;
		mmr_t	reserved_0  : 62;
	} sh_lb_loq_ctl_s;
} sh_lb_loq_ctl_u_t;
#else
typedef union sh_lb_loq_ctl_u {
	mmr_t	sh_lb_loq_ctl_regval;
	struct {
		mmr_t	reserved_0  : 62;
		mmr_t	loq_rpl_ctl : 1;
		mmr_t	loq_req_ctl : 1;
	} sh_lb_loq_ctl_s;
} sh_lb_loq_ctl_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_LB_MAX_REP_CREDIT_CNT"                  */
/*               Maximum number of reply credits from XN                */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_lb_max_rep_credit_cnt_u {
	mmr_t	sh_lb_max_rep_credit_cnt_regval;
	struct {
		mmr_t	max_cnt     : 5;
		mmr_t	reserved_0  : 59;
	} sh_lb_max_rep_credit_cnt_s;
} sh_lb_max_rep_credit_cnt_u_t;
#else
typedef union sh_lb_max_rep_credit_cnt_u {
	mmr_t	sh_lb_max_rep_credit_cnt_regval;
	struct {
		mmr_t	reserved_0  : 59;
		mmr_t	max_cnt     : 5;
	} sh_lb_max_rep_credit_cnt_s;
} sh_lb_max_rep_credit_cnt_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_LB_MAX_REQ_CREDIT_CNT"                  */
/*              Maximum number of request credits from XN               */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_lb_max_req_credit_cnt_u {
	mmr_t	sh_lb_max_req_credit_cnt_regval;
	struct {
		mmr_t	max_cnt     : 5;
		mmr_t	reserved_0  : 59;
	} sh_lb_max_req_credit_cnt_s;
} sh_lb_max_req_credit_cnt_u_t;
#else
typedef union sh_lb_max_req_credit_cnt_u {
	mmr_t	sh_lb_max_req_credit_cnt_regval;
	struct {
		mmr_t	reserved_0  : 59;
		mmr_t	max_cnt     : 5;
	} sh_lb_max_req_credit_cnt_s;
} sh_lb_max_req_credit_cnt_u_t;
#endif

/* ==================================================================== */
/*                      Register "SH_PIO_TIME_OUT"                      */
/*                    Local Block PIO time out value                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pio_time_out_u {
	mmr_t	sh_pio_time_out_regval;
	struct {
		mmr_t	value       : 16;
		mmr_t	reserved_0  : 48;
	} sh_pio_time_out_s;
} sh_pio_time_out_u_t;
#else
typedef union sh_pio_time_out_u {
	mmr_t	sh_pio_time_out_regval;
	struct {
		mmr_t	reserved_0  : 48;
		mmr_t	value       : 16;
	} sh_pio_time_out_s;
} sh_pio_time_out_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_PIO_NACK_RESET"                     */
/*               Local Block PIO Reset for nack counters                */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pio_nack_reset_u {
	mmr_t	sh_pio_nack_reset_regval;
	struct {
		mmr_t	pulse       : 1;
		mmr_t	reserved_0  : 63;
	} sh_pio_nack_reset_s;
} sh_pio_nack_reset_u_t;
#else
typedef union sh_pio_nack_reset_u {
	mmr_t	sh_pio_nack_reset_regval;
	struct {
		mmr_t	reserved_0  : 63;
		mmr_t	pulse       : 1;
	} sh_pio_nack_reset_s;
} sh_pio_nack_reset_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_CONVEYOR_BELT_TIME_OUT"                 */
/*               Local Block conveyor belt time out value               */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_conveyor_belt_time_out_u {
	mmr_t	sh_conveyor_belt_time_out_regval;
	struct {
		mmr_t	value       : 12;
		mmr_t	reserved_0  : 52;
	} sh_conveyor_belt_time_out_s;
} sh_conveyor_belt_time_out_u_t;
#else
typedef union sh_conveyor_belt_time_out_u {
	mmr_t	sh_conveyor_belt_time_out_regval;
	struct {
		mmr_t	reserved_0  : 52;
		mmr_t	value       : 12;
	} sh_conveyor_belt_time_out_s;
} sh_conveyor_belt_time_out_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_LB_CREDIT_STATUS"                    */
/*                    Credit Counter Status Register                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_lb_credit_status_u {
	mmr_t	sh_lb_credit_status_regval;
	struct {
		mmr_t	liq_rq_credit : 5;
		mmr_t	reserved_0    : 1;
		mmr_t	liq_rp_credit : 4;
		mmr_t	reserved_1    : 2;
		mmr_t	linvv_credit  : 6;
		mmr_t	loq_rq_credit : 5;
		mmr_t	loq_rp_credit : 5;
		mmr_t	reserved_2    : 36;
	} sh_lb_credit_status_s;
} sh_lb_credit_status_u_t;
#else
typedef union sh_lb_credit_status_u {
	mmr_t	sh_lb_credit_status_regval;
	struct {
		mmr_t	reserved_2    : 36;
		mmr_t	loq_rp_credit : 5;
		mmr_t	loq_rq_credit : 5;
		mmr_t	linvv_credit  : 6;
		mmr_t	reserved_1    : 2;
		mmr_t	liq_rp_credit : 4;
		mmr_t	reserved_0    : 1;
		mmr_t	liq_rq_credit : 5;
	} sh_lb_credit_status_s;
} sh_lb_credit_status_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_LB_DEBUG_LOCAL_SEL"                   */
/*                         LB Debug Port Select                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_lb_debug_local_sel_u {
	mmr_t	sh_lb_debug_local_sel_regval;
	struct {
		mmr_t	nibble0_chiplet_sel : 3;
		mmr_t	reserved_0          : 1;
		mmr_t	nibble0_nibble_sel  : 3;
		mmr_t	reserved_1          : 1;
		mmr_t	nibble1_chiplet_sel : 3;
		mmr_t	reserved_2          : 1;
		mmr_t	nibble1_nibble_sel  : 3;
		mmr_t	reserved_3          : 1;
		mmr_t	nibble2_chiplet_sel : 3;
		mmr_t	reserved_4          : 1;
		mmr_t	nibble2_nibble_sel  : 3;
		mmr_t	reserved_5          : 1;
		mmr_t	nibble3_chiplet_sel : 3;
		mmr_t	reserved_6          : 1;
		mmr_t	nibble3_nibble_sel  : 3;
		mmr_t	reserved_7          : 1;
		mmr_t	nibble4_chiplet_sel : 3;
		mmr_t	reserved_8          : 1;
		mmr_t	nibble4_nibble_sel  : 3;
		mmr_t	reserved_9          : 1;
		mmr_t	nibble5_chiplet_sel : 3;
		mmr_t	reserved_10         : 1;
		mmr_t	nibble5_nibble_sel  : 3;
		mmr_t	reserved_11         : 1;
		mmr_t	nibble6_chiplet_sel : 3;
		mmr_t	reserved_12         : 1;
		mmr_t	nibble6_nibble_sel  : 3;
		mmr_t	reserved_13         : 1;
		mmr_t	nibble7_chiplet_sel : 3;
		mmr_t	reserved_14         : 1;
		mmr_t	nibble7_nibble_sel  : 3;
		mmr_t	trigger_enable      : 1;
	} sh_lb_debug_local_sel_s;
} sh_lb_debug_local_sel_u_t;
#else
typedef union sh_lb_debug_local_sel_u {
	mmr_t	sh_lb_debug_local_sel_regval;
	struct {
		mmr_t	trigger_enable      : 1;
		mmr_t	nibble7_nibble_sel  : 3;
		mmr_t	reserved_14         : 1;
		mmr_t	nibble7_chiplet_sel : 3;
		mmr_t	reserved_13         : 1;
		mmr_t	nibble6_nibble_sel  : 3;
		mmr_t	reserved_12         : 1;
		mmr_t	nibble6_chiplet_sel : 3;
		mmr_t	reserved_11         : 1;
		mmr_t	nibble5_nibble_sel  : 3;
		mmr_t	reserved_10         : 1;
		mmr_t	nibble5_chiplet_sel : 3;
		mmr_t	reserved_9          : 1;
		mmr_t	nibble4_nibble_sel  : 3;
		mmr_t	reserved_8          : 1;
		mmr_t	nibble4_chiplet_sel : 3;
		mmr_t	reserved_7          : 1;
		mmr_t	nibble3_nibble_sel  : 3;
		mmr_t	reserved_6          : 1;
		mmr_t	nibble3_chiplet_sel : 3;
		mmr_t	reserved_5          : 1;
		mmr_t	nibble2_nibble_sel  : 3;
		mmr_t	reserved_4          : 1;
		mmr_t	nibble2_chiplet_sel : 3;
		mmr_t	reserved_3          : 1;
		mmr_t	nibble1_nibble_sel  : 3;
		mmr_t	reserved_2          : 1;
		mmr_t	nibble1_chiplet_sel : 3;
		mmr_t	reserved_1          : 1;
		mmr_t	nibble0_nibble_sel  : 3;
		mmr_t	reserved_0          : 1;
		mmr_t	nibble0_chiplet_sel : 3;
	} sh_lb_debug_local_sel_s;
} sh_lb_debug_local_sel_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_LB_DEBUG_PERF_SEL"                    */
/*                   LB Debug Port Performance Select                   */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_lb_debug_perf_sel_u {
	mmr_t	sh_lb_debug_perf_sel_regval;
	struct {
		mmr_t	nibble0_chiplet_sel : 3;
		mmr_t	reserved_0          : 1;
		mmr_t	nibble0_nibble_sel  : 3;
		mmr_t	reserved_1          : 1;
		mmr_t	nibble1_chiplet_sel : 3;
		mmr_t	reserved_2          : 1;
		mmr_t	nibble1_nibble_sel  : 3;
		mmr_t	reserved_3          : 1;
		mmr_t	nibble2_chiplet_sel : 3;
		mmr_t	reserved_4          : 1;
		mmr_t	nibble2_nibble_sel  : 3;
		mmr_t	reserved_5          : 1;
		mmr_t	nibble3_chiplet_sel : 3;
		mmr_t	reserved_6          : 1;
		mmr_t	nibble3_nibble_sel  : 3;
		mmr_t	reserved_7          : 1;
		mmr_t	nibble4_chiplet_sel : 3;
		mmr_t	reserved_8          : 1;
		mmr_t	nibble4_nibble_sel  : 3;
		mmr_t	reserved_9          : 1;
		mmr_t	nibble5_chiplet_sel : 3;
		mmr_t	reserved_10         : 1;
		mmr_t	nibble5_nibble_sel  : 3;
		mmr_t	reserved_11         : 1;
		mmr_t	nibble6_chiplet_sel : 3;
		mmr_t	reserved_12         : 1;
		mmr_t	nibble6_nibble_sel  : 3;
		mmr_t	reserved_13         : 1;
		mmr_t	nibble7_chiplet_sel : 3;
		mmr_t	reserved_14         : 1;
		mmr_t	nibble7_nibble_sel  : 3;
		mmr_t	reserved_15         : 1;
	} sh_lb_debug_perf_sel_s;
} sh_lb_debug_perf_sel_u_t;
#else
typedef union sh_lb_debug_perf_sel_u {
	mmr_t	sh_lb_debug_perf_sel_regval;
	struct {
		mmr_t	reserved_15         : 1;
		mmr_t	nibble7_nibble_sel  : 3;
		mmr_t	reserved_14         : 1;
		mmr_t	nibble7_chiplet_sel : 3;
		mmr_t	reserved_13         : 1;
		mmr_t	nibble6_nibble_sel  : 3;
		mmr_t	reserved_12         : 1;
		mmr_t	nibble6_chiplet_sel : 3;
		mmr_t	reserved_11         : 1;
		mmr_t	nibble5_nibble_sel  : 3;
		mmr_t	reserved_10         : 1;
		mmr_t	nibble5_chiplet_sel : 3;
		mmr_t	reserved_9          : 1;
		mmr_t	nibble4_nibble_sel  : 3;
		mmr_t	reserved_8          : 1;
		mmr_t	nibble4_chiplet_sel : 3;
		mmr_t	reserved_7          : 1;
		mmr_t	nibble3_nibble_sel  : 3;
		mmr_t	reserved_6          : 1;
		mmr_t	nibble3_chiplet_sel : 3;
		mmr_t	reserved_5          : 1;
		mmr_t	nibble2_nibble_sel  : 3;
		mmr_t	reserved_4          : 1;
		mmr_t	nibble2_chiplet_sel : 3;
		mmr_t	reserved_3          : 1;
		mmr_t	nibble1_nibble_sel  : 3;
		mmr_t	reserved_2          : 1;
		mmr_t	nibble1_chiplet_sel : 3;
		mmr_t	reserved_1          : 1;
		mmr_t	nibble0_nibble_sel  : 3;
		mmr_t	reserved_0          : 1;
		mmr_t	nibble0_chiplet_sel : 3;
	} sh_lb_debug_perf_sel_s;
} sh_lb_debug_perf_sel_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_LB_DEBUG_TRIG_SEL"                    */
/*                       LB Debug Trigger Select                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_lb_debug_trig_sel_u {
	mmr_t	sh_lb_debug_trig_sel_regval;
	struct {
		mmr_t	trigger0_chiplet_sel : 3;
		mmr_t	reserved_0           : 1;
		mmr_t	trigger0_nibble_sel  : 3;
		mmr_t	reserved_1           : 1;
		mmr_t	trigger1_chiplet_sel : 3;
		mmr_t	reserved_2           : 1;
		mmr_t	trigger1_nibble_sel  : 3;
		mmr_t	reserved_3           : 1;
		mmr_t	trigger2_chiplet_sel : 3;
		mmr_t	reserved_4           : 1;
		mmr_t	trigger2_nibble_sel  : 3;
		mmr_t	reserved_5           : 1;
		mmr_t	trigger3_chiplet_sel : 3;
		mmr_t	reserved_6           : 1;
		mmr_t	trigger3_nibble_sel  : 3;
		mmr_t	reserved_7           : 1;
		mmr_t	trigger4_chiplet_sel : 3;
		mmr_t	reserved_8           : 1;
		mmr_t	trigger4_nibble_sel  : 3;
		mmr_t	reserved_9           : 1;
		mmr_t	trigger5_chiplet_sel : 3;
		mmr_t	reserved_10          : 1;
		mmr_t	trigger5_nibble_sel  : 3;
		mmr_t	reserved_11          : 1;
		mmr_t	trigger6_chiplet_sel : 3;
		mmr_t	reserved_12          : 1;
		mmr_t	trigger6_nibble_sel  : 3;
		mmr_t	reserved_13          : 1;
		mmr_t	trigger7_chiplet_sel : 3;
		mmr_t	reserved_14          : 1;
		mmr_t	trigger7_nibble_sel  : 3;
		mmr_t	reserved_15          : 1;
	} sh_lb_debug_trig_sel_s;
} sh_lb_debug_trig_sel_u_t;
#else
typedef union sh_lb_debug_trig_sel_u {
	mmr_t	sh_lb_debug_trig_sel_regval;
	struct {
		mmr_t	reserved_15          : 1;
		mmr_t	trigger7_nibble_sel  : 3;
		mmr_t	reserved_14          : 1;
		mmr_t	trigger7_chiplet_sel : 3;
		mmr_t	reserved_13          : 1;
		mmr_t	trigger6_nibble_sel  : 3;
		mmr_t	reserved_12          : 1;
		mmr_t	trigger6_chiplet_sel : 3;
		mmr_t	reserved_11          : 1;
		mmr_t	trigger5_nibble_sel  : 3;
		mmr_t	reserved_10          : 1;
		mmr_t	trigger5_chiplet_sel : 3;
		mmr_t	reserved_9           : 1;
		mmr_t	trigger4_nibble_sel  : 3;
		mmr_t	reserved_8           : 1;
		mmr_t	trigger4_chiplet_sel : 3;
		mmr_t	reserved_7           : 1;
		mmr_t	trigger3_nibble_sel  : 3;
		mmr_t	reserved_6           : 1;
		mmr_t	trigger3_chiplet_sel : 3;
		mmr_t	reserved_5           : 1;
		mmr_t	trigger2_nibble_sel  : 3;
		mmr_t	reserved_4           : 1;
		mmr_t	trigger2_chiplet_sel : 3;
		mmr_t	reserved_3           : 1;
		mmr_t	trigger1_nibble_sel  : 3;
		mmr_t	reserved_2           : 1;
		mmr_t	trigger1_chiplet_sel : 3;
		mmr_t	reserved_1           : 1;
		mmr_t	trigger0_nibble_sel  : 3;
		mmr_t	reserved_0           : 1;
		mmr_t	trigger0_chiplet_sel : 3;
	} sh_lb_debug_trig_sel_s;
} sh_lb_debug_trig_sel_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_LB_ERROR_DETAIL_1"                    */
/*                  LB Error capture information: HDR1                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_lb_error_detail_1_u {
	mmr_t	sh_lb_error_detail_1_regval;
	struct {
		mmr_t	command     : 8;
		mmr_t	suppl       : 14;
		mmr_t	reserved_0  : 2;
		mmr_t	source      : 14;
		mmr_t	reserved_1  : 2;
		mmr_t	dest        : 3;
		mmr_t	reserved_2  : 5;
		mmr_t	hdr_err     : 1;
		mmr_t	data_err    : 1;
		mmr_t	reserved_3  : 13;
		mmr_t	valid       : 1;
	} sh_lb_error_detail_1_s;
} sh_lb_error_detail_1_u_t;
#else
typedef union sh_lb_error_detail_1_u {
	mmr_t	sh_lb_error_detail_1_regval;
	struct {
		mmr_t	valid       : 1;
		mmr_t	reserved_3  : 13;
		mmr_t	data_err    : 1;
		mmr_t	hdr_err     : 1;
		mmr_t	reserved_2  : 5;
		mmr_t	dest        : 3;
		mmr_t	reserved_1  : 2;
		mmr_t	source      : 14;
		mmr_t	reserved_0  : 2;
		mmr_t	suppl       : 14;
		mmr_t	command     : 8;
	} sh_lb_error_detail_1_s;
} sh_lb_error_detail_1_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_LB_ERROR_DETAIL_2"                    */
/*                            LB Error Bits                             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_lb_error_detail_2_u {
	mmr_t	sh_lb_error_detail_2_regval;
	struct {
		mmr_t	address     : 47;
		mmr_t	reserved_0  : 17;
	} sh_lb_error_detail_2_s;
} sh_lb_error_detail_2_u_t;
#else
typedef union sh_lb_error_detail_2_u {
	mmr_t	sh_lb_error_detail_2_regval;
	struct {
		mmr_t	reserved_0  : 17;
		mmr_t	address     : 47;
	} sh_lb_error_detail_2_s;
} sh_lb_error_detail_2_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_LB_ERROR_DETAIL_3"                    */
/*                            LB Error Bits                             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_lb_error_detail_3_u {
	mmr_t	sh_lb_error_detail_3_regval;
	struct {
		mmr_t	data        : 64;
	} sh_lb_error_detail_3_s;
} sh_lb_error_detail_3_u_t;
#else
typedef union sh_lb_error_detail_3_u {
	mmr_t	sh_lb_error_detail_3_regval;
	struct {
		mmr_t	data        : 64;
	} sh_lb_error_detail_3_s;
} sh_lb_error_detail_3_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_LB_ERROR_DETAIL_4"                    */
/*                            LB Error Bits                             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_lb_error_detail_4_u {
	mmr_t	sh_lb_error_detail_4_regval;
	struct {
		mmr_t	route       : 64;
	} sh_lb_error_detail_4_s;
} sh_lb_error_detail_4_u_t;
#else
typedef union sh_lb_error_detail_4_u {
	mmr_t	sh_lb_error_detail_4_regval;
	struct {
		mmr_t	route       : 64;
	} sh_lb_error_detail_4_s;
} sh_lb_error_detail_4_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_LB_ERROR_DETAIL_5"                    */
/*                            LB Error Bits                             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_lb_error_detail_5_u {
	mmr_t	sh_lb_error_detail_5_regval;
	struct {
		mmr_t	read_retry       : 1;
		mmr_t	ptc1_write       : 1;
		mmr_t	write_retry      : 1;
		mmr_t	count_a_overflow : 1;
		mmr_t	count_b_overflow : 1;
		mmr_t	nack_a_timeout   : 1;
		mmr_t	nack_b_timeout   : 1;
		mmr_t	reserved_0       : 57;
	} sh_lb_error_detail_5_s;
} sh_lb_error_detail_5_u_t;
#else
typedef union sh_lb_error_detail_5_u {
	mmr_t	sh_lb_error_detail_5_regval;
	struct {
		mmr_t	reserved_0       : 57;
		mmr_t	nack_b_timeout   : 1;
		mmr_t	nack_a_timeout   : 1;
		mmr_t	count_b_overflow : 1;
		mmr_t	count_a_overflow : 1;
		mmr_t	write_retry      : 1;
		mmr_t	ptc1_write       : 1;
		mmr_t	read_retry       : 1;
	} sh_lb_error_detail_5_s;
} sh_lb_error_detail_5_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_LB_ERROR_MASK"                      */
/*                            LB Error Mask                             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_lb_error_mask_u {
	mmr_t	sh_lb_error_mask_regval;
	struct {
		mmr_t	rq_bad_cmd            : 1;
		mmr_t	rp_bad_cmd            : 1;
		mmr_t	rq_short              : 1;
		mmr_t	rp_short              : 1;
		mmr_t	rq_long               : 1;
		mmr_t	rp_long               : 1;
		mmr_t	rq_bad_data           : 1;
		mmr_t	rp_bad_data           : 1;
		mmr_t	rq_bad_addr           : 1;
		mmr_t	rq_time_out           : 1;
		mmr_t	linvv_overflow        : 1;
		mmr_t	unexpected_linv       : 1;
		mmr_t	ptc_1_timeout         : 1;
		mmr_t	junk_bus_err          : 1;
		mmr_t	pio_cb_err            : 1;
		mmr_t	vector_rq_route_error : 1;
		mmr_t	vector_rp_route_error : 1;
		mmr_t	gclk_drop             : 1;
		mmr_t	rq_fifo_error         : 1;
		mmr_t	rp_fifo_error         : 1;
		mmr_t	unexp_valid           : 1;
		mmr_t	rq_credit_overflow    : 1;
		mmr_t	rp_credit_overflow    : 1;
		mmr_t	reserved_0            : 41;
	} sh_lb_error_mask_s;
} sh_lb_error_mask_u_t;
#else
typedef union sh_lb_error_mask_u {
	mmr_t	sh_lb_error_mask_regval;
	struct {
		mmr_t	reserved_0            : 41;
		mmr_t	rp_credit_overflow    : 1;
		mmr_t	rq_credit_overflow    : 1;
		mmr_t	unexp_valid           : 1;
		mmr_t	rp_fifo_error         : 1;
		mmr_t	rq_fifo_error         : 1;
		mmr_t	gclk_drop             : 1;
		mmr_t	vector_rp_route_error : 1;
		mmr_t	vector_rq_route_error : 1;
		mmr_t	pio_cb_err            : 1;
		mmr_t	junk_bus_err          : 1;
		mmr_t	ptc_1_timeout         : 1;
		mmr_t	unexpected_linv       : 1;
		mmr_t	linvv_overflow        : 1;
		mmr_t	rq_time_out           : 1;
		mmr_t	rq_bad_addr           : 1;
		mmr_t	rp_bad_data           : 1;
		mmr_t	rq_bad_data           : 1;
		mmr_t	rp_long               : 1;
		mmr_t	rq_long               : 1;
		mmr_t	rp_short              : 1;
		mmr_t	rq_short              : 1;
		mmr_t	rp_bad_cmd            : 1;
		mmr_t	rq_bad_cmd            : 1;
	} sh_lb_error_mask_s;
} sh_lb_error_mask_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_LB_ERROR_OVERFLOW"                    */
/*                          LB Error Overflow                           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_lb_error_overflow_u {
	mmr_t	sh_lb_error_overflow_regval;
	struct {
		mmr_t	rq_bad_cmd_ovrfl            : 1;
		mmr_t	rp_bad_cmd_ovrfl            : 1;
		mmr_t	rq_short_ovrfl              : 1;
		mmr_t	rp_short_ovrfl              : 1;
		mmr_t	rq_long_ovrfl               : 1;
		mmr_t	rp_long_ovrfl               : 1;
		mmr_t	rq_bad_data_ovrfl           : 1;
		mmr_t	rp_bad_data_ovrfl           : 1;
		mmr_t	rq_bad_addr_ovrfl           : 1;
		mmr_t	rq_time_out_ovrfl           : 1;
		mmr_t	linvv_overflow_ovrfl        : 1;
		mmr_t	unexpected_linv_ovrfl       : 1;
		mmr_t	ptc_1_timeout_ovrfl         : 1;
		mmr_t	junk_bus_err_ovrfl          : 1;
		mmr_t	pio_cb_err_ovrfl            : 1;
		mmr_t	vector_rq_route_error_ovrfl : 1;
		mmr_t	vector_rp_route_error_ovrfl : 1;
		mmr_t	gclk_drop_ovrfl             : 1;
		mmr_t	rq_fifo_error_ovrfl         : 1;
		mmr_t	rp_fifo_error_ovrfl         : 1;
		mmr_t	unexp_valid_ovrfl           : 1;
		mmr_t	rq_credit_overflow_ovrfl    : 1;
		mmr_t	rp_credit_overflow_ovrfl    : 1;
		mmr_t	reserved_0                  : 41;
	} sh_lb_error_overflow_s;
} sh_lb_error_overflow_u_t;
#else
typedef union sh_lb_error_overflow_u {
	mmr_t	sh_lb_error_overflow_regval;
	struct {
		mmr_t	reserved_0                  : 41;
		mmr_t	rp_credit_overflow_ovrfl    : 1;
		mmr_t	rq_credit_overflow_ovrfl    : 1;
		mmr_t	unexp_valid_ovrfl           : 1;
		mmr_t	rp_fifo_error_ovrfl         : 1;
		mmr_t	rq_fifo_error_ovrfl         : 1;
		mmr_t	gclk_drop_ovrfl             : 1;
		mmr_t	vector_rp_route_error_ovrfl : 1;
		mmr_t	vector_rq_route_error_ovrfl : 1;
		mmr_t	pio_cb_err_ovrfl            : 1;
		mmr_t	junk_bus_err_ovrfl          : 1;
		mmr_t	ptc_1_timeout_ovrfl         : 1;
		mmr_t	unexpected_linv_ovrfl       : 1;
		mmr_t	linvv_overflow_ovrfl        : 1;
		mmr_t	rq_time_out_ovrfl           : 1;
		mmr_t	rq_bad_addr_ovrfl           : 1;
		mmr_t	rp_bad_data_ovrfl           : 1;
		mmr_t	rq_bad_data_ovrfl           : 1;
		mmr_t	rp_long_ovrfl               : 1;
		mmr_t	rq_long_ovrfl               : 1;
		mmr_t	rp_short_ovrfl              : 1;
		mmr_t	rq_short_ovrfl              : 1;
		mmr_t	rp_bad_cmd_ovrfl            : 1;
		mmr_t	rq_bad_cmd_ovrfl            : 1;
	} sh_lb_error_overflow_s;
} sh_lb_error_overflow_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_LB_ERROR_SUMMARY"                    */
/*                            LB Error Bits                             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_lb_error_summary_u {
	mmr_t	sh_lb_error_summary_regval;
	struct {
		mmr_t	rq_bad_cmd            : 1;
		mmr_t	rp_bad_cmd            : 1;
		mmr_t	rq_short              : 1;
		mmr_t	rp_short              : 1;
		mmr_t	rq_long               : 1;
		mmr_t	rp_long               : 1;
		mmr_t	rq_bad_data           : 1;
		mmr_t	rp_bad_data           : 1;
		mmr_t	rq_bad_addr           : 1;
		mmr_t	rq_time_out           : 1;
		mmr_t	linvv_overflow        : 1;
		mmr_t	unexpected_linv       : 1;
		mmr_t	ptc_1_timeout         : 1;
		mmr_t	junk_bus_err          : 1;
		mmr_t	pio_cb_err            : 1;
		mmr_t	vector_rq_route_error : 1;
		mmr_t	vector_rp_route_error : 1;
		mmr_t	gclk_drop             : 1;
		mmr_t	rq_fifo_error         : 1;
		mmr_t	rp_fifo_error         : 1;
		mmr_t	unexp_valid           : 1;
		mmr_t	rq_credit_overflow    : 1;
		mmr_t	rp_credit_overflow    : 1;
		mmr_t	reserved_0            : 41;
	} sh_lb_error_summary_s;
} sh_lb_error_summary_u_t;
#else
typedef union sh_lb_error_summary_u {
	mmr_t	sh_lb_error_summary_regval;
	struct {
		mmr_t	reserved_0            : 41;
		mmr_t	rp_credit_overflow    : 1;
		mmr_t	rq_credit_overflow    : 1;
		mmr_t	unexp_valid           : 1;
		mmr_t	rp_fifo_error         : 1;
		mmr_t	rq_fifo_error         : 1;
		mmr_t	gclk_drop             : 1;
		mmr_t	vector_rp_route_error : 1;
		mmr_t	vector_rq_route_error : 1;
		mmr_t	pio_cb_err            : 1;
		mmr_t	junk_bus_err          : 1;
		mmr_t	ptc_1_timeout         : 1;
		mmr_t	unexpected_linv       : 1;
		mmr_t	linvv_overflow        : 1;
		mmr_t	rq_time_out           : 1;
		mmr_t	rq_bad_addr           : 1;
		mmr_t	rp_bad_data           : 1;
		mmr_t	rq_bad_data           : 1;
		mmr_t	rp_long               : 1;
		mmr_t	rq_long               : 1;
		mmr_t	rp_short              : 1;
		mmr_t	rq_short              : 1;
		mmr_t	rp_bad_cmd            : 1;
		mmr_t	rq_bad_cmd            : 1;
	} sh_lb_error_summary_s;
} sh_lb_error_summary_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_LB_FIRST_ERROR"                     */
/*                            LB First Error                            */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_lb_first_error_u {
	mmr_t	sh_lb_first_error_regval;
	struct {
		mmr_t	rq_bad_cmd            : 1;
		mmr_t	rp_bad_cmd            : 1;
		mmr_t	rq_short              : 1;
		mmr_t	rp_short              : 1;
		mmr_t	rq_long               : 1;
		mmr_t	rp_long               : 1;
		mmr_t	rq_bad_data           : 1;
		mmr_t	rp_bad_data           : 1;
		mmr_t	rq_bad_addr           : 1;
		mmr_t	rq_time_out           : 1;
		mmr_t	linvv_overflow        : 1;
		mmr_t	unexpected_linv       : 1;
		mmr_t	ptc_1_timeout         : 1;
		mmr_t	junk_bus_err          : 1;
		mmr_t	pio_cb_err            : 1;
		mmr_t	vector_rq_route_error : 1;
		mmr_t	vector_rp_route_error : 1;
		mmr_t	gclk_drop             : 1;
		mmr_t	rq_fifo_error         : 1;
		mmr_t	rp_fifo_error         : 1;
		mmr_t	unexp_valid           : 1;
		mmr_t	rq_credit_overflow    : 1;
		mmr_t	rp_credit_overflow    : 1;
		mmr_t	reserved_0            : 41;
	} sh_lb_first_error_s;
} sh_lb_first_error_u_t;
#else
typedef union sh_lb_first_error_u {
	mmr_t	sh_lb_first_error_regval;
	struct {
		mmr_t	reserved_0            : 41;
		mmr_t	rp_credit_overflow    : 1;
		mmr_t	rq_credit_overflow    : 1;
		mmr_t	unexp_valid           : 1;
		mmr_t	rp_fifo_error         : 1;
		mmr_t	rq_fifo_error         : 1;
		mmr_t	gclk_drop             : 1;
		mmr_t	vector_rp_route_error : 1;
		mmr_t	vector_rq_route_error : 1;
		mmr_t	pio_cb_err            : 1;
		mmr_t	junk_bus_err          : 1;
		mmr_t	ptc_1_timeout         : 1;
		mmr_t	unexpected_linv       : 1;
		mmr_t	linvv_overflow        : 1;
		mmr_t	rq_time_out           : 1;
		mmr_t	rq_bad_addr           : 1;
		mmr_t	rp_bad_data           : 1;
		mmr_t	rq_bad_data           : 1;
		mmr_t	rp_long               : 1;
		mmr_t	rq_long               : 1;
		mmr_t	rp_short              : 1;
		mmr_t	rq_short              : 1;
		mmr_t	rp_bad_cmd            : 1;
		mmr_t	rq_bad_cmd            : 1;
	} sh_lb_first_error_s;
} sh_lb_first_error_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_LB_LAST_CREDIT"                     */
/*                    Credit counter status register                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_lb_last_credit_u {
	mmr_t	sh_lb_last_credit_regval;
	struct {
		mmr_t	liq_rq_credit : 5;
		mmr_t	reserved_0    : 1;
		mmr_t	liq_rp_credit : 4;
		mmr_t	reserved_1    : 2;
		mmr_t	linvv_credit  : 6;
		mmr_t	loq_rq_credit : 5;
		mmr_t	loq_rp_credit : 5;
		mmr_t	reserved_2    : 36;
	} sh_lb_last_credit_s;
} sh_lb_last_credit_u_t;
#else
typedef union sh_lb_last_credit_u {
	mmr_t	sh_lb_last_credit_regval;
	struct {
		mmr_t	reserved_2    : 36;
		mmr_t	loq_rp_credit : 5;
		mmr_t	loq_rq_credit : 5;
		mmr_t	linvv_credit  : 6;
		mmr_t	reserved_1    : 2;
		mmr_t	liq_rp_credit : 4;
		mmr_t	reserved_0    : 1;
		mmr_t	liq_rq_credit : 5;
	} sh_lb_last_credit_s;
} sh_lb_last_credit_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_LB_NACK_STATUS"                     */
/*                     Nack Counter Status Register                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_lb_nack_status_u {
	mmr_t	sh_lb_nack_status_regval;
	struct {
		mmr_t	pio_nack_a       : 12;
		mmr_t	reserved_0       : 4;
		mmr_t	pio_nack_b       : 12;
		mmr_t	reserved_1       : 4;
		mmr_t	junk_nack        : 16;
		mmr_t	cb_timeout_count : 12;
		mmr_t	cb_state         : 2;
		mmr_t	reserved_2       : 2;
	} sh_lb_nack_status_s;
} sh_lb_nack_status_u_t;
#else
typedef union sh_lb_nack_status_u {
	mmr_t	sh_lb_nack_status_regval;
	struct {
		mmr_t	reserved_2       : 2;
		mmr_t	cb_state         : 2;
		mmr_t	cb_timeout_count : 12;
		mmr_t	junk_nack        : 16;
		mmr_t	reserved_1       : 4;
		mmr_t	pio_nack_b       : 12;
		mmr_t	reserved_0       : 4;
		mmr_t	pio_nack_a       : 12;
	} sh_lb_nack_status_s;
} sh_lb_nack_status_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_LB_TRIGGER_COMPARE"                   */
/*                    LB Test-point Trigger Compare                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_lb_trigger_compare_u {
	mmr_t	sh_lb_trigger_compare_regval;
	struct {
		mmr_t	mask        : 32;
		mmr_t	reserved_0  : 32;
	} sh_lb_trigger_compare_s;
} sh_lb_trigger_compare_u_t;
#else
typedef union sh_lb_trigger_compare_u {
	mmr_t	sh_lb_trigger_compare_regval;
	struct {
		mmr_t	reserved_0  : 32;
		mmr_t	mask        : 32;
	} sh_lb_trigger_compare_s;
} sh_lb_trigger_compare_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_LB_TRIGGER_DATA"                     */
/*                  LB Test-point Trigger Compare Data                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_lb_trigger_data_u {
	mmr_t	sh_lb_trigger_data_regval;
	struct {
		mmr_t	compare_pattern : 32;
		mmr_t	reserved_0      : 32;
	} sh_lb_trigger_data_s;
} sh_lb_trigger_data_u_t;
#else
typedef union sh_lb_trigger_data_u {
	mmr_t	sh_lb_trigger_data_regval;
	struct {
		mmr_t	reserved_0      : 32;
		mmr_t	compare_pattern : 32;
	} sh_lb_trigger_data_s;
} sh_lb_trigger_data_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_PI_AEC_CONFIG"                      */
/*              PI Adaptive Error Correction Configuration              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_aec_config_u {
	mmr_t	sh_pi_aec_config_regval;
	struct {
		mmr_t	mode        : 3;
		mmr_t	reserved_0  : 61;
	} sh_pi_aec_config_s;
} sh_pi_aec_config_u_t;
#else
typedef union sh_pi_aec_config_u {
	mmr_t	sh_pi_aec_config_regval;
	struct {
		mmr_t	reserved_0  : 61;
		mmr_t	mode        : 3;
	} sh_pi_aec_config_s;
} sh_pi_aec_config_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_PI_AFI_ERROR_MASK"                    */
/*                          PI AFI Error Mask                           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_afi_error_mask_u {
	mmr_t	sh_pi_afi_error_mask_regval;
	struct {
		mmr_t	reserved_0   : 21;
		mmr_t	hung_bus     : 1;
		mmr_t	rsp_parity   : 1;
		mmr_t	ioq_overrun  : 1;
		mmr_t	req_format   : 1;
		mmr_t	addr_access  : 1;
		mmr_t	req_parity   : 1;
		mmr_t	addr_parity  : 1;
		mmr_t	shub_fsb_dqe : 1;
		mmr_t	shub_fsb_uce : 1;
		mmr_t	shub_fsb_ce  : 1;
		mmr_t	livelock     : 1;
		mmr_t	bad_snoop    : 1;
		mmr_t	fsb_tbl_miss : 1;
		mmr_t	msg_len      : 1;
		mmr_t	reserved_1   : 29;
	} sh_pi_afi_error_mask_s;
} sh_pi_afi_error_mask_u_t;
#else
typedef union sh_pi_afi_error_mask_u {
	mmr_t	sh_pi_afi_error_mask_regval;
	struct {
		mmr_t	reserved_1   : 29;
		mmr_t	msg_len      : 1;
		mmr_t	fsb_tbl_miss : 1;
		mmr_t	bad_snoop    : 1;
		mmr_t	livelock     : 1;
		mmr_t	shub_fsb_ce  : 1;
		mmr_t	shub_fsb_uce : 1;
		mmr_t	shub_fsb_dqe : 1;
		mmr_t	addr_parity  : 1;
		mmr_t	req_parity   : 1;
		mmr_t	addr_access  : 1;
		mmr_t	req_format   : 1;
		mmr_t	ioq_overrun  : 1;
		mmr_t	rsp_parity   : 1;
		mmr_t	hung_bus     : 1;
		mmr_t	reserved_0   : 21;
	} sh_pi_afi_error_mask_s;
} sh_pi_afi_error_mask_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_PI_AFI_TEST_POINT_COMPARE"                */
/*                      PI AFI Test Point Compare                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_afi_test_point_compare_u {
	mmr_t	sh_pi_afi_test_point_compare_regval;
	struct {
		mmr_t	compare_mask    : 32;
		mmr_t	compare_pattern : 32;
	} sh_pi_afi_test_point_compare_s;
} sh_pi_afi_test_point_compare_u_t;
#else
typedef union sh_pi_afi_test_point_compare_u {
	mmr_t	sh_pi_afi_test_point_compare_regval;
	struct {
		mmr_t	compare_pattern : 32;
		mmr_t	compare_mask    : 32;
	} sh_pi_afi_test_point_compare_s;
} sh_pi_afi_test_point_compare_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_PI_AFI_TEST_POINT_SELECT"                */
/*                       PI AFI Test Point Select                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_afi_test_point_select_u {
	mmr_t	sh_pi_afi_test_point_select_regval;
	struct {
		mmr_t	nibble0_chiplet_sel : 4;
		mmr_t	nibble0_nibble_sel  : 3;
		mmr_t	reserved_0          : 1;
		mmr_t	nibble1_chiplet_sel : 4;
		mmr_t	nibble1_nibble_sel  : 3;
		mmr_t	reserved_1          : 1;
		mmr_t	nibble2_chiplet_sel : 4;
		mmr_t	nibble2_nibble_sel  : 3;
		mmr_t	reserved_2          : 1;
		mmr_t	nibble3_chiplet_sel : 4;
		mmr_t	nibble3_nibble_sel  : 3;
		mmr_t	reserved_3          : 1;
		mmr_t	nibble4_chiplet_sel : 4;
		mmr_t	nibble4_nibble_sel  : 3;
		mmr_t	reserved_4          : 1;
		mmr_t	nibble5_chiplet_sel : 4;
		mmr_t	nibble5_nibble_sel  : 3;
		mmr_t	reserved_5          : 1;
		mmr_t	nibble6_chiplet_sel : 4;
		mmr_t	nibble6_nibble_sel  : 3;
		mmr_t	reserved_6          : 1;
		mmr_t	nibble7_chiplet_sel : 4;
		mmr_t	nibble7_nibble_sel  : 3;
		mmr_t	trigger_enable      : 1;
	} sh_pi_afi_test_point_select_s;
} sh_pi_afi_test_point_select_u_t;
#else
typedef union sh_pi_afi_test_point_select_u {
	mmr_t	sh_pi_afi_test_point_select_regval;
	struct {
		mmr_t	trigger_enable      : 1;
		mmr_t	nibble7_nibble_sel  : 3;
		mmr_t	nibble7_chiplet_sel : 4;
		mmr_t	reserved_6          : 1;
		mmr_t	nibble6_nibble_sel  : 3;
		mmr_t	nibble6_chiplet_sel : 4;
		mmr_t	reserved_5          : 1;
		mmr_t	nibble5_nibble_sel  : 3;
		mmr_t	nibble5_chiplet_sel : 4;
		mmr_t	reserved_4          : 1;
		mmr_t	nibble4_nibble_sel  : 3;
		mmr_t	nibble4_chiplet_sel : 4;
		mmr_t	reserved_3          : 1;
		mmr_t	nibble3_nibble_sel  : 3;
		mmr_t	nibble3_chiplet_sel : 4;
		mmr_t	reserved_2          : 1;
		mmr_t	nibble2_nibble_sel  : 3;
		mmr_t	nibble2_chiplet_sel : 4;
		mmr_t	reserved_1          : 1;
		mmr_t	nibble1_nibble_sel  : 3;
		mmr_t	nibble1_chiplet_sel : 4;
		mmr_t	reserved_0          : 1;
		mmr_t	nibble0_nibble_sel  : 3;
		mmr_t	nibble0_chiplet_sel : 4;
	} sh_pi_afi_test_point_select_s;
} sh_pi_afi_test_point_select_u_t;
#endif

/* ==================================================================== */
/*            Register "SH_PI_AFI_TEST_POINT_TRIGGER_SELECT"            */
/*                  PI CRBC Test Point Trigger Select                   */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_afi_test_point_trigger_select_u {
	mmr_t	sh_pi_afi_test_point_trigger_select_regval;
	struct {
		mmr_t	trigger0_chiplet_sel : 4;
		mmr_t	trigger0_nibble_sel  : 3;
		mmr_t	reserved_0           : 1;
		mmr_t	trigger1_chiplet_sel : 4;
		mmr_t	trigger1_nibble_sel  : 3;
		mmr_t	reserved_1           : 1;
		mmr_t	trigger2_chiplet_sel : 4;
		mmr_t	trigger2_nibble_sel  : 3;
		mmr_t	reserved_2           : 1;
		mmr_t	trigger3_chiplet_sel : 4;
		mmr_t	trigger3_nibble_sel  : 3;
		mmr_t	reserved_3           : 1;
		mmr_t	trigger4_chiplet_sel : 4;
		mmr_t	trigger4_nibble_sel  : 3;
		mmr_t	reserved_4           : 1;
		mmr_t	trigger5_chiplet_sel : 4;
		mmr_t	trigger5_nibble_sel  : 3;
		mmr_t	reserved_5           : 1;
		mmr_t	trigger6_chiplet_sel : 4;
		mmr_t	trigger6_nibble_sel  : 3;
		mmr_t	reserved_6           : 1;
		mmr_t	trigger7_chiplet_sel : 4;
		mmr_t	trigger7_nibble_sel  : 3;
		mmr_t	reserved_7           : 1;
	} sh_pi_afi_test_point_trigger_select_s;
} sh_pi_afi_test_point_trigger_select_u_t;
#else
typedef union sh_pi_afi_test_point_trigger_select_u {
	mmr_t	sh_pi_afi_test_point_trigger_select_regval;
	struct {
		mmr_t	reserved_7           : 1;
		mmr_t	trigger7_nibble_sel  : 3;
		mmr_t	trigger7_chiplet_sel : 4;
		mmr_t	reserved_6           : 1;
		mmr_t	trigger6_nibble_sel  : 3;
		mmr_t	trigger6_chiplet_sel : 4;
		mmr_t	reserved_5           : 1;
		mmr_t	trigger5_nibble_sel  : 3;
		mmr_t	trigger5_chiplet_sel : 4;
		mmr_t	reserved_4           : 1;
		mmr_t	trigger4_nibble_sel  : 3;
		mmr_t	trigger4_chiplet_sel : 4;
		mmr_t	reserved_3           : 1;
		mmr_t	trigger3_nibble_sel  : 3;
		mmr_t	trigger3_chiplet_sel : 4;
		mmr_t	reserved_2           : 1;
		mmr_t	trigger2_nibble_sel  : 3;
		mmr_t	trigger2_chiplet_sel : 4;
		mmr_t	reserved_1           : 1;
		mmr_t	trigger1_nibble_sel  : 3;
		mmr_t	trigger1_chiplet_sel : 4;
		mmr_t	reserved_0           : 1;
		mmr_t	trigger0_nibble_sel  : 3;
		mmr_t	trigger0_chiplet_sel : 4;
	} sh_pi_afi_test_point_trigger_select_s;
} sh_pi_afi_test_point_trigger_select_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PI_AUTO_REPLY_ENABLE"                  */
/*                         PI Auto Reply Enable                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_auto_reply_enable_u {
	mmr_t	sh_pi_auto_reply_enable_regval;
	struct {
		mmr_t	auto_reply_enable : 1;
		mmr_t	reserved_0        : 63;
	} sh_pi_auto_reply_enable_s;
} sh_pi_auto_reply_enable_u_t;
#else
typedef union sh_pi_auto_reply_enable_u {
	mmr_t	sh_pi_auto_reply_enable_regval;
	struct {
		mmr_t	reserved_0        : 63;
		mmr_t	auto_reply_enable : 1;
	} sh_pi_auto_reply_enable_s;
} sh_pi_auto_reply_enable_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_PI_CAM_CONTROL"                     */
/*                      CRB CAM MMR Access Control                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_cam_control_u {
	mmr_t	sh_pi_cam_control_regval;
	struct {
		mmr_t	cam_indx          : 7;
		mmr_t	reserved_0        : 1;
		mmr_t	cam_write         : 1;
		mmr_t	rrb_rd_xfer_clear : 1;
		mmr_t	reserved_1        : 53;
		mmr_t	start             : 1;
	} sh_pi_cam_control_s;
} sh_pi_cam_control_u_t;
#else
typedef union sh_pi_cam_control_u {
	mmr_t	sh_pi_cam_control_regval;
	struct {
		mmr_t	start             : 1;
		mmr_t	reserved_1        : 53;
		mmr_t	rrb_rd_xfer_clear : 1;
		mmr_t	cam_write         : 1;
		mmr_t	reserved_0        : 1;
		mmr_t	cam_indx          : 7;
	} sh_pi_cam_control_s;
} sh_pi_cam_control_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_PI_CRBC_TEST_POINT_COMPARE"               */
/*                      PI CRBC Test Point Compare                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_crbc_test_point_compare_u {
	mmr_t	sh_pi_crbc_test_point_compare_regval;
	struct {
		mmr_t	compare_mask    : 32;
		mmr_t	compare_pattern : 32;
	} sh_pi_crbc_test_point_compare_s;
} sh_pi_crbc_test_point_compare_u_t;
#else
typedef union sh_pi_crbc_test_point_compare_u {
	mmr_t	sh_pi_crbc_test_point_compare_regval;
	struct {
		mmr_t	compare_pattern : 32;
		mmr_t	compare_mask    : 32;
	} sh_pi_crbc_test_point_compare_s;
} sh_pi_crbc_test_point_compare_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_PI_CRBC_TEST_POINT_SELECT"                */
/*                      PI CRBC Test Point Select                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_crbc_test_point_select_u {
	mmr_t	sh_pi_crbc_test_point_select_regval;
	struct {
		mmr_t	nibble0_chiplet_sel : 3;
		mmr_t	reserved_0          : 1;
		mmr_t	nibble0_nibble_sel  : 3;
		mmr_t	reserved_1          : 1;
		mmr_t	nibble1_chiplet_sel : 3;
		mmr_t	reserved_2          : 1;
		mmr_t	nibble1_nibble_sel  : 3;
		mmr_t	reserved_3          : 1;
		mmr_t	nibble2_chiplet_sel : 3;
		mmr_t	reserved_4          : 1;
		mmr_t	nibble2_nibble_sel  : 3;
		mmr_t	reserved_5          : 1;
		mmr_t	nibble3_chiplet_sel : 3;
		mmr_t	reserved_6          : 1;
		mmr_t	nibble3_nibble_sel  : 3;
		mmr_t	reserved_7          : 1;
		mmr_t	nibble4_chiplet_sel : 3;
		mmr_t	reserved_8          : 1;
		mmr_t	nibble4_nibble_sel  : 3;
		mmr_t	reserved_9          : 1;
		mmr_t	nibble5_chiplet_sel : 3;
		mmr_t	reserved_10         : 1;
		mmr_t	nibble5_nibble_sel  : 3;
		mmr_t	reserved_11         : 1;
		mmr_t	nibble6_chiplet_sel : 3;
		mmr_t	reserved_12         : 1;
		mmr_t	nibble6_nibble_sel  : 3;
		mmr_t	reserved_13         : 1;
		mmr_t	nibble7_chiplet_sel : 3;
		mmr_t	reserved_14         : 1;
		mmr_t	nibble7_nibble_sel  : 3;
		mmr_t	trigger_enable      : 1;
	} sh_pi_crbc_test_point_select_s;
} sh_pi_crbc_test_point_select_u_t;
#else
typedef union sh_pi_crbc_test_point_select_u {
	mmr_t	sh_pi_crbc_test_point_select_regval;
	struct {
		mmr_t	trigger_enable      : 1;
		mmr_t	nibble7_nibble_sel  : 3;
		mmr_t	reserved_14         : 1;
		mmr_t	nibble7_chiplet_sel : 3;
		mmr_t	reserved_13         : 1;
		mmr_t	nibble6_nibble_sel  : 3;
		mmr_t	reserved_12         : 1;
		mmr_t	nibble6_chiplet_sel : 3;
		mmr_t	reserved_11         : 1;
		mmr_t	nibble5_nibble_sel  : 3;
		mmr_t	reserved_10         : 1;
		mmr_t	nibble5_chiplet_sel : 3;
		mmr_t	reserved_9          : 1;
		mmr_t	nibble4_nibble_sel  : 3;
		mmr_t	reserved_8          : 1;
		mmr_t	nibble4_chiplet_sel : 3;
		mmr_t	reserved_7          : 1;
		mmr_t	nibble3_nibble_sel  : 3;
		mmr_t	reserved_6          : 1;
		mmr_t	nibble3_chiplet_sel : 3;
		mmr_t	reserved_5          : 1;
		mmr_t	nibble2_nibble_sel  : 3;
		mmr_t	reserved_4          : 1;
		mmr_t	nibble2_chiplet_sel : 3;
		mmr_t	reserved_3          : 1;
		mmr_t	nibble1_nibble_sel  : 3;
		mmr_t	reserved_2          : 1;
		mmr_t	nibble1_chiplet_sel : 3;
		mmr_t	reserved_1          : 1;
		mmr_t	nibble0_nibble_sel  : 3;
		mmr_t	reserved_0          : 1;
		mmr_t	nibble0_chiplet_sel : 3;
	} sh_pi_crbc_test_point_select_s;
} sh_pi_crbc_test_point_select_u_t;
#endif

/* ==================================================================== */
/*           Register "SH_PI_CRBC_TEST_POINT_TRIGGER_SELECT"            */
/*                  PI CRBC Test Point Trigger Select                   */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_crbc_test_point_trigger_select_u {
	mmr_t	sh_pi_crbc_test_point_trigger_select_regval;
	struct {
		mmr_t	trigger0_chiplet_sel : 3;
		mmr_t	reserved_0           : 1;
		mmr_t	trigger0_nibble_sel  : 3;
		mmr_t	reserved_1           : 1;
		mmr_t	trigger1_chiplet_sel : 3;
		mmr_t	reserved_2           : 1;
		mmr_t	trigger1_nibble_sel  : 3;
		mmr_t	reserved_3           : 1;
		mmr_t	trigger2_chiplet_sel : 3;
		mmr_t	reserved_4           : 1;
		mmr_t	trigger2_nibble_sel  : 3;
		mmr_t	reserved_5           : 1;
		mmr_t	trigger3_chiplet_sel : 3;
		mmr_t	reserved_6           : 1;
		mmr_t	trigger3_nibble_sel  : 3;
		mmr_t	reserved_7           : 1;
		mmr_t	trigger4_chiplet_sel : 3;
		mmr_t	reserved_8           : 1;
		mmr_t	trigger4_nibble_sel  : 3;
		mmr_t	reserved_9           : 1;
		mmr_t	trigger5_chiplet_sel : 3;
		mmr_t	reserved_10          : 1;
		mmr_t	trigger5_nibble_sel  : 3;
		mmr_t	reserved_11          : 1;
		mmr_t	trigger6_chiplet_sel : 3;
		mmr_t	reserved_12          : 1;
		mmr_t	trigger6_nibble_sel  : 3;
		mmr_t	reserved_13          : 1;
		mmr_t	trigger7_chiplet_sel : 3;
		mmr_t	reserved_14          : 1;
		mmr_t	trigger7_nibble_sel  : 3;
		mmr_t	reserved_15          : 1;
	} sh_pi_crbc_test_point_trigger_select_s;
} sh_pi_crbc_test_point_trigger_select_u_t;
#else
typedef union sh_pi_crbc_test_point_trigger_select_u {
	mmr_t	sh_pi_crbc_test_point_trigger_select_regval;
	struct {
		mmr_t	reserved_15          : 1;
		mmr_t	trigger7_nibble_sel  : 3;
		mmr_t	reserved_14          : 1;
		mmr_t	trigger7_chiplet_sel : 3;
		mmr_t	reserved_13          : 1;
		mmr_t	trigger6_nibble_sel  : 3;
		mmr_t	reserved_12          : 1;
		mmr_t	trigger6_chiplet_sel : 3;
		mmr_t	reserved_11          : 1;
		mmr_t	trigger5_nibble_sel  : 3;
		mmr_t	reserved_10          : 1;
		mmr_t	trigger5_chiplet_sel : 3;
		mmr_t	reserved_9           : 1;
		mmr_t	trigger4_nibble_sel  : 3;
		mmr_t	reserved_8           : 1;
		mmr_t	trigger4_chiplet_sel : 3;
		mmr_t	reserved_7           : 1;
		mmr_t	trigger3_nibble_sel  : 3;
		mmr_t	reserved_6           : 1;
		mmr_t	trigger3_chiplet_sel : 3;
		mmr_t	reserved_5           : 1;
		mmr_t	trigger2_nibble_sel  : 3;
		mmr_t	reserved_4           : 1;
		mmr_t	trigger2_chiplet_sel : 3;
		mmr_t	reserved_3           : 1;
		mmr_t	trigger1_nibble_sel  : 3;
		mmr_t	reserved_2           : 1;
		mmr_t	trigger1_chiplet_sel : 3;
		mmr_t	reserved_1           : 1;
		mmr_t	trigger0_nibble_sel  : 3;
		mmr_t	reserved_0           : 1;
		mmr_t	trigger0_chiplet_sel : 3;
	} sh_pi_crbc_test_point_trigger_select_s;
} sh_pi_crbc_test_point_trigger_select_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_PI_CRBP_ERROR_MASK"                   */
/*                          PI CRBP Error Mask                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_crbp_error_mask_u {
	mmr_t	sh_pi_crbp_error_mask_regval;
	struct {
		mmr_t	fsb_proto_err   : 1;
		mmr_t	gfx_rp_err      : 1;
		mmr_t	xb_proto_err    : 1;
		mmr_t	mem_rp_err      : 1;
		mmr_t	pio_rp_err      : 1;
		mmr_t	mem_to_err      : 1;
		mmr_t	pio_to_err      : 1;
		mmr_t	fsb_shub_uce    : 1;
		mmr_t	fsb_shub_ce     : 1;
		mmr_t	msg_color_err   : 1;
		mmr_t	md_rq_q_oflow   : 1;
		mmr_t	md_rp_q_oflow   : 1;
		mmr_t	xn_rq_q_oflow   : 1;
		mmr_t	xn_rp_q_oflow   : 1;
		mmr_t	nack_oflow      : 1;
		mmr_t	gfx_int_0       : 1;
		mmr_t	gfx_int_1       : 1;
		mmr_t	md_rq_crd_oflow : 1;
		mmr_t	md_rp_crd_oflow : 1;
		mmr_t	xn_rq_crd_oflow : 1;
		mmr_t	xn_rp_crd_oflow : 1;
		mmr_t	reserved_0      : 43;
	} sh_pi_crbp_error_mask_s;
} sh_pi_crbp_error_mask_u_t;
#else
typedef union sh_pi_crbp_error_mask_u {
	mmr_t	sh_pi_crbp_error_mask_regval;
	struct {
		mmr_t	reserved_0      : 43;
		mmr_t	xn_rp_crd_oflow : 1;
		mmr_t	xn_rq_crd_oflow : 1;
		mmr_t	md_rp_crd_oflow : 1;
		mmr_t	md_rq_crd_oflow : 1;
		mmr_t	gfx_int_1       : 1;
		mmr_t	gfx_int_0       : 1;
		mmr_t	nack_oflow      : 1;
		mmr_t	xn_rp_q_oflow   : 1;
		mmr_t	xn_rq_q_oflow   : 1;
		mmr_t	md_rp_q_oflow   : 1;
		mmr_t	md_rq_q_oflow   : 1;
		mmr_t	msg_color_err   : 1;
		mmr_t	fsb_shub_ce     : 1;
		mmr_t	fsb_shub_uce    : 1;
		mmr_t	pio_to_err      : 1;
		mmr_t	mem_to_err      : 1;
		mmr_t	pio_rp_err      : 1;
		mmr_t	mem_rp_err      : 1;
		mmr_t	xb_proto_err    : 1;
		mmr_t	gfx_rp_err      : 1;
		mmr_t	fsb_proto_err   : 1;
	} sh_pi_crbp_error_mask_s;
} sh_pi_crbp_error_mask_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_PI_CRBP_FSB_PIPE_COMPARE"                */
/*                        CRBP FSB Pipe Compare                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_crbp_fsb_pipe_compare_u {
	mmr_t	sh_pi_crbp_fsb_pipe_compare_regval;
	struct {
		mmr_t	compare_address : 47;
		mmr_t	compare_req     : 6;
		mmr_t	reserved_0      : 11;
	} sh_pi_crbp_fsb_pipe_compare_s;
} sh_pi_crbp_fsb_pipe_compare_u_t;
#else
typedef union sh_pi_crbp_fsb_pipe_compare_u {
	mmr_t	sh_pi_crbp_fsb_pipe_compare_regval;
	struct {
		mmr_t	reserved_0      : 11;
		mmr_t	compare_req     : 6;
		mmr_t	compare_address : 47;
	} sh_pi_crbp_fsb_pipe_compare_s;
} sh_pi_crbp_fsb_pipe_compare_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_PI_CRBP_FSB_PIPE_MASK"                  */
/*                          CRBP Compare Mask                           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_crbp_fsb_pipe_mask_u {
	mmr_t	sh_pi_crbp_fsb_pipe_mask_regval;
	struct {
		mmr_t	compare_address_mask : 47;
		mmr_t	compare_req_mask     : 6;
		mmr_t	reserved_0           : 11;
	} sh_pi_crbp_fsb_pipe_mask_s;
} sh_pi_crbp_fsb_pipe_mask_u_t;
#else
typedef union sh_pi_crbp_fsb_pipe_mask_u {
	mmr_t	sh_pi_crbp_fsb_pipe_mask_regval;
	struct {
		mmr_t	reserved_0           : 11;
		mmr_t	compare_req_mask     : 6;
		mmr_t	compare_address_mask : 47;
	} sh_pi_crbp_fsb_pipe_mask_s;
} sh_pi_crbp_fsb_pipe_mask_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_PI_CRBP_TEST_POINT_COMPARE"               */
/*                      PI CRBP Test Point Compare                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_crbp_test_point_compare_u {
	mmr_t	sh_pi_crbp_test_point_compare_regval;
	struct {
		mmr_t	compare_mask    : 32;
		mmr_t	compare_pattern : 32;
	} sh_pi_crbp_test_point_compare_s;
} sh_pi_crbp_test_point_compare_u_t;
#else
typedef union sh_pi_crbp_test_point_compare_u {
	mmr_t	sh_pi_crbp_test_point_compare_regval;
	struct {
		mmr_t	compare_pattern : 32;
		mmr_t	compare_mask    : 32;
	} sh_pi_crbp_test_point_compare_s;
} sh_pi_crbp_test_point_compare_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_PI_CRBP_TEST_POINT_SELECT"                */
/*                      PI CRBP Test Point Select                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_crbp_test_point_select_u {
	mmr_t	sh_pi_crbp_test_point_select_regval;
	struct {
		mmr_t	nibble0_chiplet_sel : 3;
		mmr_t	reserved_0          : 1;
		mmr_t	nibble0_nibble_sel  : 3;
		mmr_t	reserved_1          : 1;
		mmr_t	nibble1_chiplet_sel : 3;
		mmr_t	reserved_2          : 1;
		mmr_t	nibble1_nibble_sel  : 3;
		mmr_t	reserved_3          : 1;
		mmr_t	nibble2_chiplet_sel : 3;
		mmr_t	reserved_4          : 1;
		mmr_t	nibble2_nibble_sel  : 3;
		mmr_t	reserved_5          : 1;
		mmr_t	nibble3_chiplet_sel : 3;
		mmr_t	reserved_6          : 1;
		mmr_t	nibble3_nibble_sel  : 3;
		mmr_t	reserved_7          : 1;
		mmr_t	nibble4_chiplet_sel : 3;
		mmr_t	reserved_8          : 1;
		mmr_t	nibble4_nibble_sel  : 3;
		mmr_t	reserved_9          : 1;
		mmr_t	nibble5_chiplet_sel : 3;
		mmr_t	reserved_10         : 1;
		mmr_t	nibble5_nibble_sel  : 3;
		mmr_t	reserved_11         : 1;
		mmr_t	nibble6_chiplet_sel : 3;
		mmr_t	reserved_12         : 1;
		mmr_t	nibble6_nibble_sel  : 3;
		mmr_t	reserved_13         : 1;
		mmr_t	nibble7_chiplet_sel : 3;
		mmr_t	reserved_14         : 1;
		mmr_t	nibble7_nibble_sel  : 3;
		mmr_t	trigger_enable      : 1;
	} sh_pi_crbp_test_point_select_s;
} sh_pi_crbp_test_point_select_u_t;
#else
typedef union sh_pi_crbp_test_point_select_u {
	mmr_t	sh_pi_crbp_test_point_select_regval;
	struct {
		mmr_t	trigger_enable      : 1;
		mmr_t	nibble7_nibble_sel  : 3;
		mmr_t	reserved_14         : 1;
		mmr_t	nibble7_chiplet_sel : 3;
		mmr_t	reserved_13         : 1;
		mmr_t	nibble6_nibble_sel  : 3;
		mmr_t	reserved_12         : 1;
		mmr_t	nibble6_chiplet_sel : 3;
		mmr_t	reserved_11         : 1;
		mmr_t	nibble5_nibble_sel  : 3;
		mmr_t	reserved_10         : 1;
		mmr_t	nibble5_chiplet_sel : 3;
		mmr_t	reserved_9          : 1;
		mmr_t	nibble4_nibble_sel  : 3;
		mmr_t	reserved_8          : 1;
		mmr_t	nibble4_chiplet_sel : 3;
		mmr_t	reserved_7          : 1;
		mmr_t	nibble3_nibble_sel  : 3;
		mmr_t	reserved_6          : 1;
		mmr_t	nibble3_chiplet_sel : 3;
		mmr_t	reserved_5          : 1;
		mmr_t	nibble2_nibble_sel  : 3;
		mmr_t	reserved_4          : 1;
		mmr_t	nibble2_chiplet_sel : 3;
		mmr_t	reserved_3          : 1;
		mmr_t	nibble1_nibble_sel  : 3;
		mmr_t	reserved_2          : 1;
		mmr_t	nibble1_chiplet_sel : 3;
		mmr_t	reserved_1          : 1;
		mmr_t	nibble0_nibble_sel  : 3;
		mmr_t	reserved_0          : 1;
		mmr_t	nibble0_chiplet_sel : 3;
	} sh_pi_crbp_test_point_select_s;
} sh_pi_crbp_test_point_select_u_t;
#endif

/* ==================================================================== */
/*           Register "SH_PI_CRBP_TEST_POINT_TRIGGER_SELECT"            */
/*                  PI CRBP Test Point Trigger Select                   */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_crbp_test_point_trigger_select_u {
	mmr_t	sh_pi_crbp_test_point_trigger_select_regval;
	struct {
		mmr_t	trigger0_chiplet_sel : 3;
		mmr_t	reserved_0           : 1;
		mmr_t	trigger0_nibble_sel  : 3;
		mmr_t	reserved_1           : 1;
		mmr_t	trigger1_chiplet_sel : 3;
		mmr_t	reserved_2           : 1;
		mmr_t	trigger1_nibble_sel  : 3;
		mmr_t	reserved_3           : 1;
		mmr_t	trigger2_chiplet_sel : 3;
		mmr_t	reserved_4           : 1;
		mmr_t	trigger2_nibble_sel  : 3;
		mmr_t	reserved_5           : 1;
		mmr_t	trigger3_chiplet_sel : 3;
		mmr_t	reserved_6           : 1;
		mmr_t	trigger3_nibble_sel  : 3;
		mmr_t	reserved_7           : 1;
		mmr_t	trigger4_chiplet_sel : 3;
		mmr_t	reserved_8           : 1;
		mmr_t	trigger4_nibble_sel  : 3;
		mmr_t	reserved_9           : 1;
		mmr_t	trigger5_chiplet_sel : 3;
		mmr_t	reserved_10          : 1;
		mmr_t	trigger5_nibble_sel  : 3;
		mmr_t	reserved_11          : 1;
		mmr_t	trigger6_chiplet_sel : 3;
		mmr_t	reserved_12          : 1;
		mmr_t	trigger6_nibble_sel  : 3;
		mmr_t	reserved_13          : 1;
		mmr_t	trigger7_chiplet_sel : 3;
		mmr_t	reserved_14          : 1;
		mmr_t	trigger7_nibble_sel  : 3;
		mmr_t	reserved_15          : 1;
	} sh_pi_crbp_test_point_trigger_select_s;
} sh_pi_crbp_test_point_trigger_select_u_t;
#else
typedef union sh_pi_crbp_test_point_trigger_select_u {
	mmr_t	sh_pi_crbp_test_point_trigger_select_regval;
	struct {
		mmr_t	reserved_15          : 1;
		mmr_t	trigger7_nibble_sel  : 3;
		mmr_t	reserved_14          : 1;
		mmr_t	trigger7_chiplet_sel : 3;
		mmr_t	reserved_13          : 1;
		mmr_t	trigger6_nibble_sel  : 3;
		mmr_t	reserved_12          : 1;
		mmr_t	trigger6_chiplet_sel : 3;
		mmr_t	reserved_11          : 1;
		mmr_t	trigger5_nibble_sel  : 3;
		mmr_t	reserved_10          : 1;
		mmr_t	trigger5_chiplet_sel : 3;
		mmr_t	reserved_9           : 1;
		mmr_t	trigger4_nibble_sel  : 3;
		mmr_t	reserved_8           : 1;
		mmr_t	trigger4_chiplet_sel : 3;
		mmr_t	reserved_7           : 1;
		mmr_t	trigger3_nibble_sel  : 3;
		mmr_t	reserved_6           : 1;
		mmr_t	trigger3_chiplet_sel : 3;
		mmr_t	reserved_5           : 1;
		mmr_t	trigger2_nibble_sel  : 3;
		mmr_t	reserved_4           : 1;
		mmr_t	trigger2_chiplet_sel : 3;
		mmr_t	reserved_3           : 1;
		mmr_t	trigger1_nibble_sel  : 3;
		mmr_t	reserved_2           : 1;
		mmr_t	trigger1_chiplet_sel : 3;
		mmr_t	reserved_1           : 1;
		mmr_t	trigger0_nibble_sel  : 3;
		mmr_t	reserved_0           : 1;
		mmr_t	trigger0_chiplet_sel : 3;
	} sh_pi_crbp_test_point_trigger_select_s;
} sh_pi_crbp_test_point_trigger_select_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_PI_CRBP_XB_PIPE_COMPARE_0"                */
/*                         CRBP XB Pipe Compare                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_crbp_xb_pipe_compare_0_u {
	mmr_t	sh_pi_crbp_xb_pipe_compare_0_regval;
	struct {
		mmr_t	compare_address : 47;
		mmr_t	compare_command : 8;
		mmr_t	reserved_0      : 9;
	} sh_pi_crbp_xb_pipe_compare_0_s;
} sh_pi_crbp_xb_pipe_compare_0_u_t;
#else
typedef union sh_pi_crbp_xb_pipe_compare_0_u {
	mmr_t	sh_pi_crbp_xb_pipe_compare_0_regval;
	struct {
		mmr_t	reserved_0      : 9;
		mmr_t	compare_command : 8;
		mmr_t	compare_address : 47;
	} sh_pi_crbp_xb_pipe_compare_0_s;
} sh_pi_crbp_xb_pipe_compare_0_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_PI_CRBP_XB_PIPE_COMPARE_1"                */
/*                         CRBP XB Pipe Compare                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_crbp_xb_pipe_compare_1_u {
	mmr_t	sh_pi_crbp_xb_pipe_compare_1_regval;
	struct {
		mmr_t	compare_source       : 14;
		mmr_t	reserved_0           : 2;
		mmr_t	compare_supplemental : 14;
		mmr_t	reserved_1           : 2;
		mmr_t	compare_echo         : 9;
		mmr_t	reserved_2           : 23;
	} sh_pi_crbp_xb_pipe_compare_1_s;
} sh_pi_crbp_xb_pipe_compare_1_u_t;
#else
typedef union sh_pi_crbp_xb_pipe_compare_1_u {
	mmr_t	sh_pi_crbp_xb_pipe_compare_1_regval;
	struct {
		mmr_t	reserved_2           : 23;
		mmr_t	compare_echo         : 9;
		mmr_t	reserved_1           : 2;
		mmr_t	compare_supplemental : 14;
		mmr_t	reserved_0           : 2;
		mmr_t	compare_source       : 14;
	} sh_pi_crbp_xb_pipe_compare_1_s;
} sh_pi_crbp_xb_pipe_compare_1_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_PI_CRBP_XB_PIPE_MASK_0"                 */
/*                     CRBP Compare Mask Register 1                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_crbp_xb_pipe_mask_0_u {
	mmr_t	sh_pi_crbp_xb_pipe_mask_0_regval;
	struct {
		mmr_t	compare_address_mask : 47;
		mmr_t	compare_command_mask : 8;
		mmr_t	reserved_0           : 9;
	} sh_pi_crbp_xb_pipe_mask_0_s;
} sh_pi_crbp_xb_pipe_mask_0_u_t;
#else
typedef union sh_pi_crbp_xb_pipe_mask_0_u {
	mmr_t	sh_pi_crbp_xb_pipe_mask_0_regval;
	struct {
		mmr_t	reserved_0           : 9;
		mmr_t	compare_command_mask : 8;
		mmr_t	compare_address_mask : 47;
	} sh_pi_crbp_xb_pipe_mask_0_s;
} sh_pi_crbp_xb_pipe_mask_0_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_PI_CRBP_XB_PIPE_MASK_1"                 */
/*                 CRBP XB Pipe Compare Mask Register 1                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_crbp_xb_pipe_mask_1_u {
	mmr_t	sh_pi_crbp_xb_pipe_mask_1_regval;
	struct {
		mmr_t	compare_source_mask       : 14;
		mmr_t	reserved_0                : 2;
		mmr_t	compare_supplemental_mask : 14;
		mmr_t	reserved_1                : 2;
		mmr_t	compare_echo_mask         : 9;
		mmr_t	reserved_2                : 23;
	} sh_pi_crbp_xb_pipe_mask_1_s;
} sh_pi_crbp_xb_pipe_mask_1_u_t;
#else
typedef union sh_pi_crbp_xb_pipe_mask_1_u {
	mmr_t	sh_pi_crbp_xb_pipe_mask_1_regval;
	struct {
		mmr_t	reserved_2                : 23;
		mmr_t	compare_echo_mask         : 9;
		mmr_t	reserved_1                : 2;
		mmr_t	compare_supplemental_mask : 14;
		mmr_t	reserved_0                : 2;
		mmr_t	compare_source_mask       : 14;
	} sh_pi_crbp_xb_pipe_mask_1_s;
} sh_pi_crbp_xb_pipe_mask_1_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PI_DPC_QUEUE_CONFIG"                   */
/*                       DPC Queue Configuration                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_dpc_queue_config_u {
	mmr_t	sh_pi_dpc_queue_config_regval;
	struct {
		mmr_t	dwcq_ae_level  : 5;
		mmr_t	reserved_0     : 3;
		mmr_t	dwcq_af_thresh : 5;
		mmr_t	reserved_1     : 3;
		mmr_t	fwcq_ae_level  : 5;
		mmr_t	reserved_2     : 3;
		mmr_t	fwcq_af_thresh : 5;
		mmr_t	reserved_3     : 35;
	} sh_pi_dpc_queue_config_s;
} sh_pi_dpc_queue_config_u_t;
#else
typedef union sh_pi_dpc_queue_config_u {
	mmr_t	sh_pi_dpc_queue_config_regval;
	struct {
		mmr_t	reserved_3     : 35;
		mmr_t	fwcq_af_thresh : 5;
		mmr_t	reserved_2     : 3;
		mmr_t	fwcq_ae_level  : 5;
		mmr_t	reserved_1     : 3;
		mmr_t	dwcq_af_thresh : 5;
		mmr_t	reserved_0     : 3;
		mmr_t	dwcq_ae_level  : 5;
	} sh_pi_dpc_queue_config_s;
} sh_pi_dpc_queue_config_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_PI_ERROR_MASK"                      */
/*                            PI Error Mask                             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_error_mask_u {
	mmr_t	sh_pi_error_mask_regval;
	struct {
		mmr_t	fsb_proto_err   : 1;
		mmr_t	gfx_rp_err      : 1;
		mmr_t	xb_proto_err    : 1;
		mmr_t	mem_rp_err      : 1;
		mmr_t	pio_rp_err      : 1;
		mmr_t	mem_to_err      : 1;
		mmr_t	pio_to_err      : 1;
		mmr_t	fsb_shub_uce    : 1;
		mmr_t	fsb_shub_ce     : 1;
		mmr_t	msg_color_err   : 1;
		mmr_t	md_rq_q_oflow   : 1;
		mmr_t	md_rp_q_oflow   : 1;
		mmr_t	xn_rq_q_oflow   : 1;
		mmr_t	xn_rp_q_oflow   : 1;
		mmr_t	nack_oflow      : 1;
		mmr_t	gfx_int_0       : 1;
		mmr_t	gfx_int_1       : 1;
		mmr_t	md_rq_crd_oflow : 1;
		mmr_t	md_rp_crd_oflow : 1;
		mmr_t	xn_rq_crd_oflow : 1;
		mmr_t	xn_rp_crd_oflow : 1;
		mmr_t	hung_bus        : 1;
		mmr_t	rsp_parity      : 1;
		mmr_t	ioq_overrun     : 1;
		mmr_t	req_format      : 1;
		mmr_t	addr_access     : 1;
		mmr_t	req_parity      : 1;
		mmr_t	addr_parity     : 1;
		mmr_t	shub_fsb_dqe    : 1;
		mmr_t	shub_fsb_uce    : 1;
		mmr_t	shub_fsb_ce     : 1;
		mmr_t	livelock        : 1;
		mmr_t	bad_snoop       : 1;
		mmr_t	fsb_tbl_miss    : 1;
		mmr_t	msg_length      : 1;
		mmr_t	reserved_0      : 29;
	} sh_pi_error_mask_s;
} sh_pi_error_mask_u_t;
#else
typedef union sh_pi_error_mask_u {
	mmr_t	sh_pi_error_mask_regval;
	struct {
		mmr_t	reserved_0      : 29;
		mmr_t	msg_length      : 1;
		mmr_t	fsb_tbl_miss    : 1;
		mmr_t	bad_snoop       : 1;
		mmr_t	livelock        : 1;
		mmr_t	shub_fsb_ce     : 1;
		mmr_t	shub_fsb_uce    : 1;
		mmr_t	shub_fsb_dqe    : 1;
		mmr_t	addr_parity     : 1;
		mmr_t	req_parity      : 1;
		mmr_t	addr_access     : 1;
		mmr_t	req_format      : 1;
		mmr_t	ioq_overrun     : 1;
		mmr_t	rsp_parity      : 1;
		mmr_t	hung_bus        : 1;
		mmr_t	xn_rp_crd_oflow : 1;
		mmr_t	xn_rq_crd_oflow : 1;
		mmr_t	md_rp_crd_oflow : 1;
		mmr_t	md_rq_crd_oflow : 1;
		mmr_t	gfx_int_1       : 1;
		mmr_t	gfx_int_0       : 1;
		mmr_t	nack_oflow      : 1;
		mmr_t	xn_rp_q_oflow   : 1;
		mmr_t	xn_rq_q_oflow   : 1;
		mmr_t	md_rp_q_oflow   : 1;
		mmr_t	md_rq_q_oflow   : 1;
		mmr_t	msg_color_err   : 1;
		mmr_t	fsb_shub_ce     : 1;
		mmr_t	fsb_shub_uce    : 1;
		mmr_t	pio_to_err      : 1;
		mmr_t	mem_to_err      : 1;
		mmr_t	pio_rp_err      : 1;
		mmr_t	mem_rp_err      : 1;
		mmr_t	xb_proto_err    : 1;
		mmr_t	gfx_rp_err      : 1;
		mmr_t	fsb_proto_err   : 1;
	} sh_pi_error_mask_s;
} sh_pi_error_mask_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_PI_EXPRESS_REPLY_CONFIG"                 */
/*                    PI Express Reply Configuration                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_express_reply_config_u {
	mmr_t	sh_pi_express_reply_config_regval;
	struct {
		mmr_t	mode        : 3;
		mmr_t	reserved_0  : 61;
	} sh_pi_express_reply_config_s;
} sh_pi_express_reply_config_u_t;
#else
typedef union sh_pi_express_reply_config_u {
	mmr_t	sh_pi_express_reply_config_regval;
	struct {
		mmr_t	reserved_0  : 61;
		mmr_t	mode        : 3;
	} sh_pi_express_reply_config_s;
} sh_pi_express_reply_config_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PI_FSB_COMPARE_VALUE"                  */
/*                          FSB Compare Value                           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_fsb_compare_value_u {
	mmr_t	sh_pi_fsb_compare_value_regval;
	struct {
		mmr_t	compare_value : 64;
	} sh_pi_fsb_compare_value_s;
} sh_pi_fsb_compare_value_u_t;
#else
typedef union sh_pi_fsb_compare_value_u {
	mmr_t	sh_pi_fsb_compare_value_regval;
	struct {
		mmr_t	compare_value : 64;
	} sh_pi_fsb_compare_value_s;
} sh_pi_fsb_compare_value_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PI_FSB_COMPARE_MASK"                   */
/*                           FSB Compare Mask                           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_fsb_compare_mask_u {
	mmr_t	sh_pi_fsb_compare_mask_regval;
	struct {
		mmr_t	mask_value  : 64;
	} sh_pi_fsb_compare_mask_s;
} sh_pi_fsb_compare_mask_u_t;
#else
typedef union sh_pi_fsb_compare_mask_u {
	mmr_t	sh_pi_fsb_compare_mask_regval;
	struct {
		mmr_t	mask_value  : 64;
	} sh_pi_fsb_compare_mask_s;
} sh_pi_fsb_compare_mask_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_PI_FSB_ERROR_INJECTION"                 */
/*                     Inject an Error onto the FSB                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_fsb_error_injection_u {
	mmr_t	sh_pi_fsb_error_injection_regval;
	struct {
		mmr_t	rp_pe_to_fsb     : 1;
		mmr_t	ap0_pe_to_fsb    : 1;
		mmr_t	ap1_pe_to_fsb    : 1;
		mmr_t	rsp_pe_to_fsb    : 1;
		mmr_t	dw0_ce_to_fsb    : 1;
		mmr_t	dw0_uce_to_fsb   : 1;
		mmr_t	dw1_ce_to_fsb    : 1;
		mmr_t	dw1_uce_to_fsb   : 1;
		mmr_t	ip0_pe_to_fsb    : 1;
		mmr_t	ip1_pe_to_fsb    : 1;
		mmr_t	reserved_0       : 6;
		mmr_t	rp_pe_from_fsb   : 1;
		mmr_t	ap0_pe_from_fsb  : 1;
		mmr_t	ap1_pe_from_fsb  : 1;
		mmr_t	rsp_pe_from_fsb  : 1;
		mmr_t	dw0_ce_from_fsb  : 1;
		mmr_t	dw0_uce_from_fsb : 1;
		mmr_t	dw1_ce_from_fsb  : 1;
		mmr_t	dw1_uce_from_fsb : 1;
		mmr_t	dw2_ce_from_fsb  : 1;
		mmr_t	dw2_uce_from_fsb : 1;
		mmr_t	dw3_ce_from_fsb  : 1;
		mmr_t	dw3_uce_from_fsb : 1;
		mmr_t	reserved_1       : 4;
		mmr_t	ioq_overrun      : 1;
		mmr_t	livelock         : 1;
		mmr_t	bus_hang         : 1;
		mmr_t	reserved_2       : 29;
	} sh_pi_fsb_error_injection_s;
} sh_pi_fsb_error_injection_u_t;
#else
typedef union sh_pi_fsb_error_injection_u {
	mmr_t	sh_pi_fsb_error_injection_regval;
	struct {
		mmr_t	reserved_2       : 29;
		mmr_t	bus_hang         : 1;
		mmr_t	livelock         : 1;
		mmr_t	ioq_overrun      : 1;
		mmr_t	reserved_1       : 4;
		mmr_t	dw3_uce_from_fsb : 1;
		mmr_t	dw3_ce_from_fsb  : 1;
		mmr_t	dw2_uce_from_fsb : 1;
		mmr_t	dw2_ce_from_fsb  : 1;
		mmr_t	dw1_uce_from_fsb : 1;
		mmr_t	dw1_ce_from_fsb  : 1;
		mmr_t	dw0_uce_from_fsb : 1;
		mmr_t	dw0_ce_from_fsb  : 1;
		mmr_t	rsp_pe_from_fsb  : 1;
		mmr_t	ap1_pe_from_fsb  : 1;
		mmr_t	ap0_pe_from_fsb  : 1;
		mmr_t	rp_pe_from_fsb   : 1;
		mmr_t	reserved_0       : 6;
		mmr_t	ip1_pe_to_fsb    : 1;
		mmr_t	ip0_pe_to_fsb    : 1;
		mmr_t	dw1_uce_to_fsb   : 1;
		mmr_t	dw1_ce_to_fsb    : 1;
		mmr_t	dw0_uce_to_fsb   : 1;
		mmr_t	dw0_ce_to_fsb    : 1;
		mmr_t	rsp_pe_to_fsb    : 1;
		mmr_t	ap1_pe_to_fsb    : 1;
		mmr_t	ap0_pe_to_fsb    : 1;
		mmr_t	rp_pe_to_fsb     : 1;
	} sh_pi_fsb_error_injection_s;
} sh_pi_fsb_error_injection_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_PI_MD2PI_REPLY_VC_CONFIG"                */
/*             MD-to-PI Reply Virtual Channel Configuration             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_md2pi_reply_vc_config_u {
	mmr_t	sh_pi_md2pi_reply_vc_config_regval;
	struct {
		mmr_t	hdr_depth             : 4;
		mmr_t	data_depth            : 4;
		mmr_t	max_credits           : 6;
		mmr_t	reserved_0            : 48;
		mmr_t	force_credit          : 1;
		mmr_t	capture_credit_status : 1;
	} sh_pi_md2pi_reply_vc_config_s;
} sh_pi_md2pi_reply_vc_config_u_t;
#else
typedef union sh_pi_md2pi_reply_vc_config_u {
	mmr_t	sh_pi_md2pi_reply_vc_config_regval;
	struct {
		mmr_t	capture_credit_status : 1;
		mmr_t	force_credit          : 1;
		mmr_t	reserved_0            : 48;
		mmr_t	max_credits           : 6;
		mmr_t	data_depth            : 4;
		mmr_t	hdr_depth             : 4;
	} sh_pi_md2pi_reply_vc_config_s;
} sh_pi_md2pi_reply_vc_config_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_PI_MD2PI_REQUEST_VC_CONFIG"               */
/*            MD-to-PI Request Virtual Channel Configuration            */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_md2pi_request_vc_config_u {
	mmr_t	sh_pi_md2pi_request_vc_config_regval;
	struct {
		mmr_t	hdr_depth             : 4;
		mmr_t	data_depth            : 4;
		mmr_t	max_credits           : 6;
		mmr_t	reserved_0            : 48;
		mmr_t	force_credit          : 1;
		mmr_t	capture_credit_status : 1;
	} sh_pi_md2pi_request_vc_config_s;
} sh_pi_md2pi_request_vc_config_u_t;
#else
typedef union sh_pi_md2pi_request_vc_config_u {
	mmr_t	sh_pi_md2pi_request_vc_config_regval;
	struct {
		mmr_t	capture_credit_status : 1;
		mmr_t	force_credit          : 1;
		mmr_t	reserved_0            : 48;
		mmr_t	max_credits           : 6;
		mmr_t	data_depth            : 4;
		mmr_t	hdr_depth             : 4;
	} sh_pi_md2pi_request_vc_config_s;
} sh_pi_md2pi_request_vc_config_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_PI_QUEUE_ERROR_INJECTION"                */
/*                       PI Queue Error Injection                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_queue_error_injection_u {
	mmr_t	sh_pi_queue_error_injection_regval;
	struct {
		mmr_t	dat_dfr_q      : 1;
		mmr_t	dxb_wtl_cmnd_q : 1;
		mmr_t	fsb_wtl_cmnd_q : 1;
		mmr_t	mdpi_rpy_bfr   : 1;
		mmr_t	ptc_intr       : 1;
		mmr_t	rxl_kill_q     : 1;
		mmr_t	rxl_rdy_q      : 1;
		mmr_t	xnpi_rpy_bfr   : 1;
		mmr_t	reserved_0     : 56;
	} sh_pi_queue_error_injection_s;
} sh_pi_queue_error_injection_u_t;
#else
typedef union sh_pi_queue_error_injection_u {
	mmr_t	sh_pi_queue_error_injection_regval;
	struct {
		mmr_t	reserved_0     : 56;
		mmr_t	xnpi_rpy_bfr   : 1;
		mmr_t	rxl_rdy_q      : 1;
		mmr_t	rxl_kill_q     : 1;
		mmr_t	ptc_intr       : 1;
		mmr_t	mdpi_rpy_bfr   : 1;
		mmr_t	fsb_wtl_cmnd_q : 1;
		mmr_t	dxb_wtl_cmnd_q : 1;
		mmr_t	dat_dfr_q      : 1;
	} sh_pi_queue_error_injection_s;
} sh_pi_queue_error_injection_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_PI_TEST_POINT_COMPARE"                  */
/*                        PI Test Point Compare                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_test_point_compare_u {
	mmr_t	sh_pi_test_point_compare_regval;
	struct {
		mmr_t	compare_mask    : 32;
		mmr_t	compare_pattern : 32;
	} sh_pi_test_point_compare_s;
} sh_pi_test_point_compare_u_t;
#else
typedef union sh_pi_test_point_compare_u {
	mmr_t	sh_pi_test_point_compare_regval;
	struct {
		mmr_t	compare_pattern : 32;
		mmr_t	compare_mask    : 32;
	} sh_pi_test_point_compare_s;
} sh_pi_test_point_compare_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PI_TEST_POINT_SELECT"                  */
/*                         PI Test Point Select                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_test_point_select_u {
	mmr_t	sh_pi_test_point_select_regval;
	struct {
		mmr_t	nibble0_chiplet_sel : 3;
		mmr_t	reserved_0          : 1;
		mmr_t	nibble0_nibble_sel  : 3;
		mmr_t	reserved_1          : 1;
		mmr_t	nibble1_chiplet_sel : 3;
		mmr_t	reserved_2          : 1;
		mmr_t	nibble1_nibble_sel  : 3;
		mmr_t	reserved_3          : 1;
		mmr_t	nibble2_chiplet_sel : 3;
		mmr_t	reserved_4          : 1;
		mmr_t	nibble2_nibble_sel  : 3;
		mmr_t	reserved_5          : 1;
		mmr_t	nibble3_chiplet_sel : 3;
		mmr_t	reserved_6          : 1;
		mmr_t	nibble3_nibble_sel  : 3;
		mmr_t	reserved_7          : 1;
		mmr_t	nibble4_chiplet_sel : 3;
		mmr_t	reserved_8          : 1;
		mmr_t	nibble4_nibble_sel  : 3;
		mmr_t	reserved_9          : 1;
		mmr_t	nibble5_chiplet_sel : 3;
		mmr_t	reserved_10         : 1;
		mmr_t	nibble5_nibble_sel  : 3;
		mmr_t	reserved_11         : 1;
		mmr_t	nibble6_chiplet_sel : 3;
		mmr_t	reserved_12         : 1;
		mmr_t	nibble6_nibble_sel  : 3;
		mmr_t	reserved_13         : 1;
		mmr_t	nibble7_chiplet_sel : 3;
		mmr_t	reserved_14         : 1;
		mmr_t	nibble7_nibble_sel  : 3;
		mmr_t	trigger_enable      : 1;
	} sh_pi_test_point_select_s;
} sh_pi_test_point_select_u_t;
#else
typedef union sh_pi_test_point_select_u {
	mmr_t	sh_pi_test_point_select_regval;
	struct {
		mmr_t	trigger_enable      : 1;
		mmr_t	nibble7_nibble_sel  : 3;
		mmr_t	reserved_14         : 1;
		mmr_t	nibble7_chiplet_sel : 3;
		mmr_t	reserved_13         : 1;
		mmr_t	nibble6_nibble_sel  : 3;
		mmr_t	reserved_12         : 1;
		mmr_t	nibble6_chiplet_sel : 3;
		mmr_t	reserved_11         : 1;
		mmr_t	nibble5_nibble_sel  : 3;
		mmr_t	reserved_10         : 1;
		mmr_t	nibble5_chiplet_sel : 3;
		mmr_t	reserved_9          : 1;
		mmr_t	nibble4_nibble_sel  : 3;
		mmr_t	reserved_8          : 1;
		mmr_t	nibble4_chiplet_sel : 3;
		mmr_t	reserved_7          : 1;
		mmr_t	nibble3_nibble_sel  : 3;
		mmr_t	reserved_6          : 1;
		mmr_t	nibble3_chiplet_sel : 3;
		mmr_t	reserved_5          : 1;
		mmr_t	nibble2_nibble_sel  : 3;
		mmr_t	reserved_4          : 1;
		mmr_t	nibble2_chiplet_sel : 3;
		mmr_t	reserved_3          : 1;
		mmr_t	nibble1_nibble_sel  : 3;
		mmr_t	reserved_2          : 1;
		mmr_t	nibble1_chiplet_sel : 3;
		mmr_t	reserved_1          : 1;
		mmr_t	nibble0_nibble_sel  : 3;
		mmr_t	reserved_0          : 1;
		mmr_t	nibble0_chiplet_sel : 3;
	} sh_pi_test_point_select_s;
} sh_pi_test_point_select_u_t;
#endif

/* ==================================================================== */
/*              Register "SH_PI_TEST_POINT_TRIGGER_SELECT"              */
/*                     PI Test Point Trigger Select                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_test_point_trigger_select_u {
	mmr_t	sh_pi_test_point_trigger_select_regval;
	struct {
		mmr_t	trigger0_chiplet_sel : 3;
		mmr_t	reserved_0           : 1;
		mmr_t	trigger0_nibble_sel  : 3;
		mmr_t	reserved_1           : 1;
		mmr_t	trigger1_chiplet_sel : 3;
		mmr_t	reserved_2           : 1;
		mmr_t	trigger1_nibble_sel  : 3;
		mmr_t	reserved_3           : 1;
		mmr_t	trigger2_chiplet_sel : 3;
		mmr_t	reserved_4           : 1;
		mmr_t	trigger2_nibble_sel  : 3;
		mmr_t	reserved_5           : 1;
		mmr_t	trigger3_chiplet_sel : 3;
		mmr_t	reserved_6           : 1;
		mmr_t	trigger3_nibble_sel  : 3;
		mmr_t	reserved_7           : 1;
		mmr_t	trigger4_chiplet_sel : 3;
		mmr_t	reserved_8           : 1;
		mmr_t	trigger4_nibble_sel  : 3;
		mmr_t	reserved_9           : 1;
		mmr_t	trigger5_chiplet_sel : 3;
		mmr_t	reserved_10          : 1;
		mmr_t	trigger5_nibble_sel  : 3;
		mmr_t	reserved_11          : 1;
		mmr_t	trigger6_chiplet_sel : 3;
		mmr_t	reserved_12          : 1;
		mmr_t	trigger6_nibble_sel  : 3;
		mmr_t	reserved_13          : 1;
		mmr_t	trigger7_chiplet_sel : 3;
		mmr_t	reserved_14          : 1;
		mmr_t	trigger7_nibble_sel  : 3;
		mmr_t	reserved_15          : 1;
	} sh_pi_test_point_trigger_select_s;
} sh_pi_test_point_trigger_select_u_t;
#else
typedef union sh_pi_test_point_trigger_select_u {
	mmr_t	sh_pi_test_point_trigger_select_regval;
	struct {
		mmr_t	reserved_15          : 1;
		mmr_t	trigger7_nibble_sel  : 3;
		mmr_t	reserved_14          : 1;
		mmr_t	trigger7_chiplet_sel : 3;
		mmr_t	reserved_13          : 1;
		mmr_t	trigger6_nibble_sel  : 3;
		mmr_t	reserved_12          : 1;
		mmr_t	trigger6_chiplet_sel : 3;
		mmr_t	reserved_11          : 1;
		mmr_t	trigger5_nibble_sel  : 3;
		mmr_t	reserved_10          : 1;
		mmr_t	trigger5_chiplet_sel : 3;
		mmr_t	reserved_9           : 1;
		mmr_t	trigger4_nibble_sel  : 3;
		mmr_t	reserved_8           : 1;
		mmr_t	trigger4_chiplet_sel : 3;
		mmr_t	reserved_7           : 1;
		mmr_t	trigger3_nibble_sel  : 3;
		mmr_t	reserved_6           : 1;
		mmr_t	trigger3_chiplet_sel : 3;
		mmr_t	reserved_5           : 1;
		mmr_t	trigger2_nibble_sel  : 3;
		mmr_t	reserved_4           : 1;
		mmr_t	trigger2_chiplet_sel : 3;
		mmr_t	reserved_3           : 1;
		mmr_t	trigger1_nibble_sel  : 3;
		mmr_t	reserved_2           : 1;
		mmr_t	trigger1_chiplet_sel : 3;
		mmr_t	reserved_1           : 1;
		mmr_t	trigger0_nibble_sel  : 3;
		mmr_t	reserved_0           : 1;
		mmr_t	trigger0_chiplet_sel : 3;
	} sh_pi_test_point_trigger_select_s;
} sh_pi_test_point_trigger_select_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_PI_XN2PI_REPLY_VC_CONFIG"                */
/*             XN-to-PI Reply Virtual Channel Configuration             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_xn2pi_reply_vc_config_u {
	mmr_t	sh_pi_xn2pi_reply_vc_config_regval;
	struct {
		mmr_t	hdr_depth             : 4;
		mmr_t	data_depth            : 4;
		mmr_t	max_credits           : 6;
		mmr_t	reserved_0            : 48;
		mmr_t	force_credit          : 1;
		mmr_t	capture_credit_status : 1;
	} sh_pi_xn2pi_reply_vc_config_s;
} sh_pi_xn2pi_reply_vc_config_u_t;
#else
typedef union sh_pi_xn2pi_reply_vc_config_u {
	mmr_t	sh_pi_xn2pi_reply_vc_config_regval;
	struct {
		mmr_t	capture_credit_status : 1;
		mmr_t	force_credit          : 1;
		mmr_t	reserved_0            : 48;
		mmr_t	max_credits           : 6;
		mmr_t	data_depth            : 4;
		mmr_t	hdr_depth             : 4;
	} sh_pi_xn2pi_reply_vc_config_s;
} sh_pi_xn2pi_reply_vc_config_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_PI_XN2PI_REQUEST_VC_CONFIG"               */
/*            XN-to-PI Request Virtual Channel Configuration            */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_xn2pi_request_vc_config_u {
	mmr_t	sh_pi_xn2pi_request_vc_config_regval;
	struct {
		mmr_t	hdr_depth             : 4;
		mmr_t	data_depth            : 4;
		mmr_t	max_credits           : 6;
		mmr_t	reserved_0            : 48;
		mmr_t	force_credit          : 1;
		mmr_t	capture_credit_status : 1;
	} sh_pi_xn2pi_request_vc_config_s;
} sh_pi_xn2pi_request_vc_config_u_t;
#else
typedef union sh_pi_xn2pi_request_vc_config_u {
	mmr_t	sh_pi_xn2pi_request_vc_config_regval;
	struct {
		mmr_t	capture_credit_status : 1;
		mmr_t	force_credit          : 1;
		mmr_t	reserved_0            : 48;
		mmr_t	max_credits           : 6;
		mmr_t	data_depth            : 4;
		mmr_t	hdr_depth             : 4;
	} sh_pi_xn2pi_request_vc_config_s;
} sh_pi_xn2pi_request_vc_config_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_PI_AEC_STATUS"                      */
/*                 PI Adaptive Error Correction Status                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_aec_status_u {
	mmr_t	sh_pi_aec_status_regval;
	struct {
		mmr_t	state       : 3;
		mmr_t	reserved_0  : 61;
	} sh_pi_aec_status_s;
} sh_pi_aec_status_u_t;
#else
typedef union sh_pi_aec_status_u {
	mmr_t	sh_pi_aec_status_regval;
	struct {
		mmr_t	reserved_0  : 61;
		mmr_t	state       : 3;
	} sh_pi_aec_status_s;
} sh_pi_aec_status_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_PI_AFI_FIRST_ERROR"                   */
/*                          PI AFI First Error                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_afi_first_error_u {
	mmr_t	sh_pi_afi_first_error_regval;
	struct {
		mmr_t	reserved_0   : 7;
		mmr_t	fsb_shub_uce : 1;
		mmr_t	fsb_shub_ce  : 1;
		mmr_t	reserved_1   : 12;
		mmr_t	hung_bus     : 1;
		mmr_t	rsp_parity   : 1;
		mmr_t	ioq_overrun  : 1;
		mmr_t	req_format   : 1;
		mmr_t	addr_access  : 1;
		mmr_t	req_parity   : 1;
		mmr_t	addr_parity  : 1;
		mmr_t	shub_fsb_dqe : 1;
		mmr_t	shub_fsb_uce : 1;
		mmr_t	shub_fsb_ce  : 1;
		mmr_t	livelock     : 1;
		mmr_t	bad_snoop    : 1;
		mmr_t	fsb_tbl_miss : 1;
		mmr_t	msg_len      : 1;
		mmr_t	reserved_2   : 29;
	} sh_pi_afi_first_error_s;
} sh_pi_afi_first_error_u_t;
#else
typedef union sh_pi_afi_first_error_u {
	mmr_t	sh_pi_afi_first_error_regval;
	struct {
		mmr_t	reserved_2   : 29;
		mmr_t	msg_len      : 1;
		mmr_t	fsb_tbl_miss : 1;
		mmr_t	bad_snoop    : 1;
		mmr_t	livelock     : 1;
		mmr_t	shub_fsb_ce  : 1;
		mmr_t	shub_fsb_uce : 1;
		mmr_t	shub_fsb_dqe : 1;
		mmr_t	addr_parity  : 1;
		mmr_t	req_parity   : 1;
		mmr_t	addr_access  : 1;
		mmr_t	req_format   : 1;
		mmr_t	ioq_overrun  : 1;
		mmr_t	rsp_parity   : 1;
		mmr_t	hung_bus     : 1;
		mmr_t	reserved_1   : 12;
		mmr_t	fsb_shub_ce  : 1;
		mmr_t	fsb_shub_uce : 1;
		mmr_t	reserved_0   : 7;
	} sh_pi_afi_first_error_s;
} sh_pi_afi_first_error_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_PI_CAM_ADDRESS_READ_DATA"                */
/*                    CRB CAM MMR Address Read Data                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_cam_address_read_data_u {
	mmr_t	sh_pi_cam_address_read_data_regval;
	struct {
		mmr_t	cam_addr     : 48;
		mmr_t	reserved_0   : 15;
		mmr_t	cam_addr_val : 1;
	} sh_pi_cam_address_read_data_s;
} sh_pi_cam_address_read_data_u_t;
#else
typedef union sh_pi_cam_address_read_data_u {
	mmr_t	sh_pi_cam_address_read_data_regval;
	struct {
		mmr_t	cam_addr_val : 1;
		mmr_t	reserved_0   : 15;
		mmr_t	cam_addr     : 48;
	} sh_pi_cam_address_read_data_s;
} sh_pi_cam_address_read_data_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_PI_CAM_LPRA_READ_DATA"                  */
/*                      CRB CAM MMR LPRA Read Data                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_cam_lpra_read_data_u {
	mmr_t	sh_pi_cam_lpra_read_data_regval;
	struct {
		mmr_t	cam_lpra    : 64;
	} sh_pi_cam_lpra_read_data_s;
} sh_pi_cam_lpra_read_data_u_t;
#else
typedef union sh_pi_cam_lpra_read_data_u {
	mmr_t	sh_pi_cam_lpra_read_data_regval;
	struct {
		mmr_t	cam_lpra    : 64;
	} sh_pi_cam_lpra_read_data_s;
} sh_pi_cam_lpra_read_data_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_PI_CAM_STATE_READ_DATA"                 */
/*                     CRB CAM MMR State Read Data                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_cam_state_read_data_u {
	mmr_t	sh_pi_cam_state_read_data_regval;
	struct {
		mmr_t	cam_state         : 4;
		mmr_t	cam_to            : 1;
		mmr_t	cam_state_rd_pend : 1;
		mmr_t	reserved_0        : 26;
		mmr_t	cam_lpra          : 18;
		mmr_t	reserved_1        : 13;
		mmr_t	cam_rd_data_val   : 1;
	} sh_pi_cam_state_read_data_s;
} sh_pi_cam_state_read_data_u_t;
#else
typedef union sh_pi_cam_state_read_data_u {
	mmr_t	sh_pi_cam_state_read_data_regval;
	struct {
		mmr_t	cam_rd_data_val   : 1;
		mmr_t	reserved_1        : 13;
		mmr_t	cam_lpra          : 18;
		mmr_t	reserved_0        : 26;
		mmr_t	cam_state_rd_pend : 1;
		mmr_t	cam_to            : 1;
		mmr_t	cam_state         : 4;
	} sh_pi_cam_state_read_data_s;
} sh_pi_cam_state_read_data_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_PI_CORRECTED_DETAIL_1"                  */
/*                      PI Corrected Error Detail                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_corrected_detail_1_u {
	mmr_t	sh_pi_corrected_detail_1_regval;
	struct {
		mmr_t	address     : 48;
		mmr_t	syndrome    : 8;
		mmr_t	dep         : 8;
	} sh_pi_corrected_detail_1_s;
} sh_pi_corrected_detail_1_u_t;
#else
typedef union sh_pi_corrected_detail_1_u {
	mmr_t	sh_pi_corrected_detail_1_regval;
	struct {
		mmr_t	dep         : 8;
		mmr_t	syndrome    : 8;
		mmr_t	address     : 48;
	} sh_pi_corrected_detail_1_s;
} sh_pi_corrected_detail_1_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_PI_CORRECTED_DETAIL_2"                  */
/*                     PI Corrected Error Detail 2                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_corrected_detail_2_u {
	mmr_t	sh_pi_corrected_detail_2_regval;
	struct {
		mmr_t	data        : 64;
	} sh_pi_corrected_detail_2_s;
} sh_pi_corrected_detail_2_u_t;
#else
typedef union sh_pi_corrected_detail_2_u {
	mmr_t	sh_pi_corrected_detail_2_regval;
	struct {
		mmr_t	data        : 64;
	} sh_pi_corrected_detail_2_s;
} sh_pi_corrected_detail_2_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_PI_CORRECTED_DETAIL_3"                  */
/*                     PI Corrected Error Detail 3                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_corrected_detail_3_u {
	mmr_t	sh_pi_corrected_detail_3_regval;
	struct {
		mmr_t	address     : 48;
		mmr_t	syndrome    : 8;
		mmr_t	dep         : 8;
	} sh_pi_corrected_detail_3_s;
} sh_pi_corrected_detail_3_u_t;
#else
typedef union sh_pi_corrected_detail_3_u {
	mmr_t	sh_pi_corrected_detail_3_regval;
	struct {
		mmr_t	dep         : 8;
		mmr_t	syndrome    : 8;
		mmr_t	address     : 48;
	} sh_pi_corrected_detail_3_s;
} sh_pi_corrected_detail_3_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_PI_CORRECTED_DETAIL_4"                  */
/*                     PI Corrected Error Detail 4                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_corrected_detail_4_u {
	mmr_t	sh_pi_corrected_detail_4_regval;
	struct {
		mmr_t	data        : 64;
	} sh_pi_corrected_detail_4_s;
} sh_pi_corrected_detail_4_u_t;
#else
typedef union sh_pi_corrected_detail_4_u {
	mmr_t	sh_pi_corrected_detail_4_regval;
	struct {
		mmr_t	data        : 64;
	} sh_pi_corrected_detail_4_s;
} sh_pi_corrected_detail_4_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PI_CRBP_FIRST_ERROR"                   */
/*                         PI CRBP First Error                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_crbp_first_error_u {
	mmr_t	sh_pi_crbp_first_error_regval;
	struct {
		mmr_t	fsb_proto_err   : 1;
		mmr_t	gfx_rp_err      : 1;
		mmr_t	xb_proto_err    : 1;
		mmr_t	mem_rp_err      : 1;
		mmr_t	pio_rp_err      : 1;
		mmr_t	mem_to_err      : 1;
		mmr_t	pio_to_err      : 1;
		mmr_t	fsb_shub_uce    : 1;
		mmr_t	fsb_shub_ce     : 1;
		mmr_t	msg_color_err   : 1;
		mmr_t	md_rq_q_oflow   : 1;
		mmr_t	md_rp_q_oflow   : 1;
		mmr_t	xn_rq_q_oflow   : 1;
		mmr_t	xn_rp_q_oflow   : 1;
		mmr_t	nack_oflow      : 1;
		mmr_t	gfx_int_0       : 1;
		mmr_t	gfx_int_1       : 1;
		mmr_t	md_rq_crd_oflow : 1;
		mmr_t	md_rp_crd_oflow : 1;
		mmr_t	xn_rq_crd_oflow : 1;
		mmr_t	xn_rp_crd_oflow : 1;
		mmr_t	reserved_0      : 43;
	} sh_pi_crbp_first_error_s;
} sh_pi_crbp_first_error_u_t;
#else
typedef union sh_pi_crbp_first_error_u {
	mmr_t	sh_pi_crbp_first_error_regval;
	struct {
		mmr_t	reserved_0      : 43;
		mmr_t	xn_rp_crd_oflow : 1;
		mmr_t	xn_rq_crd_oflow : 1;
		mmr_t	md_rp_crd_oflow : 1;
		mmr_t	md_rq_crd_oflow : 1;
		mmr_t	gfx_int_1       : 1;
		mmr_t	gfx_int_0       : 1;
		mmr_t	nack_oflow      : 1;
		mmr_t	xn_rp_q_oflow   : 1;
		mmr_t	xn_rq_q_oflow   : 1;
		mmr_t	md_rp_q_oflow   : 1;
		mmr_t	md_rq_q_oflow   : 1;
		mmr_t	msg_color_err   : 1;
		mmr_t	fsb_shub_ce     : 1;
		mmr_t	fsb_shub_uce    : 1;
		mmr_t	pio_to_err      : 1;
		mmr_t	mem_to_err      : 1;
		mmr_t	pio_rp_err      : 1;
		mmr_t	mem_rp_err      : 1;
		mmr_t	xb_proto_err    : 1;
		mmr_t	gfx_rp_err      : 1;
		mmr_t	fsb_proto_err   : 1;
	} sh_pi_crbp_first_error_s;
} sh_pi_crbp_first_error_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_PI_ERROR_DETAIL_1"                    */
/*                          PI Error Detail 1                           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_error_detail_1_u {
	mmr_t	sh_pi_error_detail_1_regval;
	struct {
		mmr_t	status      : 64;
	} sh_pi_error_detail_1_s;
} sh_pi_error_detail_1_u_t;
#else
typedef union sh_pi_error_detail_1_u {
	mmr_t	sh_pi_error_detail_1_regval;
	struct {
		mmr_t	status      : 64;
	} sh_pi_error_detail_1_s;
} sh_pi_error_detail_1_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_PI_ERROR_DETAIL_2"                    */
/*                          PI Error Detail 2                           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_error_detail_2_u {
	mmr_t	sh_pi_error_detail_2_regval;
	struct {
		mmr_t	status      : 64;
	} sh_pi_error_detail_2_s;
} sh_pi_error_detail_2_u_t;
#else
typedef union sh_pi_error_detail_2_u {
	mmr_t	sh_pi_error_detail_2_regval;
	struct {
		mmr_t	status      : 64;
	} sh_pi_error_detail_2_s;
} sh_pi_error_detail_2_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_PI_ERROR_OVERFLOW"                    */
/*                          PI Error Overflow                           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_error_overflow_u {
	mmr_t	sh_pi_error_overflow_regval;
	struct {
		mmr_t	fsb_proto_err   : 1;
		mmr_t	gfx_rp_err      : 1;
		mmr_t	xb_proto_err    : 1;
		mmr_t	mem_rp_err      : 1;
		mmr_t	pio_rp_err      : 1;
		mmr_t	mem_to_err      : 1;
		mmr_t	pio_to_err      : 1;
		mmr_t	fsb_shub_uce    : 1;
		mmr_t	fsb_shub_ce     : 1;
		mmr_t	msg_color_err   : 1;
		mmr_t	md_rq_q_oflow   : 1;
		mmr_t	md_rp_q_oflow   : 1;
		mmr_t	xn_rq_q_oflow   : 1;
		mmr_t	xn_rp_q_oflow   : 1;
		mmr_t	nack_oflow      : 1;
		mmr_t	gfx_int_0       : 1;
		mmr_t	gfx_int_1       : 1;
		mmr_t	md_rq_crd_oflow : 1;
		mmr_t	md_rp_crd_oflow : 1;
		mmr_t	xn_rq_crd_oflow : 1;
		mmr_t	xn_rp_crd_oflow : 1;
		mmr_t	hung_bus        : 1;
		mmr_t	rsp_parity      : 1;
		mmr_t	ioq_overrun     : 1;
		mmr_t	req_format      : 1;
		mmr_t	addr_access     : 1;
		mmr_t	req_parity      : 1;
		mmr_t	addr_parity     : 1;
		mmr_t	shub_fsb_dqe    : 1;
		mmr_t	shub_fsb_uce    : 1;
		mmr_t	shub_fsb_ce     : 1;
		mmr_t	livelock        : 1;
		mmr_t	bad_snoop       : 1;
		mmr_t	fsb_tbl_miss    : 1;
		mmr_t	msg_length      : 1;
		mmr_t	reserved_0      : 29;
	} sh_pi_error_overflow_s;
} sh_pi_error_overflow_u_t;
#else
typedef union sh_pi_error_overflow_u {
	mmr_t	sh_pi_error_overflow_regval;
	struct {
		mmr_t	reserved_0      : 29;
		mmr_t	msg_length      : 1;
		mmr_t	fsb_tbl_miss    : 1;
		mmr_t	bad_snoop       : 1;
		mmr_t	livelock        : 1;
		mmr_t	shub_fsb_ce     : 1;
		mmr_t	shub_fsb_uce    : 1;
		mmr_t	shub_fsb_dqe    : 1;
		mmr_t	addr_parity     : 1;
		mmr_t	req_parity      : 1;
		mmr_t	addr_access     : 1;
		mmr_t	req_format      : 1;
		mmr_t	ioq_overrun     : 1;
		mmr_t	rsp_parity      : 1;
		mmr_t	hung_bus        : 1;
		mmr_t	xn_rp_crd_oflow : 1;
		mmr_t	xn_rq_crd_oflow : 1;
		mmr_t	md_rp_crd_oflow : 1;
		mmr_t	md_rq_crd_oflow : 1;
		mmr_t	gfx_int_1       : 1;
		mmr_t	gfx_int_0       : 1;
		mmr_t	nack_oflow      : 1;
		mmr_t	xn_rp_q_oflow   : 1;
		mmr_t	xn_rq_q_oflow   : 1;
		mmr_t	md_rp_q_oflow   : 1;
		mmr_t	md_rq_q_oflow   : 1;
		mmr_t	msg_color_err   : 1;
		mmr_t	fsb_shub_ce     : 1;
		mmr_t	fsb_shub_uce    : 1;
		mmr_t	pio_to_err      : 1;
		mmr_t	mem_to_err      : 1;
		mmr_t	pio_rp_err      : 1;
		mmr_t	mem_rp_err      : 1;
		mmr_t	xb_proto_err    : 1;
		mmr_t	gfx_rp_err      : 1;
		mmr_t	fsb_proto_err   : 1;
	} sh_pi_error_overflow_s;
} sh_pi_error_overflow_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_PI_ERROR_SUMMARY"                    */
/*                           PI Error Summary                           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_error_summary_u {
	mmr_t	sh_pi_error_summary_regval;
	struct {
		mmr_t	fsb_proto_err   : 1;
		mmr_t	gfx_rp_err      : 1;
		mmr_t	xb_proto_err    : 1;
		mmr_t	mem_rp_err      : 1;
		mmr_t	pio_rp_err      : 1;
		mmr_t	mem_to_err      : 1;
		mmr_t	pio_to_err      : 1;
		mmr_t	fsb_shub_uce    : 1;
		mmr_t	fsb_shub_ce     : 1;
		mmr_t	msg_color_err   : 1;
		mmr_t	md_rq_q_oflow   : 1;
		mmr_t	md_rp_q_oflow   : 1;
		mmr_t	xn_rq_q_oflow   : 1;
		mmr_t	xn_rp_q_oflow   : 1;
		mmr_t	nack_oflow      : 1;
		mmr_t	gfx_int_0       : 1;
		mmr_t	gfx_int_1       : 1;
		mmr_t	md_rq_crd_oflow : 1;
		mmr_t	md_rp_crd_oflow : 1;
		mmr_t	xn_rq_crd_oflow : 1;
		mmr_t	xn_rp_crd_oflow : 1;
		mmr_t	hung_bus        : 1;
		mmr_t	rsp_parity      : 1;
		mmr_t	ioq_overrun     : 1;
		mmr_t	req_format      : 1;
		mmr_t	addr_access     : 1;
		mmr_t	req_parity      : 1;
		mmr_t	addr_parity     : 1;
		mmr_t	shub_fsb_dqe    : 1;
		mmr_t	shub_fsb_uce    : 1;
		mmr_t	shub_fsb_ce     : 1;
		mmr_t	livelock        : 1;
		mmr_t	bad_snoop       : 1;
		mmr_t	fsb_tbl_miss    : 1;
		mmr_t	msg_length      : 1;
		mmr_t	reserved_0      : 29;
	} sh_pi_error_summary_s;
} sh_pi_error_summary_u_t;
#else
typedef union sh_pi_error_summary_u {
	mmr_t	sh_pi_error_summary_regval;
	struct {
		mmr_t	reserved_0      : 29;
		mmr_t	msg_length      : 1;
		mmr_t	fsb_tbl_miss    : 1;
		mmr_t	bad_snoop       : 1;
		mmr_t	livelock        : 1;
		mmr_t	shub_fsb_ce     : 1;
		mmr_t	shub_fsb_uce    : 1;
		mmr_t	shub_fsb_dqe    : 1;
		mmr_t	addr_parity     : 1;
		mmr_t	req_parity      : 1;
		mmr_t	addr_access     : 1;
		mmr_t	req_format      : 1;
		mmr_t	ioq_overrun     : 1;
		mmr_t	rsp_parity      : 1;
		mmr_t	hung_bus        : 1;
		mmr_t	xn_rp_crd_oflow : 1;
		mmr_t	xn_rq_crd_oflow : 1;
		mmr_t	md_rp_crd_oflow : 1;
		mmr_t	md_rq_crd_oflow : 1;
		mmr_t	gfx_int_1       : 1;
		mmr_t	gfx_int_0       : 1;
		mmr_t	nack_oflow      : 1;
		mmr_t	xn_rp_q_oflow   : 1;
		mmr_t	xn_rq_q_oflow   : 1;
		mmr_t	md_rp_q_oflow   : 1;
		mmr_t	md_rq_q_oflow   : 1;
		mmr_t	msg_color_err   : 1;
		mmr_t	fsb_shub_ce     : 1;
		mmr_t	fsb_shub_uce    : 1;
		mmr_t	pio_to_err      : 1;
		mmr_t	mem_to_err      : 1;
		mmr_t	pio_rp_err      : 1;
		mmr_t	mem_rp_err      : 1;
		mmr_t	xb_proto_err    : 1;
		mmr_t	gfx_rp_err      : 1;
		mmr_t	fsb_proto_err   : 1;
	} sh_pi_error_summary_s;
} sh_pi_error_summary_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_PI_EXPRESS_REPLY_STATUS"                 */
/*                       PI Express Reply Status                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_express_reply_status_u {
	mmr_t	sh_pi_express_reply_status_regval;
	struct {
		mmr_t	state       : 3;
		mmr_t	reserved_0  : 61;
	} sh_pi_express_reply_status_s;
} sh_pi_express_reply_status_u_t;
#else
typedef union sh_pi_express_reply_status_u {
	mmr_t	sh_pi_express_reply_status_regval;
	struct {
		mmr_t	reserved_0  : 61;
		mmr_t	state       : 3;
	} sh_pi_express_reply_status_s;
} sh_pi_express_reply_status_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_PI_FIRST_ERROR"                     */
/*                            PI First Error                            */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_first_error_u {
	mmr_t	sh_pi_first_error_regval;
	struct {
		mmr_t	fsb_proto_err   : 1;
		mmr_t	gfx_rp_err      : 1;
		mmr_t	xb_proto_err    : 1;
		mmr_t	mem_rp_err      : 1;
		mmr_t	pio_rp_err      : 1;
		mmr_t	mem_to_err      : 1;
		mmr_t	pio_to_err      : 1;
		mmr_t	fsb_shub_uce    : 1;
		mmr_t	fsb_shub_ce     : 1;
		mmr_t	msg_color_err   : 1;
		mmr_t	md_rq_q_oflow   : 1;
		mmr_t	md_rp_q_oflow   : 1;
		mmr_t	xn_rq_q_oflow   : 1;
		mmr_t	xn_rp_q_oflow   : 1;
		mmr_t	nack_oflow      : 1;
		mmr_t	gfx_int_0       : 1;
		mmr_t	gfx_int_1       : 1;
		mmr_t	md_rq_crd_oflow : 1;
		mmr_t	md_rp_crd_oflow : 1;
		mmr_t	xn_rq_crd_oflow : 1;
		mmr_t	xn_rp_crd_oflow : 1;
		mmr_t	hung_bus        : 1;
		mmr_t	rsp_parity      : 1;
		mmr_t	ioq_overrun     : 1;
		mmr_t	req_format      : 1;
		mmr_t	addr_access     : 1;
		mmr_t	req_parity      : 1;
		mmr_t	addr_parity     : 1;
		mmr_t	shub_fsb_dqe    : 1;
		mmr_t	shub_fsb_uce    : 1;
		mmr_t	shub_fsb_ce     : 1;
		mmr_t	livelock        : 1;
		mmr_t	bad_snoop       : 1;
		mmr_t	fsb_tbl_miss    : 1;
		mmr_t	msg_length      : 1;
		mmr_t	reserved_0      : 29;
	} sh_pi_first_error_s;
} sh_pi_first_error_u_t;
#else
typedef union sh_pi_first_error_u {
	mmr_t	sh_pi_first_error_regval;
	struct {
		mmr_t	reserved_0      : 29;
		mmr_t	msg_length      : 1;
		mmr_t	fsb_tbl_miss    : 1;
		mmr_t	bad_snoop       : 1;
		mmr_t	livelock        : 1;
		mmr_t	shub_fsb_ce     : 1;
		mmr_t	shub_fsb_uce    : 1;
		mmr_t	shub_fsb_dqe    : 1;
		mmr_t	addr_parity     : 1;
		mmr_t	req_parity      : 1;
		mmr_t	addr_access     : 1;
		mmr_t	req_format      : 1;
		mmr_t	ioq_overrun     : 1;
		mmr_t	rsp_parity      : 1;
		mmr_t	hung_bus        : 1;
		mmr_t	xn_rp_crd_oflow : 1;
		mmr_t	xn_rq_crd_oflow : 1;
		mmr_t	md_rp_crd_oflow : 1;
		mmr_t	md_rq_crd_oflow : 1;
		mmr_t	gfx_int_1       : 1;
		mmr_t	gfx_int_0       : 1;
		mmr_t	nack_oflow      : 1;
		mmr_t	xn_rp_q_oflow   : 1;
		mmr_t	xn_rq_q_oflow   : 1;
		mmr_t	md_rp_q_oflow   : 1;
		mmr_t	md_rq_q_oflow   : 1;
		mmr_t	msg_color_err   : 1;
		mmr_t	fsb_shub_ce     : 1;
		mmr_t	fsb_shub_uce    : 1;
		mmr_t	pio_to_err      : 1;
		mmr_t	mem_to_err      : 1;
		mmr_t	pio_rp_err      : 1;
		mmr_t	mem_rp_err      : 1;
		mmr_t	xb_proto_err    : 1;
		mmr_t	gfx_rp_err      : 1;
		mmr_t	fsb_proto_err   : 1;
	} sh_pi_first_error_s;
} sh_pi_first_error_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_PI_PI2MD_REPLY_VC_STATUS"                */
/*                PI-to-MD Reply Virtual Channel Status                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_pi2md_reply_vc_status_u {
	mmr_t	sh_pi_pi2md_reply_vc_status_regval;
	struct {
		mmr_t	output_crd_stat : 6;
		mmr_t	reserved_0      : 58;
	} sh_pi_pi2md_reply_vc_status_s;
} sh_pi_pi2md_reply_vc_status_u_t;
#else
typedef union sh_pi_pi2md_reply_vc_status_u {
	mmr_t	sh_pi_pi2md_reply_vc_status_regval;
	struct {
		mmr_t	reserved_0      : 58;
		mmr_t	output_crd_stat : 6;
	} sh_pi_pi2md_reply_vc_status_s;
} sh_pi_pi2md_reply_vc_status_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_PI_PI2MD_REQUEST_VC_STATUS"               */
/*               PI-to-MD Request Virtual Channel Status                */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_pi2md_request_vc_status_u {
	mmr_t	sh_pi_pi2md_request_vc_status_regval;
	struct {
		mmr_t	output_crd_stat : 6;
		mmr_t	reserved_0      : 58;
	} sh_pi_pi2md_request_vc_status_s;
} sh_pi_pi2md_request_vc_status_u_t;
#else
typedef union sh_pi_pi2md_request_vc_status_u {
	mmr_t	sh_pi_pi2md_request_vc_status_regval;
	struct {
		mmr_t	reserved_0      : 58;
		mmr_t	output_crd_stat : 6;
	} sh_pi_pi2md_request_vc_status_s;
} sh_pi_pi2md_request_vc_status_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_PI_PI2XN_REPLY_VC_STATUS"                */
/*                PI-to-XN Reply Virtual Channel Status                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_pi2xn_reply_vc_status_u {
	mmr_t	sh_pi_pi2xn_reply_vc_status_regval;
	struct {
		mmr_t	output_crd_stat : 6;
		mmr_t	reserved_0      : 58;
	} sh_pi_pi2xn_reply_vc_status_s;
} sh_pi_pi2xn_reply_vc_status_u_t;
#else
typedef union sh_pi_pi2xn_reply_vc_status_u {
	mmr_t	sh_pi_pi2xn_reply_vc_status_regval;
	struct {
		mmr_t	reserved_0      : 58;
		mmr_t	output_crd_stat : 6;
	} sh_pi_pi2xn_reply_vc_status_s;
} sh_pi_pi2xn_reply_vc_status_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_PI_PI2XN_REQUEST_VC_STATUS"               */
/*               PI-to-XN Request Virtual Channel Status                */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_pi2xn_request_vc_status_u {
	mmr_t	sh_pi_pi2xn_request_vc_status_regval;
	struct {
		mmr_t	output_crd_stat : 6;
		mmr_t	reserved_0      : 58;
	} sh_pi_pi2xn_request_vc_status_s;
} sh_pi_pi2xn_request_vc_status_u_t;
#else
typedef union sh_pi_pi2xn_request_vc_status_u {
	mmr_t	sh_pi_pi2xn_request_vc_status_regval;
	struct {
		mmr_t	reserved_0      : 58;
		mmr_t	output_crd_stat : 6;
	} sh_pi_pi2xn_request_vc_status_s;
} sh_pi_pi2xn_request_vc_status_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_PI_UNCORRECTED_DETAIL_1"                 */
/*                    PI Uncorrected Error Detail 1                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_uncorrected_detail_1_u {
	mmr_t	sh_pi_uncorrected_detail_1_regval;
	struct {
		mmr_t	address     : 48;
		mmr_t	syndrome    : 8;
		mmr_t	dep         : 8;
	} sh_pi_uncorrected_detail_1_s;
} sh_pi_uncorrected_detail_1_u_t;
#else
typedef union sh_pi_uncorrected_detail_1_u {
	mmr_t	sh_pi_uncorrected_detail_1_regval;
	struct {
		mmr_t	dep         : 8;
		mmr_t	syndrome    : 8;
		mmr_t	address     : 48;
	} sh_pi_uncorrected_detail_1_s;
} sh_pi_uncorrected_detail_1_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_PI_UNCORRECTED_DETAIL_2"                 */
/*                    PI Uncorrected Error Detail 2                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_uncorrected_detail_2_u {
	mmr_t	sh_pi_uncorrected_detail_2_regval;
	struct {
		mmr_t	data        : 64;
	} sh_pi_uncorrected_detail_2_s;
} sh_pi_uncorrected_detail_2_u_t;
#else
typedef union sh_pi_uncorrected_detail_2_u {
	mmr_t	sh_pi_uncorrected_detail_2_regval;
	struct {
		mmr_t	data        : 64;
	} sh_pi_uncorrected_detail_2_s;
} sh_pi_uncorrected_detail_2_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_PI_UNCORRECTED_DETAIL_3"                 */
/*                    PI Uncorrected Error Detail 3                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_uncorrected_detail_3_u {
	mmr_t	sh_pi_uncorrected_detail_3_regval;
	struct {
		mmr_t	address     : 48;
		mmr_t	syndrome    : 8;
		mmr_t	dep         : 8;
	} sh_pi_uncorrected_detail_3_s;
} sh_pi_uncorrected_detail_3_u_t;
#else
typedef union sh_pi_uncorrected_detail_3_u {
	mmr_t	sh_pi_uncorrected_detail_3_regval;
	struct {
		mmr_t	dep         : 8;
		mmr_t	syndrome    : 8;
		mmr_t	address     : 48;
	} sh_pi_uncorrected_detail_3_s;
} sh_pi_uncorrected_detail_3_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_PI_UNCORRECTED_DETAIL_4"                 */
/*                    PI Uncorrected Error Detail 4                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_uncorrected_detail_4_u {
	mmr_t	sh_pi_uncorrected_detail_4_regval;
	struct {
		mmr_t	data        : 64;
	} sh_pi_uncorrected_detail_4_s;
} sh_pi_uncorrected_detail_4_u_t;
#else
typedef union sh_pi_uncorrected_detail_4_u {
	mmr_t	sh_pi_uncorrected_detail_4_regval;
	struct {
		mmr_t	data        : 64;
	} sh_pi_uncorrected_detail_4_s;
} sh_pi_uncorrected_detail_4_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_PI_MD2PI_REPLY_VC_STATUS"                */
/*                MD-to-PI Reply Virtual Channel Status                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_md2pi_reply_vc_status_u {
	mmr_t	sh_pi_md2pi_reply_vc_status_regval;
	struct {
		mmr_t	input_hdr_crd_stat : 4;
		mmr_t	input_dat_crd_stat : 4;
		mmr_t	input_queue_stat   : 4;
		mmr_t	reserved_0         : 52;
	} sh_pi_md2pi_reply_vc_status_s;
} sh_pi_md2pi_reply_vc_status_u_t;
#else
typedef union sh_pi_md2pi_reply_vc_status_u {
	mmr_t	sh_pi_md2pi_reply_vc_status_regval;
	struct {
		mmr_t	reserved_0         : 52;
		mmr_t	input_queue_stat   : 4;
		mmr_t	input_dat_crd_stat : 4;
		mmr_t	input_hdr_crd_stat : 4;
	} sh_pi_md2pi_reply_vc_status_s;
} sh_pi_md2pi_reply_vc_status_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_PI_MD2PI_REQUEST_VC_STATUS"               */
/*               MD-to-PI Request Virtual Channel Status                */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_md2pi_request_vc_status_u {
	mmr_t	sh_pi_md2pi_request_vc_status_regval;
	struct {
		mmr_t	input_hdr_crd_stat : 4;
		mmr_t	input_dat_crd_stat : 4;
		mmr_t	input_queue_stat   : 4;
		mmr_t	reserved_0         : 52;
	} sh_pi_md2pi_request_vc_status_s;
} sh_pi_md2pi_request_vc_status_u_t;
#else
typedef union sh_pi_md2pi_request_vc_status_u {
	mmr_t	sh_pi_md2pi_request_vc_status_regval;
	struct {
		mmr_t	reserved_0         : 52;
		mmr_t	input_queue_stat   : 4;
		mmr_t	input_dat_crd_stat : 4;
		mmr_t	input_hdr_crd_stat : 4;
	} sh_pi_md2pi_request_vc_status_s;
} sh_pi_md2pi_request_vc_status_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_PI_XN2PI_REPLY_VC_STATUS"                */
/*                XN-to-PI Reply Virtual Channel Status                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_xn2pi_reply_vc_status_u {
	mmr_t	sh_pi_xn2pi_reply_vc_status_regval;
	struct {
		mmr_t	input_hdr_crd_stat : 4;
		mmr_t	input_dat_crd_stat : 4;
		mmr_t	input_queue_stat   : 4;
		mmr_t	reserved_0         : 52;
	} sh_pi_xn2pi_reply_vc_status_s;
} sh_pi_xn2pi_reply_vc_status_u_t;
#else
typedef union sh_pi_xn2pi_reply_vc_status_u {
	mmr_t	sh_pi_xn2pi_reply_vc_status_regval;
	struct {
		mmr_t	reserved_0         : 52;
		mmr_t	input_queue_stat   : 4;
		mmr_t	input_dat_crd_stat : 4;
		mmr_t	input_hdr_crd_stat : 4;
	} sh_pi_xn2pi_reply_vc_status_s;
} sh_pi_xn2pi_reply_vc_status_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_PI_XN2PI_REQUEST_VC_STATUS"               */
/*               XN-to-PI Request Virtual Channel Status                */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_xn2pi_request_vc_status_u {
	mmr_t	sh_pi_xn2pi_request_vc_status_regval;
	struct {
		mmr_t	input_hdr_crd_stat : 4;
		mmr_t	input_dat_crd_stat : 4;
		mmr_t	input_queue_stat   : 4;
		mmr_t	reserved_0         : 52;
	} sh_pi_xn2pi_request_vc_status_s;
} sh_pi_xn2pi_request_vc_status_u_t;
#else
typedef union sh_pi_xn2pi_request_vc_status_u {
	mmr_t	sh_pi_xn2pi_request_vc_status_regval;
	struct {
		mmr_t	reserved_0         : 52;
		mmr_t	input_queue_stat   : 4;
		mmr_t	input_dat_crd_stat : 4;
		mmr_t	input_hdr_crd_stat : 4;
	} sh_pi_xn2pi_request_vc_status_s;
} sh_pi_xn2pi_request_vc_status_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_XNPI_SIC_FLOW"                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnpi_sic_flow_u {
	mmr_t	sh_xnpi_sic_flow_regval;
	struct {
		mmr_t	debit_vc0_withhold   : 5;
		mmr_t	reserved_0           : 2;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	debit_vc2_withhold   : 5;
		mmr_t	reserved_1           : 2;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	credit_vc0_test      : 5;
		mmr_t	reserved_2           : 3;
		mmr_t	credit_vc0_dyn       : 5;
		mmr_t	reserved_3           : 3;
		mmr_t	credit_vc0_cap       : 5;
		mmr_t	reserved_4           : 3;
		mmr_t	credit_vc2_test      : 5;
		mmr_t	reserved_5           : 3;
		mmr_t	credit_vc2_dyn       : 5;
		mmr_t	reserved_6           : 3;
		mmr_t	credit_vc2_cap       : 5;
		mmr_t	reserved_7           : 2;
		mmr_t	disable_bypass_out   : 1;
	} sh_xnpi_sic_flow_s;
} sh_xnpi_sic_flow_u_t;
#else
typedef union sh_xnpi_sic_flow_u {
	mmr_t	sh_xnpi_sic_flow_regval;
	struct {
		mmr_t	disable_bypass_out   : 1;
		mmr_t	reserved_7           : 2;
		mmr_t	credit_vc2_cap       : 5;
		mmr_t	reserved_6           : 3;
		mmr_t	credit_vc2_dyn       : 5;
		mmr_t	reserved_5           : 3;
		mmr_t	credit_vc2_test      : 5;
		mmr_t	reserved_4           : 3;
		mmr_t	credit_vc0_cap       : 5;
		mmr_t	reserved_3           : 3;
		mmr_t	credit_vc0_dyn       : 5;
		mmr_t	reserved_2           : 3;
		mmr_t	credit_vc0_test      : 5;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	reserved_1           : 2;
		mmr_t	debit_vc2_withhold   : 5;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	reserved_0           : 2;
		mmr_t	debit_vc0_withhold   : 5;
	} sh_xnpi_sic_flow_s;
} sh_xnpi_sic_flow_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XNPI_TO_NI0_PORT_FLOW"                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnpi_to_ni0_port_flow_u {
	mmr_t	sh_xnpi_to_ni0_port_flow_regval;
	struct {
		mmr_t	debit_vc0_withhold   : 6;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	debit_vc2_withhold   : 6;
		mmr_t	reserved_1           : 1;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	reserved_2           : 8;
		mmr_t	credit_vc0_dyn       : 6;
		mmr_t	reserved_3           : 2;
		mmr_t	credit_vc0_cap       : 6;
		mmr_t	reserved_4           : 10;
		mmr_t	credit_vc2_dyn       : 6;
		mmr_t	reserved_5           : 2;
		mmr_t	credit_vc2_cap       : 6;
		mmr_t	reserved_6           : 2;
	} sh_xnpi_to_ni0_port_flow_s;
} sh_xnpi_to_ni0_port_flow_u_t;
#else
typedef union sh_xnpi_to_ni0_port_flow_u {
	mmr_t	sh_xnpi_to_ni0_port_flow_regval;
	struct {
		mmr_t	reserved_6           : 2;
		mmr_t	credit_vc2_cap       : 6;
		mmr_t	reserved_5           : 2;
		mmr_t	credit_vc2_dyn       : 6;
		mmr_t	reserved_4           : 10;
		mmr_t	credit_vc0_cap       : 6;
		mmr_t	reserved_3           : 2;
		mmr_t	credit_vc0_dyn       : 6;
		mmr_t	reserved_2           : 8;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	reserved_1           : 1;
		mmr_t	debit_vc2_withhold   : 6;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc0_withhold   : 6;
	} sh_xnpi_to_ni0_port_flow_s;
} sh_xnpi_to_ni0_port_flow_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XNPI_TO_NI1_PORT_FLOW"                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnpi_to_ni1_port_flow_u {
	mmr_t	sh_xnpi_to_ni1_port_flow_regval;
	struct {
		mmr_t	debit_vc0_withhold   : 6;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	debit_vc2_withhold   : 6;
		mmr_t	reserved_1           : 1;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	reserved_2           : 8;
		mmr_t	credit_vc0_dyn       : 6;
		mmr_t	reserved_3           : 2;
		mmr_t	credit_vc0_cap       : 6;
		mmr_t	reserved_4           : 10;
		mmr_t	credit_vc2_dyn       : 6;
		mmr_t	reserved_5           : 2;
		mmr_t	credit_vc2_cap       : 6;
		mmr_t	reserved_6           : 2;
	} sh_xnpi_to_ni1_port_flow_s;
} sh_xnpi_to_ni1_port_flow_u_t;
#else
typedef union sh_xnpi_to_ni1_port_flow_u {
	mmr_t	sh_xnpi_to_ni1_port_flow_regval;
	struct {
		mmr_t	reserved_6           : 2;
		mmr_t	credit_vc2_cap       : 6;
		mmr_t	reserved_5           : 2;
		mmr_t	credit_vc2_dyn       : 6;
		mmr_t	reserved_4           : 10;
		mmr_t	credit_vc0_cap       : 6;
		mmr_t	reserved_3           : 2;
		mmr_t	credit_vc0_dyn       : 6;
		mmr_t	reserved_2           : 8;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	reserved_1           : 1;
		mmr_t	debit_vc2_withhold   : 6;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc0_withhold   : 6;
	} sh_xnpi_to_ni1_port_flow_s;
} sh_xnpi_to_ni1_port_flow_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XNPI_TO_IILB_PORT_FLOW"                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnpi_to_iilb_port_flow_u {
	mmr_t	sh_xnpi_to_iilb_port_flow_regval;
	struct {
		mmr_t	debit_vc0_withhold   : 6;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	debit_vc2_withhold   : 6;
		mmr_t	reserved_1           : 1;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	reserved_2           : 8;
		mmr_t	credit_vc0_dyn       : 6;
		mmr_t	reserved_3           : 2;
		mmr_t	credit_vc0_cap       : 6;
		mmr_t	reserved_4           : 10;
		mmr_t	credit_vc2_dyn       : 6;
		mmr_t	reserved_5           : 2;
		mmr_t	credit_vc2_cap       : 6;
		mmr_t	reserved_6           : 2;
	} sh_xnpi_to_iilb_port_flow_s;
} sh_xnpi_to_iilb_port_flow_u_t;
#else
typedef union sh_xnpi_to_iilb_port_flow_u {
	mmr_t	sh_xnpi_to_iilb_port_flow_regval;
	struct {
		mmr_t	reserved_6           : 2;
		mmr_t	credit_vc2_cap       : 6;
		mmr_t	reserved_5           : 2;
		mmr_t	credit_vc2_dyn       : 6;
		mmr_t	reserved_4           : 10;
		mmr_t	credit_vc0_cap       : 6;
		mmr_t	reserved_3           : 2;
		mmr_t	credit_vc0_dyn       : 6;
		mmr_t	reserved_2           : 8;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	reserved_1           : 1;
		mmr_t	debit_vc2_withhold   : 6;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc0_withhold   : 6;
	} sh_xnpi_to_iilb_port_flow_s;
} sh_xnpi_to_iilb_port_flow_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_XNPI_FR_NI0_PORT_FLOW_FIFO"               */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnpi_fr_ni0_port_flow_fifo_u {
	mmr_t	sh_xnpi_fr_ni0_port_flow_fifo_regval;
	struct {
		mmr_t	entry_vc0_dyn  : 6;
		mmr_t	reserved_0     : 2;
		mmr_t	entry_vc0_cap  : 6;
		mmr_t	reserved_1     : 2;
		mmr_t	entry_vc2_dyn  : 6;
		mmr_t	reserved_2     : 2;
		mmr_t	entry_vc2_cap  : 6;
		mmr_t	reserved_3     : 2;
		mmr_t	entry_vc0_test : 5;
		mmr_t	reserved_4     : 3;
		mmr_t	entry_vc2_test : 5;
		mmr_t	reserved_5     : 19;
	} sh_xnpi_fr_ni0_port_flow_fifo_s;
} sh_xnpi_fr_ni0_port_flow_fifo_u_t;
#else
typedef union sh_xnpi_fr_ni0_port_flow_fifo_u {
	mmr_t	sh_xnpi_fr_ni0_port_flow_fifo_regval;
	struct {
		mmr_t	reserved_5     : 19;
		mmr_t	entry_vc2_test : 5;
		mmr_t	reserved_4     : 3;
		mmr_t	entry_vc0_test : 5;
		mmr_t	reserved_3     : 2;
		mmr_t	entry_vc2_cap  : 6;
		mmr_t	reserved_2     : 2;
		mmr_t	entry_vc2_dyn  : 6;
		mmr_t	reserved_1     : 2;
		mmr_t	entry_vc0_cap  : 6;
		mmr_t	reserved_0     : 2;
		mmr_t	entry_vc0_dyn  : 6;
	} sh_xnpi_fr_ni0_port_flow_fifo_s;
} sh_xnpi_fr_ni0_port_flow_fifo_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_XNPI_FR_NI1_PORT_FLOW_FIFO"               */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnpi_fr_ni1_port_flow_fifo_u {
	mmr_t	sh_xnpi_fr_ni1_port_flow_fifo_regval;
	struct {
		mmr_t	entry_vc0_dyn  : 6;
		mmr_t	reserved_0     : 2;
		mmr_t	entry_vc0_cap  : 6;
		mmr_t	reserved_1     : 2;
		mmr_t	entry_vc2_dyn  : 6;
		mmr_t	reserved_2     : 2;
		mmr_t	entry_vc2_cap  : 6;
		mmr_t	reserved_3     : 2;
		mmr_t	entry_vc0_test : 5;
		mmr_t	reserved_4     : 3;
		mmr_t	entry_vc2_test : 5;
		mmr_t	reserved_5     : 19;
	} sh_xnpi_fr_ni1_port_flow_fifo_s;
} sh_xnpi_fr_ni1_port_flow_fifo_u_t;
#else
typedef union sh_xnpi_fr_ni1_port_flow_fifo_u {
	mmr_t	sh_xnpi_fr_ni1_port_flow_fifo_regval;
	struct {
		mmr_t	reserved_5     : 19;
		mmr_t	entry_vc2_test : 5;
		mmr_t	reserved_4     : 3;
		mmr_t	entry_vc0_test : 5;
		mmr_t	reserved_3     : 2;
		mmr_t	entry_vc2_cap  : 6;
		mmr_t	reserved_2     : 2;
		mmr_t	entry_vc2_dyn  : 6;
		mmr_t	reserved_1     : 2;
		mmr_t	entry_vc0_cap  : 6;
		mmr_t	reserved_0     : 2;
		mmr_t	entry_vc0_dyn  : 6;
	} sh_xnpi_fr_ni1_port_flow_fifo_s;
} sh_xnpi_fr_ni1_port_flow_fifo_u_t;
#endif

/* ==================================================================== */
/*              Register "SH_XNPI_FR_IILB_PORT_FLOW_FIFO"               */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnpi_fr_iilb_port_flow_fifo_u {
	mmr_t	sh_xnpi_fr_iilb_port_flow_fifo_regval;
	struct {
		mmr_t	entry_vc0_dyn  : 6;
		mmr_t	reserved_0     : 2;
		mmr_t	entry_vc0_cap  : 6;
		mmr_t	reserved_1     : 2;
		mmr_t	entry_vc2_dyn  : 6;
		mmr_t	reserved_2     : 2;
		mmr_t	entry_vc2_cap  : 6;
		mmr_t	reserved_3     : 2;
		mmr_t	entry_vc0_test : 5;
		mmr_t	reserved_4     : 3;
		mmr_t	entry_vc2_test : 5;
		mmr_t	reserved_5     : 19;
	} sh_xnpi_fr_iilb_port_flow_fifo_s;
} sh_xnpi_fr_iilb_port_flow_fifo_u_t;
#else
typedef union sh_xnpi_fr_iilb_port_flow_fifo_u {
	mmr_t	sh_xnpi_fr_iilb_port_flow_fifo_regval;
	struct {
		mmr_t	reserved_5     : 19;
		mmr_t	entry_vc2_test : 5;
		mmr_t	reserved_4     : 3;
		mmr_t	entry_vc0_test : 5;
		mmr_t	reserved_3     : 2;
		mmr_t	entry_vc2_cap  : 6;
		mmr_t	reserved_2     : 2;
		mmr_t	entry_vc2_dyn  : 6;
		mmr_t	reserved_1     : 2;
		mmr_t	entry_vc0_cap  : 6;
		mmr_t	reserved_0     : 2;
		mmr_t	entry_vc0_dyn  : 6;
	} sh_xnpi_fr_iilb_port_flow_fifo_s;
} sh_xnpi_fr_iilb_port_flow_fifo_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_XNMD_SIC_FLOW"                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnmd_sic_flow_u {
	mmr_t	sh_xnmd_sic_flow_regval;
	struct {
		mmr_t	debit_vc0_withhold   : 5;
		mmr_t	reserved_0           : 2;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	debit_vc2_withhold   : 5;
		mmr_t	reserved_1           : 2;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	credit_vc0_test      : 5;
		mmr_t	reserved_2           : 3;
		mmr_t	credit_vc0_dyn       : 5;
		mmr_t	reserved_3           : 3;
		mmr_t	credit_vc0_cap       : 5;
		mmr_t	reserved_4           : 3;
		mmr_t	credit_vc2_test      : 5;
		mmr_t	reserved_5           : 3;
		mmr_t	credit_vc2_dyn       : 5;
		mmr_t	reserved_6           : 3;
		mmr_t	credit_vc2_cap       : 5;
		mmr_t	reserved_7           : 2;
		mmr_t	disable_bypass_out   : 1;
	} sh_xnmd_sic_flow_s;
} sh_xnmd_sic_flow_u_t;
#else
typedef union sh_xnmd_sic_flow_u {
	mmr_t	sh_xnmd_sic_flow_regval;
	struct {
		mmr_t	disable_bypass_out   : 1;
		mmr_t	reserved_7           : 2;
		mmr_t	credit_vc2_cap       : 5;
		mmr_t	reserved_6           : 3;
		mmr_t	credit_vc2_dyn       : 5;
		mmr_t	reserved_5           : 3;
		mmr_t	credit_vc2_test      : 5;
		mmr_t	reserved_4           : 3;
		mmr_t	credit_vc0_cap       : 5;
		mmr_t	reserved_3           : 3;
		mmr_t	credit_vc0_dyn       : 5;
		mmr_t	reserved_2           : 3;
		mmr_t	credit_vc0_test      : 5;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	reserved_1           : 2;
		mmr_t	debit_vc2_withhold   : 5;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	reserved_0           : 2;
		mmr_t	debit_vc0_withhold   : 5;
	} sh_xnmd_sic_flow_s;
} sh_xnmd_sic_flow_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XNMD_TO_NI0_PORT_FLOW"                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnmd_to_ni0_port_flow_u {
	mmr_t	sh_xnmd_to_ni0_port_flow_regval;
	struct {
		mmr_t	debit_vc0_withhold   : 6;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	debit_vc2_withhold   : 6;
		mmr_t	reserved_1           : 1;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	reserved_2           : 8;
		mmr_t	credit_vc0_dyn       : 6;
		mmr_t	reserved_3           : 2;
		mmr_t	credit_vc0_cap       : 6;
		mmr_t	reserved_4           : 10;
		mmr_t	credit_vc2_dyn       : 6;
		mmr_t	reserved_5           : 2;
		mmr_t	credit_vc2_cap       : 6;
		mmr_t	reserved_6           : 2;
	} sh_xnmd_to_ni0_port_flow_s;
} sh_xnmd_to_ni0_port_flow_u_t;
#else
typedef union sh_xnmd_to_ni0_port_flow_u {
	mmr_t	sh_xnmd_to_ni0_port_flow_regval;
	struct {
		mmr_t	reserved_6           : 2;
		mmr_t	credit_vc2_cap       : 6;
		mmr_t	reserved_5           : 2;
		mmr_t	credit_vc2_dyn       : 6;
		mmr_t	reserved_4           : 10;
		mmr_t	credit_vc0_cap       : 6;
		mmr_t	reserved_3           : 2;
		mmr_t	credit_vc0_dyn       : 6;
		mmr_t	reserved_2           : 8;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	reserved_1           : 1;
		mmr_t	debit_vc2_withhold   : 6;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc0_withhold   : 6;
	} sh_xnmd_to_ni0_port_flow_s;
} sh_xnmd_to_ni0_port_flow_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XNMD_TO_NI1_PORT_FLOW"                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnmd_to_ni1_port_flow_u {
	mmr_t	sh_xnmd_to_ni1_port_flow_regval;
	struct {
		mmr_t	debit_vc0_withhold   : 6;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	debit_vc2_withhold   : 6;
		mmr_t	reserved_1           : 1;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	reserved_2           : 8;
		mmr_t	credit_vc0_dyn       : 6;
		mmr_t	reserved_3           : 2;
		mmr_t	credit_vc0_cap       : 6;
		mmr_t	reserved_4           : 10;
		mmr_t	credit_vc2_dyn       : 6;
		mmr_t	reserved_5           : 2;
		mmr_t	credit_vc2_cap       : 6;
		mmr_t	reserved_6           : 2;
	} sh_xnmd_to_ni1_port_flow_s;
} sh_xnmd_to_ni1_port_flow_u_t;
#else
typedef union sh_xnmd_to_ni1_port_flow_u {
	mmr_t	sh_xnmd_to_ni1_port_flow_regval;
	struct {
		mmr_t	reserved_6           : 2;
		mmr_t	credit_vc2_cap       : 6;
		mmr_t	reserved_5           : 2;
		mmr_t	credit_vc2_dyn       : 6;
		mmr_t	reserved_4           : 10;
		mmr_t	credit_vc0_cap       : 6;
		mmr_t	reserved_3           : 2;
		mmr_t	credit_vc0_dyn       : 6;
		mmr_t	reserved_2           : 8;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	reserved_1           : 1;
		mmr_t	debit_vc2_withhold   : 6;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc0_withhold   : 6;
	} sh_xnmd_to_ni1_port_flow_s;
} sh_xnmd_to_ni1_port_flow_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XNMD_TO_IILB_PORT_FLOW"                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnmd_to_iilb_port_flow_u {
	mmr_t	sh_xnmd_to_iilb_port_flow_regval;
	struct {
		mmr_t	debit_vc0_withhold   : 6;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	debit_vc2_withhold   : 6;
		mmr_t	reserved_1           : 1;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	reserved_2           : 8;
		mmr_t	credit_vc0_dyn       : 6;
		mmr_t	reserved_3           : 2;
		mmr_t	credit_vc0_cap       : 6;
		mmr_t	reserved_4           : 10;
		mmr_t	credit_vc2_dyn       : 6;
		mmr_t	reserved_5           : 2;
		mmr_t	credit_vc2_cap       : 6;
		mmr_t	reserved_6           : 2;
	} sh_xnmd_to_iilb_port_flow_s;
} sh_xnmd_to_iilb_port_flow_u_t;
#else
typedef union sh_xnmd_to_iilb_port_flow_u {
	mmr_t	sh_xnmd_to_iilb_port_flow_regval;
	struct {
		mmr_t	reserved_6           : 2;
		mmr_t	credit_vc2_cap       : 6;
		mmr_t	reserved_5           : 2;
		mmr_t	credit_vc2_dyn       : 6;
		mmr_t	reserved_4           : 10;
		mmr_t	credit_vc0_cap       : 6;
		mmr_t	reserved_3           : 2;
		mmr_t	credit_vc0_dyn       : 6;
		mmr_t	reserved_2           : 8;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	reserved_1           : 1;
		mmr_t	debit_vc2_withhold   : 6;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc0_withhold   : 6;
	} sh_xnmd_to_iilb_port_flow_s;
} sh_xnmd_to_iilb_port_flow_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_XNMD_FR_NI0_PORT_FLOW_FIFO"               */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnmd_fr_ni0_port_flow_fifo_u {
	mmr_t	sh_xnmd_fr_ni0_port_flow_fifo_regval;
	struct {
		mmr_t	entry_vc0_dyn  : 6;
		mmr_t	reserved_0     : 2;
		mmr_t	entry_vc0_cap  : 6;
		mmr_t	reserved_1     : 2;
		mmr_t	entry_vc2_dyn  : 6;
		mmr_t	reserved_2     : 2;
		mmr_t	entry_vc2_cap  : 6;
		mmr_t	reserved_3     : 2;
		mmr_t	entry_vc0_test : 5;
		mmr_t	reserved_4     : 3;
		mmr_t	entry_vc2_test : 5;
		mmr_t	reserved_5     : 19;
	} sh_xnmd_fr_ni0_port_flow_fifo_s;
} sh_xnmd_fr_ni0_port_flow_fifo_u_t;
#else
typedef union sh_xnmd_fr_ni0_port_flow_fifo_u {
	mmr_t	sh_xnmd_fr_ni0_port_flow_fifo_regval;
	struct {
		mmr_t	reserved_5     : 19;
		mmr_t	entry_vc2_test : 5;
		mmr_t	reserved_4     : 3;
		mmr_t	entry_vc0_test : 5;
		mmr_t	reserved_3     : 2;
		mmr_t	entry_vc2_cap  : 6;
		mmr_t	reserved_2     : 2;
		mmr_t	entry_vc2_dyn  : 6;
		mmr_t	reserved_1     : 2;
		mmr_t	entry_vc0_cap  : 6;
		mmr_t	reserved_0     : 2;
		mmr_t	entry_vc0_dyn  : 6;
	} sh_xnmd_fr_ni0_port_flow_fifo_s;
} sh_xnmd_fr_ni0_port_flow_fifo_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_XNMD_FR_NI1_PORT_FLOW_FIFO"               */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnmd_fr_ni1_port_flow_fifo_u {
	mmr_t	sh_xnmd_fr_ni1_port_flow_fifo_regval;
	struct {
		mmr_t	entry_vc0_dyn  : 6;
		mmr_t	reserved_0     : 2;
		mmr_t	entry_vc0_cap  : 6;
		mmr_t	reserved_1     : 2;
		mmr_t	entry_vc2_dyn  : 6;
		mmr_t	reserved_2     : 2;
		mmr_t	entry_vc2_cap  : 6;
		mmr_t	reserved_3     : 2;
		mmr_t	entry_vc0_test : 5;
		mmr_t	reserved_4     : 3;
		mmr_t	entry_vc2_test : 5;
		mmr_t	reserved_5     : 19;
	} sh_xnmd_fr_ni1_port_flow_fifo_s;
} sh_xnmd_fr_ni1_port_flow_fifo_u_t;
#else
typedef union sh_xnmd_fr_ni1_port_flow_fifo_u {
	mmr_t	sh_xnmd_fr_ni1_port_flow_fifo_regval;
	struct {
		mmr_t	reserved_5     : 19;
		mmr_t	entry_vc2_test : 5;
		mmr_t	reserved_4     : 3;
		mmr_t	entry_vc0_test : 5;
		mmr_t	reserved_3     : 2;
		mmr_t	entry_vc2_cap  : 6;
		mmr_t	reserved_2     : 2;
		mmr_t	entry_vc2_dyn  : 6;
		mmr_t	reserved_1     : 2;
		mmr_t	entry_vc0_cap  : 6;
		mmr_t	reserved_0     : 2;
		mmr_t	entry_vc0_dyn  : 6;
	} sh_xnmd_fr_ni1_port_flow_fifo_s;
} sh_xnmd_fr_ni1_port_flow_fifo_u_t;
#endif

/* ==================================================================== */
/*              Register "SH_XNMD_FR_IILB_PORT_FLOW_FIFO"               */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnmd_fr_iilb_port_flow_fifo_u {
	mmr_t	sh_xnmd_fr_iilb_port_flow_fifo_regval;
	struct {
		mmr_t	entry_vc0_dyn  : 6;
		mmr_t	reserved_0     : 2;
		mmr_t	entry_vc0_cap  : 6;
		mmr_t	reserved_1     : 2;
		mmr_t	entry_vc2_dyn  : 6;
		mmr_t	reserved_2     : 2;
		mmr_t	entry_vc2_cap  : 6;
		mmr_t	reserved_3     : 2;
		mmr_t	entry_vc0_test : 5;
		mmr_t	reserved_4     : 3;
		mmr_t	entry_vc2_test : 5;
		mmr_t	reserved_5     : 19;
	} sh_xnmd_fr_iilb_port_flow_fifo_s;
} sh_xnmd_fr_iilb_port_flow_fifo_u_t;
#else
typedef union sh_xnmd_fr_iilb_port_flow_fifo_u {
	mmr_t	sh_xnmd_fr_iilb_port_flow_fifo_regval;
	struct {
		mmr_t	reserved_5     : 19;
		mmr_t	entry_vc2_test : 5;
		mmr_t	reserved_4     : 3;
		mmr_t	entry_vc0_test : 5;
		mmr_t	reserved_3     : 2;
		mmr_t	entry_vc2_cap  : 6;
		mmr_t	reserved_2     : 2;
		mmr_t	entry_vc2_dyn  : 6;
		mmr_t	reserved_1     : 2;
		mmr_t	entry_vc0_cap  : 6;
		mmr_t	reserved_0     : 2;
		mmr_t	entry_vc0_dyn  : 6;
	} sh_xnmd_fr_iilb_port_flow_fifo_s;
} sh_xnmd_fr_iilb_port_flow_fifo_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_XNII_INTRA_FLOW"                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnii_intra_flow_u {
	mmr_t	sh_xnii_intra_flow_regval;
	struct {
		mmr_t	debit_vc0_withhold   : 6;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	debit_vc2_withhold   : 6;
		mmr_t	reserved_1           : 1;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	credit_vc0_test      : 7;
		mmr_t	reserved_2           : 1;
		mmr_t	credit_vc0_dyn       : 7;
		mmr_t	reserved_3           : 1;
		mmr_t	credit_vc0_cap       : 7;
		mmr_t	reserved_4           : 1;
		mmr_t	credit_vc2_test      : 7;
		mmr_t	reserved_5           : 1;
		mmr_t	credit_vc2_dyn       : 7;
		mmr_t	reserved_6           : 1;
		mmr_t	credit_vc2_cap       : 7;
		mmr_t	reserved_7           : 1;
	} sh_xnii_intra_flow_s;
} sh_xnii_intra_flow_u_t;
#else
typedef union sh_xnii_intra_flow_u {
	mmr_t	sh_xnii_intra_flow_regval;
	struct {
		mmr_t	reserved_7           : 1;
		mmr_t	credit_vc2_cap       : 7;
		mmr_t	reserved_6           : 1;
		mmr_t	credit_vc2_dyn       : 7;
		mmr_t	reserved_5           : 1;
		mmr_t	credit_vc2_test      : 7;
		mmr_t	reserved_4           : 1;
		mmr_t	credit_vc0_cap       : 7;
		mmr_t	reserved_3           : 1;
		mmr_t	credit_vc0_dyn       : 7;
		mmr_t	reserved_2           : 1;
		mmr_t	credit_vc0_test      : 7;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	reserved_1           : 1;
		mmr_t	debit_vc2_withhold   : 6;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc0_withhold   : 6;
	} sh_xnii_intra_flow_s;
} sh_xnii_intra_flow_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_XNLB_INTRA_FLOW"                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnlb_intra_flow_u {
	mmr_t	sh_xnlb_intra_flow_regval;
	struct {
		mmr_t	debit_vc0_withhold   : 6;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	debit_vc2_withhold   : 6;
		mmr_t	reserved_1           : 1;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	credit_vc0_test      : 7;
		mmr_t	reserved_2           : 1;
		mmr_t	credit_vc0_dyn       : 7;
		mmr_t	reserved_3           : 1;
		mmr_t	credit_vc0_cap       : 7;
		mmr_t	reserved_4           : 1;
		mmr_t	credit_vc2_test      : 7;
		mmr_t	reserved_5           : 1;
		mmr_t	credit_vc2_dyn       : 7;
		mmr_t	reserved_6           : 1;
		mmr_t	credit_vc2_cap       : 7;
		mmr_t	disable_bypass_in    : 1;
	} sh_xnlb_intra_flow_s;
} sh_xnlb_intra_flow_u_t;
#else
typedef union sh_xnlb_intra_flow_u {
	mmr_t	sh_xnlb_intra_flow_regval;
	struct {
		mmr_t	disable_bypass_in    : 1;
		mmr_t	credit_vc2_cap       : 7;
		mmr_t	reserved_6           : 1;
		mmr_t	credit_vc2_dyn       : 7;
		mmr_t	reserved_5           : 1;
		mmr_t	credit_vc2_test      : 7;
		mmr_t	reserved_4           : 1;
		mmr_t	credit_vc0_cap       : 7;
		mmr_t	reserved_3           : 1;
		mmr_t	credit_vc0_dyn       : 7;
		mmr_t	reserved_2           : 1;
		mmr_t	credit_vc0_test      : 7;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	reserved_1           : 1;
		mmr_t	debit_vc2_withhold   : 6;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc0_withhold   : 6;
	} sh_xnlb_intra_flow_s;
} sh_xnlb_intra_flow_u_t;
#endif

/* ==================================================================== */
/*             Register "SH_XNIILB_TO_NI0_INTRA_FLOW_DEBIT"             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xniilb_to_ni0_intra_flow_debit_u {
	mmr_t	sh_xniilb_to_ni0_intra_flow_debit_regval;
	struct {
		mmr_t	vc0_withhold   : 6;
		mmr_t	reserved_0     : 1;
		mmr_t	vc0_force_cred : 1;
		mmr_t	vc2_withhold   : 6;
		mmr_t	reserved_1     : 1;
		mmr_t	vc2_force_cred : 1;
		mmr_t	reserved_2     : 8;
		mmr_t	vc0_dyn        : 7;
		mmr_t	reserved_3     : 1;
		mmr_t	vc0_cap        : 7;
		mmr_t	reserved_4     : 9;
		mmr_t	vc2_dyn        : 7;
		mmr_t	reserved_5     : 1;
		mmr_t	vc2_cap        : 7;
		mmr_t	reserved_6     : 1;
	} sh_xniilb_to_ni0_intra_flow_debit_s;
} sh_xniilb_to_ni0_intra_flow_debit_u_t;
#else
typedef union sh_xniilb_to_ni0_intra_flow_debit_u {
	mmr_t	sh_xniilb_to_ni0_intra_flow_debit_regval;
	struct {
		mmr_t	reserved_6     : 1;
		mmr_t	vc2_cap        : 7;
		mmr_t	reserved_5     : 1;
		mmr_t	vc2_dyn        : 7;
		mmr_t	reserved_4     : 9;
		mmr_t	vc0_cap        : 7;
		mmr_t	reserved_3     : 1;
		mmr_t	vc0_dyn        : 7;
		mmr_t	reserved_2     : 8;
		mmr_t	vc2_force_cred : 1;
		mmr_t	reserved_1     : 1;
		mmr_t	vc2_withhold   : 6;
		mmr_t	vc0_force_cred : 1;
		mmr_t	reserved_0     : 1;
		mmr_t	vc0_withhold   : 6;
	} sh_xniilb_to_ni0_intra_flow_debit_s;
} sh_xniilb_to_ni0_intra_flow_debit_u_t;
#endif

/* ==================================================================== */
/*             Register "SH_XNIILB_TO_NI1_INTRA_FLOW_DEBIT"             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xniilb_to_ni1_intra_flow_debit_u {
	mmr_t	sh_xniilb_to_ni1_intra_flow_debit_regval;
	struct {
		mmr_t	vc0_withhold   : 6;
		mmr_t	reserved_0     : 1;
		mmr_t	vc0_force_cred : 1;
		mmr_t	vc2_withhold   : 6;
		mmr_t	reserved_1     : 1;
		mmr_t	vc2_force_cred : 1;
		mmr_t	reserved_2     : 8;
		mmr_t	vc0_dyn        : 7;
		mmr_t	reserved_3     : 1;
		mmr_t	vc0_cap        : 7;
		mmr_t	reserved_4     : 9;
		mmr_t	vc2_dyn        : 7;
		mmr_t	reserved_5     : 1;
		mmr_t	vc2_cap        : 7;
		mmr_t	reserved_6     : 1;
	} sh_xniilb_to_ni1_intra_flow_debit_s;
} sh_xniilb_to_ni1_intra_flow_debit_u_t;
#else
typedef union sh_xniilb_to_ni1_intra_flow_debit_u {
	mmr_t	sh_xniilb_to_ni1_intra_flow_debit_regval;
	struct {
		mmr_t	reserved_6     : 1;
		mmr_t	vc2_cap        : 7;
		mmr_t	reserved_5     : 1;
		mmr_t	vc2_dyn        : 7;
		mmr_t	reserved_4     : 9;
		mmr_t	vc0_cap        : 7;
		mmr_t	reserved_3     : 1;
		mmr_t	vc0_dyn        : 7;
		mmr_t	reserved_2     : 8;
		mmr_t	vc2_force_cred : 1;
		mmr_t	reserved_1     : 1;
		mmr_t	vc2_withhold   : 6;
		mmr_t	vc0_force_cred : 1;
		mmr_t	reserved_0     : 1;
		mmr_t	vc0_withhold   : 6;
	} sh_xniilb_to_ni1_intra_flow_debit_s;
} sh_xniilb_to_ni1_intra_flow_debit_u_t;
#endif

/* ==================================================================== */
/*             Register "SH_XNIILB_TO_MD_INTRA_FLOW_DEBIT"              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xniilb_to_md_intra_flow_debit_u {
	mmr_t	sh_xniilb_to_md_intra_flow_debit_regval;
	struct {
		mmr_t	vc0_withhold   : 6;
		mmr_t	reserved_0     : 1;
		mmr_t	vc0_force_cred : 1;
		mmr_t	vc2_withhold   : 6;
		mmr_t	reserved_1     : 1;
		mmr_t	vc2_force_cred : 1;
		mmr_t	reserved_2     : 8;
		mmr_t	vc0_dyn        : 7;
		mmr_t	reserved_3     : 1;
		mmr_t	vc0_cap        : 7;
		mmr_t	reserved_4     : 9;
		mmr_t	vc2_dyn        : 7;
		mmr_t	reserved_5     : 1;
		mmr_t	vc2_cap        : 7;
		mmr_t	reserved_6     : 1;
	} sh_xniilb_to_md_intra_flow_debit_s;
} sh_xniilb_to_md_intra_flow_debit_u_t;
#else
typedef union sh_xniilb_to_md_intra_flow_debit_u {
	mmr_t	sh_xniilb_to_md_intra_flow_debit_regval;
	struct {
		mmr_t	reserved_6     : 1;
		mmr_t	vc2_cap        : 7;
		mmr_t	reserved_5     : 1;
		mmr_t	vc2_dyn        : 7;
		mmr_t	reserved_4     : 9;
		mmr_t	vc0_cap        : 7;
		mmr_t	reserved_3     : 1;
		mmr_t	vc0_dyn        : 7;
		mmr_t	reserved_2     : 8;
		mmr_t	vc2_force_cred : 1;
		mmr_t	reserved_1     : 1;
		mmr_t	vc2_withhold   : 6;
		mmr_t	vc0_force_cred : 1;
		mmr_t	reserved_0     : 1;
		mmr_t	vc0_withhold   : 6;
	} sh_xniilb_to_md_intra_flow_debit_s;
} sh_xniilb_to_md_intra_flow_debit_u_t;
#endif

/* ==================================================================== */
/*            Register "SH_XNIILB_TO_IILB_INTRA_FLOW_DEBIT"             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xniilb_to_iilb_intra_flow_debit_u {
	mmr_t	sh_xniilb_to_iilb_intra_flow_debit_regval;
	struct {
		mmr_t	vc0_withhold   : 6;
		mmr_t	reserved_0     : 1;
		mmr_t	vc0_force_cred : 1;
		mmr_t	vc2_withhold   : 6;
		mmr_t	reserved_1     : 1;
		mmr_t	vc2_force_cred : 1;
		mmr_t	reserved_2     : 8;
		mmr_t	vc0_dyn        : 7;
		mmr_t	reserved_3     : 1;
		mmr_t	vc0_cap        : 7;
		mmr_t	reserved_4     : 9;
		mmr_t	vc2_dyn        : 7;
		mmr_t	reserved_5     : 1;
		mmr_t	vc2_cap        : 7;
		mmr_t	reserved_6     : 1;
	} sh_xniilb_to_iilb_intra_flow_debit_s;
} sh_xniilb_to_iilb_intra_flow_debit_u_t;
#else
typedef union sh_xniilb_to_iilb_intra_flow_debit_u {
	mmr_t	sh_xniilb_to_iilb_intra_flow_debit_regval;
	struct {
		mmr_t	reserved_6     : 1;
		mmr_t	vc2_cap        : 7;
		mmr_t	reserved_5     : 1;
		mmr_t	vc2_dyn        : 7;
		mmr_t	reserved_4     : 9;
		mmr_t	vc0_cap        : 7;
		mmr_t	reserved_3     : 1;
		mmr_t	vc0_dyn        : 7;
		mmr_t	reserved_2     : 8;
		mmr_t	vc2_force_cred : 1;
		mmr_t	reserved_1     : 1;
		mmr_t	vc2_withhold   : 6;
		mmr_t	vc0_force_cred : 1;
		mmr_t	reserved_0     : 1;
		mmr_t	vc0_withhold   : 6;
	} sh_xniilb_to_iilb_intra_flow_debit_s;
} sh_xniilb_to_iilb_intra_flow_debit_u_t;
#endif

/* ==================================================================== */
/*             Register "SH_XNIILB_TO_PI_INTRA_FLOW_DEBIT"              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xniilb_to_pi_intra_flow_debit_u {
	mmr_t	sh_xniilb_to_pi_intra_flow_debit_regval;
	struct {
		mmr_t	vc0_withhold   : 6;
		mmr_t	reserved_0     : 1;
		mmr_t	vc0_force_cred : 1;
		mmr_t	vc2_withhold   : 6;
		mmr_t	reserved_1     : 1;
		mmr_t	vc2_force_cred : 1;
		mmr_t	reserved_2     : 8;
		mmr_t	vc0_dyn        : 7;
		mmr_t	reserved_3     : 1;
		mmr_t	vc0_cap        : 7;
		mmr_t	reserved_4     : 9;
		mmr_t	vc2_dyn        : 7;
		mmr_t	reserved_5     : 1;
		mmr_t	vc2_cap        : 7;
		mmr_t	reserved_6     : 1;
	} sh_xniilb_to_pi_intra_flow_debit_s;
} sh_xniilb_to_pi_intra_flow_debit_u_t;
#else
typedef union sh_xniilb_to_pi_intra_flow_debit_u {
	mmr_t	sh_xniilb_to_pi_intra_flow_debit_regval;
	struct {
		mmr_t	reserved_6     : 1;
		mmr_t	vc2_cap        : 7;
		mmr_t	reserved_5     : 1;
		mmr_t	vc2_dyn        : 7;
		mmr_t	reserved_4     : 9;
		mmr_t	vc0_cap        : 7;
		mmr_t	reserved_3     : 1;
		mmr_t	vc0_dyn        : 7;
		mmr_t	reserved_2     : 8;
		mmr_t	vc2_force_cred : 1;
		mmr_t	reserved_1     : 1;
		mmr_t	vc2_withhold   : 6;
		mmr_t	vc0_force_cred : 1;
		mmr_t	reserved_0     : 1;
		mmr_t	vc0_withhold   : 6;
	} sh_xniilb_to_pi_intra_flow_debit_s;
} sh_xniilb_to_pi_intra_flow_debit_u_t;
#endif

/* ==================================================================== */
/*            Register "SH_XNIILB_FR_NI0_INTRA_FLOW_CREDIT"             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xniilb_fr_ni0_intra_flow_credit_u {
	mmr_t	sh_xniilb_fr_ni0_intra_flow_credit_regval;
	struct {
		mmr_t	vc0_test    : 7;
		mmr_t	reserved_0  : 1;
		mmr_t	vc0_dyn     : 7;
		mmr_t	reserved_1  : 1;
		mmr_t	vc0_cap     : 7;
		mmr_t	reserved_2  : 1;
		mmr_t	vc2_test    : 7;
		mmr_t	reserved_3  : 1;
		mmr_t	vc2_dyn     : 7;
		mmr_t	reserved_4  : 1;
		mmr_t	vc2_cap     : 7;
		mmr_t	reserved_5  : 17;
	} sh_xniilb_fr_ni0_intra_flow_credit_s;
} sh_xniilb_fr_ni0_intra_flow_credit_u_t;
#else
typedef union sh_xniilb_fr_ni0_intra_flow_credit_u {
	mmr_t	sh_xniilb_fr_ni0_intra_flow_credit_regval;
	struct {
		mmr_t	reserved_5  : 17;
		mmr_t	vc2_cap     : 7;
		mmr_t	reserved_4  : 1;
		mmr_t	vc2_dyn     : 7;
		mmr_t	reserved_3  : 1;
		mmr_t	vc2_test    : 7;
		mmr_t	reserved_2  : 1;
		mmr_t	vc0_cap     : 7;
		mmr_t	reserved_1  : 1;
		mmr_t	vc0_dyn     : 7;
		mmr_t	reserved_0  : 1;
		mmr_t	vc0_test    : 7;
	} sh_xniilb_fr_ni0_intra_flow_credit_s;
} sh_xniilb_fr_ni0_intra_flow_credit_u_t;
#endif

/* ==================================================================== */
/*            Register "SH_XNIILB_FR_NI1_INTRA_FLOW_CREDIT"             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xniilb_fr_ni1_intra_flow_credit_u {
	mmr_t	sh_xniilb_fr_ni1_intra_flow_credit_regval;
	struct {
		mmr_t	vc0_test    : 7;
		mmr_t	reserved_0  : 1;
		mmr_t	vc0_dyn     : 7;
		mmr_t	reserved_1  : 1;
		mmr_t	vc0_cap     : 7;
		mmr_t	reserved_2  : 1;
		mmr_t	vc2_test    : 7;
		mmr_t	reserved_3  : 1;
		mmr_t	vc2_dyn     : 7;
		mmr_t	reserved_4  : 1;
		mmr_t	vc2_cap     : 7;
		mmr_t	reserved_5  : 17;
	} sh_xniilb_fr_ni1_intra_flow_credit_s;
} sh_xniilb_fr_ni1_intra_flow_credit_u_t;
#else
typedef union sh_xniilb_fr_ni1_intra_flow_credit_u {
	mmr_t	sh_xniilb_fr_ni1_intra_flow_credit_regval;
	struct {
		mmr_t	reserved_5  : 17;
		mmr_t	vc2_cap     : 7;
		mmr_t	reserved_4  : 1;
		mmr_t	vc2_dyn     : 7;
		mmr_t	reserved_3  : 1;
		mmr_t	vc2_test    : 7;
		mmr_t	reserved_2  : 1;
		mmr_t	vc0_cap     : 7;
		mmr_t	reserved_1  : 1;
		mmr_t	vc0_dyn     : 7;
		mmr_t	reserved_0  : 1;
		mmr_t	vc0_test    : 7;
	} sh_xniilb_fr_ni1_intra_flow_credit_s;
} sh_xniilb_fr_ni1_intra_flow_credit_u_t;
#endif

/* ==================================================================== */
/*             Register "SH_XNIILB_FR_MD_INTRA_FLOW_CREDIT"             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xniilb_fr_md_intra_flow_credit_u {
	mmr_t	sh_xniilb_fr_md_intra_flow_credit_regval;
	struct {
		mmr_t	vc0_test    : 7;
		mmr_t	reserved_0  : 1;
		mmr_t	vc0_dyn     : 7;
		mmr_t	reserved_1  : 1;
		mmr_t	vc0_cap     : 7;
		mmr_t	reserved_2  : 1;
		mmr_t	vc2_test    : 7;
		mmr_t	reserved_3  : 1;
		mmr_t	vc2_dyn     : 7;
		mmr_t	reserved_4  : 1;
		mmr_t	vc2_cap     : 7;
		mmr_t	reserved_5  : 17;
	} sh_xniilb_fr_md_intra_flow_credit_s;
} sh_xniilb_fr_md_intra_flow_credit_u_t;
#else
typedef union sh_xniilb_fr_md_intra_flow_credit_u {
	mmr_t	sh_xniilb_fr_md_intra_flow_credit_regval;
	struct {
		mmr_t	reserved_5  : 17;
		mmr_t	vc2_cap     : 7;
		mmr_t	reserved_4  : 1;
		mmr_t	vc2_dyn     : 7;
		mmr_t	reserved_3  : 1;
		mmr_t	vc2_test    : 7;
		mmr_t	reserved_2  : 1;
		mmr_t	vc0_cap     : 7;
		mmr_t	reserved_1  : 1;
		mmr_t	vc0_dyn     : 7;
		mmr_t	reserved_0  : 1;
		mmr_t	vc0_test    : 7;
	} sh_xniilb_fr_md_intra_flow_credit_s;
} sh_xniilb_fr_md_intra_flow_credit_u_t;
#endif

/* ==================================================================== */
/*            Register "SH_XNIILB_FR_IILB_INTRA_FLOW_CREDIT"            */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xniilb_fr_iilb_intra_flow_credit_u {
	mmr_t	sh_xniilb_fr_iilb_intra_flow_credit_regval;
	struct {
		mmr_t	vc0_test    : 7;
		mmr_t	reserved_0  : 1;
		mmr_t	vc0_dyn     : 7;
		mmr_t	reserved_1  : 1;
		mmr_t	vc0_cap     : 7;
		mmr_t	reserved_2  : 1;
		mmr_t	vc2_test    : 7;
		mmr_t	reserved_3  : 1;
		mmr_t	vc2_dyn     : 7;
		mmr_t	reserved_4  : 1;
		mmr_t	vc2_cap     : 7;
		mmr_t	reserved_5  : 17;
	} sh_xniilb_fr_iilb_intra_flow_credit_s;
} sh_xniilb_fr_iilb_intra_flow_credit_u_t;
#else
typedef union sh_xniilb_fr_iilb_intra_flow_credit_u {
	mmr_t	sh_xniilb_fr_iilb_intra_flow_credit_regval;
	struct {
		mmr_t	reserved_5  : 17;
		mmr_t	vc2_cap     : 7;
		mmr_t	reserved_4  : 1;
		mmr_t	vc2_dyn     : 7;
		mmr_t	reserved_3  : 1;
		mmr_t	vc2_test    : 7;
		mmr_t	reserved_2  : 1;
		mmr_t	vc0_cap     : 7;
		mmr_t	reserved_1  : 1;
		mmr_t	vc0_dyn     : 7;
		mmr_t	reserved_0  : 1;
		mmr_t	vc0_test    : 7;
	} sh_xniilb_fr_iilb_intra_flow_credit_s;
} sh_xniilb_fr_iilb_intra_flow_credit_u_t;
#endif

/* ==================================================================== */
/*             Register "SH_XNIILB_FR_PI_INTRA_FLOW_CREDIT"             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xniilb_fr_pi_intra_flow_credit_u {
	mmr_t	sh_xniilb_fr_pi_intra_flow_credit_regval;
	struct {
		mmr_t	vc0_test    : 7;
		mmr_t	reserved_0  : 1;
		mmr_t	vc0_dyn     : 7;
		mmr_t	reserved_1  : 1;
		mmr_t	vc0_cap     : 7;
		mmr_t	reserved_2  : 1;
		mmr_t	vc2_test    : 7;
		mmr_t	reserved_3  : 1;
		mmr_t	vc2_dyn     : 7;
		mmr_t	reserved_4  : 1;
		mmr_t	vc2_cap     : 7;
		mmr_t	reserved_5  : 17;
	} sh_xniilb_fr_pi_intra_flow_credit_s;
} sh_xniilb_fr_pi_intra_flow_credit_u_t;
#else
typedef union sh_xniilb_fr_pi_intra_flow_credit_u {
	mmr_t	sh_xniilb_fr_pi_intra_flow_credit_regval;
	struct {
		mmr_t	reserved_5  : 17;
		mmr_t	vc2_cap     : 7;
		mmr_t	reserved_4  : 1;
		mmr_t	vc2_dyn     : 7;
		mmr_t	reserved_3  : 1;
		mmr_t	vc2_test    : 7;
		mmr_t	reserved_2  : 1;
		mmr_t	vc0_cap     : 7;
		mmr_t	reserved_1  : 1;
		mmr_t	vc0_dyn     : 7;
		mmr_t	reserved_0  : 1;
		mmr_t	vc0_test    : 7;
	} sh_xniilb_fr_pi_intra_flow_credit_s;
} sh_xniilb_fr_pi_intra_flow_credit_u_t;
#endif

/* ==================================================================== */
/*              Register "SH_XNNI0_TO_PI_INTRA_FLOW_DEBIT"              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni0_to_pi_intra_flow_debit_u {
	mmr_t	sh_xnni0_to_pi_intra_flow_debit_regval;
	struct {
		mmr_t	vc0_withhold   : 6;
		mmr_t	reserved_0     : 1;
		mmr_t	vc0_force_cred : 1;
		mmr_t	vc2_withhold   : 6;
		mmr_t	reserved_1     : 1;
		mmr_t	vc2_force_cred : 1;
		mmr_t	reserved_2     : 8;
		mmr_t	vc0_dyn        : 7;
		mmr_t	reserved_3     : 1;
		mmr_t	vc0_cap        : 7;
		mmr_t	reserved_4     : 9;
		mmr_t	vc2_dyn        : 7;
		mmr_t	reserved_5     : 1;
		mmr_t	vc2_cap        : 7;
		mmr_t	reserved_6     : 1;
	} sh_xnni0_to_pi_intra_flow_debit_s;
} sh_xnni0_to_pi_intra_flow_debit_u_t;
#else
typedef union sh_xnni0_to_pi_intra_flow_debit_u {
	mmr_t	sh_xnni0_to_pi_intra_flow_debit_regval;
	struct {
		mmr_t	reserved_6     : 1;
		mmr_t	vc2_cap        : 7;
		mmr_t	reserved_5     : 1;
		mmr_t	vc2_dyn        : 7;
		mmr_t	reserved_4     : 9;
		mmr_t	vc0_cap        : 7;
		mmr_t	reserved_3     : 1;
		mmr_t	vc0_dyn        : 7;
		mmr_t	reserved_2     : 8;
		mmr_t	vc2_force_cred : 1;
		mmr_t	reserved_1     : 1;
		mmr_t	vc2_withhold   : 6;
		mmr_t	vc0_force_cred : 1;
		mmr_t	reserved_0     : 1;
		mmr_t	vc0_withhold   : 6;
	} sh_xnni0_to_pi_intra_flow_debit_s;
} sh_xnni0_to_pi_intra_flow_debit_u_t;
#endif

/* ==================================================================== */
/*              Register "SH_XNNI0_TO_MD_INTRA_FLOW_DEBIT"              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni0_to_md_intra_flow_debit_u {
	mmr_t	sh_xnni0_to_md_intra_flow_debit_regval;
	struct {
		mmr_t	vc0_withhold   : 6;
		mmr_t	reserved_0     : 1;
		mmr_t	vc0_force_cred : 1;
		mmr_t	vc2_withhold   : 6;
		mmr_t	reserved_1     : 1;
		mmr_t	vc2_force_cred : 1;
		mmr_t	reserved_2     : 8;
		mmr_t	vc0_dyn        : 7;
		mmr_t	reserved_3     : 1;
		mmr_t	vc0_cap        : 7;
		mmr_t	reserved_4     : 9;
		mmr_t	vc2_dyn        : 7;
		mmr_t	reserved_5     : 1;
		mmr_t	vc2_cap        : 7;
		mmr_t	reserved_6     : 1;
	} sh_xnni0_to_md_intra_flow_debit_s;
} sh_xnni0_to_md_intra_flow_debit_u_t;
#else
typedef union sh_xnni0_to_md_intra_flow_debit_u {
	mmr_t	sh_xnni0_to_md_intra_flow_debit_regval;
	struct {
		mmr_t	reserved_6     : 1;
		mmr_t	vc2_cap        : 7;
		mmr_t	reserved_5     : 1;
		mmr_t	vc2_dyn        : 7;
		mmr_t	reserved_4     : 9;
		mmr_t	vc0_cap        : 7;
		mmr_t	reserved_3     : 1;
		mmr_t	vc0_dyn        : 7;
		mmr_t	reserved_2     : 8;
		mmr_t	vc2_force_cred : 1;
		mmr_t	reserved_1     : 1;
		mmr_t	vc2_withhold   : 6;
		mmr_t	vc0_force_cred : 1;
		mmr_t	reserved_0     : 1;
		mmr_t	vc0_withhold   : 6;
	} sh_xnni0_to_md_intra_flow_debit_s;
} sh_xnni0_to_md_intra_flow_debit_u_t;
#endif

/* ==================================================================== */
/*             Register "SH_XNNI0_TO_IILB_INTRA_FLOW_DEBIT"             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni0_to_iilb_intra_flow_debit_u {
	mmr_t	sh_xnni0_to_iilb_intra_flow_debit_regval;
	struct {
		mmr_t	vc0_withhold   : 6;
		mmr_t	reserved_0     : 1;
		mmr_t	vc0_force_cred : 1;
		mmr_t	vc2_withhold   : 6;
		mmr_t	reserved_1     : 1;
		mmr_t	vc2_force_cred : 1;
		mmr_t	reserved_2     : 8;
		mmr_t	vc0_dyn        : 7;
		mmr_t	reserved_3     : 1;
		mmr_t	vc0_cap        : 7;
		mmr_t	reserved_4     : 9;
		mmr_t	vc2_dyn        : 7;
		mmr_t	reserved_5     : 1;
		mmr_t	vc2_cap        : 7;
		mmr_t	reserved_6     : 1;
	} sh_xnni0_to_iilb_intra_flow_debit_s;
} sh_xnni0_to_iilb_intra_flow_debit_u_t;
#else
typedef union sh_xnni0_to_iilb_intra_flow_debit_u {
	mmr_t	sh_xnni0_to_iilb_intra_flow_debit_regval;
	struct {
		mmr_t	reserved_6     : 1;
		mmr_t	vc2_cap        : 7;
		mmr_t	reserved_5     : 1;
		mmr_t	vc2_dyn        : 7;
		mmr_t	reserved_4     : 9;
		mmr_t	vc0_cap        : 7;
		mmr_t	reserved_3     : 1;
		mmr_t	vc0_dyn        : 7;
		mmr_t	reserved_2     : 8;
		mmr_t	vc2_force_cred : 1;
		mmr_t	reserved_1     : 1;
		mmr_t	vc2_withhold   : 6;
		mmr_t	vc0_force_cred : 1;
		mmr_t	reserved_0     : 1;
		mmr_t	vc0_withhold   : 6;
	} sh_xnni0_to_iilb_intra_flow_debit_s;
} sh_xnni0_to_iilb_intra_flow_debit_u_t;
#endif

/* ==================================================================== */
/*             Register "SH_XNNI0_FR_PI_INTRA_FLOW_CREDIT"              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni0_fr_pi_intra_flow_credit_u {
	mmr_t	sh_xnni0_fr_pi_intra_flow_credit_regval;
	struct {
		mmr_t	vc0_test    : 7;
		mmr_t	reserved_0  : 1;
		mmr_t	vc0_dyn     : 7;
		mmr_t	reserved_1  : 1;
		mmr_t	vc0_cap     : 7;
		mmr_t	reserved_2  : 1;
		mmr_t	vc2_test    : 7;
		mmr_t	reserved_3  : 1;
		mmr_t	vc2_dyn     : 7;
		mmr_t	reserved_4  : 1;
		mmr_t	vc2_cap     : 7;
		mmr_t	reserved_5  : 17;
	} sh_xnni0_fr_pi_intra_flow_credit_s;
} sh_xnni0_fr_pi_intra_flow_credit_u_t;
#else
typedef union sh_xnni0_fr_pi_intra_flow_credit_u {
	mmr_t	sh_xnni0_fr_pi_intra_flow_credit_regval;
	struct {
		mmr_t	reserved_5  : 17;
		mmr_t	vc2_cap     : 7;
		mmr_t	reserved_4  : 1;
		mmr_t	vc2_dyn     : 7;
		mmr_t	reserved_3  : 1;
		mmr_t	vc2_test    : 7;
		mmr_t	reserved_2  : 1;
		mmr_t	vc0_cap     : 7;
		mmr_t	reserved_1  : 1;
		mmr_t	vc0_dyn     : 7;
		mmr_t	reserved_0  : 1;
		mmr_t	vc0_test    : 7;
	} sh_xnni0_fr_pi_intra_flow_credit_s;
} sh_xnni0_fr_pi_intra_flow_credit_u_t;
#endif

/* ==================================================================== */
/*             Register "SH_XNNI0_FR_MD_INTRA_FLOW_CREDIT"              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni0_fr_md_intra_flow_credit_u {
	mmr_t	sh_xnni0_fr_md_intra_flow_credit_regval;
	struct {
		mmr_t	vc0_test    : 7;
		mmr_t	reserved_0  : 1;
		mmr_t	vc0_dyn     : 7;
		mmr_t	reserved_1  : 1;
		mmr_t	vc0_cap     : 7;
		mmr_t	reserved_2  : 1;
		mmr_t	vc2_test    : 7;
		mmr_t	reserved_3  : 1;
		mmr_t	vc2_dyn     : 7;
		mmr_t	reserved_4  : 1;
		mmr_t	vc2_cap     : 7;
		mmr_t	reserved_5  : 17;
	} sh_xnni0_fr_md_intra_flow_credit_s;
} sh_xnni0_fr_md_intra_flow_credit_u_t;
#else
typedef union sh_xnni0_fr_md_intra_flow_credit_u {
	mmr_t	sh_xnni0_fr_md_intra_flow_credit_regval;
	struct {
		mmr_t	reserved_5  : 17;
		mmr_t	vc2_cap     : 7;
		mmr_t	reserved_4  : 1;
		mmr_t	vc2_dyn     : 7;
		mmr_t	reserved_3  : 1;
		mmr_t	vc2_test    : 7;
		mmr_t	reserved_2  : 1;
		mmr_t	vc0_cap     : 7;
		mmr_t	reserved_1  : 1;
		mmr_t	vc0_dyn     : 7;
		mmr_t	reserved_0  : 1;
		mmr_t	vc0_test    : 7;
	} sh_xnni0_fr_md_intra_flow_credit_s;
} sh_xnni0_fr_md_intra_flow_credit_u_t;
#endif

/* ==================================================================== */
/*            Register "SH_XNNI0_FR_IILB_INTRA_FLOW_CREDIT"             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni0_fr_iilb_intra_flow_credit_u {
	mmr_t	sh_xnni0_fr_iilb_intra_flow_credit_regval;
	struct {
		mmr_t	vc0_test    : 7;
		mmr_t	reserved_0  : 1;
		mmr_t	vc0_dyn     : 7;
		mmr_t	reserved_1  : 1;
		mmr_t	vc0_cap     : 7;
		mmr_t	reserved_2  : 1;
		mmr_t	vc2_test    : 7;
		mmr_t	reserved_3  : 1;
		mmr_t	vc2_dyn     : 7;
		mmr_t	reserved_4  : 1;
		mmr_t	vc2_cap     : 7;
		mmr_t	reserved_5  : 17;
	} sh_xnni0_fr_iilb_intra_flow_credit_s;
} sh_xnni0_fr_iilb_intra_flow_credit_u_t;
#else
typedef union sh_xnni0_fr_iilb_intra_flow_credit_u {
	mmr_t	sh_xnni0_fr_iilb_intra_flow_credit_regval;
	struct {
		mmr_t	reserved_5  : 17;
		mmr_t	vc2_cap     : 7;
		mmr_t	reserved_4  : 1;
		mmr_t	vc2_dyn     : 7;
		mmr_t	reserved_3  : 1;
		mmr_t	vc2_test    : 7;
		mmr_t	reserved_2  : 1;
		mmr_t	vc0_cap     : 7;
		mmr_t	reserved_1  : 1;
		mmr_t	vc0_dyn     : 7;
		mmr_t	reserved_0  : 1;
		mmr_t	vc0_test    : 7;
	} sh_xnni0_fr_iilb_intra_flow_credit_s;
} sh_xnni0_fr_iilb_intra_flow_credit_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_XNNI0_0_INTRANI_FLOW"                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni0_0_intrani_flow_u {
	mmr_t	sh_xnni0_0_intrani_flow_regval;
	struct {
		mmr_t	debit_vc0_withhold   : 6;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	reserved_1           : 56;
	} sh_xnni0_0_intrani_flow_s;
} sh_xnni0_0_intrani_flow_u_t;
#else
typedef union sh_xnni0_0_intrani_flow_u {
	mmr_t	sh_xnni0_0_intrani_flow_regval;
	struct {
		mmr_t	reserved_1           : 56;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc0_withhold   : 6;
	} sh_xnni0_0_intrani_flow_s;
} sh_xnni0_0_intrani_flow_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_XNNI0_1_INTRANI_FLOW"                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni0_1_intrani_flow_u {
	mmr_t	sh_xnni0_1_intrani_flow_regval;
	struct {
		mmr_t	debit_vc1_withhold   : 6;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc1_force_cred : 1;
		mmr_t	reserved_1           : 56;
	} sh_xnni0_1_intrani_flow_s;
} sh_xnni0_1_intrani_flow_u_t;
#else
typedef union sh_xnni0_1_intrani_flow_u {
	mmr_t	sh_xnni0_1_intrani_flow_regval;
	struct {
		mmr_t	reserved_1           : 56;
		mmr_t	debit_vc1_force_cred : 1;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc1_withhold   : 6;
	} sh_xnni0_1_intrani_flow_s;
} sh_xnni0_1_intrani_flow_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_XNNI0_2_INTRANI_FLOW"                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni0_2_intrani_flow_u {
	mmr_t	sh_xnni0_2_intrani_flow_regval;
	struct {
		mmr_t	debit_vc2_withhold   : 6;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	reserved_1           : 56;
	} sh_xnni0_2_intrani_flow_s;
} sh_xnni0_2_intrani_flow_u_t;
#else
typedef union sh_xnni0_2_intrani_flow_u {
	mmr_t	sh_xnni0_2_intrani_flow_regval;
	struct {
		mmr_t	reserved_1           : 56;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc2_withhold   : 6;
	} sh_xnni0_2_intrani_flow_s;
} sh_xnni0_2_intrani_flow_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_XNNI0_3_INTRANI_FLOW"                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni0_3_intrani_flow_u {
	mmr_t	sh_xnni0_3_intrani_flow_regval;
	struct {
		mmr_t	debit_vc3_withhold   : 6;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc3_force_cred : 1;
		mmr_t	reserved_1           : 56;
	} sh_xnni0_3_intrani_flow_s;
} sh_xnni0_3_intrani_flow_u_t;
#else
typedef union sh_xnni0_3_intrani_flow_u {
	mmr_t	sh_xnni0_3_intrani_flow_regval;
	struct {
		mmr_t	reserved_1           : 56;
		mmr_t	debit_vc3_force_cred : 1;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc3_withhold   : 6;
	} sh_xnni0_3_intrani_flow_s;
} sh_xnni0_3_intrani_flow_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_XNNI0_VCSWITCH_FLOW"                   */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni0_vcswitch_flow_u {
	mmr_t	sh_xnni0_vcswitch_flow_regval;
	struct {
		mmr_t	ni_vcfifo_dateline_switch : 1;
		mmr_t	reserved_0                : 7;
		mmr_t	pi_vcfifo_switch          : 1;
		mmr_t	reserved_1                : 7;
		mmr_t	md_vcfifo_switch          : 1;
		mmr_t	reserved_2                : 7;
		mmr_t	iilb_vcfifo_switch        : 1;
		mmr_t	reserved_3                : 7;
		mmr_t	disable_sync_bypass_in    : 1;
		mmr_t	disable_sync_bypass_out   : 1;
		mmr_t	async_fifoes              : 1;
		mmr_t	reserved_4                : 29;
	} sh_xnni0_vcswitch_flow_s;
} sh_xnni0_vcswitch_flow_u_t;
#else
typedef union sh_xnni0_vcswitch_flow_u {
	mmr_t	sh_xnni0_vcswitch_flow_regval;
	struct {
		mmr_t	reserved_4                : 29;
		mmr_t	async_fifoes              : 1;
		mmr_t	disable_sync_bypass_out   : 1;
		mmr_t	disable_sync_bypass_in    : 1;
		mmr_t	reserved_3                : 7;
		mmr_t	iilb_vcfifo_switch        : 1;
		mmr_t	reserved_2                : 7;
		mmr_t	md_vcfifo_switch          : 1;
		mmr_t	reserved_1                : 7;
		mmr_t	pi_vcfifo_switch          : 1;
		mmr_t	reserved_0                : 7;
		mmr_t	ni_vcfifo_dateline_switch : 1;
	} sh_xnni0_vcswitch_flow_s;
} sh_xnni0_vcswitch_flow_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_XNNI0_TIMER_REG"                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni0_timer_reg_u {
	mmr_t	sh_xnni0_timer_reg_regval;
	struct {
		mmr_t	timeout_reg     : 24;
		mmr_t	reserved_0      : 8;
		mmr_t	linkcleanup_reg : 1;
		mmr_t	reserved_1      : 31;
	} sh_xnni0_timer_reg_s;
} sh_xnni0_timer_reg_u_t;
#else
typedef union sh_xnni0_timer_reg_u {
	mmr_t	sh_xnni0_timer_reg_regval;
	struct {
		mmr_t	reserved_1      : 31;
		mmr_t	linkcleanup_reg : 1;
		mmr_t	reserved_0      : 8;
		mmr_t	timeout_reg     : 24;
	} sh_xnni0_timer_reg_s;
} sh_xnni0_timer_reg_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_XNNI0_FIFO02_FLOW"                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni0_fifo02_flow_u {
	mmr_t	sh_xnni0_fifo02_flow_regval;
	struct {
		mmr_t	count_vc0_limit : 4;
		mmr_t	reserved_0      : 4;
		mmr_t	count_vc0_dyn   : 4;
		mmr_t	reserved_1      : 4;
		mmr_t	count_vc0_cap   : 4;
		mmr_t	reserved_2      : 4;
		mmr_t	count_vc2_limit : 4;
		mmr_t	reserved_3      : 4;
		mmr_t	count_vc2_dyn   : 4;
		mmr_t	reserved_4      : 4;
		mmr_t	count_vc2_cap   : 4;
		mmr_t	reserved_5      : 20;
	} sh_xnni0_fifo02_flow_s;
} sh_xnni0_fifo02_flow_u_t;
#else
typedef union sh_xnni0_fifo02_flow_u {
	mmr_t	sh_xnni0_fifo02_flow_regval;
	struct {
		mmr_t	reserved_5      : 20;
		mmr_t	count_vc2_cap   : 4;
		mmr_t	reserved_4      : 4;
		mmr_t	count_vc2_dyn   : 4;
		mmr_t	reserved_3      : 4;
		mmr_t	count_vc2_limit : 4;
		mmr_t	reserved_2      : 4;
		mmr_t	count_vc0_cap   : 4;
		mmr_t	reserved_1      : 4;
		mmr_t	count_vc0_dyn   : 4;
		mmr_t	reserved_0      : 4;
		mmr_t	count_vc0_limit : 4;
	} sh_xnni0_fifo02_flow_s;
} sh_xnni0_fifo02_flow_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_XNNI0_FIFO13_FLOW"                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni0_fifo13_flow_u {
	mmr_t	sh_xnni0_fifo13_flow_regval;
	struct {
		mmr_t	count_vc1_limit : 4;
		mmr_t	reserved_0      : 4;
		mmr_t	count_vc1_dyn   : 4;
		mmr_t	reserved_1      : 4;
		mmr_t	count_vc1_cap   : 4;
		mmr_t	reserved_2      : 4;
		mmr_t	count_vc3_limit : 4;
		mmr_t	reserved_3      : 4;
		mmr_t	count_vc3_dyn   : 4;
		mmr_t	reserved_4      : 4;
		mmr_t	count_vc3_cap   : 4;
		mmr_t	reserved_5      : 20;
	} sh_xnni0_fifo13_flow_s;
} sh_xnni0_fifo13_flow_u_t;
#else
typedef union sh_xnni0_fifo13_flow_u {
	mmr_t	sh_xnni0_fifo13_flow_regval;
	struct {
		mmr_t	reserved_5      : 20;
		mmr_t	count_vc3_cap   : 4;
		mmr_t	reserved_4      : 4;
		mmr_t	count_vc3_dyn   : 4;
		mmr_t	reserved_3      : 4;
		mmr_t	count_vc3_limit : 4;
		mmr_t	reserved_2      : 4;
		mmr_t	count_vc1_cap   : 4;
		mmr_t	reserved_1      : 4;
		mmr_t	count_vc1_dyn   : 4;
		mmr_t	reserved_0      : 4;
		mmr_t	count_vc1_limit : 4;
	} sh_xnni0_fifo13_flow_s;
} sh_xnni0_fifo13_flow_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_XNNI0_NI_FLOW"                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni0_ni_flow_u {
	mmr_t	sh_xnni0_ni_flow_regval;
	struct {
		mmr_t	vc0_limit   : 4;
		mmr_t	reserved_0  : 4;
		mmr_t	vc0_dyn     : 4;
		mmr_t	vc0_cap     : 4;
		mmr_t	vc1_limit   : 4;
		mmr_t	reserved_1  : 4;
		mmr_t	vc1_dyn     : 4;
		mmr_t	vc1_cap     : 4;
		mmr_t	vc2_limit   : 4;
		mmr_t	reserved_2  : 4;
		mmr_t	vc2_dyn     : 4;
		mmr_t	vc2_cap     : 4;
		mmr_t	vc3_limit   : 4;
		mmr_t	reserved_3  : 4;
		mmr_t	vc3_dyn     : 4;
		mmr_t	vc3_cap     : 4;
	} sh_xnni0_ni_flow_s;
} sh_xnni0_ni_flow_u_t;
#else
typedef union sh_xnni0_ni_flow_u {
	mmr_t	sh_xnni0_ni_flow_regval;
	struct {
		mmr_t	vc3_cap     : 4;
		mmr_t	vc3_dyn     : 4;
		mmr_t	reserved_3  : 4;
		mmr_t	vc3_limit   : 4;
		mmr_t	vc2_cap     : 4;
		mmr_t	vc2_dyn     : 4;
		mmr_t	reserved_2  : 4;
		mmr_t	vc2_limit   : 4;
		mmr_t	vc1_cap     : 4;
		mmr_t	vc1_dyn     : 4;
		mmr_t	reserved_1  : 4;
		mmr_t	vc1_limit   : 4;
		mmr_t	vc0_cap     : 4;
		mmr_t	vc0_dyn     : 4;
		mmr_t	reserved_0  : 4;
		mmr_t	vc0_limit   : 4;
	} sh_xnni0_ni_flow_s;
} sh_xnni0_ni_flow_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_XNNI0_DEAD_FLOW"                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni0_dead_flow_u {
	mmr_t	sh_xnni0_dead_flow_regval;
	struct {
		mmr_t	vc0_limit   : 4;
		mmr_t	reserved_0  : 4;
		mmr_t	vc0_dyn     : 4;
		mmr_t	vc0_cap     : 4;
		mmr_t	vc1_limit   : 4;
		mmr_t	reserved_1  : 4;
		mmr_t	vc1_dyn     : 4;
		mmr_t	vc1_cap     : 4;
		mmr_t	vc2_limit   : 4;
		mmr_t	reserved_2  : 4;
		mmr_t	vc2_dyn     : 4;
		mmr_t	vc2_cap     : 4;
		mmr_t	vc3_limit   : 4;
		mmr_t	reserved_3  : 4;
		mmr_t	vc3_dyn     : 4;
		mmr_t	vc3_cap     : 4;
	} sh_xnni0_dead_flow_s;
} sh_xnni0_dead_flow_u_t;
#else
typedef union sh_xnni0_dead_flow_u {
	mmr_t	sh_xnni0_dead_flow_regval;
	struct {
		mmr_t	vc3_cap     : 4;
		mmr_t	vc3_dyn     : 4;
		mmr_t	reserved_3  : 4;
		mmr_t	vc3_limit   : 4;
		mmr_t	vc2_cap     : 4;
		mmr_t	vc2_dyn     : 4;
		mmr_t	reserved_2  : 4;
		mmr_t	vc2_limit   : 4;
		mmr_t	vc1_cap     : 4;
		mmr_t	vc1_dyn     : 4;
		mmr_t	reserved_1  : 4;
		mmr_t	vc1_limit   : 4;
		mmr_t	vc0_cap     : 4;
		mmr_t	vc0_dyn     : 4;
		mmr_t	reserved_0  : 4;
		mmr_t	vc0_limit   : 4;
	} sh_xnni0_dead_flow_s;
} sh_xnni0_dead_flow_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_XNNI0_INJECT_AGE"                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni0_inject_age_u {
	mmr_t	sh_xnni0_inject_age_regval;
	struct {
		mmr_t	request_inject : 8;
		mmr_t	reply_inject   : 8;
		mmr_t	reserved_0     : 48;
	} sh_xnni0_inject_age_s;
} sh_xnni0_inject_age_u_t;
#else
typedef union sh_xnni0_inject_age_u {
	mmr_t	sh_xnni0_inject_age_regval;
	struct {
		mmr_t	reserved_0     : 48;
		mmr_t	reply_inject   : 8;
		mmr_t	request_inject : 8;
	} sh_xnni0_inject_age_s;
} sh_xnni0_inject_age_u_t;
#endif

/* ==================================================================== */
/*              Register "SH_XNNI1_TO_PI_INTRA_FLOW_DEBIT"              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni1_to_pi_intra_flow_debit_u {
	mmr_t	sh_xnni1_to_pi_intra_flow_debit_regval;
	struct {
		mmr_t	vc0_withhold   : 6;
		mmr_t	reserved_0     : 1;
		mmr_t	vc0_force_cred : 1;
		mmr_t	vc2_withhold   : 6;
		mmr_t	reserved_1     : 1;
		mmr_t	vc2_force_cred : 1;
		mmr_t	reserved_2     : 8;
		mmr_t	vc0_dyn        : 7;
		mmr_t	reserved_3     : 1;
		mmr_t	vc0_cap        : 7;
		mmr_t	reserved_4     : 9;
		mmr_t	vc2_dyn        : 7;
		mmr_t	reserved_5     : 1;
		mmr_t	vc2_cap        : 7;
		mmr_t	reserved_6     : 1;
	} sh_xnni1_to_pi_intra_flow_debit_s;
} sh_xnni1_to_pi_intra_flow_debit_u_t;
#else
typedef union sh_xnni1_to_pi_intra_flow_debit_u {
	mmr_t	sh_xnni1_to_pi_intra_flow_debit_regval;
	struct {
		mmr_t	reserved_6     : 1;
		mmr_t	vc2_cap        : 7;
		mmr_t	reserved_5     : 1;
		mmr_t	vc2_dyn        : 7;
		mmr_t	reserved_4     : 9;
		mmr_t	vc0_cap        : 7;
		mmr_t	reserved_3     : 1;
		mmr_t	vc0_dyn        : 7;
		mmr_t	reserved_2     : 8;
		mmr_t	vc2_force_cred : 1;
		mmr_t	reserved_1     : 1;
		mmr_t	vc2_withhold   : 6;
		mmr_t	vc0_force_cred : 1;
		mmr_t	reserved_0     : 1;
		mmr_t	vc0_withhold   : 6;
	} sh_xnni1_to_pi_intra_flow_debit_s;
} sh_xnni1_to_pi_intra_flow_debit_u_t;
#endif

/* ==================================================================== */
/*              Register "SH_XNNI1_TO_MD_INTRA_FLOW_DEBIT"              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni1_to_md_intra_flow_debit_u {
	mmr_t	sh_xnni1_to_md_intra_flow_debit_regval;
	struct {
		mmr_t	vc0_withhold   : 6;
		mmr_t	reserved_0     : 1;
		mmr_t	vc0_force_cred : 1;
		mmr_t	vc2_withhold   : 6;
		mmr_t	reserved_1     : 1;
		mmr_t	vc2_force_cred : 1;
		mmr_t	reserved_2     : 8;
		mmr_t	vc0_dyn        : 7;
		mmr_t	reserved_3     : 1;
		mmr_t	vc0_cap        : 7;
		mmr_t	reserved_4     : 9;
		mmr_t	vc2_dyn        : 7;
		mmr_t	reserved_5     : 1;
		mmr_t	vc2_cap        : 7;
		mmr_t	reserved_6     : 1;
	} sh_xnni1_to_md_intra_flow_debit_s;
} sh_xnni1_to_md_intra_flow_debit_u_t;
#else
typedef union sh_xnni1_to_md_intra_flow_debit_u {
	mmr_t	sh_xnni1_to_md_intra_flow_debit_regval;
	struct {
		mmr_t	reserved_6     : 1;
		mmr_t	vc2_cap        : 7;
		mmr_t	reserved_5     : 1;
		mmr_t	vc2_dyn        : 7;
		mmr_t	reserved_4     : 9;
		mmr_t	vc0_cap        : 7;
		mmr_t	reserved_3     : 1;
		mmr_t	vc0_dyn        : 7;
		mmr_t	reserved_2     : 8;
		mmr_t	vc2_force_cred : 1;
		mmr_t	reserved_1     : 1;
		mmr_t	vc2_withhold   : 6;
		mmr_t	vc0_force_cred : 1;
		mmr_t	reserved_0     : 1;
		mmr_t	vc0_withhold   : 6;
	} sh_xnni1_to_md_intra_flow_debit_s;
} sh_xnni1_to_md_intra_flow_debit_u_t;
#endif

/* ==================================================================== */
/*             Register "SH_XNNI1_TO_IILB_INTRA_FLOW_DEBIT"             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni1_to_iilb_intra_flow_debit_u {
	mmr_t	sh_xnni1_to_iilb_intra_flow_debit_regval;
	struct {
		mmr_t	vc0_withhold   : 6;
		mmr_t	reserved_0     : 1;
		mmr_t	vc0_force_cred : 1;
		mmr_t	vc2_withhold   : 6;
		mmr_t	reserved_1     : 1;
		mmr_t	vc2_force_cred : 1;
		mmr_t	reserved_2     : 8;
		mmr_t	vc0_dyn        : 7;
		mmr_t	reserved_3     : 1;
		mmr_t	vc0_cap        : 7;
		mmr_t	reserved_4     : 9;
		mmr_t	vc2_dyn        : 7;
		mmr_t	reserved_5     : 1;
		mmr_t	vc2_cap        : 7;
		mmr_t	reserved_6     : 1;
	} sh_xnni1_to_iilb_intra_flow_debit_s;
} sh_xnni1_to_iilb_intra_flow_debit_u_t;
#else
typedef union sh_xnni1_to_iilb_intra_flow_debit_u {
	mmr_t	sh_xnni1_to_iilb_intra_flow_debit_regval;
	struct {
		mmr_t	reserved_6     : 1;
		mmr_t	vc2_cap        : 7;
		mmr_t	reserved_5     : 1;
		mmr_t	vc2_dyn        : 7;
		mmr_t	reserved_4     : 9;
		mmr_t	vc0_cap        : 7;
		mmr_t	reserved_3     : 1;
		mmr_t	vc0_dyn        : 7;
		mmr_t	reserved_2     : 8;
		mmr_t	vc2_force_cred : 1;
		mmr_t	reserved_1     : 1;
		mmr_t	vc2_withhold   : 6;
		mmr_t	vc0_force_cred : 1;
		mmr_t	reserved_0     : 1;
		mmr_t	vc0_withhold   : 6;
	} sh_xnni1_to_iilb_intra_flow_debit_s;
} sh_xnni1_to_iilb_intra_flow_debit_u_t;
#endif

/* ==================================================================== */
/*             Register "SH_XNNI1_FR_PI_INTRA_FLOW_CREDIT"              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni1_fr_pi_intra_flow_credit_u {
	mmr_t	sh_xnni1_fr_pi_intra_flow_credit_regval;
	struct {
		mmr_t	vc0_test    : 7;
		mmr_t	reserved_0  : 1;
		mmr_t	vc0_dyn     : 7;
		mmr_t	reserved_1  : 1;
		mmr_t	vc0_cap     : 7;
		mmr_t	reserved_2  : 1;
		mmr_t	vc2_test    : 7;
		mmr_t	reserved_3  : 1;
		mmr_t	vc2_dyn     : 7;
		mmr_t	reserved_4  : 1;
		mmr_t	vc2_cap     : 7;
		mmr_t	reserved_5  : 17;
	} sh_xnni1_fr_pi_intra_flow_credit_s;
} sh_xnni1_fr_pi_intra_flow_credit_u_t;
#else
typedef union sh_xnni1_fr_pi_intra_flow_credit_u {
	mmr_t	sh_xnni1_fr_pi_intra_flow_credit_regval;
	struct {
		mmr_t	reserved_5  : 17;
		mmr_t	vc2_cap     : 7;
		mmr_t	reserved_4  : 1;
		mmr_t	vc2_dyn     : 7;
		mmr_t	reserved_3  : 1;
		mmr_t	vc2_test    : 7;
		mmr_t	reserved_2  : 1;
		mmr_t	vc0_cap     : 7;
		mmr_t	reserved_1  : 1;
		mmr_t	vc0_dyn     : 7;
		mmr_t	reserved_0  : 1;
		mmr_t	vc0_test    : 7;
	} sh_xnni1_fr_pi_intra_flow_credit_s;
} sh_xnni1_fr_pi_intra_flow_credit_u_t;
#endif

/* ==================================================================== */
/*             Register "SH_XNNI1_FR_MD_INTRA_FLOW_CREDIT"              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni1_fr_md_intra_flow_credit_u {
	mmr_t	sh_xnni1_fr_md_intra_flow_credit_regval;
	struct {
		mmr_t	vc0_test    : 7;
		mmr_t	reserved_0  : 1;
		mmr_t	vc0_dyn     : 7;
		mmr_t	reserved_1  : 1;
		mmr_t	vc0_cap     : 7;
		mmr_t	reserved_2  : 1;
		mmr_t	vc2_test    : 7;
		mmr_t	reserved_3  : 1;
		mmr_t	vc2_dyn     : 7;
		mmr_t	reserved_4  : 1;
		mmr_t	vc2_cap     : 7;
		mmr_t	reserved_5  : 17;
	} sh_xnni1_fr_md_intra_flow_credit_s;
} sh_xnni1_fr_md_intra_flow_credit_u_t;
#else
typedef union sh_xnni1_fr_md_intra_flow_credit_u {
	mmr_t	sh_xnni1_fr_md_intra_flow_credit_regval;
	struct {
		mmr_t	reserved_5  : 17;
		mmr_t	vc2_cap     : 7;
		mmr_t	reserved_4  : 1;
		mmr_t	vc2_dyn     : 7;
		mmr_t	reserved_3  : 1;
		mmr_t	vc2_test    : 7;
		mmr_t	reserved_2  : 1;
		mmr_t	vc0_cap     : 7;
		mmr_t	reserved_1  : 1;
		mmr_t	vc0_dyn     : 7;
		mmr_t	reserved_0  : 1;
		mmr_t	vc0_test    : 7;
	} sh_xnni1_fr_md_intra_flow_credit_s;
} sh_xnni1_fr_md_intra_flow_credit_u_t;
#endif

/* ==================================================================== */
/*            Register "SH_XNNI1_FR_IILB_INTRA_FLOW_CREDIT"             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni1_fr_iilb_intra_flow_credit_u {
	mmr_t	sh_xnni1_fr_iilb_intra_flow_credit_regval;
	struct {
		mmr_t	vc0_test    : 7;
		mmr_t	reserved_0  : 1;
		mmr_t	vc0_dyn     : 7;
		mmr_t	reserved_1  : 1;
		mmr_t	vc0_cap     : 7;
		mmr_t	reserved_2  : 1;
		mmr_t	vc2_test    : 7;
		mmr_t	reserved_3  : 1;
		mmr_t	vc2_dyn     : 7;
		mmr_t	reserved_4  : 1;
		mmr_t	vc2_cap     : 7;
		mmr_t	reserved_5  : 17;
	} sh_xnni1_fr_iilb_intra_flow_credit_s;
} sh_xnni1_fr_iilb_intra_flow_credit_u_t;
#else
typedef union sh_xnni1_fr_iilb_intra_flow_credit_u {
	mmr_t	sh_xnni1_fr_iilb_intra_flow_credit_regval;
	struct {
		mmr_t	reserved_5  : 17;
		mmr_t	vc2_cap     : 7;
		mmr_t	reserved_4  : 1;
		mmr_t	vc2_dyn     : 7;
		mmr_t	reserved_3  : 1;
		mmr_t	vc2_test    : 7;
		mmr_t	reserved_2  : 1;
		mmr_t	vc0_cap     : 7;
		mmr_t	reserved_1  : 1;
		mmr_t	vc0_dyn     : 7;
		mmr_t	reserved_0  : 1;
		mmr_t	vc0_test    : 7;
	} sh_xnni1_fr_iilb_intra_flow_credit_s;
} sh_xnni1_fr_iilb_intra_flow_credit_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_XNNI1_0_INTRANI_FLOW"                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni1_0_intrani_flow_u {
	mmr_t	sh_xnni1_0_intrani_flow_regval;
	struct {
		mmr_t	debit_vc0_withhold   : 6;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	reserved_1           : 56;
	} sh_xnni1_0_intrani_flow_s;
} sh_xnni1_0_intrani_flow_u_t;
#else
typedef union sh_xnni1_0_intrani_flow_u {
	mmr_t	sh_xnni1_0_intrani_flow_regval;
	struct {
		mmr_t	reserved_1           : 56;
		mmr_t	debit_vc0_force_cred : 1;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc0_withhold   : 6;
	} sh_xnni1_0_intrani_flow_s;
} sh_xnni1_0_intrani_flow_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_XNNI1_1_INTRANI_FLOW"                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni1_1_intrani_flow_u {
	mmr_t	sh_xnni1_1_intrani_flow_regval;
	struct {
		mmr_t	debit_vc1_withhold   : 6;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc1_force_cred : 1;
		mmr_t	reserved_1           : 56;
	} sh_xnni1_1_intrani_flow_s;
} sh_xnni1_1_intrani_flow_u_t;
#else
typedef union sh_xnni1_1_intrani_flow_u {
	mmr_t	sh_xnni1_1_intrani_flow_regval;
	struct {
		mmr_t	reserved_1           : 56;
		mmr_t	debit_vc1_force_cred : 1;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc1_withhold   : 6;
	} sh_xnni1_1_intrani_flow_s;
} sh_xnni1_1_intrani_flow_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_XNNI1_2_INTRANI_FLOW"                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni1_2_intrani_flow_u {
	mmr_t	sh_xnni1_2_intrani_flow_regval;
	struct {
		mmr_t	debit_vc2_withhold   : 6;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	reserved_1           : 56;
	} sh_xnni1_2_intrani_flow_s;
} sh_xnni1_2_intrani_flow_u_t;
#else
typedef union sh_xnni1_2_intrani_flow_u {
	mmr_t	sh_xnni1_2_intrani_flow_regval;
	struct {
		mmr_t	reserved_1           : 56;
		mmr_t	debit_vc2_force_cred : 1;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc2_withhold   : 6;
	} sh_xnni1_2_intrani_flow_s;
} sh_xnni1_2_intrani_flow_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_XNNI1_3_INTRANI_FLOW"                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni1_3_intrani_flow_u {
	mmr_t	sh_xnni1_3_intrani_flow_regval;
	struct {
		mmr_t	debit_vc3_withhold   : 6;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc3_force_cred : 1;
		mmr_t	reserved_1           : 56;
	} sh_xnni1_3_intrani_flow_s;
} sh_xnni1_3_intrani_flow_u_t;
#else
typedef union sh_xnni1_3_intrani_flow_u {
	mmr_t	sh_xnni1_3_intrani_flow_regval;
	struct {
		mmr_t	reserved_1           : 56;
		mmr_t	debit_vc3_force_cred : 1;
		mmr_t	reserved_0           : 1;
		mmr_t	debit_vc3_withhold   : 6;
	} sh_xnni1_3_intrani_flow_s;
} sh_xnni1_3_intrani_flow_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_XNNI1_VCSWITCH_FLOW"                   */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni1_vcswitch_flow_u {
	mmr_t	sh_xnni1_vcswitch_flow_regval;
	struct {
		mmr_t	ni_vcfifo_dateline_switch : 1;
		mmr_t	reserved_0                : 7;
		mmr_t	pi_vcfifo_switch          : 1;
		mmr_t	reserved_1                : 7;
		mmr_t	md_vcfifo_switch          : 1;
		mmr_t	reserved_2                : 7;
		mmr_t	iilb_vcfifo_switch        : 1;
		mmr_t	reserved_3                : 7;
		mmr_t	disable_sync_bypass_in    : 1;
		mmr_t	disable_sync_bypass_out   : 1;
		mmr_t	async_fifoes              : 1;
		mmr_t	reserved_4                : 29;
	} sh_xnni1_vcswitch_flow_s;
} sh_xnni1_vcswitch_flow_u_t;
#else
typedef union sh_xnni1_vcswitch_flow_u {
	mmr_t	sh_xnni1_vcswitch_flow_regval;
	struct {
		mmr_t	reserved_4                : 29;
		mmr_t	async_fifoes              : 1;
		mmr_t	disable_sync_bypass_out   : 1;
		mmr_t	disable_sync_bypass_in    : 1;
		mmr_t	reserved_3                : 7;
		mmr_t	iilb_vcfifo_switch        : 1;
		mmr_t	reserved_2                : 7;
		mmr_t	md_vcfifo_switch          : 1;
		mmr_t	reserved_1                : 7;
		mmr_t	pi_vcfifo_switch          : 1;
		mmr_t	reserved_0                : 7;
		mmr_t	ni_vcfifo_dateline_switch : 1;
	} sh_xnni1_vcswitch_flow_s;
} sh_xnni1_vcswitch_flow_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_XNNI1_TIMER_REG"                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni1_timer_reg_u {
	mmr_t	sh_xnni1_timer_reg_regval;
	struct {
		mmr_t	timeout_reg     : 24;
		mmr_t	reserved_0      : 8;
		mmr_t	linkcleanup_reg : 1;
		mmr_t	reserved_1      : 31;
	} sh_xnni1_timer_reg_s;
} sh_xnni1_timer_reg_u_t;
#else
typedef union sh_xnni1_timer_reg_u {
	mmr_t	sh_xnni1_timer_reg_regval;
	struct {
		mmr_t	reserved_1      : 31;
		mmr_t	linkcleanup_reg : 1;
		mmr_t	reserved_0      : 8;
		mmr_t	timeout_reg     : 24;
	} sh_xnni1_timer_reg_s;
} sh_xnni1_timer_reg_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_XNNI1_FIFO02_FLOW"                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni1_fifo02_flow_u {
	mmr_t	sh_xnni1_fifo02_flow_regval;
	struct {
		mmr_t	count_vc0_limit : 4;
		mmr_t	reserved_0      : 4;
		mmr_t	count_vc0_dyn   : 4;
		mmr_t	reserved_1      : 4;
		mmr_t	count_vc0_cap   : 4;
		mmr_t	reserved_2      : 4;
		mmr_t	count_vc2_limit : 4;
		mmr_t	reserved_3      : 4;
		mmr_t	count_vc2_dyn   : 4;
		mmr_t	reserved_4      : 4;
		mmr_t	count_vc2_cap   : 4;
		mmr_t	reserved_5      : 20;
	} sh_xnni1_fifo02_flow_s;
} sh_xnni1_fifo02_flow_u_t;
#else
typedef union sh_xnni1_fifo02_flow_u {
	mmr_t	sh_xnni1_fifo02_flow_regval;
	struct {
		mmr_t	reserved_5      : 20;
		mmr_t	count_vc2_cap   : 4;
		mmr_t	reserved_4      : 4;
		mmr_t	count_vc2_dyn   : 4;
		mmr_t	reserved_3      : 4;
		mmr_t	count_vc2_limit : 4;
		mmr_t	reserved_2      : 4;
		mmr_t	count_vc0_cap   : 4;
		mmr_t	reserved_1      : 4;
		mmr_t	count_vc0_dyn   : 4;
		mmr_t	reserved_0      : 4;
		mmr_t	count_vc0_limit : 4;
	} sh_xnni1_fifo02_flow_s;
} sh_xnni1_fifo02_flow_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_XNNI1_FIFO13_FLOW"                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni1_fifo13_flow_u {
	mmr_t	sh_xnni1_fifo13_flow_regval;
	struct {
		mmr_t	count_vc1_limit : 4;
		mmr_t	reserved_0      : 4;
		mmr_t	count_vc1_dyn   : 4;
		mmr_t	reserved_1      : 4;
		mmr_t	count_vc1_cap   : 4;
		mmr_t	reserved_2      : 4;
		mmr_t	count_vc3_limit : 4;
		mmr_t	reserved_3      : 4;
		mmr_t	count_vc3_dyn   : 4;
		mmr_t	reserved_4      : 4;
		mmr_t	count_vc3_cap   : 4;
		mmr_t	reserved_5      : 20;
	} sh_xnni1_fifo13_flow_s;
} sh_xnni1_fifo13_flow_u_t;
#else
typedef union sh_xnni1_fifo13_flow_u {
	mmr_t	sh_xnni1_fifo13_flow_regval;
	struct {
		mmr_t	reserved_5      : 20;
		mmr_t	count_vc3_cap   : 4;
		mmr_t	reserved_4      : 4;
		mmr_t	count_vc3_dyn   : 4;
		mmr_t	reserved_3      : 4;
		mmr_t	count_vc3_limit : 4;
		mmr_t	reserved_2      : 4;
		mmr_t	count_vc1_cap   : 4;
		mmr_t	reserved_1      : 4;
		mmr_t	count_vc1_dyn   : 4;
		mmr_t	reserved_0      : 4;
		mmr_t	count_vc1_limit : 4;
	} sh_xnni1_fifo13_flow_s;
} sh_xnni1_fifo13_flow_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_XNNI1_NI_FLOW"                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni1_ni_flow_u {
	mmr_t	sh_xnni1_ni_flow_regval;
	struct {
		mmr_t	vc0_limit   : 4;
		mmr_t	reserved_0  : 4;
		mmr_t	vc0_dyn     : 4;
		mmr_t	vc0_cap     : 4;
		mmr_t	vc1_limit   : 4;
		mmr_t	reserved_1  : 4;
		mmr_t	vc1_dyn     : 4;
		mmr_t	vc1_cap     : 4;
		mmr_t	vc2_limit   : 4;
		mmr_t	reserved_2  : 4;
		mmr_t	vc2_dyn     : 4;
		mmr_t	vc2_cap     : 4;
		mmr_t	vc3_limit   : 4;
		mmr_t	reserved_3  : 4;
		mmr_t	vc3_dyn     : 4;
		mmr_t	vc3_cap     : 4;
	} sh_xnni1_ni_flow_s;
} sh_xnni1_ni_flow_u_t;
#else
typedef union sh_xnni1_ni_flow_u {
	mmr_t	sh_xnni1_ni_flow_regval;
	struct {
		mmr_t	vc3_cap     : 4;
		mmr_t	vc3_dyn     : 4;
		mmr_t	reserved_3  : 4;
		mmr_t	vc3_limit   : 4;
		mmr_t	vc2_cap     : 4;
		mmr_t	vc2_dyn     : 4;
		mmr_t	reserved_2  : 4;
		mmr_t	vc2_limit   : 4;
		mmr_t	vc1_cap     : 4;
		mmr_t	vc1_dyn     : 4;
		mmr_t	reserved_1  : 4;
		mmr_t	vc1_limit   : 4;
		mmr_t	vc0_cap     : 4;
		mmr_t	vc0_dyn     : 4;
		mmr_t	reserved_0  : 4;
		mmr_t	vc0_limit   : 4;
	} sh_xnni1_ni_flow_s;
} sh_xnni1_ni_flow_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_XNNI1_DEAD_FLOW"                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni1_dead_flow_u {
	mmr_t	sh_xnni1_dead_flow_regval;
	struct {
		mmr_t	vc0_limit   : 4;
		mmr_t	reserved_0  : 4;
		mmr_t	vc0_dyn     : 4;
		mmr_t	vc0_cap     : 4;
		mmr_t	vc1_limit   : 4;
		mmr_t	reserved_1  : 4;
		mmr_t	vc1_dyn     : 4;
		mmr_t	vc1_cap     : 4;
		mmr_t	vc2_limit   : 4;
		mmr_t	reserved_2  : 4;
		mmr_t	vc2_dyn     : 4;
		mmr_t	vc2_cap     : 4;
		mmr_t	vc3_limit   : 4;
		mmr_t	reserved_3  : 4;
		mmr_t	vc3_dyn     : 4;
		mmr_t	vc3_cap     : 4;
	} sh_xnni1_dead_flow_s;
} sh_xnni1_dead_flow_u_t;
#else
typedef union sh_xnni1_dead_flow_u {
	mmr_t	sh_xnni1_dead_flow_regval;
	struct {
		mmr_t	vc3_cap     : 4;
		mmr_t	vc3_dyn     : 4;
		mmr_t	reserved_3  : 4;
		mmr_t	vc3_limit   : 4;
		mmr_t	vc2_cap     : 4;
		mmr_t	vc2_dyn     : 4;
		mmr_t	reserved_2  : 4;
		mmr_t	vc2_limit   : 4;
		mmr_t	vc1_cap     : 4;
		mmr_t	vc1_dyn     : 4;
		mmr_t	reserved_1  : 4;
		mmr_t	vc1_limit   : 4;
		mmr_t	vc0_cap     : 4;
		mmr_t	vc0_dyn     : 4;
		mmr_t	reserved_0  : 4;
		mmr_t	vc0_limit   : 4;
	} sh_xnni1_dead_flow_s;
} sh_xnni1_dead_flow_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_XNNI1_INJECT_AGE"                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnni1_inject_age_u {
	mmr_t	sh_xnni1_inject_age_regval;
	struct {
		mmr_t	request_inject : 8;
		mmr_t	reply_inject   : 8;
		mmr_t	reserved_0     : 48;
	} sh_xnni1_inject_age_s;
} sh_xnni1_inject_age_u_t;
#else
typedef union sh_xnni1_inject_age_u {
	mmr_t	sh_xnni1_inject_age_regval;
	struct {
		mmr_t	reserved_0     : 48;
		mmr_t	reply_inject   : 8;
		mmr_t	request_inject : 8;
	} sh_xnni1_inject_age_s;
} sh_xnni1_inject_age_u_t;
#endif

/* ==================================================================== */
/*                      Register "SH_XN_DEBUG_SEL"                      */
/*                         XN Debug Port Select                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_debug_sel_u {
	mmr_t	sh_xn_debug_sel_regval;
	struct {
		mmr_t	nibble0_rlm_sel    : 3;
		mmr_t	reserved_0         : 1;
		mmr_t	nibble0_nibble_sel : 3;
		mmr_t	reserved_1         : 1;
		mmr_t	nibble1_rlm_sel    : 3;
		mmr_t	reserved_2         : 1;
		mmr_t	nibble1_nibble_sel : 3;
		mmr_t	reserved_3         : 1;
		mmr_t	nibble2_rlm_sel    : 3;
		mmr_t	reserved_4         : 1;
		mmr_t	nibble2_nibble_sel : 3;
		mmr_t	reserved_5         : 1;
		mmr_t	nibble3_rlm_sel    : 3;
		mmr_t	reserved_6         : 1;
		mmr_t	nibble3_nibble_sel : 3;
		mmr_t	reserved_7         : 1;
		mmr_t	nibble4_rlm_sel    : 3;
		mmr_t	reserved_8         : 1;
		mmr_t	nibble4_nibble_sel : 3;
		mmr_t	reserved_9         : 1;
		mmr_t	nibble5_rlm_sel    : 3;
		mmr_t	reserved_10        : 1;
		mmr_t	nibble5_nibble_sel : 3;
		mmr_t	reserved_11        : 1;
		mmr_t	nibble6_rlm_sel    : 3;
		mmr_t	reserved_12        : 1;
		mmr_t	nibble6_nibble_sel : 3;
		mmr_t	reserved_13        : 1;
		mmr_t	nibble7_rlm_sel    : 3;
		mmr_t	reserved_14        : 1;
		mmr_t	nibble7_nibble_sel : 3;
		mmr_t	trigger_enable     : 1;
	} sh_xn_debug_sel_s;
} sh_xn_debug_sel_u_t;
#else
typedef union sh_xn_debug_sel_u {
	mmr_t	sh_xn_debug_sel_regval;
	struct {
		mmr_t	trigger_enable     : 1;
		mmr_t	nibble7_nibble_sel : 3;
		mmr_t	reserved_14        : 1;
		mmr_t	nibble7_rlm_sel    : 3;
		mmr_t	reserved_13        : 1;
		mmr_t	nibble6_nibble_sel : 3;
		mmr_t	reserved_12        : 1;
		mmr_t	nibble6_rlm_sel    : 3;
		mmr_t	reserved_11        : 1;
		mmr_t	nibble5_nibble_sel : 3;
		mmr_t	reserved_10        : 1;
		mmr_t	nibble5_rlm_sel    : 3;
		mmr_t	reserved_9         : 1;
		mmr_t	nibble4_nibble_sel : 3;
		mmr_t	reserved_8         : 1;
		mmr_t	nibble4_rlm_sel    : 3;
		mmr_t	reserved_7         : 1;
		mmr_t	nibble3_nibble_sel : 3;
		mmr_t	reserved_6         : 1;
		mmr_t	nibble3_rlm_sel    : 3;
		mmr_t	reserved_5         : 1;
		mmr_t	nibble2_nibble_sel : 3;
		mmr_t	reserved_4         : 1;
		mmr_t	nibble2_rlm_sel    : 3;
		mmr_t	reserved_3         : 1;
		mmr_t	nibble1_nibble_sel : 3;
		mmr_t	reserved_2         : 1;
		mmr_t	nibble1_rlm_sel    : 3;
		mmr_t	reserved_1         : 1;
		mmr_t	nibble0_nibble_sel : 3;
		mmr_t	reserved_0         : 1;
		mmr_t	nibble0_rlm_sel    : 3;
	} sh_xn_debug_sel_s;
} sh_xn_debug_sel_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_XN_DEBUG_TRIG_SEL"                    */
/*                       XN Debug trigger Select                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_debug_trig_sel_u {
	mmr_t	sh_xn_debug_trig_sel_regval;
	struct {
		mmr_t	trigger0_rlm_sel    : 3;
		mmr_t	reserved_0          : 1;
		mmr_t	trigger0_nibble_sel : 3;
		mmr_t	reserved_1          : 1;
		mmr_t	trigger1_rlm_sel    : 3;
		mmr_t	reserved_2          : 1;
		mmr_t	trigger1_nibble_sel : 3;
		mmr_t	reserved_3          : 1;
		mmr_t	trigger2_rlm_sel    : 3;
		mmr_t	reserved_4          : 1;
		mmr_t	trigger2_nibble_sel : 3;
		mmr_t	reserved_5          : 1;
		mmr_t	trigger3_rlm_sel    : 3;
		mmr_t	reserved_6          : 1;
		mmr_t	trigger3_nibble_sel : 3;
		mmr_t	reserved_7          : 1;
		mmr_t	trigger4_rlm_sel    : 3;
		mmr_t	reserved_8          : 1;
		mmr_t	trigger4_nibble_sel : 3;
		mmr_t	reserved_9          : 1;
		mmr_t	trigger5_rlm_sel    : 3;
		mmr_t	reserved_10         : 1;
		mmr_t	trigger5_nibble_sel : 3;
		mmr_t	reserved_11         : 1;
		mmr_t	trigger6_rlm_sel    : 3;
		mmr_t	reserved_12         : 1;
		mmr_t	trigger6_nibble_sel : 3;
		mmr_t	reserved_13         : 1;
		mmr_t	trigger7_rlm_sel    : 3;
		mmr_t	reserved_14         : 1;
		mmr_t	trigger7_nibble_sel : 3;
		mmr_t	reserved_15         : 1;
	} sh_xn_debug_trig_sel_s;
} sh_xn_debug_trig_sel_u_t;
#else
typedef union sh_xn_debug_trig_sel_u {
	mmr_t	sh_xn_debug_trig_sel_regval;
	struct {
		mmr_t	reserved_15         : 1;
		mmr_t	trigger7_nibble_sel : 3;
		mmr_t	reserved_14         : 1;
		mmr_t	trigger7_rlm_sel    : 3;
		mmr_t	reserved_13         : 1;
		mmr_t	trigger6_nibble_sel : 3;
		mmr_t	reserved_12         : 1;
		mmr_t	trigger6_rlm_sel    : 3;
		mmr_t	reserved_11         : 1;
		mmr_t	trigger5_nibble_sel : 3;
		mmr_t	reserved_10         : 1;
		mmr_t	trigger5_rlm_sel    : 3;
		mmr_t	reserved_9          : 1;
		mmr_t	trigger4_nibble_sel : 3;
		mmr_t	reserved_8          : 1;
		mmr_t	trigger4_rlm_sel    : 3;
		mmr_t	reserved_7          : 1;
		mmr_t	trigger3_nibble_sel : 3;
		mmr_t	reserved_6          : 1;
		mmr_t	trigger3_rlm_sel    : 3;
		mmr_t	reserved_5          : 1;
		mmr_t	trigger2_nibble_sel : 3;
		mmr_t	reserved_4          : 1;
		mmr_t	trigger2_rlm_sel    : 3;
		mmr_t	reserved_3          : 1;
		mmr_t	trigger1_nibble_sel : 3;
		mmr_t	reserved_2          : 1;
		mmr_t	trigger1_rlm_sel    : 3;
		mmr_t	reserved_1          : 1;
		mmr_t	trigger0_nibble_sel : 3;
		mmr_t	reserved_0          : 1;
		mmr_t	trigger0_rlm_sel    : 3;
	} sh_xn_debug_trig_sel_s;
} sh_xn_debug_trig_sel_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_XN_TRIGGER_COMPARE"                   */
/*                           XN Debug Compare                           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_trigger_compare_u {
	mmr_t	sh_xn_trigger_compare_regval;
	struct {
		mmr_t	mask        : 32;
		mmr_t	reserved_0  : 32;
	} sh_xn_trigger_compare_s;
} sh_xn_trigger_compare_u_t;
#else
typedef union sh_xn_trigger_compare_u {
	mmr_t	sh_xn_trigger_compare_regval;
	struct {
		mmr_t	reserved_0  : 32;
		mmr_t	mask        : 32;
	} sh_xn_trigger_compare_s;
} sh_xn_trigger_compare_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_XN_TRIGGER_DATA"                     */
/*                        XN Debug Compare Data                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_trigger_data_u {
	mmr_t	sh_xn_trigger_data_regval;
	struct {
		mmr_t	compare_pattern : 32;
		mmr_t	reserved_0      : 32;
	} sh_xn_trigger_data_s;
} sh_xn_trigger_data_u_t;
#else
typedef union sh_xn_trigger_data_u {
	mmr_t	sh_xn_trigger_data_regval;
	struct {
		mmr_t	reserved_0      : 32;
		mmr_t	compare_pattern : 32;
	} sh_xn_trigger_data_s;
} sh_xn_trigger_data_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_XN_IILB_DEBUG_SEL"                    */
/*                      XN IILB Debug Port Select                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_iilb_debug_sel_u {
	mmr_t	sh_xn_iilb_debug_sel_regval;
	struct {
		mmr_t	nibble0_input_sel  : 3;
		mmr_t	reserved_0         : 1;
		mmr_t	nibble0_nibble_sel : 3;
		mmr_t	reserved_1         : 1;
		mmr_t	nibble1_input_sel  : 3;
		mmr_t	reserved_2         : 1;
		mmr_t	nibble1_nibble_sel : 3;
		mmr_t	reserved_3         : 1;
		mmr_t	nibble2_input_sel  : 3;
		mmr_t	reserved_4         : 1;
		mmr_t	nibble2_nibble_sel : 3;
		mmr_t	reserved_5         : 1;
		mmr_t	nibble3_input_sel  : 3;
		mmr_t	reserved_6         : 1;
		mmr_t	nibble3_nibble_sel : 3;
		mmr_t	reserved_7         : 1;
		mmr_t	nibble4_input_sel  : 3;
		mmr_t	reserved_8         : 1;
		mmr_t	nibble4_nibble_sel : 3;
		mmr_t	reserved_9         : 1;
		mmr_t	nibble5_input_sel  : 3;
		mmr_t	reserved_10        : 1;
		mmr_t	nibble5_nibble_sel : 3;
		mmr_t	reserved_11        : 1;
		mmr_t	nibble6_input_sel  : 3;
		mmr_t	reserved_12        : 1;
		mmr_t	nibble6_nibble_sel : 3;
		mmr_t	reserved_13        : 1;
		mmr_t	nibble7_input_sel  : 3;
		mmr_t	reserved_14        : 1;
		mmr_t	nibble7_nibble_sel : 3;
		mmr_t	reserved_15        : 1;
	} sh_xn_iilb_debug_sel_s;
} sh_xn_iilb_debug_sel_u_t;
#else
typedef union sh_xn_iilb_debug_sel_u {
	mmr_t	sh_xn_iilb_debug_sel_regval;
	struct {
		mmr_t	reserved_15        : 1;
		mmr_t	nibble7_nibble_sel : 3;
		mmr_t	reserved_14        : 1;
		mmr_t	nibble7_input_sel  : 3;
		mmr_t	reserved_13        : 1;
		mmr_t	nibble6_nibble_sel : 3;
		mmr_t	reserved_12        : 1;
		mmr_t	nibble6_input_sel  : 3;
		mmr_t	reserved_11        : 1;
		mmr_t	nibble5_nibble_sel : 3;
		mmr_t	reserved_10        : 1;
		mmr_t	nibble5_input_sel  : 3;
		mmr_t	reserved_9         : 1;
		mmr_t	nibble4_nibble_sel : 3;
		mmr_t	reserved_8         : 1;
		mmr_t	nibble4_input_sel  : 3;
		mmr_t	reserved_7         : 1;
		mmr_t	nibble3_nibble_sel : 3;
		mmr_t	reserved_6         : 1;
		mmr_t	nibble3_input_sel  : 3;
		mmr_t	reserved_5         : 1;
		mmr_t	nibble2_nibble_sel : 3;
		mmr_t	reserved_4         : 1;
		mmr_t	nibble2_input_sel  : 3;
		mmr_t	reserved_3         : 1;
		mmr_t	nibble1_nibble_sel : 3;
		mmr_t	reserved_2         : 1;
		mmr_t	nibble1_input_sel  : 3;
		mmr_t	reserved_1         : 1;
		mmr_t	nibble0_nibble_sel : 3;
		mmr_t	reserved_0         : 1;
		mmr_t	nibble0_input_sel  : 3;
	} sh_xn_iilb_debug_sel_s;
} sh_xn_iilb_debug_sel_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_XN_PI_DEBUG_SEL"                     */
/*                       XN PI Debug Port Select                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_pi_debug_sel_u {
	mmr_t	sh_xn_pi_debug_sel_regval;
	struct {
		mmr_t	nibble0_input_sel  : 3;
		mmr_t	reserved_0         : 1;
		mmr_t	nibble0_nibble_sel : 3;
		mmr_t	reserved_1         : 1;
		mmr_t	nibble1_input_sel  : 3;
		mmr_t	reserved_2         : 1;
		mmr_t	nibble1_nibble_sel : 3;
		mmr_t	reserved_3         : 1;
		mmr_t	nibble2_input_sel  : 3;
		mmr_t	reserved_4         : 1;
		mmr_t	nibble2_nibble_sel : 3;
		mmr_t	reserved_5         : 1;
		mmr_t	nibble3_input_sel  : 3;
		mmr_t	reserved_6         : 1;
		mmr_t	nibble3_nibble_sel : 3;
		mmr_t	reserved_7         : 1;
		mmr_t	nibble4_input_sel  : 3;
		mmr_t	reserved_8         : 1;
		mmr_t	nibble4_nibble_sel : 3;
		mmr_t	reserved_9         : 1;
		mmr_t	nibble5_input_sel  : 3;
		mmr_t	reserved_10        : 1;
		mmr_t	nibble5_nibble_sel : 3;
		mmr_t	reserved_11        : 1;
		mmr_t	nibble6_input_sel  : 3;
		mmr_t	reserved_12        : 1;
		mmr_t	nibble6_nibble_sel : 3;
		mmr_t	reserved_13        : 1;
		mmr_t	nibble7_input_sel  : 3;
		mmr_t	reserved_14        : 1;
		mmr_t	nibble7_nibble_sel : 3;
		mmr_t	reserved_15        : 1;
	} sh_xn_pi_debug_sel_s;
} sh_xn_pi_debug_sel_u_t;
#else
typedef union sh_xn_pi_debug_sel_u {
	mmr_t	sh_xn_pi_debug_sel_regval;
	struct {
		mmr_t	reserved_15        : 1;
		mmr_t	nibble7_nibble_sel : 3;
		mmr_t	reserved_14        : 1;
		mmr_t	nibble7_input_sel  : 3;
		mmr_t	reserved_13        : 1;
		mmr_t	nibble6_nibble_sel : 3;
		mmr_t	reserved_12        : 1;
		mmr_t	nibble6_input_sel  : 3;
		mmr_t	reserved_11        : 1;
		mmr_t	nibble5_nibble_sel : 3;
		mmr_t	reserved_10        : 1;
		mmr_t	nibble5_input_sel  : 3;
		mmr_t	reserved_9         : 1;
		mmr_t	nibble4_nibble_sel : 3;
		mmr_t	reserved_8         : 1;
		mmr_t	nibble4_input_sel  : 3;
		mmr_t	reserved_7         : 1;
		mmr_t	nibble3_nibble_sel : 3;
		mmr_t	reserved_6         : 1;
		mmr_t	nibble3_input_sel  : 3;
		mmr_t	reserved_5         : 1;
		mmr_t	nibble2_nibble_sel : 3;
		mmr_t	reserved_4         : 1;
		mmr_t	nibble2_input_sel  : 3;
		mmr_t	reserved_3         : 1;
		mmr_t	nibble1_nibble_sel : 3;
		mmr_t	reserved_2         : 1;
		mmr_t	nibble1_input_sel  : 3;
		mmr_t	reserved_1         : 1;
		mmr_t	nibble0_nibble_sel : 3;
		mmr_t	reserved_0         : 1;
		mmr_t	nibble0_input_sel  : 3;
	} sh_xn_pi_debug_sel_s;
} sh_xn_pi_debug_sel_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_XN_MD_DEBUG_SEL"                     */
/*                       XN MD Debug Port Select                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_md_debug_sel_u {
	mmr_t	sh_xn_md_debug_sel_regval;
	struct {
		mmr_t	nibble0_input_sel  : 3;
		mmr_t	reserved_0         : 1;
		mmr_t	nibble0_nibble_sel : 3;
		mmr_t	reserved_1         : 1;
		mmr_t	nibble1_input_sel  : 3;
		mmr_t	reserved_2         : 1;
		mmr_t	nibble1_nibble_sel : 3;
		mmr_t	reserved_3         : 1;
		mmr_t	nibble2_input_sel  : 3;
		mmr_t	reserved_4         : 1;
		mmr_t	nibble2_nibble_sel : 3;
		mmr_t	reserved_5         : 1;
		mmr_t	nibble3_input_sel  : 3;
		mmr_t	reserved_6         : 1;
		mmr_t	nibble3_nibble_sel : 3;
		mmr_t	reserved_7         : 1;
		mmr_t	nibble4_input_sel  : 3;
		mmr_t	reserved_8         : 1;
		mmr_t	nibble4_nibble_sel : 3;
		mmr_t	reserved_9         : 1;
		mmr_t	nibble5_input_sel  : 3;
		mmr_t	reserved_10        : 1;
		mmr_t	nibble5_nibble_sel : 3;
		mmr_t	reserved_11        : 1;
		mmr_t	nibble6_input_sel  : 3;
		mmr_t	reserved_12        : 1;
		mmr_t	nibble6_nibble_sel : 3;
		mmr_t	reserved_13        : 1;
		mmr_t	nibble7_input_sel  : 3;
		mmr_t	reserved_14        : 1;
		mmr_t	nibble7_nibble_sel : 3;
		mmr_t	reserved_15        : 1;
	} sh_xn_md_debug_sel_s;
} sh_xn_md_debug_sel_u_t;
#else
typedef union sh_xn_md_debug_sel_u {
	mmr_t	sh_xn_md_debug_sel_regval;
	struct {
		mmr_t	reserved_15        : 1;
		mmr_t	nibble7_nibble_sel : 3;
		mmr_t	reserved_14        : 1;
		mmr_t	nibble7_input_sel  : 3;
		mmr_t	reserved_13        : 1;
		mmr_t	nibble6_nibble_sel : 3;
		mmr_t	reserved_12        : 1;
		mmr_t	nibble6_input_sel  : 3;
		mmr_t	reserved_11        : 1;
		mmr_t	nibble5_nibble_sel : 3;
		mmr_t	reserved_10        : 1;
		mmr_t	nibble5_input_sel  : 3;
		mmr_t	reserved_9         : 1;
		mmr_t	nibble4_nibble_sel : 3;
		mmr_t	reserved_8         : 1;
		mmr_t	nibble4_input_sel  : 3;
		mmr_t	reserved_7         : 1;
		mmr_t	nibble3_nibble_sel : 3;
		mmr_t	reserved_6         : 1;
		mmr_t	nibble3_input_sel  : 3;
		mmr_t	reserved_5         : 1;
		mmr_t	nibble2_nibble_sel : 3;
		mmr_t	reserved_4         : 1;
		mmr_t	nibble2_input_sel  : 3;
		mmr_t	reserved_3         : 1;
		mmr_t	nibble1_nibble_sel : 3;
		mmr_t	reserved_2         : 1;
		mmr_t	nibble1_input_sel  : 3;
		mmr_t	reserved_1         : 1;
		mmr_t	nibble0_nibble_sel : 3;
		mmr_t	reserved_0         : 1;
		mmr_t	nibble0_input_sel  : 3;
	} sh_xn_md_debug_sel_s;
} sh_xn_md_debug_sel_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_XN_NI0_DEBUG_SEL"                    */
/*                       XN NI0 Debug Port Select                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni0_debug_sel_u {
	mmr_t	sh_xn_ni0_debug_sel_regval;
	struct {
		mmr_t	nibble0_input_sel  : 3;
		mmr_t	reserved_0         : 1;
		mmr_t	nibble0_nibble_sel : 3;
		mmr_t	reserved_1         : 1;
		mmr_t	nibble1_input_sel  : 3;
		mmr_t	reserved_2         : 1;
		mmr_t	nibble1_nibble_sel : 3;
		mmr_t	reserved_3         : 1;
		mmr_t	nibble2_input_sel  : 3;
		mmr_t	reserved_4         : 1;
		mmr_t	nibble2_nibble_sel : 3;
		mmr_t	reserved_5         : 1;
		mmr_t	nibble3_input_sel  : 3;
		mmr_t	reserved_6         : 1;
		mmr_t	nibble3_nibble_sel : 3;
		mmr_t	reserved_7         : 1;
		mmr_t	nibble4_input_sel  : 3;
		mmr_t	reserved_8         : 1;
		mmr_t	nibble4_nibble_sel : 3;
		mmr_t	reserved_9         : 1;
		mmr_t	nibble5_input_sel  : 3;
		mmr_t	reserved_10        : 1;
		mmr_t	nibble5_nibble_sel : 3;
		mmr_t	reserved_11        : 1;
		mmr_t	nibble6_input_sel  : 3;
		mmr_t	reserved_12        : 1;
		mmr_t	nibble6_nibble_sel : 3;
		mmr_t	reserved_13        : 1;
		mmr_t	nibble7_input_sel  : 3;
		mmr_t	reserved_14        : 1;
		mmr_t	nibble7_nibble_sel : 3;
		mmr_t	reserved_15        : 1;
	} sh_xn_ni0_debug_sel_s;
} sh_xn_ni0_debug_sel_u_t;
#else
typedef union sh_xn_ni0_debug_sel_u {
	mmr_t	sh_xn_ni0_debug_sel_regval;
	struct {
		mmr_t	reserved_15        : 1;
		mmr_t	nibble7_nibble_sel : 3;
		mmr_t	reserved_14        : 1;
		mmr_t	nibble7_input_sel  : 3;
		mmr_t	reserved_13        : 1;
		mmr_t	nibble6_nibble_sel : 3;
		mmr_t	reserved_12        : 1;
		mmr_t	nibble6_input_sel  : 3;
		mmr_t	reserved_11        : 1;
		mmr_t	nibble5_nibble_sel : 3;
		mmr_t	reserved_10        : 1;
		mmr_t	nibble5_input_sel  : 3;
		mmr_t	reserved_9         : 1;
		mmr_t	nibble4_nibble_sel : 3;
		mmr_t	reserved_8         : 1;
		mmr_t	nibble4_input_sel  : 3;
		mmr_t	reserved_7         : 1;
		mmr_t	nibble3_nibble_sel : 3;
		mmr_t	reserved_6         : 1;
		mmr_t	nibble3_input_sel  : 3;
		mmr_t	reserved_5         : 1;
		mmr_t	nibble2_nibble_sel : 3;
		mmr_t	reserved_4         : 1;
		mmr_t	nibble2_input_sel  : 3;
		mmr_t	reserved_3         : 1;
		mmr_t	nibble1_nibble_sel : 3;
		mmr_t	reserved_2         : 1;
		mmr_t	nibble1_input_sel  : 3;
		mmr_t	reserved_1         : 1;
		mmr_t	nibble0_nibble_sel : 3;
		mmr_t	reserved_0         : 1;
		mmr_t	nibble0_input_sel  : 3;
	} sh_xn_ni0_debug_sel_s;
} sh_xn_ni0_debug_sel_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_XN_NI1_DEBUG_SEL"                    */
/*                       XN NI1 Debug Port Select                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni1_debug_sel_u {
	mmr_t	sh_xn_ni1_debug_sel_regval;
	struct {
		mmr_t	nibble0_input_sel  : 3;
		mmr_t	reserved_0         : 1;
		mmr_t	nibble0_nibble_sel : 3;
		mmr_t	reserved_1         : 1;
		mmr_t	nibble1_input_sel  : 3;
		mmr_t	reserved_2         : 1;
		mmr_t	nibble1_nibble_sel : 3;
		mmr_t	reserved_3         : 1;
		mmr_t	nibble2_input_sel  : 3;
		mmr_t	reserved_4         : 1;
		mmr_t	nibble2_nibble_sel : 3;
		mmr_t	reserved_5         : 1;
		mmr_t	nibble3_input_sel  : 3;
		mmr_t	reserved_6         : 1;
		mmr_t	nibble3_nibble_sel : 3;
		mmr_t	reserved_7         : 1;
		mmr_t	nibble4_input_sel  : 3;
		mmr_t	reserved_8         : 1;
		mmr_t	nibble4_nibble_sel : 3;
		mmr_t	reserved_9         : 1;
		mmr_t	nibble5_input_sel  : 3;
		mmr_t	reserved_10        : 1;
		mmr_t	nibble5_nibble_sel : 3;
		mmr_t	reserved_11        : 1;
		mmr_t	nibble6_input_sel  : 3;
		mmr_t	reserved_12        : 1;
		mmr_t	nibble6_nibble_sel : 3;
		mmr_t	reserved_13        : 1;
		mmr_t	nibble7_input_sel  : 3;
		mmr_t	reserved_14        : 1;
		mmr_t	nibble7_nibble_sel : 3;
		mmr_t	reserved_15        : 1;
	} sh_xn_ni1_debug_sel_s;
} sh_xn_ni1_debug_sel_u_t;
#else
typedef union sh_xn_ni1_debug_sel_u {
	mmr_t	sh_xn_ni1_debug_sel_regval;
	struct {
		mmr_t	reserved_15        : 1;
		mmr_t	nibble7_nibble_sel : 3;
		mmr_t	reserved_14        : 1;
		mmr_t	nibble7_input_sel  : 3;
		mmr_t	reserved_13        : 1;
		mmr_t	nibble6_nibble_sel : 3;
		mmr_t	reserved_12        : 1;
		mmr_t	nibble6_input_sel  : 3;
		mmr_t	reserved_11        : 1;
		mmr_t	nibble5_nibble_sel : 3;
		mmr_t	reserved_10        : 1;
		mmr_t	nibble5_input_sel  : 3;
		mmr_t	reserved_9         : 1;
		mmr_t	nibble4_nibble_sel : 3;
		mmr_t	reserved_8         : 1;
		mmr_t	nibble4_input_sel  : 3;
		mmr_t	reserved_7         : 1;
		mmr_t	nibble3_nibble_sel : 3;
		mmr_t	reserved_6         : 1;
		mmr_t	nibble3_input_sel  : 3;
		mmr_t	reserved_5         : 1;
		mmr_t	nibble2_nibble_sel : 3;
		mmr_t	reserved_4         : 1;
		mmr_t	nibble2_input_sel  : 3;
		mmr_t	reserved_3         : 1;
		mmr_t	nibble1_nibble_sel : 3;
		mmr_t	reserved_2         : 1;
		mmr_t	nibble1_input_sel  : 3;
		mmr_t	reserved_1         : 1;
		mmr_t	nibble0_nibble_sel : 3;
		mmr_t	reserved_0         : 1;
		mmr_t	nibble0_input_sel  : 3;
	} sh_xn_ni1_debug_sel_s;
} sh_xn_ni1_debug_sel_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_IILB_LB_CMP_EXP_DATA0"                */
/*                 IILB compare LB input expected data0                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_iilb_lb_cmp_exp_data0_u {
	mmr_t	sh_xn_iilb_lb_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_iilb_lb_cmp_exp_data0_s;
} sh_xn_iilb_lb_cmp_exp_data0_u_t;
#else
typedef union sh_xn_iilb_lb_cmp_exp_data0_u {
	mmr_t	sh_xn_iilb_lb_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_iilb_lb_cmp_exp_data0_s;
} sh_xn_iilb_lb_cmp_exp_data0_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_IILB_LB_CMP_EXP_DATA1"                */
/*                 IILB compare LB input expected data1                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_iilb_lb_cmp_exp_data1_u {
	mmr_t	sh_xn_iilb_lb_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_iilb_lb_cmp_exp_data1_s;
} sh_xn_iilb_lb_cmp_exp_data1_u_t;
#else
typedef union sh_xn_iilb_lb_cmp_exp_data1_u {
	mmr_t	sh_xn_iilb_lb_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_iilb_lb_cmp_exp_data1_s;
} sh_xn_iilb_lb_cmp_exp_data1_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_IILB_LB_CMP_ENABLE0"                 */
/*                    IILB compare LB input enable0                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_iilb_lb_cmp_enable0_u {
	mmr_t	sh_xn_iilb_lb_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_iilb_lb_cmp_enable0_s;
} sh_xn_iilb_lb_cmp_enable0_u_t;
#else
typedef union sh_xn_iilb_lb_cmp_enable0_u {
	mmr_t	sh_xn_iilb_lb_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_iilb_lb_cmp_enable0_s;
} sh_xn_iilb_lb_cmp_enable0_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_IILB_LB_CMP_ENABLE1"                 */
/*                    IILB compare LB input enable1                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_iilb_lb_cmp_enable1_u {
	mmr_t	sh_xn_iilb_lb_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_iilb_lb_cmp_enable1_s;
} sh_xn_iilb_lb_cmp_enable1_u_t;
#else
typedef union sh_xn_iilb_lb_cmp_enable1_u {
	mmr_t	sh_xn_iilb_lb_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_iilb_lb_cmp_enable1_s;
} sh_xn_iilb_lb_cmp_enable1_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_IILB_II_CMP_EXP_DATA0"                */
/*                 IILB compare II input expected data0                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_iilb_ii_cmp_exp_data0_u {
	mmr_t	sh_xn_iilb_ii_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_iilb_ii_cmp_exp_data0_s;
} sh_xn_iilb_ii_cmp_exp_data0_u_t;
#else
typedef union sh_xn_iilb_ii_cmp_exp_data0_u {
	mmr_t	sh_xn_iilb_ii_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_iilb_ii_cmp_exp_data0_s;
} sh_xn_iilb_ii_cmp_exp_data0_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_IILB_II_CMP_EXP_DATA1"                */
/*                 IILB compare II input expected data1                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_iilb_ii_cmp_exp_data1_u {
	mmr_t	sh_xn_iilb_ii_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_iilb_ii_cmp_exp_data1_s;
} sh_xn_iilb_ii_cmp_exp_data1_u_t;
#else
typedef union sh_xn_iilb_ii_cmp_exp_data1_u {
	mmr_t	sh_xn_iilb_ii_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_iilb_ii_cmp_exp_data1_s;
} sh_xn_iilb_ii_cmp_exp_data1_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_IILB_II_CMP_ENABLE0"                 */
/*                    IILB compare II input enable0                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_iilb_ii_cmp_enable0_u {
	mmr_t	sh_xn_iilb_ii_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_iilb_ii_cmp_enable0_s;
} sh_xn_iilb_ii_cmp_enable0_u_t;
#else
typedef union sh_xn_iilb_ii_cmp_enable0_u {
	mmr_t	sh_xn_iilb_ii_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_iilb_ii_cmp_enable0_s;
} sh_xn_iilb_ii_cmp_enable0_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_IILB_II_CMP_ENABLE1"                 */
/*                    IILB compare II input enable1                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_iilb_ii_cmp_enable1_u {
	mmr_t	sh_xn_iilb_ii_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_iilb_ii_cmp_enable1_s;
} sh_xn_iilb_ii_cmp_enable1_u_t;
#else
typedef union sh_xn_iilb_ii_cmp_enable1_u {
	mmr_t	sh_xn_iilb_ii_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_iilb_ii_cmp_enable1_s;
} sh_xn_iilb_ii_cmp_enable1_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_IILB_MD_CMP_EXP_DATA0"                */
/*                 IILB compare MD input expected data0                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_iilb_md_cmp_exp_data0_u {
	mmr_t	sh_xn_iilb_md_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_iilb_md_cmp_exp_data0_s;
} sh_xn_iilb_md_cmp_exp_data0_u_t;
#else
typedef union sh_xn_iilb_md_cmp_exp_data0_u {
	mmr_t	sh_xn_iilb_md_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_iilb_md_cmp_exp_data0_s;
} sh_xn_iilb_md_cmp_exp_data0_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_IILB_MD_CMP_EXP_DATA1"                */
/*                 IILB compare MD input expected data1                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_iilb_md_cmp_exp_data1_u {
	mmr_t	sh_xn_iilb_md_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_iilb_md_cmp_exp_data1_s;
} sh_xn_iilb_md_cmp_exp_data1_u_t;
#else
typedef union sh_xn_iilb_md_cmp_exp_data1_u {
	mmr_t	sh_xn_iilb_md_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_iilb_md_cmp_exp_data1_s;
} sh_xn_iilb_md_cmp_exp_data1_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_IILB_MD_CMP_ENABLE0"                 */
/*                    IILB compare MD input enable0                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_iilb_md_cmp_enable0_u {
	mmr_t	sh_xn_iilb_md_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_iilb_md_cmp_enable0_s;
} sh_xn_iilb_md_cmp_enable0_u_t;
#else
typedef union sh_xn_iilb_md_cmp_enable0_u {
	mmr_t	sh_xn_iilb_md_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_iilb_md_cmp_enable0_s;
} sh_xn_iilb_md_cmp_enable0_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_IILB_MD_CMP_ENABLE1"                 */
/*                    IILB compare MD input enable1                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_iilb_md_cmp_enable1_u {
	mmr_t	sh_xn_iilb_md_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_iilb_md_cmp_enable1_s;
} sh_xn_iilb_md_cmp_enable1_u_t;
#else
typedef union sh_xn_iilb_md_cmp_enable1_u {
	mmr_t	sh_xn_iilb_md_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_iilb_md_cmp_enable1_s;
} sh_xn_iilb_md_cmp_enable1_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_IILB_PI_CMP_EXP_DATA0"                */
/*                 IILB compare PI input expected data0                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_iilb_pi_cmp_exp_data0_u {
	mmr_t	sh_xn_iilb_pi_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_iilb_pi_cmp_exp_data0_s;
} sh_xn_iilb_pi_cmp_exp_data0_u_t;
#else
typedef union sh_xn_iilb_pi_cmp_exp_data0_u {
	mmr_t	sh_xn_iilb_pi_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_iilb_pi_cmp_exp_data0_s;
} sh_xn_iilb_pi_cmp_exp_data0_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_IILB_PI_CMP_EXP_DATA1"                */
/*                 IILB compare PI input expected data1                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_iilb_pi_cmp_exp_data1_u {
	mmr_t	sh_xn_iilb_pi_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_iilb_pi_cmp_exp_data1_s;
} sh_xn_iilb_pi_cmp_exp_data1_u_t;
#else
typedef union sh_xn_iilb_pi_cmp_exp_data1_u {
	mmr_t	sh_xn_iilb_pi_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_iilb_pi_cmp_exp_data1_s;
} sh_xn_iilb_pi_cmp_exp_data1_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_IILB_PI_CMP_ENABLE0"                 */
/*                    IILB compare PI input enable0                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_iilb_pi_cmp_enable0_u {
	mmr_t	sh_xn_iilb_pi_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_iilb_pi_cmp_enable0_s;
} sh_xn_iilb_pi_cmp_enable0_u_t;
#else
typedef union sh_xn_iilb_pi_cmp_enable0_u {
	mmr_t	sh_xn_iilb_pi_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_iilb_pi_cmp_enable0_s;
} sh_xn_iilb_pi_cmp_enable0_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_IILB_PI_CMP_ENABLE1"                 */
/*                    IILB compare PI input enable1                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_iilb_pi_cmp_enable1_u {
	mmr_t	sh_xn_iilb_pi_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_iilb_pi_cmp_enable1_s;
} sh_xn_iilb_pi_cmp_enable1_u_t;
#else
typedef union sh_xn_iilb_pi_cmp_enable1_u {
	mmr_t	sh_xn_iilb_pi_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_iilb_pi_cmp_enable1_s;
} sh_xn_iilb_pi_cmp_enable1_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_XN_IILB_NI0_CMP_EXP_DATA0"                */
/*                IILB compare NI0 input expected data0                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_iilb_ni0_cmp_exp_data0_u {
	mmr_t	sh_xn_iilb_ni0_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_iilb_ni0_cmp_exp_data0_s;
} sh_xn_iilb_ni0_cmp_exp_data0_u_t;
#else
typedef union sh_xn_iilb_ni0_cmp_exp_data0_u {
	mmr_t	sh_xn_iilb_ni0_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_iilb_ni0_cmp_exp_data0_s;
} sh_xn_iilb_ni0_cmp_exp_data0_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_XN_IILB_NI0_CMP_EXP_DATA1"                */
/*                IILB compare NI0 input expected data1                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_iilb_ni0_cmp_exp_data1_u {
	mmr_t	sh_xn_iilb_ni0_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_iilb_ni0_cmp_exp_data1_s;
} sh_xn_iilb_ni0_cmp_exp_data1_u_t;
#else
typedef union sh_xn_iilb_ni0_cmp_exp_data1_u {
	mmr_t	sh_xn_iilb_ni0_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_iilb_ni0_cmp_exp_data1_s;
} sh_xn_iilb_ni0_cmp_exp_data1_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_IILB_NI0_CMP_ENABLE0"                 */
/*                    IILB compare NI0 input enable0                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_iilb_ni0_cmp_enable0_u {
	mmr_t	sh_xn_iilb_ni0_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_iilb_ni0_cmp_enable0_s;
} sh_xn_iilb_ni0_cmp_enable0_u_t;
#else
typedef union sh_xn_iilb_ni0_cmp_enable0_u {
	mmr_t	sh_xn_iilb_ni0_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_iilb_ni0_cmp_enable0_s;
} sh_xn_iilb_ni0_cmp_enable0_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_IILB_NI0_CMP_ENABLE1"                 */
/*                    IILB compare NI0 input enable1                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_iilb_ni0_cmp_enable1_u {
	mmr_t	sh_xn_iilb_ni0_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_iilb_ni0_cmp_enable1_s;
} sh_xn_iilb_ni0_cmp_enable1_u_t;
#else
typedef union sh_xn_iilb_ni0_cmp_enable1_u {
	mmr_t	sh_xn_iilb_ni0_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_iilb_ni0_cmp_enable1_s;
} sh_xn_iilb_ni0_cmp_enable1_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_XN_IILB_NI1_CMP_EXP_DATA0"                */
/*                IILB compare NI1 input expected data0                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_iilb_ni1_cmp_exp_data0_u {
	mmr_t	sh_xn_iilb_ni1_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_iilb_ni1_cmp_exp_data0_s;
} sh_xn_iilb_ni1_cmp_exp_data0_u_t;
#else
typedef union sh_xn_iilb_ni1_cmp_exp_data0_u {
	mmr_t	sh_xn_iilb_ni1_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_iilb_ni1_cmp_exp_data0_s;
} sh_xn_iilb_ni1_cmp_exp_data0_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_XN_IILB_NI1_CMP_EXP_DATA1"                */
/*                IILB compare NI1 input expected data1                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_iilb_ni1_cmp_exp_data1_u {
	mmr_t	sh_xn_iilb_ni1_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_iilb_ni1_cmp_exp_data1_s;
} sh_xn_iilb_ni1_cmp_exp_data1_u_t;
#else
typedef union sh_xn_iilb_ni1_cmp_exp_data1_u {
	mmr_t	sh_xn_iilb_ni1_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_iilb_ni1_cmp_exp_data1_s;
} sh_xn_iilb_ni1_cmp_exp_data1_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_IILB_NI1_CMP_ENABLE0"                 */
/*                    IILB compare NI1 input enable0                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_iilb_ni1_cmp_enable0_u {
	mmr_t	sh_xn_iilb_ni1_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_iilb_ni1_cmp_enable0_s;
} sh_xn_iilb_ni1_cmp_enable0_u_t;
#else
typedef union sh_xn_iilb_ni1_cmp_enable0_u {
	mmr_t	sh_xn_iilb_ni1_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_iilb_ni1_cmp_enable0_s;
} sh_xn_iilb_ni1_cmp_enable0_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_IILB_NI1_CMP_ENABLE1"                 */
/*                    IILB compare NI1 input enable1                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_iilb_ni1_cmp_enable1_u {
	mmr_t	sh_xn_iilb_ni1_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_iilb_ni1_cmp_enable1_s;
} sh_xn_iilb_ni1_cmp_enable1_u_t;
#else
typedef union sh_xn_iilb_ni1_cmp_enable1_u {
	mmr_t	sh_xn_iilb_ni1_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_iilb_ni1_cmp_enable1_s;
} sh_xn_iilb_ni1_cmp_enable1_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_MD_IILB_CMP_EXP_DATA0"                */
/*                 MD compare IILB input expected data0                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_md_iilb_cmp_exp_data0_u {
	mmr_t	sh_xn_md_iilb_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_md_iilb_cmp_exp_data0_s;
} sh_xn_md_iilb_cmp_exp_data0_u_t;
#else
typedef union sh_xn_md_iilb_cmp_exp_data0_u {
	mmr_t	sh_xn_md_iilb_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_md_iilb_cmp_exp_data0_s;
} sh_xn_md_iilb_cmp_exp_data0_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_MD_IILB_CMP_EXP_DATA1"                */
/*                 MD compare IILB input expected data1                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_md_iilb_cmp_exp_data1_u {
	mmr_t	sh_xn_md_iilb_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_md_iilb_cmp_exp_data1_s;
} sh_xn_md_iilb_cmp_exp_data1_u_t;
#else
typedef union sh_xn_md_iilb_cmp_exp_data1_u {
	mmr_t	sh_xn_md_iilb_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_md_iilb_cmp_exp_data1_s;
} sh_xn_md_iilb_cmp_exp_data1_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_MD_IILB_CMP_ENABLE0"                 */
/*                    MD compare IILB input enable0                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_md_iilb_cmp_enable0_u {
	mmr_t	sh_xn_md_iilb_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_md_iilb_cmp_enable0_s;
} sh_xn_md_iilb_cmp_enable0_u_t;
#else
typedef union sh_xn_md_iilb_cmp_enable0_u {
	mmr_t	sh_xn_md_iilb_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_md_iilb_cmp_enable0_s;
} sh_xn_md_iilb_cmp_enable0_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_MD_IILB_CMP_ENABLE1"                 */
/*                    MD compare IILB input enable1                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_md_iilb_cmp_enable1_u {
	mmr_t	sh_xn_md_iilb_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_md_iilb_cmp_enable1_s;
} sh_xn_md_iilb_cmp_enable1_u_t;
#else
typedef union sh_xn_md_iilb_cmp_enable1_u {
	mmr_t	sh_xn_md_iilb_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_md_iilb_cmp_enable1_s;
} sh_xn_md_iilb_cmp_enable1_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_MD_NI0_CMP_EXP_DATA0"                 */
/*                 MD compare NI0 input expected data0                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_md_ni0_cmp_exp_data0_u {
	mmr_t	sh_xn_md_ni0_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_md_ni0_cmp_exp_data0_s;
} sh_xn_md_ni0_cmp_exp_data0_u_t;
#else
typedef union sh_xn_md_ni0_cmp_exp_data0_u {
	mmr_t	sh_xn_md_ni0_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_md_ni0_cmp_exp_data0_s;
} sh_xn_md_ni0_cmp_exp_data0_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_MD_NI0_CMP_EXP_DATA1"                 */
/*                 MD compare NI0 input expected data1                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_md_ni0_cmp_exp_data1_u {
	mmr_t	sh_xn_md_ni0_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_md_ni0_cmp_exp_data1_s;
} sh_xn_md_ni0_cmp_exp_data1_u_t;
#else
typedef union sh_xn_md_ni0_cmp_exp_data1_u {
	mmr_t	sh_xn_md_ni0_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_md_ni0_cmp_exp_data1_s;
} sh_xn_md_ni0_cmp_exp_data1_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_MD_NI0_CMP_ENABLE0"                  */
/*                     MD compare NI0 input enable0                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_md_ni0_cmp_enable0_u {
	mmr_t	sh_xn_md_ni0_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_md_ni0_cmp_enable0_s;
} sh_xn_md_ni0_cmp_enable0_u_t;
#else
typedef union sh_xn_md_ni0_cmp_enable0_u {
	mmr_t	sh_xn_md_ni0_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_md_ni0_cmp_enable0_s;
} sh_xn_md_ni0_cmp_enable0_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_MD_NI0_CMP_ENABLE1"                  */
/*                     MD compare NI0 input enable1                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_md_ni0_cmp_enable1_u {
	mmr_t	sh_xn_md_ni0_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_md_ni0_cmp_enable1_s;
} sh_xn_md_ni0_cmp_enable1_u_t;
#else
typedef union sh_xn_md_ni0_cmp_enable1_u {
	mmr_t	sh_xn_md_ni0_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_md_ni0_cmp_enable1_s;
} sh_xn_md_ni0_cmp_enable1_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_MD_NI1_CMP_EXP_DATA0"                 */
/*                 MD compare NI1 input expected data0                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_md_ni1_cmp_exp_data0_u {
	mmr_t	sh_xn_md_ni1_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_md_ni1_cmp_exp_data0_s;
} sh_xn_md_ni1_cmp_exp_data0_u_t;
#else
typedef union sh_xn_md_ni1_cmp_exp_data0_u {
	mmr_t	sh_xn_md_ni1_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_md_ni1_cmp_exp_data0_s;
} sh_xn_md_ni1_cmp_exp_data0_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_MD_NI1_CMP_EXP_DATA1"                 */
/*                 MD compare NI1 input expected data1                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_md_ni1_cmp_exp_data1_u {
	mmr_t	sh_xn_md_ni1_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_md_ni1_cmp_exp_data1_s;
} sh_xn_md_ni1_cmp_exp_data1_u_t;
#else
typedef union sh_xn_md_ni1_cmp_exp_data1_u {
	mmr_t	sh_xn_md_ni1_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_md_ni1_cmp_exp_data1_s;
} sh_xn_md_ni1_cmp_exp_data1_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_MD_NI1_CMP_ENABLE0"                  */
/*                     MD compare NI1 input enable0                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_md_ni1_cmp_enable0_u {
	mmr_t	sh_xn_md_ni1_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_md_ni1_cmp_enable0_s;
} sh_xn_md_ni1_cmp_enable0_u_t;
#else
typedef union sh_xn_md_ni1_cmp_enable0_u {
	mmr_t	sh_xn_md_ni1_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_md_ni1_cmp_enable0_s;
} sh_xn_md_ni1_cmp_enable0_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_MD_NI1_CMP_ENABLE1"                  */
/*                     MD compare NI1 input enable1                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_md_ni1_cmp_enable1_u {
	mmr_t	sh_xn_md_ni1_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_md_ni1_cmp_enable1_s;
} sh_xn_md_ni1_cmp_enable1_u_t;
#else
typedef union sh_xn_md_ni1_cmp_enable1_u {
	mmr_t	sh_xn_md_ni1_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_md_ni1_cmp_enable1_s;
} sh_xn_md_ni1_cmp_enable1_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_MD_SIC_CMP_EXP_HDR0"                 */
/*                MD compare SIC input expected header0                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_md_sic_cmp_exp_hdr0_u {
	mmr_t	sh_xn_md_sic_cmp_exp_hdr0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_md_sic_cmp_exp_hdr0_s;
} sh_xn_md_sic_cmp_exp_hdr0_u_t;
#else
typedef union sh_xn_md_sic_cmp_exp_hdr0_u {
	mmr_t	sh_xn_md_sic_cmp_exp_hdr0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_md_sic_cmp_exp_hdr0_s;
} sh_xn_md_sic_cmp_exp_hdr0_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_MD_SIC_CMP_EXP_HDR1"                 */
/*                MD compare SIC input expected header1                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_md_sic_cmp_exp_hdr1_u {
	mmr_t	sh_xn_md_sic_cmp_exp_hdr1_regval;
	struct {
		mmr_t	data        : 42;
		mmr_t	reserved_0  : 22;
	} sh_xn_md_sic_cmp_exp_hdr1_s;
} sh_xn_md_sic_cmp_exp_hdr1_u_t;
#else
typedef union sh_xn_md_sic_cmp_exp_hdr1_u {
	mmr_t	sh_xn_md_sic_cmp_exp_hdr1_regval;
	struct {
		mmr_t	reserved_0  : 22;
		mmr_t	data        : 42;
	} sh_xn_md_sic_cmp_exp_hdr1_s;
} sh_xn_md_sic_cmp_exp_hdr1_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_XN_MD_SIC_CMP_HDR_ENABLE0"                */
/*                    MD compare SIC header enable0                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_md_sic_cmp_hdr_enable0_u {
	mmr_t	sh_xn_md_sic_cmp_hdr_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_md_sic_cmp_hdr_enable0_s;
} sh_xn_md_sic_cmp_hdr_enable0_u_t;
#else
typedef union sh_xn_md_sic_cmp_hdr_enable0_u {
	mmr_t	sh_xn_md_sic_cmp_hdr_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_md_sic_cmp_hdr_enable0_s;
} sh_xn_md_sic_cmp_hdr_enable0_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_XN_MD_SIC_CMP_HDR_ENABLE1"                */
/*                    MD compare SIC header enable1                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_md_sic_cmp_hdr_enable1_u {
	mmr_t	sh_xn_md_sic_cmp_hdr_enable1_regval;
	struct {
		mmr_t	enable      : 42;
		mmr_t	reserved_0  : 22;
	} sh_xn_md_sic_cmp_hdr_enable1_s;
} sh_xn_md_sic_cmp_hdr_enable1_u_t;
#else
typedef union sh_xn_md_sic_cmp_hdr_enable1_u {
	mmr_t	sh_xn_md_sic_cmp_hdr_enable1_regval;
	struct {
		mmr_t	reserved_0  : 22;
		mmr_t	enable      : 42;
	} sh_xn_md_sic_cmp_hdr_enable1_s;
} sh_xn_md_sic_cmp_hdr_enable1_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_XN_MD_SIC_CMP_DATA0"                   */
/*                         MD compare SIC data0                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_md_sic_cmp_data0_u {
	mmr_t	sh_xn_md_sic_cmp_data0_regval;
	struct {
		mmr_t	data0       : 64;
	} sh_xn_md_sic_cmp_data0_s;
} sh_xn_md_sic_cmp_data0_u_t;
#else
typedef union sh_xn_md_sic_cmp_data0_u {
	mmr_t	sh_xn_md_sic_cmp_data0_regval;
	struct {
		mmr_t	data0       : 64;
	} sh_xn_md_sic_cmp_data0_s;
} sh_xn_md_sic_cmp_data0_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_XN_MD_SIC_CMP_DATA1"                   */
/*                         MD compare SIC data1                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_md_sic_cmp_data1_u {
	mmr_t	sh_xn_md_sic_cmp_data1_regval;
	struct {
		mmr_t	data1       : 64;
	} sh_xn_md_sic_cmp_data1_s;
} sh_xn_md_sic_cmp_data1_u_t;
#else
typedef union sh_xn_md_sic_cmp_data1_u {
	mmr_t	sh_xn_md_sic_cmp_data1_regval;
	struct {
		mmr_t	data1       : 64;
	} sh_xn_md_sic_cmp_data1_s;
} sh_xn_md_sic_cmp_data1_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_XN_MD_SIC_CMP_DATA2"                   */
/*                         MD compare SIC data2                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_md_sic_cmp_data2_u {
	mmr_t	sh_xn_md_sic_cmp_data2_regval;
	struct {
		mmr_t	data2       : 64;
	} sh_xn_md_sic_cmp_data2_s;
} sh_xn_md_sic_cmp_data2_u_t;
#else
typedef union sh_xn_md_sic_cmp_data2_u {
	mmr_t	sh_xn_md_sic_cmp_data2_regval;
	struct {
		mmr_t	data2       : 64;
	} sh_xn_md_sic_cmp_data2_s;
} sh_xn_md_sic_cmp_data2_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_XN_MD_SIC_CMP_DATA3"                   */
/*                         MD compare SIC data3                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_md_sic_cmp_data3_u {
	mmr_t	sh_xn_md_sic_cmp_data3_regval;
	struct {
		mmr_t	data3       : 64;
	} sh_xn_md_sic_cmp_data3_s;
} sh_xn_md_sic_cmp_data3_u_t;
#else
typedef union sh_xn_md_sic_cmp_data3_u {
	mmr_t	sh_xn_md_sic_cmp_data3_regval;
	struct {
		mmr_t	data3       : 64;
	} sh_xn_md_sic_cmp_data3_s;
} sh_xn_md_sic_cmp_data3_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_XN_MD_SIC_CMP_DATA_ENABLE0"               */
/*                     MD enable compare SIC data0                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_md_sic_cmp_data_enable0_u {
	mmr_t	sh_xn_md_sic_cmp_data_enable0_regval;
	struct {
		mmr_t	data_enable0 : 64;
	} sh_xn_md_sic_cmp_data_enable0_s;
} sh_xn_md_sic_cmp_data_enable0_u_t;
#else
typedef union sh_xn_md_sic_cmp_data_enable0_u {
	mmr_t	sh_xn_md_sic_cmp_data_enable0_regval;
	struct {
		mmr_t	data_enable0 : 64;
	} sh_xn_md_sic_cmp_data_enable0_s;
} sh_xn_md_sic_cmp_data_enable0_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_XN_MD_SIC_CMP_DATA_ENABLE1"               */
/*                     MD enable compare SIC data1                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_md_sic_cmp_data_enable1_u {
	mmr_t	sh_xn_md_sic_cmp_data_enable1_regval;
	struct {
		mmr_t	data_enable1 : 64;
	} sh_xn_md_sic_cmp_data_enable1_s;
} sh_xn_md_sic_cmp_data_enable1_u_t;
#else
typedef union sh_xn_md_sic_cmp_data_enable1_u {
	mmr_t	sh_xn_md_sic_cmp_data_enable1_regval;
	struct {
		mmr_t	data_enable1 : 64;
	} sh_xn_md_sic_cmp_data_enable1_s;
} sh_xn_md_sic_cmp_data_enable1_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_XN_MD_SIC_CMP_DATA_ENABLE2"               */
/*                     MD enable compare SIC data2                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_md_sic_cmp_data_enable2_u {
	mmr_t	sh_xn_md_sic_cmp_data_enable2_regval;
	struct {
		mmr_t	data_enable2 : 64;
	} sh_xn_md_sic_cmp_data_enable2_s;
} sh_xn_md_sic_cmp_data_enable2_u_t;
#else
typedef union sh_xn_md_sic_cmp_data_enable2_u {
	mmr_t	sh_xn_md_sic_cmp_data_enable2_regval;
	struct {
		mmr_t	data_enable2 : 64;
	} sh_xn_md_sic_cmp_data_enable2_s;
} sh_xn_md_sic_cmp_data_enable2_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_XN_MD_SIC_CMP_DATA_ENABLE3"               */
/*                     MD enable compare SIC data3                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_md_sic_cmp_data_enable3_u {
	mmr_t	sh_xn_md_sic_cmp_data_enable3_regval;
	struct {
		mmr_t	data_enable3 : 64;
	} sh_xn_md_sic_cmp_data_enable3_s;
} sh_xn_md_sic_cmp_data_enable3_u_t;
#else
typedef union sh_xn_md_sic_cmp_data_enable3_u {
	mmr_t	sh_xn_md_sic_cmp_data_enable3_regval;
	struct {
		mmr_t	data_enable3 : 64;
	} sh_xn_md_sic_cmp_data_enable3_s;
} sh_xn_md_sic_cmp_data_enable3_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_PI_IILB_CMP_EXP_DATA0"                */
/*                 PI compare IILB input expected data0                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_pi_iilb_cmp_exp_data0_u {
	mmr_t	sh_xn_pi_iilb_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_pi_iilb_cmp_exp_data0_s;
} sh_xn_pi_iilb_cmp_exp_data0_u_t;
#else
typedef union sh_xn_pi_iilb_cmp_exp_data0_u {
	mmr_t	sh_xn_pi_iilb_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_pi_iilb_cmp_exp_data0_s;
} sh_xn_pi_iilb_cmp_exp_data0_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_PI_IILB_CMP_EXP_DATA1"                */
/*                 PI compare IILB input expected data1                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_pi_iilb_cmp_exp_data1_u {
	mmr_t	sh_xn_pi_iilb_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_pi_iilb_cmp_exp_data1_s;
} sh_xn_pi_iilb_cmp_exp_data1_u_t;
#else
typedef union sh_xn_pi_iilb_cmp_exp_data1_u {
	mmr_t	sh_xn_pi_iilb_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_pi_iilb_cmp_exp_data1_s;
} sh_xn_pi_iilb_cmp_exp_data1_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_PI_IILB_CMP_ENABLE0"                 */
/*                    PI compare IILB input enable0                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_pi_iilb_cmp_enable0_u {
	mmr_t	sh_xn_pi_iilb_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_pi_iilb_cmp_enable0_s;
} sh_xn_pi_iilb_cmp_enable0_u_t;
#else
typedef union sh_xn_pi_iilb_cmp_enable0_u {
	mmr_t	sh_xn_pi_iilb_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_pi_iilb_cmp_enable0_s;
} sh_xn_pi_iilb_cmp_enable0_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_PI_IILB_CMP_ENABLE1"                 */
/*                    PI compare IILB input enable1                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_pi_iilb_cmp_enable1_u {
	mmr_t	sh_xn_pi_iilb_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_pi_iilb_cmp_enable1_s;
} sh_xn_pi_iilb_cmp_enable1_u_t;
#else
typedef union sh_xn_pi_iilb_cmp_enable1_u {
	mmr_t	sh_xn_pi_iilb_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_pi_iilb_cmp_enable1_s;
} sh_xn_pi_iilb_cmp_enable1_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_PI_NI0_CMP_EXP_DATA0"                 */
/*                 PI compare NI0 input expected data0                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_pi_ni0_cmp_exp_data0_u {
	mmr_t	sh_xn_pi_ni0_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_pi_ni0_cmp_exp_data0_s;
} sh_xn_pi_ni0_cmp_exp_data0_u_t;
#else
typedef union sh_xn_pi_ni0_cmp_exp_data0_u {
	mmr_t	sh_xn_pi_ni0_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_pi_ni0_cmp_exp_data0_s;
} sh_xn_pi_ni0_cmp_exp_data0_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_PI_NI0_CMP_EXP_DATA1"                 */
/*                 PI compare NI0 input expected data1                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_pi_ni0_cmp_exp_data1_u {
	mmr_t	sh_xn_pi_ni0_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_pi_ni0_cmp_exp_data1_s;
} sh_xn_pi_ni0_cmp_exp_data1_u_t;
#else
typedef union sh_xn_pi_ni0_cmp_exp_data1_u {
	mmr_t	sh_xn_pi_ni0_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_pi_ni0_cmp_exp_data1_s;
} sh_xn_pi_ni0_cmp_exp_data1_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_PI_NI0_CMP_ENABLE0"                  */
/*                     PI compare NI0 input enable0                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_pi_ni0_cmp_enable0_u {
	mmr_t	sh_xn_pi_ni0_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_pi_ni0_cmp_enable0_s;
} sh_xn_pi_ni0_cmp_enable0_u_t;
#else
typedef union sh_xn_pi_ni0_cmp_enable0_u {
	mmr_t	sh_xn_pi_ni0_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_pi_ni0_cmp_enable0_s;
} sh_xn_pi_ni0_cmp_enable0_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_PI_NI0_CMP_ENABLE1"                  */
/*                     PI compare NI0 input enable1                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_pi_ni0_cmp_enable1_u {
	mmr_t	sh_xn_pi_ni0_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_pi_ni0_cmp_enable1_s;
} sh_xn_pi_ni0_cmp_enable1_u_t;
#else
typedef union sh_xn_pi_ni0_cmp_enable1_u {
	mmr_t	sh_xn_pi_ni0_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_pi_ni0_cmp_enable1_s;
} sh_xn_pi_ni0_cmp_enable1_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_PI_NI1_CMP_EXP_DATA0"                 */
/*                 PI compare NI1 input expected data0                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_pi_ni1_cmp_exp_data0_u {
	mmr_t	sh_xn_pi_ni1_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_pi_ni1_cmp_exp_data0_s;
} sh_xn_pi_ni1_cmp_exp_data0_u_t;
#else
typedef union sh_xn_pi_ni1_cmp_exp_data0_u {
	mmr_t	sh_xn_pi_ni1_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_pi_ni1_cmp_exp_data0_s;
} sh_xn_pi_ni1_cmp_exp_data0_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_PI_NI1_CMP_EXP_DATA1"                 */
/*                 PI compare NI1 input expected data1                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_pi_ni1_cmp_exp_data1_u {
	mmr_t	sh_xn_pi_ni1_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_pi_ni1_cmp_exp_data1_s;
} sh_xn_pi_ni1_cmp_exp_data1_u_t;
#else
typedef union sh_xn_pi_ni1_cmp_exp_data1_u {
	mmr_t	sh_xn_pi_ni1_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_pi_ni1_cmp_exp_data1_s;
} sh_xn_pi_ni1_cmp_exp_data1_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_PI_NI1_CMP_ENABLE0"                  */
/*                     PI compare NI1 input enable0                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_pi_ni1_cmp_enable0_u {
	mmr_t	sh_xn_pi_ni1_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_pi_ni1_cmp_enable0_s;
} sh_xn_pi_ni1_cmp_enable0_u_t;
#else
typedef union sh_xn_pi_ni1_cmp_enable0_u {
	mmr_t	sh_xn_pi_ni1_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_pi_ni1_cmp_enable0_s;
} sh_xn_pi_ni1_cmp_enable0_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_PI_NI1_CMP_ENABLE1"                  */
/*                     PI compare NI1 input enable1                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_pi_ni1_cmp_enable1_u {
	mmr_t	sh_xn_pi_ni1_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_pi_ni1_cmp_enable1_s;
} sh_xn_pi_ni1_cmp_enable1_u_t;
#else
typedef union sh_xn_pi_ni1_cmp_enable1_u {
	mmr_t	sh_xn_pi_ni1_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_pi_ni1_cmp_enable1_s;
} sh_xn_pi_ni1_cmp_enable1_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_PI_SIC_CMP_EXP_HDR0"                 */
/*                PI compare SIC input expected header0                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_pi_sic_cmp_exp_hdr0_u {
	mmr_t	sh_xn_pi_sic_cmp_exp_hdr0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_pi_sic_cmp_exp_hdr0_s;
} sh_xn_pi_sic_cmp_exp_hdr0_u_t;
#else
typedef union sh_xn_pi_sic_cmp_exp_hdr0_u {
	mmr_t	sh_xn_pi_sic_cmp_exp_hdr0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_pi_sic_cmp_exp_hdr0_s;
} sh_xn_pi_sic_cmp_exp_hdr0_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_PI_SIC_CMP_EXP_HDR1"                 */
/*                PI compare SIC input expected header1                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_pi_sic_cmp_exp_hdr1_u {
	mmr_t	sh_xn_pi_sic_cmp_exp_hdr1_regval;
	struct {
		mmr_t	data        : 42;
		mmr_t	reserved_0  : 22;
	} sh_xn_pi_sic_cmp_exp_hdr1_s;
} sh_xn_pi_sic_cmp_exp_hdr1_u_t;
#else
typedef union sh_xn_pi_sic_cmp_exp_hdr1_u {
	mmr_t	sh_xn_pi_sic_cmp_exp_hdr1_regval;
	struct {
		mmr_t	reserved_0  : 22;
		mmr_t	data        : 42;
	} sh_xn_pi_sic_cmp_exp_hdr1_s;
} sh_xn_pi_sic_cmp_exp_hdr1_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_XN_PI_SIC_CMP_HDR_ENABLE0"                */
/*                    PI compare SIC header enable0                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_pi_sic_cmp_hdr_enable0_u {
	mmr_t	sh_xn_pi_sic_cmp_hdr_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_pi_sic_cmp_hdr_enable0_s;
} sh_xn_pi_sic_cmp_hdr_enable0_u_t;
#else
typedef union sh_xn_pi_sic_cmp_hdr_enable0_u {
	mmr_t	sh_xn_pi_sic_cmp_hdr_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_pi_sic_cmp_hdr_enable0_s;
} sh_xn_pi_sic_cmp_hdr_enable0_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_XN_PI_SIC_CMP_HDR_ENABLE1"                */
/*                    PI compare SIC header enable1                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_pi_sic_cmp_hdr_enable1_u {
	mmr_t	sh_xn_pi_sic_cmp_hdr_enable1_regval;
	struct {
		mmr_t	enable      : 42;
		mmr_t	reserved_0  : 22;
	} sh_xn_pi_sic_cmp_hdr_enable1_s;
} sh_xn_pi_sic_cmp_hdr_enable1_u_t;
#else
typedef union sh_xn_pi_sic_cmp_hdr_enable1_u {
	mmr_t	sh_xn_pi_sic_cmp_hdr_enable1_regval;
	struct {
		mmr_t	reserved_0  : 22;
		mmr_t	enable      : 42;
	} sh_xn_pi_sic_cmp_hdr_enable1_s;
} sh_xn_pi_sic_cmp_hdr_enable1_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_XN_PI_SIC_CMP_DATA0"                   */
/*                         PI compare SIC data0                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_pi_sic_cmp_data0_u {
	mmr_t	sh_xn_pi_sic_cmp_data0_regval;
	struct {
		mmr_t	data0       : 64;
	} sh_xn_pi_sic_cmp_data0_s;
} sh_xn_pi_sic_cmp_data0_u_t;
#else
typedef union sh_xn_pi_sic_cmp_data0_u {
	mmr_t	sh_xn_pi_sic_cmp_data0_regval;
	struct {
		mmr_t	data0       : 64;
	} sh_xn_pi_sic_cmp_data0_s;
} sh_xn_pi_sic_cmp_data0_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_XN_PI_SIC_CMP_DATA1"                   */
/*                         PI compare SIC data1                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_pi_sic_cmp_data1_u {
	mmr_t	sh_xn_pi_sic_cmp_data1_regval;
	struct {
		mmr_t	data1       : 64;
	} sh_xn_pi_sic_cmp_data1_s;
} sh_xn_pi_sic_cmp_data1_u_t;
#else
typedef union sh_xn_pi_sic_cmp_data1_u {
	mmr_t	sh_xn_pi_sic_cmp_data1_regval;
	struct {
		mmr_t	data1       : 64;
	} sh_xn_pi_sic_cmp_data1_s;
} sh_xn_pi_sic_cmp_data1_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_XN_PI_SIC_CMP_DATA2"                   */
/*                         PI compare SIC data2                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_pi_sic_cmp_data2_u {
	mmr_t	sh_xn_pi_sic_cmp_data2_regval;
	struct {
		mmr_t	data2       : 64;
	} sh_xn_pi_sic_cmp_data2_s;
} sh_xn_pi_sic_cmp_data2_u_t;
#else
typedef union sh_xn_pi_sic_cmp_data2_u {
	mmr_t	sh_xn_pi_sic_cmp_data2_regval;
	struct {
		mmr_t	data2       : 64;
	} sh_xn_pi_sic_cmp_data2_s;
} sh_xn_pi_sic_cmp_data2_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_XN_PI_SIC_CMP_DATA3"                   */
/*                         PI compare SIC data3                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_pi_sic_cmp_data3_u {
	mmr_t	sh_xn_pi_sic_cmp_data3_regval;
	struct {
		mmr_t	data3       : 64;
	} sh_xn_pi_sic_cmp_data3_s;
} sh_xn_pi_sic_cmp_data3_u_t;
#else
typedef union sh_xn_pi_sic_cmp_data3_u {
	mmr_t	sh_xn_pi_sic_cmp_data3_regval;
	struct {
		mmr_t	data3       : 64;
	} sh_xn_pi_sic_cmp_data3_s;
} sh_xn_pi_sic_cmp_data3_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_XN_PI_SIC_CMP_DATA_ENABLE0"               */
/*                     PI enable compare SIC data0                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_pi_sic_cmp_data_enable0_u {
	mmr_t	sh_xn_pi_sic_cmp_data_enable0_regval;
	struct {
		mmr_t	data_enable0 : 64;
	} sh_xn_pi_sic_cmp_data_enable0_s;
} sh_xn_pi_sic_cmp_data_enable0_u_t;
#else
typedef union sh_xn_pi_sic_cmp_data_enable0_u {
	mmr_t	sh_xn_pi_sic_cmp_data_enable0_regval;
	struct {
		mmr_t	data_enable0 : 64;
	} sh_xn_pi_sic_cmp_data_enable0_s;
} sh_xn_pi_sic_cmp_data_enable0_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_XN_PI_SIC_CMP_DATA_ENABLE1"               */
/*                     PI enable compare SIC data1                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_pi_sic_cmp_data_enable1_u {
	mmr_t	sh_xn_pi_sic_cmp_data_enable1_regval;
	struct {
		mmr_t	data_enable1 : 64;
	} sh_xn_pi_sic_cmp_data_enable1_s;
} sh_xn_pi_sic_cmp_data_enable1_u_t;
#else
typedef union sh_xn_pi_sic_cmp_data_enable1_u {
	mmr_t	sh_xn_pi_sic_cmp_data_enable1_regval;
	struct {
		mmr_t	data_enable1 : 64;
	} sh_xn_pi_sic_cmp_data_enable1_s;
} sh_xn_pi_sic_cmp_data_enable1_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_XN_PI_SIC_CMP_DATA_ENABLE2"               */
/*                     PI enable compare SIC data2                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_pi_sic_cmp_data_enable2_u {
	mmr_t	sh_xn_pi_sic_cmp_data_enable2_regval;
	struct {
		mmr_t	data_enable2 : 64;
	} sh_xn_pi_sic_cmp_data_enable2_s;
} sh_xn_pi_sic_cmp_data_enable2_u_t;
#else
typedef union sh_xn_pi_sic_cmp_data_enable2_u {
	mmr_t	sh_xn_pi_sic_cmp_data_enable2_regval;
	struct {
		mmr_t	data_enable2 : 64;
	} sh_xn_pi_sic_cmp_data_enable2_s;
} sh_xn_pi_sic_cmp_data_enable2_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_XN_PI_SIC_CMP_DATA_ENABLE3"               */
/*                     PI enable compare SIC data3                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_pi_sic_cmp_data_enable3_u {
	mmr_t	sh_xn_pi_sic_cmp_data_enable3_regval;
	struct {
		mmr_t	data_enable3 : 64;
	} sh_xn_pi_sic_cmp_data_enable3_s;
} sh_xn_pi_sic_cmp_data_enable3_u_t;
#else
typedef union sh_xn_pi_sic_cmp_data_enable3_u {
	mmr_t	sh_xn_pi_sic_cmp_data_enable3_regval;
	struct {
		mmr_t	data_enable3 : 64;
	} sh_xn_pi_sic_cmp_data_enable3_s;
} sh_xn_pi_sic_cmp_data_enable3_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_XN_NI0_IILB_CMP_EXP_DATA0"                */
/*                NI0 compare IILB input expected data0                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni0_iilb_cmp_exp_data0_u {
	mmr_t	sh_xn_ni0_iilb_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni0_iilb_cmp_exp_data0_s;
} sh_xn_ni0_iilb_cmp_exp_data0_u_t;
#else
typedef union sh_xn_ni0_iilb_cmp_exp_data0_u {
	mmr_t	sh_xn_ni0_iilb_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni0_iilb_cmp_exp_data0_s;
} sh_xn_ni0_iilb_cmp_exp_data0_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_XN_NI0_IILB_CMP_EXP_DATA1"                */
/*                NI0 compare IILB input expected data1                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni0_iilb_cmp_exp_data1_u {
	mmr_t	sh_xn_ni0_iilb_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni0_iilb_cmp_exp_data1_s;
} sh_xn_ni0_iilb_cmp_exp_data1_u_t;
#else
typedef union sh_xn_ni0_iilb_cmp_exp_data1_u {
	mmr_t	sh_xn_ni0_iilb_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni0_iilb_cmp_exp_data1_s;
} sh_xn_ni0_iilb_cmp_exp_data1_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_NI0_IILB_CMP_ENABLE0"                 */
/*                    NI0 compare IILB input enable0                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni0_iilb_cmp_enable0_u {
	mmr_t	sh_xn_ni0_iilb_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni0_iilb_cmp_enable0_s;
} sh_xn_ni0_iilb_cmp_enable0_u_t;
#else
typedef union sh_xn_ni0_iilb_cmp_enable0_u {
	mmr_t	sh_xn_ni0_iilb_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni0_iilb_cmp_enable0_s;
} sh_xn_ni0_iilb_cmp_enable0_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_NI0_IILB_CMP_ENABLE1"                 */
/*                    NI0 compare IILB input enable1                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni0_iilb_cmp_enable1_u {
	mmr_t	sh_xn_ni0_iilb_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni0_iilb_cmp_enable1_s;
} sh_xn_ni0_iilb_cmp_enable1_u_t;
#else
typedef union sh_xn_ni0_iilb_cmp_enable1_u {
	mmr_t	sh_xn_ni0_iilb_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni0_iilb_cmp_enable1_s;
} sh_xn_ni0_iilb_cmp_enable1_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_NI0_PI_CMP_EXP_DATA0"                 */
/*                 NI0 compare PI input expected data0                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni0_pi_cmp_exp_data0_u {
	mmr_t	sh_xn_ni0_pi_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni0_pi_cmp_exp_data0_s;
} sh_xn_ni0_pi_cmp_exp_data0_u_t;
#else
typedef union sh_xn_ni0_pi_cmp_exp_data0_u {
	mmr_t	sh_xn_ni0_pi_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni0_pi_cmp_exp_data0_s;
} sh_xn_ni0_pi_cmp_exp_data0_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_NI0_PI_CMP_EXP_DATA1"                 */
/*                 NI0 compare PI input expected data1                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni0_pi_cmp_exp_data1_u {
	mmr_t	sh_xn_ni0_pi_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni0_pi_cmp_exp_data1_s;
} sh_xn_ni0_pi_cmp_exp_data1_u_t;
#else
typedef union sh_xn_ni0_pi_cmp_exp_data1_u {
	mmr_t	sh_xn_ni0_pi_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni0_pi_cmp_exp_data1_s;
} sh_xn_ni0_pi_cmp_exp_data1_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_NI0_PI_CMP_ENABLE0"                  */
/*                     NI0 compare PI input enable0                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni0_pi_cmp_enable0_u {
	mmr_t	sh_xn_ni0_pi_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni0_pi_cmp_enable0_s;
} sh_xn_ni0_pi_cmp_enable0_u_t;
#else
typedef union sh_xn_ni0_pi_cmp_enable0_u {
	mmr_t	sh_xn_ni0_pi_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni0_pi_cmp_enable0_s;
} sh_xn_ni0_pi_cmp_enable0_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_NI0_PI_CMP_ENABLE1"                  */
/*                     NI0 compare PI input enable1                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni0_pi_cmp_enable1_u {
	mmr_t	sh_xn_ni0_pi_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni0_pi_cmp_enable1_s;
} sh_xn_ni0_pi_cmp_enable1_u_t;
#else
typedef union sh_xn_ni0_pi_cmp_enable1_u {
	mmr_t	sh_xn_ni0_pi_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni0_pi_cmp_enable1_s;
} sh_xn_ni0_pi_cmp_enable1_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_NI0_MD_CMP_EXP_DATA0"                 */
/*                 NI0 compare MD input expected data0                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni0_md_cmp_exp_data0_u {
	mmr_t	sh_xn_ni0_md_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni0_md_cmp_exp_data0_s;
} sh_xn_ni0_md_cmp_exp_data0_u_t;
#else
typedef union sh_xn_ni0_md_cmp_exp_data0_u {
	mmr_t	sh_xn_ni0_md_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni0_md_cmp_exp_data0_s;
} sh_xn_ni0_md_cmp_exp_data0_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_NI0_MD_CMP_EXP_DATA1"                 */
/*                 NI0 compare MD input expected data1                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni0_md_cmp_exp_data1_u {
	mmr_t	sh_xn_ni0_md_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni0_md_cmp_exp_data1_s;
} sh_xn_ni0_md_cmp_exp_data1_u_t;
#else
typedef union sh_xn_ni0_md_cmp_exp_data1_u {
	mmr_t	sh_xn_ni0_md_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni0_md_cmp_exp_data1_s;
} sh_xn_ni0_md_cmp_exp_data1_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_NI0_MD_CMP_ENABLE0"                  */
/*                     NI0 compare MD input enable0                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni0_md_cmp_enable0_u {
	mmr_t	sh_xn_ni0_md_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni0_md_cmp_enable0_s;
} sh_xn_ni0_md_cmp_enable0_u_t;
#else
typedef union sh_xn_ni0_md_cmp_enable0_u {
	mmr_t	sh_xn_ni0_md_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni0_md_cmp_enable0_s;
} sh_xn_ni0_md_cmp_enable0_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_NI0_MD_CMP_ENABLE1"                  */
/*                     NI0 compare MD input enable1                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni0_md_cmp_enable1_u {
	mmr_t	sh_xn_ni0_md_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni0_md_cmp_enable1_s;
} sh_xn_ni0_md_cmp_enable1_u_t;
#else
typedef union sh_xn_ni0_md_cmp_enable1_u {
	mmr_t	sh_xn_ni0_md_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni0_md_cmp_enable1_s;
} sh_xn_ni0_md_cmp_enable1_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_NI0_NI_CMP_EXP_DATA0"                 */
/*                 NI0 compare NI input expected data0                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni0_ni_cmp_exp_data0_u {
	mmr_t	sh_xn_ni0_ni_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni0_ni_cmp_exp_data0_s;
} sh_xn_ni0_ni_cmp_exp_data0_u_t;
#else
typedef union sh_xn_ni0_ni_cmp_exp_data0_u {
	mmr_t	sh_xn_ni0_ni_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni0_ni_cmp_exp_data0_s;
} sh_xn_ni0_ni_cmp_exp_data0_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_NI0_NI_CMP_EXP_DATA1"                 */
/*                 NI0 compare NI input expected data1                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni0_ni_cmp_exp_data1_u {
	mmr_t	sh_xn_ni0_ni_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni0_ni_cmp_exp_data1_s;
} sh_xn_ni0_ni_cmp_exp_data1_u_t;
#else
typedef union sh_xn_ni0_ni_cmp_exp_data1_u {
	mmr_t	sh_xn_ni0_ni_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni0_ni_cmp_exp_data1_s;
} sh_xn_ni0_ni_cmp_exp_data1_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_NI0_NI_CMP_ENABLE0"                  */
/*                     NI0 compare NI input enable0                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni0_ni_cmp_enable0_u {
	mmr_t	sh_xn_ni0_ni_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni0_ni_cmp_enable0_s;
} sh_xn_ni0_ni_cmp_enable0_u_t;
#else
typedef union sh_xn_ni0_ni_cmp_enable0_u {
	mmr_t	sh_xn_ni0_ni_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni0_ni_cmp_enable0_s;
} sh_xn_ni0_ni_cmp_enable0_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_NI0_NI_CMP_ENABLE1"                  */
/*                     NI0 compare NI input enable1                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni0_ni_cmp_enable1_u {
	mmr_t	sh_xn_ni0_ni_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni0_ni_cmp_enable1_s;
} sh_xn_ni0_ni_cmp_enable1_u_t;
#else
typedef union sh_xn_ni0_ni_cmp_enable1_u {
	mmr_t	sh_xn_ni0_ni_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni0_ni_cmp_enable1_s;
} sh_xn_ni0_ni_cmp_enable1_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_NI0_LLP_CMP_EXP_DATA0"                */
/*                 NI0 compare LLP input expected data0                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni0_llp_cmp_exp_data0_u {
	mmr_t	sh_xn_ni0_llp_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni0_llp_cmp_exp_data0_s;
} sh_xn_ni0_llp_cmp_exp_data0_u_t;
#else
typedef union sh_xn_ni0_llp_cmp_exp_data0_u {
	mmr_t	sh_xn_ni0_llp_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni0_llp_cmp_exp_data0_s;
} sh_xn_ni0_llp_cmp_exp_data0_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_NI0_LLP_CMP_EXP_DATA1"                */
/*                 NI0 compare LLP input expected data1                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni0_llp_cmp_exp_data1_u {
	mmr_t	sh_xn_ni0_llp_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni0_llp_cmp_exp_data1_s;
} sh_xn_ni0_llp_cmp_exp_data1_u_t;
#else
typedef union sh_xn_ni0_llp_cmp_exp_data1_u {
	mmr_t	sh_xn_ni0_llp_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni0_llp_cmp_exp_data1_s;
} sh_xn_ni0_llp_cmp_exp_data1_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_NI0_LLP_CMP_ENABLE0"                 */
/*                    NI0 compare LLP input enable0                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni0_llp_cmp_enable0_u {
	mmr_t	sh_xn_ni0_llp_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni0_llp_cmp_enable0_s;
} sh_xn_ni0_llp_cmp_enable0_u_t;
#else
typedef union sh_xn_ni0_llp_cmp_enable0_u {
	mmr_t	sh_xn_ni0_llp_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni0_llp_cmp_enable0_s;
} sh_xn_ni0_llp_cmp_enable0_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_NI0_LLP_CMP_ENABLE1"                 */
/*                    NI0 compare LLP input enable1                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni0_llp_cmp_enable1_u {
	mmr_t	sh_xn_ni0_llp_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni0_llp_cmp_enable1_s;
} sh_xn_ni0_llp_cmp_enable1_u_t;
#else
typedef union sh_xn_ni0_llp_cmp_enable1_u {
	mmr_t	sh_xn_ni0_llp_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni0_llp_cmp_enable1_s;
} sh_xn_ni0_llp_cmp_enable1_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_XN_NI1_IILB_CMP_EXP_DATA0"                */
/*                NI1 compare IILB input expected data0                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni1_iilb_cmp_exp_data0_u {
	mmr_t	sh_xn_ni1_iilb_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni1_iilb_cmp_exp_data0_s;
} sh_xn_ni1_iilb_cmp_exp_data0_u_t;
#else
typedef union sh_xn_ni1_iilb_cmp_exp_data0_u {
	mmr_t	sh_xn_ni1_iilb_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni1_iilb_cmp_exp_data0_s;
} sh_xn_ni1_iilb_cmp_exp_data0_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_XN_NI1_IILB_CMP_EXP_DATA1"                */
/*                NI1 compare IILB input expected data1                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni1_iilb_cmp_exp_data1_u {
	mmr_t	sh_xn_ni1_iilb_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni1_iilb_cmp_exp_data1_s;
} sh_xn_ni1_iilb_cmp_exp_data1_u_t;
#else
typedef union sh_xn_ni1_iilb_cmp_exp_data1_u {
	mmr_t	sh_xn_ni1_iilb_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni1_iilb_cmp_exp_data1_s;
} sh_xn_ni1_iilb_cmp_exp_data1_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_NI1_IILB_CMP_ENABLE0"                 */
/*                    NI1 compare IILB input enable0                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni1_iilb_cmp_enable0_u {
	mmr_t	sh_xn_ni1_iilb_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni1_iilb_cmp_enable0_s;
} sh_xn_ni1_iilb_cmp_enable0_u_t;
#else
typedef union sh_xn_ni1_iilb_cmp_enable0_u {
	mmr_t	sh_xn_ni1_iilb_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni1_iilb_cmp_enable0_s;
} sh_xn_ni1_iilb_cmp_enable0_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_NI1_IILB_CMP_ENABLE1"                 */
/*                    NI1 compare IILB input enable1                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni1_iilb_cmp_enable1_u {
	mmr_t	sh_xn_ni1_iilb_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni1_iilb_cmp_enable1_s;
} sh_xn_ni1_iilb_cmp_enable1_u_t;
#else
typedef union sh_xn_ni1_iilb_cmp_enable1_u {
	mmr_t	sh_xn_ni1_iilb_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni1_iilb_cmp_enable1_s;
} sh_xn_ni1_iilb_cmp_enable1_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_NI1_PI_CMP_EXP_DATA0"                 */
/*                 NI1 compare PI input expected data0                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni1_pi_cmp_exp_data0_u {
	mmr_t	sh_xn_ni1_pi_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni1_pi_cmp_exp_data0_s;
} sh_xn_ni1_pi_cmp_exp_data0_u_t;
#else
typedef union sh_xn_ni1_pi_cmp_exp_data0_u {
	mmr_t	sh_xn_ni1_pi_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni1_pi_cmp_exp_data0_s;
} sh_xn_ni1_pi_cmp_exp_data0_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_NI1_PI_CMP_EXP_DATA1"                 */
/*                 NI1 compare PI input expected data1                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni1_pi_cmp_exp_data1_u {
	mmr_t	sh_xn_ni1_pi_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni1_pi_cmp_exp_data1_s;
} sh_xn_ni1_pi_cmp_exp_data1_u_t;
#else
typedef union sh_xn_ni1_pi_cmp_exp_data1_u {
	mmr_t	sh_xn_ni1_pi_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni1_pi_cmp_exp_data1_s;
} sh_xn_ni1_pi_cmp_exp_data1_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_NI1_PI_CMP_ENABLE0"                  */
/*                     NI1 compare PI input enable0                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni1_pi_cmp_enable0_u {
	mmr_t	sh_xn_ni1_pi_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni1_pi_cmp_enable0_s;
} sh_xn_ni1_pi_cmp_enable0_u_t;
#else
typedef union sh_xn_ni1_pi_cmp_enable0_u {
	mmr_t	sh_xn_ni1_pi_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni1_pi_cmp_enable0_s;
} sh_xn_ni1_pi_cmp_enable0_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_NI1_PI_CMP_ENABLE1"                  */
/*                     NI1 compare PI input enable1                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni1_pi_cmp_enable1_u {
	mmr_t	sh_xn_ni1_pi_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni1_pi_cmp_enable1_s;
} sh_xn_ni1_pi_cmp_enable1_u_t;
#else
typedef union sh_xn_ni1_pi_cmp_enable1_u {
	mmr_t	sh_xn_ni1_pi_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni1_pi_cmp_enable1_s;
} sh_xn_ni1_pi_cmp_enable1_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_NI1_MD_CMP_EXP_DATA0"                 */
/*                 NI1 compare MD input expected data0                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni1_md_cmp_exp_data0_u {
	mmr_t	sh_xn_ni1_md_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni1_md_cmp_exp_data0_s;
} sh_xn_ni1_md_cmp_exp_data0_u_t;
#else
typedef union sh_xn_ni1_md_cmp_exp_data0_u {
	mmr_t	sh_xn_ni1_md_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni1_md_cmp_exp_data0_s;
} sh_xn_ni1_md_cmp_exp_data0_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_NI1_MD_CMP_EXP_DATA1"                 */
/*                 NI1 compare MD input expected data1                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni1_md_cmp_exp_data1_u {
	mmr_t	sh_xn_ni1_md_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni1_md_cmp_exp_data1_s;
} sh_xn_ni1_md_cmp_exp_data1_u_t;
#else
typedef union sh_xn_ni1_md_cmp_exp_data1_u {
	mmr_t	sh_xn_ni1_md_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni1_md_cmp_exp_data1_s;
} sh_xn_ni1_md_cmp_exp_data1_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_NI1_MD_CMP_ENABLE0"                  */
/*                     NI1 compare MD input enable0                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni1_md_cmp_enable0_u {
	mmr_t	sh_xn_ni1_md_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni1_md_cmp_enable0_s;
} sh_xn_ni1_md_cmp_enable0_u_t;
#else
typedef union sh_xn_ni1_md_cmp_enable0_u {
	mmr_t	sh_xn_ni1_md_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni1_md_cmp_enable0_s;
} sh_xn_ni1_md_cmp_enable0_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_NI1_MD_CMP_ENABLE1"                  */
/*                     NI1 compare MD input enable1                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni1_md_cmp_enable1_u {
	mmr_t	sh_xn_ni1_md_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni1_md_cmp_enable1_s;
} sh_xn_ni1_md_cmp_enable1_u_t;
#else
typedef union sh_xn_ni1_md_cmp_enable1_u {
	mmr_t	sh_xn_ni1_md_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni1_md_cmp_enable1_s;
} sh_xn_ni1_md_cmp_enable1_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_NI1_NI_CMP_EXP_DATA0"                 */
/*                 NI1 compare NI input expected data0                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni1_ni_cmp_exp_data0_u {
	mmr_t	sh_xn_ni1_ni_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni1_ni_cmp_exp_data0_s;
} sh_xn_ni1_ni_cmp_exp_data0_u_t;
#else
typedef union sh_xn_ni1_ni_cmp_exp_data0_u {
	mmr_t	sh_xn_ni1_ni_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni1_ni_cmp_exp_data0_s;
} sh_xn_ni1_ni_cmp_exp_data0_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_NI1_NI_CMP_EXP_DATA1"                 */
/*                 NI1 compare NI input expected data1                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni1_ni_cmp_exp_data1_u {
	mmr_t	sh_xn_ni1_ni_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni1_ni_cmp_exp_data1_s;
} sh_xn_ni1_ni_cmp_exp_data1_u_t;
#else
typedef union sh_xn_ni1_ni_cmp_exp_data1_u {
	mmr_t	sh_xn_ni1_ni_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni1_ni_cmp_exp_data1_s;
} sh_xn_ni1_ni_cmp_exp_data1_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_NI1_NI_CMP_ENABLE0"                  */
/*                     NI1 compare NI input enable0                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni1_ni_cmp_enable0_u {
	mmr_t	sh_xn_ni1_ni_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni1_ni_cmp_enable0_s;
} sh_xn_ni1_ni_cmp_enable0_u_t;
#else
typedef union sh_xn_ni1_ni_cmp_enable0_u {
	mmr_t	sh_xn_ni1_ni_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni1_ni_cmp_enable0_s;
} sh_xn_ni1_ni_cmp_enable0_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_NI1_NI_CMP_ENABLE1"                  */
/*                     NI1 compare NI input enable1                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni1_ni_cmp_enable1_u {
	mmr_t	sh_xn_ni1_ni_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni1_ni_cmp_enable1_s;
} sh_xn_ni1_ni_cmp_enable1_u_t;
#else
typedef union sh_xn_ni1_ni_cmp_enable1_u {
	mmr_t	sh_xn_ni1_ni_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni1_ni_cmp_enable1_s;
} sh_xn_ni1_ni_cmp_enable1_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_NI1_LLP_CMP_EXP_DATA0"                */
/*                 NI1 compare LLP input expected data0                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni1_llp_cmp_exp_data0_u {
	mmr_t	sh_xn_ni1_llp_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni1_llp_cmp_exp_data0_s;
} sh_xn_ni1_llp_cmp_exp_data0_u_t;
#else
typedef union sh_xn_ni1_llp_cmp_exp_data0_u {
	mmr_t	sh_xn_ni1_llp_cmp_exp_data0_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni1_llp_cmp_exp_data0_s;
} sh_xn_ni1_llp_cmp_exp_data0_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_NI1_LLP_CMP_EXP_DATA1"                */
/*                 NI1 compare LLP input expected data1                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni1_llp_cmp_exp_data1_u {
	mmr_t	sh_xn_ni1_llp_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni1_llp_cmp_exp_data1_s;
} sh_xn_ni1_llp_cmp_exp_data1_u_t;
#else
typedef union sh_xn_ni1_llp_cmp_exp_data1_u {
	mmr_t	sh_xn_ni1_llp_cmp_exp_data1_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_ni1_llp_cmp_exp_data1_s;
} sh_xn_ni1_llp_cmp_exp_data1_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_NI1_LLP_CMP_ENABLE0"                 */
/*                    NI1 compare LLP input enable0                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni1_llp_cmp_enable0_u {
	mmr_t	sh_xn_ni1_llp_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni1_llp_cmp_enable0_s;
} sh_xn_ni1_llp_cmp_enable0_u_t;
#else
typedef union sh_xn_ni1_llp_cmp_enable0_u {
	mmr_t	sh_xn_ni1_llp_cmp_enable0_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni1_llp_cmp_enable0_s;
} sh_xn_ni1_llp_cmp_enable0_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_NI1_LLP_CMP_ENABLE1"                 */
/*                    NI1 compare LLP input enable1                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_ni1_llp_cmp_enable1_u {
	mmr_t	sh_xn_ni1_llp_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni1_llp_cmp_enable1_s;
} sh_xn_ni1_llp_cmp_enable1_u_t;
#else
typedef union sh_xn_ni1_llp_cmp_enable1_u {
	mmr_t	sh_xn_ni1_llp_cmp_enable1_regval;
	struct {
		mmr_t	enable      : 64;
	} sh_xn_ni1_llp_cmp_enable1_s;
} sh_xn_ni1_llp_cmp_enable1_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_XNPI_ECC_INJ_REG"                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnpi_ecc_inj_reg_u {
	mmr_t	sh_xnpi_ecc_inj_reg_regval;
	struct {
		mmr_t	byte0          : 8;
		mmr_t	reserved_0     : 4;
		mmr_t	data_1shot0    : 1;
		mmr_t	data_cont0     : 1;
		mmr_t	data_cb_1shot0 : 1;
		mmr_t	data_cb_cont0  : 1;
		mmr_t	byte1          : 8;
		mmr_t	reserved_1     : 4;
		mmr_t	data_1shot1    : 1;
		mmr_t	data_cont1     : 1;
		mmr_t	data_cb_1shot1 : 1;
		mmr_t	data_cb_cont1  : 1;
		mmr_t	byte2          : 8;
		mmr_t	reserved_2     : 4;
		mmr_t	data_1shot2    : 1;
		mmr_t	data_cont2     : 1;
		mmr_t	data_cb_1shot2 : 1;
		mmr_t	data_cb_cont2  : 1;
		mmr_t	byte3          : 8;
		mmr_t	reserved_3     : 4;
		mmr_t	data_1shot3    : 1;
		mmr_t	data_cont3     : 1;
		mmr_t	data_cb_1shot3 : 1;
		mmr_t	data_cb_cont3  : 1;
	} sh_xnpi_ecc_inj_reg_s;
} sh_xnpi_ecc_inj_reg_u_t;
#else
typedef union sh_xnpi_ecc_inj_reg_u {
	mmr_t	sh_xnpi_ecc_inj_reg_regval;
	struct {
		mmr_t	data_cb_cont3  : 1;
		mmr_t	data_cb_1shot3 : 1;
		mmr_t	data_cont3     : 1;
		mmr_t	data_1shot3    : 1;
		mmr_t	reserved_3     : 4;
		mmr_t	byte3          : 8;
		mmr_t	data_cb_cont2  : 1;
		mmr_t	data_cb_1shot2 : 1;
		mmr_t	data_cont2     : 1;
		mmr_t	data_1shot2    : 1;
		mmr_t	reserved_2     : 4;
		mmr_t	byte2          : 8;
		mmr_t	data_cb_cont1  : 1;
		mmr_t	data_cb_1shot1 : 1;
		mmr_t	data_cont1     : 1;
		mmr_t	data_1shot1    : 1;
		mmr_t	reserved_1     : 4;
		mmr_t	byte1          : 8;
		mmr_t	data_cb_cont0  : 1;
		mmr_t	data_cb_1shot0 : 1;
		mmr_t	data_cont0     : 1;
		mmr_t	data_1shot0    : 1;
		mmr_t	reserved_0     : 4;
		mmr_t	byte0          : 8;
	} sh_xnpi_ecc_inj_reg_s;
} sh_xnpi_ecc_inj_reg_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XNPI_ECC0_INJ_MASK_REG"                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnpi_ecc0_inj_mask_reg_u {
	mmr_t	sh_xnpi_ecc0_inj_mask_reg_regval;
	struct {
		mmr_t	mask_ecc0   : 64;
	} sh_xnpi_ecc0_inj_mask_reg_s;
} sh_xnpi_ecc0_inj_mask_reg_u_t;
#else
typedef union sh_xnpi_ecc0_inj_mask_reg_u {
	mmr_t	sh_xnpi_ecc0_inj_mask_reg_regval;
	struct {
		mmr_t	mask_ecc0   : 64;
	} sh_xnpi_ecc0_inj_mask_reg_s;
} sh_xnpi_ecc0_inj_mask_reg_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XNPI_ECC1_INJ_MASK_REG"                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnpi_ecc1_inj_mask_reg_u {
	mmr_t	sh_xnpi_ecc1_inj_mask_reg_regval;
	struct {
		mmr_t	mask_ecc1   : 64;
	} sh_xnpi_ecc1_inj_mask_reg_s;
} sh_xnpi_ecc1_inj_mask_reg_u_t;
#else
typedef union sh_xnpi_ecc1_inj_mask_reg_u {
	mmr_t	sh_xnpi_ecc1_inj_mask_reg_regval;
	struct {
		mmr_t	mask_ecc1   : 64;
	} sh_xnpi_ecc1_inj_mask_reg_s;
} sh_xnpi_ecc1_inj_mask_reg_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XNPI_ECC2_INJ_MASK_REG"                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnpi_ecc2_inj_mask_reg_u {
	mmr_t	sh_xnpi_ecc2_inj_mask_reg_regval;
	struct {
		mmr_t	mask_ecc2   : 64;
	} sh_xnpi_ecc2_inj_mask_reg_s;
} sh_xnpi_ecc2_inj_mask_reg_u_t;
#else
typedef union sh_xnpi_ecc2_inj_mask_reg_u {
	mmr_t	sh_xnpi_ecc2_inj_mask_reg_regval;
	struct {
		mmr_t	mask_ecc2   : 64;
	} sh_xnpi_ecc2_inj_mask_reg_s;
} sh_xnpi_ecc2_inj_mask_reg_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XNPI_ECC3_INJ_MASK_REG"                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnpi_ecc3_inj_mask_reg_u {
	mmr_t	sh_xnpi_ecc3_inj_mask_reg_regval;
	struct {
		mmr_t	mask_ecc3   : 64;
	} sh_xnpi_ecc3_inj_mask_reg_s;
} sh_xnpi_ecc3_inj_mask_reg_u_t;
#else
typedef union sh_xnpi_ecc3_inj_mask_reg_u {
	mmr_t	sh_xnpi_ecc3_inj_mask_reg_regval;
	struct {
		mmr_t	mask_ecc3   : 64;
	} sh_xnpi_ecc3_inj_mask_reg_s;
} sh_xnpi_ecc3_inj_mask_reg_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_XNMD_ECC_INJ_REG"                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnmd_ecc_inj_reg_u {
	mmr_t	sh_xnmd_ecc_inj_reg_regval;
	struct {
		mmr_t	byte0          : 8;
		mmr_t	reserved_0     : 4;
		mmr_t	data_1shot0    : 1;
		mmr_t	data_cont0     : 1;
		mmr_t	data_cb_1shot0 : 1;
		mmr_t	data_cb_cont0  : 1;
		mmr_t	byte1          : 8;
		mmr_t	reserved_1     : 4;
		mmr_t	data_1shot1    : 1;
		mmr_t	data_cont1     : 1;
		mmr_t	data_cb_1shot1 : 1;
		mmr_t	data_cb_cont1  : 1;
		mmr_t	byte2          : 8;
		mmr_t	reserved_2     : 4;
		mmr_t	data_1shot2    : 1;
		mmr_t	data_cont2     : 1;
		mmr_t	data_cb_1shot2 : 1;
		mmr_t	data_cb_cont2  : 1;
		mmr_t	byte3          : 8;
		mmr_t	reserved_3     : 4;
		mmr_t	data_1shot3    : 1;
		mmr_t	data_cont3     : 1;
		mmr_t	data_cb_1shot3 : 1;
		mmr_t	data_cb_cont3  : 1;
	} sh_xnmd_ecc_inj_reg_s;
} sh_xnmd_ecc_inj_reg_u_t;
#else
typedef union sh_xnmd_ecc_inj_reg_u {
	mmr_t	sh_xnmd_ecc_inj_reg_regval;
	struct {
		mmr_t	data_cb_cont3  : 1;
		mmr_t	data_cb_1shot3 : 1;
		mmr_t	data_cont3     : 1;
		mmr_t	data_1shot3    : 1;
		mmr_t	reserved_3     : 4;
		mmr_t	byte3          : 8;
		mmr_t	data_cb_cont2  : 1;
		mmr_t	data_cb_1shot2 : 1;
		mmr_t	data_cont2     : 1;
		mmr_t	data_1shot2    : 1;
		mmr_t	reserved_2     : 4;
		mmr_t	byte2          : 8;
		mmr_t	data_cb_cont1  : 1;
		mmr_t	data_cb_1shot1 : 1;
		mmr_t	data_cont1     : 1;
		mmr_t	data_1shot1    : 1;
		mmr_t	reserved_1     : 4;
		mmr_t	byte1          : 8;
		mmr_t	data_cb_cont0  : 1;
		mmr_t	data_cb_1shot0 : 1;
		mmr_t	data_cont0     : 1;
		mmr_t	data_1shot0    : 1;
		mmr_t	reserved_0     : 4;
		mmr_t	byte0          : 8;
	} sh_xnmd_ecc_inj_reg_s;
} sh_xnmd_ecc_inj_reg_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XNMD_ECC0_INJ_MASK_REG"                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnmd_ecc0_inj_mask_reg_u {
	mmr_t	sh_xnmd_ecc0_inj_mask_reg_regval;
	struct {
		mmr_t	mask_ecc0   : 64;
	} sh_xnmd_ecc0_inj_mask_reg_s;
} sh_xnmd_ecc0_inj_mask_reg_u_t;
#else
typedef union sh_xnmd_ecc0_inj_mask_reg_u {
	mmr_t	sh_xnmd_ecc0_inj_mask_reg_regval;
	struct {
		mmr_t	mask_ecc0   : 64;
	} sh_xnmd_ecc0_inj_mask_reg_s;
} sh_xnmd_ecc0_inj_mask_reg_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XNMD_ECC1_INJ_MASK_REG"                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnmd_ecc1_inj_mask_reg_u {
	mmr_t	sh_xnmd_ecc1_inj_mask_reg_regval;
	struct {
		mmr_t	mask_ecc1   : 64;
	} sh_xnmd_ecc1_inj_mask_reg_s;
} sh_xnmd_ecc1_inj_mask_reg_u_t;
#else
typedef union sh_xnmd_ecc1_inj_mask_reg_u {
	mmr_t	sh_xnmd_ecc1_inj_mask_reg_regval;
	struct {
		mmr_t	mask_ecc1   : 64;
	} sh_xnmd_ecc1_inj_mask_reg_s;
} sh_xnmd_ecc1_inj_mask_reg_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XNMD_ECC2_INJ_MASK_REG"                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnmd_ecc2_inj_mask_reg_u {
	mmr_t	sh_xnmd_ecc2_inj_mask_reg_regval;
	struct {
		mmr_t	mask_ecc2   : 64;
	} sh_xnmd_ecc2_inj_mask_reg_s;
} sh_xnmd_ecc2_inj_mask_reg_u_t;
#else
typedef union sh_xnmd_ecc2_inj_mask_reg_u {
	mmr_t	sh_xnmd_ecc2_inj_mask_reg_regval;
	struct {
		mmr_t	mask_ecc2   : 64;
	} sh_xnmd_ecc2_inj_mask_reg_s;
} sh_xnmd_ecc2_inj_mask_reg_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XNMD_ECC3_INJ_MASK_REG"                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnmd_ecc3_inj_mask_reg_u {
	mmr_t	sh_xnmd_ecc3_inj_mask_reg_regval;
	struct {
		mmr_t	mask_ecc3   : 64;
	} sh_xnmd_ecc3_inj_mask_reg_s;
} sh_xnmd_ecc3_inj_mask_reg_u_t;
#else
typedef union sh_xnmd_ecc3_inj_mask_reg_u {
	mmr_t	sh_xnmd_ecc3_inj_mask_reg_regval;
	struct {
		mmr_t	mask_ecc3   : 64;
	} sh_xnmd_ecc3_inj_mask_reg_s;
} sh_xnmd_ecc3_inj_mask_reg_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_XNMD_ECC_ERR_REPORT"                   */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnmd_ecc_err_report_u {
	mmr_t	sh_xnmd_ecc_err_report_regval;
	struct {
		mmr_t	ecc_disable0 : 1;
		mmr_t	reserved_0   : 15;
		mmr_t	ecc_disable1 : 1;
		mmr_t	reserved_1   : 15;
		mmr_t	ecc_disable2 : 1;
		mmr_t	reserved_2   : 15;
		mmr_t	ecc_disable3 : 1;
		mmr_t	reserved_3   : 15;
	} sh_xnmd_ecc_err_report_s;
} sh_xnmd_ecc_err_report_u_t;
#else
typedef union sh_xnmd_ecc_err_report_u {
	mmr_t	sh_xnmd_ecc_err_report_regval;
	struct {
		mmr_t	reserved_3   : 15;
		mmr_t	ecc_disable3 : 1;
		mmr_t	reserved_2   : 15;
		mmr_t	ecc_disable2 : 1;
		mmr_t	reserved_1   : 15;
		mmr_t	ecc_disable1 : 1;
		mmr_t	reserved_0   : 15;
		mmr_t	ecc_disable0 : 1;
	} sh_xnmd_ecc_err_report_s;
} sh_xnmd_ecc_err_report_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_NI0_ERROR_SUMMARY_1"                   */
/*                       ni0  Error Summary Bits                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni0_error_summary_1_u {
	mmr_t	sh_ni0_error_summary_1_regval;
	struct {
		mmr_t	overflow_fifo02_debit0        : 1;
		mmr_t	overflow_fifo02_debit2        : 1;
		mmr_t	overflow_fifo13_debit0        : 1;
		mmr_t	overflow_fifo13_debit2        : 1;
		mmr_t	overflow_fifo02_vc0_pop       : 1;
		mmr_t	overflow_fifo02_vc2_pop       : 1;
		mmr_t	overflow_fifo13_vc1_pop       : 1;
		mmr_t	overflow_fifo13_vc3_pop       : 1;
		mmr_t	overflow_fifo02_vc0_push      : 1;
		mmr_t	overflow_fifo02_vc2_push      : 1;
		mmr_t	overflow_fifo13_vc1_push      : 1;
		mmr_t	overflow_fifo13_vc3_push      : 1;
		mmr_t	overflow_fifo02_vc0_credit    : 1;
		mmr_t	overflow_fifo02_vc2_credit    : 1;
		mmr_t	overflow_fifo13_vc0_credit    : 1;
		mmr_t	overflow_fifo13_vc2_credit    : 1;
		mmr_t	overflow0_vc0_credit          : 1;
		mmr_t	overflow1_vc0_credit          : 1;
		mmr_t	overflow2_vc0_credit          : 1;
		mmr_t	overflow0_vc2_credit          : 1;
		mmr_t	overflow1_vc2_credit          : 1;
		mmr_t	overflow2_vc2_credit          : 1;
		mmr_t	overflow_pi_fifo_debit0       : 1;
		mmr_t	overflow_pi_fifo_debit2       : 1;
		mmr_t	overflow_iilb_fifo_debit0     : 1;
		mmr_t	overflow_iilb_fifo_debit2     : 1;
		mmr_t	overflow_md_fifo_debit0       : 1;
		mmr_t	overflow_md_fifo_debit2       : 1;
		mmr_t	overflow_ni_fifo_debit0       : 1;
		mmr_t	overflow_ni_fifo_debit1       : 1;
		mmr_t	overflow_ni_fifo_debit2       : 1;
		mmr_t	overflow_ni_fifo_debit3       : 1;
		mmr_t	overflow_pi_fifo_vc0_pop      : 1;
		mmr_t	overflow_pi_fifo_vc2_pop      : 1;
		mmr_t	overflow_iilb_fifo_vc0_pop    : 1;
		mmr_t	overflow_iilb_fifo_vc2_pop    : 1;
		mmr_t	overflow_md_fifo_vc0_pop      : 1;
		mmr_t	overflow_md_fifo_vc2_pop      : 1;
		mmr_t	overflow_ni_fifo_vc0_pop      : 1;
		mmr_t	overflow_ni_fifo_vc2_pop      : 1;
		mmr_t	overflow_pi_fifo_vc0_push     : 1;
		mmr_t	overflow_pi_fifo_vc2_push     : 1;
		mmr_t	overflow_iilb_fifo_vc0_push   : 1;
		mmr_t	overflow_iilb_fifo_vc2_push   : 1;
		mmr_t	overflow_md_fifo_vc0_push     : 1;
		mmr_t	overflow_md_fifo_vc2_push     : 1;
		mmr_t	overflow_pi_fifo_vc0_credit   : 1;
		mmr_t	overflow_pi_fifo_vc2_credit   : 1;
		mmr_t	overflow_iilb_fifo_vc0_credit : 1;
		mmr_t	overflow_iilb_fifo_vc2_credit : 1;
		mmr_t	overflow_md_fifo_vc0_credit   : 1;
		mmr_t	overflow_md_fifo_vc2_credit   : 1;
		mmr_t	overflow_ni_fifo_vc0_credit   : 1;
		mmr_t	overflow_ni_fifo_vc1_credit   : 1;
		mmr_t	overflow_ni_fifo_vc2_credit   : 1;
		mmr_t	overflow_ni_fifo_vc3_credit   : 1;
		mmr_t	tail_timeout_fifo02_vc0       : 1;
		mmr_t	tail_timeout_fifo02_vc2       : 1;
		mmr_t	tail_timeout_fifo13_vc1       : 1;
		mmr_t	tail_timeout_fifo13_vc3       : 1;
		mmr_t	tail_timeout_ni_vc0           : 1;
		mmr_t	tail_timeout_ni_vc1           : 1;
		mmr_t	tail_timeout_ni_vc2           : 1;
		mmr_t	tail_timeout_ni_vc3           : 1;
	} sh_ni0_error_summary_1_s;
} sh_ni0_error_summary_1_u_t;
#else
typedef union sh_ni0_error_summary_1_u {
	mmr_t	sh_ni0_error_summary_1_regval;
	struct {
		mmr_t	tail_timeout_ni_vc3           : 1;
		mmr_t	tail_timeout_ni_vc2           : 1;
		mmr_t	tail_timeout_ni_vc1           : 1;
		mmr_t	tail_timeout_ni_vc0           : 1;
		mmr_t	tail_timeout_fifo13_vc3       : 1;
		mmr_t	tail_timeout_fifo13_vc1       : 1;
		mmr_t	tail_timeout_fifo02_vc2       : 1;
		mmr_t	tail_timeout_fifo02_vc0       : 1;
		mmr_t	overflow_ni_fifo_vc3_credit   : 1;
		mmr_t	overflow_ni_fifo_vc2_credit   : 1;
		mmr_t	overflow_ni_fifo_vc1_credit   : 1;
		mmr_t	overflow_ni_fifo_vc0_credit   : 1;
		mmr_t	overflow_md_fifo_vc2_credit   : 1;
		mmr_t	overflow_md_fifo_vc0_credit   : 1;
		mmr_t	overflow_iilb_fifo_vc2_credit : 1;
		mmr_t	overflow_iilb_fifo_vc0_credit : 1;
		mmr_t	overflow_pi_fifo_vc2_credit   : 1;
		mmr_t	overflow_pi_fifo_vc0_credit   : 1;
		mmr_t	overflow_md_fifo_vc2_push     : 1;
		mmr_t	overflow_md_fifo_vc0_push     : 1;
		mmr_t	overflow_iilb_fifo_vc2_push   : 1;
		mmr_t	overflow_iilb_fifo_vc0_push   : 1;
		mmr_t	overflow_pi_fifo_vc2_push     : 1;
		mmr_t	overflow_pi_fifo_vc0_push     : 1;
		mmr_t	overflow_ni_fifo_vc2_pop      : 1;
		mmr_t	overflow_ni_fifo_vc0_pop      : 1;
		mmr_t	overflow_md_fifo_vc2_pop      : 1;
		mmr_t	overflow_md_fifo_vc0_pop      : 1;
		mmr_t	overflow_iilb_fifo_vc2_pop    : 1;
		mmr_t	overflow_iilb_fifo_vc0_pop    : 1;
		mmr_t	overflow_pi_fifo_vc2_pop      : 1;
		mmr_t	overflow_pi_fifo_vc0_pop      : 1;
		mmr_t	overflow_ni_fifo_debit3       : 1;
		mmr_t	overflow_ni_fifo_debit2       : 1;
		mmr_t	overflow_ni_fifo_debit1       : 1;
		mmr_t	overflow_ni_fifo_debit0       : 1;
		mmr_t	overflow_md_fifo_debit2       : 1;
		mmr_t	overflow_md_fifo_debit0       : 1;
		mmr_t	overflow_iilb_fifo_debit2     : 1;
		mmr_t	overflow_iilb_fifo_debit0     : 1;
		mmr_t	overflow_pi_fifo_debit2       : 1;
		mmr_t	overflow_pi_fifo_debit0       : 1;
		mmr_t	overflow2_vc2_credit          : 1;
		mmr_t	overflow1_vc2_credit          : 1;
		mmr_t	overflow0_vc2_credit          : 1;
		mmr_t	overflow2_vc0_credit          : 1;
		mmr_t	overflow1_vc0_credit          : 1;
		mmr_t	overflow0_vc0_credit          : 1;
		mmr_t	overflow_fifo13_vc2_credit    : 1;
		mmr_t	overflow_fifo13_vc0_credit    : 1;
		mmr_t	overflow_fifo02_vc2_credit    : 1;
		mmr_t	overflow_fifo02_vc0_credit    : 1;
		mmr_t	overflow_fifo13_vc3_push      : 1;
		mmr_t	overflow_fifo13_vc1_push      : 1;
		mmr_t	overflow_fifo02_vc2_push      : 1;
		mmr_t	overflow_fifo02_vc0_push      : 1;
		mmr_t	overflow_fifo13_vc3_pop       : 1;
		mmr_t	overflow_fifo13_vc1_pop       : 1;
		mmr_t	overflow_fifo02_vc2_pop       : 1;
		mmr_t	overflow_fifo02_vc0_pop       : 1;
		mmr_t	overflow_fifo13_debit2        : 1;
		mmr_t	overflow_fifo13_debit0        : 1;
		mmr_t	overflow_fifo02_debit2        : 1;
		mmr_t	overflow_fifo02_debit0        : 1;
	} sh_ni0_error_summary_1_s;
} sh_ni0_error_summary_1_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_NI0_ERROR_SUMMARY_2"                   */
/*                       ni0  Error Summary Bits                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni0_error_summary_2_u {
	mmr_t	sh_ni0_error_summary_2_regval;
	struct {
		mmr_t	illegal_vcni                   : 1;
		mmr_t	illegal_vcpi                   : 1;
		mmr_t	illegal_vcmd                   : 1;
		mmr_t	illegal_vciilb                 : 1;
		mmr_t	underflow_fifo02_vc0_pop       : 1;
		mmr_t	underflow_fifo02_vc2_pop       : 1;
		mmr_t	underflow_fifo13_vc1_pop       : 1;
		mmr_t	underflow_fifo13_vc3_pop       : 1;
		mmr_t	underflow_fifo02_vc0_push      : 1;
		mmr_t	underflow_fifo02_vc2_push      : 1;
		mmr_t	underflow_fifo13_vc1_push      : 1;
		mmr_t	underflow_fifo13_vc3_push      : 1;
		mmr_t	underflow_fifo02_vc0_credit    : 1;
		mmr_t	underflow_fifo02_vc2_credit    : 1;
		mmr_t	underflow_fifo13_vc0_credit    : 1;
		mmr_t	underflow_fifo13_vc2_credit    : 1;
		mmr_t	underflow0_vc0_credit          : 1;
		mmr_t	underflow1_vc0_credit          : 1;
		mmr_t	underflow2_vc0_credit          : 1;
		mmr_t	underflow0_vc2_credit          : 1;
		mmr_t	underflow1_vc2_credit          : 1;
		mmr_t	underflow2_vc2_credit          : 1;
		mmr_t	reserved_0                     : 10;
		mmr_t	underflow_pi_fifo_vc0_pop      : 1;
		mmr_t	underflow_pi_fifo_vc2_pop      : 1;
		mmr_t	underflow_iilb_fifo_vc0_pop    : 1;
		mmr_t	underflow_iilb_fifo_vc2_pop    : 1;
		mmr_t	underflow_md_fifo_vc0_pop      : 1;
		mmr_t	underflow_md_fifo_vc2_pop      : 1;
		mmr_t	underflow_ni_fifo_vc0_pop      : 1;
		mmr_t	underflow_ni_fifo_vc2_pop      : 1;
		mmr_t	underflow_pi_fifo_vc0_push     : 1;
		mmr_t	underflow_pi_fifo_vc2_push     : 1;
		mmr_t	underflow_iilb_fifo_vc0_push   : 1;
		mmr_t	underflow_iilb_fifo_vc2_push   : 1;
		mmr_t	underflow_md_fifo_vc0_push     : 1;
		mmr_t	underflow_md_fifo_vc2_push     : 1;
		mmr_t	underflow_pi_fifo_vc0_credit   : 1;
		mmr_t	underflow_pi_fifo_vc2_credit   : 1;
		mmr_t	underflow_iilb_fifo_vc0_credit : 1;
		mmr_t	underflow_iilb_fifo_vc2_credit : 1;
		mmr_t	underflow_md_fifo_vc0_credit   : 1;
		mmr_t	underflow_md_fifo_vc2_credit   : 1;
		mmr_t	underflow_ni_fifo_vc0_credit   : 1;
		mmr_t	underflow_ni_fifo_vc1_credit   : 1;
		mmr_t	underflow_ni_fifo_vc2_credit   : 1;
		mmr_t	underflow_ni_fifo_vc3_credit   : 1;
		mmr_t	llp_deadlock_vc0               : 1;
		mmr_t	llp_deadlock_vc1               : 1;
		mmr_t	llp_deadlock_vc2               : 1;
		mmr_t	llp_deadlock_vc3               : 1;
		mmr_t	chiplet_nomatch                : 1;
		mmr_t	lut_read_error                 : 1;
		mmr_t	retry_timeout_error            : 1;
		mmr_t	reserved_1                     : 1;
	} sh_ni0_error_summary_2_s;
} sh_ni0_error_summary_2_u_t;
#else
typedef union sh_ni0_error_summary_2_u {
	mmr_t	sh_ni0_error_summary_2_regval;
	struct {
		mmr_t	reserved_1                     : 1;
		mmr_t	retry_timeout_error            : 1;
		mmr_t	lut_read_error                 : 1;
		mmr_t	chiplet_nomatch                : 1;
		mmr_t	llp_deadlock_vc3               : 1;
		mmr_t	llp_deadlock_vc2               : 1;
		mmr_t	llp_deadlock_vc1               : 1;
		mmr_t	llp_deadlock_vc0               : 1;
		mmr_t	underflow_ni_fifo_vc3_credit   : 1;
		mmr_t	underflow_ni_fifo_vc2_credit   : 1;
		mmr_t	underflow_ni_fifo_vc1_credit   : 1;
		mmr_t	underflow_ni_fifo_vc0_credit   : 1;
		mmr_t	underflow_md_fifo_vc2_credit   : 1;
		mmr_t	underflow_md_fifo_vc0_credit   : 1;
		mmr_t	underflow_iilb_fifo_vc2_credit : 1;
		mmr_t	underflow_iilb_fifo_vc0_credit : 1;
		mmr_t	underflow_pi_fifo_vc2_credit   : 1;
		mmr_t	underflow_pi_fifo_vc0_credit   : 1;
		mmr_t	underflow_md_fifo_vc2_push     : 1;
		mmr_t	underflow_md_fifo_vc0_push     : 1;
		mmr_t	underflow_iilb_fifo_vc2_push   : 1;
		mmr_t	underflow_iilb_fifo_vc0_push   : 1;
		mmr_t	underflow_pi_fifo_vc2_push     : 1;
		mmr_t	underflow_pi_fifo_vc0_push     : 1;
		mmr_t	underflow_ni_fifo_vc2_pop      : 1;
		mmr_t	underflow_ni_fifo_vc0_pop      : 1;
		mmr_t	underflow_md_fifo_vc2_pop      : 1;
		mmr_t	underflow_md_fifo_vc0_pop      : 1;
		mmr_t	underflow_iilb_fifo_vc2_pop    : 1;
		mmr_t	underflow_iilb_fifo_vc0_pop    : 1;
		mmr_t	underflow_pi_fifo_vc2_pop      : 1;
		mmr_t	underflow_pi_fifo_vc0_pop      : 1;
		mmr_t	reserved_0                     : 10;
		mmr_t	underflow2_vc2_credit          : 1;
		mmr_t	underflow1_vc2_credit          : 1;
		mmr_t	underflow0_vc2_credit          : 1;
		mmr_t	underflow2_vc0_credit          : 1;
		mmr_t	underflow1_vc0_credit          : 1;
		mmr_t	underflow0_vc0_credit          : 1;
		mmr_t	underflow_fifo13_vc2_credit    : 1;
		mmr_t	underflow_fifo13_vc0_credit    : 1;
		mmr_t	underflow_fifo02_vc2_credit    : 1;
		mmr_t	underflow_fifo02_vc0_credit    : 1;
		mmr_t	underflow_fifo13_vc3_push      : 1;
		mmr_t	underflow_fifo13_vc1_push      : 1;
		mmr_t	underflow_fifo02_vc2_push      : 1;
		mmr_t	underflow_fifo02_vc0_push      : 1;
		mmr_t	underflow_fifo13_vc3_pop       : 1;
		mmr_t	underflow_fifo13_vc1_pop       : 1;
		mmr_t	underflow_fifo02_vc2_pop       : 1;
		mmr_t	underflow_fifo02_vc0_pop       : 1;
		mmr_t	illegal_vciilb                 : 1;
		mmr_t	illegal_vcmd                   : 1;
		mmr_t	illegal_vcpi                   : 1;
		mmr_t	illegal_vcni                   : 1;
	} sh_ni0_error_summary_2_s;
} sh_ni0_error_summary_2_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_NI0_ERROR_OVERFLOW_1"                  */
/*                       ni0  Error Overflow Bits                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni0_error_overflow_1_u {
	mmr_t	sh_ni0_error_overflow_1_regval;
	struct {
		mmr_t	overflow_fifo02_debit0        : 1;
		mmr_t	overflow_fifo02_debit2        : 1;
		mmr_t	overflow_fifo13_debit0        : 1;
		mmr_t	overflow_fifo13_debit2        : 1;
		mmr_t	overflow_fifo02_vc0_pop       : 1;
		mmr_t	overflow_fifo02_vc2_pop       : 1;
		mmr_t	overflow_fifo13_vc1_pop       : 1;
		mmr_t	overflow_fifo13_vc3_pop       : 1;
		mmr_t	overflow_fifo02_vc0_push      : 1;
		mmr_t	overflow_fifo02_vc2_push      : 1;
		mmr_t	overflow_fifo13_vc1_push      : 1;
		mmr_t	overflow_fifo13_vc3_push      : 1;
		mmr_t	overflow_fifo02_vc0_credit    : 1;
		mmr_t	overflow_fifo02_vc2_credit    : 1;
		mmr_t	overflow_fifo13_vc0_credit    : 1;
		mmr_t	overflow_fifo13_vc2_credit    : 1;
		mmr_t	overflow0_vc0_credit          : 1;
		mmr_t	overflow1_vc0_credit          : 1;
		mmr_t	overflow2_vc0_credit          : 1;
		mmr_t	overflow0_vc2_credit          : 1;
		mmr_t	overflow1_vc2_credit          : 1;
		mmr_t	overflow2_vc2_credit          : 1;
		mmr_t	overflow_pi_fifo_debit0       : 1;
		mmr_t	overflow_pi_fifo_debit2       : 1;
		mmr_t	overflow_iilb_fifo_debit0     : 1;
		mmr_t	overflow_iilb_fifo_debit2     : 1;
		mmr_t	overflow_md_fifo_debit0       : 1;
		mmr_t	overflow_md_fifo_debit2       : 1;
		mmr_t	overflow_ni_fifo_debit0       : 1;
		mmr_t	overflow_ni_fifo_debit1       : 1;
		mmr_t	overflow_ni_fifo_debit2       : 1;
		mmr_t	overflow_ni_fifo_debit3       : 1;
		mmr_t	overflow_pi_fifo_vc0_pop      : 1;
		mmr_t	overflow_pi_fifo_vc2_pop      : 1;
		mmr_t	overflow_iilb_fifo_vc0_pop    : 1;
		mmr_t	overflow_iilb_fifo_vc2_pop    : 1;
		mmr_t	overflow_md_fifo_vc0_pop      : 1;
		mmr_t	overflow_md_fifo_vc2_pop      : 1;
		mmr_t	overflow_ni_fifo_vc0_pop      : 1;
		mmr_t	overflow_ni_fifo_vc2_pop      : 1;
		mmr_t	overflow_pi_fifo_vc0_push     : 1;
		mmr_t	overflow_pi_fifo_vc2_push     : 1;
		mmr_t	overflow_iilb_fifo_vc0_push   : 1;
		mmr_t	overflow_iilb_fifo_vc2_push   : 1;
		mmr_t	overflow_md_fifo_vc0_push     : 1;
		mmr_t	overflow_md_fifo_vc2_push     : 1;
		mmr_t	overflow_pi_fifo_vc0_credit   : 1;
		mmr_t	overflow_pi_fifo_vc2_credit   : 1;
		mmr_t	overflow_iilb_fifo_vc0_credit : 1;
		mmr_t	overflow_iilb_fifo_vc2_credit : 1;
		mmr_t	overflow_md_fifo_vc0_credit   : 1;
		mmr_t	overflow_md_fifo_vc2_credit   : 1;
		mmr_t	overflow_ni_fifo_vc0_credit   : 1;
		mmr_t	overflow_ni_fifo_vc1_credit   : 1;
		mmr_t	overflow_ni_fifo_vc2_credit   : 1;
		mmr_t	overflow_ni_fifo_vc3_credit   : 1;
		mmr_t	tail_timeout_fifo02_vc0       : 1;
		mmr_t	tail_timeout_fifo02_vc2       : 1;
		mmr_t	tail_timeout_fifo13_vc1       : 1;
		mmr_t	tail_timeout_fifo13_vc3       : 1;
		mmr_t	tail_timeout_ni_vc0           : 1;
		mmr_t	tail_timeout_ni_vc1           : 1;
		mmr_t	tail_timeout_ni_vc2           : 1;
		mmr_t	tail_timeout_ni_vc3           : 1;
	} sh_ni0_error_overflow_1_s;
} sh_ni0_error_overflow_1_u_t;
#else
typedef union sh_ni0_error_overflow_1_u {
	mmr_t	sh_ni0_error_overflow_1_regval;
	struct {
		mmr_t	tail_timeout_ni_vc3           : 1;
		mmr_t	tail_timeout_ni_vc2           : 1;
		mmr_t	tail_timeout_ni_vc1           : 1;
		mmr_t	tail_timeout_ni_vc0           : 1;
		mmr_t	tail_timeout_fifo13_vc3       : 1;
		mmr_t	tail_timeout_fifo13_vc1       : 1;
		mmr_t	tail_timeout_fifo02_vc2       : 1;
		mmr_t	tail_timeout_fifo02_vc0       : 1;
		mmr_t	overflow_ni_fifo_vc3_credit   : 1;
		mmr_t	overflow_ni_fifo_vc2_credit   : 1;
		mmr_t	overflow_ni_fifo_vc1_credit   : 1;
		mmr_t	overflow_ni_fifo_vc0_credit   : 1;
		mmr_t	overflow_md_fifo_vc2_credit   : 1;
		mmr_t	overflow_md_fifo_vc0_credit   : 1;
		mmr_t	overflow_iilb_fifo_vc2_credit : 1;
		mmr_t	overflow_iilb_fifo_vc0_credit : 1;
		mmr_t	overflow_pi_fifo_vc2_credit   : 1;
		mmr_t	overflow_pi_fifo_vc0_credit   : 1;
		mmr_t	overflow_md_fifo_vc2_push     : 1;
		mmr_t	overflow_md_fifo_vc0_push     : 1;
		mmr_t	overflow_iilb_fifo_vc2_push   : 1;
		mmr_t	overflow_iilb_fifo_vc0_push   : 1;
		mmr_t	overflow_pi_fifo_vc2_push     : 1;
		mmr_t	overflow_pi_fifo_vc0_push     : 1;
		mmr_t	overflow_ni_fifo_vc2_pop      : 1;
		mmr_t	overflow_ni_fifo_vc0_pop      : 1;
		mmr_t	overflow_md_fifo_vc2_pop      : 1;
		mmr_t	overflow_md_fifo_vc0_pop      : 1;
		mmr_t	overflow_iilb_fifo_vc2_pop    : 1;
		mmr_t	overflow_iilb_fifo_vc0_pop    : 1;
		mmr_t	overflow_pi_fifo_vc2_pop      : 1;
		mmr_t	overflow_pi_fifo_vc0_pop      : 1;
		mmr_t	overflow_ni_fifo_debit3       : 1;
		mmr_t	overflow_ni_fifo_debit2       : 1;
		mmr_t	overflow_ni_fifo_debit1       : 1;
		mmr_t	overflow_ni_fifo_debit0       : 1;
		mmr_t	overflow_md_fifo_debit2       : 1;
		mmr_t	overflow_md_fifo_debit0       : 1;
		mmr_t	overflow_iilb_fifo_debit2     : 1;
		mmr_t	overflow_iilb_fifo_debit0     : 1;
		mmr_t	overflow_pi_fifo_debit2       : 1;
		mmr_t	overflow_pi_fifo_debit0       : 1;
		mmr_t	overflow2_vc2_credit          : 1;
		mmr_t	overflow1_vc2_credit          : 1;
		mmr_t	overflow0_vc2_credit          : 1;
		mmr_t	overflow2_vc0_credit          : 1;
		mmr_t	overflow1_vc0_credit          : 1;
		mmr_t	overflow0_vc0_credit          : 1;
		mmr_t	overflow_fifo13_vc2_credit    : 1;
		mmr_t	overflow_fifo13_vc0_credit    : 1;
		mmr_t	overflow_fifo02_vc2_credit    : 1;
		mmr_t	overflow_fifo02_vc0_credit    : 1;
		mmr_t	overflow_fifo13_vc3_push      : 1;
		mmr_t	overflow_fifo13_vc1_push      : 1;
		mmr_t	overflow_fifo02_vc2_push      : 1;
		mmr_t	overflow_fifo02_vc0_push      : 1;
		mmr_t	overflow_fifo13_vc3_pop       : 1;
		mmr_t	overflow_fifo13_vc1_pop       : 1;
		mmr_t	overflow_fifo02_vc2_pop       : 1;
		mmr_t	overflow_fifo02_vc0_pop       : 1;
		mmr_t	overflow_fifo13_debit2        : 1;
		mmr_t	overflow_fifo13_debit0        : 1;
		mmr_t	overflow_fifo02_debit2        : 1;
		mmr_t	overflow_fifo02_debit0        : 1;
	} sh_ni0_error_overflow_1_s;
} sh_ni0_error_overflow_1_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_NI0_ERROR_OVERFLOW_2"                  */
/*                       ni0  Error Overflow Bits                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni0_error_overflow_2_u {
	mmr_t	sh_ni0_error_overflow_2_regval;
	struct {
		mmr_t	illegal_vcni                   : 1;
		mmr_t	illegal_vcpi                   : 1;
		mmr_t	illegal_vcmd                   : 1;
		mmr_t	illegal_vciilb                 : 1;
		mmr_t	underflow_fifo02_vc0_pop       : 1;
		mmr_t	underflow_fifo02_vc2_pop       : 1;
		mmr_t	underflow_fifo13_vc1_pop       : 1;
		mmr_t	underflow_fifo13_vc3_pop       : 1;
		mmr_t	underflow_fifo02_vc0_push      : 1;
		mmr_t	underflow_fifo02_vc2_push      : 1;
		mmr_t	underflow_fifo13_vc1_push      : 1;
		mmr_t	underflow_fifo13_vc3_push      : 1;
		mmr_t	underflow_fifo02_vc0_credit    : 1;
		mmr_t	underflow_fifo02_vc2_credit    : 1;
		mmr_t	underflow_fifo13_vc0_credit    : 1;
		mmr_t	underflow_fifo13_vc2_credit    : 1;
		mmr_t	underflow0_vc0_credit          : 1;
		mmr_t	underflow1_vc0_credit          : 1;
		mmr_t	underflow2_vc0_credit          : 1;
		mmr_t	underflow0_vc2_credit          : 1;
		mmr_t	underflow1_vc2_credit          : 1;
		mmr_t	underflow2_vc2_credit          : 1;
		mmr_t	reserved_0                     : 10;
		mmr_t	underflow_pi_fifo_vc0_pop      : 1;
		mmr_t	underflow_pi_fifo_vc2_pop      : 1;
		mmr_t	underflow_iilb_fifo_vc0_pop    : 1;
		mmr_t	underflow_iilb_fifo_vc2_pop    : 1;
		mmr_t	underflow_md_fifo_vc0_pop      : 1;
		mmr_t	underflow_md_fifo_vc2_pop      : 1;
		mmr_t	underflow_ni_fifo_vc0_pop      : 1;
		mmr_t	underflow_ni_fifo_vc2_pop      : 1;
		mmr_t	underflow_pi_fifo_vc0_push     : 1;
		mmr_t	underflow_pi_fifo_vc2_push     : 1;
		mmr_t	underflow_iilb_fifo_vc0_push   : 1;
		mmr_t	underflow_iilb_fifo_vc2_push   : 1;
		mmr_t	underflow_md_fifo_vc0_push     : 1;
		mmr_t	underflow_md_fifo_vc2_push     : 1;
		mmr_t	underflow_pi_fifo_vc0_credit   : 1;
		mmr_t	underflow_pi_fifo_vc2_credit   : 1;
		mmr_t	underflow_iilb_fifo_vc0_credit : 1;
		mmr_t	underflow_iilb_fifo_vc2_credit : 1;
		mmr_t	underflow_md_fifo_vc0_credit   : 1;
		mmr_t	underflow_md_fifo_vc2_credit   : 1;
		mmr_t	underflow_ni_fifo_vc0_credit   : 1;
		mmr_t	underflow_ni_fifo_vc1_credit   : 1;
		mmr_t	underflow_ni_fifo_vc2_credit   : 1;
		mmr_t	underflow_ni_fifo_vc3_credit   : 1;
		mmr_t	llp_deadlock_vc0               : 1;
		mmr_t	llp_deadlock_vc1               : 1;
		mmr_t	llp_deadlock_vc2               : 1;
		mmr_t	llp_deadlock_vc3               : 1;
		mmr_t	chiplet_nomatch                : 1;
		mmr_t	lut_read_error                 : 1;
		mmr_t	retry_timeout_error            : 1;
		mmr_t	reserved_1                     : 1;
	} sh_ni0_error_overflow_2_s;
} sh_ni0_error_overflow_2_u_t;
#else
typedef union sh_ni0_error_overflow_2_u {
	mmr_t	sh_ni0_error_overflow_2_regval;
	struct {
		mmr_t	reserved_1                     : 1;
		mmr_t	retry_timeout_error            : 1;
		mmr_t	lut_read_error                 : 1;
		mmr_t	chiplet_nomatch                : 1;
		mmr_t	llp_deadlock_vc3               : 1;
		mmr_t	llp_deadlock_vc2               : 1;
		mmr_t	llp_deadlock_vc1               : 1;
		mmr_t	llp_deadlock_vc0               : 1;
		mmr_t	underflow_ni_fifo_vc3_credit   : 1;
		mmr_t	underflow_ni_fifo_vc2_credit   : 1;
		mmr_t	underflow_ni_fifo_vc1_credit   : 1;
		mmr_t	underflow_ni_fifo_vc0_credit   : 1;
		mmr_t	underflow_md_fifo_vc2_credit   : 1;
		mmr_t	underflow_md_fifo_vc0_credit   : 1;
		mmr_t	underflow_iilb_fifo_vc2_credit : 1;
		mmr_t	underflow_iilb_fifo_vc0_credit : 1;
		mmr_t	underflow_pi_fifo_vc2_credit   : 1;
		mmr_t	underflow_pi_fifo_vc0_credit   : 1;
		mmr_t	underflow_md_fifo_vc2_push     : 1;
		mmr_t	underflow_md_fifo_vc0_push     : 1;
		mmr_t	underflow_iilb_fifo_vc2_push   : 1;
		mmr_t	underflow_iilb_fifo_vc0_push   : 1;
		mmr_t	underflow_pi_fifo_vc2_push     : 1;
		mmr_t	underflow_pi_fifo_vc0_push     : 1;
		mmr_t	underflow_ni_fifo_vc2_pop      : 1;
		mmr_t	underflow_ni_fifo_vc0_pop      : 1;
		mmr_t	underflow_md_fifo_vc2_pop      : 1;
		mmr_t	underflow_md_fifo_vc0_pop      : 1;
		mmr_t	underflow_iilb_fifo_vc2_pop    : 1;
		mmr_t	underflow_iilb_fifo_vc0_pop    : 1;
		mmr_t	underflow_pi_fifo_vc2_pop      : 1;
		mmr_t	underflow_pi_fifo_vc0_pop      : 1;
		mmr_t	reserved_0                     : 10;
		mmr_t	underflow2_vc2_credit          : 1;
		mmr_t	underflow1_vc2_credit          : 1;
		mmr_t	underflow0_vc2_credit          : 1;
		mmr_t	underflow2_vc0_credit          : 1;
		mmr_t	underflow1_vc0_credit          : 1;
		mmr_t	underflow0_vc0_credit          : 1;
		mmr_t	underflow_fifo13_vc2_credit    : 1;
		mmr_t	underflow_fifo13_vc0_credit    : 1;
		mmr_t	underflow_fifo02_vc2_credit    : 1;
		mmr_t	underflow_fifo02_vc0_credit    : 1;
		mmr_t	underflow_fifo13_vc3_push      : 1;
		mmr_t	underflow_fifo13_vc1_push      : 1;
		mmr_t	underflow_fifo02_vc2_push      : 1;
		mmr_t	underflow_fifo02_vc0_push      : 1;
		mmr_t	underflow_fifo13_vc3_pop       : 1;
		mmr_t	underflow_fifo13_vc1_pop       : 1;
		mmr_t	underflow_fifo02_vc2_pop       : 1;
		mmr_t	underflow_fifo02_vc0_pop       : 1;
		mmr_t	illegal_vciilb                 : 1;
		mmr_t	illegal_vcmd                   : 1;
		mmr_t	illegal_vcpi                   : 1;
		mmr_t	illegal_vcni                   : 1;
	} sh_ni0_error_overflow_2_s;
} sh_ni0_error_overflow_2_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_NI0_ERROR_MASK_1"                    */
/*                         ni0  Error Mask Bits                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni0_error_mask_1_u {
	mmr_t	sh_ni0_error_mask_1_regval;
	struct {
		mmr_t	overflow_fifo02_debit0        : 1;
		mmr_t	overflow_fifo02_debit2        : 1;
		mmr_t	overflow_fifo13_debit0        : 1;
		mmr_t	overflow_fifo13_debit2        : 1;
		mmr_t	overflow_fifo02_vc0_pop       : 1;
		mmr_t	overflow_fifo02_vc2_pop       : 1;
		mmr_t	overflow_fifo13_vc1_pop       : 1;
		mmr_t	overflow_fifo13_vc3_pop       : 1;
		mmr_t	overflow_fifo02_vc0_push      : 1;
		mmr_t	overflow_fifo02_vc2_push      : 1;
		mmr_t	overflow_fifo13_vc1_push      : 1;
		mmr_t	overflow_fifo13_vc3_push      : 1;
		mmr_t	overflow_fifo02_vc0_credit    : 1;
		mmr_t	overflow_fifo02_vc2_credit    : 1;
		mmr_t	overflow_fifo13_vc0_credit    : 1;
		mmr_t	overflow_fifo13_vc2_credit    : 1;
		mmr_t	overflow0_vc0_credit          : 1;
		mmr_t	overflow1_vc0_credit          : 1;
		mmr_t	overflow2_vc0_credit          : 1;
		mmr_t	overflow0_vc2_credit          : 1;
		mmr_t	overflow1_vc2_credit          : 1;
		mmr_t	overflow2_vc2_credit          : 1;
		mmr_t	overflow_pi_fifo_debit0       : 1;
		mmr_t	overflow_pi_fifo_debit2       : 1;
		mmr_t	overflow_iilb_fifo_debit0     : 1;
		mmr_t	overflow_iilb_fifo_debit2     : 1;
		mmr_t	overflow_md_fifo_debit0       : 1;
		mmr_t	overflow_md_fifo_debit2       : 1;
		mmr_t	overflow_ni_fifo_debit0       : 1;
		mmr_t	overflow_ni_fifo_debit1       : 1;
		mmr_t	overflow_ni_fifo_debit2       : 1;
		mmr_t	overflow_ni_fifo_debit3       : 1;
		mmr_t	overflow_pi_fifo_vc0_pop      : 1;
		mmr_t	overflow_pi_fifo_vc2_pop      : 1;
		mmr_t	overflow_iilb_fifo_vc0_pop    : 1;
		mmr_t	overflow_iilb_fifo_vc2_pop    : 1;
		mmr_t	overflow_md_fifo_vc0_pop      : 1;
		mmr_t	overflow_md_fifo_vc2_pop      : 1;
		mmr_t	overflow_ni_fifo_vc0_pop      : 1;
		mmr_t	overflow_ni_fifo_vc2_pop      : 1;
		mmr_t	overflow_pi_fifo_vc0_push     : 1;
		mmr_t	overflow_pi_fifo_vc2_push     : 1;
		mmr_t	overflow_iilb_fifo_vc0_push   : 1;
		mmr_t	overflow_iilb_fifo_vc2_push   : 1;
		mmr_t	overflow_md_fifo_vc0_push     : 1;
		mmr_t	overflow_md_fifo_vc2_push     : 1;
		mmr_t	overflow_pi_fifo_vc0_credit   : 1;
		mmr_t	overflow_pi_fifo_vc2_credit   : 1;
		mmr_t	overflow_iilb_fifo_vc0_credit : 1;
		mmr_t	overflow_iilb_fifo_vc2_credit : 1;
		mmr_t	overflow_md_fifo_vc0_credit   : 1;
		mmr_t	overflow_md_fifo_vc2_credit   : 1;
		mmr_t	overflow_ni_fifo_vc0_credit   : 1;
		mmr_t	overflow_ni_fifo_vc1_credit   : 1;
		mmr_t	overflow_ni_fifo_vc2_credit   : 1;
		mmr_t	overflow_ni_fifo_vc3_credit   : 1;
		mmr_t	tail_timeout_fifo02_vc0       : 1;
		mmr_t	tail_timeout_fifo02_vc2       : 1;
		mmr_t	tail_timeout_fifo13_vc1       : 1;
		mmr_t	tail_timeout_fifo13_vc3       : 1;
		mmr_t	tail_timeout_ni_vc0           : 1;
		mmr_t	tail_timeout_ni_vc1           : 1;
		mmr_t	tail_timeout_ni_vc2           : 1;
		mmr_t	tail_timeout_ni_vc3           : 1;
	} sh_ni0_error_mask_1_s;
} sh_ni0_error_mask_1_u_t;
#else
typedef union sh_ni0_error_mask_1_u {
	mmr_t	sh_ni0_error_mask_1_regval;
	struct {
		mmr_t	tail_timeout_ni_vc3           : 1;
		mmr_t	tail_timeout_ni_vc2           : 1;
		mmr_t	tail_timeout_ni_vc1           : 1;
		mmr_t	tail_timeout_ni_vc0           : 1;
		mmr_t	tail_timeout_fifo13_vc3       : 1;
		mmr_t	tail_timeout_fifo13_vc1       : 1;
		mmr_t	tail_timeout_fifo02_vc2       : 1;
		mmr_t	tail_timeout_fifo02_vc0       : 1;
		mmr_t	overflow_ni_fifo_vc3_credit   : 1;
		mmr_t	overflow_ni_fifo_vc2_credit   : 1;
		mmr_t	overflow_ni_fifo_vc1_credit   : 1;
		mmr_t	overflow_ni_fifo_vc0_credit   : 1;
		mmr_t	overflow_md_fifo_vc2_credit   : 1;
		mmr_t	overflow_md_fifo_vc0_credit   : 1;
		mmr_t	overflow_iilb_fifo_vc2_credit : 1;
		mmr_t	overflow_iilb_fifo_vc0_credit : 1;
		mmr_t	overflow_pi_fifo_vc2_credit   : 1;
		mmr_t	overflow_pi_fifo_vc0_credit   : 1;
		mmr_t	overflow_md_fifo_vc2_push     : 1;
		mmr_t	overflow_md_fifo_vc0_push     : 1;
		mmr_t	overflow_iilb_fifo_vc2_push   : 1;
		mmr_t	overflow_iilb_fifo_vc0_push   : 1;
		mmr_t	overflow_pi_fifo_vc2_push     : 1;
		mmr_t	overflow_pi_fifo_vc0_push     : 1;
		mmr_t	overflow_ni_fifo_vc2_pop      : 1;
		mmr_t	overflow_ni_fifo_vc0_pop      : 1;
		mmr_t	overflow_md_fifo_vc2_pop      : 1;
		mmr_t	overflow_md_fifo_vc0_pop      : 1;
		mmr_t	overflow_iilb_fifo_vc2_pop    : 1;
		mmr_t	overflow_iilb_fifo_vc0_pop    : 1;
		mmr_t	overflow_pi_fifo_vc2_pop      : 1;
		mmr_t	overflow_pi_fifo_vc0_pop      : 1;
		mmr_t	overflow_ni_fifo_debit3       : 1;
		mmr_t	overflow_ni_fifo_debit2       : 1;
		mmr_t	overflow_ni_fifo_debit1       : 1;
		mmr_t	overflow_ni_fifo_debit0       : 1;
		mmr_t	overflow_md_fifo_debit2       : 1;
		mmr_t	overflow_md_fifo_debit0       : 1;
		mmr_t	overflow_iilb_fifo_debit2     : 1;
		mmr_t	overflow_iilb_fifo_debit0     : 1;
		mmr_t	overflow_pi_fifo_debit2       : 1;
		mmr_t	overflow_pi_fifo_debit0       : 1;
		mmr_t	overflow2_vc2_credit          : 1;
		mmr_t	overflow1_vc2_credit          : 1;
		mmr_t	overflow0_vc2_credit          : 1;
		mmr_t	overflow2_vc0_credit          : 1;
		mmr_t	overflow1_vc0_credit          : 1;
		mmr_t	overflow0_vc0_credit          : 1;
		mmr_t	overflow_fifo13_vc2_credit    : 1;
		mmr_t	overflow_fifo13_vc0_credit    : 1;
		mmr_t	overflow_fifo02_vc2_credit    : 1;
		mmr_t	overflow_fifo02_vc0_credit    : 1;
		mmr_t	overflow_fifo13_vc3_push      : 1;
		mmr_t	overflow_fifo13_vc1_push      : 1;
		mmr_t	overflow_fifo02_vc2_push      : 1;
		mmr_t	overflow_fifo02_vc0_push      : 1;
		mmr_t	overflow_fifo13_vc3_pop       : 1;
		mmr_t	overflow_fifo13_vc1_pop       : 1;
		mmr_t	overflow_fifo02_vc2_pop       : 1;
		mmr_t	overflow_fifo02_vc0_pop       : 1;
		mmr_t	overflow_fifo13_debit2        : 1;
		mmr_t	overflow_fifo13_debit0        : 1;
		mmr_t	overflow_fifo02_debit2        : 1;
		mmr_t	overflow_fifo02_debit0        : 1;
	} sh_ni0_error_mask_1_s;
} sh_ni0_error_mask_1_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_NI0_ERROR_MASK_2"                    */
/*                         ni0  Error Mask Bits                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni0_error_mask_2_u {
	mmr_t	sh_ni0_error_mask_2_regval;
	struct {
		mmr_t	illegal_vcni                   : 1;
		mmr_t	illegal_vcpi                   : 1;
		mmr_t	illegal_vcmd                   : 1;
		mmr_t	illegal_vciilb                 : 1;
		mmr_t	underflow_fifo02_vc0_pop       : 1;
		mmr_t	underflow_fifo02_vc2_pop       : 1;
		mmr_t	underflow_fifo13_vc1_pop       : 1;
		mmr_t	underflow_fifo13_vc3_pop       : 1;
		mmr_t	underflow_fifo02_vc0_push      : 1;
		mmr_t	underflow_fifo02_vc2_push      : 1;
		mmr_t	underflow_fifo13_vc1_push      : 1;
		mmr_t	underflow_fifo13_vc3_push      : 1;
		mmr_t	underflow_fifo02_vc0_credit    : 1;
		mmr_t	underflow_fifo02_vc2_credit    : 1;
		mmr_t	underflow_fifo13_vc0_credit    : 1;
		mmr_t	underflow_fifo13_vc2_credit    : 1;
		mmr_t	underflow0_vc0_credit          : 1;
		mmr_t	underflow1_vc0_credit          : 1;
		mmr_t	underflow2_vc0_credit          : 1;
		mmr_t	underflow0_vc2_credit          : 1;
		mmr_t	underflow1_vc2_credit          : 1;
		mmr_t	underflow2_vc2_credit          : 1;
		mmr_t	reserved_0                     : 10;
		mmr_t	underflow_pi_fifo_vc0_pop      : 1;
		mmr_t	underflow_pi_fifo_vc2_pop      : 1;
		mmr_t	underflow_iilb_fifo_vc0_pop    : 1;
		mmr_t	underflow_iilb_fifo_vc2_pop    : 1;
		mmr_t	underflow_md_fifo_vc0_pop      : 1;
		mmr_t	underflow_md_fifo_vc2_pop      : 1;
		mmr_t	underflow_ni_fifo_vc0_pop      : 1;
		mmr_t	underflow_ni_fifo_vc2_pop      : 1;
		mmr_t	underflow_pi_fifo_vc0_push     : 1;
		mmr_t	underflow_pi_fifo_vc2_push     : 1;
		mmr_t	underflow_iilb_fifo_vc0_push   : 1;
		mmr_t	underflow_iilb_fifo_vc2_push   : 1;
		mmr_t	underflow_md_fifo_vc0_push     : 1;
		mmr_t	underflow_md_fifo_vc2_push     : 1;
		mmr_t	underflow_pi_fifo_vc0_credit   : 1;
		mmr_t	underflow_pi_fifo_vc2_credit   : 1;
		mmr_t	underflow_iilb_fifo_vc0_credit : 1;
		mmr_t	underflow_iilb_fifo_vc2_credit : 1;
		mmr_t	underflow_md_fifo_vc0_credit   : 1;
		mmr_t	underflow_md_fifo_vc2_credit   : 1;
		mmr_t	underflow_ni_fifo_vc0_credit   : 1;
		mmr_t	underflow_ni_fifo_vc1_credit   : 1;
		mmr_t	underflow_ni_fifo_vc2_credit   : 1;
		mmr_t	underflow_ni_fifo_vc3_credit   : 1;
		mmr_t	llp_deadlock_vc0               : 1;
		mmr_t	llp_deadlock_vc1               : 1;
		mmr_t	llp_deadlock_vc2               : 1;
		mmr_t	llp_deadlock_vc3               : 1;
		mmr_t	chiplet_nomatch                : 1;
		mmr_t	lut_read_error                 : 1;
		mmr_t	retry_timeout_error            : 1;
		mmr_t	reserved_1                     : 1;
	} sh_ni0_error_mask_2_s;
} sh_ni0_error_mask_2_u_t;
#else
typedef union sh_ni0_error_mask_2_u {
	mmr_t	sh_ni0_error_mask_2_regval;
	struct {
		mmr_t	reserved_1                     : 1;
		mmr_t	retry_timeout_error            : 1;
		mmr_t	lut_read_error                 : 1;
		mmr_t	chiplet_nomatch                : 1;
		mmr_t	llp_deadlock_vc3               : 1;
		mmr_t	llp_deadlock_vc2               : 1;
		mmr_t	llp_deadlock_vc1               : 1;
		mmr_t	llp_deadlock_vc0               : 1;
		mmr_t	underflow_ni_fifo_vc3_credit   : 1;
		mmr_t	underflow_ni_fifo_vc2_credit   : 1;
		mmr_t	underflow_ni_fifo_vc1_credit   : 1;
		mmr_t	underflow_ni_fifo_vc0_credit   : 1;
		mmr_t	underflow_md_fifo_vc2_credit   : 1;
		mmr_t	underflow_md_fifo_vc0_credit   : 1;
		mmr_t	underflow_iilb_fifo_vc2_credit : 1;
		mmr_t	underflow_iilb_fifo_vc0_credit : 1;
		mmr_t	underflow_pi_fifo_vc2_credit   : 1;
		mmr_t	underflow_pi_fifo_vc0_credit   : 1;
		mmr_t	underflow_md_fifo_vc2_push     : 1;
		mmr_t	underflow_md_fifo_vc0_push     : 1;
		mmr_t	underflow_iilb_fifo_vc2_push   : 1;
		mmr_t	underflow_iilb_fifo_vc0_push   : 1;
		mmr_t	underflow_pi_fifo_vc2_push     : 1;
		mmr_t	underflow_pi_fifo_vc0_push     : 1;
		mmr_t	underflow_ni_fifo_vc2_pop      : 1;
		mmr_t	underflow_ni_fifo_vc0_pop      : 1;
		mmr_t	underflow_md_fifo_vc2_pop      : 1;
		mmr_t	underflow_md_fifo_vc0_pop      : 1;
		mmr_t	underflow_iilb_fifo_vc2_pop    : 1;
		mmr_t	underflow_iilb_fifo_vc0_pop    : 1;
		mmr_t	underflow_pi_fifo_vc2_pop      : 1;
		mmr_t	underflow_pi_fifo_vc0_pop      : 1;
		mmr_t	reserved_0                     : 10;
		mmr_t	underflow2_vc2_credit          : 1;
		mmr_t	underflow1_vc2_credit          : 1;
		mmr_t	underflow0_vc2_credit          : 1;
		mmr_t	underflow2_vc0_credit          : 1;
		mmr_t	underflow1_vc0_credit          : 1;
		mmr_t	underflow0_vc0_credit          : 1;
		mmr_t	underflow_fifo13_vc2_credit    : 1;
		mmr_t	underflow_fifo13_vc0_credit    : 1;
		mmr_t	underflow_fifo02_vc2_credit    : 1;
		mmr_t	underflow_fifo02_vc0_credit    : 1;
		mmr_t	underflow_fifo13_vc3_push      : 1;
		mmr_t	underflow_fifo13_vc1_push      : 1;
		mmr_t	underflow_fifo02_vc2_push      : 1;
		mmr_t	underflow_fifo02_vc0_push      : 1;
		mmr_t	underflow_fifo13_vc3_pop       : 1;
		mmr_t	underflow_fifo13_vc1_pop       : 1;
		mmr_t	underflow_fifo02_vc2_pop       : 1;
		mmr_t	underflow_fifo02_vc0_pop       : 1;
		mmr_t	illegal_vciilb                 : 1;
		mmr_t	illegal_vcmd                   : 1;
		mmr_t	illegal_vcpi                   : 1;
		mmr_t	illegal_vcni                   : 1;
	} sh_ni0_error_mask_2_s;
} sh_ni0_error_mask_2_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_NI0_FIRST_ERROR_1"                    */
/*                        ni0  First Error Bits                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni0_first_error_1_u {
	mmr_t	sh_ni0_first_error_1_regval;
	struct {
		mmr_t	overflow_fifo02_debit0        : 1;
		mmr_t	overflow_fifo02_debit2        : 1;
		mmr_t	overflow_fifo13_debit0        : 1;
		mmr_t	overflow_fifo13_debit2        : 1;
		mmr_t	overflow_fifo02_vc0_pop       : 1;
		mmr_t	overflow_fifo02_vc2_pop       : 1;
		mmr_t	overflow_fifo13_vc1_pop       : 1;
		mmr_t	overflow_fifo13_vc3_pop       : 1;
		mmr_t	overflow_fifo02_vc0_push      : 1;
		mmr_t	overflow_fifo02_vc2_push      : 1;
		mmr_t	overflow_fifo13_vc1_push      : 1;
		mmr_t	overflow_fifo13_vc3_push      : 1;
		mmr_t	overflow_fifo02_vc0_credit    : 1;
		mmr_t	overflow_fifo02_vc2_credit    : 1;
		mmr_t	overflow_fifo13_vc0_credit    : 1;
		mmr_t	overflow_fifo13_vc2_credit    : 1;
		mmr_t	overflow0_vc0_credit          : 1;
		mmr_t	overflow1_vc0_credit          : 1;
		mmr_t	overflow2_vc0_credit          : 1;
		mmr_t	overflow0_vc2_credit          : 1;
		mmr_t	overflow1_vc2_credit          : 1;
		mmr_t	overflow2_vc2_credit          : 1;
		mmr_t	overflow_pi_fifo_debit0       : 1;
		mmr_t	overflow_pi_fifo_debit2       : 1;
		mmr_t	overflow_iilb_fifo_debit0     : 1;
		mmr_t	overflow_iilb_fifo_debit2     : 1;
		mmr_t	overflow_md_fifo_debit0       : 1;
		mmr_t	overflow_md_fifo_debit2       : 1;
		mmr_t	overflow_ni_fifo_debit0       : 1;
		mmr_t	overflow_ni_fifo_debit1       : 1;
		mmr_t	overflow_ni_fifo_debit2       : 1;
		mmr_t	overflow_ni_fifo_debit3       : 1;
		mmr_t	overflow_pi_fifo_vc0_pop      : 1;
		mmr_t	overflow_pi_fifo_vc2_pop      : 1;
		mmr_t	overflow_iilb_fifo_vc0_pop    : 1;
		mmr_t	overflow_iilb_fifo_vc2_pop    : 1;
		mmr_t	overflow_md_fifo_vc0_pop      : 1;
		mmr_t	overflow_md_fifo_vc2_pop      : 1;
		mmr_t	overflow_ni_fifo_vc0_pop      : 1;
		mmr_t	overflow_ni_fifo_vc2_pop      : 1;
		mmr_t	overflow_pi_fifo_vc0_push     : 1;
		mmr_t	overflow_pi_fifo_vc2_push     : 1;
		mmr_t	overflow_iilb_fifo_vc0_push   : 1;
		mmr_t	overflow_iilb_fifo_vc2_push   : 1;
		mmr_t	overflow_md_fifo_vc0_push     : 1;
		mmr_t	overflow_md_fifo_vc2_push     : 1;
		mmr_t	overflow_pi_fifo_vc0_credit   : 1;
		mmr_t	overflow_pi_fifo_vc2_credit   : 1;
		mmr_t	overflow_iilb_fifo_vc0_credit : 1;
		mmr_t	overflow_iilb_fifo_vc2_credit : 1;
		mmr_t	overflow_md_fifo_vc0_credit   : 1;
		mmr_t	overflow_md_fifo_vc2_credit   : 1;
		mmr_t	overflow_ni_fifo_vc0_credit   : 1;
		mmr_t	overflow_ni_fifo_vc1_credit   : 1;
		mmr_t	overflow_ni_fifo_vc2_credit   : 1;
		mmr_t	overflow_ni_fifo_vc3_credit   : 1;
		mmr_t	tail_timeout_fifo02_vc0       : 1;
		mmr_t	tail_timeout_fifo02_vc2       : 1;
		mmr_t	tail_timeout_fifo13_vc1       : 1;
		mmr_t	tail_timeout_fifo13_vc3       : 1;
		mmr_t	tail_timeout_ni_vc0           : 1;
		mmr_t	tail_timeout_ni_vc1           : 1;
		mmr_t	tail_timeout_ni_vc2           : 1;
		mmr_t	tail_timeout_ni_vc3           : 1;
	} sh_ni0_first_error_1_s;
} sh_ni0_first_error_1_u_t;
#else
typedef union sh_ni0_first_error_1_u {
	mmr_t	sh_ni0_first_error_1_regval;
	struct {
		mmr_t	tail_timeout_ni_vc3           : 1;
		mmr_t	tail_timeout_ni_vc2           : 1;
		mmr_t	tail_timeout_ni_vc1           : 1;
		mmr_t	tail_timeout_ni_vc0           : 1;
		mmr_t	tail_timeout_fifo13_vc3       : 1;
		mmr_t	tail_timeout_fifo13_vc1       : 1;
		mmr_t	tail_timeout_fifo02_vc2       : 1;
		mmr_t	tail_timeout_fifo02_vc0       : 1;
		mmr_t	overflow_ni_fifo_vc3_credit   : 1;
		mmr_t	overflow_ni_fifo_vc2_credit   : 1;
		mmr_t	overflow_ni_fifo_vc1_credit   : 1;
		mmr_t	overflow_ni_fifo_vc0_credit   : 1;
		mmr_t	overflow_md_fifo_vc2_credit   : 1;
		mmr_t	overflow_md_fifo_vc0_credit   : 1;
		mmr_t	overflow_iilb_fifo_vc2_credit : 1;
		mmr_t	overflow_iilb_fifo_vc0_credit : 1;
		mmr_t	overflow_pi_fifo_vc2_credit   : 1;
		mmr_t	overflow_pi_fifo_vc0_credit   : 1;
		mmr_t	overflow_md_fifo_vc2_push     : 1;
		mmr_t	overflow_md_fifo_vc0_push     : 1;
		mmr_t	overflow_iilb_fifo_vc2_push   : 1;
		mmr_t	overflow_iilb_fifo_vc0_push   : 1;
		mmr_t	overflow_pi_fifo_vc2_push     : 1;
		mmr_t	overflow_pi_fifo_vc0_push     : 1;
		mmr_t	overflow_ni_fifo_vc2_pop      : 1;
		mmr_t	overflow_ni_fifo_vc0_pop      : 1;
		mmr_t	overflow_md_fifo_vc2_pop      : 1;
		mmr_t	overflow_md_fifo_vc0_pop      : 1;
		mmr_t	overflow_iilb_fifo_vc2_pop    : 1;
		mmr_t	overflow_iilb_fifo_vc0_pop    : 1;
		mmr_t	overflow_pi_fifo_vc2_pop      : 1;
		mmr_t	overflow_pi_fifo_vc0_pop      : 1;
		mmr_t	overflow_ni_fifo_debit3       : 1;
		mmr_t	overflow_ni_fifo_debit2       : 1;
		mmr_t	overflow_ni_fifo_debit1       : 1;
		mmr_t	overflow_ni_fifo_debit0       : 1;
		mmr_t	overflow_md_fifo_debit2       : 1;
		mmr_t	overflow_md_fifo_debit0       : 1;
		mmr_t	overflow_iilb_fifo_debit2     : 1;
		mmr_t	overflow_iilb_fifo_debit0     : 1;
		mmr_t	overflow_pi_fifo_debit2       : 1;
		mmr_t	overflow_pi_fifo_debit0       : 1;
		mmr_t	overflow2_vc2_credit          : 1;
		mmr_t	overflow1_vc2_credit          : 1;
		mmr_t	overflow0_vc2_credit          : 1;
		mmr_t	overflow2_vc0_credit          : 1;
		mmr_t	overflow1_vc0_credit          : 1;
		mmr_t	overflow0_vc0_credit          : 1;
		mmr_t	overflow_fifo13_vc2_credit    : 1;
		mmr_t	overflow_fifo13_vc0_credit    : 1;
		mmr_t	overflow_fifo02_vc2_credit    : 1;
		mmr_t	overflow_fifo02_vc0_credit    : 1;
		mmr_t	overflow_fifo13_vc3_push      : 1;
		mmr_t	overflow_fifo13_vc1_push      : 1;
		mmr_t	overflow_fifo02_vc2_push      : 1;
		mmr_t	overflow_fifo02_vc0_push      : 1;
		mmr_t	overflow_fifo13_vc3_pop       : 1;
		mmr_t	overflow_fifo13_vc1_pop       : 1;
		mmr_t	overflow_fifo02_vc2_pop       : 1;
		mmr_t	overflow_fifo02_vc0_pop       : 1;
		mmr_t	overflow_fifo13_debit2        : 1;
		mmr_t	overflow_fifo13_debit0        : 1;
		mmr_t	overflow_fifo02_debit2        : 1;
		mmr_t	overflow_fifo02_debit0        : 1;
	} sh_ni0_first_error_1_s;
} sh_ni0_first_error_1_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_NI0_FIRST_ERROR_2"                    */
/*                         ni0 First Error Bits                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni0_first_error_2_u {
	mmr_t	sh_ni0_first_error_2_regval;
	struct {
		mmr_t	illegal_vcni                   : 1;
		mmr_t	illegal_vcpi                   : 1;
		mmr_t	illegal_vcmd                   : 1;
		mmr_t	illegal_vciilb                 : 1;
		mmr_t	underflow_fifo02_vc0_pop       : 1;
		mmr_t	underflow_fifo02_vc2_pop       : 1;
		mmr_t	underflow_fifo13_vc1_pop       : 1;
		mmr_t	underflow_fifo13_vc3_pop       : 1;
		mmr_t	underflow_fifo02_vc0_push      : 1;
		mmr_t	underflow_fifo02_vc2_push      : 1;
		mmr_t	underflow_fifo13_vc1_push      : 1;
		mmr_t	underflow_fifo13_vc3_push      : 1;
		mmr_t	underflow_fifo02_vc0_credit    : 1;
		mmr_t	underflow_fifo02_vc2_credit    : 1;
		mmr_t	underflow_fifo13_vc0_credit    : 1;
		mmr_t	underflow_fifo13_vc2_credit    : 1;
		mmr_t	underflow0_vc0_credit          : 1;
		mmr_t	underflow1_vc0_credit          : 1;
		mmr_t	underflow2_vc0_credit          : 1;
		mmr_t	underflow0_vc2_credit          : 1;
		mmr_t	underflow1_vc2_credit          : 1;
		mmr_t	underflow2_vc2_credit          : 1;
		mmr_t	reserved_0                     : 10;
		mmr_t	underflow_pi_fifo_vc0_pop      : 1;
		mmr_t	underflow_pi_fifo_vc2_pop      : 1;
		mmr_t	underflow_iilb_fifo_vc0_pop    : 1;
		mmr_t	underflow_iilb_fifo_vc2_pop    : 1;
		mmr_t	underflow_md_fifo_vc0_pop      : 1;
		mmr_t	underflow_md_fifo_vc2_pop      : 1;
		mmr_t	underflow_ni_fifo_vc0_pop      : 1;
		mmr_t	underflow_ni_fifo_vc2_pop      : 1;
		mmr_t	underflow_pi_fifo_vc0_push     : 1;
		mmr_t	underflow_pi_fifo_vc2_push     : 1;
		mmr_t	underflow_iilb_fifo_vc0_push   : 1;
		mmr_t	underflow_iilb_fifo_vc2_push   : 1;
		mmr_t	underflow_md_fifo_vc0_push     : 1;
		mmr_t	underflow_md_fifo_vc2_push     : 1;
		mmr_t	underflow_pi_fifo_vc0_credit   : 1;
		mmr_t	underflow_pi_fifo_vc2_credit   : 1;
		mmr_t	underflow_iilb_fifo_vc0_credit : 1;
		mmr_t	underflow_iilb_fifo_vc2_credit : 1;
		mmr_t	underflow_md_fifo_vc0_credit   : 1;
		mmr_t	underflow_md_fifo_vc2_credit   : 1;
		mmr_t	underflow_ni_fifo_vc0_credit   : 1;
		mmr_t	underflow_ni_fifo_vc1_credit   : 1;
		mmr_t	underflow_ni_fifo_vc2_credit   : 1;
		mmr_t	underflow_ni_fifo_vc3_credit   : 1;
		mmr_t	llp_deadlock_vc0               : 1;
		mmr_t	llp_deadlock_vc1               : 1;
		mmr_t	llp_deadlock_vc2               : 1;
		mmr_t	llp_deadlock_vc3               : 1;
		mmr_t	chiplet_nomatch                : 1;
		mmr_t	lut_read_error                 : 1;
		mmr_t	retry_timeout_error            : 1;
		mmr_t	reserved_1                     : 1;
	} sh_ni0_first_error_2_s;
} sh_ni0_first_error_2_u_t;
#else
typedef union sh_ni0_first_error_2_u {
	mmr_t	sh_ni0_first_error_2_regval;
	struct {
		mmr_t	reserved_1                     : 1;
		mmr_t	retry_timeout_error            : 1;
		mmr_t	lut_read_error                 : 1;
		mmr_t	chiplet_nomatch                : 1;
		mmr_t	llp_deadlock_vc3               : 1;
		mmr_t	llp_deadlock_vc2               : 1;
		mmr_t	llp_deadlock_vc1               : 1;
		mmr_t	llp_deadlock_vc0               : 1;
		mmr_t	underflow_ni_fifo_vc3_credit   : 1;
		mmr_t	underflow_ni_fifo_vc2_credit   : 1;
		mmr_t	underflow_ni_fifo_vc1_credit   : 1;
		mmr_t	underflow_ni_fifo_vc0_credit   : 1;
		mmr_t	underflow_md_fifo_vc2_credit   : 1;
		mmr_t	underflow_md_fifo_vc0_credit   : 1;
		mmr_t	underflow_iilb_fifo_vc2_credit : 1;
		mmr_t	underflow_iilb_fifo_vc0_credit : 1;
		mmr_t	underflow_pi_fifo_vc2_credit   : 1;
		mmr_t	underflow_pi_fifo_vc0_credit   : 1;
		mmr_t	underflow_md_fifo_vc2_push     : 1;
		mmr_t	underflow_md_fifo_vc0_push     : 1;
		mmr_t	underflow_iilb_fifo_vc2_push   : 1;
		mmr_t	underflow_iilb_fifo_vc0_push   : 1;
		mmr_t	underflow_pi_fifo_vc2_push     : 1;
		mmr_t	underflow_pi_fifo_vc0_push     : 1;
		mmr_t	underflow_ni_fifo_vc2_pop      : 1;
		mmr_t	underflow_ni_fifo_vc0_pop      : 1;
		mmr_t	underflow_md_fifo_vc2_pop      : 1;
		mmr_t	underflow_md_fifo_vc0_pop      : 1;
		mmr_t	underflow_iilb_fifo_vc2_pop    : 1;
		mmr_t	underflow_iilb_fifo_vc0_pop    : 1;
		mmr_t	underflow_pi_fifo_vc2_pop      : 1;
		mmr_t	underflow_pi_fifo_vc0_pop      : 1;
		mmr_t	reserved_0                     : 10;
		mmr_t	underflow2_vc2_credit          : 1;
		mmr_t	underflow1_vc2_credit          : 1;
		mmr_t	underflow0_vc2_credit          : 1;
		mmr_t	underflow2_vc0_credit          : 1;
		mmr_t	underflow1_vc0_credit          : 1;
		mmr_t	underflow0_vc0_credit          : 1;
		mmr_t	underflow_fifo13_vc2_credit    : 1;
		mmr_t	underflow_fifo13_vc0_credit    : 1;
		mmr_t	underflow_fifo02_vc2_credit    : 1;
		mmr_t	underflow_fifo02_vc0_credit    : 1;
		mmr_t	underflow_fifo13_vc3_push      : 1;
		mmr_t	underflow_fifo13_vc1_push      : 1;
		mmr_t	underflow_fifo02_vc2_push      : 1;
		mmr_t	underflow_fifo02_vc0_push      : 1;
		mmr_t	underflow_fifo13_vc3_pop       : 1;
		mmr_t	underflow_fifo13_vc1_pop       : 1;
		mmr_t	underflow_fifo02_vc2_pop       : 1;
		mmr_t	underflow_fifo02_vc0_pop       : 1;
		mmr_t	illegal_vciilb                 : 1;
		mmr_t	illegal_vcmd                   : 1;
		mmr_t	illegal_vcpi                   : 1;
		mmr_t	illegal_vcni                   : 1;
	} sh_ni0_first_error_2_s;
} sh_ni0_first_error_2_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_NI0_ERROR_DETAIL_1"                   */
/*                ni0 Chiplet no match header bits 63:0                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni0_error_detail_1_u {
	mmr_t	sh_ni0_error_detail_1_regval;
	struct {
		mmr_t	header      : 64;
	} sh_ni0_error_detail_1_s;
} sh_ni0_error_detail_1_u_t;
#else
typedef union sh_ni0_error_detail_1_u {
	mmr_t	sh_ni0_error_detail_1_regval;
	struct {
		mmr_t	header      : 64;
	} sh_ni0_error_detail_1_s;
} sh_ni0_error_detail_1_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_NI0_ERROR_DETAIL_2"                   */
/*               ni0 Chiplet no match header bits 127:64                */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni0_error_detail_2_u {
	mmr_t	sh_ni0_error_detail_2_regval;
	struct {
		mmr_t	header      : 64;
	} sh_ni0_error_detail_2_s;
} sh_ni0_error_detail_2_u_t;
#else
typedef union sh_ni0_error_detail_2_u {
	mmr_t	sh_ni0_error_detail_2_regval;
	struct {
		mmr_t	header      : 64;
	} sh_ni0_error_detail_2_s;
} sh_ni0_error_detail_2_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_NI1_ERROR_SUMMARY_1"                   */
/*                       ni1  Error Summary Bits                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni1_error_summary_1_u {
	mmr_t	sh_ni1_error_summary_1_regval;
	struct {
		mmr_t	overflow_fifo02_debit0        : 1;
		mmr_t	overflow_fifo02_debit2        : 1;
		mmr_t	overflow_fifo13_debit0        : 1;
		mmr_t	overflow_fifo13_debit2        : 1;
		mmr_t	overflow_fifo02_vc0_pop       : 1;
		mmr_t	overflow_fifo02_vc2_pop       : 1;
		mmr_t	overflow_fifo13_vc1_pop       : 1;
		mmr_t	overflow_fifo13_vc3_pop       : 1;
		mmr_t	overflow_fifo02_vc0_push      : 1;
		mmr_t	overflow_fifo02_vc2_push      : 1;
		mmr_t	overflow_fifo13_vc1_push      : 1;
		mmr_t	overflow_fifo13_vc3_push      : 1;
		mmr_t	overflow_fifo02_vc0_credit    : 1;
		mmr_t	overflow_fifo02_vc2_credit    : 1;
		mmr_t	overflow_fifo13_vc0_credit    : 1;
		mmr_t	overflow_fifo13_vc2_credit    : 1;
		mmr_t	overflow0_vc0_credit          : 1;
		mmr_t	overflow1_vc0_credit          : 1;
		mmr_t	overflow2_vc0_credit          : 1;
		mmr_t	overflow0_vc2_credit          : 1;
		mmr_t	overflow1_vc2_credit          : 1;
		mmr_t	overflow2_vc2_credit          : 1;
		mmr_t	overflow_pi_fifo_debit0       : 1;
		mmr_t	overflow_pi_fifo_debit2       : 1;
		mmr_t	overflow_iilb_fifo_debit0     : 1;
		mmr_t	overflow_iilb_fifo_debit2     : 1;
		mmr_t	overflow_md_fifo_debit0       : 1;
		mmr_t	overflow_md_fifo_debit2       : 1;
		mmr_t	overflow_ni_fifo_debit0       : 1;
		mmr_t	overflow_ni_fifo_debit1       : 1;
		mmr_t	overflow_ni_fifo_debit2       : 1;
		mmr_t	overflow_ni_fifo_debit3       : 1;
		mmr_t	overflow_pi_fifo_vc0_pop      : 1;
		mmr_t	overflow_pi_fifo_vc2_pop      : 1;
		mmr_t	overflow_iilb_fifo_vc0_pop    : 1;
		mmr_t	overflow_iilb_fifo_vc2_pop    : 1;
		mmr_t	overflow_md_fifo_vc0_pop      : 1;
		mmr_t	overflow_md_fifo_vc2_pop      : 1;
		mmr_t	overflow_ni_fifo_vc0_pop      : 1;
		mmr_t	overflow_ni_fifo_vc2_pop      : 1;
		mmr_t	overflow_pi_fifo_vc0_push     : 1;
		mmr_t	overflow_pi_fifo_vc2_push     : 1;
		mmr_t	overflow_iilb_fifo_vc0_push   : 1;
		mmr_t	overflow_iilb_fifo_vc2_push   : 1;
		mmr_t	overflow_md_fifo_vc0_push     : 1;
		mmr_t	overflow_md_fifo_vc2_push     : 1;
		mmr_t	overflow_pi_fifo_vc0_credit   : 1;
		mmr_t	overflow_pi_fifo_vc2_credit   : 1;
		mmr_t	overflow_iilb_fifo_vc0_credit : 1;
		mmr_t	overflow_iilb_fifo_vc2_credit : 1;
		mmr_t	overflow_md_fifo_vc0_credit   : 1;
		mmr_t	overflow_md_fifo_vc2_credit   : 1;
		mmr_t	overflow_ni_fifo_vc0_credit   : 1;
		mmr_t	overflow_ni_fifo_vc1_credit   : 1;
		mmr_t	overflow_ni_fifo_vc2_credit   : 1;
		mmr_t	overflow_ni_fifo_vc3_credit   : 1;
		mmr_t	tail_timeout_fifo02_vc0       : 1;
		mmr_t	tail_timeout_fifo02_vc2       : 1;
		mmr_t	tail_timeout_fifo13_vc1       : 1;
		mmr_t	tail_timeout_fifo13_vc3       : 1;
		mmr_t	tail_timeout_ni_vc0           : 1;
		mmr_t	tail_timeout_ni_vc1           : 1;
		mmr_t	tail_timeout_ni_vc2           : 1;
		mmr_t	tail_timeout_ni_vc3           : 1;
	} sh_ni1_error_summary_1_s;
} sh_ni1_error_summary_1_u_t;
#else
typedef union sh_ni1_error_summary_1_u {
	mmr_t	sh_ni1_error_summary_1_regval;
	struct {
		mmr_t	tail_timeout_ni_vc3           : 1;
		mmr_t	tail_timeout_ni_vc2           : 1;
		mmr_t	tail_timeout_ni_vc1           : 1;
		mmr_t	tail_timeout_ni_vc0           : 1;
		mmr_t	tail_timeout_fifo13_vc3       : 1;
		mmr_t	tail_timeout_fifo13_vc1       : 1;
		mmr_t	tail_timeout_fifo02_vc2       : 1;
		mmr_t	tail_timeout_fifo02_vc0       : 1;
		mmr_t	overflow_ni_fifo_vc3_credit   : 1;
		mmr_t	overflow_ni_fifo_vc2_credit   : 1;
		mmr_t	overflow_ni_fifo_vc1_credit   : 1;
		mmr_t	overflow_ni_fifo_vc0_credit   : 1;
		mmr_t	overflow_md_fifo_vc2_credit   : 1;
		mmr_t	overflow_md_fifo_vc0_credit   : 1;
		mmr_t	overflow_iilb_fifo_vc2_credit : 1;
		mmr_t	overflow_iilb_fifo_vc0_credit : 1;
		mmr_t	overflow_pi_fifo_vc2_credit   : 1;
		mmr_t	overflow_pi_fifo_vc0_credit   : 1;
		mmr_t	overflow_md_fifo_vc2_push     : 1;
		mmr_t	overflow_md_fifo_vc0_push     : 1;
		mmr_t	overflow_iilb_fifo_vc2_push   : 1;
		mmr_t	overflow_iilb_fifo_vc0_push   : 1;
		mmr_t	overflow_pi_fifo_vc2_push     : 1;
		mmr_t	overflow_pi_fifo_vc0_push     : 1;
		mmr_t	overflow_ni_fifo_vc2_pop      : 1;
		mmr_t	overflow_ni_fifo_vc0_pop      : 1;
		mmr_t	overflow_md_fifo_vc2_pop      : 1;
		mmr_t	overflow_md_fifo_vc0_pop      : 1;
		mmr_t	overflow_iilb_fifo_vc2_pop    : 1;
		mmr_t	overflow_iilb_fifo_vc0_pop    : 1;
		mmr_t	overflow_pi_fifo_vc2_pop      : 1;
		mmr_t	overflow_pi_fifo_vc0_pop      : 1;
		mmr_t	overflow_ni_fifo_debit3       : 1;
		mmr_t	overflow_ni_fifo_debit2       : 1;
		mmr_t	overflow_ni_fifo_debit1       : 1;
		mmr_t	overflow_ni_fifo_debit0       : 1;
		mmr_t	overflow_md_fifo_debit2       : 1;
		mmr_t	overflow_md_fifo_debit0       : 1;
		mmr_t	overflow_iilb_fifo_debit2     : 1;
		mmr_t	overflow_iilb_fifo_debit0     : 1;
		mmr_t	overflow_pi_fifo_debit2       : 1;
		mmr_t	overflow_pi_fifo_debit0       : 1;
		mmr_t	overflow2_vc2_credit          : 1;
		mmr_t	overflow1_vc2_credit          : 1;
		mmr_t	overflow0_vc2_credit          : 1;
		mmr_t	overflow2_vc0_credit          : 1;
		mmr_t	overflow1_vc0_credit          : 1;
		mmr_t	overflow0_vc0_credit          : 1;
		mmr_t	overflow_fifo13_vc2_credit    : 1;
		mmr_t	overflow_fifo13_vc0_credit    : 1;
		mmr_t	overflow_fifo02_vc2_credit    : 1;
		mmr_t	overflow_fifo02_vc0_credit    : 1;
		mmr_t	overflow_fifo13_vc3_push      : 1;
		mmr_t	overflow_fifo13_vc1_push      : 1;
		mmr_t	overflow_fifo02_vc2_push      : 1;
		mmr_t	overflow_fifo02_vc0_push      : 1;
		mmr_t	overflow_fifo13_vc3_pop       : 1;
		mmr_t	overflow_fifo13_vc1_pop       : 1;
		mmr_t	overflow_fifo02_vc2_pop       : 1;
		mmr_t	overflow_fifo02_vc0_pop       : 1;
		mmr_t	overflow_fifo13_debit2        : 1;
		mmr_t	overflow_fifo13_debit0        : 1;
		mmr_t	overflow_fifo02_debit2        : 1;
		mmr_t	overflow_fifo02_debit0        : 1;
	} sh_ni1_error_summary_1_s;
} sh_ni1_error_summary_1_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_NI1_ERROR_SUMMARY_2"                   */
/*                       ni1  Error Summary Bits                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni1_error_summary_2_u {
	mmr_t	sh_ni1_error_summary_2_regval;
	struct {
		mmr_t	illegal_vcni                   : 1;
		mmr_t	illegal_vcpi                   : 1;
		mmr_t	illegal_vcmd                   : 1;
		mmr_t	illegal_vciilb                 : 1;
		mmr_t	underflow_fifo02_vc0_pop       : 1;
		mmr_t	underflow_fifo02_vc2_pop       : 1;
		mmr_t	underflow_fifo13_vc1_pop       : 1;
		mmr_t	underflow_fifo13_vc3_pop       : 1;
		mmr_t	underflow_fifo02_vc0_push      : 1;
		mmr_t	underflow_fifo02_vc2_push      : 1;
		mmr_t	underflow_fifo13_vc1_push      : 1;
		mmr_t	underflow_fifo13_vc3_push      : 1;
		mmr_t	underflow_fifo02_vc0_credit    : 1;
		mmr_t	underflow_fifo02_vc2_credit    : 1;
		mmr_t	underflow_fifo13_vc0_credit    : 1;
		mmr_t	underflow_fifo13_vc2_credit    : 1;
		mmr_t	underflow0_vc0_credit          : 1;
		mmr_t	underflow1_vc0_credit          : 1;
		mmr_t	underflow2_vc0_credit          : 1;
		mmr_t	underflow0_vc2_credit          : 1;
		mmr_t	underflow1_vc2_credit          : 1;
		mmr_t	underflow2_vc2_credit          : 1;
		mmr_t	reserved_0                     : 10;
		mmr_t	underflow_pi_fifo_vc0_pop      : 1;
		mmr_t	underflow_pi_fifo_vc2_pop      : 1;
		mmr_t	underflow_iilb_fifo_vc0_pop    : 1;
		mmr_t	underflow_iilb_fifo_vc2_pop    : 1;
		mmr_t	underflow_md_fifo_vc0_pop      : 1;
		mmr_t	underflow_md_fifo_vc2_pop      : 1;
		mmr_t	underflow_ni_fifo_vc0_pop      : 1;
		mmr_t	underflow_ni_fifo_vc2_pop      : 1;
		mmr_t	underflow_pi_fifo_vc0_push     : 1;
		mmr_t	underflow_pi_fifo_vc2_push     : 1;
		mmr_t	underflow_iilb_fifo_vc0_push   : 1;
		mmr_t	underflow_iilb_fifo_vc2_push   : 1;
		mmr_t	underflow_md_fifo_vc0_push     : 1;
		mmr_t	underflow_md_fifo_vc2_push     : 1;
		mmr_t	underflow_pi_fifo_vc0_credit   : 1;
		mmr_t	underflow_pi_fifo_vc2_credit   : 1;
		mmr_t	underflow_iilb_fifo_vc0_credit : 1;
		mmr_t	underflow_iilb_fifo_vc2_credit : 1;
		mmr_t	underflow_md_fifo_vc0_credit   : 1;
		mmr_t	underflow_md_fifo_vc2_credit   : 1;
		mmr_t	underflow_ni_fifo_vc0_credit   : 1;
		mmr_t	underflow_ni_fifo_vc1_credit   : 1;
		mmr_t	underflow_ni_fifo_vc2_credit   : 1;
		mmr_t	underflow_ni_fifo_vc3_credit   : 1;
		mmr_t	llp_deadlock_vc0               : 1;
		mmr_t	llp_deadlock_vc1               : 1;
		mmr_t	llp_deadlock_vc2               : 1;
		mmr_t	llp_deadlock_vc3               : 1;
		mmr_t	chiplet_nomatch                : 1;
		mmr_t	lut_read_error                 : 1;
		mmr_t	retry_timeout_error            : 1;
		mmr_t	reserved_1                     : 1;
	} sh_ni1_error_summary_2_s;
} sh_ni1_error_summary_2_u_t;
#else
typedef union sh_ni1_error_summary_2_u {
	mmr_t	sh_ni1_error_summary_2_regval;
	struct {
		mmr_t	reserved_1                     : 1;
		mmr_t	retry_timeout_error            : 1;
		mmr_t	lut_read_error                 : 1;
		mmr_t	chiplet_nomatch                : 1;
		mmr_t	llp_deadlock_vc3               : 1;
		mmr_t	llp_deadlock_vc2               : 1;
		mmr_t	llp_deadlock_vc1               : 1;
		mmr_t	llp_deadlock_vc0               : 1;
		mmr_t	underflow_ni_fifo_vc3_credit   : 1;
		mmr_t	underflow_ni_fifo_vc2_credit   : 1;
		mmr_t	underflow_ni_fifo_vc1_credit   : 1;
		mmr_t	underflow_ni_fifo_vc0_credit   : 1;
		mmr_t	underflow_md_fifo_vc2_credit   : 1;
		mmr_t	underflow_md_fifo_vc0_credit   : 1;
		mmr_t	underflow_iilb_fifo_vc2_credit : 1;
		mmr_t	underflow_iilb_fifo_vc0_credit : 1;
		mmr_t	underflow_pi_fifo_vc2_credit   : 1;
		mmr_t	underflow_pi_fifo_vc0_credit   : 1;
		mmr_t	underflow_md_fifo_vc2_push     : 1;
		mmr_t	underflow_md_fifo_vc0_push     : 1;
		mmr_t	underflow_iilb_fifo_vc2_push   : 1;
		mmr_t	underflow_iilb_fifo_vc0_push   : 1;
		mmr_t	underflow_pi_fifo_vc2_push     : 1;
		mmr_t	underflow_pi_fifo_vc0_push     : 1;
		mmr_t	underflow_ni_fifo_vc2_pop      : 1;
		mmr_t	underflow_ni_fifo_vc0_pop      : 1;
		mmr_t	underflow_md_fifo_vc2_pop      : 1;
		mmr_t	underflow_md_fifo_vc0_pop      : 1;
		mmr_t	underflow_iilb_fifo_vc2_pop    : 1;
		mmr_t	underflow_iilb_fifo_vc0_pop    : 1;
		mmr_t	underflow_pi_fifo_vc2_pop      : 1;
		mmr_t	underflow_pi_fifo_vc0_pop      : 1;
		mmr_t	reserved_0                     : 10;
		mmr_t	underflow2_vc2_credit          : 1;
		mmr_t	underflow1_vc2_credit          : 1;
		mmr_t	underflow0_vc2_credit          : 1;
		mmr_t	underflow2_vc0_credit          : 1;
		mmr_t	underflow1_vc0_credit          : 1;
		mmr_t	underflow0_vc0_credit          : 1;
		mmr_t	underflow_fifo13_vc2_credit    : 1;
		mmr_t	underflow_fifo13_vc0_credit    : 1;
		mmr_t	underflow_fifo02_vc2_credit    : 1;
		mmr_t	underflow_fifo02_vc0_credit    : 1;
		mmr_t	underflow_fifo13_vc3_push      : 1;
		mmr_t	underflow_fifo13_vc1_push      : 1;
		mmr_t	underflow_fifo02_vc2_push      : 1;
		mmr_t	underflow_fifo02_vc0_push      : 1;
		mmr_t	underflow_fifo13_vc3_pop       : 1;
		mmr_t	underflow_fifo13_vc1_pop       : 1;
		mmr_t	underflow_fifo02_vc2_pop       : 1;
		mmr_t	underflow_fifo02_vc0_pop       : 1;
		mmr_t	illegal_vciilb                 : 1;
		mmr_t	illegal_vcmd                   : 1;
		mmr_t	illegal_vcpi                   : 1;
		mmr_t	illegal_vcni                   : 1;
	} sh_ni1_error_summary_2_s;
} sh_ni1_error_summary_2_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_NI1_ERROR_OVERFLOW_1"                  */
/*                       ni1  Error Overflow Bits                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni1_error_overflow_1_u {
	mmr_t	sh_ni1_error_overflow_1_regval;
	struct {
		mmr_t	overflow_fifo02_debit0        : 1;
		mmr_t	overflow_fifo02_debit2        : 1;
		mmr_t	overflow_fifo13_debit0        : 1;
		mmr_t	overflow_fifo13_debit2        : 1;
		mmr_t	overflow_fifo02_vc0_pop       : 1;
		mmr_t	overflow_fifo02_vc2_pop       : 1;
		mmr_t	overflow_fifo13_vc1_pop       : 1;
		mmr_t	overflow_fifo13_vc3_pop       : 1;
		mmr_t	overflow_fifo02_vc0_push      : 1;
		mmr_t	overflow_fifo02_vc2_push      : 1;
		mmr_t	overflow_fifo13_vc1_push      : 1;
		mmr_t	overflow_fifo13_vc3_push      : 1;
		mmr_t	overflow_fifo02_vc0_credit    : 1;
		mmr_t	overflow_fifo02_vc2_credit    : 1;
		mmr_t	overflow_fifo13_vc0_credit    : 1;
		mmr_t	overflow_fifo13_vc2_credit    : 1;
		mmr_t	overflow0_vc0_credit          : 1;
		mmr_t	overflow1_vc0_credit          : 1;
		mmr_t	overflow2_vc0_credit          : 1;
		mmr_t	overflow0_vc2_credit          : 1;
		mmr_t	overflow1_vc2_credit          : 1;
		mmr_t	overflow2_vc2_credit          : 1;
		mmr_t	overflow_pi_fifo_debit0       : 1;
		mmr_t	overflow_pi_fifo_debit2       : 1;
		mmr_t	overflow_iilb_fifo_debit0     : 1;
		mmr_t	overflow_iilb_fifo_debit2     : 1;
		mmr_t	overflow_md_fifo_debit0       : 1;
		mmr_t	overflow_md_fifo_debit2       : 1;
		mmr_t	overflow_ni_fifo_debit0       : 1;
		mmr_t	overflow_ni_fifo_debit1       : 1;
		mmr_t	overflow_ni_fifo_debit2       : 1;
		mmr_t	overflow_ni_fifo_debit3       : 1;
		mmr_t	overflow_pi_fifo_vc0_pop      : 1;
		mmr_t	overflow_pi_fifo_vc2_pop      : 1;
		mmr_t	overflow_iilb_fifo_vc0_pop    : 1;
		mmr_t	overflow_iilb_fifo_vc2_pop    : 1;
		mmr_t	overflow_md_fifo_vc0_pop      : 1;
		mmr_t	overflow_md_fifo_vc2_pop      : 1;
		mmr_t	overflow_ni_fifo_vc0_pop      : 1;
		mmr_t	overflow_ni_fifo_vc2_pop      : 1;
		mmr_t	overflow_pi_fifo_vc0_push     : 1;
		mmr_t	overflow_pi_fifo_vc2_push     : 1;
		mmr_t	overflow_iilb_fifo_vc0_push   : 1;
		mmr_t	overflow_iilb_fifo_vc2_push   : 1;
		mmr_t	overflow_md_fifo_vc0_push     : 1;
		mmr_t	overflow_md_fifo_vc2_push     : 1;
		mmr_t	overflow_pi_fifo_vc0_credit   : 1;
		mmr_t	overflow_pi_fifo_vc2_credit   : 1;
		mmr_t	overflow_iilb_fifo_vc0_credit : 1;
		mmr_t	overflow_iilb_fifo_vc2_credit : 1;
		mmr_t	overflow_md_fifo_vc0_credit   : 1;
		mmr_t	overflow_md_fifo_vc2_credit   : 1;
		mmr_t	overflow_ni_fifo_vc0_credit   : 1;
		mmr_t	overflow_ni_fifo_vc1_credit   : 1;
		mmr_t	overflow_ni_fifo_vc2_credit   : 1;
		mmr_t	overflow_ni_fifo_vc3_credit   : 1;
		mmr_t	tail_timeout_fifo02_vc0       : 1;
		mmr_t	tail_timeout_fifo02_vc2       : 1;
		mmr_t	tail_timeout_fifo13_vc1       : 1;
		mmr_t	tail_timeout_fifo13_vc3       : 1;
		mmr_t	tail_timeout_ni_vc0           : 1;
		mmr_t	tail_timeout_ni_vc1           : 1;
		mmr_t	tail_timeout_ni_vc2           : 1;
		mmr_t	tail_timeout_ni_vc3           : 1;
	} sh_ni1_error_overflow_1_s;
} sh_ni1_error_overflow_1_u_t;
#else
typedef union sh_ni1_error_overflow_1_u {
	mmr_t	sh_ni1_error_overflow_1_regval;
	struct {
		mmr_t	tail_timeout_ni_vc3           : 1;
		mmr_t	tail_timeout_ni_vc2           : 1;
		mmr_t	tail_timeout_ni_vc1           : 1;
		mmr_t	tail_timeout_ni_vc0           : 1;
		mmr_t	tail_timeout_fifo13_vc3       : 1;
		mmr_t	tail_timeout_fifo13_vc1       : 1;
		mmr_t	tail_timeout_fifo02_vc2       : 1;
		mmr_t	tail_timeout_fifo02_vc0       : 1;
		mmr_t	overflow_ni_fifo_vc3_credit   : 1;
		mmr_t	overflow_ni_fifo_vc2_credit   : 1;
		mmr_t	overflow_ni_fifo_vc1_credit   : 1;
		mmr_t	overflow_ni_fifo_vc0_credit   : 1;
		mmr_t	overflow_md_fifo_vc2_credit   : 1;
		mmr_t	overflow_md_fifo_vc0_credit   : 1;
		mmr_t	overflow_iilb_fifo_vc2_credit : 1;
		mmr_t	overflow_iilb_fifo_vc0_credit : 1;
		mmr_t	overflow_pi_fifo_vc2_credit   : 1;
		mmr_t	overflow_pi_fifo_vc0_credit   : 1;
		mmr_t	overflow_md_fifo_vc2_push     : 1;
		mmr_t	overflow_md_fifo_vc0_push     : 1;
		mmr_t	overflow_iilb_fifo_vc2_push   : 1;
		mmr_t	overflow_iilb_fifo_vc0_push   : 1;
		mmr_t	overflow_pi_fifo_vc2_push     : 1;
		mmr_t	overflow_pi_fifo_vc0_push     : 1;
		mmr_t	overflow_ni_fifo_vc2_pop      : 1;
		mmr_t	overflow_ni_fifo_vc0_pop      : 1;
		mmr_t	overflow_md_fifo_vc2_pop      : 1;
		mmr_t	overflow_md_fifo_vc0_pop      : 1;
		mmr_t	overflow_iilb_fifo_vc2_pop    : 1;
		mmr_t	overflow_iilb_fifo_vc0_pop    : 1;
		mmr_t	overflow_pi_fifo_vc2_pop      : 1;
		mmr_t	overflow_pi_fifo_vc0_pop      : 1;
		mmr_t	overflow_ni_fifo_debit3       : 1;
		mmr_t	overflow_ni_fifo_debit2       : 1;
		mmr_t	overflow_ni_fifo_debit1       : 1;
		mmr_t	overflow_ni_fifo_debit0       : 1;
		mmr_t	overflow_md_fifo_debit2       : 1;
		mmr_t	overflow_md_fifo_debit0       : 1;
		mmr_t	overflow_iilb_fifo_debit2     : 1;
		mmr_t	overflow_iilb_fifo_debit0     : 1;
		mmr_t	overflow_pi_fifo_debit2       : 1;
		mmr_t	overflow_pi_fifo_debit0       : 1;
		mmr_t	overflow2_vc2_credit          : 1;
		mmr_t	overflow1_vc2_credit          : 1;
		mmr_t	overflow0_vc2_credit          : 1;
		mmr_t	overflow2_vc0_credit          : 1;
		mmr_t	overflow1_vc0_credit          : 1;
		mmr_t	overflow0_vc0_credit          : 1;
		mmr_t	overflow_fifo13_vc2_credit    : 1;
		mmr_t	overflow_fifo13_vc0_credit    : 1;
		mmr_t	overflow_fifo02_vc2_credit    : 1;
		mmr_t	overflow_fifo02_vc0_credit    : 1;
		mmr_t	overflow_fifo13_vc3_push      : 1;
		mmr_t	overflow_fifo13_vc1_push      : 1;
		mmr_t	overflow_fifo02_vc2_push      : 1;
		mmr_t	overflow_fifo02_vc0_push      : 1;
		mmr_t	overflow_fifo13_vc3_pop       : 1;
		mmr_t	overflow_fifo13_vc1_pop       : 1;
		mmr_t	overflow_fifo02_vc2_pop       : 1;
		mmr_t	overflow_fifo02_vc0_pop       : 1;
		mmr_t	overflow_fifo13_debit2        : 1;
		mmr_t	overflow_fifo13_debit0        : 1;
		mmr_t	overflow_fifo02_debit2        : 1;
		mmr_t	overflow_fifo02_debit0        : 1;
	} sh_ni1_error_overflow_1_s;
} sh_ni1_error_overflow_1_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_NI1_ERROR_OVERFLOW_2"                  */
/*                       ni1  Error Overflow Bits                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni1_error_overflow_2_u {
	mmr_t	sh_ni1_error_overflow_2_regval;
	struct {
		mmr_t	illegal_vcni                   : 1;
		mmr_t	illegal_vcpi                   : 1;
		mmr_t	illegal_vcmd                   : 1;
		mmr_t	illegal_vciilb                 : 1;
		mmr_t	underflow_fifo02_vc0_pop       : 1;
		mmr_t	underflow_fifo02_vc2_pop       : 1;
		mmr_t	underflow_fifo13_vc1_pop       : 1;
		mmr_t	underflow_fifo13_vc3_pop       : 1;
		mmr_t	underflow_fifo02_vc0_push      : 1;
		mmr_t	underflow_fifo02_vc2_push      : 1;
		mmr_t	underflow_fifo13_vc1_push      : 1;
		mmr_t	underflow_fifo13_vc3_push      : 1;
		mmr_t	underflow_fifo02_vc0_credit    : 1;
		mmr_t	underflow_fifo02_vc2_credit    : 1;
		mmr_t	underflow_fifo13_vc0_credit    : 1;
		mmr_t	underflow_fifo13_vc2_credit    : 1;
		mmr_t	underflow0_vc0_credit          : 1;
		mmr_t	underflow1_vc0_credit          : 1;
		mmr_t	underflow2_vc0_credit          : 1;
		mmr_t	underflow0_vc2_credit          : 1;
		mmr_t	underflow1_vc2_credit          : 1;
		mmr_t	underflow2_vc2_credit          : 1;
		mmr_t	reserved_0                     : 10;
		mmr_t	underflow_pi_fifo_vc0_pop      : 1;
		mmr_t	underflow_pi_fifo_vc2_pop      : 1;
		mmr_t	underflow_iilb_fifo_vc0_pop    : 1;
		mmr_t	underflow_iilb_fifo_vc2_pop    : 1;
		mmr_t	underflow_md_fifo_vc0_pop      : 1;
		mmr_t	underflow_md_fifo_vc2_pop      : 1;
		mmr_t	underflow_ni_fifo_vc0_pop      : 1;
		mmr_t	underflow_ni_fifo_vc2_pop      : 1;
		mmr_t	underflow_pi_fifo_vc0_push     : 1;
		mmr_t	underflow_pi_fifo_vc2_push     : 1;
		mmr_t	underflow_iilb_fifo_vc0_push   : 1;
		mmr_t	underflow_iilb_fifo_vc2_push   : 1;
		mmr_t	underflow_md_fifo_vc0_push     : 1;
		mmr_t	underflow_md_fifo_vc2_push     : 1;
		mmr_t	underflow_pi_fifo_vc0_credit   : 1;
		mmr_t	underflow_pi_fifo_vc2_credit   : 1;
		mmr_t	underflow_iilb_fifo_vc0_credit : 1;
		mmr_t	underflow_iilb_fifo_vc2_credit : 1;
		mmr_t	underflow_md_fifo_vc0_credit   : 1;
		mmr_t	underflow_md_fifo_vc2_credit   : 1;
		mmr_t	underflow_ni_fifo_vc0_credit   : 1;
		mmr_t	underflow_ni_fifo_vc1_credit   : 1;
		mmr_t	underflow_ni_fifo_vc2_credit   : 1;
		mmr_t	underflow_ni_fifo_vc3_credit   : 1;
		mmr_t	llp_deadlock_vc0               : 1;
		mmr_t	llp_deadlock_vc1               : 1;
		mmr_t	llp_deadlock_vc2               : 1;
		mmr_t	llp_deadlock_vc3               : 1;
		mmr_t	chiplet_nomatch                : 1;
		mmr_t	lut_read_error                 : 1;
		mmr_t	retry_timeout_error            : 1;
		mmr_t	reserved_1                     : 1;
	} sh_ni1_error_overflow_2_s;
} sh_ni1_error_overflow_2_u_t;
#else
typedef union sh_ni1_error_overflow_2_u {
	mmr_t	sh_ni1_error_overflow_2_regval;
	struct {
		mmr_t	reserved_1                     : 1;
		mmr_t	retry_timeout_error            : 1;
		mmr_t	lut_read_error                 : 1;
		mmr_t	chiplet_nomatch                : 1;
		mmr_t	llp_deadlock_vc3               : 1;
		mmr_t	llp_deadlock_vc2               : 1;
		mmr_t	llp_deadlock_vc1               : 1;
		mmr_t	llp_deadlock_vc0               : 1;
		mmr_t	underflow_ni_fifo_vc3_credit   : 1;
		mmr_t	underflow_ni_fifo_vc2_credit   : 1;
		mmr_t	underflow_ni_fifo_vc1_credit   : 1;
		mmr_t	underflow_ni_fifo_vc0_credit   : 1;
		mmr_t	underflow_md_fifo_vc2_credit   : 1;
		mmr_t	underflow_md_fifo_vc0_credit   : 1;
		mmr_t	underflow_iilb_fifo_vc2_credit : 1;
		mmr_t	underflow_iilb_fifo_vc0_credit : 1;
		mmr_t	underflow_pi_fifo_vc2_credit   : 1;
		mmr_t	underflow_pi_fifo_vc0_credit   : 1;
		mmr_t	underflow_md_fifo_vc2_push     : 1;
		mmr_t	underflow_md_fifo_vc0_push     : 1;
		mmr_t	underflow_iilb_fifo_vc2_push   : 1;
		mmr_t	underflow_iilb_fifo_vc0_push   : 1;
		mmr_t	underflow_pi_fifo_vc2_push     : 1;
		mmr_t	underflow_pi_fifo_vc0_push     : 1;
		mmr_t	underflow_ni_fifo_vc2_pop      : 1;
		mmr_t	underflow_ni_fifo_vc0_pop      : 1;
		mmr_t	underflow_md_fifo_vc2_pop      : 1;
		mmr_t	underflow_md_fifo_vc0_pop      : 1;
		mmr_t	underflow_iilb_fifo_vc2_pop    : 1;
		mmr_t	underflow_iilb_fifo_vc0_pop    : 1;
		mmr_t	underflow_pi_fifo_vc2_pop      : 1;
		mmr_t	underflow_pi_fifo_vc0_pop      : 1;
		mmr_t	reserved_0                     : 10;
		mmr_t	underflow2_vc2_credit          : 1;
		mmr_t	underflow1_vc2_credit          : 1;
		mmr_t	underflow0_vc2_credit          : 1;
		mmr_t	underflow2_vc0_credit          : 1;
		mmr_t	underflow1_vc0_credit          : 1;
		mmr_t	underflow0_vc0_credit          : 1;
		mmr_t	underflow_fifo13_vc2_credit    : 1;
		mmr_t	underflow_fifo13_vc0_credit    : 1;
		mmr_t	underflow_fifo02_vc2_credit    : 1;
		mmr_t	underflow_fifo02_vc0_credit    : 1;
		mmr_t	underflow_fifo13_vc3_push      : 1;
		mmr_t	underflow_fifo13_vc1_push      : 1;
		mmr_t	underflow_fifo02_vc2_push      : 1;
		mmr_t	underflow_fifo02_vc0_push      : 1;
		mmr_t	underflow_fifo13_vc3_pop       : 1;
		mmr_t	underflow_fifo13_vc1_pop       : 1;
		mmr_t	underflow_fifo02_vc2_pop       : 1;
		mmr_t	underflow_fifo02_vc0_pop       : 1;
		mmr_t	illegal_vciilb                 : 1;
		mmr_t	illegal_vcmd                   : 1;
		mmr_t	illegal_vcpi                   : 1;
		mmr_t	illegal_vcni                   : 1;
	} sh_ni1_error_overflow_2_s;
} sh_ni1_error_overflow_2_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_NI1_ERROR_MASK_1"                    */
/*                         ni1  Error Mask Bits                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni1_error_mask_1_u {
	mmr_t	sh_ni1_error_mask_1_regval;
	struct {
		mmr_t	overflow_fifo02_debit0        : 1;
		mmr_t	overflow_fifo02_debit2        : 1;
		mmr_t	overflow_fifo13_debit0        : 1;
		mmr_t	overflow_fifo13_debit2        : 1;
		mmr_t	overflow_fifo02_vc0_pop       : 1;
		mmr_t	overflow_fifo02_vc2_pop       : 1;
		mmr_t	overflow_fifo13_vc1_pop       : 1;
		mmr_t	overflow_fifo13_vc3_pop       : 1;
		mmr_t	overflow_fifo02_vc0_push      : 1;
		mmr_t	overflow_fifo02_vc2_push      : 1;
		mmr_t	overflow_fifo13_vc1_push      : 1;
		mmr_t	overflow_fifo13_vc3_push      : 1;
		mmr_t	overflow_fifo02_vc0_credit    : 1;
		mmr_t	overflow_fifo02_vc2_credit    : 1;
		mmr_t	overflow_fifo13_vc0_credit    : 1;
		mmr_t	overflow_fifo13_vc2_credit    : 1;
		mmr_t	overflow0_vc0_credit          : 1;
		mmr_t	overflow1_vc0_credit          : 1;
		mmr_t	overflow2_vc0_credit          : 1;
		mmr_t	overflow0_vc2_credit          : 1;
		mmr_t	overflow1_vc2_credit          : 1;
		mmr_t	overflow2_vc2_credit          : 1;
		mmr_t	overflow_pi_fifo_debit0       : 1;
		mmr_t	overflow_pi_fifo_debit2       : 1;
		mmr_t	overflow_iilb_fifo_debit0     : 1;
		mmr_t	overflow_iilb_fifo_debit2     : 1;
		mmr_t	overflow_md_fifo_debit0       : 1;
		mmr_t	overflow_md_fifo_debit2       : 1;
		mmr_t	overflow_ni_fifo_debit0       : 1;
		mmr_t	overflow_ni_fifo_debit1       : 1;
		mmr_t	overflow_ni_fifo_debit2       : 1;
		mmr_t	overflow_ni_fifo_debit3       : 1;
		mmr_t	overflow_pi_fifo_vc0_pop      : 1;
		mmr_t	overflow_pi_fifo_vc2_pop      : 1;
		mmr_t	overflow_iilb_fifo_vc0_pop    : 1;
		mmr_t	overflow_iilb_fifo_vc2_pop    : 1;
		mmr_t	overflow_md_fifo_vc0_pop      : 1;
		mmr_t	overflow_md_fifo_vc2_pop      : 1;
		mmr_t	overflow_ni_fifo_vc0_pop      : 1;
		mmr_t	overflow_ni_fifo_vc2_pop      : 1;
		mmr_t	overflow_pi_fifo_vc0_push     : 1;
		mmr_t	overflow_pi_fifo_vc2_push     : 1;
		mmr_t	overflow_iilb_fifo_vc0_push   : 1;
		mmr_t	overflow_iilb_fifo_vc2_push   : 1;
		mmr_t	overflow_md_fifo_vc0_push     : 1;
		mmr_t	overflow_md_fifo_vc2_push     : 1;
		mmr_t	overflow_pi_fifo_vc0_credit   : 1;
		mmr_t	overflow_pi_fifo_vc2_credit   : 1;
		mmr_t	overflow_iilb_fifo_vc0_credit : 1;
		mmr_t	overflow_iilb_fifo_vc2_credit : 1;
		mmr_t	overflow_md_fifo_vc0_credit   : 1;
		mmr_t	overflow_md_fifo_vc2_credit   : 1;
		mmr_t	overflow_ni_fifo_vc0_credit   : 1;
		mmr_t	overflow_ni_fifo_vc1_credit   : 1;
		mmr_t	overflow_ni_fifo_vc2_credit   : 1;
		mmr_t	overflow_ni_fifo_vc3_credit   : 1;
		mmr_t	tail_timeout_fifo02_vc0       : 1;
		mmr_t	tail_timeout_fifo02_vc2       : 1;
		mmr_t	tail_timeout_fifo13_vc1       : 1;
		mmr_t	tail_timeout_fifo13_vc3       : 1;
		mmr_t	tail_timeout_ni_vc0           : 1;
		mmr_t	tail_timeout_ni_vc1           : 1;
		mmr_t	tail_timeout_ni_vc2           : 1;
		mmr_t	tail_timeout_ni_vc3           : 1;
	} sh_ni1_error_mask_1_s;
} sh_ni1_error_mask_1_u_t;
#else
typedef union sh_ni1_error_mask_1_u {
	mmr_t	sh_ni1_error_mask_1_regval;
	struct {
		mmr_t	tail_timeout_ni_vc3           : 1;
		mmr_t	tail_timeout_ni_vc2           : 1;
		mmr_t	tail_timeout_ni_vc1           : 1;
		mmr_t	tail_timeout_ni_vc0           : 1;
		mmr_t	tail_timeout_fifo13_vc3       : 1;
		mmr_t	tail_timeout_fifo13_vc1       : 1;
		mmr_t	tail_timeout_fifo02_vc2       : 1;
		mmr_t	tail_timeout_fifo02_vc0       : 1;
		mmr_t	overflow_ni_fifo_vc3_credit   : 1;
		mmr_t	overflow_ni_fifo_vc2_credit   : 1;
		mmr_t	overflow_ni_fifo_vc1_credit   : 1;
		mmr_t	overflow_ni_fifo_vc0_credit   : 1;
		mmr_t	overflow_md_fifo_vc2_credit   : 1;
		mmr_t	overflow_md_fifo_vc0_credit   : 1;
		mmr_t	overflow_iilb_fifo_vc2_credit : 1;
		mmr_t	overflow_iilb_fifo_vc0_credit : 1;
		mmr_t	overflow_pi_fifo_vc2_credit   : 1;
		mmr_t	overflow_pi_fifo_vc0_credit   : 1;
		mmr_t	overflow_md_fifo_vc2_push     : 1;
		mmr_t	overflow_md_fifo_vc0_push     : 1;
		mmr_t	overflow_iilb_fifo_vc2_push   : 1;
		mmr_t	overflow_iilb_fifo_vc0_push   : 1;
		mmr_t	overflow_pi_fifo_vc2_push     : 1;
		mmr_t	overflow_pi_fifo_vc0_push     : 1;
		mmr_t	overflow_ni_fifo_vc2_pop      : 1;
		mmr_t	overflow_ni_fifo_vc0_pop      : 1;
		mmr_t	overflow_md_fifo_vc2_pop      : 1;
		mmr_t	overflow_md_fifo_vc0_pop      : 1;
		mmr_t	overflow_iilb_fifo_vc2_pop    : 1;
		mmr_t	overflow_iilb_fifo_vc0_pop    : 1;
		mmr_t	overflow_pi_fifo_vc2_pop      : 1;
		mmr_t	overflow_pi_fifo_vc0_pop      : 1;
		mmr_t	overflow_ni_fifo_debit3       : 1;
		mmr_t	overflow_ni_fifo_debit2       : 1;
		mmr_t	overflow_ni_fifo_debit1       : 1;
		mmr_t	overflow_ni_fifo_debit0       : 1;
		mmr_t	overflow_md_fifo_debit2       : 1;
		mmr_t	overflow_md_fifo_debit0       : 1;
		mmr_t	overflow_iilb_fifo_debit2     : 1;
		mmr_t	overflow_iilb_fifo_debit0     : 1;
		mmr_t	overflow_pi_fifo_debit2       : 1;
		mmr_t	overflow_pi_fifo_debit0       : 1;
		mmr_t	overflow2_vc2_credit          : 1;
		mmr_t	overflow1_vc2_credit          : 1;
		mmr_t	overflow0_vc2_credit          : 1;
		mmr_t	overflow2_vc0_credit          : 1;
		mmr_t	overflow1_vc0_credit          : 1;
		mmr_t	overflow0_vc0_credit          : 1;
		mmr_t	overflow_fifo13_vc2_credit    : 1;
		mmr_t	overflow_fifo13_vc0_credit    : 1;
		mmr_t	overflow_fifo02_vc2_credit    : 1;
		mmr_t	overflow_fifo02_vc0_credit    : 1;
		mmr_t	overflow_fifo13_vc3_push      : 1;
		mmr_t	overflow_fifo13_vc1_push      : 1;
		mmr_t	overflow_fifo02_vc2_push      : 1;
		mmr_t	overflow_fifo02_vc0_push      : 1;
		mmr_t	overflow_fifo13_vc3_pop       : 1;
		mmr_t	overflow_fifo13_vc1_pop       : 1;
		mmr_t	overflow_fifo02_vc2_pop       : 1;
		mmr_t	overflow_fifo02_vc0_pop       : 1;
		mmr_t	overflow_fifo13_debit2        : 1;
		mmr_t	overflow_fifo13_debit0        : 1;
		mmr_t	overflow_fifo02_debit2        : 1;
		mmr_t	overflow_fifo02_debit0        : 1;
	} sh_ni1_error_mask_1_s;
} sh_ni1_error_mask_1_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_NI1_ERROR_MASK_2"                    */
/*                         ni1  Error Mask Bits                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni1_error_mask_2_u {
	mmr_t	sh_ni1_error_mask_2_regval;
	struct {
		mmr_t	illegal_vcni                   : 1;
		mmr_t	illegal_vcpi                   : 1;
		mmr_t	illegal_vcmd                   : 1;
		mmr_t	illegal_vciilb                 : 1;
		mmr_t	underflow_fifo02_vc0_pop       : 1;
		mmr_t	underflow_fifo02_vc2_pop       : 1;
		mmr_t	underflow_fifo13_vc1_pop       : 1;
		mmr_t	underflow_fifo13_vc3_pop       : 1;
		mmr_t	underflow_fifo02_vc0_push      : 1;
		mmr_t	underflow_fifo02_vc2_push      : 1;
		mmr_t	underflow_fifo13_vc1_push      : 1;
		mmr_t	underflow_fifo13_vc3_push      : 1;
		mmr_t	underflow_fifo02_vc0_credit    : 1;
		mmr_t	underflow_fifo02_vc2_credit    : 1;
		mmr_t	underflow_fifo13_vc0_credit    : 1;
		mmr_t	underflow_fifo13_vc2_credit    : 1;
		mmr_t	underflow0_vc0_credit          : 1;
		mmr_t	underflow1_vc0_credit          : 1;
		mmr_t	underflow2_vc0_credit          : 1;
		mmr_t	underflow0_vc2_credit          : 1;
		mmr_t	underflow1_vc2_credit          : 1;
		mmr_t	underflow2_vc2_credit          : 1;
		mmr_t	reserved_0                     : 10;
		mmr_t	underflow_pi_fifo_vc0_pop      : 1;
		mmr_t	underflow_pi_fifo_vc2_pop      : 1;
		mmr_t	underflow_iilb_fifo_vc0_pop    : 1;
		mmr_t	underflow_iilb_fifo_vc2_pop    : 1;
		mmr_t	underflow_md_fifo_vc0_pop      : 1;
		mmr_t	underflow_md_fifo_vc2_pop      : 1;
		mmr_t	underflow_ni_fifo_vc0_pop      : 1;
		mmr_t	underflow_ni_fifo_vc2_pop      : 1;
		mmr_t	underflow_pi_fifo_vc0_push     : 1;
		mmr_t	underflow_pi_fifo_vc2_push     : 1;
		mmr_t	underflow_iilb_fifo_vc0_push   : 1;
		mmr_t	underflow_iilb_fifo_vc2_push   : 1;
		mmr_t	underflow_md_fifo_vc0_push     : 1;
		mmr_t	underflow_md_fifo_vc2_push     : 1;
		mmr_t	underflow_pi_fifo_vc0_credit   : 1;
		mmr_t	underflow_pi_fifo_vc2_credit   : 1;
		mmr_t	underflow_iilb_fifo_vc0_credit : 1;
		mmr_t	underflow_iilb_fifo_vc2_credit : 1;
		mmr_t	underflow_md_fifo_vc0_credit   : 1;
		mmr_t	underflow_md_fifo_vc2_credit   : 1;
		mmr_t	underflow_ni_fifo_vc0_credit   : 1;
		mmr_t	underflow_ni_fifo_vc1_credit   : 1;
		mmr_t	underflow_ni_fifo_vc2_credit   : 1;
		mmr_t	underflow_ni_fifo_vc3_credit   : 1;
		mmr_t	llp_deadlock_vc0               : 1;
		mmr_t	llp_deadlock_vc1               : 1;
		mmr_t	llp_deadlock_vc2               : 1;
		mmr_t	llp_deadlock_vc3               : 1;
		mmr_t	chiplet_nomatch                : 1;
		mmr_t	lut_read_error                 : 1;
		mmr_t	retry_timeout_error            : 1;
		mmr_t	reserved_1                     : 1;
	} sh_ni1_error_mask_2_s;
} sh_ni1_error_mask_2_u_t;
#else
typedef union sh_ni1_error_mask_2_u {
	mmr_t	sh_ni1_error_mask_2_regval;
	struct {
		mmr_t	reserved_1                     : 1;
		mmr_t	retry_timeout_error            : 1;
		mmr_t	lut_read_error                 : 1;
		mmr_t	chiplet_nomatch                : 1;
		mmr_t	llp_deadlock_vc3               : 1;
		mmr_t	llp_deadlock_vc2               : 1;
		mmr_t	llp_deadlock_vc1               : 1;
		mmr_t	llp_deadlock_vc0               : 1;
		mmr_t	underflow_ni_fifo_vc3_credit   : 1;
		mmr_t	underflow_ni_fifo_vc2_credit   : 1;
		mmr_t	underflow_ni_fifo_vc1_credit   : 1;
		mmr_t	underflow_ni_fifo_vc0_credit   : 1;
		mmr_t	underflow_md_fifo_vc2_credit   : 1;
		mmr_t	underflow_md_fifo_vc0_credit   : 1;
		mmr_t	underflow_iilb_fifo_vc2_credit : 1;
		mmr_t	underflow_iilb_fifo_vc0_credit : 1;
		mmr_t	underflow_pi_fifo_vc2_credit   : 1;
		mmr_t	underflow_pi_fifo_vc0_credit   : 1;
		mmr_t	underflow_md_fifo_vc2_push     : 1;
		mmr_t	underflow_md_fifo_vc0_push     : 1;
		mmr_t	underflow_iilb_fifo_vc2_push   : 1;
		mmr_t	underflow_iilb_fifo_vc0_push   : 1;
		mmr_t	underflow_pi_fifo_vc2_push     : 1;
		mmr_t	underflow_pi_fifo_vc0_push     : 1;
		mmr_t	underflow_ni_fifo_vc2_pop      : 1;
		mmr_t	underflow_ni_fifo_vc0_pop      : 1;
		mmr_t	underflow_md_fifo_vc2_pop      : 1;
		mmr_t	underflow_md_fifo_vc0_pop      : 1;
		mmr_t	underflow_iilb_fifo_vc2_pop    : 1;
		mmr_t	underflow_iilb_fifo_vc0_pop    : 1;
		mmr_t	underflow_pi_fifo_vc2_pop      : 1;
		mmr_t	underflow_pi_fifo_vc0_pop      : 1;
		mmr_t	reserved_0                     : 10;
		mmr_t	underflow2_vc2_credit          : 1;
		mmr_t	underflow1_vc2_credit          : 1;
		mmr_t	underflow0_vc2_credit          : 1;
		mmr_t	underflow2_vc0_credit          : 1;
		mmr_t	underflow1_vc0_credit          : 1;
		mmr_t	underflow0_vc0_credit          : 1;
		mmr_t	underflow_fifo13_vc2_credit    : 1;
		mmr_t	underflow_fifo13_vc0_credit    : 1;
		mmr_t	underflow_fifo02_vc2_credit    : 1;
		mmr_t	underflow_fifo02_vc0_credit    : 1;
		mmr_t	underflow_fifo13_vc3_push      : 1;
		mmr_t	underflow_fifo13_vc1_push      : 1;
		mmr_t	underflow_fifo02_vc2_push      : 1;
		mmr_t	underflow_fifo02_vc0_push      : 1;
		mmr_t	underflow_fifo13_vc3_pop       : 1;
		mmr_t	underflow_fifo13_vc1_pop       : 1;
		mmr_t	underflow_fifo02_vc2_pop       : 1;
		mmr_t	underflow_fifo02_vc0_pop       : 1;
		mmr_t	illegal_vciilb                 : 1;
		mmr_t	illegal_vcmd                   : 1;
		mmr_t	illegal_vcpi                   : 1;
		mmr_t	illegal_vcni                   : 1;
	} sh_ni1_error_mask_2_s;
} sh_ni1_error_mask_2_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_NI1_FIRST_ERROR_1"                    */
/*                        ni1  First Error Bits                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni1_first_error_1_u {
	mmr_t	sh_ni1_first_error_1_regval;
	struct {
		mmr_t	overflow_fifo02_debit0        : 1;
		mmr_t	overflow_fifo02_debit2        : 1;
		mmr_t	overflow_fifo13_debit0        : 1;
		mmr_t	overflow_fifo13_debit2        : 1;
		mmr_t	overflow_fifo02_vc0_pop       : 1;
		mmr_t	overflow_fifo02_vc2_pop       : 1;
		mmr_t	overflow_fifo13_vc1_pop       : 1;
		mmr_t	overflow_fifo13_vc3_pop       : 1;
		mmr_t	overflow_fifo02_vc0_push      : 1;
		mmr_t	overflow_fifo02_vc2_push      : 1;
		mmr_t	overflow_fifo13_vc1_push      : 1;
		mmr_t	overflow_fifo13_vc3_push      : 1;
		mmr_t	overflow_fifo02_vc0_credit    : 1;
		mmr_t	overflow_fifo02_vc2_credit    : 1;
		mmr_t	overflow_fifo13_vc0_credit    : 1;
		mmr_t	overflow_fifo13_vc2_credit    : 1;
		mmr_t	overflow0_vc0_credit          : 1;
		mmr_t	overflow1_vc0_credit          : 1;
		mmr_t	overflow2_vc0_credit          : 1;
		mmr_t	overflow0_vc2_credit          : 1;
		mmr_t	overflow1_vc2_credit          : 1;
		mmr_t	overflow2_vc2_credit          : 1;
		mmr_t	overflow_pi_fifo_debit0       : 1;
		mmr_t	overflow_pi_fifo_debit2       : 1;
		mmr_t	overflow_iilb_fifo_debit0     : 1;
		mmr_t	overflow_iilb_fifo_debit2     : 1;
		mmr_t	overflow_md_fifo_debit0       : 1;
		mmr_t	overflow_md_fifo_debit2       : 1;
		mmr_t	overflow_ni_fifo_debit0       : 1;
		mmr_t	overflow_ni_fifo_debit1       : 1;
		mmr_t	overflow_ni_fifo_debit2       : 1;
		mmr_t	overflow_ni_fifo_debit3       : 1;
		mmr_t	overflow_pi_fifo_vc0_pop      : 1;
		mmr_t	overflow_pi_fifo_vc2_pop      : 1;
		mmr_t	overflow_iilb_fifo_vc0_pop    : 1;
		mmr_t	overflow_iilb_fifo_vc2_pop    : 1;
		mmr_t	overflow_md_fifo_vc0_pop      : 1;
		mmr_t	overflow_md_fifo_vc2_pop      : 1;
		mmr_t	overflow_ni_fifo_vc0_pop      : 1;
		mmr_t	overflow_ni_fifo_vc2_pop      : 1;
		mmr_t	overflow_pi_fifo_vc0_push     : 1;
		mmr_t	overflow_pi_fifo_vc2_push     : 1;
		mmr_t	overflow_iilb_fifo_vc0_push   : 1;
		mmr_t	overflow_iilb_fifo_vc2_push   : 1;
		mmr_t	overflow_md_fifo_vc0_push     : 1;
		mmr_t	overflow_md_fifo_vc2_push     : 1;
		mmr_t	overflow_pi_fifo_vc0_credit   : 1;
		mmr_t	overflow_pi_fifo_vc2_credit   : 1;
		mmr_t	overflow_iilb_fifo_vc0_credit : 1;
		mmr_t	overflow_iilb_fifo_vc2_credit : 1;
		mmr_t	overflow_md_fifo_vc0_credit   : 1;
		mmr_t	overflow_md_fifo_vc2_credit   : 1;
		mmr_t	overflow_ni_fifo_vc0_credit   : 1;
		mmr_t	overflow_ni_fifo_vc1_credit   : 1;
		mmr_t	overflow_ni_fifo_vc2_credit   : 1;
		mmr_t	overflow_ni_fifo_vc3_credit   : 1;
		mmr_t	tail_timeout_fifo02_vc0       : 1;
		mmr_t	tail_timeout_fifo02_vc2       : 1;
		mmr_t	tail_timeout_fifo13_vc1       : 1;
		mmr_t	tail_timeout_fifo13_vc3       : 1;
		mmr_t	tail_timeout_ni_vc0           : 1;
		mmr_t	tail_timeout_ni_vc1           : 1;
		mmr_t	tail_timeout_ni_vc2           : 1;
		mmr_t	tail_timeout_ni_vc3           : 1;
	} sh_ni1_first_error_1_s;
} sh_ni1_first_error_1_u_t;
#else
typedef union sh_ni1_first_error_1_u {
	mmr_t	sh_ni1_first_error_1_regval;
	struct {
		mmr_t	tail_timeout_ni_vc3           : 1;
		mmr_t	tail_timeout_ni_vc2           : 1;
		mmr_t	tail_timeout_ni_vc1           : 1;
		mmr_t	tail_timeout_ni_vc0           : 1;
		mmr_t	tail_timeout_fifo13_vc3       : 1;
		mmr_t	tail_timeout_fifo13_vc1       : 1;
		mmr_t	tail_timeout_fifo02_vc2       : 1;
		mmr_t	tail_timeout_fifo02_vc0       : 1;
		mmr_t	overflow_ni_fifo_vc3_credit   : 1;
		mmr_t	overflow_ni_fifo_vc2_credit   : 1;
		mmr_t	overflow_ni_fifo_vc1_credit   : 1;
		mmr_t	overflow_ni_fifo_vc0_credit   : 1;
		mmr_t	overflow_md_fifo_vc2_credit   : 1;
		mmr_t	overflow_md_fifo_vc0_credit   : 1;
		mmr_t	overflow_iilb_fifo_vc2_credit : 1;
		mmr_t	overflow_iilb_fifo_vc0_credit : 1;
		mmr_t	overflow_pi_fifo_vc2_credit   : 1;
		mmr_t	overflow_pi_fifo_vc0_credit   : 1;
		mmr_t	overflow_md_fifo_vc2_push     : 1;
		mmr_t	overflow_md_fifo_vc0_push     : 1;
		mmr_t	overflow_iilb_fifo_vc2_push   : 1;
		mmr_t	overflow_iilb_fifo_vc0_push   : 1;
		mmr_t	overflow_pi_fifo_vc2_push     : 1;
		mmr_t	overflow_pi_fifo_vc0_push     : 1;
		mmr_t	overflow_ni_fifo_vc2_pop      : 1;
		mmr_t	overflow_ni_fifo_vc0_pop      : 1;
		mmr_t	overflow_md_fifo_vc2_pop      : 1;
		mmr_t	overflow_md_fifo_vc0_pop      : 1;
		mmr_t	overflow_iilb_fifo_vc2_pop    : 1;
		mmr_t	overflow_iilb_fifo_vc0_pop    : 1;
		mmr_t	overflow_pi_fifo_vc2_pop      : 1;
		mmr_t	overflow_pi_fifo_vc0_pop      : 1;
		mmr_t	overflow_ni_fifo_debit3       : 1;
		mmr_t	overflow_ni_fifo_debit2       : 1;
		mmr_t	overflow_ni_fifo_debit1       : 1;
		mmr_t	overflow_ni_fifo_debit0       : 1;
		mmr_t	overflow_md_fifo_debit2       : 1;
		mmr_t	overflow_md_fifo_debit0       : 1;
		mmr_t	overflow_iilb_fifo_debit2     : 1;
		mmr_t	overflow_iilb_fifo_debit0     : 1;
		mmr_t	overflow_pi_fifo_debit2       : 1;
		mmr_t	overflow_pi_fifo_debit0       : 1;
		mmr_t	overflow2_vc2_credit          : 1;
		mmr_t	overflow1_vc2_credit          : 1;
		mmr_t	overflow0_vc2_credit          : 1;
		mmr_t	overflow2_vc0_credit          : 1;
		mmr_t	overflow1_vc0_credit          : 1;
		mmr_t	overflow0_vc0_credit          : 1;
		mmr_t	overflow_fifo13_vc2_credit    : 1;
		mmr_t	overflow_fifo13_vc0_credit    : 1;
		mmr_t	overflow_fifo02_vc2_credit    : 1;
		mmr_t	overflow_fifo02_vc0_credit    : 1;
		mmr_t	overflow_fifo13_vc3_push      : 1;
		mmr_t	overflow_fifo13_vc1_push      : 1;
		mmr_t	overflow_fifo02_vc2_push      : 1;
		mmr_t	overflow_fifo02_vc0_push      : 1;
		mmr_t	overflow_fifo13_vc3_pop       : 1;
		mmr_t	overflow_fifo13_vc1_pop       : 1;
		mmr_t	overflow_fifo02_vc2_pop       : 1;
		mmr_t	overflow_fifo02_vc0_pop       : 1;
		mmr_t	overflow_fifo13_debit2        : 1;
		mmr_t	overflow_fifo13_debit0        : 1;
		mmr_t	overflow_fifo02_debit2        : 1;
		mmr_t	overflow_fifo02_debit0        : 1;
	} sh_ni1_first_error_1_s;
} sh_ni1_first_error_1_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_NI1_FIRST_ERROR_2"                    */
/*                         ni1 First Error Bits                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni1_first_error_2_u {
	mmr_t	sh_ni1_first_error_2_regval;
	struct {
		mmr_t	illegal_vcni                   : 1;
		mmr_t	illegal_vcpi                   : 1;
		mmr_t	illegal_vcmd                   : 1;
		mmr_t	illegal_vciilb                 : 1;
		mmr_t	underflow_fifo02_vc0_pop       : 1;
		mmr_t	underflow_fifo02_vc2_pop       : 1;
		mmr_t	underflow_fifo13_vc1_pop       : 1;
		mmr_t	underflow_fifo13_vc3_pop       : 1;
		mmr_t	underflow_fifo02_vc0_push      : 1;
		mmr_t	underflow_fifo02_vc2_push      : 1;
		mmr_t	underflow_fifo13_vc1_push      : 1;
		mmr_t	underflow_fifo13_vc3_push      : 1;
		mmr_t	underflow_fifo02_vc0_credit    : 1;
		mmr_t	underflow_fifo02_vc2_credit    : 1;
		mmr_t	underflow_fifo13_vc0_credit    : 1;
		mmr_t	underflow_fifo13_vc2_credit    : 1;
		mmr_t	underflow0_vc0_credit          : 1;
		mmr_t	underflow1_vc0_credit          : 1;
		mmr_t	underflow2_vc0_credit          : 1;
		mmr_t	underflow0_vc2_credit          : 1;
		mmr_t	underflow1_vc2_credit          : 1;
		mmr_t	underflow2_vc2_credit          : 1;
		mmr_t	reserved_0                     : 10;
		mmr_t	underflow_pi_fifo_vc0_pop      : 1;
		mmr_t	underflow_pi_fifo_vc2_pop      : 1;
		mmr_t	underflow_iilb_fifo_vc0_pop    : 1;
		mmr_t	underflow_iilb_fifo_vc2_pop    : 1;
		mmr_t	underflow_md_fifo_vc0_pop      : 1;
		mmr_t	underflow_md_fifo_vc2_pop      : 1;
		mmr_t	underflow_ni_fifo_vc0_pop      : 1;
		mmr_t	underflow_ni_fifo_vc2_pop      : 1;
		mmr_t	underflow_pi_fifo_vc0_push     : 1;
		mmr_t	underflow_pi_fifo_vc2_push     : 1;
		mmr_t	underflow_iilb_fifo_vc0_push   : 1;
		mmr_t	underflow_iilb_fifo_vc2_push   : 1;
		mmr_t	underflow_md_fifo_vc0_push     : 1;
		mmr_t	underflow_md_fifo_vc2_push     : 1;
		mmr_t	underflow_pi_fifo_vc0_credit   : 1;
		mmr_t	underflow_pi_fifo_vc2_credit   : 1;
		mmr_t	underflow_iilb_fifo_vc0_credit : 1;
		mmr_t	underflow_iilb_fifo_vc2_credit : 1;
		mmr_t	underflow_md_fifo_vc0_credit   : 1;
		mmr_t	underflow_md_fifo_vc2_credit   : 1;
		mmr_t	underflow_ni_fifo_vc0_credit   : 1;
		mmr_t	underflow_ni_fifo_vc1_credit   : 1;
		mmr_t	underflow_ni_fifo_vc2_credit   : 1;
		mmr_t	underflow_ni_fifo_vc3_credit   : 1;
		mmr_t	llp_deadlock_vc0               : 1;
		mmr_t	llp_deadlock_vc1               : 1;
		mmr_t	llp_deadlock_vc2               : 1;
		mmr_t	llp_deadlock_vc3               : 1;
		mmr_t	chiplet_nomatch                : 1;
		mmr_t	lut_read_error                 : 1;
		mmr_t	retry_timeout_error            : 1;
		mmr_t	reserved_1                     : 1;
	} sh_ni1_first_error_2_s;
} sh_ni1_first_error_2_u_t;
#else
typedef union sh_ni1_first_error_2_u {
	mmr_t	sh_ni1_first_error_2_regval;
	struct {
		mmr_t	reserved_1                     : 1;
		mmr_t	retry_timeout_error            : 1;
		mmr_t	lut_read_error                 : 1;
		mmr_t	chiplet_nomatch                : 1;
		mmr_t	llp_deadlock_vc3               : 1;
		mmr_t	llp_deadlock_vc2               : 1;
		mmr_t	llp_deadlock_vc1               : 1;
		mmr_t	llp_deadlock_vc0               : 1;
		mmr_t	underflow_ni_fifo_vc3_credit   : 1;
		mmr_t	underflow_ni_fifo_vc2_credit   : 1;
		mmr_t	underflow_ni_fifo_vc1_credit   : 1;
		mmr_t	underflow_ni_fifo_vc0_credit   : 1;
		mmr_t	underflow_md_fifo_vc2_credit   : 1;
		mmr_t	underflow_md_fifo_vc0_credit   : 1;
		mmr_t	underflow_iilb_fifo_vc2_credit : 1;
		mmr_t	underflow_iilb_fifo_vc0_credit : 1;
		mmr_t	underflow_pi_fifo_vc2_credit   : 1;
		mmr_t	underflow_pi_fifo_vc0_credit   : 1;
		mmr_t	underflow_md_fifo_vc2_push     : 1;
		mmr_t	underflow_md_fifo_vc0_push     : 1;
		mmr_t	underflow_iilb_fifo_vc2_push   : 1;
		mmr_t	underflow_iilb_fifo_vc0_push   : 1;
		mmr_t	underflow_pi_fifo_vc2_push     : 1;
		mmr_t	underflow_pi_fifo_vc0_push     : 1;
		mmr_t	underflow_ni_fifo_vc2_pop      : 1;
		mmr_t	underflow_ni_fifo_vc0_pop      : 1;
		mmr_t	underflow_md_fifo_vc2_pop      : 1;
		mmr_t	underflow_md_fifo_vc0_pop      : 1;
		mmr_t	underflow_iilb_fifo_vc2_pop    : 1;
		mmr_t	underflow_iilb_fifo_vc0_pop    : 1;
		mmr_t	underflow_pi_fifo_vc2_pop      : 1;
		mmr_t	underflow_pi_fifo_vc0_pop      : 1;
		mmr_t	reserved_0                     : 10;
		mmr_t	underflow2_vc2_credit          : 1;
		mmr_t	underflow1_vc2_credit          : 1;
		mmr_t	underflow0_vc2_credit          : 1;
		mmr_t	underflow2_vc0_credit          : 1;
		mmr_t	underflow1_vc0_credit          : 1;
		mmr_t	underflow0_vc0_credit          : 1;
		mmr_t	underflow_fifo13_vc2_credit    : 1;
		mmr_t	underflow_fifo13_vc0_credit    : 1;
		mmr_t	underflow_fifo02_vc2_credit    : 1;
		mmr_t	underflow_fifo02_vc0_credit    : 1;
		mmr_t	underflow_fifo13_vc3_push      : 1;
		mmr_t	underflow_fifo13_vc1_push      : 1;
		mmr_t	underflow_fifo02_vc2_push      : 1;
		mmr_t	underflow_fifo02_vc0_push      : 1;
		mmr_t	underflow_fifo13_vc3_pop       : 1;
		mmr_t	underflow_fifo13_vc1_pop       : 1;
		mmr_t	underflow_fifo02_vc2_pop       : 1;
		mmr_t	underflow_fifo02_vc0_pop       : 1;
		mmr_t	illegal_vciilb                 : 1;
		mmr_t	illegal_vcmd                   : 1;
		mmr_t	illegal_vcpi                   : 1;
		mmr_t	illegal_vcni                   : 1;
	} sh_ni1_first_error_2_s;
} sh_ni1_first_error_2_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_NI1_ERROR_DETAIL_1"                   */
/*                ni1 Chiplet no match header bits 63:0                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni1_error_detail_1_u {
	mmr_t	sh_ni1_error_detail_1_regval;
	struct {
		mmr_t	header      : 64;
	} sh_ni1_error_detail_1_s;
} sh_ni1_error_detail_1_u_t;
#else
typedef union sh_ni1_error_detail_1_u {
	mmr_t	sh_ni1_error_detail_1_regval;
	struct {
		mmr_t	header      : 64;
	} sh_ni1_error_detail_1_s;
} sh_ni1_error_detail_1_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_NI1_ERROR_DETAIL_2"                   */
/*               ni1 Chiplet no match header bits 127:64                */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni1_error_detail_2_u {
	mmr_t	sh_ni1_error_detail_2_regval;
	struct {
		mmr_t	header      : 64;
	} sh_ni1_error_detail_2_s;
} sh_ni1_error_detail_2_u_t;
#else
typedef union sh_ni1_error_detail_2_u {
	mmr_t	sh_ni1_error_detail_2_regval;
	struct {
		mmr_t	header      : 64;
	} sh_ni1_error_detail_2_s;
} sh_ni1_error_detail_2_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_CORRECTED_DETAIL_1"                  */
/*                       Corrected error details                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_corrected_detail_1_u {
	mmr_t	sh_xn_corrected_detail_1_regval;
	struct {
		mmr_t	ecc0_syndrome : 8;
		mmr_t	ecc0_wc       : 2;
		mmr_t	ecc0_vc       : 2;
		mmr_t	reserved_0    : 4;
		mmr_t	ecc1_syndrome : 8;
		mmr_t	ecc1_wc       : 2;
		mmr_t	ecc1_vc       : 2;
		mmr_t	reserved_1    : 4;
		mmr_t	ecc2_syndrome : 8;
		mmr_t	ecc2_wc       : 2;
		mmr_t	ecc2_vc       : 2;
		mmr_t	reserved_2    : 4;
		mmr_t	ecc3_syndrome : 8;
		mmr_t	ecc3_wc       : 2;
		mmr_t	ecc3_vc       : 2;
		mmr_t	reserved_3    : 4;
	} sh_xn_corrected_detail_1_s;
} sh_xn_corrected_detail_1_u_t;
#else
typedef union sh_xn_corrected_detail_1_u {
	mmr_t	sh_xn_corrected_detail_1_regval;
	struct {
		mmr_t	reserved_3    : 4;
		mmr_t	ecc3_vc       : 2;
		mmr_t	ecc3_wc       : 2;
		mmr_t	ecc3_syndrome : 8;
		mmr_t	reserved_2    : 4;
		mmr_t	ecc2_vc       : 2;
		mmr_t	ecc2_wc       : 2;
		mmr_t	ecc2_syndrome : 8;
		mmr_t	reserved_1    : 4;
		mmr_t	ecc1_vc       : 2;
		mmr_t	ecc1_wc       : 2;
		mmr_t	ecc1_syndrome : 8;
		mmr_t	reserved_0    : 4;
		mmr_t	ecc0_vc       : 2;
		mmr_t	ecc0_wc       : 2;
		mmr_t	ecc0_syndrome : 8;
	} sh_xn_corrected_detail_1_s;
} sh_xn_corrected_detail_1_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_CORRECTED_DETAIL_2"                  */
/*                         Corrected error data                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_corrected_detail_2_u {
	mmr_t	sh_xn_corrected_detail_2_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_corrected_detail_2_s;
} sh_xn_corrected_detail_2_u_t;
#else
typedef union sh_xn_corrected_detail_2_u {
	mmr_t	sh_xn_corrected_detail_2_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_corrected_detail_2_s;
} sh_xn_corrected_detail_2_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_CORRECTED_DETAIL_3"                  */
/*                       Corrected error header0                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_corrected_detail_3_u {
	mmr_t	sh_xn_corrected_detail_3_regval;
	struct {
		mmr_t	header0     : 64;
	} sh_xn_corrected_detail_3_s;
} sh_xn_corrected_detail_3_u_t;
#else
typedef union sh_xn_corrected_detail_3_u {
	mmr_t	sh_xn_corrected_detail_3_regval;
	struct {
		mmr_t	header0     : 64;
	} sh_xn_corrected_detail_3_s;
} sh_xn_corrected_detail_3_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XN_CORRECTED_DETAIL_4"                  */
/*                       Corrected error header1                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_corrected_detail_4_u {
	mmr_t	sh_xn_corrected_detail_4_regval;
	struct {
		mmr_t	header1     : 42;
		mmr_t	reserved_0  : 20;
		mmr_t	err_group   : 2;
	} sh_xn_corrected_detail_4_s;
} sh_xn_corrected_detail_4_u_t;
#else
typedef union sh_xn_corrected_detail_4_u {
	mmr_t	sh_xn_corrected_detail_4_regval;
	struct {
		mmr_t	err_group   : 2;
		mmr_t	reserved_0  : 20;
		mmr_t	header1     : 42;
	} sh_xn_corrected_detail_4_s;
} sh_xn_corrected_detail_4_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_UNCORRECTED_DETAIL_1"                 */
/*                      Uncorrected error details                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_uncorrected_detail_1_u {
	mmr_t	sh_xn_uncorrected_detail_1_regval;
	struct {
		mmr_t	ecc0_syndrome : 8;
		mmr_t	ecc0_wc       : 2;
		mmr_t	ecc0_vc       : 2;
		mmr_t	reserved_0    : 4;
		mmr_t	ecc1_syndrome : 8;
		mmr_t	ecc1_wc       : 2;
		mmr_t	ecc1_vc       : 2;
		mmr_t	reserved_1    : 4;
		mmr_t	ecc2_syndrome : 8;
		mmr_t	ecc2_wc       : 2;
		mmr_t	ecc2_vc       : 2;
		mmr_t	reserved_2    : 4;
		mmr_t	ecc3_syndrome : 8;
		mmr_t	ecc3_wc       : 2;
		mmr_t	ecc3_vc       : 2;
		mmr_t	reserved_3    : 4;
	} sh_xn_uncorrected_detail_1_s;
} sh_xn_uncorrected_detail_1_u_t;
#else
typedef union sh_xn_uncorrected_detail_1_u {
	mmr_t	sh_xn_uncorrected_detail_1_regval;
	struct {
		mmr_t	reserved_3    : 4;
		mmr_t	ecc3_vc       : 2;
		mmr_t	ecc3_wc       : 2;
		mmr_t	ecc3_syndrome : 8;
		mmr_t	reserved_2    : 4;
		mmr_t	ecc2_vc       : 2;
		mmr_t	ecc2_wc       : 2;
		mmr_t	ecc2_syndrome : 8;
		mmr_t	reserved_1    : 4;
		mmr_t	ecc1_vc       : 2;
		mmr_t	ecc1_wc       : 2;
		mmr_t	ecc1_syndrome : 8;
		mmr_t	reserved_0    : 4;
		mmr_t	ecc0_vc       : 2;
		mmr_t	ecc0_wc       : 2;
		mmr_t	ecc0_syndrome : 8;
	} sh_xn_uncorrected_detail_1_s;
} sh_xn_uncorrected_detail_1_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_UNCORRECTED_DETAIL_2"                 */
/*                        Uncorrected error data                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_uncorrected_detail_2_u {
	mmr_t	sh_xn_uncorrected_detail_2_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_uncorrected_detail_2_s;
} sh_xn_uncorrected_detail_2_u_t;
#else
typedef union sh_xn_uncorrected_detail_2_u {
	mmr_t	sh_xn_uncorrected_detail_2_regval;
	struct {
		mmr_t	data        : 64;
	} sh_xn_uncorrected_detail_2_s;
} sh_xn_uncorrected_detail_2_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_UNCORRECTED_DETAIL_3"                 */
/*                      Uncorrected error header0                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_uncorrected_detail_3_u {
	mmr_t	sh_xn_uncorrected_detail_3_regval;
	struct {
		mmr_t	header0     : 64;
	} sh_xn_uncorrected_detail_3_s;
} sh_xn_uncorrected_detail_3_u_t;
#else
typedef union sh_xn_uncorrected_detail_3_u {
	mmr_t	sh_xn_uncorrected_detail_3_regval;
	struct {
		mmr_t	header0     : 64;
	} sh_xn_uncorrected_detail_3_s;
} sh_xn_uncorrected_detail_3_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_XN_UNCORRECTED_DETAIL_4"                 */
/*                      Uncorrected error header1                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_uncorrected_detail_4_u {
	mmr_t	sh_xn_uncorrected_detail_4_regval;
	struct {
		mmr_t	header1     : 42;
		mmr_t	reserved_0  : 20;
		mmr_t	err_group   : 2;
	} sh_xn_uncorrected_detail_4_s;
} sh_xn_uncorrected_detail_4_u_t;
#else
typedef union sh_xn_uncorrected_detail_4_u {
	mmr_t	sh_xn_uncorrected_detail_4_regval;
	struct {
		mmr_t	err_group   : 2;
		mmr_t	reserved_0  : 20;
		mmr_t	header1     : 42;
	} sh_xn_uncorrected_detail_4_s;
} sh_xn_uncorrected_detail_4_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_XNMD_ERROR_DETAIL_1"                   */
/*                      Look Up Table Address (md)                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnmd_error_detail_1_u {
	mmr_t	sh_xnmd_error_detail_1_regval;
	struct {
		mmr_t	lut_addr    : 11;
		mmr_t	reserved_0  : 53;
	} sh_xnmd_error_detail_1_s;
} sh_xnmd_error_detail_1_u_t;
#else
typedef union sh_xnmd_error_detail_1_u {
	mmr_t	sh_xnmd_error_detail_1_regval;
	struct {
		mmr_t	reserved_0  : 53;
		mmr_t	lut_addr    : 11;
	} sh_xnmd_error_detail_1_s;
} sh_xnmd_error_detail_1_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_XNPI_ERROR_DETAIL_1"                   */
/*                      Look Up Table Address (pi)                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnpi_error_detail_1_u {
	mmr_t	sh_xnpi_error_detail_1_regval;
	struct {
		mmr_t	lut_addr    : 11;
		mmr_t	reserved_0  : 53;
	} sh_xnpi_error_detail_1_s;
} sh_xnpi_error_detail_1_u_t;
#else
typedef union sh_xnpi_error_detail_1_u {
	mmr_t	sh_xnpi_error_detail_1_regval;
	struct {
		mmr_t	reserved_0  : 53;
		mmr_t	lut_addr    : 11;
	} sh_xnpi_error_detail_1_s;
} sh_xnpi_error_detail_1_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XNIILB_ERROR_DETAIL_1"                  */
/*                    Chiplet NoMatch header [63:0]                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xniilb_error_detail_1_u {
	mmr_t	sh_xniilb_error_detail_1_regval;
	struct {
		mmr_t	header      : 64;
	} sh_xniilb_error_detail_1_s;
} sh_xniilb_error_detail_1_u_t;
#else
typedef union sh_xniilb_error_detail_1_u {
	mmr_t	sh_xniilb_error_detail_1_regval;
	struct {
		mmr_t	header      : 64;
	} sh_xniilb_error_detail_1_s;
} sh_xniilb_error_detail_1_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XNIILB_ERROR_DETAIL_2"                  */
/*                   Chiplet NoMatch header [127:64]                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xniilb_error_detail_2_u {
	mmr_t	sh_xniilb_error_detail_2_regval;
	struct {
		mmr_t	header      : 64;
	} sh_xniilb_error_detail_2_s;
} sh_xniilb_error_detail_2_u_t;
#else
typedef union sh_xniilb_error_detail_2_u {
	mmr_t	sh_xniilb_error_detail_2_regval;
	struct {
		mmr_t	header      : 64;
	} sh_xniilb_error_detail_2_s;
} sh_xniilb_error_detail_2_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XNIILB_ERROR_DETAIL_3"                  */
/*                     Look Up Table Address (iilb)                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xniilb_error_detail_3_u {
	mmr_t	sh_xniilb_error_detail_3_regval;
	struct {
		mmr_t	lut_addr    : 11;
		mmr_t	reserved_0  : 53;
	} sh_xniilb_error_detail_3_s;
} sh_xniilb_error_detail_3_u_t;
#else
typedef union sh_xniilb_error_detail_3_u {
	mmr_t	sh_xniilb_error_detail_3_regval;
	struct {
		mmr_t	reserved_0  : 53;
		mmr_t	lut_addr    : 11;
	} sh_xniilb_error_detail_3_s;
} sh_xniilb_error_detail_3_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_NI0_ERROR_DETAIL_3"                   */
/*                     Look Up Table Address (ni0)                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni0_error_detail_3_u {
	mmr_t	sh_ni0_error_detail_3_regval;
	struct {
		mmr_t	lut_addr    : 11;
		mmr_t	reserved_0  : 53;
	} sh_ni0_error_detail_3_s;
} sh_ni0_error_detail_3_u_t;
#else
typedef union sh_ni0_error_detail_3_u {
	mmr_t	sh_ni0_error_detail_3_regval;
	struct {
		mmr_t	reserved_0  : 53;
		mmr_t	lut_addr    : 11;
	} sh_ni0_error_detail_3_s;
} sh_ni0_error_detail_3_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_NI1_ERROR_DETAIL_3"                   */
/*                     Look Up Table Address (ni1)                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ni1_error_detail_3_u {
	mmr_t	sh_ni1_error_detail_3_regval;
	struct {
		mmr_t	lut_addr    : 11;
		mmr_t	reserved_0  : 53;
	} sh_ni1_error_detail_3_s;
} sh_ni1_error_detail_3_u_t;
#else
typedef union sh_ni1_error_detail_3_u {
	mmr_t	sh_ni1_error_detail_3_regval;
	struct {
		mmr_t	reserved_0  : 53;
		mmr_t	lut_addr    : 11;
	} sh_ni1_error_detail_3_s;
} sh_ni1_error_detail_3_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_XN_ERROR_SUMMARY"                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_error_summary_u {
	mmr_t	sh_xn_error_summary_regval;
	struct {
		mmr_t	ni0_pop_overflow        : 1;
		mmr_t	ni0_push_overflow       : 1;
		mmr_t	ni0_credit_overflow     : 1;
		mmr_t	ni0_debit_overflow      : 1;
		mmr_t	ni0_pop_underflow       : 1;
		mmr_t	ni0_push_underflow      : 1;
		mmr_t	ni0_credit_underflow    : 1;
		mmr_t	ni0_llp_error           : 1;
		mmr_t	ni0_pipe_error          : 1;
		mmr_t	ni1_pop_overflow        : 1;
		mmr_t	ni1_push_overflow       : 1;
		mmr_t	ni1_credit_overflow     : 1;
		mmr_t	ni1_debit_overflow      : 1;
		mmr_t	ni1_pop_underflow       : 1;
		mmr_t	ni1_push_underflow      : 1;
		mmr_t	ni1_credit_underflow    : 1;
		mmr_t	ni1_llp_error           : 1;
		mmr_t	ni1_pipe_error          : 1;
		mmr_t	xnmd_credit_overflow    : 1;
		mmr_t	xnmd_debit_overflow     : 1;
		mmr_t	xnmd_data_buff_overflow : 1;
		mmr_t	xnmd_credit_underflow   : 1;
		mmr_t	xnmd_sbe_error          : 1;
		mmr_t	xnmd_uce_error          : 1;
		mmr_t	xnmd_lut_error          : 1;
		mmr_t	xnpi_credit_overflow    : 1;
		mmr_t	xnpi_debit_overflow     : 1;
		mmr_t	xnpi_data_buff_overflow : 1;
		mmr_t	xnpi_credit_underflow   : 1;
		mmr_t	xnpi_sbe_error          : 1;
		mmr_t	xnpi_uce_error          : 1;
		mmr_t	xnpi_lut_error          : 1;
		mmr_t	iilb_debit_overflow     : 1;
		mmr_t	iilb_credit_overflow    : 1;
		mmr_t	iilb_fifo_overflow      : 1;
		mmr_t	iilb_credit_underflow   : 1;
		mmr_t	iilb_fifo_underflow     : 1;
		mmr_t	iilb_chiplet_or_lut     : 1;
		mmr_t	reserved_0              : 26;
	} sh_xn_error_summary_s;
} sh_xn_error_summary_u_t;
#else
typedef union sh_xn_error_summary_u {
	mmr_t	sh_xn_error_summary_regval;
	struct {
		mmr_t	reserved_0              : 26;
		mmr_t	iilb_chiplet_or_lut     : 1;
		mmr_t	iilb_fifo_underflow     : 1;
		mmr_t	iilb_credit_underflow   : 1;
		mmr_t	iilb_fifo_overflow      : 1;
		mmr_t	iilb_credit_overflow    : 1;
		mmr_t	iilb_debit_overflow     : 1;
		mmr_t	xnpi_lut_error          : 1;
		mmr_t	xnpi_uce_error          : 1;
		mmr_t	xnpi_sbe_error          : 1;
		mmr_t	xnpi_credit_underflow   : 1;
		mmr_t	xnpi_data_buff_overflow : 1;
		mmr_t	xnpi_debit_overflow     : 1;
		mmr_t	xnpi_credit_overflow    : 1;
		mmr_t	xnmd_lut_error          : 1;
		mmr_t	xnmd_uce_error          : 1;
		mmr_t	xnmd_sbe_error          : 1;
		mmr_t	xnmd_credit_underflow   : 1;
		mmr_t	xnmd_data_buff_overflow : 1;
		mmr_t	xnmd_debit_overflow     : 1;
		mmr_t	xnmd_credit_overflow    : 1;
		mmr_t	ni1_pipe_error          : 1;
		mmr_t	ni1_llp_error           : 1;
		mmr_t	ni1_credit_underflow    : 1;
		mmr_t	ni1_push_underflow      : 1;
		mmr_t	ni1_pop_underflow       : 1;
		mmr_t	ni1_debit_overflow      : 1;
		mmr_t	ni1_credit_overflow     : 1;
		mmr_t	ni1_push_overflow       : 1;
		mmr_t	ni1_pop_overflow        : 1;
		mmr_t	ni0_pipe_error          : 1;
		mmr_t	ni0_llp_error           : 1;
		mmr_t	ni0_credit_underflow    : 1;
		mmr_t	ni0_push_underflow      : 1;
		mmr_t	ni0_pop_underflow       : 1;
		mmr_t	ni0_debit_overflow      : 1;
		mmr_t	ni0_credit_overflow     : 1;
		mmr_t	ni0_push_overflow       : 1;
		mmr_t	ni0_pop_overflow        : 1;
	} sh_xn_error_summary_s;
} sh_xn_error_summary_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_XN_ERROR_OVERFLOW"                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_error_overflow_u {
	mmr_t	sh_xn_error_overflow_regval;
	struct {
		mmr_t	ni0_pop_overflow        : 1;
		mmr_t	ni0_push_overflow       : 1;
		mmr_t	ni0_credit_overflow     : 1;
		mmr_t	ni0_debit_overflow      : 1;
		mmr_t	ni0_pop_underflow       : 1;
		mmr_t	ni0_push_underflow      : 1;
		mmr_t	ni0_credit_underflow    : 1;
		mmr_t	ni0_llp_error           : 1;
		mmr_t	ni0_pipe_error          : 1;
		mmr_t	ni1_pop_overflow        : 1;
		mmr_t	ni1_push_overflow       : 1;
		mmr_t	ni1_credit_overflow     : 1;
		mmr_t	ni1_debit_overflow      : 1;
		mmr_t	ni1_pop_underflow       : 1;
		mmr_t	ni1_push_underflow      : 1;
		mmr_t	ni1_credit_underflow    : 1;
		mmr_t	ni1_llp_error           : 1;
		mmr_t	ni1_pipe_error          : 1;
		mmr_t	xnmd_credit_overflow    : 1;
		mmr_t	xnmd_debit_overflow     : 1;
		mmr_t	xnmd_data_buff_overflow : 1;
		mmr_t	xnmd_credit_underflow   : 1;
		mmr_t	xnmd_sbe_error          : 1;
		mmr_t	xnmd_uce_error          : 1;
		mmr_t	xnmd_lut_error          : 1;
		mmr_t	xnpi_credit_overflow    : 1;
		mmr_t	xnpi_debit_overflow     : 1;
		mmr_t	xnpi_data_buff_overflow : 1;
		mmr_t	xnpi_credit_underflow   : 1;
		mmr_t	xnpi_sbe_error          : 1;
		mmr_t	xnpi_uce_error          : 1;
		mmr_t	xnpi_lut_error          : 1;
		mmr_t	iilb_debit_overflow     : 1;
		mmr_t	iilb_credit_overflow    : 1;
		mmr_t	iilb_fifo_overflow      : 1;
		mmr_t	iilb_credit_underflow   : 1;
		mmr_t	iilb_fifo_underflow     : 1;
		mmr_t	iilb_chiplet_or_lut     : 1;
		mmr_t	reserved_0              : 26;
	} sh_xn_error_overflow_s;
} sh_xn_error_overflow_u_t;
#else
typedef union sh_xn_error_overflow_u {
	mmr_t	sh_xn_error_overflow_regval;
	struct {
		mmr_t	reserved_0              : 26;
		mmr_t	iilb_chiplet_or_lut     : 1;
		mmr_t	iilb_fifo_underflow     : 1;
		mmr_t	iilb_credit_underflow   : 1;
		mmr_t	iilb_fifo_overflow      : 1;
		mmr_t	iilb_credit_overflow    : 1;
		mmr_t	iilb_debit_overflow     : 1;
		mmr_t	xnpi_lut_error          : 1;
		mmr_t	xnpi_uce_error          : 1;
		mmr_t	xnpi_sbe_error          : 1;
		mmr_t	xnpi_credit_underflow   : 1;
		mmr_t	xnpi_data_buff_overflow : 1;
		mmr_t	xnpi_debit_overflow     : 1;
		mmr_t	xnpi_credit_overflow    : 1;
		mmr_t	xnmd_lut_error          : 1;
		mmr_t	xnmd_uce_error          : 1;
		mmr_t	xnmd_sbe_error          : 1;
		mmr_t	xnmd_credit_underflow   : 1;
		mmr_t	xnmd_data_buff_overflow : 1;
		mmr_t	xnmd_debit_overflow     : 1;
		mmr_t	xnmd_credit_overflow    : 1;
		mmr_t	ni1_pipe_error          : 1;
		mmr_t	ni1_llp_error           : 1;
		mmr_t	ni1_credit_underflow    : 1;
		mmr_t	ni1_push_underflow      : 1;
		mmr_t	ni1_pop_underflow       : 1;
		mmr_t	ni1_debit_overflow      : 1;
		mmr_t	ni1_credit_overflow     : 1;
		mmr_t	ni1_push_overflow       : 1;
		mmr_t	ni1_pop_overflow        : 1;
		mmr_t	ni0_pipe_error          : 1;
		mmr_t	ni0_llp_error           : 1;
		mmr_t	ni0_credit_underflow    : 1;
		mmr_t	ni0_push_underflow      : 1;
		mmr_t	ni0_pop_underflow       : 1;
		mmr_t	ni0_debit_overflow      : 1;
		mmr_t	ni0_credit_overflow     : 1;
		mmr_t	ni0_push_overflow       : 1;
		mmr_t	ni0_pop_overflow        : 1;
	} sh_xn_error_overflow_s;
} sh_xn_error_overflow_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_XN_ERROR_MASK"                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_error_mask_u {
	mmr_t	sh_xn_error_mask_regval;
	struct {
		mmr_t	ni0_pop_overflow        : 1;
		mmr_t	ni0_push_overflow       : 1;
		mmr_t	ni0_credit_overflow     : 1;
		mmr_t	ni0_debit_overflow      : 1;
		mmr_t	ni0_pop_underflow       : 1;
		mmr_t	ni0_push_underflow      : 1;
		mmr_t	ni0_credit_underflow    : 1;
		mmr_t	ni0_llp_error           : 1;
		mmr_t	ni0_pipe_error          : 1;
		mmr_t	ni1_pop_overflow        : 1;
		mmr_t	ni1_push_overflow       : 1;
		mmr_t	ni1_credit_overflow     : 1;
		mmr_t	ni1_debit_overflow      : 1;
		mmr_t	ni1_pop_underflow       : 1;
		mmr_t	ni1_push_underflow      : 1;
		mmr_t	ni1_credit_underflow    : 1;
		mmr_t	ni1_llp_error           : 1;
		mmr_t	ni1_pipe_error          : 1;
		mmr_t	xnmd_credit_overflow    : 1;
		mmr_t	xnmd_debit_overflow     : 1;
		mmr_t	xnmd_data_buff_overflow : 1;
		mmr_t	xnmd_credit_underflow   : 1;
		mmr_t	xnmd_sbe_error          : 1;
		mmr_t	xnmd_uce_error          : 1;
		mmr_t	xnmd_lut_error          : 1;
		mmr_t	xnpi_credit_overflow    : 1;
		mmr_t	xnpi_debit_overflow     : 1;
		mmr_t	xnpi_data_buff_overflow : 1;
		mmr_t	xnpi_credit_underflow   : 1;
		mmr_t	xnpi_sbe_error          : 1;
		mmr_t	xnpi_uce_error          : 1;
		mmr_t	xnpi_lut_error          : 1;
		mmr_t	iilb_debit_overflow     : 1;
		mmr_t	iilb_credit_overflow    : 1;
		mmr_t	iilb_fifo_overflow      : 1;
		mmr_t	iilb_credit_underflow   : 1;
		mmr_t	iilb_fifo_underflow     : 1;
		mmr_t	iilb_chiplet_or_lut     : 1;
		mmr_t	reserved_0              : 26;
	} sh_xn_error_mask_s;
} sh_xn_error_mask_u_t;
#else
typedef union sh_xn_error_mask_u {
	mmr_t	sh_xn_error_mask_regval;
	struct {
		mmr_t	reserved_0              : 26;
		mmr_t	iilb_chiplet_or_lut     : 1;
		mmr_t	iilb_fifo_underflow     : 1;
		mmr_t	iilb_credit_underflow   : 1;
		mmr_t	iilb_fifo_overflow      : 1;
		mmr_t	iilb_credit_overflow    : 1;
		mmr_t	iilb_debit_overflow     : 1;
		mmr_t	xnpi_lut_error          : 1;
		mmr_t	xnpi_uce_error          : 1;
		mmr_t	xnpi_sbe_error          : 1;
		mmr_t	xnpi_credit_underflow   : 1;
		mmr_t	xnpi_data_buff_overflow : 1;
		mmr_t	xnpi_debit_overflow     : 1;
		mmr_t	xnpi_credit_overflow    : 1;
		mmr_t	xnmd_lut_error          : 1;
		mmr_t	xnmd_uce_error          : 1;
		mmr_t	xnmd_sbe_error          : 1;
		mmr_t	xnmd_credit_underflow   : 1;
		mmr_t	xnmd_data_buff_overflow : 1;
		mmr_t	xnmd_debit_overflow     : 1;
		mmr_t	xnmd_credit_overflow    : 1;
		mmr_t	ni1_pipe_error          : 1;
		mmr_t	ni1_llp_error           : 1;
		mmr_t	ni1_credit_underflow    : 1;
		mmr_t	ni1_push_underflow      : 1;
		mmr_t	ni1_pop_underflow       : 1;
		mmr_t	ni1_debit_overflow      : 1;
		mmr_t	ni1_credit_overflow     : 1;
		mmr_t	ni1_push_overflow       : 1;
		mmr_t	ni1_pop_overflow        : 1;
		mmr_t	ni0_pipe_error          : 1;
		mmr_t	ni0_llp_error           : 1;
		mmr_t	ni0_credit_underflow    : 1;
		mmr_t	ni0_push_underflow      : 1;
		mmr_t	ni0_pop_underflow       : 1;
		mmr_t	ni0_debit_overflow      : 1;
		mmr_t	ni0_credit_overflow     : 1;
		mmr_t	ni0_push_overflow       : 1;
		mmr_t	ni0_pop_overflow        : 1;
	} sh_xn_error_mask_s;
} sh_xn_error_mask_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_XN_FIRST_ERROR"                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_first_error_u {
	mmr_t	sh_xn_first_error_regval;
	struct {
		mmr_t	ni0_pop_overflow        : 1;
		mmr_t	ni0_push_overflow       : 1;
		mmr_t	ni0_credit_overflow     : 1;
		mmr_t	ni0_debit_overflow      : 1;
		mmr_t	ni0_pop_underflow       : 1;
		mmr_t	ni0_push_underflow      : 1;
		mmr_t	ni0_credit_underflow    : 1;
		mmr_t	ni0_llp_error           : 1;
		mmr_t	ni0_pipe_error          : 1;
		mmr_t	ni1_pop_overflow        : 1;
		mmr_t	ni1_push_overflow       : 1;
		mmr_t	ni1_credit_overflow     : 1;
		mmr_t	ni1_debit_overflow      : 1;
		mmr_t	ni1_pop_underflow       : 1;
		mmr_t	ni1_push_underflow      : 1;
		mmr_t	ni1_credit_underflow    : 1;
		mmr_t	ni1_llp_error           : 1;
		mmr_t	ni1_pipe_error          : 1;
		mmr_t	xnmd_credit_overflow    : 1;
		mmr_t	xnmd_debit_overflow     : 1;
		mmr_t	xnmd_data_buff_overflow : 1;
		mmr_t	xnmd_credit_underflow   : 1;
		mmr_t	xnmd_sbe_error          : 1;
		mmr_t	xnmd_uce_error          : 1;
		mmr_t	xnmd_lut_error          : 1;
		mmr_t	xnpi_credit_overflow    : 1;
		mmr_t	xnpi_debit_overflow     : 1;
		mmr_t	xnpi_data_buff_overflow : 1;
		mmr_t	xnpi_credit_underflow   : 1;
		mmr_t	xnpi_sbe_error          : 1;
		mmr_t	xnpi_uce_error          : 1;
		mmr_t	xnpi_lut_error          : 1;
		mmr_t	iilb_debit_overflow     : 1;
		mmr_t	iilb_credit_overflow    : 1;
		mmr_t	iilb_fifo_overflow      : 1;
		mmr_t	iilb_credit_underflow   : 1;
		mmr_t	iilb_fifo_underflow     : 1;
		mmr_t	iilb_chiplet_or_lut     : 1;
		mmr_t	reserved_0              : 26;
	} sh_xn_first_error_s;
} sh_xn_first_error_u_t;
#else
typedef union sh_xn_first_error_u {
	mmr_t	sh_xn_first_error_regval;
	struct {
		mmr_t	reserved_0              : 26;
		mmr_t	iilb_chiplet_or_lut     : 1;
		mmr_t	iilb_fifo_underflow     : 1;
		mmr_t	iilb_credit_underflow   : 1;
		mmr_t	iilb_fifo_overflow      : 1;
		mmr_t	iilb_credit_overflow    : 1;
		mmr_t	iilb_debit_overflow     : 1;
		mmr_t	xnpi_lut_error          : 1;
		mmr_t	xnpi_uce_error          : 1;
		mmr_t	xnpi_sbe_error          : 1;
		mmr_t	xnpi_credit_underflow   : 1;
		mmr_t	xnpi_data_buff_overflow : 1;
		mmr_t	xnpi_debit_overflow     : 1;
		mmr_t	xnpi_credit_overflow    : 1;
		mmr_t	xnmd_lut_error          : 1;
		mmr_t	xnmd_uce_error          : 1;
		mmr_t	xnmd_sbe_error          : 1;
		mmr_t	xnmd_credit_underflow   : 1;
		mmr_t	xnmd_data_buff_overflow : 1;
		mmr_t	xnmd_debit_overflow     : 1;
		mmr_t	xnmd_credit_overflow    : 1;
		mmr_t	ni1_pipe_error          : 1;
		mmr_t	ni1_llp_error           : 1;
		mmr_t	ni1_credit_underflow    : 1;
		mmr_t	ni1_push_underflow      : 1;
		mmr_t	ni1_pop_underflow       : 1;
		mmr_t	ni1_debit_overflow      : 1;
		mmr_t	ni1_credit_overflow     : 1;
		mmr_t	ni1_push_overflow       : 1;
		mmr_t	ni1_pop_overflow        : 1;
		mmr_t	ni0_pipe_error          : 1;
		mmr_t	ni0_llp_error           : 1;
		mmr_t	ni0_credit_underflow    : 1;
		mmr_t	ni0_push_underflow      : 1;
		mmr_t	ni0_pop_underflow       : 1;
		mmr_t	ni0_debit_overflow      : 1;
		mmr_t	ni0_credit_overflow     : 1;
		mmr_t	ni0_push_overflow       : 1;
		mmr_t	ni0_pop_overflow        : 1;
	} sh_xn_first_error_s;
} sh_xn_first_error_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_XNIILB_ERROR_SUMMARY"                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xniilb_error_summary_u {
	mmr_t	sh_xniilb_error_summary_regval;
	struct {
		mmr_t	overflow_ii_debit0            : 1;
		mmr_t	overflow_ii_debit2            : 1;
		mmr_t	overflow_lb_debit0            : 1;
		mmr_t	overflow_lb_debit2            : 1;
		mmr_t	overflow_ii_vc0               : 1;
		mmr_t	overflow_ii_vc2               : 1;
		mmr_t	underflow_ii_vc0              : 1;
		mmr_t	underflow_ii_vc2              : 1;
		mmr_t	overflow_lb_vc0               : 1;
		mmr_t	overflow_lb_vc2               : 1;
		mmr_t	underflow_lb_vc0              : 1;
		mmr_t	underflow_lb_vc2              : 1;
		mmr_t	overflow_pi_vc0_credit_in     : 1;
		mmr_t	overflow_iilb_vc0_credit_in   : 1;
		mmr_t	overflow_md_vc0_credit_in     : 1;
		mmr_t	overflow_ni0_vc0_credit_in    : 1;
		mmr_t	overflow_ni1_vc0_credit_in    : 1;
		mmr_t	overflow_pi_vc2_credit_in     : 1;
		mmr_t	overflow_iilb_vc2_credit_in   : 1;
		mmr_t	overflow_md_vc2_credit_in     : 1;
		mmr_t	overflow_ni0_vc2_credit_in    : 1;
		mmr_t	overflow_ni1_vc2_credit_in    : 1;
		mmr_t	underflow_pi_vc0_credit_in    : 1;
		mmr_t	underflow_iilb_vc0_credit_in  : 1;
		mmr_t	underflow_md_vc0_credit_in    : 1;
		mmr_t	underflow_ni0_vc0_credit_in   : 1;
		mmr_t	underflow_ni1_vc0_credit_in   : 1;
		mmr_t	underflow_pi_vc2_credit_in    : 1;
		mmr_t	underflow_iilb_vc2_credit_in  : 1;
		mmr_t	underflow_md_vc2_credit_in    : 1;
		mmr_t	underflow_ni0_vc2_credit_in   : 1;
		mmr_t	underflow_ni1_vc2_credit_in   : 1;
		mmr_t	overflow_pi_debit0            : 1;
		mmr_t	overflow_pi_debit2            : 1;
		mmr_t	overflow_iilb_debit0          : 1;
		mmr_t	overflow_iilb_debit2          : 1;
		mmr_t	overflow_md_debit0            : 1;
		mmr_t	overflow_md_debit2            : 1;
		mmr_t	overflow_ni0_debit0           : 1;
		mmr_t	overflow_ni0_debit2           : 1;
		mmr_t	overflow_ni1_debit0           : 1;
		mmr_t	overflow_ni1_debit2           : 1;
		mmr_t	overflow_pi_vc0_credit_out    : 1;
		mmr_t	overflow_pi_vc2_credit_out    : 1;
		mmr_t	overflow_md_vc0_credit_out    : 1;
		mmr_t	overflow_md_vc2_credit_out    : 1;
		mmr_t	overflow_iilb_vc0_credit_out  : 1;
		mmr_t	overflow_iilb_vc2_credit_out  : 1;
		mmr_t	overflow_ni0_vc0_credit_out   : 1;
		mmr_t	overflow_ni0_vc2_credit_out   : 1;
		mmr_t	overflow_ni1_vc0_credit_out   : 1;
		mmr_t	overflow_ni1_vc2_credit_out   : 1;
		mmr_t	underflow_pi_vc0_credit_out   : 1;
		mmr_t	underflow_pi_vc2_credit_out   : 1;
		mmr_t	underflow_md_vc0_credit_out   : 1;
		mmr_t	underflow_md_vc2_credit_out   : 1;
		mmr_t	underflow_iilb_vc0_credit_out : 1;
		mmr_t	underflow_iilb_vc2_credit_out : 1;
		mmr_t	underflow_ni0_vc0_credit_out  : 1;
		mmr_t	underflow_ni0_vc2_credit_out  : 1;
		mmr_t	underflow_ni1_vc0_credit_out  : 1;
		mmr_t	underflow_ni1_vc2_credit_out  : 1;
		mmr_t	chiplet_nomatch               : 1;
		mmr_t	lut_read_error                : 1;
	} sh_xniilb_error_summary_s;
} sh_xniilb_error_summary_u_t;
#else
typedef union sh_xniilb_error_summary_u {
	mmr_t	sh_xniilb_error_summary_regval;
	struct {
		mmr_t	lut_read_error                : 1;
		mmr_t	chiplet_nomatch               : 1;
		mmr_t	underflow_ni1_vc2_credit_out  : 1;
		mmr_t	underflow_ni1_vc0_credit_out  : 1;
		mmr_t	underflow_ni0_vc2_credit_out  : 1;
		mmr_t	underflow_ni0_vc0_credit_out  : 1;
		mmr_t	underflow_iilb_vc2_credit_out : 1;
		mmr_t	underflow_iilb_vc0_credit_out : 1;
		mmr_t	underflow_md_vc2_credit_out   : 1;
		mmr_t	underflow_md_vc0_credit_out   : 1;
		mmr_t	underflow_pi_vc2_credit_out   : 1;
		mmr_t	underflow_pi_vc0_credit_out   : 1;
		mmr_t	overflow_ni1_vc2_credit_out   : 1;
		mmr_t	overflow_ni1_vc0_credit_out   : 1;
		mmr_t	overflow_ni0_vc2_credit_out   : 1;
		mmr_t	overflow_ni0_vc0_credit_out   : 1;
		mmr_t	overflow_iilb_vc2_credit_out  : 1;
		mmr_t	overflow_iilb_vc0_credit_out  : 1;
		mmr_t	overflow_md_vc2_credit_out    : 1;
		mmr_t	overflow_md_vc0_credit_out    : 1;
		mmr_t	overflow_pi_vc2_credit_out    : 1;
		mmr_t	overflow_pi_vc0_credit_out    : 1;
		mmr_t	overflow_ni1_debit2           : 1;
		mmr_t	overflow_ni1_debit0           : 1;
		mmr_t	overflow_ni0_debit2           : 1;
		mmr_t	overflow_ni0_debit0           : 1;
		mmr_t	overflow_md_debit2            : 1;
		mmr_t	overflow_md_debit0            : 1;
		mmr_t	overflow_iilb_debit2          : 1;
		mmr_t	overflow_iilb_debit0          : 1;
		mmr_t	overflow_pi_debit2            : 1;
		mmr_t	overflow_pi_debit0            : 1;
		mmr_t	underflow_ni1_vc2_credit_in   : 1;
		mmr_t	underflow_ni0_vc2_credit_in   : 1;
		mmr_t	underflow_md_vc2_credit_in    : 1;
		mmr_t	underflow_iilb_vc2_credit_in  : 1;
		mmr_t	underflow_pi_vc2_credit_in    : 1;
		mmr_t	underflow_ni1_vc0_credit_in   : 1;
		mmr_t	underflow_ni0_vc0_credit_in   : 1;
		mmr_t	underflow_md_vc0_credit_in    : 1;
		mmr_t	underflow_iilb_vc0_credit_in  : 1;
		mmr_t	underflow_pi_vc0_credit_in    : 1;
		mmr_t	overflow_ni1_vc2_credit_in    : 1;
		mmr_t	overflow_ni0_vc2_credit_in    : 1;
		mmr_t	overflow_md_vc2_credit_in     : 1;
		mmr_t	overflow_iilb_vc2_credit_in   : 1;
		mmr_t	overflow_pi_vc2_credit_in     : 1;
		mmr_t	overflow_ni1_vc0_credit_in    : 1;
		mmr_t	overflow_ni0_vc0_credit_in    : 1;
		mmr_t	overflow_md_vc0_credit_in     : 1;
		mmr_t	overflow_iilb_vc0_credit_in   : 1;
		mmr_t	overflow_pi_vc0_credit_in     : 1;
		mmr_t	underflow_lb_vc2              : 1;
		mmr_t	underflow_lb_vc0              : 1;
		mmr_t	overflow_lb_vc2               : 1;
		mmr_t	overflow_lb_vc0               : 1;
		mmr_t	underflow_ii_vc2              : 1;
		mmr_t	underflow_ii_vc0              : 1;
		mmr_t	overflow_ii_vc2               : 1;
		mmr_t	overflow_ii_vc0               : 1;
		mmr_t	overflow_lb_debit2            : 1;
		mmr_t	overflow_lb_debit0            : 1;
		mmr_t	overflow_ii_debit2            : 1;
		mmr_t	overflow_ii_debit0            : 1;
	} sh_xniilb_error_summary_s;
} sh_xniilb_error_summary_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_XNIILB_ERROR_OVERFLOW"                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xniilb_error_overflow_u {
	mmr_t	sh_xniilb_error_overflow_regval;
	struct {
		mmr_t	overflow_ii_debit0            : 1;
		mmr_t	overflow_ii_debit2            : 1;
		mmr_t	overflow_lb_debit0            : 1;
		mmr_t	overflow_lb_debit2            : 1;
		mmr_t	overflow_ii_vc0               : 1;
		mmr_t	overflow_ii_vc2               : 1;
		mmr_t	underflow_ii_vc0              : 1;
		mmr_t	underflow_ii_vc2              : 1;
		mmr_t	overflow_lb_vc0               : 1;
		mmr_t	overflow_lb_vc2               : 1;
		mmr_t	underflow_lb_vc0              : 1;
		mmr_t	underflow_lb_vc2              : 1;
		mmr_t	overflow_pi_vc0_credit_in     : 1;
		mmr_t	overflow_iilb_vc0_credit_in   : 1;
		mmr_t	overflow_md_vc0_credit_in     : 1;
		mmr_t	overflow_ni0_vc0_credit_in    : 1;
		mmr_t	overflow_ni1_vc0_credit_in    : 1;
		mmr_t	overflow_pi_vc2_credit_in     : 1;
		mmr_t	overflow_iilb_vc2_credit_in   : 1;
		mmr_t	overflow_md_vc2_credit_in     : 1;
		mmr_t	overflow_ni0_vc2_credit_in    : 1;
		mmr_t	overflow_ni1_vc2_credit_in    : 1;
		mmr_t	underflow_pi_vc0_credit_in    : 1;
		mmr_t	underflow_iilb_vc0_credit_in  : 1;
		mmr_t	underflow_md_vc0_credit_in    : 1;
		mmr_t	underflow_ni0_vc0_credit_in   : 1;
		mmr_t	underflow_ni1_vc0_credit_in   : 1;
		mmr_t	underflow_pi_vc2_credit_in    : 1;
		mmr_t	underflow_iilb_vc2_credit_in  : 1;
		mmr_t	underflow_md_vc2_credit_in    : 1;
		mmr_t	underflow_ni0_vc2_credit_in   : 1;
		mmr_t	underflow_ni1_vc2_credit_in   : 1;
		mmr_t	overflow_pi_debit0            : 1;
		mmr_t	overflow_pi_debit2            : 1;
		mmr_t	overflow_iilb_debit0          : 1;
		mmr_t	overflow_iilb_debit2          : 1;
		mmr_t	overflow_md_debit0            : 1;
		mmr_t	overflow_md_debit2            : 1;
		mmr_t	overflow_ni0_debit0           : 1;
		mmr_t	overflow_ni0_debit2           : 1;
		mmr_t	overflow_ni1_debit0           : 1;
		mmr_t	overflow_ni1_debit2           : 1;
		mmr_t	overflow_pi_vc0_credit_out    : 1;
		mmr_t	overflow_pi_vc2_credit_out    : 1;
		mmr_t	overflow_md_vc0_credit_out    : 1;
		mmr_t	overflow_md_vc2_credit_out    : 1;
		mmr_t	overflow_iilb_vc0_credit_out  : 1;
		mmr_t	overflow_iilb_vc2_credit_out  : 1;
		mmr_t	overflow_ni0_vc0_credit_out   : 1;
		mmr_t	overflow_ni0_vc2_credit_out   : 1;
		mmr_t	overflow_ni1_vc0_credit_out   : 1;
		mmr_t	overflow_ni1_vc2_credit_out   : 1;
		mmr_t	underflow_pi_vc0_credit_out   : 1;
		mmr_t	underflow_pi_vc2_credit_out   : 1;
		mmr_t	underflow_md_vc0_credit_out   : 1;
		mmr_t	underflow_md_vc2_credit_out   : 1;
		mmr_t	underflow_iilb_vc0_credit_out : 1;
		mmr_t	underflow_iilb_vc2_credit_out : 1;
		mmr_t	underflow_ni0_vc0_credit_out  : 1;
		mmr_t	underflow_ni0_vc2_credit_out  : 1;
		mmr_t	underflow_ni1_vc0_credit_out  : 1;
		mmr_t	underflow_ni1_vc2_credit_out  : 1;
		mmr_t	chiplet_nomatch               : 1;
		mmr_t	lut_read_error                : 1;
	} sh_xniilb_error_overflow_s;
} sh_xniilb_error_overflow_u_t;
#else
typedef union sh_xniilb_error_overflow_u {
	mmr_t	sh_xniilb_error_overflow_regval;
	struct {
		mmr_t	lut_read_error                : 1;
		mmr_t	chiplet_nomatch               : 1;
		mmr_t	underflow_ni1_vc2_credit_out  : 1;
		mmr_t	underflow_ni1_vc0_credit_out  : 1;
		mmr_t	underflow_ni0_vc2_credit_out  : 1;
		mmr_t	underflow_ni0_vc0_credit_out  : 1;
		mmr_t	underflow_iilb_vc2_credit_out : 1;
		mmr_t	underflow_iilb_vc0_credit_out : 1;
		mmr_t	underflow_md_vc2_credit_out   : 1;
		mmr_t	underflow_md_vc0_credit_out   : 1;
		mmr_t	underflow_pi_vc2_credit_out   : 1;
		mmr_t	underflow_pi_vc0_credit_out   : 1;
		mmr_t	overflow_ni1_vc2_credit_out   : 1;
		mmr_t	overflow_ni1_vc0_credit_out   : 1;
		mmr_t	overflow_ni0_vc2_credit_out   : 1;
		mmr_t	overflow_ni0_vc0_credit_out   : 1;
		mmr_t	overflow_iilb_vc2_credit_out  : 1;
		mmr_t	overflow_iilb_vc0_credit_out  : 1;
		mmr_t	overflow_md_vc2_credit_out    : 1;
		mmr_t	overflow_md_vc0_credit_out    : 1;
		mmr_t	overflow_pi_vc2_credit_out    : 1;
		mmr_t	overflow_pi_vc0_credit_out    : 1;
		mmr_t	overflow_ni1_debit2           : 1;
		mmr_t	overflow_ni1_debit0           : 1;
		mmr_t	overflow_ni0_debit2           : 1;
		mmr_t	overflow_ni0_debit0           : 1;
		mmr_t	overflow_md_debit2            : 1;
		mmr_t	overflow_md_debit0            : 1;
		mmr_t	overflow_iilb_debit2          : 1;
		mmr_t	overflow_iilb_debit0          : 1;
		mmr_t	overflow_pi_debit2            : 1;
		mmr_t	overflow_pi_debit0            : 1;
		mmr_t	underflow_ni1_vc2_credit_in   : 1;
		mmr_t	underflow_ni0_vc2_credit_in   : 1;
		mmr_t	underflow_md_vc2_credit_in    : 1;
		mmr_t	underflow_iilb_vc2_credit_in  : 1;
		mmr_t	underflow_pi_vc2_credit_in    : 1;
		mmr_t	underflow_ni1_vc0_credit_in   : 1;
		mmr_t	underflow_ni0_vc0_credit_in   : 1;
		mmr_t	underflow_md_vc0_credit_in    : 1;
		mmr_t	underflow_iilb_vc0_credit_in  : 1;
		mmr_t	underflow_pi_vc0_credit_in    : 1;
		mmr_t	overflow_ni1_vc2_credit_in    : 1;
		mmr_t	overflow_ni0_vc2_credit_in    : 1;
		mmr_t	overflow_md_vc2_credit_in     : 1;
		mmr_t	overflow_iilb_vc2_credit_in   : 1;
		mmr_t	overflow_pi_vc2_credit_in     : 1;
		mmr_t	overflow_ni1_vc0_credit_in    : 1;
		mmr_t	overflow_ni0_vc0_credit_in    : 1;
		mmr_t	overflow_md_vc0_credit_in     : 1;
		mmr_t	overflow_iilb_vc0_credit_in   : 1;
		mmr_t	overflow_pi_vc0_credit_in     : 1;
		mmr_t	underflow_lb_vc2              : 1;
		mmr_t	underflow_lb_vc0              : 1;
		mmr_t	overflow_lb_vc2               : 1;
		mmr_t	overflow_lb_vc0               : 1;
		mmr_t	underflow_ii_vc2              : 1;
		mmr_t	underflow_ii_vc0              : 1;
		mmr_t	overflow_ii_vc2               : 1;
		mmr_t	overflow_ii_vc0               : 1;
		mmr_t	overflow_lb_debit2            : 1;
		mmr_t	overflow_lb_debit0            : 1;
		mmr_t	overflow_ii_debit2            : 1;
		mmr_t	overflow_ii_debit0            : 1;
	} sh_xniilb_error_overflow_s;
} sh_xniilb_error_overflow_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_XNIILB_ERROR_MASK"                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xniilb_error_mask_u {
	mmr_t	sh_xniilb_error_mask_regval;
	struct {
		mmr_t	overflow_ii_debit0            : 1;
		mmr_t	overflow_ii_debit2            : 1;
		mmr_t	overflow_lb_debit0            : 1;
		mmr_t	overflow_lb_debit2            : 1;
		mmr_t	overflow_ii_vc0               : 1;
		mmr_t	overflow_ii_vc2               : 1;
		mmr_t	underflow_ii_vc0              : 1;
		mmr_t	underflow_ii_vc2              : 1;
		mmr_t	overflow_lb_vc0               : 1;
		mmr_t	overflow_lb_vc2               : 1;
		mmr_t	underflow_lb_vc0              : 1;
		mmr_t	underflow_lb_vc2              : 1;
		mmr_t	overflow_pi_vc0_credit_in     : 1;
		mmr_t	overflow_iilb_vc0_credit_in   : 1;
		mmr_t	overflow_md_vc0_credit_in     : 1;
		mmr_t	overflow_ni0_vc0_credit_in    : 1;
		mmr_t	overflow_ni1_vc0_credit_in    : 1;
		mmr_t	overflow_pi_vc2_credit_in     : 1;
		mmr_t	overflow_iilb_vc2_credit_in   : 1;
		mmr_t	overflow_md_vc2_credit_in     : 1;
		mmr_t	overflow_ni0_vc2_credit_in    : 1;
		mmr_t	overflow_ni1_vc2_credit_in    : 1;
		mmr_t	underflow_pi_vc0_credit_in    : 1;
		mmr_t	underflow_iilb_vc0_credit_in  : 1;
		mmr_t	underflow_md_vc0_credit_in    : 1;
		mmr_t	underflow_ni0_vc0_credit_in   : 1;
		mmr_t	underflow_ni1_vc0_credit_in   : 1;
		mmr_t	underflow_pi_vc2_credit_in    : 1;
		mmr_t	underflow_iilb_vc2_credit_in  : 1;
		mmr_t	underflow_md_vc2_credit_in    : 1;
		mmr_t	underflow_ni0_vc2_credit_in   : 1;
		mmr_t	underflow_ni1_vc2_credit_in   : 1;
		mmr_t	overflow_pi_debit0            : 1;
		mmr_t	overflow_pi_debit2            : 1;
		mmr_t	overflow_iilb_debit0          : 1;
		mmr_t	overflow_iilb_debit2          : 1;
		mmr_t	overflow_md_debit0            : 1;
		mmr_t	overflow_md_debit2            : 1;
		mmr_t	overflow_ni0_debit0           : 1;
		mmr_t	overflow_ni0_debit2           : 1;
		mmr_t	overflow_ni1_debit0           : 1;
		mmr_t	overflow_ni1_debit2           : 1;
		mmr_t	overflow_pi_vc0_credit_out    : 1;
		mmr_t	overflow_pi_vc2_credit_out    : 1;
		mmr_t	overflow_md_vc0_credit_out    : 1;
		mmr_t	overflow_md_vc2_credit_out    : 1;
		mmr_t	overflow_iilb_vc0_credit_out  : 1;
		mmr_t	overflow_iilb_vc2_credit_out  : 1;
		mmr_t	overflow_ni0_vc0_credit_out   : 1;
		mmr_t	overflow_ni0_vc2_credit_out   : 1;
		mmr_t	overflow_ni1_vc0_credit_out   : 1;
		mmr_t	overflow_ni1_vc2_credit_out   : 1;
		mmr_t	underflow_pi_vc0_credit_out   : 1;
		mmr_t	underflow_pi_vc2_credit_out   : 1;
		mmr_t	underflow_md_vc0_credit_out   : 1;
		mmr_t	underflow_md_vc2_credit_out   : 1;
		mmr_t	underflow_iilb_vc0_credit_out : 1;
		mmr_t	underflow_iilb_vc2_credit_out : 1;
		mmr_t	underflow_ni0_vc0_credit_out  : 1;
		mmr_t	underflow_ni0_vc2_credit_out  : 1;
		mmr_t	underflow_ni1_vc0_credit_out  : 1;
		mmr_t	underflow_ni1_vc2_credit_out  : 1;
		mmr_t	chiplet_nomatch               : 1;
		mmr_t	lut_read_error                : 1;
	} sh_xniilb_error_mask_s;
} sh_xniilb_error_mask_u_t;
#else
typedef union sh_xniilb_error_mask_u {
	mmr_t	sh_xniilb_error_mask_regval;
	struct {
		mmr_t	lut_read_error                : 1;
		mmr_t	chiplet_nomatch               : 1;
		mmr_t	underflow_ni1_vc2_credit_out  : 1;
		mmr_t	underflow_ni1_vc0_credit_out  : 1;
		mmr_t	underflow_ni0_vc2_credit_out  : 1;
		mmr_t	underflow_ni0_vc0_credit_out  : 1;
		mmr_t	underflow_iilb_vc2_credit_out : 1;
		mmr_t	underflow_iilb_vc0_credit_out : 1;
		mmr_t	underflow_md_vc2_credit_out   : 1;
		mmr_t	underflow_md_vc0_credit_out   : 1;
		mmr_t	underflow_pi_vc2_credit_out   : 1;
		mmr_t	underflow_pi_vc0_credit_out   : 1;
		mmr_t	overflow_ni1_vc2_credit_out   : 1;
		mmr_t	overflow_ni1_vc0_credit_out   : 1;
		mmr_t	overflow_ni0_vc2_credit_out   : 1;
		mmr_t	overflow_ni0_vc0_credit_out   : 1;
		mmr_t	overflow_iilb_vc2_credit_out  : 1;
		mmr_t	overflow_iilb_vc0_credit_out  : 1;
		mmr_t	overflow_md_vc2_credit_out    : 1;
		mmr_t	overflow_md_vc0_credit_out    : 1;
		mmr_t	overflow_pi_vc2_credit_out    : 1;
		mmr_t	overflow_pi_vc0_credit_out    : 1;
		mmr_t	overflow_ni1_debit2           : 1;
		mmr_t	overflow_ni1_debit0           : 1;
		mmr_t	overflow_ni0_debit2           : 1;
		mmr_t	overflow_ni0_debit0           : 1;
		mmr_t	overflow_md_debit2            : 1;
		mmr_t	overflow_md_debit0            : 1;
		mmr_t	overflow_iilb_debit2          : 1;
		mmr_t	overflow_iilb_debit0          : 1;
		mmr_t	overflow_pi_debit2            : 1;
		mmr_t	overflow_pi_debit0            : 1;
		mmr_t	underflow_ni1_vc2_credit_in   : 1;
		mmr_t	underflow_ni0_vc2_credit_in   : 1;
		mmr_t	underflow_md_vc2_credit_in    : 1;
		mmr_t	underflow_iilb_vc2_credit_in  : 1;
		mmr_t	underflow_pi_vc2_credit_in    : 1;
		mmr_t	underflow_ni1_vc0_credit_in   : 1;
		mmr_t	underflow_ni0_vc0_credit_in   : 1;
		mmr_t	underflow_md_vc0_credit_in    : 1;
		mmr_t	underflow_iilb_vc0_credit_in  : 1;
		mmr_t	underflow_pi_vc0_credit_in    : 1;
		mmr_t	overflow_ni1_vc2_credit_in    : 1;
		mmr_t	overflow_ni0_vc2_credit_in    : 1;
		mmr_t	overflow_md_vc2_credit_in     : 1;
		mmr_t	overflow_iilb_vc2_credit_in   : 1;
		mmr_t	overflow_pi_vc2_credit_in     : 1;
		mmr_t	overflow_ni1_vc0_credit_in    : 1;
		mmr_t	overflow_ni0_vc0_credit_in    : 1;
		mmr_t	overflow_md_vc0_credit_in     : 1;
		mmr_t	overflow_iilb_vc0_credit_in   : 1;
		mmr_t	overflow_pi_vc0_credit_in     : 1;
		mmr_t	underflow_lb_vc2              : 1;
		mmr_t	underflow_lb_vc0              : 1;
		mmr_t	overflow_lb_vc2               : 1;
		mmr_t	overflow_lb_vc0               : 1;
		mmr_t	underflow_ii_vc2              : 1;
		mmr_t	underflow_ii_vc0              : 1;
		mmr_t	overflow_ii_vc2               : 1;
		mmr_t	overflow_ii_vc0               : 1;
		mmr_t	overflow_lb_debit2            : 1;
		mmr_t	overflow_lb_debit0            : 1;
		mmr_t	overflow_ii_debit2            : 1;
		mmr_t	overflow_ii_debit0            : 1;
	} sh_xniilb_error_mask_s;
} sh_xniilb_error_mask_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_XNIILB_FIRST_ERROR"                   */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xniilb_first_error_u {
	mmr_t	sh_xniilb_first_error_regval;
	struct {
		mmr_t	overflow_ii_debit0            : 1;
		mmr_t	overflow_ii_debit2            : 1;
		mmr_t	overflow_lb_debit0            : 1;
		mmr_t	overflow_lb_debit2            : 1;
		mmr_t	overflow_ii_vc0               : 1;
		mmr_t	overflow_ii_vc2               : 1;
		mmr_t	underflow_ii_vc0              : 1;
		mmr_t	underflow_ii_vc2              : 1;
		mmr_t	overflow_lb_vc0               : 1;
		mmr_t	overflow_lb_vc2               : 1;
		mmr_t	underflow_lb_vc0              : 1;
		mmr_t	underflow_lb_vc2              : 1;
		mmr_t	overflow_pi_vc0_credit_in     : 1;
		mmr_t	overflow_iilb_vc0_credit_in   : 1;
		mmr_t	overflow_md_vc0_credit_in     : 1;
		mmr_t	overflow_ni0_vc0_credit_in    : 1;
		mmr_t	overflow_ni1_vc0_credit_in    : 1;
		mmr_t	overflow_pi_vc2_credit_in     : 1;
		mmr_t	overflow_iilb_vc2_credit_in   : 1;
		mmr_t	overflow_md_vc2_credit_in     : 1;
		mmr_t	overflow_ni0_vc2_credit_in    : 1;
		mmr_t	overflow_ni1_vc2_credit_in    : 1;
		mmr_t	underflow_pi_vc0_credit_in    : 1;
		mmr_t	underflow_iilb_vc0_credit_in  : 1;
		mmr_t	underflow_md_vc0_credit_in    : 1;
		mmr_t	underflow_ni0_vc0_credit_in   : 1;
		mmr_t	underflow_ni1_vc0_credit_in   : 1;
		mmr_t	underflow_pi_vc2_credit_in    : 1;
		mmr_t	underflow_iilb_vc2_credit_in  : 1;
		mmr_t	underflow_md_vc2_credit_in    : 1;
		mmr_t	underflow_ni0_vc2_credit_in   : 1;
		mmr_t	underflow_ni1_vc2_credit_in   : 1;
		mmr_t	overflow_pi_debit0            : 1;
		mmr_t	overflow_pi_debit2            : 1;
		mmr_t	overflow_iilb_debit0          : 1;
		mmr_t	overflow_iilb_debit2          : 1;
		mmr_t	overflow_md_debit0            : 1;
		mmr_t	overflow_md_debit2            : 1;
		mmr_t	overflow_ni0_debit0           : 1;
		mmr_t	overflow_ni0_debit2           : 1;
		mmr_t	overflow_ni1_debit0           : 1;
		mmr_t	overflow_ni1_debit2           : 1;
		mmr_t	overflow_pi_vc0_credit_out    : 1;
		mmr_t	overflow_pi_vc2_credit_out    : 1;
		mmr_t	overflow_md_vc0_credit_out    : 1;
		mmr_t	overflow_md_vc2_credit_out    : 1;
		mmr_t	overflow_iilb_vc0_credit_out  : 1;
		mmr_t	overflow_iilb_vc2_credit_out  : 1;
		mmr_t	overflow_ni0_vc0_credit_out   : 1;
		mmr_t	overflow_ni0_vc2_credit_out   : 1;
		mmr_t	overflow_ni1_vc0_credit_out   : 1;
		mmr_t	overflow_ni1_vc2_credit_out   : 1;
		mmr_t	underflow_pi_vc0_credit_out   : 1;
		mmr_t	underflow_pi_vc2_credit_out   : 1;
		mmr_t	underflow_md_vc0_credit_out   : 1;
		mmr_t	underflow_md_vc2_credit_out   : 1;
		mmr_t	underflow_iilb_vc0_credit_out : 1;
		mmr_t	underflow_iilb_vc2_credit_out : 1;
		mmr_t	underflow_ni0_vc0_credit_out  : 1;
		mmr_t	underflow_ni0_vc2_credit_out  : 1;
		mmr_t	underflow_ni1_vc0_credit_out  : 1;
		mmr_t	underflow_ni1_vc2_credit_out  : 1;
		mmr_t	chiplet_nomatch               : 1;
		mmr_t	lut_read_error                : 1;
	} sh_xniilb_first_error_s;
} sh_xniilb_first_error_u_t;
#else
typedef union sh_xniilb_first_error_u {
	mmr_t	sh_xniilb_first_error_regval;
	struct {
		mmr_t	lut_read_error                : 1;
		mmr_t	chiplet_nomatch               : 1;
		mmr_t	underflow_ni1_vc2_credit_out  : 1;
		mmr_t	underflow_ni1_vc0_credit_out  : 1;
		mmr_t	underflow_ni0_vc2_credit_out  : 1;
		mmr_t	underflow_ni0_vc0_credit_out  : 1;
		mmr_t	underflow_iilb_vc2_credit_out : 1;
		mmr_t	underflow_iilb_vc0_credit_out : 1;
		mmr_t	underflow_md_vc2_credit_out   : 1;
		mmr_t	underflow_md_vc0_credit_out   : 1;
		mmr_t	underflow_pi_vc2_credit_out   : 1;
		mmr_t	underflow_pi_vc0_credit_out   : 1;
		mmr_t	overflow_ni1_vc2_credit_out   : 1;
		mmr_t	overflow_ni1_vc0_credit_out   : 1;
		mmr_t	overflow_ni0_vc2_credit_out   : 1;
		mmr_t	overflow_ni0_vc0_credit_out   : 1;
		mmr_t	overflow_iilb_vc2_credit_out  : 1;
		mmr_t	overflow_iilb_vc0_credit_out  : 1;
		mmr_t	overflow_md_vc2_credit_out    : 1;
		mmr_t	overflow_md_vc0_credit_out    : 1;
		mmr_t	overflow_pi_vc2_credit_out    : 1;
		mmr_t	overflow_pi_vc0_credit_out    : 1;
		mmr_t	overflow_ni1_debit2           : 1;
		mmr_t	overflow_ni1_debit0           : 1;
		mmr_t	overflow_ni0_debit2           : 1;
		mmr_t	overflow_ni0_debit0           : 1;
		mmr_t	overflow_md_debit2            : 1;
		mmr_t	overflow_md_debit0            : 1;
		mmr_t	overflow_iilb_debit2          : 1;
		mmr_t	overflow_iilb_debit0          : 1;
		mmr_t	overflow_pi_debit2            : 1;
		mmr_t	overflow_pi_debit0            : 1;
		mmr_t	underflow_ni1_vc2_credit_in   : 1;
		mmr_t	underflow_ni0_vc2_credit_in   : 1;
		mmr_t	underflow_md_vc2_credit_in    : 1;
		mmr_t	underflow_iilb_vc2_credit_in  : 1;
		mmr_t	underflow_pi_vc2_credit_in    : 1;
		mmr_t	underflow_ni1_vc0_credit_in   : 1;
		mmr_t	underflow_ni0_vc0_credit_in   : 1;
		mmr_t	underflow_md_vc0_credit_in    : 1;
		mmr_t	underflow_iilb_vc0_credit_in  : 1;
		mmr_t	underflow_pi_vc0_credit_in    : 1;
		mmr_t	overflow_ni1_vc2_credit_in    : 1;
		mmr_t	overflow_ni0_vc2_credit_in    : 1;
		mmr_t	overflow_md_vc2_credit_in     : 1;
		mmr_t	overflow_iilb_vc2_credit_in   : 1;
		mmr_t	overflow_pi_vc2_credit_in     : 1;
		mmr_t	overflow_ni1_vc0_credit_in    : 1;
		mmr_t	overflow_ni0_vc0_credit_in    : 1;
		mmr_t	overflow_md_vc0_credit_in     : 1;
		mmr_t	overflow_iilb_vc0_credit_in   : 1;
		mmr_t	overflow_pi_vc0_credit_in     : 1;
		mmr_t	underflow_lb_vc2              : 1;
		mmr_t	underflow_lb_vc0              : 1;
		mmr_t	overflow_lb_vc2               : 1;
		mmr_t	overflow_lb_vc0               : 1;
		mmr_t	underflow_ii_vc2              : 1;
		mmr_t	underflow_ii_vc0              : 1;
		mmr_t	overflow_ii_vc2               : 1;
		mmr_t	overflow_ii_vc0               : 1;
		mmr_t	overflow_lb_debit2            : 1;
		mmr_t	overflow_lb_debit0            : 1;
		mmr_t	overflow_ii_debit2            : 1;
		mmr_t	overflow_ii_debit0            : 1;
	} sh_xniilb_first_error_s;
} sh_xniilb_first_error_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_XNPI_ERROR_SUMMARY"                   */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnpi_error_summary_u {
	mmr_t	sh_xnpi_error_summary_regval;
	struct {
		mmr_t	underflow_ni0_vc0           : 1;
		mmr_t	overflow_ni0_vc0            : 1;
		mmr_t	underflow_ni0_vc2           : 1;
		mmr_t	overflow_ni0_vc2            : 1;
		mmr_t	underflow_ni1_vc0           : 1;
		mmr_t	overflow_ni1_vc0            : 1;
		mmr_t	underflow_ni1_vc2           : 1;
		mmr_t	overflow_ni1_vc2            : 1;
		mmr_t	underflow_iilb_vc0          : 1;
		mmr_t	overflow_iilb_vc0           : 1;
		mmr_t	underflow_iilb_vc2          : 1;
		mmr_t	overflow_iilb_vc2           : 1;
		mmr_t	underflow_vc0_credit        : 1;
		mmr_t	overflow_vc0_credit         : 1;
		mmr_t	underflow_vc2_credit        : 1;
		mmr_t	overflow_vc2_credit         : 1;
		mmr_t	overflow_databuff_vc0       : 1;
		mmr_t	overflow_databuff_vc2       : 1;
		mmr_t	lut_read_error              : 1;
		mmr_t	single_bit_error0           : 1;
		mmr_t	single_bit_error1           : 1;
		mmr_t	single_bit_error2           : 1;
		mmr_t	single_bit_error3           : 1;
		mmr_t	uncor_error0                : 1;
		mmr_t	uncor_error1                : 1;
		mmr_t	uncor_error2                : 1;
		mmr_t	uncor_error3                : 1;
		mmr_t	underflow_sic_cntr0         : 1;
		mmr_t	overflow_sic_cntr0          : 1;
		mmr_t	underflow_sic_cntr2         : 1;
		mmr_t	overflow_sic_cntr2          : 1;
		mmr_t	overflow_ni0_debit0         : 1;
		mmr_t	overflow_ni0_debit2         : 1;
		mmr_t	overflow_ni1_debit0         : 1;
		mmr_t	overflow_ni1_debit2         : 1;
		mmr_t	overflow_iilb_debit0        : 1;
		mmr_t	overflow_iilb_debit2        : 1;
		mmr_t	underflow_ni0_vc0_credit    : 1;
		mmr_t	overflow_ni0_vc0_credit     : 1;
		mmr_t	underflow_ni0_vc2_credit    : 1;
		mmr_t	overflow_ni0_vc2_credit     : 1;
		mmr_t	underflow_ni1_vc0_credit    : 1;
		mmr_t	overflow_ni1_vc0_credit     : 1;
		mmr_t	underflow_ni1_vc2_credit    : 1;
		mmr_t	overflow_ni1_vc2_credit     : 1;
		mmr_t	underflow_iilb_vc0_credit   : 1;
		mmr_t	overflow_iilb_vc0_credit    : 1;
		mmr_t	underflow_iilb_vc2_credit   : 1;
		mmr_t	overflow_iilb_vc2_credit    : 1;
		mmr_t	overflow_header_cancel_fifo : 1;
		mmr_t	reserved_0                  : 14;
	} sh_xnpi_error_summary_s;
} sh_xnpi_error_summary_u_t;
#else
typedef union sh_xnpi_error_summary_u {
	mmr_t	sh_xnpi_error_summary_regval;
	struct {
		mmr_t	reserved_0                  : 14;
		mmr_t	overflow_header_cancel_fifo : 1;
		mmr_t	overflow_iilb_vc2_credit    : 1;
		mmr_t	underflow_iilb_vc2_credit   : 1;
		mmr_t	overflow_iilb_vc0_credit    : 1;
		mmr_t	underflow_iilb_vc0_credit   : 1;
		mmr_t	overflow_ni1_vc2_credit     : 1;
		mmr_t	underflow_ni1_vc2_credit    : 1;
		mmr_t	overflow_ni1_vc0_credit     : 1;
		mmr_t	underflow_ni1_vc0_credit    : 1;
		mmr_t	overflow_ni0_vc2_credit     : 1;
		mmr_t	underflow_ni0_vc2_credit    : 1;
		mmr_t	overflow_ni0_vc0_credit     : 1;
		mmr_t	underflow_ni0_vc0_credit    : 1;
		mmr_t	overflow_iilb_debit2        : 1;
		mmr_t	overflow_iilb_debit0        : 1;
		mmr_t	overflow_ni1_debit2         : 1;
		mmr_t	overflow_ni1_debit0         : 1;
		mmr_t	overflow_ni0_debit2         : 1;
		mmr_t	overflow_ni0_debit0         : 1;
		mmr_t	overflow_sic_cntr2          : 1;
		mmr_t	underflow_sic_cntr2         : 1;
		mmr_t	overflow_sic_cntr0          : 1;
		mmr_t	underflow_sic_cntr0         : 1;
		mmr_t	uncor_error3                : 1;
		mmr_t	uncor_error2                : 1;
		mmr_t	uncor_error1                : 1;
		mmr_t	uncor_error0                : 1;
		mmr_t	single_bit_error3           : 1;
		mmr_t	single_bit_error2           : 1;
		mmr_t	single_bit_error1           : 1;
		mmr_t	single_bit_error0           : 1;
		mmr_t	lut_read_error              : 1;
		mmr_t	overflow_databuff_vc2       : 1;
		mmr_t	overflow_databuff_vc0       : 1;
		mmr_t	overflow_vc2_credit         : 1;
		mmr_t	underflow_vc2_credit        : 1;
		mmr_t	overflow_vc0_credit         : 1;
		mmr_t	underflow_vc0_credit        : 1;
		mmr_t	overflow_iilb_vc2           : 1;
		mmr_t	underflow_iilb_vc2          : 1;
		mmr_t	overflow_iilb_vc0           : 1;
		mmr_t	underflow_iilb_vc0          : 1;
		mmr_t	overflow_ni1_vc2            : 1;
		mmr_t	underflow_ni1_vc2           : 1;
		mmr_t	overflow_ni1_vc0            : 1;
		mmr_t	underflow_ni1_vc0           : 1;
		mmr_t	overflow_ni0_vc2            : 1;
		mmr_t	underflow_ni0_vc2           : 1;
		mmr_t	overflow_ni0_vc0            : 1;
		mmr_t	underflow_ni0_vc0           : 1;
	} sh_xnpi_error_summary_s;
} sh_xnpi_error_summary_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_XNPI_ERROR_OVERFLOW"                   */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnpi_error_overflow_u {
	mmr_t	sh_xnpi_error_overflow_regval;
	struct {
		mmr_t	underflow_ni0_vc0           : 1;
		mmr_t	overflow_ni0_vc0            : 1;
		mmr_t	underflow_ni0_vc2           : 1;
		mmr_t	overflow_ni0_vc2            : 1;
		mmr_t	underflow_ni1_vc0           : 1;
		mmr_t	overflow_ni1_vc0            : 1;
		mmr_t	underflow_ni1_vc2           : 1;
		mmr_t	overflow_ni1_vc2            : 1;
		mmr_t	underflow_iilb_vc0          : 1;
		mmr_t	overflow_iilb_vc0           : 1;
		mmr_t	underflow_iilb_vc2          : 1;
		mmr_t	overflow_iilb_vc2           : 1;
		mmr_t	underflow_vc0_credit        : 1;
		mmr_t	overflow_vc0_credit         : 1;
		mmr_t	underflow_vc2_credit        : 1;
		mmr_t	overflow_vc2_credit         : 1;
		mmr_t	overflow_databuff_vc0       : 1;
		mmr_t	overflow_databuff_vc2       : 1;
		mmr_t	lut_read_error              : 1;
		mmr_t	single_bit_error0           : 1;
		mmr_t	single_bit_error1           : 1;
		mmr_t	single_bit_error2           : 1;
		mmr_t	single_bit_error3           : 1;
		mmr_t	uncor_error0                : 1;
		mmr_t	uncor_error1                : 1;
		mmr_t	uncor_error2                : 1;
		mmr_t	uncor_error3                : 1;
		mmr_t	underflow_sic_cntr0         : 1;
		mmr_t	overflow_sic_cntr0          : 1;
		mmr_t	underflow_sic_cntr2         : 1;
		mmr_t	overflow_sic_cntr2          : 1;
		mmr_t	overflow_ni0_debit0         : 1;
		mmr_t	overflow_ni0_debit2         : 1;
		mmr_t	overflow_ni1_debit0         : 1;
		mmr_t	overflow_ni1_debit2         : 1;
		mmr_t	overflow_iilb_debit0        : 1;
		mmr_t	overflow_iilb_debit2        : 1;
		mmr_t	underflow_ni0_vc0_credit    : 1;
		mmr_t	overflow_ni0_vc0_credit     : 1;
		mmr_t	underflow_ni0_vc2_credit    : 1;
		mmr_t	overflow_ni0_vc2_credit     : 1;
		mmr_t	underflow_ni1_vc0_credit    : 1;
		mmr_t	overflow_ni1_vc0_credit     : 1;
		mmr_t	underflow_ni1_vc2_credit    : 1;
		mmr_t	overflow_ni1_vc2_credit     : 1;
		mmr_t	underflow_iilb_vc0_credit   : 1;
		mmr_t	overflow_iilb_vc0_credit    : 1;
		mmr_t	underflow_iilb_vc2_credit   : 1;
		mmr_t	overflow_iilb_vc2_credit    : 1;
		mmr_t	overflow_header_cancel_fifo : 1;
		mmr_t	reserved_0                  : 14;
	} sh_xnpi_error_overflow_s;
} sh_xnpi_error_overflow_u_t;
#else
typedef union sh_xnpi_error_overflow_u {
	mmr_t	sh_xnpi_error_overflow_regval;
	struct {
		mmr_t	reserved_0                  : 14;
		mmr_t	overflow_header_cancel_fifo : 1;
		mmr_t	overflow_iilb_vc2_credit    : 1;
		mmr_t	underflow_iilb_vc2_credit   : 1;
		mmr_t	overflow_iilb_vc0_credit    : 1;
		mmr_t	underflow_iilb_vc0_credit   : 1;
		mmr_t	overflow_ni1_vc2_credit     : 1;
		mmr_t	underflow_ni1_vc2_credit    : 1;
		mmr_t	overflow_ni1_vc0_credit     : 1;
		mmr_t	underflow_ni1_vc0_credit    : 1;
		mmr_t	overflow_ni0_vc2_credit     : 1;
		mmr_t	underflow_ni0_vc2_credit    : 1;
		mmr_t	overflow_ni0_vc0_credit     : 1;
		mmr_t	underflow_ni0_vc0_credit    : 1;
		mmr_t	overflow_iilb_debit2        : 1;
		mmr_t	overflow_iilb_debit0        : 1;
		mmr_t	overflow_ni1_debit2         : 1;
		mmr_t	overflow_ni1_debit0         : 1;
		mmr_t	overflow_ni0_debit2         : 1;
		mmr_t	overflow_ni0_debit0         : 1;
		mmr_t	overflow_sic_cntr2          : 1;
		mmr_t	underflow_sic_cntr2         : 1;
		mmr_t	overflow_sic_cntr0          : 1;
		mmr_t	underflow_sic_cntr0         : 1;
		mmr_t	uncor_error3                : 1;
		mmr_t	uncor_error2                : 1;
		mmr_t	uncor_error1                : 1;
		mmr_t	uncor_error0                : 1;
		mmr_t	single_bit_error3           : 1;
		mmr_t	single_bit_error2           : 1;
		mmr_t	single_bit_error1           : 1;
		mmr_t	single_bit_error0           : 1;
		mmr_t	lut_read_error              : 1;
		mmr_t	overflow_databuff_vc2       : 1;
		mmr_t	overflow_databuff_vc0       : 1;
		mmr_t	overflow_vc2_credit         : 1;
		mmr_t	underflow_vc2_credit        : 1;
		mmr_t	overflow_vc0_credit         : 1;
		mmr_t	underflow_vc0_credit        : 1;
		mmr_t	overflow_iilb_vc2           : 1;
		mmr_t	underflow_iilb_vc2          : 1;
		mmr_t	overflow_iilb_vc0           : 1;
		mmr_t	underflow_iilb_vc0          : 1;
		mmr_t	overflow_ni1_vc2            : 1;
		mmr_t	underflow_ni1_vc2           : 1;
		mmr_t	overflow_ni1_vc0            : 1;
		mmr_t	underflow_ni1_vc0           : 1;
		mmr_t	overflow_ni0_vc2            : 1;
		mmr_t	underflow_ni0_vc2           : 1;
		mmr_t	overflow_ni0_vc0            : 1;
		mmr_t	underflow_ni0_vc0           : 1;
	} sh_xnpi_error_overflow_s;
} sh_xnpi_error_overflow_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_XNPI_ERROR_MASK"                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnpi_error_mask_u {
	mmr_t	sh_xnpi_error_mask_regval;
	struct {
		mmr_t	underflow_ni0_vc0           : 1;
		mmr_t	overflow_ni0_vc0            : 1;
		mmr_t	underflow_ni0_vc2           : 1;
		mmr_t	overflow_ni0_vc2            : 1;
		mmr_t	underflow_ni1_vc0           : 1;
		mmr_t	overflow_ni1_vc0            : 1;
		mmr_t	underflow_ni1_vc2           : 1;
		mmr_t	overflow_ni1_vc2            : 1;
		mmr_t	underflow_iilb_vc0          : 1;
		mmr_t	overflow_iilb_vc0           : 1;
		mmr_t	underflow_iilb_vc2          : 1;
		mmr_t	overflow_iilb_vc2           : 1;
		mmr_t	underflow_vc0_credit        : 1;
		mmr_t	overflow_vc0_credit         : 1;
		mmr_t	underflow_vc2_credit        : 1;
		mmr_t	overflow_vc2_credit         : 1;
		mmr_t	overflow_databuff_vc0       : 1;
		mmr_t	overflow_databuff_vc2       : 1;
		mmr_t	lut_read_error              : 1;
		mmr_t	single_bit_error0           : 1;
		mmr_t	single_bit_error1           : 1;
		mmr_t	single_bit_error2           : 1;
		mmr_t	single_bit_error3           : 1;
		mmr_t	uncor_error0                : 1;
		mmr_t	uncor_error1                : 1;
		mmr_t	uncor_error2                : 1;
		mmr_t	uncor_error3                : 1;
		mmr_t	underflow_sic_cntr0         : 1;
		mmr_t	overflow_sic_cntr0          : 1;
		mmr_t	underflow_sic_cntr2         : 1;
		mmr_t	overflow_sic_cntr2          : 1;
		mmr_t	overflow_ni0_debit0         : 1;
		mmr_t	overflow_ni0_debit2         : 1;
		mmr_t	overflow_ni1_debit0         : 1;
		mmr_t	overflow_ni1_debit2         : 1;
		mmr_t	overflow_iilb_debit0        : 1;
		mmr_t	overflow_iilb_debit2        : 1;
		mmr_t	underflow_ni0_vc0_credit    : 1;
		mmr_t	overflow_ni0_vc0_credit     : 1;
		mmr_t	underflow_ni0_vc2_credit    : 1;
		mmr_t	overflow_ni0_vc2_credit     : 1;
		mmr_t	underflow_ni1_vc0_credit    : 1;
		mmr_t	overflow_ni1_vc0_credit     : 1;
		mmr_t	underflow_ni1_vc2_credit    : 1;
		mmr_t	overflow_ni1_vc2_credit     : 1;
		mmr_t	underflow_iilb_vc0_credit   : 1;
		mmr_t	overflow_iilb_vc0_credit    : 1;
		mmr_t	underflow_iilb_vc2_credit   : 1;
		mmr_t	overflow_iilb_vc2_credit    : 1;
		mmr_t	overflow_header_cancel_fifo : 1;
		mmr_t	reserved_0                  : 14;
	} sh_xnpi_error_mask_s;
} sh_xnpi_error_mask_u_t;
#else
typedef union sh_xnpi_error_mask_u {
	mmr_t	sh_xnpi_error_mask_regval;
	struct {
		mmr_t	reserved_0                  : 14;
		mmr_t	overflow_header_cancel_fifo : 1;
		mmr_t	overflow_iilb_vc2_credit    : 1;
		mmr_t	underflow_iilb_vc2_credit   : 1;
		mmr_t	overflow_iilb_vc0_credit    : 1;
		mmr_t	underflow_iilb_vc0_credit   : 1;
		mmr_t	overflow_ni1_vc2_credit     : 1;
		mmr_t	underflow_ni1_vc2_credit    : 1;
		mmr_t	overflow_ni1_vc0_credit     : 1;
		mmr_t	underflow_ni1_vc0_credit    : 1;
		mmr_t	overflow_ni0_vc2_credit     : 1;
		mmr_t	underflow_ni0_vc2_credit    : 1;
		mmr_t	overflow_ni0_vc0_credit     : 1;
		mmr_t	underflow_ni0_vc0_credit    : 1;
		mmr_t	overflow_iilb_debit2        : 1;
		mmr_t	overflow_iilb_debit0        : 1;
		mmr_t	overflow_ni1_debit2         : 1;
		mmr_t	overflow_ni1_debit0         : 1;
		mmr_t	overflow_ni0_debit2         : 1;
		mmr_t	overflow_ni0_debit0         : 1;
		mmr_t	overflow_sic_cntr2          : 1;
		mmr_t	underflow_sic_cntr2         : 1;
		mmr_t	overflow_sic_cntr0          : 1;
		mmr_t	underflow_sic_cntr0         : 1;
		mmr_t	uncor_error3                : 1;
		mmr_t	uncor_error2                : 1;
		mmr_t	uncor_error1                : 1;
		mmr_t	uncor_error0                : 1;
		mmr_t	single_bit_error3           : 1;
		mmr_t	single_bit_error2           : 1;
		mmr_t	single_bit_error1           : 1;
		mmr_t	single_bit_error0           : 1;
		mmr_t	lut_read_error              : 1;
		mmr_t	overflow_databuff_vc2       : 1;
		mmr_t	overflow_databuff_vc0       : 1;
		mmr_t	overflow_vc2_credit         : 1;
		mmr_t	underflow_vc2_credit        : 1;
		mmr_t	overflow_vc0_credit         : 1;
		mmr_t	underflow_vc0_credit        : 1;
		mmr_t	overflow_iilb_vc2           : 1;
		mmr_t	underflow_iilb_vc2          : 1;
		mmr_t	overflow_iilb_vc0           : 1;
		mmr_t	underflow_iilb_vc0          : 1;
		mmr_t	overflow_ni1_vc2            : 1;
		mmr_t	underflow_ni1_vc2           : 1;
		mmr_t	overflow_ni1_vc0            : 1;
		mmr_t	underflow_ni1_vc0           : 1;
		mmr_t	overflow_ni0_vc2            : 1;
		mmr_t	underflow_ni0_vc2           : 1;
		mmr_t	overflow_ni0_vc0            : 1;
		mmr_t	underflow_ni0_vc0           : 1;
	} sh_xnpi_error_mask_s;
} sh_xnpi_error_mask_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_XNPI_FIRST_ERROR"                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnpi_first_error_u {
	mmr_t	sh_xnpi_first_error_regval;
	struct {
		mmr_t	underflow_ni0_vc0           : 1;
		mmr_t	overflow_ni0_vc0            : 1;
		mmr_t	underflow_ni0_vc2           : 1;
		mmr_t	overflow_ni0_vc2            : 1;
		mmr_t	underflow_ni1_vc0           : 1;
		mmr_t	overflow_ni1_vc0            : 1;
		mmr_t	underflow_ni1_vc2           : 1;
		mmr_t	overflow_ni1_vc2            : 1;
		mmr_t	underflow_iilb_vc0          : 1;
		mmr_t	overflow_iilb_vc0           : 1;
		mmr_t	underflow_iilb_vc2          : 1;
		mmr_t	overflow_iilb_vc2           : 1;
		mmr_t	underflow_vc0_credit        : 1;
		mmr_t	overflow_vc0_credit         : 1;
		mmr_t	underflow_vc2_credit        : 1;
		mmr_t	overflow_vc2_credit         : 1;
		mmr_t	overflow_databuff_vc0       : 1;
		mmr_t	overflow_databuff_vc2       : 1;
		mmr_t	lut_read_error              : 1;
		mmr_t	single_bit_error0           : 1;
		mmr_t	single_bit_error1           : 1;
		mmr_t	single_bit_error2           : 1;
		mmr_t	single_bit_error3           : 1;
		mmr_t	uncor_error0                : 1;
		mmr_t	uncor_error1                : 1;
		mmr_t	uncor_error2                : 1;
		mmr_t	uncor_error3                : 1;
		mmr_t	underflow_sic_cntr0         : 1;
		mmr_t	overflow_sic_cntr0          : 1;
		mmr_t	underflow_sic_cntr2         : 1;
		mmr_t	overflow_sic_cntr2          : 1;
		mmr_t	overflow_ni0_debit0         : 1;
		mmr_t	overflow_ni0_debit2         : 1;
		mmr_t	overflow_ni1_debit0         : 1;
		mmr_t	overflow_ni1_debit2         : 1;
		mmr_t	overflow_iilb_debit0        : 1;
		mmr_t	overflow_iilb_debit2        : 1;
		mmr_t	underflow_ni0_vc0_credit    : 1;
		mmr_t	overflow_ni0_vc0_credit     : 1;
		mmr_t	underflow_ni0_vc2_credit    : 1;
		mmr_t	overflow_ni0_vc2_credit     : 1;
		mmr_t	underflow_ni1_vc0_credit    : 1;
		mmr_t	overflow_ni1_vc0_credit     : 1;
		mmr_t	underflow_ni1_vc2_credit    : 1;
		mmr_t	overflow_ni1_vc2_credit     : 1;
		mmr_t	underflow_iilb_vc0_credit   : 1;
		mmr_t	overflow_iilb_vc0_credit    : 1;
		mmr_t	underflow_iilb_vc2_credit   : 1;
		mmr_t	overflow_iilb_vc2_credit    : 1;
		mmr_t	overflow_header_cancel_fifo : 1;
		mmr_t	reserved_0                  : 14;
	} sh_xnpi_first_error_s;
} sh_xnpi_first_error_u_t;
#else
typedef union sh_xnpi_first_error_u {
	mmr_t	sh_xnpi_first_error_regval;
	struct {
		mmr_t	reserved_0                  : 14;
		mmr_t	overflow_header_cancel_fifo : 1;
		mmr_t	overflow_iilb_vc2_credit    : 1;
		mmr_t	underflow_iilb_vc2_credit   : 1;
		mmr_t	overflow_iilb_vc0_credit    : 1;
		mmr_t	underflow_iilb_vc0_credit   : 1;
		mmr_t	overflow_ni1_vc2_credit     : 1;
		mmr_t	underflow_ni1_vc2_credit    : 1;
		mmr_t	overflow_ni1_vc0_credit     : 1;
		mmr_t	underflow_ni1_vc0_credit    : 1;
		mmr_t	overflow_ni0_vc2_credit     : 1;
		mmr_t	underflow_ni0_vc2_credit    : 1;
		mmr_t	overflow_ni0_vc0_credit     : 1;
		mmr_t	underflow_ni0_vc0_credit    : 1;
		mmr_t	overflow_iilb_debit2        : 1;
		mmr_t	overflow_iilb_debit0        : 1;
		mmr_t	overflow_ni1_debit2         : 1;
		mmr_t	overflow_ni1_debit0         : 1;
		mmr_t	overflow_ni0_debit2         : 1;
		mmr_t	overflow_ni0_debit0         : 1;
		mmr_t	overflow_sic_cntr2          : 1;
		mmr_t	underflow_sic_cntr2         : 1;
		mmr_t	overflow_sic_cntr0          : 1;
		mmr_t	underflow_sic_cntr0         : 1;
		mmr_t	uncor_error3                : 1;
		mmr_t	uncor_error2                : 1;
		mmr_t	uncor_error1                : 1;
		mmr_t	uncor_error0                : 1;
		mmr_t	single_bit_error3           : 1;
		mmr_t	single_bit_error2           : 1;
		mmr_t	single_bit_error1           : 1;
		mmr_t	single_bit_error0           : 1;
		mmr_t	lut_read_error              : 1;
		mmr_t	overflow_databuff_vc2       : 1;
		mmr_t	overflow_databuff_vc0       : 1;
		mmr_t	overflow_vc2_credit         : 1;
		mmr_t	underflow_vc2_credit        : 1;
		mmr_t	overflow_vc0_credit         : 1;
		mmr_t	underflow_vc0_credit        : 1;
		mmr_t	overflow_iilb_vc2           : 1;
		mmr_t	underflow_iilb_vc2          : 1;
		mmr_t	overflow_iilb_vc0           : 1;
		mmr_t	underflow_iilb_vc0          : 1;
		mmr_t	overflow_ni1_vc2            : 1;
		mmr_t	underflow_ni1_vc2           : 1;
		mmr_t	overflow_ni1_vc0            : 1;
		mmr_t	underflow_ni1_vc0           : 1;
		mmr_t	overflow_ni0_vc2            : 1;
		mmr_t	underflow_ni0_vc2           : 1;
		mmr_t	overflow_ni0_vc0            : 1;
		mmr_t	underflow_ni0_vc0           : 1;
	} sh_xnpi_first_error_s;
} sh_xnpi_first_error_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_XNMD_ERROR_SUMMARY"                   */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnmd_error_summary_u {
	mmr_t	sh_xnmd_error_summary_regval;
	struct {
		mmr_t	underflow_ni0_vc0           : 1;
		mmr_t	overflow_ni0_vc0            : 1;
		mmr_t	underflow_ni0_vc2           : 1;
		mmr_t	overflow_ni0_vc2            : 1;
		mmr_t	underflow_ni1_vc0           : 1;
		mmr_t	overflow_ni1_vc0            : 1;
		mmr_t	underflow_ni1_vc2           : 1;
		mmr_t	overflow_ni1_vc2            : 1;
		mmr_t	underflow_iilb_vc0          : 1;
		mmr_t	overflow_iilb_vc0           : 1;
		mmr_t	underflow_iilb_vc2          : 1;
		mmr_t	overflow_iilb_vc2           : 1;
		mmr_t	underflow_vc0_credit        : 1;
		mmr_t	overflow_vc0_credit         : 1;
		mmr_t	underflow_vc2_credit        : 1;
		mmr_t	overflow_vc2_credit         : 1;
		mmr_t	overflow_databuff_vc0       : 1;
		mmr_t	overflow_databuff_vc2       : 1;
		mmr_t	lut_read_error              : 1;
		mmr_t	single_bit_error0           : 1;
		mmr_t	single_bit_error1           : 1;
		mmr_t	single_bit_error2           : 1;
		mmr_t	single_bit_error3           : 1;
		mmr_t	uncor_error0                : 1;
		mmr_t	uncor_error1                : 1;
		mmr_t	uncor_error2                : 1;
		mmr_t	uncor_error3                : 1;
		mmr_t	underflow_sic_cntr0         : 1;
		mmr_t	overflow_sic_cntr0          : 1;
		mmr_t	underflow_sic_cntr2         : 1;
		mmr_t	overflow_sic_cntr2          : 1;
		mmr_t	overflow_ni0_debit0         : 1;
		mmr_t	overflow_ni0_debit2         : 1;
		mmr_t	overflow_ni1_debit0         : 1;
		mmr_t	overflow_ni1_debit2         : 1;
		mmr_t	overflow_iilb_debit0        : 1;
		mmr_t	overflow_iilb_debit2        : 1;
		mmr_t	underflow_ni0_vc0_credit    : 1;
		mmr_t	overflow_ni0_vc0_credit     : 1;
		mmr_t	underflow_ni0_vc2_credit    : 1;
		mmr_t	overflow_ni0_vc2_credit     : 1;
		mmr_t	underflow_ni1_vc0_credit    : 1;
		mmr_t	overflow_ni1_vc0_credit     : 1;
		mmr_t	underflow_ni1_vc2_credit    : 1;
		mmr_t	overflow_ni1_vc2_credit     : 1;
		mmr_t	underflow_iilb_vc0_credit   : 1;
		mmr_t	overflow_iilb_vc0_credit    : 1;
		mmr_t	underflow_iilb_vc2_credit   : 1;
		mmr_t	overflow_iilb_vc2_credit    : 1;
		mmr_t	overflow_header_cancel_fifo : 1;
		mmr_t	reserved_0                  : 14;
	} sh_xnmd_error_summary_s;
} sh_xnmd_error_summary_u_t;
#else
typedef union sh_xnmd_error_summary_u {
	mmr_t	sh_xnmd_error_summary_regval;
	struct {
		mmr_t	reserved_0                  : 14;
		mmr_t	overflow_header_cancel_fifo : 1;
		mmr_t	overflow_iilb_vc2_credit    : 1;
		mmr_t	underflow_iilb_vc2_credit   : 1;
		mmr_t	overflow_iilb_vc0_credit    : 1;
		mmr_t	underflow_iilb_vc0_credit   : 1;
		mmr_t	overflow_ni1_vc2_credit     : 1;
		mmr_t	underflow_ni1_vc2_credit    : 1;
		mmr_t	overflow_ni1_vc0_credit     : 1;
		mmr_t	underflow_ni1_vc0_credit    : 1;
		mmr_t	overflow_ni0_vc2_credit     : 1;
		mmr_t	underflow_ni0_vc2_credit    : 1;
		mmr_t	overflow_ni0_vc0_credit     : 1;
		mmr_t	underflow_ni0_vc0_credit    : 1;
		mmr_t	overflow_iilb_debit2        : 1;
		mmr_t	overflow_iilb_debit0        : 1;
		mmr_t	overflow_ni1_debit2         : 1;
		mmr_t	overflow_ni1_debit0         : 1;
		mmr_t	overflow_ni0_debit2         : 1;
		mmr_t	overflow_ni0_debit0         : 1;
		mmr_t	overflow_sic_cntr2          : 1;
		mmr_t	underflow_sic_cntr2         : 1;
		mmr_t	overflow_sic_cntr0          : 1;
		mmr_t	underflow_sic_cntr0         : 1;
		mmr_t	uncor_error3                : 1;
		mmr_t	uncor_error2                : 1;
		mmr_t	uncor_error1                : 1;
		mmr_t	uncor_error0                : 1;
		mmr_t	single_bit_error3           : 1;
		mmr_t	single_bit_error2           : 1;
		mmr_t	single_bit_error1           : 1;
		mmr_t	single_bit_error0           : 1;
		mmr_t	lut_read_error              : 1;
		mmr_t	overflow_databuff_vc2       : 1;
		mmr_t	overflow_databuff_vc0       : 1;
		mmr_t	overflow_vc2_credit         : 1;
		mmr_t	underflow_vc2_credit        : 1;
		mmr_t	overflow_vc0_credit         : 1;
		mmr_t	underflow_vc0_credit        : 1;
		mmr_t	overflow_iilb_vc2           : 1;
		mmr_t	underflow_iilb_vc2          : 1;
		mmr_t	overflow_iilb_vc0           : 1;
		mmr_t	underflow_iilb_vc0          : 1;
		mmr_t	overflow_ni1_vc2            : 1;
		mmr_t	underflow_ni1_vc2           : 1;
		mmr_t	overflow_ni1_vc0            : 1;
		mmr_t	underflow_ni1_vc0           : 1;
		mmr_t	overflow_ni0_vc2            : 1;
		mmr_t	underflow_ni0_vc2           : 1;
		mmr_t	overflow_ni0_vc0            : 1;
		mmr_t	underflow_ni0_vc0           : 1;
	} sh_xnmd_error_summary_s;
} sh_xnmd_error_summary_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_XNMD_ERROR_OVERFLOW"                   */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnmd_error_overflow_u {
	mmr_t	sh_xnmd_error_overflow_regval;
	struct {
		mmr_t	underflow_ni0_vc0           : 1;
		mmr_t	overflow_ni0_vc0            : 1;
		mmr_t	underflow_ni0_vc2           : 1;
		mmr_t	overflow_ni0_vc2            : 1;
		mmr_t	underflow_ni1_vc0           : 1;
		mmr_t	overflow_ni1_vc0            : 1;
		mmr_t	underflow_ni1_vc2           : 1;
		mmr_t	overflow_ni1_vc2            : 1;
		mmr_t	underflow_iilb_vc0          : 1;
		mmr_t	overflow_iilb_vc0           : 1;
		mmr_t	underflow_iilb_vc2          : 1;
		mmr_t	overflow_iilb_vc2           : 1;
		mmr_t	underflow_vc0_credit        : 1;
		mmr_t	overflow_vc0_credit         : 1;
		mmr_t	underflow_vc2_credit        : 1;
		mmr_t	overflow_vc2_credit         : 1;
		mmr_t	overflow_databuff_vc0       : 1;
		mmr_t	overflow_databuff_vc2       : 1;
		mmr_t	lut_read_error              : 1;
		mmr_t	single_bit_error0           : 1;
		mmr_t	single_bit_error1           : 1;
		mmr_t	single_bit_error2           : 1;
		mmr_t	single_bit_error3           : 1;
		mmr_t	uncor_error0                : 1;
		mmr_t	uncor_error1                : 1;
		mmr_t	uncor_error2                : 1;
		mmr_t	uncor_error3                : 1;
		mmr_t	underflow_sic_cntr0         : 1;
		mmr_t	overflow_sic_cntr0          : 1;
		mmr_t	underflow_sic_cntr2         : 1;
		mmr_t	overflow_sic_cntr2          : 1;
		mmr_t	overflow_ni0_debit0         : 1;
		mmr_t	overflow_ni0_debit2         : 1;
		mmr_t	overflow_ni1_debit0         : 1;
		mmr_t	overflow_ni1_debit2         : 1;
		mmr_t	overflow_iilb_debit0        : 1;
		mmr_t	overflow_iilb_debit2        : 1;
		mmr_t	underflow_ni0_vc0_credit    : 1;
		mmr_t	overflow_ni0_vc0_credit     : 1;
		mmr_t	underflow_ni0_vc2_credit    : 1;
		mmr_t	overflow_ni0_vc2_credit     : 1;
		mmr_t	underflow_ni1_vc0_credit    : 1;
		mmr_t	overflow_ni1_vc0_credit     : 1;
		mmr_t	underflow_ni1_vc2_credit    : 1;
		mmr_t	overflow_ni1_vc2_credit     : 1;
		mmr_t	underflow_iilb_vc0_credit   : 1;
		mmr_t	overflow_iilb_vc0_credit    : 1;
		mmr_t	underflow_iilb_vc2_credit   : 1;
		mmr_t	overflow_iilb_vc2_credit    : 1;
		mmr_t	overflow_header_cancel_fifo : 1;
		mmr_t	reserved_0                  : 14;
	} sh_xnmd_error_overflow_s;
} sh_xnmd_error_overflow_u_t;
#else
typedef union sh_xnmd_error_overflow_u {
	mmr_t	sh_xnmd_error_overflow_regval;
	struct {
		mmr_t	reserved_0                  : 14;
		mmr_t	overflow_header_cancel_fifo : 1;
		mmr_t	overflow_iilb_vc2_credit    : 1;
		mmr_t	underflow_iilb_vc2_credit   : 1;
		mmr_t	overflow_iilb_vc0_credit    : 1;
		mmr_t	underflow_iilb_vc0_credit   : 1;
		mmr_t	overflow_ni1_vc2_credit     : 1;
		mmr_t	underflow_ni1_vc2_credit    : 1;
		mmr_t	overflow_ni1_vc0_credit     : 1;
		mmr_t	underflow_ni1_vc0_credit    : 1;
		mmr_t	overflow_ni0_vc2_credit     : 1;
		mmr_t	underflow_ni0_vc2_credit    : 1;
		mmr_t	overflow_ni0_vc0_credit     : 1;
		mmr_t	underflow_ni0_vc0_credit    : 1;
		mmr_t	overflow_iilb_debit2        : 1;
		mmr_t	overflow_iilb_debit0        : 1;
		mmr_t	overflow_ni1_debit2         : 1;
		mmr_t	overflow_ni1_debit0         : 1;
		mmr_t	overflow_ni0_debit2         : 1;
		mmr_t	overflow_ni0_debit0         : 1;
		mmr_t	overflow_sic_cntr2          : 1;
		mmr_t	underflow_sic_cntr2         : 1;
		mmr_t	overflow_sic_cntr0          : 1;
		mmr_t	underflow_sic_cntr0         : 1;
		mmr_t	uncor_error3                : 1;
		mmr_t	uncor_error2                : 1;
		mmr_t	uncor_error1                : 1;
		mmr_t	uncor_error0                : 1;
		mmr_t	single_bit_error3           : 1;
		mmr_t	single_bit_error2           : 1;
		mmr_t	single_bit_error1           : 1;
		mmr_t	single_bit_error0           : 1;
		mmr_t	lut_read_error              : 1;
		mmr_t	overflow_databuff_vc2       : 1;
		mmr_t	overflow_databuff_vc0       : 1;
		mmr_t	overflow_vc2_credit         : 1;
		mmr_t	underflow_vc2_credit        : 1;
		mmr_t	overflow_vc0_credit         : 1;
		mmr_t	underflow_vc0_credit        : 1;
		mmr_t	overflow_iilb_vc2           : 1;
		mmr_t	underflow_iilb_vc2          : 1;
		mmr_t	overflow_iilb_vc0           : 1;
		mmr_t	underflow_iilb_vc0          : 1;
		mmr_t	overflow_ni1_vc2            : 1;
		mmr_t	underflow_ni1_vc2           : 1;
		mmr_t	overflow_ni1_vc0            : 1;
		mmr_t	underflow_ni1_vc0           : 1;
		mmr_t	overflow_ni0_vc2            : 1;
		mmr_t	underflow_ni0_vc2           : 1;
		mmr_t	overflow_ni0_vc0            : 1;
		mmr_t	underflow_ni0_vc0           : 1;
	} sh_xnmd_error_overflow_s;
} sh_xnmd_error_overflow_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_XNMD_ERROR_MASK"                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnmd_error_mask_u {
	mmr_t	sh_xnmd_error_mask_regval;
	struct {
		mmr_t	underflow_ni0_vc0           : 1;
		mmr_t	overflow_ni0_vc0            : 1;
		mmr_t	underflow_ni0_vc2           : 1;
		mmr_t	overflow_ni0_vc2            : 1;
		mmr_t	underflow_ni1_vc0           : 1;
		mmr_t	overflow_ni1_vc0            : 1;
		mmr_t	underflow_ni1_vc2           : 1;
		mmr_t	overflow_ni1_vc2            : 1;
		mmr_t	underflow_iilb_vc0          : 1;
		mmr_t	overflow_iilb_vc0           : 1;
		mmr_t	underflow_iilb_vc2          : 1;
		mmr_t	overflow_iilb_vc2           : 1;
		mmr_t	underflow_vc0_credit        : 1;
		mmr_t	overflow_vc0_credit         : 1;
		mmr_t	underflow_vc2_credit        : 1;
		mmr_t	overflow_vc2_credit         : 1;
		mmr_t	overflow_databuff_vc0       : 1;
		mmr_t	overflow_databuff_vc2       : 1;
		mmr_t	lut_read_error              : 1;
		mmr_t	single_bit_error0           : 1;
		mmr_t	single_bit_error1           : 1;
		mmr_t	single_bit_error2           : 1;
		mmr_t	single_bit_error3           : 1;
		mmr_t	uncor_error0                : 1;
		mmr_t	uncor_error1                : 1;
		mmr_t	uncor_error2                : 1;
		mmr_t	uncor_error3                : 1;
		mmr_t	underflow_sic_cntr0         : 1;
		mmr_t	overflow_sic_cntr0          : 1;
		mmr_t	underflow_sic_cntr2         : 1;
		mmr_t	overflow_sic_cntr2          : 1;
		mmr_t	overflow_ni0_debit0         : 1;
		mmr_t	overflow_ni0_debit2         : 1;
		mmr_t	overflow_ni1_debit0         : 1;
		mmr_t	overflow_ni1_debit2         : 1;
		mmr_t	overflow_iilb_debit0        : 1;
		mmr_t	overflow_iilb_debit2        : 1;
		mmr_t	underflow_ni0_vc0_credit    : 1;
		mmr_t	overflow_ni0_vc0_credit     : 1;
		mmr_t	underflow_ni0_vc2_credit    : 1;
		mmr_t	overflow_ni0_vc2_credit     : 1;
		mmr_t	underflow_ni1_vc0_credit    : 1;
		mmr_t	overflow_ni1_vc0_credit     : 1;
		mmr_t	underflow_ni1_vc2_credit    : 1;
		mmr_t	overflow_ni1_vc2_credit     : 1;
		mmr_t	underflow_iilb_vc0_credit   : 1;
		mmr_t	overflow_iilb_vc0_credit    : 1;
		mmr_t	underflow_iilb_vc2_credit   : 1;
		mmr_t	overflow_iilb_vc2_credit    : 1;
		mmr_t	overflow_header_cancel_fifo : 1;
		mmr_t	reserved_0                  : 14;
	} sh_xnmd_error_mask_s;
} sh_xnmd_error_mask_u_t;
#else
typedef union sh_xnmd_error_mask_u {
	mmr_t	sh_xnmd_error_mask_regval;
	struct {
		mmr_t	reserved_0                  : 14;
		mmr_t	overflow_header_cancel_fifo : 1;
		mmr_t	overflow_iilb_vc2_credit    : 1;
		mmr_t	underflow_iilb_vc2_credit   : 1;
		mmr_t	overflow_iilb_vc0_credit    : 1;
		mmr_t	underflow_iilb_vc0_credit   : 1;
		mmr_t	overflow_ni1_vc2_credit     : 1;
		mmr_t	underflow_ni1_vc2_credit    : 1;
		mmr_t	overflow_ni1_vc0_credit     : 1;
		mmr_t	underflow_ni1_vc0_credit    : 1;
		mmr_t	overflow_ni0_vc2_credit     : 1;
		mmr_t	underflow_ni0_vc2_credit    : 1;
		mmr_t	overflow_ni0_vc0_credit     : 1;
		mmr_t	underflow_ni0_vc0_credit    : 1;
		mmr_t	overflow_iilb_debit2        : 1;
		mmr_t	overflow_iilb_debit0        : 1;
		mmr_t	overflow_ni1_debit2         : 1;
		mmr_t	overflow_ni1_debit0         : 1;
		mmr_t	overflow_ni0_debit2         : 1;
		mmr_t	overflow_ni0_debit0         : 1;
		mmr_t	overflow_sic_cntr2          : 1;
		mmr_t	underflow_sic_cntr2         : 1;
		mmr_t	overflow_sic_cntr0          : 1;
		mmr_t	underflow_sic_cntr0         : 1;
		mmr_t	uncor_error3                : 1;
		mmr_t	uncor_error2                : 1;
		mmr_t	uncor_error1                : 1;
		mmr_t	uncor_error0                : 1;
		mmr_t	single_bit_error3           : 1;
		mmr_t	single_bit_error2           : 1;
		mmr_t	single_bit_error1           : 1;
		mmr_t	single_bit_error0           : 1;
		mmr_t	lut_read_error              : 1;
		mmr_t	overflow_databuff_vc2       : 1;
		mmr_t	overflow_databuff_vc0       : 1;
		mmr_t	overflow_vc2_credit         : 1;
		mmr_t	underflow_vc2_credit        : 1;
		mmr_t	overflow_vc0_credit         : 1;
		mmr_t	underflow_vc0_credit        : 1;
		mmr_t	overflow_iilb_vc2           : 1;
		mmr_t	underflow_iilb_vc2          : 1;
		mmr_t	overflow_iilb_vc0           : 1;
		mmr_t	underflow_iilb_vc0          : 1;
		mmr_t	overflow_ni1_vc2            : 1;
		mmr_t	underflow_ni1_vc2           : 1;
		mmr_t	overflow_ni1_vc0            : 1;
		mmr_t	underflow_ni1_vc0           : 1;
		mmr_t	overflow_ni0_vc2            : 1;
		mmr_t	underflow_ni0_vc2           : 1;
		mmr_t	overflow_ni0_vc0            : 1;
		mmr_t	underflow_ni0_vc0           : 1;
	} sh_xnmd_error_mask_s;
} sh_xnmd_error_mask_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_XNMD_FIRST_ERROR"                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xnmd_first_error_u {
	mmr_t	sh_xnmd_first_error_regval;
	struct {
		mmr_t	underflow_ni0_vc0           : 1;
		mmr_t	overflow_ni0_vc0            : 1;
		mmr_t	underflow_ni0_vc2           : 1;
		mmr_t	overflow_ni0_vc2            : 1;
		mmr_t	underflow_ni1_vc0           : 1;
		mmr_t	overflow_ni1_vc0            : 1;
		mmr_t	underflow_ni1_vc2           : 1;
		mmr_t	overflow_ni1_vc2            : 1;
		mmr_t	underflow_iilb_vc0          : 1;
		mmr_t	overflow_iilb_vc0           : 1;
		mmr_t	underflow_iilb_vc2          : 1;
		mmr_t	overflow_iilb_vc2           : 1;
		mmr_t	underflow_vc0_credit        : 1;
		mmr_t	overflow_vc0_credit         : 1;
		mmr_t	underflow_vc2_credit        : 1;
		mmr_t	overflow_vc2_credit         : 1;
		mmr_t	overflow_databuff_vc0       : 1;
		mmr_t	overflow_databuff_vc2       : 1;
		mmr_t	lut_read_error              : 1;
		mmr_t	single_bit_error0           : 1;
		mmr_t	single_bit_error1           : 1;
		mmr_t	single_bit_error2           : 1;
		mmr_t	single_bit_error3           : 1;
		mmr_t	uncor_error0                : 1;
		mmr_t	uncor_error1                : 1;
		mmr_t	uncor_error2                : 1;
		mmr_t	uncor_error3                : 1;
		mmr_t	underflow_sic_cntr0         : 1;
		mmr_t	overflow_sic_cntr0          : 1;
		mmr_t	underflow_sic_cntr2         : 1;
		mmr_t	overflow_sic_cntr2          : 1;
		mmr_t	overflow_ni0_debit0         : 1;
		mmr_t	overflow_ni0_debit2         : 1;
		mmr_t	overflow_ni1_debit0         : 1;
		mmr_t	overflow_ni1_debit2         : 1;
		mmr_t	overflow_iilb_debit0        : 1;
		mmr_t	overflow_iilb_debit2        : 1;
		mmr_t	underflow_ni0_vc0_credit    : 1;
		mmr_t	overflow_ni0_vc0_credit     : 1;
		mmr_t	underflow_ni0_vc2_credit    : 1;
		mmr_t	overflow_ni0_vc2_credit     : 1;
		mmr_t	underflow_ni1_vc0_credit    : 1;
		mmr_t	overflow_ni1_vc0_credit     : 1;
		mmr_t	underflow_ni1_vc2_credit    : 1;
		mmr_t	overflow_ni1_vc2_credit     : 1;
		mmr_t	underflow_iilb_vc0_credit   : 1;
		mmr_t	overflow_iilb_vc0_credit    : 1;
		mmr_t	underflow_iilb_vc2_credit   : 1;
		mmr_t	overflow_iilb_vc2_credit    : 1;
		mmr_t	overflow_header_cancel_fifo : 1;
		mmr_t	reserved_0                  : 14;
	} sh_xnmd_first_error_s;
} sh_xnmd_first_error_u_t;
#else
typedef union sh_xnmd_first_error_u {
	mmr_t	sh_xnmd_first_error_regval;
	struct {
		mmr_t	reserved_0                  : 14;
		mmr_t	overflow_header_cancel_fifo : 1;
		mmr_t	overflow_iilb_vc2_credit    : 1;
		mmr_t	underflow_iilb_vc2_credit   : 1;
		mmr_t	overflow_iilb_vc0_credit    : 1;
		mmr_t	underflow_iilb_vc0_credit   : 1;
		mmr_t	overflow_ni1_vc2_credit     : 1;
		mmr_t	underflow_ni1_vc2_credit    : 1;
		mmr_t	overflow_ni1_vc0_credit     : 1;
		mmr_t	underflow_ni1_vc0_credit    : 1;
		mmr_t	overflow_ni0_vc2_credit     : 1;
		mmr_t	underflow_ni0_vc2_credit    : 1;
		mmr_t	overflow_ni0_vc0_credit     : 1;
		mmr_t	underflow_ni0_vc0_credit    : 1;
		mmr_t	overflow_iilb_debit2        : 1;
		mmr_t	overflow_iilb_debit0        : 1;
		mmr_t	overflow_ni1_debit2         : 1;
		mmr_t	overflow_ni1_debit0         : 1;
		mmr_t	overflow_ni0_debit2         : 1;
		mmr_t	overflow_ni0_debit0         : 1;
		mmr_t	overflow_sic_cntr2          : 1;
		mmr_t	underflow_sic_cntr2         : 1;
		mmr_t	overflow_sic_cntr0          : 1;
		mmr_t	underflow_sic_cntr0         : 1;
		mmr_t	uncor_error3                : 1;
		mmr_t	uncor_error2                : 1;
		mmr_t	uncor_error1                : 1;
		mmr_t	uncor_error0                : 1;
		mmr_t	single_bit_error3           : 1;
		mmr_t	single_bit_error2           : 1;
		mmr_t	single_bit_error1           : 1;
		mmr_t	single_bit_error0           : 1;
		mmr_t	lut_read_error              : 1;
		mmr_t	overflow_databuff_vc2       : 1;
		mmr_t	overflow_databuff_vc0       : 1;
		mmr_t	overflow_vc2_credit         : 1;
		mmr_t	underflow_vc2_credit        : 1;
		mmr_t	overflow_vc0_credit         : 1;
		mmr_t	underflow_vc0_credit        : 1;
		mmr_t	overflow_iilb_vc2           : 1;
		mmr_t	underflow_iilb_vc2          : 1;
		mmr_t	overflow_iilb_vc0           : 1;
		mmr_t	underflow_iilb_vc0          : 1;
		mmr_t	overflow_ni1_vc2            : 1;
		mmr_t	underflow_ni1_vc2           : 1;
		mmr_t	overflow_ni1_vc0            : 1;
		mmr_t	underflow_ni1_vc0           : 1;
		mmr_t	overflow_ni0_vc2            : 1;
		mmr_t	underflow_ni0_vc2           : 1;
		mmr_t	overflow_ni0_vc0            : 1;
		mmr_t	underflow_ni0_vc0           : 1;
	} sh_xnmd_first_error_s;
} sh_xnmd_first_error_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_AUTO_REPLY_ENABLE0"                   */
/*                 Automatic Maintenance Reply Enable 0                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_auto_reply_enable0_u {
	mmr_t	sh_auto_reply_enable0_regval;
	struct {
		mmr_t	enable0     : 64;
	} sh_auto_reply_enable0_s;
} sh_auto_reply_enable0_u_t;
#else
typedef union sh_auto_reply_enable0_u {
	mmr_t	sh_auto_reply_enable0_regval;
	struct {
		mmr_t	enable0     : 64;
	} sh_auto_reply_enable0_s;
} sh_auto_reply_enable0_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_AUTO_REPLY_ENABLE1"                   */
/*                 Automatic Maintenance Reply Enable 1                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_auto_reply_enable1_u {
	mmr_t	sh_auto_reply_enable1_regval;
	struct {
		mmr_t	enable1     : 64;
	} sh_auto_reply_enable1_s;
} sh_auto_reply_enable1_u_t;
#else
typedef union sh_auto_reply_enable1_u {
	mmr_t	sh_auto_reply_enable1_regval;
	struct {
		mmr_t	enable1     : 64;
	} sh_auto_reply_enable1_s;
} sh_auto_reply_enable1_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_AUTO_REPLY_HEADER0"                   */
/*                 Automatic Maintenance Reply Header 0                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_auto_reply_header0_u {
	mmr_t	sh_auto_reply_header0_regval;
	struct {
		mmr_t	header0     : 64;
	} sh_auto_reply_header0_s;
} sh_auto_reply_header0_u_t;
#else
typedef union sh_auto_reply_header0_u {
	mmr_t	sh_auto_reply_header0_regval;
	struct {
		mmr_t	header0     : 64;
	} sh_auto_reply_header0_s;
} sh_auto_reply_header0_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_AUTO_REPLY_HEADER1"                   */
/*                 Automatic Maintenance Reply Header 1                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_auto_reply_header1_u {
	mmr_t	sh_auto_reply_header1_regval;
	struct {
		mmr_t	header1     : 64;
	} sh_auto_reply_header1_s;
} sh_auto_reply_header1_u_t;
#else
typedef union sh_auto_reply_header1_u {
	mmr_t	sh_auto_reply_header1_regval;
	struct {
		mmr_t	header1     : 64;
	} sh_auto_reply_header1_s;
} sh_auto_reply_header1_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_ENABLE_RP_AUTO_REPLY"                  */
/*         Enable Automatic Maintenance Reply From Reply Queue          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_enable_rp_auto_reply_u {
	mmr_t	sh_enable_rp_auto_reply_regval;
	struct {
		mmr_t	enable      : 1;
		mmr_t	reserved_0  : 63;
	} sh_enable_rp_auto_reply_s;
} sh_enable_rp_auto_reply_u_t;
#else
typedef union sh_enable_rp_auto_reply_u {
	mmr_t	sh_enable_rp_auto_reply_regval;
	struct {
		mmr_t	reserved_0  : 63;
		mmr_t	enable      : 1;
	} sh_enable_rp_auto_reply_s;
} sh_enable_rp_auto_reply_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_ENABLE_RQ_AUTO_REPLY"                  */
/*        Enable Automatic Maintenance Reply From Request Queue         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_enable_rq_auto_reply_u {
	mmr_t	sh_enable_rq_auto_reply_regval;
	struct {
		mmr_t	enable      : 1;
		mmr_t	reserved_0  : 63;
	} sh_enable_rq_auto_reply_s;
} sh_enable_rq_auto_reply_u_t;
#else
typedef union sh_enable_rq_auto_reply_u {
	mmr_t	sh_enable_rq_auto_reply_regval;
	struct {
		mmr_t	reserved_0  : 63;
		mmr_t	enable      : 1;
	} sh_enable_rq_auto_reply_s;
} sh_enable_rq_auto_reply_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_REDIRECT_INVAL"                     */
/*               Redirect invalidate to LB instead of PI                */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_redirect_inval_u {
	mmr_t	sh_redirect_inval_regval;
	struct {
		mmr_t	redirect    : 1;
		mmr_t	reserved_0  : 63;
	} sh_redirect_inval_s;
} sh_redirect_inval_u_t;
#else
typedef union sh_redirect_inval_u {
	mmr_t	sh_redirect_inval_regval;
	struct {
		mmr_t	reserved_0  : 63;
		mmr_t	redirect    : 1;
	} sh_redirect_inval_s;
} sh_redirect_inval_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_DIAG_MSG_CNTRL"                     */
/*                 Diagnostic Message Control Register                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_diag_msg_cntrl_u {
	mmr_t	sh_diag_msg_cntrl_regval;
	struct {
		mmr_t	msg_length          : 6;
		mmr_t	error_inject_point  : 6;
		mmr_t	error_inject_enable : 1;
		mmr_t	port                : 1;
		mmr_t	reserved_0          : 48;
		mmr_t	start               : 1;
		mmr_t	busy                : 1;
	} sh_diag_msg_cntrl_s;
} sh_diag_msg_cntrl_u_t;
#else
typedef union sh_diag_msg_cntrl_u {
	mmr_t	sh_diag_msg_cntrl_regval;
	struct {
		mmr_t	busy                : 1;
		mmr_t	start               : 1;
		mmr_t	reserved_0          : 48;
		mmr_t	port                : 1;
		mmr_t	error_inject_enable : 1;
		mmr_t	error_inject_point  : 6;
		mmr_t	msg_length          : 6;
	} sh_diag_msg_cntrl_s;
} sh_diag_msg_cntrl_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_DIAG_MSG_DATA0L"                     */
/*                    Diagnostic Data, lower 64 bits                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_diag_msg_data0l_u {
	mmr_t	sh_diag_msg_data0l_regval;
	struct {
		mmr_t	data_lower  : 64;
	} sh_diag_msg_data0l_s;
} sh_diag_msg_data0l_u_t;
#else
typedef union sh_diag_msg_data0l_u {
	mmr_t	sh_diag_msg_data0l_regval;
	struct {
		mmr_t	data_lower  : 64;
	} sh_diag_msg_data0l_s;
} sh_diag_msg_data0l_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_DIAG_MSG_DATA0U"                     */
/*                   Diagnostice Data, upper 64 bits                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_diag_msg_data0u_u {
	mmr_t	sh_diag_msg_data0u_regval;
	struct {
		mmr_t	data_upper  : 64;
	} sh_diag_msg_data0u_s;
} sh_diag_msg_data0u_u_t;
#else
typedef union sh_diag_msg_data0u_u {
	mmr_t	sh_diag_msg_data0u_regval;
	struct {
		mmr_t	data_upper  : 64;
	} sh_diag_msg_data0u_s;
} sh_diag_msg_data0u_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_DIAG_MSG_DATA1L"                     */
/*                    Diagnostic Data, lower 64 bits                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_diag_msg_data1l_u {
	mmr_t	sh_diag_msg_data1l_regval;
	struct {
		mmr_t	data_lower  : 64;
	} sh_diag_msg_data1l_s;
} sh_diag_msg_data1l_u_t;
#else
typedef union sh_diag_msg_data1l_u {
	mmr_t	sh_diag_msg_data1l_regval;
	struct {
		mmr_t	data_lower  : 64;
	} sh_diag_msg_data1l_s;
} sh_diag_msg_data1l_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_DIAG_MSG_DATA1U"                     */
/*                   Diagnostice Data, upper 64 bits                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_diag_msg_data1u_u {
	mmr_t	sh_diag_msg_data1u_regval;
	struct {
		mmr_t	data_upper  : 64;
	} sh_diag_msg_data1u_s;
} sh_diag_msg_data1u_u_t;
#else
typedef union sh_diag_msg_data1u_u {
	mmr_t	sh_diag_msg_data1u_regval;
	struct {
		mmr_t	data_upper  : 64;
	} sh_diag_msg_data1u_s;
} sh_diag_msg_data1u_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_DIAG_MSG_DATA2L"                     */
/*                    Diagnostic Data, lower 64 bits                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_diag_msg_data2l_u {
	mmr_t	sh_diag_msg_data2l_regval;
	struct {
		mmr_t	data_lower  : 64;
	} sh_diag_msg_data2l_s;
} sh_diag_msg_data2l_u_t;
#else
typedef union sh_diag_msg_data2l_u {
	mmr_t	sh_diag_msg_data2l_regval;
	struct {
		mmr_t	data_lower  : 64;
	} sh_diag_msg_data2l_s;
} sh_diag_msg_data2l_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_DIAG_MSG_DATA2U"                     */
/*                   Diagnostice Data, upper 64 bits                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_diag_msg_data2u_u {
	mmr_t	sh_diag_msg_data2u_regval;
	struct {
		mmr_t	data_upper  : 64;
	} sh_diag_msg_data2u_s;
} sh_diag_msg_data2u_u_t;
#else
typedef union sh_diag_msg_data2u_u {
	mmr_t	sh_diag_msg_data2u_regval;
	struct {
		mmr_t	data_upper  : 64;
	} sh_diag_msg_data2u_s;
} sh_diag_msg_data2u_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_DIAG_MSG_DATA3L"                     */
/*                    Diagnostic Data, lower 64 bits                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_diag_msg_data3l_u {
	mmr_t	sh_diag_msg_data3l_regval;
	struct {
		mmr_t	data_lower  : 64;
	} sh_diag_msg_data3l_s;
} sh_diag_msg_data3l_u_t;
#else
typedef union sh_diag_msg_data3l_u {
	mmr_t	sh_diag_msg_data3l_regval;
	struct {
		mmr_t	data_lower  : 64;
	} sh_diag_msg_data3l_s;
} sh_diag_msg_data3l_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_DIAG_MSG_DATA3U"                     */
/*                   Diagnostice Data, upper 64 bits                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_diag_msg_data3u_u {
	mmr_t	sh_diag_msg_data3u_regval;
	struct {
		mmr_t	data_upper  : 64;
	} sh_diag_msg_data3u_s;
} sh_diag_msg_data3u_u_t;
#else
typedef union sh_diag_msg_data3u_u {
	mmr_t	sh_diag_msg_data3u_regval;
	struct {
		mmr_t	data_upper  : 64;
	} sh_diag_msg_data3u_s;
} sh_diag_msg_data3u_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_DIAG_MSG_DATA4L"                     */
/*                    Diagnostic Data, lower 64 bits                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_diag_msg_data4l_u {
	mmr_t	sh_diag_msg_data4l_regval;
	struct {
		mmr_t	data_lower  : 64;
	} sh_diag_msg_data4l_s;
} sh_diag_msg_data4l_u_t;
#else
typedef union sh_diag_msg_data4l_u {
	mmr_t	sh_diag_msg_data4l_regval;
	struct {
		mmr_t	data_lower  : 64;
	} sh_diag_msg_data4l_s;
} sh_diag_msg_data4l_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_DIAG_MSG_DATA4U"                     */
/*                   Diagnostice Data, upper 64 bits                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_diag_msg_data4u_u {
	mmr_t	sh_diag_msg_data4u_regval;
	struct {
		mmr_t	data_upper  : 64;
	} sh_diag_msg_data4u_s;
} sh_diag_msg_data4u_u_t;
#else
typedef union sh_diag_msg_data4u_u {
	mmr_t	sh_diag_msg_data4u_regval;
	struct {
		mmr_t	data_upper  : 64;
	} sh_diag_msg_data4u_s;
} sh_diag_msg_data4u_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_DIAG_MSG_DATA5L"                     */
/*                    Diagnostic Data, lower 64 bits                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_diag_msg_data5l_u {
	mmr_t	sh_diag_msg_data5l_regval;
	struct {
		mmr_t	data_lower  : 64;
	} sh_diag_msg_data5l_s;
} sh_diag_msg_data5l_u_t;
#else
typedef union sh_diag_msg_data5l_u {
	mmr_t	sh_diag_msg_data5l_regval;
	struct {
		mmr_t	data_lower  : 64;
	} sh_diag_msg_data5l_s;
} sh_diag_msg_data5l_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_DIAG_MSG_DATA5U"                     */
/*                   Diagnostice Data, upper 64 bits                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_diag_msg_data5u_u {
	mmr_t	sh_diag_msg_data5u_regval;
	struct {
		mmr_t	data_upper  : 64;
	} sh_diag_msg_data5u_s;
} sh_diag_msg_data5u_u_t;
#else
typedef union sh_diag_msg_data5u_u {
	mmr_t	sh_diag_msg_data5u_regval;
	struct {
		mmr_t	data_upper  : 64;
	} sh_diag_msg_data5u_s;
} sh_diag_msg_data5u_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_DIAG_MSG_DATA6L"                     */
/*                    Diagnostic Data, lower 64 bits                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_diag_msg_data6l_u {
	mmr_t	sh_diag_msg_data6l_regval;
	struct {
		mmr_t	data_lower  : 64;
	} sh_diag_msg_data6l_s;
} sh_diag_msg_data6l_u_t;
#else
typedef union sh_diag_msg_data6l_u {
	mmr_t	sh_diag_msg_data6l_regval;
	struct {
		mmr_t	data_lower  : 64;
	} sh_diag_msg_data6l_s;
} sh_diag_msg_data6l_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_DIAG_MSG_DATA6U"                     */
/*                   Diagnostice Data, upper 64 bits                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_diag_msg_data6u_u {
	mmr_t	sh_diag_msg_data6u_regval;
	struct {
		mmr_t	data_upper  : 64;
	} sh_diag_msg_data6u_s;
} sh_diag_msg_data6u_u_t;
#else
typedef union sh_diag_msg_data6u_u {
	mmr_t	sh_diag_msg_data6u_regval;
	struct {
		mmr_t	data_upper  : 64;
	} sh_diag_msg_data6u_s;
} sh_diag_msg_data6u_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_DIAG_MSG_DATA7L"                     */
/*                    Diagnostic Data, lower 64 bits                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_diag_msg_data7l_u {
	mmr_t	sh_diag_msg_data7l_regval;
	struct {
		mmr_t	data_lower  : 64;
	} sh_diag_msg_data7l_s;
} sh_diag_msg_data7l_u_t;
#else
typedef union sh_diag_msg_data7l_u {
	mmr_t	sh_diag_msg_data7l_regval;
	struct {
		mmr_t	data_lower  : 64;
	} sh_diag_msg_data7l_s;
} sh_diag_msg_data7l_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_DIAG_MSG_DATA7U"                     */
/*                   Diagnostice Data, upper 64 bits                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_diag_msg_data7u_u {
	mmr_t	sh_diag_msg_data7u_regval;
	struct {
		mmr_t	data_upper  : 64;
	} sh_diag_msg_data7u_s;
} sh_diag_msg_data7u_u_t;
#else
typedef union sh_diag_msg_data7u_u {
	mmr_t	sh_diag_msg_data7u_regval;
	struct {
		mmr_t	data_upper  : 64;
	} sh_diag_msg_data7u_s;
} sh_diag_msg_data7u_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_DIAG_MSG_DATA8L"                     */
/*                    Diagnostic Data, lower 64 bits                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_diag_msg_data8l_u {
	mmr_t	sh_diag_msg_data8l_regval;
	struct {
		mmr_t	data_lower  : 64;
	} sh_diag_msg_data8l_s;
} sh_diag_msg_data8l_u_t;
#else
typedef union sh_diag_msg_data8l_u {
	mmr_t	sh_diag_msg_data8l_regval;
	struct {
		mmr_t	data_lower  : 64;
	} sh_diag_msg_data8l_s;
} sh_diag_msg_data8l_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_DIAG_MSG_DATA8U"                     */
/*                   Diagnostice Data, upper 64 bits                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_diag_msg_data8u_u {
	mmr_t	sh_diag_msg_data8u_regval;
	struct {
		mmr_t	data_upper  : 64;
	} sh_diag_msg_data8u_s;
} sh_diag_msg_data8u_u_t;
#else
typedef union sh_diag_msg_data8u_u {
	mmr_t	sh_diag_msg_data8u_regval;
	struct {
		mmr_t	data_upper  : 64;
	} sh_diag_msg_data8u_s;
} sh_diag_msg_data8u_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_DIAG_MSG_HDR0"                      */
/*              Diagnostice Data, lower 64 bits of header               */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_diag_msg_hdr0_u {
	mmr_t	sh_diag_msg_hdr0_regval;
	struct {
		mmr_t	header0     : 64;
	} sh_diag_msg_hdr0_s;
} sh_diag_msg_hdr0_u_t;
#else
typedef union sh_diag_msg_hdr0_u {
	mmr_t	sh_diag_msg_hdr0_regval;
	struct {
		mmr_t	header0     : 64;
	} sh_diag_msg_hdr0_s;
} sh_diag_msg_hdr0_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_DIAG_MSG_HDR1"                      */
/*              Diagnostice Data, upper 64 bits of header               */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_diag_msg_hdr1_u {
	mmr_t	sh_diag_msg_hdr1_regval;
	struct {
		mmr_t	header1     : 64;
	} sh_diag_msg_hdr1_s;
} sh_diag_msg_hdr1_u_t;
#else
typedef union sh_diag_msg_hdr1_u {
	mmr_t	sh_diag_msg_hdr1_regval;
	struct {
		mmr_t	header1     : 64;
	} sh_diag_msg_hdr1_s;
} sh_diag_msg_hdr1_u_t;
#endif

/* ==================================================================== */
/*                      Register "SH_DEBUG_SELECT"                      */
/*                        SHub Debug Port Select                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_debug_select_u {
	mmr_t	sh_debug_select_regval;
	struct {
		mmr_t	nibble0_nibble_sel  : 3;
		mmr_t	nibble0_chiplet_sel : 3;
		mmr_t	nibble1_nibble_sel  : 3;
		mmr_t	nibble1_chiplet_sel : 3;
		mmr_t	nibble2_nibble_sel  : 3;
		mmr_t	nibble2_chiplet_sel : 3;
		mmr_t	nibble3_nibble_sel  : 3;
		mmr_t	nibble3_chiplet_sel : 3;
		mmr_t	nibble4_nibble_sel  : 3;
		mmr_t	nibble4_chiplet_sel : 3;
		mmr_t	nibble5_nibble_sel  : 3;
		mmr_t	nibble5_chiplet_sel : 3;
		mmr_t	nibble6_nibble_sel  : 3;
		mmr_t	nibble6_chiplet_sel : 3;
		mmr_t	nibble7_nibble_sel  : 3;
		mmr_t	nibble7_chiplet_sel : 3;
		mmr_t	debug_ii_sel        : 3;
		mmr_t	sel_ii              : 9;
		mmr_t	reserved_0          : 3;
		mmr_t	trigger_enable      : 1;
	} sh_debug_select_s;
} sh_debug_select_u_t;
#else
typedef union sh_debug_select_u {
	mmr_t	sh_debug_select_regval;
	struct {
		mmr_t	trigger_enable      : 1;
		mmr_t	reserved_0          : 3;
		mmr_t	sel_ii              : 9;
		mmr_t	debug_ii_sel        : 3;
		mmr_t	nibble7_chiplet_sel : 3;
		mmr_t	nibble7_nibble_sel  : 3;
		mmr_t	nibble6_chiplet_sel : 3;
		mmr_t	nibble6_nibble_sel  : 3;
		mmr_t	nibble5_chiplet_sel : 3;
		mmr_t	nibble5_nibble_sel  : 3;
		mmr_t	nibble4_chiplet_sel : 3;
		mmr_t	nibble4_nibble_sel  : 3;
		mmr_t	nibble3_chiplet_sel : 3;
		mmr_t	nibble3_nibble_sel  : 3;
		mmr_t	nibble2_chiplet_sel : 3;
		mmr_t	nibble2_nibble_sel  : 3;
		mmr_t	nibble1_chiplet_sel : 3;
		mmr_t	nibble1_nibble_sel  : 3;
		mmr_t	nibble0_chiplet_sel : 3;
		mmr_t	nibble0_nibble_sel  : 3;
	} sh_debug_select_s;
} sh_debug_select_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_TRIGGER_COMPARE_MASK"                  */
/*                      SHub Trigger Compare Mask                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_trigger_compare_mask_u {
	mmr_t	sh_trigger_compare_mask_regval;
	struct {
		mmr_t	mask        : 32;
		mmr_t	reserved_0  : 32;
	} sh_trigger_compare_mask_s;
} sh_trigger_compare_mask_u_t;
#else
typedef union sh_trigger_compare_mask_u {
	mmr_t	sh_trigger_compare_mask_regval;
	struct {
		mmr_t	reserved_0  : 32;
		mmr_t	mask        : 32;
	} sh_trigger_compare_mask_s;
} sh_trigger_compare_mask_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_TRIGGER_COMPARE_PATTERN"                 */
/*                     SHub Trigger Compare Pattern                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_trigger_compare_pattern_u {
	mmr_t	sh_trigger_compare_pattern_regval;
	struct {
		mmr_t	data        : 32;
		mmr_t	reserved_0  : 32;
	} sh_trigger_compare_pattern_s;
} sh_trigger_compare_pattern_u_t;
#else
typedef union sh_trigger_compare_pattern_u {
	mmr_t	sh_trigger_compare_pattern_regval;
	struct {
		mmr_t	reserved_0  : 32;
		mmr_t	data        : 32;
	} sh_trigger_compare_pattern_s;
} sh_trigger_compare_pattern_u_t;
#endif

/* ==================================================================== */
/*                      Register "SH_TRIGGER_SEL"                       */
/*                  Trigger select for SHUB debug port                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_trigger_sel_u {
	mmr_t	sh_trigger_sel_regval;
	struct {
		mmr_t	nibble0_input_sel  : 3;
		mmr_t	reserved_0         : 1;
		mmr_t	nibble0_nibble_sel : 3;
		mmr_t	reserved_1         : 1;
		mmr_t	nibble1_input_sel  : 3;
		mmr_t	reserved_2         : 1;
		mmr_t	nibble1_nibble_sel : 3;
		mmr_t	reserved_3         : 1;
		mmr_t	nibble2_input_sel  : 3;
		mmr_t	reserved_4         : 1;
		mmr_t	nibble2_nibble_sel : 3;
		mmr_t	reserved_5         : 1;
		mmr_t	nibble3_input_sel  : 3;
		mmr_t	reserved_6         : 1;
		mmr_t	nibble3_nibble_sel : 3;
		mmr_t	reserved_7         : 1;
		mmr_t	nibble4_input_sel  : 3;
		mmr_t	reserved_8         : 1;
		mmr_t	nibble4_nibble_sel : 3;
		mmr_t	reserved_9         : 1;
		mmr_t	nibble5_input_sel  : 3;
		mmr_t	reserved_10        : 1;
		mmr_t	nibble5_nibble_sel : 3;
		mmr_t	reserved_11        : 1;
		mmr_t	nibble6_input_sel  : 3;
		mmr_t	reserved_12        : 1;
		mmr_t	nibble6_nibble_sel : 3;
		mmr_t	reserved_13        : 1;
		mmr_t	nibble7_input_sel  : 3;
		mmr_t	reserved_14        : 1;
		mmr_t	nibble7_nibble_sel : 3;
		mmr_t	reserved_15        : 1;
	} sh_trigger_sel_s;
} sh_trigger_sel_u_t;
#else
typedef union sh_trigger_sel_u {
	mmr_t	sh_trigger_sel_regval;
	struct {
		mmr_t	reserved_15        : 1;
		mmr_t	nibble7_nibble_sel : 3;
		mmr_t	reserved_14        : 1;
		mmr_t	nibble7_input_sel  : 3;
		mmr_t	reserved_13        : 1;
		mmr_t	nibble6_nibble_sel : 3;
		mmr_t	reserved_12        : 1;
		mmr_t	nibble6_input_sel  : 3;
		mmr_t	reserved_11        : 1;
		mmr_t	nibble5_nibble_sel : 3;
		mmr_t	reserved_10        : 1;
		mmr_t	nibble5_input_sel  : 3;
		mmr_t	reserved_9         : 1;
		mmr_t	nibble4_nibble_sel : 3;
		mmr_t	reserved_8         : 1;
		mmr_t	nibble4_input_sel  : 3;
		mmr_t	reserved_7         : 1;
		mmr_t	nibble3_nibble_sel : 3;
		mmr_t	reserved_6         : 1;
		mmr_t	nibble3_input_sel  : 3;
		mmr_t	reserved_5         : 1;
		mmr_t	nibble2_nibble_sel : 3;
		mmr_t	reserved_4         : 1;
		mmr_t	nibble2_input_sel  : 3;
		mmr_t	reserved_3         : 1;
		mmr_t	nibble1_nibble_sel : 3;
		mmr_t	reserved_2         : 1;
		mmr_t	nibble1_input_sel  : 3;
		mmr_t	reserved_1         : 1;
		mmr_t	nibble0_nibble_sel : 3;
		mmr_t	reserved_0         : 1;
		mmr_t	nibble0_input_sel  : 3;
	} sh_trigger_sel_s;
} sh_trigger_sel_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_STOP_CLK_CONTROL"                    */
/*                          Stop Clock Control                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_stop_clk_control_u {
	mmr_t	sh_stop_clk_control_regval;
	struct {
		mmr_t	stimulus    : 5;
		mmr_t	event       : 1;
		mmr_t	polarity    : 1;
		mmr_t	mode        : 1;
		mmr_t	reserved_0  : 56;
	} sh_stop_clk_control_s;
} sh_stop_clk_control_u_t;
#else
typedef union sh_stop_clk_control_u {
	mmr_t	sh_stop_clk_control_regval;
	struct {
		mmr_t	reserved_0  : 56;
		mmr_t	mode        : 1;
		mmr_t	polarity    : 1;
		mmr_t	event       : 1;
		mmr_t	stimulus    : 5;
	} sh_stop_clk_control_s;
} sh_stop_clk_control_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_STOP_CLK_DELAY_PHASE"                  */
/*                        Stop Clock Delay Phase                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_stop_clk_delay_phase_u {
	mmr_t	sh_stop_clk_delay_phase_regval;
	struct {
		mmr_t	delay       : 8;
		mmr_t	reserved_0  : 56;
	} sh_stop_clk_delay_phase_s;
} sh_stop_clk_delay_phase_u_t;
#else
typedef union sh_stop_clk_delay_phase_u {
	mmr_t	sh_stop_clk_delay_phase_regval;
	struct {
		mmr_t	reserved_0  : 56;
		mmr_t	delay       : 8;
	} sh_stop_clk_delay_phase_s;
} sh_stop_clk_delay_phase_u_t;
#endif

/* ==================================================================== */
/*                      Register "SH_TSF_ARM_MASK"                      */
/*                 Trigger sequencing facility arm mask                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_tsf_arm_mask_u {
	mmr_t	sh_tsf_arm_mask_regval;
	struct {
		mmr_t	mask        : 64;
	} sh_tsf_arm_mask_s;
} sh_tsf_arm_mask_u_t;
#else
typedef union sh_tsf_arm_mask_u {
	mmr_t	sh_tsf_arm_mask_regval;
	struct {
		mmr_t	mask        : 64;
	} sh_tsf_arm_mask_s;
} sh_tsf_arm_mask_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_TSF_COUNTER_PRESETS"                   */
/*             Trigger sequencing facility counter presets              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_tsf_counter_presets_u {
	mmr_t	sh_tsf_counter_presets_regval;
	struct {
		mmr_t	count_32    : 32;
		mmr_t	count_16    : 16;
		mmr_t	count_8b    : 8;
		mmr_t	count_8a    : 8;
	} sh_tsf_counter_presets_s;
} sh_tsf_counter_presets_u_t;
#else
typedef union sh_tsf_counter_presets_u {
	mmr_t	sh_tsf_counter_presets_regval;
	struct {
		mmr_t	count_8a    : 8;
		mmr_t	count_8b    : 8;
		mmr_t	count_16    : 16;
		mmr_t	count_32    : 32;
	} sh_tsf_counter_presets_s;
} sh_tsf_counter_presets_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_TSF_DECREMENT_CTL"                    */
/*        Trigger sequencing facility counter decrement control         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_tsf_decrement_ctl_u {
	mmr_t	sh_tsf_decrement_ctl_regval;
	struct {
		mmr_t	ctl         : 16;
		mmr_t	reserved_0  : 48;
	} sh_tsf_decrement_ctl_s;
} sh_tsf_decrement_ctl_u_t;
#else
typedef union sh_tsf_decrement_ctl_u {
	mmr_t	sh_tsf_decrement_ctl_regval;
	struct {
		mmr_t	reserved_0  : 48;
		mmr_t	ctl         : 16;
	} sh_tsf_decrement_ctl_s;
} sh_tsf_decrement_ctl_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_TSF_DIAG_MSG_CTL"                    */
/*        Trigger sequencing facility diagnostic message control        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_tsf_diag_msg_ctl_u {
	mmr_t	sh_tsf_diag_msg_ctl_regval;
	struct {
		mmr_t	enable      : 8;
		mmr_t	reserved_0  : 56;
	} sh_tsf_diag_msg_ctl_s;
} sh_tsf_diag_msg_ctl_u_t;
#else
typedef union sh_tsf_diag_msg_ctl_u {
	mmr_t	sh_tsf_diag_msg_ctl_regval;
	struct {
		mmr_t	reserved_0  : 56;
		mmr_t	enable      : 8;
	} sh_tsf_diag_msg_ctl_s;
} sh_tsf_diag_msg_ctl_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_TSF_DISARM_MASK"                     */
/*               Trigger sequencing facility disarm mask                */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_tsf_disarm_mask_u {
	mmr_t	sh_tsf_disarm_mask_regval;
	struct {
		mmr_t	mask        : 64;
	} sh_tsf_disarm_mask_s;
} sh_tsf_disarm_mask_u_t;
#else
typedef union sh_tsf_disarm_mask_u {
	mmr_t	sh_tsf_disarm_mask_regval;
	struct {
		mmr_t	mask        : 64;
	} sh_tsf_disarm_mask_s;
} sh_tsf_disarm_mask_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_TSF_ENABLE_CTL"                     */
/*          Trigger sequencing facility counter enable control          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_tsf_enable_ctl_u {
	mmr_t	sh_tsf_enable_ctl_regval;
	struct {
		mmr_t	ctl         : 16;
		mmr_t	reserved_0  : 48;
	} sh_tsf_enable_ctl_s;
} sh_tsf_enable_ctl_u_t;
#else
typedef union sh_tsf_enable_ctl_u {
	mmr_t	sh_tsf_enable_ctl_regval;
	struct {
		mmr_t	reserved_0  : 48;
		mmr_t	ctl         : 16;
	} sh_tsf_enable_ctl_s;
} sh_tsf_enable_ctl_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_TSF_SOFTWARE_ARM"                    */
/*               Trigger sequencing facility software arm               */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_tsf_software_arm_u {
	mmr_t	sh_tsf_software_arm_regval;
	struct {
		mmr_t	bit0        : 1;
		mmr_t	bit1        : 1;
		mmr_t	bit2        : 1;
		mmr_t	bit3        : 1;
		mmr_t	bit4        : 1;
		mmr_t	bit5        : 1;
		mmr_t	bit6        : 1;
		mmr_t	bit7        : 1;
		mmr_t	reserved_0  : 56;
	} sh_tsf_software_arm_s;
} sh_tsf_software_arm_u_t;
#else
typedef union sh_tsf_software_arm_u {
	mmr_t	sh_tsf_software_arm_regval;
	struct {
		mmr_t	reserved_0  : 56;
		mmr_t	bit7        : 1;
		mmr_t	bit6        : 1;
		mmr_t	bit5        : 1;
		mmr_t	bit4        : 1;
		mmr_t	bit3        : 1;
		mmr_t	bit2        : 1;
		mmr_t	bit1        : 1;
		mmr_t	bit0        : 1;
	} sh_tsf_software_arm_s;
} sh_tsf_software_arm_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_TSF_SOFTWARE_DISARM"                   */
/*             Trigger sequencing facility software disarm              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_tsf_software_disarm_u {
	mmr_t	sh_tsf_software_disarm_regval;
	struct {
		mmr_t	bit0        : 1;
		mmr_t	bit1        : 1;
		mmr_t	bit2        : 1;
		mmr_t	bit3        : 1;
		mmr_t	bit4        : 1;
		mmr_t	bit5        : 1;
		mmr_t	bit6        : 1;
		mmr_t	bit7        : 1;
		mmr_t	reserved_0  : 56;
	} sh_tsf_software_disarm_s;
} sh_tsf_software_disarm_u_t;
#else
typedef union sh_tsf_software_disarm_u {
	mmr_t	sh_tsf_software_disarm_regval;
	struct {
		mmr_t	reserved_0  : 56;
		mmr_t	bit7        : 1;
		mmr_t	bit6        : 1;
		mmr_t	bit5        : 1;
		mmr_t	bit4        : 1;
		mmr_t	bit3        : 1;
		mmr_t	bit2        : 1;
		mmr_t	bit1        : 1;
		mmr_t	bit0        : 1;
	} sh_tsf_software_disarm_s;
} sh_tsf_software_disarm_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_TSF_SOFTWARE_TRIGGERED"                 */
/*            Trigger sequencing facility software triggered            */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_tsf_software_triggered_u {
	mmr_t	sh_tsf_software_triggered_regval;
	struct {
		mmr_t	bit0        : 1;
		mmr_t	bit1        : 1;
		mmr_t	bit2        : 1;
		mmr_t	bit3        : 1;
		mmr_t	bit4        : 1;
		mmr_t	bit5        : 1;
		mmr_t	bit6        : 1;
		mmr_t	bit7        : 1;
		mmr_t	reserved_0  : 56;
	} sh_tsf_software_triggered_s;
} sh_tsf_software_triggered_u_t;
#else
typedef union sh_tsf_software_triggered_u {
	mmr_t	sh_tsf_software_triggered_regval;
	struct {
		mmr_t	reserved_0  : 56;
		mmr_t	bit7        : 1;
		mmr_t	bit6        : 1;
		mmr_t	bit5        : 1;
		mmr_t	bit4        : 1;
		mmr_t	bit3        : 1;
		mmr_t	bit2        : 1;
		mmr_t	bit1        : 1;
		mmr_t	bit0        : 1;
	} sh_tsf_software_triggered_s;
} sh_tsf_software_triggered_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_TSF_TRIGGER_MASK"                    */
/*               Trigger sequencing facility trigger mask               */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_tsf_trigger_mask_u {
	mmr_t	sh_tsf_trigger_mask_regval;
	struct {
		mmr_t	mask        : 64;
	} sh_tsf_trigger_mask_s;
} sh_tsf_trigger_mask_u_t;
#else
typedef union sh_tsf_trigger_mask_u {
	mmr_t	sh_tsf_trigger_mask_regval;
	struct {
		mmr_t	mask        : 64;
	} sh_tsf_trigger_mask_s;
} sh_tsf_trigger_mask_u_t;
#endif

/* ==================================================================== */
/*                        Register "SH_VEC_DATA"                        */
/*                  Vector Write Request Message Data                   */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_vec_data_u {
	mmr_t	sh_vec_data_regval;
	struct {
		mmr_t	data        : 64;
	} sh_vec_data_s;
} sh_vec_data_u_t;
#else
typedef union sh_vec_data_u {
	mmr_t	sh_vec_data_regval;
	struct {
		mmr_t	data        : 64;
	} sh_vec_data_s;
} sh_vec_data_u_t;
#endif

/* ==================================================================== */
/*                       Register "SH_VEC_PARMS"                        */
/*                  Vector Message Parameters Register                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_vec_parms_u {
	mmr_t	sh_vec_parms_regval;
	struct {
		mmr_t	type        : 1;
		mmr_t	ni_port     : 1;
		mmr_t	reserved_0  : 1;
		mmr_t	address     : 32;
		mmr_t	pio_id      : 11;
		mmr_t	reserved_1  : 16;
		mmr_t	start       : 1;
		mmr_t	busy        : 1;
	} sh_vec_parms_s;
} sh_vec_parms_u_t;
#else
typedef union sh_vec_parms_u {
	mmr_t	sh_vec_parms_regval;
	struct {
		mmr_t	busy        : 1;
		mmr_t	start       : 1;
		mmr_t	reserved_1  : 16;
		mmr_t	pio_id      : 11;
		mmr_t	address     : 32;
		mmr_t	reserved_0  : 1;
		mmr_t	ni_port     : 1;
		mmr_t	type        : 1;
	} sh_vec_parms_s;
} sh_vec_parms_u_t;
#endif

/* ==================================================================== */
/*                       Register "SH_VEC_ROUTE"                        */
/*                     Vector Request Message Route                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_vec_route_u {
	mmr_t	sh_vec_route_regval;
	struct {
		mmr_t	route       : 64;
	} sh_vec_route_s;
} sh_vec_route_u_t;
#else
typedef union sh_vec_route_u {
	mmr_t	sh_vec_route_regval;
	struct {
		mmr_t	route       : 64;
	} sh_vec_route_s;
} sh_vec_route_u_t;
#endif

/* ==================================================================== */
/*                        Register "SH_CPU_PERM"                        */
/*                    CPU MMR Access Permission Bits                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_cpu_perm_u {
	mmr_t	sh_cpu_perm_regval;
	struct {
		mmr_t	access_bits : 64;
	} sh_cpu_perm_s;
} sh_cpu_perm_u_t;
#else
typedef union sh_cpu_perm_u {
	mmr_t	sh_cpu_perm_regval;
	struct {
		mmr_t	access_bits : 64;
	} sh_cpu_perm_s;
} sh_cpu_perm_u_t;
#endif

/* ==================================================================== */
/*                      Register "SH_CPU_PERM_OVR"                      */
/*                  CPU MMR Access Permission Override                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_cpu_perm_ovr_u {
	mmr_t	sh_cpu_perm_ovr_regval;
	struct {
		mmr_t	override    : 64;
	} sh_cpu_perm_ovr_s;
} sh_cpu_perm_ovr_u_t;
#else
typedef union sh_cpu_perm_ovr_u {
	mmr_t	sh_cpu_perm_ovr_regval;
	struct {
		mmr_t	override    : 64;
	} sh_cpu_perm_ovr_s;
} sh_cpu_perm_ovr_u_t;
#endif

/* ==================================================================== */
/*                      Register "SH_EXT_IO_PERM"                       */
/*                External IO MMR Access Permission Bits                */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ext_io_perm_u {
	mmr_t	sh_ext_io_perm_regval;
	struct {
		mmr_t	access_bits : 64;
	} sh_ext_io_perm_s;
} sh_ext_io_perm_u_t;
#else
typedef union sh_ext_io_perm_u {
	mmr_t	sh_ext_io_perm_regval;
	struct {
		mmr_t	access_bits : 64;
	} sh_ext_io_perm_s;
} sh_ext_io_perm_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_EXT_IOI_ACCESS"                     */
/*             External IO Interrupt Access Permission Bits             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ext_ioi_access_u {
	mmr_t	sh_ext_ioi_access_regval;
	struct {
		mmr_t	access_bits : 64;
	} sh_ext_ioi_access_s;
} sh_ext_ioi_access_u_t;
#else
typedef union sh_ext_ioi_access_u {
	mmr_t	sh_ext_ioi_access_regval;
	struct {
		mmr_t	access_bits : 64;
	} sh_ext_ioi_access_s;
} sh_ext_ioi_access_u_t;
#endif

/* ==================================================================== */
/*                      Register "SH_GC_FIL_CTRL"                       */
/*                   SHub Global Clock Filter Control                   */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_gc_fil_ctrl_u {
	mmr_t	sh_gc_fil_ctrl_regval;
	struct {
		mmr_t	offset          : 5;
		mmr_t	reserved_0      : 3;
		mmr_t	mask_counter    : 12;
		mmr_t	mask_enable     : 1;
		mmr_t	reserved_1      : 3;
		mmr_t	dropout_counter : 10;
		mmr_t	reserved_2      : 2;
		mmr_t	dropout_thresh  : 10;
		mmr_t	reserved_3      : 2;
		mmr_t	error_counter   : 10;
		mmr_t	reserved_4      : 6;
	} sh_gc_fil_ctrl_s;
} sh_gc_fil_ctrl_u_t;
#else
typedef union sh_gc_fil_ctrl_u {
	mmr_t	sh_gc_fil_ctrl_regval;
	struct {
		mmr_t	reserved_4      : 6;
		mmr_t	error_counter   : 10;
		mmr_t	reserved_3      : 2;
		mmr_t	dropout_thresh  : 10;
		mmr_t	reserved_2      : 2;
		mmr_t	dropout_counter : 10;
		mmr_t	reserved_1      : 3;
		mmr_t	mask_enable     : 1;
		mmr_t	mask_counter    : 12;
		mmr_t	reserved_0      : 3;
		mmr_t	offset          : 5;
	} sh_gc_fil_ctrl_s;
} sh_gc_fil_ctrl_u_t;
#endif

/* ==================================================================== */
/*                      Register "SH_GC_SRC_CTRL"                       */
/*                      SHub Global Clock Control                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_gc_src_ctrl_u {
	mmr_t	sh_gc_src_ctrl_regval;
	struct {
		mmr_t	enable_counter : 1;
		mmr_t	reserved_0     : 3;
		mmr_t	max_count      : 10;
		mmr_t	reserved_1     : 2;
		mmr_t	counter        : 10;
		mmr_t	reserved_2     : 2;
		mmr_t	toggle_bit     : 1;
		mmr_t	reserved_3     : 3;
		mmr_t	source_sel     : 2;
		mmr_t	reserved_4     : 30;
	} sh_gc_src_ctrl_s;
} sh_gc_src_ctrl_u_t;
#else
typedef union sh_gc_src_ctrl_u {
	mmr_t	sh_gc_src_ctrl_regval;
	struct {
		mmr_t	reserved_4     : 30;
		mmr_t	source_sel     : 2;
		mmr_t	reserved_3     : 3;
		mmr_t	toggle_bit     : 1;
		mmr_t	reserved_2     : 2;
		mmr_t	counter        : 10;
		mmr_t	reserved_1     : 2;
		mmr_t	max_count      : 10;
		mmr_t	reserved_0     : 3;
		mmr_t	enable_counter : 1;
	} sh_gc_src_ctrl_s;
} sh_gc_src_ctrl_u_t;
#endif

/* ==================================================================== */
/*                       Register "SH_HARD_RESET"                       */
/*                           SHub Hard Reset                            */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_hard_reset_u {
	mmr_t	sh_hard_reset_regval;
	struct {
		mmr_t	hard_reset  : 1;
		mmr_t	reserved_0  : 63;
	} sh_hard_reset_s;
} sh_hard_reset_u_t;
#else
typedef union sh_hard_reset_u {
	mmr_t	sh_hard_reset_regval;
	struct {
		mmr_t	reserved_0  : 63;
		mmr_t	hard_reset  : 1;
	} sh_hard_reset_s;
} sh_hard_reset_u_t;
#endif

/* ==================================================================== */
/*                        Register "SH_IO_PERM"                         */
/*                    II MMR Access Permission Bits                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_io_perm_u {
	mmr_t	sh_io_perm_regval;
	struct {
		mmr_t	access_bits : 64;
	} sh_io_perm_s;
} sh_io_perm_u_t;
#else
typedef union sh_io_perm_u {
	mmr_t	sh_io_perm_regval;
	struct {
		mmr_t	access_bits : 64;
	} sh_io_perm_s;
} sh_io_perm_u_t;
#endif

/* ==================================================================== */
/*                       Register "SH_IOI_ACCESS"                       */
/*                 II Interrupt Access Permission Bits                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ioi_access_u {
	mmr_t	sh_ioi_access_regval;
	struct {
		mmr_t	access_bits : 64;
	} sh_ioi_access_s;
} sh_ioi_access_u_t;
#else
typedef union sh_ioi_access_u {
	mmr_t	sh_ioi_access_regval;
	struct {
		mmr_t	access_bits : 64;
	} sh_ioi_access_s;
} sh_ioi_access_u_t;
#endif

/* ==================================================================== */
/*                       Register "SH_IPI_ACCESS"                       */
/*                 CPU interrupt Access Permission Bits                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ipi_access_u {
	mmr_t	sh_ipi_access_regval;
	struct {
		mmr_t	access_bits : 64;
	} sh_ipi_access_s;
} sh_ipi_access_u_t;
#else
typedef union sh_ipi_access_u {
	mmr_t	sh_ipi_access_regval;
	struct {
		mmr_t	access_bits : 64;
	} sh_ipi_access_s;
} sh_ipi_access_u_t;
#endif

/* ==================================================================== */
/*                      Register "SH_JTAG_CONFIG"                       */
/*                       SHub JTAG configuration                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_jtag_config_u {
	mmr_t	sh_jtag_config_regval;
	struct {
		mmr_t	md_clk_sel                    : 2;
		mmr_t	ni_clk_sel                    : 1;
		mmr_t	ii_clk_sel                    : 2;
		mmr_t	wrt90_target                  : 14;
		mmr_t	wrt90_overrider               : 1;
		mmr_t	wrt90_override                : 1;
		mmr_t	jtag_mci_reset_delay          : 4;
		mmr_t	jtag_mci_target               : 14;
		mmr_t	jtag_mci_override             : 1;
		mmr_t	fsb_config_ioq_depth          : 1;
		mmr_t	fsb_config_sample_binit       : 1;
		mmr_t	fsb_config_enable_bus_parking : 1;
		mmr_t	fsb_config_clock_ratio        : 5;
		mmr_t	fsb_config_output_tristate    : 4;
		mmr_t	fsb_config_enable_bist        : 1;
		mmr_t	fsb_config_aux                : 2;
		mmr_t	gtl_config_re                 : 1;
		mmr_t	reserved_0                    : 8;
	} sh_jtag_config_s;
} sh_jtag_config_u_t;
#else
typedef union sh_jtag_config_u {
	mmr_t	sh_jtag_config_regval;
	struct {
		mmr_t	reserved_0                    : 8;
		mmr_t	gtl_config_re                 : 1;
		mmr_t	fsb_config_aux                : 2;
		mmr_t	fsb_config_enable_bist        : 1;
		mmr_t	fsb_config_output_tristate    : 4;
		mmr_t	fsb_config_clock_ratio        : 5;
		mmr_t	fsb_config_enable_bus_parking : 1;
		mmr_t	fsb_config_sample_binit       : 1;
		mmr_t	fsb_config_ioq_depth          : 1;
		mmr_t	jtag_mci_override             : 1;
		mmr_t	jtag_mci_target               : 14;
		mmr_t	jtag_mci_reset_delay          : 4;
		mmr_t	wrt90_override                : 1;
		mmr_t	wrt90_overrider               : 1;
		mmr_t	wrt90_target                  : 14;
		mmr_t	ii_clk_sel                    : 2;
		mmr_t	ni_clk_sel                    : 1;
		mmr_t	md_clk_sel                    : 2;
	} sh_jtag_config_s;
} sh_jtag_config_u_t;
#endif

/* ==================================================================== */
/*                        Register "SH_SHUB_ID"                         */
/*                            SHub ID Number                            */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_shub_id_u {
	mmr_t	sh_shub_id_regval;
	struct {
		mmr_t	force1        : 1;
		mmr_t	manufacturer  : 11;
		mmr_t	part_number   : 16;
		mmr_t	revision      : 4;
		mmr_t	node_id       : 11;
		mmr_t	reserved_0    : 1;
		mmr_t	sharing_mode  : 2;
		mmr_t	reserved_1    : 2;
		mmr_t	nodes_per_bit : 5;
		mmr_t	reserved_2    : 3;
		mmr_t	ni_port       : 1;
		mmr_t	reserved_3    : 7;
	} sh_shub_id_s;
} sh_shub_id_u_t;
#else
typedef union sh_shub_id_u {
	mmr_t	sh_shub_id_regval;
	struct {
		mmr_t	reserved_3    : 7;
		mmr_t	ni_port       : 1;
		mmr_t	reserved_2    : 3;
		mmr_t	nodes_per_bit : 5;
		mmr_t	reserved_1    : 2;
		mmr_t	sharing_mode  : 2;
		mmr_t	reserved_0    : 1;
		mmr_t	node_id       : 11;
		mmr_t	revision      : 4;
		mmr_t	part_number   : 16;
		mmr_t	manufacturer  : 11;
		mmr_t	force1        : 1;
	} sh_shub_id_s;
} sh_shub_id_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_SHUBS_PRESENT0"                     */
/*         Shubs 0 - 63 Present. Used for invalidate generation         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_shubs_present0_u {
	mmr_t	sh_shubs_present0_regval;
	struct {
		mmr_t	shubs_present0 : 64;
	} sh_shubs_present0_s;
} sh_shubs_present0_u_t;
#else
typedef union sh_shubs_present0_u {
	mmr_t	sh_shubs_present0_regval;
	struct {
		mmr_t	shubs_present0 : 64;
	} sh_shubs_present0_s;
} sh_shubs_present0_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_SHUBS_PRESENT1"                     */
/*        Shubs 64 - 127 Present. Used for invalidate generation        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_shubs_present1_u {
	mmr_t	sh_shubs_present1_regval;
	struct {
		mmr_t	shubs_present1 : 64;
	} sh_shubs_present1_s;
} sh_shubs_present1_u_t;
#else
typedef union sh_shubs_present1_u {
	mmr_t	sh_shubs_present1_regval;
	struct {
		mmr_t	shubs_present1 : 64;
	} sh_shubs_present1_s;
} sh_shubs_present1_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_SHUBS_PRESENT2"                     */
/*       Shubs 128 - 191 Present. Used for invalidate generation        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_shubs_present2_u {
	mmr_t	sh_shubs_present2_regval;
	struct {
		mmr_t	shubs_present2 : 64;
	} sh_shubs_present2_s;
} sh_shubs_present2_u_t;
#else
typedef union sh_shubs_present2_u {
	mmr_t	sh_shubs_present2_regval;
	struct {
		mmr_t	shubs_present2 : 64;
	} sh_shubs_present2_s;
} sh_shubs_present2_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_SHUBS_PRESENT3"                     */
/*       Shubs 192 - 255 Present. Used for invalidate generation        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_shubs_present3_u {
	mmr_t	sh_shubs_present3_regval;
	struct {
		mmr_t	shubs_present3 : 64;
	} sh_shubs_present3_s;
} sh_shubs_present3_u_t;
#else
typedef union sh_shubs_present3_u {
	mmr_t	sh_shubs_present3_regval;
	struct {
		mmr_t	shubs_present3 : 64;
	} sh_shubs_present3_s;
} sh_shubs_present3_u_t;
#endif

/* ==================================================================== */
/*                       Register "SH_SOFT_RESET"                       */
/*                           SHub Soft Reset                            */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_soft_reset_u {
	mmr_t	sh_soft_reset_regval;
	struct {
		mmr_t	soft_reset  : 1;
		mmr_t	reserved_0  : 63;
	} sh_soft_reset_s;
} sh_soft_reset_u_t;
#else
typedef union sh_soft_reset_u {
	mmr_t	sh_soft_reset_regval;
	struct {
		mmr_t	reserved_0  : 63;
		mmr_t	soft_reset  : 1;
	} sh_soft_reset_s;
} sh_soft_reset_u_t;
#endif

/* ==================================================================== */
/*                      Register "SH_FIRST_ERROR"                       */
/*                    Shub Global First Error Flags                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_first_error_u {
	mmr_t	sh_first_error_regval;
	struct {
		mmr_t	first_error : 19;
		mmr_t	reserved_0  : 45;
	} sh_first_error_s;
} sh_first_error_u_t;
#else
typedef union sh_first_error_u {
	mmr_t	sh_first_error_regval;
	struct {
		mmr_t	reserved_0  : 45;
		mmr_t	first_error : 19;
	} sh_first_error_s;
} sh_first_error_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_II_HW_TIME_STAMP"                    */
/*                     II hardware error time stamp                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ii_hw_time_stamp_u {
	mmr_t	sh_ii_hw_time_stamp_regval;
	struct {
		mmr_t	time        : 63;
		mmr_t	valid       : 1;
	} sh_ii_hw_time_stamp_s;
} sh_ii_hw_time_stamp_u_t;
#else
typedef union sh_ii_hw_time_stamp_u {
	mmr_t	sh_ii_hw_time_stamp_regval;
	struct {
		mmr_t	valid       : 1;
		mmr_t	time        : 63;
	} sh_ii_hw_time_stamp_s;
} sh_ii_hw_time_stamp_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_LB_HW_TIME_STAMP"                    */
/*                     LB hardware error time stamp                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_lb_hw_time_stamp_u {
	mmr_t	sh_lb_hw_time_stamp_regval;
	struct {
		mmr_t	time        : 63;
		mmr_t	valid       : 1;
	} sh_lb_hw_time_stamp_s;
} sh_lb_hw_time_stamp_u_t;
#else
typedef union sh_lb_hw_time_stamp_u {
	mmr_t	sh_lb_hw_time_stamp_regval;
	struct {
		mmr_t	valid       : 1;
		mmr_t	time        : 63;
	} sh_lb_hw_time_stamp_s;
} sh_lb_hw_time_stamp_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_MD_COR_TIME_STAMP"                    */
/*                   MD correctable error time stamp                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_cor_time_stamp_u {
	mmr_t	sh_md_cor_time_stamp_regval;
	struct {
		mmr_t	time        : 63;
		mmr_t	valid       : 1;
	} sh_md_cor_time_stamp_s;
} sh_md_cor_time_stamp_u_t;
#else
typedef union sh_md_cor_time_stamp_u {
	mmr_t	sh_md_cor_time_stamp_regval;
	struct {
		mmr_t	valid       : 1;
		mmr_t	time        : 63;
	} sh_md_cor_time_stamp_s;
} sh_md_cor_time_stamp_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_MD_HW_TIME_STAMP"                    */
/*                     MD hardware error time stamp                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_hw_time_stamp_u {
	mmr_t	sh_md_hw_time_stamp_regval;
	struct {
		mmr_t	time        : 63;
		mmr_t	valid       : 1;
	} sh_md_hw_time_stamp_s;
} sh_md_hw_time_stamp_u_t;
#else
typedef union sh_md_hw_time_stamp_u {
	mmr_t	sh_md_hw_time_stamp_regval;
	struct {
		mmr_t	valid       : 1;
		mmr_t	time        : 63;
	} sh_md_hw_time_stamp_s;
} sh_md_hw_time_stamp_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_MD_UNCOR_TIME_STAMP"                   */
/*                  MD uncorrectable error time stamp                   */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_uncor_time_stamp_u {
	mmr_t	sh_md_uncor_time_stamp_regval;
	struct {
		mmr_t	time        : 63;
		mmr_t	valid       : 1;
	} sh_md_uncor_time_stamp_s;
} sh_md_uncor_time_stamp_u_t;
#else
typedef union sh_md_uncor_time_stamp_u {
	mmr_t	sh_md_uncor_time_stamp_regval;
	struct {
		mmr_t	valid       : 1;
		mmr_t	time        : 63;
	} sh_md_uncor_time_stamp_s;
} sh_md_uncor_time_stamp_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_PI_COR_TIME_STAMP"                    */
/*                   PI correctable error time stamp                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_cor_time_stamp_u {
	mmr_t	sh_pi_cor_time_stamp_regval;
	struct {
		mmr_t	time        : 63;
		mmr_t	valid       : 1;
	} sh_pi_cor_time_stamp_s;
} sh_pi_cor_time_stamp_u_t;
#else
typedef union sh_pi_cor_time_stamp_u {
	mmr_t	sh_pi_cor_time_stamp_regval;
	struct {
		mmr_t	valid       : 1;
		mmr_t	time        : 63;
	} sh_pi_cor_time_stamp_s;
} sh_pi_cor_time_stamp_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_PI_HW_TIME_STAMP"                    */
/*                     PI hardware error time stamp                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_hw_time_stamp_u {
	mmr_t	sh_pi_hw_time_stamp_regval;
	struct {
		mmr_t	time        : 63;
		mmr_t	valid       : 1;
	} sh_pi_hw_time_stamp_s;
} sh_pi_hw_time_stamp_u_t;
#else
typedef union sh_pi_hw_time_stamp_u {
	mmr_t	sh_pi_hw_time_stamp_regval;
	struct {
		mmr_t	valid       : 1;
		mmr_t	time        : 63;
	} sh_pi_hw_time_stamp_s;
} sh_pi_hw_time_stamp_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PI_UNCOR_TIME_STAMP"                   */
/*                  PI uncorrectable error time stamp                   */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_uncor_time_stamp_u {
	mmr_t	sh_pi_uncor_time_stamp_regval;
	struct {
		mmr_t	time        : 63;
		mmr_t	valid       : 1;
	} sh_pi_uncor_time_stamp_s;
} sh_pi_uncor_time_stamp_u_t;
#else
typedef union sh_pi_uncor_time_stamp_u {
	mmr_t	sh_pi_uncor_time_stamp_regval;
	struct {
		mmr_t	valid       : 1;
		mmr_t	time        : 63;
	} sh_pi_uncor_time_stamp_s;
} sh_pi_uncor_time_stamp_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PROC0_ADV_TIME_STAMP"                  */
/*                      Proc 0 advisory time stamp                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_proc0_adv_time_stamp_u {
	mmr_t	sh_proc0_adv_time_stamp_regval;
	struct {
		mmr_t	time        : 63;
		mmr_t	valid       : 1;
	} sh_proc0_adv_time_stamp_s;
} sh_proc0_adv_time_stamp_u_t;
#else
typedef union sh_proc0_adv_time_stamp_u {
	mmr_t	sh_proc0_adv_time_stamp_regval;
	struct {
		mmr_t	valid       : 1;
		mmr_t	time        : 63;
	} sh_proc0_adv_time_stamp_s;
} sh_proc0_adv_time_stamp_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PROC0_ERR_TIME_STAMP"                  */
/*                       Proc 0 error time stamp                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_proc0_err_time_stamp_u {
	mmr_t	sh_proc0_err_time_stamp_regval;
	struct {
		mmr_t	time        : 63;
		mmr_t	valid       : 1;
	} sh_proc0_err_time_stamp_s;
} sh_proc0_err_time_stamp_u_t;
#else
typedef union sh_proc0_err_time_stamp_u {
	mmr_t	sh_proc0_err_time_stamp_regval;
	struct {
		mmr_t	valid       : 1;
		mmr_t	time        : 63;
	} sh_proc0_err_time_stamp_s;
} sh_proc0_err_time_stamp_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PROC1_ADV_TIME_STAMP"                  */
/*                      Proc 1 advisory time stamp                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_proc1_adv_time_stamp_u {
	mmr_t	sh_proc1_adv_time_stamp_regval;
	struct {
		mmr_t	time        : 63;
		mmr_t	valid       : 1;
	} sh_proc1_adv_time_stamp_s;
} sh_proc1_adv_time_stamp_u_t;
#else
typedef union sh_proc1_adv_time_stamp_u {
	mmr_t	sh_proc1_adv_time_stamp_regval;
	struct {
		mmr_t	valid       : 1;
		mmr_t	time        : 63;
	} sh_proc1_adv_time_stamp_s;
} sh_proc1_adv_time_stamp_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PROC1_ERR_TIME_STAMP"                  */
/*                       Proc 1 error time stamp                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_proc1_err_time_stamp_u {
	mmr_t	sh_proc1_err_time_stamp_regval;
	struct {
		mmr_t	time        : 63;
		mmr_t	valid       : 1;
	} sh_proc1_err_time_stamp_s;
} sh_proc1_err_time_stamp_u_t;
#else
typedef union sh_proc1_err_time_stamp_u {
	mmr_t	sh_proc1_err_time_stamp_regval;
	struct {
		mmr_t	valid       : 1;
		mmr_t	time        : 63;
	} sh_proc1_err_time_stamp_s;
} sh_proc1_err_time_stamp_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PROC2_ADV_TIME_STAMP"                  */
/*                      Proc 2 advisory time stamp                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_proc2_adv_time_stamp_u {
	mmr_t	sh_proc2_adv_time_stamp_regval;
	struct {
		mmr_t	time        : 63;
		mmr_t	valid       : 1;
	} sh_proc2_adv_time_stamp_s;
} sh_proc2_adv_time_stamp_u_t;
#else
typedef union sh_proc2_adv_time_stamp_u {
	mmr_t	sh_proc2_adv_time_stamp_regval;
	struct {
		mmr_t	valid       : 1;
		mmr_t	time        : 63;
	} sh_proc2_adv_time_stamp_s;
} sh_proc2_adv_time_stamp_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PROC2_ERR_TIME_STAMP"                  */
/*                       Proc 2 error time stamp                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_proc2_err_time_stamp_u {
	mmr_t	sh_proc2_err_time_stamp_regval;
	struct {
		mmr_t	time        : 63;
		mmr_t	valid       : 1;
	} sh_proc2_err_time_stamp_s;
} sh_proc2_err_time_stamp_u_t;
#else
typedef union sh_proc2_err_time_stamp_u {
	mmr_t	sh_proc2_err_time_stamp_regval;
	struct {
		mmr_t	valid       : 1;
		mmr_t	time        : 63;
	} sh_proc2_err_time_stamp_s;
} sh_proc2_err_time_stamp_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PROC3_ADV_TIME_STAMP"                  */
/*                      Proc 3 advisory time stamp                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_proc3_adv_time_stamp_u {
	mmr_t	sh_proc3_adv_time_stamp_regval;
	struct {
		mmr_t	time        : 63;
		mmr_t	valid       : 1;
	} sh_proc3_adv_time_stamp_s;
} sh_proc3_adv_time_stamp_u_t;
#else
typedef union sh_proc3_adv_time_stamp_u {
	mmr_t	sh_proc3_adv_time_stamp_regval;
	struct {
		mmr_t	valid       : 1;
		mmr_t	time        : 63;
	} sh_proc3_adv_time_stamp_s;
} sh_proc3_adv_time_stamp_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PROC3_ERR_TIME_STAMP"                  */
/*                       Proc 3 error time stamp                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_proc3_err_time_stamp_u {
	mmr_t	sh_proc3_err_time_stamp_regval;
	struct {
		mmr_t	time        : 63;
		mmr_t	valid       : 1;
	} sh_proc3_err_time_stamp_s;
} sh_proc3_err_time_stamp_u_t;
#else
typedef union sh_proc3_err_time_stamp_u {
	mmr_t	sh_proc3_err_time_stamp_regval;
	struct {
		mmr_t	valid       : 1;
		mmr_t	time        : 63;
	} sh_proc3_err_time_stamp_s;
} sh_proc3_err_time_stamp_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_XN_COR_TIME_STAMP"                    */
/*                   XN correctable error time stamp                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_cor_time_stamp_u {
	mmr_t	sh_xn_cor_time_stamp_regval;
	struct {
		mmr_t	time        : 63;
		mmr_t	valid       : 1;
	} sh_xn_cor_time_stamp_s;
} sh_xn_cor_time_stamp_u_t;
#else
typedef union sh_xn_cor_time_stamp_u {
	mmr_t	sh_xn_cor_time_stamp_regval;
	struct {
		mmr_t	valid       : 1;
		mmr_t	time        : 63;
	} sh_xn_cor_time_stamp_s;
} sh_xn_cor_time_stamp_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_XN_HW_TIME_STAMP"                    */
/*                     XN hardware error time stamp                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_hw_time_stamp_u {
	mmr_t	sh_xn_hw_time_stamp_regval;
	struct {
		mmr_t	time        : 63;
		mmr_t	valid       : 1;
	} sh_xn_hw_time_stamp_s;
} sh_xn_hw_time_stamp_u_t;
#else
typedef union sh_xn_hw_time_stamp_u {
	mmr_t	sh_xn_hw_time_stamp_regval;
	struct {
		mmr_t	valid       : 1;
		mmr_t	time        : 63;
	} sh_xn_hw_time_stamp_s;
} sh_xn_hw_time_stamp_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_XN_UNCOR_TIME_STAMP"                   */
/*                  XN uncorrectable error time stamp                   */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_uncor_time_stamp_u {
	mmr_t	sh_xn_uncor_time_stamp_regval;
	struct {
		mmr_t	time        : 63;
		mmr_t	valid       : 1;
	} sh_xn_uncor_time_stamp_s;
} sh_xn_uncor_time_stamp_u_t;
#else
typedef union sh_xn_uncor_time_stamp_u {
	mmr_t	sh_xn_uncor_time_stamp_regval;
	struct {
		mmr_t	valid       : 1;
		mmr_t	time        : 63;
	} sh_xn_uncor_time_stamp_s;
} sh_xn_uncor_time_stamp_u_t;
#endif

/* ==================================================================== */
/*                       Register "SH_DEBUG_PORT"                       */
/*                           SHub Debug Port                            */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_debug_port_u {
	mmr_t	sh_debug_port_regval;
	struct {
		mmr_t	debug_nibble0 : 4;
		mmr_t	debug_nibble1 : 4;
		mmr_t	debug_nibble2 : 4;
		mmr_t	debug_nibble3 : 4;
		mmr_t	debug_nibble4 : 4;
		mmr_t	debug_nibble5 : 4;
		mmr_t	debug_nibble6 : 4;
		mmr_t	debug_nibble7 : 4;
		mmr_t	reserved_0    : 32;
	} sh_debug_port_s;
} sh_debug_port_u_t;
#else
typedef union sh_debug_port_u {
	mmr_t	sh_debug_port_regval;
	struct {
		mmr_t	reserved_0    : 32;
		mmr_t	debug_nibble7 : 4;
		mmr_t	debug_nibble6 : 4;
		mmr_t	debug_nibble5 : 4;
		mmr_t	debug_nibble4 : 4;
		mmr_t	debug_nibble3 : 4;
		mmr_t	debug_nibble2 : 4;
		mmr_t	debug_nibble1 : 4;
		mmr_t	debug_nibble0 : 4;
	} sh_debug_port_s;
} sh_debug_port_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_II_DEBUG_DATA"                      */
/*                            II Debug Data                             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ii_debug_data_u {
	mmr_t	sh_ii_debug_data_regval;
	struct {
		mmr_t	ii_data     : 32;
		mmr_t	reserved_0  : 32;
	} sh_ii_debug_data_s;
} sh_ii_debug_data_u_t;
#else
typedef union sh_ii_debug_data_u {
	mmr_t	sh_ii_debug_data_regval;
	struct {
		mmr_t	reserved_0  : 32;
		mmr_t	ii_data     : 32;
	} sh_ii_debug_data_s;
} sh_ii_debug_data_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_II_WRAP_DEBUG_DATA"                   */
/*                      SHub II Wrapper Debug Data                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ii_wrap_debug_data_u {
	mmr_t	sh_ii_wrap_debug_data_regval;
	struct {
		mmr_t	ii_wrap_data : 32;
		mmr_t	reserved_0   : 32;
	} sh_ii_wrap_debug_data_s;
} sh_ii_wrap_debug_data_u_t;
#else
typedef union sh_ii_wrap_debug_data_u {
	mmr_t	sh_ii_wrap_debug_data_regval;
	struct {
		mmr_t	reserved_0   : 32;
		mmr_t	ii_wrap_data : 32;
	} sh_ii_wrap_debug_data_s;
} sh_ii_wrap_debug_data_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_LB_DEBUG_DATA"                      */
/*                          SHub LB Debug Data                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_lb_debug_data_u {
	mmr_t	sh_lb_debug_data_regval;
	struct {
		mmr_t	lb_data     : 32;
		mmr_t	reserved_0  : 32;
	} sh_lb_debug_data_s;
} sh_lb_debug_data_u_t;
#else
typedef union sh_lb_debug_data_u {
	mmr_t	sh_lb_debug_data_regval;
	struct {
		mmr_t	reserved_0  : 32;
		mmr_t	lb_data     : 32;
	} sh_lb_debug_data_s;
} sh_lb_debug_data_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_MD_DEBUG_DATA"                      */
/*                          SHub MD Debug Data                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_debug_data_u {
	mmr_t	sh_md_debug_data_regval;
	struct {
		mmr_t	md_data     : 32;
		mmr_t	reserved_0  : 32;
	} sh_md_debug_data_s;
} sh_md_debug_data_u_t;
#else
typedef union sh_md_debug_data_u {
	mmr_t	sh_md_debug_data_regval;
	struct {
		mmr_t	reserved_0  : 32;
		mmr_t	md_data     : 32;
	} sh_md_debug_data_s;
} sh_md_debug_data_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_PI_DEBUG_DATA"                      */
/*                          SHub PI Debug Data                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_debug_data_u {
	mmr_t	sh_pi_debug_data_regval;
	struct {
		mmr_t	pi_data     : 32;
		mmr_t	reserved_0  : 32;
	} sh_pi_debug_data_s;
} sh_pi_debug_data_u_t;
#else
typedef union sh_pi_debug_data_u {
	mmr_t	sh_pi_debug_data_regval;
	struct {
		mmr_t	reserved_0  : 32;
		mmr_t	pi_data     : 32;
	} sh_pi_debug_data_s;
} sh_pi_debug_data_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_XN_DEBUG_DATA"                      */
/*                          SHub XN Debug Data                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_debug_data_u {
	mmr_t	sh_xn_debug_data_regval;
	struct {
		mmr_t	xn_data     : 32;
		mmr_t	reserved_0  : 32;
	} sh_xn_debug_data_s;
} sh_xn_debug_data_u_t;
#else
typedef union sh_xn_debug_data_u {
	mmr_t	sh_xn_debug_data_regval;
	struct {
		mmr_t	reserved_0  : 32;
		mmr_t	xn_data     : 32;
	} sh_xn_debug_data_s;
} sh_xn_debug_data_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_TSF_ARMED_STATE"                     */
/*                Trigger sequencing facility arm state                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_tsf_armed_state_u {
	mmr_t	sh_tsf_armed_state_regval;
	struct {
		mmr_t	state       : 8;
		mmr_t	reserved_0  : 56;
	} sh_tsf_armed_state_s;
} sh_tsf_armed_state_u_t;
#else
typedef union sh_tsf_armed_state_u {
	mmr_t	sh_tsf_armed_state_regval;
	struct {
		mmr_t	reserved_0  : 56;
		mmr_t	state       : 8;
	} sh_tsf_armed_state_s;
} sh_tsf_armed_state_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_TSF_COUNTER_VALUE"                    */
/*              Trigger sequencing facility counter value               */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_tsf_counter_value_u {
	mmr_t	sh_tsf_counter_value_regval;
	struct {
		mmr_t	count_32    : 32;
		mmr_t	count_16    : 16;
		mmr_t	count_8b    : 8;
		mmr_t	count_8a    : 8;
	} sh_tsf_counter_value_s;
} sh_tsf_counter_value_u_t;
#else
typedef union sh_tsf_counter_value_u {
	mmr_t	sh_tsf_counter_value_regval;
	struct {
		mmr_t	count_8a    : 8;
		mmr_t	count_8b    : 8;
		mmr_t	count_16    : 16;
		mmr_t	count_32    : 32;
	} sh_tsf_counter_value_s;
} sh_tsf_counter_value_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_TSF_TRIGGERED_STATE"                   */
/*             Trigger sequencing facility triggered state              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_tsf_triggered_state_u {
	mmr_t	sh_tsf_triggered_state_regval;
	struct {
		mmr_t	state       : 8;
		mmr_t	reserved_0  : 56;
	} sh_tsf_triggered_state_s;
} sh_tsf_triggered_state_u_t;
#else
typedef union sh_tsf_triggered_state_u {
	mmr_t	sh_tsf_triggered_state_regval;
	struct {
		mmr_t	reserved_0  : 56;
		mmr_t	state       : 8;
	} sh_tsf_triggered_state_s;
} sh_tsf_triggered_state_u_t;
#endif

/* ==================================================================== */
/*                       Register "SH_VEC_RDDATA"                       */
/*                      Vector Reply Message Data                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_vec_rddata_u {
	mmr_t	sh_vec_rddata_regval;
	struct {
		mmr_t	data        : 64;
	} sh_vec_rddata_s;
} sh_vec_rddata_u_t;
#else
typedef union sh_vec_rddata_u {
	mmr_t	sh_vec_rddata_regval;
	struct {
		mmr_t	data        : 64;
	} sh_vec_rddata_s;
} sh_vec_rddata_u_t;
#endif

/* ==================================================================== */
/*                       Register "SH_VEC_RETURN"                       */
/*                  Vector Reply Message Return Route                   */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_vec_return_u {
	mmr_t	sh_vec_return_regval;
	struct {
		mmr_t	route       : 64;
	} sh_vec_return_s;
} sh_vec_return_u_t;
#else
typedef union sh_vec_return_u {
	mmr_t	sh_vec_return_regval;
	struct {
		mmr_t	route       : 64;
	} sh_vec_return_s;
} sh_vec_return_u_t;
#endif

/* ==================================================================== */
/*                       Register "SH_VEC_STATUS"                       */
/*                     Vector Reply Message Status                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_vec_status_u {
	mmr_t	sh_vec_status_regval;
	struct {
		mmr_t	type         : 3;
		mmr_t	address      : 32;
		mmr_t	pio_id       : 11;
		mmr_t	source       : 14;
		mmr_t	reserved_0   : 2;
		mmr_t	overrun      : 1;
		mmr_t	status_valid : 1;
	} sh_vec_status_s;
} sh_vec_status_u_t;
#else
typedef union sh_vec_status_u {
	mmr_t	sh_vec_status_regval;
	struct {
		mmr_t	status_valid : 1;
		mmr_t	overrun      : 1;
		mmr_t	reserved_0   : 2;
		mmr_t	source       : 14;
		mmr_t	pio_id       : 11;
		mmr_t	address      : 32;
		mmr_t	type         : 3;
	} sh_vec_status_s;
} sh_vec_status_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_PERFORMANCE_COUNT0_CONTROL"               */
/*                    Performance Counter 0 Control                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_performance_count0_control_u {
	mmr_t	sh_performance_count0_control_regval;
	struct {
		mmr_t	up_stimulus     : 5;
		mmr_t	up_event        : 1;
		mmr_t	up_polarity     : 1;
		mmr_t	up_mode         : 1;
		mmr_t	dn_stimulus     : 5;
		mmr_t	dn_event        : 1;
		mmr_t	dn_polarity     : 1;
		mmr_t	dn_mode         : 1;
		mmr_t	inc_enable      : 1;
		mmr_t	dec_enable      : 1;
		mmr_t	peak_det_enable : 1;
		mmr_t	reserved_0      : 45;
	} sh_performance_count0_control_s;
} sh_performance_count0_control_u_t;
#else
typedef union sh_performance_count0_control_u {
	mmr_t	sh_performance_count0_control_regval;
	struct {
		mmr_t	reserved_0      : 45;
		mmr_t	peak_det_enable : 1;
		mmr_t	dec_enable      : 1;
		mmr_t	inc_enable      : 1;
		mmr_t	dn_mode         : 1;
		mmr_t	dn_polarity     : 1;
		mmr_t	dn_event        : 1;
		mmr_t	dn_stimulus     : 5;
		mmr_t	up_mode         : 1;
		mmr_t	up_polarity     : 1;
		mmr_t	up_event        : 1;
		mmr_t	up_stimulus     : 5;
	} sh_performance_count0_control_s;
} sh_performance_count0_control_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_PERFORMANCE_COUNT1_CONTROL"               */
/*                    Performance Counter 1 Control                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_performance_count1_control_u {
	mmr_t	sh_performance_count1_control_regval;
	struct {
		mmr_t	up_stimulus     : 5;
		mmr_t	up_event        : 1;
		mmr_t	up_polarity     : 1;
		mmr_t	up_mode         : 1;
		mmr_t	dn_stimulus     : 5;
		mmr_t	dn_event        : 1;
		mmr_t	dn_polarity     : 1;
		mmr_t	dn_mode         : 1;
		mmr_t	inc_enable      : 1;
		mmr_t	dec_enable      : 1;
		mmr_t	peak_det_enable : 1;
		mmr_t	reserved_0      : 45;
	} sh_performance_count1_control_s;
} sh_performance_count1_control_u_t;
#else
typedef union sh_performance_count1_control_u {
	mmr_t	sh_performance_count1_control_regval;
	struct {
		mmr_t	reserved_0      : 45;
		mmr_t	peak_det_enable : 1;
		mmr_t	dec_enable      : 1;
		mmr_t	inc_enable      : 1;
		mmr_t	dn_mode         : 1;
		mmr_t	dn_polarity     : 1;
		mmr_t	dn_event        : 1;
		mmr_t	dn_stimulus     : 5;
		mmr_t	up_mode         : 1;
		mmr_t	up_polarity     : 1;
		mmr_t	up_event        : 1;
		mmr_t	up_stimulus     : 5;
	} sh_performance_count1_control_s;
} sh_performance_count1_control_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_PERFORMANCE_COUNT2_CONTROL"               */
/*                    Performance Counter 2 Control                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_performance_count2_control_u {
	mmr_t	sh_performance_count2_control_regval;
	struct {
		mmr_t	up_stimulus     : 5;
		mmr_t	up_event        : 1;
		mmr_t	up_polarity     : 1;
		mmr_t	up_mode         : 1;
		mmr_t	dn_stimulus     : 5;
		mmr_t	dn_event        : 1;
		mmr_t	dn_polarity     : 1;
		mmr_t	dn_mode         : 1;
		mmr_t	inc_enable      : 1;
		mmr_t	dec_enable      : 1;
		mmr_t	peak_det_enable : 1;
		mmr_t	reserved_0      : 45;
	} sh_performance_count2_control_s;
} sh_performance_count2_control_u_t;
#else
typedef union sh_performance_count2_control_u {
	mmr_t	sh_performance_count2_control_regval;
	struct {
		mmr_t	reserved_0      : 45;
		mmr_t	peak_det_enable : 1;
		mmr_t	dec_enable      : 1;
		mmr_t	inc_enable      : 1;
		mmr_t	dn_mode         : 1;
		mmr_t	dn_polarity     : 1;
		mmr_t	dn_event        : 1;
		mmr_t	dn_stimulus     : 5;
		mmr_t	up_mode         : 1;
		mmr_t	up_polarity     : 1;
		mmr_t	up_event        : 1;
		mmr_t	up_stimulus     : 5;
	} sh_performance_count2_control_s;
} sh_performance_count2_control_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_PERFORMANCE_COUNT3_CONTROL"               */
/*                    Performance Counter 3 Control                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_performance_count3_control_u {
	mmr_t	sh_performance_count3_control_regval;
	struct {
		mmr_t	up_stimulus     : 5;
		mmr_t	up_event        : 1;
		mmr_t	up_polarity     : 1;
		mmr_t	up_mode         : 1;
		mmr_t	dn_stimulus     : 5;
		mmr_t	dn_event        : 1;
		mmr_t	dn_polarity     : 1;
		mmr_t	dn_mode         : 1;
		mmr_t	inc_enable      : 1;
		mmr_t	dec_enable      : 1;
		mmr_t	peak_det_enable : 1;
		mmr_t	reserved_0      : 45;
	} sh_performance_count3_control_s;
} sh_performance_count3_control_u_t;
#else
typedef union sh_performance_count3_control_u {
	mmr_t	sh_performance_count3_control_regval;
	struct {
		mmr_t	reserved_0      : 45;
		mmr_t	peak_det_enable : 1;
		mmr_t	dec_enable      : 1;
		mmr_t	inc_enable      : 1;
		mmr_t	dn_mode         : 1;
		mmr_t	dn_polarity     : 1;
		mmr_t	dn_event        : 1;
		mmr_t	dn_stimulus     : 5;
		mmr_t	up_mode         : 1;
		mmr_t	up_polarity     : 1;
		mmr_t	up_event        : 1;
		mmr_t	up_stimulus     : 5;
	} sh_performance_count3_control_s;
} sh_performance_count3_control_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_PERFORMANCE_COUNT4_CONTROL"               */
/*                    Performance Counter 4 Control                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_performance_count4_control_u {
	mmr_t	sh_performance_count4_control_regval;
	struct {
		mmr_t	up_stimulus     : 5;
		mmr_t	up_event        : 1;
		mmr_t	up_polarity     : 1;
		mmr_t	up_mode         : 1;
		mmr_t	dn_stimulus     : 5;
		mmr_t	dn_event        : 1;
		mmr_t	dn_polarity     : 1;
		mmr_t	dn_mode         : 1;
		mmr_t	inc_enable      : 1;
		mmr_t	dec_enable      : 1;
		mmr_t	peak_det_enable : 1;
		mmr_t	reserved_0      : 45;
	} sh_performance_count4_control_s;
} sh_performance_count4_control_u_t;
#else
typedef union sh_performance_count4_control_u {
	mmr_t	sh_performance_count4_control_regval;
	struct {
		mmr_t	reserved_0      : 45;
		mmr_t	peak_det_enable : 1;
		mmr_t	dec_enable      : 1;
		mmr_t	inc_enable      : 1;
		mmr_t	dn_mode         : 1;
		mmr_t	dn_polarity     : 1;
		mmr_t	dn_event        : 1;
		mmr_t	dn_stimulus     : 5;
		mmr_t	up_mode         : 1;
		mmr_t	up_polarity     : 1;
		mmr_t	up_event        : 1;
		mmr_t	up_stimulus     : 5;
	} sh_performance_count4_control_s;
} sh_performance_count4_control_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_PERFORMANCE_COUNT5_CONTROL"               */
/*                    Performance Counter 5 Control                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_performance_count5_control_u {
	mmr_t	sh_performance_count5_control_regval;
	struct {
		mmr_t	up_stimulus     : 5;
		mmr_t	up_event        : 1;
		mmr_t	up_polarity     : 1;
		mmr_t	up_mode         : 1;
		mmr_t	dn_stimulus     : 5;
		mmr_t	dn_event        : 1;
		mmr_t	dn_polarity     : 1;
		mmr_t	dn_mode         : 1;
		mmr_t	inc_enable      : 1;
		mmr_t	dec_enable      : 1;
		mmr_t	peak_det_enable : 1;
		mmr_t	reserved_0      : 45;
	} sh_performance_count5_control_s;
} sh_performance_count5_control_u_t;
#else
typedef union sh_performance_count5_control_u {
	mmr_t	sh_performance_count5_control_regval;
	struct {
		mmr_t	reserved_0      : 45;
		mmr_t	peak_det_enable : 1;
		mmr_t	dec_enable      : 1;
		mmr_t	inc_enable      : 1;
		mmr_t	dn_mode         : 1;
		mmr_t	dn_polarity     : 1;
		mmr_t	dn_event        : 1;
		mmr_t	dn_stimulus     : 5;
		mmr_t	up_mode         : 1;
		mmr_t	up_polarity     : 1;
		mmr_t	up_event        : 1;
		mmr_t	up_stimulus     : 5;
	} sh_performance_count5_control_s;
} sh_performance_count5_control_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_PERFORMANCE_COUNT6_CONTROL"               */
/*                    Performance Counter 6 Control                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_performance_count6_control_u {
	mmr_t	sh_performance_count6_control_regval;
	struct {
		mmr_t	up_stimulus     : 5;
		mmr_t	up_event        : 1;
		mmr_t	up_polarity     : 1;
		mmr_t	up_mode         : 1;
		mmr_t	dn_stimulus     : 5;
		mmr_t	dn_event        : 1;
		mmr_t	dn_polarity     : 1;
		mmr_t	dn_mode         : 1;
		mmr_t	inc_enable      : 1;
		mmr_t	dec_enable      : 1;
		mmr_t	peak_det_enable : 1;
		mmr_t	reserved_0      : 45;
	} sh_performance_count6_control_s;
} sh_performance_count6_control_u_t;
#else
typedef union sh_performance_count6_control_u {
	mmr_t	sh_performance_count6_control_regval;
	struct {
		mmr_t	reserved_0      : 45;
		mmr_t	peak_det_enable : 1;
		mmr_t	dec_enable      : 1;
		mmr_t	inc_enable      : 1;
		mmr_t	dn_mode         : 1;
		mmr_t	dn_polarity     : 1;
		mmr_t	dn_event        : 1;
		mmr_t	dn_stimulus     : 5;
		mmr_t	up_mode         : 1;
		mmr_t	up_polarity     : 1;
		mmr_t	up_event        : 1;
		mmr_t	up_stimulus     : 5;
	} sh_performance_count6_control_s;
} sh_performance_count6_control_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_PERFORMANCE_COUNT7_CONTROL"               */
/*                    Performance Counter 7 Control                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_performance_count7_control_u {
	mmr_t	sh_performance_count7_control_regval;
	struct {
		mmr_t	up_stimulus     : 5;
		mmr_t	up_event        : 1;
		mmr_t	up_polarity     : 1;
		mmr_t	up_mode         : 1;
		mmr_t	dn_stimulus     : 5;
		mmr_t	dn_event        : 1;
		mmr_t	dn_polarity     : 1;
		mmr_t	dn_mode         : 1;
		mmr_t	inc_enable      : 1;
		mmr_t	dec_enable      : 1;
		mmr_t	peak_det_enable : 1;
		mmr_t	reserved_0      : 45;
	} sh_performance_count7_control_s;
} sh_performance_count7_control_u_t;
#else
typedef union sh_performance_count7_control_u {
	mmr_t	sh_performance_count7_control_regval;
	struct {
		mmr_t	reserved_0      : 45;
		mmr_t	peak_det_enable : 1;
		mmr_t	dec_enable      : 1;
		mmr_t	inc_enable      : 1;
		mmr_t	dn_mode         : 1;
		mmr_t	dn_polarity     : 1;
		mmr_t	dn_event        : 1;
		mmr_t	dn_stimulus     : 5;
		mmr_t	up_mode         : 1;
		mmr_t	up_polarity     : 1;
		mmr_t	up_event        : 1;
		mmr_t	up_stimulus     : 5;
	} sh_performance_count7_control_s;
} sh_performance_count7_control_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_PROFILE_DN_CONTROL"                   */
/*                     Profile Counter Down Control                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_profile_dn_control_u {
	mmr_t	sh_profile_dn_control_regval;
	struct {
		mmr_t	stimulus    : 5;
		mmr_t	event       : 1;
		mmr_t	polarity    : 1;
		mmr_t	mode        : 1;
		mmr_t	reserved_0  : 56;
	} sh_profile_dn_control_s;
} sh_profile_dn_control_u_t;
#else
typedef union sh_profile_dn_control_u {
	mmr_t	sh_profile_dn_control_regval;
	struct {
		mmr_t	reserved_0  : 56;
		mmr_t	mode        : 1;
		mmr_t	polarity    : 1;
		mmr_t	event       : 1;
		mmr_t	stimulus    : 5;
	} sh_profile_dn_control_s;
} sh_profile_dn_control_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PROFILE_PEAK_CONTROL"                  */
/*                     Profile Counter Peak Control                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_profile_peak_control_u {
	mmr_t	sh_profile_peak_control_regval;
	struct {
		mmr_t	reserved_0  : 3;
		mmr_t	stimulus    : 1;
		mmr_t	reserved_1  : 1;
		mmr_t	event       : 1;
		mmr_t	polarity    : 1;
		mmr_t	reserved_2  : 57;
	} sh_profile_peak_control_s;
} sh_profile_peak_control_u_t;
#else
typedef union sh_profile_peak_control_u {
	mmr_t	sh_profile_peak_control_regval;
	struct {
		mmr_t	reserved_2  : 57;
		mmr_t	polarity    : 1;
		mmr_t	event       : 1;
		mmr_t	reserved_1  : 1;
		mmr_t	stimulus    : 1;
		mmr_t	reserved_0  : 3;
	} sh_profile_peak_control_s;
} sh_profile_peak_control_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_PROFILE_RANGE"                      */
/*                        Profile Counter Range                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_profile_range_u {
	mmr_t	sh_profile_range_regval;
	struct {
		mmr_t	range0      : 8;
		mmr_t	range1      : 8;
		mmr_t	range2      : 8;
		mmr_t	range3      : 8;
		mmr_t	range4      : 8;
		mmr_t	range5      : 8;
		mmr_t	range6      : 8;
		mmr_t	range7      : 8;
	} sh_profile_range_s;
} sh_profile_range_u_t;
#else
typedef union sh_profile_range_u {
	mmr_t	sh_profile_range_regval;
	struct {
		mmr_t	range7      : 8;
		mmr_t	range6      : 8;
		mmr_t	range5      : 8;
		mmr_t	range4      : 8;
		mmr_t	range3      : 8;
		mmr_t	range2      : 8;
		mmr_t	range1      : 8;
		mmr_t	range0      : 8;
	} sh_profile_range_s;
} sh_profile_range_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_PROFILE_UP_CONTROL"                   */
/*                      Profile Counter Up Control                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_profile_up_control_u {
	mmr_t	sh_profile_up_control_regval;
	struct {
		mmr_t	stimulus    : 5;
		mmr_t	event       : 1;
		mmr_t	polarity    : 1;
		mmr_t	mode        : 1;
		mmr_t	reserved_0  : 56;
	} sh_profile_up_control_s;
} sh_profile_up_control_u_t;
#else
typedef union sh_profile_up_control_u {
	mmr_t	sh_profile_up_control_regval;
	struct {
		mmr_t	reserved_0  : 56;
		mmr_t	mode        : 1;
		mmr_t	polarity    : 1;
		mmr_t	event       : 1;
		mmr_t	stimulus    : 5;
	} sh_profile_up_control_s;
} sh_profile_up_control_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PERFORMANCE_COUNTER0"                  */
/*                        Performance Counter 0                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_performance_counter0_u {
	mmr_t	sh_performance_counter0_regval;
	struct {
		mmr_t	count       : 32;
		mmr_t	reserved_0  : 32;
	} sh_performance_counter0_s;
} sh_performance_counter0_u_t;
#else
typedef union sh_performance_counter0_u {
	mmr_t	sh_performance_counter0_regval;
	struct {
		mmr_t	reserved_0  : 32;
		mmr_t	count       : 32;
	} sh_performance_counter0_s;
} sh_performance_counter0_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PERFORMANCE_COUNTER1"                  */
/*                        Performance Counter 1                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_performance_counter1_u {
	mmr_t	sh_performance_counter1_regval;
	struct {
		mmr_t	count       : 32;
		mmr_t	reserved_0  : 32;
	} sh_performance_counter1_s;
} sh_performance_counter1_u_t;
#else
typedef union sh_performance_counter1_u {
	mmr_t	sh_performance_counter1_regval;
	struct {
		mmr_t	reserved_0  : 32;
		mmr_t	count       : 32;
	} sh_performance_counter1_s;
} sh_performance_counter1_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PERFORMANCE_COUNTER2"                  */
/*                        Performance Counter 2                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_performance_counter2_u {
	mmr_t	sh_performance_counter2_regval;
	struct {
		mmr_t	count       : 32;
		mmr_t	reserved_0  : 32;
	} sh_performance_counter2_s;
} sh_performance_counter2_u_t;
#else
typedef union sh_performance_counter2_u {
	mmr_t	sh_performance_counter2_regval;
	struct {
		mmr_t	reserved_0  : 32;
		mmr_t	count       : 32;
	} sh_performance_counter2_s;
} sh_performance_counter2_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PERFORMANCE_COUNTER3"                  */
/*                        Performance Counter 3                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_performance_counter3_u {
	mmr_t	sh_performance_counter3_regval;
	struct {
		mmr_t	count       : 32;
		mmr_t	reserved_0  : 32;
	} sh_performance_counter3_s;
} sh_performance_counter3_u_t;
#else
typedef union sh_performance_counter3_u {
	mmr_t	sh_performance_counter3_regval;
	struct {
		mmr_t	reserved_0  : 32;
		mmr_t	count       : 32;
	} sh_performance_counter3_s;
} sh_performance_counter3_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PERFORMANCE_COUNTER4"                  */
/*                        Performance Counter 4                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_performance_counter4_u {
	mmr_t	sh_performance_counter4_regval;
	struct {
		mmr_t	count       : 32;
		mmr_t	reserved_0  : 32;
	} sh_performance_counter4_s;
} sh_performance_counter4_u_t;
#else
typedef union sh_performance_counter4_u {
	mmr_t	sh_performance_counter4_regval;
	struct {
		mmr_t	reserved_0  : 32;
		mmr_t	count       : 32;
	} sh_performance_counter4_s;
} sh_performance_counter4_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PERFORMANCE_COUNTER5"                  */
/*                        Performance Counter 5                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_performance_counter5_u {
	mmr_t	sh_performance_counter5_regval;
	struct {
		mmr_t	count       : 32;
		mmr_t	reserved_0  : 32;
	} sh_performance_counter5_s;
} sh_performance_counter5_u_t;
#else
typedef union sh_performance_counter5_u {
	mmr_t	sh_performance_counter5_regval;
	struct {
		mmr_t	reserved_0  : 32;
		mmr_t	count       : 32;
	} sh_performance_counter5_s;
} sh_performance_counter5_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PERFORMANCE_COUNTER6"                  */
/*                        Performance Counter 6                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_performance_counter6_u {
	mmr_t	sh_performance_counter6_regval;
	struct {
		mmr_t	count       : 32;
		mmr_t	reserved_0  : 32;
	} sh_performance_counter6_s;
} sh_performance_counter6_u_t;
#else
typedef union sh_performance_counter6_u {
	mmr_t	sh_performance_counter6_regval;
	struct {
		mmr_t	reserved_0  : 32;
		mmr_t	count       : 32;
	} sh_performance_counter6_s;
} sh_performance_counter6_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_PERFORMANCE_COUNTER7"                  */
/*                        Performance Counter 7                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_performance_counter7_u {
	mmr_t	sh_performance_counter7_regval;
	struct {
		mmr_t	count       : 32;
		mmr_t	reserved_0  : 32;
	} sh_performance_counter7_s;
} sh_performance_counter7_u_t;
#else
typedef union sh_performance_counter7_u {
	mmr_t	sh_performance_counter7_regval;
	struct {
		mmr_t	reserved_0  : 32;
		mmr_t	count       : 32;
	} sh_performance_counter7_s;
} sh_performance_counter7_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_PROFILE_COUNTER"                     */
/*                           Profile Counter                            */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_profile_counter_u {
	mmr_t	sh_profile_counter_regval;
	struct {
		mmr_t	counter     : 8;
		mmr_t	reserved_0  : 56;
	} sh_profile_counter_s;
} sh_profile_counter_u_t;
#else
typedef union sh_profile_counter_u {
	mmr_t	sh_profile_counter_regval;
	struct {
		mmr_t	reserved_0  : 56;
		mmr_t	counter     : 8;
	} sh_profile_counter_s;
} sh_profile_counter_u_t;
#endif

/* ==================================================================== */
/*                      Register "SH_PROFILE_PEAK"                      */
/*                         Profile Peak Counter                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_profile_peak_u {
	mmr_t	sh_profile_peak_regval;
	struct {
		mmr_t	counter     : 8;
		mmr_t	reserved_0  : 56;
	} sh_profile_peak_s;
} sh_profile_peak_u_t;
#else
typedef union sh_profile_peak_u {
	mmr_t	sh_profile_peak_regval;
	struct {
		mmr_t	reserved_0  : 56;
		mmr_t	counter     : 8;
	} sh_profile_peak_s;
} sh_profile_peak_u_t;
#endif

/* ==================================================================== */
/*                         Register "SH_PTC_0"                          */
/*       Puge Translation Cache Message Configuration Information       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ptc_0_u {
	mmr_t	sh_ptc_0_regval;
	struct {
		mmr_t	a           : 1;
		mmr_t	reserved_0  : 1;
		mmr_t	ps          : 6;
		mmr_t	rid         : 24;
		mmr_t	reserved_1  : 31;
		mmr_t	start       : 1;
	} sh_ptc_0_s;
} sh_ptc_0_u_t;
#else
typedef union sh_ptc_0_u {
	mmr_t	sh_ptc_0_regval;
	struct {
		mmr_t	start       : 1;
		mmr_t	reserved_1  : 31;
		mmr_t	rid         : 24;
		mmr_t	ps          : 6;
		mmr_t	reserved_0  : 1;
		mmr_t	a           : 1;
	} sh_ptc_0_s;
} sh_ptc_0_u_t;
#endif

/* ==================================================================== */
/*                         Register "SH_PTC_1"                          */
/*       Puge Translation Cache Message Configuration Information       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ptc_1_u {
	mmr_t	sh_ptc_1_regval;
	struct {
		mmr_t	reserved_0  : 12;
		mmr_t	vpn         : 49;
		mmr_t	reserved_1  : 2;
		mmr_t	start       : 1;
	} sh_ptc_1_s;
} sh_ptc_1_u_t;
#else
typedef union sh_ptc_1_u {
	mmr_t	sh_ptc_1_regval;
	struct {
		mmr_t	start       : 1;
		mmr_t	reserved_1  : 2;
		mmr_t	vpn         : 49;
		mmr_t	reserved_0  : 12;
	} sh_ptc_1_s;
} sh_ptc_1_u_t;
#endif

/* ==================================================================== */
/*                       Register "SH_PTC_PARMS"                        */
/*                       PTC Time-out parmaeters                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_ptc_parms_u {
	mmr_t	sh_ptc_parms_regval;
	struct {
		mmr_t	ptc_to_wrap : 24;
		mmr_t	ptc_to_val  : 12;
		mmr_t	reserved_0  : 28;
	} sh_ptc_parms_s;
} sh_ptc_parms_u_t;
#else
typedef union sh_ptc_parms_u {
	mmr_t	sh_ptc_parms_regval;
	struct {
		mmr_t	reserved_0  : 28;
		mmr_t	ptc_to_val  : 12;
		mmr_t	ptc_to_wrap : 24;
	} sh_ptc_parms_s;
} sh_ptc_parms_u_t;
#endif

/* ==================================================================== */
/*                        Register "SH_INT_CMPA"                        */
/*                  RTC Compare Value for Processor A                   */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_int_cmpa_u {
	mmr_t	sh_int_cmpa_regval;
	struct {
		mmr_t	real_time_cmpa : 55;
		mmr_t	reserved_0     : 9;
	} sh_int_cmpa_s;
} sh_int_cmpa_u_t;
#else
typedef union sh_int_cmpa_u {
	mmr_t	sh_int_cmpa_regval;
	struct {
		mmr_t	reserved_0     : 9;
		mmr_t	real_time_cmpa : 55;
	} sh_int_cmpa_s;
} sh_int_cmpa_u_t;
#endif

/* ==================================================================== */
/*                        Register "SH_INT_CMPB"                        */
/*                  RTC Compare Value for Processor B                   */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_int_cmpb_u {
	mmr_t	sh_int_cmpb_regval;
	struct {
		mmr_t	real_time_cmpb : 55;
		mmr_t	reserved_0     : 9;
	} sh_int_cmpb_s;
} sh_int_cmpb_u_t;
#else
typedef union sh_int_cmpb_u {
	mmr_t	sh_int_cmpb_regval;
	struct {
		mmr_t	reserved_0     : 9;
		mmr_t	real_time_cmpb : 55;
	} sh_int_cmpb_s;
} sh_int_cmpb_u_t;
#endif

/* ==================================================================== */
/*                        Register "SH_INT_CMPC"                        */
/*                  RTC Compare Value for Processor C                   */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_int_cmpc_u {
	mmr_t	sh_int_cmpc_regval;
	struct {
		mmr_t	real_time_cmpc : 55;
		mmr_t	reserved_0     : 9;
	} sh_int_cmpc_s;
} sh_int_cmpc_u_t;
#else
typedef union sh_int_cmpc_u {
	mmr_t	sh_int_cmpc_regval;
	struct {
		mmr_t	reserved_0     : 9;
		mmr_t	real_time_cmpc : 55;
	} sh_int_cmpc_s;
} sh_int_cmpc_u_t;
#endif

/* ==================================================================== */
/*                        Register "SH_INT_CMPD"                        */
/*                  RTC Compare Value for Processor D                   */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_int_cmpd_u {
	mmr_t	sh_int_cmpd_regval;
	struct {
		mmr_t	real_time_cmpd : 55;
		mmr_t	reserved_0     : 9;
	} sh_int_cmpd_s;
} sh_int_cmpd_u_t;
#else
typedef union sh_int_cmpd_u {
	mmr_t	sh_int_cmpd_regval;
	struct {
		mmr_t	reserved_0     : 9;
		mmr_t	real_time_cmpd : 55;
	} sh_int_cmpd_s;
} sh_int_cmpd_u_t;
#endif

/* ==================================================================== */
/*                        Register "SH_INT_PROF"                        */
/*                      Profile Compare Registers                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_int_prof_u {
	mmr_t	sh_int_prof_regval;
	struct {
		mmr_t	profile_compare : 32;
		mmr_t	reserved_0      : 32;
	} sh_int_prof_s;
} sh_int_prof_u_t;
#else
typedef union sh_int_prof_u {
	mmr_t	sh_int_prof_regval;
	struct {
		mmr_t	reserved_0      : 32;
		mmr_t	profile_compare : 32;
	} sh_int_prof_s;
} sh_int_prof_u_t;
#endif

/* ==================================================================== */
/*                          Register "SH_RTC"                           */
/*                           Real-time Clock                            */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_rtc_u {
	mmr_t	sh_rtc_regval;
	struct {
		mmr_t	real_time_clock : 55;
		mmr_t	reserved_0      : 9;
	} sh_rtc_s;
} sh_rtc_u_t;
#else
typedef union sh_rtc_u {
	mmr_t	sh_rtc_regval;
	struct {
		mmr_t	reserved_0      : 9;
		mmr_t	real_time_clock : 55;
	} sh_rtc_s;
} sh_rtc_u_t;
#endif

/* ==================================================================== */
/*                        Register "SH_SCRATCH0"                        */
/*                          Scratch Register 0                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_scratch0_u {
	mmr_t	sh_scratch0_regval;
	struct {
		mmr_t	scratch0    : 64;
	} sh_scratch0_s;
} sh_scratch0_u_t;
#else
typedef union sh_scratch0_u {
	mmr_t	sh_scratch0_regval;
	struct {
		mmr_t	scratch0    : 64;
	} sh_scratch0_s;
} sh_scratch0_u_t;
#endif

/* ==================================================================== */
/*                        Register "SH_SCRATCH1"                        */
/*                          Scratch Register 1                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_scratch1_u {
	mmr_t	sh_scratch1_regval;
	struct {
		mmr_t	scratch1    : 64;
	} sh_scratch1_s;
} sh_scratch1_u_t;
#else
typedef union sh_scratch1_u {
	mmr_t	sh_scratch1_regval;
	struct {
		mmr_t	scratch1    : 64;
	} sh_scratch1_s;
} sh_scratch1_u_t;
#endif

/* ==================================================================== */
/*                        Register "SH_SCRATCH2"                        */
/*                          Scratch Register 2                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_scratch2_u {
	mmr_t	sh_scratch2_regval;
	struct {
		mmr_t	scratch2    : 64;
	} sh_scratch2_s;
} sh_scratch2_u_t;
#else
typedef union sh_scratch2_u {
	mmr_t	sh_scratch2_regval;
	struct {
		mmr_t	scratch2    : 64;
	} sh_scratch2_s;
} sh_scratch2_u_t;
#endif

/* ==================================================================== */
/*                        Register "SH_SCRATCH3"                        */
/*                          Scratch Register 3                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_scratch3_u {
	mmr_t	sh_scratch3_regval;
	struct {
		mmr_t	scratch3    : 1;
		mmr_t	reserved_0  : 63;
	} sh_scratch3_s;
} sh_scratch3_u_t;
#else
typedef union sh_scratch3_u {
	mmr_t	sh_scratch3_regval;
	struct {
		mmr_t	reserved_0  : 63;
		mmr_t	scratch3    : 1;
	} sh_scratch3_s;
} sh_scratch3_u_t;
#endif

/* ==================================================================== */
/*                        Register "SH_SCRATCH4"                        */
/*                          Scratch Register 4                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_scratch4_u {
	mmr_t	sh_scratch4_regval;
	struct {
		mmr_t	scratch4    : 1;
		mmr_t	reserved_0  : 63;
	} sh_scratch4_s;
} sh_scratch4_u_t;
#else
typedef union sh_scratch4_u {
	mmr_t	sh_scratch4_regval;
	struct {
		mmr_t	reserved_0  : 63;
		mmr_t	scratch4    : 1;
	} sh_scratch4_s;
} sh_scratch4_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_CRB_MESSAGE_CONTROL"                   */
/*               Coherent Request Buffer Message Control                */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_crb_message_control_u {
	mmr_t	sh_crb_message_control_regval;
	struct {
		mmr_t	system_coherence_enable           : 1;
		mmr_t	local_speculative_message_enable  : 1;
		mmr_t	remote_speculative_message_enable : 1;
		mmr_t	message_color                     : 1;
		mmr_t	message_color_enable              : 1;
		mmr_t	rrb_attribute_mismatch_fsb_enable : 1;
		mmr_t	wrb_attribute_mismatch_fsb_enable : 1;
		mmr_t	irb_attribute_mismatch_fsb_enable : 1;
		mmr_t	rrb_attribute_mismatch_xb_enable  : 1;
		mmr_t	wrb_attribute_mismatch_xb_enable  : 1;
		mmr_t	suppress_bogus_writes             : 1;
		mmr_t	enable_ivack_consolidation        : 1;
		mmr_t	reserved_0                        : 20;
		mmr_t	ivack_stall_count                 : 16;
		mmr_t	ivack_throttle_control            : 16;
	} sh_crb_message_control_s;
} sh_crb_message_control_u_t;
#else
typedef union sh_crb_message_control_u {
	mmr_t	sh_crb_message_control_regval;
	struct {
		mmr_t	ivack_throttle_control            : 16;
		mmr_t	ivack_stall_count                 : 16;
		mmr_t	reserved_0                        : 20;
		mmr_t	enable_ivack_consolidation        : 1;
		mmr_t	suppress_bogus_writes             : 1;
		mmr_t	wrb_attribute_mismatch_xb_enable  : 1;
		mmr_t	rrb_attribute_mismatch_xb_enable  : 1;
		mmr_t	irb_attribute_mismatch_fsb_enable : 1;
		mmr_t	wrb_attribute_mismatch_fsb_enable : 1;
		mmr_t	rrb_attribute_mismatch_fsb_enable : 1;
		mmr_t	message_color_enable              : 1;
		mmr_t	message_color                     : 1;
		mmr_t	remote_speculative_message_enable : 1;
		mmr_t	local_speculative_message_enable  : 1;
		mmr_t	system_coherence_enable           : 1;
	} sh_crb_message_control_s;
} sh_crb_message_control_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_CRB_NACK_LIMIT"                     */
/*                            CRB Nack Limit                            */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_crb_nack_limit_u {
	mmr_t	sh_crb_nack_limit_regval;
	struct {
		mmr_t	limit       : 12;
		mmr_t	pri_freq    : 4;
		mmr_t	reserved_0  : 47;
		mmr_t	enable      : 1;
	} sh_crb_nack_limit_s;
} sh_crb_nack_limit_u_t;
#else
typedef union sh_crb_nack_limit_u {
	mmr_t	sh_crb_nack_limit_regval;
	struct {
		mmr_t	enable      : 1;
		mmr_t	reserved_0  : 47;
		mmr_t	pri_freq    : 4;
		mmr_t	limit       : 12;
	} sh_crb_nack_limit_s;
} sh_crb_nack_limit_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_CRB_TIMEOUT_PRESCALE"                  */
/*               Coherent Request Buffer Timeout Prescale               */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_crb_timeout_prescale_u {
	mmr_t	sh_crb_timeout_prescale_regval;
	struct {
		mmr_t	scaling_factor : 32;
		mmr_t	reserved_0     : 32;
	} sh_crb_timeout_prescale_s;
} sh_crb_timeout_prescale_u_t;
#else
typedef union sh_crb_timeout_prescale_u {
	mmr_t	sh_crb_timeout_prescale_regval;
	struct {
		mmr_t	reserved_0     : 32;
		mmr_t	scaling_factor : 32;
	} sh_crb_timeout_prescale_s;
} sh_crb_timeout_prescale_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_CRB_TIMEOUT_SKID"                    */
/*              Coherent Request Buffer Timeout Skid Limit              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_crb_timeout_skid_u {
	mmr_t	sh_crb_timeout_skid_regval;
	struct {
		mmr_t	skid             : 6;
		mmr_t	reserved_0       : 57;
		mmr_t	reset_skid_count : 1;
	} sh_crb_timeout_skid_s;
} sh_crb_timeout_skid_u_t;
#else
typedef union sh_crb_timeout_skid_u {
	mmr_t	sh_crb_timeout_skid_regval;
	struct {
		mmr_t	reset_skid_count : 1;
		mmr_t	reserved_0       : 57;
		mmr_t	skid             : 6;
	} sh_crb_timeout_skid_s;
} sh_crb_timeout_skid_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_MEMORY_WRITE_STATUS_0"                  */
/*                    Memory Write Status for CPU 0                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_memory_write_status_0_u {
	mmr_t	sh_memory_write_status_0_regval;
	struct {
		mmr_t	pending_write_count : 6;
		mmr_t	reserved_0          : 58;
	} sh_memory_write_status_0_s;
} sh_memory_write_status_0_u_t;
#else
typedef union sh_memory_write_status_0_u {
	mmr_t	sh_memory_write_status_0_regval;
	struct {
		mmr_t	reserved_0          : 58;
		mmr_t	pending_write_count : 6;
	} sh_memory_write_status_0_s;
} sh_memory_write_status_0_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_MEMORY_WRITE_STATUS_1"                  */
/*                    Memory Write Status for CPU 1                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_memory_write_status_1_u {
	mmr_t	sh_memory_write_status_1_regval;
	struct {
		mmr_t	pending_write_count : 6;
		mmr_t	reserved_0          : 58;
	} sh_memory_write_status_1_s;
} sh_memory_write_status_1_u_t;
#else
typedef union sh_memory_write_status_1_u {
	mmr_t	sh_memory_write_status_1_regval;
	struct {
		mmr_t	reserved_0          : 58;
		mmr_t	pending_write_count : 6;
	} sh_memory_write_status_1_s;
} sh_memory_write_status_1_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_PIO_WRITE_STATUS_0"                   */
/*                      PIO Write Status for CPU 0                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pio_write_status_0_u {
	mmr_t	sh_pio_write_status_0_regval;
	struct {
		mmr_t	multi_write_error   : 1;
		mmr_t	write_deadlock      : 1;
		mmr_t	write_error         : 1;
		mmr_t	write_error_address : 47;
		mmr_t	reserved_0          : 6;
		mmr_t	pending_write_count : 6;
		mmr_t	reserved_1          : 1;
		mmr_t	writes_ok           : 1;
	} sh_pio_write_status_0_s;
} sh_pio_write_status_0_u_t;
#else
typedef union sh_pio_write_status_0_u {
	mmr_t	sh_pio_write_status_0_regval;
	struct {
		mmr_t	writes_ok           : 1;
		mmr_t	reserved_1          : 1;
		mmr_t	pending_write_count : 6;
		mmr_t	reserved_0          : 6;
		mmr_t	write_error_address : 47;
		mmr_t	write_error         : 1;
		mmr_t	write_deadlock      : 1;
		mmr_t	multi_write_error   : 1;
	} sh_pio_write_status_0_s;
} sh_pio_write_status_0_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_PIO_WRITE_STATUS_1"                   */
/*                      PIO Write Status for CPU 1                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pio_write_status_1_u {
	mmr_t	sh_pio_write_status_1_regval;
	struct {
		mmr_t	multi_write_error   : 1;
		mmr_t	write_deadlock      : 1;
		mmr_t	write_error         : 1;
		mmr_t	write_error_address : 47;
		mmr_t	reserved_0          : 6;
		mmr_t	pending_write_count : 6;
		mmr_t	reserved_1          : 1;
		mmr_t	writes_ok           : 1;
	} sh_pio_write_status_1_s;
} sh_pio_write_status_1_u_t;
#else
typedef union sh_pio_write_status_1_u {
	mmr_t	sh_pio_write_status_1_regval;
	struct {
		mmr_t	writes_ok           : 1;
		mmr_t	reserved_1          : 1;
		mmr_t	pending_write_count : 6;
		mmr_t	reserved_0          : 6;
		mmr_t	write_error_address : 47;
		mmr_t	write_error         : 1;
		mmr_t	write_deadlock      : 1;
		mmr_t	multi_write_error   : 1;
	} sh_pio_write_status_1_s;
} sh_pio_write_status_1_u_t;
#endif

/* ==================================================================== */
/*             Register "SH_MEMORY_WRITE_STATUS_NON_USER_0"             */
/*            Memory Write Status for CPU 0. OS access only             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_memory_write_status_non_user_0_u {
	mmr_t	sh_memory_write_status_non_user_0_regval;
	struct {
		mmr_t	pending_write_count : 6;
		mmr_t	reserved_0          : 57;
		mmr_t	clear               : 1;
	} sh_memory_write_status_non_user_0_s;
} sh_memory_write_status_non_user_0_u_t;
#else
typedef union sh_memory_write_status_non_user_0_u {
	mmr_t	sh_memory_write_status_non_user_0_regval;
	struct {
		mmr_t	clear               : 1;
		mmr_t	reserved_0          : 57;
		mmr_t	pending_write_count : 6;
	} sh_memory_write_status_non_user_0_s;
} sh_memory_write_status_non_user_0_u_t;
#endif

/* ==================================================================== */
/*             Register "SH_MEMORY_WRITE_STATUS_NON_USER_1"             */
/*            Memory Write Status for CPU 1. OS access only             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_memory_write_status_non_user_1_u {
	mmr_t	sh_memory_write_status_non_user_1_regval;
	struct {
		mmr_t	pending_write_count : 6;
		mmr_t	reserved_0          : 57;
		mmr_t	clear               : 1;
	} sh_memory_write_status_non_user_1_s;
} sh_memory_write_status_non_user_1_u_t;
#else
typedef union sh_memory_write_status_non_user_1_u {
	mmr_t	sh_memory_write_status_non_user_1_regval;
	struct {
		mmr_t	clear               : 1;
		mmr_t	reserved_0          : 57;
		mmr_t	pending_write_count : 6;
	} sh_memory_write_status_non_user_1_s;
} sh_memory_write_status_non_user_1_u_t;
#endif

/* ==================================================================== */
/*                      Register "SH_MMRBIST_ERR"                       */
/*                  Error capture for bist read errors                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_mmrbist_err_u {
	mmr_t	sh_mmrbist_err_regval;
	struct {
		mmr_t	addr              : 33;
		mmr_t	reserved_0        : 3;
		mmr_t	detected          : 1;
		mmr_t	multiple_detected : 1;
		mmr_t	cancelled         : 1;
		mmr_t	reserved_1        : 25;
	} sh_mmrbist_err_s;
} sh_mmrbist_err_u_t;
#else
typedef union sh_mmrbist_err_u {
	mmr_t	sh_mmrbist_err_regval;
	struct {
		mmr_t	reserved_1        : 25;
		mmr_t	cancelled         : 1;
		mmr_t	multiple_detected : 1;
		mmr_t	detected          : 1;
		mmr_t	reserved_0        : 3;
		mmr_t	addr              : 33;
	} sh_mmrbist_err_s;
} sh_mmrbist_err_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_MISC_ERR_HDR_LOWER"                   */
/*                       Header capture register                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_misc_err_hdr_lower_u {
	mmr_t	sh_misc_err_hdr_lower_regval;
	struct {
		mmr_t	reserved_0  : 3;
		mmr_t	addr        : 33;
		mmr_t	cmd         : 8;
		mmr_t	src         : 14;
		mmr_t	reserved_1  : 2;
		mmr_t	write       : 1;
		mmr_t	reserved_2  : 2;
		mmr_t	valid       : 1;
	} sh_misc_err_hdr_lower_s;
} sh_misc_err_hdr_lower_u_t;
#else
typedef union sh_misc_err_hdr_lower_u {
	mmr_t	sh_misc_err_hdr_lower_regval;
	struct {
		mmr_t	valid       : 1;
		mmr_t	reserved_2  : 2;
		mmr_t	write       : 1;
		mmr_t	reserved_1  : 2;
		mmr_t	src         : 14;
		mmr_t	cmd         : 8;
		mmr_t	addr        : 33;
		mmr_t	reserved_0  : 3;
	} sh_misc_err_hdr_lower_s;
} sh_misc_err_hdr_lower_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_MISC_ERR_HDR_UPPER"                   */
/*           Error header capture packet and protocol errors            */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_misc_err_hdr_upper_u {
	mmr_t	sh_misc_err_hdr_upper_regval;
	struct {
		mmr_t	dir_protocol  : 1;
		mmr_t	illegal_cmd   : 1;
		mmr_t	nonexist_addr : 1;
		mmr_t	rmw_uc        : 1;
		mmr_t	rmw_cor       : 1;
		mmr_t	dir_acc       : 1;
		mmr_t	pi_pkt_size   : 1;
		mmr_t	xn_pkt_size   : 1;
		mmr_t	reserved_0    : 12;
		mmr_t	echo          : 9;
		mmr_t	reserved_1    : 35;
	} sh_misc_err_hdr_upper_s;
} sh_misc_err_hdr_upper_u_t;
#else
typedef union sh_misc_err_hdr_upper_u {
	mmr_t	sh_misc_err_hdr_upper_regval;
	struct {
		mmr_t	reserved_1    : 35;
		mmr_t	echo          : 9;
		mmr_t	reserved_0    : 12;
		mmr_t	xn_pkt_size   : 1;
		mmr_t	pi_pkt_size   : 1;
		mmr_t	dir_acc       : 1;
		mmr_t	rmw_cor       : 1;
		mmr_t	rmw_uc        : 1;
		mmr_t	nonexist_addr : 1;
		mmr_t	illegal_cmd   : 1;
		mmr_t	dir_protocol  : 1;
	} sh_misc_err_hdr_upper_s;
} sh_misc_err_hdr_upper_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_DIR_UC_ERR_HDR_LOWER"                  */
/*                       Header capture register                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_dir_uc_err_hdr_lower_u {
	mmr_t	sh_dir_uc_err_hdr_lower_regval;
	struct {
		mmr_t	reserved_0  : 3;
		mmr_t	addr        : 33;
		mmr_t	cmd         : 8;
		mmr_t	src         : 14;
		mmr_t	reserved_1  : 2;
		mmr_t	write       : 1;
		mmr_t	reserved_2  : 2;
		mmr_t	valid       : 1;
	} sh_dir_uc_err_hdr_lower_s;
} sh_dir_uc_err_hdr_lower_u_t;
#else
typedef union sh_dir_uc_err_hdr_lower_u {
	mmr_t	sh_dir_uc_err_hdr_lower_regval;
	struct {
		mmr_t	valid       : 1;
		mmr_t	reserved_2  : 2;
		mmr_t	write       : 1;
		mmr_t	reserved_1  : 2;
		mmr_t	src         : 14;
		mmr_t	cmd         : 8;
		mmr_t	addr        : 33;
		mmr_t	reserved_0  : 3;
	} sh_dir_uc_err_hdr_lower_s;
} sh_dir_uc_err_hdr_lower_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_DIR_UC_ERR_HDR_UPPER"                  */
/*           Error header capture packet and protocol errors            */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_dir_uc_err_hdr_upper_u {
	mmr_t	sh_dir_uc_err_hdr_upper_regval;
	struct {
		mmr_t	reserved_0  : 3;
		mmr_t	dir_uc      : 1;
		mmr_t	reserved_1  : 16;
		mmr_t	echo        : 9;
		mmr_t	reserved_2  : 35;
	} sh_dir_uc_err_hdr_upper_s;
} sh_dir_uc_err_hdr_upper_u_t;
#else
typedef union sh_dir_uc_err_hdr_upper_u {
	mmr_t	sh_dir_uc_err_hdr_upper_regval;
	struct {
		mmr_t	reserved_2  : 35;
		mmr_t	echo        : 9;
		mmr_t	reserved_1  : 16;
		mmr_t	dir_uc      : 1;
		mmr_t	reserved_0  : 3;
	} sh_dir_uc_err_hdr_upper_s;
} sh_dir_uc_err_hdr_upper_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_DIR_COR_ERR_HDR_LOWER"                  */
/*                       Header capture register                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_dir_cor_err_hdr_lower_u {
	mmr_t	sh_dir_cor_err_hdr_lower_regval;
	struct {
		mmr_t	reserved_0  : 3;
		mmr_t	addr        : 33;
		mmr_t	cmd         : 8;
		mmr_t	src         : 14;
		mmr_t	reserved_1  : 2;
		mmr_t	write       : 1;
		mmr_t	reserved_2  : 2;
		mmr_t	valid       : 1;
	} sh_dir_cor_err_hdr_lower_s;
} sh_dir_cor_err_hdr_lower_u_t;
#else
typedef union sh_dir_cor_err_hdr_lower_u {
	mmr_t	sh_dir_cor_err_hdr_lower_regval;
	struct {
		mmr_t	valid       : 1;
		mmr_t	reserved_2  : 2;
		mmr_t	write       : 1;
		mmr_t	reserved_1  : 2;
		mmr_t	src         : 14;
		mmr_t	cmd         : 8;
		mmr_t	addr        : 33;
		mmr_t	reserved_0  : 3;
	} sh_dir_cor_err_hdr_lower_s;
} sh_dir_cor_err_hdr_lower_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_DIR_COR_ERR_HDR_UPPER"                  */
/*           Error header capture packet and protocol errors            */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_dir_cor_err_hdr_upper_u {
	mmr_t	sh_dir_cor_err_hdr_upper_regval;
	struct {
		mmr_t	reserved_0  : 8;
		mmr_t	dir_cor     : 1;
		mmr_t	reserved_1  : 11;
		mmr_t	echo        : 9;
		mmr_t	reserved_2  : 35;
	} sh_dir_cor_err_hdr_upper_s;
} sh_dir_cor_err_hdr_upper_u_t;
#else
typedef union sh_dir_cor_err_hdr_upper_u {
	mmr_t	sh_dir_cor_err_hdr_upper_regval;
	struct {
		mmr_t	reserved_2  : 35;
		mmr_t	echo        : 9;
		mmr_t	reserved_1  : 11;
		mmr_t	dir_cor     : 1;
		mmr_t	reserved_0  : 8;
	} sh_dir_cor_err_hdr_upper_s;
} sh_dir_cor_err_hdr_upper_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_MEM_ERROR_SUMMARY"                    */
/*                          Memory error flags                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_mem_error_summary_u {
	mmr_t	sh_mem_error_summary_regval;
	struct {
		mmr_t	illegal_cmd           : 1;
		mmr_t	nonexist_addr         : 1;
		mmr_t	dqlp_dir_perr         : 1;
		mmr_t	dqrp_dir_perr         : 1;
		mmr_t	dqlp_dir_uc           : 1;
		mmr_t	dqlp_dir_cor          : 1;
		mmr_t	dqrp_dir_uc           : 1;
		mmr_t	dqrp_dir_cor          : 1;
		mmr_t	acx_int_hw            : 1;
		mmr_t	acy_int_hw            : 1;
		mmr_t	dir_acc               : 1;
		mmr_t	reserved_0            : 1;
		mmr_t	dqlp_int_uc           : 1;
		mmr_t	dqlp_int_cor          : 1;
		mmr_t	dqlp_int_hw           : 1;
		mmr_t	reserved_1            : 1;
		mmr_t	dqls_int_uc           : 1;
		mmr_t	dqls_int_cor          : 1;
		mmr_t	dqls_int_hw           : 1;
		mmr_t	reserved_2            : 1;
		mmr_t	dqrp_int_uc           : 1;
		mmr_t	dqrp_int_cor          : 1;
		mmr_t	dqrp_int_hw           : 1;
		mmr_t	reserved_3            : 1;
		mmr_t	dqrs_int_uc           : 1;
		mmr_t	dqrs_int_cor          : 1;
		mmr_t	dqrs_int_hw           : 1;
		mmr_t	reserved_4            : 1;
		mmr_t	pi_reply_overflow     : 1;
		mmr_t	xn_reply_overflow     : 1;
		mmr_t	pi_request_overflow   : 1;
		mmr_t	xn_request_overflow   : 1;
		mmr_t	red_black_err_timeout : 1;
		mmr_t	pi_pkt_size           : 1;
		mmr_t	xn_pkt_size           : 1;
		mmr_t	reserved_5            : 29;
	} sh_mem_error_summary_s;
} sh_mem_error_summary_u_t;
#else
typedef union sh_mem_error_summary_u {
	mmr_t	sh_mem_error_summary_regval;
	struct {
		mmr_t	reserved_5            : 29;
		mmr_t	xn_pkt_size           : 1;
		mmr_t	pi_pkt_size           : 1;
		mmr_t	red_black_err_timeout : 1;
		mmr_t	xn_request_overflow   : 1;
		mmr_t	pi_request_overflow   : 1;
		mmr_t	xn_reply_overflow     : 1;
		mmr_t	pi_reply_overflow     : 1;
		mmr_t	reserved_4            : 1;
		mmr_t	dqrs_int_hw           : 1;
		mmr_t	dqrs_int_cor          : 1;
		mmr_t	dqrs_int_uc           : 1;
		mmr_t	reserved_3            : 1;
		mmr_t	dqrp_int_hw           : 1;
		mmr_t	dqrp_int_cor          : 1;
		mmr_t	dqrp_int_uc           : 1;
		mmr_t	reserved_2            : 1;
		mmr_t	dqls_int_hw           : 1;
		mmr_t	dqls_int_cor          : 1;
		mmr_t	dqls_int_uc           : 1;
		mmr_t	reserved_1            : 1;
		mmr_t	dqlp_int_hw           : 1;
		mmr_t	dqlp_int_cor          : 1;
		mmr_t	dqlp_int_uc           : 1;
		mmr_t	reserved_0            : 1;
		mmr_t	dir_acc               : 1;
		mmr_t	acy_int_hw            : 1;
		mmr_t	acx_int_hw            : 1;
		mmr_t	dqrp_dir_cor          : 1;
		mmr_t	dqrp_dir_uc           : 1;
		mmr_t	dqlp_dir_cor          : 1;
		mmr_t	dqlp_dir_uc           : 1;
		mmr_t	dqrp_dir_perr         : 1;
		mmr_t	dqlp_dir_perr         : 1;
		mmr_t	nonexist_addr         : 1;
		mmr_t	illegal_cmd           : 1;
	} sh_mem_error_summary_s;
} sh_mem_error_summary_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_MEM_ERROR_OVERFLOW"                   */
/*                          Memory error flags                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_mem_error_overflow_u {
	mmr_t	sh_mem_error_overflow_regval;
	struct {
		mmr_t	illegal_cmd           : 1;
		mmr_t	nonexist_addr         : 1;
		mmr_t	dqlp_dir_perr         : 1;
		mmr_t	dqrp_dir_perr         : 1;
		mmr_t	dqlp_dir_uc           : 1;
		mmr_t	dqlp_dir_cor          : 1;
		mmr_t	dqrp_dir_uc           : 1;
		mmr_t	dqrp_dir_cor          : 1;
		mmr_t	acx_int_hw            : 1;
		mmr_t	acy_int_hw            : 1;
		mmr_t	dir_acc               : 1;
		mmr_t	reserved_0            : 1;
		mmr_t	dqlp_int_uc           : 1;
		mmr_t	dqlp_int_cor          : 1;
		mmr_t	dqlp_int_hw           : 1;
		mmr_t	reserved_1            : 1;
		mmr_t	dqls_int_uc           : 1;
		mmr_t	dqls_int_cor          : 1;
		mmr_t	dqls_int_hw           : 1;
		mmr_t	reserved_2            : 1;
		mmr_t	dqrp_int_uc           : 1;
		mmr_t	dqrp_int_cor          : 1;
		mmr_t	dqrp_int_hw           : 1;
		mmr_t	reserved_3            : 1;
		mmr_t	dqrs_int_uc           : 1;
		mmr_t	dqrs_int_cor          : 1;
		mmr_t	dqrs_int_hw           : 1;
		mmr_t	reserved_4            : 1;
		mmr_t	pi_reply_overflow     : 1;
		mmr_t	xn_reply_overflow     : 1;
		mmr_t	pi_request_overflow   : 1;
		mmr_t	xn_request_overflow   : 1;
		mmr_t	red_black_err_timeout : 1;
		mmr_t	pi_pkt_size           : 1;
		mmr_t	xn_pkt_size           : 1;
		mmr_t	reserved_5            : 29;
	} sh_mem_error_overflow_s;
} sh_mem_error_overflow_u_t;
#else
typedef union sh_mem_error_overflow_u {
	mmr_t	sh_mem_error_overflow_regval;
	struct {
		mmr_t	reserved_5            : 29;
		mmr_t	xn_pkt_size           : 1;
		mmr_t	pi_pkt_size           : 1;
		mmr_t	red_black_err_timeout : 1;
		mmr_t	xn_request_overflow   : 1;
		mmr_t	pi_request_overflow   : 1;
		mmr_t	xn_reply_overflow     : 1;
		mmr_t	pi_reply_overflow     : 1;
		mmr_t	reserved_4            : 1;
		mmr_t	dqrs_int_hw           : 1;
		mmr_t	dqrs_int_cor          : 1;
		mmr_t	dqrs_int_uc           : 1;
		mmr_t	reserved_3            : 1;
		mmr_t	dqrp_int_hw           : 1;
		mmr_t	dqrp_int_cor          : 1;
		mmr_t	dqrp_int_uc           : 1;
		mmr_t	reserved_2            : 1;
		mmr_t	dqls_int_hw           : 1;
		mmr_t	dqls_int_cor          : 1;
		mmr_t	dqls_int_uc           : 1;
		mmr_t	reserved_1            : 1;
		mmr_t	dqlp_int_hw           : 1;
		mmr_t	dqlp_int_cor          : 1;
		mmr_t	dqlp_int_uc           : 1;
		mmr_t	reserved_0            : 1;
		mmr_t	dir_acc               : 1;
		mmr_t	acy_int_hw            : 1;
		mmr_t	acx_int_hw            : 1;
		mmr_t	dqrp_dir_cor          : 1;
		mmr_t	dqrp_dir_uc           : 1;
		mmr_t	dqlp_dir_cor          : 1;
		mmr_t	dqlp_dir_uc           : 1;
		mmr_t	dqrp_dir_perr         : 1;
		mmr_t	dqlp_dir_perr         : 1;
		mmr_t	nonexist_addr         : 1;
		mmr_t	illegal_cmd           : 1;
	} sh_mem_error_overflow_s;
} sh_mem_error_overflow_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_MEM_ERROR_MASK"                     */
/*                          Memory error flags                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_mem_error_mask_u {
	mmr_t	sh_mem_error_mask_regval;
	struct {
		mmr_t	illegal_cmd           : 1;
		mmr_t	nonexist_addr         : 1;
		mmr_t	dqlp_dir_perr         : 1;
		mmr_t	dqrp_dir_perr         : 1;
		mmr_t	dqlp_dir_uc           : 1;
		mmr_t	dqlp_dir_cor          : 1;
		mmr_t	dqrp_dir_uc           : 1;
		mmr_t	dqrp_dir_cor          : 1;
		mmr_t	acx_int_hw            : 1;
		mmr_t	acy_int_hw            : 1;
		mmr_t	dir_acc               : 1;
		mmr_t	reserved_0            : 1;
		mmr_t	dqlp_int_uc           : 1;
		mmr_t	dqlp_int_cor          : 1;
		mmr_t	dqlp_int_hw           : 1;
		mmr_t	reserved_1            : 1;
		mmr_t	dqls_int_uc           : 1;
		mmr_t	dqls_int_cor          : 1;
		mmr_t	dqls_int_hw           : 1;
		mmr_t	reserved_2            : 1;
		mmr_t	dqrp_int_uc           : 1;
		mmr_t	dqrp_int_cor          : 1;
		mmr_t	dqrp_int_hw           : 1;
		mmr_t	reserved_3            : 1;
		mmr_t	dqrs_int_uc           : 1;
		mmr_t	dqrs_int_cor          : 1;
		mmr_t	dqrs_int_hw           : 1;
		mmr_t	reserved_4            : 1;
		mmr_t	pi_reply_overflow     : 1;
		mmr_t	xn_reply_overflow     : 1;
		mmr_t	pi_request_overflow   : 1;
		mmr_t	xn_request_overflow   : 1;
		mmr_t	red_black_err_timeout : 1;
		mmr_t	pi_pkt_size           : 1;
		mmr_t	xn_pkt_size           : 1;
		mmr_t	reserved_5            : 29;
	} sh_mem_error_mask_s;
} sh_mem_error_mask_u_t;
#else
typedef union sh_mem_error_mask_u {
	mmr_t	sh_mem_error_mask_regval;
	struct {
		mmr_t	reserved_5            : 29;
		mmr_t	xn_pkt_size           : 1;
		mmr_t	pi_pkt_size           : 1;
		mmr_t	red_black_err_timeout : 1;
		mmr_t	xn_request_overflow   : 1;
		mmr_t	pi_request_overflow   : 1;
		mmr_t	xn_reply_overflow     : 1;
		mmr_t	pi_reply_overflow     : 1;
		mmr_t	reserved_4            : 1;
		mmr_t	dqrs_int_hw           : 1;
		mmr_t	dqrs_int_cor          : 1;
		mmr_t	dqrs_int_uc           : 1;
		mmr_t	reserved_3            : 1;
		mmr_t	dqrp_int_hw           : 1;
		mmr_t	dqrp_int_cor          : 1;
		mmr_t	dqrp_int_uc           : 1;
		mmr_t	reserved_2            : 1;
		mmr_t	dqls_int_hw           : 1;
		mmr_t	dqls_int_cor          : 1;
		mmr_t	dqls_int_uc           : 1;
		mmr_t	reserved_1            : 1;
		mmr_t	dqlp_int_hw           : 1;
		mmr_t	dqlp_int_cor          : 1;
		mmr_t	dqlp_int_uc           : 1;
		mmr_t	reserved_0            : 1;
		mmr_t	dir_acc               : 1;
		mmr_t	acy_int_hw            : 1;
		mmr_t	acx_int_hw            : 1;
		mmr_t	dqrp_dir_cor          : 1;
		mmr_t	dqrp_dir_uc           : 1;
		mmr_t	dqlp_dir_cor          : 1;
		mmr_t	dqlp_dir_uc           : 1;
		mmr_t	dqrp_dir_perr         : 1;
		mmr_t	dqlp_dir_perr         : 1;
		mmr_t	nonexist_addr         : 1;
		mmr_t	illegal_cmd           : 1;
	} sh_mem_error_mask_s;
} sh_mem_error_mask_u_t;
#endif

/* ==================================================================== */
/*                       Register "SH_X_DIMM_CFG"                       */
/*                       AC Mem Config Registers                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_x_dimm_cfg_u {
	mmr_t	sh_x_dimm_cfg_regval;
	struct {
		mmr_t	dimm0_size  : 3;
		mmr_t	dimm0_2bk   : 1;
		mmr_t	dimm0_rev   : 1;
		mmr_t	dimm0_cs    : 2;
		mmr_t	reserved_0  : 1;
		mmr_t	dimm1_size  : 3;
		mmr_t	dimm1_2bk   : 1;
		mmr_t	dimm1_rev   : 1;
		mmr_t	dimm1_cs    : 2;
		mmr_t	reserved_1  : 1;
		mmr_t	dimm2_size  : 3;
		mmr_t	dimm2_2bk   : 1;
		mmr_t	dimm2_rev   : 1;
		mmr_t	dimm2_cs    : 2;
		mmr_t	reserved_2  : 1;
		mmr_t	dimm3_size  : 3;
		mmr_t	dimm3_2bk   : 1;
		mmr_t	dimm3_rev   : 1;
		mmr_t	dimm3_cs    : 2;
		mmr_t	reserved_3  : 1;
		mmr_t	freq        : 4;
		mmr_t	reserved_4  : 28;
	} sh_x_dimm_cfg_s;
} sh_x_dimm_cfg_u_t;
#else
typedef union sh_x_dimm_cfg_u {
	mmr_t	sh_x_dimm_cfg_regval;
	struct {
		mmr_t	reserved_4  : 28;
		mmr_t	freq        : 4;
		mmr_t	reserved_3  : 1;
		mmr_t	dimm3_cs    : 2;
		mmr_t	dimm3_rev   : 1;
		mmr_t	dimm3_2bk   : 1;
		mmr_t	dimm3_size  : 3;
		mmr_t	reserved_2  : 1;
		mmr_t	dimm2_cs    : 2;
		mmr_t	dimm2_rev   : 1;
		mmr_t	dimm2_2bk   : 1;
		mmr_t	dimm2_size  : 3;
		mmr_t	reserved_1  : 1;
		mmr_t	dimm1_cs    : 2;
		mmr_t	dimm1_rev   : 1;
		mmr_t	dimm1_2bk   : 1;
		mmr_t	dimm1_size  : 3;
		mmr_t	reserved_0  : 1;
		mmr_t	dimm0_cs    : 2;
		mmr_t	dimm0_rev   : 1;
		mmr_t	dimm0_2bk   : 1;
		mmr_t	dimm0_size  : 3;
	} sh_x_dimm_cfg_s;
} sh_x_dimm_cfg_u_t;
#endif

/* ==================================================================== */
/*                       Register "SH_Y_DIMM_CFG"                       */
/*                       AC Mem Config Registers                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_y_dimm_cfg_u {
	mmr_t	sh_y_dimm_cfg_regval;
	struct {
		mmr_t	dimm0_size  : 3;
		mmr_t	dimm0_2bk   : 1;
		mmr_t	dimm0_rev   : 1;
		mmr_t	dimm0_cs    : 2;
		mmr_t	reserved_0  : 1;
		mmr_t	dimm1_size  : 3;
		mmr_t	dimm1_2bk   : 1;
		mmr_t	dimm1_rev   : 1;
		mmr_t	dimm1_cs    : 2;
		mmr_t	reserved_1  : 1;
		mmr_t	dimm2_size  : 3;
		mmr_t	dimm2_2bk   : 1;
		mmr_t	dimm2_rev   : 1;
		mmr_t	dimm2_cs    : 2;
		mmr_t	reserved_2  : 1;
		mmr_t	dimm3_size  : 3;
		mmr_t	dimm3_2bk   : 1;
		mmr_t	dimm3_rev   : 1;
		mmr_t	dimm3_cs    : 2;
		mmr_t	reserved_3  : 1;
		mmr_t	freq        : 4;
		mmr_t	reserved_4  : 28;
	} sh_y_dimm_cfg_s;
} sh_y_dimm_cfg_u_t;
#else
typedef union sh_y_dimm_cfg_u {
	mmr_t	sh_y_dimm_cfg_regval;
	struct {
		mmr_t	reserved_4  : 28;
		mmr_t	freq        : 4;
		mmr_t	reserved_3  : 1;
		mmr_t	dimm3_cs    : 2;
		mmr_t	dimm3_rev   : 1;
		mmr_t	dimm3_2bk   : 1;
		mmr_t	dimm3_size  : 3;
		mmr_t	reserved_2  : 1;
		mmr_t	dimm2_cs    : 2;
		mmr_t	dimm2_rev   : 1;
		mmr_t	dimm2_2bk   : 1;
		mmr_t	dimm2_size  : 3;
		mmr_t	reserved_1  : 1;
		mmr_t	dimm1_cs    : 2;
		mmr_t	dimm1_rev   : 1;
		mmr_t	dimm1_2bk   : 1;
		mmr_t	dimm1_size  : 3;
		mmr_t	reserved_0  : 1;
		mmr_t	dimm0_cs    : 2;
		mmr_t	dimm0_rev   : 1;
		mmr_t	dimm0_2bk   : 1;
		mmr_t	dimm0_size  : 3;
	} sh_y_dimm_cfg_s;
} sh_y_dimm_cfg_u_t;
#endif

/* ==================================================================== */
/*                      Register "SH_JNR_DIMM_CFG"                      */
/*                       AC Mem Config Registers                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_jnr_dimm_cfg_u {
	mmr_t	sh_jnr_dimm_cfg_regval;
	struct {
		mmr_t	dimm0_size  : 3;
		mmr_t	dimm0_2bk   : 1;
		mmr_t	dimm0_rev   : 1;
		mmr_t	dimm0_cs    : 2;
		mmr_t	reserved_0  : 1;
		mmr_t	dimm1_size  : 3;
		mmr_t	dimm1_2bk   : 1;
		mmr_t	dimm1_rev   : 1;
		mmr_t	dimm1_cs    : 2;
		mmr_t	reserved_1  : 1;
		mmr_t	dimm2_size  : 3;
		mmr_t	dimm2_2bk   : 1;
		mmr_t	dimm2_rev   : 1;
		mmr_t	dimm2_cs    : 2;
		mmr_t	reserved_2  : 1;
		mmr_t	dimm3_size  : 3;
		mmr_t	dimm3_2bk   : 1;
		mmr_t	dimm3_rev   : 1;
		mmr_t	dimm3_cs    : 2;
		mmr_t	reserved_3  : 1;
		mmr_t	freq        : 4;
		mmr_t	reserved_4  : 28;
	} sh_jnr_dimm_cfg_s;
} sh_jnr_dimm_cfg_u_t;
#else
typedef union sh_jnr_dimm_cfg_u {
	mmr_t	sh_jnr_dimm_cfg_regval;
	struct {
		mmr_t	reserved_4  : 28;
		mmr_t	freq        : 4;
		mmr_t	reserved_3  : 1;
		mmr_t	dimm3_cs    : 2;
		mmr_t	dimm3_rev   : 1;
		mmr_t	dimm3_2bk   : 1;
		mmr_t	dimm3_size  : 3;
		mmr_t	reserved_2  : 1;
		mmr_t	dimm2_cs    : 2;
		mmr_t	dimm2_rev   : 1;
		mmr_t	dimm2_2bk   : 1;
		mmr_t	dimm2_size  : 3;
		mmr_t	reserved_1  : 1;
		mmr_t	dimm1_cs    : 2;
		mmr_t	dimm1_rev   : 1;
		mmr_t	dimm1_2bk   : 1;
		mmr_t	dimm1_size  : 3;
		mmr_t	reserved_0  : 1;
		mmr_t	dimm0_cs    : 2;
		mmr_t	dimm0_rev   : 1;
		mmr_t	dimm0_2bk   : 1;
		mmr_t	dimm0_size  : 3;
	} sh_jnr_dimm_cfg_s;
} sh_jnr_dimm_cfg_u_t;
#endif

/* ==================================================================== */
/*                      Register "SH_X_PHASE_CFG"                       */
/*                      AC Phase Config Registers                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_x_phase_cfg_u {
	mmr_t	sh_x_phase_cfg_regval;
	struct {
		mmr_t	ld_a        : 5;
		mmr_t	ld_b        : 5;
		mmr_t	dq_ld_a     : 5;
		mmr_t	dq_ld_b     : 5;
		mmr_t	hold        : 5;
		mmr_t	hold_req    : 5;
		mmr_t	add_cp      : 5;
		mmr_t	bubble_en   : 5;
		mmr_t	pha_bubble  : 3;
		mmr_t	phb_bubble  : 3;
		mmr_t	phc_bubble  : 3;
		mmr_t	phd_bubble  : 3;
		mmr_t	phe_bubble  : 3;
		mmr_t	sel_a       : 4;
		mmr_t	dq_sel_a    : 4;
		mmr_t	reserved_0  : 1;
	} sh_x_phase_cfg_s;
} sh_x_phase_cfg_u_t;
#else
typedef union sh_x_phase_cfg_u {
	mmr_t	sh_x_phase_cfg_regval;
	struct {
		mmr_t	reserved_0  : 1;
		mmr_t	dq_sel_a    : 4;
		mmr_t	sel_a       : 4;
		mmr_t	phe_bubble  : 3;
		mmr_t	phd_bubble  : 3;
		mmr_t	phc_bubble  : 3;
		mmr_t	phb_bubble  : 3;
		mmr_t	pha_bubble  : 3;
		mmr_t	bubble_en   : 5;
		mmr_t	add_cp      : 5;
		mmr_t	hold_req    : 5;
		mmr_t	hold        : 5;
		mmr_t	dq_ld_b     : 5;
		mmr_t	dq_ld_a     : 5;
		mmr_t	ld_b        : 5;
		mmr_t	ld_a        : 5;
	} sh_x_phase_cfg_s;
} sh_x_phase_cfg_u_t;
#endif

/* ==================================================================== */
/*                         Register "SH_X_CFG"                          */
/*                         AC Config Registers                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_x_cfg_u {
	mmr_t	sh_x_cfg_regval;
	struct {
		mmr_t	mode_serial             : 1;
		mmr_t	dirc_random_replacement : 1;
		mmr_t	dir_counter_init        : 6;
		mmr_t	ta_dlys                 : 32;
		mmr_t	da_bb_clr               : 4;
		mmr_t	dc_bb_clr               : 4;
		mmr_t	wt_bb_clr               : 4;
		mmr_t	sso_wt_en               : 1;
		mmr_t	trcd2_en                : 1;
		mmr_t	trcd4_en                : 1;
		mmr_t	req_cntr_dis            : 1;
		mmr_t	req_cntr_val            : 6;
		mmr_t	inv_cas_addr            : 1;
		mmr_t	clr_dir_cache           : 1;
	} sh_x_cfg_s;
} sh_x_cfg_u_t;
#else
typedef union sh_x_cfg_u {
	mmr_t	sh_x_cfg_regval;
	struct {
		mmr_t	clr_dir_cache           : 1;
		mmr_t	inv_cas_addr            : 1;
		mmr_t	req_cntr_val            : 6;
		mmr_t	req_cntr_dis            : 1;
		mmr_t	trcd4_en                : 1;
		mmr_t	trcd2_en                : 1;
		mmr_t	sso_wt_en               : 1;
		mmr_t	wt_bb_clr               : 4;
		mmr_t	dc_bb_clr               : 4;
		mmr_t	da_bb_clr               : 4;
		mmr_t	ta_dlys                 : 32;
		mmr_t	dir_counter_init        : 6;
		mmr_t	dirc_random_replacement : 1;
		mmr_t	mode_serial             : 1;
	} sh_x_cfg_s;
} sh_x_cfg_u_t;
#endif

/* ==================================================================== */
/*                       Register "SH_X_DQCT_CFG"                       */
/*                         AC Config Registers                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_x_dqct_cfg_u {
	mmr_t	sh_x_dqct_cfg_regval;
	struct {
		mmr_t	rd_sel      : 4;
		mmr_t	wt_sel      : 4;
		mmr_t	dta_rd_sel  : 4;
		mmr_t	dta_wt_sel  : 4;
		mmr_t	dir_rd_sel  : 4;
		mmr_t	mdir_rd_sel : 4;
		mmr_t	reserved_0  : 40;
	} sh_x_dqct_cfg_s;
} sh_x_dqct_cfg_u_t;
#else
typedef union sh_x_dqct_cfg_u {
	mmr_t	sh_x_dqct_cfg_regval;
	struct {
		mmr_t	reserved_0  : 40;
		mmr_t	mdir_rd_sel : 4;
		mmr_t	dir_rd_sel  : 4;
		mmr_t	dta_wt_sel  : 4;
		mmr_t	dta_rd_sel  : 4;
		mmr_t	wt_sel      : 4;
		mmr_t	rd_sel      : 4;
	} sh_x_dqct_cfg_s;
} sh_x_dqct_cfg_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_X_REFRESH_CONTROL"                    */
/*                       Refresh Control Register                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_x_refresh_control_u {
	mmr_t	sh_x_refresh_control_regval;
	struct {
		mmr_t	enable      : 8;
		mmr_t	interval    : 9;
		mmr_t	hold        : 6;
		mmr_t	interleave  : 1;
		mmr_t	half_rate   : 4;
		mmr_t	reserved_0  : 36;
	} sh_x_refresh_control_s;
} sh_x_refresh_control_u_t;
#else
typedef union sh_x_refresh_control_u {
	mmr_t	sh_x_refresh_control_regval;
	struct {
		mmr_t	reserved_0  : 36;
		mmr_t	half_rate   : 4;
		mmr_t	interleave  : 1;
		mmr_t	hold        : 6;
		mmr_t	interval    : 9;
		mmr_t	enable      : 8;
	} sh_x_refresh_control_s;
} sh_x_refresh_control_u_t;
#endif

/* ==================================================================== */
/*                      Register "SH_Y_PHASE_CFG"                       */
/*                      AC Phase Config Registers                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_y_phase_cfg_u {
	mmr_t	sh_y_phase_cfg_regval;
	struct {
		mmr_t	ld_a        : 5;
		mmr_t	ld_b        : 5;
		mmr_t	dq_ld_a     : 5;
		mmr_t	dq_ld_b     : 5;
		mmr_t	hold        : 5;
		mmr_t	hold_req    : 5;
		mmr_t	add_cp      : 5;
		mmr_t	bubble_en   : 5;
		mmr_t	pha_bubble  : 3;
		mmr_t	phb_bubble  : 3;
		mmr_t	phc_bubble  : 3;
		mmr_t	phd_bubble  : 3;
		mmr_t	phe_bubble  : 3;
		mmr_t	sel_a       : 4;
		mmr_t	dq_sel_a    : 4;
		mmr_t	reserved_0  : 1;
	} sh_y_phase_cfg_s;
} sh_y_phase_cfg_u_t;
#else
typedef union sh_y_phase_cfg_u {
	mmr_t	sh_y_phase_cfg_regval;
	struct {
		mmr_t	reserved_0  : 1;
		mmr_t	dq_sel_a    : 4;
		mmr_t	sel_a       : 4;
		mmr_t	phe_bubble  : 3;
		mmr_t	phd_bubble  : 3;
		mmr_t	phc_bubble  : 3;
		mmr_t	phb_bubble  : 3;
		mmr_t	pha_bubble  : 3;
		mmr_t	bubble_en   : 5;
		mmr_t	add_cp      : 5;
		mmr_t	hold_req    : 5;
		mmr_t	hold        : 5;
		mmr_t	dq_ld_b     : 5;
		mmr_t	dq_ld_a     : 5;
		mmr_t	ld_b        : 5;
		mmr_t	ld_a        : 5;
	} sh_y_phase_cfg_s;
} sh_y_phase_cfg_u_t;
#endif

/* ==================================================================== */
/*                         Register "SH_Y_CFG"                          */
/*                         AC Config Registers                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_y_cfg_u {
	mmr_t	sh_y_cfg_regval;
	struct {
		mmr_t	mode_serial             : 1;
		mmr_t	dirc_random_replacement : 1;
		mmr_t	dir_counter_init        : 6;
		mmr_t	ta_dlys                 : 32;
		mmr_t	da_bb_clr               : 4;
		mmr_t	dc_bb_clr               : 4;
		mmr_t	wt_bb_clr               : 4;
		mmr_t	sso_wt_en               : 1;
		mmr_t	trcd2_en                : 1;
		mmr_t	trcd4_en                : 1;
		mmr_t	req_cntr_dis            : 1;
		mmr_t	req_cntr_val            : 6;
		mmr_t	inv_cas_addr            : 1;
		mmr_t	clr_dir_cache           : 1;
	} sh_y_cfg_s;
} sh_y_cfg_u_t;
#else
typedef union sh_y_cfg_u {
	mmr_t	sh_y_cfg_regval;
	struct {
		mmr_t	clr_dir_cache           : 1;
		mmr_t	inv_cas_addr            : 1;
		mmr_t	req_cntr_val            : 6;
		mmr_t	req_cntr_dis            : 1;
		mmr_t	trcd4_en                : 1;
		mmr_t	trcd2_en                : 1;
		mmr_t	sso_wt_en               : 1;
		mmr_t	wt_bb_clr               : 4;
		mmr_t	dc_bb_clr               : 4;
		mmr_t	da_bb_clr               : 4;
		mmr_t	ta_dlys                 : 32;
		mmr_t	dir_counter_init        : 6;
		mmr_t	dirc_random_replacement : 1;
		mmr_t	mode_serial             : 1;
	} sh_y_cfg_s;
} sh_y_cfg_u_t;
#endif

/* ==================================================================== */
/*                       Register "SH_Y_DQCT_CFG"                       */
/*                         AC Config Registers                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_y_dqct_cfg_u {
	mmr_t	sh_y_dqct_cfg_regval;
	struct {
		mmr_t	rd_sel      : 4;
		mmr_t	wt_sel      : 4;
		mmr_t	dta_rd_sel  : 4;
		mmr_t	dta_wt_sel  : 4;
		mmr_t	dir_rd_sel  : 4;
		mmr_t	mdir_rd_sel : 4;
		mmr_t	reserved_0  : 40;
	} sh_y_dqct_cfg_s;
} sh_y_dqct_cfg_u_t;
#else
typedef union sh_y_dqct_cfg_u {
	mmr_t	sh_y_dqct_cfg_regval;
	struct {
		mmr_t	reserved_0  : 40;
		mmr_t	mdir_rd_sel : 4;
		mmr_t	dir_rd_sel  : 4;
		mmr_t	dta_wt_sel  : 4;
		mmr_t	dta_rd_sel  : 4;
		mmr_t	wt_sel      : 4;
		mmr_t	rd_sel      : 4;
	} sh_y_dqct_cfg_s;
} sh_y_dqct_cfg_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_Y_REFRESH_CONTROL"                    */
/*                       Refresh Control Register                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_y_refresh_control_u {
	mmr_t	sh_y_refresh_control_regval;
	struct {
		mmr_t	enable      : 8;
		mmr_t	interval    : 9;
		mmr_t	hold        : 6;
		mmr_t	interleave  : 1;
		mmr_t	half_rate   : 4;
		mmr_t	reserved_0  : 36;
	} sh_y_refresh_control_s;
} sh_y_refresh_control_u_t;
#else
typedef union sh_y_refresh_control_u {
	mmr_t	sh_y_refresh_control_regval;
	struct {
		mmr_t	reserved_0  : 36;
		mmr_t	half_rate   : 4;
		mmr_t	interleave  : 1;
		mmr_t	hold        : 6;
		mmr_t	interval    : 9;
		mmr_t	enable      : 8;
	} sh_y_refresh_control_s;
} sh_y_refresh_control_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_MEM_RED_BLACK"                      */
/*                     MD fairness watchdog timers                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_mem_red_black_u {
	mmr_t	sh_mem_red_black_regval;
	struct {
		mmr_t	time        : 16;
		mmr_t	err_time    : 36;
		mmr_t	reserved_0  : 12;
	} sh_mem_red_black_s;
} sh_mem_red_black_u_t;
#else
typedef union sh_mem_red_black_u {
	mmr_t	sh_mem_red_black_regval;
	struct {
		mmr_t	reserved_0  : 12;
		mmr_t	err_time    : 36;
		mmr_t	time        : 16;
	} sh_mem_red_black_s;
} sh_mem_red_black_u_t;
#endif

/* ==================================================================== */
/*                      Register "SH_MISC_MEM_CFG"                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_misc_mem_cfg_u {
	mmr_t	sh_misc_mem_cfg_regval;
	struct {
		mmr_t	express_header_enable       : 1;
		mmr_t	spec_header_enable          : 1;
		mmr_t	jnr_bypass_enable           : 1;
		mmr_t	xn_rd_same_as_pi            : 1;
		mmr_t	low_write_buffer_threshold  : 6;
		mmr_t	reserved_0                  : 2;
		mmr_t	low_victim_buffer_threshold : 6;
		mmr_t	reserved_1                  : 2;
		mmr_t	throttle_cnt                : 8;
		mmr_t	disabled_read_tnums         : 5;
		mmr_t	reserved_2                  : 3;
		mmr_t	disabled_write_tnums        : 5;
		mmr_t	reserved_3                  : 3;
		mmr_t	disabled_victims            : 6;
		mmr_t	reserved_4                  : 2;
		mmr_t	alternate_xn_rp_plane       : 1;
		mmr_t	reserved_5                  : 11;
	} sh_misc_mem_cfg_s;
} sh_misc_mem_cfg_u_t;
#else
typedef union sh_misc_mem_cfg_u {
	mmr_t	sh_misc_mem_cfg_regval;
	struct {
		mmr_t	reserved_5                  : 11;
		mmr_t	alternate_xn_rp_plane       : 1;
		mmr_t	reserved_4                  : 2;
		mmr_t	disabled_victims            : 6;
		mmr_t	reserved_3                  : 3;
		mmr_t	disabled_write_tnums        : 5;
		mmr_t	reserved_2                  : 3;
		mmr_t	disabled_read_tnums         : 5;
		mmr_t	throttle_cnt                : 8;
		mmr_t	reserved_1                  : 2;
		mmr_t	low_victim_buffer_threshold : 6;
		mmr_t	reserved_0                  : 2;
		mmr_t	low_write_buffer_threshold  : 6;
		mmr_t	xn_rd_same_as_pi            : 1;
		mmr_t	jnr_bypass_enable           : 1;
		mmr_t	spec_header_enable          : 1;
		mmr_t	express_header_enable       : 1;
	} sh_misc_mem_cfg_s;
} sh_misc_mem_cfg_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_PIO_RQ_CRD_CTL"                     */
/*                  pio_rq Credit Circulation Control                   */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pio_rq_crd_ctl_u {
	mmr_t	sh_pio_rq_crd_ctl_regval;
	struct {
		mmr_t	depth       : 6;
		mmr_t	reserved_0  : 58;
	} sh_pio_rq_crd_ctl_s;
} sh_pio_rq_crd_ctl_u_t;
#else
typedef union sh_pio_rq_crd_ctl_u {
	mmr_t	sh_pio_rq_crd_ctl_regval;
	struct {
		mmr_t	reserved_0  : 58;
		mmr_t	depth       : 6;
	} sh_pio_rq_crd_ctl_s;
} sh_pio_rq_crd_ctl_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_PI_MD_RQ_CRD_CTL"                    */
/*                 pi_md_rq Credit Circulation Control                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_md_rq_crd_ctl_u {
	mmr_t	sh_pi_md_rq_crd_ctl_regval;
	struct {
		mmr_t	depth       : 6;
		mmr_t	reserved_0  : 58;
	} sh_pi_md_rq_crd_ctl_s;
} sh_pi_md_rq_crd_ctl_u_t;
#else
typedef union sh_pi_md_rq_crd_ctl_u {
	mmr_t	sh_pi_md_rq_crd_ctl_regval;
	struct {
		mmr_t	reserved_0  : 58;
		mmr_t	depth       : 6;
	} sh_pi_md_rq_crd_ctl_s;
} sh_pi_md_rq_crd_ctl_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_PI_MD_RP_CRD_CTL"                    */
/*                 pi_md_rp Credit Circulation Control                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_pi_md_rp_crd_ctl_u {
	mmr_t	sh_pi_md_rp_crd_ctl_regval;
	struct {
		mmr_t	depth       : 6;
		mmr_t	reserved_0  : 58;
	} sh_pi_md_rp_crd_ctl_s;
} sh_pi_md_rp_crd_ctl_u_t;
#else
typedef union sh_pi_md_rp_crd_ctl_u {
	mmr_t	sh_pi_md_rp_crd_ctl_regval;
	struct {
		mmr_t	reserved_0  : 58;
		mmr_t	depth       : 6;
	} sh_pi_md_rp_crd_ctl_s;
} sh_pi_md_rp_crd_ctl_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_XN_MD_RQ_CRD_CTL"                    */
/*                 xn_md_rq Credit Circulation Control                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_md_rq_crd_ctl_u {
	mmr_t	sh_xn_md_rq_crd_ctl_regval;
	struct {
		mmr_t	depth       : 6;
		mmr_t	reserved_0  : 58;
	} sh_xn_md_rq_crd_ctl_s;
} sh_xn_md_rq_crd_ctl_u_t;
#else
typedef union sh_xn_md_rq_crd_ctl_u {
	mmr_t	sh_xn_md_rq_crd_ctl_regval;
	struct {
		mmr_t	reserved_0  : 58;
		mmr_t	depth       : 6;
	} sh_xn_md_rq_crd_ctl_s;
} sh_xn_md_rq_crd_ctl_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_XN_MD_RP_CRD_CTL"                    */
/*                 xn_md_rp Credit Circulation Control                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_xn_md_rp_crd_ctl_u {
	mmr_t	sh_xn_md_rp_crd_ctl_regval;
	struct {
		mmr_t	depth       : 6;
		mmr_t	reserved_0  : 58;
	} sh_xn_md_rp_crd_ctl_s;
} sh_xn_md_rp_crd_ctl_u_t;
#else
typedef union sh_xn_md_rp_crd_ctl_u {
	mmr_t	sh_xn_md_rp_crd_ctl_regval;
	struct {
		mmr_t	reserved_0  : 58;
		mmr_t	depth       : 6;
	} sh_xn_md_rp_crd_ctl_s;
} sh_xn_md_rp_crd_ctl_u_t;
#endif

/* ==================================================================== */
/*                         Register "SH_X_TAG0"                         */
/*                           AC tag Registers                           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_x_tag0_u {
	mmr_t	sh_x_tag0_regval;
	struct {
		mmr_t	tag         : 20;
		mmr_t	reserved_0  : 44;
	} sh_x_tag0_s;
} sh_x_tag0_u_t;
#else
typedef union sh_x_tag0_u {
	mmr_t	sh_x_tag0_regval;
	struct {
		mmr_t	reserved_0  : 44;
		mmr_t	tag         : 20;
	} sh_x_tag0_s;
} sh_x_tag0_u_t;
#endif

/* ==================================================================== */
/*                         Register "SH_X_TAG1"                         */
/*                           AC tag Registers                           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_x_tag1_u {
	mmr_t	sh_x_tag1_regval;
	struct {
		mmr_t	tag         : 20;
		mmr_t	reserved_0  : 44;
	} sh_x_tag1_s;
} sh_x_tag1_u_t;
#else
typedef union sh_x_tag1_u {
	mmr_t	sh_x_tag1_regval;
	struct {
		mmr_t	reserved_0  : 44;
		mmr_t	tag         : 20;
	} sh_x_tag1_s;
} sh_x_tag1_u_t;
#endif

/* ==================================================================== */
/*                         Register "SH_X_TAG2"                         */
/*                           AC tag Registers                           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_x_tag2_u {
	mmr_t	sh_x_tag2_regval;
	struct {
		mmr_t	tag         : 20;
		mmr_t	reserved_0  : 44;
	} sh_x_tag2_s;
} sh_x_tag2_u_t;
#else
typedef union sh_x_tag2_u {
	mmr_t	sh_x_tag2_regval;
	struct {
		mmr_t	reserved_0  : 44;
		mmr_t	tag         : 20;
	} sh_x_tag2_s;
} sh_x_tag2_u_t;
#endif

/* ==================================================================== */
/*                         Register "SH_X_TAG3"                         */
/*                           AC tag Registers                           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_x_tag3_u {
	mmr_t	sh_x_tag3_regval;
	struct {
		mmr_t	tag         : 20;
		mmr_t	reserved_0  : 44;
	} sh_x_tag3_s;
} sh_x_tag3_u_t;
#else
typedef union sh_x_tag3_u {
	mmr_t	sh_x_tag3_regval;
	struct {
		mmr_t	reserved_0  : 44;
		mmr_t	tag         : 20;
	} sh_x_tag3_s;
} sh_x_tag3_u_t;
#endif

/* ==================================================================== */
/*                         Register "SH_X_TAG4"                         */
/*                           AC tag Registers                           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_x_tag4_u {
	mmr_t	sh_x_tag4_regval;
	struct {
		mmr_t	tag         : 20;
		mmr_t	reserved_0  : 44;
	} sh_x_tag4_s;
} sh_x_tag4_u_t;
#else
typedef union sh_x_tag4_u {
	mmr_t	sh_x_tag4_regval;
	struct {
		mmr_t	reserved_0  : 44;
		mmr_t	tag         : 20;
	} sh_x_tag4_s;
} sh_x_tag4_u_t;
#endif

/* ==================================================================== */
/*                         Register "SH_X_TAG5"                         */
/*                           AC tag Registers                           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_x_tag5_u {
	mmr_t	sh_x_tag5_regval;
	struct {
		mmr_t	tag         : 20;
		mmr_t	reserved_0  : 44;
	} sh_x_tag5_s;
} sh_x_tag5_u_t;
#else
typedef union sh_x_tag5_u {
	mmr_t	sh_x_tag5_regval;
	struct {
		mmr_t	reserved_0  : 44;
		mmr_t	tag         : 20;
	} sh_x_tag5_s;
} sh_x_tag5_u_t;
#endif

/* ==================================================================== */
/*                         Register "SH_X_TAG6"                         */
/*                           AC tag Registers                           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_x_tag6_u {
	mmr_t	sh_x_tag6_regval;
	struct {
		mmr_t	tag         : 20;
		mmr_t	reserved_0  : 44;
	} sh_x_tag6_s;
} sh_x_tag6_u_t;
#else
typedef union sh_x_tag6_u {
	mmr_t	sh_x_tag6_regval;
	struct {
		mmr_t	reserved_0  : 44;
		mmr_t	tag         : 20;
	} sh_x_tag6_s;
} sh_x_tag6_u_t;
#endif

/* ==================================================================== */
/*                         Register "SH_X_TAG7"                         */
/*                           AC tag Registers                           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_x_tag7_u {
	mmr_t	sh_x_tag7_regval;
	struct {
		mmr_t	tag         : 20;
		mmr_t	reserved_0  : 44;
	} sh_x_tag7_s;
} sh_x_tag7_u_t;
#else
typedef union sh_x_tag7_u {
	mmr_t	sh_x_tag7_regval;
	struct {
		mmr_t	reserved_0  : 44;
		mmr_t	tag         : 20;
	} sh_x_tag7_s;
} sh_x_tag7_u_t;
#endif

/* ==================================================================== */
/*                         Register "SH_Y_TAG0"                         */
/*                           AC tag Registers                           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_y_tag0_u {
	mmr_t	sh_y_tag0_regval;
	struct {
		mmr_t	tag         : 20;
		mmr_t	reserved_0  : 44;
	} sh_y_tag0_s;
} sh_y_tag0_u_t;
#else
typedef union sh_y_tag0_u {
	mmr_t	sh_y_tag0_regval;
	struct {
		mmr_t	reserved_0  : 44;
		mmr_t	tag         : 20;
	} sh_y_tag0_s;
} sh_y_tag0_u_t;
#endif

/* ==================================================================== */
/*                         Register "SH_Y_TAG1"                         */
/*                           AC tag Registers                           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_y_tag1_u {
	mmr_t	sh_y_tag1_regval;
	struct {
		mmr_t	tag         : 20;
		mmr_t	reserved_0  : 44;
	} sh_y_tag1_s;
} sh_y_tag1_u_t;
#else
typedef union sh_y_tag1_u {
	mmr_t	sh_y_tag1_regval;
	struct {
		mmr_t	reserved_0  : 44;
		mmr_t	tag         : 20;
	} sh_y_tag1_s;
} sh_y_tag1_u_t;
#endif

/* ==================================================================== */
/*                         Register "SH_Y_TAG2"                         */
/*                           AC tag Registers                           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_y_tag2_u {
	mmr_t	sh_y_tag2_regval;
	struct {
		mmr_t	tag         : 20;
		mmr_t	reserved_0  : 44;
	} sh_y_tag2_s;
} sh_y_tag2_u_t;
#else
typedef union sh_y_tag2_u {
	mmr_t	sh_y_tag2_regval;
	struct {
		mmr_t	reserved_0  : 44;
		mmr_t	tag         : 20;
	} sh_y_tag2_s;
} sh_y_tag2_u_t;
#endif

/* ==================================================================== */
/*                         Register "SH_Y_TAG3"                         */
/*                           AC tag Registers                           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_y_tag3_u {
	mmr_t	sh_y_tag3_regval;
	struct {
		mmr_t	tag         : 20;
		mmr_t	reserved_0  : 44;
	} sh_y_tag3_s;
} sh_y_tag3_u_t;
#else
typedef union sh_y_tag3_u {
	mmr_t	sh_y_tag3_regval;
	struct {
		mmr_t	reserved_0  : 44;
		mmr_t	tag         : 20;
	} sh_y_tag3_s;
} sh_y_tag3_u_t;
#endif

/* ==================================================================== */
/*                         Register "SH_Y_TAG4"                         */
/*                           AC tag Registers                           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_y_tag4_u {
	mmr_t	sh_y_tag4_regval;
	struct {
		mmr_t	tag         : 20;
		mmr_t	reserved_0  : 44;
	} sh_y_tag4_s;
} sh_y_tag4_u_t;
#else
typedef union sh_y_tag4_u {
	mmr_t	sh_y_tag4_regval;
	struct {
		mmr_t	reserved_0  : 44;
		mmr_t	tag         : 20;
	} sh_y_tag4_s;
} sh_y_tag4_u_t;
#endif

/* ==================================================================== */
/*                         Register "SH_Y_TAG5"                         */
/*                           AC tag Registers                           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_y_tag5_u {
	mmr_t	sh_y_tag5_regval;
	struct {
		mmr_t	tag         : 20;
		mmr_t	reserved_0  : 44;
	} sh_y_tag5_s;
} sh_y_tag5_u_t;
#else
typedef union sh_y_tag5_u {
	mmr_t	sh_y_tag5_regval;
	struct {
		mmr_t	reserved_0  : 44;
		mmr_t	tag         : 20;
	} sh_y_tag5_s;
} sh_y_tag5_u_t;
#endif

/* ==================================================================== */
/*                         Register "SH_Y_TAG6"                         */
/*                           AC tag Registers                           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_y_tag6_u {
	mmr_t	sh_y_tag6_regval;
	struct {
		mmr_t	tag         : 20;
		mmr_t	reserved_0  : 44;
	} sh_y_tag6_s;
} sh_y_tag6_u_t;
#else
typedef union sh_y_tag6_u {
	mmr_t	sh_y_tag6_regval;
	struct {
		mmr_t	reserved_0  : 44;
		mmr_t	tag         : 20;
	} sh_y_tag6_s;
} sh_y_tag6_u_t;
#endif

/* ==================================================================== */
/*                         Register "SH_Y_TAG7"                         */
/*                           AC tag Registers                           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_y_tag7_u {
	mmr_t	sh_y_tag7_regval;
	struct {
		mmr_t	tag         : 20;
		mmr_t	reserved_0  : 44;
	} sh_y_tag7_s;
} sh_y_tag7_u_t;
#else
typedef union sh_y_tag7_u {
	mmr_t	sh_y_tag7_regval;
	struct {
		mmr_t	reserved_0  : 44;
		mmr_t	tag         : 20;
	} sh_y_tag7_s;
} sh_y_tag7_u_t;
#endif

/* ==================================================================== */
/*                      Register "SH_MMRBIST_BASE"                      */
/*                        mmr/bist base address                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_mmrbist_base_u {
	mmr_t	sh_mmrbist_base_regval;
	struct {
		mmr_t	reserved_0  : 3;
		mmr_t	dword_addr  : 47;
		mmr_t	reserved_1  : 14;
	} sh_mmrbist_base_s;
} sh_mmrbist_base_u_t;
#else
typedef union sh_mmrbist_base_u {
	mmr_t	sh_mmrbist_base_regval;
	struct {
		mmr_t	reserved_1  : 14;
		mmr_t	dword_addr  : 47;
		mmr_t	reserved_0  : 3;
	} sh_mmrbist_base_s;
} sh_mmrbist_base_u_t;
#endif

/* ==================================================================== */
/*                      Register "SH_MMRBIST_CTL"                       */
/*                          Bist base address                           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_mmrbist_ctl_u {
	mmr_t	sh_mmrbist_ctl_regval;
	struct {
		mmr_t	block_length : 31;
		mmr_t	reserved_0   : 1;
		mmr_t	cmd          : 7;
		mmr_t	reserved_1   : 1;
		mmr_t	in_progress  : 1;
		mmr_t	fail         : 1;
		mmr_t	mem_idle     : 1;
		mmr_t	reserved_2   : 1;
		mmr_t	reset_state  : 1;
		mmr_t	reserved_3   : 19;
	} sh_mmrbist_ctl_s;
} sh_mmrbist_ctl_u_t;
#else
typedef union sh_mmrbist_ctl_u {
	mmr_t	sh_mmrbist_ctl_regval;
	struct {
		mmr_t	reserved_3   : 19;
		mmr_t	reset_state  : 1;
		mmr_t	reserved_2   : 1;
		mmr_t	mem_idle     : 1;
		mmr_t	fail         : 1;
		mmr_t	in_progress  : 1;
		mmr_t	reserved_1   : 1;
		mmr_t	cmd          : 7;
		mmr_t	reserved_0   : 1;
		mmr_t	block_length : 31;
	} sh_mmrbist_ctl_s;
} sh_mmrbist_ctl_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_MD_DBUG_DATA_CFG"                    */
/*                configuration for md debug data muxes                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dbug_data_cfg_u {
	mmr_t	sh_md_dbug_data_cfg_regval;
	struct {
		mmr_t	nibble0_chiplet : 3;
		mmr_t	reserved_0      : 1;
		mmr_t	nibble0_nibble  : 3;
		mmr_t	reserved_1      : 1;
		mmr_t	nibble1_chiplet : 3;
		mmr_t	reserved_2      : 1;
		mmr_t	nibble1_nibble  : 3;
		mmr_t	reserved_3      : 1;
		mmr_t	nibble2_chiplet : 3;
		mmr_t	reserved_4      : 1;
		mmr_t	nibble2_nibble  : 3;
		mmr_t	reserved_5      : 1;
		mmr_t	nibble3_chiplet : 3;
		mmr_t	reserved_6      : 1;
		mmr_t	nibble3_nibble  : 3;
		mmr_t	reserved_7      : 1;
		mmr_t	nibble4_chiplet : 3;
		mmr_t	reserved_8      : 1;
		mmr_t	nibble4_nibble  : 3;
		mmr_t	reserved_9      : 1;
		mmr_t	nibble5_chiplet : 3;
		mmr_t	reserved_10     : 1;
		mmr_t	nibble5_nibble  : 3;
		mmr_t	reserved_11     : 1;
		mmr_t	nibble6_chiplet : 3;
		mmr_t	reserved_12     : 1;
		mmr_t	nibble6_nibble  : 3;
		mmr_t	reserved_13     : 1;
		mmr_t	nibble7_chiplet : 3;
		mmr_t	reserved_14     : 1;
		mmr_t	nibble7_nibble  : 3;
		mmr_t	reserved_15     : 1;
	} sh_md_dbug_data_cfg_s;
} sh_md_dbug_data_cfg_u_t;
#else
typedef union sh_md_dbug_data_cfg_u {
	mmr_t	sh_md_dbug_data_cfg_regval;
	struct {
		mmr_t	reserved_15     : 1;
		mmr_t	nibble7_nibble  : 3;
		mmr_t	reserved_14     : 1;
		mmr_t	nibble7_chiplet : 3;
		mmr_t	reserved_13     : 1;
		mmr_t	nibble6_nibble  : 3;
		mmr_t	reserved_12     : 1;
		mmr_t	nibble6_chiplet : 3;
		mmr_t	reserved_11     : 1;
		mmr_t	nibble5_nibble  : 3;
		mmr_t	reserved_10     : 1;
		mmr_t	nibble5_chiplet : 3;
		mmr_t	reserved_9      : 1;
		mmr_t	nibble4_nibble  : 3;
		mmr_t	reserved_8      : 1;
		mmr_t	nibble4_chiplet : 3;
		mmr_t	reserved_7      : 1;
		mmr_t	nibble3_nibble  : 3;
		mmr_t	reserved_6      : 1;
		mmr_t	nibble3_chiplet : 3;
		mmr_t	reserved_5      : 1;
		mmr_t	nibble2_nibble  : 3;
		mmr_t	reserved_4      : 1;
		mmr_t	nibble2_chiplet : 3;
		mmr_t	reserved_3      : 1;
		mmr_t	nibble1_nibble  : 3;
		mmr_t	reserved_2      : 1;
		mmr_t	nibble1_chiplet : 3;
		mmr_t	reserved_1      : 1;
		mmr_t	nibble0_nibble  : 3;
		mmr_t	reserved_0      : 1;
		mmr_t	nibble0_chiplet : 3;
	} sh_md_dbug_data_cfg_s;
} sh_md_dbug_data_cfg_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_MD_DBUG_TRIGGER_CFG"                   */
/*                 configuration for md debug triggers                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dbug_trigger_cfg_u {
	mmr_t	sh_md_dbug_trigger_cfg_regval;
	struct {
		mmr_t	nibble0_chiplet : 3;
		mmr_t	reserved_0      : 1;
		mmr_t	nibble0_nibble  : 3;
		mmr_t	reserved_1      : 1;
		mmr_t	nibble1_chiplet : 3;
		mmr_t	reserved_2      : 1;
		mmr_t	nibble1_nibble  : 3;
		mmr_t	reserved_3      : 1;
		mmr_t	nibble2_chiplet : 3;
		mmr_t	reserved_4      : 1;
		mmr_t	nibble2_nibble  : 3;
		mmr_t	reserved_5      : 1;
		mmr_t	nibble3_chiplet : 3;
		mmr_t	reserved_6      : 1;
		mmr_t	nibble3_nibble  : 3;
		mmr_t	reserved_7      : 1;
		mmr_t	nibble4_chiplet : 3;
		mmr_t	reserved_8      : 1;
		mmr_t	nibble4_nibble  : 3;
		mmr_t	reserved_9      : 1;
		mmr_t	nibble5_chiplet : 3;
		mmr_t	reserved_10     : 1;
		mmr_t	nibble5_nibble  : 3;
		mmr_t	reserved_11     : 1;
		mmr_t	nibble6_chiplet : 3;
		mmr_t	reserved_12     : 1;
		mmr_t	nibble6_nibble  : 3;
		mmr_t	reserved_13     : 1;
		mmr_t	nibble7_chiplet : 3;
		mmr_t	reserved_14     : 1;
		mmr_t	nibble7_nibble  : 3;
		mmr_t	enable          : 1;
	} sh_md_dbug_trigger_cfg_s;
} sh_md_dbug_trigger_cfg_u_t;
#else
typedef union sh_md_dbug_trigger_cfg_u {
	mmr_t	sh_md_dbug_trigger_cfg_regval;
	struct {
		mmr_t	enable          : 1;
		mmr_t	nibble7_nibble  : 3;
		mmr_t	reserved_14     : 1;
		mmr_t	nibble7_chiplet : 3;
		mmr_t	reserved_13     : 1;
		mmr_t	nibble6_nibble  : 3;
		mmr_t	reserved_12     : 1;
		mmr_t	nibble6_chiplet : 3;
		mmr_t	reserved_11     : 1;
		mmr_t	nibble5_nibble  : 3;
		mmr_t	reserved_10     : 1;
		mmr_t	nibble5_chiplet : 3;
		mmr_t	reserved_9      : 1;
		mmr_t	nibble4_nibble  : 3;
		mmr_t	reserved_8      : 1;
		mmr_t	nibble4_chiplet : 3;
		mmr_t	reserved_7      : 1;
		mmr_t	nibble3_nibble  : 3;
		mmr_t	reserved_6      : 1;
		mmr_t	nibble3_chiplet : 3;
		mmr_t	reserved_5      : 1;
		mmr_t	nibble2_nibble  : 3;
		mmr_t	reserved_4      : 1;
		mmr_t	nibble2_chiplet : 3;
		mmr_t	reserved_3      : 1;
		mmr_t	nibble1_nibble  : 3;
		mmr_t	reserved_2      : 1;
		mmr_t	nibble1_chiplet : 3;
		mmr_t	reserved_1      : 1;
		mmr_t	nibble0_nibble  : 3;
		mmr_t	reserved_0      : 1;
		mmr_t	nibble0_chiplet : 3;
	} sh_md_dbug_trigger_cfg_s;
} sh_md_dbug_trigger_cfg_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_MD_DBUG_COMPARE"                     */
/*                  md debug compare pattern and mask                   */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dbug_compare_u {
	mmr_t	sh_md_dbug_compare_regval;
	struct {
		mmr_t	pattern     : 32;
		mmr_t	mask        : 32;
	} sh_md_dbug_compare_s;
} sh_md_dbug_compare_u_t;
#else
typedef union sh_md_dbug_compare_u {
	mmr_t	sh_md_dbug_compare_regval;
	struct {
		mmr_t	mask        : 32;
		mmr_t	pattern     : 32;
	} sh_md_dbug_compare_s;
} sh_md_dbug_compare_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_X_MOD_DBUG_SEL"                     */
/*                         MD acx debug select                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_x_mod_dbug_sel_u {
	mmr_t	sh_x_mod_dbug_sel_regval;
	struct {
		mmr_t	tag_sel     : 8;
		mmr_t	wbq_sel     : 8;
		mmr_t	arb_sel     : 8;
		mmr_t	atl_sel     : 11;
		mmr_t	atr_sel     : 11;
		mmr_t	dql_sel     : 6;
		mmr_t	dqr_sel     : 6;
		mmr_t	reserved_0  : 6;
	} sh_x_mod_dbug_sel_s;
} sh_x_mod_dbug_sel_u_t;
#else
typedef union sh_x_mod_dbug_sel_u {
	mmr_t	sh_x_mod_dbug_sel_regval;
	struct {
		mmr_t	reserved_0  : 6;
		mmr_t	dqr_sel     : 6;
		mmr_t	dql_sel     : 6;
		mmr_t	atr_sel     : 11;
		mmr_t	atl_sel     : 11;
		mmr_t	arb_sel     : 8;
		mmr_t	wbq_sel     : 8;
		mmr_t	tag_sel     : 8;
	} sh_x_mod_dbug_sel_s;
} sh_x_mod_dbug_sel_u_t;
#endif

/* ==================================================================== */
/*                       Register "SH_X_DBUG_SEL"                       */
/*                         MD acx debug select                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_x_dbug_sel_u {
	mmr_t	sh_x_dbug_sel_regval;
	struct {
		mmr_t	dbg_sel     : 24;
		mmr_t	reserved_0  : 40;
	} sh_x_dbug_sel_s;
} sh_x_dbug_sel_u_t;
#else
typedef union sh_x_dbug_sel_u {
	mmr_t	sh_x_dbug_sel_regval;
	struct {
		mmr_t	reserved_0  : 40;
		mmr_t	dbg_sel     : 24;
	} sh_x_dbug_sel_s;
} sh_x_dbug_sel_u_t;
#endif

/* ==================================================================== */
/*                      Register "SH_X_LADDR_CMP"                       */
/*                        MD acx address compare                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_x_laddr_cmp_u {
	mmr_t	sh_x_laddr_cmp_regval;
	struct {
		mmr_t	cmp_val     : 28;
		mmr_t	reserved_0  : 4;
		mmr_t	mask_val    : 28;
		mmr_t	reserved_1  : 4;
	} sh_x_laddr_cmp_s;
} sh_x_laddr_cmp_u_t;
#else
typedef union sh_x_laddr_cmp_u {
	mmr_t	sh_x_laddr_cmp_regval;
	struct {
		mmr_t	reserved_1  : 4;
		mmr_t	mask_val    : 28;
		mmr_t	reserved_0  : 4;
		mmr_t	cmp_val     : 28;
	} sh_x_laddr_cmp_s;
} sh_x_laddr_cmp_u_t;
#endif

/* ==================================================================== */
/*                      Register "SH_X_RADDR_CMP"                       */
/*                        MD acx address compare                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_x_raddr_cmp_u {
	mmr_t	sh_x_raddr_cmp_regval;
	struct {
		mmr_t	cmp_val     : 28;
		mmr_t	reserved_0  : 4;
		mmr_t	mask_val    : 28;
		mmr_t	reserved_1  : 4;
	} sh_x_raddr_cmp_s;
} sh_x_raddr_cmp_u_t;
#else
typedef union sh_x_raddr_cmp_u {
	mmr_t	sh_x_raddr_cmp_regval;
	struct {
		mmr_t	reserved_1  : 4;
		mmr_t	mask_val    : 28;
		mmr_t	reserved_0  : 4;
		mmr_t	cmp_val     : 28;
	} sh_x_raddr_cmp_s;
} sh_x_raddr_cmp_u_t;
#endif

/* ==================================================================== */
/*                       Register "SH_X_TAG_CMP"                        */
/*                        MD acx tagmgr compare                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_x_tag_cmp_u {
	mmr_t	sh_x_tag_cmp_regval;
	struct {
		mmr_t	cmd         : 8;
		mmr_t	addr        : 33;
		mmr_t	src         : 14;
		mmr_t	reserved_0  : 9;
	} sh_x_tag_cmp_s;
} sh_x_tag_cmp_u_t;
#else
typedef union sh_x_tag_cmp_u {
	mmr_t	sh_x_tag_cmp_regval;
	struct {
		mmr_t	reserved_0  : 9;
		mmr_t	src         : 14;
		mmr_t	addr        : 33;
		mmr_t	cmd         : 8;
	} sh_x_tag_cmp_s;
} sh_x_tag_cmp_u_t;
#endif

/* ==================================================================== */
/*                       Register "SH_X_TAG_MASK"                       */
/*                          MD acx tagmgr mask                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_x_tag_mask_u {
	mmr_t	sh_x_tag_mask_regval;
	struct {
		mmr_t	cmd         : 8;
		mmr_t	addr        : 33;
		mmr_t	src         : 14;
		mmr_t	reserved_0  : 9;
	} sh_x_tag_mask_s;
} sh_x_tag_mask_u_t;
#else
typedef union sh_x_tag_mask_u {
	mmr_t	sh_x_tag_mask_regval;
	struct {
		mmr_t	reserved_0  : 9;
		mmr_t	src         : 14;
		mmr_t	addr        : 33;
		mmr_t	cmd         : 8;
	} sh_x_tag_mask_s;
} sh_x_tag_mask_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_Y_MOD_DBUG_SEL"                     */
/*                         MD acy debug select                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_y_mod_dbug_sel_u {
	mmr_t	sh_y_mod_dbug_sel_regval;
	struct {
		mmr_t	tag_sel     : 8;
		mmr_t	wbq_sel     : 8;
		mmr_t	arb_sel     : 8;
		mmr_t	atl_sel     : 11;
		mmr_t	atr_sel     : 11;
		mmr_t	dql_sel     : 6;
		mmr_t	dqr_sel     : 6;
		mmr_t	reserved_0  : 6;
	} sh_y_mod_dbug_sel_s;
} sh_y_mod_dbug_sel_u_t;
#else
typedef union sh_y_mod_dbug_sel_u {
	mmr_t	sh_y_mod_dbug_sel_regval;
	struct {
		mmr_t	reserved_0  : 6;
		mmr_t	dqr_sel     : 6;
		mmr_t	dql_sel     : 6;
		mmr_t	atr_sel     : 11;
		mmr_t	atl_sel     : 11;
		mmr_t	arb_sel     : 8;
		mmr_t	wbq_sel     : 8;
		mmr_t	tag_sel     : 8;
	} sh_y_mod_dbug_sel_s;
} sh_y_mod_dbug_sel_u_t;
#endif

/* ==================================================================== */
/*                       Register "SH_Y_DBUG_SEL"                       */
/*                         MD acy debug select                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_y_dbug_sel_u {
	mmr_t	sh_y_dbug_sel_regval;
	struct {
		mmr_t	dbg_sel     : 24;
		mmr_t	reserved_0  : 40;
	} sh_y_dbug_sel_s;
} sh_y_dbug_sel_u_t;
#else
typedef union sh_y_dbug_sel_u {
	mmr_t	sh_y_dbug_sel_regval;
	struct {
		mmr_t	reserved_0  : 40;
		mmr_t	dbg_sel     : 24;
	} sh_y_dbug_sel_s;
} sh_y_dbug_sel_u_t;
#endif

/* ==================================================================== */
/*                      Register "SH_Y_LADDR_CMP"                       */
/*                        MD acy address compare                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_y_laddr_cmp_u {
	mmr_t	sh_y_laddr_cmp_regval;
	struct {
		mmr_t	cmp_val     : 28;
		mmr_t	reserved_0  : 4;
		mmr_t	mask_val    : 28;
		mmr_t	reserved_1  : 4;
	} sh_y_laddr_cmp_s;
} sh_y_laddr_cmp_u_t;
#else
typedef union sh_y_laddr_cmp_u {
	mmr_t	sh_y_laddr_cmp_regval;
	struct {
		mmr_t	reserved_1  : 4;
		mmr_t	mask_val    : 28;
		mmr_t	reserved_0  : 4;
		mmr_t	cmp_val     : 28;
	} sh_y_laddr_cmp_s;
} sh_y_laddr_cmp_u_t;
#endif

/* ==================================================================== */
/*                      Register "SH_Y_RADDR_CMP"                       */
/*                        MD acy address compare                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_y_raddr_cmp_u {
	mmr_t	sh_y_raddr_cmp_regval;
	struct {
		mmr_t	cmp_val     : 28;
		mmr_t	reserved_0  : 4;
		mmr_t	mask_val    : 28;
		mmr_t	reserved_1  : 4;
	} sh_y_raddr_cmp_s;
} sh_y_raddr_cmp_u_t;
#else
typedef union sh_y_raddr_cmp_u {
	mmr_t	sh_y_raddr_cmp_regval;
	struct {
		mmr_t	reserved_1  : 4;
		mmr_t	mask_val    : 28;
		mmr_t	reserved_0  : 4;
		mmr_t	cmp_val     : 28;
	} sh_y_raddr_cmp_s;
} sh_y_raddr_cmp_u_t;
#endif

/* ==================================================================== */
/*                       Register "SH_Y_TAG_CMP"                        */
/*                        MD acy tagmgr compare                         */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_y_tag_cmp_u {
	mmr_t	sh_y_tag_cmp_regval;
	struct {
		mmr_t	cmd         : 8;
		mmr_t	addr        : 33;
		mmr_t	src         : 14;
		mmr_t	reserved_0  : 9;
	} sh_y_tag_cmp_s;
} sh_y_tag_cmp_u_t;
#else
typedef union sh_y_tag_cmp_u {
	mmr_t	sh_y_tag_cmp_regval;
	struct {
		mmr_t	reserved_0  : 9;
		mmr_t	src         : 14;
		mmr_t	addr        : 33;
		mmr_t	cmd         : 8;
	} sh_y_tag_cmp_s;
} sh_y_tag_cmp_u_t;
#endif

/* ==================================================================== */
/*                       Register "SH_Y_TAG_MASK"                       */
/*                          MD acy tagmgr mask                          */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_y_tag_mask_u {
	mmr_t	sh_y_tag_mask_regval;
	struct {
		mmr_t	cmd         : 8;
		mmr_t	addr        : 33;
		mmr_t	src         : 14;
		mmr_t	reserved_0  : 9;
	} sh_y_tag_mask_s;
} sh_y_tag_mask_u_t;
#else
typedef union sh_y_tag_mask_u {
	mmr_t	sh_y_tag_mask_regval;
	struct {
		mmr_t	reserved_0  : 9;
		mmr_t	src         : 14;
		mmr_t	addr        : 33;
		mmr_t	cmd         : 8;
	} sh_y_tag_mask_s;
} sh_y_tag_mask_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_MD_JNR_DBUG_DATA_CFG"                  */
/*              configuration for md jnr debug data muxes               */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_jnr_dbug_data_cfg_u {
	mmr_t	sh_md_jnr_dbug_data_cfg_regval;
	struct {
		mmr_t	nibble0_sel : 3;
		mmr_t	reserved_0  : 1;
		mmr_t	nibble1_sel : 3;
		mmr_t	reserved_1  : 1;
		mmr_t	nibble2_sel : 3;
		mmr_t	reserved_2  : 1;
		mmr_t	nibble3_sel : 3;
		mmr_t	reserved_3  : 1;
		mmr_t	nibble4_sel : 3;
		mmr_t	reserved_4  : 1;
		mmr_t	nibble5_sel : 3;
		mmr_t	reserved_5  : 1;
		mmr_t	nibble6_sel : 3;
		mmr_t	reserved_6  : 1;
		mmr_t	nibble7_sel : 3;
		mmr_t	reserved_7  : 33;
	} sh_md_jnr_dbug_data_cfg_s;
} sh_md_jnr_dbug_data_cfg_u_t;
#else
typedef union sh_md_jnr_dbug_data_cfg_u {
	mmr_t	sh_md_jnr_dbug_data_cfg_regval;
	struct {
		mmr_t	reserved_7  : 33;
		mmr_t	nibble7_sel : 3;
		mmr_t	reserved_6  : 1;
		mmr_t	nibble6_sel : 3;
		mmr_t	reserved_5  : 1;
		mmr_t	nibble5_sel : 3;
		mmr_t	reserved_4  : 1;
		mmr_t	nibble4_sel : 3;
		mmr_t	reserved_3  : 1;
		mmr_t	nibble3_sel : 3;
		mmr_t	reserved_2  : 1;
		mmr_t	nibble2_sel : 3;
		mmr_t	reserved_1  : 1;
		mmr_t	nibble1_sel : 3;
		mmr_t	reserved_0  : 1;
		mmr_t	nibble0_sel : 3;
	} sh_md_jnr_dbug_data_cfg_s;
} sh_md_jnr_dbug_data_cfg_u_t;
#endif

/* ==================================================================== */
/*                     Register "SH_MD_LAST_CREDIT"                     */
/*                 captures last credit values on reset                 */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_last_credit_u {
	mmr_t	sh_md_last_credit_regval;
	struct {
		mmr_t	rq_to_pi    : 6;
		mmr_t	reserved_0  : 2;
		mmr_t	rp_to_pi    : 6;
		mmr_t	reserved_1  : 2;
		mmr_t	rq_to_xn    : 6;
		mmr_t	reserved_2  : 2;
		mmr_t	rp_to_xn    : 6;
		mmr_t	reserved_3  : 2;
		mmr_t	to_lb       : 6;
		mmr_t	reserved_4  : 26;
	} sh_md_last_credit_s;
} sh_md_last_credit_u_t;
#else
typedef union sh_md_last_credit_u {
	mmr_t	sh_md_last_credit_regval;
	struct {
		mmr_t	reserved_4  : 26;
		mmr_t	to_lb       : 6;
		mmr_t	reserved_3  : 2;
		mmr_t	rp_to_xn    : 6;
		mmr_t	reserved_2  : 2;
		mmr_t	rq_to_xn    : 6;
		mmr_t	reserved_1  : 2;
		mmr_t	rp_to_pi    : 6;
		mmr_t	reserved_0  : 2;
		mmr_t	rq_to_pi    : 6;
	} sh_md_last_credit_s;
} sh_md_last_credit_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_MEM_CAPTURE_ADDR"                    */
/*                   Address capture address register                   */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_mem_capture_addr_u {
	mmr_t	sh_mem_capture_addr_regval;
	struct {
		mmr_t	reserved_0  : 3;
		mmr_t	addr        : 33;
		mmr_t	cmd         : 8;
		mmr_t	reserved_1  : 20;
	} sh_mem_capture_addr_s;
} sh_mem_capture_addr_u_t;
#else
typedef union sh_mem_capture_addr_u {
	mmr_t	sh_mem_capture_addr_regval;
	struct {
		mmr_t	reserved_1  : 20;
		mmr_t	cmd         : 8;
		mmr_t	addr        : 33;
		mmr_t	reserved_0  : 3;
	} sh_mem_capture_addr_s;
} sh_mem_capture_addr_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_MEM_CAPTURE_MASK"                    */
/*                    Address capture mask register                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_mem_capture_mask_u {
	mmr_t	sh_mem_capture_mask_regval;
	struct {
		mmr_t	reserved_0    : 3;
		mmr_t	addr          : 33;
		mmr_t	cmd           : 8;
		mmr_t	enable_local  : 1;
		mmr_t	enable_remote : 1;
		mmr_t	reserved_1    : 18;
	} sh_mem_capture_mask_s;
} sh_mem_capture_mask_u_t;
#else
typedef union sh_mem_capture_mask_u {
	mmr_t	sh_mem_capture_mask_regval;
	struct {
		mmr_t	reserved_1    : 18;
		mmr_t	enable_remote : 1;
		mmr_t	enable_local  : 1;
		mmr_t	cmd           : 8;
		mmr_t	addr          : 33;
		mmr_t	reserved_0    : 3;
	} sh_mem_capture_mask_s;
} sh_mem_capture_mask_u_t;
#endif

/* ==================================================================== */
/*                    Register "SH_MEM_CAPTURE_HDR"                     */
/*                   Address capture header register                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_mem_capture_hdr_u {
	mmr_t	sh_mem_capture_hdr_regval;
	struct {
		mmr_t	reserved_0  : 3;
		mmr_t	addr        : 33;
		mmr_t	cmd         : 8;
		mmr_t	src         : 14;
		mmr_t	cntr        : 6;
	} sh_mem_capture_hdr_s;
} sh_mem_capture_hdr_u_t;
#else
typedef union sh_mem_capture_hdr_u {
	mmr_t	sh_mem_capture_hdr_regval;
	struct {
		mmr_t	cntr        : 6;
		mmr_t	src         : 14;
		mmr_t	cmd         : 8;
		mmr_t	addr        : 33;
		mmr_t	reserved_0  : 3;
	} sh_mem_capture_hdr_s;
} sh_mem_capture_hdr_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_MD_DQLP_MMR_DIR_CONFIG"                 */
/*                     DQ directory config register                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_dir_config_u {
	mmr_t	sh_md_dqlp_mmr_dir_config_regval;
	struct {
		mmr_t	sys_size    : 3;
		mmr_t	en_direcc   : 1;
		mmr_t	en_dirpois  : 1;
		mmr_t	reserved_0  : 59;
	} sh_md_dqlp_mmr_dir_config_s;
} sh_md_dqlp_mmr_dir_config_u_t;
#else
typedef union sh_md_dqlp_mmr_dir_config_u {
	mmr_t	sh_md_dqlp_mmr_dir_config_regval;
	struct {
		mmr_t	reserved_0  : 59;
		mmr_t	en_dirpois  : 1;
		mmr_t	en_direcc   : 1;
		mmr_t	sys_size    : 3;
	} sh_md_dqlp_mmr_dir_config_s;
} sh_md_dqlp_mmr_dir_config_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQLP_MMR_DIR_PRESVEC0"                */
/*                      node [63:0] presence bits                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_dir_presvec0_u {
	mmr_t	sh_md_dqlp_mmr_dir_presvec0_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqlp_mmr_dir_presvec0_s;
} sh_md_dqlp_mmr_dir_presvec0_u_t;
#else
typedef union sh_md_dqlp_mmr_dir_presvec0_u {
	mmr_t	sh_md_dqlp_mmr_dir_presvec0_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqlp_mmr_dir_presvec0_s;
} sh_md_dqlp_mmr_dir_presvec0_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQLP_MMR_DIR_PRESVEC1"                */
/*                     node [127:64] presence bits                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_dir_presvec1_u {
	mmr_t	sh_md_dqlp_mmr_dir_presvec1_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqlp_mmr_dir_presvec1_s;
} sh_md_dqlp_mmr_dir_presvec1_u_t;
#else
typedef union sh_md_dqlp_mmr_dir_presvec1_u {
	mmr_t	sh_md_dqlp_mmr_dir_presvec1_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqlp_mmr_dir_presvec1_s;
} sh_md_dqlp_mmr_dir_presvec1_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQLP_MMR_DIR_PRESVEC2"                */
/*                     node [191:128] presence bits                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_dir_presvec2_u {
	mmr_t	sh_md_dqlp_mmr_dir_presvec2_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqlp_mmr_dir_presvec2_s;
} sh_md_dqlp_mmr_dir_presvec2_u_t;
#else
typedef union sh_md_dqlp_mmr_dir_presvec2_u {
	mmr_t	sh_md_dqlp_mmr_dir_presvec2_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqlp_mmr_dir_presvec2_s;
} sh_md_dqlp_mmr_dir_presvec2_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQLP_MMR_DIR_PRESVEC3"                */
/*                     node [255:192] presence bits                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_dir_presvec3_u {
	mmr_t	sh_md_dqlp_mmr_dir_presvec3_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqlp_mmr_dir_presvec3_s;
} sh_md_dqlp_mmr_dir_presvec3_u_t;
#else
typedef union sh_md_dqlp_mmr_dir_presvec3_u {
	mmr_t	sh_md_dqlp_mmr_dir_presvec3_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqlp_mmr_dir_presvec3_s;
} sh_md_dqlp_mmr_dir_presvec3_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQLP_MMR_DIR_LOCVEC0"                 */
/*                        local vector for acc=0                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_dir_locvec0_u {
	mmr_t	sh_md_dqlp_mmr_dir_locvec0_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqlp_mmr_dir_locvec0_s;
} sh_md_dqlp_mmr_dir_locvec0_u_t;
#else
typedef union sh_md_dqlp_mmr_dir_locvec0_u {
	mmr_t	sh_md_dqlp_mmr_dir_locvec0_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqlp_mmr_dir_locvec0_s;
} sh_md_dqlp_mmr_dir_locvec0_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQLP_MMR_DIR_LOCVEC1"                 */
/*                        local vector for acc=1                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_dir_locvec1_u {
	mmr_t	sh_md_dqlp_mmr_dir_locvec1_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqlp_mmr_dir_locvec1_s;
} sh_md_dqlp_mmr_dir_locvec1_u_t;
#else
typedef union sh_md_dqlp_mmr_dir_locvec1_u {
	mmr_t	sh_md_dqlp_mmr_dir_locvec1_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqlp_mmr_dir_locvec1_s;
} sh_md_dqlp_mmr_dir_locvec1_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQLP_MMR_DIR_LOCVEC2"                 */
/*                        local vector for acc=2                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_dir_locvec2_u {
	mmr_t	sh_md_dqlp_mmr_dir_locvec2_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqlp_mmr_dir_locvec2_s;
} sh_md_dqlp_mmr_dir_locvec2_u_t;
#else
typedef union sh_md_dqlp_mmr_dir_locvec2_u {
	mmr_t	sh_md_dqlp_mmr_dir_locvec2_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqlp_mmr_dir_locvec2_s;
} sh_md_dqlp_mmr_dir_locvec2_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQLP_MMR_DIR_LOCVEC3"                 */
/*                        local vector for acc=3                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_dir_locvec3_u {
	mmr_t	sh_md_dqlp_mmr_dir_locvec3_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqlp_mmr_dir_locvec3_s;
} sh_md_dqlp_mmr_dir_locvec3_u_t;
#else
typedef union sh_md_dqlp_mmr_dir_locvec3_u {
	mmr_t	sh_md_dqlp_mmr_dir_locvec3_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqlp_mmr_dir_locvec3_s;
} sh_md_dqlp_mmr_dir_locvec3_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQLP_MMR_DIR_LOCVEC4"                 */
/*                        local vector for acc=4                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_dir_locvec4_u {
	mmr_t	sh_md_dqlp_mmr_dir_locvec4_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqlp_mmr_dir_locvec4_s;
} sh_md_dqlp_mmr_dir_locvec4_u_t;
#else
typedef union sh_md_dqlp_mmr_dir_locvec4_u {
	mmr_t	sh_md_dqlp_mmr_dir_locvec4_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqlp_mmr_dir_locvec4_s;
} sh_md_dqlp_mmr_dir_locvec4_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQLP_MMR_DIR_LOCVEC5"                 */
/*                        local vector for acc=5                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_dir_locvec5_u {
	mmr_t	sh_md_dqlp_mmr_dir_locvec5_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqlp_mmr_dir_locvec5_s;
} sh_md_dqlp_mmr_dir_locvec5_u_t;
#else
typedef union sh_md_dqlp_mmr_dir_locvec5_u {
	mmr_t	sh_md_dqlp_mmr_dir_locvec5_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqlp_mmr_dir_locvec5_s;
} sh_md_dqlp_mmr_dir_locvec5_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQLP_MMR_DIR_LOCVEC6"                 */
/*                        local vector for acc=6                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_dir_locvec6_u {
	mmr_t	sh_md_dqlp_mmr_dir_locvec6_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqlp_mmr_dir_locvec6_s;
} sh_md_dqlp_mmr_dir_locvec6_u_t;
#else
typedef union sh_md_dqlp_mmr_dir_locvec6_u {
	mmr_t	sh_md_dqlp_mmr_dir_locvec6_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqlp_mmr_dir_locvec6_s;
} sh_md_dqlp_mmr_dir_locvec6_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQLP_MMR_DIR_LOCVEC7"                 */
/*                        local vector for acc=7                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_dir_locvec7_u {
	mmr_t	sh_md_dqlp_mmr_dir_locvec7_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqlp_mmr_dir_locvec7_s;
} sh_md_dqlp_mmr_dir_locvec7_u_t;
#else
typedef union sh_md_dqlp_mmr_dir_locvec7_u {
	mmr_t	sh_md_dqlp_mmr_dir_locvec7_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqlp_mmr_dir_locvec7_s;
} sh_md_dqlp_mmr_dir_locvec7_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQLP_MMR_DIR_PRIVEC0"                 */
/*                      privilege vector for acc=0                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_dir_privec0_u {
	mmr_t	sh_md_dqlp_mmr_dir_privec0_regval;
	struct {
		mmr_t	in          : 14;
		mmr_t	out         : 14;
		mmr_t	reserved_0  : 36;
	} sh_md_dqlp_mmr_dir_privec0_s;
} sh_md_dqlp_mmr_dir_privec0_u_t;
#else
typedef union sh_md_dqlp_mmr_dir_privec0_u {
	mmr_t	sh_md_dqlp_mmr_dir_privec0_regval;
	struct {
		mmr_t	reserved_0  : 36;
		mmr_t	out         : 14;
		mmr_t	in          : 14;
	} sh_md_dqlp_mmr_dir_privec0_s;
} sh_md_dqlp_mmr_dir_privec0_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQLP_MMR_DIR_PRIVEC1"                 */
/*                      privilege vector for acc=1                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_dir_privec1_u {
	mmr_t	sh_md_dqlp_mmr_dir_privec1_regval;
	struct {
		mmr_t	in          : 14;
		mmr_t	out         : 14;
		mmr_t	reserved_0  : 36;
	} sh_md_dqlp_mmr_dir_privec1_s;
} sh_md_dqlp_mmr_dir_privec1_u_t;
#else
typedef union sh_md_dqlp_mmr_dir_privec1_u {
	mmr_t	sh_md_dqlp_mmr_dir_privec1_regval;
	struct {
		mmr_t	reserved_0  : 36;
		mmr_t	out         : 14;
		mmr_t	in          : 14;
	} sh_md_dqlp_mmr_dir_privec1_s;
} sh_md_dqlp_mmr_dir_privec1_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQLP_MMR_DIR_PRIVEC2"                 */
/*                      privilege vector for acc=2                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_dir_privec2_u {
	mmr_t	sh_md_dqlp_mmr_dir_privec2_regval;
	struct {
		mmr_t	in          : 14;
		mmr_t	out         : 14;
		mmr_t	reserved_0  : 36;
	} sh_md_dqlp_mmr_dir_privec2_s;
} sh_md_dqlp_mmr_dir_privec2_u_t;
#else
typedef union sh_md_dqlp_mmr_dir_privec2_u {
	mmr_t	sh_md_dqlp_mmr_dir_privec2_regval;
	struct {
		mmr_t	reserved_0  : 36;
		mmr_t	out         : 14;
		mmr_t	in          : 14;
	} sh_md_dqlp_mmr_dir_privec2_s;
} sh_md_dqlp_mmr_dir_privec2_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQLP_MMR_DIR_PRIVEC3"                 */
/*                      privilege vector for acc=3                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_dir_privec3_u {
	mmr_t	sh_md_dqlp_mmr_dir_privec3_regval;
	struct {
		mmr_t	in          : 14;
		mmr_t	out         : 14;
		mmr_t	reserved_0  : 36;
	} sh_md_dqlp_mmr_dir_privec3_s;
} sh_md_dqlp_mmr_dir_privec3_u_t;
#else
typedef union sh_md_dqlp_mmr_dir_privec3_u {
	mmr_t	sh_md_dqlp_mmr_dir_privec3_regval;
	struct {
		mmr_t	reserved_0  : 36;
		mmr_t	out         : 14;
		mmr_t	in          : 14;
	} sh_md_dqlp_mmr_dir_privec3_s;
} sh_md_dqlp_mmr_dir_privec3_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQLP_MMR_DIR_PRIVEC4"                 */
/*                      privilege vector for acc=4                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_dir_privec4_u {
	mmr_t	sh_md_dqlp_mmr_dir_privec4_regval;
	struct {
		mmr_t	in          : 14;
		mmr_t	out         : 14;
		mmr_t	reserved_0  : 36;
	} sh_md_dqlp_mmr_dir_privec4_s;
} sh_md_dqlp_mmr_dir_privec4_u_t;
#else
typedef union sh_md_dqlp_mmr_dir_privec4_u {
	mmr_t	sh_md_dqlp_mmr_dir_privec4_regval;
	struct {
		mmr_t	reserved_0  : 36;
		mmr_t	out         : 14;
		mmr_t	in          : 14;
	} sh_md_dqlp_mmr_dir_privec4_s;
} sh_md_dqlp_mmr_dir_privec4_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQLP_MMR_DIR_PRIVEC5"                 */
/*                      privilege vector for acc=5                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_dir_privec5_u {
	mmr_t	sh_md_dqlp_mmr_dir_privec5_regval;
	struct {
		mmr_t	in          : 14;
		mmr_t	out         : 14;
		mmr_t	reserved_0  : 36;
	} sh_md_dqlp_mmr_dir_privec5_s;
} sh_md_dqlp_mmr_dir_privec5_u_t;
#else
typedef union sh_md_dqlp_mmr_dir_privec5_u {
	mmr_t	sh_md_dqlp_mmr_dir_privec5_regval;
	struct {
		mmr_t	reserved_0  : 36;
		mmr_t	out         : 14;
		mmr_t	in          : 14;
	} sh_md_dqlp_mmr_dir_privec5_s;
} sh_md_dqlp_mmr_dir_privec5_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQLP_MMR_DIR_PRIVEC6"                 */
/*                      privilege vector for acc=6                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_dir_privec6_u {
	mmr_t	sh_md_dqlp_mmr_dir_privec6_regval;
	struct {
		mmr_t	in          : 14;
		mmr_t	out         : 14;
		mmr_t	reserved_0  : 36;
	} sh_md_dqlp_mmr_dir_privec6_s;
} sh_md_dqlp_mmr_dir_privec6_u_t;
#else
typedef union sh_md_dqlp_mmr_dir_privec6_u {
	mmr_t	sh_md_dqlp_mmr_dir_privec6_regval;
	struct {
		mmr_t	reserved_0  : 36;
		mmr_t	out         : 14;
		mmr_t	in          : 14;
	} sh_md_dqlp_mmr_dir_privec6_s;
} sh_md_dqlp_mmr_dir_privec6_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQLP_MMR_DIR_PRIVEC7"                 */
/*                      privilege vector for acc=7                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_dir_privec7_u {
	mmr_t	sh_md_dqlp_mmr_dir_privec7_regval;
	struct {
		mmr_t	in          : 14;
		mmr_t	out         : 14;
		mmr_t	reserved_0  : 36;
	} sh_md_dqlp_mmr_dir_privec7_s;
} sh_md_dqlp_mmr_dir_privec7_u_t;
#else
typedef union sh_md_dqlp_mmr_dir_privec7_u {
	mmr_t	sh_md_dqlp_mmr_dir_privec7_regval;
	struct {
		mmr_t	reserved_0  : 36;
		mmr_t	out         : 14;
		mmr_t	in          : 14;
	} sh_md_dqlp_mmr_dir_privec7_s;
} sh_md_dqlp_mmr_dir_privec7_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_MD_DQLP_MMR_DIR_TIMER"                  */
/*                            MD SXRO timer                             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_dir_timer_u {
	mmr_t	sh_md_dqlp_mmr_dir_timer_regval;
	struct {
		mmr_t	timer_div   : 12;
		mmr_t	timer_en    : 1;
		mmr_t	timer_cur   : 9;
		mmr_t	reserved_0  : 42;
	} sh_md_dqlp_mmr_dir_timer_s;
} sh_md_dqlp_mmr_dir_timer_u_t;
#else
typedef union sh_md_dqlp_mmr_dir_timer_u {
	mmr_t	sh_md_dqlp_mmr_dir_timer_regval;
	struct {
		mmr_t	reserved_0  : 42;
		mmr_t	timer_cur   : 9;
		mmr_t	timer_en    : 1;
		mmr_t	timer_div   : 12;
	} sh_md_dqlp_mmr_dir_timer_s;
} sh_md_dqlp_mmr_dir_timer_u_t;
#endif

/* ==================================================================== */
/*              Register "SH_MD_DQLP_MMR_PIOWD_DIR_ENTRY"               */
/*                       directory pio write data                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_piowd_dir_entry_u {
	mmr_t	sh_md_dqlp_mmr_piowd_dir_entry_regval;
	struct {
		mmr_t	dira        : 26;
		mmr_t	dirb        : 26;
		mmr_t	pri         : 3;
		mmr_t	acc         : 3;
		mmr_t	reserved_0  : 6;
	} sh_md_dqlp_mmr_piowd_dir_entry_s;
} sh_md_dqlp_mmr_piowd_dir_entry_u_t;
#else
typedef union sh_md_dqlp_mmr_piowd_dir_entry_u {
	mmr_t	sh_md_dqlp_mmr_piowd_dir_entry_regval;
	struct {
		mmr_t	reserved_0  : 6;
		mmr_t	acc         : 3;
		mmr_t	pri         : 3;
		mmr_t	dirb        : 26;
		mmr_t	dira        : 26;
	} sh_md_dqlp_mmr_piowd_dir_entry_s;
} sh_md_dqlp_mmr_piowd_dir_entry_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_MD_DQLP_MMR_PIOWD_DIR_ECC"                */
/*                        directory ecc register                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_piowd_dir_ecc_u {
	mmr_t	sh_md_dqlp_mmr_piowd_dir_ecc_regval;
	struct {
		mmr_t	ecca        : 7;
		mmr_t	eccb        : 7;
		mmr_t	reserved_0  : 50;
	} sh_md_dqlp_mmr_piowd_dir_ecc_s;
} sh_md_dqlp_mmr_piowd_dir_ecc_u_t;
#else
typedef union sh_md_dqlp_mmr_piowd_dir_ecc_u {
	mmr_t	sh_md_dqlp_mmr_piowd_dir_ecc_regval;
	struct {
		mmr_t	reserved_0  : 50;
		mmr_t	eccb        : 7;
		mmr_t	ecca        : 7;
	} sh_md_dqlp_mmr_piowd_dir_ecc_s;
} sh_md_dqlp_mmr_piowd_dir_ecc_u_t;
#endif

/* ==================================================================== */
/*             Register "SH_MD_DQLP_MMR_XPIORD_XDIR_ENTRY"              */
/*                      x directory pio read data                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_xpiord_xdir_entry_u {
	mmr_t	sh_md_dqlp_mmr_xpiord_xdir_entry_regval;
	struct {
		mmr_t	dira        : 26;
		mmr_t	dirb        : 26;
		mmr_t	pri         : 3;
		mmr_t	acc         : 3;
		mmr_t	cor         : 1;
		mmr_t	unc         : 1;
		mmr_t	reserved_0  : 4;
	} sh_md_dqlp_mmr_xpiord_xdir_entry_s;
} sh_md_dqlp_mmr_xpiord_xdir_entry_u_t;
#else
typedef union sh_md_dqlp_mmr_xpiord_xdir_entry_u {
	mmr_t	sh_md_dqlp_mmr_xpiord_xdir_entry_regval;
	struct {
		mmr_t	reserved_0  : 4;
		mmr_t	unc         : 1;
		mmr_t	cor         : 1;
		mmr_t	acc         : 3;
		mmr_t	pri         : 3;
		mmr_t	dirb        : 26;
		mmr_t	dira        : 26;
	} sh_md_dqlp_mmr_xpiord_xdir_entry_s;
} sh_md_dqlp_mmr_xpiord_xdir_entry_u_t;
#endif

/* ==================================================================== */
/*              Register "SH_MD_DQLP_MMR_XPIORD_XDIR_ECC"               */
/*                           x directory ecc                            */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_xpiord_xdir_ecc_u {
	mmr_t	sh_md_dqlp_mmr_xpiord_xdir_ecc_regval;
	struct {
		mmr_t	ecca        : 7;
		mmr_t	eccb        : 7;
		mmr_t	reserved_0  : 50;
	} sh_md_dqlp_mmr_xpiord_xdir_ecc_s;
} sh_md_dqlp_mmr_xpiord_xdir_ecc_u_t;
#else
typedef union sh_md_dqlp_mmr_xpiord_xdir_ecc_u {
	mmr_t	sh_md_dqlp_mmr_xpiord_xdir_ecc_regval;
	struct {
		mmr_t	reserved_0  : 50;
		mmr_t	eccb        : 7;
		mmr_t	ecca        : 7;
	} sh_md_dqlp_mmr_xpiord_xdir_ecc_s;
} sh_md_dqlp_mmr_xpiord_xdir_ecc_u_t;
#endif

/* ==================================================================== */
/*             Register "SH_MD_DQLP_MMR_YPIORD_YDIR_ENTRY"              */
/*                      y directory pio read data                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_ypiord_ydir_entry_u {
	mmr_t	sh_md_dqlp_mmr_ypiord_ydir_entry_regval;
	struct {
		mmr_t	dira        : 26;
		mmr_t	dirb        : 26;
		mmr_t	pri         : 3;
		mmr_t	acc         : 3;
		mmr_t	cor         : 1;
		mmr_t	unc         : 1;
		mmr_t	reserved_0  : 4;
	} sh_md_dqlp_mmr_ypiord_ydir_entry_s;
} sh_md_dqlp_mmr_ypiord_ydir_entry_u_t;
#else
typedef union sh_md_dqlp_mmr_ypiord_ydir_entry_u {
	mmr_t	sh_md_dqlp_mmr_ypiord_ydir_entry_regval;
	struct {
		mmr_t	reserved_0  : 4;
		mmr_t	unc         : 1;
		mmr_t	cor         : 1;
		mmr_t	acc         : 3;
		mmr_t	pri         : 3;
		mmr_t	dirb        : 26;
		mmr_t	dira        : 26;
	} sh_md_dqlp_mmr_ypiord_ydir_entry_s;
} sh_md_dqlp_mmr_ypiord_ydir_entry_u_t;
#endif

/* ==================================================================== */
/*              Register "SH_MD_DQLP_MMR_YPIORD_YDIR_ECC"               */
/*                           y directory ecc                            */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_ypiord_ydir_ecc_u {
	mmr_t	sh_md_dqlp_mmr_ypiord_ydir_ecc_regval;
	struct {
		mmr_t	ecca        : 7;
		mmr_t	eccb        : 7;
		mmr_t	reserved_0  : 50;
	} sh_md_dqlp_mmr_ypiord_ydir_ecc_s;
} sh_md_dqlp_mmr_ypiord_ydir_ecc_u_t;
#else
typedef union sh_md_dqlp_mmr_ypiord_ydir_ecc_u {
	mmr_t	sh_md_dqlp_mmr_ypiord_ydir_ecc_regval;
	struct {
		mmr_t	reserved_0  : 50;
		mmr_t	eccb        : 7;
		mmr_t	ecca        : 7;
	} sh_md_dqlp_mmr_ypiord_ydir_ecc_s;
} sh_md_dqlp_mmr_ypiord_ydir_ecc_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_MD_DQLP_MMR_XCERR1"                   */
/*              correctable dir ecc group 1 error register              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_xcerr1_u {
	mmr_t	sh_md_dqlp_mmr_xcerr1_regval;
	struct {
		mmr_t	grp1        : 36;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	arm         : 1;
		mmr_t	reserved_0  : 25;
	} sh_md_dqlp_mmr_xcerr1_s;
} sh_md_dqlp_mmr_xcerr1_u_t;
#else
typedef union sh_md_dqlp_mmr_xcerr1_u {
	mmr_t	sh_md_dqlp_mmr_xcerr1_regval;
	struct {
		mmr_t	reserved_0  : 25;
		mmr_t	arm         : 1;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	grp1        : 36;
	} sh_md_dqlp_mmr_xcerr1_s;
} sh_md_dqlp_mmr_xcerr1_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_MD_DQLP_MMR_XCERR2"                   */
/*              correctable dir ecc group 2 error register              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_xcerr2_u {
	mmr_t	sh_md_dqlp_mmr_xcerr2_regval;
	struct {
		mmr_t	grp2        : 36;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	reserved_0  : 26;
	} sh_md_dqlp_mmr_xcerr2_s;
} sh_md_dqlp_mmr_xcerr2_u_t;
#else
typedef union sh_md_dqlp_mmr_xcerr2_u {
	mmr_t	sh_md_dqlp_mmr_xcerr2_regval;
	struct {
		mmr_t	reserved_0  : 26;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	grp2        : 36;
	} sh_md_dqlp_mmr_xcerr2_s;
} sh_md_dqlp_mmr_xcerr2_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_MD_DQLP_MMR_XUERR1"                   */
/*             uncorrectable dir ecc group 1 error register             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_xuerr1_u {
	mmr_t	sh_md_dqlp_mmr_xuerr1_regval;
	struct {
		mmr_t	grp1        : 36;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	arm         : 1;
		mmr_t	reserved_0  : 25;
	} sh_md_dqlp_mmr_xuerr1_s;
} sh_md_dqlp_mmr_xuerr1_u_t;
#else
typedef union sh_md_dqlp_mmr_xuerr1_u {
	mmr_t	sh_md_dqlp_mmr_xuerr1_regval;
	struct {
		mmr_t	reserved_0  : 25;
		mmr_t	arm         : 1;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	grp1        : 36;
	} sh_md_dqlp_mmr_xuerr1_s;
} sh_md_dqlp_mmr_xuerr1_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_MD_DQLP_MMR_XUERR2"                   */
/*             uncorrectable dir ecc group 2 error register             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_xuerr2_u {
	mmr_t	sh_md_dqlp_mmr_xuerr2_regval;
	struct {
		mmr_t	grp2        : 36;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	reserved_0  : 26;
	} sh_md_dqlp_mmr_xuerr2_s;
} sh_md_dqlp_mmr_xuerr2_u_t;
#else
typedef union sh_md_dqlp_mmr_xuerr2_u {
	mmr_t	sh_md_dqlp_mmr_xuerr2_regval;
	struct {
		mmr_t	reserved_0  : 26;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	grp2        : 36;
	} sh_md_dqlp_mmr_xuerr2_s;
} sh_md_dqlp_mmr_xuerr2_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_MD_DQLP_MMR_XPERR"                    */
/*                       protocol error register                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_xperr_u {
	mmr_t	sh_md_dqlp_mmr_xperr_regval;
	struct {
		mmr_t	dir         : 26;
		mmr_t	cmd         : 8;
		mmr_t	src         : 14;
		mmr_t	prige       : 1;
		mmr_t	priv        : 1;
		mmr_t	cor         : 1;
		mmr_t	unc         : 1;
		mmr_t	mybit       : 8;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	arm         : 1;
		mmr_t	reserved_0  : 1;
	} sh_md_dqlp_mmr_xperr_s;
} sh_md_dqlp_mmr_xperr_u_t;
#else
typedef union sh_md_dqlp_mmr_xperr_u {
	mmr_t	sh_md_dqlp_mmr_xperr_regval;
	struct {
		mmr_t	reserved_0  : 1;
		mmr_t	arm         : 1;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	mybit       : 8;
		mmr_t	unc         : 1;
		mmr_t	cor         : 1;
		mmr_t	priv        : 1;
		mmr_t	prige       : 1;
		mmr_t	src         : 14;
		mmr_t	cmd         : 8;
		mmr_t	dir         : 26;
	} sh_md_dqlp_mmr_xperr_s;
} sh_md_dqlp_mmr_xperr_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_MD_DQLP_MMR_YCERR1"                   */
/*              correctable dir ecc group 1 error register              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_ycerr1_u {
	mmr_t	sh_md_dqlp_mmr_ycerr1_regval;
	struct {
		mmr_t	grp1        : 36;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	arm         : 1;
		mmr_t	reserved_0  : 25;
	} sh_md_dqlp_mmr_ycerr1_s;
} sh_md_dqlp_mmr_ycerr1_u_t;
#else
typedef union sh_md_dqlp_mmr_ycerr1_u {
	mmr_t	sh_md_dqlp_mmr_ycerr1_regval;
	struct {
		mmr_t	reserved_0  : 25;
		mmr_t	arm         : 1;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	grp1        : 36;
	} sh_md_dqlp_mmr_ycerr1_s;
} sh_md_dqlp_mmr_ycerr1_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_MD_DQLP_MMR_YCERR2"                   */
/*              correctable dir ecc group 2 error register              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_ycerr2_u {
	mmr_t	sh_md_dqlp_mmr_ycerr2_regval;
	struct {
		mmr_t	grp2        : 36;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	reserved_0  : 26;
	} sh_md_dqlp_mmr_ycerr2_s;
} sh_md_dqlp_mmr_ycerr2_u_t;
#else
typedef union sh_md_dqlp_mmr_ycerr2_u {
	mmr_t	sh_md_dqlp_mmr_ycerr2_regval;
	struct {
		mmr_t	reserved_0  : 26;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	grp2        : 36;
	} sh_md_dqlp_mmr_ycerr2_s;
} sh_md_dqlp_mmr_ycerr2_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_MD_DQLP_MMR_YUERR1"                   */
/*             uncorrectable dir ecc group 1 error register             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_yuerr1_u {
	mmr_t	sh_md_dqlp_mmr_yuerr1_regval;
	struct {
		mmr_t	grp1        : 36;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	arm         : 1;
		mmr_t	reserved_0  : 25;
	} sh_md_dqlp_mmr_yuerr1_s;
} sh_md_dqlp_mmr_yuerr1_u_t;
#else
typedef union sh_md_dqlp_mmr_yuerr1_u {
	mmr_t	sh_md_dqlp_mmr_yuerr1_regval;
	struct {
		mmr_t	reserved_0  : 25;
		mmr_t	arm         : 1;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	grp1        : 36;
	} sh_md_dqlp_mmr_yuerr1_s;
} sh_md_dqlp_mmr_yuerr1_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_MD_DQLP_MMR_YUERR2"                   */
/*             uncorrectable dir ecc group 2 error register             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_yuerr2_u {
	mmr_t	sh_md_dqlp_mmr_yuerr2_regval;
	struct {
		mmr_t	grp2        : 36;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	reserved_0  : 26;
	} sh_md_dqlp_mmr_yuerr2_s;
} sh_md_dqlp_mmr_yuerr2_u_t;
#else
typedef union sh_md_dqlp_mmr_yuerr2_u {
	mmr_t	sh_md_dqlp_mmr_yuerr2_regval;
	struct {
		mmr_t	reserved_0  : 26;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	grp2        : 36;
	} sh_md_dqlp_mmr_yuerr2_s;
} sh_md_dqlp_mmr_yuerr2_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_MD_DQLP_MMR_YPERR"                    */
/*                       protocol error register                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_yperr_u {
	mmr_t	sh_md_dqlp_mmr_yperr_regval;
	struct {
		mmr_t	dir         : 26;
		mmr_t	cmd         : 8;
		mmr_t	src         : 14;
		mmr_t	prige       : 1;
		mmr_t	priv        : 1;
		mmr_t	cor         : 1;
		mmr_t	unc         : 1;
		mmr_t	mybit       : 8;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	arm         : 1;
		mmr_t	reserved_0  : 1;
	} sh_md_dqlp_mmr_yperr_s;
} sh_md_dqlp_mmr_yperr_u_t;
#else
typedef union sh_md_dqlp_mmr_yperr_u {
	mmr_t	sh_md_dqlp_mmr_yperr_regval;
	struct {
		mmr_t	reserved_0  : 1;
		mmr_t	arm         : 1;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	mybit       : 8;
		mmr_t	unc         : 1;
		mmr_t	cor         : 1;
		mmr_t	priv        : 1;
		mmr_t	prige       : 1;
		mmr_t	src         : 14;
		mmr_t	cmd         : 8;
		mmr_t	dir         : 26;
	} sh_md_dqlp_mmr_yperr_s;
} sh_md_dqlp_mmr_yperr_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQLP_MMR_DIR_CMDTRIG"                 */
/*                             cmd triggers                             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_dir_cmdtrig_u {
	mmr_t	sh_md_dqlp_mmr_dir_cmdtrig_regval;
	struct {
		mmr_t	cmd0        : 8;
		mmr_t	cmd1        : 8;
		mmr_t	cmd2        : 8;
		mmr_t	cmd3        : 8;
		mmr_t	reserved_0  : 32;
	} sh_md_dqlp_mmr_dir_cmdtrig_s;
} sh_md_dqlp_mmr_dir_cmdtrig_u_t;
#else
typedef union sh_md_dqlp_mmr_dir_cmdtrig_u {
	mmr_t	sh_md_dqlp_mmr_dir_cmdtrig_regval;
	struct {
		mmr_t	reserved_0  : 32;
		mmr_t	cmd3        : 8;
		mmr_t	cmd2        : 8;
		mmr_t	cmd1        : 8;
		mmr_t	cmd0        : 8;
	} sh_md_dqlp_mmr_dir_cmdtrig_s;
} sh_md_dqlp_mmr_dir_cmdtrig_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQLP_MMR_DIR_TBLTRIG"                 */
/*                          dir table trigger                           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_dir_tbltrig_u {
	mmr_t	sh_md_dqlp_mmr_dir_tbltrig_regval;
	struct {
		mmr_t	src         : 14;
		mmr_t	cmd         : 8;
		mmr_t	acc         : 2;
		mmr_t	prige       : 1;
		mmr_t	dirst       : 9;
		mmr_t	mybit       : 8;
		mmr_t	reserved_0  : 22;
	} sh_md_dqlp_mmr_dir_tbltrig_s;
} sh_md_dqlp_mmr_dir_tbltrig_u_t;
#else
typedef union sh_md_dqlp_mmr_dir_tbltrig_u {
	mmr_t	sh_md_dqlp_mmr_dir_tbltrig_regval;
	struct {
		mmr_t	reserved_0  : 22;
		mmr_t	mybit       : 8;
		mmr_t	dirst       : 9;
		mmr_t	prige       : 1;
		mmr_t	acc         : 2;
		mmr_t	cmd         : 8;
		mmr_t	src         : 14;
	} sh_md_dqlp_mmr_dir_tbltrig_s;
} sh_md_dqlp_mmr_dir_tbltrig_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQLP_MMR_DIR_TBLMASK"                 */
/*                        dir table trigger mask                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_dir_tblmask_u {
	mmr_t	sh_md_dqlp_mmr_dir_tblmask_regval;
	struct {
		mmr_t	src         : 14;
		mmr_t	cmd         : 8;
		mmr_t	acc         : 2;
		mmr_t	prige       : 1;
		mmr_t	dirst       : 9;
		mmr_t	mybit       : 8;
		mmr_t	reserved_0  : 22;
	} sh_md_dqlp_mmr_dir_tblmask_s;
} sh_md_dqlp_mmr_dir_tblmask_u_t;
#else
typedef union sh_md_dqlp_mmr_dir_tblmask_u {
	mmr_t	sh_md_dqlp_mmr_dir_tblmask_regval;
	struct {
		mmr_t	reserved_0  : 22;
		mmr_t	mybit       : 8;
		mmr_t	dirst       : 9;
		mmr_t	prige       : 1;
		mmr_t	acc         : 2;
		mmr_t	cmd         : 8;
		mmr_t	src         : 14;
	} sh_md_dqlp_mmr_dir_tblmask_s;
} sh_md_dqlp_mmr_dir_tblmask_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_MD_DQLP_MMR_XBIST_H"                   */
/*                    rising edge bist/fill pattern                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_xbist_h_u {
	mmr_t	sh_md_dqlp_mmr_xbist_h_regval;
	struct {
		mmr_t	pat         : 32;
		mmr_t	reserved_0  : 8;
		mmr_t	inv         : 1;
		mmr_t	rot         : 1;
		mmr_t	arm         : 1;
		mmr_t	reserved_1  : 21;
	} sh_md_dqlp_mmr_xbist_h_s;
} sh_md_dqlp_mmr_xbist_h_u_t;
#else
typedef union sh_md_dqlp_mmr_xbist_h_u {
	mmr_t	sh_md_dqlp_mmr_xbist_h_regval;
	struct {
		mmr_t	reserved_1  : 21;
		mmr_t	arm         : 1;
		mmr_t	rot         : 1;
		mmr_t	inv         : 1;
		mmr_t	reserved_0  : 8;
		mmr_t	pat         : 32;
	} sh_md_dqlp_mmr_xbist_h_s;
} sh_md_dqlp_mmr_xbist_h_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_MD_DQLP_MMR_XBIST_L"                   */
/*                    falling edge bist/fill pattern                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_xbist_l_u {
	mmr_t	sh_md_dqlp_mmr_xbist_l_regval;
	struct {
		mmr_t	pat         : 32;
		mmr_t	reserved_0  : 8;
		mmr_t	inv         : 1;
		mmr_t	rot         : 1;
		mmr_t	reserved_1  : 22;
	} sh_md_dqlp_mmr_xbist_l_s;
} sh_md_dqlp_mmr_xbist_l_u_t;
#else
typedef union sh_md_dqlp_mmr_xbist_l_u {
	mmr_t	sh_md_dqlp_mmr_xbist_l_regval;
	struct {
		mmr_t	reserved_1  : 22;
		mmr_t	rot         : 1;
		mmr_t	inv         : 1;
		mmr_t	reserved_0  : 8;
		mmr_t	pat         : 32;
	} sh_md_dqlp_mmr_xbist_l_s;
} sh_md_dqlp_mmr_xbist_l_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQLP_MMR_XBIST_ERR_H"                 */
/*                    rising edge bist error pattern                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_xbist_err_h_u {
	mmr_t	sh_md_dqlp_mmr_xbist_err_h_regval;
	struct {
		mmr_t	pat         : 32;
		mmr_t	reserved_0  : 8;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	reserved_1  : 22;
	} sh_md_dqlp_mmr_xbist_err_h_s;
} sh_md_dqlp_mmr_xbist_err_h_u_t;
#else
typedef union sh_md_dqlp_mmr_xbist_err_h_u {
	mmr_t	sh_md_dqlp_mmr_xbist_err_h_regval;
	struct {
		mmr_t	reserved_1  : 22;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	reserved_0  : 8;
		mmr_t	pat         : 32;
	} sh_md_dqlp_mmr_xbist_err_h_s;
} sh_md_dqlp_mmr_xbist_err_h_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQLP_MMR_XBIST_ERR_L"                 */
/*                   falling edge bist error pattern                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_xbist_err_l_u {
	mmr_t	sh_md_dqlp_mmr_xbist_err_l_regval;
	struct {
		mmr_t	pat         : 32;
		mmr_t	reserved_0  : 8;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	reserved_1  : 22;
	} sh_md_dqlp_mmr_xbist_err_l_s;
} sh_md_dqlp_mmr_xbist_err_l_u_t;
#else
typedef union sh_md_dqlp_mmr_xbist_err_l_u {
	mmr_t	sh_md_dqlp_mmr_xbist_err_l_regval;
	struct {
		mmr_t	reserved_1  : 22;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	reserved_0  : 8;
		mmr_t	pat         : 32;
	} sh_md_dqlp_mmr_xbist_err_l_s;
} sh_md_dqlp_mmr_xbist_err_l_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_MD_DQLP_MMR_YBIST_H"                   */
/*                    rising edge bist/fill pattern                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_ybist_h_u {
	mmr_t	sh_md_dqlp_mmr_ybist_h_regval;
	struct {
		mmr_t	pat         : 32;
		mmr_t	reserved_0  : 8;
		mmr_t	inv         : 1;
		mmr_t	rot         : 1;
		mmr_t	arm         : 1;
		mmr_t	reserved_1  : 21;
	} sh_md_dqlp_mmr_ybist_h_s;
} sh_md_dqlp_mmr_ybist_h_u_t;
#else
typedef union sh_md_dqlp_mmr_ybist_h_u {
	mmr_t	sh_md_dqlp_mmr_ybist_h_regval;
	struct {
		mmr_t	reserved_1  : 21;
		mmr_t	arm         : 1;
		mmr_t	rot         : 1;
		mmr_t	inv         : 1;
		mmr_t	reserved_0  : 8;
		mmr_t	pat         : 32;
	} sh_md_dqlp_mmr_ybist_h_s;
} sh_md_dqlp_mmr_ybist_h_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_MD_DQLP_MMR_YBIST_L"                   */
/*                    falling edge bist/fill pattern                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_ybist_l_u {
	mmr_t	sh_md_dqlp_mmr_ybist_l_regval;
	struct {
		mmr_t	pat         : 32;
		mmr_t	reserved_0  : 8;
		mmr_t	inv         : 1;
		mmr_t	rot         : 1;
		mmr_t	reserved_1  : 22;
	} sh_md_dqlp_mmr_ybist_l_s;
} sh_md_dqlp_mmr_ybist_l_u_t;
#else
typedef union sh_md_dqlp_mmr_ybist_l_u {
	mmr_t	sh_md_dqlp_mmr_ybist_l_regval;
	struct {
		mmr_t	reserved_1  : 22;
		mmr_t	rot         : 1;
		mmr_t	inv         : 1;
		mmr_t	reserved_0  : 8;
		mmr_t	pat         : 32;
	} sh_md_dqlp_mmr_ybist_l_s;
} sh_md_dqlp_mmr_ybist_l_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQLP_MMR_YBIST_ERR_H"                 */
/*                    rising edge bist error pattern                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_ybist_err_h_u {
	mmr_t	sh_md_dqlp_mmr_ybist_err_h_regval;
	struct {
		mmr_t	pat         : 32;
		mmr_t	reserved_0  : 8;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	reserved_1  : 22;
	} sh_md_dqlp_mmr_ybist_err_h_s;
} sh_md_dqlp_mmr_ybist_err_h_u_t;
#else
typedef union sh_md_dqlp_mmr_ybist_err_h_u {
	mmr_t	sh_md_dqlp_mmr_ybist_err_h_regval;
	struct {
		mmr_t	reserved_1  : 22;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	reserved_0  : 8;
		mmr_t	pat         : 32;
	} sh_md_dqlp_mmr_ybist_err_h_s;
} sh_md_dqlp_mmr_ybist_err_h_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQLP_MMR_YBIST_ERR_L"                 */
/*                   falling edge bist error pattern                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqlp_mmr_ybist_err_l_u {
	mmr_t	sh_md_dqlp_mmr_ybist_err_l_regval;
	struct {
		mmr_t	pat         : 32;
		mmr_t	reserved_0  : 8;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	reserved_1  : 22;
	} sh_md_dqlp_mmr_ybist_err_l_s;
} sh_md_dqlp_mmr_ybist_err_l_u_t;
#else
typedef union sh_md_dqlp_mmr_ybist_err_l_u {
	mmr_t	sh_md_dqlp_mmr_ybist_err_l_regval;
	struct {
		mmr_t	reserved_1  : 22;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	reserved_0  : 8;
		mmr_t	pat         : 32;
	} sh_md_dqlp_mmr_ybist_err_l_s;
} sh_md_dqlp_mmr_ybist_err_l_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_MD_DQLS_MMR_XBIST_H"                   */
/*                    rising edge bist/fill pattern                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqls_mmr_xbist_h_u {
	mmr_t	sh_md_dqls_mmr_xbist_h_regval;
	struct {
		mmr_t	pat         : 40;
		mmr_t	inv         : 1;
		mmr_t	rot         : 1;
		mmr_t	arm         : 1;
		mmr_t	reserved_0  : 21;
	} sh_md_dqls_mmr_xbist_h_s;
} sh_md_dqls_mmr_xbist_h_u_t;
#else
typedef union sh_md_dqls_mmr_xbist_h_u {
	mmr_t	sh_md_dqls_mmr_xbist_h_regval;
	struct {
		mmr_t	reserved_0  : 21;
		mmr_t	arm         : 1;
		mmr_t	rot         : 1;
		mmr_t	inv         : 1;
		mmr_t	pat         : 40;
	} sh_md_dqls_mmr_xbist_h_s;
} sh_md_dqls_mmr_xbist_h_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_MD_DQLS_MMR_XBIST_L"                   */
/*                    falling edge bist/fill pattern                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqls_mmr_xbist_l_u {
	mmr_t	sh_md_dqls_mmr_xbist_l_regval;
	struct {
		mmr_t	pat         : 40;
		mmr_t	inv         : 1;
		mmr_t	rot         : 1;
		mmr_t	reserved_0  : 22;
	} sh_md_dqls_mmr_xbist_l_s;
} sh_md_dqls_mmr_xbist_l_u_t;
#else
typedef union sh_md_dqls_mmr_xbist_l_u {
	mmr_t	sh_md_dqls_mmr_xbist_l_regval;
	struct {
		mmr_t	reserved_0  : 22;
		mmr_t	rot         : 1;
		mmr_t	inv         : 1;
		mmr_t	pat         : 40;
	} sh_md_dqls_mmr_xbist_l_s;
} sh_md_dqls_mmr_xbist_l_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQLS_MMR_XBIST_ERR_H"                 */
/*                    rising edge bist error pattern                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqls_mmr_xbist_err_h_u {
	mmr_t	sh_md_dqls_mmr_xbist_err_h_regval;
	struct {
		mmr_t	pat         : 40;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	reserved_0  : 22;
	} sh_md_dqls_mmr_xbist_err_h_s;
} sh_md_dqls_mmr_xbist_err_h_u_t;
#else
typedef union sh_md_dqls_mmr_xbist_err_h_u {
	mmr_t	sh_md_dqls_mmr_xbist_err_h_regval;
	struct {
		mmr_t	reserved_0  : 22;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	pat         : 40;
	} sh_md_dqls_mmr_xbist_err_h_s;
} sh_md_dqls_mmr_xbist_err_h_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQLS_MMR_XBIST_ERR_L"                 */
/*                   falling edge bist error pattern                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqls_mmr_xbist_err_l_u {
	mmr_t	sh_md_dqls_mmr_xbist_err_l_regval;
	struct {
		mmr_t	pat         : 40;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	reserved_0  : 22;
	} sh_md_dqls_mmr_xbist_err_l_s;
} sh_md_dqls_mmr_xbist_err_l_u_t;
#else
typedef union sh_md_dqls_mmr_xbist_err_l_u {
	mmr_t	sh_md_dqls_mmr_xbist_err_l_regval;
	struct {
		mmr_t	reserved_0  : 22;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	pat         : 40;
	} sh_md_dqls_mmr_xbist_err_l_s;
} sh_md_dqls_mmr_xbist_err_l_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_MD_DQLS_MMR_YBIST_H"                   */
/*                    rising edge bist/fill pattern                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqls_mmr_ybist_h_u {
	mmr_t	sh_md_dqls_mmr_ybist_h_regval;
	struct {
		mmr_t	pat         : 40;
		mmr_t	inv         : 1;
		mmr_t	rot         : 1;
		mmr_t	arm         : 1;
		mmr_t	reserved_0  : 21;
	} sh_md_dqls_mmr_ybist_h_s;
} sh_md_dqls_mmr_ybist_h_u_t;
#else
typedef union sh_md_dqls_mmr_ybist_h_u {
	mmr_t	sh_md_dqls_mmr_ybist_h_regval;
	struct {
		mmr_t	reserved_0  : 21;
		mmr_t	arm         : 1;
		mmr_t	rot         : 1;
		mmr_t	inv         : 1;
		mmr_t	pat         : 40;
	} sh_md_dqls_mmr_ybist_h_s;
} sh_md_dqls_mmr_ybist_h_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_MD_DQLS_MMR_YBIST_L"                   */
/*                    falling edge bist/fill pattern                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqls_mmr_ybist_l_u {
	mmr_t	sh_md_dqls_mmr_ybist_l_regval;
	struct {
		mmr_t	pat         : 40;
		mmr_t	inv         : 1;
		mmr_t	rot         : 1;
		mmr_t	reserved_0  : 22;
	} sh_md_dqls_mmr_ybist_l_s;
} sh_md_dqls_mmr_ybist_l_u_t;
#else
typedef union sh_md_dqls_mmr_ybist_l_u {
	mmr_t	sh_md_dqls_mmr_ybist_l_regval;
	struct {
		mmr_t	reserved_0  : 22;
		mmr_t	rot         : 1;
		mmr_t	inv         : 1;
		mmr_t	pat         : 40;
	} sh_md_dqls_mmr_ybist_l_s;
} sh_md_dqls_mmr_ybist_l_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQLS_MMR_YBIST_ERR_H"                 */
/*                    rising edge bist error pattern                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqls_mmr_ybist_err_h_u {
	mmr_t	sh_md_dqls_mmr_ybist_err_h_regval;
	struct {
		mmr_t	pat         : 40;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	reserved_0  : 22;
	} sh_md_dqls_mmr_ybist_err_h_s;
} sh_md_dqls_mmr_ybist_err_h_u_t;
#else
typedef union sh_md_dqls_mmr_ybist_err_h_u {
	mmr_t	sh_md_dqls_mmr_ybist_err_h_regval;
	struct {
		mmr_t	reserved_0  : 22;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	pat         : 40;
	} sh_md_dqls_mmr_ybist_err_h_s;
} sh_md_dqls_mmr_ybist_err_h_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQLS_MMR_YBIST_ERR_L"                 */
/*                   falling edge bist error pattern                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqls_mmr_ybist_err_l_u {
	mmr_t	sh_md_dqls_mmr_ybist_err_l_regval;
	struct {
		mmr_t	pat         : 40;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	reserved_0  : 22;
	} sh_md_dqls_mmr_ybist_err_l_s;
} sh_md_dqls_mmr_ybist_err_l_u_t;
#else
typedef union sh_md_dqls_mmr_ybist_err_l_u {
	mmr_t	sh_md_dqls_mmr_ybist_err_l_regval;
	struct {
		mmr_t	reserved_0  : 22;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	pat         : 40;
	} sh_md_dqls_mmr_ybist_err_l_s;
} sh_md_dqls_mmr_ybist_err_l_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_MD_DQLS_MMR_JNR_DEBUG"                  */
/*                    joiner/fct debug configuration                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqls_mmr_jnr_debug_u {
	mmr_t	sh_md_dqls_mmr_jnr_debug_regval;
	struct {
		mmr_t	px          : 1;
		mmr_t	rw          : 1;
		mmr_t	reserved_0  : 62;
	} sh_md_dqls_mmr_jnr_debug_s;
} sh_md_dqls_mmr_jnr_debug_u_t;
#else
typedef union sh_md_dqls_mmr_jnr_debug_u {
	mmr_t	sh_md_dqls_mmr_jnr_debug_regval;
	struct {
		mmr_t	reserved_0  : 62;
		mmr_t	rw          : 1;
		mmr_t	px          : 1;
	} sh_md_dqls_mmr_jnr_debug_s;
} sh_md_dqls_mmr_jnr_debug_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_MD_DQLS_MMR_XAMOPW_ERR"                 */
/*                  amo/partial rmw ecc error register                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqls_mmr_xamopw_err_u {
	mmr_t	sh_md_dqls_mmr_xamopw_err_regval;
	struct {
		mmr_t	ssyn        : 8;
		mmr_t	scor        : 1;
		mmr_t	sunc        : 1;
		mmr_t	reserved_0  : 6;
		mmr_t	rsyn        : 8;
		mmr_t	rcor        : 1;
		mmr_t	runc        : 1;
		mmr_t	reserved_1  : 6;
		mmr_t	arm         : 1;
		mmr_t	reserved_2  : 31;
	} sh_md_dqls_mmr_xamopw_err_s;
} sh_md_dqls_mmr_xamopw_err_u_t;
#else
typedef union sh_md_dqls_mmr_xamopw_err_u {
	mmr_t	sh_md_dqls_mmr_xamopw_err_regval;
	struct {
		mmr_t	reserved_2  : 31;
		mmr_t	arm         : 1;
		mmr_t	reserved_1  : 6;
		mmr_t	runc        : 1;
		mmr_t	rcor        : 1;
		mmr_t	rsyn        : 8;
		mmr_t	reserved_0  : 6;
		mmr_t	sunc        : 1;
		mmr_t	scor        : 1;
		mmr_t	ssyn        : 8;
	} sh_md_dqls_mmr_xamopw_err_s;
} sh_md_dqls_mmr_xamopw_err_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_MD_DQRP_MMR_DIR_CONFIG"                 */
/*                     DQ directory config register                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_dir_config_u {
	mmr_t	sh_md_dqrp_mmr_dir_config_regval;
	struct {
		mmr_t	sys_size    : 3;
		mmr_t	en_direcc   : 1;
		mmr_t	en_dirpois  : 1;
		mmr_t	reserved_0  : 59;
	} sh_md_dqrp_mmr_dir_config_s;
} sh_md_dqrp_mmr_dir_config_u_t;
#else
typedef union sh_md_dqrp_mmr_dir_config_u {
	mmr_t	sh_md_dqrp_mmr_dir_config_regval;
	struct {
		mmr_t	reserved_0  : 59;
		mmr_t	en_dirpois  : 1;
		mmr_t	en_direcc   : 1;
		mmr_t	sys_size    : 3;
	} sh_md_dqrp_mmr_dir_config_s;
} sh_md_dqrp_mmr_dir_config_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQRP_MMR_DIR_PRESVEC0"                */
/*                      node [63:0] presence bits                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_dir_presvec0_u {
	mmr_t	sh_md_dqrp_mmr_dir_presvec0_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqrp_mmr_dir_presvec0_s;
} sh_md_dqrp_mmr_dir_presvec0_u_t;
#else
typedef union sh_md_dqrp_mmr_dir_presvec0_u {
	mmr_t	sh_md_dqrp_mmr_dir_presvec0_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqrp_mmr_dir_presvec0_s;
} sh_md_dqrp_mmr_dir_presvec0_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQRP_MMR_DIR_PRESVEC1"                */
/*                     node [127:64] presence bits                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_dir_presvec1_u {
	mmr_t	sh_md_dqrp_mmr_dir_presvec1_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqrp_mmr_dir_presvec1_s;
} sh_md_dqrp_mmr_dir_presvec1_u_t;
#else
typedef union sh_md_dqrp_mmr_dir_presvec1_u {
	mmr_t	sh_md_dqrp_mmr_dir_presvec1_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqrp_mmr_dir_presvec1_s;
} sh_md_dqrp_mmr_dir_presvec1_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQRP_MMR_DIR_PRESVEC2"                */
/*                     node [191:128] presence bits                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_dir_presvec2_u {
	mmr_t	sh_md_dqrp_mmr_dir_presvec2_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqrp_mmr_dir_presvec2_s;
} sh_md_dqrp_mmr_dir_presvec2_u_t;
#else
typedef union sh_md_dqrp_mmr_dir_presvec2_u {
	mmr_t	sh_md_dqrp_mmr_dir_presvec2_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqrp_mmr_dir_presvec2_s;
} sh_md_dqrp_mmr_dir_presvec2_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQRP_MMR_DIR_PRESVEC3"                */
/*                     node [255:192] presence bits                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_dir_presvec3_u {
	mmr_t	sh_md_dqrp_mmr_dir_presvec3_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqrp_mmr_dir_presvec3_s;
} sh_md_dqrp_mmr_dir_presvec3_u_t;
#else
typedef union sh_md_dqrp_mmr_dir_presvec3_u {
	mmr_t	sh_md_dqrp_mmr_dir_presvec3_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqrp_mmr_dir_presvec3_s;
} sh_md_dqrp_mmr_dir_presvec3_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQRP_MMR_DIR_LOCVEC0"                 */
/*                        local vector for acc=0                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_dir_locvec0_u {
	mmr_t	sh_md_dqrp_mmr_dir_locvec0_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqrp_mmr_dir_locvec0_s;
} sh_md_dqrp_mmr_dir_locvec0_u_t;
#else
typedef union sh_md_dqrp_mmr_dir_locvec0_u {
	mmr_t	sh_md_dqrp_mmr_dir_locvec0_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqrp_mmr_dir_locvec0_s;
} sh_md_dqrp_mmr_dir_locvec0_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQRP_MMR_DIR_LOCVEC1"                 */
/*                        local vector for acc=1                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_dir_locvec1_u {
	mmr_t	sh_md_dqrp_mmr_dir_locvec1_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqrp_mmr_dir_locvec1_s;
} sh_md_dqrp_mmr_dir_locvec1_u_t;
#else
typedef union sh_md_dqrp_mmr_dir_locvec1_u {
	mmr_t	sh_md_dqrp_mmr_dir_locvec1_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqrp_mmr_dir_locvec1_s;
} sh_md_dqrp_mmr_dir_locvec1_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQRP_MMR_DIR_LOCVEC2"                 */
/*                        local vector for acc=2                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_dir_locvec2_u {
	mmr_t	sh_md_dqrp_mmr_dir_locvec2_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqrp_mmr_dir_locvec2_s;
} sh_md_dqrp_mmr_dir_locvec2_u_t;
#else
typedef union sh_md_dqrp_mmr_dir_locvec2_u {
	mmr_t	sh_md_dqrp_mmr_dir_locvec2_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqrp_mmr_dir_locvec2_s;
} sh_md_dqrp_mmr_dir_locvec2_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQRP_MMR_DIR_LOCVEC3"                 */
/*                        local vector for acc=3                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_dir_locvec3_u {
	mmr_t	sh_md_dqrp_mmr_dir_locvec3_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqrp_mmr_dir_locvec3_s;
} sh_md_dqrp_mmr_dir_locvec3_u_t;
#else
typedef union sh_md_dqrp_mmr_dir_locvec3_u {
	mmr_t	sh_md_dqrp_mmr_dir_locvec3_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqrp_mmr_dir_locvec3_s;
} sh_md_dqrp_mmr_dir_locvec3_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQRP_MMR_DIR_LOCVEC4"                 */
/*                        local vector for acc=4                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_dir_locvec4_u {
	mmr_t	sh_md_dqrp_mmr_dir_locvec4_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqrp_mmr_dir_locvec4_s;
} sh_md_dqrp_mmr_dir_locvec4_u_t;
#else
typedef union sh_md_dqrp_mmr_dir_locvec4_u {
	mmr_t	sh_md_dqrp_mmr_dir_locvec4_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqrp_mmr_dir_locvec4_s;
} sh_md_dqrp_mmr_dir_locvec4_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQRP_MMR_DIR_LOCVEC5"                 */
/*                        local vector for acc=5                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_dir_locvec5_u {
	mmr_t	sh_md_dqrp_mmr_dir_locvec5_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqrp_mmr_dir_locvec5_s;
} sh_md_dqrp_mmr_dir_locvec5_u_t;
#else
typedef union sh_md_dqrp_mmr_dir_locvec5_u {
	mmr_t	sh_md_dqrp_mmr_dir_locvec5_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqrp_mmr_dir_locvec5_s;
} sh_md_dqrp_mmr_dir_locvec5_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQRP_MMR_DIR_LOCVEC6"                 */
/*                        local vector for acc=6                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_dir_locvec6_u {
	mmr_t	sh_md_dqrp_mmr_dir_locvec6_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqrp_mmr_dir_locvec6_s;
} sh_md_dqrp_mmr_dir_locvec6_u_t;
#else
typedef union sh_md_dqrp_mmr_dir_locvec6_u {
	mmr_t	sh_md_dqrp_mmr_dir_locvec6_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqrp_mmr_dir_locvec6_s;
} sh_md_dqrp_mmr_dir_locvec6_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQRP_MMR_DIR_LOCVEC7"                 */
/*                        local vector for acc=7                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_dir_locvec7_u {
	mmr_t	sh_md_dqrp_mmr_dir_locvec7_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqrp_mmr_dir_locvec7_s;
} sh_md_dqrp_mmr_dir_locvec7_u_t;
#else
typedef union sh_md_dqrp_mmr_dir_locvec7_u {
	mmr_t	sh_md_dqrp_mmr_dir_locvec7_regval;
	struct {
		mmr_t	vec         : 64;
	} sh_md_dqrp_mmr_dir_locvec7_s;
} sh_md_dqrp_mmr_dir_locvec7_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQRP_MMR_DIR_PRIVEC0"                 */
/*                      privilege vector for acc=0                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_dir_privec0_u {
	mmr_t	sh_md_dqrp_mmr_dir_privec0_regval;
	struct {
		mmr_t	in          : 14;
		mmr_t	out         : 14;
		mmr_t	reserved_0  : 36;
	} sh_md_dqrp_mmr_dir_privec0_s;
} sh_md_dqrp_mmr_dir_privec0_u_t;
#else
typedef union sh_md_dqrp_mmr_dir_privec0_u {
	mmr_t	sh_md_dqrp_mmr_dir_privec0_regval;
	struct {
		mmr_t	reserved_0  : 36;
		mmr_t	out         : 14;
		mmr_t	in          : 14;
	} sh_md_dqrp_mmr_dir_privec0_s;
} sh_md_dqrp_mmr_dir_privec0_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQRP_MMR_DIR_PRIVEC1"                 */
/*                      privilege vector for acc=1                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_dir_privec1_u {
	mmr_t	sh_md_dqrp_mmr_dir_privec1_regval;
	struct {
		mmr_t	in          : 14;
		mmr_t	out         : 14;
		mmr_t	reserved_0  : 36;
	} sh_md_dqrp_mmr_dir_privec1_s;
} sh_md_dqrp_mmr_dir_privec1_u_t;
#else
typedef union sh_md_dqrp_mmr_dir_privec1_u {
	mmr_t	sh_md_dqrp_mmr_dir_privec1_regval;
	struct {
		mmr_t	reserved_0  : 36;
		mmr_t	out         : 14;
		mmr_t	in          : 14;
	} sh_md_dqrp_mmr_dir_privec1_s;
} sh_md_dqrp_mmr_dir_privec1_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQRP_MMR_DIR_PRIVEC2"                 */
/*                      privilege vector for acc=2                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_dir_privec2_u {
	mmr_t	sh_md_dqrp_mmr_dir_privec2_regval;
	struct {
		mmr_t	in          : 14;
		mmr_t	out         : 14;
		mmr_t	reserved_0  : 36;
	} sh_md_dqrp_mmr_dir_privec2_s;
} sh_md_dqrp_mmr_dir_privec2_u_t;
#else
typedef union sh_md_dqrp_mmr_dir_privec2_u {
	mmr_t	sh_md_dqrp_mmr_dir_privec2_regval;
	struct {
		mmr_t	reserved_0  : 36;
		mmr_t	out         : 14;
		mmr_t	in          : 14;
	} sh_md_dqrp_mmr_dir_privec2_s;
} sh_md_dqrp_mmr_dir_privec2_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQRP_MMR_DIR_PRIVEC3"                 */
/*                      privilege vector for acc=3                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_dir_privec3_u {
	mmr_t	sh_md_dqrp_mmr_dir_privec3_regval;
	struct {
		mmr_t	in          : 14;
		mmr_t	out         : 14;
		mmr_t	reserved_0  : 36;
	} sh_md_dqrp_mmr_dir_privec3_s;
} sh_md_dqrp_mmr_dir_privec3_u_t;
#else
typedef union sh_md_dqrp_mmr_dir_privec3_u {
	mmr_t	sh_md_dqrp_mmr_dir_privec3_regval;
	struct {
		mmr_t	reserved_0  : 36;
		mmr_t	out         : 14;
		mmr_t	in          : 14;
	} sh_md_dqrp_mmr_dir_privec3_s;
} sh_md_dqrp_mmr_dir_privec3_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQRP_MMR_DIR_PRIVEC4"                 */
/*                      privilege vector for acc=4                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_dir_privec4_u {
	mmr_t	sh_md_dqrp_mmr_dir_privec4_regval;
	struct {
		mmr_t	in          : 14;
		mmr_t	out         : 14;
		mmr_t	reserved_0  : 36;
	} sh_md_dqrp_mmr_dir_privec4_s;
} sh_md_dqrp_mmr_dir_privec4_u_t;
#else
typedef union sh_md_dqrp_mmr_dir_privec4_u {
	mmr_t	sh_md_dqrp_mmr_dir_privec4_regval;
	struct {
		mmr_t	reserved_0  : 36;
		mmr_t	out         : 14;
		mmr_t	in          : 14;
	} sh_md_dqrp_mmr_dir_privec4_s;
} sh_md_dqrp_mmr_dir_privec4_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQRP_MMR_DIR_PRIVEC5"                 */
/*                      privilege vector for acc=5                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_dir_privec5_u {
	mmr_t	sh_md_dqrp_mmr_dir_privec5_regval;
	struct {
		mmr_t	in          : 14;
		mmr_t	out         : 14;
		mmr_t	reserved_0  : 36;
	} sh_md_dqrp_mmr_dir_privec5_s;
} sh_md_dqrp_mmr_dir_privec5_u_t;
#else
typedef union sh_md_dqrp_mmr_dir_privec5_u {
	mmr_t	sh_md_dqrp_mmr_dir_privec5_regval;
	struct {
		mmr_t	reserved_0  : 36;
		mmr_t	out         : 14;
		mmr_t	in          : 14;
	} sh_md_dqrp_mmr_dir_privec5_s;
} sh_md_dqrp_mmr_dir_privec5_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQRP_MMR_DIR_PRIVEC6"                 */
/*                      privilege vector for acc=6                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_dir_privec6_u {
	mmr_t	sh_md_dqrp_mmr_dir_privec6_regval;
	struct {
		mmr_t	in          : 14;
		mmr_t	out         : 14;
		mmr_t	reserved_0  : 36;
	} sh_md_dqrp_mmr_dir_privec6_s;
} sh_md_dqrp_mmr_dir_privec6_u_t;
#else
typedef union sh_md_dqrp_mmr_dir_privec6_u {
	mmr_t	sh_md_dqrp_mmr_dir_privec6_regval;
	struct {
		mmr_t	reserved_0  : 36;
		mmr_t	out         : 14;
		mmr_t	in          : 14;
	} sh_md_dqrp_mmr_dir_privec6_s;
} sh_md_dqrp_mmr_dir_privec6_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQRP_MMR_DIR_PRIVEC7"                 */
/*                      privilege vector for acc=7                      */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_dir_privec7_u {
	mmr_t	sh_md_dqrp_mmr_dir_privec7_regval;
	struct {
		mmr_t	in          : 14;
		mmr_t	out         : 14;
		mmr_t	reserved_0  : 36;
	} sh_md_dqrp_mmr_dir_privec7_s;
} sh_md_dqrp_mmr_dir_privec7_u_t;
#else
typedef union sh_md_dqrp_mmr_dir_privec7_u {
	mmr_t	sh_md_dqrp_mmr_dir_privec7_regval;
	struct {
		mmr_t	reserved_0  : 36;
		mmr_t	out         : 14;
		mmr_t	in          : 14;
	} sh_md_dqrp_mmr_dir_privec7_s;
} sh_md_dqrp_mmr_dir_privec7_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_MD_DQRP_MMR_DIR_TIMER"                  */
/*                            MD SXRO timer                             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_dir_timer_u {
	mmr_t	sh_md_dqrp_mmr_dir_timer_regval;
	struct {
		mmr_t	timer_div   : 12;
		mmr_t	timer_en    : 1;
		mmr_t	timer_cur   : 9;
		mmr_t	reserved_0  : 42;
	} sh_md_dqrp_mmr_dir_timer_s;
} sh_md_dqrp_mmr_dir_timer_u_t;
#else
typedef union sh_md_dqrp_mmr_dir_timer_u {
	mmr_t	sh_md_dqrp_mmr_dir_timer_regval;
	struct {
		mmr_t	reserved_0  : 42;
		mmr_t	timer_cur   : 9;
		mmr_t	timer_en    : 1;
		mmr_t	timer_div   : 12;
	} sh_md_dqrp_mmr_dir_timer_s;
} sh_md_dqrp_mmr_dir_timer_u_t;
#endif

/* ==================================================================== */
/*              Register "SH_MD_DQRP_MMR_PIOWD_DIR_ENTRY"               */
/*                       directory pio write data                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_piowd_dir_entry_u {
	mmr_t	sh_md_dqrp_mmr_piowd_dir_entry_regval;
	struct {
		mmr_t	dira        : 26;
		mmr_t	dirb        : 26;
		mmr_t	pri         : 3;
		mmr_t	acc         : 3;
		mmr_t	reserved_0  : 6;
	} sh_md_dqrp_mmr_piowd_dir_entry_s;
} sh_md_dqrp_mmr_piowd_dir_entry_u_t;
#else
typedef union sh_md_dqrp_mmr_piowd_dir_entry_u {
	mmr_t	sh_md_dqrp_mmr_piowd_dir_entry_regval;
	struct {
		mmr_t	reserved_0  : 6;
		mmr_t	acc         : 3;
		mmr_t	pri         : 3;
		mmr_t	dirb        : 26;
		mmr_t	dira        : 26;
	} sh_md_dqrp_mmr_piowd_dir_entry_s;
} sh_md_dqrp_mmr_piowd_dir_entry_u_t;
#endif

/* ==================================================================== */
/*               Register "SH_MD_DQRP_MMR_PIOWD_DIR_ECC"                */
/*                        directory ecc register                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_piowd_dir_ecc_u {
	mmr_t	sh_md_dqrp_mmr_piowd_dir_ecc_regval;
	struct {
		mmr_t	ecca        : 7;
		mmr_t	eccb        : 7;
		mmr_t	reserved_0  : 50;
	} sh_md_dqrp_mmr_piowd_dir_ecc_s;
} sh_md_dqrp_mmr_piowd_dir_ecc_u_t;
#else
typedef union sh_md_dqrp_mmr_piowd_dir_ecc_u {
	mmr_t	sh_md_dqrp_mmr_piowd_dir_ecc_regval;
	struct {
		mmr_t	reserved_0  : 50;
		mmr_t	eccb        : 7;
		mmr_t	ecca        : 7;
	} sh_md_dqrp_mmr_piowd_dir_ecc_s;
} sh_md_dqrp_mmr_piowd_dir_ecc_u_t;
#endif

/* ==================================================================== */
/*             Register "SH_MD_DQRP_MMR_XPIORD_XDIR_ENTRY"              */
/*                      x directory pio read data                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_xpiord_xdir_entry_u {
	mmr_t	sh_md_dqrp_mmr_xpiord_xdir_entry_regval;
	struct {
		mmr_t	dira        : 26;
		mmr_t	dirb        : 26;
		mmr_t	pri         : 3;
		mmr_t	acc         : 3;
		mmr_t	cor         : 1;
		mmr_t	unc         : 1;
		mmr_t	reserved_0  : 4;
	} sh_md_dqrp_mmr_xpiord_xdir_entry_s;
} sh_md_dqrp_mmr_xpiord_xdir_entry_u_t;
#else
typedef union sh_md_dqrp_mmr_xpiord_xdir_entry_u {
	mmr_t	sh_md_dqrp_mmr_xpiord_xdir_entry_regval;
	struct {
		mmr_t	reserved_0  : 4;
		mmr_t	unc         : 1;
		mmr_t	cor         : 1;
		mmr_t	acc         : 3;
		mmr_t	pri         : 3;
		mmr_t	dirb        : 26;
		mmr_t	dira        : 26;
	} sh_md_dqrp_mmr_xpiord_xdir_entry_s;
} sh_md_dqrp_mmr_xpiord_xdir_entry_u_t;
#endif

/* ==================================================================== */
/*              Register "SH_MD_DQRP_MMR_XPIORD_XDIR_ECC"               */
/*                           x directory ecc                            */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_xpiord_xdir_ecc_u {
	mmr_t	sh_md_dqrp_mmr_xpiord_xdir_ecc_regval;
	struct {
		mmr_t	ecca        : 7;
		mmr_t	eccb        : 7;
		mmr_t	reserved_0  : 50;
	} sh_md_dqrp_mmr_xpiord_xdir_ecc_s;
} sh_md_dqrp_mmr_xpiord_xdir_ecc_u_t;
#else
typedef union sh_md_dqrp_mmr_xpiord_xdir_ecc_u {
	mmr_t	sh_md_dqrp_mmr_xpiord_xdir_ecc_regval;
	struct {
		mmr_t	reserved_0  : 50;
		mmr_t	eccb        : 7;
		mmr_t	ecca        : 7;
	} sh_md_dqrp_mmr_xpiord_xdir_ecc_s;
} sh_md_dqrp_mmr_xpiord_xdir_ecc_u_t;
#endif

/* ==================================================================== */
/*             Register "SH_MD_DQRP_MMR_YPIORD_YDIR_ENTRY"              */
/*                      y directory pio read data                       */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_ypiord_ydir_entry_u {
	mmr_t	sh_md_dqrp_mmr_ypiord_ydir_entry_regval;
	struct {
		mmr_t	dira        : 26;
		mmr_t	dirb        : 26;
		mmr_t	pri         : 3;
		mmr_t	acc         : 3;
		mmr_t	cor         : 1;
		mmr_t	unc         : 1;
		mmr_t	reserved_0  : 4;
	} sh_md_dqrp_mmr_ypiord_ydir_entry_s;
} sh_md_dqrp_mmr_ypiord_ydir_entry_u_t;
#else
typedef union sh_md_dqrp_mmr_ypiord_ydir_entry_u {
	mmr_t	sh_md_dqrp_mmr_ypiord_ydir_entry_regval;
	struct {
		mmr_t	reserved_0  : 4;
		mmr_t	unc         : 1;
		mmr_t	cor         : 1;
		mmr_t	acc         : 3;
		mmr_t	pri         : 3;
		mmr_t	dirb        : 26;
		mmr_t	dira        : 26;
	} sh_md_dqrp_mmr_ypiord_ydir_entry_s;
} sh_md_dqrp_mmr_ypiord_ydir_entry_u_t;
#endif

/* ==================================================================== */
/*              Register "SH_MD_DQRP_MMR_YPIORD_YDIR_ECC"               */
/*                           y directory ecc                            */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_ypiord_ydir_ecc_u {
	mmr_t	sh_md_dqrp_mmr_ypiord_ydir_ecc_regval;
	struct {
		mmr_t	ecca        : 7;
		mmr_t	eccb        : 7;
		mmr_t	reserved_0  : 50;
	} sh_md_dqrp_mmr_ypiord_ydir_ecc_s;
} sh_md_dqrp_mmr_ypiord_ydir_ecc_u_t;
#else
typedef union sh_md_dqrp_mmr_ypiord_ydir_ecc_u {
	mmr_t	sh_md_dqrp_mmr_ypiord_ydir_ecc_regval;
	struct {
		mmr_t	reserved_0  : 50;
		mmr_t	eccb        : 7;
		mmr_t	ecca        : 7;
	} sh_md_dqrp_mmr_ypiord_ydir_ecc_s;
} sh_md_dqrp_mmr_ypiord_ydir_ecc_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_MD_DQRP_MMR_XCERR1"                   */
/*              correctable dir ecc group 1 error register              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_xcerr1_u {
	mmr_t	sh_md_dqrp_mmr_xcerr1_regval;
	struct {
		mmr_t	grp1        : 36;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	arm         : 1;
		mmr_t	reserved_0  : 25;
	} sh_md_dqrp_mmr_xcerr1_s;
} sh_md_dqrp_mmr_xcerr1_u_t;
#else
typedef union sh_md_dqrp_mmr_xcerr1_u {
	mmr_t	sh_md_dqrp_mmr_xcerr1_regval;
	struct {
		mmr_t	reserved_0  : 25;
		mmr_t	arm         : 1;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	grp1        : 36;
	} sh_md_dqrp_mmr_xcerr1_s;
} sh_md_dqrp_mmr_xcerr1_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_MD_DQRP_MMR_XCERR2"                   */
/*              correctable dir ecc group 2 error register              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_xcerr2_u {
	mmr_t	sh_md_dqrp_mmr_xcerr2_regval;
	struct {
		mmr_t	grp2        : 36;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	reserved_0  : 26;
	} sh_md_dqrp_mmr_xcerr2_s;
} sh_md_dqrp_mmr_xcerr2_u_t;
#else
typedef union sh_md_dqrp_mmr_xcerr2_u {
	mmr_t	sh_md_dqrp_mmr_xcerr2_regval;
	struct {
		mmr_t	reserved_0  : 26;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	grp2        : 36;
	} sh_md_dqrp_mmr_xcerr2_s;
} sh_md_dqrp_mmr_xcerr2_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_MD_DQRP_MMR_XUERR1"                   */
/*             uncorrectable dir ecc group 1 error register             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_xuerr1_u {
	mmr_t	sh_md_dqrp_mmr_xuerr1_regval;
	struct {
		mmr_t	grp1        : 36;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	arm         : 1;
		mmr_t	reserved_0  : 25;
	} sh_md_dqrp_mmr_xuerr1_s;
} sh_md_dqrp_mmr_xuerr1_u_t;
#else
typedef union sh_md_dqrp_mmr_xuerr1_u {
	mmr_t	sh_md_dqrp_mmr_xuerr1_regval;
	struct {
		mmr_t	reserved_0  : 25;
		mmr_t	arm         : 1;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	grp1        : 36;
	} sh_md_dqrp_mmr_xuerr1_s;
} sh_md_dqrp_mmr_xuerr1_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_MD_DQRP_MMR_XUERR2"                   */
/*             uncorrectable dir ecc group 2 error register             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_xuerr2_u {
	mmr_t	sh_md_dqrp_mmr_xuerr2_regval;
	struct {
		mmr_t	grp2        : 36;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	reserved_0  : 26;
	} sh_md_dqrp_mmr_xuerr2_s;
} sh_md_dqrp_mmr_xuerr2_u_t;
#else
typedef union sh_md_dqrp_mmr_xuerr2_u {
	mmr_t	sh_md_dqrp_mmr_xuerr2_regval;
	struct {
		mmr_t	reserved_0  : 26;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	grp2        : 36;
	} sh_md_dqrp_mmr_xuerr2_s;
} sh_md_dqrp_mmr_xuerr2_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_MD_DQRP_MMR_XPERR"                    */
/*                       protocol error register                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_xperr_u {
	mmr_t	sh_md_dqrp_mmr_xperr_regval;
	struct {
		mmr_t	dir         : 26;
		mmr_t	cmd         : 8;
		mmr_t	src         : 14;
		mmr_t	prige       : 1;
		mmr_t	priv        : 1;
		mmr_t	cor         : 1;
		mmr_t	unc         : 1;
		mmr_t	mybit       : 8;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	arm         : 1;
		mmr_t	reserved_0  : 1;
	} sh_md_dqrp_mmr_xperr_s;
} sh_md_dqrp_mmr_xperr_u_t;
#else
typedef union sh_md_dqrp_mmr_xperr_u {
	mmr_t	sh_md_dqrp_mmr_xperr_regval;
	struct {
		mmr_t	reserved_0  : 1;
		mmr_t	arm         : 1;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	mybit       : 8;
		mmr_t	unc         : 1;
		mmr_t	cor         : 1;
		mmr_t	priv        : 1;
		mmr_t	prige       : 1;
		mmr_t	src         : 14;
		mmr_t	cmd         : 8;
		mmr_t	dir         : 26;
	} sh_md_dqrp_mmr_xperr_s;
} sh_md_dqrp_mmr_xperr_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_MD_DQRP_MMR_YCERR1"                   */
/*              correctable dir ecc group 1 error register              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_ycerr1_u {
	mmr_t	sh_md_dqrp_mmr_ycerr1_regval;
	struct {
		mmr_t	grp1        : 36;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	arm         : 1;
		mmr_t	reserved_0  : 25;
	} sh_md_dqrp_mmr_ycerr1_s;
} sh_md_dqrp_mmr_ycerr1_u_t;
#else
typedef union sh_md_dqrp_mmr_ycerr1_u {
	mmr_t	sh_md_dqrp_mmr_ycerr1_regval;
	struct {
		mmr_t	reserved_0  : 25;
		mmr_t	arm         : 1;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	grp1        : 36;
	} sh_md_dqrp_mmr_ycerr1_s;
} sh_md_dqrp_mmr_ycerr1_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_MD_DQRP_MMR_YCERR2"                   */
/*              correctable dir ecc group 2 error register              */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_ycerr2_u {
	mmr_t	sh_md_dqrp_mmr_ycerr2_regval;
	struct {
		mmr_t	grp2        : 36;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	reserved_0  : 26;
	} sh_md_dqrp_mmr_ycerr2_s;
} sh_md_dqrp_mmr_ycerr2_u_t;
#else
typedef union sh_md_dqrp_mmr_ycerr2_u {
	mmr_t	sh_md_dqrp_mmr_ycerr2_regval;
	struct {
		mmr_t	reserved_0  : 26;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	grp2        : 36;
	} sh_md_dqrp_mmr_ycerr2_s;
} sh_md_dqrp_mmr_ycerr2_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_MD_DQRP_MMR_YUERR1"                   */
/*             uncorrectable dir ecc group 1 error register             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_yuerr1_u {
	mmr_t	sh_md_dqrp_mmr_yuerr1_regval;
	struct {
		mmr_t	grp1        : 36;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	arm         : 1;
		mmr_t	reserved_0  : 25;
	} sh_md_dqrp_mmr_yuerr1_s;
} sh_md_dqrp_mmr_yuerr1_u_t;
#else
typedef union sh_md_dqrp_mmr_yuerr1_u {
	mmr_t	sh_md_dqrp_mmr_yuerr1_regval;
	struct {
		mmr_t	reserved_0  : 25;
		mmr_t	arm         : 1;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	grp1        : 36;
	} sh_md_dqrp_mmr_yuerr1_s;
} sh_md_dqrp_mmr_yuerr1_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_MD_DQRP_MMR_YUERR2"                   */
/*             uncorrectable dir ecc group 2 error register             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_yuerr2_u {
	mmr_t	sh_md_dqrp_mmr_yuerr2_regval;
	struct {
		mmr_t	grp2        : 36;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	reserved_0  : 26;
	} sh_md_dqrp_mmr_yuerr2_s;
} sh_md_dqrp_mmr_yuerr2_u_t;
#else
typedef union sh_md_dqrp_mmr_yuerr2_u {
	mmr_t	sh_md_dqrp_mmr_yuerr2_regval;
	struct {
		mmr_t	reserved_0  : 26;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	grp2        : 36;
	} sh_md_dqrp_mmr_yuerr2_s;
} sh_md_dqrp_mmr_yuerr2_u_t;
#endif

/* ==================================================================== */
/*                   Register "SH_MD_DQRP_MMR_YPERR"                    */
/*                       protocol error register                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_yperr_u {
	mmr_t	sh_md_dqrp_mmr_yperr_regval;
	struct {
		mmr_t	dir         : 26;
		mmr_t	cmd         : 8;
		mmr_t	src         : 14;
		mmr_t	prige       : 1;
		mmr_t	priv        : 1;
		mmr_t	cor         : 1;
		mmr_t	unc         : 1;
		mmr_t	mybit       : 8;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	arm         : 1;
		mmr_t	reserved_0  : 1;
	} sh_md_dqrp_mmr_yperr_s;
} sh_md_dqrp_mmr_yperr_u_t;
#else
typedef union sh_md_dqrp_mmr_yperr_u {
	mmr_t	sh_md_dqrp_mmr_yperr_regval;
	struct {
		mmr_t	reserved_0  : 1;
		mmr_t	arm         : 1;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	mybit       : 8;
		mmr_t	unc         : 1;
		mmr_t	cor         : 1;
		mmr_t	priv        : 1;
		mmr_t	prige       : 1;
		mmr_t	src         : 14;
		mmr_t	cmd         : 8;
		mmr_t	dir         : 26;
	} sh_md_dqrp_mmr_yperr_s;
} sh_md_dqrp_mmr_yperr_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQRP_MMR_DIR_CMDTRIG"                 */
/*                             cmd triggers                             */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_dir_cmdtrig_u {
	mmr_t	sh_md_dqrp_mmr_dir_cmdtrig_regval;
	struct {
		mmr_t	cmd0        : 8;
		mmr_t	cmd1        : 8;
		mmr_t	cmd2        : 8;
		mmr_t	cmd3        : 8;
		mmr_t	reserved_0  : 32;
	} sh_md_dqrp_mmr_dir_cmdtrig_s;
} sh_md_dqrp_mmr_dir_cmdtrig_u_t;
#else
typedef union sh_md_dqrp_mmr_dir_cmdtrig_u {
	mmr_t	sh_md_dqrp_mmr_dir_cmdtrig_regval;
	struct {
		mmr_t	reserved_0  : 32;
		mmr_t	cmd3        : 8;
		mmr_t	cmd2        : 8;
		mmr_t	cmd1        : 8;
		mmr_t	cmd0        : 8;
	} sh_md_dqrp_mmr_dir_cmdtrig_s;
} sh_md_dqrp_mmr_dir_cmdtrig_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQRP_MMR_DIR_TBLTRIG"                 */
/*                          dir table trigger                           */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_dir_tbltrig_u {
	mmr_t	sh_md_dqrp_mmr_dir_tbltrig_regval;
	struct {
		mmr_t	src         : 14;
		mmr_t	cmd         : 8;
		mmr_t	acc         : 2;
		mmr_t	prige       : 1;
		mmr_t	dirst       : 9;
		mmr_t	mybit       : 8;
		mmr_t	reserved_0  : 22;
	} sh_md_dqrp_mmr_dir_tbltrig_s;
} sh_md_dqrp_mmr_dir_tbltrig_u_t;
#else
typedef union sh_md_dqrp_mmr_dir_tbltrig_u {
	mmr_t	sh_md_dqrp_mmr_dir_tbltrig_regval;
	struct {
		mmr_t	reserved_0  : 22;
		mmr_t	mybit       : 8;
		mmr_t	dirst       : 9;
		mmr_t	prige       : 1;
		mmr_t	acc         : 2;
		mmr_t	cmd         : 8;
		mmr_t	src         : 14;
	} sh_md_dqrp_mmr_dir_tbltrig_s;
} sh_md_dqrp_mmr_dir_tbltrig_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQRP_MMR_DIR_TBLMASK"                 */
/*                        dir table trigger mask                        */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_dir_tblmask_u {
	mmr_t	sh_md_dqrp_mmr_dir_tblmask_regval;
	struct {
		mmr_t	src         : 14;
		mmr_t	cmd         : 8;
		mmr_t	acc         : 2;
		mmr_t	prige       : 1;
		mmr_t	dirst       : 9;
		mmr_t	mybit       : 8;
		mmr_t	reserved_0  : 22;
	} sh_md_dqrp_mmr_dir_tblmask_s;
} sh_md_dqrp_mmr_dir_tblmask_u_t;
#else
typedef union sh_md_dqrp_mmr_dir_tblmask_u {
	mmr_t	sh_md_dqrp_mmr_dir_tblmask_regval;
	struct {
		mmr_t	reserved_0  : 22;
		mmr_t	mybit       : 8;
		mmr_t	dirst       : 9;
		mmr_t	prige       : 1;
		mmr_t	acc         : 2;
		mmr_t	cmd         : 8;
		mmr_t	src         : 14;
	} sh_md_dqrp_mmr_dir_tblmask_s;
} sh_md_dqrp_mmr_dir_tblmask_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_MD_DQRP_MMR_XBIST_H"                   */
/*                    rising edge bist/fill pattern                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_xbist_h_u {
	mmr_t	sh_md_dqrp_mmr_xbist_h_regval;
	struct {
		mmr_t	pat         : 32;
		mmr_t	reserved_0  : 8;
		mmr_t	inv         : 1;
		mmr_t	rot         : 1;
		mmr_t	arm         : 1;
		mmr_t	reserved_1  : 21;
	} sh_md_dqrp_mmr_xbist_h_s;
} sh_md_dqrp_mmr_xbist_h_u_t;
#else
typedef union sh_md_dqrp_mmr_xbist_h_u {
	mmr_t	sh_md_dqrp_mmr_xbist_h_regval;
	struct {
		mmr_t	reserved_1  : 21;
		mmr_t	arm         : 1;
		mmr_t	rot         : 1;
		mmr_t	inv         : 1;
		mmr_t	reserved_0  : 8;
		mmr_t	pat         : 32;
	} sh_md_dqrp_mmr_xbist_h_s;
} sh_md_dqrp_mmr_xbist_h_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_MD_DQRP_MMR_XBIST_L"                   */
/*                    falling edge bist/fill pattern                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_xbist_l_u {
	mmr_t	sh_md_dqrp_mmr_xbist_l_regval;
	struct {
		mmr_t	pat         : 32;
		mmr_t	reserved_0  : 8;
		mmr_t	inv         : 1;
		mmr_t	rot         : 1;
		mmr_t	reserved_1  : 22;
	} sh_md_dqrp_mmr_xbist_l_s;
} sh_md_dqrp_mmr_xbist_l_u_t;
#else
typedef union sh_md_dqrp_mmr_xbist_l_u {
	mmr_t	sh_md_dqrp_mmr_xbist_l_regval;
	struct {
		mmr_t	reserved_1  : 22;
		mmr_t	rot         : 1;
		mmr_t	inv         : 1;
		mmr_t	reserved_0  : 8;
		mmr_t	pat         : 32;
	} sh_md_dqrp_mmr_xbist_l_s;
} sh_md_dqrp_mmr_xbist_l_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQRP_MMR_XBIST_ERR_H"                 */
/*                    rising edge bist error pattern                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_xbist_err_h_u {
	mmr_t	sh_md_dqrp_mmr_xbist_err_h_regval;
	struct {
		mmr_t	pat         : 32;
		mmr_t	reserved_0  : 8;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	reserved_1  : 22;
	} sh_md_dqrp_mmr_xbist_err_h_s;
} sh_md_dqrp_mmr_xbist_err_h_u_t;
#else
typedef union sh_md_dqrp_mmr_xbist_err_h_u {
	mmr_t	sh_md_dqrp_mmr_xbist_err_h_regval;
	struct {
		mmr_t	reserved_1  : 22;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	reserved_0  : 8;
		mmr_t	pat         : 32;
	} sh_md_dqrp_mmr_xbist_err_h_s;
} sh_md_dqrp_mmr_xbist_err_h_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQRP_MMR_XBIST_ERR_L"                 */
/*                   falling edge bist error pattern                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_xbist_err_l_u {
	mmr_t	sh_md_dqrp_mmr_xbist_err_l_regval;
	struct {
		mmr_t	pat         : 32;
		mmr_t	reserved_0  : 8;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	reserved_1  : 22;
	} sh_md_dqrp_mmr_xbist_err_l_s;
} sh_md_dqrp_mmr_xbist_err_l_u_t;
#else
typedef union sh_md_dqrp_mmr_xbist_err_l_u {
	mmr_t	sh_md_dqrp_mmr_xbist_err_l_regval;
	struct {
		mmr_t	reserved_1  : 22;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	reserved_0  : 8;
		mmr_t	pat         : 32;
	} sh_md_dqrp_mmr_xbist_err_l_s;
} sh_md_dqrp_mmr_xbist_err_l_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_MD_DQRP_MMR_YBIST_H"                   */
/*                    rising edge bist/fill pattern                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_ybist_h_u {
	mmr_t	sh_md_dqrp_mmr_ybist_h_regval;
	struct {
		mmr_t	pat         : 32;
		mmr_t	reserved_0  : 8;
		mmr_t	inv         : 1;
		mmr_t	rot         : 1;
		mmr_t	arm         : 1;
		mmr_t	reserved_1  : 21;
	} sh_md_dqrp_mmr_ybist_h_s;
} sh_md_dqrp_mmr_ybist_h_u_t;
#else
typedef union sh_md_dqrp_mmr_ybist_h_u {
	mmr_t	sh_md_dqrp_mmr_ybist_h_regval;
	struct {
		mmr_t	reserved_1  : 21;
		mmr_t	arm         : 1;
		mmr_t	rot         : 1;
		mmr_t	inv         : 1;
		mmr_t	reserved_0  : 8;
		mmr_t	pat         : 32;
	} sh_md_dqrp_mmr_ybist_h_s;
} sh_md_dqrp_mmr_ybist_h_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_MD_DQRP_MMR_YBIST_L"                   */
/*                    falling edge bist/fill pattern                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_ybist_l_u {
	mmr_t	sh_md_dqrp_mmr_ybist_l_regval;
	struct {
		mmr_t	pat         : 32;
		mmr_t	reserved_0  : 8;
		mmr_t	inv         : 1;
		mmr_t	rot         : 1;
		mmr_t	reserved_1  : 22;
	} sh_md_dqrp_mmr_ybist_l_s;
} sh_md_dqrp_mmr_ybist_l_u_t;
#else
typedef union sh_md_dqrp_mmr_ybist_l_u {
	mmr_t	sh_md_dqrp_mmr_ybist_l_regval;
	struct {
		mmr_t	reserved_1  : 22;
		mmr_t	rot         : 1;
		mmr_t	inv         : 1;
		mmr_t	reserved_0  : 8;
		mmr_t	pat         : 32;
	} sh_md_dqrp_mmr_ybist_l_s;
} sh_md_dqrp_mmr_ybist_l_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQRP_MMR_YBIST_ERR_H"                 */
/*                    rising edge bist error pattern                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_ybist_err_h_u {
	mmr_t	sh_md_dqrp_mmr_ybist_err_h_regval;
	struct {
		mmr_t	pat         : 32;
		mmr_t	reserved_0  : 8;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	reserved_1  : 22;
	} sh_md_dqrp_mmr_ybist_err_h_s;
} sh_md_dqrp_mmr_ybist_err_h_u_t;
#else
typedef union sh_md_dqrp_mmr_ybist_err_h_u {
	mmr_t	sh_md_dqrp_mmr_ybist_err_h_regval;
	struct {
		mmr_t	reserved_1  : 22;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	reserved_0  : 8;
		mmr_t	pat         : 32;
	} sh_md_dqrp_mmr_ybist_err_h_s;
} sh_md_dqrp_mmr_ybist_err_h_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQRP_MMR_YBIST_ERR_L"                 */
/*                   falling edge bist error pattern                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrp_mmr_ybist_err_l_u {
	mmr_t	sh_md_dqrp_mmr_ybist_err_l_regval;
	struct {
		mmr_t	pat         : 32;
		mmr_t	reserved_0  : 8;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	reserved_1  : 22;
	} sh_md_dqrp_mmr_ybist_err_l_s;
} sh_md_dqrp_mmr_ybist_err_l_u_t;
#else
typedef union sh_md_dqrp_mmr_ybist_err_l_u {
	mmr_t	sh_md_dqrp_mmr_ybist_err_l_regval;
	struct {
		mmr_t	reserved_1  : 22;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	reserved_0  : 8;
		mmr_t	pat         : 32;
	} sh_md_dqrp_mmr_ybist_err_l_s;
} sh_md_dqrp_mmr_ybist_err_l_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_MD_DQRS_MMR_XBIST_H"                   */
/*                    rising edge bist/fill pattern                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrs_mmr_xbist_h_u {
	mmr_t	sh_md_dqrs_mmr_xbist_h_regval;
	struct {
		mmr_t	pat         : 40;
		mmr_t	inv         : 1;
		mmr_t	rot         : 1;
		mmr_t	arm         : 1;
		mmr_t	reserved_0  : 21;
	} sh_md_dqrs_mmr_xbist_h_s;
} sh_md_dqrs_mmr_xbist_h_u_t;
#else
typedef union sh_md_dqrs_mmr_xbist_h_u {
	mmr_t	sh_md_dqrs_mmr_xbist_h_regval;
	struct {
		mmr_t	reserved_0  : 21;
		mmr_t	arm         : 1;
		mmr_t	rot         : 1;
		mmr_t	inv         : 1;
		mmr_t	pat         : 40;
	} sh_md_dqrs_mmr_xbist_h_s;
} sh_md_dqrs_mmr_xbist_h_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_MD_DQRS_MMR_XBIST_L"                   */
/*                    falling edge bist/fill pattern                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrs_mmr_xbist_l_u {
	mmr_t	sh_md_dqrs_mmr_xbist_l_regval;
	struct {
		mmr_t	pat         : 40;
		mmr_t	inv         : 1;
		mmr_t	rot         : 1;
		mmr_t	reserved_0  : 22;
	} sh_md_dqrs_mmr_xbist_l_s;
} sh_md_dqrs_mmr_xbist_l_u_t;
#else
typedef union sh_md_dqrs_mmr_xbist_l_u {
	mmr_t	sh_md_dqrs_mmr_xbist_l_regval;
	struct {
		mmr_t	reserved_0  : 22;
		mmr_t	rot         : 1;
		mmr_t	inv         : 1;
		mmr_t	pat         : 40;
	} sh_md_dqrs_mmr_xbist_l_s;
} sh_md_dqrs_mmr_xbist_l_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQRS_MMR_XBIST_ERR_H"                 */
/*                    rising edge bist error pattern                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrs_mmr_xbist_err_h_u {
	mmr_t	sh_md_dqrs_mmr_xbist_err_h_regval;
	struct {
		mmr_t	pat         : 40;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	reserved_0  : 22;
	} sh_md_dqrs_mmr_xbist_err_h_s;
} sh_md_dqrs_mmr_xbist_err_h_u_t;
#else
typedef union sh_md_dqrs_mmr_xbist_err_h_u {
	mmr_t	sh_md_dqrs_mmr_xbist_err_h_regval;
	struct {
		mmr_t	reserved_0  : 22;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	pat         : 40;
	} sh_md_dqrs_mmr_xbist_err_h_s;
} sh_md_dqrs_mmr_xbist_err_h_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQRS_MMR_XBIST_ERR_L"                 */
/*                   falling edge bist error pattern                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrs_mmr_xbist_err_l_u {
	mmr_t	sh_md_dqrs_mmr_xbist_err_l_regval;
	struct {
		mmr_t	pat         : 40;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	reserved_0  : 22;
	} sh_md_dqrs_mmr_xbist_err_l_s;
} sh_md_dqrs_mmr_xbist_err_l_u_t;
#else
typedef union sh_md_dqrs_mmr_xbist_err_l_u {
	mmr_t	sh_md_dqrs_mmr_xbist_err_l_regval;
	struct {
		mmr_t	reserved_0  : 22;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	pat         : 40;
	} sh_md_dqrs_mmr_xbist_err_l_s;
} sh_md_dqrs_mmr_xbist_err_l_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_MD_DQRS_MMR_YBIST_H"                   */
/*                    rising edge bist/fill pattern                     */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrs_mmr_ybist_h_u {
	mmr_t	sh_md_dqrs_mmr_ybist_h_regval;
	struct {
		mmr_t	pat         : 40;
		mmr_t	inv         : 1;
		mmr_t	rot         : 1;
		mmr_t	arm         : 1;
		mmr_t	reserved_0  : 21;
	} sh_md_dqrs_mmr_ybist_h_s;
} sh_md_dqrs_mmr_ybist_h_u_t;
#else
typedef union sh_md_dqrs_mmr_ybist_h_u {
	mmr_t	sh_md_dqrs_mmr_ybist_h_regval;
	struct {
		mmr_t	reserved_0  : 21;
		mmr_t	arm         : 1;
		mmr_t	rot         : 1;
		mmr_t	inv         : 1;
		mmr_t	pat         : 40;
	} sh_md_dqrs_mmr_ybist_h_s;
} sh_md_dqrs_mmr_ybist_h_u_t;
#endif

/* ==================================================================== */
/*                  Register "SH_MD_DQRS_MMR_YBIST_L"                   */
/*                    falling edge bist/fill pattern                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrs_mmr_ybist_l_u {
	mmr_t	sh_md_dqrs_mmr_ybist_l_regval;
	struct {
		mmr_t	pat         : 40;
		mmr_t	inv         : 1;
		mmr_t	rot         : 1;
		mmr_t	reserved_0  : 22;
	} sh_md_dqrs_mmr_ybist_l_s;
} sh_md_dqrs_mmr_ybist_l_u_t;
#else
typedef union sh_md_dqrs_mmr_ybist_l_u {
	mmr_t	sh_md_dqrs_mmr_ybist_l_regval;
	struct {
		mmr_t	reserved_0  : 22;
		mmr_t	rot         : 1;
		mmr_t	inv         : 1;
		mmr_t	pat         : 40;
	} sh_md_dqrs_mmr_ybist_l_s;
} sh_md_dqrs_mmr_ybist_l_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQRS_MMR_YBIST_ERR_H"                 */
/*                    rising edge bist error pattern                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrs_mmr_ybist_err_h_u {
	mmr_t	sh_md_dqrs_mmr_ybist_err_h_regval;
	struct {
		mmr_t	pat         : 40;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	reserved_0  : 22;
	} sh_md_dqrs_mmr_ybist_err_h_s;
} sh_md_dqrs_mmr_ybist_err_h_u_t;
#else
typedef union sh_md_dqrs_mmr_ybist_err_h_u {
	mmr_t	sh_md_dqrs_mmr_ybist_err_h_regval;
	struct {
		mmr_t	reserved_0  : 22;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	pat         : 40;
	} sh_md_dqrs_mmr_ybist_err_h_s;
} sh_md_dqrs_mmr_ybist_err_h_u_t;
#endif

/* ==================================================================== */
/*                Register "SH_MD_DQRS_MMR_YBIST_ERR_L"                 */
/*                   falling edge bist error pattern                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrs_mmr_ybist_err_l_u {
	mmr_t	sh_md_dqrs_mmr_ybist_err_l_regval;
	struct {
		mmr_t	pat         : 40;
		mmr_t	val         : 1;
		mmr_t	more        : 1;
		mmr_t	reserved_0  : 22;
	} sh_md_dqrs_mmr_ybist_err_l_s;
} sh_md_dqrs_mmr_ybist_err_l_u_t;
#else
typedef union sh_md_dqrs_mmr_ybist_err_l_u {
	mmr_t	sh_md_dqrs_mmr_ybist_err_l_regval;
	struct {
		mmr_t	reserved_0  : 22;
		mmr_t	more        : 1;
		mmr_t	val         : 1;
		mmr_t	pat         : 40;
	} sh_md_dqrs_mmr_ybist_err_l_s;
} sh_md_dqrs_mmr_ybist_err_l_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_MD_DQRS_MMR_JNR_DEBUG"                  */
/*                    joiner/fct debug configuration                    */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrs_mmr_jnr_debug_u {
	mmr_t	sh_md_dqrs_mmr_jnr_debug_regval;
	struct {
		mmr_t	px          : 1;
		mmr_t	rw          : 1;
		mmr_t	reserved_0  : 62;
	} sh_md_dqrs_mmr_jnr_debug_s;
} sh_md_dqrs_mmr_jnr_debug_u_t;
#else
typedef union sh_md_dqrs_mmr_jnr_debug_u {
	mmr_t	sh_md_dqrs_mmr_jnr_debug_regval;
	struct {
		mmr_t	reserved_0  : 62;
		mmr_t	rw          : 1;
		mmr_t	px          : 1;
	} sh_md_dqrs_mmr_jnr_debug_s;
} sh_md_dqrs_mmr_jnr_debug_u_t;
#endif

/* ==================================================================== */
/*                 Register "SH_MD_DQRS_MMR_YAMOPW_ERR"                 */
/*                  amo/partial rmw ecc error register                  */
/* ==================================================================== */

#ifdef LITTLE_ENDIAN
typedef union sh_md_dqrs_mmr_yamopw_err_u {
	mmr_t	sh_md_dqrs_mmr_yamopw_err_regval;
	struct {
		mmr_t	ssyn        : 8;
		mmr_t	scor        : 1;
		mmr_t	sunc        : 1;
		mmr_t	reserved_0  : 6;
		mmr_t	rsyn        : 8;
		mmr_t	rcor        : 1;
		mmr_t	runc        : 1;
		mmr_t	reserved_1  : 6;
		mmr_t	arm         : 1;
		mmr_t	reserved_2  : 31;
	} sh_md_dqrs_mmr_yamopw_err_s;
} sh_md_dqrs_mmr_yamopw_err_u_t;
#else
typedef union sh_md_dqrs_mmr_yamopw_err_u {
	mmr_t	sh_md_dqrs_mmr_yamopw_err_regval;
	struct {
		mmr_t	reserved_2  : 31;
		mmr_t	arm         : 1;
		mmr_t	reserved_1  : 6;
		mmr_t	runc        : 1;
		mmr_t	rcor        : 1;
		mmr_t	rsyn        : 8;
		mmr_t	reserved_0  : 6;
		mmr_t	sunc        : 1;
		mmr_t	scor        : 1;
		mmr_t	ssyn        : 8;
	} sh_md_dqrs_mmr_yamopw_err_s;
} sh_md_dqrs_mmr_yamopw_err_u_t;
#endif


#endif /* _ASM_IA64_SN_SN2_SHUB_MMR_T_H */

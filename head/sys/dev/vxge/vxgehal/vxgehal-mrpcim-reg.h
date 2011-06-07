/*-
 * Copyright(c) 2002-2011 Exar Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification are permitted provided the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *    3. Neither the name of the Exar Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*$FreeBSD$*/

#ifndef	VXGE_HAL_MRPCIM_REGS_H
#define	VXGE_HAL_MRPCIM_REGS_H

__EXTERN_BEGIN_DECLS

typedef struct vxge_hal_mrpcim_reg_t {

/* 0x00000 */	u64	g3fbct_int_status;
#define	VXGE_HAL_G3FBCT_INT_STATUS_ERR_G3IF_INT		    mBIT(0)
/* 0x00008 */	u64	g3fbct_int_mask;
/* 0x00010 */	u64	g3fbct_err_reg;
#define	VXGE_HAL_G3FBCT_ERR_REG_G3IF_SM_ERR		    mBIT(4)
#define	VXGE_HAL_G3FBCT_ERR_REG_G3IF_GDDR3_DECC		    mBIT(5)
#define	VXGE_HAL_G3FBCT_ERR_REG_G3IF_GDDR3_U_DECC	    mBIT(6)
#define	VXGE_HAL_G3FBCT_ERR_REG_G3IF_CTRL_FIFO_DECC	    mBIT(7)
#define	VXGE_HAL_G3FBCT_ERR_REG_G3IF_GDDR3_SECC		    mBIT(29)
#define	VXGE_HAL_G3FBCT_ERR_REG_G3IF_GDDR3_U_SECC	    mBIT(30)
#define	VXGE_HAL_G3FBCT_ERR_REG_G3IF_CTRL_FIFO_SECC	    mBIT(31)
/* 0x00018 */	u64	g3fbct_err_mask;
/* 0x00020 */	u64	g3fbct_err_alarm;
/* 0x00028 */	u64	g3fbct_config0;
#define	VXGE_HAL_G3FBCT_CONFIG0_RD_CMD_LATENCY_RPATH(val)   vBIT(val, 5, 3)
#define	VXGE_HAL_G3FBCT_CONFIG0_RD_CMD_LATENCY(val)	    vBIT(val, 13, 3)
#define	VXGE_HAL_G3FBCT_CONFIG0_REFRESH_PER(val)	    vBIT(val, 16, 16)
#define	VXGE_HAL_G3FBCT_CONFIG0_TRC(val)		    vBIT(val, 35, 5)
#define	VXGE_HAL_G3FBCT_CONFIG0_TRRD(val)		    vBIT(val, 44, 4)
#define	VXGE_HAL_G3FBCT_CONFIG0_TFAW(val)		    vBIT(val, 50, 6)
#define	VXGE_HAL_G3FBCT_CONFIG0_RD_FIFO_THR(val)	    vBIT(val, 58, 6)
/* 0x00030 */	u64	g3fbct_config1;
#define	VXGE_HAL_G3FBCT_CONFIG1_BIC_THR(val)		    vBIT(val, 3, 5)
#define	VXGE_HAL_G3FBCT_CONFIG1_BIC_OFF			    mBIT(15)
#define	VXGE_HAL_G3FBCT_CONFIG1_IGNORE_BEM		    mBIT(23)
#define	VXGE_HAL_G3FBCT_CONFIG1_RD_SAMPLING(val)	    vBIT(val, 29, 3)
#define	VXGE_HAL_G3FBCT_CONFIG1_CMD_START_PHASE		    mBIT(39)
#define	VXGE_HAL_G3FBCT_CONFIG1_BIC_HI_THR(val)		    vBIT(val, 43, 5)
#define	VXGE_HAL_G3FBCT_CONFIG1_BIC_MODE(val)		    vBIT(val, 54, 2)
#define	VXGE_HAL_G3FBCT_CONFIG1_ECC_ENABLE(val)		    vBIT(val, 57, 7)
/* 0x00038 */	u64	g3fbct_config2;
#define	VXGE_HAL_G3FBCT_CONFIG2_DEV_USE_ENABLE(val)	    vBIT(val, 6, 2)
#define	VXGE_HAL_G3FBCT_CONFIG2_DEV_USE_VALUE(val)	    vBIT(val, 9, 7)
#define	VXGE_HAL_G3FBCT_CONFIG2_ARBITER_CTRL(val)	    vBIT(val, 22, 2)
#define	VXGE_HAL_G3FBCT_CONFIG2_DEFINE_CAD		    mBIT(31)
#define	VXGE_HAL_G3FBCT_CONFIG2_DEFINE_NOP_AD		    mBIT(39)
#define	VXGE_HAL_G3FBCT_CONFIG2_LAST_CADD(val)		    vBIT(val, 43, 13)
/* 0x00040 */	u64	g3fbct_init0;
#define	VXGE_HAL_G3FBCT_INIT0_MRS_BAD(val)		    vBIT(val, 5, 3)
#define	VXGE_HAL_G3FBCT_INIT0_MRS_WL(val)		    vBIT(val, 13, 3)
#define	VXGE_HAL_G3FBCT_INIT0_MRS_DLL			    mBIT(23)
#define	VXGE_HAL_G3FBCT_INIT0_MRS_TM			    mBIT(39)
#define	VXGE_HAL_G3FBCT_INIT0_MRS_CL(val)		    vBIT(val, 44, 4)
#define	VXGE_HAL_G3FBCT_INIT0_MRS_BT			    mBIT(55)
#define	VXGE_HAL_G3FBCT_INIT0_MRS_BL(val)		    vBIT(val, 62, 2)
/* 0x00048 */	u64	g3fbct_init1;
#define	VXGE_HAL_G3FBCT_INIT1_EMRS_BAD(val)		    vBIT(val, 5, 3)
#define	VXGE_HAL_G3FBCT_INIT1_EMRS_AD_TER		    mBIT(15)
#define	VXGE_HAL_G3FBCT_INIT1_EMRS_ID			    mBIT(23)
#define	VXGE_HAL_G3FBCT_INIT1_EMRS_RON			    mBIT(39)
#define	VXGE_HAL_G3FBCT_INIT1_EMRS_AL			    mBIT(47)
#define	VXGE_HAL_G3FBCT_INIT1_EMRS_TWR(val)		    vBIT(val, 53, 3)
#define	VXGE_HAL_G3FBCT_INIT1_EMRS_DQ_TER(val)		    vBIT(val, 62, 2)
/* 0x00050 */	u64	g3fbct_init2;
#define	VXGE_HAL_G3FBCT_INIT2_EMRS_DR_STR(val)		    vBIT(val, 6, 2)
#define	VXGE_HAL_G3FBCT_INIT2_START_INI			    mBIT(15)
#define	VXGE_HAL_G3FBCT_INIT2_POWER_UP_DELAY(val)	    vBIT(val, 16, 24)
#define	VXGE_HAL_G3FBCT_INIT2_ACTIVE_CMD_DELAY(val)	    vBIT(val, 40, 24)
/* 0x00058 */	u64	g3fbct_init3;
#define	VXGE_HAL_G3FBCT_INIT3_TRP_DELAY(val)		    vBIT(val, 0, 8)
#define	VXGE_HAL_G3FBCT_INIT3_TMRD_DELAY(val)		    vBIT(val, 8, 8)
#define	VXGE_HAL_G3FBCT_INIT3_TWR2PRE_DELAY(val)	    vBIT(val, 16, 8)
#define	VXGE_HAL_G3FBCT_INIT3_TRD2PRE_DELAY(val)	    vBIT(val, 24, 8)
#define	VXGE_HAL_G3FBCT_INIT3_TRCDR_DELAY(val)		    vBIT(val, 32, 8)
#define	VXGE_HAL_G3FBCT_INIT3_TRCDW_DELAY(val)		    vBIT(val, 40, 8)
#define	VXGE_HAL_G3FBCT_INIT3_TWR2RD_DELAY(val)		    vBIT(val, 48, 8)
#define	VXGE_HAL_G3FBCT_INIT3_TRD2WR_DELAY(val)		    vBIT(val, 56, 8)
/* 0x00060 */ u64 g3fbct_init4;
#define	VXGE_HAL_G3FBCT_INIT4_TRFC_DELAY(val)		    vBIT(val, 0, 8)
#define	VXGE_HAL_G3FBCT_INIT4_REFRESH_BURSTS(val)	    vBIT(val, 12, 4)
#define	VXGE_HAL_G3FBCT_INIT4_CKE_INIT_VAL		    mBIT(31)
#define	VXGE_HAL_G3FBCT_INIT4_VENDOR_ID(val)		    vBIT(val, 32, 8)
#define	VXGE_HAL_G3FBCT_INIT4_OOO_DEPTH(val)		    vBIT(val, 42, 6)
#define	VXGE_HAL_G3FBCT_INIT4_ICTRL_INIT_DONE		    mBIT(55)
#define	VXGE_HAL_G3FBCT_INIT4_IOCAL_WAIT_DISABLE	    mBIT(63)
/* 0x00068 */ u64 g3fbct_init5;
#define	VXGE_HAL_G3FBCT_INIT5_TRAS_DELAY(val)		    vBIT(val, 3, 5)
#define	VXGE_HAL_G3FBCT_INIT5_TVID_DELAY(val)		    vBIT(val, 8, 8)
#define	VXGE_HAL_G3FBCT_INIT5_TWR_APRE2CMD(val)		    vBIT(val, 16, 8)
#define	VXGE_HAL_G3FBCT_INIT5_TRD_APRE2CMD(val)		    vBIT(val, 24, 8)
#define	VXGE_HAL_G3FBCT_INIT5_TWR_APRE2CMD_CON(val)	    vBIT(val, 32, 8)
#define	VXGE_HAL_G3FBCT_INIT5_GDDR3_DLL_DELAY(val)	    vBIT(val, 40, 24)
/* 0x00070 */	u64	g3fbct_dll_training1;
#define	VXGE_HAL_G3FBCT_DLL_TRAINING1_DLL_TRA_DATA00(val)   vBIT(val, 0, 64)
/* 0x00078 */	u64	g3fbct_dll_training2;
#define	VXGE_HAL_G3FBCT_DLL_TRAINING2_DLL_TRA_DATA01(val)   vBIT(val, 0, 64)
/* 0x00080 */	u64	g3fbct_dll_training3;
#define	VXGE_HAL_G3FBCT_DLL_TRAINING3_DLL_TRA_DATA10(val)   vBIT(val, 0, 64)
/* 0x00088 */	u64	g3fbct_dll_training4;
#define	VXGE_HAL_G3FBCT_DLL_TRAINING4_DLL_TRA_DATA11(val)   vBIT(val, 0, 64)
/* 0x00090 */	u64	g3fbct_dll_training6;
#define	VXGE_HAL_G3FBCT_DLL_TRAINING6_DLL_TRA_DATA20(val)   vBIT(val, 0, 64)
/* 0x00098 */	u64	g3fbct_dll_training7;
#define	VXGE_HAL_G3FBCT_DLL_TRAINING7_DLL_TRA_DATA21(val)   vBIT(val, 0, 64)
/* 0x000a0 */	u64	g3fbct_dll_training8;
#define	VXGE_HAL_G3FBCT_DLL_TRAINING8_DLL_TRA_DATA30(val)   vBIT(val, 0, 64)
/* 0x000a8 */	u64	g3fbct_dll_training9;
#define	VXGE_HAL_G3FBCT_DLL_TRAINING9_DLL_TRA_DATA31(val)   vBIT(val, 0, 64)
/* 0x000b0 */	u64	g3fbct_dll_training5;
#define	VXGE_HAL_G3FBCT_DLL_TRAINING5_DLL_TRA_RADD(val)	    vBIT(val, 2, 14)
#define	VXGE_HAL_G3FBCT_DLL_TRAINING5_DLL_TRA_CADD0(val)    vBIT(val, 21, 11)
#define	VXGE_HAL_G3FBCT_DLL_TRAINING5_DLL_TRA_CADD1(val)    vBIT(val, 37, 11)
/* 0x000b8 */	u64	g3fbct_dll_training10;
#define	VXGE_HAL_G3FBCT_DLL_TRAINING10_DLL_TP_READS(val)    vBIT(val, 4, 4)
#define	VXGE_HAL_G3FBCT_DLL_TRAINING10_DLL_SAMPLES(val)	    vBIT(val, 8, 8)
#define	VXGE_HAL_G3FBCT_DLL_TRAINING10_TRA_LOOPS(val)	    vBIT(val, 18, 14)
#define	VXGE_HAL_G3FBCT_DLL_TRAINING10_TRA_PASS_CNT(val)    vBIT(val, 33, 7)
#define	VXGE_HAL_G3FBCT_DLL_TRAINING10_TRA_STEP(val)	    vBIT(val, 41, 7)
/* 0x000c0 */	u64	g3fbct_dll_training11;
#define	VXGE_HAL_G3FBCT_DLL_TRAINING11_ICTRL_DLL_TRA_CNT(val) vBIT(val, 0, 48)
#define	VXGE_HAL_G3FBCT_DLL_TRAINING11_ICTRL_DLL_TRA_DIS(val) vBIT(val, 54, 2)
/* 0x000c8 */	u64	g3fbct_init6;
#define	VXGE_HAL_G3FBCT_INIT6_TWR_APRE2RD_DELAY(val)	    vBIT(val, 4, 4)
#define	VXGE_HAL_G3FBCT_INIT6_TWR_APRE2WR_DELAY(val)	    vBIT(val, 12, 4)
#define	VXGE_HAL_G3FBCT_INIT6_TWR_APRE2PRE_DELAY(val)	    vBIT(val, 20, 4)
#define	VXGE_HAL_G3FBCT_INIT6_TWR_APRE2ACT_DELAY(val)	    vBIT(val, 28, 4)
#define	VXGE_HAL_G3FBCT_INIT6_TRD_APRE2RD_DELAY(val)	    vBIT(val, 36, 4)
#define	VXGE_HAL_G3FBCT_INIT6_TRD_APRE2WR_DELAY(val)	    vBIT(val, 44, 4)
#define	VXGE_HAL_G3FBCT_INIT6_TRD_APRE2PRE_DELAY(val)	    vBIT(val, 52, 4)
#define	VXGE_HAL_G3FBCT_INIT6_TRD_APRE2ACT_DELAY(val)	    vBIT(val, 60, 4)
/* 0x000d0 */	u64	g3fbct_test0;
#define	VXGE_HAL_G3FBCT_TEST0_TEST_START_RADD(val)	    vBIT(val, 2, 14)
#define	VXGE_HAL_G3FBCT_TEST0_TEST_END_RADD(val)	    vBIT(val, 18, 14)
#define	VXGE_HAL_G3FBCT_TEST0_TEST_START_CADD(val)	    vBIT(val, 37, 11)
#define	VXGE_HAL_G3FBCT_TEST0_TEST_END_CADD(val)	    vBIT(val, 53, 11)
/* 0x000d8 */	u64	g3fbct_test01;
#define	VXGE_HAL_G3FBCT_TEST01_TEST_BANK(val)		    vBIT(val, 0, 8)
#define	VXGE_HAL_G3FBCT_TEST01_TEST_CTRL(val)		    vBIT(val, 12, 4)
#define	VXGE_HAL_G3FBCT_TEST01_TEST_MODE		    mBIT(23)
#define	VXGE_HAL_G3FBCT_TEST01_TEST_GO			    mBIT(31)
#define	VXGE_HAL_G3FBCT_TEST01_TEST_DONE		    mBIT(39)
#define	VXGE_HAL_G3FBCT_TEST01_ECC_DEC_TEST_FAIL_CNTR(val)  vBIT(val, 40, 16)
#define	VXGE_HAL_G3FBCT_TEST01_TEST_DATA_ADDR		    mBIT(63)
/* 0x000e0 */	u64	g3fbct_test1;
#define	VXGE_HAL_G3FBCT_TEST1_TX_TEST_DATA(val)		    vBIT(val, 0, 64)
/* 0x000e8 */	u64	g3fbct_test2;
#define	VXGE_HAL_G3FBCT_TEST2_TX_TEST_DATA(val)		    vBIT(val, 0, 64)
/* 0x000f0 */	u64	g3fbct_test11;
#define	VXGE_HAL_G3FBCT_TEST11_TX_TEST_DATA1(val)	    vBIT(val, 0, 64)
/* 0x000f8 */	u64	g3fbct_test21;
#define	VXGE_HAL_G3FBCT_TEST21_TX_TEST_DATA1(val)	    vBIT(val, 0, 64)
/* 0x00100 */	u64	g3fbct_test3;
#define	VXGE_HAL_G3FBCT_TEST3_ECC_DEC_RX_TEST_DATA(val)	    vBIT(val, 0, 64)
/* 0x00108 */	u64	g3fbct_test4;
#define	VXGE_HAL_G3FBCT_TEST4_ECC_DEC_RX_TEST_DATA(val)	    vBIT(val, 0, 64)
/* 0x00110 */	u64	g3fbct_test31;
#define	VXGE_HAL_G3FBCT_TEST31_ECC_DEC_RX_TEST_DATA1(val)   vBIT(val, 0, 64)
/* 0x00118 */	u64	g3fbct_test41;
#define	VXGE_HAL_G3FBCT_TEST41_ECC_DEC_RX_TEST_DATA1(val)   vBIT(val, 0, 64)
/* 0x00120 */	u64	g3fbct_test5;
#define	VXGE_HAL_G3FBCT_TEST5_ECC_DEC_RX_FAILED_TEST_DATA(val) vBIT(val, 0, 64)
/* 0x00128 */	u64	g3fbct_test6;
#define	VXGE_HAL_G3FBCT_TEST6_ECC_DEC_RX_FAILED_TEST_DATA(val) vBIT(val, 0, 64)
/* 0x00130 */	u64	g3fbct_test51;
#define	VXGE_HAL_G3FBCT_TEST51_ECC_DEC_RX_FAILED_TEST_DATA1(val)\
							    vBIT(val, 0, 64)
/* 0x00138 */	u64	g3fbct_test61;
#define	VXGE_HAL_G3FBCT_TEST61_ECC_DEC_RX_FAILED_TEST_DATA1(val)\
							    vBIT(val, 0, 64)
/* 0x00140 */	u64	g3fbct_test7;
#define	VXGE_HAL_G3FBCT_TEST7_ECC_DEC_TEST_FAILED_RADD(val) vBIT(val, 0, 14)
#define	VXGE_HAL_G3FBCT_TEST7_ECC_DEC_TEST_FAILED_CADD(val) vBIT(val, 19, 11)
#define	VXGE_HAL_G3FBCT_TEST7_ECC_DEC_TEST_FAILED_BANK(val) vBIT(val, 32, 8)
/* 0x00148 */	u64	g3fbct_test71;
#define	VXGE_HAL_G3FBCT_TEST71_ECC_DEC_TEST_FAILED_RADD1(val) vBIT(val, 0, 14)
#define	VXGE_HAL_G3FBCT_TEST71_ECC_DEC_TEST_FAILED_CADD1(val) vBIT(val, 19, 11)
#define	VXGE_HAL_G3FBCT_TEST71_ECC_DEC_TEST_FAILED_BANK1(val) vBIT(val, 32, 8)
	u8	unused001b0[0x001b0 - 0x00150];

/* 0x001b0 */	u64	g3fbct_loop_back;
#define	VXGE_HAL_G3FBCT_LOOP_BACK_TDATA(val)		    vBIT(val, 0, 32)
#define	VXGE_HAL_G3FBCT_LOOP_BACK_MODE			    mBIT(39)
#define	VXGE_HAL_G3FBCT_LOOP_BACK_GO			    mBIT(47)
#define	VXGE_HAL_G3FBCT_LOOP_BACK_DONE			    mBIT(55)
#define	VXGE_HAL_G3FBCT_LOOP_BACK_RDLL_IDLE_VAL(val)	    vBIT(val, 56, 8)
/* 0x001b8 */	u64	g3fbct_loop_back1;
#define	VXGE_HAL_G3FBCT_LOOP_BACK1_RDLL_START_VAL(val)	    vBIT(val, 1, 7)
#define	VXGE_HAL_G3FBCT_LOOP_BACK1_RDLL_END_VAL(val)	    vBIT(val, 9, 7)
#define	VXGE_HAL_G3FBCT_LOOP_BACK1_WDLL_IDLE_VAL(val)	    vBIT(val, 16, 8)
#define	VXGE_HAL_G3FBCT_LOOP_BACK1_WDLL_START_VAL(val)	    vBIT(val, 25, 7)
#define	VXGE_HAL_G3FBCT_LOOP_BACK1_WDLL_END_VAL(val)	    vBIT(val, 33, 7)
#define	VXGE_HAL_G3FBCT_LOOP_BACK1_STEPS(val)		    vBIT(val, 45, 3)
#define	VXGE_HAL_G3FBCT_LOOP_BACK1_RDLL_MIN_FILTER(val)	    vBIT(val, 49, 7)
#define	VXGE_HAL_G3FBCT_LOOP_BACK1_RDLL_MAX_FILTER(val)	    vBIT(val, 57, 7)
/* 0x001c0 */	u64	g3fbct_loop_back2;
#define	VXGE_HAL_G3FBCT_LOOP_BACK2_WDLL_MIN_FILTER(val)	    vBIT(val, 1, 7)
#define	VXGE_HAL_G3FBCT_LOOP_BACK2_WDLL_MAX_FILTER(val)	    vBIT(val, 9, 7)
/* 0x001c8 */	u64	g3fbct_loop_back3;
#define	VXGE_HAL_G3FBCT_LOOP_BACK3_LBCTRL_CM_RDLL_RESULT(val) vBIT(val, 0, 8)
#define	VXGE_HAL_G3FBCT_LOOP_BACK3_LBCTRL_CM_WDLL_RESULT(val) vBIT(val, 8, 8)
#define	VXGE_HAL_G3FBCT_LOOP_BACK3_LBCTRL_CM_RDLL_MON_RESULT(val)\
							    vBIT(val, 16, 8)
/* 0x001d0 */	u64	g3fbct_loop_back4;
#define	VXGE_HAL_G3FBCT_LOOP_BACK4_LBCTRL_IO_PASS_FAILN(val) vBIT(val, 0, 32)
/* 0x001d8 */	u64	g3fbct_loop_back5;
#define	VXGE_HAL_G3FBCT_LOOP_BACK5_RDLL_START_IO_VAL(val)   vBIT(val, 1, 7)
#define	VXGE_HAL_G3FBCT_LOOP_BACK5_RDLL_END_IO_VAL(val)	    vBIT(val, 9, 7)
	u8	unused00200[0x00200 - 0x001e0];

/* 0x00200 */	u64	g3fbct_loop_back_rdll[4];
#define	VXGE_HAL_G3FBCT_LOOP_BACK_RDLL_LBCTRL_MIN_VAL(val)  vBIT(val, 1, 7)
#define	VXGE_HAL_G3FBCT_LOOP_BACK_RDLL_LBCTRL_MAX_VAL(val)  vBIT(val, 9, 7)
#define	VXGE_HAL_G3FBCT_LOOP_BACK_RDLL_LBCTRL_MON_MIN_VAL(val) vBIT(val, 17, 7)
#define	VXGE_HAL_G3FBCT_LOOP_BACK_RDLL_LBCTRL_MON_MAX_VAL(val) vBIT(val, 25, 7)
/* 0x00220 */	u64	g3fbct_loop_back_wdll[4];
#define	VXGE_HAL_G3FBCT_LOOP_BACK_WDLL_LBCTRL_MIN_VAL(val)  vBIT(val, 1, 7)
#define	VXGE_HAL_G3FBCT_LOOP_BACK_WDLL_LBCTRL_MAX_VAL(val)  vBIT(val, 9, 7)
/* 0x00240 */	u64	g3fbct_tran_wrd_cnt;
#define	VXGE_HAL_G3FBCT_TRAN_WRD_CNT_CTRL_PIPE_WR(val)	    vBIT(val, 0, 32)
#define	VXGE_HAL_G3FBCT_TRAN_WRD_CNT_CTRL_PIPE_RD(val)	    vBIT(val, 32, 32)
/* 0x00248 */	u64	g3fbct_tran_ap_cnt;
#define	VXGE_HAL_G3FBCT_TRAN_AP_CNT_CTRL_PIPE_ACT(val)	    vBIT(val, 0, 16)
#define	VXGE_HAL_G3FBCT_TRAN_AP_CNT_CTRL_PIPE_PRE(val)	    vBIT(val, 16, 16)
#define	VXGE_HAL_G3FBCT_TRAN_AP_CNT_UPDATE		    mBIT(39)
/* 0x00250 */	u64	g3fbct_g3bist;
#define	VXGE_HAL_G3FBCT_G3BIST_DISABLE_MAIN		    mBIT(7)
#define	VXGE_HAL_G3FBCT_G3BIST_DISABLE_ICTRL		    mBIT(15)
#define	VXGE_HAL_G3FBCT_G3BIST_BTCTRL_STATUS_MAIN(val)	    vBIT(val, 21, 3)
#define	VXGE_HAL_G3FBCT_G3BIST_BTCTRL_STATUS_ICTRL(val)	    vBIT(val, 29, 3)
	u8	unused00a00[0x00a00 - 0x00258];

/* 0x00a00 */	u64	wrdma_int_status;
#define	VXGE_HAL_WRDMA_INT_STATUS_RC_ALARM_RC_INT	    mBIT(0)
#define	VXGE_HAL_WRDMA_INT_STATUS_RXDRM_SM_ERR_RXDRM_INT    mBIT(1)
#define	VXGE_HAL_WRDMA_INT_STATUS_RXDCM_SM_ERR_RXDCM_SM_INT mBIT(2)
#define	VXGE_HAL_WRDMA_INT_STATUS_RXDWM_SM_ERR_RXDWM_INT    mBIT(3)
#define	VXGE_HAL_WRDMA_INT_STATUS_RDA_ERR_RDA_INT	    mBIT(6)
#define	VXGE_HAL_WRDMA_INT_STATUS_RDA_ECC_DB_RDA_ECC_DB_INT mBIT(8)
#define	VXGE_HAL_WRDMA_INT_STATUS_RDA_ECC_SG_RDA_ECC_SG_INT mBIT(9)
#define	VXGE_HAL_WRDMA_INT_STATUS_FRF_ALARM_FRF_INT	    mBIT(12)
#define	VXGE_HAL_WRDMA_INT_STATUS_ROCRC_ALARM_ROCRC_INT	    mBIT(13)
#define	VXGE_HAL_WRDMA_INT_STATUS_WDE0_ALARM_WDE0_INT	    mBIT(14)
#define	VXGE_HAL_WRDMA_INT_STATUS_WDE1_ALARM_WDE1_INT	    mBIT(15)
#define	VXGE_HAL_WRDMA_INT_STATUS_WDE2_ALARM_WDE2_INT	    mBIT(16)
#define	VXGE_HAL_WRDMA_INT_STATUS_WDE3_ALARM_WDE3_INT	    mBIT(17)
/* 0x00a08 */	u64	wrdma_int_mask;
/* 0x00a10 */	u64	rc_alarm_reg;
#define	VXGE_HAL_RC_ALARM_REG_FTC_SM_ERR		    mBIT(0)
#define	VXGE_HAL_RC_ALARM_REG_FTC_SM_PHASE_ERR		    mBIT(1)
#define	VXGE_HAL_RC_ALARM_REG_BTDWM_SM_ERR		    mBIT(2)
#define	VXGE_HAL_RC_ALARM_REG_BTC_SM_ERR		    mBIT(3)
#define	VXGE_HAL_RC_ALARM_REG_BTDCM_SM_ERR		    mBIT(4)
#define	VXGE_HAL_RC_ALARM_REG_BTDRM_SM_ERR		    mBIT(5)
#define	VXGE_HAL_RC_ALARM_REG_RMM_RXD_RC_ECC_DB_ERR	    mBIT(6)
#define	VXGE_HAL_RC_ALARM_REG_RMM_RXD_RC_ECC_SG_ERR	    mBIT(7)
#define	VXGE_HAL_RC_ALARM_REG_RHS_RXD_RHS_ECC_DB_ERR	    mBIT(8)
#define	VXGE_HAL_RC_ALARM_REG_RHS_RXD_RHS_ECC_SG_ERR	    mBIT(9)
#define	VXGE_HAL_RC_ALARM_REG_RMM_SM_ERR		    mBIT(10)
#define	VXGE_HAL_RC_ALARM_REG_BTC_VPATH_MISMATCH_ERR	    mBIT(12)
/* 0x00a18 */	u64	rc_alarm_mask;
/* 0x00a20 */	u64	rc_alarm_alarm;
/* 0x00a28 */	u64	rxdrm_sm_err_reg;
#define	VXGE_HAL_RXDRM_SM_ERR_REG_PRC_VP(n)		    mBIT(n)
/* 0x00a30 */	u64	rxdrm_sm_err_mask;
/* 0x00a38 */	u64	rxdrm_sm_err_alarm;
/* 0x00a40 */	u64	rxdcm_sm_err_reg;
#define	VXGE_HAL_RXDCM_SM_ERR_REG_PRC_VP(n)		    mBIT(n)
/* 0x00a48 */	u64	rxdcm_sm_err_mask;
/* 0x00a50 */	u64	rxdcm_sm_err_alarm;
/* 0x00a58 */	u64	rxdwm_sm_err_reg;
#define	VXGE_HAL_RXDWM_SM_ERR_REG_PRC_VP(n)		    mBIT(n)
/* 0x00a60 */	u64	rxdwm_sm_err_mask;
/* 0x00a68 */	u64	rxdwm_sm_err_alarm;
/* 0x00a70 */	u64	rda_err_reg;
#define	VXGE_HAL_RDA_ERR_REG_RDA_SM0_ERR_ALARM		    mBIT(0)
#define	VXGE_HAL_RDA_ERR_REG_RDA_MISC_ERR		    mBIT(1)
#define	VXGE_HAL_RDA_ERR_REG_RDA_PCIX_ERR		    mBIT(2)
#define	VXGE_HAL_RDA_ERR_REG_RDA_RXD_ECC_DB_ERR		    mBIT(3)
#define	VXGE_HAL_RDA_ERR_REG_RDA_FRM_ECC_DB_ERR		    mBIT(4)
#define	VXGE_HAL_RDA_ERR_REG_RDA_UQM_ECC_DB_ERR		    mBIT(5)
#define	VXGE_HAL_RDA_ERR_REG_RDA_IMM_ECC_DB_ERR		    mBIT(6)
#define	VXGE_HAL_RDA_ERR_REG_RDA_TIM_ECC_DB_ERR		    mBIT(7)
/* 0x00a78 */	u64	rda_err_mask;
/* 0x00a80 */	u64	rda_err_alarm;
/* 0x00a88 */	u64	rda_ecc_db_reg;
#define	VXGE_HAL_RDA_ECC_DB_REG_RDA_RXD_ERR(n)		    mBIT(n)
/* 0x00a90 */	u64	rda_ecc_db_mask;
/* 0x00a98 */	u64	rda_ecc_db_alarm;
/* 0x00aa0 */	u64	rda_ecc_sg_reg;
#define	VXGE_HAL_RDA_ECC_SG_REG_RDA_RXD_ERR(n)		    mBIT(n)
/* 0x00aa8 */	u64	rda_ecc_sg_mask;
/* 0x00ab0 */	u64	rda_ecc_sg_alarm;
/* 0x00ab8 */	u64	rqa_err_reg;
#define	VXGE_HAL_RQA_ERR_REG_RQA_SM_ERR_ALARM		    mBIT(0)
/* 0x00ac0 */	u64	rqa_err_mask;
/* 0x00ac8 */	u64	rqa_err_alarm;
/* 0x00ad0 */	u64	frf_alarm_reg;
#define	VXGE_HAL_FRF_ALARM_REG_PRC_VP_FRF_SM_ERR(n)	    mBIT(n)
/* 0x00ad8 */	u64	frf_alarm_mask;
/* 0x00ae0 */	u64	frf_alarm_alarm;
/* 0x00ae8 */	u64	rocrc_alarm_reg;
#define	VXGE_HAL_ROCRC_ALARM_REG_QCQ_QCC_BYP_ECC_DB	    mBIT(0)
#define	VXGE_HAL_ROCRC_ALARM_REG_QCQ_QCC_BYP_ECC_SG	    mBIT(1)
#define	VXGE_HAL_ROCRC_ALARM_REG_NOA_NMA_SM_ERR		    mBIT(2)
#define	VXGE_HAL_ROCRC_ALARM_REG_NOA_IMMM_ECC_DB	    mBIT(3)
#define	VXGE_HAL_ROCRC_ALARM_REG_NOA_IMMM_ECC_SG	    mBIT(4)
#define	VXGE_HAL_ROCRC_ALARM_REG_UDQ_UMQM_ECC_DB	    mBIT(5)
#define	VXGE_HAL_ROCRC_ALARM_REG_UDQ_UMQM_ECC_SG	    mBIT(6)
#define	VXGE_HAL_ROCRC_ALARM_REG_NOA_RCBM_ECC_DB	    mBIT(11)
#define	VXGE_HAL_ROCRC_ALARM_REG_NOA_RCBM_ECC_SG	    mBIT(12)
#define	VXGE_HAL_ROCRC_ALARM_REG_QCQ_MULTI_EGB_RSVD_ERR	    mBIT(13)
#define	VXGE_HAL_ROCRC_ALARM_REG_QCQ_MULTI_EGB_OWN_ERR	    mBIT(14)
#define	VXGE_HAL_ROCRC_ALARM_REG_QCQ_MULTI_BYP_OWN_ERR	    mBIT(15)
#define	VXGE_HAL_ROCRC_ALARM_REG_QCQ_OWN_NOT_ASSIGNED_ERR   mBIT(16)
#define	VXGE_HAL_ROCRC_ALARM_REG_QCQ_OWN_RSVD_SYNC_ERR	    mBIT(17)
#define	VXGE_HAL_ROCRC_ALARM_REG_QCQ_LOST_EGB_ERR	    mBIT(18)
#define	VXGE_HAL_ROCRC_ALARM_REG_RCQ_BYPQ0_OVERFLOW	    mBIT(19)
#define	VXGE_HAL_ROCRC_ALARM_REG_RCQ_BYPQ1_OVERFLOW	    mBIT(20)
#define	VXGE_HAL_ROCRC_ALARM_REG_RCQ_BYPQ2_OVERFLOW	    mBIT(21)
#define	VXGE_HAL_ROCRC_ALARM_REG_NOA_WCT_CMD_FIFO_ERR	    mBIT(22)
/* 0x00af0 */	u64	rocrc_alarm_mask;
/* 0x00af8 */	u64	rocrc_alarm_alarm;
/* 0x00b00 */	u64	wde0_alarm_reg;
#define	VXGE_HAL_WDE0_ALARM_REG_WDE0_DCC_SM_ERR		    mBIT(0)
#define	VXGE_HAL_WDE0_ALARM_REG_WDE0_PRM_SM_ERR		    mBIT(1)
#define	VXGE_HAL_WDE0_ALARM_REG_WDE0_CP_SM_ERR		    mBIT(2)
#define	VXGE_HAL_WDE0_ALARM_REG_WDE0_CP_CMD_ERR		    mBIT(3)
#define	VXGE_HAL_WDE0_ALARM_REG_WDE0_PCR_SM_ERR		    mBIT(4)
/* 0x00b08 */	u64	wde0_alarm_mask;
/* 0x00b10 */	u64	wde0_alarm_alarm;
/* 0x00b18 */	u64	wde1_alarm_reg;
#define	VXGE_HAL_WDE1_ALARM_REG_WDE1_DCC_SM_ERR		    mBIT(0)
#define	VXGE_HAL_WDE1_ALARM_REG_WDE1_PRM_SM_ERR		    mBIT(1)
#define	VXGE_HAL_WDE1_ALARM_REG_WDE1_CP_SM_ERR		    mBIT(2)
#define	VXGE_HAL_WDE1_ALARM_REG_WDE1_CP_CMD_ERR		    mBIT(3)
#define	VXGE_HAL_WDE1_ALARM_REG_WDE1_PCR_SM_ERR		    mBIT(4)
/* 0x00b20 */	u64	wde1_alarm_mask;
/* 0x00b28 */	u64	wde1_alarm_alarm;
/* 0x00b30 */	u64	wde2_alarm_reg;
#define	VXGE_HAL_WDE2_ALARM_REG_WDE2_DCC_SM_ERR		    mBIT(0)
#define	VXGE_HAL_WDE2_ALARM_REG_WDE2_PRM_SM_ERR		    mBIT(1)
#define	VXGE_HAL_WDE2_ALARM_REG_WDE2_CP_SM_ERR		    mBIT(2)
#define	VXGE_HAL_WDE2_ALARM_REG_WDE2_CP_CMD_ERR		    mBIT(3)
#define	VXGE_HAL_WDE2_ALARM_REG_WDE2_PCR_SM_ERR		    mBIT(4)
/* 0x00b38 */	u64	wde2_alarm_mask;
/* 0x00b40 */	u64	wde2_alarm_alarm;
/* 0x00b48 */	u64	wde3_alarm_reg;
#define	VXGE_HAL_WDE3_ALARM_REG_WDE3_DCC_SM_ERR		    mBIT(0)
#define	VXGE_HAL_WDE3_ALARM_REG_WDE3_PRM_SM_ERR		    mBIT(1)
#define	VXGE_HAL_WDE3_ALARM_REG_WDE3_CP_SM_ERR		    mBIT(2)
#define	VXGE_HAL_WDE3_ALARM_REG_WDE3_CP_CMD_ERR		    mBIT(3)
#define	VXGE_HAL_WDE3_ALARM_REG_WDE3_PCR_SM_ERR		    mBIT(4)
/* 0x00b50 */	u64	wde3_alarm_mask;
/* 0x00b58 */	u64	wde3_alarm_alarm;
/* 0x00b60 */	u64	rc_cfg;
#define	VXGE_HAL_RC_CFG_RXD_ERR_MASK(val)		    vBIT(val, 0, 4)
#define	VXGE_HAL_RC_CFG_RXD_RD_RO			    mBIT(12)
#define	VXGE_HAL_RC_CFG_FIXED_BUFFER_SIZE		    mBIT(13)
#define	VXGE_HAL_RC_CFG_ENABLE_VP_CFG_CHANGE_WHILE_BUSY	    mBIT(14)
#define	VXGE_HAL_RC_CFG_PRESERVE_BUFFER_SIZE		    mBIT(15)
/* 0x00b68 */	u64	ecc_cfg;
#define	VXGE_HAL_ECC_CFG_RXD_RC_ECC_ENABLE_N		    mBIT(0)
#define	VXGE_HAL_ECC_CFG_RXD_RHS_ECC_ENABLE_N		    mBIT(1)
#define	VXGE_HAL_ECC_CFG_NOA_IMMM_ECC_ENABLE_N		    mBIT(4)
#define	VXGE_HAL_ECC_CFG_UDQ_UMQM_ECC_ENABLE_N		    mBIT(5)
#define	VXGE_HAL_ECC_CFG_RCBM_CQB_ECC_ENABLE_N		    mBIT(7)
/* 0x00b70 */	u64	rxd_cfg_1bm;
#define	VXGE_HAL_RXD_CFG_1BM_QW_SIZE(val)		    vBIT(val, 5, 3)
#define	VXGE_HAL_RXD_CFG_1BM_QW2WRITE(val)		    vBIT(val, 8, 8)
#define	VXGE_HAL_RXD_CFG_1BM_HCW_QWOFF(val)		    vBIT(val, 21, 3)
#define	VXGE_HAL_RXD_CFG_1BM_RTH_VAL_QWOFF(val)		    vBIT(val, 29, 3)
#define	VXGE_HAL_RXD_CFG_1BM_RTH_VAL_W0OFF(val)		    vBIT(val, 38, 2)
#define	VXGE_HAL_RXD_CFG_1BM_RTH_VAL_W1OFF(val)		    vBIT(val, 46, 2)
#define	VXGE_HAL_RXD_CFG_1BM_HEAD_OWN_QWOFF(val)	    vBIT(val, 53, 3)
#define	VXGE_HAL_RXD_CFG_1BM_HEAD_OWN_BOFF(val)		    vBIT(val, 61, 3)
/* 0x00b78 */	u64	rxd_cfg1_1bm;
#define	VXGE_HAL_RXD_CFG1_1BM_BUFF1_SIZE_QWOFF(val)	    vBIT(val, 5, 3)
#define	VXGE_HAL_RXD_CFG1_1BM_TRSF_CODE_QWOFF(val)	    vBIT(val, 45, 3)
#define	VXGE_HAL_RXD_CFG1_1BM_TRSF_CODE_BOFF(val)	    vBIT(val, 53, 3)
#define	VXGE_HAL_RXD_CFG1_1BM_RTH_BUCKET_DATA_QWOF(val)	    vBIT(val, 61, 3)
/* 0x00b80 */	u64	rxd_cfg2_1bm;
#define	VXGE_HAL_RXD_CFG2_1BM_RTH_BUCKET_DATA_BOFF(val)	    vBIT(val, 5, 3)
#define	VXGE_HAL_RXD_CFG2_1BM_BUFF1_SIZE_WOFF(val)	    vBIT(val, 14, 2)
#define	VXGE_HAL_RXD_CFG2_1BM_FRM_INFO_QWOFF(val)	    vBIT(val, 53, 3)
#define	VXGE_HAL_RXD_CFG2_1BM_FRM_INFO_BOFF(val)	    vBIT(val, 61, 3)
/* 0x00b88 */	u64	rxd_cfg3_1bm;
#define	VXGE_HAL_RXD_CFG3_1BM_BUFF1_PTR_QWOFF(val)	    vBIT(val, 5, 3)
#define	VXGE_HAL_RXD_CFG3_1BM_TAIL_OWN_QWOFF(val)	    vBIT(val, 45, 3)
#define	VXGE_HAL_RXD_CFG3_1BM_TAIL_OWN_BOFF(val)	    vBIT(val, 53, 3)
#define	VXGE_HAL_RXD_CFG3_1BM_HEAD_OWN_BIT_IDX(val)	    vBIT(val, 57, 3)
#define	VXGE_HAL_RXD_CFG3_1BM_TAIL_OWN_BIT_IDX(val)	    vBIT(val, 61, 3)
/* 0x00b90 */	u64	rxd_cfg4_1bm;
#define	VXGE_HAL_RXD_CFG4_1BM_L3C_QWOFF(val)		    vBIT(val, 5, 3)
#define	VXGE_HAL_RXD_CFG4_1BM_L3C_WOFF(val)		    vBIT(val, 14, 2)
#define	VXGE_HAL_RXD_CFG4_1BM_L4C_QWOFF(val)		    vBIT(val, 21, 3)
#define	VXGE_HAL_RXD_CFG4_1BM_L4C_WOFF(val)		    vBIT(val, 30, 2)
#define	VXGE_HAL_RXD_CFG4_1BM_VTAG_QWOFF(val)		    vBIT(val, 37, 3)
#define	VXGE_HAL_RXD_CFG4_1BM_VTAG_WOFF(val)		    vBIT(val, 46, 2)
#define	VXGE_HAL_RXD_CFG4_1BM_RTH_INFO_QWOFF(val)	    vBIT(val, 53, 3)
#define	VXGE_HAL_RXD_CFG4_1BM_RTH_INFO_BOFF(val)	    vBIT(val, 61, 3)
/* 0x00b98 */	u64	rxd_cfg_3bm;
#define	VXGE_HAL_RXD_CFG_3BM_QW_SIZE(val)		    vBIT(val, 5, 3)
#define	VXGE_HAL_RXD_CFG_3BM_QW2WRITE(val)		    vBIT(val, 8, 8)
#define	VXGE_HAL_RXD_CFG_3BM_HCW_QWOFF(val)		    vBIT(val, 21, 3)
#define	VXGE_HAL_RXD_CFG_3BM_RTH_VAL_QWOFF(val)		    vBIT(val, 29, 3)
#define	VXGE_HAL_RXD_CFG_3BM_RTH_VAL_W0OFF(val)		    vBIT(val, 38, 2)
#define	VXGE_HAL_RXD_CFG_3BM_RTH_VAL_W1OFF(val)		    vBIT(val, 46, 2)
#define	VXGE_HAL_RXD_CFG_3BM_HEAD_OWN_QWOFF(val)	    vBIT(val, 53, 3)
#define	VXGE_HAL_RXD_CFG_3BM_HEAD_OWN_BOFF(val)		    vBIT(val, 61, 3)
/* 0x00ba0 */	u64	rxd_cfg1_3bm;
#define	VXGE_HAL_RXD_CFG1_3BM_BUFF1_SIZE_QWOFF(val)	    vBIT(val, 5, 3)
#define	VXGE_HAL_RXD_CFG1_3BM_BUFF2_SIZE_QWOFF(val)	    vBIT(val, 13, 3)
#define	VXGE_HAL_RXD_CFG1_3BM_BUFF3_SIZE_QWOFF(val)	    vBIT(val, 21, 3)
#define	VXGE_HAL_RXD_CFG1_3BM_TRSF_CODE_QWOFF(val)	    vBIT(val, 45, 3)
#define	VXGE_HAL_RXD_CFG1_3BM_TRSF_CODE_BOFF(val)	    vBIT(val, 53, 3)
#define	VXGE_HAL_RXD_CFG1_3BM_RTH_BUCKET_DATA_QWOF(val)	    vBIT(val, 61, 3)
/* 0x00ba8 */	u64	rxd_cfg2_3bm;
#define	VXGE_HAL_RXD_CFG2_3BM_RTH_BUCKET_DATA_BOFF(val)	    vBIT(val, 5, 3)
#define	VXGE_HAL_RXD_CFG2_3BM_BUFF1_SIZE_WOFF(val)	    vBIT(val, 14, 2)
#define	VXGE_HAL_RXD_CFG2_3BM_BUFF2_SIZE_WOFF(val)	    vBIT(val, 22, 2)
#define	VXGE_HAL_RXD_CFG2_3BM_BUFF3_SIZE_WOFF(val)	    vBIT(val, 30, 2)
#define	VXGE_HAL_RXD_CFG2_3BM_FRM_INFO_QWOFF(val)	    vBIT(val, 53, 3)
#define	VXGE_HAL_RXD_CFG2_3BM_FRM_INFO_BOFF(val)	    vBIT(val, 61, 3)
/* 0x00bb0 */	u64	rxd_cfg3_3bm;
#define	VXGE_HAL_RXD_CFG3_3BM_BUFF1_PTR_QWOFF(val)	    vBIT(val, 5, 3)
#define	VXGE_HAL_RXD_CFG3_3BM_BUFF2_PTR_QWOFF(val)	    vBIT(val, 13, 3)
#define	VXGE_HAL_RXD_CFG3_3BM_BUFF3_PTR_QWOFF(val)	    vBIT(val, 21, 3)
#define	VXGE_HAL_RXD_CFG3_3BM_TAIL_OWN_QWOFF(val)	    vBIT(val, 45, 3)
#define	VXGE_HAL_RXD_CFG3_3BM_TAIL_OWN_BOFF(val)	    vBIT(val, 53, 3)
#define	VXGE_HAL_RXD_CFG3_3BM_HEAD_OWN_BIT_IDX(val)	    vBIT(val, 57, 3)
#define	VXGE_HAL_RXD_CFG3_3BM_TAIL_OWN_BIT_IDX(val)	    vBIT(val, 61, 3)
/* 0x00bb8 */	u64	rxd_cfg4_3bm;
#define	VXGE_HAL_RXD_CFG4_3BM_L3C_QWOFF(val)		    vBIT(val, 5, 3)
#define	VXGE_HAL_RXD_CFG4_3BM_L3C_WOFF(val)		    vBIT(val, 14, 2)
#define	VXGE_HAL_RXD_CFG4_3BM_L4C_QWOFF(val)		    vBIT(val, 21, 3)
#define	VXGE_HAL_RXD_CFG4_3BM_L4C_WOFF(val)		    vBIT(val, 30, 2)
#define	VXGE_HAL_RXD_CFG4_3BM_VTAG_QWOFF(val)		    vBIT(val, 37, 3)
#define	VXGE_HAL_RXD_CFG4_3BM_VTAG_WOFF(val)		    vBIT(val, 46, 2)
#define	VXGE_HAL_RXD_CFG4_3BM_RTH_INFO_QWOFF(val)	    vBIT(val, 53, 3)
#define	VXGE_HAL_RXD_CFG4_3BM_RTH_INFO_BOFF(val)	    vBIT(val, 61, 3)
/* 0x00bc0 */	u64	rxd_cfg_5bm;
#define	VXGE_HAL_RXD_CFG_5BM_QW_SIZE(val)		    vBIT(val, 5, 3)
#define	VXGE_HAL_RXD_CFG_5BM_QW2WRITE(val)		    vBIT(val, 8, 8)
#define	VXGE_HAL_RXD_CFG_5BM_HCW_QWOFF(val)		    vBIT(val, 21, 3)
#define	VXGE_HAL_RXD_CFG_5BM_RTH_VAL_QWOFF(val)		    vBIT(val, 29, 3)
#define	VXGE_HAL_RXD_CFG_5BM_RTH_VAL_W0OFF(val)		    vBIT(val, 38, 2)
#define	VXGE_HAL_RXD_CFG_5BM_RTH_VAL_W1OFF(val)		    vBIT(val, 46, 2)
#define	VXGE_HAL_RXD_CFG_5BM_HEAD_OWN_QWOFF(val)	    vBIT(val, 53, 3)
#define	VXGE_HAL_RXD_CFG_5BM_HEAD_OWN_BOFF(val)		    vBIT(val, 61, 3)
/* 0x00bc8 */	u64	rxd_cfg1_5bm;
#define	VXGE_HAL_RXD_CFG1_5BM_BUFF1_SIZE_QWOFF(val)	    vBIT(val, 5, 3)
#define	VXGE_HAL_RXD_CFG1_5BM_BUFF2_SIZE_QWOFF(val)	    vBIT(val, 13, 3)
#define	VXGE_HAL_RXD_CFG1_5BM_BUFF3_SIZE_QWOFF(val)	    vBIT(val, 21, 3)
#define	VXGE_HAL_RXD_CFG1_5BM_BUFF4_SIZE_QWOFF(val)	    vBIT(val, 29, 3)
#define	VXGE_HAL_RXD_CFG1_5BM_BUFF5_SIZE_QWOFF(val)	    vBIT(val, 37, 3)
#define	VXGE_HAL_RXD_CFG1_5BM_TRSF_CODE_QWOFF(val)	    vBIT(val, 45, 3)
#define	VXGE_HAL_RXD_CFG1_5BM_TRSF_CODE_BOFF(val)	    vBIT(val, 53, 3)
#define	VXGE_HAL_RXD_CFG1_5BM_RTH_BUCKET_DATA_QWOF(val)	    vBIT(val, 61, 3)
/* 0x00bd0 */	u64	rxd_cfg2_5bm;
#define	VXGE_HAL_RXD_CFG2_5BM_RTH_BUCKET_DATA_BOFF(val)	    vBIT(val, 5, 3)
#define	VXGE_HAL_RXD_CFG2_5BM_BUFF1_SIZE_WOFF(val)	    vBIT(val, 14, 2)
#define	VXGE_HAL_RXD_CFG2_5BM_BUFF2_SIZE_WOFF(val)	    vBIT(val, 22, 2)
#define	VXGE_HAL_RXD_CFG2_5BM_BUFF3_SIZE_WOFF(val)	    vBIT(val, 30, 2)
#define	VXGE_HAL_RXD_CFG2_5BM_BUFF4_SIZE_WOFF(val)	    vBIT(val, 38, 2)
#define	VXGE_HAL_RXD_CFG2_5BM_BUFF5_SIZE_WOFF(val)	    vBIT(val, 46, 2)
#define	VXGE_HAL_RXD_CFG2_5BM_FRM_INFO_QWOFF(val)	    vBIT(val, 53, 3)
#define	VXGE_HAL_RXD_CFG2_5BM_FRM_INFO_BOFF(val)	    vBIT(val, 61, 3)
/* 0x00bd8 */	u64	rxd_cfg3_5bm;
#define	VXGE_HAL_RXD_CFG3_5BM_BUFF1_PTR_QWOFF(val)	    vBIT(val, 5, 3)
#define	VXGE_HAL_RXD_CFG3_5BM_BUFF2_PTR_QWOFF(val)	    vBIT(val, 13, 3)
#define	VXGE_HAL_RXD_CFG3_5BM_BUFF3_PTR_QWOFF(val)	    vBIT(val, 21, 3)
#define	VXGE_HAL_RXD_CFG3_5BM_BUFF4_PTR_QWOFF(val)	    vBIT(val, 29, 3)
#define	VXGE_HAL_RXD_CFG3_5BM_BUFF5_PTR_QWOFF(val)	    vBIT(val, 37, 3)
#define	VXGE_HAL_RXD_CFG3_5BM_TAIL_OWN_QWOFF(val)	    vBIT(val, 45, 3)
#define	VXGE_HAL_RXD_CFG3_5BM_TAIL_OWN_BOFF(val)	    vBIT(val, 53, 3)
#define	VXGE_HAL_RXD_CFG3_5BM_HEAD_OWN_BIT_IDX(val)	    vBIT(val, 57, 3)
#define	VXGE_HAL_RXD_CFG3_5BM_TAIL_OWN_BIT_IDX(val)	    vBIT(val, 61, 3)
/* 0x00be0 */	u64	rxd_cfg4_5bm;
#define	VXGE_HAL_RXD_CFG4_5BM_L3C_QWOFF(val)		    vBIT(val, 5, 3)
#define	VXGE_HAL_RXD_CFG4_5BM_L3C_WOFF(val)		    vBIT(val, 14, 2)
#define	VXGE_HAL_RXD_CFG4_5BM_L4C_QWOFF(val)		    vBIT(val, 21, 3)
#define	VXGE_HAL_RXD_CFG4_5BM_L4C_WOFF(val)		    vBIT(val, 30, 2)
#define	VXGE_HAL_RXD_CFG4_5BM_VTAG_QWOFF(val)		    vBIT(val, 37, 3)
#define	VXGE_HAL_RXD_CFG4_5BM_VTAG_WOFF(val)		    vBIT(val, 46, 2)
#define	VXGE_HAL_RXD_CFG4_5BM_RTH_INFO_QWOFF(val)	    vBIT(val, 53, 3)
#define	VXGE_HAL_RXD_CFG4_5BM_RTH_INFO_BOFF(val)	    vBIT(val, 61, 3)
/* 0x00be8 */	u64	rx_w_round_robin_0;
#define	VXGE_HAL_RX_W_ROUND_ROBIN_0_RX_W_PRIORITY_SS_0(val) vBIT(val, 3, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_0_RX_W_PRIORITY_SS_1(val) vBIT(val, 11, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_0_RX_W_PRIORITY_SS_2(val) vBIT(val, 19, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_0_RX_W_PRIORITY_SS_3(val) vBIT(val, 27, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_0_RX_W_PRIORITY_SS_4(val) vBIT(val, 35, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_0_RX_W_PRIORITY_SS_5(val) vBIT(val, 43, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_0_RX_W_PRIORITY_SS_6(val) vBIT(val, 51, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_0_RX_W_PRIORITY_SS_7(val) vBIT(val, 59, 5)
/* 0x00bf0 */	u64	rx_w_round_robin_1;
#define	VXGE_HAL_RX_W_ROUND_ROBIN_1_RX_W_PRIORITY_SS_8(val) vBIT(val, 3, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_1_RX_W_PRIORITY_SS_9(val) vBIT(val, 11, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_1_RX_W_PRIORITY_SS_10(val) vBIT(val, 19, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_1_RX_W_PRIORITY_SS_11(val) vBIT(val, 27, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_1_RX_W_PRIORITY_SS_12(val) vBIT(val, 35, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_1_RX_W_PRIORITY_SS_13(val) vBIT(val, 43, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_1_RX_W_PRIORITY_SS_14(val) vBIT(val, 51, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_1_RX_W_PRIORITY_SS_15(val) vBIT(val, 59, 5)
/* 0x00bf8 */	u64	rx_w_round_robin_2;
#define	VXGE_HAL_RX_W_ROUND_ROBIN_2_RX_W_PRIORITY_SS_16(val) vBIT(val, 3, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_2_RX_W_PRIORITY_SS_17(val) vBIT(val, 11, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_2_RX_W_PRIORITY_SS_18(val) vBIT(val, 19, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_2_RX_W_PRIORITY_SS_19(val) vBIT(val, 27, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_2_RX_W_PRIORITY_SS_20(val) vBIT(val, 35, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_2_RX_W_PRIORITY_SS_21(val) vBIT(val, 43, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_2_RX_W_PRIORITY_SS_22(val) vBIT(val, 51, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_2_RX_W_PRIORITY_SS_23(val) vBIT(val, 59, 5)
/* 0x00c00 */	u64	rx_w_round_robin_3;
#define	VXGE_HAL_RX_W_ROUND_ROBIN_3_RX_W_PRIORITY_SS_24(val) vBIT(val, 3, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_3_RX_W_PRIORITY_SS_25(val) vBIT(val, 11, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_3_RX_W_PRIORITY_SS_26(val) vBIT(val, 19, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_3_RX_W_PRIORITY_SS_27(val) vBIT(val, 27, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_3_RX_W_PRIORITY_SS_28(val) vBIT(val, 35, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_3_RX_W_PRIORITY_SS_29(val) vBIT(val, 43, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_3_RX_W_PRIORITY_SS_30(val) vBIT(val, 51, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_3_RX_W_PRIORITY_SS_31(val) vBIT(val, 59, 5)
/* 0x00c08 */	u64	rx_w_round_robin_4;
#define	VXGE_HAL_RX_W_ROUND_ROBIN_4_RX_W_PRIORITY_SS_32(val) vBIT(val, 3, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_4_RX_W_PRIORITY_SS_33(val) vBIT(val, 11, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_4_RX_W_PRIORITY_SS_34(val) vBIT(val, 19, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_4_RX_W_PRIORITY_SS_35(val) vBIT(val, 27, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_4_RX_W_PRIORITY_SS_36(val) vBIT(val, 35, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_4_RX_W_PRIORITY_SS_37(val) vBIT(val, 43, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_4_RX_W_PRIORITY_SS_38(val) vBIT(val, 51, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_4_RX_W_PRIORITY_SS_39(val) vBIT(val, 59, 5)
/* 0x00c10 */	u64	rx_w_round_robin_5;
#define	VXGE_HAL_RX_W_ROUND_ROBIN_5_RX_W_PRIORITY_SS_40(val) vBIT(val, 3, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_5_RX_W_PRIORITY_SS_41(val) vBIT(val, 11, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_5_RX_W_PRIORITY_SS_42(val) vBIT(val, 19, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_5_RX_W_PRIORITY_SS_43(val) vBIT(val, 27, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_5_RX_W_PRIORITY_SS_44(val) vBIT(val, 35, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_5_RX_W_PRIORITY_SS_45(val) vBIT(val, 43, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_5_RX_W_PRIORITY_SS_46(val) vBIT(val, 51, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_5_RX_W_PRIORITY_SS_47(val) vBIT(val, 59, 5)
/* 0x00c18 */	u64	rx_w_round_robin_6;
#define	VXGE_HAL_RX_W_ROUND_ROBIN_6_RX_W_PRIORITY_SS_48(val) vBIT(val, 3, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_6_RX_W_PRIORITY_SS_49(val) vBIT(val, 11, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_6_RX_W_PRIORITY_SS_50(val) vBIT(val, 19, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_6_RX_W_PRIORITY_SS_51(val) vBIT(val, 27, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_6_RX_W_PRIORITY_SS_52(val) vBIT(val, 35, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_6_RX_W_PRIORITY_SS_53(val) vBIT(val, 43, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_6_RX_W_PRIORITY_SS_54(val) vBIT(val, 51, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_6_RX_W_PRIORITY_SS_55(val) vBIT(val, 59, 5)
/* 0x00c20 */	u64	rx_w_round_robin_7;
#define	VXGE_HAL_RX_W_ROUND_ROBIN_7_RX_W_PRIORITY_SS_56(val) vBIT(val, 3, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_7_RX_W_PRIORITY_SS_57(val) vBIT(val, 11, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_7_RX_W_PRIORITY_SS_58(val) vBIT(val, 19, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_7_RX_W_PRIORITY_SS_59(val) vBIT(val, 27, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_7_RX_W_PRIORITY_SS_60(val) vBIT(val, 35, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_7_RX_W_PRIORITY_SS_61(val) vBIT(val, 43, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_7_RX_W_PRIORITY_SS_62(val) vBIT(val, 51, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_7_RX_W_PRIORITY_SS_63(val) vBIT(val, 59, 5)
/* 0x00c28 */	u64	rx_w_round_robin_8;
#define	VXGE_HAL_RX_W_ROUND_ROBIN_8_RX_W_PRIORITY_SS_64(val) vBIT(val, 3, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_8_RX_W_PRIORITY_SS_65(val) vBIT(val, 11, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_8_RX_W_PRIORITY_SS_66(val) vBIT(val, 19, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_8_RX_W_PRIORITY_SS_67(val) vBIT(val, 27, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_8_RX_W_PRIORITY_SS_68(val) vBIT(val, 35, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_8_RX_W_PRIORITY_SS_69(val) vBIT(val, 43, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_8_RX_W_PRIORITY_SS_70(val) vBIT(val, 51, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_8_RX_W_PRIORITY_SS_71(val) vBIT(val, 59, 5)
/* 0x00c30 */	u64	rx_w_round_robin_9;
#define	VXGE_HAL_RX_W_ROUND_ROBIN_9_RX_W_PRIORITY_SS_72(val) vBIT(val, 3, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_9_RX_W_PRIORITY_SS_73(val) vBIT(val, 11, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_9_RX_W_PRIORITY_SS_74(val) vBIT(val, 19, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_9_RX_W_PRIORITY_SS_75(val) vBIT(val, 27, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_9_RX_W_PRIORITY_SS_76(val) vBIT(val, 35, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_9_RX_W_PRIORITY_SS_77(val) vBIT(val, 43, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_9_RX_W_PRIORITY_SS_78(val) vBIT(val, 51, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_9_RX_W_PRIORITY_SS_79(val) vBIT(val, 59, 5)
/* 0x00c38 */	u64	rx_w_round_robin_10;
#define	VXGE_HAL_RX_W_ROUND_ROBIN_10_RX_W_PRIORITY_SS_80(val) vBIT(val, 3, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_10_RX_W_PRIORITY_SS_81(val) vBIT(val, 11, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_10_RX_W_PRIORITY_SS_82(val) vBIT(val, 19, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_10_RX_W_PRIORITY_SS_83(val) vBIT(val, 27, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_10_RX_W_PRIORITY_SS_84(val) vBIT(val, 35, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_10_RX_W_PRIORITY_SS_85(val) vBIT(val, 43, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_10_RX_W_PRIORITY_SS_86(val) vBIT(val, 51, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_10_RX_W_PRIORITY_SS_87(val) vBIT(val, 59, 5)
/* 0x00c40 */	u64	rx_w_round_robin_11;
#define	VXGE_HAL_RX_W_ROUND_ROBIN_11_RX_W_PRIORITY_SS_88(val) vBIT(val, 3, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_11_RX_W_PRIORITY_SS_89(val) vBIT(val, 11, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_11_RX_W_PRIORITY_SS_90(val) vBIT(val, 19, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_11_RX_W_PRIORITY_SS_91(val) vBIT(val, 27, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_11_RX_W_PRIORITY_SS_92(val) vBIT(val, 35, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_11_RX_W_PRIORITY_SS_93(val) vBIT(val, 43, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_11_RX_W_PRIORITY_SS_94(val) vBIT(val, 51, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_11_RX_W_PRIORITY_SS_95(val) vBIT(val, 59, 5)
/* 0x00c48 */	u64	rx_w_round_robin_12;
#define	VXGE_HAL_RX_W_ROUND_ROBIN_12_RX_W_PRIORITY_SS_96(val) vBIT(val, 3, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_12_RX_W_PRIORITY_SS_97(val) vBIT(val, 11, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_12_RX_W_PRIORITY_SS_98(val) vBIT(val, 19, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_12_RX_W_PRIORITY_SS_99(val) vBIT(val, 27, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_12_RX_W_PRIORITY_SS_100(val) vBIT(val, 35, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_12_RX_W_PRIORITY_SS_101(val) vBIT(val, 43, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_12_RX_W_PRIORITY_SS_102(val) vBIT(val, 51, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_12_RX_W_PRIORITY_SS_103(val) vBIT(val, 59, 5)
/* 0x00c50 */	u64	rx_w_round_robin_13;
#define	VXGE_HAL_RX_W_ROUND_ROBIN_13_RX_W_PRIORITY_SS_104(val) vBIT(val, 3, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_13_RX_W_PRIORITY_SS_105(val) vBIT(val, 11, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_13_RX_W_PRIORITY_SS_106(val) vBIT(val, 19, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_13_RX_W_PRIORITY_SS_107(val) vBIT(val, 27, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_13_RX_W_PRIORITY_SS_108(val) vBIT(val, 35, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_13_RX_W_PRIORITY_SS_109(val) vBIT(val, 43, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_13_RX_W_PRIORITY_SS_110(val) vBIT(val, 51, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_13_RX_W_PRIORITY_SS_111(val) vBIT(val, 59, 5)
/* 0x00c58 */	u64	rx_w_round_robin_14;
#define	VXGE_HAL_RX_W_ROUND_ROBIN_14_RX_W_PRIORITY_SS_112(val) vBIT(val, 3, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_14_RX_W_PRIORITY_SS_113(val) vBIT(val, 11, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_14_RX_W_PRIORITY_SS_114(val) vBIT(val, 19, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_14_RX_W_PRIORITY_SS_115(val) vBIT(val, 27, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_14_RX_W_PRIORITY_SS_116(val) vBIT(val, 35, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_14_RX_W_PRIORITY_SS_117(val) vBIT(val, 43, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_14_RX_W_PRIORITY_SS_118(val) vBIT(val, 51, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_14_RX_W_PRIORITY_SS_119(val) vBIT(val, 59, 5)
/* 0x00c60 */	u64	rx_w_round_robin_15;
#define	VXGE_HAL_RX_W_ROUND_ROBIN_15_RX_W_PRIORITY_SS_120(val) vBIT(val, 3, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_15_RX_W_PRIORITY_SS_121(val) vBIT(val, 11, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_15_RX_W_PRIORITY_SS_122(val) vBIT(val, 19, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_15_RX_W_PRIORITY_SS_123(val) vBIT(val, 27, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_15_RX_W_PRIORITY_SS_124(val) vBIT(val, 35, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_15_RX_W_PRIORITY_SS_125(val) vBIT(val, 43, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_15_RX_W_PRIORITY_SS_126(val) vBIT(val, 51, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_15_RX_W_PRIORITY_SS_127(val) vBIT(val, 59, 5)
/* 0x00c68 */	u64	rx_w_round_robin_16;
#define	VXGE_HAL_RX_W_ROUND_ROBIN_16_RX_W_PRIORITY_SS_128(val) vBIT(val, 3, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_16_RX_W_PRIORITY_SS_129(val) vBIT(val, 11, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_16_RX_W_PRIORITY_SS_130(val) vBIT(val, 19, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_16_RX_W_PRIORITY_SS_131(val) vBIT(val, 27, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_16_RX_W_PRIORITY_SS_132(val) vBIT(val, 35, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_16_RX_W_PRIORITY_SS_133(val) vBIT(val, 43, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_16_RX_W_PRIORITY_SS_134(val) vBIT(val, 51, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_16_RX_W_PRIORITY_SS_135(val) vBIT(val, 59, 5)
/* 0x00c70 */	u64	rx_w_round_robin_17;
#define	VXGE_HAL_RX_W_ROUND_ROBIN_17_RX_W_PRIORITY_SS_136(val) vBIT(val, 3, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_17_RX_W_PRIORITY_SS_137(val) vBIT(val, 11, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_17_RX_W_PRIORITY_SS_138(val) vBIT(val, 19, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_17_RX_W_PRIORITY_SS_139(val) vBIT(val, 27, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_17_RX_W_PRIORITY_SS_140(val) vBIT(val, 35, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_17_RX_W_PRIORITY_SS_141(val) vBIT(val, 43, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_17_RX_W_PRIORITY_SS_142(val) vBIT(val, 51, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_17_RX_W_PRIORITY_SS_143(val) vBIT(val, 59, 5)
/* 0x00c78 */	u64	rx_w_round_robin_18;
#define	VXGE_HAL_RX_W_ROUND_ROBIN_18_RX_W_PRIORITY_SS_144(val) vBIT(val, 3, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_18_RX_W_PRIORITY_SS_145(val) vBIT(val, 11, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_18_RX_W_PRIORITY_SS_146(val) vBIT(val, 19, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_18_RX_W_PRIORITY_SS_147(val) vBIT(val, 27, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_18_RX_W_PRIORITY_SS_148(val) vBIT(val, 35, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_18_RX_W_PRIORITY_SS_149(val) vBIT(val, 43, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_18_RX_W_PRIORITY_SS_150(val) vBIT(val, 51, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_18_RX_W_PRIORITY_SS_151(val) vBIT(val, 59, 5)
/* 0x00c80 */	u64	rx_w_round_robin_19;
#define	VXGE_HAL_RX_W_ROUND_ROBIN_19_RX_W_PRIORITY_SS_152(val) vBIT(val, 3, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_19_RX_W_PRIORITY_SS_153(val) vBIT(val, 11, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_19_RX_W_PRIORITY_SS_154(val) vBIT(val, 19, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_19_RX_W_PRIORITY_SS_155(val) vBIT(val, 27, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_19_RX_W_PRIORITY_SS_156(val) vBIT(val, 35, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_19_RX_W_PRIORITY_SS_157(val) vBIT(val, 43, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_19_RX_W_PRIORITY_SS_158(val) vBIT(val, 51, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_19_RX_W_PRIORITY_SS_159(val) vBIT(val, 59, 5)
/* 0x00c88 */	u64	rx_w_round_robin_20;
#define	VXGE_HAL_RX_W_ROUND_ROBIN_20_RX_W_PRIORITY_SS_160(val) vBIT(val, 3, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_20_RX_W_PRIORITY_SS_161(val) vBIT(val, 11, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_20_RX_W_PRIORITY_SS_162(val) vBIT(val, 19, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_20_RX_W_PRIORITY_SS_163(val) vBIT(val, 27, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_20_RX_W_PRIORITY_SS_164(val) vBIT(val, 35, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_20_RX_W_PRIORITY_SS_165(val) vBIT(val, 43, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_20_RX_W_PRIORITY_SS_166(val) vBIT(val, 51, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_20_RX_W_PRIORITY_SS_167(val) vBIT(val, 59, 5)
/* 0x00c90 */	u64	rx_w_round_robin_21;
#define	VXGE_HAL_RX_W_ROUND_ROBIN_21_RX_W_PRIORITY_SS_168(val) vBIT(val, 3, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_21_RX_W_PRIORITY_SS_169(val) vBIT(val, 11, 5)
#define	VXGE_HAL_RX_W_ROUND_ROBIN_21_RX_W_PRIORITY_SS_170(val) vBIT(val, 19, 5)
/* 0x00c98 */	u64	rx_queue_priority_0;
#define	VXGE_HAL_RX_QUEUE_PRIORITY_0_RX_Q_NUMBER_0(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_RX_QUEUE_PRIORITY_0_RX_Q_NUMBER_1(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_RX_QUEUE_PRIORITY_0_RX_Q_NUMBER_2(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_RX_QUEUE_PRIORITY_0_RX_Q_NUMBER_3(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_RX_QUEUE_PRIORITY_0_RX_Q_NUMBER_4(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_RX_QUEUE_PRIORITY_0_RX_Q_NUMBER_5(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_RX_QUEUE_PRIORITY_0_RX_Q_NUMBER_6(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_RX_QUEUE_PRIORITY_0_RX_Q_NUMBER_7(val)	    vBIT(val, 59, 5)
/* 0x00ca0 */	u64	rx_queue_priority_1;
#define	VXGE_HAL_RX_QUEUE_PRIORITY_1_RX_Q_NUMBER_8(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_RX_QUEUE_PRIORITY_1_RX_Q_NUMBER_9(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_RX_QUEUE_PRIORITY_1_RX_Q_NUMBER_10(val)    vBIT(val, 19, 5)
#define	VXGE_HAL_RX_QUEUE_PRIORITY_1_RX_Q_NUMBER_11(val)    vBIT(val, 27, 5)
#define	VXGE_HAL_RX_QUEUE_PRIORITY_1_RX_Q_NUMBER_12(val)    vBIT(val, 35, 5)
#define	VXGE_HAL_RX_QUEUE_PRIORITY_1_RX_Q_NUMBER_13(val)    vBIT(val, 43, 5)
#define	VXGE_HAL_RX_QUEUE_PRIORITY_1_RX_Q_NUMBER_14(val)    vBIT(val, 51, 5)
#define	VXGE_HAL_RX_QUEUE_PRIORITY_1_RX_Q_NUMBER_15(val)    vBIT(val, 59, 5)
/* 0x00ca8 */	u64	rx_queue_priority_2;
#define	VXGE_HAL_RX_QUEUE_PRIORITY_2_RX_Q_NUMBER_16(val)    vBIT(val, 3, 5)
	u8	unused00cc8[0x00cc8 - 0x00cb0];

/* 0x00cc8 */	u64	replication_queue_priority;
#define	VXGE_HAL_REPLICATION_QUEUE_PRIORITY_REPLICATION_QUEUE_PRIORITY(val)\
							    vBIT(val, 59, 5)
/* 0x00cd0 */	u64	rx_queue_select;
#define	VXGE_HAL_RX_QUEUE_SELECT_NUMBER(n)		    mBIT(n)
#define	VXGE_HAL_RX_QUEUE_SELECT_ENABLE_CODE		    mBIT(15)
#define	VXGE_HAL_RX_QUEUE_SELECT_ENABLE_HIERARCHICAL_PRTY   mBIT(23)
/* 0x00cd8 */	u64	rqa_vpbp_ctrl;
#define	VXGE_HAL_RQA_VPBP_CTRL_WR_XON_DIS		    mBIT(15)
#define	VXGE_HAL_RQA_VPBP_CTRL_ROCRC_DIS		    mBIT(23)
#define	VXGE_HAL_RQA_VPBP_CTRL_TXPE_DIS	mBIT(31)
/* 0x00ce0 */	u64	rx_multi_cast_ctrl;
#define	VXGE_HAL_RX_MULTI_CAST_CTRL_TIME_OUT_DIS	    mBIT(0)
#define	VXGE_HAL_RX_MULTI_CAST_CTRL_FRM_DROP_DIS	    mBIT(1)
#define	VXGE_HAL_RX_MULTI_CAST_CTRL_NO_RXD_TIME_OUT_CNT(val) vBIT(val, 2, 30)
#define	VXGE_HAL_RX_MULTI_CAST_CTRL_TIME_OUT_CNT(val)	    vBIT(val, 32, 32)
/* 0x00ce8 */	u64	wde_prm_ctrl;
#define	VXGE_HAL_WDE_PRM_CTRL_SPAV_THRESHOLD(val)	    vBIT(val, 2, 10)
#define	VXGE_HAL_WDE_PRM_CTRL_SPLIT_THRESHOLD(val)	    vBIT(val, 18, 14)
#define	VXGE_HAL_WDE_PRM_CTRL_SPLIT_ON_1ST_ROW		    mBIT(32)
#define	VXGE_HAL_WDE_PRM_CTRL_SPLIT_ON_ROW_BNDRY	    mBIT(33)
#define	VXGE_HAL_WDE_PRM_CTRL_FB_ROW_SIZE(val)		    vBIT(val, 46, 2)
/* 0x00cf0 */	u64	noa_ctrl;
#define	VXGE_HAL_NOA_CTRL_FRM_PRTY_QUOTA(val)		    vBIT(val, 3, 5)
#define	VXGE_HAL_NOA_CTRL_NON_FRM_PRTY_QUOTA(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_NOA_CTRL_IGNORE_KDFC_IF_STATUS		    mBIT(16)
#define	VXGE_HAL_NOA_CTRL_MAX_JOB_CNT_FOR_WDE0(val)	    vBIT(val, 37, 4)
#define	VXGE_HAL_NOA_CTRL_MAX_JOB_CNT_FOR_WDE1(val)	    vBIT(val, 45, 4)
#define	VXGE_HAL_NOA_CTRL_MAX_JOB_CNT_FOR_WDE2(val)	    vBIT(val, 53, 4)
#define	VXGE_HAL_NOA_CTRL_MAX_JOB_CNT_FOR_WDE3(val)	    vBIT(val, 60, 4)
/* 0x00cf8 */	u64	phase_cfg;
#define	VXGE_HAL_PHASE_CFG_QCC_WR_PHASE_EN		    mBIT(0)
#define	VXGE_HAL_PHASE_CFG_QCC_RD_PHASE_EN		    mBIT(3)
#define	VXGE_HAL_PHASE_CFG_IMMM_WR_PHASE_EN		    mBIT(7)
#define	VXGE_HAL_PHASE_CFG_IMMM_RD_PHASE_EN		    mBIT(11)
#define	VXGE_HAL_PHASE_CFG_UMQM_WR_PHASE_EN		    mBIT(15)
#define	VXGE_HAL_PHASE_CFG_UMQM_RD_PHASE_EN		    mBIT(19)
#define	VXGE_HAL_PHASE_CFG_RCBM_WR_PHASE_EN		    mBIT(23)
#define	VXGE_HAL_PHASE_CFG_RCBM_RD_PHASE_EN		    mBIT(27)
#define	VXGE_HAL_PHASE_CFG_RXD_RC_WR_PHASE_EN		    mBIT(31)
#define	VXGE_HAL_PHASE_CFG_RXD_RC_RD_PHASE_EN		    mBIT(35)
#define	VXGE_HAL_PHASE_CFG_RXD_RHS_WR_PHASE_EN		    mBIT(39)
#define	VXGE_HAL_PHASE_CFG_RXD_RHS_RD_PHASE_EN		    mBIT(43)
/* 0x00d00 */	u64	rcq_bypq_cfg;
#define	VXGE_HAL_RCQ_BYPQ_CFG_OVERFLOW_THRESHOLD(val)	    vBIT(val, 10, 22)
#define	VXGE_HAL_RCQ_BYPQ_CFG_BYP_ON_THRESHOLD(val)	    vBIT(val, 39, 9)
#define	VXGE_HAL_RCQ_BYPQ_CFG_BYP_OFF_THRESHOLD(val)	    vBIT(val, 55, 9)
	u8	unused00e00[0x00e00 - 0x00d08];

/* 0x00e00 */	u64	doorbell_int_status;
#define	VXGE_HAL_DOORBELL_INT_STATUS_KDFC_ERR_REG_TXDMA_KDFC_INT mBIT(7)
#define	VXGE_HAL_DOORBELL_INT_STATUS_USDC_ERR_REG_TXDMA_USDC_INT mBIT(15)
/* 0x00e08 */	u64	doorbell_int_mask;
/* 0x00e10 */	u64	kdfc_err_reg;
#define	VXGE_HAL_KDFC_ERR_REG_KDFC_KDFC_ECC_SG_ERR	    mBIT(7)
#define	VXGE_HAL_KDFC_ERR_REG_KDFC_KDFC_ECC_DB_ERR	    mBIT(15)
#define	VXGE_HAL_KDFC_ERR_REG_KDFC_KDFC_SM_ERR_ALARM	    mBIT(23)
#define	VXGE_HAL_KDFC_ERR_REG_KDFC_KDFC_MISC_ERR_1	    mBIT(32)
#define	VXGE_HAL_KDFC_ERR_REG_KDFC_KDFC_PCIX_ERR	    mBIT(39)
/* 0x00e18 */	u64	kdfc_err_mask;
/* 0x00e20 */	u64	kdfc_err_reg_alarm;
#define	VXGE_HAL_KDFC_ERR_REG_ALARM_KDFC_KDFC_ECC_SG_ERR    mBIT(7)
#define	VXGE_HAL_KDFC_ERR_REG_ALARM_KDFC_KDFC_ECC_DB_ERR    mBIT(15)
#define	VXGE_HAL_KDFC_ERR_REG_ALARM_KDFC_KDFC_SM_ERR_ALARM  mBIT(23)
#define	VXGE_HAL_KDFC_ERR_REG_ALARM_KDFC_KDFC_MISC_ERR_1    mBIT(32)
#define	VXGE_HAL_KDFC_ERR_REG_ALARM_KDFC_KDFC_PCIX_ERR	    mBIT(39)
/* 0x00e28 */	u64	usdc_err_reg;
#define	VXGE_HAL_USDC_ERR_REG_USDC_FIFO_ECC_SG_ERR	    mBIT(4)
#define	VXGE_HAL_USDC_ERR_REG_USDC_WA_ECC_SG_ERR	    mBIT(5)
#define	VXGE_HAL_USDC_ERR_REG_USDC_CA_ECC_SG_ERR	    mBIT(6)
#define	VXGE_HAL_USDC_ERR_REG_USDC_SA_ECC_SG_ERR	    mBIT(7)
#define	VXGE_HAL_USDC_ERR_REG_USDC_FIFO_ECC_DB_ERR	    mBIT(12)
#define	VXGE_HAL_USDC_ERR_REG_USDC_WA_ECC_DB_ERR	    mBIT(13)
#define	VXGE_HAL_USDC_ERR_REG_USDC_CA_ECC_DB_ERR	    mBIT(14)
#define	VXGE_HAL_USDC_ERR_REG_USDC_SA_ECC_DB_ERR	    mBIT(15)
#define	VXGE_HAL_USDC_ERR_REG_USDC_USDC_SM_ERR_ALARM	    mBIT(23)
#define	VXGE_HAL_USDC_ERR_REG_USDC_USDC_MISC_ERR_0	    mBIT(30)
#define	VXGE_HAL_USDC_ERR_REG_USDC_USDC_MISC_ERR_1	    mBIT(31)
#define	VXGE_HAL_USDC_ERR_REG_USDC_USDC_PCI_ERR		    mBIT(39)
/* 0x00e30 */	u64	usdc_err_mask;
/* 0x00e38 */	u64	usdc_err_reg_alarm;
#define	VXGE_HAL_USDC_ERR_REG_ALARM_USDC_FIFO_ECC_SG_ERR    mBIT(4)
#define	VXGE_HAL_USDC_ERR_REG_ALARM_USDC_WA_ECC_SG_ERR	    mBIT(5)
#define	VXGE_HAL_USDC_ERR_REG_ALARM_USDC_CA_ECC_SG_ERR	    mBIT(6)
#define	VXGE_HAL_USDC_ERR_REG_ALARM_USDC_SA_ECC_SG_ERR	    mBIT(7)
#define	VXGE_HAL_USDC_ERR_REG_ALARM_USDC_FIFO_ECC_DB_ERR    mBIT(12)
#define	VXGE_HAL_USDC_ERR_REG_ALARM_USDC_WA_ECC_DB_ERR	    mBIT(13)
#define	VXGE_HAL_USDC_ERR_REG_ALARM_USDC_CA_ECC_DB_ERR	    mBIT(14)
#define	VXGE_HAL_USDC_ERR_REG_ALARM_USDC_SA_ECC_DB_ERR	    mBIT(15)
#define	VXGE_HAL_USDC_ERR_REG_ALARM_USDC_USDC_SM_ERR_ALARM  mBIT(23)
#define	VXGE_HAL_USDC_ERR_REG_ALARM_USDC_USDC_MISC_ERR_0    mBIT(30)
#define	VXGE_HAL_USDC_ERR_REG_ALARM_USDC_USDC_MISC_ERR_1    mBIT(31)
#define	VXGE_HAL_USDC_ERR_REG_ALARM_USDC_USDC_PCI_ERR	    mBIT(39)
/* 0x00e40 */	u64	kdfc_vp_partition_0;
#define	VXGE_HAL_KDFC_VP_PARTITION_0_ENABLE		    mBIT(0)
#define	VXGE_HAL_KDFC_VP_PARTITION_0_NUMBER_0(val)	    vBIT(val, 5, 3)
#define	VXGE_HAL_KDFC_VP_PARTITION_0_LENGTH_0(val)	    vBIT(val, 17, 15)
#define	VXGE_HAL_KDFC_VP_PARTITION_0_NUMBER_1(val)	    vBIT(val, 37, 3)
#define	VXGE_HAL_KDFC_VP_PARTITION_0_LENGTH_1(val)	    vBIT(val, 49, 15)
/* 0x00e48 */	u64	kdfc_vp_partition_1;
#define	VXGE_HAL_KDFC_VP_PARTITION_1_NUMBER_2(val)	    vBIT(val, 5, 3)
#define	VXGE_HAL_KDFC_VP_PARTITION_1_LENGTH_2(val)	    vBIT(val, 17, 15)
#define	VXGE_HAL_KDFC_VP_PARTITION_1_NUMBER_3(val)	    vBIT(val, 37, 3)
#define	VXGE_HAL_KDFC_VP_PARTITION_1_LENGTH_3(val)	    vBIT(val, 49, 15)
/* 0x00e50 */	u64	kdfc_vp_partition_2;
#define	VXGE_HAL_KDFC_VP_PARTITION_2_NUMBER_4(val)	    vBIT(val, 5, 3)
#define	VXGE_HAL_KDFC_VP_PARTITION_2_LENGTH_4(val)	    vBIT(val, 17, 15)
#define	VXGE_HAL_KDFC_VP_PARTITION_2_NUMBER_5(val)	    vBIT(val, 37, 3)
#define	VXGE_HAL_KDFC_VP_PARTITION_2_LENGTH_5(val)	    vBIT(val, 49, 15)
/* 0x00e58 */	u64	kdfc_vp_partition_3;
#define	VXGE_HAL_KDFC_VP_PARTITION_3_NUMBER_6(val)	    vBIT(val, 5, 3)
#define	VXGE_HAL_KDFC_VP_PARTITION_3_LENGTH_6(val)	    vBIT(val, 17, 15)
#define	VXGE_HAL_KDFC_VP_PARTITION_3_NUMBER_7(val)	    vBIT(val, 37, 3)
#define	VXGE_HAL_KDFC_VP_PARTITION_3_LENGTH_7(val)	    vBIT(val, 49, 15)
/* 0x00e60 */	u64	kdfc_vp_partition_4;
#define	VXGE_HAL_KDFC_VP_PARTITION_4_LENGTH_8(val)	    vBIT(val, 17, 15)
#define	VXGE_HAL_KDFC_VP_PARTITION_4_LENGTH_9(val)	    vBIT(val, 49, 15)
/* 0x00e68 */	u64	kdfc_vp_partition_5;
#define	VXGE_HAL_KDFC_VP_PARTITION_5_LENGTH_10(val)	    vBIT(val, 17, 15)
#define	VXGE_HAL_KDFC_VP_PARTITION_5_LENGTH_11(val)	    vBIT(val, 49, 15)
/* 0x00e70 */	u64	kdfc_vp_partition_6;
#define	VXGE_HAL_KDFC_VP_PARTITION_6_LENGTH_12(val)	    vBIT(val, 17, 15)
#define	VXGE_HAL_KDFC_VP_PARTITION_6_LENGTH_13(val)	    vBIT(val, 49, 15)
/* 0x00e78 */	u64	kdfc_vp_partition_7;
#define	VXGE_HAL_KDFC_VP_PARTITION_7_LENGTH_14(val)	    vBIT(val, 17, 15)
#define	VXGE_HAL_KDFC_VP_PARTITION_7_LENGTH_15(val)	    vBIT(val, 49, 15)
/* 0x00e80 */	u64	kdfc_vp_partition_8;
#define	VXGE_HAL_KDFC_VP_PARTITION_8_LENGTH_16(val)	    vBIT(val, 17, 15)
/* 0x00e88 */	u64	kdfc_w_round_robin_0;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_0_NUMBER_0(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_0_NUMBER_1(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_0_NUMBER_2(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_0_NUMBER_3(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_0_NUMBER_4(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_0_NUMBER_5(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_0_NUMBER_6(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_0_NUMBER_7(val)	    vBIT(val, 59, 5)
/* 0x00e90 */	u64	kdfc_w_round_robin_1;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_1_NUMBER_8(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_1_NUMBER_9(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_1_NUMBER_10(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_1_NUMBER_11(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_1_NUMBER_12(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_1_NUMBER_13(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_1_NUMBER_14(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_1_NUMBER_15(val)	    vBIT(val, 59, 5)
/* 0x00e98 */	u64	kdfc_w_round_robin_2;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_2_NUMBER_16(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_2_NUMBER_17(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_2_NUMBER_18(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_2_NUMBER_19(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_2_NUMBER_20(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_2_NUMBER_21(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_2_NUMBER_22(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_2_NUMBER_23(val)	    vBIT(val, 59, 5)
/* 0x00ea0 */	u64	kdfc_w_round_robin_3;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_3_NUMBER_24(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_3_NUMBER_25(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_3_NUMBER_26(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_3_NUMBER_27(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_3_NUMBER_28(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_3_NUMBER_29(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_3_NUMBER_30(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_3_NUMBER_31(val)	    vBIT(val, 59, 5)
/* 0x00ea8 */	u64	kdfc_w_round_robin_4;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_4_NUMBER_32(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_4_NUMBER_33(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_4_NUMBER_34(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_4_NUMBER_35(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_4_NUMBER_36(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_4_NUMBER_37(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_4_NUMBER_38(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_4_NUMBER_39(val)	    vBIT(val, 59, 5)
/* 0x00eb0 */	u64	kdfc_w_round_robin_5;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_5_NUMBER_40(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_5_NUMBER_41(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_5_NUMBER_42(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_5_NUMBER_43(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_5_NUMBER_44(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_5_NUMBER_45(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_5_NUMBER_46(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_5_NUMBER_47(val)	    vBIT(val, 59, 5)
/* 0x00eb8 */	u64	kdfc_w_round_robin_6;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_6_NUMBER_48(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_6_NUMBER_49(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_6_NUMBER_50(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_6_NUMBER_51(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_6_NUMBER_52(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_6_NUMBER_53(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_6_NUMBER_54(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_6_NUMBER_55(val)	    vBIT(val, 59, 5)
/* 0x00ec0 */	u64	kdfc_w_round_robin_7;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_7_NUMBER_56(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_7_NUMBER_57(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_7_NUMBER_58(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_7_NUMBER_59(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_7_NUMBER_60(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_7_NUMBER_61(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_7_NUMBER_62(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_7_NUMBER_63(val)	    vBIT(val, 59, 5)
/* 0x00ec8 */	u64	kdfc_w_round_robin_8;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_8_NUMBER_64(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_8_NUMBER_65(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_8_NUMBER_66(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_8_NUMBER_67(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_8_NUMBER_68(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_8_NUMBER_69(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_8_NUMBER_70(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_8_NUMBER_71(val)	    vBIT(val, 59, 5)
/* 0x00ed0 */	u64	kdfc_w_round_robin_9;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_9_NUMBER_72(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_9_NUMBER_73(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_9_NUMBER_74(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_9_NUMBER_75(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_9_NUMBER_76(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_9_NUMBER_77(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_9_NUMBER_78(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_9_NUMBER_79(val)	    vBIT(val, 59, 5)
/* 0x00ed8 */	u64	kdfc_w_round_robin_10;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_10_NUMBER_80(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_10_NUMBER_81(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_10_NUMBER_82(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_10_NUMBER_83(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_10_NUMBER_84(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_10_NUMBER_85(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_10_NUMBER_86(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_10_NUMBER_87(val)	    vBIT(val, 59, 5)
/* 0x00ee0 */	u64	kdfc_w_round_robin_11;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_11_NUMBER_88(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_11_NUMBER_89(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_11_NUMBER_90(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_11_NUMBER_91(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_11_NUMBER_92(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_11_NUMBER_93(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_11_NUMBER_94(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_11_NUMBER_95(val)	    vBIT(val, 59, 5)
/* 0x00ee8 */	u64	kdfc_w_round_robin_12;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_12_NUMBER_96(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_12_NUMBER_97(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_12_NUMBER_98(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_12_NUMBER_99(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_12_NUMBER_100(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_12_NUMBER_101(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_12_NUMBER_102(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_12_NUMBER_103(val)	    vBIT(val, 59, 5)
/* 0x00ef0 */	u64	kdfc_w_round_robin_13;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_13_NUMBER_104(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_13_NUMBER_105(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_13_NUMBER_106(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_13_NUMBER_107(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_13_NUMBER_108(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_13_NUMBER_109(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_13_NUMBER_110(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_13_NUMBER_111(val)	    vBIT(val, 59, 5)
/* 0x00ef8 */	u64	kdfc_w_round_robin_14;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_14_NUMBER_112(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_14_NUMBER_113(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_14_NUMBER_114(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_14_NUMBER_115(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_14_NUMBER_116(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_14_NUMBER_117(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_14_NUMBER_118(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_14_NUMBER_119(val)	    vBIT(val, 59, 5)
/* 0x00f00 */	u64	kdfc_w_round_robin_15;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_15_NUMBER_120(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_15_NUMBER_121(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_15_NUMBER_122(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_15_NUMBER_123(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_15_NUMBER_124(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_15_NUMBER_125(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_15_NUMBER_126(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_15_NUMBER_127(val)	    vBIT(val, 59, 5)
/* 0x00f08 */	u64	kdfc_w_round_robin_16;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_16_NUMBER_128(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_16_NUMBER_129(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_16_NUMBER_130(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_16_NUMBER_131(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_16_NUMBER_132(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_16_NUMBER_133(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_16_NUMBER_134(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_16_NUMBER_135(val)	    vBIT(val, 59, 5)
/* 0x00f10 */	u64	kdfc_w_round_robin_17;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_17_NUMBER_136(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_17_NUMBER_137(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_17_NUMBER_138(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_17_NUMBER_139(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_17_NUMBER_140(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_17_NUMBER_141(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_17_NUMBER_142(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_17_NUMBER_143(val)	    vBIT(val, 59, 5)
/* 0x00f18 */	u64	kdfc_w_round_robin_18;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_18_NUMBER_144(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_18_NUMBER_145(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_18_NUMBER_146(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_18_NUMBER_147(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_18_NUMBER_148(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_18_NUMBER_149(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_18_NUMBER_150(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_18_NUMBER_151(val)	    vBIT(val, 59, 5)
/* 0x00f20 */	u64	kdfc_w_round_robin_19;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_19_NUMBER_152(val)	    vBIT(val, 3, 5)
/* 0x00f28 */	u64	kdfc_w_round_robin_20;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_20_NUMBER_0(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_20_NUMBER_1(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_20_NUMBER_2(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_20_NUMBER_3(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_20_NUMBER_4(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_20_NUMBER_5(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_20_NUMBER_6(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_20_NUMBER_7(val)	    vBIT(val, 59, 5)
/* 0x00f30 */	u64	kdfc_w_round_robin_21;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_21_NUMBER_8(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_21_NUMBER_9(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_21_NUMBER_10(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_21_NUMBER_11(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_21_NUMBER_12(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_21_NUMBER_13(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_21_NUMBER_14(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_21_NUMBER_15(val)	    vBIT(val, 59, 5)
/* 0x00f38 */	u64	kdfc_w_round_robin_22;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_22_NUMBER_16(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_22_NUMBER_17(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_22_NUMBER_18(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_22_NUMBER_19(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_22_NUMBER_20(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_22_NUMBER_21(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_22_NUMBER_22(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_22_NUMBER_23(val)	    vBIT(val, 59, 5)
/* 0x00f40 */	u64	kdfc_w_round_robin_23;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_23_NUMBER_24(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_23_NUMBER_25(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_23_NUMBER_26(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_23_NUMBER_27(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_23_NUMBER_28(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_23_NUMBER_29(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_23_NUMBER_30(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_23_NUMBER_31(val)	    vBIT(val, 59, 5)
/* 0x00f48 */	u64	kdfc_w_round_robin_24;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_24_NUMBER_32(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_24_NUMBER_33(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_24_NUMBER_34(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_24_NUMBER_35(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_24_NUMBER_36(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_24_NUMBER_37(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_24_NUMBER_38(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_24_NUMBER_39(val)	    vBIT(val, 59, 5)
/* 0x00f50 */	u64	kdfc_w_round_robin_25;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_25_NUMBER_40(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_25_NUMBER_41(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_25_NUMBER_42(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_25_NUMBER_43(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_25_NUMBER_44(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_25_NUMBER_45(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_25_NUMBER_46(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_25_NUMBER_47(val)	    vBIT(val, 59, 5)
/* 0x00f58 */	u64	kdfc_w_round_robin_26;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_26_NUMBER_48(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_26_NUMBER_49(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_26_NUMBER_50(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_26_NUMBER_51(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_26_NUMBER_52(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_26_NUMBER_53(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_26_NUMBER_54(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_26_NUMBER_55(val)	    vBIT(val, 59, 5)
/* 0x00f60 */	u64	kdfc_w_round_robin_27;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_27_NUMBER_56(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_27_NUMBER_57(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_27_NUMBER_58(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_27_NUMBER_59(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_27_NUMBER_60(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_27_NUMBER_61(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_27_NUMBER_62(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_27_NUMBER_63(val)	    vBIT(val, 59, 5)
/* 0x00f68 */	u64	kdfc_w_round_robin_28;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_28_NUMBER_64(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_28_NUMBER_65(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_28_NUMBER_66(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_28_NUMBER_67(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_28_NUMBER_68(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_28_NUMBER_69(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_28_NUMBER_70(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_28_NUMBER_71(val)	    vBIT(val, 59, 5)
/* 0x00f70 */	u64	kdfc_w_round_robin_29;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_29_NUMBER_72(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_29_NUMBER_73(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_29_NUMBER_74(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_29_NUMBER_75(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_29_NUMBER_76(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_29_NUMBER_77(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_29_NUMBER_78(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_29_NUMBER_79(val)	    vBIT(val, 59, 5)
/* 0x00f78 */	u64	kdfc_w_round_robin_30;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_30_NUMBER_80(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_30_NUMBER_81(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_30_NUMBER_82(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_30_NUMBER_83(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_30_NUMBER_84(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_30_NUMBER_85(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_30_NUMBER_86(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_30_NUMBER_87(val)	    vBIT(val, 59, 5)
/* 0x00f80 */	u64	kdfc_w_round_robin_31;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_31_NUMBER_88(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_31_NUMBER_89(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_31_NUMBER_90(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_31_NUMBER_91(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_31_NUMBER_92(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_31_NUMBER_93(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_31_NUMBER_94(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_31_NUMBER_95(val)	    vBIT(val, 59, 5)
/* 0x00f88 */	u64	kdfc_w_round_robin_32;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_32_NUMBER_96(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_32_NUMBER_97(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_32_NUMBER_98(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_32_NUMBER_99(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_32_NUMBER_100(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_32_NUMBER_101(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_32_NUMBER_102(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_32_NUMBER_103(val)	    vBIT(val, 59, 5)
/* 0x00f90 */	u64	kdfc_w_round_robin_33;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_33_NUMBER_104(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_33_NUMBER_105(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_33_NUMBER_106(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_33_NUMBER_107(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_33_NUMBER_108(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_33_NUMBER_109(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_33_NUMBER_110(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_33_NUMBER_111(val)	    vBIT(val, 59, 5)
/* 0x00f98 */	u64	kdfc_w_round_robin_34;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_34_NUMBER_112(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_34_NUMBER_113(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_34_NUMBER_114(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_34_NUMBER_115(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_34_NUMBER_116(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_34_NUMBER_117(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_34_NUMBER_118(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_34_NUMBER_119(val)	    vBIT(val, 59, 5)
/* 0x00fa0 */	u64	kdfc_w_round_robin_35;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_35_NUMBER_120(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_35_NUMBER_121(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_35_NUMBER_122(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_35_NUMBER_123(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_35_NUMBER_124(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_35_NUMBER_125(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_35_NUMBER_126(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_35_NUMBER_127(val)	    vBIT(val, 59, 5)
/* 0x00fa8 */	u64	kdfc_w_round_robin_36;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_36_NUMBER_128(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_36_NUMBER_129(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_36_NUMBER_130(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_36_NUMBER_131(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_36_NUMBER_132(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_36_NUMBER_133(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_36_NUMBER_134(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_36_NUMBER_135(val)	    vBIT(val, 59, 5)
/* 0x00fb0 */	u64	kdfc_w_round_robin_37;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_37_NUMBER_136(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_37_NUMBER_137(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_37_NUMBER_138(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_37_NUMBER_139(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_37_NUMBER_140(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_37_NUMBER_141(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_37_NUMBER_142(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_37_NUMBER_143(val)	    vBIT(val, 59, 5)
/* 0x00fb8 */	u64	kdfc_w_round_robin_38;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_38_NUMBER_144(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_38_NUMBER_145(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_38_NUMBER_146(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_38_NUMBER_147(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_38_NUMBER_148(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_38_NUMBER_149(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_38_NUMBER_150(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_38_NUMBER_151(val)	    vBIT(val, 59, 5)
/* 0x00fc0 */	u64	kdfc_w_round_robin_39;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_39_NUMBER_152(val)	    vBIT(val, 3, 5)
/* 0x00fc8 */	u64	kdfc_w_round_robin_40;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_40_NUMBER_0(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_40_NUMBER_1(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_40_NUMBER_2(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_40_NUMBER_3(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_40_NUMBER_4(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_40_NUMBER_5(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_40_NUMBER_6(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_40_NUMBER_7(val)	    vBIT(val, 59, 5)
/* 0x00fd0 */	u64	kdfc_w_round_robin_41;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_41_NUMBER_8(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_41_NUMBER_9(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_41_NUMBER_10(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_41_NUMBER_11(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_41_NUMBER_12(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_41_NUMBER_13(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_41_NUMBER_14(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_41_NUMBER_15(val)	    vBIT(val, 59, 5)
/* 0x00fd8 */	u64	kdfc_w_round_robin_42;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_42_NUMBER_16(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_42_NUMBER_17(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_42_NUMBER_18(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_42_NUMBER_19(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_42_NUMBER_20(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_42_NUMBER_21(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_42_NUMBER_22(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_42_NUMBER_23(val)	    vBIT(val, 59, 5)
/* 0x00fe0 */	u64	kdfc_w_round_robin_43;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_43_NUMBER_24(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_43_NUMBER_25(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_43_NUMBER_26(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_43_NUMBER_27(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_43_NUMBER_28(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_43_NUMBER_29(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_43_NUMBER_30(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_43_NUMBER_31(val)	    vBIT(val, 59, 5)
/* 0x00fe8 */	u64	kdfc_w_round_robin_44;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_44_NUMBER_32(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_44_NUMBER_33(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_44_NUMBER_34(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_44_NUMBER_35(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_44_NUMBER_36(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_44_NUMBER_37(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_44_NUMBER_38(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_44_NUMBER_39(val)	    vBIT(val, 59, 5)
/* 0x00ff0 */	u64	kdfc_w_round_robin_45;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_45_NUMBER_40(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_45_NUMBER_41(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_45_NUMBER_42(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_45_NUMBER_43(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_45_NUMBER_44(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_45_NUMBER_45(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_45_NUMBER_46(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_45_NUMBER_47(val)	    vBIT(val, 59, 5)
/* 0x00ff8 */	u64	kdfc_w_round_robin_46;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_46_NUMBER_48(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_46_NUMBER_49(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_46_NUMBER_50(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_46_NUMBER_51(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_46_NUMBER_52(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_46_NUMBER_53(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_46_NUMBER_54(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_46_NUMBER_55(val)	    vBIT(val, 59, 5)
/* 0x01000 */	u64	kdfc_w_round_robin_47;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_47_NUMBER_56(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_47_NUMBER_57(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_47_NUMBER_58(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_47_NUMBER_59(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_47_NUMBER_60(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_47_NUMBER_61(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_47_NUMBER_62(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_47_NUMBER_63(val)	    vBIT(val, 59, 5)
/* 0x01008 */	u64	kdfc_w_round_robin_48;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_48_NUMBER_64(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_48_NUMBER_65(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_48_NUMBER_66(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_48_NUMBER_67(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_48_NUMBER_68(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_48_NUMBER_69(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_48_NUMBER_70(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_48_NUMBER_71(val)	    vBIT(val, 59, 5)
/* 0x01010 */	u64	kdfc_w_round_robin_49;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_49_NUMBER_72(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_49_NUMBER_73(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_49_NUMBER_74(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_49_NUMBER_75(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_49_NUMBER_76(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_49_NUMBER_77(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_49_NUMBER_78(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_49_NUMBER_79(val)	    vBIT(val, 59, 5)
/* 0x01018 */	u64	kdfc_w_round_robin_50;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_50_NUMBER_80(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_50_NUMBER_81(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_50_NUMBER_82(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_50_NUMBER_83(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_50_NUMBER_84(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_50_NUMBER_85(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_50_NUMBER_86(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_50_NUMBER_87(val)	    vBIT(val, 59, 5)
/* 0x01020 */	u64	kdfc_w_round_robin_51;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_51_NUMBER_88(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_51_NUMBER_89(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_51_NUMBER_90(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_51_NUMBER_91(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_51_NUMBER_92(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_51_NUMBER_93(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_51_NUMBER_94(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_51_NUMBER_95(val)	    vBIT(val, 59, 5)
/* 0x01028 */	u64	kdfc_w_round_robin_52;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_52_NUMBER_96(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_52_NUMBER_97(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_52_NUMBER_98(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_52_NUMBER_99(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_52_NUMBER_100(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_52_NUMBER_101(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_52_NUMBER_102(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_52_NUMBER_103(val)	    vBIT(val, 59, 5)
/* 0x01030 */	u64	kdfc_w_round_robin_53;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_53_NUMBER_104(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_53_NUMBER_105(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_53_NUMBER_106(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_53_NUMBER_107(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_53_NUMBER_108(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_53_NUMBER_109(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_53_NUMBER_110(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_53_NUMBER_111(val)	    vBIT(val, 59, 5)
/* 0x01038 */	u64	kdfc_w_round_robin_54;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_54_NUMBER_112(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_54_NUMBER_113(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_54_NUMBER_114(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_54_NUMBER_115(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_54_NUMBER_116(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_54_NUMBER_117(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_54_NUMBER_118(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_54_NUMBER_119(val)	    vBIT(val, 59, 5)
/* 0x01040 */	u64	kdfc_w_round_robin_55;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_55_NUMBER_120(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_55_NUMBER_121(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_55_NUMBER_122(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_55_NUMBER_123(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_55_NUMBER_124(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_55_NUMBER_125(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_55_NUMBER_126(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_55_NUMBER_127(val)	    vBIT(val, 59, 5)
/* 0x01048 */	u64	kdfc_w_round_robin_56;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_56_NUMBER_128(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_56_NUMBER_129(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_56_NUMBER_130(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_56_NUMBER_131(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_56_NUMBER_132(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_56_NUMBER_133(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_56_NUMBER_134(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_56_NUMBER_135(val)	    vBIT(val, 59, 5)
/* 0x01050 */	u64	kdfc_w_round_robin_57;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_57_NUMBER_136(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_57_NUMBER_137(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_57_NUMBER_138(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_57_NUMBER_139(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_57_NUMBER_140(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_57_NUMBER_141(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_57_NUMBER_142(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_57_NUMBER_143(val)	    vBIT(val, 59, 5)
/* 0x01058 */	u64	kdfc_w_round_robin_58;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_58_NUMBER_144(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_58_NUMBER_145(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_58_NUMBER_146(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_58_NUMBER_147(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_58_NUMBER_148(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_58_NUMBER_149(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_58_NUMBER_150(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_58_NUMBER_151(val)	    vBIT(val, 59, 5)
/* 0x01060 */	u64	kdfc_w_round_robin_59;
#define	VXGE_HAL_KDFC_W_ROUND_ROBIN_59_NUMBER_152(val)	    vBIT(val, 3, 5)
/* 0x01068 */	u64	kdfc_entry_type_sel_0;
#define	VXGE_HAL_KDFC_ENTRY_TYPE_SEL_0_NUMBER_0(val)	    vBIT(val, 6, 2)
#define	VXGE_HAL_KDFC_ENTRY_TYPE_SEL_0_NUMBER_1(val)	    vBIT(val, 14, 2)
#define	VXGE_HAL_KDFC_ENTRY_TYPE_SEL_0_NUMBER_2(val)	    vBIT(val, 22, 2)
#define	VXGE_HAL_KDFC_ENTRY_TYPE_SEL_0_NUMBER_3(val)	    vBIT(val, 30, 2)
#define	VXGE_HAL_KDFC_ENTRY_TYPE_SEL_0_NUMBER_4(val)	    vBIT(val, 38, 2)
#define	VXGE_HAL_KDFC_ENTRY_TYPE_SEL_0_NUMBER_5(val)	    vBIT(val, 46, 2)
#define	VXGE_HAL_KDFC_ENTRY_TYPE_SEL_0_NUMBER_6(val)	    vBIT(val, 54, 2)
#define	VXGE_HAL_KDFC_ENTRY_TYPE_SEL_0_NUMBER_7(val)	    vBIT(val, 62, 2)
/* 0x01070 */	u64	kdfc_entry_type_sel_1;
#define	VXGE_HAL_KDFC_ENTRY_TYPE_SEL_1_NUMBER_8(val)	    vBIT(val, 6, 2)
/* 0x01078 */	u64	kdfc_fifo_0_ctrl;
#define	VXGE_HAL_KDFC_FIFO_0_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x01080 */	u64	kdfc_fifo_1_ctrl;
#define	VXGE_HAL_KDFC_FIFO_1_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x01088 */	u64	kdfc_fifo_2_ctrl;
#define	VXGE_HAL_KDFC_FIFO_2_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x01090 */	u64	kdfc_fifo_3_ctrl;
#define	VXGE_HAL_KDFC_FIFO_3_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x01098 */	u64	kdfc_fifo_4_ctrl;
#define	VXGE_HAL_KDFC_FIFO_4_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x010a0 */	u64	kdfc_fifo_5_ctrl;
#define	VXGE_HAL_KDFC_FIFO_5_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x010a8 */	u64	kdfc_fifo_6_ctrl;
#define	VXGE_HAL_KDFC_FIFO_6_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x010b0 */	u64	kdfc_fifo_7_ctrl;
#define	VXGE_HAL_KDFC_FIFO_7_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x010b8 */	u64	kdfc_fifo_8_ctrl;
#define	VXGE_HAL_KDFC_FIFO_8_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x010c0 */	u64	kdfc_fifo_9_ctrl;
#define	VXGE_HAL_KDFC_FIFO_9_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x010c8 */	u64	kdfc_fifo_10_ctrl;
#define	VXGE_HAL_KDFC_FIFO_10_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x010d0 */	u64	kdfc_fifo_11_ctrl;
#define	VXGE_HAL_KDFC_FIFO_11_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x010d8 */	u64	kdfc_fifo_12_ctrl;
#define	VXGE_HAL_KDFC_FIFO_12_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x010e0 */	u64	kdfc_fifo_13_ctrl;
#define	VXGE_HAL_KDFC_FIFO_13_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x010e8 */	u64	kdfc_fifo_14_ctrl;
#define	VXGE_HAL_KDFC_FIFO_14_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x010f0 */	u64	kdfc_fifo_15_ctrl;
#define	VXGE_HAL_KDFC_FIFO_15_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x010f8 */	u64	kdfc_fifo_16_ctrl;
#define	VXGE_HAL_KDFC_FIFO_16_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x01100 */	u64	kdfc_fifo_17_ctrl;
#define	VXGE_HAL_KDFC_FIFO_17_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x01108 */	u64	kdfc_fifo_18_ctrl;
#define	VXGE_HAL_KDFC_FIFO_18_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x01110 */	u64	kdfc_fifo_19_ctrl;
#define	VXGE_HAL_KDFC_FIFO_19_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x01118 */	u64	kdfc_fifo_20_ctrl;
#define	VXGE_HAL_KDFC_FIFO_20_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x01120 */	u64	kdfc_fifo_21_ctrl;
#define	VXGE_HAL_KDFC_FIFO_21_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x01128 */	u64	kdfc_fifo_22_ctrl;
#define	VXGE_HAL_KDFC_FIFO_22_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x01130 */	u64	kdfc_fifo_23_ctrl;
#define	VXGE_HAL_KDFC_FIFO_23_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x01138 */	u64	kdfc_fifo_24_ctrl;
#define	VXGE_HAL_KDFC_FIFO_24_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x01140 */	u64	kdfc_fifo_25_ctrl;
#define	VXGE_HAL_KDFC_FIFO_25_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x01148 */	u64	kdfc_fifo_26_ctrl;
#define	VXGE_HAL_KDFC_FIFO_26_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x01150 */	u64	kdfc_fifo_27_ctrl;
#define	VXGE_HAL_KDFC_FIFO_27_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x01158 */	u64	kdfc_fifo_28_ctrl;
#define	VXGE_HAL_KDFC_FIFO_28_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x01160 */	u64	kdfc_fifo_29_ctrl;
#define	VXGE_HAL_KDFC_FIFO_29_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x01168 */	u64	kdfc_fifo_30_ctrl;
#define	VXGE_HAL_KDFC_FIFO_30_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x01170 */	u64	kdfc_fifo_31_ctrl;
#define	VXGE_HAL_KDFC_FIFO_31_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x01178 */	u64	kdfc_fifo_32_ctrl;
#define	VXGE_HAL_KDFC_FIFO_32_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x01180 */	u64	kdfc_fifo_33_ctrl;
#define	VXGE_HAL_KDFC_FIFO_33_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x01188 */	u64	kdfc_fifo_34_ctrl;
#define	VXGE_HAL_KDFC_FIFO_34_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x01190 */	u64	kdfc_fifo_35_ctrl;
#define	VXGE_HAL_KDFC_FIFO_35_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x01198 */	u64	kdfc_fifo_36_ctrl;
#define	VXGE_HAL_KDFC_FIFO_36_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x011a0 */	u64	kdfc_fifo_37_ctrl;
#define	VXGE_HAL_KDFC_FIFO_37_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x011a8 */	u64	kdfc_fifo_38_ctrl;
#define	VXGE_HAL_KDFC_FIFO_38_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x011b0 */	u64	kdfc_fifo_39_ctrl;
#define	VXGE_HAL_KDFC_FIFO_39_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x011b8 */	u64	kdfc_fifo_40_ctrl;
#define	VXGE_HAL_KDFC_FIFO_40_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x011c0 */	u64	kdfc_fifo_41_ctrl;
#define	VXGE_HAL_KDFC_FIFO_41_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x011c8 */	u64	kdfc_fifo_42_ctrl;
#define	VXGE_HAL_KDFC_FIFO_42_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x011d0 */	u64	kdfc_fifo_43_ctrl;
#define	VXGE_HAL_KDFC_FIFO_43_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x011d8 */	u64	kdfc_fifo_44_ctrl;
#define	VXGE_HAL_KDFC_FIFO_44_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x011e0 */	u64	kdfc_fifo_45_ctrl;
#define	VXGE_HAL_KDFC_FIFO_45_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x011e8 */	u64	kdfc_fifo_46_ctrl;
#define	VXGE_HAL_KDFC_FIFO_46_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x011f0 */	u64	kdfc_fifo_47_ctrl;
#define	VXGE_HAL_KDFC_FIFO_47_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x011f8 */	u64	kdfc_fifo_48_ctrl;
#define	VXGE_HAL_KDFC_FIFO_48_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x01200 */	u64	kdfc_fifo_49_ctrl;
#define	VXGE_HAL_KDFC_FIFO_49_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x01208 */	u64	kdfc_fifo_50_ctrl;
#define	VXGE_HAL_KDFC_FIFO_50_CTRL_WRR_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x01210 */	u64	kdfc_krnl_usr_ctrl;
#define	VXGE_HAL_KDFC_KRNL_USR_CTRL_CODE(val)		    vBIT(val, 4, 4)
/* 0x01218 */	u64	kdfc_pda_monitor;
#define	VXGE_HAL_KDFC_PDA_MONITOR_KDFC_ACCEPT		    mBIT(7)
#define	VXGE_HAL_KDFC_PDA_MONITOR_FIFO_NO(val)		    vBIT(val, 10, 6)
#define	VXGE_HAL_KDFC_PDA_MONITOR_FIFO_ADD(val)		    vBIT(val, 17, 15)
#define	VXGE_HAL_KDFC_PDA_MONITOR_TYPE(val)		    vBIT(val, 32, 8)
#define	VXGE_HAL_KDFC_PDA_MONITOR_VP(val)		    vBIT(val, 43, 5)
/* 0x01220 */	u64	kdfc_mp_monitor;
#define	VXGE_HAL_KDFC_MP_MONITOR_KDFC_ACCEPT		    mBIT(7)
#define	VXGE_HAL_KDFC_MP_MONITOR_FIFO_NO(val)		    vBIT(val, 10, 6)
#define	VXGE_HAL_KDFC_MP_MONITOR_FIFO_ADD(val)		    vBIT(val, 17, 15)
#define	VXGE_HAL_KDFC_MP_MONITOR_TYPE(val)		    vBIT(val, 32, 8)
#define	VXGE_HAL_KDFC_MP_MONITOR_VP(val)		    vBIT(val, 43, 5)
/* 0x01228 */	u64	kdfc_pe_monitor;
#define	VXGE_HAL_KDFC_PE_MONITOR_KDFC_CREDIT		    mBIT(7)
#define	VXGE_HAL_KDFC_PE_MONITOR_FIFO_NO(val)		    vBIT(val, 10, 6)
#define	VXGE_HAL_KDFC_PE_MONITOR_FIFO_ADD(val)		    vBIT(val, 17, 15)
#define	VXGE_HAL_KDFC_PE_MONITOR_TYPE(val)		    vBIT(val, 32, 8)
#define	VXGE_HAL_KDFC_PE_MONITOR_VP(val)		    vBIT(val, 43, 5)
#define	VXGE_HAL_KDFC_PE_MONITOR_IMM_DATA_CNT(val)	    vBIT(val, 48, 8)
/* 0x01230 */	u64	kdfc_read_cntrl;
#define	VXGE_HAL_KDFC_READ_CNTRL_KDFC_FREEZE		    mBIT(7)
#define	VXGE_HAL_KDFC_READ_CNTRL_KDFC_RDCTRL(val)	    vBIT(val, 14, 2)
#define	VXGE_HAL_KDFC_READ_CNTRL_KDFC_WORD_SEL		    mBIT(23)
#define	VXGE_HAL_KDFC_READ_CNTRL_KDFC_ADDR(val)		    vBIT(val, 49, 15)
/* 0x01238 */	u64	kdfc_read_data;
#define	VXGE_HAL_KDFC_READ_DATA_READ_DATA(val)		    vBIT(val, 0, 64)
/* 0x01240 */	u64	kdfc_force_valid_ctrl;
#define	VXGE_HAL_KDFC_FORCE_VALID_CTRL_FORCE_VALID	    mBIT(7)
/* 0x01248 */	u64	kdfc_multi_cycle_ctrl;
#define	VXGE_HAL_KDFC_MULTI_CYCLE_CTRL_MULTI_CYCLE_SEL(val) vBIT(val, 6, 2)
/* 0x01250 */	u64	kdfc_ecc_ctrl;
#define	VXGE_HAL_KDFC_ECC_CTRL_ECC_DISABLE		    mBIT(7)
/* 0x01258 */	u64	kdfc_vpbp_ctrl;
#define	VXGE_HAL_KDFC_VPBP_CTRL_RD_XON_DIS		    mBIT(7)
#define	VXGE_HAL_KDFC_VPBP_CTRL_ROCRC_DIS		    mBIT(23)
#define	VXGE_HAL_KDFC_VPBP_CTRL_H2L_DIS			    mBIT(31)
#define	VXGE_HAL_KDFC_VPBP_CTRL_MSG_ONE_DIS		    mBIT(39)
#define	VXGE_HAL_KDFC_VPBP_CTRL_MSG_DMQ_DIS		    mBIT(47)
#define	VXGE_HAL_KDFC_VPBP_CTRL_PDA_DIS			    mBIT(55)
	u8	unused01600[0x01600 - 0x01260];

/* 0x01600 */	u64	rxmac_int_status;
#define	VXGE_HAL_RXMAC_INT_STATUS_RXMAC_GEN_ERR_RXMAC_GEN_INT mBIT(3)
#define	VXGE_HAL_RXMAC_INT_STATUS_RXMAC_ECC_ERR_RXMAC_ECC_INT mBIT(7)
#define	VXGE_HAL_RXMAC_INT_STATUS_RXMAC_VARIOUS_ERR_RXMAC_VARIOUS_INT mBIT(11)
/* 0x01608 */	u64	rxmac_int_mask;
	u8	unused01618[0x01618 - 0x01610];

/* 0x01618 */	u64	rxmac_gen_err_reg;
/* 0x01620 */	u64	rxmac_gen_err_mask;
/* 0x01628 */	u64	rxmac_gen_err_alarm;
/* 0x01630 */	u64	rxmac_ecc_err_reg;
#define	VXGE_HAL_RXMAC_ECC_ERR_REG_RMAC_PORT0_RMAC_RTS_PART_SG_ERR(val)\
							    vBIT(val, 0, 4)
#define	VXGE_HAL_RXMAC_ECC_ERR_REG_RMAC_PORT0_RMAC_RTS_PART_DB_ERR(val)\
							    vBIT(val, 4, 4)
#define	VXGE_HAL_RXMAC_ECC_ERR_REG_RMAC_PORT1_RMAC_RTS_PART_SG_ERR(val)\
							    vBIT(val, 8, 4)
#define	VXGE_HAL_RXMAC_ECC_ERR_REG_RMAC_PORT1_RMAC_RTS_PART_DB_ERR(val)\
							    vBIT(val, 12, 4)
#define	VXGE_HAL_RXMAC_ECC_ERR_REG_RMAC_PORT2_RMAC_RTS_PART_SG_ERR(val)\
							    vBIT(val, 16, 4)
#define	VXGE_HAL_RXMAC_ECC_ERR_REG_RMAC_PORT2_RMAC_RTS_PART_DB_ERR(val)\
							    vBIT(val, 20, 4)
#define	VXGE_HAL_RXMAC_ECC_ERR_REG_RTSJ_RMAC_DA_LKP_PRT0_SG_ERR(val)\
							    vBIT(val, 24, 2)
#define	VXGE_HAL_RXMAC_ECC_ERR_REG_RTSJ_RMAC_DA_LKP_PRT0_DB_ERR(val)\
							    vBIT(val, 26, 2)
#define	VXGE_HAL_RXMAC_ECC_ERR_REG_RTSJ_RMAC_DA_LKP_PRT1_SG_ERR(val)\
							    vBIT(val, 28, 2)
#define	VXGE_HAL_RXMAC_ECC_ERR_REG_RTSJ_RMAC_DA_LKP_PRT1_DB_ERR(val)\
							    vBIT(val, 30, 2)
#define	VXGE_HAL_RXMAC_ECC_ERR_REG_RTSJ_RMAC_VID_LKP_SG_ERR	mBIT(32)
#define	VXGE_HAL_RXMAC_ECC_ERR_REG_RTSJ_RMAC_VID_LKP_DB_ERR	mBIT(33)
#define	VXGE_HAL_RXMAC_ECC_ERR_REG_RTSJ_RMAC_PN_LKP_PRT0_SG_ERR	mBIT(34)
#define	VXGE_HAL_RXMAC_ECC_ERR_REG_RTSJ_RMAC_PN_LKP_PRT0_DB_ERR	mBIT(35)
#define	VXGE_HAL_RXMAC_ECC_ERR_REG_RTSJ_RMAC_PN_LKP_PRT1_SG_ERR	mBIT(36)
#define	VXGE_HAL_RXMAC_ECC_ERR_REG_RTSJ_RMAC_PN_LKP_PRT1_DB_ERR	mBIT(37)
#define	VXGE_HAL_RXMAC_ECC_ERR_REG_RTSJ_RMAC_PN_LKP_PRT2_SG_ERR	mBIT(38)
#define	VXGE_HAL_RXMAC_ECC_ERR_REG_RTSJ_RMAC_PN_LKP_PRT2_DB_ERR	mBIT(39)
#define	VXGE_HAL_RXMAC_ECC_ERR_REG_RTSJ_RMAC_RTH_MASK_SG_ERR(val)\
							    vBIT(val, 40, 7)
#define	VXGE_HAL_RXMAC_ECC_ERR_REG_RTSJ_RMAC_RTH_MASK_DB_ERR(val)\
							    vBIT(val, 47, 7)
#define	VXGE_HAL_RXMAC_ECC_ERR_REG_RTSJ_RMAC_RTH_LKP_SG_ERR(val)\
							    vBIT(val, 54, 3)
#define	VXGE_HAL_RXMAC_ECC_ERR_REG_RTSJ_RMAC_RTH_LKP_DB_ERR(val)\
							    vBIT(val, 57, 3)
#define	VXGE_HAL_RXMAC_ECC_ERR_REG_RTSJ_RMAC_DS_LKP_SG_ERR  mBIT(60)
#define	VXGE_HAL_RXMAC_ECC_ERR_REG_RTSJ_RMAC_DS_LKP_DB_ERR  mBIT(61)
/* 0x01638 */	u64	rxmac_ecc_err_mask;
/* 0x01640 */	u64	rxmac_ecc_err_alarm;
/* 0x01648 */	u64	rxmac_various_err_reg;
#define	VXGE_HAL_RXMAC_VARIOUS_ERR_REG_RMAC_RMAC_PORT0_FSM_ERR mBIT(0)
#define	VXGE_HAL_RXMAC_VARIOUS_ERR_REG_RMAC_RMAC_PORT1_FSM_ERR mBIT(1)
#define	VXGE_HAL_RXMAC_VARIOUS_ERR_REG_RMAC_RMAC_PORT2_FSM_ERR mBIT(2)
#define	VXGE_HAL_RXMAC_VARIOUS_ERR_REG_RMACJ_RMACJ_FSM_ERR  mBIT(3)
/* 0x01650 */	u64	rxmac_various_err_mask;
/* 0x01658 */	u64	rxmac_various_err_alarm;
/* 0x01660 */	u64	rxmac_gen_cfg;
#define	VXGE_HAL_RXMAC_GEN_CFG_SCALE_RMAC_UTIL		    mBIT(11)
/* 0x01668 */	u64	rxmac_authorize_all_addr;
#define	VXGE_HAL_RXMAC_AUTHORIZE_ALL_ADDR_VP(n)		    mBIT(n)
/* 0x01670 */	u64	rxmac_authorize_all_vid;
#define	VXGE_HAL_RXMAC_AUTHORIZE_ALL_VID_VP(n)		    mBIT(n)
	u8	unused016b8[0x016b8 - 0x01678];

/* 0x016b8 */	u64	rxmac_thresh_cross_repl;
#define	VXGE_HAL_RXMAC_THRESH_CROSS_REPL_RMACJ_PAUSE_LOW_UP_CROSSED	mBIT(3)
#define	VXGE_HAL_RXMAC_THRESH_CROSS_REPL_RMACJ_PAUSE_LOW_DOWN_CROSSED	mBIT(7)
#define	VXGE_HAL_RXMAC_THRESH_CROSS_REPL_RMACJ_PAUSE_HIGH_UP_CROSSED	mBIT(11)
#define	VXGE_HAL_RXMAC_THRESH_CROSS_REPL_RMACJ_PAUSE_HIGH_DOWN_CROSSED	mBIT(15)
#define	VXGE_HAL_RXMAC_THRESH_CROSS_REPL_RMACJ_RED0_UP_CROSSED		mBIT(35)
#define	VXGE_HAL_RXMAC_THRESH_CROSS_REPL_RMACJ_RED0_DOWN_CROSSED	mBIT(39)
#define	VXGE_HAL_RXMAC_THRESH_CROSS_REPL_RMACJ_RED1_UP_CROSSED		mBIT(43)
#define	VXGE_HAL_RXMAC_THRESH_CROSS_REPL_RMACJ_RED1_DOWN_CROSSED	mBIT(47)
#define	VXGE_HAL_RXMAC_THRESH_CROSS_REPL_RMACJ_RED2_UP_CROSSED		mBIT(51)
#define	VXGE_HAL_RXMAC_THRESH_CROSS_REPL_RMACJ_RED2_DOWN_CROSSED	mBIT(55)
#define	VXGE_HAL_RXMAC_THRESH_CROSS_REPL_RMACJ_RED3_UP_CROSSED		mBIT(59)
#define	VXGE_HAL_RXMAC_THRESH_CROSS_REPL_RMACJ_RED3_DOWN_CROSSED	mBIT(63)
/* 0x016c0 */	u64	rxmac_red_rate_repl_queue;
#define	VXGE_HAL_RXMAC_RED_RATE_REPL_QUEUE_CRATE_THR0(val)  vBIT(val, 0, 4)
#define	VXGE_HAL_RXMAC_RED_RATE_REPL_QUEUE_CRATE_THR1(val)  vBIT(val, 4, 4)
#define	VXGE_HAL_RXMAC_RED_RATE_REPL_QUEUE_CRATE_THR2(val)  vBIT(val, 8, 4)
#define	VXGE_HAL_RXMAC_RED_RATE_REPL_QUEUE_CRATE_THR3(val)  vBIT(val, 12, 4)
#define	VXGE_HAL_RXMAC_RED_RATE_REPL_QUEUE_FRATE_THR0(val)  vBIT(val, 16, 4)
#define	VXGE_HAL_RXMAC_RED_RATE_REPL_QUEUE_FRATE_THR1(val)  vBIT(val, 20, 4)
#define	VXGE_HAL_RXMAC_RED_RATE_REPL_QUEUE_FRATE_THR2(val)  vBIT(val, 24, 4)
#define	VXGE_HAL_RXMAC_RED_RATE_REPL_QUEUE_FRATE_THR3(val)  vBIT(val, 28, 4)
#define	VXGE_HAL_RXMAC_RED_RATE_REPL_QUEUE_TRICKLE_EN	    mBIT(35)
	u8	unused016e0[0x016e0 - 0x016c8];

/* 0x016e0 */	u64	rxmac_cfg0_port[3];
#define	VXGE_HAL_RXMAC_CFG0_PORT_RMAC_EN		    mBIT(3)
#define	VXGE_HAL_RXMAC_CFG0_PORT_STRIP_FCS		    mBIT(7)
#define	VXGE_HAL_RXMAC_CFG0_PORT_DISCARD_PFRM		    mBIT(11)
#define	VXGE_HAL_RXMAC_CFG0_PORT_IGNORE_FCS_ERR		    mBIT(15)
#define	VXGE_HAL_RXMAC_CFG0_PORT_IGNORE_LONG_ERR	    mBIT(19)
#define	VXGE_HAL_RXMAC_CFG0_PORT_IGNORE_USIZED_ERR	    mBIT(23)
#define	VXGE_HAL_RXMAC_CFG0_PORT_IGNORE_LEN_MISMATCH	    mBIT(27)
#define	VXGE_HAL_RXMAC_CFG0_PORT_MAX_PYLD_LEN(val)	    vBIT(val, 50, 14)
	u8	unused01710[0x01710 - 0x016f8];

/* 0x01710 */	u64	rxmac_cfg2_port[3];
#define	VXGE_HAL_RXMAC_CFG2_PORT_PROM_EN		    mBIT(3)
/* 0x01728 */	u64	rxmac_pause_cfg_port[3];
#define	VXGE_HAL_RXMAC_PAUSE_CFG_PORT_GEN_EN		    mBIT(3)
#define	VXGE_HAL_RXMAC_PAUSE_CFG_PORT_RCV_EN		    mBIT(7)
#define	VXGE_HAL_RXMAC_PAUSE_CFG_PORT_ACCEL_SEND(val)	    vBIT(val, 9, 3)
#define	VXGE_HAL_RXMAC_PAUSE_CFG_PORT_DUAL_THR		    mBIT(15)
#define	VXGE_HAL_RXMAC_PAUSE_CFG_PORT_HIGH_PTIME(val)	    vBIT(val, 20, 16)
#define	VXGE_HAL_RXMAC_PAUSE_CFG_PORT_IGNORE_PF_FCS_ERR	    mBIT(39)
#define	VXGE_HAL_RXMAC_PAUSE_CFG_PORT_IGNORE_PF_LEN_ERR	    mBIT(43)
#define	VXGE_HAL_RXMAC_PAUSE_CFG_PORT_LIMITER_EN	    mBIT(47)
#define	VXGE_HAL_RXMAC_PAUSE_CFG_PORT_MAX_LIMIT(val)	    vBIT(val, 48, 8)
#define	VXGE_HAL_RXMAC_PAUSE_CFG_PORT_PERMIT_RATEMGMT_CTRL  mBIT(59)
	u8	unused01758[0x01758 - 0x01740];

/* 0x01758 */	u64	rxmac_red_cfg0_port[3];
#define	VXGE_HAL_RXMAC_RED_CFG0_PORT_RED_EN_VP(n)	    mBIT(n)
/* 0x01770 */	u64	rxmac_red_cfg1_port[3];
#define	VXGE_HAL_RXMAC_RED_CFG1_PORT_FINE_EN		    mBIT(3)
#define	VXGE_HAL_RXMAC_RED_CFG1_PORT_RED_EN_REPL_QUEUE	    mBIT(11)
/* 0x01788 */	u64	rxmac_red_cfg2_port[3];
#define	VXGE_HAL_RXMAC_RED_CFG2_PORT_TRICKLE_EN_VP(n)	    mBIT(n)
/* 0x017a0 */	u64	rxmac_link_util_port[3];
#define	VXGE_HAL_RXMAC_LINK_UTIL_PORT_RMAC_RMAC_UTILIZATION(val) vBIT(val, 1, 7)
#define	VXGE_HAL_RXMAC_LINK_UTIL_PORT_RMAC_UTIL_CFG(val)    vBIT(val, 8, 4)
#define	VXGE_HAL_RXMAC_LINK_UTIL_PORT_RMAC_RMAC_FRAC_UTIL(val) vBIT(val, 12, 4)
#define	VXGE_HAL_RXMAC_LINK_UTIL_PORT_RMAC_PKT_WEIGHT(val)  vBIT(val, 16, 4)
#define	VXGE_HAL_RXMAC_LINK_UTIL_PORT_RMAC_RMAC_SCALE_FACTOR mBIT(23)
	u8	unused017d0[0x017d0 - 0x017b8];

/* 0x017d0 */	u64	rxmac_status_port[3];
#define	VXGE_HAL_RXMAC_STATUS_PORT_RMAC_RX_FRM_RCVD	    mBIT(3)
	u8	unused01800[0x01800 - 0x017e8];

/* 0x01800 */	u64	rxmac_rx_pa_cfg0;
#define	VXGE_HAL_RXMAC_RX_PA_CFG0_IGNORE_FRAME_ERR	    mBIT(3)
#define	VXGE_HAL_RXMAC_RX_PA_CFG0_SUPPORT_SNAP_AB_N	    mBIT(7)
#define	VXGE_HAL_RXMAC_RX_PA_CFG0_SEARCH_FOR_HAO	    mBIT(18)
#define	VXGE_HAL_RXMAC_RX_PA_CFG0_SUPPORT_MOBILE_IPV6_HDRS  mBIT(19)
#define	VXGE_HAL_RXMAC_RX_PA_CFG0_IPV6_STOP_SEARCHING	    mBIT(23)
#define	VXGE_HAL_RXMAC_RX_PA_CFG0_NO_PS_IF_UNKNOWN	    mBIT(27)
#define	VXGE_HAL_RXMAC_RX_PA_CFG0_SEARCH_FOR_ETYPE	    mBIT(35)
#define	VXGE_HAL_RXMAC_RX_PA_CFG0_TOSS_ANY_FRM_IF_L3_CSUM_ERR mBIT(39)
#define	VXGE_HAL_RXMAC_RX_PA_CFG0_TOSS_OFFLD_FRM_IF_L3_CSUM_ERR mBIT(43)
#define	VXGE_HAL_RXMAC_RX_PA_CFG0_TOSS_ANY_FRM_IF_L4_CSUM_ERR	mBIT(47)
#define	VXGE_HAL_RXMAC_RX_PA_CFG0_TOSS_OFFLD_FRM_IF_L4_CSUM_ERR	mBIT(51)
#define	VXGE_HAL_RXMAC_RX_PA_CFG0_TOSS_ANY_FRM_IF_RPA_ERR	mBIT(55)
#define	VXGE_HAL_RXMAC_RX_PA_CFG0_TOSS_OFFLD_FRM_IF_RPA_ERR	mBIT(59)
#define	VXGE_HAL_RXMAC_RX_PA_CFG0_JUMBO_SNAP_EN		    mBIT(63)
/* 0x01808 */	u64	rxmac_rx_pa_cfg1;
#define	VXGE_HAL_RXMAC_RX_PA_CFG1_REPL_IPV4_TCP_INCL_PH	    mBIT(3)
#define	VXGE_HAL_RXMAC_RX_PA_CFG1_REPL_IPV6_TCP_INCL_PH	    mBIT(7)
#define	VXGE_HAL_RXMAC_RX_PA_CFG1_REPL_IPV4_UDP_INCL_PH	    mBIT(11)
#define	VXGE_HAL_RXMAC_RX_PA_CFG1_REPL_IPV6_UDP_INCL_PH	    mBIT(15)
#define	VXGE_HAL_RXMAC_RX_PA_CFG1_REPL_L4_INCL_CF	    mBIT(19)
#define	VXGE_HAL_RXMAC_RX_PA_CFG1_REPL_STRIP_VLAN_TAG	    mBIT(23)
	u8	unused01828[0x01828 - 0x01810];

/* 0x01828 */	u64	rts_mgr_cfg0;
#define	VXGE_HAL_RTS_MGR_CFG0_RTS_DP_SP_PRIORITY	    mBIT(3)
#define	VXGE_HAL_RTS_MGR_CFG0_FLEX_L4PRTCL_VALUE(val)	    vBIT(val, 24, 8)
#define	VXGE_HAL_RTS_MGR_CFG0_ICMP_TRASH		    mBIT(35)
#define	VXGE_HAL_RTS_MGR_CFG0_TCPSYN_TRASH		    mBIT(39)
#define	VXGE_HAL_RTS_MGR_CFG0_ZL4PYLD_TRASH		    mBIT(43)
#define	VXGE_HAL_RTS_MGR_CFG0_L4PRTCL_TCP_TRASH		    mBIT(47)
#define	VXGE_HAL_RTS_MGR_CFG0_L4PRTCL_UDP_TRASH		    mBIT(51)
#define	VXGE_HAL_RTS_MGR_CFG0_L4PRTCL_FLEX_TRASH	    mBIT(55)
#define	VXGE_HAL_RTS_MGR_CFG0_IPFRAG_TRASH		    mBIT(59)
/* 0x01830 */	u64	rts_mgr_cfg1;
#define	VXGE_HAL_RTS_MGR_CFG1_DA_ACTIVE_TABLE		    mBIT(3)
#define	VXGE_HAL_RTS_MGR_CFG1_PN_ACTIVE_TABLE		    mBIT(7)
/* 0x01838 */	u64	rts_mgr_criteria_priority;
#define	VXGE_HAL_RTS_MGR_CRITERIA_PRIORITY_ETYPE(val)	    vBIT(val, 5, 3)
#define	VXGE_HAL_RTS_MGR_CRITERIA_PRIORITY_ICMP_TCPSYN(val) vBIT(val, 9, 3)
#define	VXGE_HAL_RTS_MGR_CRITERIA_PRIORITY_L4PN(val)	    vBIT(val, 13, 3)
#define	VXGE_HAL_RTS_MGR_CRITERIA_PRIORITY_RANGE_L4PN(val)  vBIT(val, 17, 3)
#define	VXGE_HAL_RTS_MGR_CRITERIA_PRIORITY_RTH_IT(val)	    vBIT(val, 21, 3)
#define	VXGE_HAL_RTS_MGR_CRITERIA_PRIORITY_DS(val)	    vBIT(val, 25, 3)
#define	VXGE_HAL_RTS_MGR_CRITERIA_PRIORITY_QOS(val)	    vBIT(val, 29, 3)
#define	VXGE_HAL_RTS_MGR_CRITERIA_PRIORITY_ZL4PYLD(val)	    vBIT(val, 33, 3)
#define	VXGE_HAL_RTS_MGR_CRITERIA_PRIORITY_L4PRTCL(val)	    vBIT(val, 37, 3)
/* 0x01840 */	u64	rts_mgr_da_pause_cfg;
#define	VXGE_HAL_RTS_MGR_DA_PAUSE_CFG_VPATH_VECTOR(val)	    vBIT(val, 0, 17)
/* 0x01848 */	u64	rts_mgr_da_slow_proto_cfg;
#define	VXGE_HAL_RTS_MGR_DA_SLOW_PROTO_CFG_VPATH_VECTOR(val) vBIT(val, 0, 17)
	u8	unused018a8[0x018a8 - 0x01850];

/* 0x018a8 */	u64	rts_mgr_steer_ctrl;
#define	VXGE_HAL_RTS_MGR_STEER_CTRL_WE			    mBIT(7)
#define	VXGE_HAL_RTS_MGR_STEER_CTRL_DATA_STRUCT_SEL(val)    vBIT(val, 8, 4)
#define	VXGE_HAL_RTS_MGR_STEER_CTRL_STROBE		    mBIT(15)
#define	VXGE_HAL_RTS_MGR_STEER_CTRL_BEHAV_TBL_SEL	    mBIT(23)
#define	VXGE_HAL_RTS_MGR_STEER_CTRL_TABLE_SEL		    mBIT(27)
#define	VXGE_HAL_RTS_MGR_STEER_CTRL_OFFSET(val)		    vBIT(val, 35, 13)
#define	VXGE_HAL_RTS_MGR_STEER_CTRL_RMACJ_STATUS	    mBIT(0)
/* 0x018b0 */	u64	rts_mgr_steer_data0;
#define	VXGE_HAL_RTS_MGR_STEER_DATA0_DATA(val)		    vBIT(val, 0, 64)
/* 0x018b8 */	u64	rts_mgr_steer_data1;
#define	VXGE_HAL_RTS_MGR_STEER_DATA1_DATA(val)		    vBIT(val, 0, 64)
/* 0x018c0 */	u64	rts_mgr_steer_vpath_vector;
#define	VXGE_HAL_RTS_MGR_STEER_VPATH_VECTOR_VPATH_VECTOR(val) vBIT(val, 0, 17)
	u8	unused01930[0x01930 - 0x018c8];

/* 0x01930 */	u64	xmac_stats_rx_xgmii_char;
#define	VXGE_HAL_XMAC_STATS_RX_XGMII_CHAR_LANE_CHAR1(val)   vBIT(val, 1, 3)
#define	VXGE_HAL_XMAC_STATS_RX_XGMII_CHAR_RXC_CHAR1	    mBIT(7)
#define	VXGE_HAL_XMAC_STATS_RX_XGMII_CHAR_RXD_CHAR1(val)    vBIT(val, 8, 8)
#define	VXGE_HAL_XMAC_STATS_RX_XGMII_CHAR_LANE_CHAR2(val)   vBIT(val, 17, 3)
#define	VXGE_HAL_XMAC_STATS_RX_XGMII_CHAR_RXC_CHAR2	    mBIT(23)
#define	VXGE_HAL_XMAC_STATS_RX_XGMII_CHAR_RXD_CHAR2(val)    vBIT(val, 24, 8)
#define	VXGE_HAL_XMAC_STATS_RX_XGMII_CHAR_BEHAV_CHAR2_NEAR_CHAR1 mBIT(39)
#define	VXGE_HAL_XMAC_STATS_RX_XGMII_CHAR_BEHAV_CHAR2_NUM_CHAR(val)\
							    vBIT(val, 40, 16)
/* 0x01938 */	u64	xmac_stats_rx_xgmii_column1;
#define	VXGE_HAL_XMAC_STATS_RX_XGMII_COLUMN1_RXC_LANE0	    mBIT(7)
#define	VXGE_HAL_XMAC_STATS_RX_XGMII_COLUMN1_RXD_LANE0(val) vBIT(val, 8, 8)
#define	VXGE_HAL_XMAC_STATS_RX_XGMII_COLUMN1_RXC_LANE1	    mBIT(23)
#define	VXGE_HAL_XMAC_STATS_RX_XGMII_COLUMN1_RXD_LANE1(val) vBIT(val, 24, 8)
#define	VXGE_HAL_XMAC_STATS_RX_XGMII_COLUMN1_RXC_LANE2	    mBIT(39)
#define	VXGE_HAL_XMAC_STATS_RX_XGMII_COLUMN1_RXD_LANE2(val) vBIT(val, 40, 8)
#define	VXGE_HAL_XMAC_STATS_RX_XGMII_COLUMN1_RXC_LANE3	    mBIT(55)
#define	VXGE_HAL_XMAC_STATS_RX_XGMII_COLUMN1_RXD_LANE3(val) vBIT(val, 56, 8)
/* 0x01940 */	u64	xmac_stats_rx_xgmii_column2;
#define	VXGE_HAL_XMAC_STATS_RX_XGMII_COLUMN2_RXC_LANE0	    mBIT(7)
#define	VXGE_HAL_XMAC_STATS_RX_XGMII_COLUMN2_RXD_LANE0(val) vBIT(val, 8, 8)
#define	VXGE_HAL_XMAC_STATS_RX_XGMII_COLUMN2_RXC_LANE1	    mBIT(23)
#define	VXGE_HAL_XMAC_STATS_RX_XGMII_COLUMN2_RXD_LANE1(val) vBIT(val, 24, 8)
#define	VXGE_HAL_XMAC_STATS_RX_XGMII_COLUMN2_RXC_LANE2	    mBIT(39)
#define	VXGE_HAL_XMAC_STATS_RX_XGMII_COLUMN2_RXD_LANE2(val) vBIT(val, 40, 8)
#define	VXGE_HAL_XMAC_STATS_RX_XGMII_COLUMN2_RXC_LANE3	    mBIT(55)
#define	VXGE_HAL_XMAC_STATS_RX_XGMII_COLUMN2_RXD_LANE3(val) vBIT(val, 56, 8)
/* 0x01948 */	u64	xmac_stats_rx_xgmii_behav_column2;
#define	VXGE_HAL_XMAC_STATS_RX_XGMII_BEHAV_COLUMN2_NEAR_COL1 mBIT(7)
#define	VXGE_HAL_XMAC_STATS_RX_XGMII_BEHAV_COLUMN2_NUM_COL(val) vBIT(val, 8, 16)
/* 0x01950 */	u64	xmac_rx_xgmii_capture_ctrl_port[3];
#define	VXGE_HAL_XMAC_RX_XGMII_CAPTURE_CTRL_PORT_EN	    mBIT(3)
#define	VXGE_HAL_XMAC_RX_XGMII_CAPTURE_CTRL_PORT_READBACK   mBIT(7)
/* 0x01968 */	u64	dbg_stat_rx_any_frms;
#define	VXGE_HAL_DBG_STAT_RX_ANY_FRMS_PORT0_RX_ANY_FRMS(val) vBIT(val, 0, 8)
#define	VXGE_HAL_DBG_STAT_RX_ANY_FRMS_PORT1_RX_ANY_FRMS(val) vBIT(val, 8, 8)
#define	VXGE_HAL_DBG_STAT_RX_ANY_FRMS_PORT2_RX_ANY_FRMS(val) vBIT(val, 16, 8)
	u8	unused01a00[0x01a00 - 0x01970];

/* 0x01a00 */	u64	rxmac_red_rate_vp[17];
#define	VXGE_HAL_RXMAC_RED_RATE_VP_CRATE_THR0(val)	    vBIT(val, 0, 4)
#define	VXGE_HAL_RXMAC_RED_RATE_VP_CRATE_THR1(val)	    vBIT(val, 4, 4)
#define	VXGE_HAL_RXMAC_RED_RATE_VP_CRATE_THR2(val)	    vBIT(val, 8, 4)
#define	VXGE_HAL_RXMAC_RED_RATE_VP_CRATE_THR3(val)	    vBIT(val, 12, 4)
#define	VXGE_HAL_RXMAC_RED_RATE_VP_FRATE_THR0(val)	    vBIT(val, 16, 4)
#define	VXGE_HAL_RXMAC_RED_RATE_VP_FRATE_THR1(val)	    vBIT(val, 20, 4)
#define	VXGE_HAL_RXMAC_RED_RATE_VP_FRATE_THR2(val)	    vBIT(val, 24, 4)
#define	VXGE_HAL_RXMAC_RED_RATE_VP_FRATE_THR3(val)	    vBIT(val, 28, 4)
	u8	unused01c00[0x01c00 - 0x01a88];

/* 0x01c00 */	u64	rxmac_thresh_cross_vp[17];
#define	VXGE_HAL_RXMAC_THRESH_CROSS_VP_RMACJ_PAUSE_LOW_UP_CROSSED   mBIT(3)
#define	VXGE_HAL_RXMAC_THRESH_CROSS_VP_RMACJ_PAUSE_LOW_DOWN_CROSSED mBIT(7)
#define	VXGE_HAL_RXMAC_THRESH_CROSS_VP_RMACJ_PAUSE_HIGH_UP_CROSSED  mBIT(11)
#define	VXGE_HAL_RXMAC_THRESH_CROSS_VP_RMACJ_PAUSE_HIGH_DOWN_CROSSED mBIT(15)
#define	VXGE_HAL_RXMAC_THRESH_CROSS_VP_RMACJ_RED_THR0_UP_CROSSED    mBIT(35)
#define	VXGE_HAL_RXMAC_THRESH_CROSS_VP_RMACJ_RED_THR0_DOWN_CROSSED  mBIT(39)
#define	VXGE_HAL_RXMAC_THRESH_CROSS_VP_RMACJ_RED_THR1_UP_CROSSED    mBIT(43)
#define	VXGE_HAL_RXMAC_THRESH_CROSS_VP_RMACJ_RED_THR1_DOWN_CROSSED  mBIT(47)
#define	VXGE_HAL_RXMAC_THRESH_CROSS_VP_RMACJ_RED_THR2_UP_CROSSED    mBIT(51)
#define	VXGE_HAL_RXMAC_THRESH_CROSS_VP_RMACJ_RED_THR2_DOWN_CROSSED  mBIT(55)
#define	VXGE_HAL_RXMAC_THRESH_CROSS_VP_RMACJ_RED_THR3_UP_CROSSED    mBIT(59)
#define	VXGE_HAL_RXMAC_THRESH_CROSS_VP_RMACJ_RED_THR3_DOWN_CROSSED  mBIT(63)
	u8	unused01e00[0x01e00 - 0x01c88];

/* 0x01e00 */	u64	xgmac_int_status;
#define	VXGE_HAL_XGMAC_INT_STATUS_XMAC_GEN_ERR_XMAC_GEN_INT mBIT(3)
#define	VXGE_HAL_XGMAC_INT_STATUS_XMAC_LINK_ERR_PORT0_XMAC_LINK_INT_PORT0\
							    mBIT(7)
#define	VXGE_HAL_XGMAC_INT_STATUS_XMAC_LINK_ERR_PORT1_XMAC_LINK_INT_PORT1\
							    mBIT(11)
#define	VXGE_HAL_XGMAC_INT_STATUS_XGXS_GEN_ERR_XGXS_GEN_INT mBIT(15)
#define	VXGE_HAL_XGMAC_INT_STATUS_ASIC_NTWK_ERR_ASIC_NTWK_INT mBIT(19)
#define	VXGE_HAL_XGMAC_INT_STATUS_ASIC_GPIO_ERR_ASIC_GPIO_INT mBIT(23)
/* 0x01e08 */	u64	xgmac_int_mask;
/* 0x01e10 */	u64	xmac_gen_err_reg;
#define	VXGE_HAL_XMAC_GEN_ERR_REG_LAGC_LAG_PORT0_ACTOR_CHURN_DETECTED mBIT(7)
#define	VXGE_HAL_XMAC_GEN_ERR_REG_LAGC_LAG_PORT0_PARTNER_CHURN_DETECTED	mBIT(11)
#define	VXGE_HAL_XMAC_GEN_ERR_REG_LAGC_LAG_PORT0_RECEIVED_LACPDU    mBIT(15)
#define	VXGE_HAL_XMAC_GEN_ERR_REG_LAGC_LAG_PORT1_ACTOR_CHURN_DETECTED	mBIT(19)
#define	VXGE_HAL_XMAC_GEN_ERR_REG_LAGC_LAG_PORT1_PARTNER_CHURN_DETECTED	mBIT(23)
#define	VXGE_HAL_XMAC_GEN_ERR_REG_LAGC_LAG_PORT1_RECEIVED_LACPDU    mBIT(27)
#define	VXGE_HAL_XMAC_GEN_ERR_REG_XLCM_LAG_FAILOVER_DETECTED	mBIT(31)
#define	VXGE_HAL_XMAC_GEN_ERR_REG_XSTATS_RMAC_STATS_TILE0_SG_ERR(val)\
							    vBIT(val, 40, 2)
#define	VXGE_HAL_XMAC_GEN_ERR_REG_XSTATS_RMAC_STATS_TILE0_DB_ERR(val)\
							    vBIT(val, 42, 2)
#define	VXGE_HAL_XMAC_GEN_ERR_REG_XSTATS_RMAC_STATS_TILE1_SG_ERR(val)\
							    vBIT(val, 44, 2)
#define	VXGE_HAL_XMAC_GEN_ERR_REG_XSTATS_RMAC_STATS_TILE1_DB_ERR(val)\
							    vBIT(val, 46, 2)
#define	VXGE_HAL_XMAC_GEN_ERR_REG_XSTATS_RMAC_STATS_TILE2_SG_ERR(val)\
							    vBIT(val, 48, 2)
#define	VXGE_HAL_XMAC_GEN_ERR_REG_XSTATS_RMAC_STATS_TILE2_DB_ERR(val)\
							    vBIT(val, 50, 2)
#define	VXGE_HAL_XMAC_GEN_ERR_REG_XSTATS_RMAC_STATS_TILE3_SG_ERR(val)\
							    vBIT(val, 52, 2)
#define	VXGE_HAL_XMAC_GEN_ERR_REG_XSTATS_RMAC_STATS_TILE3_DB_ERR(val)\
							    vBIT(val, 54, 2)
#define	VXGE_HAL_XMAC_GEN_ERR_REG_XSTATS_RMAC_STATS_TILE4_SG_ERR(val)\
							    vBIT(val, 56, 2)
#define	VXGE_HAL_XMAC_GEN_ERR_REG_XSTATS_RMAC_STATS_TILE4_DB_ERR(val)\
							    vBIT(val, 58, 2)
#define	VXGE_HAL_XMAC_GEN_ERR_REG_XMACJ_XMAC_FSM_ERR	    mBIT(63)
/* 0x01e18 */	u64	xmac_gen_err_mask;
/* 0x01e20 */	u64	xmac_gen_err_alarm;
/* 0x01e28 */	u64	xmac_link_err_port0_reg;
#define	VXGE_HAL_XMAC_LINK_ERR_PORT_REG_XMACJ_PORT_DOWN	    mBIT(3)
#define	VXGE_HAL_XMAC_LINK_ERR_PORT_REG_XMACJ_PORT_UP	    mBIT(7)
#define	VXGE_HAL_XMAC_LINK_ERR_PORT_REG_XMACJ_PORT_WENT_DOWN mBIT(11)
#define	VXGE_HAL_XMAC_LINK_ERR_PORT_REG_XMACJ_PORT_WENT_UP  mBIT(15)
#define	VXGE_HAL_XMAC_LINK_ERR_PORT_REG_XMACJ_PORT_REAFFIRMED_FAULT mBIT(19)
#define	VXGE_HAL_XMAC_LINK_ERR_PORT_REG_XMACJ_PORT_REAFFIRMED_OK    mBIT(23)
#define	VXGE_HAL_XMAC_LINK_ERR_PORT_REG_XMACJ_LINK_DOWN	    mBIT(27)
#define	VXGE_HAL_XMAC_LINK_ERR_PORT_REG_XMACJ_LINK_UP	    mBIT(31)
#define	VXGE_HAL_XMAC_LINK_ERR_PORT_REG_RATEMGMT_RATE_CHANGE mBIT(35)
#define	VXGE_HAL_XMAC_LINK_ERR_PORT_REG_RATEMGMT_LASI_INV   mBIT(39)
#define	VXGE_HAL_XMAC_LINK_ERR_PORT_REG_XMDIO_MDIO_MGR_ACCESS_COMPLETE mBIT(47)
/* 0x01e30 */	u64	xmac_link_err_port0_mask;
/* 0x01e38 */	u64	xmac_link_err_port0_alarm;
/* 0x01e40 */	u64	xmac_link_err_port1_reg;
/* 0x01e48 */	u64	xmac_link_err_port1_mask;
/* 0x01e50 */	u64	xmac_link_err_port1_alarm;
/* 0x01e58 */	u64	xgxs_gen_err_reg;
#define	VXGE_HAL_XGXS_GEN_ERR_REG_XGXS_XGXS_FSM_ERR	    mBIT(63)
/* 0x01e60 */	u64	xgxs_gen_err_mask;
/* 0x01e68 */	u64	xgxs_gen_err_alarm;
/* 0x01e70 */	u64	asic_ntwk_err_reg;
#define	VXGE_HAL_ASIC_NTWK_ERR_REG_XMACJ_NTWK_DOWN	    mBIT(3)
#define	VXGE_HAL_ASIC_NTWK_ERR_REG_XMACJ_NTWK_UP	    mBIT(7)
#define	VXGE_HAL_ASIC_NTWK_ERR_REG_XMACJ_NTWK_WENT_DOWN	    mBIT(11)
#define	VXGE_HAL_ASIC_NTWK_ERR_REG_XMACJ_NTWK_WENT_UP	    mBIT(15)
#define	VXGE_HAL_ASIC_NTWK_ERR_REG_XMACJ_NTWK_REAFFIRMED_FAULT	mBIT(19)
#define	VXGE_HAL_ASIC_NTWK_ERR_REG_XMACJ_NTWK_REAFFIRMED_OK	mBIT(23)
/* 0x01e78 */	u64	asic_ntwk_err_mask;
/* 0x01e80 */	u64	asic_ntwk_err_alarm;
/* 0x01e88 */	u64	asic_gpio_err_reg;
#define	VXGE_HAL_ASIC_GPIO_ERR_REG_XMACJ_GPIO_INT(n)	    mBIT(n)
/* 0x01e90 */	u64	asic_gpio_err_mask;
/* 0x01e98 */	u64	asic_gpio_err_alarm;
/* 0x01ea0 */	u64	xgmac_gen_status;
#define	VXGE_HAL_XGMAC_GEN_STATUS_XMACJ_NTWK_OK		    mBIT(3)
#define	VXGE_HAL_XGMAC_GEN_STATUS_XMACJ_NTWK_DATA_RATE	    mBIT(11)
/* 0x01ea8 */	u64	xgmac_gen_fw_memo_status;
#define	VXGE_HAL_XGMAC_GEN_FW_MEMO_STATUS_XMACJ_EVENTS_PENDING(val)\
							    vBIT(val, 0, 17)
/* 0x01eb0 */	u64	xgmac_gen_fw_memo_mask;
#define	VXGE_HAL_XGMAC_GEN_FW_MEMO_MASK_MASK(val)	    vBIT(val, 0, 64)
/* 0x01eb8 */	u64	xgmac_gen_fw_vpath_to_vsport_status;
#define	VXGE_HAL_XGMAC_GEN_FW_VPATH_TO_VSPORT_STATUS_XMACJ_EVENTS_PENDING(val)\
							    vBIT(val, 0, 17)
/* 0x01ec0 */	u64	xgmac_main_cfg_port[2];
#define	VXGE_HAL_XGMAC_MAIN_CFG_PORT_PORT_EN		    mBIT(3)
/* 0x01ed0 */	u64	xgmac_debounce_port[2];
#define	VXGE_HAL_XGMAC_DEBOUNCE_PORT_PERIOD_LINK_UP(val)    vBIT(val, 0, 4)
#define	VXGE_HAL_XGMAC_DEBOUNCE_PORT_PERIOD_LINK_DOWN(val)  vBIT(val, 4, 4)
#define	VXGE_HAL_XGMAC_DEBOUNCE_PORT_PERIOD_PORT_UP(val)    vBIT(val, 8, 4)
#define	VXGE_HAL_XGMAC_DEBOUNCE_PORT_PERIOD_PORT_DOWN(val)  vBIT(val, 12, 4)
/* 0x01ee0 */	u64	xgmac_status_port[2];
#define	VXGE_HAL_XGMAC_STATUS_PORT_RMAC_REMOTE_FAULT	    mBIT(3)
#define	VXGE_HAL_XGMAC_STATUS_PORT_RMAC_LOCAL_FAULT	    mBIT(7)
#define	VXGE_HAL_XGMAC_STATUS_PORT_XMACJ_MAC_PHY_LAYER_AVAIL mBIT(11)
#define	VXGE_HAL_XGMAC_STATUS_PORT_XMACJ_PORT_OK	    mBIT(15)
	u8	unused01f40[0x01f40 - 0x01ef0];

/* 0x01f40 */	u64	xmac_gen_cfg;
#define	VXGE_HAL_XMAC_GEN_CFG_RATEMGMT_MAC_RATE_SEL(val)    vBIT(val, 2, 2)
#define	VXGE_HAL_XMAC_GEN_CFG_TX_HEAD_DROP_WHEN_FAULT	    mBIT(7)
#define	VXGE_HAL_XMAC_GEN_CFG_FAULT_BEHAVIOUR		    mBIT(27)
#define	VXGE_HAL_XMAC_GEN_CFG_PERIOD_NTWK_UP(val)	    vBIT(val, 28, 4)
#define	VXGE_HAL_XMAC_GEN_CFG_PERIOD_NTWK_DOWN(val)	    vBIT(val, 32, 4)
/* 0x01f48 */	u64	xmac_timestamp;
#define	VXGE_HAL_XMAC_TIMESTAMP_EN			    mBIT(3)
#define	VXGE_HAL_XMAC_TIMESTAMP_USE_LINK_ID(val)	    vBIT(val, 6, 2)
#define	VXGE_HAL_XMAC_TIMESTAMP_INTERVAL(val)		    vBIT(val, 12, 4)
#define	VXGE_HAL_XMAC_TIMESTAMP_TIMER_RESTART		    mBIT(19)
#define	VXGE_HAL_XMAC_TIMESTAMP_XMACJ_ROLLOVER_CNT(val)	    vBIT(val, 32, 16)
/* 0x01f50 */	u64	xmac_stats_gen_cfg;
#define	VXGE_HAL_XMAC_STATS_GEN_CFG_PRTAGGR_CUM_TIMER(val)  vBIT(val, 4, 4)
#define	VXGE_HAL_XMAC_STATS_GEN_CFG_VPATH_CUM_TIMER(val)    vBIT(val, 8, 4)
#define	VXGE_HAL_XMAC_STATS_GEN_CFG_VLAN_HANDLING	    mBIT(15)
/* 0x01f58 */	u64	xmac_stats_sys_cmd;
#define	VXGE_HAL_XMAC_STATS_SYS_CMD_OP(val)		    vBIT(val, 5, 3)
#define	VXGE_HAL_XMAC_STATS_SYS_CMD_STROBE		    mBIT(15)
#define	VXGE_HAL_XMAC_STATS_SYS_CMD_LOC_SEL(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_XMAC_STATS_SYS_CMD_OFFSET_SEL(val)	    vBIT(val, 32, 8)
/* 0x01f60 */	u64	xmac_stats_sys_data;
#define	VXGE_HAL_XMAC_STATS_SYS_DATA_XSMGR_DATA(val)	    vBIT(val, 0, 64)
	u8	unused01f80[0x01f80 - 0x01f68];

/* 0x01f80 */	u64	asic_ntwk_ctrl;
#define	VXGE_HAL_ASIC_NTWK_CTRL_REQ_TEST_NTWK		    mBIT(3)
#define	VXGE_HAL_ASIC_NTWK_CTRL_PORT0_REQ_TEST_PORT	    mBIT(11)
#define	VXGE_HAL_ASIC_NTWK_CTRL_PORT1_REQ_TEST_PORT	    mBIT(15)
/* 0x01f88 */	u64	asic_ntwk_cfg_show_port_info;
#define	VXGE_HAL_ASIC_NTWK_CFG_SHOW_PORT_INFO_VP(n)	    mBIT(n)
/* 0x01f90 */	u64	asic_ntwk_cfg_port_num;
#define	VXGE_HAL_ASIC_NTWK_CFG_PORT_NUM_VP(n)		    mBIT(n)
/* 0x01f98 */	u64	xmac_cfg_port[3];
#define	VXGE_HAL_XMAC_CFG_PORT_XGMII_LOOPBACK		    mBIT(3)
#define	VXGE_HAL_XMAC_CFG_PORT_XGMII_REVERSE_LOOPBACK	    mBIT(7)
#define	VXGE_HAL_XMAC_CFG_PORT_XGMII_TX_BEHAV		    mBIT(11)
#define	VXGE_HAL_XMAC_CFG_PORT_XGMII_RX_BEHAV		    mBIT(15)
/* 0x01fb0 */	u64	xmac_station_addr_port[2];
#define	VXGE_HAL_XMAC_STATION_ADDR_PORT_MAC_ADDR(val)	    vBIT(val, 0, 48)
/* 0x01fc0 */	u64	asic_led_activity_ctrl_port[3];
#define	VXGE_HAL_ASIC_LED_ACTIVITY_CTRL_PORT_TX_ACT_PULSE_EXTEND mBIT(11)
#define	VXGE_HAL_ASIC_LED_ACTIVITY_CTRL_PORT_RX_ACT_PULSE_EXTEND mBIT(15)
#define	VXGE_HAL_ASIC_LED_ACTIVITY_CTRL_PORT_COMBINE_TXRX   mBIT(35)
	u8	unused02020[0x02020 - 0x01fd8];

/* 0x02020 */	u64	lag_cfg;
#define	VXGE_HAL_LAG_CFG_EN				    mBIT(3)
#define	VXGE_HAL_LAG_CFG_MODE(val)			    vBIT(val, 6, 2)
#define	VXGE_HAL_LAG_CFG_TX_DISCARD_BEHAV		    mBIT(11)
#define	VXGE_HAL_LAG_CFG_RX_DISCARD_BEHAV		    mBIT(15)
#define	VXGE_HAL_LAG_CFG_PREF_INDIV_PORT_NUM		    mBIT(19)
/* 0x02028 */	u64	lag_status;
#define	VXGE_HAL_LAG_STATUS_XLCM_WAITING_TO_FAILBACK	    mBIT(3)
#define	VXGE_HAL_LAG_STATUS_XLCM_TIMER_VAL_COLD_FAILOVER(val) vBIT(val, 8, 8)
/* 0x02030 */	u64	lag_active_passive_cfg;
#define	VXGE_HAL_LAG_ACTIVE_PASSIVE_CFG_HOT_STANDBY	    mBIT(3)
#define	VXGE_HAL_LAG_ACTIVE_PASSIVE_CFG_LACP_DECIDES	    mBIT(7)
#define	VXGE_HAL_LAG_ACTIVE_PASSIVE_CFG_PREF_ACTIVE_PORT_NUM mBIT(11)
#define	VXGE_HAL_LAG_ACTIVE_PASSIVE_CFG_AUTO_FAILBACK	    mBIT(15)
#define	VXGE_HAL_LAG_ACTIVE_PASSIVE_CFG_FAILBACK_EN	    mBIT(19)
#define	VXGE_HAL_LAG_ACTIVE_PASSIVE_CFG_COLD_FAILOVER_TIMEOUT(val)\
							    vBIT(val, 32, 16)
	u8	unused02040[0x02040 - 0x02038];

/* 0x02040 */	u64	lag_lacp_cfg;
#define	VXGE_HAL_LAG_LACP_CFG_EN			    mBIT(3)
#define	VXGE_HAL_LAG_LACP_CFG_LACP_BEGIN		    mBIT(7)
#define	VXGE_HAL_LAG_LACP_CFG_DISCARD_LACP		    mBIT(11)
#define	VXGE_HAL_LAG_LACP_CFG_LIBERAL_LEN_CHK		    mBIT(15)
/* 0x02048 */	u64	lag_timer_cfg_1;
#define	VXGE_HAL_LAG_TIMER_CFG_1_FAST_PER(val)		    vBIT(val, 0, 16)
#define	VXGE_HAL_LAG_TIMER_CFG_1_SLOW_PER(val)		    vBIT(val, 16, 16)
#define	VXGE_HAL_LAG_TIMER_CFG_1_SHORT_TIMEOUT(val)	    vBIT(val, 32, 16)
#define	VXGE_HAL_LAG_TIMER_CFG_1_LONG_TIMEOUT(val)	    vBIT(val, 48, 16)
/* 0x02050 */	u64	lag_timer_cfg_2;
#define	VXGE_HAL_LAG_TIMER_CFG_2_CHURN_DET(val)		    vBIT(val, 0, 16)
#define	VXGE_HAL_LAG_TIMER_CFG_2_AGGR_WAIT(val)		    vBIT(val, 16, 16)
#define	VXGE_HAL_LAG_TIMER_CFG_2_SHORT_TIMER_SCALE(val)	    vBIT(val, 32, 16)
#define	VXGE_HAL_LAG_TIMER_CFG_2_LONG_TIMER_SCALE(val)	    vBIT(val, 48, 16)
/* 0x02058 */	u64	lag_sys_id;
#define	VXGE_HAL_LAG_SYS_ID_ADDR(val)			    vBIT(val, 0, 48)
#define	VXGE_HAL_LAG_SYS_ID_USE_PORT_ADDR		    mBIT(51)
#define	VXGE_HAL_LAG_SYS_ID_ADDR_SEL			    mBIT(55)
/* 0x02060 */	u64	lag_sys_cfg;
#define	VXGE_HAL_LAG_SYS_CFG_SYS_PRI(val)		    vBIT(val, 0, 16)
	u8	unused02070[0x02070 - 0x02068];

/* 0x02070 */	u64	lag_aggr_addr_cfg[2];
#define	VXGE_HAL_LAG_AGGR_ADDR_CFG_ADDR(val)		    vBIT(val, 0, 48)
#define	VXGE_HAL_LAG_AGGR_ADDR_CFG_USE_PORT_ADDR	    mBIT(51)
#define	VXGE_HAL_LAG_AGGR_ADDR_CFG_ADDR_SEL		    mBIT(55)
/* 0x02080 */	u64	lag_aggr_id_cfg[2];
#define	VXGE_HAL_LAG_AGGR_ID_CFG_ID(val)		    vBIT(val, 0, 16)
/* 0x02090 */	u64	lag_aggr_admin_key[2];
#define	VXGE_HAL_LAG_AGGR_ADMIN_KEY_KEY(val)		    vBIT(val, 0, 16)
/* 0x020a0 */	u64	lag_aggr_alt_admin_key;
#define	VXGE_HAL_LAG_AGGR_ALT_ADMIN_KEY_KEY(val)	    vBIT(val, 0, 16)
#define	VXGE_HAL_LAG_AGGR_ALT_ADMIN_KEY_ALT_AGGR	    mBIT(19)
/* 0x020a8 */	u64	lag_aggr_oper_key[2];
#define	VXGE_HAL_LAG_AGGR_OPER_KEY_LAGC_KEY(val)	    vBIT(val, 0, 16)
/* 0x020b8 */	u64	lag_aggr_partner_sys_id[2];
#define	VXGE_HAL_LAG_AGGR_PARTNER_SYS_ID_LAGC_ADDR(val)	    vBIT(val, 0, 48)
/* 0x020c8 */	u64	lag_aggr_partner_info[2];
#define	VXGE_HAL_LAG_AGGR_PARTNER_INFO_LAGC_SYS_PRI(val)    vBIT(val, 0, 16)
#define	VXGE_HAL_LAG_AGGR_PARTNER_INFO_LAGC_OPER_KEY(val)   vBIT(val, 16, 16)
/* 0x020d8 */	u64	lag_aggr_state[2];
#define	VXGE_HAL_LAG_AGGR_STATE_LAGC_TX			    mBIT(3)
#define	VXGE_HAL_LAG_AGGR_STATE_LAGC_RX			    mBIT(7)
#define	VXGE_HAL_LAG_AGGR_STATE_LAGC_READY		    mBIT(11)
#define	VXGE_HAL_LAG_AGGR_STATE_LAGC_INDIVIDUAL		    mBIT(15)
	u8	unused020f0[0x020f0 - 0x020e8];

/* 0x020f0 */	u64	lag_port_cfg[2];
#define	VXGE_HAL_LAG_PORT_CFG_EN			    mBIT(3)
#define	VXGE_HAL_LAG_PORT_CFG_DISCARD_SLOW_PROTO	    mBIT(7)
#define	VXGE_HAL_LAG_PORT_CFG_HOST_CHOSEN_AGGR		    mBIT(11)
#define	VXGE_HAL_LAG_PORT_CFG_DISCARD_UNKNOWN_SLOW_PROTO    mBIT(15)
/* 0x02100 */	u64	lag_port_actor_admin_cfg[2];
#define	VXGE_HAL_LAG_PORT_ACTOR_ADMIN_CFG_PORT_NUM(val)	    vBIT(val, 0, 16)
#define	VXGE_HAL_LAG_PORT_ACTOR_ADMIN_CFG_PORT_PRI(val)	    vBIT(val, 16, 16)
#define	VXGE_HAL_LAG_PORT_ACTOR_ADMIN_CFG_KEY_10G(val)	    vBIT(val, 32, 16)
#define	VXGE_HAL_LAG_PORT_ACTOR_ADMIN_CFG_KEY_1G(val)	    vBIT(val, 48, 16)
/* 0x02110 */	u64	lag_port_actor_admin_state[2];
#define	VXGE_HAL_LAG_PORT_ACTOR_ADMIN_STATE_LACP_ACTIVITY   mBIT(3)
#define	VXGE_HAL_LAG_PORT_ACTOR_ADMIN_STATE_LACP_TIMEOUT    mBIT(7)
#define	VXGE_HAL_LAG_PORT_ACTOR_ADMIN_STATE_AGGREGATION	    mBIT(11)
#define	VXGE_HAL_LAG_PORT_ACTOR_ADMIN_STATE_SYNCHRONIZATION mBIT(15)
#define	VXGE_HAL_LAG_PORT_ACTOR_ADMIN_STATE_COLLECTING	    mBIT(19)
#define	VXGE_HAL_LAG_PORT_ACTOR_ADMIN_STATE_DISTRIBUTING    mBIT(23)
#define	VXGE_HAL_LAG_PORT_ACTOR_ADMIN_STATE_DEFAULTED	    mBIT(27)
#define	VXGE_HAL_LAG_PORT_ACTOR_ADMIN_STATE_EXPIRED	    mBIT(31)
/* 0x02120 */	u64	lag_port_partner_admin_sys_id[2];
#define	VXGE_HAL_LAG_PORT_PARTNER_ADMIN_SYS_ID_ADDR(val)    vBIT(val, 0, 48)
/* 0x02130 */	u64	lag_port_partner_admin_cfg[2];
#define	VXGE_HAL_LAG_PORT_PARTNER_ADMIN_CFG_SYS_PRI(val)    vBIT(val, 0, 16)
#define	VXGE_HAL_LAG_PORT_PARTNER_ADMIN_CFG_KEY(val)	    vBIT(val, 16, 16)
#define	VXGE_HAL_LAG_PORT_PARTNER_ADMIN_CFG_PORT_NUM(val)   vBIT(val, 32, 16)
#define	VXGE_HAL_LAG_PORT_PARTNER_ADMIN_CFG_PORT_PRI(val)   vBIT(val, 48, 16)
/* 0x02140 */	u64	lag_port_partner_admin_state[2];
#define	VXGE_HAL_LAG_PORT_PARTNER_ADMIN_STATE_LACP_ACTIVITY mBIT(3)
#define	VXGE_HAL_LAG_PORT_PARTNER_ADMIN_STATE_LACP_TIMEOUT  mBIT(7)
#define	VXGE_HAL_LAG_PORT_PARTNER_ADMIN_STATE_AGGREGATION   mBIT(11)
#define	VXGE_HAL_LAG_PORT_PARTNER_ADMIN_STATE_SYNCHRONIZATION mBIT(15)
#define	VXGE_HAL_LAG_PORT_PARTNER_ADMIN_STATE_COLLECTING    mBIT(19)
#define	VXGE_HAL_LAG_PORT_PARTNER_ADMIN_STATE_DISTRIBUTING  mBIT(23)
#define	VXGE_HAL_LAG_PORT_PARTNER_ADMIN_STATE_DEFAULTED	    mBIT(27)
#define	VXGE_HAL_LAG_PORT_PARTNER_ADMIN_STATE_EXPIRED	    mBIT(31)
/* 0x02150 */	u64	lag_port_to_aggr[2];
#define	VXGE_HAL_LAG_PORT_TO_AGGR_LAGC_AGGR_ID(val)	    vBIT(val, 0, 16)
#define	VXGE_HAL_LAG_PORT_TO_AGGR_LAGC_AGGR_VLD_ID	    mBIT(19)
/* 0x02160 */	u64	lag_port_actor_oper_key[2];
#define	VXGE_HAL_LAG_PORT_ACTOR_OPER_KEY_LAGC_KEY(val)	    vBIT(val, 0, 16)
/* 0x02170 */	u64	lag_port_actor_oper_state[2];
#define	VXGE_HAL_LAG_PORT_ACTOR_OPER_STATE_LAGC_LACP_ACTIVITY	mBIT(3)
#define	VXGE_HAL_LAG_PORT_ACTOR_OPER_STATE_LAGC_LACP_TIMEOUT	mBIT(7)
#define	VXGE_HAL_LAG_PORT_ACTOR_OPER_STATE_LAGC_AGGREGATION	mBIT(11)
#define	VXGE_HAL_LAG_PORT_ACTOR_OPER_STATE_LAGC_SYNCHRONIZATION	mBIT(15)
#define	VXGE_HAL_LAG_PORT_ACTOR_OPER_STATE_LAGC_COLLECTING	mBIT(19)
#define	VXGE_HAL_LAG_PORT_ACTOR_OPER_STATE_LAGC_DISTRIBUTING	mBIT(23)
#define	VXGE_HAL_LAG_PORT_ACTOR_OPER_STATE_LAGC_DEFAULTED	mBIT(27)
#define	VXGE_HAL_LAG_PORT_ACTOR_OPER_STATE_LAGC_EXPIRED		mBIT(31)
/* 0x02180 */	u64	lag_port_partner_oper_sys_id[2];
#define	VXGE_HAL_LAG_PORT_PARTNER_OPER_SYS_ID_LAGC_ADDR(val) vBIT(val, 0, 48)
/* 0x02190 */	u64	lag_port_partner_oper_info[2];
#define	VXGE_HAL_LAG_PORT_PARTNER_OPER_INFO_LAGC_SYS_PRI(val) vBIT(val, 0, 16)
#define	VXGE_HAL_LAG_PORT_PARTNER_OPER_INFO_LAGC_KEY(val)   vBIT(val, 16, 16)
#define	VXGE_HAL_LAG_PORT_PARTNER_OPER_INFO_LAGC_PORT_NUM(val) vBIT(val, 32, 16)
#define	VXGE_HAL_LAG_PORT_PARTNER_OPER_INFO_LAGC_PORT_PRI(val) vBIT(val, 48, 16)
/* 0x021a0 */	u64	lag_port_partner_oper_state[2];
#define	VXGE_HAL_LAG_PORT_PARTNER_OPER_STATE_LAGC_LACP_ACTIVITY	mBIT(3)
#define	VXGE_HAL_LAG_PORT_PARTNER_OPER_STATE_LAGC_LACP_TIMEOUT	mBIT(7)
#define	VXGE_HAL_LAG_PORT_PARTNER_OPER_STATE_LAGC_AGGREGATION	mBIT(11)
#define	VXGE_HAL_LAG_PORT_PARTNER_OPER_STATE_LAGC_SYNCHRONIZATION mBIT(15)
#define	VXGE_HAL_LAG_PORT_PARTNER_OPER_STATE_LAGC_COLLECTING	mBIT(19)
#define	VXGE_HAL_LAG_PORT_PARTNER_OPER_STATE_LAGC_DISTRIBUTING	mBIT(23)
#define	VXGE_HAL_LAG_PORT_PARTNER_OPER_STATE_LAGC_DEFAULTED	mBIT(27)
#define	VXGE_HAL_LAG_PORT_PARTNER_OPER_STATE_LAGC_EXPIRED	mBIT(31)
/* 0x021b0 */	u64	lag_port_state_vars[2];
#define	VXGE_HAL_LAG_PORT_STATE_VARS_LAGC_READY		    mBIT(3)
#define	VXGE_HAL_LAG_PORT_STATE_VARS_LAGC_SELECTED(val)	    vBIT(val, 6, 2)
#define	VXGE_HAL_LAG_PORT_STATE_VARS_LAGC_AGGR_NUM	    mBIT(11)
#define	VXGE_HAL_LAG_PORT_STATE_VARS_LAGC_PORT_MOVED	    mBIT(15)
#define	VXGE_HAL_LAG_PORT_STATE_VARS_LAGC_PORT_ENABLED	    mBIT(18)
#define	VXGE_HAL_LAG_PORT_STATE_VARS_LAGC_PORT_DISABLED	    mBIT(19)
#define	VXGE_HAL_LAG_PORT_STATE_VARS_LAGC_NTT		    mBIT(23)
#define	VXGE_HAL_LAG_PORT_STATE_VARS_LAGC_ACTOR_CHURN	    mBIT(27)
#define	VXGE_HAL_LAG_PORT_STATE_VARS_LAGC_PARTNER_CHURN	    mBIT(31)
#define	VXGE_HAL_LAG_PORT_STATE_VARS_LAGC_ACTOR_INFO_LEN_MISMATCH mBIT(32)
#define	VXGE_HAL_LAG_PORT_STATE_VARS_LAGC_PARTNER_INFO_LEN_MISMATCH mBIT(33)
#define	VXGE_HAL_LAG_PORT_STATE_VARS_LAGC_COLL_INFO_LEN_MISMATCH mBIT(34)
#define	VXGE_HAL_LAG_PORT_STATE_VARS_LAGC_TERM_INFO_LEN_MISMATCH mBIT(35)
#define	VXGE_HAL_LAG_PORT_STATE_VARS_LAGC_RX_FSM_STATE(val) vBIT(val, 37, 3)
#define	VXGE_HAL_LAG_PORT_STATE_VARS_LAGC_MUX_FSM_STATE(val) vBIT(val, 41, 3)
#define	VXGE_HAL_LAG_PORT_STATE_VARS_LAGC_MUX_REASON(val)   vBIT(val, 44, 4)
#define	VXGE_HAL_LAG_PORT_STATE_VARS_LAGC_ACTOR_CHURN_STATE mBIT(54)
#define	VXGE_HAL_LAG_PORT_STATE_VARS_LAGC_PARTNER_CHURN_STATE mBIT(55)
#define	VXGE_HAL_LAG_PORT_STATE_VARS_LAGC_ACTOR_CHURN_COUNT(val)\
							    vBIT(val, 56, 4)
#define	VXGE_HAL_LAG_PORT_STATE_VARS_LAGC_PARTNER_CHURN_COUNT(val)\
							    vBIT(val, 60, 4)
/* 0x021c0 */	u64	lag_port_timer_cntr[2];
#define	VXGE_HAL_LAG_PORT_TIMER_CNTR_LAGC_CURRENT_while (val) vBIT(val, 0, 8)
#define	VXGE_HAL_LAG_PORT_TIMER_CNTR_LAGC_PERIODIC_while (val) vBIT(val, 8, 8)
#define	VXGE_HAL_LAG_PORT_TIMER_CNTR_LAGC_WAIT_while (val)  vBIT(val, 16, 8)
#define	VXGE_HAL_LAG_PORT_TIMER_CNTR_LAGC_TX_LACP(val)	    vBIT(val, 24, 8)
#define	VXGE_HAL_LAG_PORT_TIMER_CNTR_LAGC_ACTOR_SYNC_TRANSITION_COUNT(val)\
							    vBIT(val, 32, 8)
#define	VXGE_HAL_LAG_PORT_TIMER_CNTR_LAGC_PARTNER_SYNC_TRANSITION_COUNT(val)\
							    vBIT(val, 40, 8)
#define	VXGE_HAL_LAG_PORT_TIMER_CNTR_LAGC_ACTOR_CHANGE_COUNT(val)\
							    vBIT(val, 48, 8)
#define	VXGE_HAL_LAG_PORT_TIMER_CNTR_LAGC_PARTNER_CHANGE_COUNT(val)\
							    vBIT(val, 56, 8)
	u8	unused021e0[0x021e0 - 0x021d0];

/* 0x021e0 */	u64	transceiver_reset_port[2];
#define	VXGE_HAL_TRANSCEIVER_RESET_PORT_TCVR_RESET(val)	    vBIT(val, 0, 8)
/* 0x021f0 */	u64	transceiver_ctrl_port[2];
#define	VXGE_HAL_TRANSCEIVER_CTRL_PORT_TCVR_TX_ON	    mBIT(3)
/* 0x02200 */	u64	asic_gpio_ctrl;
#define	VXGE_HAL_ASIC_GPIO_CTRL_XMACJ_GPIO_DATA_IN(n)	    mBIT(n)
#define	VXGE_HAL_ASIC_GPIO_CTRL_GPIO_DATA_OUT(n)	    mBIT(n)
#define	VXGE_HAL_ASIC_GPIO_CTRL_GPIO_OUT_EN(n)		    mBIT(n)
/* 0x02208 */	u64	asic_led_beacon_ctrl;
#define	VXGE_HAL_ASIC_LED_BEACON_CTRL_PORT0_LINK_INVERT	    mBIT(3)
#define	VXGE_HAL_ASIC_LED_BEACON_CTRL_PORT0_10G_INVERT	    mBIT(7)
#define	VXGE_HAL_ASIC_LED_BEACON_CTRL_PORT0_TX_ACT_INVERT   mBIT(11)
#define	VXGE_HAL_ASIC_LED_BEACON_CTRL_PORT0_RX_ACT_INVERT   mBIT(15)
#define	VXGE_HAL_ASIC_LED_BEACON_CTRL_PORT1_LINK_INVERT	    mBIT(19)
#define	VXGE_HAL_ASIC_LED_BEACON_CTRL_PORT1_10G_INVERT	    mBIT(23)
#define	VXGE_HAL_ASIC_LED_BEACON_CTRL_PORT1_TX_ACT_INVERT   mBIT(27)
#define	VXGE_HAL_ASIC_LED_BEACON_CTRL_PORT1_RX_ACT_INVERT   mBIT(31)
#define	VXGE_HAL_ASIC_LED_BEACON_CTRL_AUX_LED1_INVERT	    mBIT(35)
#define	VXGE_HAL_ASIC_LED_BEACON_CTRL_AUX_LED2_INVERT	    mBIT(39)
/* 0x02210 */	u64	asic_led_ctrl0;
#define	VXGE_HAL_ASIC_LED_CTRL0_PORT0_LINK_ON		    mBIT(3)
#define	VXGE_HAL_ASIC_LED_CTRL0_PORT0_10G_ON		    mBIT(7)
#define	VXGE_HAL_ASIC_LED_CTRL0_PORT1_LINK_ON		    mBIT(19)
#define	VXGE_HAL_ASIC_LED_CTRL0_PORT1_10G_ON		    mBIT(23)
#define	VXGE_HAL_ASIC_LED_CTRL0_AUX_LED1_ON		    mBIT(35)
#define	VXGE_HAL_ASIC_LED_CTRL0_AUX_LED2_ON		    mBIT(39)
/* 0x02218 */	u64	asic_led_ctrl1;
#define	VXGE_HAL_ASIC_LED_CTRL1_PORT0_LINK_SOURCE(val)	    vBIT(val, 2, 2)
#define	VXGE_HAL_ASIC_LED_CTRL1_PORT0_10G_SOURCE(val)	    vBIT(val, 6, 2)
#define	VXGE_HAL_ASIC_LED_CTRL1_PORT1_LINK_SOURCE(val)	    vBIT(val, 10, 2)
#define	VXGE_HAL_ASIC_LED_CTRL1_PORT1_10G_SOURCE(val)	    vBIT(val, 14, 2)
#define	VXGE_HAL_ASIC_LED_CTRL1_PORT0_LINK_PULSE_EXTEND	    mBIT(19)
#define	VXGE_HAL_ASIC_LED_CTRL1_PORT0_10G_PULSE_EXTEND	    mBIT(23)
#define	VXGE_HAL_ASIC_LED_CTRL1_PORT1_LINK_PULSE_EXTEND	    mBIT(27)
#define	VXGE_HAL_ASIC_LED_CTRL1_PORT1_10G_PULSE_EXTEND	    mBIT(31)
#define	VXGE_HAL_ASIC_LED_CTRL1_PORT0_LINK_EXT_SEL(val)	    vBIT(val, 32, 4)
#define	VXGE_HAL_ASIC_LED_CTRL1_PORT0_10G_EXT_SEL(val)	    vBIT(val, 36, 4)
#define	VXGE_HAL_ASIC_LED_CTRL1_PORT1_LINK_EXT_SEL(val)	    vBIT(val, 40, 4)
#define	VXGE_HAL_ASIC_LED_CTRL1_PORT1_10G_EXT_SEL(val)	    vBIT(val, 44, 4)
#define	VXGE_HAL_ASIC_LED_CTRL1_PORT0_LINK_INT_SEL(val)	    vBIT(val, 48, 4)
#define	VXGE_HAL_ASIC_LED_CTRL1_PORT0_10G_INT_SEL(val)	    vBIT(val, 52, 4)
#define	VXGE_HAL_ASIC_LED_CTRL1_PORT1_LINK_INT_SEL(val)	    vBIT(val, 56, 4)
#define	VXGE_HAL_ASIC_LED_CTRL1_PORT1_10G_INT_SEL(val)	    vBIT(val, 60, 4)
/* 0x02220 */	u64	asic_led_debug_sel;
#define	VXGE_HAL_ASIC_LED_DEBUG_SEL_XGMAC_SEL0(val)	    vBIT(val, 2, 6)
#define	VXGE_HAL_ASIC_LED_DEBUG_SEL_XGMAC_SEL1(val)	    vBIT(val, 10, 6)
#define	VXGE_HAL_ASIC_LED_DEBUG_SEL_XGMAC_SEL2(val)	    vBIT(val, 18, 6)
#define	VXGE_HAL_ASIC_LED_DEBUG_SEL_XGMAC_SEL3(val)	    vBIT(val, 26, 6)
	u8	unused02300[0x02300 - 0x02228];

/* 0x02300 */	u64	usdc_sgrp_partition;
#define	VXGE_HAL_USDC_SGRP_PARTITION_ENABLE		    mBIT(7)
/* 0x02308 */	u64	usdc_ugrp_priority_0;
#define	VXGE_HAL_USDC_UGRP_PRIORITY_0_NUMBER_0(val)	    vBIT(val, 3, 5)
/* 0x02310 */	u64	usdc_ugrp_priority_1;
#define	VXGE_HAL_USDC_UGRP_PRIORITY_1_NUMBER_1(val)	    vBIT(val, 3, 5)
/* 0x02318 */	u64	usdc_ugrp_priority_2;
#define	VXGE_HAL_USDC_UGRP_PRIORITY_2_NUMBER_2(val)	    vBIT(val, 3, 5)
/* 0x02320 */	u64	usdc_ugrp_priority_3;
#define	VXGE_HAL_USDC_UGRP_PRIORITY_3_NUMBER_3(val)	    vBIT(val, 3, 5)
/* 0x02328 */	u64	usdc_ugrp_priority_4;
#define	VXGE_HAL_USDC_UGRP_PRIORITY_4_NUMBER_4(val)	    vBIT(val, 3, 5)
/* 0x02330 */	u64	usdc_ugrp_priority_5;
#define	VXGE_HAL_USDC_UGRP_PRIORITY_5_NUMBER_5(val)	    vBIT(val, 3, 5)
/* 0x02338 */	u64	usdc_ugrp_priority_6;
#define	VXGE_HAL_USDC_UGRP_PRIORITY_6_NUMBER_6(val)	    vBIT(val, 3, 5)
/* 0x02340 */	u64	usdc_ugrp_priority_7;
#define	VXGE_HAL_USDC_UGRP_PRIORITY_7_NUMBER_7(val)	    vBIT(val, 3, 5)
/* 0x02348 */	u64	usdc_ugrp_priority_8;
#define	VXGE_HAL_USDC_UGRP_PRIORITY_8_NUMBER_8(val)	    vBIT(val, 3, 5)
/* 0x02350 */	u64	usdc_ugrp_priority_9;
#define	VXGE_HAL_USDC_UGRP_PRIORITY_9_NUMBER_9(val)	    vBIT(val, 3, 5)
/* 0x02358 */	u64	usdc_ugrp_priority_10;
#define	VXGE_HAL_USDC_UGRP_PRIORITY_10_NUMBER_10(val)	    vBIT(val, 3, 5)
/* 0x02360 */	u64	usdc_ugrp_priority_11;
#define	VXGE_HAL_USDC_UGRP_PRIORITY_11_NUMBER_11(val)	    vBIT(val, 3, 5)
/* 0x02368 */	u64	usdc_ugrp_priority_12;
#define	VXGE_HAL_USDC_UGRP_PRIORITY_12_NUMBER_12(val)	    vBIT(val, 3, 5)
/* 0x02370 */	u64	usdc_ugrp_priority_13;
#define	VXGE_HAL_USDC_UGRP_PRIORITY_13_NUMBER_13(val)	    vBIT(val, 3, 5)
/* 0x02378 */	u64	usdc_ugrp_priority_14;
#define	VXGE_HAL_USDC_UGRP_PRIORITY_14_NUMBER_14(val)	    vBIT(val, 3, 5)
/* 0x02380 */	u64	usdc_ugrp_priority_15;
#define	VXGE_HAL_USDC_UGRP_PRIORITY_15_NUMBER_15(val)	    vBIT(val, 3, 5)
/* 0x02388 */	u64	usdc_ugrp_priority_16;
#define	VXGE_HAL_USDC_UGRP_PRIORITY_16_NUMBER_16(val)	    vBIT(val, 3, 5)
	u8	unused02398[0x02398 - 0x02390];

/* 0x02398 */	u64	ugrp_htn_wrr_priority_0;
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_0_NUMBER_0(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_0_NUMBER_1(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_0_NUMBER_2(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_0_NUMBER_3(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_0_NUMBER_4(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_0_NUMBER_5(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_0_NUMBER_6(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_0_NUMBER_7(val)	    vBIT(val, 59, 5)
/* 0x023a0 */	u64	ugrp_htn_wrr_priority_1;
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_1_NUMBER_8(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_1_NUMBER_9(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_1_NUMBER_10(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_1_NUMBER_11(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_1_NUMBER_12(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_1_NUMBER_13(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_1_NUMBER_14(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_1_NUMBER_15(val)	    vBIT(val, 59, 5)
/* 0x023a8 */	u64	ugrp_htn_wrr_priority_2;
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_2_NUMBER_16(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_2_NUMBER_17(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_2_NUMBER_18(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_2_NUMBER_19(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_2_NUMBER_20(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_2_NUMBER_21(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_2_NUMBER_22(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_2_NUMBER_23(val)	    vBIT(val, 59, 5)
/* 0x023b0 */	u64	ugrp_htn_wrr_priority_3;
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_3_NUMBER_24(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_3_NUMBER_25(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_3_NUMBER_26(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_3_NUMBER_27(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_3_NUMBER_28(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_3_NUMBER_29(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_3_NUMBER_30(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_3_NUMBER_31(val)	    vBIT(val, 59, 5)
/* 0x023b8 */	u64	ugrp_htn_wrr_priority_4;
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_4_NUMBER_32(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_4_NUMBER_33(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_4_NUMBER_34(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_4_NUMBER_35(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_4_NUMBER_36(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_4_NUMBER_37(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_4_NUMBER_38(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_4_NUMBER_39(val)	    vBIT(val, 59, 5)
/* 0x023c0 */	u64	ugrp_htn_wrr_priority_5;
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_5_NUMBER_40(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_5_NUMBER_41(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_5_NUMBER_42(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_5_NUMBER_43(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_5_NUMBER_44(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_5_NUMBER_45(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_5_NUMBER_46(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_5_NUMBER_47(val)	    vBIT(val, 59, 5)
/* 0x023c8 */	u64	ugrp_htn_wrr_priority_6;
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_6_NUMBER_48(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_6_NUMBER_49(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_6_NUMBER_50(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_6_NUMBER_51(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_6_NUMBER_52(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_6_NUMBER_53(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_6_NUMBER_54(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_6_NUMBER_55(val)	    vBIT(val, 59, 5)
/* 0x023d0 */	u64	ugrp_htn_wrr_priority_7;
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_7_NUMBER_56(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_7_NUMBER_57(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_7_NUMBER_58(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_7_NUMBER_59(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_7_NUMBER_60(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_7_NUMBER_61(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_7_NUMBER_62(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_7_NUMBER_63(val)	    vBIT(val, 59, 5)
/* 0x023d8 */	u64	ugrp_htn_wrr_priority_8;
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_8_NUMBER_64(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_8_NUMBER_65(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_8_NUMBER_66(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_8_NUMBER_67(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_8_NUMBER_68(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_8_NUMBER_69(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_8_NUMBER_70(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_8_NUMBER_71(val)	    vBIT(val, 59, 5)
/* 0x023e0 */	u64	ugrp_htn_wrr_priority_9;
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_9_NUMBER_72(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_9_NUMBER_73(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_9_NUMBER_74(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_9_NUMBER_75(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_9_NUMBER_76(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_9_NUMBER_77(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_9_NUMBER_78(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_9_NUMBER_79(val)	    vBIT(val, 59, 5)
/* 0x023e8 */	u64	ugrp_htn_wrr_priority_10;
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_10_NUMBER_80(val)    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_10_NUMBER_81(val)    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_10_NUMBER_82(val)    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_10_NUMBER_83(val)    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_10_NUMBER_84(val)    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_10_NUMBER_85(val)    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_10_NUMBER_86(val)    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_10_NUMBER_87(val)    vBIT(val, 59, 5)
/* 0x023f0 */	u64	ugrp_htn_wrr_priority_11;
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_11_NUMBER_88(val)    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_11_NUMBER_89(val)    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_11_NUMBER_90(val)    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_11_NUMBER_91(val)    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_11_NUMBER_92(val)    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_11_NUMBER_93(val)    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_11_NUMBER_94(val)    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_11_NUMBER_95(val)    vBIT(val, 59, 5)
/* 0x023f8 */	u64	ugrp_htn_wrr_priority_12;
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_12_NUMBER_96(val)    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_12_NUMBER_97(val)    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_12_NUMBER_98(val)    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_12_NUMBER_99(val)    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_12_NUMBER_100(val)   vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_12_NUMBER_101(val)   vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_12_NUMBER_102(val)   vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_12_NUMBER_103(val)   vBIT(val, 59, 5)
/* 0x02400 */	u64	ugrp_htn_wrr_priority_13;
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_13_NUMBER_104(val)   vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_13_NUMBER_105(val)   vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_13_NUMBER_106(val)   vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_13_NUMBER_107(val)   vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_13_NUMBER_108(val)   vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_13_NUMBER_109(val)   vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_13_NUMBER_110(val)   vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_13_NUMBER_111(val)   vBIT(val, 59, 5)
/* 0x02408 */	u64	ugrp_htn_wrr_priority_14;
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_14_NUMBER_112(val)   vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_14_NUMBER_113(val)   vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_14_NUMBER_114(val)   vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_14_NUMBER_115(val)   vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_14_NUMBER_116(val)   vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_14_NUMBER_117(val)   vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_14_NUMBER_118(val)   vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_14_NUMBER_119(val)   vBIT(val, 59, 5)
/* 0x02410 */	u64	ugrp_htn_wrr_priority_15;
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_15_NUMBER_120(val)   vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_15_NUMBER_121(val)   vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_15_NUMBER_122(val)   vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_15_NUMBER_123(val)   vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_15_NUMBER_124(val)   vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_15_NUMBER_125(val)   vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_15_NUMBER_126(val)   vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_15_NUMBER_127(val)   vBIT(val, 59, 5)
/* 0x02418 */	u64	ugrp_htn_wrr_priority_16;
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_16_NUMBER_128(val)   vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_16_NUMBER_129(val)   vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_16_NUMBER_130(val)   vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_16_NUMBER_131(val)   vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_16_NUMBER_132(val)   vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_16_NUMBER_133(val)   vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_16_NUMBER_134(val)   vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_16_NUMBER_135(val)   vBIT(val, 59, 5)
/* 0x02420 */	u64	ugrp_htn_wrr_priority_17;
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_17_NUMBER_136(val)   vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_17_NUMBER_137(val)   vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_17_NUMBER_138(val)   vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_17_NUMBER_139(val)   vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_17_NUMBER_140(val)   vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_17_NUMBER_141(val)   vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_17_NUMBER_142(val)   vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_17_NUMBER_143(val)   vBIT(val, 59, 5)
/* 0x02428 */	u64	ugrp_htn_wrr_priority_18;
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_18_NUMBER_144(val)   vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_18_NUMBER_145(val)   vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_18_NUMBER_146(val)   vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_18_NUMBER_147(val)   vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_18_NUMBER_148(val)   vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_18_NUMBER_149(val)   vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_18_NUMBER_150(val)   vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_18_NUMBER_151(val)   vBIT(val, 59, 5)
/* 0x02430 */	u64	ugrp_htn_wrr_priority_19;
#define	VXGE_HAL_UGRP_HTN_WRR_PRIORITY_19_NUMBER_152(val)   vBIT(val, 3, 5)
/* 0x02438 */	u64	usdc_vplane[17];
#define	VXGE_HAL_USDC_VPLANE_SGRP_OWN(val)		    vBIT(val, 0, 32)
	u8	unused024c8[0x024c8 - 0x024c0];

/* 0x024c8 */	u64	usdc_sgrp_assignment;
#define	VXGE_HAL_USDC_SGRP_ASSIGNMENT_USDC_SGRP_0_ERR	    mBIT(0)
#define	VXGE_HAL_USDC_SGRP_ASSIGNMENT_USDC_SGRP_1_ERR	    mBIT(1)
#define	VXGE_HAL_USDC_SGRP_ASSIGNMENT_USDC_SGRP_2_ERR	    mBIT(2)
#define	VXGE_HAL_USDC_SGRP_ASSIGNMENT_USDC_SGRP_3_ERR	    mBIT(3)
#define	VXGE_HAL_USDC_SGRP_ASSIGNMENT_USDC_SGRP_4_ERR	    mBIT(4)
#define	VXGE_HAL_USDC_SGRP_ASSIGNMENT_USDC_SGRP_5_ERR	    mBIT(5)
#define	VXGE_HAL_USDC_SGRP_ASSIGNMENT_USDC_SGRP_6_ERR	    mBIT(6)
#define	VXGE_HAL_USDC_SGRP_ASSIGNMENT_USDC_SGRP_7_ERR	    mBIT(7)
#define	VXGE_HAL_USDC_SGRP_ASSIGNMENT_USDC_SGRP_8_ERR	    mBIT(8)
#define	VXGE_HAL_USDC_SGRP_ASSIGNMENT_USDC_SGRP_9_ERR	    mBIT(9)
#define	VXGE_HAL_USDC_SGRP_ASSIGNMENT_USDC_SGRP_10_ERR	    mBIT(10)
#define	VXGE_HAL_USDC_SGRP_ASSIGNMENT_USDC_SGRP_11_ERR	    mBIT(11)
#define	VXGE_HAL_USDC_SGRP_ASSIGNMENT_USDC_SGRP_12_ERR	    mBIT(12)
#define	VXGE_HAL_USDC_SGRP_ASSIGNMENT_USDC_SGRP_13_ERR	    mBIT(13)
#define	VXGE_HAL_USDC_SGRP_ASSIGNMENT_USDC_SGRP_14_ERR	    mBIT(14)
#define	VXGE_HAL_USDC_SGRP_ASSIGNMENT_USDC_SGRP_15_ERR	    mBIT(15)
#define	VXGE_HAL_USDC_SGRP_ASSIGNMENT_USDC_SGRP_16_ERR	    mBIT(16)
#define	VXGE_HAL_USDC_SGRP_ASSIGNMENT_USDC_SGRP_17_ERR	    mBIT(17)
#define	VXGE_HAL_USDC_SGRP_ASSIGNMENT_USDC_SGRP_18_ERR	    mBIT(18)
#define	VXGE_HAL_USDC_SGRP_ASSIGNMENT_USDC_SGRP_19_ERR	    mBIT(19)
#define	VXGE_HAL_USDC_SGRP_ASSIGNMENT_USDC_SGRP_20_ERR	    mBIT(20)
#define	VXGE_HAL_USDC_SGRP_ASSIGNMENT_USDC_SGRP_21_ERR	    mBIT(21)
#define	VXGE_HAL_USDC_SGRP_ASSIGNMENT_USDC_SGRP_22_ERR	    mBIT(22)
#define	VXGE_HAL_USDC_SGRP_ASSIGNMENT_USDC_SGRP_23_ERR	    mBIT(23)
#define	VXGE_HAL_USDC_SGRP_ASSIGNMENT_USDC_SGRP_24_ERR	    mBIT(24)
#define	VXGE_HAL_USDC_SGRP_ASSIGNMENT_USDC_SGRP_25_ERR	    mBIT(25)
#define	VXGE_HAL_USDC_SGRP_ASSIGNMENT_USDC_SGRP_26_ERR	    mBIT(26)
#define	VXGE_HAL_USDC_SGRP_ASSIGNMENT_USDC_SGRP_27_ERR	    mBIT(27)
#define	VXGE_HAL_USDC_SGRP_ASSIGNMENT_USDC_SGRP_28_ERR	    mBIT(28)
#define	VXGE_HAL_USDC_SGRP_ASSIGNMENT_USDC_SGRP_29_ERR	    mBIT(29)
#define	VXGE_HAL_USDC_SGRP_ASSIGNMENT_USDC_SGRP_30_ERR	    mBIT(30)
#define	VXGE_HAL_USDC_SGRP_ASSIGNMENT_USDC_SGRP_31_ERR	    mBIT(31)
/* 0x024d0 */	u64	usdc_cntrl;
#define	VXGE_HAL_USDC_CNTRL_MIN_VALUE(val)		    vBIT(val, 1, 7)
/* 0x024d8 */	u64	usdc_read_cntrl;
#define	VXGE_HAL_USDC_READ_CNTRL_USDC_FREEZE		    mBIT(7)
#define	VXGE_HAL_USDC_READ_CNTRL_USDC_RDCTRL(val)	    vBIT(val, 14, 2)
#define	VXGE_HAL_USDC_READ_CNTRL_USDC_WORD_SEL		    mBIT(23)
#define	VXGE_HAL_USDC_READ_CNTRL_USDC_ADDR(val)		    vBIT(val, 49, 15)
/* 0x024e0 */	u64	usdc_read_data;
#define	VXGE_HAL_USDC_READ_DATA_READ_DATA(val)		    vBIT(val, 0, 64)
	u8	unused02500[0x02500 - 0x024e8];

/* 0x02500 */	u64	ugrp_srq_wrr_priority_0;
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_0_NUMBER_0(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_0_NUMBER_1(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_0_NUMBER_2(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_0_NUMBER_3(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_0_NUMBER_4(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_0_NUMBER_5(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_0_NUMBER_6(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_0_NUMBER_7(val)	    vBIT(val, 59, 5)
/* 0x02508 */	u64	ugrp_srq_wrr_priority_1;
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_1_NUMBER_8(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_1_NUMBER_9(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_1_NUMBER_10(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_1_NUMBER_11(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_1_NUMBER_12(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_1_NUMBER_13(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_1_NUMBER_14(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_1_NUMBER_15(val)	    vBIT(val, 59, 5)
/* 0x02510 */	u64	ugrp_srq_wrr_priority_2;
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_2_NUMBER_16(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_2_NUMBER_17(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_2_NUMBER_18(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_2_NUMBER_19(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_2_NUMBER_20(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_2_NUMBER_21(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_2_NUMBER_22(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_2_NUMBER_23(val)	    vBIT(val, 59, 5)
/* 0x02518 */	u64	ugrp_srq_wrr_priority_3;
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_3_NUMBER_24(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_3_NUMBER_25(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_3_NUMBER_26(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_3_NUMBER_27(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_3_NUMBER_28(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_3_NUMBER_29(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_3_NUMBER_30(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_3_NUMBER_31(val)	    vBIT(val, 59, 5)
/* 0x02520 */	u64	ugrp_srq_wrr_priority_4;
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_4_NUMBER_32(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_4_NUMBER_33(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_4_NUMBER_34(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_4_NUMBER_35(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_4_NUMBER_36(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_4_NUMBER_37(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_4_NUMBER_38(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_4_NUMBER_39(val)	    vBIT(val, 59, 5)
/* 0x02528 */	u64	ugrp_srq_wrr_priority_5;
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_5_NUMBER_40(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_5_NUMBER_41(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_5_NUMBER_42(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_5_NUMBER_43(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_5_NUMBER_44(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_5_NUMBER_45(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_5_NUMBER_46(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_5_NUMBER_47(val)	    vBIT(val, 59, 5)
/* 0x02530 */	u64	ugrp_srq_wrr_priority_6;
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_6_NUMBER_48(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_6_NUMBER_49(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_6_NUMBER_50(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_6_NUMBER_51(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_6_NUMBER_52(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_6_NUMBER_53(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_6_NUMBER_54(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_6_NUMBER_55(val)	    vBIT(val, 59, 5)
/* 0x02538 */	u64	ugrp_srq_wrr_priority_7;
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_7_NUMBER_56(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_7_NUMBER_57(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_7_NUMBER_58(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_7_NUMBER_59(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_7_NUMBER_60(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_7_NUMBER_61(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_7_NUMBER_62(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_7_NUMBER_63(val)	    vBIT(val, 59, 5)
/* 0x02540 */	u64	ugrp_srq_wrr_priority_8;
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_8_NUMBER_64(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_8_NUMBER_65(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_8_NUMBER_66(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_8_NUMBER_67(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_8_NUMBER_68(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_8_NUMBER_69(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_8_NUMBER_70(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_8_NUMBER_71(val)	    vBIT(val, 59, 5)
/* 0x02548 */	u64	ugrp_srq_wrr_priority_9;
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_9_NUMBER_72(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_9_NUMBER_73(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_9_NUMBER_74(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_9_NUMBER_75(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_9_NUMBER_76(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_9_NUMBER_77(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_9_NUMBER_78(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_9_NUMBER_79(val)	    vBIT(val, 59, 5)
/* 0x02550 */	u64	ugrp_srq_wrr_priority_10;
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_10_NUMBER_80(val)    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_10_NUMBER_81(val)    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_10_NUMBER_82(val)    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_10_NUMBER_83(val)    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_10_NUMBER_84(val)    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_10_NUMBER_85(val)    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_10_NUMBER_86(val)    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_10_NUMBER_87(val)    vBIT(val, 59, 5)
/* 0x02558 */	u64	ugrp_srq_wrr_priority_11;
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_11_NUMBER_88(val)    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_11_NUMBER_89(val)    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_11_NUMBER_90(val)    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_11_NUMBER_91(val)    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_11_NUMBER_92(val)    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_11_NUMBER_93(val)    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_11_NUMBER_94(val)    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_11_NUMBER_95(val)    vBIT(val, 59, 5)
/* 0x02560 */	u64	ugrp_srq_wrr_priority_12;
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_12_NUMBER_96(val)    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_12_NUMBER_97(val)    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_12_NUMBER_98(val)    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_12_NUMBER_99(val)    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_12_NUMBER_100(val)   vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_12_NUMBER_101(val)   vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_12_NUMBER_102(val)   vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_12_NUMBER_103(val)   vBIT(val, 59, 5)
/* 0x02568 */	u64	ugrp_srq_wrr_priority_13;
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_13_NUMBER_104(val)   vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_13_NUMBER_105(val)   vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_13_NUMBER_106(val)   vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_13_NUMBER_107(val)   vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_13_NUMBER_108(val)   vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_13_NUMBER_109(val)   vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_13_NUMBER_110(val)   vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_13_NUMBER_111(val)   vBIT(val, 59, 5)
/* 0x02570 */	u64	ugrp_srq_wrr_priority_14;
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_14_NUMBER_112(val)   vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_14_NUMBER_113(val)   vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_14_NUMBER_114(val)   vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_14_NUMBER_115(val)   vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_14_NUMBER_116(val)   vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_14_NUMBER_117(val)   vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_14_NUMBER_118(val)   vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_14_NUMBER_119(val)   vBIT(val, 59, 5)
/* 0x02578 */	u64	ugrp_srq_wrr_priority_15;
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_15_NUMBER_120(val)   vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_15_NUMBER_121(val)   vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_15_NUMBER_122(val)   vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_15_NUMBER_123(val)   vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_15_NUMBER_124(val)   vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_15_NUMBER_125(val)   vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_15_NUMBER_126(val)   vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_15_NUMBER_127(val)   vBIT(val, 59, 5)
/* 0x02580 */	u64	ugrp_srq_wrr_priority_16;
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_16_NUMBER_128(val)   vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_16_NUMBER_129(val)   vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_16_NUMBER_130(val)   vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_16_NUMBER_131(val)   vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_16_NUMBER_132(val)   vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_16_NUMBER_133(val)   vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_16_NUMBER_134(val)   vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_16_NUMBER_135(val)   vBIT(val, 59, 5)
/* 0x02588 */	u64	ugrp_srq_wrr_priority_17;
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_17_NUMBER_136(val)   vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_17_NUMBER_137(val)   vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_17_NUMBER_138(val)   vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_17_NUMBER_139(val)   vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_17_NUMBER_140(val)   vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_17_NUMBER_141(val)   vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_17_NUMBER_142(val)   vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_17_NUMBER_143(val)   vBIT(val, 59, 5)
/* 0x02590 */	u64	ugrp_srq_wrr_priority_18;
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_18_NUMBER_144(val)   vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_18_NUMBER_145(val)   vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_18_NUMBER_146(val)   vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_18_NUMBER_147(val)   vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_18_NUMBER_148(val)   vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_18_NUMBER_149(val)   vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_18_NUMBER_150(val)   vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_18_NUMBER_151(val)   vBIT(val, 59, 5)
/* 0x02598 */	u64	ugrp_srq_wrr_priority_19;
#define	VXGE_HAL_UGRP_SRQ_WRR_PRIORITY_19_NUMBER_152(val)   vBIT(val, 3, 5)
/* 0x025a0 */	u64	ugrp_cqrq_wrr_priority_0;
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_0_NUMBER_0(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_0_NUMBER_1(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_0_NUMBER_2(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_0_NUMBER_3(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_0_NUMBER_4(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_0_NUMBER_5(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_0_NUMBER_6(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_0_NUMBER_7(val)	    vBIT(val, 59, 5)
/* 0x025a8 */	u64	ugrp_cqrq_wrr_priority_1;
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_1_NUMBER_8(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_1_NUMBER_9(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_1_NUMBER_10(val)    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_1_NUMBER_11(val)    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_1_NUMBER_12(val)    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_1_NUMBER_13(val)    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_1_NUMBER_14(val)    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_1_NUMBER_15(val)    vBIT(val, 59, 5)
/* 0x025b0 */	u64	ugrp_cqrq_wrr_priority_2;
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_2_NUMBER_16(val)    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_2_NUMBER_17(val)    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_2_NUMBER_18(val)    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_2_NUMBER_19(val)    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_2_NUMBER_20(val)    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_2_NUMBER_21(val)    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_2_NUMBER_22(val)    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_2_NUMBER_23(val)    vBIT(val, 59, 5)
/* 0x025b8 */	u64	ugrp_cqrq_wrr_priority_3;
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_3_NUMBER_24(val)    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_3_NUMBER_25(val)    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_3_NUMBER_26(val)    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_3_NUMBER_27(val)    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_3_NUMBER_28(val)    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_3_NUMBER_29(val)    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_3_NUMBER_30(val)    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_3_NUMBER_31(val)    vBIT(val, 59, 5)
/* 0x025c0 */	u64	ugrp_cqrq_wrr_priority_4;
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_4_NUMBER_32(val)    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_4_NUMBER_33(val)    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_4_NUMBER_34(val)    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_4_NUMBER_35(val)    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_4_NUMBER_36(val)    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_4_NUMBER_37(val)    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_4_NUMBER_38(val)    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_4_NUMBER_39(val)    vBIT(val, 59, 5)
/* 0x025c8 */	u64	ugrp_cqrq_wrr_priority_5;
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_5_NUMBER_40(val)    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_5_NUMBER_41(val)    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_5_NUMBER_42(val)    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_5_NUMBER_43(val)    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_5_NUMBER_44(val)    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_5_NUMBER_45(val)    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_5_NUMBER_46(val)    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_5_NUMBER_47(val)    vBIT(val, 59, 5)
/* 0x025d0 */	u64	ugrp_cqrq_wrr_priority_6;
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_6_NUMBER_48(val)    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_6_NUMBER_49(val)    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_6_NUMBER_50(val)    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_6_NUMBER_51(val)    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_6_NUMBER_52(val)    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_6_NUMBER_53(val)    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_6_NUMBER_54(val)    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_6_NUMBER_55(val)    vBIT(val, 59, 5)
/* 0x025d8 */	u64	ugrp_cqrq_wrr_priority_7;
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_7_NUMBER_56(val)    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_7_NUMBER_57(val)    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_7_NUMBER_58(val)    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_7_NUMBER_59(val)    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_7_NUMBER_60(val)    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_7_NUMBER_61(val)    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_7_NUMBER_62(val)    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_7_NUMBER_63(val)    vBIT(val, 59, 5)
/* 0x025e0 */	u64	ugrp_cqrq_wrr_priority_8;
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_8_NUMBER_64(val)    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_8_NUMBER_65(val)    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_8_NUMBER_66(val)    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_8_NUMBER_67(val)    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_8_NUMBER_68(val)    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_8_NUMBER_69(val)    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_8_NUMBER_70(val)    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_8_NUMBER_71(val)    vBIT(val, 59, 5)
/* 0x025e8 */	u64	ugrp_cqrq_wrr_priority_9;
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_9_NUMBER_72(val)    vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_9_NUMBER_73(val)    vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_9_NUMBER_74(val)    vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_9_NUMBER_75(val)    vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_9_NUMBER_76(val)    vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_9_NUMBER_77(val)    vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_9_NUMBER_78(val)    vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_9_NUMBER_79(val)    vBIT(val, 59, 5)
/* 0x025f0 */	u64	ugrp_cqrq_wrr_priority_10;
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_10_NUMBER_80(val)   vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_10_NUMBER_81(val)   vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_10_NUMBER_82(val)   vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_10_NUMBER_83(val)   vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_10_NUMBER_84(val)   vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_10_NUMBER_85(val)   vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_10_NUMBER_86(val)   vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_10_NUMBER_87(val)   vBIT(val, 59, 5)
/* 0x025f8 */	u64	ugrp_cqrq_wrr_priority_11;
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_11_NUMBER_88(val)   vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_11_NUMBER_89(val)   vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_11_NUMBER_90(val)   vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_11_NUMBER_91(val)   vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_11_NUMBER_92(val)   vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_11_NUMBER_93(val)   vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_11_NUMBER_94(val)   vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_11_NUMBER_95(val)   vBIT(val, 59, 5)
/* 0x02600 */	u64	ugrp_cqrq_wrr_priority_12;
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_12_NUMBER_96(val)   vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_12_NUMBER_97(val)   vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_12_NUMBER_98(val)   vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_12_NUMBER_99(val)   vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_12_NUMBER_100(val)  vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_12_NUMBER_101(val)  vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_12_NUMBER_102(val)  vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_12_NUMBER_103(val)  vBIT(val, 59, 5)
/* 0x02608 */	u64	ugrp_cqrq_wrr_priority_13;
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_13_NUMBER_104(val)  vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_13_NUMBER_105(val)  vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_13_NUMBER_106(val)  vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_13_NUMBER_107(val)  vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_13_NUMBER_108(val)  vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_13_NUMBER_109(val)  vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_13_NUMBER_110(val)  vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_13_NUMBER_111(val)  vBIT(val, 59, 5)
/* 0x02610 */	u64	ugrp_cqrq_wrr_priority_14;
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_14_NUMBER_112(val)  vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_14_NUMBER_113(val)  vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_14_NUMBER_114(val)  vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_14_NUMBER_115(val)  vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_14_NUMBER_116(val)  vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_14_NUMBER_117(val)  vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_14_NUMBER_118(val)  vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_14_NUMBER_119(val)  vBIT(val, 59, 5)
/* 0x02618 */	u64	ugrp_cqrq_wrr_priority_15;
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_15_NUMBER_120(val)  vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_15_NUMBER_121(val)  vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_15_NUMBER_122(val)  vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_15_NUMBER_123(val)  vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_15_NUMBER_124(val)  vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_15_NUMBER_125(val)  vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_15_NUMBER_126(val)  vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_15_NUMBER_127(val)  vBIT(val, 59, 5)
/* 0x02620 */	u64	ugrp_cqrq_wrr_priority_16;
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_16_NUMBER_128(val)  vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_16_NUMBER_129(val)  vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_16_NUMBER_130(val)  vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_16_NUMBER_131(val)  vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_16_NUMBER_132(val)  vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_16_NUMBER_133(val)  vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_16_NUMBER_134(val)  vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_16_NUMBER_135(val)  vBIT(val, 59, 5)
/* 0x02628 */	u64	ugrp_cqrq_wrr_priority_17;
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_17_NUMBER_136(val)  vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_17_NUMBER_137(val)  vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_17_NUMBER_138(val)  vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_17_NUMBER_139(val)  vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_17_NUMBER_140(val)  vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_17_NUMBER_141(val)  vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_17_NUMBER_142(val)  vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_17_NUMBER_143(val)  vBIT(val, 59, 5)
/* 0x02630 */	u64	ugrp_cqrq_wrr_priority_18;
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_18_NUMBER_144(val)  vBIT(val, 3, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_18_NUMBER_145(val)  vBIT(val, 11, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_18_NUMBER_146(val)  vBIT(val, 19, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_18_NUMBER_147(val)  vBIT(val, 27, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_18_NUMBER_148(val)  vBIT(val, 35, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_18_NUMBER_149(val)  vBIT(val, 43, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_18_NUMBER_150(val)  vBIT(val, 51, 5)
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_18_NUMBER_151(val)  vBIT(val, 59, 5)
/* 0x02638 */	u64	ugrp_cqrq_wrr_priority_19;
#define	VXGE_HAL_UGRP_CQRQ_WRR_PRIORITY_19_NUMBER_152(val)  vBIT(val, 3, 5)
/* 0x02640 */	u64	usdc_ecc_ctrl;
#define	VXGE_HAL_USDC_ECC_CTRL_ECC_DISABLE		    mBIT(7)
/* 0x02648 */	u64	usdc_vpbp_ctrl;
#define	VXGE_HAL_USDC_VPBP_CTRL_MSG_DIS			    mBIT(0)
#define	VXGE_HAL_USDC_VPBP_CTRL_H2L_DIS			    mBIT(1)
	u8	unused02700[0x02700 - 0x02650];

/* 0x02700 */	u64	rtdma_int_status;
#define	VXGE_HAL_RTDMA_INT_STATUS_PDA_ALARM_PDA_INT	    mBIT(1)
#define	VXGE_HAL_RTDMA_INT_STATUS_PCC_ERROR_PCC_INT	    mBIT(2)
#define	VXGE_HAL_RTDMA_INT_STATUS_LSO_ERROR_LSO_INT	    mBIT(4)
#define	VXGE_HAL_RTDMA_INT_STATUS_SM_ERROR_SM_INT	    mBIT(5)
/* 0x02708 */	u64	rtdma_int_mask;
/* 0x02710 */	u64	pda_alarm_reg;
#define	VXGE_HAL_PDA_ALARM_REG_PDA_HSC_FIFO_ERR		    mBIT(0)
#define	VXGE_HAL_PDA_ALARM_REG_PDA_SM_ERR		    mBIT(1)
/* 0x02718 */	u64	pda_alarm_mask;
/* 0x02720 */	u64	pda_alarm_alarm;
/* 0x02728 */	u64	pcc_error_reg;
#define	VXGE_HAL_PCC_ERROR_REG_PCC_PCC_FRM_BUF_SBE(n)	    mBIT(n)
#define	VXGE_HAL_PCC_ERROR_REG_PCC_PCC_TXDO_SBE(n)	    mBIT(n)
#define	VXGE_HAL_PCC_ERROR_REG_PCC_PCC_FRM_BUF_DBE(n)	    mBIT(n)
#define	VXGE_HAL_PCC_ERROR_REG_PCC_PCC_TXDO_DBE(n)	    mBIT(n)
#define	VXGE_HAL_PCC_ERROR_REG_PCC_PCC_FSM_ERR_ALARM(n)	    mBIT(n)
#define	VXGE_HAL_PCC_ERROR_REG_PCC_PCC_SERR(n)		    mBIT(n)
/* 0x02730 */	u64	pcc_error_mask;
/* 0x02738 */	u64	pcc_error_alarm;
/* 0x02740 */	u64	lso_error_reg;
#define	VXGE_HAL_LSO_ERROR_REG_PCC_LSO_ABORT(n)		    mBIT(n)
#define	VXGE_HAL_LSO_ERROR_REG_PCC_LSO_FSM_ERR_ALARM(n)	    mBIT(n)
/* 0x02748 */	u64	lso_error_mask;
/* 0x02750 */	u64	lso_error_alarm;
/* 0x02758 */	u64	sm_error_reg;
#define	VXGE_HAL_SM_ERROR_REG_SM_FSM_ERR_ALARM		    mBIT(15)
/* 0x02760 */	u64	sm_error_mask;
/* 0x02768 */	u64	sm_error_alarm;
/* 0x02770 */	u64	pda_control;
#define	VXGE_HAL_PDA_CONTROL_PCC_INTERLOCK_EN		    mBIT(7)
#define	VXGE_HAL_PDA_CONTROL_SPLIT_IDLE			    mBIT(15)
#define	VXGE_HAL_PDA_CONTROL_PCC_MAX_DISABLE		    mBIT(23)
#define	VXGE_HAL_PDA_CONTROL_H2L_DO_GATE_EN		    mBIT(31)
#define	VXGE_HAL_PDA_CONTROL_TXD_INT_NUM_CTLR		    mBIT(39)
#define	VXGE_HAL_PDA_CONTROL_ISSUE_8B_READ		    mBIT(47)
/* 0x02778 */	u64	pda_pda_control_0;
#define	VXGE_HAL_PDA_PDA_CONTROL_0_PCC_MAX(val)		    vBIT(val, 4, 4)
#define	VXGE_HAL_PDA_PDA_CONTROL_0_FE_MAX(val)		    vBIT(val, 13, 3)
/* 0x02780 */	u64	pda_pda_service_state_0;
#define	VXGE_HAL_PDA_PDA_SERVICE_STATE_0_NUMBER_0(val)	    vBIT(val, 5, 3)
#define	VXGE_HAL_PDA_PDA_SERVICE_STATE_0_NUMBER_1(val)	    vBIT(val, 13, 3)
#define	VXGE_HAL_PDA_PDA_SERVICE_STATE_0_NUMBER_2(val)	    vBIT(val, 21, 3)
#define	VXGE_HAL_PDA_PDA_SERVICE_STATE_0_NUMBER_3(val)	    vBIT(val, 29, 3)
#define	VXGE_HAL_PDA_PDA_SERVICE_STATE_0_NUMBER_4(val)	    vBIT(val, 37, 3)
#define	VXGE_HAL_PDA_PDA_SERVICE_STATE_0_NUMBER_5(val)	    vBIT(val, 45, 3)
#define	VXGE_HAL_PDA_PDA_SERVICE_STATE_0_NUMBER_6(val)	    vBIT(val, 53, 3)
#define	VXGE_HAL_PDA_PDA_SERVICE_STATE_0_NUMBER_7(val)	    vBIT(val, 61, 3)
/* 0x02788 */	u64	pda_pda_service_state_1;
#define	VXGE_HAL_PDA_PDA_SERVICE_STATE_1_NUMBER_8(val)	    vBIT(val, 5, 3)
#define	VXGE_HAL_PDA_PDA_SERVICE_STATE_1_NUMBER_9(val)	    vBIT(val, 13, 3)
#define	VXGE_HAL_PDA_PDA_SERVICE_STATE_1_NUMBER_10(val)	    vBIT(val, 21, 3)
#define	VXGE_HAL_PDA_PDA_SERVICE_STATE_1_NUMBER_11(val)	    vBIT(val, 29, 3)
#define	VXGE_HAL_PDA_PDA_SERVICE_STATE_1_NUMBER_12(val)	    vBIT(val, 37, 3)
#define	VXGE_HAL_PDA_PDA_SERVICE_STATE_1_NUMBER_13(val)	    vBIT(val, 45, 3)
#define	VXGE_HAL_PDA_PDA_SERVICE_STATE_1_NUMBER_14(val)	    vBIT(val, 53, 3)
#define	VXGE_HAL_PDA_PDA_SERVICE_STATE_1_NUMBER_15(val)	    vBIT(val, 61, 3)
/* 0x02790 */	u64	pda_pda_service_state_2;
#define	VXGE_HAL_PDA_PDA_SERVICE_STATE_2_NUMBER_16(val)	    vBIT(val, 5, 3)
#define	VXGE_HAL_PDA_PDA_SERVICE_STATE_2_NUMBER_17(val)	    vBIT(val, 13, 3)
#define	VXGE_HAL_PDA_PDA_SERVICE_STATE_2_NUMBER_18(val)	    vBIT(val, 21, 3)
#define	VXGE_HAL_PDA_PDA_SERVICE_STATE_2_NUMBER_19(val)	    vBIT(val, 29, 3)
#define	VXGE_HAL_PDA_PDA_SERVICE_STATE_2_NUMBER_20(val)	    vBIT(val, 37, 3)
/* 0x02798 */	u64	pda_pda_task_priority_number;
#define	VXGE_HAL_PDA_PDA_TASK_PRIORITY_NUMBER_CXP(val)	    vBIT(val, 5, 3)
#define	VXGE_HAL_PDA_PDA_TASK_PRIORITY_NUMBER_H2L(val)	    vBIT(val, 13, 3)
#define	VXGE_HAL_PDA_PDA_TASK_PRIORITY_NUMBER_KDFC(val)	    vBIT(val, 21, 3)
#define	VXGE_HAL_PDA_PDA_TASK_PRIORITY_NUMBER_MP(val)	    vBIT(val, 29, 3)
#define	VXGE_HAL_PDA_PDA_TASK_PRIORITY_NUMBER_PE(val)	    vBIT(val, 37, 3)
#define	VXGE_HAL_PDA_PDA_TASK_PRIORITY_NUMBER_QCC(val)	    vBIT(val, 45, 3)
/* 0x027a0 */	u64	pda_vp;
#define	VXGE_HAL_PDA_VP_RD_XON_ENABLE			    mBIT(0)
#define	VXGE_HAL_PDA_VP_WR_XON_ENABLE			    mBIT(1)
#define	VXGE_HAL_PDA_VP_NO_ACTIVITY_DISABLE		    mBIT(2)
/* 0x027a8 */	u64	txd_ownership_ctrl;
#define	VXGE_HAL_TXD_OWNERSHIP_CTRL_KEEP_OWNERSHIP	    mBIT(7)
/* 0x027b0 */	u64	pcc_cfg;
#define	VXGE_HAL_PCC_CFG_PCC_ENABLE(n)			    mBIT(n)
#define	VXGE_HAL_PCC_CFG_PCC_ECC_ENABLE_N(n)		    mBIT(n)
/* 0x027b8 */	u64	pcc_control;
#define	VXGE_HAL_PCC_CONTROL_FE_ENABLE(val)		    vBIT(val, 6, 2)
#define	VXGE_HAL_PCC_CONTROL_EARLY_ASSIGN_EN		    mBIT(15)
#define	VXGE_HAL_PCC_CONTROL_UNBLOCK_DB_ERR		    mBIT(31)
/* 0x027c0 */	u64	pda_status1;
#define	VXGE_HAL_PDA_STATUS1_PDA_WRAP_0_CTR(val)	    vBIT(val, 4, 4)
#define	VXGE_HAL_PDA_STATUS1_PDA_WRAP_1_CTR(val)	    vBIT(val, 12, 4)
#define	VXGE_HAL_PDA_STATUS1_PDA_WRAP_2_CTR(val)	    vBIT(val, 20, 4)
#define	VXGE_HAL_PDA_STATUS1_PDA_WRAP_3_CTR(val)	    vBIT(val, 28, 4)
#define	VXGE_HAL_PDA_STATUS1_PDA_WRAP_4_CTR(val)	    vBIT(val, 36, 4)
#define	VXGE_HAL_PDA_STATUS1_PDA_WRAP_5_CTR(val)	    vBIT(val, 44, 4)
#define	VXGE_HAL_PDA_STATUS1_PDA_WRAP_6_CTR(val)	    vBIT(val, 52, 4)
#define	VXGE_HAL_PDA_STATUS1_PDA_WRAP_7_CTR(val)	    vBIT(val, 60, 4)
/* 0x027c8 */	u64	rtdma_bw_timer;
#define	VXGE_HAL_RTDMA_BW_TIMER_TIMER_CTRL(val)		    vBIT(val, 12, 4)
	u8	unused02900[0x02900 - 0x027d0];

/* 0x02900 */	u64	g3cmct_int_status;
#define	VXGE_HAL_G3CMCT_INT_STATUS_ERR_G3IF_INT		    mBIT(0)
/* 0x02908 */	u64	g3cmct_int_mask;
/* 0x02910 */	u64	g3cmct_err_reg;
#define	VXGE_HAL_G3CMCT_ERR_REG_G3IF_SM_ERR		    mBIT(4)
#define	VXGE_HAL_G3CMCT_ERR_REG_G3IF_GDDR3_DECC		    mBIT(5)
#define	VXGE_HAL_G3CMCT_ERR_REG_G3IF_GDDR3_U_DECC	    mBIT(6)
#define	VXGE_HAL_G3CMCT_ERR_REG_G3IF_CTRL_FIFO_DECC	    mBIT(7)
#define	VXGE_HAL_G3CMCT_ERR_REG_G3IF_GDDR3_SECC		    mBIT(29)
#define	VXGE_HAL_G3CMCT_ERR_REG_G3IF_GDDR3_U_SECC	    mBIT(30)
#define	VXGE_HAL_G3CMCT_ERR_REG_G3IF_CTRL_FIFO_SECC	    mBIT(31)
/* 0x02918 */	u64	g3cmct_err_mask;
/* 0x02920 */	u64	g3cmct_err_alarm;
/* 0x02928 */	u64	g3cmct_config0;
#define	VXGE_HAL_G3CMCT_CONFIG0_RD_CMD_LATENCY_RPATH(val)   vBIT(val, 5, 3)
#define	VXGE_HAL_G3CMCT_CONFIG0_RD_CMD_LATENCY(val)	    vBIT(val, 13, 3)
#define	VXGE_HAL_G3CMCT_CONFIG0_REFRESH_PER(val)	    vBIT(val, 16, 16)
#define	VXGE_HAL_G3CMCT_CONFIG0_TRC(val)		    vBIT(val, 35, 5)
#define	VXGE_HAL_G3CMCT_CONFIG0_TRRD(val)		    vBIT(val, 44, 4)
#define	VXGE_HAL_G3CMCT_CONFIG0_TFAW(val)		    vBIT(val, 50, 6)
#define	VXGE_HAL_G3CMCT_CONFIG0_RD_FIFO_THR(val)	    vBIT(val, 58, 6)
/* 0x02930 */	u64	g3cmct_config1;
#define	VXGE_HAL_G3CMCT_CONFIG1_BIC_THR(val)		    vBIT(val, 3, 5)
#define	VXGE_HAL_G3CMCT_CONFIG1_BIC_OFF			    mBIT(15)
#define	VXGE_HAL_G3CMCT_CONFIG1_IGNORE_BEM		    mBIT(23)
#define	VXGE_HAL_G3CMCT_CONFIG1_RD_SAMPLING(val)	    vBIT(val, 29, 3)
#define	VXGE_HAL_G3CMCT_CONFIG1_CMD_START_PHASE		    mBIT(39)
#define	VXGE_HAL_G3CMCT_CONFIG1_BIC_HI_THR(val)		    vBIT(val, 43, 5)
#define	VXGE_HAL_G3CMCT_CONFIG1_BIC_MODE(val)		    vBIT(val, 54, 2)
#define	VXGE_HAL_G3CMCT_CONFIG1_ECC_ENABLE(val)		    vBIT(val, 57, 7)
/* 0x02938 */	u64	g3cmct_config2;
#define	VXGE_HAL_G3CMCT_CONFIG2_DEV_USE_ENABLE(val)	    vBIT(val, 6, 2)
#define	VXGE_HAL_G3CMCT_CONFIG2_DEV_USE_VALUE(val)	    vBIT(val, 9, 7)
#define	VXGE_HAL_G3CMCT_CONFIG2_ARBITER_CTRL(val)	    vBIT(val, 22, 2)
#define	VXGE_HAL_G3CMCT_CONFIG2_DEFINE_CAD		    mBIT(31)
#define	VXGE_HAL_G3CMCT_CONFIG2_DEFINE_NOP_AD		    mBIT(39)
#define	VXGE_HAL_G3CMCT_CONFIG2_LAST_CADD(val)		    vBIT(val, 43, 13)
/* 0x02940 */	u64	g3cmct_init0;
#define	VXGE_HAL_G3CMCT_INIT0_MRS_BAD(val)		    vBIT(val, 5, 3)
#define	VXGE_HAL_G3CMCT_INIT0_MRS_WL(val)		    vBIT(val, 13, 3)
#define	VXGE_HAL_G3CMCT_INIT0_MRS_DLL			    mBIT(23)
#define	VXGE_HAL_G3CMCT_INIT0_MRS_TM			    mBIT(39)
#define	VXGE_HAL_G3CMCT_INIT0_MRS_CL(val)		    vBIT(val, 44, 4)
#define	VXGE_HAL_G3CMCT_INIT0_MRS_BT			    mBIT(55)
#define	VXGE_HAL_G3CMCT_INIT0_MRS_BL(val)		    vBIT(val, 62, 2)
/* 0x02948 */	u64	g3cmct_init1;
#define	VXGE_HAL_G3CMCT_INIT1_EMRS_BAD(val)		    vBIT(val, 5, 3)
#define	VXGE_HAL_G3CMCT_INIT1_EMRS_AD_TER		    mBIT(15)
#define	VXGE_HAL_G3CMCT_INIT1_EMRS_ID			    mBIT(23)
#define	VXGE_HAL_G3CMCT_INIT1_EMRS_RON			    mBIT(39)
#define	VXGE_HAL_G3CMCT_INIT1_EMRS_AL			    mBIT(47)
#define	VXGE_HAL_G3CMCT_INIT1_EMRS_TWR(val)		    vBIT(val, 53, 3)
#define	VXGE_HAL_G3CMCT_INIT1_EMRS_DQ_TER(val)		    vBIT(val, 62, 2)
/* 0x02950 */	u64	g3cmct_init2;
#define	VXGE_HAL_G3CMCT_INIT2_EMRS_DR_STR(val)		    vBIT(val, 6, 2)
#define	VXGE_HAL_G3CMCT_INIT2_START_INI	mBIT(15)
#define	VXGE_HAL_G3CMCT_INIT2_POWER_UP_DELAY(val)	    vBIT(val, 16, 24)
#define	VXGE_HAL_G3CMCT_INIT2_ACTIVE_CMD_DELAY(val)	    vBIT(val, 40, 24)
/* 0x02958 */	u64	g3cmct_init3;
#define	VXGE_HAL_G3CMCT_INIT3_TRP_DELAY(val)		    vBIT(val, 0, 8)
#define	VXGE_HAL_G3CMCT_INIT3_TMRD_DELAY(val)		    vBIT(val, 8, 8)
#define	VXGE_HAL_G3CMCT_INIT3_TWR2PRE_DELAY(val)	    vBIT(val, 16, 8)
#define	VXGE_HAL_G3CMCT_INIT3_TRD2PRE_DELAY(val)	    vBIT(val, 24, 8)
#define	VXGE_HAL_G3CMCT_INIT3_TRCDR_DELAY(val)		    vBIT(val, 32, 8)
#define	VXGE_HAL_G3CMCT_INIT3_TRCDW_DELAY(val)		    vBIT(val, 40, 8)
#define	VXGE_HAL_G3CMCT_INIT3_TWR2RD_DELAY(val)		    vBIT(val, 48, 8)
#define	VXGE_HAL_G3CMCT_INIT3_TRD2WR_DELAY(val)		    vBIT(val, 56, 8)
/* 0x02960 */	u64	g3cmct_init4;
#define	VXGE_HAL_G3CMCT_INIT4_TRFC_DELAY(val)		    vBIT(val, 0, 8)
#define	VXGE_HAL_G3CMCT_INIT4_REFRESH_BURSTS(val)	    vBIT(val, 12, 4)
#define	VXGE_HAL_G3CMCT_INIT4_CKE_INIT_VAL		    mBIT(31)
#define	VXGE_HAL_G3CMCT_INIT4_VENDOR_ID(val)		    vBIT(val, 32, 8)
#define	VXGE_HAL_G3CMCT_INIT4_OOO_DEPTH(val)		    vBIT(val, 42, 6)
#define	VXGE_HAL_G3CMCT_INIT4_ICTRL_INIT_DONE		    mBIT(55)
#define	VXGE_HAL_G3CMCT_INIT4_IOCAL_WAIT_DISABLE	    mBIT(63)
/* 0x02968 */	u64	g3cmct_init5;
#define	VXGE_HAL_G3CMCT_INIT5_TRAS_DELAY(val)		    vBIT(val, 3, 5)
#define	VXGE_HAL_G3CMCT_INIT5_TVID_DELAY(val)		    vBIT(val, 8, 8)
#define	VXGE_HAL_G3CMCT_INIT5_TWR_APRE2CMD(val)		    vBIT(val, 16, 8)
#define	VXGE_HAL_G3CMCT_INIT5_TRD_APRE2CMD(val)		    vBIT(val, 24, 8)
#define	VXGE_HAL_G3CMCT_INIT5_TWR_APRE2CMD_CON(val)	    vBIT(val, 32, 8)
#define	VXGE_HAL_G3CMCT_INIT5_GDDR3_DLL_DELAY(val)	    vBIT(val, 40, 24)
/* 0x02970 */	u64	g3cmct_dll_training1;
#define	VXGE_HAL_G3CMCT_DLL_TRAINING1_DLL_TRA_DATA00(val)   vBIT(val, 0, 64)
/* 0x02978 */	u64	g3cmct_dll_training2;
#define	VXGE_HAL_G3CMCT_DLL_TRAINING2_DLL_TRA_DATA01(val)   vBIT(val, 0, 64)
/* 0x02980 */	u64	g3cmct_dll_training3;
#define	VXGE_HAL_G3CMCT_DLL_TRAINING3_DLL_TRA_DATA10(val)   vBIT(val, 0, 64)
/* 0x02988 */	u64	g3cmct_dll_training4;
#define	VXGE_HAL_G3CMCT_DLL_TRAINING4_DLL_TRA_DATA11(val)   vBIT(val, 0, 64)
/* 0x02990 */	u64	g3cmct_dll_training6;
#define	VXGE_HAL_G3CMCT_DLL_TRAINING6_DLL_TRA_DATA20(val)   vBIT(val, 0, 64)
/* 0x02998 */	u64	g3cmct_dll_training7;
#define	VXGE_HAL_G3CMCT_DLL_TRAINING7_DLL_TRA_DATA21(val)   vBIT(val, 0, 64)
/* 0x029a0 */	u64	g3cmct_dll_training8;
#define	VXGE_HAL_G3CMCT_DLL_TRAINING8_DLL_TRA_DATA30(val)   vBIT(val, 0, 64)
/* 0x029a8 */	u64	g3cmct_dll_training9;
#define	VXGE_HAL_G3CMCT_DLL_TRAINING9_DLL_TRA_DATA31(val)   vBIT(val, 0, 64)
/* 0x029b0 */	u64	g3cmct_dll_training5;
#define	VXGE_HAL_G3CMCT_DLL_TRAINING5_DLL_TRA_RADD(val)	    vBIT(val, 2, 14)
#define	VXGE_HAL_G3CMCT_DLL_TRAINING5_DLL_TRA_CADD0(val)    vBIT(val, 21, 11)
#define	VXGE_HAL_G3CMCT_DLL_TRAINING5_DLL_TRA_CADD1(val)    vBIT(val, 37, 11)
/* 0x029b8 */	u64	g3cmct_dll_training10;
#define	VXGE_HAL_G3CMCT_DLL_TRAINING10_DLL_TP_READS(val)    vBIT(val, 4, 4)
#define	VXGE_HAL_G3CMCT_DLL_TRAINING10_DLL_SAMPLES(val)	    vBIT(val, 8, 8)
#define	VXGE_HAL_G3CMCT_DLL_TRAINING10_TRA_LOOPS(val)	    vBIT(val, 18, 14)
#define	VXGE_HAL_G3CMCT_DLL_TRAINING10_TRA_PASS_CNT(val)    vBIT(val, 33, 7)
#define	VXGE_HAL_G3CMCT_DLL_TRAINING10_TRA_STEP(val)	    vBIT(val, 41, 7)
/* 0x029c0 */	u64	g3cmct_dll_training11;
#define	VXGE_HAL_G3CMCT_DLL_TRAINING11_ICTRL_DLL_TRA_CNT(val) vBIT(val, 0, 48)
#define	VXGE_HAL_G3CMCT_DLL_TRAINING11_ICTRL_DLL_TRA_DIS(val) vBIT(val, 54, 2)
/* 0x029c8 */	u64	g3cmct_init6;
#define	VXGE_HAL_G3CMCT_INIT6_TWR_APRE2RD_DELAY(val)	    vBIT(val, 4, 4)
#define	VXGE_HAL_G3CMCT_INIT6_TWR_APRE2WR_DELAY(val)	    vBIT(val, 12, 4)
#define	VXGE_HAL_G3CMCT_INIT6_TWR_APRE2PRE_DELAY(val)	    vBIT(val, 20, 4)
#define	VXGE_HAL_G3CMCT_INIT6_TWR_APRE2ACT_DELAY(val)	    vBIT(val, 28, 4)
#define	VXGE_HAL_G3CMCT_INIT6_TRD_APRE2RD_DELAY(val)	    vBIT(val, 36, 4)
#define	VXGE_HAL_G3CMCT_INIT6_TRD_APRE2WR_DELAY(val)	    vBIT(val, 44, 4)
#define	VXGE_HAL_G3CMCT_INIT6_TRD_APRE2PRE_DELAY(val)	    vBIT(val, 52, 4)
#define	VXGE_HAL_G3CMCT_INIT6_TRD_APRE2ACT_DELAY(val)	    vBIT(val, 60, 4)
/* 0x029d0 */	u64	g3cmct_test0;
#define	VXGE_HAL_G3CMCT_TEST0_TEST_START_RADD(val)	    vBIT(val, 2, 14)
#define	VXGE_HAL_G3CMCT_TEST0_TEST_END_RADD(val)	    vBIT(val, 18, 14)
#define	VXGE_HAL_G3CMCT_TEST0_TEST_START_CADD(val)	    vBIT(val, 37, 11)
#define	VXGE_HAL_G3CMCT_TEST0_TEST_END_CADD(val)	    vBIT(val, 53, 11)
/* 0x029d8 */	u64	g3cmct_test01;
#define	VXGE_HAL_G3CMCT_TEST01_TEST_BANK(val)		    vBIT(val, 0, 8)
#define	VXGE_HAL_G3CMCT_TEST01_TEST_CTRL(val)		    vBIT(val, 12, 4)
#define	VXGE_HAL_G3CMCT_TEST01_TEST_MODE		    mBIT(23)
#define	VXGE_HAL_G3CMCT_TEST01_TEST_GO			    mBIT(31)
#define	VXGE_HAL_G3CMCT_TEST01_TEST_DONE		    mBIT(39)
#define	VXGE_HAL_G3CMCT_TEST01_ECC_DEC_TEST_FAIL_CNTR(val)  vBIT(val, 40, 16)
#define	VXGE_HAL_G3CMCT_TEST01_TEST_DATA_ADDR		    mBIT(63)
/* 0x029e0 */	u64	g3cmct_test1;
#define	VXGE_HAL_G3CMCT_TEST1_TX_TEST_DATA(val)		    vBIT(val, 0, 64)
/* 0x029e8 */	u64	g3cmct_test2;
#define	VXGE_HAL_G3CMCT_TEST2_TX_TEST_DATA(val)		    vBIT(val, 0, 64)
/* 0x029f0 */	u64	g3cmct_test11;
#define	VXGE_HAL_G3CMCT_TEST11_TX_TEST_DATA1(val)	    vBIT(val, 0, 64)
/* 0x029f8 */	u64	g3cmct_test21;
#define	VXGE_HAL_G3CMCT_TEST21_TX_TEST_DATA1(val)	    vBIT(val, 0, 64)
/* 0x02a00 */	u64	g3cmct_test3;
#define	VXGE_HAL_G3CMCT_TEST3_ECC_DEC_RX_TEST_DATA(val)	    vBIT(val, 0, 64)
/* 0x02a08 */	u64	g3cmct_test4;
#define	VXGE_HAL_G3CMCT_TEST4_ECC_DEC_RX_TEST_DATA(val)	    vBIT(val, 0, 64)
/* 0x02a10 */	u64	g3cmct_test31;
#define	VXGE_HAL_G3CMCT_TEST31_ECC_DEC_RX_TEST_DATA1(val)   vBIT(val, 0, 64)
/* 0x02a18 */	u64	g3cmct_test41;
#define	VXGE_HAL_G3CMCT_TEST41_ECC_DEC_RX_TEST_DATA1(val)   vBIT(val, 0, 64)
/* 0x02a20 */	u64	g3cmct_test5;
#define	VXGE_HAL_G3CMCT_TEST5_ECC_DEC_RX_FAILED_TEST_DATA(val) vBIT(val, 0, 64)
/* 0x02a28 */	u64	g3cmct_test6;
#define	VXGE_HAL_G3CMCT_TEST6_ECC_DEC_RX_FAILED_TEST_DATA(val) vBIT(val, 0, 64)
/* 0x02a30 */	u64	g3cmct_test51;
#define	VXGE_HAL_G3CMCT_TEST51_ECC_DEC_RX_FAILED_TEST_DATA1(val)\
							    vBIT(val, 0, 64)
/* 0x02a38 */	u64	g3cmct_test61;
#define	VXGE_HAL_G3CMCT_TEST61_ECC_DEC_RX_FAILED_TEST_DATA1(val)\
							    vBIT(val, 0, 64)
/* 0x02a40 */	u64	g3cmct_test7;
#define	VXGE_HAL_G3CMCT_TEST7_ECC_DEC_TEST_FAILED_RADD(val) vBIT(val, 0, 14)
#define	VXGE_HAL_G3CMCT_TEST7_ECC_DEC_TEST_FAILED_CADD(val) vBIT(val, 19, 11)
#define	VXGE_HAL_G3CMCT_TEST7_ECC_DEC_TEST_FAILED_BANK(val) vBIT(val, 32, 8)
/* 0x02a48 */	u64	g3cmct_test71;
#define	VXGE_HAL_G3CMCT_TEST71_ECC_DEC_TEST_FAILED_RADD1(val) vBIT(val, 0, 14)
#define	VXGE_HAL_G3CMCT_TEST71_ECC_DEC_TEST_FAILED_CADD1(val) vBIT(val, 19, 11)
#define	VXGE_HAL_G3CMCT_TEST71_ECC_DEC_TEST_FAILED_BANK1(val) vBIT(val, 32, 8)
/* 0x02a50 */	u64	g3cmct_init41;
#define	VXGE_HAL_G3CMCT_INIT41_VENDOR_ID_U(val)		    vBIT(val, 0, 8)
#define	VXGE_HAL_G3CMCT_INIT41_ENABLE_CMU		    mBIT(15)
/* 0x02a58 */	u64	g3cmct_test8;
#define	VXGE_HAL_G3CMCT_TEST8_ECC_DEC_U_RX_TEST_DATA_U(val) vBIT(val, 0, 64)
/* 0x02a60 */	u64	g3cmct_test9;
#define	VXGE_HAL_G3CMCT_TEST9_ECC_DEC_U_RX_TEST_DATA_U(val) vBIT(val, 0, 64)
/* 0x02a68 */	u64	g3cmct_test10;
#define	VXGE_HAL_G3CMCT_TEST10_ECC_DEC_U_RX_TEST_DATA1_U(val) vBIT(val, 0, 64)
/* 0x02a70 */	u64	g3cmct_test101;
#define	VXGE_HAL_G3CMCT_TEST101_ECC_DEC_U_RX_TEST_DATA1_U(val) vBIT(val, 0, 64)
/* 0x02a78 */	u64	g3cmct_test12;
#define	VXGE_HAL_G3CMCT_TEST12_ECC_DEC_U_RX_FAILED_TEST_DATA_U(val)\
							    vBIT(val, 0, 64)
/* 0x02a80 */	u64	g3cmct_test13;
#define	VXGE_HAL_G3CMCT_TEST13_ECC_DEC_U_RX_FAILED_TEST_DATA_U(val)\
							    vBIT(val, 0, 64)
/* 0x02a88 */	u64	g3cmct_test14;
#define	VXGE_HAL_G3CMCT_TEST14_ECC_DEC_U_RX_FAILED_TEST_DATA1_U(val)\
							    vBIT(val, 0, 64)
/* 0x02a90 */	u64	g3cmct_test15;
#define	VXGE_HAL_G3CMCT_TEST15_ECC_DEC_U_RX_FAILED_TEST_DATA1_U(val)\
							    vBIT(val, 0, 64)
/* 0x02a98 */	u64	g3cmct_test16;
#define	VXGE_HAL_G3CMCT_TEST16_ECC_DEC_U_TEST_FAILED_RADD_U(val)\
							    vBIT(val, 0, 14)
#define	VXGE_HAL_G3CMCT_TEST16_ECC_DEC_U_TEST_FAILED_CADD_U(val)\
							    vBIT(val, 19, 11)
#define	VXGE_HAL_G3CMCT_TEST16_ECC_DEC_U_TEST_FAILED_BANK_U(val)\
							    vBIT(val, 32, 8)
/* 0x02aa0 */	u64	g3cmct_test17;
#define	VXGE_HAL_G3CMCT_TEST17_ECC_DEC_U_TEST_FAILED_RADD1_U(val)\
							    vBIT(val, 0, 14)
#define	VXGE_HAL_G3CMCT_TEST17_ECC_DEC_U_TEST_FAILED_CADD1_U(val)\
							    vBIT(val, 19, 11)
#define	VXGE_HAL_G3CMCT_TEST17_ECC_DEC_U_TEST_FAILED_BANK1_U(val)\
							    vBIT(val, 32, 8)
/* 0x02aa8 */	u64	g3cmct_test18;
#define	VXGE_HAL_G3CMCT_TEST18_ECC_DEC_U_TEST_FAIL_CNTR_U(val)\
							    vBIT(val, 0, 16)
/* 0x02ab0 */	u64	g3cmct_loop_back;
#define	VXGE_HAL_G3CMCT_LOOP_BACK_TDATA(val)		    vBIT(val, 0, 32)
#define	VXGE_HAL_G3CMCT_LOOP_BACK_MODE			    mBIT(39)
#define	VXGE_HAL_G3CMCT_LOOP_BACK_GO			    mBIT(47)
#define	VXGE_HAL_G3CMCT_LOOP_BACK_DONE			    mBIT(55)
#define	VXGE_HAL_G3CMCT_LOOP_BACK_RDLL_IDLE_VAL(val)	    vBIT(val, 56, 8)
/* 0x02ab8 */	u64	g3cmct_loop_back1;
#define	VXGE_HAL_G3CMCT_LOOP_BACK1_RDLL_START_VAL(val)	    vBIT(val, 1, 7)
#define	VXGE_HAL_G3CMCT_LOOP_BACK1_RDLL_END_VAL(val)	    vBIT(val, 9, 7)
#define	VXGE_HAL_G3CMCT_LOOP_BACK1_WDLL_IDLE_VAL(val)	    vBIT(val, 16, 8)
#define	VXGE_HAL_G3CMCT_LOOP_BACK1_WDLL_START_VAL(val)	    vBIT(val, 25, 7)
#define	VXGE_HAL_G3CMCT_LOOP_BACK1_WDLL_END_VAL(val)	    vBIT(val, 33, 7)
#define	VXGE_HAL_G3CMCT_LOOP_BACK1_STEPS(val)		    vBIT(val, 45, 3)
#define	VXGE_HAL_G3CMCT_LOOP_BACK1_RDLL_MIN_FILTER(val)	    vBIT(val, 49, 7)
#define	VXGE_HAL_G3CMCT_LOOP_BACK1_RDLL_MAX_FILTER(val)	    vBIT(val, 57, 7)
/* 0x02ac0 */	u64	g3cmct_loop_back2;
#define	VXGE_HAL_G3CMCT_LOOP_BACK2_WDLL_MIN_FILTER(val)	    vBIT(val, 1, 7)
#define	VXGE_HAL_G3CMCT_LOOP_BACK2_WDLL_MAX_FILTER(val)	    vBIT(val, 9, 7)
/* 0x02ac8 */	u64	g3cmct_loop_back3;
#define	VXGE_HAL_G3CMCT_LOOP_BACK3_LBCTRL_CMU_RDLL_RESULT(val) vBIT(val, 0, 8)
#define	VXGE_HAL_G3CMCT_LOOP_BACK3_LBCTRL_CMU_WDLL_RESULT(val) vBIT(val, 8, 8)
#define	VXGE_HAL_G3CMCT_LOOP_BACK3_LBCTRL_CML_RDLL_RESULT(val) vBIT(val, 16, 8)
#define	VXGE_HAL_G3CMCT_LOOP_BACK3_LBCTRL_CML_WDLL_RESULT(val) vBIT(val, 24, 8)
#define	VXGE_HAL_G3CMCT_LOOP_BACK3_LBCTRL_CMU_RDLL_MON_RESULT(val)\
							    vBIT(val, 32, 8)
#define	VXGE_HAL_G3CMCT_LOOP_BACK3_LBCTRL_CML_RDLL_MON_RESULT(val)\
							    vBIT(val, 40, 8)
/* 0x02ad0 */	u64	g3cmct_loop_back4;
#define	VXGE_HAL_G3CMCT_LOOP_BACK4_LBCTRL_IO_U_PASS_FAILN(val) vBIT(val, 0, 32)
#define	VXGE_HAL_G3CMCT_LOOP_BACK4_LBCTRL_IO_L_PASS_FAILN(val) vBIT(val, 32, 32)
/* 0x02ad8 */	u64	g3cmct_loop_back5;
#define	VXGE_HAL_G3CMCT_LOOP_BACK5_RDLL_START_IO_VAL(val)   vBIT(val, 1, 7)
#define	VXGE_HAL_G3CMCT_LOOP_BACK5_RDLL_END_IO_VAL(val)	    vBIT(val, 9, 7)
	u8	unused02b00[0x02b00 - 0x02ae0];

/* 0x02b00 */	u64	g3cmct_loop_back_rdll[4];
#define	VXGE_HAL_G3CMCT_LOOP_BACK_RDLL_LBCTRL_U_MIN_VAL(val) vBIT(val, 1, 7)
#define	VXGE_HAL_G3CMCT_LOOP_BACK_RDLL_LBCTRL_U_MAX_VAL(val) vBIT(val, 9, 7)
#define	VXGE_HAL_G3CMCT_LOOP_BACK_RDLL_LBCTRL_L_MIN_VAL(val) vBIT(val, 17, 7)
#define	VXGE_HAL_G3CMCT_LOOP_BACK_RDLL_LBCTRL_L_MAX_VAL(val) vBIT(val, 25, 7)
#define	VXGE_HAL_G3CMCT_LOOP_BACK_RDLL_LBCTRL_MON_U_MIN_VAL(val)\
							    vBIT(val, 33, 7)
#define	VXGE_HAL_G3CMCT_LOOP_BACK_RDLL_LBCTRL_MON_U_MAX_VAL(val)\
							    vBIT(val, 41, 7)
#define	VXGE_HAL_G3CMCT_LOOP_BACK_RDLL_LBCTRL_MON_L_MIN_VAL(val)\
							    vBIT(val, 49, 7)
#define	VXGE_HAL_G3CMCT_LOOP_BACK_RDLL_LBCTRL_MON_L_MAX_VAL(val)\
							    vBIT(val, 57, 7)
/* 0x02b20 */	u64	g3cmct_loop_back_wdll[4];
#define	VXGE_HAL_G3CMCT_LOOP_BACK_WDLL_LBCTRL_U_MIN_VAL(val) vBIT(val, 1, 7)
#define	VXGE_HAL_G3CMCT_LOOP_BACK_WDLL_LBCTRL_U_MAX_VAL(val) vBIT(val, 9, 7)
#define	VXGE_HAL_G3CMCT_LOOP_BACK_WDLL_LBCTRL_L_MIN_VAL(val) vBIT(val, 17, 7)
#define	VXGE_HAL_G3CMCT_LOOP_BACK_WDLL_LBCTRL_L_MAX_VAL(val) vBIT(val, 25, 7)
/* 0x02b40 */	u64	g3cmct_tran_wrd_cnt;
#define	VXGE_HAL_G3CMCT_TRAN_WRD_CNT_CTRL_PIPE_WR(val)	    vBIT(val, 0, 32)
#define	VXGE_HAL_G3CMCT_TRAN_WRD_CNT_CTRL_PIPE_RD(val)	    vBIT(val, 32, 32)
/* 0x02b48 */	u64	g3cmct_tran_ap_cnt;
#define	VXGE_HAL_G3CMCT_TRAN_AP_CNT_CTRL_PIPE_ACT(val)	    vBIT(val, 0, 16)
#define	VXGE_HAL_G3CMCT_TRAN_AP_CNT_CTRL_PIPE_PRE(val)	    vBIT(val, 16, 16)
#define	VXGE_HAL_G3CMCT_TRAN_AP_CNT_UPDATE		    mBIT(39)
/* 0x02b50 */	u64	g3cmct_g3bist;
#define	VXGE_HAL_G3CMCT_G3BIST_DISABLE_MAIN		    mBIT(7)
#define	VXGE_HAL_G3CMCT_G3BIST_DISABLE_ICTRL		    mBIT(15)
#define	VXGE_HAL_G3CMCT_G3BIST_BTCTRL_STATUS_MAIN(val)	    vBIT(val, 21, 3)
#define	VXGE_HAL_G3CMCT_G3BIST_BTCTRL_STATUS_ICTRL(val)	    vBIT(val, 29, 3)
	u8	unused03000[0x03000 - 0x02b58];

/* 0x03000 */	u64	mc_int_status;
#define	VXGE_HAL_MC_INT_STATUS_MC_ERR_MC_INT		    mBIT(3)
#define	VXGE_HAL_MC_INT_STATUS_GROCRC_ALARM_ROCRC_INT	    mBIT(7)
#define	VXGE_HAL_MC_INT_STATUS_FAU_GEN_ERR_FAU_GEN_INT	    mBIT(11)
#define	VXGE_HAL_MC_INT_STATUS_FAU_ECC_ERR_FAU_ECC_INT	    mBIT(15)
/* 0x03008 */	u64	mc_int_mask;
/* 0x03010 */	u64	mc_err_reg;
#define	VXGE_HAL_MC_ERR_REG_MC_XFMD_MEM_ECC_SG_ERR_A	    mBIT(3)
#define	VXGE_HAL_MC_ERR_REG_MC_XFMD_MEM_ECC_SG_ERR_B	    mBIT(4)
#define	VXGE_HAL_MC_ERR_REG_MC_G3IF_RD_FIFO_ECC_SG_ERR	    mBIT(5)
#define	VXGE_HAL_MC_ERR_REG_MC_MIRI_ECC_SG_ERR_0	    mBIT(6)
#define	VXGE_HAL_MC_ERR_REG_MC_MIRI_ECC_SG_ERR_1	    mBIT(7)
#define	VXGE_HAL_MC_ERR_REG_MC_XFMD_MEM_ECC_DB_ERR_A	    mBIT(10)
#define	VXGE_HAL_MC_ERR_REG_MC_XFMD_MEM_ECC_DB_ERR_B	    mBIT(11)
#define	VXGE_HAL_MC_ERR_REG_MC_G3IF_RD_FIFO_ECC_DB_ERR	    mBIT(12)
#define	VXGE_HAL_MC_ERR_REG_MC_MIRI_ECC_DB_ERR_0	    mBIT(13)
#define	VXGE_HAL_MC_ERR_REG_MC_MIRI_ECC_DB_ERR_1	    mBIT(14)
#define	VXGE_HAL_MC_ERR_REG_MC_SM_ERR			    mBIT(15)
/* 0x03018 */	u64	mc_err_mask;
/* 0x03020 */	u64	mc_err_alarm;
/* 0x03028 */	u64	grocrc_alarm_reg;
#define	VXGE_HAL_GROCRC_ALARM_REG_XFMD_WR_FIFO_ERR	    mBIT(3)
#define	VXGE_HAL_GROCRC_ALARM_REG_WDE2MSR_RD_FIFO_ERR	    mBIT(7)
/* 0x03030 */	u64	grocrc_alarm_mask;
/* 0x03038 */	u64	grocrc_alarm_alarm;
	u8	unused03100[0x03100 - 0x03040];

/* 0x03100 */	u64	rx_thresh_cfg_repl;
#define	VXGE_HAL_RX_THRESH_CFG_REPL_PAUSE_LOW_THR(val)	    vBIT(val, 0, 8)
#define	VXGE_HAL_RX_THRESH_CFG_REPL_PAUSE_HIGH_THR(val)	    vBIT(val, 8, 8)
#define	VXGE_HAL_RX_THRESH_CFG_REPL_RED_THR_0(val)	    vBIT(val, 16, 8)
#define	VXGE_HAL_RX_THRESH_CFG_REPL_RED_THR_1(val)	    vBIT(val, 24, 8)
#define	VXGE_HAL_RX_THRESH_CFG_REPL_RED_THR_2(val)	    vBIT(val, 32, 8)
#define	VXGE_HAL_RX_THRESH_CFG_REPL_RED_THR_3(val)	    vBIT(val, 40, 8)
#define	VXGE_HAL_RX_THRESH_CFG_REPL_GLOBAL_WOL_EN	    mBIT(62)
#define	VXGE_HAL_RX_THRESH_CFG_REPL_EXACT_VP_MATCH_REQ	    mBIT(63)
/* 0x03108 */	u64	dbg_reg1_0;
#define	VXGE_HAL_DBG_REG1_0_INCTRL_QUEUE0_RX_NON_OFFLOAD_FRM_CNT(val)\
							    vBIT(val, 0, 16)
#define	VXGE_HAL_DBG_REG1_0_INCTRL_QUEUE0_RX_OFFLOAD_FRM_CNT(val)\
							    vBIT(val, 16, 16)
#define	VXGE_HAL_DBG_REG1_0_RP_QUEUE0_NON_OFFLOAD_XMFD_CNT(val)\
							    vBIT(val, 32, 16)
#define	VXGE_HAL_DBG_REG1_0_RP_QUEUE0_OFFLOAD_XFMD_CNT(val) vBIT(val, 48, 16)
/* 0x03110 */	u64	dbg_reg1_1;
#define	VXGE_HAL_DBG_REG1_1_INCTRL_QUEUE1_RX_NON_OFFLOAD_FRM_CNT(val)\
							    vBIT(val, 0, 16)
#define	VXGE_HAL_DBG_REG1_1_INCTRL_QUEUE1_RX_OFFLOAD_FRM_CNT(val)\
							    vBIT(val, 16, 16)
#define	VXGE_HAL_DBG_REG1_1_RP_QUEUE1_NON_OFFLOAD_XMFD_CNT(val)\
							    vBIT(val, 32, 16)
#define	VXGE_HAL_DBG_REG1_1_RP_QUEUE1_OFFLOAD_XFMD_CNT(val) vBIT(val, 48, 16)
/* 0x03118 */	u64	dbg_reg1_2;
#define	VXGE_HAL_DBG_REG1_2_INCTRL_QUEUE2_RX_NON_OFFLOAD_FRM_CNT(val)\
							    vBIT(val, 0, 16)
#define	VXGE_HAL_DBG_REG1_2_INCTRL_QUEUE2_RX_OFFLOAD_FRM_CNT(val)\
							    vBIT(val, 16, 16)
#define	VXGE_HAL_DBG_REG1_2_RP_QUEUE2_NON_OFFLOAD_XMFD_CNT(val)\
							    vBIT(val, 32, 16)
#define	VXGE_HAL_DBG_REG1_2_RP_QUEUE2_OFFLOAD_XFMD_CNT(val) vBIT(val, 48, 16)
/* 0x03120 */	u64	dbg_reg1_3;
#define	VXGE_HAL_DBG_REG1_3_INCTRL_QUEUE3_RX_NON_OFFLOAD_FRM_CNT(val)\
							    vBIT(val, 0, 16)
#define	VXGE_HAL_DBG_REG1_3_INCTRL_QUEUE3_RX_OFFLOAD_FRM_CNT(val)\
							    vBIT(val, 16, 16)
#define	VXGE_HAL_DBG_REG1_3_RP_QUEUE3_NON_OFFLOAD_XMFD_CNT(val)\
							    vBIT(val, 32, 16)
#define	VXGE_HAL_DBG_REG1_3_RP_QUEUE3_OFFLOAD_XFMD_CNT(val) vBIT(val, 48, 16)
/* 0x03128 */	u64	dbg_reg1_4;
#define	VXGE_HAL_DBG_REG1_4_INCTRL_QUEUE4_RX_NON_OFFLOAD_FRM_CNT(val)\
							    vBIT(val, 0, 16)
#define	VXGE_HAL_DBG_REG1_4_INCTRL_QUEUE4_RX_OFFLOAD_FRM_CNT(val)\
							    vBIT(val, 16, 16)
#define	VXGE_HAL_DBG_REG1_4_RP_QUEUE4_NON_OFFLOAD_XMFD_CNT(val)\
							    vBIT(val, 32, 16)
#define	VXGE_HAL_DBG_REG1_4_RP_QUEUE4_OFFLOAD_XFMD_CNT(val) vBIT(val, 48, 16)
/* 0x03130 */	u64	dbg_reg1_5;
#define	VXGE_HAL_DBG_REG1_5_INCTRL_QUEUE5_RX_NON_OFFLOAD_FRM_CNT(val)\
							    vBIT(val, 0, 16)
#define	VXGE_HAL_DBG_REG1_5_INCTRL_QUEUE5_RX_OFFLOAD_FRM_CNT(val)\
							    vBIT(val, 16, 16)
#define	VXGE_HAL_DBG_REG1_5_RP_QUEUE5_NON_OFFLOAD_XMFD_CNT(val)\
							    vBIT(val, 32, 16)
#define	VXGE_HAL_DBG_REG1_5_RP_QUEUE5_OFFLOAD_XFMD_CNT(val) vBIT(val, 48, 16)
/* 0x03138 */	u64	dbg_reg1_6;
#define	VXGE_HAL_DBG_REG1_6_INCTRL_QUEUE6_RX_NON_OFFLOAD_FRM_CNT(val)\
							    vBIT(val, 0, 16)
#define	VXGE_HAL_DBG_REG1_6_INCTRL_QUEUE6_RX_OFFLOAD_FRM_CNT(val)\
							    vBIT(val, 16, 16)
#define	VXGE_HAL_DBG_REG1_6_RP_QUEUE6_NON_OFFLOAD_XMFD_CNT(val)\
							    vBIT(val, 32, 16)
#define	VXGE_HAL_DBG_REG1_6_RP_QUEUE6_OFFLOAD_XFMD_CNT(val) vBIT(val, 48, 16)
/* 0x03140 */	u64	dbg_reg1_7;
#define	VXGE_HAL_DBG_REG1_7_INCTRL_QUEUE7_RX_NON_OFFLOAD_FRM_CNT(val)\
							    vBIT(val, 0, 16)
#define	VXGE_HAL_DBG_REG1_7_INCTRL_QUEUE7_RX_OFFLOAD_FRM_CNT(val)\
							    vBIT(val, 16, 16)
#define	VXGE_HAL_DBG_REG1_7_RP_QUEUE7_NON_OFFLOAD_XMFD_CNT(val)\
							    vBIT(val, 32, 16)
#define	VXGE_HAL_DBG_REG1_7_RP_QUEUE7_OFFLOAD_XFMD_CNT(val) vBIT(val, 48, 16)
/* 0x03148 */	u64	dbg_reg2;
#define	VXGE_HAL_DBG_REG2_XFMDCNT_XFMD_AVAILABLE(val)	    vBIT(val, 6, 18)
#define	VXGE_HAL_DBG_REG2_RP_FBMC_PTM_DATA_PHASES(val)	    vBIT(val, 24, 32)
/* 0x03150 */	u64	dbg_reg3;
#define	VXGE_HAL_DBG_REG3_XFMD_ADV_FBMC_RQA_QUEUE_STROBES(val) vBIT(val, 0, 16)
#define	VXGE_HAL_DBG_REG3_XFMD_ADV_FBMC_RQA_MC_STROBES(val) vBIT(val, 16, 16)
#define	VXGE_HAL_DBG_REG3_XFMD_ADV_RQA_FBMC_QUEUE_SELECT(val) vBIT(val, 32, 16)
#define	VXGE_HAL_DBG_REG3_XFMD_ADV_RQA_FBMC_MC_SELECT(val)  vBIT(val, 48, 16)
/* 0x03158 */	u64	dbg_reg4;
#define	VXGE_HAL_DBG_REG4_RP_FBMC_ONE_HEADERS(val)	    vBIT(val, 0, 16)
/* 0x03160 */	u64	dbg_reg5;
#define	VXGE_HAL_DBG_REG5_INCTRL_TOTAL_ING_FRMS(val)	    vBIT(val, 0, 32)
#define	VXGE_HAL_DBG_REG5_RP_TOTAL_EGR_FRMS(val)	    vBIT(val, 32, 32)
	u8	unused03200[0x03200 - 0x03168];

/* 0x03200 */	u64	rx_queue_cfg;
#define	VXGE_HAL_RX_QUEUE_CFG_QUEUE_SIZE_ENABLE		    mBIT(39)
#define	VXGE_HAL_RX_QUEUE_CFG_INGRESS_FIFO_THR(val)	    vBIT(val, 60, 4)
/* 0x03208 */	u64	rx_queue_size_q[15];
#define	VXGE_HAL_RX_QUEUE_SIZE_Q_SIZE(val)		    vBIT(val, 0, 24)
#define	VXGE_HAL_RX_QUEUE_SIZE_Q_LAST_ADD(val)		    vBIT(val, 24, 24)
/* 0x03280 */	u64	rx_queue_size_q15;
#define	VXGE_HAL_RX_QUEUE_SIZE_Q15_SIZE(val)		    vBIT(val, 0, 24)
#define	VXGE_HAL_RX_QUEUE_SIZE_Q15_LAST_ADD(val)	    vBIT(val, 24, 24)
/* 0x03288 */	u64	rx_queue_size_q16;
#define	VXGE_HAL_RX_QUEUE_SIZE_Q16_SIZE(val)		    vBIT(val, 0, 24)
#define	VXGE_HAL_RX_QUEUE_SIZE_Q16_LAST_ADD(val)	    vBIT(val, 24, 24)
/* 0x03290 */	u64	rx_queue_size_q17;
#define	VXGE_HAL_RX_QUEUE_SIZE_Q17_SIZE(val)		    vBIT(val, 0, 24)
#define	VXGE_HAL_RX_QUEUE_SIZE_Q17_LAST_ADD(val)	    vBIT(val, 24, 24)
	u8	unused032a0[0x032a0 - 0x03298];

/* 0x032a0 */	u64	rx_queue_start_q0;
#define	VXGE_HAL_RX_QUEUE_START_Q0_QUEUE_BANKS(val)	    vBIT(val, 6, 2)
#define	VXGE_HAL_RX_QUEUE_START_Q0_SBANK(val)		    vBIT(val, 13, 3)
#define	VXGE_HAL_RX_QUEUE_START_Q0_SROW(val)		    vBIT(val, 18, 14)
#define	VXGE_HAL_RX_QUEUE_START_Q0_FDP_OFFLOAD_OUTST_FRMS(val) vBIT(val, 39, 9)
#define	VXGE_HAL_RX_QUEUE_START_Q0_FDP_NONOFFLOAD_OUTST_FRMS(val)\
							    vBIT(val, 55, 9)
/* 0x032a8 */	u64	rx_queue_start_q1;
#define	VXGE_HAL_RX_QUEUE_START_Q1_QUEUE_BANKS(val)	    vBIT(val, 6, 2)
#define	VXGE_HAL_RX_QUEUE_START_Q1_SBANK(val)		    vBIT(val, 13, 3)
#define	VXGE_HAL_RX_QUEUE_START_Q1_SROW(val)		    vBIT(val, 18, 14)
#define	VXGE_HAL_RX_QUEUE_START_Q1_FDP_OFFLOAD_OUTST_FRMS(val) vBIT(val, 39, 9)
#define	VXGE_HAL_RX_QUEUE_START_Q1_FDP_NONOFFLOAD_OUTST_FRMS(val)\
							    vBIT(val, 55, 9)
/* 0x032b0 */	u64	rx_queue_start_q2;
#define	VXGE_HAL_RX_QUEUE_START_Q2_QUEUE_BANKS(val)	    vBIT(val, 6, 2)
#define	VXGE_HAL_RX_QUEUE_START_Q2_SBANK(val)		    vBIT(val, 13, 3)
#define	VXGE_HAL_RX_QUEUE_START_Q2_SROW(val)		    vBIT(val, 18, 14)
#define	VXGE_HAL_RX_QUEUE_START_Q2_FDP_OFFLOAD_OUTST_FRMS(val)\
							    vBIT(val, 39, 9)
#define	VXGE_HAL_RX_QUEUE_START_Q2_FDP_NONOFFLOAD_OUTST_FRMS(val)\
							    vBIT(val, 55, 9)
/* 0x032b8 */	u64	rx_queue_start_q3;
#define	VXGE_HAL_RX_QUEUE_START_Q3_QUEUE_BANKS(val)	    vBIT(val, 6, 2)
#define	VXGE_HAL_RX_QUEUE_START_Q3_SBANK(val)		    vBIT(val, 13, 3)
#define	VXGE_HAL_RX_QUEUE_START_Q3_SROW(val)		    vBIT(val, 18, 14)
#define	VXGE_HAL_RX_QUEUE_START_Q3_FDP_OFFLOAD_OUTST_FRMS(val)\
							    vBIT(val, 39, 9)
#define	VXGE_HAL_RX_QUEUE_START_Q3_FDP_NONOFFLOAD_OUTST_FRMS(val)\
							    vBIT(val, 55, 9)
/* 0x032c0 */	u64	rx_queue_start_q4;
#define	VXGE_HAL_RX_QUEUE_START_Q4_QUEUE_BANKS(val)	    vBIT(val, 6, 2)
#define	VXGE_HAL_RX_QUEUE_START_Q4_SBANK(val)		    vBIT(val, 13, 3)
#define	VXGE_HAL_RX_QUEUE_START_Q4_SROW(val)		    vBIT(val, 18, 14)
#define	VXGE_HAL_RX_QUEUE_START_Q4_FDP_OFFLOAD_OUTST_FRMS(val)\
							    vBIT(val, 39, 9)
#define	VXGE_HAL_RX_QUEUE_START_Q4_FDP_NONOFFLOAD_OUTST_FRMS(val)\
							    vBIT(val, 55, 9)
/* 0x032c8 */	u64	rx_queue_start_q5;
#define	VXGE_HAL_RX_QUEUE_START_Q5_QUEUE_BANKS(val)	    vBIT(val, 6, 2)
#define	VXGE_HAL_RX_QUEUE_START_Q5_SBANK(val)		    vBIT(val, 13, 3)
#define	VXGE_HAL_RX_QUEUE_START_Q5_SROW(val)		    vBIT(val, 18, 14)
#define	VXGE_HAL_RX_QUEUE_START_Q5_FDP_OFFLOAD_OUTST_FRMS(val) vBIT(val, 39, 9)
#define	VXGE_HAL_RX_QUEUE_START_Q5_FDP_NONOFFLOAD_OUTST_FRMS(val)\
							    vBIT(val, 55, 9)
/* 0x032d0 */	u64	rx_queue_start_q6;
#define	VXGE_HAL_RX_QUEUE_START_Q6_QUEUE_BANKS(val)	    vBIT(val, 6, 2)
#define	VXGE_HAL_RX_QUEUE_START_Q6_SBANK(val)		    vBIT(val, 13, 3)
#define	VXGE_HAL_RX_QUEUE_START_Q6_SROW(val)		    vBIT(val, 18, 14)
#define	VXGE_HAL_RX_QUEUE_START_Q6_FDP_OFFLOAD_OUTST_FRMS(val) vBIT(val, 39, 9)
#define	VXGE_HAL_RX_QUEUE_START_Q6_FDP_NONOFFLOAD_OUTST_FRMS(val)\
							    vBIT(val, 55, 9)
/* 0x032d8 */	u64	rx_queue_start_q7;
#define	VXGE_HAL_RX_QUEUE_START_Q7_QUEUE_BANKS(val)	    vBIT(val, 6, 2)
#define	VXGE_HAL_RX_QUEUE_START_Q7_SBANK(val)		    vBIT(val, 13, 3)
#define	VXGE_HAL_RX_QUEUE_START_Q7_SROW(val)		    vBIT(val, 18, 14)
#define	VXGE_HAL_RX_QUEUE_START_Q7_FDP_OFFLOAD_OUTST_FRMS(val) vBIT(val, 39, 9)
#define	VXGE_HAL_RX_QUEUE_START_Q7_FDP_NONOFFLOAD_OUTST_FRMS(val)\
							    vBIT(val, 55, 9)
/* 0x032e0 */	u64	rx_queue_start_q8;
#define	VXGE_HAL_RX_QUEUE_START_Q8_QUEUE_BANKS(val)	    vBIT(val, 6, 2)
#define	VXGE_HAL_RX_QUEUE_START_Q8_SBANK(val)		    vBIT(val, 13, 3)
#define	VXGE_HAL_RX_QUEUE_START_Q8_SROW(val)		    vBIT(val, 18, 14)
#define	VXGE_HAL_RX_QUEUE_START_Q8_FDP_OFFLOAD_OUTST_FRMS(val) vBIT(val, 39, 9)
/* 0x032e8 */	u64	rx_queue_start_q9;
#define	VXGE_HAL_RX_QUEUE_START_Q9_QUEUE_BANKS(val)	    vBIT(val, 6, 2)
#define	VXGE_HAL_RX_QUEUE_START_Q9_SBANK(val)		    vBIT(val, 13, 3)
#define	VXGE_HAL_RX_QUEUE_START_Q9_SROW(val)		    vBIT(val, 18, 14)
#define	VXGE_HAL_RX_QUEUE_START_Q9_FDP_OFFLOAD_OUTST_FRMS(val) vBIT(val, 39, 9)
/* 0x032f0 */	u64	rx_queue_start_q10;
#define	VXGE_HAL_RX_QUEUE_START_Q10_QUEUE_BANKS(val)	    vBIT(val, 6, 2)
#define	VXGE_HAL_RX_QUEUE_START_Q10_SBANK(val)		    vBIT(val, 13, 3)
#define	VXGE_HAL_RX_QUEUE_START_Q10_SROW(val)		    vBIT(val, 18, 14)
#define	VXGE_HAL_RX_QUEUE_START_Q10_FDP_OFFLOAD_OUTST_FRMS(val) vBIT(val, 39, 9)
/* 0x032f8 */	u64	rx_queue_start_q11;
#define	VXGE_HAL_RX_QUEUE_START_Q11_QUEUE_BANKS(val)	    vBIT(val, 6, 2)
#define	VXGE_HAL_RX_QUEUE_START_Q11_SBANK(val)		    vBIT(val, 13, 3)
#define	VXGE_HAL_RX_QUEUE_START_Q11_SROW(val)		    vBIT(val, 18, 14)
#define	VXGE_HAL_RX_QUEUE_START_Q11_FDP_OFFLOAD_OUTST_FRMS(val) vBIT(val, 39, 9)
/* 0x03300 */	u64	rx_queue_start_q12;
#define	VXGE_HAL_RX_QUEUE_START_Q12_QUEUE_BANKS(val)	    vBIT(val, 6, 2)
#define	VXGE_HAL_RX_QUEUE_START_Q12_SBANK(val)		    vBIT(val, 13, 3)
#define	VXGE_HAL_RX_QUEUE_START_Q12_SROW(val)		    vBIT(val, 18, 14)
#define	VXGE_HAL_RX_QUEUE_START_Q12_FDP_OFFLOAD_OUTST_FRMS(val) vBIT(val, 39, 9)
/* 0x03308 */	u64	rx_queue_start_q13;
#define	VXGE_HAL_RX_QUEUE_START_Q13_QUEUE_BANKS(val)	    vBIT(val, 6, 2)
#define	VXGE_HAL_RX_QUEUE_START_Q13_SBANK(val)		    vBIT(val, 13, 3)
#define	VXGE_HAL_RX_QUEUE_START_Q13_SROW(val)		    vBIT(val, 18, 14)
#define	VXGE_HAL_RX_QUEUE_START_Q13_FDP_OFFLOAD_OUTST_FRMS(val) vBIT(val, 39, 9)
/* 0x03310 */	u64	rx_queue_start_q14;
#define	VXGE_HAL_RX_QUEUE_START_Q14_QUEUE_BANKS(val)	    vBIT(val, 6, 2)
#define	VXGE_HAL_RX_QUEUE_START_Q14_SBANK(val)		    vBIT(val, 13, 3)
#define	VXGE_HAL_RX_QUEUE_START_Q14_SROW(val)		    vBIT(val, 18, 14)
#define	VXGE_HAL_RX_QUEUE_START_Q14_FDP_OFFLOAD_OUTST_FRMS(val)	vBIT(val, 39, 9)
/* 0x03318 */	u64	rx_queue_start_q15;
#define	VXGE_HAL_RX_QUEUE_START_Q15_QUEUE_BANKS(val)	    vBIT(val, 6, 2)
#define	VXGE_HAL_RX_QUEUE_START_Q15_SBANK(val)		    vBIT(val, 13, 3)
#define	VXGE_HAL_RX_QUEUE_START_Q15_SROW(val)		    vBIT(val, 18, 14)
#define	VXGE_HAL_RX_QUEUE_START_Q15_FDP_OFFLOAD_OUTST_FRMS(val)	vBIT(val, 39, 9)
/* 0x03320 */	u64	rx_queue_start_q16;
#define	VXGE_HAL_RX_QUEUE_START_Q16_QUEUE_BANKS(val)	    vBIT(val, 6, 2)
#define	VXGE_HAL_RX_QUEUE_START_Q16_SBANK(val)		    vBIT(val, 13, 3)
#define	VXGE_HAL_RX_QUEUE_START_Q16_SROW(val)		    vBIT(val, 18, 14)
#define	VXGE_HAL_RX_QUEUE_START_Q16_FDP_OFFLOAD_OUTST_FRMS(val)	vBIT(val, 39, 9)
/* 0x03328 */	u64	rx_queue_start_q17;
#define	VXGE_HAL_RX_QUEUE_START_Q17_QUEUE_BANKS(val)	    vBIT(val, 6, 2)
#define	VXGE_HAL_RX_QUEUE_START_Q17_SBANK(val)		    vBIT(val, 13, 3)
#define	VXGE_HAL_RX_QUEUE_START_Q17_SROW(val)		    vBIT(val, 18, 14)
/* 0x03330 */	u64	fm_definition;
#define	VXGE_HAL_FM_DEFINITION_FM_SIZE(val)		    vBIT(val, 6, 2)
#define	VXGE_HAL_FM_DEFINITION_FM_COLUMNS(val)		    vBIT(val, 14, 2)
#define	VXGE_HAL_FM_DEFINITION_QUEUE_SPAV_MARGIN(val)	    vBIT(val, 16, 8)
	u8	unused03380[0x03380 - 0x03338];

/* 0x03380 */	u64	traffic_ctrl;
#define	VXGE_HAL_TRAFFIC_CTRL_BLOCK_ING_PATH		    mBIT(7)
#define	VXGE_HAL_TRAFFIC_CTRL_BLOCK_EGR_PATH		    mBIT(15)
#define	VXGE_HAL_TRAFFIC_CTRL_OFFLOAD_MAX_FRAMES(val)	    vBIT(val, 24, 8)
#define	VXGE_HAL_TRAFFIC_CTRL_NOFFLOAD_MAX_FRAMES(val)	    vBIT(val, 32, 8)
#define	VXGE_HAL_TRAFFIC_CTRL_MSP_MAX_FRAMES(val)	    vBIT(val, 40, 8)
/* 0x03388 */	u64	xfmd_arb_ctrl;
#define	VXGE_HAL_XFMD_ARB_CTRL_ISTAGE_MASK		    mBIT(7)
#define	VXGE_HAL_XFMD_ARB_CTRL_EN_OFF(val)		    vBIT(val, 15, 17)
#define	VXGE_HAL_XFMD_ARB_CTRL_EN_NOFF(val)		    vBIT(val, 39, 17)
/* 0x03390 */	u64	xfmd_arb_ctrl1;
#define	VXGE_HAL_XFMD_ARB_CTRL1_PROMOTE_NOFF(val)	    vBIT(val, 6, 18)
/* 0x03398 */	u64	rd_tranc_ctrl;
#define	VXGE_HAL_RD_TRANC_CTRL_ARB(val)			    vBIT(val, 4, 4)
/* 0x033a0 */	u64	fm_arb;
#define	VXGE_HAL_FM_ARB_CTRL(val)			    vBIT(val, 0, 8)
#define	VXGE_HAL_FM_ARB_TIMER(val)			    vBIT(val, 8, 8)
#define	VXGE_HAL_FM_ARB_EN_QHIST(val)			    vBIT(val, 16, 8)
#define	VXGE_HAL_FM_ARB_ACT_ARB_QHIST(val)		    vBIT(val, 28, 4)
#define	VXGE_HAL_FM_ARB_QHIST_CNT(val)			    vBIT(val, 32, 16)
#define	VXGE_HAL_FM_ARB_WR_DELAY_CNT(val)		    vBIT(val, 52, 4)
#define	VXGE_HAL_FM_ARB_WR_WINDOW_CNT(val)		    vBIT(val, 56, 8)
/* 0x033a8 */	u64	arb;
#define	VXGE_HAL_ARB_HP_CAL(val)			    vBIT(val, 0, 8)
#define	VXGE_HAL_ARB_XFMD_LAST_MASK(val)		    vBIT(val, 11, 5)
#define	VXGE_HAL_ARB_HP_XFMD_PRI(val)			    vBIT(val, 22, 2)
/* 0x033b0 */	u64	settings0;
#define	VXGE_HAL_SETTINGS0_CTRL_FIFO_THR(val)		    vBIT(val, 4, 4)
/* 0x033b8 */	u64	fbmc_ecc_cfg;
#define	VXGE_HAL_FBMC_ECC_CFG_ENABLE(val)		    vBIT(val, 3, 5)
	u8	unused03400[0x03400 - 0x033c0];

/* 0x03400 */	u64	pcipif_int_status;
#define	VXGE_HAL_PCIPIF_INT_STATUS_DBECC_ERR_DBECC_ERR_INT  mBIT(3)
#define	VXGE_HAL_PCIPIF_INT_STATUS_SBECC_ERR_SBECC_ERR_INT  mBIT(7)
#define	VXGE_HAL_PCIPIF_INT_STATUS_GENERAL_ERR_GENERAL_ERR_INT mBIT(11)
#define	VXGE_HAL_PCIPIF_INT_STATUS_SRPCIM_MSG_SRPCIM_MSG_INT mBIT(15)
#define	VXGE_HAL_PCIPIF_INT_STATUS_MRPCIM_SPARE_R1_MRPCIM_SPARE_R1_INT mBIT(19)
/* 0x03408 */	u64	pcipif_int_mask;
/* 0x03410 */	u64	dbecc_err_reg;
#define	VXGE_HAL_DBECC_ERR_REG_PCI_RETRY_BUF_DB_ERR	    mBIT(3)
#define	VXGE_HAL_DBECC_ERR_REG_PCI_RETRY_SOT_DB_ERR	    mBIT(7)
#define	VXGE_HAL_DBECC_ERR_REG_PCI_P_HDR_DB_ERR		    mBIT(11)
#define	VXGE_HAL_DBECC_ERR_REG_PCI_P_DATA_DB_ERR	    mBIT(15)
#define	VXGE_HAL_DBECC_ERR_REG_PCI_NP_HDR_DB_ERR	    mBIT(19)
#define	VXGE_HAL_DBECC_ERR_REG_PCI_NP_DATA_DB_ERR	    mBIT(23)
/* 0x03418 */	u64	dbecc_err_mask;
/* 0x03420 */	u64	dbecc_err_alarm;
/* 0x03428 */	u64	sbecc_err_reg;
#define	VXGE_HAL_SBECC_ERR_REG_PCI_RETRY_BUF_SG_ERR	    mBIT(3)
#define	VXGE_HAL_SBECC_ERR_REG_PCI_RETRY_SOT_SG_ERR	    mBIT(7)
#define	VXGE_HAL_SBECC_ERR_REG_PCI_P_HDR_SG_ERR		    mBIT(11)
#define	VXGE_HAL_SBECC_ERR_REG_PCI_P_DATA_SG_ERR	    mBIT(15)
#define	VXGE_HAL_SBECC_ERR_REG_PCI_NP_HDR_SG_ERR	    mBIT(19)
#define	VXGE_HAL_SBECC_ERR_REG_PCI_NP_DATA_SG_ERR	    mBIT(23)
/* 0x03430 */	u64	sbecc_err_mask;
/* 0x03438 */	u64	sbecc_err_alarm;
/* 0x03440 */	u64	general_err_reg;
#define	VXGE_HAL_GENERAL_ERR_REG_PCI_DROPPED_ILLEGAL_CFG    mBIT(3)
#define	VXGE_HAL_GENERAL_ERR_REG_PCI_ILLEGAL_MEM_MAP_PROG   mBIT(7)
#define	VXGE_HAL_GENERAL_ERR_REG_PCI_LINK_RST_FSM_ERR	    mBIT(11)
#define	VXGE_HAL_GENERAL_ERR_REG_PCI_RX_ILLEGAL_TLP_VPLANE  mBIT(15)
#define	VXGE_HAL_GENERAL_ERR_REG_PCI_TRAINING_RESET_DET	    mBIT(19)
#define	VXGE_HAL_GENERAL_ERR_REG_PCI_PCI_LINK_DOWN_DET	    mBIT(23)
#define	VXGE_HAL_GENERAL_ERR_REG_PCI_RESET_ACK_DLLP	    mBIT(27)
/* 0x03448 */	u64	general_err_mask;
/* 0x03450 */	u64	general_err_alarm;
/* 0x03458 */	u64	srpcim_msg_reg;
#define	VXGE_HAL_SRPCIM_MSG_REG_SWIF_SRPCIM_TO_MRPCIM_VPLANE0_RMSG_INT	mBIT(0)
#define	VXGE_HAL_SRPCIM_MSG_REG_SWIF_SRPCIM_TO_MRPCIM_VPLANE1_RMSG_INT	mBIT(1)
#define	VXGE_HAL_SRPCIM_MSG_REG_SWIF_SRPCIM_TO_MRPCIM_VPLANE2_RMSG_INT	mBIT(2)
#define	VXGE_HAL_SRPCIM_MSG_REG_SWIF_SRPCIM_TO_MRPCIM_VPLANE3_RMSG_INT	mBIT(3)
#define	VXGE_HAL_SRPCIM_MSG_REG_SWIF_SRPCIM_TO_MRPCIM_VPLANE4_RMSG_INT	mBIT(4)
#define	VXGE_HAL_SRPCIM_MSG_REG_SWIF_SRPCIM_TO_MRPCIM_VPLANE5_RMSG_INT	mBIT(5)
#define	VXGE_HAL_SRPCIM_MSG_REG_SWIF_SRPCIM_TO_MRPCIM_VPLANE6_RMSG_INT	mBIT(6)
#define	VXGE_HAL_SRPCIM_MSG_REG_SWIF_SRPCIM_TO_MRPCIM_VPLANE7_RMSG_INT	mBIT(7)
#define	VXGE_HAL_SRPCIM_MSG_REG_SWIF_SRPCIM_TO_MRPCIM_VPLANE8_RMSG_INT	mBIT(8)
#define	VXGE_HAL_SRPCIM_MSG_REG_SWIF_SRPCIM_TO_MRPCIM_VPLANE9_RMSG_INT	mBIT(9)
#define	VXGE_HAL_SRPCIM_MSG_REG_SWIF_SRPCIM_TO_MRPCIM_VPLANE10_RMSG_INT	mBIT(10)
#define	VXGE_HAL_SRPCIM_MSG_REG_SWIF_SRPCIM_TO_MRPCIM_VPLANE11_RMSG_INT	mBIT(11)
#define	VXGE_HAL_SRPCIM_MSG_REG_SWIF_SRPCIM_TO_MRPCIM_VPLANE12_RMSG_INT	mBIT(12)
#define	VXGE_HAL_SRPCIM_MSG_REG_SWIF_SRPCIM_TO_MRPCIM_VPLANE13_RMSG_INT	mBIT(13)
#define	VXGE_HAL_SRPCIM_MSG_REG_SWIF_SRPCIM_TO_MRPCIM_VPLANE14_RMSG_INT	mBIT(14)
#define	VXGE_HAL_SRPCIM_MSG_REG_SWIF_SRPCIM_TO_MRPCIM_VPLANE15_RMSG_INT	mBIT(15)
#define	VXGE_HAL_SRPCIM_MSG_REG_SWIF_SRPCIM_TO_MRPCIM_VPLANE16_RMSG_INT	mBIT(16)
/* 0x03460 */	u64	srpcim_msg_mask;
/* 0x03468 */	u64	srpcim_msg_alarm;
	u8	unused03600[0x03600 - 0x03470];

/* 0x03600 */	u64	gcmg1_int_status;
#define	VXGE_HAL_GCMG1_INT_STATUS_GSSCC_ERR_GSSCC_INT	    mBIT(0)
#define	VXGE_HAL_GCMG1_INT_STATUS_GSSC0_ERR0_GSSC0_0_INT    mBIT(1)
#define	VXGE_HAL_GCMG1_INT_STATUS_GSSC0_ERR1_GSSC0_1_INT    mBIT(2)
#define	VXGE_HAL_GCMG1_INT_STATUS_GSSC1_ERR0_GSSC1_0_INT    mBIT(3)
#define	VXGE_HAL_GCMG1_INT_STATUS_GSSC1_ERR1_GSSC1_1_INT    mBIT(4)
#define	VXGE_HAL_GCMG1_INT_STATUS_GSSC2_ERR0_GSSC2_0_INT    mBIT(5)
#define	VXGE_HAL_GCMG1_INT_STATUS_GSSC2_ERR1_GSSC2_1_INT    mBIT(6)
#define	VXGE_HAL_GCMG1_INT_STATUS_UQM_ERR_UQM_INT	    mBIT(7)
#define	VXGE_HAL_GCMG1_INT_STATUS_GQCC_ERR_GQCC_INT	    mBIT(8)
/* 0x03608 */	u64	gcmg1_int_mask;
/* 0x03610 */	u64	gsscc_err_reg;
#define	VXGE_HAL_GSSCC_ERR_REG_SSCC_SSR_SG_ERR(val)	    vBIT(val, 6, 2)
#define	VXGE_HAL_GSSCC_ERR_REG_SSCC_TSR_SG_ERR(val)	    vBIT(val, 10, 6)
#define	VXGE_HAL_GSSCC_ERR_REG_SSCC_OVERLAPPING_SYNC_ERR    mBIT(23)
#define	VXGE_HAL_GSSCC_ERR_REG_SSCC_SSR_DB_ERR(val)	    vBIT(val, 38, 2)
#define	VXGE_HAL_GSSCC_ERR_REG_SSCC_TSR_DB_ERR(val)	    vBIT(val, 42, 6)
#define	VXGE_HAL_GSSCC_ERR_REG_SSCC_CP2STE_UFLOW_ERR	    mBIT(55)
#define	VXGE_HAL_GSSCC_ERR_REG_SSCC_CP2TTE_UFLOW_ERR	    mBIT(63)
/* 0x03618 */	u64	gsscc_err_mask;
/* 0x03620 */	u64	gsscc_err_alarm;
/* 0x03628 */	u64	gssc_err0_reg[3];
#define	VXGE_HAL_GSSC_ERR0_REG_SSCC_STATE_SG_ERR(val)	    vBIT(val, 0, 8)
#define	VXGE_HAL_GSSC_ERR0_REG_SSCC_CM_RESP_SG_ERR(val)	    vBIT(val, 12, 4)
#define	VXGE_HAL_GSSC_ERR0_REG_SSCC_SSR_RESP_SG_ERR(val)    vBIT(val, 22, 2)
#define	VXGE_HAL_GSSC_ERR0_REG_SSCC_TSR_RESP_SG_ERR(val)    vBIT(val, 26, 6)
#define	VXGE_HAL_GSSC_ERR0_REG_SSCC_STATE_DB_ERR(val)	    vBIT(val, 32, 8)
#define	VXGE_HAL_GSSC_ERR0_REG_SSCC_CM_RESP_DB_ERR(val)	    vBIT(val, 44, 4)
#define	VXGE_HAL_GSSC_ERR0_REG_SSCC_SSR_RESP_DB_ERR(val)    vBIT(val, 54, 2)
#define	VXGE_HAL_GSSC_ERR0_REG_SSCC_TSR_RESP_DB_ERR(val)    vBIT(val, 58, 6)
/* 0x03630 */	u64	gssc_err0_mask[3];
/* 0x03638 */	u64	gssc_err0_alarm[3];
/* 0x03670 */	u64	gssc_err1_reg[3];
#define	VXGE_HAL_GSSC_ERR1_REG_SSCC_CM_RESP_DB_ERR	    mBIT(0)
#define	VXGE_HAL_GSSC_ERR1_REG_SSCC_SCREQ_ERR		    mBIT(1)
#define	VXGE_HAL_GSSC_ERR1_REG_SSCC_CM_RESP_OFLOW_ERR	    mBIT(2)
#define	VXGE_HAL_GSSC_ERR1_REG_SSCC_CM_RESP_R_WN_ERR	    mBIT(3)
#define	VXGE_HAL_GSSC_ERR1_REG_SSCC_CM_RESP_UFLOW_ERR	    mBIT(4)
#define	VXGE_HAL_GSSC_ERR1_REG_SSCC_CM_REQ_OFLOW_ERR	    mBIT(5)
#define	VXGE_HAL_GSSC_ERR1_REG_SSCC_CM_REQ_UFLOW_ERR	    mBIT(6)
#define	VXGE_HAL_GSSC_ERR1_REG_SSCC_FSM_OFLOW_ERR	    mBIT(7)
#define	VXGE_HAL_GSSC_ERR1_REG_SSCC_FSM_UFLOW_ERR	    mBIT(8)
#define	VXGE_HAL_GSSC_ERR1_REG_SSCC_SSR_REQ_OFLOW_ERR	    mBIT(9)
#define	VXGE_HAL_GSSC_ERR1_REG_SSCC_SSR_REQ_UFLOW_ERR	    mBIT(10)
#define	VXGE_HAL_GSSC_ERR1_REG_SSCC_SSR_RESP_OFLOW_ERR	    mBIT(11)
#define	VXGE_HAL_GSSC_ERR1_REG_SSCC_SSR_RESP_R_WN_ERR	    mBIT(12)
#define	VXGE_HAL_GSSC_ERR1_REG_SSCC_SSR_RESP_UFLOW_ERR	    mBIT(13)
#define	VXGE_HAL_GSSC_ERR1_REG_SSCC_TSR_REQ_OFLOW_ERR	    mBIT(14)
#define	VXGE_HAL_GSSC_ERR1_REG_SSCC_TSR_REQ_UFLOW_ERR	    mBIT(15)
#define	VXGE_HAL_GSSC_ERR1_REG_SSCC_TSR_RESP_OFLOW_ERR	    mBIT(16)
#define	VXGE_HAL_GSSC_ERR1_REG_SSCC_TSR_RESP_R_WN_ERR	    mBIT(17)
#define	VXGE_HAL_GSSC_ERR1_REG_SSCC_TSR_RESP_UFLOW_ERR	    mBIT(18)
#define	VXGE_HAL_GSSC_ERR1_REG_SSCC_SCRESP_ERR		    mBIT(19)
/* 0x03678 */	u64	gssc_err1_mask[3];
/* 0x03680 */	u64	gssc_err1_alarm[3];
/* 0x036b8 */	u64	gqcc_err_reg;
#define	VXGE_HAL_GQCC_ERR_REG_QCC_CQM_CACHE_PB_SG_ERR(val)  vBIT(val, 0, 4)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_SQM_CACHE_PB_SG_ERR(val)  vBIT(val, 4, 4)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_CQM_CACHE_PB_DB_ERR(val)  vBIT(val, 8, 4)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_SQM_CACHE_PB_DB_ERR(val)  vBIT(val, 12, 4)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_CQM_CCMREQCMD_FIFO_ERR    mBIT(16)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_CQM_CCMREQDAT_FIFO_ERR    mBIT(17)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_CQM_CCM_CAM_FIFO_PUSH_ERR mBIT(18)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_CQM_CCM_CAM_EIP_FIFO_PUSH_ERR	mBIT(19)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_CQM_CCM2CMA_FIFO_POP_ERR  mBIT(20)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_SQM_CCM_CAM_FIFO_PUSH_ERR mBIT(24)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_SQM_CCM_CAM_EIP_FIFO_PUSH_ERR	mBIT(25)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_SQM_CCM2CMA_LP_FIFO_POP_ERR mBIT(26)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_SQM_CCM2CMA_HP_FIFO_POP_ERR mBIT(27)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_SQM_WSE2CMA_FIFO_POP_ERR  mBIT(28)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_SQM_RRP2CMA_LP_FIFO_POP_ERR mBIT(29)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_SQM_RRP2CMA_HP_FIFO_POP_ERR mBIT(30)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_SQM_IPWOGRRESP_FIFO_POP_ERR mBIT(31)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_SQM_LPRPEDAT_FIFO_ERR	    mBIT(32)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_SQM_LPWRRESP_FIFO_PUSH_ERR mBIT(33)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_SQM_LPCMCREQCMD_ERR	    mBIT(34)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_SQM_HPCMCREQCMD_ERR	    mBIT(35)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_SQM_CMCREQDAT_ERR	    mBIT(36)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_CQM_CMA_CMR_SM_ERR	    mBIT(41)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_CQM_CMA_CAR_SM_ERR	    mBIT(42)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_SQM_CMA_HCMR_SM_ERR	    mBIT(43)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_SQM_CMA_LCMR_SM_ERR	    mBIT(44)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_SQM_CMA_CAR_SM_ERR	    mBIT(45)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_CQM_CMA_CMR_INFO_ERR	    mBIT(55)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_SQM_CMA_WSE_WQE_RD_ERR    mBIT(56)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_SQM_CMA2WGM_NEXT_WQE_PTR_ERR mBIT(57)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_SQM_CMA2RLM_RMV_DATA_ERR  mBIT(58)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_SQM_CMA2DLM_RMV_DATA_ERR  mBIT(59)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_SQM_CMA2ELM_RMV_DATA_ERR  mBIT(60)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_CQM_CMA2CGM_CQEGRP_ROW_DATA_ERR mBIT(61)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_CQM_CMA2RLM_RMV_DATA_ERR  mBIT(62)
#define	VXGE_HAL_GQCC_ERR_REG_QCC_CQM_CMA2ELM_RMV_DATA_ERR  mBIT(63)
/* 0x036c0 */	u64	gqcc_err_mask;
/* 0x036c8 */	u64	gqcc_err_alarm;
/* 0x036d0 */	u64	uqm_err_reg;
#define	VXGE_HAL_UQM_ERR_REG_UQM_UQM_CMCREQ_ECC_SG_ERR	    mBIT(0)
#define	VXGE_HAL_UQM_ERR_REG_UQM_UQM_CMCREQ_ECC_DB_ERR	    mBIT(1)
#define	VXGE_HAL_UQM_ERR_REG_UQM_UQM_SM_ERR		    mBIT(8)
/* 0x036d8 */	u64	uqm_err_mask;
/* 0x036e0 */	u64	uqm_err_alarm;
/* 0x036e8 */	u64	sscc_config;
#define	VXGE_HAL_SSCC_CONFIG_HIT_SCHASH_INDEX_MSB(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_SSCC_CONFIG_HIT_SCHASH_INDEX_LSB(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_SSCC_CONFIG_TIMEOUT_VALUE(val)		    vBIT(val, 16, 16)
#define	VXGE_HAL_SSCC_CONFIG_ALLOW_NOTFOUND_CACHING	    mBIT(39)
#define	VXGE_HAL_SSCC_CONFIG_ALRO_SCHASH_INDEX_MSB(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_SSCC_CONFIG_ALRO_SCHASH_INDEX_LSB(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_SSCC_CONFIG_NULL_LOOKUP		    mBIT(63)
/* 0x036f0 */	u64	sscc_mask_0;
#define	VXGE_HAL_SSCC_MASK_0_IPV6_SA_TOP(val)		    vBIT(val, 0, 64)
/* 0x036f8 */	u64	sscc_mask_1;
#define	VXGE_HAL_SSCC_MASK_1_IPV6_SA_BOTTOM(val)	    vBIT(val, 0, 64)
/* 0x03700 */	u64	sscc_mask_2;
#define	VXGE_HAL_SSCC_MASK_2_IPV6_DA_TOP(val)		    vBIT(val, 0, 64)
/* 0x03708 */	u64	sscc_mask_3;
#define	VXGE_HAL_SSCC_MASK_3_IPV6_DA_BOTTOM(val)	    vBIT(val, 0, 64)
/* 0x03710 */	u64	sscc_mask_4;
#define	VXGE_HAL_SSCC_MASK_4_IPV4_SA(val)		    vBIT(val, 0, 32)
#define	VXGE_HAL_SSCC_MASK_4_IPV4_DA(val)		    vBIT(val, 32, 32)
/* 0x03718 */	u64	sscc_mask_5;
#define	VXGE_HAL_SSCC_MASK_5_TCP_SP(val)		    vBIT(val, 0, 16)
#define	VXGE_HAL_SSCC_MASK_5_TCP_DP(val)		    vBIT(val, 16, 16)
#define	VXGE_HAL_SSCC_MASK_5_VLANID(val)		    vBIT(val, 52, 12)
/* 0x03720 */	u64	gcmg1_ecc;
#define	VXGE_HAL_GCMG1_ECC_ENABLE_SSCC_N		    mBIT(7)
#define	VXGE_HAL_GCMG1_ECC_ENABLE_UQM_N	mBIT(15)
#define	VXGE_HAL_GCMG1_ECC_ENABLE_QCC_N	mBIT(23)
	u8	unused03a00[0x03a00 - 0x03728];

/* 0x03a00 */	u64	pcmg1_int_status;
#define	VXGE_HAL_PCMG1_INT_STATUS_PSSCC_ERR_PSSCC_INT	    mBIT(0)
#define	VXGE_HAL_PCMG1_INT_STATUS_PQCC_ERR_PQCC_INT	    mBIT(1)
#define	VXGE_HAL_PCMG1_INT_STATUS_PQCC_CQM_ERR_PQCC_CQM_INT mBIT(2)
#define	VXGE_HAL_PCMG1_INT_STATUS_PQCC_SQM_ERR_PQCC_SQM_INT mBIT(3)
/* 0x03a08 */	u64	pcmg1_int_mask;
/* 0x03a10 */	u64	psscc_err_reg;
#define	VXGE_HAL_PSSCC_ERR_REG_SSCC_CP2STE_OFLOW_ERR	    mBIT(0)
#define	VXGE_HAL_PSSCC_ERR_REG_SSCC_CP2TTE_OFLOW_ERR	    mBIT(1)
/* 0x03a18 */	u64	psscc_err_mask;
/* 0x03a20 */	u64	psscc_err_alarm;
/* 0x03a28 */	u64	pqcc_err_reg;
#define	VXGE_HAL_PQCC_ERR_REG_QCC_SQM_MAX_WQE_GRP_INFO_ERR  mBIT(0)
#define	VXGE_HAL_PQCC_ERR_REG_QCC_SQM_WQE_FREE_LIST_EMPTY_INFO_ERR  mBIT(1)
#define	VXGE_HAL_PQCC_ERR_REG_QCC_SQM_FLM_WQE_ID_FIFO_ERR   mBIT(2)
#define	VXGE_HAL_PQCC_ERR_REG_QCC_SQM_CACHE_FULL_INFO_ERR   mBIT(3)
#define	VXGE_HAL_PQCC_ERR_REG_QCC_QCC_PDA_ARB_SM_ERR	    mBIT(32)
#define	VXGE_HAL_PQCC_ERR_REG_QCC_QCC_CP_ARB_SM_ERR	    mBIT(33)
#define	VXGE_HAL_PQCC_ERR_REG_QCC_QCC_CXP2QCC_FIFO_ERR	    mBIT(63)
/* 0x03a30 */	u64	pqcc_err_mask;
/* 0x03a38 */	u64	pqcc_err_alarm;
/* 0x03a40 */	u64	pqcc_cqm_err_reg;
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_CACHE_PA_SG_ERR(val) vBIT(val, 0, 4)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_DMACQERSP_SG_ERR  mBIT(4)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_CACHE_PA_DB_ERR(val) vBIT(val, 8, 4)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_DMACQERSP_DB_ER   mBIT(12)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_CCM_RMW_FIFO_ERR  mBIT(16)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_CAE_RLM_FIFO_ERR  mBIT(17)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_CCM_CAM_FIFO_POP_ERR mBIT(18)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_CCM_CAM_EIP_FIFO_POP_ERR mBIT(19)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_CCM2CMA_FIFO_PUSH_ERR	mBIT(20)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_HPRPEREQ_FIFO_ERR mBIT(21)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_LPRPEREQ_FIFO_ERR mBIT(22)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_LPRPERSP_FIFO_ERR mBIT(23)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_CMP_USDC_DBELL_FIFO_ERR   mBIT(24)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_CMP_CXP_MSG_IN_FIFO_ERR   mBIT(25)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_CMP_CXP_MSG_OUT_FIFO_ERR  mBIT(26)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_CAE_ELM_FIFO_ERR  mBIT(27)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_CGM_CCM_REQ_FIFO_ERR mBIT(28)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_EXCESSIVE_RD_RESP_ERR mBIT(29)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_CDR_SERR	    mBIT(32)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_WGM_FLM_SM_ERR    mBIT(33)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_WGM_CRP_SM_ERR    mBIT(34)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_WGM_ARB_SM_ERR    mBIT(35)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_CMP_RCL_SM_ERR    mBIT(36)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_CMP_CIN_SM_ERR    mBIT(37)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_CSE_SM_ERR	    mBIT(38)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_CCM_SM_ERR	    mBIT(39)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_CAE_RLM_SM_ERR    mBIT(40)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_CAE_RLM_ADD_SM_ERR mBIT(41)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_CAE_ELM_SM_ERR    mBIT(42)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_CAE_ELM_ADD_SM_ERR mBIT(43)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_CACHE_FULL_INFO_ERR mBIT(58)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_MAX_CQE_GRP_INFO_ERR mBIT(59)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_CDR_SM_INFO_ERR   mBIT(60)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_BAD_CIN_INFO_ERR  mBIT(61)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_NO_CQE_GRP_INFO_ERR mBIT(62)
#define	VXGE_HAL_PQCC_CQM_ERR_REG_QCC_CQM_BAD_VPIN_INFO_ERR mBIT(63)
/* 0x03a48 */	u64	pqcc_cqm_err_mask;
/* 0x03a50 */	u64	pqcc_cqm_err_alarm;
/* 0x03a58 */	u64	pqcc_sqm_err_reg;
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_CACHE_PA_SG_ERR(val) vBIT(val, 0, 4)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_LPRPEDAT_SG_ERR(val) vBIT(val, 4, 4)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_DMAWQERSP_SG_ERR  mBIT(8)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_RPEREQDAT_SG_ERR  mBIT(9)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_BAD_VPIN_INFO_ERR mBIT(10)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_WDR_SM_INFO_ERR   mBIT(11)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_BAD_SIN_INFO_ERR  mBIT(12)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_EXCESSIVE_RD_RESP_ERR	mBIT(13)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_DMAWQERSP_DB_ERR  mBIT(14)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_RPEREQDAT_DB_ERR  mBIT(15)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_CACHE_PA_DB_ERR(val) vBIT(val, 16, 4)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_LPRPEDAT_DB_ERR(val) vBIT(val, 20, 4)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_WAE_RLM_FIFO_ERR  mBIT(24)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_CCM_CAM_FIFO_POP_ERR mBIT(25)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_CCM_CAM_EIP_FIFO_POP_ERR mBIT(26)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_CCM2CMA_LP_FIFO_PUSH_ERR mBIT(27)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_CCM2CMA_HP_FIFO_PUSH_ERR mBIT(28)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_WSE2CMA_FIFO_PUSH_ERR mBIT(29)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_RRP2CMA_LP_FIFO_PUSH_ERR mBIT(30)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_RRP2CMA_HP_FIFO_PUSH_ERR mBIT(31)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_HPRPEREQ_FIFO_ERR mBIT(32)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_IPWOGRREQSB_FIFO_ERR mBIT(33)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_IPWOGRRESP_FIFO_POP_ERR mBIT(34)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_LPRPEDAT_FIFO_ERR	mBIT(35)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_LPRPEREQ_FIFO_ERR	mBIT(36)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_LPRPERESP_FIFO_ERR	mBIT(37)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_LPRPERESPSB_FIFO_ERR	mBIT(38)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_LPWRREQSB_FIFO_ERR	mBIT(39)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_LPWRRESP_FIFO_POP_ERR	mBIT(40)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_SWRRESP_FIFO_ERR	mBIT(41)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_WGM_RPE_REQ_FIFO_ERR	mBIT(42)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_WGM_RPE_LASTOD_FIFO_ERR mBIT(43)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_CMP_USDC_DBELL_FIFO_ERR mBIT(44)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_CMP_CXP_MSG_IN_FIFO_ERR mBIT(45)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_CMP_CXP_MSG_OUT_FIFO_ERR mBIT(46)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_CCM_RMW_FIFO_ERR	mBIT(47)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_WAE_ELM_FIFO_ERR	mBIT(48)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_WAE_DLM_FIFO_ERR	mBIT(49)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_RRP_RESPDATA_ARB_SM_ERR mBIT(50)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_WDR_SERR		mBIT(51)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_CMA_RLP_SM_ERR	mBIT(52)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_WGM_FLM_SM_ERR	mBIT(53)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_CMP_RCL_SM_ERR	mBIT(54)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_CMP_CIN_SM_ERR	mBIT(55)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_WSE_SM_ERR		mBIT(56)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_CCM_SM_ERR		mBIT(57)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_WAE_RLM_SM_ERR	mBIT(58)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_WAE_RLM_ADD_SM_ERR	mBIT(59)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_WAE_ELM_SM_ERR	mBIT(60)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_WAE_ELM_ADD_SM_ERR	mBIT(61)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_WAE_DLM_SM_ERR	mBIT(62)
#define	VXGE_HAL_PQCC_SQM_ERR_REG_QCC_SQM_WAE_DLM_ADD_SM_ERR	mBIT(63)
/* 0x03a60 */	u64	pqcc_sqm_err_mask;
/* 0x03a68 */	u64	pqcc_sqm_err_alarm;
/* 0x03a70 */	u64	qcc_srq_cqrq;
#define	VXGE_HAL_QCC_SRQ_CQRQ_POLL_TIMER(val)		    vBIT(val, 0, 32)
#define	VXGE_HAL_QCC_SRQ_CQRQ_MAX_EOL_POLLS(val)	    vBIT(val, 32, 8)
#define	VXGE_HAL_QCC_SRQ_CQRQ_CONSERVATIVE_SM_CRD_RTN	    mBIT(47)
/* 0x03a78 */	u64	qcc_err_policy;
#define	VXGE_HAL_QCC_ERR_POLICY_CQM_CQE(val)		    vBIT(val, 4, 4)
#define	VXGE_HAL_QCC_ERR_POLICY_SQM_WQE(val)		    vBIT(val, 12, 4)
#define	VXGE_HAL_QCC_ERR_POLICY_SQM_SRQIR(val)		    vBIT(val, 22, 2)
/* 0x03a80 */	u64	qcc_bp_ctrl;
#define	VXGE_HAL_QCC_BP_CTRL_RD_XON			    mBIT(7)
/* 0x03a88 */	u64	pcmg1_ecc;
#define	VXGE_HAL_PCMG1_ECC_ENABLE_QCC_N			    mBIT(23)
/* 0x03a90 */	u64	qcc_cqm_cqrq_id;
#define	VXGE_HAL_QCC_CQM_CQRQ_ID_CQM_BAD_VPIN_CQRQ_ID(val)  vBIT(val, 0, 16)
#define	VXGE_HAL_QCC_CQM_CQRQ_ID_CQM_BAD_CIN_CQRQ_ID(val)   vBIT(val, 16, 16)
#define	VXGE_HAL_QCC_CQM_CQRQ_ID_CQM_MAX_CQE_GRP_CQRQ_ID(val) vBIT(val, 32, 16)
#define	VXGE_HAL_QCC_CQM_CQRQ_ID_CQM_CQM_CDR_CQRQ_ID(val)   vBIT(val, 48, 16)
/* 0x03a98 */	u64	qcc_sqm_srq_id;
#define	VXGE_HAL_QCC_SQM_SRQ_ID_SQM_BAD_VPIN_SRQ_ID(val)    vBIT(val, 0, 16)
#define	VXGE_HAL_QCC_SQM_SRQ_ID_SQM_BAD_SIN_SRQ_ID(val)	    vBIT(val, 16, 16)
#define	VXGE_HAL_QCC_SQM_SRQ_ID_SQM_MAX_WQE_GRP_SRQ_ID(val) vBIT(val, 32, 16)
#define	VXGE_HAL_QCC_SQM_SRQ_ID_SQM_SQM_WDR_SRQ_ID(val)	    vBIT(val, 48, 16)
/* 0x03aa0 */	u64	qcc_cqm_flm_id;
#define	VXGE_HAL_QCC_CQM_FLM_ID_CQM_CQM_CCM_STATE_SERR(val) vBIT(val, 1, 7)
#define	VXGE_HAL_QCC_CQM_FLM_ID_CQM_CQM_FLM_HEAD_CQEGRP_ID(val) vBIT(val, 8, 24)
#define	VXGE_HAL_QCC_CQM_FLM_ID_CQM_CQM_FLM_TAIL_CQEGRP_ID(val)\
							    vBIT(val, 40, 24)
/* 0x03aa8 */	u64	qcc_sqm_flm_id;
#define	VXGE_HAL_QCC_SQM_FLM_ID_SQM_SQM_NO_WQE_OD_GRP_AVAIL mBIT(0)
#define	VXGE_HAL_QCC_SQM_FLM_ID_SQM_SQM_CCM_STATE_SERR(val) vBIT(val, 1, 7)
#define	VXGE_HAL_QCC_SQM_FLM_ID_SQM_SQM_FLM_HEAD_WQEGRP_ID(val) vBIT(val, 8, 24)
#define	VXGE_HAL_QCC_SQM_FLM_ID_SQM_SQM_FLM_TAIL_WQEGRP_ID(val)\
							    vBIT(val, 40, 24)
	u8	unused04000[0x04000 - 0x03ab0];

/* 0x04000 */	u64	one_int_status;
#define	VXGE_HAL_ONE_INT_STATUS_RXPE_ERR_RXPE_INT	    mBIT(7)
#define	VXGE_HAL_ONE_INT_STATUS_TXPE_BCC_MEM_SG_ECC_ERR_TXPE_BCC_MEM_SG_ECC_INT\
							    mBIT(13)
#define	VXGE_HAL_ONE_INT_STATUS_TXPE_BCC_MEM_DB_ECC_ERR_TXPE_BCC_MEM_DB_ECC_INT\
							    mBIT(14)
#define	VXGE_HAL_ONE_INT_STATUS_TXPE_ERR_TXPE_INT	    mBIT(15)
#define	VXGE_HAL_ONE_INT_STATUS_DLM_ERR_DLM_INT		    mBIT(23)
#define	VXGE_HAL_ONE_INT_STATUS_PE_ERR_PE_INT		    mBIT(31)
#define	VXGE_HAL_ONE_INT_STATUS_RPE_ERR_RPE_INT		    mBIT(39)
#define	VXGE_HAL_ONE_INT_STATUS_RPE_FSM_ERR_RPE_FSM_INT	    mBIT(47)
#define	VXGE_HAL_ONE_INT_STATUS_OES_ERR_OES_INT		    mBIT(55)
/* 0x04008 */	u64	one_int_mask;
/* 0x04010 */	u64	rpe_err_reg;
#define	VXGE_HAL_RPE_ERR_REG_RPE_RCM_PA_DB_ERR(val)	    vBIT(val, 0, 4)
#define	VXGE_HAL_RPE_ERR_REG_RPE_RCM_PB_DB_ERR(val)	    vBIT(val, 4, 4)
#define	VXGE_HAL_RPE_ERR_REG_RPE_PDM_FRAME_DB_ERR	    mBIT(8)
#define	VXGE_HAL_RPE_ERR_REG_RPE_PDM_RCMD_DB_ERR	    mBIT(9)
#define	VXGE_HAL_RPE_ERR_REG_RPE_RCQ_DB_ERR		    mBIT(10)
#define	VXGE_HAL_RPE_ERR_REG_RPE_RCO_PBLE_DB_ERR	    mBIT(11)
#define	VXGE_HAL_RPE_ERR_REG_RPE_RCM_PA_SG_ERR(val)	    vBIT(val, 16, 4)
#define	VXGE_HAL_RPE_ERR_REG_RPE_RCM_PB_SG_ERR(val)	    vBIT(val, 20, 4)
#define	VXGE_HAL_RPE_ERR_REG_RPE_PDM_FRAME_SG_ERR	    mBIT(24)
#define	VXGE_HAL_RPE_ERR_REG_RPE_PDM_RCMD_SG_ERR	    mBIT(25)
#define	VXGE_HAL_RPE_ERR_REG_RPE_RCQ_SG_ERR		    mBIT(26)
#define	VXGE_HAL_RPE_ERR_REG_RPE_RCO_PBLE_SG_ERR	    mBIT(27)
#define	VXGE_HAL_RPE_ERR_REG_RPE_CMI_CTXTRDRQ_FIFO_ERR	    mBIT(32)
#define	VXGE_HAL_RPE_ERR_REG_RPE_CMI_CTXTWRRQ_FIFO_ERR	    mBIT(33)
#define	VXGE_HAL_RPE_ERR_REG_RPE_CMI_CQRQLDRQ_FIFO_ERR	    mBIT(34)
#define	VXGE_HAL_RPE_ERR_REG_RPE_CMI_SRQLDRQ_FIFO_ERR	    mBIT(35)
#define	VXGE_HAL_RPE_ERR_REG_RPE_CMI_WQERDRQ_FIFO_ERR	    mBIT(36)
#define	VXGE_HAL_RPE_ERR_REG_RPE_CMI_WQEWRRQ_FIFO_ERR	    mBIT(37)
#define	VXGE_HAL_RPE_ERR_REG_RPE_CMI_CQEAVAILRQ_FIFO_ERR    mBIT(38)
#define	VXGE_HAL_RPE_ERR_REG_RPE_CMI_WQECOMPL_FIFO_ERR	    mBIT(39)
#define	VXGE_HAL_RPE_ERR_REG_RPE_CMI_CQEADDRRQ_FIFO_ERR	    mBIT(40)
#define	VXGE_HAL_RPE_ERR_REG_RPE_RCC_CTXTLDNT_FIFO_ERR	    mBIT(41)
#define	VXGE_HAL_RPE_ERR_REG_RPE_RCC_RCCRESP_FIFO_ERR	    mBIT(42)
#define	VXGE_HAL_RPE_ERR_REG_RPE_QEM_OESPREINIT_FIFO_ERR    mBIT(43)
#define	VXGE_HAL_RPE_ERR_REG_RPE_QEM_EVENT_FIFO_ERR	    mBIT(44)
#define	VXGE_HAL_RPE_ERR_REG_RPE_QEM_WQELDNT_FIFO_ERR	    mBIT(45)
#define	VXGE_HAL_RPE_ERR_REG_RPE_QEM_QEMRESP_FIFO_ERR	    mBIT(46)
#define	VXGE_HAL_RPE_ERR_REG_RPE_QEM_PDM_CMD_FIFO_ERR	    mBIT(47)
#define	VXGE_HAL_RPE_ERR_REG_RPE_PDM_CMDRESP_FIFO_ERR	    mBIT(48)
#define	VXGE_HAL_RPE_ERR_REG_RPE_PDM_FRAME_FIFO_ERR	    mBIT(49)
#define	VXGE_HAL_RPE_ERR_REG_RPE_PDM_EPE_SPQ_FIFO_ERR	    mBIT(50)
#define	VXGE_HAL_RPE_ERR_REG_RPE_PDM_EPE_STCRESP_FIFO_ERR   mBIT(51)
#define	VXGE_HAL_RPE_ERR_REG_RPE_PDM_RIM_RIMIPB_FIFO_ERR    mBIT(52)
#define	VXGE_HAL_RPE_ERR_REG_RPE_RCI_MCQLEN_FIFO_ERR	    mBIT(53)
#define	VXGE_HAL_RPE_ERR_REG_RPE_RCI_PCQLEN_FIFO_ERR	    mBIT(54)
#define	VXGE_HAL_RPE_ERR_REG_RPE_RCI_RDLIM_FIFO_ERR	    mBIT(55)
#define	VXGE_HAL_RPE_ERR_REG_RPE_MSG_RCMD_FIFO_ERR	    mBIT(56)
#define	VXGE_HAL_RPE_ERR_REG_RPE_DLM_RCMD_FIFO_ERR	    mBIT(57)
#define	VXGE_HAL_RPE_ERR_REG_RPE_PDM_RCMD_FIFO_ERR	    mBIT(58)
#define	VXGE_HAL_RPE_ERR_REG_RPE_RCQ_FIFO_ERR		    mBIT(59)
#define	VXGE_HAL_RPE_ERR_REG_RPE_RCO_CQE_FIFO_ERR	    mBIT(60)
#define	VXGE_HAL_RPE_ERR_REG_RPE_RCO_PBLE_FIFO_ERR	    mBIT(61)
/* 0x04018 */	u64	rpe_err_mask;
/* 0x04020 */	u64	rpe_err_alarm;
/* 0x04028 */	u64	pe_err_reg;
#define	VXGE_HAL_PE_ERR_REG_PE_PE_CDP_CTXT_PA_SG_ERR	    mBIT(0)
#define	VXGE_HAL_PE_ERR_REG_PE_PE_CDP_CTXT_PB_SG_ERR	    mBIT(1)
#define	VXGE_HAL_PE_ERR_REG_PE_PE_TIMER_SG_ERR		    mBIT(2)
#define	VXGE_HAL_PE_ERR_REG_PE_CDP_CTXTRRQ_RDFIFO_STATE_SM_ERR mBIT(8)
#define	VXGE_HAL_PE_ERR_REG_PE_CDP_CTXTRRQ_STATE_SM_ERR	    mBIT(9)
#define	VXGE_HAL_PE_ERR_REG_PE_CDP_CTXTWRQ_ADDR_STATE_SM_ERR mBIT(10)
#define	VXGE_HAL_PE_ERR_REG_PE_CDP_CTXTWRQ_DATA_STATE_SM_ERR mBIT(11)
#define	VXGE_HAL_PE_ERR_REG_PE_CDP_DLM_CTXT_STATE_SM_ERR    mBIT(12)
#define	VXGE_HAL_PE_ERR_REG_PE_CDP_RDMEM_ADDR_STATE_SM_ERR  mBIT(13)
#define	VXGE_HAL_PE_ERR_REG_PE_CDP_RDMEM_DATA_STATE_SM_ERR  mBIT(14)
#define	VXGE_HAL_PE_ERR_REG_PE_CDP_RDRESP_STATE_SM_ERR	    mBIT(15)
#define	VXGE_HAL_PE_ERR_REG_PE_CDP_RXPE_RDCTXT_DATA_STATE_SM_ERR mBIT(16)
#define	VXGE_HAL_PE_ERR_REG_PE_CDP_RXPEIF_STATE_SM_ERR	    mBIT(17)
#define	VXGE_HAL_PE_ERR_REG_PE_CDP_TCM_CTXT_STATE_SM_ERR    mBIT(18)
#define	VXGE_HAL_PE_ERR_REG_PE_SCC_CTXT_CNTRL_SM_ERR	    mBIT(19)
#define	VXGE_HAL_PE_ERR_REG_PE_SCC_RECALL_SM_ERR	    mBIT(20)
#define	VXGE_HAL_PE_ERR_REG_PE_SCC_NCE_FETCH_STATE_SM_ERR   mBIT(21)
#define	VXGE_HAL_PE_ERR_REG_PE_NCC_NCE_CNTRL_SM_ERR	    mBIT(22)
#define	VXGE_HAL_PE_ERR_REG_PE_NCC_NDP_MEMCNTRL_STATE_SM_ERR	mBIT(23)
#define	VXGE_HAL_PE_ERR_REG_PE_NCC_NDP_NRRQ_RDFIFO_STATE_SM_ERR	mBIT(24)
#define	VXGE_HAL_PE_ERR_REG_PE_NCC_NDP_NRRQ_STATE_SM_ERR    mBIT(25)
#define	VXGE_HAL_PE_ERR_REG_PE_NCC_NDP_NWRQ_RDFIFO_STATE_SM_ERR	mBIT(26)
#define	VXGE_HAL_PE_ERR_REG_PE_NCC_NDP_RDMEM_DATA_STATE_SM_ERR	mBIT(27)
#define	VXGE_HAL_PE_ERR_REG_PE_CMGIF_HDREQ_ARB_STATE_SM_ERR	mBIT(28)
#define	VXGE_HAL_PE_ERR_REG_PE_CMGIF_HNREQ_ARB_STATE_SM_ERR	mBIT(29)
#define	VXGE_HAL_PE_ERR_REG_PE_CMGIF_LDREQ_ARB_STATE_SM_ERR	mBIT(30)
#define	VXGE_HAL_PE_ERR_REG_PE_CMGIF_LNREQ_ARB_STATE_SM_ERR	mBIT(31)
#define	VXGE_HAL_PE_ERR_REG_PE_CDP_CTXTRRQ_FIFO_ERR	    mBIT(32)
#define	VXGE_HAL_PE_ERR_REG_PE_CDP_CTXT_FIFO_ERR	    mBIT(33)
#define	VXGE_HAL_PE_ERR_REG_PE_CDP_RXPE_CTXT_WR_PHASE_ERR   mBIT(34)
#define	VXGE_HAL_PE_ERR_REG_PE_CDP_RXPE_CTXT_RD_PHASE_ERR   mBIT(35)
#define	VXGE_HAL_PE_ERR_REG_PE_CDP_CTXTRRQ_RD_RESP_PHASE_ERR mBIT(36)
#define	VXGE_HAL_PE_ERR_REG_PE_NDP_NRRQ_FIFO_ERR	    mBIT(37)
#define	VXGE_HAL_PE_ERR_REG_PE_NDP_NWRQ_FIFO_ERR	    mBIT(38)
#define	VXGE_HAL_PE_ERR_REG_PE_NCC_NDP_WRMEM_PHASE_ERR	    mBIT(39)
#define	VXGE_HAL_PE_ERR_REG_PE_NCC_PE_RESP_CMD_PHASE_ERR    mBIT(40)
#define	VXGE_HAL_PE_ERR_REG_PE_PE_TIMER_SM_ERR		    mBIT(48)
#define	VXGE_HAL_PE_ERR_REG_PE_PET_MEM_ARB_ERR		    mBIT(49)
#define	VXGE_HAL_PE_ERR_REG_PE_PET_UPDATE_FSM_ERR	    mBIT(50)
#define	VXGE_HAL_PE_ERR_REG_PE_PE_CDP_CTXT_PA_DB_ERR	    mBIT(61)
#define	VXGE_HAL_PE_ERR_REG_PE_PE_CDP_CTXT_PB_DB_ERR	    mBIT(62)
#define	VXGE_HAL_PE_ERR_REG_PE_PE_TIMER_DB_ERR		    mBIT(63)
/* 0x04030 */	u64	pe_err_mask;
/* 0x04038 */	u64	pe_err_alarm;
/* 0x04040 */	u64	rxpe_err_reg;
#define	VXGE_HAL_RXPE_ERR_REG_RXPE_XT0_FRM_SG_ERR	    mBIT(0)
#define	VXGE_HAL_RXPE_ERR_REG_RXPE_XT1_FRM_SG_ERR	    mBIT(1)
#define	VXGE_HAL_RXPE_ERR_REG_RXPE_FPDU_MEM_SG_ERR	    mBIT(2)
#define	VXGE_HAL_RXPE_ERR_REG_RXPE_MSG2RXPE_SG_ERR(val)	    vBIT(val, 3, 2)
#define	VXGE_HAL_RXPE_ERR_REG_RXPE_XT0_IRAM_SG_ERR(val)	    vBIT(val, 5, 2)
#define	VXGE_HAL_RXPE_ERR_REG_RXPE_XT1_IRAM_SG_ERR(val)	    vBIT(val, 7, 2)
#define	VXGE_HAL_RXPE_ERR_REG_RXPE_XT_DRAM_PA_SG_ERR(val)   vBIT(val, 9, 2)
#define	VXGE_HAL_RXPE_ERR_REG_RXPE_XT_DRAM_PB_SG_ERR(val)   vBIT(val, 11, 2)
#define	VXGE_HAL_RXPE_ERR_REG_RXPE_XT0_TRCE_SG_ERR	    mBIT(13)
#define	VXGE_HAL_RXPE_ERR_REG_RXPE_XT1_TRCE_SG_ERR	    mBIT(14)
#define	VXGE_HAL_RXPE_ERR_REG_RXPE_XT0_FRM_DB_ERR	    mBIT(32)
#define	VXGE_HAL_RXPE_ERR_REG_RXPE_XT1_FRM_DB_ERR	    mBIT(33)
#define	VXGE_HAL_RXPE_ERR_REG_RXPE_FPDU_MEM_DB_ERR	    mBIT(34)
#define	VXGE_HAL_RXPE_ERR_REG_RXPE_MSG2RXPE_DB_ERR(val)	    vBIT(val, 35, 2)
#define	VXGE_HAL_RXPE_ERR_REG_RXPE_XT0_IRAM_DB_ERR(val)	    vBIT(val, 37, 2)
#define	VXGE_HAL_RXPE_ERR_REG_RXPE_XT1_IRAM_DB_ERR(val)	    vBIT(val, 39, 2)
#define	VXGE_HAL_RXPE_ERR_REG_RXPE_XT_DRAM_PA_DB_ERR(val)   vBIT(val, 41, 2)
#define	VXGE_HAL_RXPE_ERR_REG_RXPE_XT_DRAM_PB_DB_ERR(val)   vBIT(val, 43, 2)
#define	VXGE_HAL_RXPE_ERR_REG_RXPE_XT0_TRCE_DB_ERR	    mBIT(45)
#define	VXGE_HAL_RXPE_ERR_REG_RXPE_XT1_TRCE_DB_ERR	    mBIT(46)
#define	VXGE_HAL_RXPE_ERR_REG_RXPE_XT0_XLMI_SERR	    mBIT(54)
#define	VXGE_HAL_RXPE_ERR_REG_RXPE_XT1_XLMI_SERR	    mBIT(55)
#define	VXGE_HAL_RXPE_ERR_REG_RXPE_DRAM_WR_ERR		    mBIT(58)
#define	VXGE_HAL_RXPE_ERR_REG_RXPE_IMSGIN_WR_FSM_ERR	    mBIT(59)
#define	VXGE_HAL_RXPE_ERR_REG_RXPE_IMSGIN_EVCTRL_FSM_ERR    mBIT(60)
#define	VXGE_HAL_RXPE_ERR_REG_RXPE_MSG2RXPE_FIFO_ERR	    mBIT(61)
#define	VXGE_HAL_RXPE_ERR_REG_RXPE_IMSGOUT_COLLISION_ERR    mBIT(62)
#define	VXGE_HAL_RXPE_ERR_REG_RXPE_SM_ERR		    mBIT(63)
/* 0x04048 */	u64	rxpe_err_mask;
/* 0x04050 */	u64	rxpe_err_alarm;
/* 0x04058 */	u64	dlm_err_reg;
#define	VXGE_HAL_DLM_ERR_REG_DLM_PE_DLM_CTXT_PA_SG_ERR	    mBIT(0)
#define	VXGE_HAL_DLM_ERR_REG_DLM_PE_DLM_CTXT_PB_SG_ERR	    mBIT(1)
#define	VXGE_HAL_DLM_ERR_REG_DLM_PE_DLM_ACK_PA_SG_ERR	    mBIT(2)
#define	VXGE_HAL_DLM_ERR_REG_DLM_PE_DLM_ACK_PB_SG_ERR	    mBIT(3)
#define	VXGE_HAL_DLM_ERR_REG_DLM_PE_DLM_RIRR_PA_SG_ERR	    mBIT(4)
#define	VXGE_HAL_DLM_ERR_REG_DLM_PE_DLM_RIRR_PB_SG_ERR	    mBIT(5)
#define	VXGE_HAL_DLM_ERR_REG_DLM_PE_DLM_AWRQ_MEM_SG_ERR	    mBIT(6)
#define	VXGE_HAL_DLM_ERR_REG_DLM_PE_DLM_LWRQ_MEM_SG_ERR	    mBIT(7)
#define	VXGE_HAL_DLM_ERR_REG_DLM_PE_DLM_CTXT_PA_DB_ERR	    mBIT(8)
#define	VXGE_HAL_DLM_ERR_REG_DLM_PE_DLM_CTXT_PB_DB_ERR	    mBIT(9)
#define	VXGE_HAL_DLM_ERR_REG_DLM_PE_DLM_ACK_PA_DB_ERR	    mBIT(10)
#define	VXGE_HAL_DLM_ERR_REG_DLM_PE_DLM_ACK_PB_DB_ERR	    mBIT(11)
#define	VXGE_HAL_DLM_ERR_REG_DLM_PE_DLM_RIRR_PA_DB_ERR	    mBIT(12)
#define	VXGE_HAL_DLM_ERR_REG_DLM_PE_DLM_RIRR_PB_DB_ERR	    mBIT(13)
#define	VXGE_HAL_DLM_ERR_REG_DLM_PE_DLM_AWRQ_MEM_DB_ERR	    mBIT(14)
#define	VXGE_HAL_DLM_ERR_REG_DLM_PE_DLM_LWRQ_MEM_DB_ERR	    mBIT(15)
#define	VXGE_HAL_DLM_ERR_REG_DLM_ACC_PA_STATE_SM_ERR	    mBIT(16)
#define	VXGE_HAL_DLM_ERR_REG_DLM_ACC_PB_STATE_SM_ERR	    mBIT(17)
#define	VXGE_HAL_DLM_ERR_REG_DLM_ACK_RDMEM_DATA_STATE_SM_ERR mBIT(18)
#define	VXGE_HAL_DLM_ERR_REG_DLM_AFLM_RDFIFO_STATE_SM_ERR   mBIT(19)
#define	VXGE_HAL_DLM_ERR_REG_DLM_AFLM_STATE_SM_ERR	    mBIT(20)
#define	VXGE_HAL_DLM_ERR_REG_DLM_APTR_ALLOC_STATE_SM_ERR    mBIT(21)
#define	VXGE_HAL_DLM_ERR_REG_DLM_ARRQ_RDFIFO_STATE_SM_ERR   mBIT(22)
#define	VXGE_HAL_DLM_ERR_REG_DLM_ARRQ_STATE_SM_ERR	    mBIT(23)
#define	VXGE_HAL_DLM_ERR_REG_DLM_AWRQ_STATE_SM_ERR	    mBIT(24)
#define	VXGE_HAL_DLM_ERR_REG_DLM_EVENT_CTXT_STATE_SM_ERR    mBIT(25)
#define	VXGE_HAL_DLM_ERR_REG_DLM_LCC_PA_STATE_SM_ERR	    mBIT(26)
#define	VXGE_HAL_DLM_ERR_REG_DLM_LCC_PB_STATE_SM_ERR	    mBIT(27)
#define	VXGE_HAL_DLM_ERR_REG_DLM_LFLM_RDFIFO_STATE_SM_ERR   mBIT(28)
#define	VXGE_HAL_DLM_ERR_REG_DLM_LFLM_STATE_SM_ERR	    mBIT(29)
#define	VXGE_HAL_DLM_ERR_REG_DLM_LPTR_ALLOC_STATE_SM_ERR    mBIT(30)
#define	VXGE_HAL_DLM_ERR_REG_DLM_LRRQ_RDFIFO_STATE_SM_ERR   mBIT(31)
#define	VXGE_HAL_DLM_ERR_REG_DLM_LRRQ_STATE_SM_ERR	    mBIT(32)
#define	VXGE_HAL_DLM_ERR_REG_DLM_LWRQ_STATE_SM_ERR	    mBIT(33)
#define	VXGE_HAL_DLM_ERR_REG_DLM_PCIWR_STATE_SM_ERR	    mBIT(34)
#define	VXGE_HAL_DLM_ERR_REG_DLM_PFETCH_STATE_SM_ERR	    mBIT(35)
#define	VXGE_HAL_DLM_ERR_REG_DLM_RCC_PA_STATE_SM_ERR	    mBIT(36)
#define	VXGE_HAL_DLM_ERR_REG_DLM_RCC_PB_STATE_SM_ERR	    mBIT(37)
#define	VXGE_HAL_DLM_ERR_REG_DLM_RFLM_RDFIFO_STATE_SM_ERR   mBIT(38)
#define	VXGE_HAL_DLM_ERR_REG_DLM_RFLM_STATE_SM_ERR	    mBIT(39)
#define	VXGE_HAL_DLM_ERR_REG_DLM_RIRR_RDMEM_DATA_STATE_SM_ERR mBIT(40)
#define	VXGE_HAL_DLM_ERR_REG_DLM_RPTR_ALLOC_STATE_SM_ERR    mBIT(41)
#define	VXGE_HAL_DLM_ERR_REG_DLM_RRRQ_RDFIFO_STATE_SM_ERR   mBIT(42)
#define	VXGE_HAL_DLM_ERR_REG_DLM_RRRQ_STATE_SM_ERR	    mBIT(43)
#define	VXGE_HAL_DLM_ERR_REG_DLM_RWRQ_STATE_SM_ERR	    mBIT(44)
#define	VXGE_HAL_DLM_ERR_REG_DLM_RXACK_STATE_SM_ERR	    mBIT(45)
#define	VXGE_HAL_DLM_ERR_REG_DLM_RXLIRR_STATE_SM_ERR	    mBIT(46)
#define	VXGE_HAL_DLM_ERR_REG_DLM_RXRIRR_STATE_SM_ERR	    mBIT(47)
#define	VXGE_HAL_DLM_ERR_REG_DLM_TXACK_RETX_STATE_SM_ERR    mBIT(48)
#define	VXGE_HAL_DLM_ERR_REG_DLM_TXACK_STATE_SM_ERR	    mBIT(49)
#define	VXGE_HAL_DLM_ERR_REG_DLM_TXLIRR_STATE_SM_ERR	    mBIT(50)
#define	VXGE_HAL_DLM_ERR_REG_DLM_TXRIRR_RETX_STATE_SM_ERR   mBIT(51)
#define	VXGE_HAL_DLM_ERR_REG_DLM_TXRIRR_STATE_SM_ERR	    mBIT(52)
#define	VXGE_HAL_DLM_ERR_REG_DLM_PREFETCH_ERR		    mBIT(53)
#define	VXGE_HAL_DLM_ERR_REG_DLM_AFLM_FIFO_ERR		    mBIT(55)
#define	VXGE_HAL_DLM_ERR_REG_DLM_RFLM_FIFO_ERR		    mBIT(56)
#define	VXGE_HAL_DLM_ERR_REG_DLM_LFLM_FIFO_ERR		    mBIT(57)
#define	VXGE_HAL_DLM_ERR_REG_DLM_ARRQ_FIFO_ERR		    mBIT(58)
#define	VXGE_HAL_DLM_ERR_REG_DLM_RRRQ_FIFO_ERR		    mBIT(59)
#define	VXGE_HAL_DLM_ERR_REG_DLM_LRRQ_FIFO_ERR		    mBIT(60)
#define	VXGE_HAL_DLM_ERR_REG_DLM_ACK_PTR_FIFO_ERR	    mBIT(61)
#define	VXGE_HAL_DLM_ERR_REG_DLM_RIRR_PTR_FIFO_ERR	    mBIT(62)
#define	VXGE_HAL_DLM_ERR_REG_DLM_LIRR_PTR_FIFO_ERR	    mBIT(63)
/* 0x04060 */	u64	dlm_err_mask;
/* 0x04068 */	u64	dlm_err_alarm;
/* 0x04070 */	u64	oes_err_reg;
#define	VXGE_HAL_OES_ERR_REG_OES_INPUT_ARB_SM_ERR	    mBIT(0)
#define	VXGE_HAL_OES_ERR_REG_OES_PEND_ARB_SM_ERR	    mBIT(1)
#define	VXGE_HAL_OES_ERR_REG_OES_RXSEG_FIFO_ERR		    mBIT(2)
#define	VXGE_HAL_OES_ERR_REG_OES_RXEVT_FIFO_ERR		    mBIT(3)
#define	VXGE_HAL_OES_ERR_REG_OES_TXTDB_FIFO_ERR		    mBIT(4)
#define	VXGE_HAL_OES_ERR_REG_OES_RXTX_FIFO_ERR		    mBIT(5)
#define	VXGE_HAL_OES_ERR_REG_OES_TXIMSG_FIFO_ERR	    mBIT(6)
#define	VXGE_HAL_OES_ERR_REG_OES_TXCONT_FIFO_ERR	    mBIT(7)
/* 0x04078 */	u64	oes_err_mask;
/* 0x04080 */	u64	oes_err_alarm;
/* 0x04088 */	u64	txpe_err_reg;
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_MSG2TXPE_SG_ERR(val)	    vBIT(val, 0, 2)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_TCM_DATA_PA_SG_ERR	    mBIT(2)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_TCM_DATA_PB_SG_ERR	    mBIT(3)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_XT_DRAM_SG_ERR(val)	    vBIT(val, 4, 2)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_XT_IRAM_SG_ERR(val)	    vBIT(val, 6, 2)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_XT_TRACE_SG_ERR	    mBIT(8)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_DOOR_IMM_SG_ERR	    mBIT(9)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_NCE_PA_SG_ERR	    mBIT(10)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_NCE_PB_SG_ERR	    mBIT(11)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_TCM_INFO_PA_SG_ERR	    mBIT(12)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_TCM_INFO_PB_SG_ERR	    mBIT(13)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_TCM_STG_SG_ERR	    mBIT(14)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_MSG2TXPE_DB_ERR(val)	    vBIT(val, 16, 2)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_TCM_DATA_PA_DB_ERR	    mBIT(18)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_TCM_DATA_PB_DB_ERR	    mBIT(19)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_XT_DRAM_DB_ERR(val)	    vBIT(val, 20, 2)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_XT_IRAM_DB_ERR(val)	    vBIT(val, 22, 2)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_XT_TRACE_DB_ERR	    mBIT(24)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_DOOR_IMM_DB_ERR	    mBIT(25)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_NCE_PA_DB_ERR	    mBIT(26)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_NCE_PB_DB_ERR	    mBIT(27)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_TCM_INFO_PA_DB_ERR	    mBIT(28)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_TCM_INFO_PB_DB_ERR	    mBIT(29)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_TCM_STG_DB_ERR	    mBIT(30)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_DOOR_SM_ERR		    mBIT(32)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_IMSGIN_SM_ERR	    mBIT(33)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_SEND_SM_ERR		    mBIT(34)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_SEND_TCE_CHOICE_SM_ERR   mBIT(35)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_SEND_DIV_SM_ERR	    mBIT(36)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_DMA_SM_ERR		    mBIT(37)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_DMA_RES_SM_ERR	    mBIT(38)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_DMA_NACK_SM_ERR	    mBIT(39)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_RDTCE_SM_ERR		    mBIT(40)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_CMGIF_RDRQ_SM_ERR	    mBIT(41)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_CMGIF_READDRES_SM_ERR    mBIT(42)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_TCM_CTXT_SM_ERR	    mBIT(43)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_PRI_TCE_UPDATE_SM_ERR    mBIT(44)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_DMA_GET_SM_ERR	    mBIT(45)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_DMA_DONE_SM_ERR	    mBIT(46)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_INIT_SM_ERR		    mBIT(47)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_FETCH_SM_ERR		    mBIT(48)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_HOG_SM_ERR		    mBIT(49)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_PMON_SM_ERR		    mBIT(50)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_XT_DRAM_SM_ERR	    mBIT(51)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_NCM_CTXT_SM_ERR	    mBIT(52)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_NCM_MEM_SM_ERR	    mBIT(53)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_DMA_RQ_SM_ERR	    mBIT(54)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_RDRES_PHASE_ERR	    mBIT(55)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_XT_XLMI_SERR		    mBIT(56)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_DOOR_WRP_ERR		    mBIT(57)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_DOOR_FIFO_ERR	    mBIT(58)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_DMA_DFIFO_ERR	    mBIT(59)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_DMA_HFIFO_ERR	    mBIT(60)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_SEND_DIVIDE_ERR	    mBIT(61)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_PDA_NACK_FIFO_ERR	    mBIT(62)
#define	VXGE_HAL_TXPE_ERR_REG_TXPE_MEM_CONFLICT_ERR	    mBIT(63)
/* 0x04090 */	u64	txpe_err_mask;
/* 0x04098 */	u64	txpe_err_alarm;
/* 0x040a0 */	u64	txpe_bcc_mem_sg_ecc_err_reg;
#define	VXGE_HAL_TXPE_BCC_MEM_SG_ECC_ERR_REG_TXPE_BASE_TXPE_SG_ERR(val)\
							    vBIT(val, 0, 32)
#define	VXGE_HAL_TXPE_BCC_MEM_SG_ECC_ERR_REG_TXPE_BASE_CDP_SG_ERR(val)\
							    vBIT(val, 32, 32)
/* 0x040a8 */	u64	txpe_bcc_mem_sg_ecc_err_mask;
/* 0x040b0 */	u64	txpe_bcc_mem_sg_ecc_err_alarm;
/* 0x040b8 */	u64	txpe_bcc_mem_db_ecc_err_reg;
#define	VXGE_HAL_TXPE_BCC_MEM_DB_ECC_ERR_REG_TXPE_BASE_TXPE_DB_ERR(val)\
							    vBIT(val, 0, 32)
#define	VXGE_HAL_TXPE_BCC_MEM_DB_ECC_ERR_REG_TXPE_BASE_CDP_DB_ERR(val)\
							    vBIT(val, 32, 32)
/* 0x040c0 */	u64	txpe_bcc_mem_db_ecc_err_mask;
/* 0x040c8 */	u64	txpe_bcc_mem_db_ecc_err_alarm;
/* 0x040d0 */	u64	rpe_fsm_err_reg;
#define	VXGE_HAL_RPE_FSM_ERR_REG_RPE_CMI_SHADOW_ERR	    mBIT(0)
#define	VXGE_HAL_RPE_FSM_ERR_REG_RPE_RCC_SHADOW_ERR	    mBIT(1)
#define	VXGE_HAL_RPE_FSM_ERR_REG_RPE_RCM_SHADOW_ERR	    mBIT(2)
#define	VXGE_HAL_RPE_FSM_ERR_REG_RPE_QEM_SHADOW_ERR	    mBIT(3)
#define	VXGE_HAL_RPE_FSM_ERR_REG_RPE_PDM_SHADOW_ERR	    mBIT(4)
#define	VXGE_HAL_RPE_FSM_ERR_REG_RPE_RCI_SHADOW_ERR	    mBIT(5)
#define	VXGE_HAL_RPE_FSM_ERR_REG_RPE_RCO_SHADOW_ERR	    mBIT(6)
#define	VXGE_HAL_RPE_FSM_ERR_REG_RPE_CMI_RWM_ERR	    mBIT(7)
#define	VXGE_HAL_RPE_FSM_ERR_REG_RPE_CMI_RRM_ERR	    mBIT(8)
#define	VXGE_HAL_RPE_FSM_ERR_REG_RPE_RCC_SCC_ERR	    mBIT(9)
#define	VXGE_HAL_RPE_FSM_ERR_REG_RPE_RCC_CMM_ERR	    mBIT(10)
#define	VXGE_HAL_RPE_FSM_ERR_REG_RPE_QEM_OIF_ERR	    mBIT(11)
#define	VXGE_HAL_RPE_FSM_ERR_REG_RPE_QEM_FPG_ERR	    mBIT(12)
#define	VXGE_HAL_RPE_FSM_ERR_REG_RPE_QEM_WCC_ERR	    mBIT(13)
#define	VXGE_HAL_RPE_FSM_ERR_REG_RPE_QEM_WMM_ERR	    mBIT(14)
#define	VXGE_HAL_RPE_FSM_ERR_REG_RPE_PDM_OIF_ERR	    mBIT(15)
#define	VXGE_HAL_RPE_FSM_ERR_REG_RPE_PDM_QRI_ERR	    mBIT(16)
#define	VXGE_HAL_RPE_FSM_ERR_REG_RPE_PDM_CTL_EFS_ERR	    mBIT(17)
#define	VXGE_HAL_RPE_FSM_ERR_REG_RPE_PDM_CTL_EFS_UNDEF_EVENT mBIT(18)
#define	VXGE_HAL_RPE_FSM_ERR_REG_RPE_PDM_EPE_BS_ERR	    mBIT(19)
#define	VXGE_HAL_RPE_FSM_ERR_REG_RPE_PDM_EPE_IWP_ERR	    mBIT(20)
#define	VXGE_HAL_RPE_FSM_ERR_REG_RPE_PDM_EPE_LRO_ERR	    mBIT(21)
#define	VXGE_HAL_RPE_FSM_ERR_REG_RPE_PDM_RIM_HDR_ERR	    mBIT(22)
#define	VXGE_HAL_RPE_FSM_ERR_REG_RPE_PDM_RIM_MUX_ERR	    mBIT(23)
#define	VXGE_HAL_RPE_FSM_ERR_REG_RPE_PDM_RIM_RLC_ERR	    mBIT(24)
#define	VXGE_HAL_RPE_FSM_ERR_REG_RPE_RCI_IPM_DLM_ERR	    mBIT(25)
#define	VXGE_HAL_RPE_FSM_ERR_REG_RPE_RCI_IPM_MSG_ERR	    mBIT(26)
#define	VXGE_HAL_RPE_FSM_ERR_REG_RPE_RCI_ARB_ERR	    mBIT(27)
#define	VXGE_HAL_RPE_FSM_ERR_REG_RPE_RCO_HBI_ERR	    mBIT(28)
#define	VXGE_HAL_RPE_FSM_ERR_REG_RPE_RCO_OPC_ERR	    mBIT(29)
#define	VXGE_HAL_RPE_FSM_ERR_REG_RPE_PDM_CTL_EFS_FW_ERR	    mBIT(32)
/* 0x040d8 */	u64	rpe_fsm_err_mask;
/* 0x040e0 */	u64	rpe_fsm_err_alarm;
	u8	unused04100[0x04100 - 0x040e8];

/* 0x04100 */	u64	one_cfg;
#define	VXGE_HAL_ONE_CFG_ONE_CFG_RDY			    mBIT(7)
/* 0x04108 */	u64	sgrp_alloc[17];
#define	VXGE_HAL_SGRP_ALLOC_SGRP_ALLOC(val)		    vBIT(val, 0, 64)
/* 0x04190 */	u64	sgrp_iwarp_lro_alloc;
#define	VXGE_HAL_SGRP_IWARP_LRO_ALLOC_ENABLE_IWARP	    mBIT(7)
#define	VXGE_HAL_SGRP_IWARP_LRO_ALLOC_LAST_IWARP_SGRP(val)  vBIT(val, 11, 5)
/* 0x04198 */	u64	rpe_cfg0;
#define	VXGE_HAL_RPE_CFG0_RCC_NBR_SLOTS(val)		    vBIT(val, 3, 5)
#define	VXGE_HAL_RPE_CFG0_RCC_NBR_FREE_SLOTS(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_RPE_CFG0_RCC_MODE			    mBIT(23)
#define	VXGE_HAL_RPE_CFG0_LL_SEND_MAX_SIZE(val)		    vBIT(val, 24, 8)
#define	VXGE_HAL_RPE_CFG0_BS_ACK_WQE_PF_ENA		    mBIT(38)
#define	VXGE_HAL_RPE_CFG0_IWARP_ISL_PF_ENA		    mBIT(39)
#define	VXGE_HAL_RPE_CFG0_PDM_FRAME_ECC_ENABLE_N	    mBIT(43)
#define	VXGE_HAL_RPE_CFG0_PDM_RCMD_ECC_ENABLE_N		    mBIT(44)
#define	VXGE_HAL_RPE_CFG0_RCQ_ECC_ENABLE_N		    mBIT(45)
#define	VXGE_HAL_RPE_CFG0_RCO_PBLE_ECC_ENABLE_N		    mBIT(46)
#define	VXGE_HAL_RPE_CFG0_RCM_ECC_ENABLE_N		    mBIT(47)
#define	VXGE_HAL_RPE_CFG0_PDM_FRAME_PHASE_ENABLE	    mBIT(50)
#define	VXGE_HAL_RPE_CFG0_DLM_RCMD_PHASE_ENABLE		    mBIT(51)
#define	VXGE_HAL_RPE_CFG0_MSG_RCMD_PHASE_ENABLE		    mBIT(52)
#define	VXGE_HAL_RPE_CFG0_PDM_RCMD_PHASE_ENABLE		    mBIT(53)
#define	VXGE_HAL_RPE_CFG0_RCQ_PHASE_ENABLE		    mBIT(54)
#define	VXGE_HAL_RPE_CFG0_RCO_PBLE_PHASE_ENABLE		    mBIT(55)
/* 0x041a0 */	u64	rpe_cfg1;
#define	VXGE_HAL_RPE_CFG1_WQEOWN_LRO_CTR_ENA		    mBIT(5)
#define	VXGE_HAL_RPE_CFG1_WQEOWN_BS_CTR_ENA		    mBIT(6)
#define	VXGE_HAL_RPE_CFG1_WQEOWN_IWARP_CTR_ENA		    mBIT(7)
#define	VXGE_HAL_RPE_CFG1_DLM_RCMD_MAX_CREDITS(val)	    vBIT(val, 10, 6)
#define	VXGE_HAL_RPE_CFG1_MSG_RCMD_MAX_CREDITS(val)	    vBIT(val, 18, 6)
#define	VXGE_HAL_RPE_CFG1_PDM_RCMD_MAX_CREDITS(val)	    vBIT(val, 25, 7)
#define	VXGE_HAL_RPE_CFG1_RCQ_MAX_CREDITS(val)		    vBIT(val, 32, 8)
#define	VXGE_HAL_RPE_CFG1_RCQ_DLM_PRI(val)		    vBIT(val, 46, 2)
#define	VXGE_HAL_RPE_CFG1_RCQ_MSG_PRI(val)		    vBIT(val, 54, 2)
#define	VXGE_HAL_RPE_CFG1_RCQ_PDM_PRI(val)		    vBIT(val, 62, 2)
/* 0x041a8 */	u64	rpe_cfg2;
#define	VXGE_HAL_RPE_CFG2_RCQ_ARB_CAL0_PRI(val)		    vBIT(val, 6, 2)
#define	VXGE_HAL_RPE_CFG2_RCQ_ARB_CAL1_PRI(val)		    vBIT(val, 14, 2)
#define	VXGE_HAL_RPE_CFG2_RCQ_ARB_CAL2_PRI(val)		    vBIT(val, 22, 2)
#define	VXGE_HAL_RPE_CFG2_RCQ_ARB_CAL3_PRI(val)		    vBIT(val, 30, 2)
#define	VXGE_HAL_RPE_CFG2_RCQ_ARB_CAL4_PRI(val)		    vBIT(val, 38, 2)
#define	VXGE_HAL_RPE_CFG2_RCQ_ARB_CAL5_PRI(val)		    vBIT(val, 46, 2)
#define	VXGE_HAL_RPE_CFG2_RDMA_WRITE_ORDER_ENABLE	    mBIT(49)
#define	VXGE_HAL_RPE_CFG2_RDMA_RDRESP_ORDER_ENABLE	    mBIT(50)
#define	VXGE_HAL_RPE_CFG2_RDMA_SEND_ORDER_ENABLE	    mBIT(51)
#define	VXGE_HAL_RPE_CFG2_RDMA_RDREQ_ORDER_ENABLE	    mBIT(52)
#define	VXGE_HAL_RPE_CFG2_RDMA_TERMINATE_ORDER_ENABLE	    mBIT(53)
#define	VXGE_HAL_RPE_CFG2_IWARP_MISALIGNED_ORDER_ENABLE	    mBIT(54)
#define	VXGE_HAL_RPE_CFG2_IWARP_TIMER_ORDER_ENABLE	    mBIT(55)
#define	VXGE_HAL_RPE_CFG2_IWARP_IMSG_ORDER_ENABLE	    mBIT(56)
#define	VXGE_HAL_RPE_CFG2_BS_IWARP_ACK_ORDER_ENABLE	    mBIT(57)
#define	VXGE_HAL_RPE_CFG2_BS_DATA_ORDER_ENABLE		    mBIT(58)
#define	VXGE_HAL_RPE_CFG2_BS_TIMER_ORDER_ENABLE		    mBIT(59)
#define	VXGE_HAL_RPE_CFG2_BS_IMSG_ORDER_ENABLE		    mBIT(60)
#define	VXGE_HAL_RPE_CFG2_LRO_FRAME_ORDER_ENABLE	    mBIT(61)
#define	VXGE_HAL_RPE_CFG2_LRO_TIMER_ORDER_ENABLE	    mBIT(62)
#define	VXGE_HAL_RPE_CFG2_LRO_IMSG_ORDER_ENABLE		    mBIT(63)
	u8	unused041c0[0x041c0 - 0x041b0];

/* 0x041c0 */	u64	rpe_cfg5;
#define	VXGE_HAL_RPE_CFG5_LRO_IGNORE_RPA_PARSE_ERRS	    mBIT(4)
#define	VXGE_HAL_RPE_CFG5_LRO_IGNORE_FRM_INT_ERRS	    mBIT(5)
#define	VXGE_HAL_RPE_CFG5_LRO_IGNORE_L3_CSUM_ERRS	    mBIT(6)
#define	VXGE_HAL_RPE_CFG5_LRO_IGNORE_L4_CSUM_ERRS	    mBIT(7)
#define	VXGE_HAL_RPE_CFG5_LRO_NORM_SCATTER_IPV4_OPTIONS	    mBIT(14)
#define	VXGE_HAL_RPE_CFG5_LRO_NORM_SCATTER_IPV6_EXTHDRS	    mBIT(15)
#define	VXGE_HAL_RPE_CFG5_USE_CONCISE_ADAPTIVE_LRO_CQE	    mBIT(22)
#define	VXGE_HAL_RPE_CFG5_USE_CONCISE_PRECONFIG_LRO_CQE	    mBIT(23)
/* 0x041c8 */	u64	wqeown0;
#define	VXGE_HAL_WQEOWN0_RPE_LRO_CTR(val)		    vBIT(val, 13, 19)
#define	VXGE_HAL_WQEOWN0_RPE_BS_CTR(val)		    vBIT(val, 45, 19)
/* 0x041d0 */	u64	wqeown1;
#define	VXGE_HAL_WQEOWN1_RPE_IWARP_CTR(val)		    vBIT(val, 13, 19)
/* 0x041d8 */	u64	rpe_wqeown2;
#define	VXGE_HAL_RPE_WQEOWN2_LRO_THRESHOLD(val)		    vBIT(val, 13, 19)
#define	VXGE_HAL_RPE_WQEOWN2_BS_THRESHOLD(val)		    vBIT(val, 45, 19)
	u8	unused04200[0x04200 - 0x041e0];

/* 0x04200 */	u64	pe_ctxt;
#define	VXGE_HAL_PE_CTXT_SCC_TRIGGER_READ		    mBIT(7)
#define	VXGE_HAL_PE_CTXT_S1_SIZE(val)			    vBIT(val, 10, 6)
#define	VXGE_HAL_PE_CTXT_S2_SIZE(val)			    vBIT(val, 26, 6)
#define	VXGE_HAL_PE_CTXT_S3_SIZE(val)			    vBIT(val, 42, 6)
#define	VXGE_HAL_PE_CTXT_NP_XFER			    mBIT(55)
#define	VXGE_HAL_PE_CTXT_NP_SPACER			    mBIT(63)
/* 0x04208 */	u64	pe_cfg;
#define	VXGE_HAL_PE_CFG_RXPE_ECC_ENABLE_N		    mBIT(7)
#define	VXGE_HAL_PE_CFG_TXPE_ECC_ENABLE_N		    mBIT(15)
#define	VXGE_HAL_PE_CFG_DLM_ECC_ENABLE_N		    mBIT(23)
#define	VXGE_HAL_PE_CFG_CDP_ECC_ENABLE_N		    mBIT(31)
#define	VXGE_HAL_PE_CFG_PET_ECC_ENABLE_N		    mBIT(39)
#define	VXGE_HAL_PE_CFG_MAX_RXB2B(val)			    vBIT(val, 56, 8)
/* 0x04210 */	u64	pe_stats_cmd;
#define	VXGE_HAL_PE_STATS_CMD_GO			    mBIT(7)
#define	VXGE_HAL_PE_STATS_CMD_SELECT_TXPE		    mBIT(15)
#define	VXGE_HAL_PE_STATS_CMD_ADDRESS(val)		    vBIT(val, 21, 11)
/* 0x04218 */	u64	pe_stats_data;
#define	VXGE_HAL_PE_STATS_DATA_PE_RETURNED(val)		    vBIT(val, 0, 64)
/* 0x04220 */	u64	rxpe_fp_mask;
#define	VXGE_HAL_RXPE_FP_MASK_RXPE_FP_MASK(val)		    vBIT(val, 18, 46)
/* 0x04228 */	u64	rxpe_cfg;
#define	VXGE_HAL_RXPE_CFG_FW_EXTEND_FP	mBIT(7)
#define	VXGE_HAL_RXPE_CFG_RETXK_SP_DONE	mBIT(15)
/* 0x04230 */	u64	pe_xt_ctrl1;
#define	VXGE_HAL_PE_XT_CTRL1_IRAM_ADDRESS(val)		    vBIT(val, 4, 12)
#define	VXGE_HAL_PE_XT_CTRL1_ENABLE_GO_FOR_WR		    mBIT(23)
#define	VXGE_HAL_PE_XT_CTRL1_IRAM_READ	mBIT(27)
#define	VXGE_HAL_PE_XT_CTRL1_TXP_IRAM_SEL		    mBIT(29)
#define	VXGE_HAL_PE_XT_CTRL1_RXP0_IRAM_SEL		    mBIT(30)
#define	VXGE_HAL_PE_XT_CTRL1_RXP1_IRAM_SEL		    mBIT(31)
#define	VXGE_HAL_PE_XT_CTRL1_TXP_IRAM_ECC_ENABLE_N	    mBIT(37)
#define	VXGE_HAL_PE_XT_CTRL1_RXP0_IRAM_ECC_ENABLE_N	    mBIT(38)
#define	VXGE_HAL_PE_XT_CTRL1_RXP1_IRAM_ECC_ENABLE_N	    mBIT(39)
#define	VXGE_HAL_PE_XT_CTRL1_TXP_DRAM_ECC_ENABLE_N	    mBIT(46)
#define	VXGE_HAL_PE_XT_CTRL1_RXP_DRAM_ECC_ENABLE_N	    mBIT(47)
#define	VXGE_HAL_PE_XT_CTRL1_TXP_RUNSTALL		    mBIT(53)
#define	VXGE_HAL_PE_XT_CTRL1_RXP0_RUNSTALL		    mBIT(54)
#define	VXGE_HAL_PE_XT_CTRL1_RXP1_RUNSTALL		    mBIT(55)
#define	VXGE_HAL_PE_XT_CTRL1_TXP_BRESET			    mBIT(61)
#define	VXGE_HAL_PE_XT_CTRL1_RXP0_BRESET		    mBIT(62)
#define	VXGE_HAL_PE_XT_CTRL1_RXP1_BRESET		    mBIT(63)
/* 0x04238 */	u64	pe_xt_ctrl2;
#define	VXGE_HAL_PE_XT_CTRL2_IRAM_WRITE_DATA(val)	    vBIT(val, 0, 64)
/* 0x04240 */	u64	pe_xt_ctrl3;
#define	VXGE_HAL_PE_XT_CTRL3_GO	mBIT(63)
/* 0x04248 */	u64	pe_xt_ctrl4;
#define	VXGE_HAL_PE_XT_CTRL4_PE_IRAM_READ_DATA(val)	    vBIT(val, 0, 64)
/* 0x04250 */	u64	pet_iwarp_counters;
#define	VXGE_HAL_PET_IWARP_COUNTERS_MASTER(val)		    vBIT(val, 0, 32)
#define	VXGE_HAL_PET_IWARP_COUNTERS_INTERVAL(val)	    vBIT(val, 40, 24)
/* 0x04258 */	u64	pet_iwarp_slow_counter;
#define	VXGE_HAL_PET_IWARP_SLOW_COUNTER_MASTER(val)	    vBIT(val, 0, 32)
/* 0x04260 */	u64	pet_iwarp_timers;
#define	VXGE_HAL_PET_IWARP_TIMERS_TCP_NOW(val)		    vBIT(val, 0, 32)
#define	VXGE_HAL_PET_IWARP_TIMERS_TCP_SLOW_CLK(val)	    vBIT(val, 32, 32)
/* 0x04268 */	u64	pet_lro_cfg;
#define	VXGE_HAL_PET_LRO_CFG_START_VALUE(val)		    vBIT(val, 6, 2)
/* 0x04270 */	u64	pet_lro_counters;
#define	VXGE_HAL_PET_LRO_COUNTERS_MASTER(val)		    vBIT(val, 0, 32)
#define	VXGE_HAL_PET_LRO_COUNTERS_INTERVAL(val)		    vBIT(val, 40, 24)
/* 0x04278 */	u64	pet_timer_bp_ctrl;
#define	VXGE_HAL_PET_TIMER_BP_CTRL_RD_XON		    mBIT(7)
#define	VXGE_HAL_PET_TIMER_BP_CTRL_WR_XON		    mBIT(15)
#define	VXGE_HAL_PET_TIMER_BP_CTRL_ROCRC_BYP		    mBIT(23)
#define	VXGE_HAL_PET_TIMER_BP_CTRL_H2L_BYP		    mBIT(31)
/* 0x04280 */	u64	pe_vp_ack[17];
#define	VXGE_HAL_PE_VP_ACK_BLK_LIMIT(val)		    vBIT(val, 32, 32)
/* 0x04308 */	u64	pe_vp[17];
#define	VXGE_HAL_PE_VP_RIRR_BLK_LIMIT(val)		    vBIT(val, 0, 32)
#define	VXGE_HAL_PE_VP_LIRR_BLK_LIMIT(val)		    vBIT(val, 32, 32)
/* 0x04390 */	u64	dlm_cfg;
#define	VXGE_HAL_DLM_CFG_AWRQ_PHASE_ENABLE		    mBIT(7)
#define	VXGE_HAL_DLM_CFG_ACK_PTR_AE_LEVEL(val)		    vBIT(val, 12, 4)
#define	VXGE_HAL_DLM_CFG_LWRQ_PHASE_ENABLE		    mBIT(23)
#define	VXGE_HAL_DLM_CFG_LIRR_PTR_AE_LEVEL(val)		    vBIT(val, 28, 4)
#define	VXGE_HAL_DLM_CFG_RIRR_PTR_AE_LEVEL(val)		    vBIT(val, 44, 4)
	u8	unused04400[0x04400 - 0x04398];

/* 0x04400 */	u64	txpe_towi_cfg;
#define	VXGE_HAL_TXPE_TOWI_CFG_TOWI_CACHE_SIZE(val)	    vBIT(val, 48, 8)
#define	VXGE_HAL_TXPE_TOWI_CFG_TOWI_DMA_THRESHOLD(val)	    vBIT(val, 56, 8)
	u8	unused04410[0x04410 - 0x04408];

/* 0x04410 */	u64	txpe_pmon;
#define	VXGE_HAL_TXPE_PMON_GO				    mBIT(15)
#define	VXGE_HAL_TXPE_PMON_SAMPLE_PERIOD(val)		    vBIT(val, 16, 48)
/* 0x04418 */	u64	txpe_pmon_downcount;
#define	VXGE_HAL_TXPE_PMON_DOWNCOUNT_TXPE_REMAINDER(val)    vBIT(val, 16, 48)
/* 0x04420 */	u64	txpe_pmon_event;
#define	VXGE_HAL_TXPE_PMON_EVENT_TXPE_STALL_CNT(val)	    vBIT(val, 16, 48)
/* 0x04428 */	u64	txpe_pmon_other;
#define	VXGE_HAL_TXPE_PMON_OTHER_TXPE_STALL_CNT(val)	    vBIT(val, 16, 48)
	u8	unused04500[0x04500 - 0x04430];

/* 0x04500 */	u64	oes_inevt;
#define	VXGE_HAL_OES_INEVT_PRIORITY_0(val)		    vBIT(val, 5, 3)
#define	VXGE_HAL_OES_INEVT_PRIORITY_1(val)		    vBIT(val, 13, 3)
#define	VXGE_HAL_OES_INEVT_PRIORITY_2(val)		    vBIT(val, 21, 3)
#define	VXGE_HAL_OES_INEVT_PRIORITY_3(val)		    vBIT(val, 29, 3)
#define	VXGE_HAL_OES_INEVT_PRIORITY_4(val)		    vBIT(val, 37, 3)
#define	VXGE_HAL_OES_INEVT_CFG_SP_WRR			    mBIT(63)
/* 0x04508 */	u64	oes_inbkbkevt;
#define	VXGE_HAL_OES_INBKBKEVT_PRIORITY_0(val)		    vBIT(val, 5, 3)
#define	VXGE_HAL_OES_INBKBKEVT_PRIORITY_1(val)		    vBIT(val, 13, 3)
#define	VXGE_HAL_OES_INBKBKEVT_PRIORITY_2(val)		    vBIT(val, 21, 3)
#define	VXGE_HAL_OES_INBKBKEVT_PRIORITY_3(val)		    vBIT(val, 29, 3)
#define	VXGE_HAL_OES_INBKBKEVT_PRIORITY_4(val)		    vBIT(val, 37, 3)
/* 0x04510 */	u64	oes_inevt_wrr0;
#define	VXGE_HAL_OES_INEVT_WRR0_SS_0(val)		    vBIT(val, 5, 3)
#define	VXGE_HAL_OES_INEVT_WRR0_SS_1(val)		    vBIT(val, 13, 3)
#define	VXGE_HAL_OES_INEVT_WRR0_SS_2(val)		    vBIT(val, 21, 3)
#define	VXGE_HAL_OES_INEVT_WRR0_SS_3(val)		    vBIT(val, 29, 3)
#define	VXGE_HAL_OES_INEVT_WRR0_SS_4(val)		    vBIT(val, 37, 3)
#define	VXGE_HAL_OES_INEVT_WRR0_SS_5(val)		    vBIT(val, 45, 3)
#define	VXGE_HAL_OES_INEVT_WRR0_SS_6(val)		    vBIT(val, 53, 3)
#define	VXGE_HAL_OES_INEVT_WRR0_SS_7(val)		    vBIT(val, 61, 3)
/* 0x04518 */	u64	oes_inevt_wrr1;
#define	VXGE_HAL_OES_INEVT_WRR1_SS_8(val)		    vBIT(val, 5, 3)
#define	VXGE_HAL_OES_INEVT_WRR1_SS_9(val)		    vBIT(val, 13, 3)
#define	VXGE_HAL_OES_INEVT_WRR1_SS_10(val)		    vBIT(val, 21, 3)
#define	VXGE_HAL_OES_INEVT_WRR1_SS_11(val)		    vBIT(val, 29, 3)
#define	VXGE_HAL_OES_INEVT_WRR1_SS_12(val)		    vBIT(val, 37, 3)
#define	VXGE_HAL_OES_INEVT_WRR1_SS_13(val)		    vBIT(val, 45, 3)
#define	VXGE_HAL_OES_INEVT_WRR1_SS_14(val)		    vBIT(val, 53, 3)
/* 0x04520 */	u64	oes_pendevt;
#define	VXGE_HAL_OES_PENDEVT_PRIORITY_0(val)		    vBIT(val, 5, 3)
#define	VXGE_HAL_OES_PENDEVT_PRIORITY_1(val)		    vBIT(val, 13, 3)
#define	VXGE_HAL_OES_PENDEVT_PRIORITY_2(val)		    vBIT(val, 21, 3)
#define	VXGE_HAL_OES_PENDEVT_PRIORITY_3(val)		    vBIT(val, 29, 3)
#define	VXGE_HAL_OES_PENDEVT_PRIORITY_4(val)		    vBIT(val, 37, 3)
#define	VXGE_HAL_OES_PENDEVT_CFG_SP_WRR			    mBIT(63)
/* 0x04528 */	u64	oes_pendbkbkevt;
#define	VXGE_HAL_OES_PENDBKBKEVT_PRIORITY_0(val)	    vBIT(val, 5, 3)
#define	VXGE_HAL_OES_PENDBKBKEVT_PRIORITY_1(val)	    vBIT(val, 13, 3)
#define	VXGE_HAL_OES_PENDBKBKEVT_PRIORITY_2(val)	    vBIT(val, 21, 3)
#define	VXGE_HAL_OES_PENDBKBKEVT_PRIORITY_3(val)	    vBIT(val, 29, 3)
#define	VXGE_HAL_OES_PENDBKBKEVT_PRIORITY_4(val)	    vBIT(val, 37, 3)
/* 0x04530 */	u64	oes_pendevt_wrr0;
#define	VXGE_HAL_OES_PENDEVT_WRR0_SS_0(val)		    vBIT(val, 5, 3)
#define	VXGE_HAL_OES_PENDEVT_WRR0_SS_1(val)		    vBIT(val, 13, 3)
#define	VXGE_HAL_OES_PENDEVT_WRR0_SS_2(val)		    vBIT(val, 21, 3)
#define	VXGE_HAL_OES_PENDEVT_WRR0_SS_3(val)		    vBIT(val, 29, 3)
#define	VXGE_HAL_OES_PENDEVT_WRR0_SS_4(val)		    vBIT(val, 37, 3)
#define	VXGE_HAL_OES_PENDEVT_WRR0_SS_5(val)		    vBIT(val, 45, 3)
#define	VXGE_HAL_OES_PENDEVT_WRR0_SS_6(val)		    vBIT(val, 53, 3)
#define	VXGE_HAL_OES_PENDEVT_WRR0_SS_7(val)		    vBIT(val, 61, 3)
/* 0x04538 */	u64	oes_pendevt_wrr1;
#define	VXGE_HAL_OES_PENDEVT_WRR1_SS_8(val)		    vBIT(val, 5, 3)
#define	VXGE_HAL_OES_PENDEVT_WRR1_SS_9(val)		    vBIT(val, 13, 3)
#define	VXGE_HAL_OES_PENDEVT_WRR1_SS_10(val)		    vBIT(val, 21, 3)
#define	VXGE_HAL_OES_PENDEVT_WRR1_SS_11(val)		    vBIT(val, 29, 3)
#define	VXGE_HAL_OES_PENDEVT_WRR1_SS_12(val)		    vBIT(val, 37, 3)
#define	VXGE_HAL_OES_PENDEVT_WRR1_SS_13(val)		    vBIT(val, 45, 3)
#define	VXGE_HAL_OES_PENDEVT_WRR1_SS_14(val)		    vBIT(val, 53, 3)
/* 0x04540 */	u64	oes_pend_queue;
#define	VXGE_HAL_OES_PEND_QUEUE_RX_PEND_THRESHOLD(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_OES_PEND_QUEUE_TX_PEND_THRESHOLD(val)	    vBIT(val, 57, 7)
	u8	unused04800[0x04800 - 0x04548];

/* 0x04800 */	u64	rocrc_bypq0_stat_watermark;
#define	VXGE_HAL_ROCRC_BYPQ0_STAT_WATERMARK_RCQ_ROCRC_BYPQ0_STAT_WATERMARK(val)\
							    vBIT(val, 11, 22)
/* 0x04808 */	u64	rocrc_bypq1_stat_watermark;
#define	VXGE_HAL_ROCRC_BYPQ1_STAT_WATERMARK_RCQ_ROCRC_BYPQ1_STAT_WATERMARK(val)\
							    vBIT(val, 11, 22)
/* 0x04810 */	u64	rocrc_bypq2_stat_watermark;
#define	VXGE_HAL_ROCRC_BYPQ2_STAT_WATERMARK_RCQ_ROCRC_BYPQ2_STAT_WATERMARK(val)\
							    vBIT(val, 11, 22)
/* 0x04818 */	u64	noa_wct_ctrl;
#define	VXGE_HAL_NOA_WCT_CTRL_VP_INT_NUM		    mBIT(0)
/* 0x04820 */	u64	rc_cfg2;
#define	VXGE_HAL_RC_CFG2_BUFF1_SIZE(val)		    vBIT(val, 0, 16)
#define	VXGE_HAL_RC_CFG2_BUFF2_SIZE(val)		    vBIT(val, 16, 16)
#define	VXGE_HAL_RC_CFG2_BUFF3_SIZE(val)		    vBIT(val, 32, 16)
#define	VXGE_HAL_RC_CFG2_BUFF4_SIZE(val)		    vBIT(val, 48, 16)
/* 0x04828 */	u64	rc_cfg3;
#define	VXGE_HAL_RC_CFG3_BUFF5_SIZE(val)		    vBIT(val, 0, 16)
/* 0x04830 */	u64	rx_multi_cast_ctrl1;
#define	VXGE_HAL_RX_MULTI_CAST_CTRL1_ENABLE		    mBIT(7)
#define	VXGE_HAL_RX_MULTI_CAST_CTRL1_DELAY_COUNT(val)	    vBIT(val, 11, 5)
/* 0x04838 */	u64	rxdm_dbg_rd;
#define	VXGE_HAL_RXDM_DBG_RD_ADDR(val)			    vBIT(val, 0, 12)
#define	VXGE_HAL_RXDM_DBG_RD_ENABLE			    mBIT(31)
/* 0x04840 */	u64	rxdm_dbg_rd_data;
#define	VXGE_HAL_RXDM_DBG_RD_DATA_RMC_RXDM_DBG_RD_DATA(val) vBIT(val, 0, 64)
/* 0x04848 */	u64	rqa_top_prty_for_vh[17];
#define	VXGE_HAL_RQA_TOP_PRTY_FOR_VH_RQA_TOP_PRTY_FOR_VH(val) vBIT(val, 59, 5)
	u8	unused04900[0x04900 - 0x048d0];

/* 0x04900 */	u64	tim_status;
#define	VXGE_HAL_TIM_STATUS_TIM_RESET_IN_PROGRESS	    mBIT(0)
/* 0x04908 */	u64	tim_ecc_enable;
#define	VXGE_HAL_TIM_ECC_ENABLE_VBLS_N			    mBIT(7)
#define	VXGE_HAL_TIM_ECC_ENABLE_BMAP_N			    mBIT(15)
#define	VXGE_HAL_TIM_ECC_ENABLE_BMAP_MSG_N		    mBIT(23)
/* 0x04910 */	u64	tim_bp_ctrl;
#define	VXGE_HAL_TIM_BP_CTRL_RD_XON			    mBIT(7)
#define	VXGE_HAL_TIM_BP_CTRL_WR_XON			    mBIT(15)
#define	VXGE_HAL_TIM_BP_CTRL_ROCRC_BYP			    mBIT(23)
/* 0x04918 */	u64	tim_resource_assignment_vh[17];
#define	VXGE_HAL_TIM_RESOURCE_ASSIGNMENT_VH_BMAP_ROOT(val)  vBIT(val, 0, 32)
/* 0x049a0 */	u64	tim_bmap_mapping_vp_err[17];
#define	VXGE_HAL_TIM_BMAP_MAPPING_VP_ERR_TIM_DEST_VPATH(val) vBIT(val, 3, 5)
	u8	unused04b00[0x04b00 - 0x04a28];

/* 0x04b00 */	u64	gcmg2_int_status;
#define	VXGE_HAL_GCMG2_INT_STATUS_GXTMC_ERR_GXTMC_INT	    mBIT(7)
#define	VXGE_HAL_GCMG2_INT_STATUS_GCP_ERR_GCP_INT	    mBIT(15)
#define	VXGE_HAL_GCMG2_INT_STATUS_CMC_ERR_CMC_INT	    mBIT(23)
/* 0x04b08 */	u64	gcmg2_int_mask;
/* 0x04b10 */	u64	gxtmc_err_reg;
#define	VXGE_HAL_GXTMC_ERR_REG_XTMC_BDT_MEM_DB_ERR(val)	    vBIT(val, 0, 4)
#define	VXGE_HAL_GXTMC_ERR_REG_XTMC_BDT_MEM_SG_ERR(val)	    vBIT(val, 4, 4)
#define	VXGE_HAL_GXTMC_ERR_REG_XTMC_CMC_RD_DATA_DB_ERR	    mBIT(8)
#define	VXGE_HAL_GXTMC_ERR_REG_XTMC_REQ_FIFO_ERR	    mBIT(9)
#define	VXGE_HAL_GXTMC_ERR_REG_XTMC_REQ_DATA_FIFO_ERR	    mBIT(10)
#define	VXGE_HAL_GXTMC_ERR_REG_XTMC_WR_RSP_FIFO_ERR	    mBIT(11)
#define	VXGE_HAL_GXTMC_ERR_REG_XTMC_RD_RSP_FIFO_ERR	    mBIT(12)
#define	VXGE_HAL_GXTMC_ERR_REG_XTMC_CMI_WRP_FIFO_ERR	    mBIT(13)
#define	VXGE_HAL_GXTMC_ERR_REG_XTMC_CMI_WRP_ERR		    mBIT(14)
#define	VXGE_HAL_GXTMC_ERR_REG_XTMC_CMI_RRP_FIFO_ERR	    mBIT(15)
#define	VXGE_HAL_GXTMC_ERR_REG_XTMC_CMI_RRP_ERR		    mBIT(16)
#define	VXGE_HAL_GXTMC_ERR_REG_XTMC_CMI_DATA_SM_ERR	    mBIT(17)
#define	VXGE_HAL_GXTMC_ERR_REG_XTMC_CMI_CMC0_IF_ERR	    mBIT(18)
#define	VXGE_HAL_GXTMC_ERR_REG_XTMC_BDT_CMI_ARB_SM_ERR	    mBIT(19)
#define	VXGE_HAL_GXTMC_ERR_REG_XTMC_BDT_CMI_CFC_SM_ERR	    mBIT(20)
#define	VXGE_HAL_GXTMC_ERR_REG_XTMC_BDT_CMI_DFETCH_CREDIT_OVERFLOW mBIT(21)
#define	VXGE_HAL_GXTMC_ERR_REG_XTMC_BDT_CMI_DFETCH_CREDIT_UNDERFLOW mBIT(22)
#define	VXGE_HAL_GXTMC_ERR_REG_XTMC_BDT_CMI_DFETCH_SM_ERR   mBIT(23)
#define	VXGE_HAL_GXTMC_ERR_REG_XTMC_BDT_CMI_RCTRL_CREDIT_OVERFLOW mBIT(24)
#define	VXGE_HAL_GXTMC_ERR_REG_XTMC_BDT_CMI_RCTRL_CREDIT_UNDERFLOW mBIT(25)
#define	VXGE_HAL_GXTMC_ERR_REG_XTMC_BDT_CMI_RCTRL_SM_ERR    mBIT(26)
#define	VXGE_HAL_GXTMC_ERR_REG_XTMC_BDT_CMI_WCOMPL_SM_ERR   mBIT(27)
#define	VXGE_HAL_GXTMC_ERR_REG_XTMC_BDT_CMI_WCOMPL_TAG_ERR  mBIT(28)
#define	VXGE_HAL_GXTMC_ERR_REG_XTMC_BDT_CMI_WREQ_SM_ERR	    mBIT(29)
#define	VXGE_HAL_GXTMC_ERR_REG_XTMC_BDT_CMI_WREQ_FIFO_ERR   mBIT(30)
#define	VXGE_HAL_GXTMC_ERR_REG_XTMC_CP2BDT_RFIFO_POP_ERR    mBIT(31)
#define	VXGE_HAL_GXTMC_ERR_REG_XTMC_XTMC_BDT_CMI_OP_ERR	    mBIT(32)
#define	VXGE_HAL_GXTMC_ERR_REG_XTMC_XTMC_BDT_DFETCH_OP_ERR  mBIT(33)
#define	VXGE_HAL_GXTMC_ERR_REG_XTMC_XTMC_BDT_DFIFO_ERR	    mBIT(34)
#define	VXGE_HAL_GXTMC_ERR_REG_XTMC_CMI_ARB_SM_ERR	    mBIT(35)
/* 0x04b18 */	u64	gxtmc_err_mask;
/* 0x04b20 */	u64	gxtmc_err_alarm;
/* 0x04b28 */	u64	cmc_err_reg;
#define	VXGE_HAL_CMC_ERR_REG_CMC_CMC_SM_ERR		    mBIT(0)
/* 0x04b30 */	u64	cmc_err_mask;
/* 0x04b38 */	u64	cmc_err_alarm;
/* 0x04b40 */	u64	gcp_err_reg;
#define	VXGE_HAL_GCP_ERR_REG_CP_H2L2CP_FIFO_ERR		    mBIT(0)
#define	VXGE_HAL_GCP_ERR_REG_CP_STC2CP_FIFO_ERR		    mBIT(1)
#define	VXGE_HAL_GCP_ERR_REG_CP_STE2CP_FIFO_ERR		    mBIT(2)
#define	VXGE_HAL_GCP_ERR_REG_CP_TTE2CP_FIFO_ERR		    mBIT(3)
/* 0x04b48 */	u64	gcp_err_mask;
/* 0x04b50 */	u64	gcp_err_alarm;
/* 0x04b58 */	u64	cmc_l2_client_uqm_1;
#define	VXGE_HAL_CMC_L2_CLIENT_UQM_1_NUMBER(val)	    vBIT(val, 5, 3)
/* 0x04b60 */	u64	cmc_l2_client_ssc_l;
#define	VXGE_HAL_CMC_L2_CLIENT_SSC_L_NUMBER(val)	    vBIT(val, 5, 3)
/* 0x04b68 */	u64	cmc_l2_client_qcc_sqm_0;
#define	VXGE_HAL_CMC_L2_CLIENT_QCC_SQM_0_NUMBER(val)	    vBIT(val, 5, 3)
/* 0x04b70 */	u64	cmc_l2_client_dam_0;
#define	VXGE_HAL_CMC_L2_CLIENT_DAM_0_NUMBER(val)	    vBIT(val, 5, 3)
/* 0x04b78 */	u64	cmc_l2_client_h2l_0;
#define	VXGE_HAL_CMC_L2_CLIENT_H2L_0_NUMBER(val)	    vBIT(val, 5, 3)
/* 0x04b80 */	u64	cmc_l2_client_stc_0;
#define	VXGE_HAL_CMC_L2_CLIENT_STC_0_NUMBER(val)	    vBIT(val, 5, 3)
/* 0x04b88 */	u64	cmc_l2_client_xtmc_0;
#define	VXGE_HAL_CMC_L2_CLIENT_XTMC_0_NUMBER(val)	    vBIT(val, 5, 3)
/* 0x04b90 */	u64	cmc_wrr_l2_calendar_0;
#define	VXGE_HAL_CMC_WRR_L2_CALENDAR_0_NUMBER_0(val)	    vBIT(val, 5, 3)
#define	VXGE_HAL_CMC_WRR_L2_CALENDAR_0_NUMBER_1(val)	    vBIT(val, 13, 3)
#define	VXGE_HAL_CMC_WRR_L2_CALENDAR_0_NUMBER_2(val)	    vBIT(val, 21, 3)
#define	VXGE_HAL_CMC_WRR_L2_CALENDAR_0_NUMBER_3(val)	    vBIT(val, 29, 3)
#define	VXGE_HAL_CMC_WRR_L2_CALENDAR_0_NUMBER_4(val)	    vBIT(val, 37, 3)
#define	VXGE_HAL_CMC_WRR_L2_CALENDAR_0_NUMBER_5(val)	    vBIT(val, 45, 3)
#define	VXGE_HAL_CMC_WRR_L2_CALENDAR_0_NUMBER_6(val)	    vBIT(val, 53, 3)
#define	VXGE_HAL_CMC_WRR_L2_CALENDAR_0_NUMBER_7(val)	    vBIT(val, 61, 3)
/* 0x04b98 */	u64	cmc_wrr_l2_calendar_1;
#define	VXGE_HAL_CMC_WRR_L2_CALENDAR_1_NUMBER_8(val)	    vBIT(val, 5, 3)
#define	VXGE_HAL_CMC_WRR_L2_CALENDAR_1_NUMBER_9(val)	    vBIT(val, 13, 3)
#define	VXGE_HAL_CMC_WRR_L2_CALENDAR_1_NUMBER_10(val)	    vBIT(val, 21, 3)
#define	VXGE_HAL_CMC_WRR_L2_CALENDAR_1_NUMBER_11(val)	    vBIT(val, 29, 3)
#define	VXGE_HAL_CMC_WRR_L2_CALENDAR_1_NUMBER_12(val)	    vBIT(val, 37, 3)
#define	VXGE_HAL_CMC_WRR_L2_CALENDAR_1_NUMBER_13(val)	    vBIT(val, 45, 3)
#define	VXGE_HAL_CMC_WRR_L2_CALENDAR_1_NUMBER_14(val)	    vBIT(val, 53, 3)
#define	VXGE_HAL_CMC_WRR_L2_CALENDAR_1_NUMBER_15(val)	    vBIT(val, 61, 3)
/* 0x04ba0 */	u64	cmc_wrr_l2_calendar_2;
#define	VXGE_HAL_CMC_WRR_L2_CALENDAR_2_NUMBER_16(val)	    vBIT(val, 5, 3)
#define	VXGE_HAL_CMC_WRR_L2_CALENDAR_2_NUMBER_17(val)	    vBIT(val, 13, 3)
#define	VXGE_HAL_CMC_WRR_L2_CALENDAR_2_NUMBER_18(val)	    vBIT(val, 21, 3)
#define	VXGE_HAL_CMC_WRR_L2_CALENDAR_2_NUMBER_19(val)	    vBIT(val, 29, 3)
#define	VXGE_HAL_CMC_WRR_L2_CALENDAR_2_NUMBER_20(val)	    vBIT(val, 37, 3)
#define	VXGE_HAL_CMC_WRR_L2_CALENDAR_2_NUMBER_21(val)	    vBIT(val, 45, 3)
#define	VXGE_HAL_CMC_WRR_L2_CALENDAR_2_NUMBER_22(val)	    vBIT(val, 53, 3)
#define	VXGE_HAL_CMC_WRR_L2_CALENDAR_2_NUMBER_23(val)	    vBIT(val, 61, 3)
/* 0x04ba8 */	u64	cmc_wrr_l2_calendar_3;
#define	VXGE_HAL_CMC_WRR_L2_CALENDAR_3_NUMBER_24(val)	    vBIT(val, 5, 3)
#define	VXGE_HAL_CMC_WRR_L2_CALENDAR_3_NUMBER_25(val)	    vBIT(val, 13, 3)
#define	VXGE_HAL_CMC_WRR_L2_CALENDAR_3_NUMBER_26(val)	    vBIT(val, 21, 3)
#define	VXGE_HAL_CMC_WRR_L2_CALENDAR_3_NUMBER_27(val)	    vBIT(val, 29, 3)
/* 0x04bb0 */	u64	cmc_l3_client_qcc_sqm_1;
#define	VXGE_HAL_CMC_L3_CLIENT_QCC_SQM_1_NUMBER(val)	    vBIT(val, 5, 3)
/* 0x04bb8 */	u64	cmc_l3_client_qcc_cqm;
#define	VXGE_HAL_CMC_L3_CLIENT_QCC_CQM_NUMBER(val)	    vBIT(val, 5, 3)
/* 0x04bc0 */	u64	cmc_l3_client_dam_1;
#define	VXGE_HAL_CMC_L3_CLIENT_DAM_1_NUMBER(val)	    vBIT(val, 5, 3)
/* 0x04bc8 */	u64	cmc_l3_client_h2l_1;
#define	VXGE_HAL_CMC_L3_CLIENT_H2L_1_NUMBER(val)	    vBIT(val, 5, 3)
/* 0x04bd0 */	u64	cmc_l3_client_stc_1;
#define	VXGE_HAL_CMC_L3_CLIENT_STC_1_NUMBER(val)	    vBIT(val, 5, 3)
/* 0x04bd8 */	u64	cmc_l3_client_xtmc_1;
#define	VXGE_HAL_CMC_L3_CLIENT_XTMC_1_NUMBER(val)	    vBIT(val, 5, 3)
/* 0x04be0 */	u64	cmc_wrr_l3_calendar_0;
#define	VXGE_HAL_CMC_WRR_L3_CALENDAR_0_NUMBER_0(val)	    vBIT(val, 5, 3)
#define	VXGE_HAL_CMC_WRR_L3_CALENDAR_0_NUMBER_1(val)	    vBIT(val, 13, 3)
#define	VXGE_HAL_CMC_WRR_L3_CALENDAR_0_NUMBER_2(val)	    vBIT(val, 21, 3)
#define	VXGE_HAL_CMC_WRR_L3_CALENDAR_0_NUMBER_3(val)	    vBIT(val, 29, 3)
#define	VXGE_HAL_CMC_WRR_L3_CALENDAR_0_NUMBER_4(val)	    vBIT(val, 37, 3)
#define	VXGE_HAL_CMC_WRR_L3_CALENDAR_0_NUMBER_5(val)	    vBIT(val, 45, 3)
#define	VXGE_HAL_CMC_WRR_L3_CALENDAR_0_NUMBER_6(val)	    vBIT(val, 53, 3)
#define	VXGE_HAL_CMC_WRR_L3_CALENDAR_0_NUMBER_7(val)	    vBIT(val, 61, 3)
/* 0x04be8 */	u64	cmc_wrr_l3_calendar_1;
#define	VXGE_HAL_CMC_WRR_L3_CALENDAR_1_NUMBER_8(val)	    vBIT(val, 5, 3)
#define	VXGE_HAL_CMC_WRR_L3_CALENDAR_1_NUMBER_9(val)	    vBIT(val, 13, 3)
#define	VXGE_HAL_CMC_WRR_L3_CALENDAR_1_NUMBER_10(val)	    vBIT(val, 21, 3)
#define	VXGE_HAL_CMC_WRR_L3_CALENDAR_1_NUMBER_11(val)	    vBIT(val, 29, 3)
#define	VXGE_HAL_CMC_WRR_L3_CALENDAR_1_NUMBER_12(val)	    vBIT(val, 37, 3)
#define	VXGE_HAL_CMC_WRR_L3_CALENDAR_1_NUMBER_13(val)	    vBIT(val, 45, 3)
#define	VXGE_HAL_CMC_WRR_L3_CALENDAR_1_NUMBER_14(val)	    vBIT(val, 53, 3)
#define	VXGE_HAL_CMC_WRR_L3_CALENDAR_1_NUMBER_15(val)	    vBIT(val, 61, 3)
/* 0x04bf0 */	u64	cmc_wrr_l3_calendar_2;
#define	VXGE_HAL_CMC_WRR_L3_CALENDAR_2_NUMBER_16(val)	    vBIT(val, 5, 3)
#define	VXGE_HAL_CMC_WRR_L3_CALENDAR_2_NUMBER_17(val)	    vBIT(val, 13, 3)
#define	VXGE_HAL_CMC_WRR_L3_CALENDAR_2_NUMBER_18(val)	    vBIT(val, 21, 3)
#define	VXGE_HAL_CMC_WRR_L3_CALENDAR_2_NUMBER_19(val)	    vBIT(val, 29, 3)
#define	VXGE_HAL_CMC_WRR_L3_CALENDAR_2_NUMBER_20(val)	    vBIT(val, 37, 3)
/* 0x04bf8 */	u64	cmc_user_doorbell_partition;
#define	VXGE_HAL_CMC_USER_DOORBELL_PARTITION_BASE(val)	    vBIT(val, 8, 24)
/* 0x04c00 */	u64	cmc_hit_record_partition_0;
#define	VXGE_HAL_CMC_HIT_RECORD_PARTITION_0_BASE(val)	    vBIT(val, 8, 24)
/* 0x04c08 */	u64	cmc_hit_record_partition_1;
#define	VXGE_HAL_CMC_HIT_RECORD_PARTITION_1_BASE(val)	    vBIT(val, 8, 24)
/* 0x04c10 */	u64	cmc_hit_record_partition_2;
#define	VXGE_HAL_CMC_HIT_RECORD_PARTITION_2_BASE(val)	    vBIT(val, 8, 24)
/* 0x04c18 */	u64	cmc_hit_record_partition_3;
#define	VXGE_HAL_CMC_HIT_RECORD_PARTITION_3_BASE(val)	    vBIT(val, 8, 24)
/* 0x04c20 */	u64	cmc_hit_record_partition_4;
#define	VXGE_HAL_CMC_HIT_RECORD_PARTITION_4_BASE(val)	    vBIT(val, 8, 24)
/* 0x04c28 */	u64	cmc_hit_record_partition_5;
#define	VXGE_HAL_CMC_HIT_RECORD_PARTITION_5_BASE(val)	    vBIT(val, 8, 24)
/* 0x04c30 */	u64	cmc_hit_record_partition_6;
#define	VXGE_HAL_CMC_HIT_RECORD_PARTITION_6_BASE(val)	    vBIT(val, 8, 24)
/* 0x04c38 */	u64	cmc_hit_record_partition_7;
#define	VXGE_HAL_CMC_HIT_RECORD_PARTITION_7_BASE(val)	    vBIT(val, 8, 24)
/* 0x04c40 */	u64	cmc_c_scr_record_partition_0;
#define	VXGE_HAL_CMC_C_SCR_RECORD_PARTITION_0_BASE(val)	    vBIT(val, 8, 24)
/* 0x04c48 */	u64	cmc_c_scr_record_partition_1;
#define	VXGE_HAL_CMC_C_SCR_RECORD_PARTITION_1_BASE(val)	    vBIT(val, 8, 24)
/* 0x04c50 */	u64	cmc_c_scr_record_partition_2;
#define	VXGE_HAL_CMC_C_SCR_RECORD_PARTITION_2_BASE(val)	    vBIT(val, 8, 24)
/* 0x04c58 */	u64	cmc_c_scr_record_partition_3;
#define	VXGE_HAL_CMC_C_SCR_RECORD_PARTITION_3_BASE(val)	    vBIT(val, 8, 24)
/* 0x04c60 */	u64	cmc_c_scr_record_partition_4;
#define	VXGE_HAL_CMC_C_SCR_RECORD_PARTITION_4_BASE(val)	    vBIT(val, 8, 24)
/* 0x04c68 */	u64	cmc_c_scr_record_partition_5;
#define	VXGE_HAL_CMC_C_SCR_RECORD_PARTITION_5_BASE(val)	    vBIT(val, 8, 24)
/* 0x04c70 */	u64	cmc_c_scr_record_partition_6;
#define	VXGE_HAL_CMC_C_SCR_RECORD_PARTITION_6_BASE(val)	    vBIT(val, 8, 24)
/* 0x04c78 */	u64	cmc_c_scr_record_partition_7;
#define	VXGE_HAL_CMC_C_SCR_RECORD_PARTITION_7_BASE(val)	    vBIT(val, 8, 24)
/* 0x04c80 */	u64	cmc_wqe_od_group_record_partition;
#define	VXGE_HAL_CMC_WQE_OD_GROUP_RECORD_PARTITION_BASE(val) vBIT(val, 8, 24)
/* 0x04c88 */	u64	cmc_ack_record_partition;
#define	VXGE_HAL_CMC_ACK_RECORD_PARTITION_BASE(val)	    vBIT(val, 8, 24)
/* 0x04c90 */	u64	cmc_lirr_record_partition;
#define	VXGE_HAL_CMC_LIRR_RECORD_PARTITION_BASE(val)	    vBIT(val, 8, 24)
/* 0x04c98 */	u64	cmc_rirr_record_partition;
#define	VXGE_HAL_CMC_RIRR_RECORD_PARTITION_BASE(val)	    vBIT(val, 8, 24)
/* 0x04ca0 */	u64	cmc_tce_record_partition;
#define	VXGE_HAL_CMC_TCE_RECORD_PARTITION_BASE(val)	    vBIT(val, 8, 24)
/* 0x04ca8 */	u64	cmc_hoq_record_partition;
#define	VXGE_HAL_CMC_HOQ_RECORD_PARTITION_BASE(val)	    vBIT(val, 8, 24)
/* 0x04cb0 */	u64	cmc_stag_vp_record_partition[17];
#define	VXGE_HAL_CMC_STAG_VP_RECORD_PARTITION_BASE(val)	    vBIT(val, 8, 24)
/* 0x04d38 */	u64	cmc_r_scr_record_partition;
#define	VXGE_HAL_CMC_R_SCR_RECORD_PARTITION_BASE(val)	    vBIT(val, 8, 24)
/* 0x04d40 */	u64	cmc_cqrq_context_record_partition;
#define	VXGE_HAL_CMC_CQRQ_CONTEXT_RECORD_PARTITION_BASE(val) vBIT(val, 8, 24)
/* 0x04d48 */	u64	cmc_cqe_group_record_partition;
#define	VXGE_HAL_CMC_CQE_GROUP_RECORD_PARTITION_BASE(val)   vBIT(val, 8, 24)
/* 0x04d50 */	u64	cmc_p_scr_record_partition;
#define	VXGE_HAL_CMC_P_SCR_RECORD_PARTITION_BASE(val)	    vBIT(val, 8, 24)
/* 0x04d58 */	u64	cmc_nce_context_record_partition;
#define	VXGE_HAL_CMC_NCE_CONTEXT_RECORD_PARTITION_BASE(val) vBIT(val, 8, 24)
/* 0x04d60 */	u64	cmc_bypass_queue_partition;
#define	VXGE_HAL_CMC_BYPASS_QUEUE_PARTITION_BASE(val)	    vBIT(val, 8, 24)
/* 0x04d68 */	u64	cmc_h_scr_record_partition;
#define	VXGE_HAL_CMC_H_SCR_RECORD_PARTITION_BASE(val)	    vBIT(val, 8, 24)
/* 0x04d70 */	u64	cmc_pbl_record_partition;
#define	VXGE_HAL_CMC_PBL_RECORD_PARTITION_BASE(val)	    vBIT(val, 8, 24)
/* 0x04d78 */	u64	cmc_lit_record_partition;
#define	VXGE_HAL_CMC_LIT_RECORD_PARTITION_BASE(val)	    vBIT(val, 8, 24)
/* 0x04d80 */	u64	cmc_srq_context_record_partition;
#define	VXGE_HAL_CMC_SRQ_CONTEXT_RECORD_PARTITION_BASE(val) vBIT(val, 8, 24)
/* 0x04d88 */	u64	cmc_p_scr_record;
#define	VXGE_HAL_CMC_P_SCR_RECORD_SIZE(val)		    vBIT(val, 2, 6)
/* 0x04d90 */	u64	cmc_device_select;
#define	VXGE_HAL_CMC_DEVICE_SELECT_CODE(val)		    vBIT(val, 5, 3)
/* 0x04d98 */	u64	g3if_fifo_dst_ecc;
#define	VXGE_HAL_G3IF_FIFO_DST_ECC_ENABLE(val)		    vBIT(val, 3, 5)
/* 0x04da0 */	u64	gxtmc_cfg;
#define	VXGE_HAL_GXTMC_CFG_CMC_PRI			    mBIT(7)
#define	VXGE_HAL_GXTMC_CFG_GPSYNC_WAIT_TOKEN_ENABLE	    mBIT(13)
#define	VXGE_HAL_GXTMC_CFG_GPSYNC_CNTDOWN_TIMER_ENABLE	    mBIT(14)
#define	VXGE_HAL_GXTMC_CFG_GPSYNC_SRC_NOTIFY_ENABLE	    mBIT(15)
#define	VXGE_HAL_GXTMC_CFG_GPSYNC_CNTDOWN_START_VALUE(val)  vBIT(val, 20, 4)
#define	VXGE_HAL_GXTMC_CFG_BDT_MEM_ECC_ENABLE_N		    mBIT(31)
	u8	unused04f00[0x04f00 - 0x04da8];

/* 0x04f00 */	u64	pcmg2_int_status;
#define	VXGE_HAL_PCMG2_INT_STATUS_PXTMC_ERR_PXTMC_INT	    mBIT(7)
#define	VXGE_HAL_PCMG2_INT_STATUS_CP_EXC_CP_XT_EXC_INT	    mBIT(15)
#define	VXGE_HAL_PCMG2_INT_STATUS_CP_ERR_CP_ERR_INT	    mBIT(23)
/* 0x04f08 */	u64	pcmg2_int_mask;
/* 0x04f10 */	u64	pxtmc_err_reg;
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_XT_PIF_SRAM_DB_ERR(val) vBIT(val, 0, 2)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_MPT_REQ_FIFO_ERR	    mBIT(2)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_MPT_PRSP_FIFO_ERR	    mBIT(3)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_MPT_WRSP_FIFO_ERR	    mBIT(4)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_UPT_REQ_FIFO_ERR	    mBIT(5)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_UPT_PRSP_FIFO_ERR	    mBIT(6)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_UPT_WRSP_FIFO_ERR	    mBIT(7)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_CPT_REQ_FIFO_ERR	    mBIT(8)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_CPT_PRSP_FIFO_ERR	    mBIT(9)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_CPT_WRSP_FIFO_ERR	    mBIT(10)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_REQ_FIFO_ERR	    mBIT(11)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_REQ_DATA_FIFO_ERR	    mBIT(12)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_WR_RSP_FIFO_ERR	    mBIT(13)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_RD_RSP_FIFO_ERR	    mBIT(14)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_MPT_REQ_SHADOW_ERR	    mBIT(15)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_MPT_RSP_SHADOW_ERR	    mBIT(16)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_UPT_REQ_SHADOW_ERR	    mBIT(17)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_UPT_RSP_SHADOW_ERR	    mBIT(18)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_CPT_REQ_SHADOW_ERR	    mBIT(19)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_CPT_RSP_SHADOW_ERR	    mBIT(20)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_XIL_SHADOW_ERR	    mBIT(21)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_ARB_SHADOW_ERR	    mBIT(22)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_RAM_SHADOW_ERR	    mBIT(23)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_CMW_SHADOW_ERR	    mBIT(24)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_CMR_SHADOW_ERR	    mBIT(25)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_MPT_REQ_FSM_ERR	    mBIT(26)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_MPT_RSP_FSM_ERR	    mBIT(27)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_UPT_REQ_FSM_ERR	    mBIT(28)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_UPT_RSP_FSM_ERR	    mBIT(29)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_CPT_REQ_FSM_ERR	    mBIT(30)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_CPT_RSP_FSM_ERR	    mBIT(31)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_XIL_FSM_ERR		    mBIT(32)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_ARB_FSM_ERR		    mBIT(33)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_CMW_FSM_ERR		    mBIT(34)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_CMR_FSM_ERR		    mBIT(35)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_MXP_RD_PROT_ERR	    mBIT(36)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_UXP_RD_PROT_ERR	    mBIT(37)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_CXP_RD_PROT_ERR	    mBIT(38)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_MXP_WR_PROT_ERR	    mBIT(39)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_UXP_WR_PROT_ERR	    mBIT(40)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_CXP_WR_PROT_ERR	    mBIT(41)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_MXP_INV_ADDR_ERR	    mBIT(42)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_UXP_INV_ADDR_ERR	    mBIT(43)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_CXP_INV_ADDR_ERR	    mBIT(44)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_MXP_RD_PROT_INFO_ERR    mBIT(45)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_UXP_RD_PROT_INFO_ERR    mBIT(46)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_CXP_RD_PROT_INFO_ERR    mBIT(47)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_MXP_WR_PROT_INFO_ERR    mBIT(48)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_UXP_WR_PROT_INFO_ERR    mBIT(49)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_CXP_WR_PROT_INFO_ERR    mBIT(50)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_MXP_INV_ADDR_INFO_ERR   mBIT(51)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_UXP_INV_ADDR_INFO_ERR   mBIT(52)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_CXP_INV_ADDR_INFO_ERR   mBIT(53)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_XT_PIF_SRAM_SG_ERR(val) vBIT(val, 54, 2)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_CP2BDT_DFIFO_PUSH_ERR   mBIT(56)
#define	VXGE_HAL_PXTMC_ERR_REG_XTMC_CP2BDT_RFIFO_PUSH_ERR   mBIT(57)
/* 0x04f18 */	u64	pxtmc_err_mask;
/* 0x04f20 */	u64	pxtmc_err_alarm;
/* 0x04f28 */	u64	cp_err_reg;
#define	VXGE_HAL_CP_ERR_REG_CP_CP_DCACHE_SG_ERR(val)	    vBIT(val, 0, 8)
#define	VXGE_HAL_CP_ERR_REG_CP_CP_ICACHE_SG_ERR(val)	    vBIT(val, 8, 2)
#define	VXGE_HAL_CP_ERR_REG_CP_CP_DTAG_SG_ERR		    mBIT(10)
#define	VXGE_HAL_CP_ERR_REG_CP_CP_ITAG_SG_ERR		    mBIT(11)
#define	VXGE_HAL_CP_ERR_REG_CP_CP_TRACE_SG_ERR		    mBIT(12)
#define	VXGE_HAL_CP_ERR_REG_CP_DMA2CP_SG_ERR		    mBIT(13)
#define	VXGE_HAL_CP_ERR_REG_CP_MP2CP_SG_ERR		    mBIT(14)
#define	VXGE_HAL_CP_ERR_REG_CP_QCC2CP_SG_ERR		    mBIT(15)
#define	VXGE_HAL_CP_ERR_REG_CP_STC2CP_SG_ERR(val)	    vBIT(val, 16, 2)
#define	VXGE_HAL_CP_ERR_REG_CP_CP_DCACHE_DB_ERR(val)	    vBIT(val, 24, 8)
#define	VXGE_HAL_CP_ERR_REG_CP_CP_ICACHE_DB_ERR(val)	    vBIT(val, 32, 2)
#define	VXGE_HAL_CP_ERR_REG_CP_CP_DTAG_DB_ERR		    mBIT(34)
#define	VXGE_HAL_CP_ERR_REG_CP_CP_ITAG_DB_ERR		    mBIT(35)
#define	VXGE_HAL_CP_ERR_REG_CP_CP_TRACE_DB_ERR		    mBIT(36)
#define	VXGE_HAL_CP_ERR_REG_CP_DMA2CP_DB_ERR		    mBIT(37)
#define	VXGE_HAL_CP_ERR_REG_CP_MP2CP_DB_ERR		    mBIT(38)
#define	VXGE_HAL_CP_ERR_REG_CP_QCC2CP_DB_ERR		    mBIT(39)
#define	VXGE_HAL_CP_ERR_REG_CP_STC2CP_DB_ERR(val)	    vBIT(val, 40, 2)
#define	VXGE_HAL_CP_ERR_REG_CP_H2L2CP_FIFO_ERR		    mBIT(48)
#define	VXGE_HAL_CP_ERR_REG_CP_STC2CP_FIFO_ERR		    mBIT(49)
#define	VXGE_HAL_CP_ERR_REG_CP_STE2CP_FIFO_ERR		    mBIT(50)
#define	VXGE_HAL_CP_ERR_REG_CP_TTE2CP_FIFO_ERR		    mBIT(51)
#define	VXGE_HAL_CP_ERR_REG_CP_SWIF2CP_FIFO_ERR		    mBIT(52)
#define	VXGE_HAL_CP_ERR_REG_CP_CP2DMA_FIFO_ERR		    mBIT(53)
#define	VXGE_HAL_CP_ERR_REG_CP_DAM2CP_FIFO_ERR		    mBIT(54)
#define	VXGE_HAL_CP_ERR_REG_CP_MP2CP_FIFO_ERR		    mBIT(55)
#define	VXGE_HAL_CP_ERR_REG_CP_QCC2CP_FIFO_ERR		    mBIT(56)
#define	VXGE_HAL_CP_ERR_REG_CP_DMA2CP_FIFO_ERR		    mBIT(57)
#define	VXGE_HAL_CP_ERR_REG_CP_CP_WAKE_FSM_INTEGRITY_ERR    mBIT(60)
#define	VXGE_HAL_CP_ERR_REG_CP_CP_PMON_FSM_INTEGRITY_ERR    mBIT(61)
#define	VXGE_HAL_CP_ERR_REG_CP_DMA_RD_SHADOW_ERR	    mBIT(62)
#define	VXGE_HAL_CP_ERR_REG_CP_PIFT_CREDIT_ERR		    mBIT(63)
/* 0x04f30 */	u64	cp_err_mask;
/* 0x04f38 */	u64	cp_err_alarm;
/* 0x04f40 */	u64	cp_xt_ctrl1;
#define	VXGE_HAL_CP_XT_CTRL1_CP_WAKEUP			    mBIT(47)
#define	VXGE_HAL_CP_XT_CTRL1_CP_RUNSTALL		    mBIT(55)
#define	VXGE_HAL_CP_XT_CTRL1_CP_BRESET			    mBIT(63)
/* 0x04f48 */	u64	cp_gen_cfg;
#define	VXGE_HAL_CP_GEN_CFG_MULT_DMA_RD_REQ_ENA		    mBIT(7)
#define	VXGE_HAL_CP_GEN_CFG_DMA_RD_PER_VPLANE_CHK_ENA	    mBIT(15)
#define	VXGE_HAL_CP_GEN_CFG_DMA_RD_XON_CHK_ENA		    mBIT(23)
#define	VXGE_HAL_CP_GEN_CFG_CAUSE_INT_IS_CRITICAL	    mBIT(31)
/* 0x04f50 */	u64	cp_exc_reg;
#define	VXGE_HAL_CP_EXC_REG_CP_CP_CAUSE_INFO_INT	    mBIT(47)
#define	VXGE_HAL_CP_EXC_REG_CP_CP_CAUSE_CRIT_INT	    mBIT(55)
#define	VXGE_HAL_CP_EXC_REG_CP_CP_SERR			    mBIT(63)
/* 0x04f58 */	u64	cp_exc_mask;
/* 0x04f60 */	u64	cp_exc_alarm;
/* 0x04f68 */	u64	cp_exc_cause;
#define	VXGE_HAL_CP_EXC_CAUSE_CP_CP_CAUSE(val)		    vBIT(val, 32, 32)
	u8	unused04fe8[0x04fe8 - 0x04f70];

/* 0x04fe8 */	u64	xtmc_img_ctrl0;
#define	VXGE_HAL_XTMC_IMG_CTRL0_LD_BANK_DEPTH(val)	    vBIT(val, 5, 3)
#define	VXGE_HAL_XTMC_IMG_CTRL0_ENABLE_GO		    mBIT(15)
#define	VXGE_HAL_XTMC_IMG_CTRL0_IMG_LD_COMPLETE		    mBIT(23)
#define	VXGE_HAL_XTMC_IMG_CTRL0_LAST_DATA		    mBIT(31)
#define	VXGE_HAL_XTMC_IMG_CTRL0_ADDR(val)		    vBIT(val, 40, 24)
/* 0x04ff0 */	u64	xtmc_img_ctrl1;
#define	VXGE_HAL_XTMC_IMG_CTRL1_DATA(val)		    vBIT(val, 0, 64)
/* 0x04ff8 */	u64	xtmc_img_ctrl2;
#define	VXGE_HAL_XTMC_IMG_CTRL2_XTMC_LD_BANK_AVAIL	    mBIT(63)
/* 0x05000 */	u64	xtmc_img_ctrl3;
#define	VXGE_HAL_XTMC_IMG_CTRL3_XTMC_ALL_DATA_WRITTEN	    mBIT(63)
/* 0x05008 */	u64	xtmc_img_ctrl4;
#define	VXGE_HAL_XTMC_IMG_CTRL4_GO			    mBIT(63)
/* 0x05010 */	u64	pxtmc_cfg0;
#define	VXGE_HAL_PXTMC_CFG0_XT_PIF_SRAM_ECC_ENABLE_N	    mBIT(3)
#define	VXGE_HAL_PXTMC_CFG0_XT_PIF_SRAM_PHASE_ENA	    mBIT(7)
#define	VXGE_HAL_PXTMC_CFG0_MXP_RD_PROT_ENA		    mBIT(11)
#define	VXGE_HAL_PXTMC_CFG0_MXP_WR_PROT_ENA		    mBIT(15)
#define	VXGE_HAL_PXTMC_CFG0_UXP_RD_PROT_ENA		    mBIT(19)
#define	VXGE_HAL_PXTMC_CFG0_UXP_WR_PROT_ENA		    mBIT(23)
#define	VXGE_HAL_PXTMC_CFG0_CXP_RD_PROT_ENA		    mBIT(27)
#define	VXGE_HAL_PXTMC_CFG0_CXP_WR_PROT_ENA		    mBIT(31)
#define	VXGE_HAL_PXTMC_CFG0_INVALID_ADDR_CHECK_ENA	    mBIT(39)
#define	VXGE_HAL_PXTMC_CFG0_SUPPRESS_RD_ON_ADDR_ERR	    mBIT(43)
#define	VXGE_HAL_PXTMC_CFG0_SUPPRESS_WR_ON_ADDR_ERR	    mBIT(47)
#define	VXGE_HAL_PXTMC_CFG0_ARB_DURING_4BYTE_WR_ENA	    mBIT(55)
/* 0x05018 */	u64	pxtmc_cfg1;
#define	VXGE_HAL_PXTMC_CFG1_MAX_NBR_MXP_EVENTS(val)	    vBIT(val, 6, 2)
#define	VXGE_HAL_PXTMC_CFG1_MAX_NBR_UXP_EVENTS(val)	    vBIT(val, 14, 2)
#define	VXGE_HAL_PXTMC_CFG1_MAX_NBR_CXP_EVENTS(val)	    vBIT(val, 22, 2)
#define	VXGE_HAL_PXTMC_CFG1_PGSYNC_WAIT_TOKEN_ENABLE	    mBIT(29)
#define	VXGE_HAL_PXTMC_CFG1_PGSYNC_CNTDOWN_TIMER_ENABLE	    mBIT(30)
#define	VXGE_HAL_PXTMC_CFG1_PGSYNC_SRC_NOTIFY_ENABLE	    mBIT(31)
#define	VXGE_HAL_PXTMC_CFG1_PGSYNC_CNTDOWN_START_VALUE(val) vBIT(val, 36, 4)
/* 0x05020 */	u64	xtmc_mem_cfg;
#define	VXGE_HAL_XTMC_MEM_CFG_CTXT_MEM_SPARSE_BASE(val)	    vBIT(val, 5, 3)
#define	VXGE_HAL_XTMC_MEM_CFG_CTXT_MEM_PACKED_BASE(val)	    vBIT(val, 13, 3)
#define	VXGE_HAL_XTMC_MEM_CFG_SHARED_SRAM_BASE(val)	    vBIT(val, 21, 3)
#define	VXGE_HAL_XTMC_MEM_CFG_CTXT_MEM_SIZE(val)	    vBIT(val, 29, 3)
#define	VXGE_HAL_XTMC_MEM_CFG_SRAM_SPARSE_BASE_ADDR(val)    vBIT(val, 32, 16)
#define	VXGE_HAL_XTMC_MEM_CFG_SRAM_PACKED_BASE_ADDR(val)    vBIT(val, 48, 16)
/* 0x05028 */	u64	xtmc_mem_bypass_cfg;
#define	VXGE_HAL_XTMC_MEM_BYPASS_CFG_CTXT_MEM_SPARSE_BASE(val) vBIT(val, 5, 3)
#define	VXGE_HAL_XTMC_MEM_BYPASS_CFG_CTXT_MEM_PACKED_BASE(val) vBIT(val, 13, 3)
#define	VXGE_HAL_XTMC_MEM_BYPASS_CFG_SHARED_SRAM_BASE(val)  vBIT(val, 21, 3)
/* 0x05030 */	u64	xtmc_cxp_region0;
#define	VXGE_HAL_XTMC_CXP_REGION0_START_ADDR(val)	    vBIT(val, 0, 32)
#define	VXGE_HAL_XTMC_CXP_REGION0_END_ADDR(val)		    vBIT(val, 32, 32)
/* 0x05038 */	u64	xtmc_mxp_region0;
#define	VXGE_HAL_XTMC_MXP_REGION0_START_ADDR(val)	    vBIT(val, 0, 32)
#define	VXGE_HAL_XTMC_MXP_REGION0_END_ADDR(val)		    vBIT(val, 32, 32)
/* 0x05040 */	u64	xtmc_uxp_region0;
#define	VXGE_HAL_XTMC_UXP_REGION0_START_ADDR(val)	    vBIT(val, 0, 32)
#define	VXGE_HAL_XTMC_UXP_REGION0_END_ADDR(val)		    vBIT(val, 32, 32)
/* 0x05048 */	u64	xtmc_cxp_region1;
#define	VXGE_HAL_XTMC_CXP_REGION1_START_ADDR(val)	    vBIT(val, 0, 32)
#define	VXGE_HAL_XTMC_CXP_REGION1_END_ADDR(val)		    vBIT(val, 32, 32)
/* 0x05050 */	u64	xtmc_mxp_region1;
#define	VXGE_HAL_XTMC_MXP_REGION1_START_ADDR(val)	    vBIT(val, 0, 32)
#define	VXGE_HAL_XTMC_MXP_REGION1_END_ADDR(val)		    vBIT(val, 32, 32)
/* 0x05058 */	u64	xtmc_uxp_region1;
#define	VXGE_HAL_XTMC_UXP_REGION1_START_ADDR(val)	    vBIT(val, 0, 32)
#define	VXGE_HAL_XTMC_UXP_REGION1_END_ADDR(val)		    vBIT(val, 32, 32)
/* 0x05060 */	u64	xtmc_cxp_region2;
#define	VXGE_HAL_XTMC_CXP_REGION2_START_ADDR(val)	    vBIT(val, 0, 32)
#define	VXGE_HAL_XTMC_CXP_REGION2_END_ADDR(val)		    vBIT(val, 32, 32)
/* 0x05068 */	u64	xtmc_mxp_region2;
#define	VXGE_HAL_XTMC_MXP_REGION2_START_ADDR(val)	    vBIT(val, 0, 32)
#define	VXGE_HAL_XTMC_MXP_REGION2_END_ADDR(val)		    vBIT(val, 32, 32)
/* 0x05070 */	u64	xtmc_uxp_region2;
#define	VXGE_HAL_XTMC_UXP_REGION2_START_ADDR(val)	    vBIT(val, 0, 32)
#define	VXGE_HAL_XTMC_UXP_REGION2_END_ADDR(val)		    vBIT(val, 32, 32)
	u8	unused05200[0x05200 - 0x05078];

/* 0x05200 */	u64	msg_int_status;
#define	VXGE_HAL_MSG_INT_STATUS_TIM_ERR_TIM_INT		    mBIT(7)
#define	VXGE_HAL_MSG_INT_STATUS_MSG_EXC_MSG_XT_EXC_INT	    mBIT(60)
#define	VXGE_HAL_MSG_INT_STATUS_MSG_ERR3_MSG_ERR3_INT	    mBIT(61)
#define	VXGE_HAL_MSG_INT_STATUS_MSG_ERR2_MSG_ERR2_INT	    mBIT(62)
#define	VXGE_HAL_MSG_INT_STATUS_MSG_ERR_MSG_ERR_INT	    mBIT(63)
/* 0x05208 */	u64	msg_int_mask;
/* 0x05210 */	u64	tim_err_reg;
#define	VXGE_HAL_TIM_ERR_REG_TIM_VBLS_SG_ERR		    mBIT(4)
#define	VXGE_HAL_TIM_ERR_REG_TIM_BMAP_PA_SG_ERR		    mBIT(5)
#define	VXGE_HAL_TIM_ERR_REG_TIM_BMAP_PB_SG_ERR		    mBIT(6)
#define	VXGE_HAL_TIM_ERR_REG_TIM_BMAP_MSG_SG_ERR	    mBIT(7)
#define	VXGE_HAL_TIM_ERR_REG_TIM_VBLS_DB_ERR		    mBIT(12)
#define	VXGE_HAL_TIM_ERR_REG_TIM_BMAP_PA_DB_ERR		    mBIT(13)
#define	VXGE_HAL_TIM_ERR_REG_TIM_BMAP_PB_DB_ERR		    mBIT(14)
#define	VXGE_HAL_TIM_ERR_REG_TIM_BMAP_MSG_DB_ERR	    mBIT(15)
#define	VXGE_HAL_TIM_ERR_REG_TIM_BMAP_MEM_CNTRL_SM_ERR	    mBIT(18)
#define	VXGE_HAL_TIM_ERR_REG_TIM_BMAP_MSG_MEM_CNTRL_SM_ERR mBIT(19)
#define	VXGE_HAL_TIM_ERR_REG_TIM_MPIF_PCIWR_ERR		    mBIT(20)
#define	VXGE_HAL_TIM_ERR_REG_TIM_ROCRC_BMAP_UPDT_FIFO_ERR   mBIT(22)
#define	VXGE_HAL_TIM_ERR_REG_TIM_CREATE_BMAPMSG_FIFO_ERR    mBIT(23)
#define	VXGE_HAL_TIM_ERR_REG_TIM_ROCRCIF_MISMATCH	    mBIT(46)
#define	VXGE_HAL_TIM_ERR_REG_TIM_BMAP_MAPPING_VP_ERR(n)	    mBIT(n)
/* 0x05218 */	u64	tim_err_mask;
/* 0x05220 */	u64	tim_err_alarm;
/* 0x05228 */	u64	msg_err_reg;
#define	VXGE_HAL_MSG_ERR_REG_UP_UXP_WAKE_FSM_INTEGRITY_ERR  mBIT(0)
#define	VXGE_HAL_MSG_ERR_REG_MP_MXP_WAKE_FSM_INTEGRITY_ERR  mBIT(1)
#define	VXGE_HAL_MSG_ERR_REG_MSG_QUE_DMQ_DMA_READ_CMD_FSM_INTEGRITY_ERR mBIT(2)
#define	VXGE_HAL_MSG_ERR_REG_MSG_QUE_DMQ_DMA_RESP_FSM_INTEGRITY_ERR	mBIT(3)
#define	VXGE_HAL_MSG_ERR_REG_MSG_QUE_DMQ_OWN_FSM_INTEGRITY_ERR	mBIT(4)
#define	VXGE_HAL_MSG_ERR_REG_MSG_QUE_PDA_ACC_FSM_INTEGRITY_ERR	mBIT(5)
#define	VXGE_HAL_MSG_ERR_REG_MP_MXP_PMON_FSM_INTEGRITY_ERR  mBIT(6)
#define	VXGE_HAL_MSG_ERR_REG_UP_UXP_PMON_FSM_INTEGRITY_ERR  mBIT(7)
#define	VXGE_HAL_MSG_ERR_REG_UP_UXP_DTAG_SG_ERR		    mBIT(8)
#define	VXGE_HAL_MSG_ERR_REG_UP_UXP_ITAG_SG_ERR		    mBIT(10)
#define	VXGE_HAL_MSG_ERR_REG_MP_MXP_DTAG_SG_ERR		    mBIT(12)
#define	VXGE_HAL_MSG_ERR_REG_MP_MXP_ITAG_SG_ERR		    mBIT(14)
#define	VXGE_HAL_MSG_ERR_REG_UP_UXP_TRACE_SG_ERR	    mBIT(16)
#define	VXGE_HAL_MSG_ERR_REG_MP_MXP_TRACE_SG_ERR	    mBIT(17)
#define	VXGE_HAL_MSG_ERR_REG_MSG_QUE_CMG2MSG_SG_ERR	    mBIT(18)
#define	VXGE_HAL_MSG_ERR_REG_MSG_QUE_TXPE2MSG_SG_ERR	    mBIT(19)
#define	VXGE_HAL_MSG_ERR_REG_MSG_QUE_RXPE2MSG_SG_ERR	    mBIT(20)
#define	VXGE_HAL_MSG_ERR_REG_MSG_QUE_RPE2MSG_SG_ERR	    mBIT(21)
#define	VXGE_HAL_MSG_ERR_REG_MSG_QUE_UMQ_SG_ERR		    mBIT(26)
#define	VXGE_HAL_MSG_ERR_REG_MSG_QUE_BWR_PF_SG_ERR	    mBIT(27)
#define	VXGE_HAL_MSG_ERR_REG_MSG_QUE_DMQ_ECC_SG_ERR	    mBIT(29)
#define	VXGE_HAL_MSG_ERR_REG_MSG_QUE_DMA_RESP_ECC_SG_ERR    mBIT(31)
#define	VXGE_HAL_MSG_ERR_REG_MSG_XFMDQRY_FSM_INTEGRITY_ERR  mBIT(33)
#define	VXGE_HAL_MSG_ERR_REG_MSG_FRMQRY_FSM_INTEGRITY_ERR   mBIT(34)
#define	VXGE_HAL_MSG_ERR_REG_MSG_QUE_UMQ_WRITE_FSM_INTEGRITY_ERR mBIT(35)
#define	VXGE_HAL_MSG_ERR_REG_MSG_QUE_UMQ_BWR_PF_FSM_INTEGRITY_ERR mBIT(36)
#define	VXGE_HAL_MSG_ERR_REG_MSG_QUE_REG_RESP_FIFO_ERR	    mBIT(38)
#define	VXGE_HAL_MSG_ERR_REG_UP_UXP_DTAG_DB_ERR		    mBIT(39)
#define	VXGE_HAL_MSG_ERR_REG_UP_UXP_ITAG_DB_ERR		    mBIT(41)
#define	VXGE_HAL_MSG_ERR_REG_MP_MXP_DTAG_DB_ERR		    mBIT(43)
#define	VXGE_HAL_MSG_ERR_REG_MP_MXP_ITAG_DB_ERR		    mBIT(45)
#define	VXGE_HAL_MSG_ERR_REG_UP_UXP_TRACE_DB_ERR	    mBIT(47)
#define	VXGE_HAL_MSG_ERR_REG_MP_MXP_TRACE_DB_ERR	    mBIT(48)
#define	VXGE_HAL_MSG_ERR_REG_MSG_QUE_CMG2MSG_DB_ERR	    mBIT(49)
#define	VXGE_HAL_MSG_ERR_REG_MSG_QUE_TXPE2MSG_DB_ERR	    mBIT(50)
#define	VXGE_HAL_MSG_ERR_REG_MSG_QUE_RXPE2MSG_DB_ERR	    mBIT(51)
#define	VXGE_HAL_MSG_ERR_REG_MSG_QUE_RPE2MSG_DB_ERR	    mBIT(52)
#define	VXGE_HAL_MSG_ERR_REG_MSG_QUE_REG_READ_FIFO_ERR	    mBIT(53)
#define	VXGE_HAL_MSG_ERR_REG_MSG_QUE_MXP2UXP_FIFO_ERR	    mBIT(54)
#define	VXGE_HAL_MSG_ERR_REG_MSG_QUE_KDFC_SIF_FIFO_ERR	    mBIT(55)
#define	VXGE_HAL_MSG_ERR_REG_MSG_QUE_CXP2SWIF_FIFO_ERR	    mBIT(56)
#define	VXGE_HAL_MSG_ERR_REG_MSG_QUE_UMQ_DB_ERR		    mBIT(57)
#define	VXGE_HAL_MSG_ERR_REG_MSG_QUE_BWR_PF_DB_ERR	    mBIT(58)
#define	VXGE_HAL_MSG_ERR_REG_MSG_QUE_BWR_SIF_FIFO_ERR	    mBIT(59)
#define	VXGE_HAL_MSG_ERR_REG_MSG_QUE_DMQ_ECC_DB_ERR	    mBIT(60)
#define	VXGE_HAL_MSG_ERR_REG_MSG_QUE_DMA_READ_FIFO_ERR	    mBIT(61)
#define	VXGE_HAL_MSG_ERR_REG_MSG_QUE_DMA_RESP_ECC_DB_ERR    mBIT(62)
#define	VXGE_HAL_MSG_ERR_REG_MSG_QUE_UXP2MXP_FIFO_ERR	    mBIT(63)
/* 0x05230 */	u64	msg_err_mask;
/* 0x05238 */	u64	msg_err_alarm;
/* 0x05240 */	u64	msg_xt_ctrl;
#define	VXGE_HAL_MSG_XT_CTRL_MXP_CAUSE_INT_IS_CRITICAL	    mBIT(35)
#define	VXGE_HAL_MSG_XT_CTRL_UXP_CAUSE_INT_IS_CRITICAL	    mBIT(39)
#define	VXGE_HAL_MSG_XT_CTRL_MXP_WAKEUP			    mBIT(46)
#define	VXGE_HAL_MSG_XT_CTRL_UXP_WAKEUP			    mBIT(47)
#define	VXGE_HAL_MSG_XT_CTRL_MXP_RUNSTALL		    mBIT(54)
#define	VXGE_HAL_MSG_XT_CTRL_UXP_RUNSTALL		    mBIT(55)
#define	VXGE_HAL_MSG_XT_CTRL_MXP_BRESET			    mBIT(62)
#define	VXGE_HAL_MSG_XT_CTRL_UXP_BRESET			    mBIT(63)
	u8	unused052a8[0x052a8 - 0x05248];

/* 0x052a8 */	u64	msg_dispatch;
#define	VXGE_HAL_MSG_DISPATCH_MESS_TYPE_ENABLE		    mBIT(55)
#define	VXGE_HAL_MSG_DISPATCH_VPATH_CUTOFF(val)		    vBIT(val, 59, 5)
	u8	unused05340[0x05340 - 0x052b0];

/* 0x05340 */	u64	msg_exc_reg;
#define	VXGE_HAL_MSG_EXC_REG_MP_MXP_CAUSE_INFO_INT	    mBIT(50)
#define	VXGE_HAL_MSG_EXC_REG_MP_MXP_CAUSE_CRIT_INT	    mBIT(51)
#define	VXGE_HAL_MSG_EXC_REG_UP_UXP_CAUSE_INFO_INT	    mBIT(54)
#define	VXGE_HAL_MSG_EXC_REG_UP_UXP_CAUSE_CRIT_INT	    mBIT(55)
#define	VXGE_HAL_MSG_EXC_REG_MP_MXP_SERR		    mBIT(62)
#define	VXGE_HAL_MSG_EXC_REG_UP_UXP_SERR		    mBIT(63)
/* 0x05348 */	u64	msg_exc_mask;
/* 0x05350 */	u64	msg_exc_alarm;
/* 0x05358 */	u64	msg_exc_cause;
#define	VXGE_HAL_MSG_EXC_CAUSE_MP_MXP(val)		    vBIT(val, 0, 32)
#define	VXGE_HAL_MSG_EXC_CAUSE_UP_UXP(val)		    vBIT(val, 32, 32)
	u8	unused05368[0x05368 - 0x05360];

/* 0x05368 */	u64	msg_direct_pic;
#define	VXGE_HAL_MSG_DIRECT_PIC_PIPELINE_EN		    mBIT(55)
#define	VXGE_HAL_MSG_DIRECT_PIC_UMQ_WRITE_ENABLE	    mBIT(56)
#define	VXGE_HAL_MSG_DIRECT_PIC_UMQ_VPA(val)		    vBIT(val, 59, 5)
/* 0x05370 */	u64	umq_ir_test_vpa;
#define	VXGE_HAL_UMQ_IR_TEST_VPA_NUMBER(val)		    vBIT(val, 0, 5)
/* 0x05378 */	u64	umq_ir_test_byte;
#define	VXGE_HAL_UMQ_IR_TEST_BYTE_VALUE_START(val)	    vBIT(val, 0, 32)
/* 0x05380 */	u64	msg_err2_reg;
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_CMG2MSG_DISPATCH_FSM_INTEGRITY_ERR mBIT(0)
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_DMQ_DISPATCH_FSM_INTEGRITY_ERR	mBIT(1)
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_SWIF_DISPATCH_FSM_INTEGRITY_ERR	mBIT(2)
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_PIC_WRITE_FSM_INTEGRITY_ERR	mBIT(3)
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_SWIFREG_FSM_INTEGRITY_ERR		mBIT(4)
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_TIM_WRITE_FSM_INTEGRITY_ERR	mBIT(5)
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_UMQ_TA_FSM_INTEGRITY_ERR	mBIT(6)
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_TXPE_TA_FSM_INTEGRITY_ERR	mBIT(7)
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_RXPE_TA_FSM_INTEGRITY_ERR	mBIT(8)
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_SWIF_TA_FSM_INTEGRITY_ERR	mBIT(9)
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_DMA_TA_FSM_INTEGRITY_ERR	mBIT(10)
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_CP_TA_FSM_INTEGRITY_ERR	mBIT(11)
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA16_FSM_INTEGRITY_ERR\
							    mBIT(12)
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA15_FSM_INTEGRITY_ERR\
							    mBIT(13)
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA14_FSM_INTEGRITY_ERR\
							    mBIT(14)
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA13_FSM_INTEGRITY_ERR\
							    mBIT(15)
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA12_FSM_INTEGRITY_ERR\
							    mBIT(16)
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA11_FSM_INTEGRITY_ERR\
							    mBIT(17)
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA10_FSM_INTEGRITY_ERR\
							    mBIT(18)
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA9_FSM_INTEGRITY_ERR	mBIT(19)
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA8_FSM_INTEGRITY_ERR	mBIT(20)
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA7_FSM_INTEGRITY_ERR	mBIT(21)
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA6_FSM_INTEGRITY_ERR	mBIT(22)
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA5_FSM_INTEGRITY_ERR	mBIT(23)
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA4_FSM_INTEGRITY_ERR	mBIT(24)
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA3_FSM_INTEGRITY_ERR	mBIT(25)
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA2_FSM_INTEGRITY_ERR	mBIT(26)
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA1_FSM_INTEGRITY_ERR	mBIT(27)
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA0_FSM_INTEGRITY_ERR	mBIT(28)
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_FBMC_OWN_FSM_INTEGRITY_ERR	mBIT(29)
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_TXPE2MSG_DISPATCH_FSM_INTEGRITY_ERR\
							    mBIT(30)
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_RXPE2MSG_DISPATCH_FSM_INTEGRITY_ERR\
							    mBIT(31)
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_RPE2MSG_DISPATCH_FSM_INTEGRITY_ERR\
							    mBIT(32)
#define	VXGE_HAL_MSG_ERR2_REG_MP_MP_PIFT_IF_CREDIT_CNT_ERR  mBIT(33)
#define	VXGE_HAL_MSG_ERR2_REG_UP_UP_PIFT_IF_CREDIT_CNT_ERR  mBIT(34)
#define	VXGE_HAL_MSG_ERR2_REG_MSG_QUE_UMQ2PIC_CMD_FIFO_ERR  mBIT(62)
#define	VXGE_HAL_MSG_ERR2_REG_TIM_TIM2MSG_CMD_FIFO_ERR	    mBIT(63)
/* 0x05388 */	u64	msg_err2_mask;
/* 0x05390 */	u64	msg_err2_alarm;
/* 0x05398 */	u64	msg_err3_reg;
#define	VXGE_HAL_MSG_ERR3_REG_UP_UXP_DCACHE_SG_ERR0	    mBIT(0)
#define	VXGE_HAL_MSG_ERR3_REG_UP_UXP_DCACHE_SG_ERR1	    mBIT(1)
#define	VXGE_HAL_MSG_ERR3_REG_UP_UXP_DCACHE_SG_ERR2	    mBIT(2)
#define	VXGE_HAL_MSG_ERR3_REG_UP_UXP_DCACHE_SG_ERR3	    mBIT(3)
#define	VXGE_HAL_MSG_ERR3_REG_UP_UXP_DCACHE_SG_ERR4	    mBIT(4)
#define	VXGE_HAL_MSG_ERR3_REG_UP_UXP_DCACHE_SG_ERR5	    mBIT(5)
#define	VXGE_HAL_MSG_ERR3_REG_UP_UXP_DCACHE_SG_ERR6	    mBIT(6)
#define	VXGE_HAL_MSG_ERR3_REG_UP_UXP_DCACHE_SG_ERR7	    mBIT(7)
#define	VXGE_HAL_MSG_ERR3_REG_UP_UXP_ICACHE_SG_ERR0	    mBIT(8)
#define	VXGE_HAL_MSG_ERR3_REG_UP_UXP_ICACHE_SG_ERR1	    mBIT(9)
#define	VXGE_HAL_MSG_ERR3_REG_MP_MXP_DCACHE_SG_ERR0	    mBIT(16)
#define	VXGE_HAL_MSG_ERR3_REG_MP_MXP_DCACHE_SG_ERR1	    mBIT(17)
#define	VXGE_HAL_MSG_ERR3_REG_MP_MXP_DCACHE_SG_ERR2	    mBIT(18)
#define	VXGE_HAL_MSG_ERR3_REG_MP_MXP_DCACHE_SG_ERR3	    mBIT(19)
#define	VXGE_HAL_MSG_ERR3_REG_MP_MXP_DCACHE_SG_ERR4	    mBIT(20)
#define	VXGE_HAL_MSG_ERR3_REG_MP_MXP_DCACHE_SG_ERR5	    mBIT(21)
#define	VXGE_HAL_MSG_ERR3_REG_MP_MXP_DCACHE_SG_ERR6	    mBIT(22)
#define	VXGE_HAL_MSG_ERR3_REG_MP_MXP_DCACHE_SG_ERR7	    mBIT(23)
#define	VXGE_HAL_MSG_ERR3_REG_MP_MXP_ICACHE_SG_ERR0	    mBIT(24)
#define	VXGE_HAL_MSG_ERR3_REG_MP_MXP_ICACHE_SG_ERR1	    mBIT(25)
#define	VXGE_HAL_MSG_ERR3_REG_UP_UXP_DCACHE_DB_ERR0	    mBIT(32)
#define	VXGE_HAL_MSG_ERR3_REG_UP_UXP_DCACHE_DB_ERR1	    mBIT(33)
#define	VXGE_HAL_MSG_ERR3_REG_UP_UXP_DCACHE_DB_ERR2	    mBIT(34)
#define	VXGE_HAL_MSG_ERR3_REG_UP_UXP_DCACHE_DB_ERR3	    mBIT(35)
#define	VXGE_HAL_MSG_ERR3_REG_UP_UXP_DCACHE_DB_ERR4	    mBIT(36)
#define	VXGE_HAL_MSG_ERR3_REG_UP_UXP_DCACHE_DB_ERR5	    mBIT(37)
#define	VXGE_HAL_MSG_ERR3_REG_UP_UXP_DCACHE_DB_ERR6	    mBIT(38)
#define	VXGE_HAL_MSG_ERR3_REG_UP_UXP_DCACHE_DB_ERR7	    mBIT(39)
#define	VXGE_HAL_MSG_ERR3_REG_UP_UXP_ICACHE_DB_ERR0	    mBIT(40)
#define	VXGE_HAL_MSG_ERR3_REG_UP_UXP_ICACHE_DB_ERR1	    mBIT(41)
#define	VXGE_HAL_MSG_ERR3_REG_MP_MXP_DCACHE_DB_ERR0	    mBIT(48)
#define	VXGE_HAL_MSG_ERR3_REG_MP_MXP_DCACHE_DB_ERR1	    mBIT(49)
#define	VXGE_HAL_MSG_ERR3_REG_MP_MXP_DCACHE_DB_ERR2	    mBIT(50)
#define	VXGE_HAL_MSG_ERR3_REG_MP_MXP_DCACHE_DB_ERR3	    mBIT(51)
#define	VXGE_HAL_MSG_ERR3_REG_MP_MXP_DCACHE_DB_ERR4	    mBIT(52)
#define	VXGE_HAL_MSG_ERR3_REG_MP_MXP_DCACHE_DB_ERR5	    mBIT(53)
#define	VXGE_HAL_MSG_ERR3_REG_MP_MXP_DCACHE_DB_ERR6	    mBIT(54)
#define	VXGE_HAL_MSG_ERR3_REG_MP_MXP_DCACHE_DB_ERR7	    mBIT(55)
#define	VXGE_HAL_MSG_ERR3_REG_MP_MXP_ICACHE_DB_ERR0	    mBIT(56)
#define	VXGE_HAL_MSG_ERR3_REG_MP_MXP_ICACHE_DB_ERR1	    mBIT(57)
/* 0x053a0 */	u64	msg_err3_mask;
/* 0x053a8 */	u64	msg_err3_alarm;
/* 0x053b0 */	u64	umq_ir_test_byte_notify;
#define	VXGE_HAL_UMQ_IR_TEST_BYTE_NOTIFY_PULSE		    mBIT(3)
/* 0x053b8 */	u64	msg_bp_ctrl;
#define	VXGE_HAL_MSG_BP_CTRL_RD_XON_EN			    mBIT(7)
#define	VXGE_HAL_MSG_BP_CTRL_WR_XON_E			    mBIT(15)
#define	VXGE_HAL_MSG_BP_CTRL_ROCRC_BYP_EN		    mBIT(23)
/* 0x053c0 */	u64	umq_bwr_pfch_init[17];
#define	VXGE_HAL_UMQ_BWR_PFCH_INIT_NUMBER(val)		    vBIT(val, 0, 8)
/* 0x05448 */	u64	umq_bwr_pfch_init_notify[17];
#define	VXGE_HAL_UMQ_BWR_PFCH_INIT_NOTIFY_PULSE		    mBIT(3)
/* 0x054d0 */	u64	umq_bwr_eol;
#define	VXGE_HAL_UMQ_BWR_EOL_POLL_LATENCY(val)		    vBIT(val, 32, 32)
/* 0x054d8 */	u64	umq_bwr_eol_latency_notify;
#define	VXGE_HAL_UMQ_BWR_EOL_LATENCY_NOTIFY_PULSE	    mBIT(3)
	u8	unused05600[0x05600 - 0x054e0];

/* 0x05600 */	u64	fau_gen_err_reg;
#define	VXGE_HAL_FAU_GEN_ERR_REG_FMPF_PORT0_PERMANENT_STOP  mBIT(3)
#define	VXGE_HAL_FAU_GEN_ERR_REG_FMPF_PORT1_PERMANENT_STOP  mBIT(7)
#define	VXGE_HAL_FAU_GEN_ERR_REG_FMPF_PORT2_PERMANENT_STOP  mBIT(11)
#define	VXGE_HAL_FAU_GEN_ERR_REG_FALR_AUTO_LRO_NOTIF mBIT(15)
/* 0x05608 */	u64	fau_gen_err_mask;
/* 0x05610 */	u64	fau_gen_err_alarm;
/* 0x05618 */	u64	fau_ecc_err_reg;
#define	VXGE_HAL_FAU_ECC_ERR_REG_FAU_PORT0_FAU_MAC2F_N_SG_ERR mBIT(0)
#define	VXGE_HAL_FAU_ECC_ERR_REG_FAU_PORT0_FAU_MAC2F_N_DB_ERR mBIT(1)
#define	VXGE_HAL_FAU_ECC_ERR_REG_FAU_PORT0_FAU_MAC2F_W_SG_ERR(val)\
							    vBIT(val, 2, 2)
#define	VXGE_HAL_FAU_ECC_ERR_REG_FAU_PORT0_FAU_MAC2F_W_DB_ERR(val)\
							    vBIT(val, 4, 2)
#define	VXGE_HAL_FAU_ECC_ERR_REG_FAU_PORT1_FAU_MAC2F_N_SG_ERR mBIT(6)
#define	VXGE_HAL_FAU_ECC_ERR_REG_FAU_PORT1_FAU_MAC2F_N_DB_ERR mBIT(7)
#define	VXGE_HAL_FAU_ECC_ERR_REG_FAU_PORT1_FAU_MAC2F_W_SG_ERR(val)\
							    vBIT(val, 8, 2)
#define	VXGE_HAL_FAU_ECC_ERR_REG_FAU_PORT1_FAU_MAC2F_W_DB_ERR(val)\
							    vBIT(val, 10, 2)
#define	VXGE_HAL_FAU_ECC_ERR_REG_FAU_PORT2_FAU_MAC2F_N_SG_ERR mBIT(12)
#define	VXGE_HAL_FAU_ECC_ERR_REG_FAU_PORT2_FAU_MAC2F_N_DB_ERR mBIT(13)
#define	VXGE_HAL_FAU_ECC_ERR_REG_FAU_PORT2_FAU_MAC2F_W_SG_ERR(val)\
							    vBIT(val, 14, 2)
#define	VXGE_HAL_FAU_ECC_ERR_REG_FAU_PORT2_FAU_MAC2F_W_DB_ERR(val)\
							    vBIT(val, 16, 2)
#define	VXGE_HAL_FAU_ECC_ERR_REG_FAU_FAU_XFMD_INS_SG_ERR(val) vBIT(val, 18, 2)
#define	VXGE_HAL_FAU_ECC_ERR_REG_FAU_FAU_XFMD_INS_DB_ERR(val) vBIT(val, 20, 2)
#define	VXGE_HAL_FAU_ECC_ERR_REG_FAUJ_FAU_FSM_ERR	    mBIT(31)
/* 0x05620 */	u64	fau_ecc_err_mask;
/* 0x05628 */	u64	fau_ecc_err_alarm;
	u8	unused05648[0x05648 - 0x05630];

/* 0x05648 */	u64	fau_global_cfg;
#define	VXGE_HAL_FAU_GLOBAL_CFG_ARB_ALG(val)		    vBIT(val, 2, 2)
/* 0x05650 */	u64	rx_datapath_util;
#define	VXGE_HAL_RX_DATAPATH_UTIL_FAU_RX_UTILIZATION(val)   vBIT(val, 7, 9)
#define	VXGE_HAL_RX_DATAPATH_UTIL_RX_UTIL_CFG(val)	    vBIT(val, 16, 4)
#define	VXGE_HAL_RX_DATAPATH_UTIL_FAU_RX_FRAC_UTIL(val)	    vBIT(val, 20, 4)
#define	VXGE_HAL_RX_DATAPATH_UTIL_RX_PKT_WEIGHT(val)	    vBIT(val, 24, 4)
/* 0x05658 */	u64	fau_pa_cfg;
#define	VXGE_HAL_FAU_PA_CFG_REPL_L4_COMP_CSUM		    mBIT(3)
#define	VXGE_HAL_FAU_PA_CFG_REPL_L3_INCL_CF		    mBIT(7)
#define	VXGE_HAL_FAU_PA_CFG_REPL_L3_COMP_CSUM		    mBIT(11)
	u8	unused05668[0x05668 - 0x05660];

/* 0x05668 */	u64	dbg_stats_fau_rx_path;
#define	VXGE_HAL_DBG_STATS_FAU_RX_PATH_RX_PERMITTED_FRMS(val) vBIT(val, 32, 32)
/* 0x05670 */	u64	fau_auto_lro_control;
#define	VXGE_HAL_FAU_AUTO_LRO_CONTROL_OPERATION_TYPE	    mBIT(7)
#define	VXGE_HAL_FAU_AUTO_LRO_CONTROL_FRAME_COUNT(val)	    vBIT(val, 8, 24)
#define	VXGE_HAL_FAU_AUTO_LRO_CONTROL_TIMER_VALUE(val)	    vBIT(val, 32, 32)
/* 0x05678 */	u64	fau_auto_lro_data_0;
#define	VXGE_HAL_FAU_AUTO_LRO_DATA_0_SOURCE_VPATH(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_FAU_AUTO_LRO_DATA_0_HAS_VLAN		    mBIT(14)
#define	VXGE_HAL_FAU_AUTO_LRO_DATA_0_IS_IPV6		    mBIT(15)
#define	VXGE_HAL_FAU_AUTO_LRO_DATA_0_VLAN_VID(val)	    vBIT(val, 20, 12)
#define	VXGE_HAL_FAU_AUTO_LRO_DATA_0_TCP_DEST_PORT(val)	    vBIT(val, 32, 16)
#define	VXGE_HAL_FAU_AUTO_LRO_DATA_0_TCP_SOURCE_PORT(val)   vBIT(val, 48, 16)
/* 0x05680 */	u64	fau_auto_lro_data_1;
#define	VXGE_HAL_FAU_AUTO_LRO_DATA_1_IP_SOURCE_ADDR_0(val)  vBIT(val, 0, 64)
/* 0x05688 */	u64	fau_auto_lro_data_2;
#define	VXGE_HAL_FAU_AUTO_LRO_DATA_2_IP_SOURCE_ADDR_1(val)  vBIT(val, 0, 64)
/* 0x05690 */	u64	fau_auto_lro_data_3;
#define	VXGE_HAL_FAU_AUTO_LRO_DATA_3_IP_DEST_ADDR_0(val)    vBIT(val, 0, 64)
/* 0x05698 */	u64	fau_auto_lro_data_4;
#define	VXGE_HAL_FAU_AUTO_LRO_DATA_4_IP_DEST_ADDR_1(val)    vBIT(val, 0, 64)
	u8	unused056c0[0x056c0 - 0x056a0];

/* 0x056c0 */	u64	fau_lag_cfg;
#define	VXGE_HAL_FAU_LAG_CFG_COLL_ALG(val)		    vBIT(val, 2, 2)
#define	VXGE_HAL_FAU_LAG_CFG_INCR_RX_AGGR_STATS		    mBIT(7)
	u8	unused05700[0x05700 - 0x056c8];

/* 0x05700 */	u64	fau_mpa_cfg;
#define	VXGE_HAL_FAU_MPA_CFG_CRC_CHK_EN	mBIT(3)
#define	VXGE_HAL_FAU_MPA_CFG_MRK_LEN_CHK_EN		    mBIT(7)
	u8	unused057a0[0x057a0 - 0x05708];

/* 0x057a0 */	u64	xmac_rx_xgmii_capture_data_port[3];
#define	VXGE_HAL_XMAC_RX_XGMII_CAPTURE_DATA_PORT_COL_INDX(val) vBIT(val, 0, 12)
#define	VXGE_HAL_XMAC_RX_XGMII_CAPTURE_DATA_PORT_FAUJ_FLAG(val) vBIT(val, 26, 2)
#define	VXGE_HAL_XMAC_RX_XGMII_CAPTURE_DATA_PORT_FAUJ_RXC(val) vBIT(val, 28, 4)
#define	VXGE_HAL_XMAC_RX_XGMII_CAPTURE_DATA_PORT_FAUJ_RXD(val) vBIT(val, 32, 32)
	u8	unused05800[0x05800 - 0x057b8];

/* 0x05800 */	u64	tpa_int_status;
#define	VXGE_HAL_TPA_INT_STATUS_ORP_ERR_ORP_INT		    mBIT(15)
#define	VXGE_HAL_TPA_INT_STATUS_PTM_ALARM_PTM_INT	    mBIT(23)
#define	VXGE_HAL_TPA_INT_STATUS_TPA_ERROR_TPA_INT	    mBIT(31)
/* 0x05808 */	u64	tpa_int_mask;
/* 0x05810 */	u64	orp_err_reg;
#define	VXGE_HAL_ORP_ERR_REG_ORP_FIFO_SG_ERR		    mBIT(3)
#define	VXGE_HAL_ORP_ERR_REG_ORP_FIFO_DB_ERR		    mBIT(7)
#define	VXGE_HAL_ORP_ERR_REG_ORP_XFMD_FIFO_UFLOW_ERR	    mBIT(11)
#define	VXGE_HAL_ORP_ERR_REG_ORP_FRM_FIFO_UFLOW_ERR	    mBIT(15)
#define	VXGE_HAL_ORP_ERR_REG_ORP_XFMD_RCV_FSM_ERR	    mBIT(19)
#define	VXGE_HAL_ORP_ERR_REG_ORP_OUTREAD_FSM_ERR	    mBIT(23)
#define	VXGE_HAL_ORP_ERR_REG_ORP_OUTQEM_FSM_ERR		    mBIT(27)
#define	VXGE_HAL_ORP_ERR_REG_ORP_XFMD_RCV_SHADOW_ERR	    mBIT(31)
#define	VXGE_HAL_ORP_ERR_REG_ORP_OUTREAD_SHADOW_ERR	    mBIT(35)
#define	VXGE_HAL_ORP_ERR_REG_ORP_OUTQEM_SHADOW_ERR	    mBIT(39)
#define	VXGE_HAL_ORP_ERR_REG_ORP_OUTFRM_SHADOW_ERR	    mBIT(43)
#define	VXGE_HAL_ORP_ERR_REG_ORP_OPTPRS_SHADOW_ERR	    mBIT(47)
/* 0x05818 */	u64	orp_err_mask;
/* 0x05820 */	u64	orp_err_alarm;
/* 0x05828 */	u64	ptm_alarm_reg;
#define	VXGE_HAL_PTM_ALARM_REG_PTM_RDCTRL_SYNC_ERR	    mBIT(3)
#define	VXGE_HAL_PTM_ALARM_REG_PTM_RDCTRL_FIFO_ERR	    mBIT(7)
#define	VXGE_HAL_PTM_ALARM_REG_XFMD_RD_FIFO_ERR		    mBIT(11)
#define	VXGE_HAL_PTM_ALARM_REG_WDE2MSR_WR_FIFO_ERR	    mBIT(15)
#define	VXGE_HAL_PTM_ALARM_REG_PTM_FRMM_ECC_DB_ERR(val)	    vBIT(val, 18, 2)
#define	VXGE_HAL_PTM_ALARM_REG_PTM_FRMM_ECC_SG_ERR(val)	    vBIT(val, 22, 2)
/* 0x05830 */	u64	ptm_alarm_mask;
/* 0x05838 */	u64	ptm_alarm_alarm;
/* 0x05840 */	u64	tpa_error_reg;
#define	VXGE_HAL_TPA_ERROR_REG_TPA_FSM_ERR_ALARM	    mBIT(3)
#define	VXGE_HAL_TPA_ERROR_REG_TPA_TPA_DA_LKUP_PRT0_DB_ERR  mBIT(7)
#define	VXGE_HAL_TPA_ERROR_REG_TPA_TPA_DA_LKUP_PRT0_SG_ERR  mBIT(11)
/* 0x05848 */	u64	tpa_error_mask;
/* 0x05850 */	u64	tpa_error_alarm;
/* 0x05858 */	u64	tpa_global_cfg;
#define	VXGE_HAL_TPA_GLOBAL_CFG_SUPPORT_SNAP_AB_N	    mBIT(7)
#define	VXGE_HAL_TPA_GLOBAL_CFG_ECC_ENABLE_N		    mBIT(35)
/* 0x05860 */	u64	tx_datapath_util;
#define	VXGE_HAL_TX_DATAPATH_UTIL_TPA_TX_UTILIZATION(val)   vBIT(val, 7, 9)
#define	VXGE_HAL_TX_DATAPATH_UTIL_TX_UTIL_CFG(val)	    vBIT(val, 16, 4)
#define	VXGE_HAL_TX_DATAPATH_UTIL_TPA_TX_FRAC_UTIL(val)	    vBIT(val, 20, 4)
#define	VXGE_HAL_TX_DATAPATH_UTIL_TX_PKT_WEIGHT(val)	    vBIT(val, 24, 4)
/* 0x05868 */	u64	orp_cfg;
#define	VXGE_HAL_ORP_CFG_FIFO_CREDITS(val)		    vBIT(val, 5, 3)
#define	VXGE_HAL_ORP_CFG_ORP_FIFO_ECC_ENABLE_N		    mBIT(15)
#define	VXGE_HAL_ORP_CFG_FIFO_PHASE_EN			    mBIT(23)
/* 0x05870 */	u64	ptm_ecc_cfg;
#define	VXGE_HAL_PTM_ECC_CFG_PTM_FRMM_ECC_EN_N		    mBIT(3)
/* 0x05878 */	u64	ptm_phase_cfg;
#define	VXGE_HAL_PTM_PHASE_CFG_FRMM_WR_PHASE_EN		    mBIT(3)
#define	VXGE_HAL_PTM_PHASE_CFG_FRMM_RD_PHASE_EN		    mBIT(7)
/* 0x05880 */	u64	orp_lro_events;
#define	VXGE_HAL_ORP_LRO_EVENTS_ORP_LRO_EVENTS(val)	    vBIT(val, 0, 64)
/* 0x05888 */	u64	orp_bs_events;
#define	VXGE_HAL_ORP_BS_EVENTS_ORP_BS_EVENTS(val)	    vBIT(val, 0, 64)
/* 0x05890 */	u64	orp_iwarp_events;
#define	VXGE_HAL_ORP_IWARP_EVENTS_ORP_IWARP_EVENTS(val)	    vBIT(val, 0, 64)
/* 0x05898 */	u64	dbg_stats_tpa_tx_path;
#define	VXGE_HAL_DBG_STATS_TPA_TX_PATH_TX_PERMITTED_FRMS(val) vBIT(val, 32, 32)
	u8	unused05900[0x05900 - 0x058a0];

/* 0x05900 */	u64	tmac_int_status;
#define	VXGE_HAL_TMAC_INT_STATUS_TXMAC_GEN_ERR_TXMAC_GEN_INT	mBIT(3)
#define	VXGE_HAL_TMAC_INT_STATUS_TXMAC_ECC_ERR_TXMAC_ECC_INT	mBIT(7)
/* 0x05908 */	u64	tmac_int_mask;
/* 0x05910 */	u64	txmac_gen_err_reg;
#define	VXGE_HAL_TXMAC_GEN_ERR_REG_TMACJ_PERMANENT_STOP	    mBIT(3)
#define	VXGE_HAL_TXMAC_GEN_ERR_REG_TMACJ_NO_VALID_VSPORT    mBIT(7)
/* 0x05918 */	u64	txmac_gen_err_mask;
/* 0x05920 */	u64	txmac_gen_err_alarm;
/* 0x05928 */	u64	txmac_ecc_err_reg;
#define	VXGE_HAL_TXMAC_ECC_ERR_REG_TMACJ_TMAC_TPA2MAC_SG_ERR	mBIT(3)
#define	VXGE_HAL_TXMAC_ECC_ERR_REG_TMACJ_TMAC_TPA2MAC_DB_ERR	mBIT(7)
#define	VXGE_HAL_TXMAC_ECC_ERR_REG_TMACJ_TMAC_TPA2M_SB_SG_ERR	mBIT(11)
#define	VXGE_HAL_TXMAC_ECC_ERR_REG_TMACJ_TMAC_TPA2M_SB_DB_ERR	mBIT(15)
#define	VXGE_HAL_TXMAC_ECC_ERR_REG_TMACJ_TMAC_TPA2M_DA_SG_ERR	mBIT(19)
#define	VXGE_HAL_TXMAC_ECC_ERR_REG_TMACJ_TMAC_TPA2M_DA_DB_ERR	mBIT(23)
#define	VXGE_HAL_TXMAC_ECC_ERR_REG_TMAC_TMAC_PORT0_FSM_ERR  mBIT(27)
#define	VXGE_HAL_TXMAC_ECC_ERR_REG_TMAC_TMAC_PORT1_FSM_ERR  mBIT(31)
#define	VXGE_HAL_TXMAC_ECC_ERR_REG_TMAC_TMAC_PORT2_FSM_ERR  mBIT(35)
#define	VXGE_HAL_TXMAC_ECC_ERR_REG_TMACJ_TMACJ_FSM_ERR	    mBIT(39)
/* 0x05930 */	u64	txmac_ecc_err_mask;
/* 0x05938 */	u64	txmac_ecc_err_alarm;
	u8	unused05948[0x05948-0x05940];

/* 0x05948 */	u64	txmac_gen_cfg1;
#define	VXGE_HAL_TXMAC_GEN_CFG1_TX_SWITCH_DISABLE	    mBIT(7)
#define	VXGE_HAL_TXMAC_GEN_CFG1_LOSSY_SWITCH		    mBIT(11)
#define	VXGE_HAL_TXMAC_GEN_CFG1_LOSSY_WIRE		    mBIT(15)
#define	VXGE_HAL_TXMAC_GEN_CFG1_SCALE_TMAC_UTIL		    mBIT(27)
#define	VXGE_HAL_TXMAC_GEN_CFG1_DISCARD_WHEN_TMAC_DISABLED  mBIT(35)
#define	VXGE_HAL_TXMAC_GEN_CFG1_IFS_EN			    mBIT(39)
#define	VXGE_HAL_TXMAC_GEN_CFG1_IFS_STRETCH_RATIO(val)	    vBIT(val, 40, 16)
#define	VXGE_HAL_TXMAC_GEN_CFG1_IFS_NUM_EXTENSION(val)	    vBIT(val, 59, 5)
	u8	unused05958[0x05958 - 0x05950];

/* 0x05958 */	u64	txmac_err_inject_cfg;
#define	VXGE_HAL_TXMAC_ERR_INJECT_CFG_INJECTOR_ERROR_RATE(val) vBIT(val, 0, 32)
/* 0x05960 */	u64	txmac_frmgen_cfg;
#define	VXGE_HAL_TXMAC_FRMGEN_CFG_EN			    mBIT(3)
#define	VXGE_HAL_TXMAC_FRMGEN_CFG_MODE(val)		    vBIT(val, 6, 2)
#define	VXGE_HAL_TXMAC_FRMGEN_CFG_PERIOD(val)		    vBIT(val, 8, 4)
#define	VXGE_HAL_TXMAC_FRMGEN_CFG_SEND_TO_WIRE		    mBIT(15)
#define	VXGE_HAL_TXMAC_FRMGEN_CFG_VPATH_VECTOR(val)	    vBIT(val, 19, 17)
#define	VXGE_HAL_TXMAC_FRMGEN_CFG_SRC_VPATH(val)	    vBIT(val, 39, 5)
#define	VXGE_HAL_TXMAC_FRMGEN_CFG_HOST_STEERING(val)	    vBIT(val, 44, 2)
#define	VXGE_HAL_TXMAC_FRMGEN_CFG_IFS_SEL(val)		    vBIT(val, 47, 3)
/* 0x05968 */	u64	txmac_frmgen_contents;
#define	VXGE_HAL_TXMAC_FRMGEN_CONTENTS_PATTERN_SEL(val)	    vBIT(val, 2, 2)
#define	VXGE_HAL_TXMAC_FRMGEN_CONTENTS_DA_SEL(val)	    vBIT(val, 6, 2)
#define	VXGE_HAL_TXMAC_FRMGEN_CONTENTS_LEN_SEL		    mBIT(11)
#define	VXGE_HAL_TXMAC_FRMGEN_CONTENTS_MIN_LEN(val)	    vBIT(val, 14, 14)
#define	VXGE_HAL_TXMAC_FRMGEN_CONTENTS_MAX_LEN(val)	    vBIT(val, 30, 14)
#define	VXGE_HAL_TXMAC_FRMGEN_CONTENTS_LT_FIELD(val)	    vBIT(val, 44, 16)
#define	VXGE_HAL_TXMAC_FRMGEN_CONTENTS_DATA_SEL(val)	    vBIT(val, 62, 2)
/* 0x05970 */	u64	txmac_frmgen_data;
#define	VXGE_HAL_TXMAC_FRMGEN_DATA_FRMDATA(val)		    vBIT(val, 0, 64)
/* 0x05978 */	u64	dbg_stat_tx_any_frms;
#define	VXGE_HAL_DBG_STAT_TX_ANY_FRMS_PORT0_TX_ANY_FRMS(val) vBIT(val, 0, 8)
#define	VXGE_HAL_DBG_STAT_TX_ANY_FRMS_PORT1_TX_ANY_FRMS(val) vBIT(val, 8, 8)
#define	VXGE_HAL_DBG_STAT_TX_ANY_FRMS_PORT2_TX_ANY_FRMS(val) vBIT(val, 16, 8)
	u8	unused059a0[0x059a0 - 0x05980];

/* 0x059a0 */	u64	txmac_link_util_port[3];
#define	VXGE_HAL_TXMAC_LINK_UTIL_PORT_TMAC_TMAC_UTILIZATION(val) vBIT(val, 1, 7)
#define	VXGE_HAL_TXMAC_LINK_UTIL_PORT_TMAC_UTIL_CFG(val)    vBIT(val, 8, 4)
#define	VXGE_HAL_TXMAC_LINK_UTIL_PORT_TMAC_TMAC_FRAC_UTIL(val) vBIT(val, 12, 4)
#define	VXGE_HAL_TXMAC_LINK_UTIL_PORT_TMAC_PKT_WEIGHT(val)  vBIT(val, 16, 4)
#define	VXGE_HAL_TXMAC_LINK_UTIL_PORT_TMAC_TMAC_SCALE_FACTOR mBIT(23)
/* 0x059b8 */	u64	txmac_cfg0_port[3];
#define	VXGE_HAL_TXMAC_CFG0_PORT_TMAC_EN		    mBIT(3)
#define	VXGE_HAL_TXMAC_CFG0_PORT_APPEND_PAD		    mBIT(7)
#define	VXGE_HAL_TXMAC_CFG0_PORT_PAD_BYTE(val)		    vBIT(val, 8, 8)
/* 0x059d0 */	u64	txmac_cfg1_port[3];
#define	VXGE_HAL_TXMAC_CFG1_PORT_AVG_IPG(val)		    vBIT(val, 40, 8)
/* 0x059e8 */	u64	txmac_status_port[3];
#define	VXGE_HAL_TXMAC_STATUS_PORT_TMAC_TX_FRM_SENT	    mBIT(3)
	u8	unused05a20[0x05a20 - 0x05a00];

/* 0x05a20 */	u64	lag_distrib_dest;
#define	VXGE_HAL_LAG_DISTRIB_DEST_MAP_VPATH(n)		    mBIT(n)
/* 0x05a28 */	u64	lag_marker_cfg;
#define	VXGE_HAL_LAG_MARKER_CFG_GEN_RCVR_EN		    mBIT(3)
#define	VXGE_HAL_LAG_MARKER_CFG_RESP_EN			    mBIT(7)
#define	VXGE_HAL_LAG_MARKER_CFG_RESP_TIMEOUT(val)	    vBIT(val, 16, 16)
#define	VXGE_HAL_LAG_MARKER_CFG_SLOW_PROTO_MRKR_MIN_INTERVAL(val)\
							    vBIT(val, 32, 16)
#define	VXGE_HAL_LAG_MARKER_CFG_THROTTLE_MRKR_RESP	    mBIT(51)
/* 0x05a30 */	u64	lag_tx_cfg;
#define	VXGE_HAL_LAG_TX_CFG_INCR_TX_AGGR_STATS		    mBIT(3)
#define	VXGE_HAL_LAG_TX_CFG_DISTRIB_ALG_SEL(val)	    vBIT(val, 6, 2)
#define	VXGE_HAL_LAG_TX_CFG_DISTRIB_REMAP_IF_FAIL	    mBIT(11)
#define	VXGE_HAL_LAG_TX_CFG_COLL_MAX_DELAY(val)		    vBIT(val, 16, 16)
/* 0x05a38 */	u64	lag_tx_status;
#define	VXGE_HAL_LAG_TX_STATUS_TLAG_TIMER_VAL_EMPTIED_LINK(val) vBIT(val, 0, 8)
#define	VXGE_HAL_LAG_TX_STATUS_TLAG_TIMER_VAL_SLOW_PROTO_MRKR(val)\
							    vBIT(val, 8, 8)
#define	VXGE_HAL_LAG_TX_STATUS_TLAG_TIMER_VAL_SLOW_PROTO_MRKRRESP(val)\
							    vBIT(val, 16, 8)
	u8	unused05a50[0x05a50 - 0x05a40];

/* 0x05a50 */	u64	txmac_stats_tx_xgmii_char;
#define	VXGE_HAL_TXMAC_STATS_TX_XGMII_CHAR_LANE_CHAR1(val)  vBIT(val, 1, 3)
#define	VXGE_HAL_TXMAC_STATS_TX_XGMII_CHAR_TXC_CHAR1	    mBIT(7)
#define	VXGE_HAL_TXMAC_STATS_TX_XGMII_CHAR_TXD_CHAR1(val)   vBIT(val, 8, 8)
#define	VXGE_HAL_TXMAC_STATS_TX_XGMII_CHAR_LANE_CHAR2(val)  vBIT(val, 17, 3)
#define	VXGE_HAL_TXMAC_STATS_TX_XGMII_CHAR_TXC_CHAR2	    mBIT(23)
#define	VXGE_HAL_TXMAC_STATS_TX_XGMII_CHAR_TXD_CHAR2(val)   vBIT(val, 24, 8)
#define	VXGE_HAL_TXMAC_STATS_TX_XGMII_CHAR_BEHAV_CHAR2_NEAR_CHAR1 mBIT(39)
#define	VXGE_HAL_TXMAC_STATS_TX_XGMII_CHAR_BEHAV_CHAR2_NUM_CHAR(val)\
							    vBIT(val, 40, 16)
/* 0x05a58 */	u64	txmac_stats_tx_xgmii_column1;
#define	VXGE_HAL_TXMAC_STATS_TX_XGMII_COLUMN1_TXC_LANE0	    mBIT(7)
#define	VXGE_HAL_TXMAC_STATS_TX_XGMII_COLUMN1_TXD_LANE0(val) vBIT(val, 8, 8)
#define	VXGE_HAL_TXMAC_STATS_TX_XGMII_COLUMN1_TXC_LANE1	    mBIT(23)
#define	VXGE_HAL_TXMAC_STATS_TX_XGMII_COLUMN1_TXD_LANE1(val) vBIT(val, 24, 8)
#define	VXGE_HAL_TXMAC_STATS_TX_XGMII_COLUMN1_TXC_LANE2	    mBIT(39)
#define	VXGE_HAL_TXMAC_STATS_TX_XGMII_COLUMN1_TXD_LANE2(val) vBIT(val, 40, 8)
#define	VXGE_HAL_TXMAC_STATS_TX_XGMII_COLUMN1_TXC_LANE3	    mBIT(55)
#define	VXGE_HAL_TXMAC_STATS_TX_XGMII_COLUMN1_TXD_LANE3(val) vBIT(val, 56, 8)
/* 0x05a60 */	u64	txmac_stats_tx_xgmii_column2;
#define	VXGE_HAL_TXMAC_STATS_TX_XGMII_COLUMN2_TXC_LANE0	    mBIT(7)
#define	VXGE_HAL_TXMAC_STATS_TX_XGMII_COLUMN2_TXD_LANE0(val) vBIT(val, 8, 8)
#define	VXGE_HAL_TXMAC_STATS_TX_XGMII_COLUMN2_TXC_LANE1	    mBIT(23)
#define	VXGE_HAL_TXMAC_STATS_TX_XGMII_COLUMN2_TXD_LANE1(val) vBIT(val, 24, 8)
#define	VXGE_HAL_TXMAC_STATS_TX_XGMII_COLUMN2_TXC_LANE2	    mBIT(39)
#define	VXGE_HAL_TXMAC_STATS_TX_XGMII_COLUMN2_TXD_LANE2(val) vBIT(val, 40, 8)
#define	VXGE_HAL_TXMAC_STATS_TX_XGMII_COLUMN2_TXC_LANE3	    mBIT(55)
#define	VXGE_HAL_TXMAC_STATS_TX_XGMII_COLUMN2_TXD_LANE3(val) vBIT(val, 56, 8)
/* 0x05a68 */	u64	txmac_stats_tx_xgmii_behav_column2;
#define	VXGE_HAL_TXMAC_STATS_TX_XGMII_BEHAV_COLUMN2_NEAR_COL1 mBIT(7)
#define	VXGE_HAL_TXMAC_STATS_TX_XGMII_BEHAV_COLUMN2_NUM_COL(val)\
							    vBIT(val, 8, 16)
	u8	unused05b00[0x05b00 - 0x05a70];

/* 0x05b00 */	u64	sharedio_status;
#define	VXGE_HAL_SHAREDIO_STATUS_PCI_NEGOTIATED_ACTIVE_VPLANE(val)\
							    vBIT(val, 0, 17)
#define	VXGE_HAL_SHAREDIO_STATUS_PCI_NEGOTIATED_VPLANE_COUNT(val)\
							    vBIT(val, 20, 8)
#define	VXGE_HAL_SHAREDIO_STATUS_PCI_NEGOTIATED_SHC	    mBIT(31)
#define	VXGE_HAL_SHAREDIO_STATUS_PCI_SHARED_IO_MODE	    mBIT(34)
#define	VXGE_HAL_SHAREDIO_STATUS_PCI_RX_ILLEGAL_TLP_VPLANE_VAL(val)\
							    vBIT(val, 36, 8)
/* 0x05b08 */	u64	crdt_status1_vplane[17];
#define	VXGE_HAL_CRDT_STATUS1_VPLANE_PCI_ABS_PD(val)	    vBIT(val, 4, 12)
#define	VXGE_HAL_CRDT_STATUS1_VPLANE_PCI_ABS_NPD(val)	    vBIT(val, 20, 12)
#define	VXGE_HAL_CRDT_STATUS1_VPLANE_PCI_ABS_CPLD(val)	    vBIT(val, 36, 12)
#define	VXGE_HAL_CRDT_STATUS1_VPLANE_PCI_ABS_PD_INFINITE    mBIT(51)
#define	VXGE_HAL_CRDT_STATUS1_VPLANE_PCI_ABS_NPD_INFINITE   mBIT(55)
#define	VXGE_HAL_CRDT_STATUS1_VPLANE_PCI_ABS_CPLD_INFINITE  mBIT(59)
/* 0x05b90 */	u64	crdt_status2_vplane[17];
#define	VXGE_HAL_CRDT_STATUS2_VPLANE_PCI_ABS_PH(val)	    vBIT(val, 0, 8)
#define	VXGE_HAL_CRDT_STATUS2_VPLANE_PCI_ABS_NPH(val)	    vBIT(val, 8, 8)
#define	VXGE_HAL_CRDT_STATUS2_VPLANE_PCI_ABS_CPLH(val)	    vBIT(val, 16, 8)
#define	VXGE_HAL_CRDT_STATUS2_VPLANE_PCI_ABS_PH_INFINITE    mBIT(31)
#define	VXGE_HAL_CRDT_STATUS2_VPLANE_PCI_ABS_NPH_INFINITE   mBIT(35)
#define	VXGE_HAL_CRDT_STATUS2_VPLANE_PCI_ABS_CPLH_INFINITE  mBIT(39)
/* 0x05c18 */	u64	crdt_status3_vplane[17];
#define	VXGE_HAL_CRDT_STATUS3_VPLANE_PCI_AVAIL_ABS_BUF_PD(val) vBIT(val, 4, 12)
#define	VXGE_HAL_CRDT_STATUS3_VPLANE_PCI_AVAIL_ABS_BUF_NPD(val)\
							    vBIT(val, 20, 12)
#define	VXGE_HAL_CRDT_STATUS3_VPLANE_PCI_AVAIL_ABS_BUF_CPLD(val)\
							    vBIT(val, 36, 12)
/* 0x05ca0 */	u64	crdt_status4_vplane[17];
#define	VXGE_HAL_CRDT_STATUS4_VPLANE_PCI_AVAIL_ABS_BUF_PH(val) vBIT(val, 0, 8)
#define	VXGE_HAL_CRDT_STATUS4_VPLANE_PCI_AVAIL_ABS_BUF_NPH(val) vBIT(val, 8, 8)
#define	VXGE_HAL_CRDT_STATUS4_VPLANE_PCI_AVAIL_ABS_BUF_CPLH(val)\
							    vBIT(val, 16, 8)
/* 0x05d28 */	u64	crdt_status5;
#define	VXGE_HAL_CRDT_STATUS5_PCI_DEPL_PH(val)		    vBIT(val, 0, 17)
#define	VXGE_HAL_CRDT_STATUS5_PCI_DEPL_NPH(val)		    vBIT(val, 20, 17)
#define	VXGE_HAL_CRDT_STATUS5_PCI_DEPL_CPLH(val)	    vBIT(val, 40, 17)
/* 0x05d30 */	u64	crdt_status6;
#define	VXGE_HAL_CRDT_STATUS6_PCI_DEPL_PD(val)		    vBIT(val, 0, 17)
#define	VXGE_HAL_CRDT_STATUS6_PCI_DEPL_NPD(val)		    vBIT(val, 20, 17)
#define	VXGE_HAL_CRDT_STATUS6_PCI_DEPL_CPLD(val)	    vBIT(val, 40, 17)
/* 0x05d38 */	u64	crdt_status7;
#define	VXGE_HAL_CRDT_STATUS7_PCI_ABS_PD(val)		    vBIT(val, 4, 12)
#define	VXGE_HAL_CRDT_STATUS7_PCI_ABS_NPD(val)		    vBIT(val, 20, 12)
#define	VXGE_HAL_CRDT_STATUS7_PCI_ABS_CPLD(val)		    vBIT(val, 36, 12)
#define	VXGE_HAL_CRDT_STATUS7_PCI_ABS_PD_INFINITE	    mBIT(51)
#define	VXGE_HAL_CRDT_STATUS7_PCI_ABS_NPD_INFINITE	    mBIT(55)
#define	VXGE_HAL_CRDT_STATUS7_PCI_ABS_CPLD_INFINITE	    mBIT(59)
/* 0x05d40 */	u64	crdt_status8;
#define	VXGE_HAL_CRDT_STATUS8_PCI_ABS_PH(val)		    vBIT(val, 0, 8)
#define	VXGE_HAL_CRDT_STATUS8_PCI_ABS_NPH(val)		    vBIT(val, 8, 8)
#define	VXGE_HAL_CRDT_STATUS8_PCI_ABS_CPLH(val)		    vBIT(val, 16, 8)
#define	VXGE_HAL_CRDT_STATUS8_PCI_ABS_PH_INFINITE	    mBIT(31)
#define	VXGE_HAL_CRDT_STATUS8_PCI_ABS_NPH_INFINITE	    mBIT(35)
#define	VXGE_HAL_CRDT_STATUS8_PCI_ABS_CPLH_INFINITE	    mBIT(39)
/* 0x05d48 */	u64	srpcim_to_mrpcim_vplane_rmsg[17];
#define	VXGE_HAL_SRPCIM_TO_MRPCIM_VPLANE_RMSG_RMSG(val)	    vBIT(val, 0, 64)
	u8	unused06000[0x06000 - 0x05dd0];

/* 0x06000 */	u64	pcie_lane_cfg1;
#define	VXGE_HAL_PCIE_LANE_CFG1_RX_0_SEL(val)		    vBIT(val, 1, 3)
#define	VXGE_HAL_PCIE_LANE_CFG1_RX_1_SEL(val)		    vBIT(val, 5, 3)
#define	VXGE_HAL_PCIE_LANE_CFG1_RX_2_SEL(val)		    vBIT(val, 9, 3)
#define	VXGE_HAL_PCIE_LANE_CFG1_RX_3_SEL(val)		    vBIT(val, 13, 3)
#define	VXGE_HAL_PCIE_LANE_CFG1_RX_4_SEL(val)		    vBIT(val, 17, 3)
#define	VXGE_HAL_PCIE_LANE_CFG1_RX_5_SEL(val)		    vBIT(val, 21, 3)
#define	VXGE_HAL_PCIE_LANE_CFG1_RX_6_SEL(val)		    vBIT(val, 25, 3)
#define	VXGE_HAL_PCIE_LANE_CFG1_RX_7_SEL(val)		    vBIT(val, 29, 3)
#define	VXGE_HAL_PCIE_LANE_CFG1_TX_0_SEL(val)		    vBIT(val, 33, 3)
#define	VXGE_HAL_PCIE_LANE_CFG1_TX_1_SEL(val)		    vBIT(val, 37, 3)
#define	VXGE_HAL_PCIE_LANE_CFG1_TX_2_SEL(val)		    vBIT(val, 41, 3)
#define	VXGE_HAL_PCIE_LANE_CFG1_TX_3_SEL(val)		    vBIT(val, 45, 3)
#define	VXGE_HAL_PCIE_LANE_CFG1_TX_4_SEL(val)		    vBIT(val, 49, 3)
#define	VXGE_HAL_PCIE_LANE_CFG1_TX_5_SEL(val)		    vBIT(val, 53, 3)
#define	VXGE_HAL_PCIE_LANE_CFG1_TX_6_SEL(val)		    vBIT(val, 57, 3)
#define	VXGE_HAL_PCIE_LANE_CFG1_TX_7_SEL(val)		    vBIT(val, 61, 3)
/* 0x06008 */	u64	pcie_lane_cfg2;
#define	VXGE_HAL_PCIE_LANE_CFG2_STROBE			    mBIT(0)
/* 0x06010 */	u64	pcicfg_no_to_func_cfg[25];
#define	VXGE_HAL_PCICFG_NO_TO_FUNC_CFG_PCICFG_NO_TO_FUNC_CFG(val)\
							    vBIT(val, 3, 5)
/* 0x060d8 */	u64	resource_to_vplane_cfg[17];
#define	VXGE_HAL_RESOURCE_TO_VPLANE_CFG_RESOURCE_TO_VPLANE_CFG(val)\
							    vBIT(val, 3, 5)
/* 0x06160 */	u64	pcicfg_no_to_vplane_cfg[25];
#define	VXGE_HAL_PCICFG_NO_TO_VPLANE_CFG_PCICFG_NO_TO_VPLANE_CFG(val)\
							    vBIT(val, 3, 5)
/* 0x06228 */	u64	general_cfg;
#define	VXGE_HAL_GENERAL_CFG_ENABLE_FLR_ON_MRIOV_DIS	    mBIT(0)
#define	VXGE_HAL_GENERAL_CFG_ENABLE_FLR_ON_SRIOV_DIS	    mBIT(1)
#define	VXGE_HAL_GENERAL_CFG_MULTI_FUNC_8_MODE		    mBIT(2)
#define	VXGE_HAL_GENERAL_CFG_EN_RST_CPLTO_IN_LUT	    mBIT(3)
#define	VXGE_HAL_GENERAL_CFG_RST_CPLTO_VAL(val)		    vBIT(val, 4, 4)
#define	VXGE_HAL_GENERAL_CFG_SHARED_IO_MODE		    mBIT(11)
#define	VXGE_HAL_GENERAL_CFG_INIT_OSD_COUNT(val)	    vBIT(val, 12, 8)
#define	VXGE_HAL_GENERAL_CFG_INIT_SHC(val)		    vBIT(val, 20, 8)
#define	VXGE_HAL_GENERAL_CFG_INITOSD_VERSION(val)	    vBIT(val, 29, 3)
#define	VXGE_HAL_GENERAL_CFG_SNOOP_CPLH_CRDT_ON_BUS	    mBIT(35)
#define	VXGE_HAL_GENERAL_CFG_FC_UPDT_FREQ_VAL(val)	    vBIT(val, 36, 4)
#define	VXGE_HAL_GENERAL_CFG_RX_MEM_ECC_ENABLE_N	    mBIT(43)
#define	VXGE_HAL_GENERAL_CFG_TX_MEM_ECC_ENABLE_N	    mBIT(47)
#define	VXGE_HAL_GENERAL_CFG_MRIOV_CFG_EN		    mBIT(51)
#define	VXGE_HAL_GENERAL_CFG_HIDE_VPD_CAPABILITY	    mBIT(53)
#define	VXGE_HAL_GENERAL_CFG_FORCE_RDS_TO_USE_PF_REQID	    mBIT(54)
#define	VXGE_HAL_GENERAL_CFG_POISON_ADVISORY		    mBIT(55)
#define	VXGE_HAL_GENERAL_CFG_CPL_TIMEOUT_ADVISORY	    mBIT(56)
#define	VXGE_HAL_GENERAL_CFG_UNEXP_CPL_ADVISORY		    mBIT(57)
#define	VXGE_HAL_GENERAL_CFG_UR_ADVISORY		    mBIT(58)
#define	VXGE_HAL_GENERAL_CFG_CA_ADVISORY		    mBIT(59)
#define	VXGE_HAL_GENERAL_CFG_WAIT_FOR_CPLH_CRDT_ON_BUS	    mBIT(60)
#define	VXGE_HAL_GENERAL_CFG_EN_SEND_ERR_MSG_FOR_SERR	    mBIT(61)
#define	VXGE_HAL_GENERAL_CFG_SEND_NF_MSG_FOR_SERR	    mBIT(62)
#define	VXGE_HAL_GENERAL_CFG_VF_MUST_USE_CFG_TYPE0	    mBIT(63)
/* 0x06230 */	u64	start_bist;
#define	VXGE_HAL_START_BIST_START_BIST			    mBIT(0)
/* 0x06238 */	u64	bist_cfg;
#define	VXGE_HAL_BIST_CFG_IGNORE_MEM_RDY		    mBIT(3)
#define	VXGE_HAL_BIST_CFG_ENABLE			    mBIT(7)
#define	VXGE_HAL_BIST_CFG_JTAG_BIST_COMPLETION_CODE(val)    vBIT(val, 8, 4)
/* 0x06240 */	u64	pci_link_control;
#define	VXGE_HAL_PCI_LINK_CONTROL_APP_REQ_RETRY_EN	    mBIT(3)
#define	VXGE_HAL_PCI_LINK_CONTROL_APP_LTSSM_EN		    mBIT(7)
/* 0x06248 */	u64	show_sriov_cap;
#define	VXGE_HAL_SHOW_SRIOV_CAP_SHOW_SRIOV_CAP(val)	    vBIT(val, 0, 9)
/* 0x06250 */	u64	link_rst_wait_cnt;
#define	VXGE_HAL_LINK_RST_WAIT_CNT_LINK_RST_WAIT_CNT(val)   vBIT(val, 0, 16)
/* 0x06258 */	u64	pcie_based_crdt_cfg1;
#define	VXGE_HAL_PCIE_BASED_CRDT_CFG1_INIT_PD(val)	    vBIT(val, 4, 12)
#define	VXGE_HAL_PCIE_BASED_CRDT_CFG1_INIT_NPD(val)	    vBIT(val, 20, 12)
#define	VXGE_HAL_PCIE_BASED_CRDT_CFG1_INIT_CPLD(val)	    vBIT(val, 36, 12)
/* 0x06260 */	u64	pcie_based_crdt_cfg2;
#define	VXGE_HAL_PCIE_BASED_CRDT_CFG2_INIT_PH(val)	    vBIT(val, 0, 8)
#define	VXGE_HAL_PCIE_BASED_CRDT_CFG2_INIT_NPH(val)	    vBIT(val, 8, 8)
#define	VXGE_HAL_PCIE_BASED_CRDT_CFG2_INIT_CPLH(val)	    vBIT(val, 16, 8)
/* 0x06268 */	u64	sharedio_abs_based_crdt_cfg1_vplane[17];
#define	VXGE_HAL_SHAREDIO_ABS_BASED_CRDT_CFG1_VPLANE_ABS_PD(val)\
							    vBIT(val, 4, 12)
#define	VXGE_HAL_SHAREDIO_ABS_BASED_CRDT_CFG1_VPLANE_ABS_NPD(val)\
							    vBIT(val, 20, 12)
#define	VXGE_HAL_SHAREDIO_ABS_BASED_CRDT_CFG1_VPLANE_ABS_CPLD(val)\
							    vBIT(val, 36, 12)
#define	VXGE_HAL_SHAREDIO_ABS_BASED_CRDT_CFG1_VPLANE_ABS_PD_INFINITE	mBIT(51)
#define	VXGE_HAL_SHAREDIO_ABS_BASED_CRDT_CFG1_VPLANE_ABS_NPD_INFINITE	mBIT(55)
#define	VXGE_HAL_SHAREDIO_ABS_BASED_CRDT_CFG1_VPLANE_ABS_CPLD_INFINITE	mBIT(59)
/* 0x062f0 */	u64	sharedio_abs_based_crdt_cfg2_vplane[17];
#define	VXGE_HAL_SHAREDIO_ABS_BASED_CRDT_CFG2_VPLANE_ABS_PH(val)\
							    vBIT(val, 0, 8)
#define	VXGE_HAL_SHAREDIO_ABS_BASED_CRDT_CFG2_VPLANE_ABS_NPH(val)\
							    vBIT(val, 8, 8)
#define	VXGE_HAL_SHAREDIO_ABS_BASED_CRDT_CFG2_VPLANE_ABS_CPLH(val)\
							    vBIT(val, 16, 8)
#define	VXGE_HAL_SHAREDIO_ABS_BASED_CRDT_CFG2_VPLANE_ABS_PH_INFINITE	mBIT(31)
#define	VXGE_HAL_SHAREDIO_ABS_BASED_CRDT_CFG2_VPLANE_ABS_NPH_INFINITE	mBIT(35)
#define	VXGE_HAL_SHAREDIO_ABS_BASED_CRDT_CFG2_VPLANE_ABS_CPLH_INFINITE	mBIT(39)
/* 0x06378 */	u64	arbiter_cfg;
#define	VXGE_HAL_ARBITER_CFG_CPL_PRIORITY(val)		    vBIT(val, 2, 2)
#define	VXGE_HAL_ARBITER_CFG_MRD_PRIORITY(val)		    vBIT(val, 6, 2)
#define	VXGE_HAL_ARBITER_CFG_MWR_PRIORITY(val)		    vBIT(val, 10, 2)
#define	VXGE_HAL_ARBITER_CFG_CHK_PRIORITY_MATCH_ONLY	    mBIT(15)
#define	VXGE_HAL_ARBITER_CFG_CALSTATE0_PRIORITY(val)	    vBIT(val, 18, 2)
#define	VXGE_HAL_ARBITER_CFG_CALSTATE1_PRIORITY(val)	    vBIT(val, 22, 2)
#define	VXGE_HAL_ARBITER_CFG_CALSTATE2_PRIORITY(val)	    vBIT(val, 26, 2)
#define	VXGE_HAL_ARBITER_CFG_CALSTATE3_PRIORITY(val)	    vBIT(val, 30, 2)
#define	VXGE_HAL_ARBITER_CFG_CALSTATE4_PRIORITY(val)	    vBIT(val, 34, 2)
#define	VXGE_HAL_ARBITER_CFG_CALSTATE5_PRIORITY(val)	    vBIT(val, 38, 2)
/* 0x06380 */	u64	serdes_cfg1;
#define	VXGE_HAL_SERDES_CFG1_TX_CLOCK_ALIGN(val)	    vBIT(val, 0, 8)
#define	VXGE_HAL_SERDES_CFG1_TX_CALC(val)		    vBIT(val, 8, 8)
#define	VXGE_HAL_SERDES_CFG1_TX_LVL(val)		    vBIT(val, 19, 5)
#define	VXGE_HAL_SERDES_CFG1_LOS_LVL(val)		    vBIT(val, 27, 5)
/* 0x06388 */	u64	serdes_cfg2;
#define	VXGE_HAL_SERDES_CFG2_TX_0_BOOST(val)		    vBIT(val, 0, 4)
#define	VXGE_HAL_SERDES_CFG2_TX_1_BOOST(val)		    vBIT(val, 4, 4)
#define	VXGE_HAL_SERDES_CFG2_TX_2_BOOST(val)		    vBIT(val, 8, 4)
#define	VXGE_HAL_SERDES_CFG2_TX_3_BOOST(val)		    vBIT(val, 12, 4)
#define	VXGE_HAL_SERDES_CFG2_TX_4_BOOST(val)		    vBIT(val, 16, 4)
#define	VXGE_HAL_SERDES_CFG2_TX_5_BOOST(val)		    vBIT(val, 20, 4)
#define	VXGE_HAL_SERDES_CFG2_TX_6_BOOST(val)		    vBIT(val, 24, 4)
#define	VXGE_HAL_SERDES_CFG2_TX_7_BOOST(val)		    vBIT(val, 28, 4)
#define	VXGE_HAL_SERDES_CFG2_TX_0_ATTEN(val)		    vBIT(val, 33, 3)
#define	VXGE_HAL_SERDES_CFG2_TX_1_ATTEN(val)		    vBIT(val, 37, 3)
#define	VXGE_HAL_SERDES_CFG2_TX_2_ATTEN(val)		    vBIT(val, 41, 3)
#define	VXGE_HAL_SERDES_CFG2_TX_3_ATTEN(val)		    vBIT(val, 45, 3)
#define	VXGE_HAL_SERDES_CFG2_TX_4_ATTEN(val)		    vBIT(val, 49, 3)
#define	VXGE_HAL_SERDES_CFG2_TX_5_ATTEN(val)		    vBIT(val, 53, 3)
#define	VXGE_HAL_SERDES_CFG2_TX_6_ATTEN(val)		    vBIT(val, 57, 3)
#define	VXGE_HAL_SERDES_CFG2_TX_7_ATTEN(val)		    vBIT(val, 61, 3)
/* 0x06390 */	u64	serdes_cfg3;
#define	VXGE_HAL_SERDES_CFG3_TX_0_EDGERATE(val)		    vBIT(val, 2, 2)
#define	VXGE_HAL_SERDES_CFG3_TX_1_EDGERATE(val)		    vBIT(val, 6, 2)
#define	VXGE_HAL_SERDES_CFG3_TX_2_EDGERATE(val)		    vBIT(val, 10, 2)
#define	VXGE_HAL_SERDES_CFG3_TX_3_EDGERATE(val)		    vBIT(val, 14, 2)
#define	VXGE_HAL_SERDES_CFG3_TX_4_EDGERATE(val)		    vBIT(val, 18, 2)
#define	VXGE_HAL_SERDES_CFG3_TX_5_EDGERATE(val)		    vBIT(val, 22, 2)
#define	VXGE_HAL_SERDES_CFG3_TX_6_EDGERATE(val)		    vBIT(val, 26, 2)
#define	VXGE_HAL_SERDES_CFG3_TX_7_EDGERATE(val)		    vBIT(val, 30, 2)
#define	VXGE_HAL_SERDES_CFG3_RX_0_EQ_VAL(val)		    vBIT(val, 33, 3)
#define	VXGE_HAL_SERDES_CFG3_RX_1_EQ_VAL(val)		    vBIT(val, 37, 3)
#define	VXGE_HAL_SERDES_CFG3_RX_2_EQ_VAL(val)		    vBIT(val, 41, 3)
#define	VXGE_HAL_SERDES_CFG3_RX_3_EQ_VAL(val)		    vBIT(val, 45, 3)
#define	VXGE_HAL_SERDES_CFG3_RX_4_EQ_VAL(val)		    vBIT(val, 49, 3)
#define	VXGE_HAL_SERDES_CFG3_RX_5_EQ_VAL(val)		    vBIT(val, 53, 3)
#define	VXGE_HAL_SERDES_CFG3_RX_6_EQ_VAL(val)		    vBIT(val, 57, 3)
#define	VXGE_HAL_SERDES_CFG3_RX_7_EQ_VAL(val)		    vBIT(val, 61, 3)
/* 0x06398 */	u64	vhlabel_to_vplane_cfg[17];
#define	VXGE_HAL_VHLABEL_TO_VPLANE_CFG_VHLABEL_TO_VPLANE_CFG(val)\
							    vBIT(val, 3, 5)
/* 0x06420 */	u64	mrpcim_to_srpcim_vplane_wmsg[17];
#define	VXGE_HAL_MRPCIM_TO_SRPCIM_VPLANE_WMSG_WMSG(val)	    vBIT(val, 0, 64)
/* 0x064a8 */	u64	mrpcim_to_srpcim_vplane_wmsg_trig[17];
#define	VXGE_HAL_MRPCIM_TO_SRPCIM_VPLANE_WMSG_TRIG_TRIG	    mBIT(0)
/* 0x06530 */	u64	debug_stats0;
#define	VXGE_HAL_DEBUG_STATS0_RSTDROP_MSG(val)		    vBIT(val, 0, 32)
#define	VXGE_HAL_DEBUG_STATS0_RSTDROP_CPL(val)		    vBIT(val, 32, 32)
/* 0x06538 */	u64	debug_stats1;
#define	VXGE_HAL_DEBUG_STATS1_RSTDROP_CLIENT0(val)	    vBIT(val, 0, 32)
#define	VXGE_HAL_DEBUG_STATS1_RSTDROP_CLIENT1(val)	    vBIT(val, 32, 32)
/* 0x06540 */	u64	debug_stats2;
#define	VXGE_HAL_DEBUG_STATS2_RSTDROP_CLIENT2(val)	    vBIT(val, 0, 32)
/* 0x06548 */	u64	debug_stats3_vplane[17];
#define	VXGE_HAL_DEBUG_STATS3_VPLANE_DEPL_PH(val)	    vBIT(val, 0, 16)
#define	VXGE_HAL_DEBUG_STATS3_VPLANE_DEPL_NPH(val)	    vBIT(val, 16, 16)
#define	VXGE_HAL_DEBUG_STATS3_VPLANE_DEPL_CPLH(val)	    vBIT(val, 32, 16)
/* 0x065d0 */	u64	debug_stats4_vplane[17];
#define	VXGE_HAL_DEBUG_STATS4_VPLANE_DEPL_PD(val)	    vBIT(val, 0, 16)
#define	VXGE_HAL_DEBUG_STATS4_VPLANE_DEPL_NPD(val)	    vBIT(val, 16, 16)
#define	VXGE_HAL_DEBUG_STATS4_VPLANE_DEPL_CPLD(val)	    vBIT(val, 32, 16)
	u8	unused06b00[0x06b00 - 0x06658];

/* 0x06b00 */	u64	rc_rxdmem_end_ofst[16];
#define	VXGE_HAL_RC_RXDMEM_END_OFST_RC_RXDMEM_END_OFST(val) vBIT(val, 49, 8)
	u8	unused07000[0x07000 - 0x06b80];

/* 0x07000 */	u64	mrpcim_general_int_status;
#define	VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_PIC_INT	    mBIT(0)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_PCI_INT	    mBIT(1)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_RTDMA_INT	    mBIT(2)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_WRDMA_INT	    mBIT(3)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_G3CMCT_INT	    mBIT(4)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_GCMG1_INT	    mBIT(5)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_GCMG2_INT	    mBIT(6)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_GCMG3_INT	    mBIT(7)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_G3CMIFL_INT	    mBIT(8)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_G3CMIFU_INT	    mBIT(9)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_PCMG1_INT	    mBIT(10)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_PCMG2_INT	    mBIT(11)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_PCMG3_INT	    mBIT(12)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_XMAC_INT	    mBIT(13)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_RXMAC_INT	    mBIT(14)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_TMAC_INT	    mBIT(15)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_G3FBIF_INT	    mBIT(16)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_FBMC_INT	    mBIT(17)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_G3FBCT_INT	    mBIT(18)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_TPA_INT	    mBIT(19)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_DRBELL_INT	    mBIT(20)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_ONE_INT	    mBIT(21)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_MSG_INT	    mBIT(22)
/* 0x07008 */	u64	mrpcim_general_int_mask;
#define	VXGE_HAL_MRPCIM_GENERAL_INT_MASK_PIC_INT	    mBIT(0)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_MASK_PCI_INT	    mBIT(1)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_MASK_RTDMA_INT	    mBIT(2)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_MASK_WRDMA_INT	    mBIT(3)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_MASK_G3CMCT_INT	    mBIT(4)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_MASK_GCMG1_INT	    mBIT(5)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_MASK_GCMG2_INT	    mBIT(6)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_MASK_GCMG3_INT	    mBIT(7)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_MASK_G3CMIFL_INT	    mBIT(8)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_MASK_G3CMIFU_INT	    mBIT(9)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_MASK_PCMG1_INT	    mBIT(10)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_MASK_PCMG2_INT	    mBIT(11)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_MASK_PCMG3_INT	    mBIT(12)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_MASK_XMAC_INT	    mBIT(13)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_MASK_RXMAC_INT	    mBIT(14)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_MASK_TMAC_INT	    mBIT(15)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_MASK_G3FBIF_INT	    mBIT(16)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_MASK_FBMC_INT	    mBIT(17)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_MASK_G3FBCT_INT	    mBIT(18)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_MASK_TPA_INT	    mBIT(19)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_MASK_DRBELL_INT	    mBIT(20)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_MASK_ONE_INT	    mBIT(21)
#define	VXGE_HAL_MRPCIM_GENERAL_INT_MASK_MSG_INT	    mBIT(22)
/* 0x07010 */	u64	mrpcim_ppif_int_status;
#define	VXGE_HAL_MRPCIM_PPIF_INT_STATUS_INI_ERRORS_INI_INT  mBIT(3)
#define	VXGE_HAL_MRPCIM_PPIF_INT_STATUS_DMA_ERRORS_DMA_INT  mBIT(7)
#define	VXGE_HAL_MRPCIM_PPIF_INT_STATUS_TGT_ERRORS_TGT_INT  mBIT(11)
#define	VXGE_HAL_MRPCIM_PPIF_INT_STATUS_CONFIG_ERRORS_CONFIG_INT mBIT(15)
#define	VXGE_HAL_MRPCIM_PPIF_INT_STATUS_CRDT_ERRORS_CRDT_INT mBIT(19)
#define	VXGE_HAL_MRPCIM_PPIF_INT_STATUS_MRPCIM_GENERAL_ERRORS_GENERAL_INT\
							    mBIT(23)
#define	VXGE_HAL_MRPCIM_PPIF_INT_STATUS_PLL_ERRORS_PLL_INT  mBIT(27)
#define	VXGE_HAL_MRPCIM_PPIF_INT_STATUS_CRDT_ERRORS_VPLANE0_CRD_INT_VPLANE0_INT\
							    mBIT(31)
#define	VXGE_HAL_MRPCIM_PPIF_INT_STATUS_CRDT_ERRORS_VPLANE1_CRD_INT_VPLANE1_INT\
							    mBIT(32)
#define	VXGE_HAL_MRPCIM_PPIF_INT_STATUS_CRDT_ERRORS_VPLANE2_CRD_INT_VPLANE2_INT\
							    mBIT(33)
#define	VXGE_HAL_MRPCIM_PPIF_INT_STATUS_CRDT_ERRORS_VPLANE3_CRD_INT_VPLANE3_INT\
							    mBIT(34)
#define	VXGE_HAL_MRPCIM_PPIF_INT_STATUS_CRDT_ERRORS_VPLANE4_CRD_INT_VPLANE4_INT\
							    mBIT(35)
#define	VXGE_HAL_MRPCIM_PPIF_INT_STATUS_CRDT_ERRORS_VPLANE5_CRD_INT_VPLANE5_INT\
							    mBIT(36)
#define	VXGE_HAL_MRPCIM_PPIF_INT_STATUS_CRDT_ERRORS_VPLANE6_CRD_INT_VPLANE6_INT\
							    mBIT(37)
#define	VXGE_HAL_MRPCIM_PPIF_INT_STATUS_CRDT_ERRORS_VPLANE7_CRD_INT_VPLANE7_INT\
							    mBIT(38)
#define	VXGE_HAL_MRPCIM_PPIF_INT_STATUS_CRDT_ERRORS_VPLANE8_CRD_INT_VPLANE8_INT\
							    mBIT(39)
#define	VXGE_HAL_MRPCIM_PPIF_INT_STATUS_CRDT_ERRORS_VPLANE9_CRD_INT_VPLANE9_INT\
							    mBIT(40)
#define	\
    VXGE_HAL_MRPCIM_PPIF_INT_STATUS_CRDT_ERRORS_VPLANE10_CRD_INT_VPLANE10_INT\
							    mBIT(41)
#define	\
    VXGE_HAL_MRPCIM_PPIF_INT_STATUS_CRDT_ERRORS_VPLANE11_CRD_INT_VPLANE11_INT\
							    mBIT(42)
#define	\
    VXGE_HAL_MRPCIM_PPIF_INT_STATUS_CRDT_ERRORS_VPLANE12_CRD_INT_VPLANE12_INT\
							    mBIT(43)
#define	\
    VXGE_HAL_MRPCIM_PPIF_INT_STATUS_CRDT_ERRORS_VPLANE13_CRD_INT_VPLANE13_INT\
							    mBIT(44)
#define	\
    VXGE_HAL_MRPCIM_PPIF_INT_STATUS_CRDT_ERRORS_VPLANE14_CRD_INT_VPLANE14_INT\
							    mBIT(45)
#define	\
    VXGE_HAL_MRPCIM_PPIF_INT_STATUS_CRDT_ERRORS_VPLANE15_CRD_INT_VPLANE15_INT\
							    mBIT(46)
#define	\
    VXGE_HAL_MRPCIM_PPIF_INT_STATUS_CRDT_ERRORS_VPLANE16_CRD_INT_VPLANE16_INT\
							    mBIT(47)
#define	VXGE_HAL_MRPCIM_PPIF_INT_STATUS_SRPCIM_TO_MRPCIM_ALARM_INT  mBIT(51)
#define	VXGE_HAL_MRPCIM_PPIF_INT_STATUS_VPATH_TO_MRPCIM_ALARM_INT   mBIT(55)
/* 0x07018 */	u64	mrpcim_ppif_int_mask;
	u8	unused07028[0x07028 - 0x07020];

/* 0x07028 */	u64	ini_errors_reg;
#define	VXGE_HAL_INI_ERRORS_REG_SCPL_CPL_TIMEOUT_UNUSED_TAG mBIT(3)
#define	VXGE_HAL_INI_ERRORS_REG_SCPL_CPL_TIMEOUT	    mBIT(7)
#define	VXGE_HAL_INI_ERRORS_REG_DCPL_FSM_ERR		    mBIT(11)
#define	VXGE_HAL_INI_ERRORS_REG_DCPL_POISON		    mBIT(12)
#define	VXGE_HAL_INI_ERRORS_REG_DCPL_UNSUPPORTED	    mBIT(15)
#define	VXGE_HAL_INI_ERRORS_REG_DCPL_ABORT		    mBIT(19)
#define	VXGE_HAL_INI_ERRORS_REG_INI_TLP_ABORT		    mBIT(23)
#define	VXGE_HAL_INI_ERRORS_REG_INI_DLLP_ABORT		    mBIT(27)
#define	VXGE_HAL_INI_ERRORS_REG_INI_ECRC_ERR		    mBIT(31)
#define	VXGE_HAL_INI_ERRORS_REG_INI_BUF_DB_ERR		    mBIT(35)
#define	VXGE_HAL_INI_ERRORS_REG_INI_BUF_SG_ERR		    mBIT(39)
#define	VXGE_HAL_INI_ERRORS_REG_INI_DATA_OVERFLOW	    mBIT(43)
#define	VXGE_HAL_INI_ERRORS_REG_INI_HDR_OVERFLOW	    mBIT(47)
#define	VXGE_HAL_INI_ERRORS_REG_INI_MRD_SYS_DROP	    mBIT(51)
#define	VXGE_HAL_INI_ERRORS_REG_INI_MWR_SYS_DROP	    mBIT(55)
#define	VXGE_HAL_INI_ERRORS_REG_INI_MRD_CLIENT_DROP	    mBIT(59)
#define	VXGE_HAL_INI_ERRORS_REG_INI_MWR_CLIENT_DROP	    mBIT(63)
/* 0x07030 */	u64	ini_errors_mask;
/* 0x07038 */	u64	ini_errors_alarm;
/* 0x07040 */	u64	dma_errors_reg;
#define	VXGE_HAL_DMA_ERRORS_REG_RDARB_FSM_ERR		    mBIT(3)
#define	VXGE_HAL_DMA_ERRORS_REG_WRARB_FSM_ERR		    mBIT(7)
#define	VXGE_HAL_DMA_ERRORS_REG_DMA_WRDMA_WR_HDR_OVERFLOW   mBIT(8)
#define	VXGE_HAL_DMA_ERRORS_REG_DMA_WRDMA_WR_HDR_UNDERFLOW  mBIT(9)
#define	VXGE_HAL_DMA_ERRORS_REG_DMA_WRDMA_WR_DATA_OVERFLOW  mBIT(10)
#define	VXGE_HAL_DMA_ERRORS_REG_DMA_WRDMA_WR_DATA_UNDERFLOW mBIT(11)
#define	VXGE_HAL_DMA_ERRORS_REG_DMA_MSG_WR_HDR_OVERFLOW	    mBIT(12)
#define	VXGE_HAL_DMA_ERRORS_REG_DMA_MSG_WR_HDR_UNDERFLOW    mBIT(13)
#define	VXGE_HAL_DMA_ERRORS_REG_DMA_MSG_WR_DATA_OVERFLOW    mBIT(14)
#define	VXGE_HAL_DMA_ERRORS_REG_DMA_MSG_WR_DATA_UNDERFLOW   mBIT(15)
#define	VXGE_HAL_DMA_ERRORS_REG_DMA_STATS_WR_HDR_OVERFLOW   mBIT(16)
#define	VXGE_HAL_DMA_ERRORS_REG_DMA_STATS_WR_HDR_UNDERFLOW  mBIT(17)
#define	VXGE_HAL_DMA_ERRORS_REG_DMA_STATS_WR_DATA_OVERFLOW  mBIT(18)
#define	VXGE_HAL_DMA_ERRORS_REG_DMA_STATS_WR_DATA_UNDERFLOW mBIT(19)
#define	VXGE_HAL_DMA_ERRORS_REG_DMA_RTDMA_WR_HDR_OVERFLOW   mBIT(20)
#define	VXGE_HAL_DMA_ERRORS_REG_DMA_RTDMA_WR_HDR_UNDERFLOW  mBIT(21)
#define	VXGE_HAL_DMA_ERRORS_REG_DMA_RTDMA_WR_DATA_OVERFLOW  mBIT(22)
#define	VXGE_HAL_DMA_ERRORS_REG_DMA_RTDMA_WR_DATA_UNDERFLOW mBIT(23)
#define	VXGE_HAL_DMA_ERRORS_REG_DMA_WRDMA_RD_HDR_OVERFLOW   mBIT(24)
#define	VXGE_HAL_DMA_ERRORS_REG_DMA_WRDMA_RD_HDR_UNDERFLOW  mBIT(25)
#define	VXGE_HAL_DMA_ERRORS_REG_DMA_RTDMA_RD_HDR_OVERFLOW   mBIT(28)
#define	VXGE_HAL_DMA_ERRORS_REG_DMA_RTDMA_RD_HDR_UNDERFLOW  mBIT(29)
#define	VXGE_HAL_DMA_ERRORS_REG_DBLGEN_FSM_ERR		    mBIT(32)
#define	VXGE_HAL_DMA_ERRORS_REG_DBLGEN_CREDIT_FSM_ERR	    mBIT(33)
#define	VXGE_HAL_DMA_ERRORS_REG_DBLGEN_DMA_WRR_SM_ERR	    mBIT(34)
/* 0x07048 */	u64	dma_errors_mask;
/* 0x07050 */	u64	dma_errors_alarm;
/* 0x07058 */	u64	tgt_errors_reg;
#define	VXGE_HAL_TGT_ERRORS_REG_TGT_VENDOR_MSG		    mBIT(0)
#define	VXGE_HAL_TGT_ERRORS_REG_TGT_MSG_UNLOCK		    mBIT(1)
#define	VXGE_HAL_TGT_ERRORS_REG_TGT_ILLEGAL_TLP_BE	    mBIT(2)
#define	VXGE_HAL_TGT_ERRORS_REG_TGT_BOOT_WRITE		    mBIT(3)
#define	VXGE_HAL_TGT_ERRORS_REG_TGT_PIF_WR_CROSS_QWRANGE    mBIT(4)
#define	VXGE_HAL_TGT_ERRORS_REG_TGT_PIF_READ_CROSS_QWRANGE  mBIT(5)
#define	VXGE_HAL_TGT_ERRORS_REG_TGT_KDFC_READ		    mBIT(6)
#define	VXGE_HAL_TGT_ERRORS_REG_TGT_USDC_READ		    mBIT(7)
#define	VXGE_HAL_TGT_ERRORS_REG_TGT_USDC_WR_CROSS_QWRANGE   mBIT(8)
#define	VXGE_HAL_TGT_ERRORS_REG_TGT_MSIX_BEYOND_RANGE	    mBIT(9)
#define	VXGE_HAL_TGT_ERRORS_REG_TGT_WR_TO_KDFC_POISON	    mBIT(10)
#define	VXGE_HAL_TGT_ERRORS_REG_TGT_WR_TO_USDC_POISON	    mBIT(11)
#define	VXGE_HAL_TGT_ERRORS_REG_TGT_WR_TO_PIF_POISON	    mBIT(12)
#define	VXGE_HAL_TGT_ERRORS_REG_TGT_WR_TO_MSIX_POISON	    mBIT(13)
#define	VXGE_HAL_TGT_ERRORS_REG_TGT_WR_TO_MRIOV_POISON	    mBIT(14)
#define	VXGE_HAL_TGT_ERRORS_REG_TGT_NOT_MEM_TLP		    mBIT(15)
#define	VXGE_HAL_TGT_ERRORS_REG_TGT_UNKNOWN_MEM_TLP	    mBIT(16)
#define	VXGE_HAL_TGT_ERRORS_REG_TGT_REQ_FSM_ERR		    mBIT(17)
#define	VXGE_HAL_TGT_ERRORS_REG_TGT_CPL_FSM_ERR		    mBIT(18)
#define	VXGE_HAL_TGT_ERRORS_REG_TGT_KDFC_PROT_ERR	    mBIT(19)
#define	VXGE_HAL_TGT_ERRORS_REG_TGT_SWIF_PROT_ERR	    mBIT(20)
#define	VXGE_HAL_TGT_ERRORS_REG_TGT_MRIOV_MEM_MAP_CFG_ERR   mBIT(21)
/* 0x07060 */	u64	tgt_errors_mask;
/* 0x07068 */	u64	tgt_errors_alarm;
/* 0x07070 */	u64	config_errors_reg;
#define	VXGE_HAL_CONFIG_ERRORS_REG_I2C_ILLEGAL_STOP_COND    mBIT(3)
#define	VXGE_HAL_CONFIG_ERRORS_REG_I2C_ILLEGAL_START_COND   mBIT(7)
#define	VXGE_HAL_CONFIG_ERRORS_REG_I2C_EXP_RD_CNT	    mBIT(11)
#define	VXGE_HAL_CONFIG_ERRORS_REG_I2C_EXTRA_CYCLE	    mBIT(15)
#define	VXGE_HAL_CONFIG_ERRORS_REG_I2C_MAIN_FSM_ERR	    mBIT(19)
#define	VXGE_HAL_CONFIG_ERRORS_REG_I2C_REQ_COLLISION	    mBIT(23)
#define	VXGE_HAL_CONFIG_ERRORS_REG_I2C_REG_FSM_ERR	    mBIT(27)
#define	VXGE_HAL_CONFIG_ERRORS_REG_CFGM_I2C_TIMEOUT	    mBIT(31)
#define	VXGE_HAL_CONFIG_ERRORS_REG_RIC_I2C_TIMEOUT	    mBIT(35)
#define	VXGE_HAL_CONFIG_ERRORS_REG_CFGM_FSM_ERR		    mBIT(39)
#define	VXGE_HAL_CONFIG_ERRORS_REG_RIC_FSM_ERR		    mBIT(43)
#define	VXGE_HAL_CONFIG_ERRORS_REG_PIFM_ILLEGAL_ACCESS	    mBIT(47)
#define	VXGE_HAL_CONFIG_ERRORS_REG_PIFM_TIMEOUT		    mBIT(51)
#define	VXGE_HAL_CONFIG_ERRORS_REG_PIFM_FSM_ERR		    mBIT(55)
#define	VXGE_HAL_CONFIG_ERRORS_REG_PIFM_TO_FSM_ERR	    mBIT(59)
#define	VXGE_HAL_CONFIG_ERRORS_REG_RIC_RIC_RD_TIMEOUT	    mBIT(63)
/* 0x07078 */	u64	config_errors_mask;
/* 0x07080 */	u64	config_errors_alarm;
	u8	unused07090[0x07090 - 0x07088];

/* 0x07090 */	u64	crdt_errors_reg;
#define	VXGE_HAL_CRDT_ERRORS_REG_WRCRDTARB_FSM_ERR	    mBIT(11)
#define	VXGE_HAL_CRDT_ERRORS_REG_WRCRDTARB_INTCTL_ILLEGAL_CRD_DEAL mBIT(15)
#define	VXGE_HAL_CRDT_ERRORS_REG_WRCRDTARB_PDA_ILLEGAL_CRD_DEAL	mBIT(19)
#define	VXGE_HAL_CRDT_ERRORS_REG_WRCRDTARB_PCI_MSG_ILLEGAL_CRD_DEAL mBIT(23)
#define	VXGE_HAL_CRDT_ERRORS_REG_RDCRDTARB_FSM_ERR	    mBIT(35)
#define	VXGE_HAL_CRDT_ERRORS_REG_RDCRDTARB_RDA_ILLEGAL_CRD_DEAL	mBIT(39)
#define	VXGE_HAL_CRDT_ERRORS_REG_RDCRDTARB_PDA_ILLEGAL_CRD_DEAL	mBIT(43)
#define	VXGE_HAL_CRDT_ERRORS_REG_RDCRDTARB_DBLGEN_ILLEGAL_CRD_DEAL mBIT(47)
/* 0x07098 */	u64	crdt_errors_mask;
/* 0x070a0 */	u64	crdt_errors_alarm;
	u8	unused070b0[0x070b0 - 0x070a8];

/* 0x070b0 */	u64	mrpcim_general_errors_reg;
#define	VXGE_HAL_MRPCIM_GENERAL_ERRORS_REG_STATSB_FSM_ERR   mBIT(3)
#define	VXGE_HAL_MRPCIM_GENERAL_ERRORS_REG_XGEN_FSM_ERR	    mBIT(7)
#define	VXGE_HAL_MRPCIM_GENERAL_ERRORS_REG_XMEM_FSM_ERR	    mBIT(11)
#define	VXGE_HAL_MRPCIM_GENERAL_ERRORS_REG_KDFCCTL_FSM_ERR  mBIT(15)
#define	VXGE_HAL_MRPCIM_GENERAL_ERRORS_REG_MRIOVCTL_FSM_ERR mBIT(19)
#define	VXGE_HAL_MRPCIM_GENERAL_ERRORS_REG_SPI_FLSH_ERR	    mBIT(23)
#define	VXGE_HAL_MRPCIM_GENERAL_ERRORS_REG_SPI_IIC_ACK_ERR  mBIT(27)
#define	VXGE_HAL_MRPCIM_GENERAL_ERRORS_REG_SPI_IIC_CHKSUM_ERR mBIT(31)
#define	VXGE_HAL_MRPCIM_GENERAL_ERRORS_REG_INI_SERR_DET	    mBIT(35)
#define	VXGE_HAL_MRPCIM_GENERAL_ERRORS_REG_INTCTL_MSIX_FSM_ERR mBIT(39)
#define	VXGE_HAL_MRPCIM_GENERAL_ERRORS_REG_INTCTL_MSI_OVERFLOW mBIT(43)
#define	VXGE_HAL_MRPCIM_GENERAL_ERRORS_REG_PPIF_PCI_NOT_FLUSH_SW_RESET\
							    mBIT(47)
#define	VXGE_HAL_MRPCIM_GENERAL_ERRORS_REG_PPIF_SW_RESET_FSM_ERR mBIT(51)
/* 0x070b8 */	u64	mrpcim_general_errors_mask;
/* 0x070c0 */	u64	mrpcim_general_errors_alarm;
	u8	unused070d0[0x070d0 - 0x070c8];

/* 0x070d0 */	u64	pll_errors_reg;
#define	VXGE_HAL_PLL_ERRORS_REG_CORE_CMG_PLL_OOL	    mBIT(3)
#define	VXGE_HAL_PLL_ERRORS_REG_CORE_FB_PLL_OOL		    mBIT(7)
#define	VXGE_HAL_PLL_ERRORS_REG_CORE_X_PLL_OOL		    mBIT(11)
/* 0x070d8 */	u64	pll_errors_mask;
/* 0x070e0 */	u64	pll_errors_alarm;
/* 0x070e8 */	u64	srpcim_to_mrpcim_alarm_reg;
#define	VXGE_HAL_SRPCIM_TO_MRPCIM_ALARM_REG_ALARM(val)	    vBIT(val, 0, 17)
/* 0x070f0 */	u64	srpcim_to_mrpcim_alarm_mask;
/* 0x070f8 */	u64	srpcim_to_mrpcim_alarm_alarm;
/* 0x07100 */	u64	vpath_to_mrpcim_alarm_reg;
#define	VXGE_HAL_VPATH_TO_MRPCIM_ALARM_REG_ALARM(val)	    vBIT(val, 0, 17)
/* 0x07108 */	u64	vpath_to_mrpcim_alarm_mask;
/* 0x07110 */	u64	vpath_to_mrpcim_alarm_alarm;
	u8	unused07128[0x07128 - 0x07118];

/* 0x07128 */	u64	crdt_errors_vplane_reg[17];
#define	VXGE_HAL_CRDT_ERRORS_VPLANE_REG_WRCRDTARB_P_H_CONSUME_CRDT_ERR	mBIT(3)
#define	VXGE_HAL_CRDT_ERRORS_VPLANE_REG_WRCRDTARB_P_D_CONSUME_CRDT_ERR	mBIT(7)
#define	VXGE_HAL_CRDT_ERRORS_VPLANE_REG_WRCRDTARB_P_H_RETURN_CRDT_ERR	mBIT(11)
#define	VXGE_HAL_CRDT_ERRORS_VPLANE_REG_WRCRDTARB_P_D_RETURN_CRDT_ERR	mBIT(15)
#define	VXGE_HAL_CRDT_ERRORS_VPLANE_REG_RDCRDTARB_NP_H_CONSUME_CRDT_ERR	mBIT(19)
#define	VXGE_HAL_CRDT_ERRORS_VPLANE_REG_RDCRDTARB_NP_H_RETURN_CRDT_ERR	mBIT(23)
#define	VXGE_HAL_CRDT_ERRORS_VPLANE_REG_RDCRDTARB_TAG_CONSUME_TAG_ERR	mBIT(27)
#define	VXGE_HAL_CRDT_ERRORS_VPLANE_REG_RDCRDTARB_TAG_RETURN_TAG_ERR	mBIT(31)
/* 0x07130 */	u64	crdt_errors_vplane_mask[17];
/* 0x07138 */	u64	crdt_errors_vplane_alarm[17];
	u8	unused072f0[0x072f0 - 0x072c0];

/* 0x072f0 */	u64	mrpcim_rst_in_prog;
#define	VXGE_HAL_MRPCIM_RST_IN_PROG_MRPCIM_RST_IN_PROG	    mBIT(7)
/* 0x072f8 */	u64	mrpcim_reg_modified;
#define	VXGE_HAL_MRPCIM_REG_MODIFIED_MRPCIM_REG_MODIFIED    mBIT(7)
/* 0x07300 */	u64	split_table_status1;
#define	VXGE_HAL_SPLIT_TABLE_STATUS1_SCPL_TAG_ENTRY1(val)   vBIT(val, 0, 64)
/* 0x07308 */	u64	split_table_status2;
#define	VXGE_HAL_SPLIT_TABLE_STATUS2_SCPL_TAG_ENTRY2(val)   vBIT(val, 0, 64)
/* 0x07310 */	u64	split_table_status3;
#define	VXGE_HAL_SPLIT_TABLE_STATUS3_SCPL_TAG_ENTRY3(val)   vBIT(val, 0, 64)
/* 0x07318 */	u64	mrpcim_general_status1;
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS1_INI_RCPL_ERRSYND(val) vBIT(val, 0, 8)
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS1_XGMAC_MISC_INT_ALARM  mBIT(11)
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS1_SCPL_NUM_OUTSTANDING_RDS(val)\
							    vBIT(val, 18, 6)
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS1_TGT_VENDOR_MSG_PAYLOAD(val)\
							    vBIT(val, 32, 32)
/* 0x07320 */	u64	mrpcim_general_status2;
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS2_CFGM_TIMEOUT_ADDR(val) vBIT(val, 6, 10)
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS2_RIC_TIMEOUT_ADDR(val) vBIT(val, 22, 10)
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS2_PIFM_ILLEGAL_CLIENT(val)\
							    vBIT(val, 34, 2)
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS2_PIFM_ILLEGAL_RD_WRN mBIT(39)
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS2_PIFM_ILLEGAL_ADDR(val) vBIT(val, 44, 20)
/* 0x07328 */	u64	mrpcim_general_status3;
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS3_PIFM_TIMEOUT_ADDR(val) vBIT(val, 0, 20)
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS3_TGT_NOT_MEM_TLP_FMT(val)\
							    vBIT(val, 21, 2)
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS3_TGT_NOT_MEM_TLP_TYPE(val)\
							    vBIT(val, 23, 5)
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS3_TGT_UNKNOWN_MEM_TLP_FMT(val)\
							    vBIT(val, 29, 2)
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS3_TGT_UNKNOWN_MEM_TLP_TYPE(val)\
							    vBIT(val, 31, 5)
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS3_RDCRDTARB_REACHED_MAX_TAGS_FOR_VPLANE0\
							    mBIT(40)
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS3_RDCRDTARB_REACHED_MAX_TAGS_FOR_VPLANE1\
							    mBIT(41)
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS3_RDCRDTARB_REACHED_MAX_TAGS_FOR_VPLANE2\
							    mBIT(42)
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS3_RDCRDTARB_REACHED_MAX_TAGS_FOR_VPLANE3\
							    mBIT(43)
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS3_RDCRDTARB_REACHED_MAX_TAGS_FOR_VPLANE4\
							    mBIT(44)
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS3_RDCRDTARB_REACHED_MAX_TAGS_FOR_VPLANE5\
							    mBIT(45)
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS3_RDCRDTARB_REACHED_MAX_TAGS_FOR_VPLANE6\
							    mBIT(46)
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS3_RDCRDTARB_REACHED_MAX_TAGS_FOR_VPLANE7\
							    mBIT(47)
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS3_RDCRDTARB_REACHED_MAX_TAGS_FOR_VPLANE8\
							    mBIT(48)
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS3_RDCRDTARB_REACHED_MAX_TAGS_FOR_VPLANE9\
							    mBIT(49)
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS3_RDCRDTARB_REACHED_MAX_TAGS_FOR_VPLANE10\
							    mBIT(50)
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS3_RDCRDTARB_REACHED_MAX_TAGS_FOR_VPLANE11\
							    mBIT(51)
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS3_RDCRDTARB_REACHED_MAX_TAGS_FOR_VPLANE12\
							    mBIT(52)
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS3_RDCRDTARB_REACHED_MAX_TAGS_FOR_VPLANE13\
							    mBIT(53)
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS3_RDCRDTARB_REACHED_MAX_TAGS_FOR_VPLANE14\
							    mBIT(54)
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS3_RDCRDTARB_REACHED_MAX_TAGS_FOR_VPLANE15\
							    mBIT(55)
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS3_RDCRDTARB_REACHED_MAX_TAGS_FOR_VPLANE16\
							    mBIT(56)
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS3_RDCRDTARB_TAGS_DEPLETED mBIT(60)
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS3_RDCRDTARB_REACHED_MAX_RDA_TAGS mBIT(61)
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS3_RDCRDTARB_REACHED_MAX_PDA_TAGS mBIT(62)
#define	VXGE_HAL_MRPCIM_GENERAL_STATUS3_RDCRDTARB_REACHED_MAX_DBLGEN_TAGS\
							    mBIT(63)
	u8	unused07338[0x07338 - 0x07330];

/* 0x07338 */	u64	test_status;
#define	VXGE_HAL_TEST_STATUS_PERR_INS_TX_WR_EP_DONE	    mBIT(3)
#define	VXGE_HAL_TEST_STATUS_PERR_INS_TX_RD_EP_DONE	    mBIT(7)
#define	VXGE_HAL_TEST_STATUS_PERR_INS_TX_CPL_EP_DONE	    mBIT(11)
#define	VXGE_HAL_TEST_STATUS_PERR_INS_TX_ECRCERR_DONE	    mBIT(15)
#define	VXGE_HAL_TEST_STATUS_PERR_INS_TX_LCRCERR_DONE	    mBIT(19)
#define	VXGE_HAL_TEST_STATUS_PERR_INS_RX_ECRCERR_DONE	    mBIT(23)
#define	VXGE_HAL_TEST_STATUS_PERR_INS_RX_LCRCERR_DONE	    mBIT(27)
	u8	unused07348[0x07348 - 0x07340];

/* 0x07348 */	u64	kdfcctl_dbg_status;
#define	VXGE_HAL_KDFCCTL_DBG_STATUS_KDFCCTL_ADDR_ERR(val)   vBIT(val, 2, 22)
#define	VXGE_HAL_KDFCCTL_DBG_STATUS_KDFCCTL_FIFO_NO_ERR(val) vBIT(val, 26, 6)
/* 0x07350 */	u64	msix_addr;
#define	VXGE_HAL_MSIX_ADDR_MSIX_ADDR(val)		    vBIT(val, 0, 64)
/* 0x07358 */	u64	msix_table;
#define	VXGE_HAL_MSIX_TABLE_DATA(val)			    vBIT(val, 0, 32)
#define	VXGE_HAL_MSIX_TABLE_MASK			    mBIT(63)
/* 0x07360 */	u64	msix_ctl;
#define	VXGE_HAL_MSIX_CTL_VECTOR_NO(val)		    vBIT(val, 1, 7)
#define	VXGE_HAL_MSIX_CTL_WRITE_OR_READ			    mBIT(15)
/* 0x07368 */	u64	msix_access_table;
#define	VXGE_HAL_MSIX_ACCESS_TABLE_MSIX_ACCESS_TABLE	    mBIT(0)
	u8	unused07378[0x07378 - 0x07370];

/* 0x07378 */	u64	write_arb_pending;
#define	VXGE_HAL_WRITE_ARB_PENDING_WRARB_WRDMA		    mBIT(3)
#define	VXGE_HAL_WRITE_ARB_PENDING_WRARB_RTDMA		    mBIT(7)
#define	VXGE_HAL_WRITE_ARB_PENDING_WRARB_MSG		    mBIT(11)
#define	VXGE_HAL_WRITE_ARB_PENDING_WRARB_STATSB		    mBIT(15)
#define	VXGE_HAL_WRITE_ARB_PENDING_WRARB_INTCTL		    mBIT(19)
/* 0x07380 */	u64	read_arb_pending;
#define	VXGE_HAL_READ_ARB_PENDING_RDARB_WRDMA		    mBIT(3)
#define	VXGE_HAL_READ_ARB_PENDING_RDARB_RTDMA		    mBIT(7)
#define	VXGE_HAL_READ_ARB_PENDING_RDARB_DBLGEN		    mBIT(11)
/* 0x07388 */	u64	dmaif_dmadbl_pending;
#define	VXGE_HAL_DMAIF_DMADBL_PENDING_DMAIF_WRDMA_WR	    mBIT(0)
#define	VXGE_HAL_DMAIF_DMADBL_PENDING_DMAIF_WRDMA_RD	    mBIT(1)
#define	VXGE_HAL_DMAIF_DMADBL_PENDING_DMAIF_RTDMA_WR	    mBIT(2)
#define	VXGE_HAL_DMAIF_DMADBL_PENDING_DMAIF_RTDMA_RD	    mBIT(3)
#define	VXGE_HAL_DMAIF_DMADBL_PENDING_DMAIF_MSG_WR	    mBIT(4)
#define	VXGE_HAL_DMAIF_DMADBL_PENDING_DMAIF_STATS_WR	    mBIT(5)
#define	VXGE_HAL_DMAIF_DMADBL_PENDING_DBLGEN_IN_PROG(val)   vBIT(val, 13, 51)
/* 0x07390 */	u64	wrcrdtarb_status0_vplane[17];
#define	VXGE_HAL_WRCRDTARB_STATUS0_VPLANE_WRCRDTARB_ABS_AVAIL_P_H(val)\
							    vBIT(val, 0, 8)
/* 0x07418 */	u64	wrcrdtarb_status1_vplane[17];
#define	VXGE_HAL_WRCRDTARB_STATUS1_VPLANE_WRCRDTARB_ABS_AVAIL_P_D(val)\
							    vBIT(val, 4, 12)
	u8	unused07500[0x07500 - 0x074a0];

/* 0x07500 */	u64	mrpcim_general_cfg1;
#define	VXGE_HAL_MRPCIM_GENERAL_CFG1_CLEAR_SERR		    mBIT(7)
/* 0x07508 */	u64	mrpcim_general_cfg2;
#define	VXGE_HAL_MRPCIM_GENERAL_CFG2_INS_TX_WR_TD	    mBIT(3)
#define	VXGE_HAL_MRPCIM_GENERAL_CFG2_INS_TX_RD_TD	    mBIT(7)
#define	VXGE_HAL_MRPCIM_GENERAL_CFG2_INS_TX_CPL_TD	    mBIT(11)
#define	VXGE_HAL_MRPCIM_GENERAL_CFG2_INI_TIMEOUT_EN_MWR	    mBIT(15)
#define	VXGE_HAL_MRPCIM_GENERAL_CFG2_INI_TIMEOUT_EN_MRD	    mBIT(19)
#define	VXGE_HAL_MRPCIM_GENERAL_CFG2_IGNORE_VPATH_RST_FOR_MSIX mBIT(23)
#define	VXGE_HAL_MRPCIM_GENERAL_CFG2_FLASH_READ_MSB	    mBIT(27)
#define	VXGE_HAL_MRPCIM_GENERAL_CFG2_DIS_HOST_PIPELINE_WR   mBIT(31)
#define	VXGE_HAL_MRPCIM_GENERAL_CFG2_MRPCIM_STATS_ENABLE    mBIT(43)
#define	VXGE_HAL_MRPCIM_GENERAL_CFG2_MRPCIM_STATS_MAP_TO_VPATH(val)\
							    vBIT(val, 47, 5)
#define	VXGE_HAL_MRPCIM_GENERAL_CFG2_EN_BLOCK_MSIX_DUE_TO_SERR mBIT(55)
#define	VXGE_HAL_MRPCIM_GENERAL_CFG2_FORCE_SENDING_INTA	    mBIT(59)
#define	VXGE_HAL_MRPCIM_GENERAL_CFG2_DIS_SWIF_PROT_ON_RDS   mBIT(63)
/* 0x07510 */	u64	mrpcim_general_cfg3;
#define	VXGE_HAL_MRPCIM_GENERAL_CFG3_PROTECTION_CA_OR_UNSUPN mBIT(0)
#define	VXGE_HAL_MRPCIM_GENERAL_CFG3_ILLEGAL_RD_CA_OR_UNSUPN mBIT(3)
#define	VXGE_HAL_MRPCIM_GENERAL_CFG3_RD_BYTE_SWAPEN	    mBIT(7)
#define	VXGE_HAL_MRPCIM_GENERAL_CFG3_RD_BIT_FLIPEN	    mBIT(11)
#define	VXGE_HAL_MRPCIM_GENERAL_CFG3_WR_BYTE_SWAPEN	    mBIT(15)
#define	VXGE_HAL_MRPCIM_GENERAL_CFG3_WR_BIT_FLIPEN	    mBIT(19)
#define	VXGE_HAL_MRPCIM_GENERAL_CFG3_MR_MAX_MVFS(val)	    vBIT(val, 20, 16)
#define	VXGE_HAL_MRPCIM_GENERAL_CFG3_MR_MVF_TBL_SIZE(val)   vBIT(val, 36, 16)
#define	VXGE_HAL_MRPCIM_GENERAL_CFG3_PF0_SW_RESET_EN	    mBIT(55)
#define	VXGE_HAL_MRPCIM_GENERAL_CFG3_REG_MODIFIED_CFG(val)  vBIT(val, 56, 2)
#define	VXGE_HAL_MRPCIM_GENERAL_CFG3_CPL_ECC_ENABLE_N	    mBIT(59)
#define	VXGE_HAL_MRPCIM_GENERAL_CFG3_BYPASS_DAISY_CHAIN	    mBIT(63)
/* 0x07518 */	u64	mrpcim_stats_start_host_addr;
#define	VXGE_HAL_MRPCIM_STATS_START_HOST_ADDR_MRPCIM_STATS_START_HOST_ADDR(val)\
							    vBIT(val, 0, 57)
/* 0x07520 */	u64	asic_mode;
#define	VXGE_HAL_ASIC_MODE_PIC(val)			    vBIT(val, 2, 2)
/* 0x07528 */	u64	dis_fw_pipeline_wr;
#define	VXGE_HAL_DIS_FW_PIPELINE_WR_DIS_FW_PIPELINE_WR	    mBIT(0)
/* 0x07530 */	u64	ini_timeout_val;
#define	VXGE_HAL_INI_TIMEOUT_VAL_MWR(val)		    vBIT(val, 0, 32)
#define	VXGE_HAL_INI_TIMEOUT_VAL_MRD(val)		    vBIT(val, 32, 32)
/* 0x07538 */	u64	pic_arbiter_cfg;
#define	VXGE_HAL_PIC_ARBITER_CFG_DMA_READ_EN		    mBIT(3)
#define	VXGE_HAL_PIC_ARBITER_CFG_DMA_WRITE_EN		    mBIT(7)
#define	VXGE_HAL_PIC_ARBITER_CFG_DBLGEN_WRR_EN		    mBIT(11)
#define	VXGE_HAL_PIC_ARBITER_CFG_WRCRDTARB_EN		    mBIT(15)
#define	VXGE_HAL_PIC_ARBITER_CFG_RDCRDTARB_EN		    mBIT(19)
/* 0x07540 */	u64	read_arbiter;
#define	VXGE_HAL_READ_ARBITER_WRDMA_PRIORITY(val)	    vBIT(val, 2, 2)
#define	VXGE_HAL_READ_ARBITER_RTDMA_PRIORITY(val)	    vBIT(val, 6, 2)
#define	VXGE_HAL_READ_ARBITER_DBLGEN_PRIORITY(val)	    vBIT(val, 10, 2)
#define	VXGE_HAL_READ_ARBITER_CALSTATE0_PRIORITY(val)	    vBIT(val, 14, 2)
#define	VXGE_HAL_READ_ARBITER_CALSTATE1_PRIORITY(val)	    vBIT(val, 18, 2)
#define	VXGE_HAL_READ_ARBITER_CALSTATE2_PRIORITY(val)	    vBIT(val, 22, 2)
#define	VXGE_HAL_READ_ARBITER_CALSTATE3_PRIORITY(val)	    vBIT(val, 26, 2)
#define	VXGE_HAL_READ_ARBITER_CALSTATE4_PRIORITY(val)	    vBIT(val, 30, 2)
#define	VXGE_HAL_READ_ARBITER_CALSTATE5_PRIORITY(val)	    vBIT(val, 34, 2)
#define	VXGE_HAL_READ_ARBITER_CHECK_PRIORITY_MATCH_ONLY	    mBIT(39)
/* 0x07548 */	u64	write_arbiter;
#define	VXGE_HAL_WRITE_ARBITER_WRDMA_PRIORITY(val)	    vBIT(val, 2, 2)
#define	VXGE_HAL_WRITE_ARBITER_RTDMA_PRIORITY(val)	    vBIT(val, 6, 2)
#define	VXGE_HAL_WRITE_ARBITER_STATS_PRIORITY(val)	    vBIT(val, 10, 2)
#define	VXGE_HAL_WRITE_ARBITER_MSG_PRIORITY(val)	    vBIT(val, 14, 2)
#define	VXGE_HAL_WRITE_ARBITER_CALSTATE0_PRIORITY(val)	    vBIT(val, 18, 2)
#define	VXGE_HAL_WRITE_ARBITER_CALSTATE1_PRIORITY(val)	    vBIT(val, 22, 2)
#define	VXGE_HAL_WRITE_ARBITER_CALSTATE2_PRIORITY(val)	    vBIT(val, 26, 2)
#define	VXGE_HAL_WRITE_ARBITER_CALSTATE3_PRIORITY(val)	    vBIT(val, 30, 2)
#define	VXGE_HAL_WRITE_ARBITER_CALSTATE4_PRIORITY(val)	    vBIT(val, 34, 2)
#define	VXGE_HAL_WRITE_ARBITER_CALSTATE5_PRIORITY(val)	    vBIT(val, 38, 2)
#define	VXGE_HAL_WRITE_ARBITER_CALSTATE6_PRIORITY(val)	    vBIT(val, 42, 2)
#define	VXGE_HAL_WRITE_ARBITER_CALSTATE7_PRIORITY(val)	    vBIT(val, 46, 2)
#define	VXGE_HAL_WRITE_ARBITER_CALSTATE8_PRIORITY(val)	    vBIT(val, 50, 2)
#define	VXGE_HAL_WRITE_ARBITER_CALSTATE9_PRIORITY(val)	    vBIT(val, 52, 2)
#define	VXGE_HAL_WRITE_ARBITER_CHECK_PRIORITY_MATCH_ONLY    mBIT(55)
/* 0x07550 */	u64	adapter_control;
#define	VXGE_HAL_ADAPTER_CONTROL_ADAPTER_EN		    mBIT(7)
#define	VXGE_HAL_ADAPTER_CONTROL_DISABLE_RIC		    mBIT(49)
#define	VXGE_HAL_ADAPTER_CONTROL_ECC_ENABLE_N		    mBIT(55)
/* 0x07558 */	u64	program_cfg0;
#define	VXGE_HAL_PROGRAM_CFG0_I2C_SLAVE_ADDR(val)	    vBIT(val, 1, 7)
#define	VXGE_HAL_PROGRAM_CFG0_CFGM_TIMEOUT_EN		    mBIT(11)
#define	VXGE_HAL_PROGRAM_CFG0_PIFM_TIMEOUT_EN		    mBIT(15)
/* 0x07560 */	u64	program_cfg1;
#define	VXGE_HAL_PROGRAM_CFG1_CFGM_TIMEOUT_LOAD_VAL(val)    vBIT(val, 0, 32)
#define	VXGE_HAL_PROGRAM_CFG1_PIFM_TIMEOUT_LOAD_VAL(val)    vBIT(val, 32, 32)
/* 0x07568 */	u64	dblgen_wrr_cfg1;
#define	VXGE_HAL_DBLGEN_WRR_CFG1_CTRL_SS_0_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG1_CTRL_SS_1_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG1_CTRL_SS_2_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG1_CTRL_SS_3_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG1_CTRL_SS_4_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG1_CTRL_SS_5_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG1_CTRL_SS_6_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG1_CTRL_SS_7_NUM(val)	    vBIT(val, 59, 5)
/* 0x07570 */	u64	dblgen_wrr_cfg2;
#define	VXGE_HAL_DBLGEN_WRR_CFG2_CTRL_SS_8_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG2_CTRL_SS_9_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG2_CTRL_SS_10_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG2_CTRL_SS_11_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG2_CTRL_SS_12_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG2_CTRL_SS_13_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG2_CTRL_SS_14_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG2_CTRL_SS_15_NUM(val)	    vBIT(val, 59, 5)
/* 0x07578 */	u64	dblgen_wrr_cfg3;
#define	VXGE_HAL_DBLGEN_WRR_CFG3_CTRL_SS_16_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG3_CTRL_SS_17_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG3_CTRL_SS_18_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG3_CTRL_SS_19_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG3_CTRL_SS_20_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG3_CTRL_SS_21_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG3_CTRL_SS_22_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG3_CTRL_SS_23_NUM(val)	    vBIT(val, 59, 5)
/* 0x07580 */	u64	dblgen_wrr_cfg4;
#define	VXGE_HAL_DBLGEN_WRR_CFG4_CTRL_SS_24_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG4_CTRL_SS_25_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG4_CTRL_SS_26_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG4_CTRL_SS_27_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG4_CTRL_SS_28_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG4_CTRL_SS_29_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG4_CTRL_SS_30_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG4_CTRL_SS_31_NUM(val)	    vBIT(val, 59, 5)
/* 0x07588 */	u64	dblgen_wrr_cfg5;
#define	VXGE_HAL_DBLGEN_WRR_CFG5_CTRL_SS_32_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG5_CTRL_SS_33_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG5_CTRL_SS_34_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG5_CTRL_SS_35_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG5_CTRL_SS_36_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG5_CTRL_SS_37_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG5_CTRL_SS_38_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG5_CTRL_SS_39_NUM(val)	    vBIT(val, 59, 5)
/* 0x07590 */	u64	dblgen_wrr_cfg6;
#define	VXGE_HAL_DBLGEN_WRR_CFG6_CTRL_SS_40_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG6_CTRL_SS_41_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG6_CTRL_SS_42_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG6_CTRL_SS_43_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG6_CTRL_SS_44_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG6_CTRL_SS_45_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG6_CTRL_SS_46_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG6_CTRL_SS_47_NUM(val)	    vBIT(val, 59, 5)
/* 0x07598 */	u64	dblgen_wrr_cfg7;
#define	VXGE_HAL_DBLGEN_WRR_CFG7_CTRL_SS_48_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG7_CTRL_SS_49_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG7_CTRL_SS_50_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG7_CTRL_SS_51_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG7_CTRL_SS_52_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG7_CTRL_SS_53_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG7_CTRL_SS_54_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG7_CTRL_SS_55_NUM(val)	    vBIT(val, 59, 5)
/* 0x075a0 */	u64	dblgen_wrr_cfg8;
#define	VXGE_HAL_DBLGEN_WRR_CFG8_CTRL_SS_56_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG8_CTRL_SS_57_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG8_CTRL_SS_58_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG8_CTRL_SS_59_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG8_CTRL_SS_60_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG8_CTRL_SS_61_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG8_CTRL_SS_62_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG8_CTRL_SS_63_NUM(val)	    vBIT(val, 59, 5)
/* 0x075a8 */	u64	dblgen_wrr_cfg9;
#define	VXGE_HAL_DBLGEN_WRR_CFG9_CTRL_SS_64_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG9_CTRL_SS_65_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG9_CTRL_SS_66_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG9_CTRL_SS_67_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG9_CTRL_SS_68_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG9_CTRL_SS_69_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG9_CTRL_SS_70_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG9_CTRL_SS_71_NUM(val)	    vBIT(val, 59, 5)
/* 0x075b0 */	u64	dblgen_wrr_cfg10;
#define	VXGE_HAL_DBLGEN_WRR_CFG10_CTRL_SS_72_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG10_CTRL_SS_73_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG10_CTRL_SS_74_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG10_CTRL_SS_75_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG10_CTRL_SS_76_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG10_CTRL_SS_77_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG10_CTRL_SS_78_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG10_CTRL_SS_79_NUM(val)	    vBIT(val, 59, 5)
/* 0x075b8 */	u64	dblgen_wrr_cfg11;
#define	VXGE_HAL_DBLGEN_WRR_CFG11_CTRL_SS_80_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG11_CTRL_SS_81_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG11_CTRL_SS_82_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG11_CTRL_SS_83_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG11_CTRL_SS_84_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG11_CTRL_SS_85_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG11_CTRL_SS_86_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG11_CTRL_SS_87_NUM(val)	    vBIT(val, 59, 5)
/* 0x075c0 */	u64	dblgen_wrr_cfg12;
#define	VXGE_HAL_DBLGEN_WRR_CFG12_CTRL_SS_88_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG12_CTRL_SS_89_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG12_CTRL_SS_90_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG12_CTRL_SS_91_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG12_CTRL_SS_92_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG12_CTRL_SS_93_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG12_CTRL_SS_94_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG12_CTRL_SS_95_NUM(val)	    vBIT(val, 59, 5)
/* 0x075c8 */	u64	dblgen_wrr_cfg13;
#define	VXGE_HAL_DBLGEN_WRR_CFG13_CTRL_SS_96_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG13_CTRL_SS_97_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG13_CTRL_SS_98_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG13_CTRL_SS_99_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG13_CTRL_SS_100_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG13_CTRL_SS_101_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG13_CTRL_SS_102_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG13_CTRL_SS_103_NUM(val)	    vBIT(val, 59, 5)
/* 0x075d0 */	u64	dblgen_wrr_cfg14;
#define	VXGE_HAL_DBLGEN_WRR_CFG14_CTRL_SS_104_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG14_CTRL_SS_105_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG14_CTRL_SS_106_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG14_CTRL_SS_107_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG14_CTRL_SS_108_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG14_CTRL_SS_109_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG14_CTRL_SS_110_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG14_CTRL_SS_111_NUM(val)	    vBIT(val, 59, 5)
/* 0x075d8 */	u64	dblgen_wrr_cfg15;
#define	VXGE_HAL_DBLGEN_WRR_CFG15_CTRL_SS_112_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG15_CTRL_SS_113_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG15_CTRL_SS_114_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG15_CTRL_SS_115_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG15_CTRL_SS_116_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG15_CTRL_SS_117_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG15_CTRL_SS_118_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG15_CTRL_SS_119_NUM(val)	    vBIT(val, 59, 5)
/* 0x075e0 */	u64	dblgen_wrr_cfg16;
#define	VXGE_HAL_DBLGEN_WRR_CFG16_CTRL_SS_120_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG16_CTRL_SS_121_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG16_CTRL_SS_122_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG16_CTRL_SS_123_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG16_CTRL_SS_124_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG16_CTRL_SS_125_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG16_CTRL_SS_126_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG16_CTRL_SS_127_NUM(val)	    vBIT(val, 59, 5)
/* 0x075e8 */	u64	dblgen_wrr_cfg17;
#define	VXGE_HAL_DBLGEN_WRR_CFG17_CTRL_SS_128_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG17_CTRL_SS_129_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG17_CTRL_SS_130_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG17_CTRL_SS_131_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG17_CTRL_SS_132_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG17_CTRL_SS_133_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG17_CTRL_SS_134_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG17_CTRL_SS_135_NUM(val)	    vBIT(val, 59, 5)
/* 0x075f0 */	u64	dblgen_wrr_cfg18;
#define	VXGE_HAL_DBLGEN_WRR_CFG18_CTRL_SS_136_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG18_CTRL_SS_137_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG18_CTRL_SS_138_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG18_CTRL_SS_139_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG18_CTRL_SS_140_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG18_CTRL_SS_141_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG18_CTRL_SS_142_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG18_CTRL_SS_143_NUM(val)	    vBIT(val, 59, 5)
/* 0x075f8 */	u64	dblgen_wrr_cfg19;
#define	VXGE_HAL_DBLGEN_WRR_CFG19_CTRL_SS_144_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG19_CTRL_SS_145_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG19_CTRL_SS_146_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG19_CTRL_SS_147_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG19_CTRL_SS_148_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG19_CTRL_SS_149_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG19_CTRL_SS_150_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_DBLGEN_WRR_CFG19_CTRL_SS_151_NUM(val)	    vBIT(val, 59, 5)
/* 0x07600 */	u64	dblgen_wrr_cfg20;
#define	VXGE_HAL_DBLGEN_WRR_CFG20_CTRL_SS_152_NUM(val)	    vBIT(val, 3, 5)
/* 0x07608 */	u64	debug_cfg1;
#define	VXGE_HAL_DEBUG_CFG1_TAG_TO_OBSERVE(val)		    vBIT(val, 3, 5)
#define	VXGE_HAL_DEBUG_CFG1_DIS_REL_OF_TAG_DUE_TO_ERR	    mBIT(11)
	u8	unused07900[0x07900 - 0x07610];

/* 0x07900 */	u64	test_cfg1;
#define	VXGE_HAL_TEST_CFG1_PERR_INS_TX_WR_EP		    mBIT(19)
#define	VXGE_HAL_TEST_CFG1_PERR_INS_TX_RD_EP		    mBIT(23)
#define	VXGE_HAL_TEST_CFG1_PERR_INS_TX_CPL_EP		    mBIT(27)
#define	VXGE_HAL_TEST_CFG1_PERR_INS_TX_ECRCERR		    mBIT(31)
#define	VXGE_HAL_TEST_CFG1_PERR_INS_TX_LCRCERR		    mBIT(35)
#define	VXGE_HAL_TEST_CFG1_PERR_INS_RX_ECRCERR		    mBIT(39)
#define	VXGE_HAL_TEST_CFG1_PERR_INS_RX_LCRCERR		    mBIT(43)
/* 0x07908 */	u64	test_cfg2;
#define	VXGE_HAL_TEST_CFG2_PERR_TIMEOUT_VAL(val)	    vBIT(val, 0, 32)
/* 0x07910 */	u64	test_cfg3;
#define	VXGE_HAL_TEST_CFG3_PERR_TRIGGER_TIMER		    mBIT(0)
/* 0x07918 */	u64	wrcrdtarb_cfg0;
#define	VXGE_HAL_WRCRDTARB_CFG0_WAIT_CNT(val)		    vBIT(val, 48, 4)
#define	VXGE_HAL_WRCRDTARB_CFG0_STATS_PRTY_TIMEOUT_EN	    mBIT(55)
#define	VXGE_HAL_WRCRDTARB_CFG0_STATS_DROP_TIMEOUT_EN	    mBIT(59)
#define	VXGE_HAL_WRCRDTARB_CFG0_EN_XON			    mBIT(63)
/* 0x07920 */	u64	wrcrdtarb_cfg1;
#define	VXGE_HAL_WRCRDTARB_CFG1_RST_CREDIT		    mBIT(0)
/* 0x07928 */	u64	wrcrdtarb_cfg2;
#define	VXGE_HAL_WRCRDTARB_CFG2_STATS_PRTY_TIMEOUT_VAL(val) vBIT(val, 0, 32)
#define	VXGE_HAL_WRCRDTARB_CFG2_STATS_DROP_TIMEOUT_VAL(val) vBIT(val, 32, 32)
/* 0x07930 */	u64	test_wrcrdtarb_cfg1;
#define	VXGE_HAL_TEST_WRCRDTARB_CFG1_BLOCK_VPLANE_TIMEOUT1_VAL(val)\
							    vBIT(val, 0, 32)
#define	VXGE_HAL_TEST_WRCRDTARB_CFG1_BLOCK_VPLANE_TIMEOUT2_VAL(val)\
							    vBIT(val, 32, 32)
/* 0x07938 */	u64	test_wrcrdtarb_cfg2;
#define	VXGE_HAL_TEST_WRCRDTARB_CFG2_BLOCK_VPLANE_TIMEOUT3_VAL(val)\
							    vBIT(val, 0, 32)
#define	VXGE_HAL_TEST_WRCRDTARB_CFG2_BLOCK_VPLANE_TIMEOUT4_VAL(val)\
							    vBIT(val, 32, 32)
/* 0x07940 */	u64	test_wrcrdtarb_cfg3;
#define	VXGE_HAL_TEST_WRCRDTARB_CFG3_TIMEOUT1_MAP(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_TEST_WRCRDTARB_CFG3_TIMEOUT2_MAP(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_TEST_WRCRDTARB_CFG3_TIMEOUT3_MAP(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_TEST_WRCRDTARB_CFG3_TIMEOUT4_MAP(val)	    vBIT(val, 27, 5)
/* 0x07948 */	u64	test_wrcrdtarb_cfg4;
#define	VXGE_HAL_TEST_WRCRDTARB_CFG4_BLOCK_VPLANE_TIMEOUT1_EN mBIT(3)
#define	VXGE_HAL_TEST_WRCRDTARB_CFG4_BLOCK_VPLANE_TIMEOUT2_EN mBIT(7)
#define	VXGE_HAL_TEST_WRCRDTARB_CFG4_BLOCK_VPLANE_TIMEOUT3_EN mBIT(11)
#define	VXGE_HAL_TEST_WRCRDTARB_CFG4_BLOCK_VPLANE_TIMEOUT4_EN mBIT(15)
/* 0x07950 */	u64	rdcrdtarb_cfg0;
#define	VXGE_HAL_RDCRDTARB_CFG0_RDA_MAX_OUTSTANDING_RDS(val) vBIT(val, 18, 6)
#define	VXGE_HAL_RDCRDTARB_CFG0_PDA_MAX_OUTSTANDING_RDS(val) vBIT(val, 26, 6)
#define	VXGE_HAL_RDCRDTARB_CFG0_DBLGEN_MAX_OUTSTANDING_RDS(val) vBIT(val, 34, 6)
#define	VXGE_HAL_RDCRDTARB_CFG0_WAIT_CNT(val)		    vBIT(val, 48, 4)
#define	VXGE_HAL_RDCRDTARB_CFG0_MAX_OUTSTANDING_RDS(val)    vBIT(val, 54, 6)
#define	VXGE_HAL_RDCRDTARB_CFG0_EN_XON			    mBIT(63)
/* 0x07958 */	u64	rdcrdtarb_cfg1;
#define	VXGE_HAL_RDCRDTARB_CFG1_RST_CREDIT		    mBIT(0)
/* 0x07960 */	u64	rdcrdtarb_cfg2;
#define	VXGE_HAL_RDCRDTARB_CFG2_SOFTNAK_TIMER_VAL_DIV4(val) vBIT(val, 0, 32)
/* 0x07968 */	u64	test_rdcrdtarb_cfg1;
#define	VXGE_HAL_TEST_RDCRDTARB_CFG1_BLOCK_VPLANE_TIMEOUT1_VAL(val)\
							    vBIT(val, 0, 32)
#define	VXGE_HAL_TEST_RDCRDTARB_CFG1_BLOCK_VPLANE_TIMEOUT2_VAL(val)\
							    vBIT(val, 32, 32)
/* 0x07970 */	u64	test_rdcrdtarb_cfg2;
#define	VXGE_HAL_TEST_RDCRDTARB_CFG2_BLOCK_VPLANE_TIMEOUT3_VAL(val)\
							    vBIT(val, 0, 32)
#define	VXGE_HAL_TEST_RDCRDTARB_CFG2_BLOCK_VPLANE_TIMEOUT4_VAL(val)\
							    vBIT(val, 32, 32)
/* 0x07978 */	u64	test_rdcrdtarb_cfg3;
#define	VXGE_HAL_TEST_RDCRDTARB_CFG3_TIMEOUT1_MAP(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_TEST_RDCRDTARB_CFG3_TIMEOUT2_MAP(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_TEST_RDCRDTARB_CFG3_TIMEOUT3_MAP(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_TEST_RDCRDTARB_CFG3_TIMEOUT4_MAP(val)	    vBIT(val, 27, 5)
/* 0x07980 */	u64	test_rdcrdtarb_cfg4;
#define	VXGE_HAL_TEST_RDCRDTARB_CFG4_BLOCK_VPLANE_TIMEOUT1_EN mBIT(3)
#define	VXGE_HAL_TEST_RDCRDTARB_CFG4_BLOCK_VPLANE_TIMEOUT2_EN mBIT(7)
#define	VXGE_HAL_TEST_RDCRDTARB_CFG4_BLOCK_VPLANE_TIMEOUT3_EN mBIT(11)
#define	VXGE_HAL_TEST_RDCRDTARB_CFG4_BLOCK_VPLANE_TIMEOUT4_EN mBIT(15)
/* 0x07988 */	u64	pic_debug_control;
#define	VXGE_HAL_PIC_DEBUG_CONTROL_DBG_ALL_CLKA_SEL(val)    vBIT(val, 0, 4)
#define	VXGE_HAL_PIC_DEBUG_CONTROL_DBG_ALL_CLKB_SEL(val)    vBIT(val, 4, 4)
#define	VXGE_HAL_PIC_DEBUG_CONTROL_DBG_ALL_DA_SEL(val)	    vBIT(val, 10, 6)
#define	VXGE_HAL_PIC_DEBUG_CONTROL_DBG_ALL_DB_SEL(val)	    vBIT(val, 18, 6)
#define	VXGE_HAL_PIC_DEBUG_CONTROL_DBGA_SEL(val)	    vBIT(val, 28, 4)
#define	VXGE_HAL_PIC_DEBUG_CONTROL_DBGB_SEL(val)	    vBIT(val, 32, 4)
	u8	unused079d8[0x079d8 - 0x07990];

/* 0x079d8 */	u64	spi_control_3_reg;
#define	VXGE_HAL_SPI_CONTROL_3_REG_SECTOR_0_WR_EN(val)	    vBIT(val, 0, 32)
/* 0x079e0 */	u64	clock_cfg0;
#define	VXGE_HAL_CLOCK_CFG0_ONE_LRO_EN			    mBIT(3)
#define	VXGE_HAL_CLOCK_CFG0_ONE_IWARP_EN		    mBIT(7)
/* 0x079e8 */	u64	stats_bp_ctrl;
#define	VXGE_HAL_STATS_BP_CTRL_WR_XON			    mBIT(7)
/* 0x079f0 */	u64	kdfcdma_bp_ctrl;
#define	VXGE_HAL_KDFCDMA_BP_CTRL_RD_XON			    mBIT(3)
/* 0x079f8 */	u64	intctl_bp_ctrl;
#define	VXGE_HAL_INTCTL_BP_CTRL_WR_XON			    mBIT(3)
/* 0x07a00 */	u64	vector_srpcim_alarm_map[9];
#define	VXGE_HAL_VECTOR_SRPCIM_ALARM_MAP_VECTOR_SRPCIM_ALARM_MAP(val)\
							    vBIT(val, 17, 7)
	u8	unused07b10[0x07b10 - 0x07a48];

/* 0x07b10 */	u64	vplane_rdcrdtarb_cfg0[17];
#define	VXGE_HAL_VPLANE_RDCRDTARB_CFG0_TAGS_THRESHOLD_XOFF(val)\
							    vBIT(val, 27, 5)
#define	VXGE_HAL_VPLANE_RDCRDTARB_CFG0_MAX_OUTSTANDING_RDS(val)\
							    vBIT(val, 34, 6)
	u8	unused07ba0[0x07ba0 - 0x07b98];

/* 0x07ba0 */	u64	mrpcim_spi_control;
#define	VXGE_HAL_MRPCIM_SPI_CONTROL_KEY(val)		    vBIT(val, 0, 4)
#define	VXGE_HAL_MRPCIM_SPI_CONTROL_SEL1		    mBIT(4)
#define	VXGE_HAL_MRPCIM_SPI_CONTROL_NACK		    mBIT(5)
#define	VXGE_HAL_MRPCIM_SPI_CONTROL_DONE		    mBIT(6)
#define	VXGE_HAL_MRPCIM_SPI_CONTROL_REQ			    mBIT(7)
#define	VXGE_HAL_MRPCIM_SPI_CONTROL_BYTE_CNT(val)	    vBIT(val, 29, 3)
#define	VXGE_HAL_MRPCIM_SPI_CONTROL_CMD(val)		    vBIT(val, 32, 8)
#define	VXGE_HAL_MRPCIM_SPI_CONTROL_ADD(val)		    vBIT(val, 40, 24)
/* 0x07ba8 */	u64	mrpcim_spi_data;
#define	VXGE_HAL_MRPCIM_SPI_DATA_SPI_RWDATA(val)	    vBIT(val, 0, 64)
/* 0x07bb0 */	u64	mrpcim_spi_write_protect;
#define	VXGE_HAL_MRPCIM_SPI_WRITE_PROTECT_HWPE		    mBIT(7)
#define	VXGE_HAL_MRPCIM_SPI_WRITE_PROTECT_SPI_16ADDR_EN	    mBIT(14)
#define	VXGE_HAL_MRPCIM_SPI_WRITE_PROTECT_SPI_2DEV_EN	    mBIT(15)
#define	VXGE_HAL_MRPCIM_SPI_WRITE_PROTECT_SLOWCK	    mBIT(63)
	u8	unused07be0[0x07be0 - 0x07bb8];

/* 0x07be0 */	u64	chip_full_reset;
#define	VXGE_HAL_CHIP_FULL_RESET_CHIP_FULL_RESET(val)	    vBIT(val, 0, 8)
/* 0x07be8 */	u64	bf_sw_reset;
#define	VXGE_HAL_BF_SW_RESET_BF_SW_RESET(val)		    vBIT(val, 0, 8)
/* 0x07bf0 */	u64	sw_reset_status;
#define	VXGE_HAL_SW_RESET_STATUS_RESET_CMPLT		    mBIT(7)
#define	VXGE_HAL_SW_RESET_STATUS_INIT_CMPLT		    mBIT(15)
	u8	unused07c28[0x07c20 - 0x07bf8];

/* 0x07c20 */	u64	sw_reset_cfg1;
#define	VXGE_HAL_SW_RESET_CFG1_TYPE			    mBIT(0)
#define	VXGE_HAL_SW_RESET_CFG1_WAIT_TIME_FOR_FLUSH_PCI(val) vBIT(val, 7, 25)
#define	VXGE_HAL_SW_RESET_CFG1_SOPR_ASSERT_TIME(val)	    vBIT(val, 32, 4)
#define	VXGE_HAL_SW_RESET_CFG1_WAIT_TIME_AFTER_RESET(val)   vBIT(val, 38, 25)
/* 0x07c28 */	u64	ric_timeout;
#define	VXGE_HAL_RIC_TIMEOUT_EN				    mBIT(3)
#define	VXGE_HAL_RIC_TIMEOUT_VAL(val)			    vBIT(val, 32, 32)
/* 0x07c30 */	u64	mrpcim_pci_config_access_cfg1;
#define	VXGE_HAL_MRPCIM_PCI_CONFIG_ACCESS_CFG1_ADDRESS(val) vBIT(val, 4, 10)
#define	VXGE_HAL_MRPCIM_PCI_CONFIG_ACCESS_CFG1_VPLANE(val)  vBIT(val, 19, 5)
#define	VXGE_HAL_MRPCIM_PCI_CONFIG_ACCESS_CFG1_FUNC(val)    vBIT(val, 27, 5)
#define	VXGE_HAL_MRPCIM_PCI_CONFIG_ACCESS_CFG1_RD_OR_WRN    mBIT(39)
/* 0x07c38 */	u64	mrpcim_pci_config_access_cfg2;
#define	VXGE_HAL_MRPCIM_PCI_CONFIG_ACCESS_CFG2_REQ	    mBIT(0)
/* 0x07c40 */	u64	mrpcim_pci_config_access_status;
#define	VXGE_HAL_MRPCIM_PCI_CONFIG_ACCESS_STATUS_ACCESS_ERR mBIT(0)
#define	VXGE_HAL_MRPCIM_PCI_CONFIG_ACCESS_STATUS_DATA(val)  vBIT(val, 32, 32)
	u8	unused07ca8[0x07ca8 - 0x07c48];

/* 0x07ca8 */	u64	rdcrdtarb_status0_vplane[17];
#define	VXGE_HAL_RDCRDTARB_STATUS0_VPLANE_RDCRDTARB_ABS_AVAIL_NP_H(val)\
							    vBIT(val, 0, 8)
/* 0x07d30 */	u64	mrpcim_debug_stats0;
#define	VXGE_HAL_MRPCIM_DEBUG_STATS0_INI_WR_DROP(val)	    vBIT(val, 0, 32)
#define	VXGE_HAL_MRPCIM_DEBUG_STATS0_INI_RD_DROP(val)	    vBIT(val, 32, 32)
/* 0x07d38 */	u64	mrpcim_debug_stats1_vplane[17];
#define	VXGE_HAL_MRPCIM_DEBUG_STATS1_VPLANE_WRCRDTARB_PH_CRDT_DEPLETED(val)\
							    vBIT(val, 32, 32)
/* 0x07dc0 */	u64	mrpcim_debug_stats2_vplane[17];
#define	VXGE_HAL_MRPCIM_DEBUG_STATS2_VPLANE_WRCRDTARB_PD_CRDT_DEPLETED(val)\
							    vBIT(val, 32, 32)
/* 0x07e48 */	u64	mrpcim_debug_stats3_vplane[17];
#define	VXGE_HAL_MRPCIM_DEBUG_STATS3_VPLANE_RDCRDTARB_NPH_CRDT_DEPLETED(val)\
							    vBIT(val, 32, 32)
/* 0x07ed0 */	u64	mrpcim_debug_stats4;
#define	VXGE_HAL_MRPCIM_DEBUG_STATS4_INI_WR_VPIN_DROP(val)  vBIT(val, 0, 32)
#define	VXGE_HAL_MRPCIM_DEBUG_STATS4_INI_RD_VPIN_DROP(val)  vBIT(val, 32, 32)
/* 0x07ed8 */	u64	genstats_count01;
#define	VXGE_HAL_GENSTATS_COUNT01_GENSTATS_COUNT1(val)	    vBIT(val, 0, 32)
#define	VXGE_HAL_GENSTATS_COUNT01_GENSTATS_COUNT0(val)	    vBIT(val, 32, 32)
/* 0x07ee0 */	u64	genstats_count23;
#define	VXGE_HAL_GENSTATS_COUNT23_GENSTATS_COUNT3(val)	    vBIT(val, 0, 32)
#define	VXGE_HAL_GENSTATS_COUNT23_GENSTATS_COUNT2(val)	    vBIT(val, 32, 32)
/* 0x07ee8 */	u64	genstats_count4;
#define	VXGE_HAL_GENSTATS_COUNT4_GENSTATS_COUNT4(val)	    vBIT(val, 32, 32)
/* 0x07ef0 */	u64	genstats_count5;
#define	VXGE_HAL_GENSTATS_COUNT5_GENSTATS_COUNT5(val)	    vBIT(val, 32, 32)
/* 0x07ef8 */	u64	mrpcim_mmio_cfg1;
#define	VXGE_HAL_MRPCIM_MMIO_CFG1_WRITE_DATA(val)	    vBIT(val, 0, 32)
#define	VXGE_HAL_MRPCIM_MMIO_CFG1_ADDRESS(val)		    vBIT(val, 34, 6)
#define	VXGE_HAL_MRPCIM_MMIO_CFG1_MRIOVCTL_READ_DATA(val)   vBIT(val, 48, 16)
/* 0x07f00 */	u64	mrpcim_mmio_cfg2;
#define	VXGE_HAL_MRPCIM_MMIO_CFG2_WRITE_CS		    mBIT(0)
/* 0x07f08 */	u64	genstats_cfg[6];
#define	VXGE_HAL_GENSTATS_CFG_DTYPE_SEL(val)		    vBIT(val, 3, 5)
#define	VXGE_HAL_GENSTATS_CFG_CLIENT_NO_SEL(val)	    vBIT(val, 9, 3)
#define	VXGE_HAL_GENSTATS_CFG_WR_RD_CPL_SEL(val)	    vBIT(val, 14, 2)
#define	VXGE_HAL_GENSTATS_CFG_VPATH_SEL(val)		    vBIT(val, 31, 17)
/* 0x07f38 */	u64	genstat_64bit_cfg;
#define	VXGE_HAL_GENSTAT_64BIT_CFG_EN_FOR_GENSTATS0	    mBIT(3)
#define	VXGE_HAL_GENSTAT_64BIT_CFG_EN_FOR_GENSTATS2	    mBIT(7)
/* 0x07f40 */	u64	pll_slip_counters;
#define	VXGE_HAL_PLL_SLIP_COUNTERS_CMG(val)		    vBIT(val, 0, 16)
#define	VXGE_HAL_PLL_SLIP_COUNTERS_FB(val)		    vBIT(val, 16, 16)
#define	VXGE_HAL_PLL_SLIP_COUNTERS_X(val)		    vBIT(val, 32, 16)
	u8	unused08000[0x08000 - 0x07f48];

/* 0x08000 */	u64	gcmg3_int_status;
#define	VXGE_HAL_GCMG3_INT_STATUS_GSTC_ERR0_GSTC0_INT	    mBIT(0)
#define	VXGE_HAL_GCMG3_INT_STATUS_GSTC_ERR1_GSTC1_INT	    mBIT(1)
#define	VXGE_HAL_GCMG3_INT_STATUS_GH2L_ERR0_GH2L0_INT	    mBIT(2)
#define	VXGE_HAL_GCMG3_INT_STATUS_GHSQ_ERR_GH2L1_INT	    mBIT(3)
#define	VXGE_HAL_GCMG3_INT_STATUS_GHSQ_ERR2_GH2L2_INT	    mBIT(4)
#define	VXGE_HAL_GCMG3_INT_STATUS_GH2L_SMERR0_GH2L3_INT	    mBIT(5)
#define	VXGE_HAL_GCMG3_INT_STATUS_GHSQ_ERR3_GH2L4_INT	    mBIT(6)
/* 0x08008 */	u64	gcmg3_int_mask;
/* 0x08010 */	u64	gstc_err0_reg;
#define	VXGE_HAL_GSTC_ERR0_REG_STC_BDM_CACHE_DB_ERR(val)    vBIT(val, 0, 3)
#define	VXGE_HAL_GSTC_ERR0_REG_STC_BDM_CMRSP_DB_ERR(val)    vBIT(val, 3, 5)
#define	VXGE_HAL_GSTC_ERR0_REG_STC_ECI_CACHE0_DB_ERR(val)   vBIT(val, 8, 4)
#define	VXGE_HAL_GSTC_ERR0_REG_STC_ECI_CACHE1_DB_ERR(val)   vBIT(val, 12, 4)
#define	VXGE_HAL_GSTC_ERR0_REG_STC_H2L_EVENT_DB_ERR(val)    vBIT(val, 16, 5)
#define	VXGE_HAL_GSTC_ERR0_REG_STC_PRM_EVENT_DB_ERR(val)    vBIT(val, 21, 3)
#define	VXGE_HAL_GSTC_ERR0_REG_STC_SRCH_MEM_DB_ERR(val)	    vBIT(val, 24, 2)
#define	VXGE_HAL_GSTC_ERR0_REG_STC_CMCIF_RD_DATA_DB_ERR	    mBIT(26)
#define	VXGE_HAL_GSTC_ERR0_REG_STC_BDM_CACHE_SG_ERR(val)    vBIT(val, 32, 3)
#define	VXGE_HAL_GSTC_ERR0_REG_STC_BDM_CMRSP_SG_ERR(val)    vBIT(val, 35, 5)
#define	VXGE_HAL_GSTC_ERR0_REG_STC_ECI_CACHE0_SG_ERR(val)   vBIT(val, 40, 4)
#define	VXGE_HAL_GSTC_ERR0_REG_STC_ECI_CACHE1_SG_ERR(val)   vBIT(val, 44, 4)
#define	VXGE_HAL_GSTC_ERR0_REG_STC_H2L_EVENT_SG_ERR(val)    vBIT(val, 48, 5)
#define	VXGE_HAL_GSTC_ERR0_REG_STC_PRM_EVENT_SG_ERR(val)    vBIT(val, 53, 3)
#define	VXGE_HAL_GSTC_ERR0_REG_STC_SRCH_MEM_SG_ERR(val)	    vBIT(val, 56, 2)
/* 0x08018 */	u64	gstc_err0_mask;
/* 0x08020 */	u64	gstc_err0_alarm;
/* 0x08028 */	u64	gstc_err1_reg;
#define	VXGE_HAL_GSTC_ERR1_REG_STC_RPEIF_REQ_FIFO_ERR	    mBIT(0)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_RPEIF_ECRESP_FIFO_ERR    mBIT(1)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_RPEIF_BUFFRESP_FIFO_ERR  mBIT(2)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_H2L_EVENT_FIFO_ERR	    mBIT(3)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_ARB_RPE_FIFO_ERR	    mBIT(4)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_ARB_REQ_FIFO_ERR	    mBIT(5)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_SSM_EVENT_FIFO_ERR	    mBIT(6)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_SSM_CMRSP_FIFO_ERR	    mBIT(7)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_ECI_ARB_FIFO_ERR	    mBIT(8)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_PRM_EVENT_FIFO_ERR	    mBIT(9)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_PRM_PAC_FIFO_ERR	    mBIT(10)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_BDM_EVENT_FIFO_ERR	    mBIT(11)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_BDM_CMRSP_FIFO_ERR	    mBIT(12)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_CMCIF_FIFO_ERR	    mBIT(13)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_CP2STC_FIFO_ERR	    mBIT(14)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_CPIF_CREDIT_FIFO_ERR	    mBIT(15)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_RPEIF_SHADOW_ERR	    mBIT(16)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_ARB_REQ_SHADOW_ERR	    mBIT(17)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_ARB_CTL_SHADOW_ERR	    mBIT(18)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_SCC_SHADOW_ERR	    mBIT(19)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_SSM_SHADOW_ERR	    mBIT(20)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_SSM_SYNC_SHADOW_ERR	    mBIT(21)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_ECI_ARB_SHADOW_ERR	    mBIT(22)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_ECI_SYNC_SHADOW_ERR	    mBIT(23)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_ECI_EPE_SHADOW_ERR	    mBIT(24)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_PRM_PAC_SHADOW_ERR	    mBIT(25)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_PRM_PSM_SHADOW_ERR	    mBIT(26)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_PRM_PRC_SHADOW_ERR	    mBIT(27)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_BDM_SHADOW_ERR	    mBIT(28)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_CMCIF_SHADOW_ERR	    mBIT(29)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_CPIF_SHADOW_ERR	    mBIT(30)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_SCC_CLM_ERR		    mBIT(32)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_SCC_RMM_FSM_ERR	    mBIT(33)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_ECI_EPE_FSM_ERR	    mBIT(34)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_PRM_PAC_PBLESIZE0_ERR    mBIT(35)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_PRM_PAC_QUOTIENT_ERR	    mBIT(36)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_PRM_PRC_FSM_ERR	    mBIT(37)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_BDM_FSM_ERR		    mBIT(38)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_BDM_WRAP_ERR		    mBIT(39)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_RPEIF_BUFFER_ERR	    mBIT(40)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_CMCIF_FSM_ERR	    mBIT(41)
#define	VXGE_HAL_GSTC_ERR1_REG_STC_UNK_CP_MSG_TYPE	    mBIT(42)
/* 0x08030 */	u64	gstc_err1_mask;
/* 0x08038 */	u64	gstc_err1_alarm;
/* 0x08040 */	u64	gh2l_err0_reg;
#define	VXGE_HAL_GH2L_ERR0_REG_H2L_HOC_DATX_DB_ERR(val)	    vBIT(val, 0, 2)
#define	VXGE_HAL_GH2L_ERR0_REG_H2L_WRDBL0_DB_ERR(val)	    vBIT(val, 2, 2)
#define	VXGE_HAL_GH2L_ERR0_REG_H2L_WRDBL1_DB_ERR(val)	    vBIT(val, 4, 2)
#define	VXGE_HAL_GH2L_ERR0_REG_H2L_WRBUF_DB_ERR(val)	    vBIT(val, 6, 2)
#define	VXGE_HAL_GH2L_ERR0_REG_H2L_RD_RSP_DB_ERR	    mBIT(8)
#define	VXGE_HAL_GH2L_ERR0_REG_H2L_CMCRSP_DB_ERR(val)	    vBIT(val, 9, 4)
#define	VXGE_HAL_GH2L_ERR0_REG_H2L_HOC_HEAD_DB_ERR(val)	    vBIT(val, 13, 2)
#define	VXGE_HAL_GH2L_ERR0_REG_H2L_OD_MEM_PA_DB_ERR	    mBIT(15)
#define	VXGE_HAL_GH2L_ERR0_REG_H2L_OD_MEM_PB_DB_ERR	    mBIT(16)
#define	VXGE_HAL_GH2L_ERR0_REG_H2L_HOC_DATX_SG_ERR(val)	    vBIT(val, 32, 2)
#define	VXGE_HAL_GH2L_ERR0_REG_H2L_WRDBL0_SG_ERR(val)	    vBIT(val, 34, 2)
#define	VXGE_HAL_GH2L_ERR0_REG_H2L_WRDBL1_SG_ERR(val)	    vBIT(val, 36, 2)
#define	VXGE_HAL_GH2L_ERR0_REG_H2L_WRBUF_SG_ERR(val)	    vBIT(val, 38, 2)
#define	VXGE_HAL_GH2L_ERR0_REG_H2L_CMCRSP_SG_ERR(val)	    vBIT(val, 41, 4)
#define	VXGE_HAL_GH2L_ERR0_REG_H2L_HOC_HEAD_SG_ERR(val)	    vBIT(val, 45, 2)
#define	VXGE_HAL_GH2L_ERR0_REG_H2L_OD_MEM_PA_SG_ERR	    mBIT(47)
#define	VXGE_HAL_GH2L_ERR0_REG_H2L_OD_MEM_PB_SG_ERR	    mBIT(48)
/* 0x08048 */	u64	gh2l_err0_mask;
/* 0x08050 */	u64	gh2l_err0_alarm;
/* 0x08058 */	u64	ghsq_err_reg;
#define	VXGE_HAL_GHSQ_ERR_REG_H2L_CMP_WR_COMP_OFLOW_ERR	    mBIT(0)
#define	VXGE_HAL_GHSQ_ERR_REG_H2L_CMP_WR_COMP_UFLOW_ERR	    mBIT(1)
#define	VXGE_HAL_GHSQ_ERR_REG_H2L_DAT_CTL_OFLOW_ERR	    mBIT(2)
#define	VXGE_HAL_GHSQ_ERR_REG_H2L_DAT_CTL_UFLOW_ERR	    mBIT(3)
#define	VXGE_HAL_GHSQ_ERR_REG_H2L_DAT_OFLOW_ERR		    mBIT(4)
#define	VXGE_HAL_GHSQ_ERR_REG_H2L_DAT_UFLOW_ERR		    mBIT(5)
#define	VXGE_HAL_GHSQ_ERR_REG_H2L_WR_DAT224_BB_OFLOW_ERR    mBIT(6)
#define	VXGE_HAL_GHSQ_ERR_REG_H2L_WR_DAT224_BB_UFLOW_ERR    mBIT(7)
#define	VXGE_HAL_GHSQ_ERR_REG_H2L_WR_REQ_OFLOW_ERR	    mBIT(8)
#define	VXGE_HAL_GHSQ_ERR_REG_H2L_WR_REQ_UFLOW_ERR	    mBIT(9)
#define	VXGE_HAL_GHSQ_ERR_REG_H2L_WRDBL_OFLOW_ERR	    mBIT(10)
#define	VXGE_HAL_GHSQ_ERR_REG_H2L_WRDBL_UFLOW_ERR	    mBIT(11)
#define	VXGE_HAL_GHSQ_ERR_REG_H2L_HOC_XFER_DATX_UFLOW_ERR   mBIT(12)
#define	VXGE_HAL_GHSQ_ERR_REG_H2L_HOC_XFER_CTLX_UFLOW_ERR   mBIT(13)
#define	VXGE_HAL_GHSQ_ERR_REG_H2L_CMP_RD_RSP_OFLOW_ERR	    mBIT(14)
#define	VXGE_HAL_GHSQ_ERR_REG_H2L_CMP_RD_RSP_UFLOW_ERR	    mBIT(15)
#define	VXGE_HAL_GHSQ_ERR_REG_H2L_CMP_RD_TRANS_POPCRDCNT_OFLOW_ERR mBIT(16)
#define	VXGE_HAL_GHSQ_ERR_REG_H2L_CMP_RD_TRANS_POPCRDCNT_UFLOW_ERR mBIT(17)
/* 0x08060 */	u64	ghsq_err_mask;
/* 0x08068 */	u64	ghsq_err_alarm;
/* 0x08070 */	u64	ghsq_err2_reg;
#define	VXGE_HAL_GHSQ_ERR2_REG_H2L_OFLOW_ERR(n)		    mBIT(n)
#define	VXGE_HAL_GHSQ_ERR2_REG_H2L_UFLOW_ERR(n)		    mBIT(n)
/* 0x08078 */	u64	ghsq_err2_mask;
/* 0x08080 */	u64	ghsq_err2_alarm;
/* 0x08088 */	u64	ghsq_err3_reg;
#define	VXGE_HAL_GHSQ_ERR3_REG_H2L_OFLOW_ERR(n)		    mBIT(n)
#define	VXGE_HAL_GHSQ_ERR3_REG_H2L_UFLOW_ERR(n)		    mBIT(n)
/* 0x08090 */	u64	ghsq_err3_mask;
/* 0x08098 */	u64	ghsq_err3_alarm;
/* 0x080a0 */	u64	gh2l_smerr0_reg;
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_OAE_HOPIF_SM_ERR	    mBIT(0)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_OAE_NR_SM_ERR	    mBIT(1)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_OAE_HOF_SM_ERR	    mBIT(2)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_OAE_ROP_SM_ERR	    mBIT(3)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_OAE_OADE_SM_ERR	    mBIT(4)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_OAE_ODOG_SM_ERR	    mBIT(5)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_DOG_BATCH_DONE_SM_ERROR0 mBIT(6)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_DOG_BATCH_DONE_SM_ERROR1 mBIT(7)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_DOG_BATCH_DONE_SM_ERROR2 mBIT(8)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_DOG_BATCH_DONE_SM_ERROR3 mBIT(9)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_DOG_DOGLE_SM_ERROR0    mBIT(10)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_DOG_DOGLE_SM_ERROR1    mBIT(11)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_DOG_DOGLE_SM_ERROR2    mBIT(12)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_DOG_DOGLE_SM_ERROR3    mBIT(13)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_DOG_ERR_KILL_SM_ERROR0 mBIT(14)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_DOG_ERR_KILL_SM_ERROR1 mBIT(15)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_DOG_ERR_KILL_SM_ERROR2 mBIT(16)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_DOG_ERR_KILL_SM_ERROR3 mBIT(17)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_DOG_MSG_GEN_SM_ERROR0  mBIT(18)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_DOG_MSG_GEN_SM_ERROR1  mBIT(19)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_DOG_MSG_GEN_SM_ERROR2  mBIT(20)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_DOG_MSG_GEN_SM_ERROR3  mBIT(21)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_DOG_ORD_SM_ERROR0	    mBIT(22)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_DOG_ORD_SM_ERROR1	    mBIT(23)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_DOG_ORD_SM_ERROR2	    mBIT(24)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_DOG_ORD_SM_ERROR3	    mBIT(25)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_DOG_STAG_KILL_SM_ERROR mBIT(26)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_H2L_CPIF_IMP_SM_ERROR  mBIT(27)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_H2L_CPIF_OMP_SM_ERROR  mBIT(28)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_H2L_CPIF_RECALL_SM_ERROR mBIT(29)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_LOG_CCTL_FIFO_ERR	    mBIT(30)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_RETXK_CCTL_FIFO_ERR    mBIT(31)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_HOP_HCC_HANDSHAKE_ERR  mBIT(32)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_HOP_ARB_HANDSHAKE_ERR  mBIT(33)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_HOP_RETXK_HANDSHAKE_ERR mBIT(34)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_HOP_OAE_HANDSHAKE_ERR  mBIT(35)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_HOP_VPATH_ERR	    mBIT(36)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_HOP_HO_SIZE_ERR	    mBIT(37)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_HOP_HO_PARSE_ERR	    mBIT(38)
#define	VXGE_HAL_GH2L_SMERR0_REG_H2L_HOP_ARB_SM_ERR	    mBIT(39)
/* 0x080a8 */	u64	gh2l_smerr0_mask;
/* 0x080b0 */	u64	gh2l_smerr0_alarm;
/* 0x080b8 */	u64	hcc_alarm_reg;
#define	VXGE_HAL_HCC_ALARM_REG_H2L_RWCRA_RW0_SG_ERR(val)    vBIT(val, 0, 4)
#define	VXGE_HAL_HCC_ALARM_REG_H2L_RWCRA_RW0_DB_ERR(val)    vBIT(val, 4, 4)
#define	VXGE_HAL_HCC_ALARM_REG_H2L_RWCRA_RW1_SG_ERR(val)    vBIT(val, 8, 4)
#define	VXGE_HAL_HCC_ALARM_REG_H2L_RWCRA_RW1_DB_ERR(val)    vBIT(val, 12, 4)
#define	VXGE_HAL_HCC_ALARM_REG_H2L_CWBC_FSM_ERR		    mBIT(19)
#define	VXGE_HAL_HCC_ALARM_REG_H2L_RCC_FSM_ERR		    mBIT(23)
/* 0x080c0 */	u64	hcc_alarm_mask;
/* 0x080c8 */	u64	hcc_alarm_alarm;
/* 0x080d0 */	u64	gstc_cfg0;
#define	VXGE_HAL_GSTC_CFG0_RPE_PF_ENA			    mBIT(7)
#define	VXGE_HAL_GSTC_CFG0_SCC_MODE			    mBIT(15)
#define	VXGE_HAL_GSTC_CFG0_SCC_NBR_FREE_SLOTS(val)	    vBIT(val, 18, 6)
#define	VXGE_HAL_GSTC_CFG0_STC_LEFT_HASH_INDEX(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_GSTC_CFG0_STC_RIGHT_HASH_INDEX(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_GSTC_CFG0_INCL_ECI_FIFOS_PBL_SYNC	    mBIT(47)
#define	VXGE_HAL_GSTC_CFG0_MW_LOCAL_ACCESS_ENA		    mBIT(55)
#define	VXGE_HAL_GSTC_CFG0_LD_FW_CTRL_FIELDS		    mBIT(62)
#define	VXGE_HAL_GSTC_CFG0_ONLY_ROW0_DUSE1_WRITABLE	    mBIT(63)
/* 0x080d8 */	u64	gstc_cfg1;
#define	VXGE_HAL_GSTC_CFG1_INDIRECT_MODE(val)		    vBIT(val, 0, 17)
#define	VXGE_HAL_GSTC_CFG1_RPE_PF_COUNTDOWN(val)	    vBIT(val, 36, 12)
#define	VXGE_HAL_GSTC_CFG1_BDM_RATE_CTRL(val)		    vBIT(val, 54, 2)
#define	VXGE_HAL_GSTC_CFG1_BDM_EXTRA_RPE_PRM_RD		    mBIT(63)
/* 0x080e0 */	u64	gstc_cfg2;
#define	VXGE_HAL_GSTC_CFG2_MAX_FRE_CMREQ_ENTRIES(val)	    vBIT(val, 5, 3)
#define	VXGE_HAL_GSTC_CFG2_NO_STAG_KILL_WIRE_INV	    mBIT(12)
#define	VXGE_HAL_GSTC_CFG2_NO_STAG_KILL_CP_INV		    mBIT(13)
#define	VXGE_HAL_GSTC_CFG2_NO_STAG_KILL_CP_DEALLOC	    mBIT(14)
#define	VXGE_HAL_GSTC_CFG2_NO_STAG_KILL_CP_SUSP		    mBIT(15)
#define	VXGE_HAL_GSTC_CFG2_BDM_CACHE_ECC_ENABLE_N	    mBIT(16)
#define	VXGE_HAL_GSTC_CFG2_BDM_CMRSP_ECC_ENABLE_N	    mBIT(17)
#define	VXGE_HAL_GSTC_CFG2_ECI_CACHE0_ECC_ENABLE_N	    mBIT(18)
#define	VXGE_HAL_GSTC_CFG2_ECI_CACHE1_ECC_ENABLE_N	    mBIT(19)
#define	VXGE_HAL_GSTC_CFG2_H2L_EVENT_ECC_ENABLE_N	    mBIT(20)
#define	VXGE_HAL_GSTC_CFG2_PRM_EVENT_ECC_ENABLE_N	    mBIT(21)
#define	VXGE_HAL_GSTC_CFG2_SRCH_MEM_ECC_ENABLE_N	    mBIT(22)
#define	VXGE_HAL_GSTC_CFG2_GPSYNC_WAIT_TOKEN_ENABLE	    mBIT(29)
#define	VXGE_HAL_GSTC_CFG2_GPSYNC_CNTDOWN_TIMER_ENABLE	    mBIT(30)
#define	VXGE_HAL_GSTC_CFG2_GPSYNC_SRC_NOTIFY_ENABLE	    mBIT(31)
#define	VXGE_HAL_GSTC_CFG2_GPSYNC_CNTDOWN_START_VALUE(val)  vBIT(val, 36, 4)
/* 0x080e8 */	u64	stc_arb_cfg0;
#define	VXGE_HAL_STC_ARB_CFG0_RPE_PRI(val)		    vBIT(val, 6, 2)
#define	VXGE_HAL_STC_ARB_CFG0_H2L_PRI(val)		    vBIT(val, 14, 2)
#define	VXGE_HAL_STC_ARB_CFG0_CP_PRI(val)		    vBIT(val, 22, 2)
#define	VXGE_HAL_STC_ARB_CFG0_CAL0_PRI(val)		    vBIT(val, 30, 2)
#define	VXGE_HAL_STC_ARB_CFG0_CAL1_PRI(val)		    vBIT(val, 38, 2)
#define	VXGE_HAL_STC_ARB_CFG0_CAL2_PRI(val)		    vBIT(val, 46, 2)
#define	VXGE_HAL_STC_ARB_CFG0_CAL3_PRI(val)		    vBIT(val, 54, 2)
#define	VXGE_HAL_STC_ARB_CFG0_CAL4_PRI(val)		    vBIT(val, 62, 2)
/* 0x080f0 */	u64	stc_arb_cfg1;
#define	VXGE_HAL_STC_ARB_CFG1_CAL5_PRI(val)		    vBIT(val, 6, 2)
#define	VXGE_HAL_STC_ARB_CFG1_CAL6_PRI(val)		    vBIT(val, 14, 2)
#define	VXGE_HAL_STC_ARB_CFG1_CAL7_PRI(val)		    vBIT(val, 22, 2)
#define	VXGE_HAL_STC_ARB_CFG1_CAL8_PRI(val)		    vBIT(val, 30, 2)
/* 0x080f8 */	u64	stc_arb_cfg2;
#define	VXGE_HAL_STC_ARB_CFG2_MAX_NBR_H2L0_EVENTS(val)	    vBIT(val, 4, 4)
#define	VXGE_HAL_STC_ARB_CFG2_MAX_NBR_H2L1_EVENTS(val)	    vBIT(val, 12, 4)
#define	VXGE_HAL_STC_ARB_CFG2_MAX_NBR_H2L2_EVENTS(val)	    vBIT(val, 20, 4)
#define	VXGE_HAL_STC_ARB_CFG2_MAX_NBR_H2L3_EVENTS(val)	    vBIT(val, 28, 4)
#define	VXGE_HAL_STC_ARB_CFG2_MAX_NBR_RPE_EVENTS(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_STC_ARB_CFG2_MAX_NBR_MR_EVENTS(val)	    vBIT(val, 45, 3)
/* 0x08100 */	u64	stc_arb_cfg3;
#define	VXGE_HAL_STC_ARB_CFG3_MAX_NBR_H2L0_FETCHES(val)	    vBIT(val, 5, 3)
#define	VXGE_HAL_STC_ARB_CFG3_MAX_NBR_H2L1_FETCHES(val)	    vBIT(val, 13, 3)
#define	VXGE_HAL_STC_ARB_CFG3_MAX_NBR_H2L2_FETCHES(val)	    vBIT(val, 21, 3)
#define	VXGE_HAL_STC_ARB_CFG3_MAX_NBR_H2L3_FETCHES(val)	    vBIT(val, 29, 3)
#define	VXGE_HAL_STC_ARB_CFG3_MAX_NBR_RPE_FETCHES(val)	    vBIT(val, 37, 3)
#define	VXGE_HAL_STC_ARB_CFG3_MAX_NBR_RPE_PF_FETCHES(val)   vBIT(val, 46, 2)
/* 0x08108 */	u64	stc_jhash_cfg;
#define	VXGE_HAL_STC_JHASH_CFG_GOLDEN(val)		    vBIT(val, 0, 32)
#define	VXGE_HAL_STC_JHASH_CFG_INIT_VAL(val)		    vBIT(val, 32, 32)
/* 0x08110 */	u64	stc_smi_arb_cfg0;
#define	VXGE_HAL_STC_SMI_ARB_CFG0_RPE_PRI(val)		    vBIT(val, 6, 2)
#define	VXGE_HAL_STC_SMI_ARB_CFG0_H2L_PRI(val)		    vBIT(val, 14, 2)
#define	VXGE_HAL_STC_SMI_ARB_CFG0_CP_PRI(val)		    vBIT(val, 22, 2)
#define	VXGE_HAL_STC_SMI_ARB_CFG0_CAL0_PRI(val)		    vBIT(val, 30, 2)
#define	VXGE_HAL_STC_SMI_ARB_CFG0_CAL1_PRI(val)		    vBIT(val, 38, 2)
#define	VXGE_HAL_STC_SMI_ARB_CFG0_CAL2_PRI(val)		    vBIT(val, 46, 2)
#define	VXGE_HAL_STC_SMI_ARB_CFG0_CAL3_PRI(val)		    vBIT(val, 54, 2)
#define	VXGE_HAL_STC_SMI_ARB_CFG0_CAL4_PRI(val)		    vBIT(val, 62, 2)
/* 0x08118 */	u64	stc_smi_arb_cfg1;
#define	VXGE_HAL_STC_SMI_ARB_CFG1_CAL5_PRI(val)		    vBIT(val, 6, 2)
#define	VXGE_HAL_STC_SMI_ARB_CFG1_CAL6_PRI(val)		    vBIT(val, 14, 2)
#define	VXGE_HAL_STC_SMI_ARB_CFG1_CAL7_PRI(val)		    vBIT(val, 22, 2)
#define	VXGE_HAL_STC_SMI_ARB_CFG1_CAL8_PRI(val)		    vBIT(val, 30, 2)
#define	VXGE_HAL_STC_SMI_ARB_CFG1_CAL9_PRI(val)		    vBIT(val, 38, 2)
#define	VXGE_HAL_STC_SMI_ARB_CFG1_SAME_PRI_B2B_CAL	    mBIT(48)
/* 0x08120 */	u64	stc_caa_arb_cfg0;
#define	VXGE_HAL_STC_CAA_ARB_CFG0_RPE_PRI(val)		    vBIT(val, 6, 2)
#define	VXGE_HAL_STC_CAA_ARB_CFG0_H2L_PRI(val)		    vBIT(val, 14, 2)
#define	VXGE_HAL_STC_CAA_ARB_CFG0_CP_PRI(val)		    vBIT(val, 22, 2)
#define	VXGE_HAL_STC_CAA_ARB_CFG0_CAL0_PRI(val)		    vBIT(val, 30, 2)
#define	VXGE_HAL_STC_CAA_ARB_CFG0_CAL1_PRI(val)		    vBIT(val, 38, 2)
#define	VXGE_HAL_STC_CAA_ARB_CFG0_CAL2_PRI(val)		    vBIT(val, 46, 2)
#define	VXGE_HAL_STC_CAA_ARB_CFG0_CAL3_PRI(val)		    vBIT(val, 54, 2)
#define	VXGE_HAL_STC_CAA_ARB_CFG0_CAL4_PRI(val)		    vBIT(val, 62, 2)
/* 0x08128 */	u64	stc_caa_arb_cfg1;
#define	VXGE_HAL_STC_CAA_ARB_CFG1_CAL5_PRI(val)		    vBIT(val, 6, 2)
#define	VXGE_HAL_STC_CAA_ARB_CFG1_CAL6_PRI(val)		    vBIT(val, 14, 2)
#define	VXGE_HAL_STC_CAA_ARB_CFG1_CAL7_PRI(val)		    vBIT(val, 22, 2)
#define	VXGE_HAL_STC_CAA_ARB_CFG1_CAL8_PRI(val)		    vBIT(val, 30, 2)
#define	VXGE_HAL_STC_CAA_ARB_CFG1_SAME_PRI_B2B_CAL	    mBIT(39)
/* 0x08130 */	u64	stc_eci_arb_cfg0;
#define	VXGE_HAL_STC_ECI_ARB_CFG0_RPE_PRI(val)		    vBIT(val, 6, 2)
#define	VXGE_HAL_STC_ECI_ARB_CFG0_H2L_PRI(val)		    vBIT(val, 14, 2)
#define	VXGE_HAL_STC_ECI_ARB_CFG0_CP_PRI(val)		    vBIT(val, 22, 2)
#define	VXGE_HAL_STC_ECI_ARB_CFG0_CAL0_PRI(val)		    vBIT(val, 30, 2)
#define	VXGE_HAL_STC_ECI_ARB_CFG0_CAL1_PRI(val)		    vBIT(val, 38, 2)
#define	VXGE_HAL_STC_ECI_ARB_CFG0_CAL2_PRI(val)		    vBIT(val, 46, 2)
#define	VXGE_HAL_STC_ECI_ARB_CFG0_CAL3_PRI(val)		    vBIT(val, 54, 2)
#define	VXGE_HAL_STC_ECI_ARB_CFG0_CAL4_PRI(val)		    vBIT(val, 62, 2)
/* 0x08138 */	u64	stc_eci_arb_cfg1;
#define	VXGE_HAL_STC_ECI_ARB_CFG1_CAL5_PRI(val)		    vBIT(val, 6, 2)
#define	VXGE_HAL_STC_ECI_ARB_CFG1_CAL6_PRI(val)		    vBIT(val, 14, 2)
#define	VXGE_HAL_STC_ECI_ARB_CFG1_CAL7_PRI(val)		    vBIT(val, 22, 2)
#define	VXGE_HAL_STC_ECI_ARB_CFG1_CAL8_PRI(val)		    vBIT(val, 30, 2)
/* 0x08140 */	u64	stc_eci_cfg0;
#define	VXGE_HAL_STC_ECI_CFG0_SUSPEND_DEALLOC_STAGS_ENA	    mBIT(4)
#define	VXGE_HAL_STC_ECI_CFG0_MULT_SUSPEND_ERR_ENA	    mBIT(5)
#define	VXGE_HAL_STC_ECI_CFG0_SUSPEND_PDID_CHECK_ENA	    mBIT(6)
#define	VXGE_HAL_STC_ECI_CFG0_UNSUSPEND_PDID_CHECK_ENA	    mBIT(7)
#define	VXGE_HAL_STC_ECI_CFG0_ALTER_NUM_MWS_KEY_CHECK_ENA   mBIT(14)
#define	VXGE_HAL_STC_ECI_CFG0_ALTER_NUM_MWS_PDID_CHECK_ENA  mBIT(15)
#define	VXGE_HAL_STC_ECI_CFG0_SET_SHARED_KEY_CHECK_ENA	    mBIT(23)
#define	VXGE_HAL_STC_ECI_CFG0_STAG_WR_FAIL_IF_DEALLOC	    mBIT(31)
#define	VXGE_HAL_STC_ECI_CFG0_PLACEMENT_MR_DEFERRAL_ENA	    mBIT(34)
#define	VXGE_HAL_STC_ECI_CFG0_SUSPEND_MR_DEFERRAL_ENA	    mBIT(35)
#define	VXGE_HAL_STC_ECI_CFG0_ALTER_NUM_MWS_MR_DEFERRAL_ENA mBIT(36)
#define	VXGE_HAL_STC_ECI_CFG0_BIND_MW_MR_DEFERRAL_ENA	    mBIT(37)
#define	VXGE_HAL_STC_ECI_CFG0_SET_SHARED_MR_DEFERRAL_ENA    mBIT(38)
#define	VXGE_HAL_STC_ECI_CFG0_STAG_WR_MR_DEFERRAL_ENA	    mBIT(39)
#define	VXGE_HAL_STC_ECI_CFG0_RESUBMIT_INTERVAL(val)	    vBIT(val, 40, 8)
#define	VXGE_HAL_STC_ECI_CFG0_SUSP_STAG_PLACE_STALL_ENA	    mBIT(54)
#define	VXGE_HAL_STC_ECI_CFG0_SUSP_STAG_WIRE_INV_STALL_ENA  mBIT(55)
#define	VXGE_HAL_STC_ECI_CFG0_SUSP_STAG_WIRE_INV_ENA	    mBIT(56)
#define	VXGE_HAL_STC_ECI_CFG0_SUSP_STAG_CP_INV_ENA	    mBIT(57)
#define	VXGE_HAL_STC_ECI_CFG0_SUSP_STAG_MR_EVENT_ENA	    mBIT(58)
#define	VXGE_HAL_STC_ECI_CFG0_SUSP_STAG_DEALLOC_ENA	    mBIT(59)
#define	VXGE_HAL_STC_ECI_CFG0_SUSP_STAG_ALTER_NUM_MWS_ENA   mBIT(60)
#define	VXGE_HAL_STC_ECI_CFG0_SUSP_STAG_BIND_MW_ENA	    mBIT(61)
#define	VXGE_HAL_STC_ECI_CFG0_SUSP_STAG_SET_SHARED_ENA	    mBIT(62)
#define	VXGE_HAL_STC_ECI_CFG0_SUSP_STAG_STAG_WR_ENA	    mBIT(63)
/* 0x08148 */	u64	stc_prm_cfg0;
#define	VXGE_HAL_STC_PRM_CFG0_PAC_RPE_PRI		    mBIT(6)
#define	VXGE_HAL_STC_PRM_CFG0_PAC_H2L_PRI		    mBIT(7)
#define	VXGE_HAL_STC_PRM_CFG0_PAC_CAL0_PRI		    mBIT(8)
#define	VXGE_HAL_STC_PRM_CFG0_PAC_CAL1_PRI		    mBIT(9)
#define	VXGE_HAL_STC_PRM_CFG0_PAC_CAL2_PRI		    mBIT(10)
#define	VXGE_HAL_STC_PRM_CFG0_PAC_CAL3_PRI		    mBIT(11)
#define	VXGE_HAL_STC_PRM_CFG0_PAC_CAL4_PRI		    mBIT(12)
#define	VXGE_HAL_STC_PRM_CFG0_PAC_CAL5_PRI		    mBIT(13)
#define	VXGE_HAL_STC_PRM_CFG0_PAC_CAL6_PRI		    mBIT(14)
#define	VXGE_HAL_STC_PRM_CFG0_PAC_CAL7_PRI		    mBIT(15)
#define	VXGE_HAL_STC_PRM_CFG0_PRC_RPE_PRI		    mBIT(22)
#define	VXGE_HAL_STC_PRM_CFG0_PRC_H2L_PRI		    mBIT(23)
#define	VXGE_HAL_STC_PRM_CFG0_PRC_CAL0_PRI		    mBIT(24)
#define	VXGE_HAL_STC_PRM_CFG0_PRC_CAL1_PRI		    mBIT(25)
#define	VXGE_HAL_STC_PRM_CFG0_PRC_CAL2_PRI		    mBIT(26)
#define	VXGE_HAL_STC_PRM_CFG0_PRC_CAL3_PRI		    mBIT(27)
#define	VXGE_HAL_STC_PRM_CFG0_PRC_CAL4_PRI		    mBIT(28)
#define	VXGE_HAL_STC_PRM_CFG0_PRC_CAL5_PRI		    mBIT(29)
#define	VXGE_HAL_STC_PRM_CFG0_PRC_CAL6_PRI		    mBIT(30)
#define	VXGE_HAL_STC_PRM_CFG0_PRC_CAL7_PRI		    mBIT(31)
#define	VXGE_HAL_STC_PRM_CFG0_RDUSE_ENA			    mBIT(39)
/* 0x08150 */	u64	h2l_misc_cfg;
#define	VXGE_HAL_H2L_MISC_CFG_HSQ_FORCE_CMP		    mBIT(0)
#define	VXGE_HAL_H2L_MISC_CFG_HOP_IPID_MSB		    mBIT(1)
#define	VXGE_HAL_H2L_MISC_CFG_HOP_ARB_ENABLE		    mBIT(2)
#define	VXGE_HAL_H2L_MISC_CFG_HOP_ENFORCE_HSN		    mBIT(3)
#define	VXGE_HAL_H2L_MISC_CFG_HOP_ENFORCE_RD_XON	    mBIT(4)
#define	VXGE_HAL_H2L_MISC_CFG_HOP_ENFORCE_PDA_VPBP	    mBIT(5)
#define	VXGE_HAL_H2L_MISC_CFG_OAE_VPBP_CHECK_ENA	    mBIT(6)
#define	VXGE_HAL_H2L_MISC_CFG_OAE_XON_CHECK_ENA		    mBIT(7)
#define	VXGE_HAL_H2L_MISC_CFG_HOCHEAD_RD_THRES(val)	    vBIT(val, 10, 6)
#define	VXGE_HAL_H2L_MISC_CFG_HCC_WB_THRESHOLD(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_H2L_MISC_CFG_HOP_BCK_STATS_MODE(val)	    vBIT(val, 25, 2)
#define	VXGE_HAL_H2L_MISC_CFG_HOP_BCK_STATS_VPATH(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_H2L_MISC_CFG_HOC_DATX_ECC_ENABLE_N	    mBIT(35)
#define	VXGE_HAL_H2L_MISC_CFG_WRDBL_ECC_ENABLE_N	    mBIT(36)
#define	VXGE_HAL_H2L_MISC_CFG_WRBUF_ECC_ENABLE_N	    mBIT(37)
#define	VXGE_HAL_H2L_MISC_CFG_CMCRSP_ECC_ENABLE_N	    mBIT(38)
#define	VXGE_HAL_H2L_MISC_CFG_HOC_HEAD_ECC_ENABLE_N	    mBIT(39)
#define	VXGE_HAL_H2L_MISC_CFG_OD_MEM_ECC_ENABLE_N	    mBIT(40)
#define	VXGE_HAL_H2L_MISC_CFG_RW_CACHE_ECC_ENABLE_N	    mBIT(41)
/* 0x08158 */	u64	hsq_cfg[17];
#define	VXGE_HAL_HSQ_CFG_BASE_ADDR(val)			    vBIT(val, 8, 24)
#define	VXGE_HAL_HSQ_CFG_SIZE224(val)			    vBIT(val, 40, 24)
/* 0x081e0 */	u64	usdc_vpbp_cfg;
#define	VXGE_HAL_USDC_VPBP_CFG_THRES224(val)		    vBIT(val, 8, 24)
#define	VXGE_HAL_USDC_VPBP_CFG_HYST224(val)		    vBIT(val, 40, 24)
/* 0x081e8 */	u64	kdfc_vpbp_cfg;
#define	VXGE_HAL_KDFC_VPBP_CFG_THRES224(val)		    vBIT(val, 8, 24)
#define	VXGE_HAL_KDFC_VPBP_CFG_HYST224(val)		    vBIT(val, 40, 24)
/* 0x081f0 */	u64	txpe_vpbp_cfg;
#define	VXGE_HAL_TXPE_VPBP_CFG_THRES224(val)		    vBIT(val, 8, 24)
#define	VXGE_HAL_TXPE_VPBP_CFG_HYST224(val)		    vBIT(val, 40, 24)
/* 0x081f8 */	u64	one_vpbp_cfg;
#define	VXGE_HAL_ONE_VPBP_CFG_THRES224(val)		    vBIT(val, 8, 24)
#define	VXGE_HAL_ONE_VPBP_CFG_HYST224(val)		    vBIT(val, 40, 24)
/* 0x08200 */	u64	hoparb_wrr_ctrl_0;
#define	VXGE_HAL_HOPARB_WRR_CTRL_0_SS_0_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_0_SS_1_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_0_SS_2_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_0_SS_3_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_0_SS_4_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_0_SS_5_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_0_SS_6_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_0_SS_7_NUM(val)	    vBIT(val, 59, 5)
/* 0x08208 */	u64	hoparb_wrr_ctrl_1;
#define	VXGE_HAL_HOPARB_WRR_CTRL_1_SS_8_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_1_SS_9_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_1_SS_10_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_1_SS_11_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_1_SS_12_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_1_SS_13_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_1_SS_14_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_1_SS_15_NUM(val)	    vBIT(val, 59, 5)
/* 0x08210 */	u64	hoparb_wrr_ctrl_2;
#define	VXGE_HAL_HOPARB_WRR_CTRL_2_SS_16_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_2_SS_17_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_2_SS_18_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_2_SS_19_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_2_SS_20_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_2_SS_21_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_2_SS_22_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_2_SS_23_NUM(val)	    vBIT(val, 59, 5)
/* 0x08218 */	u64	hoparb_wrr_ctrl_3;
#define	VXGE_HAL_HOPARB_WRR_CTRL_3_SS_24_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_3_SS_25_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_3_SS_26_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_3_SS_27_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_3_SS_28_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_3_SS_29_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_3_SS_30_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_3_SS_31_NUM(val)	    vBIT(val, 59, 5)
/* 0x08220 */	u64	hoparb_wrr_ctrl_4;
#define	VXGE_HAL_HOPARB_WRR_CTRL_4_SS_32_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_4_SS_33_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_4_SS_34_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_4_SS_35_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_4_SS_36_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_4_SS_37_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_4_SS_38_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_4_SS_39_NUM(val)	    vBIT(val, 59, 5)
/* 0x08228 */	u64	hoparb_wrr_ctrl_5;
#define	VXGE_HAL_HOPARB_WRR_CTRL_5_SS_40_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_5_SS_41_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_5_SS_42_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_5_SS_43_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_5_SS_44_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_5_SS_45_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_5_SS_46_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_5_SS_47_NUM(val)	    vBIT(val, 59, 5)
/* 0x08230 */	u64	hoparb_wrr_ctrl_6;
#define	VXGE_HAL_HOPARB_WRR_CTRL_6_SS_48_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_6_SS_49_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_6_SS_50_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_6_SS_51_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_6_SS_52_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_6_SS_53_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_6_SS_54_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_6_SS_55_NUM(val)	    vBIT(val, 59, 5)
/* 0x08238 */	u64	hoparb_wrr_ctrl_7;
#define	VXGE_HAL_HOPARB_WRR_CTRL_7_SS_56_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_7_SS_57_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_7_SS_58_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_7_SS_59_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_7_SS_60_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_7_SS_61_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_7_SS_62_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_7_SS_63_NUM(val)	    vBIT(val, 59, 5)
/* 0x08240 */	u64	hoparb_wrr_ctrl_8;
#define	VXGE_HAL_HOPARB_WRR_CTRL_8_SS_64_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_8_SS_65_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_8_SS_66_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_8_SS_67_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_8_SS_68_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_8_SS_69_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_8_SS_70_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_8_SS_71_NUM(val)	    vBIT(val, 59, 5)
/* 0x08248 */	u64	hoparb_wrr_ctrl_9;
#define	VXGE_HAL_HOPARB_WRR_CTRL_9_SS_72_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_9_SS_73_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_9_SS_74_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_9_SS_75_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_9_SS_76_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_9_SS_77_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_9_SS_78_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_9_SS_79_NUM(val)	    vBIT(val, 59, 5)
/* 0x08250 */	u64	hoparb_wrr_ctrl_10;
#define	VXGE_HAL_HOPARB_WRR_CTRL_10_SS_80_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_10_SS_81_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_10_SS_82_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_10_SS_83_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_10_SS_84_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_10_SS_85_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_10_SS_86_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_10_SS_87_NUM(val)	    vBIT(val, 59, 5)
/* 0x08258 */	u64	hoparb_wrr_ctrl_11;
#define	VXGE_HAL_HOPARB_WRR_CTRL_11_SS_88_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_11_SS_89_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_11_SS_90_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_11_SS_91_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_11_SS_92_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_11_SS_93_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_11_SS_94_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_11_SS_95_NUM(val)	    vBIT(val, 59, 5)
/* 0x08260 */	u64	hoparb_wrr_ctrl_12;
#define	VXGE_HAL_HOPARB_WRR_CTRL_12_SS_96_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_12_SS_97_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_12_SS_98_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_12_SS_99_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_12_SS_100_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_12_SS_101_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_12_SS_102_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_12_SS_103_NUM(val)	    vBIT(val, 59, 5)
/* 0x08268 */	u64	hoparb_wrr_ctrl_13;
#define	VXGE_HAL_HOPARB_WRR_CTRL_13_SS_104_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_13_SS_105_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_13_SS_106_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_13_SS_107_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_13_SS_108_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_13_SS_109_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_13_SS_110_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_13_SS_111_NUM(val)	    vBIT(val, 59, 5)
/* 0x08270 */	u64	hoparb_wrr_ctrl_14;
#define	VXGE_HAL_HOPARB_WRR_CTRL_14_SS_112_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_14_SS_113_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_14_SS_114_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_14_SS_115_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_14_SS_116_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_14_SS_117_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_14_SS_118_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_14_SS_119_NUM(val)	    vBIT(val, 59, 5)
/* 0x08278 */	u64	hoparb_wrr_ctrl_15;
#define	VXGE_HAL_HOPARB_WRR_CTRL_15_SS_120_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_15_SS_121_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_15_SS_122_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_15_SS_123_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_15_SS_124_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_15_SS_125_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_15_SS_126_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_15_SS_127_NUM(val)	    vBIT(val, 59, 5)
/* 0x08280 */	u64	hoparb_wrr_ctrl_16;
#define	VXGE_HAL_HOPARB_WRR_CTRL_16_SS_128_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_16_SS_129_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_16_SS_130_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_16_SS_131_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_16_SS_132_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_16_SS_133_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_16_SS_134_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_16_SS_135_NUM(val)	    vBIT(val, 59, 5)
/* 0x08288 */	u64	hoparb_wrr_ctrl_17;
#define	VXGE_HAL_HOPARB_WRR_CTRL_17_SS_136_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_17_SS_137_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_17_SS_138_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_17_SS_139_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_17_SS_140_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_17_SS_141_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_17_SS_142_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_17_SS_143_NUM(val)	    vBIT(val, 59, 5)
/* 0x08290 */	u64	hoparb_wrr_ctrl_18;
#define	VXGE_HAL_HOPARB_WRR_CTRL_18_SS_144_NUM(val)	    vBIT(val, 3, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_18_SS_145_NUM(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_18_SS_146_NUM(val)	    vBIT(val, 19, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_18_SS_147_NUM(val)	    vBIT(val, 27, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_18_SS_148_NUM(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_18_SS_149_NUM(val)	    vBIT(val, 43, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_18_SS_150_NUM(val)	    vBIT(val, 51, 5)
#define	VXGE_HAL_HOPARB_WRR_CTRL_18_SS_151_NUM(val)	    vBIT(val, 59, 5)
/* 0x08298 */	u64	hoparb_wrr_ctrl_19;
#define	VXGE_HAL_HOPARB_WRR_CTRL_19_SS_152_NUM(val)	    vBIT(val, 3, 5)
/* 0x082a0 */	u64	hoparb_wrr_cmp_0;
#define	VXGE_HAL_HOPARB_WRR_CMP_0_VP0_NUM(val)		    vBIT(val, 3, 5)
#define	VXGE_HAL_HOPARB_WRR_CMP_0_VP1_NUM(val)		    vBIT(val, 11, 5)
#define	VXGE_HAL_HOPARB_WRR_CMP_0_VP2_NUM(val)		    vBIT(val, 19, 5)
#define	VXGE_HAL_HOPARB_WRR_CMP_0_VP3_NUM(val)		    vBIT(val, 27, 5)
#define	VXGE_HAL_HOPARB_WRR_CMP_0_VP4_NUM(val)		    vBIT(val, 35, 5)
#define	VXGE_HAL_HOPARB_WRR_CMP_0_VP5_NUM(val)		    vBIT(val, 43, 5)
#define	VXGE_HAL_HOPARB_WRR_CMP_0_VP6_NUM(val)		    vBIT(val, 51, 5)
#define	VXGE_HAL_HOPARB_WRR_CMP_0_VP7_NUM(val)		    vBIT(val, 59, 5)
/* 0x082a8 */	u64	hoparb_wrr_cmp_1;
#define	VXGE_HAL_HOPARB_WRR_CMP_1_VP8_NUM(val)		    vBIT(val, 3, 5)
#define	VXGE_HAL_HOPARB_WRR_CMP_1_VP9_NUM(val)		    vBIT(val, 11, 5)
#define	VXGE_HAL_HOPARB_WRR_CMP_1_VP10_NUM(val)		    vBIT(val, 19, 5)
#define	VXGE_HAL_HOPARB_WRR_CMP_1_VP11_NUM(val)		    vBIT(val, 27, 5)
#define	VXGE_HAL_HOPARB_WRR_CMP_1_VP12_NUM(val)		    vBIT(val, 35, 5)
#define	VXGE_HAL_HOPARB_WRR_CMP_1_VP13_NUM(val)		    vBIT(val, 43, 5)
#define	VXGE_HAL_HOPARB_WRR_CMP_1_VP14_NUM(val)		    vBIT(val, 51, 5)
#define	VXGE_HAL_HOPARB_WRR_CMP_1_VP15_NUM(val)		    vBIT(val, 59, 5)
/* 0x082b0 */	u64	hoparb_wrr_cmp_2;
#define	VXGE_HAL_HOPARB_WRR_CMP_2_VP16_NUM(val)		    vBIT(val, 3, 5)
	u8	unused082e8[0x082e8 - 0x082b8];

/* 0x082e8 */	u64	hop_bck_stats0;
#define	VXGE_HAL_HOP_BCK_STATS0_HO_DISPATCH_CNT(val)	    vBIT(val, 0, 32)
#define	VXGE_HAL_HOP_BCK_STATS0_HO_DROP_CNT(val)	    vBIT(val, 32, 32)
	u8	unused08400[0x08400 - 0x082f0];

/* 0x08400 */	u64	pcmg3_int_status;
#define	VXGE_HAL_PCMG3_INT_STATUS_DAM_ERR_DAM_INT	    mBIT(0)
#define	VXGE_HAL_PCMG3_INT_STATUS_PSTC_ERR_PSTC_INT	    mBIT(1)
#define	VXGE_HAL_PCMG3_INT_STATUS_PH2L_ERR0_PH2L_INT	    mBIT(2)
/* 0x08408 */	u64	pcmg3_int_mask;
/* 0x08410 */	u64	dam_err_reg;
#define	VXGE_HAL_DAM_ERR_REG_DAM_RDSB_ECC_SG_ERR	    mBIT(0)
#define	VXGE_HAL_DAM_ERR_REG_DAM_WRSB_ECC_SG_ERR	    mBIT(1)
#define	VXGE_HAL_DAM_ERR_REG_DAM_HPPEDAT_ECC_SG_ERR	    mBIT(3)
#define	VXGE_HAL_DAM_ERR_REG_DAM_LPPEDAT_ECC_SG_ERR	    mBIT(4)
#define	VXGE_HAL_DAM_ERR_REG_DAM_WRRESP_ECC_SG_ERR	    mBIT(5)
#define	VXGE_HAL_DAM_ERR_REG_DAM_RDSB_ECC_DB_ERR	    mBIT(32)
#define	VXGE_HAL_DAM_ERR_REG_DAM_WRSB_ECC_DB_ERR	    mBIT(33)
#define	VXGE_HAL_DAM_ERR_REG_DAM_HPPEDAT_ECC_DB_ERR	    mBIT(34)
#define	VXGE_HAL_DAM_ERR_REG_DAM_LPPEDAT_ECC_DB_ERR	    mBIT(35)
#define	VXGE_HAL_DAM_ERR_REG_DAM_WRRESP_ECC_DB_ERR	    mBIT(36)
#define	VXGE_HAL_DAM_ERR_REG_DAM_HPRD_ERR		    mBIT(40)
#define	VXGE_HAL_DAM_ERR_REG_DAM_LPRD_0_ERR		    mBIT(41)
#define	VXGE_HAL_DAM_ERR_REG_DAM_LPRD_1_ERR		    mBIT(42)
#define	VXGE_HAL_DAM_ERR_REG_DAM_HPPEDAT_OVERFLOW_ERR	    mBIT(48)
#define	VXGE_HAL_DAM_ERR_REG_DAM_LPPEDAT_OVERFLOW_ERR	    mBIT(49)
#define	VXGE_HAL_DAM_ERR_REG_DAM_WRRESP_OVERFLOW_ERR	    mBIT(50)
#define	VXGE_HAL_DAM_ERR_REG_DAM_SM_ERR			    mBIT(56)
/* 0x08418 */	u64	dam_err_mask;
/* 0x08420 */	u64	dam_err_alarm;
/* 0x08428 */	u64	pstc_err_reg;
#define	VXGE_HAL_PSTC_ERR_REG_STC_RPEIF_REQ_FIFO_ERR	    mBIT(0)
#define	VXGE_HAL_PSTC_ERR_REG_STC_RPEIF_ECRESP_FIFO_ERR	    mBIT(1)
#define	VXGE_HAL_PSTC_ERR_REG_STC_RPEIF_BUFFRESP_FIFO_ERR   mBIT(2)
#define	VXGE_HAL_PSTC_ERR_REG_STC_ARB_RPE_FIFO_ERR	    mBIT(3)
#define	VXGE_HAL_PSTC_ERR_REG_STC_CP2STC_FIFO_ERR	    mBIT(4)
/* 0x08430 */	u64	pstc_err_mask;
/* 0x08438 */	u64	pstc_err_alarm;
/* 0x08440 */	u64	ph2l_err0_reg;
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_HOC_XFER_DATX_OFLOW_ERR  mBIT(0)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_HOC_XFER_CTLX_OFLOW_ERR  mBIT(1)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_HOC_XFER_PARSE_ERR	    mBIT(2)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_HOC_XFER_TCPOP_BYTES_ERR mBIT(3)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_HOC_XFER_IDATA_BYTES_ERR mBIT(4)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_HOC_XFER_PLDTYPE_ERR	    mBIT(5)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_HOC_XFER_OD_ODLIST_LEN_ERR mBIT(6)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_HOC_XFER_VPATH_ERR	    mBIT(7)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_PHDR_MEM_DB_ERR(val)	    vBIT(val, 8, 2)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_IDATA_MEM_DB_ERR(val)    vBIT(val, 10, 2)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_RO_CACHE_DB_ERR(val)	    vBIT(val, 12, 3)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_RETXK_TBL_DB_ERR	    mBIT(15)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_LOG_MUX_FIFO_ERR	    mBIT(16)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_LOG_CCTL_FIFO_ERR	    mBIT(17)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_RETXK_CCTL_FIFO_ERR	    mBIT(18)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_LOG_MUX_CRED_CNT_ERR	    mBIT(19)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_LOG_PDI_CRED_CNT_ERR	    mBIT(20)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_LOG_PCTL_SHADOW_ERR	    mBIT(21)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_LOG_OPC_SHADOW_ERR	    mBIT(22)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_LOG_MUX_SHADOW_ERR	    mBIT(23)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_LOG_PDI_SHADOW_ERR	    mBIT(24)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_RETXK_LCTL_SHADOW_ERR    mBIT(26)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_RETXK_TXI_SHADOW_ERR	    mBIT(27)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_RETXK_RXI_SHADOW_ERR	    mBIT(28)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_RETXK_HPI_SHADOW_ERR	    mBIT(29)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_RETXK_CCTL_SHADOW_ERR    mBIT(30)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_LOG_PCTL_FSM_ERR	    mBIT(31)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_LOG_MUX_FSM_ERR	    mBIT(32)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_LOG_LO_COMPL_ERR	    mBIT(33)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_RETXK_LCTL_FSM_ERR	    mBIT(34)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_RETXK_TXI_FSM_ERR	    mBIT(35)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_RETXK_SLOT_MGMT_ERR	    mBIT(36)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_RETXK_HPI_FSM_ERR	    mBIT(37)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_RETXK_CCTL_FSM_ERR	    mBIT(38)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_ROCRC_HOP_OFLOW_ERR	    mBIT(39)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_PDA_H2L_DONE_FIFO_OVERFLOW mBIT(40)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_PHDR_MEM_SG_ERR(val)	    vBIT(val, 48, 2)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_IDATA_MEM_SG_ERR(val)    vBIT(val, 50, 2)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_RO_CACHE_SG_ERR(val)	    vBIT(val, 52, 3)
#define	VXGE_HAL_PH2L_ERR0_REG_H2L_RETXK_TBL_SG_ERR	    mBIT(55)
/* 0x08448 */	u64	ph2l_err0_mask;
/* 0x08450 */	u64	ph2l_err0_alarm;
/* 0x08458 */	u64	dam_bypass_queue_0;
#define	VXGE_HAL_DAM_BYPASS_QUEUE_0_ENABLE		    mBIT(0)
#define	VXGE_HAL_DAM_BYPASS_QUEUE_0_BASE(val)		    vBIT(val, 8, 24)
#define	VXGE_HAL_DAM_BYPASS_QUEUE_0_LENGTH(val)		    vBIT(val, 40, 24)
/* 0x08460 */	u64	dam_bypass_queue_1;
#define	VXGE_HAL_DAM_BYPASS_QUEUE_1_BASE(val)		    vBIT(val, 8, 24)
#define	VXGE_HAL_DAM_BYPASS_QUEUE_1_LENGTH(val)		    vBIT(val, 40, 24)
/* 0x08468 */	u64	dam_bypass_queue_2;
#define	VXGE_HAL_DAM_BYPASS_QUEUE_2_BASE(val)		    vBIT(val, 8, 24)
#define	VXGE_HAL_DAM_BYPASS_QUEUE_2_LENGTH(val)		    vBIT(val, 40, 24)
/* 0x08470 */	u64	dam_ecc_ctrl;
#define	VXGE_HAL_DAM_ECC_CTRL_DISABLE			    mBIT(0)
/* 0x08478 */	u64	ph2l_cfg0;
#define	VXGE_HAL_PH2L_CFG0_PHDR_MEM_ECC_ENABLE_N	    mBIT(15)
#define	VXGE_HAL_PH2L_CFG0_IDATA_MEM_ECC_ENABLE_N	    mBIT(23)
#define	VXGE_HAL_PH2L_CFG0_RO_CACHE_ECC_ENABLE_N	    mBIT(31)
#define	VXGE_HAL_PH2L_CFG0_RETXK_TBL_ECC_ENABLE_N	    mBIT(39)
#define	VXGE_HAL_PH2L_CFG0_LOG_XON_CHECK_ENA		    mBIT(47)
#define	VXGE_HAL_PH2L_CFG0_LOG_VPBP_CHECK_ENA		    mBIT(55)
#define	VXGE_HAL_PH2L_CFG0_NBR_RETX_SLOTS_PER_VP(val)	    vBIT(val, 62, 2)
/* 0x08480 */	u64	pstc_cfg0;
#define	VXGE_HAL_PSTC_CFG0_PGSYNC_WAIT_TOKEN_ENABLE	    mBIT(5)
#define	VXGE_HAL_PSTC_CFG0_PGSYNC_CNTDOWN_TIMER_ENABLE	    mBIT(6)
#define	VXGE_HAL_PSTC_CFG0_PGSYNC_SRC_NOTIFY_ENABLE	    mBIT(7)
#define	VXGE_HAL_PSTC_CFG0_PGSYNC_CNTDOWN_START_VALUE(val)  vBIT(val, 12, 4)
	u8	unused08510[0x08510 - 0x08488];

/* 0x08510 */	u64	neterion_membist_control;
#define	VXGE_HAL_NETERION_MEMBIST_CONTROL_INC_CMG1	    mBIT(0)
#define	VXGE_HAL_NETERION_MEMBIST_CONTROL_INC_CMG2	    mBIT(1)
#define	VXGE_HAL_NETERION_MEMBIST_CONTROL_INC_CMG3	    mBIT(2)
#define	VXGE_HAL_NETERION_MEMBIST_CONTROL_INC_DRBELL	    mBIT(3)
#define	VXGE_HAL_NETERION_MEMBIST_CONTROL_INC_FBIF	    mBIT(4)
#define	VXGE_HAL_NETERION_MEMBIST_CONTROL_INC_MSG	    mBIT(5)
#define	VXGE_HAL_NETERION_MEMBIST_CONTROL_INC_ONE	    mBIT(6)
#define	VXGE_HAL_NETERION_MEMBIST_CONTROL_INC_PCI	    mBIT(7)
#define	VXGE_HAL_NETERION_MEMBIST_CONTROL_INC_RTDMA	    mBIT(8)
#define	VXGE_HAL_NETERION_MEMBIST_CONTROL_INC_WRDMA	    mBIT(9)
#define	VXGE_HAL_NETERION_MEMBIST_CONTROL_INC_XGMAC	    mBIT(10)
#define	VXGE_HAL_NETERION_MEMBIST_CONTROL_INC_FB	    mBIT(11)
#define	VXGE_HAL_NETERION_MEMBIST_CONTROL_INC_CM	    mBIT(12)
#define	VXGE_HAL_NETERION_MEMBIST_CONTROL_OVERRIDE_FB_DONE  mBIT(16)
#define	VXGE_HAL_NETERION_MEMBIST_CONTROL_OVERRIDE_CM_DONE  mBIT(17)
#define	VXGE_HAL_NETERION_MEMBIST_CONTROL_INCLUDE_PCIE_MEMS mBIT(24)
#define	VXGE_HAL_NETERION_MEMBIST_CONTROL_LAUNCH	    mBIT(31)
#define	VXGE_HAL_NETERION_MEMBIST_CONTROL_NMBC_DONE	    mBIT(48)
#define	VXGE_HAL_NETERION_MEMBIST_CONTROL_NMBC_ERROR(val)   vBIT(val, 56, 4)
/* 0x08518 */	u64	neterion_membist_errors;
#define	VXGE_HAL_NETERION_MEMBIST_ERRORS_NMBC_CMG1(val)	    vBIT(val, 0, 3)
#define	VXGE_HAL_NETERION_MEMBIST_ERRORS_NMBC_CMG2(val)	    vBIT(val, 3, 3)
#define	VXGE_HAL_NETERION_MEMBIST_ERRORS_NMBC_CMG3(val)	    vBIT(val, 6, 3)
#define	VXGE_HAL_NETERION_MEMBIST_ERRORS_NMBC_DRBELL(val)   vBIT(val, 9, 3)
#define	VXGE_HAL_NETERION_MEMBIST_ERRORS_NMBC_FBif (val)    vBIT(val, 12, 3)
#define	VXGE_HAL_NETERION_MEMBIST_ERRORS_NMBC_MSG(val)	    vBIT(val, 15, 3)
#define	VXGE_HAL_NETERION_MEMBIST_ERRORS_NMBC_ONE(val)	    vBIT(val, 18, 3)
#define	VXGE_HAL_NETERION_MEMBIST_ERRORS_NMBC_PCI(val)	    vBIT(val, 21, 3)
#define	VXGE_HAL_NETERION_MEMBIST_ERRORS_NMBC_RTDMA(val)    vBIT(val, 24, 3)
#define	VXGE_HAL_NETERION_MEMBIST_ERRORS_NMBC_WRDMA(val)    vBIT(val, 27, 3)
#define	VXGE_HAL_NETERION_MEMBIST_ERRORS_NMBC_XGMAC(val)    vBIT(val, 30, 3)
#define	VXGE_HAL_NETERION_MEMBIST_ERRORS_NMBC_FB	    mBIT(33)
#define	VXGE_HAL_NETERION_MEMBIST_ERRORS_NMBC_CM	    mBIT(34)
/* 0x08520 */	u64	rr_cqm_cache_rtl_top_0;
#define	VXGE_HAL_RR_CQM_CACHE_RTL_TOP_0_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_CQM_CACHE_RTL_TOP_0_CMG1_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 8)
#define	VXGE_HAL_RR_CQM_CACHE_RTL_TOP_0_CMG1_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 10, 2)
#define	VXGE_HAL_RR_CQM_CACHE_RTL_TOP_0_CMG1_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 12, 8)
#define	VXGE_HAL_RR_CQM_CACHE_RTL_TOP_0_CMG1_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 20, 2)
/* 0x08528 */	u64	rr_cqm_cache_rtl_top_1;
#define	VXGE_HAL_RR_CQM_CACHE_RTL_TOP_1_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_CQM_CACHE_RTL_TOP_1_CMG1_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 8)
#define	VXGE_HAL_RR_CQM_CACHE_RTL_TOP_1_CMG1_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 10, 2)
#define	VXGE_HAL_RR_CQM_CACHE_RTL_TOP_1_CMG1_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 12, 8)
#define	VXGE_HAL_RR_CQM_CACHE_RTL_TOP_1_CMG1_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 20, 2)
/* 0x08530 */	u64	rr_sqm_cache_rtl_top_0;
#define	VXGE_HAL_RR_SQM_CACHE_RTL_TOP_0_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_SQM_CACHE_RTL_TOP_0_CMG1_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 8)
#define	VXGE_HAL_RR_SQM_CACHE_RTL_TOP_0_CMG1_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 10, 2)
#define	VXGE_HAL_RR_SQM_CACHE_RTL_TOP_0_CMG1_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 12, 8)
#define	VXGE_HAL_RR_SQM_CACHE_RTL_TOP_0_CMG1_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 20, 2)
/* 0x08538 */	u64	rr_sqm_cache_rtl_top_1;
#define	VXGE_HAL_RR_SQM_CACHE_RTL_TOP_1_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_SQM_CACHE_RTL_TOP_1_CMG1_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 8)
#define	VXGE_HAL_RR_SQM_CACHE_RTL_TOP_1_CMG1_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 10, 2)
#define	VXGE_HAL_RR_SQM_CACHE_RTL_TOP_1_CMG1_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 12, 8)
#define	VXGE_HAL_RR_SQM_CACHE_RTL_TOP_1_CMG1_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 20, 2)
/* 0x08540 */	u64	rf_sqm_lprpedat_rtl_top_0;
#define	VXGE_HAL_RF_SQM_LPRPEDAT_RTL_TOP_0_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SQM_LPRPEDAT_RTL_TOP_0_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08548 */	u64	rf_sqm_lprpedat_rtl_top_1;
#define	VXGE_HAL_RF_SQM_LPRPEDAT_RTL_TOP_1_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SQM_LPRPEDAT_RTL_TOP_1_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08550 */	u64	rr_sqm_dmawqersp_rtl_top_0;
#define	VXGE_HAL_RR_SQM_DMAWQERSP_RTL_TOP_0_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_SQM_DMAWQERSP_RTL_TOP_0_CMG1_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 7)
#define	VXGE_HAL_RR_SQM_DMAWQERSP_RTL_TOP_0_CMG1_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 9, 3)
#define	VXGE_HAL_RR_SQM_DMAWQERSP_RTL_TOP_0_CMG1_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 12, 7)
#define	VXGE_HAL_RR_SQM_DMAWQERSP_RTL_TOP_0_CMG1_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 19, 3)
/* 0x08558 */	u64	rr_sqm_dmawqersp_rtl_top_1;
#define	VXGE_HAL_RR_SQM_DMAWQERSP_RTL_TOP_1_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_SQM_DMAWQERSP_RTL_TOP_1_CMG1_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 7)
#define	VXGE_HAL_RR_SQM_DMAWQERSP_RTL_TOP_1_CMG1_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 9, 3)
#define	VXGE_HAL_RR_SQM_DMAWQERSP_RTL_TOP_1_CMG1_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 12, 7)
#define	VXGE_HAL_RR_SQM_DMAWQERSP_RTL_TOP_1_CMG1_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 19, 3)
/* 0x08560 */	u64	rf_cqm_dmacqersp_rtl_top;
#define	VXGE_HAL_RF_CQM_DMACQERSP_RTL_TOP_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_CQM_DMACQERSP_RTL_TOP_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08568 */	u64	rf_sqm_rpereqdat_rtl_top_0;
#define	VXGE_HAL_RF_SQM_RPEREQDAT_RTL_TOP_0_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SQM_RPEREQDAT_RTL_TOP_0_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08570 */	u64	rf_sqm_rpereqdat_rtl_top_1;
#define	VXGE_HAL_RF_SQM_RPEREQDAT_RTL_TOP_1_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SQM_RPEREQDAT_RTL_TOP_1_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08578 */	u64	rf_sscc_ssr_rtl_top_0_0;
#define	VXGE_HAL_RF_SSCC_SSR_RTL_TOP_0_0_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSCC_SSR_RTL_TOP_0_0_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08580 */	u64	rf_sscc_ssr_rtl_top_1_0;
#define	VXGE_HAL_RF_SSCC_SSR_RTL_TOP_1_0_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSCC_SSR_RTL_TOP_1_0_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08588 */	u64	rf_sscc_ssr_rtl_top_0_1;
#define	VXGE_HAL_RF_SSCC_SSR_RTL_TOP_0_1_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSCC_SSR_RTL_TOP_0_1_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08590 */	u64	rf_sscc_ssr_rtl_top_1_1;
#define	VXGE_HAL_RF_SSCC_SSR_RTL_TOP_1_1_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSCC_SSR_RTL_TOP_1_1_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08598 */	u64	rf_ssc_cm_resp_rtl_top_1_ssc0;
#define	VXGE_HAL_RF_SSC_CM_RESP_RTL_TOP_1_SSC0_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSC_CM_RESP_RTL_TOP_1_SSC0_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x085a0 */	u64	rf_ssc_cm_resp_rtl_top_0_ssc1;
#define	VXGE_HAL_RF_SSC_CM_RESP_RTL_TOP_0_SSC1_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSC_CM_RESP_RTL_TOP_0_SSC1_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x085a8 */	u64	rf_ssc_cm_resp_rtl_top_1_sscl;
#define	VXGE_HAL_RF_SSC_CM_RESP_RTL_TOP_1_SSCL_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSC_CM_RESP_RTL_TOP_1_SSCL_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x085b0 */	u64	rf_ssc_cm_resp_rtl_top_0_ssc0;
#define	VXGE_HAL_RF_SSC_CM_RESP_RTL_TOP_0_SSC0_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSC_CM_RESP_RTL_TOP_0_SSC0_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x085b8 */	u64	rf_ssc_cm_resp_rtl_top_1_ssc1;
#define	VXGE_HAL_RF_SSC_CM_RESP_RTL_TOP_1_SSC1_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSC_CM_RESP_RTL_TOP_1_SSC1_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x085c0 */	u64	rf_ssc_cm_resp_rtl_top_0_sscl;
#define	VXGE_HAL_RF_SSC_CM_RESP_RTL_TOP_0_SSCL_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSC_CM_RESP_RTL_TOP_0_SSCL_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x085c8 */	u64	rf_ssc_ssr_resp_rtl_top_ssc0;
#define	VXGE_HAL_RF_SSC_SSR_RESP_RTL_TOP_SSC0_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSC_SSR_RESP_RTL_TOP_SSC0_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x085d0 */	u64	rf_ssc_ssr_resp_rtl_top_ssc1;
#define	VXGE_HAL_RF_SSC_SSR_RESP_RTL_TOP_SSC1_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSC_SSR_RESP_RTL_TOP_SSC1_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x085d8 */	u64	rf_ssc_ssr_resp_rtl_top_sscl;
#define	VXGE_HAL_RF_SSC_SSR_RESP_RTL_TOP_SSCL_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSC_SSR_RESP_RTL_TOP_SSCL_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x085e0 */	u64	rf_ssc_tsr_resp_rtl_top_1_ssc0;
#define	VXGE_HAL_RF_SSC_TSR_RESP_RTL_TOP_1_SSC0_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSC_TSR_RESP_RTL_TOP_1_SSC0_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x085e8 */	u64	rf_ssc_tsr_resp_rtl_top_2_ssc0;
#define	VXGE_HAL_RF_SSC_TSR_RESP_RTL_TOP_2_SSC0_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSC_TSR_RESP_RTL_TOP_2_SSC0_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x085f0 */	u64	rf_ssc_tsr_resp_rtl_top_2_ssc1;
#define	VXGE_HAL_RF_SSC_TSR_RESP_RTL_TOP_2_SSC1_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSC_TSR_RESP_RTL_TOP_2_SSC1_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x085f8 */	u64	rf_ssc_tsr_resp_rtl_top_0_sscl;
#define	VXGE_HAL_RF_SSC_TSR_RESP_RTL_TOP_0_SSCL_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSC_TSR_RESP_RTL_TOP_0_SSCL_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08600 */	u64	rf_ssc_tsr_resp_rtl_top_0_ssc0;
#define	VXGE_HAL_RF_SSC_TSR_RESP_RTL_TOP_0_SSC0_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSC_TSR_RESP_RTL_TOP_0_SSC0_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08608 */	u64	rf_ssc_tsr_resp_rtl_top_0_ssc1;
#define	VXGE_HAL_RF_SSC_TSR_RESP_RTL_TOP_0_SSC1_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSC_TSR_RESP_RTL_TOP_0_SSC1_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08610 */	u64	rf_ssc_tsr_resp_rtl_top_1_ssc1;
#define	VXGE_HAL_RF_SSC_TSR_RESP_RTL_TOP_1_SSC1_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSC_TSR_RESP_RTL_TOP_1_SSC1_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08618 */	u64	rf_ssc_tsr_resp_rtl_top_1_sscl;
#define	VXGE_HAL_RF_SSC_TSR_RESP_RTL_TOP_1_SSCL_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSC_TSR_RESP_RTL_TOP_1_SSCL_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08620 */	u64	rf_ssc_tsr_resp_rtl_top_2_sscl;
#define	VXGE_HAL_RF_SSC_TSR_RESP_RTL_TOP_2_SSCL_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSC_TSR_RESP_RTL_TOP_2_SSCL_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08628 */	u64	rf_ssc_state_rtl_top_1_ssc0;
#define	VXGE_HAL_RF_SSC_STATE_RTL_TOP_1_SSC0_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSC_STATE_RTL_TOP_1_SSC0_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08630 */	u64	rf_ssc_state_rtl_top_2_ssc0;
#define	VXGE_HAL_RF_SSC_STATE_RTL_TOP_2_SSC0_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSC_STATE_RTL_TOP_2_SSC0_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08638 */	u64	rf_ssc_state_rtl_top_1_ssc1;
#define	VXGE_HAL_RF_SSC_STATE_RTL_TOP_1_SSC1_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSC_STATE_RTL_TOP_1_SSC1_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08640 */	u64	rf_ssc_state_rtl_top_2_ssc1;
#define	VXGE_HAL_RF_SSC_STATE_RTL_TOP_2_SSC1_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSC_STATE_RTL_TOP_2_SSC1_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08648 */	u64	rf_ssc_state_rtl_top_1_sscl;
#define	VXGE_HAL_RF_SSC_STATE_RTL_TOP_1_SSCL_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSC_STATE_RTL_TOP_1_SSCL_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08650 */	u64	rf_ssc_state_rtl_top_2_sscl;
#define	VXGE_HAL_RF_SSC_STATE_RTL_TOP_2_SSCL_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSC_STATE_RTL_TOP_2_SSCL_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08658 */	u64	rf_ssc_state_rtl_top_0_ssc0;
#define	VXGE_HAL_RF_SSC_STATE_RTL_TOP_0_SSC0_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSC_STATE_RTL_TOP_0_SSC0_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08660 */	u64	rf_ssc_state_rtl_top_3_ssc0;
#define	VXGE_HAL_RF_SSC_STATE_RTL_TOP_3_SSC0_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSC_STATE_RTL_TOP_3_SSC0_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08668 */	u64	rf_ssc_state_rtl_top_0_ssc1;
#define	VXGE_HAL_RF_SSC_STATE_RTL_TOP_0_SSC1_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSC_STATE_RTL_TOP_0_SSC1_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08670 */	u64	rf_ssc_state_rtl_top_3_ssc1;
#define	VXGE_HAL_RF_SSC_STATE_RTL_TOP_3_SSC1_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSC_STATE_RTL_TOP_3_SSC1_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08678 */	u64	rf_ssc_state_rtl_top_0_sscl;
#define	VXGE_HAL_RF_SSC_STATE_RTL_TOP_0_SSCL_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSC_STATE_RTL_TOP_0_SSCL_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08680 */	u64	rf_ssc_state_rtl_top_3_sscl;
#define	VXGE_HAL_RF_SSC_STATE_RTL_TOP_3_SSCL_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSC_STATE_RTL_TOP_3_SSCL_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08688 */	u64	rf_sscc_tsr_rtl_top_0;
#define	VXGE_HAL_RF_SSCC_TSR_RTL_TOP_0_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSCC_TSR_RTL_TOP_0_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08690 */	u64	rf_sscc_tsr_rtl_top_1;
#define	VXGE_HAL_RF_SSCC_TSR_RTL_TOP_1_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSCC_TSR_RTL_TOP_1_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08698 */	u64	rf_sscc_tsr_rtl_top_2;
#define	VXGE_HAL_RF_SSCC_TSR_RTL_TOP_2_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_SSCC_TSR_RTL_TOP_2_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x086a0 */	u64	rf_uqm_cmcreq_rtl_top;
#define	VXGE_HAL_RF_UQM_CMCREQ_RTL_TOP_CMG1_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_UQM_CMCREQ_RTL_TOP_CMG1_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x086a8 */	u64	rr0_g3if_cm_ctrl_rtl_top;
#define	VXGE_HAL_RR0_G3IF_CM_CTRL_RTL_TOP_CMG2_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR0_G3IF_CM_CTRL_RTL_TOP_CMG2_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x086b0 */	u64	rr1_g3if_cm_ctrl_rtl_top;
#define	VXGE_HAL_RR1_G3IF_CM_CTRL_RTL_TOP_CMG2_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR1_G3IF_CM_CTRL_RTL_TOP_CMG2_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x086b8 */	u64	rr2_g3if_cm_ctrl_rtl_top;
#define	VXGE_HAL_RR2_G3IF_CM_CTRL_RTL_TOP_CMG2_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR2_G3IF_CM_CTRL_RTL_TOP_CMG2_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x086c0 */	u64	rf_g3if_cm_rd_rtl_top0;
#define	VXGE_HAL_RF_G3IF_CM_RD_RTL_TOP0_CMG2_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_G3IF_CM_RD_RTL_TOP0_CMG2_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x086c8 */	u64	rf_g3if_cm_rd_rtl_top1;
#define	VXGE_HAL_RF_G3IF_CM_RD_RTL_TOP1_CMG2_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_G3IF_CM_RD_RTL_TOP1_CMG2_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x086d0 */	u64	rf_g3if_cm_rd_rtl_top2;
#define	VXGE_HAL_RF_G3IF_CM_RD_RTL_TOP2_CMG2_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_G3IF_CM_RD_RTL_TOP2_CMG2_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x086d8 */	u64	rf_cmg_msg2cmg_rtl_top_0_0;
#define	VXGE_HAL_RF_CMG_MSG2CMG_RTL_TOP_0_0_CMG2_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_CMG_MSG2CMG_RTL_TOP_0_0_CMG2_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x086e0 */	u64	rf_cmg_msg2cmg_rtl_top_1_0;
#define	VXGE_HAL_RF_CMG_MSG2CMG_RTL_TOP_1_0_CMG2_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_CMG_MSG2CMG_RTL_TOP_1_0_CMG2_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x086e8 */	u64	rf_cmg_msg2cmg_rtl_top_0_1;
#define	VXGE_HAL_RF_CMG_MSG2CMG_RTL_TOP_0_1_CMG2_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_CMG_MSG2CMG_RTL_TOP_0_1_CMG2_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x086f0 */	u64	rf_cmg_msg2cmg_rtl_top_1_1;
#define	VXGE_HAL_RF_CMG_MSG2CMG_RTL_TOP_1_1_CMG2_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_CMG_MSG2CMG_RTL_TOP_1_1_CMG2_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x086f8 */	u64	rf_cp_dma_resp_rtl_top_0;
#define	VXGE_HAL_RF_CP_DMA_RESP_RTL_TOP_0_CMG2_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_CP_DMA_RESP_RTL_TOP_0_CMG2_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08700 */	u64	rf_cp_dma_resp_rtl_top_1;
#define	VXGE_HAL_RF_CP_DMA_RESP_RTL_TOP_1_CMG2_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_CP_DMA_RESP_RTL_TOP_1_CMG2_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08708 */	u64	rf_cp_dma_resp_rtl_top_2;
#define	VXGE_HAL_RF_CP_DMA_RESP_RTL_TOP_2_CMG2_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_CP_DMA_RESP_RTL_TOP_2_CMG2_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08710 */	u64	rf_cp_qcc2cxp_rtl_top;
#define	VXGE_HAL_RF_CP_QCC2CXP_RTL_TOP_CMG2_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_CP_QCC2CXP_RTL_TOP_CMG2_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08718 */	u64	rf_cp_stc2cp_rtl_top;
#define	VXGE_HAL_RF_CP_STC2CP_RTL_TOP_CMG2_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_CP_STC2CP_RTL_TOP_CMG2_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08720 */	u64	rf_cp_xt_trace_rtl_top;
#define	VXGE_HAL_RF_CP_XT_TRACE_RTL_TOP_CMG2_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_CP_XT_TRACE_RTL_TOP_CMG2_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08728 */	u64	rf_cp_xt_dtag_rtl_top;
#define	VXGE_HAL_RF_CP_XT_DTAG_RTL_TOP_CMG2_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_CP_XT_DTAG_RTL_TOP_CMG2_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08730 */	u64	rf_cp_xt_icache_rtl_top_0_0;
#define	VXGE_HAL_RF_CP_XT_ICACHE_RTL_TOP_0_0_CMG2_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_CP_XT_ICACHE_RTL_TOP_0_0_CMG2_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08738 */	u64	rf_cp_xt_icache_rtl_top_1_0;
#define	VXGE_HAL_RF_CP_XT_ICACHE_RTL_TOP_1_0_CMG2_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_CP_XT_ICACHE_RTL_TOP_1_0_CMG2_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08740 */	u64	rf_cp_xt_icache_rtl_top_0_1;
#define	VXGE_HAL_RF_CP_XT_ICACHE_RTL_TOP_0_1_CMG2_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_CP_XT_ICACHE_RTL_TOP_0_1_CMG2_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08748 */	u64	rf_cp_xt_icache_rtl_top_1_1;
#define	VXGE_HAL_RF_CP_XT_ICACHE_RTL_TOP_1_1_CMG2_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_CP_XT_ICACHE_RTL_TOP_1_1_CMG2_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08750 */	u64	rf_cp_xt_itag_rtl_top;
#define	VXGE_HAL_RF_CP_XT_ITAG_RTL_TOP_CMG2_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_CP_XT_ITAG_RTL_TOP_CMG2_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08758 */	u64	rf_cp_xt_dcache_rtl_top_0_0;
#define	VXGE_HAL_RF_CP_XT_DCACHE_RTL_TOP_0_0_CMG2_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_CP_XT_DCACHE_RTL_TOP_0_0_CMG2_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08760 */	u64	rf_cp_xt_dcache_rtl_top_1_0;
#define	VXGE_HAL_RF_CP_XT_DCACHE_RTL_TOP_1_0_CMG2_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_CP_XT_DCACHE_RTL_TOP_1_0_CMG2_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08768 */	u64	rf_cp_xt_dcache_rtl_top_0_1;
#define	VXGE_HAL_RF_CP_XT_DCACHE_RTL_TOP_0_1_CMG2_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_CP_XT_DCACHE_RTL_TOP_0_1_CMG2_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08770 */	u64	rf_cp_xt_dcache_rtl_top_1_1;
#define	VXGE_HAL_RF_CP_XT_DCACHE_RTL_TOP_1_1_CMG2_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_CP_XT_DCACHE_RTL_TOP_1_1_CMG2_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08778 */	u64	rf_xtmc_bdt_mem_rtl_top_0;
#define	VXGE_HAL_RF_XTMC_BDT_MEM_RTL_TOP_0_CMG2_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_XTMC_BDT_MEM_RTL_TOP_0_CMG2_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08780 */	u64	rf_xtmc_bdt_mem_rtl_top_1;
#define	VXGE_HAL_RF_XTMC_BDT_MEM_RTL_TOP_1_CMG2_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_XTMC_BDT_MEM_RTL_TOP_1_CMG2_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08788 */	u64	rf_xt_pif_sram_rtl_top_sram0;
#define	VXGE_HAL_RF_XT_PIF_SRAM_RTL_TOP_SRAM0_CMG2_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_XT_PIF_SRAM_RTL_TOP_SRAM0_CMG2_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08790 */	u64	rf_xt_pif_sram_rtl_top_sram1;
#define	VXGE_HAL_RF_XT_PIF_SRAM_RTL_TOP_SRAM1_CMG2_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_XT_PIF_SRAM_RTL_TOP_SRAM1_CMG2_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08798 */	u64	rf_stc_srch_mem_rtl_top_0_0;
#define	VXGE_HAL_RF_STC_SRCH_MEM_RTL_TOP_0_0_CMG3_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_STC_SRCH_MEM_RTL_TOP_0_0_CMG3_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x087a0 */	u64	rf_stc_srch_mem_rtl_top_1_0;
#define	VXGE_HAL_RF_STC_SRCH_MEM_RTL_TOP_1_0_CMG3_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_STC_SRCH_MEM_RTL_TOP_1_0_CMG3_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x087a8 */	u64	rf_stc_srch_mem_rtl_top_0_1;
#define	VXGE_HAL_RF_STC_SRCH_MEM_RTL_TOP_0_1_CMG3_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_STC_SRCH_MEM_RTL_TOP_0_1_CMG3_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x087b0 */	u64	rf_stc_srch_mem_rtl_top_1_1;
#define	VXGE_HAL_RF_STC_SRCH_MEM_RTL_TOP_1_1_CMG3_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_STC_SRCH_MEM_RTL_TOP_1_1_CMG3_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x087b8 */	u64	rf_dam_wrresp_rtl_top;
#define	VXGE_HAL_RF_DAM_WRRESP_RTL_TOP_CMG3_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_DAM_WRRESP_RTL_TOP_CMG3_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x087c0 */	u64	rf_dam_rdsb_fifo_rtl_top;
#define	VXGE_HAL_RF_DAM_RDSB_FIFO_RTL_TOP_CMG3_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_DAM_RDSB_FIFO_RTL_TOP_CMG3_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x087c8 */	u64	rf_dam_wrsb_fifo_rtl_top;
#define	VXGE_HAL_RF_DAM_WRSB_FIFO_RTL_TOP_CMG3_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_DAM_WRSB_FIFO_RTL_TOP_CMG3_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x087d0 */	u64	rr_dbf_ladd_0_dbl_rtl_top;
#define	VXGE_HAL_RR_DBF_LADD_0_DBL_RTL_TOP_DRBELL_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_DBF_LADD_0_DBL_RTL_TOP_DRBELL_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 6)
#define	VXGE_HAL_RR_DBF_LADD_0_DBL_RTL_TOP_DRBELL_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 8, 5)
#define	VXGE_HAL_RR_DBF_LADD_0_DBL_RTL_TOP_DRBELL_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 13, 6)
#define	VXGE_HAL_RR_DBF_LADD_0_DBL_RTL_TOP_DRBELL_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 19, 5)
/* 0x087d8 */	u64	rr_dbf_ladd_1_dbl_rtl_top;
#define	VXGE_HAL_RR_DBF_LADD_1_DBL_RTL_TOP_DRBELL_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_DBF_LADD_1_DBL_RTL_TOP_DRBELL_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 6)
#define	VXGE_HAL_RR_DBF_LADD_1_DBL_RTL_TOP_DRBELL_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 8, 5)
#define	VXGE_HAL_RR_DBF_LADD_1_DBL_RTL_TOP_DRBELL_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 13, 6)
#define	VXGE_HAL_RR_DBF_LADD_1_DBL_RTL_TOP_DRBELL_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 19, 5)
/* 0x087e0 */	u64	rr_dbf_ladd_2_dbl_rtl_top;
#define	VXGE_HAL_RR_DBF_LADD_2_DBL_RTL_TOP_DRBELL_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_DBF_LADD_2_DBL_RTL_TOP_DRBELL_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 6)
#define	VXGE_HAL_RR_DBF_LADD_2_DBL_RTL_TOP_DRBELL_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 8, 5)
#define	VXGE_HAL_RR_DBF_LADD_2_DBL_RTL_TOP_DRBELL_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 13, 6)
#define	VXGE_HAL_RR_DBF_LADD_2_DBL_RTL_TOP_DRBELL_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 19, 5)
/* 0x087e8 */	u64	rr_dbf_hadd_0_dbl_rtl_top;
#define	VXGE_HAL_RR_DBF_HADD_0_DBL_RTL_TOP_DRBELL_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_DBF_HADD_0_DBL_RTL_TOP_DRBELL_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 6)
#define	VXGE_HAL_RR_DBF_HADD_0_DBL_RTL_TOP_DRBELL_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 8, 5)
#define	VXGE_HAL_RR_DBF_HADD_0_DBL_RTL_TOP_DRBELL_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 13, 6)
#define	VXGE_HAL_RR_DBF_HADD_0_DBL_RTL_TOP_DRBELL_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 19, 5)
/* 0x087f0 */	u64	rr_dbf_hadd_1_dbl_rtl_top;
#define	VXGE_HAL_RR_DBF_HADD_1_DBL_RTL_TOP_DRBELL_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_DBF_HADD_1_DBL_RTL_TOP_DRBELL_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 6)
#define	VXGE_HAL_RR_DBF_HADD_1_DBL_RTL_TOP_DRBELL_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 8, 5)
#define	VXGE_HAL_RR_DBF_HADD_1_DBL_RTL_TOP_DRBELL_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 13, 6)
#define	VXGE_HAL_RR_DBF_HADD_1_DBL_RTL_TOP_DRBELL_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 19, 5)
/* 0x087f8 */	u64	rr_dbf_hadd_2_dbl_rtl_top;
#define	VXGE_HAL_RR_DBF_HADD_2_DBL_RTL_TOP_DRBELL_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_DBF_HADD_2_DBL_RTL_TOP_DRBELL_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 6)
#define	VXGE_HAL_RR_DBF_HADD_2_DBL_RTL_TOP_DRBELL_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 8, 5)
#define	VXGE_HAL_RR_DBF_HADD_2_DBL_RTL_TOP_DRBELL_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 13, 6)
#define	VXGE_HAL_RR_DBF_HADD_2_DBL_RTL_TOP_DRBELL_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 19, 5)
/* 0x08800 */	u64	rf_usdc_0_fifo_rtl_top;
#define	VXGE_HAL_RF_USDC_0_FIFO_RTL_TOP_DRBELL_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_USDC_0_FIFO_RTL_TOP_DRBELL_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08808 */	u64	rf_usdc_1_fifo_rtl_top;
#define	VXGE_HAL_RF_USDC_1_FIFO_RTL_TOP_DRBELL_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_USDC_1_FIFO_RTL_TOP_DRBELL_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08810 */	u64	rf_usdc_0_wa_rtl_top;
#define	VXGE_HAL_RF_USDC_0_WA_RTL_TOP_DRBELL_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_USDC_0_WA_RTL_TOP_DRBELL_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08818 */	u64	rf_usdc_1_wa_rtl_top;
#define	VXGE_HAL_RF_USDC_1_WA_RTL_TOP_DRBELL_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_USDC_1_WA_RTL_TOP_DRBELL_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08820 */	u64	rf_usdc_0_sa_rtl_top;
#define	VXGE_HAL_RF_USDC_0_SA_RTL_TOP_DRBELL_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_USDC_0_SA_RTL_TOP_DRBELL_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08828 */	u64	rf_usdc_1_sa_rtl_top;
#define	VXGE_HAL_RF_USDC_1_SA_RTL_TOP_DRBELL_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_USDC_1_SA_RTL_TOP_DRBELL_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08830 */	u64	rf_usdc_0_ca_rtl_top;
#define	VXGE_HAL_RF_USDC_0_CA_RTL_TOP_DRBELL_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_USDC_0_CA_RTL_TOP_DRBELL_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08838 */	u64	rf_usdc_1_ca_rtl_top;
#define	VXGE_HAL_RF_USDC_1_CA_RTL_TOP_DRBELL_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_USDC_1_CA_RTL_TOP_DRBELL_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08840 */	u64	rf_g3if_fb_rd1;
#define	VXGE_HAL_RF_G3IF_FB_RD1_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_G3IF_FB_RD1_FBIF_NMB_IO_ALL_FUSE(val)   vBIT(val, 2, 8)
/* 0x08848 */	u64	rf_g3if_fb_rd2;
#define	VXGE_HAL_RF_G3IF_FB_RD2_FBIF_NMB_IO_REPAIR_STATUS(val) vBIT(val, 0, 2)
#define	VXGE_HAL_RF_G3IF_FB_RD2_FBIF_NMB_IO_ALL_FUSE(val)   vBIT(val, 2, 8)
/* 0x08850 */	u64	rf_g3if_fb_ctrl_rtl_top1;
#define	VXGE_HAL_RF_G3IF_FB_CTRL_RTL_TOP1_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_G3IF_FB_CTRL_RTL_TOP1_FBIF_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08858 */	u64	rf_g3if_fb_ctrl_rtl_top;
#define	VXGE_HAL_RF_G3IF_FB_CTRL_RTL_TOP_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_G3IF_FB_CTRL_RTL_TOP_FBIF_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08860 */	u64	rr_rocrc_frmbuf_rtl_top_0;
#define	VXGE_HAL_RR_ROCRC_FRMBUF_RTL_TOP_0_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_ROCRC_FRMBUF_RTL_TOP_0_FBIF_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 8)
#define	VXGE_HAL_RR_ROCRC_FRMBUF_RTL_TOP_0_FBIF_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 10, 2)
#define	VXGE_HAL_RR_ROCRC_FRMBUF_RTL_TOP_0_FBIF_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 12, 8)
#define	VXGE_HAL_RR_ROCRC_FRMBUF_RTL_TOP_0_FBIF_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 20, 2)
/* 0x08868 */	u64	rr_rocrc_frmbuf_rtl_top_1;
#define	VXGE_HAL_RR_ROCRC_FRMBUF_RTL_TOP_1_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_ROCRC_FRMBUF_RTL_TOP_1_FBIF_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 8)
#define	VXGE_HAL_RR_ROCRC_FRMBUF_RTL_TOP_1_FBIF_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 10, 2)
#define	VXGE_HAL_RR_ROCRC_FRMBUF_RTL_TOP_1_FBIF_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 12, 8)
#define	VXGE_HAL_RR_ROCRC_FRMBUF_RTL_TOP_1_FBIF_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 20, 2)
/* 0x08870 */	u64	rr_fau_xfmd_ins_rtl_top;
#define	VXGE_HAL_RR_FAU_XFMD_INS_RTL_TOP_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_FAU_XFMD_INS_RTL_TOP_FBIF_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 8)
#define	VXGE_HAL_RR_FAU_XFMD_INS_RTL_TOP_FBIF_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 10, 2)
#define	VXGE_HAL_RR_FAU_XFMD_INS_RTL_TOP_FBIF_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 12, 8)
#define	VXGE_HAL_RR_FAU_XFMD_INS_RTL_TOP_FBIF_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 20, 2)
/* 0x08878 */	u64	rf_fbmc_xfmd_rtl_top_a1;
#define	VXGE_HAL_RF_FBMC_XFMD_RTL_TOP_A1_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_FBMC_XFMD_RTL_TOP_A1_FBIF_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08880 */	u64	rf_fbmc_xfmd_rtl_top_a2;
#define	VXGE_HAL_RF_FBMC_XFMD_RTL_TOP_A2_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_FBMC_XFMD_RTL_TOP_A2_FBIF_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08888 */	u64	rf_fbmc_xfmd_rtl_top_a3;
#define	VXGE_HAL_RF_FBMC_XFMD_RTL_TOP_A3_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_FBMC_XFMD_RTL_TOP_A3_FBIF_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08890 */	u64	rf_fbmc_xfmd_rtl_top_b1;
#define	VXGE_HAL_RF_FBMC_XFMD_RTL_TOP_B1_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_FBMC_XFMD_RTL_TOP_B1_FBIF_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08898 */	u64	rf_fbmc_xfmd_rtl_top_b2;
#define	VXGE_HAL_RF_FBMC_XFMD_RTL_TOP_B2_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_FBMC_XFMD_RTL_TOP_B2_FBIF_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x088a0 */	u64	rf_fbmc_xfmd_rtl_top_b3;
#define	VXGE_HAL_RF_FBMC_XFMD_RTL_TOP_B3_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_FBMC_XFMD_RTL_TOP_B3_FBIF_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x088a8 */	u64	rr_fau_mac2f_w_h_rtl_top_port0;
#define	VXGE_HAL_RR_FAU_MAC2F_W_H_RTL_TOP_PORT0_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_FAU_MAC2F_W_H_RTL_TOP_PORT0_FBIF_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 8)
#define	VXGE_HAL_RR_FAU_MAC2F_W_H_RTL_TOP_PORT0_FBIF_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 10, 2)
#define	VXGE_HAL_RR_FAU_MAC2F_W_H_RTL_TOP_PORT0_FBIF_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 12, 8)
#define	VXGE_HAL_RR_FAU_MAC2F_W_H_RTL_TOP_PORT0_FBIF_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 20, 2)
/* 0x088b0 */	u64	rr_fau_mac2f_w_h_rtl_top_port1;
#define	VXGE_HAL_RR_FAU_MAC2F_W_H_RTL_TOP_PORT1_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_FAU_MAC2F_W_H_RTL_TOP_PORT1_FBIF_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 8)
#define	VXGE_HAL_RR_FAU_MAC2F_W_H_RTL_TOP_PORT1_FBIF_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 10, 2)
#define	VXGE_HAL_RR_FAU_MAC2F_W_H_RTL_TOP_PORT1_FBIF_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 12, 8)
#define	VXGE_HAL_RR_FAU_MAC2F_W_H_RTL_TOP_PORT1_FBIF_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 20, 2)
/* 0x088b8 */	u64	rr_fau_mac2f_n_h_rtl_top_port0;
#define	VXGE_HAL_RR_FAU_MAC2F_N_H_RTL_TOP_PORT0_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_FAU_MAC2F_N_H_RTL_TOP_PORT0_FBIF_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 7)
#define	VXGE_HAL_RR_FAU_MAC2F_N_H_RTL_TOP_PORT0_FBIF_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 9, 3)
#define	VXGE_HAL_RR_FAU_MAC2F_N_H_RTL_TOP_PORT0_FBIF_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 12, 7)
#define	VXGE_HAL_RR_FAU_MAC2F_N_H_RTL_TOP_PORT0_FBIF_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 19, 3)
/* 0x088c0 */	u64	rr_fau_mac2f_n_h_rtl_top_port1;
#define	VXGE_HAL_RR_FAU_MAC2F_N_H_RTL_TOP_PORT1_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_FAU_MAC2F_N_H_RTL_TOP_PORT1_FBIF_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 7)
#define	VXGE_HAL_RR_FAU_MAC2F_N_H_RTL_TOP_PORT1_FBIF_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 9, 3)
#define	VXGE_HAL_RR_FAU_MAC2F_N_H_RTL_TOP_PORT1_FBIF_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 12, 7)
#define	VXGE_HAL_RR_FAU_MAC2F_N_H_RTL_TOP_PORT1_FBIF_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 19, 3)
/* 0x088c8 */	u64	rr_fau_mac2f_w_l_rtl_top_port2;
#define	VXGE_HAL_RR_FAU_MAC2F_W_L_RTL_TOP_PORT2_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_FAU_MAC2F_W_L_RTL_TOP_PORT2_FBIF_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 8)
#define	VXGE_HAL_RR_FAU_MAC2F_W_L_RTL_TOP_PORT2_FBIF_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 10, 2)
#define	VXGE_HAL_RR_FAU_MAC2F_W_L_RTL_TOP_PORT2_FBIF_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 12, 8)
#define	VXGE_HAL_RR_FAU_MAC2F_W_L_RTL_TOP_PORT2_FBIF_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 20, 2)
/* 0x088d0 */	u64	rr_fau_mac2f_n_l_rtl_top_port2;
#define	VXGE_HAL_RR_FAU_MAC2F_N_L_RTL_TOP_PORT2_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_FAU_MAC2F_N_L_RTL_TOP_PORT2_FBIF_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 7)
#define	VXGE_HAL_RR_FAU_MAC2F_N_L_RTL_TOP_PORT2_FBIF_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 9, 3)
#define	VXGE_HAL_RR_FAU_MAC2F_N_L_RTL_TOP_PORT2_FBIF_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 12, 7)
#define	VXGE_HAL_RR_FAU_MAC2F_N_L_RTL_TOP_PORT2_FBIF_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 19, 3)
/* 0x088d8 */	u64	rf_orp_frm_fifo_rtl_top_0;
#define	VXGE_HAL_RF_ORP_FRM_FIFO_RTL_TOP_0_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_ORP_FRM_FIFO_RTL_TOP_0_FBIF_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x088e0 */	u64	rf_orp_frm_fifo_rtl_top_1;
#define	VXGE_HAL_RF_ORP_FRM_FIFO_RTL_TOP_1_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_ORP_FRM_FIFO_RTL_TOP_1_FBIF_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x088e8 */	u64	rf_tpa_da_lkp_rtl_top_0_0;
#define	VXGE_HAL_RF_TPA_DA_LKP_RTL_TOP_0_0_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_TPA_DA_LKP_RTL_TOP_0_0_FBIF_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x088f0 */	u64	rf_tpa_da_lkp_rtl_top_1_0;
#define	VXGE_HAL_RF_TPA_DA_LKP_RTL_TOP_1_0_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_TPA_DA_LKP_RTL_TOP_1_0_FBIF_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x088f8 */	u64	rf_tpa_da_lkp_rtl_top_0_1;
#define	VXGE_HAL_RF_TPA_DA_LKP_RTL_TOP_0_1_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_TPA_DA_LKP_RTL_TOP_0_1_FBIF_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08900 */	u64	rf_tpa_da_lkp_rtl_top_1_1;
#define	VXGE_HAL_RF_TPA_DA_LKP_RTL_TOP_1_1_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_TPA_DA_LKP_RTL_TOP_1_1_FBIF_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08908 */	u64	rf_tmac_tpa2mac_rtl_top_0_0;
#define	VXGE_HAL_RF_TMAC_TPA2MAC_RTL_TOP_0_0_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_TMAC_TPA2MAC_RTL_TOP_0_0_FBIF_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08910 */	u64	rf_tmac_tpa2mac_rtl_top_1_0;
#define	VXGE_HAL_RF_TMAC_TPA2MAC_RTL_TOP_1_0_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_TMAC_TPA2MAC_RTL_TOP_1_0_FBIF_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08918 */	u64	rf_tmac_tpa2mac_rtl_top_2_0;
#define	VXGE_HAL_RF_TMAC_TPA2MAC_RTL_TOP_2_0_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_TMAC_TPA2MAC_RTL_TOP_2_0_FBIF_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08920 */	u64	rf_tmac_tpa2mac_rtl_top_0_1;
#define	VXGE_HAL_RF_TMAC_TPA2MAC_RTL_TOP_0_1_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_TMAC_TPA2MAC_RTL_TOP_0_1_FBIF_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08928 */	u64	rf_tmac_tpa2mac_rtl_top_1_1;
#define	VXGE_HAL_RF_TMAC_TPA2MAC_RTL_TOP_1_1_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_TMAC_TPA2MAC_RTL_TOP_1_1_FBIF_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08930 */	u64	rf_tmac_tpa2mac_rtl_top_2_1;
#define	VXGE_HAL_RF_TMAC_TPA2MAC_RTL_TOP_2_1_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_TMAC_TPA2MAC_RTL_TOP_2_1_FBIF_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08938 */	u64	rf_tmac_tpa2mac_rtl_top_0_2;
#define	VXGE_HAL_RF_TMAC_TPA2MAC_RTL_TOP_0_2_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_TMAC_TPA2MAC_RTL_TOP_0_2_FBIF_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08940 */	u64	rf_tmac_tpa2mac_rtl_top_1_2;
#define	VXGE_HAL_RF_TMAC_TPA2MAC_RTL_TOP_1_2_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_TMAC_TPA2MAC_RTL_TOP_1_2_FBIF_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08948 */	u64	rf_tmac_tpa2mac_rtl_top_2_2;
#define	VXGE_HAL_RF_TMAC_TPA2MAC_RTL_TOP_2_2_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_TMAC_TPA2MAC_RTL_TOP_2_2_FBIF_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08950 */	u64	rf_tmac_tpa2m_da_rtl_top;
#define	VXGE_HAL_RF_TMAC_TPA2M_DA_RTL_TOP_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_TMAC_TPA2M_DA_RTL_TOP_FBIF_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08958 */	u64	rf_tmac_tpa2m_sb_rtl_top;
#define	VXGE_HAL_RF_TMAC_TPA2M_SB_RTL_TOP_FBIF_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_TMAC_TPA2M_SB_RTL_TOP_FBIF_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08960 */	u64	rf_xt_trace_rtl_top_mp;
#define	VXGE_HAL_RF_XT_TRACE_RTL_TOP_MP_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_XT_TRACE_RTL_TOP_MP_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08968 */	u64	rf_mp_xt_dtag_rtl_top;
#define	VXGE_HAL_RF_MP_XT_DTAG_RTL_TOP_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_MP_XT_DTAG_RTL_TOP_MSG_NMB_IO_ALL_FUSE(val) vBIT(val, 2, 6)
/* 0x08970 */	u64	rf_mp_xt_icache_rtl_top_0_0;
#define	VXGE_HAL_RF_MP_XT_ICACHE_RTL_TOP_0_0_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_MP_XT_ICACHE_RTL_TOP_0_0_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08978 */	u64	rf_mp_xt_icache_rtl_top_1_0;
#define	VXGE_HAL_RF_MP_XT_ICACHE_RTL_TOP_1_0_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_MP_XT_ICACHE_RTL_TOP_1_0_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08980 */	u64	rf_mp_xt_icache_rtl_top_0_1;
#define	VXGE_HAL_RF_MP_XT_ICACHE_RTL_TOP_0_1_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_MP_XT_ICACHE_RTL_TOP_0_1_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08988 */	u64	rf_mp_xt_icache_rtl_top_1_1;
#define	VXGE_HAL_RF_MP_XT_ICACHE_RTL_TOP_1_1_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_MP_XT_ICACHE_RTL_TOP_1_1_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08990 */	u64	rf_mp_xt_itag_rtl_top;
#define	VXGE_HAL_RF_MP_XT_ITAG_RTL_TOP_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_MP_XT_ITAG_RTL_TOP_MSG_NMB_IO_ALL_FUSE(val) vBIT(val, 2, 6)
/* 0x08998 */	u64	rf_mp_xt_dcache_rtl_top_0_0;
#define	VXGE_HAL_RF_MP_XT_DCACHE_RTL_TOP_0_0_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_MP_XT_DCACHE_RTL_TOP_0_0_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x089a0 */	u64	rf_mp_xt_dcache_rtl_top_1_0;
#define	VXGE_HAL_RF_MP_XT_DCACHE_RTL_TOP_1_0_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_MP_XT_DCACHE_RTL_TOP_1_0_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x089a8 */	u64	rf_mp_xt_dcache_rtl_top_0_1;
#define	VXGE_HAL_RF_MP_XT_DCACHE_RTL_TOP_0_1_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_MP_XT_DCACHE_RTL_TOP_0_1_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x089b0 */	u64	rf_mp_xt_dcache_rtl_top_1_1;
#define	VXGE_HAL_RF_MP_XT_DCACHE_RTL_TOP_1_1_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_MP_XT_DCACHE_RTL_TOP_1_1_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x089b8 */	u64	rf_msg_bwr_pf_rtl_top_0;
#define	VXGE_HAL_RF_MSG_BWR_PF_RTL_TOP_0_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_MSG_BWR_PF_RTL_TOP_0_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x089c0 */	u64	rf_msg_bwr_pf_rtl_top_1;
#define	VXGE_HAL_RF_MSG_BWR_PF_RTL_TOP_1_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_MSG_BWR_PF_RTL_TOP_1_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x089c8 */	u64	rf_msg_umq_rtl_top_0;
#define	VXGE_HAL_RF_MSG_UMQ_RTL_TOP_0_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_MSG_UMQ_RTL_TOP_0_MSG_NMB_IO_ALL_FUSE(val) vBIT(val, 2, 7)
/* 0x089d0 */	u64	rf_msg_umq_rtl_top_1;
#define	VXGE_HAL_RF_MSG_UMQ_RTL_TOP_1_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_MSG_UMQ_RTL_TOP_1_MSG_NMB_IO_ALL_FUSE(val) vBIT(val, 2, 7)
/* 0x089d8 */	u64	rf_msg_dmq_rtl_top_0;
#define	VXGE_HAL_RF_MSG_DMQ_RTL_TOP_0_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_MSG_DMQ_RTL_TOP_0_MSG_NMB_IO_ALL_FUSE(val) vBIT(val, 2, 6)
/* 0x089e0 */	u64	rf_msg_dmq_rtl_top_1;
#define	VXGE_HAL_RF_MSG_DMQ_RTL_TOP_1_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_MSG_DMQ_RTL_TOP_1_MSG_NMB_IO_ALL_FUSE(val) vBIT(val, 2, 6)
/* 0x089e8 */	u64	rf_msg_dmq_rtl_top_2;
#define	VXGE_HAL_RF_MSG_DMQ_RTL_TOP_2_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_MSG_DMQ_RTL_TOP_2_MSG_NMB_IO_ALL_FUSE(val) vBIT(val, 2, 6)
/* 0x089f0 */	u64	rf_msg_dma_resp_rtl_top_0;
#define	VXGE_HAL_RF_MSG_DMA_RESP_RTL_TOP_0_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_MSG_DMA_RESP_RTL_TOP_0_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x089f8 */	u64	rf_msg_dma_resp_rtl_top_1;
#define	VXGE_HAL_RF_MSG_DMA_RESP_RTL_TOP_1_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_MSG_DMA_RESP_RTL_TOP_1_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08a00 */	u64	rf_msg_dma_resp_rtl_top_2;
#define	VXGE_HAL_RF_MSG_DMA_RESP_RTL_TOP_2_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_MSG_DMA_RESP_RTL_TOP_2_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08a08 */	u64	rf_msg_cmg2msg_rtl_top_0_0;
#define	VXGE_HAL_RF_MSG_CMG2MSG_RTL_TOP_0_0_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_MSG_CMG2MSG_RTL_TOP_0_0_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08a10 */	u64	rf_msg_cmg2msg_rtl_top_1_0;
#define	VXGE_HAL_RF_MSG_CMG2MSG_RTL_TOP_1_0_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_MSG_CMG2MSG_RTL_TOP_1_0_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08a18 */	u64	rf_msg_cmg2msg_rtl_top_0_1;
#define	VXGE_HAL_RF_MSG_CMG2MSG_RTL_TOP_0_1_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_MSG_CMG2MSG_RTL_TOP_0_1_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08a20 */	u64	rf_msg_cmg2msg_rtl_top_1_1;
#define	VXGE_HAL_RF_MSG_CMG2MSG_RTL_TOP_1_1_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_MSG_CMG2MSG_RTL_TOP_1_1_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08a28 */	u64	rf_msg_txpe2msg_rtl_top;
#define	VXGE_HAL_RF_MSG_TXPE2MSG_RTL_TOP_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_MSG_TXPE2MSG_RTL_TOP_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08a30 */	u64	rf_msg_rxpe2msg_rtl_top;
#define	VXGE_HAL_RF_MSG_RXPE2MSG_RTL_TOP_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_MSG_RXPE2MSG_RTL_TOP_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08a38 */	u64	rf_msg_rpe2msg_rtl_top;
#define	VXGE_HAL_RF_MSG_RPE2MSG_RTL_TOP_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_MSG_RPE2MSG_RTL_TOP_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08a40 */	u64	rr_tim_bmap_rtl_top;
#define	VXGE_HAL_RR_TIM_BMAP_RTL_TOP_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_TIM_BMAP_RTL_TOP_MSG_NMB_IO_BANK1_FUSE(val) vBIT(val, 2, 8)
#define	VXGE_HAL_RR_TIM_BMAP_RTL_TOP_MSG_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 10, 2)
#define	VXGE_HAL_RR_TIM_BMAP_RTL_TOP_MSG_NMB_IO_BANK0_FUSE(val) vBIT(val, 12, 8)
#define	VXGE_HAL_RR_TIM_BMAP_RTL_TOP_MSG_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 20, 2)
/* 0x08a48 */	u64	rf_tim_vbls_rtl_top;
#define	VXGE_HAL_RF_TIM_VBLS_RTL_TOP_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_TIM_VBLS_RTL_TOP_MSG_NMB_IO_ALL_FUSE(val) vBIT(val, 2, 8)
/* 0x08a50 */	u64	rf_tim_bmap_msg_rtl_top_0_0;
#define	VXGE_HAL_RF_TIM_BMAP_MSG_RTL_TOP_0_0_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_TIM_BMAP_MSG_RTL_TOP_0_0_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08a58 */	u64	rf_tim_bmap_msg_rtl_top_1_0;
#define	VXGE_HAL_RF_TIM_BMAP_MSG_RTL_TOP_1_0_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_TIM_BMAP_MSG_RTL_TOP_1_0_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08a60 */	u64	rf_tim_bmap_msg_rtl_top_2_0;
#define	VXGE_HAL_RF_TIM_BMAP_MSG_RTL_TOP_2_0_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_TIM_BMAP_MSG_RTL_TOP_2_0_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08a68 */	u64	rf_tim_bmap_msg_rtl_top_0_1;
#define	VXGE_HAL_RF_TIM_BMAP_MSG_RTL_TOP_0_1_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_TIM_BMAP_MSG_RTL_TOP_0_1_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08a70 */	u64	rf_tim_bmap_msg_rtl_top_1_1;
#define	VXGE_HAL_RF_TIM_BMAP_MSG_RTL_TOP_1_1_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_TIM_BMAP_MSG_RTL_TOP_1_1_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08a78 */	u64	rf_tim_bmap_msg_rtl_top_2_1;
#define	VXGE_HAL_RF_TIM_BMAP_MSG_RTL_TOP_2_1_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_TIM_BMAP_MSG_RTL_TOP_2_1_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08a80 */	u64	rf_tim_bmap_msg_rtl_top_0_2;
#define	VXGE_HAL_RF_TIM_BMAP_MSG_RTL_TOP_0_2_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_TIM_BMAP_MSG_RTL_TOP_0_2_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08a88 */	u64	rf_tim_bmap_msg_rtl_top_1_2;
#define	VXGE_HAL_RF_TIM_BMAP_MSG_RTL_TOP_1_2_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_TIM_BMAP_MSG_RTL_TOP_1_2_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08a90 */	u64	rf_tim_bmap_msg_rtl_top_2_2;
#define	VXGE_HAL_RF_TIM_BMAP_MSG_RTL_TOP_2_2_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_TIM_BMAP_MSG_RTL_TOP_2_2_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08a98 */	u64	rf_tim_bmap_msg_rtl_top_0_3;
#define	VXGE_HAL_RF_TIM_BMAP_MSG_RTL_TOP_0_3_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_TIM_BMAP_MSG_RTL_TOP_0_3_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08aa0 */	u64	rf_tim_bmap_msg_rtl_top_1_3;
#define	VXGE_HAL_RF_TIM_BMAP_MSG_RTL_TOP_1_3_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_TIM_BMAP_MSG_RTL_TOP_1_3_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08aa8 */	u64	rf_tim_bmap_msg_rtl_top_2_3;
#define	VXGE_HAL_RF_TIM_BMAP_MSG_RTL_TOP_2_3_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_TIM_BMAP_MSG_RTL_TOP_2_3_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08ab0 */	u64	rf_tim_bmap_msg_rtl_top_0_4;
#define	VXGE_HAL_RF_TIM_BMAP_MSG_RTL_TOP_0_4_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_TIM_BMAP_MSG_RTL_TOP_0_4_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08ab8 */	u64	rf_tim_bmap_msg_rtl_top_1_4;
#define	VXGE_HAL_RF_TIM_BMAP_MSG_RTL_TOP_1_4_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_TIM_BMAP_MSG_RTL_TOP_1_4_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08ac0 */	u64	rf_tim_bmap_msg_rtl_top_2_4;
#define	VXGE_HAL_RF_TIM_BMAP_MSG_RTL_TOP_2_4_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_TIM_BMAP_MSG_RTL_TOP_2_4_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08ac8 */	u64	rf_xt_trace_rtl_top_up;
#define	VXGE_HAL_RF_XT_TRACE_RTL_TOP_UP_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_XT_TRACE_RTL_TOP_UP_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08ad0 */	u64	rf_up_xt_dtag_rtl_top;
#define	VXGE_HAL_RF_UP_XT_DTAG_RTL_TOP_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_UP_XT_DTAG_RTL_TOP_MSG_NMB_IO_ALL_FUSE(val) vBIT(val, 2, 6)
/* 0x08ad8 */	u64	rf_up_xt_icache_rtl_top_0_0;
#define	VXGE_HAL_RF_UP_XT_ICACHE_RTL_TOP_0_0_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_UP_XT_ICACHE_RTL_TOP_0_0_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08ae0 */	u64	rf_up_xt_icache_rtl_top_1_0;
#define	VXGE_HAL_RF_UP_XT_ICACHE_RTL_TOP_1_0_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_UP_XT_ICACHE_RTL_TOP_1_0_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08ae8 */	u64	rf_up_xt_icache_rtl_top_0_1;
#define	VXGE_HAL_RF_UP_XT_ICACHE_RTL_TOP_0_1_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_UP_XT_ICACHE_RTL_TOP_0_1_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08af0 */	u64	rf_up_xt_icache_rtl_top_1_1;
#define	VXGE_HAL_RF_UP_XT_ICACHE_RTL_TOP_1_1_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_UP_XT_ICACHE_RTL_TOP_1_1_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08af8 */	u64	rf_up_xt_itag_rtl_top;
#define	VXGE_HAL_RF_UP_XT_ITAG_RTL_TOP_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_UP_XT_ITAG_RTL_TOP_MSG_NMB_IO_ALL_FUSE(val) vBIT(val, 2, 6)
/* 0x08b00 */	u64	rf_up_xt_dcache_rtl_top_0_0;
#define	VXGE_HAL_RF_UP_XT_DCACHE_RTL_TOP_0_0_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_UP_XT_DCACHE_RTL_TOP_0_0_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08b08 */	u64	rf_up_xt_dcache_rtl_top_1_0;
#define	VXGE_HAL_RF_UP_XT_DCACHE_RTL_TOP_1_0_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_UP_XT_DCACHE_RTL_TOP_1_0_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08b10 */	u64	rf_up_xt_dcache_rtl_top_0_1;
#define	VXGE_HAL_RF_UP_XT_DCACHE_RTL_TOP_0_1_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_UP_XT_DCACHE_RTL_TOP_0_1_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08b18 */	u64	rf_up_xt_dcache_rtl_top_1_1;
#define	VXGE_HAL_RF_UP_XT_DCACHE_RTL_TOP_1_1_MSG_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_UP_XT_DCACHE_RTL_TOP_1_1_MSG_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08b20 */	u64	rr_rxpe_xt0_iram_rtl_top_0;
#define	VXGE_HAL_RR_RXPE_XT0_IRAM_RTL_TOP_0_ONE_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_RXPE_XT0_IRAM_RTL_TOP_0_ONE_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 7)
#define	VXGE_HAL_RR_RXPE_XT0_IRAM_RTL_TOP_0_ONE_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 9, 4)
#define	VXGE_HAL_RR_RXPE_XT0_IRAM_RTL_TOP_0_ONE_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 13, 7)
#define	VXGE_HAL_RR_RXPE_XT0_IRAM_RTL_TOP_0_ONE_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 20, 4)
/* 0x08b28 */	u64	rr_rxpe_xt0_iram_rtl_top_1;
#define	VXGE_HAL_RR_RXPE_XT0_IRAM_RTL_TOP_1_ONE_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_RXPE_XT0_IRAM_RTL_TOP_1_ONE_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 7)
#define	VXGE_HAL_RR_RXPE_XT0_IRAM_RTL_TOP_1_ONE_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 9, 4)
#define	VXGE_HAL_RR_RXPE_XT0_IRAM_RTL_TOP_1_ONE_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 13, 7)
#define	VXGE_HAL_RR_RXPE_XT0_IRAM_RTL_TOP_1_ONE_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 20, 4)
/* 0x08b30 */	u64	rr_rxpe_xt_dram_rtl_top_0;
#define	VXGE_HAL_RR_RXPE_XT_DRAM_RTL_TOP_0_ONE_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_RXPE_XT_DRAM_RTL_TOP_0_ONE_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 7)
#define	VXGE_HAL_RR_RXPE_XT_DRAM_RTL_TOP_0_ONE_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 9, 3)
#define	VXGE_HAL_RR_RXPE_XT_DRAM_RTL_TOP_0_ONE_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 12, 7)
#define	VXGE_HAL_RR_RXPE_XT_DRAM_RTL_TOP_0_ONE_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 19, 3)
/* 0x08b38 */	u64	rr_rxpe_xt_dram_rtl_top_1;
#define	VXGE_HAL_RR_RXPE_XT_DRAM_RTL_TOP_1_ONE_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_RXPE_XT_DRAM_RTL_TOP_1_ONE_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 7)
#define	VXGE_HAL_RR_RXPE_XT_DRAM_RTL_TOP_1_ONE_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 9, 3)
#define	VXGE_HAL_RR_RXPE_XT_DRAM_RTL_TOP_1_ONE_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 12, 7)
#define	VXGE_HAL_RR_RXPE_XT_DRAM_RTL_TOP_1_ONE_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 19, 3)
/* 0x08b40 */	u64	rf_rxpe_msg2rxpe_rtl_top_0;
#define	VXGE_HAL_RF_RXPE_MSG2RXPE_RTL_TOP_0_ONE_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RXPE_MSG2RXPE_RTL_TOP_0_ONE_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08b48 */	u64	rf_rxpe_msg2rxpe_rtl_top_1;
#define	VXGE_HAL_RF_RXPE_MSG2RXPE_RTL_TOP_1_ONE_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RXPE_MSG2RXPE_RTL_TOP_1_ONE_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08b50 */	u64	rf_rxpe_xt0_frm_rtl_top;
#define	VXGE_HAL_RF_RXPE_XT0_FRM_RTL_TOP_ONE_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RXPE_XT0_FRM_RTL_TOP_ONE_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08b58 */	u64	rf_rpe_pdm_rcmd_rtl_top;
#define	VXGE_HAL_RF_RPE_PDM_RCMD_RTL_TOP_ONE_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RPE_PDM_RCMD_RTL_TOP_ONE_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08b60 */	u64	rf_rpe_rcq_rtl_top;
#define	VXGE_HAL_RF_RPE_RCQ_RTL_TOP_ONE_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RPE_RCQ_RTL_TOP_ONE_NMB_IO_ALL_FUSE(val) vBIT(val, 2, 8)
/* 0x08b68 */	u64	rf_rpe_rco_pble_rtl_top;
#define	VXGE_HAL_RF_RPE_RCO_PBLE_RTL_TOP_ONE_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RPE_RCO_PBLE_RTL_TOP_ONE_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08b70 */	u64	rr_rxpe_xt1_iram_rtl_top_0;
#define	VXGE_HAL_RR_RXPE_XT1_IRAM_RTL_TOP_0_ONE_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_RXPE_XT1_IRAM_RTL_TOP_0_ONE_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 7)
#define	VXGE_HAL_RR_RXPE_XT1_IRAM_RTL_TOP_0_ONE_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 9, 4)
#define	VXGE_HAL_RR_RXPE_XT1_IRAM_RTL_TOP_0_ONE_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 13, 7)
#define	VXGE_HAL_RR_RXPE_XT1_IRAM_RTL_TOP_0_ONE_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 20, 4)
/* 0x08b78 */	u64	rr_rxpe_xt1_iram_rtl_top_1;
#define	VXGE_HAL_RR_RXPE_XT1_IRAM_RTL_TOP_1_ONE_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_RXPE_XT1_IRAM_RTL_TOP_1_ONE_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 7)
#define	VXGE_HAL_RR_RXPE_XT1_IRAM_RTL_TOP_1_ONE_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 9, 4)
#define	VXGE_HAL_RR_RXPE_XT1_IRAM_RTL_TOP_1_ONE_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 13, 7)
#define	VXGE_HAL_RR_RXPE_XT1_IRAM_RTL_TOP_1_ONE_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 20, 4)
/* 0x08b80 */	u64	rr_rpe_sccm_rtl_top_0;
#define	VXGE_HAL_RR_RPE_SCCM_RTL_TOP_0_ONE_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_RPE_SCCM_RTL_TOP_0_ONE_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 8)
#define	VXGE_HAL_RR_RPE_SCCM_RTL_TOP_0_ONE_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 10, 2)
#define	VXGE_HAL_RR_RPE_SCCM_RTL_TOP_0_ONE_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 12, 8)
#define	VXGE_HAL_RR_RPE_SCCM_RTL_TOP_0_ONE_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 20, 2)
/* 0x08b88 */	u64	rr_rpe_sccm_rtl_top_1;
#define	VXGE_HAL_RR_RPE_SCCM_RTL_TOP_1_ONE_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_RPE_SCCM_RTL_TOP_1_ONE_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 8)
#define	VXGE_HAL_RR_RPE_SCCM_RTL_TOP_1_ONE_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 10, 2)
#define	VXGE_HAL_RR_RPE_SCCM_RTL_TOP_1_ONE_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 12, 8)
#define	VXGE_HAL_RR_RPE_SCCM_RTL_TOP_1_ONE_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 20, 2)
/* 0x08b90 */	u64	rr_pe_pet_timer_rtl_top_0;
#define	VXGE_HAL_RR_PE_PET_TIMER_RTL_TOP_0_ONE_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_PE_PET_TIMER_RTL_TOP_0_ONE_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 7)
#define	VXGE_HAL_RR_PE_PET_TIMER_RTL_TOP_0_ONE_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 9, 3)
#define	VXGE_HAL_RR_PE_PET_TIMER_RTL_TOP_0_ONE_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 12, 7)
#define	VXGE_HAL_RR_PE_PET_TIMER_RTL_TOP_0_ONE_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 19, 3)
/* 0x08b98 */	u64	rr_pe_pet_timer_rtl_top_1;
#define	VXGE_HAL_RR_PE_PET_TIMER_RTL_TOP_1_ONE_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_PE_PET_TIMER_RTL_TOP_1_ONE_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 7)
#define	VXGE_HAL_RR_PE_PET_TIMER_RTL_TOP_1_ONE_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 9, 3)
#define	VXGE_HAL_RR_PE_PET_TIMER_RTL_TOP_1_ONE_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 12, 7)
#define	VXGE_HAL_RR_PE_PET_TIMER_RTL_TOP_1_ONE_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 19, 3)
/* 0x08ba0 */	u64	rf_pe_dlm_lwrq_rtl_top_0;
#define	VXGE_HAL_RF_PE_DLM_LWRQ_RTL_TOP_0_ONE_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PE_DLM_LWRQ_RTL_TOP_0_ONE_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08ba8 */	u64	rf_pe_dlm_lwrq_rtl_top_1;
#define	VXGE_HAL_RF_PE_DLM_LWRQ_RTL_TOP_1_ONE_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PE_DLM_LWRQ_RTL_TOP_1_ONE_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08bb0 */	u64	rf_txpe_msg2txpe_rtl_top_0;
#define	VXGE_HAL_RF_TXPE_MSG2TXPE_RTL_TOP_0_ONE_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_TXPE_MSG2TXPE_RTL_TOP_0_ONE_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08bb8 */	u64	rf_txpe_msg2txpe_rtl_top_1;
#define	VXGE_HAL_RF_TXPE_MSG2TXPE_RTL_TOP_1_ONE_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_TXPE_MSG2TXPE_RTL_TOP_1_ONE_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08bc0 */	u64	rf_pci_retry_buf_rtl_top_0;
#define	VXGE_HAL_RF_PCI_RETRY_BUF_RTL_TOP_0_PCI_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PCI_RETRY_BUF_RTL_TOP_0_PCI_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08bc8 */	u64	rf_pci_retry_buf_rtl_top_1;
#define	VXGE_HAL_RF_PCI_RETRY_BUF_RTL_TOP_1_PCI_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PCI_RETRY_BUF_RTL_TOP_1_PCI_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08bd0 */	u64	rf_pci_retry_buf_rtl_top_2;
#define	VXGE_HAL_RF_PCI_RETRY_BUF_RTL_TOP_2_PCI_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PCI_RETRY_BUF_RTL_TOP_2_PCI_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08bd8 */	u64	rf_pci_retry_buf_rtl_top_3;
#define	VXGE_HAL_RF_PCI_RETRY_BUF_RTL_TOP_3_PCI_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PCI_RETRY_BUF_RTL_TOP_3_PCI_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08be0 */	u64	rf_pci_retry_buf_rtl_top_4;
#define	VXGE_HAL_RF_PCI_RETRY_BUF_RTL_TOP_4_PCI_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PCI_RETRY_BUF_RTL_TOP_4_PCI_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08be8 */	u64	rf_pci_retry_buf_rtl_top_5;
#define	VXGE_HAL_RF_PCI_RETRY_BUF_RTL_TOP_5_PCI_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PCI_RETRY_BUF_RTL_TOP_5_PCI_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08bf0 */	u64	rf_pci_sot_buf_rtl_top;
#define	VXGE_HAL_RF_PCI_SOT_BUF_RTL_TOP_PCI_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PCI_SOT_BUF_RTL_TOP_PCI_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08bf8 */	u64	rf_pci_rx_ph_rtl_top;
#define	VXGE_HAL_RF_PCI_RX_PH_RTL_TOP_PCI_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PCI_RX_PH_RTL_TOP_PCI_NMB_IO_ALL_FUSE(val) vBIT(val, 2, 8)
/* 0x08c00 */	u64	rf_pci_rx_nph_rtl_top;
#define	VXGE_HAL_RF_PCI_RX_NPH_RTL_TOP_PCI_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PCI_RX_NPH_RTL_TOP_PCI_NMB_IO_ALL_FUSE(val) vBIT(val, 2, 8)
/* 0x08c08 */	u64	rf_pci_rx_pd_rtl_top_0;
#define	VXGE_HAL_RF_PCI_RX_PD_RTL_TOP_0_PCI_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PCI_RX_PD_RTL_TOP_0_PCI_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08c10 */	u64	rf_pci_rx_pd_rtl_top_1;
#define	VXGE_HAL_RF_PCI_RX_PD_RTL_TOP_1_PCI_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PCI_RX_PD_RTL_TOP_1_PCI_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08c18 */	u64	rf_pci_rx_pd_rtl_top_2;
#define	VXGE_HAL_RF_PCI_RX_PD_RTL_TOP_2_PCI_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PCI_RX_PD_RTL_TOP_2_PCI_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08c20 */	u64	rf_pci_rx_pd_rtl_top_3;
#define	VXGE_HAL_RF_PCI_RX_PD_RTL_TOP_3_PCI_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PCI_RX_PD_RTL_TOP_3_PCI_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08c28 */	u64	rf_pci_rx_pd_rtl_top_4;
#define	VXGE_HAL_RF_PCI_RX_PD_RTL_TOP_4_PCI_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PCI_RX_PD_RTL_TOP_4_PCI_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08c30 */	u64	rf_pci_rx_pd_rtl_top_5;
#define	VXGE_HAL_RF_PCI_RX_PD_RTL_TOP_5_PCI_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PCI_RX_PD_RTL_TOP_5_PCI_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08c38 */	u64	rf_pci_rx_pd_rtl_top_6;
#define	VXGE_HAL_RF_PCI_RX_PD_RTL_TOP_6_PCI_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PCI_RX_PD_RTL_TOP_6_PCI_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08c40 */	u64	rf_pci_rx_pd_rtl_top_7;
#define	VXGE_HAL_RF_PCI_RX_PD_RTL_TOP_7_PCI_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PCI_RX_PD_RTL_TOP_7_PCI_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08c48 */	u64	rf_pci_rx_pd_rtl_top_8;
#define	VXGE_HAL_RF_PCI_RX_PD_RTL_TOP_8_PCI_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PCI_RX_PD_RTL_TOP_8_PCI_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08c50 */	u64	rf_pci_rx_pd_rtl_top_9;
#define	VXGE_HAL_RF_PCI_RX_PD_RTL_TOP_9_PCI_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PCI_RX_PD_RTL_TOP_9_PCI_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08c58 */	u64	rf_pci_rx_pd_rtl_top_10;
#define	VXGE_HAL_RF_PCI_RX_PD_RTL_TOP_10_PCI_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PCI_RX_PD_RTL_TOP_10_PCI_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08c60 */	u64	rf_pci_rx_pd_rtl_top_11;
#define	VXGE_HAL_RF_PCI_RX_PD_RTL_TOP_11_PCI_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PCI_RX_PD_RTL_TOP_11_PCI_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08c68 */	u64	rf_pci_rx_npd_rtl_top_0;
#define	VXGE_HAL_RF_PCI_RX_NPD_RTL_TOP_0_PCI_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PCI_RX_NPD_RTL_TOP_0_PCI_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08c70 */	u64	rf_pci_rx_npd_rtl_top_1;
#define	VXGE_HAL_RF_PCI_RX_NPD_RTL_TOP_1_PCI_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PCI_RX_NPD_RTL_TOP_1_PCI_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08c78 */	u64	rf_pic_kdfc_dbl_rtl_top_0;
#define	VXGE_HAL_RF_PIC_KDFC_DBL_RTL_TOP_0_PCI_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PIC_KDFC_DBL_RTL_TOP_0_PCI_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08c80 */	u64	rf_pic_kdfc_dbl_rtl_top_1;
#define	VXGE_HAL_RF_PIC_KDFC_DBL_RTL_TOP_1_PCI_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PIC_KDFC_DBL_RTL_TOP_1_PCI_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08c88 */	u64	rf_pic_kdfc_dbl_rtl_top_2;
#define	VXGE_HAL_RF_PIC_KDFC_DBL_RTL_TOP_2_PCI_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PIC_KDFC_DBL_RTL_TOP_2_PCI_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08c90 */	u64	rf_pic_kdfc_dbl_rtl_top_3;
#define	VXGE_HAL_RF_PIC_KDFC_DBL_RTL_TOP_3_PCI_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PIC_KDFC_DBL_RTL_TOP_3_PCI_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08c98 */	u64	rf_pic_kdfc_dbl_rtl_top_4;
#define	VXGE_HAL_RF_PIC_KDFC_DBL_RTL_TOP_4_PCI_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PIC_KDFC_DBL_RTL_TOP_4_PCI_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08ca0 */	u64	rf_pcc_txdo_rtl_top_pcc0;
#define	VXGE_HAL_RF_PCC_TXDO_RTL_TOP_PCC0_RTDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PCC_TXDO_RTL_TOP_PCC0_RTDMA_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08ca8 */	u64	rf_pcc_txdo_rtl_top_pcc1;
#define	VXGE_HAL_RF_PCC_TXDO_RTL_TOP_PCC1_RTDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PCC_TXDO_RTL_TOP_PCC1_RTDMA_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08cb0 */	u64	rf_pcc_txdo_rtl_top_pcc2;
#define	VXGE_HAL_RF_PCC_TXDO_RTL_TOP_PCC2_RTDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PCC_TXDO_RTL_TOP_PCC2_RTDMA_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08cb8 */	u64	rf_pcc_txdo_rtl_top_pcc3;
#define	VXGE_HAL_RF_PCC_TXDO_RTL_TOP_PCC3_RTDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PCC_TXDO_RTL_TOP_PCC3_RTDMA_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08cc0 */	u64	rf_pcc_txdo_rtl_top_pcc4;
#define	VXGE_HAL_RF_PCC_TXDO_RTL_TOP_PCC4_RTDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PCC_TXDO_RTL_TOP_PCC4_RTDMA_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08cc8 */	u64	rf_pcc_txdo_rtl_top_pcc5;
#define	VXGE_HAL_RF_PCC_TXDO_RTL_TOP_PCC5_RTDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PCC_TXDO_RTL_TOP_PCC5_RTDMA_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08cd0 */	u64	rf_pcc_txdo_rtl_top_pcc6;
#define	VXGE_HAL_RF_PCC_TXDO_RTL_TOP_PCC6_RTDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PCC_TXDO_RTL_TOP_PCC6_RTDMA_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08cd8 */	u64	rf_pcc_txdo_rtl_top_pcc7;
#define	VXGE_HAL_RF_PCC_TXDO_RTL_TOP_PCC7_RTDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_PCC_TXDO_RTL_TOP_PCC7_RTDMA_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08ce0 */	u64	rr_pcc_ass_buf_rtl_top_pcc1;
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC1_RTDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC1_RTDMA_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 8)
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC1_RTDMA_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 10, 2)
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC1_RTDMA_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 12, 8)
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC1_RTDMA_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 20, 2)
/* 0x08ce8 */	u64	rr_pcc_ass_buf_rtl_top_pcc3;
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC3_RTDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC3_RTDMA_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 8)
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC3_RTDMA_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 10, 2)
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC3_RTDMA_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 12, 8)
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC3_RTDMA_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 20, 2)
/* 0x08cf0 */	u64	rr_pcc_ass_buf_rtl_top_pcc5;
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC5_RTDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC5_RTDMA_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 8)
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC5_RTDMA_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 10, 2)
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC5_RTDMA_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 12, 8)
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC5_RTDMA_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 20, 2)
/* 0x08cf8 */	u64	rr_pcc_ass_buf_rtl_top_pcc7;
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC7_RTDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC7_RTDMA_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 8)
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC7_RTDMA_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 10, 2)
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC7_RTDMA_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 12, 8)
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC7_RTDMA_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 20, 2)
/* 0x08d00 */	u64	rr_pcc_ass_buf_rtl_top_pcc0;
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC0_RTDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC0_RTDMA_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 8)
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC0_RTDMA_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 10, 2)
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC0_RTDMA_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 12, 8)
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC0_RTDMA_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 20, 2)
/* 0x08d08 */	u64	rr_pcc_ass_buf_rtl_top_pcc2;
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC2_RTDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC2_RTDMA_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 8)
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC2_RTDMA_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 10, 2)
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC2_RTDMA_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 12, 8)
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC2_RTDMA_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 20, 2)
/* 0x08d10 */	u64	rr_pcc_ass_buf_rtl_top_pcc6;
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC6_RTDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC6_RTDMA_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 8)
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC6_RTDMA_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 10, 2)
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC6_RTDMA_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 12, 8)
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC6_RTDMA_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 20, 2)
/* 0x08d18 */	u64	rr_pcc_ass_buf_rtl_top_pcc4;
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC4_RTDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC4_RTDMA_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 8)
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC4_RTDMA_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 10, 2)
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC4_RTDMA_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 12, 8)
#define	VXGE_HAL_RR_PCC_ASS_BUF_RTL_TOP_PCC4_RTDMA_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 20, 2)
/* 0x08d20 */	u64	rf_rocrc_cmdq_bp_rtl_top_0_wrapper0;
#define	VXGE_HAL_RF_ROCRC_CMDQ_BP_RTL_TOP_0_W0_WRDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_ROCRC_CMDQ_BP_RTL_TOP_0_W0_WRDMA_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08d28 */	u64	rf_rocrc_cmdq_bp_rtl_top_1_wrapper0;
#define	VXGE_HAL_RF_ROCRC_CMDQ_BP_RTL_TOP_1_W0_WRDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_ROCRC_CMDQ_BP_RTL_TOP_1_W0_WRDMA_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08d30 */	u64	rf_rocrc_cmdq_bp_rtl_top_2_wrapper0;
#define	VXGE_HAL_RF_ROCRC_CMDQ_BP_RTL_TOP_2_W0_WRDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_ROCRC_CMDQ_BP_RTL_TOP_2_WRAPPER0_WRDMA_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08d38 */	u64	rf_rocrc_cmdq_bp_rtl_top_0_wrapper1;
#define	VXGE_HAL_RF_ROCRC_CMDQ_BP_RTL_TOP_0_W1_WRDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_ROCRC_CMDQ_BP_RTL_TOP_0_W1_WRDMA_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08d40 */	u64	rf_rocrc_cmdq_bp_rtl_top_1_wrapper1;
#define	VXGE_HAL_RF_ROCRC_CMDQ_BP_RTL_TOP_1_W1_WRDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_ROCRC_CMDQ_BP_RTL_TOP_1_W1_WRDMA_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08d48 */	u64	rf_rocrc_cmdq_bp_rtl_top_2_wrapper1;
#define	VXGE_HAL_RF_ROCRC_CMDQ_BP_RTL_TOP_2_W1_WRDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_ROCRC_CMDQ_BP_RTL_TOP_2_W1_WRDMA_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08d50 */	u64	rf_rocrc_cmdq_bp_rtl_top_0_wrapper2;
#define	VXGE_HAL_RF_ROCRC_CMDQ_BP_RTL_TOP_0_W2_WRDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_ROCRC_CMDQ_BP_RTL_TOP_0_WRAPPER2_WRDMA_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08d58 */	u64	rf_rocrc_cmdq_bp_rtl_top_1_wrapper2;
#define	VXGE_HAL_RF_ROCRC_CMDQ_BP_RTL_TOP_1_W2_WRDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_ROCRC_CMDQ_BP_RTL_TOP_1_W2_WRDMA_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08d60 */	u64	rf_rocrc_cmdq_bp_rtl_top_2_wrapper2;
#define	VXGE_HAL_RF_ROCRC_CMDQ_BP_RTL_TOP_2_W2_WRDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_ROCRC_CMDQ_BP_RTL_TOP_2_W2_WRDMA_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08d68 */	u64	rr_rocrc_rxd_rtl_top_rxd0;
#define	VXGE_HAL_RR_ROCRC_RXD_RTL_TOP_RXD0_WRDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_ROCRC_RXD_RTL_TOP_RXD0_WRDMA_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 8)
#define	VXGE_HAL_RR_ROCRC_RXD_RTL_TOP_RXD0_WRDMA_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 10, 2)
#define	VXGE_HAL_RR_ROCRC_RXD_RTL_TOP_RXD0_WRDMA_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 12, 8)
#define	VXGE_HAL_RR_ROCRC_RXD_RTL_TOP_RXD0_WRDMA_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 20, 2)
/* 0x08d70 */	u64	rr_rocrc_rxd_rtl_top_rxd1;
#define	VXGE_HAL_RR_ROCRC_RXD_RTL_TOP_RXD1_WRDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_ROCRC_RXD_RTL_TOP_RXD1_WRDMA_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 8)
#define	VXGE_HAL_RR_ROCRC_RXD_RTL_TOP_RXD1_WRDMA_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 10, 2)
#define	VXGE_HAL_RR_ROCRC_RXD_RTL_TOP_RXD1_WRDMA_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 12, 8)
#define	VXGE_HAL_RR_ROCRC_RXD_RTL_TOP_RXD1_WRDMA_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 20, 2)
/* 0x08d78 */	u64	rf_rocrc_umq_mdq_rtl_top_0;
#define	VXGE_HAL_RF_ROCRC_UMQ_MDQ_RTL_TOP_0_WRDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_ROCRC_UMQ_MDQ_RTL_TOP_0_WRDMA_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08d80 */	u64	rf_rocrc_umq_mdq_rtl_top_1;
#define	VXGE_HAL_RF_ROCRC_UMQ_MDQ_RTL_TOP_1_WRDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_ROCRC_UMQ_MDQ_RTL_TOP_1_WRDMA_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08d88 */	u64	rf_rocrc_umq_mdq_rtl_top_2;
#define	VXGE_HAL_RF_ROCRC_UMQ_MDQ_RTL_TOP_2_WRDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_ROCRC_UMQ_MDQ_RTL_TOP_2_WRDMA_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08d90 */	u64	rf_rocrc_umq_mdq_rtl_top_3;
#define	VXGE_HAL_RF_ROCRC_UMQ_MDQ_RTL_TOP_3_WRDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_ROCRC_UMQ_MDQ_RTL_TOP_3_WRDMA_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08d98 */	u64	rf_rocrc_umq_mdq_rtl_top_4;
#define	VXGE_HAL_RF_ROCRC_UMQ_MDQ_RTL_TOP_4_WRDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_ROCRC_UMQ_MDQ_RTL_TOP_4_WRDMA_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08da0 */	u64	rf_rocrc_umq_mdq_rtl_top_5;
#define	VXGE_HAL_RF_ROCRC_UMQ_MDQ_RTL_TOP_5_WRDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_ROCRC_UMQ_MDQ_RTL_TOP_5_WRDMA_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08da8 */	u64	rf_rocrc_umq_mdq_rtl_top_6;
#define	VXGE_HAL_RF_ROCRC_UMQ_MDQ_RTL_TOP_6_WRDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_ROCRC_UMQ_MDQ_RTL_TOP_6_WRDMA_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08db0 */	u64	rf_rocrc_umq_mdq_rtl_top_7;
#define	VXGE_HAL_RF_ROCRC_UMQ_MDQ_RTL_TOP_7_WRDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_ROCRC_UMQ_MDQ_RTL_TOP_7_WRDMA_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08db8 */	u64	rf_rocrc_immdbuf_rtl_top;
#define	VXGE_HAL_RF_ROCRC_IMMDBUF_RTL_TOP_WRDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_ROCRC_IMMDBUF_RTL_TOP_WRDMA_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08dc0 */	u64	rf_rocrc_qcc_byp_rtl_top_0;
#define	VXGE_HAL_RF_ROCRC_QCC_BYP_RTL_TOP_0_WRDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_ROCRC_QCC_BYP_RTL_TOP_0_WRDMA_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08dc8 */	u64	rf_rocrc_qcc_byp_rtl_top_1;
#define	VXGE_HAL_RF_ROCRC_QCC_BYP_RTL_TOP_1_WRDMA_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_ROCRC_QCC_BYP_RTL_TOP_1_WRDMA_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08dd0 */	u64	rr_rmac_da_lkp_rtl_top_0;
#define	VXGE_HAL_RR_RMAC_DA_LKP_RTL_TOP_0_XGMAC_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_RMAC_DA_LKP_RTL_TOP_0_XGMAC_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 6)
#define	VXGE_HAL_RR_RMAC_DA_LKP_RTL_TOP_0_XGMAC_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 8, 2)
#define	VXGE_HAL_RR_RMAC_DA_LKP_RTL_TOP_0_XGMAC_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 10, 6)
#define	VXGE_HAL_RR_RMAC_DA_LKP_RTL_TOP_0_XGMAC_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 16, 2)
/* 0x08dd8 */	u64	rr_rmac_da_lkp_rtl_top_1;
#define	VXGE_HAL_RR_RMAC_DA_LKP_RTL_TOP_1_XGMAC_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_RMAC_DA_LKP_RTL_TOP_1_XGMAC_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 6)
#define	VXGE_HAL_RR_RMAC_DA_LKP_RTL_TOP_1_XGMAC_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 8, 2)
#define	VXGE_HAL_RR_RMAC_DA_LKP_RTL_TOP_1_XGMAC_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 10, 6)
#define	VXGE_HAL_RR_RMAC_DA_LKP_RTL_TOP_1_XGMAC_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 16, 2)
/* 0x08de0 */	u64	rr_rmac_da_lkp_rtl_top_2;
#define	VXGE_HAL_RR_RMAC_DA_LKP_RTL_TOP_2_XGMAC_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_RMAC_DA_LKP_RTL_TOP_2_XGMAC_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 6)
#define	VXGE_HAL_RR_RMAC_DA_LKP_RTL_TOP_2_XGMAC_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 8, 2)
#define	VXGE_HAL_RR_RMAC_DA_LKP_RTL_TOP_2_XGMAC_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 10, 6)
#define	VXGE_HAL_RR_RMAC_DA_LKP_RTL_TOP_2_XGMAC_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 16, 2)
/* 0x08de8 */	u64	rr_rmac_da_lkp_rtl_top_3;
#define	VXGE_HAL_RR_RMAC_DA_LKP_RTL_TOP_3_XGMAC_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_RMAC_DA_LKP_RTL_TOP_3_XGMAC_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 6)
#define	VXGE_HAL_RR_RMAC_DA_LKP_RTL_TOP_3_XGMAC_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 8, 2)
#define	VXGE_HAL_RR_RMAC_DA_LKP_RTL_TOP_3_XGMAC_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 10, 6)
#define	VXGE_HAL_RR_RMAC_DA_LKP_RTL_TOP_3_XGMAC_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 16, 2)
/* 0x08df0 */	u64	rr_rmac_pn_lkp_d_rtl_top;
#define	VXGE_HAL_RR_RMAC_PN_LKP_D_RTL_TOP_XGMAC_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RR_RMAC_PN_LKP_D_RTL_TOP_XGMAC_NMB_IO_BANK1_FUSE(val)\
							    vBIT(val, 2, 7)
#define	VXGE_HAL_RR_RMAC_PN_LKP_D_RTL_TOP_XGMAC_NMB_IO_BANK1_ADD_FUSE(val)\
							    vBIT(val, 9, 2)
#define	VXGE_HAL_RR_RMAC_PN_LKP_D_RTL_TOP_XGMAC_NMB_IO_BANK0_FUSE(val)\
							    vBIT(val, 11, 7)
#define	VXGE_HAL_RR_RMAC_PN_LKP_D_RTL_TOP_XGMAC_NMB_IO_BANK0_ADD_FUSE(val)\
							    vBIT(val, 18, 2)
/* 0x08df8 */	u64	rf_rmac_pn_lkp_s_rtl_top_0;
#define	VXGE_HAL_RF_RMAC_PN_LKP_S_RTL_TOP_0_XGMAC_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_PN_LKP_S_RTL_TOP_0_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08e00 */	u64	rf_rmac_pn_lkp_s_rtl_top_1;
#define	VXGE_HAL_RF_RMAC_PN_LKP_S_RTL_TOP_1_XGMAC_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_PN_LKP_S_RTL_TOP_1_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08e08 */	u64	rf_rmac_rth_lkp_rtl_top_0_0;
#define	VXGE_HAL_RF_RMAC_RTH_LKP_RTL_TOP_0_0_XGMAC_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_RTH_LKP_RTL_TOP_0_0_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08e10 */	u64	rf_rmac_rth_lkp_rtl_top_1_0;
#define	VXGE_HAL_RF_RMAC_RTH_LKP_RTL_TOP_1_0_XGMAC_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_RTH_LKP_RTL_TOP_1_0_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08e18 */	u64	rf_rmac_rth_lkp_rtl_top_0_1;
#define	VXGE_HAL_RF_RMAC_RTH_LKP_RTL_TOP_0_1_XGMAC_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_RTH_LKP_RTL_TOP_0_1_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08e20 */	u64	rf_rmac_rth_lkp_rtl_top_1_1;
#define	VXGE_HAL_RF_RMAC_RTH_LKP_RTL_TOP_1_1_XGMAC_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_RTH_LKP_RTL_TOP_1_1_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08e28 */	u64	rf_rmac_ds_lkp_rtl_top;
#define	VXGE_HAL_RF_RMAC_DS_LKP_RTL_TOP_XGMAC_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_DS_LKP_RTL_TOP_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08e30 */	u64	rf_rmac_rts_part_rtl_top_0_rmac0;
#define	VXGE_HAL_RF_RMAC_RTS_PART_RTL_TOP_0_RMAC0_XGMAC_NMB_IO_REP_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_RTS_PART_RTL_TOP_0_RMAC0_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08e38 */	u64	rf_rmac_rts_part_rtl_top_1_rmac0;
#define	VXGE_HAL_RF_RMAC_RTS_PART_RTL_TOP_1_RMAC0_XGMAC_NMB_IO_REP_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_RTS_PART_RTL_TOP_1_RMAC0_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08e40 */	u64	rf_rmac_rts_part_rtl_top_0_rmac1;
#define	VXGE_HAL_RF_RMAC_RTS_PART_RTL_TOP_0_RMAC1_XGMAC_NMB_IO_REP_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_RTS_PART_RTL_TOP_0_RMAC1_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08e48 */	u64	rf_rmac_rts_part_rtl_top_1_rmac1;
#define	VXGE_HAL_RF_RMAC_RTS_PART_RTL_TOP_1_RMAC1_XGMAC_NMB_IO_REP_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_RTS_PART_RTL_TOP_1_RMAC1_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08e50 */	u64	rf_rmac_rts_part_rtl_top_0_rmac2;
#define	VXGE_HAL_RF_RMAC_RTS_PART_RTL_TOP_0_RMAC2_XGMAC_NMB_IO_REP_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_RTS_PART_RTL_TOP_0_RMAC2_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08e58 */	u64	rf_rmac_rts_part_rtl_top_1_rmac2;
#define	VXGE_HAL_RF_RMAC_RTS_PART_RTL_TOP_1_RMAC2_XGMAC_NMB_IO_REP_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_RTS_PART_RTL_TOP_1_RMAC2_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08e60 */	u64	rf_rmac_rth_mask_rtl_top_0;
#define	VXGE_HAL_RF_RMAC_RTH_MASK_RTL_TOP_0_XGMAC_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_RTH_MASK_RTL_TOP_0_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08e68 */	u64	rf_rmac_rth_mask_rtl_top_1;
#define	VXGE_HAL_RF_RMAC_RTH_MASK_RTL_TOP_1_XGMAC_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_RTH_MASK_RTL_TOP_1_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08e70 */	u64	rf_rmac_rth_mask_rtl_top_2;
#define	VXGE_HAL_RF_RMAC_RTH_MASK_RTL_TOP_2_XGMAC_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_RTH_MASK_RTL_TOP_2_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08e78 */	u64	rf_rmac_rth_mask_rtl_top_3;
#define	VXGE_HAL_RF_RMAC_RTH_MASK_RTL_TOP_3_XGMAC_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_RTH_MASK_RTL_TOP_3_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 8)
/* 0x08e80 */	u64	rf_rmac_vid_lkp_rtl_top_0;
#define	VXGE_HAL_RF_RMAC_VID_LKP_RTL_TOP_0_XGMAC_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_VID_LKP_RTL_TOP_0_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08e88 */	u64	rf_rmac_vid_lkp_rtl_top_1;
#define	VXGE_HAL_RF_RMAC_VID_LKP_RTL_TOP_1_XGMAC_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_VID_LKP_RTL_TOP_1_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08e90 */	u64	rf_rmac_vid_lkp_rtl_top_2;
#define	VXGE_HAL_RF_RMAC_VID_LKP_RTL_TOP_2_XGMAC_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_VID_LKP_RTL_TOP_2_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08e98 */	u64	rf_rmac_vid_lkp_rtl_top_3;
#define	VXGE_HAL_RF_RMAC_VID_LKP_RTL_TOP_3_XGMAC_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_VID_LKP_RTL_TOP_3_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08ea0 */	u64	rf_rmac_vid_lkp_rtl_top_4;
#define	VXGE_HAL_RF_RMAC_VID_LKP_RTL_TOP_4_XGMAC_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_VID_LKP_RTL_TOP_4_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08ea8 */	u64	rf_rmac_vid_lkp_rtl_top_5;
#define	VXGE_HAL_RF_RMAC_VID_LKP_RTL_TOP_5_XGMAC_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_VID_LKP_RTL_TOP_5_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08eb0 */	u64	rf_rmac_vid_lkp_rtl_top_6;
#define	VXGE_HAL_RF_RMAC_VID_LKP_RTL_TOP_6_XGMAC_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_VID_LKP_RTL_TOP_6_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08eb8 */	u64	rf_rmac_vid_lkp_rtl_top_7;
#define	VXGE_HAL_RF_RMAC_VID_LKP_RTL_TOP_7_XGMAC_NMB_IO_REPAIR_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_VID_LKP_RTL_TOP_7_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 6)
/* 0x08ec0 */	u64	rf_rmac_stats_rtl_top_0_stats_0;
#define	VXGE_HAL_RF_RMAC_STATS_RTL_TOP_0_STATS_0_XGMAC_NMB_IO_REP_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_STATS_RTL_TOP_0_STATS_0_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08ec8 */	u64	rf_rmac_stats_rtl_top_1_stats_0;
#define	VXGE_HAL_RF_RMAC_STATS_RTL_TOP_1_STATS_0_XGMAC_NMB_IO_REP_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_STATS_RTL_TOP_1_STATS_0_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08ed0 */	u64	rf_rmac_stats_rtl_top_0_stats_1;
#define	VXGE_HAL_RF_RMAC_STATS_RTL_TOP_0_STATS_1_XGMAC_NMB_IO_REP_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_STATS_RTL_TOP_0_STATS_1_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08ed8 */	u64	rf_rmac_stats_rtl_top_1_stats_1;
#define	VXGE_HAL_RF_RMAC_STATS_RTL_TOP_1_STATS_1_XGMAC_NMB_IO_REP_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_STATS_RTL_TOP_1_STATS_1_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08ee0 */	u64	rf_rmac_stats_rtl_top_0_stats_2;
#define	VXGE_HAL_RF_RMAC_STATS_RTL_TOP_0_STATS_2_XGMAC_NMB_IO_REP_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_STATS_RTL_TOP_0_STATS_2_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08ee8 */	u64	rf_rmac_stats_rtl_top_1_stats_2;
#define	VXGE_HAL_RF_RMAC_STATS_RTL_TOP_1_STATS_2_XGMAC_NMB_IO_REP_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_STATS_RTL_TOP_1_STATS_2_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08ef0 */	u64	rf_rmac_stats_rtl_top_0_stats_3;
#define	VXGE_HAL_RF_RMAC_STATS_RTL_TOP_0_STATS_3_XGMAC_NMB_IO_REP_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_STATS_RTL_TOP_0_STATS_3_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08ef8 */	u64	rf_rmac_stats_rtl_top_1_stats_3;
#define	VXGE_HAL_RF_RMAC_STATS_RTL_TOP_1_STATS_3_XGMAC_NMB_IO_REP_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_STATS_RTL_TOP_1_STATS_3_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08f00 */	u64	rf_rmac_stats_rtl_top_0_stats_4;
#define	VXGE_HAL_RF_RMAC_STATS_RTL_TOP_0_STATS_4_XGMAC_NMB_IO_REP_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_STATS_RTL_TOP_0_STATS_4_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
/* 0x08f08 */	u64	rf_rmac_stats_rtl_top_1_stats_4;
#define	VXGE_HAL_RF_RMAC_STATS_RTL_TOP_1_STATS_4_XGMAC_NMB_IO_REP_STATUS(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_RF_RMAC_STATS_RTL_TOP_1_STATS_4_XGMAC_NMB_IO_ALL_FUSE(val)\
							    vBIT(val, 2, 7)
	u8	unused09000[0x09000 - 0x08f10];

/* 0x09000 */	u64	g3ifcmd_fb_int_status;
#define	VXGE_HAL_G3IFCMD_FB_INT_STATUS_ERR_G3IF_INT	    mBIT(0)
/* 0x09008 */	u64	g3ifcmd_fb_int_mask;
/* 0x09010 */	u64	g3ifcmd_fb_err_reg;
#define	VXGE_HAL_G3IFCMD_FB_ERR_REG_G3IF_CK_DLL_LOCK	    mBIT(6)
#define	VXGE_HAL_G3IFCMD_FB_ERR_REG_G3IF_SM_ERR		    mBIT(7)
#define	VXGE_HAL_G3IFCMD_FB_ERR_REG_G3IF_RWDQS_DLL_LOCK(val) vBIT(val, 24, 8)
#define	VXGE_HAL_G3IFCMD_FB_ERR_REG_G3IF_IOCAL_FAULT	    mBIT(55)
/* 0x09018 */	u64	g3ifcmd_fb_err_mask;
/* 0x09020 */	u64	g3ifcmd_fb_err_alarm;
/* 0x09028 */	u64	g3ifcmd_fb_dll_ck0;
#define	VXGE_HAL_G3IFCMD_FB_DLL_CK0_DLL_0_SA_CAL(val)	    vBIT(val, 0, 8)
#define	VXGE_HAL_G3IFCMD_FB_DLL_CK0_DLL_0_SB_CAL(val)	    vBIT(val, 8, 8)
#define	VXGE_HAL_G3IFCMD_FB_DLL_CK0_ROLL		    mBIT(23)
#define	VXGE_HAL_G3IFCMD_FB_DLL_CK0_CMD_ADD_DLL_0_S(val)    vBIT(val, 25, 7)
#define	VXGE_HAL_G3IFCMD_FB_DLL_CK0_DLL_ENABLE		    mBIT(39)
#define	VXGE_HAL_G3IFCMD_FB_DLL_CK0_DLL_UPD(val)	    vBIT(val, 44, 4)
/* 0x09030 */	u64	g3ifcmd_fb_io_ctrl;
#define	VXGE_HAL_G3IFCMD_FB_IO_CTRL_DRIVE		    mBIT(7)
#define	VXGE_HAL_G3IFCMD_FB_IO_CTRL_TERM(val)		    vBIT(val, 13, 3)
/* 0x09038 */	u64	g3ifcmd_fb_iocal;
#define	VXGE_HAL_G3IFCMD_FB_IOCAL_RST_CYCLES(val)	    vBIT(val, 0, 16)
#define	VXGE_HAL_G3IFCMD_FB_IOCAL_RST_VALUE(val)	    vBIT(val, 17, 7)
#define	VXGE_HAL_G3IFCMD_FB_IOCAL_CORR_VALUE(val)	    vBIT(val, 24, 8)
#define	VXGE_HAL_G3IFCMD_FB_IOCAL_IOCAL_CTRL_CAL_VALUE0(val) vBIT(val, 33, 7)
#define	VXGE_HAL_G3IFCMD_FB_IOCAL_IOCAL_CTRL_CAL_VALUE1(val) vBIT(val, 41, 7)
#define	VXGE_HAL_G3IFCMD_FB_IOCAL_IOCAL_CTRL_CAL_VALUE2(val) vBIT(val, 49, 7)
#define	VXGE_HAL_G3IFCMD_FB_IOCAL_IOCAL_CTRL_CAL_VALUE3(val) vBIT(val, 57, 7)
/* 0x09040 */	u64	g3ifcmd_fb_master_dll_ck;
#define	VXGE_HAL_G3IFCMD_FB_MASTER_DLL_CK_DDR_GR_RAW(val)   vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFCMD_FB_MASTER_DLL_CK_SAMPLE(val)	    vBIT(val, 8, 8)
/* 0x09048 */	u64	g3ifcmd_fb_dll_training;
#define	VXGE_HAL_G3IFCMD_FB_DLL_TRAINING_TRA_START	    mBIT(6)
#define	VXGE_HAL_G3IFCMD_FB_DLL_TRAINING_TRA_DISABLE	    mBIT(7)
#define	VXGE_HAL_G3IFCMD_FB_DLL_TRAINING_START_CODE(val)    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFCMD_FB_DLL_TRAINING_END_CODE(val)	    vBIT(val, 17, 7)
	u8	unused09110[0x09110 - 0x09050];

/* 0x09110 */	u64	g3ifgr01_fb_group0_dll_rdqs;
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_RDQS_SA_CAL(val)    vBIT(val, 0, 8)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_RDQS_SB_CAL(val)    vBIT(val, 8, 8)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_RDQS_ATRA_SA_CAL(val) vBIT(val, 32, 8)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_RDQS_ATRA_SB_CAL(val) vBIT(val, 40, 8)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_RDQS_DDR_DLL_S(val) vBIT(val, 57, 7)
/* 0x09118 */	u64	g3ifgr01_fb_group0_dll_rdqs1;
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_RDQS1_ROLL	    mBIT(7)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_RDQS1_DLL_ENABLE    mBIT(14)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_RDQS1_DLL_ENABLE_ATRA mBIT(15)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_RDQS1_DLL_UPD(val)  vBIT(val, 21, 3)
/* 0x09120 */	u64	g3ifgr01_fb_group0_dll_wdqs;
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_WDQS_SA_CAL(val)    vBIT(val, 0, 8)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_WDQS_SB_CAL(val)    vBIT(val, 8, 8)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_WDQS_DDR_DLL_S(val) vBIT(val, 57, 7)
/* 0x09128 */	u64	g3ifgr01_fb_group0_dll_wdqs1;
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_WDQS1_ROLL	    mBIT(7)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_WDQS1_DLL_ENABLE    mBIT(15)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_WDQS1_DLL_UPD(val)  vBIT(val, 21, 3)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_WDQS1_SEL_MASTER_WDQS_CKN mBIT(31)
/* 0x09130 */	u64	g3ifgr01_fb_group0_dll_training1;
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_TRAINING1_DDR_TRA_STATUS(val)\
							    vBIT(val, 4, 4)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_TRAINING1_DDR_TRA_MIN(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_TRAINING1_DDR_TRA_MAX(val)\
							    vBIT(val, 17, 7)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_TRAINING1_DDR_ATRA_STATUS(val)\
							    vBIT(val, 36, 4)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_TRAINING1_DDR_ATRA_MIN(val)\
							    vBIT(val, 41, 7)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_TRAINING1_DDR_ATRA_MAX(val)\
							    vBIT(val, 49, 7)
/* 0x09138 */	u64	g3ifgr01_fb_group0_dll_training2;
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_TRAINING2_DDR_ATRA_PASS_CNT(val)\
							    vBIT(val, 0, 32)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_TRAINING2_DDR_ATRA_FAIL_CNT(val)\
							    vBIT(val, 32, 16)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_TRAINING2_DDR_ATRA_TIMER_FAIL_CNT(val)\
							    vBIT(val, 48, 16)
/* 0x09140 */	u64	g3ifgr01_fb_group0_dll_training3;
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_TRAINING3_DLL_TRA_DATA00(val)\
							    vBIT(val, 0, 16)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_TRAINING3_DLL_TRA_DATA01(val)\
							    vBIT(val, 16, 16)
/* 0x09148 */	u64	g3ifgr01_fb_group0_dll_act_training5;
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_ACT_TRAINING5_START_CODE(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_ACT_TRAINING5_END_CODE(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_ACT_TRAINING5_DISABLE mBIT(23)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_ACT_TRAINING5_TCNT(val)\
							    vBIT(val, 28, 4)
/* 0x09150 */	u64	g3ifgr01_fb_group0_dll_training6;
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_TRAINING6_DLL_TRA_EN_HALF_EYE_VALID\
							    mBIT(7)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_TRAINING6_DLL_ATRA_EN_HALF_EYE_VALID\
							    mBIT(15)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_TRAINING6_DLL_TRA_EN_MASTER_CORR\
							    mBIT(23)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_TRAINING6_DLL_ATRA_EN_MASTER_CORR\
							    mBIT(31)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_TRAINING6_DLL_SEL_TRA_ONLY mBIT(39)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_TRAINING6_DLL_EN_MOVING_AVR mBIT(47)
/* 0x09158 */	u64	g3ifgr01_fb_group0_dll_atra_offset;
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_ATRA_OFFSET_EQUATION(val)\
							    vBIT(val, 6, 2)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_ATRA_OFFSET_DDR_VALUE(val)\
							    vBIT(val, 8, 8)
/* 0x09160 */	u64	g3ifgr01_fb_group0_dll_tra_hold;
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_TRA_HOLD_DDR_MASTER_MIN(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_TRA_HOLD_DDR_MASTER_MAX(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_TRA_HOLD_DDR_TIME(val)\
							    vBIT(val, 16, 24)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_TRA_HOLD_DDR_UPDATES(val)\
							    vBIT(val, 40, 24)
/* 0x09168 */	u64	g3ifgr01_fb_group0_dll_atra_hold;
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_ATRA_HOLD_DDR_MASTER_MIN(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_ATRA_HOLD_DDR_MASTER_MAX(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_ATRA_HOLD_DDR_TIME(val)\
							    vBIT(val, 16, 24)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_ATRA_HOLD_DDR_UPDATES(val)\
							    vBIT(val, 40, 24)
/* 0x09170 */	u64	g3ifgr01_fb_group0_dll_master_codes;
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_MASTER_CODES_DDR_RDQS_TRA_HOLD(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_MASTER_CODES_DDR_RDQS_ATRA_HOLD(val)\
							    vBIT(val, 25, 7)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_MASTER_CODES_DDR_WDQS_RAW(val)\
							    vBIT(val, 41, 7)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_MASTER_CODES_DDR_RDQS_RAW(val)\
							    vBIT(val, 57, 7)
/* 0x09178 */	u64	g3ifgr01_fb_group0_dll_atra_timer;
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_ATRA_TIMER_VALUE(val)\
							    vBIT(val, 0, 16)
#define	VXGE_HAL_G3IFGR01_FB_GROUP0_DLL_ATRA_TIMER_ENABLED  mBIT(23)
/* 0x09180 */	u64	g3ifgr01_fb_group1_dll_rdqs;
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_RDQS_SA_CAL(val)\
							    vBIT(val, 0, 8)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_RDQS_SB_CAL(val)\
							    vBIT(val, 8, 8)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_RDQS_ATRA_SA_CAL(val)\
							    vBIT(val, 32, 8)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_RDQS_ATRA_SB_CAL(val)\
							    vBIT(val, 40, 8)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_RDQS_DDR_DLL_S(val)\
							    vBIT(val, 57, 7)
/* 0x09188 */	u64	g3ifgr01_fb_group1_dll_rdqs1;
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_RDQS1_ROLL	    mBIT(7)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_RDQS1_DLL_ENABLE    mBIT(14)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_RDQS1_DLL_ENABLE_ATRA mBIT(15)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_RDQS1_DLL_UPD(val)  vBIT(val, 21, 3)
/* 0x09190 */	u64	g3ifgr01_fb_group1_dll_wdqs;
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_WDQS_SA_CAL(val)    vBIT(val, 0, 8)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_WDQS_SB_CAL(val)    vBIT(val, 8, 8)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_WDQS_DDR_DLL_S(val) vBIT(val, 57, 7)
/* 0x09198 */	u64	g3ifgr01_fb_group1_dll_wdqs1;
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_WDQS1_ROLL	    mBIT(7)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_WDQS1_DLL_ENABLE    mBIT(15)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_WDQS1_DLL_UPD(val)  vBIT(val, 21, 3)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_WDQS1_SEL_MASTER_WDQS_CKN mBIT(31)
/* 0x091a0 */	u64	g3ifgr01_fb_group1_dll_training1;
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_TRAINING1_DDR_TRA_STATUS(val)\
							    vBIT(val, 4, 4)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_TRAINING1_DDR_TRA_MIN(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_TRAINING1_DDR_TRA_MAX(val)\
							    vBIT(val, 17, 7)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_TRAINING1_DDR_ATRA_STATUS(val)\
							    vBIT(val, 36, 4)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_TRAINING1_DDR_ATRA_MIN(val)\
							    vBIT(val, 41, 7)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_TRAINING1_DDR_ATRA_MAX(val)\
							    vBIT(val, 49, 7)
/* 0x091a8 */	u64	g3ifgr01_fb_group1_dll_training2;
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_TRAINING2_DDR_ATRA_PASS_CNT(val)\
							    vBIT(val, 0, 32)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_TRAINING2_DDR_ATRA_FAIL_CNT(val)\
							    vBIT(val, 32, 16)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_TRAINING2_DDR_ATRA_TIMER_FAIL_CNT(val)\
							    vBIT(val, 48, 16)
/* 0x091b0 */	u64	g3ifgr01_fb_group1_dll_training3;
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_TRAINING3_DLL_TRA_DATA00(val)\
							    vBIT(val, 0, 16)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_TRAINING3_DLL_TRA_DATA01(val)\
							    vBIT(val, 16, 16)
/* 0x091b8 */	u64	g3ifgr01_fb_group1_dll_act_training5;
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_ACT_TRAINING5_START_CODE(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_ACT_TRAINING5_END_CODE(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_ACT_TRAINING5_DISABLE	mBIT(23)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_ACT_TRAINING5_TCNT(val)	vBIT(val, 28, 4)
/* 0x091c0 */	u64	g3ifgr01_fb_group1_dll_training6;
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_TRAINING6_DLL_TRA_EN_HALF_EYE_VALID\
								mBIT(7)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_TRAINING6_DLL_ATRA_EN_HALF_EYE_VALID\
								mBIT(15)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_TRAINING6_DLL_TRA_EN_MASTER_CORRECTION\
								mBIT(23)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_TRAINING6_DLL_ATRA_EN_MASTER_CORRECTION\
								mBIT(31)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_TRAINING6_DLL_SEL_TRA_ONLY mBIT(39)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_TRAINING6_DLL_EN_MOVING_AVR mBIT(47)
/* 0x091c8 */	u64	g3ifgr01_fb_group1_dll_atra_offset;
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_ATRA_OFFSET_EQUATION(val)\
							    vBIT(val, 6, 2)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_ATRA_OFFSET_DDR_VALUE(val)\
							    vBIT(val, 8, 8)
/* 0x091d0 */	u64	g3ifgr01_fb_group1_dll_tra_hold;
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_TRA_HOLD_DDR_MASTER_MIN(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_TRA_HOLD_DDR_MASTER_MAX(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_TRA_HOLD_DDR_TIME(val)\
							    vBIT(val, 16, 24)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_TRA_HOLD_DDR_UPDATES(val)\
							    vBIT(val, 40, 24)
/* 0x091d8 */	u64	g3ifgr01_fb_group1_dll_atra_hold;
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_ATRA_HOLD_DDR_MASTER_MIN(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_ATRA_HOLD_DDR_MASTER_MAX(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_ATRA_HOLD_DDR_TIME(val)\
							    vBIT(val, 16, 24)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_ATRA_HOLD_DDR_UPDATES(val)\
							    vBIT(val, 40, 24)
/* 0x091e0 */	u64	g3ifgr01_fb_group1_dll_master_codes;
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_MASTER_CODES_DDR_RDQS_TRA_HOLD(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_MASTER_CODES_DDR_RDQS_ATRA_HOLD(val)\
							    vBIT(val, 25, 7)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_MASTER_CODES_DDR_WDQS_RAW(val)\
							    vBIT(val, 41, 7)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_MASTER_CODES_DDR_RDQS_RAW(val)\
							    vBIT(val, 57, 7)
/* 0x091e8 */	u64	g3ifgr01_fb_group1_dll_atra_timer;
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_ATRA_TIMER_VALUE(val) vBIT(val, 0, 16)
#define	VXGE_HAL_G3IFGR01_FB_GROUP1_DLL_ATRA_TIMER_ENABLED  mBIT(23)
	u8	unused09210[0x09210 - 0x091f0];

/* 0x09210 */	u64	g3ifgr23_fb_group2_dll_rdqs;
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_RDQS_SA_CAL(val)    vBIT(val, 0, 8)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_RDQS_SB_CAL(val)    vBIT(val, 8, 8)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_RDQS_ATRA_SA_CAL(val) vBIT(val, 32, 8)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_RDQS_ATRA_SB_CAL(val) vBIT(val, 40, 8)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_RDQS_DDR_DLL_S(val) vBIT(val, 57, 7)
/* 0x09218 */	u64	g3ifgr23_fb_group2_dll_rdqs1;
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_RDQS1_ROLL	    mBIT(7)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_RDQS1_DLL_ENABLE    mBIT(14)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_RDQS1_DLL_ENABLE_ATRA mBIT(15)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_RDQS1_DLL_UPD(val)  vBIT(val, 21, 3)
/* 0x09220 */	u64	g3ifgr23_fb_group2_dll_wdqs;
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_WDQS_SA_CAL(val)    vBIT(val, 0, 8)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_WDQS_SB_CAL(val)    vBIT(val, 8, 8)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_WDQS_DDR_DLL_S(val) vBIT(val, 57, 7)
/* 0x09228 */	u64	g3ifgr23_fb_group2_dll_wdqs1;
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_WDQS1_ROLL	    mBIT(7)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_WDQS1_DLL_ENABLE    mBIT(15)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_WDQS1_DLL_UPD(val)  vBIT(val, 21, 3)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_WDQS1_SEL_MASTER_WDQS_CKN mBIT(31)
/* 0x09230 */	u64	g3ifgr23_fb_group2_dll_training1;
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_TRAINING1_DDR_TRA_STATUS(val)\
							    vBIT(val, 4, 4)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_TRAINING1_DDR_TRA_MIN(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_TRAINING1_DDR_TRA_MAX(val)\
							    vBIT(val, 17, 7)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_TRAINING1_DDR_ATRA_STATUS(val)\
							    vBIT(val, 36, 4)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_TRAINING1_DDR_ATRA_MIN(val)\
							    vBIT(val, 41, 7)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_TRAINING1_DDR_ATRA_MAX(val)\
							    vBIT(val, 49, 7)
/* 0x09238 */	u64	g3ifgr23_fb_group2_dll_training2;
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_TRAINING2_DDR_ATRA_PASS_CNT(val)\
							    vBIT(val, 0, 32)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_TRAINING2_DDR_ATRA_FAIL_CNT(val)\
							    vBIT(val, 32, 16)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_TRAINING2_DDR_ATRA_TIMER_FAIL_CNT(val)\
							    vBIT(val, 48, 16)
/* 0x09240 */	u64	g3ifgr23_fb_group2_dll_training3;
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_TRAINING3_DLL_TRA_DATA00(val)\
							    vBIT(val, 0, 16)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_TRAINING3_DLL_TRA_DATA01(val)\
							    vBIT(val, 16, 16)
/* 0x09248 */	u64	g3ifgr23_fb_group2_dll_act_training5;
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_ACT_TRAINING5_START_CODE(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_ACT_TRAINING5_END_CODE(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_ACT_TRAINING5_DISABLE mBIT(23)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_ACT_TRAINING5_TCNT(val)\
							    vBIT(val, 28, 4)
/* 0x09250 */	u64	g3ifgr23_fb_group2_dll_training6;
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_TRAINING6_DLL_TRA_EN_HALF_EYE_VALID\
							    mBIT(7)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_TRAINING6_DLL_ATRA_EN_HALF_EYE_VALID\
							    mBIT(15)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_TRAINING6_DLL_TRA_EN_MASTER_CORRECTION\
							    mBIT(23)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_TRAINING6_DLL_ATRA_EN_MASTER_CORRECTION\
							    mBIT(31)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_TRAINING6_DLL_SEL_TRA_ONLY mBIT(39)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_TRAINING6_DLL_EN_MOVING_AVR mBIT(47)
/* 0x09258 */	u64	g3ifgr23_fb_group2_dll_atra_offset;
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_ATRA_OFFSET_EQUATION(val)\
							    vBIT(val, 6, 2)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_ATRA_OFFSET_DDR_VALUE(val)\
							    vBIT(val, 8, 8)
/* 0x09260 */	u64	g3ifgr23_fb_group2_dll_tra_hold;
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_TRA_HOLD_DDR_MASTER_MIN(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_TRA_HOLD_DDR_MASTER_MAX(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_TRA_HOLD_DDR_TIME(val)\
							    vBIT(val, 16, 24)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_TRA_HOLD_DDR_UPDATES(val)\
							    vBIT(val, 40, 24)
/* 0x09268 */	u64	g3ifgr23_fb_group2_dll_atra_hold;
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_ATRA_HOLD_DDR_MASTER_MIN(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_ATRA_HOLD_DDR_MASTER_MAX(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_ATRA_HOLD_DDR_TIME(val)\
							    vBIT(val, 16, 24)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_ATRA_HOLD_DDR_UPDATES(val)\
							    vBIT(val, 40, 24)
/* 0x09270 */	u64	g3ifgr23_fb_group2_dll_master_codes;
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_MASTER_CODES_DDR_RDQS_TRA_HOLD(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_MASTER_CODES_DDR_RDQS_ATRA_HOLD(val)\
							    vBIT(val, 25, 7)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_MASTER_CODES_DDR_WDQS_RAW(val)\
							    vBIT(val, 41, 7)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_MASTER_CODES_DDR_RDQS_RAW(val)\
							    vBIT(val, 57, 7)
/* 0x09278 */	u64	g3ifgr23_fb_group2_dll_atra_timer;
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_ATRA_TIMER_VALUE(val) vBIT(val, 0, 16)
#define	VXGE_HAL_G3IFGR23_FB_GROUP2_DLL_ATRA_TIMER_ENABLED  mBIT(23)
/* 0x09280 */	u64	g3ifgr23_fb_group3_dll_rdqs;
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_RDQS_SA_CAL(val)    vBIT(val, 0, 8)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_RDQS_SB_CAL(val)    vBIT(val, 8, 8)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_RDQS_ATRA_SA_CAL(val) vBIT(val, 32, 8)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_RDQS_ATRA_SB_CAL(val) vBIT(val, 40, 8)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_RDQS_DDR_DLL_S(val) vBIT(val, 57, 7)
/* 0x09288 */	u64	g3ifgr23_fb_group3_dll_rdqs1;
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_RDQS1_ROLL	    mBIT(7)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_RDQS1_DLL_ENABLE    mBIT(14)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_RDQS1_DLL_ENABLE_ATRA mBIT(15)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_RDQS1_DLL_UPD(val)  vBIT(val, 21, 3)
/* 0x09290 */	u64	g3ifgr23_fb_group3_dll_wdqs;
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_WDQS_SA_CAL(val)    vBIT(val, 0, 8)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_WDQS_SB_CAL(val)    vBIT(val, 8, 8)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_WDQS_DDR_DLL_S(val) vBIT(val, 57, 7)
/* 0x09298 */	u64	g3ifgr23_fb_group3_dll_wdqs1;
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_WDQS1_ROLL	    mBIT(7)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_WDQS1_DLL_ENABLE    mBIT(15)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_WDQS1_DLL_UPD(val)  vBIT(val, 21, 3)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_WDQS1_SEL_MASTER_WDQS_CKN mBIT(31)
/* 0x092a0 */	u64	g3ifgr23_fb_group3_dll_training1;
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_TRAINING1_DDR_TRA_STATUS(val)\
							    vBIT(val, 4, 4)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_TRAINING1_DDR_TRA_MIN(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_TRAINING1_DDR_TRA_MAX(val)\
							    vBIT(val, 17, 7)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_TRAINING1_DDR_ATRA_STATUS(val)\
							    vBIT(val, 36, 4)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_TRAINING1_DDR_ATRA_MIN(val)\
							    vBIT(val, 41, 7)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_TRAINING1_DDR_ATRA_MAX(val)\
							    vBIT(val, 49, 7)
/* 0x092a8 */	u64	g3ifgr23_fb_group3_dll_training2;
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_TRAINING2_DDR_ATRA_PASS_CNT(val)\
							    vBIT(val, 0, 32)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_TRAINING2_DDR_ATRA_FAIL_CNT(val)\
							    vBIT(val, 32, 16)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_TRAINING2_DDR_ATRA_TIMER_FAIL_CNT(val)\
							    vBIT(val, 48, 16)
/* 0x092b0 */	u64	g3ifgr23_fb_group3_dll_training3;
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_TRAINING3_DLL_TRA_DATA00(val)\
							    vBIT(val, 0, 16)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_TRAINING3_DLL_TRA_DATA01(val)\
							    vBIT(val, 16, 16)
/* 0x092b8 */	u64	g3ifgr23_fb_group3_dll_act_training5;
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_ACT_TRAINING5_START_CODE(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_ACT_TRAINING5_END_CODE(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_ACT_TRAINING5_DISABLE mBIT(23)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_ACT_TRAINING5_TCNT(val)	vBIT(val, 28, 4)
/* 0x092c0 */	u64	g3ifgr23_fb_group3_dll_training6;
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_TRAINING6_DLL_TRA_EN_HALF_EYE_VALID\
							    mBIT(7)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_TRAINING6_DLL_ATRA_EN_HALF_EYE_VALID\
							    mBIT(15)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_TRAINING6_DLL_TRA_EN_MASTER_CORRECTION\
							    mBIT(23)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_TRAINING6_DLL_ATRA_EN_MASTER_CORRECTION\
							    mBIT(31)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_TRAINING6_DLL_SEL_TRA_ONLY mBIT(39)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_TRAINING6_DLL_EN_MOVING_AVR mBIT(47)
/* 0x092c8 */	u64	g3ifgr23_fb_group3_dll_atra_offset;
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_ATRA_OFFSET_EQUATION(val)\
							    vBIT(val, 6, 2)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_ATRA_OFFSET_DDR_VALUE(val)\
							    vBIT(val, 8, 8)
/* 0x092d0 */	u64	g3ifgr23_fb_group3_dll_tra_hold;
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_TRA_HOLD_DDR_MASTER_MIN(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_TRA_HOLD_DDR_MASTER_MAX(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_TRA_HOLD_DDR_TIME(val)\
							    vBIT(val, 16, 24)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_TRA_HOLD_DDR_UPDATES(val)\
							    vBIT(val, 40, 24)
/* 0x092d8 */	u64	g3ifgr23_fb_group3_dll_atra_hold;
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_ATRA_HOLD_DDR_MASTER_MIN(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_ATRA_HOLD_DDR_MASTER_MAX(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_ATRA_HOLD_DDR_TIME(val)\
							    vBIT(val, 16, 24)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_ATRA_HOLD_DDR_UPDATES(val)\
							    vBIT(val, 40, 24)
/* 0x092e0 */	u64	g3ifgr23_fb_group3_dll_master_codes;
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_MASTER_CODES_DDR_RDQS_TRA_HOLD(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_MASTER_CODES_DDR_RDQS_ATRA_HOLD(val)\
							    vBIT(val, 25, 7)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_MASTER_CODES_DDR_WDQS_RAW(val)\
							    vBIT(val, 41, 7)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_MASTER_CODES_DDR_RDQS_RAW(val)\
							    vBIT(val, 57, 7)
/* 0x092e8 */	u64	g3ifgr23_fb_group3_dll_atra_timer;
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_ATRA_TIMER_VALUE(val) vBIT(val, 0, 16)
#define	VXGE_HAL_G3IFGR23_FB_GROUP3_DLL_ATRA_TIMER_ENABLED  mBIT(23)
	u8	unused09400[0x09400 - 0x092f0];

/* 0x09400 */	u64	g3ifcmd_cmu_int_status;
#define	VXGE_HAL_G3IFCMD_CMU_INT_STATUS_ERR_G3IF_INT	    mBIT(0)
/* 0x09408 */	u64	g3ifcmd_cmu_int_mask;
/* 0x09410 */	u64	g3ifcmd_cmu_err_reg;
#define	VXGE_HAL_G3IFCMD_CMU_ERR_REG_G3IF_CK_DLL_LOCK	    mBIT(6)
#define	VXGE_HAL_G3IFCMD_CMU_ERR_REG_G3IF_SM_ERR	    mBIT(7)
#define	VXGE_HAL_G3IFCMD_CMU_ERR_REG_G3IF_RWDQS_DLL_LOCK(val) vBIT(val, 24, 8)
#define	VXGE_HAL_G3IFCMD_CMU_ERR_REG_G3IF_IOCAL_FAULT	    mBIT(55)
/* 0x09418 */	u64	g3ifcmd_cmu_err_mask;
/* 0x09420 */	u64	g3ifcmd_cmu_err_alarm;
/* 0x09428 */	u64	g3ifcmd_cmu_dll_ck0;
#define	VXGE_HAL_G3IFCMD_CMU_DLL_CK0_DLL_0_SA_CAL(val)	    vBIT(val, 0, 8)
#define	VXGE_HAL_G3IFCMD_CMU_DLL_CK0_DLL_0_SB_CAL(val)	    vBIT(val, 8, 8)
#define	VXGE_HAL_G3IFCMD_CMU_DLL_CK0_ROLL		    mBIT(23)
#define	VXGE_HAL_G3IFCMD_CMU_DLL_CK0_CMD_ADD_DLL_0_S(val)   vBIT(val, 25, 7)
#define	VXGE_HAL_G3IFCMD_CMU_DLL_CK0_DLL_ENABLE		    mBIT(39)
#define	VXGE_HAL_G3IFCMD_CMU_DLL_CK0_DLL_UPD(val)	    vBIT(val, 44, 4)
/* 0x09430 */	u64	g3ifcmd_cmu_io_ctrl;
#define	VXGE_HAL_G3IFCMD_CMU_IO_CTRL_DRIVE		    mBIT(7)
#define	VXGE_HAL_G3IFCMD_CMU_IO_CTRL_TERM(val)		    vBIT(val, 13, 3)
/* 0x09438 */	u64	g3ifcmd_cmu_iocal;
#define	VXGE_HAL_G3IFCMD_CMU_IOCAL_RST_CYCLES(val)	    vBIT(val, 0, 16)
#define	VXGE_HAL_G3IFCMD_CMU_IOCAL_RST_VALUE(val)	    vBIT(val, 17, 7)
#define	VXGE_HAL_G3IFCMD_CMU_IOCAL_CORR_VALUE(val)	    vBIT(val, 24, 8)
#define	VXGE_HAL_G3IFCMD_CMU_IOCAL_IOCAL_CTRL_CAL_VALUE0(val) vBIT(val, 33, 7)
#define	VXGE_HAL_G3IFCMD_CMU_IOCAL_IOCAL_CTRL_CAL_VALUE1(val) vBIT(val, 41, 7)
#define	VXGE_HAL_G3IFCMD_CMU_IOCAL_IOCAL_CTRL_CAL_VALUE2(val) vBIT(val, 49, 7)
#define	VXGE_HAL_G3IFCMD_CMU_IOCAL_IOCAL_CTRL_CAL_VALUE3(val) vBIT(val, 57, 7)
/* 0x09440 */	u64	g3ifcmd_cmu_master_dll_ck;
#define	VXGE_HAL_G3IFCMD_CMU_MASTER_DLL_CK_DDR_GR_RAW(val)  vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFCMD_CMU_MASTER_DLL_CK_SAMPLE(val)	    vBIT(val, 8, 8)
/* 0x09448 */	u64	g3ifcmd_cmu_dll_training;
#define	VXGE_HAL_G3IFCMD_CMU_DLL_TRAINING_TRA_START	    mBIT(6)
#define	VXGE_HAL_G3IFCMD_CMU_DLL_TRAINING_TRA_DISABLE	    mBIT(7)
#define	VXGE_HAL_G3IFCMD_CMU_DLL_TRAINING_START_CODE(val)   vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFCMD_CMU_DLL_TRAINING_END_CODE(val)	    vBIT(val, 17, 7)
	u8	unused09510[0x09510 - 0x09450];

/* 0x09510 */	u64	g3ifgr01_cmu_group0_dll_rdqs;
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_RDQS_SA_CAL(val)   vBIT(val, 0, 8)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_RDQS_SB_CAL(val)   vBIT(val, 8, 8)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_RDQS_ATRA_SA_CAL(val) vBIT(val, 32, 8)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_RDQS_ATRA_SB_CAL(val) vBIT(val, 40, 8)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_RDQS_DDR_DLL_S(val) vBIT(val, 57, 7)
/* 0x09518 */	u64	g3ifgr01_cmu_group0_dll_rdqs1;
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_RDQS1_ROLL	    mBIT(7)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_RDQS1_DLL_ENABLE   mBIT(14)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_RDQS1_DLL_ENABLE_ATRA mBIT(15)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_RDQS1_DLL_UPD(val) vBIT(val, 21, 3)
/* 0x09520 */	u64	g3ifgr01_cmu_group0_dll_wdqs;
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_WDQS_SA_CAL(val)   vBIT(val, 0, 8)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_WDQS_SB_CAL(val)   vBIT(val, 8, 8)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_WDQS_DDR_DLL_S(val) vBIT(val, 57, 7)
/* 0x09528 */	u64	g3ifgr01_cmu_group0_dll_wdqs1;
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_WDQS1_ROLL	    mBIT(7)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_WDQS1_DLL_ENABLE   mBIT(15)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_WDQS1_DLL_UPD(val) vBIT(val, 21, 3)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_WDQS1_SEL_MASTER_WDQS_CKN\
							    mBIT(31)
/* 0x09530 */	u64	g3ifgr01_cmu_group0_dll_training1;
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_TRAINING1_DDR_TRA_STATUS(val)\
							    vBIT(val, 4, 4)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_TRAINING1_DDR_TRA_MIN(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_TRAINING1_DDR_TRA_MAX(val)\
							    vBIT(val, 17, 7)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_TRAINING1_DDR_ATRA_STATUS(val)\
							    vBIT(val, 36, 4)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_TRAINING1_DDR_ATRA_MIN(val)\
							    vBIT(val, 41, 7)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_TRAINING1_DDR_ATRA_MAX(val)\
							    vBIT(val, 49, 7)
/* 0x09538 */	u64	g3ifgr01_cmu_group0_dll_training2;
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_TRAINING2_DDR_ATRA_PASS_CNT(val)\
							    vBIT(val, 0, 32)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_TRAINING2_DDR_ATRA_FAIL_CNT(val)\
							    vBIT(val, 32, 16)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_TRAINING2_DDR_ATRA_TIMER_FAIL_CNT(val)\
							    vBIT(val, 48, 16)
/* 0x09540 */	u64	g3ifgr01_cmu_group0_dll_training3;
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_TRAINING3_DLL_TRA_DATA00(val)\
							    vBIT(val, 0, 16)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_TRAINING3_DLL_TRA_DATA01(val)\
							    vBIT(val, 16, 16)
/* 0x09548 */	u64	g3ifgr01_cmu_group0_dll_act_training5;
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_ACT_TRAINING5_START_CODE(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_ACT_TRAINING5_END_CODE(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_ACT_TRAINING5_DISABLE mBIT(23)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_ACT_TRAINING5_TCNT(val)\
							    vBIT(val, 28, 4)
/* 0x09550 */	u64	g3ifgr01_cmu_group0_dll_training6;
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_TRAINING6_DLL_TRA_EN_HALF_EYE_VALID\
							    mBIT(7)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_TRAINING6_DLL_ATRA_EN_HALF_EYE_VALID\
							    mBIT(15)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_TRAINING6_DLL_TRA_EN_MASTER_CORR\
							    mBIT(23)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_TRAINING6_DLL_ATRA_EN_MASTER_CORR\
							    mBIT(31)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_TRAINING6_DLL_SEL_TRA_ONLY mBIT(39)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_TRAINING6_DLL_EN_MOVING_AVR mBIT(47)
/* 0x09558 */	u64	g3ifgr01_cmu_group0_dll_atra_offset;
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_ATRA_OFFSET_EQUATION(val)\
							    vBIT(val, 6, 2)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_ATRA_OFFSET_DDR_VALUE(val)\
							    vBIT(val, 8, 8)
/* 0x09560 */	u64	g3ifgr01_cmu_group0_dll_tra_hold;
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_TRA_HOLD_DDR_MASTER_MIN(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_TRA_HOLD_DDR_MASTER_MAX(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_TRA_HOLD_DDR_TIME(val)\
							    vBIT(val, 16, 24)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_TRA_HOLD_DDR_UPDATES(val)\
							    vBIT(val, 40, 24)
/* 0x09568 */	u64	g3ifgr01_cmu_group0_dll_atra_hold;
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_ATRA_HOLD_DDR_MASTER_MIN(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_ATRA_HOLD_DDR_MASTER_MAX(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_ATRA_HOLD_DDR_TIME(val)\
							    vBIT(val, 16, 24)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_ATRA_HOLD_DDR_UPDATES(val)\
							    vBIT(val, 40, 24)
/* 0x09570 */	u64	g3ifgr01_cmu_group0_dll_master_codes;
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_MASTER_CODES_DDR_RDQS_TRA_HOLD(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_MASTER_CODES_DDR_RDQS_ATRA_HOLD(val)\
							    vBIT(val, 25, 7)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_MASTER_CODES_DDR_WDQS_RAW(val)\
							    vBIT(val, 41, 7)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_MASTER_CODES_DDR_RDQS_RAW(val)\
							    vBIT(val, 57, 7)
/* 0x09578 */	u64	g3ifgr01_cmu_group0_dll_atra_timer;
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_ATRA_TIMER_VALUE(val) vBIT(val, 0, 16)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP0_DLL_ATRA_TIMER_ENABLED mBIT(23)
/* 0x09580 */	u64	g3ifgr01_cmu_group1_dll_rdqs;
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_RDQS_SA_CAL(val)   vBIT(val, 0, 8)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_RDQS_SB_CAL(val)   vBIT(val, 8, 8)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_RDQS_ATRA_SA_CAL(val) vBIT(val, 32, 8)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_RDQS_ATRA_SB_CAL(val) vBIT(val, 40, 8)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_RDQS_DDR_DLL_S(val) vBIT(val, 57, 7)
/* 0x09588 */	u64	g3ifgr01_cmu_group1_dll_rdqs1;
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_RDQS1_ROLL	    mBIT(7)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_RDQS1_DLL_ENABLE   mBIT(14)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_RDQS1_DLL_ENABLE_ATRA mBIT(15)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_RDQS1_DLL_UPD(val) vBIT(val, 21, 3)
/* 0x09590 */	u64	g3ifgr01_cmu_group1_dll_wdqs;
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_WDQS_SA_CAL(val)   vBIT(val, 0, 8)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_WDQS_SB_CAL(val)   vBIT(val, 8, 8)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_WDQS_DDR_DLL_S(val) vBIT(val, 57, 7)
/* 0x09598 */	u64	g3ifgr01_cmu_group1_dll_wdqs1;
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_WDQS1_ROLL	    mBIT(7)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_WDQS1_DLL_ENABLE   mBIT(15)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_WDQS1_DLL_UPD(val) vBIT(val, 21, 3)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_WDQS1_SEL_MASTER_WDQS_CKN mBIT(31)
/* 0x095a0 */	u64	g3ifgr01_cmu_group1_dll_training1;
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_TRAINING1_DDR_TRA_STATUS(val)\
							    vBIT(val, 4, 4)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_TRAINING1_DDR_TRA_MIN(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_TRAINING1_DDR_TRA_MAX(val)\
							    vBIT(val, 17, 7)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_TRAINING1_DDR_ATRA_STATUS(val)\
							    vBIT(val, 36, 4)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_TRAINING1_DDR_ATRA_MIN(val)\
							    vBIT(val, 41, 7)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_TRAINING1_DDR_ATRA_MAX(val)\
							    vBIT(val, 49, 7)
/* 0x095a8 */	u64	g3ifgr01_cmu_group1_dll_training2;
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_TRAINING2_DDR_ATRA_PASS_CNT(val)\
							    vBIT(val, 0, 32)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_TRAINING2_DDR_ATRA_FAIL_CNT(val)\
							    vBIT(val, 32, 16)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_TRAINING2_DDR_ATRA_TIMER_FAIL_CNT(val)\
							    vBIT(val, 48, 16)
/* 0x095b0 */	u64	g3ifgr01_cmu_group1_dll_training3;
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_TRAINING3_DLL_TRA_DATA00(val)\
							    vBIT(val, 0, 16)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_TRAINING3_DLL_TRA_DATA01(val)\
							    vBIT(val, 16, 16)
/* 0x095b8 */	u64	g3ifgr01_cmu_group1_dll_act_training5;
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_ACT_TRAINING5_START_CODE(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_ACT_TRAINING5_END_CODE(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_ACT_TRAINING5_DISABLE mBIT(23)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_ACT_TRAINING5_TCNT(val)\
							    vBIT(val, 28, 4)
/* 0x095c0 */	u64	g3ifgr01_cmu_group1_dll_training6;
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_TRAINING6_DLL_TRA_EN_HALF_EYE_VALID\
							    mBIT(7)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_TRAINING6_DLL_ATRA_EN_HALF_EYE_VALID\
							    mBIT(15)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_TRAINING6_DLL_TRA_EN_MASTER_CORR\
							    mBIT(23)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_TRAINING6_DLL_ATRA_EN_MASTER_CORR\
							    mBIT(31)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_TRAINING6_DLL_SEL_TRA_ONLY mBIT(39)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_TRAINING6_DLL_EN_MOVING_AVR mBIT(47)
/* 0x095c8 */	u64	g3ifgr01_cmu_group1_dll_atra_offset;
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_ATRA_OFFSET_EQUATION(val)\
							    vBIT(val, 6, 2)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_ATRA_OFFSET_DDR_VALUE(val)\
							    vBIT(val, 8, 8)
/* 0x095d0 */	u64	g3ifgr01_cmu_group1_dll_tra_hold;
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_TRA_HOLD_DDR_MASTER_MIN(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_TRA_HOLD_DDR_MASTER_MAX(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_TRA_HOLD_DDR_TIME(val)\
							    vBIT(val, 16, 24)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_TRA_HOLD_DDR_UPDATES(val)\
							    vBIT(val, 40, 24)
/* 0x095d8 */	u64	g3ifgr01_cmu_group1_dll_atra_hold;
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_ATRA_HOLD_DDR_MASTER_MIN(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_ATRA_HOLD_DDR_MASTER_MAX(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_ATRA_HOLD_DDR_TIME(val)\
							    vBIT(val, 16, 24)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_ATRA_HOLD_DDR_UPDATES(val)\
							    vBIT(val, 40, 24)
/* 0x095e0 */	u64	g3ifgr01_cmu_group1_dll_master_codes;
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_MASTER_CODES_DDR_RDQS_TRA_HOLD(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_MASTER_CODES_DDR_RDQS_ATRA_HOLD(val)\
							    vBIT(val, 25, 7)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_MASTER_CODES_DDR_WDQS_RAW(val)\
							    vBIT(val, 41, 7)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_MASTER_CODES_DDR_RDQS_RAW(val)\
							    vBIT(val, 57, 7)
/* 0x095e8 */	u64	g3ifgr01_cmu_group1_dll_atra_timer;
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_ATRA_TIMER_VALUE(val) vBIT(val, 0, 16)
#define	VXGE_HAL_G3IFGR01_CMU_GROUP1_DLL_ATRA_TIMER_ENABLED mBIT(23)
	u8	unused09610[0x09610 - 0x095f0];

/* 0x09610 */	u64	g3ifgr23_cmu_group2_dll_rdqs;
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_RDQS_SA_CAL(val)   vBIT(val, 0, 8)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_RDQS_SB_CAL(val)   vBIT(val, 8, 8)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_RDQS_ATRA_SA_CAL(val) vBIT(val, 32, 8)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_RDQS_ATRA_SB_CAL(val) vBIT(val, 40, 8)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_RDQS_DDR_DLL_S(val) vBIT(val, 57, 7)
/* 0x09618 */	u64	g3ifgr23_cmu_group2_dll_rdqs1;
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_RDQS1_ROLL	    mBIT(7)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_RDQS1_DLL_ENABLE   mBIT(14)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_RDQS1_DLL_ENABLE_ATRA mBIT(15)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_RDQS1_DLL_UPD(val) vBIT(val, 21, 3)
/* 0x09620 */	u64	g3ifgr23_cmu_group2_dll_wdqs;
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_WDQS_SA_CAL(val)   vBIT(val, 0, 8)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_WDQS_SB_CAL(val)   vBIT(val, 8, 8)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_WDQS_DDR_DLL_S(val) vBIT(val, 57, 7)
/* 0x09628 */	u64	g3ifgr23_cmu_group2_dll_wdqs1;
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_WDQS1_ROLL	    mBIT(7)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_WDQS1_DLL_ENABLE\
							    mBIT(15)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_WDQS1_DLL_UPD(val)\
							    vBIT(val, 21, 3)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_WDQS1_SEL_MASTER_WDQS_CKN\
							    mBIT(31)
/* 0x09630 */	u64	g3ifgr23_cmu_group2_dll_training1;
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_TRAINING1_DDR_TRA_STATUS(val)\
							    vBIT(val, 4, 4)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_TRAINING1_DDR_TRA_MIN(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_TRAINING1_DDR_TRA_MAX(val)\
							    vBIT(val, 17, 7)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_TRAINING1_DDR_ATRA_STATUS(val)\
							    vBIT(val, 36, 4)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_TRAINING1_DDR_ATRA_MIN(val)\
							    vBIT(val, 41, 7)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_TRAINING1_DDR_ATRA_MAX(val)\
							    vBIT(val, 49, 7)
/* 0x09638 */	u64	g3ifgr23_cmu_group2_dll_training2;
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_TRAINING2_DDR_ATRA_PASS_CNT(val)\
							    vBIT(val, 0, 32)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_TRAINING2_DDR_ATRA_FAIL_CNT(val)\
							    vBIT(val, 32, 16)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_TRAINING2_DDR_ATRA_TIMER_FAIL_CNT(val)\
							    vBIT(val, 48, 16)
/* 0x09640 */	u64	g3ifgr23_cmu_group2_dll_training3;
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_TRAINING3_DLL_TRA_DATA00(val)\
							    vBIT(val, 0, 16)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_TRAINING3_DLL_TRA_DATA01(val)\
							    vBIT(val, 16, 16)
/* 0x09648 */	u64	g3ifgr23_cmu_group2_dll_act_training5;
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_ACT_TRAINING5_START_CODE(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_ACT_TRAINING5_END_CODE(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_ACT_TRAINING5_DISABLE	mBIT(23)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_ACT_TRAINING5_TCNT(val)\
							    vBIT(val, 28, 4)
/* 0x09650 */	u64	g3ifgr23_cmu_group2_dll_training6;
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_TRAINING6_DLL_TRA_EN_HALF_EYE_VALID\
							    mBIT(7)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_TRAINING6_DLL_ATRA_EN_HALF_EYE_VALID\
							    mBIT(15)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_TRAINING6_DLL_TRA_EN_MASTER_CORR\
							    mBIT(23)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_TRAINING6_DLL_ATRA_EN_MASTER_CORR\
							    mBIT(31)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_TRAINING6_DLL_SEL_TRA_ONLY mBIT(39)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_TRAINING6_DLL_EN_MOVING_AVR mBIT(47)
/* 0x09658 */	u64	g3ifgr23_cmu_group2_dll_atra_offset;
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_ATRA_OFFSET_EQUATION(val)\
							    vBIT(val, 6, 2)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_ATRA_OFFSET_DDR_VALUE(val)\
							    vBIT(val, 8, 8)
/* 0x09660 */	u64	g3ifgr23_cmu_group2_dll_tra_hold;
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_TRA_HOLD_DDR_MASTER_MIN(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_TRA_HOLD_DDR_MASTER_MAX(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_TRA_HOLD_DDR_TIME(val)\
							    vBIT(val, 16, 24)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_TRA_HOLD_DDR_UPDATES(val)\
							    vBIT(val, 40, 24)
/* 0x09668 */	u64	g3ifgr23_cmu_group2_dll_atra_hold;
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_ATRA_HOLD_DDR_MASTER_MIN(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_ATRA_HOLD_DDR_MASTER_MAX(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_ATRA_HOLD_DDR_TIME(val)\
							    vBIT(val, 16, 24)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_ATRA_HOLD_DDR_UPDATES(val)\
							    vBIT(val, 40, 24)
/* 0x09670 */	u64	g3ifgr23_cmu_group2_dll_master_codes;
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_MASTER_CODES_DDR_RDQS_TRA_HOLD(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_MASTER_CODES_DDR_RDQS_ATRA_HOLD(val)\
							    vBIT(val, 25, 7)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_MASTER_CODES_DDR_WDQS_RAW(val)\
							    vBIT(val, 41, 7)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_MASTER_CODES_DDR_RDQS_RAW(val)\
							    vBIT(val, 57, 7)
/* 0x09678 */	u64	g3ifgr23_cmu_group2_dll_atra_timer;
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_ATRA_TIMER_VALUE(val) vBIT(val, 0, 16)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP2_DLL_ATRA_TIMER_ENABLED mBIT(23)
/* 0x09680 */	u64	g3ifgr23_cmu_group3_dll_rdqs;
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_RDQS_SA_CAL(val)   vBIT(val, 0, 8)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_RDQS_SB_CAL(val)   vBIT(val, 8, 8)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_RDQS_ATRA_SA_CAL(val) vBIT(val, 32, 8)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_RDQS_ATRA_SB_CAL(val) vBIT(val, 40, 8)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_RDQS_DDR_DLL_S(val) vBIT(val, 57, 7)
/* 0x09688 */	u64	g3ifgr23_cmu_group3_dll_rdqs1;
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_RDQS1_ROLL	    mBIT(7)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_RDQS1_DLL_ENABLE   mBIT(14)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_RDQS1_DLL_ENABLE_ATRA mBIT(15)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_RDQS1_DLL_UPD(val) vBIT(val, 21, 3)
/* 0x09690 */	u64	g3ifgr23_cmu_group3_dll_wdqs;
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_WDQS_SA_CAL(val)   vBIT(val, 0, 8)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_WDQS_SB_CAL(val)   vBIT(val, 8, 8)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_WDQS_DDR_DLL_S(val) vBIT(val, 57, 7)
/* 0x09698 */	u64	g3ifgr23_cmu_group3_dll_wdqs1;
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_WDQS1_ROLL	    mBIT(7)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_WDQS1_DLL_ENABLE   mBIT(15)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_WDQS1_DLL_UPD(val) vBIT(val, 21, 3)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_WDQS1_SEL_MASTER_WDQS_CKN mBIT(31)
/* 0x096a0 */	u64	g3ifgr23_cmu_group3_dll_training1;
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_TRAINING1_DDR_TRA_STATUS(val)\
							    vBIT(val, 4, 4)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_TRAINING1_DDR_TRA_MIN(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_TRAINING1_DDR_TRA_MAX(val)\
							    vBIT(val, 17, 7)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_TRAINING1_DDR_ATRA_STATUS(val)\
							    vBIT(val, 36, 4)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_TRAINING1_DDR_ATRA_MIN(val)\
							    vBIT(val, 41, 7)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_TRAINING1_DDR_ATRA_MAX(val)\
							    vBIT(val, 49, 7)
/* 0x096a8 */	u64	g3ifgr23_cmu_group3_dll_training2;
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_TRAINING2_DDR_ATRA_PASS_CNT(val)\
							    vBIT(val, 0, 32)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_TRAINING2_DDR_ATRA_FAIL_CNT(val)\
							    vBIT(val, 32, 16)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_TRAINING2_DDR_ATRA_TIMER_FAIL_CNT(val)\
							    vBIT(val, 48, 16)
/* 0x096b0 */	u64	g3ifgr23_cmu_group3_dll_training3;
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_TRAINING3_DLL_TRA_DATA00(val)\
							    vBIT(val, 0, 16)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_TRAINING3_DLL_TRA_DATA01(val)\
							    vBIT(val, 16, 16)
/* 0x096b8 */	u64	g3ifgr23_cmu_group3_dll_act_training5;
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_ACT_TRAINING5_START_CODE(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_ACT_TRAINING5_END_CODE(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_ACT_TRAINING5_DISABLE\
							    mBIT(23)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_ACT_TRAINING5_TCNT(val)\
							    vBIT(val, 28, 4)
/* 0x096c0 */	u64	g3ifgr23_cmu_group3_dll_training6;
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_TRAINING6_DLL_TRA_EN_HALF_EYE_VALID\
							    mBIT(7)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_TRAINING6_DLL_ATRA_EN_HALF_EYE_VALID\
							    mBIT(15)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_TRAINING6_DLL_TRA_EN_MASTER_CORR\
							    mBIT(23)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_TRAINING6_DLL_ATRA_EN_MASTER_CORR\
							    mBIT(31)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_TRAINING6_DLL_SEL_TRA_ONLY mBIT(39)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_TRAINING6_DLL_EN_MOVING_AVR mBIT(47)
/* 0x096c8 */	u64	g3ifgr23_cmu_group3_dll_atra_offset;
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_ATRA_OFFSET_EQUATION(val)\
							    vBIT(val, 6, 2)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_ATRA_OFFSET_DDR_VALUE(val)\
							    vBIT(val, 8, 8)
/* 0x096d0 */	u64	g3ifgr23_cmu_group3_dll_tra_hold;
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_TRA_HOLD_DDR_MASTER_MIN(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_TRA_HOLD_DDR_MASTER_MAX(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_TRA_HOLD_DDR_TIME(val)\
							    vBIT(val, 16, 24)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_TRA_HOLD_DDR_UPDATES(val)\
							    vBIT(val, 40, 24)
/* 0x096d8 */	u64	g3ifgr23_cmu_group3_dll_atra_hold;
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_ATRA_HOLD_DDR_MASTER_MIN(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_ATRA_HOLD_DDR_MASTER_MAX(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_ATRA_HOLD_DDR_TIME(val)\
							    vBIT(val, 16, 24)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_ATRA_HOLD_DDR_UPDATES(val)\
							    vBIT(val, 40, 24)
/* 0x096e0 */	u64	g3ifgr23_cmu_group3_dll_master_codes;
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_MASTER_CODES_DDR_RDQS_TRA_HOLD(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_MASTER_CODES_DDR_RDQS_ATRA_HOLD(val)\
							    vBIT(val, 25, 7)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_MASTER_CODES_DDR_WDQS_RAW(val)\
							    vBIT(val, 41, 7)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_MASTER_CODES_DDR_RDQS_RAW(val)\
							    vBIT(val, 57, 7)
/* 0x096e8 */	u64	g3ifgr23_cmu_group3_dll_atra_timer;
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_ATRA_TIMER_VALUE(val) vBIT(val, 0, 16)
#define	VXGE_HAL_G3IFGR23_CMU_GROUP3_DLL_ATRA_TIMER_ENABLED mBIT(23)
	u8	unused09800[0x09800 - 0x096f0];

/* 0x09800 */	u64	g3ifcmd_cml_int_status;
#define	VXGE_HAL_G3IFCMD_CML_INT_STATUS_ERR_G3IF_INT	    mBIT(0)
/* 0x09808 */	u64	g3ifcmd_cml_int_mask;
/* 0x09810 */	u64	g3ifcmd_cml_err_reg;
#define	VXGE_HAL_G3IFCMD_CML_ERR_REG_G3IF_CK_DLL_LOCK	    mBIT(6)
#define	VXGE_HAL_G3IFCMD_CML_ERR_REG_G3IF_SM_ERR	    mBIT(7)
#define	VXGE_HAL_G3IFCMD_CML_ERR_REG_G3IF_RWDQS_DLL_LOCK(val)\
							    vBIT(val, 24, 8)
#define	VXGE_HAL_G3IFCMD_CML_ERR_REG_G3IF_IOCAL_FAULT	    mBIT(55)
/* 0x09818 */	u64	g3ifcmd_cml_err_mask;
/* 0x09820 */	u64	g3ifcmd_cml_err_alarm;
/* 0x09828 */	u64	g3ifcmd_cml_dll_ck0;
#define	VXGE_HAL_G3IFCMD_CML_DLL_CK0_DLL_0_SA_CAL(val)	    vBIT(val, 0, 8)
#define	VXGE_HAL_G3IFCMD_CML_DLL_CK0_DLL_0_SB_CAL(val)	    vBIT(val, 8, 8)
#define	VXGE_HAL_G3IFCMD_CML_DLL_CK0_ROLL		    mBIT(23)
#define	VXGE_HAL_G3IFCMD_CML_DLL_CK0_CMD_ADD_DLL_0_S(val)   vBIT(val, 25, 7)
#define	VXGE_HAL_G3IFCMD_CML_DLL_CK0_DLL_ENABLE	mBIT(39)
#define	VXGE_HAL_G3IFCMD_CML_DLL_CK0_DLL_UPD(val)	    vBIT(val, 44, 4)
/* 0x09830 */	u64	g3ifcmd_cml_io_ctrl;
#define	VXGE_HAL_G3IFCMD_CML_IO_CTRL_DRIVE		    mBIT(7)
#define	VXGE_HAL_G3IFCMD_CML_IO_CTRL_TERM(val)		    vBIT(val, 13, 3)
/* 0x09838 */	u64	g3ifcmd_cml_iocal;
#define	VXGE_HAL_G3IFCMD_CML_IOCAL_RST_CYCLES(val)	    vBIT(val, 0, 16)
#define	VXGE_HAL_G3IFCMD_CML_IOCAL_RST_VALUE(val)	    vBIT(val, 17, 7)
#define	VXGE_HAL_G3IFCMD_CML_IOCAL_CORR_VALUE(val)	    vBIT(val, 24, 8)
#define	VXGE_HAL_G3IFCMD_CML_IOCAL_IOCAL_CTRL_CAL_VALUE0(val)\
							    vBIT(val, 33, 7)
#define	VXGE_HAL_G3IFCMD_CML_IOCAL_IOCAL_CTRL_CAL_VALUE1(val)\
							    vBIT(val, 41, 7)
#define	VXGE_HAL_G3IFCMD_CML_IOCAL_IOCAL_CTRL_CAL_VALUE2(val)\
							    vBIT(val, 49, 7)
#define	VXGE_HAL_G3IFCMD_CML_IOCAL_IOCAL_CTRL_CAL_VALUE3(val)\
							    vBIT(val, 57, 7)
/* 0x09840 */	u64	g3ifcmd_cml_master_dll_ck;
#define	VXGE_HAL_G3IFCMD_CML_MASTER_DLL_CK_DDR_GR_RAW(val)  vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFCMD_CML_MASTER_DLL_CK_SAMPLE(val)	    vBIT(val, 8, 8)
/* 0x09848 */	u64	g3ifcmd_cml_dll_training;
#define	VXGE_HAL_G3IFCMD_CML_DLL_TRAINING_TRA_START	    mBIT(6)
#define	VXGE_HAL_G3IFCMD_CML_DLL_TRAINING_TRA_DISABLE	    mBIT(7)
#define	VXGE_HAL_G3IFCMD_CML_DLL_TRAINING_START_CODE(val)   vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFCMD_CML_DLL_TRAINING_END_CODE(val)	    vBIT(val, 17, 7)
	u8	unused09910[0x09910 - 0x09850];

/* 0x09910 */	u64	g3ifgr01_cml_group0_dll_rdqs;
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_RDQS_SA_CAL(val)   vBIT(val, 0, 8)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_RDQS_SB_CAL(val)   vBIT(val, 8, 8)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_RDQS_ATRA_SA_CAL(val)\
							    vBIT(val, 32, 8)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_RDQS_ATRA_SB_CAL(val)\
							    vBIT(val, 40, 8)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_RDQS_DDR_DLL_S(val) vBIT(val, 57, 7)
/* 0x09918 */	u64	g3ifgr01_cml_group0_dll_rdqs1;
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_RDQS1_ROLL	    mBIT(7)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_RDQS1_DLL_ENABLE   mBIT(14)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_RDQS1_DLL_ENABLE_ATRA mBIT(15)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_RDQS1_DLL_UPD(val) vBIT(val, 21, 3)
/* 0x09920 */	u64	g3ifgr01_cml_group0_dll_wdqs;
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_WDQS_SA_CAL(val)   vBIT(val, 0, 8)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_WDQS_SB_CAL(val)   vBIT(val, 8, 8)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_WDQS_DDR_DLL_S(val) vBIT(val, 57, 7)
/* 0x09928 */	u64	g3ifgr01_cml_group0_dll_wdqs1;
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_WDQS1_ROLL	    mBIT(7)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_WDQS1_DLL_ENABLE   mBIT(15)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_WDQS1_DLL_UPD(val) vBIT(val, 21, 3)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_WDQS1_SEL_MASTER_WDQS_CKN\
							    mBIT(31)
/* 0x09930 */	u64	g3ifgr01_cml_group0_dll_training1;
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_TRAINING1_DDR_TRA_STATUS(val)\
							    vBIT(val, 4, 4)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_TRAINING1_DDR_TRA_MIN(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_TRAINING1_DDR_TRA_MAX(val)\
							    vBIT(val, 17, 7)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_TRAINING1_DDR_ATRA_STATUS(val)\
							    vBIT(val, 36, 4)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_TRAINING1_DDR_ATRA_MIN(val)\
							    vBIT(val, 41, 7)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_TRAINING1_DDR_ATRA_MAX(val)\
							    vBIT(val, 49, 7)
/* 0x09938 */	u64	g3ifgr01_cml_group0_dll_training2;
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_TRAINING2_DDR_ATRA_PASS_CNT(val)\
							    vBIT(val, 0, 32)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_TRAINING2_DDR_ATRA_FAIL_CNT(val)\
							    vBIT(val, 32, 16)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_TRAINING2_DDR_ATRA_TIMER_FAIL_CNT(val)\
							    vBIT(val, 48, 16)
/* 0x09940 */	u64	g3ifgr01_cml_group0_dll_training3;
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_TRAINING3_DLL_TRA_DATA00(val)\
							    vBIT(val, 0, 16)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_TRAINING3_DLL_TRA_DATA01(val)\
							    vBIT(val, 16, 16)
/* 0x09948 */	u64	g3ifgr01_cml_group0_dll_act_training5;
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_ACT_TRAINING5_START_CODE(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_ACT_TRAINING5_END_CODE(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_ACT_TRAINING5_DISABLE mBIT(23)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_ACT_TRAINING5_TCNT(val)\
							    vBIT(val, 28, 4)
/* 0x09950 */	u64	g3ifgr01_cml_group0_dll_training6;
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_TRAINING6_DLL_TRA_EN_HALF_EYE_VALID\
							    mBIT(7)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_TRAINING6_DLL_ATRA_EN_HALF_EYE_VALID\
							    mBIT(15)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_TRAINING6_DLL_TRA_EN_MASTER_CORR\
							    mBIT(23)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_TRAINING6_DLL_ATRA_EN_MASTER_CORR\
							    mBIT(31)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_TRAINING6_DLL_SEL_TRA_ONLY mBIT(39)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_TRAINING6_DLL_EN_MOVING_AVR mBIT(47)
/* 0x09958 */	u64	g3ifgr01_cml_group0_dll_atra_offset;
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_ATRA_OFFSET_EQUATION(val)\
							    vBIT(val, 6, 2)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_ATRA_OFFSET_DDR_VALUE(val)\
							    vBIT(val, 8, 8)
/* 0x09960 */	u64	g3ifgr01_cml_group0_dll_tra_hold;
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_TRA_HOLD_DDR_MASTER_MIN(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_TRA_HOLD_DDR_MASTER_MAX(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_TRA_HOLD_DDR_TIME(val)\
							    vBIT(val, 16, 24)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_TRA_HOLD_DDR_UPDATES(val)\
							    vBIT(val, 40, 24)
/* 0x09968 */	u64	g3ifgr01_cml_group0_dll_atra_hold;
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_ATRA_HOLD_DDR_MASTER_MIN(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_ATRA_HOLD_DDR_MASTER_MAX(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_ATRA_HOLD_DDR_TIME(val)\
							    vBIT(val, 16, 24)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_ATRA_HOLD_DDR_UPDATES(val)\
							    vBIT(val, 40, 24)
/* 0x09970 */	u64	g3ifgr01_cml_group0_dll_master_codes;
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_MASTER_CODES_DDR_RDQS_TRA_HOLD(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_MASTER_CODES_DDR_RDQS_ATRA_HOLD(val)\
							    vBIT(val, 25, 7)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_MASTER_CODES_DDR_WDQS_RAW(val)\
							    vBIT(val, 41, 7)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_MASTER_CODES_DDR_RDQS_RAW(val)\
							    vBIT(val, 57, 7)
/* 0x09978 */	u64	g3ifgr01_cml_group0_dll_atra_timer;
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_ATRA_TIMER_VALUE(val) vBIT(val, 0, 16)
#define	VXGE_HAL_G3IFGR01_CML_GROUP0_DLL_ATRA_TIMER_ENABLED mBIT(23)
/* 0x09980 */	u64	g3ifgr01_cml_group1_dll_rdqs;
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_RDQS_SA_CAL(val)   vBIT(val, 0, 8)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_RDQS_SB_CAL(val)   vBIT(val, 8, 8)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_RDQS_ATRA_SA_CAL(val) vBIT(val, 32, 8)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_RDQS_ATRA_SB_CAL(val) vBIT(val, 40, 8)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_RDQS_DDR_DLL_S(val) vBIT(val, 57, 7)
/* 0x09988 */	u64	g3ifgr01_cml_group1_dll_rdqs1;
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_RDQS1_ROLL	    mBIT(7)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_RDQS1_DLL_ENABLE   mBIT(14)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_RDQS1_DLL_ENABLE_ATRA	mBIT(15)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_RDQS1_DLL_UPD(val) vBIT(val, 21, 3)
/* 0x09990 */	u64	g3ifgr01_cml_group1_dll_wdqs;
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_WDQS_SA_CAL(val)   vBIT(val, 0, 8)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_WDQS_SB_CAL(val)   vBIT(val, 8, 8)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_WDQS_DDR_DLL_S(val) vBIT(val, 57, 7)
/* 0x09998 */	u64	g3ifgr01_cml_group1_dll_wdqs1;
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_WDQS1_ROLL	    mBIT(7)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_WDQS1_DLL_ENABLE   mBIT(15)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_WDQS1_DLL_UPD(val) vBIT(val, 21, 3)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_WDQS1_SEL_MASTER_WDQS_CKN mBIT(31)
/* 0x099a0 */	u64	g3ifgr01_cml_group1_dll_training1;
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_TRAINING1_DDR_TRA_STATUS(val)\
							    vBIT(val, 4, 4)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_TRAINING1_DDR_TRA_MIN(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_TRAINING1_DDR_TRA_MAX(val)\
							    vBIT(val, 17, 7)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_TRAINING1_DDR_ATRA_STATUS(val)\
							    vBIT(val, 36, 4)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_TRAINING1_DDR_ATRA_MIN(val)\
							    vBIT(val, 41, 7)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_TRAINING1_DDR_ATRA_MAX(val)\
							    vBIT(val, 49, 7)
/* 0x099a8 */	u64	g3ifgr01_cml_group1_dll_training2;
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_TRAINING2_DDR_ATRA_PASS_CNT(val)\
							    vBIT(val, 0, 32)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_TRAINING2_DDR_ATRA_FAIL_CNT(val)\
							    vBIT(val, 32, 16)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_TRAINING2_DDR_ATRA_TIMER_FAIL_CNT(val)\
							    vBIT(val, 48, 16)
/* 0x099b0 */	u64	g3ifgr01_cml_group1_dll_training3;
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_TRAINING3_DLL_TRA_DATA00(val)\
							    vBIT(val, 0, 16)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_TRAINING3_DLL_TRA_DATA01(val)\
							    vBIT(val, 16, 16)
/* 0x099b8 */	u64	g3ifgr01_cml_group1_dll_act_training5;
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_ACT_TRAINING5_START_CODE(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_ACT_TRAINING5_END_CODE(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_ACT_TRAINING5_DISABLE	mBIT(23)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_ACT_TRAINING5_TCNT(val)\
							    vBIT(val, 28, 4)
/* 0x099c0 */	u64	g3ifgr01_cml_group1_dll_training6;
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_TRAINING6_DLL_TRA_EN_HALF_EYE_VALID\
							    mBIT(7)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_TRAINING6_DLL_ATRA_EN_HALF_EYE_VALID\
							    mBIT(15)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_TRAINING6_DLL_TRA_EN_MASTER_CORR\
							    mBIT(23)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_TRAINING6_DLL_ATRA_EN_MASTER_CORR\
							    mBIT(31)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_TRAINING6_DLL_SEL_TRA_ONLY mBIT(39)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_TRAINING6_DLL_EN_MOVING_AVR mBIT(47)
/* 0x099c8 */	u64	g3ifgr01_cml_group1_dll_atra_offset;
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_ATRA_OFFSET_EQUATION(val)\
							    vBIT(val, 6, 2)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_ATRA_OFFSET_DDR_VALUE(val)\
							    vBIT(val, 8, 8)
/* 0x099d0 */	u64	g3ifgr01_cml_group1_dll_tra_hold;
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_TRA_HOLD_DDR_MASTER_MIN(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_TRA_HOLD_DDR_MASTER_MAX(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_TRA_HOLD_DDR_TIME(val)\
							    vBIT(val, 16, 24)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_TRA_HOLD_DDR_UPDATES(val)\
							    vBIT(val, 40, 24)
/* 0x099d8 */	u64	g3ifgr01_cml_group1_dll_atra_hold;
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_ATRA_HOLD_DDR_MASTER_MIN(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_ATRA_HOLD_DDR_MASTER_MAX(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_ATRA_HOLD_DDR_TIME(val)\
							    vBIT(val, 16, 24)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_ATRA_HOLD_DDR_UPDATES(val)\
							    vBIT(val, 40, 24)
/* 0x099e0 */	u64	g3ifgr01_cml_group1_dll_master_codes;
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_MASTER_CODES_DDR_RDQS_TRA_HOLD(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_MASTER_CODES_DDR_RDQS_ATRA_HOLD(val)\
							    vBIT(val, 25, 7)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_MASTER_CODES_DDR_WDQS_RAW(val)\
							    vBIT(val, 41, 7)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_MASTER_CODES_DDR_RDQS_RAW(val)\
							    vBIT(val, 57, 7)
/* 0x099e8 */	u64	g3ifgr01_cml_group1_dll_atra_timer;
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_ATRA_TIMER_VALUE(val)\
							    vBIT(val, 0, 16)
#define	VXGE_HAL_G3IFGR01_CML_GROUP1_DLL_ATRA_TIMER_ENABLED mBIT(23)
	u8	unused09a10[0x09a10 - 0x099f0];

/* 0x09a10 */	u64	g3ifgr23_cml_group2_dll_rdqs;
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_RDQS_SA_CAL(val)   vBIT(val, 0, 8)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_RDQS_SB_CAL(val)   vBIT(val, 8, 8)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_RDQS_ATRA_SA_CAL(val) vBIT(val, 32, 8)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_RDQS_ATRA_SB_CAL(val) vBIT(val, 40, 8)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_RDQS_DDR_DLL_S(val) vBIT(val, 57, 7)
/* 0x09a18 */	u64	g3ifgr23_cml_group2_dll_rdqs1;
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_RDQS1_ROLL	    mBIT(7)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_RDQS1_DLL_ENABLE   mBIT(14)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_RDQS1_DLL_ENABLE_ATRA	mBIT(15)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_RDQS1_DLL_UPD(val) vBIT(val, 21, 3)
/* 0x09a20 */	u64	g3ifgr23_cml_group2_dll_wdqs;
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_WDQS_SA_CAL(val)   vBIT(val, 0, 8)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_WDQS_SB_CAL(val)   vBIT(val, 8, 8)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_WDQS_DDR_DLL_S(val) vBIT(val, 57, 7)
/* 0x09a28 */	u64	g3ifgr23_cml_group2_dll_wdqs1;
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_WDQS1_ROLL	    mBIT(7)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_WDQS1_DLL_ENABLE   mBIT(15)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_WDQS1_DLL_UPD(val) vBIT(val, 21, 3)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_WDQS1_SEL_MASTER_WDQS_CKN\
							    mBIT(31)
/* 0x09a30 */	u64	g3ifgr23_cml_group2_dll_training1;
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_TRAINING1_DDR_TRA_STATUS(val)\
							    vBIT(val, 4, 4)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_TRAINING1_DDR_TRA_MIN(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_TRAINING1_DDR_TRA_MAX(val)\
							    vBIT(val, 17, 7)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_TRAINING1_DDR_ATRA_STATUS(val)\
							    vBIT(val, 36, 4)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_TRAINING1_DDR_ATRA_MIN(val)\
							    vBIT(val, 41, 7)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_TRAINING1_DDR_ATRA_MAX(val)\
							    vBIT(val, 49, 7)
/* 0x09a38 */	u64	g3ifgr23_cml_group2_dll_training2;
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_TRAINING2_DDR_ATRA_PASS_CNT(val)\
							    vBIT(val, 0, 32)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_TRAINING2_DDR_ATRA_FAIL_CNT(val)\
							    vBIT(val, 32, 16)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_TRAINING2_DDR_ATRA_TIMER_FAIL_CNT(val)\
							    vBIT(val, 48, 16)
/* 0x09a40 */	u64	g3ifgr23_cml_group2_dll_training3;
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_TRAINING3_DLL_TRA_DATA00(val)\
							    vBIT(val, 0, 16)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_TRAINING3_DLL_TRA_DATA01(val)\
							    vBIT(val, 16, 16)
/* 0x09a48 */	u64	g3ifgr23_cml_group2_dll_act_training5;
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_ACT_TRAINING5_START_CODE(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_ACT_TRAINING5_END_CODE(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_ACT_TRAINING5_DISABLE mBIT(23)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_ACT_TRAINING5_TCNT(val) \
							    vBIT(val, 28, 4)
/* 0x09a50 */	u64	g3ifgr23_cml_group2_dll_training6;
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_TRAINING6_DLL_TRA_EN_HALF_EYE_VALID\
							    mBIT(7)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_TRAINING6_DLL_ATRA_EN_HALF_EYE_VALID\
							    mBIT(15)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_TRAINING6_DLL_TRA_EN_MASTER_CORR\
							    mBIT(23)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_TRAINING6_DLL_ATRA_EN_MASTER_CORR\
							    mBIT(31)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_TRAINING6_DLL_SEL_TRA_ONLY	mBIT(39)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_TRAINING6_DLL_EN_MOVING_AVR	mBIT(47)
/* 0x09a58 */	u64	g3ifgr23_cml_group2_dll_atra_offset;
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_ATRA_OFFSET_EQUATION(val)\
							    vBIT(val, 6, 2)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_ATRA_OFFSET_DDR_VALUE(val)\
							    vBIT(val, 8, 8)
/* 0x09a60 */	u64	g3ifgr23_cml_group2_dll_tra_hold;
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_TRA_HOLD_DDR_MASTER_MIN(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_TRA_HOLD_DDR_MASTER_MAX(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_TRA_HOLD_DDR_TIME(val)\
							    vBIT(val, 16, 24)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_TRA_HOLD_DDR_UPDATES(val)\
							    vBIT(val, 40, 24)
/* 0x09a68 */	u64	g3ifgr23_cml_group2_dll_atra_hold;
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_ATRA_HOLD_DDR_MASTER_MIN(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_ATRA_HOLD_DDR_MASTER_MAX(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_ATRA_HOLD_DDR_TIME(val)\
							    vBIT(val, 16, 24)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_ATRA_HOLD_DDR_UPDATES(val)\
							    vBIT(val, 40, 24)
/* 0x09a70 */	u64	g3ifgr23_cml_group2_dll_master_codes;
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_MASTER_CODES_DDR_RDQS_TRA_HOLD(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_MASTER_CODES_DDR_RDQS_ATRA_HOLD(val)\
							    vBIT(val, 25, 7)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_MASTER_CODES_DDR_WDQS_RAW(val)\
							    vBIT(val, 41, 7)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_MASTER_CODES_DDR_RDQS_RAW(val)\
							    vBIT(val, 57, 7)
/* 0x09a78 */	u64	g3ifgr23_cml_group2_dll_atra_timer;
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_ATRA_TIMER_VALUE(val) vBIT(val, 0, 16)
#define	VXGE_HAL_G3IFGR23_CML_GROUP2_DLL_ATRA_TIMER_ENABLED mBIT(23)
/* 0x09a80 */	u64	g3ifgr23_cml_group3_dll_rdqs;
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_RDQS_SA_CAL(val)   vBIT(val, 0, 8)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_RDQS_SB_CAL(val)   vBIT(val, 8, 8)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_RDQS_ATRA_SA_CAL(val) vBIT(val, 32, 8)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_RDQS_ATRA_SB_CAL(val) vBIT(val, 40, 8)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_RDQS_DDR_DLL_S(val) vBIT(val, 57, 7)
/* 0x09a88 */	u64	g3ifgr23_cml_group3_dll_rdqs1;
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_RDQS1_ROLL	    mBIT(7)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_RDQS1_DLL_ENABLE   mBIT(14)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_RDQS1_DLL_ENABLE_ATRA mBIT(15)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_RDQS1_DLL_UPD(val) vBIT(val, 21, 3)
/* 0x09a90 */	u64	g3ifgr23_cml_group3_dll_wdqs;
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_WDQS_SA_CAL(val)   vBIT(val, 0, 8)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_WDQS_SB_CAL(val)   vBIT(val, 8, 8)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_WDQS_DDR_DLL_S(val) vBIT(val, 57, 7)
/* 0x09a98 */	u64	g3ifgr23_cml_group3_dll_wdqs1;
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_WDQS1_ROLL	    mBIT(7)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_WDQS1_DLL_ENABLE   mBIT(15)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_WDQS1_DLL_UPD(val) vBIT(val, 21, 3)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_WDQS1_SEL_MASTER_WDQS_CKN mBIT(31)
/* 0x09aa0 */	u64	g3ifgr23_cml_group3_dll_training1;
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_TRAINING1_DDR_TRA_STATUS(val)\
							    vBIT(val, 4, 4)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_TRAINING1_DDR_TRA_MIN(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_TRAINING1_DDR_TRA_MAX(val)\
							    vBIT(val, 17, 7)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_TRAINING1_DDR_ATRA_STATUS(val)\
							    vBIT(val, 36, 4)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_TRAINING1_DDR_ATRA_MIN(val)\
							    vBIT(val, 41, 7)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_TRAINING1_DDR_ATRA_MAX(val)\
							    vBIT(val, 49, 7)
/* 0x09aa8 */	u64	g3ifgr23_cml_group3_dll_training2;
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_TRAINING2_DDR_ATRA_PASS_CNT(val)\
							    vBIT(val, 0, 32)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_TRAINING2_DDR_ATRA_FAIL_CNT(val)\
							    vBIT(val, 32, 16)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_TRAINING2_DDR_ATRA_TIMER_FAIL_CNT(val)\
							    vBIT(val, 48, 16)
/* 0x09ab0 */	u64	g3ifgr23_cml_group3_dll_training3;
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_TRAINING3_DLL_TRA_DATA00(val)\
							    vBIT(val, 0, 16)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_TRAINING3_DLL_TRA_DATA01(val)\
							    vBIT(val, 16, 16)
/* 0x09ab8 */	u64	g3ifgr23_cml_group3_dll_act_training5;
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_ACT_TRAINING5_START_CODE(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_ACT_TRAINING5_END_CODE(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_ACT_TRAINING5_DISABLE mBIT(23)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_ACT_TRAINING5_TCNT(val)\
							    vBIT(val, 28, 4)
/* 0x09ac0 */	u64	g3ifgr23_cml_group3_dll_training6;
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_TRAINING6_DLL_TRA_EN_HALF_EYE_VALID\
							    mBIT(7)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_TRAINING6_DLL_ATRA_EN_HALF_EYE_VALID\
							    mBIT(15)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_TRAINING6_DLL_TRA_EN_MASTER_CORR\
							    mBIT(23)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_TRAINING6_DLL_ATRA_EN_MASTER_CORR\
							    mBIT(31)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_TRAINING6_DLL_SEL_TRA_ONLY mBIT(39)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_TRAINING6_DLL_EN_MOVING_AVR mBIT(47)
/* 0x09ac8 */	u64	g3ifgr23_cml_group3_dll_atra_offset;
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_ATRA_OFFSET_EQUATION(val)\
							    vBIT(val, 6, 2)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_ATRA_OFFSET_DDR_VALUE(val)\
							    vBIT(val, 8, 8)
/* 0x09ad0 */	u64	g3ifgr23_cml_group3_dll_tra_hold;
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_TRA_HOLD_DDR_MASTER_MIN(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_TRA_HOLD_DDR_MASTER_MAX(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_TRA_HOLD_DDR_TIME(val)\
							    vBIT(val, 16, 24)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_TRA_HOLD_DDR_UPDATES(val)\
							    vBIT(val, 40, 24)
/* 0x09ad8 */	u64	g3ifgr23_cml_group3_dll_atra_hold;
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_ATRA_HOLD_DDR_MASTER_MIN(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_ATRA_HOLD_DDR_MASTER_MAX(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_ATRA_HOLD_DDR_TIME(val)\
							    vBIT(val, 16, 24)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_ATRA_HOLD_DDR_UPDATES(val)\
							    vBIT(val, 40, 24)
/* 0x09ae0 */	u64	g3ifgr23_cml_group3_dll_master_codes;
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_MASTER_CODES_DDR_RDQS_TRA_HOLD(val)\
							    vBIT(val, 9, 7)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_MASTER_CODES_DDR_RDQS_ATRA_HOLD(val)\
							    vBIT(val, 25, 7)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_MASTER_CODES_DDR_WDQS_RAW(val)\
							    vBIT(val, 41, 7)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_MASTER_CODES_DDR_RDQS_RAW(val)\
							    vBIT(val, 57, 7)
/* 0x09ae8 */	u64	g3ifgr23_cml_group3_dll_atra_timer;
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_ATRA_TIMER_VALUE(val) vBIT(val, 0, 16)
#define	VXGE_HAL_G3IFGR23_CML_GROUP3_DLL_ATRA_TIMER_ENABLED mBIT(23)
	u8	unused09b00[0x09b00 - 0x09af0];

/* 0x09b00 */	u64	vpath_to_vplane_map[17];
#define	VXGE_HAL_VPATH_TO_VPLANE_MAP_VPATH_TO_VPLANE_MAP(val) vBIT(val, 3, 5)
	u8	unused09c30[0x09c30 - 0x09b88];

/* 0x09c30 */	u64	xgxs_cfg_port[2];
#define	VXGE_HAL_XGXS_CFG_PORT_SIG_DETECT_FORCE_LOS(val)    vBIT(val, 16, 4)
#define	VXGE_HAL_XGXS_CFG_PORT_SIG_DETECT_FORCE_VALID(val)  vBIT(val, 20, 4)
#define	VXGE_HAL_XGXS_CFG_PORT_SEL_INFO_0		    mBIT(27)
#define	VXGE_HAL_XGXS_CFG_PORT_SEL_INFO_1(val)		    vBIT(val, 29, 3)
#define	VXGE_HAL_XGXS_CFG_PORT_TX_LANE0_SKEW(val)	    vBIT(val, 32, 4)
#define	VXGE_HAL_XGXS_CFG_PORT_TX_LANE1_SKEW(val)	    vBIT(val, 36, 4)
#define	VXGE_HAL_XGXS_CFG_PORT_TX_LANE2_SKEW(val)	    vBIT(val, 40, 4)
#define	VXGE_HAL_XGXS_CFG_PORT_TX_LANE3_SKEW(val)	    vBIT(val, 44, 4)
/* 0x09c40 */	u64	xgxs_rxber_cfg_port[2];
#define	VXGE_HAL_XGXS_RXBER_CFG_PORT_INTERVAL_DUR(val)	    vBIT(val, 0, 4)
#define	VXGE_HAL_XGXS_RXBER_CFG_PORT_RXGXS_INTERVAL_CNT(val) vBIT(val, 16, 48)
/* 0x09c50 */	u64	xgxs_rxber_status_port[2];
#define	VXGE_HAL_XGXS_RXBER_STATUS_PORT_RXGXS_RXGXS_LANE_A_ERR_CNT(val)\
							    vBIT(val, 0, 16)
#define	VXGE_HAL_XGXS_RXBER_STATUS_PORT_RXGXS_RXGXS_LANE_B_ERR_CNT(val)\
							    vBIT(val, 16, 16)
#define	VXGE_HAL_XGXS_RXBER_STATUS_PORT_RXGXS_RXGXS_LANE_C_ERR_CNT(val)\
							    vBIT(val, 32, 16)
#define	VXGE_HAL_XGXS_RXBER_STATUS_PORT_RXGXS_RXGXS_LANE_D_ERR_CNT(val)\
							    vBIT(val, 48, 16)
/* 0x09c60 */	u64	xgxs_status_port[2];
#define	VXGE_HAL_XGXS_STATUS_PORT_XMACJ_PCS_TX_ACTIVITY(val) vBIT(val, 0, 4)
#define	VXGE_HAL_XGXS_STATUS_PORT_XMACJ_PCS_RX_ACTIVITY(val) vBIT(val, 4, 4)
#define	VXGE_HAL_XGXS_STATUS_PORT_XMACJ_PCS_CTC_FIFO_ERR    BIT(11)
#define	VXGE_HAL_XGXS_STATUS_PORT_XMACJ_PCS_BYTE_SYNC_LOST(val) vBIT(val, 12, 4)
#define	VXGE_HAL_XGXS_STATUS_PORT_XMACJ_PCS_CTC_ERR(val)    vBIT(val, 16, 4)
#define	VXGE_HAL_XGXS_STATUS_PORT_XMACJ_PCS_ALIGNMENT_ERR   mBIT(23)
#define	VXGE_HAL_XGXS_STATUS_PORT_XMACJ_PCS_DEC_ERR(val)    vBIT(val, 24, 8)
#define	VXGE_HAL_XGXS_STATUS_PORT_XMACJ_PCS_SKIP_INS_REQ(val) vBIT(val, 32, 4)
#define	VXGE_HAL_XGXS_STATUS_PORT_XMACJ_PCS_SKIP_DEL_REQ(val) vBIT(val, 36, 4)
/* 0x09c70 */	u64	xgxs_pma_reset_port[2];
#define	VXGE_HAL_XGXS_PMA_RESET_PORT_SERDES_RESET(val)	    vBIT(val, 0, 8)
	u8	unused09c90[0x09c90 - 0x09c80];

/* 0x09c90 */	u64	xgxs_static_cfg_port[2];
#define	VXGE_HAL_XGXS_STATIC_CFG_PORT_FW_CTRL_SERDES	    mBIT(3)
	u8	unused09cc0[0x09cc0 - 0x09ca0];

/* 0x09cc0 */	u64	xgxs_serdes_fw_cfg_port[2];
#define	VXGE_HAL_XGXS_SERDES_FW_CFG_PORT_TX_EN_LANE0(val)   vBIT(val, 1, 3)
#define	VXGE_HAL_XGXS_SERDES_FW_CFG_PORT_TX_EN_LANE1(val)   vBIT(val, 5, 3)
#define	VXGE_HAL_XGXS_SERDES_FW_CFG_PORT_TX_EN_LANE2(val)   vBIT(val, 9, 3)
#define	VXGE_HAL_XGXS_SERDES_FW_CFG_PORT_TX_EN_LANE3(val)   vBIT(val, 13, 3)
#define	VXGE_HAL_XGXS_SERDES_FW_CFG_PORT_RX_EN_LANE0	    mBIT(16)
#define	VXGE_HAL_XGXS_SERDES_FW_CFG_PORT_RX_EN_LANE1	    mBIT(17)
#define	VXGE_HAL_XGXS_SERDES_FW_CFG_PORT_RX_EN_LANE2	    mBIT(18)
#define	VXGE_HAL_XGXS_SERDES_FW_CFG_PORT_RX_EN_LANE3	    mBIT(19)
#define	VXGE_HAL_XGXS_SERDES_FW_CFG_PORT_RX_PLL_PWRON_LANE0 mBIT(20)
#define	VXGE_HAL_XGXS_SERDES_FW_CFG_PORT_RX_PLL_PWRON_LANE1 mBIT(21)
#define	VXGE_HAL_XGXS_SERDES_FW_CFG_PORT_RX_PLL_PWRON_LANE2 mBIT(22)
#define	VXGE_HAL_XGXS_SERDES_FW_CFG_PORT_RX_PLL_PWRON_LANE3 mBIT(23)
#define	VXGE_HAL_XGXS_SERDES_FW_CFG_PORT_RX_TERM_EN_LANE0   mBIT(24)
#define	VXGE_HAL_XGXS_SERDES_FW_CFG_PORT_RX_TERM_EN_LANE1   mBIT(25)
#define	VXGE_HAL_XGXS_SERDES_FW_CFG_PORT_RX_TERM_EN_LANE2   mBIT(26)
#define	VXGE_HAL_XGXS_SERDES_FW_CFG_PORT_RX_TERM_EN_LANE3   mBIT(27)
#define	VXGE_HAL_XGXS_SERDES_FW_CFG_PORT_MPLL_CK_OFF	    mBIT(31)
#define	VXGE_HAL_XGXS_SERDES_FW_CFG_PORT_MPLL_PWRON	    mBIT(35)
#define	VXGE_HAL_XGXS_SERDES_FW_CFG_PORT_CKO_WORD_CON(val)  vBIT(val, 37, 3)
#define	VXGE_HAL_XGXS_SERDES_FW_CFG_PORT_RESET_N	    mBIT(43)
#define	VXGE_HAL_XGXS_SERDES_FW_CFG_PORT_CKO_WORD_READY	    mBIT(47)
#define	VXGE_HAL_XGXS_SERDES_FW_CFG_PORT_RX_CK_READY_LANE0  mBIT(48)
#define	VXGE_HAL_XGXS_SERDES_FW_CFG_PORT_RX_CK_READY_LANE1  mBIT(49)
#define	VXGE_HAL_XGXS_SERDES_FW_CFG_PORT_RX_CK_READY_LANE2  mBIT(50)
#define	VXGE_HAL_XGXS_SERDES_FW_CFG_PORT_RX_CK_READY_LANE3  mBIT(51)
#define	VXGE_HAL_XGXS_SERDES_FW_CFG_PORT_TRUST_HW_RX_CK_READY mBIT(55)
/* 0x09cd0 */	u64	xgxs_serdes_tx_cfg_port[2];
#define	VXGE_HAL_XGXS_SERDES_TX_CFG_PORT_TX_BOOST_LANE0(val) vBIT(val, 0, 4)
#define	VXGE_HAL_XGXS_SERDES_TX_CFG_PORT_TX_BOOST_LANE1(val) vBIT(val, 4, 4)
#define	VXGE_HAL_XGXS_SERDES_TX_CFG_PORT_TX_BOOST_LANE2(val) vBIT(val, 8, 4)
#define	VXGE_HAL_XGXS_SERDES_TX_CFG_PORT_TX_BOOST_LANE3(val) vBIT(val, 12, 4)
#define	VXGE_HAL_XGXS_SERDES_TX_CFG_PORT_TX_ATTEN_LANE0(val) vBIT(val, 17, 3)
#define	VXGE_HAL_XGXS_SERDES_TX_CFG_PORT_TX_ATTEN_LANE1(val) vBIT(val, 21, 3)
#define	VXGE_HAL_XGXS_SERDES_TX_CFG_PORT_TX_ATTEN_LANE2(val) vBIT(val, 25, 3)
#define	VXGE_HAL_XGXS_SERDES_TX_CFG_PORT_TX_ATTEN_LANE3(val) vBIT(val, 29, 3)
#define	VXGE_HAL_XGXS_SERDES_TX_CFG_PORT_TX_CALC_LANE0	    mBIT(32)
#define	VXGE_HAL_XGXS_SERDES_TX_CFG_PORT_TX_CALC_LANE1	    mBIT(33)
#define	VXGE_HAL_XGXS_SERDES_TX_CFG_PORT_TX_CALC_LANE2	    mBIT(34)
#define	VXGE_HAL_XGXS_SERDES_TX_CFG_PORT_TX_CALC_LANE3	    mBIT(35)
#define	VXGE_HAL_XGXS_SERDES_TX_CFG_PORT_TX_CLK_ALIGN_LANE0 mBIT(36)
#define	VXGE_HAL_XGXS_SERDES_TX_CFG_PORT_TX_CLK_ALIGN_LANE1 mBIT(37)
#define	VXGE_HAL_XGXS_SERDES_TX_CFG_PORT_TX_CLK_ALIGN_LANE2 mBIT(38)
#define	VXGE_HAL_XGXS_SERDES_TX_CFG_PORT_TX_CLK_ALIGN_LANE3 mBIT(39)
#define	VXGE_HAL_XGXS_SERDES_TX_CFG_PORT_TX_CKO_EN_LANE0    mBIT(40)
#define	VXGE_HAL_XGXS_SERDES_TX_CFG_PORT_TX_CKO_EN_LANE1    mBIT(41)
#define	VXGE_HAL_XGXS_SERDES_TX_CFG_PORT_TX_CKO_EN_LANE2    mBIT(42)
#define	VXGE_HAL_XGXS_SERDES_TX_CFG_PORT_TX_CKO_EN_LANE3    mBIT(43)
#define	VXGE_HAL_XGXS_SERDES_TX_CFG_PORT_TX_EDGERATE_LANE0(val)	vBIT(val, 44, 2)
#define	VXGE_HAL_XGXS_SERDES_TX_CFG_PORT_TX_EDGERATE_LANE1(val)	vBIT(val, 46, 2)
#define	VXGE_HAL_XGXS_SERDES_TX_CFG_PORT_TX_EDGERATE_LANE2(val)	vBIT(val, 48, 2)
#define	VXGE_HAL_XGXS_SERDES_TX_CFG_PORT_TX_EDGERATE_LANE3(val)	vBIT(val, 50, 2)
#define	VXGE_HAL_XGXS_SERDES_TX_CFG_PORT_TX_LVL(val)	    vBIT(val, 55, 5)
/* 0x09ce0 */	u64	xgxs_serdes_rx_cfg_port[2];
#define	VXGE_HAL_XGXS_SERDES_RX_CFG_PORT_RX_ALIGN_EN_LANE0  mBIT(0)
#define	VXGE_HAL_XGXS_SERDES_RX_CFG_PORT_RX_ALIGN_EN_LANE1  mBIT(1)
#define	VXGE_HAL_XGXS_SERDES_RX_CFG_PORT_RX_ALIGN_EN_LANE2  mBIT(2)
#define	VXGE_HAL_XGXS_SERDES_RX_CFG_PORT_RX_ALIGN_EN_LANE3  mBIT(3)
#define	VXGE_HAL_XGXS_SERDES_RX_CFG_PORT_RX_EQ_VAL_LANE0(val) vBIT(val, 5, 3)
#define	VXGE_HAL_XGXS_SERDES_RX_CFG_PORT_RX_EQ_VAL_LANE1(val) vBIT(val, 9, 3)
#define	VXGE_HAL_XGXS_SERDES_RX_CFG_PORT_RX_EQ_VAL_LANE2(val) vBIT(val, 13, 3)
#define	VXGE_HAL_XGXS_SERDES_RX_CFG_PORT_RX_EQ_VAL_LANE3(val) vBIT(val, 17, 3)
#define	VXGE_HAL_XGXS_SERDES_RX_CFG_PORT_RX_DPLL_MODE_LANE0(val)\
							    vBIT(val, 21, 3)
#define	VXGE_HAL_XGXS_SERDES_RX_CFG_PORT_RX_DPLL_MODE_LANE1(val)\
							    vBIT(val, 25, 3)
#define	VXGE_HAL_XGXS_SERDES_RX_CFG_PORT_RX_DPLL_MODE_LANE2(val)\
							    vBIT(val, 29, 3)
#define	VXGE_HAL_XGXS_SERDES_RX_CFG_PORT_RX_DPLL_MODE_LANE3(val)\
							    vBIT(val, 33, 3)
/* 0x09cf0 */	u64	xgxs_serdes_extra_cfg_port[2];
#define	VXGE_HAL_XGXS_SERDES_EXTRA_CFG_PORT_DPLL_RESET_LANE0 mBIT(0)
#define	VXGE_HAL_XGXS_SERDES_EXTRA_CFG_PORT_DPLL_RESET_LANE1 mBIT(1)
#define	VXGE_HAL_XGXS_SERDES_EXTRA_CFG_PORT_DPLL_RESET_LANE2 mBIT(2)
#define	VXGE_HAL_XGXS_SERDES_EXTRA_CFG_PORT_DPLL_RESET_LANE3 mBIT(3)
#define	VXGE_HAL_XGXS_SERDES_EXTRA_CFG_PORT_LOS_CTL_LANE0(val) vBIT(val, 4, 2)
#define	VXGE_HAL_XGXS_SERDES_EXTRA_CFG_PORT_LOS_CTL_LANE1(val) vBIT(val, 6, 2)
#define	VXGE_HAL_XGXS_SERDES_EXTRA_CFG_PORT_LOS_CTL_LANE2(val) vBIT(val, 8, 2)
#define	VXGE_HAL_XGXS_SERDES_EXTRA_CFG_PORT_LOS_CTL_LANE3(val) vBIT(val, 10, 2)
#define	VXGE_HAL_XGXS_SERDES_EXTRA_CFG_PORT_WIDE_XFACE	    mBIT(14)
#define	VXGE_HAL_XGXS_SERDES_EXTRA_CFG_PORT_RTUNE_DO_TUNE   mBIT(15)
#define	VXGE_HAL_XGXS_SERDES_EXTRA_CFG_PORT_LOS_LVL(val)    vBIT(val, 19, 5)
#define	VXGE_HAL_XGXS_SERDES_EXTRA_CFG_PORT_CKO_ALIVE_CON(val) vBIT(val, 28, 2)
#define	VXGE_HAL_XGXS_SERDES_EXTRA_CFG_PORT_MPLL_SS_EN	    mBIT(32)
#define	VXGE_HAL_XGXS_SERDES_EXTRA_CFG_PORT_MPLL_INT_CTL(val) vBIT(val, 33, 3)
#define	VXGE_HAL_XGXS_SERDES_EXTRA_CFG_PORT_MPLL_PROP_CTL(val) vBIT(val, 37, 3)
/* 0x09d00 */	u64	xgxs_serdes_status_port[2];
#define	VXGE_HAL_XGXS_SERDES_STATUS_PORT_XPRG_RX_COMMA_DET_LANE0(val)\
							    vBIT(val, 0, 2)
#define	VXGE_HAL_XGXS_SERDES_STATUS_PORT_XPRG_RX_COMMA_DET_LANE1(val)\
							    vBIT(val, 2, 2)
#define	VXGE_HAL_XGXS_SERDES_STATUS_PORT_XPRG_RX_COMMA_DET_LANE2(val)\
							    vBIT(val, 4, 2)
#define	VXGE_HAL_XGXS_SERDES_STATUS_PORT_XPRG_RX_COMMA_DET_LANE3(val)\
							    vBIT(val, 6, 2)
#define	VXGE_HAL_XGXS_SERDES_STATUS_PORT_XPRG_TX_RXPRES_LANE0 mBIT(8)
#define	VXGE_HAL_XGXS_SERDES_STATUS_PORT_XPRG_TX_RXPRES_LANE1 mBIT(9)
#define	VXGE_HAL_XGXS_SERDES_STATUS_PORT_XPRG_TX_RXPRES_LANE2 mBIT(10)
#define	VXGE_HAL_XGXS_SERDES_STATUS_PORT_XPRG_TX_RXPRES_LANE3 mBIT(11)
#define	VXGE_HAL_XGXS_SERDES_STATUS_PORT_XPRG_TX_DONE_LANE0 mBIT(12)
#define	VXGE_HAL_XGXS_SERDES_STATUS_PORT_XPRG_TX_DONE_LANE1 mBIT(13)
#define	VXGE_HAL_XGXS_SERDES_STATUS_PORT_XPRG_TX_DONE_LANE2 mBIT(14)
#define	VXGE_HAL_XGXS_SERDES_STATUS_PORT_XPRG_TX_DONE_LANE3 mBIT(15)
#define	VXGE_HAL_XGXS_SERDES_STATUS_PORT_XPRG_RX_PLL_STATE_LANE0 mBIT(16)
#define	VXGE_HAL_XGXS_SERDES_STATUS_PORT_XPRG_RX_PLL_STATE_LANE1 mBIT(17)
#define	VXGE_HAL_XGXS_SERDES_STATUS_PORT_XPRG_RX_PLL_STATE_LANE2 mBIT(18)
#define	VXGE_HAL_XGXS_SERDES_STATUS_PORT_XPRG_RX_PLL_STATE_LANE3 mBIT(19)
#define	VXGE_HAL_XGXS_SERDES_STATUS_PORT_XPRG_RX_VALID_LANE0 mBIT(20)
#define	VXGE_HAL_XGXS_SERDES_STATUS_PORT_XPRG_RX_VALID_LANE1 mBIT(21)
#define	VXGE_HAL_XGXS_SERDES_STATUS_PORT_XPRG_RX_VALID_LANE2 mBIT(22)
#define	VXGE_HAL_XGXS_SERDES_STATUS_PORT_XPRG_RX_VALID_LANE3 mBIT(23)
#define	VXGE_HAL_XGXS_SERDES_STATUS_PORT_XPRG_LOS_LANE0	    mBIT(24)
#define	VXGE_HAL_XGXS_SERDES_STATUS_PORT_XPRG_LOS_LANE1	    mBIT(25)
#define	VXGE_HAL_XGXS_SERDES_STATUS_PORT_XPRG_LOS_LANE2	    mBIT(26)
#define	VXGE_HAL_XGXS_SERDES_STATUS_PORT_XPRG_LOS_LANE3	    mBIT(27)
#define	VXGE_HAL_XGXS_SERDES_STATUS_PORT_XPRG_OP_DONE_ASSERTED	mBIT(30)
#define	VXGE_HAL_XGXS_SERDES_STATUS_PORT_XPRG_OP_DONE_DEASSERTED mBIT(31)
#define	VXGE_HAL_XGXS_SERDES_STATUS_PORT_XPRG_POWER_GOOD    mBIT(35)
#define	VXGE_HAL_XGXS_SERDES_STATUS_PORT_XPRG_SERDES_INIT_COMPLETE mBIT(39)
/* 0x09d10 */	u64	xgxs_serdes_cr_access_port[2];
#define	VXGE_HAL_XGXS_SERDES_CR_ACCESS_PORT_WE		    mBIT(3)
#define	VXGE_HAL_XGXS_SERDES_CR_ACCESS_PORT_STROBE	    mBIT(7)
#define	VXGE_HAL_XGXS_SERDES_CR_ACCESS_PORT_ADDR(val)	    vBIT(val, 16, 16)
#define	VXGE_HAL_XGXS_SERDES_CR_ACCESS_PORT_DATA(val)	    vBIT(val, 48, 16)
	u8	unused09d40[0x09d40 - 0x09d20];

/* 0x09d40 */	u64	xgxs_info_port[2];
#define	VXGE_HAL_XGXS_INFO_PORT_XMACJ_INFO_0(val)	    vBIT(val, 0, 32)
#define	VXGE_HAL_XGXS_INFO_PORT_XMACJ_INFO_1(val)	    vBIT(val, 32, 32)
/* 0x09d50 */	u64	ratemgmt_cfg_port[2];
#define	VXGE_HAL_RATEMGMT_CFG_PORT_MODE(val)		    vBIT(val, 2, 2)
#define	VXGE_HAL_RATEMGMT_CFG_PORT_RATE			    mBIT(7)
#define	VXGE_HAL_RATEMGMT_CFG_PORT_FIXED_USE_FSM	    mBIT(11)
#define	VXGE_HAL_RATEMGMT_CFG_PORT_ANTP_USE_FSM		    mBIT(15)
#define	VXGE_HAL_RATEMGMT_CFG_PORT_ANBE_USE_FSM		    mBIT(19)
/* 0x09d60 */	u64	ratemgmt_status_port[2];
#define	VXGE_HAL_RATEMGMT_STATUS_PORT_RATEMGMT_COMPLETE	    mBIT(3)
#define	VXGE_HAL_RATEMGMT_STATUS_PORT_RATEMGMT_RATE	    mBIT(7)
#define	VXGE_HAL_RATEMGMT_STATUS_PORT_RATEMGMT_MAC_MATCHES_PHY mBIT(11)
	u8	unused09d80[0x09d80 - 0x09d70];

/* 0x09d80 */	u64	ratemgmt_fixed_cfg_port[2];
#define	VXGE_HAL_RATEMGMT_FIXED_CFG_PORT_RESTART	    mBIT(7)
/* 0x09d90 */	u64	ratemgmt_antp_cfg_port[2];
#define	VXGE_HAL_RATEMGMT_ANTP_CFG_PORT_RESTART		    mBIT(7)
#define	VXGE_HAL_RATEMGMT_ANTP_CFG_PORT_USE_PREAMBLE_EXT_PHY mBIT(11)
#define	VXGE_HAL_RATEMGMT_ANTP_CFG_PORT_USE_ACT_SEL	    mBIT(15)
#define	VXGE_HAL_RATEMGMT_ANTP_CFG_PORT_T_RETRY_PHY_QUERY(val)\
							    vBIT(val, 16, 4)
#define	VXGE_HAL_RATEMGMT_ANTP_CFG_PORT_T_WAIT_MDIO_RESP(val)\
							    vBIT(val, 20, 4)
#define	VXGE_HAL_RATEMGMT_ANTP_CFG_PORT_T_LDOWN_REAUTO_RESP(val)\
							    vBIT(val, 24, 4)
#define	VXGE_HAL_RATEMGMT_ANTP_CFG_PORT_ADVERTISE_10G	    mBIT(31)
#define	VXGE_HAL_RATEMGMT_ANTP_CFG_PORT_ADVERTISE_1G	    mBIT(35)
/* 0x09da0 */	u64	ratemgmt_anbe_cfg_port[2];
#define	VXGE_HAL_RATEMGMT_ANBE_CFG_PORT_RESTART	mBIT(7)
#define	VXGE_HAL_RATEMGMT_ANBE_CFG_PORT_PARALLEL_DETECT_10G_KX4_ENABLE mBIT(11)
#define	VXGE_HAL_RATEMGMT_ANBE_CFG_PORT_PARALLEL_DETECT_1G_KX_ENABLE mBIT(15)
#define	VXGE_HAL_RATEMGMT_ANBE_CFG_PORT_T_SYNC_10G_KX4(val) vBIT(val, 16, 4)
#define	VXGE_HAL_RATEMGMT_ANBE_CFG_PORT_T_SYNC_1G_KX(val)   vBIT(val, 20, 4)
#define	VXGE_HAL_RATEMGMT_ANBE_CFG_PORT_T_DME_EXCHANGE(val) vBIT(val, 24, 4)
#define	VXGE_HAL_RATEMGMT_ANBE_CFG_PORT_ADVERTISE_10G_KX4   mBIT(31)
#define	VXGE_HAL_RATEMGMT_ANBE_CFG_PORT_ADVERTISE_1G_KX	    mBIT(35)
/* 0x09db0 */	u64	anbe_cfg_port[2];
#define	VXGE_HAL_ANBE_CFG_PORT_RESET_CFG_REGS(val)	    vBIT(val, 0, 8)
#define	VXGE_HAL_ANBE_CFG_PORT_ALIGN_10G_KX4_OVERRIDE(val)  vBIT(val, 10, 2)
#define	VXGE_HAL_ANBE_CFG_PORT_SYNC_1G_KX_OVERRIDE(val)	    vBIT(val, 14, 2)
/* 0x09dc0 */	u64	anbe_mgr_ctrl_port[2];
#define	VXGE_HAL_ANBE_MGR_CTRL_PORT_WE	mBIT(3)
#define	VXGE_HAL_ANBE_MGR_CTRL_PORT_STROBE		    mBIT(7)
#define	VXGE_HAL_ANBE_MGR_CTRL_PORT_ADDR(val)		    vBIT(val, 15, 9)
#define	VXGE_HAL_ANBE_MGR_CTRL_PORT_DATA(val)		    vBIT(val, 32, 32)
	u8	unused09de0[0x09de0 - 0x09dd0];

/* 0x09de0 */	u64	anbe_fw_mstr_port[2];
#define	VXGE_HAL_ANBE_FW_MSTR_PORT_CONNECT_BEAN_TO_SERDES   mBIT(3)
#define	VXGE_HAL_ANBE_FW_MSTR_PORT_TX_ZEROES_TO_SERDES	    mBIT(7)
/* 0x09df0 */	u64	anbe_hwfsm_gen_status_port[2];
#define	VXGE_HAL_ANBE_HWFSM_GEN_STATUS_PORT_RATEMGMT_CHOSE_10G_KX4_USING_PD\
							    mBIT(3)
#define	VXGE_HAL_ANBE_HWFSM_GEN_STATUS_PORT_RATEMGMT_CHOSE_10G_KX4_USING_DME\
							    mBIT(7)
#define	VXGE_HAL_ANBE_HWFSM_GEN_STATUS_PORT_RATEMGMT_CHOSE_1G_KX_USING_PD\
							    mBIT(11)
#define	VXGE_HAL_ANBE_HWFSM_GEN_STATUS_PORT_RATEMGMT_CHOSE_1G_KX_USING_DME\
							    mBIT(15)
#define	VXGE_HAL_ANBE_HWFSM_GEN_STATUS_PORT_RATEMGMT_ANBEFSM_STATE(val)\
							    vBIT(val, 18, 6)
#define	VXGE_HAL_ANBE_HWFSM_GEN_STATUS_PORT_RATEMGMT_BEAN_NEXT_PAGE_RECEIVED\
							    mBIT(27)
#define	VXGE_HAL_ANBE_HWFSM_GEN_STATUS_PORT_RATEMGMT_BEAN_PARALLEL_DETECT_FAULT\
							    mBIT(31)
#define	VXGE_HAL_ANBE_HWFSM_GEN_STATUS_PORT_RATEMGMT_BEAN_BASE_PAGE_RECEIVED\
							    mBIT(35)
#define	VXGE_HAL_ANBE_HWFSM_GEN_STATUS_PORT_RATEMGMT_BEAN_AUTONEG_COMPLETE\
							    mBIT(39)
#define	VXGE_HAL_ANBE_HWFSM_GEN_STATUS_PORT_RATEMGMT_UNEXP_NP_BEFORE_BP\
							    mBIT(43)
#define	VXGE_HAL_ANBE_HWFSM_GEN_STATUS_PORT_RATEMGMT_UNEXP_AN_COMPL_BEFORE_BP\
							    mBIT(47)
#define	VXGE_HAL_ANBE_HWFSM_GEN_STATUS_PORT_RATEMGMT_UNEXP_AN_COMPL_BEFORE_NP\
							    mBIT(51)
#define	VXGE_HAL_ANBE_HWFSM_GEN_STATUS_PORT_RATEMGMT_UNEXP_MODE_WHEN_AN_COMPL\
							    mBIT(55)
#define	VXGE_HAL_ANBE_HWFSM_GEN_STATUS_PORT_RATEMGMT_COUNT_BP(val)\
							    vBIT(val, 56, 4)
#define	VXGE_HAL_ANBE_HWFSM_GEN_STATUS_PORT_RATEMGMT_COUNT_NP(val)\
							    vBIT(val, 60, 4)
/* 0x09e00 */	u64	anbe_hwfsm_bp_status_port[2];
#define	VXGE_HAL_ANBE_HWFSM_BP_STATUS_PORT_RATEMGMT_BP_FEC_ENABLE	mBIT(32)
#define	VXGE_HAL_ANBE_HWFSM_BP_STATUS_PORT_RATEMGMT_BP_FEC_ABILITY	mBIT(33)
#define	VXGE_HAL_ANBE_HWFSM_BP_STATUS_PORT_RATEMGMT_BP_10G_KR_CAPABLE	mBIT(40)
#define	VXGE_HAL_ANBE_HWFSM_BP_STATUS_PORT_RATEMGMT_BP_10G_KX4_CAPABLE	mBIT(41)
#define	VXGE_HAL_ANBE_HWFSM_BP_STATUS_PORT_RATEMGMT_BP_1G_KX_CAPABLE	mBIT(42)
#define	VXGE_HAL_ANBE_HWFSM_BP_STATUS_PORT_RATEMGMT_BP_TX_NONCE(val)\
							    vBIT(val, 43, 5)
#define	VXGE_HAL_ANBE_HWFSM_BP_STATUS_PORT_RATEMGMT_BP_NP   mBIT(48)
#define	VXGE_HAL_ANBE_HWFSM_BP_STATUS_PORT_RATEMGMT_BP_ACK  mBIT(49)
#define	VXGE_HAL_ANBE_HWFSM_BP_STATUS_PORT_RATEMGMT_BP_REMOTE_FAULT mBIT(50)
#define	VXGE_HAL_ANBE_HWFSM_BP_STATUS_PORT_RATEMGMT_BP_ASM_DIR mBIT(51)
#define	VXGE_HAL_ANBE_HWFSM_BP_STATUS_PORT_RATEMGMT_BP_PAUSE mBIT(53)
#define	VXGE_HAL_ANBE_HWFSM_BP_STATUS_PORT_RATEMGMT_BP_ECHOED_NONCE(val)\
							    vBIT(val, 54, 5)
#define	VXGE_HAL_ANBE_HWFSM_BP_STATUS_PORT_RATEMGMT_BP_SELECTOR_FIELD(val)\
							    vBIT(val, 59, 5)
/* 0x09e10 */	u64	anbe_hwfsm_np_status_port[2];
#define	VXGE_HAL_ANBE_HWFSM_NP_STATUS_PORT_RATEMGMT_NP_BITS_47_TO_32(val)\
							    vBIT(val, 16, 16)
#define	VXGE_HAL_ANBE_HWFSM_NP_STATUS_PORT_RATEMGMT_NP_BITS_31_TO_0(val)\
							    vBIT(val, 32, 32)
	u8	unused09e30[0x09e30 - 0x09e20];

/* 0x09e30 */	u64	antp_gen_cfg_port[2];
/* 0x09e40 */	u64	antp_hwfsm_gen_status_port[2];
#define	VXGE_HAL_ANTP_HWFSM_GEN_STATUS_PORT_RATEMGMT_CHOSE_10G	mBIT(3)
#define	VXGE_HAL_ANTP_HWFSM_GEN_STATUS_PORT_RATEMGMT_CHOSE_1G	mBIT(7)
#define	VXGE_HAL_ANTP_HWFSM_GEN_STATUS_PORT_RATEMGMT_ANTPFSM_STATE(val)\
							    vBIT(val, 10, 6)
#define	VXGE_HAL_ANTP_HWFSM_GEN_STATUS_PORT_RATEMGMT_UNEXPECTED_TIMEOUT	mBIT(19)
#define	VXGE_HAL_ANTP_HWFSM_GEN_STATUS_PORT_RATEMGMT_AUTONEG_COMPLETE	mBIT(23)
#define	VXGE_HAL_ANTP_HWFSM_GEN_STATUS_PORT_RATEMGMT_UNEXPECTED_NO_LP_XNP\
							    mBIT(27)
#define	VXGE_HAL_ANTP_HWFSM_GEN_STATUS_PORT_RATEMGMT_GOT_LP_XNP	mBIT(31)
#define	VXGE_HAL_ANTP_HWFSM_GEN_STATUS_PORT_RATEMGMT_UNEXPECTED_MESSAGE_CODE\
							    mBIT(35)
#define	VXGE_HAL_ANTP_HWFSM_GEN_STATUS_PORT_RATEMGMT_GOT_LP_MESSAGE_CODE_10G_1K\
							    mBIT(39)
#define	VXGE_HAL_ANTP_HWFSM_GEN_STATUS_PORT_RATEMGMT_UNEXPECTED_NO_HCD	mBIT(43)
#define	VXGE_HAL_ANTP_HWFSM_GEN_STATUS_PORT_RATEMGMT_FOUND_HCD	mBIT(47)
#define	VXGE_HAL_ANTP_HWFSM_GEN_STATUS_PORT_RATEMGMT_UNEXPECTED_INVALID_RATE\
							    mBIT(51)
#define	VXGE_HAL_ANTP_HWFSM_GEN_STATUS_PORT_RATEMGMT_VALID_RATE	mBIT(55)
#define	VXGE_HAL_ANTP_HWFSM_GEN_STATUS_PORT_RATEMGMT_PERSISTENT_LDOWN mBIT(59)
/* 0x09e50 */	u64	antp_hwfsm_bp_status_port[2];
#define	VXGE_HAL_ANTP_HWFSM_BP_STATUS_PORT_RATEMGMT_BP_NP	mBIT(0)
#define	VXGE_HAL_ANTP_HWFSM_BP_STATUS_PORT_RATEMGMT_BP_ACK	mBIT(1)
#define	VXGE_HAL_ANTP_HWFSM_BP_STATUS_PORT_RATEMGMT_BP_RF	mBIT(2)
#define	VXGE_HAL_ANTP_HWFSM_BP_STATUS_PORT_RATEMGMT_BP_XNP	mBIT(3)
#define	VXGE_HAL_ANTP_HWFSM_BP_STATUS_PORT_RATEMGMT_BP_ABILITY_FIELD(val)\
								vBIT(val, 4, 7)
#define	VXGE_HAL_ANTP_HWFSM_BP_STATUS_PORT_RATEMGMT_BP_SELECTOR_FIELD(val)\
								vBIT(val, 11, 5)
/* 0x09e60 */	u64	antp_hwfsm_xnp_status_port[2];
#define	VXGE_HAL_ANTP_HWFSM_XNP_STATUS_PORT_RATEMGMT_XNP_NP	mBIT(0)
#define	VXGE_HAL_ANTP_HWFSM_XNP_STATUS_PORT_RATEMGMT_XNP_ACK	mBIT(1)
#define	VXGE_HAL_ANTP_HWFSM_XNP_STATUS_PORT_RATEMGMT_XNP_MP	mBIT(2)
#define	VXGE_HAL_ANTP_HWFSM_XNP_STATUS_PORT_RATEMGMT_XNP_ACK2	mBIT(3)
#define	VXGE_HAL_ANTP_HWFSM_XNP_STATUS_PORT_RATEMGMT_XNP_TOGGLE	mBIT(4)
#define	VXGE_HAL_ANTP_HWFSM_XNP_STATUS_PORT_RATEMGMT_XNP_MESSAGE_CODE(val)\
							    vBIT(val, 5, 11)
#define	VXGE_HAL_ANTP_HWFSM_XNP_STATUS_PORT_RATEMGMT_XNP_UNF_CODE_FIELD1(val)\
							    vBIT(val, 16, 16)
#define	VXGE_HAL_ANTP_HWFSM_XNP_STATUS_PORT_RATEMGMT_XNP_UNF_CODE_FIELD2(val)\
							    vBIT(val, 32, 16)
/* 0x09e70 */	u64	mdio_mgr_access_port[2];
#define	VXGE_HAL_MDIO_MGR_ACCESS_PORT_STROBE_ONE	    mBIT(3)
#define	VXGE_HAL_MDIO_MGR_ACCESS_PORT_OP_TYPE(val)	    vBIT(val, 5, 3)
#define	VXGE_HAL_MDIO_MGR_ACCESS_PORT_DEVAD(val)	    vBIT(val, 11, 5)
#define	VXGE_HAL_MDIO_MGR_ACCESS_PORT_ADDR(val)		    vBIT(val, 16, 16)
#define	VXGE_HAL_MDIO_MGR_ACCESS_PORT_DATA(val)		    vBIT(val, 32, 16)
#define	VXGE_HAL_MDIO_MGR_ACCESS_PORT_ST_PATTERN(val)	    vBIT(val, 49, 2)
#define	VXGE_HAL_MDIO_MGR_ACCESS_PORT_PREAMBLE		    mBIT(51)
#define	VXGE_HAL_MDIO_MGR_ACCESS_PORT_PRTAD(val)	    vBIT(val, 55, 5)
#define	VXGE_HAL_MDIO_MGR_ACCESS_PORT_STROBE_TWO	    mBIT(63)
	u8	unused09ea0[0x09ea0 - 0x09e80];

/* 0x09ea0 */	u64	mdio_gen_cfg_port[2];

	u8	unused0a200[0x0a200 - 0x09eb0];

/* 0x0a200 */	u64	xmac_vsport_choices_vh[17];
#define	VXGE_HAL_XMAC_VSPORT_CHOICES_VH_VSPORT_VECTOR(val)  vBIT(val, 0, 17)
	u8	unused0a400[0x0a400 - 0x0a288];

/* 0x0a400 */	u64	rx_thresh_cfg_vp[17];
#define	VXGE_HAL_RX_THRESH_CFG_VP_PAUSE_LOW_THR(val)	    vBIT(val, 0, 8)
#define	VXGE_HAL_RX_THRESH_CFG_VP_PAUSE_HIGH_THR(val)	    vBIT(val, 8, 8)
#define	VXGE_HAL_RX_THRESH_CFG_VP_RED_THR_0(val)	    vBIT(val, 16, 8)
#define	VXGE_HAL_RX_THRESH_CFG_VP_RED_THR_1(val)	    vBIT(val, 24, 8)
#define	VXGE_HAL_RX_THRESH_CFG_VP_RED_THR_2(val)	    vBIT(val, 32, 8)
#define	VXGE_HAL_RX_THRESH_CFG_VP_RED_THR_3(val)	    vBIT(val, 40, 8)
	u8	unused0ac00[0x0ac00 - 0x0a488];

/* 0x0ac00 */	u64	fau_adaptive_lro_vpath_enable;
#define	VXGE_HAL_FAU_ADAPTIVE_LRO_VPATH_ENABLE_EN(val)	    vBIT(val, 0, 17)
/* 0x0ac08 */	u64	fau_adaptive_lro_base_sid_vp[17];
#define	VXGE_HAL_FAU_ADAPTIVE_LRO_BASE_SID_VP_VALUE(val)    vBIT(val, 2, 6)
#define	VXGE_HAL_FAU_ADAPTIVE_LRO_BASE_SID_VP_USE_HASH_WIDTH(val)\
							    vBIT(val, 11, 5)

} vxge_hal_mrpcim_reg_t;

__EXTERN_END_DECLS

#endif	/* VXGE_HAL_MRPCIM_REGS_H */

/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */
#include "adf_c4xxx_ras.h"
#include "adf_accel_devices.h"
#include "adf_c4xxx_hw_data.h"
#include <adf_dev_err.h>
#include "adf_c4xxx_inline.h"
#include <sys/priv.h>

#define ADF_RAS_STR_LEN 64

static int adf_sysctl_read_ras_correctable(SYSCTL_HANDLER_ARGS)
{
	struct adf_accel_dev *accel_dev = arg1;
	unsigned long counter = 0;

	if (priv_check(curthread, PRIV_DRIVER) != 0)
		return EPERM;

	if (accel_dev->ras_counters)
		counter = atomic_read(&accel_dev->ras_counters[ADF_RAS_CORR]);

	return SYSCTL_OUT(req, &counter, sizeof(counter));
}

static int adf_sysctl_read_ras_uncorrectable(SYSCTL_HANDLER_ARGS)
{
	struct adf_accel_dev *accel_dev = arg1;
	unsigned long counter = 0;

	if (priv_check(curthread, PRIV_DRIVER) != 0)
		return EPERM;

	if (accel_dev->ras_counters)
		counter = atomic_read(&accel_dev->ras_counters[ADF_RAS_UNCORR]);

	return SYSCTL_OUT(req, &counter, sizeof(counter));
}

static int adf_sysctl_read_ras_fatal(SYSCTL_HANDLER_ARGS)
{
	struct adf_accel_dev *accel_dev = arg1;
	unsigned long counter = 0;

	if (priv_check(curthread, PRIV_DRIVER) != 0)
		return EPERM;

	if (accel_dev->ras_counters)
		counter = atomic_read(&accel_dev->ras_counters[ADF_RAS_FATAL]);

	return SYSCTL_OUT(req, &counter, sizeof(counter));
}

static int adf_sysctl_write_ras_reset(SYSCTL_HANDLER_ARGS)
{
	struct adf_accel_dev *accel_dev = arg1;
	int value = 0;
	int ret = SYSCTL_IN(req, &value, sizeof(value));

	if (priv_check(curthread, PRIV_DRIVER) != 0)
		return EPERM;

	if (!ret && value != 0 && accel_dev->ras_counters) {
	}

	return SYSCTL_OUT(req, &value, sizeof(value));
}

int
adf_init_ras(struct adf_accel_dev *accel_dev)
{
	struct sysctl_ctx_list *qat_sysctl_ctx;
	struct sysctl_oid *qat_sysctl_tree;
	struct sysctl_oid *ras_corr;
	struct sysctl_oid *ras_uncor;
	struct sysctl_oid *ras_fat;
	struct sysctl_oid *ras_res;
	int i;

	accel_dev->ras_counters = kcalloc(ADF_RAS_ERRORS,
					  sizeof(*accel_dev->ras_counters),
					  GFP_KERNEL);
	if (!accel_dev->ras_counters)
		return -ENOMEM;

	for (i = 0; i < ADF_RAS_ERRORS; ++i)

		qat_sysctl_ctx =
		    device_get_sysctl_ctx(accel_dev->accel_pci_dev.pci_dev);
	qat_sysctl_tree =
	    device_get_sysctl_tree(accel_dev->accel_pci_dev.pci_dev);
	ras_corr = SYSCTL_ADD_OID(qat_sysctl_ctx,
				  SYSCTL_CHILDREN(qat_sysctl_tree),
				  OID_AUTO,
				  "ras_correctable",
				  CTLTYPE_ULONG | CTLFLAG_RD | CTLFLAG_DYN,
				  accel_dev,
				  0,
				  adf_sysctl_read_ras_correctable,
				  "LU",
				  "QAT RAS correctable");
	accel_dev->ras_correctable = ras_corr;
	if (!accel_dev->ras_correctable) {
		device_printf(GET_DEV(accel_dev),
			      "Failed to register ras_correctable sysctl\n");
		return -EINVAL;
	}
	ras_uncor = SYSCTL_ADD_OID(qat_sysctl_ctx,
				   SYSCTL_CHILDREN(qat_sysctl_tree),
				   OID_AUTO,
				   "ras_uncorrectable",
				   CTLTYPE_ULONG | CTLFLAG_RD | CTLFLAG_DYN,
				   accel_dev,
				   0,
				   adf_sysctl_read_ras_uncorrectable,
				   "LU",
				   "QAT RAS uncorrectable");
	accel_dev->ras_uncorrectable = ras_uncor;
	if (!accel_dev->ras_uncorrectable) {
		device_printf(GET_DEV(accel_dev),
			      "Failed to register ras_uncorrectable sysctl\n");
		return -EINVAL;
	}

	ras_fat = SYSCTL_ADD_OID(qat_sysctl_ctx,
				 SYSCTL_CHILDREN(qat_sysctl_tree),
				 OID_AUTO,
				 "ras_fatal",
				 CTLTYPE_ULONG | CTLFLAG_RD | CTLFLAG_DYN,
				 accel_dev,
				 0,
				 adf_sysctl_read_ras_fatal,
				 "LU",
				 "QAT RAS fatal");
	accel_dev->ras_fatal = ras_fat;
	if (!accel_dev->ras_fatal) {
		device_printf(GET_DEV(accel_dev),
			      "Failed to register ras_fatal sysctl\n");
		return -EINVAL;
	}

	ras_res = SYSCTL_ADD_OID(qat_sysctl_ctx,
				 SYSCTL_CHILDREN(qat_sysctl_tree),
				 OID_AUTO,
				 "ras_reset",
				 CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_DYN,
				 accel_dev,
				 0,
				 adf_sysctl_write_ras_reset,
				 "I",
				 "QAT RAS reset");
	accel_dev->ras_reset = ras_res;
	if (!accel_dev->ras_reset) {
		device_printf(GET_DEV(accel_dev),
			      "Failed to register ras_reset sysctl\n");
		return -EINVAL;
	}

	return 0;
}

void
adf_exit_ras(struct adf_accel_dev *accel_dev)
{
	if (accel_dev->ras_counters) {
		remove_oid(accel_dev, accel_dev->ras_correctable);
		remove_oid(accel_dev, accel_dev->ras_uncorrectable);
		remove_oid(accel_dev, accel_dev->ras_fatal);
		remove_oid(accel_dev, accel_dev->ras_reset);

		accel_dev->ras_correctable = NULL;
		accel_dev->ras_uncorrectable = NULL;
		accel_dev->ras_fatal = NULL;
		accel_dev->ras_reset = NULL;

		kfree(accel_dev->ras_counters);
		accel_dev->ras_counters = NULL;
	}
}

static inline void
adf_log_source_iastatssm(struct adf_accel_dev *accel_dev,
			 struct resource *pmisc,
			 u32 iastatssm,
			 u32 accel_num)
{
	if (iastatssm & ADF_C4XXX_IASTATSSM_UERRSSMSH_MASK)
		device_printf(
		    GET_DEV(accel_dev),
		    "Uncorrectable error shared memory detected in accel: %u\n",
		    accel_num);

	if (iastatssm & ADF_C4XXX_IASTATSSM_CERRSSMSH_MASK)
		device_printf(
		    GET_DEV(accel_dev),
		    "Correctable error shared memory detected in accel: %u\n",
		    accel_num);

	if (iastatssm & ADF_C4XXX_IASTATSSM_UERRSSMMMP0_MASK)
		device_printf(
		    GET_DEV(accel_dev),
		    "Uncorrectable error MMP0 detected in accel: %u\n",
		    accel_num);

	if (iastatssm & ADF_C4XXX_IASTATSSM_CERRSSMMMP0_MASK)
		device_printf(GET_DEV(accel_dev),
			      "Correctable error MMP0 detected in accel: %u\n",
			      accel_num);

	if (iastatssm & ADF_C4XXX_IASTATSSM_UERRSSMMMP1_MASK)
		device_printf(
		    GET_DEV(accel_dev),
		    "Uncorrectable error MMP1 detected in accel: %u\n",
		    accel_num);

	if (iastatssm & ADF_C4XXX_IASTATSSM_CERRSSMMMP1_MASK)
		device_printf(GET_DEV(accel_dev),
			      "Correctable error MMP1 detected in accel: %u\n",
			      accel_num);

	if (iastatssm & ADF_C4XXX_IASTATSSM_UERRSSMMMP2_MASK)
		device_printf(
		    GET_DEV(accel_dev),
		    "Uncorrectable error MMP2 detected in accel: %u\n",
		    accel_num);

	if (iastatssm & ADF_C4XXX_IASTATSSM_CERRSSMMMP2_MASK)
		device_printf(GET_DEV(accel_dev),
			      "Correctable error MMP2 detected in accel: %u\n",
			      accel_num);

	if (iastatssm & ADF_C4XXX_IASTATSSM_UERRSSMMMP3_MASK)
		device_printf(
		    GET_DEV(accel_dev),
		    "Uncorrectable error MMP3 detected in accel: %u\n",
		    accel_num);

	if (iastatssm & ADF_C4XXX_IASTATSSM_CERRSSMMMP3_MASK)
		device_printf(GET_DEV(accel_dev),
			      "Correctable error MMP3 detected in accel: %u\n",
			      accel_num);

	if (iastatssm & ADF_C4XXX_IASTATSSM_UERRSSMMMP4_MASK)
		device_printf(
		    GET_DEV(accel_dev),
		    "Uncorrectable error MMP4 detected in accel: %u\n",
		    accel_num);

	if (iastatssm & ADF_C4XXX_IASTATSSM_CERRSSMMMP4_MASK)
		device_printf(GET_DEV(accel_dev),
			      "Correctable error MMP4 detected in accel: %u\n",
			      accel_num);

	if (iastatssm & ADF_C4XXX_IASTATSSM_PPERR_MASK)
		device_printf(
		    GET_DEV(accel_dev),
		    "Uncorrectable error Push or Pull detected in accel: %u\n",
		    accel_num);

	if (iastatssm & ADF_C4XXX_IASTATSSM_CPPPAR_ERR_MASK)
		device_printf(
		    GET_DEV(accel_dev),
		    "Uncorrectable CPP parity error detected in accel: %u\n",
		    accel_num);

	if (iastatssm & ADF_C4XXX_IASTATSSM_RFPAR_ERR_MASK)
		device_printf(
		    GET_DEV(accel_dev),
		    "Uncorrectable SSM RF parity error detected in accel: %u\n",
		    accel_num);
}

static inline void
adf_clear_source_statssm(struct adf_accel_dev *accel_dev,
			 struct resource *pmisc,
			 u32 statssm,
			 u32 accel_num)
{
	if (statssm & ADF_C4XXX_IASTATSSM_UERRSSMSH_MASK)
		adf_csr_fetch_and_and(pmisc,
				      ADF_C4XXX_UERRSSMSH(accel_num),
				      ADF_C4XXX_UERRSSMSH_INTS_CLEAR_MASK);

	if (statssm & ADF_C4XXX_IASTATSSM_CERRSSMSH_MASK)
		adf_csr_fetch_and_and(pmisc,
				      ADF_C4XXX_CERRSSMSH(accel_num),
				      ADF_C4XXX_CERRSSMSH_INTS_CLEAR_MASK);

	if (statssm & ADF_C4XXX_IASTATSSM_UERRSSMMMP0_MASK)
		adf_csr_fetch_and_and(pmisc,
				      ADF_C4XXX_UERRSSMMMP(accel_num, 0),
				      ~ADF_C4XXX_UERRSSMMMP_INTS_CLEAR_MASK);

	if (statssm & ADF_C4XXX_IASTATSSM_CERRSSMMMP0_MASK)
		adf_csr_fetch_and_and(pmisc,
				      ADF_C4XXX_CERRSSMMMP(accel_num, 0),
				      ~ADF_C4XXX_CERRSSMMMP_INTS_CLEAR_MASK);

	if (statssm & ADF_C4XXX_IASTATSSM_UERRSSMMMP1_MASK)
		adf_csr_fetch_and_and(pmisc,
				      ADF_C4XXX_UERRSSMMMP(accel_num, 1),
				      ~ADF_C4XXX_UERRSSMMMP_INTS_CLEAR_MASK);

	if (statssm & ADF_C4XXX_IASTATSSM_CERRSSMMMP1_MASK)
		adf_csr_fetch_and_and(pmisc,
				      ADF_C4XXX_CERRSSMMMP(accel_num, 1),
				      ~ADF_C4XXX_CERRSSMMMP_INTS_CLEAR_MASK);

	if (statssm & ADF_C4XXX_IASTATSSM_UERRSSMMMP2_MASK)
		adf_csr_fetch_and_and(pmisc,
				      ADF_C4XXX_UERRSSMMMP(accel_num, 2),
				      ~ADF_C4XXX_UERRSSMMMP_INTS_CLEAR_MASK);

	if (statssm & ADF_C4XXX_IASTATSSM_CERRSSMMMP2_MASK)
		adf_csr_fetch_and_and(pmisc,
				      ADF_C4XXX_CERRSSMMMP(accel_num, 2),
				      ~ADF_C4XXX_CERRSSMMMP_INTS_CLEAR_MASK);

	if (statssm & ADF_C4XXX_IASTATSSM_UERRSSMMMP3_MASK)
		adf_csr_fetch_and_and(pmisc,
				      ADF_C4XXX_UERRSSMMMP(accel_num, 3),
				      ~ADF_C4XXX_UERRSSMMMP_INTS_CLEAR_MASK);

	if (statssm & ADF_C4XXX_IASTATSSM_CERRSSMMMP3_MASK)
		adf_csr_fetch_and_and(pmisc,
				      ADF_C4XXX_CERRSSMMMP(accel_num, 3),
				      ~ADF_C4XXX_CERRSSMMMP_INTS_CLEAR_MASK);

	if (statssm & ADF_C4XXX_IASTATSSM_UERRSSMMMP4_MASK)
		adf_csr_fetch_and_and(pmisc,
				      ADF_C4XXX_UERRSSMMMP(accel_num, 4),
				      ~ADF_C4XXX_UERRSSMMMP_INTS_CLEAR_MASK);

	if (statssm & ADF_C4XXX_IASTATSSM_CERRSSMMMP4_MASK)
		adf_csr_fetch_and_and(pmisc,
				      ADF_C4XXX_CERRSSMMMP(accel_num, 4),
				      ~ADF_C4XXX_CERRSSMMMP_INTS_CLEAR_MASK);

	if (statssm & ADF_C4XXX_IASTATSSM_PPERR_MASK)
		adf_csr_fetch_and_and(pmisc,
				      ADF_PPERR(accel_num),
				      ~ADF_C4XXX_PPERR_INTS_CLEAR_MASK);

	if (statssm & ADF_C4XXX_IASTATSSM_RFPAR_ERR_MASK)
		adf_csr_fetch_and_or(pmisc,
				     ADF_C4XXX_SSMSOFTERRORPARITY(accel_num),
				     0UL);

	if (statssm & ADF_C4XXX_IASTATSSM_CPPPAR_ERR_MASK)
		adf_csr_fetch_and_or(pmisc,
				     ADF_C4XXX_SSMCPPERR(accel_num),
				     0UL);
}

static inline void
adf_process_errsou8(struct adf_accel_dev *accel_dev, struct resource *pmisc)
{
	int i;
	u32 mecorrerr = ADF_CSR_RD(pmisc, ADF_C4XXX_HI_ME_COR_ERRLOG);
	const unsigned long tmp_mecorrerr = mecorrerr;

	/* For each correctable error in ME increment RAS counter */
	for_each_set_bit(i,
			 &tmp_mecorrerr,
			 ADF_C4XXX_HI_ME_COR_ERRLOG_SIZE_IN_BITS)
	{
		atomic_inc(&accel_dev->ras_counters[ADF_RAS_CORR]);
		device_printf(GET_DEV(accel_dev),
			      "Correctable error detected in AE%d\n",
			      i);
	}

	/* Clear interrupt from errsou8 (RW1C) */
	ADF_CSR_WR(pmisc, ADF_C4XXX_HI_ME_COR_ERRLOG, mecorrerr);
}

static inline void
adf_handle_ae_uncorr_err(struct adf_accel_dev *accel_dev,
			 struct resource *pmisc)
{
	int i;
	u32 me_uncorr_err = ADF_CSR_RD(pmisc, ADF_C4XXX_HI_ME_UNCERR_LOG);
	const unsigned long tmp_me_uncorr_err = me_uncorr_err;

	/* For each uncorrectable fatal error in AE increment RAS error
	 * counter.
	 */
	for_each_set_bit(i,
			 &tmp_me_uncorr_err,
			 ADF_C4XXX_HI_ME_UNCOR_ERRLOG_BITS)
	{
		atomic_inc(&accel_dev->ras_counters[ADF_RAS_FATAL]);
		device_printf(GET_DEV(accel_dev),
			      "Uncorrectable error detected in AE%d\n",
			      i);
	}

	/* Clear interrupt from me_uncorr_err (RW1C) */
	ADF_CSR_WR(pmisc, ADF_C4XXX_HI_ME_UNCERR_LOG, me_uncorr_err);
}

static inline void
adf_handle_ri_mem_par_err(struct adf_accel_dev *accel_dev,
			  struct resource *pmisc,
			  bool *reset_required)
{
	u32 ri_mem_par_err_sts = 0;
	u32 ri_mem_par_err_ferr = 0;

	ri_mem_par_err_sts = ADF_CSR_RD(pmisc, ADF_C4XXX_RI_MEM_PAR_ERR_STS);

	ri_mem_par_err_ferr = ADF_CSR_RD(pmisc, ADF_C4XXX_RI_MEM_PAR_ERR_FERR);

	if (ri_mem_par_err_sts & ADF_C4XXX_RI_MEM_PAR_ERR_STS_MASK) {
		atomic_inc(&accel_dev->ras_counters[ADF_RAS_UNCORR]);
		device_printf(
		    GET_DEV(accel_dev),
		    "Uncorrectable RI memory parity error detected.\n");
	}

	if (ri_mem_par_err_sts & ADF_C4XXX_RI_MEM_MSIX_TBL_INT_MASK) {
		atomic_inc(&accel_dev->ras_counters[ADF_RAS_FATAL]);
		device_printf(
		    GET_DEV(accel_dev),
		    "Uncorrectable fatal MSIX table parity error detected.\n");
		*reset_required = true;
	}

	device_printf(GET_DEV(accel_dev),
		      "ri_mem_par_err_sts=0x%X\tri_mem_par_err_ferr=%u\n",
		      ri_mem_par_err_sts,
		      ri_mem_par_err_ferr);

	ADF_CSR_WR(pmisc, ADF_C4XXX_RI_MEM_PAR_ERR_STS, ri_mem_par_err_sts);
}

static inline void
adf_handle_ti_mem_par_err(struct adf_accel_dev *accel_dev,
			  struct resource *pmisc)
{
	u32 ti_mem_par_err_sts0 = 0;
	u32 ti_mem_par_err_sts1 = 0;
	u32 ti_mem_par_err_ferr = 0;

	ti_mem_par_err_sts0 = ADF_CSR_RD(pmisc, ADF_C4XXX_TI_MEM_PAR_ERR_STS0);
	ti_mem_par_err_sts1 = ADF_CSR_RD(pmisc, ADF_C4XXX_TI_MEM_PAR_ERR_STS1);
	ti_mem_par_err_ferr =
	    ADF_CSR_RD(pmisc, ADF_C4XXX_TI_MEM_PAR_ERR_FIRST_ERROR);

	atomic_inc(&accel_dev->ras_counters[ADF_RAS_FATAL]);
	ti_mem_par_err_sts1 &= ADF_C4XXX_TI_MEM_PAR_ERR_STS1_MASK;

	device_printf(GET_DEV(accel_dev),
		      "Uncorrectable TI memory parity error detected.\n");
	device_printf(GET_DEV(accel_dev),
		      "ti_mem_par_err_sts0=0x%X\tti_mem_par_err_sts1=0x%X\t"
		      "ti_mem_par_err_ferr=0x%X\n",
		      ti_mem_par_err_sts0,
		      ti_mem_par_err_sts1,
		      ti_mem_par_err_ferr);

	ADF_CSR_WR(pmisc, ADF_C4XXX_TI_MEM_PAR_ERR_STS0, ti_mem_par_err_sts0);
	ADF_CSR_WR(pmisc, ADF_C4XXX_TI_MEM_PAR_ERR_STS1, ti_mem_par_err_sts1);
}

static inline void
adf_log_fatal_cmd_par_err(struct adf_accel_dev *accel_dev, char *err_type)
{
	atomic_inc(&accel_dev->ras_counters[ADF_RAS_FATAL]);
	device_printf(GET_DEV(accel_dev),
		      "Fatal error detected: %s command parity\n",
		      err_type);
}

static inline void
adf_handle_host_cpp_par_err(struct adf_accel_dev *accel_dev,
			    struct resource *pmisc)
{
	u32 host_cpp_par_err = 0;

	host_cpp_par_err =
	    ADF_CSR_RD(pmisc, ADF_C4XXX_HI_CPP_AGENT_CMD_PAR_ERR_LOG);

	if (host_cpp_par_err & ADF_C4XXX_TI_CMD_PAR_ERR)
		adf_log_fatal_cmd_par_err(accel_dev, "TI");

	if (host_cpp_par_err & ADF_C4XXX_RI_CMD_PAR_ERR)
		adf_log_fatal_cmd_par_err(accel_dev, "RI");

	if (host_cpp_par_err & ADF_C4XXX_ICI_CMD_PAR_ERR)
		adf_log_fatal_cmd_par_err(accel_dev, "ICI");

	if (host_cpp_par_err & ADF_C4XXX_ICE_CMD_PAR_ERR)
		adf_log_fatal_cmd_par_err(accel_dev, "ICE");

	if (host_cpp_par_err & ADF_C4XXX_ARAM_CMD_PAR_ERR)
		adf_log_fatal_cmd_par_err(accel_dev, "ARAM");

	if (host_cpp_par_err & ADF_C4XXX_CFC_CMD_PAR_ERR)
		adf_log_fatal_cmd_par_err(accel_dev, "CFC");

	if (ADF_C4XXX_SSM_CMD_PAR_ERR(host_cpp_par_err))
		adf_log_fatal_cmd_par_err(accel_dev, "SSM");

	/* Clear interrupt from host_cpp_par_err (RW1C) */
	ADF_CSR_WR(pmisc,
		   ADF_C4XXX_HI_CPP_AGENT_CMD_PAR_ERR_LOG,
		   host_cpp_par_err);
}

static inline void
adf_process_errsou9(struct adf_accel_dev *accel_dev,
		    struct resource *pmisc,
		    u32 errsou,
		    bool *reset_required)
{
	if (errsou & ADF_C4XXX_ME_UNCORR_ERROR) {
		adf_handle_ae_uncorr_err(accel_dev, pmisc);

		/* Notify caller that function level reset is required. */
		*reset_required = true;
	}

	if (errsou & ADF_C4XXX_CPP_CMD_PAR_ERR) {
		adf_handle_host_cpp_par_err(accel_dev, pmisc);
		*reset_required = true;
	}

	/* RI memory parity errors are uncorrectable non-fatal errors
	 * with exception of bit 22 MSIX table parity error, which should
	 * be treated as fatal error, followed by device restart.
	 */
	if (errsou & ADF_C4XXX_RI_MEM_PAR_ERR)
		adf_handle_ri_mem_par_err(accel_dev, pmisc, reset_required);

	if (errsou & ADF_C4XXX_TI_MEM_PAR_ERR) {
		adf_handle_ti_mem_par_err(accel_dev, pmisc);
		*reset_required = true;
	}
}

static inline void
adf_process_exprpssmcpr(struct adf_accel_dev *accel_dev,
			struct resource *pmisc,
			u32 accel)
{
	u32 exprpssmcpr;

	/* CPR0 */
	exprpssmcpr = ADF_CSR_RD(pmisc, ADF_C4XXX_EXPRPSSMCPR0(accel));
	if (exprpssmcpr & ADF_C4XXX_EXPRPSSM_FATAL_MASK) {
		device_printf(GET_DEV(accel_dev),
			      "Uncorrectable error CPR0 detected in accel %u\n",
			      accel);
		atomic_inc(&accel_dev->ras_counters[ADF_RAS_UNCORR]);
	}
	if (exprpssmcpr & ADF_C4XXX_EXPRPSSM_SOFT_MASK) {
		device_printf(GET_DEV(accel_dev),
			      "Correctable error CPR0 detected in accel %u\n",
			      accel);
		atomic_inc(&accel_dev->ras_counters[ADF_RAS_CORR]);
	}
	ADF_CSR_WR(pmisc, ADF_C4XXX_EXPRPSSMCPR0(accel), 0);

	/* CPR1 */
	exprpssmcpr = ADF_CSR_RD(pmisc, ADF_C4XXX_EXPRPSSMCPR1(accel));
	if (exprpssmcpr & ADF_C4XXX_EXPRPSSM_FATAL_MASK) {
		device_printf(GET_DEV(accel_dev),
			      "Uncorrectable error CPR1 detected in accel %u\n",
			      accel);
		atomic_inc(&accel_dev->ras_counters[ADF_RAS_UNCORR]);
	}
	if (exprpssmcpr & ADF_C4XXX_EXPRPSSM_SOFT_MASK) {
		device_printf(GET_DEV(accel_dev),
			      "Correctable error CPR1 detected in accel %u\n",
			      accel);
		atomic_inc(&accel_dev->ras_counters[ADF_RAS_CORR]);
	}
	ADF_CSR_WR(pmisc, ADF_C4XXX_EXPRPSSMCPR1(accel), 0);
}

static inline void
adf_process_exprpssmxlt(struct adf_accel_dev *accel_dev,
			struct resource *pmisc,
			u32 accel)
{
	u32 exprpssmxlt;

	/* XTL0 */
	exprpssmxlt = ADF_CSR_RD(pmisc, ADF_C4XXX_EXPRPSSMXLT0(accel));
	if (exprpssmxlt & ADF_C4XXX_EXPRPSSM_FATAL_MASK) {
		device_printf(GET_DEV(accel_dev),
			      "Uncorrectable error XLT0 detected in accel %u\n",
			      accel);
		atomic_inc(&accel_dev->ras_counters[ADF_RAS_UNCORR]);
	}
	if (exprpssmxlt & ADF_C4XXX_EXPRPSSM_SOFT_MASK) {
		device_printf(GET_DEV(accel_dev),
			      "Correctable error XLT0 detected in accel %u\n",
			      accel);
		atomic_inc(&accel_dev->ras_counters[ADF_RAS_CORR]);
	}
	ADF_CSR_WR(pmisc, ADF_C4XXX_EXPRPSSMXLT0(accel), 0);

	/* XTL1 */
	exprpssmxlt = ADF_CSR_RD(pmisc, ADF_C4XXX_EXPRPSSMXLT1(accel));
	if (exprpssmxlt & ADF_C4XXX_EXPRPSSM_FATAL_MASK) {
		device_printf(GET_DEV(accel_dev),
			      "Uncorrectable error XLT1 detected in accel %u\n",
			      accel);
		atomic_inc(&accel_dev->ras_counters[ADF_RAS_UNCORR]);
	}
	if (exprpssmxlt & ADF_C4XXX_EXPRPSSM_SOFT_MASK) {
		device_printf(GET_DEV(accel_dev),
			      "Correctable error XLT1 detected in accel %u\n",
			      accel);
		atomic_inc(&accel_dev->ras_counters[ADF_RAS_CORR]);
	}
	ADF_CSR_WR(pmisc, ADF_C4XXX_EXPRPSSMXLT0(accel), 0);
}

static inline void
adf_process_spp_par_err(struct adf_accel_dev *accel_dev,
			struct resource *pmisc,
			u32 accel,
			bool *reset_required)
{
	/* All SPP parity errors are treated as uncorrectable fatal errors */
	atomic_inc(&accel_dev->ras_counters[ADF_RAS_FATAL]);
	*reset_required = true;
	device_printf(GET_DEV(accel_dev),
		      "Uncorrectable fatal SPP parity error detected\n");
}

static inline void
adf_process_statssm(struct adf_accel_dev *accel_dev,
		    struct resource *pmisc,
		    u32 accel,
		    bool *reset_required)
{
	u32 i;
	u32 statssm = ADF_CSR_RD(pmisc, ADF_INTSTATSSM(accel));
	u32 iastatssm = ADF_CSR_RD(pmisc, ADF_C4XXX_IAINTSTATSSM(accel));
	bool type;
	const unsigned long tmp_iastatssm = iastatssm;

	/* First collect all errors */
	for_each_set_bit(i, &tmp_iastatssm, ADF_C4XXX_IASTATSSM_BITS)
	{
		if (i == ADF_C4XXX_IASTATSSM_SLICE_HANG_ERR_BIT) {
			/* Slice Hang error is being handled in
			 * separate function adf_check_slice_hang_c4xxx(),
			 * which also increments RAS counters for
			 * SliceHang error.
			 */
			continue;
		}
		if (i == ADF_C4XXX_IASTATSSM_SPP_PAR_ERR_BIT) {
			adf_process_spp_par_err(accel_dev,
						pmisc,
						accel,
						reset_required);
			continue;
		}

		type = (i % 2) ? ADF_RAS_CORR : ADF_RAS_UNCORR;
		if (i == ADF_C4XXX_IASTATSSM_CPP_PAR_ERR_BIT)
			type = ADF_RAS_UNCORR;

		atomic_inc(&accel_dev->ras_counters[type]);
	}

	/* If iastatssm is set, we need to log the error */
	if (iastatssm & ADF_C4XXX_IASTATSSM_MASK)
		adf_log_source_iastatssm(accel_dev, pmisc, iastatssm, accel);
	/* If statssm is set, we need to clear the error sources */
	if (statssm & ADF_C4XXX_IASTATSSM_MASK)
		adf_clear_source_statssm(accel_dev, pmisc, statssm, accel);
	/* Clear the iastatssm after clearing error sources */
	if (iastatssm & ADF_C4XXX_IASTATSSM_MASK)
		adf_csr_fetch_and_and(pmisc,
				      ADF_C4XXX_IAINTSTATSSM(accel),
				      ADF_C4XXX_IASTATSSM_CLR_MASK);
}

static inline void
adf_process_errsou10(struct adf_accel_dev *accel_dev,
		     struct resource *pmisc,
		     u32 errsou,
		     u32 num_accels,
		     bool *reset_required)
{
	int accel;
	const unsigned long tmp_errsou = errsou;

	for_each_set_bit(accel, &tmp_errsou, num_accels)
	{
		adf_process_statssm(accel_dev, pmisc, accel, reset_required);
		adf_process_exprpssmcpr(accel_dev, pmisc, accel);
		adf_process_exprpssmxlt(accel_dev, pmisc, accel);
	}
}

/* ERRSOU 11 */
static inline void
adf_handle_ti_misc_err(struct adf_accel_dev *accel_dev, struct resource *pmisc)
{
	u32 ti_misc_sts = 0;
	u32 err_type = 0;

	ti_misc_sts = ADF_CSR_RD(pmisc, ADF_C4XXX_TI_MISC_STS);
	dev_dbg(GET_DEV(accel_dev), "ti_misc_sts = 0x%X\n", ti_misc_sts);

	if (ti_misc_sts & ADF_C4XXX_TI_MISC_ERR_MASK) {
		atomic_inc(&accel_dev->ras_counters[ADF_RAS_UNCORR]);

		/* If TI misc error occurred then check its type */
		err_type = ADF_C4XXX_GET_TI_MISC_ERR_TYPE(ti_misc_sts);
		if (err_type == ADF_C4XXX_TI_BME_RESP_ORDER_ERR) {
			device_printf(
			    GET_DEV(accel_dev),
			    "Uncorrectable non-fatal BME response order error.\n");

		} else if (err_type == ADF_C4XXX_TI_RESP_ORDER_ERR) {
			device_printf(
			    GET_DEV(accel_dev),
			    "Uncorrectable non-fatal response order error.\n");
		}

		/* Clear the interrupt and allow the next error to be
		 * logged.
		 */
		ADF_CSR_WR(pmisc, ADF_C4XXX_TI_MISC_STS, BIT(0));
	}
}

static inline void
adf_handle_ri_push_pull_par_err(struct adf_accel_dev *accel_dev,
				struct resource *pmisc)
{
	u32 ri_cpp_int_sts = 0;
	u32 err_clear_mask = 0;

	ri_cpp_int_sts = ADF_CSR_RD(pmisc, ADF_C4XXX_RI_CPP_INT_STS);
	dev_dbg(GET_DEV(accel_dev), "ri_cpp_int_sts = 0x%X\n", ri_cpp_int_sts);

	if (ri_cpp_int_sts & ADF_C4XXX_RI_CPP_INT_STS_PUSH_ERR) {
		atomic_inc(&accel_dev->ras_counters[ADF_RAS_UNCORR]);
		device_printf(
		    GET_DEV(accel_dev),
		    "CPP%d: Uncorrectable non-fatal RI push error detected.\n",
		    ADF_C4XXX_GET_CPP_BUS_FROM_STS(ri_cpp_int_sts));

		err_clear_mask |= ADF_C4XXX_RI_CPP_INT_STS_PUSH_ERR;
	}

	if (ri_cpp_int_sts & ADF_C4XXX_RI_CPP_INT_STS_PULL_ERR) {
		atomic_inc(&accel_dev->ras_counters[ADF_RAS_UNCORR]);
		device_printf(
		    GET_DEV(accel_dev),
		    "CPP%d: Uncorrectable non-fatal RI pull error detected.\n",
		    ADF_C4XXX_GET_CPP_BUS_FROM_STS(ri_cpp_int_sts));

		err_clear_mask |= ADF_C4XXX_RI_CPP_INT_STS_PULL_ERR;
	}

	/* Clear the interrupt for handled errors and allow the next error
	 * to be logged.
	 */
	ADF_CSR_WR(pmisc, ADF_C4XXX_RI_CPP_INT_STS, err_clear_mask);
}

static inline void
adf_handle_ti_push_pull_par_err(struct adf_accel_dev *accel_dev,
				struct resource *pmisc)
{
	u32 ti_cpp_int_sts = 0;
	u32 err_clear_mask = 0;

	ti_cpp_int_sts = ADF_CSR_RD(pmisc, ADF_C4XXX_TI_CPP_INT_STS);
	dev_dbg(GET_DEV(accel_dev), "ti_cpp_int_sts = 0x%X\n", ti_cpp_int_sts);

	if (ti_cpp_int_sts & ADF_C4XXX_TI_CPP_INT_STS_PUSH_ERR) {
		atomic_inc(&accel_dev->ras_counters[ADF_RAS_UNCORR]);
		device_printf(
		    GET_DEV(accel_dev),
		    "CPP%d: Uncorrectable non-fatal TI push error detected.\n",
		    ADF_C4XXX_GET_CPP_BUS_FROM_STS(ti_cpp_int_sts));

		err_clear_mask |= ADF_C4XXX_TI_CPP_INT_STS_PUSH_ERR;
	}

	if (ti_cpp_int_sts & ADF_C4XXX_TI_CPP_INT_STS_PULL_ERR) {
		atomic_inc(&accel_dev->ras_counters[ADF_RAS_UNCORR]);
		device_printf(
		    GET_DEV(accel_dev),
		    "CPP%d: Uncorrectable non-fatal TI pull error detected.\n",
		    ADF_C4XXX_GET_CPP_BUS_FROM_STS(ti_cpp_int_sts));

		err_clear_mask |= ADF_C4XXX_TI_CPP_INT_STS_PULL_ERR;
	}

	/* Clear the interrupt for handled errors and allow the next error
	 * to be logged.
	 */
	ADF_CSR_WR(pmisc, ADF_C4XXX_TI_CPP_INT_STS, err_clear_mask);
}

static inline void
adf_handle_aram_corr_err(struct adf_accel_dev *accel_dev,
			 struct resource *aram_base_addr)
{
	u32 aram_cerr = 0;

	aram_cerr = ADF_CSR_RD(aram_base_addr, ADF_C4XXX_ARAMCERR);
	dev_dbg(GET_DEV(accel_dev), "aram_cerr = 0x%X\n", aram_cerr);

	if (aram_cerr & ADF_C4XXX_ARAM_CORR_ERR_MASK) {
		atomic_inc(&accel_dev->ras_counters[ADF_RAS_CORR]);
		device_printf(GET_DEV(accel_dev),
			      "Correctable ARAM error detected.\n");
	}

	/* Clear correctable ARAM error interrupt. */
	ADF_C4XXX_CLEAR_CSR_BIT(aram_cerr, 0);
	ADF_CSR_WR(aram_base_addr, ADF_C4XXX_ARAMCERR, aram_cerr);
}

static inline void
adf_handle_aram_uncorr_err(struct adf_accel_dev *accel_dev,
			   struct resource *aram_base_addr)
{
	u32 aram_uerr = 0;

	aram_uerr = ADF_CSR_RD(aram_base_addr, ADF_C4XXX_ARAMUERR);
	dev_dbg(GET_DEV(accel_dev), "aram_uerr = 0x%X\n", aram_uerr);

	if (aram_uerr & ADF_C4XXX_ARAM_UNCORR_ERR_MASK) {
		atomic_inc(&accel_dev->ras_counters[ADF_RAS_UNCORR]);
		device_printf(GET_DEV(accel_dev),
			      "Uncorrectable non-fatal ARAM error detected.\n");
	}

	/* Clear uncorrectable ARAM error interrupt. */
	ADF_C4XXX_CLEAR_CSR_BIT(aram_uerr, 0);
	ADF_CSR_WR(aram_base_addr, ADF_C4XXX_ARAMUERR, aram_uerr);
}

static inline void
adf_handle_ti_pull_par_err(struct adf_accel_dev *accel_dev,
			   struct resource *pmisc)
{
	u32 ti_cpp_int_sts = 0;

	ti_cpp_int_sts = ADF_CSR_RD(pmisc, ADF_C4XXX_TI_CPP_INT_STS);
	dev_dbg(GET_DEV(accel_dev), "ti_cpp_int_sts = 0x%X\n", ti_cpp_int_sts);

	if (ti_cpp_int_sts & ADF_C4XXX_TI_CPP_INT_STS_PUSH_DATA_PAR_ERR) {
		atomic_inc(&accel_dev->ras_counters[ADF_RAS_UNCORR]);
		device_printf(
		    GET_DEV(accel_dev),
		    "CPP%d: Uncorrectable non-fatal TI pull data parity error detected.\n",
		    ADF_C4XXX_GET_CPP_BUS_FROM_STS(ti_cpp_int_sts));
	}

	/* Clear the interrupt and allow the next error to be logged. */
	ADF_CSR_WR(pmisc,
		   ADF_C4XXX_TI_CPP_INT_STS,
		   ADF_C4XXX_TI_CPP_INT_STS_PUSH_DATA_PAR_ERR);
}

static inline void
adf_handle_ri_push_par_err(struct adf_accel_dev *accel_dev,
			   struct resource *pmisc)
{
	u32 ri_cpp_int_sts = 0;

	ri_cpp_int_sts = ADF_CSR_RD(pmisc, ADF_C4XXX_RI_CPP_INT_STS);
	dev_dbg(GET_DEV(accel_dev), "ri_cpp_int_sts = 0x%X\n", ri_cpp_int_sts);

	if (ri_cpp_int_sts & ADF_C4XXX_RI_CPP_INT_STS_PUSH_DATA_PAR_ERR) {
		atomic_inc(&accel_dev->ras_counters[ADF_RAS_UNCORR]);
		device_printf(
		    GET_DEV(accel_dev),
		    "CPP%d: Uncorrectable non-fatal RI push data parity error detected.\n",
		    ADF_C4XXX_GET_CPP_BUS_FROM_STS(ri_cpp_int_sts));
	}

	/* Clear the interrupt and allow the next error to be logged. */
	ADF_CSR_WR(pmisc,
		   ADF_C4XXX_RI_CPP_INT_STS,
		   ADF_C4XXX_RI_CPP_INT_STS_PUSH_DATA_PAR_ERR);
}

static inline void
adf_log_inln_err(struct adf_accel_dev *accel_dev,
		 u32 offset,
		 u8 ras_type,
		 char *msg)
{
	if (ras_type >= ADF_RAS_ERRORS) {
		device_printf(GET_DEV(accel_dev),
			      "Invalid ras type %u\n",
			      ras_type);
		return;
	}

	if (offset == ADF_C4XXX_INLINE_INGRESS_OFFSET) {
		if (ras_type == ADF_RAS_CORR)
			dev_dbg(GET_DEV(accel_dev), "Detect ici %s\n", msg);
		else
			device_printf(GET_DEV(accel_dev),
				      "Detect ici %s\n",
				      msg);
	} else {
		if (ras_type == ADF_RAS_CORR)
			dev_dbg(GET_DEV(accel_dev), "Detect ice %s\n", msg);
		else
			device_printf(GET_DEV(accel_dev),
				      "Detect ice %s\n",
				      msg);
	}
	atomic_inc(&accel_dev->ras_counters[ras_type]);
}

static inline void
adf_handle_parser_uerr(struct adf_accel_dev *accel_dev,
		       struct resource *aram_base_addr,
		       u32 offset,
		       bool *reset_required)
{
	u32 reg_val = 0;

	reg_val = ADF_CSR_RD(aram_base_addr, ADF_C4XXX_IC_PARSER_UERR + offset);
	if (reg_val & ADF_C4XXX_PARSER_UERR_INTR) {
		/* Mask inten */
		reg_val &= ~ADF_C4XXX_PARSER_DESC_UERR_INTR_ENA;
		ADF_CSR_WR(aram_base_addr,
			   ADF_C4XXX_IC_PARSER_UERR + offset,
			   reg_val);

		/* Fatal error then increase RAS error counter
		 * and reset CPM
		 */
		adf_log_inln_err(accel_dev,
				 offset,
				 ADF_RAS_FATAL,
				 "parser uncorr fatal err");
		*reset_required = true;
	}
}

static inline void
adf_handle_mac_intr(struct adf_accel_dev *accel_dev,
		    struct resource *aram_base_addr,
		    u32 offset,
		    bool *reset_required)
{
	u64 reg_val;

	reg_val = ADF_CSR_RD64(aram_base_addr, ADF_C4XXX_MAC_IP + offset);

	/* Handle the MAC interrupts masked out in MAC_IM */
	if (reg_val & ADF_C4XXX_MAC_ERROR_TX_UNDERRUN)
		adf_log_inln_err(accel_dev,
				 offset,
				 ADF_RAS_CORR,
				 "err tx underrun");

	if (reg_val & ADF_C4XXX_MAC_ERROR_TX_FCS)
		adf_log_inln_err(accel_dev, offset, ADF_RAS_CORR, "err tx fcs");

	if (reg_val & ADF_C4XXX_MAC_ERROR_TX_DATA_CORRUPT)
		adf_log_inln_err(accel_dev,
				 offset,
				 ADF_RAS_CORR,
				 "err tx data corrupt");

	if (reg_val & ADF_C4XXX_MAC_ERROR_RX_OVERRUN) {
		*reset_required = true;
		adf_log_inln_err(accel_dev,
				 offset,
				 ADF_RAS_FATAL,
				 "err rx overrun fatal err");
	}

	if (reg_val & ADF_C4XXX_MAC_ERROR_RX_RUNT) {
		*reset_required = true;
		adf_log_inln_err(accel_dev,
				 offset,
				 ADF_RAS_FATAL,
				 "err rx runt fatal err");
	}

	if (reg_val & ADF_C4XXX_MAC_ERROR_RX_UNDERSIZE) {
		*reset_required = true;
		adf_log_inln_err(accel_dev,
				 offset,
				 ADF_RAS_FATAL,
				 "err rx undersize fatal err");
	}

	if (reg_val & ADF_C4XXX_MAC_ERROR_RX_JABBER) {
		*reset_required = true;
		adf_log_inln_err(accel_dev,
				 offset,
				 ADF_RAS_FATAL,
				 "err rx jabber fatal err");
	}

	if (reg_val & ADF_C4XXX_MAC_ERROR_RX_OVERSIZE) {
		*reset_required = true;
		adf_log_inln_err(accel_dev,
				 offset,
				 ADF_RAS_FATAL,
				 "err rx oversize fatal err");
	}

	if (reg_val & ADF_C4XXX_MAC_ERROR_RX_FCS)
		adf_log_inln_err(accel_dev, offset, ADF_RAS_CORR, "err rx fcs");

	if (reg_val & ADF_C4XXX_MAC_ERROR_RX_FRAME)
		adf_log_inln_err(accel_dev,
				 offset,
				 ADF_RAS_CORR,
				 "err rx frame");

	if (reg_val & ADF_C4XXX_MAC_ERROR_RX_CODE)
		adf_log_inln_err(accel_dev,
				 offset,
				 ADF_RAS_CORR,
				 "err rx code");

	if (reg_val & ADF_C4XXX_MAC_ERROR_RX_PREAMBLE)
		adf_log_inln_err(accel_dev,
				 offset,
				 ADF_RAS_CORR,
				 "err rx preamble");

	if (reg_val & ADF_C4XXX_MAC_RX_LINK_UP)
		adf_log_inln_err(accel_dev, offset, ADF_RAS_CORR, "rx link up");

	if (reg_val & ADF_C4XXX_MAC_INVALID_SPEED)
		adf_log_inln_err(accel_dev,
				 offset,
				 ADF_RAS_CORR,
				 "invalid speed");

	if (reg_val & ADF_C4XXX_MAC_PIA_RX_FIFO_OVERRUN) {
		*reset_required = true;
		adf_log_inln_err(accel_dev,
				 offset,
				 ADF_RAS_FATAL,
				 "pia rx fifo overrun fatal err");
	}

	if (reg_val & ADF_C4XXX_MAC_PIA_TX_FIFO_OVERRUN) {
		*reset_required = true;
		adf_log_inln_err(accel_dev,
				 offset,
				 ADF_RAS_FATAL,
				 "pia tx fifo overrun fatal err");
	}

	if (reg_val & ADF_C4XXX_MAC_PIA_TX_FIFO_UNDERRUN) {
		*reset_required = true;
		adf_log_inln_err(accel_dev,
				 offset,
				 ADF_RAS_FATAL,
				 "pia tx fifo underrun fatal err");
	}

	/* Clear the interrupt and allow the next error to be logged. */
	ADF_CSR_WR64(aram_base_addr, ADF_C4XXX_MAC_IP + offset, reg_val);
}

static inline bool
adf_handle_rf_par_err(struct adf_accel_dev *accel_dev,
		      struct resource *aram_base_addr,
		      u32 rf_par_addr,
		      u32 rf_par_msk,
		      u32 offset,
		      char *msg)
{
	u32 reg_val;
	unsigned long intr_status;
	int i;
	char strbuf[ADF_C4XXX_MAX_STR_LEN];

	/* Handle rf parity error */
	reg_val = ADF_CSR_RD(aram_base_addr, rf_par_addr + offset);
	intr_status = reg_val & rf_par_msk;
	if (intr_status) {
		for_each_set_bit(i, &intr_status, ADF_C4XXX_RF_PAR_ERR_BITS)
		{
			if (i % 2 == 0)
				snprintf(strbuf,
					 sizeof(strbuf),
					 "%s mul par %u uncorr fatal err",
					 msg,
					 RF_PAR_MUL_MAP(i));

			else
				snprintf(strbuf,
					 sizeof(strbuf),
					 "%s par %u uncorr fatal err",
					 msg,
					 RF_PAR_MAP(i));

			adf_log_inln_err(accel_dev,
					 offset,
					 ADF_RAS_FATAL,
					 strbuf);
		}

		/* Clear the interrupt and allow the next error to be logged. */
		ADF_CSR_WR(aram_base_addr, rf_par_addr + offset, reg_val);
		return true;
	}
	return false;
}

static inline void
adf_handle_cd_rf_par_err(struct adf_accel_dev *accel_dev,
			 struct resource *aram_base_addr,
			 u32 offset,
			 bool *reset_required)
{
	/* Handle reg_cd_rf_parity_err[1] */
	*reset_required |=
	    adf_handle_rf_par_err(accel_dev,
				  aram_base_addr,
				  ADF_C4XXX_IC_CD_RF_PARITY_ERR_1,
				  ADF_C4XXX_CD_RF_PAR_ERR_1_INTR,
				  offset,
				  "cd rf par[1]:") ?
	    true :
	    false;
}

static inline void
adf_handle_inln_rf_par_err(struct adf_accel_dev *accel_dev,
			   struct resource *aram_base_addr,
			   u32 offset,
			   bool *reset_required)
{
	/* Handle reg_inln_rf_parity_err[0] */
	*reset_required |=
	    adf_handle_rf_par_err(accel_dev,
				  aram_base_addr,
				  ADF_C4XXX_IC_INLN_RF_PARITY_ERR_0,
				  ADF_C4XXX_INLN_RF_PAR_ERR_0_INTR,
				  offset,
				  "inln rf par[0]:") ?
	    true :
	    false;

	/* Handle reg_inln_rf_parity_err[1] */
	*reset_required |=
	    adf_handle_rf_par_err(accel_dev,
				  aram_base_addr,
				  ADF_C4XXX_IC_INLN_RF_PARITY_ERR_1,
				  ADF_C4XXX_INLN_RF_PAR_ERR_1_INTR,
				  offset,
				  "inln rf par[1]:") ?
	    true :
	    false;

	/* Handle reg_inln_rf_parity_err[2] */
	*reset_required |=
	    adf_handle_rf_par_err(accel_dev,
				  aram_base_addr,
				  ADF_C4XXX_IC_INLN_RF_PARITY_ERR_2,
				  ADF_C4XXX_INLN_RF_PAR_ERR_2_INTR,
				  offset,
				  "inln rf par[2]:") ?
	    true :
	    false;

	/* Handle reg_inln_rf_parity_err[5] */
	*reset_required |=
	    adf_handle_rf_par_err(accel_dev,
				  aram_base_addr,
				  ADF_C4XXX_IC_INLN_RF_PARITY_ERR_5,
				  ADF_C4XXX_INLN_RF_PAR_ERR_5_INTR,
				  offset,
				  "inln rf par[5]:") ?
	    true :
	    false;
}

static inline void
adf_handle_congest_mngt_intr(struct adf_accel_dev *accel_dev,
			     struct resource *aram_base_addr,
			     u32 offset,
			     bool *reset_required)
{
	u32 reg_val;

	reg_val = ADF_CSR_RD(aram_base_addr,
			     ADF_C4XXX_IC_CONGESTION_MGMT_INT + offset);

	/* A mis-configuration of CPM, a mis-configuration of the Ethernet
	 * Complex or that the traffic profile has deviated from that for
	 * which the resources were configured
	 */
	if (reg_val & ADF_C4XXX_CONGESTION_MGMT_CTPB_GLOBAL_CROSSED) {
		adf_log_inln_err(
		    accel_dev,
		    offset,
		    ADF_RAS_FATAL,
		    "congestion mgmt ctpb global crossed fatal err");
		*reset_required = true;
	}

	if (reg_val & ADF_C4XXX_CONGESTION_MGMT_XOFF_CIRQ_OUT) {
		adf_log_inln_err(accel_dev,
				 offset,
				 ADF_RAS_CORR,
				 "congestion mgmt XOFF cirq out err");
	}

	if (reg_val & ADF_C4XXX_CONGESTION_MGMT_XOFF_CIRQ_IN) {
		adf_log_inln_err(accel_dev,
				 offset,
				 ADF_RAS_CORR,
				 "congestion mgmt XOFF cirq in err");
	}

	/* Clear the interrupt and allow the next error to be logged */
	ADF_CSR_WR(aram_base_addr,
		   ADF_C4XXX_IC_CONGESTION_MGMT_INT + offset,
		   reg_val);
}

static inline void
adf_handle_inline_intr(struct adf_accel_dev *accel_dev,
		       struct resource *aram_base_addr,
		       u32 csr_offset,
		       bool *reset_required)
{
	adf_handle_cd_rf_par_err(accel_dev,
				 aram_base_addr,
				 csr_offset,
				 reset_required);

	adf_handle_parser_uerr(accel_dev,
			       aram_base_addr,
			       csr_offset,
			       reset_required);

	adf_handle_inln_rf_par_err(accel_dev,
				   aram_base_addr,
				   csr_offset,
				   reset_required);

	adf_handle_congest_mngt_intr(accel_dev,
				     aram_base_addr,
				     csr_offset,
				     reset_required);

	adf_handle_mac_intr(accel_dev,
			    aram_base_addr,
			    csr_offset,
			    reset_required);
}

static inline void
adf_process_errsou11(struct adf_accel_dev *accel_dev,
		     struct resource *pmisc,
		     u32 errsou,
		     bool *reset_required)
{
	struct resource *aram_base_addr =
	    (&GET_BARS(accel_dev)[ADF_C4XXX_SRAM_BAR])->virt_addr;

	if (errsou & ADF_C4XXX_TI_MISC)
		adf_handle_ti_misc_err(accel_dev, pmisc);

	if (errsou & ADF_C4XXX_RI_PUSH_PULL_PAR_ERR)
		adf_handle_ri_push_pull_par_err(accel_dev, pmisc);

	if (errsou & ADF_C4XXX_TI_PUSH_PULL_PAR_ERR)
		adf_handle_ti_push_pull_par_err(accel_dev, pmisc);

	if (errsou & ADF_C4XXX_ARAM_CORR_ERR)
		adf_handle_aram_corr_err(accel_dev, aram_base_addr);

	if (errsou & ADF_C4XXX_ARAM_UNCORR_ERR)
		adf_handle_aram_uncorr_err(accel_dev, aram_base_addr);

	if (errsou & ADF_C4XXX_TI_PULL_PAR_ERR)
		adf_handle_ti_pull_par_err(accel_dev, pmisc);

	if (errsou & ADF_C4XXX_RI_PUSH_PAR_ERR)
		adf_handle_ri_push_par_err(accel_dev, pmisc);

	if (errsou & ADF_C4XXX_INLINE_INGRESS_INTR)
		adf_handle_inline_intr(accel_dev,
				       aram_base_addr,
				       ADF_C4XXX_INLINE_INGRESS_OFFSET,
				       reset_required);

	if (errsou & ADF_C4XXX_INLINE_EGRESS_INTR)
		adf_handle_inline_intr(accel_dev,
				       aram_base_addr,
				       ADF_C4XXX_INLINE_EGRESS_OFFSET,
				       reset_required);
}

bool
adf_ras_interrupts(struct adf_accel_dev *accel_dev, bool *reset_required)
{
	u32 errsou = 0;
	bool handled = false;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	u32 num_accels = hw_data->get_num_accels(hw_data);
	struct resource *pmisc =
	    (&GET_BARS(accel_dev)[ADF_C4XXX_PMISC_BAR])->virt_addr;

	if (unlikely(!reset_required)) {
		device_printf(GET_DEV(accel_dev),
			      "Invalid pointer reset_required\n");
		return false;
	}

	/* errsou8 */
	errsou = ADF_CSR_RD(pmisc, ADF_C4XXX_ERRSOU8);
	if (errsou & ADF_C4XXX_ERRSOU8_MECORR_MASK) {
		adf_process_errsou8(accel_dev, pmisc);
		handled = true;
	}

	/* errsou9 */
	errsou = ADF_CSR_RD(pmisc, ADF_C4XXX_ERRSOU9);
	if (errsou & ADF_C4XXX_ERRSOU9_ERROR_MASK) {
		adf_process_errsou9(accel_dev, pmisc, errsou, reset_required);
		handled = true;
	}

	/* errsou10 */
	errsou = ADF_CSR_RD(pmisc, ADF_C4XXX_ERRSOU10);
	if (errsou & ADF_C4XXX_ERRSOU10_RAS_MASK) {
		adf_process_errsou10(
		    accel_dev, pmisc, errsou, num_accels, reset_required);
		handled = true;
	}

	/* errsou11 */
	errsou = ADF_CSR_RD(pmisc, ADF_C4XXX_ERRSOU11);
	if (errsou & ADF_C4XXX_ERRSOU11_ERROR_MASK) {
		adf_process_errsou11(accel_dev, pmisc, errsou, reset_required);
		handled = true;
	}

	return handled;
}

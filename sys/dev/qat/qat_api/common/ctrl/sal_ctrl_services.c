/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
/**
 *****************************************************************************
 * @file sal_ctrl_services.c
 *
 * @ingroup SalCtrl
 *
 * @description
 *    This file contains the core of the service controller implementation.
 *
 *****************************************************************************/

/* QAT-API includes */
#include "cpa.h"
#include "cpa_cy_key.h"
#include "cpa_cy_ln.h"
#include "cpa_cy_dh.h"
#include "cpa_cy_dsa.h"
#include "cpa_cy_rsa.h"
#include "cpa_cy_ec.h"
#include "cpa_cy_prime.h"
#include "cpa_cy_sym.h"
#include "cpa_dc.h"

/* QAT utils includes */
#include "qat_utils.h"

/* ADF includes */
#include "icp_adf_init.h"
#include "icp_adf_transport.h"
#include "icp_accel_devices.h"
#include "icp_adf_cfg.h"
#include "icp_adf_init.h"
#include "icp_adf_accel_mgr.h"
#include "icp_adf_debug.h"

/* FW includes */
#include "icp_qat_fw_la.h"

/* SAL includes */
#include "lac_mem.h"
#include "lac_mem_pools.h"
#include "lac_list.h"
#include "lac_hooks.h"
#include "sal_string_parse.h"
#include "lac_common.h"
#include "lac_sal_types.h"
#include "lac_sal.h"
#include "lac_sal_ctrl.h"
#include "icp_sal_versions.h"

#define MAX_SUBSYSTEM_RETRY 64

static char *subsystem_name = "SAL";
/**< Name used by ADF to identify this component. */
static char *cy_dir_name = "cy";
static char *asym_dir_name = "asym";
static char *sym_dir_name = "sym";
static char *dc_dir_name = "dc";
/**< Stats dir names. */
static char *ver_file_name = "version";

static subservice_registation_handle_t sal_service_reg_handle;
/**< Data structure used by ADF to keep a reference to this component. */

/*
 * @ingroup SalCtrl
 * @description
 *      This function is used to parse the results from ADF
 *      in response to ServiceEnabled query.The results are
 *      semi-colon separated. Internally, the bitmask represented
 *      by the enabled_service is used to track which features are enabled.
 *
 * @context
 *      This functions is called from the SalCtrl_ServiceEventInit function.
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in] device              pointer to icp_accel_dev_t structure
 * @param[in] pEnabledServices    pointer to memory where enabled services will
 *                               be written.
 * @retval Status
 */
CpaStatus
SalCtrl_GetEnabledServices(icp_accel_dev_t *device, Cpa32U *pEnabledServices)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	char param_value[ADF_CFG_MAX_VAL_LEN_IN_BYTES] = { 0 };
	char *token = NULL;
	char *running = NULL;

	*pEnabledServices = 0;

	memset(param_value, 0, ADF_CFG_MAX_VAL_LEN_IN_BYTES);
	status = icp_adf_cfgGetParamValue(device,
					  LAC_CFG_SECTION_GENERAL,
					  "ServicesEnabled",
					  param_value);

	if (CPA_STATUS_SUCCESS == status) {
		running = param_value;

		token = strsep(&running, ";");

		while (NULL != token) {
			do {
				if (strncmp(token, "asym", strlen("asym")) ==
				    0) {
					*pEnabledServices |=
					    SAL_SERVICE_TYPE_CRYPTO_ASYM;
					break;
				}
				if (strncmp(token, "sym", strlen("sym")) == 0) {
					*pEnabledServices |=
					    SAL_SERVICE_TYPE_CRYPTO_SYM;
					break;
				}
				if (strncmp(token, "cy", strlen("cy")) == 0) {
					*pEnabledServices |=
					    SAL_SERVICE_TYPE_CRYPTO;
					break;
				}
				if (strncmp(token, "dc", strlen("dc")) == 0) {
					*pEnabledServices |=
					    SAL_SERVICE_TYPE_COMPRESSION;
					break;
				}

				QAT_UTILS_LOG(
				    "Error parsing enabled services from ADF.\n");
				return CPA_STATUS_FAIL;

			} while (0);
			token = strsep(&running, ";");
		}
	} else {
		QAT_UTILS_LOG("Failed to get enabled services from ADF.\n");
	}
	return status;
}

/*
 * @ingroup SalCtrl
 * @description
 *      This function is used to check whether a service is enabled
 *
 * @context
 *      This functions is called from the SalCtrl_ServiceEventInit function.
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * param[in] enabled_services    It is the bitmask for the enabled services
 * param[in] service    It is the service we want to check for
 */
CpaBoolean
SalCtrl_IsServiceEnabled(Cpa32U enabled_services, sal_service_type_t service)
{
	return (CpaBoolean)((enabled_services & (Cpa32U)(service)) != 0);
}

/*
 * @ingroup SalCtrl
 * @description
 *      This function is used to check whether enabled services has associated
 *      hardware capability support
 *
 * @context
 *      This functions is called from the SalCtrl_ServiceEventInit function.
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * param[in] device              A pointer to an icp_accel_dev_t
 * param[in] enabled_services    It is the bitmask for the enabled services
 */

CpaStatus
SalCtrl_GetSupportedServices(icp_accel_dev_t *device, Cpa32U enabled_services)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	Cpa32U capabilitiesMask = 0;

	status = icp_amgr_getAccelDevCapabilities(device, &capabilitiesMask);

	if (CPA_STATUS_SUCCESS == status) {
		if (SalCtrl_IsServiceEnabled(enabled_services,
					     SAL_SERVICE_TYPE_CRYPTO)) {
			if (!(capabilitiesMask &
			      ICP_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC) ||
			    !(capabilitiesMask &
			      ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC)) {
				QAT_UTILS_LOG(
				    "Device does not support Crypto service\n");
				status = CPA_STATUS_FAIL;
			}
		}
		if (SalCtrl_IsServiceEnabled(enabled_services,
					     SAL_SERVICE_TYPE_CRYPTO_ASYM)) {
			if (!(capabilitiesMask &
			      ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC)) {
				QAT_UTILS_LOG(
				    "Device does not support Asym service\n");
				status = CPA_STATUS_FAIL;
			}
		}
		if (SalCtrl_IsServiceEnabled(enabled_services,
					     SAL_SERVICE_TYPE_CRYPTO_SYM)) {
			if (!(capabilitiesMask &
			      ICP_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC)) {
				QAT_UTILS_LOG(
				    "Device does not support Sym service\n");
				status = CPA_STATUS_FAIL;
			}
		}
		if (SalCtrl_IsServiceEnabled(enabled_services,
					     SAL_SERVICE_TYPE_COMPRESSION)) {
			if (!(capabilitiesMask &
			      ICP_ACCEL_CAPABILITIES_COMPRESSION)) {
				QAT_UTILS_LOG(
				    "Device does not support Compression service.\n");
				status = CPA_STATUS_FAIL;
			}
		}
	}

	return status;
}

/*************************************************************************
 * @ingroup SalCtrl
 * @description
 *      This function is used to check if a service is supported
 *      on the device. The key difference between this and
 *      SalCtrl_GetSupportedServices() is that the latter treats it as
 *      an error if the service is unsupported.
 *
 * @context
 *      This can be called anywhere.
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * param[in] device
 * param[in] service    service or services to check
 *
 *************************************************************************/
CpaBoolean
SalCtrl_IsServiceSupported(icp_accel_dev_t *device,
			   sal_service_type_t service_to_check)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	Cpa32U capabilitiesMask = 0;
	CpaBoolean service_supported = CPA_TRUE;

	if (!(SalCtrl_IsServiceEnabled((Cpa32U)service_to_check,
				       SAL_SERVICE_TYPE_CRYPTO)) &&
	    !(SalCtrl_IsServiceEnabled((Cpa32U)service_to_check,
				       SAL_SERVICE_TYPE_CRYPTO_ASYM)) &&
	    !(SalCtrl_IsServiceEnabled((Cpa32U)service_to_check,
				       SAL_SERVICE_TYPE_CRYPTO_SYM)) &&
	    !(SalCtrl_IsServiceEnabled((Cpa32U)service_to_check,
				       SAL_SERVICE_TYPE_COMPRESSION))) {
		QAT_UTILS_LOG("Invalid service type\n");
		service_supported = CPA_FALSE;
	}

	status = icp_amgr_getAccelDevCapabilities(device, &capabilitiesMask);

	if (CPA_STATUS_SUCCESS != status) {
		QAT_UTILS_LOG("Can not get device capabilities.\n");
		return CPA_FALSE;
	}

	if (SalCtrl_IsServiceEnabled((Cpa32U)service_to_check,
				     SAL_SERVICE_TYPE_CRYPTO)) {
		if (!(capabilitiesMask &
		      ICP_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC) ||
		    !(capabilitiesMask &
		      ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC)) {
			QAT_UTILS_LOG(
			    "Device does not support Crypto service\n");
			service_supported = CPA_FALSE;
		}
	}
	if (SalCtrl_IsServiceEnabled((Cpa32U)service_to_check,
				     SAL_SERVICE_TYPE_CRYPTO_ASYM)) {
		if (!(capabilitiesMask &
		      ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC)) {
			QAT_UTILS_LOG("Device does not support Asym service\n");
			service_supported = CPA_FALSE;
		}
	}
	if (SalCtrl_IsServiceEnabled((Cpa32U)service_to_check,
				     SAL_SERVICE_TYPE_CRYPTO_SYM)) {
		if (!(capabilitiesMask &
		      ICP_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC)) {
			QAT_UTILS_LOG("Device does not support Sym service\n");
			service_supported = CPA_FALSE;
		}
	}
	if (SalCtrl_IsServiceEnabled((Cpa32U)service_to_check,
				     SAL_SERVICE_TYPE_COMPRESSION)) {
		if (!(capabilitiesMask & ICP_ACCEL_CAPABILITIES_COMPRESSION)) {
			QAT_UTILS_LOG(
			    "Device does not support Compression service.\n");
			service_supported = CPA_FALSE;
		}
	}

	return service_supported;
}

/*
 * @ingroup SalCtrl
 * @description
 *      This function is used to retrieve how many instances are
 *      to be configured for process specific service.
 *
 * @context
 *      This functions is called from the SalCtrl_ServiceEventInit function.
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in] device    A pointer to an icp_accel_dev_t
 * @param[in] key       Represents the parameter's name we want to query
 * @param[out] pCount   Pointer to memory where num instances will be stored
 * @retval status       returned status from ADF or _FAIL if number of instances
 *                      is out of range for the device.
 */
static CpaStatus
SalCtrl_GetInstanceCount(icp_accel_dev_t *device, char *key, Cpa32U *pCount)
{
	CpaStatus status = CPA_STATUS_FAIL;
	char param_value[ADF_CFG_MAX_VAL_LEN_IN_BYTES];

	memset(param_value, 0, ADF_CFG_MAX_VAL_LEN_IN_BYTES);
	status = icp_adf_cfgGetParamValue(device,
					  icpGetProcessName(),
					  key,
					  param_value);
	if (CPA_STATUS_SUCCESS == status) {
		*pCount =
		    (Cpa32U)(Sal_Strtoul(param_value, NULL, SAL_CFG_BASE_DEC));
		if (*pCount > SAL_MAX_NUM_INSTANCES_PER_DEV) {
			QAT_UTILS_LOG("Number of instances is out of range.\n");
			status = CPA_STATUS_FAIL;
		}
	}
	return status;
}

/**************************************************************************
 * @ingroup SalCtrl
 * @description
 *      This function calls the shutdown function on all the
 *      service instances.
 *      It also frees all service instance memory allocated at Init.
 *
 * @context
 *      This function is called from the SalCtrl_ServiceEventShutdown
 *      function.
 *
 * @assumptions
 *      params[in] should not be NULL
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in] device           An icp_accel_dev_t* type
 * @param[in] services         A pointer to the container of services
 * @param[in] dbg_dir          A pointer to the debug directory
 * @param[in] svc_type         The type of the service instance
 *
 ****************************************************************************/
static CpaStatus
SalCtrl_ServiceShutdown(icp_accel_dev_t *device,
			sal_list_t **services,
			debug_dir_info_t **debug_dir,
			sal_service_type_t svc_type)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	sal_list_t *dyn_service = NULL;
	sal_service_t *inst = NULL;

	/* Call Shutdown function for each service instance */
	SAL_FOR_EACH(*services, sal_service_t, device, shutdown, status);

	if (*debug_dir) {
		icp_adf_debugRemoveDir(*debug_dir);
		LAC_OS_FREE(*debug_dir);
		*debug_dir = NULL;
	}

	if (!icp_adf_is_dev_in_reset(device)) {
		dyn_service = *services;
		while (dyn_service) {
			inst = (sal_service_t *)SalList_getObject(dyn_service);
			if (CPA_TRUE == inst->is_dyn) {
				icp_adf_putDynInstance(device,
						       (adf_service_type_t)
							   svc_type,
						       inst->instance);
			}
			dyn_service = SalList_next(dyn_service);
		}
		/* Free Sal services controller memory */
		SalList_free(services);
	} else {
		sal_list_t *curr_element = NULL;
		sal_service_t *service = NULL;
		curr_element = *services;
		while (NULL != curr_element) {
			service =
			    (sal_service_t *)SalList_getObject(curr_element);
			service->state = SAL_SERVICE_STATE_RESTARTING;
			curr_element = SalList_next(curr_element);
		}
	}

	return status;
}

/*************************************************************************
 * @ingroup SalCtrl
 * @description
 *      This function is used to initialise the service instances.
 *      It allocates memory for service instances and invokes the
 *      Init function on them.
 *
 * @context
 *      This function is called from the SalCtrl_ServiceEventInit function.
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in] device           An icp_accel_dev_t* type
 * @param[in] services         A pointer to the container of services
 * @param[in] dbg_dir          A pointer to the debug directory
 * @param[in] dbg_dir_name     Name of the debug directory
 * @param[in] tail_list        SAL's list of services
 * @param[in] instance_count   Number of instances
 * @param[in] svc_type         The type of the service instance
 *
 *************************************************************************/
static CpaStatus
SalCtrl_ServiceInit(icp_accel_dev_t *device,
		    sal_list_t **services,
		    debug_dir_info_t **dbg_dir,
		    char *dbg_dir_name,
		    sal_list_t *tail_list,
		    Cpa32U instance_count,
		    sal_service_type_t svc_type)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	sal_service_t *pInst = NULL;
	Cpa32U i = 0;
	debug_dir_info_t *debug_dir = NULL;

	debug_dir = LAC_OS_MALLOC(sizeof(debug_dir_info_t));
	if (NULL == debug_dir) {
		QAT_UTILS_LOG("Failed to allocate memory for debug dir.\n");
		return CPA_STATUS_RESOURCE;
	}
	debug_dir->name = dbg_dir_name;
	debug_dir->parent = NULL;
	status = icp_adf_debugAddDir(device, debug_dir);
	if (CPA_STATUS_SUCCESS != status) {
		QAT_UTILS_LOG("Failed to add debug dir.\n");
		LAC_OS_FREE(debug_dir);
		debug_dir = NULL;
		return status;
	}

	if (!icp_adf_is_dev_in_reset(device)) {
		for (i = 0; i < instance_count; i++) {
			status = SalCtrl_ServiceCreate(svc_type, i, &pInst);
			if (CPA_STATUS_SUCCESS != status) {
				break;
			}
			pInst->debug_parent_dir = debug_dir;
			pInst->capabilitiesMask = device->accelCapabilitiesMask;
			status = SalList_add(services, &tail_list, pInst);
			if (CPA_STATUS_SUCCESS != status) {
				free(pInst, M_QAT);
			}
		}
	} else {
		sal_list_t *curr_element = *services;
		sal_service_t *service = NULL;
		while (NULL != curr_element) {
			service =
			    (sal_service_t *)SalList_getObject(curr_element);
			service->debug_parent_dir = debug_dir;

			if (CPA_TRUE == service->isInstanceStarted) {
				icp_qa_dev_get(device);
			}

			curr_element = SalList_next(curr_element);
		}
	}

	if (CPA_STATUS_SUCCESS != status) {
		QAT_UTILS_LOG("Failed to allocate all instances.\n");
		icp_adf_debugRemoveDir(debug_dir);
		LAC_OS_FREE(debug_dir);
		debug_dir = NULL;
		SalList_free(services);
		return status;
	}

	/* Call init function for each service instance */
	SAL_FOR_EACH(*services, sal_service_t, device, init, status);
	if (CPA_STATUS_SUCCESS != status) {
		QAT_UTILS_LOG("Failed to initialise all service instances.\n");
		/* shutdown all instances initialised before error */
		SAL_FOR_EACH_STATE(*services,
				   sal_service_t,
				   device,
				   shutdown,
				   SAL_SERVICE_STATE_INITIALIZED);
		icp_adf_debugRemoveDir(debug_dir);
		LAC_OS_FREE(debug_dir);
		debug_dir = NULL;
		SalList_free(services);
		return status;
	}
	/* initialize the debug directory for relevant service */
	*dbg_dir = debug_dir;

	return status;
}

/**************************************************************************
 * @ingroup SalCtrl
 * @description
 *      This function calls the start function on all the service instances.
 *
 * @context
 *      This function is called from the SalCtrl_ServiceEventStart function.
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in] device     An icp_accel_dev_t* type
 * @param[in] services   A pointer to the container of services
 *
 **************************************************************************/
static CpaStatus
SalCtrl_ServiceStart(icp_accel_dev_t *device, sal_list_t *services)
{
	CpaStatus status = CPA_STATUS_SUCCESS;

	/* Call Start function for each service instance */
	SAL_FOR_EACH(services, sal_service_t, device, start, status);
	if (CPA_STATUS_SUCCESS != status) {
		QAT_UTILS_LOG("Failed to start all instances.\n");
		/* stop all instances started before error */
		SAL_FOR_EACH_STATE(services,
				   sal_service_t,
				   device,
				   stop,
				   SAL_SERVICE_STATE_RUNNING);
		return status;
	}

	if (icp_adf_is_dev_in_reset(device)) {
		sal_list_t *curr_element = services;
		sal_service_t *service = NULL;
		while (NULL != curr_element) {
			service =
			    (sal_service_t *)SalList_getObject(curr_element);
			if (service->notification_cb) {
				service->notification_cb(
				    service,
				    service->cb_tag,
				    CPA_INSTANCE_EVENT_RESTARTED);
			}
			curr_element = SalList_next(curr_element);
		}
	}

	return status;
}

/****************************************************************************
 * @ingroup SalCtrl
 * @description
 *      This function calls the stop function on all the
 *      service instances.
 *
 * @context
 *      This function is called from the SalCtrl_ServiceEventStop function.
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in] device    An icp_accel_dev_t* type
 * @param[in] services   A pointer to the container of services
 *
 *************************************************************************/
static CpaStatus
SalCtrl_ServiceStop(icp_accel_dev_t *device, sal_list_t *services)
{
	CpaStatus status = CPA_STATUS_SUCCESS;

	/* Calling restarting functions */
	if (icp_adf_is_dev_in_reset(device)) {
		sal_list_t *curr_element = services;
		sal_service_t *service = NULL;
		while (NULL != curr_element) {
			service =
			    (sal_service_t *)SalList_getObject(curr_element);
			if (service->notification_cb) {
				service->notification_cb(
				    service,
				    service->cb_tag,
				    CPA_INSTANCE_EVENT_RESTARTING);
			}
			curr_element = SalList_next(curr_element);
		}
	}

	/* Call Stop function for each service instance */
	SAL_FOR_EACH(services, sal_service_t, device, stop, status);

	return status;
}

/*
 * @ingroup SalCtrl
 * @description
 *      This function is used to print hardware and software versions in proc
 *      filesystem entry via ADF Debug interface
 *
 * @context
 *    This functions is called from proc filesystem interface
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]  private_data    A pointer to a private data passed to the
 *                             function while adding a debug file.
 * @param[out] data            Pointer to a buffer where version information
 *                             needs to be printed to.
 * @param[in]  size            Size of a buffer pointed by data.
 * @param[in]  offset          Offset in a debug file
 *
 * @retval 0    This function always returns 0
 */
static int
SalCtrl_VersionDebug(void *private_data, char *data, int size, int offset)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	Cpa32U len = 0;
	icp_accel_dev_t *device = (icp_accel_dev_t *)private_data;
	char param_value[ADF_CFG_MAX_VAL_LEN_IN_BYTES] = { 0 };

	len += snprintf(
	    data + len,
	    size - len,
	    SEPARATOR BORDER
	    " Hardware and Software versions for device %d      " BORDER
	    "\n" SEPARATOR,
	    device->accelId);

	memset(param_value, 0, ADF_CFG_MAX_VAL_LEN_IN_BYTES);
	status = icp_adf_cfgGetParamValue(device,
					  LAC_CFG_SECTION_GENERAL,
					  ICP_CFG_HW_REV_ID_KEY,
					  param_value);
	LAC_CHECK_STATUS(status);

	len += snprintf(data + len,
			size - len,
			" Hardware Version:             %s %s \n",
			param_value,
			get_sku_info(device->sku));

	memset(param_value, 0, ADF_CFG_MAX_VAL_LEN_IN_BYTES);
	status = icp_adf_cfgGetParamValue(device,
					  LAC_CFG_SECTION_GENERAL,
					  ICP_CFG_UOF_VER_KEY,
					  param_value);
	LAC_CHECK_STATUS(status);

	len += snprintf(data + len,
			size - len,
			" Firmware Version:             %s \n",
			param_value);
	memset(param_value, 0, ADF_CFG_MAX_VAL_LEN_IN_BYTES);
	status = icp_adf_cfgGetParamValue(device,
					  LAC_CFG_SECTION_GENERAL,
					  ICP_CFG_MMP_VER_KEY,
					  param_value);
	LAC_CHECK_STATUS(status);

	len += snprintf(data + len,
			size - len,
			" MMP Version:                  %s \n",
			param_value);
	len += snprintf(data + len,
			size - len,
			" Driver Version:               %d.%d.%d \n",
			SAL_INFO2_DRIVER_SW_VERSION_MAJ_NUMBER,
			SAL_INFO2_DRIVER_SW_VERSION_MIN_NUMBER,
			SAL_INFO2_DRIVER_SW_VERSION_PATCH_NUMBER);

	memset(param_value, 0, ADF_CFG_MAX_VAL_LEN_IN_BYTES);
	status = icp_adf_cfgGetParamValue(device,
					  LAC_CFG_SECTION_GENERAL,
					  ICP_CFG_LO_COMPATIBLE_DRV_KEY,
					  param_value);
	LAC_CHECK_STATUS(status);

	len += snprintf(data + len,
			size - len,
			" Lowest Compatible Driver:     %s \n",
			param_value);

	len += snprintf(data + len,
			size - len,
			" QuickAssist API CY Version:   %d.%d \n",
			CPA_CY_API_VERSION_NUM_MAJOR,
			CPA_CY_API_VERSION_NUM_MINOR);
	len += snprintf(data + len,
			size - len,
			" QuickAssist API DC Version:   %d.%d \n",
			CPA_DC_API_VERSION_NUM_MAJOR,
			CPA_DC_API_VERSION_NUM_MINOR);

	len += snprintf(data + len, size - len, SEPARATOR);
	return 0;
}

/**************************************************************************
 * @ingroup SalCtrl
 * @description
 *       This function calls the shutdown function on all the service
 *       instances. It also frees all service instance memory
 *       allocated at Init.
 *
 * @context
 *      This function is called from the SalCtrl_ServiceEventHandler function.
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in] device             An icp_accel_dev_t* type
 * @param[in] enabled_services   Services enabled by user
 *
 ****************************************************************************/
static CpaStatus
SalCtrl_ServiceEventShutdown(icp_accel_dev_t *device, Cpa32U enabled_services)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	CpaStatus ret_status = CPA_STATUS_SUCCESS;
	sal_t *service_container = (sal_t *)device->pSalHandle;

	if (NULL == service_container) {
		QAT_UTILS_LOG("Private data is NULL\n");
		return CPA_STATUS_FATAL;
	}

	if (SalCtrl_IsServiceEnabled(enabled_services,
				     SAL_SERVICE_TYPE_CRYPTO)) {
		status =
		    SalCtrl_ServiceShutdown(device,
					    &service_container->crypto_services,
					    &service_container->cy_dir,
					    SAL_SERVICE_TYPE_CRYPTO);
		if (CPA_STATUS_SUCCESS != status) {
			ret_status = status;
		}
	}

	if (SalCtrl_IsServiceEnabled(enabled_services,
				     SAL_SERVICE_TYPE_CRYPTO_ASYM)) {
		status =
		    SalCtrl_ServiceShutdown(device,
					    &service_container->asym_services,
					    &service_container->asym_dir,
					    SAL_SERVICE_TYPE_CRYPTO_ASYM);
		if (CPA_STATUS_SUCCESS != status) {
			ret_status = status;
		}
	}

	if (SalCtrl_IsServiceEnabled(enabled_services,
				     SAL_SERVICE_TYPE_CRYPTO_SYM)) {
		status =
		    SalCtrl_ServiceShutdown(device,
					    &service_container->sym_services,
					    &service_container->sym_dir,
					    SAL_SERVICE_TYPE_CRYPTO_SYM);
		if (CPA_STATUS_SUCCESS != status) {
			ret_status = status;
		}
	}

	if (SalCtrl_IsServiceEnabled(enabled_services,
				     SAL_SERVICE_TYPE_COMPRESSION)) {
		status = SalCtrl_ServiceShutdown(
		    device,
		    &service_container->compression_services,
		    &service_container->dc_dir,
		    SAL_SERVICE_TYPE_COMPRESSION);
		if (CPA_STATUS_SUCCESS != status) {
			ret_status = status;
		}
	}

	if (service_container->ver_file) {
		icp_adf_debugRemoveFile(service_container->ver_file);
		LAC_OS_FREE(service_container->ver_file);
		service_container->ver_file = NULL;
	}

	if (!icp_adf_is_dev_in_reset(device)) {
		/* Free container also */
		free(service_container, M_QAT);
		device->pSalHandle = NULL;
	}

	return ret_status;
}

/*************************************************************************
 * @ingroup SalCtrl
 * @description
 *      This function is used to initialize the service instances.
 *      It first checks (via ADF query) which services are enabled in the
 *      system and the number of each services.
 *      It then invokes the init function on them which creates the
 *      instances and allocates memory for them.
 *
 * @context
 *      This function is called from the SalCtrl_ServiceEventHandler function.
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in] device             An icp_accel_dev_t* type
 * @param[in] enabled_services   Services enabled by user
 *
 *************************************************************************/
static CpaStatus
SalCtrl_ServiceEventInit(icp_accel_dev_t *device, Cpa32U enabled_services)
{
	sal_t *service_container = NULL;
	CpaStatus status = CPA_STATUS_SUCCESS;
	sal_list_t *tail_list = NULL;
	Cpa32U instance_count = 0;

	status = SalCtrl_GetSupportedServices(device, enabled_services);
	if (CPA_STATUS_SUCCESS != status) {
		QAT_UTILS_LOG("Failed to get supported services.\n");
		return status;
	}

	if (!icp_adf_is_dev_in_reset(device)) {
		service_container = malloc(sizeof(sal_t), M_QAT, M_WAITOK);
		device->pSalHandle = service_container;
		service_container->asym_services = NULL;
		service_container->sym_services = NULL;
		service_container->crypto_services = NULL;
		service_container->compression_services = NULL;
	} else {
		service_container = device->pSalHandle;
	}
	service_container->asym_dir = NULL;
	service_container->sym_dir = NULL;
	service_container->cy_dir = NULL;
	service_container->dc_dir = NULL;
	service_container->ver_file = NULL;

	service_container->ver_file = LAC_OS_MALLOC(sizeof(debug_file_info_t));
	if (NULL == service_container->ver_file) {
		free(service_container, M_QAT);
		return CPA_STATUS_RESOURCE;
	}

	memset(service_container->ver_file, 0, sizeof(debug_file_info_t));
	service_container->ver_file->name = ver_file_name;
	service_container->ver_file->seq_read = SalCtrl_VersionDebug;
	service_container->ver_file->private_data = device;
	service_container->ver_file->parent = NULL;

	status = icp_adf_debugAddFile(device, service_container->ver_file);
	if (CPA_STATUS_SUCCESS != status) {
		LAC_OS_FREE(service_container->ver_file);
		free(service_container, M_QAT);
		return status;
	}

	if (SalCtrl_IsServiceEnabled(enabled_services,
				     SAL_SERVICE_TYPE_CRYPTO_ASYM)) {
		status = SalCtrl_GetInstanceCount(device,
						  "NumberCyInstances",
						  &instance_count);
		if (CPA_STATUS_SUCCESS != status) {
			instance_count = 0;
		}
		status = SalCtrl_ServiceInit(device,
					     &service_container->asym_services,
					     &service_container->asym_dir,
					     asym_dir_name,
					     tail_list,
					     instance_count,
					     SAL_SERVICE_TYPE_CRYPTO_ASYM);
		if (CPA_STATUS_SUCCESS != status) {
			goto err_init;
		}
	}

	if (SalCtrl_IsServiceEnabled(enabled_services,
				     SAL_SERVICE_TYPE_CRYPTO_SYM)) {
		status = SalCtrl_GetInstanceCount(device,
						  "NumberCyInstances",
						  &instance_count);
		if (CPA_STATUS_SUCCESS != status) {
			instance_count = 0;
		}
		status = SalCtrl_ServiceInit(device,
					     &service_container->sym_services,
					     &service_container->sym_dir,
					     sym_dir_name,
					     tail_list,
					     instance_count,
					     SAL_SERVICE_TYPE_CRYPTO_SYM);
		if (CPA_STATUS_SUCCESS != status) {
			goto err_init;
		}
	}

	if (SalCtrl_IsServiceEnabled(enabled_services,
				     SAL_SERVICE_TYPE_CRYPTO)) {
		status = SalCtrl_GetInstanceCount(device,
						  "NumberCyInstances",
						  &instance_count);
		if (CPA_STATUS_SUCCESS != status) {
			instance_count = 0;
		}
		status =
		    SalCtrl_ServiceInit(device,
					&service_container->crypto_services,
					&service_container->cy_dir,
					cy_dir_name,
					tail_list,
					instance_count,
					SAL_SERVICE_TYPE_CRYPTO);
		if (CPA_STATUS_SUCCESS != status) {
			goto err_init;
		}
	}
	if (SalCtrl_IsServiceEnabled(enabled_services,
				     SAL_SERVICE_TYPE_COMPRESSION)) {
		status = SalCtrl_GetInstanceCount(device,
						  "NumberDcInstances",
						  &instance_count);
		if (CPA_STATUS_SUCCESS != status) {
			instance_count = 0;
		}
		status = SalCtrl_ServiceInit(
		    device,
		    &service_container->compression_services,
		    &service_container->dc_dir,
		    dc_dir_name,
		    tail_list,
		    instance_count,
		    SAL_SERVICE_TYPE_COMPRESSION);
		if (CPA_STATUS_SUCCESS != status) {
			goto err_init;
		}
	}

	return status;

err_init:
	SalCtrl_ServiceEventShutdown(device, enabled_services);
	return status;
}

/****************************************************************************
 * @ingroup SalCtrl
 * @description
 *      This function calls the stop function on all the service instances.
 *
 * @context
 *      This function is called from the SalCtrl_ServiceEventHandler function.
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in] device              An icp_accel_dev_t* type
 * @param[in] enabled_services    Enabled services by user
 *
 *************************************************************************/
static CpaStatus
SalCtrl_ServiceEventStop(icp_accel_dev_t *device, Cpa32U enabled_services)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	CpaStatus ret_status = CPA_STATUS_SUCCESS;
	sal_t *service_container = device->pSalHandle;

	if (service_container == NULL) {
		QAT_UTILS_LOG("Private data is NULL.\n");
		return CPA_STATUS_FATAL;
	}

	if (SalCtrl_IsServiceEnabled(enabled_services,
				     SAL_SERVICE_TYPE_CRYPTO_ASYM)) {
		status = SalCtrl_ServiceStop(device,
					     service_container->asym_services);
		if (CPA_STATUS_SUCCESS != status) {
			ret_status = status;
		}
	}

	if (SalCtrl_IsServiceEnabled(enabled_services,
				     SAL_SERVICE_TYPE_CRYPTO_SYM)) {
		status = SalCtrl_ServiceStop(device,
					     service_container->sym_services);
		if (CPA_STATUS_SUCCESS != status) {
			ret_status = status;
		}
	}

	if (SalCtrl_IsServiceEnabled(enabled_services,
				     SAL_SERVICE_TYPE_CRYPTO)) {
		status =
		    SalCtrl_ServiceStop(device,
					service_container->crypto_services);
		if (CPA_STATUS_SUCCESS != status) {
			ret_status = status;
		}
	}

	if (SalCtrl_IsServiceEnabled(enabled_services,
				     SAL_SERVICE_TYPE_COMPRESSION)) {
		status = SalCtrl_ServiceStop(
		    device, service_container->compression_services);
		if (CPA_STATUS_SUCCESS != status) {
			ret_status = status;
		}
	}

	return ret_status;
}

/**************************************************************************
 * @ingroup SalCtrl
 * @description
 *      This function calls the start function on all the service instances.
 *
 * @context
 *      This function is called from the SalCtrl_ServiceEventHandler function.
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in] device              An icp_accel_dev_t* type
 * @param[in] enabled_services    Enabled services by user
 *
 **************************************************************************/
static CpaStatus
SalCtrl_ServiceEventStart(icp_accel_dev_t *device, Cpa32U enabled_services)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	sal_t *service_container = device->pSalHandle;

	if (service_container == NULL) {
		QAT_UTILS_LOG("Private data is NULL.\n");
		return CPA_STATUS_FATAL;
	}

	if (SalCtrl_IsServiceEnabled(enabled_services,
				     SAL_SERVICE_TYPE_CRYPTO_ASYM)) {
		status = SalCtrl_ServiceStart(device,
					      service_container->asym_services);
		if (CPA_STATUS_SUCCESS != status) {
			goto err_start;
		}
	}

	if (SalCtrl_IsServiceEnabled(enabled_services,
				     SAL_SERVICE_TYPE_CRYPTO_SYM)) {
		status = SalCtrl_ServiceStart(device,
					      service_container->sym_services);
		if (CPA_STATUS_SUCCESS != status) {
			goto err_start;
		}
	}

	if (SalCtrl_IsServiceEnabled(enabled_services,
				     SAL_SERVICE_TYPE_CRYPTO)) {
		status =
		    SalCtrl_ServiceStart(device,
					 service_container->crypto_services);
		if (CPA_STATUS_SUCCESS != status) {
			goto err_start;
		}
	}

	if (SalCtrl_IsServiceEnabled(enabled_services,
				     SAL_SERVICE_TYPE_COMPRESSION)) {
		status = SalCtrl_ServiceStart(
		    device, service_container->compression_services);
		if (CPA_STATUS_SUCCESS != status) {
			goto err_start;
		}
	}

	return status;
err_start:
	SalCtrl_ServiceEventStop(device, enabled_services);
	return status;
}

/*************************************************************************
 * @ingroup SalCtrl
 * @description
 *      This function is the events handler registered with ADF
 *      for the QA API services (cy, dc) - kernel and user
 *
 * @context
 *      This function is called from an ADF context.
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]  device      An icp_accel_dev_t* type
 * @param[in]  event       Event from ADF
 * @param[in]  param       Parameter used for back compatibility
 *
 ***********************************************************************/
static CpaStatus
SalCtrl_ServiceEventHandler(icp_accel_dev_t *device,
			    icp_adf_subsystemEvent_t event,
			    void *param)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	CpaStatus stats_status = CPA_STATUS_SUCCESS;
	Cpa32U enabled_services = 0;

	status = SalCtrl_GetEnabledServices(device, &enabled_services);
	if (CPA_STATUS_SUCCESS != status) {
		QAT_UTILS_LOG("Failed to get enabled services.\n");
		return status;
	}

	switch (event) {
	case ICP_ADF_EVENT_INIT: {
		/* In case there is no QAT SAL needs to call InitStats */
		if (NULL == device->pQatStats) {
			status = SalStatistics_InitStatisticsCollection(device);
		}
		if (CPA_STATUS_SUCCESS != status) {
			return status;
		}

		status = SalCtrl_ServiceEventInit(device, enabled_services);
		break;
	}
	case ICP_ADF_EVENT_START: {
		status = SalCtrl_ServiceEventStart(device, enabled_services);
		break;
	}
	case ICP_ADF_EVENT_STOP: {
		status = SalCtrl_ServiceEventStop(device, enabled_services);
		break;
	}
	case ICP_ADF_EVENT_SHUTDOWN: {
		status = SalCtrl_ServiceEventShutdown(device, enabled_services);
		stats_status = SalStatistics_CleanStatisticsCollection(device);
		if (CPA_STATUS_SUCCESS != status ||
		    CPA_STATUS_SUCCESS != stats_status) {
			return CPA_STATUS_FAIL;
		}
		break;
	}
	default:
		status = CPA_STATUS_SUCCESS;
		break;
	}
	return status;
}

CpaStatus
SalCtrl_AdfServicesRegister(void)
{
	/* Fill out the global sal_service_reg_handle structure */
	sal_service_reg_handle.subserviceEventHandler =
	    SalCtrl_ServiceEventHandler;
	/* Set subsystem name to globally defined name */
	sal_service_reg_handle.subsystem_name = subsystem_name;

	return icp_adf_subsystemRegister(&sal_service_reg_handle);
}

CpaStatus
SalCtrl_AdfServicesUnregister(void)
{
	return icp_adf_subsystemUnregister(&sal_service_reg_handle);
}

CpaStatus
SalCtrl_AdfServicesStartedCheck(void)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	Cpa32U retry_num = 0;
	CpaBoolean state = CPA_FALSE;

	do {
		state = icp_adf_isSubsystemStarted(&sal_service_reg_handle);
		retry_num++;
	} while ((CPA_FALSE == state) && (retry_num < MAX_SUBSYSTEM_RETRY));

	if (CPA_FALSE == state) {
		QAT_UTILS_LOG("Sal Ctrl failed to start in given time.\n");
		status = CPA_STATUS_FAIL;
	}

	return status;
}

CpaStatus
validateConcurrRequest(Cpa32U numConcurrRequests)
{
	Cpa32U baseReq = SAL_64_CONCURR_REQUESTS;

	if (SAL_64_CONCURR_REQUESTS > numConcurrRequests) {
		QAT_UTILS_LOG(
		    "Invalid numConcurrRequests, it is less than min value.\n");
		return CPA_STATUS_FAIL;
	}

	while (SAL_MAX_CONCURR_REQUESTS >= baseReq) {
		if (baseReq != numConcurrRequests) {
			baseReq = baseReq << 1;
		} else {
			break;
		}
	}
	if (SAL_MAX_CONCURR_REQUESTS < baseReq) {
		QAT_UTILS_LOG(
		    "Invalid baseReg, it is greater than max value.\n");
		return CPA_STATUS_FAIL;
	}

	return CPA_STATUS_SUCCESS;
}

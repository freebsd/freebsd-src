/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2023, Intel Corporation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of the Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file ice_fw_logging.c
 * @brief firmware logging sysctls
 *
 * Contains sysctls to enable and configure firmware logging debug support.
 */

#include "ice_lib.h"
#include "ice_iflib.h"
#include <sys/queue.h>
#include <sys/sdt.h>

/*
 * SDT provider for DTrace probes related to firmware logging events
 */
SDT_PROVIDER_DEFINE(ice_fwlog);

/*
 * SDT DTrace probe fired when a firmware log message is received over the
 * AdminQ. It passes the buffer of the firwmare log message along with its
 * length in bytes to the DTrace framework.
 */
SDT_PROBE_DEFINE2(ice_fwlog, , , message, "uint8_t *", "int");

/*
 * Helper function prototypes
 */
static int ice_reconfig_fw_log(struct ice_softc *sc, struct ice_fwlog_cfg *cfg);

/*
 * dynamic sysctl handlers
 */
static int ice_sysctl_fwlog_set_cfg_options(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_fwlog_log_resolution(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_fwlog_register(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_fwlog_module_log_severity(SYSCTL_HANDLER_ARGS);

/**
 * ice_reconfig_fw_log - Re-program firmware logging configuration
 * @sc: private softc structure
 * @cfg: firmware log configuration to latch
 *
 * If the adminq is currently active, ask firmware to update the logging
 * configuration. If the adminq is currently down, then do nothing. In this
 * case, ice_init_hw() will re-configure firmware logging as soon as it brings
 * up the adminq.
 */
static int
ice_reconfig_fw_log(struct ice_softc *sc, struct ice_fwlog_cfg *cfg)
{
	enum ice_status status;

	ice_fwlog_init(&sc->hw, cfg);

	if (!ice_check_sq_alive(&sc->hw, &sc->hw.adminq))
		return (0);

	if (!ice_fwlog_supported(&sc->hw))
		return (0);

	status = ice_fwlog_set(&sc->hw, cfg);
	if (status) {
		device_printf(sc->dev,
		    "Failed to reconfigure firmware logging, err %s aq_err %s\n",
		    ice_status_str(status),
		    ice_aq_str(sc->hw.adminq.sq_last_status));
		return (ENODEV);
	}

	return (0);
}

#define ICE_SYSCTL_HELP_FWLOG_LOG_RESOLUTION				\
"\nControl firmware message limit to send per ARQ event"		\
"\t\nMin: 1"								\
"\t\nMax: 128"

#define ICE_SYSCTL_HELP_FWLOG_ARQ_ENA					\
"\nControl whether to enable/disable reporting to admin Rx queue"	\
"\n0 - Enable firmware reporting via ARQ"				\
"\n1 - Disable firmware reporting via ARQ"

#define ICE_SYSCTL_HELP_FWLOG_UART_ENA					\
"\nControl whether to enable/disable reporting to UART"			\
"\n0 - Enable firmware reporting via UART"				\
"\n1 - Disable firmware reporting via UART"

#define ICE_SYSCTL_HELP_FWLOG_ENABLE_ON_LOAD				\
"\nControl whether to enable logging during the attach phase"		\
"\n0 - Enable firmware logging during attach phase"			\
"\n1 - Disable firmware logging during attach phase"

#define ICE_SYSCTL_HELP_FWLOG_REGISTER					\
"\nControl whether to enable/disable firmware logging"			\
"\n0 - Enable firmware logging"						\
"\n1 - Disable firmware logging"

#define ICE_SYSCTL_HELP_FWLOG_MODULE_SEVERITY				\
"\nControl the level of log output messages for this module"		\
"\n\tverbose <4> - Verbose messages + (Error|Warning|Normal)"		\
"\n\tnormal  <3> - Normal messages  + (Error|Warning)"			\
"\n\twarning <2> - Warning messages + (Error)"				\
"\n\terror   <1> - Error messages"					\
"\n\tnone    <0> - Disables all logging for this module"

/**
 * ice_sysctl_fwlog_set_cfg_options - Sysctl for setting fwlog cfg options
 * @oidp: sysctl oid structure
 * @arg1: private softc structure
 * @arg2: option to adjust
 * @req: sysctl request pointer
 *
 * On read: displays whether firmware logging was reported during attachment
 * On write: enables/disables firmware logging during attach phase
 *
 * This has no effect on the legacy (V1) version of firmware logging.
 */
static int
ice_sysctl_fwlog_set_cfg_options(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_fwlog_cfg *cfg = &sc->hw.fwlog_cfg;
	int error;
	u16 option = (u16)arg2;
	bool enabled;

	enabled = !!(cfg->options & option);

	error = sysctl_handle_bool(oidp, &enabled, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);

	if (enabled)
		cfg->options |= option;
	else
		cfg->options &= ~option;

	return ice_reconfig_fw_log(sc, cfg);
}

/**
 * ice_sysctl_fwlog_log_resolution - Sysctl for setting log message resolution
 * @oidp: sysctl oid structure
 * @arg1: private softc structure
 * @arg2: __unused__
 * @req: sysctl request pointer
 *
 * On read: displays message queue limit before posting
 * On write: sets message queue limit before posting
 *
 * This has no effect on the legacy (V1) version of firmware logging.
 */
static int
ice_sysctl_fwlog_log_resolution(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_fwlog_cfg *cfg = &sc->hw.fwlog_cfg;
	int error;
	u8 resolution;

	UNREFERENCED_PARAMETER(arg2);

	resolution = cfg->log_resolution;

	error = sysctl_handle_8(oidp, &resolution, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);

	if ((resolution < ICE_AQC_FW_LOG_MIN_RESOLUTION) ||
	    (resolution > ICE_AQC_FW_LOG_MAX_RESOLUTION)) {
		device_printf(sc->dev, "Log resolution out-of-bounds\n");
		return (EINVAL);
	}

	cfg->log_resolution = resolution;

	return ice_reconfig_fw_log(sc, cfg);
}

/**
 * ice_sysctl_fwlog_register - Sysctl for (de)registering firmware logs
 * @oidp: sysctl oid structure
 * @arg1: private softc structure
 * @arg2: __unused__
 * @req: sysctl request pointer
 *
 * On read: displays whether firmware logging is registered
 * On write: (de)registers firmware logging.
 */
static int
ice_sysctl_fwlog_register(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_fwlog_cfg *cfg = &sc->hw.fwlog_cfg;
	enum ice_status status;
	int error;
	u8 enabled;

	UNREFERENCED_PARAMETER(arg2);

	if (ice_test_state(&sc->state, ICE_STATE_ATTACHING)) {
		device_printf(sc->dev, "Registering FW Logging via kenv is supported with the on_load option\n");
		return (EIO);
	}

	if (cfg->options & ICE_FWLOG_OPTION_IS_REGISTERED)
		enabled = true;
	else
		enabled = false;

	error = sysctl_handle_bool(oidp, &enabled, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);

	if (!ice_check_sq_alive(&sc->hw, &sc->hw.adminq))
		return (0);

	if (enabled) {
		status = ice_fwlog_register(&sc->hw);
		if (!status)
			ice_set_bit(ICE_FEATURE_FW_LOGGING, sc->feat_en);
	} else {
		status = ice_fwlog_unregister(&sc->hw);
		if (!status)
			ice_clear_bit(ICE_FEATURE_FW_LOGGING, sc->feat_en);
	}

	if (status)
		return (EIO);

	return (0);
}

/**
 * ice_sysctl_fwlog_module_log_severity - Add tunables for a FW logging module
 * @oidp: sysctl oid structure
 * @arg1: private softc structure
 * @arg2: index to logging module
 * @req: sysctl request pointer
 */
static int
ice_sysctl_fwlog_module_log_severity(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_fwlog_cfg *cfg = &sc->hw.fwlog_cfg;
	struct sbuf *sbuf;
	char *sev_str_end;
	enum ice_aqc_fw_logging_mod module = (enum ice_aqc_fw_logging_mod)arg2;
	int error, ll_num;
	u8 log_level;
	char sev_str[16];
	bool sev_set = false;

	log_level = cfg->module_entries[module].log_level;
	sbuf = sbuf_new(NULL, sev_str, sizeof(sev_str), SBUF_FIXEDLEN);
	sbuf_printf(sbuf, "%d<%s>", log_level, ice_log_sev_str(log_level));
	sbuf_finish(sbuf);
	sbuf_delete(sbuf);

	error = sysctl_handle_string(oidp, sev_str, sizeof(sev_str), req);
	if ((error) || (req->newptr == NULL))
		return (error);

	if (strcasecmp(ice_log_sev_str(ICE_FWLOG_LEVEL_VERBOSE), sev_str) == 0) {
		log_level = ICE_FWLOG_LEVEL_VERBOSE;
		sev_set = true;
	} else if (strcasecmp(ice_log_sev_str(ICE_FWLOG_LEVEL_NORMAL), sev_str) == 0) {
		log_level = ICE_FWLOG_LEVEL_NORMAL;
		sev_set = true;
	} else if (strcasecmp(ice_log_sev_str(ICE_FWLOG_LEVEL_WARNING), sev_str) == 0) {
		log_level = ICE_FWLOG_LEVEL_WARNING;
		sev_set = true;
	} else if (strcasecmp(ice_log_sev_str(ICE_FWLOG_LEVEL_ERROR), sev_str) == 0) {
		log_level = ICE_FWLOG_LEVEL_ERROR;
		sev_set = true;
	} else if (strcasecmp(ice_log_sev_str(ICE_FWLOG_LEVEL_NONE), sev_str) == 0) {
		log_level = ICE_FWLOG_LEVEL_NONE;
		sev_set = true;
	}

	if (!sev_set) {
		ll_num = strtol(sev_str, &sev_str_end, 0);
		if (sev_str_end == sev_str)
			ll_num = -1;
		if ((ll_num >= ICE_FWLOG_LEVEL_NONE) &&
		    (ll_num < ICE_FWLOG_LEVEL_INVALID))
			log_level = ll_num;
		else {
			device_printf(sc->dev,
			    "%s: \"%s\" is not a valid log level\n",
			    __func__, sev_str);
			return (EINVAL);
		}
	}

	cfg->module_entries[module].log_level = log_level;

	return ice_reconfig_fw_log(sc, cfg);
}

/**
 * ice_add_fw_logging_tunables - Add tunables to configure FW logging events
 * @sc: private softc structure
 * @parent: parent node to add the tunables under
 *
 * Add tunables for configuring the firmware logging support. This includes
 * a control to enable the logging, and controls for each module to configure
 * which events to receive.
 */
void
ice_add_fw_logging_tunables(struct ice_softc *sc, struct sysctl_oid *parent)
{
	struct sysctl_oid_list *parent_list, *fwlog_list, *module_list;
	struct sysctl_oid *fwlog_node, *module_node;
	struct sysctl_ctx_list *ctx;
	struct ice_hw *hw = &sc->hw;
	struct ice_fwlog_cfg *cfg;
	device_t dev = sc->dev;
	enum ice_aqc_fw_logging_mod module;
	u16 i;

	cfg = &hw->fwlog_cfg;
	ctx = device_get_sysctl_ctx(dev);
	parent_list = SYSCTL_CHILDREN(parent);

	fwlog_node = SYSCTL_ADD_NODE(ctx, parent_list, OID_AUTO, "fw_log",
				     ICE_CTLFLAG_DEBUG | CTLFLAG_RD, NULL,
				     "Firmware Logging");
	fwlog_list = SYSCTL_CHILDREN(fwlog_node);

	cfg->log_resolution = 10;
	SYSCTL_ADD_PROC(ctx, fwlog_list, OID_AUTO, "log_resolution",
	    ICE_CTLFLAG_DEBUG | CTLTYPE_U8 | CTLFLAG_RWTUN, sc,
	    0, ice_sysctl_fwlog_log_resolution,
	    "CU", ICE_SYSCTL_HELP_FWLOG_LOG_RESOLUTION);

	cfg->options |= ICE_FWLOG_OPTION_ARQ_ENA;
	SYSCTL_ADD_PROC(ctx, fwlog_list, OID_AUTO, "arq_en",
	    ICE_CTLFLAG_DEBUG | CTLTYPE_U8 | CTLFLAG_RWTUN, sc,
	    ICE_FWLOG_OPTION_ARQ_ENA, ice_sysctl_fwlog_set_cfg_options,
	    "CU", ICE_SYSCTL_HELP_FWLOG_ARQ_ENA);

	SYSCTL_ADD_PROC(ctx, fwlog_list, OID_AUTO, "uart_en",
	    ICE_CTLFLAG_DEBUG | CTLTYPE_U8 | CTLFLAG_RWTUN, sc,
	    ICE_FWLOG_OPTION_UART_ENA, ice_sysctl_fwlog_set_cfg_options,
	    "CU", ICE_SYSCTL_HELP_FWLOG_UART_ENA);

	SYSCTL_ADD_PROC(ctx, fwlog_list, OID_AUTO, "on_load",
	    ICE_CTLFLAG_DEBUG | CTLTYPE_U8 | CTLFLAG_RWTUN, sc,
	    ICE_FWLOG_OPTION_REGISTER_ON_INIT, ice_sysctl_fwlog_set_cfg_options,
	    "CU", ICE_SYSCTL_HELP_FWLOG_ENABLE_ON_LOAD);

	SYSCTL_ADD_PROC(ctx, fwlog_list, OID_AUTO, "register",
	    ICE_CTLFLAG_DEBUG | CTLTYPE_U8 | CTLFLAG_RWTUN, sc,
	    0, ice_sysctl_fwlog_register,
	    "CU", ICE_SYSCTL_HELP_FWLOG_REGISTER);

	module_node = SYSCTL_ADD_NODE(ctx, fwlog_list, OID_AUTO, "severity",
				      ICE_CTLFLAG_DEBUG | CTLFLAG_RD, NULL,
				      "Level of log output");

	module_list = SYSCTL_CHILDREN(module_node);

	for (i = 0; i < ICE_AQC_FW_LOG_ID_MAX; i++) {
		/* Setup some defaults */
		cfg->module_entries[i].module_id = i;
		cfg->module_entries[i].log_level = ICE_FWLOG_LEVEL_NONE;
		module = (enum ice_aqc_fw_logging_mod)i;

		SYSCTL_ADD_PROC(ctx, module_list,
		    OID_AUTO, ice_fw_module_str(module),
		    ICE_CTLFLAG_DEBUG | CTLTYPE_STRING | CTLFLAG_RWTUN, sc,
		    module, ice_sysctl_fwlog_module_log_severity,
		    "A", ICE_SYSCTL_HELP_FWLOG_MODULE_SEVERITY);
	}
}

/**
 * ice_handle_fw_log_event - Handle a firmware logging event from the AdminQ
 * @sc: pointer to private softc structure
 * @desc: the AdminQ descriptor for this firmware event
 * @buf: pointer to the buffer accompanying the AQ message
 */
void
ice_handle_fw_log_event(struct ice_softc *sc, struct ice_aq_desc *desc,
			void *buf)
{
	/* Trigger a DTrace probe event for this firmware message */
	SDT_PROBE2(ice_fwlog, , , message, (const u8 *)buf, desc->datalen);

	/* Possibly dump the firmware message to the console, if enabled */
	ice_fwlog_event_dump(&sc->hw, desc, buf);
}

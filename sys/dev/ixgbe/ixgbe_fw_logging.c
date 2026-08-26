/**
 * @file ixgbe_fw_logging.c
 * @brief firmware logging sysctls
 *
 * Contains sysctls to enable and configure firmware logging debug support.
 */

 #include "ixgbe.h"
 
 /**
  * ixgbe_reconfig_fw_log - Re-program firmware logging configuration
  * @sc: private softc structure
  * @cfg: firmware log configuration to latch
  *
  * If the adminq is currently active, ask firmware to update the logging
  * configuration. If the adminq is currently down, then do nothing. In this
  * case, ixgbe_init_hw() will re-configure firmware logging as soon as it brings
  * up the adminq.
  */
 static int
 ixgbe_reconfig_fw_log(struct ixgbe_softc *sc, struct ixgbe_fwlog_cfg *cfg)
 {
         int status;
 
         ixgbe_fwlog_init(&sc->hw, cfg);
 
         if (!ixgbe_fwlog_supported(&sc->hw))
                 return (0);
 
         status = ixgbe_fwlog_set(&sc->hw, cfg);
         if (status != IXGBE_SUCCESS) {
                 DEBUGOUT1("Failed to reconfigure firmware logging, status %d\n",
                     status);
                 return (ENODEV);
         }
 
         return (0);
 }
 
 /**
  * ixgbe_sysctl_fwlog_set_cfg_options - Sysctl for setting fwlog cfg options
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
 ixgbe_sysctl_fwlog_set_cfg_options(SYSCTL_HANDLER_ARGS)
 {
         struct ixgbe_softc *sc = (struct ixgbe_softc *)arg1;
         struct ixgbe_fwlog_cfg *cfg = &sc->hw.fwlog_cfg;
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
 
         return ixgbe_reconfig_fw_log(sc, cfg);
 }
 
 /**
  * ixgbe_sysctl_fwlog_log_resolution - Sysctl for setting log message resolution
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
 ixgbe_sysctl_fwlog_log_resolution(SYSCTL_HANDLER_ARGS)
 {
         struct ixgbe_softc *sc = (struct ixgbe_softc *)arg1;
         struct ixgbe_fwlog_cfg *cfg = &sc->hw.fwlog_cfg;
         int error;
         u8 resolution;
 
         UNREFERENCED_PARAMETER(arg2);
 
         resolution = cfg->log_resolution;
 
         error = sysctl_handle_8(oidp, &resolution, 0, req);
         if ((error) || (req->newptr == NULL))
                 return (error);
 
         if ((resolution < IXGBE_ACI_FW_LOG_MIN_RESOLUTION) ||
             (resolution > IXGBE_ACI_FW_LOG_MAX_RESOLUTION)) {
                 DEBUGOUT("Log resolution out-of-bounds\n");
                 return (EINVAL);
         }
 
         cfg->log_resolution = resolution;
 
         return ixgbe_reconfig_fw_log(sc, cfg);
 }
 
 /**
  * ixgbe_sysctl_fwlog_register - Sysctl for (de)registering firmware logs
  * @oidp: sysctl oid structure
  * @arg1: private softc structure
  * @arg2: __unused__
  * @req: sysctl request pointer
  *
  * On read: displays whether firmware logging is registered
  * On write: (de)registers firmware logging.
  */
 static int
 ixgbe_sysctl_fwlog_register(SYSCTL_HANDLER_ARGS)
 {
         struct ixgbe_softc *sc = (struct ixgbe_softc *)arg1;
         struct ixgbe_fwlog_cfg *cfg = &sc->hw.fwlog_cfg;
         int status;
         int error;
         u8 enabled;
 
         UNREFERENCED_PARAMETER(arg2);
 
         if (cfg->options & IXGBE_FWLOG_OPTION_IS_REGISTERED)
                 enabled = true;
         else
                 enabled = false;
 
         error = sysctl_handle_bool(oidp, &enabled, 0, req);
         if ((error) || (req->newptr == NULL))
                 return (error);
 
         if (enabled) {
                 status = ixgbe_fwlog_register(&sc->hw);
                 if (status == IXGBE_SUCCESS)
                         sc->feat_en |= IXGBE_FEATURE_FW_LOGGING;
         } else {
                 status = ixgbe_fwlog_unregister(&sc->hw);
                 if (status == IXGBE_SUCCESS)
                         sc->feat_en &= ~IXGBE_FEATURE_FW_LOGGING;
         }
 
         if (status != IXGBE_SUCCESS)
                 return (EIO);
 
         return (0);
 }
 
 /**
  * ixgbe_log_sev_str - Convert log level to a string
  * @log_level: the log level to convert
  *
  * Convert the u8 log level of a FW logging module into a readable
  * string for outputting in a sysctl.
  */
 struct ixgbe_str_buf {
         char str[IXGBE_STR_BUF_LEN];
 };
 
 static struct ixgbe_str_buf
 _ixgbe_log_sev_str(u8 log_level)
 {
         struct ixgbe_str_buf buf = { .str = "" };
         const char *str = NULL;
 
         switch (log_level) {
         case IXGBE_FWLOG_LEVEL_NONE:
                 str = "none";
                 break;
         case IXGBE_FWLOG_LEVEL_ERROR:
                 str = "error";
                 break;
         case IXGBE_FWLOG_LEVEL_WARNING:
                 str = "warning";
                 break;
         case IXGBE_FWLOG_LEVEL_NORMAL:
                 str = "normal";
                 break;
         case IXGBE_FWLOG_LEVEL_VERBOSE:
                 str = "verbose";
                 break;
         default:
                 break;
         }
 
         if (str)
                 snprintf(buf.str, IXGBE_STR_BUF_LEN, "%s", str);
         else
                 snprintf(buf.str, IXGBE_STR_BUF_LEN, "%u", log_level);
 
         return buf;
 }
 
 #define ixgbe_log_sev_str(log_level) _ixgbe_log_sev_str(log_level).str
 
 /**
  * ixgbe_sysctl_fwlog_module_log_severity - Add tunables for a FW logging module
  * @oidp: sysctl oid structure
  * @arg1: private softc structure
  * @arg2: index to logging module
  * @req: sysctl request pointer
  */
 static int
 ixgbe_sysctl_fwlog_module_log_severity(SYSCTL_HANDLER_ARGS)
 {
         struct ixgbe_softc *sc = (struct ixgbe_softc *)arg1;
         struct ixgbe_fwlog_cfg *cfg = &sc->hw.fwlog_cfg;
         struct sbuf *sbuf;
         char *sev_str_end;
         enum ixgbe_aci_fw_logging_mod module = (enum ixgbe_aci_fw_logging_mod)arg2;
         int error, ll_num;
         u8 log_level;
         char sev_str[16];
         bool sev_set = false;
 
         log_level = cfg->module_entries[module].log_level;
         sbuf = sbuf_new(NULL, sev_str, sizeof(sev_str), SBUF_FIXEDLEN);
         sbuf_printf(sbuf, "%d<%s>", log_level, ixgbe_log_sev_str(log_level));
         sbuf_finish(sbuf);
         sbuf_delete(sbuf);
 
         error = sysctl_handle_string(oidp, sev_str, sizeof(sev_str), req);
         if ((error) || (req->newptr == NULL))
                 return (error);
 
         if (strcasecmp(ixgbe_log_sev_str(IXGBE_FWLOG_LEVEL_VERBOSE), sev_str) == 0) {
                 log_level = IXGBE_FWLOG_LEVEL_VERBOSE;
                 sev_set = true;
         } else if (strcasecmp(ixgbe_log_sev_str(IXGBE_FWLOG_LEVEL_NORMAL), sev_str) == 0) {
                 log_level = IXGBE_FWLOG_LEVEL_NORMAL;
                 sev_set = true;
         } else if (strcasecmp(ixgbe_log_sev_str(IXGBE_FWLOG_LEVEL_WARNING), sev_str) == 0) {
                 log_level = IXGBE_FWLOG_LEVEL_WARNING;
                 sev_set = true;
         } else if (strcasecmp(ixgbe_log_sev_str(IXGBE_FWLOG_LEVEL_ERROR), sev_str) == 0) {
                 log_level = IXGBE_FWLOG_LEVEL_ERROR;
                 sev_set = true;
         } else if (strcasecmp(ixgbe_log_sev_str(IXGBE_FWLOG_LEVEL_NONE), sev_str) == 0) {
                 log_level = IXGBE_FWLOG_LEVEL_NONE;
                 sev_set = true;
         }
 
         if (!sev_set) {
                 ll_num = strtol(sev_str, &sev_str_end, 0);
                 if (sev_str_end == sev_str)
                         ll_num = -1;
                 if ((ll_num >= IXGBE_FWLOG_LEVEL_NONE) &&
                     (ll_num < IXGBE_FWLOG_LEVEL_INVALID))
                         log_level = ll_num;
                 else {
                         DEBUGOUT2("%s: \"%s\" is not a valid log level\n",
                             __func__, sev_str);
                         return (EINVAL);
                 }
         }
 
         cfg->module_entries[module].log_level = log_level;
 
         return ixgbe_reconfig_fw_log(sc, cfg);
 }
 
 #define IXGBE_SYSCTL_HELP_FWLOG_LOG_RESOLUTION				\
 "\nControl firmware message limit to send per ARQ event"		\
 "\t\nMin: 1"								\
 "\t\nMax: 128"
 
 #define IXGBE_SYSCTL_HELP_FWLOG_ARQ_ENA					\
 "\nControl whether to enable/disable reporting to admin Rx queue"	\
 "\n1 - Enable firmware reporting via ARQ"				\
 "\n0 - Disable firmware reporting via ARQ"
 
 #define IXGBE_SYSCTL_HELP_FWLOG_UART_ENA					\
 "\nControl whether to enable/disable reporting to UART"			\
 "\n1 - Enable firmware reporting via UART"				\
 "\n0 - Disable firmware reporting via UART"
 
 #define IXGBE_SYSCTL_HELP_FWLOG_ENABLE_ON_LOAD				\
 "\nControl whether to enable logging during the attach phase"		\
 "\n1 - Enable firmware logging during attach phase"			\
 "\n0 - Disable firmware logging during attach phase"
 
 #define IXGBE_SYSCTL_HELP_FWLOG_REGISTER					\
 "\nControl whether to enable/disable firmware logging"			\
 "\n1 - Enable firmware logging"						\
 "\n0 - Disable firmware logging"
 
 #define IXGBE_SYSCTL_HELP_FWLOG_MODULE_SEVERITY				\
 "\nControl the level of log output messages for this module"		\
 "\n\tverbose <4> - Verbose messages + (Error|Warning|Normal)"		\
 "\n\tnormal  <3> - Normal messages  + (Error|Warning)"			\
 "\n\twarning <2> - Warning messages + (Error)"				\
 "\n\terror   <1> - Error messages"					\
 "\n\tnone    <0> - Disables all logging for this module"
 
 /**
  * ixgbe_fw_module_str - Convert a FW logging module to a string name
  * @module: the module to convert
  *
  * Given a FW logging module id, convert it to a shorthand human readable
  * name, for generating sysctl tunables.
  */
 static const char *
 ixgbe_fw_module_str(enum ixgbe_aci_fw_logging_mod module)
 {
         switch (module) {
         case IXGBE_ACI_FW_LOG_ID_GENERAL:
                 return "general";
         case IXGBE_ACI_FW_LOG_ID_CTRL:
                 return "ctrl";
         case IXGBE_ACI_FW_LOG_ID_LINK:
                 return "link";
         case IXGBE_ACI_FW_LOG_ID_LINK_TOPO:
                 return "link_topo";
         case IXGBE_ACI_FW_LOG_ID_DNL:
                 return "dnl";
         case IXGBE_ACI_FW_LOG_ID_I2C:
                 return "i2c";
         case IXGBE_ACI_FW_LOG_ID_SDP:
                 return "sdp";
         case IXGBE_ACI_FW_LOG_ID_MDIO:
                 return "mdio";
         case IXGBE_ACI_FW_LOG_ID_ADMINQ:
                 return "adminq";
         case IXGBE_ACI_FW_LOG_ID_HDMA:
                 return "hdma";
         case IXGBE_ACI_FW_LOG_ID_LLDP:
                 return "lldp";
         case IXGBE_ACI_FW_LOG_ID_DCBX:
                 return "dcbx";
         case IXGBE_ACI_FW_LOG_ID_DCB:
                 return "dcb";
         case IXGBE_ACI_FW_LOG_ID_XLR:
                 return "xlr";
         case IXGBE_ACI_FW_LOG_ID_NVM:
                 return "nvm";
         case IXGBE_ACI_FW_LOG_ID_AUTH:
                 return "auth";
         case IXGBE_ACI_FW_LOG_ID_VPD:
                 return "vpd";
         case IXGBE_ACI_FW_LOG_ID_IOSF:
                 return "iosf";
         case IXGBE_ACI_FW_LOG_ID_PARSER:
                 return "parser";
         case IXGBE_ACI_FW_LOG_ID_SW:
                 return "sw";
         case IXGBE_ACI_FW_LOG_ID_SCHEDULER:
                 return "scheduler";
         case IXGBE_ACI_FW_LOG_ID_TXQ:
                 return "txq";
         case IXGBE_ACI_FW_LOG_ID_ACL:
                 return "acl";
         case IXGBE_ACI_FW_LOG_ID_POST:
                 return "post";
         case IXGBE_ACI_FW_LOG_ID_WATCHDOG:
                 return "watchdog";
         case IXGBE_ACI_FW_LOG_ID_TASK_DISPATCH:
                 return "task_dispatch";
         case IXGBE_ACI_FW_LOG_ID_MNG:
                 return "mng";
         case IXGBE_ACI_FW_LOG_ID_SYNCE:
                 return "synce";
         case IXGBE_ACI_FW_LOG_ID_HEALTH:
                 return "health";
         case IXGBE_ACI_FW_LOG_ID_TSDRV:
                 return "tsdrv";
         case IXGBE_ACI_FW_LOG_ID_PFREG:
                 return "pfreg";
         case IXGBE_ACI_FW_LOG_ID_MDLVER:
                 return "mdlver";
         case IXGBE_ACI_FW_LOG_ID_MAX:
                 return "unknown";
         }
 
         /* The compiler generates errors on unhandled enum values if we omit
          * the default case.
          */
         return "unknown";
 }
 
 /**
  * ixgbe_add_fw_logging_tunables - Add tunables to configure FW logging events
  * @sc: private softc structure
  * @parent: parent node to add the tunables under
  *
  * Add tunables for configuring the firmware logging support. This includes
  * a control to enable the logging, and controls for each module to configure
  * which events to receive.
  */
 void
 ixgbe_add_fw_logging_tunables(struct ixgbe_softc *sc, struct sysctl_oid *parent)
 {
         struct sysctl_oid_list *parent_list, *fwlog_list, *module_list;
         struct sysctl_oid *fwlog_node, *module_node;
         struct sysctl_ctx_list *ctx;
         struct ixgbe_hw *hw = &sc->hw;
         struct ixgbe_fwlog_cfg *cfg;
         device_t dev = sc->dev;
         enum ixgbe_aci_fw_logging_mod module;
         u16 i;
 
         cfg = &hw->fwlog_cfg;
         ctx = device_get_sysctl_ctx(dev);
         parent_list = SYSCTL_CHILDREN(parent);
 
         fwlog_node = SYSCTL_ADD_NODE(ctx, parent_list, OID_AUTO, "fw_log",
                                      CTLFLAG_RD, NULL,
                                      "Firmware Logging");
         fwlog_list = SYSCTL_CHILDREN(fwlog_node);
 
         cfg->log_resolution = 10;
         SYSCTL_ADD_PROC(ctx, fwlog_list, OID_AUTO, "log_resolution",
             CTLTYPE_U8 | CTLFLAG_RWTUN, sc,
             0, ixgbe_sysctl_fwlog_log_resolution,
             "CU", IXGBE_SYSCTL_HELP_FWLOG_LOG_RESOLUTION);
 
         cfg->options |= IXGBE_FWLOG_OPTION_ARQ_ENA;
         SYSCTL_ADD_PROC(ctx, fwlog_list, OID_AUTO, "arq_en",
             CTLTYPE_U8 | CTLFLAG_RWTUN, sc,
             IXGBE_FWLOG_OPTION_ARQ_ENA, ixgbe_sysctl_fwlog_set_cfg_options,
             "CU", IXGBE_SYSCTL_HELP_FWLOG_ARQ_ENA);
 
         SYSCTL_ADD_PROC(ctx, fwlog_list, OID_AUTO, "uart_en",
             CTLTYPE_U8 | CTLFLAG_RWTUN, sc,
             IXGBE_FWLOG_OPTION_UART_ENA, ixgbe_sysctl_fwlog_set_cfg_options,
             "CU", IXGBE_SYSCTL_HELP_FWLOG_UART_ENA);
 
         SYSCTL_ADD_PROC(ctx, fwlog_list, OID_AUTO, "on_load",
             CTLTYPE_U8 | CTLFLAG_RWTUN, sc,
             IXGBE_FWLOG_OPTION_REGISTER_ON_INIT, ixgbe_sysctl_fwlog_set_cfg_options,
             "CU", IXGBE_SYSCTL_HELP_FWLOG_ENABLE_ON_LOAD);
 
         SYSCTL_ADD_PROC(ctx, fwlog_list, OID_AUTO, "register",
             CTLTYPE_U8 | CTLFLAG_RWTUN, sc,
             0, ixgbe_sysctl_fwlog_register,
             "CU", IXGBE_SYSCTL_HELP_FWLOG_REGISTER);
 
         module_node = SYSCTL_ADD_NODE(ctx, fwlog_list, OID_AUTO, "severity",
                                       CTLFLAG_RD, NULL,
                                       "Level of log output");
 
         module_list = SYSCTL_CHILDREN(module_node);
 
         for (i = 0; i < IXGBE_ACI_FW_LOG_ID_MAX; i++) {
                 /* Setup some defaults */
                 cfg->module_entries[i].module_id = i;
                 cfg->module_entries[i].log_level = IXGBE_FWLOG_LEVEL_NONE;
                 module = (enum ixgbe_aci_fw_logging_mod)i;
 
                 SYSCTL_ADD_PROC(ctx, module_list,
                     OID_AUTO, ixgbe_fw_module_str(module),
                     CTLTYPE_STRING | CTLFLAG_RWTUN, sc,
                     module, ixgbe_sysctl_fwlog_module_log_severity,
                     "A", IXGBE_SYSCTL_HELP_FWLOG_MODULE_SEVERITY);
         }
 }
 
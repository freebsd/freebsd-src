#ifndef __ERROR_LOG_H__
#define __ERROR_LOG_H__

#define VERBOSE_ERRORS		1 /* Maybe I enlarge the kernel too much */
#undef VERBOSE_ERRORS

/* Event classes */
/* XXX: Endianess correct? NOW*/
#define INTERNAL_ERROR		0x80000000 /* set bit 0 */
#define EPOW_WARNING		0x40000000 /* set bit 1 */
#define POWERMGM_EVENTS		0x20000000 /* set bit 2 */

/* event-scan returns */
#define SEVERITY_FATAL		0x5
#define SEVERITY_ERROR		0x4
#define SEVERITY_ERROR_SYNC	0x3
#define SEVERITY_WARNING	0x2
#define SEVERITY_EVENT		0x1
#define SEVERITY_NO_ERROR	0x0
#define DISP_FULLY_RECOVERED	0x0
#define DISP_LIMITED_RECOVERY	0x1
#define DISP_NOT_RECOVERED	0x2
#define PART_PRESENT		0x0
#define PART_NOT_PRESENT	0x1
#define INITIATOR_UNKNOWN	0x0
#define INITIATOR_CPU		0x1
#define INITIATOR_PCI		0x2
#define INITIATOR_ISA		0x3
#define INITIATOR_MEMORY	0x4
#define INITIATOR_POWERMGM	0x5
#define TARGET_UNKNOWN		0x0
#define TARGET_CPU		0x1
#define TARGET_PCI		0x2
#define TARGET_ISA		0x3
#define TARGET_MEMORY		0x4
#define TARGET_POWERMGM		0x5
#define TYPE_RETRY		0x01
#define TYPE_TCE_ERR		0x02
#define TYPE_INTERN_DEV_FAIL	0x03
#define TYPE_TIMEOUT		0x04
#define TYPE_DATA_PARITY	0x05
#define TYPE_ADDR_PARITY	0x06
#define TYPE_CACHE_PARITY	0x07
#define TYPE_ADDR_INVALID	0x08
#define TYPE_ECC_UNCORR		0x09
#define TYPE_ECC_CORR		0x0a
#define TYPE_EPOW		0x40
/* I don't add PowerMGM events right now, this is a different topic */
#define TYPE_PMGM_POWER_SW_ON	0x60
#define TYPE_PMGM_POWER_SW_OFF	0x61
#define TYPE_PMGM_LID_OPEN	0x62
#define TYPE_PMGM_LID_CLOSE	0x63
#define TYPE_PMGM_SLEEP_BTN	0x64
#define TYPE_PMGM_WAKE_BTN	0x65
#define TYPE_PMGM_BATTERY_WARN	0x66
#define TYPE_PMGM_BATTERY_CRIT	0x67
#define TYPE_PMGM_SWITCH_TO_BAT	0x68
#define TYPE_PMGM_SWITCH_TO_AC	0x69
#define TYPE_PMGM_KBD_OR_MOUSE	0x6a
#define TYPE_PMGM_ENCLOS_OPEN	0x6b
#define TYPE_PMGM_ENCLOS_CLOSED	0x6c
#define TYPE_PMGM_RING_INDICATE	0x6d
#define TYPE_PMGM_LAN_ATTENTION	0x6e
#define TYPE_PMGM_TIME_ALARM	0x6f
#define TYPE_PMGM_CONFIG_CHANGE	0x70
#define TYPE_PMGM_SERVICE_PROC	0x71

typedef struct _rtas_error_log {
	unsigned long version:8;		/* Architectural version */
	unsigned long severity:3;		/* Severity level of error */
	unsigned long disposition:2;		/* Degree of recovery */
	unsigned long extended:1;		/* extended log present? */
	unsigned long /* reserved */ :2;	/* Reserved for future use */
	unsigned long initiator:4;		/* Initiator of event */
	unsigned long target:4;			/* Target of failed operation */
	unsigned long type:8;			/* General event or error*/
	unsigned long extended_log_length:32;	/* length in bytes */
} rtas_error_log;

/* ****************************************************************** */
#define ppc_rtas_errorlog_check_severity(x) \
	(_errlog_severity[x.severity])
#define ppc_rtas_errorlog_check_target(x) \
	(_errlog_target[x.target])
#define ppc_rtas_errorlog_check_initiator(x) \
	(_errlog_initiator[x.initiator])
#define ppc_rtas_errorlog_check_extended(x) \
	(_errlog_extended[x.extended])
#define ppc_rtas_errorlog_disect_extended(x) \
	do { /* implement me */ } while(0)
extern const char * ppc_rtas_errorlog_check_type (rtas_error_log error_log);
extern int ppc_rtas_errorlog_scan(void);


#endif /* __ERROR_LOG_H__ */

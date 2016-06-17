/*
 * We should not even be trying to compile this if we are not doing
 * a module.
 */
#define __NO_VERSION__
#include <linux/config.h>
#include <linux/module.h>

#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/blk.h>
#include <linux/fs.h>

#include <asm/system.h>
#include <asm/irq.h>
#include <asm/dma.h>

#include "scsi.h"
#include <scsi/scsi_ioctl.h>
#include "hosts.h"
#include "constants.h"

#include "sd.h"
#include <scsi/scsicam.h>

/*
 * This source file contains the symbol table used by scsi loadable
 * modules.
 */
EXPORT_SYMBOL(scsi_register_module);
EXPORT_SYMBOL(scsi_unregister_module);
EXPORT_SYMBOL(scsi_free);
EXPORT_SYMBOL(scsi_malloc);
EXPORT_SYMBOL(scsi_register);
EXPORT_SYMBOL(scsi_unregister);
EXPORT_SYMBOL(scsicam_bios_param);
EXPORT_SYMBOL(scsi_partsize);
EXPORT_SYMBOL(scsi_allocate_device);
EXPORT_SYMBOL(scsi_do_cmd);
EXPORT_SYMBOL(scsi_command_size);
EXPORT_SYMBOL(scsi_ioctl);
EXPORT_SYMBOL(scsi_finish_command);
EXPORT_SYMBOL(print_command);
EXPORT_SYMBOL(print_sense);
EXPORT_SYMBOL(print_req_sense);
EXPORT_SYMBOL(print_msg);
EXPORT_SYMBOL(print_status);
EXPORT_SYMBOL(scsi_dma_free_sectors);
EXPORT_SYMBOL(kernel_scsi_ioctl);
EXPORT_SYMBOL(scsi_need_isa_buffer);
EXPORT_SYMBOL(scsi_release_command);
EXPORT_SYMBOL(print_Scsi_Cmnd);
EXPORT_SYMBOL(scsi_block_when_processing_errors);
EXPORT_SYMBOL(scsi_mark_host_reset);
EXPORT_SYMBOL(scsi_ioctl_send_command);
#if defined(CONFIG_SCSI_LOGGING)	/* { */
EXPORT_SYMBOL(scsi_logging_level);
#endif

EXPORT_SYMBOL(scsi_allocate_request);
EXPORT_SYMBOL(scsi_release_request);
EXPORT_SYMBOL(scsi_wait_req);
EXPORT_SYMBOL(scsi_do_req);

EXPORT_SYMBOL(scsi_report_bus_reset);
EXPORT_SYMBOL(scsi_block_requests);
EXPORT_SYMBOL(scsi_unblock_requests);

EXPORT_SYMBOL(scsi_get_host_dev);
EXPORT_SYMBOL(scsi_free_host_dev);

EXPORT_SYMBOL(scsi_sleep);

EXPORT_SYMBOL(proc_print_scsidevice);
EXPORT_SYMBOL(proc_scsi);

EXPORT_SYMBOL(scsi_io_completion);
EXPORT_SYMBOL(scsi_end_request);

EXPORT_SYMBOL(scsi_register_blocked_host);
EXPORT_SYMBOL(scsi_deregister_blocked_host);

/*
 * This symbol is for the highlevel drivers (e.g. sg) only.
 */
EXPORT_SYMBOL(scsi_reset_provider);

/*
 * These are here only while I debug the rest of the scsi stuff.
 */
EXPORT_SYMBOL(scsi_hostlist);
EXPORT_SYMBOL(scsi_hosts);
EXPORT_SYMBOL(scsi_devicelist);
EXPORT_SYMBOL(scsi_device_types);

/*
 * Externalize timers so that HBAs can safely start/restart commands.
 */
extern void scsi_add_timer(Scsi_Cmnd *, int, void ((*) (Scsi_Cmnd *)));
extern int scsi_delete_timer(Scsi_Cmnd *);
EXPORT_SYMBOL(scsi_add_timer);
EXPORT_SYMBOL(scsi_delete_timer);

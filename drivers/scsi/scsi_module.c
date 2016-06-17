/*
 *  scsi_module.c Copyright (1994, 1995) Eric Youngdale.
 *
 * Support for loading low-level scsi drivers using the linux kernel loadable
 * module interface.
 *
 * To use, the host adapter should first define and initialize the variable
 * driver_template (datatype Scsi_Host_Template), and then include this file.
 * This should also be wrapped in a #ifdef MODULE/#endif.
 *
 * The low -level driver must also define a release function which will
 * free any irq assignments, release any dma channels, release any I/O
 * address space that might be reserved, and otherwise clean up after itself.
 * The idea is that the same driver should be able to be reloaded without
 * any difficulty.  This makes debugging new drivers easier, as you should
 * be able to load the driver, test it, unload, modify and reload.
 *
 * One *very* important caveat.  If the driver may need to do DMA on the
 * ISA bus, you must have unchecked_isa_dma set in the device template,
 * even if this might be changed during the detect routine.  This is
 * because the shpnt structure will be allocated in a special way so that
 * it will be below the appropriate DMA limit - thus if your driver uses
 * the hostdata field of shpnt, and the board must be able to access this
 * via DMA, the shpnt structure must be in a DMA accessible region of
 * memory.  This comment would be relevant for something like the buslogic
 * driver where there are many boards, only some of which do DMA onto the
 * ISA bus.  There is no convenient way of specifying whether the host
 * needs to be in a ISA DMA accessible region of memory when you call
 * scsi_register.
 */

#include <linux/module.h>
#include <linux/init.h>

static int __init init_this_scsi_driver(void)
{
	driver_template.module = THIS_MODULE;
	scsi_register_module(MODULE_SCSI_HA, &driver_template);
	if (driver_template.present)
		return 0;

	scsi_unregister_module(MODULE_SCSI_HA, &driver_template);
	return -ENODEV;
}

static void __exit exit_this_scsi_driver(void)
{
	scsi_unregister_module(MODULE_SCSI_HA, &driver_template);
}

module_init(init_this_scsi_driver);
module_exit(exit_this_scsi_driver);

/*
 * Overrides for Emacs so that we almost follow Linus's tabbing style.
 * Emacs will notice this stuff at the end of the file and automatically
 * adjust the settings for this buffer only.  This must remain at the end
 * of the file.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-indent-level: 4
 * c-brace-imaginary-offset: 0
 * c-brace-offset: -4
 * c-argdecl-indent: 4
 * c-label-offset: -4
 * c-continued-statement-offset: 4
 * c-continued-brace-offset: 0
 * indent-tabs-mode: nil
 * tab-width: 8
 * End:
 */

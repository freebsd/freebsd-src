/*
 *  linux/drivers/char/hcdp_serial.c
 *
 *  Copyright (C) 2002  Hewlett-Packard Co.
 *  Copyright (C) 2002  Khalid Aziz <khalid_aziz@hp.com>
 *
 *  Parse the EFI HCDP table to locate serial console and debug ports
 *  and initialize them
 *
 */
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/pm.h>
#include <linux/init.h>
#include <linux/serial.h>
#include <linux/serialP.h>
#include <linux/efi.h>
#include <asm/serial.h>
#include <asm/io.h>
#include <linux/hcdp_serial.h>

#undef SERIAL_DEBUG_HCDP

extern struct serial_state rs_table[];
extern int serial_nr_ports;

/*
 * Parse the HCDP table to find descriptions for headless console and 
 * debug serial ports and add them to rs_table[]. A pointer to HCDP
 * table is passed as parameter. This function should be called 
 * before serial_console_init() is called to make sure the HCDP serial 
 * console will be available for use. IA-64 kernel calls this function
 * from setup_arch() after the EFI and ACPI tables have been parsed.
 */
void __init setup_serial_hcdp(void *tablep) 
{
	hcdp_t hcdp;
	hcdp_dev_t *hcdp_dev;
	struct serial_struct serial_req;
	unsigned long iobase;
	int global_sys_irq;
	int i, nr;
	int shift_once = 1;

#ifdef SERIAL_DEBUG_HCDP
	printk("Entering setup_serial_hcdp()\n");
#endif

	/* Verify we have a valid table pointer */
	if (tablep == NULL) {
		return;
	}

	/*
	 * We do not trust firmware to give us a table starting at an
	 * aligned address. Make a local copy of the HCDP table with 
	 * aligned structures.
	 */
	memcpy(&hcdp, tablep, sizeof(hcdp));

	/*
	 * Perform a sanity check on the table. Table should have a 
	 * signature of "HCDP" and it should be atleast 82 bytes
	 * long to have any useful information.
	 */
	if ((strncmp(hcdp.signature, HCDP_SIGNATURE, 
					HCDP_SIG_LEN) != 0)) {
		return;
	}
	if (hcdp.len < 82) {
		return;
	}

#ifdef SERIAL_DEBUG_HCDP
	printk("setup_serial_hcdp(): table pointer = 0x%p\n", tablep);
	printk("                     sig = '%c%c%c%c'\n",
			hcdp.signature[0],
			hcdp.signature[1],
			hcdp.signature[2],
			hcdp.signature[3]);
	printk("                     length = %d\n", hcdp.len);
	printk("                     Rev = %d\n", hcdp.rev);
	printk("                     OEM ID = %c%c%c%c%c%c\n", 
			hcdp.oemid[0], hcdp.oemid[1], hcdp.oemid[2],
			hcdp.oemid[3], hcdp.oemid[4], hcdp.oemid[5]);
	printk("                     Number of entries = %d\n", hcdp.num_entries);
#endif

	/*
	 * Parse each device entry
	 */
	for (nr=0; nr<hcdp.num_entries; nr++) {
		hcdp_dev = &(hcdp.hcdp_dev[nr]);

		/*
		 * We will parse only the primary console device
		 * which is the first entry for these devices. We will
		 * ignore rest of the entries for the same type device that
		 * has already been parsed and initialized
		 */
		if (hcdp_dev->type != HCDP_DEV_CONSOLE)
			continue;

		iobase = (u64)(hcdp_dev->base_addr.addrhi)<<32 | hcdp_dev->base_addr.addrlo;
		global_sys_irq = hcdp_dev->global_int;
#ifdef SERIAL_DEBUG_HCDP
		printk("                 type = %s\n", 
			((hcdp_dev->type == HCDP_DEV_CONSOLE)?"Headless Console":((hcdp_dev->type == HCDP_DEV_DEBUG)?"Debug port":"Huh????")));
		printk("                 Base address space = %s\n", ((hcdp_dev->base_addr.space_id == ACPI_MEM_SPACE)?"Memory Space":((hcdp_dev->base_addr.space_id == ACPI_IO_SPACE)?"I/O space":"PCI space")));
		printk("                 Base address = 0x%p\n", iobase);
		printk("                 Global System Int = %d\n", global_sys_irq);
		printk("                 Baud rate = %d\n", hcdp_dev->baud);
		printk("                 Bits = %d\n", hcdp_dev->bits);
		printk("                 Clock rate = %d\n", hcdp_dev->clock_rate);
		if (hcdp_dev->base_addr.space_id == ACPI_PCICONF_SPACE) {
			printk("                     PCI serial port:\n");
			printk("                         Bus %d, Device %d, Vendor ID 0x%x, Dev ID 0x%x\n",
			hcdp_dev->pci_bus, hcdp_dev->pci_dev,
			hcdp_dev->pci_vendor_id, hcdp_dev->pci_dev_id);
		}
#endif


		/* 
	 	* Now build a serial_req structure to update the entry in
	 	* rs_table for the headless console port.
	 	*/
		if (hcdp_dev->clock_rate)
			serial_req.baud_base = hcdp_dev->clock_rate;
		else
			serial_req.baud_base = DEFAULT_BAUD_BASE;
		/*
	 	* Check if this is an I/O mapped address or a memory mapped address
	 	*/
		if (hcdp_dev->base_addr.space_id == ACPI_MEM_SPACE) {
			serial_req.port = 0;
			serial_req.port_high = 0;
			serial_req.iomem_base = (void *)ioremap(iobase, 64);
			serial_req.io_type = SERIAL_IO_MEM;
		}
		else if (hcdp_dev->base_addr.space_id == ACPI_IO_SPACE) {
			serial_req.port = (unsigned long) iobase & 0xffffffff;
			serial_req.port_high = (unsigned long)(((u64)iobase) >> 32);
			serial_req.iomem_base = NULL;
			serial_req.io_type = SERIAL_IO_PORT;
		}
		else if (hcdp_dev->base_addr.space_id == ACPI_PCICONF_SPACE) {
			printk("WARNING: No support for PCI serial console\n");
			return;
		}

		/*
		 * Check if HCDP defines a port already in rs_table
		 */
		for (i = 0; i < serial_nr_ports; i++) {
			if ((rs_table[i].port == serial_req.port) &&
				(rs_table[i].iomem_base==serial_req.iomem_base))
				break;
		}
		if (i == serial_nr_ports) {
			/*
			 * We have reserved a slot for HCDP defined console
			 * port at HCDP_SERIAL_CONSOLE_PORT in rs_table
			 * which is not 0. This means using this slot would
			 * put the console at a device other than ttyS0. 
			 * Users expect to see the console at ttyS0. Now 
			 * that we have determined HCDP does describe a 
			 * serial console and it is not one of the compiled
			 * in ports, let us move the entries in rs_table
			 * up by a slot towards HCDP_SERIAL_CONSOLE_PORT to 
			 * make room for the HCDP console at ttyS0. We may go
			 * through this loop more than once if 
			 * early_serial_setup() fails. Make sure we shift the
			 * entries in rs_table only once.
			 */
			if (shift_once) {
				int j;

				for (j=HCDP_SERIAL_CONSOLE_PORT; j>0; j--)
					memcpy(rs_table+j, rs_table+j-1, 
						sizeof(struct serial_state));
				shift_once = 0;
			}
			serial_req.line = 0;
		}
		else
			serial_req.line = i;

		/*
	 	* If the table does not have IRQ information, use 0 for IRQ. 
	 	* This will force rs_init() to probe for IRQ. 
	 	*/
		serial_req.irq = global_sys_irq;
		if (global_sys_irq == 0) {
			serial_req.flags = ASYNC_SKIP_TEST|ASYNC_BOOT_AUTOCONF;
		}
		else {
			serial_req.flags = ASYNC_SKIP_TEST|ASYNC_BOOT_AUTOCONF|
						ASYNC_AUTO_IRQ;
		}

		serial_req.xmit_fifo_size = serial_req.custom_divisor = 0;
		serial_req.close_delay = serial_req.hub6 = serial_req.closing_wait = 0;
		serial_req.iomem_reg_shift = 0;
		if (early_serial_setup(&serial_req) < 0) {
			printk("setup_serial_hcdp(): early_serial_setup() for HCDP serial console port failed. Will try any additional consoles in HCDP.\n");
			continue;
		}
		else
			if (hcdp_dev->type == HCDP_DEV_CONSOLE)
				break;
#ifdef SERIAL_DEBUG_HCDP
		printk("\n");
#endif
	}

#ifdef SERIAL_DEBUG_HCDP
	printk("Leaving setup_serial_hcdp()\n");
#endif
}

#ifdef CONFIG_IA64_EARLY_PRINTK_UART
unsigned long hcdp_early_uart(void)
{
	efi_system_table_t *systab;
	efi_config_table_t *config_tables;
	hcdp_t *hcdp = 0;
	hcdp_dev_t *dev;
	int i;

	systab = (efi_system_table_t *) ia64_boot_param->efi_systab;
	if (!systab)
		return 0;
	systab = __va(systab);

	config_tables = (efi_config_table_t *) systab->tables;
	if (!config_tables)
		return 0;
	config_tables = __va(config_tables);

	for (i = 0; i < systab->nr_tables; i++) {
		if (efi_guidcmp(config_tables[i].guid, HCDP_TABLE_GUID) == 0) {
			hcdp = (hcdp_t *) config_tables[i].table;
			break;
		}
	}
	if (!hcdp)
		return 0;
	hcdp = __va(hcdp);

	for (i = 0, dev = hcdp->hcdp_dev; i < hcdp->num_entries; i++, dev++) {
		if (dev->type == HCDP_DEV_CONSOLE)
			return (u64) dev->base_addr.addrhi << 32
				| dev->base_addr.addrlo;
	}
	return 0;
}
#endif

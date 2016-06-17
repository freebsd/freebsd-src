/*
 *  acpi_ksyms.c - ACPI Kernel Symbols ($Revision: 15 $)
 *
 *  Copyright (C) 2001, 2002 Andy Grover <andrew.grover@intel.com>
 *  Copyright (C) 2001, 2002 Paul Diefenbaugh <paul.s.diefenbaugh@intel.com>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or (at
 *  your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <linux/module.h>
#include <linux/acpi.h>

#ifdef CONFIG_ACPI_INTERPRETER

/* ACPI Debugger */

#ifdef ENABLE_DEBUGGER

extern int			acpi_in_debugger;

EXPORT_SYMBOL(acpi_in_debugger);
EXPORT_SYMBOL(acpi_db_user_commands);

#endif /* ENABLE_DEBUGGER */

/* ACPI Core Subsystem */

#ifdef ACPI_DEBUG_OUTPUT
EXPORT_SYMBOL(acpi_dbg_layer);
EXPORT_SYMBOL(acpi_dbg_level);
EXPORT_SYMBOL(acpi_ut_debug_print_raw);
EXPORT_SYMBOL(acpi_ut_debug_print);
EXPORT_SYMBOL(acpi_ut_status_exit);
EXPORT_SYMBOL(acpi_ut_value_exit);
EXPORT_SYMBOL(acpi_ut_exit);
EXPORT_SYMBOL(acpi_ut_trace);
#endif /*ACPI_DEBUG_OUTPUT*/

EXPORT_SYMBOL(acpi_get_handle);
EXPORT_SYMBOL(acpi_get_parent);
EXPORT_SYMBOL(acpi_get_type);
EXPORT_SYMBOL(acpi_get_name);
EXPORT_SYMBOL(acpi_get_object_info);
EXPORT_SYMBOL(acpi_get_next_object);
EXPORT_SYMBOL(acpi_evaluate_object);
EXPORT_SYMBOL(acpi_get_table);
EXPORT_SYMBOL(acpi_get_firmware_table);
EXPORT_SYMBOL(acpi_install_notify_handler);
EXPORT_SYMBOL(acpi_remove_notify_handler);
EXPORT_SYMBOL(acpi_install_gpe_handler);
EXPORT_SYMBOL(acpi_remove_gpe_handler);
EXPORT_SYMBOL(acpi_install_address_space_handler);
EXPORT_SYMBOL(acpi_remove_address_space_handler);
EXPORT_SYMBOL(acpi_install_fixed_event_handler);
EXPORT_SYMBOL(acpi_remove_fixed_event_handler);
EXPORT_SYMBOL(acpi_acquire_global_lock);
EXPORT_SYMBOL(acpi_release_global_lock);
EXPORT_SYMBOL(acpi_install_gpe_block);
EXPORT_SYMBOL(acpi_remove_gpe_block);
EXPORT_SYMBOL(acpi_get_current_resources);
EXPORT_SYMBOL(acpi_get_possible_resources);
EXPORT_SYMBOL(acpi_walk_resources);
EXPORT_SYMBOL(acpi_set_current_resources);
EXPORT_SYMBOL(acpi_enable_event);
EXPORT_SYMBOL(acpi_disable_event);
EXPORT_SYMBOL(acpi_clear_event);
EXPORT_SYMBOL(acpi_get_timer_duration);
EXPORT_SYMBOL(acpi_get_timer);
EXPORT_SYMBOL(acpi_get_sleep_type_data);
EXPORT_SYMBOL(acpi_get_register);
EXPORT_SYMBOL(acpi_set_register);
EXPORT_SYMBOL(acpi_enter_sleep_state);
EXPORT_SYMBOL(acpi_enter_sleep_state_s4bios);
EXPORT_SYMBOL(acpi_get_system_info);
EXPORT_SYMBOL(acpi_get_devices);

/* ACPI OS Services Layer (acpi_osl.c) */

EXPORT_SYMBOL(acpi_os_free);
EXPORT_SYMBOL(acpi_os_printf);
EXPORT_SYMBOL(acpi_os_sleep);
EXPORT_SYMBOL(acpi_os_stall);
EXPORT_SYMBOL(acpi_os_signal);
EXPORT_SYMBOL(acpi_os_queue_for_execution);
EXPORT_SYMBOL(acpi_os_signal_semaphore);
EXPORT_SYMBOL(acpi_os_create_semaphore);
EXPORT_SYMBOL(acpi_os_delete_semaphore);
EXPORT_SYMBOL(acpi_os_wait_semaphore);

EXPORT_SYMBOL(acpi_os_read_pci_configuration);

/* ACPI Utilities (acpi_utils.c) */

EXPORT_SYMBOL(acpi_extract_package);
EXPORT_SYMBOL(acpi_evaluate_integer);
EXPORT_SYMBOL(acpi_evaluate_reference);

#endif /*CONFIG_ACPI_INTERPRETER*/


/* ACPI Bus Driver (acpi_bus.c) */

#ifdef CONFIG_ACPI_BUS

EXPORT_SYMBOL(acpi_fadt);
EXPORT_SYMBOL(acpi_walk_namespace);
EXPORT_SYMBOL(acpi_root_dir);
EXPORT_SYMBOL(acpi_bus_get_device);
EXPORT_SYMBOL(acpi_bus_get_status);
EXPORT_SYMBOL(acpi_bus_get_power);
EXPORT_SYMBOL(acpi_bus_set_power);
EXPORT_SYMBOL(acpi_bus_generate_event);
EXPORT_SYMBOL(acpi_bus_receive_event);
EXPORT_SYMBOL(acpi_bus_register_driver);
EXPORT_SYMBOL(acpi_bus_unregister_driver);
EXPORT_SYMBOL(acpi_bus_scan);
EXPORT_SYMBOL(acpi_init);

#endif /*CONFIG_ACPI_BUS*/


/* ACPI PCI Driver (pci_irq.c) */

#ifdef CONFIG_ACPI_PCI

#include <linux/pci.h>
extern int acpi_pci_irq_enable(struct pci_dev *dev);
EXPORT_SYMBOL(acpi_pci_irq_enable);
extern int acpi_pci_irq_lookup (int segment, int bus, int device, int pin);
EXPORT_SYMBOL(acpi_pci_irq_lookup);
EXPORT_SYMBOL(acpi_pci_register_driver);
EXPORT_SYMBOL(acpi_pci_unregister_driver);
#endif /*CONFIG_ACPI_PCI */

#ifdef CONFIG_ACPI_EC
/* ACPI EC driver (ec.c) */

EXPORT_SYMBOL(ec_read);
EXPORT_SYMBOL(ec_write);
#endif


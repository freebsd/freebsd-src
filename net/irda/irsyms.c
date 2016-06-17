/*********************************************************************
 *                
 * Filename:      irsyms.c
 * Version:       0.9
 * Description:   IrDA module symbols
 * Status:        Experimental.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Mon Dec 15 13:55:39 1997
 * Modified at:   Wed Jan  5 15:12:41 2000
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 * 
 *     Copyright (c) 1997, 1999-2000 Dag Brattli, All Rights Reserved.
 *     Copyright (c) 2000-2001 Jean Tourrilhes <jt@hpl.hp.com>
 *      
 *     This program is free software; you can redistribute it and/or 
 *     modify it under the terms of the GNU General Public License as 
 *     published by the Free Software Foundation; either version 2 of 
 *     the License, or (at your option) any later version.
 *  
 *     Neither Dag Brattli nor University of Tromsø admit liability nor
 *     provide warranty for any of this software. This material is 
 *     provided "AS-IS" and at no charge.
 *     
 ********************************************************************/

#include <linux/config.h>
#include <linux/module.h>

#include <linux/init.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/smp_lock.h>

#include <asm/segment.h>

#include <net/irda/irda.h>
#include <net/irda/irmod.h>
#include <net/irda/irlap.h>
#include <net/irda/irlmp.h>
#include <net/irda/iriap.h>
#include <net/irda/irias_object.h>
#include <net/irda/irttp.h>
#include <net/irda/irda_device.h>
#include <net/irda/wrapper.h>
#include <net/irda/timer.h>
#include <net/irda/parameters.h>
#include <net/irda/crc.h>

extern struct proc_dir_entry *proc_irda;

extern void irda_proc_register(void);
extern void irda_proc_unregister(void);
extern int  irda_sysctl_register(void);
extern void irda_sysctl_unregister(void);

extern int irda_proto_init(void);
extern void irda_proto_cleanup(void);

extern int irda_device_init(void);
extern int irlan_init(void);
extern int irlan_client_init(void);
extern int irlan_server_init(void);
extern int ircomm_init(void);
extern int ircomm_tty_init(void);
extern int irlpt_client_init(void);
extern int irlpt_server_init(void);

/* IrTTP */
EXPORT_SYMBOL(irttp_open_tsap);
EXPORT_SYMBOL(irttp_close_tsap);
EXPORT_SYMBOL(irttp_connect_response);
EXPORT_SYMBOL(irttp_data_request);
EXPORT_SYMBOL(irttp_disconnect_request);
EXPORT_SYMBOL(irttp_flow_request);
EXPORT_SYMBOL(irttp_connect_request);
EXPORT_SYMBOL(irttp_udata_request);
EXPORT_SYMBOL(irttp_dup);

/* Main IrDA module */
#ifdef CONFIG_IRDA_DEBUG
EXPORT_SYMBOL(irda_debug);
#endif
EXPORT_SYMBOL(irda_notify_init);
#ifdef CONFIG_PROC_FS
EXPORT_SYMBOL(proc_irda);
#endif
EXPORT_SYMBOL(irda_param_insert);
EXPORT_SYMBOL(irda_param_extract);
EXPORT_SYMBOL(irda_param_extract_all);
EXPORT_SYMBOL(irda_param_pack);
EXPORT_SYMBOL(irda_param_unpack);

/* IrIAP/IrIAS */
EXPORT_SYMBOL(iriap_open);
EXPORT_SYMBOL(iriap_close);
EXPORT_SYMBOL(iriap_getvaluebyclass_request);
EXPORT_SYMBOL(irias_object_change_attribute);
EXPORT_SYMBOL(irias_add_integer_attrib);
EXPORT_SYMBOL(irias_add_octseq_attrib);
EXPORT_SYMBOL(irias_add_string_attrib);
EXPORT_SYMBOL(irias_insert_object);
EXPORT_SYMBOL(irias_new_object);
EXPORT_SYMBOL(irias_delete_object);
EXPORT_SYMBOL(irias_delete_value);
EXPORT_SYMBOL(irias_find_object);
EXPORT_SYMBOL(irias_find_attrib);
EXPORT_SYMBOL(irias_new_integer_value);
EXPORT_SYMBOL(irias_new_string_value);
EXPORT_SYMBOL(irias_new_octseq_value);

/* IrLMP */
EXPORT_SYMBOL(irlmp_discovery_request);
EXPORT_SYMBOL(irlmp_get_discoveries);
EXPORT_SYMBOL(sysctl_discovery_timeout);
EXPORT_SYMBOL(irlmp_register_client);
EXPORT_SYMBOL(irlmp_unregister_client);
EXPORT_SYMBOL(irlmp_update_client);
EXPORT_SYMBOL(irlmp_register_service);
EXPORT_SYMBOL(irlmp_unregister_service);
EXPORT_SYMBOL(irlmp_service_to_hint);
EXPORT_SYMBOL(irlmp_data_request);
EXPORT_SYMBOL(irlmp_open_lsap);
EXPORT_SYMBOL(irlmp_close_lsap);
EXPORT_SYMBOL(irlmp_connect_request);
EXPORT_SYMBOL(irlmp_connect_response);
EXPORT_SYMBOL(irlmp_disconnect_request);
EXPORT_SYMBOL(irlmp_get_daddr);
EXPORT_SYMBOL(irlmp_get_saddr);
EXPORT_SYMBOL(irlmp_dup);
EXPORT_SYMBOL(lmp_reasons);

/* Queue */
EXPORT_SYMBOL(hashbin_find);
EXPORT_SYMBOL(hashbin_new);
EXPORT_SYMBOL(hashbin_insert);
EXPORT_SYMBOL(hashbin_delete);
EXPORT_SYMBOL(hashbin_remove);
EXPORT_SYMBOL(hashbin_remove_this);
EXPORT_SYMBOL(hashbin_get_next);
EXPORT_SYMBOL(hashbin_get_first);

/* IrLAP */
EXPORT_SYMBOL(irlap_open);
EXPORT_SYMBOL(irlap_close);
EXPORT_SYMBOL(irda_init_max_qos_capabilies);
EXPORT_SYMBOL(irda_qos_bits_to_value);
EXPORT_SYMBOL(irda_device_setup);
EXPORT_SYMBOL(irda_device_set_media_busy);
EXPORT_SYMBOL(irda_device_txqueue_empty);

EXPORT_SYMBOL(irda_device_dongle_init);
EXPORT_SYMBOL(irda_device_dongle_cleanup);
EXPORT_SYMBOL(irda_device_register_dongle);
EXPORT_SYMBOL(irda_device_unregister_dongle);
EXPORT_SYMBOL(irda_task_execute);
EXPORT_SYMBOL(irda_task_kick);
EXPORT_SYMBOL(irda_task_next_state);
EXPORT_SYMBOL(irda_task_delete);

EXPORT_SYMBOL(async_wrap_skb);
EXPORT_SYMBOL(async_unwrap_char);
EXPORT_SYMBOL(irda_calc_crc16);
EXPORT_SYMBOL(irda_crc16_table);
EXPORT_SYMBOL(irda_start_timer);
EXPORT_SYMBOL(setup_dma);
EXPORT_SYMBOL(infrared_mode);

#ifdef CONFIG_IRTTY
EXPORT_SYMBOL(irtty_set_dtr_rts);
EXPORT_SYMBOL(irtty_register_dongle);
EXPORT_SYMBOL(irtty_unregister_dongle);
EXPORT_SYMBOL(irtty_set_packet_mode);
#endif

int __init irda_init(void)
{
	IRDA_DEBUG(0, "%s()\n", __FUNCTION__);

 	irlmp_init();
	irlap_init();
	
	iriap_init();
 	irttp_init();
	
#ifdef CONFIG_PROC_FS
	irda_proc_register();
#endif
#ifdef CONFIG_SYSCTL
	irda_sysctl_register();
#endif
	/* 
	 * Initialize modules that got compiled into the kernel 
	 */
#ifdef CONFIG_IRLAN
	irlan_init();
#endif
#ifdef CONFIG_IRCOMM
	ircomm_init();
	ircomm_tty_init();
#endif
	return 0;
}

void __exit irda_cleanup(void)
{
#ifdef CONFIG_SYSCTL
	irda_sysctl_unregister();
#endif	

#ifdef CONFIG_PROC_FS
	irda_proc_unregister();
#endif
	/* Remove higher layers */
	irttp_cleanup();
	iriap_cleanup();

	/* Remove lower layers */
	irda_device_cleanup();
	irlap_cleanup(); /* Must be done before irlmp_cleanup()! DB */

	/* Remove middle layer */
	irlmp_cleanup();
}

/*
 * Function irda_notify_init (notify)
 *
 *    Used for initializing the notify structure
 *
 */
void irda_notify_init(notify_t *notify)
{
	notify->data_indication = NULL;
	notify->udata_indication = NULL;
	notify->connect_confirm = NULL;
	notify->connect_indication = NULL;
	notify->disconnect_indication = NULL;
	notify->flow_indication = NULL;
	notify->status_indication = NULL;
	notify->instance = NULL;
	strncpy(notify->name, "Unknown", NOTIFY_MAX_NAME);
}


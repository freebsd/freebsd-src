/*
 * ds.h 1.56 2000/06/12 21:55:40
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License
 * at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and
 * limitations under the License. 
 *
 * The initial developer of the original code is David A. Hinds
 * <dahinds@users.sourceforge.net>.  Portions created by David A. Hinds
 * are Copyright (C) 1999 David A. Hinds.  All Rights Reserved.
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License version 2 (the "GPL"), in which
 * case the provisions of the GPL are applicable instead of the
 * above.  If you wish to allow the use of your version of this file
 * only under the terms of the GPL and not to allow others to use
 * your version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the GPL.  If you do not delete the
 * provisions above, a recipient may use your version of this file
 * under either the MPL or the GPL.
 */

#ifndef _LINUX_DS_H
#define _LINUX_DS_H

#include <pcmcia/driver_ops.h>
#include <pcmcia/bulkmem.h>

typedef struct tuple_parse_t {
    tuple_t		tuple;
    cisdata_t		data[255];
    cisparse_t		parse;
} tuple_parse_t;

typedef struct win_info_t {
    window_handle_t	handle;
    win_req_t		window;
    memreq_t		map;
} win_info_t;
    
typedef struct bind_info_t {
    dev_info_t		dev_info;
    u_char		function;
    struct dev_link_t	*instance;
    char		name[DEV_NAME_LEN];
    u_short		major, minor;
    void		*next;
} bind_info_t;

typedef struct mtd_info_t {
    dev_info_t		dev_info;
    u_int		Attributes;
    u_int		CardOffset;
} mtd_info_t;

typedef union ds_ioctl_arg_t {
    servinfo_t		servinfo;
    adjust_t		adjust;
    config_info_t	config;
    tuple_t		tuple;
    tuple_parse_t	tuple_parse;
    client_req_t	client_req;
    cs_status_t		status;
    conf_reg_t		conf_reg;
    cisinfo_t		cisinfo;
    region_info_t	region;
    bind_info_t		bind_info;
    mtd_info_t		mtd_info;
    win_info_t		win_info;
    cisdump_t		cisdump;
} ds_ioctl_arg_t;

#define DS_GET_CARD_SERVICES_INFO	_IOR ('d', 1, servinfo_t)
#define DS_ADJUST_RESOURCE_INFO		_IOWR('d', 2, adjust_t)
#define DS_GET_CONFIGURATION_INFO	_IOWR('d', 3, config_info_t)
#define DS_GET_FIRST_TUPLE		_IOWR('d', 4, tuple_t)
#define DS_GET_NEXT_TUPLE		_IOWR('d', 5, tuple_t)
#define DS_GET_TUPLE_DATA		_IOWR('d', 6, tuple_parse_t)
#define DS_PARSE_TUPLE			_IOWR('d', 7, tuple_parse_t)
#define DS_RESET_CARD			_IO  ('d', 8)
#define DS_GET_STATUS			_IOWR('d', 9, cs_status_t)
#define DS_ACCESS_CONFIGURATION_REGISTER _IOWR('d', 10, conf_reg_t)
#define DS_VALIDATE_CIS			_IOR ('d', 11, cisinfo_t)
#define DS_SUSPEND_CARD			_IO  ('d', 12)
#define DS_RESUME_CARD			_IO  ('d', 13)
#define DS_EJECT_CARD			_IO  ('d', 14)
#define DS_INSERT_CARD			_IO  ('d', 15)
#define DS_GET_FIRST_REGION		_IOWR('d', 16, region_info_t)
#define DS_GET_NEXT_REGION		_IOWR('d', 17, region_info_t)
#define DS_REPLACE_CIS			_IOWR('d', 18, cisdump_t)
#define DS_GET_FIRST_WINDOW		_IOR ('d', 19, win_info_t)
#define DS_GET_NEXT_WINDOW		_IOWR('d', 20, win_info_t)
#define DS_GET_MEM_PAGE			_IOWR('d', 21, win_info_t)

#define DS_BIND_REQUEST			_IOWR('d', 60, bind_info_t)
#define DS_GET_DEVICE_INFO		_IOWR('d', 61, bind_info_t) 
#define DS_GET_NEXT_DEVICE		_IOWR('d', 62, bind_info_t) 
#define DS_UNBIND_REQUEST		_IOW ('d', 63, bind_info_t)
#define DS_BIND_MTD			_IOWR('d', 64, mtd_info_t)

#ifdef __KERNEL__

typedef struct dev_link_t {
    dev_node_t		*dev;
    u_int		state, open;
    wait_queue_head_t	pending;
    struct timer_list	release;
    client_handle_t	handle;
    io_req_t		io;
    irq_req_t		irq;
    config_req_t	conf;
    window_handle_t	win;
    void		*priv;
    struct dev_link_t	*next;
} dev_link_t;

/* Flags for device state */
#define DEV_PRESENT		0x01
#define DEV_CONFIG		0x02
#define DEV_STALE_CONFIG	0x04	/* release on close */
#define DEV_STALE_LINK		0x08	/* detach on release */
#define DEV_CONFIG_PENDING	0x10
#define DEV_RELEASE_PENDING	0x20
#define DEV_SUSPEND		0x40
#define DEV_BUSY		0x80

#define DEV_OK(l) \
    ((l) && ((l->state & ~DEV_BUSY) == (DEV_CONFIG|DEV_PRESENT)))

int register_pccard_driver(dev_info_t *dev_info,
			   dev_link_t *(*attach)(void),
			   void (*detach)(dev_link_t *));

int unregister_pccard_driver(dev_info_t *dev_info);

#define register_pcmcia_driver register_pccard_driver
#define unregister_pcmcia_driver unregister_pccard_driver

#endif /* __KERNEL__ */

#endif /* _LINUX_DS_H */

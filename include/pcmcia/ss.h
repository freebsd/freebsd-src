/*
 * ss.h 1.28 2000/06/12 21:55:40
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

#ifndef _LINUX_SS_H
#define _LINUX_SS_H

#include <pcmcia/cs_types.h>

/* Definitions for card status flags for GetStatus */
#define SS_WRPROT	0x0001
#define SS_CARDLOCK	0x0002
#define SS_EJECTION	0x0004
#define SS_INSERTION	0x0008
#define SS_BATDEAD	0x0010
#define SS_BATWARN	0x0020
#define SS_READY	0x0040
#define SS_DETECT	0x0080
#define SS_POWERON	0x0100
#define SS_GPI		0x0200
#define SS_STSCHG	0x0400
#define SS_CARDBUS	0x0800
#define SS_3VCARD	0x1000
#define SS_XVCARD	0x2000
#define SS_PENDING	0x4000

/* for InquireSocket */
typedef struct socket_cap_t {
    u_int	features;
    u_int	irq_mask;
    u_int	map_size;
    ioaddr_t	io_offset;
    u_char	pci_irq;
    struct pci_dev *cb_dev;
    struct bus_operations *bus;
} socket_cap_t;

/* InquireSocket capabilities */
#define SS_CAP_PAGE_REGS	0x0001
#define SS_CAP_VIRTUAL_BUS	0x0002
#define SS_CAP_MEM_ALIGN	0x0004
#define SS_CAP_STATIC_MAP	0x0008
#define SS_CAP_PCCARD		0x4000
#define SS_CAP_CARDBUS		0x8000

/* for GetSocket, SetSocket */
typedef struct socket_state_t {
    u_int	flags;
    u_int	csc_mask;
    u_char	Vcc, Vpp;
    u_char	io_irq;
} socket_state_t;

extern socket_state_t dead_socket;

/* Socket configuration flags */
#define SS_PWR_AUTO	0x0010
#define SS_IOCARD	0x0020
#define SS_RESET	0x0040
#define SS_DMA_MODE	0x0080
#define SS_SPKR_ENA	0x0100
#define SS_OUTPUT_ENA	0x0200
#define SS_DEBOUNCED	0x0400	/* Tell driver that the debounce delay has ended */
#define SS_ZVCARD	0x0800

/* Flags for I/O port and memory windows */
#define MAP_ACTIVE	0x01
#define MAP_16BIT	0x02
#define MAP_AUTOSZ	0x04
#define MAP_0WS		0x08
#define MAP_WRPROT	0x10
#define MAP_ATTRIB	0x20
#define MAP_USE_WAIT	0x40
#define MAP_PREFETCH	0x80

/* Use this just for bridge windows */
#define MAP_IOSPACE	0x20

typedef struct pccard_io_map {
    u_char	map;
    u_char	flags;
    u_short	speed;
    ioaddr_t	start, stop;
} pccard_io_map;

typedef struct pccard_mem_map {
    u_char	map;
    u_char	flags;
    u_short	speed;
    u_long	sys_start, sys_stop;
    u_int	card_start;
} pccard_mem_map;

typedef struct cb_bridge_map {
    u_char	map;
    u_char	flags;
    u_int	start, stop;
} cb_bridge_map;

/*
 * Socket operations.
 */
struct pccard_operations {
	int (*init)(unsigned int sock);
	int (*suspend)(unsigned int sock);
	int (*register_callback)(unsigned int sock, void (*handler)(void *, unsigned int), void * info);
	int (*inquire_socket)(unsigned int sock, socket_cap_t *cap);
	int (*get_status)(unsigned int sock, u_int *value);
	int (*get_socket)(unsigned int sock, socket_state_t *state);
	int (*set_socket)(unsigned int sock, socket_state_t *state);
	int (*get_io_map)(unsigned int sock, struct pccard_io_map *io);
	int (*set_io_map)(unsigned int sock, struct pccard_io_map *io);
	int (*get_mem_map)(unsigned int sock, struct pccard_mem_map *mem);
	int (*set_mem_map)(unsigned int sock, struct pccard_mem_map *mem);
	void (*proc_setup)(unsigned int sock, struct proc_dir_entry *base);
};

/*
 *  Calls to set up low-level "Socket Services" drivers
 */
extern int register_ss_entry(int nsock, struct pccard_operations *ops);
extern void unregister_ss_entry(struct pccard_operations *ops);

#endif /* _LINUX_SS_H */

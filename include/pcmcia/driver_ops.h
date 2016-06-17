/*
 * driver_ops.h 1.15 2000/06/12 21:55:40
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

#ifndef _LINUX_DRIVER_OPS_H
#define _LINUX_DRIVER_OPS_H

#ifndef DEV_NAME_LEN
#define DEV_NAME_LEN	32
#endif

#ifdef __KERNEL__

typedef struct dev_node_t {
    char		dev_name[DEV_NAME_LEN];
    u_short		major, minor;
    struct dev_node_t	*next;
} dev_node_t;

typedef struct dev_locator_t {
    enum { LOC_ISA, LOC_PCI } bus;
    union {
	struct {
	    u_short	io_base_1, io_base_2;
	    u_long	mem_base;
	    u_char	irq, dma;
	} isa;
	struct {
	    u_char	bus;
	    u_char	devfn;
	} pci;
    } b;
} dev_locator_t;

typedef struct driver_operations {
    char		*name;
    dev_node_t		*(*attach) (dev_locator_t *loc);
    void		(*suspend) (dev_node_t *dev);
    void		(*resume) (dev_node_t *dev);
    void		(*detach) (dev_node_t *dev);
} driver_operations;

int register_driver(struct driver_operations *ops);
void unregister_driver(struct driver_operations *ops);

#endif /* __KERNEL__ */

#endif /* _LINUX_DRIVER_OPS_H */

/*
 * Generic PCI pccard driver interface.
 *
 * (C) Copyright 1999 Linus Torvalds
 *
 * This implements the common parts of PCI pccard drivers,
 * notably detection and infrastructure conversion (ie change
 * from socket index to "struct pci_dev" etc)
 *
 * This does NOT implement the actual low-level driver details,
 * and this has on purpose been left generic enough that it can
 * be used to set up a PCI PCMCIA controller (ie non-cardbus),
 * or to set up a controller.
 *
 * See for example the "yenta" driver for PCI cardbus controllers
 * conforming to the yenta cardbus specifications.
 */
#include <linux/module.h>

#include <linux/init.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/interrupt.h>

#include <pcmcia/ss.h>

#include <asm/io.h>

#include "pci_socket.h"


extern struct socket_info_t *pcmcia_register_socket (int slot,
		struct pccard_operations *vtable, int use_bus_pm);
extern void pcmcia_unregister_socket (struct socket_info_t *socket);
extern void pcmcia_suspend_socket (struct socket_info_t *socket);
extern void pcmcia_resume_socket (struct socket_info_t *socket);


/*
 * Arbitrary define. This is the array of active cardbus
 * entries.
 */
#define MAX_SOCKETS (8)
static pci_socket_t pci_socket_array[MAX_SOCKETS];

static int pci_init_socket(unsigned int sock)
{
	pci_socket_t *socket = pci_socket_array + sock;

	if (socket->op && socket->op->init)
		return socket->op->init(socket);
	return -EINVAL;
}

static int pci_suspend_socket(unsigned int sock)
{
	pci_socket_t *socket = pci_socket_array + sock;

	if (socket->op && socket->op->suspend)
		return socket->op->suspend(socket);
	return -EINVAL;
}

static int pci_register_callback(unsigned int sock, void (*handler)(void *, unsigned int), void * info)
{
	pci_socket_t *socket = pci_socket_array + sock;

	socket->handler = handler;
	socket->info = info;
	if (handler)
		MOD_INC_USE_COUNT;
	else
		MOD_DEC_USE_COUNT;
	return 0;
}

static int pci_inquire_socket(unsigned int sock, socket_cap_t *cap)
{
	pci_socket_t *socket = pci_socket_array + sock;

	*cap = socket->cap;
	return 0;
}

static int pci_get_status(unsigned int sock, unsigned int *value)
{
	pci_socket_t *socket = pci_socket_array + sock;

	if (socket->op && socket->op->get_status)
		return socket->op->get_status(socket, value);
	*value = 0;
	return -EINVAL;
}

static int pci_get_socket(unsigned int sock, socket_state_t *state)
{
	pci_socket_t *socket = pci_socket_array + sock;

	if (socket->op && socket->op->get_socket)
		return socket->op->get_socket(socket, state);
	return -EINVAL;
}

static int pci_set_socket(unsigned int sock, socket_state_t *state)
{
	pci_socket_t *socket = pci_socket_array + sock;

	if (socket->op && socket->op->set_socket)
		return socket->op->set_socket(socket, state);
	return -EINVAL;
}

static int pci_get_io_map(unsigned int sock, struct pccard_io_map *io)
{
	pci_socket_t *socket = pci_socket_array + sock;

	if (socket->op && socket->op->get_io_map)
		return socket->op->get_io_map(socket, io);
	return -EINVAL;
}

static int pci_set_io_map(unsigned int sock, struct pccard_io_map *io)
{
	pci_socket_t *socket = pci_socket_array + sock;

	if (socket->op && socket->op->set_io_map)
		return socket->op->set_io_map(socket, io);
	return -EINVAL;
}

static int pci_get_mem_map(unsigned int sock, struct pccard_mem_map *mem)
{
	pci_socket_t *socket = pci_socket_array + sock;

	if (socket->op && socket->op->get_mem_map)
		return socket->op->get_mem_map(socket, mem);
	return -EINVAL;
}

static int pci_set_mem_map(unsigned int sock, struct pccard_mem_map *mem)
{
	pci_socket_t *socket = pci_socket_array + sock;

	if (socket->op && socket->op->set_mem_map)
		return socket->op->set_mem_map(socket, mem);
	return -EINVAL;
}

static void pci_proc_setup(unsigned int sock, struct proc_dir_entry *base)
{
	pci_socket_t *socket = pci_socket_array + sock;

	if (socket->op && socket->op->proc_setup)
		socket->op->proc_setup(socket, base);
}

static struct pccard_operations pci_socket_operations = {
	pci_init_socket,
	pci_suspend_socket,
	pci_register_callback,
	pci_inquire_socket,
	pci_get_status,
	pci_get_socket,
	pci_set_socket,
	pci_get_io_map,
	pci_set_io_map,
	pci_get_mem_map,
	pci_set_mem_map,
	pci_proc_setup
};

static int __devinit add_pci_socket(int nr, struct pci_dev *dev, struct pci_socket_ops *ops)
{
	pci_socket_t *socket = nr + pci_socket_array;
	int err;
	
	memset(socket, 0, sizeof(*socket));
	socket->dev = dev;
	socket->op = ops;
	pci_set_drvdata(dev, socket);
	spin_lock_init(&socket->event_lock);
	err = socket->op->open(socket);
	if(err)
	{
		socket->dev = NULL;
		pci_set_drvdata(dev, NULL);
	}
	return err;
}

void cardbus_register(pci_socket_t *socket)
{
	int nr = socket - pci_socket_array;

	socket->pcmcia_socket = pcmcia_register_socket(nr, &pci_socket_operations, 1);
}

static int __devinit
cardbus_probe (struct pci_dev *dev, const struct pci_device_id *id)
{
	int	s;

	for (s = 0; s < MAX_SOCKETS; s++) {
		if (pci_socket_array [s].dev == 0) {
			return add_pci_socket (s, dev, &yenta_operations);
		}
	}
	return -ENODEV;
}

static void __devexit cardbus_remove (struct pci_dev *dev)
{
	pci_socket_t *socket = pci_get_drvdata(dev);

	pcmcia_unregister_socket (socket->pcmcia_socket);
	if (socket->op && socket->op->close)
		socket->op->close(socket);
	pci_set_drvdata(dev, NULL);
}

static int cardbus_suspend (struct pci_dev *dev, u32 state)
{
	pci_socket_t *socket = pci_get_drvdata(dev);
	pcmcia_suspend_socket (socket->pcmcia_socket);
	return 0;
}

static int cardbus_resume (struct pci_dev *dev)
{
	pci_socket_t *socket = pci_get_drvdata(dev);
	pcmcia_resume_socket (socket->pcmcia_socket);
	return 0;
}


static struct pci_device_id cardbus_table [] __devinitdata = { {
	class:		PCI_CLASS_BRIDGE_CARDBUS << 8,
	class_mask:	~0,

	vendor:		PCI_ANY_ID,
	device:		PCI_ANY_ID,
	subvendor:	PCI_ANY_ID,
	subdevice:	PCI_ANY_ID,
}, { /* all zeroes */ }
};
MODULE_DEVICE_TABLE(pci, cardbus_table);

static struct pci_driver pci_cardbus_driver = {
	name:		"cardbus",
	id_table:	cardbus_table,
	probe:		cardbus_probe,
	remove:		__devexit_p(cardbus_remove),
	suspend:	cardbus_suspend,
	resume:		cardbus_resume,
};

static int __init pci_socket_init(void)
{
	return pci_module_init (&pci_cardbus_driver);
}

static void __exit pci_socket_exit (void)
{
	pci_unregister_driver (&pci_cardbus_driver);
}

module_init(pci_socket_init);
module_exit(pci_socket_exit);

/*
 * cs_internal.h 1.57 2002/10/24 06:11:43
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
 *  are Copyright (C) 1999 David A. Hinds.  All Rights Reserved.
 */

#ifndef _LINUX_CS_INTERNAL_H
#define _LINUX_CS_INTERNAL_H

#include <linux/config.h>

typedef struct erase_busy_t {
    eraseq_entry_t	*erase;
    client_handle_t	client;
    struct timer_list	timeout;
    struct erase_busy_t	*prev, *next;
} erase_busy_t;

#define ERASEQ_MAGIC	0xFA67
typedef struct eraseq_t {
    u_short		eraseq_magic;
    client_handle_t	handle;
    int			count;
    eraseq_entry_t	*entry;
} eraseq_t;

#define CLIENT_MAGIC 	0x51E6
typedef struct client_t {
    u_short		client_magic;
    socket_t		Socket;
    u_char		Function;
    dev_info_t		dev_info;
    u_int		Attributes;
    u_int		state;
    event_t		EventMask, PendingEvents;
    int (*event_handler)(event_t event, int priority,
			 event_callback_args_t *);
    event_callback_args_t event_callback_args;
    struct client_t 	*next;
    u_int		mtd_count;
    wait_queue_head_t	mtd_req;
    erase_busy_t	erase_busy;
} client_t;

/* Flags in client state */
#define CLIENT_CONFIG_LOCKED	0x0001
#define CLIENT_IRQ_REQ		0x0002
#define CLIENT_IO_REQ		0x0004
#define CLIENT_UNBOUND		0x0008
#define CLIENT_STALE		0x0010
#define CLIENT_WIN_REQ(i)	(0x20<<(i))
#define CLIENT_CARDBUS		0x8000

typedef struct io_window_t {
    u_int		Attributes;
    ioaddr_t		BasePort, NumPorts;
    ioaddr_t		InUse, Config;
} io_window_t;

#define WINDOW_MAGIC	0xB35C
typedef struct window_t {
    u_short		magic;
    u_short		index;
    client_handle_t	handle;
    struct socket_info_t *sock;
    u_long		base;
    u_long		size;
    pccard_mem_map	ctl;
} window_t;

#define REGION_MAGIC	0xE3C9
typedef struct region_t {
    u_short		region_magic;
    u_short		state;
    dev_info_t		dev_info;
    client_handle_t	mtd;
    u_int		MediaID;
    region_info_t	info;
} region_t;

#define REGION_STALE	0x01

/* Each card function gets one of these guys */
typedef struct config_t {
    u_int		state;
    u_int		Attributes;
    u_int		Vcc, Vpp1, Vpp2;
    u_int		IntType;
    u_int		ConfigBase;
    u_char		Status, Pin, Copy, Option, ExtStatus;
    u_int		Present;
    u_int		CardValues;
    io_req_t		io;
    struct {
	u_int		Attributes;
    } irq;
} config_t;

/* Maximum number of IO windows per socket */
#define MAX_IO_WIN 2

/* Maximum number of memory windows per socket */
#define MAX_WIN 4

/* The size of the CIS cache */
#define MAX_CIS_TABLE	64
#define MAX_CIS_DATA	512

typedef struct socket_info_t {
    spinlock_t			lock;
    struct pccard_operations *	ss_entry;
    u_int			sock;
    socket_state_t		socket;
    socket_cap_t		cap;
    u_int			state;
    u_short			functions;
    u_short			lock_count;
    client_handle_t		clients;
    u_int			real_clients;
    client_handle_t		reset_handle;
    pccard_mem_map		cis_mem;
    u_char			*cis_virt;
    config_t			*config;
#ifdef CONFIG_CARDBUS
    struct resource *		cb_cis_res;
    u_char			*cb_cis_virt;
    struct cb_config_t		*cb_config;
#endif
    struct {
	u_int			AssignedIRQ;
	u_int			Config;
    } irq;
    io_window_t			io[MAX_IO_WIN];
    window_t			win[MAX_WIN];
    region_t			*c_region, *a_region;
    erase_busy_t		erase_busy;
    int				cis_used;
    struct {
	u_int			addr;
	u_short			len;
	u_short			attr;
    }				cis_table[MAX_CIS_TABLE];
    char			cis_cache[MAX_CIS_DATA];
    u_int			fake_cis_len;
    char			*fake_cis;
#ifdef CONFIG_PROC_FS
    struct proc_dir_entry	*proc;
#endif
    int				use_bus_pm;
} socket_info_t;

/* Flags in config state */
#define CONFIG_LOCKED		0x01
#define CONFIG_IRQ_REQ		0x02
#define CONFIG_IO_REQ		0x04

/* Flags in socket state */
#define SOCKET_PRESENT		0x0008
#define SOCKET_SETUP_PENDING	0x0010
#define SOCKET_SHUTDOWN_PENDING	0x0020
#define SOCKET_RESET_PENDING	0x0040
#define SOCKET_SUSPEND		0x0080
#define SOCKET_WIN_REQ(i)	(0x0100<<(i))
#define SOCKET_IO_REQ(i)	(0x1000<<(i))
#define SOCKET_REGION_INFO	0x4000
#define SOCKET_CARDBUS		0x8000

#define CHECK_HANDLE(h) \
    (((h) == NULL) || ((h)->client_magic != CLIENT_MAGIC))

#define CHECK_SOCKET(s) \
    (((s) >= sockets) || (socket_table[s]->ss_entry == NULL))

#define SOCKET(h) (socket_table[(h)->Socket])
#define CONFIG(h) (&SOCKET(h)->config[(h)->Function])

#define CHECK_REGION(r) \
    (((r) == NULL) || ((r)->region_magic != REGION_MAGIC))

#define CHECK_ERASEQ(q) \
    (((q) == NULL) || ((q)->eraseq_magic != ERASEQ_MAGIC))

#define EVENT(h, e, p) \
    ((h)->event_handler((e), (p), &(h)->event_callback_args))

/* In cardbus.c */
int cb_alloc(socket_info_t *s);
void cb_free(socket_info_t *s);
int cb_config(socket_info_t *s);
void cb_release(socket_info_t *s);
void cb_enable(socket_info_t *s);
void cb_disable(socket_info_t *s);
int read_cb_mem(socket_info_t *s, u_char fn, int space,
		u_int addr, u_int len, void *ptr);
void cb_release_cis_mem(socket_info_t *s);

/* In cistpl.c */
int read_cis_mem(socket_info_t *s, int attr,
		 u_int addr, u_int len, void *ptr);
void write_cis_mem(socket_info_t *s, int attr,
		   u_int addr, u_int len, void *ptr);
void release_cis_mem(socket_info_t *s);
int verify_cis_cache(socket_info_t *s);
void preload_cis_cache(socket_info_t *s);
int get_first_tuple(client_handle_t handle, tuple_t *tuple);
int get_next_tuple(client_handle_t handle, tuple_t *tuple);
int get_tuple_data(client_handle_t handle, tuple_t *tuple);
int parse_tuple(client_handle_t handle, tuple_t *tuple, cisparse_t *parse);
int validate_cis(client_handle_t handle, cisinfo_t *info);
int replace_cis(client_handle_t handle, cisdump_t *cis);
int read_tuple(client_handle_t handle, cisdata_t code, void *parse);

/* In bulkmem.c */
void retry_erase_list(struct erase_busy_t *list, u_int cause);
int get_first_region(client_handle_t handle, region_info_t *rgn);
int get_next_region(client_handle_t handle, region_info_t *rgn);
int register_mtd(client_handle_t handle, mtd_reg_t *reg);
int register_erase_queue(client_handle_t *handle, eraseq_hdr_t *header);
int deregister_erase_queue(eraseq_handle_t eraseq);
int check_erase_queue(eraseq_handle_t eraseq);
int open_memory(client_handle_t *handle, open_mem_t *open);
int close_memory(memory_handle_t handle);
int read_memory(memory_handle_t handle, mem_op_t *req, caddr_t buf);
int write_memory(memory_handle_t handle, mem_op_t *req, caddr_t buf);
int copy_memory(memory_handle_t handle, copy_op_t *req);

/* In rsrc_mgr */
void validate_mem(int (*is_valid)(u_long), int (*do_cksum)(u_long),
		  int force_low, socket_info_t *s);
int find_io_region(ioaddr_t *base, ioaddr_t num, ioaddr_t align,
		   char *name, socket_info_t *s);
int find_mem_region(u_long *base, u_long num, u_long align,
		    int force_low, char *name, socket_info_t *s);
int try_irq(u_int Attributes, int irq, int specific);
void undo_irq(u_int Attributes, int irq);
int adjust_resource_info(client_handle_t handle, adjust_t *adj);
void release_resource_db(void);
int proc_read_io(char *buf, char **start, off_t pos,
		 int count, int *eof, void *data);
int proc_read_mem(char *buf, char **start, off_t pos,
		  int count, int *eof, void *data);

#define MAX_SOCK 8
extern socket_t sockets;
extern socket_info_t *socket_table[MAX_SOCK];

#ifdef CONFIG_PROC_FS
extern struct proc_dir_entry *proc_pccard;
#endif

#ifdef PCMCIA_DEBUG
extern int pc_debug;
#define DEBUG(n, args...) do { if (pc_debug>(n)) printk(KERN_DEBUG args); } while (0)
#else
#define DEBUG(n, args...) do { } while (0)
#endif

#endif /* _LINUX_CS_INTERNAL_H */

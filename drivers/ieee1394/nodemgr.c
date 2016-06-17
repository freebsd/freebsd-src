/*
 * Node information (ConfigROM) collection and management.
 *
 * Copyright (C) 2000		Andreas E. Bombe
 *               2001-2003	Ben Collins <bcollins@debian.net>
 *
 * This code is licensed under the GPL.  See the file COPYING in the root
 * directory of the kernel sources for details.
 */

#include <linux/kernel.h>
#include <linux/config.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/interrupt.h>
#include <linux/kmod.h>
#include <linux/completion.h>
#include <linux/delay.h>
#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif

#include "ieee1394_types.h"
#include "ieee1394.h"
#include "nodemgr.h"
#include "hosts.h"
#include "ieee1394_transactions.h"
#include "highlevel.h"
#include "csr.h"
#include "nodemgr.h"



static char *nodemgr_find_oui_name(int oui)
{
#ifdef CONFIG_IEEE1394_OUI_DB
	extern struct oui_list_struct {
		int oui;
		char *name;
	} oui_list[];
	int i;

	for (i = 0; oui_list[i].name; i++)
		if (oui_list[i].oui == oui)
			return oui_list[i].name;
#endif
	return NULL;
}


/* 
 * Basically what we do here is start off retrieving the bus_info block.
 * From there will fill in some info about the node, verify it is of IEEE
 * 1394 type, and that the crc checks out ok. After that we start off with
 * the root directory, and subdirectories. To do this, we retrieve the
 * quadlet header for a directory, find out the length, and retrieve the
 * complete directory entry (be it a leaf or a directory). We then process
 * it and add the info to our structure for that particular node.
 *
 * We verify CRC's along the way for each directory/block/leaf. The entire
 * node structure is generic, and simply stores the information in a way
 * that's easy to parse by the protocol interface.
 */

/* The nodemgr maintains a number of data structures: the node list,
 * the driver list, unit directory list and the host info list.  The
 * first three lists are accessed from process context only: /proc
 * readers, insmod and rmmod, and the nodemgr thread.  Access to these
 * lists are serialized by means of the nodemgr_serialize mutex, which
 * must be taken before accessing the structures and released
 * afterwards.  The host info list is only accessed during insmod,
 * rmmod and from interrupt and allways only for a short period of
 * time, so a spinlock is used to protect this list.
 */

static DECLARE_MUTEX(nodemgr_serialize);
static LIST_HEAD(node_list);
static LIST_HEAD(driver_list);
static LIST_HEAD(unit_directory_list);


struct host_info {
	struct hpsb_host *host;
	struct completion exited;
	struct semaphore reset_sem;
	int pid;
	char daemon_name[15];
};

static struct hpsb_highlevel nodemgr_highlevel;

#ifdef CONFIG_PROC_FS

#define PUTF(fmt, args...)				\
do {							\
	len += sprintf(page + len, fmt, ## args);	\
	pos = begin + len;				\
	if (pos < off) {				\
		len = 0;				\
		begin = pos;				\
	}						\
	if (pos > off + count)				\
		goto done_proc;				\
} while (0)


static int raw1394_read_proc(char *page, char **start, off_t off,
			     int count, int *eof, void *data)
{
	struct list_head *lh;
	struct node_entry *ne;
	off_t begin = 0, pos = 0;
	int len = 0;

	if (down_interruptible(&nodemgr_serialize))
		return -EINTR;

	list_for_each(lh, &node_list) {
		struct list_head *l;
		int ud_count = 0, lud_count = 0;

		ne = list_entry(lh, struct node_entry, list);
		if (!ne)
			continue;

		PUTF("Node[" NODE_BUS_FMT "]  GUID[%016Lx]:\n",
		     NODE_BUS_ARGS(ne->host, ne->nodeid), (unsigned long long)ne->guid);

		/* Generic Node information */
		PUTF("  Vendor ID: `%s' [0x%06x]\n",
		     ne->vendor_name ?: "Unknown", ne->vendor_id);
		PUTF("  Capabilities: 0x%06x\n", ne->capabilities);
		PUTF("  Bus Options:\n");
		PUTF("    IRMC(%d) CMC(%d) ISC(%d) BMC(%d) PMC(%d) GEN(%d)\n"
		     "    LSPD(%d) MAX_REC(%d) CYC_CLK_ACC(%d)\n",
		     ne->busopt.irmc, ne->busopt.cmc, ne->busopt.isc, ne->busopt.bmc,
		     ne->busopt.pmc, ne->busopt.generation, ne->busopt.lnkspd,
		     ne->busopt.max_rec, ne->busopt.cyc_clk_acc);

		/* If this is the host entry, output some info about it aswell */
		if (ne->host != NULL && ne->host->node_id == ne->nodeid) {
			PUTF("  Host Node Status:\n");
			PUTF("    Host Driver     : %s\n", ne->host->driver->name);
			PUTF("    Nodes connected : %d\n", ne->host->node_count);
			PUTF("    Nodes active    : %d\n", ne->host->nodes_active);
			PUTF("    SelfIDs received: %d\n", ne->host->selfid_count);
			PUTF("    Irm ID          : [" NODE_BUS_FMT "]\n",
			     NODE_BUS_ARGS(ne->host, ne->host->irm_id));
			PUTF("    BusMgr ID       : [" NODE_BUS_FMT "]\n",
			     NODE_BUS_ARGS(ne->host, ne->host->busmgr_id));
			PUTF("    In Bus Reset    : %s\n", ne->host->in_bus_reset ? "yes" : "no");
			PUTF("    Root            : %s\n", ne->host->is_root ? "yes" : "no");
			PUTF("    Cycle Master    : %s\n", ne->host->is_cycmst ? "yes" : "no");
			PUTF("    IRM             : %s\n", ne->host->is_irm ? "yes" : "no");
			PUTF("    Bus Manager     : %s\n", ne->host->is_busmgr ? "yes" : "no");
		}

		/* Now the unit directories */
		list_for_each (l, &ne->unit_directories) {
			struct unit_directory *ud = list_entry (l, struct unit_directory, node_list);
			int printed = 0; // small hack

			if (ud->parent == NULL)
				PUTF("  Unit Directory %d:\n", lud_count++);
			else
				PUTF("  Logical Unit Directory %d:\n", ud_count++);
			if (ud->flags & UNIT_DIRECTORY_VENDOR_ID) {
				PUTF("    Vendor/Model ID: %s [%06x]",
				     ud->vendor_name ?: "Unknown", ud->vendor_id);
				printed = 1;
			}
			if (ud->flags & UNIT_DIRECTORY_MODEL_ID) {
				if (!printed)
					PUTF("    Vendor/Model ID: %s [%06x]",
					     ne->vendor_name ?: "Unknown", ne->vendor_id);
				PUTF(" / %s [%06x]", ud->model_name ?: "Unknown", ud->model_id);
				printed = 1;
			}
			if (printed)
				PUTF("\n");

			if (ud->flags & UNIT_DIRECTORY_SPECIFIER_ID)
				PUTF("    Software Specifier ID: %06x\n", ud->specifier_id);
			if (ud->flags & UNIT_DIRECTORY_VERSION)
				PUTF("    Software Version: %06x\n", ud->version);
			if (ud->driver)
				PUTF("    Driver: %s\n", ud->driver->name);
			PUTF("    Length (in quads): %d\n", ud->length);
		}

	}

done_proc:
	up(&nodemgr_serialize);

	*start = page + (off - begin);
	len -= (off - begin);
	if (len > count)
		len = count;
	else {
		*eof = 1;
		if (len <= 0)
			return 0;
	}

	return len;
}

#undef PUTF
#endif /* CONFIG_PROC_FS */

static void nodemgr_process_config_rom(struct node_entry *ne, 
				       quadlet_t busoptions);

static int nodemgr_read_quadlet(struct hpsb_host *host,
				nodeid_t nodeid, unsigned int generation,
				octlet_t address, quadlet_t *quad)
{
	int i;
	int ret = 0;

	for (i = 0; i < 3; i++) {
		ret = hpsb_read(host, nodeid, generation, address, quad, 4);
		if (!ret)
			break;

		set_current_state(TASK_INTERRUPTIBLE);
		if (schedule_timeout (HZ/3))
			return -1;
	}
	*quad = be32_to_cpu(*quad);

	return ret;
}

static int nodemgr_size_text_leaf(struct hpsb_host *host,
				  nodeid_t nodeid, unsigned int generation,
				  octlet_t address)
{
	quadlet_t quad;
	int size = 0;

	if (nodemgr_read_quadlet(host, nodeid, generation, address, &quad))
		return -1;

	if (CONFIG_ROM_KEY(quad) == CONFIG_ROM_DESCRIPTOR_LEAF) {
		/* This is the offset.  */
		address += 4 * CONFIG_ROM_VALUE(quad); 
		if (nodemgr_read_quadlet(host, nodeid, generation, address, &quad))
			return -1;
		/* Now we got the size of the text descriptor leaf. */
		size = CONFIG_ROM_LEAF_LENGTH(quad);
	}

	return size;
}

static int nodemgr_read_text_leaf(struct node_entry *ne,
				  octlet_t address,
				  quadlet_t *quadp)
{
	quadlet_t quad;
	int i, size, ret;

	if (nodemgr_read_quadlet(ne->host, ne->nodeid, ne->generation, address, &quad)
	    || CONFIG_ROM_KEY(quad) != CONFIG_ROM_DESCRIPTOR_LEAF)
		return -1;

	/* This is the offset.  */
	address += 4 * CONFIG_ROM_VALUE(quad);
	if (nodemgr_read_quadlet(ne->host, ne->nodeid, ne->generation, address, &quad))
		return -1;

	/* Now we got the size of the text descriptor leaf. */
	size = CONFIG_ROM_LEAF_LENGTH(quad) - 2;
	if (size <= 0)
		return -1;

	address += 4;
	for (i = 0; i < 2; i++, address += 4, quadp++) {
		if (nodemgr_read_quadlet(ne->host, ne->nodeid, ne->generation, address, quadp))
			return -1;
	}

	/* Now read the text string.  */
	ret = -ENXIO;
	for (; size > 0; size--, address += 4, quadp++) {
		for (i = 0; i < 3; i++) {
			ret = hpsb_node_read(ne, address, quadp, 4);
			if (ret != -EAGAIN)
				break;
		}
		if (ret)
			break;
	}

	return ret;
}

static struct node_entry *nodemgr_scan_root_directory
	(struct hpsb_host *host, nodeid_t nodeid, unsigned int generation)
{
	octlet_t address;
	quadlet_t quad;
	int length;
	int code, size, total_size;
	struct node_entry *ne;

	address = CSR_REGISTER_BASE + CSR_CONFIG_ROM;
	
	if (nodemgr_read_quadlet(host, nodeid, generation, address, &quad))
		return NULL;

	if (CONFIG_ROM_BUS_INFO_LENGTH(quad) == 1)  /* minimal config rom */
		return NULL;

	address += 4 + CONFIG_ROM_BUS_INFO_LENGTH(quad) * 4;

	if (nodemgr_read_quadlet(host, nodeid, generation, address, &quad))
		return NULL;
	length = CONFIG_ROM_ROOT_LENGTH(quad);
	address += 4;

	size = 0;
	total_size = sizeof(struct node_entry);
	for (; length > 0; length--, address += 4) {
		if (nodemgr_read_quadlet(host, nodeid, generation, address, &quad))
			return NULL;
		code = CONFIG_ROM_KEY(quad);

		if (code == CONFIG_ROM_VENDOR_ID && length > 0) {
			/* Check if there is a text descriptor leaf
			   immediately after this.  */
			size = nodemgr_size_text_leaf(host, nodeid, generation,
						      address + 4);
			if (size > 0) {
				address += 4;
				length--;
				total_size += (size + 1) * sizeof (quadlet_t);
			} else if (size < 0)
				return NULL;
		}
	}
	ne = kmalloc(total_size, GFP_KERNEL);

	if (!ne)
		return NULL;

	memset(ne, 0, total_size);

	if (size != 0) {
		ne->vendor_name = (const char *) &(ne->quadlets[2]);
		ne->quadlets[size] = 0;
	} else {
		ne->vendor_name = NULL;
	}

	return ne; 
}

static struct node_entry *nodemgr_create_node(octlet_t guid, quadlet_t busoptions,
					      struct host_info *hi,
					      nodeid_t nodeid, unsigned int generation)
{
	struct hpsb_host *host = hi->host;
        struct node_entry *ne;

	ne = nodemgr_scan_root_directory (host, nodeid, generation);
        if (!ne) return NULL;

        INIT_LIST_HEAD(&ne->list);
	INIT_LIST_HEAD(&ne->unit_directories);
        ne->host = host;
        ne->nodeid = nodeid;
	ne->generation = generation;
	ne->guid = guid;
	ne->guid_vendor_id = (guid >> 40) & 0xffffff;
	ne->guid_vendor_oui = nodemgr_find_oui_name(ne->guid_vendor_id);

        list_add_tail(&ne->list, &node_list);

	nodemgr_process_config_rom (ne, busoptions);

	HPSB_DEBUG("%s added: ID:BUS[" NODE_BUS_FMT "]  GUID[%016Lx]",
		   (host->node_id == nodeid) ? "Host" : "Node",
		   NODE_BUS_ARGS(host, nodeid), (unsigned long long)guid);

        return ne;
}

static struct node_entry *find_entry_by_guid(u64 guid)
{
        struct list_head *lh;
        struct node_entry *ne;
        
        list_for_each(lh, &node_list) {
                ne = list_entry(lh, struct node_entry, list);
                if (ne->guid == guid) return ne;
        }

        return NULL;
}

static struct node_entry *find_entry_by_nodeid(struct hpsb_host *host, nodeid_t nodeid)
{
	struct list_head *lh;
	struct node_entry *ne;

	list_for_each(lh, &node_list) {
		ne = list_entry(lh, struct node_entry, list);
		if (ne->nodeid == nodeid && ne->host == host)
			return ne;
	}

	return NULL;
}

static struct unit_directory *nodemgr_scan_unit_directory
	(struct node_entry *ne, octlet_t address)
{
	struct unit_directory *ud;
	quadlet_t quad;
	u8 flags, todo;
	int length, size, total_size, count;
	int vendor_name_size, model_name_size;

	if (nodemgr_read_quadlet(ne->host, ne->nodeid, ne->generation, address, &quad))
		return NULL;
	length = CONFIG_ROM_DIRECTORY_LENGTH(quad) ;
	address += 4;

	size = 0;
	total_size = sizeof (struct unit_directory);
	flags = 0;
	count = 0;
	vendor_name_size = 0;
	model_name_size = 0;
	for (; length > 0; length--, address += 4) {
		int code;
		quadlet_t value;

		if (nodemgr_read_quadlet(ne->host, ne->nodeid, ne->generation,
					 address, &quad))
			return NULL;
		code = CONFIG_ROM_KEY(quad);
		value = CONFIG_ROM_VALUE(quad);

		todo = 0;
		switch (code) {
		case CONFIG_ROM_VENDOR_ID:
			todo = UNIT_DIRECTORY_VENDOR_TEXT;
			break;

		case CONFIG_ROM_MODEL_ID:
			todo = UNIT_DIRECTORY_MODEL_TEXT;
			break;

		case CONFIG_ROM_SPECIFIER_ID:
		case CONFIG_ROM_UNIT_SW_VERSION:
			break;

		case CONFIG_ROM_DESCRIPTOR_LEAF:
		case CONFIG_ROM_DESCRIPTOR_DIRECTORY:
			/* TODO: read strings... icons? */
			break;

		default:
			/* Which types of quadlets do we want to
			   store?  Only count immediate values and
			   CSR offsets for now.  */
			code &= CONFIG_ROM_KEY_TYPE_MASK;
			if ((code & CONFIG_ROM_KEY_TYPE_LEAF) == 0)
				count++;
			break;
		}

		if (todo && length > 0) {
			/* Check if there is a text descriptor leaf
			   immediately after this.  */
			size = nodemgr_size_text_leaf(ne->host,
						      ne->nodeid,
						      ne->generation,
						      address + 4);

			if (todo == UNIT_DIRECTORY_VENDOR_TEXT)
				vendor_name_size = size;
			else
				model_name_size = size;

			if (size > 0) {
				address += 4;
				length--;
				flags |= todo;
				total_size += (size + 1) * sizeof (quadlet_t);
			}
			else if (size < 0)
				return NULL;
		}
	}

	total_size += count * sizeof (quadlet_t);
	ud = kmalloc (total_size, GFP_KERNEL);

	if (ud != NULL) {
		memset (ud, 0, total_size);
		ud->flags = flags;
		ud->length = count;
		ud->vendor_name_size = vendor_name_size;
		ud->model_name_size = model_name_size;
	}

	return ud;
}


/* This implementation currently only scans the config rom and its
 * immediate unit directories looking for software_id and
 * software_version entries, in order to get driver autoloading working. */
static struct unit_directory * nodemgr_process_unit_directory
	(struct node_entry *ne, octlet_t address, struct unit_directory *parent)
{
	struct unit_directory *ud;
	quadlet_t quad;
	quadlet_t *infop;
	int length;
	struct unit_directory *ud_temp = NULL;

	if (!(ud = nodemgr_scan_unit_directory(ne, address)))
		goto unit_directory_error;

	ud->ne = ne;
	ud->address = address;

	if (parent) {
		ud->flags |= UNIT_DIRECTORY_LUN_DIRECTORY;
		ud->parent = parent;
	}

	if (nodemgr_read_quadlet(ne->host, ne->nodeid, ne->generation,
				 address, &quad))
		goto unit_directory_error;
	length = CONFIG_ROM_DIRECTORY_LENGTH(quad) ;
	address += 4;

	infop = (quadlet_t *) ud->quadlets;
	for (; length > 0; length--, address += 4) {
		int code;
		quadlet_t value;
		quadlet_t *quadp;

		if (nodemgr_read_quadlet(ne->host, ne->nodeid, ne->generation,
					 address, &quad))
			goto unit_directory_error;
		code = CONFIG_ROM_KEY(quad) ;
		value = CONFIG_ROM_VALUE(quad);

		switch (code) {
		case CONFIG_ROM_VENDOR_ID:
			ud->vendor_id = value;
			ud->flags |= UNIT_DIRECTORY_VENDOR_ID;

			if (ud->vendor_id)
				ud->vendor_oui = nodemgr_find_oui_name(ud->vendor_id);

			if ((ud->flags & UNIT_DIRECTORY_VENDOR_TEXT) != 0) {
				length--;
				address += 4;
				quadp = &(ud->quadlets[ud->length]);
				if (nodemgr_read_text_leaf(ne, address, quadp) == 0
				    && quadp[0] == 0 && quadp[1] == 0) {
				    	/* We only support minimal
					   ASCII and English. */
					quadp[ud->vendor_name_size] = 0;
					ud->vendor_name
						= (const char *) &(quadp[2]);
				}
			}
			break;

		case CONFIG_ROM_MODEL_ID:
			ud->model_id = value;
			ud->flags |= UNIT_DIRECTORY_MODEL_ID;
			if ((ud->flags & UNIT_DIRECTORY_MODEL_TEXT) != 0) {
				length--;
				address += 4;
				quadp = &(ud->quadlets[ud->length + ud->vendor_name_size + 1]);
				if (nodemgr_read_text_leaf(ne, address, quadp) == 0
				    && quadp[0] == 0 && quadp[1] == 0) {
				    	/* We only support minimal
					   ASCII and English. */
					quadp[ud->model_name_size] = 0;
					ud->model_name
						= (const char *) &(quadp[2]);
				}
			}
			break;

		case CONFIG_ROM_SPECIFIER_ID:
			ud->specifier_id = value;
			ud->flags |= UNIT_DIRECTORY_SPECIFIER_ID;
			break;

		case CONFIG_ROM_UNIT_SW_VERSION:
			ud->version = value;
			ud->flags |= UNIT_DIRECTORY_VERSION;
			break;

		case CONFIG_ROM_DESCRIPTOR_LEAF:
		case CONFIG_ROM_DESCRIPTOR_DIRECTORY:
			/* TODO: read strings... icons? */
			break;

		case CONFIG_ROM_LOGICAL_UNIT_DIRECTORY:
			ud->flags |= UNIT_DIRECTORY_HAS_LUN_DIRECTORY;
			ud_temp = nodemgr_process_unit_directory(ne, address + value * 4, ud);

			if (ud_temp == NULL)
				break;

			/* inherit unspecified values */
			if ((ud->flags & UNIT_DIRECTORY_VENDOR_ID) &&
				!(ud_temp->flags & UNIT_DIRECTORY_VENDOR_ID))
			{
				ud_temp->flags |=  UNIT_DIRECTORY_VENDOR_ID;
				ud_temp->vendor_id = ud->vendor_id;
			}
			if ((ud->flags & UNIT_DIRECTORY_MODEL_ID) &&
				!(ud_temp->flags & UNIT_DIRECTORY_MODEL_ID))
			{
				ud_temp->flags |=  UNIT_DIRECTORY_MODEL_ID;
				ud_temp->model_id = ud->model_id;
			}
			if ((ud->flags & UNIT_DIRECTORY_SPECIFIER_ID) &&
				!(ud_temp->flags & UNIT_DIRECTORY_SPECIFIER_ID))
			{
				ud_temp->flags |=  UNIT_DIRECTORY_SPECIFIER_ID;
				ud_temp->specifier_id = ud->specifier_id;
			}
			if ((ud->flags & UNIT_DIRECTORY_VERSION) &&
				!(ud_temp->flags & UNIT_DIRECTORY_VERSION))
			{
				ud_temp->flags |=  UNIT_DIRECTORY_VERSION;
				ud_temp->version = ud->version;
			}

			break;

		default:
			/* Which types of quadlets do we want to
			   store?  Only count immediate values and
			   CSR offsets for now.  */
			code &= CONFIG_ROM_KEY_TYPE_MASK;
			if ((code & CONFIG_ROM_KEY_TYPE_LEAF) == 0)
				*infop++ = quad;
			break;
		}
	}

	list_add_tail(&ud->node_list, &ne->unit_directories);
	list_add_tail(&ud->driver_list, &unit_directory_list);

	return ud;

unit_directory_error:	
	if (ud != NULL)
		kfree(ud);
	return NULL;
}


static void nodemgr_process_root_directory(struct node_entry *ne)
{
	octlet_t address;
	quadlet_t quad;
	int length;

	address = CSR_REGISTER_BASE + CSR_CONFIG_ROM;
	
	if (nodemgr_read_quadlet(ne->host, ne->nodeid, ne->generation,
				 address, &quad))
		return;
	address += 4 + CONFIG_ROM_BUS_INFO_LENGTH(quad) * 4;

	if (nodemgr_read_quadlet(ne->host, ne->nodeid, ne->generation,
				 address, &quad))
		return;
	length = CONFIG_ROM_ROOT_LENGTH(quad);
	address += 4;

	for (; length > 0; length--, address += 4) {
		int code, value;

		if (nodemgr_read_quadlet(ne->host, ne->nodeid, ne->generation,
					 address, &quad))
			return;
		code = CONFIG_ROM_KEY(quad);
		value = CONFIG_ROM_VALUE(quad);

		switch (code) {
		case CONFIG_ROM_VENDOR_ID:
			ne->vendor_id = value;

			if (ne->vendor_id)
				ne->vendor_oui = nodemgr_find_oui_name(ne->vendor_id);

			/* Now check if there is a vendor name text
			   string.  */
			if (ne->vendor_name != NULL) {
				length--;
				address += 4;
				if (nodemgr_read_text_leaf(ne, address, ne->quadlets) != 0
				    || ne->quadlets[0] != 0 || ne->quadlets[1] != 0)
				    	/* We only support minimal
					   ASCII and English. */
					ne->vendor_name = NULL;
			}
			break;

		case CONFIG_ROM_NODE_CAPABILITES:
			ne->capabilities = value;
			break;

		case CONFIG_ROM_UNIT_DIRECTORY:
			nodemgr_process_unit_directory(ne, address + value * 4, NULL);
			break;			

		case CONFIG_ROM_DESCRIPTOR_LEAF:
		case CONFIG_ROM_DESCRIPTOR_DIRECTORY:
			/* TODO: read strings... icons? */
			break;
		}
	}
}

#ifdef CONFIG_HOTPLUG

static void nodemgr_call_policy(char *verb, struct unit_directory *ud)
{
	char *argv [3], **envp, *buf, *scratch;
	int i = 0, value;

	if (!hotplug_path [0])
		return;
	if (!current->fs->root)
		return;
	if (!(envp = (char **) kmalloc(20 * sizeof (char *), GFP_KERNEL))) {
		HPSB_DEBUG ("ENOMEM");
		return;
	}
	if (!(buf = kmalloc(256, GFP_KERNEL))) {
		kfree(envp);
		HPSB_DEBUG("ENOMEM2");
		return;
	}

	/* only one standardized param to hotplug command: type */
	argv[0] = hotplug_path;
	argv[1] = "ieee1394";
	argv[2] = 0;

	/* minimal command environment */
	envp[i++] = "HOME=/";
	envp[i++] = "PATH=/sbin:/bin:/usr/sbin:/usr/bin";

#ifdef CONFIG_IEEE1394_VERBOSEDEBUG
	/* hint that policy agent should enter no-stdout debug mode */
	envp[i++] = "DEBUG=kernel";
#endif
	/* extensible set of named bus-specific parameters,
	 * supporting multiple driver selection algorithms.
	 */
	scratch = buf;

	envp[i++] = scratch;
	scratch += sprintf(scratch, "ACTION=%s", verb) + 1;
	envp[i++] = scratch;
	scratch += sprintf(scratch, "VENDOR_ID=%06x", ud->vendor_id) + 1;
	envp[i++] = scratch;
	scratch += sprintf(scratch, "GUID=%016Lx", (long long unsigned)ud->ne->guid) + 1;
	envp[i++] = scratch;
	scratch += sprintf(scratch, "SPECIFIER_ID=%06x", ud->specifier_id) + 1;
	envp[i++] = scratch;
	scratch += sprintf(scratch, "VERSION=%06x", ud->version) + 1;
	envp[i++] = 0;

	/* NOTE: user mode daemons can call the agents too */
	HPSB_VERBOSE("NodeMgr: %s %s %016Lx", argv[0], verb, (long long unsigned)ud->ne->guid);

	value = call_usermodehelper(argv[0], argv, envp);
	kfree(buf);
	kfree(envp);
	if (value != 0)
		HPSB_DEBUG("NodeMgr: hotplug policy returned %d", value);
}

#else

static inline void
nodemgr_call_policy(char *verb, struct unit_directory *ud)
{
	HPSB_VERBOSE("NodeMgr: nodemgr_call_policy(): hotplug not enabled");
	return;
} 

#endif /* CONFIG_HOTPLUG */

static void nodemgr_claim_unit_directory(struct unit_directory *ud,
					 struct hpsb_protocol_driver *driver)
{
	ud->driver = driver;
	list_move_tail(&ud->driver_list, &driver->unit_directories);
}

static void nodemgr_release_unit_directory(struct unit_directory *ud)
{
	ud->driver = NULL;
	list_move_tail(&ud->driver_list, &unit_directory_list);
}

void hpsb_release_unit_directory(struct unit_directory *ud)
{
	down(&nodemgr_serialize);
	nodemgr_release_unit_directory(ud);
	up(&nodemgr_serialize);
}

static void nodemgr_free_unit_directories(struct node_entry *ne)
{
	struct list_head *lh, *next;
	struct unit_directory *ud;

	list_for_each_safe(lh, next, &ne->unit_directories) {
		ud = list_entry(lh, struct unit_directory, node_list);

		if (ud->driver && ud->driver->disconnect)
			ud->driver->disconnect(ud);

		nodemgr_release_unit_directory(ud);
		nodemgr_call_policy("remove", ud);

		list_del(&ud->driver_list);
		list_del(&ud->node_list);

		kfree(ud);
	}
}

static struct ieee1394_device_id *
nodemgr_match_driver(struct hpsb_protocol_driver *driver, 
		     struct unit_directory *ud)
{
	struct ieee1394_device_id *id;

	for (id = driver->id_table; id->match_flags != 0; id++) {
		if ((id->match_flags & IEEE1394_MATCH_VENDOR_ID) &&
		    id->vendor_id != ud->vendor_id)
			continue;

		if ((id->match_flags & IEEE1394_MATCH_MODEL_ID) &&
		    id->model_id != ud->model_id)
			continue;

		if ((id->match_flags & IEEE1394_MATCH_SPECIFIER_ID) &&
		    id->specifier_id != ud->specifier_id)
			continue;

		/* software version does a bitwise comparison instead of equality */
		if ((id->match_flags & IEEE1394_MATCH_VERSION) &&
		    !(id->version & ud->version))
			continue;

		return id;
	}

	return NULL;
}

static struct hpsb_protocol_driver *
nodemgr_find_driver(struct unit_directory *ud)
{
	struct list_head *l;
	struct hpsb_protocol_driver *match, *driver;
	struct ieee1394_device_id *device_id;

	match = NULL;
	list_for_each(l, &driver_list) {
		driver = list_entry(l, struct hpsb_protocol_driver, list);
		device_id = nodemgr_match_driver(driver, ud);

		if (device_id != NULL) {
			match = driver;
			break;
		}
	}

	return match;
}

static void nodemgr_bind_drivers (struct node_entry *ne)
{
	struct list_head *lh;
	struct hpsb_protocol_driver *driver;
	struct unit_directory *ud;

	list_for_each(lh, &ne->unit_directories) {
		ud = list_entry(lh, struct unit_directory, node_list);
		driver = nodemgr_find_driver(ud);
		if (driver && (!driver->probe || driver->probe(ud) == 0))
			nodemgr_claim_unit_directory(ud, driver);
		nodemgr_call_policy("add", ud);
	}
}


int hpsb_register_protocol(struct hpsb_protocol_driver *driver)
{
	struct unit_directory *ud;
	struct list_head *lh, *next;

	if (down_interruptible(&nodemgr_serialize))
		return -EINTR;

	list_add_tail(&driver->list, &driver_list);

	INIT_LIST_HEAD(&driver->unit_directories);

	list_for_each_safe (lh, next, &unit_directory_list) {
		ud = list_entry(lh, struct unit_directory, driver_list);

		if (nodemgr_match_driver(driver, ud) &&
		    (!driver->probe || driver->probe(ud) == 0))
			nodemgr_claim_unit_directory(ud, driver);
	}

	up(&nodemgr_serialize);

	/*
	 * Right now registration always succeeds, but maybe we should
	 * detect clashes in protocols handled by other drivers.
     * DRD> No because multiple drivers are needed to handle certain devices.
     * For example, a DV camera is an IEC 61883 device (dv1394) and AV/C (raw1394).
     * This will become less an issue with libiec61883 using raw1394.
     *
     * BenC: But can we handle this with an ALLOW_SHARED flag for a
     * protocol? When we get an SBP-3 driver, it will be nice if they were
     * mutually exclusive, since SBP-3 can handle SBP-2 protocol.
     *
     * Not to mention that we currently do not seem to support multiple
     * drivers claiming the same unitdirectory. If we implement both of
     * those, then we'll need to keep probing when a driver claims a
     * unitdirectory, but is sharable.
	 */

	return 0;
}

void hpsb_unregister_protocol(struct hpsb_protocol_driver *driver)
{
	struct list_head *lh, *next;
	struct unit_directory *ud;

	down(&nodemgr_serialize);

	list_del(&driver->list);

	list_for_each_safe (lh, next, &driver->unit_directories) {
		ud = list_entry(lh, struct unit_directory, driver_list);

		if (ud->driver && ud->driver->disconnect)
			ud->driver->disconnect(ud);

		nodemgr_release_unit_directory(ud);
	}

	up(&nodemgr_serialize);
}

static void nodemgr_process_config_rom(struct node_entry *ne, 
				       quadlet_t busoptions)
{
	ne->busopt.irmc		= (busoptions >> 31) & 1;
	ne->busopt.cmc		= (busoptions >> 30) & 1;
	ne->busopt.isc		= (busoptions >> 29) & 1;
	ne->busopt.bmc		= (busoptions >> 28) & 1;
	ne->busopt.pmc		= (busoptions >> 27) & 1;
	ne->busopt.cyc_clk_acc	= (busoptions >> 16) & 0xff;
	ne->busopt.max_rec	= 1 << (((busoptions >> 12) & 0xf) + 1);
	ne->busopt.generation	= (busoptions >> 4) & 0xf;
	ne->busopt.lnkspd	= busoptions & 0x7;

	HPSB_VERBOSE("NodeMgr: raw=0x%08x irmc=%d cmc=%d isc=%d bmc=%d pmc=%d "
		     "cyc_clk_acc=%d max_rec=%d gen=%d lspd=%d",
		     busoptions, ne->busopt.irmc, ne->busopt.cmc,
		     ne->busopt.isc, ne->busopt.bmc, ne->busopt.pmc,
		     ne->busopt.cyc_clk_acc, ne->busopt.max_rec,
		     ne->busopt.generation, ne->busopt.lnkspd);

	/*
	 * When the config rom changes we disconnect all drivers and
	 * free the cached unit directories and reread the whole
	 * thing.  If this was a new device, the call to
	 * nodemgr_disconnect_drivers is a no-op and all is well.
	 */
	nodemgr_free_unit_directories(ne);
	nodemgr_process_root_directory(ne);
	nodemgr_bind_drivers(ne);
}

/*
 * This function updates nodes that were present on the bus before the
 * reset and still are after the reset.  The nodeid and the config rom
 * may have changed, and the drivers managing this device must be
 * informed that this device just went through a bus reset, to allow
 * the to take whatever actions required.
 */
static void nodemgr_update_node(struct node_entry *ne, quadlet_t busoptions,
				struct host_info *hi, nodeid_t nodeid,
				unsigned int generation)
{
	struct list_head *lh;
	struct unit_directory *ud;

	if (ne->nodeid != nodeid) {
		HPSB_DEBUG("Node changed: " NODE_BUS_FMT " -> " NODE_BUS_FMT,
			   NODE_BUS_ARGS(ne->host, ne->nodeid),
			   NODE_BUS_ARGS(ne->host, nodeid));
		ne->nodeid = nodeid;
	}

	ne->generation = generation;

	if (ne->busopt.generation != ((busoptions >> 4) & 0xf))
		nodemgr_process_config_rom (ne, busoptions);

	list_for_each (lh, &ne->unit_directories) {
		ud = list_entry (lh, struct unit_directory, node_list);
		if (ud->driver && ud->driver->update != NULL)
			ud->driver->update(ud);
	}
}

static int read_businfo_block(struct hpsb_host *host, nodeid_t nodeid, unsigned int generation,
			      quadlet_t *buffer, int buffer_length)
{
	octlet_t addr = CSR_REGISTER_BASE + CSR_CONFIG_ROM;
	unsigned header_size;
	int i;

	/* IEEE P1212 says that devices should support 64byte block
	 * reads, aligned on 64byte boundaries. That doesn't seem to
	 * work though, and we are forced to doing quadlet sized
	 * reads.  */

	HPSB_VERBOSE("Initiating ConfigROM request for node " NODE_BUS_FMT,
		     NODE_BUS_ARGS(host, nodeid));

	/* 
	 * Must retry a few times if config rom read returns zero (how long?). Will
	 * not normally occur, but we should do the right thing. For example, with
	 * some sbp2 devices, the bridge chipset cannot return valid config rom reads
	 * immediately after power-on, since they need to detect the type of 
	 * device attached (disk or CD-ROM).
	 */
	for (i = 0; i < 4; i++) {
		if (nodemgr_read_quadlet(host, nodeid, generation,
					 addr, &buffer[0]) < 0) {
			HPSB_ERR("ConfigROM quadlet transaction error for node "
				 NODE_BUS_FMT, NODE_BUS_ARGS(host, nodeid));
			return -1;
		}
		if (buffer[0])
			break;

		set_current_state(TASK_INTERRUPTIBLE);
		if (schedule_timeout (HZ/4))
			return -1;
	}

	header_size = buffer[0] >> 24;
	addr += 4;

	if (header_size == 1) {
		HPSB_INFO("Node " NODE_BUS_FMT " has a minimal ROM.  "
			  "Vendor is %08x",
			  NODE_BUS_ARGS(host, nodeid), buffer[0] & 0x00ffffff);
		return -1;
	}

	if (header_size < 4) {
		HPSB_INFO("Node " NODE_BUS_FMT " has non-standard ROM "
			  "format (%d quads), cannot parse",
			  NODE_BUS_ARGS(host, nodeid), header_size);
		return -1;
	}

	for (i = 1; i < buffer_length; i++, addr += 4) {
		if (nodemgr_read_quadlet(host, nodeid, generation,
					 addr, &buffer[i]) < 0) {
			HPSB_ERR("ConfigROM quadlet transaction "
				 "error for node " NODE_BUS_FMT,
				 NODE_BUS_ARGS(host, nodeid));
			return -1;
		}
	}

	return 0;
}		

static void nodemgr_remove_node(struct node_entry *ne)
{
	HPSB_DEBUG("Node removed: ID:BUS[" NODE_BUS_FMT "]  GUID[%016Lx]",
		   NODE_BUS_ARGS(ne->host, ne->nodeid), (unsigned long long)ne->guid);

	nodemgr_free_unit_directories(ne);
	list_del(&ne->list);
	kfree(ne);

	return;
}

/* This is where we probe the nodes for their information and provided
 * features.  */
static void nodemgr_node_probe_one(struct host_info *hi,
				   nodeid_t nodeid, int generation)
{
	struct hpsb_host *host = hi->host;
	struct node_entry *ne;
	quadlet_t buffer[5];
	octlet_t guid;

	/* We need to detect when the ConfigROM's generation has changed,
	 * so we only update the node's info when it needs to be.  */

	if (read_businfo_block (host, nodeid, generation,
				buffer, sizeof(buffer) >> 2))
		return;

	if (buffer[1] != IEEE1394_BUSID_MAGIC) {
		/* This isn't a 1394 device, but we let it slide. There
		 * was a report of a device with broken firmware which
		 * reported '2394' instead of '1394', which is obviously a
		 * mistake. One would hope that a non-1394 device never
		 * gets connected to Firewire bus. If someone does, we
		 * shouldn't be held responsible, so we'll allow it with a
		 * warning.  */
		HPSB_WARN("Node " NODE_BUS_FMT " has invalid busID magic [0x%08x]",
			 NODE_BUS_ARGS(host, nodeid), buffer[1]);
	}

	guid = ((u64)buffer[3] << 32) | buffer[4];
	ne = find_entry_by_guid(guid);

	if (!ne)
		nodemgr_create_node(guid, buffer[2], hi, nodeid, generation);
	else
		nodemgr_update_node(ne, buffer[2], hi, nodeid, generation);

	return;
}

static void nodemgr_node_probe_cleanup(struct hpsb_host *host, unsigned int generation)
{
	struct list_head *lh, *next;
	struct node_entry *ne;

	/* Now check to see if we have any nodes that aren't referenced
	 * any longer.  */
	list_for_each_safe(lh, next, &node_list) {
		ne = list_entry(lh, struct node_entry, list);

		/* Only checking this host */
		if (ne->host != host)
			continue;

		/* If the generation didn't get updated, then either the
		 * node was removed, or it failed the above probe. Either
		 * way, we remove references to it, since they are
		 * invalid.  */
		if (ne->generation != generation)
			nodemgr_remove_node(ne);
	}

	return;
}

static void nodemgr_node_probe(struct host_info *hi, int generation)
{
	int count;
	struct hpsb_host *host = hi->host;
	struct selfid *sid = (struct selfid *)host->topology_map;
	nodeid_t nodeid = LOCAL_BUS;

	/* Scan each node on the bus */
	for (count = host->selfid_count; count; count--, sid++) {
		if (sid->extended)
			continue;

		if (!sid->link_active) {
			nodeid++;
			continue;
		}
		nodemgr_node_probe_one(hi, nodeid++, generation);
	}

	/* If we had a bus reset while we were scanning the bus, it is
	 * possible that we did not probe all nodes.  In that case, we
	 * skip the clean up for now, since we could remove nodes that
	 * were still on the bus.  The bus reset increased hi->reset_sem,
	 * so there's a bus scan pending which will do the clean up
	 * eventually. */
	if (generation == get_hpsb_generation(host))
		nodemgr_node_probe_cleanup(host, generation);

	return;
}

/* Because we are a 1394a-2000 compliant IRM, we need to inform all the other
 * nodes of the broadcast channel.  (Really we're only setting the validity
 * bit). Other IRM responsibilities go in here as well. */
static void nodemgr_do_irm_duties(struct hpsb_host *host)
{
	quadlet_t bc;
        
	if (!host->is_irm)
		return;

	host->csr.broadcast_channel |= 0x40000000;  /* set validity bit */

	bc = cpu_to_be32(host->csr.broadcast_channel);

	hpsb_write(host, LOCAL_BUS | ALL_NODES, get_hpsb_generation(host),
		   (CSR_REGISTER_BASE | CSR_BROADCAST_CHANNEL),
		   &bc, sizeof(quadlet_t));

	/* If there is no bus manager then we should set the root node's
	 * force_root bit to promote bus stability per the 1394
	 * spec. (8.4.2.6) */
	if (host->busmgr_id == 0xffff && host->node_count > 1)
	{
		u16 root_node = host->node_count - 1;
		struct node_entry *ne = find_entry_by_nodeid(host, root_node | LOCAL_BUS);

		if (ne && ne->busopt.cmc)
			hpsb_send_phy_config(host, root_node, -1);
		else {
			HPSB_DEBUG("The root node is not cycle master capable; "
				   "selecting a new root node and resetting...");
			hpsb_send_phy_config(host, NODEID_TO_NODE(host->node_id), -1);
			hpsb_reset_bus(host, LONG_RESET_FORCE_ROOT);
		}
	}
}

/* We need to ensure that if we are not the IRM, that the IRM node is capable of
 * everything we can do, otherwise issue a bus reset and try to become the IRM
 * ourselves. */
static int nodemgr_check_irm_capability(struct hpsb_host *host, int cycles)
{
	quadlet_t bc;
	int status;

	if (host->is_irm)
		return 1;

	status = hpsb_read(host, LOCAL_BUS | (host->irm_id),
			   get_hpsb_generation(host),
			   (CSR_REGISTER_BASE | CSR_BROADCAST_CHANNEL),
			   &bc, sizeof(quadlet_t));

	if (status < 0 || !(be32_to_cpu(bc) & 0x80000000)) {
		/* The current irm node does not have a valid BROADCAST_CHANNEL
		 * register and we do, so reset the bus with force_root set */
		HPSB_DEBUG("Current remote IRM is not 1394a-2000 compliant, resetting...");

		if (cycles >= 5) {
			/* Oh screw it! Just leave the bus as it is */
			HPSB_DEBUG("Stopping reset loop for IRM sanity");
			return 1;
		}

		hpsb_send_phy_config(host, NODEID_TO_NODE(host->node_id), -1);
		hpsb_reset_bus(host, LONG_RESET_FORCE_ROOT);

		return 0;
	}

	return 1;
}

static int nodemgr_host_thread(void *__hi)
{
	struct host_info *hi = (struct host_info *)__hi;
	struct hpsb_host *host = hi->host;
	int reset_cycles = 0;

	/* No userlevel access needed */
	daemonize();

	strcpy(current->comm, hi->daemon_name);
	
	/* Sit and wait for a signal to probe the nodes on the bus. This
	 * happens when we get a bus reset. */
	while (!down_interruptible(&hi->reset_sem) &&
	       !down_interruptible(&nodemgr_serialize)) {
		unsigned int generation = 0;
		int i;

		/* Pause for 1/4 second in 1/16 second intervals,
		 * to make sure things settle down. */
		for (i = 0; i < 4 ; i++) {
			set_current_state(TASK_INTERRUPTIBLE);
			if (schedule_timeout(HZ/16)) {
				up(&nodemgr_serialize);
				goto caught_signal;
			}

			/* Now get the generation in which the node ID's we collect
			 * are valid.  During the bus scan we will use this generation
			 * for the read transactions, so that if another reset occurs
			 * during the scan the transactions will fail instead of
			 * returning bogus data. */
			generation = get_hpsb_generation(host);

			/* If we get a reset before we are done waiting, then
			 * start the the waiting over again */
			while (!down_trylock(&hi->reset_sem))
				i = 0;
		}

		if (!nodemgr_check_irm_capability(host, reset_cycles++)) {
			/* Do nothing, we are resetting */
			up(&nodemgr_serialize);
			continue;
		}

		reset_cycles = 0;

		nodemgr_node_probe(hi, generation);
		nodemgr_do_irm_duties(host);

		up(&nodemgr_serialize);
	}

caught_signal:
	HPSB_VERBOSE("NodeMgr: Exiting thread");

	complete_and_exit(&hi->exited, 0);
}

struct node_entry *hpsb_guid_get_entry(u64 guid)
{
        struct node_entry *ne;

	down(&nodemgr_serialize);
        ne = find_entry_by_guid(guid);
	up(&nodemgr_serialize);

        return ne;
}

struct node_entry *hpsb_nodeid_get_entry(struct hpsb_host *host, nodeid_t nodeid)
{
	struct node_entry *ne;

	down(&nodemgr_serialize);
	ne = find_entry_by_nodeid(host, nodeid);
	up(&nodemgr_serialize);

	return ne;
}

/* The following four convenience functions use a struct node_entry
 * for addressing a node on the bus.  They are intended for use by any
 * process context, not just the nodemgr thread, so we need to be a
 * little careful when reading out the node ID and generation.  The
 * thing that can go wrong is that we get the node ID, then a bus
 * reset occurs, and then we read the generation.  The node ID is
 * possibly invalid, but the generation is current, and we end up
 * sending a packet to a the wrong node.
 *
 * The solution is to make sure we read the generation first, so that
 * if a reset occurs in the process, we end up with a stale generation
 * and the transactions will fail instead of silently using wrong node
 * ID's.
 */

void hpsb_node_fill_packet(struct node_entry *ne, struct hpsb_packet *pkt)
{
        pkt->host = ne->host;
        pkt->generation = ne->generation;
	barrier();
        pkt->node_id = ne->nodeid;
}

int hpsb_node_read(struct node_entry *ne, u64 addr,
		   quadlet_t *buffer, size_t length)
{
	unsigned int generation = ne->generation;

	barrier();
	return hpsb_read(ne->host, ne->nodeid, generation,
			 addr, buffer, length);
}

int hpsb_node_write(struct node_entry *ne, u64 addr, 
		    quadlet_t *buffer, size_t length)
{
	unsigned int generation = ne->generation;

	barrier();
	return hpsb_write(ne->host, ne->nodeid, generation,
			  addr, buffer, length);
}

int hpsb_node_lock(struct node_entry *ne, u64 addr, 
		   int extcode, quadlet_t *data, quadlet_t arg)
{
	unsigned int generation = ne->generation;

	barrier();
	return hpsb_lock(ne->host, ne->nodeid, generation,
			 addr, extcode, data, arg);
}

static void nodemgr_add_host(struct hpsb_host *host)
{
	struct host_info *hi;

	hi = hpsb_create_hostinfo(&nodemgr_highlevel, host, sizeof(*hi));

	if (!hi) {
		HPSB_ERR ("NodeMgr: out of memory in add host");
		return;
	}

	hi->host = host;
	init_completion(&hi->exited);
        sema_init(&hi->reset_sem, 0);

	sprintf(hi->daemon_name, "knodemgrd_%d", host->id);

	hi->pid = kernel_thread(nodemgr_host_thread, hi,
				CLONE_FS | CLONE_FILES | CLONE_SIGHAND);

	if (hi->pid < 0) {
		HPSB_ERR ("NodeMgr: failed to start %s thread for %s",
			  hi->daemon_name, host->driver->name);
		hpsb_destroy_hostinfo(&nodemgr_highlevel, host);
		return;
	}

	return;
}

static void nodemgr_host_reset(struct hpsb_host *host)
{
	struct host_info *hi = hpsb_get_hostinfo(&nodemgr_highlevel, host);

	if (hi != NULL) {
		HPSB_VERBOSE("NodeMgr: Processing host reset for %s", hi->daemon_name);
		up(&hi->reset_sem);
	} else
		HPSB_ERR ("NodeMgr: could not process reset of unused host");

	return;
}

static void nodemgr_remove_host(struct hpsb_host *host)
{
	struct list_head *lh, *next;
	struct node_entry *ne;
	struct host_info *hi = hpsb_get_hostinfo(&nodemgr_highlevel, host);

	if (hi) {
		if (hi->pid >= 0) {
			kill_proc(hi->pid, SIGTERM, 1);
			wait_for_completion(&hi->exited);
		}
	} else
		HPSB_ERR("NodeMgr: host %s does not exist, cannot remove",
			 host->driver->name);

	down(&nodemgr_serialize);

	/* Even if we fail the host_info part, remove all the node
	 * entries.  */
	list_for_each_safe(lh, next, &node_list) {
		ne = list_entry(lh, struct node_entry, list);

		if (ne->host == host)
			nodemgr_remove_node(ne);
	}

	up(&nodemgr_serialize);

	return;
}

static struct hpsb_highlevel nodemgr_highlevel = {
	.name =		"Node manager",
	.add_host =	nodemgr_add_host,
	.host_reset =	nodemgr_host_reset,
	.remove_host =	nodemgr_remove_host,
};

#define PROC_ENTRY "devices"

void init_ieee1394_nodemgr(void)
{
#ifdef CONFIG_PROC_FS
	if (!create_proc_read_entry(PROC_ENTRY, 0444, ieee1394_procfs_entry, raw1394_read_proc, NULL))
		HPSB_ERR("Can't create devices procfs entry");
#endif
	hpsb_register_highlevel(&nodemgr_highlevel);
}

void cleanup_ieee1394_nodemgr(void)
{
        hpsb_unregister_highlevel(&nodemgr_highlevel);
#ifdef CONFIG_PROC_FS
	remove_proc_entry(PROC_ENTRY, ieee1394_procfs_entry);
#endif
}

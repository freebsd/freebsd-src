/*
 *  linux/arch/i386/kernel/mca.c
 *  Written by Martin Kolinek, February 1996
 *
 * Changes:
 *
 *	Chris Beauregard July 28th, 1996
 *	- Fixed up integrated SCSI detection
 *
 *	Chris Beauregard August 3rd, 1996
 *	- Made mca_info local
 *	- Made integrated registers accessible through standard function calls
 *	- Added name field
 *	- More sanity checking
 *
 *	Chris Beauregard August 9th, 1996
 *	- Rewrote /proc/mca
 *
 *	Chris Beauregard January 7th, 1997
 *	- Added basic NMI-processing
 *	- Added more information to mca_info structure
 *
 *	David Weinehall October 12th, 1998
 *	- Made a lot of cleaning up in the source
 *	- Added use of save_flags / restore_flags
 *	- Added the 'driver_loaded' flag in MCA_adapter
 *	- Added an alternative implemention of ZP Gu's mca_find_unused_adapter
 *
 *	David Weinehall March 24th, 1999
 *	- Fixed the output of 'Driver Installed' in /proc/mca/pos
 *	- Made the Integrated Video & SCSI show up even if they have id 0000
 *
 *	Alexander Viro November 9th, 1999
 *	- Switched to regular procfs methods
 *
 *	Alfred Arnold & David Weinehall August 23rd, 2000
 *	- Added support for Planar POS-registers
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mca.h>
#include <asm/system.h>
#include <asm/io.h>
#include <linux/proc_fs.h>
#include <linux/mman.h>
#include <linux/config.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/ioport.h>
#include <asm/uaccess.h>
#include <linux/init.h>

/* This structure holds MCA information. Each (plug-in) adapter has
 * eight POS registers. Then the machine may have integrated video and
 * SCSI subsystems, which also have eight POS registers.
 * Finally, the motherboard (planar) has got POS-registers.
 * Other miscellaneous information follows.
 */

typedef enum {
	MCA_ADAPTER_NORMAL = 0,
	MCA_ADAPTER_NONE = 1,
	MCA_ADAPTER_DISABLED = 2,
	MCA_ADAPTER_ERROR = 3
} MCA_AdapterStatus;

struct MCA_adapter {
	MCA_AdapterStatus status;	/* is there a valid adapter? */
	int id;				/* adapter id value */
	unsigned char pos[8];		/* POS registers */
	int driver_loaded;		/* is there a driver installed? */
					/* 0 - No, 1 - Yes */
	char name[48];			/* adapter-name provided by driver */
	char procname[8];		/* name of /proc/mca file */
	MCA_ProcFn procfn;		/* /proc info callback */
	void* dev;			/* device/context info for callback */
};

struct MCA_info {
	/* one for each of the 8 possible slots, plus one for integrated SCSI
	 * and one for integrated video.
	 */

	struct MCA_adapter slot[MCA_NUMADAPTERS];

	/* two potential addresses for integrated SCSI adapter - this will
	 * track which one we think it is.
	 */

	unsigned char which_scsi;
};

/* The mca_info structure pointer. If MCA bus is present, the function
 * mca_probe() is invoked. The function puts motherboard, then all
 * adapters into setup mode, allocates and fills an MCA_info structure,
 * and points this pointer to the structure. Otherwise the pointer
 * is set to zero.
 */

static struct MCA_info* mca_info = NULL;

/* MCA registers */

#define MCA_MOTHERBOARD_SETUP_REG	0x94
#define MCA_ADAPTER_SETUP_REG		0x96
#define MCA_POS_REG(n)			(0x100+(n))

#define MCA_ENABLED	0x01	/* POS 2, set if adapter enabled */

/*--------------------------------------------------------------------*/

#ifdef CONFIG_PROC_FS
static void mca_do_proc_init(void);
#endif

/*--------------------------------------------------------------------*/

/* Build the status info for the adapter */

static void mca_configure_adapter_status(int slot) {
	mca_info->slot[slot].status = MCA_ADAPTER_NONE;

	mca_info->slot[slot].id = mca_info->slot[slot].pos[0]
		+ (mca_info->slot[slot].pos[1] << 8);

	if(!mca_info->slot[slot].id && slot < MCA_MAX_SLOT_NR) {

		/* id = 0x0000 usually indicates hardware failure,
		 * however, ZP Gu (zpg@castle.net> reports that his 9556
		 * has 0x0000 as id and everything still works. There
		 * also seem to be an adapter with id = 0x0000; the
		 * NCR Parallel Bus Memory Card. Until this is confirmed,
		 * however, this code will stay.
		 */

		mca_info->slot[slot].status = MCA_ADAPTER_ERROR;

		return;
	} else if(mca_info->slot[slot].id != 0xffff) {

		/* 0xffff usually indicates that there's no adapter,
		 * however, some integrated adapters may have 0xffff as
		 * their id and still be valid. Examples are on-board
		 * VGA of the 55sx, the integrated SCSI of the 56 & 57,
		 * and possibly also the 95 ULTIMEDIA.
		 */

		mca_info->slot[slot].status = MCA_ADAPTER_NORMAL;
	}

	if((mca_info->slot[slot].id == 0xffff ||
	   mca_info->slot[slot].id == 0x0000) && slot >= MCA_MAX_SLOT_NR) {
		int j;

		for(j = 2; j < 8; j++) {
			if(mca_info->slot[slot].pos[j] != 0xff) {
				mca_info->slot[slot].status = MCA_ADAPTER_NORMAL;
				break;
			}
		}
	}

	if(!(mca_info->slot[slot].pos[2] & MCA_ENABLED)) {

		/* enabled bit is in POS 2 */

		mca_info->slot[slot].status = MCA_ADAPTER_DISABLED;
	}
} /* mca_configure_adapter_status */

/*--------------------------------------------------------------------*/

struct resource mca_standard_resources[] = {
	{ "system control port B (MCA)", 0x60, 0x60 },
	{ "arbitration (MCA)", 0x90, 0x90 },
	{ "card Select Feedback (MCA)", 0x91, 0x91 },
	{ "system Control port A (MCA)", 0x92, 0x92 },
	{ "system board setup (MCA)", 0x94, 0x94 },
	{ "POS (MCA)", 0x96, 0x97 },
	{ "POS (MCA)", 0x100, 0x107 }
};

#define MCA_STANDARD_RESOURCES	(sizeof(mca_standard_resources)/sizeof(struct resource))

void __init mca_init(void)
{
	unsigned int i, j;
	unsigned long flags;

	/* WARNING: Be careful when making changes here. Putting an adapter
	 * and the motherboard simultaneously into setup mode may result in
	 * damage to chips (according to The Indispensible PC Hardware Book
	 * by Hans-Peter Messmer). Also, we disable system interrupts (so
	 * that we are not disturbed in the middle of this).
	 */

	/* Make sure the MCA bus is present */

	if(!MCA_bus)
		return;
	printk("Micro Channel bus detected.\n");

	/* Allocate MCA_info structure (at address divisible by 8) */

	mca_info = (struct MCA_info *)kmalloc(sizeof(struct MCA_info), GFP_KERNEL);

	if(mca_info == NULL) {
		printk("Failed to allocate memory for mca_info!");
		return;
	}
	memset(mca_info, 0, sizeof(struct MCA_info));

	save_flags(flags);
	cli();

	/* Make sure adapter setup is off */

	outb_p(0, MCA_ADAPTER_SETUP_REG);

	/* Read motherboard POS registers */

	outb_p(0x7f, MCA_MOTHERBOARD_SETUP_REG);
	mca_info->slot[MCA_MOTHERBOARD].name[0] = 0;
	for(j=0; j<8; j++) {
		mca_info->slot[MCA_MOTHERBOARD].pos[j] = inb_p(MCA_POS_REG(j));
	}
	mca_configure_adapter_status(MCA_MOTHERBOARD);

	/* Put motherboard into video setup mode, read integrated video
	 * POS registers, and turn motherboard setup off.
	 */

	outb_p(0xdf, MCA_MOTHERBOARD_SETUP_REG);
	mca_info->slot[MCA_INTEGVIDEO].name[0] = 0;
	for(j=0; j<8; j++) {
		mca_info->slot[MCA_INTEGVIDEO].pos[j] = inb_p(MCA_POS_REG(j));
	}
	mca_configure_adapter_status(MCA_INTEGVIDEO);

	/* Put motherboard into scsi setup mode, read integrated scsi
	 * POS registers, and turn motherboard setup off.
	 *
	 * It seems there are two possible SCSI registers. Martin says that
	 * for the 56,57, 0xf7 is the one, but fails on the 76.
	 * Alfredo (apena@vnet.ibm.com) says
	 * 0xfd works on his machine. We'll try both of them. I figure it's
	 * a good bet that only one could be valid at a time. This could
	 * screw up though if one is used for something else on the other
	 * machine.
	 */

	outb_p(0xf7, MCA_MOTHERBOARD_SETUP_REG);
	mca_info->slot[MCA_INTEGSCSI].name[0] = 0;
	for(j=0; j<8; j++) {
		if((mca_info->slot[MCA_INTEGSCSI].pos[j] = inb_p(MCA_POS_REG(j))) != 0xff)
		{
			/* 0xff all across means no device. 0x00 means
			 * something's broken, but a device is probably there.
			 * However, if you get 0x00 from a motherboard
			 * register it won't matter what we find.  For the
			 * record, on the 57SLC, the integrated SCSI
			 * adapter has 0xffff for the adapter ID, but
			 * nonzero for other registers.
			 */

			mca_info->which_scsi = 0xf7;
		}
	}
	if(!mca_info->which_scsi) {

		/* Didn't find it at 0xf7, try somewhere else... */
		mca_info->which_scsi = 0xfd;

		outb_p(0xfd, MCA_MOTHERBOARD_SETUP_REG);
		for(j=0; j<8; j++)
			mca_info->slot[MCA_INTEGSCSI].pos[j] = inb_p(MCA_POS_REG(j));
	}
	mca_configure_adapter_status(MCA_INTEGSCSI);

	/* Turn off motherboard setup */

	outb_p(0xff, MCA_MOTHERBOARD_SETUP_REG);

	/* Now loop over MCA slots: put each adapter into setup mode, and
	 * read its POS registers. Then put adapter setup off.
	 */

	for(i=0; i<MCA_MAX_SLOT_NR; i++) {
		outb_p(0x8|(i&0xf), MCA_ADAPTER_SETUP_REG);
		for(j=0; j<8; j++) {
			mca_info->slot[i].pos[j]=inb_p(MCA_POS_REG(j));
		}
		mca_info->slot[i].name[0] = 0;
		mca_info->slot[i].driver_loaded = 0;
		mca_configure_adapter_status(i);
	}
	outb_p(0, MCA_ADAPTER_SETUP_REG);

	/* Enable interrupts and return memory start */

	restore_flags(flags);

	for (i = 0; i < MCA_STANDARD_RESOURCES; i++)
		request_resource(&ioport_resource, mca_standard_resources + i);

#ifdef CONFIG_PROC_FS
	mca_do_proc_init();
#endif
}

/*--------------------------------------------------------------------*/

static void mca_handle_nmi_slot(int slot, int check_flag)
{
	if(slot < MCA_MAX_SLOT_NR) {
		printk("NMI: caused by MCA adapter in slot %d (%s)\n", slot+1,
			mca_info->slot[slot].name);
	} else if(slot == MCA_INTEGSCSI) {
		printk("NMI: caused by MCA integrated SCSI adapter (%s)\n",
			mca_info->slot[slot].name);
	} else if(slot == MCA_INTEGVIDEO) {
		printk("NMI: caused by MCA integrated video adapter (%s)\n",
			mca_info->slot[slot].name);
	} else if(slot == MCA_MOTHERBOARD) {
		printk("NMI: caused by motherboard (%s)\n",
			mca_info->slot[slot].name);
	}

	/* More info available in POS 6 and 7? */

	if(check_flag) {
		unsigned char pos6, pos7;

		pos6 = mca_read_pos(slot, 6);
		pos7 = mca_read_pos(slot, 7);

		printk("NMI: POS 6 = 0x%x, POS 7 = 0x%x\n", pos6, pos7);
	}

} /* mca_handle_nmi_slot */

/*--------------------------------------------------------------------*/

void mca_handle_nmi(void)
{

	int i;
	unsigned char pos5;

	/* First try - scan the various adapters and see if a specific
	 * adapter was responsible for the error.
	 */

	for(i = 0; i < MCA_NUMADAPTERS; i++) {

	/* Bit 7 of POS 5 is reset when this adapter has a hardware
	 * error. Bit 7 it reset if there's error information
	 * available in POS 6 and 7.
	 */

	pos5 = mca_read_pos(i, 5);

	if(!(pos5 & 0x80)) {
			mca_handle_nmi_slot(i, !(pos5 & 0x40));
			return;
		}
	}

	/* If I recall correctly, there's a whole bunch of other things that
	 * we can do to check for NMI problems, but that's all I know about
	 * at the moment.
	 */

	printk("NMI generated from unknown source!\n");
} /* mca_handle_nmi */

/*--------------------------------------------------------------------*/

/**
 *	mca_find_adapter - scan for adapters
 *	@id:	MCA identification to search for
 *	@start:	starting slot
 *
 *	Search the MCA configuration for adapters matching the 16bit
 *	ID given. The first time it should be called with start as zero
 *	and then further calls made passing the return value of the
 *	previous call until %MCA_NOTFOUND is returned.
 *
 *	Disabled adapters are not reported.
 */

int mca_find_adapter(int id, int start)
{
	if(mca_info == NULL || id == 0xffff) {
		return MCA_NOTFOUND;
	}

	for(; start >= 0 && start < MCA_NUMADAPTERS; start++) {

		/* Not sure about this. There's no point in returning
		 * adapters that aren't enabled, since they can't actually
		 * be used. However, they might be needed for statistical
		 * purposes or something... But if that is the case, the
		 * user is free to write a routine that manually iterates
		 * through the adapters.
		 */

		if(mca_info->slot[start].status == MCA_ADAPTER_DISABLED) {
			continue;
		}

		if(id == mca_info->slot[start].id) {
			return start;
		}
	}

	return MCA_NOTFOUND;
} /* mca_find_adapter() */

EXPORT_SYMBOL(mca_find_adapter);

/*--------------------------------------------------------------------*/

/**
 *	mca_find_unused_adapter - scan for unused adapters
 *	@id:	MCA identification to search for
 *	@start:	starting slot
 *
 *	Search the MCA configuration for adapters matching the 16bit
 *	ID given. The first time it should be called with start as zero
 *	and then further calls made passing the return value of the
 *	previous call until %MCA_NOTFOUND is returned.
 *
 *	Adapters that have been claimed by drivers and those that
 *	are disabled are not reported. This function thus allows a driver
 *	to scan for further cards when some may already be driven.
 */

int mca_find_unused_adapter(int id, int start)
{
	if(mca_info == NULL || id == 0xffff) {
		return MCA_NOTFOUND;
	}

	for(; start >= 0 && start < MCA_NUMADAPTERS; start++) {

		/* not sure about this. There's no point in returning
		 * adapters that aren't enabled, since they can't actually
		 * be used. However, they might be needed for statistical
		 * purposes or something... But if that is the case, the
		 * user is free to write a routine that manually iterates
		 * through the adapters.
		 */

		if(mca_info->slot[start].status == MCA_ADAPTER_DISABLED ||
		   mca_info->slot[start].driver_loaded) {
			continue;
		}

		if(id == mca_info->slot[start].id) {
			return start;
		}
	}

	return MCA_NOTFOUND;
} /* mca_find_unused_adapter() */

EXPORT_SYMBOL(mca_find_unused_adapter);

/*--------------------------------------------------------------------*/

/**
 *	mca_read_stored_pos - read POS register from boot data
 *	@slot: slot number to read from
 *	@reg:  register to read from
 *
 *	Fetch a POS value that was stored at boot time by the kernel
 *	when it scanned the MCA space. The register value is returned.
 *	Missing or invalid registers report 0.
 */

unsigned char mca_read_stored_pos(int slot, int reg)
{
	if(slot < 0 || slot >= MCA_NUMADAPTERS || mca_info == NULL) return 0;
	if(reg < 0 || reg >= 8) return 0;
	return mca_info->slot[slot].pos[reg];
} /* mca_read_stored_pos() */

EXPORT_SYMBOL(mca_read_stored_pos);

/*--------------------------------------------------------------------*/

/**
 *	mca_read_pos - read POS register from card
 *	@slot: slot number to read from
 *	@reg:  register to read from
 *
 *	Fetch a POS value directly from the hardware to obtain the
 *	current value. This is much slower than mca_read_stored_pos and
 *	may not be invoked from interrupt context. It handles the
 *	deep magic required for onboard devices transparently.
 */

unsigned char mca_read_pos(int slot, int reg)
{
	unsigned int byte = 0;
	unsigned long flags;

	if(slot < 0 || slot >= MCA_NUMADAPTERS || mca_info == NULL) return 0;
	if(reg < 0 || reg >= 8) return 0;

	save_flags(flags);
	cli();

	/* Make sure motherboard setup is off */

	outb_p(0xff, MCA_MOTHERBOARD_SETUP_REG);

	/* Read in the appropriate register */

	if(slot == MCA_INTEGSCSI && mca_info->which_scsi) {

		/* Disable adapter setup, enable motherboard setup */

		outb_p(0, MCA_ADAPTER_SETUP_REG);
		outb_p(mca_info->which_scsi, MCA_MOTHERBOARD_SETUP_REG);

		byte = inb_p(MCA_POS_REG(reg));
		outb_p(0xff, MCA_MOTHERBOARD_SETUP_REG);
	} else if(slot == MCA_INTEGVIDEO) {

		/* Disable adapter setup, enable motherboard setup */

		outb_p(0, MCA_ADAPTER_SETUP_REG);
		outb_p(0xdf, MCA_MOTHERBOARD_SETUP_REG);

		byte = inb_p(MCA_POS_REG(reg));
		outb_p(0xff, MCA_MOTHERBOARD_SETUP_REG);
	} else if(slot == MCA_MOTHERBOARD) {

		/* Disable adapter setup, enable motherboard setup */
		outb_p(0, MCA_ADAPTER_SETUP_REG);
		outb_p(0x7f, MCA_MOTHERBOARD_SETUP_REG);

		byte = inb_p(MCA_POS_REG(reg));
		outb_p(0xff, MCA_MOTHERBOARD_SETUP_REG);
	} else if(slot < MCA_MAX_SLOT_NR) {

		/* Make sure motherboard setup is off */

		outb_p(0xff, MCA_MOTHERBOARD_SETUP_REG);

		/* Read the appropriate register */

		outb_p(0x8|(slot&0xf), MCA_ADAPTER_SETUP_REG);
		byte = inb_p(MCA_POS_REG(reg));
		outb_p(0, MCA_ADAPTER_SETUP_REG);
	}

	/* Make sure the stored values are consistent, while we're here */

	mca_info->slot[slot].pos[reg] = byte;

	restore_flags(flags);

	return byte;
} /* mca_read_pos() */

EXPORT_SYMBOL(mca_read_pos);

/*--------------------------------------------------------------------*/

/**
 *	mca_write_pos - read POS register from card
 *	@slot: slot number to read from
 *	@reg:  register to read from
 *	@byte: byte to write to the POS registers
 *
 *	Store a POS value directly from the hardware. You should not
 *	normally need to use this function and should have a very good
 *	knowledge of MCA bus before you do so. Doing this wrongly can
 *	damage the hardware.
 *
 *	This function may not be used from interrupt context.
 *
 *	Note that this a technically a Bad Thing, as IBM tech stuff says
 *	you should only set POS values through their utilities.
 *	However, some devices such as the 3c523 recommend that you write
 *	back some data to make sure the configuration is consistent.
 *	I'd say that IBM is right, but I like my drivers to work.
 *
 *	This function can't do checks to see if multiple devices end up
 *	with the same resources, so you might see magic smoke if someone
 *	screws up.
 */

void mca_write_pos(int slot, int reg, unsigned char byte)
{
	unsigned long flags;

	if(slot < 0 || slot >= MCA_MAX_SLOT_NR)
		return;
	if(reg < 0 || reg >= 8)
		return;
	if(mca_info == NULL)
		return;

	save_flags(flags);
	cli();

	/* Make sure motherboard setup is off */

	outb_p(0xff, MCA_MOTHERBOARD_SETUP_REG);

	/* Read in the appropriate register */

	outb_p(0x8|(slot&0xf), MCA_ADAPTER_SETUP_REG);
	outb_p(byte, MCA_POS_REG(reg));
	outb_p(0, MCA_ADAPTER_SETUP_REG);

	restore_flags(flags);

	/* Update the global register list, while we have the byte */

	mca_info->slot[slot].pos[reg] = byte;
} /* mca_write_pos() */

EXPORT_SYMBOL(mca_write_pos);

/*--------------------------------------------------------------------*/

/**
 *	mca_set_adapter_name - Set the description of the card
 *	@slot: slot to name
 *	@name: text string for the namen
 *
 *	This function sets the name reported via /proc for this
 *	adapter slot. This is for user information only. Setting a
 *	name deletes any previous name.
 */

void mca_set_adapter_name(int slot, char* name)
{
	if(mca_info == NULL) return;

	if(slot >= 0 && slot < MCA_NUMADAPTERS) {
		if(name != NULL) {
			strncpy(mca_info->slot[slot].name, name,
				sizeof(mca_info->slot[slot].name)-1);
			mca_info->slot[slot].name[
				sizeof(mca_info->slot[slot].name)-1] = 0;
		} else {
			mca_info->slot[slot].name[0] = 0;
		}
	}
}

EXPORT_SYMBOL(mca_set_adapter_name);

/**
 *	mca_set_adapter_procfn - Set the /proc callback
 *	@slot: slot to configure
 *	@procfn: callback function to call for /proc
 *	@dev: device information passed to the callback
 *
 *	This sets up an information callback for /proc/mca/slot?.  The
 *	function is called with the buffer, slot, and device pointer (or
 *	some equally informative context information, or nothing, if you
 *	prefer), and is expected to put useful information into the
 *	buffer.  The adapter name, ID, and POS registers get printed
 *	before this is called though, so don't do it again.
 *
 *	This should be called with a %NULL @procfn when a module
 *	unregisters, thus preventing kernel crashes and other such
 *	nastiness.
 */

void mca_set_adapter_procfn(int slot, MCA_ProcFn procfn, void* dev)
{
	if(mca_info == NULL) return;

	if(slot >= 0 && slot < MCA_NUMADAPTERS) {
		mca_info->slot[slot].procfn = procfn;
		mca_info->slot[slot].dev = dev;
	}
}

EXPORT_SYMBOL(mca_set_adapter_procfn);

/**
 *	mca_is_adapter_used - check if claimed by driver
 *	@slot:	slot to check
 *
 *	Returns 1 if the slot has been claimed by a driver
 */

int mca_is_adapter_used(int slot)
{
	return mca_info->slot[slot].driver_loaded;
}

EXPORT_SYMBOL(mca_is_adapter_used);

/**
 *	mca_mark_as_used - claim an MCA device
 *	@slot:	slot to claim
 *	FIXME:  should we make this threadsafe
 *
 *	Claim an MCA slot for a device driver. If the
 *	slot is already taken the function returns 1,
 *	if it is not taken it is claimed and 0 is
 *	returned.
 */

int mca_mark_as_used(int slot)
{
	if(mca_info->slot[slot].driver_loaded) return 1;
	mca_info->slot[slot].driver_loaded = 1;
	return 0;
}

EXPORT_SYMBOL(mca_mark_as_used);

/**
 *	mca_mark_as_unused - release an MCA device
 *	@slot:	slot to claim
 *
 *	Release the slot for other drives to use.
 */

void mca_mark_as_unused(int slot)
{
	mca_info->slot[slot].driver_loaded = 0;
}

EXPORT_SYMBOL(mca_mark_as_unused);

/**
 *	mca_get_adapter_name - get the adapter description
 *	@slot:	slot to query
 *
 *	Return the adapter description if set. If it has not been
 *	set or the slot is out range then return NULL.
 */

char *mca_get_adapter_name(int slot)
{
	if(mca_info == NULL) return 0;

	if(slot >= 0 && slot < MCA_NUMADAPTERS) {
		return mca_info->slot[slot].name;
	}

	return 0;
}

EXPORT_SYMBOL(mca_get_adapter_name);

/**
 *	mca_isadapter - check if the slot holds an adapter
 *	@slot:	slot to query
 *
 *	Returns zero if the slot does not hold an adapter, non zero if
 *	it does.
 */

int mca_isadapter(int slot)
{
	if(mca_info == NULL) return 0;

	if(slot >= 0 && slot < MCA_NUMADAPTERS) {
		return ((mca_info->slot[slot].status == MCA_ADAPTER_NORMAL)
			|| (mca_info->slot[slot].status == MCA_ADAPTER_DISABLED));
	}

	return 0;
}

EXPORT_SYMBOL(mca_isadapter);


/**
 *	mca_isadapter - check if the slot holds an adapter
 *	@slot:	slot to query
 *
 *	Returns a non zero value if the slot holds an enabled adapter
 *	and zero for any other case.
 */

int mca_isenabled(int slot)
{
	if(mca_info == NULL) return 0;

	if(slot >= 0 && slot < MCA_NUMADAPTERS) {
		return (mca_info->slot[slot].status == MCA_ADAPTER_NORMAL);
	}

	return 0;
}

EXPORT_SYMBOL(mca_isenabled);

/*--------------------------------------------------------------------*/

#ifdef CONFIG_PROC_FS

int get_mca_info(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	int i, j, len = 0;

	if(MCA_bus && mca_info != NULL) {
		/* Format POS registers of eight MCA slots */

		for(i=0; i<MCA_MAX_SLOT_NR; i++) {
			len += sprintf(page+len, "Slot %d: ", i+1);
			for(j=0; j<8; j++)
				len += sprintf(page+len, "%02x ", mca_info->slot[i].pos[j]);
			len += sprintf(page+len, " %s\n", mca_info->slot[i].name);
		}

		/* Format POS registers of integrated video subsystem */

		len += sprintf(page+len, "Video : ");
		for(j=0; j<8; j++)
			len += sprintf(page+len, "%02x ", mca_info->slot[MCA_INTEGVIDEO].pos[j]);
		len += sprintf(page+len, " %s\n", mca_info->slot[MCA_INTEGVIDEO].name);

		/* Format POS registers of integrated SCSI subsystem */

		len += sprintf(page+len, "SCSI  : ");
		for(j=0; j<8; j++)
			len += sprintf(page+len, "%02x ", mca_info->slot[MCA_INTEGSCSI].pos[j]);
		len += sprintf(page+len, " %s\n", mca_info->slot[MCA_INTEGSCSI].name);

		/* Format POS registers of motherboard */

		len += sprintf(page+len, "Planar: ");
		for(j=0; j<8; j++)
			len += sprintf(page+len, "%02x ", mca_info->slot[MCA_MOTHERBOARD].pos[j]);
		len += sprintf(page+len, " %s\n", mca_info->slot[MCA_MOTHERBOARD].name);
	} else {
		/* Leave it empty if MCA not detected - this should *never*
		 * happen!
		 */
	}

	if (len <= off+count) *eof = 1;
	*start = page + off;
	len -= off;
	if (len>count) len = count;
	if (len<0) len = 0;
	return len;
}

/*--------------------------------------------------------------------*/

static int mca_default_procfn(char* buf, struct MCA_adapter *p)
{
	int len = 0, i;
	int slot = p - mca_info->slot;

	/* Print out the basic information */

	if(slot < MCA_MAX_SLOT_NR) {
		len += sprintf(buf+len, "Slot: %d\n", slot+1);
	} else if(slot == MCA_INTEGSCSI) {
		len += sprintf(buf+len, "Integrated SCSI Adapter\n");
	} else if(slot == MCA_INTEGVIDEO) {
		len += sprintf(buf+len, "Integrated Video Adapter\n");
	} else if(slot == MCA_MOTHERBOARD) {
		len += sprintf(buf+len, "Motherboard\n");
	}
	if(p->name[0]) {

		/* Drivers might register a name without /proc handler... */

		len += sprintf(buf+len, "Adapter Name: %s\n",
			p->name);
	} else {
		len += sprintf(buf+len, "Adapter Name: Unknown\n");
	}
	len += sprintf(buf+len, "Id: %02x%02x\n",
		p->pos[1], p->pos[0]);
	len += sprintf(buf+len, "Enabled: %s\nPOS: ",
		mca_isenabled(slot) ? "Yes" : "No");
	for(i=0; i<8; i++) {
		len += sprintf(buf+len, "%02x ", p->pos[i]);
	}
	len += sprintf(buf+len, "\nDriver Installed: %s",
		mca_is_adapter_used(slot) ? "Yes" : "No");
	buf[len++] = '\n';
	buf[len] = 0;

	return len;
} /* mca_default_procfn() */

static int get_mca_machine_info(char* page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	int len = 0;

	len += sprintf(page+len, "Model Id: 0x%x\n", machine_id);
	len += sprintf(page+len, "Submodel Id: 0x%x\n", machine_submodel_id);
	len += sprintf(page+len, "BIOS Revision: 0x%x\n", BIOS_revision);

	if (len <= off+count) *eof = 1;
	*start = page + off;
	len -= off;
	if (len>count) len = count;
	if (len<0) len = 0;
	return len;
}

static int mca_read_proc(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	struct MCA_adapter *p = (struct MCA_adapter *)data;
	int len = 0;

	/* Get the standard info */

	len = mca_default_procfn(page, p);

	/* Do any device-specific processing, if there is any */

	if(p->procfn) {
		len += p->procfn(page+len, p-mca_info->slot, p->dev);
	}
	if (len <= off+count) *eof = 1;
	*start = page + off;
	len -= off;
	if (len>count) len = count;
	if (len<0) len = 0;
	return len;
} /* mca_read_proc() */

/*--------------------------------------------------------------------*/

void __init mca_do_proc_init(void)
{
	int i;
	struct proc_dir_entry *proc_mca;
	struct proc_dir_entry* node = NULL;
	struct MCA_adapter *p;

	if(mca_info == NULL) return;	/* Should never happen */

	proc_mca = proc_mkdir("mca", &proc_root);
	create_proc_read_entry("pos",0,proc_mca,get_mca_info,NULL);
	create_proc_read_entry("machine",0,proc_mca,get_mca_machine_info,NULL);

	/* Initialize /proc/mca entries for existing adapters */

	for(i = 0; i < MCA_NUMADAPTERS; i++) {
		p = &mca_info->slot[i];
		p->procfn = 0;

		if(i < MCA_MAX_SLOT_NR) sprintf(p->procname,"slot%d", i+1);
		else if(i == MCA_INTEGVIDEO) sprintf(p->procname,"video");
		else if(i == MCA_INTEGSCSI) sprintf(p->procname,"scsi");
		else if(i == MCA_MOTHERBOARD) sprintf(p->procname,"planar");

		if(!mca_isadapter(i)) continue;

		node = create_proc_read_entry(p->procname, 0, proc_mca,
						mca_read_proc, (void *)p);

		if(node == NULL) {
			printk("Failed to allocate memory for MCA proc-entries!");
			return;
		}
	}

} /* mca_do_proc_init() */

#endif

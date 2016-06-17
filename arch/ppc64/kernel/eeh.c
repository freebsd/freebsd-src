/*
 * eeh.c
 * Copyright (C) 2001 Dave Engebretsen & Todd Inglett IBM Corporation
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

/* Change Activity:
 * 2001/10/27 : engebret : Created.
 * End Change Activity 
 */

#include <linux/init.h>
#include <linux/pci.h>
#include <linux/proc_fs.h>
#include <linux/bootmem.h>
#include <asm/paca.h>
#include <asm/processor.h>
#include <asm/naca.h>
#include <asm/io.h>
#include <asm/machdep.h>
#include "pci.h"

#define BUID_HI(buid) ((buid) >> 32)
#define BUID_LO(buid) ((buid) & 0xffffffff)
#define CONFIG_ADDR(busno, devfn) (((((busno) & 0xff) << 8) | ((devfn) & 0xf8)) << 8)

unsigned long eeh_total_mmio_ffs;
unsigned long eeh_false_positives;
/* RTAS tokens */
static int ibm_set_eeh_option;
static int ibm_set_slot_reset;
static int ibm_read_slot_reset_state;

static int eeh_implemented;
#define EEH_MAX_OPTS 4096
static char *eeh_opts;
static int eeh_opts_last;

pte_t *find_linux_pte(pgd_t *pgdir, unsigned long va);	/* from htab.c */
static int eeh_check_opts_config(struct device_node *dn,
				 int class_code, int vendor_id, int device_id,
				 int default_state);

unsigned long eeh_token_to_phys(unsigned long token)
{
	if (REGION_ID(token) == EEH_REGION_ID) {
		unsigned long vaddr = IO_TOKEN_TO_ADDR(token);
		pte_t *ptep = find_linux_pte(ioremap_mm.pgd, vaddr);
		unsigned long pa = pte_pagenr(*ptep) << PAGE_SHIFT;
		return pa | (vaddr & (PAGE_SIZE-1));
	} else
		return token;
}

/* Check for an eeh failure at the given token address.
 * The given value has been read and it should be 1's (0xff, 0xffff or
 * 0xffffffff).
 *
 * Probe to determine if an error actually occurred.  If not return val.
 * Otherwise panic.
 */
unsigned long eeh_check_failure(void *token, unsigned long val)
{
	unsigned long addr;
	struct pci_dev *dev;
	struct device_node *dn;
	unsigned long ret, rets[2];

	/* IO BAR access could get us here...or if we manually force EEH
	 * operation on even if the hardware won't support it.
	 */
	if (!eeh_implemented || ibm_read_slot_reset_state == RTAS_UNKNOWN_SERVICE)
		return val;

	/* Finding the phys addr + pci device is quite expensive.
	 * However, the RTAS call is MUCH slower.... :(
	 */
	addr = eeh_token_to_phys((unsigned long)token);
	dev = pci_find_dev_by_addr(addr);
	if (!dev) {
		printk("EEH: no pci dev found for addr=0x%lx\n", addr);
		return val;
	}
	dn = pci_device_to_OF_node(dev);
	if (!dn) {
		printk("EEH: no pci dn found for addr=0x%lx\n", addr);
		return val;
	}

	/* Access to IO BARs might get this far and still not want checking. */
	if (!(dn->eeh_mode & EEH_MODE_SUPPORTED) || dn->eeh_mode & EEH_MODE_NOCHECK)
		return val;


	/* Now test for an EEH failure.  This is VERY expensive.
	 * Note that the eeh_config_addr may be a parent device
	 * in the case of a device behind a bridge, or it may be
	 * function zero of a multi-function device.
	 * In any case they must share a common PHB.
	 */
	if (dn->eeh_config_addr) {
		ret = rtas_call(ibm_read_slot_reset_state, 3, 3, rets,
				dn->eeh_config_addr, BUID_HI(dn->phb->buid), BUID_LO(dn->phb->buid));
		if (ret == 0 && rets[1] == 1 && rets[0] >= 2) {
			unsigned char   slot_err_buf[RTAS_ERROR_LOG_MAX];
			unsigned long   slot_err_ret;

			memset(slot_err_buf, 0, RTAS_ERROR_LOG_MAX);
			slot_err_ret = rtas_call(rtas_token("ibm,slot-error-detail"),
						 8, 1, dn->eeh_config_addr,
						 BUID_HI(dn->phb->buid), BUID_LO(dn->phb->buid),
						 NULL, 0, __pa(slot_err_buf), RTAS_ERROR_LOG_MAX,
						 2 /* Permanent Error */);
			if (slot_err_ret == 0)
				log_error(slot_err_buf, ERR_TYPE_RTAS_LOG, 1 /* Fatal */);

			panic("EEH:  MMIO failure (%ld) on device:\n  %s %s\n",
			      rets[0], dev->slot_name, dev->name);
		}
	}
	eeh_false_positives++;
	return val;	/* good case */

}

struct eeh_early_enable_info {
	unsigned int buid_hi;
	unsigned int buid_lo;
	int adapters_enabled;
};


/* Enable/disable eeh for the given device node. */
static void *early_set_eeh(struct device_node *dn, struct eeh_early_enable_info *info, int enable)
{
	long ret;
	char *status = get_property(dn, "status", 0);
	u32 *class_code = (u32 *)get_property(dn, "class-code", 0);
	u32 *vendor_id =(u32 *) get_property(dn, "vendor-id", 0);
	u32 *device_id = (u32 *)get_property(dn, "device-id", 0);
	u32 *regs;

	if (status && strcmp(status, "ok") != 0)
		return NULL;	/* ignore devices with bad status */

	/* Weed out PHBs or other bad nodes. */
	if (!class_code || !vendor_id || !device_id)
		return NULL;

	/* Ignore known PHBs and EADs bridges */
	if (*vendor_id == PCI_VENDOR_ID_IBM &&
	    (*device_id == 0x0102 || *device_id == 0x008b ||
	     *device_id == 0x0188 || *device_id == 0x0302))
		return NULL;

	/* Now decide if we are going to "Disable" EEH checking
	 * for this device.  We still run with the EEH hardware active,
	 * but we won't be checking for ff's.  This means a driver
	 * could return bad data (very bad!), an interrupt handler could
	 * hang waiting on status bits that won't change, etc.
	 * But there are a few cases like display devices that make sense.
	 */

	if (!eeh_check_opts_config(dn, *class_code, *vendor_id, *device_id, enable)) {
		if (enable) {
			printk(KERN_INFO "EEH: %s user requested to run without EEH.\n", dn->full_name);
			enable = 0;
		}
#if 0
	/* Turn off EEH automatically for graphics ... 
	* but we don't want to do this, not really. .... */
	} else 	if ((*class_code >> 16) == PCI_BASE_CLASS_DISPLAY) {
		printk(KERN_INFO "EEH: %s DISPLAY automatically set to run without EEH.\n", dn->full_name);
		enable = 0;
#endif
	}

	if (!enable)
		dn->eeh_mode = EEH_MODE_NOCHECK;

	/* This device may already have an EEH parent. */
	if (dn->parent && (dn->parent->eeh_mode & EEH_MODE_SUPPORTED)) {
		/* Parent supports EEH. */
		dn->eeh_mode |= EEH_MODE_SUPPORTED;

		/* Recurse to parent to set EEH, since we are probably
		 * a non-eeh supporting pci bridge chip on some card. 
		 * But recurse only if our eeh setting is to be different.
		 */
		if ((enable && (EEH_MODE_NOCHECK == dn->eeh_mode)) ||
		    (!enable && (EEH_MODE_NOCHECK != dn->eeh_mode))) 
		{
			early_set_eeh (dn->parent, info, enable);
		}
		dn->eeh_config_addr = dn->parent->eeh_config_addr;
		return NULL;
	}

	/* Ok..see if this device supports EEH. */
	regs = (u32 *)get_property(dn, "reg", 0);
	if (regs) {
		/* First register entry is addr (00BBSS00)  */
		/* Try to enable/disable eeh */
		ret = rtas_call(ibm_set_eeh_option, 4, 1, NULL,
				regs[0], info->buid_hi, info->buid_lo,
				enable ? EEH_ENABLE : EEH_DISABLE);
		if (ret == 0) {
			info->adapters_enabled++;
			dn->eeh_mode |= EEH_MODE_SUPPORTED;
			dn->eeh_config_addr = regs[0];
		} else {
			printk(KERN_INFO "EEH: %s failed to %s ret=%ld\n", dn->full_name, enable ? "enable" : "disable", ret);
		}
	}
	return NULL; 
}

/* Enable eeh for the given device node. */
static void *early_enable_eeh(struct device_node *dn, void *data)
{
	struct eeh_early_enable_info *info = data;
	/* Set enable to 1, i.e. we will do checking */
	return early_set_eeh (dn, info, 1);
}

/*
 * Initialize eeh by trying to enable it for all of the adapters in the system.
 * As a side effect we can determine here if eeh is supported at all.
 * Note that we leave EEH on so failed config cycles won't cause a machine
 * check.  If a user turns off EEH for a particular adapter they are really
 * telling Linux to ignore errors.
 *
 * We should probably distinguish between "ignore errors" and "turn EEH off"
 * but for now disabling EEH for adapters is mostly to work around drivers that
 * directly access mmio space (without using the macros).
 *
 * The eeh-force-off/on option does literally what it says, so if Linux must
 * avoid enabling EEH this must be done.
 */
void eeh_init(void)
{
	struct device_node *phb;
	struct eeh_early_enable_info info;

	extern char cmd_line[];	/* Very early cmd line parse.  Cheap, but works. */
	char *eeh_force_off = strstr(cmd_line, "eeh-force-off");
	char *eeh_force_on = strstr(cmd_line, "eeh-force-on");

	ibm_set_eeh_option = rtas_token("ibm,set-eeh-option");
	ibm_set_slot_reset = rtas_token("ibm,set-slot-reset");
	ibm_read_slot_reset_state = rtas_token("ibm,read-slot-reset-state");

	/* Allow user to force eeh mode on or off -- even if the hardware
	 * doesn't exist.  This allows driver writers to at least test use
	 * of I/O macros even if we can't actually test for EEH failure.
	 */
	if (eeh_force_on > eeh_force_off)
		eeh_implemented = 1;
	else if (ibm_set_eeh_option == RTAS_UNKNOWN_SERVICE)
		return;

	if (eeh_force_off > eeh_force_on) {
		/* User is forcing EEH off.  Be noisy if it is implemented. */
		if (eeh_implemented)
			printk(KERN_WARNING "EEH: WARNING: PCI Enhanced I/O Error Handling is user disabled\n");
		eeh_implemented = 0;
		return;
	}


	/* Enable EEH for all adapters.  Note that eeh requires buid's */
	info.adapters_enabled = 0;
	for (phb = find_devices("pci"); phb; phb = phb->next) {
		int len;
		int *buid_vals = (int *) get_property(phb, "ibm,fw-phb-id", &len);
		if (!buid_vals)
			continue;
		if (len == sizeof(int)) {
			info.buid_lo = buid_vals[0];
			info.buid_hi = 0;
		} else if (len == sizeof(int)*2) {
			info.buid_hi = buid_vals[0];
			info.buid_lo = buid_vals[1];
		} else {
			printk("EEH: odd ibm,fw-phb-id len returned: %d\n", len);
			continue;
		}
		traverse_pci_devices(phb, early_enable_eeh, NULL, &info);
	}
	if (info.adapters_enabled) {
		printk(KERN_INFO "EEH: PCI Enhanced I/O Error Handling Enabled\n");
		eeh_implemented = 1;
	}
}


int eeh_set_option(struct pci_dev *dev, int option)
{
	struct device_node *dn = pci_device_to_OF_node(dev);
	struct pci_controller *phb = PCI_GET_PHB_PTR(dev);

	if (dn == NULL || phb == NULL || phb->buid == 0 || !eeh_implemented)
		return -2;

	return rtas_call(ibm_set_eeh_option, 4, 1, NULL,
			 CONFIG_ADDR(dn->busno, dn->devfn),
			 BUID_HI(phb->buid), BUID_LO(phb->buid), option);
}


/* If EEH is implemented, find the PCI device using given phys addr
 * and check to see if eeh failure checking is disabled.
 * Remap the addr (trivially) to the EEH region if not.
 * For addresses not known to PCI the vaddr is simply returned unchanged.
 */
void *eeh_ioremap(unsigned long addr, void *vaddr)
{
	struct pci_dev *dev;
	struct device_node *dn;

	if (!eeh_implemented)
		return vaddr;
	dev = pci_find_dev_by_addr(addr);
	if (!dev)
		return vaddr;
	dn = pci_device_to_OF_node(dev);
	if (!dn)
		return vaddr;
	if (dn->eeh_mode & EEH_MODE_NOCHECK)
		return vaddr;

	return (void *)IO_ADDR_TO_TOKEN(vaddr);
}

static int eeh_proc_falsepositive_read(char *page, char **start, off_t off,
			 int count, int *eof, void *data)
{
	int len;
	len = sprintf(page, "eeh_false_positives=%ld\n"
		      "eeh_total_mmio_ffs=%ld\n",
		      eeh_false_positives, eeh_total_mmio_ffs);
	return len;
}

/* Implementation of /proc/ppc64/eeh
 * For now it is one file showing false positives.
 */
static int __init eeh_init_proc(void)
{
	struct proc_dir_entry *ent = create_proc_entry("ppc64/eeh", S_IRUGO, 0);
	if (ent) {
		ent->nlink = 1;
		ent->data = NULL;
		ent->read_proc = (void *)eeh_proc_falsepositive_read;
	}
	return 0;
}

/*
 * Test if "dev" should be configured on or off.
 * This processes the options literally from left to right.
 * This lets the user specify stupid combinations of options,
 * but at least the result should be very predictable.
 */
static int eeh_check_opts_config(struct device_node *dn,
				 int class_code, int vendor_id, int device_id,
				 int default_state)
{
	char devname[32], classname[32];
	char *strs[8], *s;
	int nstrs, i;
	int ret = default_state;

	/* Build list of strings to match */
	nstrs = 0;
	s = (char *)get_property(dn, "ibm,loc-code", 0);
	if (s)
		strs[nstrs++] = s;
	sprintf(devname, "dev%04x:%04x", vendor_id, device_id);
	strs[nstrs++] = devname;
	sprintf(classname, "class%04x", class_code);
	strs[nstrs++] = classname;
	strs[nstrs++] = "";	/* yes, this matches the empty string */

	/* Now see if any string matches the eeh_opts list.
	 * The eeh_opts list entries start with + or -.
	 */
	for (s = eeh_opts; s && (s < (eeh_opts + eeh_opts_last)); s += strlen(s)+1) {
		for (i = 0; i < nstrs; i++) {
			if (strcasecmp(strs[i], s+1) == 0) {
				ret = (strs[i][0] == '+') ? 1 : 0;
			}
		}
	}
	return ret;
}

/* Handle kernel eeh-on & eeh-off cmd line options for eeh.
 *
 * We support:
 *	eeh-off=loc1,loc2,loc3...
 *
 * and this option can be repeated so
 *      eeh-off=loc1,loc2 eeh-off=loc3
 * is the same as eeh-off=loc1,loc2,loc3
 *
 * loc is an IBM location code that can be found in a manual or
 * via openfirmware (or the Hardware Management Console).
 *
 * We also support these additional "loc" values:
 *
 *	dev#:#    vendor:device id in hex (e.g. dev1022:2000)
 *	class#    class id in hex (e.g. class0200)
 *
 * If no location code is specified all devices are assumed
 * so eeh-off means eeh by default is off.
 */

/* This is implemented as a null separated list of strings.
 * Each string looks like this:  "+X" or "-X"
 * where X is a loc code, vendor:device, class (as shown above)
 * or empty which is used to indicate all.
 *
 * We interpret this option string list so that it will literally
 * behave left-to-right even if some combinations don't make sense.
 */

static int __init eeh_parm(char *str, int state)
{
	char *s, *cur, *curend;
	if (!eeh_opts) {
		eeh_opts = alloc_bootmem(EEH_MAX_OPTS);
		eeh_opts[eeh_opts_last++] = '+'; /* default */
		eeh_opts[eeh_opts_last++] = '\0';
	}
	if (*str == '\0') {
		eeh_opts[eeh_opts_last++] = state ? '+' : '-';
		eeh_opts[eeh_opts_last++] = '\0';
		return 1;
	}
	if (*str == '=')
		str++;
	for (s = str; s && *s != '\0'; s = curend) {
		cur = s;
		while (*cur == ',')
			cur++;	/* ignore empties.  Don't treat as "all-on" or "all-off" */
		curend = strchr(cur, ',');
		if (!curend)
			curend = cur + strlen(cur);
		if (*cur) {
			int curlen = curend-cur;
			if (eeh_opts_last + curlen > EEH_MAX_OPTS-2) {
				printk(KERN_INFO "EEH: sorry...too many eeh cmd line options\n");
				return 1;
			}
			eeh_opts[eeh_opts_last++] = state ? '+' : '-';
			strncpy(eeh_opts+eeh_opts_last, cur, curlen);
			eeh_opts_last += curlen;
			eeh_opts[eeh_opts_last++] = '\0';
		}
	}
	return 1;
}

static int __init eehoff_parm(char *str)
{
	return eeh_parm(str, 0);
}

static int __init eehon_parm(char *str)
{
	return eeh_parm(str, 1);
}

__initcall(eeh_init_proc);
__setup("eeh-off", eehoff_parm);
__setup("eeh-on", eehon_parm);

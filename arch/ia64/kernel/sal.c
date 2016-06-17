/*
 * System Abstraction Layer (SAL) interface routines.
 *
 * Copyright (C) 1998, 1999, 2001, 2003 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 * Copyright (C) 1999 VA Linux Systems
 * Copyright (C) 1999 Walt Drummond <drummond@valinux.com>
 */
#include <linux/config.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/string.h>

#include <asm/page.h>
#include <asm/sal.h>
#include <asm/pal.h>

spinlock_t sal_lock __cacheline_aligned = SPIN_LOCK_UNLOCKED;
unsigned long sal_platform_features;

static struct {
	void *addr;	/* function entry point */
	void *gpval;	/* gp value to use */
} pdesc;

static long
default_handler (void)
{
	return -1;
}

ia64_sal_handler ia64_sal = (ia64_sal_handler) default_handler;
ia64_sal_desc_ptc_t *ia64_ptc_domain_info;

const char *
ia64_sal_strerror (long status)
{
	const char *str;
	switch (status) {
	      case 0: str = "Call completed without error"; break;
	      case 1: str = "Effect a warm boot of the system to complete "
			      "the update"; break;
	      case -1: str = "Not implemented"; break;
	      case -2: str = "Invalid argument"; break;
	      case -3: str = "Call completed with error"; break;
	      case -4: str = "Virtual address not registered"; break;
	      case -5: str = "No information available"; break;
	      case -6: str = "Insufficient space to add the entry"; break;
	      case -7: str = "Invalid entry_addr value"; break;
	      case -8: str = "Invalid interrupt vector"; break;
	      case -9: str = "Requested memory not available"; break;
	      case -10: str = "Unable to write to the NVM device"; break;
	      case -11: str = "Invalid partition type specified"; break;
	      case -12: str = "Invalid NVM_Object id specified"; break;
	      case -13: str = "NVM_Object already has the maximum number "
				"of partitions"; break;
	      case -14: str = "Insufficient space in partition for the "
				"requested write sub-function"; break;
	      case -15: str = "Insufficient data buffer space for the "
				"requested read record sub-function"; break;
	      case -16: str = "Scratch buffer required for the write/delete "
				"sub-function"; break;
	      case -17: str = "Insufficient space in the NVM_Object for the "
				"requested create sub-function"; break;
	      case -18: str = "Invalid value specified in the partition_rec "
				"argument"; break;
	      case -19: str = "Record oriented I/O not supported for this "
				"partition"; break;
	      case -20: str = "Bad format of record to be written or "
				"required keyword variable not "
				"specified"; break;
	      default: str = "Unknown SAL status code"; break;
	}
	return str;
}

void __init
ia64_sal_handler_init (void *entry_point, void *gpval)
{
	/* fill in the SAL procedure descriptor and point ia64_sal to it: */
	pdesc.addr = entry_point;
	pdesc.gpval = gpval;
	ia64_sal = (ia64_sal_handler) &pdesc;
}


void __init
ia64_sal_init (struct ia64_sal_systab *systab)
{
	unsigned long min, max;
	char *p;
	struct ia64_sal_desc_entry_point *ep;
	int i;

	if (!systab) {
		printk(KERN_WARNING "Hmm, no SAL System Table.\n");
		return;
	}

	if (strncmp(systab->signature, "SST_", 4) != 0)
		printk(KERN_ERR "bad signature in system table!");

	/*
	 * revisions are coded in BCD, so %x does the job for us
	 */
	printk(KERN_INFO "SAL v%x.%02x: oem=%.32s, product=%.32s\n",
	       systab->sal_rev_major, systab->sal_rev_minor,
	       systab->oem_id, systab->product_id);

	min = ~0UL;
	max = 0;

	p = (char *) (systab + 1);
	for (i = 0; i < systab->entry_count; i++) {
		/*
		 * The first byte of each entry type contains the type desciptor.
		 */
		switch (*p) {
		      case SAL_DESC_ENTRY_POINT:
			ep = (struct ia64_sal_desc_entry_point *) p;
			printk(KERN_INFO "SAL: entry: pal_proc=0x%lx, sal_proc=0x%lx\n",
			       ep->pal_proc, ep->sal_proc);
			ia64_pal_handler_init(__va(ep->pal_proc));
			ia64_sal_handler_init(__va(ep->sal_proc), __va(ep->gp));
			break;

		      case SAL_DESC_PTC:
			ia64_ptc_domain_info = (ia64_sal_desc_ptc_t *)p;
			break;

		      case SAL_DESC_AP_WAKEUP:
#ifdef CONFIG_SMP
		      {
			      struct ia64_sal_desc_ap_wakeup *ap = (void *) p;

			      switch (ap->mechanism) {
				    case IA64_SAL_AP_EXTERNAL_INT:
				      ap_wakeup_vector = ap->vector;
				      printk(KERN_INFO "SAL: AP wakeup using external interrupt "
					     "vector 0x%lx\n", ap_wakeup_vector);
				      break;

				    default:
				      printk(KERN_ERR "SAL: AP wakeup mechanism unsupported!\n");
				      break;
			      }
			      break;
		      }
#endif
		      case SAL_DESC_PLATFORM_FEATURE:
		      {
			      struct ia64_sal_desc_platform_feature *pf = (void *) p;
			      sal_platform_features = pf->feature_mask;
			      printk(KERN_INFO "SAL: Platform features ");

			      if (pf->feature_mask & IA64_SAL_PLATFORM_FEATURE_BUS_LOCK)
				      printk("BusLock ");
			      if (pf->feature_mask & IA64_SAL_PLATFORM_FEATURE_IRQ_REDIR_HINT) {
				      printk("IRQ_Redirection ");
#ifdef CONFIG_SMP
				      if (no_int_routing)
					      smp_int_redirect &= ~SMP_IRQ_REDIRECTION;
				      else
					      smp_int_redirect |= SMP_IRQ_REDIRECTION;
#endif
			      }
			      if (pf->feature_mask & IA64_SAL_PLATFORM_FEATURE_IPI_REDIR_HINT) {
				      printk("IPI_Redirection ");
#ifdef CONFIG_SMP
				      if (no_int_routing)
					      smp_int_redirect &= ~SMP_IPI_REDIRECTION;
				      else
					      smp_int_redirect |= SMP_IPI_REDIRECTION;
#endif
			      }
			      if (pf->feature_mask & IA64_SAL_PLATFORM_FEATURE_ITC_DRIFT)
				      printk("ITC_Drift ");
			      printk("\n");
			      break;
 		      }

		}
		p += SAL_DESC_SIZE(*p);
	}
}

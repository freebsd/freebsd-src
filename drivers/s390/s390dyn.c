/*
 *  arch/s390/kernel/s390dyn.c
 *   S/390 dynamic device attachment
 *
 *  S390 version
 *    Copyright (C) 2000 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Ingo Adlung (adlung@de.ibm.com)
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/smp_lock.h>

#include <asm/irq.h>
#include <asm/s390io.h>
#include <asm/s390dyn.h>

static struct list_head devreg_anchor = LIST_HEAD_INIT(devreg_anchor);
static spinlock_t  dyn_lock           = SPIN_LOCK_UNLOCKED;

static inline int s390_device_register_internal(devreg_t *drinfo)
{
	struct list_head *p;

        list_for_each(p, &devreg_anchor) {
                devreg_t *pdevreg = list_entry(p, devreg_t, list);
                
                if (pdevreg == drinfo)
                        return -EINVAL;
                /*
                 * We don't allow multiple drivers to register
                 * for the same device number
                 */
                if (pdevreg->ci.devno == drinfo->ci.devno &&
                    (pdevreg->flag & DEVREG_TYPE_DEVNO) &&
                    (drinfo->flag & DEVREG_TYPE_DEVNO))
			return -EBUSY;

                if (drinfo->flag == (DEVREG_TYPE_DEVCHARS | 
				     DEVREG_EXACT_MATCH) &&
                    !memcmp(&drinfo->ci.hc, &pdevreg->ci.hc,
		            sizeof(devreg_hc_t))) 
			return -EBUSY;
        }

        /*
         * no collision found, enqueue
         */
        list_add (&drinfo->list, &devreg_anchor);
	
	return 0;
}

int s390_device_register( devreg_t *drinfo )
{
	unsigned long flags;
	int ret;

	if (drinfo == NULL ||
            !(drinfo->flag & (DEVREG_TYPE_DEVNO | DEVREG_TYPE_DEVCHARS)))
		return -EINVAL;

	spin_lock_irqsave (&dyn_lock, flags); 	
	ret = s390_device_register_internal(drinfo);
	spin_unlock_irqrestore( &dyn_lock, flags ); 	
 	
	return ret;
}

static inline int s390_device_unregister_internal(devreg_t *dreg)
{
	struct list_head *p;

        list_for_each(p, &devreg_anchor) {
                devreg_t *pdevreg = list_entry(p, devreg_t, list);

		if (pdevreg == dreg) {
			list_del (&dreg->list);
			return 0;
		}
        }
	return -EINVAL;
}

int s390_device_unregister(devreg_t *dreg)
{
	unsigned long  flags;
	int ret;

	if (dreg == NULL)
		return -EINVAL;

	spin_lock_irqsave(&dyn_lock, flags); 	
	ret = s390_device_unregister_internal(dreg);
	spin_unlock_irqrestore(&dyn_lock, flags); 	
 	
	return ret;
}

static inline devreg_t *s390_search_devreg_internal(ioinfo_t *ioinfo)
{
	struct list_head *p;
	
        list_for_each(p, &devreg_anchor) {
                devreg_t *pdevreg = list_entry(p, devreg_t, list);
		senseid_t *sid;
		int flag;

		flag = pdevreg->flag;
		sid = &ioinfo->senseid;
		if (flag & DEVREG_TYPE_DEVNO) {
                        if (ioinfo->ui.flags.dval != 1 ||
		            ioinfo->devno != pdevreg->ci.devno)
				continue;
		} else if (flag & DEVREG_TYPE_DEVCHARS) {
			if ( (flag & DEVREG_MATCH_CU_TYPE) &&
			     pdevreg->ci.hc.ctype != sid->cu_type )
				continue;
			if ( (flag & DEVREG_MATCH_CU_MODEL) &&
			     pdevreg->ci.hc.cmode != sid->cu_model )
				continue;
			if ( (flag & DEVREG_MATCH_DEV_TYPE) &&
			     pdevreg->ci.hc.dtype != sid->dev_type )
				continue;
			if ( (flag & DEVREG_MATCH_DEV_MODEL) &&
			     pdevreg->ci.hc.dmode != sid->dev_model )
				continue;
		} else {
			continue;
		}
		
		return pdevreg;
	}
	return NULL;
}

devreg_t * s390_search_devreg( ioinfo_t *ioinfo )
{
	unsigned long  flags;
	devreg_t *pdevreg;

	if (ioinfo == NULL)
		return NULL;

	spin_lock_irqsave(&dyn_lock, flags); 	
	pdevreg = s390_search_devreg_internal(ioinfo);
	spin_unlock_irqrestore(&dyn_lock, flags); 	
 	
	return pdevreg;
}

EXPORT_SYMBOL(s390_device_register);
EXPORT_SYMBOL(s390_device_unregister);


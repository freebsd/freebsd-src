/*
 * Equalizer Load-balancer for serial network interfaces.
 *
 * (c) Copyright 1995 Simon "Guru Aleph-Null" Janes
 * NCM: Network and Communications Management, Inc.
 *
 *
 *	This software may be used and distributed according to the terms
 *	of the GNU General Public License, incorporated herein by reference.
 * 
 * The author may be reached as simon@ncm.com, or C/O
 *    NCM
 *    Attn: Simon Janes
 *    6803 Whittier Ave
 *    McLean VA 22101
 *    Phone: 1-703-847-0040 ext 103
 */

/*
 * Sources:
 *   skeleton.c by Donald Becker.
 * Inspirations:
 *   The Harried and Overworked Alan Cox
 * Conspiracies:
 *   The Alan Cox and Mike McLagan plot to get someone else to do the code, 
 *   which turned out to be me.
 */

/*
 * $Log: eql.c,v $
 * Revision 1.2  1996/04/11 17:51:52  guru
 * Added one-line eql_remove_slave patch.
 *
 * Revision 1.1  1996/04/11 17:44:17  guru
 * Initial revision
 *
 * Revision 3.13  1996/01/21  15:17:18  alan
 * tx_queue_len changes.
 * reformatted.
 *
 * Revision 3.12  1995/03/22  21:07:51  anarchy
 * Added capable() checks on configuration.
 * Moved header file.
 *
 * Revision 3.11  1995/01/19  23:14:31  guru
 * 		      slave_load = (ULONG_MAX - (ULONG_MAX / 2)) -
 * 			(priority_Bps) + bytes_queued * 8;
 *
 * Revision 3.10  1995/01/19  23:07:53  guru
 * back to
 * 		      slave_load = (ULONG_MAX - (ULONG_MAX / 2)) -
 * 			(priority_Bps) + bytes_queued;
 *
 * Revision 3.9  1995/01/19  22:38:20  guru
 * 		      slave_load = (ULONG_MAX - (ULONG_MAX / 2)) -
 * 			(priority_Bps) + bytes_queued * 4;
 *
 * Revision 3.8  1995/01/19  22:30:55  guru
 *       slave_load = (ULONG_MAX - (ULONG_MAX / 2)) -
 * 			(priority_Bps) + bytes_queued * 2;
 *
 * Revision 3.7  1995/01/19  21:52:35  guru
 * printk's trimmed out.
 *
 * Revision 3.6  1995/01/19  21:49:56  guru
 * This is working pretty well. I gained 1 K/s in speed.. now it's just
 * robustness and printk's to be diked out.
 *
 * Revision 3.5  1995/01/18  22:29:59  guru
 * still crashes the kernel when the lock_wait thing is woken up.
 *
 * Revision 3.4  1995/01/18  21:59:47  guru
 * Broken set-bit locking snapshot
 *
 * Revision 3.3  1995/01/17  22:09:18  guru
 * infinite sleep in a lock somewhere..
 *
 * Revision 3.2  1995/01/15  16:46:06  guru
 * Log trimmed of non-pertinent 1.x branch messages
 *
 * Revision 3.1  1995/01/15  14:41:45  guru
 * New Scheduler and timer stuff...
 *
 * Revision 1.15  1995/01/15  14:29:02  guru
 * Will make 1.14 (now 1.15) the 3.0 branch, and the 1.12 the 2.0 branch, the one
 * with the dumber scheduler
 *
 * Revision 1.14  1995/01/15  02:37:08  guru
 * shock.. the kept-new-versions could have zonked working
 * stuff.. shudder
 *
 * Revision 1.13  1995/01/15  02:36:31  guru
 * big changes
 *
 * 	scheduler was torn out and replaced with something smarter
 *
 * 	global names not prefixed with eql_ were renamed to protect
 * 	against namespace collisions
 *
 * 	a few more abstract interfaces were added to facilitate any
 * 	potential change of datastructure.  the driver is still using
 * 	a linked list of slaves.  going to a heap would be a bit of
 * 	an overkill.
 *
 * 	this compiles fine with no warnings.
 *
 * 	the locking mechanism and timer stuff must be written however,
 * 	this version will not work otherwise
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/netdevice.h>

#include <linux/if.h>
#include <linux/if_arp.h>
#include <linux/if_eql.h>

#include <asm/uaccess.h>

static char version[] __initdata = 
	"Equalizer1996: $Revision: 1.2.1 $ $Date: 1996/09/22 13:52:00 $ Simon Janes (simon@ncm.com)\n";

#ifndef EQL_DEBUG
/* #undef EQL_DEBUG      -* print nothing at all, not even a boot-banner */
/* #define EQL_DEBUG 1   -* print only the boot-banner */
/* #define EQL_DEBUG 5   -* print major function entries */
/* #define EQL_DEBUG 20  -* print subfunction entries */
/* #define EQL_DEBUG 50  -* print utility entries */
/* #define EQL_DEBUG 100 -* print voluminous function entries */
#define EQL_DEBUG 1
#endif
static unsigned int eql_debug = EQL_DEBUG;

static int eql_init(struct net_device *dev); /*  */
static int eql_open(struct net_device *dev); /*  */
static int eql_close(struct net_device *dev); /*  */
static int eql_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd); /*  */
static int eql_slave_xmit(struct sk_buff *skb, struct net_device *dev); /*  */

static struct net_device_stats *eql_get_stats(struct net_device *dev); /*  */

/* ioctl() handlers
   ---------------- */
static int eql_enslave(struct net_device *dev,  slaving_request_t *srq); /*  */
static int eql_emancipate(struct net_device *dev, slaving_request_t *srq); /*  */

static int eql_g_slave_cfg(struct net_device *dev, slave_config_t *sc); /*  */
static int eql_s_slave_cfg(struct net_device *dev, slave_config_t *sc); /*  */

static int eql_g_master_cfg(struct net_device *dev, master_config_t *mc); /*  */
static int eql_s_master_cfg(struct net_device *dev, master_config_t *mc); /*  */

static inline int eql_is_slave(struct net_device *dev); /*  */
static inline int eql_is_master(struct net_device *dev); /*  */

static slave_t *eql_new_slave(void); /*  */
static void eql_delete_slave(slave_t *slave); /*  */

/* static long eql_slave_priority(slave_t *slave); -*  */
static inline int eql_number_slaves(slave_queue_t *queue); /*  */

static inline int eql_is_empty(slave_queue_t *queue); /*  */
static inline int eql_is_full(slave_queue_t *queue); /*  */

static slave_queue_t *eql_new_slave_queue(struct net_device *dev); /*  */
static void eql_delete_slave_queue(slave_queue_t *queue); /*  */

static int eql_insert_slave(slave_queue_t *queue, slave_t *slave); /*  */
static slave_t *eql_remove_slave(slave_queue_t *queue, slave_t *slave); /*  */

/* static int eql_insert_slave_dev(slave_queue_t *queue, struct net_device *dev); -*  */
static int eql_remove_slave_dev(slave_queue_t *queue, struct net_device *dev); /*  */

static inline struct net_device *eql_best_slave_dev(slave_queue_t *queue); /*  */
static inline slave_t *eql_best_slave(slave_queue_t *queue); /*  */
static inline slave_t *eql_first_slave(slave_queue_t *queue); /*  */
static inline slave_t *eql_next_slave(slave_queue_t *queue, slave_t *slave); /*  */

static inline void eql_set_best_slave(slave_queue_t *queue, slave_t *slave); /*  */
static inline void eql_schedule_slaves(slave_queue_t *queue); /*  */

static slave_t *eql_find_slave_dev(slave_queue_t *queue, struct net_device *dev); /*  */

/* static inline eql_lock_slave_queue(slave_queue_t *queue); -*  */
/* static inline eql_unlock_slave_queue(slave_queue_t *queue); -*  */

static void eql_timer(unsigned long param);	/*  */

/* struct net_device * interface functions 
   ---------------------------------------------------------
   */

static int __init eql_init(struct net_device *dev)
{
	static unsigned version_printed;
	/* static unsigned num_masters     = 0; */
	equalizer_t *eql = 0;

	SET_MODULE_OWNER(dev);

	if ( version_printed++ == 0 && eql_debug > 0)
		printk(version);
	/*
	 *	Initialize the device structure. 
	 */
	dev->priv = kmalloc (sizeof (equalizer_t), GFP_KERNEL);
	if (dev->priv == NULL)
		return -ENOMEM;
	memset (dev->priv, 0, sizeof (equalizer_t));
	eql = (equalizer_t *) dev->priv;

	eql->stats = kmalloc (sizeof (struct net_device_stats), GFP_KERNEL);
	if (eql->stats == NULL) 
	{
		kfree(dev->priv);
		dev->priv = NULL;
		return -ENOMEM;
	}
	memset (eql->stats, 0, sizeof (struct net_device_stats));

	init_timer (&eql->timer);
	eql->timer.data     	= (unsigned long) dev->priv;
	eql->timer.expires  	= jiffies+EQL_DEFAULT_RESCHED_IVAL;
	eql->timer.function 	= &eql_timer;
	eql->timer_on       	= 0;

	dev->open		= eql_open;
	dev->stop		= eql_close;
	dev->do_ioctl		= eql_ioctl;
	dev->hard_start_xmit	= eql_slave_xmit;
	dev->get_stats		= eql_get_stats;
  
  	/*
  	 *	Fill in the fields of the device structure with 
	 *	eql-generic values. 
	 */

	/*
	 *	Now we undo some of the things that eth_setup does
	 * 	that we don't like 
	 */
	 
	dev->mtu        	= EQL_DEFAULT_MTU;	/* set to 576 in eql.h */
	dev->flags      	= IFF_MASTER;

	dev->type       	= ARPHRD_SLIP;
	dev->tx_queue_len 	= 5;		/* Hands them off fast */

	return 0;
}

static int eql_open(struct net_device *dev)
{
	equalizer_t *eql = (equalizer_t *) dev->priv;
	slave_queue_t *new_queue;

#ifdef EQL_DEBUG
	if (eql_debug >= 5)
		printk ("%s: open\n", dev->name);
#endif

	printk ("%s: remember to turn off Van-Jacobson compression on your slave devices.\n", dev->name);

	new_queue = eql_new_slave_queue (dev);
    
	if (new_queue != 0)
	{
		new_queue->master_dev = dev;
		eql->queue = new_queue;
		eql->queue->lock = 0;
		eql->min_slaves = 1;
		eql->max_slaves = EQL_DEFAULT_MAX_SLAVES; /* 4 usually... */

		printk ("%s: adding timer\n", dev->name);
		eql->timer_on = 1;
		add_timer (&eql->timer);

		return 0;
	}
	return -ENOMEM;
}


static int eql_close(struct net_device *dev)
{
	equalizer_t *eql = (equalizer_t *) dev->priv;

#ifdef EQL_DEBUG
	if ( eql_debug >= 5)
		printk ("%s: close\n", dev->name);
#endif
	/*
	 *	The timer has to be stopped first before we start hacking away
	 *	at the data structure it scans every so often... 
	 */

#ifdef EQL_DEBUG
	printk ("%s: stopping timer\n", dev->name);
#endif	
	eql->timer_on = 0;
	del_timer (&eql->timer);

	eql_delete_slave_queue (eql->queue);

	return 0;
}


static int eql_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{  
	if(cmd!=EQL_GETMASTRCFG && cmd!=EQL_GETSLAVECFG && 
	   !capable(CAP_NET_ADMIN))
	  	return -EPERM;
	switch (cmd)
	{
		case EQL_ENSLAVE:
			return eql_enslave (dev, (slaving_request_t *) ifr->ifr_data);
		case EQL_EMANCIPATE:
			return eql_emancipate (dev, (slaving_request_t *) ifr->ifr_data);
		case EQL_GETSLAVECFG:
			return eql_g_slave_cfg (dev, (slave_config_t *) ifr->ifr_data);
		case EQL_SETSLAVECFG:
			return eql_s_slave_cfg (dev, (slave_config_t *) ifr->ifr_data);
		case EQL_GETMASTRCFG:
			return eql_g_master_cfg (dev, (master_config_t *) ifr->ifr_data);
		case EQL_SETMASTRCFG:
			return eql_s_master_cfg (dev, (master_config_t *) ifr->ifr_data);
		default:
			return -EOPNOTSUPP;
	}
}


static int eql_slave_xmit(struct sk_buff *skb, struct net_device *dev)
{
	equalizer_t *eql = (equalizer_t *) dev->priv;
	struct net_device *slave_dev = 0;
	slave_t *slave;

	if (skb == NULL)
		return 0;

	eql_schedule_slaves (eql->queue);
  
	slave = eql_best_slave (eql->queue); 
	slave_dev = slave ? slave->dev : 0;

	if ( slave_dev != 0 )
	{
#ifdef EQL_DEBUG
		if (eql_debug >= 100)
			printk ("%s: %d slaves xmitng %d B %s\n", 
				dev->name, eql_number_slaves (eql->queue), skb->len,
				slave_dev->name);
#endif
		skb->dev = slave_dev;
		skb->priority = 1;
		slave->bytes_queued += skb->len; 
		dev_queue_xmit (skb);
		eql->stats->tx_packets++;
	}
	else
	{
		/*
		 *	The alternative for this is the return 1 and have
		 *	dev_queue_xmit just queue it up on the eql's queue. 
		 */

		eql->stats->tx_dropped++;
		dev_kfree_skb(skb);
	}	  
	return 0;
}


static struct net_device_stats * eql_get_stats(struct net_device *dev)
{
	equalizer_t *eql = (equalizer_t *) dev->priv;
	return eql->stats;
}

/*
 *	Private ioctl functions
 */

static int eql_enslave(struct net_device *dev, slaving_request_t *srqp)
{
	struct net_device *master_dev;
	struct net_device *slave_dev;
	slaving_request_t srq;

	if (copy_from_user(&srq, srqp, sizeof (slaving_request_t)))
	  {
#ifdef EQL_DEBUG
	if (eql_debug >= 20)
		printk ("EQL enslave: error detected by copy_from_user\n");
#endif  
		return -EFAULT;
	  }

#ifdef EQL_DEBUG
	if (eql_debug >= 20)
		printk ("%s: enslave '%s' %ld bps\n", dev->name, 
			srq.slave_name, srq.priority);
#endif  
	master_dev = dev;		/* for "clarity" */
	slave_dev  = __dev_get_by_name (srq.slave_name);

	if (master_dev != 0 && slave_dev != 0)
	{
		if ((master_dev->flags & IFF_UP) == IFF_UP)
                {
			/*slave is not a master & not already a slave:*/
			if (! eql_is_master (slave_dev)  &&
			    ! eql_is_slave (slave_dev) )
			{
				slave_t *s = eql_new_slave ();
				equalizer_t *eql = 
					(equalizer_t *) master_dev->priv;
				if (!s)
					return -ENOMEM;
				s->dev = slave_dev;
				s->priority = srq.priority;
				s->priority_bps = srq.priority;
				s->priority_Bps = srq.priority / 8;
				slave_dev->flags |= IFF_SLAVE;
				eql_insert_slave (eql->queue, s);
				return 0;
			}
#ifdef EQL_DEBUG
			else if (eql_debug >= 20)
				printk ("EQL enslave: slave is master or slave is already slave\n");
#endif  
		}
#ifdef EQL_DEBUG
		else if (eql_debug >= 20)
			printk ("EQL enslave: master device not up!\n");
#endif  
	}
#ifdef EQL_DEBUG
	else if (eql_debug >= 20)
		printk ("EQL enslave: master or slave are NULL");
#endif  
	return -EINVAL;
}

static int eql_emancipate(struct net_device *dev, slaving_request_t *srqp)
{
	struct net_device *master_dev;
	struct net_device *slave_dev;
	slaving_request_t srq;

	if (copy_from_user(&srq, srqp, sizeof (slaving_request_t)))
		return -EFAULT;

#ifdef EQL_DEBUG
	if (eql_debug >= 20)
		printk ("%s: emancipate `%s`\n", dev->name, srq.slave_name);
#endif
	master_dev = dev;		/* for "clarity" */
	slave_dev  = __dev_get_by_name (srq.slave_name);

	if ( eql_is_slave (slave_dev) )	/* really is a slave */
	{
		equalizer_t *eql = (equalizer_t *) master_dev->priv;
		slave_dev->flags = slave_dev->flags & ~IFF_SLAVE;
		eql_remove_slave_dev (eql->queue, slave_dev);
		return 0;
	}
	return -EINVAL;
}


static int eql_g_slave_cfg(struct net_device *dev, slave_config_t *scp)
{
	slave_t *slave;
	equalizer_t *eql;
	struct net_device *slave_dev;
	slave_config_t sc;

	if (copy_from_user (&sc, scp, sizeof (slave_config_t)))
		return -EFAULT;

#ifdef EQL_DEBUG
	if (eql_debug >= 20)
		printk ("%s: get config for slave `%s'\n", dev->name, sc.slave_name);
#endif
	eql = (equalizer_t *) dev->priv;
	slave_dev = __dev_get_by_name (sc.slave_name);

	if ( eql_is_slave (slave_dev) )
	{
		slave = eql_find_slave_dev (eql->queue,  slave_dev);
		if (slave != 0)
		{
			sc.priority = slave->priority;
			if (copy_to_user (scp, &sc, sizeof (slave_config_t)))
				return -EFAULT;
			return 0;
		}
	}
	return -EINVAL;
}


static int eql_s_slave_cfg(struct net_device *dev, slave_config_t *scp)
{
	slave_t *slave;
	equalizer_t *eql;
	struct net_device *slave_dev;
	slave_config_t sc;

	if (copy_from_user (&sc, scp, sizeof (slave_config_t)))
		return -EFAULT;

#ifdef EQL_DEBUG
	if (eql_debug >= 20)
		printk ("%s: set config for slave `%s'\n", dev->name, sc.slave_name);
#endif
  

	eql = (equalizer_t *) dev->priv;
	slave_dev = __dev_get_by_name (sc.slave_name);

	if ( eql_is_slave (slave_dev) )
	{
		slave = eql_find_slave_dev (eql->queue, slave_dev);
		if (slave != 0)
		{
			slave->priority = sc.priority;
			slave->priority_bps = sc.priority;
			slave->priority_Bps = sc.priority / 8;
			return 0;
		}
	}
	return -EINVAL;
}


static int eql_g_master_cfg(struct net_device *dev, master_config_t *mcp)
{
	equalizer_t *eql;
	master_config_t mc;

#if EQL_DEBUG
	if (eql_debug >= 20)
		printk ("%s: get master config\n", dev->name);
#endif

	if ( eql_is_master (dev) )
	{
		eql = (equalizer_t *) dev->priv;
		mc.max_slaves = eql->max_slaves;
		mc.min_slaves = eql->min_slaves;
		if (copy_to_user (mcp, &mc, sizeof (master_config_t)))
			return -EFAULT;
		return 0;
	}
	return -EINVAL;
}


static int eql_s_master_cfg(struct net_device *dev, master_config_t *mcp)
{
	equalizer_t *eql;
	master_config_t mc;

	if (copy_from_user (&mc, mcp, sizeof (master_config_t)))
		return -EFAULT;
#if EQL_DEBUG
	if (eql_debug >= 20)
		printk ("%s: set master config\n", dev->name);
#endif
	if ( eql_is_master (dev) )
	{
		eql = (equalizer_t *) dev->priv;
		eql->max_slaves = mc.max_slaves;
		eql->min_slaves = mc.min_slaves;
		return 0;
	}
	return -EINVAL;
}

/*
 *	Private device support functions
 */

static inline int eql_is_slave(struct net_device *dev)
{
	if (dev)
	{
		if ((dev->flags & IFF_SLAVE) == IFF_SLAVE)
			return 1;
	}
	return 0;
}


static inline int eql_is_master(struct net_device *dev)
{
	if (dev)
	{
		if ((dev->flags & IFF_MASTER) == IFF_MASTER)
		return 1;
	}
	return 0;
}


static slave_t *eql_new_slave(void)
{
	slave_t *slave;

	slave = (slave_t *) kmalloc (sizeof (slave_t), GFP_KERNEL);
	if (slave)
		memset(slave, 0, sizeof (slave_t));
	return slave;
}


static void eql_delete_slave(slave_t *slave)
{
	kfree (slave);
}


#if 0				/* not currently used, will be used
				   when we really use a priority queue */
static long slave_Bps(slave_t *slave)
{
	return (slave->priority_Bps);
}

static long slave_bps(slave_t *slave)
{
	return (slave->priority_bps);
}

#endif

static inline int eql_number_slaves(slave_queue_t *queue)
{
	return queue->num_slaves;
}

static inline int eql_is_empty(slave_queue_t *queue)
{
	if (eql_number_slaves (queue) == 0)
		return 1;
	return 0;
}

static inline int eql_is_full(slave_queue_t *queue)
{
	equalizer_t *eql = (equalizer_t *) queue->master_dev->priv;

	if (eql_number_slaves (queue) == eql->max_slaves)
		return 1;
	return 0;
}

static slave_queue_t *eql_new_slave_queue(struct net_device *dev)
{
	slave_queue_t *queue;
	slave_t *head_slave;
	slave_t *tail_slave;

	queue = (slave_queue_t *) kmalloc (sizeof (slave_queue_t), GFP_KERNEL);
	if (!queue)
		goto err_out;

	head_slave = eql_new_slave ();
	if (!head_slave)
		goto err_out_queue;

	tail_slave = eql_new_slave ();
	if (!tail_slave)
		goto err_out_hs;

	memset (queue, 0, sizeof (slave_queue_t));

	head_slave->next = tail_slave;
	tail_slave->next = 0;
	queue->head = head_slave;
	queue->num_slaves = 0;
	queue->master_dev = dev;
	return queue;

err_out_hs:
	kfree (head_slave);
err_out_queue:
	kfree (queue);
err_out:
	return NULL;
}


static void eql_delete_slave_queue(slave_queue_t *queue)
{ 
	slave_t *zapped;
	/*
	 *	This should only be called when there isn't a
	 *	timer running that scans the data periodically.. 
	 *	dev_close stops the timer... 
	 */

	while ( ! eql_is_empty (queue) )
	{
		zapped = eql_remove_slave (queue, queue->head->next);
		eql_delete_slave (zapped);
	}
	kfree (queue->head->next);
	kfree (queue->head);
	kfree (queue);
}

static int eql_insert_slave(slave_queue_t *queue, slave_t *slave)
{
	unsigned long flags;

	save_flags(flags);
	cli ();

	if ( ! eql_is_full (queue) )
	{
		slave_t *duplicate_slave = 0;
		duplicate_slave = eql_find_slave_dev (queue, slave->dev);
		if (duplicate_slave != 0)
		{
/*			  printk ("%s: found a duplicate, killing it and replacing\n",
				  queue->master_dev->name); */
			eql_delete_slave (eql_remove_slave (queue, duplicate_slave));
		}
		slave->next = queue->head->next;
		queue->head->next = slave;
		queue->num_slaves++;
		restore_flags(flags);
		return 0;
	}
	restore_flags(flags);
	return 1;
}


static slave_t *eql_remove_slave(slave_queue_t *queue, slave_t *slave)
{
	slave_t *prev;
	slave_t *curr;
	unsigned long flags;

	save_flags(flags);
	cli ();

	prev = queue->head;
	curr = queue->head->next;
	while (curr != slave && 
		curr->dev != 0 )
	{
/* 		printk ("%s: remove_slave; searching...\n", queue->master_dev->name); */
		prev = curr;
		curr = curr->next;
	}

	if (curr == slave)
	{
		prev->next = curr->next;
		queue->num_slaves--;
		curr->dev->flags = curr->dev->flags & ~IFF_SLAVE;
		restore_flags(flags);
		return curr;
	}
	restore_flags(flags);
	return 0;			/* not found */
}


static int eql_remove_slave_dev(slave_queue_t *queue, struct net_device *dev)
{
	slave_t *prev;
	slave_t *curr;
	slave_t *target;

	target = eql_find_slave_dev (queue, dev);

	if (target != 0)
	{
		unsigned long flags;

		save_flags(flags);
		cli ();
		prev = queue->head;
		curr = prev->next;
		while (curr != target)
		{
			prev = curr;
			curr = curr->next;
		}
		prev->next = curr->next;
		queue->num_slaves--;
		restore_flags(flags);
		eql_delete_slave (curr);
		return 0;
	}
	return 1;
}


static inline struct net_device *eql_best_slave_dev(slave_queue_t *queue)
{
	if (queue->best_slave != 0)
	{
		if (queue->best_slave->dev != 0)
			return queue->best_slave->dev;
		else
			return 0;
	}
	else
		return 0;
}


static inline slave_t *eql_best_slave(slave_queue_t *queue)
{
	return queue->best_slave;
}

static inline void eql_schedule_slaves(slave_queue_t *queue)
{
	struct net_device *master_dev = queue->master_dev;
	slave_t *best_slave = 0;
	slave_t *slave_corpse = 0;

#ifdef EQL_DEBUG
	if (eql_debug >= 100)
		printk ("%s: schedule %d slaves\n", 
			master_dev->name, eql_number_slaves (queue));
#endif
	if ( eql_is_empty (queue) )
	{
		/*
		 *	No slaves to play with 
		 */
		eql_set_best_slave (queue, (slave_t *) 0);
		return;
	}
	else
	{		
		/*
		 *	Make a pass to set the best slave 
		 */
		unsigned long best_load = (unsigned long) ULONG_MAX;
		slave_t *slave = 0;
		unsigned long flags;
		int i;

		save_flags(flags);
		cli ();
		for (i = 1, slave = eql_first_slave (queue);
			i <= eql_number_slaves (queue);
			i++, slave = eql_next_slave (queue, slave))
		{
			/*
			 *	Go through the slave list once, updating best_slave 
			 *      whenever a new best_load is found, whenever a dead
			 *	slave is found, it is marked to be pulled out of the 
			 *	queue 
			 */
		
			unsigned long slave_load;
			unsigned long bytes_queued; 
			unsigned long priority_Bps; 
	  	
	  		if (slave != 0)
			{
				bytes_queued = slave->bytes_queued;
				priority_Bps = slave->priority_Bps;    
				if ( slave->dev != 0)
				{
					if ((slave->dev->flags & IFF_UP) == IFF_UP )
					{
						slave_load = (ULONG_MAX - (ULONG_MAX / 2)) - 
							(priority_Bps) + bytes_queued * 8;

		      				if (slave_load < best_load)
						{
							best_load = slave_load;
							best_slave = slave;
						}
					}
					else		/* we found a dead slave */
					{
						/* 
						 *	We only bury one slave at a time, if more than
						 *	one slave dies, we will bury him on the next 
						 *	reschedule. slaves don't die all at once that 
						 *	much anyway 
						 */
						slave_corpse = slave;
					}
				}
			}
		} /* for */
		restore_flags(flags);
		eql_set_best_slave (queue, best_slave);
	} /* else */
	if (slave_corpse != 0)
	{
		printk ("eql: scheduler found dead slave, burying...\n");
		eql_delete_slave (eql_remove_slave (queue, slave_corpse));
	}
	return;
}


static slave_t * eql_find_slave_dev(slave_queue_t *queue, struct net_device *dev)
{
	slave_t *slave = 0;
	slave = eql_first_slave(queue);

	while (slave != 0 && slave->dev != dev && slave != 0)
	{
#if 0
		if (slave->dev != 0)
			printk ("eql: find_slave_dev; looked at '%s'...\n", slave->dev->name);
		else
			printk ("eql: find_slave_dev; looked at nothing...\n");
#endif
		slave = slave->next;
	}
	return slave;
}


static inline slave_t *eql_first_slave(slave_queue_t *queue)
{
	return queue->head->next;
}


static inline slave_t *eql_next_slave(slave_queue_t *queue, slave_t *slave)
{
	return slave->next;
}

static inline void eql_set_best_slave(slave_queue_t *queue, slave_t *slave)
{
	queue->best_slave = slave;
}

static void eql_timer(unsigned long param)
{
	equalizer_t *eql = (equalizer_t *) param;
	slave_t *slave;
	slave_t *slave_corpse = 0;
	int i;
	unsigned long flags;
	
	if ( ! eql_is_empty (eql->queue) )
	{
		save_flags(flags);
		cli ();
		for (i = 1, slave = eql_first_slave (eql->queue);
			i <= eql_number_slaves (eql->queue);
			i++, slave = eql_next_slave (eql->queue, slave))
		{
			if (slave != 0)
			{
				if ((slave->dev->flags & IFF_UP) == IFF_UP )
				{
					slave->bytes_queued -= slave->priority_Bps;
					if (slave->bytes_queued < 0)
						slave->bytes_queued = 0;
				}
				else
					slave_corpse = slave;
			}
		}
		restore_flags(flags);
		if (slave_corpse != 0)
		{
			printk ("eql: timer found dead slave, burying...\n");
			eql_delete_slave (eql_remove_slave (eql->queue, slave_corpse));
		}
	}

	if (eql->timer_on != 0) 
	{
		eql->timer.expires = jiffies+EQL_DEFAULT_RESCHED_IVAL;
		add_timer (&eql->timer);
	}
}

static struct net_device dev_eql;

static int __init eql_init_module(void)
{
	strcpy(dev_eql.name, "eql");
	dev_eql.init = eql_init;
	if (register_netdev(&dev_eql) != 0) {
		printk("eql: register_netdev() returned non-zero.\n");
		return -EIO;
	}
	return 0;
}

static void __exit eql_cleanup_module(void)
{
	kfree(((equalizer_t *)dev_eql.priv)->stats );
	kfree(dev_eql.priv);
	unregister_netdev(&dev_eql);
}

module_init(eql_init_module);
module_exit(eql_cleanup_module);
MODULE_LICENSE("GPL");

/*
 * Local Variables: 
 * compile-command: "gcc -D__KERNEL__ -I/usr/src/linux/net/inet -Wall -Wstrict-prototypes -O6 -m486 -c eql.c"
 * version-control: t
 * kept-new-versions: 20
 * End:
 */

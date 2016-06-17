/*
 *  drivers/s390/misc/chandev.c
 *
 *    Copyright (C) 2000 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Denis Joseph Barrow (djbarrow@de.ibm.com,barrow_dj@yahoo.com)
 * 
 *  Generic channel device initialisation support. 
 */
#define TRUE 1
#define FALSE 0
#define __KERNEL_SYSCALLS__
#include <linux/module.h>
#include <linux/config.h>
#include <linux/types.h>
#include <linux/ctype.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <asm/irq.h>
#include <linux/init.h>
#include <linux/unistd.h>
#include <asm/chandev.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <asm/s390dyn.h>
#include <asm/queue.h>
#include <linux/kmod.h>
#ifndef MIN
#define MIN(a,b) ((a<b)?a:b)
#endif
#ifndef MAX
#define MAX(a,b) ((a>b)?a:b)
#endif



typedef struct chandev_model_info chandev_model_info;
struct chandev_model_info
{
	struct chandev_model_info *next;
	chandev_type chan_type;
	s32 cu_type;      /* control unit type  -1 = don't care */
	s16 cu_model;     /* control unit model -1 = don't care */
	s32 dev_type;     /* device type -1 = don't care */
	s16 dev_model;    /* device model -1 = don't care */
	u8  max_port_no;
	int auto_msck_recovery;
	u8  default_checksum_received_ip_pkts;
	u8  default_use_hw_stats; /* where available e.g. lcs */
	devreg_t drinfo;
};

typedef struct chandev chandev;
struct chandev
{
	struct chandev *next;
	chandev_model_info *model_info;
	chandev_subchannel_info sch;
	int owned;
};

typedef struct chandev_noauto_range chandev_noauto_range;
struct chandev_noauto_range
{
	struct chandev_noauto_range *next;
	u16     lo_devno;
	u16     hi_devno;
};

typedef struct chandev_force chandev_force;
struct chandev_force
{
	struct chandev_force *next;
	chandev_type chan_type;
	s32     devif_num; /* -1 don't care, -2 we are forcing a range e.g. tr0 implies 0 */
        u16     read_lo_devno;
	u16     write_hi_devno;
	u16     data_devno; /* only used by gigabit ethernet */
	s32     memory_usage_in_k;
        s16     port_protocol_no; /* where available e.g. lcs,-1 don't care */
	u8      checksum_received_ip_pkts;
	u8      use_hw_stats; /* where available e.g. lcs */
	/* claw specific stuff */
	chandev_claw_info  claw;
};

typedef struct chandev_probelist chandev_probelist;
struct chandev_probelist
{
	struct chandev_probelist            *next;
	chandev_probefunc                   probefunc;
	chandev_shutdownfunc                shutdownfunc;
	chandev_msck_notification_func      msck_notfunc;
	chandev_type                        chan_type;
	int                                 devices_found;
};



#define default_msck_bits ((1<<(chandev_status_not_oper-1))|(1<<(chandev_status_no_path-1))|(1<<(chandev_status_revalidate-1))|(1<<(chandev_status_gone-1)))


static char *msck_status_strs[]=
{
	"good",
	"not_operational",
	"no_path",
	"revalidate",
	"device_gone"
};

typedef struct chandev_msck_range chandev_msck_range;
struct chandev_msck_range
{
	struct chandev_msck_range *next;
	u16     lo_devno;
	u16     hi_devno;
	int      auto_msck_recovery;
};

static chandev_msck_range *chandev_msck_range_head=NULL;

typedef struct chandev_irqinfo chandev_irqinfo;
struct chandev_irqinfo
{
	chandev_irqinfo         *next;
	chandev_subchannel_info sch;
	chandev_msck_status     msck_status;
	void                    (*handler)(int, void *, struct pt_regs *);
	unsigned long           irqflags;
	void                    *dev_id;
	char                    devname[0];
};


chandev_irqinfo *chandev_irqinfo_head=NULL;

typedef struct chandev_parms chandev_parms;
struct chandev_parms
{
	chandev_parms      *next;
	chandev_type       chan_type;
	u16                lo_devno;
	u16                hi_devno;
	char               parmstr[0];
};

static chandev_type chandev_persistent=0; 

chandev_parms *chandev_parms_head=NULL;


typedef struct chandev_activelist chandev_activelist;
struct chandev_activelist
{
	struct chandev_activelist        *next;
	chandev_irqinfo                  *read_irqinfo;
	chandev_irqinfo                  *write_irqinfo;
	chandev_irqinfo                  *data_irqinfo;
	chandev_probefunc                probefunc;
	chandev_shutdownfunc             shutdownfunc;
	chandev_msck_notification_func   msck_notfunc;
	chandev_unregfunc                unreg_dev;
	chandev_type                     chan_type;
	u8                               port_no;
	chandev_category                 category;
	s32                              memory_usage_in_k;
	void                             *dev_ptr;
	char                             devname[0];
};



static chandev_model_info *chandev_models_head=NULL;
/* The only reason chandev_head is a queue is so that net devices */
/* will be by default named in the order of their irqs */
static qheader chandev_head={NULL,NULL};
static chandev_noauto_range *chandev_noauto_head=NULL;
static chandev_force *chandev_force_head=NULL;
static chandev_probelist *chandev_probelist_head=NULL;
static chandev_activelist *chandev_activelist_head=NULL;
#if LINUX_VERSION_CODE>=KERNEL_VERSION(2,3,0)
int chandev_use_devno_names=FALSE;
#endif
static int chandev_cautious_auto_detect=TRUE;
static atomic_t chandev_conf_read=ATOMIC_INIT(FALSE);
static atomic_t chandev_initialised=ATOMIC_INIT(FALSE);


static unsigned long chandev_last_machine_check;


static struct tq_struct chandev_msck_task_tq;
static atomic_t chandev_msck_thread_lock;
static atomic_t chandev_new_msck;
static unsigned long chandev_last_startmsck_list_update;


typedef enum
{
	chandev_start,
	chandev_first_tag=chandev_start,
	chandev_msck,
	chandev_num_notify_tags
} chandev_userland_notify_tag;

static char *userland_notify_strs[]=
{
	"start",
	"machine_check"
};

typedef struct chandev_userland_notify_list chandev_userland_notify_list;
struct chandev_userland_notify_list
{
	chandev_userland_notify_list    *next;
	chandev_userland_notify_tag      tag;
	chandev_msck_status              prev_status;
	chandev_msck_status              curr_status;
	char                      devname[0];
};


static chandev_userland_notify_list *chandev_userland_notify_head=NULL;




static void chandev_read_conf_if_necessary(void);
static void chandev_read_conf(void);

#if LINUX_VERSION_CODE >=KERNEL_VERSION(2,3,0)
typedef struct net_device  net_device;
#else
typedef struct device  net_device;

static inline void init_waitqueue_head(wait_queue_head_t *q)
{
	*q=NULL;
}
#endif

#if LINUX_VERSION_CODE<KERNEL_VERSION(2,3,45)
static __inline__ void netif_stop_queue(net_device *dev)
{
	dev->tbusy=1;
}

static __inline__ void netif_start_queue(net_device *dev)
{
	dev->tbusy=0;
}
#endif



#define CHANDEV_INVALID_LOCK_OWNER            -1
static long                 chandev_lock_owner;
static int                  chandev_lock_cnt; 
static spinlock_t           chandev_spinlock;
#define CHANDEV_LOCK_DEBUG 0
#if CHANDEV_LOCK_DEBUG && !defined(CONFIG_ARCH_S390X)
#define CHANDEV_BACKTRACE_LOOPCNT 10
void                        *chandev_first_lock_addr[CHANDEV_BACKTRACE_LOOPCNT],
	                    *chandev_last_lock_addr[CHANDEV_BACKTRACE_LOOPCNT],
	                    *chandev_last_unlock_addr[CHANDEV_BACKTRACE_LOOPCNT];
#define CHANDEV_BACKTRACE(variable) \
memset((variable),0,sizeof(void *)*CHANDEV_BACKTRACE_LOOPCNT); \
(variable)[0]=__builtin_return_address(0); \
if(((long)variable[0])&0x80000000)         \
{                                          \
(variable)[1]=__builtin_return_address(1); \
if(((long)variable[1])&0x80000000)         \
{                                          \
(variable)[2]=__builtin_return_address(2); \
if(((long)variable[2])&0x80000000)         \
{                                          \
(variable)[3]=__builtin_return_address(3); \
if(((long)variable[3])&0x80000000)         \
{                                          \
(variable)[4]=__builtin_return_address(4); \
if(((long)variable[4])&0x80000000)         \
{                                          \
(variable)[5]=__builtin_return_address(5); \
if(((long)variable[5])&0x80000000)         \
{                                          \
(variable)[6]=__builtin_return_address(6); \
if(((long)variable[6])&0x80000000)         \
{                                          \
(variable)[7]=__builtin_return_address(7); \
if(((long)variable[7])&0x80000000)         \
{                                          \
(variable)[8]=__builtin_return_address(8); \
if(((long)variable[8])&0x80000000)         \
{                                          \
(variable)[9]=__builtin_return_address(9); \
} \
} \
} \
} \
} \
} \
} \
} \
}
#else
#define CHANDEV_BACKTRACE(variable)
#endif



typedef struct chandev_not_oper_struct chandev_not_oper_struct;

struct  chandev_not_oper_struct
{
	chandev_not_oper_struct *next;
	int irq;
	int status;
};


/* May as well try to keep machine checks in the order they happen so
 * we use qheader for chandev_not_oper_head instead of list.
 */
static qheader chandev_not_oper_head={NULL,NULL};
static spinlock_t           chandev_not_oper_spinlock;

#define chandev_interrupt_check() \
if(in_interrupt())                \
     printk(KERN_WARNING __FUNCTION__ " called under interrupt this shouldn't happen\n")


#define for_each(variable,head) \
for((variable)=(head);(variable)!=NULL;(variable)=(variable)->next)

#define for_each_allow_delete(variable,nextmember,head) \
for((variable)=(head),(nextmember)=((head) ? (head)->next:NULL); \
(variable)!=NULL; (variable)=(nextmember),(nextmember)=((nextmember) ? (nextmember->next) : NULL))

#define for_each_allow_delete2(variable,nextmember,head) \
for((variable)=(head);(variable)!=NULL;(variable)=(nextmember))


static void chandev_lock(void)
{
	eieio();
	chandev_interrupt_check();
	if(chandev_lock_owner!=(long)current)
	{
		while(!spin_trylock(&chandev_spinlock))
			schedule();
		chandev_lock_cnt=1;
		chandev_lock_owner=(long)current;
		CHANDEV_BACKTRACE(chandev_first_lock_addr)
	}
	else
	{
		chandev_lock_cnt++;
		CHANDEV_BACKTRACE(chandev_last_lock_addr)
	}
	if(chandev_lock_cnt<0||chandev_lock_cnt>100)
	{
		printk("odd lock_cnt %d lcs_chan_lock",chandev_lock_cnt);
		chandev_lock_cnt=1;
	}
}

static int chandev_full_unlock(void)
{
	int ret_lock_cnt=chandev_lock_cnt;
	chandev_lock_cnt=0;
	chandev_lock_owner=CHANDEV_INVALID_LOCK_OWNER;
	spin_unlock(&chandev_spinlock);
	return(ret_lock_cnt);
}

static void chandev_unlock(void)
{
	if(chandev_lock_owner!=(long)current)
		printk("chandev_unlock: current=%lx"
		      " chandev_lock_owner=%lx chandev_lock_cnt=%d\n",
		      (long)current,
		      chandev_lock_owner,
		      chandev_lock_cnt);
	CHANDEV_BACKTRACE(chandev_last_unlock_addr)
	if(--chandev_lock_cnt==0)
	{
		chandev_lock_owner=CHANDEV_INVALID_LOCK_OWNER;
		spin_unlock(&chandev_spinlock);
	}
	if(chandev_lock_cnt<0)
	{
		printk("odd lock_cnt=%d in chan_unlock",chandev_lock_cnt);
		chandev_full_unlock();
	}

}



void *chandev_alloc(size_t size)
{
	void *mem=kmalloc(size,GFP_ATOMIC);
	if(mem)
		memset(mem,0,size);
	return(mem);
}

static void chandev_add_to_list(list **listhead,void *member)
{
	chandev_lock();
	add_to_list(listhead,member);
	chandev_unlock();
}

static void chandev_queuemember(qheader *qhead,void *member)
{
	chandev_lock();
	enqueue_tail(qhead,(queue *)member);
	chandev_unlock();
}

static int chandev_remove_from_list(list **listhead,list *member)
{
	int retval;

	chandev_lock();
	retval=remove_from_list(listhead,member);
	chandev_unlock();
	return(retval);
}

static int chandev_remove_from_queue(qheader *qhead,queue *member)
{
	int retval;
	
	chandev_lock();
	retval=remove_from_queue(qhead,member);
	chandev_unlock();
	return(retval);
}



void chandev_free_listmember(list **listhead,list *member)
{
	chandev_lock();
	if(member)
	{
		if(chandev_remove_from_list(listhead,member))
			kfree(member);
		else
			printk(KERN_CRIT"chandev_free_listmember detected nonexistant"
			       "listmember listhead=%p member %p\n",listhead,member);
	}
	chandev_unlock();
}

void chandev_free_queuemember(qheader *qhead,queue *member)
{
	chandev_lock();
	if(member)
	{
		if(chandev_remove_from_queue(qhead,member))
			kfree(member);
		else
			printk(KERN_CRIT"chandev_free_queuemember detected nonexistant"
			       "queuemember qhead=%p member %p\n",qhead,member);
	}
	chandev_unlock();
}



void chandev_free_all_list(list **listhead)
{
	list *head;

	chandev_lock();
	while((head=remove_listhead(listhead)))
		kfree(head);
	chandev_unlock();
}

void chandev_free_all_queue(qheader *qhead)
{
	chandev_lock();
	while(qhead->head)
		chandev_free_queuemember(qhead,qhead->head);
	chandev_unlock();
}

static void chandev_wait_for_root_fs(void)
{
	wait_queue_head_t      wait;

	init_waitqueue_head(&wait);
	/* We need to wait till there is a root filesystem */
	while(init_task.fs->root==NULL)
	{
		sleep_on_timeout(&wait,HZ);
	}
}

/* We are now hotplug compliant i.e. */
/* we typically get called in /sbin/hotplug chandev our parameters */
static int chandev_exec_start_script(void *unused)
{
	
	char **argv,*tempname;
	int retval=-ENOMEM;
	int argc,loopcnt;
	size_t allocsize;
	chandev_userland_notify_list *member;
	wait_queue_head_t      wait;
	int                    have_tag[chandev_num_notify_tags]={FALSE,};
	chandev_userland_notify_tag tagidx;
	static char * envp[] = { "HOME=/", "TERM=linux", "PATH=/sbin:/usr/sbin:/bin:/usr/bin", NULL };
	
	init_waitqueue_head(&wait);
	strcpy(current->comm,"chandev_script");

	for(loopcnt=0;loopcnt<10&&(jiffies-chandev_last_startmsck_list_update)<HZ;loopcnt++)
	{
		sleep_on_timeout(&wait,HZ);
	}
	if(!chandev_userland_notify_head)
		return(0);
	chandev_lock();
	argc=2;
	for(tagidx=chandev_first_tag;tagidx<chandev_num_notify_tags;tagidx++)
	{
		for_each(member,chandev_userland_notify_head)
		{
			if(member->tag==tagidx)
			{
				switch(tagidx)
				{
				case chandev_start:
					argc++;
					break;
				case chandev_msck:
					argc+=3;
					break;
				default:
				}
				if(have_tag[tagidx]==FALSE)
					argc++;
				have_tag[tagidx]=TRUE;

			}
		}
	}
	allocsize=(argc+1)*sizeof(char *);
        /* Warning possible stack overflow */
	/* We can't kmalloc the parameters here as execve will */
	/* not return if successful */
	argv=alloca(allocsize);
	if(argv)
	{
		memset(argv,0,allocsize);
		argv[0]=hotplug_path;
		argv[1]="chandev";
		argc=2;
		for(tagidx=chandev_first_tag;tagidx<chandev_num_notify_tags;tagidx++)
		{
			if(have_tag[tagidx])
			{
				argv[argc++]=userland_notify_strs[tagidx];
				for_each(member,chandev_userland_notify_head)
				{
					if(member->tag==tagidx)
					{
						tempname=alloca(strlen(member->devname)+1);
						if(tempname)
						{
							strcpy(tempname,member->devname);
							argv[argc++]=tempname;
						}
						else
							goto Fail;
						if(member->tag==chandev_msck)
						{
							argv[argc++]=msck_status_strs[member->prev_status];
							argv[argc++]=msck_status_strs[member->curr_status];
						}
					}
				}
			}
		}
		chandev_free_all_list((list **)&chandev_userland_notify_head);
		chandev_unlock();
		chandev_wait_for_root_fs();
		/* We are basically execve'ing here there normally is no */
		/* return */
		retval=exec_usermodehelper(hotplug_path, argv, envp);
		goto Fail2;
	}
 Fail:
	
	chandev_unlock();
 Fail2:
	return(retval);
}


void *chandev_allocstr(const char *str,size_t offset)
{
	char *member;

	if((member=chandev_alloc(offset+strlen(str)+1)))
	{
		strcpy(&member[offset],str);
	}
	return((void *)member);
}


static int chandev_add_to_userland_notify_list(chandev_userland_notify_tag tag,
char *devname, chandev_msck_status prev_status,chandev_msck_status curr_status)
{
	chandev_userland_notify_list *member,*nextmember;
	int pid;
	
	chandev_lock();
	/* remove operations still outstanding for this device */
	for_each_allow_delete(member,nextmember,chandev_userland_notify_head)
		if(strcmp(member->devname,devname)==0)
			chandev_free_listmember((list **)&chandev_userland_notify_head,(list *)member);
	

	if((member=chandev_allocstr(devname,offsetof(chandev_userland_notify_list,devname))))
	{
		member->tag=tag;
		member->prev_status=prev_status;
		member->curr_status=curr_status;
		add_to_list((list **)&chandev_userland_notify_head,(list *)member);
		chandev_last_startmsck_list_update=jiffies;
		chandev_unlock();
		pid = kernel_thread(chandev_exec_start_script,NULL,SIGCHLD);
		if(pid<0)
		{
			printk("error making kernel thread for chandev_exec_start_script\n");
			return(pid);
		}
		else
			return(0);

	}
	else
	{
		chandev_unlock();
		printk("chandev_add_to_startmscklist memory allocation failed devname=%s\n",devname);
		return(-ENOMEM);
	}
}





int chandev_oper_func(int irq,devreg_t *dreg)
{
	chandev_last_machine_check=jiffies;
	if(atomic_dec_and_test(&chandev_msck_thread_lock))
	{
		schedule_task(&chandev_msck_task_tq);
	}
	atomic_set(&chandev_new_msck,TRUE);
	return(0);
}

static void chandev_not_oper_handler(int irq,int status )
{
	chandev_not_oper_struct *new_not_oper;

	chandev_last_machine_check=jiffies;
	if((new_not_oper=kmalloc(sizeof(chandev_not_oper_struct),GFP_ATOMIC)))
	{
		new_not_oper->irq=irq;
		new_not_oper->status=status;
		spin_lock(&chandev_not_oper_spinlock);
		enqueue_tail(&chandev_not_oper_head,(queue *)new_not_oper);
		spin_unlock(&chandev_not_oper_spinlock);
		if(atomic_dec_and_test(&chandev_msck_thread_lock))
		{
			schedule_task(&chandev_msck_task_tq);
		}
	}
	else
		printk("chandev_not_oper_handler failed to allocate memory & "
		       "lost a not operational interrupt %d %x",
		       irq,status);
}

chandev_irqinfo *chandev_get_irqinfo_by_irq(int irq)
{
	chandev_irqinfo *curr_irqinfo;
	for_each(curr_irqinfo,chandev_irqinfo_head)
		if(irq==curr_irqinfo->sch.irq)
			return(curr_irqinfo);
	return(NULL);
}

chandev *chandev_get_by_irq(int irq)
{
	chandev *curr_chandev;

	for_each(curr_chandev,(chandev *)chandev_head.head)
		if(curr_chandev->sch.irq==irq)
		{
			return(curr_chandev);
		}
	return(NULL);
}

chandev_activelist *chandev_get_activelist_by_irq(int irq)
{
	chandev_activelist *curr_device;

	for_each(curr_device,chandev_activelist_head)
	{
			if(curr_device->read_irqinfo->sch.irq==irq||
			   curr_device->write_irqinfo->sch.irq==irq||
			   (curr_device->data_irqinfo&&curr_device->data_irqinfo->sch.irq==irq))
				return(curr_device);
	}
	return(NULL);
}


void chandev_remove_irqinfo_by_irq(unsigned int irq)
{
	chandev_irqinfo *remove_irqinfo;
	chandev_activelist *curr_device;

	chandev_lock();
	/* remove any orphan irqinfo left lying around. */
        if((remove_irqinfo=chandev_get_irqinfo_by_irq(irq)))
	{
		for_each(curr_device,chandev_activelist_head)
		{
			if(curr_device->read_irqinfo==remove_irqinfo)
			{
				curr_device->read_irqinfo=NULL;
				break;
			}
			if(curr_device->write_irqinfo==remove_irqinfo)
			{
				curr_device->write_irqinfo=NULL;
				break;
			}
			if(curr_device->data_irqinfo&&curr_device->data_irqinfo==remove_irqinfo)
			{
				curr_device->data_irqinfo=NULL;
				break;
			}
		}
		chandev_free_listmember((list **)&chandev_irqinfo_head,
					 (list *)remove_irqinfo);
	}
	chandev_unlock();
	
}

int chandev_add_schib_info(int irq,chandev_subchannel_info *sch)
{
	schib_t *new_schib;
	
	if((new_schib=s390_get_schib(irq)))
	{
		sch->pim=new_schib->pmcw.pim;
		memcpy(&sch->chpid,&new_schib->pmcw.chpid,sizeof(sch->chpid));
		return(0);
	}
	return(-ENODEV);
}

int chandev_request_irq(unsigned int   irq,
                      void           (*handler)(int, void *, struct pt_regs *),
                      unsigned long  irqflags,
                      const char    *devname,
                      void          *dev_id)
{
	chandev_irqinfo *new_irqinfo;
	chandev_activelist *curr_device;
	s390_dev_info_t         devinfo;
	int          retval;
	

	chandev_lock();
	if((curr_device=chandev_get_activelist_by_irq(irq)))
	{
		printk("chandev_request_irq failed devname=%s irq=%d "
		       "it already belongs to %s shutdown this device first.\n",
		       devname,irq,curr_device->devname);
		chandev_unlock();
		return(-EPERM);
	}
	/* remove any orphan irqinfo left lying around. */
	chandev_remove_irqinfo_by_irq(irq);
	chandev_unlock();
	if((new_irqinfo=chandev_allocstr(devname,offsetof(chandev_irqinfo,devname))))
	{
		
		if((retval=get_dev_info_by_irq(irq,&devinfo))||
		   (retval=s390_request_irq_special(irq,handler,
						chandev_not_oper_handler,
						irqflags,devname,dev_id)))
			kfree(new_irqinfo);
		else
		{
			new_irqinfo->msck_status=chandev_status_good;
			new_irqinfo->sch.devno=devinfo.devno;
			new_irqinfo->sch.irq=irq;
			new_irqinfo->sch.cu_type=devinfo.sid_data.cu_type; /* control unit type */
			new_irqinfo->sch.cu_model=devinfo.sid_data.cu_model; /* control unit model */
			new_irqinfo->sch.dev_type=devinfo.sid_data.dev_type; /* device type */
			new_irqinfo->sch.dev_model=devinfo.sid_data.dev_model; /* device model */
			chandev_add_schib_info(irq,&new_irqinfo->sch);
			new_irqinfo->handler=handler;
			new_irqinfo->dev_id=dev_id;
			chandev_add_to_list((list **)&chandev_irqinfo_head,new_irqinfo);
		}
	}
	else
	{
		printk("chandev_request_irq memory allocation failed devname=%s irq=%d\n",devname,irq);
		retval=-ENOMEM;
	}
	return(retval);
}

/* This should be safe to call even multiple times. */
void chandev_free_irq(unsigned int irq, void *dev_id)
{
	s390_dev_info_t devinfo;
	int err;
	
	/* remove any orphan irqinfo left lying around. */
	chandev_remove_irqinfo_by_irq(irq);
	if((err=get_dev_info_by_irq(irq,&devinfo)))
	{
		printk("chandev_free_irq get_dev_info_by_irq reported err=%X on irq %d\n"
		       "should not happen\n",err,irq);
		return;
	 }
	if(devinfo.status&DEVSTAT_DEVICE_OWNED)
	   free_irq(irq,dev_id);
}

/* This should be safe even if chandev_free_irq is already called by the device */
void chandev_free_irq_by_irqinfo(chandev_irqinfo *irqinfo)
{
	if(irqinfo)
		chandev_free_irq(irqinfo->sch.irq,irqinfo->dev_id);
}



void chandev_sprint_type_model(char *buff,s32 type,s16 model)
{
	if(type==-1)
		strcpy(buff,"    *    ");
	else
		sprintf(buff," 0x%04x  ",(int)type);
	buff+=strlen(buff);
	if(model==-1)
		strcpy(buff,"    *   ");
	else
		sprintf(buff," 0x%02x  ",(int)model);
}

void chandev_sprint_devinfo(char *buff,s32 cu_type,s16 cu_model,s32 dev_type,s16 dev_model)
{
	chandev_sprint_type_model(buff,cu_type,cu_model);
	chandev_sprint_type_model(&buff[strlen(buff)],dev_type,dev_model);
}

void chandev_remove_parms(chandev_type chan_type,int exact_match,int lo_devno)
{
	chandev_parms      *curr_parms,*next_parms;

	chandev_lock();
	for_each_allow_delete(curr_parms,next_parms,chandev_parms_head)
	{
		if(((chan_type&(curr_parms->chan_type)&&!exact_match)||
		   (chan_type==(curr_parms->chan_type)&&exact_match))&&
		   (lo_devno==-1||lo_devno==curr_parms->lo_devno))
			chandev_free_listmember((list **)&chandev_parms_head,(list *)curr_parms);
	}
	chandev_unlock();
}


void chandev_add_parms(chandev_type chan_type,u16 lo_devno,u16 hi_devno,char *parmstr)
{
	chandev_parms      *parms;

	if(lo_devno>hi_devno)
	{
		printk("chandev_add_parms detected bad device range lo_devno=0x%04x  hi_devno=0x%04x\n,",
		       (int)lo_devno,(int)hi_devno);
		return;
	}
	if((parms=chandev_allocstr(parmstr,offsetof(chandev_parms,parmstr))))
	{
		parms->chan_type=chan_type;
		parms->lo_devno=lo_devno;
		parms->hi_devno=hi_devno;
		chandev_add_to_list((list **)&chandev_parms_head,(void *)parms);
	}
	else
		printk("chandev_add_parms memory request failed\n");
}


void chandev_add_model(chandev_type chan_type,s32 cu_type,s16 cu_model,
		       s32 dev_type,s16 dev_model,u8 max_port_no,int auto_msck_recovery,
		       u8 default_checksum_received_ip_pkts,u8 default_use_hw_stats)
{
	chandev_model_info *newmodel;
	int                err;
	char buff[40];

	if((newmodel=chandev_alloc(sizeof(chandev_model_info))))
	{
		devreg_t *drinfo=&newmodel->drinfo;
		newmodel->chan_type=chan_type;
		newmodel->cu_type=cu_type;
		newmodel->cu_model=cu_model;
		newmodel->dev_type=dev_type;
		newmodel->dev_model=dev_model;
		newmodel->max_port_no=max_port_no;
		newmodel->auto_msck_recovery=auto_msck_recovery;
		newmodel->default_checksum_received_ip_pkts=default_checksum_received_ip_pkts;
		newmodel->default_use_hw_stats=default_use_hw_stats; /* where available e.g. lcs */
		if(cu_type==-1&&dev_type==-1)
		{
			chandev_sprint_devinfo(buff,newmodel->cu_type,newmodel->cu_model,
					       newmodel->dev_type,newmodel->dev_model);
			printk(KERN_INFO"can't call s390_device_register for this device chan_type/chan_model/dev_type/dev_model %s\n",buff);
			kfree(newmodel);
			return;
		}
		drinfo->flag=DEVREG_TYPE_DEVCHARS;
		if(cu_type!=-1)
			drinfo->flag|=DEVREG_MATCH_CU_TYPE;
		if(cu_model!=-1)
			drinfo->flag|=DEVREG_MATCH_CU_MODEL;
		if(dev_type!=-1)
			drinfo->flag|=DEVREG_MATCH_DEV_TYPE;
		if(dev_model!=-1)
			drinfo->flag|=DEVREG_MATCH_DEV_MODEL;
		drinfo->ci.hc.ctype=cu_type;
		drinfo->ci.hc.cmode=cu_model;
		drinfo->ci.hc.dtype=dev_type;
		drinfo->ci.hc.dmode=dev_model;
		drinfo->oper_func=chandev_oper_func;
		if((err=s390_device_register(&newmodel->drinfo)))
		{
			chandev_sprint_devinfo(buff,newmodel->cu_type,newmodel->cu_model,
					       newmodel->dev_type,newmodel->dev_model);
			printk("s390_device_register failed in chandev_add_model"
			       " this is nothing to worry about chan_type/chan_model/dev_type/dev_model %s\n",buff);
			drinfo->oper_func=NULL;
		}
		chandev_add_to_list((list **)&chandev_models_head,newmodel);
	}
}


void chandev_remove(chandev *member)
{
	chandev_free_queuemember(&chandev_head,(queue *)member);
}


void chandev_remove_all(void)
{
	chandev_free_all_queue(&chandev_head);
}

void chandev_remove_model(chandev_model_info *model)
{
	chandev *curr_chandev,*next_chandev;

	chandev_lock();
	for_each_allow_delete(curr_chandev,next_chandev,(chandev *)chandev_head.head)
		if(curr_chandev->model_info==model)
			chandev_remove(curr_chandev);
	if(model->drinfo.oper_func)
		s390_device_unregister(&model->drinfo);
	chandev_free_listmember((list **)&chandev_models_head,(list *)model);
	chandev_unlock();
}

void chandev_remove_all_models(void)
{
	chandev_lock();
	while(chandev_models_head)
		chandev_remove_model(chandev_models_head);
	chandev_unlock();
}

void chandev_del_model(s32 cu_type,s16 cu_model,s32 dev_type,s16 dev_model)
{
	chandev_model_info *curr_model,*next_model;
	
	chandev_lock();
	for_each_allow_delete(curr_model,next_model,chandev_models_head)
		if((curr_model->cu_type==cu_type||cu_type==-1)&&
		   (curr_model->cu_model==cu_model||cu_model==-1)&&
		   (curr_model->dev_type==dev_type||dev_type==-1)&&
		   (curr_model->dev_model==dev_model||dev_model==-1))
			chandev_remove_model(curr_model);			
	chandev_unlock();
}

static void chandev_init_default_models(void)
{
	/* Usually P390/Planter 3172 emulation assume maximum 16 to be safe. */
	chandev_add_model(chandev_type_lcs,0x3088,0x1,-1,-1,15,default_msck_bits,FALSE,FALSE);	

	/* 3172/2216 Paralell the 2216 allows 16 ports per card the */
	/* the original 3172 only allows 4 we will assume the max of 16 */
	chandev_add_model(chandev_type_lcs|chandev_type_ctc,0x3088,0x8,-1,-1,15,default_msck_bits,FALSE,FALSE);

	/* 3172/2216 Escon serial the 2216 allows 16 ports per card the */
	/* the original 3172 only allows 4 we will assume the max of 16 */
	chandev_add_model(chandev_type_lcs|chandev_type_escon,0x3088,0x1F,-1,-1,15,default_msck_bits,FALSE,FALSE);

	/* Only 2 ports allowed on OSA2 cards model 0x60 */
	chandev_add_model(chandev_type_lcs,0x3088,0x60,-1,-1,1,default_msck_bits,FALSE,FALSE);
	/* qeth gigabit ethernet */
	chandev_add_model(chandev_type_qeth,0x1731,0x1,0x1732,0x1,0,default_msck_bits,FALSE,FALSE);
	chandev_add_model(chandev_type_qeth,0x1731,0x5,0x1732,0x5,0,default_msck_bits,FALSE,FALSE);
	/* Osa-D we currently aren't too emotionally involved with this */
	chandev_add_model(chandev_type_osad,0x3088,0x62,-1,-1,0,default_msck_bits,FALSE,FALSE);
	/* claw */
	chandev_add_model(chandev_type_claw,0x3088,0x61,-1,-1,0,default_msck_bits,FALSE,FALSE);

	/* ficon attached ctc */
	chandev_add_model(chandev_type_escon,0x3088,0x1E,-1,-1,0,default_msck_bits,FALSE,FALSE);
}


void chandev_del_noauto(u16 devno)
{
	chandev_noauto_range *curr_noauto,*next_noauto;
	chandev_lock();
	for_each_allow_delete(curr_noauto,next_noauto,chandev_noauto_head)
		if(curr_noauto->lo_devno<=devno&&curr_noauto->hi_devno>=devno)
			chandev_free_listmember((list **)&chandev_noauto_head,(list *)curr_noauto); 
	chandev_unlock();
}

void chandev_del_msck(u16 devno)
{
	chandev_msck_range *curr_msck_range,*next_msck_range;
	chandev_lock();
	for_each_allow_delete(curr_msck_range,next_msck_range,chandev_msck_range_head)
		if(curr_msck_range->lo_devno<=devno&&curr_msck_range->hi_devno>=devno)
			chandev_free_listmember((list **)&chandev_msck_range_head,(list *)curr_msck_range); 
	chandev_unlock();
}


void chandev_add(s390_dev_info_t  *newdevinfo,chandev_model_info *newmodelinfo)
{
	chandev *new_chandev=NULL;

	if((new_chandev=chandev_alloc(sizeof(chandev))))
	{
		new_chandev->model_info=newmodelinfo;
		new_chandev->sch.devno=newdevinfo->devno;
		new_chandev->sch.irq=newdevinfo->irq;
		new_chandev->sch.cu_type=newdevinfo->sid_data.cu_type; /* control unit type */
		new_chandev->sch.cu_model=newdevinfo->sid_data.cu_model; /* control unit model */
		new_chandev->sch.dev_type=newdevinfo->sid_data.dev_type; /* device type */
		new_chandev->sch.dev_model=newdevinfo->sid_data.dev_model; /* device model */
		chandev_add_schib_info(newdevinfo->irq,&new_chandev->sch);
		new_chandev->owned=(newdevinfo->status&DEVSTAT_DEVICE_OWNED ? TRUE:FALSE);
		chandev_queuemember(&chandev_head,new_chandev);
	}
}

void chandev_unregister_probe(chandev_probefunc probefunc)
{
	chandev_probelist *curr_probe,*next_probe;

	chandev_lock();
	for_each_allow_delete(curr_probe,next_probe,chandev_probelist_head)
		if(curr_probe->probefunc==probefunc)
			chandev_free_listmember((list **)&chandev_probelist_head,
						(list *)curr_probe);
	chandev_unlock();
}

void chandev_unregister_probe_by_chan_type(chandev_type chan_type)
{
	chandev_probelist *curr_probe,*next_probe;

	chandev_lock();
	for_each_allow_delete(curr_probe,next_probe,chandev_probelist_head)
		if(curr_probe->chan_type==chan_type)
			chandev_free_listmember((list **)&chandev_probelist_head,
						(list *)curr_probe);
	chandev_unlock();
}



void chandev_reset(void)
{
	chandev_lock();
	chandev_remove_all_models();
	chandev_free_all_list((list **)&chandev_noauto_head);
	chandev_free_all_list((list **)&chandev_msck_range_head);
	chandev_free_all_list((list **)&chandev_force_head);
	chandev_remove_parms(-1,FALSE,-1);
#if LINUX_VERSION_CODE>=KERNEL_VERSION(2,3,0)
	chandev_use_devno_names=FALSE;
#endif
	chandev_persistent=0;
	chandev_unlock();
}


int chandev_is_chandev(int irq,s390_dev_info_t *devinfo,chandev_force **forceinfo,chandev_model_info **ret_model)
{
	chandev_force *curr_force;
	chandev_model_info *curr_model=NULL;
	int err;
	int retval=FALSE;

	if(forceinfo)
		*forceinfo=NULL;
	if(ret_model)
		*ret_model=NULL;
	if((err=get_dev_info_by_irq(irq,devinfo)))
	{
		printk("chandev_is_chandev get_dev_info_by_irq reported err=%X on irq %d\n"
		       "should not happen\n",err,irq);
			return FALSE;
	}
	chandev_lock();
	
	for_each(curr_model,chandev_models_head)
	{
		if(((curr_model->cu_type==devinfo->sid_data.cu_type)||(curr_model->cu_type==-1))&&
		   ((curr_model->cu_model==devinfo->sid_data.cu_model)||(curr_model->cu_model==-1))&&
		   ((curr_model->dev_type==devinfo->sid_data.dev_type)||(curr_model->dev_type==-1))&&
		   ((curr_model->dev_model==devinfo->sid_data.dev_model)||(curr_model->dev_model==-1)))
		{
			retval=TRUE;
			if(ret_model)
				*ret_model=curr_model;
			break;
		}
	}
	for_each(curr_force,chandev_force_head)
	{
		if(((curr_force->read_lo_devno==devinfo->devno)&&
		   (curr_force->write_hi_devno==devinfo->devno)&&
		    (curr_force->devif_num!=-2))||
		   ((curr_force->read_lo_devno>=devinfo->devno)&&
		    (curr_force->write_hi_devno<=devinfo->devno)&&
		    (curr_force->devif_num==-2)))
		{
			if(forceinfo)
				*forceinfo=curr_force;
			break;
		}
	}
	chandev_unlock();
	return(retval);
}

void chandev_collect_devices(void)
{
	int curr_irq,loopcnt=0;
	s390_dev_info_t   curr_devinfo;
	chandev_model_info *curr_model;
     

	for(curr_irq=get_irq_first();curr_irq>=0; curr_irq=get_irq_next(curr_irq))
	{
		/* check read chandev
		 * we had to do the cu_model check also because ctc devices
		 * have the same cutype & after asking some people
		 * the model numbers are given out pseudo randomly so
		 * we can't just take a range of them also the dev_type & models are 0
		 */
		loopcnt++;
		if(loopcnt>0x10000)
		{
			printk(KERN_ERR"chandev_collect_devices detected infinite loop bug in get_irq_next\n");
			break;
		}
		chandev_lock();
		if(chandev_is_chandev(curr_irq,&curr_devinfo,NULL,&curr_model))
			chandev_add(&curr_devinfo,curr_model);
		chandev_unlock();
	}
}

int chandev_add_force(chandev_type chan_type,s32 devif_num,u16 read_lo_devno,
u16 write_hi_devno,u16 data_devno,s32 memory_usage_in_k,s16 port_protocol_no,u8 checksum_received_ip_pkts,
u8 use_hw_stats,char *host_name,char *adapter_name,char *api_type)
{
	chandev_force *new_chandev_force;
	
	if(devif_num==-2&&read_lo_devno>write_hi_devno)
	{
		printk("chandev_add_force detected bad device range lo_devno=0x%04x  hi_devno=0x%04x\n,",
		       (int)read_lo_devno,(int)write_hi_devno);
		return(-1);
	}
	if(memory_usage_in_k<0)
	{
		printk("chandev_add_force memory_usage_in_k is bad\n");
		return(-1);
	}
	if(chan_type==chandev_type_claw)
	{
		int host_name_len=strlen(host_name),
			adapter_name_len=strlen(adapter_name),
			api_type_len=strlen(api_type);
		if(host_name_len>=CLAW_NAMELEN||host_name_len==0||
		   adapter_name_len>=CLAW_NAMELEN||adapter_name_len==0||
		   api_type_len>=CLAW_NAMELEN||api_type_len==0)
			return(-1);
	}
	if((new_chandev_force=chandev_alloc(sizeof(chandev_force))))
	{
		new_chandev_force->chan_type=chan_type;
		new_chandev_force->devif_num=devif_num;
		new_chandev_force->read_lo_devno=read_lo_devno;
		new_chandev_force->write_hi_devno=write_hi_devno;
		new_chandev_force->data_devno=data_devno;
		new_chandev_force->memory_usage_in_k=memory_usage_in_k;
		new_chandev_force->port_protocol_no=port_protocol_no;
		new_chandev_force->checksum_received_ip_pkts=checksum_received_ip_pkts;
		new_chandev_force->use_hw_stats=use_hw_stats;
		
		if(chan_type==chandev_type_claw)
		{
			strcpy(new_chandev_force->claw.host_name,host_name);
			strcpy(new_chandev_force->claw.adapter_name,adapter_name);
			strcpy(new_chandev_force->claw.api_type,api_type);
		}
		chandev_add_to_list((list **)&chandev_force_head,new_chandev_force);
	}
	return(0);
}

void chandev_del_force(int read_lo_devno)
{
	chandev_force *curr_force,*next_force;
	
	chandev_lock();
	for_each_allow_delete(curr_force,next_force,chandev_force_head)
	{
		if(curr_force->read_lo_devno==read_lo_devno||read_lo_devno==-1)
			chandev_free_listmember((list **)&chandev_force_head,
						(list *)curr_force);
	}
	chandev_unlock();
}


void chandev_shutdown(chandev_activelist *curr_device)
{
	int err=0;
	chandev_lock();


	/* unregister_netdev calls the dev->close so we shouldn't do this */
	/* this otherwise we crash */
	if(curr_device->unreg_dev)
	{
		curr_device->unreg_dev(curr_device->dev_ptr);
		curr_device->unreg_dev=NULL;
	}
	if(curr_device->shutdownfunc)
	{
		err=curr_device->shutdownfunc(curr_device->dev_ptr);
	}
	if(err)
		printk("chandev_shutdown unable to fully shutdown & unload %s err=%d\n"
		       "probably some upper layer still requires the device to exist\n",
		       curr_device->devname,err);
	else
	{
		
		chandev_free_irq_by_irqinfo(curr_device->read_irqinfo);
		chandev_free_irq_by_irqinfo(curr_device->write_irqinfo);
		if(curr_device->data_irqinfo)
			chandev_free_irq_by_irqinfo(curr_device->data_irqinfo);
		chandev_free_listmember((list **)&chandev_activelist_head,
				(list *)curr_device);
	}
	chandev_unlock();
}

void chandev_shutdown_all(void)
{
	while(chandev_activelist_head)
		chandev_shutdown(chandev_activelist_head);
}
void chandev_shutdown_by_name(char *devname)
{
	chandev_activelist *curr_device;

	chandev_lock();
	for_each(curr_device,chandev_activelist_head)
		if(strcmp(devname,curr_device->devname)==0)
		{
			chandev_shutdown(curr_device);
			break;
		}
	chandev_unlock();
}

static chandev_activelist *chandev_active(u16 devno)
{
	chandev_activelist *curr_device;

	for_each(curr_device,chandev_activelist_head)
		if(curr_device->read_irqinfo->sch.devno==devno||
		   curr_device->write_irqinfo->sch.devno==devno||
		   (curr_device->data_irqinfo&&curr_device->data_irqinfo->sch.devno==devno))
		{
			return(curr_device);
		}
	return(NULL);
}

void chandev_shutdown_by_devno(u16 devno)
{
	chandev_activelist *curr_device;

	chandev_lock();
	curr_device=chandev_active(devno);
	if(curr_device)
		chandev_shutdown(curr_device);
	chandev_unlock();
}


int chandev_pack_args(char *str)
{
	char *newstr=str,*next;
	int strcnt=1;

	while(*str)
	{
		next=str+1;
		/*remove dead spaces */
		if(isspace(*str)&&isspace(*next))
		{
			str++;
			continue;
		}
		if(isspace(*str))
		{
			*str=',';
			goto pack_dn;
		}
		if(((*str)==';')&&(*next))
		{
			strcnt++;
			*str=0;
		}
	pack_dn:
		*newstr++=*str++;
		
	}
	*newstr=0;
	return(strcnt);
}

typedef enum
{ 
	isnull=0,
	isstr=1,
	isnum=2,
	iscomma=4,
} chandev_strval;

chandev_strval chandev_strcmp(char *teststr,char **str,long *endlong)
{
	char *cur;
	chandev_strval  retval=isnull;

	int len=strlen(teststr);
	if(strncmp(teststr,*str,len)==0)
	{
		*str+=len;
		retval=isstr;
		cur=*str;
		*endlong=simple_strtol(cur,str,0);
		if(cur!=*str)
			retval|=isnum;
		if(**str==',')
		{
			retval|=iscomma;
			*str+=1;
		}
		else if(**str!=0)
			retval=isnull;
	}
	return(retval);
}


int chandev_initdevice(chandev_probeinfo *probeinfo,void *dev_ptr,u8 port_no,char *devname,chandev_category category,chandev_unregfunc unreg_dev)
{
	chandev_activelist *newdevice,*curr_device;

	chandev_interrupt_check();
	if(probeinfo->newdevice!=NULL)
	{
		printk("probeinfo->newdevice!=NULL in chandev_initdevice for %s",devname);
		return(-EPERM);
	}


	chandev_lock();
	for_each(curr_device,chandev_activelist_head)
	{
		if(strcmp(curr_device->devname,devname)==0)
		{
			printk("chandev_initdevice detected duplicate devicename %s\n",devname);
			chandev_unlock();
			return(-EPERM);
		}
	}
	if((newdevice=chandev_allocstr(devname,offsetof(chandev_activelist,devname))))
	{
		newdevice->read_irqinfo=chandev_get_irqinfo_by_irq(probeinfo->read.irq);
		newdevice->write_irqinfo=chandev_get_irqinfo_by_irq(probeinfo->write.irq);
		if(probeinfo->data_exists)
			newdevice->data_irqinfo=chandev_get_irqinfo_by_irq(probeinfo->data.irq);
		chandev_unlock();
		if(newdevice->read_irqinfo==NULL||newdevice->write_irqinfo==NULL||
		   (probeinfo->data_exists&&newdevice->data_irqinfo==NULL))
		{
			printk("chandev_initdevice, it appears that chandev_request_irq was not "
			       "called for devname=%s read_irq=%d write_irq=%d data_irq=%d\n",
			       devname,probeinfo->read.irq,probeinfo->write.irq,probeinfo->data.irq);
			kfree(newdevice);
			return(-EPERM);
		}
		newdevice->chan_type=probeinfo->chan_type;		
		newdevice->dev_ptr=dev_ptr;
		newdevice->port_no=port_no;
		newdevice->memory_usage_in_k=probeinfo->memory_usage_in_k;
		newdevice->category=category;
		newdevice->unreg_dev=unreg_dev;
		probeinfo->newdevice=newdevice;
		return(0);
	}
	chandev_unlock();
	return(-ENOMEM);
}


char *chandev_build_device_name(chandev_probeinfo *probeinfo,char *destnamebuff,char *basename,int buildfullname)
{
	if (chandev_use_devno_names&&(!probeinfo->device_forced||probeinfo->devif_num==-1)) 
		sprintf(destnamebuff,"%s%04x",basename,(int)probeinfo->read.devno);
	else
	{
		if(probeinfo->devif_num==-1)
		{
			if(buildfullname)
			{
				int idx,len=strlen(basename);
				
				chandev_activelist *curr_device;
				for(idx=0;idx<0xffff;idx++)
				{
					for_each(curr_device,chandev_activelist_head)
					{
						if(strncmp(curr_device->devname,basename,len)==0)
						{
							char numbuff[10];
							sprintf(numbuff,"%d",idx);
							if(strcmp(&curr_device->devname[len],numbuff)==0)
								goto next_idx;
						}
					}
					sprintf(destnamebuff,"%s%d",basename,idx);
					return(destnamebuff);
				next_idx:
				}
				printk("chandev_build_device_name was usable to build a unique name for %s\n",basename);
				return(NULL);
			}
			else
				sprintf(destnamebuff,"%s%%d",basename);
		}
		else
		{
			sprintf(destnamebuff,"%s%d",basename,(int)probeinfo->devif_num);
		}
	}
	return(destnamebuff);
}

#if LINUX_VERSION_CODE>=KERNEL_VERSION(2,3,0)
struct net_device *chandev_init_netdev(chandev_probeinfo *probeinfo,char *basename,
struct net_device *dev, int sizeof_priv,
struct net_device *(*init_netdevfunc)(struct net_device *dev, int sizeof_priv))
#else
struct device *chandev_init_netdev(chandev_probeinfo *probeinfo,char *basename,
struct device *dev, int sizeof_priv,
struct device *(*init_netdevfunc)(struct device *dev, int sizeof_priv))
#endif
{
#if LINUX_VERSION_CODE>=KERNEL_VERSION(2,3,0)
	struct net_device *retdevice=NULL;
	int new_device = FALSE;
#else
	struct device *retdevice=NULL;
#endif
	

	chandev_interrupt_check();
	if (!init_netdevfunc) 
	{
		printk("init_netdevfunc=NULL in chandev_init_netdev, it should not be valid.\n");
		return NULL;
	}
#if LINUX_VERSION_CODE>=KERNEL_VERSION(2,3,0)
	/* Allocate a device if one is not provided. */
        if (dev == NULL) 
	{
		/* ensure 32-byte alignment of the private area */
		int alloc_size = sizeof (*dev) + sizeof_priv + 31;

		dev = (struct net_device *) kmalloc (alloc_size, GFP_KERNEL);
		if (dev == NULL) 
		{
			printk(KERN_ERR "chandev_initnetdevice: Unable to allocate device memory.\n");
			return NULL;
		}

		memset(dev, 0, alloc_size);

		if (sizeof_priv)
			dev->priv = (void *) (((long)(dev + 1) + 31) & ~31);
		new_device=TRUE;
	}
	chandev_build_device_name(probeinfo,dev->name,basename,FALSE);
#endif
	retdevice=init_netdevfunc(dev,sizeof_priv);
#if LINUX_VERSION_CODE>=KERNEL_VERSION(2,3,0)
	/* Register device if necessary */
	/* we need to do this as init_netdev doesn't call register_netdevice */
	/* for already allocated devices */
	if (retdevice && new_device)
		register_netdev(retdevice);
#endif
#if LINUX_VERSION_CODE>=KERNEL_VERSION(2,3,0)
	/* We allocated it, so we should free it on error */
	if (!retdevice && new_device) 
		kfree(dev);
#endif
	return retdevice;
}




#if LINUX_VERSION_CODE>=KERNEL_VERSION(2,3,0)
struct net_device *chandev_initnetdevice(chandev_probeinfo *probeinfo,u8 port_no,
struct net_device *dev, int sizeof_priv, char *basename, 
struct net_device *(*init_netdevfunc)(struct net_device *dev, int sizeof_priv),
void (*unreg_netdevfunc)(struct net_device *dev))
#else
struct device *chandev_initnetdevice(chandev_probeinfo *probeinfo,u8 port_no,
struct device *dev, int sizeof_priv, char *basename,
struct device *(*init_netdevfunc)(struct device *dev, int sizeof_priv),
void (*unreg_netdevfunc)(struct device *dev))
#endif
{
#if LINUX_VERSION_CODE>=KERNEL_VERSION(2,3,0)
	struct net_device *retdevice=NULL;
	int new_device=(dev==NULL);
#else
	struct device *retdevice=NULL;
#endif

	if (!unreg_netdevfunc) 
	{
		printk("unreg_netdevfunc=NULL in chandev_initnetdevice, it should not be valid.\n");
		return NULL;
	}
	chandev_interrupt_check();
	retdevice=chandev_init_netdev(probeinfo,basename,dev,sizeof_priv,init_netdevfunc);
	if (retdevice) 
	{
		if (chandev_initdevice(probeinfo,retdevice,port_no,retdevice->name,
				      chandev_category_network_device,(chandev_unregfunc)unreg_netdevfunc)) 
		{
			unreg_netdevfunc(retdevice);
#if LINUX_VERSION_CODE>=KERNEL_VERSION(2,3,0)
			/* We allocated it, so we should free it on error */
			if(new_device)
				kfree(dev);
#endif

			retdevice = NULL;
		}
	}
	return retdevice;
}


int chandev_compare_chpid_info(chandev_subchannel_info *chan1,chandev_subchannel_info *chan2)
{
	return (chan1->pim!=chan2->pim || *chan1->chpid!=*chan2->chpid);
}

int chandev_compare_cu_dev_info(chandev_subchannel_info *chan1,chandev_subchannel_info *chan2)
{
	return ((chan1->cu_type != chan2->cu_type)||
		(chan1->cu_model != chan2->cu_model)||
		(chan1->dev_type != chan2->dev_type)||
		(chan1->dev_model != chan2->dev_model));
}

int chandev_compare_subchannel_info(chandev_subchannel_info *chan1,chandev_subchannel_info *chan2)
{
	return((chan1->devno == chan2->devno) &&
	       (chan1->cu_type == chan2->cu_type) &&
	       (chan1->cu_model == chan2->cu_model) &&
	       (chan1->dev_type == chan2->dev_type) &&
	       (chan1->dev_model == chan2->dev_model) &&
	       (chan1->pim == chan2->pim) &&
	       (*chan1->chpid == *chan2->chpid));
}


int chandev_doprobe(chandev_force *force,chandev *read,
chandev *write,chandev *data)
{
	chandev_probelist *probe;
	chandev_model_info *model_info;
	chandev_probeinfo probeinfo;
	int               rc=-1,hint=-1;
	chandev_activelist *newdevice;
	chandev_probefunc  probefunc;
	chandev_parms      *curr_parms;
	chandev_model_info dummy_model_info;

	memset(&probeinfo,0,sizeof(probeinfo));
	memset(&dummy_model_info,0,sizeof(dummy_model_info));
	probeinfo.device_forced=(force!=NULL);
	probeinfo.chpid_info_inconsistent=chandev_compare_chpid_info(&read->sch,&write->sch)||
		 (data&&chandev_compare_chpid_info(&read->sch,&data->sch));
	probeinfo.cu_dev_info_inconsistent=chandev_compare_cu_dev_info(&read->sch,&write->sch)||
		 (data&&chandev_compare_cu_dev_info(&read->sch,&data->sch));
	if(read->model_info)
		model_info=read->model_info;
	else
	{
		dummy_model_info.chan_type=chandev_type_none;
		dummy_model_info.max_port_no=16;
		model_info=&dummy_model_info;
	}
	for_each(probe,chandev_probelist_head)
	{
		if(force)
			probeinfo.chan_type = ( probe->chan_type & force->chan_type );
		else
		{
			if(chandev_cautious_auto_detect)
				probeinfo.chan_type = ( probe->chan_type == model_info->chan_type ? 
						       probe->chan_type : chandev_type_none );
			else
				probeinfo.chan_type = ( probe->chan_type & model_info->chan_type );
		}
		if(probeinfo.chan_type && (force || ( !probeinfo.cu_dev_info_inconsistent &&
		  ((probe->chan_type&(chandev_type_ctc|chandev_type_escon)) ||
		   !probeinfo.chpid_info_inconsistent))))
		{
#if LINUX_VERSION_CODE>=KERNEL_VERSION(2,3,0)
			if(chandev_use_devno_names)
				probeinfo.devif_num=read->sch.devno;
			else
#endif
				probeinfo.devif_num=-1;
			probeinfo.read=read->sch;
			probeinfo.write=write->sch;
			if(data)
			{
				probeinfo.data=data->sch;
				probeinfo.data_exists=TRUE;
			}
			probeinfo.max_port_no=(force&&(force->port_protocol_no!=-1) ? 
			      force->port_protocol_no : model_info->max_port_no);
			for_each(curr_parms,chandev_parms_head)
			{
				if(probe->chan_type==curr_parms->chan_type&&
				   read->sch.devno>=curr_parms->lo_devno&&
					read->sch.devno<=curr_parms->hi_devno)
				{
					if (!probeinfo.parmstr) {
						probeinfo.parmstr = vmalloc(sizeof(curr_parms->parmstr)+1);
						strcpy(probeinfo.parmstr, curr_parms->parmstr);
					} else {
						char *buf;

						buf = vmalloc(strlen(probeinfo.parmstr)+strlen(curr_parms->parmstr)+2);
						sprintf(buf, "%s,%s",probeinfo.parmstr, curr_parms->parmstr);
						probeinfo.parmstr=buf;
					}
				}
			}
			if(force)
			{
				if(force->chan_type==chandev_type_claw)
					memcpy(&probeinfo.claw,&force->claw,sizeof(chandev_claw_info));
				probeinfo.port_protocol_no=force->port_protocol_no;
				if(force->devif_num==-1&&force->devif_num==-2)
					probeinfo.devif_num=-1;
				else
					probeinfo.devif_num=force->devif_num;
				probeinfo.memory_usage_in_k=force->memory_usage_in_k;
				probeinfo.checksum_received_ip_pkts=force->checksum_received_ip_pkts;
				probeinfo.use_hw_stats=force->use_hw_stats;
			}
			else
			{
				probeinfo.port_protocol_no=0;
				probeinfo.checksum_received_ip_pkts=model_info->default_checksum_received_ip_pkts;
				probeinfo.use_hw_stats=model_info->default_use_hw_stats;
				probeinfo.memory_usage_in_k=0;
				if(probe->chan_type&chandev_type_lcs)
				{
					hint=(read->sch.devno&0xFF)>>1;
					if(hint>model_info->max_port_no)
					{
				/* The card is possibly emulated e.g P/390 */
				/* or possibly configured to use a shared */
				/* port configured by osa-sf. */
						hint=0;
					}
				}
			}
			probeinfo.hint_port_no=hint;
			probefunc=probe->probefunc;
			rc=probefunc(&probeinfo);
			if(rc==0)
			{
				newdevice=probeinfo.newdevice;
				if(newdevice)
				{
					newdevice->probefunc=probe->probefunc;
					newdevice->shutdownfunc=probe->shutdownfunc;
					newdevice->msck_notfunc=probe->msck_notfunc;
					probe->devices_found++;
					chandev_add_to_list((list **)&chandev_activelist_head,
							    newdevice);
					chandev_add_to_userland_notify_list(chandev_start,
								      newdevice->devname,chandev_status_good,chandev_status_good);
				}
				else
				{
					printk("chandev_initdevice either failed or wasn't called for device read_irq=0x%04x\n",probeinfo.read.irq);
				}
				break;
				
			}
		}
	}
	chandev_remove(read);
	chandev_remove(write);
	if(data)
		chandev_remove(data);
	return(rc);
}


int chandev_request_irq_from_irqinfo(chandev_irqinfo *irqinfo,chandev *this_chandev)
{
	int retval=s390_request_irq_special(irqinfo->sch.irq,
				   irqinfo->handler,
				   chandev_not_oper_handler,
				   irqinfo->irqflags,
				   irqinfo->devname,
				   irqinfo->dev_id);
	if(retval==0)
	{
		irqinfo->msck_status=chandev_status_good;
		this_chandev->owned=TRUE;
	}
	return(retval);
}

void chandev_irqallocerr(chandev_irqinfo *irqinfo,int err)
{
	printk("chandev_probe failed to realloc irq=%d for %s err=%d\n",irqinfo->sch.irq,irqinfo->devname,err);
}


void chandev_call_notification_func(chandev_activelist *curr_device,chandev_irqinfo *curr_irqinfo,
chandev_msck_status prevstatus)
{
	if(curr_irqinfo->msck_status!=prevstatus)
	{
		chandev_msck_status new_msck_status=curr_irqinfo->msck_status;
		if(curr_irqinfo->msck_status==chandev_status_good)
		{
			if(curr_device->read_irqinfo->msck_status==chandev_status_good&&
			   curr_device->write_irqinfo->msck_status==chandev_status_good)
			{
				if(curr_device->data_irqinfo)
				{
					if(curr_device->data_irqinfo->msck_status==chandev_status_good)
						new_msck_status=chandev_status_all_chans_good;
				}
				else
					new_msck_status=chandev_status_all_chans_good;
			}
		}
		if(curr_device->msck_notfunc)
		{
			curr_device->msck_notfunc(curr_device->dev_ptr,
					      curr_irqinfo->sch.irq,
					      prevstatus,new_msck_status);
		}
		if(new_msck_status!=chandev_status_good)
		{
			/* No point in sending a machine check if only one channel is good */
			chandev_add_to_userland_notify_list(chandev_msck,curr_device->devname,
						      prevstatus,curr_irqinfo->msck_status);
		}
	}
}

int chandev_find_eligible_channels(chandev *first_chandev_to_check,
			       chandev **read,chandev **write,chandev **data,chandev **next,
				   chandev_type chan_type)
{
	chandev *curr_chandev;
	int eligible_found=FALSE,changed;
	
	*next=first_chandev_to_check->next;
	*read=*write=*data=NULL;
	for_each(curr_chandev,first_chandev_to_check)
		if((curr_chandev->sch.devno&1)==0&&curr_chandev->model_info->chan_type!=chandev_type_claw)
		{
			*read=curr_chandev;
			if(chan_type==chandev_type_none)
				chan_type=(*read)->model_info->chan_type;
			break;
		}
	if(*read)
	{
		for_each(curr_chandev,(chandev *)chandev_head.head)
			if((((*read)->sch.devno|1)==curr_chandev->sch.devno)&&
			   (chandev_compare_cu_dev_info(&(*read)->sch,&curr_chandev->sch)==0)&&
			   ((chan_type&(chandev_type_ctc|chandev_type_escon))||
			    chandev_compare_chpid_info(&(*read)->sch,&curr_chandev->sch)==0))
			{
				*write=curr_chandev;
				break;
			}
	}
	if((chan_type&chandev_type_qeth))
	{
		if(*write)
		{
			for_each(curr_chandev,(chandev *)chandev_head.head)
				if((curr_chandev!=*read&&curr_chandev!=*write)&&
				   (chandev_compare_cu_dev_info(&(*read)->sch,&curr_chandev->sch)==0)&&
				   (chandev_compare_chpid_info(&(*read)->sch,&curr_chandev->sch)==0))
				{
					*data=curr_chandev;
					break;
				}
			if(*data)
				eligible_found=TRUE;
		}
		
	}
	else
		if(*write)
			eligible_found=TRUE;
	if(eligible_found)
	{
		do
		{
			changed=FALSE;
			if(*next&&
			   ((*read&&(*read==*next))||
			   (*write&&(*write==*next))||
			   (*data&&(*data==*next))))
			{
				*next=(*next)->next;
				changed=TRUE;
			}
		}while(changed==TRUE);
	}
	return(eligible_found);
}

chandev *chandev_get_free_chandev_by_devno(int devno)
{
	chandev *curr_chandev;
	if(devno==-1)
		return(NULL);
	for_each(curr_chandev,(chandev *)chandev_head.head)
		if(curr_chandev->sch.devno==devno)
		{
			if(chandev_active(devno))
				return(NULL);
			else
				return(curr_chandev);
		}
	return(NULL);

}

void chandev_probe(void)
{
	chandev *read_chandev,*write_chandev,*data_chandev,*curr_chandev,*next_chandev;
	chandev_force *curr_force;
	chandev_noauto_range *curr_noauto;
	chandev_activelist *curr_device;
	chandev_irqinfo *curr_irqinfo;
	s390_dev_info_t curr_devinfo;
	int  err;
	int auto_msck_recovery;
	chandev_msck_status prevstatus;
	chandev_msck_range *curr_msck_range;


	chandev_interrupt_check();
	chandev_read_conf_if_necessary();
	chandev_collect_devices();
	chandev_lock();
	for_each(curr_irqinfo,chandev_irqinfo_head)
	{
		if((curr_device=chandev_get_activelist_by_irq(curr_irqinfo->sch.irq)))
		{
			prevstatus=curr_irqinfo->msck_status;
			if(curr_irqinfo->msck_status!=chandev_status_good)
			{
				curr_chandev=chandev_get_by_irq(curr_irqinfo->sch.irq);
				if(curr_chandev)
				{
					auto_msck_recovery=curr_chandev->model_info->
						auto_msck_recovery;
				}
				else
					goto remove;
				for_each(curr_msck_range,chandev_msck_range_head)
				{
					if(curr_msck_range->lo_devno<=
					   curr_irqinfo->sch.devno&&
					   curr_msck_range->hi_devno>=
					   curr_irqinfo->sch.devno)
					{
						auto_msck_recovery=
							curr_msck_range->
							auto_msck_recovery;
						break;
					}
				}
				if((1<<(curr_irqinfo->msck_status-1))&auto_msck_recovery)
				{
					if(curr_irqinfo->msck_status==chandev_status_revalidate)
					{
						if((get_dev_info_by_irq(curr_irqinfo->sch.irq,&curr_devinfo)==0))
						{
							curr_irqinfo->sch.devno=curr_devinfo.devno;
							curr_irqinfo->msck_status=chandev_status_good;
						}
					}
					else
					{
						if(curr_chandev)
						{
							/* Has the device reappeared */
							if(chandev_compare_subchannel_info(
								&curr_chandev->sch,
								&curr_device->read_irqinfo->sch)||
							   chandev_compare_subchannel_info(
								&curr_chandev->sch,
								&curr_device->write_irqinfo->sch)||
							   (curr_device->data_irqinfo&&
							    chandev_compare_subchannel_info(
								    &curr_chandev->sch,
								    &curr_device->data_irqinfo->sch)))
							{
								if((err=chandev_request_irq_from_irqinfo(curr_irqinfo,curr_chandev))==0)
									curr_irqinfo->msck_status=chandev_status_good;
								else
									chandev_irqallocerr(curr_irqinfo,err);
							}
					
						}
					}
				}
			}
			chandev_call_notification_func(curr_device,curr_irqinfo,prevstatus);
		}
		/* This is required because the device can go & come back */
                /* even before we realize it is gone owing to the waits in our kernel threads */
		/* & the device will be marked as not owned but its status will be good */
                /* & an attempt to accidently reprobe it may be done. */ 
		remove:
		chandev_remove(chandev_get_by_irq(curr_irqinfo->sch.irq));
		
	}
	/* extra sanity */
	for_each_allow_delete(curr_chandev,next_chandev,(chandev *)chandev_head.head)
		if(curr_chandev->owned)
			chandev_remove(curr_chandev);
	for_each(curr_force,chandev_force_head)
	{
		if(curr_force->devif_num==-2)
		{
			for_each_allow_delete2(curr_chandev,next_chandev,(chandev *)chandev_head.head)
			{
				if(chandev_find_eligible_channels(curr_chandev,&read_chandev,
								  &write_chandev,&data_chandev,
								  &next_chandev,
								  curr_force->chan_type));
				{
					if((curr_force->read_lo_devno>=read_chandev->sch.devno)&&
					   (curr_force->write_hi_devno<=read_chandev->sch.devno)&&
					   (curr_force->read_lo_devno>=write_chandev->sch.devno)&&
					   (curr_force->write_hi_devno<=write_chandev->sch.devno)&&
					   (!data_chandev||(data_chandev&&
					   (curr_force->read_lo_devno>=data_chandev->sch.devno)&&
					   (curr_force->write_hi_devno<=data_chandev->sch.devno))))
						chandev_doprobe(curr_force,read_chandev,write_chandev,
								data_chandev);
				}
			}
		}
		else
		{
			read_chandev=chandev_get_free_chandev_by_devno(curr_force->read_lo_devno);
			if(read_chandev)
			{
				write_chandev=chandev_get_free_chandev_by_devno(curr_force->write_hi_devno);
				if(write_chandev)
				{
					if(curr_force->chan_type==chandev_type_qeth)
					{

						data_chandev=chandev_get_free_chandev_by_devno(curr_force->data_devno);
						if(data_chandev==NULL)
							printk("chandev_probe unable to force gigabit_ethernet driver invalid device  no 0x%04x given\n",curr_force->data_devno);
					}
					else
						data_chandev=NULL;
					chandev_doprobe(curr_force,read_chandev,write_chandev,
							data_chandev);
				}
			}
		}
	}
	for_each_allow_delete(curr_chandev,next_chandev,(chandev *)chandev_head.head)
	{
		for_each(curr_noauto,chandev_noauto_head)
		{
			if(curr_chandev->sch.devno>=curr_noauto->lo_devno&&
			   curr_chandev->sch.devno<=curr_noauto->hi_devno)
			{
				chandev_remove(curr_chandev);
				break;
			}
		}
	}
	for_each_allow_delete2(curr_chandev,next_chandev,(chandev *)chandev_head.head)
	{
		if(chandev_find_eligible_channels(curr_chandev,&read_chandev,
						  &write_chandev,&data_chandev,
						  &next_chandev,
						  chandev_type_none))
			chandev_doprobe(NULL,read_chandev,write_chandev,
					data_chandev);
	}
	chandev_remove_all();
	chandev_unlock();
}

static void chandev_not_oper_func(int irq,int status)
{
	chandev_irqinfo *curr_irqinfo;
	chandev_activelist *curr_device;
	
	chandev_lock();
	for_each(curr_irqinfo,chandev_irqinfo_head)
		if(curr_irqinfo->sch.irq==irq)
		{
			chandev_msck_status prevstatus=curr_irqinfo->msck_status;
			switch(status)
			{
				/* Currently defined but not used in kernel */
				/* Despite being in specs */
			case DEVSTAT_NOT_OPER:
				curr_irqinfo->msck_status=chandev_status_not_oper;
				break;
#ifdef DEVSTAT_NO_PATH
				/* Kernel hasn't this defined currently. */
				/* Despite being in specs */
			case DEVSTAT_NO_PATH:
				curr_irqinfo->msck_status=chandev_status_no_path;
				break;
#endif
			case DEVSTAT_REVALIDATE:
				curr_irqinfo->msck_status=chandev_status_revalidate;
				break;
			case DEVSTAT_DEVICE_GONE:
				curr_irqinfo->msck_status=chandev_status_gone;
				break;
                        }
                        if((curr_device=chandev_get_activelist_by_irq(irq)))
					chandev_call_notification_func(curr_device,curr_irqinfo,prevstatus);
 			else
				printk("chandev_not_oper_func received channel check for unowned irq %d",irq);
		}
	chandev_unlock();
}


static int chandev_msck_thread(void *unused)
{
	int loopcnt,not_oper_probe_required=FALSE;
	wait_queue_head_t    wait;
	chandev_not_oper_struct *new_not_oper;

	/* This loop exists because machine checks tend to come in groups & we have
           to wait for the other devnos to appear also */
	init_waitqueue_head(&wait);
	for(loopcnt=0;loopcnt<10||(jiffies-chandev_last_machine_check)<HZ;loopcnt++)
	{
		sleep_on_timeout(&wait,HZ);
	}
	atomic_set(&chandev_msck_thread_lock,1);
	while(!atomic_compare_and_swap(TRUE,FALSE,&chandev_new_msck));
	{
		chandev_probe();
	}
	while(TRUE)
	{
		
		unsigned long        flags; 
		spin_lock_irqsave(&chandev_not_oper_spinlock,flags);
		new_not_oper=(chandev_not_oper_struct *)dequeue_head(&chandev_not_oper_head);
		spin_unlock_irqrestore(&chandev_not_oper_spinlock,flags);
		if(new_not_oper)
		{
			chandev_not_oper_func(new_not_oper->irq,new_not_oper->status);
			not_oper_probe_required=TRUE;
			kfree(new_not_oper);
		}
		else
			break;
	}
	if(not_oper_probe_required)
		chandev_probe();
	return(0);
}

static void chandev_msck_task(void *unused)
{
	if(kernel_thread(chandev_msck_thread,NULL,SIGCHLD)<0)
	{
		atomic_set(&chandev_msck_thread_lock,1);
		printk("error making chandev_msck_thread kernel thread\n");
	}
}



static char *argstrs[]=
{
	"noauto",
	"del_noauto",
	"ctc",
	"escon",
	"lcs",
	"osad",
	"qeth",
	"claw",
	"add_parms",
	"del_parms",
	"del_force",
#if LINUX_VERSION_CODE>=KERNEL_VERSION(2,3,0)
	"use_devno_names",
	"dont_use_devno_names",
#endif
	"cautious_auto_detect",
	"non_cautious_auto_detect",
	"add_model",
	"del_model",
	"auto_msck",
	"del_auto_msck",
	"del_all_models",
	"reset_conf_clean",
	"reset_conf",
	"shutdown",
	"reprobe",
	"unregister_probe",
	"unregister_probe_by_chan_type",
	"read_conf",
	"dont_read_conf",
	"persist"
};

typedef enum
{
	stridx_mult=256,
	first_stridx=0,
	noauto_stridx=first_stridx,
	del_noauto_stridx,
	ctc_stridx,
	escon_stridx,
	lcs_stridx,
	osad_stridx,
        qeth_stridx,
	claw_stridx,
	add_parms_stridx,
	del_parms_stridx,
	del_force_stridx,
#if LINUX_VERSION_CODE>=KERNEL_VERSION(2,3,0)
	use_devno_names_stridx,
	dont_use_devno_names_stridx,
#endif
	cautious_auto_detect_stridx,
	non_cautious_auto_detect_stridx,
	add_model_stridx,
	del_model_stridx,
	auto_msck_stridx,
	del_auto_msck_stridx,
	del_all_models_stridx,
	reset_conf_clean_stridx,
	reset_conf_stridx,
	shutdown_stridx,
	reprobe_stridx,
	unregister_probe_stridx,
	unregister_probe_by_chan_type_stridx,
	read_conf_stridx,
	dont_read_conf_stridx,
	persist_stridx,
	last_stridx,
} chandev_str_enum;

void chandev_add_noauto(u16 lo_devno,u16 hi_devno)
{
	chandev_noauto_range *new_range;

	if((new_range=chandev_alloc(sizeof(chandev_noauto_range))))
	{
		new_range->lo_devno=lo_devno;
		new_range->hi_devno=hi_devno;
		chandev_add_to_list((list **)&chandev_noauto_head,new_range);
	}
}


void chandev_add_msck_range(u16 lo_devno,u16 hi_devno,int auto_msck_recovery)
{
	chandev_msck_range *new_range;

	if((new_range=chandev_alloc(sizeof(chandev_msck_range))))
	{
		new_range->lo_devno=lo_devno;
		new_range->hi_devno=hi_devno;
		new_range->auto_msck_recovery=auto_msck_recovery;
		chandev_add_to_list((list **)&chandev_msck_range_head,new_range);
	}
}



static char chandev_keydescript[]=
"\nchan_type key bitfield ctc=0x1,escon=0x2,lcs=0x4,osad=0x8,qeth=0x10,claw=0x20\n";


#if  CONFIG_ARCH_S390X
/* We need this as we sometimes use this to evaluate pointers */
typedef long chandev_int; 
#else
typedef int chandev_int;
#endif


#if (LINUX_VERSION_CODE<KERNEL_VERSION(2,3,0)) || (CONFIG_ARCH_S390X)
/*
 * Read an int from an option string; if available accept a subsequent
 * comma as well.
 *
 * Return values:
 * 0 : no int in string
 * 1 : int found, no subsequent comma
 * 2 : int found including a subsequent comma
 */
static chandev_int chandev_get_option(char **str,chandev_int *pint)
{
    char *cur = *str;

    if (!cur || !(*cur)) return 0;
    *pint = simple_strtol(cur,str,0);
    if (cur==*str) return 0;
    if (**str==',') {
        (*str)++;
        return 2;
    }

    return 1;
}


static char *chandev_get_options(char *str, int nints, chandev_int *ints)
{
	int res,i=1;

	while (i<nints) 
	{
		res = chandev_get_option(&str, ints+i);
		if (res==0) break;
		i++;
		if (res==1) break;
	}
	ints[0] = i-1;
	return(str);
}
#else
#define chandev_get_option get_option
#define chandev_get_options get_options
#endif
/*
 * Read an string from an option string; if available accept a subsequent
 * comma as well & set this comma to a null character when returning the string.
 *
 * Return values:
 * 0 : no string found
 * 1 : string found, no subsequent comma
 * 2 : string found including a subsequent comma
 */
static int chandev_get_string(char **instr,char **outstr)
{
	char *cur = *instr;

	if (!cur ||*cur==0)
	{
		*outstr=NULL;
		return 0;
	}
	*outstr=*instr;
	for(;;)
	{
		if(*(++cur)==',')
		{
			*cur=0;
			*instr=cur+1;
			return 2;
		}
		else if(*cur==0)
		{
			*instr=cur+1;
			return 1;
		}
	}
}




static int chandev_setup(int in_read_conf,char *instr,char *errstr,int lineno)
{
	chandev_strval   val=isnull;
	chandev_str_enum stridx;
	long             endlong;
	chandev_type     chan_type;
	char             *str,*currstr,*interpretstr=NULL;
	int              cnt,strcnt;
	int              retval=0;
#define CHANDEV_MAX_EXTRA_INTS 12
	chandev_int ints[CHANDEV_MAX_EXTRA_INTS+1];
	currstr=alloca(strlen(instr)+1);
	strcpy(currstr,instr);
	strcnt=chandev_pack_args(currstr);
	for(cnt=1;cnt<=strcnt;cnt++)
	{
		interpretstr=currstr;
		memset(ints,0,sizeof(ints));
		for(stridx=first_stridx;stridx<last_stridx;stridx++)
		{
			str=currstr;
			if((val=chandev_strcmp(argstrs[stridx],&str,&endlong)))
				break;
		}
		currstr=str;
		if(val)
		{
			val=(((chandev_strval)stridx)*stridx_mult)+(val&~isstr);
			switch(val)
			{
			case (add_parms_stridx*stridx_mult)|iscomma:
				currstr=chandev_get_options(currstr,4,ints);
				if(*currstr&&ints[0]>=1)
				{
					if(ints[0]==1)
					{
						ints[2]=0;
						ints[3]=0xffff;
					}
					else if(ints[0]==2)
						ints[3]=ints[2];
					chandev_add_parms(ints[1],ints[2],ints[3],currstr);
					goto NextOption;
				}
				else
					goto BadArgs;
				break;
			case (claw_stridx*stridx_mult)|isnum|iscomma:
			case (claw_stridx*stridx_mult)|iscomma:
				currstr=chandev_get_options(str,6,ints);
				break;
			default:
				if(val&iscomma)
					currstr=chandev_get_options(str,CHANDEV_MAX_EXTRA_INTS,ints);
				break;
			}
			switch(val)
			{
			case noauto_stridx*stridx_mult:
			case (noauto_stridx*stridx_mult)|iscomma:
				switch(ints[0])
				{
				case 0: 
					chandev_free_all_list((list **)&chandev_noauto_head);
					chandev_add_noauto(0,0xffff);
					break;
				case 1:
					ints[2]=ints[1];
				case 2:
					chandev_add_noauto(ints[1],ints[2]);
					break;
				default:
					goto BadArgs;
				}
				break;
			case (auto_msck_stridx*stridx_mult)|iscomma:
				switch(ints[0])
				{
				case 1:
					chandev_free_all_list((list **)&chandev_msck_range_head);
					chandev_add_msck_range(0,0xffff,ints[1]);
					break;
				case 2:
					chandev_add_msck_range(ints[1],ints[1],ints[2]);
					break;
				case 3:
					chandev_add_msck_range(ints[1],ints[2],ints[3]);
					break;
				default:
					goto BadArgs;
					
				}
				break;
			case del_auto_msck_stridx*stridx_mult:
			case (del_auto_msck_stridx*stridx_mult)|iscomma:
				switch(ints[0])
				{
				case 0:
					chandev_free_all_list((list **)&chandev_msck_range_head);
					break;
				case 1:
					chandev_del_msck(ints[1]);
				default:
					goto BadArgs;
				}
				break;
			case del_noauto_stridx*stridx_mult:
				chandev_free_all_list((list **)&chandev_noauto_head);
				break;
			case (del_noauto_stridx*stridx_mult)|iscomma:
				if(ints[0]==1)
					chandev_del_noauto(ints[1]);
				else
					goto BadArgs;
				break;
			case (qeth_stridx*stridx_mult)|isnum|iscomma:
				if(ints[0]<3||ints[0]>7)
					goto BadArgs;
				chandev_add_force(chandev_type_qeth,endlong,ints[1],ints[2],
						  ints[3],ints[4],ints[5],ints[6],ints[7],
						  NULL,NULL,NULL);
				break;
			case (ctc_stridx*stridx_mult)|isnum|iscomma:
			case (escon_stridx*stridx_mult)|isnum|iscomma:
			case (lcs_stridx*stridx_mult)|isnum|iscomma:
			case (osad_stridx*stridx_mult)|isnum|iscomma:
			case (ctc_stridx*stridx_mult)|iscomma:
			case (escon_stridx*stridx_mult)|iscomma:
			case (lcs_stridx*stridx_mult)|iscomma:
			case (osad_stridx*stridx_mult)|iscomma:
				switch(val&~(isnum|iscomma))
				{
				case (ctc_stridx*stridx_mult):
					chan_type=chandev_type_ctc;
					break;
				case (escon_stridx*stridx_mult):
					chan_type=chandev_type_escon;
					break;
				case (lcs_stridx*stridx_mult):
					chan_type=chandev_type_lcs;
					break;
				case (osad_stridx*stridx_mult):
					chan_type=chandev_type_osad;
					break;
				case (qeth_stridx*stridx_mult):
					chan_type=chandev_type_qeth;
					break;
				default:
					goto BadArgs;
				}
				if((val&isnum)==0)
					endlong=-2;
				if(ints[0]<2||ints[0]>6)
					goto BadArgs;
				chandev_add_force(chan_type,endlong,ints[1],ints[2],
						  0,ints[3],ints[4],ints[5],ints[6],
						  NULL,NULL,NULL);
				break;
			case (claw_stridx*stridx_mult)|isnum|iscomma:
			case (claw_stridx*stridx_mult)|iscomma:
				if(ints[0]>=2&&ints[0]<=5)
				{
					char    *host_name,*adapter_name,*api_type;
					char    *clawstr=alloca(strlen(currstr)+1);
					
					strcpy(clawstr,currstr);
					if(!(chandev_get_string(&clawstr,&host_name)==2&&
					     chandev_get_string(&clawstr,&adapter_name)==2&&
					     chandev_get_string(&clawstr,&api_type)==1&&
					     chandev_add_force(chandev_type_claw,
							       endlong,ints[1],ints[2],0,
							       ints[3],0,ints[4],ints[5],
							       host_name,adapter_name,api_type)==0))
						goto BadArgs;
						
				}
				else
					goto BadArgs;
				break;
			case (del_parms_stridx*stridx_mult):
				ints[1]=-1;
			case (del_parms_stridx*stridx_mult)|iscomma:
				if(ints[0]==0)
					ints[1]=-1;
				if(ints[0]<=1)
					ints[2]=FALSE;
				if(ints[0]<=2)
					ints[3]=-1;
				if(ints[0]>3)
					goto BadArgs;
				chandev_remove_parms(ints[1],ints[2],ints[3]);
				break;
			case (del_force_stridx*stridx_mult)|iscomma:
				if(ints[0]!=1)
					goto BadArgs;
				chandev_del_force(ints[1]);
				break;
			case (del_force_stridx*stridx_mult):
				chandev_del_force(-1);
				break;
#if LINUX_VERSION_CODE>=KERNEL_VERSION(2,3,0)
			case (use_devno_names_stridx*stridx_mult):
				chandev_use_devno_names=TRUE;
				break;
			case (dont_use_devno_names_stridx*stridx_mult):
				chandev_use_devno_names=FALSE;
				break;
#endif
			case (cautious_auto_detect_stridx*stridx_mult):
				chandev_cautious_auto_detect=TRUE;
				break;
			case (non_cautious_auto_detect_stridx*stridx_mult):
				chandev_cautious_auto_detect=FALSE;
				break;
			case (add_model_stridx*stridx_mult)|iscomma:
				if(ints[0]<3)
					goto BadArgs;
				if(ints[0]==3)
					ints[4]=-1;
				if(ints[0]<=4)
					ints[5]=-1;
				if(ints[0]<=5)
					ints[6]=-1;
				if(ints[0]<=6)
					ints[7]=default_msck_bits;
				if(ints[0]<=7)
					ints[8]=FALSE;
				if(ints[0]<=8)
					ints[9]=FALSE;
				ints[0]=7;
				chandev_add_model(ints[1],ints[2],ints[3],
						  ints[4],ints[5],ints[6],ints[7],ints[8],ints[9]);
				break;
			case (del_model_stridx*stridx_mult)|iscomma:
				if(ints[0]<2||ints[0]>4)
					goto BadArgs;
				if(ints[0]<3)
					ints[3]=-2;
				if(ints[0]<4)
					ints[4]=-2;
				ints[0]=4;
				chandev_del_model(ints[1],ints[2],ints[3],ints[4]);
				break;
			case del_all_models_stridx*stridx_mult:
				chandev_remove_all_models();
				break;
			case reset_conf_stridx*stridx_mult:
				chandev_reset();
				chandev_init_default_models();
				break;
			case reset_conf_clean_stridx*stridx_mult:
				chandev_reset();
				break;
			case shutdown_stridx*stridx_mult:
				chandev_shutdown_all();
				break;
			case (shutdown_stridx*stridx_mult)|iscomma:
				switch(ints[0])
				{
				case 0:
					if(strlen(str))
						chandev_shutdown_by_name(str);
					else
						goto BadArgs;
					break;
				case 1:
					chandev_shutdown_by_devno(ints[1]);
					break;
				default:
					goto BadArgs;
				}
				break;
			case reprobe_stridx*stridx_mult:
				chandev_probe();
				break;
			case unregister_probe_stridx*stridx_mult:
				chandev_free_all_list((list **)&chandev_probelist_head);
				break;
			case (unregister_probe_stridx*stridx_mult)|iscomma:
				if(ints[0]!=1)
					goto BadArgs;
				chandev_unregister_probe((chandev_probefunc)ints[1]);
				break;
			case (unregister_probe_by_chan_type_stridx*stridx_mult)|iscomma:
				if(ints[0]!=1)
					goto BadArgs;
				chandev_unregister_probe_by_chan_type((chandev_type)ints[1]);
				break;
			case read_conf_stridx*stridx_mult:
				if(in_read_conf)
				{
					printk("attempt to recursively call read_conf\n");
					goto BadArgs;
				}
				chandev_read_conf();
				break;
			case dont_read_conf_stridx*stridx_mult:
				atomic_set(&chandev_conf_read,TRUE);
				break;
			case (persist_stridx*stridx_mult)|iscomma:
				if(ints[0]==1)
					chandev_persistent=ints[1];
				else
					goto BadArgs;
				break;
			default:
				goto BadArgs;
			}
		}
		else
			goto BadArgs;
	NextOption:
		if(cnt<strcnt)
		{
			/* eat up stuff till next string */
			while(*(currstr++));
		}
	}
	retval=1;
 BadArgs:
	if(!retval)
	{
		printk("chandev_setup %s %s",(val==0 ? "unknown verb":"bad argument"),instr);
		if(errstr)
		{
			printk("%s %d interpreted as %s",errstr,lineno,interpretstr);
			if(strcnt>1)
			{
				if(cnt==strcnt)
					printk(" after the last semicolon\n");
				else
					printk(" before semicolon no %d",cnt);
			}
		}
		printk(".\n Type man chandev for more info.\n\n");
	}
	return(retval);
}
#define CHANDEV_KEYWORD "chandev="
static int chandev_setup_bootargs(char *str,int paramno)
{
	int len;

	char *copystr;
	for(len=0;str[len]!=0&&!isspace(str[len]);len++);
	copystr=alloca(len+1);
	strncpy(copystr,str,len);
	copystr[len]=0;
	if(chandev_setup(FALSE,copystr,"at "CHANDEV_KEYWORD" bootparam no",paramno)==0)
		return(0);
	return(len);

}

/*
  We can't parse using a __setup function as kmalloc isn't available
  at this time.
 */
static void __init chandev_parse_args(void)
{
#define CHANDEV_KEYWORD "chandev="
	extern char saved_command_line[];
	int cnt,len,paramno=1;

	len=strlen(saved_command_line)-sizeof(CHANDEV_KEYWORD);
	for(cnt=0;cnt<len;cnt++)
	{
		if(strncmp(&saved_command_line[cnt],CHANDEV_KEYWORD,
			   sizeof(CHANDEV_KEYWORD)-1)==0)
		{
			cnt+=(sizeof(CHANDEV_KEYWORD)-1);	
			cnt+=chandev_setup_bootargs(&saved_command_line[cnt],paramno);
			paramno++;
		}
	}
}

int chandev_do_setup(int in_read_conf,char *buff,int size)
{
	int curr,comment=FALSE,newline=FALSE,oldnewline=TRUE;
	char *startline=NULL,*endbuff=&buff[size];

	int lineno=0;

	*endbuff=0;
	for(;buff<=endbuff;curr++,buff++)
	{
		if(*buff==0xa||*buff==0xc||*buff==0)
		{
			if(*buff==0xa||*buff==0)
				lineno++;
			*buff=0;
			newline=TRUE;
		}
		else
		{ 
			newline=FALSE;
			if(*buff=='#')
				comment=TRUE;
		}
		if(comment==TRUE)
			*buff=0;
		if(startline==NULL&&isalpha(*buff))
			startline=buff;
		if(startline&&(buff>startline)&&(oldnewline==FALSE)&&(newline==TRUE))
		{
			if((chandev_setup(in_read_conf,startline," on line no",lineno))==0)
				return(-EINVAL);
			startline=NULL;
		}
		if(newline)
			comment=FALSE;
	        oldnewline=newline;
	}
	return(0);
}


static void chandev_read_conf(void)
{
#define CHANDEV_FILE "/etc/chandev.conf"
	struct stat statbuf;
	char        *buff;
	int         curr,left,len,fd;
	mm_segment_t oldfs;

	/* if called from chandev_register_and_probe & 
	   the driver is compiled into the kernel the
	   parameters will need to be passed in from
	   the kernel boot parameter line as the root
	   fs is not mounted yet, we can't wait here.
	*/
	if(in_interrupt()||current->fs->root==NULL)
		return;
	atomic_set(&chandev_conf_read,TRUE);
	oldfs = get_fs();
	set_fs(KERNEL_DS);
	if(stat(CHANDEV_FILE,&statbuf)==0)
	{
		set_fs(USER_DS);
		buff=vmalloc(statbuf.st_size+1);
		if(buff)
		{
			set_fs(KERNEL_DS);
			if((fd=open(CHANDEV_FILE,O_RDONLY,0))!=-1)
			{
				curr=0;
				left=statbuf.st_size;
				while((len=read(fd,&buff[curr],left))>0)
				{
					curr+=len;
					left-=len;
				}
				close(fd);
			}
			set_fs(USER_DS);
			chandev_do_setup(TRUE,buff,statbuf.st_size);
			vfree(buff);
		}
	}
	set_fs(oldfs);
}

static void chandev_read_conf_if_necessary(void)
{
	if(in_interrupt()||current->fs->root==NULL)
		return;
	if(!atomic_compare_and_swap(FALSE,TRUE,&chandev_conf_read))
		chandev_read_conf();
}

#ifdef CONFIG_PROC_FS
#define chandev_printf(exitchan,args...)     \
splen=sprintf(spbuff,##args);                \
spoffset+=splen;                             \
if(spoffset>offset) {                        \
       spbuff+=splen;                        \
       currlen+=splen;                       \
}                                            \
if(currlen>=length)                          \
       goto exitchan;

void sprintf_msck(char *buff,int auto_msck_recovery)
{
	chandev_msck_status idx;
	int first_time=TRUE;
	buff[0]=0;
	for(idx=chandev_status_first_msck;idx<chandev_status_last_msck;idx++)
	{
		if((1<<(idx-1))&auto_msck_recovery)
		{
			buff+=sprintf(buff,"%s%s",(first_time ? "":","),
				      msck_status_strs[idx]);
			first_time=FALSE;
		}
	}
}

static int chandev_read_proc(char *page, char **start, off_t offset,
			  int length, int *eof, void *data)
{
	char *spbuff=*start=page;
	int    currlen=0,splen=0;
	off_t  spoffset=0;
	chandev_model_info *curr_model;
	chandev_noauto_range *curr_noauto;
	chandev_force *curr_force;
	chandev_activelist *curr_device;
	chandev_probelist  *curr_probe;
	chandev_msck_range *curr_msck_range;
	s390_dev_info_t   curr_devinfo;
	int pass,chandevs_detected,curr_irq,loopcnt;
	chandev_irqinfo *read_irqinfo,*write_irqinfo,*data_irqinfo;
	char buff[3][80];    

	chandev_lock();
	chandev_printf(chan_exit,"\n%s\n"
		       "*'s for cu/dev type/models indicate don't cares\n",chandev_keydescript);
	chandev_printf(chan_exit,"\ncautious_auto_detect: %s\n",chandev_cautious_auto_detect ? "on":"off");
	chandev_printf(chan_exit,"\npersist = 0x%02x\n",chandev_persistent);
#if LINUX_VERSION_CODE>=KERNEL_VERSION(2,3,0)
	chandev_printf(chan_exit,"\nuse_devno_names: %s\n\n",chandev_use_devno_names ? "on":"off");
#endif
	
	if(chandev_models_head)
	{
		chandev_printf(chan_exit,"Channels enabled for detection\n");      
		chandev_printf(chan_exit,"  chan     cu      cu     dev   dev    max     checksum  use hw  auto recovery\n");
		chandev_printf(chan_exit,"  type    type    model  type  model  port_no. received   stats      type\n");
		chandev_printf(chan_exit,"==============================================================================\n");
		for_each(curr_model,chandev_models_head)
		{
			
			
			chandev_sprint_devinfo(buff[0],curr_model->cu_type,
					       curr_model->cu_model,
					       curr_model->dev_type,
					       curr_model->dev_model);
			sprintf_msck(buff[1],curr_model->auto_msck_recovery);
			chandev_printf(chan_exit,"  0x%02x  %s%3d %s     %s     %s\n",
				       curr_model->chan_type,buff[0],
				       (int)curr_model->max_port_no,
				       curr_model->default_checksum_received_ip_pkts ? "yes":"no ",
				       curr_model->default_use_hw_stats ? "yes":"no ",
				       buff[1]);         
		}
	}
        
	if(chandev_noauto_head)
	{
		chandev_printf(chan_exit,"\nNo auto devno ranges\n");
		chandev_printf(chan_exit,"   From        To   \n");
		chandev_printf(chan_exit,"====================\n");
		for_each(curr_noauto,chandev_noauto_head)
		{
			chandev_printf(chan_exit,"  0x%04x     0x%04x\n",
				       curr_noauto->lo_devno,
				       curr_noauto->hi_devno);
		}
	}
	if(chandev_msck_range_head)
	{
		
		chandev_printf(chan_exit,"\nAutomatic machine check recovery devno ranges\n");
		chandev_printf(chan_exit,"   From        To   automatic recovery type\n");
		chandev_printf(chan_exit,"===========================================\n");
		for_each(curr_msck_range,chandev_msck_range_head)
		{
			sprintf_msck(buff[0],curr_msck_range->auto_msck_recovery);
			chandev_printf(chan_exit,"  0x%04x     0x%04x %s\n",
				       curr_msck_range->lo_devno,
				       curr_msck_range->hi_devno,buff[0])
		}
	}
	if(chandev_force_head)
	{
		chandev_printf(chan_exit,"\nForced devices\n");
		chandev_printf(chan_exit,"  chan defif read   write  data   memory      port         ip    hw   host       adapter   api\n");
		chandev_printf(chan_exit,"  type  num  devno  devno  devno  usage(k) protocol no.  chksum stats name        name     name\n");
		chandev_printf(chan_exit,"===============================================================================================\n");
		for_each(curr_force,chandev_force_head)
		{
			if(curr_force->memory_usage_in_k==0)
				strcpy(buff[0],"default");
			else
				sprintf(buff[0],"%6d",curr_force->memory_usage_in_k);
			chandev_printf(chan_exit,"  0x%02x  %3d  0x%04x 0x%04x 0x%04x %7s       %3d       %1d    %1d%s",
				       (int)curr_force->chan_type,(int)curr_force->devif_num,
				       (int)curr_force->read_lo_devno,(int)curr_force->write_hi_devno,
				       (int)curr_force->data_devno,buff[0],
				       (int)curr_force->port_protocol_no,(int)curr_force->checksum_received_ip_pkts,
				       (int)curr_force->use_hw_stats,curr_force->chan_type==chandev_type_claw ? "":"\n");
			if(curr_force->chan_type==chandev_type_claw)
			{
				chandev_printf(chan_exit," %9s %9s %9s\n",
					       curr_force->claw.host_name,
					       curr_force->claw.adapter_name,
					       curr_force->claw.api_type);
			}

		}
	}
	if(chandev_probelist_head)
	{
#if CONFIG_ARCH_S390X
		chandev_printf(chan_exit,"\nRegistered probe functions\n"
			       		 "probefunc            shutdownfunc        msck_notfunc        chan  devices devices\n"
                                         "                                                             type   found  active\n"
			                 "==================================================================================\n");
#else
		chandev_printf(chan_exit,"\nRegistered probe functions\n"
			                 "probefunc   shutdownfunc   msck_notfunc   chan  devices devices\n"
                                         "                                          type   found  active\n"
			                 "===============================================================\n");
#endif
		for_each(curr_probe,chandev_probelist_head)
		{
			int devices_active=0;
			for_each(curr_device,chandev_activelist_head)
			{
				if(curr_device->probefunc==curr_probe->probefunc)
					devices_active++;
			}
			chandev_printf(chan_exit,"0x%p   0x%p   0x%p       0x%02x     %d      %d\n",
				       curr_probe->probefunc,
				       curr_probe->shutdownfunc,
				       curr_probe->msck_notfunc,
				       curr_probe->chan_type,
				       curr_probe->devices_found,
				       devices_active);
		}
	}
	if(chandev_activelist_head)
	{
		unsigned long long total_memory_usage_in_k=0;
		chandev_printf(chan_exit,
			       "\nInitialised Devices\n"
			       " read   write  data  read   write  data  chan port  dev     dev         memory   read msck    write msck    data msck\n"
			       " irq     irq    irq  devno  devno  devno type no.   ptr     name        usage(k)  status       status        status\n"
			       "=====================================================================================================================\n");
		/* We print this list backwards for cosmetic reasons */
		for(curr_device=chandev_activelist_head;
		    curr_device->next!=NULL;curr_device=curr_device->next);
		while(curr_device)
		{
			read_irqinfo=curr_device->read_irqinfo;
			write_irqinfo=curr_device->write_irqinfo;
			data_irqinfo=curr_device->data_irqinfo;
			if(data_irqinfo)
			{
				sprintf(buff[0],"0x%04x",data_irqinfo->sch.irq);
				sprintf(buff[1],"0x%04x",(int)data_irqinfo->sch.devno);
			}
			else
			{
				strcpy(buff[0],"  n/a ");
				strcpy(buff[1],"  n/a ");
			}
			if(curr_device->memory_usage_in_k<0)
			{
				sprintf(buff[2],"%d",(int)-curr_device->memory_usage_in_k);
				total_memory_usage_in_k-=curr_device->memory_usage_in_k;
			}
			else
				strcpy(buff[2],"  n/a ");
			chandev_printf(chan_exit,
				       "0x%04x 0x%04x %s 0x%04x 0x%04x %s 0x%02x %2d 0x%p %-10s  %6s   %-12s %-12s %-12s\n",
				       read_irqinfo->sch.irq,
				       write_irqinfo->sch.irq,
				       buff[0],
				       (int)read_irqinfo->sch.devno,
				       (int)write_irqinfo->sch.devno,
				       buff[1],
				       curr_device->chan_type,(int)curr_device->port_no,
				       curr_device->dev_ptr,curr_device->devname,
				       buff[2],
				       msck_status_strs[read_irqinfo->msck_status],
				       msck_status_strs[write_irqinfo->msck_status],
				       data_irqinfo ? msck_status_strs[data_irqinfo->msck_status] :
				       "not applicable");
			get_prev((list *)chandev_activelist_head,
				 (list *)curr_device,
				 (list **)&curr_device);
		}
		chandev_printf(chan_exit,"\nTotal device memory usage %Luk.\n",total_memory_usage_in_k);
	}
	chandevs_detected=FALSE;
	for(pass=FALSE;pass<=TRUE;pass++)
	{
		if(pass&&chandevs_detected)
		{
			chandev_printf(chan_exit,"\nchannels detected\n");
			chandev_printf(chan_exit,"              chan    cu    cu   dev    dev                          in chandev\n");
			chandev_printf(chan_exit,"  irq  devno  type   type  model type  model pim      chpids         use  reg.\n");
			chandev_printf(chan_exit,"===============================================================================\n");
		}
		for(curr_irq=get_irq_first(),loopcnt=0;curr_irq>=0; curr_irq=get_irq_next(curr_irq),loopcnt++)
		{
			if(loopcnt>0x10000)
			{
				printk(KERN_ERR"chandev_read_proc detected infinite loop bug in get_irq_next\n");
				goto chan_error;
			}
			if(chandev_is_chandev(curr_irq,&curr_devinfo,&curr_force,&curr_model))
			{
				schib_t *curr_schib;
				curr_schib=s390_get_schib(curr_irq);
				chandevs_detected=TRUE;
				if(pass)
				{
					chandev_printf(chan_exit,"0x%04x 0x%04x 0x%02x  0x%04x 0x%02x  0x%04x 0x%02x 0x%02x 0x%016Lx  %-5s%-5s\n",
						       curr_irq,curr_devinfo.devno,
						       ( curr_force ? curr_force->chan_type : 
						       ( curr_model ? curr_model->chan_type : 
							 chandev_type_none )),
						       (int)curr_devinfo.sid_data.cu_type,
						       (int)curr_devinfo.sid_data.cu_model,
						       (int)curr_devinfo.sid_data.dev_type,
						       (int)curr_devinfo.sid_data.dev_model,
						       (int)(curr_schib ? curr_schib->pmcw.pim : 0),
						       *(long long *)(curr_schib ? &curr_schib->pmcw.chpid[0] : 0),
						       (curr_devinfo.status&DEVSTAT_DEVICE_OWNED) ? "yes":"no ",
						       (chandev_get_irqinfo_by_irq(curr_irq) ? "yes":"no "));
						       
						       
				}
					
			}

		}
	}
	if(chandev_parms_head)
	{
		chandev_parms      *curr_parms;

		chandev_printf(chan_exit,"\n driver specific parameters\n");
		chandev_printf(chan_exit,"chan    lo    hi      driver\n");
		chandev_printf(chan_exit,"type  devno  devno  parameters\n");
		chandev_printf(chan_exit,"=============================================================================\n");
		for_each(curr_parms,chandev_parms_head)
		{
			chandev_printf(chan_exit,"0x%02x 0x%04x 0x%04x  %s\n",
				       curr_parms->chan_type,(int)curr_parms->lo_devno,
				       (int)curr_parms->hi_devno,curr_parms->parmstr);
		}
	}
 chan_error:
	*eof=TRUE;
 chan_exit:
	if(currlen>length) {
		/* rewind to previous printf so that we are correctly
		 * aligned if we get called to print another page.
                 */
		currlen-=splen;
	}
	chandev_unlock();
	return(currlen);
}


static int chandev_write_proc(struct file *file, const char *buffer,
			   unsigned long count, void *data)
{
	int         rc;
	char        *buff;
	
	if(count > 65536)
		count = 65536;
		
	buff=vmalloc(count+1);
	if(buff)
	{
		rc = copy_from_user(buff,buffer,count);
		if (rc)
			goto chandev_write_exit;
		chandev_do_setup(FALSE,buff,count);
		rc=count;
	chandev_write_exit:
		vfree(buff);
		return rc;
	}
	else
		return -ENOMEM;
	return(0);
}

static void __init chandev_create_proc(void)
{
	struct proc_dir_entry *dir_entry=
		create_proc_entry("chandev",0644,
				  &proc_root);
	if(dir_entry)
	{
		dir_entry->read_proc=&chandev_read_proc;
		dir_entry->write_proc=&chandev_write_proc;
	}
}


#endif
#if LINUX_VERSION_CODE>=KERNEL_VERSION(2,3,0)
static  
#endif
int __init chandev_init(void)
{
	atomic_set(&chandev_initialised,TRUE);
	chandev_parse_args();
	chandev_init_default_models();
#if CONFIG_PROC_FS
	chandev_create_proc();
#endif
	chandev_msck_task_tq.routine=
		chandev_msck_task;
#if LINUX_VERSION_CODE>=KERNEL_VERSION(2,3,0)
	INIT_LIST_HEAD(&chandev_msck_task_tq.list);
	chandev_msck_task_tq.sync=0;
#endif
	chandev_msck_task_tq.data=NULL;
	chandev_last_startmsck_list_update=chandev_last_machine_check=jiffies-HZ;
	atomic_set(&chandev_msck_thread_lock,1);
	chandev_lock_owner=CHANDEV_INVALID_LOCK_OWNER;
	chandev_lock_cnt=0;
	spin_lock_init(&chandev_spinlock);
	spin_lock_init(&chandev_not_oper_spinlock);
	atomic_set(&chandev_new_msck,FALSE);
	return(0);
}
#if LINUX_VERSION_CODE>=KERNEL_VERSION(2,3,0)
__initcall(chandev_init);
#endif

int chandev_register_and_probe(chandev_probefunc probefunc,
			       chandev_shutdownfunc shutdownfunc,
			       chandev_msck_notification_func msck_notfunc,
			       chandev_type chan_type)
{
	chandev_probelist *new_probe,*curr_probe;
	/* Avoid chicked & egg situations where we may be called before we */
	/* are initialised. */

	chandev_interrupt_check();
	if(!atomic_compare_and_swap(FALSE,TRUE,&chandev_initialised))
		chandev_init();
	chandev_lock();
	for_each(curr_probe,chandev_probelist_head)
	{
		if(curr_probe->probefunc==probefunc)
		{
			chandev_unlock();
			printk("chandev_register_and_probe detected duplicate probefunc %p"
			       " for chan_type  0x%02x \n",probefunc,chan_type);
			return (-EPERM);
		}
	}
	chandev_unlock();
	if((new_probe=chandev_alloc(sizeof(chandev_probelist))))
	{
		new_probe->probefunc=probefunc;
		new_probe->shutdownfunc=shutdownfunc;
		new_probe->msck_notfunc=msck_notfunc;
		new_probe->chan_type=chan_type;
		new_probe->devices_found=0;
		chandev_add_to_list((list **)&chandev_probelist_head,new_probe);
		chandev_probe();
	}
	return(new_probe ? new_probe->devices_found:-ENOMEM);
}

void chandev_unregister(chandev_probefunc probefunc,int call_shutdown)
{
	chandev_probelist *curr_probe;
	chandev_activelist *curr_device,*next_device;
	
	chandev_interrupt_check();
	chandev_lock();
	for_each(curr_probe,chandev_probelist_head)
	{
		if(curr_probe->probefunc==probefunc)
		{
			for_each_allow_delete(curr_device,next_device,chandev_activelist_head)
				if(curr_device->probefunc==probefunc&&call_shutdown)
					chandev_shutdown(curr_device);
			chandev_free_listmember((list **)&chandev_probelist_head,
						(list *)curr_probe);
			break;
		}
	}
	chandev_unlock();
}


int chandev_persist(chandev_type chan_type)
{
	return((chandev_persistent&chan_type) ? TRUE:FALSE);
}

EXPORT_SYMBOL(chandev_register_and_probe);
EXPORT_SYMBOL(chandev_request_irq);
EXPORT_SYMBOL(chandev_unregister);
EXPORT_SYMBOL(chandev_initdevice);
EXPORT_SYMBOL(chandev_build_device_name);
EXPORT_SYMBOL(chandev_initnetdevice);
EXPORT_SYMBOL(chandev_init_netdev);
EXPORT_SYMBOL(chandev_use_devno_names);
EXPORT_SYMBOL(chandev_free_irq);
EXPORT_SYMBOL(chandev_add_model);
EXPORT_SYMBOL(chandev_del_model);
EXPORT_SYMBOL(chandev_persist);


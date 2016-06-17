#define __NO_VERSION__

#include <linux/interrupt.h>	/* For task queue support */
#include <linux/delay.h>


/* For data going from/to the kernel through the ioctl argument */
#define DRM_COPY_FROM_USER_IOCTL(arg1, arg2, arg3)	\
	if ( copy_from_user(&arg1, arg2, arg3) )	\
		return -EFAULT
#define DRM_COPY_TO_USER_IOCTL(arg1, arg2, arg3)	\
	if ( copy_to_user(arg1, &arg2, arg3) )		\
		return -EFAULT


#define DRM_GETSAREA()							 \
do { 									 \
	drm_map_list_t *entry;						 \
	list_for_each_entry( entry, &dev->maplist->head, head ) {	 \
		if ( entry->map &&					 \
		     entry->map->type == _DRM_SHM &&			 \
		     (entry->map->flags & _DRM_CONTAINS_LOCK) ) {	 \
			dev_priv->sarea = entry->map;			 \
 			break;						 \
 		}							 \
 	}								 \
} while (0)

#define DRM_WAIT_ON( ret, queue, timeout, condition )	\
do {							\
	DECLARE_WAITQUEUE(entry, current);		\
	unsigned long end = jiffies + (timeout);	\
	add_wait_queue(&(queue), &entry);		\
							\
	for (;;) {					\
		set_current_state(TASK_INTERRUPTIBLE);	\
		if (condition) 				\
			break;				\
		if((signed)(end - jiffies) <= 0) {	\
			ret = -EBUSY;			\
			break;				\
		}					\
		schedule_timeout((HZ/100 > 1) ? HZ/100 : 1);	\
		if (signal_pending(current)) {		\
			ret = -EINTR;			\
			break;				\
		}					\
	}						\
	set_current_state(TASK_RUNNING);		\
	remove_wait_queue(&(queue), &entry);		\
} while (0)


 

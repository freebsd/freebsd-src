/*
 * Private include for xenbus communications.
 * 
 * Copyright (C) 2005 Rusty Russell, IBM Corporation
 *
 * This file may be distributed separately from the Linux kernel, or
 * incorporated into other software packages, subject to the following license:
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this source file (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * $FreeBSD$
 */

#ifndef _XENBUS_COMMS_H
#define _XENBUS_COMMS_H

int xs_init(void);
int xb_init_comms(void);

/* Low level routines. */
int xb_write(const void *data, unsigned len);
int xb_read(void *data, unsigned len);
int xs_input_avail(void);
extern int xb_waitq;
extern int xenbus_running;

#define __wait_event_interruptible(wchan, condition, ret) 	\
do {								\
        for (;;) {                                              \
                if (xenbus_running == 0) {			\
			break;					\
		}                                               \
		if (condition)					\
			break;					\
		if ((ret = !tsleep(wchan, PWAIT | PCATCH, "waitev", hz/10))) \
			break;							\
	}									\
} while (0)


#define wait_event_interruptible(wchan, condition)                      \
({                                                                      \
        int __ret = 0;                                                  \
        if (!(condition))                                               \
                __wait_event_interruptible(wchan, condition, __ret);    \
        __ret;                                                          \
}) 

	  
	  
#define DECLARE_MUTEX(lock) struct sema lock
#define semaphore     sema
#define rw_semaphore  sema

#define down          sema_wait
#define up            sema_post
#define down_read     sema_wait
#define up_read       sema_post
#define down_write    sema_wait
#define up_write      sema_post

/**
 * container_of - cast a member of a structure out to the containing structure
 *
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({			\
        __typeof__( ((type *)0)->member ) *__mptr = (ptr);	\
        (type *)( (char *)__mptr - offsetof(type,member) );})
 

/*
 * XXX 
 *
 */

#define GFP_KERNEL 1
#define EXPORT_SYMBOL(x)
#define kmalloc(size, unused) malloc(size, M_DEVBUF, M_WAITOK)
#define kfree(ptr) free((void *)(uintptr_t)ptr, M_DEVBUF)
#define BUG_ON        PANIC_IF
#define semaphore     sema
#define rw_semaphore  sema
#define DEFINE_SPINLOCK(lock) struct mtx lock
#define DECLARE_MUTEX(lock) struct sema lock
#define u32           uint32_t
#define list_del(head, ent)      TAILQ_REMOVE(head, ent, list) 
#define simple_strtoul strtoul
#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#define list_empty    TAILQ_EMPTY
#define wake_up       wakeup
#define BUS_ID_SIZE  128

struct xen_bus_type
{
		char *root;
		unsigned int levels;
		int (*get_bus_id)(char bus_id[BUS_ID_SIZE], const char *nodename);
		int (*probe)(const char *type, const char *dir);
		struct xendev_list_head *bus;
		int error;
#if 0
	struct bus_type bus;
	struct device dev;
#endif 
};


#if 0

void dev_changed(const char *node, struct xen_bus_type *bus);

int 
read_otherend_details(struct xenbus_device *xendev, char *id_node, 
		      char *path_node);
#endif

char *kasprintf(const char *fmt, ...);




#endif /* _XENBUS_COMMS_H */

/*
 * Local variables:
 *  c-file-style: "linux"
 *  indent-tabs-mode: t
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 */

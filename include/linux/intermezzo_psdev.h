/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */

#ifndef __PRESTO_PSDEV_H
#define __PRESTO_PSDEV_H

#define MAX_CHANNEL 16
#define PROCNAME_SIZE 32
#include <linux/locks.h>
#include <linux/smp_lock.h>
#include <linux/version.h>

/* represents state of an instance reached with /dev/intermezzo */
/* communication pending & processing queues */
struct upc_channel {
        unsigned int         uc_seq;
        wait_queue_head_t    uc_waitq;    /* Lento wait queue */
        struct list_head     uc_pending;
        struct list_head     uc_processing;
        spinlock_t            uc_lock;
        int                  uc_pid;      /* Lento's pid */
        int                  uc_hard;     /* allows signals during upcalls */
        int                  uc_no_filter;
        int                  uc_no_journal;
        int                  uc_no_upcall;
        int                  uc_timeout;  /* . sec: signals will dequeue upc */
        long                 uc_errorval; /* for testing I/O failures */
        struct list_head     uc_cache_list;
        int                  uc_minor;
};

#define ISLENTO(minor) (current->pid == izo_channels[minor].uc_pid \
                || current->p_pptr->pid == izo_channels[minor].uc_pid \
                || current->p_pptr->p_pptr->pid == izo_channels[minor].uc_pid)

extern struct upc_channel izo_channels[MAX_CHANNEL];

/* message types between presto filesystem in kernel */
#define REQ_READ   1
#define REQ_WRITE  2
#define REQ_ASYNC  4
#define REQ_DEAD   8

struct upc_req {
        struct list_head   rq_chain;
        caddr_t            rq_data;
        int                rq_flags;
        int                rq_bufsize;
        int                rq_rep_size;
        int                rq_opcode;  /* copied from data to save lookup */
        int                rq_unique;
        wait_queue_head_t  rq_sleep;   /* process' wait queue */
        unsigned long      rq_posttime;
};

#endif

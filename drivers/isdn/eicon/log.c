/*
 * Source file for diva log facility
 *
 * Copyright (C) Eicon Technology Corporation, 2000.
 *
 * Eicon File Revision :    1.5  
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#include "sys.h"
#include "idi.h"
#include "divas.h"
#include "adapter.h"
#include "divalog.h"

#include "uxio.h"

/*Counter to monitor number of messages */ 
static int m_count;
 
#define     MAX_BUFFERED_MSGS   (1000)

/* Our Linked List Structure to hold message */
typedef struct klog_link{
  klog_t klog;
  struct klog_link *next;
}KNODE;

/* First & Last structures in list*/
KNODE *head;
KNODE *tail;

/* 
 * retrieve message from FIFO buffer
 * returns NULL if buffer empty
 * otherwise returns pointer to entry 
 */

char	*DivasLogFifoRead(void)

{
	KNODE *old_head;

	if(head==NULL) 
	{
		/* Buffer Empty - No Messages */
		return NULL;	
	}

	m_count--;
	/* Keep track of message to be read & increment to next message*/
	old_head = head;
	head = head->next;
    /*Return ptr to Msg */	
    return((char *)old_head);
}

/* 
 * write message into FIFO buffer
 */

void	DivasLogFifoWrite(char *entry, int length)

{
    KNODE *new_klog;

    if(head == NULL) 
    {
	/* No Entries in Log */
	tail=NULL;
	m_count=0;
	new_klog=UxAlloc(sizeof(KNODE));

	if(new_klog==NULL)
	{
		return;
	}

	m_count++;
	memset(new_klog, 0, sizeof(KNODE));

	/* Set head & tail to point to the new Msg Struct */
	head=tail=new_klog;
	tail->next=NULL;
    }
    else
    {
	new_klog=UxAlloc(sizeof(KNODE));
	
	if(new_klog==NULL)
	{
		return;
	}

	m_count++;
	memset(new_klog, 0, sizeof(KNODE));

	/* Let last Msg Struct point to new Msg Struct & inc tail */
	tail->next=new_klog;
	tail=new_klog;
	tail->next=NULL;
    }

    if (length > sizeof(klog_t))
    {
        length = sizeof(klog_t);
    }

    memcpy(&tail->klog, entry, length);

    return;
}

/*
 * DivaslogFifoEmpty:return TRUE if FIFO buffer is empty,otherwise FALSE
 */
int DivasLogFifoEmpty(void)
{
	return (m_count == 0);
}

/*
 *DivasLogFifoFull:return TRUE if FIFO buffer is full,otherwise FALSE
 */
int DivasLogFifoFull(void)
{
	return (m_count == MAX_BUFFERED_MSGS);
}

/*
 * generate an IDI log entry
 */

void	DivasLogIdi(card_t *card, ENTITY *e, int request)

{
	klog_t		klog;

	memset(&klog, 0, sizeof(klog));

	klog.time_stamp = UxTimeGet();

	klog.length = sizeof(ENTITY) > sizeof(klog.buffer) ?
						sizeof(klog.buffer) : sizeof(ENTITY);

	klog.card = (int) (card - DivasCards);

	klog.type = request ? KLOG_IDI_REQ : KLOG_IDI_CALLBACK;
	klog.code = 0;
	memcpy(klog.buffer, e, klog.length);

    /* send to the log driver and return */

    DivasLogAdd(&klog, sizeof(klog));

	return;
}

/* ntp_data_structures.h
 *
 * This file contains the structures and function prototypes for the data 
 * structures used by the ntp configuration code and the discrete event 
 * simulator.
 *
 * Written By: Sachin Kamboj
 *             University of Delaware
 *             Newark, DE 19711
 * Copyright (c) 2006
 */

#ifndef __NTP_DATA_STRUCTURES_H__
#define __NTP_DATA_STRUCTURES_H__


/* Structures for storing a priority queue 
 * ---------------------------------------
 */

typedef struct node {
	union {
		struct node *next;
		double d;
	} nodeu;
} node;
#define node_next nodeu.next
    
typedef struct Queue {
    int (*get_order)(void *, void *);
    node *front;
    int no_of_elements;
} queue;


/* FUNCTION PROTOTYPES
 * -------------------
 */
queue *create_priority_queue(int (*get_order)(void *, void *));
void destroy_queue(queue *my_queue);
void *get_node(size_t size);
void free_node(void *my_node);
void *next_node(void *my_node);
int empty(queue *my_queue);
void *queue_head(queue *my_queue);
queue *enqueue(queue *my_queue, void *my_node);
void *dequeue(queue *my_queue);
int get_no_of_elements(queue *my_queue);
void append_queue(queue *q1, queue *q2);
int get_fifo_order(void *el1, void *el2);
queue *create_queue(void);

#endif

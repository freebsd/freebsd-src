/*
 * linux/include/asm-cris/fasttimer.h
 *
 * Fast timers for ETRAX100LX
 * This may be useful in other OS than Linux so use 2 space indentation...
 * Copyright (C) 2000, 2001, 2002, 2003 Axis Communications AB
 */

#include <linux/config.h>
#include <linux/time.h> /* struct timeval */
#include <linux/timex.h>

/* The timer0 values gives 52us resolution (1/19200) or higher
 * but interrupts at HZ
 */
/* We use timer1 to generate interrupts at desired times. */

#ifdef CONFIG_ETRAX_FAST_TIMER

typedef void fast_timer_function_type(unsigned long);

struct fast_timer { /* Close to timer_list */
  struct fast_timer *next;
  struct fast_timer *prev;
  struct timeval tv_set;
  struct timeval tv_expires;
  unsigned long delay_us;
  fast_timer_function_type *function;
  unsigned long data;
  const char *name;
};

extern struct fast_timer *fast_timer_list;

void start_one_shot_timer(struct fast_timer *t,
                          fast_timer_function_type *function,
                          unsigned long data,
                          unsigned long delay_us,
                          const char *name);

int del_fast_timer(struct fast_timer * t);
/* return 1 if deleted */

extern inline int fast_timer_pending (const struct fast_timer * t)
{
  return (t->next != NULL) || (t->prev != NULL) || (t == fast_timer_list);
}

void schedule_usleep(unsigned long us);


void fast_timer_init(void);

#endif

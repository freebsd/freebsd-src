/* tty_clk_STREAMS.c,v 3.1 1993/07/06 01:07:34 jbj Exp
 * Timestamp STREAMS module for SunOS 4.1
 *
 * Copyright 1991, Nick Sayer
 *
 * Special thanks to Greg Onufer for his debug assists.
 *
 * Should be PUSHed directly on top of a serial I/O channel.
 * For any character in a user-designated set, adds a kernel
 * timestamp to that character.
 *
 * BUGS:
 *
 * Only so many characters can be timestamped. This number, however,
 * is adjustable.
 *
 * The null character ($00) cannot be timestamped.
 *
 * The M_DATA messages passed upstream will not be the same
 * size as when they arrive from downstream, even if no
 * timestamp character is in the message. This, however,
 * should not affect anything.
 *
 */

#include "clk.h"
#if NCLK > 0
/*
 * How big should the messages we pass upstream be?
 */
#define MESSAGE_SIZE 128

#include <string.h>
#include <sys/types.h>
#include <sys/stream.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/user.h>
#include <sys/errno.h>

#include <sys/clkdefs.h>

static struct module_info rminfo = { 0, "clk", 0, INFPSZ, 0, 0 };
static struct module_info wminfo = { 0, "clk", 0, INFPSZ, 0, 0 };
static int clkopen(), clkrput(), clkwput(), clkclose();

static struct qinit rinit = { clkrput, NULL, clkopen, clkclose, NULL,
	&rminfo, NULL };

static struct qinit winit = { clkwput, NULL, NULL, NULL, NULL,
	&wminfo, NULL };

struct streamtab clkinfo = { &rinit, &winit, NULL, NULL };

struct priv_data_type
{
  char in_use;
  char string[CLK_MAXSTRSIZE];
} priv_data[NCLK];

char first_open=1;

/*
 * God only knows why, but linking with strchr() and index() fail
 * on my system, so here's a renamed copy.
 */

u_char *str_chr(s,c)
u_char *s;
int c;
{
  while (*s)
    if(*s++ == c)
      return (s-1);
  return NULL;
}

/*ARGSUSED*/
static int clkopen(q, dev, flag, sflag)
queue_t *q;
dev_t dev;
int flag;
int sflag;
{
  int i;

/* Damn it! We can't even have the global data struct properly
   initialized! So we have a mark to tell us to init the global
   data on the first open */

  if (first_open)
  {
    first_open=0;

    for(i=0;i<NCLK;i++)
      priv_data[i].in_use=0;
  }

  for(i=0;i<NCLK;i++)
    if(!priv_data[i].in_use)
    {
      priv_data[i].in_use++;
      ((struct priv_data_type *) (q->q_ptr))=priv_data+i;
      priv_data[i].string[0]=0;
      return (0);
    }
  u.u_error = EBUSY;
  return (OPENFAIL);
}

/*ARGSUSED*/
static int clkclose(q, flag)
queue_t *q;
int flag;
{
  ((struct priv_data_type *) (q->q_ptr))->in_use=0;

  return (0);
}

/*
 * Now the crux of the biscuit.
 *
 * If it's an M_DATA package, we take each character and pass
 * it to clkchar.
 */

void clkchar();

static int clkrput(q, mp)
queue_t *q;
mblk_t *mp;
{
  mblk_t *bp;

  switch(mp->b_datap->db_type)
  {
    case M_DATA:
      clkchar(0,q,2);
      for(bp=mp; bp!=NULL; bp=bp->b_cont)
      {
	while(bp->b_rptr < bp->b_wptr)
	  clkchar( ((u_char)*(bp->b_rptr++)) , q , 0 );
      }
      clkchar(0,q,1);
      freemsg(mp);
    break;
    default:
      putnext(q,mp);
    break;
  }

}

/*
 * If it's a matching M_IOCTL, handle it.
 */

static int clkwput(q, mp)
queue_t *q;
mblk_t *mp;
{
  struct iocblk *iocp;

  switch(mp->b_datap->db_type)
  {
    case M_IOCTL:
      iocp=(struct iocblk*) mp->b_rptr;
      if (iocp->ioc_cmd==CLK_SETSTR)
      {
        strncpy( ((struct priv_data_type *) (RD(q)->q_ptr))->string,
	  (char *) mp->b_cont->b_rptr,CLK_MAXSTRSIZE);
        /* make sure it's null terminated */
	((struct priv_data_type *) (RD(q)->q_ptr))->string[CLK_MAXSTRSIZE-1]=0;
	mp->b_datap->db_type = M_IOCACK;
	qreply(q,mp);
      }
      else
	putnext(q,mp);
    break;
    default:
      putnext(q,mp);
    break;
  }
}

/*
 * Now clkchar. It takes a character, a queue pointer and an action
 * flag and depending on the flag either:
 *
 * 0 - adds the character to the current message. If there's a
 * timestamp to be done, do that too. If the message is less than
 * 8 chars from being full, link in a new one, and set it up for
 * the next call.
 *
 * 1 - sends the whole mess to Valhala.
 *
 * 2 - set things up.
 *
 * Yeah, it's an ugly hack. Complaints may be filed with /dev/null.
 */


void clkchar(c,q,f)
	register u_char c;
	queue_t *q;
	char f;
{
  static char error;
  static mblk_t *message,*mp;
  struct timeval tv;

/* Get a timestamp ASAP! */
  uniqtime(&tv);

  switch(f)
  {
    case 1:
      if (!error)
        putnext(q,message);
    break;
    case 2:
      mp=message= (mblk_t*) allocb(MESSAGE_SIZE,BPRI_LO);
      error=(message==NULL);
      if (error)
	log(LOG_ERR,"clk: cannot allocate message - data lost");
    break;
    case 0:
      if (error) /* If we had an error, forget it. */
	return;

      *mp->b_wptr++=c; /* Put the char away first.

      /* If it's in the special string, append a struct timeval */

      if (str_chr( ((struct priv_data_type *) (q->q_ptr))->string ,
        c )!=NULL)
      {
	  int i;

	  for (i=0;i<sizeof(struct timeval);i++)
	    *mp->b_wptr++= *( ((char*)&tv) + i );
      }

      /* If we don't have space for a complete struct timeval, and a
         char, it's time for a new mp block */

      if (((mp->b_wptr-mp->b_rptr)+sizeof(struct timeval)+2)>MESSAGE_SIZE)
      {
	  mp->b_cont= (mblk_t*) allocb(MESSAGE_SIZE,BPRI_LO);
	  error=(mp->b_cont==NULL);
	  if (error)
	  {
	    log(LOG_ERR,"clk: cannot allocate message - data lost");
	    freemsg(message);
          }
          mp=mp->b_cont;
      }

    break;
  }
}

#endif

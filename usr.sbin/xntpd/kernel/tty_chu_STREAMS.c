/* tty_chu_STREAMS.c,v 3.1 1993/07/06 01:07:32 jbj Exp
 * CHU STREAMS module for SunOS 4.1.x
 *
 * Version 2.1
 *
 * Copyright 1991-1993, Nick Sayer
 *
 * Special thanks to Greg Onufer for his debug assists.
 * Special thanks to Matthias Urlichs for the loadable driver support
 *   code.
 *
 * Should be PUSHed directly on top of a serial I/O channel.
 * Provides complete chucode structures to user space.
 *
 * COMPILATION:
 *
 * To make a SunOS 4.1.x compatable loadable module (from the ntp kernel
 * directory):
 *
 * % cc -c -I../include -DLOADABLE tty_chu_STREAMS.c
 *
 * The resulting .o file is the loadable module. Modload it
 * with -entry _chuinit.
 *
 * You can also add it into the kernel by hacking it into the streams
 * table in the kernel, then adding it to config:
 *
 * pseudo-device    chuN
 *
 * where N is the maximum number of concurent chu sessions you expect
 * to have.
 *
 * HISTORY:
 *
 * v2.1 - Added 'sixth byte' heuristics.
 * v2.0 - first version with an actual version number.
 *        Added support for new CHU 'second 31' data format.
 *        Deleted PEDANTIC and ANAL_RETENTIVE.
 *
 */

#ifndef LOADABLE
# include "chu.h"
#else
# ifndef NCHU
#  define NCHU 3
#  define KERNEL
# endif
#endif

#if NCHU > 0

/*
 * Number of microseconds we allow between
 * character arrivals.  The speed is 300 baud
 * so this should be somewhat more than 30 msec
 */
#define	CHUMAXUSEC	(60*1000)	/* 60 msec */

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/user.h>

#include <sys/chudefs.h>

static struct module_info rminfo = { 0, "chu", 0, INFPSZ, 0, 0 };
static struct module_info wminfo = { 0, "chu", 0, INFPSZ, 0, 0 };
static int chuopen(), churput(), chuwput(), chuclose();

static struct qinit rinit = { churput, NULL, chuopen, chuclose, NULL,
	&rminfo, NULL };

static struct qinit winit = { chuwput, NULL, NULL, NULL, NULL,
	&wminfo, NULL };

struct streamtab chuinfo = { &rinit, &winit, NULL, NULL };

/*
 * Here's our private data type and structs
 */
struct priv_data 
{
  char in_use;
  struct chucode chu_struct;
} our_priv_data[NCHU];

#ifdef LOADABLE

#ifdef sun
#include <sys/conf.h>
#include <sys/buf.h>
#include <sundev/mbvar.h>
#include <sun/autoconf.h>
#include <sun/vddrv.h>

static struct vdldrv vd =
{
    VDMAGIC_PSEUDO,
    "chu",
    NULL, NULL, NULL, 0, 0, NULL, NULL, 0, 0,
};

static struct fmodsw *chu_fmod;

/*ARGSUSED*/
chuinit (fc, vdp, vdi, vds)
    unsigned int fc;
    struct vddrv *vdp;
    addr_t vdi;
    struct vdstat *vds;
{
    switch (fc) {
    case VDLOAD:
        {
            int dev, i;

            /* Find free entry in fmodsw */
            for (dev = 0; dev < fmodcnt; dev++) {
                if (fmodsw[dev].f_str == NULL)
                    break;
            }
            if (dev == fmodcnt)
                return (ENODEV);
            chu_fmod = &fmodsw[dev];

	    /* If you think a kernel would have strcpy() you're mistaken. */
            for (i = 0; i <= FMNAMESZ; i++)
                chu_fmod->f_name[i] = wminfo.mi_idname[i];

            chu_fmod->f_str = &chuinfo;
        }
        vdp->vdd_vdtab = (struct vdlinkage *) & vd;

	{
	    int i;

	    for (i=0; i<NCHU; i++)
	        our_priv_data[i].in_use=0;
	}

        return 0;
    case VDUNLOAD:
        {
            int dev;

            for (dev = 0; dev < NCHU; dev++)
                if (our_priv_data[dev].in_use) {
                    /* One of the modules is still open */
                    return (EBUSY);
                }
        }
        chu_fmod->f_name[0] = '\0';
        chu_fmod->f_str = NULL;
        return 0;
    case VDSTAT:
        return 0;
    default:
        return EIO;
    }
}

#endif

#else

char chu_first_open=1;

#endif

/*ARGSUSED*/
static int chuopen(q, dev, flag, sflag)
queue_t *q;
dev_t dev;
int flag;
int sflag;
{
  int i;

#ifndef LOADABLE
  if (chu_first_open)
  {
    chu_first_open=0;

    for(i=0;i<NCHU;i++)
      our_priv_data[i].in_use=0;
  }
#endif

  for(i=0;i<NCHU;i++)
    if (!our_priv_data[i].in_use)
    {
      ((struct priv_data *) (q->q_ptr))=&(our_priv_data[i]);
      our_priv_data[i].in_use++;
      our_priv_data[i].chu_struct.ncodechars = 0;
      return 0;
    }

  u.u_error = EBUSY;
  return (OPENFAIL);

}

/*ARGSUSED*/
static int chuclose(q, flag)
queue_t *q;
int flag;
{
  ((struct priv_data *) (q->q_ptr))->in_use=0;

  return (0);
}

/*
 * Now the crux of the biscuit.
 *
 * We will be passed data from the man downstairs. If it's not a data
 * packet, it must be important, so pass it along unmunged. If, however,
 * it is a data packet, we're gonna do special stuff to it. We're going
 * to pass each character we get to the old line discipline code we
 * include below for just such an occasion. When the old ldisc code
 * gets a full chucode struct, we'll hand it back upstairs.
 *
 * chuinput takes a single character and q (as quickly as possible).
 * passback takes a pointer to a chucode struct and q and sends it upstream.
 */

void chuinput();
void passback();

static int churput(q, mp)
queue_t *q;
mblk_t *mp;
{
  mblk_t *bp;

  switch(mp->b_datap->db_type)
  {
    case M_DATA:
      for(bp=mp; bp!=NULL; bp=bp->b_cont)
      {
	while(bp->b_rptr < bp->b_wptr)
	  chuinput( ((u_char)*(bp->b_rptr++)) , q );
      }
      freemsg(mp);
    break;
    default:
      putnext(q,mp);
    break;
  }

}

/*
 * Writing to a chu device doesn't make sense, but we'll pass them
 * through in case they're important.
 */

static int chuwput(q, mp)
queue_t *q;
mblk_t *mp;
{
  putnext(q,mp);
}

/*
 * Take a pointer to a filled chucode struct and a queue and
 * send the chucode stuff upstream
 */

void passback(outdata,q)
struct chucode *outdata;
queue_t *q;
{
  mblk_t *mp;
  int j;

  mp=(mblk_t*) allocb(sizeof(struct chucode),BPRI_LO);

  if (mp==NULL)
  {
    log(LOG_ERR,"chu: cannot allocate message");
    return;
  }

  for(j=0;j<sizeof(struct chucode); j++)
    *mp->b_wptr++ = *( ((char*)outdata) + j );

  putnext(q,mp);
}

/*
 * This routine was copied nearly verbatim from the old line discipline.
 */
void chuinput(c,q)
register u_char c;
queue_t *q;
{
  register struct chucode *chuc;
  register int i;
  long sec, usec;
  struct timeval tv;

  /*
   * Quick, Batman, get a timestamp! We need to do this
   * right away. The time between the end of the stop bit
   * and this point is critical, and should be as nearly
   * constant and as short as possible. (Un)fortunately,
   * the Sun's clock granularity is so big this isn't a
   * major problem.
   *
   * uniqtime() is totally undocumented, but there you are.
   */
  uniqtime(&tv);

  /*
   * Now, locate the chu struct once so we don't have to do it
   * over and over.
   */
  chuc=&(((struct priv_data *) (q->q_ptr))->chu_struct);

	/*
	 * Compute the difference in this character's time stamp
	 * and the last.  If it exceeds the margin, blow away all
	 * the characters currently in the buffer.
	 */
  i = (int)chuc->ncodechars;
  if (i > 0)
  {
    sec = tv.tv_sec - chuc->codetimes[i-1].tv_sec;
    usec = tv.tv_usec - chuc->codetimes[i-1].tv_usec;
    if (usec < 0)
    {
      sec -= 1;
      usec += 1000000;
    }
    if (sec != 0 || usec > CHUMAXUSEC)
    {
      i = 0;
      chuc->ncodechars = 0;
    }
  }

  /*
   * Store the character.
   */
  chuc->codechars[i] = (u_char)c;
  chuc->codetimes[i] = tv;

  /*
   * Now we perform the 'sixth byte' heuristics.
   *
   * This is a long story.
   *
   * We used to be able to count on the first byte of the code
   * having a '6' in the LSD. This prevented most code framing
   * errors (garbage before the first byte wouldn't typically
   * have a 6 in the LSD). That's no longer the case.
   *
   * We can get around this, however, by noting that the 6th byte
   * must be either equal to or one's complement of the first.
   * If we get a sixth byte that ISN'T like that, then it may
   * well be that the first byte is garbage. The right thing
   * to do is to left-shift the whole buffer one count and
   * continue to wait for the sixth byte.
   */
  if (i == NCHUCHARS/2)
  {
    register u_char temp_byte;

    temp_byte=chuc->codechars[i] ^ chuc->codechars[0];

    if ( (temp_byte) && (temp_byte!=0xff) )
    {
      register int t;
      /*
       * No match. Left-shift the buffer and try again
       */
      for(t=0;t<=NCHUCHARS/2;t++)
      {
	chuc->codechars[t]=chuc->codechars[t+1];
	chuc->codetimes[t]=chuc->codetimes[t+1];
      }

      i--; /* This is because of the ++i immediately following */
    }
  }

  /*
   * We done yet?
   */
  if (++i < NCHUCHARS)
  {
    /*
     * We're not done. Not much to do here. Save the count and wait
     * for another character.
     */
    chuc->ncodechars = (u_char)i;
  }
  else
  {
    /*
     * We are done. Mark this buffer full and pass it along.
     */
    chuc->ncodechars = NCHUCHARS;

    /*
     * Now we have a choice. Either the front half and back half
     * have to match, or be one's complement of each other.
     *
     * So let's try the first byte and see
     */

    if(chuc->codechars[0] == chuc->codechars[NCHUCHARS/2])
    {
      chuc->chutype = CHU_TIME;
      for( i=0; i<(NCHUCHARS/2); i++)
        if (chuc->codechars[i] != chuc->codechars[i+(NCHUCHARS/2)])
        {
          chuc->ncodechars = 0;
          return;
        }
    }
    else
    {
      chuc->chutype = CHU_YEAR;
      for( i=0; i<(NCHUCHARS/2); i++)
        if (((chuc->codechars[i] ^ chuc->codechars[i+(NCHUCHARS/2)]) & 0xff)
	  != 0xff )
        {
          chuc->ncodechars = 0;
          return;
        }
    }

    passback(chuc,q); /* We're done! */
    chuc->ncodechars = 0; /* Start all over again! */
  }
}

#endif

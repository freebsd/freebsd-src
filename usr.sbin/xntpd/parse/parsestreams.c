/*
 * /src/NTP/REPOSITORY/v3/parse/parsestreams.c,v 3.19 1994/02/24 16:33:54 kardel Exp
 *  
 * parsestreams.c,v 3.19 1994/02/24 16:33:54 kardel Exp
 *
 * STREAMS module for reference clocks
 * (SunOS4.x)
 *
 * Copyright (c) 1989,1990,1991,1992,1993,1994
 * Frank Kardel Friedrich-Alexander Universitaet Erlangen-Nuernberg
 *                                    
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#ifndef lint
static char rcsid[] = "parsestreams.c,v 3.19 1994/02/24 16:33:54 kardel Exp";
#endif

#include "sys/types.h"
#include "sys/conf.h"
#include "sys/buf.h"
#include "sys/param.h"
#include "sys/sysmacros.h"
#include "sys/errno.h"
#include "sys/time.h"
#include "sundev/mbvar.h"
#include "sun/autoconf.h"
#include "sys/stream.h"
#include "sys/stropts.h"
#include "sys/dir.h"
#include "sys/signal.h"
#include "sys/termios.h"
#include "sys/termio.h"
#include "sys/ttold.h"
#include "sys/user.h"
#include "sys/errno.h"
#include "sys/tty.h"
#include "machine/cpu.h"

#ifdef VDDRV
#include "sun/vddrv.h"
#endif

/*
 * no protypes here !
 */
#define P(x) ()

/*
 * use microtime instead of uniqtime if advised to
 */
#ifdef MICROTIME
#define uniqtime microtime
#endif

#define HAVE_NO_NICE		/* for the NTP headerfiles */
#include "ntp_fp.h"
#include "parse.h"
#include "sys/parsestreams.h"

#ifdef VDDRV
static unsigned int parsebusy = 0;

/*--------------- loadable driver section -----------------------------*/

extern struct streamtab parseinfo;

struct vdldrv parsesync_vd = 
{
  VDMAGIC_PSEUDO,		/* nothing like a real driver - a STREAMS module */
  "PARSE        ",		/* name this baby - keep room for revision number */
};

/*
 * strings support usually not in kernel
 */
static int strlen(s)
  register char *s;
{
  register int c;

  c = 0;
  if (s)
    {
      while (*s++)
	{
	  c++;
	}
    }
  return c;
}

static void strncpy(t, s, c)
  register char *t;
  register char *s;
  register int   c;
{
  if (s && t)
    {
      while ((c-- > 0) && (*t++ = *s++))
	;
    }
}

static int strcmp(s, t)
  register char *s;
  register char *t;
{
  register int c = 0;

  if (!s || !t || (s == t))
    {
      return 0;
    }

  while (!(c = *s++ - *t++) && *s && *t)
    /* empty loop */;
  
  return c;
}

static int strncmp(s, t, n)
  register char *s;
  register char *t;
  register int n;
{
  register int c = 0;

  if (!s || !t || (s == t))
    {
      return 0;
    }

  while (n-- && !(c = *s++ - *t++) && *s && *t)
    /* empty loop */;
  
  return c;
}
 
/*
 * driver init routine
 * since no mechanism gets us into and out of the fmodsw, we have to
 * do it ourselves
 */
/*ARGSUSED*/
int xxxinit(fc, vdp, vdi, vds)
  unsigned int fc;
  struct vddrv *vdp;
  addr_t vdi;
  struct vdstat *vds;
{
  extern struct fmodsw fmodsw[];
  extern int fmodcnt;
  
  struct fmodsw *fm    = fmodsw;
  struct fmodsw *fmend = &fmodsw[fmodcnt];
  struct fmodsw *ifm   = (struct fmodsw *)0;
  char *mname          = parseinfo.st_rdinit->qi_minfo->mi_idname;
  
  switch (fc)
    {
    case VDLOAD:
      vdp->vdd_vdtab = (struct vdlinkage *)&parsesync_vd;
      /*
       * now, jog along fmodsw scanning for an empty slot
       * and deposit our name there
       */
      while (fm <= fmend)
	{
	  if (!strncmp(fm->f_name, mname, FMNAMESZ))
	    {
	      printf("vddrinit[%s]: STREAMS module already loaded.\n", mname);
	      return(EBUSY);
	    }
	  else
	    if ((ifm == (struct fmodsw *)0) && 
                (fm->f_name[0] == '\0') && (fm->f_str == (struct streamtab *)0))
	      {
		/*
		 * got one - so move in
		 */
		ifm = fm;
		break;
	      }
	  fm++;
	}

      if (ifm == (struct fmodsw *)0)
	{
	  printf("vddrinit[%s]: no slot free for STREAMS module\n", mname);
	  return (ENOSPC);
	}
      else
        {
	  static char revision[] = "3.19";
	  char *s, *S, *t;
	  
	  strncpy(ifm->f_name, mname, FMNAMESZ);
	  ifm->f_name[FMNAMESZ] = '\0';
	  ifm->f_str = &parseinfo;
	  /*
	   * copy RCS revision into Drv_name
	   *
	   * are we forcing RCS here to do things it was not built for ?
	   */
	  s = revision;
	  if (*s == '$')
	    {
	      /*
	       * skip "$Revision: "
	       * if present. - not necessary on a -kv co (cvs export)
	       */
	      while (*s && (*s != ' '))
		{
		  s++;
		}
	      if (*s == ' ') s++;
	    }
	  
	  t = parsesync_vd.Drv_name; 
	  while (*t && (*t != ' '))
	    {
	      t++;
	    }
	  if (*t == ' ') t++;
	  
	  S = s;
	  while (*S && (((*S >= '0') && (*S <= '9')) || (*S == '.')))
	    {
	      S++;
	    }
	  
	  if (*s && *t && (S > s))
	    {
	      if (strlen(t) >= (S - s))
		{
		  (void) strncpy(t, s, S - s);
		}
	    }
	  return (0);
        } 
      break;
      
    case VDUNLOAD:
      if (parsebusy > 0)
	{
	  printf("vddrinit[%s]: STREAMS module has still %d instances active.\n", mname, parsebusy);
	  return (EBUSY);
	}
      else
	{
	  while (fm <= fmend)
	    {
	      if (!strncmp(fm->f_name, mname, FMNAMESZ))
		{
		  /*
		   * got it - kill entry
		   */
		  fm->f_name[0] = '\0';
		  fm->f_str = (struct streamtab *)0;
		  fm++;
		  
		  break;
		}
	      fm++;
	    }
	  if (fm > fmend)
	    {
	      printf("vddrinit[%s]: cannot find entry for STREAMS module\n", mname);
	      return (ENXIO);
	    }
	  else
	    return (0);
	}
      

    case VDSTAT:
      return (0);

    default:
      return (EIO);
      
    }
  return EIO;
}

#endif

/*--------------- stream module definition ----------------------------*/

static int parseopen(), parseclose(), parsewput(), parserput(), parsersvc();

static struct module_info driverinfo =
{
  0,				/* module ID number */
  "parse",			/* module name */
  0,				/* minimum accepted packet size */
  INFPSZ,			/* maximum accepted packet size */
  1,				/* high water mark - flow control */
  0				/* low water mark - flow control */
};

static struct qinit rinit =	/* read queue definition */
{
  parserput,			/* put procedure */
  parsersvc,			/* service procedure */
  parseopen,			/* open procedure */
  parseclose,			/* close procedure */
  NULL,				/* admin procedure - NOT USED FOR NOW */
  &driverinfo,			/* information structure */
  NULL				/* statistics */
};

static struct qinit winit =	/* write queue definition */
{
  parsewput,			/* put procedure */
  NULL,				/* service procedure */
  NULL,				/* open procedure */
  NULL,				/* close procedure */
  NULL,				/* admin procedure - NOT USED FOR NOW */
  &driverinfo,			/* information structure */
  NULL				/* statistics */
};

struct streamtab parseinfo =	/* stream info element for dpr driver */
{
  &rinit,			/* read queue */
  &winit,			/* write queue */
  NULL,				/* read mux */
  NULL,				/* write mux */
  NULL				/* module auto push */
};

/*--------------- driver data structures ----------------------------*/

/*
 * we usually have an inverted signal - but you
 * can change this to suit your needs
 */
int cd_invert = 1;		/* invert status of CD line - PPS support via CD input */

int parsedebug = ~0;

extern void uniqtime();

/*--------------- module implementation -----------------------------*/

#define TIMEVAL_USADD(_X_, _US_) {\
				    (_X_)->tv_usec += (_US_);\
			            if ((_X_)->tv_usec >= 1000000)\
				      {\
					 (_X_)->tv_sec++;\
					 (_X_)->tv_usec -= 1000000;\
				      }\
				 } while (0)

#if defined(sun4c) && defined(DEBUG_CD)
#include <sun4c/cpu.h>
#include <sun4c/auxio.h>
#define SET_LED(_X_) (((cpu & CPU_ARCH) == SUN4C_ARCH) ? *(u_char *)AUXIO_REG = AUX_MBO|AUX_EJECT|((_X_)?AUX_LED:0) : 0)
#else
#define SET_LED(_X_)
#endif

static int init_linemon();
static void close_linemon();

/*
 * keep here MACHINE AND OS AND ENVIRONMENT DEPENDENT
 * timing constants
 *
 * FOR ABSOLUTE PRECISION YOU NEED TO MEASURE THE TIMING
 * SKEW BETWEEN THE HW-PPS SIGNAL AND KERNEL uniqtime()
 * YOURSELF.
 *
 * YOU MUST BE QUALIFIED APPROPRIATELY FOR THESE TYPE
 * OF HW MANIPULATION !
 *
 * you need an oscilloscope and the permission for HW work
 * in order to figure out these timing constants/variables
 */
#ifdef sun
static unsigned long xsdelay = 10;   /* assume an SS2 */
static unsigned long stdelay = 350;

struct delays
{
  unsigned char mask;		/* what to check for */
  unsigned char type;		/* what to match */
  unsigned long xsdelay;	/* external status direct delay in us */
  unsigned long stdelay;	/* STREAMS message delay (M_[UN]HANGUP) */
} isr_delays[] = 
{
  /*
   * WARNING: must still be measured - currently taken from Craig Leres ppsdev
   */
#ifdef sun4c
  {CPU_ARCH|CPU_MACH, CPU_SUN4C_50, 10, 350},
  {CPU_ARCH|CPU_MACH, CPU_SUN4C_65, 15, 700},
  {CPU_ARCH|CPU_MACH, CPU_SUN4C_75, 10, 350},
#endif
#ifdef sun4m
  {CPU_ARCH|CPU_MACH, CPU_SUN4M_50,  8, 250},
  {CPU_ARCH|CPU_MACH, CPU_SUN4M_690, 8, 250},
#endif
  {0,}
};

void setup_delays()
{
  register int i;

  for (i = 0; isr_delays[i].mask; i++)
    {
      if ((cpu & isr_delays[i].mask) == isr_delays[i].type)
	{
	  xsdelay = isr_delays[i].xsdelay;
	  stdelay = isr_delays[i].stdelay;
	  return;
	}
    }
  printf("parse: WARNING: PPS kernel fudge factors unknown for this machine (Type 0x%x) - assuming SS2 (Sun4/75)\n", cpu);
}
#else
#define setup_delays()		/* empty - no need for clobbering kernel with this */
static unsigned long xsdelay = 0;   /* assume nothing */
static unsigned long stdelay = 0;
#endif

#define M_PARSE		0x0001
#define M_NOPARSE	0x0002

static int
setup_stream(q, mode)
     queue_t *q;
     int mode;
{
  mblk_t *mp;

  mp = allocb(sizeof(struct stroptions), BPRI_MED);
  if (mp)
    {
      struct stroptions *str = (struct stroptions *)mp->b_rptr;

      str->so_flags   = SO_READOPT|SO_HIWAT|SO_LOWAT;
      str->so_readopt = (mode == M_PARSE) ? RMSGD : RNORM;
      str->so_hiwat   = (mode == M_PARSE) ? sizeof(parsetime_t) : 256;
      str->so_lowat   = 0;
      mp->b_datap->db_type = M_SETOPTS;
      mp->b_wptr += sizeof(struct stroptions);
      putnext(q, mp);
      return putctl1(WR(q)->q_next, M_CTL, (mode == M_PARSE) ? MC_SERVICEIMM :
		     MC_SERVICEDEF);
    }
  else
    {
      parseprintf(DD_OPEN,("parse: setup_stream - FAILED - no MEMORY for allocb\n")); 
      return 0;
    }
}

/*ARGSUSED*/
static int parseopen(q, dev, flag, sflag)
  queue_t *q;
  dev_t dev;
  int flag;
  int sflag;
{
  register mblk_t *mp;
  register parsestream_t *parse;
  static int notice = 0;
  
  parseprintf(DD_OPEN,("parse: OPEN\n")); 
  
  if (sflag != MODOPEN)
    {			/* open only for modules */
      parseprintf(DD_OPEN,("parse: OPEN - FAILED - not MODOPEN\n")); 
      return OPENFAIL;
    }

  if (q->q_ptr != (caddr_t)NULL)
    {
      u.u_error = EBUSY;
      parseprintf(DD_OPEN,("parse: OPEN - FAILED - EXCLUSIVE ONLY\n")); 
      return OPENFAIL;
    }

#ifdef VDDRV
  parsebusy++;
#endif
  
  q->q_ptr = (caddr_t)kmem_alloc(sizeof(parsestream_t));
  WR(q)->q_ptr = q->q_ptr;
  
  parse = (parsestream_t *) q->q_ptr;
  bzero((caddr_t)parse, sizeof(*parse));
  parse->parse_queue     = q;
  parse->parse_status    = PARSE_ENABLE;
  parse->parse_ppsclockev.tv.tv_sec  = 0;
  parse->parse_ppsclockev.tv.tv_usec = 0;
  parse->parse_ppsclockev.serial     = 0;

  if (!parse_ioinit(&parse->parse_io))
    {
      /*
       * ok guys - beat it
       */
      kmem_free((caddr_t)parse, sizeof(parsestream_t));
#ifdef VDDRV
      parsebusy--;
#endif
      return OPENFAIL;
    }

  if (setup_stream(q, M_PARSE))
    {
      (void) init_linemon(q);	/* hook up PPS ISR routines if possible */
      setup_delays();
      parseprintf(DD_OPEN,("parse: OPEN - SUCCEEDED\n")); 

      /*
       * I know that you know the delete key, but you didn't write this
       * code, did you ? - So, keep the message in here.
       */
      if (!notice)
	{
	  printf("%s: Copyright (c) 1991-1994, Frank Kardel\n", parsesync_vd.Drv_name);
	  notice = 1;
	}

      return 1;
    }
  else
    {
      kmem_free((caddr_t)parse, sizeof(parsestream_t));

#ifdef VDDRV
      parsebusy--;
#endif
      return OPENFAIL;
    }
}

/*ARGSUSED*/
static int parseclose(q, flags)
  queue_t *q;
  int flags;
{
  register parsestream_t *parse = (parsestream_t *)q->q_ptr;
  register unsigned long s;
  
  parseprintf(DD_CLOSE,("parse: CLOSE\n"));
  
  s = splhigh();
  
  if (parse->parse_dqueue)
    close_linemon(parse->parse_dqueue, q);
  parse->parse_dqueue = (queue_t *)0;

  (void) splx(s);
      
  parse_ioend(&parse->parse_io);

  kmem_free((caddr_t)parse, sizeof(parsestream_t));

  q->q_ptr = (caddr_t)NULL;
  WR(q)->q_ptr = (caddr_t)NULL;

#ifdef VDDRV
  parsebusy--;
#endif
}

/*
 * move unrecognized stuff upward
 */
static parsersvc(q)
  queue_t *q;
{
  mblk_t *mp;
  
  while (mp = getq(q))
    {
      if (canput(q->q_next) || (mp->b_datap->db_type > QPCTL))
	{
	  putnext(q, mp);
          parseprintf(DD_RSVC,("parse: RSVC - putnext\n"));
	}
      else
	{
	  putbq(q, mp);
          parseprintf(DD_RSVC,("parse: RSVC - flow control wait\n"));
	  break;
	}
    }
}

/*
 * do ioctls and
 * send stuff down - dont care about
 * flow control
 */
static int parsewput(q, mp)
  queue_t *q;
  register mblk_t *mp;
{
  register int ok = 1;
  register mblk_t *datap;
  register struct iocblk *iocp;
  parsestream_t         *parse = (parsestream_t *)q->q_ptr;
  
  parseprintf(DD_WPUT,("parse: parsewput\n"));
  
  switch (mp->b_datap->db_type)
    {
    default:
      putnext(q, mp);
      break;
      
    case M_IOCTL:
      iocp = (struct iocblk *)mp->b_rptr;
      switch (iocp->ioc_cmd)
	{
	default:
	  parseprintf(DD_WPUT,("parse: parsewput - forward M_IOCTL\n"));
	  putnext(q, mp);
	  break;

	case CIOGETEV:
	  /*
	   * taken from Craig Leres ppsclock module (and modified)
	   */
	  datap = allocb(sizeof(struct ppsclockev), BPRI_MED);
	  if (datap == NULL || mp->b_cont)
	    {
	      mp->b_datap->db_type = M_IOCNAK;
	      iocp->ioc_error = (datap == NULL) ? ENOMEM : EINVAL;
	      if (datap != NULL)
		freeb(datap);
	      qreply(q, mp);
	      break;
	    }

	  mp->b_cont = datap;
	  *(struct ppsclockev *)datap->b_wptr = parse->parse_ppsclockev;
	  datap->b_wptr +=
	    sizeof(struct ppsclockev) / sizeof(*datap->b_wptr);
	  mp->b_datap->db_type = M_IOCACK;
	  iocp->ioc_count = sizeof(struct ppsclockev);
	  qreply(q, mp);
	  break;
	  
	case PARSEIOC_ENABLE:
	case PARSEIOC_DISABLE:
	  {
	    parse->parse_status = (parse->parse_status & ~PARSE_ENABLE) |
	                          (iocp->ioc_cmd == PARSEIOC_ENABLE) ?
				    PARSE_ENABLE : 0;
	    if (!setup_stream(RD(q), (parse->parse_status & PARSE_ENABLE) ?
			      M_PARSE : M_NOPARSE))
	      {
		mp->b_datap->db_type = M_IOCNAK;
	      }
	    else
	      {
		mp->b_datap->db_type = M_IOCACK;
	      }
	    qreply(q, mp);
	    break;
	  }	    

	case PARSEIOC_SETSTAT:
	case PARSEIOC_GETSTAT:
	case PARSEIOC_TIMECODE:
	case PARSEIOC_SETFMT:
	case PARSEIOC_GETFMT:
	case PARSEIOC_SETCS:
          if (iocp->ioc_count == sizeof(parsectl_t))
	    {
	      parsectl_t *dct = (parsectl_t *)mp->b_cont->b_rptr;

	      switch (iocp->ioc_cmd)
		{
		case PARSEIOC_GETSTAT:
		  parseprintf(DD_WPUT,("parse: parsewput - PARSEIOC_GETSTAT\n"));
		  ok = parse_getstat(dct, &parse->parse_io);
		  break;
		  
		case PARSEIOC_SETSTAT:
		  parseprintf(DD_WPUT,("parse: parsewput - PARSEIOC_SETSTAT\n"));
		  ok = parse_setstat(dct, &parse->parse_io);
		  break;
		  
		case PARSEIOC_TIMECODE:
		  parseprintf(DD_WPUT,("parse: parsewput - PARSEIOC_TIMECODE\n"));
		  ok = parse_timecode(dct, &parse->parse_io);
		  break;
		  
		case PARSEIOC_SETFMT:
		  parseprintf(DD_WPUT,("parse: parsewput - PARSEIOC_SETFMT\n"));
		  ok = parse_setfmt(dct, &parse->parse_io);
		  break;

		case PARSEIOC_GETFMT:
		  parseprintf(DD_WPUT,("parse: parsewput - PARSEIOC_GETFMT\n"));
		  ok = parse_getfmt(dct, &parse->parse_io);
		  break;

		case PARSEIOC_SETCS:
		  parseprintf(DD_WPUT,("parse: parsewput - PARSEIOC_SETCS\n"));
		  ok = parse_setcs(dct, &parse->parse_io);
		  break;
		}
	      mp->b_datap->db_type = ok ? M_IOCACK : M_IOCNAK;
	    }
	  else
	    {
	      mp->b_datap->db_type = M_IOCNAK;
	    }
	  parseprintf(DD_WPUT,("parse: parsewput qreply - %s\n", (mp->b_datap->db_type == M_IOCNAK) ? "M_IOCNAK" : "M_IOCACK"));
	  qreply(q, mp);
	  break;
	}
    }
}

/*
 * read characters from streams buffers
 */
static unsigned long rdchar(mp)
  register mblk_t **mp;
{
  while (*mp != (mblk_t *)NULL)
    {
      if ((*mp)->b_wptr - (*mp)->b_rptr)
	{
	  return (unsigned long)(*(unsigned char *)((*mp)->b_rptr++));
	}
      else
	{
	  register mblk_t *mmp = *mp;
	  
	  *mp = (*mp)->b_cont;
	  freeb(mmp);
	}
    }
  return ~0;
}

/*
 * convert incoming data
 */
static int parserput(q, mp)
  queue_t *q;
  mblk_t *mp;
{
  unsigned char type;
  
  switch (type = mp->b_datap->db_type)
    {
     default:
      /*
       * anything we don't know will be put on queue
       * the service routine will move it to the next one
       */
      parseprintf(DD_RPUT,("parse: parserput - forward type 0x%x\n", type));
      if (canput(q->q_next) || (mp->b_datap->db_type > QPCTL))
	{
	  putnext(q, mp);
	}
      else
	putq(q, mp);
      break;
      
     case M_BREAK:
     case M_DATA:
      {
	register parsestream_t * parse = (parsestream_t *)q->q_ptr;
	register mblk_t *nmp;
	register unsigned long ch;
	timestamp_t ctime;

	/*
	 * get time on packet delivery
	 */
	uniqtime(&ctime.tv);

	if (!(parse->parse_status & PARSE_ENABLE))
	  {
	    parseprintf(DD_RPUT,("parse: parserput - parser disabled - forward type 0x%x\n", type));
	    if (canput(q->q_next) || (mp->b_datap->db_type > QPCTL))
	      {
		putnext(q, mp);
	      }
	    else
	      putq(q, mp);
	  }
	else
	  {
	    parseprintf(DD_RPUT,("parse: parserput - M_%s\n", (type == M_DATA) ? "DATA" : "BREAK"));

	    if (type == M_DATA)
	      {
		/*
		 * parse packet looking for start an end characters
		 */
		while (mp != (mblk_t *)NULL)
		  {
		    ch = rdchar(&mp);
		    if (ch != ~0 && parse_ioread(&parse->parse_io, (char)ch, &ctime))
		      {
			/*
			 * up up and away (hopefully ...)
			 * don't press it if resources are tight or nobody wants it
			 */
			nmp = (mblk_t *)NULL;
			if (canput(parse->parse_queue->q_next) && (nmp = allocb(sizeof(parsetime_t), BPRI_MED)))
			  {
			    bcopy((caddr_t)&parse->parse_io.parse_dtime, (caddr_t)nmp->b_rptr, sizeof(parsetime_t));
			    nmp->b_wptr += sizeof(parsetime_t);
			    putnext(parse->parse_queue, nmp);
			  }
			else
			  if (nmp) freemsg(nmp);
			parse_iodone(&parse->parse_io);
		      }
		  }	
	      }
	    else
	      {
		if (parse_ioread(&parse->parse_io, (char)0, &ctime))
		  {
		    /*
		     * up up and away (hopefully ...)
		     * don't press it if resources are tight or nobody wants it
		     */
		    nmp = (mblk_t *)NULL;
		    if (canput(parse->parse_queue->q_next) && (nmp = allocb(sizeof(parsetime_t), BPRI_MED)))
		      {
			bcopy((caddr_t)&parse->parse_io.parse_dtime, (caddr_t)nmp->b_rptr, sizeof(parsetime_t));
			nmp->b_wptr += sizeof(parsetime_t);
			putnext(parse->parse_queue, nmp);
		      }
		    else
		      if (nmp) freemsg(nmp);
		    parse_iodone(&parse->parse_io);
		  }
		freemsg(mp);
	      }
	    break;
	  }
      }

      /*
       * CD PPS support for non direct ISR hack
       */
    case M_HANGUP:
    case M_UNHANGUP:
      {
	register parsestream_t * parse = (parsestream_t *)q->q_ptr;
	timestamp_t ctime;
	register mblk_t *nmp;
	register int status = cd_invert ^ (type == M_HANGUP);

	SET_LED(status);
	
	uniqtime(&ctime.tv);
	
	TIMEVAL_USADD(&ctime.tv, stdelay);
	
	parseprintf(DD_RPUT,("parse: parserput - M_%sHANGUP\n", (type == M_HANGUP) ? "" : "UN"));

	if ((parse->parse_status & PARSE_ENABLE) &&
	    parse_iopps(&parse->parse_io, status ? SYNC_ONE : SYNC_ZERO, &ctime))
	  {
	    nmp = (mblk_t *)NULL;
	    if (canput(parse->parse_queue->q_next) && (nmp = allocb(sizeof(parsetime_t), BPRI_MED)))
	      {
		bcopy((caddr_t)&parse->parse_io.parse_dtime, (caddr_t)nmp->b_rptr, sizeof(parsetime_t));
		nmp->b_wptr += sizeof(parsetime_t);
		putnext(parse->parse_queue, nmp);
	      }
	    else
	      if (nmp) freemsg(nmp);
	    parse_iodone(&parse->parse_io);
	    freemsg(mp);
	  }
	else
	  if (canput(q->q_next) || (mp->b_datap->db_type > QPCTL))
	    {
	      putnext(q, mp);
	    }
	  else
	    putq(q, mp);
	
	if (status)
	  {
	    parse->parse_ppsclockev.tv = ctime.tv;
	    ++(parse->parse_ppsclockev.serial);
	  }
      }
    }
}

static int  init_zs_linemon();	/* handle line monitor for "zs" driver */
static void close_zs_linemon();
static void zs_xsisr();		/* zs external status interupt handler */

/*-------------------- CD isr status monitor ---------------*/

static int init_linemon(q)
  register queue_t *q;
{
  register queue_t *dq;
  
  dq = WR(q);
  /*
   * we ARE doing very bad things down here (basically stealing ISR
   * hooks)
   *
   * so we chase down the STREAMS stack searching for the driver
   * and if this is a known driver we insert our ISR routine for
   * status changes in to the ExternalStatus handling hook
   */
  while (dq->q_next)
    {
      dq = dq->q_next;		/* skip down to driver */
    }

  /*
   * find appropriate driver dependent routine
   */
  if (dq->q_qinfo && dq->q_qinfo->qi_minfo)
    {
      register char *dname = dq->q_qinfo->qi_minfo->mi_idname;

      parseprintf(DD_INSTALL, ("init_linemon: driver is \"%s\"\n", dname));

#ifdef sun
      if (dname && !strcmp(dname, "zs"))
	{
	  return init_zs_linemon(dq, q);
	}
      else
#endif
	{
	  parseprintf(DD_INSTALL, ("init_linemon: driver \"%s\" not suitable for CD monitoring\n", dname));
	  return 0;
	}
    }
  parseprintf(DD_INSTALL, ("init_linemon: cannot find driver\n"));
  return 0;
}

static void close_linemon(q, my_q)
  register queue_t *q;
  register queue_t *my_q;
{
  /*
   * find appropriate driver dependent routine
   */
  if (q->q_qinfo && q->q_qinfo->qi_minfo)
    {
      register char *dname = q->q_qinfo->qi_minfo->mi_idname;

#ifdef sun
      if (dname && !strcmp(dname, "zs"))
	{
	  close_zs_linemon(q, my_q);
	  return;
	}
  parseprintf(DD_INSTALL, ("close_linemon: cannot find driver close routine for \"%s\"\n", dname));
#endif
    }
  parseprintf(DD_INSTALL, ("close_linemon: cannot find driver name\n"));
}

#ifdef sun
#include <sundev/zsreg.h>
#include <sundev/zscom.h>
#include <sundev/zsvar.h>

struct savedzsops
{
  struct zsops  zsops;
  struct zsops *oldzsops;
};

struct zsops   *emergencyzs;

static int init_zs_linemon(q, my_q)
  register queue_t *q;
  register queue_t *my_q;
{
  register struct zscom *zs;
  register struct savedzsops *szs;
  register parsestream_t  *parsestream = (parsestream_t *)my_q->q_ptr;
  /*
   * we expect the zsaline pointer in the q_data pointer
   * from there on we insert our on EXTERNAL/STATUS ISR routine
   * into the interrupt path, before the standard handler
   */
  zs = ((struct zsaline *)q->q_ptr)->za_common;
  if (!zs)
    {
      /*
       * well - not found on startup - just say no (shouldn't happen though)
       */
      return 0;
    }
  else
    {
      unsigned long s;
      
      /*
       * we do a direct replacement, in case others fiddle also
       * if somebody else grabs our hook and we disconnect
       * we are in DEEP trouble - panic is likely to be next, sorry
       */
      szs = (struct savedzsops *) kmem_alloc(sizeof(struct savedzsops));

      parsestream->parse_data   = (void *)szs;

      s = splhigh();

      parsestream->parse_dqueue = q; /* remember driver */

      szs->zsops            = *zs->zs_ops;
      szs->zsops.zsop_xsint = (int (*)())zs_xsisr; /* place our bastard */
      szs->oldzsops         = zs->zs_ops;
      emergencyzs           = zs->zs_ops;
      
      zsopinit(zs, &szs->zsops); /* hook it up */
      
      (void) splx(s);

      parseprintf(DD_INSTALL, ("init_zs_linemon: CD monitor installed\n"));

      return 1;
    }
}

/*
 * unregister our ISR routine - must call under splhigh()
 */
static void close_zs_linemon(q, my_q)
  register queue_t *q;
  register queue_t *my_q;
{
  register struct zscom *zs;
  register parsestream_t  *parsestream = (parsestream_t *)my_q->q_ptr;

  zs = ((struct zsaline *)q->q_ptr)->za_common;
  if (!zs)
    {
      /*
       * well - not found on startup - just say no (shouldn't happen though)
       */
      return;
    }
  else
    {
      register struct savedzsops *szs = (struct savedzsops *)parsestream->parse_data;
      
      zsopinit(zs, szs->oldzsops); /* reset to previous handler functions */

      kmem_free((caddr_t)szs, sizeof (struct savedzsops));
      
      parseprintf(DD_INSTALL, ("close_zs_linemon: CD monitor deleted\n"));
      return;
    }
}

#define MAXDEPTH 50		/* maximum allowed stream crawl */

#ifdef PPS_SYNC
extern hardpps();
extern struct timeval time;
#endif

/*
 * take external status interrupt (only CD interests us)
 */
static void zs_xsisr(zs)
  register struct zscom *zs;
{
  register struct zsaline *za = (struct zsaline *)zs->zs_priv;
  register struct zscc_device *zsaddr = zs->zs_addr;
  register queue_t *q;
  register unsigned char zsstatus;
  register int loopcheck;
  register char *dname;
#ifdef PPS_SYNC
  register int s;
  register long usec;
#endif

  /*
   * pick up current state
   */
  zsstatus = zsaddr->zscc_control;

  if ((za->za_rr0 ^ zsstatus) & (ZSRR0_CD|ZSRR0_SYNC))
    {
      timestamp_t cdevent;
      register int status;
      
      /*
       * CONDITIONAL external measurement support
       */
      SET_LED(zsstatus & (ZSRR0_CD|ZSRR0_SYNC));		/*
				 * inconsistent with upper SET_LED, but this
				 * is for oscilloscope business anyway and we
				 * are just interested in edge delays in the
				 * lower us range
				 */
#ifdef PPS_SYNC
      s = splclock();
      usec = time.tv_usec;
#endif
      /*
       * time stamp
       */
      uniqtime(&cdevent.tv);
      
#ifdef PPS_SYNC
      splx(s);
#endif

      /*
       * logical state
       */
      status = cd_invert ? (zsstatus & (ZSRR0_CD|ZSRR0_SYNC)) == 0 : (zsstatus & (ZSRR0_CD|ZSRR0_SYNC)) != 0;

#ifdef PPS_SYNC
      if (status)
	{
	  usec = cdevent.tv.tv_usec - usec;
	  if (usec < 0)
	    usec += 1000000;

	  hardpps(&cdevent.tv, usec);
        }
#endif

      TIMEVAL_USADD(&cdevent.tv, xsdelay);
	
      q = za->za_ttycommon.t_readq;

      /*
       * ok - now the hard part - find ourself
       */
      loopcheck = MAXDEPTH;
      
      while (q)
	{
	  if (q->q_qinfo && q->q_qinfo->qi_minfo)
	    {
	      dname = q->q_qinfo->qi_minfo->mi_idname;

	      if (!strcmp(dname, parseinfo.st_rdinit->qi_minfo->mi_idname))
		{
		  /*
		   * back home - phew (hopping along stream queues might
		   * prove dangerous to your health)
		   */

		  if ((((parsestream_t *)q->q_ptr)->parse_status & PARSE_ENABLE) &&
		      parse_iopps(&((parsestream_t *)q->q_ptr)->parse_io, status ? SYNC_ONE : SYNC_ZERO, &cdevent))
		    {
		      /*
		       * XXX - currently we do not pass up the message, as
		       * we should.
		       * for a correct behaviour wee need to block out
		       * processing until parse_iodone has been posted via
		       * a softcall-ed routine which does the message pass-up
		       * right now PPS information relies on input being
		       * received
		       */
		      parse_iodone(&((parsestream_t *)q->q_ptr)->parse_io);
		    }
		  
		  if (status)
		    {
		      ((parsestream_t *)q->q_ptr)->parse_ppsclockev.tv = cdevent.tv;
		      ++(((parsestream_t *)q->q_ptr)->parse_ppsclockev.serial);
		    }

		  parseprintf(DD_ISR, ("zs_xsisr: CD event %s has been posted for \"%s\"\n", status ? "ONE" : "ZERO", dname));
		  break;
		}
	    }

	  q = q->q_next;

	  if (!loopcheck--)
	    {
	      panic("zs_xsisr: STREAMS Queue corrupted - CD event");
	    }
	}

      /*
       * only pretend that CD has been handled
       */
      za->za_rr0 = za->za_rr0 & ~(ZSRR0_CD|ZSRR0_SYNC) | zsstatus & (ZSRR0_CD|ZSRR0_SYNC);
      ZSDELAY(2);

      if (!((za->za_rr0 ^ zsstatus) & ~(ZSRR0_CD|ZSRR0_SYNC)))
	{
	  /*
	   * all done - kill status indication and return
	   */
	  zsaddr->zscc_control = ZSWR0_RESET_STATUS; /* might kill other conditions here */
	  return;
	}
    }      

  /*
   * we are now gathered here to process some unusual external status
   * interrupts.
   * any CD events have also been handled and shouldn't be processed
   * by the original routine (unless we have a VERY busy port pin)
   * some initializations are done here, which could have been done before for
   * both code paths but have been avioded for minimum path length to
   * the uniq_time routine
   */
  dname = (char *) 0;
  q = za->za_ttycommon.t_readq;

  loopcheck = MAXDEPTH;
      
  /*
   * the real thing for everything else ...
   */
  while (q)
    {
      if (q->q_qinfo && q->q_qinfo->qi_minfo)
	{
	  dname = q->q_qinfo->qi_minfo->mi_idname;
	  if (!strcmp(dname, parseinfo.st_rdinit->qi_minfo->mi_idname))
	    {
	      register int (*zsisr)();
		  
	      /*
	       * back home - phew (hopping along stream queues might
	       * prove dangerous to your health)
	       */
	      if (zsisr = ((struct savedzsops *)((parsestream_t *)q->q_ptr)->parse_data)->oldzsops->zsop_xsint)
		(void)zsisr(zs);
	      else
		panic("zs_xsisr: unable to locate original ISR");
		  
	      parseprintf(DD_ISR, ("zs_xsisr: non CD event was processed for \"%s\"\n", dname));
	      /*
	       * now back to our program ...
	       */
	      return;
	    }
	}

      q = q->q_next;

      if (!loopcheck--)
	{
	  panic("zs_xsisr: STREAMS Queue corrupted - non CD event");
	}
    }

  /*
   * last resort - shouldn't even come here as it indicates
   * corrupted TTY structures
   */
  printf("zs_zsisr: looking for \"%s\" - found \"%s\" - taking EMERGENCY path\n", parseinfo.st_rdinit->qi_minfo->mi_idname, dname ? dname : "-NIL-");
      
  if (emergencyzs && emergencyzs->zsop_xsint)
    emergencyzs->zsop_xsint(zs);
  else
    panic("zs_xsisr: no emergency ISR handler");
}
#endif				/* sun */

/*
 * History:
 *
 * parsestreams.c,v
 * Revision 3.19  1994/02/24  16:33:54  kardel
 * CD events can also be posted on sync flag
 *
 * Revision 3.18  1994/02/24  14:12:58  kardel
 * initial PPS_SYNC support version
 *
 * Revision 3.17  1994/02/20  15:18:02  kardel
 * rcs id cleanup
 *
 * Revision 3.16  1994/02/15  22:39:50  kardel
 * memory leak on open failure closed
 *
 * Revision 3.15  1994/02/13  19:16:50  kardel
 * updated verbose Copyright message
 *
 * Revision 3.14  1994/02/02  17:45:38  kardel
 * rcs ids fixed
 *
 * Revision 3.12  1994/01/25  19:05:30  kardel
 * 94/01/23 reconcilation
 *
 * Revision 3.11  1994/01/23  17:22:07  kardel
 * 1994 reconcilation
 *
 * Revision 3.10  1993/12/15  12:48:58  kardel
 * fixed message loss on M_*HANHUP messages
 *
 * Revision 3.9  1993/11/05  15:34:55  kardel
 * shut up nice feature detection
 *
 * Revision 3.8  1993/10/22  14:27:56  kardel
 * Oct. 22nd 1993 reconcilation
 *
 * Revision 3.7  1993/10/10  18:13:53  kardel
 * Makefile reorganisation, file relocation
 *
 * Revision 3.6  1993/10/09  15:01:18  kardel
 * file structure unified
 *
 * Revision 3.5  1993/10/04  07:59:31  kardel
 * Well, at least we should know that a the tv_usec field should be in the range 0..999999
 *
 * Revision 3.4  1993/09/26  23:41:33  kardel
 * new parse driver logic
 *
 * Revision 3.3  1993/09/11  00:38:34  kardel
 * LINEMON must also cover M_[UN]HANGUP handling
 *
 * Revision 3.2  1993/07/06  10:02:56  kardel
 * DCF77 driver goes generic...
 *
 */

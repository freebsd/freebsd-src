/*
 * /src/NTP/REPOSITORY/v3/parse/parsesolaris.c,v 3.4 1993/11/13 11:13:17 kardel Exp
 *  
 * parsesolaris.c,v 3.4 1993/11/13 11:13:17 kardel Exp
 *
 * STREAMS module for reference clocks
 * (SunOS5.x - not fully tested - buyer beware ! - OS KILLERS may still be
 *  lurking in the code!)
 *
 * Copyright (c) 1993
 * derived work from parsestreams.c ((c) 1991-1993, Frank Kardel) and
 * dcf77sync.c((c) Frank Kardel)
 * Frank Kardel, Friedrich-Alexander Universitaet Erlangen-Nuernberg
 *                                    
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#ifndef lint
static char rcsid[] = "parsesolaris.c,v 3.4 1993/11/13 11:13:17 kardel Exp";
#endif

/*
 * Well, the man spec says we have to do this junk - the
 * header files tell a different story (i like that one more)
 */
#define SAFE_WR(q) (((q)->q_flag & QREADR) ? WR((q)) : (q))
#define SAFE_RD(q) (((q)->q_flag & QREADR) ? (q) : RD((q)))

/*
 * needed to cope with Solaris 2.3 header file chaos
 */
#include <sys/types.h>
/*
 * the Solaris 2.2 include list
 */
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/termios.h>
#include <sys/stream.h>
#include <sys/strtty.h>
#include <sys/stropts.h>
#include <sys/modctl.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/cpu.h>

#define STREAM /* that's what we are here for */

#define HAVE_NO_NICE		/* for the NTP headerfiles */
#include "ntp_fp.h"
#include "parse.h"
#include "sys/parsestreams.h"

static unsigned int parsebusy = 0;

/*--------------- loadable driver section -----------------------------*/

static struct streamtab parseinfo;

static struct fmodsw fmod_templ =
{
  "parse",			/* module name */
  &parseinfo,			/* module information */
  0,				/* not clean yet */
  /* lock ptr */
};

extern struct mod_ops mod_strmodops;

static struct modlstrmod modlstrmod = 
{
  &mod_strmodops,		/* a STREAMS module */
  "PARSE      - NTP reference",	/* name this baby - keep room for revision number */
  &fmod_templ
};

static struct modlinkage modlinkage =
{
  MODREV_1,
  &modlstrmod,
  NULL
};

/*
 * strings support usually not in kernel
 */
static int Strlen(s)
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

static void Strncpy(t, s, c)
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

int Strcmp(s, t)
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

/*
 * module management routines
 */
/*ARGSUSED*/
int _init(void)
{
  static char revision[] = "3.4";
  char *s, *S, *t;
  
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
	  
  t = modlstrmod.strmod_linkinfo; 
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
      if (Strlen(t) >= (S - s))
	{
	  (void) Strncpy(t, s, S - s);
	}
    }
  return (mod_install(&modlinkage));
}

/*ARGSUSED*/
int _info(struct modinfo *modinfop)
{
  return (mod_info(&modlinkage, modinfop));
}

/*ARGSUSED*/
int _fini(void)
{
  if (parsebusy > 0)
    {
      printf("_fini[%s]: STREAMS module has still %d instances active.\n", modlstrmod.strmod_linkinfo, parsebusy);
      return (EBUSY);
    }
  else
    return (mod_remove(&modlinkage));
}

/*--------------- stream module definition ----------------------------*/

static int parseopen(), parseclose(), parsewput(), parserput(), parsersvc();

static struct module_info driverinfo =
{
  0,				/* module ID number */
  fmod_templ.f_name,		/* module name - why repeated here ? compat ?*/
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

static struct streamtab parseinfo =	/* stream info element for parse driver */
{
  &rinit,			/* read queue */
  &winit,			/* write queue */
  NULL,				/* read mux */
  NULL				/* write mux */
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

#define TIMEVAL_USADD(_X_, _US_) do {\
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

  if (cputype & OBP_ARCH)
    {
      printf("parse: WARNING: PPS kernel fudge factors no yet determinable (no dev tree walk yet) - assuming SS2 (Sun4/75)\n", cputype);
      return;
    }

  for (i = 0; isr_delays[i].mask; i++)
    {
      if ((cputype & isr_delays[i].mask) == isr_delays[i].type)
	{
	  xsdelay = isr_delays[i].xsdelay;
	  stdelay = isr_delays[i].stdelay;
	  return;
	}
    }
  printf("parse: WARNING: PPS kernel fudge factors unknown for this machine (Type 0x%x) - assuming SS2 (Sun4/75)\n", cputype);
}

#define M_PARSE		0x0001
#define M_NOPARSE	0x0002

static int
setup_stream(queue_t *q, int mode)
{
  register mblk_t *mp;

  parseprintf(DD_OPEN,("parse: SETUP_STREAM - setting up stream for q=%x\n", q));

  mp = allocb(sizeof(struct stroptions), BPRI_MED);
  if (mp)
    {
      struct stroptions *str = (struct stroptions *)mp->b_wptr;

      str->so_flags   = SO_READOPT|SO_HIWAT|SO_LOWAT;
      str->so_readopt = (mode == M_PARSE) ? RMSGD : RNORM;
      str->so_hiwat   = (mode == M_PARSE) ? sizeof(parsetime_t) : 256;
      str->so_lowat   = 0;
      mp->b_datap->db_type = M_SETOPTS;
      mp->b_wptr     += sizeof(struct stroptions);
      if (!q)
	panic("NULL q - strange");
      putnext(q, mp);
      return putctl1(SAFE_WR(q)->q_next, M_CTL, (mode == M_PARSE) ? MC_SERVICEIMM :
		     MC_SERVICEDEF);
    }
  else
    {
      parseprintf(DD_OPEN,("parse: setup_stream - FAILED - no MEMORY for allocb\n")); 
      return 0;
    }
}

/*ARGSUSED*/
static int parseopen(queue_t *q, dev_t *dev, int flag, int sflag, cred_t *credp)
{
  register mblk_t *mp;
  register parsestream_t *parse;
  static int notice = 0;
  
  parseprintf(DD_OPEN,("parse: OPEN - q=%x\n", q)); 
  
  if (sflag != MODOPEN)
    {			/* open only for modules */
      parseprintf(DD_OPEN,("parse: OPEN - FAILED - not MODOPEN\n")); 
      return EIO;
    }

  if (q->q_ptr != (caddr_t)NULL)
    {
      parseprintf(DD_OPEN,("parse: OPEN - FAILED - EXCLUSIVE ONLY\n")); 
      return EBUSY;
    }

  parsebusy++;
  
  q->q_ptr = (caddr_t)kmem_alloc(sizeof(parsestream_t), KM_SLEEP);
  parseprintf(DD_OPEN,("parse: OPEN - parse area q=%x, q->q_ptr=%x\n", q, q->q_ptr)); 
  SAFE_WR(q)->q_ptr = q->q_ptr;
  parseprintf(DD_OPEN,("parse: OPEN - WQ parse area q=%x, q->q_ptr=%x\n", SAFE_WR(q), SAFE_WR(q)->q_ptr)); 
  
  parse = (parsestream_t *) q->q_ptr;
  bzero((caddr_t)parse, sizeof(*parse));
  parse->parse_queue     = q;
  parse->parse_status    = PARSE_ENABLE;
  parse->parse_ppsclockev.tv.tv_sec  = 0;
  parse->parse_ppsclockev.tv.tv_usec = 0;
  parse->parse_ppsclockev.serial     = 0;

  parseprintf(DD_OPEN,("parse: OPEN - initializing io subsystem q=%x\n", q)); 

  if (!parse_ioinit(&parse->parse_io))
    {
      /*
       * ok guys - beat it
       */
      kmem_free((caddr_t)parse, sizeof(parsestream_t));

      parsebusy--;

      return EIO;
    }

  parseprintf(DD_OPEN,("parse: OPEN - initializing stream q=%x\n", q)); 

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
	  printf("%s: Copyright (c) 1991-1993, Frank Kardel\n", modlstrmod.strmod_linkinfo);
	  notice = 1;
	}

      return 0;
    }
  else
    {
      parsebusy--;
      return EIO;
    }
}

/*ARGSUSED*/
static int parseclose(queue_t *q, int flags)
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
  SAFE_WR(q)->q_ptr = (caddr_t)NULL;

  parsebusy--;
}

/*
 * move unrecognized stuff upward
 */
static parsersvc(queue_t *q)
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
static int parsewput(queue_t *q, mblk_t *mp)
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
	    if (!setup_stream(SAFE_RD(q), (parse->parse_status & PARSE_ENABLE) ?
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
static unsigned long rdchar(mblk_t **mp)
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
static int parserput(queue_t *q, mblk_t *imp)
{
  register unsigned char type;
  mblk_t *mp = imp;
  
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
	  }
	
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

static int init_linemon(queue_t *q)
{
  register queue_t *dq;
  
  dq = SAFE_WR(q);
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
      if (dname && !Strcmp(dname, "zs"))
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

static void close_linemon(queue_t *q, queue_t *my_q)
{
  /*
   * find appropriate driver dependent routine
   */
  if (q->q_qinfo && q->q_qinfo->qi_minfo)
    {
      register char *dname = q->q_qinfo->qi_minfo->mi_idname;

#ifdef sun
      if (dname && !Strcmp(dname, "zs"))
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
#include <sys/tty.h>
#include <sys/zsdev.h>
#include <sys/ser_async.h>
#include <sys/ser_zscc.h>

/*
 * there should be some docs telling how to get to
 * sz:zs_usec_delay and zs:initzsops()
 */
#define zs_usec_delay 5

struct savedzsops
{
  struct zsops  zsops;
  struct zsops *oldzsops;
};

static struct zsops   *emergencyzs;

static int init_zs_linemon(queue_t *q, queue_t *my_q)
{
  register struct zscom *zs;
  register struct savedzsops *szs;
  register parsestream_t  *parsestream = (parsestream_t *)my_q->q_ptr;
  /*
   * we expect the zsaline pointer in the q_data pointer
   * from there on we insert our on EXTERNAL/STATUS ISR routine
   * into the interrupt path, before the standard handler
   */
  zs = ((struct asyncline *)q->q_ptr)->za_common;
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
      szs = (struct savedzsops *) kmem_alloc(sizeof(struct savedzsops), KM_SLEEP);

      parsestream->parse_data   = (void *)szs;

      mutex_enter(zs->zs_excl);

      parsestream->parse_dqueue = q; /* remember driver */

      szs->zsops            = *zs->zs_ops;
      szs->zsops.zsop_xsint = (void (*)())zs_xsisr; /* place our bastard */
      szs->oldzsops         = zs->zs_ops;
      emergencyzs           = zs->zs_ops;
      
      zs->zs_ops = &szs->zsops; /* hook it up */
      
      mutex_exit(zs->zs_excl);

      parseprintf(DD_INSTALL, ("init_zs_linemon: CD monitor installed\n"));

      return 1;
    }
}

/*
 * unregister our ISR routine - must call under splhigh()
 */
static void close_zs_linemon(queue_t *q, queue_t *my_q)
{
  register struct zscom *zs;
  register parsestream_t  *parsestream = (parsestream_t *)my_q->q_ptr;

  zs = ((struct asyncline *)q->q_ptr)->za_common;
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

      mutex_enter(zs->zs_excl);

      zs->zs_ops = szs->oldzsops; /* reset to previous handler functions */

      mutex_exit(zs->zs_excl);

      kmem_free((caddr_t)szs, sizeof (struct savedzsops));
      
      parseprintf(DD_INSTALL, ("close_zs_linemon: CD monitor deleted\n"));
      return;
    }
}

#define MAXDEPTH 50		/* maximum allowed stream crawl */

/*
 * take external status interrupt (only CD interests us)
 */
static void zs_xsisr(struct zscom *zs)
{
  register struct asyncline *za = (struct asyncline *)zs->zs_priv;
  register queue_t *q;
  register unsigned char zsstatus;
  register int loopcheck;
  register unsigned char cdstate;
  register char *dname;

  /*
   * pick up current state
   */
  zsstatus = SCC_READ0();

  if (za->za_rr0 ^ (cdstate = zsstatus & ZSRR0_CD))
    {
      timestamp_t cdevent;
      register int status;
      
      /*
       * CONDITIONAL external measurement support
       */
      SET_LED(cdstate);		/*
				 * inconsistent with upper SET_LED, but this
				 * is for oscilloscope business anyway and we
				 * are just interested in edge delays in the
				 * lower us range
				 */
      
      /*
       * time stamp
       */
      uniqtime(&cdevent.tv);

      TIMEVAL_USADD(&cdevent.tv, xsdelay);
	
      q = za->za_ttycommon.t_readq;

      /*
       * logical state
       */
      status = cd_invert ? cdstate == 0 : cdstate != 0;

      /*
       * ok - now the hard part - find ourself
       */
      loopcheck = MAXDEPTH;
      
      while (q)
	{
	  if (q->q_qinfo && q->q_qinfo->qi_minfo)
	    {
	      dname = q->q_qinfo->qi_minfo->mi_idname;

	      if (!Strcmp(dname, parseinfo.st_rdinit->qi_minfo->mi_idname))
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
      za->za_rr0 = za->za_rr0 & ~ZSRR0_CD | zsstatus & ZSRR0_CD;

      if (!((za->za_rr0 ^ zsstatus) & ~ZSRR0_CD))
	{
	  /*
	   * all done - kill status indication and return
	   */
	  SCC_WRITE0(ZSWR0_RESET_STATUS); /* might kill other conditions here */
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
	  if (!Strcmp(dname, parseinfo.st_rdinit->qi_minfo->mi_idname))
	    {
	      register void (*zsisr)();
		  
	      /*
	       * back home - phew (hopping along stream queues might
	       * prove dangerous to your health)
	       */
	      if (zsisr = ((struct savedzsops *)((parsestream_t *)q->q_ptr)->parse_data)->oldzsops->zsop_xsint)
		zsisr(zs);
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
 * parsesolaris.c,v
 * Revision 3.4  1993/11/13  11:13:17  kardel
 * Solaris 2.3 additional includes
 *
 * Revision 3.3  1993/11/11  11:20:33  kardel
 * declaration fixes
 *
 * Revision 3.2  1993/11/05  15:40:25  kardel
 * shut up nice feature detection
 *
 * Revision 3.1  1993/11/01  20:00:29  kardel
 * parse Solaris support (initial version)
 *
 *
 */

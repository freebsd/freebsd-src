/* gsc.c - device driver for handy scanners
 *
 * Current version supports:
 *
 * 	- Genius GS-4500
 *
 * Copyright (c) 1995 Gunther Schadow.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Gunther Schadow.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "gsc.h"
#if NGSC > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/syslog.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/

#include <machine/gsc.h>

#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/gscreg.h>

/***********************************************************************
 *
 * CONSTANTS & DEFINES
 *
 ***********************************************************************/

#define PROBE_FAIL    0
#define PROBE_SUCCESS 1
#define SUCCESS       0
#define FAIL         -1
#define INVALID       FAIL

#define DMA1_READY  0x08

#ifdef GSCDEBUG
#define lprintf if(scu->flags & DEBUG) printf
#else
#define lprintf (void)
#endif

#define MIN(a, b)	(((a) < (b)) ? (a) : (b))

#define TIMEOUT (hz*15)  /* timeout while reading a buffer - default value */
#define LONG    (hz/60)  /* timesteps while reading a buffer */
#define GSCPRI  PRIBIO   /* priority while reading a buffer */

/***********************************************************************
 *
 * LAYOUT OF THE MINOR NUMBER
 *
 ***********************************************************************/

#define UNIT_MASK 0xc0    /* unit gsc0 .. gsc3 */
#define UNIT(x)   (x >> 6)
#define DBUG_MASK 0x20
#define FRMT_MASK 0x18    /* output format */
#define FRMT_RAW  0x00    /* output bits as read from scanner */
#define FRMT_GRAY 0x10    /* output graymap (not implemented yet) */
#define FRMT_PBM  0x08    /* output pbm format */
#define FRMT_PGM  0x18

/***********************************************************************
 *
 * THE GEMOMETRY TABLE
 *
 ***********************************************************************/

#define GEOMTAB_SIZE 7

static const struct gsc_geom {
  int dpi;     /* dots per inch */
  int dpl;     /* dots per line */
  int g_res;   /* get resolution value (status flag) */
  int s_res;   /* set resolution value (control register) */
} geomtab[GEOMTAB_SIZE] = {
  { 100,  424, GSC_RES_100, GSC_CNT_424},
  { 200,  840, GSC_RES_200, GSC_CNT_840},
  { 300, 1264, GSC_RES_300, GSC_CNT_1264},
  { 400, 1648, GSC_RES_400, GSC_CNT_1648},
  {  -1, 1696,          -1, GSC_CNT_1696},
  {  -2, 2644,          -2, GSC_CNT_2544},
  {  -3, 3648,          -3, GSC_CNT_3648},
};

#define NEW_GEOM { INVALID, INVALID, INVALID, INVALID }

/***********************************************************************
 *
 * THE TABLE OF UNITS
 *
 ***********************************************************************/

struct _sbuf {
  size_t  size;
  size_t  poi;
  char   *base;
};

struct gsc_unit {
  int channel;            /* DMA channel */
  int data;               /* - video port */
  int stat;               /* - status port */
  int ctrl;               /* - control port */
  int clrp;               /* - clear port */
  int flags;
#define ATTACHED 0x01
#define OPEN     0x02
#define READING  0x04
#define EOF      0x08
#define DEBUG    0x10
#define PBM_MODE 0x20
  int     geometry;       /* resolution as geomtab index */
  int     blen;           /* length of buffer in lines */
  int     btime;          /* timeout of buffer in seconds/hz */
  struct  _sbuf sbuf;
  char    ctrl_byte;      /* the byte actually written to ctrl port */
  int     height;         /* height, for pnm modes */
  size_t  bcount;         /* bytes to read, for pnm modes */
  struct  _sbuf hbuf;     /* buffer for pnm header data */
#ifdef DEVFS
  void *devfs_gsc;	  /* storage for devfs tokens (handles) */
  void *devfs_gscp;
  void *devfs_gscd;
  void *devfs_gscpd;
#endif
} unittab[NGSC];

/* I could not find a reasonable buffer size limit other than by
 * experiments. MAXPHYS is obviously too much, while DEV_BSIZE and
 * PAGE_SIZE are really too small. There must be something wrong
 * with isa_dmastart/isa_dmarangecheck HELP!!!
 */
#define MAX_BUFSIZE 0x3000
#define DEFAULT_BLEN 59

/***********************************************************************
 *
 * THE PER-DRIVER RECORD FOR ISA.C
 *
 ***********************************************************************/

static	int gscprobe (struct isa_device *isdp);
static	int gscattach(struct isa_device *isdp);

struct isa_driver gscdriver = { gscprobe, gscattach, "gsc" };

static	d_open_t	gscopen;
static	d_close_t	gscclose;
static	d_read_t	gscread;
static	d_ioctl_t	gscioctl;

#define CDEV_MAJOR 47
static struct cdevsw gsc_cdevsw = 
	{ gscopen,      gscclose,       gscread,        nowrite,	/*47*/
	  gscioctl,     nostop,         nullreset,      nodevtotty,/* gsc */
	  seltrue,      nommap,         NULL,	"gsc",	NULL,	-1 };


/***********************************************************************
 *
 * LOCALLY USED SUBROUTINES
 *
 ***********************************************************************/

/***********************************************************************
 *
 * lookup_geometry -- lookup a record in the geometry table by pattern
 *
 * The caller supplies a geometry record pattern, where INVALID
 * matches anything. Returns the index in the table or INVALID if
 * lookup fails.
 */

static int
lookup_geometry(struct gsc_geom geom, const struct gsc_unit *scu)
{
  struct gsc_geom tab;
  int i;

  for(i=0; i<GEOMTAB_SIZE; i++)
    {
      tab = geomtab[i];

      if ( ( ( geom.dpi   != INVALID ) && ( tab.dpi   == geom.dpi   ) ) ||
	   ( ( geom.dpl   != INVALID ) && ( tab.dpl   == geom.dpl   ) ) ||
	   ( ( geom.g_res != INVALID ) && ( tab.g_res == geom.g_res ) ) ||
	   ( ( geom.s_res != INVALID ) && ( tab.s_res == geom.s_res ) ) )
	{
	  lprintf("gsc.lookup_geometry: "
		 "geometry lookup found: %ddpi, %ddpl\n",
		 tab.dpi, tab.dpl);
	  return i;
	}
    }

  lprintf("gsc.lookup_geometry: "
	 "geometry lookup failed on {%d, %d, 0x%02x, 0x%02x}\n",
	 geom.dpi, geom.dpl, geom.g_res, geom.s_res);

  return INVALID;
}

/***********************************************************************
 *
 * get_geometry -- read geometry from status port
 *
 * Returns the index into geometry table or INVALID if it fails to
 * either read the status byte or lookup the record.
 */

static int
get_geometry(const struct gsc_unit *scu)
{
  struct gsc_geom geom = NEW_GEOM;

  lprintf("gsc.get_geometry: get geometry at 0x%03x\n", scu->stat);

  if ( ( geom.g_res = inb(scu->stat) ) == FAIL )
    return INVALID;

  geom.g_res &= GSC_RES_MASK;

  return lookup_geometry(geom, scu);
}

/***********************************************************************
 *
 * buffer_allocate -- allocate/reallocate a buffer
 */

static int
buffer_allocate(struct gsc_unit *scu)
{
  size_t size;

  size = scu->blen * geomtab[scu->geometry].dpl / 8;

  lprintf("gsc.buffer_allocate: need 0x%x bytes\n", size);

  if ( size > MAX_BUFSIZE )
    {
      lprintf("gsc.buffer_allocate: 0x%x bytes are too much\n", size);
      return ENOMEM;
    }

  if ( scu->sbuf.base != NULL )
    if ( scu->sbuf.size == size )
      {
	lprintf("gsc.buffer_allocate: keep old buffer\n");
	return SUCCESS;
      }
    else
      {
	lprintf("gsc.buffer_allocate: release old buffer\n");
	free( scu->sbuf.base, M_DEVBUF );
      }

  scu->sbuf.base = (char *)malloc(size, M_DEVBUF, M_WAITOK);

  if ( scu->sbuf.base == NULL )
    {
      lprintf("gsc.buffer_allocate: "
	     "buffer allocatation failed for size = 0x%x\n",
	     scu->sbuf.size);
      return ENOMEM;
    }

  scu->sbuf.size = size;
  scu->sbuf.poi  = size;

  lprintf("gsc.buffer_allocate: ok\n");

  return SUCCESS;
}

/***********************************************************************
 *
 * buffer_read -- scan a buffer
 */

static int
buffer_read(struct gsc_unit *scu)
{
  int stb;
  int res = SUCCESS;
  int chan_bit;
  char *p;
  int sps;
  int delay;

  lprintf("gsc.buffer_read: begin\n");

  if (scu->ctrl_byte == INVALID)
    {
      lprintf("gsc.buffer_read: invalid ctrl_byte\n");
      return EIO;
    }

  sps=splbio();

  outb( scu->ctrl, scu->ctrl_byte | GSC_POWER_ON );
  outb( scu->clrp, 0 );
  stb = inb( scu->stat );

  isa_dmastart(B_READ, scu->sbuf.base, scu->sbuf.size, scu->channel);

  chan_bit = 0x01 << scu->channel;

  for(delay=0; !(inb(DMA1_READY) & 0x01 << scu->channel); delay += LONG)
    {
      if(delay >= scu->btime)
	{
	  splx(sps);
	  lprintf("gsc.buffer_read: timeout\n");
	  res = EWOULDBLOCK;
	  break;
	}
      res = tsleep((caddr_t)scu, GSCPRI | PCATCH, "gscread", LONG);
      if ( ( res == 0 ) || ( res == EWOULDBLOCK ) )
	res = SUCCESS;
      else
	break;
    }
  splx(sps);
  isa_dmadone(B_READ, scu->sbuf.base, scu->sbuf.size, scu->channel);
  outb( scu->clrp, 0 );

  if(res != SUCCESS)
    {
      lprintf("gsc.buffer_read: aborted with %d\n", res);
      return res;
    }

  lprintf("gsc.buffer_read: invert buffer\n");
  for(p = scu->sbuf.base + scu->sbuf.size - 1; p >= scu->sbuf.base; p--)
    *p = ~*p;

  scu->sbuf.poi = 0;
  lprintf("gsc.buffer_read: ok\n");
  return SUCCESS;
}

/***********************************************************************
 *
 * the main functions
 *
 ***********************************************************************/

/***********************************************************************
 *
 * gscprobe
 *
 * read status port and check for proper configuration:
 *  - if address group matches (status byte has reasonable value)
 *  - if DMA channel matches   (status byte has correct value)
 */

static int
gscprobe (struct isa_device *isdp)
{
  int unit = isdp->id_unit;
  struct gsc_unit *scu = unittab + unit;
  int stb;
  struct gsc_geom geom = NEW_GEOM;

  scu->flags = DEBUG;

  lprintf("gsc%d.probe "
	 "on iobase 0x%03x, irq %d, drq %d, addr %d, size %d\n",
	 unit,
	 isdp->id_iobase,
	 isdp->id_irq,
	 isdp->id_drq,
	 isdp->id_maddr,
	 isdp->id_msize);

  if ( isdp->id_iobase < 0 )
    {
      lprintf("gsc%d.probe: no iobase given\n", unit);
      return PROBE_FAIL;
    }

  stb = inb( GSC_STAT(isdp->id_iobase) );
  if (stb == FAIL)
    {
      lprintf("gsc%d.probe: get status byte failed\n", unit);
      return PROBE_FAIL;
    }

  scu->data = GSC_DATA(isdp->id_iobase);
  scu->stat = GSC_STAT(isdp->id_iobase);
  scu->ctrl = GSC_CTRL(isdp->id_iobase);
  scu->clrp = GSC_CLRP(isdp->id_iobase);

  outb(scu->clrp,stb);
  stb = inb(scu->stat);

  switch(stb & GSC_CNF_MASK) {
  case GSC_CNF_DMA1:
    lprintf("gsc%d.probe: DMA 1\n", unit);
    scu->channel = 1;
    break;

  case GSC_CNF_DMA3:
    lprintf("gsc%d.probe: DMA 3\n", unit);
    scu->channel = 3;
    break;

  case GSC_CNF_IRQ3:
    lprintf("gsc%d.probe: IRQ 3\n", unit);
    goto probe_noirq;
  case GSC_CNF_IRQ5:
    lprintf("gsc%d.probe: IRQ 5\n", unit);
  probe_noirq:
    lprintf("gsc%d.probe: sorry, can't use IRQ yet\n", unit);
    return PROBE_FAIL;
  default:
    lprintf("gsc%d.probe: invalid status byte\n", unit, stb);
    return PROBE_FAIL;
  }

  geom.g_res = stb & GSC_RES_MASK;
  scu->geometry = lookup_geometry(geom, scu);
  if (scu->geometry == INVALID)
    {
      lprintf("gsc%d.probe: geometry lookup failed\n", unit);
      return PROBE_FAIL;
    }
  else
    {
      scu->ctrl_byte = geomtab[scu->geometry].s_res;
      outb(scu->ctrl, scu->ctrl_byte | GSC_POWER_ON);

      lprintf("gsc%d.probe: status 0x%02x, %ddpi\n",
	     unit, stb, geomtab[scu->geometry].dpi);

      outb(scu->ctrl, scu->ctrl_byte & ~GSC_POWER_ON);
    }

  lprintf("gsc%d.probe: ok\n", unit);

  scu->flags &= ~DEBUG;

  return PROBE_SUCCESS;
}

/***********************************************************************
 *
 * gscattach
 *
 * finish initialization of unit structure
 * get geometry value
 */

static int
gscattach(struct isa_device *isdp)
{
  int unit = isdp->id_unit;
  struct gsc_unit *scu = unittab + unit;
  char	name[32];

  scu->flags |= DEBUG;

  lprintf("gsc%d.attach: "
	 "iobase 0x%03x, irq %d, drq %d, addr %d, size %d\n",
	 unit,
	 isdp->id_iobase,
	 isdp->id_irq,
	 isdp->id_drq,
	 isdp->id_maddr,
	 isdp->id_msize);

  printf("gsc%d: GeniScan GS-4500 at %ddpi\n",
	 unit, geomtab[scu->geometry].dpi);

  /* initialize buffer structure */
  scu->sbuf.base = NULL;
  scu->sbuf.size = INVALID;
  scu->sbuf.poi  = INVALID;

  scu->blen = DEFAULT_BLEN;
  scu->btime = TIMEOUT;

  scu->flags |= ATTACHED;
  lprintf("gsc%d.attach: ok\n", unit);
  scu->flags &= ~DEBUG;
#ifdef DEVFS
#define GSC_UID 0
#define GSC_GID 13
    sprintf(name,"gsc%d",unit);
/*            path      name  devsw    minor    type   uid gid perm*/
   scu->devfs_gsc = devfs_add_devsw("/",   name,  &gsc_cdevsw, unit<<6,
					DV_CHR, GSC_UID,  GSC_GID, 0666);
    sprintf(name,"gsc%dp",unit);
   scu->devfs_gscp = devfs_add_devsw("/",   name,  &gsc_cdevsw,
					((unit<<6) + FRMT_PBM),
					DV_CHR, GSC_UID,  GSC_GID, 0666);
    sprintf(name,"gsc%dd",unit);
   scu->devfs_gscd = devfs_add_devsw("/",   name,  &gsc_cdevsw,
					((unit<<6) + DBUG_MASK),
					DV_CHR, GSC_UID,  GSC_GID, 0666);
    sprintf(name,"gsc%dpd",unit);
   scu->devfs_gscpd = devfs_add_devsw("/",   name,  &gsc_cdevsw,
					((unit<<6) + DBUG_MASK + FRMT_PBM),
					DV_CHR, GSC_UID,  GSC_GID, 0666);
#endif /*DEVFS*/

  return SUCCESS; /* attach must not fail */
}

/***********************************************************************
 *
 * gscopen
 *
 * set open flag
 * set modes according to minor number
 * don't switch scanner on, wait until first read ioctls go before
 */

static	int
gscopen  (dev_t dev, int flags, int fmt, struct proc *p)
{
  int unit = UNIT(minor(dev)) & UNIT_MASK;
  struct gsc_unit *scu = unittab + unit;

  if ( minor(dev) & DBUG_MASK )
    scu->flags |= DEBUG;
  else
    scu->flags &= ~DEBUG;

  switch(minor(dev) & FRMT_MASK) {
  case FRMT_PBM:
    scu->flags |= PBM_MODE;
    lprintf("gsc%d.open: pbm mode\n", unit);
    break;
  case FRMT_RAW:
    lprintf("gsc%d.open: raw mode\n", unit);
    scu->flags &= ~PBM_MODE;
    break;
  default:
    lprintf("gsc%d.open: gray maps are not yet supported", unit);
    return ENXIO;
  }

  lprintf("gsc%d.open: minor %d\n",
	 unit, minor(dev));

  if ( unit >= NGSC || !( scu->flags & ATTACHED ) )
    {
      lprintf("gsc%d.open: unit was not attached successfully 0x04x\n",
	     unit, scu->flags);
      return ENXIO;
    }

  if ( scu->flags & OPEN )
    {
      lprintf("gsc%d.open: already open", unit);
      return EBUSY;
    }

  scu->flags |= OPEN;

  return SUCCESS;
}

/***********************************************************************
 *
 * gscclose
 *
 * turn off scanner
 * release the buffer
 */

static	int
gscclose (dev_t dev, int flags, int fmt, struct proc *p)
{
  int unit = UNIT(minor(dev));
  struct gsc_unit *scu = unittab + unit;

  lprintf("gsc%d.close: minor %d\n",
	 unit, minor(dev));

  if ( unit >= NGSC || !( scu->flags & ATTACHED ) )
    {
      lprintf("gsc%d.read: unit was not attached successfully 0x04x\n",
	     unit, scu->flags);
      return ENXIO;
    }

  outb(scu->ctrl, scu->ctrl_byte & ~GSC_POWER_ON);

  if ( scu->sbuf.base != NULL ) free( scu->sbuf.base, M_DEVBUF );

  scu->sbuf.base = NULL;
  scu->sbuf.size = INVALID;
  scu->sbuf.poi  = INVALID;

  scu->flags &= ~(DEBUG | OPEN | READING);

  return SUCCESS;
}

/***********************************************************************
 *
 * gscread
 */

static	int
gscread  (dev_t dev, struct uio *uio, int ioflag)
{
  int unit = UNIT(minor(dev));
  struct gsc_unit *scu = unittab + unit;
  size_t nbytes;
  int res;

  lprintf("gsc%d.read: minor %d\n", unit, minor(dev));

  if ( unit >= NGSC || !( scu->flags & ATTACHED ) )
    {
      lprintf("gsc%d.read: unit was not attached successfully 0x04x\n",
	     unit, scu->flags);
      return ENXIO;
    }

  if ( !(scu->flags & READING) )
    {
      res = buffer_allocate(scu);
      if ( res == SUCCESS )
	scu->flags |= READING;
      else
	return res;

      scu->ctrl_byte = geomtab[scu->geometry].s_res;

      /* initialize for pbm mode */
      if ( scu->flags & PBM_MODE )
	{
	  char *p;
	  int width = geomtab[scu->geometry].dpl;

	  sprintf(scu->sbuf.base,"P4 %d %d\n", width, scu->height);
	  scu->bcount = scu->height * width / 8;

	  lprintf("gsc%d.read: initializing pbm mode: `%s', bcount: 0x%x\n",
		  unit, scu->sbuf.base, scu->bcount);

	  /* move header to end of sbuf */
	  for(p=scu->sbuf.base; *p; p++);
	  while(--p >= scu->sbuf.base)
	    {
	      *(char *)(scu->sbuf.base + --scu->sbuf.poi) = *p;
	      scu->bcount++;
	    }
	}
    }

  lprintf("gsc%d.read(before buffer_read): "
	  "size 0x%x, pointer 0x%x, bcount 0x%x, ok\n",
	  unit, scu->sbuf.size, scu->sbuf.poi, scu->bcount);

  if ( scu->sbuf.poi == scu->sbuf.size )
    if ( (res = buffer_read(scu)) != SUCCESS )
      return res;

  lprintf("gsc%d.read(after buffer_read): "
	  "size 0x%x, pointer 0x%x, bcount 0x%x, ok\n",
	  unit, scu->sbuf.size, scu->sbuf.poi, scu->bcount);

  nbytes = MIN( uio->uio_resid, scu->sbuf.size - scu->sbuf.poi );

  if ( (scu->flags & PBM_MODE) )
    nbytes = MIN( nbytes, scu->bcount );

  lprintf("gsc%d.read: transferring 0x%x bytes", nbytes);

  res = uiomove(scu->sbuf.base + scu->sbuf.poi, nbytes, uio);
  if ( res != SUCCESS )
    {
      lprintf("gsc%d.read: uiomove failed %d", unit, res);
      return res;
    }

  scu->sbuf.poi += nbytes;
  if ( scu->flags & PBM_MODE ) scu->bcount -= nbytes;

  lprintf("gsc%d.read: size 0x%x, pointer 0x%x, bcount 0x%x, ok\n",
	  unit, scu->sbuf.size, scu->sbuf.poi, scu->bcount);

  return SUCCESS;
}

/***********************************************************************
 *
 * gscioctl
 *
 */

static	int
gscioctl (dev_t dev, int cmd, caddr_t data, int flag, struct proc *p)
{
  int unit = UNIT(minor(dev));
  struct gsc_unit *scu = unittab + unit;

  lprintf("gsc%d.ioctl: minor %d\n",
	 unit, minor(dev));

  if ( unit >= NGSC || !( scu->flags & ATTACHED ) )
    {
      lprintf("gsc%d.ioctl: unit was not attached successfully 0x04x\n",
	     unit, scu->flags);
      return ENXIO;
    }

  switch(cmd) {
  case GSC_SRESSW:
    lprintf("gsc%d.ioctl:GSC_SRESSW\n", unit);
    if ( scu->flags & READING )
      {
	lprintf("gsc%d:ioctl on already reading unit\n", unit);
	return EBUSY;
      }
    scu->geometry = get_geometry(scu);
    return SUCCESS;
  case GSC_GRES:
    *(int *)data=geomtab[scu->geometry].dpi;
    lprintf("gsc%d.ioctl:GSC_GRES %ddpi\n", unit, *(int *)data);
    return SUCCESS;
  case GSC_GWIDTH:
    *(int *)data=geomtab[scu->geometry].dpl;
    lprintf("gsc%d.ioctl:GSC_GWIDTH %d\n", unit, *(int *)data);
    return SUCCESS;
  case GSC_SRES:
  case GSC_SWIDTH:
    lprintf("gsc%d.ioctl:GSC_SRES or GSC_SWIDTH %d\n",
	   unit, *(int *)data);
    { int g;
      struct gsc_geom geom = NEW_GEOM;
      if ( cmd == GSC_SRES )
	geom.dpi = *(int *)data;
      else
	geom.dpl = *(int *)data;
      if ( ( g = lookup_geometry(geom, scu) ) == INVALID )
	return EINVAL;
      scu->geometry = g;
      return SUCCESS;
    }
  case GSC_GHEIGHT:
    *(int *)data=scu->height;
    lprintf("gsc%d.ioctl:GSC_GHEIGHT %d\n", unit, *(int *)data);
    return SUCCESS;
  case GSC_SHEIGHT:
    lprintf("gsc%d.ioctl:GSC_SHEIGHT %d\n", unit, *(int *)data);
    if ( scu->flags & READING )
      {
	lprintf("gsc%d:ioctl on already reading unit\n", unit);
	return EBUSY;
      }
    scu->height=*(int *)data;
    return SUCCESS;
  case GSC_GBLEN:
    *(int *)data=scu->blen;
    lprintf("gsc%d.ioctl:GSC_GBLEN %d\n", unit, *(int *)data);
    return SUCCESS;
  case GSC_SBLEN:
    lprintf("gsc%d.ioctl:GSC_SBLEN %d\n", unit, *(int *)data);
    if (*(int *)data * geomtab[scu->geometry].dpl / 8 > MAX_BUFSIZE)
      {
	lprintf("gsc%d:ioctl buffer size too high\n", unit);
	return ENOMEM;
      }
    scu->blen=*(int *)data;
    return SUCCESS;
  case GSC_GBTIME:
    *(int *)data = scu->btime / hz;
    lprintf("gsc%d.ioctl:GSC_GBTIME %d\n", unit, *(int *)data);
    return SUCCESS;
  case GSC_SBTIME:
    scu->btime = *(int *)data * hz;
    lprintf("gsc%d.ioctl:GSC_SBTIME %d\n", unit, *(int *)data);
    return SUCCESS;
  default: return ENOTTY;
  }
}


static gsc_devsw_installed = 0;

static void
gsc_drvinit(void *unused)
{
	dev_t dev;

	if( ! gsc_devsw_installed ) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev,&gsc_cdevsw, NULL);
		gsc_devsw_installed = 1;
    	}
}

SYSINIT(gscdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,gsc_drvinit,NULL)


#endif /* NGSC > 0 */

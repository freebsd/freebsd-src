/*
 * This code is derived from code available from the STB bulletin board
 *
 * $FreeBSD$
 */

/* $XFree86: mit/server/ddx/x386/common_hw/ICD2061Aalt.c,v 2.6 1994/04/15 05:10:30 dawes Exp $ */

#ifndef _KERNEL
#include "compiler.h"
#else
#define GCCUSESGAS
#define PCVT_STANDALONE 1
#endif

#define SEQREG   0x03C4
#define MISCREG  0x03C2
#define MISCREAD 0x03CC

double fref = 14.31818 * 2.0;
char ascclk[] = "VIDEO CLOCK ?";

unsigned short clknum;
unsigned short vlbus_flag;
unsigned short card;
unsigned short crtcaddr;
unsigned short clockreg;

static double range[15] = {50.0, 51.0, 53.2, 58.5, 60.7, 64.4, 66.8, 73.5,
			   75.6, 80.9, 83.2, 91.5, 100.0, 120.0, 120.0};

#ifdef __STDC__
static double genratio(unsigned int *p, unsigned int *q, double tgt);
static double f(unsigned int p, unsigned int q, double basefreq);
#if 0
static void prtbinary(unsigned int size, unsigned int val);
#endif
static void wait_vb();
static void wrt_clk_bit(unsigned int value);
static void init_clock(unsigned long setup, unsigned short crtcport);
#else
static double genratio();
static double f();
#if 0
static void prtbinary();
#endif
static void wait_vb();
static void wrt_clk_bit();
static void init_clock();
#endif

void AltICD2061SetClock(frequency, select)
register long   frequency;               /* in Hz */
int select;
{
   unsigned int m, mval, ival;
   int i;
   long dwv;
   double realval;
   double freq, fvco;
   double dev, devx;
   double delta, deltax;
   unsigned int p, q;
   unsigned int bestp, bestq;
   unsigned char tmp;

   crtcaddr=(inb(0x3CC) & 0x01) ? 0x3D4 : 0x3B4;


   outb(crtcaddr, 0x11);	/* Unlock CRTC registers */
   tmp = inb(crtcaddr + 1);
   outb(crtcaddr + 1, tmp & ~0x80);

   outw(crtcaddr, 0x4838);	/* Unlock S3 register set */
   outw(crtcaddr, 0xA039);

   clknum = select;

   freq = ((double)frequency)/1000000.0;
   if (freq > range[14])
	freq =range[14];
   else if (freq <= 6.99)
      freq = 7.0;

/*
 *  Calculate values to load into ICD 2061A clock chip to set frequency
 */
   delta = 999.0;
   dev = 999.0;
   ival = 99;
   mval = 99;

   fvco = freq / 2;
   for (m = 0; m < 8; m++) {
      fvco *= 2.0;
      for (i = 14; i >= 0; i--)
         if (fvco >= range[i])
            break;
      if (i < 0)
         continue;
      if (i == 14)
         break;
      devx = (fvco - (range[i] + range[i+1])/2)/fvco;
      if (devx < 0)
         devx = -devx;
      deltax = genratio(&p, &q, fvco);
      if (delta < deltax)
         continue;
      if (deltax < delta || devx < dev) {
         bestp = p;
         bestq = q;
         delta = deltax;
         dev = devx;
         ival = i;
         mval = m;
         }
      }
   fvco = fref;
   for (m=0; m<mval; m++)
      fvco /= 2.0;
   realval = f(bestp, bestq, fvco);
   dwv = ((((((long)ival << 7) | bestp) << 3) | mval) << 7) | bestq;

/*
 * Write ICD 2061A clock chip
 */
   init_clock(((unsigned long)dwv) | (((long)clknum) << 21), crtcaddr);

   wait_vb();
   wait_vb();
   wait_vb();
   wait_vb();
   wait_vb();
   wait_vb();
   wait_vb();		/* 0.10 second delay... */
}

static double f(p, q, base)
   unsigned int p;
   unsigned int q;
   double base;
   {
   return(base * (p + 3)/(q + 2));
   }

static double genratio(p, q, tgt)
   unsigned int *p;
   unsigned int *q;
   double tgt;
   {
   int k, m;
   double test, mindiff;
   unsigned int mmax;

   mindiff = 999999999.0;
   for (k = 13; k < 69; k++) {	       /* q={15..71}:Constraint 2 on page 14 */
      m = 50.0*k/fref - 3;
      if (m < 0)
         m = 0;
      mmax = 120*k/fref - 3;	       /* m..mmax is constraint 3 on page 14 */
      if (mmax > 128)
         mmax = 128;
      while (m < mmax) {
         test = f(m, k, fref) - tgt;
         if (test < 0) test = -test;
         if (mindiff > test) {
            mindiff = test;
            *p = m;
            *q = k;
            }
         m++;
         }
      }
   return (mindiff);
   }

#if 0
static void prtbinary(size, val)
   unsigned int size;
   unsigned int val;
   {
   unsigned int mask;
   int k;

   mask = 1;

   for (k=size; --k > 0 || mask <= val/2;)
      mask <<= 1;

   while (mask) {
      fputc((mask&val)? '1': '0' , stderr);
      mask >>= 1;
      }
   }
#endif

static void wait_vb()
   {
   while ((inb(crtcaddr+6) & 0x08) == 0)
      ;
   while (inb(crtcaddr+6) & 0x08)
      ;
   }


#ifdef __STDC__
static void init_clock(unsigned long setup, unsigned short crtcport)
#else
static void init_clock(setup, crtcport)
   unsigned long setup;
   unsigned short crtcport;
#endif
   {
   unsigned char nclk[2], clk[2];
   unsigned short restore42;
   unsigned short oldclk;
   unsigned short bitval;
   int i;
   unsigned char c;

#ifndef PCVT_STANDALONE
   (void)xf86DisableInterrupts();
#endif

   oldclk = inb(0x3CC);

   outb(crtcport, 0x42);
   restore42 = inb(crtcport+1);

   outw(0x3C4, 0x0100);

   outb(0x3C4, 1);
   c = inb(0x3C5);
   outb(0x3C5, 0x20 | c);

   outb(crtcport, 0x42);
   outb(crtcport+1, 0x03);

   outw(0x3C4, 0x0300);

   nclk[0] = oldclk & 0xF3;
   nclk[1] = nclk[0] | 0x08;
   clk[0] = nclk[0] | 0x04;
   clk[1] = nclk[0] | 0x0C;

   outb(crtcport, 0x42);
   i = inw(crtcport);

   outw(0x3C4, 0x0100);

   wrt_clk_bit(oldclk | 0x08);
   wrt_clk_bit(oldclk | 0x0C);
   for (i=0; i<5; i++) {
      wrt_clk_bit(nclk[1]);
      wrt_clk_bit(clk[1]);
      }
   wrt_clk_bit(nclk[1]);
   wrt_clk_bit(nclk[0]);
   wrt_clk_bit(clk[0]);
   wrt_clk_bit(nclk[0]);
   wrt_clk_bit(clk[0]);
   for (i=0; i<24; i++) {
      bitval = setup & 0x01;
      setup >>= 1;
      wrt_clk_bit(clk[1-bitval]);
      wrt_clk_bit(nclk[1-bitval]);
      wrt_clk_bit(nclk[bitval]);
      wrt_clk_bit(clk[bitval]);
      }
   wrt_clk_bit(clk[1]);
   wrt_clk_bit(nclk[1]);
   wrt_clk_bit(clk[1]);

   outb(0x3C4, 1);
   c = inb(0x3C5);
   outb(0x3C5, 0xDF & c);

   outb(crtcport, 0x42);
   outb(crtcport+1, restore42);

   outb(0x3C2, oldclk);

   outw(0x3C4, 0x0300);

#ifndef PCVT_STANDALONE
   xf86EnableInterrupts();
#endif

   }

static void wrt_clk_bit(value)
   unsigned int value;
   {
   int j;

   outb(0x3C2, value);
   for (j=2; --j; )
      inb(0x200);
   }

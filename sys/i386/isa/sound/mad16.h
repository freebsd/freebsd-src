
/*
 *	Initialization code for OPTI MAD16 interface chip by
 *	Davor Jadrijevic <davor@emard.pub.hr>
 *	(Included by ad1848.c when MAD16 support is enabled)
 *
 * It looks like MAD16 is similar than the Mozart chip (OAK OTI-601).
 * It could be even possible that these chips are exactly the same. Can
 * anybody confirm this?
 */

static void wr_a_mad16(int base, int v, int a)
{
 OUTB(a, base + 0xf);
 OUTB(v, base + 0x11);
}

static void wr_b_mad16(int base, int v, int a)
{
 OUTB(a, base + 0xf);
 OUTB(v, base + 0xd);
}

/*
static int rd_a_mad16(int base, int a)
{
 OUTB(a, base + 0xf);
 return INB(base + 0x11);
}
*/

static int rd_b_mad16(int base, int a)
{
 OUTB(a, base + 0xf);
 return INB(base + 0xd);
}

/*
static int rd_0_mad16(int base, int a)
{
 OUTB(a, base + 0xf);
 return INB(base + 0xf);
}

static void wr_ad(int base, int v, int a)
{
 OUTB(a, base + 4);
 OUTB(v, base + 5);
}

static int rd_ad(int base, int a)
{
 OUTB(a, base + 4);
 return INB(base + 5);
}
*/

static int mad16init(int adr)
{
 int j;
 long i;

 static int ad1848_bases[] = 
{ 0x220, -1, -1, 0x240, -1, -1, -1, -1, 0x530, 0xE80, 0xF40, 0x604, 0 };

 int mad16_base = 0xf80, ad1848_base;


 for(j = 0; (j < 16) && (ad1848_bases[j] != 0); j++)
  if(adr == ad1848_bases[j])
   break;

 if( (ad1848_base = ad1848_bases[j]) < 0x530)
 {
  printk("Unknown MAD16 setting 0x%3X\n", adr);
  return -1;
 }

 /* printk("OPTi MAD16 WSS at 0x%3X\n", ad1848_base); */

 rd_b_mad16(mad16_base, 0xe2);
 wr_a_mad16(mad16_base, 0x1a, 0xe2);
 wr_b_mad16(mad16_base, j * 16 + 1, 0xe2);
 wr_a_mad16(mad16_base, 0x1a, 0xe2);
 for( i = 0; i < 10000; i++)
  if( (INB(ad1848_base+4) & 0x80) == 0 )
   break;

 return 0;
};


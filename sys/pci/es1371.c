/*
 * Support Sound Cards based on the Ensoniq/Creative Labs ES1371/1373 
 *
 * Copyright (c) 1999 by Russell Cattelan. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgement:
 *      This product includes software developed by Russell Cattelan.
 *
 * 4. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */


#include <sys/param.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>

#include <i386/isa/snd/sound.h>

#include <pci/es1370_reg.h>

/* -------------------------------------------------------------------- */
/*
 * hweightN: returns the hamming weight (i.e. the number
 * of bits set) of a N-bit word
 */

unsigned int hweight32(unsigned int w);

__inline__ unsigned int hweight32(unsigned int w)
{
  unsigned int res = (w & 0x55555555) + ((w >> 1) & 0x55555555);
  res = (res & 0x33333333) + ((res >> 2) & 0x33333333);
  res = (res & 0x0F0F0F0F) + ((res >> 4) & 0x0F0F0F0F);
  res = (res & 0x00FF00FF) + ((res >> 8) & 0x00FF00FF);
  return (res & 0x0000FFFF) + ((res >> 16) & 0x0000FFFF);
}

/* #define ES1371_REG_STATUS		        0x14 */
#define ES1371_REG_CODEC		        0x14 /* this is really for the 1371  */
#define ES1371_CODEC_INDEX_SHIFT		16 /* this is really for the 1371  */
#define ES1371_REG_RECSRC               0x1a

#define ES1371_REG_LEGACY 0x18	/* W/R: Legacy control/status register */
#define   ES1371_REG_SMPRATE 0x10	/* W/R: Codec rate converter interface register */
#define   ES1371_SRC_RAM_ADDRO(o) (((o)&0x7f)<<25)	/* address of the sample rate converter */
#define   ES1371_SRC_RAM_ADDRM	   (0x7f<<25)	/* mask for above */
#define   ES1371_SRC_RAM_ADDRI(i) (((i)>>25)&0x7f)	/* address of the sample rate converter */
#define   ES1371_SRC_RAM_WE	   (1<<24)	/* R/W: read/write control for sample rate converter */
#define   ES1371_SRC_RAM_BUSY     (1<<23)	/* R/O: sample rate memory is busy */
#define   ES1371_SRC_DISABLE      (1<<22)	/* sample rate converter disable */
#define   ES1371_DIS_P1	   (1<<21)	/* playback channel 1 accumulator update disable */
#define   ES1371_DIS_P2	   (1<<20)	/* playback channel 1 accumulator update disable */
#define   ES1371_DIS_R1	   (1<<19)	/* record channel accumulator update disable */
#define   ES1371_SRC_RAM_DATAO(o) (((o)&0xffff)<<0)	/* current value of the sample rate converter */
#define   ES1371_SRC_RAM_DATAM	   (0xffff<<0)	/* mask for above */
#define   ES1371_SRC_RAM_DATAI(i) (((i)>>0)&0xffff)	/* current value of the sample rate converter */
#define   ES_1371_SYNC_RES	(1<<14)		/* Warm AC97 reset */
#define ES1371_CSTAT      (1<<30)
 
#define CODEC_RDY         0x80000000  /* AC97 read data valid */
#define CODEC_WIP         0x40000000  /* AC97 write in progress */
#define CODEC_PORD        0x00800000  /* 0 = write AC97 register */
#define CODEC_POADD_MASK  0x007f0000
#define CODEC_POADD_SHIFT 16
#define CODEC_PODAT_MASK  0x0000ffff
#define CODEC_PODAT_SHIFT 0

/* codec constants */

#define CODEC_ID_DEDICATEDMIC    0x001
#define CODEC_ID_MODEMCODEC      0x002
#define CODEC_ID_BASSTREBLE      0x004
#define CODEC_ID_SIMULATEDSTEREO 0x008
#define CODEC_ID_HEADPHONEOUT    0x010
#define CODEC_ID_LOUDNESS        0x020
#define CODEC_ID_18BITDAC        0x040
#define CODEC_ID_20BITDAC        0x080
#define CODEC_ID_18BITADC        0x100
#define CODEC_ID_20BITADC        0x200

#define CODEC_ID_SESHIFT    10
#define CODEC_ID_SEMASK     0x1f

/* sample rate converter */
#define SRC_RAMADDR_MASK   0xfe000000
#define SRC_RAMADDR_SHIFT  25
#define SRC_WE             0x01000000  /* read/write control for SRC RAM */
#define SRC_BUSY           0x00800000  /* SRC busy */
#define SRC_DIS            0x00400000  /* 1 = disable SRC */
#define SRC_DDAC1          0x00200000  /* 1 = disable accum update for DAC1 */
#define SRC_DDAC2          0x00100000  /* 1 = disable accum update for DAC2 */
#define SRC_DADC           0x00080000  /* 1 = disable accum update for ADC2 */
#define SRC_RAMDATA_MASK   0x0000ffff
#define SRC_RAMDATA_SHIFT  0
/*
 *  Sample rate converter addresses
 */

#define ES_SMPREG_DAC1		0x70
#define ES_SMPREG_DAC2		0x74
#define ES_SMPREG_ADC		0x78
#define ES_SMPREG_VOL_ADC	0x6c
#define ES_SMPREG_VOL_DAC1	0x7c
#define ES_SMPREG_VOL_DAC2	0x7e
#define ES_SMPREG_TRUNC_N	0x00
#define ES_SMPREG_INT_REGS	0x01
#define ES_SMPREG_ACCUM_FRAC	0x02
#define ES_SMPREG_VFREQ_FRAC	0x03


#define CODEC_PIRD        0x00800000  /* 0 = write AC97 register */
#define CODEC_PIADD_MASK  0x007f0000
#define CODEC_PIADD_SHIFT 16
#define CODEC_PIDAT_MASK  0x0000ffff
#define CODEC_PIDAT_SHIFT 0


struct initvol {
	int mixch;
	int vol;
};

struct initvol initvol[] = {
	{ SOUND_MIXER_WRITE_LINE, 0x4040 },
	{ SOUND_MIXER_WRITE_CD, 0x4040 },
	{ MIXER_WRITE(SOUND_MIXER_VIDEO), 0x4040 },
	{ SOUND_MIXER_WRITE_LINE1, 0x4040 },
/* 	{ SOUND_MIXER_WRITE_PCM, 0x4040 }, */
 	{ SOUND_MIXER_WRITE_PCM, 0x6464 },
	{ SOUND_MIXER_WRITE_VOLUME, 0x4040 },
	{ SOUND_MIXER_WRITE_BASS, 0x1010 },
	{ SOUND_MIXER_WRITE_TREBLE, 0x1010 },
	{ MIXER_WRITE(SOUND_MIXER_PHONEOUT), 0x4040 },
	{ SOUND_MIXER_WRITE_OGAIN, 0x4040 },
	{ MIXER_WRITE(SOUND_MIXER_PHONEIN), 0x4040 },
	{ SOUND_MIXER_WRITE_SPEAKER, 0x4040 },
	{ SOUND_MIXER_WRITE_MIC, 0x4040 },
	{ SOUND_MIXER_WRITE_RECLEV, 0x4040 },
	{ SOUND_MIXER_WRITE_IGAIN, 0x4040 }
};


static const char *stereo_enhancement[] = 
{
	"no 3D stereo enhancement",
	"Analog Devices Phat Stereo",
	"Creative Stereo Enhancement",
	"National Semiconductor 3D Stereo Enhancement",
	"YAMAHA Ymersion",
	"BBE 3D Stereo Enhancement",
	"Crystal Semiconductor 3D Stereo Enhancement",
	"Qsound QXpander",
	"Spatializer 3D Stereo Enhancement",
	"SRS 3D Stereo Enhancement",
	"Platform Technologies 3D Stereo Enhancement", 
	"AKM 3D Audio",
	"Aureal Stereo Enhancement",
	"AZTECH  3D Enhancement",
	"Binaura 3D Audio Enhancement",
	"ESS Technology Stereo Enhancement",
	"Harman International VMAx",
	"NVidea 3D Stereo Enhancement",
	"Philips Incredible Sound",
	"Texas Instruments 3D Stereo Enhancement",
	"VLSI Technology 3D Stereo Enhancement"
};


static const unsigned int recsrc[8] = 
{
	SOUND_MASK_MIC,
	SOUND_MASK_CD,
	SOUND_MASK_VIDEO,
	SOUND_MASK_LINE1,
	SOUND_MASK_LINE,
	SOUND_MASK_VOLUME,
	SOUND_MASK_PHONEOUT,
	SOUND_MASK_PHONEIN
};

static const unsigned char volreg[] = 
{
	/* 5 bit stereo */
	[SOUND_MIXER_LINE] = 0x10,
	[SOUND_MIXER_CD] = 0x12,
	[SOUND_MIXER_VIDEO] = 0x14,
	[SOUND_MIXER_LINE1] = 0x16,
	[SOUND_MIXER_PCM] = 0x18,
	/* 6 bit stereo */
	[SOUND_MIXER_VOLUME] = 0x02,
	[SOUND_MIXER_PHONEOUT] = 0x04,
	/* 6 bit mono */
	[SOUND_MIXER_OGAIN] = 0x06,
	[SOUND_MIXER_PHONEIN] = 0x0c,
	/* 4 bit mono but shifted by 1 */
	[SOUND_MIXER_SPEAKER] = 0x08,
	/* 6 bit mono + preamp */
	[SOUND_MIXER_MIC] = 0x0e,
	/* 4 bit stereo */
	[SOUND_MIXER_RECLEV] = 0x1c,
	/* 4 bit mono */
	[SOUND_MIXER_IGAIN] = 0x1e
};


#define swab(x) ((((x) >> 8) & 0xff) | (((x) << 8) & 0xff00))
#define AC97_PESSIMISTIC
#define put_user(a,b)  *b=a


int
es_init_1371(snddev_info *d){
  	struct es_info *es = (struct es_info *)d->device_data;
	u_int		i;
	int idx;
	int val, val2 = 0;
	if(es_debug > 0) printf("es_init_1371\n");

	strncpy(d->name,"ES1371",strlen("ES1371"));
	es->ctrl = 0;
	es->sctrl = 0;
	/* initialize the chips */
	bus_space_write_4(es->st, es->sh, ES1370_REG_CONTROL, es->ctrl);
	bus_space_write_4(es->st, es->sh, ES1370_REG_SERIAL_CONTROL, es->sctrl);
	bus_space_write_4(es->st, es->sh, ES1371_REG_LEGACY, 0);
	/* AC'97 warm reset to start the bitclk */
	bus_space_write_4(es->st, es->sh, ES1371_REG_LEGACY, es->ctrl | ES_1371_SYNC_RES);
	DELAY(2000);
	bus_space_write_4(es->st, es->sh,  ES1370_REG_SERIAL_CONTROL,es->ctrl);
	/* Init the sample rate converter */
	bus_space_write_4(es->st, es->sh, ES1371_REG_SMPRATE, ES1371_SRC_DISABLE);
	for (idx = 0; idx < 0x80; idx++)
	  es1371_src_write(d, idx, 0);
	es1371_src_write(d, ES_SMPREG_DAC1 + ES_SMPREG_TRUNC_N, 16 << 4);
	es1371_src_write(d, ES_SMPREG_DAC1 + ES_SMPREG_INT_REGS, 16 << 10);
	es1371_src_write(d, ES_SMPREG_DAC2 + ES_SMPREG_TRUNC_N, 16 << 4);
	es1371_src_write(d, ES_SMPREG_DAC2 + ES_SMPREG_INT_REGS, 16 << 10);
	es1371_src_write(d, ES_SMPREG_VOL_ADC, 1 << 12);
	es1371_src_write(d, ES_SMPREG_VOL_ADC + 1, 1 << 12);
	es1371_src_write(d, ES_SMPREG_VOL_DAC1, 1 << 12);
	es1371_src_write(d, ES_SMPREG_VOL_DAC1 + 1, 1 << 12);
	es1371_src_write(d, ES_SMPREG_VOL_DAC2, 1 << 12);
	es1371_src_write(d, ES_SMPREG_VOL_DAC2 + 1, 1 << 12);
	es1371_adc_rate(d, 22050, 1);
	es1371_dac1_rate(d, 22050, 1);
	es1371_dac2_rate(d, 22050, 1);
	/* WARNING:
	 * enabling the sample rate converter without properly programming
	 * its parameters causes the chip to lock up (the SRC busy bit will
	 * be stuck high, and I've found no way to rectify this other than
	 * power cycle)
	 */
	/*outl(0, s->io+ES1371_REG_SRCONV); */
	bus_space_write_4(es->st, es->sh, ES1371_REG_SMPRATE, 0);
	/* codec init */
	wrcodec(d, 0x00, 0); /* reset codec */
	d->bd_id = rdcodec(d, 0x00);  /* get codec ID */
	val = rdcodec(d, 0x7c);
	val2 = rdcodec(d, 0x7e);
	if(es_debug > 0) printf("init: d->bd_id 0x%x val 0x%x val2 0x%x\n",d->bd_id,val,val2);
	d->bd_id |=  CODEC_ID_BASSTREBLE;
	printf("es1371: codec vendor %c%c%c revision %d\n", 
		   (val >> 8) & 0xff, val & 0xff, (val2 >> 8) & 0xff, val2 & 0xff);
	printf("es1371: codec features");
	if (d->bd_id & CODEC_ID_DEDICATEDMIC)
	  printf(" dedicated MIC PCM in");
	if (d->bd_id & CODEC_ID_MODEMCODEC)
	  printf(" Modem Line Codec");
	if (d->bd_id & CODEC_ID_BASSTREBLE)
	  printf(" Bass & Treble");
	if (d->bd_id & CODEC_ID_SIMULATEDSTEREO)
	  printf(" Simulated Stereo");
	if (d->bd_id & CODEC_ID_HEADPHONEOUT)
	  printf(" Headphone out");
	if (d->bd_id & CODEC_ID_LOUDNESS)
	  printf(" Loudness");
	if (d->bd_id & CODEC_ID_18BITDAC)
	  printf(" 18bit DAC");
	if (d->bd_id & CODEC_ID_20BITDAC)
	  printf(" 20bit DAC");
	if (d->bd_id & CODEC_ID_18BITADC)
	  printf(" 18bit ADC");
	if (d->bd_id & CODEC_ID_20BITADC)
	  printf(" 20bit ADC");
	printf("%s\n", (d->bd_id & 0x3ff) ? "" : " none");
	val = (d->bd_id >> CODEC_ID_SESHIFT) & CODEC_ID_SEMASK;
	printf("es1371: stereo enhancement: %s\n", (val <= 20) ? stereo_enhancement[val] : "unknown");
	val = SOUND_MASK_LINE;
	mixer_ioctl_1371(d, SOUND_MIXER_WRITE_RECSRC, (caddr_t)&val, 0,NULL);
	for (i = 0; i < sizeof(initvol)/sizeof(initvol[0]); i++) {
	  val = initvol[i].vol;
	  if(es_debug > 0) printf("es_init -> mixer_ioctl_1371 0x%x\n",val);
	  mixer_ioctl_1371(d, initvol[i].mixch, (caddr_t)&val, 0,NULL);
	}
	return 0;
}


int mixer_rdch(snddev_info *s, unsigned int ch, int *arg)
{
	int j;
	
	if(es_debug > 0) printf("mixer_rdch ch 0x%x\n",ch);
	switch (ch) {
	case SOUND_MIXER_MIC:
	  j = rdcodec(s, 0x0e);
	  if (j & 0x8000){
		put_user(0, (int *)arg);
		return (*arg);
	  }
	  put_user(0x4949 - 0x202 * (j & 0x1f) + ((j & 0x40) ? 0x1b1b : 0), (int *)arg);
	  return (*arg);
	  
	case SOUND_MIXER_OGAIN:
	case SOUND_MIXER_PHONEIN:
	  j = rdcodec(s, volreg[ch]);
	  if (j & 0x8000){
		put_user(0, (int *)arg);
		return (*arg);
	  }
	  put_user(0x6464 - 0x303 * (j & 0x1f), (int *)arg);
	  return (*arg);
	  
	case SOUND_MIXER_PHONEOUT:
	  if (!(s->bd_id & CODEC_ID_HEADPHONEOUT))
		return EINVAL;
	  /* fall through */
	case SOUND_MIXER_VOLUME:
	  j = rdcodec(s, volreg[ch]);
	  if (j & 0x8000){
		put_user(0, (int *)arg);
		return (*arg);
	  }
	  put_user(0x6464 - (swab(j) & 0x1f1f) * 3, (int *)arg);
	  return (*arg);
	case SOUND_MIXER_SPEAKER:
	  j = rdcodec(s, 0x0a);
	  if (j & 0x8000){
		put_user(0, (int *)arg);
		return (*arg);
	  }
	  put_user(0x6464 - ((j >> 1) & 0xf) * 0x606, (int *)arg);
	  return (*arg);
	  
	case SOUND_MIXER_LINE:
	case SOUND_MIXER_CD:
	case SOUND_MIXER_VIDEO:
	case SOUND_MIXER_LINE1:
	case SOUND_MIXER_PCM:
	  j = rdcodec(s, volreg[ch]);
	  if (j & 0x8000){
		put_user(0, (int *)arg);
		return (*arg);
	  }
	  put_user(0x6464 - (swab(j) & 0x1f1f) * 3, (int *)arg);
	  return (*arg);
	  
	case SOUND_MIXER_BASS:
	case SOUND_MIXER_TREBLE:
	  if (!(s->bd_id & CODEC_ID_BASSTREBLE))
		return EINVAL;
	  j = rdcodec(s, 0x08);
	  if (ch == SOUND_MIXER_BASS)
		j >>= 8;
	  put_user((((j & 15) * 100) / 15) * 0x101, (int *)arg);
	  return (*arg);
	  
	  /* SOUND_MIXER_RECLEV and SOUND_MIXER_IGAIN specify gain */
	case SOUND_MIXER_RECLEV:
	  j = rdcodec(s, 0x1c);
	  if (j & 0x8000){
		put_user(0, (int *)arg);
		return (*arg);
	  }
	  put_user((swab(j)  & 0xf0f) * 6 + 0xa0a, (int *)arg);
	  return (*arg);
	  
	case SOUND_MIXER_IGAIN:
	  if (!(s->bd_id & CODEC_ID_DEDICATEDMIC))
		return EINVAL;
	  j = rdcodec(s, 0x1e);
	  if (j & 0x8000){
		put_user(0, (int *)arg);
		return (*arg);
	  }
	  put_user((j & 0xf) * 0x606 + 0xa0a, (int *)arg);
	  return (*arg);
	  
	default:
	  return EINVAL;
	}
}

int mixer_wrch(snddev_info *s, unsigned int ich, int val) 
{
	int i;
	unsigned l1, r1;

	u_int ch = (ich & 0xff);
   	if(es_debug > 0) printf("mixer_wrch ch 0x%x val 0x%x\t",ch,(val & 0xffff));

	l1 = val & 0xff;
	r1 = (val >> 8) & 0xff;
	if(es_debug > 0) printf ("l1 0x%x r1 0x%x\n",l1,r1);
	if (l1 > 100)
		l1 = 100;
	if (r1 > 100)
		r1 = 100;
	s->mix_levels[ch] = ((u_int) r1 << 8) | l1;
	switch (ch)  {
	case SOUND_MIXER_LINE:
/* 	  printf("SOUND_MIXER_LINE\n"); */
	case SOUND_MIXER_CD:
/* 	  printf("SOUND_MIXER_CD\n"); */
	case SOUND_MIXER_VIDEO:
/* 	  printf("SOUND_MIXER_VIDEO\n"); */
	case SOUND_MIXER_LINE1:
/* 	  printf("SOUND_MIXEgR_LINE1\n");*/
	case SOUND_MIXER_PCM:
	  if (es_debug >0 ) printf("SOUND_MIXER_PCM\n");
	  if (l1 < 7 && r1 < 7) {
		wrcodec(s, volreg[ch], 0x8000);
		return 0;
	  }
	  if (l1 < 7)
		l1 = 7;
	  if (r1 < 7)
		r1 = 7;
	  wrcodec(s, volreg[ch], (((100 - l1) / 3) << 8) | ((100 - r1) / 3));
	  return 0;
	  
	case SOUND_MIXER_PHONEOUT:
	  if(es_debug > 0) printf("SOUND_MIXER_PHONEOUT\n");
	  if (!(s->bd_id & CODEC_ID_HEADPHONEOUT))
		return EINVAL;
	  /* fall through */
	case SOUND_MIXER_VOLUME:
	  if(es_debug > 0) printf("SOUND_MIXER_VOLUME\n");
#ifdef AC97_PESSIMISTIC
	  if (l1 < 7 && r1 < 7) {
		wrcodec(s, volreg[ch], 0x8000);
		return 0;
	  }
	  if (l1 < 7)
		l1 = 7;
	  if (r1 < 7)
		r1 = 7;
	  wrcodec(s, volreg[ch], (((100 - l1) / 3) << 8) | ((100 - r1) / 3));
	  /*	  es1371_src_write(s, volreg[ch], (((100 - l1) / 3) << 8) | ((100 - r1) / 3)); */
	  return 0;
#else /* AC97_PESSIMISTIC */
	  if (l1 < 4 && r1 < 4) {
		wrcodec(s, volreg[ch], 0x8000);
		return 0;
	  }
	  if (l1 < 4)
		l1 = 4;
	  if (r1 < 4)
		r1 = 4;
	  wrcodec(s, volreg[ch], ((2 * (100 - l1) / 3) << 8) | (2 * (100 - r1) / 3));
	  return 0;
#endif /* AC97_PESSIMISTIC */
	  
	case SOUND_MIXER_OGAIN:
	  if(es_debug > 0) printf("SOUND_MIXER_OGAIN\n");
	case SOUND_MIXER_PHONEIN:
	  if(es_debug > 0) printf("SOUND_MIXER_PHONEIN\n");
#ifdef AC97_PESSIMISTIC
	  wrcodec(s, volreg[ch], (l1 < 7) ? 0x8000 : (100 - l1) / 3);
	  return 0;
#else /* AC97_PESSIMISTIC */
	  wrcodec(s, volreg[ch], (l1 < 4) ? 0x8000 : (2 * (100 - l1) / 3));
	  return 0;
#endif /* AC97_PESSIMISTIC */
	  
	case SOUND_MIXER_SPEAKER:
	  wrcodec(s, 0x0a, (l1 < 10) ? 0x8000 : ((100 - l1) / 6) << 1);
	  return 0;
	  
	case SOUND_MIXER_MIC:
	  if(es_debug > 0) printf("SOUND_MIXER_MIC\n");
#ifdef AC97_PESSIMISTIC
	  if (l1 < 11) {
		wrcodec(s, 0x0e, 0x8000);
		return 0;
	  }
	  i = 0;
	  if (l1 >= 27) {
		l1 -= 27;
		i = 0x40;
	  }
	  if (l1 < 11) 
		l1 = 11;
	  wrcodec(s, 0x0e, ((73 - l1) / 2) | i);
	  return 0;
#else /* AC97_PESSIMISTIC */
	  if (l1 < 9) {
		wrcodec(s, 0x0e, 0x8000);
		return 0;
	  }
	  i = 0;
	  if (l1 >= 13) {
		l1 -= 13;
		i = 0x40;
	  }
	  if (l1 < 9) 
		l1 = 9;
	  wrcodec(s, 0x0e, (((87 - l1) * 4) / 5) | i);
	  return 0;
#endif /* AC97_PESSIMISTIC */
	  
	case SOUND_MIXER_BASS:
	  if(es_debug > 0) printf("SOUND_MIXER_BASS\n");
	  val = ((l1 * 15) / 100) & 0xf;
	  wrcodec(s, 0x08, (rdcodec(s, 0x08) & 0x00ff) | (val << 8));
	  return 0;
	  
	case SOUND_MIXER_TREBLE:
	  if(es_debug > 0) printf("SOUND_MIXER_TREBLE\n");
	  val = ((l1 * 15) / 100) & 0xf;
	  wrcodec(s, 0x08, (rdcodec(s, 0x08) & 0xff00) | val);
	  return 0;
	  
	  /* SOUND_MIXER_RECLEV and SOUND_MIXER_IGAIN specify gain */
	case SOUND_MIXER_RECLEV:
	  if(es_debug > 0) printf("SOUND_MIXER_RECLEV\n");
	  if (l1 < 10 || r1 < 10) {
		wrcodec(s, 0x1c, 0x8000);
		return 0;
	  }
	  if (l1 < 10)
		l1 = 10;
	  if (r1 < 10)
		r1 = 10;
	  wrcodec(s, 0x1c, (((l1 - 10) / 6) << 8) | ((r1 - 10) / 6));
	  return 0;
	  
	case SOUND_MIXER_IGAIN:
	  if(es_debug > 0) printf("SOUND_MIXER_IGAIN\n");
	  if (!(s->bd_id & CODEC_ID_DEDICATEDMIC))
		return EINVAL;
	  wrcodec(s, 0x1e, (l1 < 10) ? 0x8000 : ((l1 - 10) / 6) & 0xf);
	  return 0;
	  
	default:
	  return EINVAL;
	}
}


/* --------------------------------------------------------------------- */

#if 0
/* hmm for some reason I changed this in the es1370 code ... should wrcodec handle this */
/* make sure to find out where this is called from */
static int
write_codec_1371(snddev_info *d, u_char i, u_char data)
{
	struct es_info *es = (struct es_info *)d->device_data;
	int		wait = 100;	/* 100 msec timeout */
	u_int32_t ret;
	do {
		if ((ret = bus_space_read_4(es->st, es->sh, ES1371_REG_STATUS) &
		      ES1371_CSTAT) == 0) {
			bus_space_write_2(es->st, es->sh, ES1371_REG_CODEC,
				((u_short)i << ES1371_CODEC_INDEX_SHIFT) | data);
			return (0);
		}
		if(es_debug > 0) printf("write_codec ret 0x%x ret & ES1371_CSTAT 0x%x\n",ret,ret & STAT_CSTAT);
		DELAY(1000);
		/* tsleep(&wait, PZERO, "sndaw", hz / 1000); */
	} while (--wait);
	printf("pcm: write_codec timed out\n");
	return (-1);
}
#endif


void wrcodec(snddev_info *s, unsigned addr, unsigned data)
{
  /*	unsigned long flags; */
  int sl;
    unsigned t, x;
	struct es_info *es = (struct es_info *)s->device_data;

	if(es_debug > 0) printf("wrcodec addr 0x%x data 0x%x\n",addr,data);

	for (t = 0; t < 0x1000; t++)
	  if(!(bus_space_read_4(es->st, es->sh,(ES1371_REG_CODEC & CODEC_WIP))))
			break;
	/*	spin_lock_irqsave(&s->lock, flags); */
	sl = spltty();
	/* save the current state for later */
 	x =  bus_space_read_4(es->st, es->sh, ES1371_REG_SMPRATE);
	/* enable SRC state data in SRC mux */
	bus_space_write_4(es->st, es->sh, ES1371_REG_SMPRATE,
					  (es1371_wait_src_ready(s) & (SRC_DIS | SRC_DDAC1 | SRC_DDAC2 | SRC_DADC)));	  
	/* wait for a SAFE time to write addr/data and then do it, dammit */
	for (t = 0; t < 0x1000; t++)
	  if (( bus_space_read_4(es->st, es->sh, ES1371_REG_SMPRATE) & 0x00070000) == 0x00010000) 
		break;
	
	if(es_debug > 2) printf("one b_s_w: 0x%x 0x%x 0x%x\n",es->sh,ES1371_REG_CODEC,
						 ((addr << CODEC_POADD_SHIFT) & CODEC_POADD_MASK) |
						 ((data << CODEC_PODAT_SHIFT) & CODEC_PODAT_MASK));
	
	bus_space_write_4(es->st, es->sh,ES1371_REG_CODEC,
					  ((addr << CODEC_POADD_SHIFT) & CODEC_POADD_MASK) |
					  ((data << CODEC_PODAT_SHIFT) & CODEC_PODAT_MASK));
	/* restore SRC reg */
	es1371_wait_src_ready(s);
	if(es_debug > 2) printf("two b_s_w: 0x%x 0x%x 0x%x\n",es->sh,ES1371_REG_SMPRATE,x);
	bus_space_write_4(es->st, es->sh,ES1371_REG_SMPRATE,x);
	/*	spin_unlock_irqrestore(&s->lock, flags); */
	splx(sl);
}

unsigned rdcodec(snddev_info *s, unsigned addr)
{
  /*  unsigned long flags; */
  int sl;
  unsigned t, x;

  struct es_info *es = (struct es_info *)s->device_data;
  if(es_debug > 5) printf("rdcodec ");
  
  for (t = 0; t < 0x1000; t++)
	if (!(x = bus_space_read_4(es->st,es->sh,ES1371_REG_CODEC) & CODEC_WIP))
	  break;
  sl = spltty();
  /* save the current state for later */
  x =  bus_space_read_4(es->st, es->sh, ES1371_REG_SMPRATE);
  /* enable SRC state data in SRC mux */
  bus_space_write_4(es->st, es->sh, ES1371_REG_SMPRATE,
					(es1371_wait_src_ready(s) & (SRC_DIS | SRC_DDAC1 | SRC_DDAC2 | SRC_DADC)));	
  /* wait for a SAFE time to write addr/data and then do it, dammit */
  for (t = 0; t < 0x1000; t++)
	if (( bus_space_read_4(es->st, es->sh, ES1371_REG_SMPRATE) & 0x00070000) == 0x00010000) 
	  break;

  bus_space_write_4(es->st, es->sh,ES1371_REG_CODEC,
					((addr << CODEC_POADD_SHIFT) & CODEC_POADD_MASK) | CODEC_PORD);

  /* restore SRC reg */
  es1371_wait_src_ready(s);
  bus_space_write_4(es->st,es->sh,ES1371_REG_SMPRATE,x);
  splx(sl);
  /* now wait for the stinkin' data (RDY) */
  for (t = 0; t < 0x1000; t++)
	if ((x = bus_space_read_4(es->st,es->sh,ES1371_REG_CODEC)) & CODEC_RDY)
	  break;
  if(es_debug > 5) printf("0x%x ret 0x%x\n",x,((x & CODEC_PIDAT_MASK) >> CODEC_PIDAT_SHIFT));
  return ((x & CODEC_PIDAT_MASK) >> CODEC_PIDAT_SHIFT);
}



int 
mixer_ioctl_1371(snddev_info *s, u_long cmd, caddr_t data, int fflag, struct proc *p)
{
  int cmdi;
  int val;	
  int *arg = (int *)data;
  
  val = *(int *)data;
  cmdi = cmd & 0xff;

  if(es_debug > 0) printf("mixer_ioctl_1371 cmd 0x%x cmdi 0x%x write %s read %s ",(u_int)cmd,cmdi,
					   (((cmd & MIXER_WRITE(0)) == MIXER_WRITE(0))?"yes":"no"),
					   (((cmd & MIXER_READ(0)) == MIXER_READ(0))?"yes":"no"));

  if (cmdi == OSS_GETVERSION){
	put_user(SOUND_VERSION, (int *)arg);
	return (0);
  }

  if ((cmd & MIXER_READ(0)) == MIXER_READ(0)){
	switch (cmdi)  {
	case SOUND_MIXER_RECSRC: /* Arg contains a bit for each recording source */
	  if (es_debug > 4) printf("mixer_ioctl_1371: SOUND_MIXER_RECSRC\n");
		put_user(recsrc[rdcodec(s, ES1371_REG_RECSRC) & 7], (int *)arg);
		break;
	case SOUND_MIXER_DEVMASK: /* Arg contains a bit for each supported device */
	  put_user(SOUND_MASK_LINE | SOUND_MASK_CD | SOUND_MASK_VIDEO |
			   SOUND_MASK_LINE1 | SOUND_MASK_PCM | SOUND_MASK_VOLUME |
			   SOUND_MASK_OGAIN | SOUND_MASK_PHONEIN | SOUND_MASK_SPEAKER |
			   SOUND_MASK_MIC | SOUND_MASK_RECLEV |
			   ((s->bd_id & CODEC_ID_BASSTREBLE) ? (SOUND_MASK_BASS | SOUND_MASK_TREBLE) : 0) |
			   ((s->bd_id & CODEC_ID_HEADPHONEOUT) ? SOUND_MASK_PHONEOUT : 0) |
			   ((s->bd_id & CODEC_ID_DEDICATEDMIC) ? SOUND_MASK_IGAIN : 0), (int *)arg);
	  if (es_debug > 4) printf("mixer_ioctl_1371: SOUND_MIXER_DEVMASK s->bd_id 0x%x arg 0x%x\n",s->bd_id,*arg);
	  break;
	case SOUND_MIXER_RECMASK: /* Arg contains a bit for each supported recording source */
	  if (es_debug > 4) printf("mixer_ioctl_1371: SOUND_MIXER_RECMASK\n");
	  put_user(SOUND_MASK_MIC | SOUND_MASK_CD | SOUND_MASK_VIDEO | SOUND_MASK_LINE1 |
			   SOUND_MASK_LINE | SOUND_MASK_VOLUME | SOUND_MASK_PHONEOUT |
			   SOUND_MASK_PHONEIN, (int *)arg);
	  break;
	case SOUND_MIXER_STEREODEVS: /* Mixer channels supporting stereo */
	  if (es_debug > 4) printf("mixer_ioctl_1371: SOUND_MIXER_STEREODEVS\n");
	  put_user(SOUND_MASK_LINE | SOUND_MASK_CD | SOUND_MASK_VIDEO |
			   SOUND_MASK_LINE1 | SOUND_MASK_PCM | SOUND_MASK_VOLUME |
			   SOUND_MASK_PHONEOUT | SOUND_MASK_RECLEV, (int *)arg);
	  break;
	case SOUND_MIXER_CAPS:
	  if (es_debug > 4) printf("mixer_ioctl_1371: SOUND_MIXER_CAPS\n");
	  put_user(SOUND_CAP_EXCL_INPUT, (int *)arg);
	  break;
	default:
	  if (es_debug > 4) printf("mixer_ioctl_1371: default\n");
	  cmdi = cmd & 0xff;
	  if (cmdi >= SOUND_MIXER_NRDEVICES)
		return EINVAL;
	  { int ret;
	   ret = mixer_rdch(s, cmdi, arg);
	   if(es_debug > 0) printf("READ done ret 0x%x arg 0x%x\n",ret,*arg);
	  }
	}
  } 
  if  ((cmd & MIXER_WRITE(0)) == MIXER_WRITE(0)){
	if(es_debug > 0) printf("WRITE cmdi 0x%x data 0x%x val 0x%x sizeof(val) %d\n",cmdi,*data,val,sizeof(val));
	switch (cmdi)  {
	  int i;
	case SOUND_MIXER_RECSRC: /* Arg contains a bit for each recording source */
	  /*	  get_user_ret(val, (int *)arg, EFAULT); */
	  i = hweight32(val);
	  if (es_debug > 0) printf("SOUND_MIXER_RECSRC i: %d\n",i);
	  if (i == 0)
		return 0; /*val = mixer_recmask(s);*/
	  else if (i > 1) 
		val &= ~recsrc[rdcodec(s, ES1371_REG_RECSRC) & 7];
	  for (i = 0; i < 8; i++) {
		if (val & recsrc[i]) {
		  wrcodec(s, ES1371_REG_RECSRC, 0x101 * i);
		  return 0;
		}
	  }
	  return 0;
	default:
	  if (es_debug > 0) printf("mixer_ioctl_1371 default cmdi: %d\n",cmdi);
	  if (cmdi >= SOUND_MIXER_NRDEVICES)
		return EINVAL;
	  /*	  get_user_ret(val, (int *)arg, EFAULT); */
	  if (mixer_wrch(s, cmdi, val))
		return EINVAL;
	  { int ret;
	  ret = mixer_rdch(s, cmdi, (int *)arg);
	  if(es_debug > 0) printf("WRITE done ret 0x%x arg 0x%x\n",ret,*arg);
	  }
	}
  }
  return (0);
}

u_int es1371_wait_src_ready(snddev_info *d){
  struct es_info *es = (struct es_info *)d->device_data;
  u_int t, r;
  
  for (t = 0; t < 500; t++) {
	if (!((r = bus_space_read_4(es->st, es->sh,ES1371_REG_SMPRATE)) & ES1371_SRC_RAM_BUSY)){
	  return r;
	}
	/* 	snd_delay(1); */
	DELAY(1000);
  }
  printf("es1371: wait source ready timeout 0x%x [0x%x]\n", ES1371_REG_SMPRATE, r);
  return 0;
}

static u_int es1371_src_read(snddev_info *d, 
							 u_short reg){
  struct es_info *es = (struct es_info *)d->device_data;
  unsigned int r;
  
  r = es1371_wait_src_ready(d) &
	(ES1371_SRC_DISABLE |
	 ES1371_DIS_P1 |
	 ES1371_DIS_P2 |
	 ES1371_DIS_R1);
  r |= ES1371_SRC_RAM_ADDRO(reg);
  bus_space_write_4(es->st, es->sh,ES1371_REG_SMPRATE,r);
  return ES1371_SRC_RAM_DATAI(es1371_wait_src_ready(d));
}

void es1371_src_write(snddev_info *d,
								 u_short reg, 
								 u_short data){
	struct es_info *es = (struct es_info *)d->device_data;
	u_int r;

	r = es1371_wait_src_ready(d) &
	    (ES1371_SRC_DISABLE | 
		 ES1371_DIS_P1 |
	     ES1371_DIS_P2 |
		 ES1371_DIS_R1);
	r |= ES1371_SRC_RAM_ADDRO(reg) |  ES1371_SRC_RAM_DATAO(data);
	if(es_debug > 1 ) printf("es1371_src_write 0x%x 0x%x\n",ES1371_REG_SMPRATE,r | ES1371_SRC_RAM_WE); 
	bus_space_write_4(es->st, es->sh,ES1371_REG_SMPRATE,r | ES1371_SRC_RAM_WE);
}

u_int 
es1371_adc_rate(snddev_info *d,
				u_int rate, 
				int set){
  u_int n, truncm, freq, result;
  
  if (rate > 48000)
	rate = 48000;
  if (rate < 4000)
	rate = 4000;
  n = rate / 3000;
  if ((1 << n) & ((1 << 15) | (1 << 13) | (1 << 11) | (1 << 9)))
	n--;
  truncm = (21 * n - 1) | 1;
  freq = ((48000UL << 15) / rate) * n;
  result = (48000UL << 15) / (freq / n);
  if (set) {
	if (rate >= 24000) {
	  if (truncm > 239)
		truncm = 239;
	  es1371_src_write(d, ES_SMPREG_ADC + ES_SMPREG_TRUNC_N,
					   (((239 - truncm) >> 1) << 9) | (n << 4));
	} else {
	  if (truncm > 119)
		truncm = 119;
	  es1371_src_write(d, ES_SMPREG_ADC + ES_SMPREG_TRUNC_N,
						   0x8000 | (((119 - truncm) >> 1) << 9) | (n << 4));
	}
	es1371_src_write(d, ES_SMPREG_ADC + ES_SMPREG_INT_REGS,
						 (es1371_src_read(d, 
										  ES_SMPREG_ADC +
										  ES_SMPREG_INT_REGS) &
						  0x00ff) | ((freq >> 5) & 0xfc00));
	es1371_src_write(d, ES_SMPREG_ADC + ES_SMPREG_VFREQ_FRAC, freq & 0x7fff);
	es1371_src_write(d, ES_SMPREG_VOL_ADC, n << 8);
	es1371_src_write(d, ES_SMPREG_VOL_ADC + 1, n << 8);
	}
	return result;
}

u_int
es1371_dac1_rate(snddev_info *d,
				 u_int rate,
				 int set){
  struct es_info *es = (struct es_info *)d->device_data;
  u_int freq, r, result;
  
  
  if (rate > 48000)
	rate = 48000;
  if (rate < 4000)
	rate = 4000;
  freq = (rate << 15) / 3000;
  result = (freq * 3000) >> 15;
  if (set) {
	r = (es1371_wait_src_ready(d) & (ES1371_SRC_DISABLE | ES1371_DIS_P2 | ES1371_DIS_R1)) | ES1371_DIS_P1;
	bus_space_write_4(es->st, es->sh,ES1371_REG_SMPRATE,r);
	if(es_debug > 0) printf("dac1_rate 0x%x\n",bus_space_read_4(es->st, es->sh,ES1371_REG_SMPRATE));
	es1371_src_write(d, ES_SMPREG_DAC1 + 
					 ES_SMPREG_INT_REGS,
					 (es1371_src_read(d, 
									  ES_SMPREG_DAC1 +
									  ES_SMPREG_INT_REGS) &
					  0x00ff) | ((freq >> 5) & 0xfc00));
	es1371_src_write(d, ES_SMPREG_DAC1 + ES_SMPREG_VFREQ_FRAC, freq & 0x7fff);
	r = (es1371_wait_src_ready(d) & (ES1371_SRC_DISABLE | ES1371_DIS_P2 | ES1371_DIS_R1));
	bus_space_write_4(es->st, es->sh,ES1371_REG_SMPRATE,r);
	if(es_debug > 0) printf("dac1_rate 0x%x\n",bus_space_read_4(es->st, es->sh,ES1371_REG_SMPRATE));
  }
  return result;
}

u_int
es1371_dac2_rate(snddev_info *d,
				 u_int rate, 
				 int set){
  u_int freq, r, result;
  struct es_info *es = (struct es_info *)d->device_data;
  
  if (rate > 48000)
	rate = 48000;
  if (rate < 4000)
	rate = 4000;
  freq = (rate << 15) / 3000;
  result = (freq * 3000) >> 15;
  if (set) {
	r = (es1371_wait_src_ready(d) & (ES1371_SRC_DISABLE | ES1371_DIS_P1 | ES1371_DIS_R1)) | ES1371_DIS_P2;
	bus_space_write_4(es->st, es->sh,ES1371_REG_SMPRATE,r);
	if(es_debug > 0) printf("dac2_rate 0x%x\n",bus_space_read_4(es->st, es->sh,ES1371_REG_SMPRATE));
	es1371_src_write(d, ES_SMPREG_DAC2 + ES_SMPREG_INT_REGS,
					 (es1371_src_read(d, ES_SMPREG_DAC2 +
									  ES_SMPREG_INT_REGS) & 
					  0x00ff) | ((freq >> 5) & 0xfc00));
	es1371_src_write(d, ES_SMPREG_DAC2 + ES_SMPREG_VFREQ_FRAC, freq & 0x7fff);
	r = (es1371_wait_src_ready(d) & (ES1371_SRC_DISABLE | ES1371_DIS_P1 | ES1371_DIS_R1));
	bus_space_write_4(es->st, es->sh,ES1371_REG_SMPRATE,r);
	if(es_debug > 0) printf("dac2_rate 0x%x\n",bus_space_read_4(es->st, es->sh,ES1371_REG_SMPRATE));
  }
  return result;
}

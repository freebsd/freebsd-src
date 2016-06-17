
/*
 *  ATI Mach64 CT/VT/GT/LT Support
 */

#include <linux/fb.h>

#include <asm/io.h>

#include <video/fbcon.h>

#include "mach64.h"
#include "atyfb.h"


/* FIXME: remove the FAIL definition */
#define FAIL(x) do { printk(x "\n"); return -EINVAL; } while (0)

static void aty_st_pll(int offset, u8 val, const struct fb_info_aty *info) stdcall;

static int aty_valid_pll_ct(const struct fb_info_aty *info, u32 vclk_per,
			    struct pll_ct *pll);
static int aty_dsp_gt(const struct fb_info_aty *info, u8 bpp, u32 stretch,
		      struct pll_ct *pll);
static int aty_var_to_pll_ct(const struct fb_info_aty *info, u32 vclk_per,
			     u8 bpp, u32 stretch, union aty_pll *pll);
static u32 aty_pll_ct_to_var(const struct fb_info_aty *info,
			     const union aty_pll *pll);

/*
 * ATI Mach64 CT clock synthesis description.
 *
 * All clocks on the Mach64 can be calculated using the same principle:
 *
 *       XTALIN * x * FB_DIV
 * CLK = ----------------------
 *       PLL_REF_DIV * POST_DIV
 * 
 * XTALIN is a fixed speed clock. Common speeds are 14.31 MHz and 29.50 MHz.
 * PLL_REF_DIV can be set by the user, but is the same for all clocks.
 * FB_DIV can be set by the user for each clock individually, it should be set
 * between 128 and 255, the chip will generate a bad clock signal for too low
 * values.
 * x depends on the type of clock; usually it is 2, but for the MCLK it can also
 * be set to 4.
 * POST_DIV can be set by the user for each clock individually, Possible values
 * are 1,2,4,8 and for some clocks other values are available too.
 * CLK is of course the clock speed that is generated.
 *
 * The Mach64 has these clocks:
 *
 * MCLK			The clock rate of the chip
 * XCLK			The clock rate of the on-chip memory
 * VCLK0		First pixel clock of first CRT controller
 * VCLK1    Second pixel clock of first CRT controller
 * VCLK2		Third pixel clock of first CRT controller
 * VCLK3    Fourth pixel clock of first CRT controller
 * VCLK			Selected pixel clock, one of VCLK0, VCLK1, VCLK2, VCLK3
 * V2CLK		Pixel clock of the second CRT controller.
 * SCLK			Multi-purpose clock
 *
 * - MCLK and XCLK use the same FB_DIV
 * - VCLK0 .. VCLK3 use the same FB_DIV
 * - V2CLK is needed when the second CRTC is used (can be used for dualhead);
 *   i.e. CRT monitor connected to laptop has different resolution than built
 *   in LCD monitor.
 * - SCLK is not available on all cards; it is known to exist on the Rage LT-PRO,
 *   Rage XL and Rage Mobility. It is known not to exist on the Mach64 VT.
 * - V2CLK is not available on all cards, most likely only the Rage LT-PRO,
 *   the Rage XL and the Rage Mobility
 *
 * SCLK can be used to:
 * - Clock the chip instead of MCLK
 * - Replace XTALIN with a user defined frequency
 * - Generate the pixel clock for the LCD monitor (instead of VCLK)
 */

 /*
  * It can be quite hard to calculate XCLK and MCLK if they don't run at the
  * same frequency. Luckily, until now all cards that need asynchrone clock
  * speeds seem to have SCLK.
  * So this driver uses SCLK to clock the chip and XCLK to clock the memory.
  */

static u8 postdividers[] = {1,2,4,8,3};

u8 stdcall aty_ld_pll(int offset, const struct fb_info_aty *info)
{
    unsigned long addr;

    addr = info->ati_regbase + CLOCK_CNTL + 1;
    /* write addr byte */
    writeb((offset << 2) | PLL_WR_EN,addr);
    addr++;
    /* read the register value */
    return readb(addr);
}

static void stdcall aty_st_pll(int offset, u8 val, const struct fb_info_aty *info)
{
    unsigned long addr;
    addr = info->ati_regbase + CLOCK_CNTL + 1;
    /* write addr byte */
    writeb((offset << 2) | PLL_WR_EN,addr);
    addr++;
    /* write the register value */
    writeb(val,addr);
    addr--;
    /* Disable write access */
    writeb(offset << 2,addr);
}

/* ------------------------------------------------------------------------- */

    /*
     *  PLL programming (Mach64 CT family)
     */

/*
 * This procedure sets the display fifo. The display fifo is a buffer that
 * contains data read from the video memory that waits to be processed by
 * the CRT controller.
 *
 * On the more modern Mach64 variants, the chip doesn't calculate the
 * interval after which the display fifo has to be reloaded from memory
 * automatically, the driver has to do it instead.
 */


#define Maximum_DSP_PRECISION 7

static int aty_dsp_gt(const struct fb_info_aty *info, u8 bpp,
		      u32 width, struct pll_ct *pll) {

    u32 multiplier,divider,ras_multiplier,ras_divider,tmp;
    u32 dsp_on,dsp_off,dsp_xclks;
    u8 vshift,xshift;
    s8 dsp_precision;

    multiplier = ((u32)info->mclk_fb_div)*pll->vclk_post_div_real;
    divider = ((u32)pll->vclk_fb_div)*info->xclk_post_div_real;

    ras_multiplier = info->xclkmaxrasdelay;
    ras_divider = 1;

    if (bpp>=8)
        divider = divider * (bpp >> 2);
	
    vshift = (6 - 2);	/* FIFO is 64 bits wide in accelerator mode ... */

    if (bpp == 0)
        vshift--;	/* ... but only 32 bits in VGA mode. */

#ifdef CONFIG_FB_ATY_GENERIC_LCD
    if (width != 0) {
        multiplier = multiplier * info->lcd_width;
        divider = divider * width;

        ras_multiplier = ras_multiplier * info->lcd_width;
        ras_divider = ras_divider * width;
    };
#endif

    /* If we don't do this, 32 bits for multiplier & divider won't be
       enough in certain situations! */
    while (((multiplier | divider) & 1) == 0) {
	multiplier = multiplier >> 1;
	divider = divider >> 1;
    };

    /* Determine DSP precision first */
    tmp = ((multiplier * info->fifo_size) << vshift) / divider;
    for (dsp_precision = -5;  tmp;  dsp_precision++)
        tmp >>= 1;
    if (dsp_precision < 0)
        dsp_precision = 0;
    else if (dsp_precision > Maximum_DSP_PRECISION)
        dsp_precision = Maximum_DSP_PRECISION;

    xshift = 6 - dsp_precision;
    vshift += xshift;

    /* Move on to dsp_off */
    dsp_off = ((multiplier * (info->fifo_size - 1)) << vshift) / divider -
	      (1 << (vshift - xshift));

    /* Next is dsp_on */
//    if (bpp == 0)
//        dsp_on = ((multiplier * 20 << vshift) + divider) / divider;
//    else {
        dsp_on = ((multiplier << vshift) + divider) / divider;
        tmp = ((ras_multiplier << xshift) + ras_divider) / ras_divider;
        if (dsp_on < tmp)
            dsp_on = tmp;
        dsp_on = dsp_on + (tmp * 2) + (info->xclkpagefaultdelay << xshift);
//    };

    /* Calculate rounding factor and apply it to dsp_on */
    tmp = ((1 << (Maximum_DSP_PRECISION - dsp_precision)) - 1) >> 1;
    dsp_on = ((dsp_on + tmp) / (tmp + 1)) * (tmp + 1);

    if (dsp_on >= ((dsp_off / (tmp + 1)) * (tmp + 1)))
    {
        dsp_on = dsp_off - (multiplier << vshift) / divider;
        dsp_on = (dsp_on / (tmp + 1)) * (tmp + 1);
    }

    /* Last but not least:  dsp_xclks */
    dsp_xclks = ((multiplier << (vshift + 5)) + divider) / divider;

    /* Get register values. */
    pll->dsp_on_off = (dsp_on << 16) + dsp_off;
    pll->dsp_config = (dsp_precision << 20) | (info->dsp_loop_latency << 16) |
                      dsp_xclks;
    return 0;
};

static int aty_valid_pll_ct(const struct fb_info_aty *info, u32 vclk_per,
			    struct pll_ct *pll)
{
    u32 q;

    /* FIXME: use the VTB/GTB /{3,6,12} post dividers if they're better suited */
    q = info->ref_clk_per*info->pll_ref_div*4/vclk_per;	/* actually 8*q */
    if (q < 16*8 || q > 255*8)
        FAIL("vclk out of range");
    else {
        pll->vclk_post_div  = (q < 128*8);
        pll->vclk_post_div += (q <  64*8);
        pll->vclk_post_div += (q <  32*8);
    };
    pll->vclk_post_div_real = postdividers[pll->vclk_post_div];
//    pll->vclk_post_div <<= 6;
    pll->vclk_fb_div = q*pll->vclk_post_div_real/8;
    pll->pll_vclk_cntl = 0x03;	/* VCLK = PLL_VCLK/VCLKx_POST */
    return 0;
}

static int aty_var_to_pll_ct(const struct fb_info_aty *info, u32 vclk_per,
			     u8 bpp, u32 width, union aty_pll *pll)
{
    int err;

    if ((err = aty_valid_pll_ct(info, vclk_per, &pll->ct)))
        return err;
    if (M64_HAS(GTB_DSP) && (err = aty_dsp_gt(info, bpp, width, &pll->ct)))
        return err;
    return 0;
}

static u32 aty_pll_ct_to_var(const struct fb_info_aty *info,
			     const union aty_pll *pll)
{
    u32 ref_clk_per = info->ref_clk_per;
    u8 pll_ref_div = info->pll_ref_div;
    u8 vclk_fb_div = pll->ct.vclk_fb_div;
    u8 vclk_post_div = pll->ct.vclk_post_div_real;

    return ref_clk_per*pll_ref_div*vclk_post_div/vclk_fb_div/2;
}

void aty_set_pll_ct(const struct fb_info_aty *info, const union aty_pll *pll)
{
    u8 a;
    aty_st_pll(PLL_VCLK_CNTL, pll->ct.pll_vclk_cntl, info);
    a = aty_ld_pll(VCLK_POST_DIV, info) & ~3;
    aty_st_pll(VCLK_POST_DIV, a | pll->ct.vclk_post_div, info);
    aty_st_pll(VCLK0_FB_DIV, pll->ct.vclk_fb_div, info);

    if (M64_HAS(GTB_DSP)) {
        aty_st_le32(DSP_CONFIG, pll->ct.dsp_config, info);
        aty_st_le32(DSP_ON_OFF, pll->ct.dsp_on_off, info);
    }
}


static void __init aty_init_pll_ct(struct fb_info_aty *info) {
    u8 pll_ref_div,pll_gen_cntl,pll_ext_cntl;
    u8 mpost_div,xpost_div;
    u8 sclk_post_div_real,sclk_fb_div,spll_cntl2;
    u32 q,i;
    u32 mc,trp,trcd,tcrd,tras;

    mc = aty_ld_le32(MEM_CNTL, info);
    trp = (mc & 0x300) >> 8;
    trcd = (mc & 0xc00) >> 10;
    tcrd = (mc & 0x1000) >> 12;    tras = (mc & 0x70000) >> 16;
    info->xclkpagefaultdelay = trcd + tcrd + trp + 2;
    info->xclkmaxrasdelay = tras + trp + 2;

    if (M64_HAS(FIFO_24)) {
        info->fifo_size = 24;
        info->xclkpagefaultdelay += 2;
        info->xclkmaxrasdelay += 3;
    } else {
        info->fifo_size = 32;
    };

    switch (info->ram_type) {
        case DRAM:
            if (info->total_vram<=1*1024*1024) {
                info->dsp_loop_latency = 10;
            } else {
                info->dsp_loop_latency = 8;
                info->xclkpagefaultdelay += 2;
            };
            break;
        case EDO:
        case PSEUDO_EDO:
            if (info->total_vram<=1*1024*1024) {
                info->dsp_loop_latency = 9;
            } else {
                info->dsp_loop_latency = 8;
                info->xclkpagefaultdelay += 1;
            };
            break;
        case SDRAM:
            if (info->total_vram<=1*1024*1024) {
                info->dsp_loop_latency = 11;
            } else {
                info->dsp_loop_latency = 10;
                info->xclkpagefaultdelay += 1;
            };
            break;
        case SGRAM:
            info->dsp_loop_latency = 8;
            info->xclkpagefaultdelay += 3;
            break;
        default:
            info->dsp_loop_latency = 11;
            info->xclkpagefaultdelay += 3;
            break;
    };

    if (info->xclkmaxrasdelay <= info->xclkpagefaultdelay)
        info->xclkmaxrasdelay = info->xclkpagefaultdelay + 1;

    /* Exit if the user does not want us to tamper with the clock
       rates of her chip. */
    if (info->mclk_per == 0) {
        u16 mclk_fb_div;
        u8 pll_ext_cntl;

        info->pll_ref_div = aty_ld_pll(PLL_REF_DIV, info);
        pll_ext_cntl = aty_ld_pll(PLL_EXT_CNTL, info);
        info->xclk_post_div_real = postdividers[pll_ext_cntl & 7];
        mclk_fb_div = aty_ld_pll(MCLK_FB_DIV, info);
        if (pll_ext_cntl & 8)
            mclk_fb_div <<= 1;
        info->mclk_fb_div = mclk_fb_div;
        return;
    };

    pll_ref_div = info->pll_per*2*255/info->ref_clk_per;
    info->pll_ref_div = pll_ref_div;

    /* FIXME: use the VTB/GTB /3 post divider if it's better suited */
    q = info->ref_clk_per*pll_ref_div*4/info->xclk_per;	/* actually 8*q */
    if (q < 16*8 || q > 255*8) {
        printk(KERN_CRIT "xclk out of range\n");
        return;
    } else {
        xpost_div  = (q < 128*8);
        xpost_div += (q <  64*8);
        xpost_div += (q <  32*8);
    };
    info->xclk_post_div_real = postdividers[xpost_div];
    info->mclk_fb_div = q*info->xclk_post_div_real/8;

    if (M64_HAS(SDRAM_MAGIC_PLL) && (info->ram_type >= SDRAM))
        pll_gen_cntl = 0x04;
    else
	/* The Rage Mobility M1 needs bit 3 set...*/
	/* original: pll_gen_cntl = 0x84 */
        pll_gen_cntl = 0x8C;

    if (M64_HAS(MAGIC_POSTDIV))
        pll_ext_cntl = 0;
    else
       	pll_ext_cntl = xpost_div;

    if (info->mclk_per == info->xclk_per)
        pll_gen_cntl |= xpost_div<<4; /* mclk == xclk */
    else {
	/* 
	 * The chip clock is not equal to the memory clock.
	 * Therefore we will use sclk to clock the chip.
	 */
        pll_gen_cntl |= 6<<4;	/* mclk == sclk*/

        q = info->ref_clk_per*pll_ref_div*4/info->mclk_per;	/* actually 8*q */
        if (q < 16*8 || q > 255*8) {
	    printk(KERN_CRIT "mclk out of range\n");
            return;
        } else {
            mpost_div  = (q < 128*8);
            mpost_div += (q <  64*8);
            mpost_div += (q <  32*8);
        };
        sclk_post_div_real = postdividers[mpost_div];
        sclk_fb_div = q*sclk_post_div_real/8;
        spll_cntl2 = mpost_div << 4;
	/*
         * This disables the sclk, crashes the computer as reported:
         * aty_st_pll(SPLL_CNTL2, 3, info);
	 *
         * So it seems the sclk must be enabled before it is used;
         * so PLL_GEN_CNTL must be programmed *after* the sclk.
	 */
        aty_st_pll(SCLK_FB_DIV, sclk_fb_div, info);
        aty_st_pll(SPLL_CNTL2, spll_cntl2, info);
	/* 
	 * The sclk has been started. However, I believe the first clock 
	 * ticks it generates are not very stable. Hope this primitive loop
	 * helps for Rage Mobilities that sometimes crash when
	 * we switch to sclk. (Daniel Mantione, 13-05-2003)
	 */
        for (i=0;i<=0x1ffff;i++);
    };

    aty_st_pll(PLL_REF_DIV, pll_ref_div, info);
    aty_st_pll(PLL_GEN_CNTL, pll_gen_cntl, info);
    aty_st_pll(MCLK_FB_DIV, info->mclk_fb_div, info);
    aty_st_pll(PLL_EXT_CNTL, pll_ext_cntl, info);
    /* Disable the extra precision pixel clock controls since we do not
       use them. */
    aty_st_pll(EXT_VPLL_CNTL, aty_ld_pll(EXT_VPLL_CNTL, info) &
	                      ~(EXT_VPLL_EN | EXT_VPLL_VGA_EN |
			        EXT_VPLL_INSYNC), info);
#if 0
    /* This code causes problems on the Rage Mobility M1
       and seems unnecessary. Comments wanted! */
    if (M64_HAS(GTB_DSP)) {
        if (M64_HAS(XL_DLL))
            aty_st_pll(DLL_CNTL, 0x80, info);
        else if (info->ram_type >= SDRAM)
            aty_st_pll(DLL_CNTL, 0xa6, info);
        else
            aty_st_pll(DLL_CNTL, 0xa0, info);
        aty_st_pll(VFC_CNTL, 0x1b, info);
    };
#endif
};

static int dummy(void)
{
    return 0;
}

const struct aty_dac_ops aty_dac_ct = {
    set_dac:	(void *)dummy,
};

const struct aty_pll_ops aty_pll_ct = {
    var_to_pll:	aty_var_to_pll_ct,
    pll_to_var:	aty_pll_ct_to_var,
    set_pll:	aty_set_pll_ct,
    init_pll:	aty_init_pll_ct
};


/*
 *   linux/drivers/video/fbmon.c
 *
 *  Copyright (C) 1999 James Simmons
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 * Notes:
 *      This code handles the different types of monitors that are out their. 
 *   Most video cards for example can support a mode like 800x600 but fix
 *   frequency monitors can't. So the code here checks if the monitor can
 *   support the mode as well as the card can. Fbmonospecs takes on 
 *   different meaning with different types of monitors. For multifrequency 
 *   monitors fbmonospecs represents the range of frequencies the monitor 
 *   can support. Only one fbmonospec needs to be allocated. The fbmonospecs 
 *   pointer in fb_info points to this one. If you specific a mode that has 
 *   timing greater than the allowed range then setting the video mode will 
 *   fail. With multifrequency monitors you can set any mode you like as long
 *   as you have a programmable clock on the video card. 
 *       With fixed frequency monitors you have only a SET of very narrow 
 *   allowed frequency ranges. So for a fixed fequency monitor you have a 
 *   array of fbmonospecs. The fbmonospecs in fb_info represents the 
 *   monitor frequency for the CURRENT mode. If you change the mode and ask
 *   for fbmonospecs you will NOT get the same values as before. Note this
 *   is not true for multifrequency monitors where you do get the same 
 *   fbmonospecs each time. Also the values in each fbmonospecs represent the 
 *   very narrow frequency band for range. Well you can't have exactly the 
 *   same frequencies from fixed monitor. So some tolerance is excepted.
 *       By DEFAULT all monitors are assumed fixed frequency since they are so
 *   easy to fry or screw up a mode with. Just try setting a 800x600 mode on
 *   one. After you boot you can run a simple program the tells what kind of 
 *   monitor you have. If you have a multifrequency monitor then you can set 
 *   any mode size you like as long as your video card has a programmable clock.
 *   By default also besides assuming you have a fixed frequency monitor it 
 *   assumes the monitor only supports lower modes. This way for example you
 *   can't set a 1280x1024 mode on a fixed frequency monitor that can only 
 *   support up to 1024x768.
 *
 */
#include <linux/tty.h>
#include <linux/fb.h>
#include <linux/module.h>

int fbmon_valid_timings(u_int pixclock, u_int htotal, u_int vtotal,
                        const struct fb_info *fb_info)
{
#if 0
  /*
   * long long divisions .... $#%%#$
   */
    unsigned long long hpicos, vpicos;
    const unsigned long long _1e12 = 1000000000000ULL;
    const struct fb_monspecs *monspecs = &fb_info->monspecs;

    hpicos = (unsigned long long)htotal*(unsigned long long)pixclock;
    vpicos = (unsigned long long)vtotal*(unsigned long long)hpicos;
    if (!vpicos)
      return 0;
    
    if (monspecs->hfmin == 0)
      return 1;
    
    if (hpicos*monspecs->hfmin > _1e12 || hpicos*monspecs->hfmax < _1e12 ||
        vpicos*monspecs->vfmin > _1e12 || vpicos*monspecs->vfmax < _1e12)
      return 0;
#endif
    return 1;
}

int fbmon_dpms(const struct fb_info *fb_info)
{
  return fb_info->monspecs.dpms;
}

EXPORT_SYMBOL(fbmon_valid_timings);

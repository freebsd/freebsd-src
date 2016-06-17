/*
 * SiS 300/630/730/540/315/550/650/651/M650/661FX/M661FX/740/741/M741/330/760
 * frame buffer driver for Linux kernels 2.4.x and 2.5.x
 *
 * Copyright (C) 2001-2004 Thomas Winischhofer, Vienna, Austria.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the named License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA
 *
 * Author:   	Thomas Winischhofer <thomas@winischhofer.net>
 *
 * Author of code base:
 *		SiS (www.sis.com.tw)
 *	 	Copyright (C) 1999 Silicon Integrated Systems, Inc.
 *
 * See http://www.winischhofer.net/ for more information and updates
 *
 * Originally based on the VBE 2.0 compliant graphic boards framebuffer driver,
 * which is (c) 1998 Gerd Knorr <kraxel@goldbach.in-berlin.de>
 *
 */

#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/console.h>
#include <linux/selection.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/vmalloc.h>
#include <linux/vt_kern.h>
#include <linux/capability.h>
#include <linux/fs.h>
#include <linux/agp_backend.h>
#include <linux/types.h>
#include <asm/uaccess.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
#include <linux/spinlock.h>
#endif

#include "osdef.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
#include <video/sisfb.h>
#else
#include <linux/sisfb.h>
#endif

#include <asm/io.h>
#ifdef CONFIG_MTRR
#include <asm/mtrr.h>
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
#include <video/fbcon.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>
#include <video/fbcon-cfb24.h>
#include <video/fbcon-cfb32.h>
#endif

#include "vgatypes.h"
#include "sis_main.h"
#include "sis.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,3)
#error "This version of sisfb requires at least 2.6.3"
#else
#if 0
#define NEWFBDEV		/* Define this as soon as sysfs support has returned */
#endif
#endif
#endif

/* -------------------- Macro definitions ---------------------------- */

#undef SISFBDEBUG 	/* no debugging */

#ifdef SISFBDEBUG
#define DPRINTK(fmt, args...) printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args)
#else
#define DPRINTK(fmt, args...)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
#ifdef SISFBACCEL
#ifdef FBCON_HAS_CFB8
extern struct display_switch fbcon_sis8;
#endif
#ifdef FBCON_HAS_CFB16
extern struct display_switch fbcon_sis16;
#endif
#ifdef FBCON_HAS_CFB32
extern struct display_switch fbcon_sis32;
#endif
#endif
#endif

/* --------------- Hardware Access Routines -------------------------- */

/* These are used on x86 hardware during probe only */
static void __devinit
sisfb_set_reg4(u16 port, u32 data)
{
	outl((u32)(data & 0xffffffff), port);
}

static u32 __devinit
sisfb_get_reg3(u16 port)
{
	return((u32)inl(port));
}

/* ------------------ Internal helper routines ----------------- */

static BOOLEAN __devinit
sisfb_interpret_edid(struct sisfb_monitor *monitor, u8 *buffer)
{
	int i, j, xres, yres, refresh, index;
	u32 emodes;

	if(buffer[0] != 0x00 || buffer[1] != 0xff ||
	   buffer[2] != 0xff || buffer[3] != 0xff ||
	   buffer[4] != 0xff || buffer[5] != 0xff ||
	   buffer[6] != 0xff || buffer[7] != 0x00) {
	   printk(KERN_DEBUG "sisfb: Bad EDID header\n");
	   return FALSE;
	}

	if(buffer[0x12] != 0x01) {
	   printk(KERN_INFO "sisfb: EDID version %d not supported\n",
	   	buffer[0x12]);
	   return FALSE;
	}

	monitor->feature = buffer[0x18];

	if(!buffer[0x14] & 0x80) {
	   if(!(buffer[0x14] & 0x08)) {
	      printk(KERN_INFO "sisfb: WARNING: Monitor does not support separate syncs\n");
	   }
	}

	if(buffer[0x13] >= 0x01) {
	   /* EDID V1 rev 1 and 2: Search for monitor descriptor
	    * to extract ranges
	    */
	    j = 0x36;
	    for(i=0; i<4; i++) {
	       if(buffer[j]     == 0x00 && buffer[j + 1] == 0x00 &&
	          buffer[j + 2] == 0x00 && buffer[j + 3] == 0xfd &&
		  buffer[j + 4] == 0x00) {
		  monitor->hmin = buffer[j + 7];
		  monitor->hmax = buffer[j + 8];
		  monitor->vmin = buffer[j + 5];
		  monitor->vmax = buffer[j + 6];
		  monitor->dclockmax = buffer[j + 9] * 10 * 1000;
		  monitor->datavalid = TRUE;
		  break;
	       }
	       j += 18;
	    }
	}

	if(!monitor->datavalid) {
	   /* Otherwise: Get a range from the list of supported
	    * Estabished Timings. This is not entirely accurate,
	    * because fixed frequency monitors are not supported
	    * that way.
	    */
	   monitor->hmin = 65535; monitor->hmax = 0;
	   monitor->vmin = 65535; monitor->vmax = 0;
	   monitor->dclockmax = 0;
	   emodes = buffer[0x23] | (buffer[0x24] << 8) | (buffer[0x25] << 16);
	   for(i = 0; i < 13; i++) {
	      if(emodes & sisfb_ddcsmodes[i].mask) {
	         if(monitor->hmin > sisfb_ddcsmodes[i].h) monitor->hmin = sisfb_ddcsmodes[i].h;
		 if(monitor->hmax < sisfb_ddcsmodes[i].h) monitor->hmax = sisfb_ddcsmodes[i].h + 1;
		 if(monitor->vmin > sisfb_ddcsmodes[i].v) monitor->vmin = sisfb_ddcsmodes[i].v;
		 if(monitor->vmax < sisfb_ddcsmodes[i].v) monitor->vmax = sisfb_ddcsmodes[i].v;
		 if(monitor->dclockmax < sisfb_ddcsmodes[i].d) monitor->dclockmax = sisfb_ddcsmodes[i].d;
	      }
	   }
	   index = 0x26;
	   for(i = 0; i < 8; i++) {
	      xres = (buffer[index] + 31) * 8;
	      switch(buffer[index + 1] & 0xc0) {
	         case 0xc0: yres = (xres * 9) / 16; break;
	         case 0x80: yres = (xres * 4) /  5; break;
	         case 0x40: yres = (xres * 3) /  4; break;
	         default:   yres = xres;	    break;
	      }
	      refresh = (buffer[index + 1] & 0x3f) + 60;
	      if((xres >= 640) && (yres >= 480)) {
                 for(j = 0; j < 8; j++) {
	            if((xres == sisfb_ddcfmodes[j].x) &&
	               (yres == sisfb_ddcfmodes[j].y) &&
		       (refresh == sisfb_ddcfmodes[j].v)) {
		      if(monitor->hmin > sisfb_ddcfmodes[j].h) monitor->hmin = sisfb_ddcfmodes[j].h;
		      if(monitor->hmax < sisfb_ddcfmodes[j].h) monitor->hmax = sisfb_ddcfmodes[j].h + 1;
		      if(monitor->vmin > sisfb_ddcsmodes[j].v) monitor->vmin = sisfb_ddcsmodes[j].v;
		      if(monitor->vmax < sisfb_ddcsmodes[j].v) monitor->vmax = sisfb_ddcsmodes[j].v;
		      if(monitor->dclockmax < sisfb_ddcsmodes[j].d) monitor->dclockmax = sisfb_ddcsmodes[i].d;
	            }
	         }
	      }
	      index += 2;
           }
	   if((monitor->hmin <= monitor->hmax) && (monitor->vmin <= monitor->vmax)) {
	      monitor->datavalid = TRUE;
	   }
	}

 	return(monitor->datavalid);
}

static void __devinit
sisfb_handle_ddc(struct sisfb_monitor *monitor, int crtno)
{
	USHORT  temp, i, realcrtno = crtno;
   	u8      buffer[256];

	monitor->datavalid = FALSE;

	if(crtno) {
       	   if(ivideo.vbflags & CRT2_LCD)      realcrtno = 1;
      	   else if(ivideo.vbflags & CRT2_VGA) realcrtno = 2;
      	   else return;
   	}

	if((sisfb_crt1off) && (!crtno)) return;

    	temp = SiS_HandleDDC(&SiS_Pr, ivideo.vbflags, ivideo.sisvga_engine, realcrtno, 0, &buffer[0]);
   	if((!temp) || (temp == 0xffff)) {
      	   printk(KERN_INFO "sisfb: CRT%d DDC probing failed\n", crtno + 1);
	   return;
   	} else {
      	   printk(KERN_INFO "sisfb: CRT%d DDC supported\n", crtno + 1);
      	   printk(KERN_INFO "sisfb: CRT%d DDC level: %s%s%s%s\n",
	   	crtno + 1,
	   	(temp & 0x1a) ? "" : "[none of the supported]",
	   	(temp & 0x02) ? "2 " : "",
	   	(temp & 0x08) ? "D&P" : "",
           	(temp & 0x10) ? "FPDI-2" : "");
      	   if(temp & 0x02) {
	      i = 3;  /* Number of retrys */
	      do {
	    	 temp = SiS_HandleDDC(&SiS_Pr, ivideo.vbflags, ivideo.sisvga_engine,
				     realcrtno, 1, &buffer[0]);
	      } while((temp) && i--);
              if(!temp) {
	    	 if(sisfb_interpret_edid(monitor, &buffer[0])) {
		    printk(KERN_INFO "sisfb: Monitor range H %d-%dKHz, V %d-%dHz, Max. dotclock %dMHz\n",
		    	monitor->hmin, monitor->hmax, monitor->vmin, monitor->vmax,
			monitor->dclockmax / 1000);
		 } else {
	       	    printk(KERN_INFO "sisfb: CRT%d DDC EDID corrupt\n", crtno + 1);
	    	 }
	      } else {
            	 printk(KERN_INFO "sisfb: CRT%d DDC reading failed\n", crtno + 1);
	      }
	   } else {
	      printk(KERN_INFO "sisfb: VESA D&P and FPDI-2 not supported yet\n");
	   }
	}
}

static void __init
sisfb_search_vesamode(unsigned int vesamode, BOOLEAN quiet)
{
	int i = 0, j = 0;

	if(vesamode == 0) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
		sisfb_mode_idx = MODE_INDEX_NONE;
#else
		if(!quiet)
		   printk(KERN_ERR "sisfb: Mode 'none' not supported anymore. Using default.\n");
		sisfb_mode_idx = DEFAULT_MODE;
#endif
		return;
	}

	vesamode &= 0x1dff;  /* Clean VESA mode number from other flags */

	while(sisbios_mode[i++].mode_no != 0) {
		if( (sisbios_mode[i-1].vesa_mode_no_1 == vesamode) ||
		    (sisbios_mode[i-1].vesa_mode_no_2 == vesamode) ) {
		    if(sisfb_fstn) {
		       if(sisbios_mode[i-1].mode_no == 0x50 ||
		          sisbios_mode[i-1].mode_no == 0x56 ||
		          sisbios_mode[i-1].mode_no == 0x53) continue;
	            } else {
		       if(sisbios_mode[i-1].mode_no == 0x5a ||
		          sisbios_mode[i-1].mode_no == 0x5b) continue;
		    }
		    sisfb_mode_idx = i - 1;
		    j = 1;
		    break;
		}
	}
	if((!j) && !quiet) printk(KERN_ERR "sisfb: Invalid VESA mode 0x%x'\n", vesamode);
}

static void __init
sisfb_search_mode(char *name, BOOLEAN quiet)
{
	int i = 0;
	unsigned int j = 0, xres = 0, yres = 0, depth = 0, rate = 0;
	char strbuf[16], strbuf1[20];
	char *nameptr = name;

	if(name == NULL) {
	   if(!quiet)
	      printk(KERN_ERR "sisfb: Internal error, using default mode.\n");
	   sisfb_mode_idx = DEFAULT_MODE;
	   return;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
        if (!strnicmp(name, sisbios_mode[MODE_INDEX_NONE].name, strlen(name))) {
	   if(!quiet)
	      printk(KERN_ERR "sisfb: Mode 'none' not supported anymore. Using default.\n");
	   sisfb_mode_idx = DEFAULT_MODE;
	   return;
	}
#endif
	if(strlen(name) <= 19) {
	   strcpy(strbuf1, name);
	   for(i=0; i<strlen(strbuf1); i++) {
	      if(strbuf1[i] < '0' || strbuf1[i] > '9') strbuf1[i] = ' ';
	   }

	   /* This does some fuzzy mode naming detection */
	   if(sscanf(strbuf1, "%u %u %u %u", &xres, &yres, &depth, &rate) == 4) {
	      if((rate <= 32) || (depth > 32)) {
	         j = rate; rate = depth; depth = j;
	      }
	      sprintf(strbuf, "%ux%ux%u", xres, yres, depth);
	      nameptr = strbuf;
	      ivideo.refresh_rate = sisfb_parm_rate = rate;
	   } else if(sscanf(strbuf1, "%u %u %u", &xres, &yres, &depth) == 3) {
	      sprintf(strbuf, "%ux%ux%u", xres, yres, depth);
	      nameptr = strbuf;
	   } else {
	      xres = 0;
	      if((sscanf(strbuf1, "%u %u", &xres, &yres) == 2) && (xres != 0)) {
	         sprintf(strbuf, "%ux%ux8", xres, yres);
	         nameptr = strbuf;
	      } else {
	         sisfb_search_vesamode(simple_strtoul(name, NULL, 0), quiet);
	         return;
	      }
	   }
	}

	i = 0; j = 0;
	while(sisbios_mode[i].mode_no != 0) {
		if(!strnicmp(nameptr, sisbios_mode[i++].name, strlen(nameptr))) {
		   if(sisfb_fstn) {
		      if(sisbios_mode[i-1].mode_no == 0x50 ||
		         sisbios_mode[i-1].mode_no == 0x56 ||
		         sisbios_mode[i-1].mode_no == 0x53) continue;
	           } else {
		      if(sisbios_mode[i-1].mode_no == 0x5a ||
		         sisbios_mode[i-1].mode_no == 0x5b) continue;
		   }
		   sisfb_mode_idx = i - 1;
		   j = 1;
		   break;
		}
	}
	if((!j) && !quiet) printk(KERN_ERR "sisfb: Invalid mode '%s'\n", nameptr);

}

static void __init
sisfb_search_crt2type(const char *name)
{
	int i = 0;

	if(name == NULL)
		return;

	while(sis_crt2type[i].type_no != -1) {
		if (!strnicmp(name, sis_crt2type[i].name, strlen(sis_crt2type[i].name))) {
			sisfb_crt2type = sis_crt2type[i].type_no;
			sisfb_tvplug = sis_crt2type[i].tvplug_no;
			sisfb_dstn = (sis_crt2type[i].flags & FL_550_DSTN) ? 1 : 0;
			sisfb_fstn = (sis_crt2type[i].flags & FL_550_FSTN) ? 1 : 0;
			break;
		}
		i++;
	}
	if(sisfb_crt2type < 0)
		printk(KERN_ERR "sisfb: Invalid CRT2 type: %s\n", name);
        if(ivideo.chip != SIS_550) {
	   sisfb_dstn = sisfb_fstn = 0;
	}
}

static void __init
sisfb_search_queuemode(const char *name)
{
	int i = 0;

	if(name == NULL)
		return;

	while (sis_queuemode[i].type_no != -1) {
		if (!strnicmp(name, sis_queuemode[i].name, strlen(sis_queuemode[i].name))) {
			sisfb_queuemode = sis_queuemode[i].type_no;
			break;
		}
		i++;
	}
	if (sisfb_queuemode < 0)
		printk(KERN_ERR "sisfb: Invalid queuemode type: %s\n", name);
}

static void __init
sisfb_search_tvstd(const char *name)
{
	int i = 0;

	if(name == NULL)
		return;

	while (sis_tvtype[i].type_no != -1) {
		if (!strnicmp(name, sis_tvtype[i].name, strlen(sis_tvtype[i].name))) {
		        ivideo.vbflags &= ~(TV_NTSC | TV_PAL | TV_PALM | TV_PALN);
			ivideo.vbflags |= sis_tvtype[i].type_no;
			break;
		}
		i++;
	}
}

static void __init
sisfb_search_specialtiming(const char *name)
{
	int i = 0;
	BOOLEAN found = FALSE;

	if(name == NULL)
		return;

	if(!strnicmp(name, "none", 4)) {
	        SiS_Pr.SiS_CustomT = CUT_FORCENONE;
		printk(KERN_DEBUG "sisfb: Special timing disabled\n");
	} else {
	   while(mycustomttable[i].chipID != 0) {
	      if(!strnicmp(name,mycustomttable[i].optionName, strlen(mycustomttable[i].optionName))) {
		 SiS_Pr.SiS_CustomT = mycustomttable[i].SpecialID;
		 found = TRUE;
		 printk(KERN_INFO "sisfb: Special timing for %s %s forced (\"%s\")\n",
		 	mycustomttable[i].vendorName, mycustomttable[i].cardName,
		 	mycustomttable[i].optionName);
		 break;
	      }
	      i++;
	   }
	   if(!found) {
	      printk(KERN_WARNING "sisfb: Invalid SpecialTiming parameter, valid are:");
	      printk(KERN_WARNING "\t\"none\" (to disable special timings)\n");
	      i = 0;
	      while(mycustomttable[i].chipID != 0) {
		 printk(KERN_WARNING "\t\"%s\" (for %s %s)\n",
		     mycustomttable[i].optionName,
		     mycustomttable[i].vendorName,
		     mycustomttable[i].cardName);
		 i++;
	      }
           }
 	}
}

static BOOLEAN
sisfb_verify_rate(struct sisfb_monitor *monitor, int mode_idx, int rate_idx, int rate)
{
	int htotal, vtotal;
	unsigned int dclock, hsync;

	if(!monitor->datavalid) return TRUE;

	if(mode_idx < 0) return FALSE;

	if(rate < (monitor->vmin - 1)) return FALSE;
	if(rate > (monitor->vmax + 1)) return FALSE;

	if(sisfb_gettotalfrommode(&SiS_Pr, &sishw_ext, sisbios_mode[mode_idx].mode_no,
	                          &htotal, &vtotal, rate_idx)) {
		dclock = (htotal * vtotal * rate) / 1000;
		if(dclock > (monitor->dclockmax + 1000)) return FALSE;
		hsync = dclock / htotal;
		if(hsync < (monitor->hmin - 1)) return FALSE;
		if(hsync > (monitor->hmax + 1)) return FALSE;
        } else {
	  	return FALSE;
	}
	return TRUE;
}

static int
sisfb_validate_mode(int myindex, unsigned long vbflags)
{
   u16 xres=0, yres, myres;

#ifdef CONFIG_FB_SIS_300
   if(ivideo.sisvga_engine == SIS_300_VGA) {
      if(!(sisbios_mode[myindex].chipset & MD_SIS300)) return(-1);
   }
#endif
#ifdef CONFIG_FB_SIS_315
   if(ivideo.sisvga_engine == SIS_315_VGA) {
      if(!(sisbios_mode[myindex].chipset & MD_SIS315)) return(-1);
   }
#endif

   myres = sisbios_mode[myindex].yres;

   switch (vbflags & VB_DISPTYPE_DISP2) {
     case CRT2_LCD:
	switch(sishw_ext.ulCRT2LCDType) {
	case LCD_640x480:  xres =  640; yres =  480;  break;
	case LCD_800x600:  xres =  800; yres =  600;  break;
        case LCD_1024x600: xres = 1024; yres =  600;  break;
	case LCD_1024x768: xres = 1024; yres =  768;  break;
	case LCD_1152x768: xres = 1152; yres =  768;  break;
	case LCD_1280x960: xres = 1280; yres =  960;  break;
	case LCD_1280x768: xres = 1280; yres =  768;  break;
	case LCD_1280x1024:xres = 1280; yres = 1024;  break;
	case LCD_1400x1050:xres = 1400; yres = 1050;  break;
	case LCD_1600x1200:xres = 1600; yres = 1200;  break;
	case LCD_320x480:  xres =  320; yres =  480;  break; /* FSTN (old) */
	case LCD_640x480_2:
	case LCD_640x480_3:xres =  640; yres =  480;  break; /* FSTN (new) */
	case LCD_1680x1050:xres = 1680; yres = 1050;  break;
	case LCD_1280x800: xres = 1280; yres =  800;  break;
	case LCD_1280x720: yres = 1280; yres =  720;  break;
	default:           xres =    0; yres =    0;  break;
	}

	if(SiS_Pr.SiS_CustomT == CUT_BARCO1366) {
	   xres = 1360; yres = 1024;
	}

	if(SiS_Pr.SiS_CustomT == CUT_PANEL848) {
	   xres = 848;  yres =  480;
	} else {
	   if(sisbios_mode[myindex].xres > xres) return(-1);
           if(myres > yres) return(-1);
	}

	if(vbflags & (VB_LVDS | VB_30xBDH)) {
	   if(sisbios_mode[myindex].xres == 320) {
	      if((myres == 240) || (myres == 480)) {
		 if(!sisfb_fstn) {
		    if(sisbios_mode[myindex].mode_no == 0x5a ||
		       sisbios_mode[myindex].mode_no == 0x5b)
		       return(-1);
		 } else {
		    if(sisbios_mode[myindex].mode_no == 0x50 ||
		       sisbios_mode[myindex].mode_no == 0x56 ||
		       sisbios_mode[myindex].mode_no == 0x53)
		       return(-1);
		 }
	      }
	   }
	}

	if(SiS_GetModeID_LCD(ivideo.sisvga_engine, vbflags, sisbios_mode[myindex].xres, sisbios_mode[myindex].yres,
	                     0, sisfb_fstn, SiS_Pr.SiS_CustomT, xres, yres) < 0x14) {
	   return(-1);
	}
	break;

     case CRT2_TV:
	if(SiS_GetModeID_TV(ivideo.sisvga_engine, vbflags, sisbios_mode[myindex].xres,
	                    sisbios_mode[myindex].yres, 0) < 0x14) {
	   return(-1);
	}
	break;

     case CRT2_VGA:
        if(SiS_GetModeID_VGA2(ivideo.sisvga_engine, vbflags, sisbios_mode[myindex].xres,
	                    sisbios_mode[myindex].yres, 0) < 0x14) {
	   return(-1);
	}
	break;
     }

     return(myindex);
}


static u8
sisfb_search_refresh_rate(unsigned int rate, int mode_idx)
{
	u16 xres, yres;
	int i = 0;

	xres = sisbios_mode[mode_idx].xres;
	yres = sisbios_mode[mode_idx].yres;

	ivideo.rate_idx = 0;
	while ((sisfb_vrate[i].idx != 0) && (sisfb_vrate[i].xres <= xres)) {
		if ((sisfb_vrate[i].xres == xres) && (sisfb_vrate[i].yres == yres)) {
			if (sisfb_vrate[i].refresh == rate) {
				ivideo.rate_idx = sisfb_vrate[i].idx;
				break;
			} else if (sisfb_vrate[i].refresh > rate) {
				if ((sisfb_vrate[i].refresh - rate) <= 3) {
					DPRINTK("sisfb: Adjusting rate from %d up to %d\n",
						rate, sisfb_vrate[i].refresh);
					ivideo.rate_idx = sisfb_vrate[i].idx;
					ivideo.refresh_rate = sisfb_vrate[i].refresh;
				} else if (((rate - sisfb_vrate[i-1].refresh) <= 2)
						&& (sisfb_vrate[i].idx != 1)) {
					DPRINTK("sisfb: Adjusting rate from %d down to %d\n",
						rate, sisfb_vrate[i-1].refresh);
					ivideo.rate_idx = sisfb_vrate[i-1].idx;
					ivideo.refresh_rate = sisfb_vrate[i-1].refresh;
				} 
				break;
			} else if((rate - sisfb_vrate[i].refresh) <= 2) {
				DPRINTK("sisfb: Adjusting rate from %d down to %d\n",
						rate, sisfb_vrate[i].refresh);
	           		ivideo.rate_idx = sisfb_vrate[i].idx;
		   		break;
	       		}
		}
		i++;
	}
	if(ivideo.rate_idx > 0) {
		return ivideo.rate_idx;
	} else {
		printk(KERN_INFO
			"sisfb: Unsupported rate %d for %dx%d\n", rate, xres, yres);
		return 0;
	}
}


static BOOLEAN
sisfb_bridgeisslave(void)
{
   unsigned char P1_00;

   if(!(ivideo.vbflags & VB_VIDEOBRIDGE)) return FALSE;

   inSISIDXREG(SISPART1,0x00,P1_00);
   if( ((ivideo.sisvga_engine == SIS_300_VGA) && (P1_00 & 0xa0) == 0x20) ||
       ((ivideo.sisvga_engine == SIS_315_VGA) && (P1_00 & 0x50) == 0x10) ) {
	   return TRUE;
   } else {
           return FALSE;
   }
}

static BOOLEAN
sisfballowretracecrt1(void)
{
   u8 temp;

   inSISIDXREG(SISCR,0x17,temp);
   if(!(temp & 0x80)) return FALSE;

   inSISIDXREG(SISSR,0x1f,temp);
   if(temp & 0xc0) return FALSE;

   return TRUE;
}

static BOOLEAN
sisfbcheckvretracecrt1(void)
{
   if(!sisfballowretracecrt1()) return FALSE;

   if(inSISREG(SISINPSTAT) & 0x08) return TRUE;
   else 			   return FALSE;
}

static void
sisfbwaitretracecrt1(void)
{
   int watchdog;

   if(!sisfballowretracecrt1()) return;

   watchdog = 65536;
   while((!(inSISREG(SISINPSTAT) & 0x08)) && --watchdog);
   watchdog = 65536;
   while((inSISREG(SISINPSTAT) & 0x08) && --watchdog);
}

static BOOLEAN
sisfbcheckvretracecrt2(void)
{
   unsigned char temp, reg;

   switch(ivideo.sisvga_engine) {
   case SIS_300_VGA:
   	reg = 0x25;
	break;
   case SIS_315_VGA:
   	reg = 0x30;
	break;
   default:
        return FALSE;
   }

   inSISIDXREG(SISPART1, reg, temp);
   if(temp & 0x02) return FALSE;
   else 	   return TRUE;
}

static BOOLEAN
sisfb_CheckVBRetrace(void)
{
   if(ivideo.currentvbflags & VB_DISPTYPE_DISP2) {
      if(sisfb_bridgeisslave()) {
         return(sisfbcheckvretracecrt1());
      } else {
         return(sisfbcheckvretracecrt2());
      }
   } 
   return(sisfbcheckvretracecrt1());
}

static int
sisfb_myblank(int blank)
{
   u8 sr01, sr11, sr1f, cr63=0, p2_0, p1_13;
   BOOLEAN backlight = TRUE;

   switch(blank) {
   case 0:	/* on */
      sr01  = 0x00;
      sr11  = 0x00;
      sr1f  = 0x00;
      cr63  = 0x00;
      p2_0  = 0x20;
      p1_13 = 0x00;
      backlight = TRUE;
      break;
   case 1:	/* blank */
      sr01  = 0x20;
      sr11  = 0x00;
      sr1f  = 0x00;
      cr63  = 0x00;
      p2_0  = 0x20;
      p1_13 = 0x00;
      backlight = TRUE;
      break;
   case 2:	/* no vsync */
      sr01  = 0x20;
      sr11  = 0x08;
      sr1f  = 0x80;
      cr63  = 0x40;
      p2_0  = 0x40;
      p1_13 = 0x80;
      backlight = FALSE;
      break;
   case 3:	/* no hsync */
      sr01  = 0x20;
      sr11  = 0x08;
      sr1f  = 0x40;
      cr63  = 0x40;
      p2_0  = 0x80;
      p1_13 = 0x40;
      backlight = FALSE;
      break;
   case 4:	/* off */
      sr01  = 0x20;
      sr11  = 0x08;
      sr1f  = 0xc0;
      cr63  = 0x40;
      p2_0  = 0xc0;
      p1_13 = 0xc0;
      backlight = FALSE;
      break;
   default:
      return 1;
   }

   if(ivideo.currentvbflags & VB_DISPTYPE_CRT1) {

      setSISIDXREG(SISSR, 0x01, ~0x20, sr01);

      if( (!sisfb_thismonitor.datavalid) ||
          ((sisfb_thismonitor.datavalid) &&
           (sisfb_thismonitor.feature & 0xe0))) {

	 if(ivideo.sisvga_engine == SIS_315_VGA) {
	    setSISIDXREG(SISCR, SiS_Pr.SiS_MyCR63, 0xbf, cr63);
	 }

	 setSISIDXREG(SISSR, 0x1f, 0x3f, sr1f);
      }

   }

   if(ivideo.currentvbflags & CRT2_LCD) {

      if(ivideo.vbflags & (VB_301LV|VB_302LV|VB_302ELV)) {
	 if(backlight) {
	    SiS_SiS30xBLOn(&SiS_Pr, &sishw_ext);
	 } else {
	    SiS_SiS30xBLOff(&SiS_Pr, &sishw_ext);
	 }
      } else if(ivideo.sisvga_engine == SIS_315_VGA) {
	 if(ivideo.vbflags & VB_CHRONTEL) {
	    if(backlight) {
	       SiS_Chrontel701xBLOn(&SiS_Pr,&sishw_ext);
	    } else {
	       SiS_Chrontel701xBLOff(&SiS_Pr);
	    }
	 }
      }

      if(((ivideo.sisvga_engine == SIS_300_VGA) &&
          (ivideo.vbflags & (VB_301|VB_30xBDH|VB_LVDS))) ||
         ((ivideo.sisvga_engine == SIS_315_VGA) &&
          ((ivideo.vbflags & (VB_LVDS | VB_CHRONTEL)) == VB_LVDS))) {
          setSISIDXREG(SISSR, 0x11, ~0x0c, sr11);
      }

      if(ivideo.sisvga_engine == SIS_300_VGA) {
         if((ivideo.vbflags & (VB_301B|VB_301C|VB_302B)) &&
            (!(ivideo.vbflags & VB_30xBDH))) {
	    setSISIDXREG(SISPART1, 0x13, 0x3f, p1_13);
	 }
      } else if(ivideo.sisvga_engine == SIS_315_VGA) {
         if((ivideo.vbflags & (VB_301B|VB_301C|VB_302B)) &&
            (!(ivideo.vbflags & VB_30xBDH))) {
	    setSISIDXREG(SISPART2, 0x00, 0x1f, p2_0);
	 }
      }

   } else if(ivideo.currentvbflags & CRT2_VGA) {

      if(ivideo.vbflags & (VB_301B|VB_301C|VB_302B)) {
         setSISIDXREG(SISPART2, 0x00, 0x1f, p2_0);
      }

   }

   return(0);
}

/* ----------- FBDev related routines for all series ----------- */

static void
sisfb_set_vparms(void)
{
   switch(ivideo.video_bpp) {
   case 8:
       	ivideo.DstColor = 0x0000;
	ivideo.SiS310_AccelDepth = 0x00000000;
	ivideo.video_cmap_len = 256;
       	break;
   case 16:
       	ivideo.DstColor = 0x8000;
       	ivideo.SiS310_AccelDepth = 0x00010000;
	ivideo.video_cmap_len = 16;
       	break;
   case 32:
       	ivideo.DstColor = 0xC000;
	ivideo.SiS310_AccelDepth = 0x00020000;
	ivideo.video_cmap_len = 16;
       	break;
   default:
 	ivideo.video_cmap_len = 16;
	printk(KERN_ERR "sisfb: Unsupported depth %d", ivideo.video_bpp);
	ivideo.accel = 0;
	break;
   }
}

static int
sisfb_do_set_var(struct fb_var_screeninfo *var, int isactive,
		      struct fb_info *info)
{
	unsigned int htotal = 0, vtotal = 0; 
	unsigned int drate = 0, hrate = 0;
	int found_mode = 0;
	int old_mode;
	u32 pixclock;

	htotal = var->left_margin + var->xres + var->right_margin + var->hsync_len;

	vtotal = var->upper_margin + var->lower_margin + var->vsync_len;

	pixclock = var->pixclock;

	if((var->vmode & FB_VMODE_MASK) == FB_VMODE_NONINTERLACED) {
		vtotal += var->yres;
		vtotal <<= 1;
	} else if((var->vmode & FB_VMODE_MASK) == FB_VMODE_DOUBLE) {
		vtotal += var->yres;
		vtotal <<= 2;
	} else if((var->vmode & FB_VMODE_MASK) == FB_VMODE_INTERLACED) {
		vtotal += var->yres;
		vtotal <<= 1;
	} else 	vtotal += var->yres;

	if(!(htotal) || !(vtotal)) {
		DPRINTK("sisfb: Invalid 'var' information\n");
		return -EINVAL;
	}

	if(pixclock && htotal && vtotal) {
	   drate = 1000000000 / pixclock;
	   hrate = (drate * 1000) / htotal;
	   ivideo.refresh_rate = (unsigned int) (hrate * 2 / vtotal);
	} else ivideo.refresh_rate = 60;

	old_mode = sisfb_mode_idx;
	sisfb_mode_idx = 0;

	while( (sisbios_mode[sisfb_mode_idx].mode_no != 0) &&
	       (sisbios_mode[sisfb_mode_idx].xres <= var->xres) ) {
		if( (sisbios_mode[sisfb_mode_idx].xres == var->xres) &&
		    (sisbios_mode[sisfb_mode_idx].yres == var->yres) &&
		    (sisbios_mode[sisfb_mode_idx].bpp == var->bits_per_pixel)) {
			ivideo.mode_no = sisbios_mode[sisfb_mode_idx].mode_no;
			found_mode = 1;
			break;
		}
		sisfb_mode_idx++;
	}

	if(found_mode)
		sisfb_mode_idx = sisfb_validate_mode(sisfb_mode_idx, ivideo.currentvbflags);
	else
		sisfb_mode_idx = -1;

       	if(sisfb_mode_idx < 0) {
		printk(KERN_ERR "sisfb: Mode %dx%dx%d not supported\n", var->xres,
		       var->yres, var->bits_per_pixel);
		sisfb_mode_idx = old_mode;
		return -EINVAL;
	}

	if(sisfb_search_refresh_rate(ivideo.refresh_rate, sisfb_mode_idx) == 0) {
		ivideo.rate_idx = sisbios_mode[sisfb_mode_idx].rate_idx;
		ivideo.refresh_rate = 60;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	if(sisfb_thismonitor.datavalid) {
	   if(!sisfb_verify_rate(&sisfb_thismonitor, sisfb_mode_idx,
	                         ivideo.rate_idx, ivideo.refresh_rate)) {
	      printk(KERN_INFO "sisfb: WARNING: Refresh rate exceeds monitor specs!\n");
	   }
	}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	if(((var->activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_NOW) && isactive) {
#else
	if(isactive) {
#endif
		sisfb_pre_setmode();

		if(SiSSetMode(&SiS_Pr, &sishw_ext, ivideo.mode_no) == 0) {
			printk(KERN_ERR "sisfb: Setting mode[0x%x] failed\n", ivideo.mode_no);
			return -EINVAL;
		}

		outSISIDXREG(SISSR, IND_SIS_PASSWORD, SIS_PASSWORD);
		
		ivideo.video_bpp = sisbios_mode[sisfb_mode_idx].bpp;
		ivideo.video_vwidth = ivideo.video_width = sisbios_mode[sisfb_mode_idx].xres;
		ivideo.video_vheight = ivideo.video_height = sisbios_mode[sisfb_mode_idx].yres;
		ivideo.org_x = ivideo.org_y = 0;
		ivideo.video_linelength = ivideo.video_width * (ivideo.video_bpp >> 3);
		ivideo.accel = 0;
		if(sisfb_accel) {
		   ivideo.accel = (var->accel_flags & FB_ACCELF_TEXT) ? -1 : 0;
		}

		sisfb_set_vparms();
		
		ivideo.current_width = ivideo.video_width;
		ivideo.current_height = ivideo.video_height;
		ivideo.current_bpp = ivideo.video_bpp;
		ivideo.current_htotal = htotal;
		ivideo.current_vtotal = vtotal;
		ivideo.current_pixclock = var->pixclock;
		ivideo.current_refresh_rate = ivideo.refresh_rate;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
                sisfb_lastrates[ivideo.mode_no] = ivideo.refresh_rate;
#endif
		
		sisfb_post_setmode();

	}
	return 0;
}

static int
sisfb_pan_var(struct fb_var_screeninfo *var)
{
	unsigned int base;

	if (var->xoffset > (var->xres_virtual - var->xres)) {
		return -EINVAL;
	}
	if(var->yoffset > (var->yres_virtual - var->yres)) {
		return -EINVAL;
	}	
		
	base = var->yoffset * var->xres_virtual + var->xoffset;
		
        /* calculate base bpp dep. */
        switch(var->bits_per_pixel) {
        case 16:
        	base >>= 1;
        	break;
	case 32:
            	break;
	case 8:
        default:
        	base >>= 2;
            	break;
        }
	
	outSISIDXREG(SISSR, IND_SIS_PASSWORD, SIS_PASSWORD);

        outSISIDXREG(SISCR, 0x0D, base & 0xFF);
	outSISIDXREG(SISCR, 0x0C, (base >> 8) & 0xFF);
	outSISIDXREG(SISSR, 0x0D, (base >> 16) & 0xFF);
	if(ivideo.sisvga_engine == SIS_315_VGA) {
		setSISIDXREG(SISSR, 0x37, 0xFE, (base >> 24) & 0x01);
	}
        if(ivideo.currentvbflags & VB_DISPTYPE_DISP2) {
		orSISIDXREG(SISPART1, ivideo.CRT2_write_enable, 0x01);
        	outSISIDXREG(SISPART1, 0x06, (base & 0xFF));
        	outSISIDXREG(SISPART1, 0x05, ((base >> 8) & 0xFF));
        	outSISIDXREG(SISPART1, 0x04, ((base >> 16) & 0xFF));
		if(ivideo.sisvga_engine == SIS_315_VGA) {
			setSISIDXREG(SISPART1, 0x02, 0x7F, ((base >> 24) & 0x01) << 7);
		}
        }
	return 0;
}

static void
sisfb_bpp_to_var(struct fb_var_screeninfo *var)
{
	switch(var->bits_per_pixel) {
	   case 8:
	   	var->red.offset = var->green.offset = var->blue.offset = 0;
		var->red.length = var->green.length = var->blue.length = 6;
		ivideo.video_cmap_len = 256;
		break;
	   case 16:
		var->red.offset = 11;
		var->red.length = 5;
		var->green.offset = 5;
		var->green.length = 6;
		var->blue.offset = 0;
		var->blue.length = 5;
		var->transp.offset = 0;
		var->transp.length = 0;
		ivideo.video_cmap_len = 16;
		break;
	   case 32:
		var->red.offset = 16;
		var->red.length = 8;
		var->green.offset = 8;
		var->green.length = 8;
		var->blue.offset = 0;
		var->blue.length = 8;
		var->transp.offset = 24;
		var->transp.length = 8;
		ivideo.video_cmap_len = 16;
		break;
	}
}

void
sis_dispinfo(struct ap_data *rec)
{
	rec->minfo.bpp      = ivideo.video_bpp;
	rec->minfo.xres     = ivideo.video_width;
	rec->minfo.yres     = ivideo.video_height;
	rec->minfo.v_xres   = ivideo.video_vwidth;
	rec->minfo.v_yres   = ivideo.video_vheight;
	rec->minfo.org_x    = ivideo.org_x;
	rec->minfo.org_y    = ivideo.org_y;
	rec->minfo.vrate    = ivideo.refresh_rate;
	rec->iobase         = ivideo.vga_base - 0x30;
	rec->mem_size       = ivideo.video_size;
	rec->disp_state     = ivideo.disp_state; 
	rec->version        = (VER_MAJOR << 24) | (VER_MINOR << 16) | VER_LEVEL; 
	rec->hasVB          = ivideo.hasVB;
	rec->TV_type        = ivideo.TV_type; 
	rec->TV_plug        = ivideo.TV_plug; 
	rec->chip           = ivideo.chip;
	rec->vbflags	    = ivideo.vbflags;
	rec->currentvbflags = ivideo.currentvbflags;
}

/* ------------ FBDev related routines for 2.4 series ----------- */

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,5,0)

static void
sisfb_crtc_to_var(struct fb_var_screeninfo *var)
{
	u16 VRE, VBE, VRS, VBS, VDE, VT;
	u16 HRE, HBE, HRS, HBS, HDE, HT;
	u8  sr_data, cr_data, cr_data2, cr_data3, mr_data;
	int A, B, C, D, E, F, temp;
	unsigned int hrate, drate, maxyres;

	inSISIDXREG(SISSR, IND_SIS_COLOR_MODE, sr_data);

	if(sr_data & SIS_INTERLACED_MODE)
	   var->vmode = FB_VMODE_INTERLACED;
	else
	   var->vmode = FB_VMODE_NONINTERLACED;

	switch ((sr_data & 0x1C) >> 2) {
	   case SIS_8BPP_COLOR_MODE:
		var->bits_per_pixel = 8;
		break;
	   case SIS_16BPP_COLOR_MODE:
		var->bits_per_pixel = 16;
		break;
	   case SIS_32BPP_COLOR_MODE:
		var->bits_per_pixel = 32;
		break;
	}

	sisfb_bpp_to_var(var);
	
	inSISIDXREG(SISSR, 0x0A, sr_data);

        inSISIDXREG(SISCR, 0x06, cr_data);

        inSISIDXREG(SISCR, 0x07, cr_data2);

	VT = (cr_data & 0xFF) | ((u16) (cr_data2 & 0x01) << 8) |
	     ((u16) (cr_data2 & 0x20) << 4) | ((u16) (sr_data & 0x01) << 10);
	A = VT + 2;

	inSISIDXREG(SISCR, 0x12, cr_data);

	VDE = (cr_data & 0xff) | ((u16) (cr_data2 & 0x02) << 7) |
	      ((u16) (cr_data2 & 0x40) << 3) | ((u16) (sr_data & 0x02) << 9);
	E = VDE + 1;

	inSISIDXREG(SISCR, 0x10, cr_data);

	VRS = (cr_data & 0xff) | ((u16) (cr_data2 & 0x04) << 6) |
	      ((u16) (cr_data2 & 0x80) << 2) | ((u16) (sr_data & 0x08) << 7);
	F = VRS + 1 - E;

	inSISIDXREG(SISCR, 0x15, cr_data);

	inSISIDXREG(SISCR, 0x09, cr_data3);

	if(cr_data3 & 0x80) var->vmode = FB_VMODE_DOUBLE;

	VBS = (cr_data & 0xff) | ((u16) (cr_data2 & 0x08) << 5) |
	      ((u16) (cr_data3 & 0x20) << 4) | ((u16) (sr_data & 0x04) << 8);

	inSISIDXREG(SISCR, 0x16, cr_data);

	VBE = (cr_data & 0xff) | ((u16) (sr_data & 0x10) << 4);
	temp = VBE - ((E - 1) & 511);
	B = (temp > 0) ? temp : (temp + 512);

	inSISIDXREG(SISCR, 0x11, cr_data);

	VRE = (cr_data & 0x0f) | ((sr_data & 0x20) >> 1);
	temp = VRE - ((E + F - 1) & 31);
	C = (temp > 0) ? temp : (temp + 32);

	D = B - F - C;

        var->yres = E;
	var->upper_margin = D;
	var->lower_margin = F;
	var->vsync_len = C;

	if((var->vmode & FB_VMODE_MASK) == FB_VMODE_INTERLACED) {
	   var->yres <<= 1;
	   var->upper_margin <<= 1;
	   var->lower_margin <<= 1;
	   var->vsync_len <<= 1;
	} else if((var->vmode & FB_VMODE_MASK) == FB_VMODE_DOUBLE) {
	   var->yres >>= 1;
	   var->upper_margin >>= 1;
	   var->lower_margin >>= 1;
	   var->vsync_len >>= 1;
	}

	inSISIDXREG(SISSR, 0x0b, sr_data);

	inSISIDXREG(SISCR, 0x00, cr_data);

	HT = (cr_data & 0xff) | ((u16) (sr_data & 0x03) << 8);
	A = HT + 5;

	inSISIDXREG(SISCR, 0x01, cr_data);

	HDE = (cr_data & 0xff) | ((u16) (sr_data & 0x0C) << 6);
	E = HDE + 1;

	inSISIDXREG(SISCR, 0x04, cr_data);

	HRS = (cr_data & 0xff) | ((u16) (sr_data & 0xC0) << 2);
	F = HRS - E - 3;

	inSISIDXREG(SISCR, 0x02, cr_data);

	HBS = (cr_data & 0xff) | ((u16) (sr_data & 0x30) << 4);

	inSISIDXREG(SISSR, 0x0c, sr_data);

	inSISIDXREG(SISCR, 0x03, cr_data);

	inSISIDXREG(SISCR, 0x05, cr_data2);

	HBE = (cr_data & 0x1f) | ((u16) (cr_data2 & 0x80) >> 2) |
	      ((u16) (sr_data & 0x03) << 6);
	HRE = (cr_data2 & 0x1f) | ((sr_data & 0x04) << 3);

	temp = HBE - ((E - 1) & 255);
	B = (temp > 0) ? temp : (temp + 256);

	temp = HRE - ((E + F + 3) & 63);
	C = (temp > 0) ? temp : (temp + 64);

	D = B - F - C;

	var->xres = var->xres_virtual = E * 8;

	if((var->xres == 320) &&
	   (var->yres == 200 || var->yres == 240)) {
		/* Terrible hack, but the correct CRTC data for
	  	 * these modes only produces a black screen...
	  	 */
       		var->left_margin = (400 - 376);
       		var->right_margin = (328 - 320);
       		var->hsync_len = (376 - 328);
	} else {
	   	var->left_margin = D * 8;
	   	var->right_margin = F * 8;
	   	var->hsync_len = C * 8;
	}
	var->activate = FB_ACTIVATE_NOW;

	var->sync = 0;

	mr_data = inSISREG(SISMISCR);
	if(mr_data & 0x80)
	   var->sync &= ~FB_SYNC_VERT_HIGH_ACT;
	else
	   var->sync |= FB_SYNC_VERT_HIGH_ACT;

	if(mr_data & 0x40)
	   var->sync &= ~FB_SYNC_HOR_HIGH_ACT;
	else
	   var->sync |= FB_SYNC_HOR_HIGH_ACT;

	VT += 2;
	VT <<= 1;
	HT = (HT + 5) * 8;

	if((var->vmode & FB_VMODE_MASK) == FB_VMODE_INTERLACED) {
	   VT <<= 1;
	}
	hrate = ivideo.refresh_rate * VT / 2;
	drate = (hrate * HT) / 1000;
	var->pixclock = (u32) (1000000000 / drate);

	if(sisfb_ypan) {
	   maxyres = ivideo.heapstart / (var->xres * (var->bits_per_pixel >> 3));
	   if(maxyres > 32767) maxyres = 32767;
	   if(sisfb_max) {
	      var->yres_virtual = maxyres;
	   } else {
	      if(var->yres_virtual > maxyres) {
	         var->yres_virtual = maxyres;
	      }
	   }
	   if(var->yres_virtual <= var->yres) {
	      var->yres_virtual = var->yres;
	   }
	} else
	   var->yres_virtual = var->yres;

}

static int
sis_getcolreg(unsigned regno, unsigned *red, unsigned *green, unsigned *blue,
			 unsigned *transp, struct fb_info *fb_info)
{
	if(regno >= ivideo.video_cmap_len)
		return 1;

	*red = sis_palette[regno].red;
	*green = sis_palette[regno].green;
	*blue = sis_palette[regno].blue;
	*transp = 0;
	return 0;
}

static int
sisfb_setcolreg(unsigned regno, unsigned red, unsigned green, unsigned blue,
                           unsigned transp, struct fb_info *fb_info)
{
	if(regno >= ivideo.video_cmap_len)
		return 1;

	sis_palette[regno].red = red;
	sis_palette[regno].green = green;
	sis_palette[regno].blue = blue;

	switch(ivideo.video_bpp) {
#ifdef FBCON_HAS_CFB8
	case 8:
	        outSISREG(SISDACA, regno);
		outSISREG(SISDACD, (red >> 10));
		outSISREG(SISDACD, (green >> 10));
		outSISREG(SISDACD, (blue >> 10));
		if(ivideo.currentvbflags & VB_DISPTYPE_DISP2) {
		        outSISREG(SISDAC2A, regno);
			outSISREG(SISDAC2D, (red >> 8));
			outSISREG(SISDAC2D, (green >> 8));
			outSISREG(SISDAC2D, (blue >> 8));
		}
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 16:
		sis_fbcon_cmap.cfb16[regno] =
		    ((red & 0xf800)) | ((green & 0xfc00) >> 5) | ((blue & 0xf800) >> 11);
		break;
#endif
#ifdef FBCON_HAS_CFB32
	case 32:
		red >>= 8;
		green >>= 8;
		blue >>= 8;
		sis_fbcon_cmap.cfb32[regno] = (red << 16) | (green << 8) | (blue);
		break;
#endif
	}
	return 0;
}

static void
sisfb_set_disp(int con, struct fb_var_screeninfo *var,
                           struct fb_info *info)
{
	struct fb_fix_screeninfo fix;
	long   flags;
	struct display *display;
	struct display_switch *sw;

	if(con >= 0)
		display = &fb_display[con];
	else
		display = &sis_disp;

	sisfb_get_fix(&fix, con, 0);

	display->screen_base = ivideo.video_vbase;
	display->visual = fix.visual;
	display->type = fix.type;
	display->type_aux = fix.type_aux;
	display->ypanstep = fix.ypanstep;
	display->ywrapstep = fix.ywrapstep;
	display->line_length = fix.line_length;
	display->next_line = fix.line_length;
	display->can_soft_blank = 1;
	display->inverse = sisfb_inverse;
	display->var = *var;

	save_flags(flags);

	switch(ivideo.video_bpp) {
#ifdef FBCON_HAS_CFB8
	   case 8:
#ifdef SISFBACCEL
		sw = ivideo.accel ? &fbcon_sis8 : &fbcon_cfb8;
#else
		sw = &fbcon_cfb8;
#endif
		break;
#endif
#ifdef FBCON_HAS_CFB16
	   case 16:
#ifdef SISFBACCEL
		sw = ivideo.accel ? &fbcon_sis16 : &fbcon_cfb16;
#else
		sw = &fbcon_cfb16;
#endif
		display->dispsw_data = sis_fbcon_cmap.cfb16;
		break;
#endif
#ifdef FBCON_HAS_CFB32
	   case 32:
#ifdef SISFBACCEL
		sw = ivideo.accel ? &fbcon_sis32 : &fbcon_cfb32;
#else
		sw = &fbcon_cfb32;
#endif
		display->dispsw_data = sis_fbcon_cmap.cfb32;
		break;
#endif
	   default:
		sw = &fbcon_dummy;
		return;
	}
	memcpy(&sisfb_sw, sw, sizeof(*sw));
	display->dispsw = &sisfb_sw;
	restore_flags(flags);

        if(sisfb_ypan) {
  	    /* display->scrollmode = 0;  */
	} else {
	    display->scrollmode = SCROLL_YREDRAW;
	    sisfb_sw.bmove = fbcon_redraw_bmove;
	}
}

static void
sisfb_do_install_cmap(int con, struct fb_info *info)
{
        if(con != currcon)
		return;

        if(fb_display[con].cmap.len)
		fb_set_cmap(&fb_display[con].cmap, 1, sisfb_setcolreg, info);
        else
		fb_set_cmap(fb_default_cmap(ivideo.video_cmap_len), 1,
			    sisfb_setcolreg, info);
}


static int
sisfb_get_var(struct fb_var_screeninfo *var, int con,
			 struct fb_info *info)
{
	if(con == -1)
		memcpy(var, &default_var, sizeof(struct fb_var_screeninfo));
	else
		*var = fb_display[con].var;

	if(sisfb_fstn) {
	   if (var->xres == 320 && var->yres == 480)
		var->yres = 240;
        }

	return 0;
}

static int
sisfb_set_var(struct fb_var_screeninfo *var, int con,
			 struct fb_info *info)
{
	int err;
	unsigned int cols, rows;

	fb_display[con].var.activate = FB_ACTIVATE_NOW;
        if(sisfb_do_set_var(var, con == currcon, info)) {
		sisfb_crtc_to_var(var);
		return -EINVAL;
	}

	sisfb_crtc_to_var(var);

	sisfb_set_disp(con, var, info);

	if(info->changevar)
		(*info->changevar) (con);

	if((err = fb_alloc_cmap(&fb_display[con].cmap, 0, 0)))
		return err;

	sisfb_do_install_cmap(con, info);

	cols = sisbios_mode[sisfb_mode_idx].cols;
	rows = sisbios_mode[sisfb_mode_idx].rows;
#if 0
	/* Why was this called here? */
 	vc_resize_con(rows, cols, fb_display[con].conp->vc_num); 
#endif	

	return 0;
}

static int
sisfb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			  struct fb_info *info)
{
        if(con == currcon)
		return fb_get_cmap(cmap, kspc, sis_getcolreg, info);

	else if (fb_display[con].cmap.len)
		fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
	else
		fb_copy_cmap(fb_default_cmap(ivideo.video_cmap_len), cmap, kspc ? 0 : 2);

	return 0;
}

static int
sisfb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			  struct fb_info *info)
{
	int err;

	if(!fb_display[con].cmap.len) {
		err = fb_alloc_cmap(&fb_display[con].cmap, ivideo.video_cmap_len, 0);
		if (err)
			return err;
	}
        
	if(con == currcon)
		return fb_set_cmap(cmap, kspc, sisfb_setcolreg, info);

	else
		fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);

	return 0;
}

static int
sisfb_pan_display(struct fb_var_screeninfo *var, int con,
			     struct fb_info* info)
{
	int err;

	if(var->vmode & FB_VMODE_YWRAP) {
		if(var->yoffset < 0 || var->yoffset >= fb_display[con].var.yres_virtual || var->xoffset)
			return -EINVAL;
	} else {
		if(var->xoffset+fb_display[con].var.xres > fb_display[con].var.xres_virtual ||
		   var->yoffset+fb_display[con].var.yres > fb_display[con].var.yres_virtual)
			return -EINVAL;
	}

        if(con == currcon) {
	   if((err = sisfb_pan_var(var)) < 0) return err;
	}

	fb_display[con].var.xoffset = var->xoffset;
	fb_display[con].var.yoffset = var->yoffset;
	if(var->vmode & FB_VMODE_YWRAP)
		fb_display[con].var.vmode |= FB_VMODE_YWRAP;
	else
		fb_display[con].var.vmode &= ~FB_VMODE_YWRAP;

	return 0;
}

static int
sisfb_mmap(struct fb_info *info, struct file *file,
		      struct vm_area_struct *vma)
{
	struct fb_var_screeninfo var;
	unsigned long start;
	unsigned long off;
	unsigned long len, mmio_off;

	if(vma->vm_pgoff > (~0UL >> PAGE_SHIFT)) return -EINVAL;

	off = vma->vm_pgoff << PAGE_SHIFT;

	start = (unsigned long) ivideo.video_base;
	len = PAGE_ALIGN((start & ~PAGE_MASK) + ivideo.video_size);
#if 0
	if (off >= len) {
		off -= len;
#endif
	/* By Jake Page: Treat mmap request with offset beyond heapstart
	 *               as request for mapping the mmio area 
	 */
	mmio_off = PAGE_ALIGN((start & ~PAGE_MASK) + ivideo.heapstart);
	if(off >= mmio_off) {
		off -= mmio_off;		
		sisfb_get_var(&var, currcon, info);
		if(var.accel_flags) return -EINVAL;

		start = (unsigned long) ivideo.mmio_base;
		len = PAGE_ALIGN((start & ~PAGE_MASK) + ivideo.mmio_size);
	}

	start &= PAGE_MASK;
	if((vma->vm_end - vma->vm_start + off) > len)	return -EINVAL;

	off += start;
	vma->vm_pgoff = off >> PAGE_SHIFT;
	vma->vm_flags |= VM_IO;   /* by Jake Page; is that really needed? */

#if defined(__i386__) || defined(__x86_64__)
	if(boot_cpu_data.x86 > 3)
		pgprot_val(vma->vm_page_prot) |= _PAGE_PCD;
#endif
        /* RedHat requires vma as the first paramater to the following call */
	if(io_remap_page_range(vma->vm_start, off, vma->vm_end - vma->vm_start,
			       vma->vm_page_prot))
		return -EAGAIN;

	return 0;
}

static void
sis_get_glyph(struct fb_info *info, SIS_GLYINFO *gly)
{
	struct display *p = &fb_display[currcon];
	u16 c;
	u8 *cdat;
	int widthb;
	u8 *gbuf = gly->gmask;
	int size;

	gly->fontheight = fontheight(p);
	gly->fontwidth = fontwidth(p);
	widthb = (fontwidth(p) + 7) / 8;

	c = gly->ch & p->charmask;
	if(fontwidth(p) <= 8)
		cdat = p->fontdata + c * fontheight(p);
	else
		cdat = p->fontdata + (c * fontheight(p) << 1);

	size = fontheight(p) * widthb;
	memcpy(gbuf, cdat, size);
	gly->ngmask = size;
}

static int
sisfb_update_var(int con, struct fb_info *info)
{
        return(sisfb_pan_var(&fb_display[con].var));
}

static int
sisfb_switch(int con, struct fb_info *info)
{
	int cols, rows;

        if(fb_display[currcon].cmap.len)
		fb_get_cmap(&fb_display[currcon].cmap, 1, sis_getcolreg, info);

	fb_display[con].var.activate = FB_ACTIVATE_NOW;

	if(!memcmp(&fb_display[con].var, &fb_display[currcon].var,
	                           sizeof(struct fb_var_screeninfo))) {
		currcon = con;
		return 1;
	}

	currcon = con;

	sisfb_do_set_var(&fb_display[con].var, 1, info);

	sisfb_set_disp(con, &fb_display[con].var, info);

	sisfb_do_install_cmap(con, info);

	cols = sisbios_mode[sisfb_mode_idx].cols;
	rows = sisbios_mode[sisfb_mode_idx].rows;
	vc_resize_con(rows, cols, fb_display[con].conp->vc_num);

	sisfb_update_var(con, info);

	return 1;
}

static void
sisfb_blank(int blank, struct fb_info *info)
{
	sisfb_myblank(blank);
}
#endif

/* ------------ FBDev related routines for 2.5 series ----------- */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)

static int
sisfb_open(struct fb_info *info, int user)
{
    return 0;
}

static int
sisfb_release(struct fb_info *info, int user)
{
    return 0;
}

static int
sisfb_get_cmap_len(const struct fb_var_screeninfo *var)
{
	int rc = 16;		

	switch(var->bits_per_pixel) {
	case 8:
		rc = 256;	
		break;
	case 16:
	case 32:
		rc = 16;
		break;
	}
	return rc;
}

static int
sisfb_setcolreg(unsigned regno, unsigned red, unsigned green, unsigned blue,
		unsigned transp, struct fb_info *info)
{
	if(regno >= sisfb_get_cmap_len(&info->var))
		return 1;

	switch(info->var.bits_per_pixel) {
	case 8:
	        outSISREG(SISDACA, regno);
		outSISREG(SISDACD, (red >> 10));
		outSISREG(SISDACD, (green >> 10));
		outSISREG(SISDACD, (blue >> 10));
		if(ivideo.currentvbflags & VB_DISPTYPE_DISP2) {
		        outSISREG(SISDAC2A, regno);
			outSISREG(SISDAC2D, (red >> 8));
			outSISREG(SISDAC2D, (green >> 8));
			outSISREG(SISDAC2D, (blue >> 8));
		}
		break;
	case 16:
		((u32 *)(info->pseudo_palette))[regno] =
		    ((red & 0xf800)) | ((green & 0xfc00) >> 5) | ((blue & 0xf800) >> 11);
		break;
	case 32:
		red >>= 8;
		green >>= 8;
		blue >>= 8;
		((u32 *) (info->pseudo_palette))[regno] =
			(red << 16) | (green << 8) | (blue);
		break;
	}
	return 0;
}

static int
sisfb_set_par(struct fb_info *info)
{
	int err;

        if((err = sisfb_do_set_var(&info->var, 1, info)))
		return err;

	sisfb_get_fix(&info->fix, info->currcon, info);

	return 0;
}

static int
sisfb_check_var(struct fb_var_screeninfo *var,
                   struct fb_info *info)
{
	unsigned int htotal = 0, vtotal = 0, myrateindex = 0;
	unsigned int drate = 0, hrate = 0, maxyres;
	int found_mode = 0;
	int refresh_rate, search_idx;
	BOOLEAN recalc_clock = FALSE;
	u32 pixclock;

	htotal = var->left_margin + var->xres + var->right_margin + var->hsync_len;

	vtotal = var->upper_margin + var->lower_margin + var->vsync_len;

	pixclock = var->pixclock;

	if((var->vmode & FB_VMODE_MASK) == FB_VMODE_NONINTERLACED) {
		vtotal += var->yres;
		vtotal <<= 1;
	} else if((var->vmode & FB_VMODE_MASK) == FB_VMODE_DOUBLE) {
		vtotal += var->yres;
		vtotal <<= 2;
	} else if((var->vmode & FB_VMODE_MASK) == FB_VMODE_INTERLACED) {
		vtotal += var->yres;
		vtotal <<= 1;
	} else 	vtotal += var->yres;

	if(!(htotal) || !(vtotal)) {
		SISFAIL("sisfb: no valid timing data");
	}

	search_idx = 0;
	while( (sisbios_mode[search_idx].mode_no != 0) &&
	       (sisbios_mode[search_idx].xres <= var->xres) ) {
		if( (sisbios_mode[search_idx].xres == var->xres) &&
		    (sisbios_mode[search_idx].yres == var->yres) &&
		    (sisbios_mode[search_idx].bpp == var->bits_per_pixel)) {
		        if(sisfb_validate_mode(search_idx, ivideo.currentvbflags) > 0) {
			   found_mode = 1;
			   break;
			}
		}
		search_idx++;
	}

	if(!found_mode) {
                search_idx = 0;
		while(sisbios_mode[search_idx].mode_no != 0) {
		   if( (var->xres <= sisbios_mode[search_idx].xres) &&
		       (var->yres <= sisbios_mode[search_idx].yres) &&
		       (var->bits_per_pixel == sisbios_mode[search_idx].bpp) ) {
		          if(sisfb_validate_mode(search_idx, ivideo.currentvbflags) > 0) {
			     found_mode = 1;
			     break;
			  }
		   }
		   search_idx++;
	        }
		if(found_mode) {
			printk(KERN_DEBUG "sisfb: Adapted from %dx%dx%d to %dx%dx%d\n",
		   		var->xres, var->yres, var->bits_per_pixel,
				sisbios_mode[search_idx].xres,
				sisbios_mode[search_idx].yres,
				var->bits_per_pixel);
			var->xres = sisbios_mode[search_idx].xres;
		      	var->yres = sisbios_mode[search_idx].yres;


		} else {
		   	printk(KERN_ERR "sisfb: Failed to find supported mode near %dx%dx%d\n",
				var->xres, var->yres, var->bits_per_pixel);
		   	return -EINVAL;
		}
	}

	if( ((ivideo.vbflags & VB_LVDS) ||			/* Slave modes on LVDS and 301B-DH */
	     ((ivideo.vbflags & VB_30xBDH) && (ivideo.currentvbflags & CRT2_LCD))) &&
	    (var->bits_per_pixel == 8) ) {
	    	refresh_rate = 60;
		recalc_clock = TRUE;
	} else if( (ivideo.current_htotal == htotal) &&		/* x=x & y=y & c=c -> assume depth change */
	    	   (ivideo.current_vtotal == vtotal) &&
	    	   (ivideo.current_pixclock == pixclock) ) {
		drate = 1000000000 / pixclock;
	        hrate = (drate * 1000) / htotal;
	        refresh_rate = (unsigned int) (hrate * 2 / vtotal);
	} else if( ( (ivideo.current_htotal != htotal) ||	/* x!=x | y!=y & c=c -> invalid pixclock */
	    	     (ivideo.current_vtotal != vtotal) ) &&
	    	   (ivideo.current_pixclock == var->pixclock) ) {
		if(sisfb_lastrates[sisbios_mode[search_idx].mode_no]) {
			refresh_rate = sisfb_lastrates[sisbios_mode[search_idx].mode_no];
		} else if(sisfb_parm_rate != -1) {
			refresh_rate = sisfb_parm_rate;
		} else {
			refresh_rate = 60;
		}
		recalc_clock = TRUE;
	} else if((pixclock) && (htotal) && (vtotal)) {
		drate = 1000000000 / pixclock;
	   	hrate = (drate * 1000) / htotal;
	   	refresh_rate = (unsigned int) (hrate * 2 / vtotal);
	} else if(ivideo.current_refresh_rate) {
		refresh_rate = ivideo.current_refresh_rate;
		recalc_clock = TRUE;
	} else {
		refresh_rate = 60;
		recalc_clock = TRUE;
	}

	myrateindex = sisfb_search_refresh_rate(refresh_rate, search_idx);

	/* Eventually recalculate timing and clock */
	if(recalc_clock) {
	   if(!myrateindex) myrateindex = sisbios_mode[search_idx].rate_idx;
	   var->pixclock = (u32) (1000000000 / sisfb_mode_rate_to_dclock(&SiS_Pr, &sishw_ext,
						sisbios_mode[search_idx].mode_no, myrateindex));
	   sisfb_mode_rate_to_ddata(&SiS_Pr, &sishw_ext,
		 			sisbios_mode[search_idx].mode_no, myrateindex,
		 			&var->left_margin, &var->right_margin,
		 			&var->upper_margin, &var->lower_margin,
		 			&var->hsync_len, &var->vsync_len,
		 			&var->sync, &var->vmode);
	   if((var->vmode & FB_VMODE_MASK) == FB_VMODE_DOUBLE) {
		var->pixclock <<= 1;
	   }
	}

	if(sisfb_thismonitor.datavalid) {
	   if(!sisfb_verify_rate(&sisfb_thismonitor, search_idx,
	                         myrateindex, refresh_rate)) {
	      printk(KERN_INFO "sisfb: WARNING: Refresh rate exceeds monitor specs!\n");
	   }
	}

	/* Adapt RGB settings */
	sisfb_bpp_to_var(var);	
	
	/* Sanity check for offsets */
	if (var->xoffset < 0)
		var->xoffset = 0;
	if (var->yoffset < 0)
		var->yoffset = 0;

	/* Horiz-panning not supported */
	if(var->xres != var->xres_virtual)
		var->xres_virtual = var->xres;
	
	if(sisfb_ypan) {
	   maxyres = ivideo.heapstart / (var->xres * (var->bits_per_pixel >> 3));
	   if(maxyres > 32767) maxyres = 32767;
	   if(sisfb_max) {
	      var->yres_virtual = maxyres;
	   } else {
	      if(var->yres_virtual > maxyres) {
	         var->yres_virtual = maxyres;
	      }
	   }
	   if(var->yres_virtual <= var->yres) {
	      var->yres_virtual = var->yres;
	   }
	} else {
	   if(var->yres != var->yres_virtual) {
	      var->yres_virtual = var->yres;
	   }
	   var->xoffset = 0;
	   var->yoffset = 0;
	}
	
	/* Truncate offsets to maximum if too high */
	if(var->xoffset > var->xres_virtual - var->xres)
		var->xoffset = var->xres_virtual - var->xres - 1;

	if(var->yoffset > var->yres_virtual - var->yres)
		var->yoffset = var->yres_virtual - var->yres - 1;
	
	/* Set everything else to 0 */
	var->red.msb_right = 
	    var->green.msb_right =
	    var->blue.msb_right =
	    var->transp.offset = var->transp.length = var->transp.msb_right = 0;		

	return 0;
}

static int
sisfb_pan_display(struct fb_var_screeninfo *var,
		     struct fb_info* info)
{
	int err;

	if(var->xoffset > (var->xres_virtual - var->xres))
		return -EINVAL;
	if(var->yoffset > (var->yres_virtual - var->yres))
		return -EINVAL;

	if(var->vmode & FB_VMODE_YWRAP) {
		if(var->yoffset < 0 ||
		   var->yoffset >= info->var.yres_virtual ||
		   var->xoffset)
		    	return -EINVAL;
	} else {
		if(var->xoffset + info->var.xres > info->var.xres_virtual ||
		   var->yoffset + info->var.yres > info->var.yres_virtual)
			return -EINVAL;
	}

	if((err = sisfb_pan_var(var)) < 0) return err;

	info->var.xoffset = var->xoffset;
	info->var.yoffset = var->yoffset;
	if(var->vmode & FB_VMODE_YWRAP)
		info->var.vmode |= FB_VMODE_YWRAP;
	else
		info->var.vmode &= ~FB_VMODE_YWRAP;

	return 0;
}

static int
sisfb_mmap(struct fb_info *info, struct file *file,
		      struct vm_area_struct *vma)
{
	unsigned long start;
	unsigned long off;
	unsigned long len, mmio_off;

	if(vma->vm_pgoff > (~0UL >> PAGE_SHIFT))  return -EINVAL;

	off = vma->vm_pgoff << PAGE_SHIFT;

	start = (unsigned long) ivideo.video_base;
	len = PAGE_ALIGN((start & ~PAGE_MASK) + ivideo.video_size);
#if 0
	if (off >= len) {
		off -= len;
#endif
	/* By Jake Page: Treat mmap request with offset beyond heapstart
	 *               as request for mapping the mmio area 
	 */
	mmio_off = PAGE_ALIGN((start & ~PAGE_MASK) + ivideo.heapstart);
	if(off >= mmio_off) {
		off -= mmio_off;		
		if(info->var.accel_flags) return -EINVAL;

		start = (unsigned long) ivideo.mmio_base;
		len = PAGE_ALIGN((start & ~PAGE_MASK) + ivideo.mmio_size);
	}

	start &= PAGE_MASK;
	if((vma->vm_end - vma->vm_start + off) > len)	return -EINVAL;

	off += start;
	vma->vm_pgoff = off >> PAGE_SHIFT;
	vma->vm_flags |= VM_IO;   /* by Jake Page; is that really needed? */

#if defined(__i386__) || defined(__x86_64__)
	if (boot_cpu_data.x86 > 3)
		pgprot_val(vma->vm_page_prot) |= _PAGE_PCD;
#endif
	if (io_remap_page_range(vma, vma->vm_start, off, vma->vm_end - vma->vm_start,
				vma->vm_page_prot))
		return -EAGAIN;

	return 0;
}

static int
sisfb_blank(int blank, struct fb_info *info)
{
	return(sisfb_myblank(blank));
}

#endif

/* ----------- FBDev related routines for all series ---------- */


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
static int sisfb_ioctl(struct inode *inode, struct file *file,
		       unsigned int cmd, unsigned long arg,
		       struct fb_info *info)
#else
static int sisfb_ioctl(struct inode *inode, struct file *file,
		       unsigned int cmd, unsigned long arg, int con,
		       struct fb_info *info)
#endif
{
	struct sis_memreq sismemreq;
	struct ap_data sisapdata;
	unsigned long sismembase = 0;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	SIS_GLYINFO sisglyinfo;
#endif

	switch (cmd) {
	   case FBIO_ALLOC:
		if(!capable(CAP_SYS_RAWIO))
			return -EPERM;
		if(copy_from_user(&sismemreq, (void *)arg, sizeof(sismemreq)))
		   	return -EFAULT;
        	sis_malloc(&sismemreq);
		if(copy_to_user((void *)arg, &sismemreq, sizeof(sismemreq))) {
			sis_free(sismemreq.offset);
		    	return -EFAULT;
		}
		break;
	   case FBIO_FREE:
		if(!capable(CAP_SYS_RAWIO))
			return -EPERM;
		if(get_user(sismembase, (unsigned long *) arg))
			return -EFAULT;
		sis_free(sismembase);
		break;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	   case FBIOGET_GLYPH:
	        if(copy_from_user(&sisglyinfo, (void *)arg, sizeof(sisglyinfo)))
			return -EFAULT;
                sis_get_glyph(info, &sisglyinfo);
		break;
	   case FBIOPUT_MODEINFO:
		{
			struct mode_info x;

			if(copy_from_user(&x, (void *)arg, sizeof(x)))
				return -EFAULT;

			ivideo.video_bpp        = x.bpp;
			ivideo.video_width      = x.xres;
			ivideo.video_height     = x.yres;
			ivideo.video_vwidth     = x.v_xres;
			ivideo.video_vheight    = x.v_yres;
			ivideo.org_x            = x.org_x;
			ivideo.org_y            = x.org_y;
			ivideo.refresh_rate     = x.vrate;
			ivideo.video_linelength = ivideo.video_vwidth * (ivideo.video_bpp >> 3);
			sisfb_set_vparms();
			break;
		}
#endif
	   case FBIOGET_HWCINFO:
		{
			unsigned long myhwcoffset = 0;

			if(ivideo.caps & HW_CURSOR_CAP)
				myhwcoffset = ivideo.hwcursor_vbase -
				    (unsigned long) ivideo.video_vbase;

			return put_user(myhwcoffset, (unsigned long *)arg);

			break;
		}
	   case FBIOGET_DISPINFO:
	   	sis_dispinfo(&sisapdata);
		if(copy_to_user((void *)arg, &sisapdata, sizeof(sisapdata)))
			return -EFAULT;
		break;
	   case SISFB_GET_INFO:  /* For communication with X driver */
	        {
			sisfb_info x;

			x.sisfb_id = SISFB_ID;
			x.sisfb_version = VER_MAJOR;
			x.sisfb_revision = VER_MINOR;
			x.sisfb_patchlevel = VER_LEVEL;
			x.chip_id = ivideo.chip_id;
			x.memory = ivideo.video_size / 1024;
			x.heapstart = ivideo.heapstart / 1024;
			if(ivideo.modechanged) {
			   x.fbvidmode = ivideo.mode_no;
			} else {
			   x.fbvidmode = ivideo.modeprechange;
			}
			x.sisfb_caps = ivideo.caps;
			x.sisfb_tqlen = 512; /* yet fixed */
			x.sisfb_pcibus = ivideo.pcibus;
			x.sisfb_pcislot = ivideo.pcislot;
			x.sisfb_pcifunc = ivideo.pcifunc;
			x.sisfb_lcdpdc = ivideo.detectedpdc;
			x.sisfb_lcdpdca = ivideo.detectedpdca;
			x.sisfb_lcda = ivideo.detectedlcda;
			x.sisfb_vbflags = ivideo.vbflags;
			x.sisfb_currentvbflags = ivideo.currentvbflags;
			x.sisfb_scalelcd = SiS_Pr.UsePanelScaler;
			x.sisfb_specialtiming = SiS_Pr.SiS_CustomT;
			x.sisfb_haveemi = SiS_Pr.HaveEMI ? 1 : 0;
			x.sisfb_haveemilcd = SiS_Pr.HaveEMILCD ? 1 : 0;
			x.sisfb_emi30 = SiS_Pr.EMI_30;
			x.sisfb_emi31 = SiS_Pr.EMI_31;
			x.sisfb_emi32 = SiS_Pr.EMI_32;
			x.sisfb_emi33 = SiS_Pr.EMI_33;
			if(copy_to_user((void *)arg, &x, sizeof(x)))
				return -EFAULT;
	                break;
		}
	   case SISFB_GET_VBRSTATUS:
	        {
			if(sisfb_CheckVBRetrace())
				return put_user(1UL, (unsigned long *) arg);
			else
				return put_user(0UL, (unsigned long *) arg);
			break;
		}
	   case SISFB_GET_AUTOMAXIMIZE:
	        {
			if(sisfb_max)
				return put_user(1UL, (unsigned long *) arg);
			else
				return put_user(0UL, (unsigned long *) arg);
			break;
		}
	   case SISFB_SET_AUTOMAXIMIZE:
	        {
			unsigned long newmax;

			if(copy_from_user(&newmax, (unsigned long *)arg, sizeof(newmax)))
				return -EFAULT;

			if(newmax) sisfb_max = 1;
			else	   sisfb_max = 0;
			break;
		}
	   default:
		return -EINVAL;
	}
	return 0;
}


static int
sisfb_get_fix(struct fb_fix_screeninfo *fix, int con,
	      struct fb_info *info)
{
	memset(fix, 0, sizeof(struct fb_fix_screeninfo));

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)	
	strcpy(fix->id, sis_fb_info->modename);
#else
	strcpy(fix->id, myid);
#endif	

	fix->smem_start = ivideo.video_base;

        if((!sisfb_mem) || (sisfb_mem > (ivideo.video_size/1024))) {
	    if(ivideo.sisvga_engine == SIS_300_VGA) {
	       if(ivideo.video_size > 0x1000000) {
	          	fix->smem_len = 0xc00000;
	       } else if(ivideo.video_size > 0x800000)
		  	fix->smem_len = 0x800000;
	       else
		  	fix->smem_len = 0x400000;
            } else {
	       	fix->smem_len = ivideo.video_size - 0x100000;
	    }
        } else
		fix->smem_len = sisfb_mem * 1024;

	fix->type        = FB_TYPE_PACKED_PIXELS;
	fix->type_aux    = 0;
	if(ivideo.video_bpp == 8)
	   fix->visual = FB_VISUAL_PSEUDOCOLOR;
	else
	   fix->visual = FB_VISUAL_TRUECOLOR;
	fix->xpanstep    = 0;

        if(sisfb_ypan) 	 fix->ypanstep = 1;

	fix->ywrapstep   = 0;
	fix->line_length = ivideo.video_linelength;
	fix->mmio_start  = ivideo.mmio_base;
	fix->mmio_len    = ivideo.mmio_size;
	if(ivideo.sisvga_engine == SIS_300_VGA) 
	   fix->accel    = FB_ACCEL_SIS_GLAMOUR;
	else if((ivideo.chip == SIS_330) || (ivideo.chip == SIS_760))
	   fix->accel    = FB_ACCEL_SIS_XABRE;
	else
	   fix->accel    = FB_ACCEL_SIS_GLAMOUR_2;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	fix->reserved[0] = ivideo.video_size & 0xFFFF;
	fix->reserved[1] = (ivideo.video_size >> 16) & 0xFFFF;
	fix->reserved[2] = ivideo.caps;
#endif

	return 0;
}

/* ----------------  fb_ops structures ----------------- */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
static struct fb_ops sisfb_ops = {
	.owner		= THIS_MODULE,
	.fb_get_fix	= sisfb_get_fix,
	.fb_get_var	= sisfb_get_var,
	.fb_set_var	= sisfb_set_var,
	.fb_get_cmap	= sisfb_get_cmap,
	.fb_set_cmap	= sisfb_set_cmap,
        .fb_pan_display = sisfb_pan_display,
	.fb_ioctl	= sisfb_ioctl,
	.fb_mmap	= sisfb_mmap,
};
#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
static struct fb_ops sisfb_ops = {
	.owner        =	THIS_MODULE,
	.fb_open      = sisfb_open,
	.fb_release   = sisfb_release,
	.fb_check_var = sisfb_check_var,
	.fb_set_par   = sisfb_set_par,
	.fb_setcolreg = sisfb_setcolreg,
        .fb_pan_display = sisfb_pan_display,
        .fb_blank     = sisfb_blank,
	.fb_fillrect  = fbcon_sis_fillrect,
	.fb_copyarea  = fbcon_sis_copyarea,
	.fb_imageblit = cfb_imageblit,
	.fb_cursor    = soft_cursor,	
	.fb_sync      = fbcon_sis_sync,
	.fb_ioctl     =	sisfb_ioctl,
	.fb_mmap      =	sisfb_mmap,
};
#endif


/* ---------------- Chip generation dependent routines ---------------- */

#ifdef CONFIG_FB_SIS_300 /* for SiS 300/630/540/730 */

static int __devinit sisfb_get_dram_size_300(void)
{
	struct pci_dev *pdev = NULL;
	int pdev_valid = 0;
	u8  pci_data, reg;
	u16 nbridge_id;

	switch(ivideo.chip) {
	   case SIS_540:
		nbridge_id = PCI_DEVICE_ID_SI_540;
		break;
	   case SIS_630:
		nbridge_id = PCI_DEVICE_ID_SI_630;
		break;
	   case SIS_730:
		nbridge_id = PCI_DEVICE_ID_SI_730;
		break;
	   default:
		nbridge_id = 0;
		break;
	}

	if(nbridge_id == 0) {   /* 300 */

	        inSISIDXREG(SISSR, IND_SIS_DRAM_SIZE,reg);
		ivideo.video_size =
		        ((unsigned int) ((reg & SIS_DRAM_SIZE_MASK) + 1) << 20);

	} else {		/* 540, 630, 730 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,74)
		pci_for_each_dev(pdev) {
#else
		while((pdev = pci_find_device(PCI_VENDOR_ID_SI, PCI_ANY_ID, pdev))) {
#endif
			if ((pdev->vendor == PCI_VENDOR_ID_SI)
				       && (pdev->device == nbridge_id)) {
				pci_read_config_byte(pdev, IND_BRI_DRAM_STATUS, &pci_data);
				pci_data = (pci_data & BRI_DRAM_SIZE_MASK) >> 4;
				ivideo.video_size = (unsigned int)(1 << (pci_data+21));
				pdev_valid = 1;

				reg = SIS_DATA_BUS_64 << 6;
				switch (pci_data) {
				   case BRI_DRAM_SIZE_2MB:
					reg |= SIS_DRAM_SIZE_2MB;
					break;
				   case BRI_DRAM_SIZE_4MB:
					reg |= SIS_DRAM_SIZE_4MB;
					break;
				   case BRI_DRAM_SIZE_8MB:
					reg |= SIS_DRAM_SIZE_8MB;
					break;
				   case BRI_DRAM_SIZE_16MB:
					reg |= SIS_DRAM_SIZE_16MB;
					break;
				   case BRI_DRAM_SIZE_32MB:
					reg |= SIS_DRAM_SIZE_32MB;
					break;
				   case BRI_DRAM_SIZE_64MB:
					reg |= SIS_DRAM_SIZE_64MB;
					break;
				}
				outSISIDXREG(SISSR, IND_SIS_DRAM_SIZE, reg);
				break;
			}  
		}   
	
		if (!pdev_valid)  return -1;
	}
	return 0;
}

#endif  /* CONFIG_FB_SIS_300 */


#ifdef CONFIG_FB_SIS_315    /* for SiS 315/550/650/740/330/661/741/760 */

static int __devinit sisfb_get_dram_size_315(void)
{
	u8  reg = 0;

	if(ivideo.chip == SIS_550 ||
	   ivideo.chip == SIS_650 ||
	   ivideo.chip == SIS_740) {

                inSISIDXREG(SISSR, IND_SIS_DRAM_SIZE, reg);
		reg &= 0x3f;
		reg++;
		reg <<= 2;
		ivideo.video_size = reg << 20;
		return 0;

	} else if(ivideo.chip == SIS_661 ||
	          ivideo.chip == SIS_741) {

		inSISIDXREG(SISCR, 0x79, reg);
		reg &= 0xf0;
		reg >>= 4;
		ivideo.video_size = (1 << reg) << 20;
		return 0;

	} else if(ivideo.chip == SIS_660 ||
		  ivideo.chip == SIS_760) {

		ivideo.video_size = 0;
		inSISIDXREG(SISCR, 0x79, reg);
		reg &= 0xf0;
		reg >>= 4;
		if(reg)	ivideo.video_size = (1 << reg) << 20;
		inSISIDXREG(SISCR, 0x78, reg);
		reg &= 0x30;
		if(reg) {
		   if(reg == 0x10) ivideo.video_size += (32 << 20);
		   else		   ivideo.video_size += (64 << 20);
		}
		return 0;

	} else {	/* 315, 330 */

	        inSISIDXREG(SISSR, IND_SIS_DRAM_SIZE, reg);
		ivideo.video_size = (1 << ((reg & 0xf0) >> 4)) << 20;

		reg &= SIS315_DUAL_CHANNEL_MASK;
		reg >>= 2;

		if(ivideo.chip == SIS_330) {

		   if(reg) ivideo.video_size <<= 1;
		
		} else {
		   
		   switch (reg) {
		      case SIS315_SINGLE_CHANNEL_2_RANK:
			   ivideo.video_size <<= 1;
			   break;
		      case SIS315_DUAL_CHANNEL_1_RANK:
			   ivideo.video_size <<= 1;
			   break;
		      case SIS315_ASYM_DDR:		/* TW: DDR asymetric */
			   ivideo.video_size += (ivideo.video_size/2);
			   break;
		   }
		}

		return 0;
	}
	
	return -1;
	
}

#endif   /* CONFIG_FB_SIS_315 */


/* -------------- video bridge detection --------------- */

static void __devinit sisfb_detect_VB_connect()
{
	u8 sr16, sr17, cr32, temp;

	if(ivideo.sisvga_engine == SIS_300_VGA) {
	
		inSISIDXREG(SISSR, IND_SIS_SCRATCH_REG_17, sr17);
	      
		if ((sr17 & 0x0F) && (ivideo.chip != SIS_300)) {

			/* PAL/NTSC is stored on SR16 on such machines */
			if (!(ivideo.vbflags & (TV_PAL | TV_NTSC | TV_PALM | TV_PALN))) {
		   		inSISIDXREG(SISSR, IND_SIS_SCRATCH_REG_16, sr16);
				if (sr16 & 0x20)
					ivideo.vbflags |= TV_PAL;
				else
					ivideo.vbflags |= TV_NTSC;
			}

		} 
		
	}
	
	inSISIDXREG(SISCR, IND_SIS_SCRATCH_REG_CR32, cr32);

	if(cr32 & SIS_CRT1) {
		sisfb_crt1off = 0;
	} else {
		if (cr32 & 0x5F) sisfb_crt1off = 1;
		else		 sisfb_crt1off = 0;
	}

	ivideo.vbflags &= ~(CRT2_TV | CRT2_LCD | CRT2_VGA);

	if(cr32 & SIS_VB_TV)   ivideo.vbflags |= CRT2_TV;
	if(cr32 & SIS_VB_LCD)  ivideo.vbflags |= CRT2_LCD;
	if(cr32 & SIS_VB_CRT2) ivideo.vbflags |= CRT2_VGA;
		
	/* Detect/set TV plug & type */
	if(sisfb_tvplug != -1) ivideo.vbflags |= sisfb_tvplug;

	if(cr32 & SIS_VB_SVIDEO)	  ivideo.vbflags |= TV_SVIDEO;
	else if (cr32 & SIS_VB_COMPOSITE) ivideo.vbflags |= TV_AVIDEO;
	else if (cr32 & SIS_VB_SCART) {
		ivideo.vbflags |= (TV_SCART | TV_PAL);
		ivideo.vbflags &= ~(TV_NTSC | TV_PALM | TV_PALN);
	}

	if(!(ivideo.vbflags & (TV_PAL | TV_NTSC | TV_PALM | TV_PALN))) {
		if(ivideo.sisvga_engine == SIS_300_VGA) {
	        	inSISIDXREG(SISSR, IND_SIS_POWER_ON_TRAP, temp);
			if(temp & 0x01) ivideo.vbflags |= TV_PAL;
			else		ivideo.vbflags |= TV_NTSC;
		} else if((ivideo.chip <= SIS_315PRO) || (ivideo.chip >= SIS_330)) {
                	inSISIDXREG(SISSR, 0x38, temp);
			if(temp & 0x01) ivideo.vbflags |= TV_PAL;
			else		ivideo.vbflags |= TV_NTSC;
	    	} else {
	        	inSISIDXREG(SISCR, 0x79, temp);
			if(temp & 0x20)	ivideo.vbflags |= TV_PAL;
			else		ivideo.vbflags |= TV_NTSC;
	    	}
	}

	/* Copy forceCRT1 option to CRT1off if option is given */
    	if(sisfb_forcecrt1 != -1) {
    		if(sisfb_forcecrt1) sisfb_crt1off = 0;
		else                sisfb_crt1off = 1;
    	}

}

static void __devinit sisfb_get_VB_type(void)
{
	u8 vb_chipid;
	u8 reg;
	char stdstr[]    = "sisfb: Detected";
	char bridgestr[] = "video bridge";
	char lvdsstr[]   = "LVDS transmitter";
  	char chrstr[]    = "Chrontel TV encoder";
	
	ivideo.hasVB = HASVB_NONE;
	sishw_ext.ujVBChipID = VB_CHIP_UNKNOWN;
	sishw_ext.Is301BDH = FALSE;

	inSISIDXREG(SISPART4, 0x00, vb_chipid);
	switch (vb_chipid) {
	   case 0x01:
		ivideo.hasVB = HASVB_301;
		inSISIDXREG(SISPART4, 0x01, reg);
		if(reg < 0xb0) {
			ivideo.vbflags |= VB_301;
			sishw_ext.ujVBChipID = VB_CHIP_301;
			printk(KERN_INFO "%s SiS301 %s\n", stdstr, bridgestr);
		} else if(reg < 0xc0) {
		 	ivideo.vbflags |= VB_301B;
			sishw_ext.ujVBChipID = VB_CHIP_301B;
			inSISIDXREG(SISPART4,0x23,reg);
			if(!(reg & 0x02)) {
			   sishw_ext.Is301BDH = TRUE;
			   ivideo.vbflags |= VB_30xBDH;
			   printk(KERN_INFO "%s SiS301B-DH %s\n", stdstr, bridgestr);
			} else {
			   printk(KERN_INFO "%s SiS301B %s\n", stdstr, bridgestr);
			}
		} else if(reg < 0xd0) {
		 	ivideo.vbflags |= VB_301C;
			sishw_ext.ujVBChipID = VB_CHIP_301C;
			printk(KERN_INFO "%s SiS301C %s\n", stdstr, bridgestr);
		} else if(reg < 0xe0) {
			ivideo.vbflags |= VB_301LV;
			sishw_ext.ujVBChipID = VB_CHIP_301LV;
			printk(KERN_INFO "%s SiS301LV %s\n", stdstr, bridgestr);
		} else if(reg <= 0xe1) {
		        inSISIDXREG(SISPART4,0x39,reg);
			if(reg == 0xff) {
			   ivideo.vbflags |= VB_302LV;
			   sishw_ext.ujVBChipID = VB_CHIP_302LV;
			   printk(KERN_INFO "%s SiS302LV %s\n", stdstr, bridgestr);
			} else {
			   ivideo.vbflags |= VB_302ELV;
			   sishw_ext.ujVBChipID = VB_CHIP_302ELV;
			   printk(KERN_INFO "%s SiS302ELV %s\n", stdstr, bridgestr);
			}
		}
		break;
	   case 0x02:
		ivideo.hasVB = HASVB_302;
		ivideo.vbflags |= VB_302B;
		sishw_ext.ujVBChipID = VB_CHIP_302B;
		printk(KERN_INFO "%s SiS302B %s\n", stdstr, bridgestr);
		break;
	}

	if((!(ivideo.vbflags & VB_VIDEOBRIDGE)) && (ivideo.chip != SIS_300)) {
		inSISIDXREG(SISCR, IND_SIS_SCRATCH_REG_CR37, reg);
		reg &= SIS_EXTERNAL_CHIP_MASK;
		reg >>= 1;
		if(ivideo.sisvga_engine == SIS_300_VGA) {
#ifdef CONFIG_FB_SIS_300
			switch (reg) {
			   case SIS_EXTERNAL_CHIP_LVDS:
				ivideo.hasVB = HASVB_LVDS;
				ivideo.vbflags |= VB_LVDS;
				printk(KERN_INFO "%s %s\n", stdstr, lvdsstr);
				break;
			   case SIS_EXTERNAL_CHIP_TRUMPION:
				ivideo.hasVB = HASVB_TRUMPION;
				ivideo.vbflags |= VB_TRUMPION;
				printk(KERN_INFO "%s Trumpion LCD scaler\n", stdstr);
				break;
			   case SIS_EXTERNAL_CHIP_CHRONTEL:
				ivideo.hasVB = HASVB_CHRONTEL;
				ivideo.vbflags |= VB_CHRONTEL;
				printk(KERN_INFO "%s %s\n", stdstr, chrstr);
				break;
			   case SIS_EXTERNAL_CHIP_LVDS_CHRONTEL:
				ivideo.hasVB = HASVB_LVDS_CHRONTEL;
				ivideo.vbflags |= (VB_LVDS | VB_CHRONTEL);
				printk(KERN_INFO "%s %s and %s\n", stdstr, lvdsstr, chrstr);
				break;
			}
#endif
		} else if(ivideo.chip < SIS_661) {
#ifdef CONFIG_FB_SIS_315
			switch (reg) {
	 	   	   case SIS310_EXTERNAL_CHIP_LVDS:
				ivideo.hasVB = HASVB_LVDS;
				ivideo.vbflags |= VB_LVDS;
				printk(KERN_INFO "%s %s\n", stdstr, lvdsstr);
				break;
		   	   case SIS310_EXTERNAL_CHIP_LVDS_CHRONTEL:
				ivideo.hasVB = HASVB_LVDS_CHRONTEL;
				ivideo.vbflags |= (VB_LVDS | VB_CHRONTEL);
				printk(KERN_INFO "%s %s and %s\n", stdstr, lvdsstr, chrstr);
				break;
			}
#endif
		} else if(ivideo.chip >= SIS_661) {
#ifdef CONFIG_FB_SIS_315
			inSISIDXREG(SISCR, 0x38, reg);
			reg >>= 5;
			switch(reg) {
			   case 0x02:
			   	ivideo.hasVB = HASVB_LVDS;
				ivideo.vbflags |= VB_LVDS;
				printk(KERN_INFO "%s %s\n", stdstr, lvdsstr);
				break;
			   case 0x03:
			   	ivideo.hasVB = HASVB_LVDS_CHRONTEL;
				ivideo.vbflags |= (VB_LVDS | VB_CHRONTEL);
				printk(KERN_INFO "%s %s and %s\n", stdstr, lvdsstr, chrstr);
				break;
			   case 0x04:
			        ivideo.hasVB = HASVB_NONE;
				ivideo.vbflags |= (VB_LVDS | VB_CONEXANT);
				printk(KERN_INFO "%s Conexant video bridge\n", stdstr);
				break;
			}
#endif
		}

	}
	
	if(ivideo.vbflags & VB_SISBRIDGE) {
		SiS_Sense30x();
	} else if(ivideo.vbflags & VB_CHRONTEL) {
		SiS_SenseCh();
	}

}

/* ------------------ Sensing routines ------------------ */

static BOOLEAN __devinit sisfb_test_DDC1(void)
{
    unsigned short old;
    int count = 48;

    old = SiS_ReadDDC1Bit(&SiS_Pr);
    do {
       if(old != SiS_ReadDDC1Bit(&SiS_Pr)) break;
    } while(count--);
    return (count == -1) ? FALSE : TRUE;
}

static void __devinit sisfb_sense_crt1(void)
{
    unsigned char SR1F, CR63=0, CR17;
    unsigned short temp = 0xffff;
    int i;
    BOOLEAN mustwait = FALSE;

    inSISIDXREG(SISSR,0x1F,SR1F);
    orSISIDXREG(SISSR,0x1F,0x04);
    andSISIDXREG(SISSR,0x1F,0x3F);
    if(SR1F & 0xc0) mustwait = TRUE;

    if(ivideo.sisvga_engine == SIS_315_VGA) {
       inSISIDXREG(SISCR,SiS_Pr.SiS_MyCR63,CR63);
       CR63 &= 0x40;
       andSISIDXREG(SISCR,SiS_Pr.SiS_MyCR63,0xBF);
    }

    inSISIDXREG(SISCR,0x17,CR17);
    CR17 &= 0x80;
    if(!CR17) {
       orSISIDXREG(SISCR,0x17,0x80);
       mustwait = TRUE;
       outSISIDXREG(SISSR, 0x00, 0x01);
       outSISIDXREG(SISSR, 0x00, 0x03);
    }

    if(mustwait) {
       for(i=0; i < 10; i++) sisfbwaitretracecrt1();
    }

    i = 3;
    do {
       temp = SiS_HandleDDC(&SiS_Pr, ivideo.vbflags, ivideo.sisvga_engine, 0, 0, NULL);
    } while(((temp == 0) || (temp == 0xffff)) && i--);

    if((temp == 0) || (temp == 0xffff)) {
       if(sisfb_test_DDC1()) temp = 1;
    }

    if((temp) && (temp != 0xffff)) {
       orSISIDXREG(SISCR,0x32,0x20);
    }

    if(ivideo.sisvga_engine == SIS_315_VGA) {
       setSISIDXREG(SISCR,SiS_Pr.SiS_MyCR63,0xBF,CR63);
    }

    setSISIDXREG(SISCR,0x17,0x7F,CR17);

    outSISIDXREG(SISSR,0x1F,SR1F);
}

/* Determine and detect attached devices on SiS30x */
static int __devinit SISDoSense(int tempbl, int tempbh, int tempcl, int tempch)
{
    int temp;

    outSISIDXREG(SISPART4,0x11,tempbl);
    temp = tempbh | tempcl;
    setSISIDXREG(SISPART4,0x10,0xe0,temp);
    SiS_DDC2Delay(&SiS_Pr, 0x1000);
    tempch &= 0x7f;
    inSISIDXREG(SISPART4,0x03,temp);
    temp ^= 0x0e;
    temp &= tempch;
    return((temp == tempch));
}

static void __devinit SiS_Sense30x(void)
{
  u8 backupP4_0d,backupP2_00;
  u8 svhs_bl, svhs_bh;
  u8 svhs_cl, svhs_ch;
  u8 cvbs_bl, cvbs_bh;
  u8 cvbs_cl, cvbs_ch;
  u8 vga2_bl, vga2_bh;
  u8 vga2_cl, vga2_ch;
  int myflag, result, haveresult, i, j;
  char stdstr[] = "sisfb: Detected";
  char tvstr[]  = "TV connected to";

  inSISIDXREG(SISPART4,0x0d,backupP4_0d);
  outSISIDXREG(SISPART4,0x0d,(backupP4_0d | 0x04));

  inSISIDXREG(SISPART2,0x00,backupP2_00);
  outSISIDXREG(SISPART2,0x00,(backupP2_00 | 0x1c));

  if(ivideo.sisvga_engine == SIS_300_VGA) {

	if(ivideo.vbflags & (VB_301B|VB_301C|VB_302B|VB_301LV|VB_302LV)) {
	   	vga2_bh = 0x01; vga2_bl = 0x90;
	   	svhs_bh = 0x01; svhs_bl = 0x6b;
	   	cvbs_bh = 0x01; cvbs_bl = 0x74;
	} else {
		vga2_bh = 0x00; vga2_bl = 0xd1;
        	svhs_bh = 0x00; svhs_bl = 0xb9;
		cvbs_bh = 0x00; cvbs_bl = 0xb3;
		inSISIDXREG(SISPART4,0x01,myflag);
	        if(myflag & 0x04) {
	           vga2_bh = 0x00; vga2_bl = 0xfd;
	           svhs_bh = 0x00; svhs_bl = 0xdd;
	      	   cvbs_bh = 0x00; cvbs_bl = 0xee;
	        }
	}

	vga2_ch = 0x0e;	vga2_cl = 0x08;
	svhs_ch = 0x04;	svhs_cl = 0x04;
	cvbs_ch = 0x08; cvbs_cl = 0x04;
	if(ivideo.vbflags & (VB_301LV|VB_302LV)) {
	   	vga2_bh = 0x00; vga2_bl = 0x00;
	   	vga2_ch = 0x00; vga2_cl = 0x00;
	}
	if(ivideo.chip == SIS_300) {
	   inSISIDXREG(SISSR,0x3b,myflag);
	   if(!(myflag & 0x01)) {
	      	vga2_bh = 0x00; vga2_bl = 0x00;
	      	vga2_ch = 0x00; vga2_cl = 0x00;
	   }
	}

  } else {

	if(ivideo.vbflags & (VB_301B|VB_302B)) {
		vga2_bh = 0x01; vga2_bl = 0x90;
		svhs_bh = 0x01; svhs_bl = 0x6b;
		cvbs_bh = 0x01; cvbs_bl = 0x74;
	} else if(ivideo.vbflags & (VB_301C|VB_302ELV)) {
	      	vga2_bh = 0x01; vga2_bl = 0x90;
	      	svhs_bh = 0x01; svhs_bl = 0x6b;
	      	cvbs_bh = 0x01; cvbs_bl = 0x10;
	} else if(ivideo.vbflags & (VB_301LV|VB_302LV)) {
	      	vga2_bh = 0x00; vga2_bl = 0x00;
	      	svhs_bh = 0x02; svhs_bl = 0x00;
	      	cvbs_bh = 0x01; cvbs_bl = 0x00;
	} else {
		vga2_bh = 0x00; vga2_bl = 0xd1;
        	svhs_bh = 0x00; svhs_bl = 0xb9;
		cvbs_bh = 0x00; cvbs_bl = 0xb3;
		inSISIDXREG(SISPART4,0x01,myflag);
	        if(myflag & 0x04) {
	           vga2_bh = 0x00; vga2_bl = 0xfd;
	           svhs_bh = 0x00; svhs_bl = 0xdd;
	           cvbs_bh = 0x00; cvbs_bl = 0xee;
	        }
	}

	if(ivideo.vbflags & (VB_301LV|VB_302LV|VB_302ELV)) {
	   vga2_bh = 0x00; vga2_bl = 0x00;
	   vga2_ch = 0x00; vga2_cl = 0x00;
	   svhs_ch = 0x04; svhs_cl = 0x08;
	   cvbs_ch = 0x08; cvbs_cl = 0x08;
	} else {
	   vga2_ch = 0x0e; vga2_cl = 0x08;
	   svhs_ch = 0x04; svhs_cl = 0x04;
	   cvbs_ch = 0x08; cvbs_cl = 0x04;
	}
    } 

    if(vga2_ch || vga2_cl || vga2_bh || vga2_bl) {
       haveresult = 0;
       for(j = 0; j < 10; j++) {
          result = 0;
          for(i = 0; i < 3; i++) {
             if(SISDoSense(vga2_bl, vga2_bh, vga2_cl, vga2_ch))
	        result++;
          }
	  if((result == 0) || (result >= 2)) break;
       }
       if(result) {
          printk(KERN_INFO "%s secondary VGA connection\n", stdstr);
	  orSISIDXREG(SISCR, 0x32, 0x10);
       } else {
	  andSISIDXREG(SISCR, 0x32, ~0x10);
       }
    }

    haveresult = 0;
    for(j = 0; j < 10; j++) {
       result = 0;
       for(i = 0; i < 3; i++) {
          if(SISDoSense(svhs_bl, svhs_bh, svhs_cl, svhs_ch))
	        result++;
       }
       if((result == 0) || (result >= 2)) break;
    }
    if(result) {
        printk(KERN_INFO "%s %s SVIDEO output\n", stdstr, tvstr);
	ivideo.vbflags |= TV_SVIDEO;
	orSISIDXREG(SISCR, 0x32, 0x02);
	andSISIDXREG(SISCR, 0x32, ~0x05);
    }

    if(!result) {

	haveresult = 0;
       	for(j = 0; j < 10; j++) {
           result = 0;
           for(i = 0; i < 3; i++) {
              if(SISDoSense(cvbs_bl, cvbs_bh, cvbs_cl, cvbs_ch))
	        result++;
           }
           if((result == 0) || (result >= 2)) break;
        }
	if(result) {
	    printk(KERN_INFO "%s %s COMPOSITE output\n", stdstr, tvstr);
	    ivideo.vbflags |= TV_AVIDEO;
	    orSISIDXREG(SISCR, 0x32, 0x01);
	    andSISIDXREG(SISCR, 0x32, ~0x06);
	} else {
	    andSISIDXREG(SISCR, 0x32, ~0x07);
	}
    }
    SISDoSense(0, 0, 0, 0);

    outSISIDXREG(SISPART2,0x00,backupP2_00);
    outSISIDXREG(SISPART4,0x0d,backupP4_0d);
}

/* Determine and detect attached TV's on Chrontel */
static void __devinit SiS_SenseCh(void)
{
   u8 temp1, temp2;
#ifdef CONFIG_FB_SIS_300
   unsigned char test[3];
   int i;
#endif
   char stdstr[] = "sisfb: Chrontel: Detected TV connected to";

   if(ivideo.chip < SIS_315H) {

#ifdef CONFIG_FB_SIS_300
       SiS_Pr.SiS_IF_DEF_CH70xx = 1;		/* Chrontel 700x */
       SiS_SetChrontelGPIO(&SiS_Pr, 0x9c);	/* Set general purpose IO for Chrontel communication */
       SiS_DDC2Delay(&SiS_Pr, 1000);
       temp1 = SiS_GetCH700x(&SiS_Pr, 0x25);
       /* See Chrontel TB31 for explanation */
       temp2 = SiS_GetCH700x(&SiS_Pr, 0x0e);
       if(((temp2 & 0x07) == 0x01) || (temp2 & 0x04)) {
	  SiS_SetCH700x(&SiS_Pr, 0x0b0e);
	  SiS_DDC2Delay(&SiS_Pr, 300);
       }
       temp2 = SiS_GetCH700x(&SiS_Pr, 0x25);
       if(temp2 != temp1) temp1 = temp2;

       if((temp1 >= 0x22) && (temp1 <= 0x50)) {
	   /* Read power status */
	   temp1 = SiS_GetCH700x(&SiS_Pr, 0x0e);
	   if((temp1 & 0x03) != 0x03) {
     	        /* Power all outputs */
		SiS_SetCH700x(&SiS_Pr, 0x0B0E);
		SiS_DDC2Delay(&SiS_Pr, 300);
	   }
	   /* Sense connected TV devices */
	   for(i = 0; i < 3; i++) {
	       SiS_SetCH700x(&SiS_Pr, 0x0110);
	       SiS_DDC2Delay(&SiS_Pr, 0x96);
	       SiS_SetCH700x(&SiS_Pr, 0x0010);
	       SiS_DDC2Delay(&SiS_Pr, 0x96);
	       temp1 = SiS_GetCH700x(&SiS_Pr, 0x10);
	       if(!(temp1 & 0x08))       test[i] = 0x02;
	       else if(!(temp1 & 0x02))  test[i] = 0x01;
	       else                      test[i] = 0;
	       SiS_DDC2Delay(&SiS_Pr, 0x96);
	   }

	   if(test[0] == test[1])      temp1 = test[0];
	   else if(test[0] == test[2]) temp1 = test[0];
	   else if(test[1] == test[2]) temp1 = test[1];
	   else {
	   	printk(KERN_INFO
			"sisfb: TV detection unreliable - test results varied\n");
		temp1 = test[2];
	   }
	   if(temp1 == 0x02) {
		printk(KERN_INFO "%s SVIDEO output\n", stdstr);
		ivideo.vbflags |= TV_SVIDEO;
		orSISIDXREG(SISCR, 0x32, 0x02);
		andSISIDXREG(SISCR, 0x32, ~0x05);
	   } else if (temp1 == 0x01) {
		printk(KERN_INFO "%s CVBS output\n", stdstr);
		ivideo.vbflags |= TV_AVIDEO;
		orSISIDXREG(SISCR, 0x32, 0x01);
		andSISIDXREG(SISCR, 0x32, ~0x06);
	   } else {
 		SiS_SetCH70xxANDOR(&SiS_Pr, 0x010E,0xF8);
		andSISIDXREG(SISCR, 0x32, ~0x07);
	   }
       } else if(temp1 == 0) {
	  SiS_SetCH70xxANDOR(&SiS_Pr, 0x010E,0xF8);
	  andSISIDXREG(SISCR, 0x32, ~0x07);
       }
       /* Set general purpose IO for Chrontel communication */
       SiS_SetChrontelGPIO(&SiS_Pr, 0x00);
#endif

   } else {

#ifdef CONFIG_FB_SIS_315
	SiS_Pr.SiS_IF_DEF_CH70xx = 2;		/* Chrontel 7019 */
        temp1 = SiS_GetCH701x(&SiS_Pr, 0x49);
	SiS_SetCH701x(&SiS_Pr, 0x2049);
	SiS_DDC2Delay(&SiS_Pr, 0x96);
	temp2 = SiS_GetCH701x(&SiS_Pr, 0x20);
	temp2 |= 0x01;
	SiS_SetCH701x(&SiS_Pr, (temp2 << 8) | 0x20);
	SiS_DDC2Delay(&SiS_Pr, 0x96);
	temp2 ^= 0x01;
	SiS_SetCH701x(&SiS_Pr, (temp2 << 8) | 0x20);
	SiS_DDC2Delay(&SiS_Pr, 0x96);
	temp2 = SiS_GetCH701x(&SiS_Pr, 0x20);
	SiS_SetCH701x(&SiS_Pr, (temp1 << 8) | 0x49);
        temp1 = 0;
	if(temp2 & 0x02) temp1 |= 0x01;
	if(temp2 & 0x10) temp1 |= 0x01;
	if(temp2 & 0x04) temp1 |= 0x02;
	if( (temp1 & 0x01) && (temp1 & 0x02) ) temp1 = 0x04;
	switch(temp1) {
	case 0x01:
	     printk(KERN_INFO "%s CVBS output\n", stdstr);
	     ivideo.vbflags |= TV_AVIDEO;
	     orSISIDXREG(SISCR, 0x32, 0x01);
	     andSISIDXREG(SISCR, 0x32, ~0x06);
             break;
	case 0x02:
	     printk(KERN_INFO "%s SVIDEO output\n", stdstr);
	     ivideo.vbflags |= TV_SVIDEO;
	     orSISIDXREG(SISCR, 0x32, 0x02);
	     andSISIDXREG(SISCR, 0x32, ~0x05);
             break;
	case 0x04:
	     printk(KERN_INFO "%s SCART output\n", stdstr);
	     orSISIDXREG(SISCR, 0x32, 0x04);
	     andSISIDXREG(SISCR, 0x32, ~0x03);
             break;
	default:
	     andSISIDXREG(SISCR, 0x32, ~0x07);
	}
#endif

   }
}


/* ------------------------ Heap routines -------------------------- */

static int __devinit
sisfb_heap_init(void)
{
	SIS_OH *poh;
	u8 temp=0;
#ifdef CONFIG_FB_SIS_315
	int            agp_enabled = 1;
	u32            agp_size;
	unsigned long  *cmdq_baseport = 0;
	unsigned long  *read_port = 0;
	unsigned long  *write_port = 0;
	SIS_CMDTYPE    cmd_type;
#ifndef AGPOFF
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	struct agp_kern_info  *agp_info;
	struct agp_memory     *agp;
#else
	agp_kern_info  *agp_info;
	agp_memory     *agp;
#endif
	unsigned long  agp_phys;
#endif
#endif
/*     The heap start is either set manually using the "mem" parameter, or
 *     defaults as follows:
 *     -) If more than 16MB videoRAM available, let our heap start at 12MB.
 *     -) If more than  8MB videoRAM available, let our heap start at  8MB.
 *     -) If 4MB or less is available, let it start at 4MB.
 *     This is for avoiding a clash with X driver which uses the beginning
 *     of the videoRAM. To limit size of X framebuffer, use Option MaxXFBMem
 *     in XF86Config-4.
 *     The heap start can also be specified by parameter "mem" when starting the sisfb
 *     driver. sisfb mem=1024 lets heap starts at 1MB, etc.
 *
 *     On the 315 and Xabre series, the default is a 1MB heap since DRI is not
 *     supported there.
 */
     if((!sisfb_mem) || (sisfb_mem > (ivideo.video_size/1024))) {
        if(ivideo.sisvga_engine == SIS_300_VGA) {
           if (ivideo.video_size > 0x1000000) {
	        ivideo.heapstart = 0xc00000;
	   } else if (ivideo.video_size > 0x800000) {
	        ivideo.heapstart = 0x800000;
	   } else {
		ivideo.heapstart = 0x400000;
	   }
	} else {
	   ivideo.heapstart = ivideo.video_size - 0x100000;
	}
     } else {
           ivideo.heapstart = sisfb_mem * 1024;
     }
     sisfb_heap_start = (unsigned long) (ivideo.video_vbase + ivideo.heapstart);
     printk(KERN_INFO "sisfb: Memory heap starting at %dK\n",
     					(int)(ivideo.heapstart / 1024));

     sisfb_heap_end = (unsigned long) ivideo.video_vbase + ivideo.video_size;
     sisfb_heap_size = sisfb_heap_end - sisfb_heap_start;

#ifdef CONFIG_FB_SIS_315
     if(ivideo.sisvga_engine == SIS_315_VGA) {
        /* Now initialize the 315/330 series' command queue mode.
	 * On 315, there are three queue modes available which
	 * are chosen by setting bits 7:5 in SR26:
	 * 1. MMIO queue mode (bit 5, 0x20). The hardware will keep
	 *    track of the queue, the FIFO, command parsing and so
	 *    on. This is the one comparable to the 300 series.
	 * 2. VRAM queue mode (bit 6, 0x40). In this case, one will
	 *    have to do queue management himself. Register 0x85c4 will
	 *    hold the location of the next free queue slot, 0x85c8
	 *    is the "queue read pointer" whose way of working is
	 *    unknown to me. Anyway, this mode would require a
	 *    translation of the MMIO commands to some kind of
	 *    accelerator assembly and writing these commands
	 *    to the memory location pointed to by 0x85c4.
	 *    We will not use this, as nobody knows how this
	 *    "assembly" works, and as it would require a complete
	 *    re-write of the accelerator code.
	 * 3. AGP queue mode (bit 7, 0x80). Works as 2., but keeps the
	 *    queue in AGP memory space.
	 *
	 * SR26 bit 4 is called "Bypass H/W queue".
	 * SR26 bit 1 is called "Enable Command Queue Auto Correction"
	 * SR26 bit 0 resets the queue
	 * Size of queue memory is encoded in bits 3:2 like this:
	 *    00  (0x00)  512K
	 *    01  (0x04)  1M
	 *    10  (0x08)  2M
	 *    11  (0x0C)  4M
	 * The queue location is to be written to 0x85C0.
	 *
         */
	cmdq_baseport = (unsigned long *)(ivideo.mmio_vbase + MMIO_QUEUE_PHYBASE);
	write_port    = (unsigned long *)(ivideo.mmio_vbase + MMIO_QUEUE_WRITEPORT);
	read_port     = (unsigned long *)(ivideo.mmio_vbase + MMIO_QUEUE_READPORT);

	DPRINTK("AGP base: 0x%p, read: 0x%p, write: 0x%p\n", cmdq_baseport, read_port, write_port);

	agp_size  = COMMAND_QUEUE_AREA_SIZE;

#ifndef AGPOFF
	if (sisfb_queuemode == AGP_CMD_QUEUE) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
		agp_info = vmalloc(sizeof(*agp_info));
		memset((void*)agp_info, 0x00, sizeof(*agp_info));
#else
		agp_info = vmalloc(sizeof(agp_kern_info));
		memset((void*)agp_info, 0x00, sizeof(agp_kern_info));
#endif
		agp_copy_info(agp_info);

		agp_backend_acquire();

		agp = agp_allocate_memory(COMMAND_QUEUE_AREA_SIZE/PAGE_SIZE,
					  AGP_NORMAL_MEMORY);
		if (agp == NULL) {
			DPRINTK("sisfb: Allocating AGP buffer failed.\n");
			agp_enabled = 0;
		} else {
			if (agp_bind_memory(agp, agp->pg_start) != 0) {
				DPRINTK("sisfb: AGP: Failed to bind memory\n");
				/* TODO: Free AGP memory here */
				agp_enabled = 0;
			} else {
				agp_enable(0);
			}
		}
	}
#else
	agp_enabled = 0;
#endif

	/* Now select the queue mode */

	if((agp_enabled) && (sisfb_queuemode == AGP_CMD_QUEUE)) {
		cmd_type = AGP_CMD_QUEUE;
		printk(KERN_INFO "sisfb: Using AGP queue mode\n");
        } else if (sisfb_queuemode == VM_CMD_QUEUE) {
		cmd_type = VM_CMD_QUEUE;
		printk(KERN_INFO "sisfb: Using VRAM queue mode\n");
	} else {
		printk(KERN_INFO "sisfb: Using MMIO queue mode\n");
		cmd_type = MMIO_CMD;
	}

	switch (agp_size) {
	   case 0x80000:
		temp = SIS_CMD_QUEUE_SIZE_512k;
		break;
	   case 0x100000:
		temp = SIS_CMD_QUEUE_SIZE_1M;
		break;
	   case 0x200000:
		temp = SIS_CMD_QUEUE_SIZE_2M;
		break;
	   case 0x400000:
		temp = SIS_CMD_QUEUE_SIZE_4M;
		break;
	}

	switch (cmd_type) {
	   case AGP_CMD_QUEUE:
#ifndef AGPOFF
		DPRINTK("sisfb: AGP buffer base = 0x%lx, offset = 0x%x, size = %dK\n",
			agp_info->aper_base, agp->physical, agp_size/1024);

		agp_phys = agp_info->aper_base + agp->physical;

		outSISIDXREG(SISCR,  IND_SIS_AGP_IO_PAD, 0);
		outSISIDXREG(SISCR,  IND_SIS_AGP_IO_PAD, SIS_AGP_2X);

                outSISIDXREG(SISSR, IND_SIS_CMDQUEUE_THRESHOLD, COMMAND_QUEUE_THRESHOLD);

		outSISIDXREG(SISSR, IND_SIS_CMDQUEUE_SET, SIS_CMD_QUEUE_RESET);

		*write_port = *read_port;

		temp |= SIS_AGP_CMDQUEUE_ENABLE;
		outSISIDXREG(SISSR, IND_SIS_CMDQUEUE_SET, temp);

		*cmdq_baseport = agp_phys;

		ivideo.caps |= AGP_CMD_QUEUE_CAP;
#endif
		break;

	   case VM_CMD_QUEUE:
		sisfb_heap_end -= COMMAND_QUEUE_AREA_SIZE;
		sisfb_heap_size -= COMMAND_QUEUE_AREA_SIZE;

		outSISIDXREG(SISSR, IND_SIS_CMDQUEUE_THRESHOLD, COMMAND_QUEUE_THRESHOLD);

		outSISIDXREG(SISSR, IND_SIS_CMDQUEUE_SET, SIS_CMD_QUEUE_RESET);

		*write_port = *read_port;

		temp |= SIS_VRAM_CMDQUEUE_ENABLE;
		outSISIDXREG(SISSR, IND_SIS_CMDQUEUE_SET, temp);

		*cmdq_baseport = ivideo.video_size - COMMAND_QUEUE_AREA_SIZE;

		ivideo.caps |= VM_CMD_QUEUE_CAP;

		break;

	   default:  /* MMIO */
	   	sisfb_heap_end -= COMMAND_QUEUE_AREA_SIZE;
		sisfb_heap_size -= COMMAND_QUEUE_AREA_SIZE;

		outSISIDXREG(SISSR, IND_SIS_CMDQUEUE_THRESHOLD, COMMAND_QUEUE_THRESHOLD);
		outSISIDXREG(SISSR, IND_SIS_CMDQUEUE_SET, SIS_CMD_QUEUE_RESET);

		*write_port = *read_port;

		/* Set Auto_Correction bit */
		temp |= (SIS_MMIO_CMD_ENABLE | SIS_CMD_AUTO_CORR);
		outSISIDXREG(SISSR, IND_SIS_CMDQUEUE_SET, temp);

		*cmdq_baseport = ivideo.video_size - COMMAND_QUEUE_AREA_SIZE;

		ivideo.caps |= MMIO_CMD_QUEUE_CAP;

		break;
	}
     } /* sisvga_engine = 315 */
#endif

#ifdef CONFIG_FB_SIS_300
     if(ivideo.sisvga_engine == SIS_300_VGA) {
  	    /* Now initialize TurboQueue. TB is always located at the very
	     * top of the video RAM. */
	    if (sisfb_heap_size >= TURBO_QUEUE_AREA_SIZE) {
		unsigned int  tqueue_pos;
		u8 tq_state;

		tqueue_pos = (ivideo.video_size -
		       TURBO_QUEUE_AREA_SIZE) / (64 * 1024);

		temp = (u8) (tqueue_pos & 0xff);

		inSISIDXREG(SISSR, IND_SIS_TURBOQUEUE_SET, tq_state);
		tq_state |= 0xf0;
		tq_state &= 0xfc;
		tq_state |= (u8) (tqueue_pos >> 8);
		outSISIDXREG(SISSR, IND_SIS_TURBOQUEUE_SET, tq_state);

		outSISIDXREG(SISSR, IND_SIS_TURBOQUEUE_ADR, temp);

		ivideo.caps |= TURBO_QUEUE_CAP;

		sisfb_heap_end -= TURBO_QUEUE_AREA_SIZE;
		sisfb_heap_size -= TURBO_QUEUE_AREA_SIZE;

	    }
     }
#endif
     /* Now reserve memory for the HWCursor. It is always located at the very
        top of the videoRAM, right below the TB memory area (if used). */
     if(sisfb_heap_size >= ivideo.hwcursor_size) {
		sisfb_heap_end -= ivideo.hwcursor_size;
		sisfb_heap_size -= ivideo.hwcursor_size;
		ivideo.hwcursor_vbase = sisfb_heap_end;

		ivideo.caps |= HW_CURSOR_CAP;
     }

     sisfb_heap.poha_chain = NULL;
     sisfb_heap.poh_freelist = NULL;

     poh = sisfb_poh_new_node();

     if(poh == NULL)  return 1;
	
     poh->poh_next = &sisfb_heap.oh_free;
     poh->poh_prev = &sisfb_heap.oh_free;
     poh->size = sisfb_heap_end - sisfb_heap_start + 1;
     poh->offset = sisfb_heap_start - (unsigned long) ivideo.video_vbase;

     DPRINTK("sisfb: Heap start:0x%p, end:0x%p, len=%dk\n",
		(char *) sisfb_heap_start, (char *) sisfb_heap_end,
		(unsigned int) poh->size / 1024);

     DPRINTK("sisfb: First Node offset:0x%x, size:%dk\n",
		(unsigned int) poh->offset, (unsigned int) poh->size / 1024);

     sisfb_heap.oh_free.poh_next = poh;
     sisfb_heap.oh_free.poh_prev = poh;
     sisfb_heap.oh_free.size = 0;
     sisfb_heap.max_freesize = poh->size;

     sisfb_heap.oh_used.poh_next = &sisfb_heap.oh_used;
     sisfb_heap.oh_used.poh_prev = &sisfb_heap.oh_used;
     sisfb_heap.oh_used.size = SENTINEL;

     return 0;
}

static SIS_OH *
sisfb_poh_new_node(void)
{
	int           i;
	unsigned long cOhs;
	SIS_OHALLOC   *poha;
	SIS_OH        *poh;

	if(sisfb_heap.poh_freelist == NULL) {
		poha = kmalloc(OH_ALLOC_SIZE, GFP_KERNEL);
		if(!poha) return NULL;

		poha->poha_next = sisfb_heap.poha_chain;
		sisfb_heap.poha_chain = poha;

		cOhs = (OH_ALLOC_SIZE - sizeof(SIS_OHALLOC)) / sizeof(SIS_OH) + 1;

		poh = &poha->aoh[0];
		for(i = cOhs - 1; i != 0; i--) {
			poh->poh_next = poh + 1;
			poh = poh + 1;
		}

		poh->poh_next = NULL;
		sisfb_heap.poh_freelist = &poha->aoh[0];
	}

	poh = sisfb_heap.poh_freelist;
	sisfb_heap.poh_freelist = poh->poh_next;

	return (poh);
}

static SIS_OH *
sisfb_poh_allocate(unsigned long size)
{
	SIS_OH *pohThis;
	SIS_OH *pohRoot;
	int     bAllocated = 0;

	if(size > sisfb_heap.max_freesize) {
		DPRINTK("sisfb: Can't allocate %dk size on offscreen\n",
			(unsigned int) size / 1024);
		return (NULL);
	}

	pohThis = sisfb_heap.oh_free.poh_next;

	while(pohThis != &sisfb_heap.oh_free) {
		if (size <= pohThis->size) {
			bAllocated = 1;
			break;
		}
		pohThis = pohThis->poh_next;
	}

	if(!bAllocated) {
		DPRINTK("sisfb: Can't allocate %dk size on offscreen\n",
			(unsigned int) size / 1024);
		return (NULL);
	}

	if(size == pohThis->size) {
		pohRoot = pohThis;
		sisfb_delete_node(pohThis);
	} else {
		pohRoot = sisfb_poh_new_node();

		if(pohRoot == NULL) {
			return (NULL);
		}

		pohRoot->offset = pohThis->offset;
		pohRoot->size = size;

		pohThis->offset += size;
		pohThis->size -= size;
	}

	sisfb_heap.max_freesize -= size;

	pohThis = &sisfb_heap.oh_used;
	sisfb_insert_node(pohThis, pohRoot);

	return (pohRoot);
}

static void
sisfb_delete_node(SIS_OH *poh)
{
	SIS_OH *poh_prev;
	SIS_OH *poh_next;

	poh_prev = poh->poh_prev;
	poh_next = poh->poh_next;

	poh_prev->poh_next = poh_next;
	poh_next->poh_prev = poh_prev;
}

static void
sisfb_insert_node(SIS_OH *pohList, SIS_OH *poh)
{
	SIS_OH *pohTemp;

	pohTemp = pohList->poh_next;

	pohList->poh_next = poh;
	pohTemp->poh_prev = poh;

	poh->poh_prev = pohList;
	poh->poh_next = pohTemp;
}

static SIS_OH *
sisfb_poh_free(unsigned long base)
{
	SIS_OH *pohThis;
	SIS_OH *poh_freed;
	SIS_OH *poh_prev;
	SIS_OH *poh_next;
	unsigned long ulUpper;
	unsigned long ulLower;
	int foundNode = 0;

	poh_freed = sisfb_heap.oh_used.poh_next;

	while(poh_freed != &sisfb_heap.oh_used) {
		if(poh_freed->offset == base) {
			foundNode = 1;
			break;
		}

		poh_freed = poh_freed->poh_next;
	}

	if(!foundNode)  return(NULL);

	sisfb_heap.max_freesize += poh_freed->size;

	poh_prev = poh_next = NULL;
	ulUpper = poh_freed->offset + poh_freed->size;
	ulLower = poh_freed->offset;

	pohThis = sisfb_heap.oh_free.poh_next;

	while(pohThis != &sisfb_heap.oh_free) {
		if(pohThis->offset == ulUpper) {
			poh_next = pohThis;
		}
			else if((pohThis->offset + pohThis->size) == ulLower) {
			poh_prev = pohThis;
		}
		pohThis = pohThis->poh_next;
	}

	sisfb_delete_node(poh_freed);

	if(poh_prev && poh_next) {
		poh_prev->size += (poh_freed->size + poh_next->size);
		sisfb_delete_node(poh_next);
		sisfb_free_node(poh_freed);
		sisfb_free_node(poh_next);
		return(poh_prev);
	}

	if(poh_prev) {
		poh_prev->size += poh_freed->size;
		sisfb_free_node(poh_freed);
		return(poh_prev);
	}

	if(poh_next) {
		poh_next->size += poh_freed->size;
		poh_next->offset = poh_freed->offset;
		sisfb_free_node(poh_freed);
		return(poh_next);
	}

	sisfb_insert_node(&sisfb_heap.oh_free, poh_freed);

	return(poh_freed);
}

static void
sisfb_free_node(SIS_OH *poh)
{
	if(poh == NULL) return;

	poh->poh_next = sisfb_heap.poh_freelist;
	sisfb_heap.poh_freelist = poh;
}

void
sis_malloc(struct sis_memreq *req)
{
	SIS_OH *poh;

	poh = sisfb_poh_allocate(req->size);

	if(poh == NULL) {
		req->offset = 0;
		req->size = 0;
		DPRINTK("sisfb: Video RAM allocation failed\n");
	} else {
		DPRINTK("sisfb: Video RAM allocation succeeded: 0x%p\n",
			(char *) (poh->offset + (unsigned long) ivideo.video_vbase));

		req->offset = poh->offset;
		req->size = poh->size;
	}
}

void
sis_free(unsigned long base)
{
	SIS_OH *poh;

	poh = sisfb_poh_free(base);

	if(poh == NULL) {
		DPRINTK("sisfb: sisfb_poh_free() failed at base 0x%x\n",
			(unsigned int) base);
	}
}

/* --------------------- SetMode routines ------------------------- */

static void
sisfb_pre_setmode(void)
{
	u8 cr30 = 0, cr31 = 0, cr33 = 0, cr35 = 0;
	
	ivideo.currentvbflags &= (VB_VIDEOBRIDGE | VB_DISPTYPE_DISP2);

	inSISIDXREG(SISCR, 0x31, cr31);
	cr31 &= ~0x60;
	cr31 |= 0x04;
	
	cr33 = ivideo.rate_idx & 0x0F;

	SiS_SetEnableDstn(&SiS_Pr, FALSE);
	SiS_SetEnableFstn(&SiS_Pr, FALSE);

	switch (ivideo.currentvbflags & VB_DISPTYPE_DISP2) {
	   case CRT2_TV:
		ivideo.disp_state = DISPTYPE_TV;
		if (ivideo.vbflags & TV_SVIDEO) {
			cr30 = (SIS_VB_OUTPUT_SVIDEO | SIS_SIMULTANEOUS_VIEW_ENABLE);
			ivideo.currentvbflags |= TV_SVIDEO;
			ivideo.TV_plug = TVPLUG_SVIDEO;
		} else if (ivideo.vbflags & TV_AVIDEO) {
			cr30 = (SIS_VB_OUTPUT_COMPOSITE | SIS_SIMULTANEOUS_VIEW_ENABLE);
			ivideo.currentvbflags |= TV_AVIDEO;
			ivideo.TV_plug = TVPLUG_COMPOSITE;
		} else if (ivideo.vbflags & TV_SCART) {
			cr30 = (SIS_VB_OUTPUT_SCART | SIS_SIMULTANEOUS_VIEW_ENABLE);
			ivideo.currentvbflags |= TV_SCART;
			ivideo.TV_plug = TVPLUG_SCART;
		}
		cr31 |= SIS_DRIVER_MODE;

		if(!(ivideo.vbflags & TV_HIVISION)) {
	        	if (ivideo.vbflags & TV_PAL) {
		 		cr31 |= 0x01;
				cr35 |= 0x01;
				ivideo.currentvbflags |= TV_PAL;
				ivideo.TV_type = TVMODE_PAL;
                	} else {
		       		cr31 &= ~0x01;
				cr35 &= ~0x01;
				ivideo.currentvbflags |= TV_NTSC;
				ivideo.TV_type = TVMODE_NTSC;
			}
		}
		break;
	   case CRT2_LCD:
		ivideo.disp_state = DISPTYPE_LCD;
		cr30  = (SIS_VB_OUTPUT_LCD | SIS_SIMULTANEOUS_VIEW_ENABLE);
		cr31 |= SIS_DRIVER_MODE;
		SiS_SetEnableDstn(&SiS_Pr, sisfb_dstn);
	        SiS_SetEnableFstn(&SiS_Pr, sisfb_fstn);
		break;		
	   case CRT2_VGA:
		ivideo.disp_state = DISPTYPE_CRT2;
		cr30 = (SIS_VB_OUTPUT_CRT2 | SIS_SIMULTANEOUS_VIEW_ENABLE);
		cr31 |= SIS_DRIVER_MODE;
		if(sisfb_nocrt2rate) {
			cr33 |= (sisbios_mode[sisfb_mode_idx].rate_idx << 4);
		} else {
			cr33 |= ((ivideo.rate_idx & 0x0F) << 4);
		}
		break;
	   default:	/* disable CRT2 */
		cr30 = 0x00;
		cr31 |= (SIS_DRIVER_MODE | SIS_VB_OUTPUT_DISABLE);
	}

	if(ivideo.chip >= SIS_661) {
	   cr31 &= ~0x01;
	   /* Leave overscan bit alone */
	   setSISIDXREG(SISCR, 0x35, ~0x10, cr35);
	}
	outSISIDXREG(SISCR, IND_SIS_SCRATCH_REG_CR30, cr30);
	outSISIDXREG(SISCR, IND_SIS_SCRATCH_REG_CR31, cr31);
	outSISIDXREG(SISCR, IND_SIS_SCRATCH_REG_CR33, cr33);

#ifdef CONFIG_FB_SIS_315
        if(ivideo.sisvga_engine == SIS_315_VGA) {
	   /* Clear LCDA and PAL-N/M bits */
	   andSISIDXREG(SISCR,0x38,~0x03);
	   if(ivideo.chip < SIS_661) {
	      andSISIDXREG(SISCR,0x38,~0xc0);
	   }
	}
#endif

	if(ivideo.accel) sisfb_syncaccel();

	SiS_Pr.SiS_UseOEM = sisfb_useoem;
}

static void
sisfb_post_setmode(void)
{
	u8 reg;
	BOOLEAN crt1isoff = FALSE;
	BOOLEAN doit = TRUE;
#ifdef CONFIG_FB_SIS_315
	u8 reg1;
#endif

	/* Now we actually HAVE changed the display mode */
        ivideo.modechanged = 1;

	/* We can't switch off CRT1 if bridge is in slave mode */
	if(ivideo.vbflags & VB_VIDEOBRIDGE) {
		inSISIDXREG(SISPART1, 0x00, reg);
#ifdef CONFIG_FB_SIS_300
		if(ivideo.sisvga_engine == SIS_300_VGA) {
			if((reg & 0xa0) == 0x20) {
				doit = FALSE;
			}
		}
#endif
#ifdef CONFIG_FB_SIS_315
		if(ivideo.sisvga_engine == SIS_315_VGA) {
			if((reg & 0x50) == 0x10) {
				doit = FALSE;
			}
		}
#endif
	} else sisfb_crt1off = 0;

	if(ivideo.sisvga_engine == SIS_300_VGA) {

#ifdef CONFIG_FB_SIS_300
	   if((sisfb_crt1off) && (doit)) {
	        crt1isoff = TRUE;
		reg = 0x00;
	   } else {
	        crt1isoff = FALSE;
		reg = 0x80;
	   }
	   setSISIDXREG(SISCR, 0x17, 0x7f, reg);
#endif

	} else {

#ifdef CONFIG_FB_SIS_315
	   if((sisfb_crt1off) && (doit)) {
	        crt1isoff = TRUE;
		reg  = 0x40;
		reg1 = 0xc0;
	   } else {
	        crt1isoff = FALSE;
		reg  = 0x00;
		reg1 = 0x00;

	   }
	   setSISIDXREG(SISCR, SiS_Pr.SiS_MyCR63, ~0x40, reg);
	   setSISIDXREG(SISSR, 0x1f, ~0xc0, reg1);
#endif

	}

	if(crt1isoff) {
	   ivideo.currentvbflags &= ~VB_DISPTYPE_CRT1;
	   ivideo.currentvbflags |= VB_SINGLE_MODE;
	   ivideo.disp_state |= DISPMODE_SINGLE;
	} else {
	   ivideo.currentvbflags |= VB_DISPTYPE_CRT1;
	   ivideo.disp_state |= DISPTYPE_CRT1;
	   if(ivideo.currentvbflags & VB_DISPTYPE_CRT2) {
	  	ivideo.currentvbflags |= VB_MIRROR_MODE;
		ivideo.disp_state |= DISPMODE_MIRROR;
	   } else {
	 	ivideo.currentvbflags |= VB_SINGLE_MODE;
		ivideo.disp_state |= DISPMODE_SINGLE;
	   }
	}

        andSISIDXREG(SISSR, IND_SIS_RAMDAC_CONTROL, ~0x04);

	if((ivideo.currentvbflags & CRT2_TV) && (ivideo.vbflags & VB_301)) {  /* Set filter for SiS301 */

		switch (ivideo.video_width) {
		   case 320:
			filter_tb = (ivideo.vbflags & TV_NTSC) ? 4 : 12;
			break;
		   case 640:
			filter_tb = (ivideo.vbflags & TV_NTSC) ? 5 : 13;
			break;
		   case 720:
			filter_tb = (ivideo.vbflags & TV_NTSC) ? 6 : 14;
			break;
		   case 400:
		   case 800:
			filter_tb = (ivideo.vbflags & TV_NTSC) ? 7 : 15;
			break;
		   default:
			filter = -1;
			break;
		}

		orSISIDXREG(SISPART1, ivideo.CRT2_write_enable, 0x01);

		if(ivideo.vbflags & TV_NTSC) {

		        andSISIDXREG(SISPART2, 0x3a, 0x1f);

			if (ivideo.vbflags & TV_SVIDEO) {

			        andSISIDXREG(SISPART2, 0x30, 0xdf);

			} else if (ivideo.vbflags & TV_AVIDEO) {

			        orSISIDXREG(SISPART2, 0x30, 0x20);

				switch (ivideo.video_width) {
				case 640:
				        outSISIDXREG(SISPART2, 0x35, 0xEB);
					outSISIDXREG(SISPART2, 0x36, 0x04);
					outSISIDXREG(SISPART2, 0x37, 0x25);
					outSISIDXREG(SISPART2, 0x38, 0x18);
					break;
				case 720:
					outSISIDXREG(SISPART2, 0x35, 0xEE);
					outSISIDXREG(SISPART2, 0x36, 0x0C);
					outSISIDXREG(SISPART2, 0x37, 0x22);
					outSISIDXREG(SISPART2, 0x38, 0x08);
					break;
				case 400:
				case 800:
					outSISIDXREG(SISPART2, 0x35, 0xEB);
					outSISIDXREG(SISPART2, 0x36, 0x15);
					outSISIDXREG(SISPART2, 0x37, 0x25);
					outSISIDXREG(SISPART2, 0x38, 0xF6);
					break;
				}
			}

		} else if(ivideo.vbflags & TV_PAL) {

			andSISIDXREG(SISPART2, 0x3A, 0x1F);

			if (ivideo.vbflags & TV_SVIDEO) {

			        andSISIDXREG(SISPART2, 0x30, 0xDF);

			} else if (ivideo.vbflags & TV_AVIDEO) {

			        orSISIDXREG(SISPART2, 0x30, 0x20);

				switch (ivideo.video_width) {
				case 640:
					outSISIDXREG(SISPART2, 0x35, 0xF1);
					outSISIDXREG(SISPART2, 0x36, 0xF7);
					outSISIDXREG(SISPART2, 0x37, 0x1F);
					outSISIDXREG(SISPART2, 0x38, 0x32);
					break;
				case 720:
					outSISIDXREG(SISPART2, 0x35, 0xF3);
					outSISIDXREG(SISPART2, 0x36, 0x00);
					outSISIDXREG(SISPART2, 0x37, 0x1D);
					outSISIDXREG(SISPART2, 0x38, 0x20);
					break;
				case 400:
				case 800:
					outSISIDXREG(SISPART2, 0x35, 0xFC);
					outSISIDXREG(SISPART2, 0x36, 0xFB);
					outSISIDXREG(SISPART2, 0x37, 0x14);
					outSISIDXREG(SISPART2, 0x38, 0x2A);
					break;
				}
			}
		}

		if((filter >= 0) && (filter <= 7)) {
			outSISIDXREG(SISPART2, 0x35, (sis_TV_filter[filter_tb].filter[filter][0]));
			outSISIDXREG(SISPART2, 0x36, (sis_TV_filter[filter_tb].filter[filter][1]));
			outSISIDXREG(SISPART2, 0x37, (sis_TV_filter[filter_tb].filter[filter][2]));
			outSISIDXREG(SISPART2, 0x38, (sis_TV_filter[filter_tb].filter[filter][3]));
		}
	  
	}
}

#ifndef MODULE
int __init sisfb_setup(char *options)
{
	char *this_opt;
	
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	sisfb_fontname[0] = '\0';
#endif

	ivideo.refresh_rate = 0;
	SiS_Pr.SiS_CustomT = CUT_NONE;
	SiS_Pr.UsePanelScaler = -1;
	SiS_Pr.CenterScreen = -1;
	SiS_Pr.LVDSHL = -1;
#if !defined(__i386__) && !defined(__x86_64__)
       	sisfb_resetcard = 0;
	sisfb_videoram = 0;
#endif

        printk(KERN_DEBUG "sisfb: Options %s\n", options);

	if(!options || !(*options))
		return 0;

	while((this_opt = strsep(&options, ",")) != NULL) {

		if(!(*this_opt)) continue;

		if(!strnicmp(this_opt, "mode:", 5)) {
			sisfb_search_mode(this_opt + 5, FALSE);
		} else if(!strnicmp(this_opt, "vesa:", 5)) {
			sisfb_search_vesamode(simple_strtoul(this_opt + 5, NULL, 0), FALSE);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
		} else if(!strnicmp(this_opt, "inverse", 7)) {
			sisfb_inverse = 1;
			/* fb_invert_cmaps(); */
		} else if(!strnicmp(this_opt, "font:", 5)) {
		        if(strlen(this_opt + 5) < 40) {
			   strncpy(sisfb_fontname, this_opt + 5, sizeof(sisfb_fontname) - 1);
			   sisfb_fontname[sizeof(sisfb_fontname) - 1] = '\0';
			}
#endif
		} else if(!strnicmp(this_opt, "vrate:", 6)) {
			ivideo.refresh_rate = simple_strtoul(this_opt + 6, NULL, 0);
			sisfb_parm_rate = ivideo.refresh_rate;
		} else if(!strnicmp(this_opt, "rate:", 5)) {
			ivideo.refresh_rate = simple_strtoul(this_opt + 5, NULL, 0);
			sisfb_parm_rate = ivideo.refresh_rate;
		} else if(!strnicmp(this_opt, "off", 3)) {
			sisfb_off = 1;
		} else if(!strnicmp(this_opt, "crt1off", 7)) {
			sisfb_crt1off = 1;
		} else if(!strnicmp(this_opt, "filter:", 7)) {
			filter = (int)simple_strtoul(this_opt + 7, NULL, 0);
		} else if(!strnicmp(this_opt, "forcecrt2type:", 14)) {
			sisfb_search_crt2type(this_opt + 14);
		} else if(!strnicmp(this_opt, "forcecrt1:", 10)) {
			sisfb_forcecrt1 = (int)simple_strtoul(this_opt + 10, NULL, 0);
                } else if(!strnicmp(this_opt, "tvmode:",7)) {
		        sisfb_search_tvstd(this_opt + 7);
                } else if(!strnicmp(this_opt, "tvstandard:",11)) {
			sisfb_search_tvstd(this_opt + 7);
                } else if(!strnicmp(this_opt, "mem:",4)) {
		        sisfb_mem = simple_strtoul(this_opt + 4, NULL, 0);
		} else if(!strnicmp(this_opt, "queuemode:", 10)) {
			sisfb_search_queuemode(this_opt + 10);
		} else if(!strnicmp(this_opt, "pdc:", 4)) {
		        sisfb_pdc = simple_strtoul(this_opt + 4, NULL, 0);
		} else if(!strnicmp(this_opt, "pdc1:", 5)) {
		        sisfb_pdca = simple_strtoul(this_opt + 5, NULL, 0);
		} else if(!strnicmp(this_opt, "noaccel", 7)) {
			sisfb_accel = 0;
		} else if(!strnicmp(this_opt, "noypan", 6)) {
		        sisfb_ypan = 0;
		} else if(!strnicmp(this_opt, "nomax", 5)) {
		        sisfb_max = 0;
		} else if(!strnicmp(this_opt, "userom:", 7)) {
			sisfb_userom = (int)simple_strtoul(this_opt + 7, NULL, 0);
		} else if(!strnicmp(this_opt, "useoem:", 7)) {
			sisfb_useoem = (int)simple_strtoul(this_opt + 7, NULL, 0);
		} else if(!strnicmp(this_opt, "nocrt2rate", 10)) {
			sisfb_nocrt2rate = 1;
	 	} else if(!strnicmp(this_opt, "scalelcd:", 9)) {
		        unsigned long temp = 2;
		        temp = simple_strtoul(this_opt + 9, NULL, 0);
		        if((temp == 0) || (temp == 1)) {
			   SiS_Pr.UsePanelScaler = temp ^ 1;
		        }
		} else if(!strnicmp(this_opt, "specialtiming:", 14)) {
			sisfb_search_specialtiming(this_opt + 14);
		} else if(!strnicmp(this_opt, "lvdshl:", 7)) {
		        unsigned long temp = 4;
		        temp = simple_strtoul(this_opt + 7, NULL, 0);
		        if((temp >= 0) && (temp <= 3)) {
			   SiS_Pr.LVDSHL = temp;
		        }
		} else if(this_opt[0] >= '0' && this_opt[0] <= '9') {
			sisfb_search_mode(this_opt, TRUE);
#if !defined(__i386__) && !defined(__x86_64__)
	        } else if(!strnicmp(this_opt, "resetcard", 9)) {
		  	sisfb_resetcard = 1;
	        } else if(!strnicmp(this_opt, "videoram:", 9)) {
		  	sisfb_videoram = simple_strtoul(this_opt + 9, NULL, 0);
#endif
		} else {
			printk(KERN_INFO "sisfb: Invalid option %s\n", this_opt);
		}

		/* Acceleration only with MMIO mode */
		if((sisfb_queuemode != -1) && (sisfb_queuemode != MMIO_CMD)) {
			sisfb_accel = 0;
		}

	}
	return 0;
}
#endif

static char * __devinit sis_find_rom(struct pci_dev *pdev)
{
#if defined(__i386__) || defined(__x86_64__)
        u32  segstart;
        unsigned char *rom_base, *rom;
        int  romptr;
	unsigned short pciid;

        for(segstart=0x000c0000; segstart<0x000f0000; segstart+=0x00001000) {

                rom_base = (unsigned char *)ioremap(segstart, 0x10000);
		if(!rom_base) continue;

		if((readb(rom_base) != 0x55) || (readb(rom_base + 1) != 0xaa)) {
		   iounmap(rom_base);
                   continue;
		}

		romptr = (unsigned short)(readb(rom_base + 0x18) | (readb(rom_base + 0x19) << 8));
		if(romptr > (0x10000 - 8)) {
		   iounmap(rom_base);
		   continue;
		}

		rom = rom_base + romptr;

		if((readb(rom)     != 'P') || (readb(rom + 1) != 'C') ||
		   (readb(rom + 2) != 'I') || (readb(rom + 3) != 'R')) {
		   iounmap(rom_base);
		   continue;
		}

		pciid = readb(rom + 4) | (readb(rom + 5) << 8);
		if(pciid != 0x1039) {
		   iounmap(rom_base);
		   continue;
		}

		pciid = readb(rom + 6) | (readb(rom + 7) << 8);
		if(pciid == ivideo.chip_id) return rom_base;

		iounmap(rom_base);
        }
#else
	unsigned char *rom_base, *rom, *myrombase = NULL;
        int  romptr;
	unsigned short pciid;
	u32 backup;

	pci_read_config_dword(pdev, PCI_ROM_ADDRESS, &backup);
	pci_write_config_dword(pdev, PCI_ROM_ADDRESS,
			(ivideo.video_base & PCI_ROM_ADDRESS_MASK) | PCI_ROM_ADDRESS_ENABLE);

	rom_base = ioremap(ivideo.video_base, 65536);
	if(rom_base) {
	   if((readb(rom_base) == 0x55) && (readb(rom_base + 1) == 0xaa)) {
	      romptr = (unsigned short)(readb(rom_base + 0x18) | (readb(rom_base + 0x19) << 8));
	      if(romptr <= (0x10000 - 8)) {
	         rom = rom_base + romptr;
		 if((readb(rom)     == 'P') && (readb(rom + 1) == 'C') &&
		    (readb(rom + 2) == 'I') && (readb(rom + 3) == 'R')) {
		    pciid = readb(rom + 4) | (readb(rom + 5) << 8);
		    if(pciid == 0x1039) {
		       pciid = readb(rom + 6) | (readb(rom + 7) << 8);
		       if(pciid == ivideo.chip_id) {
		          if((myrombase = vmalloc(65536))) {
			     memcpy_fromio(myrombase, rom_base, 65536);
			  }
		       }
		    }
		 }
	      }
	   }
	   iounmap(rom_base);
	}
        pci_write_config_dword(pdev, PCI_ROM_ADDRESS, backup);
	if(myrombase) return myrombase;
#endif
        return NULL;
}

#ifdef SIS300
static int __devinit
sisfb_chkbuswidth300(ULONG FBAddress)
{
	int i, j;
	USHORT temp;
	UCHAR reg;

	andSISIDXREG(SISSR,0x15,0xFB);
	orSISIDXREG(SISSR,0x15,0x04);
   	outSISIDXREG(SISSR,0x13,0x00);
   	outSISIDXREG(SISSR,0x14,0xBF);

	for(i=0; i<2; i++) {
	   temp = 0x1234;
	   for(j=0; j<4; j++) {
	      writew(temp, FBAddress);
	      if(readw(FBAddress) == temp) break;
	      orSISIDXREG(SISSR,0x3c,0x01);
	      inSISIDXREG(SISSR,0x05,reg);
	      inSISIDXREG(SISSR,0x05,reg);
	      andSISIDXREG(SISSR,0x3c,0xfe);
	      inSISIDXREG(SISSR,0x05,reg);
	      inSISIDXREG(SISSR,0x05,reg);
	      temp++;
	   }
	}

	writel(0x01234567L, FBAddress);
	writel(0x456789ABL, (FBAddress+4));
	writel(0x89ABCDEFL, (FBAddress+8));
	writel(0xCDEF0123L, (FBAddress+12));
	inSISIDXREG(SISSR,0x3b,reg);
	if(reg & 0x01) {
	   if(readl((FBAddress+12)) == 0xCDEF0123L) return(4);  /* Channel A 128bit */
	}
	if(readl((FBAddress+4)) == 0x456789ABL)     return(2);  /* Channel B 64bit */
	return(1);						/* 32bit */
}

static void __devinit
sisfb_setramsize300(void)
{
  	ULONG 	FBAddr = (ULONG)sishw_ext.pjVideoMemoryAddress, Addr;
	USHORT 	SR13, SR14=0, buswidth, Done, data, TotalCapacity, PhysicalAdrOtherPage=0;
	int     PseudoRankCapacity, PseudoTotalCapacity, PseudoAdrPinCount;
   	int     RankCapacity, AdrPinCount, BankNumHigh, BankNumMid, MB2Bank;
   	int     PageCapacity, PhysicalAdrHigh, PhysicalAdrHalfPage, i, j, k;
	const 	USHORT SiS_DRAMType[17][5] = {
			{0x0C,0x0A,0x02,0x40,0x39},
			{0x0D,0x0A,0x01,0x40,0x48},
			{0x0C,0x09,0x02,0x20,0x35},
			{0x0D,0x09,0x01,0x20,0x44},
			{0x0C,0x08,0x02,0x10,0x31},
			{0x0D,0x08,0x01,0x10,0x40},
			{0x0C,0x0A,0x01,0x20,0x34},
			{0x0C,0x09,0x01,0x08,0x32},
			{0x0B,0x08,0x02,0x08,0x21},
			{0x0C,0x08,0x01,0x08,0x30},
			{0x0A,0x08,0x02,0x04,0x11},
			{0x0B,0x0A,0x01,0x10,0x28},
			{0x09,0x08,0x02,0x02,0x01},
			{0x0B,0x09,0x01,0x08,0x24},
			{0x0B,0x08,0x01,0x04,0x20},
			{0x0A,0x08,0x01,0x02,0x10},
			{0x09,0x08,0x01,0x01,0x00}
		};

        buswidth = sisfb_chkbuswidth300(FBAddr);

   	MB2Bank = 16;
   	Done = 0;
   	for(i = 6; i >= 0; i--) {
      	   if(Done) break;
      	   PseudoRankCapacity = 1 << i;
      	   for(j = 4; j >= 1; j--) {
              if(Done) break;
              PseudoTotalCapacity = PseudoRankCapacity * j;
              PseudoAdrPinCount = 15 - j;
              if(PseudoTotalCapacity <= 64) {
                 for(k = 0; k <= 16; k++) {
                    if(Done) break;
                    RankCapacity = buswidth * SiS_DRAMType[k][3];
                    AdrPinCount = SiS_DRAMType[k][2] + SiS_DRAMType[k][0];
                    if(RankCapacity == PseudoRankCapacity)
                       if(AdrPinCount <= PseudoAdrPinCount) {
                          if(j == 3) {             /* Rank No */
                             BankNumHigh = RankCapacity * MB2Bank * 3 - 1;
                             BankNumMid  = RankCapacity * MB2Bank * 1 - 1;
                          } else {
                             BankNumHigh = RankCapacity * MB2Bank * j - 1;
                             BankNumMid  = RankCapacity * MB2Bank * j / 2 - 1;
                          }
                          PageCapacity = (1 << SiS_DRAMType[k][1]) * buswidth * 4;
                          PhysicalAdrHigh = BankNumHigh;
                          PhysicalAdrHalfPage = (PageCapacity / 2 + PhysicalAdrHigh) % PageCapacity;
                          PhysicalAdrOtherPage = PageCapacity * SiS_DRAMType[k][2] + PhysicalAdrHigh;
                          /* Write data */
                          andSISIDXREG(SISSR,0x15,0xFB); /* Test */
                          orSISIDXREG(SISSR,0x15,0x04);  /* Test */
                          TotalCapacity = SiS_DRAMType[k][3] * buswidth;
                          SR13 = SiS_DRAMType[k][4];
                          if(buswidth == 4) SR14 = (TotalCapacity - 1) | 0x80;
                          if(buswidth == 2) SR14 = (TotalCapacity - 1) | 0x40;
                          if(buswidth == 1) SR14 = (TotalCapacity - 1) | 0x00;
                          outSISIDXREG(SISSR,0x13,SR13);
                          outSISIDXREG(SISSR,0x14,SR14);
                          Addr = FBAddr + BankNumHigh * 64 * 1024 + PhysicalAdrHigh;
                          /* *((USHORT *)(Addr)) = (USHORT)PhysicalAdrHigh; */
			  writew(((USHORT)PhysicalAdrHigh), Addr);
                          Addr = FBAddr + BankNumMid * 64 * 1024 + PhysicalAdrHigh;
                          /* *((USHORT *)(Addr)) = (USHORT)BankNumMid; */
			  writew(((USHORT)BankNumMid), Addr);
                          Addr = FBAddr + BankNumHigh * 64 * 1024 + PhysicalAdrHalfPage;
                          /* *((USHORT *)(Addr)) = (USHORT)PhysicalAdrHalfPage; */
			  writew(((USHORT)PhysicalAdrHalfPage), Addr);
                          Addr = FBAddr + BankNumHigh * 64 * 1024 + PhysicalAdrOtherPage;
                          /* *((USHORT *)(Addr)) = PhysicalAdrOtherPage; */
			  writew(((USHORT)PhysicalAdrOtherPage), Addr);
                          /* Read data */
                          Addr = FBAddr + BankNumHigh * 64 * 1024 + PhysicalAdrHigh;
                          data = readw(Addr); /* *((USHORT *)(Addr)); */
                          if(data == PhysicalAdrHigh) Done = 1;
                       }  /* if */
                 }  /* for k */
              }  /* if */
      	   }  /* for j */
   	}  /* for i */
}

static void __devinit sisfb_post_sis300(void)
{
	u8  reg, v1, v2, v3, v4, v5, v6, v7, v8;
	u16 index, rindex, memtype = 0;

	outSISIDXREG(SISSR,0x05,SIS_PASSWORD);

	if(sishw_ext.UseROM) {
	   if(sishw_ext.pjVirtualRomBase[0x52] & 0x80) {
	      memtype = sishw_ext.pjVirtualRomBase[0x52];
 	   } else {
	      inSISIDXREG(SISSR,0x3a,memtype);
	   }
	   memtype &= 0x07;
	}

	if(ivideo.revision_id <= 0x13) {
	   v1 = 0x44; v2 = 0x42; v3 = 0x80;
	   v4 = 0x44; v5 = 0x42; v6 = 0x80;
	} else {
	   v1 = 0x68; v2 = 0x43; v3 = 0x80;  /* Assume 125Mhz MCLK */
	   v4 = 0x68; v5 = 0x43; v6 = 0x80;  /* Assume 125Mhz ECLK */
	   if(sishw_ext.UseROM) {
	      index = memtype * 5;
	      rindex = index + 0x54;
	      v1 = sishw_ext.pjVirtualRomBase[rindex++];
	      v2 = sishw_ext.pjVirtualRomBase[rindex++];
	      v3 = sishw_ext.pjVirtualRomBase[rindex++];
	      rindex = index + 0x7c;
	      v4 = sishw_ext.pjVirtualRomBase[rindex++];
	      v5 = sishw_ext.pjVirtualRomBase[rindex++];
	      v6 = sishw_ext.pjVirtualRomBase[rindex++];
	   }
	}
	outSISIDXREG(SISSR,0x28,v1);
	outSISIDXREG(SISSR,0x29,v2);
	outSISIDXREG(SISSR,0x2a,v3);
	outSISIDXREG(SISSR,0x2e,v4);
	outSISIDXREG(SISSR,0x2f,v5);
	outSISIDXREG(SISSR,0x30,v6);
	v1 = 0x10;
	if(sishw_ext.UseROM) v1 = sishw_ext.pjVirtualRomBase[0xa4];
	outSISIDXREG(SISSR,0x07,v1);       /* DAC speed */
	outSISIDXREG(SISSR,0x11,0x0f);     /* DDC, power save */
	v1 = 0x01; v2 = 0x43; v3 = 0x1e; v4 = 0x2a;
	v5 = 0x06; v6 = 0x00; v7 = 0x00; v8 = 0x00;
	if(sishw_ext.UseROM) {
	   memtype += 0xa5;
	   v1 = sishw_ext.pjVirtualRomBase[memtype];
	   v2 = sishw_ext.pjVirtualRomBase[memtype + 8];
	   v3 = sishw_ext.pjVirtualRomBase[memtype + 16];
	   v4 = sishw_ext.pjVirtualRomBase[memtype + 24];
	   v5 = sishw_ext.pjVirtualRomBase[memtype + 32];
	   v6 = sishw_ext.pjVirtualRomBase[memtype + 40];
	   v7 = sishw_ext.pjVirtualRomBase[memtype + 48];
	   v8 = sishw_ext.pjVirtualRomBase[memtype + 56];
	}
	if(ivideo.revision_id >= 0x80) v3 &= 0xfd;
	outSISIDXREG(SISSR,0x15,v1);       /* Ram type (assuming 0, BIOS 0xa5 step 8) */
	outSISIDXREG(SISSR,0x16,v2);
	outSISIDXREG(SISSR,0x17,v3);
	outSISIDXREG(SISSR,0x18,v4);
	outSISIDXREG(SISSR,0x19,v5);
	outSISIDXREG(SISSR,0x1a,v6);
	outSISIDXREG(SISSR,0x1b,v7);
	outSISIDXREG(SISSR,0x1c,v8);	   /* ---- */
	andSISIDXREG(SISSR,0x15,0xfb);
	orSISIDXREG(SISSR,0x15,0x04);
	if(sishw_ext.UseROM) {
	   if(sishw_ext.pjVirtualRomBase[0x53] & 0x02) {
	      orSISIDXREG(SISSR,0x19,0x20);
	   }
	}
	v1 = 0x04;			   /* DAC pedestal (BIOS 0xe5) */
	if(ivideo.revision_id >= 0x80) v1 |= 0x01;
	outSISIDXREG(SISSR,0x1f,v1);
	outSISIDXREG(SISSR,0x20,0xa0);     /* linear & relocated io */
	v1 = 0xf6; v2 = 0x0d; v3 = 0x00;
	if(sishw_ext.UseROM) {
	   v1 = sishw_ext.pjVirtualRomBase[0xe8];
	   v2 = sishw_ext.pjVirtualRomBase[0xe9];
	   v3 = sishw_ext.pjVirtualRomBase[0xea];
	}
	outSISIDXREG(SISSR,0x23,v1);
	outSISIDXREG(SISSR,0x24,v2);
	outSISIDXREG(SISSR,0x25,v3);
	outSISIDXREG(SISSR,0x21,0x84);
	outSISIDXREG(SISSR,0x22,0x00);
	outSISIDXREG(SISCR,0x37,0x00);
	orSISIDXREG(SISPART1,0x24,0x01);   /* unlock crt2 */
	outSISIDXREG(SISPART1,0x00,0x00);
	v1 = 0x40; v2 = 0x11;
	if(sishw_ext.UseROM) {
	   v1 = sishw_ext.pjVirtualRomBase[0xec];
	   v2 = sishw_ext.pjVirtualRomBase[0xeb];
	}
	outSISIDXREG(SISPART1,0x02,v1);
	if(ivideo.revision_id >= 0x80) v2 &= ~0x01;
	inSISIDXREG(SISPART4,0x00,reg);
	if((reg == 1) || (reg == 2)) {
	   outSISIDXREG(SISCR,0x37,0x02);
	   outSISIDXREG(SISPART2,0x00,0x1c);
	   v4 = 0x00; v5 = 0x00; v6 = 0x10;
	   if(sishw_ext.UseROM) {
	      v4 = sishw_ext.pjVirtualRomBase[0xf5];
	      v5 = sishw_ext.pjVirtualRomBase[0xf6];
	      v6 = sishw_ext.pjVirtualRomBase[0xf7];
	   }
	   outSISIDXREG(SISPART4,0x0d,v4);
	   outSISIDXREG(SISPART4,0x0e,v5);
	   outSISIDXREG(SISPART4,0x10,v6);
	   outSISIDXREG(SISPART4,0x0f,0x3f);
	   inSISIDXREG(SISPART4,0x01,reg);
	   if(reg >= 0xb0) {
	      inSISIDXREG(SISPART4,0x23,reg);
	      reg &= 0x20;
	      reg <<= 1;
	      outSISIDXREG(SISPART4,0x23,reg);
	   }
	} else {
	   v2 &= ~0x10;
	}
	outSISIDXREG(SISSR,0x32,v2);
	andSISIDXREG(SISPART1,0x24,0xfe);  /* Lock CRT2 */
	inSISIDXREG(SISSR,0x16,reg);
	reg &= 0xc3;
	outSISIDXREG(SISCR,0x35,reg);
	outSISIDXREG(SISCR,0x83,0x00);
#if !defined(__i386__) && !defined(__x86_64__)
	if(sisfb_videoram) {
	   outSISIDXREG(SISSR,0x13,0x28);  /* ? */
	   reg = ((sisfb_videoram >> 10) - 1) | 0x40;
	   outSISIDXREG(SISSR,0x14,reg);
	} else {
#endif
	   /* Need to map max FB size for finding out about RAM size */
	   sishw_ext.pjVideoMemoryAddress = ioremap(ivideo.video_base, 0x4000000);
	   if(sishw_ext.pjVideoMemoryAddress) {
	      sisfb_setramsize300();
	      iounmap(sishw_ext.pjVideoMemoryAddress);
	   } else {
	      printk(KERN_DEBUG "sisfb: Failed to map memory for size detection, assuming 8MB\n");
	      outSISIDXREG(SISSR,0x13,0x28);  /* ? */
	      outSISIDXREG(SISSR,0x14,0x47);  /* 8MB, 64bit default */
	   }
#if !defined(__i386__) && !defined(__x86_64__)
	}
#endif
	if(sishw_ext.UseROM) {
	   v1 = sishw_ext.pjVirtualRomBase[0xe6];
	   v2 = sishw_ext.pjVirtualRomBase[0xe7];
	} else {
	   inSISIDXREG(SISSR,0x3a,reg);
	   if((reg & 0x30) == 0x30) {
	      v1 = 0x04; /* PCI */
	      v2 = 0x92;
	   } else {
	      v1 = 0x14; /* AGP */
	      v2 = 0xb2;
	   }
	}
	outSISIDXREG(SISSR,0x21,v1);
	outSISIDXREG(SISSR,0x22,v2);
}
#endif


int __devinit sisfb_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct sisfb_chip_info *chipinfo = &sisfb_chip_info[ent->driver_data];
	u32 reg32;
	u16 reg16;
	u8  reg;
	int sisvga_enabled = 0;

	if(sisfb_off) return -ENXIO;

	sisfb_thismonitor.datavalid = FALSE;

	memset(&sishw_ext, 0, sizeof(sishw_ext));
	
	memset(&ivideo, 0, sizeof(ivideo));
	ivideo.detectedpdc  = 0xff;
	ivideo.detectedpdca = 0xff;
	ivideo.detectedlcda = 0xff;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
        memset(&sisfb_lastrates[0], 0, 128);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	memset(&sis_disp, 0, sizeof(sis_disp));
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,3))
	sis_fb_info = framebuffer_alloc(0, &pdev->dev);
#else
	sis_fb_info = kmalloc(sizeof(*sis_fb_info), GFP_KERNEL);
#endif
	if(!sis_fb_info) return -ENOMEM;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,3))
	memset(sis_fb_info, 0, sizeof(*sis_fb_info));
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,5,0)
	strcpy(sis_fb_info->modename, chipinfo->chip_name);
#else
	strcpy(myid, chipinfo->chip_name);
#endif

	ivideo.chip_id = pdev->device;
	pci_read_config_byte(pdev, PCI_REVISION_ID, &ivideo.revision_id);
	sishw_ext.jChipRevision = ivideo.revision_id;
	pci_read_config_word(pdev, PCI_COMMAND, &reg16);
	sisvga_enabled = reg16 & 0x01;
	ivideo.pcibus = pdev->bus->number;
	ivideo.pcislot = PCI_SLOT(pdev->devfn);
	ivideo.pcifunc = PCI_FUNC(pdev->devfn);
	ivideo.subsysvendor = pdev->subsystem_vendor;
	ivideo.subsysdevice = pdev->subsystem_device;

	ivideo.chip = chipinfo->chip;
	ivideo.sisvga_engine = chipinfo->vgaengine;
	ivideo.hwcursor_size = chipinfo->hwcursor_size;
	ivideo.CRT2_write_enable = chipinfo->CRT2_write_enable;

	/* Patch special cases */
	switch(ivideo.chip_id) {
#ifdef CONFIG_FB_SIS_300
	   case PCI_DEVICE_ID_SI_630_VGA:
		{
			sisfb_set_reg4(0xCF8, 0x80000000);
			reg32 = sisfb_get_reg3(0xCFC);
			if(reg32 == 0x07301039) {
				ivideo.chip = SIS_730;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,5,0)
				strcpy(sis_fb_info->modename, "SIS 730");
#else
				strcpy(myid, "SIS 730");
#endif
			}
			break;
		}
#endif
#ifdef CONFIG_FB_SIS_315
	   case PCI_DEVICE_ID_SI_650_VGA:
	   	{
			sisfb_set_reg4(0xCF8, 0x80000000);
			reg32 = sisfb_get_reg3(0xCFC);
			if(reg32 == 0x07401039) {
				ivideo.chip = SIS_740;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,5,0)
				strcpy(sis_fb_info->modename, "SIS 740");
#else
				strcpy(myid, "SIS 740");
#endif
			}
			break;
		}
	   case PCI_DEVICE_ID_SI_660_VGA:
	   	{
			sisfb_set_reg4(0xCF8, 0x80000000);
			reg32 = sisfb_get_reg3(0xCFC);
			if(reg32 == 0x07601039) {
				ivideo.chip = SIS_760;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,5,0)
				strcpy(sis_fb_info->modename, "SIS 760");
#else
				strcpy(myid, "SIS 760");
#endif
			} else if(reg32 == 0x06601039) {
				ivideo.chip = SIS_660;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,5,0)
				strcpy(sis_fb_info->modename, "SIS 660");
#else
				strcpy(myid, "SIS 660");
#endif
			} else if(reg32 == 0x07411039) {
				ivideo.chip = SIS_741;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,5,0)
				strcpy(sis_fb_info->modename, "SIS 741");
#else
				strcpy(myid, "SIS 741");
#endif
			} else {
				ivideo.chip = SIS_661;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,5,0)
				strcpy(sis_fb_info->modename, "SIS 661");
#else
				strcpy(myid, "SIS 661");
#endif
			}
			break;
		}
#endif
	}

	sishw_ext.jChipType = ivideo.chip;

	if( (sishw_ext.jChipType == SIS_315PRO) ||
	    (sishw_ext.jChipType == SIS_315) )
		sishw_ext.jChipType = SIS_315H;

	ivideo.video_base = pci_resource_start(pdev, 0);
	ivideo.mmio_base = pci_resource_start(pdev, 1);
	sishw_ext.ulIOAddress = SiS_Pr.RelIO = pci_resource_start(pdev, 2) + 0x30;
	ivideo.vga_base = sishw_ext.ulIOAddress;

	ivideo.mmio_size =  pci_resource_len(pdev, 1);

	if(!sisvga_enabled) {
	   if(pci_enable_device(pdev)) {
	      kfree(sis_fb_info);
	      return -EIO;
	   }
	} 

	SiS_Pr.SiS_Backup70xx = 0xff;
        SiS_Pr.SiS_CHOverScan = -1;
        SiS_Pr.SiS_ChSW = FALSE;
	SiS_Pr.SiS_UseLCDA = FALSE;
	SiS_Pr.HaveEMI = FALSE;
	SiS_Pr.HaveEMILCD = FALSE;
	SiS_Pr.OverruleEMI = FALSE;
	SiS_Pr.SiS_SensibleSR11 = FALSE;
	SiS_Pr.SiS_MyCR63 = 0x63;
	SiS_Pr.PDC = -1;
	SiS_Pr.PDCA = -1;
	if(ivideo.chip >= SIS_661) {
	   SiS_Pr.SiS_SensibleSR11 = TRUE;
	   SiS_Pr.SiS_MyCR63 = 0x53;
	}
	SiSRegInit(&SiS_Pr, sishw_ext.ulIOAddress);

#ifdef CONFIG_FB_SIS_300
	/* Find PCI systems for Chrontel/GPIO communication setup */
	if(ivideo.chip == SIS_630) {
	   int i=0;
           do {
	      if(mychswtable[i].subsysVendor == ivideo.subsysvendor &&
	         mychswtable[i].subsysCard   == ivideo.subsysdevice) {
		 SiS_Pr.SiS_ChSW = TRUE;
		 printk(KERN_DEBUG "sisfb: Identified [%s %s] requiring Chrontel/GPIO setup\n",
		        mychswtable[i].vendorName, mychswtable[i].cardName);
		 break;
              }
              i++;
           } while(mychswtable[i].subsysVendor != 0);
	}
#endif

        outSISIDXREG(SISSR, IND_SIS_PASSWORD, SIS_PASSWORD);

	if( (!sisvga_enabled)
#if !defined(__i386__) && !defined(__x86_64__)
		  	      || (sisfb_resetcard)
#endif
			      			   ) {
	   int i;
	   for(i = 0x30; i <= 0x3f; i++) {
	       outSISIDXREG(SISCR,i,0x00);
	   }
	}

	inSISIDXREG(SISCR,0x34,reg);
	ivideo.modeprechange = ((reg & 0x7f)) ? (reg & 0x7f) : 0x03;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)		
#ifdef MODULE	
	if((reg & 0x80) && (reg != 0xff)) {
	   if((sisbios_mode[sisfb_mode_idx].mode_no) != 0xFF) {
	      printk(KERN_INFO "sisfb: Cannot initialize display mode, X server is active\n");
	      kfree(sis_fb_info);
	      return -EBUSY;
	   }
	}
#endif	
#endif

	if(ivideo.sisvga_engine == SIS_315_VGA) {
	   	sishw_ext.bIntegratedMMEnabled = TRUE;
	} else if(ivideo.sisvga_engine == SIS_300_VGA) {
		if(ivideo.chip == SIS_300) {
			sishw_ext.bIntegratedMMEnabled = TRUE;
		} else {
		        inSISIDXREG(SISSR, IND_SIS_SCRATCH_REG_1A, reg);
			if(reg & SIS_SCRATCH_REG_1A_MASK)
				sishw_ext.bIntegratedMMEnabled = TRUE;
			else
				sishw_ext.bIntegratedMMEnabled = FALSE;
		}
	}

	ivideo.bios_vbase = ivideo.bios_abase = NULL;
	if(sisfb_userom) {
	    sishw_ext.pjVirtualRomBase = sis_find_rom(pdev);
#if defined(__i386__) || defined(__x86_64__)
	    ivideo.bios_vbase = sishw_ext.pjVirtualRomBase;	/* mapped */
#else
	    ivideo.bios_abase = sishw_ext.pjVirtualRomBase;	/* allocated */
#endif
	    if(sishw_ext.pjVirtualRomBase) {
		printk(KERN_INFO "sisfb: Video ROM found and %s to %p\n",
			ivideo.bios_vbase ? "mapped" : "copied",
		        sishw_ext.pjVirtualRomBase);
		sishw_ext.UseROM = TRUE;
	    } else {
	        sishw_ext.UseROM = FALSE;
	        printk(KERN_INFO "sisfb: Video ROM not found\n");
	    }
	} else {
	    sishw_ext.pjVirtualRomBase = NULL;
	    sishw_ext.UseROM = FALSE;
	    printk(KERN_INFO "sisfb: Video ROM usage disabled\n");
	}
	sishw_ext.bSkipDramSizing = 0;

        /* Find systems for special custom timing */
	if(SiS_Pr.SiS_CustomT == CUT_NONE) {
	   int i=0, j;
	   unsigned char *biosver = NULL;
           unsigned char *biosdate = NULL;
	   BOOLEAN footprint;
	   unsigned long chksum = 0;

	   if(sishw_ext.UseROM) {
	      biosver = sishw_ext.pjVirtualRomBase + 0x06;
	      biosdate = sishw_ext.pjVirtualRomBase + 0x2c;
              for(i=0; i<32768; i++) chksum += sishw_ext.pjVirtualRomBase[i];
	   }

	   i=0;
           do {
	      if( (mycustomttable[i].chipID == ivideo.chip) &&
	          ((!strlen(mycustomttable[i].biosversion)) ||
		   (sishw_ext.UseROM &&
		   (!strncmp(mycustomttable[i].biosversion, biosver, strlen(mycustomttable[i].biosversion))))) &&
	          ((!strlen(mycustomttable[i].biosdate)) ||
		   (sishw_ext.UseROM &&
		   (!strncmp(mycustomttable[i].biosdate, biosdate, strlen(mycustomttable[i].biosdate))))) &&
		  ((!mycustomttable[i].bioschksum) ||
		   (sishw_ext.UseROM &&
	           (mycustomttable[i].bioschksum == chksum)))	&&
		  (mycustomttable[i].pcisubsysvendor == ivideo.subsysvendor) &&
		  (mycustomttable[i].pcisubsyscard == ivideo.subsysdevice) ) {
		 footprint = TRUE;
	         for(j=0; j<5; j++) {
	            if(mycustomttable[i].biosFootprintAddr[j]) {
		       if(sishw_ext.UseROM) {
	                  if(sishw_ext.pjVirtualRomBase[mycustomttable[i].biosFootprintAddr[j]] !=
		      		mycustomttable[i].biosFootprintData[j])
		          footprint = FALSE;
		       } else footprint = FALSE;
		    }
	         }
	         if(footprint) {
	 	    SiS_Pr.SiS_CustomT = mycustomttable[i].SpecialID;
		    printk(KERN_DEBUG "sisfb: Identified [%s %s], special timing applies\n",
		        mycustomttable[i].vendorName,
			mycustomttable[i].cardName);
		    printk(KERN_DEBUG "sisfb: [specialtiming parameter name: %s]\n",
		    	mycustomttable[i].optionName);
	            break;
                 }
	      }
              i++;
           } while(mycustomttable[i].chipID);
	}

#ifdef CONFIG_FB_SIS_300
	/* Mode numbers for 1280x768 are different for 300 and 315 series */
	if(ivideo.sisvga_engine == SIS_300_VGA) {
		sisbios_mode[MODEINDEX_1280x768].mode_no = 0x55;
		sisbios_mode[MODEINDEX_1280x768+1].mode_no = 0x5a;
		sisbios_mode[MODEINDEX_1280x768+2].mode_no = 0x5b;
		sisbios_mode[MODEINDEX_1280x768+3].mode_no = 0x5b;
	}
#endif

	sishw_ext.pSR = vmalloc(sizeof(SIS_DSReg) * SR_BUFFER_SIZE);
	if(sishw_ext.pSR == NULL) {
		printk(KERN_ERR "sisfb: Fatal error: Allocating SRReg space failed.\n");
		if(ivideo.bios_abase) vfree(ivideo.bios_abase);
		kfree(sis_fb_info);
		return -ENODEV;
	}
	sishw_ext.pSR[0].jIdx = sishw_ext.pSR[0].jVal = 0xFF;

	sishw_ext.pCR = vmalloc(sizeof(SIS_DSReg) * CR_BUFFER_SIZE);
	if(sishw_ext.pCR == NULL) {
		printk(KERN_ERR "sisfb: Fatal error: Allocating CRReg space failed.\n");
		if(ivideo.bios_abase) vfree(ivideo.bios_abase);
		vfree(sishw_ext.pSR);
		kfree(sis_fb_info);
		return -ENODEV;
	}
	sishw_ext.pCR[0].jIdx = sishw_ext.pCR[0].jVal = 0xFF;

#ifdef CONFIG_FB_SIS_300
	if(ivideo.sisvga_engine == SIS_300_VGA) {
		if( (!sisvga_enabled)
#if !defined(__i386__) && !defined(__x86_64__)
		    		      || (sisfb_resetcard)
#endif
		  					   ) {
			if(ivideo.chip == SIS_300) sisfb_post_sis300();
		}
		if(sisfb_get_dram_size_300()) {
			printk(KERN_ERR "sisfb: Fatal error: Unable to determine RAM size\n");
			if(ivideo.bios_abase) vfree(ivideo.bios_abase);
			vfree(sishw_ext.pSR);
			vfree(sishw_ext.pCR);
			kfree(sis_fb_info);
			return -ENODEV;
		}
	}
#endif

#ifdef CONFIG_FB_SIS_315
	if(ivideo.sisvga_engine == SIS_315_VGA) {
		if( (!sisvga_enabled)
#if !defined(__i386__) && !defined(__x86_64__)
		    		     || (sisfb_resetcard)
#endif
		  					  ) {
			if((sisfb_mode_idx < 0) || ((sisbios_mode[sisfb_mode_idx].mode_no) != 0xFF)) {
				/* TODO: POSTing 315/330 not supported yet */
				/* Mapping Max FB Size for 315 Init */
			      	sishw_ext.pjVideoMemoryAddress = ioremap(ivideo.video_base, 0x8000000);
				outSISIDXREG(SISSR, IND_SIS_PASSWORD, SIS_PASSWORD);
				sishw_ext.bSkipDramSizing = TRUE;
				sishw_ext.pSR[0].jIdx = 0x13;
				sishw_ext.pSR[1].jIdx = 0x14;
				sishw_ext.pSR[2].jIdx = 0xFF;
				inSISIDXREG(SISSR, 0x13, sishw_ext.pSR[0].jVal);
				inSISIDXREG(SISSR, 0x14, sishw_ext.pSR[1].jVal);
				sishw_ext.pSR[2].jVal = 0xFF;
				iounmap(sishw_ext.pjVideoMemoryAddress);
			}
		}
		if(sisfb_get_dram_size_315()) {
			printk(KERN_INFO "sisfb: Fatal error: Unable to determine RAM size.\n");
			if(ivideo.bios_abase) vfree(ivideo.bios_abase);
			vfree(sishw_ext.pSR);
			vfree(sishw_ext.pCR);
			kfree(sis_fb_info);
			return -ENODEV;
		}
	}
#endif

	if((sisfb_mode_idx < 0) || ((sisbios_mode[sisfb_mode_idx].mode_no) != 0xFF)) { 
	        /* Enable PCI_LINEAR_ADDRESSING and MMIO_ENABLE  */
	        orSISIDXREG(SISSR, IND_SIS_PCI_ADDRESS_SET, (SIS_PCI_ADDR_ENABLE | SIS_MEM_MAP_IO_ENABLE));
                /* Enable 2D accelerator engine */
	        orSISIDXREG(SISSR, IND_SIS_MODULE_ENABLE, SIS_ENABLE_2D);
	}

	sishw_ext.ulVideoMemorySize = ivideo.video_size;

	if(sisfb_pdc != 0xff) {
	   if(ivideo.sisvga_engine == SIS_300_VGA) sisfb_pdc &= 0x3c;
	   else				           sisfb_pdc &= 0x1f;
	   SiS_Pr.PDC = sisfb_pdc;
	}
	if(ivideo.sisvga_engine == SIS_315_VGA) {
	   if(sisfb_pdca != 0xff) SiS_Pr.PDCA = sisfb_pdca & 0x1f;
	}

	if(!request_mem_region(ivideo.video_base, ivideo.video_size, "sisfb FB")) {
		printk(KERN_ERR "sisfb: Fatal error: Unable to reserve frame buffer memory\n");
		printk(KERN_ERR "sisfb: Is there another framebuffer driver active?\n");
		if(ivideo.bios_abase) vfree(ivideo.bios_abase);
		vfree(sishw_ext.pSR);
		vfree(sishw_ext.pCR);
		kfree(sis_fb_info);
		return -ENODEV;
	}

	if(!request_mem_region(ivideo.mmio_base, ivideo.mmio_size, "sisfb MMIO")) {
		printk(KERN_ERR "sisfb: Fatal error: Unable to reserve MMIO region\n");
		release_mem_region(ivideo.video_base, ivideo.video_size);
		if(ivideo.bios_abase) vfree(ivideo.bios_abase);
		vfree(sishw_ext.pSR);
		vfree(sishw_ext.pCR);
		kfree(sis_fb_info);
		return -ENODEV;
	}

	ivideo.video_vbase = sishw_ext.pjVideoMemoryAddress = ioremap(ivideo.video_base, ivideo.video_size);
	if(!ivideo.video_vbase) {
	   	printk(KERN_ERR "sisfb: Fatal error: Unable to map frame buffer memory\n");
	   	release_mem_region(ivideo.video_base, ivideo.video_size);
	   	release_mem_region(ivideo.mmio_base, ivideo.mmio_size);
		if(ivideo.bios_abase) vfree(ivideo.bios_abase);
	   	vfree(sishw_ext.pSR);
	   	vfree(sishw_ext.pCR);
	   	kfree(sis_fb_info);
	   	return -ENODEV;
	}

	ivideo.mmio_vbase = ioremap(ivideo.mmio_base, ivideo.mmio_size);
	if(!ivideo.mmio_vbase) {
	   	printk(KERN_ERR "sisfb: Fatal error: Unable to map MMIO region\n");
	   	iounmap(ivideo.video_vbase);
	   	release_mem_region(ivideo.video_base, ivideo.video_size);
	   	release_mem_region(ivideo.mmio_base, ivideo.mmio_size);
		if(ivideo.bios_abase) vfree(ivideo.bios_abase);
	   	vfree(sishw_ext.pSR);
	   	vfree(sishw_ext.pCR);
	   	kfree(sis_fb_info);
	   	return -ENODEV;
	}

	printk(KERN_INFO "sisfb: Framebuffer at 0x%lx, mapped to 0x%p, size %dk\n",
	       	ivideo.video_base, ivideo.video_vbase, ivideo.video_size / 1024);

	printk(KERN_INFO "sisfb: MMIO at 0x%lx, mapped to 0x%p, size %ldk\n",
	       	ivideo.mmio_base, ivideo.mmio_vbase, ivideo.mmio_size / 1024);

	if(sisfb_heap_init()) {
		printk(KERN_WARNING "sisfb: Failed to initialize offscreen memory heap\n");
	}

	ivideo.mtrr = (unsigned int) 0;
	
	ivideo.vbflags = 0;

	ivideo.newrom = SiSDetermineROMLayout661(&SiS_Pr, &sishw_ext);

	if((sisfb_mode_idx < 0) || ((sisbios_mode[sisfb_mode_idx].mode_no) != 0xFF)) { 
	
		sishw_ext.ujVBChipID = VB_CHIP_UNKNOWN;
		sishw_ext.Is301BDH = FALSE;

		sisfb_sense_crt1();
	
		sisfb_get_VB_type();
		
		if(ivideo.vbflags & VB_VIDEOBRIDGE) {
			sisfb_detect_VB_connect();
		}
		
		ivideo.currentvbflags = ivideo.vbflags & (VB_VIDEOBRIDGE | TV_STANDARD);
		
		if(ivideo.vbflags & VB_VIDEOBRIDGE) {
		   if(sisfb_crt2type != -1) {
		      if((sisfb_crt2type == CRT2_LCD) && (ivideo.vbflags & CRT2_LCD)) {
		         ivideo.currentvbflags |= CRT2_LCD;
		      } else if(sisfb_crt2type != CRT2_LCD) {
		         ivideo.currentvbflags |= sisfb_crt2type;
		      }
		   } else {
		      /* Chrontel 700x TV detection often unreliable, therefore use a
		       * different default order on such machines
		       */
		      if((ivideo.sisvga_engine == SIS_300_VGA) && (ivideo.vbflags & VB_CHRONTEL)) {
		         if(ivideo.vbflags & CRT2_LCD)      ivideo.currentvbflags |= CRT2_LCD;
		         else if(ivideo.vbflags & CRT2_TV)  ivideo.currentvbflags |= CRT2_TV;
		         else if(ivideo.vbflags & CRT2_VGA) ivideo.currentvbflags |= CRT2_VGA;
		      } else {
		         if(ivideo.vbflags & CRT2_TV)       ivideo.currentvbflags |= CRT2_TV;
		         else if(ivideo.vbflags & CRT2_LCD) ivideo.currentvbflags |= CRT2_LCD;
		         else if(ivideo.vbflags & CRT2_VGA) ivideo.currentvbflags |= CRT2_VGA;
		      }
		   }
		}

		if(ivideo.vbflags & CRT2_LCD) {
		   inSISIDXREG(SISCR, IND_SIS_LCD_PANEL, reg);
		   reg &= 0x0f;
		   if(ivideo.sisvga_engine == SIS_300_VGA) {
		      sishw_ext.ulCRT2LCDType = sis300paneltype[reg];
		   } else if(ivideo.chip >= SIS_661) {
		      sishw_ext.ulCRT2LCDType = sis661paneltype[reg];
		   } else {
		      sishw_ext.ulCRT2LCDType = sis310paneltype[reg];
		   }
		}
	
#ifdef CONFIG_FB_SIS_300
                /* Save the current PanelDelayCompensation if the LCD is currently used */
		if(ivideo.sisvga_engine == SIS_300_VGA) {
	           if(ivideo.vbflags & (VB_LVDS | VB_30xBDH)) {
		       int tmp;
		       inSISIDXREG(SISCR,0x30,tmp);
		       if(tmp & 0x20) {
		          /* Currently on LCD? If yes, read current pdc */
		          inSISIDXREG(SISPART1,0x13,ivideo.detectedpdc);
			  ivideo.detectedpdc &= 0x3c;
			  if(SiS_Pr.PDC == -1) {
			     /* Let option override detection */
			     SiS_Pr.PDC = ivideo.detectedpdc;
			  }
			  printk(KERN_INFO "sisfb: Detected LCD PDC 0x%02x\n",
  			         ivideo.detectedpdc);
		       }
		       if((SiS_Pr.PDC != -1) && (SiS_Pr.PDC != ivideo.detectedpdc)) {
		          printk(KERN_INFO "sisfb: Using LCD PDC 0x%02x\n",
				 SiS_Pr.PDC);
		       }
	           }
		}
#endif

#ifdef CONFIG_FB_SIS_315
		if(ivideo.sisvga_engine == SIS_315_VGA) {

		   /* Try to find about LCDA */
		   if(ivideo.vbflags & (VB_301C | VB_302B | VB_301LV | VB_302LV | VB_302ELV)) {
		      int tmp;
		      inSISIDXREG(SISPART1,0x13,tmp);
		      if(tmp & 0x04) {
		         SiS_Pr.SiS_UseLCDA = TRUE;
		         ivideo.detectedlcda = 0x03;
		         printk(KERN_DEBUG
			        "sisfb: BIOS uses LCDA for low resolution and text modes\n");
		      }
	           }

		   /* Save PDC */
		   if(ivideo.vbflags & (VB_301LV | VB_302LV | VB_302ELV)) {
		      int tmp;
		      inSISIDXREG(SISCR,0x30,tmp);
		      if((tmp & 0x20) || (ivideo.detectedlcda != 0xff)) {
		         /* Currently on LCD? If yes, read current pdc */
			 u8 pdc;
		         inSISIDXREG(SISPART1,0x2D,pdc);
			 ivideo.detectedpdc  = (pdc & 0x0f) << 1;
			 ivideo.detectedpdca = (pdc & 0xf0) >> 3;
			 inSISIDXREG(SISPART1,0x35,pdc);
			 ivideo.detectedpdc |= ((pdc >> 7) & 0x01);
			 inSISIDXREG(SISPART1,0x20,pdc);
			 ivideo.detectedpdca |= ((pdc >> 6) & 0x01);
			 if(ivideo.newrom) {
			    /* New ROM invalidates other PDC resp. */
			    if(ivideo.detectedlcda != 0xff) {
			       ivideo.detectedpdc = 0xff;
			    } else {
			       ivideo.detectedpdca = 0xff;
			    }
			 }
			 if(SiS_Pr.PDC == -1) {
			    if(ivideo.detectedpdc != 0xff) {
			       SiS_Pr.PDC = ivideo.detectedpdc;
			    }
			 }
			 if(SiS_Pr.PDCA == -1) {
			    if(ivideo.detectedpdca != 0xff) {
			       SiS_Pr.PDCA = ivideo.detectedpdca;
			    }
			 }
			 if(ivideo.detectedpdc != 0xff) {
			    printk(KERN_INFO
			         "sisfb: Detected LCD PDC 0x%02x (for LCD=CRT2)\n",
  			          ivideo.detectedpdc);
			 }
			 if(ivideo.detectedpdca != 0xff) {
			    printk(KERN_INFO
			         "sisfb: Detected LCD PDC1 0x%02x (for LCD=CRT1)\n",
  			          ivideo.detectedpdca);
			 }
		      }

		      /* Save EMI */
		      if(ivideo.vbflags & (VB_302LV | VB_302ELV)) {
		         inSISIDXREG(SISPART4,0x30,SiS_Pr.EMI_30);
			 inSISIDXREG(SISPART4,0x31,SiS_Pr.EMI_31);
			 inSISIDXREG(SISPART4,0x32,SiS_Pr.EMI_32);
			 inSISIDXREG(SISPART4,0x33,SiS_Pr.EMI_33);
			 SiS_Pr.HaveEMI = TRUE;
			 if((tmp & 0x20) || (ivideo.detectedlcda != 0xff)) {
			  	SiS_Pr.HaveEMILCD = TRUE;
			 }
		      }
		   }

		   /* Let user override detected PDCs (all bridges) */
		   if(ivideo.vbflags & (VB_301B | VB_301C | VB_301LV | VB_302LV | VB_302ELV)) {
		      if((SiS_Pr.PDC != -1) && (SiS_Pr.PDC != ivideo.detectedpdc)) {
		         printk(KERN_INFO "sisfb: Using LCD PDC 0x%02x (for LCD=CRT2)\n",
				 SiS_Pr.PDC);
		      }
		      if((SiS_Pr.PDCA != -1) && (SiS_Pr.PDCA != ivideo.detectedpdca)) {
		         printk(KERN_INFO "sisfb: Using LCD PDC1 0x%02x (for LCD=CRT1)\n",
				 SiS_Pr.PDCA);
		      }
		   }

		}
#endif

		if(!sisfb_crt1off) {
		   	sisfb_handle_ddc(&sisfb_thismonitor, 0);
		} else {
		   	if((ivideo.vbflags & (VB_301|VB_301B|VB_301C|VB_302B)) &&
		      	   (ivideo.vbflags & (CRT2_VGA | CRT2_LCD))) {
		      		sisfb_handle_ddc(&sisfb_thismonitor, 1);
		   	}
		}

		if(sisfb_mode_idx >= 0)
			sisfb_mode_idx = sisfb_validate_mode(sisfb_mode_idx, ivideo.currentvbflags);

		if(sisfb_mode_idx < 0) {
			switch(ivideo.currentvbflags & VB_DISPTYPE_DISP2) {
			   case CRT2_LCD:
				sisfb_mode_idx = DEFAULT_LCDMODE;
				break;
			   case CRT2_TV:
				sisfb_mode_idx = DEFAULT_TVMODE;
				break;
			   default:
				sisfb_mode_idx = DEFAULT_MODE;
				break;
			}
		}

		ivideo.mode_no = sisbios_mode[sisfb_mode_idx].mode_no;

		if(ivideo.refresh_rate != 0)
			sisfb_search_refresh_rate(ivideo.refresh_rate, sisfb_mode_idx);

		if(ivideo.rate_idx == 0) {
			ivideo.rate_idx = sisbios_mode[sisfb_mode_idx].rate_idx;
			ivideo.refresh_rate = 60;
		}

		if(sisfb_thismonitor.datavalid) {
			if(!sisfb_verify_rate(&sisfb_thismonitor, sisfb_mode_idx,
			                      ivideo.rate_idx, ivideo.refresh_rate)) {
				printk(KERN_INFO "sisfb: WARNING: Refresh rate exceeds monitor specs!\n");
			}
		}

		ivideo.video_bpp = sisbios_mode[sisfb_mode_idx].bpp;
		ivideo.video_vwidth = ivideo.video_width = sisbios_mode[sisfb_mode_idx].xres;
		ivideo.video_vheight = ivideo.video_height = sisbios_mode[sisfb_mode_idx].yres;
		ivideo.org_x = ivideo.org_y = 0;
		ivideo.video_linelength = ivideo.video_width * (ivideo.video_bpp >> 3);
		
		sisfb_set_vparms();
		
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)	

		/* ---------------- For 2.4: Now switch the mode ------------------ */		
		
		printk(KERN_INFO "sisfb: Mode is %dx%dx%d (%dHz)\n",
	       		ivideo.video_width, ivideo.video_height, ivideo.video_bpp,
			ivideo.refresh_rate);

		sisfb_pre_setmode();

		if(SiSSetMode(&SiS_Pr, &sishw_ext, ivideo.mode_no) == 0) {
			printk(KERN_ERR "sisfb: Fatal error: Setting mode[0x%x] failed\n",
				ivideo.mode_no);
			if(ivideo.bios_abase) vfree(ivideo.bios_abase);
			vfree(sishw_ext.pSR);
			vfree(sishw_ext.pCR);
			release_mem_region(ivideo.video_base, ivideo.video_size);
			release_mem_region(ivideo.mmio_base, ivideo.mmio_size);
			kfree(sis_fb_info);
			return -EINVAL;
		}

		outSISIDXREG(SISSR, IND_SIS_PASSWORD, SIS_PASSWORD);

		sisfb_post_setmode();
		
		ivideo.accel = 0;
		if(sisfb_accel) {
		   ivideo.accel = -1;
		   default_var.accel_flags |= FB_ACCELF_TEXT;
		   sisfb_initaccel();
		}

		/* Maximize regardless of sisfb_max at startup */
		default_var.yres_virtual = 32767;
		sisfb_crtc_to_var(&default_var);
		
		sis_fb_info->node = -1;
		sis_fb_info->flags = FBINFO_FLAG_DEFAULT;
		sis_fb_info->blank = &sisfb_blank;
		sis_fb_info->fbops = &sisfb_ops;
		sis_fb_info->switch_con = &sisfb_switch;
		sis_fb_info->updatevar = &sisfb_update_var;
		sis_fb_info->changevar = NULL;
		sis_fb_info->disp = &sis_disp;
		strcpy(sis_fb_info->fontname, sisfb_fontname);
			
		sisfb_set_disp(-1, &default_var, sis_fb_info);
		
#else		/* --------- For 2.6: Setup a somewhat sane default var ------------ */

		printk(KERN_INFO "sisfb: Default mode is %dx%dx%d (%dHz)\n",
	       		ivideo.video_width, ivideo.video_height, ivideo.video_bpp,
			ivideo.refresh_rate);

		default_var.xres = default_var.xres_virtual = ivideo.video_width;
		default_var.yres = default_var.yres_virtual = ivideo.video_height;
		default_var.bits_per_pixel = ivideo.video_bpp;

		sisfb_bpp_to_var(&default_var);
		
		default_var.pixclock = (u32) (1000000000 /
				sisfb_mode_rate_to_dclock(&SiS_Pr, &sishw_ext,
						ivideo.mode_no, ivideo.rate_idx));
						
		if(sisfb_mode_rate_to_ddata(&SiS_Pr, &sishw_ext,
			 ivideo.mode_no, ivideo.rate_idx,
			 &default_var.left_margin, &default_var.right_margin, 
			 &default_var.upper_margin, &default_var.lower_margin,
			 &default_var.hsync_len, &default_var.vsync_len,
			 &default_var.sync, &default_var.vmode)) {
		   if((default_var.vmode & FB_VMODE_MASK) == FB_VMODE_DOUBLE) {
		      default_var.pixclock <<= 1;
	   	   }
	        }

		ivideo.accel = 0;
		if(sisfb_accel) {
		   ivideo.accel = -1;
		   default_var.accel_flags |= FB_ACCELF_TEXT;
		   sisfb_initaccel();
		}
		
		if(sisfb_ypan) {
		   /* Maximize regardless of sisfb_max at startup */
	    	   default_var.yres_virtual =
				ivideo.heapstart / (default_var.xres * (default_var.bits_per_pixel >> 3));
		   if(default_var.yres_virtual > 32767) default_var.yres_virtual = 32767;
	    	   if(default_var.yres_virtual <= default_var.yres) {
	              default_var.yres_virtual = default_var.yres;
	    	   }
		}

		sis_fb_info->flags = FBINFO_FLAG_DEFAULT;
		sis_fb_info->var = default_var;
		sis_fb_info->fix = sisfb_fix;
		sis_fb_info->par = &ivideo;
		sis_fb_info->screen_base = ivideo.video_vbase;
		sis_fb_info->fbops = &sisfb_ops;
#ifdef NEWFBDEV
		sis_fb_info->class_dev.dev = &pdev->dev;
#endif
		sisfb_get_fix(&sis_fb_info->fix, -1, sis_fb_info);
		sis_fb_info->pseudo_palette = pseudo_palette;
		
		fb_alloc_cmap(&sis_fb_info->cmap, 256 , 0);
#endif

		printk(KERN_DEBUG "sisfb: Initial vbflags 0x%lx\n", ivideo.vbflags);

#ifdef CONFIG_MTRR
		ivideo.mtrr = mtrr_add((unsigned int) ivideo.video_base,
				(unsigned int) ivideo.video_size,
				MTRR_TYPE_WRCOMB, 1);
		if(ivideo.mtrr) {
			printk(KERN_DEBUG "sisfb: Added MTRRs\n");
		}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
		vc_resize_con(1, 1, 0);
#endif

		if(register_framebuffer(sis_fb_info) < 0) {
			if(ivideo.bios_abase) vfree(ivideo.bios_abase);
			vfree(sishw_ext.pSR);
			vfree(sishw_ext.pCR);
			release_mem_region(ivideo.video_base, ivideo.video_size);
			release_mem_region(ivideo.mmio_base, ivideo.mmio_size);
			printk(KERN_ERR "sisfb: Fatal error: Failed to register framebuffer\n");
			kfree(sis_fb_info);
			return -EINVAL;
		}

		ivideo.registered = 1;

		pci_set_drvdata(pdev, sis_fb_info);

		printk(KERN_DEBUG "sisfb: Installed SISFB_GET_INFO ioctl (%x)\n", SISFB_GET_INFO);
		printk(KERN_DEBUG "sisfb: Installed SISFB_GET_VBRSTATUS ioctl (%x)\n", SISFB_GET_VBRSTATUS);

		printk(KERN_INFO "sisfb: 2D acceleration is %s, scrolling mode %s\n",
		     sisfb_accel ? "enabled" : "disabled",
		     sisfb_ypan  ? (sisfb_max ? "ypan (auto-max)" : "ypan (no auto-max)") : "redraw");


		printk(KERN_INFO "fb%d: %s frame buffer device, Version %d.%d.%02d\n",
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	       		GET_FB_IDX(sis_fb_info->node), sis_fb_info->modename,
#else
	       		sis_fb_info->node, myid,
#endif
			VER_MAJOR, VER_MINOR, VER_LEVEL);

		printk(KERN_INFO "sisfb: (C) 2001-2004 Thomas Winischhofer.\n");

	}	/* if mode = "none" */

	return 0;
}

/*****************************************************/
/*                PCI DEVICE HANDLING                */
/*****************************************************/

static void __devexit sisfb_remove(struct pci_dev *pdev)
{
	/* Unmap */
	iounmap(ivideo.video_vbase);
	iounmap(ivideo.mmio_vbase);
	if(ivideo.bios_vbase) iounmap(ivideo.bios_vbase);

	/* Release mem regions */
	release_mem_region(ivideo.video_base, ivideo.video_size);
	release_mem_region(ivideo.mmio_base, ivideo.mmio_size);

#ifdef CONFIG_MTRR
	/* Release MTRR region */
	if(ivideo.mtrr) {
		mtrr_del(ivideo.mtrr,
		      (unsigned int)ivideo.video_base,
	              (unsigned int)ivideo.video_size);
	}
#endif

	/* Unregister the framebuffer */
	if(ivideo.registered) {
		unregister_framebuffer(sis_fb_info);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,3))
		framebuffer_release(sis_fb_info);
#else
		kfree(sis_fb_info);
#endif
	}

	if(sishw_ext.pSR) vfree(sishw_ext.pSR);
	if(sishw_ext.pCR) vfree(sishw_ext.pCR);
	if(ivideo.bios_abase) vfree(ivideo.bios_abase);

	pci_set_drvdata(pdev, NULL);

	/* TODO: Restore the initial mode
	 * This sounds easy but is as good as impossible
	 * on many machines with SiS chip and video bridge
	 * since text modes are always set up differently
	 * from machine to machine. Depends on the type
	 * of integration between chipset and bridge.
	 */
	if(ivideo.registered) {
	   printk(KERN_INFO "sisfb: Restoring of text mode not supported yet\n");
	}
};

static struct pci_driver sisfb_driver = {
	.name		= "sisfb",
	.id_table 	= sisfb_pci_table,
	.probe 		= sisfb_probe,
	.remove 	= __devexit_p(sisfb_remove)
};

int __init sisfb_init(void)
{
	return(pci_module_init(&sisfb_driver));
}

/*****************************************************/
/*                      MODULE                       */
/*****************************************************/

#ifdef MODULE

static char         *mode = NULL;
static int          vesa = -1;
static unsigned int rate = 0;
static unsigned int crt1off = 1;
static unsigned int mem = 0;
static char         *forcecrt2type = NULL;
static int          forcecrt1 = -1;
static char         *queuemode = NULL;
static int          pdc = -1;
static int          pdc1 = -1;
static int          noaccel = -1;
static int          noypan  = -1;
static int	    nomax = -1;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
static int          inverse = 0;
#endif
static int          userom = 1;
static int          useoem = -1;
static char         *tvstandard = NULL;
static int	    nocrt2rate = 0;
static int          scalelcd = -1;
static char	    *specialtiming = NULL;
static int	    lvdshl = -1;
#if !defined(__i386__) && !defined(__x86_64__)
static int	    resetcard = 0;
static int	    videoram = 0;
#endif

MODULE_DESCRIPTION("SiS 300/540/630/730/315/550/65x/661/74x/330/760 framebuffer driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Thomas Winischhofer <thomas@winischhofer.net>; SiS; Others");

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
MODULE_PARM(mode, "s");
MODULE_PARM_DESC(mode,
       "\nSelects the desired display mode in the format [X]x[Y]x[Depth], eg.\n"
         "1024x768x16. Other formats supported include XxY-Depth and\n"
	 "XxY-Depth@Rate. If the parameter is only one (decimal or hexadecimal)\n"
	 "number, it will be interpreted as a VESA mode number. (default: none if\n"
	 "sisfb is a module; this leaves the console untouched and the driver will\n"
	 "only do the video memory management for eg. DRM/DRI; 800x600x8 if sisfb\n"
	 "is in the kernel)");
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)	 
MODULE_PARM(mode, "s");
MODULE_PARM_DESC(mode,
       "\nSelects the desired default display mode in the format XxYxDepth,\n"
         "eg. 1024x768x16. Other formats supported include XxY-Depth and\n"
	 "XxY-Depth@Rate. If the parameter is only one (decimal or hexadecimal)\n"
	 "number, it will be interpreted as a VESA mode number. (default: 800x600x8)");
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
MODULE_PARM(vesa, "i");
MODULE_PARM_DESC(vesa,
       "\nSelects the desired display mode by VESA defined mode number, eg. 0x117\n"
         "(default: 0x0000 if sisfb is a module; this leaves the console untouched\n"
	 "and the driver will only do the video memory management for eg. DRM/DRI;\n"
	 "0x0103 if sisfb is in the kernel)");
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
MODULE_PARM(vesa, "i");
MODULE_PARM_DESC(vesa,
       "\nSelects the desired default display mode by VESA defined mode number, eg.\n"
         "0x117 (default: 0x0103)");
#endif

MODULE_PARM(rate, "i");
MODULE_PARM_DESC(rate,
	"\nSelects the desired vertical refresh rate for CRT1 (external VGA) in Hz.\n"
	  "If the mode is specified in the format XxY-Depth@Rate, this parameter\n"
	  "will be ignored (default: 60)");

MODULE_PARM(crt1off,   "i");
MODULE_PARM_DESC(crt1off,
	"(Deprecated, please use forcecrt1)");

MODULE_PARM(filter, "i");
MODULE_PARM_DESC(filter,
	"\nSelects TV flicker filter type (only for systems with a SiS301 video bridge).\n"
	  "(Possible values 0-7, default: [no filter])");

MODULE_PARM(queuemode,   "s");
MODULE_PARM_DESC(queuemode,
	"\nSelects the queue mode on 315/550/65x/74x/330/760. Possible choices are AGP,\n"
  	  "VRAM and MMIO. AGP is only available if the kernel has AGP support. The queue\n"
	  "mode is important to programs using the 2D/3D accelerator of the SiS chip.\n"
	  "The modes require a totally different way of programming the engines. If any\n"
	  "mode than MMIO is selected, sisfb will disable its own 2D acceleration. On\n"
	  "300/540/630/730, this option is ignored. (default: MMIO)");

/* TW: "Import" the options from the X driver */
MODULE_PARM(mem,    "i");
MODULE_PARM_DESC(mem,
	"\nDetermines the beginning of the video memory heap in KB. This heap is used\n"
	  "for video RAM management for eg. DRM/DRI. On 300 series, the default depends\n"
	  "on the amount of video RAM available. If 8MB of video RAM or less is available,\n"
	  "the heap starts at 4096KB, if between 8 and 16MB are available at 8192KB,\n"
	  "otherwise at 12288KB. On 315 and Xabre series, the heap is 1MB by default. The\n"
	  "value is to be specified without 'KB' and should match the MaxXFBMem setting\n"
	  "for XFree 4.x (x>=2).");

MODULE_PARM(forcecrt2type, "s");
MODULE_PARM_DESC(forcecrt2type,
	"\nIf this option is omitted, the driver autodetects CRT2 output devices, such as\n"
	  "LCD, TV or secondary VGA. With this option, this autodetection can be\n"
	  "overridden. Possible parameters are LCD, TV, VGA or NONE. NONE disables CRT2.\n"
	  "On systems with a SiS video bridge, parameters SVIDEO, COMPOSITE or SCART can\n"
	  "be used instead of TV to override the TV detection. (default: [autodetected])");

MODULE_PARM(forcecrt1, "i");
MODULE_PARM_DESC(forcecrt1,
	"\nNormally, the driver autodetects whether or not CRT1 (external VGA) is \n"
	  "connected. With this option, the detection can be overridden (1=CRT1 ON,\n"
	  " 0=CRT1 off) (default: [autodetected])");

MODULE_PARM(pdc, "i");
MODULE_PARM_DESC(pdc,
        "\nThis is for manually selecting the LCD panel delay compensation. The driver\n"
	  "should detect this correctly in most cases; however, sometimes this is not\n"
	  "possible. If you see 'small waves' on the LCD, try setting this to 4, 32 or 24\n"
	  "on a 300 series chipset; 6 on a 315 series chipset. If the problem persists,\n"
	  "try other values (on 300 series: between 4 and 60 in steps of 4; on 315 series:\n"
	  "any value from 0 to 31). (default: autodetected, if LCD is active during start)");

MODULE_PARM(pdc1, "i");
MODULE_PARM_DESC(pdc1,
        "\nThis is same as pdc, but for LCD-via CRT1. Hence, this is for the 315 series\n"
	  "only. (default: autodetected if LCD is in LCD-via-CRT1 mode during startup)");

MODULE_PARM(noaccel, "i");
MODULE_PARM_DESC(noaccel,
        "\nIf set to anything other than 0, 2D acceleration will be disabled.\n"
	  "(default: 0)");

MODULE_PARM(noypan, "i");
MODULE_PARM_DESC(noypan,
        "\nIf set to anything other than 0, y-panning will be disabled and scrolling\n"
 	  "will be performed by redrawing the screen. (default: 0)");

MODULE_PARM(nomax, "i");
MODULE_PARM_DESC(nomax,
        "\nIf y-panning is enabled, sisfb will by default use the entire available video\n"
	  "memory for the virtual screen in order to optimize scrolling performance. If\n"
	  "this is set to anything other than 0, sisfb will not do this and thereby \n"
	  "enable the user to positively specify a virtual Y size of the screen using\n"
	  "fbset. (default: 0)\n");

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)	
MODULE_PARM(inverse, "i");
MODULE_PARM_DESC(inverse,
        "\nSetting this to anything but 0 should invert the display colors, but this\n"
	  "does not seem to work. (default: 0)");
#endif	

MODULE_PARM(userom, "i");
MODULE_PARM_DESC(userom,
        "\nSetting this to 0 keeps sisfb from using the video BIOS data which is needed\n"
	  "for some LCD and TV setup. (default: 1)");

MODULE_PARM(useoem, "i");
MODULE_PARM_DESC(useoem,
        "\nSetting this to 0 keeps sisfb from using its internel OEM data for some LCD\n"
	  "panels and TV connector types. (default: [auto])");

MODULE_PARM(tvstandard, "s");
MODULE_PARM_DESC(tvstandard,
	"\nThis allows overriding the BIOS default for the TV standard. Valid choices are\n"
	  "pal and ntsc. (default: [auto])");

MODULE_PARM(nocrt2rate, "i");
MODULE_PARM_DESC(nocrt2rate,
	"\nSetting this to 1 will force the driver to use the default refresh rate for\n"
	  "CRT2 if CRT2 type is VGA. (default: 0, use same rate as CRT1)");

MODULE_PARM(scalelcd, "i");
MODULE_PARM_DESC(scalelcd,
	"\nSetting this to 1 will force the driver to scale the LCD image to the panel's\n"
	  "native resolution. Setting it to 0 will disable scaling; if the panel can\n"
	  "scale by itself, it will probably do this, otherwise you will see a black bar\n"
	  "around the screen image. Default: [autodetect if panel can scale]");

MODULE_PARM(specialtiming, "s");

MODULE_PARM(lvdshl, "i");

#if !defined(__i386__) && !defined(__x86_64__)
MODULE_PARM(resetcard, "i");
MODULE_PARM_DESC(resetcard,
	"\nSet this to 1 in order to reset (POST) the card on non-x86 machines where\n"
	  "the BIOS did not POST the card (only supported for SiS 300/305 currently).\n"
	  "Default: 0");
MODULE_PARM(videoram, "i");
MODULE_PARM_DESC(videoram,
	"\nSet this to the amount of video RAM (in kilobyte) the card has. Required on\n"
	  "some non-x86 architectures where the memory auto detection fails. Only\n"
	  "relevant if resetcard is set, too. Default: [auto-detect]");
#endif

int __init sisfb_init_module(void)
{
	SiS_Pr.UsePanelScaler = -1;
	SiS_Pr.CenterScreen = -1;
	SiS_Pr.SiS_CustomT = CUT_NONE;
	SiS_Pr.LVDSHL = -1;

	ivideo.refresh_rate = sisfb_parm_rate = rate;

	if((scalelcd == 0) || (scalelcd == 1)) {
	   SiS_Pr.UsePanelScaler = scalelcd ^ 1;
	}

	if(mode)
		sisfb_search_mode(mode, FALSE);
	else if(vesa != -1)
		sisfb_search_vesamode(vesa, FALSE);
	else
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
		/* For 2.4, set mode=none if no mode is given  */
		sisfb_mode_idx = MODE_INDEX_NONE;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
		/* For 2.5, we don't need this "mode=none" stuff anymore */
		sisfb_mode_idx = DEFAULT_MODE;
#endif

	if(forcecrt2type)
		sisfb_search_crt2type(forcecrt2type);

	if(tvstandard)
		sisfb_search_tvstd(tvstandard);

	if(crt1off == 0)
		sisfb_crt1off = 1;
	else
		sisfb_crt1off = 0;

	sisfb_forcecrt1 = forcecrt1;
	if(forcecrt1 == 1)
		sisfb_crt1off = 0;
	else if(forcecrt1 == 0)
		sisfb_crt1off = 1;

	if(noaccel == 1)      sisfb_accel = 0;
	else if(noaccel == 0) sisfb_accel = 1;

	if(noypan == 1)       sisfb_ypan = 0;
	else if(noypan == 0)  sisfb_ypan = 1;

	if(nomax == 1)        sisfb_max = 0;
	else if(nomax == 0)   sisfb_max = 1;
	
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	if(inverse)           sisfb_inverse = 1;
	sisfb_fontname[0] = '\0';
#endif

	if(mem)		      sisfb_mem = mem;

	sisfb_userom = userom;

	sisfb_useoem = useoem;

	if(queuemode)         sisfb_search_queuemode(queuemode);

	/* If other queuemode than MMIO, disable 2D accel and ypan */
	if((sisfb_queuemode != -1) && (sisfb_queuemode != MMIO_CMD)) {
	        sisfb_accel = 0;
	}

        if(pdc != -1)  sisfb_pdc  = (pdc  & 0x7f);
	if(pdc1 != -1) sisfb_pdca = (pdc1 & 0x1f);

	sisfb_nocrt2rate = nocrt2rate;

	if(specialtiming)
		sisfb_search_specialtiming(specialtiming);

	if((lvdshl >= 0) && (lvdshl <= 3)) SiS_Pr.LVDSHL = lvdshl;

#if !defined(__i386__) && !defined(__x86_64__)
        if(resetcard)   sisfb_resetcard = 1;
	else		sisfb_resetcard = 0;

	if(videoram)    sisfb_videoram = videoram;
#endif

        return(sisfb_init());
}

static void __exit sisfb_remove_module(void)
{
	pci_unregister_driver(&sisfb_driver);
	printk(KERN_DEBUG "sisfb: Module unloaded\n");
}

module_init(sisfb_init_module);
module_exit(sisfb_remove_module);

#endif 	   /*  /MODULE  */

EXPORT_SYMBOL(sis_malloc);
EXPORT_SYMBOL(sis_free);
EXPORT_SYMBOL(sis_dispinfo);

EXPORT_SYMBOL(ivideo);


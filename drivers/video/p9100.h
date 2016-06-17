/*
 * Register information for the Weitek P9100 as found
 * on the Tadpole Sparcbook 3 laptops.
 * 
 * From the technical specification document provided by Tadpole.
 *
 * Derrick J Brashear (shadow@dementia.org)
 */

#ifndef _P9100_H_
#define _P9100_H_

/* P9100 control registers */
#define P9100_SYSCTL_OFF	0x0UL
#define P9100_VIDEOCTL_OFF	0x100UL
#define P9100_VRAMCTL_OFF 	0x180UL
#define P9100_RAMDAC_OFF 	0x200UL
#define P9100_VIDEOCOPROC_OFF 	0x400UL

/* P9100 command registers */
#define P9100_CMD_OFF 0x0UL

/* P9100 framebuffer memory */
#define P9100_FB_OFF 0x0UL


/* 3 bits: 2=8bpp 3=16bpp 5=32bpp 7=24bpp */
#define SYS_CONFIG_PIXELSIZE_SHIFT 26 

#define SCREENPAINT_TIMECTL1_ENABLE_VIDEO 0x20 /* 0 = off, 1 = on */

struct p9100_ctrl {
  /* Registers for the system control */
  __volatile__ __u32 sys_base;
  __volatile__ __u32 sys_config;
  __volatile__ __u32 sys_intr;
  __volatile__ __u32 sys_int_ena;
  __volatile__ __u32 sys_alt_rd;
  __volatile__ __u32 sys_alt_wr;
  __volatile__ __u32 sys_xxx[58];
  /* Registers for the video control */
  __volatile__ __u32 vid_base;
  __volatile__ __u32 vid_hcnt;
  __volatile__ __u32 vid_htotal;
  __volatile__ __u32 vid_hsync_rise;
  __volatile__ __u32 vid_hblank_rise;
  __volatile__ __u32 vid_hblank_fall;
  __volatile__ __u32 vid_hcnt_preload;
  __volatile__ __u32 vid_vcnt;
  __volatile__ __u32 vid_vlen;
  __volatile__ __u32 vid_vsync_rise;
  __volatile__ __u32 vid_vblank_rise;
  __volatile__ __u32 vid_vblank_fall;
  __volatile__ __u32 vid_vcnt_preload;
  __volatile__ __u32 vid_screenpaint_addr;
  __volatile__ __u32 vid_screenpaint_timectl1;
  __volatile__ __u32 vid_screenpaint_qsfcnt;
  __volatile__ __u32 vid_screenpaint_timectl2;
  __volatile__ __u32 vid_xxx[15];
  /* Registers for the video control */
  __volatile__ __u32 vram_base;
  __volatile__ __u32 vram_memcfg;
  __volatile__ __u32 vram_refresh_pd;
  __volatile__ __u32 vram_refresh_cnt;
  __volatile__ __u32 vram_raslo_max;
  __volatile__ __u32 vram_raslo_cur;
  __volatile__ __u32 pwrup_cfg;
  __volatile__ __u32 vram_xxx[25];
  /* Registers for IBM RGB528 Palette */
  __volatile__ __u32 ramdac_cmap_wridx; 
  __volatile__ __u32 ramdac_palette_data;
  __volatile__ __u32 ramdac_pixel_mask;
  __volatile__ __u32 ramdac_palette_rdaddr;
  __volatile__ __u32 ramdac_idx_lo;
  __volatile__ __u32 ramdac_idx_hi;
  __volatile__ __u32 ramdac_idx_data;
  __volatile__ __u32 ramdac_idx_ctl;
  __volatile__ __u32 ramdac_xxx[1784];
};

struct p9100_cmd_parameng {
  __volatile__ __u32 parameng_status;
  __volatile__ __u32 parameng_bltcmd;
  __volatile__ __u32 parameng_quadcmd;
};

#endif /* _P9100_H_ */

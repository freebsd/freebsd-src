/***************************************************************************
 * Video4Linux driver for W996[87]CF JPEG USB Dual Mode Camera Chip.       *
 *                                                                         *
 * Copyright (C) 2002 2003 by Luca Risolia <luca.risolia@studio.unibo.it>  *
 *                                                                         *
 * - Memory management code from bttv driver by Ralph Metzler,             *
 *   Marcus Metzler and Gerd Knorr.                                        *
 * - I2C interface to kernel, high-level CMOS sensor control routines and  *
 *   some symbolic names from OV511 driver by Mark W. McClelland.          *
 * - Low-level I2C fast write function by Piotr Czerczak.                  *
 * - Low-level I2C read function by Frederic Jouault.                      *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify    *
 * it under the terms of the GNU General Public License as published by    *
 * the Free Software Foundation; either version 2 of the License, or       *
 * (at your option) any later version.                                     *
 *                                                                         *
 * This program is distributed in the hope that it will be useful,         *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License       *
 * along with this program; if not, write to the Free Software             *
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.               *
 ***************************************************************************/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <linux/wrapper.h>
#include <asm/page.h>
#include <asm/uaccess.h>

#include "w9968cf.h"
#include "w9968cf_decoder.h"


/****************************************************************************
 * Module macros and paramaters                                             *
 ****************************************************************************/

MODULE_AUTHOR(W9968CF_MODULE_AUTHOR" "W9968CF_AUTHOR_EMAIL);
MODULE_DESCRIPTION(W9968CF_MODULE_NAME" "W9968CF_MODULE_VERSION);
MODULE_LICENSE(W9968CF_MODULE_LICENSE);
MODULE_SUPPORTED_DEVICE("Video");

static u8 vppmod_load = W9968CF_VPPMOD_LOAD;
static u8 simcams = W9968CF_SIMCAMS;
static s8 video_nr[]={[0 ... W9968CF_MAX_DEVICES-1] = -1}; /* -1=first free */
static u16 packet_size[] = {[0 ... W9968CF_MAX_DEVICES-1]=W9968CF_PACKET_SIZE};
static u8 max_buffers[] = {[0 ... W9968CF_MAX_DEVICES-1] = W9968CF_BUFFERS};
static u8 double_buffer[] = {[0 ... W9968CF_MAX_DEVICES-1] = 
                             W9968CF_DOUBLE_BUFFER};
static u8 clamping[] = {[0 ... W9968CF_MAX_DEVICES-1] = W9968CF_CLAMPING};
static u8 filter_type[]= {[0 ... W9968CF_MAX_DEVICES-1] = W9968CF_FILTER_TYPE};
static u8 largeview[]= {[0 ... W9968CF_MAX_DEVICES-1] = W9968CF_LARGEVIEW};
static u8 decompression[] = {[0 ... W9968CF_MAX_DEVICES-1] = 
                             W9968CF_DECOMPRESSION};
static u8 upscaling[]= {[0 ... W9968CF_MAX_DEVICES-1] = W9968CF_UPSCALING};
static u8 force_palette[] = {[0 ... W9968CF_MAX_DEVICES-1] = 0};
static u8 force_rgb[] = {[0 ... W9968CF_MAX_DEVICES-1] = W9968CF_FORCE_RGB};
static u8 autobright[] = {[0 ... W9968CF_MAX_DEVICES-1] = W9968CF_AUTOBRIGHT};
static u8 autoexp[] = {[0 ... W9968CF_MAX_DEVICES-1] = W9968CF_AUTOEXP};
static u8 lightfreq[] = {[0 ... W9968CF_MAX_DEVICES-1] = W9968CF_LIGHTFREQ};
static u8 bandingfilter[] = {[0 ... W9968CF_MAX_DEVICES-1]=
                             W9968CF_BANDINGFILTER};
static s8 clockdiv[] = {[0 ... W9968CF_MAX_DEVICES-1] = W9968CF_CLOCKDIV};
static u8 backlight[] = {[0 ... W9968CF_MAX_DEVICES-1] = W9968CF_BACKLIGHT};
static u8 mirror[] = {[0 ... W9968CF_MAX_DEVICES-1] = W9968CF_MIRROR};
static u8 monochrome[] = {[0 ... W9968CF_MAX_DEVICES-1]=W9968CF_MONOCHROME};
static u16 brightness[] = {[0 ... W9968CF_MAX_DEVICES-1] = W9968CF_BRIGHTNESS};
static u16 hue[] = {[0 ... W9968CF_MAX_DEVICES-1] = W9968CF_HUE};
static u16 colour[] = {[0 ... W9968CF_MAX_DEVICES-1] = W9968CF_COLOUR};
static u16 contrast[] = {[0 ... W9968CF_MAX_DEVICES-1] = W9968CF_CONTRAST};
static u16 whiteness[] = {[0 ... W9968CF_MAX_DEVICES-1] = W9968CF_WHITENESS};
#ifdef W9968CF_DEBUG
static u8 debug = W9968CF_DEBUG_LEVEL;
static u8 specific_debug = W9968CF_SPECIFIC_DEBUG;
#endif

MODULE_PARM(vppmod_load, "b");
MODULE_PARM(simcams, "i");
MODULE_PARM(video_nr, "0-" __MODULE_STRING(W9968CF_MAX_DEVICES) "i");
MODULE_PARM(packet_size, "0-" __MODULE_STRING(W9968CF_MAX_DEVICES) "i");
MODULE_PARM(max_buffers, "0-" __MODULE_STRING(W9968CF_MAX_DEVICES) "i");
MODULE_PARM(double_buffer, "0-" __MODULE_STRING(W9968CF_MAX_DEVICES) "b");
MODULE_PARM(clamping, "0-" __MODULE_STRING(W9968CF_MAX_DEVICES) "b");
MODULE_PARM(filter_type, "0-" __MODULE_STRING(W9968CF_MAX_DEVICES) "i");
MODULE_PARM(largeview, "0-" __MODULE_STRING(W9968CF_MAX_DEVICES) "b");
MODULE_PARM(decompression, "0-" __MODULE_STRING(W9968CF_MAX_DEVICES) "i");
MODULE_PARM(upscaling, "0-" __MODULE_STRING(W9968CF_MAX_DEVICES) "b");
MODULE_PARM(force_palette, "0-" __MODULE_STRING(W9968CF_MAX_DEVICES) "i");
MODULE_PARM(force_rgb, "0-" __MODULE_STRING(W9968CF_MAX_DEVICES) "i");
MODULE_PARM(autobright, "0-" __MODULE_STRING(W9968CF_MAX_DEVICES) "b");
MODULE_PARM(autoexp, "0-" __MODULE_STRING(W9968CF_MAX_DEVICES) "b");
MODULE_PARM(lightfreq, "0-" __MODULE_STRING(W9968CF_MAX_DEVICES) "i");
MODULE_PARM(bandingfilter, "0-" __MODULE_STRING(W9968CF_MAX_DEVICES) "b");
MODULE_PARM(clockdiv, "0-" __MODULE_STRING(W9968CF_MAX_DEVICES) "i");
MODULE_PARM(backlight, "0-" __MODULE_STRING(W9968CF_MAX_DEVICES) "b");
MODULE_PARM(mirror, "0-" __MODULE_STRING(W9968CF_MAX_DEVICES) "b");
MODULE_PARM(monochrome, "0-" __MODULE_STRING(W9968CF_MAX_DEVICES) "b");
MODULE_PARM(brightness, "0-" __MODULE_STRING(W9968CF_MAX_DEVICES) "l");
MODULE_PARM(hue, "0-" __MODULE_STRING(W9968CF_MAX_DEVICES) "l");
MODULE_PARM(colour, "0-" __MODULE_STRING(W9968CF_MAX_DEVICES) "l");
MODULE_PARM(contrast, "0-" __MODULE_STRING(W9968CF_MAX_DEVICES) "l");
MODULE_PARM(whiteness, "0-" __MODULE_STRING(W9968CF_MAX_DEVICES) "l");
#ifdef W9968CF_DEBUG
MODULE_PARM(debug, "i");
MODULE_PARM(specific_debug, "b");
#endif

MODULE_PARM_DESC(vppmod_load, 
                 "\n<0|1> Automatic 'w9968cf-vpp' module loading."
                 "\n0 disable, 1 enable."
                 "\nIf enabled, every time an application attempts to open a"
                 "\ncamera, 'insmod' searches for the video post-processing"
                 "\nmodule in the system and loads it automatically (if"
                 "\npresent). The 'w9968cf-vpp' module adds extra image"
                 "\nmanipulation functions to the 'w9968cf' module, like"
                 "\nsoftware up-scaling,colour conversions and video decoding."
                 "\nDefault value is "__MODULE_STRING(W9968CF_VPPMOD_LOAD)"."
                 "\n");
MODULE_PARM_DESC(simcams, 
                 "\n<n> Number of cameras allowed to stream simultaneously."
                 "\nn may vary from 0 to "
                 __MODULE_STRING(W9968CF_MAX_DEVICES)"."
                 "\nDefault value is "__MODULE_STRING(W9968CF_SIMCAMS)"."
                 "\n");
MODULE_PARM_DESC(video_nr,
                 "\n<-1|n[,...]> Specify V4L minor mode number."
                 "\n -1 = use next available (default)"
                 "\n  n = use minor number n (integer >= 0)"
                 "\nYou can specify " __MODULE_STRING(W9968CF_MAX_DEVICES)
                 " cameras this way."
                 "\nFor example:"
                 "\nvideo_nr=-1,2,-1 would assign minor number 2 to"
                 "\nthe second camera and use auto for the first"
                 "\none and for every other camera."
                 "\n");
MODULE_PARM_DESC(packet_size,
                 "\n<n[,...]> Specify the maximum data payload"
                 "\nsize in bytes for alternate settings, for each device."
                 "\nn is scaled between 63 and 1023 "
                 "(default is "__MODULE_STRING(W9968CF_PACKET_SIZE)")."
                 "\n");
MODULE_PARM_DESC(max_buffers,
                 "\n<n[,...]> For advanced users."
                 "\nSpecify the maximum number of video frame buffers"
                 "\nto allocate for each device, from 2 to "
                 __MODULE_STRING(W9968CF_MAX_BUFFERS)
                 ". (default is "__MODULE_STRING(W9968CF_BUFFERS)")."
                 "\n");
MODULE_PARM_DESC(double_buffer, 
                 "\n<0|1[,...]> "
                 "Hardware double buffering: 0 disabled, 1 enabled."
                 "\nIt should be enabled if you want smooth video output: if"
                 "\nyou obtain out of sync. video, disable it at all, or try"
                 "\nto decrease the 'clockdiv' module paramater value."
                 "\nDefault value is "__MODULE_STRING(W9968CF_DOUBLE_BUFFER)
                 " for every device."
                 "\n");
MODULE_PARM_DESC(clamping, 
                 "\n<0|1[,...]> Video data clamping: 0 disabled, 1 enabled."
                 "\nDefault value is "__MODULE_STRING(W9968CF_CLAMPING)
                 " for every device."
                 "\n");
MODULE_PARM_DESC(filter_type, 
                 "\n<0|1|2[,...]> Video filter type."
                 "\n0 none, 1 (1-2-1) 3-tap filter, "
                 "2 (2-3-6-3-2) 5-tap filter."
                 "\nDefault value is "__MODULE_STRING(W9968CF_FILTER_TYPE)
                 " for every device."
                 "\nThe filter is used to reduce noise and aliasing artifacts"
                 "\nproduced by the CCD or CMOS sensor, and the scaling"
                 " process."
                 "\n");
MODULE_PARM_DESC(largeview, 
                 "\n<0|1[,...]> Large view: 0 disabled, 1 enabled."
                 "\nDefault value is "__MODULE_STRING(W9968CF_LARGEVIEW)
                 " for every device."
                 "\n");
MODULE_PARM_DESC(upscaling, 
                 "\n<0|1[,...]> Software scaling (for non-compressed video):"
                 "\n0 disabled, 1 enabled."
                 "\nDisable it if you have a slow CPU or you don't have"
                 " enough memory."
                 "\nDefault value is "__MODULE_STRING(W9968CF_UPSCALING)
                 " for every device."
                 "\nIf 'w9968cf-vpp' is not loaded, this paramater is"
                 " set to 0."
                 "\n");
MODULE_PARM_DESC(decompression,
                 "\n<0|1|2[,...]> Software video decompression:"
                 "\n- 0 disables decompression (doesn't allow formats needing"
                 " decompression)"
                 "\n- 1 forces decompression (allows formats needing"
                 " decompression only);"
                 "\n- 2 allows any permitted formats."
                 "\nFormats supporting compressed video are YUV422P and"
                 " YUV420P/YUV420 "
                 "\nin any resolutions where both width and height are "
                 "a multiple of 16."
                 "\nDefault value is "__MODULE_STRING(W9968CF_DECOMPRESSION)
                 " for every device."
                 "\nIf 'w9968cf-vpp' is not loaded, forcing decompression is "
                 "\nnot allowed; in this case this paramater is set to 2."
                 "\n");
MODULE_PARM_DESC(force_palette,
                 "\n<0"
                 "|" __MODULE_STRING(VIDEO_PALETTE_UYVY)
                 "|" __MODULE_STRING(VIDEO_PALETTE_YUV420)
                 "|" __MODULE_STRING(VIDEO_PALETTE_YUV422P)
                 "|" __MODULE_STRING(VIDEO_PALETTE_YUV420P)
                 "|" __MODULE_STRING(VIDEO_PALETTE_YUYV)
                 "|" __MODULE_STRING(VIDEO_PALETTE_YUV422)
                 "|" __MODULE_STRING(VIDEO_PALETTE_GREY)
                 "|" __MODULE_STRING(VIDEO_PALETTE_RGB555)
                 "|" __MODULE_STRING(VIDEO_PALETTE_RGB565)
                 "|" __MODULE_STRING(VIDEO_PALETTE_RGB24)
                 "|" __MODULE_STRING(VIDEO_PALETTE_RGB32)
                 "[,...]>"
                 " Force picture palette."
                 "\nIn order:"
                 "\n- 0 allows any of the following formats:"
                 "\n- UYVY    16 bpp - Original video, compression disabled"
                 "\n- YUV420  12 bpp - Original video, compression enabled"
                 "\n- YUV422P 16 bpp - Original video, compression enabled"
                 "\n- YUV420P 12 bpp - Original video, compression enabled"
                 "\n- YUVY    16 bpp - Software conversion from UYVY"
                 "\n- YUV422  16 bpp - Software conversion from UYVY"
                 "\n- GREY     8 bpp - Software conversion from UYVY"
                 "\n- RGB555  16 bpp - Software conversion from UYVY"
                 "\n- RGB565  16 bpp - Software conversion from UYVY"
                 "\n- RGB24   24 bpp - Software conversion from UYVY"
                 "\n- RGB32   32 bpp - Software conversion from UYVY"
                 "\nWhen not 0, this paramater will override 'decompression'."
                 "\nDefault value is 0 for every device."
                 "\nInitial palette is "
                 __MODULE_STRING(W9968CF_PALETTE_DECOMP_ON)"."
                 "\nIf 'w9968cf-vpp' is not loaded, this paramater is"
                 " set to 9 (UYVY)."
                 "\n");
MODULE_PARM_DESC(force_rgb, 
                 "\n<0|1[,...]> Read RGB video data instead of BGR:"
                 "\n 1 = use RGB component ordering."
                 "\n 0 = use BGR component ordering."
                 "\nThis parameter has effect when using RGBX palettes only."
                 "\nDefault value is "__MODULE_STRING(W9968CF_FORCE_RGB)
                 " for every device."
                 "\n");
MODULE_PARM_DESC(autobright,
                 "\n<0|1[,...]> CMOS sensor automatically changes brightness:"
                 "\n 0 = no, 1 = yes"
                 "\nDefault value is "__MODULE_STRING(W9968CF_AUTOBRIGHT)
                 " for every device."
                 "\n");
MODULE_PARM_DESC(autoexp,
                 "\n<0|1[,...]> CMOS sensor automatically changes exposure:"
                 "\n 0 = no, 1 = yes"
                 "\nDefault value is "__MODULE_STRING(W9968CF_AUTOEXP)
                 " for every device."
                 "\n");
MODULE_PARM_DESC(lightfreq,
                 "\n<50|60[,...]> Light frequency in Hz:"
                 "\n 50 for European and Asian lighting,"
                 " 60 for American lighting."
                 "\nDefault value is "__MODULE_STRING(W9968CF_LIGHTFREQ)
                 " for every device."
                 "\n");
MODULE_PARM_DESC(bandingfilter,
                 "\n<0|1[,...]> Banding filter to reduce effects of"
                 " fluorescent lighting:"
                 "\n 0 disabled, 1 enabled."
                 "\nThis filter tries to reduce the pattern of horizontal"
                 "\nlight/dark bands caused by some (usually fluorescent)"
                 " lighting."
                 "\nDefault value is "__MODULE_STRING(W9968CF_BANDINGFILTER)
                 " for every device."
                 "\n");
MODULE_PARM_DESC(clockdiv,
                 "\n<-1|n[,...]> "
                 "Force pixel clock divisor to a specific value (for experts):"
                 "\n  n may vary from 0 to 127."
                 "\n -1 for automatic value."
                 "\nSee also the 'double_buffer' module paramater."
                 "\nDefault value is "__MODULE_STRING(W9968CF_CLOCKDIV)
                 " for every device."
                 "\n");
MODULE_PARM_DESC(backlight,
                 "\n<0|1[,...]> Objects are lit from behind:"
                 "\n 0 = no, 1 = yes"
                 "\nDefault value is "__MODULE_STRING(W9968CF_BACKLIGHT)
                 " for every device."
                 "\n");
MODULE_PARM_DESC(mirror,
                 "\n<0|1[,...]> Reverse image horizontally:"
                 "\n 0 = no, 1 = yes"
                 "\nDefault value is "__MODULE_STRING(W9968CF_MIRROR)
                 " for every device."
                 "\n");
MODULE_PARM_DESC(monochrome,
                 "\n<0|1[,...]> Use OV CMOS sensor as monochrome sensor:"
                 "\n 0 = no, 1 = yes"
                 "\nNot all the sensors support monochrome color."
                 "\nDefault value is "__MODULE_STRING(W9968CF_MONOCHROME)
                 " for every device."
                 "\n");
MODULE_PARM_DESC(brightness, 
                 "\n<n[,...]> Set picture brightness (0-65535)."
                 "\nDefault value is "__MODULE_STRING(W9968CF_BRIGHTNESS)
                 " for every device."
                 "\nThis parameter has no effect if 'autobright' is enabled."
                 "\n");
MODULE_PARM_DESC(hue, 
                 "\n<n[,...]> Set picture hue (0-65535)."
                 "\nDefault value is "__MODULE_STRING(W9968CF_HUE)
                 " for every device."
                 "\n");
MODULE_PARM_DESC(colour, 
                 "\n<n[,...]> Set picture saturation (0-65535)."
                 "\nDefault value is "__MODULE_STRING(W9968CF_COLOUR)
                 " for every device."
                 "\n");
MODULE_PARM_DESC(contrast, 
                 "\n<n[,...]> Set picture contrast (0-65535)."
                 "\nDefault value is "__MODULE_STRING(W9968CF_CONTRAST)
                 " for every device."
                 "\n");
MODULE_PARM_DESC(whiteness, 
                 "\n<n[,...]> Set picture whiteness (0-65535)."
                 "\nDefault value is "__MODULE_STRING(W9968CF_WHITENESS)
                 " for every device."
                 "\n");
#ifdef W9968CF_DEBUG
MODULE_PARM_DESC(debug,
                 "\n<n> Debugging information level, from 0 to 6:"
                 "\n0 = none (use carefully)"
                 "\n1 = critical errors"
                 "\n2 = significant informations"
                 "\n3 = configuration or general messages"
                 "\n4 = warnings"
                 "\n5 = called functions"
                 "\n6 = function internals"
                 "\nLevel 5 and 6 are useful for testing only, when just "
                 "one device is used."
                 "\nDefault value is "__MODULE_STRING(W9968CF_DEBUG_LEVEL)"."
                 "\n");
MODULE_PARM_DESC(specific_debug,
                 "\n<0|1> Enable or disable specific debugging messages:"
                 "\n0 = print messages concerning every level"
                 " <= 'debug' level."
                 "\n1 = print messages concerning the level"
                 " indicated by 'debug'."
                 "\nDefault value is "
                 __MODULE_STRING(W9968CF_SPECIFIC_DEBUG)"."
                 "\n");
#endif /* W9968CF_DEBUG */



/****************************************************************************
 * Some prototypes                                                          *
 ****************************************************************************/

/* Video4linux interface */
static struct file_operations w9968cf_fops;
static int w9968cf_open(struct inode*, struct file*);
static int w9968cf_release(struct inode*, struct file*);
static ssize_t w9968cf_read(struct file*, char*, size_t, loff_t*);
static int w9968cf_mmap(struct file*, struct vm_area_struct*);
static int w9968cf_ioctl(struct inode*, struct file*, unsigned, unsigned long);
static int w9968cf_v4l_ioctl(struct inode*, struct file*, unsigned int, void*);

#if defined(CONFIG_VIDEO_PROC_FS)
/* /proc interface */
static void w9968cf_proc_create(void);
static void w9968cf_proc_destroy(void);
static void w9968cf_proc_create_dev(struct w9968cf_device*);
static void w9968cf_proc_destroy_dev(struct w9968cf_device*);
static int w9968cf_proc_read_global(char*, char**, off_t, int, int*, void*);
static int w9968cf_proc_read_dev(char*, char**, off_t, int, int*, void*);
#endif

/* USB-specific */
static int w9968cf_start_transfer(struct w9968cf_device*);
static int w9968cf_stop_transfer(struct w9968cf_device*);
static void w9968cf_urb_complete(struct urb *urb);
static int w9968cf_write_reg(struct w9968cf_device*, u16 value, u16 index);
static int w9968cf_read_reg(struct w9968cf_device*, u16 index);
static int w9968cf_write_fsb(struct w9968cf_device*, u16* data);
static int w9968cf_write_sb(struct w9968cf_device*, u16 value);
static int w9968cf_read_sb(struct w9968cf_device*);
static int w9968cf_upload_quantizationtables(struct w9968cf_device*);

/* Low-level I2C (SMBus) I/O */
static int w9968cf_smbus_start(struct w9968cf_device*);
static int w9968cf_smbus_stop(struct w9968cf_device*);
static int w9968cf_smbus_write_byte(struct w9968cf_device*, u8 v);
static int w9968cf_smbus_read_byte(struct w9968cf_device*, u8* v);
static int w9968cf_smbus_write_ack(struct w9968cf_device*);
static int w9968cf_smbus_read_ack(struct w9968cf_device*);
static int w9968cf_smbus_refresh_bus(struct w9968cf_device*);
static int w9968cf_i2c_adap_read_byte(struct w9968cf_device* cam,
                                      u16 address, u8* value);
static int w9968cf_i2c_adap_read_byte_data(struct w9968cf_device*, u16 address, 
                                           u8 subaddress, u8* value);
static int w9968cf_i2c_adap_write_byte(struct w9968cf_device*,
                                       u16 address, u8 subaddress);
static int w9968cf_i2c_adap_fastwrite_byte_data(struct w9968cf_device*,
                                                u16 address, u8 subaddress,
                                                u8 value);

/* I2C interface to kernel */
static int w9968cf_i2c_init(struct w9968cf_device*);
static int w9968cf_i2c_smbus_xfer(struct i2c_adapter*, u16 addr, 
                                  unsigned short flags, char read_write, 
                                  u8 command, int size, union i2c_smbus_data*);
static u32 w9968cf_i2c_func(struct i2c_adapter*);
static int w9968cf_i2c_attach_inform(struct i2c_client*);
static int w9968cf_i2c_detach_inform(struct i2c_client*);
static int w9968cf_i2c_control(struct i2c_adapter*, unsigned int cmd,
                               unsigned long arg);
static void w9968cf_i2c_inc_use(struct i2c_adapter*);
static void w9968cf_i2c_dec_use(struct i2c_adapter*);

/* Memory management */
static inline unsigned long kvirt_to_pa(unsigned long adr);
static void* rvmalloc(unsigned long size);
static void rvfree(void *mem, unsigned long size);
static void w9968cf_deallocate_memory(struct w9968cf_device*);
static int  w9968cf_allocate_memory(struct w9968cf_device*);
static inline unsigned long w9968cf_get_max_bufsize(struct w9968cf_device*);

/* High-level CMOS sensor control functions */
static int w9968cf_sensor_set_control(struct w9968cf_device*,int cid,int val);
static int w9968cf_sensor_get_control(struct w9968cf_device*,int cid,int *val);
static inline int w9968cf_sensor_cmd(struct w9968cf_device*, 
                                     unsigned int cmd, void *arg);
static int w9968cf_sensor_init(struct w9968cf_device*);
static int w9968cf_sensor_update_settings(struct w9968cf_device*);
static int w9968cf_sensor_get_picture(struct w9968cf_device*);
static int w9968cf_sensor_update_picture(struct w9968cf_device*, 
                                         struct video_picture pict);

/* Other helper functions */
static void w9968cf_configure_camera(struct w9968cf_device*,struct usb_device*,
                                     enum w9968cf_model_id, 
                                     const unsigned short dev_nr);
static int w9968cf_turn_on_led(struct w9968cf_device*);
static int w9968cf_init_chip(struct w9968cf_device*);
static int w9968cf_set_picture(struct w9968cf_device*, struct video_picture);
static int w9968cf_set_window(struct w9968cf_device*, struct video_window);
static inline u16 w9968cf_valid_palette(u16 palette);
static inline u16 w9968cf_valid_depth(u16 palette);
static inline u8 w9968cf_need_decompression(u16 palette);
static int w9968cf_postprocess_frame(struct w9968cf_device*, 
                                     struct w9968cf_frame_t*);
static int w9968cf_adjust_window_size(struct w9968cf_device*, u16* w, u16* h);
static void w9968cf_init_framelist(struct w9968cf_device*);
static void w9968cf_push_frame(struct w9968cf_device*, u8 f_num);
static void w9968cf_pop_frame(struct w9968cf_device*,struct w9968cf_frame_t**);
static void w9968cf_release_resources(struct w9968cf_device*);

/* Intermodule communication */
static int w9968cf_vppmod_detect(void);
static void w9968cf_vppmod_release(void);

/* Pointers to registered video post-processing functions */
static void (*w9968cf_vpp_init_decoder)(void);
static int (*w9968cf_vpp_check_headers)(const unsigned char*,
                                        const unsigned long);
static int (*w9968cf_vpp_decode)(const char*, const unsigned, 
                                 const unsigned, const unsigned, char*);
static void (*w9968cf_vpp_swap_yuvbytes)(void*, unsigned long);
static void (*w9968cf_vpp_uyvy_to_rgbx)(u8*, unsigned long, u8*, u16, u8);
static void (*w9968cf_vpp_scale_up)(u8*, u8*, u16, u16, u16, u16, u16);



/****************************************************************************
 * Symbolic names                                                           *
 ****************************************************************************/

/* Used to represent a list of values and their respective symbolic names */
struct w9968cf_symbolic_list {
	const int num;
	const char *name;
};

/*-------------------------------------------------------------------------- 
  Returns the name of the matching element in the symbolic_list array. The
  end of the list must be marked with an element that has a NULL name.
  --------------------------------------------------------------------------*/
static inline const char * 
symbolic(struct w9968cf_symbolic_list list[], const int num)
{
	int i;

	for (i = 0; list[i].name != NULL; i++)
		if (list[i].num == num)
			return (list[i].name);

	return "Unknown";
}

static struct w9968cf_symbolic_list camlist[] = {
	{ W9968CF_MOD_GENERIC, "W996[87]CF JPEG USB Dual Mode Camera" },
	{ W9968CF_MOD_CLVBWGP, "Creative Labs Video Blaster WebCam Go Plus" },

	/* Other cameras (having the same descriptors as Generic W996[87]CF) */
	{ W9968CF_MOD_ADPA5R, "Aroma Digi Pen ADG-5000 Refurbished" },
	{ W9986CF_MOD_AU, "AVerTV USB" },
	{ W9968CF_MOD_CLVBWG, "Creative Labs Video Blaster WebCam Go" },
	{ W9968CF_MOD_DLLDK, "Die Lebon LDC-D35A Digital Kamera" },
	{ W9968CF_MOD_EEEMC, "Ezonics EZ-802 EZMega Cam" },
	{ W9968CF_MOD_ODPVDMPC, "OPCOM Digi Pen VGA Dual Mode Pen Camera" },

	{  -1, NULL }
};

static struct w9968cf_symbolic_list senlist[] = {
	{ CC_OV76BE,   "OV76BE" },
	{ CC_OV7610,   "OV7610" },
	{ CC_OV7620,   "OV7620" },
	{ CC_OV7620AE, "OV7620AE" },
	{ CC_OV6620,   "OV6620" },
	{ CC_OV6630,   "OV6630" },
	{ CC_OV6630AE, "OV6630AE" },
	{ CC_OV6630AF, "OV6630AF" },
	{ -1, NULL }
};

/* Video4Linux1 palettes */
static struct w9968cf_symbolic_list v4l1_plist[] = {
	{ VIDEO_PALETTE_GREY,    "GREY" },
	{ VIDEO_PALETTE_HI240,   "HI240" },
	{ VIDEO_PALETTE_RGB565,  "RGB565" },
	{ VIDEO_PALETTE_RGB24,   "RGB24" },
	{ VIDEO_PALETTE_RGB32,   "RGB32" },
	{ VIDEO_PALETTE_RGB555,  "RGB555" },
	{ VIDEO_PALETTE_YUV422,  "YUV422" },
	{ VIDEO_PALETTE_YUYV,    "YUYV" },
	{ VIDEO_PALETTE_UYVY,    "UYVY" },
	{ VIDEO_PALETTE_YUV420,  "YUV420" },
	{ VIDEO_PALETTE_YUV411,  "YUV411" },
	{ VIDEO_PALETTE_RAW,     "RAW" },
	{ VIDEO_PALETTE_YUV422P, "YUV422P" },
	{ VIDEO_PALETTE_YUV411P, "YUV411P" },
	{ VIDEO_PALETTE_YUV420P, "YUV420P" },
	{ VIDEO_PALETTE_YUV410P, "YUV410P" },
	{ -1, NULL }
};

/* Decoder error codes: */
static struct w9968cf_symbolic_list decoder_errlist[] = {
	{ W9968CF_DEC_ERR_CORRUPTED_DATA, "Corrupted data" },
	{ W9968CF_DEC_ERR_BUF_OVERFLOW,   "Buffer overflow" },
	{ W9968CF_DEC_ERR_NO_SOI,         "SOI marker not found" },     
	{ W9968CF_DEC_ERR_NO_SOF0,        "SOF0 marker not found" },
	{ W9968CF_DEC_ERR_NO_SOS,         "SOS marker not found" },
	{ W9968CF_DEC_ERR_NO_EOI,         "EOI marker not found" },
	{ -1, NULL }
};

/* URB error codes: */
static struct w9968cf_symbolic_list urb_errlist[] = {
	{ -ENOMEM,    "No memory for allocation of internal structures" },
	{ -ENOSPC,    "The host controller's bandwidth is already consumed" },
	{ -ENOENT,    "URB was canceled by unlink_urb" },
	{ -EXDEV,     "ISO transfer only partially completed" },
	{ -EAGAIN,    "Too match scheduled for the future" },
	{ -ENXIO,     "URB already queued" },
	{ -EFBIG,     "Too much ISO frames requested" },
	{ -ENOSR,     "Buffer error (overrun)" },
	{ -EPIPE,     "Specified endpoint is stalled (device not responding)"},
	{ -EOVERFLOW, "Babble (bad cable?)" },
	{ -EPROTO,    "Bit-stuff error (bad cable?)" },
	{ -EILSEQ,    "CRC/Timeout" },
	{ -ETIMEDOUT, "NAK (device does not respond)" },
	{ -1, NULL }
};



/****************************************************************************
 * Memory management functions                                              *
 ****************************************************************************/

/* Here we want the physical address of the memory.
   This is used when initializing the contents of the area. */
static inline unsigned long kvirt_to_pa(unsigned long adr)
{
	unsigned long kva, ret;

	kva = (unsigned long) page_address(vmalloc_to_page((void *)adr));
	kva |= adr & (PAGE_SIZE-1); /* restore the offset */
	ret = __pa(kva);
	return ret;
}


static void* rvmalloc(unsigned long size)
{
	void* mem;
	unsigned long adr;

	size = PAGE_ALIGN(size);
	mem = vmalloc_32(size);
	if (!mem)
		return NULL;

	memset(mem, 0, size); /* Clear the ram out, no junk to the user */
	adr = (unsigned long) mem;
	while (size > 0) {
		mem_map_reserve(vmalloc_to_page((void *)adr));
		adr += PAGE_SIZE;
		size -= PAGE_SIZE;
	}

	return mem;
}


static void rvfree(void* mem, unsigned long size)
{
	unsigned long adr;

	if (!mem)
		return;

	adr = (unsigned long) mem;
	while ((long) size > 0) {
		mem_map_unreserve(vmalloc_to_page((void *)adr));
		adr += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
	vfree(mem);
}


/*--------------------------------------------------------------------------
  Return the maximum size (in bytes) of a frame buffer.
  --------------------------------------------------------------------------*/
static inline unsigned long w9968cf_get_max_bufsize(struct w9968cf_device* cam)
{
	u8 bpp = (w9968cf_vppmod_present) ? 4 : 2;
	return (cam->upscaling) ? W9968CF_MAX_WIDTH*W9968CF_MAX_HEIGHT*bpp :
	                          cam->maxwidth*cam->maxheight*bpp;
}


/*--------------------------------------------------------------------------
  Deallocate previously allocated memory.
  --------------------------------------------------------------------------*/
static void w9968cf_deallocate_memory(struct w9968cf_device* cam)
{
	u8 i;

	/* Free the isochronous transfer buffers */
	for (i = 0; i < W9968CF_URBS; i++) {
		kfree(cam->transfer_buffer[i]);
		cam->transfer_buffer[i] = NULL;
	}

	/* Free temporary frame buffer */
	if (cam->frame_tmp.buffer) {
		rvfree(cam->frame_tmp.buffer, W9968CF_HW_BUF_SIZE);
		cam->frame_tmp.buffer = NULL;
	}

	/* Free helper buffer */
	if (cam->vpp_buffer) {
		rvfree(cam->vpp_buffer, w9968cf_get_max_bufsize(cam));
		cam->vpp_buffer = NULL;
	}
	
	/* Free video frame buffers */
	if (cam->frame[0].buffer) {
		rvfree(cam->frame[0].buffer, 
		       cam->nbuffers * w9968cf_get_max_bufsize(cam));
		cam->frame[0].buffer = NULL;
	}

	cam->nbuffers = 0;

	DBG(5, "Memory successfully deallocated.")
}


/*--------------------------------------------------------------------------
  Allocate memory buffers for USB transfers and video frames.
  This function is called by open() only.
  Return 0 on success, a negative number otherwise.
  --------------------------------------------------------------------------*/
static int w9968cf_allocate_memory(struct w9968cf_device* cam)
{
	const unsigned long bufsize = w9968cf_get_max_bufsize(cam);
	const u16 p_size = wMaxPacketSize[cam->altsetting-1];
	void* buff = NULL;
	u8 i;

	/* NOTE: Deallocation is done elsewhere in case of error */

	/* Allocate memory for the isochronous transfer buffers */
	for (i = 0; i < W9968CF_URBS; i++) {
		if (!(cam->transfer_buffer[i] =
		      kmalloc(W9968CF_ISO_PACKETS*p_size, GFP_KERNEL))) {
			DBG(1, "Couldn't allocate memory for the isochronous "
			       "transfer buffers (%d bytes).", 
			    p_size * W9968CF_ISO_PACKETS)
			return -ENOMEM;
		}
		memset(cam->transfer_buffer[i], 0, W9968CF_ISO_PACKETS*p_size);
	}

	/* Allocate memory for the temporary frame buffer */
	if (!(cam->frame_tmp.buffer = rvmalloc(W9968CF_HW_BUF_SIZE))) {
		DBG(1, "Couldn't allocate memory for the temporary "
		       "video frame buffer (%i bytes).", W9968CF_HW_BUF_SIZE)
		return -ENOMEM;
	}

	/* Allocate memory for the helper buffer */
	if (w9968cf_vppmod_present) {
		if (!(cam->vpp_buffer = rvmalloc(bufsize))) {
			DBG(1, "Couldn't allocate memory for the helper buffer"
			       " (%li bytes).", bufsize)
			return -ENOMEM;
		}
	} else
		cam->vpp_buffer = NULL;

	/* Allocate memory for video frame buffers */	
	cam->nbuffers = cam->max_buffers;
	while (cam->nbuffers >= 2) {
		if ((buff = rvmalloc(cam->nbuffers * bufsize)))
			break;
		else
			cam->nbuffers--;
	}

	if (!buff) {
		DBG(1, "Couldn't allocate memory for the video frame buffers.")
		cam->nbuffers = 0;
		return -ENOMEM;
	}

	if (cam->nbuffers != cam->max_buffers)
		DBG(2, "Couldn't allocate memory for %d video frame buffers. "
		       "Only memory for %d buffers has been allocated.",
		    cam->max_buffers, cam->nbuffers)

	for (i = 0; i < cam->nbuffers; i++) {
		cam->frame[i].buffer = buff + i*bufsize;
		/* Circular list */
		if (i != cam->nbuffers-1)
			cam->frame[i].next = &cam->frame[i+1];
		else
			cam->frame[i].next = &cam->frame[0];
		cam->frame[i].status = F_UNUSED;
	}

	DBG(5, "Memory successfully allocated.")
	return 0;
}



/****************************************************************************
 * /proc interface                                                          *
 ****************************************************************************/

#if defined(CONFIG_VIDEO_PROC_FS)

static struct proc_dir_entry* w9968cf_proc_entry, * w9968cf_proc_entry_global;
extern struct proc_dir_entry* video_proc_entry;

#define YES_NO(x) ((x) ? "yes" : "no")

/*--------------------------------------------------------------------------
  Read /proc/video/w9968cf/global 
  --------------------------------------------------------------------------*/
static int 
w9968cf_proc_read_global(char* page, char** start, off_t offset,
                         int count, int* eof, void* data)
{
	struct list_head *list = (struct list_head*)data, *ptr;
	struct w9968cf_device* cam;
	char* out = page;
	int len;
	u8 ncams = 0;

	if (down_interruptible(&w9968cf_devlist_sem))
		return -ERESTARTSYS;

	/* How many connected cameras ? */
	list_for_each(ptr, list)
		ncams++;

	out += sprintf(out,"driver_name       : %s\n", W9968CF_MODULE_NAME);
	out += sprintf(out,"driver_version    : %s\n", W9968CF_MODULE_VERSION);
	out += sprintf(out,"driver_author     : %s\n", W9968CF_MODULE_AUTHOR);
	out += sprintf(out,"author_email      : %s\n", W9968CF_AUTHOR_EMAIL);
	out += sprintf(out,"vpp_w9968cf_module: %s\n",
	               w9968cf_vppmod_present ? "detected" : "not detected");
	out += sprintf(out,"max_allowed_cams  : %d\n", simcams);
	out += sprintf(out,"registered_cameras: %d\n", ncams);

	list_for_each(ptr, list) {
		cam = list_entry(ptr, struct w9968cf_device, v4llist);
		out += sprintf(out,"/dev/video%d       : %s\n", 
		               cam->v4ldev->minor, symbolic(camlist, cam->id));
	}

	up(&w9968cf_devlist_sem);

	len = out - page;
	len -= offset;
	if (len < count) {
		*eof = 1;
		if (len <= 0)
			return 0;
	} else
		len = count;

	*start = page + offset;

	return len;
}


/*--------------------------------------------------------------------------
  Read /proc/video/w9968cf/dev<minor#>
  --------------------------------------------------------------------------*/
static int 
w9968cf_proc_read_dev(char* page, char** start, off_t offset,
                      int count, int* eof, void* data)
{
	struct w9968cf_device* cam = (struct w9968cf_device* )data;
	int len;
	char* out = page;

	if (down_interruptible(&cam->procfs_sem))
		return -ERESTARTSYS;

	if (offset == 0)
		w9968cf_sensor_get_picture(cam);

	out += sprintf(out,"camera_model      : %s\n",
	               symbolic(camlist, cam->id));
	out += sprintf(out,"sensor_model      : %s\n",
	               symbolic(senlist, cam->sensor));
	out += sprintf(out,"sensor_monochrome : %s\n",
	               YES_NO(cam->monochrome));
	if (cam->users)
		out += sprintf(out,"user_program      : %s\n",cam->command);
	out += sprintf(out,"packet_size_bytes : %d\n", 
	               wMaxPacketSize[cam->altsetting-1]);
	out += sprintf(out,"allocated_buffers : %d\n",cam->nbuffers);
	out += sprintf(out,"streaming         : %s\n",YES_NO(cam->streaming));
	out += sprintf(out,"compression       : %s\n",
	               YES_NO(cam->vpp_flag & VPP_DECOMPRESSION));
	out += sprintf(out,"uyvy_to_rgbx      : %s\n",
	               YES_NO(cam->vpp_flag & VPP_UYVY_TO_RGBX));
	out += sprintf(out,"uv-y_reordering   : %s\n", 
	               YES_NO(cam->vpp_flag & VPP_SWAP_YUV_BYTES));
	out += sprintf(out,"software_upscaling: %s\n",
	               YES_NO(cam->vpp_flag & VPP_UPSCALE));
	out += sprintf(out,"clock_divisor     : %d\n",cam->clockdiv);
	out += sprintf(out,"palette           : %s\n", 
	               symbolic(v4l1_plist, cam->picture.palette));
	out += sprintf(out,"depth_bpp         : %d\n",cam->picture.depth);
	out += sprintf(out,"x_window_offset   : %d\n",cam->window.x);
	out += sprintf(out,"y_window_offset   : %d\n",cam->window.y);	
	out += sprintf(out,"width             : %d\n",cam->window.width);
	out += sprintf(out,"height            : %d\n",cam->window.height);
	out += sprintf(out,"max_buffers       : %d\n",cam->max_buffers);
	out += sprintf(out,"decompression     : %s\n",
	               (!cam->decompression)? "off" :
	               ((cam->decompression == 1) ? "force" : "on"));
	out += sprintf(out,"force_rgb         : %s\n",YES_NO(cam->force_rgb));
	out += sprintf(out,"video_clamping    : %s\n",YES_NO(cam->clamping));
	out += sprintf(out,"hw_double_buffer  : %s\n",
	               YES_NO(cam->double_buffer));
	out += sprintf(out,"filter_type       : %s\n",
	               (!cam->filter_type) ? "none" : 
	               ((cam->filter_type == 1) ? "3-tap" : "5-tap"));
	out += sprintf(out,"large_view        : %s\n",YES_NO(cam->largeview));
	out += sprintf(out,"auto_brightness   : %s\n",YES_NO(cam->auto_brt));
	out += sprintf(out,"auto_exposure     : %s\n",YES_NO(cam->auto_exp));
	out += sprintf(out,"light_frequency   : %d\n",cam->lightfreq);
	out += sprintf(out,"banding_filter    : %s\n",YES_NO(cam->bandfilt));
	out += sprintf(out,"back_light        : %s\n",YES_NO(cam->backlight));
	out += sprintf(out,"mirror            : %s\n",YES_NO(cam->mirror));
	out += sprintf(out,"brightness        : %d\n",cam->picture.brightness);
	out += sprintf(out,"colour            : %d\n",cam->picture.colour);
	out += sprintf(out,"contrast          : %d\n",cam->picture.contrast);
	out += sprintf(out,"hue               : %d\n",cam->picture.hue);
	out += sprintf(out,"whiteness         : %d\n",cam->picture.whiteness);
	out += sprintf(out,"hs_polarity       : %d\n",cam->hs_polarity);
	out += sprintf(out,"vs_polarity       : %d\n",cam->vs_polarity);
#ifdef W9968CF_DEBUG
	out += sprintf(out,"debug_level       : %d\n",debug);
	out += sprintf(out,"specific_debug    : %s\n",YES_NO(specific_debug));
#endif

	len = out - page;
	len -= offset;
	if (len < count) {
		*eof = 1;
		if (len <= 0) {
			up(&cam->procfs_sem);
			return 0;
		}
	} else
		len = count;

	*start = page + offset;

	up(&cam->procfs_sem);
	return len;
}


static void w9968cf_proc_create_dev(struct w9968cf_device* cam)
{
	char name[6];

	if (!w9968cf_proc_entry)
		return;

	/* Create per-device readable entry */
	sprintf(name, "dev%d", cam->v4ldev->minor);
	cam->proc_dev = create_proc_read_entry(name, S_IFREG|S_IRUGO|S_IWUSR,
	                                       w9968cf_proc_entry,
	                                       w9968cf_proc_read_dev,
	                                       (void*)cam);
	if (!cam->proc_dev)
		return;
	cam->proc_dev->owner = THIS_MODULE;

	DBG(2, "Per-device entry /proc/video/w9968cf/dev%d created.",
	    cam->v4ldev->minor)
}


static void w9968cf_proc_destroy_dev(struct w9968cf_device* cam)
{
	char name[6];

	if (!cam->proc_dev)
		return;

	sprintf(name, "dev%d", cam->v4ldev->minor);

	/* Destroy per-device entry */
	remove_proc_entry(name, w9968cf_proc_entry);

	DBG(2, "Per-device entry /proc/video/w9968cf/dev%d removed.",
	    cam->v4ldev->minor)
}


static void w9968cf_proc_create(void)
{
	if (!video_proc_entry) {
		DBG(2, "Error: /proc/video/ does not exist.")
		return;
	}

	w9968cf_proc_entry = create_proc_entry("w9968cf", S_IFDIR,
	                                       video_proc_entry);
	if (!w9968cf_proc_entry) {
		DBG(2, "Unable to create /proc/video/w9968cf/")
		return;
	}
	w9968cf_proc_entry->owner = THIS_MODULE;

	w9968cf_proc_entry_global = create_proc_read_entry("global", 
	                                             S_IFREG|S_IRUGO|S_IWUSR,
	                                             w9968cf_proc_entry,
	                                             w9968cf_proc_read_global, 
	                                             (void*)&w9968cf_dev_list);
	if (w9968cf_proc_entry_global)
		w9968cf_proc_entry_global->owner = THIS_MODULE;
	else
		DBG(2, "Unable to create /proc/video/w9968cf/global")

	DBG(2, "Main entry /proc/video/w9968cf/global created.")
}


static void w9968cf_proc_destroy(void)
{
	if (!w9968cf_proc_entry)
		return;

	if (w9968cf_proc_entry_global)
		remove_proc_entry("global", w9968cf_proc_entry);

	remove_proc_entry("w9968cf", video_proc_entry);

	DBG(2, "Main entry /proc/video/w9968cf/global removed.")
}

#endif /* CONFIG_VIDEO_PROC_FS */



/****************************************************************************
 * USB-specific functions                                                   *
 ****************************************************************************/

/*--------------------------------------------------------------------------
  This is an handler function which is called after the URBs are completed.
  It collects multiple data packets coming from the camera by putting them
  into frame buffers: one or more zero data length data packets are used to
  mark the end of a video frame; the first non-zero data packet is the start
  of the next video frame; if an error is encountered in a packet, the entire
  video frame is discarded and grabbed again.
  If there are no requested frames in the FIFO list, packets are collected into
  a temporary buffer. 
  --------------------------------------------------------------------------*/
static void w9968cf_urb_complete(struct urb *urb)
{
	struct w9968cf_device* cam = (struct w9968cf_device*)urb->context;
	struct w9968cf_frame_t** f;
	unsigned long maxbufsize;
	unsigned int len, status;
	void* pos;
	u8 i;
	int err = 0;

	if ((!cam->streaming) || cam->disconnected) {
		DBG(4, "Got interrupt, but not streaming.")
		return;
	}

	maxbufsize = min( (unsigned long)W9968CF_HW_BUF_SIZE, 
	                  w9968cf_get_max_bufsize(cam) );

	/* "(*f)" will be used instead of "cam->frame_current" */
	f = &cam->frame_current;

	/* If a frame has been requested and we are grabbing into  
	   the temporary frame, we'll switch to that requested frame */
	if ((*f) == &cam->frame_tmp && *cam->requested_frame) {
		if (cam->frame_tmp.status == F_GRABBING) {
			w9968cf_pop_frame(cam, &cam->frame_current);
			(*f)->status = F_GRABBING;
			(*f)->length = cam->frame_tmp.length;
			memcpy((*f)->buffer, cam->frame_tmp.buffer,
			       (*f)->length);
			DBG(6, "Switched from temp. frame to frame #%d", 
			    (*f) - &cam->frame[0])
		}
	}

	for (i = 0; i < urb->number_of_packets; i++) {
		len    = urb->iso_frame_desc[i].actual_length;
		status = urb->iso_frame_desc[i].status;
		pos    = urb->iso_frame_desc[i].offset + urb->transfer_buffer;

		if (status && len != 0) {
			DBG(4, "URB failed, error in data packet "
			       "(error #%d, %s).",
			    status, symbolic(urb_errlist, status))
			(*f)->status = F_ERROR;
			continue;
		}

		if (len) { /* start of frame */

			if ((*f)->status == F_UNUSED) {
				(*f)->status = F_GRABBING;
				(*f)->length = 0;
			}

			/* Buffer overflows shouldn't happen, however...*/
			if ((*f)->length + len > maxbufsize) {
				DBG(4, "Buffer overflow: bad data packets.")
				(*f)->status = F_ERROR;
			}

			if ((*f)->status == F_GRABBING) {
				memcpy((*f)->buffer + (*f)->length, pos, len);
				(*f)->length += len;
			}

		} else if ((*f)->status == F_GRABBING) { /* end of frame */

			DBG(6, "Frame #%d successfully grabbed.",
			    ((*f)==&cam->frame_tmp ? -1 : (*f)-&cam->frame[0]))

			if (cam->vpp_flag & VPP_DECOMPRESSION) {
				err=(*w9968cf_vpp_check_headers)((*f)->buffer,
				                                 (*f)->length);
				if (err) {
					DBG(4, "Skip corrupted frame: %s",
					    symbolic(decoder_errlist, err))
					(*f)->status = F_UNUSED;
					continue; /* grab this frame again */
				}
			}

			(*f)->status = F_READY;
			(*f)->queued = 0;

			/* Take a pointer to the new frame from the FIFO list.
			   If the list is empty,we'll use the temporary frame*/
			if (*cam->requested_frame)
				w9968cf_pop_frame(cam, &cam->frame_current);
			else {
				cam->frame_current = &cam->frame_tmp;
				(*f)->status = F_UNUSED;
			}

		} else if ((*f)->status == F_ERROR)
			(*f)->status = F_UNUSED; /* grab it again */

		PDBGG("Frame length %li | pack.#%d | pack.len. %d | state %d",
		      (unsigned long)(*f)->length, i, len, (*f)->status)

	} /* end for */

	/* Resubmit this URB */
	urb->dev = cam->usbdev;
	urb->status = 0;
	spin_lock(&cam->urb_lock);
	if (cam->streaming)
		if ((err = usb_submit_urb(urb))) {
			cam->misconfigured = 1;
			DBG(1, "Couldn't resubmit the URB: error %d, %s",
			    err, symbolic(urb_errlist, err));
		}
	spin_unlock(&cam->urb_lock);

	/* Wake up the user process */
	if (waitqueue_active(&cam->wait_queue))
		wake_up_interruptible(&cam->wait_queue);
}


/*---------------------------------------------------------------------------
  Setup the URB structures for the isochronous transfer.
  Submit the URBs so that the data transfer begins.
  Return 0 on success, a negative number otherwise.
  ---------------------------------------------------------------------------*/
static int w9968cf_start_transfer(struct w9968cf_device* cam)
{
	struct usb_device *udev = cam->usbdev;
	struct urb* urb;
	const u16 p_size = wMaxPacketSize[cam->altsetting-1];
	u16 w, h, d;
	int vidcapt;
	u32 t_size;
	int err = 0;
	s8 i, j;

	for (i = 0; i < W9968CF_URBS; i++) {
		urb = usb_alloc_urb(W9968CF_ISO_PACKETS);
		cam->urb[i] = urb;
		if (!urb) {
			for (j = 0; j < i; j++)
				usb_free_urb(cam->urb[j]);
			DBG(1, "Couldn't allocate the URB structures.")
			return -ENOMEM;
		}
		urb->dev = udev;
		urb->context = (void*)cam;
		urb->pipe = usb_rcvisocpipe(udev, 1);
		urb->transfer_flags = USB_ISO_ASAP;
		urb->number_of_packets = W9968CF_ISO_PACKETS;
		urb->complete = w9968cf_urb_complete;
		urb->transfer_buffer = cam->transfer_buffer[i];
		urb->transfer_buffer_length = p_size*W9968CF_ISO_PACKETS;
		urb->interval = 1;
		for (j = 0; j < W9968CF_ISO_PACKETS; j++) {
			urb->iso_frame_desc[j].offset = p_size*j;
			urb->iso_frame_desc[j].length = p_size;
		}
	}

	/* Transfer size per frame, in WORD ! */
	d = cam->hw_depth;
	w = cam->hw_width;
	h = cam->hw_height;

	t_size = (w*h*d)/16;

	err = w9968cf_write_reg(cam, 0xbf17, 0x00); /* reset everything */
	err += w9968cf_write_reg(cam, 0xbf10, 0x00); /* normal operation */

	/* Transfer size */
	err += w9968cf_write_reg(cam, t_size & 0xffff, 0x3d); /* low bits */
	err += w9968cf_write_reg(cam, t_size >> 16, 0x3e);    /* high bits */

	if (cam->vpp_flag & VPP_DECOMPRESSION)
		err += w9968cf_upload_quantizationtables(cam);

	vidcapt = w9968cf_read_reg(cam, 0x16); /* read picture settings */
	err += w9968cf_write_reg(cam, vidcapt|0x8000, 0x16); /* capt. enable */

	err += usb_set_interface(udev, 0, cam->altsetting);
	err += w9968cf_write_reg(cam, 0x8a05, 0x3c); /* USB FIFO enable */

	if (err || (vidcapt < 0)) {
		for (i = 0; i < W9968CF_URBS; i++)
			usb_free_urb(cam->urb[i]);
		DBG(1, "Couldn't tell the camera to start the data transfer.")
		return err;
	}

	w9968cf_init_framelist(cam);

	/* Begin to grab into the temporary buffer */
	cam->frame_tmp.status = F_UNUSED;
	cam->frame_tmp.queued = 0;
	cam->frame_current = &cam->frame_tmp;

	if (!(cam->vpp_flag & VPP_DECOMPRESSION))
		DBG(5, "Isochronous transfer size: %li bytes/frame.",
		    (unsigned long)t_size*2)

	DBG(5, "Starting the isochronous transfer...")

	/* Submit the URBs */
	for (i = 0; i < W9968CF_URBS; i++) {
		err = usb_submit_urb(cam->urb[i]);
		if (err) {
			for (j = i-1; j >= 0; j--)
				if (!usb_unlink_urb(cam->urb[j]))
					usb_free_urb(cam->urb[j]);
			DBG(1, "Couldn't send a transfer request to the "
			       "USB core (error #%d, %s).", err, 
			    symbolic(urb_errlist, err))
		}
	}

	cam->streaming = 1;

	return 0;
}


/*--------------------------------------------------------------------------
  Stop the isochronous transfer and set alternate setting to 0 (0Mb/s).
  Return 0 on success, a negative number otherwise.
  --------------------------------------------------------------------------*/
static int w9968cf_stop_transfer(struct w9968cf_device* cam)
{
	struct usb_device *udev = cam->usbdev;
	unsigned long lock_flags;
	int err = 0;
	s8 i;

	/* This avoids race conditions with usb_submit_urb() 
	   in the URB completition handler */
	spin_lock_irqsave(&cam->urb_lock, lock_flags);
	cam->streaming = 0;
	spin_unlock_irqrestore(&cam->urb_lock, lock_flags);

	for (i = W9968CF_URBS-1; i >= 0; i--)
		if (cam->urb[i]) {
			if (!usb_unlink_urb(cam->urb[i])) {
				usb_free_urb(cam->urb[i]);
				cam->urb[i] = NULL;
			}
		}

	if (cam->disconnected)
		goto exit;

	err = w9968cf_write_reg(cam, 0x0a05, 0x3c); /* stop USB transfer */
	err += usb_set_interface(udev, 0, 0); /* 0 Mb/s */
	err += w9968cf_write_reg(cam, 0x0000, 0x39); /* disable JPEG encoder */
	err += w9968cf_write_reg(cam, 0x0000, 0x16); /* stop video capture */

	if (err) {
		DBG(2, "Failed to tell the camera to stop the isochronous "
		       "transfer. However this is not a critical error.")
		return -EIO;
	}

exit:
	DBG(5, "Isochronous transfer stopped.")
	return 0;
}


/*--------------------------------------------------------------------------
  Write a W9968CF register. 
  Return 0 on success, -1 otherwise.
  --------------------------------------------------------------------------*/
static int w9968cf_write_reg(struct w9968cf_device* cam, u16 value, u16 index)
{
	struct usb_device* udev = cam->usbdev;
	int res;

	res = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0,
	                      USB_TYPE_VENDOR | USB_DIR_OUT | USB_RECIP_DEVICE,
	                      value, index, NULL, 0, W9968CF_USB_CTRL_TIMEOUT);

	if (res < 0)
		DBG(4, "Failed to write a register "
		       "(value 0x%04X, index 0x%02X, error #%d, %s).",
		    value, index, res, symbolic(urb_errlist, res))

	return (res >= 0) ? 0 : -1;
}


/*--------------------------------------------------------------------------
  Read a W9968CF register. 
  Return the register value on success, -1 otherwise.
  --------------------------------------------------------------------------*/
static int w9968cf_read_reg(struct w9968cf_device* cam, u16 index)
{
	struct usb_device* udev = cam->usbdev;
	u16* buff = cam->control_buffer;
	int res;

	res = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0), 1,
	                      USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                      0, index, buff, 2, W9968CF_USB_CTRL_TIMEOUT);

	if (res < 0)
		DBG(4, "Failed to read a register "
		       "(index 0x%02X, error #%d, %s).",
		    index, res, symbolic(urb_errlist, res))

	return (res >= 0) ? (int)(*buff) : -1;
}


/*--------------------------------------------------------------------------
  Write 64-bit data to the fast serial bus registers.
  Return 0 on success, -1 otherwise.
  --------------------------------------------------------------------------*/
static int w9968cf_write_fsb(struct w9968cf_device* cam, u16* data)
{
	struct usb_device* udev = cam->usbdev;
	u16 value;
	int res;

	value = *data++;

	res = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0,
	                      USB_TYPE_VENDOR | USB_DIR_OUT | USB_RECIP_DEVICE,
	                      value, 0x06, data, 6, W9968CF_USB_CTRL_TIMEOUT);

	if (res < 0)
		DBG(4, "Failed to write the FSB registers "
		       "(error #%d, %s).", res, symbolic(urb_errlist, res))

	return (res >= 0) ? 0 : -1;
}


/*--------------------------------------------------------------------------
  Write data to the serial bus control register.
  Return 0 on success, a negative number otherwise.
  --------------------------------------------------------------------------*/
static int w9968cf_write_sb(struct w9968cf_device* cam, u16 value)
{
	int err = 0;

	err = w9968cf_write_reg(cam, value, 0x01);
	udelay(W9968CF_I2C_BUS_DELAY);

	return err;
}


/*--------------------------------------------------------------------------
  Read data from the serial bus control register.
  Return 0 on success, a negative number otherwise.
  --------------------------------------------------------------------------*/
static int w9968cf_read_sb(struct w9968cf_device* cam)
{
	int v = 0;

	v = w9968cf_read_reg(cam, 0x01);
	udelay(W9968CF_I2C_BUS_DELAY);

	return v;
}


/*--------------------------------------------------------------------------
  Upload quantization tables for the JPEG compression.
  This function is called by w9968cf_start_transfer().
  Return 0 on success, a negative number otherwise.
  --------------------------------------------------------------------------*/
static int w9968cf_upload_quantizationtables(struct w9968cf_device* cam)
{
	u16 a, b;
	int err = 0, i, j;

	err += w9968cf_write_reg(cam, 0x0010, 0x39); /* JPEG clock enable */

	for (i = 0, j = 0; i < 32; i++, j += 2) {
		a = Y_QUANTABLE[j] | ((unsigned)(Y_QUANTABLE[j+1]) << 8);
		b = UV_QUANTABLE[j] | ((unsigned)(UV_QUANTABLE[j+1]) << 8);
		err += w9968cf_write_reg(cam, a, 0x40+i);
		err += w9968cf_write_reg(cam, b, 0x60+i);
	}
	err += w9968cf_write_reg(cam, 0x0012, 0x39); /* JPEG encoder enable */

	return err;
}



/****************************************************************************
 * Low-level I2C I/O functions.                                             *
 * The adapter supports the following I2C transfer functions:               *
 * i2c_adap_fastwrite_byte_data() (at 400 kHz bit frequency only)           *
 * i2c_adap_read_byte_data()                                                *
 * i2c_adap_read_byte()                                                     *
 ****************************************************************************/

static int w9968cf_smbus_start(struct w9968cf_device* cam)
{
	int err = 0;

	err += w9968cf_write_sb(cam, 0x0011); /* SDE=1, SDA=0, SCL=1 */
	err += w9968cf_write_sb(cam, 0x0010); /* SDE=1, SDA=0, SCL=0 */

	return err;
}


static int w9968cf_smbus_stop(struct w9968cf_device* cam)
{
	int err = 0;

	err += w9968cf_write_sb(cam, 0x0011); /* SDE=1, SDA=0, SCL=1 */
	err += w9968cf_write_sb(cam, 0x0013); /* SDE=1, SDA=1, SCL=1 */

	return err;
}


static int w9968cf_smbus_write_byte(struct w9968cf_device* cam, u8 v)
{
	u8 bit;
	int err = 0, sda;

	for (bit = 0 ; bit < 8 ; bit++) {
		sda = (v & 0x80) ? 2 : 0;
		v <<= 1;
		/* SDE=1, SDA=sda, SCL=0 */
		err += w9968cf_write_sb(cam, 0x10 | sda);
		/* SDE=1, SDA=sda, SCL=1 */
		err += w9968cf_write_sb(cam, 0x11 | sda);
		/* SDE=1, SDA=sda, SCL=0 */
		err += w9968cf_write_sb(cam, 0x10 | sda);
	}

	return err;
}


static int w9968cf_smbus_read_byte(struct w9968cf_device* cam, u8* v)
{
	u8 bit;
	int err = 0;

	*v = 0;
	for (bit = 0 ; bit < 8 ; bit++) {
		*v <<= 1;
		err += w9968cf_write_sb(cam, 0x0013);
		*v |= (w9968cf_read_sb(cam) & 0x0008) ? 1 : 0;
		err += w9968cf_write_sb(cam, 0x0012);
	}

	return err;
}


static int w9968cf_smbus_write_ack(struct w9968cf_device* cam)
{
	int err = 0;

	err += w9968cf_write_sb(cam, 0x0010); /* SDE=1, SDA=0, SCL=0 */
	err += w9968cf_write_sb(cam, 0x0011); /* SDE=1, SDA=0, SCL=1 */
	err += w9968cf_write_sb(cam, 0x0010); /* SDE=1, SDA=0, SCL=0 */

	return err;
}


static int w9968cf_smbus_read_ack(struct w9968cf_device* cam)
{
	int err = 0, sda;

	err += w9968cf_write_sb(cam, 0x0013); /* SDE=1, SDA=1, SCL=1 */
	sda = (w9968cf_read_sb(cam) & 0x08) ? 1 : 0; /* sda = SDA */
	err += w9968cf_write_sb(cam, 0x0012); /* SDE=1, SDA=1, SCL=0 */
	if (sda < 0)
		err += sda;
	if (sda == 1) {
		DBG(6, "Couldn't receive the ACK.")
		err += -1;
	}

	return err;
}


/* This is seems to refresh the communication through the serial bus */
static int w9968cf_smbus_refresh_bus(struct w9968cf_device* cam)
{
	int err = 0, j;

	for (j = 1; j <= 10; j++) {
		err = w9968cf_write_reg(cam, 0x0020, 0x01);
		err += w9968cf_write_reg(cam, 0x0000, 0x01);
		if (err)
			break;
	}

	return err;
}


/* SMBus protocol: S Addr Wr [A] Subaddr [A] Value [A] P */
static int 
w9968cf_i2c_adap_fastwrite_byte_data(struct w9968cf_device* cam, 
                                     u16 address, u8 subaddress,u8 value)
{
	u16* data = cam->data_buffer;
	int err = 0;

	err += w9968cf_smbus_refresh_bus(cam);

	/* Enable SBUS outputs */
	err += w9968cf_write_sb(cam, 0x0020);

	data[0] = 0x082f | ((address & 0x80) ? 0x1500 : 0x0);
	data[0] |= (address & 0x40) ? 0x4000 : 0x0;
	data[1] = 0x2082 | ((address & 0x40) ? 0x0005 : 0x0);
	data[1] |= (address & 0x20) ? 0x0150 : 0x0;
	data[1] |= (address & 0x10) ? 0x5400 : 0x0;
	data[2] = 0x8208 | ((address & 0x08) ? 0x0015 : 0x0);
	data[2] |= (address & 0x04) ? 0x0540 : 0x0;
	data[2] |= (address & 0x02) ? 0x5000 : 0x0;
	data[3] = 0x1d20 | ((address & 0x02) ? 0x0001 : 0x0);
	data[3] |= (address & 0x01) ? 0x0054 : 0x0;

	err += w9968cf_write_fsb(cam, data);

	data[0] = 0x8208 | ((subaddress & 0x80) ? 0x0015 : 0x0);
	data[0] |= (subaddress & 0x40) ? 0x0540 : 0x0;
	data[0] |= (subaddress & 0x20) ? 0x5000 : 0x0;
	data[1] = 0x0820 | ((subaddress & 0x20) ? 0x0001 : 0x0);
	data[1] |= (subaddress & 0x10) ? 0x0054 : 0x0;
	data[1] |= (subaddress & 0x08) ? 0x1500 : 0x0;
	data[1] |= (subaddress & 0x04) ? 0x4000 : 0x0;
	data[2] = 0x2082 | ((subaddress & 0x04) ? 0x0005 : 0x0);
	data[2] |= (subaddress & 0x02) ? 0x0150 : 0x0;
	data[2] |= (subaddress & 0x01) ? 0x5400 : 0x0;
	data[3] = 0x001d;

	err += w9968cf_write_fsb(cam, data);

	data[0] = 0x8208 | ((value & 0x80) ? 0x0015 : 0x0);
	data[0] |= (value & 0x40) ? 0x0540 : 0x0;
	data[0] |= (value & 0x20) ? 0x5000 : 0x0;
	data[1] = 0x0820 | ((value & 0x20) ? 0x0001 : 0x0);
	data[1] |= (value & 0x10) ? 0x0054 : 0x0;
	data[1] |= (value & 0x08) ? 0x1500 : 0x0;
	data[1] |= (value & 0x04) ? 0x4000 : 0x0;
	data[2] = 0x2082 | ((value & 0x04) ? 0x0005 : 0x0);
	data[2] |= (value & 0x02) ? 0x0150 : 0x0;
	data[2] |= (value & 0x01) ? 0x5400 : 0x0;
	data[3] = 0xfe1d;

	err += w9968cf_write_fsb(cam, data);

	/* Disable SBUS outputs */
	err += w9968cf_write_sb(cam, 0x0000);

	if (!err)
		DBG(5, "I2C write byte data done, addr.0x%04X, subaddr.0x%02X "
		       "value 0x%02X.", address, subaddress, value)
	else
		DBG(5, "I2C write byte data failed, addr.0x%04X, "
		       "subaddr.0x%02X, value 0x%02X.", 
	            address, subaddress, value)

	return err;
}


/* SMBus protocol: S Addr Wr [A] Subaddr [A] P S Addr+1 Rd [A] [Value] NA P */
static int 
w9968cf_i2c_adap_read_byte_data(struct w9968cf_device* cam, 
                                u16 address, u8 subaddress, 
                                u8* value)
{
	int err = 0;

	/* Serial data enable */
	err += w9968cf_write_sb(cam, 0x0013); /* don't change ! */

	err += w9968cf_smbus_start(cam);
	err += w9968cf_smbus_write_byte(cam, address);
	err += w9968cf_smbus_read_ack(cam);
	err += w9968cf_smbus_write_byte(cam, subaddress);
	err += w9968cf_smbus_read_ack(cam);
	err += w9968cf_smbus_stop(cam);
	err += w9968cf_smbus_start(cam);
	err += w9968cf_smbus_write_byte(cam, address + 1);
	err += w9968cf_smbus_read_ack(cam);
	err += w9968cf_smbus_read_byte(cam, value);
	err += w9968cf_smbus_write_ack(cam);
	err += w9968cf_smbus_stop(cam);

	/* Serial data disable */
	err += w9968cf_write_sb(cam, 0x0000);

	if (!err)
		DBG(5, "I2C read byte data done, addr.0x%04X, "
		       "subaddr.0x%02X, value 0x%02X.", 
		    address, subaddress, *value)
	else
		DBG(5, "I2C read byte data failed, addr.0x%04X, "
		       "subaddr.0x%02X, wrong value 0x%02X.",
		    address, subaddress, *value)

	return err;
}


/* SMBus protocol: S Addr+1 Rd [A] [Value] NA P */
static int 
w9968cf_i2c_adap_read_byte(struct w9968cf_device* cam,
                           u16 address, u8* value)
{
	int err = 0;

	/* Serial data enable */
	err += w9968cf_write_sb(cam, 0x0013);

	err += w9968cf_smbus_start(cam);
	err += w9968cf_smbus_write_byte(cam, address + 1);
	err += w9968cf_smbus_read_ack(cam);
	err += w9968cf_smbus_read_byte(cam, value);
	err += w9968cf_smbus_write_ack(cam);
	err += w9968cf_smbus_stop(cam);
 
	/* Serial data disable */
	err += w9968cf_write_sb(cam, 0x0000);

	if (!err)
		DBG(5, "I2C read byte done, addr.0x%04X."
		       "value 0x%02X.", address, *value)
	else
		DBG(5, "I2C read byte failed, addr.0x%04X."
		       "wrong value 0x%02X.", address, *value)

	return err;
}


/* SMBus protocol: S Addr Wr [A] Value [A] P */
static int 
w9968cf_i2c_adap_write_byte(struct w9968cf_device* cam,
                            u16 address, u8 value)
{
	DBG(4, "i2c_write_byte() is an unsupported transfer mode.")
	return -EINVAL;
}



/****************************************************************************
 * I2C interface to kernel                                                  *
 ****************************************************************************/

static int
w9968cf_i2c_smbus_xfer(struct i2c_adapter *adapter, u16 addr, 
                       unsigned short flags, char read_write, u8 command,
                       int size, union i2c_smbus_data *data)
{
	struct w9968cf_device* cam = adapter->data;
	u8 i;
	int err = 0; 

	switch (addr) {
		case OV6xx0_SID:
		case OV7xx0_SID:
			break;
		default:
			DBG(4, "Rejected slave ID 0x%04X", addr)
			return -EINVAL;
	}

	if (size == I2C_SMBUS_BYTE) {
		/* Why addr <<= 1? See OVXXX0_SID defines in ovcamchip.h */
		addr <<= 1;

		if (read_write == I2C_SMBUS_WRITE)
 			err = w9968cf_i2c_adap_write_byte(cam, addr, command);
		else if (read_write == I2C_SMBUS_READ) 
			err = w9968cf_i2c_adap_read_byte(cam,addr,&data->byte);

	} else if (size == I2C_SMBUS_BYTE_DATA) {
		addr <<= 1;

		if (read_write == I2C_SMBUS_WRITE)
 			err = w9968cf_i2c_adap_fastwrite_byte_data(cam, addr,
			                                  command, data->byte);
		else if (read_write == I2C_SMBUS_READ) {
			for (i = 1; i <= W9968CF_I2C_RW_RETRIES; i++) {
				err = w9968cf_i2c_adap_read_byte_data(cam,addr,
				                         command, &data->byte);
				if (err) {
					if (w9968cf_smbus_refresh_bus(cam)) {
						err = -EIO;
						break;
					}
				} else
					break;
			}

		} else
			return -EINVAL;

	} else {
		DBG(4, "Unsupported I2C transfer mode (%d)", size)
		return -EINVAL;
	}

	return err;
}


static u32 w9968cf_i2c_func(struct i2c_adapter* adap)
{
	return I2C_FUNC_SMBUS_READ_BYTE |
	       I2C_FUNC_SMBUS_READ_BYTE_DATA  |
	       I2C_FUNC_SMBUS_WRITE_BYTE_DATA;
}


static int w9968cf_i2c_attach_inform(struct i2c_client* client)
{
	struct w9968cf_device* cam = client->adapter->data;
	const char* clientname = client->name;
	int id = client->driver->id, err = 0;

	if (id == I2C_DRIVERID_OVCAMCHIP) {
		cam->sensor_client = client;
		err = w9968cf_sensor_init(cam);
		if (err) {
			cam->sensor_client = NULL;
			return err;
		}
	} else {
		DBG(4, "Rejected client [%s] with driver [%s]", 
		    clientname, client->driver->name)
		return -EINVAL;
	}

	DBG(5, "I2C attach client [%s] with driver [%s]",
	    clientname, client->driver->name)

	return 0;
}


static int w9968cf_i2c_detach_inform(struct i2c_client* client)
{
	struct w9968cf_device* cam = client->adapter->data;
	const char* clientname = client->name;

	if (cam->sensor_client == client) {
		cam->sensor_client = NULL;
	}

	DBG(5, "I2C detach client [%s]", clientname)

	return 0;
}


static int 
w9968cf_i2c_control(struct i2c_adapter* adapter, unsigned int cmd,
                    unsigned long arg)
{
	return 0;
}


static void w9968cf_i2c_inc_use(struct i2c_adapter* adap)
{
	MOD_INC_USE_COUNT;
}


static void w9968cf_i2c_dec_use(struct i2c_adapter* adap)
{
	MOD_DEC_USE_COUNT;
}


static int w9968cf_i2c_init(struct w9968cf_device* cam)
{
	int err = 0;

	static struct i2c_algorithm algo = {
		.name =          "W996[87]CF algorithm",
		.id =            I2C_ALGO_SMBUS,
		.smbus_xfer =    w9968cf_i2c_smbus_xfer,
		.algo_control =  w9968cf_i2c_control,
		.functionality = w9968cf_i2c_func,
	};

	static struct i2c_adapter adap = {
		.id =                I2C_ALGO_SMBUS | I2C_HW_SMBUS_W9968CF,
		.inc_use =           w9968cf_i2c_inc_use,
		.dec_use =           w9968cf_i2c_dec_use,
		.client_register =   w9968cf_i2c_attach_inform,
		.client_unregister = w9968cf_i2c_detach_inform,
		.algo =              &algo,
	};

	memcpy(&cam->i2c_adapter, &adap, sizeof(struct i2c_adapter));
	strcpy(cam->i2c_adapter.name, "w9968cf");
	cam->i2c_adapter.data = cam;

	DBG(6, "Registering I2C adapter with kernel...")

	err = i2c_add_adapter(&cam->i2c_adapter);
	if (err)
		DBG(1, "Failed to register the I2C adapter.")
	else
		DBG(5, "I2C adapter registered.")

	return err;
}



/****************************************************************************
 * Helper functions                                                         *
 ****************************************************************************/

/*--------------------------------------------------------------------------
  Turn on the LED on some webcams. A beep should be heard too.
  Return 0 on success, a negative number otherwise.
  --------------------------------------------------------------------------*/
static int w9968cf_turn_on_led(struct w9968cf_device* cam)
{
	int err = 0;

	err += w9968cf_write_reg(cam, 0xff00, 0x00); /* power-down */
	err += w9968cf_write_reg(cam, 0xbf17, 0x00); /* reset everything */
	err += w9968cf_write_reg(cam, 0xbf10, 0x00); /* normal operation */
	err += w9968cf_write_reg(cam, 0x0010, 0x01); /* serial bus, SDS high */
	err += w9968cf_write_reg(cam, 0x0000, 0x01); /* serial bus, SDS low */
	err += w9968cf_write_reg(cam, 0x0010, 0x01); /* ..high 'beep-beep' */

	if (err)
		DBG(2, "Couldn't turn on the LED.")

	DBG(5, "LED turned on.")

	return err;
}


/*--------------------------------------------------------------------------
  Write some registers for the device initialization.
  This function is called once on open().
  Return 0 on success, a negative number otherwise.
  --------------------------------------------------------------------------*/
static int w9968cf_init_chip(struct w9968cf_device* cam)
{
	int err = 0;

	err += w9968cf_write_reg(cam, 0xff00, 0x00); /* power off */
	err += w9968cf_write_reg(cam, 0xbf10, 0x00); /* power on */

	err += w9968cf_write_reg(cam, 0x405d, 0x03); /* DRAM timings */
	err += w9968cf_write_reg(cam, 0x0030, 0x04); /* SDRAM timings */

	err += w9968cf_write_reg(cam, 0x0000, 0x20); /* Y frame buf.0, low */
	err += w9968cf_write_reg(cam, 0x0000, 0x21); /* Y frame buf.0, high */
	err += w9968cf_write_reg(cam, 0xb000, 0x22); /* Y frame buf.1, low */
	err += w9968cf_write_reg(cam, 0x0004, 0x23); /* Y frame buf.1, high */
	err += w9968cf_write_reg(cam, 0x5800, 0x24); /* U frame buf.0, low */
	err += w9968cf_write_reg(cam, 0x0002, 0x25); /* U frame buf.0, high */
	err += w9968cf_write_reg(cam, 0x0800, 0x26); /* U frame buf.1, low */
	err += w9968cf_write_reg(cam, 0x0007, 0x27); /* U frame buf.1, high */
	err += w9968cf_write_reg(cam, 0x8400, 0x28); /* V frame buf.0, low */
	err += w9968cf_write_reg(cam, 0x0003, 0x29); /* V frame buf.0, high */
	err += w9968cf_write_reg(cam, 0x3400, 0x2a); /* V frame buf.1, low */
	err += w9968cf_write_reg(cam, 0x0008, 0x2b); /* V frame buf.1, high */

	err += w9968cf_write_reg(cam, 0x6000, 0x32); /* JPEG bitstream buf 0 */
	err += w9968cf_write_reg(cam, 0x0009, 0x33); /* JPEG bitstream buf 0 */
	err += w9968cf_write_reg(cam, 0x2000, 0x34); /* JPEG bitstream buf 1 */
	err += w9968cf_write_reg(cam, 0x000d, 0x35); /* JPEG bitstream buf 1 */

	err += w9968cf_write_reg(cam, 0x0000, 0x36);/* JPEG restart interval */
	err += w9968cf_write_reg(cam, 0x0804, 0x37);/*JPEG VLE FIFO threshold*/
	err += w9968cf_write_reg(cam, 0x0000, 0x38);/* disable hw up-scaling */
	err += w9968cf_write_reg(cam, 0x0000, 0x3f); /* JPEG/MCTL test data */

	err += w9968cf_set_picture(cam, cam->picture); /* this before */
	err += w9968cf_set_window(cam, cam->window);

	if (err)
		DBG(1, "Chip initialization failed.")
	else
		DBG(5, "Chip successfully initialized.")

	return err;
}


/*--------------------------------------------------------------------------
  Change the picture settings of the camera.
  Return 0 on success, a negative number otherwise.
  --------------------------------------------------------------------------*/
static int
w9968cf_set_picture(struct w9968cf_device* cam, struct video_picture pict)
{
	u16 fmt, hw_depth, hw_palette, reg_v = 0x0000;
	int err = 0;

	/* Make sure we are using a valid depth */
	pict.depth = w9968cf_valid_depth(pict.palette);

	fmt = pict.palette;

	hw_depth = pict.depth; /* depth used by the winbond chip */
	hw_palette = pict.palette; /* palette used by the winbond chip */

	/* VS & HS polarities */
	reg_v = (cam->vs_polarity << 12) | (cam->hs_polarity << 11);

	switch (fmt)
	{
		case VIDEO_PALETTE_UYVY:
			reg_v |= 0x0000;
			cam->vpp_flag = VPP_NONE;
			break;
		case VIDEO_PALETTE_YUV422P:
			reg_v |= 0x0002;
			cam->vpp_flag = VPP_DECOMPRESSION;
			break;
		case VIDEO_PALETTE_YUV420:
		case VIDEO_PALETTE_YUV420P:
			reg_v |= 0x0003;
			cam->vpp_flag = VPP_DECOMPRESSION;
			break;
		case VIDEO_PALETTE_YUYV:
		case VIDEO_PALETTE_YUV422:
			reg_v |= 0x0000;
			cam->vpp_flag = VPP_SWAP_YUV_BYTES;
			hw_palette = VIDEO_PALETTE_UYVY;
			break;
		/* Original video is used instead of RGBX palettes. 
		   Software conversion later. */
		case VIDEO_PALETTE_GREY:
		case VIDEO_PALETTE_RGB555:
		case VIDEO_PALETTE_RGB565:
		case VIDEO_PALETTE_RGB24:
		case VIDEO_PALETTE_RGB32:
			reg_v |= 0x0000; /* UYVY 16 bit is used */
			hw_depth = 16;
			hw_palette = VIDEO_PALETTE_UYVY;
			cam->vpp_flag = VPP_UYVY_TO_RGBX;
			break;
	}

	/* FIXME: 'hardware double buffer' doesn't work when compressed video
	          is enabled (corrupted frames). */
	if (cam->double_buffer && !(cam->vpp_flag & VPP_DECOMPRESSION))
		reg_v |= 0x0080;

	if (cam->clamping)
		reg_v |= 0x0020;

	if (cam->filter_type == 1)
		reg_v |= 0x0008;
	else if (cam->filter_type == 2)
		reg_v |= 0x000c;

	if ((err = w9968cf_write_reg(cam, reg_v, 0x16)))
		goto error;

	if ((err = w9968cf_sensor_update_picture(cam, pict)))
		goto error;

	/* If all went well, update the device data structure */
	memcpy(&cam->picture, &pict, sizeof(pict));
	cam->hw_depth = hw_depth;
	cam->hw_palette = hw_palette;

	/* Settings changed, so we clear the frame buffers */
	memset(cam->frame[0].buffer, 0, 
	       cam->nbuffers*w9968cf_get_max_bufsize(cam));

	DBG(4, "Palette is %s, depth is %d bpp.",
	    symbolic(v4l1_plist, pict.palette), pict.depth)

	return 0;

error:
	DBG(1, "Failed to change picture settings.")
	return err;
}


/*--------------------------------------------------------------------------
  Change the capture area size of the camera.
  This function _must_ be called _after_ w9968cf_set_picture().
  Return 0 on success, a negative number otherwise.
  --------------------------------------------------------------------------*/
static int
w9968cf_set_window(struct w9968cf_device* cam, struct video_window win)
{
	u16 x, y, w, h, scx, scy, cw, ch, ax, ay;
	unsigned long fw, fh;
	struct ovcamchip_window s_win;
	int err = 0;

	/* Work around to avoid FP arithmetics */
	#define __SC(x) ((x) << 10)
	#define __UNSC(x) ((x) >> 10)

	/* Make sure we are using a supported resolution */
	if ((err = w9968cf_adjust_window_size(cam, (u16*)&win.width, 
	                                      (u16*)&win.height)))
		goto error;

	/* Scaling factors */
	fw = __SC(win.width) / cam->maxwidth;
	fh = __SC(win.height) / cam->maxheight;

	/* Set up the width and height values used by the chip */
	if ((win.width > cam->maxwidth) || (win.height > cam->maxheight)) {
		cam->vpp_flag |= VPP_UPSCALE;
		/* Calculate largest w,h mantaining the same w/h ratio */
		w = (fw >= fh) ? cam->maxwidth : __SC(win.width)/fh;
		h = (fw >= fh) ? __SC(win.height)/fw : cam->maxheight;
		if (w < cam->minwidth) /* just in case */
			w = cam->minwidth;
		if (h < cam->minheight) /* just in case */
			h = cam->minheight;
	} else {
		cam->vpp_flag &= ~VPP_UPSCALE;
		w = win.width;
		h = win.height;
	}

	/* x,y offsets of the cropped area */
	scx = cam->start_cropx;
	scy = cam->start_cropy;

	/* Calculate cropped area manteining the right w/h ratio */
	if (cam->largeview && !(cam->vpp_flag & VPP_UPSCALE)) {
		cw = (fw >= fh) ? cam->maxwidth : __SC(win.width)/fh;
		ch = (fw >= fh) ? __SC(win.height)/fw : cam->maxheight;
	} else {
		cw = w;
		ch = h;
	}

	/* Setup the window of the sensor */
	s_win.format = VIDEO_PALETTE_UYVY;
	s_win.width = cam->maxwidth;
	s_win.height = cam->maxheight;
	s_win.quarter = 0; /* full progressive video */

	/* Center it */
	s_win.x = (s_win.width - cw) / 2;
	s_win.y = (s_win.height - ch) / 2;

	/* Clock divisor */
	if (cam->clockdiv >= 0)
		s_win.clockdiv = cam->clockdiv; /* manual override */
	else
		switch (cam->sensor) {
			case CC_OV6620:
				s_win.clockdiv = 0;
				break;
			case CC_OV6630:
				s_win.clockdiv = 0;
				break;
			case CC_OV76BE:
			case CC_OV7610:
			case CC_OV7620:
				s_win.clockdiv = 0;
				break;
			default:
				s_win.clockdiv = W9968CF_DEF_CLOCKDIVISOR;
		}

	/* We have to scale win.x and win.y offsets */
	if ( (cam->largeview && !(cam->vpp_flag & VPP_UPSCALE))
	     || (cam->vpp_flag & VPP_UPSCALE) ) {
		ax = __SC(win.x)/fw;
		ay = __SC(win.y)/fh;
	} else {
		ax = win.x;
		ay = win.y;
	}

	if ((ax + cw) > cam->maxwidth)
		ax = cam->maxwidth - cw;

	if ((ay + ch) > cam->maxheight)
		ay = cam->maxheight - ch;

	/* Adjust win.x, win.y */
	if ( (cam->largeview && !(cam->vpp_flag & VPP_UPSCALE))
	     || (cam->vpp_flag & VPP_UPSCALE) ) {
		win.x = __UNSC(ax*fw);
		win.y = __UNSC(ay*fh);
	} else {
		win.x = ax;
		win.y = ay;
	}

	/* Offsets used by the chip */
	x = ax + s_win.x;
	y = ay + s_win.y;

	/* Go ! */
	if ((err = w9968cf_sensor_cmd(cam, OVCAMCHIP_CMD_S_MODE, &s_win)))
		goto error;

	err += w9968cf_write_reg(cam, scx + x, 0x10);
	err += w9968cf_write_reg(cam, scy + y, 0x11);
	err += w9968cf_write_reg(cam, scx + x + cw, 0x12);
	err += w9968cf_write_reg(cam, scy + y + ch, 0x13);
	err += w9968cf_write_reg(cam, w, 0x14);
	err += w9968cf_write_reg(cam, h, 0x15);

	/* JPEG width & height */
	err += w9968cf_write_reg(cam, w, 0x30);
	err += w9968cf_write_reg(cam, h, 0x31);

	/* Y & UV frame buffer strides (in WORD) */
	if (cam->vpp_flag & VPP_DECOMPRESSION) {
		err += w9968cf_write_reg(cam, w/2, 0x2c);
		err += w9968cf_write_reg(cam, w/4, 0x2d);
	} else
		err += w9968cf_write_reg(cam, w, 0x2c);

	if (err)
		goto error;

	/* If all went well, update the device data structure */
	memcpy(&cam->window, &win, sizeof(win));
	cam->hw_width = w;
	cam->hw_height = h;

	/* Settings changed, so we clear the frame buffers */
	memset(cam->frame[0].buffer, 0, 
	       cam->nbuffers*w9968cf_get_max_bufsize(cam));

	DBG(4, "The capture area is %dx%d, Offset (x,y)=(%d,%d).", 
	    win.width, win.height, win.x, win.y)

	PDBGG("x=%d ,y=%d, w=%d, h=%d, ax=%d, ay=%d, s_win.x=%d, s_win.y=%d, "
	      "cw=%d, ch=%d, win.x=%d ,win.y=%d, win.width=%d, win.height=%d",
	      x, y, w, h, ax, ay, s_win.x, s_win.y, cw, ch, win.x, win.y,
	      win.width, win.height)

	return 0;

error:
	DBG(1, "Failed to change the capture area size.")
	return err;
}


/*--------------------------------------------------------------------------
  Return non-zero if the palette is supported, 0 otherwise.
  --------------------------------------------------------------------------*/
static inline u16 w9968cf_valid_palette(u16 palette)
{
	u8 i = 0;
	while (w9968cf_formatlist[i].palette != 0) {
		if (palette == w9968cf_formatlist[i].palette)
			return palette;
		i++;
	}
	return 0;
}


/*--------------------------------------------------------------------------
  Return the depth corresponding to the given palette.
  Palette _must_ be supported !
  --------------------------------------------------------------------------*/
static inline u16 w9968cf_valid_depth(u16 palette)
{
	u8 i=0;
	while (w9968cf_formatlist[i].palette != palette)
		i++;

	return w9968cf_formatlist[i].depth;
}


/*--------------------------------------------------------------------------
  Return non-zero if the format requires decompression, 0 otherwise.
  --------------------------------------------------------------------------*/
static inline u8 w9968cf_need_decompression(u16 palette)
{
	u8 i = 0;
	while (w9968cf_formatlist[i].palette != 0) {
		if (palette == w9968cf_formatlist[i].palette)
			return w9968cf_formatlist[i].compression;
		i++;
	}
	return 0;
}


/*-------------------------------------------------------------------------- 
  Adjust the asked values for window width and height.
  Return 0 on success, -1 otherwise.
  --------------------------------------------------------------------------*/
static int 
w9968cf_adjust_window_size(struct w9968cf_device* cam, u16* width, u16* height)
{
	u16 maxw, maxh;

	if ((*width < cam->minwidth) || (*height < cam->minheight))
		return -ERANGE;

	maxw = cam->upscaling && !(cam->vpp_flag & VPP_DECOMPRESSION)
	       && w9968cf_vppmod_present ? W9968CF_MAX_WIDTH : cam->maxwidth;
	maxh = cam->upscaling && !(cam->vpp_flag & VPP_DECOMPRESSION)
	       && w9968cf_vppmod_present ? W9968CF_MAX_HEIGHT : cam->maxheight;

	if (*width > maxw)
		*width = maxw;
	if (*height > maxh)
		*height = maxh;

	if (cam->vpp_flag & VPP_DECOMPRESSION) {
		*width  &= ~15L; /* multiple of 16 */
		*height &= ~15L;
	}

	PDBGG("Window size adjusted w=%d, h=%d ", *width, *height)

	return 0;
}


/*--------------------------------------------------------------------------
  Initialize the FIFO list of requested frames.
  --------------------------------------------------------------------------*/
static void w9968cf_init_framelist(struct w9968cf_device* cam)
{
	u8 i;

	for (i = 0; i < cam->nbuffers; i++) {
		cam->requested_frame[i] = NULL;
		cam->frame[i].queued = 0;
		cam->frame[i].status = F_UNUSED;
	}
}


/*--------------------------------------------------------------------------
  Add a frame in the FIFO list of requested frames.
  This function is called in process context.
  --------------------------------------------------------------------------*/
static void w9968cf_push_frame(struct w9968cf_device* cam, u8 f_num)
{
	u8 f;
	unsigned long lock_flags;

	spin_lock_irqsave(&cam->flist_lock, lock_flags);

	for (f=0; cam->requested_frame[f] != NULL; f++);
	cam->requested_frame[f] = &cam->frame[f_num];
	cam->frame[f_num].queued = 1;
	cam->frame[f_num].status = F_UNUSED; /* clear the status */

	spin_unlock_irqrestore(&cam->flist_lock, lock_flags);

	DBG(6, "Frame #%d pushed into the FIFO list. Position %d.", f_num, f)
}


/*--------------------------------------------------------------------------
  Read, store and remove the first pointer in the FIFO list of requested
  frames. This function is called in interrupt context.
  --------------------------------------------------------------------------*/
static void 
w9968cf_pop_frame(struct w9968cf_device* cam, struct w9968cf_frame_t** framep)
{
	u8 i;

	spin_lock(&cam->flist_lock);

	*framep = cam->requested_frame[0];

	/* Shift the list of pointers */
	for (i = 0; i < cam->nbuffers-1; i++)
		cam->requested_frame[i] = cam->requested_frame[i+1];
	cam->requested_frame[i] = NULL;

	spin_unlock(&cam->flist_lock);

	DBG(6,"Popped frame #%d from the list.",*framep-&cam->frame[0])
}


/*--------------------------------------------------------------------------
  High-level video post-processing routine on grabbed frames.
  Return 0 on success, a negative number otherwise.
  --------------------------------------------------------------------------*/
static int 
w9968cf_postprocess_frame(struct w9968cf_device* cam, 
                          struct w9968cf_frame_t* fr)
{
	void *pIn = fr->buffer, *pOut = cam->vpp_buffer, *tmp;
	u16 w = cam->window.width,
	    h = cam->window.height,
	    d = cam->picture.depth,
	    fmt = cam->picture.palette,
	    rgb = cam->force_rgb,
	    hw_w = cam->hw_width,
	    hw_h = cam->hw_height,
	    hw_d = cam->hw_depth;
	int err = 0;

	#define _PSWAP(pIn, pOut) {tmp = (pIn); (pIn) = (pOut); (pOut) = tmp;}

	if (cam->vpp_flag & VPP_DECOMPRESSION) {
		memcpy(pOut, pIn, fr->length);
		_PSWAP(pIn, pOut)
		err = (*w9968cf_vpp_decode)(pIn, fr->length, hw_w, hw_h, pOut);
		PDBGG("Compressed frame length: %li",(unsigned long)fr->length)
		fr->length = (hw_w*hw_h*hw_d)/8;
		_PSWAP(pIn, pOut)
		if (err) {
			DBG(4, "An error occurred while decoding the frame: "
			       "%s.", symbolic(decoder_errlist, err))
			return err;
		} else
			DBG(6, "Frame decoded")
	}

	if (cam->vpp_flag & VPP_SWAP_YUV_BYTES) {
		(*w9968cf_vpp_swap_yuvbytes)(pIn, fr->length);
		DBG(6, "Original UYVY component ordering changed.")
	}

	if (cam->vpp_flag & VPP_UPSCALE) {
		(*w9968cf_vpp_scale_up)(pIn, pOut, hw_w, hw_h, hw_d, w, h);
		fr->length = (w*h*hw_d)/8;
		_PSWAP(pIn, pOut)
		DBG(6, "Vertical up-scaling done: %d,%d,%dbpp->%d,%d",
		    hw_w, hw_h, hw_d, w, h)
	}

	if (cam->vpp_flag & VPP_UYVY_TO_RGBX) {
		(*w9968cf_vpp_uyvy_to_rgbx)(pIn, fr->length, pOut, fmt, rgb);
		fr->length = (w*h*d)/8;
		_PSWAP(pIn, pOut)
		DBG(6, "UYVY-16bit to %s conversion done.", 
		    symbolic(v4l1_plist, fmt))
	}

	if (pOut == fr->buffer)
		memcpy(fr->buffer, cam->vpp_buffer, fr->length);

	return 0;
}



/****************************************************************************
 * CMOS sensor control routines                                             *
 ****************************************************************************/

static int 
w9968cf_sensor_set_control(struct w9968cf_device* cam, int cid, int val)
{
	struct ovcamchip_control ctl;
	int err;

	ctl.id = cid;
	ctl.value = val;

	err = w9968cf_sensor_cmd(cam, OVCAMCHIP_CMD_S_CTRL, &ctl);

	return err;
}


static int 
w9968cf_sensor_get_control(struct w9968cf_device* cam, int cid, int* val)
{
	struct ovcamchip_control ctl;
	int err;

	ctl.id = cid;

	err = w9968cf_sensor_cmd(cam, OVCAMCHIP_CMD_G_CTRL, &ctl);
	if (!err)
		*val = ctl.value;

	return err;
}


static inline int
w9968cf_sensor_cmd(struct w9968cf_device* cam, unsigned int cmd, void* arg)
{
	struct i2c_client* c = cam->sensor_client;
	int rc = 0;

	if (c->driver->command) {
		rc = c->driver->command(cam->sensor_client, cmd, arg);
		/* The I2C driver returns -EPERM on non-supported controls */
		return (rc < 0 && rc != -EPERM) ? rc : 0;
	} else
		return -ENODEV;
}


/*--------------------------------------------------------------------------
  Update some settings of the CMOS sensor.
  Returns: 0 on success, a negative number otherwise.
  --------------------------------------------------------------------------*/
static int w9968cf_sensor_update_settings(struct w9968cf_device* cam)
{
	int err = 0;

	/* Auto brightness */
	err = w9968cf_sensor_set_control(cam, OVCAMCHIP_CID_AUTOBRIGHT, 
	                                 cam->auto_brt);
	if (err)
		return err;

	/* Auto exposure */
	err = w9968cf_sensor_set_control(cam, OVCAMCHIP_CID_AUTOEXP, 
	                                 cam->auto_exp);
	if (err)
		return err;

	/* Banding filter */
	err = w9968cf_sensor_set_control(cam, OVCAMCHIP_CID_BANDFILT, 
	                                 cam->bandfilt);
	if (err)
		return err;

	/* Light frequency */
	err = w9968cf_sensor_set_control(cam, OVCAMCHIP_CID_FREQ,
	                                 cam->lightfreq);
	if (err)
		return err;

	/* Back light */
	err = w9968cf_sensor_set_control(cam, OVCAMCHIP_CID_BACKLIGHT,
	                                 cam->backlight);
	if (err)
		return err;

	/* Mirror */
	err = w9968cf_sensor_set_control(cam, OVCAMCHIP_CID_MIRROR,
	                                 cam->mirror);
	if (err)
		return err;

	return 0;
}


/*--------------------------------------------------------------------------
  Get some current picture settings from the CMOS sensor and update the
  internal 'picture' structure of the camera.
  Returns: 0 on success, a negative number otherwise.
  --------------------------------------------------------------------------*/
static int w9968cf_sensor_get_picture(struct w9968cf_device* cam)
{
	int err, v;

	err = w9968cf_sensor_get_control(cam, OVCAMCHIP_CID_CONT, &v);
	if (err)
		return err;
	cam->picture.contrast = v;

	err = w9968cf_sensor_get_control(cam, OVCAMCHIP_CID_BRIGHT, &v);
	if (err)
		return err;
	cam->picture.brightness = v;

	err = w9968cf_sensor_get_control(cam, OVCAMCHIP_CID_SAT, &v);
	if (err)
		return err;
	cam->picture.colour = v;

	err = w9968cf_sensor_get_control(cam, OVCAMCHIP_CID_HUE, &v);
	if (err)
		return err;
	cam->picture.hue = v;

	DBG(5, "Got picture settings from the CMOS sensor.")

	PDBGG("Brightness, contrast, hue, colour, whiteness are "
	      "%d,%d,%d,%d,%d.", cam->picture.brightness,cam->picture.contrast,
	      cam->picture.hue, cam->picture.colour, cam->picture.whiteness)

	return 0;
}


/*--------------------------------------------------------------------------
  Update picture settings of the CMOS sensor.
  Returns: 0 on success, a negative number otherwise.
  --------------------------------------------------------------------------*/
static int
w9968cf_sensor_update_picture(struct w9968cf_device* cam, 
                              struct video_picture pict)
{
	int err = 0;

	if ((!cam->sensor_initialized)
	    || pict.contrast != cam->picture.contrast) {
		err = w9968cf_sensor_set_control(cam, OVCAMCHIP_CID_CONT,
		                                 pict.contrast);
		if (err)
			goto fail;
		DBG(4, "Contrast changed from %d to %d.",
		    cam->picture.contrast, pict.contrast)
		cam->picture.contrast = pict.contrast;
	}

	if (((!cam->sensor_initialized) || 
	    pict.brightness != cam->picture.brightness) && (!cam->auto_brt)) {
		err = w9968cf_sensor_set_control(cam, OVCAMCHIP_CID_BRIGHT, 
		                                 pict.brightness);
		if (err)
			goto fail;
		DBG(4, "Brightness changed from %d to %d.",
		    cam->picture.brightness, pict.brightness)
		cam->picture.brightness = pict.brightness;
	}

	if ((!cam->sensor_initialized) || pict.colour != cam->picture.colour) {
		err = w9968cf_sensor_set_control(cam, OVCAMCHIP_CID_SAT, 
		                                 pict.colour);
		if (err)
			goto fail;
		DBG(4, "Colour changed from %d to %d.",
		    cam->picture.colour, pict.colour)
		cam->picture.colour = pict.colour;
	}

	if ((!cam->sensor_initialized) || pict.hue != cam->picture.hue) {
		err = w9968cf_sensor_set_control(cam, OVCAMCHIP_CID_HUE, 
		                                 pict.hue);
		if (err)
			goto fail;
		DBG(4, "Hue changed from %d to %d.",
		    cam->picture.hue, pict.hue)
		cam->picture.hue = pict.hue;
	}

	return 0;

fail:
	DBG(4, "Failed to change sensor picture setting.")
	return err;
}



/****************************************************************************
 * Camera configuration                                                     *
 ****************************************************************************/

/*--------------------------------------------------------------------------
  This function is called when a supported CMOS sensor is detected.
  Return 0 if the initialization succeeds, a negative number otherwise.
  --------------------------------------------------------------------------*/
static int w9968cf_sensor_init(struct w9968cf_device* cam)
{
	int err = 0;

	if ((err = w9968cf_sensor_cmd(cam, OVCAMCHIP_CMD_INITIALIZE, 
	                              &cam->monochrome)))
		goto error;

	if ((err = w9968cf_sensor_cmd(cam, OVCAMCHIP_CMD_Q_SUBTYPE, 
	                              &cam->sensor)))
		goto error;

	/* NOTE: Make sure width and height are a multiple of 16 */
	switch (cam->sensor_client->addr) {
		case OV6xx0_SID:
			cam->maxwidth = 352;
			cam->maxheight = 288;
			cam->minwidth = 64;
			cam->minheight = 48;
			break;
		case OV7xx0_SID:
			cam->maxwidth = 640;
			cam->maxheight = 480;
			cam->minwidth = 64;
			cam->minheight = 48;
			break;
		default:
			DBG(1, "Not supported CMOS sensor detected for %s.",
			    symbolic(camlist, cam->id))
			return -EINVAL;
	}

	/* These values depend on the ones in the ovxxx0.c sources */
	switch (cam->sensor) {
		case CC_OV7620:
			cam->start_cropx = 287;
			cam->start_cropy = 35;
			/* Seems to work around a bug in the CMOS sensor */
			cam->vs_polarity = 1;
			cam->hs_polarity = 1;
			break;
		default:
			cam->start_cropx = 320;
			cam->start_cropy = 35;
			cam->vs_polarity = 1;
			cam->hs_polarity = 0;
	}

	if ((err = w9968cf_sensor_update_settings(cam)))
		goto error;

	if ((err = w9968cf_sensor_update_picture(cam, cam->picture)))
		goto error;

	cam->sensor_initialized = 1;

	DBG(2, "%s CMOS sensor initialized.", symbolic(senlist, cam->sensor))
	return 0;

error:
	cam->sensor_initialized = 0;
	cam->sensor = CC_UNKNOWN;
	DBG(1, "CMOS sensor initialization failed for %s (/dev/video%d). "
	       "Try to detach and attach this device again.",
	    symbolic(camlist, cam->id), cam->v4ldev->minor)
	return err;
}


/*--------------------------------------------------------------------------
  Fill some basic fields in the main device data structure.
  This function is called once on w9968cf_usb_probe() for each recognized 
  camera.
  --------------------------------------------------------------------------*/
static void
w9968cf_configure_camera(struct w9968cf_device* cam,
                         struct usb_device* udev,
                         enum w9968cf_model_id mod_id,
                         const unsigned short dev_nr)
{
#ifdef CONFIG_VIDEO_PROC_FS
	init_MUTEX(&cam->procfs_sem);
#endif
	init_MUTEX(&cam->fileop_sem);
	init_waitqueue_head(&cam->open);
	spin_lock_init(&cam->urb_lock);
	spin_lock_init(&cam->flist_lock);

	cam->users = 0;
	cam->disconnected = 0;
	cam->usbdev = udev;
	cam->id = mod_id;
	cam->sensor = CC_UNKNOWN;
	cam->sensor_initialized = 0;

	/* Calculate the alternate setting number (from 1 to 16)
	   according to the 'packet_size' module parameter */
	if (packet_size[dev_nr] < W9968CF_MIN_PACKET_SIZE)
		packet_size[dev_nr] = W9968CF_MIN_PACKET_SIZE;
	for (cam->altsetting = 1;
	     packet_size[dev_nr] < wMaxPacketSize[cam->altsetting-1];
	     cam->altsetting++);

	cam->max_buffers = (max_buffers[dev_nr] < 2 || 
	                    max_buffers[dev_nr] > W9968CF_MAX_BUFFERS)
	                   ? W9968CF_BUFFERS : (u8)max_buffers[dev_nr];

	cam->double_buffer = (double_buffer[dev_nr] == 0 || 
	                      double_buffer[dev_nr] == 1)
	                     ? (u8)double_buffer[dev_nr]:W9968CF_DOUBLE_BUFFER;

	cam->clamping = (clamping[dev_nr] == 0 || clamping[dev_nr] == 1)
	                ? (u8)clamping[dev_nr] : W9968CF_CLAMPING;
	
	cam->filter_type = (filter_type[dev_nr] == 0 ||
	                    filter_type[dev_nr] == 1 ||
	                    filter_type[dev_nr] == 2)
	                   ? (u8)filter_type[dev_nr] : W9968CF_FILTER_TYPE;

	cam->capture = 1;

	cam->largeview = (largeview[dev_nr] == 0 || largeview[dev_nr] == 1)
	                 ? (u8)largeview[dev_nr] : W9968CF_LARGEVIEW;

	cam->decompression = (decompression[dev_nr] == 0 || 
	                      decompression[dev_nr] == 1 ||
	                      decompression[dev_nr] == 2)
	                     ? (u8)decompression[dev_nr]:W9968CF_DECOMPRESSION;

	cam->upscaling = (upscaling[dev_nr] == 0 || 
	                  upscaling[dev_nr] == 1)
	                 ? (u8)upscaling[dev_nr] : W9968CF_UPSCALING;

	cam->auto_brt = (autobright[dev_nr] == 0 || autobright[dev_nr] == 1)
	                ? (u8)autobright[dev_nr] : W9968CF_AUTOBRIGHT;

	cam->auto_exp = (autoexp[dev_nr] == 0 || autoexp[dev_nr] == 1)
	                ? (u8)autoexp[dev_nr] : W9968CF_AUTOEXP;

	cam->lightfreq = (lightfreq[dev_nr] == 50 || lightfreq[dev_nr] == 60)
	                 ? (u8)lightfreq[dev_nr] : W9968CF_LIGHTFREQ;

	cam->bandfilt = (bandingfilter[dev_nr] == 0 || 
	                 bandingfilter[dev_nr] == 1)
	                ? (u8)bandingfilter[dev_nr] : W9968CF_BANDINGFILTER;

	cam->backlight = (backlight[dev_nr] == 0 || backlight[dev_nr] == 1)
	                 ? (u8)backlight[dev_nr] : W9968CF_BACKLIGHT;

	cam->clockdiv = (clockdiv[dev_nr] == -1 || clockdiv[dev_nr] >= 0)
	                ? (s8)clockdiv[dev_nr] : W9968CF_CLOCKDIV;

	cam->mirror = (mirror[dev_nr] == 0 || mirror[dev_nr] == 1)
	              ? (u8)mirror[dev_nr] : W9968CF_MIRROR;

	cam->monochrome = (monochrome[dev_nr] == 0 || monochrome[dev_nr] == 1)
	                  ? monochrome[dev_nr] : W9968CF_MONOCHROME;

	cam->picture.brightness = (u16)brightness[dev_nr];
	cam->picture.hue = (u16)hue[dev_nr];
	cam->picture.colour = (u16)colour[dev_nr];
	cam->picture.contrast = (u16)contrast[dev_nr];
	cam->picture.whiteness = (u16)whiteness[dev_nr];
	if (w9968cf_valid_palette((u16)force_palette[dev_nr])) {
		cam->picture.palette = (u16)force_palette[dev_nr];
		cam->force_palette = 1;
	} else {
		cam->force_palette = 0;
		if (cam->decompression == 0)
			cam->picture.palette = W9968CF_PALETTE_DECOMP_OFF;
		else if (cam->decompression == 1)
			cam->picture.palette = W9968CF_PALETTE_DECOMP_FORCE;
		else
			cam->picture.palette = W9968CF_PALETTE_DECOMP_ON;
	}

	cam->force_rgb = (force_rgb[dev_nr] == 0 || force_rgb[dev_nr] == 1)
	                 ? (u8)force_rgb[dev_nr] : W9968CF_FORCE_RGB;

	cam->window.x = 0;
	cam->window.y = 0;
	cam->window.width = W9968CF_WIDTH;
	cam->window.height = W9968CF_HEIGHT;
	cam->window.chromakey = 0;
	cam->window.clipcount = 0;
	cam->window.flags = 0;

	/* If the video post-processing module is not present, some paramaters
	   must be overridden: */
	if (!w9968cf_vppmod_present) {
		if (cam->decompression == 1)
			cam->decompression = 2;
		cam->upscaling = 0;
		if (cam->picture.palette != VIDEO_PALETTE_UYVY)
			cam->force_palette = 0;
		cam->picture.palette = VIDEO_PALETTE_UYVY;
	}

	cam->picture.depth = w9968cf_valid_depth(cam->picture.palette);

	DBG(3, "%s configured with settings #%d:", 
	    symbolic(camlist, cam->id), dev_nr)
	
	DBG(3, "- Data packet size for USB isochrnous transfer: %d bytes.",
	    wMaxPacketSize[cam->altsetting-1])
	
	DBG(3, "- Number of requested video frame buffers: %d", 
	    cam->max_buffers)

	if (cam->double_buffer)
		DBG(3, "- Hardware double buffering enabled.")
	else 
		DBG(3, "- Hardware double buffering disabled.")

	if (cam->filter_type == 0)
		DBG(3, "- Video filtering disabled.")
	else if (cam->filter_type == 1)
		DBG(3, "- Video filtering enabled: type 1-2-1.")
	else if (cam->filter_type == 2)
		DBG(3, "- Video filtering enabled: type 2-3-6-3-2.")

	if (cam->clamping)
		DBG(3, "- Video data clamping (CCIR-601 format) enabled.")
	else
		DBG(3, "- Video data clamping (CCIR-601 format) disabled.")

	if (cam->largeview)
		DBG(3, "- Large view enabled.")
	else
		DBG(3, "- Large view disabled.")

	if ((cam->decompression) == 0 && (!cam->force_palette))
		DBG(3, "- Decompression disabled.")
	else if ((cam->decompression) == 1 && (!cam->force_palette))
		DBG(3, "- Decompression forced.")
	else if ((cam->decompression) == 2 && (!cam->force_palette))
		DBG(3, "- Decompression allowed.")

	if (cam->upscaling)
		DBG(3, "- Software image scaling enabled.")
	else
		DBG(3, "- Software image scaling disabled.")

	if (cam->force_palette)
		DBG(3, "- Image palette forced to %s.",
		    symbolic(v4l1_plist, cam->picture.palette))

	if (cam->force_rgb)
		DBG(3, "- RGB component ordering will be used instead of BGR.")

	if (cam->auto_brt)
		DBG(3, "- Auto brightness enabled.")
	else
		DBG(3, "- Auto brightness disabled.")

	if (cam->auto_exp)
		DBG(3, "- Auto exposure enabled.")
	else
		DBG(3, "- Auto exposure disabled.")

	if (cam->backlight)
		DBG(3, "- Backlight exposure algorithm enabled.")
	else
		DBG(3, "- Backlight exposure algorithm disabled.")

	if (cam->mirror)
		DBG(3, "- Mirror enabled.")
	else
		DBG(3, "- Mirror disabled.")

	if (cam->bandfilt)
		DBG(3, "- Banding filter enabled.")
	else
		DBG(3, "- Banding filter disabled.")

	DBG(3, "- Power lighting frequency: %d", cam->lightfreq)

	if (cam->clockdiv == -1)
		DBG(3, "- Automatic clock divisor enabled.")
	else
		DBG(3, "- Clock divisor: %d", cam->clockdiv)

	if (cam->monochrome)
		DBG(3, "- CMOS sensor used as monochrome.")
	else
		DBG(3, "- CMOS sensor not used as monochrome.")
}


/*--------------------------------------------------------------------------
  Release the resources used by the driver.
  This function is called on disconnect 
  (or on close if deallocation has been deferred)
  --------------------------------------------------------------------------*/
static void w9968cf_release_resources(struct w9968cf_device* cam)
{
	down(&w9968cf_devlist_sem);

	DBG(2, "V4L device deregistered: /dev/video%d", cam->v4ldev->minor)

#ifdef CONFIG_VIDEO_PROC_FS
	w9968cf_proc_destroy_dev(cam);
#endif
	video_unregister_device(cam->v4ldev);
	list_del(&cam->v4llist);
	i2c_del_adapter(&cam->i2c_adapter);
	w9968cf_deallocate_memory(cam);
	kfree(cam->control_buffer);
	kfree(cam->data_buffer);

	up(&w9968cf_devlist_sem);

	DBG(5, "Resources released.")
}



/****************************************************************************
 * Video4Linux interface                                                    *
 ****************************************************************************/

static int w9968cf_open(struct inode* inode, struct file* filp)
{
	struct w9968cf_device* cam;
	int err;

	cam = (struct w9968cf_device*)video_get_drvdata(video_devdata(filp));

	down(&cam->dev_sem);

	if (cam->sensor == CC_UNKNOWN) {
		DBG(2, "No supported CMOS sensor has been detected by the "
		       "'ovcamchip' module for the %s (/dev/video%d). Make "
		       "sure it is loaded *before* the 'w9968cf' module.", 
		    symbolic(camlist, cam->id), cam->v4ldev->minor)
		up(&cam->dev_sem);
		return -ENODEV;
	}

	while (cam->users) {
		DBG(2, "%s (/dev/video%d) has been already occupied by '%s'.",
		    symbolic(camlist, cam->id),cam->v4ldev->minor,cam->command)
		if ((filp->f_flags & O_NONBLOCK)||(filp->f_flags & O_NDELAY)) {
			up(&cam->dev_sem);
			return -EWOULDBLOCK;
		}
		up(&cam->dev_sem);
		err = wait_event_interruptible(cam->open, cam->disconnected ||
		                               (cam->users == 0));
		if (err)
			return err;
		if (cam->disconnected)
			return -ENODEV;
		down(&cam->dev_sem);
	}

	DBG(5, "Opening the %s, /dev/video%d ...",
	    symbolic(camlist, cam->id), cam->v4ldev->minor)

	cam->streaming = 0;
	cam->misconfigured = 0;

	if (!w9968cf_vppmod_present)
		w9968cf_vppmod_detect();

	if ((err = w9968cf_allocate_memory(cam)))
		goto deallocate_memory;

	if ((err = w9968cf_init_chip(cam)))
		goto deallocate_memory;

	if ((err = w9968cf_start_transfer(cam)))
		goto deallocate_memory;

	filp->private_data = cam;

	cam->users++;
	strcpy(cam->command, current->comm);

	init_waitqueue_head(&cam->wait_queue);

	up(&cam->dev_sem);

	DBG(5, "Video device is open.")
	return 0;

deallocate_memory:
	w9968cf_deallocate_memory(cam);
	DBG(2, "Failed to open the video device.")
	up(&cam->dev_sem);
	return err;
}


static int w9968cf_release(struct inode* inode, struct file* filp)
{
	struct w9968cf_device* cam;

	cam = (struct w9968cf_device*)video_get_drvdata(video_devdata(filp));

	down(&cam->dev_sem); /* prevent disconnect() to be called */

	w9968cf_stop_transfer(cam);

	if (cam->disconnected) {
		w9968cf_release_resources(cam);
		up(&cam->dev_sem);
		kfree(cam);
		return 0;
	}

	cam->users--;
	w9968cf_deallocate_memory(cam);

	if (waitqueue_active(&cam->open))
		wake_up_interruptible(&cam->open);

	DBG(5, "Video device closed.")
	up(&cam->dev_sem);
	return 0;
}


static ssize_t
w9968cf_read(struct file* filp, char* buf, size_t count, loff_t* f_pos)
{
	struct w9968cf_device* cam;
	struct w9968cf_frame_t* fr;
	int err = 0;

	cam = (struct w9968cf_device*)video_get_drvdata(video_devdata(filp));

	if (filp->f_flags & O_NONBLOCK)
		return -EWOULDBLOCK;

	if (down_interruptible(&cam->fileop_sem))
		return -ERESTARTSYS;

	if (cam->disconnected) {
		DBG(2, "Device not present.")
		up(&cam->fileop_sem);
		return -ENODEV;
	}

	if (cam->misconfigured) {
		DBG(2, "The camera is misconfigured. Close and open it again.")
		up(&cam->fileop_sem);
		return -EIO;
	}

	if (!cam->frame[0].queued)
		w9968cf_push_frame(cam, 0);

	if (!cam->frame[1].queued)
		w9968cf_push_frame(cam, 1);

	err = wait_event_interruptible(cam->wait_queue,
	                               cam->frame[0].status == F_READY ||
	                               cam->frame[1].status == F_READY ||
	                               cam->disconnected);
	if (err) {
		up(&cam->fileop_sem);
		return err;
	}
	if (cam->disconnected) {
		up(&cam->fileop_sem);
		return -ENODEV;
	}

	fr = (cam->frame[0].status == F_READY) ? &cam->frame[0]:&cam->frame[1];

	if (w9968cf_vppmod_present)
		w9968cf_postprocess_frame(cam, fr);

	if (count > fr->length)
		count = fr->length;

	if (copy_to_user(buf, fr->buffer, count)) {
		fr->status = F_UNUSED;
		up(&cam->fileop_sem);
		return -EFAULT;
	}
	*f_pos += count;

	fr->status = F_UNUSED;

	DBG(5, "%d bytes read.", count)

	up(&cam->fileop_sem);
	return count;
}


static int w9968cf_mmap(struct file* filp, struct vm_area_struct *vma)
{
	struct w9968cf_device* cam = (struct w9968cf_device*)
	                             video_get_drvdata(video_devdata(filp));
	unsigned long vsize = vma->vm_end - vma->vm_start,
	              psize = cam->nbuffers * w9968cf_get_max_bufsize(cam),
	              start = vma->vm_start,
	              pos = (unsigned long)cam->frame[0].buffer,
	              page;

	if (cam->disconnected) {
		DBG(2, "Device not present.")
		return -ENODEV;
	}

	if (cam->misconfigured) {
		DBG(2, "The camera is misconfigured. Close and open it again.")
		return -EIO;
	}

	PDBGG("mmapping %li bytes...", vsize)

        if (vsize > psize - (vma->vm_pgoff << PAGE_SHIFT))
		return -EAGAIN;

	while (vsize > 0) {
		page = kvirt_to_pa(pos) + vma->vm_pgoff;
		if(remap_page_range(start, page, PAGE_SIZE, vma->vm_page_prot))
			return -EAGAIN;
		start += PAGE_SIZE;
		pos += PAGE_SIZE;
		vsize = (vsize > PAGE_SIZE) ? vsize-PAGE_SIZE : 0;
	}

	DBG(5, "mmap method successfully called.")
	return 0;
}


static int
w9968cf_ioctl(struct inode* inode, struct file* filp,
              unsigned int cmd, unsigned long arg)
{
	struct w9968cf_device* cam;
	int err;

	cam = (struct w9968cf_device*)video_get_drvdata(video_devdata(filp));

	if (down_interruptible(&cam->fileop_sem))
		return -ERESTARTSYS;

	if (cam->disconnected) {
		DBG(2, "Device not present.")
		up(&cam->fileop_sem);
		return -ENODEV;
	}

	if (cam->misconfigured) {
		DBG(2, "The camera is misconfigured. Close and open it again.")
		up(&cam->fileop_sem);
		return -EIO;
	}

	err = w9968cf_v4l_ioctl(inode, filp, cmd, (void* )arg);

	up(&cam->fileop_sem);
	return err;
}


static int 
w9968cf_v4l_ioctl(struct inode* inode, struct file* filp,
                  unsigned int cmd, void* arg)
{
	struct w9968cf_device* cam;
	const char* v4l1_ioctls[] = {
		"?", "CGAP", "GCHAN", "SCHAN", "GTUNER", "STUNER", 
		"GPICT", "SPICT", "CCAPTURE", "GWIN", "SWIN", "GFBUF",
		"SFBUF", "KEY", "GFREQ", "SFREQ", "GAUDIO", "SAUDIO",
		"SYNC", "MCAPTURE", "GMBUF", "GUNIT", "GCAPTURE", "SCAPTURE",
		"SPLAYMODE", "SWRITEMODE", "GPLAYINFO", "SMICROCODE", 
		"GVBIFMT", "SVBIFMT" 
	};

	#define V4L1_IOCTL(cmd) \
	        ((_IOC_NR((cmd)) < sizeof(v4l1_ioctls)/sizeof(char*)) ? \
	        v4l1_ioctls[_IOC_NR((cmd))] : "?")

	cam = (struct w9968cf_device*)video_get_drvdata(video_devdata(filp));

	switch (cmd) {

	case VIDIOCGCAP: /* get video capability */
	{
		struct video_capability cap = {
			.type = VID_TYPE_CAPTURE | VID_TYPE_SCALES,
			.channels = 1,
			.audios = 0,
			.minwidth = cam->minwidth,
			.minheight = cam->minheight,
		};
		sprintf(cap.name, "W996[87]CF USB Camera #%d", 
		        cam->v4ldev->minor);
		cap.maxwidth = (cam->upscaling && w9968cf_vppmod_present)
		               ? W9968CF_MAX_WIDTH : cam->maxwidth;
		cap.maxheight = (cam->upscaling && w9968cf_vppmod_present)
		                ? W9968CF_MAX_HEIGHT : cam->maxheight;

		if (copy_to_user(arg, &cap, sizeof(cap)))
			return -EFAULT;

		DBG(5, "VIDIOCGCAP successfully called.")
		return 0;
	}

	case VIDIOCGCHAN: /* get video channel informations */
	{
		struct video_channel chan;
		if (copy_from_user(&chan, arg, sizeof(chan)))
			return -EFAULT;

		if (chan.channel != 0)
			return -EINVAL;

		strcpy(chan.name, "Camera");
		chan.tuners = 0;
		chan.flags = 0;
		chan.type = VIDEO_TYPE_CAMERA;
		chan.norm = VIDEO_MODE_AUTO;

		if (copy_to_user(arg, &chan, sizeof(chan)))
			return -EFAULT;

		DBG(5, "VIDIOCGCHAN successfully called.")
		return 0;
	}

	case VIDIOCSCHAN: /* set active channel */
	{
		struct video_channel chan;

		if (copy_from_user(&chan, arg, sizeof(chan)))
			return -EFAULT;

		if (chan.channel != 0)
			return -EINVAL;

		DBG(5, "VIDIOCSCHAN successfully called.")
		return 0;
	}

	case VIDIOCGPICT: /* get image properties of the picture */
	{
		if (w9968cf_sensor_get_picture(cam))
			return -EIO;

		if (copy_to_user(arg, &cam->picture, sizeof(cam->picture)))
			return -EFAULT;

		DBG(5, "VIDIOCGPICT successfully called.")
		return 0;
	}

	case VIDIOCSPICT: /* change picture settings */
	{
		struct video_picture pict;
		int err = 0;

		if (copy_from_user(&pict, arg, sizeof(pict)))
			return -EFAULT;

		if ( (cam->force_palette || !w9968cf_vppmod_present) 
		     && pict.palette != cam->picture.palette ) {
			DBG(4, "Palette %s rejected. Only %s is allowed.",
			    symbolic(v4l1_plist, pict.palette),
			    symbolic(v4l1_plist, cam->picture.palette))
			return -EINVAL;
		}

		if (!w9968cf_valid_palette(pict.palette)) {
			DBG(4, "Palette %s not supported. VIDIOCSPICT failed.",
			    symbolic(v4l1_plist, pict.palette))
			return -EINVAL;
		}

		if (!cam->force_palette) {
		   if (cam->decompression == 0) {
		      if (w9968cf_need_decompression(pict.palette)) {
		         DBG(4, "Decompression disabled: palette %s is not "
		                "allowed. VIDIOCSPICT failed.",
		             symbolic(v4l1_plist, pict.palette))
		         return -EINVAL;
		      }
		   } else if (cam->decompression == 1) {
		      if (!w9968cf_need_decompression(pict.palette)) {
		         DBG(4, "Decompression forced: palette %s is not "
		                "allowed. VIDIOCSPICT failed.",
		             symbolic(v4l1_plist, pict.palette))
		         return -EINVAL;
		      }
		   }
		}

		if (pict.depth != w9968cf_valid_depth(pict.palette)) {
			DBG(4, "Requested depth %d bpp is not valid for %s "
			       "palette: ignored and changed to %d bpp.", 
			    pict.depth, symbolic(v4l1_plist, pict.palette),
			    w9968cf_valid_depth(pict.palette))
			pict.depth = w9968cf_valid_depth(pict.palette);
		}

		if (pict.palette != cam->picture.palette) {
			if(*cam->requested_frame
			   || cam->frame_current->queued) {
				err = wait_event_interruptible
				      ( cam->wait_queue,
				        cam->disconnected ||
				        (!*cam->requested_frame &&
				         !cam->frame_current->queued) );
				if (err)
					return err;
				if (cam->disconnected)
					return -ENODEV;
			}

			if (w9968cf_stop_transfer(cam))
				goto ioctl_fail;

			if (w9968cf_set_picture(cam, pict))
				goto ioctl_fail;

			if (w9968cf_start_transfer(cam))
				goto ioctl_fail;

		} else if (w9968cf_sensor_update_picture(cam, pict))
			return -EIO;


		DBG(5, "VIDIOCSPICT successfully called.")
		return 0;
	}

	case VIDIOCSWIN: /* set capture area */
	{
		struct video_window win;
		int err = 0;

		if (copy_from_user(&win, arg, sizeof(win)))
			return -EFAULT;

		DBG(6, "VIDIOCSWIN called: clipcount=%d, flags=%d, "
		       "x=%d, y=%d, %dx%d", win.clipcount, win.flags,
		    win.x, win.y, win.width, win.height)

		if (win.clipcount != 0 || win.flags != 0)
			return -EINVAL;

		if ((err = w9968cf_adjust_window_size(cam, (u16*)&win.width,
		                                      (u16*)&win.height))) {
			DBG(4, "Resolution not supported (%dx%d)."
			       "VIDIOCSWIN failed.", win.width, win.height)
			return err;
		}

		if (win.x != cam->window.x ||
		    win.y != cam->window.y ||
		    win.width != cam->window.width ||
		    win.height != cam->window.height) {
			if(*cam->requested_frame
			   || cam->frame_current->queued) {
				err = wait_event_interruptible
				      ( cam->wait_queue,
				        cam->disconnected ||
				        (!*cam->requested_frame &&
				         !cam->frame_current->queued) );
				if (err)
					return err;
				if (cam->disconnected)
					return -ENODEV;
			}

			if (w9968cf_stop_transfer(cam))
				goto ioctl_fail;

			/* This _must_ be called before set_window() */
			if (w9968cf_set_picture(cam, cam->picture))
				goto ioctl_fail;

			if (w9968cf_set_window(cam, win))
				goto ioctl_fail;

			if (w9968cf_start_transfer(cam))
				goto ioctl_fail;
		}

		DBG(5, "VIDIOCSWIN successfully called. ")
		return 0;
	}

	case VIDIOCGWIN: /* get current window properties */
	{
		if (copy_to_user(arg,&cam->window,sizeof(struct video_window)))
			return -EFAULT;

		DBG(5, "VIDIOCGWIN successfully called.")
		return 0;
	}

	case VIDIOCGMBUF: /* request for memory (mapped) buffer */
	{
		struct video_mbuf mbuf;
		u8 i;

		mbuf.size = cam->nbuffers * w9968cf_get_max_bufsize(cam);
		mbuf.frames = cam->nbuffers;
		for (i = 0; i < cam->nbuffers; i++)
			mbuf.offsets[i] = (unsigned long)cam->frame[i].buffer -
			                  (unsigned long)cam->frame[0].buffer;

		if (copy_to_user(arg, &mbuf, sizeof(mbuf)))
			return -EFAULT;

		DBG(5, "VIDIOCGMBUF successfully called.")
		return 0;
	}

	case VIDIOCMCAPTURE: /* start the capture to a frame */
	{
		struct video_mmap mmap;
		struct w9968cf_frame_t* fr;
		int err = 0;

		if (copy_from_user(&mmap, arg, sizeof(mmap)))
			return -EFAULT;

		DBG(6, "VIDIOCMCAPTURE called: frame #%d, format=%s, %dx%d",
		    mmap.frame, symbolic(v4l1_plist, mmap.format), 
		    mmap.width, mmap.height)

		if (mmap.frame >= cam->nbuffers) {
			DBG(4, "Invalid frame number (%d). "
			       "VIDIOCMCAPTURE failed.", mmap.frame)
			return -EINVAL;
		}

		if (mmap.format!=cam->picture.palette && 
		    (cam->force_palette || !w9968cf_vppmod_present)) {
			DBG(4, "Palette %s rejected. Only %s is allowed.",
			    symbolic(v4l1_plist, mmap.format),
			    symbolic(v4l1_plist, cam->picture.palette))
			return -EINVAL;
		}

		if (!w9968cf_valid_palette(mmap.format)) {
			DBG(4, "Palette %s not supported. "
			       "VIDIOCMCAPTURE failed.", 
			    symbolic(v4l1_plist, mmap.format))
			return -EINVAL;
		}

		if (!cam->force_palette) {
		   if (cam->decompression == 0) {
		      if (w9968cf_need_decompression(mmap.format)) {
		         DBG(4, "Decompression disabled: palette %s is not "
		                "allowed. VIDIOCSPICT failed.",
		             symbolic(v4l1_plist, mmap.format))
		         return -EINVAL;
		      }
		   } else if (cam->decompression == 1) {
		      if (!w9968cf_need_decompression(mmap.format)) {
		         DBG(4, "Decompression forced: palette %s is not "
		                "allowed. VIDIOCSPICT failed.",
		             symbolic(v4l1_plist, mmap.format))
		         return -EINVAL;
		      }
		   }
		}

		if ((err = w9968cf_adjust_window_size(cam, (u16*)&mmap.width, 
		                                      (u16*)&mmap.height))) {
			DBG(4, "Resolution not supported (%dx%d). "
			       "VIDIOCMCAPTURE failed.",
			    mmap.width, mmap.height)
			return err;
		}

		fr = &cam->frame[mmap.frame];

		if (mmap.width  != cam->window.width ||
		    mmap.height != cam->window.height ||
		    mmap.format != cam->picture.palette) {

			struct video_window win;
			struct video_picture pict;

			if(*cam->requested_frame
			   || cam->frame_current->queued) {
				DBG(6, "VIDIOCMCAPTURE. Change settings for "
				       "frame #%d: %dx%d, format %s. Wait...",
				    mmap.frame, mmap.width, mmap.height,
			            symbolic(v4l1_plist, mmap.format))
				err = wait_event_interruptible
				      ( cam->wait_queue,
				        cam->disconnected ||
				        (!*cam->requested_frame &&
				         !cam->frame_current->queued) );
				if (err)
					return err;
				if (cam->disconnected)
					return -ENODEV;
			}

			memcpy(&win, &cam->window, sizeof(win));
			memcpy(&pict, &cam->picture, sizeof(pict));
			win.width = mmap.width;
			win.height = mmap.height;
			pict.palette = mmap.format;

			if (w9968cf_stop_transfer(cam))
				goto ioctl_fail;

			/* This before set_window */
			if (w9968cf_set_picture(cam, pict)) 
				goto ioctl_fail;

			if (w9968cf_set_window(cam, win))
				goto ioctl_fail;

			if (w9968cf_start_transfer(cam))
				goto ioctl_fail;

		} else 	if (fr->queued) {

			DBG(6, "Wait until frame #%d is free.", mmap.frame)
			
			err = wait_event_interruptible(cam->wait_queue, 
			                               cam->disconnected ||
			                               (!fr->queued));
			if (err)
				return err;
			if (cam->disconnected)
				return -ENODEV;
		}

		w9968cf_push_frame(cam, mmap.frame);
		DBG(5, "VIDIOCMCAPTURE(%d): successfully called.", mmap.frame)
		return 0;
	}

	case VIDIOCSYNC: /* wait until the capture of a frame is finished */
	{
		unsigned int f_num;
		struct w9968cf_frame_t* fr;
		int err = 0;

		if (copy_from_user(&f_num, arg, sizeof(f_num)))
			return -EFAULT;

		if (f_num >= cam->nbuffers) {
			DBG(4, "Invalid frame number (%d). "
			       "VIDIOCMCAPTURE failed.", f_num)
			return -EINVAL;
		}

		DBG(6, "VIDIOCSYNC called for frame #%d", f_num)

		fr = &cam->frame[f_num];

		switch (fr->status) {
		case F_UNUSED:
			if (!fr->queued) {
				DBG(4, "VIDIOSYNC: Frame #%d not requested!",
				    f_num)
				return -EFAULT;
			}
		case F_ERROR:
		case F_GRABBING:
			err = wait_event_interruptible(cam->wait_queue, 
			                               (fr->status == F_READY)
			                               || cam->disconnected);
			if (err)
				return err;
			if (cam->disconnected)
				return -ENODEV;
			break;
		case F_READY:
			break;
		}

		if (w9968cf_vppmod_present)
			w9968cf_postprocess_frame(cam, fr);

		fr->status = F_UNUSED;

		DBG(5, "VIDIOCSYNC(%d) successfully called.", f_num)
		return 0;
	}

	case VIDIOCGUNIT:/* report the unit numbers of the associated devices*/
	{
		struct video_unit unit = {
			.video = cam->v4ldev->minor,
			.vbi = VIDEO_NO_UNIT,
			.radio = VIDEO_NO_UNIT,
			.audio = VIDEO_NO_UNIT,
			.teletext = VIDEO_NO_UNIT,
		};

		if (copy_to_user(arg, &unit, sizeof(unit)))
			return -EFAULT;

		DBG(5, "VIDIOCGUNIT successfully called.")
		return 0;
	}

	case VIDIOCKEY:
		return 0;

	case VIDIOCGFBUF:
	{
		struct video_buffer* buffer = (struct video_buffer*)arg;

		if (clear_user(buffer, sizeof(struct video_buffer)))
			return -EFAULT;

		DBG(5, "VIDIOCGFBUF successfully called.")
		return 0;
	}

	case VIDIOCGTUNER:
	{
		struct video_tuner tuner;
		if (copy_from_user(&tuner, arg, sizeof(tuner)))
			return -EFAULT;

		if (tuner.tuner != 0)
			return -EINVAL;

		strcpy(tuner.name, "no_tuner");
		tuner.rangelow = 0;
		tuner.rangehigh = 0;
		tuner.flags = VIDEO_TUNER_NORM;
		tuner.mode = VIDEO_MODE_AUTO;
		tuner.signal = 0xffff;

		if (copy_to_user(arg, &tuner, sizeof(tuner)))
			return -EFAULT;

		DBG(5, "VIDIOCGTUNER successfully called.")
		return 0;
	}

	case VIDIOCSTUNER:
	{
		struct video_tuner tuner;
		if (copy_from_user(&tuner, arg, sizeof(tuner)))
			return -EFAULT;

		if (tuner.tuner != 0)
			return -EINVAL;

		if (tuner.mode != VIDEO_MODE_AUTO)
			return -EINVAL;

		DBG(5, "VIDIOCSTUNER successfully called.")
		return 0;
	}

	case VIDIOCSFBUF:
	case VIDIOCCAPTURE:
	case VIDIOCGFREQ:
	case VIDIOCSFREQ:
	case VIDIOCGAUDIO:
	case VIDIOCSAUDIO:
	case VIDIOCSPLAYMODE:
	case VIDIOCSWRITEMODE:
	case VIDIOCGPLAYINFO:
	case VIDIOCSMICROCODE:
	case VIDIOCGVBIFMT:
	case VIDIOCSVBIFMT:
		DBG(4, "Unsupported V4L1 IOCtl: VIDIOC%s "
		       "(type 0x%01X, "
		       "n. 0x%01X, "
		       "dir. 0x%01X, " 
		       "size 0x%02X).",
		    V4L1_IOCTL(cmd),
		    _IOC_TYPE(cmd),_IOC_NR(cmd),_IOC_DIR(cmd),_IOC_SIZE(cmd))

		return -EINVAL;

	default:
		DBG(4, "Invalid V4L1 IOCtl: VIDIOC%s "
		       "type 0x%01X, "
		       "n. 0x%01X, "
		       "dir. 0x%01X, "
		       "size 0x%02X.",
		    V4L1_IOCTL(cmd),
		    _IOC_TYPE(cmd),_IOC_NR(cmd),_IOC_DIR(cmd),_IOC_SIZE(cmd))

		return -ENOIOCTLCMD;

	} /* end of switch */

ioctl_fail:
	cam->misconfigured = 1;
	DBG(1, "VIDIOC%s failed because of hardware problems. "
	       "To use the camera, close and open it again.", V4L1_IOCTL(cmd))
	return -EFAULT;
}


static struct file_operations w9968cf_fops = {
	.owner =   THIS_MODULE,
	.open =    w9968cf_open,
	.release = w9968cf_release,
	.read =    w9968cf_read,
	.ioctl =   w9968cf_ioctl,
	.mmap =    w9968cf_mmap,
	.llseek =  no_llseek,
};



/****************************************************************************
 * USB probe and V4L registration, disconnect and id_table[] definition     *
 ****************************************************************************/

static void*
w9968cf_usb_probe(struct usb_device* udev,
                  unsigned int ifnum, const struct usb_device_id* id)
{
	struct w9968cf_device* cam;
	int err = 0;
	enum w9968cf_model_id mod_id;
	struct list_head* ptr;
	u8 sc = 0; /* number of simultaneous cameras */
	static unsigned short dev_nr = 0; /* we are handling device number n */

	if (udev->descriptor.idVendor  == winbond_id_table[0].idVendor &&
	    udev->descriptor.idProduct == winbond_id_table[0].idProduct)
		mod_id = W9968CF_MOD_CLVBWGP; /* see camlist[] table */

	else if (udev->descriptor.idVendor  == winbond_id_table[1].idVendor &&
	         udev->descriptor.idProduct == winbond_id_table[1].idProduct)
		mod_id = W9968CF_MOD_GENERIC; /* see camlist[] table */

	else
		return NULL;

	/* We don't handle multi-config cameras */
	if (udev->descriptor.bNumConfigurations != 1)
		return NULL;

	DBG(2, "%s detected.", symbolic(camlist, mod_id))

	if (simcams > W9968CF_MAX_DEVICES)
		simcams = W9968CF_SIMCAMS;

	/* How many cameras are connected ? */
	down(&w9968cf_devlist_sem);
	list_for_each(ptr, &w9968cf_dev_list)
		sc++;
	up(&w9968cf_devlist_sem);

	if (sc >= simcams) {
		DBG(2, "Device rejected: too many connected cameras "
		       "(max. %d)", simcams)
		return NULL;
	}

	cam = (struct w9968cf_device*)
	          kmalloc(sizeof(struct w9968cf_device), GFP_KERNEL);

	if (!cam) {
		DBG(1, "Couldn't allocate %d bytes of kernel memory.",
		    sizeof(struct w9968cf_device))
		err = -ENOMEM;
		goto fail;
	}
	memset(cam, 0, sizeof(*cam));

	init_MUTEX(&cam->dev_sem);
	down(&cam->dev_sem);

	/* Allocate 2 bytes of memory for camera control USB transfers */
	if (!(cam->control_buffer = (u16*)kmalloc(2, GFP_KERNEL))) {
		DBG(1,"Couldn't allocate memory for camera control transfers.")
		err = -ENOMEM;
		goto fail;
	}
	memset(cam->control_buffer, 0, 2);

	/* Allocate 8 bytes of memory for USB data transfers to the FSB */
	if (!(cam->data_buffer = (u16*)kmalloc(8, GFP_KERNEL))) {
		DBG(1, "Couldn't allocate memory for data "
		       "transfers to the FSB.")
		err = -ENOMEM;
		goto fail;
	}
	memset(cam->data_buffer, 0, 8);

	/* Register the V4L device */
	cam->v4ldev = video_device_alloc();
	if (!cam->v4ldev) {
		DBG(1, "Could not allocate memory for a V4L structure.")
		err = -ENOMEM;
		goto fail;
	}

	strcpy(cam->v4ldev->name, symbolic(camlist, mod_id));
	cam->v4ldev->owner = THIS_MODULE;
	cam->v4ldev->type = VID_TYPE_CAPTURE | VID_TYPE_SCALES;
	cam->v4ldev->hardware = VID_HARDWARE_W9968CF;
	cam->v4ldev->fops = &w9968cf_fops;
	cam->v4ldev->minor = video_nr[dev_nr];
	cam->v4ldev->release = video_device_release;
	video_set_drvdata(cam->v4ldev, cam);

	err = video_register_device(cam->v4ldev, VFL_TYPE_GRABBER,
	                            video_nr[dev_nr]);
	if (err) {
		DBG(1, "V4L device registration failed.")
		if (err == -ENFILE && video_nr[dev_nr] == -1)
			DBG(2, "Couldn't find a free /dev/videoX node.")
		video_nr[dev_nr] = -1;
		dev_nr = (dev_nr < W9968CF_MAX_DEVICES-1) ? dev_nr+1 : 0;
		goto fail;
	}

	DBG(2, "V4L device registered as /dev/video%d", cam->v4ldev->minor)

	/* Set some basic constants */
	w9968cf_configure_camera(cam, udev, mod_id, dev_nr);

	/* Ok, add a new entry into the list of V4L registered devices */
	down(&w9968cf_devlist_sem);
	list_add(&cam->v4llist, &w9968cf_dev_list);
	up(&w9968cf_devlist_sem);
	dev_nr = (dev_nr < W9968CF_MAX_DEVICES-1) ? dev_nr+1 : 0;

	w9968cf_turn_on_led(cam);

	w9968cf_i2c_init(cam);

#ifdef CONFIG_VIDEO_PROC_FS
	w9968cf_proc_create_dev(cam);
#endif

	up(&cam->dev_sem);

	return (void*)cam;

fail: /* Free unused memory */
	if (cam) {
		if (cam->control_buffer)
			kfree(cam->control_buffer);
		if (cam->data_buffer)
			kfree(cam->data_buffer);
		if (cam->v4ldev)
			video_device_release(cam->v4ldev);
		up(&cam->dev_sem);
		kfree(cam);
	}
	return NULL;
}


static void
w9968cf_usb_disconnect(struct usb_device* udev, void* drv_context)
{
	struct w9968cf_device* cam = (struct w9968cf_device*)drv_context;

	if (cam) {
		/* Prevent concurrent accesses to data */
		down(&cam->dev_sem); 

		cam->streaming = 0;
		cam->disconnected = 1;

		DBG(2, "Disconnecting %s...", symbolic(camlist, cam->id))

		if (waitqueue_active(&cam->open))
			wake_up_interruptible(&cam->open);

		if (cam->users) {
			DBG(2, "The device is open (/dev/video%d)! "
			       "Process name: %s. Deregistration and memory "
			       "deallocation are deferred on close.",
			    cam->v4ldev->minor, cam->command)

			cam->misconfigured = 1;

			if (waitqueue_active(&cam->wait_queue))
				wake_up_interruptible(&cam->wait_queue);
		} else
			w9968cf_release_resources(cam);

		up(&cam->dev_sem);

		if (!cam->users)
			kfree(cam);
	}
}


static struct usb_driver w9968cf_usb_driver = {
	.owner =      THIS_MODULE,
	.name =       "w9968cf",
	.id_table =   winbond_id_table,
	.probe =      w9968cf_usb_probe,
	.disconnect = w9968cf_usb_disconnect,
};



/****************************************************************************
 * Module init, exit and intermodule communication                          *
 ****************************************************************************/

static int w9968cf_vppmod_detect(void)
{
	w9968cf_vpp_init_decoder = inter_module_get("w9968cf_init_decoder");

	if (!w9968cf_vpp_init_decoder) {
		if (vppmod_load)
			w9968cf_vpp_init_decoder = inter_module_get_request
		                                  ( "w9968cf_init_decoder",
			                            "w9968cf-vpp" );
		if (!w9968cf_vpp_init_decoder) {
			w9968cf_vppmod_present = 0;
			DBG(4, "Video post-processing module not detected.")
			return -ENODEV;
		}
	}

	w9968cf_vpp_check_headers = inter_module_get("w9968cf_check_headers");
	w9968cf_vpp_decode = inter_module_get("w9968cf_decode");
	w9968cf_vpp_swap_yuvbytes = inter_module_get("w9968cf_swap_yuvbytes");
	w9968cf_vpp_uyvy_to_rgbx = inter_module_get("w9968cf_uyvy_to_rgbx");
	w9968cf_vpp_scale_up = inter_module_get("w9968cf_scale_up");

	w9968cf_vppmod_present = 1;

	/* Initialization */
	(*w9968cf_vpp_init_decoder)();

	DBG(2, "Video post-processing module detected.")
	return 0;
}


static void w9968cf_vppmod_release(void)
{
	inter_module_put("w9968cf_init_decoder");
	inter_module_put("w9968cf_check_headers");
	inter_module_put("w9968cf_decode");
	inter_module_put("w9968cf_swap_yuvbytes");
	inter_module_put("w9968cf_uyvy_to_rgbx");
	inter_module_put("w9968cf_scale_up");

	DBG(2, "Video post-processing module released.")
}


static int __init w9968cf_module_init(void)
{
	int err;

	DBG(2, W9968CF_MODULE_NAME" "W9968CF_MODULE_VERSION)
	DBG(3, W9968CF_MODULE_AUTHOR)

	init_MUTEX(&w9968cf_devlist_sem);

#ifdef CONFIG_VIDEO_PROC_FS
	w9968cf_proc_create();
#endif

	w9968cf_vppmod_detect();

	if ((err = usb_register(&w9968cf_usb_driver))) {
		if (w9968cf_vppmod_present)
			w9968cf_vppmod_release();
#ifdef CONFIG_VIDEO_PROC_FS
		w9968cf_proc_destroy();
#endif
		return err;
	}

	return 0;
}


static void __exit w9968cf_module_exit(void)
{
	/* w9968cf_usb_disconnect() will be called */
	usb_deregister(&w9968cf_usb_driver);

#ifdef CONFIG_VIDEO_PROC_FS
	w9968cf_proc_destroy();	
#endif

	if (w9968cf_vppmod_present)
		w9968cf_vppmod_release();

	DBG(2, W9968CF_MODULE_NAME" deregistered.")
}


module_init(w9968cf_module_init);
module_exit(w9968cf_module_exit);

EXPORT_NO_SYMBOLS;

/*
 *	epson1356fb.h  --  Epson SED1356 Framebuffer Driver
 *
 *	Copyright 2001, 2002, 2003 MontaVista Software Inc.
 *	Author: MontaVista Software, Inc.
 *		stevel@mvista.com or source@mvista.com
 *
 *	This program is free software; you can redistribute  it and/or modify it
 *	under  the terms of  the GNU General  Public License as published by the
 *	Free Software Foundation;  either version 2 of the  License, or (at your
 *	option) any later version.
 *
 *	THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *	WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *	MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *	NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *	NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *	USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *	ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *	THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	You should have received a copy of the  GNU General Public License along
 *	with this program; if not, write  to the Free Software Foundation, Inc.,
 *	675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifdef E1356FB_DEBUG
#define DPRINTK(a,b...) printk(KERN_DEBUG "e1356fb: %s: " a, __FUNCTION__ , ## b)
#else
#define DPRINTK(a,b...)
#endif 

#define E1356_REG_SIZE  0x200000

#define PICOS2KHZ(a) (1000000000UL/(a))
#define KHZ2PICOS(a) (1000000000UL/(a))

#define MAX_PIXCLOCK  40000 // KHz
#define NTSC_PIXCLOCK 14318 // KHz
#define PAL_PIXCLOCK  17734 // KHz

/*
 * Maximum percent errors between desired pixel clock and
 * supported pixel clock. Lower-than and higher-than desired
 * clock percent errors.
 */
#define MAX_PCLK_ERROR_LOWER  10
#define MAX_PCLK_ERROR_HIGHER -1

#define fontwidth_x8(p) (((fontwidth(p) + 7) >> 3) << 3)

/*
 * Register Structures
 */

// Basic
#define REG_BASE_BASIC     0x00
typedef struct {
	u8 rev_code;           // 00
	u8 misc;               // 01
} reg_basic_t;

// General IO Pins
#define REG_BASE_GENIO     0x04
typedef struct {
	u8 gpio_cfg;           // 04
	u8 gpio_cfg2;          // 05 SED13806
	u8 spacer[2];          // 06
	u8 gpio_ctrl;          // 08
	u8 gpio_ctrl2;         // 09 SED13806
} reg_genio_t;

// MD Config Readback
#define REG_BASE_MDCFG     0x0c
typedef struct {
	u8 md_cfg_stat0;       // 0C
	u8 md_cfg_stat1;       // 0D
} reg_mdcfg_t;

// Clock Config
#define REG_BASE_CLKCFG    0x10
typedef struct {
	u8 mem_clk_cfg;        // 10
	u8 spacer1[3];         // 11
	u8 lcd_pclk_cfg;       // 14
	u8 spacer2[3];         // 15
	u8 crttv_pclk_cfg;     // 18
	u8 spacer3[3];         // 19
	u8 mpclk_cfg;          // 1C
	u8 spacer4;            // 1D
	u8 cpu2mem_wait_sel;   // 1E
} reg_clkcfg_t;

// Memory Config
#define REG_BASE_MEMCFG    0x20
typedef struct {
	u8 mem_cfg;            // 20
	u8 dram_refresh;       // 21
	u8 spacer[8];          // 22
	u8 dram_timings_ctrl0; // 2A
	u8 dram_timings_ctrl1; // 2B
} reg_memcfg_t;

// Panel Config
#define REG_BASE_PANELCFG  0x30
typedef struct {
	u8 panel_type;         // 30
	u8 mod_rate;           // 31
} reg_panelcfg_t;

// LCD and CRTTV Display Config
#define REG_BASE_LCD_DISPCFG   0x32
#define REG_BASE_CRTTV_DISPCFG 0x50
typedef struct {
	u8 hdw;                // 32 or 50
	u8 spacer1;            // 33 or 51
	u8 hndp;               // 34 or 52
	u8 hsync_start;        // 35 or 53
	u8 hsync_pulse;        // 36 or 54
	u8 spacer2;            // 37 or 55
	u8 vdh0;               // 38 or 56
	u8 vdh1;               // 39 or 57
	u8 vndp;               // 3A or 58
	u8 vsync_start;        // 3B or 59
	u8 vsync_pulse;        // 3C or 5A
	u8 tv_output_ctrl;     // 5B (TV only)
} reg_dispcfg_t;

// LCD and CRTTV Display Mode
#define REG_BASE_LCD_DISPMODE   0x40
#define REG_BASE_CRTTV_DISPMODE 0x60
typedef struct {
	u8 disp_mode;          // 40 or 60
	u8 lcd_misc;           // 41 (LCD only)
	u8 start_addr0;        // 42 or 62
	u8 start_addr1;        // 43 or 63
	u8 start_addr2;        // 44 or 64
	u8 spacer1;            // 45 or 65
	u8 mem_addr_offset0;   // 46 or 66
	u8 mem_addr_offset1;   // 47 or 67
	u8 pixel_panning;      // 48 or 68
	u8 spacer2;            // 49 or 69
	u8 fifo_high_thresh;   // 4A or 6A
	u8 fifo_low_thresh;    // 4B or 6B
} reg_dispmode_t;

// LCD and CRTTV Ink/Cursor
#define REG_BASE_LCD_INKCURS   0x70
#define REG_BASE_CRTTV_INKCURS 0x80
typedef struct {
	u8 ctrl;               // 70 or 80
	u8 start_addr;         // 71 or 81
	u8 x_pos0;             // 72 or 82
	u8 x_pos1;             // 73 or 83
	u8 y_pos0;             // 74 or 84
	u8 y_pos1;             // 75 or 85
	u8 blue0;              // 76 or 86
	u8 green0;             // 77 or 87
	u8 red0;               // 78 or 88
	u8 spacer1;            // 79 or 89
	u8 blue1;              // 7A or 8A
	u8 green1;             // 7B or 8B
	u8 red1;               // 7C or 8C
	u8 spacer2;            // 7D or 8D
	u8 fifo;               // 7E or 8E
} reg_inkcurs_t;

// BitBlt Config
#define REG_BASE_BITBLT        0x100
typedef struct {
	u8 ctrl0;              // 100
	u8 ctrl1;              // 101
	u8 rop_code;           // 102
	u8 operation;          // 103
	u8 src_start_addr0;    // 104
	u8 src_start_addr1;    // 105
	u8 src_start_addr2;    // 106
	u8 spacer1;            // 107
	u8 dest_start_addr0;   // 108
	u8 dest_start_addr1;   // 109
	u8 dest_start_addr2;   // 10A
	u8 spacer2;            // 10B
	u8 mem_addr_offset0;   // 10C
	u8 mem_addr_offset1;   // 10D
	u8 spacer3[2];         // 10E
	u8 width0;             // 110
	u8 width1;             // 111
	u8 height0;            // 112
	u8 height1;            // 113
	u8 bg_color0;          // 114
	u8 bg_color1;          // 115
	u8 spacer4[2];         // 116
	u8 fg_color0;          // 118
	u8 fg_color1;          // 119
} reg_bitblt_t;

// LUT
#define REG_BASE_LUT           0x1e0
typedef struct {
	u8 mode;               // 1E0
	u8 spacer1;            // 1E1
	u8 addr;               // 1E2
	u8 spacer2;            // 1E3
	u8 data;               // 1E4
} reg_lut_t;

// Power Save Config
#define REG_BASE_PWRSAVE       0x1f0
typedef struct {
	u8 cfg;                // 1F0
	u8 status;             // 1F1
} reg_pwrsave_t;

// Misc
#define REG_BASE_MISC          0x1f4
typedef struct {
	u8 cpu2mem_watchdog;   // 1F4
	u8 spacer[7];          // 1F5
	u8 disp_mode;          // 1FC
} reg_misc_t;

// MediaPlug
#define REG_BASE_MEDIAPLUG     0x1000
typedef struct {
	u8 lcmd;               // 1000
	u8 spacer1;            // 1001
	u8 reserved_lcmd;      // 1002
	u8 spacer2;            // 1003
	u8 cmd;                // 1004
	u8 spacer3;            // 1005
	u8 reserved_cmd;       // 1006
	u8 spacer4;            // 1007
	u8 data;               // 1008
} reg_mediaplug_t;

// BitBlt data register. 16-bit access only
#define REG_BASE_BITBLT_DATA   0x100000

typedef struct {
	reg_basic_t* basic;
	reg_genio_t* genio;
	reg_mdcfg_t* md_cfg;
	reg_clkcfg_t* clk_cfg;
	reg_memcfg_t* mem_cfg;
	reg_panelcfg_t* panel_cfg;
	reg_dispcfg_t* lcd_cfg;
	reg_dispcfg_t* crttv_cfg;
	reg_dispmode_t* lcd_mode;
	reg_dispmode_t* crttv_mode;
	reg_inkcurs_t* lcd_inkcurs;
	reg_inkcurs_t* crttv_inkcurs;
	reg_bitblt_t* bitblt;
	reg_lut_t* lut;
	reg_pwrsave_t* pwr_save;
	reg_misc_t* misc;
	reg_mediaplug_t* mediaplug;
	u16* bitblt_data;
} e1356_reg_t;


/*--------------------------------------------------------*/

enum mem_type_t {
	MEM_TYPE_EDO_2CAS = 0,
	MEM_TYPE_FPM_2CAS,
	MEM_TYPE_EDO_2WE,
	MEM_TYPE_FPM_2WE,
	MEM_TYPE_EMBEDDED_SDRAM = 0x80
};

enum mem_smr_t {
	MEM_SMR_CBR = 0,
	MEM_SMR_SELF,
	MEM_SMR_NONE
};

enum disp_type_t {
	DISP_TYPE_LCD = 0,
	DISP_TYPE_TFT,
	DISP_TYPE_CRT,
	DISP_TYPE_PAL,
	DISP_TYPE_NTSC
};

/*
 * Maximum timing values, as determined by the SED1356 register
 * field sizes. All are indexed by display type, except
 * max_hsync_start which is first indexed by color depth,
 * then by display type.
 */
static const int max_hndp[5] = {256, 256, 512, 511, 510};
static const int max_hsync_start[2][5] = {
	{0, 252, 507, 505, 505}, // 8 bpp
	{0, 254, 509, 507, 507}  // 16 bpp
};
static const int max_hsync_width[5] = {0, 128, 128, 0, 0};
static const int max_vndp[5] = {64, 64, 128, 128, 128};
static const int max_vsync_start[5] = {0, 64, 128, 128, 128};
static const int max_vsync_width[5] = {0, 8, 8, 0, 0};

#define IS_PANEL(disp_type) \
    (disp_type == DISP_TYPE_LCD || disp_type == DISP_TYPE_TFT)
#define IS_CRT(disp_type) (disp_type == DISP_TYPE_CRT)
#define IS_TV(disp_type) \
    (disp_type == DISP_TYPE_NTSC || disp_type == DISP_TYPE_PAL)


enum tv_filters_t {
	TV_FILT_LUM = 1,
	TV_FILT_CHROM = 2,
	TV_FILT_FLICKER = 4
};

enum tv_format_t {
	TV_FMT_COMPOSITE = 0,
	TV_FMT_S_VIDEO
};


struct e1356fb_fix {
	int system;       // the number of a pre-packaged system
	phys_t regbase_phys; // phys start address of registers
	phys_t membase_phys; // phys start address of fb memory

	// Memory parameters
	int mem_speed;    // speed: 50, 60, 70, or 80 (nsec)
	int mem_type;     // mem type: EDO-2CAS, FPM-2CAS, EDO-2WE, FPM-2WE
	int mem_refresh;  // refresh rate in KHz
	int mem_smr;      // suspend mode refresh: CAS_BEFORE_RAS, SELF, or NONE
	// Clocks
	int busclk;       // BUSCLK frequency, in KHz
	int mclk;         // MCLK freq, in KHz, will either be BUSCLK or BUSCLK/2
	int clki;         // CLKI frequency, in KHz
	int clki2;        // CLKI2 frequency, in KHz

	int disp_type;    // LCD, TFT, CRT, PAL, or NTSC

	// TV Options
	u8  tv_filt;      // TV Filter mask, LUM, CHROM, and FLICKER
	int tv_fmt;       // TV output format, COMPOSITE or S_VIDEO
    
	// Panel (LCD,TFT) Options
	int panel_el;     // enable support for EL-type panels
	int panel_width;  // Panel data width: LCD: 4/8/16, TFT: 9/12/18
    
	// Misc
	int noaccel;
	int nopan;
#ifdef CONFIG_MTRR
	int nomtrr;
#endif
	int nohwcursor;
	int mmunalign;    // force unaligned returned VA in mmap()
	char fontname[40];

	char *mode_option;
};


typedef struct {
	int pixclk_d;     // Desired Pixel Clock, KHz
	int pixclk;       // Closest supported clock to desired clock, KHz
	int error;        // percent error between pixclock and pixclock_d
	int clksrc;       // equal to busclk, mclk, clki, or clki2, KHz
	int divisor;      // pixclk = clksrc/divisor, where divisor = 1,2,3, or 4
	u8  pixclk_bits;  // pixclock register value for above settings
} pixclock_info_t;


struct e1356fb_par {
	int width;
	int height;
	int width_virt;   // Width in pixels
	int height_virt;  // Height in lines
	int bpp;          // bits-per-pixel
	int Bpp;          // Bytes-per-pixel

	// Timing
	pixclock_info_t ipclk;
	int horiz_ndp;    // Horiz. Non-Display Period, pixels
	int vert_ndp;     // Vert. Non-Display Period, lines
	int hsync_pol;    // Polarity of horiz. sync signal (HRTC for CRT/TV,
	// FPLINE for TFT). 0=active lo, 1=active hi
	int hsync_start;  // Horiz. Sync Start position, pixels
	int hsync_width;  // Horiz. Sync Pulse width, pixels
	int hsync_freq;   // calculated horizontal sync frequency
	int vsync_pol;    // Polarity of vert. sync signal (VRTC for CRT/TV,
	// FPFRAME for TFT). 0=active lo, 1=active hi
	int vsync_start;  // Vert. Sync Start position, lines
	int vsync_width;  // Vert. Sync Pulse width, lines
	int vsync_freq;   // calculated vertical sync frequency

	int cmap_len;     // color-map length
};



struct fb_info_e1356 {
	struct fb_info fb_info;

	void *regbase_virt;
	unsigned long regbase_size;
	void *membase_virt;
	unsigned long fb_size;

	e1356_reg_t reg;

	void* putcs_buffer;
    
	int max_pixclock;   // Max supported pixel clock, KHz
	int open, mmaped;   // open count, is mmap'ed
	
	u8 chip_rev;
    
#ifdef CONFIG_MTRR
	int mtrr_idx;
#endif

#ifdef SHADOW_FRAME_BUFFER
	struct {
		void* fb;
		struct timer_list timer;
	} shadow;
#endif

	struct { unsigned red, green, blue, pad; } palette[256];
	struct display disp;

#if defined(FBCON_HAS_CFB16)
	u16 fbcon_cmap16[16];
#endif
    
	struct {
		int type;
		int state;
		int w,h,u;
		int x,y,redraw;
		unsigned long enable,disable;
		struct timer_list timer;
		spinlock_t lock; 
	} cursor;
 
	struct e1356fb_fix fix;
	struct e1356fb_par default_par;
	struct e1356fb_par current_par;
};


// The following are boot options for particular SED1356-based target systems

enum {
	SYS_NULL,
	SYS_PB1000,
	SYS_PB1500,
	SYS_SDU1356,
	SYS_CLIO1050,
	NUM_SYSTEMS // must be last
};

static struct {
	struct e1356fb_fix fix;
	struct e1356fb_par par;
} systems[NUM_SYSTEMS] = {

	/*
	 * NULL system to help us detect missing options
 	 * when the driver is compiled as a module.
	 */
	{
		{   // fix
			SYS_NULL,
		},
		{   // par
		}
	},

	/*
	 * Alchemy Pb1000 evaluation board, SED1356
	 */
	{
		{   // fix
			SYS_PB1000,
			0xE00000000, 0xE00200000,
			60, MEM_TYPE_EDO_2CAS, 64, MEM_SMR_CBR,
			0, 0,   // BUSCLK and MCLK are calculated at run-time
			40000, 14318, // CLKI, CLKI2
#ifdef CONFIG_PB1000_CRT
			DISP_TYPE_CRT,
			0, 0, // TV Options
			0, 0, // Panel options
#elif defined (CONFIG_PB1000_NTSC)
			DISP_TYPE_NTSC,
			TV_FILT_FLICKER|TV_FILT_LUM|TV_FILT_CHROM,
			TV_FMT_COMPOSITE,
			0, 0, // Panel options
#elif defined (CONFIG_PB1000_TFT)
			DISP_TYPE_TFT,
			0, 0, // TV Options
			0, 12, // Panel options, EL panel?, data width?
#else
			DISP_TYPE_PAL,
			TV_FILT_FLICKER|TV_FILT_LUM|TV_FILT_CHROM,
			TV_FMT_COMPOSITE,
			0, 0, // Panel options
#endif
			0, 0,
#ifdef CONFIG_MTRR
			0,
#endif
			0,
			0,
			{0},
			"800x600@60"
		},
		{   // par
			0, 0, 800, 600, 8, 1,
			// timings will be set by modedb
			{0}, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			256
		}
	},

	/*
	 * Alchemy Pb1500 evaluation board, SED13806
	 */
	{
		{   // fix
			SYS_PB1500,
			0xE1B000000, 0xE1B200000,
			50, MEM_TYPE_EMBEDDED_SDRAM, 64, MEM_SMR_CBR,
			0, 0,   // BUSCLK and MCLK are calculated at run-time
			40000, 14318, // CLKI, CLKI2
#ifdef CONFIG_PB1500_CRT
			DISP_TYPE_CRT,
			0, 0, // TV Options
			0, 0, // Panel options
#else
			DISP_TYPE_TFT,
			0, 0, // TV Options
			0, 12, // Panel options, EL panel?, data width?
#endif
			0, 0,
#ifdef CONFIG_MTRR
			0,
#endif
			0,
			0,
			{0},
			"800x600@60"
		},
		{   // par
			0, 0, 800, 600, 8, 1,
			// timings will be set by modedb
			{0}, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			256
		}
	},

	/*
	 * Epson SDU1356B0C PCI eval card. These settings assume the
	 * card is configured for PCI, the MediaPlug is disabled,
	 * and the onboard clock synthesizer is at the power-up
	 * clock settings.
	 */
	{
		{   // fix
			SYS_SDU1356,
			0x0, 0x0,  // addresses obtained from PCI config space
			// FIXME: just guess for now
			60, MEM_TYPE_EDO_2CAS, 64, MEM_SMR_CBR,
			33000, 0, 40000, 25175, // BUSCLK, MCLK, CLKI, CLKI2
			DISP_TYPE_CRT,
			0, 0,
			0, 0,
			0, 0,
#ifdef CONFIG_MTRR
			0,
#endif
			0,
			0,
			{0},
			"800x600@60"
		},
		{   // par
			0, 0, 1024, 768, 8, 1,
			// timings will be set by modedb
			{0}, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			256
		}
	},

	/*
	 * Vadem Clio 1050 - this is for the benefit of the Linux-VR project.
	 * FIXME: Most of these settings are just guesses, until I can get a
	 * Clio 1050 and dump the registers that WinCE has setup.
	 */
	{
		{   // fix
			SYS_CLIO1050,
			0x0a000000, 0x0a200000,
			60, MEM_TYPE_EDO_2CAS, 64, MEM_SMR_CBR,
			40000, 40000, 14318, 14318,
			DISP_TYPE_TFT,
			0, 0,
			0, 16,
			0, 0,
#ifdef CONFIG_MTRR
			0,
#endif
			0,
			0,
			{0},
			"640x480@85"
		},
		{   // par
			0, 0, 1024, 768, 16, 2,
			// timings will be set by modedb
			{0}, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			16
		}
	}
};

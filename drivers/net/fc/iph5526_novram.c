/********************************************************************** 
 * Reading the NVRAM on the Interphase 5526 PCI Fibre Channel Card. 
 * All contents in this file : courtesy Interphase Corporation.
 * Special thanks to Kevin Quick, kquick@iphase.com.
 **********************************************************************/

#define FF_MAGIC        0x4646
#define DB_MAGIC        0x4442
#define DL_MAGIC        0x444d


#define CMD_LEN         9

/***********
 *
 *      Switches and defines for header files.
 *
 *      The following defines are used to turn on and off
 *      various options in the header files. Primarily useful
 *      for debugging.
 *
 ***********/

static const unsigned short novram_default[4] = {
    FF_MAGIC,
    DB_MAGIC,
    DL_MAGIC,
    0 };


/*
 * a list of the commands that can be sent to the NOVRAM
 */

#define NR_EXTEND  0x100
#define NR_WRITE   0x140
#define NR_READ    0x180
#define NR_ERASE   0x1c0

#define EWDS    0x00
#define WRAL    0x10
#define ERAL    0x20
#define EWEN    0x30

/*
 * Defines for the pins on the NOVRAM
 */

#define BIT(x)          (1 << (x))

#define NVDI_B          31
#define NVDI            BIT(NVDI_B)
#define NVDO            BIT(9)
#define NVCE            BIT(30)
#define NVSK            BIT(29)
#define NV_MANUAL       BIT(28)

/***********
 *
 *      Include files.
 *
 ***********/

#define KeStallExecutionProcessor(x)    {volatile int d, p;\
		  for (d=0; d<x; d++) for (p=0; p<10; p++);\
				     }


/***********************
 *
 * This define ands the value and the current config register and puts
 * the result in the config register
 *
 ***********************/

#define CFG_AND(val) { volatile int t; \
			   t = readl(fi->n_r.ptr_novram_hw_control_reg);   \
			   t &= (val);                                  \
			   writel(t, fi->n_r.ptr_novram_hw_control_reg);   \
		   }

/***********************
 *
 * This define ors the value and the current config register and puts
 * the result in the config register
 *
 ***********************/

#define CFG_OR(val) { volatile int t; \
			   t = readl(fi->n_r.ptr_novram_hw_control_reg);   \
			   t |= (val);                                  \
			   writel(t, fi->n_r.ptr_novram_hw_control_reg);   \
		   }

/***********************
 *
 * Send a command to the NOVRAM, the command is in cmd.
 *
 * clear CE and SK. Then assert CE.
 * Clock each of the command bits out in the correct order with SK
 * exit with CE still asserted
 *
 ***********************/

#define NVRAM_CMD(cmd) { int i; \
			 int c = cmd; \
			 CFG_AND(~(NVCE|NVSK)); \
			 CFG_OR(NVCE); \
			 for (i=0; i<CMD_LEN; i++) { \
			     NVRAM_CLKOUT((c & (1 << (CMD_LEN - 1))) ? 1 : 0);\
			     c <<= 1; } }

/***********************
 *
 * clear the CE, this must be used after each command is complete
 *
 ***********************/

#define NVRAM_CLR_CE    CFG_AND(~NVCE)

/***********************
 *
 * clock the data bit in bitval out to the NOVRAM.  The bitval must be
 * a 1 or 0, or the clockout operation is undefined
 *
 ***********************/

#define NVRAM_CLKOUT(bitval) {\
			   CFG_AND(~NVDI); \
			   CFG_OR((bitval) << NVDI_B); \
			   KeStallExecutionProcessor(5);\
			   CFG_OR(NVSK); \
			   KeStallExecutionProcessor(5);\
			   CFG_AND( ~NVSK); \
			   }

/***********************
 *
 * clock the data bit in and return a 1 or 0, depending on the value
 * that was received from the NOVRAM
 *
 ***********************/

#define NVRAM_CLKIN(val)        {\
		       CFG_OR(NVSK); \
			   KeStallExecutionProcessor(5);\
		       CFG_AND(~NVSK); \
			   KeStallExecutionProcessor(5);\
		       val = (readl(fi->n_r.ptr_novram_hw_status_reg) & NVDO) ? 1 : 0; \
		       }

/***********
 *
 *      Function Prototypes
 *
 ***********/

static int iph5526_nr_get(struct fc_info *fi, int addr);
static void iph5526_nr_do_init(struct fc_info *fi);
static void iph5526_nr_checksum(struct fc_info *fi);


/*******************************************************************
 *
 *      Local routine:  iph5526_nr_do_init
 *      Purpose:        initialize novram server
 *      Description:
 *
 *      iph5526_nr_do_init reads the novram into the temporary holding place.
 *      A checksum is done on the area and the Magic Cookies are checked.
 *      If any of them are bad, the NOVRAM is initialized with the 
 *      default values and a warning message is displayed.
 *
 *******************************************************************/

static void iph5526_nr_do_init(struct fc_info *fi)
{
    int i;
    unsigned short chksum = 0;
    int bad = 0;

    for (i=0; i<IPH5526_NOVRAM_SIZE; i++) {
	fi->n_r.data[i] = iph5526_nr_get(fi, i);
	chksum += fi->n_r.data[i];
    }

    if (chksum) 
	bad = 1;

    if (fi->n_r.data[IPH5526_NOVRAM_SIZE - 4] != FF_MAGIC)
	bad = 1;
    if (fi->n_r.data[IPH5526_NOVRAM_SIZE - 3] != DB_MAGIC)
	bad = 1;                 
	if (fi->n_r.data[IPH5526_NOVRAM_SIZE - 2] != DL_MAGIC)
	bad = 1;

    if (bad) {
	for (i=0; i<IPH5526_NOVRAM_SIZE; i++) {
	    if (i < (IPH5526_NOVRAM_SIZE - 4)) {
		fi->n_r.data[i] = 0xffff;
	    } else {
		fi->n_r.data[i] = novram_default[i - (IPH5526_NOVRAM_SIZE - 4)];
	    }
	}
	iph5526_nr_checksum(fi);
    }
}


/*******************************************************************
 *
 *      Local routine:  iph5526_nr_get
 *      Purpose:        read a single word of NOVRAM
 *      Description:
 *
 *      read the 16 bits that make up a word addr of the novram.  
 *      The 16 bits of data that are read are returned as the return value
 *
 *******************************************************************/

static int iph5526_nr_get(struct fc_info *fi, int addr)
{
    int i;
    int t;
    int val = 0;

    CFG_OR(NV_MANUAL);

    /*
     * read the first bit that was clocked with the falling edge of the
     * the last command data clock
     */

    NVRAM_CMD(NR_READ + addr);

    /*
     * Now read the rest of the bits, the next bit read is D1, then D2,
     * and so on
     */

    val = 0;
    for (i=0; i<16; i++) {
	NVRAM_CLKIN(t);
	val <<= 1;
	val |= t;
    }
    NVRAM_CLR_CE;

    CFG_OR(NVDI);
    CFG_AND(~NV_MANUAL);

    return(val);
}




/*******************************************************************
 *
 *      Local routine:  iph5526_nr_checksum
 *      Purpose:        calculate novram checksum on fi->n_r.data
 *      Description:
 *
 *      calculate a checksum for the novram on the image that is
 *      currently in fi->n_r.data
 *
 *******************************************************************/

static void iph5526_nr_checksum(struct fc_info *fi)
{
    int i;
    unsigned short chksum = 0;

    for (i=0; i<(IPH5526_NOVRAM_SIZE - 1); i++)
	chksum += fi->n_r.data[i];

    fi->n_r.data[i] = -chksum;
}

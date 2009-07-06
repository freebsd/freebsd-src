/*
 *  octeon_ebt3000_cf.c
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bio.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/power.h>
#include <sys/smp.h>
#include <sys/time.h>
#include <sys/timetc.h>
#include <sys/malloc.h>

#include <geom/geom.h>

#include <machine/clock.h>
#include <machine/locore.h>
#include <machine/md_var.h>
#include <machine/cpuregs.h>

#include "octeon_ebt3000_cf.h"
#include "driveid.h"
#include <mips/octeon1/octeon_pcmap_regs.h>

/* ATA Commands */
#define CMD_READ_SECTOR		0x20
#define CMD_WRITE_SECTOR	0x30
#define CMD_IDENTIFY		0xEC

/* The ATA Task File */
#define TF_DATA			0x00
#define TF_ERROR		0x01
#define TF_PRECOMP		0x01
#define TF_SECTOR_COUNT		0x02
#define TF_SECTOR_NUMBER	0x03
#define TF_CYL_LSB		0x04
#define TF_CYL_MSB		0x05
#define TF_DRV_HEAD		0x06
#define TF_STATUS		0x07
#define TF_COMMAND		0x07

/* Status Register */
#define STATUS_BSY		0x80	/* Drive is busy */
#define STATUS_RDY		0x40	/* Drive is ready */
#define STATUS_DRQ		0x08	/* Data can be transferred */

/* Miscelaneous */
#define SECTOR_SIZE		512
#define WAIT_DELAY		1000
#define NR_TRIES		1000
#define SWAP_SHORT(x)		((x << 8) | (x >> 8))
#define SWAP_LONG(x)		(((x << 24) & 0xFF000000) | ((x <<  8) & 0x00FF0000) | \
				 ((x >> 8) & 0x0000FF00)  | ((x << 24) & 0x000000FF) )
#define MODEL_STR_SIZE		40


/* Globals */
int	bus_width;
void	*base_addr;

/* Device softc */
struct cf_priv {

	device_t dev;
	struct drive_param *drive_param;

	struct bio_queue_head cf_bq;
	struct g_geom *cf_geom;
	struct g_provider *cf_provider;

};

/* Device parameters */
struct drive_param{
	union {
		char buf[SECTOR_SIZE];
		struct hd_driveid driveid;
	} u;

	char model[MODEL_STR_SIZE];
	uint32_t nr_sectors;
	uint16_t sector_size;
	uint16_t heads;
	uint16_t tracks;
	uint16_t sec_track;

} drive_param;

/* GEOM class implementation */
static g_access_t       cf_access;
static g_start_t        cf_start;
static g_ioctl_t        cf_ioctl;

struct g_class g_cf_class = {
        .name =         "CF",
        .version =      G_VERSION,
        .start =        cf_start,
        .access =       cf_access,
        .ioctl =        cf_ioctl,
};

/* Device methods */
static int	cf_probe(device_t);
static void	cf_identify(driver_t *, device_t);
static int	cf_attach(device_t);
static int	cf_attach_geom(void *, int);

/* ATA methods */
static void	cf_cmd_identify(void);
static void	cf_cmd_write(uint32_t, uint32_t, void *);
static void	cf_cmd_read(uint32_t, uint32_t, void *);
static void	cf_wait_busy(void);
static void	cf_send_cmd(uint32_t, uint8_t);
static void	cf_attach_geom_proxy(void *arg, int flag);

/* Miscelenous */
static void	cf_swap_ascii(unsigned char[], char[]);


/* ------------------------------------------------------------------- *
 *                      cf_access()                                    *
 * ------------------------------------------------------------------- */
static int cf_access (struct g_provider *pp, int r, int w, int e)
{

	pp->sectorsize = drive_param.sector_size;
        pp->stripesize = drive_param.heads * drive_param.sec_track * drive_param.sector_size;
        pp->mediasize  = pp->stripesize * drive_param.tracks;

	return (0);
}


/* ------------------------------------------------------------------- *
 *                      cf_start()                                     *
 * ------------------------------------------------------------------- */
static void cf_start (struct bio *bp)
{
	/*
	* Handle actual I/O requests. The request is passed down through
	* the bio struct.
	*/

	if(bp->bio_cmd & BIO_GETATTR) {
		if (g_handleattr_int(bp, "GEOM::fwsectors", drive_param.sec_track))
                        return;
                if (g_handleattr_int(bp, "GEOM::fwheads",   drive_param.heads))
                        return;
                g_io_deliver(bp, ENOIOCTL);
                return;
	}

	if ((bp->bio_cmd & (BIO_READ | BIO_WRITE))) {

		if (bp->bio_cmd & BIO_READ) {
			cf_cmd_read(bp->bio_length / drive_param.sector_size,
					bp->bio_offset / drive_param.sector_size, bp->bio_data);

		} else if (bp->bio_cmd & BIO_WRITE) {
			cf_cmd_write(bp->bio_length / drive_param.sector_size,
					bp->bio_offset/drive_param.sector_size, bp->bio_data);
		}

		bp->bio_resid = 0;
		bp->bio_completed = bp->bio_length;
		g_io_deliver(bp, 0);
	}
}


static int cf_ioctl (struct g_provider *pp, u_long cmd, void *data, int fflag, struct thread *td)
{
    return (0);
}


/* ------------------------------------------------------------------- *
 *                      cf_cmd_read()                                  *
 * ------------------------------------------------------------------- *
 *
 *  Read nr_sectors from the device starting from start_sector.
 */
static void cf_cmd_read (uint32_t nr_sectors, uint32_t start_sector, void *buf)
{
	unsigned long lba;
	uint32_t count;
	uint16_t *ptr_16;
	uint8_t  *ptr_8;

//#define OCTEON_VISUAL_CF_0 1
#ifdef OCTEON_VISUAL_CF_0
        octeon_led_write_char(0, 'R');
#endif
	ptr_8  = (uint8_t*)buf;
	ptr_16 = (uint16_t*)buf;
	lba = start_sector; 


	while (nr_sectors--) {

		cf_send_cmd(lba, CMD_READ_SECTOR);

		if (bus_width == 8) {
			volatile uint8_t *task_file = (volatile uint8_t*)base_addr;
        		volatile uint8_t dummy;
			for (count = 0; count < SECTOR_SIZE; count++) {
				*ptr_8++ = task_file[TF_DATA];
				if ((count & 0xf) == 0) dummy = task_file[TF_STATUS];
			}
		} else {
			volatile uint16_t *task_file = (volatile uint16_t*)base_addr;
        		volatile uint16_t dummy;
			for (count = 0; count < SECTOR_SIZE; count+=2) {
				uint16_t temp;
				temp = task_file[TF_DATA];
				*ptr_16++ = SWAP_SHORT(temp);
				if ((count & 0xf) == 0) dummy = task_file[TF_STATUS/2];
			}
		}  

		lba ++;
	}
#ifdef OCTEON_VISUAL_CF_0
        octeon_led_write_char(0, ' ');
#endif
}


/* ------------------------------------------------------------------- *
 *                      cf_cmd_write()                                 *
 * ------------------------------------------------------------------- *
 *
 * Write nr_sectors to the device starting from start_sector.
 */
static void cf_cmd_write (uint32_t nr_sectors, uint32_t start_sector, void *buf)
{
	uint32_t lba;
	uint32_t count;
	uint16_t *ptr_16;
	uint8_t  *ptr_8;
	
//#define OCTEON_VISUAL_CF_1 1
#ifdef OCTEON_VISUAL_CF_1
        octeon_led_write_char(1, 'W');
#endif
	lba = start_sector;
	ptr_8  = (uint8_t*)buf;
	ptr_16 = (uint16_t*)buf;

	while (nr_sectors--) {

		cf_send_cmd(lba, CMD_WRITE_SECTOR);

		if (bus_width == 8) {
			volatile uint8_t *task_file;
        		volatile uint8_t dummy;

			task_file = (volatile uint8_t *) base_addr;
			for (count = 0; count < SECTOR_SIZE; count++) {
				task_file[TF_DATA] =  *ptr_8++;
				if ((count & 0xf) == 0) dummy = task_file[TF_STATUS];
			}
		} else {
			volatile uint16_t *task_file;
        		volatile uint16_t dummy;

			task_file = (volatile uint16_t *) base_addr;
			for (count = 0; count < SECTOR_SIZE; count+=2) {
				uint16_t temp = *ptr_16++;
				task_file[TF_DATA] =  SWAP_SHORT(temp);
				if ((count & 0xf) == 0) dummy = task_file[TF_STATUS/2];
			}
		} 

		lba ++;
	}
#ifdef OCTEON_VISUAL_CF_1
        octeon_led_write_char(1, ' ');
#endif
}


/* ------------------------------------------------------------------- *
 *                      cf_cmd_identify()                              *
 * ------------------------------------------------------------------- *
 *
 * Read parameters and other information from the drive and store 
 * it in the drive_param structure
 *
 */
static void cf_cmd_identify (void)
{
	int count;
	uint8_t status;

	if (bus_width == 8) {
        	volatile uint8_t *task_file;

        	task_file = (volatile uint8_t *) base_addr;

		while ((status = task_file[TF_STATUS]) & STATUS_BSY) {
			DELAY(WAIT_DELAY);
        	}

        	task_file[TF_SECTOR_COUNT]  = 0;
        	task_file[TF_SECTOR_NUMBER] = 0;
        	task_file[TF_CYL_LSB]  = 0;
        	task_file[TF_CYL_MSB]  = 0;
        	task_file[TF_DRV_HEAD] = 0;
        	task_file[TF_COMMAND]  = CMD_IDENTIFY;

		cf_wait_busy();

        	for (count = 0; count < SECTOR_SIZE; count++) 
               	 	drive_param.u.buf[count] = task_file[TF_DATA];

	} else {
		volatile uint16_t *task_file;

		task_file = (volatile uint16_t *) base_addr;

		while ((status = (task_file[TF_STATUS/2]>>8)) & STATUS_BSY) {
			DELAY(WAIT_DELAY);
		}

		task_file[TF_SECTOR_COUNT/2]  = 0; /* this includes TF_SECTOR_NUMBER */
		task_file[TF_CYL_LSB/2]  = 0; /* this includes TF_CYL_MSB */
		task_file[TF_DRV_HEAD/2] = 0 | (CMD_IDENTIFY<<8); /* this includes TF_COMMAND */

		cf_wait_busy();

		for (count = 0; count < SECTOR_SIZE; count+=2) {
			uint16_t temp;
			temp = task_file[TF_DATA];
			
			/* endianess will be swapped below */
			drive_param.u.buf[count]   = (temp & 0xff);
			drive_param.u.buf[count+1] = (temp & 0xff00)>>8;
		}
	}

	cf_swap_ascii(drive_param.u.driveid.model, drive_param.model);

	drive_param.sector_size =  512;   //=  SWAP_SHORT (drive_param.u.driveid.sector_bytes);
	drive_param.heads 	=  SWAP_SHORT (drive_param.u.driveid.cur_heads);
	drive_param.tracks	=  SWAP_SHORT (drive_param.u.driveid.cur_cyls); 
	drive_param.sec_track   =  SWAP_SHORT (drive_param.u.driveid.cur_sectors);
	drive_param.nr_sectors  =  SWAP_LONG  (drive_param.u.driveid.lba_capacity);

}


/* ------------------------------------------------------------------- *
 *                      cf_send_cmd()                                  *
 * ------------------------------------------------------------------- *
 *
 * Send command to read/write one sector specified by lba.
 *
 */
static void cf_send_cmd (uint32_t lba, uint8_t cmd)
{
	uint8_t status;

	if (bus_width == 8) {
		volatile uint8_t *task_file;

		task_file = (volatile uint8_t *) base_addr;

		while ( (status = task_file[TF_STATUS]) & STATUS_BSY) {
			DELAY(WAIT_DELAY);
		}

		task_file[TF_SECTOR_COUNT]  = 1;
		task_file[TF_SECTOR_NUMBER] = (lba & 0xff);
		task_file[TF_CYL_LSB]  =  ((lba >> 8) & 0xff);
		task_file[TF_CYL_MSB]  =  ((lba >> 16) & 0xff);
		task_file[TF_DRV_HEAD] =  ((lba >> 24) & 0xff) | 0xe0; 
		task_file[TF_COMMAND]  =  cmd;

	} else {
		volatile uint16_t *task_file;

		task_file = (volatile uint16_t *) base_addr;

		while ( (status = (task_file[TF_STATUS/2]>>8)) & STATUS_BSY) {
			DELAY(WAIT_DELAY);
		}

		task_file[TF_SECTOR_COUNT/2]  = 1 | ((lba & 0xff) << 8);
		task_file[TF_CYL_LSB/2]  =  ((lba >> 8) & 0xff) | (((lba >> 16) & 0xff) << 8);
		task_file[TF_DRV_HEAD/2] =  (((lba >> 24) & 0xff) | 0xe0) | (cmd << 8); 

	}

	cf_wait_busy();
}

/* ------------------------------------------------------------------- *
 *                      cf_wait_busy()                                 *
 * ------------------------------------------------------------------- *
 *
 * Wait until the drive finishes a given command and data is
 * ready to be transferred. This is done by repeatedly checking 
 * the BSY and DRQ bits of the status register. When the controller
 * is ready for data transfer, it clears the BSY bit and sets the 
 * DRQ bit.
 *
 */
static void cf_wait_busy (void)
{
	uint8_t status;

//#define OCTEON_VISUAL_CF_2 1
#ifdef OCTEON_VISUAL_CF_2
        static int where0 = 0;

        octeon_led_run_wheel(&where0, 2);
#endif

	if (bus_width == 8) {
		volatile uint8_t *task_file;
		task_file = (volatile uint8_t *)base_addr;

		status = task_file[TF_STATUS];	
		while ((status & STATUS_BSY) == STATUS_BSY || (status & STATUS_DRQ) != STATUS_DRQ ) {
			DELAY(WAIT_DELAY);
			status = task_file[TF_STATUS];
		}
	} else {
		volatile uint16_t *task_file;
		task_file = (volatile uint16_t *)base_addr;

		status = task_file[TF_STATUS/2]>>8;	
		while ((status & STATUS_BSY) == STATUS_BSY || (status & STATUS_DRQ) != STATUS_DRQ ) {
			DELAY(WAIT_DELAY);
			status = (uint8_t)(task_file[TF_STATUS/2]>>8);
		}
	}

#ifdef OCTEON_VISUAL_CF_2
        octeon_led_write_char(2, ' ');
#endif
}

/* ------------------------------------------------------------------- *
 *                      cf_swap_ascii()                                *
 * ------------------------------------------------------------------- *
 *
 * The ascii string returned by the controller specifying 
 * the model of the drive is byte-swaped. This routine 
 * corrects the byte ordering.
 *
 */
static void cf_swap_ascii (unsigned char str1[], char str2[])
{
	int i;

	for(i = 0; i < MODEL_STR_SIZE; i++) {
            str2[i] = str1[i^1];
        }
}


/* ------------------------------------------------------------------- *
 *                      cf_probe()                                     *
 * ------------------------------------------------------------------- */

static int cf_probe (device_t dev)
{
    	if (!octeon_board_real()) return 1;

	if (device_get_unit(dev) != 0) {
                panic("can't attach more devices\n");
        }

        device_set_desc(dev, "Octeon Compact Flash Driver");

	cf_cmd_identify();

        return (0);
}

/* ------------------------------------------------------------------- *
 *                      cf_identify()                                  *
 * ------------------------------------------------------------------- *
 *
 * Find the bootbus region for the CF to determine 
 * 16 or 8 bit and check to see if device is 
 * inserted.
 *
 */
static void cf_identify (driver_t *drv, device_t parent)
{
	uint8_t status;
        int bus_region;
	int count = 0;
        octeon_mio_boot_reg_cfgx_t cfg;


    	if (!octeon_board_real())
		return;

	base_addr = (void *) MIPS_PHYS_TO_KSEG0(OCTEON_CF_COMMON_BASE_ADDR);

        for (bus_region = 0; bus_region < 8; bus_region++)
        {
                cfg.word64 = oct_read64(OCTEON_MIO_BOOT_REG_CFGX(bus_region));
                if (cfg.bits.base == OCTEON_CF_COMMON_BASE_ADDR >> 16)
                {
                        bus_width = (cfg.bits.width) ? 16: 8;
                        printf("Compact flash found in bootbus region %d (%d bit).\n", bus_region, bus_width);
                        break;
                }
        }

	if (bus_width == 8) {
		volatile uint8_t *task_file;
		task_file = (volatile uint8_t *) base_addr;
		/* Check if CF is inserted */
		while ( (status = task_file[TF_STATUS]) & STATUS_BSY){
			if ((count++) == NR_TRIES )     {
				printf("Compact Flash not present\n");
				return;
                	}
			DELAY(WAIT_DELAY);
        	}
	} else {
		volatile uint16_t *task_file;
		task_file = (volatile uint16_t *) base_addr;
		/* Check if CF is inserted */
		while ( (status = (task_file[TF_STATUS/2]>>8)) & STATUS_BSY){
			if ((count++) == NR_TRIES )     {
				printf("Compact Flash not present\n");
				return;
                	}
			DELAY(WAIT_DELAY);
        	}
	}

	BUS_ADD_CHILD(parent, 0, "cf", 0);
}


/* ------------------------------------------------------------------- *
 *                      cf_attach_geom()                               *
 * ------------------------------------------------------------------- */

static int cf_attach_geom (void *arg, int flag)
{
	struct cf_priv *cf_priv;

	cf_priv = (struct cf_priv *) arg;
	cf_priv->cf_geom = g_new_geomf(&g_cf_class, "cf%d", device_get_unit(cf_priv->dev));
	cf_priv->cf_provider = g_new_providerf(cf_priv->cf_geom, cf_priv->cf_geom->name);
	cf_priv->cf_geom->softc = cf_priv;
        g_error_provider(cf_priv->cf_provider, 0);

        return (0);
}

/* ------------------------------------------------------------------- *
 *                      cf_attach_geom()                               *
 * ------------------------------------------------------------------- */
static void cf_attach_geom_proxy (void *arg, int flag)
{
    cf_attach_geom(arg, flag);
}



/* ------------------------------------------------------------------- *
 *                      cf_attach()                                    *
 * ------------------------------------------------------------------- */

static int cf_attach (device_t dev)
{
	struct cf_priv *cf_priv;

    	if (!octeon_board_real()) return 1;

	cf_priv = device_get_softc(dev);
	cf_priv->dev = dev;
	cf_priv->drive_param = &drive_param;

	g_post_event(cf_attach_geom_proxy, cf_priv, M_WAITOK, NULL);
	bioq_init(&cf_priv->cf_bq);

        return 0;
}


static device_method_t cf_methods[] = {
        /* Device interface */
        DEVMETHOD(device_probe,         cf_probe),
        DEVMETHOD(device_identify,      cf_identify),
        DEVMETHOD(device_attach,        cf_attach),
        DEVMETHOD(device_detach,        bus_generic_detach),
        DEVMETHOD(device_shutdown,      bus_generic_shutdown),

        { 0, 0 }
};

static driver_t cf_driver = {
        "cf", 
	cf_methods, 
	sizeof(struct cf_priv)
};

static devclass_t cf_devclass;

DRIVER_MODULE(cf, nexus, cf_driver, cf_devclass, 0, 0);


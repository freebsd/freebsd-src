/*
 * jazz_esp.c: Driver for SCSI chip on Mips Magnum Boards (JAZZ architecture)
 *
 * Copyright (C) 1997 Thomas Bogendoerfer (tsbogend@alpha.franken.de)
 *
 * jazz_esp is based on David S. Miller's ESP driver and cyber_esp
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/blk.h>
#include <linux/proc_fs.h>
#include <linux/stat.h>

#include "scsi.h"
#include "hosts.h"
#include "NCR53C9x.h"
#include "jazz_esp.h"

#include <asm/irq.h>
#include <asm/jazz.h>
#include <asm/jazzdma.h>
#include <asm/dma.h>

#include <asm/pgtable.h>

static int  dma_bytes_sent(struct NCR_ESP *esp, int fifo_count);
static int  dma_can_transfer(struct NCR_ESP *esp, Scsi_Cmnd *sp);
static void dma_dump_state(struct NCR_ESP *esp);
static void dma_init_read(struct NCR_ESP *esp, __u32 vaddress, int length);
static void dma_init_write(struct NCR_ESP *esp, __u32 vaddress, int length);
static void dma_ints_off(struct NCR_ESP *esp);
static void dma_ints_on(struct NCR_ESP *esp);
static int  dma_irq_p(struct NCR_ESP *esp);
static int  dma_ports_p(struct NCR_ESP *esp);
static void dma_setup(struct NCR_ESP *esp, __u32 addr, int count, int write);
static void dma_mmu_get_scsi_one (struct NCR_ESP *esp, Scsi_Cmnd *sp);
static void dma_mmu_get_scsi_sgl (struct NCR_ESP *esp, Scsi_Cmnd *sp);
static void dma_mmu_release_scsi_one (struct NCR_ESP *esp, Scsi_Cmnd *sp);
static void dma_mmu_release_scsi_sgl (struct NCR_ESP *esp, Scsi_Cmnd *sp);
static void dma_advance_sg (Scsi_Cmnd *sp);
static void dma_led_off(struct NCR_ESP *);
static void dma_led_on(struct NCR_ESP *);


static volatile unsigned char cmd_buffer[16];
				/* This is where all commands are put
				 * before they are trasfered to the ESP chip
				 * via PIO.
				 */

/***************************************************************** Detection */
int jazz_esp_detect(Scsi_Host_Template *tpnt)
{
    struct NCR_ESP *esp;
    struct ConfigDev *esp_dev;

    /*
     * first assumption it is there:-)
     */
    if (1) {
	esp_dev = 0;
	esp = esp_allocate(tpnt, (void *) esp_dev);
	
	/* Do command transfer with programmed I/O */
	esp->do_pio_cmds = 1;
	
	/* Required functions */
	esp->dma_bytes_sent = &dma_bytes_sent;
	esp->dma_can_transfer = &dma_can_transfer;
	esp->dma_dump_state = &dma_dump_state;
	esp->dma_init_read = &dma_init_read;
	esp->dma_init_write = &dma_init_write;
	esp->dma_ints_off = &dma_ints_off;
	esp->dma_ints_on = &dma_ints_on;
	esp->dma_irq_p = &dma_irq_p;
	esp->dma_ports_p = &dma_ports_p;
	esp->dma_setup = &dma_setup;

	/* Optional functions */
	esp->dma_barrier = 0;
	esp->dma_drain = 0;
	esp->dma_invalidate = 0;
	esp->dma_irq_entry = 0;
	esp->dma_irq_exit = 0;
	esp->dma_poll = 0;
	esp->dma_reset = 0;
	esp->dma_led_off = &dma_led_off;
	esp->dma_led_on = &dma_led_on;
	
	/* virtual DMA functions */
	esp->dma_mmu_get_scsi_one = &dma_mmu_get_scsi_one;
	esp->dma_mmu_get_scsi_sgl = &dma_mmu_get_scsi_sgl;
	esp->dma_mmu_release_scsi_one = &dma_mmu_release_scsi_one;
	esp->dma_mmu_release_scsi_sgl = &dma_mmu_release_scsi_sgl;
	esp->dma_advance_sg = &dma_advance_sg;


	/* SCSI chip speed */
	esp->cfreq = 40000000;

	/* 
	 * we don't give the address of DMA channel, but the number
	 * of DMA channel, so we can use the jazz DMA functions
	 * 
	 */
	esp->dregs = JAZZ_SCSI_DMA;
	
	/* ESP register base */
	esp->eregs = (struct ESP_regs *)(JAZZ_SCSI_BASE);
	
	/* Set the command buffer */
	esp->esp_command = (volatile unsigned char *)cmd_buffer;
	
	/* get virtual dma address for command buffer */
	esp->esp_command_dvma = vdma_alloc(PHYSADDR(cmd_buffer), sizeof (cmd_buffer));
	
	esp->irq = JAZZ_SCSI_IRQ;
	request_irq(JAZZ_SCSI_IRQ, esp_intr, SA_INTERRUPT, "JAZZ SCSI",
	            NULL);

	/*
	 * FIXME, look if the scsi id is available from NVRAM
	 */
	esp->scsi_id = 7;
		
	/* Check for differential SCSI-bus */
	/* What is this stuff? */
	esp->diff = 0;

	esp_initialize(esp);
	
	printk("ESP: Total of %d ESP hosts found, %d actually in use.\n", nesps,esps_in_use);
	esps_running = esps_in_use;
	return esps_in_use;
    }
    return 0;
}

/************************************************************* DMA Functions */
static int dma_bytes_sent(struct NCR_ESP *esp, int fifo_count)
{
    return fifo_count;
}

static int dma_can_transfer(struct NCR_ESP *esp, Scsi_Cmnd *sp)
{
    /*
     * maximum DMA size is 1MB
     */
    unsigned long sz = sp->SCp.this_residual;
    if(sz > 0x100000)
	sz = 0x100000;
    return sz;
}

static void dma_dump_state(struct NCR_ESP *esp)
{
    
    ESPLOG(("esp%d: dma -- enable <%08x> residue <%08x\n",
	    esp->esp_id, vdma_get_enable((int)esp->dregs), vdma_get_residue((int)esp->dregs)));
}

static void dma_init_read(struct NCR_ESP *esp, __u32 vaddress, int length)
{
    dma_cache_wback_inv ((unsigned long)phys_to_virt(vdma_log2phys(vaddress)), length);
    vdma_disable ((int)esp->dregs);
    vdma_set_mode ((int)esp->dregs, DMA_MODE_READ);
    vdma_set_addr ((int)esp->dregs, vaddress);
    vdma_set_count ((int)esp->dregs, length);
    vdma_enable ((int)esp->dregs);
}

static void dma_init_write(struct NCR_ESP *esp, __u32 vaddress, int length)
{
    dma_cache_wback_inv ((unsigned long)phys_to_virt(vdma_log2phys(vaddress)), length);    
    vdma_disable ((int)esp->dregs);    
    vdma_set_mode ((int)esp->dregs, DMA_MODE_WRITE);
    vdma_set_addr ((int)esp->dregs, vaddress);
    vdma_set_count ((int)esp->dregs, length);
    vdma_enable ((int)esp->dregs);    
}

static void dma_ints_off(struct NCR_ESP *esp)
{
    disable_irq(esp->irq);
}

static void dma_ints_on(struct NCR_ESP *esp)
{
    enable_irq(esp->irq);
}

static int dma_irq_p(struct NCR_ESP *esp)
{
    return (esp_read(esp->eregs->esp_status) & ESP_STAT_INTR);
}

static int dma_ports_p(struct NCR_ESP *esp)
{
    int enable = vdma_get_enable((int)esp->dregs);
    
    return (enable & R4030_CHNL_ENABLE);
}

static void dma_setup(struct NCR_ESP *esp, __u32 addr, int count, int write)
{
    /* 
     * On the Sparc, DMA_ST_WRITE means "move data from device to memory"
     * so when (write) is true, it actually means READ!
     */
    if(write){
	dma_init_read(esp, addr, count);
    } else {
	dma_init_write(esp, addr, count);
    }
}

static void dma_mmu_get_scsi_one (struct NCR_ESP *esp, Scsi_Cmnd *sp)
{
    sp->SCp.have_data_in = vdma_alloc(PHYSADDR(sp->SCp.buffer), sp->SCp.this_residual);
    sp->SCp.ptr = (char *)((unsigned long)sp->SCp.have_data_in);
}

static void dma_mmu_get_scsi_sgl (struct NCR_ESP *esp, Scsi_Cmnd *sp)
{
    int sz = sp->SCp.buffers_residual;
    struct mmu_sglist *sg = (struct mmu_sglist *) sp->SCp.buffer;
    
    while (sz >= 0) {
	sg[sz].dvma_addr = vdma_alloc(PHYSADDR(sg[sz].addr), sg[sz].len);
	sz--;
    }
    sp->SCp.ptr=(char *)((unsigned long)sp->SCp.buffer->dvma_address);
}    

static void dma_mmu_release_scsi_one (struct NCR_ESP *esp, Scsi_Cmnd *sp)
{
    vdma_free(sp->SCp.have_data_in);
}

static void dma_mmu_release_scsi_sgl (struct NCR_ESP *esp, Scsi_Cmnd *sp)
{
    int sz = sp->use_sg - 1;
    struct mmu_sglist *sg = (struct mmu_sglist *)sp->buffer;
			
    while(sz >= 0) {
	vdma_free(sg[sz].dvma_addr);
	sz--;
    }
}

static void dma_advance_sg (Scsi_Cmnd *sp)
{
    sp->SCp.ptr = (char *)((unsigned long)sp->SCp.buffer->dvma_address);
}

#define JAZZ_HDC_LED   0xe000d100 /* FIXME, find correct address */

static void dma_led_off(struct NCR_ESP *esp)
{
#if 0    
    *(unsigned char *)JAZZ_HDC_LED = 0;
#endif    
}

static void dma_led_on(struct NCR_ESP *esp)
{    
#if 0    
    *(unsigned char *)JAZZ_HDC_LED = 1;
#endif    
}

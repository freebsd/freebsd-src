/* 
   3w-xxxx.c -- 3ware Storage Controller device driver for Linux.

   Written By: Adam Radford <linux@3ware.com>
   Modifications By: Joel Jacobson <linux@3ware.com>
   		     Arnaldo Carvalho de Melo <acme@conectiva.com.br>
                     Brad Strand <linux@3ware.com>

   Copyright (C) 1999-2003 3ware Inc.

   Kernel compatablity By: 	Andre Hedrick <andre@suse.com>
   Non-Copyright (C) 2000	Andre Hedrick <andre@suse.com>
   
   Further tiny build fixes and trivial hoovering    Alan Cox

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,           
   but WITHOUT ANY WARRANTY; without even the implied warranty of            
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             
   GNU General Public License for more details.                              

   NO WARRANTY                                                               
   THE PROGRAM IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR        
   CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED INCLUDING, WITHOUT      
   LIMITATION, ANY WARRANTIES OR CONDITIONS OF TITLE, NON-INFRINGEMENT,      
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Each Recipient is    
   solely responsible for determining the appropriateness of using and       
   distributing the Program and assumes all risks associated with its        
   exercise of rights under this Agreement, including but not limited to     
   the risks and costs of program errors, damage to or loss of data,         
   programs or equipment, and unavailability or interruption of operations.  

   DISCLAIMER OF LIABILITY                                                   
   NEITHER RECIPIENT NOR ANY CONTRIBUTORS SHALL HAVE ANY LIABILITY FOR ANY   
   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL        
   DAMAGES (INCLUDING WITHOUT LIMITATION LOST PROFITS), HOWEVER CAUSED AND   
   ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR     
   TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE    
   USE OR DISTRIBUTION OF THE PROGRAM OR THE EXERCISE OF ANY RIGHTS GRANTED  
   HEREUNDER, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES             

   You should have received a copy of the GNU General Public License         
   along with this program; if not, write to the Free Software               
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 

   Bugs/Comments/Suggestions should be mailed to:                            
   linux@3ware.com

   For more information, goto:
   http://www.3ware.com

   History
   -------
   0.1.000 -     Initial release.
   0.4.000 -     Added support for Asynchronous Event Notification through
                 ioctls for 3DM.
   1.0.000 -     Added DPO & FUA bit support for WRITE_10 & WRITE_6 cdb
                 to disable drive write-cache before writes.
   1.1.000 -     Fixed performance bug with DPO & FUA not existing for WRITE_6.
   1.2.000 -     Added support for clean shutdown notification/feature table.
   1.02.00.001 - Added support for full command packet posts through ioctls
                 for 3DM.
                 Bug fix so hot spare drives don't show up.
   1.02.00.002 - Fix bug with tw_setfeature() call that caused oops on some
                 systems.
   08/21/00    - release previously allocated resources on failure at
                 tw_allocate_memory (acme)
   1.02.00.003 - Fix tw_interrupt() to report error to scsi layer when
                 controller status is non-zero.
                 Added handling of request_sense opcode.
                 Fix possible null pointer dereference in 
                 tw_reset_device_extension()
   1.02.00.004 - Add support for device id of 3ware 7000 series controllers.
                 Make tw_setfeature() call with interrupts disabled.
                 Register interrupt handler before enabling interrupts.
                 Clear attention interrupt before draining aen queue.
   1.02.00.005 - Allocate bounce buffers and custom queue depth for raid5 for
                 6000 and 5000 series controllers.
                 Reduce polling mdelays causing problems on some systems.
                 Fix use_sg = 1 calculation bug.
                 Check for scsi_register returning NULL.
                 Add aen count to /proc/scsi/3w-xxxx.
                 Remove aen code unit masking in tw_aen_complete().
   1.02.00.006 - Remove unit from printk in tw_scsi_eh_abort(), causing
                 possible oops.
                 Fix possible null pointer dereference in tw_scsi_queue()
                 if done function pointer was invalid.
   1.02.00.007 - Fix possible null pointer dereferences in tw_ioctl().
                 Remove check for invalid done function pointer from
                 tw_scsi_queue().
   1.02.00.008 - Set max sectors per io to TW_MAX_SECTORS in tw_findcards().
                 Add tw_decode_error() for printing readable error messages.
                 Print some useful information on certain aen codes.
                 Add tw_decode_bits() for interpreting status register output.
                 Make scsi_set_pci_device() for kernels >= 2.4.4
                 Fix bug where aen's could be lost before a reset.
                 Re-add spinlocks in tw_scsi_detect().
                 Fix possible null pointer dereference in tw_aen_drain_queue()
                 during initialization.
                 Clear pci parity errors during initialization and during io.
   1.02.00.009 - Remove redundant increment in tw_state_request_start().
                 Add ioctl support for direct ATA command passthru.
                 Add entire aen code string list.
   1.02.00.010 - Cleanup queueing code, fix jbod thoughput.
                 Fix get_param for specific units.
   1.02.00.011 - Fix bug in tw_aen_complete() where aen's could be lost.
                 Fix tw_aen_drain_queue() to display useful info at init.
                 Set tw_host->max_id for 12 port cards.
                 Add ioctl support for raw command packet post from userspace
                 with sglist fragments (parameter and io).
   1.02.00.012 - Fix read capacity to under report by 1 sector to fix get
                 last sector ioctl.
   1.02.00.013 - Fix bug where more AEN codes weren't coming out during
                 driver initialization.
                 Improved handling of PCI aborts.
   1.02.00.014 - Fix bug in tw_findcards() where AEN code could be lost.
                 Increase timeout in tw_aen_drain_queue() to 30 seconds.
   1.02.00.015 - Re-write raw command post with data ioctl method.
   1.02.00.016 - Set max_cmd_len for 3dm.
                 Set max sectors to 256 for non raid5 & 6XXX.          
   1.02.00.017 - Modified pci parity error handling/clearing from config space
                 during initialization.
   1.02.00.018 - Better handling of request sense opcode and sense information
                 for failed commands.  Add tw_decode_sense().
                 Replace all mdelay()'s with scsi_sleep().
   1.02.00.019 - Revert mdelay's and scsi_sleep's, this caused problems on
                 some SMP systems.
   1.02.00.020 - Bump cmd_per_lun in SHT to 255 for better jbod performance.
                 Make module license ifdef'd for backward compatibility.
                 Improve handling of errors in tw_interrupt().
                 Add handling/clearing of controller queue error.
                 Better alignment checking in tw_allocate_memory().
                 Cleanup tw_initialize_device_extension().
                 Empty stale responses before draining aen queue.
                 Fix tw_scsi_eh_abort() to not reset on every io abort.
                 Set can_queue in SHT to 255 to prevent hang from AEN.
   1.02.00.021 - Fix possible null pointer dereference in tw_scsi_release().
   1.02.00.022 - Fix bug in tw_aen_drain_queue() where unit # was always zero.
   1.02.00.023 - Add severity levels to AEN strings.
   1.02.00.024 - Fix command interrupt spurious error messages.
                 Fix bug in raw command post with data ioctl method.
                 Fix bug where rollcall sometimes failed with cable errors.
   1.02.00.025 - Print unit # on all command timeouts.
   1.02.00.026 - Fix possible infinite retry bug with power glitch induced
                 drive timeouts.
                 Cleanup some AEN severity levels.
   1.02.00.027 - Add drive not supported AEN code for SATA controllers.
                 Remove spurious unknown ioctl error message.
   1.02.00.028 - Fix bug where multiple controllers with no units were the
                 same card number.
                 Fix bug where cards were being shut down more than once.
   1.02.00.029 - Add new pci dma mapping for kernel 2.4 for highmem support.
                 Replace pci_map_single() with pci_map_page() for highmem.
                 Check for tw_setfeature() failure.
   1.02.00.030 - Make driver 64-bit clean.
   1.02.00.031 - Cleanup polling timeouts/routines in several places.
                 Add support for mode sense opcode.
                 Add support for cache mode page.
                 Add support for synchronize cache opcode.
   1.02.00.032 - Fix small multicard rollcall bug.
                 Make driver stay loaded with no units for hot add/swap.
                 Add support for "twe" character device for ioctls.
                 Clean up request_id queueing code.
                 Fix tw_scsi_queue() spinlocks.
   1.02.00.033 - Fix tw_aen_complete() to not queue 'queue empty' AEN's.
                 Initialize queues correctly when loading with no valid units.
   1.02.00.034 - Fix tw_decode_bits() to handle multiple errors.
                 Add support for user configurable cmd_per_lun.
                 Add support for sht->select_queue_depths.
   1.02.00.035 - Improve tw_allocate_memory() memory allocation.
                 Fix tw_chrdev_ioctl() to sleep correctly.
   1.02.00.036 - Increase character ioctl timeout to 60 seconds.
   1.02.00.037 - Fix tw_ioctl() to handle all non-data ATA passthru cmds
                 for 'smartmontools' support.
*/

#include <linux/module.h>

MODULE_AUTHOR ("3ware Inc.");
#ifdef CONFIG_SMP
MODULE_DESCRIPTION ("3ware Storage Controller Linux Driver (SMP)");
#else
MODULE_DESCRIPTION ("3ware Storage Controller Linux Driver");
#endif

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/time.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/blk.h>
#include <linux/hdreg.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/smp.h>
#include <linux/reboot.h>
#include <linux/spinlock.h>

#include <asm/errno.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>

#define __3W_C			/* let 3w-xxxx.h know it is use */

#include "sd.h"
#include "scsi.h"
#include "hosts.h"

#include "3w-xxxx.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,10)
MODULE_LICENSE("GPL");
#endif

static int tw_chrdev_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
static int tw_chrdev_open(struct inode *inode, struct file *file);
static int tw_chrdev_release(struct inode *inode, struct file *file);
static int tw_copy_info(TW_Info *info, char *fmt, ...);
static void tw_copy_mem_info(TW_Info *info, char *data, int len);
static void tw_interrupt(int irq, void *dev_instance, struct pt_regs *regs);
static int tw_halt(struct notifier_block *nb, ulong event, void *buf);
static int tw_map_scsi_sg_data(struct pci_dev *pdev, Scsi_Cmnd *cmd);
static u32 tw_map_scsi_single_data(struct pci_dev *pdev, Scsi_Cmnd *cmd);
static void tw_unmap_scsi_data(struct pci_dev *pdev, Scsi_Cmnd *cmd);

/* Notifier block to get a notify on system shutdown/halt/reboot */
static struct notifier_block tw_notifier = {
	tw_halt, NULL, 0
};

/* File operations struct for character device */
static struct file_operations tw_fops = {
	owner: THIS_MODULE,
	ioctl: tw_chrdev_ioctl,
	open: tw_chrdev_open,
	release: tw_chrdev_release
};

/* Globals */
char *tw_driver_version="1.02.00.037";
TW_Device_Extension *tw_device_extension_list[TW_MAX_SLOT];
int tw_device_extension_count = 0;
static int twe_major = -1;

/* Functions */

/* This function will complete an aen request from the isr */
int tw_aen_complete(TW_Device_Extension *tw_dev, int request_id) 
{
	TW_Param *param;
	unsigned short aen;
	int error = 0, table_max = 0;

	dprintk(KERN_WARNING "3w-xxxx: tw_aen_complete()\n");
	if (tw_dev->alignment_virtual_address[request_id] == NULL) {
		printk(KERN_WARNING "3w-xxxx: tw_aen_complete(): Bad alignment virtual address.\n");
		return 1;
	}
	param = (TW_Param *)tw_dev->alignment_virtual_address[request_id];
	aen = *(unsigned short *)(param->data);
	dprintk(KERN_NOTICE "3w-xxxx: tw_aen_complete(): Queue'd code 0x%x\n", aen);

	/* Print some useful info when certain aen codes come out */
	if (aen == 0x0ff) {
		printk(KERN_WARNING "3w-xxxx: scsi%d: AEN: INFO: AEN queue overflow.\n", tw_dev->host->host_no);
	} else {
		table_max = sizeof(tw_aen_string)/sizeof(char *);
		if ((aen & 0x0ff) < table_max) {
			if ((tw_aen_string[aen & 0xff][strlen(tw_aen_string[aen & 0xff])-1]) == '#') {
				printk(KERN_WARNING "3w-xxxx: scsi%d: AEN: %s%d.\n", tw_dev->host->host_no, tw_aen_string[aen & 0xff], aen >> 8);
			} else {
				if (aen != 0x0) 
					printk(KERN_WARNING "3w-xxxx: scsi%d: AEN: %s.\n", tw_dev->host->host_no, tw_aen_string[aen & 0xff]);
			}
		} else {
			printk(KERN_WARNING "3w-xxxx: scsi%d: Received AEN %d.\n", tw_dev->host->host_no, aen);
		}
	}
	if (aen != TW_AEN_QUEUE_EMPTY) {
		tw_dev->aen_count++;

		/* Now queue the code */
		tw_dev->aen_queue[tw_dev->aen_tail] = aen;
		if (tw_dev->aen_tail == TW_Q_LENGTH - 1) {
			tw_dev->aen_tail = TW_Q_START;
		} else {
			tw_dev->aen_tail = tw_dev->aen_tail + 1;
		}
		if (tw_dev->aen_head == tw_dev->aen_tail) {
			if (tw_dev->aen_head == TW_Q_LENGTH - 1) {
				tw_dev->aen_head = TW_Q_START;
			} else {
				tw_dev->aen_head = tw_dev->aen_head + 1;
			}
		}

		error = tw_aen_read_queue(tw_dev, request_id);
		if (error) {
			printk(KERN_WARNING "3w-xxxx: scsi%d: Error completing AEN.\n", tw_dev->host->host_no);
			tw_dev->state[request_id] = TW_S_COMPLETED;
			tw_state_request_finish(tw_dev, request_id);
		}
	} else {
		tw_dev->state[request_id] = TW_S_COMPLETED;
		tw_state_request_finish(tw_dev, request_id);
	}

	return 0;
} /* End tw_aen_complete() */

/* This function will drain the aen queue after a soft reset */
int tw_aen_drain_queue(TW_Device_Extension *tw_dev)
{
	TW_Command *command_packet;
	TW_Param *param;
	int request_id = 0;
	u32 command_que_addr;
	unsigned long command_que_value;
	unsigned long param_value;
	TW_Response_Queue response_queue;
	u32 response_que_addr;
	unsigned short aen;
	unsigned short aen_code;
	int finished = 0;
	int first_reset = 0;
	int queue = 0;
	int found = 0, table_max = 0;

	dprintk(KERN_NOTICE "3w-xxxx: tw_aen_drain_queue()\n");

	command_que_addr = tw_dev->registers.command_que_addr;
	response_que_addr = tw_dev->registers.response_que_addr;

	if (tw_poll_status(tw_dev, TW_STATUS_ATTENTION_INTERRUPT | TW_STATUS_MICROCONTROLLER_READY, 30)) {
		dprintk(KERN_WARNING "3w-xxxx: tw_aen_drain_queue(): No attention interrupt for card %d.\n", tw_device_extension_count);
		return 1;
	}
	tw_clear_attention_interrupt(tw_dev);

	/* Empty response queue */
	tw_empty_response_que(tw_dev);

	/* Initialize command packet */
	if (tw_dev->command_packet_virtual_address[request_id] == NULL) {
		printk(KERN_WARNING "3w-xxxx: tw_aen_drain_queue(): Bad command packet virtual address.\n");
		return 1;
	}
	command_packet = (TW_Command *)tw_dev->command_packet_virtual_address[request_id];
	memset(command_packet, 0, sizeof(TW_Sector));
	command_packet->byte0.opcode = TW_OP_GET_PARAM;
	command_packet->byte0.sgl_offset = 2;
	command_packet->size = 4;
	command_packet->request_id = request_id;
	command_packet->byte3.unit = 0;
	command_packet->byte3.host_id = 0;
	command_packet->status = 0;
	command_packet->flags = 0;
	command_packet->byte6.parameter_count = 1;
	command_que_value = tw_dev->command_packet_physical_address[request_id];
	if (command_que_value == 0) {
		printk(KERN_WARNING "3w-xxxx: tw_aen_drain_queue(): Bad command packet physical address.\n");
		return 1;
	}

	/* Now setup the param */
	if (tw_dev->alignment_virtual_address[request_id] == NULL) {
		printk(KERN_WARNING "3w-xxxx: tw_aen_drain_queue(): Bad alignment virtual address.\n");
		return 1;
	}
	param = (TW_Param *)tw_dev->alignment_virtual_address[request_id];
	memset(param, 0, sizeof(TW_Sector));
	param->table_id = 0x401; /* AEN table */
	param->parameter_id = 2; /* Unit code */
	param->parameter_size_bytes = 2;
	param_value = tw_dev->alignment_physical_address[request_id];
	if (param_value == 0) {
		printk(KERN_WARNING "3w-xxxx: tw_aen_drain_queue(): Bad alignment physical address.\n");
		return 1;
	}
	command_packet->byte8.param.sgl[0].address = param_value;
	command_packet->byte8.param.sgl[0].length = sizeof(TW_Sector);

	/* Now drain the controller's aen queue */
	do {
		/* Post command packet */
		outl(command_que_value, command_que_addr);

		/* Now poll for completion */
		if (tw_poll_status_gone(tw_dev, TW_STATUS_RESPONSE_QUEUE_EMPTY, 30) == 0) {
			response_queue.value = inl(response_que_addr);
			request_id = (unsigned char)response_queue.u.response_id;
			if (request_id != 0) {
				/* Unexpected request id */
				printk(KERN_WARNING "3w-xxxx: tw_aen_drain_queue(): Unexpected request id.\n");
				return 1;
			}
	
			if (command_packet->status != 0) {
				if (command_packet->flags != TW_AEN_TABLE_UNDEFINED) {
					/* Bad response */
					tw_decode_sense(tw_dev, request_id, 0);
					return 1;
				} else {
					/* We know this is a 3w-1x00, and doesn't support aen's */
					return 0;
				}
			}

			/* Now check the aen */
			aen = *(unsigned short *)(param->data);
			aen_code = (aen & 0x0ff);
			queue = 0;
			switch (aen_code) {
				case TW_AEN_QUEUE_EMPTY:
					dprintk(KERN_WARNING "3w-xxxx: AEN: %s.\n", tw_aen_string[aen & 0xff]);
					if (first_reset != 1) {
						return 1;
					} else {
						finished = 1;
					}
					break;
				case TW_AEN_SOFT_RESET:
					if (first_reset == 0) {
						first_reset = 1;
					} else {
						printk(KERN_WARNING "3w-xxxx: AEN: %s.\n", tw_aen_string[aen & 0xff]);
						tw_dev->aen_count++;
						queue = 1;
					}
					break;
				default:
					if (aen == 0x0ff) {
						printk(KERN_WARNING "3w-xxxx: AEN: INFO: AEN queue overflow.\n");
					} else {
						table_max = sizeof(tw_aen_string)/sizeof(char *);
						if ((aen & 0x0ff) < table_max) {
							if ((tw_aen_string[aen & 0xff][strlen(tw_aen_string[aen & 0xff])-1]) == '#') {
								printk(KERN_WARNING "3w-xxxx: AEN: %s%d.\n", tw_aen_string[aen & 0xff], aen >> 8);
							} else {
								printk(KERN_WARNING "3w-xxxx: AEN: %s.\n", tw_aen_string[aen & 0xff]);
							}
						} else
							printk(KERN_WARNING "3w-xxxx: Received AEN %d.\n", aen);
					}
					tw_dev->aen_count++;
					queue = 1;
			}

			/* Now put the aen on the aen_queue */
			if (queue == 1) {
				tw_dev->aen_queue[tw_dev->aen_tail] = aen;
				if (tw_dev->aen_tail == TW_Q_LENGTH - 1) {
					tw_dev->aen_tail = TW_Q_START;
				} else {
					tw_dev->aen_tail = tw_dev->aen_tail + 1;
				}
				if (tw_dev->aen_head == tw_dev->aen_tail) {
					if (tw_dev->aen_head == TW_Q_LENGTH - 1) {
						tw_dev->aen_head = TW_Q_START;
					} else {
						tw_dev->aen_head = tw_dev->aen_head + 1;
					}
				}
			}
			found = 1;
		}
		if (found == 0) {
			printk(KERN_WARNING "3w-xxxx: tw_aen_drain_queue(): Response never received.\n");
			return 1;
		}
	} while (finished == 0);

	return 0;
} /* End tw_aen_drain_queue() */

/* This function will read the aen queue from the isr */
int tw_aen_read_queue(TW_Device_Extension *tw_dev, int request_id) 
{
	TW_Command *command_packet;
	TW_Param *param;
	u32 command_que_addr;
	unsigned long command_que_value;
	u32 status_reg_value = 0, status_reg_addr;
	unsigned long param_value = 0;

	dprintk(KERN_NOTICE "3w-xxxx: tw_aen_read_queue()\n");
	command_que_addr = tw_dev->registers.command_que_addr;
	status_reg_addr = tw_dev->registers.status_reg_addr;

	status_reg_value = inl(status_reg_addr);
	if (tw_check_bits(status_reg_value)) {
		dprintk(KERN_WARNING "3w-xxxx: tw_aen_read_queue(): Unexpected bits.\n");
		tw_decode_bits(tw_dev, status_reg_value, 1);
		return 1;
	}
	if (tw_dev->command_packet_virtual_address[request_id] == NULL) {
		printk(KERN_WARNING "3w-xxxx: tw_aen_read_queue(): Bad command packet virtual address.\n");
		return 1;
	}
	command_packet = (TW_Command *)tw_dev->command_packet_virtual_address[request_id];
	memset(command_packet, 0, sizeof(TW_Sector));
	command_packet->byte0.opcode = TW_OP_GET_PARAM;
	command_packet->byte0.sgl_offset = 2;
	command_packet->size = 4;
	command_packet->request_id = request_id;
	command_packet->byte3.unit = 0;
	command_packet->byte3.host_id = 0;
	command_packet->status = 0;
	command_packet->flags = 0;
	command_packet->byte6.parameter_count = 1;
	command_que_value = tw_dev->command_packet_physical_address[request_id];
	if (command_que_value == 0) {
		printk(KERN_WARNING "3w-xxxx: tw_aen_read_queue(): Bad command packet physical address.\n");
		return 1;
	}
	/* Now setup the param */
	if (tw_dev->alignment_virtual_address[request_id] == NULL) {
		printk(KERN_WARNING "3w-xxxx: tw_aen_read_queue(): Bad alignment virtual address.\n");
		return 1;
	}
	param = (TW_Param *)tw_dev->alignment_virtual_address[request_id];
	memset(param, 0, sizeof(TW_Sector));
	param->table_id = 0x401; /* AEN table */
	param->parameter_id = 2; /* Unit code */
	param->parameter_size_bytes = 2;
	param_value = tw_dev->alignment_physical_address[request_id];
	if (param_value == 0) {
		printk(KERN_WARNING "3w-xxxx: tw_aen_read_queue(): Bad alignment physical address.\n");
		return 1;
	}
	command_packet->byte8.param.sgl[0].address = param_value;
	command_packet->byte8.param.sgl[0].length = sizeof(TW_Sector);

	/* Now post the command packet */
	if ((status_reg_value & TW_STATUS_COMMAND_QUEUE_FULL) == 0) {
		dprintk(KERN_WARNING "3w-xxxx: tw_aen_read_queue(): Post succeeded.\n");
		tw_dev->srb[request_id] = 0; /* Flag internal command */
		tw_dev->state[request_id] = TW_S_POSTED;
		outl(command_que_value, command_que_addr);
	} else {
		printk(KERN_WARNING "3w-xxxx: tw_aen_read_queue(): Post failed, will retry.\n");
		return 1;
	}

	return 0;
} /* End tw_aen_read_queue() */

/* This function will allocate memory and check if it is 16 d-word aligned */
int tw_allocate_memory(TW_Device_Extension *tw_dev, int size, int which)
{
	int i, imax;
	dma_addr_t dma_handle;
	unsigned long *cpu_addr = NULL;

	dprintk(KERN_NOTICE "3w-xxxx: tw_allocate_memory()\n");


	if (which == 2)
		imax = TW_MAX_BOUNCEBUF;
	else
		imax = TW_Q_LENGTH;

	cpu_addr = pci_alloc_consistent(tw_dev->tw_pci_dev, size*imax, &dma_handle);
	if (cpu_addr == NULL) {
		printk(KERN_WARNING "3w-xxxx: pci_alloc_consistent() failed.\n");
		return 1;
	}

	if ((unsigned long)cpu_addr % (tw_dev->tw_pci_dev->device == TW_DEVICE_ID ? TW_ALIGNMENT_6000 : TW_ALIGNMENT_7000)) {
		printk(KERN_WARNING "3w-xxxx: Couldn't allocate correctly aligned memory.\n");
		pci_free_consistent(tw_dev->tw_pci_dev, size*imax, cpu_addr, dma_handle);
		return 1;
	}

	memset(cpu_addr, 0, size*imax);

	for (i=0;i<imax;i++) {
		switch(which) {
		case 0:
			tw_dev->command_packet_physical_address[i] = dma_handle+(i*size);
			tw_dev->command_packet_virtual_address[i] = (unsigned long *)((unsigned char *)cpu_addr + (i*size));
			break;
		case 1:
			tw_dev->alignment_physical_address[i] = dma_handle+(i*size);
			tw_dev->alignment_virtual_address[i] = (unsigned long *)((unsigned char *)cpu_addr + (i*size));
			break;
		case 2:
			tw_dev->bounce_buffer_phys[i] = dma_handle+(i*size);
			tw_dev->bounce_buffer[i] = (unsigned long *)((unsigned char *)cpu_addr + (i*size));
			break;
		default:
			printk(KERN_WARNING "3w-xxxx: tw_allocate_memory(): case slip in tw_allocate_memory()\n");
			return 1;
		}
	}

	return 0;
} /* End tw_allocate_memory() */

/* This function will check the status register for unexpected bits */
int tw_check_bits(u32 status_reg_value)
{
	if ((status_reg_value & TW_STATUS_EXPECTED_BITS) != TW_STATUS_EXPECTED_BITS) {  
		dprintk(KERN_WARNING "3w-xxxx: tw_check_bits(): No expected bits (0x%x).\n", status_reg_value);
		return 1;
	}
	if ((status_reg_value & TW_STATUS_UNEXPECTED_BITS) != 0) {
		dprintk(KERN_WARNING "3w-xxxx: tw_check_bits(): Found unexpected bits (0x%x).\n", status_reg_value);
		return 1;
	}

	return 0;
} /* End tw_check_bits() */

/* This function will report controller error status */
int tw_check_errors(TW_Device_Extension *tw_dev) 
{
	u32 status_reg_addr, status_reg_value;
  
	status_reg_addr = tw_dev->registers.status_reg_addr;
	status_reg_value = inl(status_reg_addr);

	if (TW_STATUS_ERRORS(status_reg_value) || tw_check_bits(status_reg_value)) {
		tw_decode_bits(tw_dev, status_reg_value, 0);
		return 1;
	}

	return 0;
} /* End tw_check_errors() */

/* This function handles ioctl for the character device */
static int tw_chrdev_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int error, request_id;
	dma_addr_t dma_handle;
	unsigned short tw_aen_code;
	unsigned long flags;
	unsigned int data_buffer_length = 0;
	unsigned long data_buffer_length_adjusted = 0;
	unsigned long *cpu_addr;
	long timeout;
	TW_New_Ioctl *tw_ioctl;
	TW_Passthru *passthru;
	TW_Device_Extension *tw_dev = tw_device_extension_list[MINOR(inode->i_rdev)];
	int retval = -EFAULT;

	dprintk(KERN_WARNING "3w-xxxx: tw_chrdev_ioctl()\n");

	/* Only let one of these through at a time */
	if (down_interruptible(&tw_dev->ioctl_sem))
		return -EINTR;

	/* First copy down the buffer length */
	error = copy_from_user(&data_buffer_length, (void *)arg, sizeof(unsigned int));
	if (error)
		goto out;

	/* Check size */
	if (data_buffer_length > TW_MAX_SECTORS * 512) {
		retval = -EINVAL;
		goto out;
	}

	/* Hardware can only do multiple of 512 byte transfers */
	data_buffer_length_adjusted = (data_buffer_length + 511) & ~511;

	/* Now allocate ioctl buf memory */
	cpu_addr = pci_alloc_consistent(tw_dev->tw_pci_dev, data_buffer_length_adjusted+sizeof(TW_New_Ioctl) - 1, &dma_handle);
	if (cpu_addr == NULL) {
		retval = -ENOMEM;
		goto out;
	}

	tw_ioctl = (TW_New_Ioctl *)cpu_addr;

	/* Now copy down the entire ioctl */
	error = copy_from_user(tw_ioctl, (void *)arg, data_buffer_length + sizeof(TW_New_Ioctl) - 1);
	if (error)
		goto out2;

	passthru = (TW_Passthru *)&tw_ioctl->firmware_command;

	/* See which ioctl we are doing */
	switch (cmd) {
		case TW_OP_NOP:
			dprintk(KERN_WARNING "3w-xxxx: tw_chrdev_ioctl(): caught TW_OP_NOP.\n");
			break;
		case TW_OP_AEN_LISTEN:
			dprintk(KERN_WARNING "3w-xxxx: tw_chrdev_ioctl(): caught TW_AEN_LISTEN.\n");
			memset(tw_ioctl->data_buffer, 0, tw_ioctl->data_buffer_length);
			spin_lock_irqsave(&tw_dev->tw_lock, flags);
			if (tw_dev->aen_head == tw_dev->aen_tail) {
				tw_aen_code = TW_AEN_QUEUE_EMPTY;
			} else {
				tw_aen_code = tw_dev->aen_queue[tw_dev->aen_head];
				if (tw_dev->aen_head == TW_Q_LENGTH - 1) {
					tw_dev->aen_head = TW_Q_START;
				} else {
					tw_dev->aen_head = tw_dev->aen_head + 1;
				}
			}
			spin_unlock_irqrestore(&tw_dev->tw_lock, flags);
			memcpy(tw_ioctl->data_buffer, &tw_aen_code, sizeof(tw_aen_code));
			break;
		case TW_CMD_PACKET_WITH_DATA:
			dprintk(KERN_WARNING "3w-xxxx: tw_chrdev_ioctl(): caught TW_CMD_PACKET_WITH_DATA.\n");
			spin_lock_irqsave(&tw_dev->tw_lock, flags);

			tw_state_request_start(tw_dev, &request_id);

			/* Flag internal command */
			tw_dev->srb[request_id] = 0;

			/* Flag chrdev ioctl */
			tw_dev->chrdev_request_id = request_id;

			tw_ioctl->firmware_command.request_id = request_id;

			/* Load the sg list */
			switch (tw_ioctl->firmware_command.byte0.sgl_offset) {
			case 2:
				tw_ioctl->firmware_command.byte8.param.sgl[0].address = dma_handle + sizeof(TW_New_Ioctl) - 1;
				tw_ioctl->firmware_command.byte8.param.sgl[0].length = data_buffer_length_adjusted;
				break;
			case 3:
				tw_ioctl->firmware_command.byte8.io.sgl[0].address = dma_handle + sizeof(TW_New_Ioctl) - 1;
				tw_ioctl->firmware_command.byte8.io.sgl[0].length = data_buffer_length_adjusted;
				break;
			case 5:
				passthru->sg_list[0].address = dma_handle + sizeof(TW_New_Ioctl) - 1;
				passthru->sg_list[0].length = data_buffer_length_adjusted;
				break;
			}

			memcpy(tw_dev->command_packet_virtual_address[request_id], &(tw_ioctl->firmware_command), sizeof(TW_Command));

			/* Now post the command packet to the controller */
			tw_post_command_packet(tw_dev, request_id);
			spin_unlock_irqrestore(&tw_dev->tw_lock, flags);

			timeout = TW_IOCTL_CHRDEV_TIMEOUT*HZ;

			/* Now wait for the command to complete */
			tw_wait_event_interruptible_timeout(tw_dev->ioctl_wqueue, tw_dev->chrdev_request_id == TW_IOCTL_CHRDEV_FREE, timeout);

			/* Check if we timed out, got a signal, or didn't get 
                           an interrupt */
			if ((timeout <= 0) && (tw_dev->chrdev_request_id != TW_IOCTL_CHRDEV_FREE)) {
				/* Now we need to reset the board */
				if (timeout == -ERESTARTSYS) {
					retval = timeout;
				} else {
					printk(KERN_WARNING "3w-xxxx: scsi%d: Character ioctl (0x%x) timed out, resetting card.\n", tw_dev->host->host_no, cmd);
					retval = -EIO;
				}
				spin_lock_irqsave(&tw_dev->tw_lock, flags);
				tw_dev->state[request_id] = TW_S_COMPLETED;
				tw_state_request_finish(tw_dev, request_id);
				tw_dev->posted_request_count--;
				if (tw_reset_device_extension(tw_dev)) {
					printk(KERN_WARNING "3w-xxxx: tw_chrdev_ioctl(): Reset failed for card %d.\n", tw_dev->host->host_no);
				}
				spin_unlock_irqrestore(&tw_dev->tw_lock, flags);
				goto out2;
			}

			/* Now copy in the command packet response */
			memcpy(&(tw_ioctl->firmware_command), tw_dev->command_packet_virtual_address[request_id], sizeof(TW_Command));

			/* Now complete the io */
			spin_lock_irqsave(&tw_dev->tw_lock, flags);
			tw_dev->posted_request_count--;
			tw_dev->state[request_id] = TW_S_COMPLETED;
			tw_state_request_finish(tw_dev, request_id);
			spin_unlock_irqrestore(&tw_dev->tw_lock, flags);
			break;
		default:
			retval = -ENOTTY;
			goto out2;
	}

	/* Now copy the entire response to userspace */
	error = copy_to_user((void *)arg, tw_ioctl, sizeof(TW_New_Ioctl) + tw_ioctl->data_buffer_length - 1);
	if (error == 0)
		retval = 0;
out2:
	/* Now free ioctl buf memory */
	pci_free_consistent(tw_dev->tw_pci_dev, data_buffer_length_adjusted+sizeof(TW_New_Ioctl) - 1, cpu_addr, dma_handle);
out:
	up(&tw_dev->ioctl_sem);
	return retval;
} /* End tw_chrdev_ioctl() */

/* This function handles open for the character device */
static int tw_chrdev_open(struct inode *inode, struct file *file)
{
	unsigned int minor_number;

	dprintk(KERN_WARNING "3w-xxxx: tw_ioctl_open()\n");

	minor_number = MINOR(inode->i_rdev);
	if (minor_number >= tw_device_extension_count)
		return -ENODEV;

	return 0;
} /* End tw_chrdev_open() */

/* This function handles close for the character device */
static int tw_chrdev_release(struct inode *inode, struct file *file)
{
	dprintk(KERN_WARNING "3w-xxxx: tw_ioctl_release()\n");

	return 0;
} /* End tw_chrdev_release() */

/* This function will clear all interrupts on the controller */
void tw_clear_all_interrupts(TW_Device_Extension *tw_dev)
{
	u32 control_reg_addr, control_reg_value;

	control_reg_addr = tw_dev->registers.control_reg_addr;
	control_reg_value = TW_STATUS_VALID_INTERRUPT;
	outl(control_reg_value, control_reg_addr);
} /* End tw_clear_all_interrupts() */

/* This function will clear the attention interrupt */
void tw_clear_attention_interrupt(TW_Device_Extension *tw_dev)
{
	u32 control_reg_addr, control_reg_value;
  
	control_reg_addr = tw_dev->registers.control_reg_addr;
	control_reg_value = TW_CONTROL_CLEAR_ATTENTION_INTERRUPT;
	outl(control_reg_value, control_reg_addr);
} /* End tw_clear_attention_interrupt() */

/* This function will clear the host interrupt */
void tw_clear_host_interrupt(TW_Device_Extension *tw_dev)
{
	u32 control_reg_addr, control_reg_value;

	control_reg_addr = tw_dev->registers.control_reg_addr;
	control_reg_value = TW_CONTROL_CLEAR_HOST_INTERRUPT;
	outl(control_reg_value, control_reg_addr);
} /* End tw_clear_host_interrupt() */

/* This function is called by tw_scsi_proc_info */
static int tw_copy_info(TW_Info *info, char *fmt, ...) 
{
	va_list args;
	char buf[81];
	int len;
  
	va_start(args, fmt);
	len = vsprintf(buf, fmt, args);
	va_end(args);
	tw_copy_mem_info(info, buf, len);
	return len;
} /* End tw_copy_info() */

/* This function is called by tw_scsi_proc_info */
static void tw_copy_mem_info(TW_Info *info, char *data, int len)
{
	if (info->position + len > info->length)
		len = info->length - info->position;

	if (info->position + len < info->offset) {
		info->position += len;
		return;
	}
	if (info->position < info->offset) {
		data += (info->offset - info->position);
		len  -= (info->offset - info->position);
	}
	if (len > 0) {
		memcpy(info->buffer + info->position, data, len);
		info->position += len;
	}
} /* End tw_copy_mem_info() */

/* This function will print readable messages from status register errors */
int tw_decode_bits(TW_Device_Extension *tw_dev, u32 status_reg_value, int print_host)
{
	char host[16];
	
	dprintk(KERN_WARNING "3w-xxxx: tw_decode_bits()\n");

	if (print_host)
		sprintf(host, " scsi%d:", tw_dev->host->host_no);
	else
		host[0] = '\0';

	if (status_reg_value & TW_STATUS_PCI_PARITY_ERROR) {
		printk(KERN_WARNING "3w-xxxx:%s PCI Parity Error: clearing.\n", host);
		outl(TW_CONTROL_CLEAR_PARITY_ERROR, tw_dev->registers.control_reg_addr);
	}

	if (status_reg_value & TW_STATUS_PCI_ABORT) {
		printk(KERN_WARNING "3w-xxxx:%s PCI Abort: clearing.\n", host);
		outl(TW_CONTROL_CLEAR_PCI_ABORT, tw_dev->registers.control_reg_addr);
		pci_write_config_word(tw_dev->tw_pci_dev, PCI_STATUS, TW_PCI_CLEAR_PCI_ABORT);
	}
	
	if (status_reg_value & TW_STATUS_QUEUE_ERROR) {
		printk(KERN_WARNING "3w-xxxx:%s Controller Queue Error: clearing.\n", host);
		outl(TW_CONTROL_CLEAR_QUEUE_ERROR, tw_dev->registers.control_reg_addr);
	}
	
	if (status_reg_value & TW_STATUS_SBUF_WRITE_ERROR) {
		printk(KERN_WARNING "3w-xxxx:%s SBUF Write Error: clearing.\n", host);
		outl(TW_CONTROL_CLEAR_SBUF_WRITE_ERROR, tw_dev->registers.control_reg_addr);
	}

	if (status_reg_value & TW_STATUS_MICROCONTROLLER_ERROR) {
		if (tw_dev->reset_print == 0) {
			printk(KERN_WARNING "3w-xxxx:%s Microcontroller Error: clearing.\n", host);
			tw_dev->reset_print = 1;
		}
		return 1;
	}

	return 0;
} /* End tw_decode_bits() */

/* This function will return valid sense buffer information for failed cmds */
int tw_decode_sense(TW_Device_Extension *tw_dev, int request_id, int fill_sense)
{
	int i;
	TW_Command *command;

	dprintk(KERN_WARNING "3w-xxxx: tw_decode_sense()\n");
	command = (TW_Command *)tw_dev->command_packet_virtual_address[request_id];

	printk(KERN_WARNING "3w-xxxx: scsi%d: Command failed: status = 0x%x, flags = 0x%x, unit #%d.\n", tw_dev->host->host_no, command->status, command->flags, command->byte3.unit);

	/* Attempt to return intelligent sense information */
	if (fill_sense) {
		if ((command->status == 0xc7) || (command->status == 0xcb)) {
			for (i=0;i<(sizeof(tw_sense_table)/sizeof(tw_sense_table[0]));i++) {
				if (command->flags == tw_sense_table[i][0]) {

					/* Valid bit and 'current errors' */
					tw_dev->srb[request_id]->sense_buffer[0] = (0x1 << 7 | 0x70);

					/* Sense key */
					tw_dev->srb[request_id]->sense_buffer[2] = tw_sense_table[i][1];

					/* Additional sense length */
					tw_dev->srb[request_id]->sense_buffer[7] = 0xa; /* 10 bytes */

					/* Additional sense code */
					tw_dev->srb[request_id]->sense_buffer[12] = tw_sense_table[i][2];

					/* Additional sense code qualifier */
					tw_dev->srb[request_id]->sense_buffer[13] = tw_sense_table[i][3];

					tw_dev->srb[request_id]->result = (DID_OK << 16) | (CHECK_CONDITION << 1);
					return TW_ISR_DONT_RESULT; /* Special case for isr to not over-write result */
				}
			}
		}

		/* If no table match, error so we get a reset */
		return 1;
	}

	return 0;
} /* End tw_decode_sense() */

/* This function will disable interrupts on the controller */  
void tw_disable_interrupts(TW_Device_Extension *tw_dev) 
{
	u32 control_reg_value, control_reg_addr;

	control_reg_addr = tw_dev->registers.control_reg_addr;
	control_reg_value = TW_CONTROL_DISABLE_INTERRUPTS;
	outl(control_reg_value, control_reg_addr);
} /* End tw_disable_interrupts() */

/* This function will empty the response que */
void tw_empty_response_que(TW_Device_Extension *tw_dev) 
{
	u32 status_reg_addr, status_reg_value;
	u32 response_que_addr, response_que_value;

	status_reg_addr = tw_dev->registers.status_reg_addr;
	response_que_addr = tw_dev->registers.response_que_addr;
  
	status_reg_value = inl(status_reg_addr);

	while ((status_reg_value & TW_STATUS_RESPONSE_QUEUE_EMPTY) == 0) {
		response_que_value = inl(response_que_addr);
		status_reg_value = inl(status_reg_addr);
	}
} /* End tw_empty_response_que() */

/* This function will enable interrupts on the controller */
void tw_enable_interrupts(TW_Device_Extension *tw_dev)
{
	u32 control_reg_value, control_reg_addr;

	control_reg_addr = tw_dev->registers.control_reg_addr;
	control_reg_value = (TW_CONTROL_ENABLE_INTERRUPTS |
			     TW_CONTROL_UNMASK_RESPONSE_INTERRUPT);
	outl(control_reg_value, control_reg_addr);
} /* End tw_enable_interrupts() */

/* This function will enable interrupts on the controller */
void tw_enable_and_clear_interrupts(TW_Device_Extension *tw_dev)
{
	u32 control_reg_value, control_reg_addr;

	control_reg_addr = tw_dev->registers.control_reg_addr;
	control_reg_value = (TW_CONTROL_CLEAR_ATTENTION_INTERRUPT |
			     TW_CONTROL_UNMASK_RESPONSE_INTERRUPT |
			     TW_CONTROL_ENABLE_INTERRUPTS);
	outl(control_reg_value, control_reg_addr);
} /* End tw_enable_and_clear_interrupts() */

/* This function will find and initialize all cards */
int tw_findcards(Scsi_Host_Template *tw_host) 
{
	int numcards = 0, tries = 0, error = 0;
	struct Scsi_Host *host;
	TW_Device_Extension *tw_dev;
	TW_Device_Extension *tw_dev2;
	struct pci_dev *tw_pci_dev = NULL;
	u32 status_reg_value;
	unsigned char c = 1;
	int i, j = -1;
	u16 device[TW_NUMDEVICES] = { TW_DEVICE_ID, TW_DEVICE_ID2 };

	dprintk(KERN_NOTICE "3w-xxxx: tw_findcards()\n");

	for (i=0;i<TW_NUMDEVICES;i++) {
		while ((tw_pci_dev = pci_find_device(TW_VENDOR_ID, device[i], tw_pci_dev))) {
			j++;
			if (pci_enable_device(tw_pci_dev))
				continue;

			/* We only need 32-bit addressing for 5,6,7xxx cards */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,4)
			if (pci_set_dma_mask(tw_pci_dev, 0xffffffff)) {
				printk(KERN_WARNING "3w-xxxx: No suitable DMA available.\n");
				continue;
			}
#endif

			/* Prepare temporary device extension */
			tw_dev=(TW_Device_Extension *)kmalloc(sizeof(TW_Device_Extension), GFP_ATOMIC);
			if (tw_dev == NULL) {
				printk(KERN_WARNING "3w-xxxx: tw_findcards(): kmalloc() failed for card %d.\n", j);
				continue;
			}
			memset(tw_dev, 0, sizeof(TW_Device_Extension));

			/* Save pci_dev struct to device extension */
			tw_dev->tw_pci_dev = tw_pci_dev;

			error = tw_initialize_device_extension(tw_dev);
			if (error) {
				printk(KERN_WARNING "3w-xxxx: tw_findcards(): Couldn't initialize device extension for card %d.\n", j);
				tw_free_device_extension(tw_dev);
				kfree(tw_dev);
				continue;
			}

			/* Calculate the cards register addresses */
			tw_dev->registers.base_addr = pci_resource_start(tw_pci_dev, 0);
			tw_dev->registers.control_reg_addr = pci_resource_start(tw_pci_dev, 0);
			tw_dev->registers.status_reg_addr = pci_resource_start(tw_pci_dev, 0) + 0x4;
			tw_dev->registers.command_que_addr = pci_resource_start(tw_pci_dev, 0) + 0x8;
			tw_dev->registers.response_que_addr = pci_resource_start(tw_pci_dev, 0) + 0xC;

			/* Check for errors and clear them */
			status_reg_value = inl(tw_dev->registers.status_reg_addr);
			if (TW_STATUS_ERRORS(status_reg_value))
				tw_decode_bits(tw_dev, status_reg_value, 0);
			
			/* Poll status register for 60 secs for 'Controller Ready' flag */
			if (tw_poll_status(tw_dev, TW_STATUS_MICROCONTROLLER_READY, 60)) {
				printk(KERN_WARNING "3w-xxxx: tw_findcards(): Microcontroller not ready for card %d.\n", j);
				tw_free_device_extension(tw_dev);
				kfree(tw_dev);
				continue;
			}

			/* Disable interrupts on the card */
			tw_disable_interrupts(tw_dev);

			tries = 0;

			while (tries < TW_MAX_RESET_TRIES) {
				/* Do soft reset */
				tw_soft_reset(tw_dev);

				error = tw_aen_drain_queue(tw_dev);
				if (error) {
					printk(KERN_WARNING "3w-xxxx: AEN drain failed for card %d.\n", j);
					tries++;
					continue;
				}

				/* Check for controller errors */
				if (tw_check_errors(tw_dev)) {
					printk(KERN_WARNING "3w-xxxx: Controller errors found, retrying for card %d.\n", j);
					tries++;
					continue;
				}

				/* Now the controller is in a good state */
				break;
			}

			if (tries >= TW_MAX_RESET_TRIES) {
				printk(KERN_WARNING "3w-xxxx: Controller errors, card not responding, check all cabling for card %d.\n", j);
				tw_free_device_extension(tw_dev);
				kfree(tw_dev);
				continue;
			}

			/* Make sure that io region isn't already taken */
			if (check_region((tw_dev->tw_pci_dev->resource[0].start), TW_IO_ADDRESS_RANGE)) {
				printk(KERN_WARNING "3w-xxxx: tw_findcards(): Couldn't get io range 0x%lx-0x%lx for card %d.\n", 
				       (tw_dev->tw_pci_dev->resource[0].start), 
				       (tw_dev->tw_pci_dev->resource[0].start) + 
				       TW_IO_ADDRESS_RANGE, j);
				tw_free_device_extension(tw_dev);
				kfree(tw_dev);
				continue;
			}
    
			/* Reserve the io address space */
			request_region((tw_dev->tw_pci_dev->resource[0].start), TW_IO_ADDRESS_RANGE, TW_DEVICE_NAME);
			error = tw_initialize_units(tw_dev);
			if (error) {
				printk(KERN_WARNING "3w-xxxx: No valid units for card %d.\n", j);
			}

			error = tw_initconnection(tw_dev, TW_INIT_MESSAGE_CREDITS);
			if (error) {
				printk(KERN_WARNING "3w-xxxx: Connection initialization failed for card %d.\n", j);
				release_region((tw_dev->tw_pci_dev->resource[0].start), TW_IO_ADDRESS_RANGE);
				tw_free_device_extension(tw_dev);
				kfree(tw_dev);
				continue;
			}

			/* Set card status as online */
			tw_dev->online = 1;

			/* Calculate max cmds per lun, and setup queues */
			if ((tw_dev->num_raid_five > 0) && (tw_dev->tw_pci_dev->device == TW_DEVICE_ID)) {
				tw_host->cmd_per_lun = (TW_MAX_BOUNCEBUF-2)/tw_dev->num_units;
				tw_dev->free_head = TW_Q_START;
				tw_dev->free_tail = TW_Q_START;
				tw_dev->free_wrap = TW_MAX_BOUNCEBUF - 1;
			} else {
				/* Check for user configured cmd_per_lun */
#ifdef CONFIG_3W_XXXX_CMD_PER_LUN
				tw_host->cmd_per_lun = CONFIG_3W_XXXX_CMD_PER_LUN;
				if (tw_host->cmd_per_lun > TW_MAX_CMDS_PER_LUN)
					tw_host->cmd_per_lun = TW_MAX_CMDS_PER_LUN;
#else
				/* Use SHT cmd_per_lun default */
				tw_host->cmd_per_lun = TW_MAX_CMDS_PER_LUN;
#endif
				tw_dev->free_head = TW_Q_START;
				tw_dev->free_tail = TW_Q_START;
				tw_dev->free_wrap = TW_Q_LENGTH - 1;
			}

		/* Register the card with the kernel SCSI layer */
			host = scsi_register(tw_host, sizeof(TW_Device_Extension));
			if (host == NULL) {
				printk(KERN_WARNING "3w-xxxx: tw_findcards(): scsi_register() failed for card %d.\n", j);
				release_region((tw_dev->tw_pci_dev->resource[0].start), TW_IO_ADDRESS_RANGE);
				tw_free_device_extension(tw_dev);
				kfree(tw_dev);
				continue;
			}

			/* Set max target id's */
			host->max_id = TW_MAX_UNITS;

			/* Set max cdb size in bytes */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,15)			
			host->max_cmd_len = TW_MAX_CDB_LEN;
#endif

			/* Set max sectors per io */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,7)
			if ((tw_dev->num_raid_five > 0) && (tw_dev->tw_pci_dev->device == TW_DEVICE_ID))
				host->max_sectors = TW_MAX_BOUNCE_SECTORS;
			else
				host->max_sectors = TW_MAX_SECTORS;
#endif

			host->select_queue_depths = tw_select_queue_depths;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,4)
			scsi_set_pci_device(host, tw_pci_dev);
#endif

			status_reg_value = inl(tw_dev->registers.status_reg_addr);

			printk(KERN_NOTICE "scsi%d : Found a 3ware Storage Controller at 0x%x, IRQ: %d, P-chip: %d.%d\n", host->host_no,
				(u32)(tw_pci_dev->resource[0].start), tw_pci_dev->irq, 
				(status_reg_value & TW_STATUS_MAJOR_VERSION_MASK) >> 28, 
				(status_reg_value & TW_STATUS_MINOR_VERSION_MASK) >> 24);

			if (host->hostdata) {
				tw_dev2 = (TW_Device_Extension *)host->hostdata;
				memcpy(tw_dev2, tw_dev, sizeof(TW_Device_Extension));
				/* Need to init the sem/wqueue after the copy */
				init_MUTEX(&tw_dev2->ioctl_sem);
				init_waitqueue_head(&tw_dev2->ioctl_wqueue);

				tw_device_extension_list[tw_device_extension_count] = tw_dev2;
				numcards++;
				tw_device_extension_count = numcards;
				tw_dev2->host = host;
			} else { 
				printk(KERN_WARNING "3w-xxxx: tw_findcards(): Bad scsi host data for card %d.\n", j);
				scsi_unregister(host);
				release_region((tw_dev->tw_pci_dev->resource[0].start), TW_IO_ADDRESS_RANGE);
				tw_free_device_extension(tw_dev);
				kfree(tw_dev);
				continue;
			}

			/* Tell the firmware we support shutdown notification*/
			error = tw_setfeature(tw_dev2, 2, 1, &c);
			if (error) {
				printk(KERN_WARNING "3w-xxxx: Unable to set features for card %d, old firmware or card.\n", j);
			}

			/* Now setup the interrupt handler */
			error = tw_setup_irq(tw_dev2);
			if (error) {
				printk(KERN_WARNING "3w-xxxx: tw_findcards(): Error requesting irq for card %d.\n", j);
				scsi_unregister(host);
				release_region((tw_dev->tw_pci_dev->resource[0].start), TW_IO_ADDRESS_RANGE);

				tw_free_device_extension(tw_dev);
				kfree(tw_dev);
				numcards--;
				continue;
			}

			/* Re-enable interrupts on the card */
			tw_enable_interrupts(tw_dev2);

			/* Free the temporary device extension */
			if (tw_dev)
				kfree(tw_dev);
		}
	}

	if (numcards == 0) {
		printk(KERN_WARNING "3w-xxxx: No cards found.\n");
	} else {
		register_reboot_notifier(&tw_notifier);
		if ((twe_major = register_chrdev (0, "twe", &tw_fops)) < 0) {
			printk(KERN_WARNING "3w-xxxx: Unable to register \"twe\" character device, error = %d.\n", twe_major);
		}
	}

	return numcards;
} /* End tw_findcards() */

/* This function will free up device extension resources */
void tw_free_device_extension(TW_Device_Extension *tw_dev)
{
	dprintk(KERN_NOTICE "3w-xxxx: tw_free_device_extension()\n");

	/* Free command packet and generic buffer memory */
	if (tw_dev->command_packet_physical_address[0])
		pci_free_consistent(tw_dev->tw_pci_dev, sizeof(TW_Command)*TW_Q_LENGTH, tw_dev->command_packet_virtual_address[0], tw_dev->command_packet_physical_address[0]);

	if (tw_dev->alignment_physical_address[0])
		pci_free_consistent(tw_dev->tw_pci_dev, sizeof(TW_Sector)*TW_Q_LENGTH, tw_dev->alignment_virtual_address[0], tw_dev->alignment_physical_address[0]);

	if (tw_dev->bounce_buffer[0])
		pci_free_consistent(tw_dev->tw_pci_dev, sizeof(TW_Sector)*TW_MAX_BOUNCE_SECTORS*TW_MAX_BOUNCEBUF, tw_dev->bounce_buffer[0], tw_dev->bounce_buffer_phys[0]);
} /* End tw_free_device_extension() */

/* Clean shutdown routine */
static int tw_halt(struct notifier_block *nb, ulong event, void *buf)
{
	int i;

	for (i=0;i<tw_device_extension_count;i++) {
		if (tw_device_extension_list[i]->online == 1) {
			printk(KERN_NOTICE "3w-xxxx: Shutting down card %d.\n", i);
			tw_shutdown_device(tw_device_extension_list[i]);
			tw_device_extension_list[i]->online = 0;
		}
	}
	unregister_reboot_notifier(&tw_notifier);

	return NOTIFY_OK;
} /* End tw_halt() */

/* This function will send an initconnection command to controller */
int tw_initconnection(TW_Device_Extension *tw_dev, int message_credits) 
{
	unsigned long command_que_value;
	u32 command_que_addr;
	u32 response_que_addr;
	TW_Command  *command_packet;
	TW_Response_Queue response_queue;
	int request_id = 0;

	dprintk(KERN_NOTICE "3w-xxxx: tw_initconnection()\n");
	command_que_addr = tw_dev->registers.command_que_addr;
	response_que_addr = tw_dev->registers.response_que_addr;

	/* Initialize InitConnection command packet */
	if (tw_dev->command_packet_virtual_address[request_id] == NULL) {
		printk(KERN_WARNING "3w-xxxx: tw_initconnection(): Bad command packet virtual address.\n");
		return 1;
	}

	command_packet = (TW_Command *)tw_dev->command_packet_virtual_address[request_id];
	memset(command_packet, 0, sizeof(TW_Sector));
	command_packet->byte0.opcode = TW_OP_INIT_CONNECTION;
	command_packet->byte0.sgl_offset = 0x0;
	command_packet->size = TW_INIT_COMMAND_PACKET_SIZE;
	command_packet->request_id = request_id;
	command_packet->byte3.unit = 0x0;
	command_packet->byte3.host_id = 0x0;
	command_packet->status = 0x0;
	command_packet->flags = 0x0;
	command_packet->byte6.message_credits = message_credits; 
	command_packet->byte8.init_connection.response_queue_pointer = 0x0;
	command_que_value = tw_dev->command_packet_physical_address[request_id];

	if (command_que_value == 0) {
		printk(KERN_WARNING "3w-xxxx: tw_initconnection(): Bad command packet physical address.\n");
		return 1;
	}
  
	/* Send command packet to the board */
	outl(command_que_value, command_que_addr);
    
	/* Poll for completion */
	if (tw_poll_status_gone(tw_dev, TW_STATUS_RESPONSE_QUEUE_EMPTY, 30) == 0) {
		response_queue.value = inl(response_que_addr);
		request_id = (unsigned char)response_queue.u.response_id;
		if (request_id != 0) {
			/* unexpected request id */
			printk(KERN_WARNING "3w-xxxx: tw_initconnection(): Unexpected request id.\n");
			return 1;
		}
		if (command_packet->status != 0) {
			/* bad response */
			tw_decode_sense(tw_dev, request_id, 0);
			return 1;
		}
	}
	return 0;
} /* End tw_initconnection() */

/* This function will initialize the fields of a device extension */
int tw_initialize_device_extension(TW_Device_Extension *tw_dev)
{
	int i, error = 0;

	dprintk(KERN_NOTICE "3w-xxxx: tw_initialize_device_extension()\n");

	/* Initialize command packet buffers */
	error = tw_allocate_memory(tw_dev, sizeof(TW_Command), 0);
	if (error) {
		printk(KERN_WARNING "3w-xxxx: Command packet memory allocation failed.\n");
		return 1;
	}

	/* Initialize generic buffer */
	error = tw_allocate_memory(tw_dev, sizeof(TW_Sector), 1);
	if (error) {
		printk(KERN_WARNING "3w-xxxx: Generic memory allocation failed.\n");
		return 1;
	}
	
	for (i=0;i<TW_Q_LENGTH;i++) {
		tw_dev->free_queue[i] = i;
		tw_dev->state[i] = TW_S_INITIAL;
	}

	tw_dev->pending_head = TW_Q_START;
	tw_dev->pending_tail = TW_Q_START;
	spin_lock_init(&tw_dev->tw_lock);
	tw_dev->chrdev_request_id = TW_IOCTL_CHRDEV_FREE;

	return 0;
} /* End tw_initialize_device_extension() */

/* This function will get unit info from the controller */
int tw_initialize_units(TW_Device_Extension *tw_dev) 
{
	int found = 0, error = 0;
	unsigned char request_id = 0;
	TW_Command *command_packet;
	TW_Param *param;
	int i, j, imax, num_units = 0, num_raid_five = 0;
	unsigned long command_que_value;
	u32 command_que_addr;
	u32 response_que_addr;
	TW_Response_Queue response_queue;
	unsigned long param_value;
	unsigned char *is_unit_present;
	unsigned char *raid_level;

	dprintk(KERN_NOTICE "3w-xxxx: tw_initialize_units()\n");

	command_que_addr = tw_dev->registers.command_que_addr;
	response_que_addr = tw_dev->registers.response_que_addr;
  
	/* Setup the command packet */
	command_packet = (TW_Command *)tw_dev->command_packet_virtual_address[request_id];
	if (command_packet == NULL) {
		printk(KERN_WARNING "3w-xxxx: tw_initialize_units(): Bad command packet virtual address.\n");
		return 1;
	}
	memset(command_packet, 0, sizeof(TW_Sector));
	command_packet->byte0.opcode      = TW_OP_GET_PARAM;
	command_packet->byte0.sgl_offset  = 2;
	command_packet->size              = 4;
	command_packet->request_id        = request_id;
	command_packet->byte3.unit        = 0;
	command_packet->byte3.host_id     = 0;
	command_packet->status            = 0;
	command_packet->flags             = 0;
	command_packet->byte6.block_count = 1;

	/* Now setup the param */
	if (tw_dev->alignment_virtual_address[request_id] == NULL) {
		printk(KERN_WARNING "3w-xxxx: tw_initialize_units(): Bad alignment virtual address.\n");
		return 1;
	}
	param = (TW_Param *)tw_dev->alignment_virtual_address[request_id];
	memset(param, 0, sizeof(TW_Sector));
	param->table_id = 3;       /* unit summary table */
	param->parameter_id = 3;   /* unitstatus parameter */
	param->parameter_size_bytes = TW_MAX_UNITS;
	param_value = tw_dev->alignment_physical_address[request_id];
	if (param_value == 0) {
		printk(KERN_WARNING "3w-xxxx: tw_initialize_units(): Bad alignment physical address.\n");
		return 1;
	}

	command_packet->byte8.param.sgl[0].address = param_value;
	command_packet->byte8.param.sgl[0].length = sizeof(TW_Sector);

	/* Post the command packet to the board */
	command_que_value = tw_dev->command_packet_physical_address[request_id];
	if (command_que_value == 0) {
		printk(KERN_WARNING "3w-xxxx: tw_initialize_units(): Bad command packet physical address.\n");
		return 1;
	}
	outl(command_que_value, command_que_addr);

	/* Poll for completion */
	if (tw_poll_status_gone(tw_dev, TW_STATUS_RESPONSE_QUEUE_EMPTY, 30) == 0) {
		response_queue.value = inl(response_que_addr);
		request_id = (unsigned char)response_queue.u.response_id;
		if (request_id != 0) {
			/* unexpected request id */
			printk(KERN_WARNING "3w-xxxx: tw_initialize_units(): Unexpected request id.\n");
			return 1;
		}
		if (command_packet->status != 0) {
			/* bad response */
			tw_decode_sense(tw_dev, request_id, 0);
			return 1;
		}
		found = 1;
	}
	if (found == 0) {
		/* response never received */
		printk(KERN_WARNING "3w-xxxx: tw_initialize_units(): No response.\n");
		return 1;
	}

	param = (TW_Param *)tw_dev->alignment_virtual_address[request_id];
	is_unit_present = (unsigned char *)&(param->data[0]);
  
	/* Show all units present */
	imax = TW_MAX_UNITS;
	for(i=0; i<imax; i++) {
		if (is_unit_present[i] == 0) {
			tw_dev->is_unit_present[i] = FALSE;
		} else {
		  if (is_unit_present[i] & TW_UNIT_ONLINE) {
			dprintk(KERN_NOTICE "3w-xxxx: tw_initialize_units(): Unit %d found.\n", i);
			tw_dev->is_unit_present[i] = TRUE;
			num_units++;
		  }
		}
	}
	tw_dev->num_units = num_units;

	if (num_units == 0) {
		dprintk(KERN_NOTICE "3w-xxxx: tw_initialize_units(): No units found.\n");
		return 1;
	}

	/* Find raid 5 arrays */
	for (j=0;j<TW_MAX_UNITS;j++) {
		if (tw_dev->is_unit_present[j] == 0)
			continue;
		command_packet = (TW_Command *)tw_dev->command_packet_virtual_address[request_id];
		if (command_packet == NULL) {
			printk(KERN_WARNING "3w-xxxx: tw_initialize_units(): Bad command packet virtual address.\n");
			return 1;
		}
		memset(command_packet, 0, sizeof(TW_Sector));
		command_packet->byte0.opcode      = TW_OP_GET_PARAM;
		command_packet->byte0.sgl_offset  = 2;
		command_packet->size              = 4;
		command_packet->request_id        = request_id;
		command_packet->byte3.unit        = 0;
		command_packet->byte3.host_id     = 0;
		command_packet->status            = 0;
		command_packet->flags             = 0;
		command_packet->byte6.block_count = 1;

		/* Now setup the param */
		if (tw_dev->alignment_virtual_address[request_id] == NULL) {
			printk(KERN_WARNING "3w-xxxx: tw_initialize_units(): Bad alignment virtual address.\n");
			return 1;
		}
		param = (TW_Param *)tw_dev->alignment_virtual_address[request_id];
		memset(param, 0, sizeof(TW_Sector));
		param->table_id = 0x300+j; /* unit summary table */
		param->parameter_id = 0x6; /* unit descriptor */
		param->parameter_size_bytes = 0xc;
		param_value = tw_dev->alignment_physical_address[request_id];
		if (param_value == 0) {
			printk(KERN_WARNING "3w-xxxx: tw_initialize_units(): Bad alignment physical address.\n");
			return 1;
		}

		command_packet->byte8.param.sgl[0].address = param_value;
		command_packet->byte8.param.sgl[0].length = sizeof(TW_Sector);

		/* Post the command packet to the board */
		command_que_value = tw_dev->command_packet_physical_address[request_id];
		if (command_que_value == 0) {
			printk(KERN_WARNING "3w-xxxx: tw_initialize_units(): Bad command packet physical address.\n");
			return 1;
		}
		outl(command_que_value, command_que_addr);

		/* Poll for completion */
		if (tw_poll_status_gone(tw_dev, TW_STATUS_RESPONSE_QUEUE_EMPTY, 30) == 0) {
			response_queue.value = inl(response_que_addr);
			request_id = (unsigned char)response_queue.u.response_id;
			if (request_id != 0) {
				/* unexpected request id */
				printk(KERN_WARNING "3w-xxxx: tw_initialize_units(): Unexpected request id.\n");
				return 1;
			}
			if (command_packet->status != 0) {
				/* bad response */
				tw_decode_sense(tw_dev, request_id, 0);
				return 1;
			}
			found = 1;
		}
		if (found == 0) {
			/* response never received */
			printk(KERN_WARNING "3w-xxxx: tw_initialize_units(): No response.\n");
			return 1;
		}

		param = (TW_Param *)tw_dev->alignment_virtual_address[request_id];
		raid_level = (unsigned char *)&(param->data[1]);
		if (*raid_level == 5) {
			dprintk(KERN_WARNING "3w-xxxx: Found unit %d to be a raid5 unit.\n", j);
			tw_dev->is_raid_five[j] = 1;
			num_raid_five++;
		}
	}
	tw_dev->num_raid_five = num_raid_five;

	/* Now allocate raid5 bounce buffers */
	if ((num_raid_five != 0) && (tw_dev->tw_pci_dev->device == TW_DEVICE_ID)) {
		error = tw_allocate_memory(tw_dev, sizeof(TW_Sector)*TW_MAX_BOUNCE_SECTORS, 2);
		if (error) {
			printk(KERN_WARNING "3w-xxxx: Bounce buffer allocation failed.\n");
			return 1;
		}
	}
  
	return 0;
} /* End tw_initialize_units() */

/* This function is the interrupt service routine */
static void tw_interrupt(int irq, void *dev_instance, struct pt_regs *regs) 
{
	int request_id;
	u32 status_reg_addr, status_reg_value;
	u32 response_que_addr;
	TW_Device_Extension *tw_dev = (TW_Device_Extension *)dev_instance;
	TW_Response_Queue response_que;
	int error = 0, retval = 0;
	unsigned long flags = 0;
	TW_Command *command_packet;

	dprintk(KERN_WARNING "3w-xxxx: tw_interrupt()\n");

	/* See if we are already running on another processor */
	if (test_and_set_bit(TW_IN_INTR, &tw_dev->flags))
		return;

	/* Get the block layer lock for io completions */
	spin_lock_irqsave(&io_request_lock, flags);
	
	/* See if the interrupt matches this instance */
	if (tw_dev->tw_pci_dev->irq == irq) {

		/* Make sure io isn't queueing */
		spin_lock(&tw_dev->tw_lock);

		/* Read the registers */
		status_reg_addr = tw_dev->registers.status_reg_addr;
		response_que_addr = tw_dev->registers.response_que_addr;
		status_reg_value = inl(status_reg_addr);

		/* Check if this is our interrupt, otherwise bail */
		if (!(status_reg_value & TW_STATUS_VALID_INTERRUPT))
			goto tw_interrupt_bail;

		/* Check controller for errors */
		if (tw_check_bits(status_reg_value)) {
			dprintk(KERN_WARNING "3w-xxxx: tw_interrupt(): Unexpected bits.\n");
			if (tw_decode_bits(tw_dev, status_reg_value, 1)) {
				tw_clear_all_interrupts(tw_dev);
				goto tw_interrupt_bail;
			}
		}

		/* Handle host interrupt */
		if (status_reg_value & TW_STATUS_HOST_INTERRUPT) {
			dprintk(KERN_NOTICE "3w-xxxx: tw_interrupt(): Received host interrupt.\n");
			tw_clear_host_interrupt(tw_dev);
		}

		/* Handle attention interrupt */
		if (status_reg_value & TW_STATUS_ATTENTION_INTERRUPT) {
			dprintk(KERN_NOTICE "3w-xxxx: tw_interrupt(): Received attention interrupt.\n");
			tw_clear_attention_interrupt(tw_dev);
			tw_state_request_start(tw_dev, &request_id);
			error = tw_aen_read_queue(tw_dev, request_id);
			if (error) {
				printk(KERN_WARNING "3w-xxxx: scsi%d: Error reading aen queue.\n", tw_dev->host->host_no);
				tw_dev->state[request_id] = TW_S_COMPLETED;
				tw_state_request_finish(tw_dev, request_id);
			}
		}

		/* Handle command interrupt */
		if (status_reg_value & TW_STATUS_COMMAND_INTERRUPT) {
			/* Drain as many pending commands as we can */
			while (tw_dev->pending_request_count > 0) {
				request_id = tw_dev->pending_queue[tw_dev->pending_head];
				if (tw_dev->state[request_id] != TW_S_PENDING) {
					printk(KERN_WARNING "3w-xxxx: scsi%d: Found request id that wasn't pending.\n", tw_dev->host->host_no);
					break;
				}
				if (tw_post_command_packet(tw_dev, request_id)==0) {
					if (tw_dev->pending_head == TW_Q_LENGTH-1) {
						tw_dev->pending_head = TW_Q_START;
					} else {
						tw_dev->pending_head = tw_dev->pending_head + 1;
					}
					tw_dev->pending_request_count--;
				} else {
					/* If we get here, we will continue re-posting on the next command interrupt */
					break;
				}
			}
			/* If there are no more pending requests, we mask command interrupt */
			if (tw_dev->pending_request_count == 0) 
				tw_mask_command_interrupt(tw_dev);
		}

		/* Handle response interrupt */
		if (status_reg_value & TW_STATUS_RESPONSE_INTERRUPT) {
			/* Drain the response queue from the board */
			while ((status_reg_value & TW_STATUS_RESPONSE_QUEUE_EMPTY) == 0) {
				/* Read response queue register */
				response_que.value = inl(response_que_addr);
				request_id = response_que.u.response_id;
				command_packet = (TW_Command *)tw_dev->command_packet_virtual_address[request_id];
				error = 0;

				/* Check for bad response */
				if (command_packet->status != 0) {
					/* If internal command, don't error, don't fill sense */
					if (tw_dev->srb[request_id] == 0) {
						tw_decode_sense(tw_dev, request_id, 0);
					} else {
						error = tw_decode_sense(tw_dev, request_id, 1);
					}
				}

				/* Check for correct state */
				if (tw_dev->state[request_id] != TW_S_POSTED) {
					/* Handle timed out ioctl's */
					if (tw_dev->srb[request_id] != 0) {
						if (tw_dev->srb[request_id]->cmnd[0] != TW_IOCTL) {
							printk(KERN_WARNING "3w-xxxx: scsi%d: Received a request id (%d) (opcode = 0x%x) that wasn't posted.\n", tw_dev->host->host_no, request_id, command_packet->byte0.opcode);
							error = 1;
						}
					}
				}

				dprintk(KERN_NOTICE "3w-xxxx: tw_interrupt(): Response queue request id: %d.\n", request_id);

				/* Check for internal command completion */
				if (tw_dev->srb[request_id] == 0) {
					dprintk(KERN_WARNING "3w-xxxx: tw_interrupt(): Found internally posted command.\n");
					/* Check for chrdev ioctl completion */
					if (request_id != tw_dev->chrdev_request_id) {
						retval = tw_aen_complete(tw_dev, request_id);
						if (retval) {
							printk(KERN_WARNING "3w-xxxx: scsi%d: Error completing aen.\n", tw_dev->host->host_no);
						}
					} else {
						tw_dev->chrdev_request_id = TW_IOCTL_CHRDEV_FREE;
						wake_up(&tw_dev->ioctl_wqueue);
					}
				} else {
				switch (tw_dev->srb[request_id]->cmnd[0]) {
					case READ_10:
					case READ_6:
						dprintk(KERN_NOTICE "3w-xxxx: tw_interrupt(): caught READ_10/READ_6\n");
						break;
					case WRITE_10:
					case WRITE_6:
						dprintk(KERN_NOTICE "3w-xxxx: tw_interrupt(): caught WRITE_10/WRITE_6\n");
						break;
					case TEST_UNIT_READY:
						dprintk(KERN_NOTICE "3w-xxxx: tw_interrupt(): caught TEST_UNIT_READY\n");
						error = tw_scsiop_test_unit_ready_complete(tw_dev, request_id);
						break;
					case INQUIRY:
						dprintk(KERN_NOTICE "3w-xxxx: tw_interrupt(): caught INQUIRY\n");
						error = tw_scsiop_inquiry_complete(tw_dev, request_id);
						break;
					case READ_CAPACITY:
						dprintk(KERN_NOTICE "3w-xxxx: tw_interrupt(): caught READ_CAPACITY\n");
						error = tw_scsiop_read_capacity_complete(tw_dev, request_id);
						break;
					case MODE_SENSE:
						dprintk(KERN_NOTICE "3w-xxxx: tw_interrupt(): caught MODE_SENSE\n");
						error = tw_scsiop_mode_sense_complete(tw_dev, request_id);
						break;
					case SYNCHRONIZE_CACHE:
						dprintk(KERN_NOTICE "3w-xxxx: tw_interrupt(): caught SYNCHRONIZE_CACHE\n");
						break;
					case TW_IOCTL:
						dprintk(KERN_NOTICE "3w-xxxx: tw_interrupt(): caught TW_IOCTL\n");
						error = tw_ioctl_complete(tw_dev, request_id);
						break;
					default:
						printk(KERN_WARNING "3w-xxxx: case slip in tw_interrupt()\n");
						error = 1;
					}

					/* If no error command was a success */
					if (error == 0) {
						tw_dev->srb[request_id]->result = (DID_OK << 16);
					}

					/* If error, command failed */
					if (error == 1) {
						/* Ask for a host reset */
						tw_dev->srb[request_id]->result = (DID_OK << 16) | (CHECK_CONDITION << 1);
					}
					
					/* Now complete the io */
					if ((error != TW_ISR_DONT_COMPLETE)) {
						tw_dev->state[request_id] = TW_S_COMPLETED;
						tw_state_request_finish(tw_dev, request_id);
						tw_dev->posted_request_count--;
						tw_dev->srb[request_id]->scsi_done(tw_dev->srb[request_id]);
						tw_unmap_scsi_data(tw_dev->tw_pci_dev, tw_dev->srb[request_id]);
					}
				}

				/* Check for valid status after each drain */
				status_reg_value = inl(status_reg_addr);
				if (tw_check_bits(status_reg_value)) {
					dprintk(KERN_WARNING "3w-xxxx: tw_interrupt(): Unexpected bits.\n");
					if (tw_decode_bits(tw_dev, status_reg_value, 1)) {
						tw_clear_all_interrupts(tw_dev);
						goto tw_interrupt_bail;
					}
				}
			}
		}
tw_interrupt_bail:
		spin_unlock(&tw_dev->tw_lock);
	} else
		dprintk(KERN_WARNING "3w-xxxx: tw_interrupt() called for wrong instance.\n");

	spin_unlock_irqrestore(&io_request_lock, flags);
	clear_bit(TW_IN_INTR, &tw_dev->flags);
} /* End tw_interrupt() */

/* This function handles ioctls from userspace to the driver */
int tw_ioctl(TW_Device_Extension *tw_dev, int request_id)
{
	unsigned char opcode;
	int bufflen, error = 0;
	TW_Param *param;
	TW_Command *command_packet, *command_save;
	unsigned long param_value;
	TW_Ioctl *ioctl = NULL;
	TW_Passthru *passthru = NULL;
	int tw_aen_code, i, use_sg;
	unsigned long *data_ptr;
	int total_bytes = 0, posted = 0;
	dma_addr_t dma_handle;
	struct timeval before, timeout;

	ioctl = (TW_Ioctl *)tw_dev->srb[request_id]->request_buffer;
	if (ioctl == NULL) {
		printk(KERN_WARNING "3w-xxxx: tw_ioctl(): Request buffer NULL.\n");
		tw_dev->state[request_id] = TW_S_COMPLETED;
		tw_state_request_finish(tw_dev, request_id);
		tw_dev->srb[request_id]->result = (DID_OK << 16);
		tw_dev->srb[request_id]->scsi_done(tw_dev->srb[request_id]);
		return 0;
	}
	bufflen = tw_dev->srb[request_id]->request_bufflen;

	/* Initialize command packet */
	command_packet = (TW_Command *)tw_dev->command_packet_virtual_address[request_id];
	if (command_packet == NULL) {
		printk(KERN_WARNING "3w-xxxx: tw_ioctl(): Bad command packet virtual address.\n");
		tw_dev->state[request_id] = TW_S_COMPLETED;
		tw_state_request_finish(tw_dev, request_id);
		tw_dev->srb[request_id]->result = (DID_OK << 16);
		tw_dev->srb[request_id]->scsi_done(tw_dev->srb[request_id]);
		return 0;
	}
	memset(command_packet, 0, sizeof(TW_Sector));

	/* Initialize param */
	if (tw_dev->alignment_virtual_address[request_id] == NULL) {
		printk(KERN_WARNING "3w-xxxx: tw_ioctl(): Bad alignment virtual address.\n");
		tw_dev->state[request_id] = TW_S_COMPLETED;
		tw_state_request_finish(tw_dev, request_id);
		tw_dev->srb[request_id]->result = (DID_OK << 16);
		tw_dev->srb[request_id]->scsi_done(tw_dev->srb[request_id]);
		return 0;
	}
	param = (TW_Param *)tw_dev->alignment_virtual_address[request_id];
	memset(param, 0, sizeof(TW_Sector));

	dprintk(KERN_NOTICE "opcode = %d table_id = %d parameter_id = %d parameter_size_bytes = %d\n", ioctl->opcode, ioctl->table_id, ioctl->parameter_id, ioctl->parameter_size_bytes);
	opcode = ioctl->opcode;

	switch (opcode) {
		case TW_OP_NOP:
			dprintk(KERN_NOTICE "3w-xxxx: tw_ioctl(): caught TW_OP_NOP.\n");
			command_packet->byte0.opcode = TW_OP_NOP;
			break;
		case TW_OP_GET_PARAM:
			dprintk(KERN_NOTICE "3w-xxxx: tw_ioctl(): caught TW_OP_GET_PARAM.\n");
			command_packet->byte0.opcode = TW_OP_GET_PARAM;
			command_packet->byte3.unit = ioctl->unit_index;
			param->table_id = ioctl->table_id;
			param->parameter_id = ioctl->parameter_id;
			param->parameter_size_bytes = ioctl->parameter_size_bytes;
			tw_dev->ioctl_size[request_id] = ioctl->parameter_size_bytes;
			dprintk(KERN_NOTICE "table_id = %d parameter_id = %d parameter_size_bytes %d\n", param->table_id, param->parameter_id, param->parameter_size_bytes);
			break;
		case TW_OP_SET_PARAM:
			dprintk(KERN_NOTICE "3w-xxxx: tw_ioctl(): caught TW_OP_SET_PARAM: table_id = %d, parameter_id = %d, parameter_size_bytes = %d.\n",
			ioctl->table_id, ioctl->parameter_id, ioctl->parameter_size_bytes);
			if (ioctl->data != NULL) {
				command_packet->byte0.opcode = TW_OP_SET_PARAM;
				param->table_id = ioctl->table_id;
				param->parameter_id = ioctl->parameter_id;
				param->parameter_size_bytes = ioctl->parameter_size_bytes;
				memcpy(param->data, ioctl->data, ioctl->parameter_size_bytes);
				break;
			} else {
				printk(KERN_WARNING "3w-xxxx: tw_ioctl(): ioctl->data NULL.\n");
				return 1;
			}
		case TW_OP_AEN_LISTEN:
			dprintk(KERN_NOTICE "3w-xxxx: tw_ioctl(): caught TW_OP_AEN_LISTEN.\n");
			if (tw_dev->aen_head == tw_dev->aen_tail) {
				/* aen queue empty */
				dprintk(KERN_NOTICE "3w-xxxx: tw_ioctl(): Aen queue empty.\n");
				tw_aen_code = TW_AEN_QUEUE_EMPTY;
				memcpy(tw_dev->srb[request_id]->request_buffer, &tw_aen_code, ioctl->parameter_size_bytes);
			} else {
				/* Copy aen queue entry to request buffer */
				dprintk(KERN_NOTICE "3w-xxxx: tw_ioctl(): Returning aen 0x%x\n", tw_dev->aen_queue[tw_dev->aen_head]);
				tw_aen_code = tw_dev->aen_queue[tw_dev->aen_head];
				memcpy(tw_dev->srb[request_id]->request_buffer, &tw_aen_code, ioctl->parameter_size_bytes);
				if (tw_dev->aen_head == TW_Q_LENGTH - 1) {
					tw_dev->aen_head = TW_Q_START;
				} else {
					tw_dev->aen_head = tw_dev->aen_head + 1;
				}
			}
			tw_dev->state[request_id] = TW_S_COMPLETED;
			tw_state_request_finish(tw_dev, request_id);
			tw_dev->srb[request_id]->result = (DID_OK << 16);
			tw_dev->srb[request_id]->scsi_done(tw_dev->srb[request_id]);
			return 0;
		case TW_ATA_PASSTHRU:
			if (ioctl->data != NULL) {
				memcpy(command_packet, ioctl->data, sizeof(TW_Command));
				command_packet->request_id = request_id;
			} else {
				printk(KERN_WARNING "3w-xxxx: tw_ioctl(): ioctl->data NULL.\n");
				return 1;
			}

			passthru = (TW_Passthru *)tw_dev->command_packet_virtual_address[request_id];
			/* Don't load sg_list for non-data ATA cmds */
			if ((passthru->param != 0) && (passthru->param != 0x8)) {
				passthru->sg_list[0].length = passthru->sector_count*512;
				if (passthru->sg_list[0].length > TW_MAX_PASSTHRU_BYTES) {
					printk(KERN_WARNING "3w-xxxx: tw_ioctl(): Passthru size (%d) too big.\n", passthru->sg_list[0].length);
					return 1;
				}
				passthru->sg_list[0].address = tw_dev->alignment_physical_address[request_id];
			}
			tw_post_command_packet(tw_dev, request_id);
			return 0;
		case TW_CMD_PACKET:
			dprintk(KERN_WARNING "3w-xxxx: tw_ioctl(): caught TW_CMD_PACKET.\n");
			if (ioctl->data != NULL) {
				memcpy(command_packet, ioctl->data, sizeof(TW_Command));
				command_packet->request_id = request_id;
				tw_post_command_packet(tw_dev, request_id);
				return 0;
			} else {
				printk(KERN_WARNING "3w-xxxx: tw_ioctl(): ioctl->data NULL.\n");
				return 1;
			}
		case TW_CMD_PACKET_WITH_DATA:
			dprintk(KERN_WARNING "3w-xxxx: tw_ioctl(): caught TW_CMD_PACKET_WITH_DATA.\n");
			command_save = (TW_Command *)tw_dev->alignment_virtual_address[request_id];
			if (command_save == NULL) {
				printk(KERN_WARNING "3w-xxxx: scsi%d: tw_ioctl(): Bad alignment virtual address.\n", tw_dev->host->host_no);
				return 1;
			}
			if (ioctl->data != NULL) {
				/* Copy down the command packet */
				memcpy(command_packet, ioctl->data, sizeof(TW_Command));
				memcpy(command_save, ioctl->data, sizeof(TW_Command));
				command_packet->request_id = request_id;

				/* Now deal with the two possible sglists */
				if (command_packet->byte0.sgl_offset == 2) {
					use_sg = command_packet->size - 3;
					for (i=0;i<use_sg;i++)
						total_bytes+=command_packet->byte8.param.sgl[i].length;
					tw_dev->ioctl_data[request_id] = pci_alloc_consistent(tw_dev->tw_pci_dev, total_bytes, &dma_handle);

					if (!tw_dev->ioctl_data[request_id]) {
						printk(KERN_WARNING "3w-xxxx: scsi%d: tw_ioctl(): pci_alloc_consistent() failed for request_id %d.\n", tw_dev->host->host_no, request_id);
						return 1;
					}

					/* Copy param sglist into the kernel */
					data_ptr = tw_dev->ioctl_data[request_id];
					for (i=0;i<use_sg;i++) {
						if (command_packet->byte8.param.sgl[i].address != 0) {
							error = copy_from_user(data_ptr, (void *)(unsigned long)command_packet->byte8.param.sgl[i].address, command_packet->byte8.param.sgl[i].length);
							if (error) {
								dprintk(KERN_WARNING "3w-xxxx: scsi%d: Error copying param sglist from userspace.\n", tw_dev->host->host_no);
								goto tw_ioctl_bail;
							}
						} else {
							printk(KERN_WARNING "3w-xxxx: scsi%d: tw_ioctl(): Bad param sgl address.\n", tw_dev->host->host_no);
							tw_dev->srb[request_id]->result = (DID_RESET << 16);
							goto tw_ioctl_bail;
						}
						data_ptr+=command_packet->byte8.param.sgl[i].length;
					}
					command_packet->size = 4;
					command_packet->byte8.param.sgl[0].address = dma_handle;
					command_packet->byte8.param.sgl[0].length = total_bytes;
				}
				if (command_packet->byte0.sgl_offset == 3) {
					use_sg = command_packet->size - 4;
					for (i=0;i<use_sg;i++)
						total_bytes+=command_packet->byte8.io.sgl[i].length;
					tw_dev->ioctl_data[request_id] = pci_alloc_consistent(tw_dev->tw_pci_dev, total_bytes, &dma_handle);

					if (!tw_dev->ioctl_data[request_id]) {
						printk(KERN_WARNING "3w-xxxx: scsi%d: tw_ioctl(): pci_alloc_consistent() failed for request_id %d.\n", tw_dev->host->host_no, request_id);
						return 1;
					}
					if (command_packet->byte0.opcode == TW_OP_WRITE) {
						/* Copy io sglist into the kernel */
						data_ptr = tw_dev->ioctl_data[request_id];
						for (i=0;i<use_sg;i++) {
							if (command_packet->byte8.io.sgl[i].address != 0) {
								error = copy_from_user(data_ptr, (void *)(unsigned long)command_packet->byte8.io.sgl[i].address, command_packet->byte8.io.sgl[i].length);
								if (error) {
									dprintk(KERN_WARNING "3w-xxxx: scsi%d: Error copying io sglist from userspace.\n", tw_dev->host->host_no);
									goto tw_ioctl_bail;
								}
							} else {
								printk(KERN_WARNING "3w-xxxx: scsi%d: tw_ioctl(): Bad io sgl address.\n", tw_dev->host->host_no);
								tw_dev->srb[request_id]->result = (DID_RESET << 16);
								goto tw_ioctl_bail;
							}
							data_ptr+=command_packet->byte8.io.sgl[i].length;
						}
					}
					command_packet->size = 5;
					command_packet->byte8.io.sgl[0].address = dma_handle;
					command_packet->byte8.io.sgl[0].length = total_bytes;
				}

				spin_unlock(&tw_dev->tw_lock);
				spin_unlock_irq(&io_request_lock);

				set_bit(TW_IN_IOCTL, &tw_dev->flags);

				/* Finally post the command packet */
				tw_post_command_packet(tw_dev, request_id);
				posted = 1;
				do_gettimeofday(&before);

			tw_ioctl_retry:
				mdelay(TW_IOCTL_WAIT_TIME);
				if (test_bit(TW_IN_IOCTL, &tw_dev->flags)) {
					do_gettimeofday(&timeout);
					if (before.tv_sec + TW_IOCTL_TIMEOUT < timeout.tv_sec) {
						spin_lock_irq(&io_request_lock);
						spin_lock(&tw_dev->tw_lock);
						goto tw_ioctl_bail;
					} else {
						goto tw_ioctl_retry;
					}
				}

				spin_lock_irq(&io_request_lock);
				spin_lock(&tw_dev->tw_lock);

				if (signal_pending(current)) {
					dprintk(KERN_WARNING "3w-xxxx: scsi%d: tw_ioctl(): Signal pending, aborting ioctl().\n", tw_dev->host->host_no);
					tw_dev->srb[request_id]->result = (DID_OK << 16);
					goto tw_ioctl_bail;
				}

				tw_dev->srb[request_id]->result = (DID_OK << 16);
				/* Now copy up the param or io sglist to userspace */
				if (command_packet->byte0.sgl_offset == 2) {
					use_sg = command_save->size - 3;
					data_ptr = tw_dev->ioctl_data[request_id];
					for (i=0;i<use_sg;i++) {
						if (command_save->byte8.param.sgl[i].address != 0) {
							error = copy_to_user((void *)(unsigned long)command_save->byte8.param.sgl[i].address, data_ptr, command_save->byte8.param.sgl[i].length);
							if (error) {
								dprintk(KERN_WARNING "3w-xxxx: scsi%d: Error copying param sglist to userspace.\n", tw_dev->host->host_no);
								goto tw_ioctl_bail;
							}
							dprintk(KERN_WARNING "3w-xxxx: scsi%d: Copied %ld bytes to pid %d.\n", tw_dev->host->host_no, command_save->byte8.param.sgl[i].length, current->pid);
							data_ptr+=command_save->byte8.param.sgl[i].length;
						} else {
							printk(KERN_WARNING "3w-xxxx: scsi%d: tw_ioctl(): Bad param sgl address.\n", tw_dev->host->host_no);
							tw_dev->srb[request_id]->result = (DID_RESET << 16);
							goto tw_ioctl_bail;
						}
					}
				}
				if (command_packet->byte0.sgl_offset == 3) {
					use_sg = command_save->size - 4;
					if (command_packet->byte0.opcode == TW_OP_READ) {
						data_ptr = tw_dev->ioctl_data[request_id];
						for(i=0;i<use_sg;i++) {
							if (command_save->byte8.io.sgl[i].address != 0) {
								error = copy_to_user((void *)(unsigned long)command_save->byte8.io.sgl[i].address, data_ptr, command_save->byte8.io.sgl[i].length);
								if (error) {
									dprintk(KERN_WARNING "3w-xxxx: scsi%d: Error copying io sglist to userspace.\n", tw_dev->host->host_no);
									goto tw_ioctl_bail;
								}
								dprintk(KERN_WARNING "3w-xxxx: scsi%d: Copied %ld bytes to pid %d.\n", tw_dev->host->host_no, command_save->byte8.io.sgl[i].length, current->pid);
								data_ptr+=command_save->byte8.io.sgl[i].length;
							} else {
								printk(KERN_WARNING "3w-xxxx: scsi%d: tw_ioctl(): Bad io sgl address.\n", tw_dev->host->host_no);
								tw_dev->srb[request_id]->result = (DID_RESET << 16);
								goto tw_ioctl_bail;
							}
						}
					}
				}
				
			tw_ioctl_bail:

				/* Free up sglist memory */
				if (tw_dev->ioctl_data[request_id])
					pci_free_consistent(tw_dev->tw_pci_dev, total_bytes, tw_dev->ioctl_data[request_id], dma_handle);
				else
					printk(KERN_WARNING "3w-xxxx: scsi%d: tw_ioctl(): Error freeing ioctl data.\n", tw_dev->host->host_no);
				
				/* Now complete the io */
				tw_dev->state[request_id] = TW_S_COMPLETED;
				tw_state_request_finish(tw_dev, request_id);
				if (posted)
					tw_dev->posted_request_count--;
				tw_dev->srb[request_id]->scsi_done(tw_dev->srb[request_id]);
				return 0;
			} else {
				printk(KERN_WARNING "3w-xxxx: tw_ioctl(): ioctl->data NULL.\n");
				return 1;
			}
		default:
			dprintk(KERN_WARNING "3w-xxxx: Unknown ioctl 0x%x.\n", opcode);
			tw_dev->state[request_id] = TW_S_COMPLETED;
			tw_state_request_finish(tw_dev, request_id);
			tw_dev->srb[request_id]->result = (DID_OK << 16);
			tw_dev->srb[request_id]->scsi_done(tw_dev->srb[request_id]);
			return 0;
	}

	param_value = tw_dev->alignment_physical_address[request_id];
	if (param_value == 0) {
		printk(KERN_WARNING "3w-xxxx: tw_ioctl(): Bad alignment physical address.\n");
		tw_dev->state[request_id] = TW_S_COMPLETED;
		tw_state_request_finish(tw_dev, request_id);
		tw_dev->srb[request_id]->result = (DID_OK << 16);
		tw_dev->srb[request_id]->scsi_done(tw_dev->srb[request_id]);
	}

	command_packet->byte8.param.sgl[0].address = param_value;
	command_packet->byte8.param.sgl[0].length = sizeof(TW_Sector);

	command_packet->byte0.sgl_offset = 2;
	command_packet->size = 4;
	command_packet->request_id = request_id;
	command_packet->byte3.host_id = 0;
	command_packet->status = 0;
	command_packet->flags = 0;
	command_packet->byte6.parameter_count = 1;

	/* Now try to post the command to the board */
	tw_post_command_packet(tw_dev, request_id);

	return 0;
} /* End tw_ioctl() */

/* This function is called by the isr to complete ioctl requests */
int tw_ioctl_complete(TW_Device_Extension *tw_dev, int request_id)
{
	unsigned char *param_data;
	unsigned char *buff;
	TW_Param *param;
	TW_Ioctl *ioctl = NULL;
	TW_Passthru *passthru = NULL;
	TW_Command *command_packet;

	ioctl = (TW_Ioctl *)tw_dev->srb[request_id]->request_buffer;
	dprintk(KERN_NOTICE "3w-xxxx: tw_ioctl_complete()\n");
	buff = tw_dev->srb[request_id]->request_buffer;
	if (buff == NULL) {
		printk(KERN_WARNING "3w-xxxx: tw_ioctl_complete(): Request buffer NULL.\n");
		return 1;
	}

	command_packet = (TW_Command *)tw_dev->command_packet_virtual_address[request_id];
	if (command_packet == NULL) {
		printk(KERN_WARNING "3w-xxxx: scsi%d: tw_ioctl_complete(): Bad command packet virtual address.\n", tw_dev->host->host_no);
		return 1;
	}

	dprintk(KERN_NOTICE "3w-xxxx: tw_ioctl_complete(): Request_bufflen = %d\n", tw_dev->srb[request_id]->request_bufflen);

	ioctl = (TW_Ioctl *)buff;
	switch (ioctl->opcode) {
		case TW_ATA_PASSTHRU:
			passthru = (TW_Passthru *)ioctl->data;
			/* Don't return data for non-data ATA cmds */
			if ((passthru->param != 0) && (passthru->param != 0x8))
				memcpy(buff, tw_dev->alignment_virtual_address[request_id], passthru->sector_count * 512);
			else {
				/* For non-data cmds, return cmd pkt */
				if (tw_dev->srb[request_id]->request_bufflen >= sizeof(TW_Command))
					memcpy(buff, tw_dev->command_packet_virtual_address[request_id], sizeof(TW_Command));
			}
			break;
		case TW_CMD_PACKET_WITH_DATA:
			dprintk(KERN_WARNING "3w-xxxx: tw_ioctl_complete(): caught TW_CMD_PACKET_WITH_DATA.\n");
			clear_bit(TW_IN_IOCTL, &tw_dev->flags);
			return TW_ISR_DONT_COMPLETE; /* Special case for isr to not complete io */
		default:
			memset(buff, 0, tw_dev->srb[request_id]->request_bufflen);
			param = (TW_Param *)tw_dev->alignment_virtual_address[request_id];
			if (param == NULL) {
				printk(KERN_WARNING "3w-xxxx: tw_ioctl_complete(): Bad alignment virtual address.\n");
				return 1;
			}
			param_data = &(param->data[0]);
			memcpy(buff, param_data, tw_dev->ioctl_size[request_id]);
	}
	return 0;
} /* End tw_ioctl_complete() */

static int tw_map_scsi_sg_data(struct pci_dev *pdev, Scsi_Cmnd *cmd)
{
	int use_sg;
	int dma_dir = scsi_to_pci_dma_dir(cmd->sc_data_direction);

	dprintk(KERN_WARNING "3w-xxxx: tw_map_scsi_sg_data()\n");
	
	if (cmd->use_sg == 0)
		return 0;

	use_sg = pci_map_sg(pdev, cmd->buffer, cmd->use_sg, dma_dir);

	if (use_sg == 0) {
		printk(KERN_WARNING "3w-xxxx: tw_map_scsi_sg_data(): pci_map_sg() failed.\n");
		return 0;
	}

	cmd->SCp.phase = 2;
	cmd->SCp.have_data_in = use_sg;

	return use_sg;
} /* End tw_map_scsi_sg_data() */

static u32 tw_map_scsi_single_data(struct pci_dev *pdev, Scsi_Cmnd *cmd)
{
	dma_addr_t mapping;
	int dma_dir = scsi_to_pci_dma_dir(cmd->sc_data_direction);

	dprintk(KERN_WARNING "3w-xxxx: tw_map_scsi_single_data()\n");
	
	if (cmd->request_bufflen == 0)
		return 0;
#ifdef BLK_BOUNCE_HIGH
	mapping = pci_map_page(pdev, virt_to_page(cmd->request_buffer), ((unsigned long)cmd->request_buffer & ~PAGE_MASK), cmd->request_bufflen, dma_dir);
#else
	mapping = pci_map_single(pdev, cmd->request_buffer, cmd->request_bufflen, dma_dir);
#endif

	if (mapping == 0) {
		printk(KERN_WARNING "3w-xxxx: tw_map_scsi_single_data(): pci_map_page() failed.\n");
		return 0;
	}

	cmd->SCp.phase = 1;
	cmd->SCp.have_data_in = mapping;

	return mapping;
} /* End tw_map_scsi_single_data() */

/* This function will mask the command interrupt */
void tw_mask_command_interrupt(TW_Device_Extension *tw_dev)
{
	u32 control_reg_addr, control_reg_value;
	
	control_reg_addr = tw_dev->registers.control_reg_addr;
	control_reg_value = TW_CONTROL_MASK_COMMAND_INTERRUPT;
	outl(control_reg_value, control_reg_addr);
} /* End tw_mask_command_interrupt() */

/* This function will poll the status register for a flag */
int tw_poll_status(TW_Device_Extension *tw_dev, u32 flag, int seconds)
{
	u32 status_reg_addr, status_reg_value;
	struct timeval before, timeout;

	status_reg_addr = tw_dev->registers.status_reg_addr;
	do_gettimeofday(&before);
	status_reg_value = inl(status_reg_addr);

	if (tw_check_bits(status_reg_value)) {
		dprintk(KERN_WARNING "3w-xxxx: tw_poll_status(): Unexpected bits.\n");
		tw_decode_bits(tw_dev, status_reg_value, 0);
	}

	while ((status_reg_value & flag) != flag) {
		status_reg_value = inl(status_reg_addr);

		if (tw_check_bits(status_reg_value)) {
			dprintk(KERN_WARNING "3w-xxxx: tw_poll_status(): Unexpected bits.\n");
			tw_decode_bits(tw_dev, status_reg_value, 0);
		}

		do_gettimeofday(&timeout);
		if (before.tv_sec + seconds < timeout.tv_sec) { 
			dprintk(KERN_WARNING "3w-xxxx: tw_poll_status(): Flag 0x%x not found.\n", flag);
			return 1;
		}
		mdelay(5);
	}
	return 0;
} /* End tw_poll_status() */

/* This function will poll the status register for disappearance of a flag */
int tw_poll_status_gone(TW_Device_Extension *tw_dev, u32 flag, int seconds)
{
        u32 status_reg_addr, status_reg_value;
        struct timeval before, timeout;

        status_reg_addr = tw_dev->registers.status_reg_addr;
        do_gettimeofday(&before);
        status_reg_value = inl(status_reg_addr);

	if (tw_check_bits(status_reg_value)) {
		dprintk(KERN_WARNING "3w-xxxx: tw_poll_status_gone(): Unexpected bits.\n");
		tw_decode_bits(tw_dev, status_reg_value, 0);
	}

        while ((status_reg_value & flag) != 0) {
                status_reg_value = inl(status_reg_addr);

		if (tw_check_bits(status_reg_value)) {
			dprintk(KERN_WARNING "3w-xxxx: tw_poll_status_gone(): Unexpected bits.\n");
			tw_decode_bits(tw_dev, status_reg_value, 0);
		}

                do_gettimeofday(&timeout);
                if (before.tv_sec + seconds < timeout.tv_sec) {
			dprintk(KERN_WARNING "3w-xxxx: tw_poll_status_gone(): Flag 0x%x never disappeared.\n", flag);
                        return 1;
                }
                mdelay(5);
        }
        return 0;
} /* End tw_poll_status_gone() */

/* This function will attempt to post a command packet to the board */
int tw_post_command_packet(TW_Device_Extension *tw_dev, int request_id)
{
	u32 status_reg_addr, status_reg_value;
	unsigned long command_que_value;
	u32 command_que_addr;

	dprintk(KERN_NOTICE "3w-xxxx: tw_post_command_packet()\n");
	command_que_addr = tw_dev->registers.command_que_addr;
	command_que_value = tw_dev->command_packet_physical_address[request_id];
	status_reg_addr = tw_dev->registers.status_reg_addr;
	status_reg_value = inl(status_reg_addr);

	if (tw_check_bits(status_reg_value)) {
		dprintk(KERN_WARNING "3w-xxxx: tw_post_command_packet(): Unexpected bits.\n");
		tw_decode_bits(tw_dev, status_reg_value, 1);
	}

	if ((status_reg_value & TW_STATUS_COMMAND_QUEUE_FULL) == 0) {
		/* We successfully posted the command packet */
		outl(command_que_value, command_que_addr);
		tw_dev->state[request_id] = TW_S_POSTED;
		tw_dev->posted_request_count++;
		if (tw_dev->posted_request_count > tw_dev->max_posted_request_count) {
			tw_dev->max_posted_request_count = tw_dev->posted_request_count;
		}
	} else {
		/* Couldn't post the command packet, so we do it in the isr */
		if (tw_dev->state[request_id] != TW_S_PENDING) {
			tw_dev->state[request_id] = TW_S_PENDING;
			tw_dev->pending_request_count++;
			if (tw_dev->pending_request_count > tw_dev->max_pending_request_count) {
				tw_dev->max_pending_request_count = tw_dev->pending_request_count;
			}
			tw_dev->pending_queue[tw_dev->pending_tail] = request_id;
			if (tw_dev->pending_tail == TW_Q_LENGTH-1) {
				tw_dev->pending_tail = TW_Q_START;
			} else {
				tw_dev->pending_tail = tw_dev->pending_tail + 1;
			}
		} 
		tw_unmask_command_interrupt(tw_dev);
		return 1;
	}
	return 0;
} /* End tw_post_command_packet() */

/* This function will reset a device extension */
int tw_reset_device_extension(TW_Device_Extension *tw_dev) 
{
	int imax = 0;
	int i = 0;
	Scsi_Cmnd *srb;

	dprintk(KERN_NOTICE "3w-xxxx: tw_reset_device_extension()\n");
	imax = TW_Q_LENGTH;

	if (tw_reset_sequence(tw_dev)) {
		printk(KERN_WARNING "3w-xxxx: scsi%d: Reset sequence failed.\n", tw_dev->host->host_no);
		return 1;
	}

	/* Abort all requests that are in progress */
	for (i=0;i<imax;i++) {
		if ((tw_dev->state[i] != TW_S_FINISHED) && 
		    (tw_dev->state[i] != TW_S_INITIAL) &&
		    (tw_dev->state[i] != TW_S_COMPLETED)) {
			srb = tw_dev->srb[i];
			if (srb != NULL) {
				srb->result = (DID_RESET << 16);
				tw_dev->srb[i]->scsi_done(tw_dev->srb[i]);
				tw_unmap_scsi_data(tw_dev->tw_pci_dev, tw_dev->srb[i]);
			}
		}
	}

	/* Reset queues and counts */
	for (i=0;i<imax;i++) {
		tw_dev->free_queue[i] = i;
		tw_dev->state[i] = TW_S_INITIAL;
	}
	tw_dev->free_head = TW_Q_START;
	tw_dev->free_tail = TW_Q_START;
	tw_dev->posted_request_count = 0;
	tw_dev->pending_request_count = 0;
	tw_dev->pending_head = TW_Q_START;
	tw_dev->pending_tail = TW_Q_START;
	tw_dev->reset_print = 0;
	tw_dev->chrdev_request_id = TW_IOCTL_CHRDEV_FREE;

	return 0;
} /* End tw_reset_device_extension() */

/* This function will reset a controller */
int tw_reset_sequence(TW_Device_Extension *tw_dev) 
{
	int error = 0;
	int tries = 0;

	/* Disable interrupts */
	tw_disable_interrupts(tw_dev);

	/* Reset the board */
	while (tries < TW_MAX_RESET_TRIES) {
		tw_soft_reset(tw_dev);

		error = tw_aen_drain_queue(tw_dev);
		if (error) {
			printk(KERN_WARNING "3w-xxxx: scsi%d: AEN drain failed, retrying.\n", tw_dev->host->host_no);
			tries++;
			continue;
		}

		/* Check for controller errors */
		if (tw_check_errors(tw_dev)) {
			printk(KERN_WARNING "3w-xxxx: scsi%d: Controller errors found, retrying.\n", tw_dev->host->host_no);
			tries++;
			continue;
		}

		/* Now the controller is in a good state */
		break;
	}

	if (tries >= TW_MAX_RESET_TRIES) {
		printk(KERN_WARNING "3w-xxxx: scsi%d: Controller errors, card not responding, check all cabling.\n", tw_dev->host->host_no);
		return 1;
	}

	error = tw_initconnection(tw_dev, TW_INIT_MESSAGE_CREDITS);
	if (error) {
		printk(KERN_WARNING "3w-xxxx: scsi%d: Connection initialization failed.\n", tw_dev->host->host_no);
		return 1;
	}

	/* Re-enable interrupts */
	tw_enable_and_clear_interrupts(tw_dev);

	return 0;
} /* End tw_reset_sequence() */

/* This funciton returns unit geometry in cylinders/heads/sectors */
int tw_scsi_biosparam(Disk *disk, kdev_t dev, int geom[]) 
{
	int heads, sectors, cylinders;
	TW_Device_Extension *tw_dev;
	
	dprintk(KERN_NOTICE "3w-xxxx: tw_scsi_biosparam()\n");
	tw_dev = (TW_Device_Extension *)disk->device->host->hostdata;

	heads = 64;
	sectors = 32;
	cylinders = disk->capacity / (heads * sectors);

	if (disk->capacity >= 0x200000) {
		heads = 255;
		sectors = 63;
		cylinders = disk->capacity / (heads * sectors);
	}

	dprintk(KERN_NOTICE "3w-xxxx: tw_scsi_biosparam(): heads = %d, sectors = %d, cylinders = %d\n", heads, sectors, cylinders);
	geom[0] = heads;			 
	geom[1] = sectors;
	geom[2] = cylinders;

	return 0;
} /* End tw_scsi_biosparam() */

/* This function will find and initialize any cards */
int tw_scsi_detect(Scsi_Host_Template *tw_host)
{
	int ret;
	
	dprintk(KERN_NOTICE "3w-xxxx: tw_scsi_detect()\n");

	printk(KERN_WARNING "3ware Storage Controller device driver for Linux v%s.\n", tw_driver_version);

	/* Check if the kernel has PCI interface compiled in */
	if (!pci_present()) {
		printk(KERN_WARNING "3w-xxxx: tw_scsi_detect(): No pci interface present.\n");
		return 0;
	}

	spin_unlock_irq(&io_request_lock);
	ret = tw_findcards(tw_host);
	spin_lock_irq(&io_request_lock);

	return ret;
} /* End tw_scsi_detect() */

/* This is the new scsi eh abort function */
int tw_scsi_eh_abort(Scsi_Cmnd *SCpnt) 
{
	TW_Device_Extension *tw_dev=NULL;
	int i = 0;

	dprintk(KERN_NOTICE "3w-xxxx: tw_scsi_eh_abort()\n");

	if (!SCpnt) {
		printk(KERN_WARNING "3w-xxxx: tw_scsi_eh_abort(): Invalid Scsi_Cmnd.\n");
		return (FAILED);
	}

	tw_dev = (TW_Device_Extension *)SCpnt->host->hostdata;
	if (tw_dev == NULL) {
		printk(KERN_WARNING "3w-xxxx: tw_scsi_eh_abort(): Invalid device extension.\n");
		return (FAILED);
	}

	spin_lock(&tw_dev->tw_lock);
	tw_dev->num_aborts++;

	/* If the command hasn't been posted yet, we can do the abort */
	for (i=0;i<TW_Q_LENGTH;i++) {
		if (tw_dev->srb[i] == SCpnt) {
			if (tw_dev->state[i] == TW_S_STARTED) {
				printk(KERN_WARNING "3w-xxxx: scsi%d: Unit #%d: Command (%p) timed out.\n", tw_dev->host->host_no, tw_dev->srb[i]==0 ? 0 : tw_dev->srb[i]->target, SCpnt);
				tw_dev->state[i] = TW_S_COMPLETED;
				tw_state_request_finish(tw_dev, i);
				spin_unlock(&tw_dev->tw_lock);
				return (SUCCESS);
			}
			if (tw_dev->state[i] == TW_S_PENDING) {
				printk(KERN_WARNING "3w-xxxx: scsi%d: Unit #%d: Command (%p) timed out.\n", tw_dev->host->host_no, tw_dev->srb[i]==0 ? 0 : tw_dev->srb[i]->target, SCpnt);
				if (tw_dev->pending_head == TW_Q_LENGTH-1) {
					tw_dev->pending_head = TW_Q_START;
				} else {
					tw_dev->pending_head = tw_dev->pending_head + 1;
				}
				tw_dev->pending_request_count--;
				tw_dev->state[i] = TW_S_COMPLETED;
				tw_state_request_finish(tw_dev, i);
				spin_unlock(&tw_dev->tw_lock);
				return (SUCCESS);
			}
			if (tw_dev->state[i] == TW_S_POSTED) {
				/* If the command has already been posted, we have to reset the card */
				printk(KERN_WARNING "3w-xxxx: scsi%d: Unit #%d: Command (%p) timed out, resetting card.\n", tw_dev->host->host_no, tw_dev->srb[i]==0 ? 0 : tw_dev->srb[i]->target, SCpnt);
				/* We have to let AEN requests through before the reset */
				spin_unlock(&tw_dev->tw_lock);
				spin_unlock_irq(&io_request_lock);
				mdelay(TW_AEN_WAIT_TIME);
				spin_lock_irq(&io_request_lock);
				spin_lock(&tw_dev->tw_lock);

				if (tw_reset_device_extension(tw_dev)) {
					dprintk(KERN_WARNING "3w-xxxx: tw_scsi_eh_abort(): Reset failed for card %d.\n", tw_dev->host->host_no);
					spin_unlock(&tw_dev->tw_lock);
					return (FAILED);
				}
			}
		}
	}

	spin_unlock(&tw_dev->tw_lock);
	return (SUCCESS);
} /* End tw_scsi_eh_abort() */

/* This is the new scsi eh reset function */
int tw_scsi_eh_reset(Scsi_Cmnd *SCpnt) 
{
	TW_Device_Extension *tw_dev=NULL;

	dprintk(KERN_NOTICE "3w-xxxx: tw_scsi_eh_reset()\n");

	if (!SCpnt) {
		printk(KERN_WARNING "3w-xxxx: tw_scsi_eh_reset(): Invalid Scsi_Cmnd.\n");
		return (FAILED);
	}

	tw_dev = (TW_Device_Extension *)SCpnt->host->hostdata;
	if (tw_dev == NULL) {
		printk(KERN_WARNING "3w-xxxx: tw_scsi_eh_reset(): Invalid device extension.\n");
		return (FAILED);
	}

	/* We have to let AEN requests through before the reset */
	spin_unlock_irq(&io_request_lock);
	mdelay(TW_AEN_WAIT_TIME);
	spin_lock_irq(&io_request_lock);

	spin_lock(&tw_dev->tw_lock);
	tw_dev->num_resets++;

	/* Now reset the card and some of the device extension data */
	if (tw_reset_device_extension(tw_dev)) {
		printk(KERN_WARNING "3w-xxxx: scsi%d: Reset failed.\n", tw_dev->host->host_no);
		spin_unlock(&tw_dev->tw_lock);
		return (FAILED);
	}
	printk(KERN_WARNING "3w-xxxx: scsi%d: Reset succeeded.\n", tw_dev->host->host_no);
	spin_unlock(&tw_dev->tw_lock);

	return (SUCCESS);
} /* End tw_scsi_eh_reset() */

/* This function handles input and output from /proc/scsi/3w-xxxx/x */
int tw_scsi_proc_info(char *buffer, char **start, off_t offset, int length, int hostno, int inout) 
{
	TW_Device_Extension *tw_dev = NULL;
	TW_Info info;
	int i;
	int j;

	dprintk(KERN_NOTICE "3w-xxxx: tw_scsi_proc_info()\n");

	/* Find the correct device extension */
	for (i=0;i<tw_device_extension_count;i++) 
		if (tw_device_extension_list[i]->host->host_no == hostno) 
			tw_dev = tw_device_extension_list[i];
	if (tw_dev == NULL) {
		printk(KERN_WARNING "3w-xxxx: tw_scsi_proc_info(): Couldn't locate device extension.\n");
		return (-EINVAL);
	}

	info.buffer = buffer;
	info.length = length;
	info.offset = offset;
	info.position = 0;
	
	if (inout) {
		/* Write */
		if (strncmp(buffer, "debug", 5) == 0) {
			printk(KERN_INFO "3w-xxxx: Posted commands:\n");
			for (j=0;j<TW_Q_LENGTH;j++) {
				if (tw_dev->state[j] == TW_S_POSTED) {
					TW_Command *command = (TW_Command *)tw_dev->command_packet_virtual_address[j];
					printk(KERN_INFO "3w-xxxx: Request_id: %d\n", j);
					printk(KERN_INFO "Opcode: 0x%x\n", command->byte0.opcode);
					printk(KERN_INFO "Block_count: 0x%x\n", command->byte6.block_count);
					printk(KERN_INFO "LBA: 0x%x\n", command->byte8.io.lba);
					printk(KERN_INFO "Physical command packet addr: 0x%lx\n", tw_dev->command_packet_physical_address[j]);
					printk(KERN_INFO "Scsi_Cmnd: %p\n", tw_dev->srb[j]);
				}
			}
			printk(KERN_INFO "3w-xxxx: Free_head: %3d\n", tw_dev->free_head);
			printk(KERN_INFO "3w-xxxx: Free_tail: %3d\n", tw_dev->free_tail);
		} 
		return length;
	} else {
		/* Read */
		if (start) {
			*start = buffer;
		}
		tw_copy_info(&info, "scsi%d: 3ware Storage Controller\n", hostno);
		tw_copy_info(&info, "Driver version: %s\n", tw_driver_version);
		tw_copy_info(&info, "Current commands posted:       %3d\n", tw_dev->posted_request_count);
		tw_copy_info(&info, "Max commands posted:           %3d\n", tw_dev->max_posted_request_count);
		tw_copy_info(&info, "Current pending commands:      %3d\n", tw_dev->pending_request_count);
		tw_copy_info(&info, "Max pending commands:          %3d\n", tw_dev->max_pending_request_count);
		tw_copy_info(&info, "Last sgl length:               %3d\n", tw_dev->sgl_entries);
		tw_copy_info(&info, "Max sgl length:                %3d\n", tw_dev->max_sgl_entries);
		tw_copy_info(&info, "Last sector count:             %3d\n", tw_dev->sector_count);
		tw_copy_info(&info, "Max sector count:              %3d\n", tw_dev->max_sector_count);
		tw_copy_info(&info, "Resets:                        %3d\n", tw_dev->num_resets);
		tw_copy_info(&info, "Aborts:                        %3d\n", tw_dev->num_aborts);
		tw_copy_info(&info, "AEN's:                         %3d\n", tw_dev->aen_count);
	}
	if (info.position > info.offset) {
		return (info.position - info.offset);
	} else { 
		return 0;
	}
} /* End tw_scsi_proc_info() */

/* This is the main scsi queue function to handle scsi opcodes */
int tw_scsi_queue(Scsi_Cmnd *SCpnt, void (*done)(Scsi_Cmnd *)) 
{
	unsigned char *command = SCpnt->cmnd;
	int request_id = 0;
	int error = 0;
	TW_Device_Extension *tw_dev = (TW_Device_Extension *)SCpnt->host->hostdata;

	if (tw_dev == NULL) {
		printk(KERN_WARNING "3w-xxxx: tw_scsi_queue(): Invalid device extension.\n");
		SCpnt->result = (DID_ERROR << 16);
		done(SCpnt);
		return 0;
	}

	spin_lock(&tw_dev->tw_lock);
	dprintk(KERN_NOTICE "3w-xxxx: tw_scsi_queue()\n");

	/* Skip scsi command if it isn't for us */
	if ((SCpnt->channel != 0) || (SCpnt->lun != 0)) {
		SCpnt->result = (DID_BAD_TARGET << 16);
		done(SCpnt);
		spin_unlock(&tw_dev->tw_lock);
		return 0;
	}
	
	/* Save done function into Scsi_Cmnd struct */
	SCpnt->scsi_done = done;
		 
	/* Queue the command and get a request id */
	tw_state_request_start(tw_dev, &request_id);

	/* Save the scsi command for use by the ISR */
	tw_dev->srb[request_id] = SCpnt;

	/* Initialize phase to zero */
	SCpnt->SCp.phase = 0;

	switch (*command) {
		case READ_10:
		case READ_6:
		case WRITE_10:
		case WRITE_6:
			dprintk(KERN_NOTICE "3w-xxxx: tw_scsi_queue(): caught READ/WRITE.\n");
			error = tw_scsiop_read_write(tw_dev, request_id);
			break;
		case TEST_UNIT_READY:
			dprintk(KERN_NOTICE "3w-xxxx: tw_scsi_queue(): caught TEST_UNIT_READY.\n");
			error = tw_scsiop_test_unit_ready(tw_dev, request_id);
			break;
		case INQUIRY:
			dprintk(KERN_NOTICE "3w-xxxx: tw_scsi_queue(): caught INQUIRY.\n");
			error = tw_scsiop_inquiry(tw_dev, request_id);
			break;
		case READ_CAPACITY:
			dprintk(KERN_NOTICE "3w-xxxx: tw_scsi_queue(): caught READ_CAPACITY.\n");
			error = tw_scsiop_read_capacity(tw_dev, request_id);
			break;
	        case REQUEST_SENSE:
		        dprintk(KERN_NOTICE "3w-xxxx: tw_scsi_queue(): caught REQUEST_SENSE.\n");
		        error = tw_scsiop_request_sense(tw_dev, request_id);
		        break;
		case MODE_SENSE:
			dprintk(KERN_NOTICE "3w-xxxx: tw_scsi_queue(): caught MODE_SENSE.\n");
			error = tw_scsiop_mode_sense(tw_dev, request_id);
			break;
		case SYNCHRONIZE_CACHE:
			dprintk(KERN_NOTICE "3w-xxxx: tw_scsi_queue(): caught SYNCHRONIZE_CACHE.\n");
			error = tw_scsiop_synchronize_cache(tw_dev, request_id);
			break;
		case TW_IOCTL:
			dprintk(KERN_NOTICE "3w-xxxx: tw_scsi_queue(): caught TW_SCSI_IOCTL.\n");
			error = tw_ioctl(tw_dev, request_id);
			break;
		default:
			printk(KERN_NOTICE "3w-xxxx: scsi%d: Unknown scsi opcode: 0x%x\n", tw_dev->host->host_no, *command);
			tw_dev->state[request_id] = TW_S_COMPLETED;
			tw_state_request_finish(tw_dev, request_id);
			SCpnt->result = (DID_BAD_TARGET << 16);
			done(SCpnt);
	}
	if (error) {
		tw_dev->state[request_id] = TW_S_COMPLETED;
		tw_state_request_finish(tw_dev, request_id);
		SCpnt->result = (DID_ERROR << 16);
		done(SCpnt);
	}
	spin_unlock(&tw_dev->tw_lock);

	return 0;
} /* End tw_scsi_queue() */

/* This function will release the resources on an rmmod call */
int tw_scsi_release(struct Scsi_Host *tw_host) 
{
	TW_Device_Extension *tw_dev;
	tw_dev = (TW_Device_Extension *)tw_host->hostdata;

	dprintk(KERN_NOTICE "3w-xxxx: tw_scsi_release()\n");

	/* Fake like we just shut down, so notify the card that
	 * we "shut down cleanly".
	 */
	tw_halt(0, 0, 0);  // parameters aren't actually used

	/* Free up the IO region */
	release_region((tw_dev->tw_pci_dev->resource[0].start), TW_IO_ADDRESS_RANGE);

	/* Free up the IRQ */
	free_irq(tw_dev->tw_pci_dev->irq, tw_dev);

	/* Unregister character device */
	if (twe_major >= 0) {
		unregister_chrdev(twe_major, "twe");
		twe_major = -1;
	}

	/* Free up device extension resources */
	tw_free_device_extension(tw_dev);

	/* Tell kernel scsi-layer we are gone */
	scsi_unregister(tw_host);

	return 0;
} /* End tw_scsi_release() */

/* This function handles scsi inquiry commands */
int tw_scsiop_inquiry(TW_Device_Extension *tw_dev, int request_id)
{
	TW_Param *param;
	TW_Command *command_packet;
	unsigned long command_que_value;
	u32 command_que_addr;
	unsigned long param_value;

	dprintk(KERN_NOTICE "3w-xxxx: tw_scsiop_inquiry()\n");

	/* Initialize command packet */
	command_que_addr = tw_dev->registers.command_que_addr;
	command_packet = (TW_Command *)tw_dev->command_packet_virtual_address[request_id];
	if (command_packet == NULL) {
		printk(KERN_WARNING "3w-xxxx: tw_scsiop_inquiry(): Bad command packet virtual address.\n");
		return 1;
	}
	memset(command_packet, 0, sizeof(TW_Sector));
	command_packet->byte0.opcode = TW_OP_GET_PARAM;
	command_packet->byte0.sgl_offset = 2;
	command_packet->size = 4;
	command_packet->request_id = request_id;
	command_packet->byte3.unit = 0;
	command_packet->byte3.host_id = 0;
	command_packet->status = 0;
	command_packet->flags = 0;
	command_packet->byte6.parameter_count = 1;

	/* Now setup the param */
	if (tw_dev->alignment_virtual_address[request_id] == NULL) {
		printk(KERN_WARNING "3w-xxxx: tw_scsiop_inquiry(): Bad alignment virtual address.\n");
		return 1;
	}
	param = (TW_Param *)tw_dev->alignment_virtual_address[request_id];
	memset(param, 0, sizeof(TW_Sector));
	param->table_id = 3;	 /* unit summary table */
	param->parameter_id = 3; /* unitsstatus parameter */
	param->parameter_size_bytes = TW_MAX_UNITS;
	param_value = tw_dev->alignment_physical_address[request_id];
	if (param_value == 0) {
		printk(KERN_WARNING "3w-xxxx: tw_scsiop_inquiry(): Bad alignment physical address.\n");
		return 1;
	}

	command_packet->byte8.param.sgl[0].address = param_value;
	command_packet->byte8.param.sgl[0].length = sizeof(TW_Sector);
	command_que_value = tw_dev->command_packet_physical_address[request_id];
	if (command_que_value == 0) {
		printk(KERN_WARNING "3w-xxxx: tw_scsiop_inquiry(): Bad command packet physical address.\n");
		return 1;
	}

	/* Now try to post the command packet */
	tw_post_command_packet(tw_dev, request_id);

	return 0;
} /* End tw_scsiop_inquiry() */

/* This function is called by the isr to complete an inquiry command */
int tw_scsiop_inquiry_complete(TW_Device_Extension *tw_dev, int request_id)
{
	unsigned char *is_unit_present;
	unsigned char *request_buffer;
	TW_Param *param;

	dprintk(KERN_NOTICE "3w-xxxx: tw_scsiop_inquiry_complete()\n");

	/* Fill request buffer */
	if (tw_dev->srb[request_id]->request_buffer == NULL) {
		printk(KERN_WARNING "3w-xxxx: tw_scsiop_inquiry_complete(): Request buffer NULL.\n");
		return 1;
	}
	request_buffer = tw_dev->srb[request_id]->request_buffer;
	memset(request_buffer, 0, tw_dev->srb[request_id]->request_bufflen);
	request_buffer[0] = TYPE_DISK; /* Peripheral device type */
	request_buffer[1] = 0;	       /* Device type modifier */
	request_buffer[2] = 0;	       /* No ansi/iso compliance */
	request_buffer[4] = 31;	       /* Additional length */
	memcpy(&request_buffer[8], "3ware   ", 8);	 /* Vendor ID */
	sprintf(&request_buffer[16], "Logical Disk %-2d ", tw_dev->srb[request_id]->target);
	memcpy(&request_buffer[32], tw_driver_version, 3);

	param = (TW_Param *)tw_dev->alignment_virtual_address[request_id];
	if (param == NULL) {
		printk(KERN_WARNING "3w-xxxx: tw_scsiop_inquiry_complete(): Bad alignment virtual address.\n");
		return 1;
	}
	is_unit_present = &(param->data[0]);

	if (is_unit_present[tw_dev->srb[request_id]->target] & TW_UNIT_ONLINE) {
		tw_dev->is_unit_present[tw_dev->srb[request_id]->target] = TRUE;
	} else {
		tw_dev->is_unit_present[tw_dev->srb[request_id]->target] = FALSE;
		tw_dev->srb[request_id]->result = (DID_BAD_TARGET << 16);
		return TW_ISR_DONT_RESULT;
	}

	return 0;
} /* End tw_scsiop_inquiry_complete() */

/* This function handles scsi mode_sense commands */
int tw_scsiop_mode_sense(TW_Device_Extension *tw_dev, int request_id)
{
	TW_Param *param;
	TW_Command *command_packet;
	unsigned long command_que_value;
	unsigned long param_value;

	dprintk(KERN_NOTICE "3w-xxxx: tw_scsiop_mode_sense()\n");

	/* Only page control = 0, page code = 0x8 (cache page) supported */
	if (tw_dev->srb[request_id]->cmnd[2] != 0x8) {
		tw_dev->state[request_id] = TW_S_COMPLETED;
		tw_state_request_finish(tw_dev, request_id);
		tw_dev->srb[request_id]->result = (DID_OK << 16);
		tw_dev->srb[request_id]->scsi_done(tw_dev->srb[request_id]);
		return 0;
	}

	/* Now read firmware cache setting for this unit */
	command_packet = (TW_Command *)tw_dev->command_packet_virtual_address[request_id];
	if (command_packet == NULL) {
		printk(KERN_WARNING "3w-xxxx: tw_scsiop_mode_sense(): Bad command packet virtual address.\n");
		return 1;
	}

	/* Setup the command packet */
	memset(command_packet, 0, sizeof(TW_Sector));
	command_packet->byte0.opcode = TW_OP_GET_PARAM;
	command_packet->byte0.sgl_offset = 2;
	command_packet->size = 4;
	command_packet->request_id = request_id;
	command_packet->byte3.unit = 0;
	command_packet->byte3.host_id = 0;
	command_packet->status = 0;
	command_packet->flags = 0;
	command_packet->byte6.parameter_count = 1;

	/* Setup the param */
	if (tw_dev->alignment_virtual_address[request_id] == NULL) {
		printk(KERN_WARNING "3w-xxxx: tw_scsiop_mode_sense(): Bad alignment virtual address.\n");
		return 1;
	}

	param = (TW_Param *)tw_dev->alignment_virtual_address[request_id];
	memset(param, 0, sizeof(TW_Sector));
	param->table_id = TW_UNIT_INFORMATION_TABLE_BASE + tw_dev->srb[request_id]->target;
	param->parameter_id = 7; /* unit flags */
	param->parameter_size_bytes = 1;
	param_value = tw_dev->alignment_physical_address[request_id];
	if (param_value == 0) {
		printk(KERN_WARNING "3w-xxxx: tw_scsiop_mode_sense(): Bad alignment physical address.\n");
		return 1;
	}

	command_packet->byte8.param.sgl[0].address = param_value;
	command_packet->byte8.param.sgl[0].length = sizeof(TW_Sector);
	command_que_value = tw_dev->command_packet_physical_address[request_id];
	if (command_que_value == 0) {
		printk(KERN_WARNING "3w-xxxx: tw_scsiop_mode_sense(): Bad command packet physical address.\n");
		return 1;
	}

	/* Now try to post the command packet */
	tw_post_command_packet(tw_dev, request_id);

	return 0;
} /* End tw_scsiop_mode_sense() */

/* This function is called by the isr to complete a mode sense command */
int tw_scsiop_mode_sense_complete(TW_Device_Extension *tw_dev, int request_id)
{
	TW_Param *param;
	unsigned char *flags;
	unsigned char *request_buffer;

	dprintk(KERN_NOTICE "3w-xxxx: tw_scsiop_mode_sense_complete()\n");

	param = (TW_Param *)tw_dev->alignment_virtual_address[request_id];
	if (param == NULL) {
		printk(KERN_WARNING "3w-xxxx: tw_scsiop_mode_sense_complete(): Bad alignment virtual address.\n");
		return 1;
	}
	flags = (char *)&(param->data[0]);
	request_buffer = tw_dev->srb[request_id]->buffer;
	memset(request_buffer, 0, tw_dev->srb[request_id]->request_bufflen);

	request_buffer[0] = 0xf;        /* mode data length */
	request_buffer[1] = 0;          /* default medium type */
	request_buffer[2] = 0x10;       /* dpo/fua support on */
	request_buffer[3] = 0;          /* no block descriptors */
	request_buffer[4] = 0x8;        /* caching page */
	request_buffer[5] = 0xa;        /* page length */
	if (*flags & 0x1)
		request_buffer[6] = 0x4;        /* WCE on */
	else
		request_buffer[6] = 0x0;        /* WCE off */

	return 0;
} /* End tw_scsiop_mode_sense_complete() */

/* This function handles scsi read_capacity commands */
int tw_scsiop_read_capacity(TW_Device_Extension *tw_dev, int request_id) 
{
	TW_Param *param;
	TW_Command *command_packet;
	unsigned long command_que_value;
	u32 command_que_addr;
	unsigned long param_value;

	dprintk(KERN_NOTICE "3w-xxxx: tw_scsiop_read_capacity()\n");

	/* Initialize command packet */
	command_que_addr = tw_dev->registers.command_que_addr;
	command_packet = (TW_Command *)tw_dev->command_packet_virtual_address[request_id];

	if (command_packet == NULL) {
		dprintk(KERN_NOTICE "3w-xxxx: tw_scsiop_read_capacity(): Bad command packet virtual address.\n");
		return 1;
	}
	memset(command_packet, 0, sizeof(TW_Sector));
	command_packet->byte0.opcode = TW_OP_GET_PARAM;
	command_packet->byte0.sgl_offset = 2;
	command_packet->size = 4;
	command_packet->request_id = request_id;
	command_packet->byte3.unit = tw_dev->srb[request_id]->target;
	command_packet->byte3.host_id = 0;
	command_packet->status = 0;
	command_packet->flags = 0;
	command_packet->byte6.block_count = 1;

	/* Now setup the param */
	if (tw_dev->alignment_virtual_address[request_id] == NULL) {
		dprintk(KERN_NOTICE "3w-xxxx: tw_scsiop_read_capacity(): Bad alignment virtual address.\n");
		return 1;
	}
	param = (TW_Param *)tw_dev->alignment_virtual_address[request_id];
	memset(param, 0, sizeof(TW_Sector));
	param->table_id = TW_UNIT_INFORMATION_TABLE_BASE + 
	tw_dev->srb[request_id]->target;
	param->parameter_id = 4;	/* unitcapacity parameter */
	param->parameter_size_bytes = 4;
	param_value = tw_dev->alignment_physical_address[request_id];
	if (param_value == 0) {
		dprintk(KERN_NOTICE "3w-xxxx: tw_scsiop_read_capacity(): Bad alignment physical address.\n");
		return 1;
	}
  
	command_packet->byte8.param.sgl[0].address = param_value;
	command_packet->byte8.param.sgl[0].length = sizeof(TW_Sector);
	command_que_value = tw_dev->command_packet_physical_address[request_id];
	if (command_que_value == 0) {
		dprintk(KERN_NOTICE "3w-xxxx: tw_scsiop_read_capacity(): Bad command packet physical address.\n");
		return 1;
	}

	/* Now try to post the command to the board */
	tw_post_command_packet(tw_dev, request_id);
  
	return 0;
} /* End tw_scsiop_read_capacity() */

/* This function is called by the isr to complete a readcapacity command */
int tw_scsiop_read_capacity_complete(TW_Device_Extension *tw_dev, int request_id)
{
	unsigned char *param_data;
	u32 capacity;
	char *buff;
	TW_Param *param;

	dprintk(KERN_NOTICE "3w-xxxx: tw_scsiop_read_capacity_complete()\n");

	buff = tw_dev->srb[request_id]->request_buffer;
	if (buff == NULL) {
		printk(KERN_WARNING "3w-xxxx: tw_scsiop_read_capacity_complete(): Request buffer NULL.\n");
		return 1;
	}
	memset(buff, 0, tw_dev->srb[request_id]->request_bufflen);
	param = (TW_Param *)tw_dev->alignment_virtual_address[request_id];
	if (param == NULL) {
		printk(KERN_WARNING "3w-xxxx: tw_scsiop_read_capacity_complete(): Bad alignment virtual address.\n");
		return 1;
	}
	param_data = &(param->data[0]);

	capacity = (param_data[3] << 24) | (param_data[2] << 16) | 
		   (param_data[1] << 8) | param_data[0];

	/* Subtract one sector to fix get last sector ioctl */
	capacity -= 1;

	dprintk(KERN_NOTICE "3w-xxxx: tw_scsiop_read_capacity_complete(): Capacity = 0x%x.\n", capacity);

	/* Number of LBA's */
	buff[0] = (capacity >> 24);
	buff[1] = (capacity >> 16) & 0xff;
	buff[2] = (capacity >> 8) & 0xff;
	buff[3] = capacity & 0xff;

	/* Block size in bytes (512) */
	buff[4] = (TW_BLOCK_SIZE >> 24);
	buff[5] = (TW_BLOCK_SIZE >> 16) & 0xff;
	buff[6] = (TW_BLOCK_SIZE >> 8) & 0xff;
	buff[7] = TW_BLOCK_SIZE & 0xff;

	return 0;
} /* End tw_scsiop_read_capacity_complete() */

/* This function handles scsi read or write commands */
int tw_scsiop_read_write(TW_Device_Extension *tw_dev, int request_id) 
{
	TW_Command *command_packet;
	unsigned long command_que_value;
	u32 command_que_addr = 0x0;
	u32 lba = 0x0, num_sectors = 0x0, buffaddr = 0x0;
	int i, use_sg, count = 0;
	Scsi_Cmnd *srb;
	struct scatterlist *sglist;

	dprintk(KERN_NOTICE "3w-xxxx: tw_scsiop_read_write()\n");

	if (tw_dev->srb[request_id]->request_buffer == NULL) {
		printk(KERN_WARNING "3w-xxxx: tw_scsiop_read_write(): Request buffer NULL.\n");
		return 1;
	}
	sglist = (struct scatterlist *)tw_dev->srb[request_id]->request_buffer;
	srb = tw_dev->srb[request_id];

	/* Initialize command packet */
	command_que_addr = tw_dev->registers.command_que_addr;
	command_packet = (TW_Command *)tw_dev->command_packet_virtual_address[request_id];
	if (command_packet == NULL) {
		dprintk(KERN_NOTICE "3w-xxxx: tw_scsiop_read_write(): Bad command packet virtual address.\n");
		return 1;
	}

	if (srb->cmnd[0] == READ_6 || srb->cmnd[0] == READ_10) {
		command_packet->byte0.opcode = TW_OP_READ;
	} else {
		command_packet->byte0.opcode = TW_OP_WRITE;
	}

	command_packet->byte0.sgl_offset = 3;
	command_packet->size = 3;
	command_packet->request_id = request_id;
	command_packet->byte3.unit = srb->target;
	command_packet->byte3.host_id = 0;
	command_packet->status = 0;
	command_packet->flags = 0;

	if (srb->cmnd[0] == WRITE_10) {
		if ((srb->cmnd[1] & 0x8) || (srb->cmnd[1] & 0x10))
			command_packet->flags = 1;
	}

	if (srb->cmnd[0] == READ_6 || srb->cmnd[0] == WRITE_6) {
		lba = ((u32)srb->cmnd[1] << 16) | ((u32)srb->cmnd[2] << 8) | (u32)srb->cmnd[3];
		num_sectors = (u32)srb->cmnd[4];
	} else {
		lba = ((u32)srb->cmnd[2] << 24) | ((u32)srb->cmnd[3] << 16) | ((u32)srb->cmnd[4] << 8) | (u32)srb->cmnd[5];
		num_sectors = (u32)srb->cmnd[8] | ((u32)srb->cmnd[7] << 8);
	}
  
	/* Update sector statistic */
	tw_dev->sector_count = num_sectors;
	if (tw_dev->sector_count > tw_dev->max_sector_count)
		tw_dev->max_sector_count = tw_dev->sector_count;
  
	dprintk(KERN_NOTICE "3w-xxxx: tw_scsiop_read_write(): lba = 0x%x num_sectors = 0x%x\n", lba, num_sectors);
	command_packet->byte8.io.lba = lba;
	command_packet->byte6.block_count = num_sectors;

	if ((tw_dev->is_raid_five[tw_dev->srb[request_id]->target] == 0) || (srb->cmnd[0] == READ_6) || (srb->cmnd[0] == READ_10) || (tw_dev->tw_pci_dev->device == TW_DEVICE_ID2)) {
		/* Do this if there are no sg list entries */
		if (tw_dev->srb[request_id]->use_sg == 0) {    
			dprintk(KERN_NOTICE "3w-xxxx: tw_scsiop_read_write(): SG = 0\n");
			buffaddr = tw_map_scsi_single_data(tw_dev->tw_pci_dev, tw_dev->srb[request_id]);
			if (buffaddr == 0)
				return 1;

			command_packet->byte8.io.sgl[0].address = buffaddr;
			command_packet->byte8.io.sgl[0].length = tw_dev->srb[request_id]->request_bufflen;
			command_packet->size+=2;
		}

		/* Do this if we have multiple sg list entries */
		if (tw_dev->srb[request_id]->use_sg > 0) {
			use_sg = tw_map_scsi_sg_data(tw_dev->tw_pci_dev, tw_dev->srb[request_id]);
			if (use_sg == 0)
				return 1;

			for (i=0;i<use_sg; i++) {
				command_packet->byte8.io.sgl[i].address = sg_dma_address(&sglist[i]);
				command_packet->byte8.io.sgl[i].length = sg_dma_len(&sglist[i]);
				command_packet->size+=2;
			}
		}
	} else {
                /* Do this if there are no sg list entries for raid 5 */
                if (tw_dev->srb[request_id]->use_sg == 0) {
			dprintk(KERN_WARNING "doing raid 5 write use_sg = 0, bounce_buffer[%d] = 0x%p\n", request_id, tw_dev->bounce_buffer[request_id]);
			buffaddr = tw_map_scsi_single_data(tw_dev->tw_pci_dev, tw_dev->srb[request_id]);
			if (buffaddr == 0)
				return 1;

			memcpy(tw_dev->bounce_buffer[request_id], bus_to_virt(buffaddr), tw_dev->srb[request_id]->request_bufflen);
			command_packet->byte8.io.sgl[0].address = tw_dev->bounce_buffer_phys[request_id];
			command_packet->byte8.io.sgl[0].length = tw_dev->srb[request_id]->request_bufflen;
			command_packet->size+=2;
                }

                /* Do this if we have multiple sg list entries for raid 5 */
                if (tw_dev->srb[request_id]->use_sg > 0) {
                        dprintk(KERN_WARNING "doing raid 5 write use_sg = %d, sglist[0].length = %d\n", tw_dev->srb[request_id]->use_sg, sglist[0].length);
			use_sg = tw_map_scsi_sg_data(tw_dev->tw_pci_dev, tw_dev->srb[request_id]);
			if (use_sg == 0)
				return 1;

			for (i=0;i<use_sg; i++) {
				memcpy((char *)tw_dev->bounce_buffer[request_id]+count, bus_to_virt(sg_dma_address(&sglist[i])), sg_dma_len(&sglist[i]));
				count+=sg_dma_len(&sglist[i]);
                        }
                        command_packet->byte8.io.sgl[0].address = tw_dev->bounce_buffer_phys[request_id];
                        command_packet->byte8.io.sgl[0].length = count;
                        command_packet->size = 5; /* single sgl */
                }
        }

	/* Update SG statistics */
	tw_dev->sgl_entries = tw_dev->srb[request_id]->use_sg;
	if (tw_dev->sgl_entries > tw_dev->max_sgl_entries)
		tw_dev->max_sgl_entries = tw_dev->sgl_entries;

	command_que_value = tw_dev->command_packet_physical_address[request_id];
	if (command_que_value == 0) {
		dprintk(KERN_WARNING "3w-xxxx: tw_scsiop_read_write(): Bad command packet physical address.\n");
		return 1;
	}
      
	/* Now try to post the command to the board */
	tw_post_command_packet(tw_dev, request_id);

	return 0;
} /* End tw_scsiop_read_write() */

/* This function will handle the request sense scsi command */
int tw_scsiop_request_sense(TW_Device_Extension *tw_dev, int request_id)
{
	dprintk(KERN_NOTICE "3w-xxxx: tw_scsiop_request_sense()\n");
  
	/* For now we just zero the request buffer */
	memset(tw_dev->srb[request_id]->request_buffer, 0, tw_dev->srb[request_id]->request_bufflen);
       
	tw_dev->state[request_id] = TW_S_COMPLETED;
	tw_state_request_finish(tw_dev, request_id);
  
	/* If we got a request_sense, we probably want a reset, return error */
	tw_dev->srb[request_id]->result = (DID_ERROR << 16);
	tw_dev->srb[request_id]->scsi_done(tw_dev->srb[request_id]);
  
	return 0;
} /* End tw_scsiop_request_sense() */

/* This function will handle synchronize cache scsi command */
int tw_scsiop_synchronize_cache(TW_Device_Extension *tw_dev, int request_id)
{
	TW_Command *command_packet;
	unsigned long command_que_value;

	dprintk(KERN_NOTICE "3w-xxxx: tw_scsiop_synchronize_cache()\n");

	/* Send firmware flush command for this unit */
	command_packet = (TW_Command *)tw_dev->command_packet_virtual_address[request_id];
	if (command_packet == NULL) {
		printk(KERN_WARNING "3w-xxxx: tw_scsiop_synchronize_cache(): Bad command packet virtual address.\n");
		return 1;
	}

	/* Setup the command packet */
	memset(command_packet, 0, sizeof(TW_Sector));
	command_packet->byte0.opcode = TW_OP_FLUSH_CACHE;
	command_packet->byte0.sgl_offset = 0;
	command_packet->size = 2;
	command_packet->request_id = request_id;
	command_packet->byte3.unit = tw_dev->srb[request_id]->target;
	command_packet->byte3.host_id = 0;
	command_packet->status = 0;
	command_packet->flags = 0;
	command_packet->byte6.parameter_count = 1;
	command_que_value = tw_dev->command_packet_physical_address[request_id];
	if (command_que_value == 0) {
		printk(KERN_WARNING "3w-xxxx: tw_scsiop_synchronize_cache(): Bad command packet physical address.\n");
		return 1;
	}

	/* Now try to post the command packet */
	tw_post_command_packet(tw_dev, request_id);

	return 0;
} /* End tw_scsiop_synchronize_cache() */

/* This function will handle test unit ready scsi command */
int tw_scsiop_test_unit_ready(TW_Device_Extension *tw_dev, int request_id)
{

	TW_Param *param;
	TW_Command *command_packet;
	unsigned long command_que_value;
	u32 command_que_addr;
	unsigned long param_value;

	dprintk(KERN_NOTICE "3w-xxxx: tw_scsiop_test_unit_ready()\n");

	/* Initialize command packet */
	command_que_addr = tw_dev->registers.command_que_addr;
	command_packet = (TW_Command *)tw_dev->command_packet_virtual_address[request_id];
	if (command_packet == NULL) {
		printk(KERN_WARNING "3w-xxxx: tw_scsiop_test_unit_ready(): Bad command packet virtual address.\n");
		return 1;
	}
	memset(command_packet, 0, sizeof(TW_Sector));
	command_packet->byte0.opcode = TW_OP_GET_PARAM;
	command_packet->byte0.sgl_offset = 2;
	command_packet->size = 4;
	command_packet->request_id = request_id;
	command_packet->byte3.unit = 0;
	command_packet->byte3.host_id = 0;
	command_packet->status = 0;
	command_packet->flags = 0;
	command_packet->byte6.parameter_count = 1;

	/* Now setup the param */
	if (tw_dev->alignment_virtual_address[request_id] == NULL) {
		printk(KERN_WARNING "3w-xxxx: tw_scsiop_test_unit_ready(): Bad alignment virtual address.\n");
		return 1;
	}
	param = (TW_Param *)tw_dev->alignment_virtual_address[request_id];
	memset(param, 0, sizeof(TW_Sector));
	param->table_id = 3;	 /* unit summary table */
	param->parameter_id = 3; /* unitsstatus parameter */
	param->parameter_size_bytes = TW_MAX_UNITS;
	param_value = tw_dev->alignment_physical_address[request_id];
	if (param_value == 0) {
		printk(KERN_WARNING "3w-xxxx: tw_scsiop_test_unit_ready(): Bad alignment physical address.\n");
		return 1;
	}

	command_packet->byte8.param.sgl[0].address = param_value;
	command_packet->byte8.param.sgl[0].length = sizeof(TW_Sector);
	command_que_value = tw_dev->command_packet_physical_address[request_id];
	if (command_que_value == 0) {
		printk(KERN_WARNING "3w-xxxx: tw_scsiop_test_unit_ready(): Bad command packet physical address.\n");
		return 1;
	}

	/* Now try to post the command packet */
	tw_post_command_packet(tw_dev, request_id);

	return 0;
} /* End tw_scsiop_test_unit_ready() */

/* This function is called by the isr to complete a testunitready command */
int tw_scsiop_test_unit_ready_complete(TW_Device_Extension *tw_dev, int request_id)
{
	unsigned char *is_unit_present;
	TW_Param *param;

	dprintk(KERN_WARNING "3w-xxxx: tw_scsiop_test_unit_ready_complete()\n");

	param = (TW_Param *)tw_dev->alignment_virtual_address[request_id];
	if (param == NULL) {
		printk(KERN_WARNING "3w-xxxx: tw_scsiop_test_unit_ready_complete(): Bad alignment virtual address.\n");
		return 1;
	}
	is_unit_present = &(param->data[0]);

	if (is_unit_present[tw_dev->srb[request_id]->target] & TW_UNIT_ONLINE) {
		tw_dev->is_unit_present[tw_dev->srb[request_id]->target] = TRUE;
	} else {
		tw_dev->is_unit_present[tw_dev->srb[request_id]->target] = FALSE;
		tw_dev->srb[request_id]->result = (DID_BAD_TARGET << 16);
		return TW_ISR_DONT_RESULT;
	}

	return 0;
} /* End tw_scsiop_test_unit_ready_complete() */

/* This function will select queue depths for a target */
void tw_select_queue_depths(struct Scsi_Host *host, Scsi_Device *dev)
{
	Scsi_Device *ptr;
	TW_Device_Extension *tw_dev = (TW_Device_Extension *)host->hostdata;

	dprintk(KERN_WARNING "3w-xxxx: tw_select_queue_depths()\n");

	for (ptr = dev; ptr != NULL; ptr = ptr->next) {
		if (ptr->host == host) {
			if ((tw_dev->num_raid_five > 0) && (tw_dev->tw_pci_dev->device == TW_DEVICE_ID)) {
				ptr->queue_depth = (TW_MAX_BOUNCEBUF-2)/tw_dev->num_units;
			} else {
#ifdef CONFIG_3W_XXXX_CMD_PER_LUN
				ptr->queue_depth = CONFIG_3W_XXXX_CMD_PER_LUN;
				if (ptr->queue_depth > TW_MAX_CMDS_PER_LUN)
					ptr->queue_depth = TW_MAX_CMDS_PER_LUN;
#else
				ptr->queue_depth = TW_MAX_CMDS_PER_LUN;
#endif
			}
		}
	}
} /* End tw_select_queue_depths() */

/* Set a value in the features table */
int tw_setfeature(TW_Device_Extension *tw_dev, int parm, int param_size,
                  unsigned char *val)
{
	TW_Param *param;
	TW_Command  *command_packet;
	TW_Response_Queue response_queue;
	int request_id = 0;
	unsigned long command_que_value;
	u32 command_que_addr;
	u32 response_que_addr;
	unsigned long param_value;

  	/* Initialize SetParam command packet */
	if (tw_dev->command_packet_virtual_address[request_id] == NULL) {
		printk(KERN_WARNING "3w-xxxx: tw_setfeature(): Bad command packet virtual address.\n");
		return 1;
	}
	command_packet = (TW_Command *)tw_dev->command_packet_virtual_address[request_id];
	memset(command_packet, 0, sizeof(TW_Sector));
	param = (TW_Param *)tw_dev->alignment_virtual_address[request_id];

	command_packet->byte0.opcode = TW_OP_SET_PARAM;
	command_packet->byte0.sgl_offset  = 2;
	param->table_id = 0x404;  /* Features table */
	param->parameter_id = parm;
	param->parameter_size_bytes = param_size;
	memcpy(param->data, val, param_size);

	param_value = tw_dev->alignment_physical_address[request_id];
	if (param_value == 0) {
		printk(KERN_WARNING "3w-xxxx: tw_ioctl(): Bad alignment physical address.\n");
		tw_dev->state[request_id] = TW_S_COMPLETED;
		tw_state_request_finish(tw_dev, request_id);
		tw_dev->srb[request_id]->result = (DID_OK << 16);
		tw_dev->srb[request_id]->scsi_done(tw_dev->srb[request_id]);
	}
	command_packet->byte8.param.sgl[0].address = param_value;
	command_packet->byte8.param.sgl[0].length = sizeof(TW_Sector);

	command_packet->size = 4;
	command_packet->request_id = request_id;
	command_packet->byte6.parameter_count = 1;

  	command_que_value = tw_dev->command_packet_physical_address[request_id];
	if (command_que_value == 0) {
		printk(KERN_WARNING "3w-xxxx: tw_setfeature(): Bad command packet physical address.\n");
	return 1;
	}
	command_que_addr = tw_dev->registers.command_que_addr;
	response_que_addr = tw_dev->registers.response_que_addr;

	/* Send command packet to the board */
	outl(command_que_value, command_que_addr);

	/* Poll for completion */
	if (tw_poll_status_gone(tw_dev, TW_STATUS_RESPONSE_QUEUE_EMPTY, 30) == 0) {
		response_queue.value = inl(response_que_addr);
		request_id = (unsigned char)response_queue.u.response_id;
		if (request_id != 0) {
			/* unexpected request id */
			printk(KERN_WARNING "3w-xxxx: tw_setfeature(): Unexpected request id.\n");
			return 1;
		}
		if (command_packet->status != 0) {
			/* bad response */
			tw_decode_sense(tw_dev, request_id, 0);
			return 1;
		}
	}

	return 0;
} /* End tw_setfeature() */

/* This function will setup the interrupt handler */
int tw_setup_irq(TW_Device_Extension *tw_dev)
{
	char *device = TW_DEVICE_NAME;
	int error;

	dprintk(KERN_NOTICE "3w-xxxx: tw_setup_irq()\n");
	error = request_irq(tw_dev->tw_pci_dev->irq, tw_interrupt, SA_SHIRQ, device, tw_dev);

	if (error < 0) {
		printk(KERN_WARNING "3w-xxxx: scsi%d: Error requesting IRQ: %d.\n", tw_dev->host->host_no, tw_dev->tw_pci_dev->irq);
		return 1;
	}
	return 0;
} /* End tw_setup_irq() */

/* This function will tell the controller we're shutting down by sending
   initconnection with a 1 */
int tw_shutdown_device(TW_Device_Extension *tw_dev)
{
	int error;

	/* Disable interrupts */
	tw_disable_interrupts(tw_dev);

	/* poke the board */
	error = tw_initconnection(tw_dev, 1);
	if (error) {
		printk(KERN_WARNING "3w-xxxx: scsi%d: Connection shutdown failed.\n", tw_dev->host->host_no);
	} else {
		printk(KERN_NOTICE "3w-xxxx: Shutdown complete.\n");
	}

	/* Re-enable interrupts */
	tw_enable_and_clear_interrupts(tw_dev);

	return 0;
} /* End tw_shutdown_device() */

/* This function will soft reset the controller */
void tw_soft_reset(TW_Device_Extension *tw_dev) 
{
	u32 control_reg_addr, control_reg_value;

	control_reg_addr = tw_dev->registers.control_reg_addr;
	control_reg_value = (	TW_CONTROL_ISSUE_SOFT_RESET |
				TW_CONTROL_CLEAR_HOST_INTERRUPT |
				TW_CONTROL_CLEAR_ATTENTION_INTERRUPT |
				TW_CONTROL_MASK_COMMAND_INTERRUPT |
				TW_CONTROL_MASK_RESPONSE_INTERRUPT |
				TW_CONTROL_CLEAR_ERROR_STATUS | 
				TW_CONTROL_DISABLE_INTERRUPTS);
	outl(control_reg_value, control_reg_addr);
} /* End tw_soft_reset() */

/* This function will free a request_id */
int tw_state_request_finish(TW_Device_Extension *tw_dev, int request_id)
{
	dprintk(KERN_NOTICE "3w-xxxx: tw_state_request_finish()\n");
  
	tw_dev->free_queue[tw_dev->free_tail] = request_id;
	tw_dev->state[request_id] = TW_S_FINISHED;
	if (tw_dev->free_tail == tw_dev->free_wrap)
		tw_dev->free_tail = TW_Q_START;
	else
		tw_dev->free_tail++;

	dprintk(KERN_NOTICE "3w-xxxx: tw_state_request_finish(): Freeing request_id %d\n", request_id);

	return 0;
} /* End tw_state_request_finish() */

/* This function will assign an available request_id */
int tw_state_request_start(TW_Device_Extension *tw_dev, int *request_id)
{
	int id = 0;

	dprintk(KERN_NOTICE "3w-xxxx: tw_state_request_start()\n");
	
	/* Obtain next free request_id */
	id = tw_dev->free_queue[tw_dev->free_head];
	if (tw_dev->free_head == tw_dev->free_wrap)
		tw_dev->free_head = TW_Q_START;
	else
		tw_dev->free_head++;

	*request_id = id;
	tw_dev->state[id] = TW_S_STARTED;

	dprintk(KERN_NOTICE "3w-xxxx: tw_state_request_start(): id = %d.\n", id);
	return 0;
} /* End tw_state_request_start() */

static void tw_unmap_scsi_data(struct pci_dev *pdev, Scsi_Cmnd *cmd)
{
	int dma_dir = scsi_to_pci_dma_dir(cmd->sc_data_direction);
	
	dprintk(KERN_WARNING "3w-xxxx: tw_unamp_scsi_data()\n");
	
	switch(cmd->SCp.phase) {
		case 1:
#ifdef BLK_BOUNCE_HIGH
			pci_unmap_page(pdev, cmd->SCp.have_data_in, cmd->request_bufflen, dma_dir);
#else
			pci_unmap_single(pdev, cmd->SCp.have_data_in, cmd->request_bufflen, dma_dir);
#endif
			break;
		case 2:
			pci_unmap_sg(pdev, cmd->request_buffer, cmd->use_sg, dma_dir);
			break;
	}
} /* End tw_unmap_scsi_data() */

/* This function will unmask the command interrupt on the controller */
void tw_unmask_command_interrupt(TW_Device_Extension *tw_dev)
{
	u32 control_reg_addr, control_reg_value;

	control_reg_addr = tw_dev->registers.control_reg_addr;
	control_reg_value = TW_CONTROL_UNMASK_COMMAND_INTERRUPT;
	outl(control_reg_value, control_reg_addr);
} /* End tw_unmask_command_interrupt() */

/* Now get things going */
static Scsi_Host_Template driver_template = TWXXXX;
#include "scsi_module.c"


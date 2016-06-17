/*
*
* mwavedd.c -- mwave device driver
*
*
* Written By: Mike Sullivan IBM Corporation
*
* Copyright (C) 1999 IBM Corporation
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* NO WARRANTY
* THE PROGRAM IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR
* CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED INCLUDING, WITHOUT
* LIMITATION, ANY WARRANTIES OR CONDITIONS OF TITLE, NON-INFRINGEMENT,
* MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Each Recipient is
* solely responsible for determining the appropriateness of using and
* distributing the Program and assumes all risks associated with its
* exercise of rights under this Agreement, including but not limited to
* the risks and costs of program errors, damage to or loss of data,
* programs or equipment, and unavailability or interruption of operations.
*
* DISCLAIMER OF LIABILITY
* NEITHER RECIPIENT NOR ANY CONTRIBUTORS SHALL HAVE ANY LIABILITY FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING WITHOUT LIMITATION LOST PROFITS), HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
* TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
* USE OR DISTRIBUTION OF THE PROGRAM OR THE EXERCISE OF ANY RIGHTS GRANTED
* HEREUNDER, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*
* 10/23/2000 - Alpha Release
*	First release to the public
*/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/major.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/serial.h>
#include <linux/sched.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
#include <linux/spinlock.h>
#else
#include <asm/spinlock.h>
#endif
#include <linux/delay.h>
#include "smapi.h"
#include "mwavedd.h"
#include "3780i.h"
#include "tp3780i.h"

#ifndef __exit
#define __exit
#endif

MODULE_DESCRIPTION("3780i Advanced Communications Processor (Mwave) driver");
MODULE_AUTHOR("Mike Sullivan and Paul Schroeder");
MODULE_LICENSE("GPL");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
static int mwave_get_info(char *buf, char **start, off_t offset, int len);
#else
static int mwave_read_proc(char *buf, char **start, off_t offset, int xlen, int unused);
static struct proc_dir_entry mwave_proc = {
	0,                      /* unsigned short low_ino */
	5,                      /* unsigned short namelen */
	"mwave",                /* const char *name */
	S_IFREG | S_IRUGO,      /* mode_t mode */
	1,                      /* nlink_t nlink */
	0,                      /* uid_t uid */
	0,                      /* gid_t gid */
	0,                      /* unsigned long size */
	NULL,                   /* struct inode_operations *ops */
	&mwave_read_proc        /* int (*get_info) (...) */
};
#endif

/*
* These parameters support the setting of MWave resources. Note that no
* checks are made against other devices (ie. superio) for conflicts.
* We'll depend on users using the tpctl utility to do that for now
*/
int mwave_debug = 0;
int mwave_3780i_irq = 0;
int mwave_3780i_io = 0;
int mwave_uart_irq = 0;
int mwave_uart_io = 0;
MODULE_PARM(mwave_debug, "i");
MODULE_PARM(mwave_3780i_irq, "i");
MODULE_PARM(mwave_3780i_io, "i");
MODULE_PARM(mwave_uart_irq, "i");
MODULE_PARM(mwave_uart_io, "i");

static int mwave_open(struct inode *inode, struct file *file);
static int mwave_close(struct inode *inode, struct file *file);
static int mwave_ioctl(struct inode *inode, struct file *filp,
                       unsigned int iocmd, unsigned long ioarg);

MWAVE_DEVICE_DATA mwave_s_mdd;

static int mwave_open(struct inode *inode, struct file *file)
{
	unsigned int retval = 0;

	PRINTK_3(TRACE_MWAVE,
		"mwavedd::mwave_open, entry inode %x file %x\n",
		(int) inode, (int) file);
	PRINTK_2(TRACE_MWAVE,
		"mwavedd::mwave_open, exit return retval %x\n", retval);

	MOD_INC_USE_COUNT;
	return retval;
}

static int mwave_close(struct inode *inode, struct file *file)
{
	unsigned int retval = 0;

	PRINTK_3(TRACE_MWAVE,
		"mwavedd::mwave_close, entry inode %x file %x\n",
		(int) inode, (int) file);

	PRINTK_2(TRACE_MWAVE, "mwavedd::mwave_close, exit retval %x\n",
		retval);

	MOD_DEC_USE_COUNT;
	return retval;
}

static int mwave_ioctl(struct inode *inode, struct file *file,
                       unsigned int iocmd, unsigned long ioarg)
{
	unsigned int retval = 0;
	pMWAVE_DEVICE_DATA pDrvData = &mwave_s_mdd;

	PRINTK_5(TRACE_MWAVE,
		"mwavedd::mwave_ioctl, entry inode %x file %x cmd %x arg %x\n",
		(int) inode, (int) file, iocmd, (int) ioarg);

	switch (iocmd) {

		case IOCTL_MW_RESET:
			PRINTK_1(TRACE_MWAVE,
				"mwavedd::mwave_ioctl, IOCTL_MW_RESET calling tp3780I_ResetDSP\n");
			retval = tp3780I_ResetDSP(&pDrvData->rBDData);
			PRINTK_2(TRACE_MWAVE,
				"mwavedd::mwave_ioctl, IOCTL_MW_RESET retval %x from tp3780I_ResetDSP\n",
				retval);
			break;
	
		case IOCTL_MW_RUN:
			PRINTK_1(TRACE_MWAVE,
				"mwavedd::mwave_ioctl, IOCTL_MW_RUN calling tp3780I_StartDSP\n");
			retval = tp3780I_StartDSP(&pDrvData->rBDData);
			PRINTK_2(TRACE_MWAVE,
				"mwavedd::mwave_ioctl, IOCTL_MW_RUN retval %x from tp3780I_StartDSP\n",
				retval);
			break;
	
		case IOCTL_MW_DSP_ABILITIES: {
			MW_ABILITIES rAbilities;
	
			PRINTK_1(TRACE_MWAVE,
				"mwavedd::mwave_ioctl, IOCTL_MW_DSP_ABILITIES calling tp3780I_QueryAbilities\n");
			retval = tp3780I_QueryAbilities(&pDrvData->rBDData, &rAbilities);
			PRINTK_2(TRACE_MWAVE,
				"mwavedd::mwave_ioctl, IOCTL_MW_DSP_ABILITIES retval %x from tp3780I_QueryAbilities\n",
				retval);
			if (retval == 0) {
				if( copy_to_user((char *) ioarg, (char *) &rAbilities, sizeof(MW_ABILITIES)) )
					return -EFAULT;
			}
			PRINTK_2(TRACE_MWAVE,
				"mwavedd::mwave_ioctl, IOCTL_MW_DSP_ABILITIES exit retval %x\n",
				retval);
		}
			break;
	
		case IOCTL_MW_READ_DATA:
		case IOCTL_MW_READCLEAR_DATA: {
			MW_READWRITE rReadData;
			unsigned short *pusBuffer = 0;
	
			if( copy_from_user((char *) &rReadData, (char *) ioarg, sizeof(MW_READWRITE)) )
				return -EFAULT;
			pusBuffer = (unsigned short *) (rReadData.pBuf);
	
			PRINTK_4(TRACE_MWAVE,
				"mwavedd::mwave_ioctl IOCTL_MW_READ_DATA, size %lx, ioarg %lx pusBuffer %p\n",
				rReadData.ulDataLength, ioarg, pusBuffer);
			retval = tp3780I_ReadWriteDspDStore(&pDrvData->rBDData, iocmd,
				(void *) pusBuffer, rReadData.ulDataLength, rReadData.usDspAddress);
		}
			break;
	
		case IOCTL_MW_READ_INST: {
			MW_READWRITE rReadData;
			unsigned short *pusBuffer = 0;
	
			if( copy_from_user((char *) &rReadData, (char *) ioarg, sizeof(MW_READWRITE)) )
				return -EFAULT;
			pusBuffer = (unsigned short *) (rReadData.pBuf);
	
			PRINTK_4(TRACE_MWAVE,
				"mwavedd::mwave_ioctl IOCTL_MW_READ_INST, size %lx, ioarg %lx pusBuffer %p\n",
				rReadData.ulDataLength / 2, ioarg,
				pusBuffer);
			retval = tp3780I_ReadWriteDspDStore(&pDrvData->rBDData,
				iocmd, pusBuffer,
				rReadData.ulDataLength / 2,
				rReadData.usDspAddress);
		}
			break;
	
		case IOCTL_MW_WRITE_DATA: {
			MW_READWRITE rWriteData;
			unsigned short *pusBuffer = 0;
	
			if( copy_from_user((char *) &rWriteData, (char *) ioarg, sizeof(MW_READWRITE)) )
				return -EFAULT;
			pusBuffer = (unsigned short *) (rWriteData.pBuf);
	
			PRINTK_4(TRACE_MWAVE,
				"mwavedd::mwave_ioctl IOCTL_MW_WRITE_DATA, size %lx, ioarg %lx pusBuffer %p\n",
				rWriteData.ulDataLength, ioarg,
				pusBuffer);
			retval = tp3780I_ReadWriteDspDStore(&pDrvData->rBDData, iocmd,
				pusBuffer, rWriteData.ulDataLength, rWriteData.usDspAddress);
		}
			break;
	
		case IOCTL_MW_WRITE_INST: {
			MW_READWRITE rWriteData;
			unsigned short *pusBuffer = 0;
	
			if( copy_from_user((char *) &rWriteData, (char *) ioarg, sizeof(MW_READWRITE)) )
				return -EFAULT;
			pusBuffer = (unsigned short *) (rWriteData.pBuf);
	
			PRINTK_4(TRACE_MWAVE,
				"mwavedd::mwave_ioctl IOCTL_MW_WRITE_INST, size %lx, ioarg %lx pusBuffer %p\n",
				rWriteData.ulDataLength, ioarg,
				pusBuffer);
			retval = tp3780I_ReadWriteDspIStore(&pDrvData->rBDData, iocmd,
					pusBuffer, rWriteData.ulDataLength, rWriteData.usDspAddress);
		}
			break;
	
		case IOCTL_MW_REGISTER_IPC: {
			unsigned int ipcnum = (unsigned int) ioarg;
	
			PRINTK_3(TRACE_MWAVE,
				"mwavedd::mwave_ioctl IOCTL_MW_REGISTER_IPC ipcnum %x entry usIntCount %x\n",
				ipcnum,
				pDrvData->IPCs[ipcnum].usIntCount);
	
			if (ipcnum > 16) {
				PRINTK_ERROR(KERN_ERR_MWAVE "mwavedd::mwave_ioctl: IOCTL_MW_REGISTER_IPC: Error: Invalid ipcnum %x\n", ipcnum);
				return -EINVAL;
			}
			pDrvData->IPCs[ipcnum].bIsHere = FALSE;
			pDrvData->IPCs[ipcnum].bIsEnabled = TRUE;
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
			current->nice = -20;	/* boost to provide priority timing */
	#else
			current->priority = 0x28;	/* boost to provide priority timing */
	#endif
	
			PRINTK_2(TRACE_MWAVE,
				"mwavedd::mwave_ioctl IOCTL_MW_REGISTER_IPC ipcnum %x exit\n",
				ipcnum);
		}
			break;
	
		case IOCTL_MW_GET_IPC: {
			unsigned int ipcnum = (unsigned int) ioarg;
			spinlock_t ipc_lock = SPIN_LOCK_UNLOCKED;
			unsigned long flags;
	
			PRINTK_3(TRACE_MWAVE,
				"mwavedd::mwave_ioctl IOCTL_MW_GET_IPC ipcnum %x, usIntCount %x\n",
				ipcnum,
				pDrvData->IPCs[ipcnum].usIntCount);
			if (ipcnum > 16) {
				PRINTK_ERROR(KERN_ERR_MWAVE "mwavedd::mwave_ioctl: IOCTL_MW_GET_IPC: Error: Invalid ipcnum %x\n", ipcnum);
				return -EINVAL;
			}
	
			if (pDrvData->IPCs[ipcnum].bIsEnabled == TRUE) {
				PRINTK_2(TRACE_MWAVE,
					"mwavedd::mwave_ioctl, thread for ipc %x going to sleep\n",
					ipcnum);
	
				spin_lock_irqsave(&ipc_lock, flags);
				/* check whether an event was signalled by */
				/* the interrupt handler while we were gone */
				if (pDrvData->IPCs[ipcnum].usIntCount == 1) {	/* first int has occurred (race condition) */
					pDrvData->IPCs[ipcnum].usIntCount = 2;	/* first int has been handled */
					spin_unlock_irqrestore(&ipc_lock, flags);
					PRINTK_2(TRACE_MWAVE,
						"mwavedd::mwave_ioctl IOCTL_MW_GET_IPC ipcnum %x handling first int\n",
						ipcnum);
				} else {	/* either 1st int has not yet occurred, or we have already handled the first int */
					pDrvData->IPCs[ipcnum].bIsHere = TRUE;
					interruptible_sleep_on(&pDrvData->IPCs[ipcnum].ipc_wait_queue);
					pDrvData->IPCs[ipcnum].bIsHere = FALSE;
					if (pDrvData->IPCs[ipcnum].usIntCount == 1) {
						pDrvData->IPCs[ipcnum].
						usIntCount = 2;
					}
					spin_unlock_irqrestore(&ipc_lock, flags);
					PRINTK_2(TRACE_MWAVE,
						"mwavedd::mwave_ioctl IOCTL_MW_GET_IPC ipcnum %x woke up and returning to application\n",
						ipcnum);
				}
				PRINTK_2(TRACE_MWAVE,
					"mwavedd::mwave_ioctl IOCTL_MW_GET_IPC, returning thread for ipc %x processing\n",
					ipcnum);
			}
		}
			break;
	
		case IOCTL_MW_UNREGISTER_IPC: {
			unsigned int ipcnum = (unsigned int) ioarg;
	
			PRINTK_2(TRACE_MWAVE,
				"mwavedd::mwave_ioctl IOCTL_MW_UNREGISTER_IPC ipcnum %x\n",
				ipcnum);
			if (ipcnum > 16) {
				PRINTK_ERROR(KERN_ERR_MWAVE "mwavedd::mwave_ioctl: IOCTL_MW_UNREGISTER_IPC: Error: Invalid ipcnum %x\n", ipcnum);
				return -EINVAL;
			}
			if (pDrvData->IPCs[ipcnum].bIsEnabled == TRUE) {
				pDrvData->IPCs[ipcnum].bIsEnabled = FALSE;
				if (pDrvData->IPCs[ipcnum].bIsHere == TRUE) {
					wake_up_interruptible(&pDrvData->IPCs[ipcnum].ipc_wait_queue);
				}
			}
		}
			break;
	
		default:
			PRINTK_ERROR(KERN_ERR_MWAVE "mwavedd::mwave_ioctl: Error: Unrecognized iocmd %x\n", iocmd);
			return -ENOTTY;
			break;
	} /* switch */

	PRINTK_2(TRACE_MWAVE, "mwavedd::mwave_ioctl, exit retval %x\n", retval);

	return retval;
}


static ssize_t mwave_read(struct file *file, char *buf, size_t count,
                          loff_t * ppos)
{
	PRINTK_5(TRACE_MWAVE,
		"mwavedd::mwave_read entry file %p, buf %p, count %x ppos %p\n",
		file, buf, count, ppos);

	return -EINVAL;
}


static ssize_t mwave_write(struct file *file, const char *buf,
                           size_t count, loff_t * ppos)
{
	PRINTK_5(TRACE_MWAVE,
		"mwavedd::mwave_write entry file %p, buf %p, count %x ppos %p\n",
		file, buf, count, ppos);

	return -EINVAL;
}


static int register_serial_portandirq(unsigned int port, int irq)
{
	struct serial_struct serial;

	switch ( port ) {
		case 0x3f8:
		case 0x2f8:
		case 0x3e8:
		case 0x2e8:
			/* OK */
			break;
		default:
			PRINTK_ERROR(KERN_ERR_MWAVE "mwavedd::register_serial_portandirq: Error: Illegal port %x\n", port );
			return -1;
	} /* switch */
	/* port is okay */

	switch ( irq ) {
		case 3:
		case 4:
		case 5:
		case 7:
			/* OK */
			break;
		default:
			PRINTK_ERROR(KERN_ERR_MWAVE "mwavedd::register_serial_portandirq: Error: Illegal irq %x\n", irq );
			return -1;
	} /* switch */
	/* irq is okay */

	memset(&serial, 0, sizeof(serial));
	serial.port = port;
	serial.irq = irq;
	serial.flags = ASYNC_SHARE_IRQ;

	return register_serial(&serial);
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
static struct file_operations mwave_fops = {
	owner:THIS_MODULE,
	read:mwave_read,
	write:mwave_write,
	ioctl:mwave_ioctl,
	open:mwave_open,
	release:mwave_close
};
#else
static struct file_operations mwave_fops = {
	NULL,			/* lseek */
	mwave_read,		/* read */
	mwave_write,		/* write */
	NULL,			/* readdir */
	NULL,			/* poll */
	mwave_ioctl,		/* ioctl */
	NULL,			/* mmap */
	mwave_open,		/* open */
	NULL,			/* flush */
	mwave_close		/* release */
};
#endif

static struct miscdevice mwave_misc_dev = { MWAVE_MINOR, "mwave", &mwave_fops };

/*
* mwave_init is called on module load
*
* mwave_exit is called on module unload
* mwave_exit is also used to clean up after an aborted mwave_init
*/
static void mwave_exit(void)
{
	pMWAVE_DEVICE_DATA pDrvData = &mwave_s_mdd;

	PRINTK_1(TRACE_MWAVE, "mwavedd::mwave_exit entry\n");

	if (pDrvData->bProcEntryCreated) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
		remove_proc_entry("mwave", NULL);
#else
		proc_unregister(&proc_root, mwave_proc.low_ino);
#endif
	}
	if ( pDrvData->sLine >= 0 ) {
		unregister_serial(pDrvData->sLine);
	}
	if (pDrvData->bMwaveDevRegistered) {
		misc_deregister(&mwave_misc_dev);
	}
	if (pDrvData->bDSPEnabled) {
		tp3780I_DisableDSP(&pDrvData->rBDData);
	}
	if (pDrvData->bResourcesClaimed) {
		tp3780I_ReleaseResources(&pDrvData->rBDData);
	}
	if (pDrvData->bBDInitialized) {
		tp3780I_Cleanup(&pDrvData->rBDData);
	}

	PRINTK_1(TRACE_MWAVE, "mwavedd::mwave_exit exit\n");
}

module_exit(mwave_exit);

static int __init mwave_init(void)
{
	int i;
	int retval = 0;
	int resultMiscRegister;
	pMWAVE_DEVICE_DATA pDrvData = &mwave_s_mdd;

	memset(&mwave_s_mdd, 0, sizeof(MWAVE_DEVICE_DATA));

	PRINTK_1(TRACE_MWAVE, "mwavedd::mwave_init entry\n");

	pDrvData->bBDInitialized = FALSE;
	pDrvData->bResourcesClaimed = FALSE;
	pDrvData->bDSPEnabled = FALSE;
	pDrvData->bDSPReset = FALSE;
	pDrvData->bMwaveDevRegistered = FALSE;
	pDrvData->sLine = -1;
	pDrvData->bProcEntryCreated = FALSE;

	for (i = 0; i < 16; i++) {
		pDrvData->IPCs[i].bIsEnabled = FALSE;
		pDrvData->IPCs[i].bIsHere = FALSE;
		pDrvData->IPCs[i].usIntCount = 0;	/* no ints received yet */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
		init_waitqueue_head(&pDrvData->IPCs[i].ipc_wait_queue);
#endif
	}

	retval = tp3780I_InitializeBoardData(&pDrvData->rBDData);
	PRINTK_2(TRACE_MWAVE,
		"mwavedd::mwave_init, return from tp3780I_InitializeBoardData retval %x\n",
		retval);
	if (retval) {
		PRINTK_ERROR(KERN_ERR_MWAVE "mwavedd::mwave_init: Error: Failed to initialize board data\n");
		goto cleanup_error;
	}
	pDrvData->bBDInitialized = TRUE;

	retval = tp3780I_CalcResources(&pDrvData->rBDData);
	PRINTK_2(TRACE_MWAVE,
		"mwavedd::mwave_init, return from tp3780I_CalcResources retval %x\n",
		retval);
	if (retval) {
		PRINTK_ERROR(KERN_ERR_MWAVE "mwavedd:mwave_init: Error: Failed to calculate resources\n");
		goto cleanup_error;
	}

	retval = tp3780I_ClaimResources(&pDrvData->rBDData);
	PRINTK_2(TRACE_MWAVE,
		"mwavedd::mwave_init, return from tp3780I_ClaimResources retval %x\n",
		retval);
	if (retval) {
		PRINTK_ERROR(KERN_ERR_MWAVE "mwavedd:mwave_init: Error: Failed to claim resources\n");
		goto cleanup_error;
	}
	pDrvData->bResourcesClaimed = TRUE;

	retval = tp3780I_EnableDSP(&pDrvData->rBDData);
	PRINTK_2(TRACE_MWAVE,
		"mwavedd::mwave_init, return from tp3780I_EnableDSP retval %x\n",
		retval);
	if (retval) {
		PRINTK_ERROR(KERN_ERR_MWAVE "mwavedd:mwave_init: Error: Failed to enable DSP\n");
		goto cleanup_error;
	}
	pDrvData->bDSPEnabled = TRUE;

	resultMiscRegister = misc_register(&mwave_misc_dev);
	if (resultMiscRegister < 0) {
		PRINTK_ERROR(KERN_ERR_MWAVE "mwavedd:mwave_init: Error: Failed to register misc device\n");
		goto cleanup_error;
	}
	pDrvData->bMwaveDevRegistered = TRUE;

	pDrvData->sLine = register_serial_portandirq(
		pDrvData->rBDData.rDspSettings.usUartBaseIO,
		pDrvData->rBDData.rDspSettings.usUartIrq
	);
	if (pDrvData->sLine < 0) {
		PRINTK_ERROR(KERN_ERR_MWAVE "mwavedd:mwave_init: Error: Failed to register serial driver\n");
		goto cleanup_error;
	}
	/* uart is registered */

	if (
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
		!create_proc_info_entry("mwave", 0, NULL, mwave_get_info)
#else
		proc_register(&proc_root, &mwave_proc)
#endif
	) {
		PRINTK_ERROR(KERN_ERR_MWAVE "mwavedd::mwave_init: Error: Failed to register /proc/mwave\n");
		goto cleanup_error;
	}
	pDrvData->bProcEntryCreated = TRUE;

	/* SUCCESS! */
	return 0;

	cleanup_error:
	PRINTK_ERROR(KERN_ERR_MWAVE "mwavedd::mwave_init: Error: Failed to initialize\n");
	mwave_exit(); /* clean up */

	return -EIO;
}

module_init(mwave_init);


/*
* proc entry stuff added by Ian Pilcher <pilcher@us.ibm.com>
*/

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
static int mwave_get_info(char *buf, char **start, off_t offset, int len)
{
	DSP_3780I_CONFIG_SETTINGS *pSettings = &mwave_s_mdd.rBDData.rDspSettings;

	char *out = buf;

	out += sprintf(out, "3780i_IRQ %i\n", pSettings->usDspIrq);
	out += sprintf(out, "3780i_DMA %i\n", pSettings->usDspDma);
	out += sprintf(out, "3780i_IO  %#.4x\n", pSettings->usDspBaseIO);
	out += sprintf(out, "UART_IRQ  %i\n", pSettings->usUartIrq);
	out += sprintf(out, "UART_IO   %#.4x\n", pSettings->usUartBaseIO);

	return out - buf;
}
#else /* kernel version < 2.4.0 */
static int mwave_read_proc(char *buf, char **start, off_t offset,
                           int xlen, int unused)
{
	DSP_3780I_CONFIG_SETTINGS *pSettings = &mwave_s_mdd.rBDData.rDspSettings;
	int len;

	len = sprintf(buf,        "3780i_IRQ %i\n", pSettings->usDspIrq);
	len += sprintf(&buf[len], "3780i_DMA %i\n", pSettings->usDspDma);
	len += sprintf(&buf[len], "3780i_IO  %#.4x\n", pSettings->usDspBaseIO);
	len += sprintf(&buf[len], "UART_IRQ  %i\n", pSettings->usUartIrq);
	len += sprintf(&buf[len], "UART_IO   %#.4x\n", pSettings->usUartBaseIO);

	return len;
}
#endif

/*******************************************************************************
*
*      "cs4281_wrapper.h" --  Cirrus Logic-Crystal CS4281 linux audio driver.
*
*      Copyright (C) 2000,2001  Cirrus Logic Corp.  
*            -- tom woller (twoller@crystal.cirrus.com) or
*               (audio@crystal.cirrus.com).
*
*      This program is free software; you can redistribute it and/or modify
*      it under the terms of the GNU General Public License as published by
*      the Free Software Foundation; either version 2 of the License, or
*      (at your option) any later version.
*
*      This program is distributed in the hope that it will be useful,
*      but WITHOUT ANY WARRANTY; without even the implied warranty of
*      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*      GNU General Public License for more details.
*
*      You should have received a copy of the GNU General Public License
*      along with this program; if not, write to the Free Software
*      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*
* 12/22/00 trw - new file. 
* 04/18/01 trw - rework entire wrapper logic.
*
*******************************************************************************/
#ifndef __CS4281_WRAPPER_H
#define __CS4281_WRAPPER_H

/* 2.4.x wrapper */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,9)
static int cs4281_null_suspend(struct pci_dev *pcidev, u32 unused) { return 0; }
static int cs4281_null_resume(struct pci_dev *pcidev) { return 0; }
#else
#define no_llseek cs4281_llseek
static loff_t cs4281_llseek(struct file *file, loff_t offset, int origin)
{
	return -ESPIPE;
}
void cs4281_null_suspend(struct pci_dev *pcidev) { return; }
void cs4281_null_resume(struct pci_dev *pcidev) { return; }
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,3)
/* Some versions of 2.4.2 resolve pci_set_dma_mask and some do not... 
*  but 2.4.0 definitely does not 
*/
#define pci_set_dma_mask(dev,data) 0;
#else
#endif
#define cs4x_mem_map_reserve(page) mem_map_reserve(page)
#define cs4x_mem_map_unreserve(page) mem_map_unreserve(page)

#endif /* #ifndef __CS4281_WRAPPER_H */

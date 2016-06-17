/*******************************************************************************
*
*      "cs4281pm-24.h" --  Cirrus Logic-Crystal CS4281 linux audio driver.
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
* 2001.04.05 trw - new file. 
*
*******************************************************************************/
#ifndef NOT_CS4281_PM
#include <linux/pm.h>
#include <linux/config.h>
#include "cs4281pm.h"

//#define CS4281_PCI_PM_SUPPORT_ENABLE 1
#if CS4281_PCI_PM_SUPPORT_ENABLE
#define cs_pm_register(a, b, c) 0;
#define cs_pm_unregister_all(a)
/* 
* for now (12/22/00) only enable the pm_register PM support.
* allow these table entries to be null.
*/
#define CS4281_SUSPEND_TBL cs4281_suspend_tbl
#define CS4281_RESUME_TBL cs4281_resume_tbl
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,12)
static int cs4281_suspend_tbl(struct pci_dev *pcidev, u32 unused);
static int cs4281_resume_tbl(struct pci_dev *pcidev);
#else 
void cs4281_suspend_tbl(struct pci_dev *pcidev);
void cs4281_resume_tbl(struct pci_dev *pcidev);
#endif //LINUX_VERSION
#else //CS4281_PCI_PM_SUPPORT_ENABLE
#ifdef CONFIG_PM
int cs4281_pm_callback(struct pm_dev *dev, pm_request_t rqst, void *data);
#define cs_pm_register(a, b, c) pm_register((a), (b), (c));
#define cs_pm_unregister_all(a) pm_unregister_all((a));
#define CS4281_SUSPEND_TBL cs4281_null_suspend
#define CS4281_RESUME_TBL cs4281_null_resume
#else
#define cs_pm_register(a, b, c) 0;
#define cs_pm_unregister_all(a)
#define CS4281_SUSPEND_TBL cs4281_null_suspend
#define CS4281_RESUME_TBL cs4281_null_resume
#endif //CONFIG_PM
#endif //CS4281_PCI_PM_SUPPORT_ENABLE
#else
#define cs_pm_register(a, b, c) 0;
#define cs_pm_unregister_all(a)
#define CS4281_SUSPEND_TBL cs4281_null_suspend
#define CS4281_RESUME_TBL cs4281_null_resume
#endif //NOT_CS4281_PM

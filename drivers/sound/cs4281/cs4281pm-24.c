/*******************************************************************************
*
*      "cs4281pm.c" --  Cirrus Logic-Crystal CS4281 linux audio driver.
*
*      Copyright (C) 2000,2001  Cirrus Logic Corp.  
*            -- tom woller (twoller@crystal.cirrus.com) or
*               (pcaudio@crystal.cirrus.com).
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
*
*******************************************************************************/

#ifndef NOT_CS4281_PM

#if CS4281_PCI_PM_SUPPORT_ENABLE
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,12)
static int cs4281_suspend_tbl(struct pci_dev *pcidev, u32 unused)
{
	struct cs4281_state *s = pci_get_drvdata(pcidev);
	cs4281_suspend(s);
	return 0;
}

static int cs4281_resume_tbl(struct pci_dev *pcidev)
{
	struct cs4281_state *s = pci_get_drvdata(pcidev);
	cs4281_resume(s);
	return 0;
}
#else
void cs4281_suspend_tbl(struct pci_dev *pcidev)
{
	struct cs4281_state *s = pci_get_drvdata(pcidev);
	cs4281_suspend(s);
	return;
}

void cs4281_resume_tbl(struct pci_dev *pcidev)
{
	struct cs4281_state *s = pci_get_drvdata(pcidev);
	cs4281_resume(s);
	return;
}
#endif
#else
int cs4281_pm_callback(struct pm_dev *dev, pm_request_t rqst, void *data)
{
	struct cs4281_state *state;

	CS_DBGOUT(CS_PM, 2, printk(
		"cs4281: cs4281_pm_callback()+ dev=0x%x rqst=0x%x state=%d\n",
			(unsigned)dev,(unsigned)rqst,(unsigned)data));
	state = (struct cs4281_state *) dev->data;
	if (state) {
		switch(rqst) {
			case PM_SUSPEND:
				CS_DBGOUT(CS_PM, 2, printk(KERN_INFO
					"cs4281: PM suspend request\n"));
				if(cs4281_suspend(state))
				{
				    CS_DBGOUT(CS_ERROR, 2, printk(KERN_INFO
					"cs4281: PM suspend request refused\n"));
					return 1; 
				}
				break;
			case PM_RESUME:
				CS_DBGOUT(CS_PM, 2, printk(KERN_INFO
					"cs4281: PM resume request\n"));
				if(cs4281_resume(state))
				{
				    CS_DBGOUT(CS_ERROR, 2, printk(KERN_INFO
					"cs4281: PM resume request refused\n"));
					return 1;
				}
				break;
		}
	}
	CS_DBGOUT(CS_PM, 2, printk("cs4281: cs4281_pm_callback()- 0\n"));

	return 0;
}
#endif //#if CS4281_PCI_PM_SUPPORT_ENABLE

#else /* NOT_CS4281_PM */
#define CS4281_SUSPEND_TBL cs4281_null_suspend
#define CS4281_RESUME_TBL cs4281_null_resume
#endif /* NOT_CS4281_PM */

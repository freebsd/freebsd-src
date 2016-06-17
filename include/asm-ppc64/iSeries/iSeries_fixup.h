
#ifndef	__ISERIES_FIXUP_H__
#define	__ISERIES_FIXUP_H__
#include <linux/pci.h>

#ifdef __cplusplus
extern "C" {
#endif

void iSeries_fixup (void);
void iSeries_fixup_bus (struct pci_bus*);
unsigned int iSeries_scan_slot (struct pci_dev*, u16, u8, u8);


/* Need to store information related to the PHB bucc and make it accessible to the hose */
struct iSeries_hose_arch_data {
	u32 hvBusNumber;
};


#ifdef __cplusplus
}
#endif

#endif /* __ISERIES_FIXUP_H__ */

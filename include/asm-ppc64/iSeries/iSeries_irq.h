
#ifndef	__ISERIES_IRQ_H__
#define	__ISERIES_IRQ_H__


#ifdef __cplusplus
extern "C" {
#endif

unsigned int iSeries_startup_IRQ(unsigned int);
void iSeries_shutdown_IRQ(unsigned int);
void iSeries_enable_IRQ(unsigned int);
void iSeries_disable_IRQ(unsigned int);
void iSeries_end_IRQ(unsigned int);
void iSeries_init_IRQ(void);
void iSeries_init_irqMap(int);
int  iSeries_allocate_IRQ(HvBusNumber, HvSubBusNumber, HvAgentId);
int  iSeries_assign_IRQ(int, HvBusNumber, HvSubBusNumber, HvAgentId);
void iSeries_activate_IRQs(void);

int XmPciLpEvent_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __ISERIES_IRQ_H__ */

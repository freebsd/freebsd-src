#ifndef _LINUX_MPP_H
#define _LINUX_MPP_H

/*
 * Definitions related to Massively Parallel Processing support.
 */

/* All mpp implementations must supply these functions */

extern void mpp_init(void);
extern void mpp_hw_init(void);
extern void mpp_procfs_init(void);

extern int mpp_num_cells(void);
extern int mpp_cid(void);
extern int get_mppinfo(char *buffer);

#endif

/*
 *  $FreeBSD$
 */


#ifdef CONFIG_DEFINED
#include <cxgb_osdep.h>
#include <common/cxgb_common.h>
#include <cxgb_ioctl.h>
#include <cxgb_offload.h>
#include <common/cxgb_regs.h>
#include <common/cxgb_t3_cpl.h>
#include <dev/cxgb/common/cxgb_ctl_defs.h>
#include <dev/cxgb/common/cxgb_sge_defs.h>
#include <common/cxgb_firmware_exports.h>
#include <sys/mvec.h>
#include <ulp/toecore/toedev.h>
#include <sys/mbufq.h>
#include <common/jhash.h>


#else
#include <dev/cxgb/cxgb_osdep.h>
#include <dev/cxgb/common/cxgb_common.h>
#include <dev/cxgb/cxgb_ioctl.h>
#include <dev/cxgb/cxgb_offload.h>
#include <dev/cxgb/common/cxgb_regs.h>
#include <dev/cxgb/common/cxgb_t3_cpl.h>
#include <dev/cxgb/common/cxgb_ctl_defs.h>
#include <dev/cxgb/common/cxgb_sge_defs.h>
#include <dev/cxgb/common/cxgb_firmware_exports.h>

#include <dev/cxgb/sys/mvec.h>
#include <dev/cxgb/ulp/toecore/toedev.h>
#include <dev/cxgb/sys/mbufq.h>
#include <dev/cxgb/common/jhash.h>
#endif

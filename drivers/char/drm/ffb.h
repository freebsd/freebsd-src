/* ffb.h -- ffb DRM template customization -*- linux-c -*-
 */

#ifndef __FFB_H__
#define __FFB_H__

/* This remains constant for all DRM template files.
 */
#define DRM(x) ffb_##x

/* General customization:
 */
#define __HAVE_KERNEL_CTX_SWITCH	1
#define __HAVE_RELEASE			1
#endif

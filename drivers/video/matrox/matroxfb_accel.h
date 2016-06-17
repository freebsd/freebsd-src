#ifndef __MATROXFB_ACCEL_H__
#define __MATROXFB_ACCEL_H__

#include "matroxfb_base.h"

void matrox_init_putc(WPMINFO struct display* p, void (*)(WPMINFO struct display *p));
void matrox_cfbX_init(WPMINFO struct display* p);
void matrox_text_round(CPMINFO struct fb_var_screeninfo* var, struct display* p);
void initMatrox(WPMINFO struct display* p);

#endif

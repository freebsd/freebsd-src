#ifndef __MATROXFB_MISC_H__
#define __MATROXFB_MISC_H__

#include "matroxfb_base.h"

/* also for modules */
int matroxfb_PLL_calcclock(const struct matrox_pll_features* pll, unsigned int freq, unsigned int fmax,
	unsigned int* in, unsigned int* feed, unsigned int* post);
static inline int PLL_calcclock(CPMINFO unsigned int freq, unsigned int fmax,
		unsigned int* in, unsigned int* feed, unsigned int* post) {
	return matroxfb_PLL_calcclock(&ACCESS_FBINFO(features.pll), freq, fmax, in, feed, post);
}

void matroxfb_createcursorshape(WPMINFO struct display* p, int vmode);
int matroxfb_vgaHWinit(WPMINFO struct my_timming* m, struct display* p);
void matroxfb_vgaHWrestore(WPMINFO2);
void matroxfb_fastfont_init(struct matrox_fb_info* minfo);
int matrox_text_loadfont(WPMINFO struct display* p);
int matroxfb_fastfont_tryset(WPMINFO struct display* p);
void matroxfb_read_pins(WPMINFO2);

#endif	/* __MATROXFB_MISC_H__ */

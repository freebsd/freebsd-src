#include "desktop/plotters.h"

extern const struct plotter_table fb_plotters;

nsfb_t *framebuffer_initialise(const char *fename, int width, int height, int bpp);
void framebuffer_finalise(void);
bool framebuffer_set_cursor(struct fbtk_bitmap *bm);

/** Set framebuffer surface to render into
 *
 * @return return old surface
 */
nsfb_t *framebuffer_set_surface(nsfb_t *new_nsfb);

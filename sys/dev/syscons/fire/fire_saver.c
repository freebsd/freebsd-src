
/*
 * brad forschinger, 19990504
 * <retch@flag.blackened.net>
 * 
 * written with much help from warp_saver.c
 * 
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/syslog.h>

#include <machine/md_var.h>
#include <machine/random.h>

#include <saver.h>

#define X_SIZE 320
#define Y_SIZE 200

int blanked;
u_char fire_pal[768];
u_char buf[X_SIZE * (Y_SIZE + 1)];

int fire_saver(video_adapter_t *adp, int blank) {
  static u_char *vid;
  int x, y;

  if (blank) {
    if (blanked <= 0) {
      int red, green, blue;
      int palette_index;
      
      set_video_mode(adp, M_VGA_CG320);
      
      /* build palette */
      red = green = blue = 0;
      for (palette_index = 0; palette_index < 256; palette_index++) {

	red++;
	if (red > 128)
	  green += 2;
	
	fire_pal[(palette_index * 3) + 0] = red;
	fire_pal[(palette_index * 3) + 1] = green;
	fire_pal[(palette_index * 3) + 2] = blue;
      }
      load_palette(adp, fire_pal);
      
      blanked++;
      vid = (u_char *)adp->va_window;
    }
    
    /* make a new bottom line */
    for (x = 0, y = Y_SIZE; x < X_SIZE; x++) 
      buf[x + (y * X_SIZE)] = random() % 160 + 96;
  
    /* fade the flames out */
    for (y = 0; y < Y_SIZE; y++) {
      for (x = 0; x < X_SIZE; x++) {
	buf[x + (y * X_SIZE)] = (buf[(x + 0) + ((y + 0) * X_SIZE)] +
				 buf[(x - 1) + ((y + 1) * X_SIZE)] +
				 buf[(x + 0) + ((y + 1) * X_SIZE)] +
				 buf[(x + 1) + ((y + 1) * X_SIZE)]) / 4;
	if (buf[x + (y * X_SIZE)] > 0)
	  buf[x + (y * X_SIZE)]--;
      }
    }
    
    /* put our buffer into video ram */
    memcpy(vid, buf, X_SIZE * Y_SIZE);
  } else {
    blanked = 0;
  }
  
  return(0);
}

int fire_initialise(video_adapter_t *adp) {
  video_info_t info;
  
  if (get_mode_info(adp, M_VGA_CG320, &info)) {
    log(LOG_NOTICE, "fire_saver: the console does not support M_VGA_CG320\n");
    return(ENODEV);
  } 
  
  blanked = 0;    
  
  return(0);
}
  
int fire_terminate(video_adapter_t *adp) {
  return(0);
}

static scrn_saver_t fire_module = {
  "fire_saver", fire_initialise, fire_terminate, fire_saver, NULL
};

SAVER_MODULE(fire_saver, fire_module);

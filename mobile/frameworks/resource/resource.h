/*
 * Resource Management
 * Display metrics, configuration, asset loading, string resources
 */

#ifndef _RESOURCE_H_
#define _RESOURCE_H_

#include <sys/types.h>
#include <stdlib.h>

#define RES_DPI_LDPI     120
#define RES_DPI_MDPI     160
#define RES_DPI_HDPI     240
#define RES_DPI_XHDPI    320
#define RES_DPI_XXHDPI   480
#define RES_DPI_XXXHDPI  640

typedef enum {
    RES_ORIENTATION_PORTRAIT  = 0,
    RES_ORIENTATION_LANDSCAPE = 1,
    RES_ORIENTATION_SQUARE   = 2,
} res_orientation_t;

typedef enum {
    RES_UI_MODE_NORMAL  = 0,
    RES_UI_MODE_DESK    = 1,
    RES_UI_MODE_CAR     = 2,
    RES_UI_MODE_TELEV   = 3,
    RES_UI_MODE_APPLIANCE = 4,
} res_ui_mode_t;

typedef enum {
    RES_TOUCHSCREEN_NOTOUCH = 0,
    RES_TOUCHSCREEN_STYLUS  = 1,
    RES_TOUCHSCREEN_FINGER  = 2,
} res_touchscreen_t;

typedef struct display_t {
    int  width;
    int  height;
    int  density_dpi;
    int  refresh_rate;    /* Hz */
    int  physical_width_mm;
    int  physical_height_mm;
} display_t;

typedef struct config_t {
    res_orientation_t orientation;
    res_ui_mode_t      ui_mode;
    res_touchscreen_t  touchscreen;
    int                smallest_width_dp;
    int                screen_height_dp;
    int                screen_width_dp;
} config_t;

typedef struct asset_t {
    void  *data;
    size_t size;
    char   type[32];
} asset_t;

int res_init(void);
void res_shutdown(void);

int res_get_display(display_t *out_disp);
int res_get_config(config_t *out_cfg);
int res_density_to_dpi(const char *density);
float res_scale_factor(void);

int    res_get_string(int id, char *out_buf, size_t max_len);
asset_t *res_load_asset(const char *type, const char *name);
void   res_free_asset(asset_t *asset);

int res_dpi_to_density(int dpi);

#endif /* _RESOURCE_H_ */

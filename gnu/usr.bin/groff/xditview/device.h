
typedef struct _DeviceFont DeviceFont;

typedef struct _Device {
    char *name;
    int sizescale;
    int res;
    int unitwidth;
    int paperlength;
    int paperwidth;
    int X11;
    DeviceFont *fonts;
} Device;

extern void device_destroy();
extern Device *device_load();
extern DeviceFont *device_find_font();
extern int device_char_width();
extern char *device_name_for_code();
extern int device_code_width();
extern int device_font_special();

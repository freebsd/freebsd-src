
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

void device_destroy(Device *);
Device *device_load(const char *);
DeviceFont *device_find_font(Device *, const char *);
int device_char_width(DeviceFont *, int, const char *, int *);
char *device_name_for_code(DeviceFont *, int);
int device_code_width(DeviceFont *, int, int, int *);
int device_font_special(DeviceFont *);

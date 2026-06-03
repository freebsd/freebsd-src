/*
 * View System - UI widget hierarchy
 */

#ifndef _VIEW_H_
#define _VIEW_H_

#include <stdint.h>
#include <stdlib.h>

#define VIEW_MAX_CHILDREN 64
#define WRAP_CONTENT   -1
#define MATCH_PARENT   -2

#define VIEW_TYPE_VIEW        0
#define VIEW_TYPE_VIEWGROUP   1
#define VIEW_TYPE_BUTTON      2
#define VIEW_TYPE_TEXTVIEW    3
#define VIEW_TYPE_IMAGEVIEW   4
#define VIEW_TYPE_EDITABLE    5
#define VIEW_TYPE_SCROLLVIEW  6

typedef enum {
    ORIENTATION_VERTICAL   = 0,
    ORIENTATION_HORIZONTAL = 1,
} orientation_t;

typedef struct view_group view_group_t;

typedef struct view {
    int         id;
    int         type;
    int8_t      visible;
    int32_t     x, y, w, h;
    int32_t     measured_w, measured_h;
    int32_t     layout_w, layout_h;
    int32_t     padding[4];
    int32_t     margin[4];
    uint32_t    bg_color;
    uint32_t    text_color;
    int32_t     text_size;
    char        text[256];
    void       *priv;
    view_group_t *parent;
} view_t;

struct view_group {
    view_t      base;
    view_t     *children[VIEW_MAX_CHILDREN];
    int         child_count;
    int         stack_mode;
};

int  view_init(void);
void view_shutdown(void);

view_t      *view_create(int id);
view_group_t *view_group_create(int id, int stack_mode);

void view_set_layout_params(view_t *v, int w, int h);
void view_set_bg(view_t *v, uint32_t color);
void view_set_text(view_t *v, const char *text);
void view_set_text_color(view_t *v, uint32_t color);
void view_set_text_size(view_t *v, int size);
void view_set_padding(view_t *v, int left, int top, int right, int bottom);
void view_set_margin(view_t *v, int left, int top, int right, int bottom);
void view_set_visible(view_t *v, int visible);

/* Layout */
void view_layout_linear(view_t *v, int spec_w, int spec_h, int x, int y, int w, int h);
void view_layout_frame(view_t *v, int spec_w, int spec_h, int x, int y, int w, int h);
void view_layout_relative(view_t *v, int spec_w, int spec_h, int x, int y, int w, int h);

/* Inflate an XML layout string */
view_t *view_inflate(const char *xml, view_group_t *parent);

int      view_add_child(view_group_t *parent, view_t *child);
view_t  *view_find_by_id(view_t *v, int id);

#endif /* _VIEW_H_ */

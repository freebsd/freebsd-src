/*
 * View System - UI widget hierarchy - Implementation
 */

#include <sys/param.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>

#include "view.h"

static int g_init = 0;

int
view_init(void)
{
    if (g_init)
        return 0;

    view_t *root = view_create(0x7f000001);
    /* Root window */
    g_init = 1;
    return 0;
}

void
view_shutdown(void)
{
    g_init = 0;
}

view_t *
view_create(int id)
{
    view_t *v = calloc(1, sizeof(view_t));
    if (!v) return NULL;
    v->id = id;
    v->type = VIEW_TYPE_VIEW;
    v->visible = 1;
    v->layout_w = MATCH_PARENT;
    v->layout_h = MATCH_PARENT;
    v->bg_color = 0xFF000000;
    v->text_color = 0xFFFFFFFF;
    v->text_size = 14;
    return v;
}

view_group_t *
view_group_create(int id, int stack_mode)
{
    view_group_t *vg = calloc(1, sizeof(view_group_t));
    if (!vg) return NULL;
    view_t *base = view_create(id);
    if (!base) { free(vg); return NULL; }
    vg->base = *base;
    vg->base.type = VIEW_TYPE_VIEWGROUP;
    vg->stack_mode = stack_mode;
    free(base);
    return vg;
}

void
view_set_layout_params(view_t *v, int w, int h)
{
    if (!v) return;
    v->layout_w = w;
    v->layout_h = h;
}

void
view_set_bg(view_t *v, uint32_t color)
{
    if (!v) return;
    v->bg_color = color;
}

void
view_set_text(view_t *v, const char *text)
{
    if (!v || !text) return;
    strlcpy(v->text, text, sizeof(v->text));
}

void
view_set_text_color(view_t *v, uint32_t color)
{
    if (!v) return;
    v->text_color = color;
}

void
view_set_text_size(view_t *v, int size)
{
    if (!v) return;
    v->text_size = size;
}

void
view_set_padding(view_t *v, int l, int t, int r, int b)
{
    if (!v) return;
    v->padding[0] = l; v->padding[1] = t;
    v->padding[2] = r; v->padding[3] = b;
}

void
view_set_margin(view_t *v, int l, int t, int r, int b)
{
    if (!v) return;
    v->margin[0] = l; v->margin[1] = t;
    v->margin[2] = r; v->margin[3] = b;
}

void
view_set_visible(view_t *v, int visible)
{
    if (!v) return;
    v->visible = (int8_t)visible;
}

int
view_add_child(view_group_t *parent, view_t *child)
{
    if (!parent || !child || parent->base.child_count >= VIEW_MAX_CHILDREN)
        return -1;
    parent->children[parent->base.child_count++] = child;
    child->parent = (view_group_t *)parent;
    return 0;
}

view_t *
view_find_by_id(view_t *v, int id)
{
    (void)id;
    return v;
}

view_t *
view_inflate(const char *xml, view_group_t *parent)
{
    (void)xml; (void)parent;
    return NULL;
}

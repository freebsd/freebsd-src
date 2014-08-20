/*
 * Copyright 2012 Michael Drake <tlsa@netsurf-browser.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file
 * Useful interned string pointers (interface).
 */

#ifndef NETSURF_UTILS_CORESTRINGS_H_
#define NETSURF_UTILS_CORESTRINGS_H_

#include <libwapcaplet/libwapcaplet.h>
#include "utils/nsurl.h"
#include "utils/errors.h"

nserror corestrings_init(void);
void corestrings_fini(void);

/* lwc_string strings */
extern lwc_string *corestring_lwc_a;
extern lwc_string *corestring_lwc_about;
extern lwc_string *corestring_lwc_abscenter;
extern lwc_string *corestring_lwc_absmiddle;
extern lwc_string *corestring_lwc_align;
extern lwc_string *corestring_lwc_applet;
extern lwc_string *corestring_lwc_base;
extern lwc_string *corestring_lwc_baseline;
extern lwc_string *corestring_lwc_body;
extern lwc_string *corestring_lwc_bottom;
extern lwc_string *corestring_lwc_button;
extern lwc_string *corestring_lwc_caption;
extern lwc_string *corestring_lwc_center;
extern lwc_string *corestring_lwc_charset;
extern lwc_string *corestring_lwc_checkbox;
extern lwc_string *corestring_lwc_circle;
extern lwc_string *corestring_lwc_col;
extern lwc_string *corestring_lwc_data;
extern lwc_string *corestring_lwc_default;
extern lwc_string *corestring_lwc_div;
extern lwc_string *corestring_lwc_embed;
extern lwc_string *corestring_lwc_file;
extern lwc_string *corestring_lwc_filename;
extern lwc_string *corestring_lwc_font;
extern lwc_string *corestring_lwc_frame;
extern lwc_string *corestring_lwc_frameset;
extern lwc_string *corestring_lwc_h1;
extern lwc_string *corestring_lwc_h2;
extern lwc_string *corestring_lwc_h3;
extern lwc_string *corestring_lwc_h4;
extern lwc_string *corestring_lwc_h5;
extern lwc_string *corestring_lwc_h6;
extern lwc_string *corestring_lwc_head;
extern lwc_string *corestring_lwc_hidden;
extern lwc_string *corestring_lwc_hr;
extern lwc_string *corestring_lwc_html;
extern lwc_string *corestring_lwc_http;
extern lwc_string *corestring_lwc_https;
extern lwc_string *corestring_lwc_icon;
extern lwc_string *corestring_lwc_iframe;
extern lwc_string *corestring_lwc_image;
extern lwc_string *corestring_lwc_img;
extern lwc_string *corestring_lwc_input;
extern lwc_string *corestring_lwc_justify;
extern lwc_string *corestring_lwc_left;
extern lwc_string *corestring_lwc_li;
extern lwc_string *corestring_lwc_link;
extern lwc_string *corestring_lwc_meta;
extern lwc_string *corestring_lwc_middle;
extern lwc_string *corestring_lwc_multipart_form_data; /* multipart/form-data */
extern lwc_string *corestring_lwc_no;
extern lwc_string *corestring_lwc_noscript;
extern lwc_string *corestring_lwc_object;
extern lwc_string *corestring_lwc_optgroup;
extern lwc_string *corestring_lwc_option;
extern lwc_string *corestring_lwc_p;
extern lwc_string *corestring_lwc_param;
extern lwc_string *corestring_lwc_password;
extern lwc_string *corestring_lwc_poly;
extern lwc_string *corestring_lwc_polygon;
extern lwc_string *corestring_lwc_post;
extern lwc_string *corestring_lwc_radio;
extern lwc_string *corestring_lwc_rect;
extern lwc_string *corestring_lwc_rectangle;
extern lwc_string *corestring_lwc_refresh;
extern lwc_string *corestring_lwc_reset;
extern lwc_string *corestring_lwc_resource;
extern lwc_string *corestring_lwc_right;
extern lwc_string *corestring_lwc_search;
extern lwc_string *corestring_lwc_select;
extern lwc_string *corestring_lwc_shortcut_icon; /* shortcut icon */
extern lwc_string *corestring_lwc_src;
extern lwc_string *corestring_lwc_style;
extern lwc_string *corestring_lwc_submit;
extern lwc_string *corestring_lwc_table;
extern lwc_string *corestring_lwc_tbody;
extern lwc_string *corestring_lwc_td;
extern lwc_string *corestring_lwc_text;
extern lwc_string *corestring_lwc_textarea;
extern lwc_string *corestring_lwc_texttop;
extern lwc_string *corestring_lwc_text_css; /* text/css */
extern lwc_string *corestring_lwc_tfoot;
extern lwc_string *corestring_lwc_th;
extern lwc_string *corestring_lwc_thead;
extern lwc_string *corestring_lwc_title;
extern lwc_string *corestring_lwc_top;
extern lwc_string *corestring_lwc_tr;
extern lwc_string *corestring_lwc_ul;
extern lwc_string *corestring_lwc_url;
extern lwc_string *corestring_lwc_yes;
extern lwc_string *corestring_lwc__blank;
extern lwc_string *corestring_lwc__parent;
extern lwc_string *corestring_lwc__self;
extern lwc_string *corestring_lwc__top;

struct dom_string;

/* dom_string strings */
extern struct dom_string *corestring_dom_a;
extern struct dom_string *corestring_dom_alt;
extern struct dom_string *corestring_dom_abort;
extern struct dom_string *corestring_dom_afterprint;
extern struct dom_string *corestring_dom_align;
extern struct dom_string *corestring_dom_area;
extern struct dom_string *corestring_dom_async;
extern struct dom_string *corestring_dom_background;
extern struct dom_string *corestring_dom_beforeprint;
extern struct dom_string *corestring_dom_beforeunload;
extern struct dom_string *corestring_dom_bgcolor;
extern struct dom_string *corestring_dom_blur;
extern struct dom_string *corestring_dom_border;
extern struct dom_string *corestring_dom_bordercolor;
extern struct dom_string *corestring_dom_cancel;
extern struct dom_string *corestring_dom_canplay;
extern struct dom_string *corestring_dom_canplaythrough;
extern struct dom_string *corestring_dom_cellpadding;
extern struct dom_string *corestring_dom_cellspacing;
extern struct dom_string *corestring_dom_change;
extern struct dom_string *corestring_dom_charset;
extern struct dom_string *corestring_dom_class;
extern struct dom_string *corestring_dom_classid;
extern struct dom_string *corestring_dom_click;
extern struct dom_string *corestring_dom_close;
extern struct dom_string *corestring_dom_codebase;
extern struct dom_string *corestring_dom_color;
extern struct dom_string *corestring_dom_cols;
extern struct dom_string *corestring_dom_colspan;
extern struct dom_string *corestring_dom_content;
extern struct dom_string *corestring_dom_contextmenu;
extern struct dom_string *corestring_dom_coords;
extern struct dom_string *corestring_dom_cuechange;
extern struct dom_string *corestring_dom_data;
extern struct dom_string *corestring_dom_dblclick;
extern struct dom_string *corestring_dom_defer;
extern struct dom_string *corestring_dom_DOMAttrModified;
extern struct dom_string *corestring_dom_DOMNodeInserted;
extern struct dom_string *corestring_dom_DOMNodeInsertedIntoDocument;
extern struct dom_string *corestring_dom_DOMSubtreeModified;
extern struct dom_string *corestring_dom_drag;
extern struct dom_string *corestring_dom_dragend;
extern struct dom_string *corestring_dom_dragenter;
extern struct dom_string *corestring_dom_dragleave;
extern struct dom_string *corestring_dom_dragover;
extern struct dom_string *corestring_dom_dragstart;
extern struct dom_string *corestring_dom_drop;
extern struct dom_string *corestring_dom_durationchange;
extern struct dom_string *corestring_dom_emptied;
extern struct dom_string *corestring_dom_ended;
extern struct dom_string *corestring_dom_error;
extern struct dom_string *corestring_dom_focus;
extern struct dom_string *corestring_dom_frameborder;
extern struct dom_string *corestring_dom_hashchange;
extern struct dom_string *corestring_dom_height;
extern struct dom_string *corestring_dom_href;
extern struct dom_string *corestring_dom_hreflang;
extern struct dom_string *corestring_dom_hspace;
extern struct dom_string *corestring_dom_http_equiv; /* http-equiv */
extern struct dom_string *corestring_dom_id;
extern struct dom_string *corestring_dom_input;
extern struct dom_string *corestring_dom_invalid;
extern struct dom_string *corestring_dom_keydown;
extern struct dom_string *corestring_dom_keypress;
extern struct dom_string *corestring_dom_keyup;
extern struct dom_string *corestring_dom_link;
extern struct dom_string *corestring_dom_load;
extern struct dom_string *corestring_dom_loadeddata;
extern struct dom_string *corestring_dom_loadedmetadata;
extern struct dom_string *corestring_dom_loadstart;
extern struct dom_string *corestring_dom_map;
extern struct dom_string *corestring_dom_marginheight;
extern struct dom_string *corestring_dom_marginwidth;
extern struct dom_string *corestring_dom_media;
extern struct dom_string *corestring_dom_message;
extern struct dom_string *corestring_dom_mousedown;
extern struct dom_string *corestring_dom_mousemove;
extern struct dom_string *corestring_dom_mouseout;
extern struct dom_string *corestring_dom_mouseover;
extern struct dom_string *corestring_dom_mouseup;
extern struct dom_string *corestring_dom_mousewheel;
extern struct dom_string *corestring_dom_name;
extern struct dom_string *corestring_dom_nohref;
extern struct dom_string *corestring_dom_noresize;
extern struct dom_string *corestring_dom_offline;
extern struct dom_string *corestring_dom_online;
extern struct dom_string *corestring_dom_pagehide;
extern struct dom_string *corestring_dom_pageshow;
extern struct dom_string *corestring_dom_pause;
extern struct dom_string *corestring_dom_play;
extern struct dom_string *corestring_dom_playing;
extern struct dom_string *corestring_dom_popstate;
extern struct dom_string *corestring_dom_progress;
extern struct dom_string *corestring_dom_ratechange;
extern struct dom_string *corestring_dom_readystatechange;
extern struct dom_string *corestring_dom_rect;
extern struct dom_string *corestring_dom_rel;
extern struct dom_string *corestring_dom_reset;
extern struct dom_string *corestring_dom_resize;
extern struct dom_string *corestring_dom_rows;
extern struct dom_string *corestring_dom_rowspan;
extern struct dom_string *corestring_dom_scroll;
extern struct dom_string *corestring_dom_scrolling;
extern struct dom_string *corestring_dom_seeked;
extern struct dom_string *corestring_dom_seeking;
extern struct dom_string *corestring_dom_select;
extern struct dom_string *corestring_dom_selected;
extern struct dom_string *corestring_dom_shape;
extern struct dom_string *corestring_dom_show;
extern struct dom_string *corestring_dom_size;
extern struct dom_string *corestring_dom_sizes;
extern struct dom_string *corestring_dom_src;
extern struct dom_string *corestring_dom_stalled;
extern struct dom_string *corestring_dom_storage;
extern struct dom_string *corestring_dom_style;
extern struct dom_string *corestring_dom_submit;
extern struct dom_string *corestring_dom_suspend;
extern struct dom_string *corestring_dom_target;
extern struct dom_string *corestring_dom_text;
extern struct dom_string *corestring_dom_text_javascript; /* text/javascript */
extern struct dom_string *corestring_dom_timeupdate;
extern struct dom_string *corestring_dom_title;
extern struct dom_string *corestring_dom_type;
extern struct dom_string *corestring_dom_unload;
extern struct dom_string *corestring_dom_valign;
extern struct dom_string *corestring_dom_value;
extern struct dom_string *corestring_dom_vlink;
extern struct dom_string *corestring_dom_volumechange;
extern struct dom_string *corestring_dom_vspace;
extern struct dom_string *corestring_dom_waiting;
extern struct dom_string *corestring_dom_width;
/* DOM node types */
extern struct dom_string *corestring_dom_BUTTON;
extern struct dom_string *corestring_dom_INPUT;
extern struct dom_string *corestring_dom_SELECT;
extern struct dom_string *corestring_dom_TEXTAREA;
/* DOM input node types */
extern struct dom_string *corestring_dom_button;
/* extern struct dom_string *corestring_dom_submit; */
/* extern struct dom_string *corestring_dom_reset; */
extern struct dom_string *corestring_dom_image;
extern struct dom_string *corestring_dom_radio;
extern struct dom_string *corestring_dom_checkbox;
extern struct dom_string *corestring_dom_file;
extern struct dom_string *corestring_dom_on;
/* DOM userdata keys */
extern struct dom_string *corestring_dom___ns_key_box_node_data;
extern struct dom_string *corestring_dom___ns_key_libcss_node_data;
extern struct dom_string *corestring_dom___ns_key_file_name_node_data;
extern struct dom_string *corestring_dom___ns_key_image_coords_node_data;

/* URLs */
extern nsurl *corestring_nsurl_about_blank;

#endif

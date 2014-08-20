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
 * Useful interned string pointers (implementation).
 */

#include <dom/dom.h>

#include "utils/corestrings.h"
#include "utils/utils.h"

/* lwc_string strings */
lwc_string *corestring_lwc_a;
lwc_string *corestring_lwc_about;
lwc_string *corestring_lwc_abscenter;
lwc_string *corestring_lwc_absmiddle;
lwc_string *corestring_lwc_align;
lwc_string *corestring_lwc_applet;
lwc_string *corestring_lwc_base;
lwc_string *corestring_lwc_baseline;
lwc_string *corestring_lwc_body;
lwc_string *corestring_lwc_bottom;
lwc_string *corestring_lwc_button;
lwc_string *corestring_lwc_caption;
lwc_string *corestring_lwc_center;
lwc_string *corestring_lwc_charset;
lwc_string *corestring_lwc_checkbox;
lwc_string *corestring_lwc_circle;
lwc_string *corestring_lwc_col;
lwc_string *corestring_lwc_data;
lwc_string *corestring_lwc_default;
lwc_string *corestring_lwc_div;
lwc_string *corestring_lwc_embed;
lwc_string *corestring_lwc_file;
lwc_string *corestring_lwc_filename;
lwc_string *corestring_lwc_font;
lwc_string *corestring_lwc_frame;
lwc_string *corestring_lwc_frameset;
lwc_string *corestring_lwc_h1;
lwc_string *corestring_lwc_h2;
lwc_string *corestring_lwc_h3;
lwc_string *corestring_lwc_h4;
lwc_string *corestring_lwc_h5;
lwc_string *corestring_lwc_h6;
lwc_string *corestring_lwc_head;
lwc_string *corestring_lwc_hidden;
lwc_string *corestring_lwc_hr;
lwc_string *corestring_lwc_html;
lwc_string *corestring_lwc_http;
lwc_string *corestring_lwc_https;
lwc_string *corestring_lwc_icon;
lwc_string *corestring_lwc_iframe;
lwc_string *corestring_lwc_image;
lwc_string *corestring_lwc_img;
lwc_string *corestring_lwc_input;
lwc_string *corestring_lwc_justify;
lwc_string *corestring_lwc_left;
lwc_string *corestring_lwc_li;
lwc_string *corestring_lwc_link;
lwc_string *corestring_lwc_meta;
lwc_string *corestring_lwc_middle;
lwc_string *corestring_lwc_multipart_form_data;
lwc_string *corestring_lwc_no;
lwc_string *corestring_lwc_noscript;
lwc_string *corestring_lwc_object;
lwc_string *corestring_lwc_optgroup;
lwc_string *corestring_lwc_option;
lwc_string *corestring_lwc_p;
lwc_string *corestring_lwc_param;
lwc_string *corestring_lwc_password;
lwc_string *corestring_lwc_poly;
lwc_string *corestring_lwc_polygon;
lwc_string *corestring_lwc_post;
lwc_string *corestring_lwc_radio;
lwc_string *corestring_lwc_rect;
lwc_string *corestring_lwc_rectangle;
lwc_string *corestring_lwc_refresh;
lwc_string *corestring_lwc_reset;
lwc_string *corestring_lwc_resource;
lwc_string *corestring_lwc_right;
lwc_string *corestring_lwc_search;
lwc_string *corestring_lwc_select;
lwc_string *corestring_lwc_shortcut_icon;
lwc_string *corestring_lwc_src;
lwc_string *corestring_lwc_style;
lwc_string *corestring_lwc_submit;
lwc_string *corestring_lwc_table;
lwc_string *corestring_lwc_tbody;
lwc_string *corestring_lwc_td;
lwc_string *corestring_lwc_text;
lwc_string *corestring_lwc_textarea;
lwc_string *corestring_lwc_texttop;
lwc_string *corestring_lwc_text_css;
lwc_string *corestring_lwc_tfoot;
lwc_string *corestring_lwc_th;
lwc_string *corestring_lwc_thead;
lwc_string *corestring_lwc_title;
lwc_string *corestring_lwc_top;
lwc_string *corestring_lwc_tr;
lwc_string *corestring_lwc_ul;
lwc_string *corestring_lwc_url;
lwc_string *corestring_lwc_yes;
lwc_string *corestring_lwc__blank;
lwc_string *corestring_lwc__parent;
lwc_string *corestring_lwc__self;
lwc_string *corestring_lwc__top;

/* dom_string strings */
dom_string *corestring_dom_a;
dom_string *corestring_dom_alt;
dom_string *corestring_dom_abort;
dom_string *corestring_dom_afterprint;
dom_string *corestring_dom_align;
dom_string *corestring_dom_area;
dom_string *corestring_dom_async;
dom_string *corestring_dom_background;
dom_string *corestring_dom_beforeprint;
dom_string *corestring_dom_beforeunload;
dom_string *corestring_dom_bgcolor;
dom_string *corestring_dom_blur;
dom_string *corestring_dom_border;
dom_string *corestring_dom_bordercolor;
dom_string *corestring_dom_cancel;
dom_string *corestring_dom_canplay;
dom_string *corestring_dom_canplaythrough;
dom_string *corestring_dom_cellpadding;
dom_string *corestring_dom_cellspacing;
dom_string *corestring_dom_change;
dom_string *corestring_dom_charset;
dom_string *corestring_dom_class;
dom_string *corestring_dom_classid;
dom_string *corestring_dom_click;
dom_string *corestring_dom_close;
dom_string *corestring_dom_codebase;
dom_string *corestring_dom_color;
dom_string *corestring_dom_cols;
dom_string *corestring_dom_colspan;
dom_string *corestring_dom_content;
dom_string *corestring_dom_contextmenu;
dom_string *corestring_dom_coords;
dom_string *corestring_dom_cuechange;
dom_string *corestring_dom_data;
dom_string *corestring_dom_dblclick;
dom_string *corestring_dom_defer;
dom_string *corestring_dom_DOMAttrModified;
dom_string *corestring_dom_DOMNodeInserted;
dom_string *corestring_dom_DOMNodeInsertedIntoDocument;
dom_string *corestring_dom_DOMSubtreeModified;
dom_string *corestring_dom_drag;
dom_string *corestring_dom_dragend;
dom_string *corestring_dom_dragenter;
dom_string *corestring_dom_dragleave;
dom_string *corestring_dom_dragover;
dom_string *corestring_dom_dragstart;
dom_string *corestring_dom_drop;
dom_string *corestring_dom_durationchange;
dom_string *corestring_dom_emptied;
dom_string *corestring_dom_ended;
dom_string *corestring_dom_error;
dom_string *corestring_dom_focus;
dom_string *corestring_dom_frameborder;
dom_string *corestring_dom_hashchange;
dom_string *corestring_dom_height;
dom_string *corestring_dom_href;
dom_string *corestring_dom_hreflang;
dom_string *corestring_dom_hspace;
dom_string *corestring_dom_http_equiv;
dom_string *corestring_dom_id;
dom_string *corestring_dom_input;
dom_string *corestring_dom_invalid;
dom_string *corestring_dom_keydown;
dom_string *corestring_dom_keypress;
dom_string *corestring_dom_keyup;
dom_string *corestring_dom_link;
dom_string *corestring_dom_load;
dom_string *corestring_dom_loadeddata;
dom_string *corestring_dom_loadedmetadata;
dom_string *corestring_dom_loadstart;
dom_string *corestring_dom_map;
dom_string *corestring_dom_marginheight;
dom_string *corestring_dom_marginwidth;
dom_string *corestring_dom_media;
dom_string *corestring_dom_message;
dom_string *corestring_dom_mousedown;
dom_string *corestring_dom_mousemove;
dom_string *corestring_dom_mouseout;
dom_string *corestring_dom_mouseover;
dom_string *corestring_dom_mouseup;
dom_string *corestring_dom_mousewheel;
dom_string *corestring_dom_name;
dom_string *corestring_dom_nohref;
dom_string *corestring_dom_noresize;
dom_string *corestring_dom_offline;
dom_string *corestring_dom_online;
dom_string *corestring_dom_pagehide;
dom_string *corestring_dom_pageshow;
dom_string *corestring_dom_pause;
dom_string *corestring_dom_play;
dom_string *corestring_dom_playing;
dom_string *corestring_dom_popstate;
dom_string *corestring_dom_progress;
dom_string *corestring_dom_ratechange;
dom_string *corestring_dom_readystatechange;
dom_string *corestring_dom_rect;
dom_string *corestring_dom_rel;
dom_string *corestring_dom_reset;
dom_string *corestring_dom_resize;
dom_string *corestring_dom_rows;
dom_string *corestring_dom_rowspan;
dom_string *corestring_dom_scroll;
dom_string *corestring_dom_scrolling;
dom_string *corestring_dom_seeked;
dom_string *corestring_dom_seeking;
dom_string *corestring_dom_select;
dom_string *corestring_dom_selected;
dom_string *corestring_dom_shape;
dom_string *corestring_dom_show;
dom_string *corestring_dom_size;
dom_string *corestring_dom_sizes;
dom_string *corestring_dom_src;
dom_string *corestring_dom_stalled;
dom_string *corestring_dom_storage;
dom_string *corestring_dom_style;
dom_string *corestring_dom_submit;
dom_string *corestring_dom_suspend;
dom_string *corestring_dom_target;
dom_string *corestring_dom_text;
dom_string *corestring_dom_text_javascript;
dom_string *corestring_dom_timeupdate;
dom_string *corestring_dom_title;
dom_string *corestring_dom_type;
dom_string *corestring_dom_unload;
dom_string *corestring_dom_valign;
dom_string *corestring_dom_value;
dom_string *corestring_dom_vlink;
dom_string *corestring_dom_volumechange;
dom_string *corestring_dom_vspace;
dom_string *corestring_dom_waiting;
dom_string *corestring_dom_width;
dom_string *corestring_dom_BUTTON;
dom_string *corestring_dom_INPUT;
dom_string *corestring_dom_SELECT;
dom_string *corestring_dom_TEXTAREA;
dom_string *corestring_dom_button;
dom_string *corestring_dom_image;
dom_string *corestring_dom_radio;
dom_string *corestring_dom_checkbox;
dom_string *corestring_dom_file;
dom_string *corestring_dom_on;
dom_string *corestring_dom___ns_key_box_node_data;
dom_string *corestring_dom___ns_key_libcss_node_data;
dom_string *corestring_dom___ns_key_file_name_node_data;
dom_string *corestring_dom___ns_key_image_coords_node_data;

/* nsurl URLs */
nsurl *corestring_nsurl_about_blank;

/*
 * Free the core strings
 */
void corestrings_fini(void)
{
#define CSS_LWC_STRING_UNREF(NAME)					\
	do {								\
		if (corestring_lwc_##NAME != NULL) {			\
			lwc_string_unref(corestring_lwc_##NAME);	\
			corestring_lwc_##NAME = NULL;			\
		}							\
	} while (0)

	CSS_LWC_STRING_UNREF(a);
	CSS_LWC_STRING_UNREF(about);
	CSS_LWC_STRING_UNREF(abscenter);
	CSS_LWC_STRING_UNREF(absmiddle);
	CSS_LWC_STRING_UNREF(align);
	CSS_LWC_STRING_UNREF(applet);
	CSS_LWC_STRING_UNREF(base);
	CSS_LWC_STRING_UNREF(baseline);
	CSS_LWC_STRING_UNREF(body);
	CSS_LWC_STRING_UNREF(bottom);
	CSS_LWC_STRING_UNREF(button);
	CSS_LWC_STRING_UNREF(caption);
	CSS_LWC_STRING_UNREF(charset);
	CSS_LWC_STRING_UNREF(center);
	CSS_LWC_STRING_UNREF(checkbox);
	CSS_LWC_STRING_UNREF(circle);
	CSS_LWC_STRING_UNREF(col);
	CSS_LWC_STRING_UNREF(data);
	CSS_LWC_STRING_UNREF(default);
	CSS_LWC_STRING_UNREF(div);
	CSS_LWC_STRING_UNREF(embed);
	CSS_LWC_STRING_UNREF(file);
	CSS_LWC_STRING_UNREF(filename);
	CSS_LWC_STRING_UNREF(font);
	CSS_LWC_STRING_UNREF(frame);
	CSS_LWC_STRING_UNREF(frameset);
	CSS_LWC_STRING_UNREF(h1);
	CSS_LWC_STRING_UNREF(h2);
	CSS_LWC_STRING_UNREF(h3);
	CSS_LWC_STRING_UNREF(h4);
	CSS_LWC_STRING_UNREF(h5);
	CSS_LWC_STRING_UNREF(h6);
	CSS_LWC_STRING_UNREF(head);
	CSS_LWC_STRING_UNREF(hidden);
	CSS_LWC_STRING_UNREF(hr);
	CSS_LWC_STRING_UNREF(html);
	CSS_LWC_STRING_UNREF(http);
	CSS_LWC_STRING_UNREF(https);
	CSS_LWC_STRING_UNREF(icon);
	CSS_LWC_STRING_UNREF(iframe);
	CSS_LWC_STRING_UNREF(image);
	CSS_LWC_STRING_UNREF(img);
	CSS_LWC_STRING_UNREF(input);
	CSS_LWC_STRING_UNREF(justify);
	CSS_LWC_STRING_UNREF(left);
	CSS_LWC_STRING_UNREF(li);
	CSS_LWC_STRING_UNREF(link);
	CSS_LWC_STRING_UNREF(meta);
	CSS_LWC_STRING_UNREF(middle);
	CSS_LWC_STRING_UNREF(multipart_form_data);
	CSS_LWC_STRING_UNREF(no);
	CSS_LWC_STRING_UNREF(noscript);
	CSS_LWC_STRING_UNREF(object);
	CSS_LWC_STRING_UNREF(optgroup);
	CSS_LWC_STRING_UNREF(option);
	CSS_LWC_STRING_UNREF(p);
	CSS_LWC_STRING_UNREF(param);
	CSS_LWC_STRING_UNREF(password);
	CSS_LWC_STRING_UNREF(poly);
	CSS_LWC_STRING_UNREF(polygon);
	CSS_LWC_STRING_UNREF(post);
	CSS_LWC_STRING_UNREF(radio);
	CSS_LWC_STRING_UNREF(rect);
	CSS_LWC_STRING_UNREF(rectangle);
	CSS_LWC_STRING_UNREF(refresh);
	CSS_LWC_STRING_UNREF(reset);
	CSS_LWC_STRING_UNREF(resource);
	CSS_LWC_STRING_UNREF(right);
	CSS_LWC_STRING_UNREF(search);
	CSS_LWC_STRING_UNREF(select);
	CSS_LWC_STRING_UNREF(shortcut_icon);
	CSS_LWC_STRING_UNREF(src);
	CSS_LWC_STRING_UNREF(style);
	CSS_LWC_STRING_UNREF(submit);
	CSS_LWC_STRING_UNREF(table);
	CSS_LWC_STRING_UNREF(tbody);
	CSS_LWC_STRING_UNREF(td);
	CSS_LWC_STRING_UNREF(text);
	CSS_LWC_STRING_UNREF(textarea);
	CSS_LWC_STRING_UNREF(texttop);
	CSS_LWC_STRING_UNREF(text_css);
	CSS_LWC_STRING_UNREF(tfoot);
	CSS_LWC_STRING_UNREF(th);
	CSS_LWC_STRING_UNREF(thead);
	CSS_LWC_STRING_UNREF(title);
	CSS_LWC_STRING_UNREF(top);
	CSS_LWC_STRING_UNREF(tr);
	CSS_LWC_STRING_UNREF(ul);
	CSS_LWC_STRING_UNREF(url);
	CSS_LWC_STRING_UNREF(yes);
	CSS_LWC_STRING_UNREF(_blank);
	CSS_LWC_STRING_UNREF(_parent);
	CSS_LWC_STRING_UNREF(_self);
	CSS_LWC_STRING_UNREF(_top);

#undef CSS_LWC_STRING_UNREF

#define CSS_DOM_STRING_UNREF(NAME)					\
	do {								\
		if (corestring_dom_##NAME != NULL) {			\
			dom_string_unref(corestring_dom_##NAME);	\
			corestring_dom_##NAME = NULL;			\
		}							\
	} while (0)

	CSS_DOM_STRING_UNREF(a);
	CSS_DOM_STRING_UNREF(abort);
	CSS_DOM_STRING_UNREF(afterprint);
	CSS_DOM_STRING_UNREF(align);
	CSS_DOM_STRING_UNREF(alt);
	CSS_DOM_STRING_UNREF(area);
	CSS_DOM_STRING_UNREF(async);
	CSS_DOM_STRING_UNREF(background);
	CSS_DOM_STRING_UNREF(beforeprint);
	CSS_DOM_STRING_UNREF(beforeunload);
	CSS_DOM_STRING_UNREF(bgcolor);
	CSS_DOM_STRING_UNREF(blur);
	CSS_DOM_STRING_UNREF(border);
	CSS_DOM_STRING_UNREF(bordercolor);
	CSS_DOM_STRING_UNREF(cancel);
	CSS_DOM_STRING_UNREF(canplay);
	CSS_DOM_STRING_UNREF(canplaythrough);
	CSS_DOM_STRING_UNREF(cellpadding);
	CSS_DOM_STRING_UNREF(cellspacing);
	CSS_DOM_STRING_UNREF(change);
	CSS_DOM_STRING_UNREF(charset);
	CSS_DOM_STRING_UNREF(class);
	CSS_DOM_STRING_UNREF(classid);
	CSS_DOM_STRING_UNREF(click);
	CSS_DOM_STRING_UNREF(close);
	CSS_DOM_STRING_UNREF(codebase);
	CSS_DOM_STRING_UNREF(color);
	CSS_DOM_STRING_UNREF(cols);
	CSS_DOM_STRING_UNREF(colspan);
	CSS_DOM_STRING_UNREF(content);
	CSS_DOM_STRING_UNREF(contextmenu);
	CSS_DOM_STRING_UNREF(coords);
	CSS_DOM_STRING_UNREF(cuechange);
	CSS_DOM_STRING_UNREF(data);
	CSS_DOM_STRING_UNREF(dblclick);
	CSS_DOM_STRING_UNREF(defer);
	CSS_DOM_STRING_UNREF(DOMAttrModified);
	CSS_DOM_STRING_UNREF(DOMNodeInserted);
	CSS_DOM_STRING_UNREF(DOMNodeInsertedIntoDocument);
	CSS_DOM_STRING_UNREF(DOMSubtreeModified);
	CSS_DOM_STRING_UNREF(drag);
	CSS_DOM_STRING_UNREF(dragend);
	CSS_DOM_STRING_UNREF(dragenter);
	CSS_DOM_STRING_UNREF(dragleave);
	CSS_DOM_STRING_UNREF(dragover);
	CSS_DOM_STRING_UNREF(dragstart);
	CSS_DOM_STRING_UNREF(drop);
	CSS_DOM_STRING_UNREF(durationchange);
	CSS_DOM_STRING_UNREF(emptied);
	CSS_DOM_STRING_UNREF(ended);
	CSS_DOM_STRING_UNREF(error);
	CSS_DOM_STRING_UNREF(focus);
	CSS_DOM_STRING_UNREF(frameborder);
	CSS_DOM_STRING_UNREF(hashchange);
	CSS_DOM_STRING_UNREF(height);
	CSS_DOM_STRING_UNREF(href);
	CSS_DOM_STRING_UNREF(hreflang);
	CSS_DOM_STRING_UNREF(hspace);
	CSS_DOM_STRING_UNREF(http_equiv);
	CSS_DOM_STRING_UNREF(id);
	CSS_DOM_STRING_UNREF(input);
	CSS_DOM_STRING_UNREF(invalid);
	CSS_DOM_STRING_UNREF(keydown);
	CSS_DOM_STRING_UNREF(keypress);
	CSS_DOM_STRING_UNREF(keyup);
	CSS_DOM_STRING_UNREF(link);
	CSS_DOM_STRING_UNREF(load);
	CSS_DOM_STRING_UNREF(loadeddata);
	CSS_DOM_STRING_UNREF(loadedmetadata);
	CSS_DOM_STRING_UNREF(loadstart);
	CSS_DOM_STRING_UNREF(map);
	CSS_DOM_STRING_UNREF(marginheight);
	CSS_DOM_STRING_UNREF(marginwidth);
	CSS_DOM_STRING_UNREF(media);
	CSS_DOM_STRING_UNREF(message);
	CSS_DOM_STRING_UNREF(mousedown);
	CSS_DOM_STRING_UNREF(mousemove);
	CSS_DOM_STRING_UNREF(mouseout);
	CSS_DOM_STRING_UNREF(mouseover);
	CSS_DOM_STRING_UNREF(mouseup);
	CSS_DOM_STRING_UNREF(mousewheel);
	CSS_DOM_STRING_UNREF(name);
	CSS_DOM_STRING_UNREF(nohref);
	CSS_DOM_STRING_UNREF(noresize);
	CSS_DOM_STRING_UNREF(offline);
	CSS_DOM_STRING_UNREF(online);
	CSS_DOM_STRING_UNREF(pagehide);
	CSS_DOM_STRING_UNREF(pageshow);
	CSS_DOM_STRING_UNREF(pause);
	CSS_DOM_STRING_UNREF(play);
	CSS_DOM_STRING_UNREF(playing);
	CSS_DOM_STRING_UNREF(popstate);
	CSS_DOM_STRING_UNREF(progress);
	CSS_DOM_STRING_UNREF(ratechange);
	CSS_DOM_STRING_UNREF(readystatechange);
	CSS_DOM_STRING_UNREF(rect);
	CSS_DOM_STRING_UNREF(rel);
	CSS_DOM_STRING_UNREF(reset);
	CSS_DOM_STRING_UNREF(resize);
	CSS_DOM_STRING_UNREF(rows);
	CSS_DOM_STRING_UNREF(rowspan);
	CSS_DOM_STRING_UNREF(scroll);
	CSS_DOM_STRING_UNREF(scrolling);
	CSS_DOM_STRING_UNREF(seeked);
	CSS_DOM_STRING_UNREF(seeking);
	CSS_DOM_STRING_UNREF(select);
	CSS_DOM_STRING_UNREF(selected);
	CSS_DOM_STRING_UNREF(shape);
	CSS_DOM_STRING_UNREF(show);
	CSS_DOM_STRING_UNREF(size);
	CSS_DOM_STRING_UNREF(sizes);
	CSS_DOM_STRING_UNREF(src);
	CSS_DOM_STRING_UNREF(stalled);
	CSS_DOM_STRING_UNREF(storage);
	CSS_DOM_STRING_UNREF(style);
	CSS_DOM_STRING_UNREF(submit);
	CSS_DOM_STRING_UNREF(suspend);
	CSS_DOM_STRING_UNREF(target);
	CSS_DOM_STRING_UNREF(text);
	CSS_DOM_STRING_UNREF(text_javascript);
	CSS_DOM_STRING_UNREF(timeupdate);
	CSS_DOM_STRING_UNREF(title);
	CSS_DOM_STRING_UNREF(type);
	CSS_DOM_STRING_UNREF(unload);
	CSS_DOM_STRING_UNREF(valign);
	CSS_DOM_STRING_UNREF(value);
	CSS_DOM_STRING_UNREF(vlink);
	CSS_DOM_STRING_UNREF(volumechange);
	CSS_DOM_STRING_UNREF(vspace);
	CSS_DOM_STRING_UNREF(waiting);
	CSS_DOM_STRING_UNREF(width);
	/* DOM node names, not really CSS */
	CSS_DOM_STRING_UNREF(BUTTON);
	CSS_DOM_STRING_UNREF(INPUT);
	CSS_DOM_STRING_UNREF(SELECT);
	CSS_DOM_STRING_UNREF(TEXTAREA);
	/* DOM input types, not really CSS */
	CSS_DOM_STRING_UNREF(button);
	CSS_DOM_STRING_UNREF(image);
	CSS_DOM_STRING_UNREF(radio);
	CSS_DOM_STRING_UNREF(checkbox);
	CSS_DOM_STRING_UNREF(file);
	CSS_DOM_STRING_UNREF(on);
	/* DOM userdata keys, not really CSS */
	CSS_DOM_STRING_UNREF(__ns_key_box_node_data);
	CSS_DOM_STRING_UNREF(__ns_key_libcss_node_data);
	CSS_DOM_STRING_UNREF(__ns_key_file_name_node_data);
	CSS_DOM_STRING_UNREF(__ns_key_image_coords_node_data);
#undef CSS_DOM_STRING_UNREF

	/* nsurl URLs */
	if (corestring_nsurl_about_blank != NULL)
		nsurl_unref(corestring_nsurl_about_blank);
}


/*
 * Create the core strings
 */
nserror corestrings_init(void)
{
	lwc_error lerror;
	nserror error;
	dom_exception exc;

#define CSS_LWC_STRING_INTERN(NAME)					\
	do {								\
		lerror = lwc_intern_string(				\
				(const char *)#NAME,			\
				sizeof(#NAME) - 1,			\
				&corestring_lwc_##NAME );		\
		if ((lerror != lwc_error_ok) || 			\
				(corestring_lwc_##NAME == NULL)) {	\
			error = NSERROR_NOMEM;				\
			goto error;					\
		}							\
	} while(0)

	CSS_LWC_STRING_INTERN(a);
	CSS_LWC_STRING_INTERN(about);
	CSS_LWC_STRING_INTERN(abscenter);
	CSS_LWC_STRING_INTERN(absmiddle);
	CSS_LWC_STRING_INTERN(align);
	CSS_LWC_STRING_INTERN(applet);
	CSS_LWC_STRING_INTERN(base);
	CSS_LWC_STRING_INTERN(baseline);
	CSS_LWC_STRING_INTERN(body);
	CSS_LWC_STRING_INTERN(bottom);
	CSS_LWC_STRING_INTERN(button);
	CSS_LWC_STRING_INTERN(caption);
	CSS_LWC_STRING_INTERN(charset);
	CSS_LWC_STRING_INTERN(center);
	CSS_LWC_STRING_INTERN(checkbox);
	CSS_LWC_STRING_INTERN(circle);
	CSS_LWC_STRING_INTERN(col);
	CSS_LWC_STRING_INTERN(data);
	CSS_LWC_STRING_INTERN(default);
	CSS_LWC_STRING_INTERN(div);
	CSS_LWC_STRING_INTERN(embed);
	CSS_LWC_STRING_INTERN(file);
	CSS_LWC_STRING_INTERN(filename);
	CSS_LWC_STRING_INTERN(font);
	CSS_LWC_STRING_INTERN(frame);
	CSS_LWC_STRING_INTERN(frameset);
	CSS_LWC_STRING_INTERN(h1);
	CSS_LWC_STRING_INTERN(h2);
	CSS_LWC_STRING_INTERN(h3);
	CSS_LWC_STRING_INTERN(h4);
	CSS_LWC_STRING_INTERN(h5);
	CSS_LWC_STRING_INTERN(h6);
	CSS_LWC_STRING_INTERN(head);
	CSS_LWC_STRING_INTERN(hidden);
	CSS_LWC_STRING_INTERN(hr);
	CSS_LWC_STRING_INTERN(html);
	CSS_LWC_STRING_INTERN(http);
	CSS_LWC_STRING_INTERN(https);
	CSS_LWC_STRING_INTERN(icon);
	CSS_LWC_STRING_INTERN(iframe);
	CSS_LWC_STRING_INTERN(image);
	CSS_LWC_STRING_INTERN(img);
	CSS_LWC_STRING_INTERN(input);
	CSS_LWC_STRING_INTERN(justify);
	CSS_LWC_STRING_INTERN(left);
	CSS_LWC_STRING_INTERN(li);
	CSS_LWC_STRING_INTERN(link);
	CSS_LWC_STRING_INTERN(meta);
	CSS_LWC_STRING_INTERN(middle);
	CSS_LWC_STRING_INTERN(no);
	CSS_LWC_STRING_INTERN(noscript);
	CSS_LWC_STRING_INTERN(object);
	CSS_LWC_STRING_INTERN(optgroup);
	CSS_LWC_STRING_INTERN(option);
	CSS_LWC_STRING_INTERN(p);
	CSS_LWC_STRING_INTERN(param);
	CSS_LWC_STRING_INTERN(password);
	CSS_LWC_STRING_INTERN(poly);
	CSS_LWC_STRING_INTERN(polygon);
	CSS_LWC_STRING_INTERN(post);
	CSS_LWC_STRING_INTERN(radio);
	CSS_LWC_STRING_INTERN(rect);
	CSS_LWC_STRING_INTERN(rectangle);
	CSS_LWC_STRING_INTERN(refresh);
	CSS_LWC_STRING_INTERN(reset);
	CSS_LWC_STRING_INTERN(resource);
	CSS_LWC_STRING_INTERN(right);
	CSS_LWC_STRING_INTERN(search);
	CSS_LWC_STRING_INTERN(select);
	CSS_LWC_STRING_INTERN(src);
	CSS_LWC_STRING_INTERN(style);
	CSS_LWC_STRING_INTERN(submit);
	CSS_LWC_STRING_INTERN(table);
	CSS_LWC_STRING_INTERN(tbody);
	CSS_LWC_STRING_INTERN(td);
	CSS_LWC_STRING_INTERN(text);
	CSS_LWC_STRING_INTERN(textarea);
	CSS_LWC_STRING_INTERN(texttop);
	CSS_LWC_STRING_INTERN(tfoot);
	CSS_LWC_STRING_INTERN(th);
	CSS_LWC_STRING_INTERN(thead);
	CSS_LWC_STRING_INTERN(title);
	CSS_LWC_STRING_INTERN(top);
	CSS_LWC_STRING_INTERN(tr);
	CSS_LWC_STRING_INTERN(ul);
	CSS_LWC_STRING_INTERN(url);
	CSS_LWC_STRING_INTERN(yes);
	CSS_LWC_STRING_INTERN(_blank);
	CSS_LWC_STRING_INTERN(_parent);
	CSS_LWC_STRING_INTERN(_self);
	CSS_LWC_STRING_INTERN(_top);
#undef CSS_LWC_STRING_INTERN


	lerror = lwc_intern_string("multipart/form-data",
			SLEN("multipart/form-data"),
			&corestring_lwc_multipart_form_data);
	if ((lerror != lwc_error_ok) ||
			(corestring_lwc_multipart_form_data == NULL)) {
		error = NSERROR_NOMEM;
		goto error;
	}

	lerror = lwc_intern_string("shortcut icon", SLEN("shortcut icon"),
			&corestring_lwc_shortcut_icon);
	if ((lerror != lwc_error_ok) || (corestring_lwc_shortcut_icon == NULL)) {
		error = NSERROR_NOMEM;
		goto error;
	}

	lerror = lwc_intern_string("text/css", SLEN("text/css"),
			&corestring_lwc_text_css);
	if ((lerror != lwc_error_ok) || (corestring_lwc_text_css == NULL)) {
		error = NSERROR_NOMEM;
		goto error;
	}


#define CSS_DOM_STRING_INTERN(NAME)					\
	do {								\
		exc = dom_string_create_interned(			\
				(const uint8_t *)#NAME,			\
				sizeof(#NAME) - 1,			\
				&corestring_dom_##NAME );		\
		if ((exc != DOM_NO_ERR) || 				\
				(corestring_dom_##NAME == NULL)) {	\
			error = NSERROR_NOMEM;				\
			goto error;					\
		}							\
	} while(0)

	CSS_DOM_STRING_INTERN(a);
	CSS_DOM_STRING_INTERN(abort);
	CSS_DOM_STRING_INTERN(afterprint);
	CSS_DOM_STRING_INTERN(align);
	CSS_DOM_STRING_INTERN(alt);
	CSS_DOM_STRING_INTERN(area);
	CSS_DOM_STRING_INTERN(async);
	CSS_DOM_STRING_INTERN(background);
	CSS_DOM_STRING_INTERN(beforeprint);
	CSS_DOM_STRING_INTERN(beforeunload);
	CSS_DOM_STRING_INTERN(bgcolor);
	CSS_DOM_STRING_INTERN(blur);
	CSS_DOM_STRING_INTERN(border);
	CSS_DOM_STRING_INTERN(bordercolor);
	CSS_DOM_STRING_INTERN(cancel);
	CSS_DOM_STRING_INTERN(canplay);
	CSS_DOM_STRING_INTERN(canplaythrough);
	CSS_DOM_STRING_INTERN(cellpadding);
	CSS_DOM_STRING_INTERN(cellspacing);
	CSS_DOM_STRING_INTERN(change);
	CSS_DOM_STRING_INTERN(charset);
	CSS_DOM_STRING_INTERN(class);
	CSS_DOM_STRING_INTERN(classid);
	CSS_DOM_STRING_INTERN(click);
	CSS_DOM_STRING_INTERN(close);
	CSS_DOM_STRING_INTERN(codebase);
	CSS_DOM_STRING_INTERN(color);
	CSS_DOM_STRING_INTERN(cols);
	CSS_DOM_STRING_INTERN(colspan);
	CSS_DOM_STRING_INTERN(content);
	CSS_DOM_STRING_INTERN(contextmenu);
	CSS_DOM_STRING_INTERN(coords);
	CSS_DOM_STRING_INTERN(cuechange);
	CSS_DOM_STRING_INTERN(data);
	CSS_DOM_STRING_INTERN(dblclick);
	CSS_DOM_STRING_INTERN(defer);
	CSS_DOM_STRING_INTERN(DOMAttrModified);
	CSS_DOM_STRING_INTERN(DOMNodeInserted);
	CSS_DOM_STRING_INTERN(DOMNodeInsertedIntoDocument);
	CSS_DOM_STRING_INTERN(DOMSubtreeModified);
	CSS_DOM_STRING_INTERN(drag);
	CSS_DOM_STRING_INTERN(dragend);
	CSS_DOM_STRING_INTERN(dragenter);
	CSS_DOM_STRING_INTERN(dragleave);
	CSS_DOM_STRING_INTERN(dragover);
	CSS_DOM_STRING_INTERN(dragstart);
	CSS_DOM_STRING_INTERN(drop);
	CSS_DOM_STRING_INTERN(durationchange);
	CSS_DOM_STRING_INTERN(emptied);
	CSS_DOM_STRING_INTERN(ended);
	CSS_DOM_STRING_INTERN(error);
	CSS_DOM_STRING_INTERN(focus);
	CSS_DOM_STRING_INTERN(frameborder);
	CSS_DOM_STRING_INTERN(hashchange);
	CSS_DOM_STRING_INTERN(height);
	CSS_DOM_STRING_INTERN(href);
	CSS_DOM_STRING_INTERN(hreflang);
	CSS_DOM_STRING_INTERN(hspace);
	/* http-equiv: see below */
	CSS_DOM_STRING_INTERN(id);
	CSS_DOM_STRING_INTERN(input);
	CSS_DOM_STRING_INTERN(invalid);
	CSS_DOM_STRING_INTERN(keydown);
	CSS_DOM_STRING_INTERN(keypress);
	CSS_DOM_STRING_INTERN(keyup);
	CSS_DOM_STRING_INTERN(link);
	CSS_DOM_STRING_INTERN(load);
	CSS_DOM_STRING_INTERN(loadeddata);
	CSS_DOM_STRING_INTERN(loadedmetadata);
	CSS_DOM_STRING_INTERN(loadstart);
	CSS_DOM_STRING_INTERN(map);
	CSS_DOM_STRING_INTERN(marginheight);
	CSS_DOM_STRING_INTERN(marginwidth);
	CSS_DOM_STRING_INTERN(media);
	CSS_DOM_STRING_INTERN(message);
	CSS_DOM_STRING_INTERN(mousedown);
	CSS_DOM_STRING_INTERN(mousemove);
	CSS_DOM_STRING_INTERN(mouseout);
	CSS_DOM_STRING_INTERN(mouseover);
	CSS_DOM_STRING_INTERN(mouseup);
	CSS_DOM_STRING_INTERN(mousewheel);
	CSS_DOM_STRING_INTERN(name);
	CSS_DOM_STRING_INTERN(nohref);
	CSS_DOM_STRING_INTERN(noresize);
	CSS_DOM_STRING_INTERN(offline);
	CSS_DOM_STRING_INTERN(online);
	CSS_DOM_STRING_INTERN(pagehide);
	CSS_DOM_STRING_INTERN(pageshow);
	CSS_DOM_STRING_INTERN(pause);
	CSS_DOM_STRING_INTERN(play);
	CSS_DOM_STRING_INTERN(playing);
	CSS_DOM_STRING_INTERN(popstate);
	CSS_DOM_STRING_INTERN(progress);
	CSS_DOM_STRING_INTERN(ratechange);
	CSS_DOM_STRING_INTERN(readystatechange);
	CSS_DOM_STRING_INTERN(rect);
	CSS_DOM_STRING_INTERN(rel);
	CSS_DOM_STRING_INTERN(reset);
	CSS_DOM_STRING_INTERN(resize);
	CSS_DOM_STRING_INTERN(rows);
	CSS_DOM_STRING_INTERN(rowspan);
	CSS_DOM_STRING_INTERN(scroll);
	CSS_DOM_STRING_INTERN(scrolling);
	CSS_DOM_STRING_INTERN(seeked);
	CSS_DOM_STRING_INTERN(seeking);
	CSS_DOM_STRING_INTERN(select);
	CSS_DOM_STRING_INTERN(selected);
	CSS_DOM_STRING_INTERN(shape);
	CSS_DOM_STRING_INTERN(show);
	CSS_DOM_STRING_INTERN(size);
	CSS_DOM_STRING_INTERN(sizes);
	CSS_DOM_STRING_INTERN(src);
	CSS_DOM_STRING_INTERN(stalled);
	CSS_DOM_STRING_INTERN(storage);
	CSS_DOM_STRING_INTERN(style);
	CSS_DOM_STRING_INTERN(submit);
	CSS_DOM_STRING_INTERN(suspend);
	CSS_DOM_STRING_INTERN(target);
	CSS_DOM_STRING_INTERN(text);
	CSS_DOM_STRING_INTERN(timeupdate);
	CSS_DOM_STRING_INTERN(title);
	CSS_DOM_STRING_INTERN(type);
	CSS_DOM_STRING_INTERN(unload);
	CSS_DOM_STRING_INTERN(valign);
	CSS_DOM_STRING_INTERN(value);
	CSS_DOM_STRING_INTERN(vlink);
	CSS_DOM_STRING_INTERN(volumechange);
	CSS_DOM_STRING_INTERN(vspace);
	CSS_DOM_STRING_INTERN(waiting);
	CSS_DOM_STRING_INTERN(width);
	/* DOM node names, not really CSS */
	CSS_DOM_STRING_INTERN(BUTTON);
	CSS_DOM_STRING_INTERN(INPUT);
	CSS_DOM_STRING_INTERN(SELECT);
	CSS_DOM_STRING_INTERN(TEXTAREA);
	/* DOM input types, not really CSS */
	CSS_DOM_STRING_INTERN(button);
	CSS_DOM_STRING_INTERN(image);
	CSS_DOM_STRING_INTERN(radio);
	CSS_DOM_STRING_INTERN(checkbox);
	CSS_DOM_STRING_INTERN(file);
	CSS_DOM_STRING_INTERN(on);
	/* DOM userdata keys, not really CSS */
	CSS_DOM_STRING_INTERN(__ns_key_box_node_data);
	CSS_DOM_STRING_INTERN(__ns_key_libcss_node_data);
	CSS_DOM_STRING_INTERN(__ns_key_file_name_node_data);
	CSS_DOM_STRING_INTERN(__ns_key_image_coords_node_data);
#undef CSS_DOM_STRING_INTERN

	exc = dom_string_create_interned((const uint8_t *) "text/javascript",
			SLEN("text/javascript"),
			&corestring_dom_text_javascript);
	if ((exc != DOM_NO_ERR) || (corestring_dom_text_javascript == NULL)) {
		error = NSERROR_NOMEM;
		goto error;
	}

	exc = dom_string_create_interned((const uint8_t *) "http-equiv",
			SLEN("http-equiv"),
			&corestring_dom_http_equiv);
	if ((exc != DOM_NO_ERR) || (corestring_dom_http_equiv == NULL)) {
		error = NSERROR_NOMEM;
		goto error;
	}

	error = nsurl_create("about:blank", &corestring_nsurl_about_blank);
	if (error != NSERROR_OK) {
		goto error;
	}

	return NSERROR_OK;

error:
	corestrings_fini();

	return error;
}

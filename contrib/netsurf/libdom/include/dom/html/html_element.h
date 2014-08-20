/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef dom_html_element_h_
#define dom_html_element_h_

#include <dom/core/element.h>

typedef struct dom_html_element dom_html_element;

typedef struct dom_html_element_vtable {
        struct dom_element_vtable base;
        
        dom_exception (*dom_html_element_get_id)(struct dom_html_element *element,
                                                 dom_string **id);
        dom_exception (*dom_html_element_set_id)(struct dom_html_element *element,
                                                 dom_string *id);
        dom_exception (*dom_html_element_get_title)(struct dom_html_element *element,
                                                 dom_string **title);
        dom_exception (*dom_html_element_set_title)(struct dom_html_element *element,
                                                 dom_string *title);
        dom_exception (*dom_html_element_get_lang)(struct dom_html_element *element,
                                                 dom_string **lang);
        dom_exception (*dom_html_element_set_lang)(struct dom_html_element *element,
                                                 dom_string *lang);
        dom_exception (*dom_html_element_get_dir)(struct dom_html_element *element,
                                                 dom_string **dir);
        dom_exception (*dom_html_element_set_dir)(struct dom_html_element *element,
                                                 dom_string *dir);
        dom_exception (*dom_html_element_get_class_name)(struct dom_html_element *element,
                                                 dom_string **class_name);
        dom_exception (*dom_html_element_set_class_name)(struct dom_html_element *element,
                                                 dom_string *class_name);
} dom_html_element_vtable;

static inline dom_exception dom_html_element_get_id(struct dom_html_element *element,
                                                    dom_string **id)
{
        return ((dom_html_element_vtable *) ((dom_node *) element)->vtable)->
                dom_html_element_get_id(element, id);
}
#define dom_html_element_get_id(e, id) dom_html_element_get_id( \
		(dom_html_element *) (e), (id))

static inline dom_exception dom_html_element_set_id(struct dom_html_element *element,
                                                    dom_string *id)
{
        return ((dom_html_element_vtable *) ((dom_node *) element)->vtable)->
                dom_html_element_set_id(element, id);
}
#define dom_html_element_set_id(e, id) dom_html_element_set_id( \
		(dom_html_element *) (e), (id))

static inline dom_exception dom_html_element_get_title(struct dom_html_element *element,
                                                    dom_string **title)
{
        return ((dom_html_element_vtable *) ((dom_node *) element)->vtable)->
                dom_html_element_get_title(element, title);
}
#define dom_html_element_get_title(e, title) dom_html_element_get_title( \
		(dom_html_element *) (e), (title))

static inline dom_exception dom_html_element_set_title(struct dom_html_element *element,
                                                    dom_string *title)
{
        return ((dom_html_element_vtable *) ((dom_node *) element)->vtable)->
                dom_html_element_set_title(element, title);
}
#define dom_html_element_set_title(e, title) dom_html_element_set_title( \
		(dom_html_element *) (e), (title))

static inline dom_exception dom_html_element_get_lang(struct dom_html_element *element,
                                                    dom_string **lang)
{
        return ((dom_html_element_vtable *) ((dom_node *) element)->vtable)->
                dom_html_element_get_lang(element, lang);
}
#define dom_html_element_get_lang(e, lang) dom_html_element_get_lang( \
		(dom_html_element *) (e), (lang))

static inline dom_exception dom_html_element_set_lang(struct dom_html_element *element,
                                                    dom_string *lang)
{
        return ((dom_html_element_vtable *) ((dom_node *) element)->vtable)->
                dom_html_element_set_lang(element, lang);
}
#define dom_html_element_set_lang(e, lang) dom_html_element_set_lang( \
		(dom_html_element *) (e), (lang))

static inline dom_exception dom_html_element_get_dir(struct dom_html_element *element,
                                                    dom_string **dir)
{
        return ((dom_html_element_vtable *) ((dom_node *) element)->vtable)->
                dom_html_element_get_dir(element, dir);
}
#define dom_html_element_get_dir(e, dir) dom_html_element_get_dir( \
		(dom_html_element *) (e), (dir))

static inline dom_exception dom_html_element_set_dir(struct dom_html_element *element,
                                                    dom_string *dir)
{
        return ((dom_html_element_vtable *) ((dom_node *) element)->vtable)->
                dom_html_element_set_dir(element, dir);
}
#define dom_html_element_set_dir(e, dir) dom_html_element_set_dir( \
		(dom_html_element *) (e), (dir))

static inline dom_exception dom_html_element_get_class_name(struct dom_html_element *element,
                                                    dom_string **class_name)
{
        return ((dom_html_element_vtable *) ((dom_node *) element)->vtable)->
                dom_html_element_get_class_name(element, class_name);
}
#define dom_html_element_get_class_name(e, class_name) dom_html_element_get_class_name( \
		(dom_html_element *) (e), (class_name))

static inline dom_exception dom_html_element_set_class_name(struct dom_html_element *element,
                                                    dom_string *class_name)
{
        return ((dom_html_element_vtable *) ((dom_node *) element)->vtable)->
                dom_html_element_set_class_name(element, class_name);
}
#define dom_html_element_set_class_name(e, class_name) dom_html_element_set_class_name( \
		(dom_html_element *) (e), (class_name))

#endif


/*
 * This file is part of Libsvgtiny
 * Licensed under the MIT License,
 *                http://opensource.org/licenses/mit-license.php
 * Copyright 2008-2009 James Bursa <james@semichrome.net>
 * Copyright 2012 Daniel Silverstone <dsilvers@netsurf-browser.org>
 */

#include <assert.h>
#include <math.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dom/dom.h>
#include <dom/bindings/xml/xmlparser.h>

#include "svgtiny.h"
#include "svgtiny_internal.h"

#ifndef M_PI
#define M_PI		3.14159265358979323846
#endif

#define KAPPA		0.5522847498

static svgtiny_code svgtiny_parse_svg(dom_element *svg,
		struct svgtiny_parse_state state);
static svgtiny_code svgtiny_parse_path(dom_element *path,
		struct svgtiny_parse_state state);
static svgtiny_code svgtiny_parse_rect(dom_element *rect,
		struct svgtiny_parse_state state);
static svgtiny_code svgtiny_parse_circle(dom_element *circle,
		struct svgtiny_parse_state state);
static svgtiny_code svgtiny_parse_ellipse(dom_element *ellipse,
		struct svgtiny_parse_state state);
static svgtiny_code svgtiny_parse_line(dom_element *line,
		struct svgtiny_parse_state state);
static svgtiny_code svgtiny_parse_poly(dom_element *poly,
		struct svgtiny_parse_state state, bool polygon);
static svgtiny_code svgtiny_parse_text(dom_element *text,
		struct svgtiny_parse_state state);
static void svgtiny_parse_position_attributes(dom_element *node,
		const struct svgtiny_parse_state state,
		float *x, float *y, float *width, float *height);
static void svgtiny_parse_paint_attributes(dom_element *node,
		struct svgtiny_parse_state *state);
static void svgtiny_parse_font_attributes(dom_element *node,
		struct svgtiny_parse_state *state);
static void svgtiny_parse_transform_attributes(dom_element *node,
		struct svgtiny_parse_state *state);
static svgtiny_code svgtiny_add_path(float *p, unsigned int n,
		struct svgtiny_parse_state *state);
static void _svgtiny_parse_color(const char *s, svgtiny_colour *c,
		struct svgtiny_parse_state *state);

/**
 * Set the local externally-stored parts of a parse state.
 * Call this in functions that made a new state on the stack.
 * Doesn't make own copy of global state, such as the interned string list.
 */
static void svgtiny_setup_state_local(struct svgtiny_parse_state *state)
{
	if (state->gradient_x1 != NULL) {
		dom_string_ref(state->gradient_x1);
	}
	if (state->gradient_y1 != NULL) {
		dom_string_ref(state->gradient_y1);
	}
	if (state->gradient_x2 != NULL) {
		dom_string_ref(state->gradient_x2);
	}
	if (state->gradient_y2 != NULL) {
		dom_string_ref(state->gradient_y2);
	}
}

/**
 * Cleanup the local externally-stored parts of a parse state.
 * Call this in functions that made a new state on the stack.
 * Doesn't cleanup global state, such as the interned string list.
 */
static void svgtiny_cleanup_state_local(struct svgtiny_parse_state *state)
{
	if (state->gradient_x1 != NULL) {
		dom_string_unref(state->gradient_x1);
		state->gradient_x1 = NULL;
	}
	if (state->gradient_y1 != NULL) {
		dom_string_unref(state->gradient_y1);
		state->gradient_y1 = NULL;
	}
	if (state->gradient_x2 != NULL) {
		dom_string_unref(state->gradient_x2);
		state->gradient_x2 = NULL;
	}
	if (state->gradient_y2 != NULL) {
		dom_string_unref(state->gradient_y2);
		state->gradient_y2 = NULL;
	}
}


/**
 * Create a new svgtiny_diagram structure.
 */

struct svgtiny_diagram *svgtiny_create(void)
{
	struct svgtiny_diagram *diagram;

	diagram = calloc(sizeof(*diagram), 1);
	if (!diagram)
		return 0;

	return diagram;
	free(diagram);
	return NULL;
}

static void ignore_msg(uint32_t severity, void *ctx, const char *msg, ...)
{
	UNUSED(severity);
	UNUSED(ctx);
	UNUSED(msg);
}

/**
 * Parse a block of memory into a svgtiny_diagram.
 */

svgtiny_code svgtiny_parse(struct svgtiny_diagram *diagram,
		const char *buffer, size_t size, const char *url,
		int viewport_width, int viewport_height)
{
	dom_document *document;
	dom_exception exc;
	dom_xml_parser *parser;
	dom_xml_error err;
	dom_element *svg;
	dom_string *svg_name;
	lwc_string *svg_name_lwc;
	struct svgtiny_parse_state state;
	float x, y, width, height;
	svgtiny_code code;

	assert(diagram);
	assert(buffer);
	assert(url);

	UNUSED(url);

	state.gradient_x1 = NULL;
	state.gradient_y1 = NULL;
	state.gradient_x2 = NULL;
	state.gradient_y2 = NULL;

	parser = dom_xml_parser_create(NULL, NULL,
				       ignore_msg, NULL, &document);

	if (parser == NULL)
		return svgtiny_LIBDOM_ERROR;

	err = dom_xml_parser_parse_chunk(parser, (uint8_t *)buffer, size);
	if (err != DOM_XML_OK) {
		dom_node_unref(document);
		dom_xml_parser_destroy(parser);
		return svgtiny_LIBDOM_ERROR;
	}

	err = dom_xml_parser_completed(parser);
	if (err != DOM_XML_OK) {
		dom_node_unref(document);
		dom_xml_parser_destroy(parser);
		return svgtiny_LIBDOM_ERROR;
	}

	/* We're done parsing, drop the parser.
	 * We now own the document entirely.
	 */
	dom_xml_parser_destroy(parser);

	/* find root <svg> element */
	exc = dom_document_get_document_element(document, &svg);
	if (exc != DOM_NO_ERR) {
		dom_node_unref(document);
		return svgtiny_LIBDOM_ERROR;
	}
	exc = dom_node_get_node_name(svg, &svg_name);
	if (exc != DOM_NO_ERR) {
		dom_node_unref(svg);
		dom_node_unref(document);
		return svgtiny_LIBDOM_ERROR;
	}
	if (lwc_intern_string("svg", 3 /* SLEN("svg") */,
			      &svg_name_lwc) != lwc_error_ok) {
		dom_string_unref(svg_name);
		dom_node_unref(svg);
		dom_node_unref(document);
		return svgtiny_LIBDOM_ERROR;
	}
	if (!dom_string_caseless_lwc_isequal(svg_name, svg_name_lwc)) {
		lwc_string_unref(svg_name_lwc);
		dom_string_unref(svg_name);
		dom_node_unref(svg);
		dom_node_unref(document);
		return svgtiny_NOT_SVG;
	}

	lwc_string_unref(svg_name_lwc);
	dom_string_unref(svg_name);

	/* get graphic dimensions */
	memset(&state, 0, sizeof(state));
	state.diagram = diagram;
	state.document = document;
	state.viewport_width = viewport_width;
	state.viewport_height = viewport_height;

#define SVGTINY_STRING_ACTION2(s,n)					\
	if (dom_string_create_interned((const uint8_t *) #n,		\
				       strlen(#n), &state.interned_##s)	\
	    != DOM_NO_ERR) {						\
		code = svgtiny_LIBDOM_ERROR;				\
		goto cleanup;						\
	}
#include "svgtiny_strings.h"
#undef SVGTINY_STRING_ACTION2

	svgtiny_parse_position_attributes(svg, state, &x, &y, &width, &height);
	diagram->width = width;
	diagram->height = height;

	/* set up parsing state */
	state.viewport_width = width;
	state.viewport_height = height;
	state.ctm.a = 1; /*(float) viewport_width / (float) width;*/
	state.ctm.b = 0;
	state.ctm.c = 0;
	state.ctm.d = 1; /*(float) viewport_height / (float) height;*/
	state.ctm.e = 0; /*x;*/
	state.ctm.f = 0; /*y;*/
	/*state.style = css_base_style;
	state.style.font_size.value.length.value = option_font_size * 0.1;*/
	state.fill = 0x000000;
	state.stroke = svgtiny_TRANSPARENT;
	state.stroke_width = 1;
	state.linear_gradient_stop_count = 0;

	/* parse tree */
	code = svgtiny_parse_svg(svg, state);

	dom_node_unref(svg);
	dom_node_unref(document);

cleanup:
	svgtiny_cleanup_state_local(&state);
#define SVGTINY_STRING_ACTION2(s,n)			\
	if (state.interned_##s != NULL)			\
		dom_string_unref(state.interned_##s);
#include "svgtiny_strings.h"
#undef SVGTINY_STRING_ACTION2
	return code;
}


/**
 * Parse a <svg> or <g> element node.
 */

svgtiny_code svgtiny_parse_svg(dom_element *svg,
		struct svgtiny_parse_state state)
{
	float x, y, width, height;
	dom_string *view_box;
	dom_element *child;
	dom_exception exc;

	svgtiny_setup_state_local(&state);

	svgtiny_parse_position_attributes(svg, state, &x, &y, &width, &height);
	svgtiny_parse_paint_attributes(svg, &state);
	svgtiny_parse_font_attributes(svg, &state);

	exc = dom_element_get_attribute(svg, state.interned_viewBox,
					&view_box);
	if (exc != DOM_NO_ERR) {
		svgtiny_cleanup_state_local(&state);
		return svgtiny_LIBDOM_ERROR;
	}

	if (view_box) {
		char *s = strndup(dom_string_data(view_box),
				  dom_string_byte_length(view_box));
		float min_x, min_y, vwidth, vheight;
		if (sscanf(s, "%f,%f,%f,%f",
				&min_x, &min_y, &vwidth, &vheight) == 4 ||
				sscanf(s, "%f %f %f %f",
				&min_x, &min_y, &vwidth, &vheight) == 4) {
			state.ctm.a = (float) state.viewport_width / vwidth;
			state.ctm.d = (float) state.viewport_height / vheight;
			state.ctm.e += -min_x * state.ctm.a;
			state.ctm.f += -min_y * state.ctm.d;
		}
		free(s);
		dom_string_unref(view_box);
	}

	svgtiny_parse_transform_attributes(svg, &state);

	exc = dom_node_get_first_child(svg, (dom_node **) (void *) &child);
	if (exc != DOM_NO_ERR) {
		svgtiny_cleanup_state_local(&state);
		return svgtiny_LIBDOM_ERROR;
	}
	while (child != NULL) {
		dom_element *next;
		dom_node_type nodetype;
		svgtiny_code code = svgtiny_OK;

		exc = dom_node_get_node_type(child, &nodetype);
		if (exc != DOM_NO_ERR) {
			dom_node_unref(child);
			return svgtiny_LIBDOM_ERROR;
		}
		if (nodetype == DOM_ELEMENT_NODE) {
			dom_string *nodename;
			exc = dom_node_get_node_name(child, &nodename);
			if (exc != DOM_NO_ERR) {
				dom_node_unref(child);
				svgtiny_cleanup_state_local(&state);
				return svgtiny_LIBDOM_ERROR;
			}
			if (dom_string_caseless_isequal(state.interned_svg,
							nodename))
				code = svgtiny_parse_svg(child, state);
			else if (dom_string_caseless_isequal(state.interned_g,
							     nodename))
				code = svgtiny_parse_svg(child, state);
			else if (dom_string_caseless_isequal(state.interned_a,
							     nodename))
				code = svgtiny_parse_svg(child, state);
			else if (dom_string_caseless_isequal(state.interned_path,
							     nodename))
				code = svgtiny_parse_path(child, state);
			else if (dom_string_caseless_isequal(state.interned_rect,
							     nodename))
				code = svgtiny_parse_rect(child, state);
			else if (dom_string_caseless_isequal(state.interned_circle,
							     nodename))
				code = svgtiny_parse_circle(child, state);
			else if (dom_string_caseless_isequal(state.interned_ellipse,
							     nodename))
				code = svgtiny_parse_ellipse(child, state);
			else if (dom_string_caseless_isequal(state.interned_line,
							     nodename))
				code = svgtiny_parse_line(child, state);
			else if (dom_string_caseless_isequal(state.interned_polyline,
							     nodename))
				code = svgtiny_parse_poly(child, state, false);
			else if (dom_string_caseless_isequal(state.interned_polygon,
							     nodename))
				code = svgtiny_parse_poly(child, state, true);
			else if (dom_string_caseless_isequal(state.interned_text,
							     nodename))
				code = svgtiny_parse_text(child, state);
			dom_string_unref(nodename);
		}
		if (code != svgtiny_OK) {
			dom_node_unref(child);
			svgtiny_cleanup_state_local(&state);
			return code;
		}
		exc = dom_node_get_next_sibling(child,
						(dom_node **) (void *) &next);
		dom_node_unref(child);
		if (exc != DOM_NO_ERR) {
			svgtiny_cleanup_state_local(&state);
			return svgtiny_LIBDOM_ERROR;
		}
		child = next;
	}

	svgtiny_cleanup_state_local(&state);
	return svgtiny_OK;
}



/**
 * Parse a <path> element node.
 *
 * http://www.w3.org/TR/SVG11/paths#PathElement
 */

svgtiny_code svgtiny_parse_path(dom_element *path,
		struct svgtiny_parse_state state)
{
	svgtiny_code err;
	dom_string *path_d_str;
	dom_exception exc;
	char *s, *path_d;
	float *p;
	unsigned int i;
	float last_x = 0, last_y = 0;
	float last_cubic_x = 0, last_cubic_y = 0;
	float last_quad_x = 0, last_quad_y = 0;

	svgtiny_setup_state_local(&state);

	svgtiny_parse_paint_attributes(path, &state);
	svgtiny_parse_transform_attributes(path, &state);

	/* read d attribute */
	exc = dom_element_get_attribute(path, state.interned_d, &path_d_str);
	if (exc != DOM_NO_ERR) {
		state.diagram->error_line = -1; /* path->line; */
		state.diagram->error_message = "path: error retrieving d attribute";
		svgtiny_cleanup_state_local(&state);
		return svgtiny_SVG_ERROR;
	}

	if (path_d_str == NULL) {
		state.diagram->error_line = -1; /* path->line; */
		state.diagram->error_message = "path: missing d attribute";
		svgtiny_cleanup_state_local(&state);
		return svgtiny_SVG_ERROR;
	}

	s = path_d = strndup(dom_string_data(path_d_str),
			     dom_string_byte_length(path_d_str));
	dom_string_unref(path_d_str);
	if (s == NULL) {
		svgtiny_cleanup_state_local(&state);
		return svgtiny_OUT_OF_MEMORY;
	}
	/* allocate space for path: it will never have more elements than d */
	p = malloc(sizeof p[0] * strlen(s));
	if (!p) {
		free(path_d);
		svgtiny_cleanup_state_local(&state);
		return svgtiny_OUT_OF_MEMORY;
	}

	/* parse d and build path */
	for (i = 0; s[i]; i++)
		if (s[i] == ',')
			s[i] = ' ';
	i = 0;
	while (*s) {
		char command[2];
		int plot_command;
		float x, y, x1, y1, x2, y2, rx, ry, rotation, large_arc, sweep;
		int n;

		/* moveto (M, m), lineto (L, l) (2 arguments) */
		if (sscanf(s, " %1[MmLl] %f %f %n", command, &x, &y, &n) == 3) {
			/*LOG(("moveto or lineto"));*/
			if (*command == 'M' || *command == 'm')
				plot_command = svgtiny_PATH_MOVE;
			else
				plot_command = svgtiny_PATH_LINE;
			do {
				p[i++] = plot_command;
				if ('a' <= *command) {
					x += last_x;
					y += last_y;
				}
				p[i++] = last_cubic_x = last_quad_x = last_x
						= x;
				p[i++] = last_cubic_y = last_quad_y = last_y
						= y;
				s += n;
				plot_command = svgtiny_PATH_LINE;
			} while (sscanf(s, "%f %f %n", &x, &y, &n) == 2);

		/* closepath (Z, z) (no arguments) */
		} else if (sscanf(s, " %1[Zz] %n", command, &n) == 1) {
			/*LOG(("closepath"));*/
			p[i++] = svgtiny_PATH_CLOSE;
			s += n;

		/* horizontal lineto (H, h) (1 argument) */
		} else if (sscanf(s, " %1[Hh] %f %n", command, &x, &n) == 2) {
			/*LOG(("horizontal lineto"));*/
			do {
				p[i++] = svgtiny_PATH_LINE;
				if (*command == 'h')
					x += last_x;
				p[i++] = last_cubic_x = last_quad_x = last_x
						= x;
				p[i++] = last_cubic_y = last_quad_y = last_y;
				s += n;
			} while (sscanf(s, "%f %n", &x, &n) == 1);

		/* vertical lineto (V, v) (1 argument) */
		} else if (sscanf(s, " %1[Vv] %f %n", command, &y, &n) == 2) {
			/*LOG(("vertical lineto"));*/
			do {
				p[i++] = svgtiny_PATH_LINE;
				if (*command == 'v')
					y += last_y;
				p[i++] = last_cubic_x = last_quad_x = last_x;
				p[i++] = last_cubic_y = last_quad_y = last_y
						= y;
				s += n;
			} while (sscanf(s, "%f %n", &x, &n) == 1);

		/* curveto (C, c) (6 arguments) */
		} else if (sscanf(s, " %1[Cc] %f %f %f %f %f %f %n", command,
				&x1, &y1, &x2, &y2, &x, &y, &n) == 7) {
			/*LOG(("curveto"));*/
			do {
				p[i++] = svgtiny_PATH_BEZIER;
				if (*command == 'c') {
					x1 += last_x;
					y1 += last_y;
					x2 += last_x;
					y2 += last_y;
					x += last_x;
					y += last_y;
				}
				p[i++] = x1;
				p[i++] = y1;
				p[i++] = last_cubic_x = x2;
				p[i++] = last_cubic_y = y2;
				p[i++] = last_quad_x = last_x = x;
				p[i++] = last_quad_y = last_y = y;
				s += n;
			} while (sscanf(s, "%f %f %f %f %f %f %n",
					&x1, &y1, &x2, &y2, &x, &y, &n) == 6);

		/* shorthand/smooth curveto (S, s) (4 arguments) */
		} else if (sscanf(s, " %1[Ss] %f %f %f %f %n", command,
				&x2, &y2, &x, &y, &n) == 5) {
			/*LOG(("shorthand/smooth curveto"));*/
			do {
				p[i++] = svgtiny_PATH_BEZIER;
				x1 = last_x + (last_x - last_cubic_x);
				y1 = last_y + (last_y - last_cubic_y);
				if (*command == 's') {
					x2 += last_x;
					y2 += last_y;
					x += last_x;
					y += last_y;
				}
				p[i++] = x1;
				p[i++] = y1;
				p[i++] = last_cubic_x = x2;
				p[i++] = last_cubic_y = y2;
				p[i++] = last_quad_x = last_x = x;
				p[i++] = last_quad_y = last_y = y;
				s += n;
			} while (sscanf(s, "%f %f %f %f %n",
					&x2, &y2, &x, &y, &n) == 4);

		/* quadratic Bezier curveto (Q, q) (4 arguments) */
		} else if (sscanf(s, " %1[Qq] %f %f %f %f %n", command,
				&x1, &y1, &x, &y, &n) == 5) {
			/*LOG(("quadratic Bezier curveto"));*/
			do {
				p[i++] = svgtiny_PATH_BEZIER;
				last_quad_x = x1;
				last_quad_y = y1;
				if (*command == 'q') {
					x1 += last_x;
					y1 += last_y;
					x += last_x;
					y += last_y;
				}
				p[i++] = 1./3 * last_x + 2./3 * x1;
				p[i++] = 1./3 * last_y + 2./3 * y1;
				p[i++] = 2./3 * x1 + 1./3 * x;
				p[i++] = 2./3 * y1 + 1./3 * y;
				p[i++] = last_cubic_x = last_x = x;
				p[i++] = last_cubic_y = last_y = y;
				s += n;
			} while (sscanf(s, "%f %f %f %f %n",
					&x1, &y1, &x, &y, &n) == 4);

		/* shorthand/smooth quadratic Bezier curveto (T, t)
		   (2 arguments) */
		} else if (sscanf(s, " %1[Tt] %f %f %n", command,
				&x, &y, &n) == 3) {
			/*LOG(("shorthand/smooth quadratic Bezier curveto"));*/
			do {
				p[i++] = svgtiny_PATH_BEZIER;
				x1 = last_x + (last_x - last_quad_x);
				y1 = last_y + (last_y - last_quad_y);
				last_quad_x = x1;
				last_quad_y = y1;
				if (*command == 't') {
					x1 += last_x;
					y1 += last_y;
					x += last_x;
					y += last_y;
				}
				p[i++] = 1./3 * last_x + 2./3 * x1;
				p[i++] = 1./3 * last_y + 2./3 * y1;
				p[i++] = 2./3 * x1 + 1./3 * x;
				p[i++] = 2./3 * y1 + 1./3 * y;
				p[i++] = last_cubic_x = last_x = x;
				p[i++] = last_cubic_y = last_y = y;
				s += n;
			} while (sscanf(s, "%f %f %n",
					&x, &y, &n) == 2);

		/* elliptical arc (A, a) (7 arguments) */
		} else if (sscanf(s, " %1[Aa] %f %f %f %f %f %f %f %n", command,
				&rx, &ry, &rotation, &large_arc, &sweep,
				&x, &y, &n) == 8) {
			do {
				p[i++] = svgtiny_PATH_LINE;
				if (*command == 'a') {
					x += last_x;
					y += last_y;
				}
				p[i++] = last_cubic_x = last_quad_x = last_x
						= x;
				p[i++] = last_cubic_y = last_quad_y = last_y
						= y;
				s += n;
			} while (sscanf(s, "%f %f %f %f %f %f %f %n",
				&rx, &ry, &rotation, &large_arc, &sweep,
				&x, &y, &n) == 7);

		} else {
			fprintf(stderr, "parse failed at \"%s\"\n", s);
			break;
		}
	}

	free(path_d);

	if (i <= 4) {
		/* no real segments in path */
		free(p);
		svgtiny_cleanup_state_local(&state);
		return svgtiny_OK;
	}

	err = svgtiny_add_path(p, i, &state);

	svgtiny_cleanup_state_local(&state);

	return err;
}


/**
 * Parse a <rect> element node.
 *
 * http://www.w3.org/TR/SVG11/shapes#RectElement
 */

svgtiny_code svgtiny_parse_rect(dom_element *rect,
		struct svgtiny_parse_state state)
{
	svgtiny_code err;
	float x, y, width, height;
	float *p;

	svgtiny_setup_state_local(&state);

	svgtiny_parse_position_attributes(rect, state,
			&x, &y, &width, &height);
	svgtiny_parse_paint_attributes(rect, &state);
	svgtiny_parse_transform_attributes(rect, &state);

	p = malloc(13 * sizeof p[0]);
	if (!p) {
		svgtiny_cleanup_state_local(&state);
		return svgtiny_OUT_OF_MEMORY;
	}

	p[0] = svgtiny_PATH_MOVE;
	p[1] = x;
	p[2] = y;
	p[3] = svgtiny_PATH_LINE;
	p[4] = x + width;
	p[5] = y;
	p[6] = svgtiny_PATH_LINE;
	p[7] = x + width;
	p[8] = y + height;
	p[9] = svgtiny_PATH_LINE;
	p[10] = x;
	p[11] = y + height;
	p[12] = svgtiny_PATH_CLOSE;

	err = svgtiny_add_path(p, 13, &state);

	svgtiny_cleanup_state_local(&state);

	return err;
}


/**
 * Parse a <circle> element node.
 */

svgtiny_code svgtiny_parse_circle(dom_element *circle,
		struct svgtiny_parse_state state)
{
	svgtiny_code err;
	float x = 0, y = 0, r = -1;
	float *p;
	dom_string *attr;
	dom_exception exc;

	svgtiny_setup_state_local(&state);

	exc = dom_element_get_attribute(circle, state.interned_cx, &attr);
	if (exc != DOM_NO_ERR) {
		svgtiny_cleanup_state_local(&state);
		return svgtiny_LIBDOM_ERROR;
	}
	if (attr != NULL) {
		x = svgtiny_parse_length(attr, state.viewport_width, state);
	}
	dom_string_unref(attr);

	exc = dom_element_get_attribute(circle, state.interned_cy, &attr);
	if (exc != DOM_NO_ERR) {
		svgtiny_cleanup_state_local(&state);
		return svgtiny_LIBDOM_ERROR;
	}
	if (attr != NULL) {
		y = svgtiny_parse_length(attr, state.viewport_height, state);
	}
	dom_string_unref(attr);

	exc = dom_element_get_attribute(circle, state.interned_r, &attr);
	if (exc != DOM_NO_ERR) {
		svgtiny_cleanup_state_local(&state);
		return svgtiny_LIBDOM_ERROR;
	}
	if (attr != NULL) {
		r = svgtiny_parse_length(attr, state.viewport_width, state);
	}
	dom_string_unref(attr);

	svgtiny_parse_paint_attributes(circle, &state);
	svgtiny_parse_transform_attributes(circle, &state);

	if (r < 0) {
		state.diagram->error_line = -1; /* circle->line; */
		state.diagram->error_message = "circle: r missing or negative";
		svgtiny_cleanup_state_local(&state);
		return svgtiny_SVG_ERROR;
	}
	if (r == 0) {
		svgtiny_cleanup_state_local(&state);
		return svgtiny_OK;
	}

	p = malloc(32 * sizeof p[0]);
	if (!p) {
		svgtiny_cleanup_state_local(&state);
		return svgtiny_OUT_OF_MEMORY;
	}

	p[0] = svgtiny_PATH_MOVE;
	p[1] = x + r;
	p[2] = y;
	p[3] = svgtiny_PATH_BEZIER;
	p[4] = x + r;
	p[5] = y + r * KAPPA;
	p[6] = x + r * KAPPA;
	p[7] = y + r;
	p[8] = x;
	p[9] = y + r;
	p[10] = svgtiny_PATH_BEZIER;
	p[11] = x - r * KAPPA;
	p[12] = y + r;
	p[13] = x - r;
	p[14] = y + r * KAPPA;
	p[15] = x - r;
	p[16] = y;
	p[17] = svgtiny_PATH_BEZIER;
	p[18] = x - r;
	p[19] = y - r * KAPPA;
	p[20] = x - r * KAPPA;
	p[21] = y - r;
	p[22] = x;
	p[23] = y - r;
	p[24] = svgtiny_PATH_BEZIER;
	p[25] = x + r * KAPPA;
	p[26] = y - r;
	p[27] = x + r;
	p[28] = y - r * KAPPA;
	p[29] = x + r;
	p[30] = y;
	p[31] = svgtiny_PATH_CLOSE;

	err = svgtiny_add_path(p, 32, &state);

	svgtiny_cleanup_state_local(&state);
	
	return err;
}


/**
 * Parse an <ellipse> element node.
 */

svgtiny_code svgtiny_parse_ellipse(dom_element *ellipse,
		struct svgtiny_parse_state state)
{
	svgtiny_code err;
	float x = 0, y = 0, rx = -1, ry = -1;
	float *p;
	dom_string *attr;
	dom_exception exc;

	svgtiny_setup_state_local(&state);

	exc = dom_element_get_attribute(ellipse, state.interned_cx, &attr);
	if (exc != DOM_NO_ERR) {
		svgtiny_cleanup_state_local(&state);
		return svgtiny_LIBDOM_ERROR;
	}
	if (attr != NULL) {
		x = svgtiny_parse_length(attr, state.viewport_width, state);
	}
	dom_string_unref(attr);

	exc = dom_element_get_attribute(ellipse, state.interned_cy, &attr);
	if (exc != DOM_NO_ERR) {
		svgtiny_cleanup_state_local(&state);
		return svgtiny_LIBDOM_ERROR;
	}
	if (attr != NULL) {
		y = svgtiny_parse_length(attr, state.viewport_height, state);
	}
	dom_string_unref(attr);

	exc = dom_element_get_attribute(ellipse, state.interned_rx, &attr);
	if (exc != DOM_NO_ERR) {
		svgtiny_cleanup_state_local(&state);
		return svgtiny_LIBDOM_ERROR;
	}
	if (attr != NULL) {
		rx = svgtiny_parse_length(attr, state.viewport_width, state);
	}
	dom_string_unref(attr);

	exc = dom_element_get_attribute(ellipse, state.interned_ry, &attr);
	if (exc != DOM_NO_ERR) {
		svgtiny_cleanup_state_local(&state);
		return svgtiny_LIBDOM_ERROR;
	}
	if (attr != NULL) {
		ry = svgtiny_parse_length(attr, state.viewport_width, state);
	}
	dom_string_unref(attr);

	svgtiny_parse_paint_attributes(ellipse, &state);
	svgtiny_parse_transform_attributes(ellipse, &state);

	if (rx < 0 || ry < 0) {
		state.diagram->error_line = -1; /* ellipse->line; */
		state.diagram->error_message = "ellipse: rx or ry missing "
				"or negative";
		svgtiny_cleanup_state_local(&state);
		return svgtiny_SVG_ERROR;
	}
	if (rx == 0 || ry == 0) {
		svgtiny_cleanup_state_local(&state);
		return svgtiny_OK;
	}

	p = malloc(32 * sizeof p[0]);
	if (!p) {
		svgtiny_cleanup_state_local(&state);
		return svgtiny_OUT_OF_MEMORY;
	}

	p[0] = svgtiny_PATH_MOVE;
	p[1] = x + rx;
	p[2] = y;
	p[3] = svgtiny_PATH_BEZIER;
	p[4] = x + rx;
	p[5] = y + ry * KAPPA;
	p[6] = x + rx * KAPPA;
	p[7] = y + ry;
	p[8] = x;
	p[9] = y + ry;
	p[10] = svgtiny_PATH_BEZIER;
	p[11] = x - rx * KAPPA;
	p[12] = y + ry;
	p[13] = x - rx;
	p[14] = y + ry * KAPPA;
	p[15] = x - rx;
	p[16] = y;
	p[17] = svgtiny_PATH_BEZIER;
	p[18] = x - rx;
	p[19] = y - ry * KAPPA;
	p[20] = x - rx * KAPPA;
	p[21] = y - ry;
	p[22] = x;
	p[23] = y - ry;
	p[24] = svgtiny_PATH_BEZIER;
	p[25] = x + rx * KAPPA;
	p[26] = y - ry;
	p[27] = x + rx;
	p[28] = y - ry * KAPPA;
	p[29] = x + rx;
	p[30] = y;
	p[31] = svgtiny_PATH_CLOSE;
	
	err = svgtiny_add_path(p, 32, &state);

	svgtiny_cleanup_state_local(&state);

	return err;
}


/**
 * Parse a <line> element node.
 */

svgtiny_code svgtiny_parse_line(dom_element *line,
		struct svgtiny_parse_state state)
{
	svgtiny_code err;
	float x1 = 0, y1 = 0, x2 = 0, y2 = 0;
	float *p;
	dom_string *attr;
	dom_exception exc;

	svgtiny_setup_state_local(&state);

	exc = dom_element_get_attribute(line, state.interned_x1, &attr);
	if (exc != DOM_NO_ERR) {
		svgtiny_cleanup_state_local(&state);
		return svgtiny_LIBDOM_ERROR;
	}
	if (attr != NULL) {
		x1 = svgtiny_parse_length(attr, state.viewport_width, state);
	}
	dom_string_unref(attr);

	exc = dom_element_get_attribute(line, state.interned_y1, &attr);
	if (exc != DOM_NO_ERR) {
		svgtiny_cleanup_state_local(&state);
		return svgtiny_LIBDOM_ERROR;
	}
	if (attr != NULL) {
		y1 = svgtiny_parse_length(attr, state.viewport_height, state);
	}
	dom_string_unref(attr);

	exc = dom_element_get_attribute(line, state.interned_x2, &attr);
	if (exc != DOM_NO_ERR) {
		svgtiny_cleanup_state_local(&state);
		return svgtiny_LIBDOM_ERROR;
	}
	if (attr != NULL) {
		x2 = svgtiny_parse_length(attr, state.viewport_width, state);
	}
	dom_string_unref(attr);

	exc = dom_element_get_attribute(line, state.interned_y2, &attr);
	if (exc != DOM_NO_ERR) {
		svgtiny_cleanup_state_local(&state);
		return svgtiny_LIBDOM_ERROR;
	}
	if (attr != NULL) {
		y2 = svgtiny_parse_length(attr, state.viewport_height, state);
	}
	dom_string_unref(attr);

	svgtiny_parse_paint_attributes(line, &state);
	svgtiny_parse_transform_attributes(line, &state);

	p = malloc(7 * sizeof p[0]);
	if (!p) {
		svgtiny_cleanup_state_local(&state);
		return svgtiny_OUT_OF_MEMORY;
	}

	p[0] = svgtiny_PATH_MOVE;
	p[1] = x1;
	p[2] = y1;
	p[3] = svgtiny_PATH_LINE;
	p[4] = x2;
	p[5] = y2;
	p[6] = svgtiny_PATH_CLOSE;

	err = svgtiny_add_path(p, 7, &state);

	svgtiny_cleanup_state_local(&state);

	return err;
}


/**
 * Parse a <polyline> or <polygon> element node.
 *
 * http://www.w3.org/TR/SVG11/shapes#PolylineElement
 * http://www.w3.org/TR/SVG11/shapes#PolygonElement
 */

svgtiny_code svgtiny_parse_poly(dom_element *poly,
		struct svgtiny_parse_state state, bool polygon)
{
	svgtiny_code err;
	dom_string *points_str;
	dom_exception exc;
	char *s, *points;
	float *p;
	unsigned int i;

	svgtiny_setup_state_local(&state);

	svgtiny_parse_paint_attributes(poly, &state);
	svgtiny_parse_transform_attributes(poly, &state);
	
	exc = dom_element_get_attribute(poly, state.interned_points,
					&points_str);
	if (exc != DOM_NO_ERR) {
		svgtiny_cleanup_state_local(&state);
		return svgtiny_LIBDOM_ERROR;
	}
	
	if (points_str == NULL) {
		state.diagram->error_line = -1; /* poly->line; */
		state.diagram->error_message =
				"polyline/polygon: missing points attribute";
		svgtiny_cleanup_state_local(&state);
		return svgtiny_SVG_ERROR;
	}

	s = points = strndup(dom_string_data(points_str),
			     dom_string_byte_length(points_str));
	dom_string_unref(points_str);
	/* read points attribute */
	if (s == NULL) {
		svgtiny_cleanup_state_local(&state);
		return svgtiny_OUT_OF_MEMORY;
	}
	/* allocate space for path: it will never have more elements than s */
	p = malloc(sizeof p[0] * strlen(s));
	if (!p) {
		free(points);
		svgtiny_cleanup_state_local(&state);
		return svgtiny_OUT_OF_MEMORY;
	}

	/* parse s and build path */
	for (i = 0; s[i]; i++)
		if (s[i] == ',')
			s[i] = ' ';
	i = 0;
	while (*s) {
		float x, y;
		int n;

		if (sscanf(s, "%f %f %n", &x, &y, &n) == 2) {
			if (i == 0)
				p[i++] = svgtiny_PATH_MOVE;
			else
				p[i++] = svgtiny_PATH_LINE;
			p[i++] = x;
			p[i++] = y;
			s += n;
                } else {
                	break;
                }
        }
        if (polygon)
		p[i++] = svgtiny_PATH_CLOSE;

	free(points);

	err = svgtiny_add_path(p, i, &state);

	svgtiny_cleanup_state_local(&state);

	return err;
}


/**
 * Parse a <text> or <tspan> element node.
 */

svgtiny_code svgtiny_parse_text(dom_element *text,
		struct svgtiny_parse_state state)
{
	float x, y, width, height;
	float px, py;
	dom_node *child;
	dom_exception exc;

	svgtiny_setup_state_local(&state);

	svgtiny_parse_position_attributes(text, state,
			&x, &y, &width, &height);
	svgtiny_parse_font_attributes(text, &state);
	svgtiny_parse_transform_attributes(text, &state);

	px = state.ctm.a * x + state.ctm.c * y + state.ctm.e;
	py = state.ctm.b * x + state.ctm.d * y + state.ctm.f;
/* 	state.ctm.e = px - state.origin_x; */
/* 	state.ctm.f = py - state.origin_y; */

	/*struct css_style style = state.style;
	style.font_size.value.length.value *= state.ctm.a;*/
	
        exc = dom_node_get_first_child(text, &child);
	if (exc != DOM_NO_ERR) {
		return svgtiny_LIBDOM_ERROR;
		svgtiny_cleanup_state_local(&state);
	}
	while (child != NULL) {
		dom_node *next;
		dom_node_type nodetype;
		svgtiny_code code = svgtiny_OK;

		exc = dom_node_get_node_type(child, &nodetype);
		if (exc != DOM_NO_ERR) {
			dom_node_unref(child);
			svgtiny_cleanup_state_local(&state);
			return svgtiny_LIBDOM_ERROR;
		}
		if (nodetype == DOM_ELEMENT_NODE) {
			dom_string *nodename;
			exc = dom_node_get_node_name(child, &nodename);
			if (exc != DOM_NO_ERR) {
				dom_node_unref(child);
				svgtiny_cleanup_state_local(&state);
				return svgtiny_LIBDOM_ERROR;
			}
			if (dom_string_caseless_isequal(nodename,
							state.interned_tspan))
				code = svgtiny_parse_text((dom_element *)child,
							  state);
			dom_string_unref(nodename);
		} else if (nodetype == DOM_TEXT_NODE) {
			struct svgtiny_shape *shape = svgtiny_add_shape(&state);
			dom_string *content;
			if (shape == NULL) {
				dom_node_unref(child);
				svgtiny_cleanup_state_local(&state);
				return svgtiny_OUT_OF_MEMORY;
			}
			exc = dom_text_get_whole_text(child, &content);
			if (exc != DOM_NO_ERR) {
				dom_node_unref(child);
				svgtiny_cleanup_state_local(&state);
				return svgtiny_LIBDOM_ERROR;
			}
			if (content != NULL) {
				shape->text = strndup(dom_string_data(content),
						      dom_string_byte_length(content));
				dom_string_unref(content);
			} else {
				shape->text = strdup("");
			}
			shape->text_x = px;
			shape->text_y = py;
			state.diagram->shape_count++;
		}

		if (code != svgtiny_OK) {
			dom_node_unref(child);
			svgtiny_cleanup_state_local(&state);
			return code;
		}
		exc = dom_node_get_next_sibling(child, &next);
		dom_node_unref(child);
		if (exc != DOM_NO_ERR) {
			svgtiny_cleanup_state_local(&state);
			return svgtiny_LIBDOM_ERROR;
		}
		child = next;
	}

	svgtiny_cleanup_state_local(&state);

	return svgtiny_OK;
}


/**
 * Parse x, y, width, and height attributes, if present.
 */

void svgtiny_parse_position_attributes(dom_element *node,
		const struct svgtiny_parse_state state,
		float *x, float *y, float *width, float *height)
{
	dom_string *attr;
	dom_exception exc;

	*x = 0;
	*y = 0;
	*width = state.viewport_width;
	*height = state.viewport_height;

	exc = dom_element_get_attribute(node, state.interned_x, &attr);
	if (exc == DOM_NO_ERR && attr != NULL) {
		*x = svgtiny_parse_length(attr, state.viewport_width, state);
		dom_string_unref(attr);
	}

	exc = dom_element_get_attribute(node, state.interned_y, &attr);
	if (exc == DOM_NO_ERR && attr != NULL) {
		*y = svgtiny_parse_length(attr, state.viewport_height, state);
		dom_string_unref(attr);
	}

	exc = dom_element_get_attribute(node, state.interned_width, &attr);
	if (exc == DOM_NO_ERR && attr != NULL) {
		*width = svgtiny_parse_length(attr, state.viewport_width,
					      state);
		dom_string_unref(attr);
	}

	exc = dom_element_get_attribute(node, state.interned_height, &attr);
	if (exc == DOM_NO_ERR && attr != NULL) {
		*height = svgtiny_parse_length(attr, state.viewport_height,
					       state);
		dom_string_unref(attr);
	}
}


/**
 * Parse a length as a number of pixels.
 */

static float _svgtiny_parse_length(const char *s, int viewport_size,
				   const struct svgtiny_parse_state state)
{
	int num_length = strspn(s, "0123456789+-.");
	const char *unit = s + num_length;
	float n = atof((const char *) s);
	float font_size = 20; /*css_len2px(&state.style.font_size.value.length, 0);*/

	UNUSED(state);

	if (unit[0] == 0) {
		return n;
	} else if (unit[0] == '%') {
		return n / 100.0 * viewport_size;
	} else if (unit[0] == 'e' && unit[1] == 'm') {
		return n * font_size;
	} else if (unit[0] == 'e' && unit[1] == 'x') {
		return n / 2.0 * font_size;
	} else if (unit[0] == 'p' && unit[1] == 'x') {
		return n;
	} else if (unit[0] == 'p' && unit[1] == 't') {
		return n * 1.25;
	} else if (unit[0] == 'p' && unit[1] == 'c') {
		return n * 15.0;
	} else if (unit[0] == 'm' && unit[1] == 'm') {
		return n * 3.543307;
	} else if (unit[0] == 'c' && unit[1] == 'm') {
		return n * 35.43307;
	} else if (unit[0] == 'i' && unit[1] == 'n') {
		return n * 90;
	}

	return 0;
}

float svgtiny_parse_length(dom_string *s, int viewport_size,
			   const struct svgtiny_parse_state state)
{
	char *ss = strndup(dom_string_data(s), dom_string_byte_length(s));
	float ret = _svgtiny_parse_length(ss, viewport_size, state);
	free(ss);
	return ret;
}

/**
 * Parse paint attributes, if present.
 */

void svgtiny_parse_paint_attributes(dom_element *node,
		struct svgtiny_parse_state *state)
{
	dom_string *attr;
	dom_exception exc;
	
	exc = dom_element_get_attribute(node, state->interned_fill, &attr);
	if (exc == DOM_NO_ERR && attr != NULL) {
		svgtiny_parse_color(attr, &state->fill, state);
		dom_string_unref(attr);
	}

	exc = dom_element_get_attribute(node, state->interned_stroke, &attr);
	if (exc == DOM_NO_ERR && attr != NULL) {
		svgtiny_parse_color(attr, &state->stroke, state);
		dom_string_unref(attr);
	}

	exc = dom_element_get_attribute(node, state->interned_stroke_width, &attr);
	if (exc == DOM_NO_ERR && attr != NULL) {
		state->stroke_width = svgtiny_parse_length(attr,
						state->viewport_width, *state);
		dom_string_unref(attr);
	}

	exc = dom_element_get_attribute(node, state->interned_style, &attr);
	if (exc == DOM_NO_ERR && attr != NULL) {
		char *style = strndup(dom_string_data(attr),
				      dom_string_byte_length(attr));
		const char *s;
		char *value;
		if ((s = strstr(style, "fill:"))) {
			s += 5;
			while (*s == ' ')
				s++;
			value = strndup(s, strcspn(s, "; "));
			_svgtiny_parse_color(value, &state->fill, state);
			free(value);
		}
		if ((s = strstr(style, "stroke:"))) {
			s += 7;
			while (*s == ' ')
				s++;
			value = strndup(s, strcspn(s, "; "));
			_svgtiny_parse_color(value, &state->stroke, state);
			free(value);
		}
		if ((s = strstr(style, "stroke-width:"))) {
			s += 13;
			while (*s == ' ')
				s++;
			value = strndup(s, strcspn(s, "; "));
			state->stroke_width = _svgtiny_parse_length(value,
						state->viewport_width, *state);
			free(value);
		}
		free(style);
		dom_string_unref(attr);
	}
}


/**
 * Parse a colour.
 */

static void _svgtiny_parse_color(const char *s, svgtiny_colour *c,
		struct svgtiny_parse_state *state)
{
	unsigned int r, g, b;
	float rf, gf, bf;
	size_t len = strlen(s);
	char *id = 0, *rparen;

	if (len == 4 && s[0] == '#') {
		if (sscanf(s + 1, "%1x%1x%1x", &r, &g, &b) == 3)
			*c = svgtiny_RGB(r | r << 4, g | g << 4, b | b << 4);

	} else if (len == 7 && s[0] == '#') {
		if (sscanf(s + 1, "%2x%2x%2x", &r, &g, &b) == 3)
			*c = svgtiny_RGB(r, g, b);

	} else if (10 <= len && s[0] == 'r' && s[1] == 'g' && s[2] == 'b' &&
			s[3] == '(' && s[len - 1] == ')') {
		if (sscanf(s + 4, "%u,%u,%u", &r, &g, &b) == 3)
			*c = svgtiny_RGB(r, g, b);
		else if (sscanf(s + 4, "%f%%,%f%%,%f%%", &rf, &gf, &bf) == 3) {
			b = bf * 255 / 100;
			g = gf * 255 / 100;
			r = rf * 255 / 100;
			*c = svgtiny_RGB(r, g, b);
		}

	} else if (len == 4 && strcmp(s, "none") == 0) {
		*c = svgtiny_TRANSPARENT;

	} else if (5 < len && s[0] == 'u' && s[1] == 'r' && s[2] == 'l' &&
			s[3] == '(') {
		if (s[4] == '#') {
			id = strdup(s + 5);
			if (!id)
				return;
			rparen = strchr(id, ')');
			if (rparen)
				*rparen = 0;
			svgtiny_find_gradient(id, state);
			free(id);
			if (state->linear_gradient_stop_count == 0)
				*c = svgtiny_TRANSPARENT;
			else if (state->linear_gradient_stop_count == 1)
				*c = state->gradient_stop[0].color;
			else
				*c = svgtiny_LINEAR_GRADIENT;
		}

	} else {
		const struct svgtiny_named_color *named_color;
		named_color = svgtiny_color_lookup(s, strlen(s));
		if (named_color)
			*c = named_color->color;
	}
}

void svgtiny_parse_color(dom_string *s, svgtiny_colour *c,
		struct svgtiny_parse_state *state)
{
	char *ss = strndup(dom_string_data(s), dom_string_byte_length(s));
	_svgtiny_parse_color(ss, c, state);
	free(ss);
}

/**
 * Parse font attributes, if present.
 */

void svgtiny_parse_font_attributes(dom_element *node,
		struct svgtiny_parse_state *state)
{
	/* TODO: Implement this, it never used to be */
	UNUSED(node);
	UNUSED(state);
#ifdef WRITTEN_THIS_PROPERLY
	const xmlAttr *attr;

	UNUSED(state);

	for (attr = node->properties; attr; attr = attr->next) {
		if (strcmp((const char *) attr->name, "font-size") == 0) {
			/*if (css_parse_length(
					(const char *) attr->children->content,
					&state->style.font_size.value.length,
					true, true)) {
				state->style.font_size.size =
						CSS_FONT_SIZE_LENGTH;
			}*/
		}
        }
#endif
}


/**
 * Parse transform attributes, if present.
 *
 * http://www.w3.org/TR/SVG11/coords#TransformAttribute
 */

void svgtiny_parse_transform_attributes(dom_element *node,
		struct svgtiny_parse_state *state)
{
	char *transform;
	dom_string *attr;
	dom_exception exc;
	
	exc = dom_element_get_attribute(node, state->interned_transform,
					&attr);
	if (exc == DOM_NO_ERR && attr != NULL) {
		transform = strndup(dom_string_data(attr),
				    dom_string_byte_length(attr));
		svgtiny_parse_transform(transform, &state->ctm.a, &state->ctm.b,
				&state->ctm.c, &state->ctm.d,
				&state->ctm.e, &state->ctm.f);
		free(transform);
		dom_string_unref(attr);
	}
}


/**
 * Parse a transform string.
 */

void svgtiny_parse_transform(char *s, float *ma, float *mb,
		float *mc, float *md, float *me, float *mf)
{
	float a, b, c, d, e, f;
	float za, zb, zc, zd, ze, zf;
	float angle, x, y;
	int n;
	unsigned int i;

	for (i = 0; s[i]; i++)
		if (s[i] == ',')
			s[i] = ' ';

	while (*s) {
		a = d = 1;
		b = c = 0;
		e = f = 0;
		if (sscanf(s, "matrix (%f %f %f %f %f %f) %n",
					&a, &b, &c, &d, &e, &f, &n) == 6)
			;
		else if (sscanf(s, "translate (%f %f) %n",
					&e, &f, &n) == 2)
			;
		else if (sscanf(s, "translate (%f) %n",
					&e, &n) == 1)
			;
		else if (sscanf(s, "scale (%f %f) %n",
					&a, &d, &n) == 2)
			;
		else if (sscanf(s, "scale (%f) %n",
					&a, &n) == 1)
			d = a;
		else if (sscanf(s, "rotate (%f %f %f) %n",
					&angle, &x, &y, &n) == 3) {
			angle = angle / 180 * M_PI;
			a = cos(angle);
			b = sin(angle);
			c = -sin(angle);
			d = cos(angle);
			e = -x * cos(angle) + y * sin(angle) + x;
			f = -x * sin(angle) - y * cos(angle) + y;
		} else if (sscanf(s, "rotate (%f) %n",
					&angle, &n) == 1) {
			angle = angle / 180 * M_PI;
			a = cos(angle);
			b = sin(angle);
			c = -sin(angle);
			d = cos(angle);
		} else if (sscanf(s, "skewX (%f) %n",
					&angle, &n) == 1) {
			angle = angle / 180 * M_PI;
			c = tan(angle);
		} else if (sscanf(s, "skewY (%f) %n",
					&angle, &n) == 1) {
			angle = angle / 180 * M_PI;
			b = tan(angle);
		} else
			break;
		za = *ma * a + *mc * b;
		zb = *mb * a + *md * b;
		zc = *ma * c + *mc * d;
		zd = *mb * c + *md * d;
		ze = *ma * e + *mc * f + *me;
		zf = *mb * e + *md * f + *mf;
		*ma = za;
		*mb = zb;
		*mc = zc;
		*md = zd;
		*me = ze;
		*mf = zf;
		s += n;
	}
}


/**
 * Add a path to the svgtiny_diagram.
 */

svgtiny_code svgtiny_add_path(float *p, unsigned int n,
		struct svgtiny_parse_state *state)
{
	struct svgtiny_shape *shape;

	if (state->fill == svgtiny_LINEAR_GRADIENT)
		return svgtiny_add_path_linear_gradient(p, n, state);

	svgtiny_transform_path(p, n, state);

	shape = svgtiny_add_shape(state);
	if (!shape) {
		free(p);
		return svgtiny_OUT_OF_MEMORY;
	}
	shape->path = p;
	shape->path_length = n;
	state->diagram->shape_count++;

	return svgtiny_OK;
}


/**
 * Add a svgtiny_shape to the svgtiny_diagram.
 */

struct svgtiny_shape *svgtiny_add_shape(struct svgtiny_parse_state *state)
{
	struct svgtiny_shape *shape = realloc(state->diagram->shape,
			(state->diagram->shape_count + 1) *
			sizeof (state->diagram->shape[0]));
	if (!shape)
		return 0;
	state->diagram->shape = shape;

	shape += state->diagram->shape_count;
	shape->path = 0;
	shape->path_length = 0;
	shape->text = 0;
	shape->fill = state->fill;
	shape->stroke = state->stroke;
	shape->stroke_width = lroundf((float) state->stroke_width *
			(state->ctm.a + state->ctm.d) / 2.0);
	if (0 < state->stroke_width && shape->stroke_width == 0)
		shape->stroke_width = 1;

	return shape;
}


/**
 * Apply the current transformation matrix to a path.
 */

void svgtiny_transform_path(float *p, unsigned int n,
		struct svgtiny_parse_state *state)
{
	unsigned int j;

	for (j = 0; j != n; ) {
		unsigned int points = 0;
		unsigned int k;
		switch ((int) p[j]) {
		case svgtiny_PATH_MOVE:
		case svgtiny_PATH_LINE:
			points = 1;
			break;
		case svgtiny_PATH_CLOSE:
			points = 0;
			break;
		case svgtiny_PATH_BEZIER:
			points = 3;
			break;
		default:
			assert(0);
		}
		j++;
		for (k = 0; k != points; k++) {
			float x0 = p[j], y0 = p[j + 1];
			float x = state->ctm.a * x0 + state->ctm.c * y0 +
				state->ctm.e;
			float y = state->ctm.b * x0 + state->ctm.d * y0 +
				state->ctm.f;
			p[j] = x;
			p[j + 1] = y;
			j += 2;
		}
	}
}


/**
 * Free all memory used by a diagram.
 */

void svgtiny_free(struct svgtiny_diagram *svg)
{
	unsigned int i;
	assert(svg);

	for (i = 0; i != svg->shape_count; i++) {
		free(svg->shape[i].path);
		free(svg->shape[i].text);
	}
	
	free(svg->shape);

	free(svg);
}

#ifndef HAVE_STRNDUP
char *svgtiny_strndup(const char *s, size_t n)
{
	size_t len;
	char *s2;

	for (len = 0; len != n && s[len]; len++)
		continue;

	s2 = malloc(len + 1);
	if (s2 == NULL)
		return NULL;

	memcpy(s2, s, len);
	s2[len] = '\0';

	return s2;
}
#endif


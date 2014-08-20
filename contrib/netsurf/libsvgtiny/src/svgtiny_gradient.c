/*
 * This file is part of Libsvgtiny
 * Licensed under the MIT License,
 *                http://opensource.org/licenses/mit-license.php
 * Copyright 2008 James Bursa <james@semichrome.net>
 */

#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "svgtiny.h"
#include "svgtiny_internal.h"

#undef GRADIENT_DEBUG

static svgtiny_code svgtiny_parse_linear_gradient(dom_element *linear,
		struct svgtiny_parse_state *state);
static float svgtiny_parse_gradient_offset(const char *s);
static void svgtiny_path_bbox(float *p, unsigned int n,
		float *x0, float *y0, float *x1, float *y1);
static void svgtiny_invert_matrix(float *m, float *inv);


/**
 * Find a gradient by id and parse it.
 */

void svgtiny_find_gradient(const char *id, struct svgtiny_parse_state *state)
{
	dom_element *gradient;
	dom_string *id_str, *name;
	dom_exception exc;

	#ifdef GRADIENT_DEBUG
	fprintf(stderr, "svgtiny_find_gradient: id \"%s\"\n", id);
	#endif

	state->linear_gradient_stop_count = 0;
	if (state->gradient_x1 != NULL)
		dom_string_unref(state->gradient_x1);
	if (state->gradient_y1 != NULL)
		dom_string_unref(state->gradient_y1);
	if (state->gradient_x2 != NULL)
		dom_string_unref(state->gradient_x2);
	if (state->gradient_y2 != NULL)
		dom_string_unref(state->gradient_y2);
	state->gradient_x1 = dom_string_ref(state->interned_zero_percent);
	state->gradient_y1 = dom_string_ref(state->interned_zero_percent);
	state->gradient_x2 = dom_string_ref(state->interned_hundred_percent);
	state->gradient_y2 = dom_string_ref(state->interned_zero_percent);
	state->gradient_user_space_on_use = false;
	state->gradient_transform.a = 1;
	state->gradient_transform.b = 0;
	state->gradient_transform.c = 0;
	state->gradient_transform.d = 1;
	state->gradient_transform.e = 0;
	state->gradient_transform.f = 0;
	
	exc = dom_string_create_interned((const uint8_t *) id,
			strlen(id), &id_str);
	if (exc != DOM_NO_ERR)
		return;
	
	exc = dom_document_get_element_by_id(state->document, id_str,
					     &gradient);
	dom_string_unref(id_str);
	if (exc != DOM_NO_ERR || gradient == NULL) {
		#ifdef GRADIENT_DEBUG
		fprintf(stderr, "gradient \"%s\" not found\n", id);
		#endif
		return;
	}
	
	exc = dom_node_get_node_name(gradient, &name);
	if (exc != DOM_NO_ERR) {
		dom_node_unref(gradient);
		return;
	}
	
	if (dom_string_isequal(name, state->interned_linearGradient))
		svgtiny_parse_linear_gradient(gradient, state);
	
	dom_node_unref(gradient);
	dom_string_unref(name);

	#ifdef GRADIENT_DEBUG
	fprintf(stderr, "linear_gradient_stop_count %i\n",
			state->linear_gradient_stop_count);
	#endif
}


/**
 * Parse a <linearGradient> element node.
 *
 * http://www.w3.org/TR/SVG11/pservers#LinearGradients
 */

svgtiny_code svgtiny_parse_linear_gradient(dom_element *linear,
		struct svgtiny_parse_state *state)
{
	unsigned int i = 0;
	dom_string *attr;
	dom_exception exc;
	dom_nodelist *stops;
	
	exc = dom_element_get_attribute(linear, state->interned_href, &attr);
	if (exc == DOM_NO_ERR && attr != NULL) {
		if (dom_string_data(attr)[0] == (uint8_t) '#') {
			char *s = strndup(dom_string_data(attr) + 1,
					  dom_string_byte_length(attr) - 1);
			svgtiny_find_gradient(s, state);
			free(s);
		}
		dom_string_unref(attr);
	}

	exc = dom_element_get_attribute(linear, state->interned_x1, &attr);
	if (exc == DOM_NO_ERR && attr != NULL) {
		dom_string_unref(state->gradient_x1);
		state->gradient_x1 = attr;
		attr = NULL;
	}

	exc = dom_element_get_attribute(linear, state->interned_y1, &attr);
	if (exc == DOM_NO_ERR && attr != NULL) {
		dom_string_unref(state->gradient_y1);
		state->gradient_y1 = attr;
		attr = NULL;
	}

	exc = dom_element_get_attribute(linear, state->interned_x2, &attr);
	if (exc == DOM_NO_ERR && attr != NULL) {
		dom_string_unref(state->gradient_x2);
		state->gradient_x2 = attr;
		attr = NULL;
	}

	exc = dom_element_get_attribute(linear, state->interned_y2, &attr);
	if (exc == DOM_NO_ERR && attr != NULL) {
		dom_string_unref(state->gradient_y2);
		state->gradient_y2 = attr;
		attr = NULL;
	}
	
	exc = dom_element_get_attribute(linear, state->interned_gradientUnits,
					&attr);
	if (exc == DOM_NO_ERR && attr != NULL) {
		state->gradient_user_space_on_use = 
			dom_string_isequal(attr,
					   state->interned_userSpaceOnUse);
		dom_string_unref(attr);
	}
	
	exc = dom_element_get_attribute(linear,
					state->interned_gradientTransform,
					&attr);
	if (exc == DOM_NO_ERR && attr != NULL) {
		float a = 1, b = 0, c = 0, d = 1, e = 0, f = 0;
		char *s = strndup(dom_string_data(attr),
				  dom_string_byte_length(attr));
		if (s == NULL) {
			dom_string_unref(attr);
			return svgtiny_OUT_OF_MEMORY;
		}
		svgtiny_parse_transform(s, &a, &b, &c, &d, &e, &f);
		free(s);
		#ifdef GRADIENT_DEBUG
		fprintf(stderr, "transform %g %g %g %g %g %g\n",
			a, b, c, d, e, f);
		#endif
		state->gradient_transform.a = a;
		state->gradient_transform.b = b;
		state->gradient_transform.c = c;
		state->gradient_transform.d = d;
		state->gradient_transform.e = e;
		state->gradient_transform.f = f;
		dom_string_unref(attr);
        }
	
	exc = dom_element_get_elements_by_tag_name(linear,
						   state->interned_stop,
						   &stops);
	if (exc == DOM_NO_ERR && stops != NULL) {
		uint32_t listlen, stopnr;
		exc = dom_nodelist_get_length(stops, &listlen);
		if (exc != DOM_NO_ERR) {
			dom_nodelist_unref(stops);
			goto no_more_stops;
		}
		
		for (stopnr = 0; stopnr < listlen; ++stopnr) {
			dom_element *stop;
			float offset = -1;
			svgtiny_colour color = svgtiny_TRANSPARENT;
			exc = dom_nodelist_item(stops, stopnr,
						(dom_node **) (void *) &stop);
			if (exc != DOM_NO_ERR)
				continue;
			exc = dom_element_get_attribute(stop,
							state->interned_offset,
							&attr);
			if (exc == DOM_NO_ERR && attr != NULL) {
				char *s = strndup(dom_string_data(attr),
						  dom_string_byte_length(attr));
				offset = svgtiny_parse_gradient_offset(s);
				free(s);
				dom_string_unref(attr);
			}
			exc = dom_element_get_attribute(stop,
							state->interned_stop_color,
							&attr);
			if (exc == DOM_NO_ERR && attr != NULL) {
				svgtiny_parse_color(attr, &color, state);
				dom_string_unref(attr);
			}
			exc = dom_element_get_attribute(stop,
							state->interned_style,
							&attr);
			if (exc == DOM_NO_ERR && attr != NULL) {
				char *content = strndup(dom_string_data(attr),
							dom_string_byte_length(attr));
				const char *s;
				dom_string *value;
				if ((s = strstr(content, "stop-color:"))) {
					s += 11;
					while (*s == ' ')
						s++;
					exc = dom_string_create_interned(
						(const uint8_t *) s,
						strcspn(s, "; "),
						&value);
					if (exc != DOM_NO_ERR && 
					    value != NULL) {
						svgtiny_parse_color(value,
								    &color,
								    state);
						dom_string_unref(value);
					}
				}
				free(content);
				dom_string_unref(attr);
			}
			if (offset != -1 && color != svgtiny_TRANSPARENT) {
				#ifdef GRADIENT_DEBUG
				fprintf(stderr, "stop %g %x\n", offset, color);
				#endif
				state->gradient_stop[i].offset = offset;
				state->gradient_stop[i].color = color;
				i++;
			}
			dom_node_unref(stop);
			if (i == svgtiny_MAX_STOPS)
				break;
		}
		
		dom_nodelist_unref(stops);
	}
no_more_stops:	
	if (i > 0)
		state->linear_gradient_stop_count = i;

	return svgtiny_OK;
}


float svgtiny_parse_gradient_offset(const char *s)
{
	int num_length = strspn(s, "0123456789+-.");
	const char *unit = s + num_length;
	float n = atof((const char *) s);

	if (unit[0] == 0)
		;
	else if (unit[0] == '%')
		n /= 100.0;
	else
		return -1;

	if (n < 0)
		n = 0;
	if (1 < n)
		n = 1;
	return n;
}


/**
 * Add a path with a linear gradient fill to the svgtiny_diagram.
 */

svgtiny_code svgtiny_add_path_linear_gradient(float *p, unsigned int n,
		struct svgtiny_parse_state *state)
{
	struct grad_point {
		float x, y, r;
	};
	float object_x0, object_y0, object_x1, object_y1;
	float gradient_x0, gradient_y0, gradient_x1, gradient_y1,
	      gradient_dx, gradient_dy;
	float trans[6];
	unsigned int steps = 10;
	float x0 = 0, y0 = 0, x0_trans, y0_trans, r0; /* segment start point */
	float x1, y1, x1_trans, y1_trans, r1; /* segment end point */
	/* segment control points (beziers only) */
	float c0x = 0, c0y = 0, c1x = 0, c1y = 0;
	float gradient_norm_squared;
	struct svgtiny_list *pts;
	float min_r = 1000;
	unsigned int min_pt = 0;
	unsigned int j;
	unsigned int stop_count;
	unsigned int current_stop;
	float last_stop_r;
	float current_stop_r;
	int red0, green0, blue0, red1, green1, blue1;
	unsigned int t, a, b;

	/* determine object bounding box */
	svgtiny_path_bbox(p, n, &object_x0, &object_y0, &object_x1, &object_y1);
	#ifdef GRADIENT_DEBUG
	fprintf(stderr, "object bbox: (%g %g) (%g %g)\n",
			object_x0, object_y0, object_x1, object_y1);
	#endif

	if (!state->gradient_user_space_on_use) {
		gradient_x0 = object_x0 +
				svgtiny_parse_length(state->gradient_x1,
					object_x1 - object_x0, *state);
		gradient_y0 = object_y0 +
				svgtiny_parse_length(state->gradient_y1,
					object_y1 - object_y0, *state);
		gradient_x1 = object_x0 +
				svgtiny_parse_length(state->gradient_x2,
					object_x1 - object_x0, *state);
		gradient_y1 = object_y0 +
				svgtiny_parse_length(state->gradient_y2,
					object_y1 - object_y0, *state);
	} else {
		gradient_x0 = svgtiny_parse_length(state->gradient_x1,
				state->viewport_width, *state);
		gradient_y0 = svgtiny_parse_length(state->gradient_y1,
				state->viewport_height, *state);
		gradient_x1 = svgtiny_parse_length(state->gradient_x2,
				state->viewport_width, *state);
		gradient_y1 = svgtiny_parse_length(state->gradient_y2,
				state->viewport_height, *state);
	}
	gradient_dx = gradient_x1 - gradient_x0;
	gradient_dy = gradient_y1 - gradient_y0;
	#ifdef GRADIENT_DEBUG
	fprintf(stderr, "gradient vector: (%g %g) => (%g %g)\n",
			gradient_x0, gradient_y0, gradient_x1, gradient_y1);
	#endif

	/* show theoretical gradient strips for debugging */
	/*unsigned int strips = 10;
	for (unsigned int z = 0; z != strips; z++) {
		float f0, fd, strip_x0, strip_y0, strip_dx, strip_dy;
		f0 = (float) z / (float) strips;
		fd = (float) 1 / (float) strips;
		strip_x0 = gradient_x0 + f0 * gradient_dx;
		strip_y0 = gradient_y0 + f0 * gradient_dy;
		strip_dx = fd * gradient_dx;
		strip_dy = fd * gradient_dy;
		fprintf(stderr, "strip %i vector: (%g %g) + (%g %g)\n",
				z, strip_x0, strip_y0, strip_dx, strip_dy);

		float *p = malloc(13 * sizeof p[0]);
		if (!p)
			return svgtiny_OUT_OF_MEMORY;
		p[0] = svgtiny_PATH_MOVE;
		p[1] = strip_x0 + (strip_dy * 3);
		p[2] = strip_y0 - (strip_dx * 3);
		p[3] = svgtiny_PATH_LINE;
		p[4] = p[1] + strip_dx;
		p[5] = p[2] + strip_dy;
		p[6] = svgtiny_PATH_LINE;
		p[7] = p[4] - (strip_dy * 6);
		p[8] = p[5] + (strip_dx * 6);
		p[9] = svgtiny_PATH_LINE;
		p[10] = p[7] - strip_dx;
		p[11] = p[8] - strip_dy;
		p[12] = svgtiny_PATH_CLOSE;
		svgtiny_transform_path(p, 13, state);
		struct svgtiny_shape *shape = svgtiny_add_shape(state);
		if (!shape) {
			free(p);
			return svgtiny_OUT_OF_MEMORY;
		}
		shape->path = p;
		shape->path_length = 13;
		shape->fill = svgtiny_TRANSPARENT;
		shape->stroke = svgtiny_RGB(0, 0xff, 0);
		state->diagram->shape_count++;
	}*/

	/* invert gradient transform for applying to vertices */
	svgtiny_invert_matrix(&state->gradient_transform.a, trans);
	#ifdef GRADIENT_DEBUG
	fprintf(stderr, "inverse transform %g %g %g %g %g %g\n",
			trans[0], trans[1], trans[2], trans[3],
			trans[4], trans[5]);
	#endif

	/* compute points on the path for triangle vertices */
	/* r, r0, r1 are distance along gradient vector */
	gradient_norm_squared = gradient_dx * gradient_dx +
	                              gradient_dy * gradient_dy;
	pts = svgtiny_list_create(
			sizeof (struct grad_point));
	if (!pts)
		return svgtiny_OUT_OF_MEMORY;
	for (j = 0; j != n; ) {
		int segment_type = (int) p[j];
		struct grad_point *point;
		unsigned int z;

		if (segment_type == svgtiny_PATH_MOVE) {
			x0 = p[j + 1];
			y0 = p[j + 2];
			j += 3;
			continue;
		}

		assert(segment_type == svgtiny_PATH_CLOSE ||
				segment_type == svgtiny_PATH_LINE ||
				segment_type == svgtiny_PATH_BEZIER);

		/* start point (x0, y0) */
		x0_trans = trans[0]*x0 + trans[2]*y0 + trans[4];
		y0_trans = trans[1]*x0 + trans[3]*y0 + trans[5];
		r0 = ((x0_trans - gradient_x0) * gradient_dx +
				(y0_trans - gradient_y0) * gradient_dy) /
				gradient_norm_squared;
		point = svgtiny_list_push(pts);
		if (!point) {
			svgtiny_list_free(pts);
			return svgtiny_OUT_OF_MEMORY;
		}
		point->x = x0;
		point->y = y0;
		point->r = r0;
		if (r0 < min_r) {
			min_r = r0;
			min_pt = svgtiny_list_size(pts) - 1;
		}

		/* end point (x1, y1) */
		if (segment_type == svgtiny_PATH_LINE) {
			x1 = p[j + 1];
			y1 = p[j + 2];
			j += 3;
		} else if (segment_type == svgtiny_PATH_CLOSE) {
			x1 = p[1];
			y1 = p[2];
			j++;
		} else /* svgtiny_PATH_BEZIER */ {
			c0x = p[j + 1];
			c0y = p[j + 2];
			c1x = p[j + 3];
			c1y = p[j + 4];
			x1 = p[j + 5];
			y1 = p[j + 6];
			j += 7;
		}
		x1_trans = trans[0]*x1 + trans[2]*y1 + trans[4];
		y1_trans = trans[1]*x1 + trans[3]*y1 + trans[5];
		r1 = ((x1_trans - gradient_x0) * gradient_dx +
				(y1_trans - gradient_y0) * gradient_dy) /
				gradient_norm_squared;

		/* determine steps from change in r */

		if(isnan(r0) || isnan(r1)) {
			steps = 1;
		} else {
			steps = ceilf(fabsf(r1 - r0) / 0.05);
		}

		if (steps == 0)
			steps = 1;
		#ifdef GRADIENT_DEBUG
		fprintf(stderr, "r0 %g, r1 %g, steps %i\n",
				r0, r1, steps);
		#endif

		/* loop through intermediate points */
		for (z = 1; z != steps; z++) {
			float t, x, y, x_trans, y_trans, r;
			struct grad_point *point;
			t = (float) z / (float) steps;
			if (segment_type == svgtiny_PATH_BEZIER) {
				x = (1-t) * (1-t) * (1-t) * x0 +
					3 * t * (1-t) * (1-t) * c0x +
					3 * t * t * (1-t) * c1x +
					t * t * t * x1;
				y = (1-t) * (1-t) * (1-t) * y0 +
					3 * t * (1-t) * (1-t) * c0y +
					3 * t * t * (1-t) * c1y +
					t * t * t * y1;
			} else {
				x = (1-t) * x0 + t * x1;
				y = (1-t) * y0 + t * y1;
			}
			x_trans = trans[0]*x + trans[2]*y + trans[4];
			y_trans = trans[1]*x + trans[3]*y + trans[5];
			r = ((x_trans - gradient_x0) * gradient_dx +
					(y_trans - gradient_y0) * gradient_dy) /
					gradient_norm_squared;
			#ifdef GRADIENT_DEBUG
			fprintf(stderr, "(%g %g [%g]) ", x, y, r);
			#endif
			point = svgtiny_list_push(pts);
			if (!point) {
				svgtiny_list_free(pts);
				return svgtiny_OUT_OF_MEMORY;
			}
			point->x = x;
			point->y = y;
			point->r = r;
			if (r < min_r) {
				min_r = r;
				min_pt = svgtiny_list_size(pts) - 1;
			}
		}
		#ifdef GRADIENT_DEBUG
		fprintf(stderr, "\n");
		#endif

		/* next segment start point is this segment end point */
		x0 = x1;
		y0 = y1;
	}
	#ifdef GRADIENT_DEBUG
	fprintf(stderr, "pts size %i, min_pt %i, min_r %.3f\n",
			svgtiny_list_size(pts), min_pt, min_r);
	#endif

	/* render triangles */
	stop_count = state->linear_gradient_stop_count;
	assert(2 <= stop_count);
	current_stop = 0;
	last_stop_r = 0;
	current_stop_r = state->gradient_stop[0].offset;
	red0 = red1 = svgtiny_RED(state->gradient_stop[0].color);
	green0 = green1 = svgtiny_GREEN(state->gradient_stop[0].color);
	blue0 = blue1 = svgtiny_BLUE(state->gradient_stop[0].color);
	t = min_pt;
	a = (min_pt + 1) % svgtiny_list_size(pts);
	b = min_pt == 0 ? svgtiny_list_size(pts) - 1 : min_pt - 1;
	while (a != b) {
		struct grad_point *point_t = svgtiny_list_get(pts, t);
		struct grad_point *point_a = svgtiny_list_get(pts, a);
		struct grad_point *point_b = svgtiny_list_get(pts, b);
		float mean_r = (point_t->r + point_a->r + point_b->r) / 3;
		float *p;
		struct svgtiny_shape *shape;
		/*fprintf(stderr, "triangle: t %i %.3f a %i %.3f b %i %.3f "
				"mean_r %.3f\n",
				t, pts[t].r, a, pts[a].r, b, pts[b].r,
				mean_r);*/
		while (current_stop != stop_count && current_stop_r < mean_r) {
			current_stop++;
			if (current_stop == stop_count)
				break;
			red0 = red1;
			green0 = green1;
			blue0 = blue1;
			red1 = svgtiny_RED(state->
					gradient_stop[current_stop].color);
			green1 = svgtiny_GREEN(state->
					gradient_stop[current_stop].color);
			blue1 = svgtiny_BLUE(state->
					gradient_stop[current_stop].color);
			last_stop_r = current_stop_r;
			current_stop_r = state->
					gradient_stop[current_stop].offset;
		}
		p = malloc(10 * sizeof p[0]);
		if (!p)
			return svgtiny_OUT_OF_MEMORY;
		p[0] = svgtiny_PATH_MOVE;
		p[1] = point_t->x;
		p[2] = point_t->y;
		p[3] = svgtiny_PATH_LINE;
		p[4] = point_a->x;
		p[5] = point_a->y;
		p[6] = svgtiny_PATH_LINE;
		p[7] = point_b->x;
		p[8] = point_b->y;
		p[9] = svgtiny_PATH_CLOSE;
		svgtiny_transform_path(p, 10, state);
		shape = svgtiny_add_shape(state);
		if (!shape) {
			free(p);
			return svgtiny_OUT_OF_MEMORY;
		}
		shape->path = p;
		shape->path_length = 10;
		/*shape->fill = svgtiny_TRANSPARENT;*/
		if (current_stop == 0)
			shape->fill = state->gradient_stop[0].color;
		else if (current_stop == stop_count)
			shape->fill = state->
					gradient_stop[stop_count - 1].color;
		else {
			float stop_r = (mean_r - last_stop_r) /
				(current_stop_r - last_stop_r);
			shape->fill = svgtiny_RGB(
				(int) ((1 - stop_r) * red0 + stop_r * red1),
				(int) ((1 - stop_r) * green0 + stop_r * green1),
				(int) ((1 - stop_r) * blue0 + stop_r * blue1));
		}
		shape->stroke = svgtiny_TRANSPARENT;
		#ifdef GRADIENT_DEBUG
		shape->stroke = svgtiny_RGB(0, 0, 0xff);
		#endif
		state->diagram->shape_count++;
		if (point_a->r < point_b->r) {
			t = a;
			a = (a + 1) % svgtiny_list_size(pts);
		} else {
			t = b;
			b = b == 0 ? svgtiny_list_size(pts) - 1 : b - 1;
		}
	}

	/* render gradient vector for debugging */
	#ifdef GRADIENT_DEBUG
	{
		float *p = malloc(7 * sizeof p[0]);
		if (!p)
			return svgtiny_OUT_OF_MEMORY;
		p[0] = svgtiny_PATH_MOVE;
		p[1] = gradient_x0;
		p[2] = gradient_y0;
		p[3] = svgtiny_PATH_LINE;
		p[4] = gradient_x1;
		p[5] = gradient_y1;
		p[6] = svgtiny_PATH_CLOSE;
		svgtiny_transform_path(p, 7, state);
		struct svgtiny_shape *shape = svgtiny_add_shape(state);
		if (!shape) {
			free(p);
			return svgtiny_OUT_OF_MEMORY;
		}
		shape->path = p;
		shape->path_length = 7;
		shape->fill = svgtiny_TRANSPARENT;
		shape->stroke = svgtiny_RGB(0xff, 0, 0);
		state->diagram->shape_count++;
	}
	#endif

	/* render triangle vertices with r values for debugging */
	#ifdef GRADIENT_DEBUG
	for (unsigned int i = 0; i != svgtiny_list_size(pts); i++) {
		struct grad_point *point = svgtiny_list_get(pts, i);
		struct svgtiny_shape *shape = svgtiny_add_shape(state);
		if (!shape)
			return svgtiny_OUT_OF_MEMORY;
		char *text = malloc(20);
		if (!text)
			return svgtiny_OUT_OF_MEMORY;
		sprintf(text, "%i=%.3f", i, point->r);
		shape->text = text;
		shape->text_x = state->ctm.a * point->x +
				state->ctm.c * point->y + state->ctm.e;
		shape->text_y = state->ctm.b * point->x +
				state->ctm.d * point->y + state->ctm.f;
		shape->fill = svgtiny_RGB(0, 0, 0);
		shape->stroke = svgtiny_TRANSPARENT;
		state->diagram->shape_count++;
	}
	#endif

	/* plot actual path outline */
	if (state->stroke != svgtiny_TRANSPARENT) {
		struct svgtiny_shape *shape;
		svgtiny_transform_path(p, n, state);

		shape = svgtiny_add_shape(state);
		if (!shape) {
			free(p);
			return svgtiny_OUT_OF_MEMORY;
		}
		shape->path = p;
		shape->path_length = n;
		shape->fill = svgtiny_TRANSPARENT;
		state->diagram->shape_count++;
	} else {
		free(p);
	}

	svgtiny_list_free(pts);

	return svgtiny_OK;
}


/**
 * Get the bounding box of path.
 */

void svgtiny_path_bbox(float *p, unsigned int n,
		float *x0, float *y0, float *x1, float *y1)
{
	unsigned int j;

	*x0 = *x1 = p[1];
	*y0 = *y1 = p[2];

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
			float x = p[j], y = p[j + 1];
			if (x < *x0)
				*x0 = x;
			else if (*x1 < x)
				*x1 = x;
			if (y < *y0)
				*y0 = y;
			else if (*y1 < y)
				*y1 = y;
			j += 2;
		}
	}
}


/**
 * Invert a transformation matrix.
 */
void svgtiny_invert_matrix(float *m, float *inv)
{
	float determinant = m[0]*m[3] - m[1]*m[2];
	inv[0] = m[3] / determinant;
	inv[1] = -m[1] / determinant;
	inv[2] = -m[2] / determinant;
	inv[3] = m[0] / determinant;
	inv[4] = (m[2]*m[5] - m[3]*m[4]) / determinant;
	inv[5] = (m[1]*m[4] - m[0]*m[5]) / determinant;
}


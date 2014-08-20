/*
 * This file is part of Libsvgtiny
 * Licensed under the MIT License,
 *                http://opensource.org/licenses/mit-license.php
 * Copyright 2009-2010 James Bursa <james@semichrome.net>
 */

/*
 * This example loads an SVG using libsvgtiny and then displays it in an X11
 * window using cairo.
 *
 * Functions of interest for libsvgtiny use are:
 *  main() - loads an SVG using svgtiny_create() and svgtiny_parse()
 *  event_diagram_expose() - renders the SVG by stepping through the shapes
 *
 * Compile using:
 *  gcc -g -W -Wall -o svgtiny_display_x11 svgtiny_display_x11.c \
 *          `pkg-config --cflags --libs libsvgtiny cairo` -lX11
 */


#include <libgen.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <cairo.h>
#include <cairo-xlib.h>
#include "svgtiny.h"


struct svgtiny_diagram *diagram;
Display *display;
Window diagram_window;
Atom wm_protocols_atom, wm_delete_window_atom;
char *svg_path;
float scale = 1.0;
bool quit = false;


void gui_init(void);
void gui_quit(void);
void update_window_title(void);
void gui_poll(void);
void event_diagram_key_press(XKeyEvent *key_event);
void event_diagram_expose(const XExposeEvent *expose_event);
void render_path(cairo_t *cr, float scale, struct svgtiny_shape *path);
void die(const char *message);


/**
 * Main program.
 */
int main(int argc, char *argv[])
{
	FILE *fd;
	struct stat sb;
	char *buffer;
	size_t size;
	size_t n;
	svgtiny_code code;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s FILE\n", argv[0]);
		return 1;
	}
	svg_path = argv[1];

	/* load file into memory buffer */
	fd = fopen(svg_path, "rb");
	if (!fd) {
		perror(svg_path);
		return 1;
	}

	if (stat(svg_path, &sb)) {
		perror(svg_path);
		return 1;
	}
	size = sb.st_size;

	buffer = malloc(size);
	if (!buffer) {
		fprintf(stderr, "Unable to allocate %lld bytes\n",
				(long long) size);
		return 1;
	}

	n = fread(buffer, 1, size, fd);
	if (n != size) {
		perror(svg_path);
		return 1;
	}

	fclose(fd);

	/* create svgtiny object */
	diagram = svgtiny_create();
	if (!diagram) {
		fprintf(stderr, "svgtiny_create failed\n");
		return 1;
	}

	/* parse */
	code = svgtiny_parse(diagram, buffer, size, svg_path, 1000, 1000);
	if (code != svgtiny_OK) {
		fprintf(stderr, "svgtiny_parse failed: ");
		switch (code) {
		case svgtiny_OUT_OF_MEMORY:
			fprintf(stderr, "svgtiny_OUT_OF_MEMORY");
			break;
		case svgtiny_LIBXML_ERROR:
			fprintf(stderr, "svgtiny_LIBXML_ERROR");
			break;
		case svgtiny_NOT_SVG:
			fprintf(stderr, "svgtiny_NOT_SVG");
			break;
		case svgtiny_SVG_ERROR:
			fprintf(stderr, "svgtiny_SVG_ERROR: line %i: %s",
					diagram->error_line,
					diagram->error_message);
			break;
		default:
			fprintf(stderr, "unknown svgtiny_code %i", code);
			break;
		}
		fprintf(stderr, "\n");
	}

	free(buffer);

	/*printf("viewbox 0 0 %u %u\n",
			diagram->width, diagram->height);*/

	gui_init();

	while (!quit) {
		gui_poll();
	}

	gui_quit();
	svgtiny_free(diagram);

	return 0;
}


/**
 * Initialize X11 interface.
 */
void gui_init(void)
{
	display = XOpenDisplay(NULL);
	if (!display)
		die("XOpenDisplay failed: is DISPLAY set?");

	diagram_window = XCreateSimpleWindow(display,
			DefaultRootWindow(display),
			0, 0, diagram->width, diagram->height, 0, 0, 0);

	update_window_title();

	XMapWindow(display, diagram_window);
	XSelectInput(display, diagram_window,
			KeyPressMask |
			ButtonPressMask |
			ExposureMask |
			StructureNotifyMask);

	wm_protocols_atom = XInternAtom(display, "WM_PROTOCOLS", False);
	wm_delete_window_atom = XInternAtom(display, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(display, diagram_window, &wm_delete_window_atom, 1);
}


/**
 * Free X11 interface.
 */
void gui_quit(void)
{
	XCloseDisplay(display);
}


/**
 * Update window title to show current state.
 */
void update_window_title(void)
{
	char title[100];
	char *svg_path_copy;
	char *base_name;

	svg_path_copy = strdup(svg_path);
	if (!svg_path_copy) {
		fprintf(stderr, "out of memory\n");
		return;
	}

	base_name = basename(svg_path_copy);

	snprintf(title, sizeof title, "%s (%i%%) - svgtiny",
			base_name, (int) roundf(scale * 100.0));

	XStoreName(display, diagram_window, title);

	free(svg_path_copy);
}


/**
 * Handle an X11 event.
 */
void gui_poll(void)
{
	XEvent event;
	XNextEvent(display, &event);

	switch (event.type) {
	case KeyPress:
		if (event.xkey.window == diagram_window) {
			event_diagram_key_press(&event.xkey);
		}
		break;
	case Expose:
		if (event.xexpose.window == diagram_window) {
			event_diagram_expose(&event.xexpose);
		}
		break;
	case ClientMessage:
		if (event.xclient.message_type == wm_protocols_atom &&
				event.xclient.format == 32 &&
				(Atom) event.xclient.data.l[0] ==
				wm_delete_window_atom)
			quit = true;
		break;
	default:
		/*printf("unknown event %i\n", event.type);*/
		break;
	}
}


/**
 * Handle an X11 KeyPress event in the diagram window.
 */
void event_diagram_key_press(XKeyEvent *key_event)
{
	KeySym key_sym;
	float new_scale = scale;
	unsigned int width, height;

	key_sym = XLookupKeysym(key_event, 0);

	switch (key_sym) {
	case XK_q:
	case XK_Escape:
		quit = true;
		break;

	case XK_minus:
	case XK_KP_Subtract:
		new_scale -= 0.1;
		break;

	case XK_equal:
	case XK_plus:
	case XK_KP_Add:
		new_scale += 0.1;
		break;
	
	case XK_1:
	case XK_KP_Multiply:
	case XK_KP_1:
		new_scale = 1;
		break;
	
	case XK_2:
	case XK_KP_2:
		new_scale = 2;
		break;

	default:
		break;
	}

	if (new_scale < 0.1)
		new_scale = 0.1;
	else if (5 < new_scale)
		new_scale = 5;

	if (new_scale == scale)
		return;

	scale = new_scale;
	width = diagram->width * scale;
	height = diagram->height * scale;
	if (width < 400)
		width = 400;
	if (height < 400)
		height = 400;
	XResizeWindow(display, diagram_window, width, height);
	XClearArea(display, diagram_window, 0, 0, 0, 0, True);
	update_window_title();
}


/**
 * Handle an X11 Expose event of the diagram window.
 */
void event_diagram_expose(const XExposeEvent *expose_event)
{
	cairo_surface_t *surface;
	cairo_t *cr;
	cairo_status_t status;
	unsigned int i;

	if (expose_event->count != 0)
		return;

	surface = cairo_xlib_surface_create(display, diagram_window,
			DefaultVisual(display, DefaultScreen(display)),
			diagram->width * scale, diagram->height * scale);
	if (!surface) {
		fprintf(stderr, "cairo_xlib_surface_create failed\n");
		return;
	}

	cr = cairo_create(surface);
	status = cairo_status(cr);
	if (status != CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "cairo_create failed: %s\n",
				cairo_status_to_string(status));
		cairo_destroy(cr);
		cairo_surface_destroy(surface);
		return;
	}

	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_paint(cr);

	for (i = 0; i != diagram->shape_count; i++) {
		if (diagram->shape[i].path) {
			render_path(cr, scale, &diagram->shape[i]);

		} else if (diagram->shape[i].text) {
			cairo_set_source_rgb(cr,
				svgtiny_RED(diagram->shape[i].stroke) / 255.0,
				svgtiny_GREEN(diagram->shape[i].stroke) / 255.0,
				svgtiny_BLUE(diagram->shape[i].stroke) / 255.0);
			cairo_move_to(cr,
					scale * diagram->shape[i].text_x,
					scale * diagram->shape[i].text_y);
			cairo_show_text(cr, diagram->shape[i].text);
		}
	}

	status = cairo_status(cr);
	if (status != CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "cairo error: %s\n",
				cairo_status_to_string(status));
		cairo_destroy(cr);
		cairo_surface_destroy(surface);
		return;
	}

	cairo_destroy(cr);
	cairo_surface_destroy(surface);
}


/**
 * Render an svgtiny path using cairo.
 */
void render_path(cairo_t *cr, float scale, struct svgtiny_shape *path)
{
	unsigned int j;

	cairo_new_path(cr);
	for (j = 0; j != path->path_length; ) {
		switch ((int) path->path[j]) {
		case svgtiny_PATH_MOVE:
			cairo_move_to(cr,
					scale * path->path[j + 1],
					scale * path->path[j + 2]);
			j += 3;
			break;
		case svgtiny_PATH_CLOSE:
			cairo_close_path(cr);
			j += 1;
			break;
		case svgtiny_PATH_LINE:
			cairo_line_to(cr,
					scale * path->path[j + 1],
					scale * path->path[j + 2]);
			j += 3;
			break;
		case svgtiny_PATH_BEZIER:
			cairo_curve_to(cr,
					scale * path->path[j + 1],
					scale * path->path[j + 2],
					scale * path->path[j + 3],
					scale * path->path[j + 4],
					scale * path->path[j + 5],
					scale * path->path[j + 6]);
			j += 7;
			break;
		default:
			printf("error ");
			j += 1;
		}
	}
	if (path->fill != svgtiny_TRANSPARENT) {
		cairo_set_source_rgb(cr,
				svgtiny_RED(path->fill) / 255.0,
				svgtiny_GREEN(path->fill) / 255.0,
				svgtiny_BLUE(path->fill) / 255.0);
		cairo_fill_preserve(cr);
	}
	if (path->stroke != svgtiny_TRANSPARENT) {
		cairo_set_source_rgb(cr,
				svgtiny_RED(path->stroke) / 255.0,
				svgtiny_GREEN(path->stroke) / 255.0,
				svgtiny_BLUE(path->stroke) / 255.0);
		cairo_set_line_width(cr, scale * path->stroke_width);
		cairo_stroke_preserve(cr);
	}
}


/**
 * Exit with fatal error.
 */
void die(const char *message)
{
	fprintf(stderr, "%s\n", message);
	exit(1);
}


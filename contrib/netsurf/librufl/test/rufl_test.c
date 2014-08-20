/*
 * This file is part of RUfl
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license
 * Copyright 2006 James Bursa <james@semichrome.net>
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "rufl.h"


static void try(rufl_code code, const char *context);
static int move_to(os_coord *to, void *user);
static int line_to(os_coord *to, void *user);
static int cubic_to(os_coord *control1, os_coord *control2, os_coord *to,
		void *user);
static void callback(void *context,
		const char *font_name, unsigned int font_size,
		const char *s8, unsigned short *s16, unsigned int n,
		int x, int y);


int main(void)
{
	char utf8_test[] = "Hello,	world! ὕαλον "
			"Uherské Hradiště. 𐀀";
	int width;
	size_t char_offset;
	int x;
	int actual_x;
	struct rufl_decomp_funcs funcs = { move_to, line_to, cubic_to };
	int bbox[4];

	try(rufl_init(), "rufl_init");
	rufl_dump_state();
	try(rufl_paint("NewHall", rufl_WEIGHT_400, 240,
			utf8_test, sizeof utf8_test - 1,
			1200, 1000, 0), "rufl_paint");
	try(rufl_width("NewHall", rufl_WEIGHT_400, 240,
			utf8_test, sizeof utf8_test - 1,
			&width), "rufl_width");
	printf("width: %i\n", width);
	for (x = 0; x < width + 100; x += 100) {
		try(rufl_x_to_offset("NewHall", rufl_WEIGHT_400, 240,
				utf8_test, sizeof utf8_test - 1,
				x, &char_offset, &actual_x),
				"rufl_x_to_offset");
		printf("x to offset: %i -> %i %zi \"%s\"\n", x, actual_x,
				char_offset, utf8_test + char_offset);
		try(rufl_split("NewHall", rufl_WEIGHT_400, 240,
				utf8_test, sizeof utf8_test - 1,
				x, &char_offset, &actual_x),
				"rufl_split");
		printf("split: %i -> %i %zi \"%s\"\n", x, actual_x,
				char_offset, utf8_test + char_offset);
	}
	try(rufl_decompose_glyph("Homerton", rufl_WEIGHT_400, 1280,
				"A", 1, &funcs, 0),
				"rufl_decompose_glyph");
	try(rufl_paint_callback("NewHall", rufl_WEIGHT_400, 240,
			utf8_test, sizeof utf8_test - 1,
			1200, 1000, callback, 0), "rufl_paint_callback");
	try(rufl_font_bbox("NewHall", rufl_WEIGHT_400, 240, bbox),
			"rufl_font_bbox");
	printf("bbox: %i %i %i %i\n", bbox[0], bbox[1], bbox[2], bbox[3]);
	rufl_quit();

	return 0;
}


void try(rufl_code code, const char *context)
{
	if (code == rufl_OK)
		return;
	else if (code == rufl_OUT_OF_MEMORY)
		printf("error: %s: out of memory\n", context);
	else if (code == rufl_FONT_MANAGER_ERROR)
		printf("error: %s: Font Manager error %x %s\n", context,
				rufl_fm_error->errnum,
				rufl_fm_error->errmess);
	else if (code == rufl_FONT_NOT_FOUND)
		printf("error: %s: font not found\n", context);
	else if (code == rufl_IO_ERROR)
		printf("error: %s: io error: %i %s\n", context, errno,
				strerror(errno));
	else if (code == rufl_IO_EOF)
		printf("error: %s: eof\n", context);
	else
		printf("error: %s: unknown error\n", context);
	rufl_quit();
	exit(1);
}


int move_to(os_coord *to, void *user)
{
	(void) user;

	printf("Move to (%d,%d)\n", to->x, to->y);

	return 0;
}


int line_to(os_coord *to, void *user)
{
	(void) user;

	printf("Line to (%d,%d)\n", to->x, to->y);

	return 0;
}


int cubic_to(os_coord *control1, os_coord *control2, os_coord *to,
		void *user)
{
	(void) user;

	printf("Bezier to (%d,%d),(%d,%d),(%d,%d)\n",
			control1->x, control1->y,
			control2->x, control2->y,
			to->x, to->y);

	return 0;
}


void callback(void *context,
		const char *font_name, unsigned int font_size,
		const char *s8, unsigned short *s16, unsigned int n,
		int x, int y)
{
	(void) context;

	printf("callback: \"%s\", %u, ", font_name, font_size);
	if (s8)
		printf("s8 \"%.*s\" ", n, s8);
	else {
		printf("s16 \"");
		for (unsigned int i = 0; i != n; i++)
			printf("%x ", (unsigned int) s16[i]);
		printf("\" ");
	}
	printf("%i %i\n", x, y);
}

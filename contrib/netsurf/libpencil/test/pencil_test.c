/*
 * This file is part of Pencil
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license
 * Copyright 2005 James Bursa <james@semichrome.net>
 */

#include <assert.h>
#include <stdio.h>
#include <oslib/osfile.h>
#include <oslib/osspriteop.h>
#include <rufl.h>
#include "pencil.h"


#define SPRITE "Resources:$.Resources.Desktop.Sprites"


static void test_pencil(void);


int main(int argc, char *argv[])
{
	rufl_code code;

	(void) argc;
	(void) argv;

	code = rufl_init();
	if (code != rufl_OK) {
		printf("rufl_init failed: %i\n", code);
		return 1;
	}

	test_pencil();

	rufl_quit();

	return 0;
}


void test_pencil(void)
{
	struct pencil_diagram *diagram;
	pencil_code code;
	int path[] = {2, 100, 40, 8, 100, 400, 8, 300, 300, 0};
	char utf8_test[] = "Hello,	world! ὕαλον "
			"Uherské Hradiště. 𐀀";
	char *drawfile_buffer;
	size_t drawfile_size;
	os_error *error;
	fileswitch_object_type obj_type;
	int size;
	osspriteop_area *area;

	diagram = pencil_create();
	if (!diagram) {
		printf("pencil_create failed\n");
		return;
	}

	code = pencil_text(diagram,
			100, 40,
			"Homerton", rufl_WEIGHT_400,
			320,
			"Hello, world!", 13,
			0x000000);
	if (code != pencil_OK) {
		printf("pencil_text failed: %i\n", code);
		return;
	}

	code = pencil_path(diagram,
			path, sizeof path / sizeof path[0],
			0x00ff00, 0x0000ff,
			5, pencil_JOIN_ROUND,
			pencil_CAP_BUTT, pencil_CAP_TRIANGLE,
			15, 20,
			false, pencil_SOLID);
	if (code != pencil_OK) {
		printf("pencil_path failed: %i\n", code);
		return;
	}

	code = pencil_text(diagram,
			100, 400,
			"NewHall", rufl_WEIGHT_400,
			320,
			utf8_test, sizeof utf8_test,
			0xff0000);
	if (code != pencil_OK) {
		printf("pencil_text failed: %i\n", code);
		return;
	}

	error = xosfile_read_no_path(SPRITE, &obj_type, 0, 0, &size, 0);
	if (error) {
		printf("xosfile_read_no_path failed: 0x%x: %s\n",
				error->errnum, error->errmess);
		return;
	}
	if (obj_type != fileswitch_IS_FILE) {
		printf("File " SPRITE " does not exist\n");
		return;
	}

	area = malloc(size + 4);
	if (!area) {
		printf("Out of memory\n");
		return;
	}
	area->size = size + 4;
	area->sprite_count = 0;
	area->first = 0;
	area->used = 16;

	error = xosspriteop_load_sprite_file(osspriteop_USER_AREA,
			area, SPRITE);
	if (error) {
		printf("xosspriteop_load_sprite_file failed: 0x%x: %s\n",
				error->errnum, error->errmess);
		return;
	}

	code = pencil_sprite(diagram, 400, 200, 200, 100,
			((char *) area) + area->first);
	if (code != pencil_OK) {
		printf("pencil_sprite failed: %i\n", code);
		return;
	}

	pencil_dump(diagram);

	code = pencil_save_drawfile(diagram, "Pencil-Test",
			&drawfile_buffer, &drawfile_size);
	if (code != pencil_OK) {
		printf("pencil_save_drawfile failed: %i\n", code);
		return;
	}
	assert(drawfile_buffer);

	error = xosfile_save_stamped("DrawFile", osfile_TYPE_DRAW,
			(byte *) drawfile_buffer, 
			(byte *) drawfile_buffer + drawfile_size);
	if (error) {
		printf("xosfile_save_stamped failed: 0x%x: %s\n",
				error->errnum, error->errmess);
		return;
	}

	pencil_free(diagram);
}

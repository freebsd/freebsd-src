/*
 * This file is part of Libsvgtiny
 * Licensed under the MIT License,
 *                http://opensource.org/licenses/mit-license.php
 * Copyright 2008 James Bursa <james@semichrome.net>
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>

#include <svgtiny.h>

#include "libnsfb.h"
#include "libnsfb_plot.h"
#include "libnsfb_event.h"

    nsfb_t *nsfb;
    nsfb_bbox_t screen_box;
    uint8_t *fbptr;
    int fbstride;
    nsfb_event_t event;

static int setup_fb(void)
{
    nsfb = nsfb_init(NSFB_FRONTEND_SDL);
    if (nsfb == NULL) {
        fprintf(stderr, "Unable to initialise nsfb with SDL frontend\n");
        return 1;
    }

    if (nsfb_init_frontend(nsfb) == -1) {
        fprintf(stderr, "Unable to initialise nsfb frontend\n");
        return 2;
    }

    /* get the geometry of the whole screen */
    screen_box.x0 = screen_box.y0 = 0;
    nsfb_get_geometry(nsfb, &screen_box.x1, &screen_box.y1, NULL);

    nsfb_get_framebuffer(nsfb, &fbptr, &fbstride);

    /* claim the whole screen for update */
    nsfb_claim(nsfb, &screen_box);

    nsfb_plot_clg(nsfb, 0xffffffff);

    return 0;
}

int main(int argc, char *argv[])
{
    FILE *fd;
    float scale = 1.0;
    struct stat sb;
    char *buffer;
    size_t size;
    size_t n;
    struct svgtiny_diagram *diagram;
    svgtiny_code code;

    if (argc != 2 && argc != 3) {
        fprintf(stderr, "Usage: %s FILE [SCALE]\n", argv[0]);
        return 1;
    }

    /* load file into memory buffer */
    fd = fopen(argv[1], "rb");
    if (!fd) {
        perror(argv[1]);
        return 1;
    }

    if (stat(argv[1], &sb)) {
        perror(argv[1]);
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
        perror(argv[1]);
        return 1;
    }

    fclose(fd);

    /* read scale argument */
    if (argc == 3) {
        scale = atof(argv[2]);
        if (scale == 0)
            scale = 1.0;
    }

    /* create svgtiny object */
    diagram = svgtiny_create();
    if (!diagram) {
        fprintf(stderr, "svgtiny_create failed\n");
        return 1;
    }

    /* parse */
    code = svgtiny_parse(diagram, buffer, size, argv[1], 1000, 1000);
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

    if (setup_fb() != 0)
        return 1;

    for (unsigned int i = 0; i != diagram->shape_count; i++) {
        nsfb_plot_pen_t pen;
        pen.stroke_colour = svgtiny_RED(diagram->shape[i].stroke) |
                            svgtiny_GREEN(diagram->shape[i].stroke) << 8|
                            svgtiny_BLUE(diagram->shape[i].stroke) << 16;
        pen.fill_colour = svgtiny_RED(diagram->shape[i].fill) |
                            svgtiny_GREEN(diagram->shape[i].fill) << 8|
                            svgtiny_BLUE(diagram->shape[i].fill) << 16;

        if (diagram->shape[i].fill == svgtiny_TRANSPARENT)
            pen.fill_type = NFSB_PLOT_OPTYPE_NONE;
        else
            pen.fill_type = NFSB_PLOT_OPTYPE_SOLID;

        if (diagram->shape[i].stroke == svgtiny_TRANSPARENT)
            pen.stroke_type = NFSB_PLOT_OPTYPE_NONE;
        else
            pen.stroke_type = NFSB_PLOT_OPTYPE_SOLID;

        pen.stroke_width = scale * diagram->shape[i].stroke_width;

        if (diagram->shape[i].path) {
            nsfb_plot_pathop_t *fb_path;
            int fb_path_c;
            unsigned int j;
            fb_path = malloc(diagram->shape[i].path_length * 3 * sizeof(nsfb_plot_pathop_t));
            fb_path_c = 0;

            for (j = 0;
                 j != diagram->shape[i].path_length; ) {
                switch ((int) diagram->shape[i].path[j]) {
                case svgtiny_PATH_MOVE:
                    fb_path[fb_path_c].operation = NFSB_PLOT_PATHOP_MOVE;
                    fb_path[fb_path_c].point.x = scale * diagram->shape[i].path[j + 1];
                    fb_path[fb_path_c].point.y = scale * diagram->shape[i].path[j + 2];
                    fb_path_c++;
                    j += 3;
                    break;

                case svgtiny_PATH_CLOSE:	
                    fb_path[fb_path_c].operation = NFSB_PLOT_PATHOP_LINE;
                    fb_path[fb_path_c].point.x = fb_path[0].point.x;
                    fb_path[fb_path_c].point.y = fb_path[0].point.y;
                    fb_path_c++;
                    j += 1;
                    break;

                case svgtiny_PATH_LINE:
                    fb_path[fb_path_c].operation = NFSB_PLOT_PATHOP_LINE;
                    fb_path[fb_path_c].point.x = scale * diagram->shape[i].path[j + 1];
                    fb_path[fb_path_c].point.y = scale * diagram->shape[i].path[j + 2];
                    fb_path_c++;

                    j += 3;
                    break;

                case svgtiny_PATH_BEZIER:
                    fb_path[fb_path_c].operation = NFSB_PLOT_PATHOP_MOVE;
                    fb_path[fb_path_c].point.x = scale * diagram->shape[i].path[j + 1];
                    fb_path[fb_path_c].point.y = scale * diagram->shape[i].path[j + 2];
                    fb_path_c++;
                    fb_path[fb_path_c].operation = NFSB_PLOT_PATHOP_MOVE;
                    fb_path[fb_path_c].point.x = scale * diagram->shape[i].path[j + 3];
                    fb_path[fb_path_c].point.y = scale * diagram->shape[i].path[j + 4];
                    fb_path_c++;
                    fb_path[fb_path_c].operation = NFSB_PLOT_PATHOP_CUBIC;
                    fb_path[fb_path_c].point.x = scale * diagram->shape[i].path[j + 5];
                    fb_path[fb_path_c].point.y = scale * diagram->shape[i].path[j + 6];
                    fb_path_c++;

                    j += 7;
                    break;

                default:
                    printf("error ");
                    j += 1;
                }
            }

            nsfb_plot_path(nsfb, fb_path_c, fb_path, &pen);
        } else if (diagram->shape[i].text) {
            /* printf("text %g %g '%s' ",
               scale * diagram->shape[i].text_x,
               scale * diagram->shape[i].text_y,
               diagram->shape[i].text);*/
        }
    }

    svgtiny_free(diagram);

    nsfb_update(nsfb, &screen_box);
    
    while (event.type != NSFB_EVENT_CONTROL)
        nsfb_event(nsfb, &event, -1);

    return 0;
}

/*

cc -g -std=c99 -D_BSD_SOURCE -I/home/vince/netsurf/libnsfb/include/ -I/home/vince/netsurf/libnsfb/src -I/usr/local/include -I/usr/include/libxml2 -Wall -Wextra -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -Werror -pedantic -Wno-overlength-strings   -DNDEBUG -O2 -DBUILD_TARGET_Linux -DBUILD_HOST_Linux -o build-Linux-Linux-release-lib-static/test_svgtiny.o -c test/svgtiny.c

cc -o build-Linux-Linux-release-lib-static/test_svgtiny build-Linux-Linux-release-lib-static/test_svgtiny.o -Wl,--whole-archive -lnsfb -Wl,--no-whole-archive -lSDL -Lbuild-Linux-Linux-release-lib-static/ -lnsfb -lsvgtiny -lxml2

 */

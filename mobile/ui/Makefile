# Mobile UI Makefile
# Builds the UI library and existing components

.PHONY: all clean install

CC = cc
CFLAGS = -Wall -Wextra -O2 -g -I.
LDFLAGS =

SRCS_COMMON = framebuffer.c ui_widget.c ui_animation.c mobile_ui.c input.c systemui.c touch_gst.c
OBJS_COMMON = $(SRCS_COMMON:.c=.o)

SRCS_TOOLKIT = toolkit/mobile_ui.c toolkit/widgets.c
OBJS_TOOLKIT = $(SRCS_TOOLKIT:.c=.o)

SRCS_COMP = compositor/mobile_compositor.c compositor/compositor_core.c
OBJS_COMP = $(SRCS_COMP:.c=.o)

SRCS_WM = window_manager/window_mgr.c window_manager/window_animator.c
OBJS_WM = $(SRCS_WM:.c=.o)

all: libmobileui.a toolkit/libmobileui_toolkit.a compositor/libmobileui_comp.a

libmobileui.a: $(OBJS_COMMON) $(OBJS_WM)
	ar rcs $@ $(OBJS_COMMON) $(OBJS_WM)

toolkit/libmobileui_toolkit.a: $(OBJS_TOOLKIT)
	cd toolkit && ar rcs libmobileui_toolkit.a $(OBJS_TOOLKIT:.c=.o)

compositor/libmobileui_comp.a: $(OBJS_COMP)
	cd compositor && ar rcs libmobileui_comp.a $(OBJS_COMP:.c=.o)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

compositor/%.o: compositor/%.c
	$(CC) $(CFLAGS) -Icompositor -c -o $@ $<

toolkit/%.o: toolkit/%.c
	$(CC) $(CFLAGS) -Itoolkit -c -o $@ $<

window_manager/%.o: window_manager/%.c
	$(CC) $(CFLAGS) -Iwindow_manager -c -o $@ $<

clean:
	rm -f *.o libmobileui.a
	rm -f toolkit/*.o toolkit/libmobileui_toolkit.a
	rm -f compositor/*.o compositor/libmobileui_comp.a
	rm -f window_manager/*.o

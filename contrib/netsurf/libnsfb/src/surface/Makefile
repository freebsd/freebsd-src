# Sources

# Common surface code and heap based surface handler
SURFACE_HANDLER_yes := surface.c ram.c

# optional surface handlers
SURFACE_HANDLER_$(NSFB_ABLE_AVAILABLE) += able.c
SURFACE_HANDLER_$(NSFB_LINUX_AVAILABLE) += linux.c
SURFACE_HANDLER_$(NSFB_SDL_AVAILABLE) += sdl.c
SURFACE_HANDLER_$(NSFB_XCB_AVAILABLE) += x.c
SURFACE_HANDLER_$(NSFB_VNC_AVAILABLE) += vnc.c
SURFACE_HANDLER_$(NSFB_WLD_AVAILABLE) += wld.c

DIR_SOURCES := $(SURFACE_HANDLER_yes)

include $(NSBUILD)/Makefile.subdir

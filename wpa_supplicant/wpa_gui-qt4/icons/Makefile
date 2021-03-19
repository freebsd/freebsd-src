#!/usr/bin/make -f

NAMES := wpa_gui ap laptop group invitation
SIZES := 16x16 22x22 32x32 48x48 64x64 128x128
ICONS := $(addsuffix .png, $(foreach name, $(NAMES), $(foreach size, $(SIZES), $(size)/$(name))))
ICONS += $(addsuffix .xpm, $(NAMES))

ifeq (1, $(shell which inkscape; echo $$?))
$(error "No inkscape in PATH, it is required for exporting icons.")
else
ifeq (0, $(shell inkscape --without-gui 2>&1 > /dev/null; echo $$?))
# Inkscape < 1.0
INKSCAPE_GUI_FLAG := --without-gui
INKSCAPE_OUTPUT_FLAG := --export-png
else
# Inkscape >= 1.0
INKSCAPE_GUI_FLAG :=
INKSCAPE_OUTPUT_FLAG := --export-filename
endif
endif

all: $(ICONS)

%.png:
	mkdir -p hicolor/$(word 1, $(subst /, ,$(@)))/apps/
	inkscape $(subst .png,.svg, $(word 2, $(subst /, , $(@)))) $(INKSCAPE_GUI_FLAG) \
		--export-width=$(word 1, $(subst x, , $(@)))  \
	        --export-height=$(word 2, $(subst x, , $(subst /, , $(@)))) \
		$(INKSCAPE_OUTPUT_FLAG)=hicolor/$(word 1, $(subst /, ,$(@)))/apps/$(word 2, $(subst /, , $@))

%.xpm:
	mkdir -p pixmaps/
	convert hicolor/16x16/apps/$(@:.xpm=.png) pixmaps/$(@:.xpm=-16.xpm)
	convert hicolor/32x32/apps/$(@:.xpm=.png) pixmaps/$@

clean:
	$(RM) -r pixmaps hicolor

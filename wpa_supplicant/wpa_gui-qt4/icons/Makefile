#!/usr/bin/make -f

NAMES := wpa_gui ap laptop group invitation
SIZES := 16x16 22x22 32x32 48x48 64x64 128x128
ICONS := $(addsuffix .png, $(foreach name, $(NAMES), $(foreach size, $(SIZES), $(size)/$(name))))
ICONS += $(addsuffix .xpm, $(NAMES))

all: $(ICONS)

%.png:
	mkdir -p hicolor/$(word 1, $(subst /, ,$(@)))/apps/
	inkscape $(subst .png,.svg, $(word 2, $(subst /, , $(@)))) --without-gui \
		--export-width=$(word 1, $(subst x, , $(@)))  \
	        --export-height=$(word 2, $(subst x, , $(subst /, , $(@)))) \
		--export-png=hicolor/$(word 1, $(subst /, ,$(@)))/apps/$(word 2, $(subst /, , $@))

%.xpm:
	mkdir -p pixmaps/
	convert hicolor/16x16/apps/$(@:.xpm=.png) pixmaps/$(@:.xpm=-16.xpm)
	convert hicolor/32x32/apps/$(@:.xpm=.png) pixmaps/$@

clean:
	$(RM) -r pixmaps hicolor

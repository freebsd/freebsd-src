#!/usr/bin/make -f

NAME  := wpa_gui
SVG   := $(NAME).svg
SIZES := 16x16 22x22 32x32 48x48 64x64 128x128
ICONS := $(addsuffix .png,$(SIZES))
ICONS += $(addsuffix .xpm,$(NAME) $(NAME)-16)

all: $(ICONS)

%.png:
	mkdir -p hicolor/$(@:.png=)/apps/
	inkscape $(SVG) --without-gui \
		--export-width=$(word 1,$(subst x, ,$(@:.png=)))  \
	        --export-height=$(word 2,$(subst x, ,$(@:.png=))) \
		--export-png=hicolor/$(@:.png=)/apps/$(NAME).png

$(NAME).xpm:
	mkdir -p pixmaps/
	convert hicolor/32x32/apps/$(NAME).png pixmaps/$@

$(NAME)-16.xpm:
	mkdir -p pixmaps/
	convert hicolor/16x16/apps/$(NAME).png pixmaps/$@

clean:
	$(RM) -r pixmaps hicolor

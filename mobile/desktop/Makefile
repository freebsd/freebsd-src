SUBDIRS = wallpaper icon_theme dock panel notifications lockscreen overview \
          workspace animations search app_launcher task_switcher theme panel_applets

CFLAGS = -Wall -Wextra -std=c99 -I. -I../ui -I../services -I../frameworks
LDFLAGS = -lm

all: $(SUBDIRS)

$(SUBDIRS):
	@echo "Building desktop $@..."
	@$(MAKE) -C $@ all

install:
	@echo "Installing desktop..."
	@mkdir -p rootfs/mobile/share/desktop/{themes,icons,wallpapers}
	@mkdir -p rootfs/mobile/share/applications
	@mkdir -p rootfs/etc

clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean || true; \
	done
	@rm -f desktop-*.a

.PHONY: all clean install $(SUBDIRS)

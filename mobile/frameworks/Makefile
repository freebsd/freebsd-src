# Mobile Frameworks - Top-Level Makefile
# Builds all framework subsystems

.PHONY: all clean install

SUBDIRS = app_launcher zygote ipc permissions sandbox lifecycle resource \
          activity view content_resolver notification_mgr

all:
	@echo "Building mobile frameworks..."
	@for dir in $(SUBDIRS); do \
		echo "  $$dir"; \
		$(MAKE) -C $$dir all || exit 1; \
	done

clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean || true; \
	done

install: all
	@mkdir -p ../../rootfs/mobile/lib
	@for dir in $(SUBDIRS); do \
		cp $$dir/*.so ../../rootfs/mobile/lib/ 2>/dev/null || true; \
	done
	@echo "Frameworks installed to rootfs/mobile/lib/"

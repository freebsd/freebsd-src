#
# This dir holds all of the images for PPC machines.
# Tom Rini	January 2001

include $(TOPDIR)/Rules.make

vmlinux.gz: $(TOPDIR)/vmlinux
	$(OBJCOPY) --strip-all -S -O binary $(TOPDIR)/vmlinux vmlinux
	gzip -vf9 vmlinux

clean:
	rm -f sImage vmapus vmlinux* miboot* zImage* zvmlinux* uImage

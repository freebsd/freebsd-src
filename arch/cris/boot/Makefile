#
# arch/cris/boot/Makefile
#

zImage: compressed/vmlinuz

compressed/vmlinuz: $(TOPDIR)/vmlinux
	@$(MAKE) -C compressed vmlinuz

dep:
	@$(MAKE) -C compressed depend
	@$(MAKE) -C rescue depend

clean:
	rm -f zImage tools/build compressed/vmlinux.out
	@$(MAKE) -C compressed clean

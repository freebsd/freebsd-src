# Sources
DIR_SOURCES := svgtiny.c svgtiny_gradient.c svgtiny_list.c

SOURCES := $(SOURCES) $(BUILDDIR)/src_colors.c

$(BUILDDIR)/src_colors.c: src/colors.gperf
	$(VQ)$(ECHO) "   GPERF: $<"
	$(Q)gperf --output-file=$@.tmp $<
# Hack for GCC 4.2 compatibility (gperf 3.0.4 solves this properly)
	$(Q)$(SED) -e 's/#ifdef __GNUC_STDC_INLINE__/#if defined __GNUC_STDC_INLINE__ || defined __GNUC_GNU_INLINE__/' $@.tmp >$@
	$(Q)$(RM) $@.tmp

include $(NSBUILD)/Makefile.subdir

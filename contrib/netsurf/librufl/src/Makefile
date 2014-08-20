# Sources
DIR_SOURCES := rufl_character_set_test.c rufl_decompose.c rufl_dump_state.c \
		rufl_find.c rufl_init.c rufl_invalidate_cache.c \
		rufl_metrics.c rufl_paint.c rufl_quit.c

ifeq ($(toolchain),norcroft)
  DIR_SOURCES := $(DIR_SOURCES) strfuncs.c
endif

SOURCES := $(SOURCES) $(BUILDDIR)/rufl_glyph_map.c

$(BUILDDIR)/rufl_glyph_map.c: src/Glyphs
	$(VQ)$(ECHO) "MKGLYPHS: $<"
	$(Q)$(PERL) tools/makeglyphs < $< > $@

include $(NSBUILD)/Makefile.subdir

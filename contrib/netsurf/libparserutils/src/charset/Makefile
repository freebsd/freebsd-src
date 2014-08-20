# Sources
DIR_SOURCES := aliases.c codec.c

$(DIR)aliases.c: $(DIR)aliases.inc

$(DIR)aliases.inc: build/make-aliases.pl build/Aliases
	$(VQ)$(ECHO) "   ALIAS: $@"
	$(Q)$(PERL) build/make-aliases.pl

ifeq ($(findstring clean,$(MAKECMDGOALS)),clean)
  CLEAN_ITEMS := $(CLEAN_ITEMS) $(DIR)aliases.inc
endif

include $(NSBUILD)/Makefile.subdir

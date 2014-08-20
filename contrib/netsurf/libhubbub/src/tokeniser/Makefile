# Sources
DIR_SOURCES := entities.c tokeniser.c

$(DIR)entities.c: $(DIR)entities.inc

$(DIR)entities.inc: build/make-entities.pl build/Entities
	$(VQ)$(ECHO) "ENTITIES: $@"
	$(Q)$(PERL) build/make-entities.pl

ifeq ($(findstring clean,$(MAKECMDGOALS)),clean)
  CLEAN_ITEMS := $(CLEAN_ITEMS) $(DIR)entities.inc
endif

include $(NSBUILD)/Makefile.subdir

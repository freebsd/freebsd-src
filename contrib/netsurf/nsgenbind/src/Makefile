CFLAGS := $(CFLAGS) -I$(BUILDDIR) -Isrc/ -g -DYYENABLE_NLS=0

# Sources in this directory
DIR_SOURCES := nsgenbind.c webidl-ast.c nsgenbind-ast.c jsapi-libdom.c jsapi-libdom-operator.c jsapi-libdom-property.c jsapi-libdom-const.c

SOURCES := $(SOURCES) $(BUILDDIR)/nsgenbind-parser.c $(BUILDDIR)/nsgenbind-lexer.c $(BUILDDIR)/webidl-parser.c $(BUILDDIR)/webidl-lexer.c

$(BUILDDIR)/%-lexer.c $(BUILDDIR)/%-lexer.h: src/%-lexer.l
	$(VQ)$(ECHO) "    FLEX: $<"
	$(Q)flex --outfile=$(BUILDDIR)/$(*F)-lexer.c --header-file=$(BUILDDIR)/$(*F)-lexer.h $<

$(BUILDDIR)/%-lexer.c: $(BUILDDIR)/%-parser.h

# Bison 2.6 and later require api.prefix, but this breaks Bison 2.5 and earlier.
bisonvsn := $(word 4,$(shell bison --version))
bisonmaj := $(word 1,$(subst ., ,$(bisonvsn)))
bisonmin := $(word 2,$(subst ., ,$(bisonvsn)))
ifeq ($(bisonmaj),1)
  BISON_DEFINES = --name-prefix=$(*F)_
else
  ifeq ($(bisonmaj),2)
    ifneq ($(findstring $(bisonmin),"0 1 2 3 4 5"),)
      BISON_DEFINES = --name-prefix=$(*F)_
    else
      BISON_DEFINES = --define=api.prefix=$(*F)_
    endif
  else
    BISON_DEFINES = --define=api.prefix=$(*F)_
  endif
endif

$(BUILDDIR)/%-parser.c $(BUILDDIR)/%-parser.h: src/%-parser.y
	$(VQ)$(ECHO) "   BISON: $<"
	$(Q)bison -d -t $(BISON_DEFINES) --report=all --output=$(BUILDDIR)/$(*F)-parser.c --defines=$(BUILDDIR)/$(*F)-parser.h $<

# Grab the core makefile
include $(NSBUILD)/Makefile.subdir

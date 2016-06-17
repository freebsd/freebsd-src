#
# This file contains rules which are shared between multiple Makefiles.
#

#
# False targets.
#
.PHONY: dummy

#
# Special variables which should not be exported
#
unexport EXTRA_AFLAGS
unexport EXTRA_CFLAGS
unexport EXTRA_LDFLAGS
unexport EXTRA_ARFLAGS
unexport SUBDIRS
unexport SUB_DIRS
unexport ALL_SUB_DIRS
unexport MOD_SUB_DIRS
unexport O_TARGET
unexport ALL_MOBJS

unexport obj-y
unexport obj-m
unexport obj-n
unexport obj-
unexport export-objs
unexport subdir-y
unexport subdir-m
unexport subdir-n
unexport subdir-

comma	:= ,
EXTRA_CFLAGS_nostdinc := $(EXTRA_CFLAGS) $(kbuild_2_4_nostdinc)

#
# Get things started.
#
first_rule: sub_dirs
	$(MAKE) all_targets

both-m          := $(filter $(mod-subdirs), $(subdir-y))
SUB_DIRS	:= $(subdir-y)
MOD_SUB_DIRS	:= $(sort $(subdir-m) $(both-m))
ALL_SUB_DIRS	:= $(sort $(subdir-y) $(subdir-m) $(subdir-n) $(subdir-))


#
# Common rules
#

%.s: %.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS_nostdinc) -DKBUILD_BASENAME=$(subst $(comma),_,$(subst -,_,$(*F))) $(CFLAGS_$@) -S $< -o $@

%.i: %.c
	$(CPP) $(CFLAGS) $(EXTRA_CFLAGS_nostdinc) -DKBUILD_BASENAME=$(subst $(comma),_,$(subst -,_,$(*F))) $(CFLAGS_$@) $< > $@

%.o: %.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS_nostdinc) -DKBUILD_BASENAME=$(subst $(comma),_,$(subst -,_,$(*F))) $(CFLAGS_$@) -c -o $@ $<
	@ ( \
	    echo 'ifeq ($(strip $(subst $(comma),:,$(CFLAGS) $(EXTRA_CFLAGS_nostdinc) $(CFLAGS_$@))),$$(strip $$(subst $$(comma),:,$$(CFLAGS) $$(EXTRA_CFLAGS_nostdinc) $$(CFLAGS_$@))))' ; \
	    echo 'FILES_FLAGS_UP_TO_DATE += $@' ; \
	    echo 'endif' \
	) > $(dir $@)/.$(notdir $@).flags

%.o: %.s
	$(AS) $(AFLAGS) $(EXTRA_CFLAGS) -o $@ $<

# Old makefiles define their own rules for compiling .S files,
# but these standard rules are available for any Makefile that
# wants to use them.  Our plan is to incrementally convert all
# the Makefiles to these standard rules.  -- rmk, mec
ifdef USE_STANDARD_AS_RULE

%.s: %.S
	$(CPP) $(AFLAGS) $(EXTRA_AFLAGS) $(AFLAGS_$@) $< > $@

%.o: %.S
	$(CC) $(AFLAGS) $(EXTRA_AFLAGS) $(AFLAGS_$@) -c -o $@ $<

endif

%.lst: %.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS_nostdinc) $(CFLAGS_$@) -g -c -o $*.o $<
	$(TOPDIR)/scripts/makelst $* $(TOPDIR) $(OBJDUMP)
#
#
#
all_targets: $(O_TARGET) $(L_TARGET)

#
# Rule to compile a set of .o files into one .o file
#
ifdef O_TARGET
$(O_TARGET): $(obj-y)
	rm -f $@
    ifneq "$(strip $(obj-y))" ""
	$(LD) $(LDFLAGS) $(EXTRA_LDFLAGS) -r -o $@ $(filter $(obj-y), $^)
    else
	$(AR) rcs $@
    endif
	@ ( \
	    echo 'ifeq ($(strip $(subst $(comma),:,$(LDFLAGS) $(EXTRA_LDFLAGS) $(obj-y))),$$(strip $$(subst $$(comma),:,$$(LDFLAGS) $$(EXTRA_LDFLAGS) $$(obj-y))))' ; \
	    echo 'FILES_FLAGS_UP_TO_DATE += $@' ; \
	    echo 'endif' \
	) > $(dir $@)/.$(notdir $@).flags
endif # O_TARGET

#
# Rule to compile a set of .o files into one .a file
#
ifdef L_TARGET
$(L_TARGET): $(obj-y)
	rm -f $@
	$(AR) $(EXTRA_ARFLAGS) rcs $@ $(obj-y)
	@ ( \
	    echo 'ifeq ($(strip $(subst $(comma),:,$(EXTRA_ARFLAGS) $(obj-y))),$$(strip $$(subst $$(comma),:,$$(EXTRA_ARFLAGS) $$(obj-y))))' ; \
	    echo 'FILES_FLAGS_UP_TO_DATE += $@' ; \
	    echo 'endif' \
	) > $(dir $@)/.$(notdir $@).flags
endif


#
# This make dependencies quickly
#
fastdep: dummy
	$(TOPDIR)/scripts/mkdep $(CFLAGS) $(EXTRA_CFLAGS_nostdinc) -- $(wildcard *.[chS]) > .depend
ifdef ALL_SUB_DIRS
	$(MAKE) $(patsubst %,_sfdep_%,$(ALL_SUB_DIRS)) _FASTDEP_ALL_SUB_DIRS="$(ALL_SUB_DIRS)"
endif

ifdef _FASTDEP_ALL_SUB_DIRS
$(patsubst %,_sfdep_%,$(_FASTDEP_ALL_SUB_DIRS)):
	$(MAKE) -C $(patsubst _sfdep_%,%,$@) fastdep
endif

	
#
# A rule to make subdirectories
#
subdir-list = $(sort $(patsubst %,_subdir_%,$(SUB_DIRS)))
sub_dirs: dummy $(subdir-list)

ifdef SUB_DIRS
$(subdir-list) : dummy
	$(MAKE) -C $(patsubst _subdir_%,%,$@)
endif

#
# A rule to make modules
#
ALL_MOBJS = $(filter-out $(obj-y), $(obj-m))
ifneq "$(strip $(ALL_MOBJS))" ""
MOD_DESTDIR := $(shell $(CONFIG_SHELL) $(TOPDIR)/scripts/pathdown.sh)
endif

unexport MOD_DIRS
MOD_DIRS := $(MOD_SUB_DIRS) $(MOD_IN_SUB_DIRS)
ifneq "$(strip $(MOD_DIRS))" ""
.PHONY: $(patsubst %,_modsubdir_%,$(MOD_DIRS))
$(patsubst %,_modsubdir_%,$(MOD_DIRS)) : dummy
	$(MAKE) -C $(patsubst _modsubdir_%,%,$@) modules

.PHONY: $(patsubst %,_modinst_%,$(MOD_DIRS))
$(patsubst %,_modinst_%,$(MOD_DIRS)) : dummy
	$(MAKE) -C $(patsubst _modinst_%,%,$@) modules_install
endif

.PHONY: modules
modules: $(ALL_MOBJS) dummy \
	 $(patsubst %,_modsubdir_%,$(MOD_DIRS))

.PHONY: _modinst__
_modinst__: dummy
ifneq "$(strip $(ALL_MOBJS))" ""
	mkdir -p $(MODLIB)/kernel/$(MOD_DESTDIR)
	cp $(sort $(ALL_MOBJS)) $(MODLIB)/kernel/$(MOD_DESTDIR)
endif

.PHONY: modules_install
modules_install: _modinst__ \
	 $(patsubst %,_modinst_%,$(MOD_DIRS))

#
# A rule to do nothing
#
dummy:

#
# This is useful for testing
#
script:
	$(SCRIPT)

#
# This sets version suffixes on exported symbols
# Separate the object into "normal" objects and "exporting" objects
# Exporting objects are: all objects that define symbol tables
#
ifdef CONFIG_MODULES

multi-used	:= $(filter $(list-multi), $(obj-y) $(obj-m))
multi-objs	:= $(foreach m, $(multi-used), $($(basename $(m))-objs))
active-objs	:= $(sort $(multi-objs) $(obj-y) $(obj-m))

ifdef CONFIG_MODVERSIONS
ifneq "$(strip $(export-objs))" ""

MODINCL = $(TOPDIR)/include/linux/modules

# The -w option (enable warnings) for genksyms will return here in 2.1
# So where has it gone?
#
# Added the SMP separator to stop module accidents between uniprocessor
# and SMP Intel boxes - AC - from bits by Michael Chastain
#

ifdef CONFIG_SMP
	genksyms_smp_prefix := -p smp_
else
	genksyms_smp_prefix := 
endif

$(MODINCL)/%.ver: %.c
	@if [ ! -r $(MODINCL)/$*.stamp -o $(MODINCL)/$*.stamp -ot $< ]; then \
		echo '$(CC) $(CFLAGS) $(EXTRA_CFLAGS_nostdinc) -E -D__GENKSYMS__ $<'; \
		echo '| $(GENKSYMS) $(genksyms_smp_prefix) -k $(VERSION).$(PATCHLEVEL).$(SUBLEVEL) > $@.tmp'; \
		$(CC) $(CFLAGS) $(EXTRA_CFLAGS_nostdinc) -E -D__GENKSYMS__ $< \
		| $(GENKSYMS) $(genksyms_smp_prefix) -k $(VERSION).$(PATCHLEVEL).$(SUBLEVEL) > $@.tmp; \
		if [ -r $@ ] && cmp -s $@ $@.tmp; then echo $@ is unchanged; rm -f $@.tmp; \
		else echo mv $@.tmp $@; mv -f $@.tmp $@; fi; \
	fi; touch $(MODINCL)/$*.stamp
	
$(addprefix $(MODINCL)/,$(export-objs:.o=.ver)): $(TOPDIR)/include/linux/autoconf.h

# updates .ver files but not modversions.h
fastdep: $(addprefix $(MODINCL)/,$(export-objs:.o=.ver))

# updates .ver files and modversions.h like before (is this needed?)
dep: fastdep update-modverfile

endif # export-objs 

# update modversions.h, but only if it would change
update-modverfile:
	@(echo "#ifndef _LINUX_MODVERSIONS_H";\
	  echo "#define _LINUX_MODVERSIONS_H"; \
	  echo "#include <linux/modsetver.h>"; \
	  cd $(TOPDIR)/include/linux/modules; \
	  for f in *.ver; do \
	    if [ -f $$f ]; then echo "#include <linux/modules/$${f}>"; fi; \
	  done; \
	  echo "#endif"; \
	) > $(TOPDIR)/include/linux/modversions.h.tmp
	@if [ -r $(TOPDIR)/include/linux/modversions.h ] && cmp -s $(TOPDIR)/include/linux/modversions.h $(TOPDIR)/include/linux/modversions.h.tmp; then \
		echo $(TOPDIR)/include/linux/modversions.h was not updated; \
		rm -f $(TOPDIR)/include/linux/modversions.h.tmp; \
	else \
		echo $(TOPDIR)/include/linux/modversions.h was updated; \
		mv -f $(TOPDIR)/include/linux/modversions.h.tmp $(TOPDIR)/include/linux/modversions.h; \
	fi

$(active-objs): $(TOPDIR)/include/linux/modversions.h

else

$(TOPDIR)/include/linux/modversions.h:
	@echo "#include <linux/modsetver.h>" > $@

endif # CONFIG_MODVERSIONS

ifneq "$(strip $(export-objs))" ""
$(export-objs): $(export-objs:.o=.c) $(TOPDIR)/include/linux/modversions.h
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS_nostdinc) -DKBUILD_BASENAME=$(subst $(comma),_,$(subst -,_,$(*F))) $(CFLAGS_$@) -DEXPORT_SYMTAB -c $(@:.o=.c)
	@ ( \
	    echo 'ifeq ($(strip $(subst $(comma),:,$(CFLAGS) $(EXTRA_CFLAGS_nostdinc) $(CFLAGS_$@) -DEXPORT_SYMTAB)),$$(strip $$(subst $$(comma),:,$$(CFLAGS) $$(EXTRA_CFLAGS_nostdinc) $$(CFLAGS_$@) -DEXPORT_SYMTAB)))' ; \
	    echo 'FILES_FLAGS_UP_TO_DATE += $@' ; \
	    echo 'endif' \
	) > $(dir $@)/.$(notdir $@).flags
endif

endif # CONFIG_MODULES


#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif

ifneq ($(wildcard $(TOPDIR)/.hdepend),)
include $(TOPDIR)/.hdepend
endif

#
# Find files whose flags have changed and force recompilation.
# For safety, this works in the converse direction:
#   every file is forced, except those whose flags are positively up-to-date.
#
FILES_FLAGS_UP_TO_DATE :=

# For use in expunging commas from flags, which mung our checking.
comma = ,

FILES_FLAGS_EXIST := $(wildcard .*.flags)
ifneq ($(FILES_FLAGS_EXIST),)
include $(FILES_FLAGS_EXIST)
endif

FILES_FLAGS_CHANGED := $(strip \
    $(filter-out $(FILES_FLAGS_UP_TO_DATE), \
	$(O_TARGET) $(L_TARGET) $(active-objs) \
	))

# A kludge: .S files don't get flag dependencies (yet),
#   because that will involve changing a lot of Makefiles.  Also
#   suppress object files explicitly listed in $(IGNORE_FLAGS_OBJS).
#   This allows handling of assembly files that get translated into
#   multiple object files (see arch/ia64/lib/idiv.S, for example).
FILES_FLAGS_CHANGED := $(strip \
    $(filter-out $(patsubst %.S, %.o, $(wildcard *.S) $(IGNORE_FLAGS_OBJS)), \
    $(FILES_FLAGS_CHANGED)))

ifneq ($(FILES_FLAGS_CHANGED),)
$(FILES_FLAGS_CHANGED): dummy
endif

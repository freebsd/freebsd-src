XEN_ROOT=../../../..
include $(XEN_ROOT)/Config.mk

architectures := x86_32 x86_64 ia64
headers := $(patsubst %, %.h, $(architectures))
scripts := $(wildcard *.py)

.PHONY: all clean check-headers
all: $(headers) check-headers

clean:
	rm -f $(headers)
	rm -f checker checker.c $(XEN_TARGET_ARCH).size
	rm -f *.pyc *.o *~

ifeq ($(CROSS_COMPILE)$(XEN_TARGET_ARCH),$(XEN_COMPILE_ARCH))
check-headers: checker
	./checker > $(XEN_TARGET_ARCH).size
	diff -u reference.size $(XEN_TARGET_ARCH).size
checker: checker.c $(headers)
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $<
else
check-headers:
	@echo "cross build: skipping check"
endif

x86_32.h: ../arch-x86/xen-x86_32.h ../arch-x86/xen.h ../xen.h $(scripts)
	python mkheader.py $* $@ $(filter %.h,$^)

x86_64.h: ../arch-x86/xen-x86_64.h ../arch-x86/xen.h ../xen.h $(scripts)
	python mkheader.py $* $@ $(filter %.h,$^)

ia64.h: ../arch-ia64.h ../xen.h $(scripts)
	python mkheader.py $* $@ $(filter %.h,$^)

checker.c: $(scripts)
	python mkchecker.py $(XEN_TARGET_ARCH) $@ $(architectures)

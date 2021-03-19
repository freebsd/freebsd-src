CFLAGS += -I$(abspath ../../../src)
CFLAGS += -I$(abspath ../../../src/utils)

ALL=libhostap_imc.so libhostap_imv.so libhostap2_imc.so libhostap2_imv.so
all: $(ALL)

Q=@
E=echo
ifeq ($(V), 1)
Q=
E=true
endif
ifeq ($(QUIET), 1)
Q=@
E=true
endif

lib%.so: %.c
	$(Q)$(CC) $(LDFLAGS) -o $@ $(CFLAGS) -shared -rdynamic -fPIC $<
	@$(E) "  CC " $@

clean:
	rm -f $(ALL)

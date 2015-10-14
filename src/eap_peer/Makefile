all: libeap_peer.a

clean:
	rm -f *~ *.o *.so *.d *.gcno *.gcda *.gcov libeap_peer.a

install:
	if ls *.so >/dev/null 2>&1; then \
		install -d $(DESTDIR)$(LIBDIR)/wpa_supplicant && \
		cp *.so $(DESTDIR)$(LIBDIR)/wpa_supplicant \
	; fi

include ../lib.rules

CFLAGS += -DIEEE8021X_EAPOL

LIB_OBJS= \
	eap.o \
	eap_methods.o

libeap_peer.a: $(LIB_OBJS)
	$(AR) crT $@ $?

-include $(OBJS:%.o=%.d)

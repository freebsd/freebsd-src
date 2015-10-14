all: libeap_common.a

clean:
	rm -f *~ *.o *.d *.gcno *.gcda *.gcov libeap_common.a

install:
	@echo Nothing to be made.

include ../lib.rules

LIB_OBJS= \
	chap.o \
	eap_common.o \
	eap_eke_common.o \
	eap_eke_common.o \
	eap_fast_common.o \
	eap_gpsk_common.o \
	eap_ikev2_common.o \
	eap_pax_common.o \
	eap_peap_common.o \
	eap_psk_common.o \
	eap_pwd_common.o \
	eap_sake_common.o \
	eap_sim_common.o \
	eap_wsc_common.o \
	ikev2_common.o

libeap_common.a: $(LIB_OBJS)
	$(AR) crT $@ $?

-include $(OBJS:%.o=%.d)

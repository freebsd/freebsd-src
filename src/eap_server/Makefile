all: libeap_server.a

clean:
	rm -f *~ *.o *.d *.gcno *.gcda *.gcov libeap_server.a

install:
	@echo Nothing to be made.

include ../lib.rules

CFLAGS += -DCONFIG_HS20

LIB_OBJS= \
	eap_server.o \
	eap_server_identity.o \
	eap_server_methods.o

libeap_server.a: $(LIB_OBJS)
	$(AR) crT $@ $?

-include $(OBJS:%.o=%.d)

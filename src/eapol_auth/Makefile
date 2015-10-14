all: libeapol_auth.a

clean:
	rm -f *~ *.o *.d *.gcno *.gcda *.gcov libeapol_auth.a

install:
	@echo Nothing to be made.

include ../lib.rules

LIB_OBJS = eapol_auth_sm.o eapol_auth_dump.o

libeapol_auth.a: $(LIB_OBJS)
	$(AR) crT $@ $?

-include $(OBJS:%.o=%.d)

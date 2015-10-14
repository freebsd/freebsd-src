all: libeapol_supp.a

clean:
	rm -f *~ *.o *.d *.gcno *.gcda *.gcov libeapol_supp.a

install:
	@echo Nothing to be made.

include ../lib.rules

CFLAGS += -DIEEE8021X_EAPOL

LIB_OBJS = eapol_supp_sm.o

libeapol_supp.a: $(LIB_OBJS)
	$(AR) crT $@ $?

-include $(OBJS:%.o=%.d)

all: libl2_packet.a

clean:
	rm -f *~ *.o *.d *.gcno *.gcda *.gcov libl2_packet.a

install:
	@echo Nothing to be made.

include ../lib.rules

LIB_OBJS = l2_packet_linux.o

libl2_packet.a: $(LIB_OBJS)
	$(AR) crT $@ $?

-include $(OBJS:%.o=%.d)

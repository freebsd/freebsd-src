all: libutils.a

clean:
	rm -f *~ *.o *.d *.gcno *.gcda *.gcov libutils.a

install:
	@echo Nothing to be made.


include ../lib.rules

#CFLAGS += -DWPA_TRACE
CFLAGS += -DCONFIG_IPV6
CFLAGS += -DCONFIG_DEBUG_FILE

LIB_OBJS= \
	base64.o \
	bitfield.o \
	common.o \
	crc32.o \
	ip_addr.o \
	json.o \
	radiotap.o \
	trace.o \
	uuid.o \
	wpa_debug.o \
	wpabuf.o

# Pick correct OS wrapper implementation
LIB_OBJS += os_unix.o

# Pick correct event loop implementation
LIB_OBJS += eloop.o

# Pick correct edit implementation
LIB_OBJS += edit.o

#LIB_OBJS += pcsc_funcs.o

libutils.a: $(LIB_OBJS)
	$(AR) crT $@ $?

-include $(OBJS:%.o=%.d)

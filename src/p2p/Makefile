all: libp2p.a

clean:
	rm -f *~ *.o *.d *.gcno *.gcda *.gcov libp2p.a

install:
	@echo Nothing to be made.

include ../lib.rules

CFLAGS += -DCONFIG_WIFI_DISPLAY
CFLAGS += -DCONFIG_WPS_NFC

LIB_OBJS= \
	p2p_build.o \
	p2p.o \
	p2p_dev_disc.o \
	p2p_go_neg.o \
	p2p_group.o \
	p2p_invitation.o \
	p2p_parse.o \
	p2p_pd.o \
	p2p_sd.o \
	p2p_utils.o

libp2p.a: $(LIB_OBJS)
	$(AR) crT $@ $?

-include $(OBJS:%.o=%.d)

all: libcommon.a

clean:
	rm -f *~ *.o *.d *.gcno *.gcda *.gcov libcommon.a

install:
	@echo Nothing to be made.

include ../lib.rules

CFLAGS += -DCONFIG_IEEE80211R
CFLAGS += -DCONFIG_IEEE80211W
CFLAGS += -DCONFIG_HS20
CFLAGS += -DCONFIG_SAE
CFLAGS += -DCONFIG_SUITE
CFLAGS += -DCONFIG_SUITEB

LIB_OBJS= \
	gas.o \
	hw_features_common.o \
	ieee802_11_common.o \
	sae.o \
	wpa_common.o

libcommon.a: $(LIB_OBJS)
	$(AR) crT $@ $?

-include $(OBJS:%.o=%.d)

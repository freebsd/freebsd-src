#
# Makefile for alpha-specific library files..
#

.S.s:
	$(CPP) -D__ASSEMBLY__ $(CFLAGS) -o $*.s $<
.S.o:
	$(CC) -D__ASSEMBLY__ $(CFLAGS) -c -o $*.o $<

# Many of these routines have implementations tuned for ev6.
# Choose them iff we're targeting ev6 specifically.
ev6 :=
ifeq ($(CONFIG_ALPHA_EV6),y)
  ev6 := ev6-
endif

# Several make use of the cttz instruction introduced in ev67.
ev67 :=
ifeq ($(CONFIG_ALPHA_EV67),y)
  ev67 := ev67-
endif

OBJS =	__divqu.o __remqu.o __divlu.o __remlu.o \
	udelay.o \
	$(ev6)memset.o \
	$(ev6)memcpy.o \
	memmove.o \
	io.o \
	checksum.o \
	csum_partial_copy.o \
	$(ev67)strlen.o \
	$(ev67)strcat.o \
	strcpy.o \
	$(ev67)strncat.o \
	strncpy.o \
	$(ev6)stxcpy.o \
	$(ev6)stxncpy.o \
	$(ev67)strchr.o \
	$(ev67)strrchr.o \
	$(ev6)memchr.o \
	$(ev6)copy_user.o \
	$(ev6)clear_user.o \
	$(ev6)strncpy_from_user.o \
	$(ev67)strlen_user.o \
	$(ev6)csum_ipv6_magic.o \
	$(ev6)clear_page.o \
	$(ev6)copy_page.o \
	strcasecmp.o \
	fpreg.o \
	callback_srm.o srm_puts.o srm_printk.o

ifeq ($(CONFIG_SMP),y)
  OBJS += dec_and_lock.o
endif

lib.a: $(OBJS)
	$(AR) rcs lib.a $(OBJS)

__divqu.o: $(ev6)divide.S
	$(CC) $(AFLAGS) -DDIV -c -o __divqu.o $(ev6)divide.S

__remqu.o: $(ev6)divide.S
	$(CC) $(AFLAGS) -DREM -c -o __remqu.o $(ev6)divide.S

__divlu.o: $(ev6)divide.S
	$(CC) $(AFLAGS) -DDIV -DINTSIZE -c -o __divlu.o $(ev6)divide.S

__remlu.o: $(ev6)divide.S
	$(CC) $(AFLAGS) -DREM -DINTSIZE -c -o __remlu.o $(ev6)divide.S

dep:

include $(TOPDIR)/Rules.make

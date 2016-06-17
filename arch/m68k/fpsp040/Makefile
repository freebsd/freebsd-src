#
# Makefile for Linux arch/m68k/fpsp040 source directory
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

.S.o:
	$(CC) $(AFLAGS) -traditional -c -o $*.o $<

OS_TARGET := fpsp.o

OS_OBJS   := bindec.o binstr.o decbin.o do_func.o gen_except.o get_op.o \
	    kernel_ex.o res_func.o round.o sacos.o sasin.o satan.o satanh.o \
	    scosh.o setox.o sgetem.o sint.o slog2.o slogn.o \
	    smovecr.o srem_mod.o scale.o \
	    ssin.o ssinh.o stan.o stanh.o sto_res.o stwotox.o tbldo.o util.o \
	    x_bsun.o x_fline.o x_operr.o x_ovfl.o x_snan.o x_store.o \
	    x_unfl.o x_unimp.o x_unsupp.o bugfix.o skeleton.o

all:	$(OS_TARGET)

include $(TOPDIR)/Rules.make

$(OS_OBJS): fpsp.h

$(OS_TARGET): $(OS_OBJS) fpsp.h
	$(LD) -x -r -o $(OS_TARGET) $(OS_OBJS)

# Target: Remote AMD 29000 that runs Unix kernel on NYU Ultra3 processor board

# This builds a gdb that should run on a host (we use sun3os4) that
# then communicates over the serial line to either an Adapt or MiniMon,
# for use in debugging Unix kernels.
# As compared to ordinary remote 29K debugging, this changes the register
# numbering a bit, to hold kernel regs, and adds support for looking at
# the upage.

TDEPFILES= a29k-tdep.o remote-mm.o remote-adapt.o
TM_FILE= tm-ultra3.h

MT_CFLAGS = -DKERNEL_DEBUGGING -DNO_HIF_SUPPORT

# Target: Big-endian mips board, typically an IDT.
TDEPFILES= mips-tdep.o remote-mips.o dve3900-rom.o monitor.o dsrec.o
TM_FILE= tm-tx39.h
SIM_OBS = remote-sim.o
SIM = ../sim/mips/libsim.a

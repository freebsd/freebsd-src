# Target: Motorola PPC on Linux
TDEPFILES= rs6000-tdep.o ppc-linux-tdep.o solib.o solib-svr4.o solib-legacy.o
TM_FILE= tm-linux.h

SIM_OBS = remote-sim.o
SIM = ../sim/ppc/libsim.a

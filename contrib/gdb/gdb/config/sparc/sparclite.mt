# Target: Fujitsu SPARClite processor
TDEPFILES= sparc-tdep.o sparcl-tdep.o 
TM_FILE= tm-sparclite.h
SIM_OBS = remote-sim.o
SIM = ../sim/erc32/libsim.a

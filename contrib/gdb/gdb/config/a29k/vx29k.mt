# Target: AMD 29k running VxWorks
TDEPFILES= a29k-tdep.o remote-vx.o remote-vx29k.o xdr_ld.o xdr_ptrace.o xdr_rdb.o 
TM_FILE= tm-vx29k.h
MT_CFLAGS = -DNO_HIF_SUPPORT

# This script searches for DAIF instructions which have a register argument and could thus be used to ROP disable debug exceptions
llvm-objdump-14 /home/allison/freebsd/obj/home/allison/freebsd/src/arm64.aarch64/sys/GENERIC/kernel -d | egrep 'msr\s+DAIF, [xw][0-9]+'

#!/bin/sh

# $FreeBSD$

VMMCTL="sudo ./vmmctl"
VMNAME=sample

${VMMCTL} --vm=${VMNAME} --create
${VMMCTL} --vm=${VMNAME} --set-lowmem=128 --set-highmem=256
${VMMCTL} --vm=${VMNAME} --get-lowmem --get-highmem

CR0_PE=$((1 << 0))
CR0_PG=$((1 << 31))
CR0=$(($CR0_PE | $CR0_PG))
${VMMCTL} --vm=${VMNAME} --set-cr0=${CR0} --get-cr0

# XXX this is bogus the value of %cr3 should come from the loader
CR3=0
${VMMCTL} --vm=${VMNAME} --set-cr3=${CR3} --get-cr3

CR4_PAE=$((1 << 5))
CR4=$((${CR4_PAE}))
${VMMCTL} --vm=${VMNAME} --set-cr4=${CR4} --get-cr4

DR7=0x00000400		# Table 9-1 from Intel Architecture Manual 3A
${VMMCTL} --vm=${VMNAME} --set-dr7=${DR7} --get-dr7

#
# XXX the values of rsp and rip are bogus and should come from the loader.
#
RSP=0xa5a5a5a5
RIP=0x0000bfbfbfbf0000
RFLAGS=0x2
${VMMCTL} --vm=${VMNAME} --set-rsp=${RSP} --get-rsp
${VMMCTL} --vm=${VMNAME} --set-rip=${RIP} --get-rip
${VMMCTL} --vm=${VMNAME} --set-rflags=${RFLAGS} --get-rflags

# Set "hidden" state of %cs descriptor to indicate long mode code segment.
#
# Note that this should match the contents of the entry pointed to by the
# segment selector in the GDTR.
#
${VMMCTL} --vm=${VMNAME} --set-desc-cs --desc-access=0x00002098 --get-desc-cs

# Set "hidden" state of all data descriptors to indicate a usable segment.
# The only useful fields are the "Present" and "Descriptor Type" bits.
${VMMCTL} --vm=${VMNAME} --set-desc-ds --desc-access=0x00000090 --get-desc-ds
${VMMCTL} --vm=${VMNAME} --set-desc-es --desc-access=0x00000090 --get-desc-es
${VMMCTL} --vm=${VMNAME} --set-desc-fs --desc-access=0x00000090 --get-desc-fs
${VMMCTL} --vm=${VMNAME} --set-desc-gs --desc-access=0x00000090 --get-desc-gs
${VMMCTL} --vm=${VMNAME} --set-desc-ss --desc-access=0x00000090 --get-desc-ss

#
# Set the code segment selector to point to entry at offset 8 in the GDTR.
#
${VMMCTL} --vm=${VMNAME} --set-cs=0x0008 --get-cs

# Set all the remaining data segment selectors to point to entry at offset
# 16 in the GDTR.
${VMMCTL} --vm=${VMNAME} --set-ds=0x0010 --get-ds
${VMMCTL} --vm=${VMNAME} --set-es=0x0010 --get-es
${VMMCTL} --vm=${VMNAME} --set-fs=0x0010 --get-fs
${VMMCTL} --vm=${VMNAME} --set-gs=0x0010 --get-gs
${VMMCTL} --vm=${VMNAME} --set-ss=0x0010 --get-ss

# XXX the value of the GDTR should come from the loader.
# Set the GDTR
GDTR_BASE=0xffff0000
GDTR_LIMIT=0x10
${VMMCTL} --vm=${VMNAME} --set-desc-gdtr --desc-base=${GDTR_BASE} --desc-limit=${GDTR_LIMIT} --get-desc-gdtr

${VMMCTL} --vm=${VMNAME} --set-pinning=0 --get-pinning
${VMMCTL} --vm=${VMNAME} --set-pinning=-1 --get-pinning

${VMMCTL} --vm=${VMNAME} --destroy

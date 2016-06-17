/*
 *  include/asm-s390/gdb-stub.h
 *
 *  S390 version
 *    Copyright (C) 1999 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Denis Joseph Barrow (djbarrow@de.ibm.com,barrow_dj@yahoo.com)
 */

#ifndef __S390_GDB_STUB__
#define __S390_GDB_STUB__
#include <linux/config.h>
#if CONFIG_REMOTE_DEBUG
#include <asm/s390-gdbregs.h>
#include <asm/ptrace.h>
extern int    gdb_stub_initialised;
extern void gdb_stub_handle_exception(gdb_pt_regs *regs,int sigval);
#endif
#endif

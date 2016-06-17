/*
 *  arch/s390/kernel/ieee.h
 *
 *  S390 version
 *    Copyright (C) 1999 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Martin Schwidefsky (schwidefsky@de.ibm.com),
 */

#include <linux/sched.h>

static    inline void _adddf(int R1,int R2)
{
  current->tss.fprs[R1].fd = current->tss.fprs[R1].fd +
                             current->tss.fprs[R2].fd;
}

static    inline void _subdf(int R1,int R2)
{
  current->tss.fprs[R1].fd = current->tss.fprs[R1].fd -
                             current->tss.fprs[R2].fd;
}

static    inline void _muldf(int R1,int R2)
{
  current->tss.fprs[R1].fd = current->tss.fprs[R1].fd *
                             current->tss.fprs[R2].fd;
}

static    inline void _divdf(int R1,int R2)
{
  current->tss.fprs[R1].fd = current->tss.fprs[R1].fd /
                             current->tss.fprs[R2].fd;
}

static    inline void _negdf(int R1,int R2)
{
  current->tss.fprs[R1].fd = -current->tss.fprs[R1].fd;
}

static    inline void _fixdfsi(int R1,int R2)
{
  current->tss.regs->gprs[R1] = (__u32) current->tss.fprs[R2].fd;
}

static    inline void _extendsidf(int R1,int R2)
{
  current->tss.fprs[R1].fd = (double) current->tss.regs->gprs[R2];
}


static    inline  void _addsf(int R1,int R2)
{
  current->tss.fprs[R1].ff = current->tss.fprs[R1].ff +
                             current->tss.fprs[R2].ff;
}

static    inline  void _subsf(int R1,int R2)
{
  current->tss.fprs[R1].ff = current->tss.fprs[R1].ff -
                             current->tss.fprs[R2].ff;
}

static    inline void _mulsf(int R1,int R2)
{
  current->tss.fprs[R1].ff = current->tss.fprs[R1].ff *
                             current->tss.fprs[R2].ff;
}

static    inline void _divsf(int R1,int R2)
{
  current->tss.fprs[R1].ff = current->tss.fprs[R1].ff /
                             current->tss.fprs[R2].ff;
}

static    inline void _negsf(int R1,int R2)
{
  current->tss.fprs[R1].ff = -current->tss.fprs[R1].ff;
}

static    inline void _fixsfsi(int R1,int R2)
{
  current->tss.regs->gprs[R1] = (__u32) current->tss.fprs[R2].ff;
}

static    inline void _extendsisf(int R1,int R2)
{
  current->tss.fprs[R1].ff = (double) current->tss.regs->gprs[R2];
}



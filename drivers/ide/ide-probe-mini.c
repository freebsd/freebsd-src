/*
 *  linux/drivers/ide/ide-probe-mini.c	Version 1
 *
 *  Copyright (C) 1994-1998  Linus Torvalds & authors (see below)
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/kmod.h>

#ifdef MODULE
extern int ideprobe_init_module(void);

int init_module (void)
{
    return ideprobe_init_module();
}

extern void ideprobe_cleanup_module(void);

void cleanup_module (void)
{
    ideprobe_cleanup_module();
}
MODULE_LICENSE("GPL");
#endif /* MODULE */

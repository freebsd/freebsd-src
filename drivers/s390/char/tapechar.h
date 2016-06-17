
/***************************************************************************
 *
 *  drivers/s390/char/tapechar.h
 *    character device frontend for tape device driver
 *
 *  S390 and zSeries version
 *    Copyright (C) 2001 IBM Corporation
 *    Author(s): Carsten Otte <cotte@de.ibm.com>
 *               Tuan Ngo-Anh <ngoanh@de.ibm.com>
 *
 *
 ****************************************************************************
 */

#ifndef TAPECHAR_H
#define TAPECHAR_H
#include <linux/config.h>
#define TAPECHAR_DEFAULTMODE 0020644
#define  TAPE_MAJOR                    0        /* get dynamic major since no major officialy defined for tape */
/*
 * Prototypes for tape_fops
 */
ssize_t tape_read(struct file *, char *, size_t, loff_t *);
ssize_t tape_write(struct file *, const char *, size_t, loff_t *);
int tape_ioctl(struct inode *,struct file *,unsigned int,unsigned long);
int tape_open (struct inode *,struct file *);
int tape_release (struct inode *,struct file *);
#ifdef CONFIG_DEVFS_FS
void tapechar_mkdevfstree (tape_info_t* ti);
#endif
void tapechar_init (void);
void tapechar_uninit (void);
#endif /* TAPECHAR_H */

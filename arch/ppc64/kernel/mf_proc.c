/*
 * mf_proc.c
 * Copyright (C) 2001 Kyle A. Lucke  IBM Corporation
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */


/* Change Activity: */
/* End Change Activity */

#ifndef _MF_PROC_H
#include <asm/iSeries/mf_proc.h>
#endif
#ifndef MF_H_INCLUDED
#include <asm/iSeries/mf.h>
#endif
#include <asm/uaccess.h>

static struct proc_dir_entry *mf_proc_root = NULL;

int proc_mf_dump_cmdline
(char *page, char **start, off_t off, int count, int *eof, void *data);

int proc_mf_dump_vmlinux
(char *page, char **start, off_t off, int count, int *eof, void *data);

int proc_mf_dump_side
(char *page, char **start, off_t off, int count, int *eof, void *data);

int proc_mf_change_side
(struct file *file, const char *buffer, unsigned long count, void *data);

int proc_mf_dump_src
(char *page, char **start, off_t off, int count, int *eof, void *data);
int proc_mf_change_src (struct file *file, const char *buffer, unsigned long count, void *data);
int proc_mf_change_cmdline(struct file *file, const char *buffer, unsigned long count, void *data);
int proc_mf_change_vmlinux(struct file *file, const char *buffer, unsigned long count, void *data);


void mf_proc_init(struct proc_dir_entry *iSeries_proc)
{
	struct proc_dir_entry *ent = NULL;
	struct proc_dir_entry *mf_a = NULL;
	struct proc_dir_entry *mf_b = NULL;
	struct proc_dir_entry *mf_c = NULL;
	struct proc_dir_entry *mf_d = NULL;

	mf_proc_root = proc_mkdir("mf", iSeries_proc);
	if (!mf_proc_root) return;

	mf_a = proc_mkdir("A", mf_proc_root);
	if (!mf_a) return;

	ent = create_proc_entry("cmdline", S_IFREG|S_IRUSR|S_IWUSR, mf_a);
	if (!ent) return;
	ent->nlink = 1;
	ent->data = (void *)0;
	ent->read_proc = proc_mf_dump_cmdline;
	ent->write_proc = proc_mf_change_cmdline;

	ent = create_proc_entry("vmlinux", S_IFREG|S_IWUSR, mf_a);
	if (!ent) return;
	ent->nlink = 1;
	ent->data = (void *)0;
	ent->write_proc = proc_mf_change_vmlinux;
	ent->read_proc = NULL;

	mf_b = proc_mkdir("B", mf_proc_root);
	if (!mf_b) return;

	ent = create_proc_entry("cmdline", S_IFREG|S_IRUSR|S_IWUSR, mf_b);
	if (!ent) return;
	ent->nlink = 1;
	ent->data = (void *)1;
	ent->read_proc = proc_mf_dump_cmdline;
	ent->write_proc = proc_mf_change_cmdline;

	ent = create_proc_entry("vmlinux", S_IFREG|S_IWUSR, mf_b);
	if (!ent) return;
	ent->nlink = 1;
	ent->data = (void *)1;
	ent->write_proc = proc_mf_change_vmlinux;
	ent->read_proc = NULL;

	mf_c = proc_mkdir("C", mf_proc_root);
	if (!mf_c) return;

	ent = create_proc_entry("cmdline", S_IFREG|S_IRUSR|S_IWUSR, mf_c);
	if (!ent) return;
	ent->nlink = 1;
	ent->data = (void *)2;
	ent->read_proc = proc_mf_dump_cmdline;
	ent->write_proc = proc_mf_change_cmdline;

	ent = create_proc_entry("vmlinux", S_IFREG|S_IWUSR, mf_c);
	if (!ent) return;
	ent->nlink = 1;
	ent->data = (void *)2;
	ent->write_proc = proc_mf_change_vmlinux;
	ent->read_proc = NULL;

	mf_d = proc_mkdir("D", mf_proc_root);
	if (!mf_d) return;


	ent = create_proc_entry("cmdline", S_IFREG|S_IRUSR|S_IWUSR, mf_d);
	if (!ent) return;
	ent->nlink = 1;
	ent->data = (void *)3;
	ent->read_proc = proc_mf_dump_cmdline;
	ent->write_proc = proc_mf_change_cmdline;
#if 0
	ent = create_proc_entry("vmlinux", S_IFREG|S_IRUSR, mf_d);
	if (!ent) return;
	ent->nlink = 1;
	ent->data = (void *)3;
	ent->read_proc = proc_mf_dump_vmlinux;
	ent->write_proc = NULL;
#endif
	ent = create_proc_entry("side", S_IFREG|S_IRUSR|S_IWUSR, mf_proc_root);
	if (!ent) return;
	ent->nlink = 1;
	ent->data = (void *)0;
	ent->read_proc = proc_mf_dump_side;
	ent->write_proc = proc_mf_change_side;

	ent = create_proc_entry("src", S_IFREG|S_IRUSR|S_IWUSR, mf_proc_root);
	if (!ent) return;
	ent->nlink = 1;
	ent->data = (void *)0;
	ent->read_proc = proc_mf_dump_src;
	ent->write_proc = proc_mf_change_src;
}

int proc_mf_dump_cmdline
(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int		len = count;
	char *p;
    
	len = mf_getCmdLine(page, &len, (u64)data);
   
	p = page + len - 1;
	while ( p > page ) {
		if ( (*p == 0) || (*p == ' ') )
			--p;
		else
			break;
	}
	if ( *p != '\n' ) {
		++p;
		*p = '\n';
	}
	++p;
	*p = 0;
	len = p - page;
    
	len -= off;			
	if (len < count) {		
		*eof = 1;		
		if (len <= 0)		
			return 0;	
	} else				
		len = count;		
	*start = page + off;		
	return len;			
}

int proc_mf_dump_vmlinux
(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int sizeToGet = count;
	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	if (mf_getVmlinuxChunk(page, &sizeToGet, off, (u64)data) == 0)
	{
		if (sizeToGet != 0)
		{
			*start = page + off;
			return sizeToGet;
		} else {
			*eof = 1;
			return 0;
		}
	} else {
		*eof = 1;
		return 0;
	}
}

int proc_mf_dump_side
(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int		len = 0;

	char mf_current_side = mf_getSide();
	len = sprintf(page, "%c\n", mf_current_side);

	if (len <= off+count) *eof = 1;
	*start = page + off;
	len -= off;
	if (len>count) len = count;
	if (len<0) len = 0;
	return len;			
}

int proc_mf_change_side(struct file *file, const char *buffer, unsigned long count, void *data)
{
	char side;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;
	if (count == 0)
		return 0;
	if (get_user(side, buffer))
		return -EFAULT;

	if ((side != 'A') && (side != 'B') && (side != 'C') && (side != 'D'))
	{
		printk(KERN_ERR "mf_proc.c: proc_mf_change_side: invalid side\n");
		return -EINVAL;
	}

	mf_setSide(side);

	return count;			
}

int proc_mf_dump_src
(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int		len = 0;
	mf_getSrcHistory(page, count);
	len = count;
	len -= off;			
	if (len < count) {		
		*eof = 1;		
		if (len <= 0)		
			return 0;	
	} else				
		len = count;		
	*start = page + off;		
	return len;			
}

int proc_mf_change_src(struct file *file, const char *buffer, unsigned long count, void *data)
{
	char stkbuf[10];
	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	if ((count < 4) && (count != 1)) {
		printk(KERN_ERR "mf_proc: invalid src\n");
		return -EINVAL;
	}

	if (count > 9)
		count = 9;
	if (copy_from_user (stkbuf, buffer, count))
		return -EFAULT;

	if ((count == 1) && ((*stkbuf) == '\0')) {
		mf_clearSrc();
	} else {
		mf_displaySrc(*(u32 *)stkbuf);
	}

	return count;			
}

int proc_mf_change_cmdline(struct file *file, const char *buffer, unsigned long count, void *data)
{
	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	mf_setCmdLine(buffer, count, (u64)data);

	return count;			
}

int proc_mf_change_vmlinux(struct file *file, const char *buffer, unsigned long count, void *data)
{
	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	mf_setVmlinuxChunk(buffer, count, file->f_pos, (u64)data);
	file->f_pos += count;

	return count;			
}

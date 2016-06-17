/* $Id: inode.c,v 1.15 2001/11/12 09:43:39 davem Exp $
 * openpromfs.c: /proc/openprom handling routines
 *
 * Copyright (C) 1996-1999 Jakub Jelinek  (jakub@redhat.com)
 * Copyright (C) 1998      Eddie C. Dost  (ecd@skynet.be)
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/openprom_fs.h>
#include <linux/locks.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>

#include <asm/openprom.h>
#include <asm/oplib.h>
#include <asm/uaccess.h>

#define ALIASES_NNODES 64

typedef struct {
	u16	parent;
	u16	next;
	u16	child;
	u16	first_prop;
	u32	node;
} openpromfs_node;

typedef struct {
#define OPP_STRING	0x10
#define OPP_STRINGLIST	0x20
#define OPP_BINARY	0x40
#define OPP_HEXSTRING	0x80
#define OPP_DIRTY	0x01
#define OPP_QUOTED	0x02
#define OPP_NOTQUOTED	0x04
#define OPP_ASCIIZ	0x08
	u32	flag;
	u32	alloclen;
	u32	len;
	char	*value;
	char	name[8];
} openprom_property;

static openpromfs_node *nodes = NULL;
static int alloced = 0;
static u16 last_node = 0;
static u16 first_prop = 0;
static u16 options = 0xffff;
static u16 aliases = 0xffff;
static int aliases_nodes = 0;
static char *alias_names [ALIASES_NNODES];

#define OPENPROM_ROOT_INO	16
#define OPENPROM_FIRST_INO	OPENPROM_ROOT_INO
#define NODE(ino) nodes[ino - OPENPROM_FIRST_INO]
#define NODE2INO(node) (node + OPENPROM_FIRST_INO)
#define NODEP2INO(no) (no + OPENPROM_FIRST_INO + last_node)

static int openpromfs_create (struct inode *, struct dentry *, int);
static int openpromfs_readdir(struct file *, void *, filldir_t);
static struct dentry *openpromfs_lookup(struct inode *, struct dentry *dentry);
static int openpromfs_unlink (struct inode *, struct dentry *dentry);

static ssize_t nodenum_read(struct file *file, char *buf,
			    size_t count, loff_t *ppos)
{
	struct inode *inode = file->f_dentry->d_inode;
	char buffer[10];
	
	if (count < 0 || !inode->u.generic_ip)
		return -EINVAL;
	sprintf (buffer, "%8.8x\n", (u32)(long)(inode->u.generic_ip));
	if (file->f_pos >= 9)
		return 0;
	if (count > 9 - file->f_pos)
		count = 9 - file->f_pos;
	copy_to_user(buf, buffer + file->f_pos, count);
	file->f_pos += count;
	return count;
}

static ssize_t property_read(struct file *filp, char *buf,
			     size_t count, loff_t *ppos)
{
	struct inode *inode = filp->f_dentry->d_inode;
	int i, j, k;
	u32 node;
	char *p, *s;
	u32 *q;
	openprom_property *op;
	char buffer[64];
	
	if (filp->f_pos >= 0xffffff)
		return -EINVAL;
	if (!filp->private_data) {
		node = nodes[(u16)((long)inode->u.generic_ip)].node;
		i = ((u32)(long)inode->u.generic_ip) >> 16;
		if ((u16)((long)inode->u.generic_ip) == aliases) {
			if (i >= aliases_nodes)
				p = 0;
			else
				p = alias_names [i];
		} else
			for (p = prom_firstprop (node, buffer);
			     i && p && *p;
			     p = prom_nextprop (node, p, buffer), i--)
				/* nothing */ ;
		if (!p || !*p)
			return -EIO;
		i = prom_getproplen (node, p);
		if (i < 0) {
			if ((u16)((long)inode->u.generic_ip) == aliases)
				i = 0;
			else
				return -EIO;
		}
		k = i;
		if (i < 64) i = 64;
		filp->private_data = kmalloc (sizeof (openprom_property)
					      + (j = strlen (p)) + 2 * i,
					      GFP_KERNEL);
		if (!filp->private_data)
			return -ENOMEM;
		op = (openprom_property *)filp->private_data;
		op->flag = 0;
		op->alloclen = 2 * i;
		strcpy (op->name, p);
		op->value = (char *)(((unsigned long)(op->name + j + 4)) & ~3);
		op->len = k;
		if (k && prom_getproperty (node, p, op->value, i) < 0)
			return -EIO;
		op->value [k] = 0;
		if (k) {
			for (s = 0, p = op->value; p < op->value + k; p++) {
				if ((*p >= ' ' && *p <= '~') || *p == '\n') {
					op->flag |= OPP_STRING;
					s = p;
					continue;
				}
				if (p > op->value && !*p && s == p - 1) {
					if (p < op->value + k - 1)
						op->flag |= OPP_STRINGLIST;
					else
						op->flag |= OPP_ASCIIZ;
					continue;
				}
				if (k == 1 && !*p) {
					op->flag |= (OPP_STRING|OPP_ASCIIZ);
					break;
				}
				op->flag &= ~(OPP_STRING|OPP_STRINGLIST);
				if (k & 3)
					op->flag |= OPP_HEXSTRING;
				else
					op->flag |= OPP_BINARY;
				break;
			}
			if (op->flag & OPP_STRINGLIST)
				op->flag &= ~(OPP_STRING);
			if (op->flag & OPP_ASCIIZ)
				op->len--;
		}
	} else
		op = (openprom_property *)filp->private_data;
	if (!count || !(op->len || (op->flag & OPP_ASCIIZ)))
		return 0;
	if (op->flag & OPP_STRINGLIST) {
		for (k = 0, p = op->value; p < op->value + op->len; p++)
			if (!*p)
				k++;
		i = op->len + 4 * k + 3;
	} else if (op->flag & OPP_STRING) {
		i = op->len + 3;
	} else if (op->flag & OPP_BINARY) {
		i = (op->len * 9) >> 2;
	} else {
		i = (op->len << 1) + 1;
	}
	k = filp->f_pos;
	if (k >= i) return 0;
	if (count > i - k) count = i - k;
	if (op->flag & OPP_STRING) {
		if (!k) {
			__put_user('\'', buf);
			k++;
			count--;
		}

		if (k + count >= i - 2)
			j = i - 2 - k;
		else
			j = count;

		if (j >= 0) {
			copy_to_user(buf + k - filp->f_pos,
				     op->value + k - 1, j);
			count -= j;
			k += j;
		}

		if (count)
			__put_user('\'', &buf [k++ - filp->f_pos]);
		if (count > 1)
			__put_user('\n', &buf [k++ - filp->f_pos]);

	} else if (op->flag & OPP_STRINGLIST) {
		char *tmp;

		tmp = kmalloc (i, GFP_KERNEL);
		if (!tmp)
			return -ENOMEM;

		s = tmp;
		*s++ = '\'';
		for (p = op->value; p < op->value + op->len; p++) {
			if (!*p) {
				strcpy(s, "' + '");
				s += 5;
				continue;
			}
			*s++ = *p;
		}
		strcpy(s, "'\n");

		copy_to_user(buf, tmp + k, count);

		kfree(tmp);
		k += count;

	} else if (op->flag & OPP_BINARY) {
		char buffer[10];
		u32 *first, *last;
		int first_off, last_cnt;

		first = ((u32 *)op->value) + k / 9;
		first_off = k % 9;
		last = ((u32 *)op->value) + (k + count - 1) / 9;
		last_cnt = (k + count) % 9;
		if (!last_cnt) last_cnt = 9;

		if (first == last) {
			sprintf (buffer, "%08x.", *first);
			copy_to_user (buf, buffer + first_off, last_cnt - first_off);
			buf += last_cnt - first_off;
		} else {		
			for (q = first; q <= last; q++) {
				sprintf (buffer, "%08x.", *q);
				if (q == first) {
					copy_to_user (buf, buffer + first_off,
						      9 - first_off);
					buf += 9 - first_off;
				} else if (q == last) {
					copy_to_user (buf, buffer, last_cnt);
					buf += last_cnt;
				} else {
					copy_to_user (buf, buffer, 9);
					buf += 9;
				}
			}
		}

		if (last == (u32 *)(op->value + op->len - 4) && last_cnt == 9)
			__put_user('\n', (buf - 1));

		k += count;

	} else if (op->flag & OPP_HEXSTRING) {
		char buffer[2];

		if ((k < i - 1) && (k & 1)) {
			sprintf (buffer, "%02x", *(op->value + (k >> 1)));
			__put_user(buffer[1], &buf[k++ - filp->f_pos]);
			count--;
		}

		for (; (count > 1) && (k < i - 1); k += 2) {
			sprintf (buffer, "%02x", *(op->value + (k >> 1)));
			copy_to_user (buf + k - filp->f_pos, buffer, 2);
			count -= 2;
		}

		if (count && (k < i - 1)) {
			sprintf (buffer, "%02x", *(op->value + (k >> 1)));
			__put_user(buffer[0], &buf[k++ - filp->f_pos]);
			count--;
		}

		if (count)
			__put_user('\n', &buf [k++ - filp->f_pos]);
	}
	count = k - filp->f_pos;
	filp->f_pos = k;
	return count;
}

static ssize_t property_write(struct file *filp, const char *buf,
			      size_t count, loff_t *ppos)
{
	int i, j, k;
	char *p;
	u32 *q;
	void *b;
	openprom_property *op;
	
	if (filp->f_pos >= 0xffffff)
		return -EINVAL;
	if (!filp->private_data) {
		i = property_read (filp, NULL, 0, 0);
		if (i)
			return i;
	}
	k = filp->f_pos;
	op = (openprom_property *)filp->private_data;
	if (!(op->flag & OPP_STRING)) {
		u32 *first, *last;
		int first_off, last_cnt;
		u32 mask, mask2;
		char tmp [9];
		int forcelen = 0;
		
		j = k % 9;
		for (i = 0; i < count; i++, j++) {
			if (j == 9) j = 0;
			if (!j) {
				char ctmp;
				__get_user(ctmp, &buf[i]);
				if (ctmp != '.') {
					if (ctmp != '\n') {
						if (op->flag & OPP_BINARY)
							return -EINVAL;
						else
							goto write_try_string;
					} else {
						count = i + 1;
						forcelen = 1;
						break;
					}
				}
			} else {
				char ctmp;
				__get_user(ctmp, &buf[i]);
				if (ctmp < '0' || 
				    (ctmp > '9' && ctmp < 'A') ||
				    (ctmp > 'F' && ctmp < 'a') ||
				    ctmp > 'f') {
					if (op->flag & OPP_BINARY)
						return -EINVAL;
					else
						goto write_try_string;
				}
			}
		}
		op->flag |= OPP_BINARY;
		tmp [8] = 0;
		i = ((count + k + 8) / 9) << 2;
		if (op->alloclen <= i) {
			b = kmalloc (sizeof (openprom_property) + 2 * i,
				     GFP_KERNEL);
			if (!b)
				return -ENOMEM;
			memcpy (b, filp->private_data,
				sizeof (openprom_property)
				+ strlen (op->name) + op->alloclen);
			memset (((char *)b) + sizeof (openprom_property)
				+ strlen (op->name) + op->alloclen, 
				0, 2 * i - op->alloclen);
			op = (openprom_property *)b;
			op->alloclen = 2*i;
			b = filp->private_data;
			filp->private_data = (void *)op;
			kfree (b);
		}
		first = ((u32 *)op->value) + (k / 9);
		first_off = k % 9;
		last = (u32 *)(op->value + i);
		last_cnt = (k + count) % 9;
		if (first + 1 == last) {
			memset (tmp, '0', 8);
			copy_from_user (tmp + first_off, buf,
					(count + first_off > 8) ? 8 - first_off : count);
			mask = 0xffffffff;
			mask2 = 0xffffffff;
			for (j = 0; j < first_off; j++)
				mask >>= 1;
			for (j = 8 - count - first_off; j > 0; j--)
				mask2 <<= 1;
			mask &= mask2;
			if (mask) {
				*first &= ~mask;
				*first |= simple_strtoul (tmp, 0, 16);
				op->flag |= OPP_DIRTY;
			}
		} else {
			op->flag |= OPP_DIRTY;
			for (q = first; q < last; q++) {
				if (q == first) {
					if (first_off < 8) {
						memset (tmp, '0', 8);
						copy_from_user (tmp + first_off, buf,
								8 - first_off);
						mask = 0xffffffff;
						for (j = 0; j < first_off; j++)
							mask >>= 1;
						*q &= ~mask;
						*q |= simple_strtoul (tmp,0,16);
					}
					buf += 9;
				} else if ((q == last - 1) && last_cnt
					   && (last_cnt < 8)) {
					memset (tmp, '0', 8);
					copy_from_user (tmp, buf, last_cnt);
					mask = 0xffffffff;
					for (j = 0; j < 8 - last_cnt; j++)
						mask <<= 1;
					*q &= ~mask;
					*q |= simple_strtoul (tmp, 0, 16);
					buf += last_cnt;
				} else {
					char tchars[17]; /* XXX yuck... */

					copy_from_user(tchars, buf, 16);
					*q = simple_strtoul (tchars, 0, 16);
					buf += 9;
				}
			}
		}
		if (!forcelen) {
			if (op->len < i)
				op->len = i;
		} else
			op->len = i;
		filp->f_pos += count;
	}
write_try_string:
	if (!(op->flag & OPP_BINARY)) {
		if (!(op->flag & (OPP_QUOTED | OPP_NOTQUOTED))) {
			char ctmp;

			/* No way, if somebody starts writing from the middle, 
			 * we don't know whether he uses quotes around or not 
			 */
			if (k > 0)
				return -EINVAL;
			__get_user(ctmp, buf);
			if (ctmp == '\'') {
				op->flag |= OPP_QUOTED;
				buf++;
				count--;
				filp->f_pos++;
				if (!count) {
					op->flag |= OPP_STRING;
					return 1;
				}
			} else
				op->flag |= OPP_NOTQUOTED;
		}
		op->flag |= OPP_STRING;
		if (op->alloclen <= count + filp->f_pos) {
			b = kmalloc (sizeof (openprom_property)
				     + 2 * (count + filp->f_pos), GFP_KERNEL);
			if (!b)
				return -ENOMEM;
			memcpy (b, filp->private_data,
				sizeof (openprom_property)
				+ strlen (op->name) + op->alloclen);
			memset (((char *)b) + sizeof (openprom_property)
				+ strlen (op->name) + op->alloclen, 
				0, 2*(count - filp->f_pos) - op->alloclen);
			op = (openprom_property *)b;
			op->alloclen = 2*(count + filp->f_pos);
			b = filp->private_data;
			filp->private_data = (void *)op;
			kfree (b);
		}
		p = op->value + filp->f_pos - ((op->flag & OPP_QUOTED) ? 1 : 0);
		copy_from_user (p, buf, count);
		op->flag |= OPP_DIRTY;
		for (i = 0; i < count; i++, p++)
			if (*p == '\n') {
				*p = 0;
				break;
			}
		if (i < count) {
			op->len = p - op->value;
			filp->f_pos += i + 1;
			if ((p > op->value) && (op->flag & OPP_QUOTED)
			    && (*(p - 1) == '\''))
				op->len--;
		} else {
			if (p - op->value > op->len)
				op->len = p - op->value;
			filp->f_pos += count;
		}
	}
	return filp->f_pos - k;
}

int property_release (struct inode *inode, struct file *filp)
{
	openprom_property *op = (openprom_property *)filp->private_data;
	unsigned long flags;
	int error;
	u32 node;
	
	if (!op)
		return 0;
	lock_kernel();
	node = nodes[(u16)((long)inode->u.generic_ip)].node;
	if ((u16)((long)inode->u.generic_ip) == aliases) {
		if ((op->flag & OPP_DIRTY) && (op->flag & OPP_STRING)) {
			char *p = op->name;
			int i = (op->value - op->name) - strlen (op->name) - 1;
			op->value [op->len] = 0;
			*(op->value - 1) = ' ';
			if (i) {
				for (p = op->value - i - 2; p >= op->name; p--)
					p[i] = *p;
				p = op->name + i;
			}
			memcpy (p - 8, "nvalias ", 8);
			prom_feval (p - 8);
		}
	} else if (op->flag & OPP_DIRTY) {
		if (op->flag & OPP_STRING) {
			op->value [op->len] = 0;
			save_and_cli (flags);
			error = prom_setprop (node, op->name,
					      op->value, op->len + 1);
			restore_flags (flags);
			if (error <= 0)
				printk (KERN_WARNING "openpromfs: "
					"Couldn't write property %s\n",
					op->name);
		} else if ((op->flag & OPP_BINARY) || !op->len) {
			save_and_cli (flags);
			error = prom_setprop (node, op->name,
					      op->value, op->len);
			restore_flags (flags);
			if (error <= 0)
				printk (KERN_WARNING "openpromfs: "
					"Couldn't write property %s\n",
					op->name);
		} else {
			printk (KERN_WARNING "openpromfs: "
				"Unknown property type of %s\n",
				op->name);
		}
	}
	unlock_kernel();
	kfree (filp->private_data);
	return 0;
}

static struct file_operations openpromfs_prop_ops = {
	read:		property_read,
	write:		property_write,
	release:	property_release,
};

static struct file_operations openpromfs_nodenum_ops = {
	read:		nodenum_read,
};

static struct file_operations openprom_operations = {
	read:		generic_read_dir,
	readdir:	openpromfs_readdir,
};

static struct inode_operations openprom_alias_inode_operations = {
	create:		openpromfs_create,
	lookup:		openpromfs_lookup,
	unlink:		openpromfs_unlink,
};

static struct inode_operations openprom_inode_operations = {
	lookup:		openpromfs_lookup,
};

static int lookup_children(u16 n, const char * name, int len)
{
	int ret;
	u16 node;
	for (; n != 0xffff; n = nodes[n].next) {
		node = nodes[n].child;
		if (node != 0xffff) {
			char buffer[128];
			int i;
			char *p;
			
			while (node != 0xffff) {
				if (prom_getname (nodes[node].node,
						  buffer, 128) >= 0) {
					i = strlen (buffer);
					if ((len == i)
					    && !strncmp (buffer, name, len))
						return NODE2INO(node);
					p = strchr (buffer, '@');
					if (p && (len == p - buffer)
					    && !strncmp (buffer, name, len))
						return NODE2INO(node);
				}
				node = nodes[node].next;
			}
		} else
			continue;
		ret = lookup_children (nodes[n].child, name, len);
		if (ret) return ret;
	}
	return 0;
}

static struct dentry *openpromfs_lookup(struct inode * dir, struct dentry *dentry)
{
	int ino = 0;
#define OPFSL_DIR	0
#define OPFSL_PROPERTY	1
#define OPFSL_NODENUM	2
	int type = 0;
	char buffer[128];
	char *p;
	const char *name;
	u32 n;
	u16 dirnode;
	unsigned int len;
	int i;
	struct inode *inode;
	char buffer2[64];
	
	inode = NULL;
	name = dentry->d_name.name;
	len = dentry->d_name.len;
	if (name [0] == '.' && len == 5 && !strncmp (name + 1, "node", 4)) {
		ino = NODEP2INO(NODE(dir->i_ino).first_prop);
		type = OPFSL_NODENUM;
	}
	if (!ino) {
		u16 node = NODE(dir->i_ino).child;
		while (node != 0xffff) {
			if (prom_getname (nodes[node].node, buffer, 128) >= 0) {
				i = strlen (buffer);
				if (len == i && !strncmp (buffer, name, len)) {
					ino = NODE2INO(node);
					type = OPFSL_DIR;
					break;
				}
				p = strchr (buffer, '@');
				if (p && (len == p - buffer)
				    && !strncmp (buffer, name, len)) {
					ino = NODE2INO(node);
					type = OPFSL_DIR;
					break;
				}
			}
			node = nodes[node].next;
		}
	}
	n = NODE(dir->i_ino).node;
	dirnode = dir->i_ino - OPENPROM_FIRST_INO;
	if (!ino) {
		int j = NODEP2INO(NODE(dir->i_ino).first_prop);
		if (dirnode != aliases) {
			for (p = prom_firstprop (n, buffer2);
			     p && *p;
			     p = prom_nextprop (n, p, buffer2)) {
				j++;
				if ((len == strlen (p))
				    && !strncmp (p, name, len)) {
					ino = j;
					type = OPFSL_PROPERTY;
					break;
				}
			}
		} else {
			int k;
			for (k = 0; k < aliases_nodes; k++) {
				j++;
				if (alias_names [k]
				    && (len == strlen (alias_names [k]))
				    && !strncmp (alias_names [k], name, len)) {
					ino = j;
					type = OPFSL_PROPERTY;
					break;
				}
			}
		}
	}
	if (!ino) {
		ino = lookup_children (NODE(dir->i_ino).child, name, len);
		if (ino)
			type = OPFSL_DIR;
		else
			return ERR_PTR(-ENOENT);
	}
	inode = iget (dir->i_sb, ino);
	if (!inode)
		return ERR_PTR(-EINVAL);
	switch (type) {
	case OPFSL_DIR:
		inode->i_mode = S_IFDIR | S_IRUGO | S_IXUGO;
		if (ino == OPENPROM_FIRST_INO + aliases) {
			inode->i_mode |= S_IWUSR;
			inode->i_op = &openprom_alias_inode_operations;
		} else
			inode->i_op = &openprom_inode_operations;
		inode->i_fop = &openprom_operations;
		inode->i_nlink = 2;
		break;
	case OPFSL_NODENUM:
		inode->i_mode = S_IFREG | S_IRUGO;
		inode->i_fop = &openpromfs_nodenum_ops;
		inode->i_nlink = 1;
		inode->u.generic_ip = (void *)(long)(n);
		break;
	case OPFSL_PROPERTY:
		if ((dirnode == options) && (len == 17)
		    && !strncmp (name, "security-password", 17))
			inode->i_mode = S_IFREG | S_IRUSR | S_IWUSR;
		else {
			inode->i_mode = S_IFREG | S_IRUGO;
			if (dirnode == options || dirnode == aliases) {
				if (len != 4 || strncmp (name, "name", 4))
					inode->i_mode |= S_IWUSR;
			}
		}
		inode->i_fop = &openpromfs_prop_ops;
		inode->i_nlink = 1;
		if (inode->i_size < 0)
			inode->i_size = 0;
		inode->u.generic_ip = (void *)(long)(((u16)dirnode) | 
			(((u16)(ino - NODEP2INO(NODE(dir->i_ino).first_prop) - 1)) << 16));
		break;
	}

	inode->i_gid = 0;
	inode->i_uid = 0;

	d_add(dentry, inode);
	return NULL;
}

static int openpromfs_readdir(struct file * filp, void * dirent, filldir_t filldir)
{
	struct inode *inode = filp->f_dentry->d_inode;
	unsigned int ino;
	u32 n;
	int i, j;
	char buffer[128];
	u16 node;
	char *p;
	char buffer2[64];
	
	ino = inode->i_ino;
	i = filp->f_pos;
	switch (i) {
	case 0:
		if (filldir(dirent, ".", 1, i, ino, DT_DIR) < 0) return 0;
		i++;
		filp->f_pos++;
		/* fall thru */
	case 1:
		if (filldir(dirent, "..", 2, i, 
			(NODE(ino).parent == 0xffff) ? 
			OPENPROM_ROOT_INO : NODE2INO(NODE(ino).parent), DT_DIR) < 0) 
			return 0;
		i++;
		filp->f_pos++;
		/* fall thru */
	default:
		i -= 2;
		node = NODE(ino).child;
		while (i && node != 0xffff) {
			node = nodes[node].next;
			i--;
		}
		while (node != 0xffff) {
			if (prom_getname (nodes[node].node, buffer, 128) < 0)
				return 0;
			if (filldir(dirent, buffer, strlen(buffer),
				    filp->f_pos, NODE2INO(node), DT_DIR) < 0)
				return 0;
			filp->f_pos++;
			node = nodes[node].next;
		}
		j = NODEP2INO(NODE(ino).first_prop);
		if (!i) {
			if (filldir(dirent, ".node", 5, filp->f_pos, j, DT_REG) < 0)
				return 0;
			filp->f_pos++;
		} else
			i--;
		n = NODE(ino).node;
		if (ino == OPENPROM_FIRST_INO + aliases) {
			for (j++; i < aliases_nodes; i++, j++) {
				if (alias_names [i]) {
					if (filldir (dirent, alias_names [i], 
						strlen (alias_names [i]), 
						filp->f_pos, j, DT_REG) < 0) return 0;
					filp->f_pos++;
				}
			}
		} else {
			for (p = prom_firstprop (n, buffer2);
			     p && *p;
			     p = prom_nextprop (n, p, buffer2)) {
				j++;
				if (i) i--;
				else {
					if (filldir(dirent, p, strlen(p),
						    filp->f_pos, j, DT_REG) < 0)
						return 0;
					filp->f_pos++;
				}
			}
		}
	}
	return 0;
}

static int openpromfs_create (struct inode *dir, struct dentry *dentry, int mode)
{
	char *p;
	struct inode *inode;
	
	if (!dir)
		return -ENOENT;
	if (dentry->d_name.len > 256)
		return -EINVAL;
	if (aliases_nodes == ALIASES_NNODES)
		return -EIO;
	p = kmalloc (dentry->d_name.len + 1, GFP_KERNEL);
	if (!p)
		return -ENOMEM;
	strncpy (p, dentry->d_name.name, dentry->d_name.len);
	p [dentry->d_name.len] = 0;
	alias_names [aliases_nodes++] = p;
	inode = iget (dir->i_sb,
			NODEP2INO(NODE(dir->i_ino).first_prop) + aliases_nodes);
	if (!inode)
		return -EINVAL;
	inode->i_mode = S_IFREG | S_IRUGO | S_IWUSR;
	inode->i_fop = &openpromfs_prop_ops;
	inode->i_nlink = 1;
	if (inode->i_size < 0) inode->i_size = 0;
	inode->u.generic_ip = (void *)(long)(((u16)aliases) | 
			(((u16)(aliases_nodes - 1)) << 16));
	d_instantiate(dentry, inode);
	return 0;
}

static int openpromfs_unlink (struct inode *dir, struct dentry *dentry)
{
	unsigned int len;
	char *p;
	const char *name;
	int i;
	
	name = dentry->d_name.name;
	len = dentry->d_name.len;
	for (i = 0; i < aliases_nodes; i++)
		if ((strlen (alias_names [i]) == len)
		    && !strncmp (name, alias_names[i], len)) {
			char buffer[512];
			
			p = alias_names [i];
			alias_names [i] = NULL;
			kfree (p);
			strcpy (buffer, "nvunalias ");
			memcpy (buffer + 10, name, len);
			buffer [10 + len] = 0;
			prom_feval (buffer);
		}
	return 0;
}

/* {{{ init section */
#ifndef MODULE
static int __init check_space (u16 n)
#else
static int check_space (u16 n)
#endif
{
	unsigned long pages;

	if ((1 << alloced) * PAGE_SIZE < (n + 2) * sizeof(openpromfs_node)) {
		pages = __get_free_pages (GFP_KERNEL, alloced + 1);
		if (!pages)
			return -1;

		if (nodes) {
			memcpy ((char *)pages, (char *)nodes,
				(1 << alloced) * PAGE_SIZE);
			free_pages ((unsigned long)nodes, alloced);
		}
		alloced++;
		nodes = (openpromfs_node *)pages;
	}
	return 0;
}

#ifndef MODULE
static u16 __init get_nodes (u16 parent, u32 node)
#else
static u16 get_nodes (u16 parent, u32 node)
#endif
{
	char *p;
	u16 n = last_node++, i;
	char buffer[64];

	if (check_space (n) < 0)
		return 0xffff;
	nodes[n].parent = parent;
	nodes[n].node = node;
	nodes[n].next = 0xffff;
	nodes[n].child = 0xffff;
	nodes[n].first_prop = first_prop++;
	if (!parent) {
		char buffer[8];
		int j;
		
		if ((j = prom_getproperty (node, "name", buffer, 8)) >= 0) {
		    buffer[j] = 0;
		    if (!strcmp (buffer, "options"))
			options = n;
		    else if (!strcmp (buffer, "aliases"))
		        aliases = n;
		}
	}
	if (n != aliases)
		for (p = prom_firstprop (node, buffer);
		     p && p != (char *)-1 && *p;
		     p = prom_nextprop (node, p, buffer))
			first_prop++;
	else {
		char *q;
		for (p = prom_firstprop (node, buffer);
		     p && p != (char *)-1 && *p;
		     p = prom_nextprop (node, p, buffer)) {
			if (aliases_nodes == ALIASES_NNODES)
				break;
			for (i = 0; i < aliases_nodes; i++)
				if (!strcmp (p, alias_names [i]))
					break;
			if (i < aliases_nodes)
				continue;
			q = kmalloc (strlen (p) + 1, GFP_KERNEL);
			if (!q)
				return 0xffff;
			strcpy (q, p);
			alias_names [aliases_nodes++] = q;
		}
		first_prop += ALIASES_NNODES;
	}
	node = prom_getchild (node);
	if (node) {
		parent = get_nodes (n, node);
		if (parent == 0xffff)
			return 0xffff;
		nodes[n].child = parent;
		while ((node = prom_getsibling (node)) != 0) {
			i = get_nodes (n, node);
			if (i == 0xffff)
				return 0xffff;
			nodes[parent].next = i;
			parent = i;
		}
	}
	return n;
}

static void openprom_read_inode(struct inode * inode)
{
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
	if (inode->i_ino == OPENPROM_ROOT_INO) {
		inode->i_op = &openprom_inode_operations;
		inode->i_fop = &openprom_operations;
		inode->i_mode = S_IFDIR | S_IRUGO | S_IXUGO;
	}
}

static int openprom_statfs(struct super_block *sb, struct statfs *buf)
{
	buf->f_type = OPENPROM_SUPER_MAGIC;
	buf->f_bsize = PAGE_SIZE/sizeof(long);	/* ??? */
	buf->f_bfree = 0;
	buf->f_bavail = 0;
	buf->f_ffree = 0;
	buf->f_namelen = NAME_MAX;
	return 0;
}

static struct super_operations openprom_sops = { 
	read_inode:	openprom_read_inode,
	statfs:		openprom_statfs,
};

struct super_block *openprom_read_super(struct super_block *s,void *data, 
				    int silent)
{
	struct inode * root_inode;

	s->s_blocksize = 1024;
	s->s_blocksize_bits = 10;
	s->s_magic = OPENPROM_SUPER_MAGIC;
	s->s_op = &openprom_sops;
	root_inode = iget(s, OPENPROM_ROOT_INO);
	if (!root_inode)
		goto out_no_root;
	s->s_root = d_alloc_root(root_inode);
	if (!s->s_root)
		goto out_no_root;
	return s;

out_no_root:
	printk("openprom_read_super: get root inode failed\n");
	iput(root_inode);
	return NULL;
}

static DECLARE_FSTYPE(openprom_fs_type, "openpromfs", openprom_read_super, 0);

static int __init init_openprom_fs(void)
{
	nodes = (openpromfs_node *)__get_free_pages(GFP_KERNEL, 0);
	if (!nodes) {
		printk (KERN_WARNING "openpromfs: can't get free page\n");
		return -EIO;
	}
	if (get_nodes (0xffff, prom_root_node) == 0xffff) {
		printk (KERN_WARNING "openpromfs: couldn't setup tree\n");
		return -EIO;
	}
	nodes[last_node].first_prop = first_prop;
	return register_filesystem(&openprom_fs_type);
}

static void __exit exit_openprom_fs(void)
{
	int i;
	unregister_filesystem(&openprom_fs_type);
	free_pages ((unsigned long)nodes, alloced);
	for (i = 0; i < aliases_nodes; i++)
		if (alias_names [i])
			kfree (alias_names [i]);
	nodes = NULL;
}

EXPORT_NO_SYMBOLS;

module_init(init_openprom_fs)
module_exit(exit_openprom_fs)
MODULE_LICENSE("GPL");

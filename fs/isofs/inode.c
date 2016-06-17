/*
 *  linux/fs/isofs/inode.c
 *
 *  (C) 1991  Linus Torvalds - minix filesystem
 *      1992, 1993, 1994  Eric Youngdale Modified for ISO 9660 filesystem.
 *      1994  Eberhard Moenkeberg - multi session handling.
 *      1995  Mark Dobie - allow mounting of some weird VideoCDs and PhotoCDs.
 *	1997  Gordon Chaffee - Joliet CDs
 *	1998  Eric Lammerts - ISO 9660 Level 3
 */

#include <linux/config.h>
#include <linux/module.h>

#include <linux/stat.h>
#include <linux/sched.h>
#include <linux/iso_fs.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/locks.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/cdrom.h>
#include <linux/init.h>
#include <linux/nls.h>
#include <linux/ctype.h>
#include <linux/smp_lock.h>
#include <linux/blkdev.h>

#include <asm/system.h>
#include <asm/uaccess.h>

#include "zisofs.h"

/*
 * We have no support for "multi volume" CDs, but more and more disks carry
 * wrong information within the volume descriptors.
 */
#define IGNORE_WRONG_MULTI_VOLUME_SPECS
#define BEQUIET

#ifdef LEAK_CHECK
static int check_malloc = 0;
static int check_bread = 0;
#endif

static int isofs_hashi(struct dentry *parent, struct qstr *qstr);
static int isofs_hash(struct dentry *parent, struct qstr *qstr);
static int isofs_dentry_cmpi(struct dentry *dentry, struct qstr *a, struct qstr *b);
static int isofs_dentry_cmp(struct dentry *dentry, struct qstr *a, struct qstr *b);

#ifdef CONFIG_JOLIET
static int isofs_hashi_ms(struct dentry *parent, struct qstr *qstr);
static int isofs_hash_ms(struct dentry *parent, struct qstr *qstr);
static int isofs_dentry_cmpi_ms(struct dentry *dentry, struct qstr *a, struct qstr *b);
static int isofs_dentry_cmp_ms(struct dentry *dentry, struct qstr *a, struct qstr *b);
#endif

static void isofs_put_super(struct super_block *sb)
{
#ifdef CONFIG_JOLIET
	if (sb->u.isofs_sb.s_nls_iocharset) {
		unload_nls(sb->u.isofs_sb.s_nls_iocharset);
		sb->u.isofs_sb.s_nls_iocharset = NULL;
	}
#endif

#ifdef LEAK_CHECK
	printk("Outstanding mallocs:%d, outstanding buffers: %d\n",
	       check_malloc, check_bread);
#endif

	return;
}

static void isofs_read_inode(struct inode *);
static int isofs_statfs (struct super_block *, struct statfs *);

static struct super_operations isofs_sops = {
	read_inode:	isofs_read_inode,
	put_super:	isofs_put_super,
	statfs:		isofs_statfs,
};

static struct dentry_operations isofs_dentry_ops[] = {
	{
		d_hash:		isofs_hash,
		d_compare:	isofs_dentry_cmp,
	},
	{
		d_hash:		isofs_hashi,
		d_compare:	isofs_dentry_cmpi,
	},
#ifdef CONFIG_JOLIET
	{
		d_hash:		isofs_hash_ms,
		d_compare:	isofs_dentry_cmp_ms,
	},
	{
		d_hash:		isofs_hashi_ms,
		d_compare:	isofs_dentry_cmpi_ms,
	}
#endif
};

struct iso9660_options{
	char map;
	char rock;
	char joliet;
	char cruft;
	char unhide;
	char nocompress;
	unsigned char check;
	unsigned int blocksize;
	mode_t mode;
	gid_t gid;
	uid_t uid;
	char *iocharset;
	unsigned char utf8;
        /* LVE */
        s32 session;
        s32 sbsector;
};

/*
 * Compute the hash for the isofs name corresponding to the dentry.
 */
static int
isofs_hash_common(struct dentry *dentry, struct qstr *qstr, int ms)
{
	const char *name;
	int len;

	len = qstr->len;
	name = qstr->name;
	if (ms) {
		while (len && name[len-1] == '.')
			len--;
	}

	qstr->hash = full_name_hash(name, len);

	return 0;
}

/*
 * Compute the hash for the isofs name corresponding to the dentry.
 */
static int
isofs_hashi_common(struct dentry *dentry, struct qstr *qstr, int ms)
{
	const char *name;
	int len;
	char c;
	unsigned long hash;

	len = qstr->len;
	name = qstr->name;
	if (ms) {
		while (len && name[len-1] == '.')
			len--;
	}

	hash = init_name_hash();
	while (len--) {
		c = tolower(*name++);
		hash = partial_name_hash(tolower(c), hash);
	}
	qstr->hash = end_name_hash(hash);

	return 0;
}

/*
 * Case insensitive compare of two isofs names.
 */
static int
isofs_dentry_cmpi_common(struct dentry *dentry,struct qstr *a,struct qstr *b,int ms)
{
	int alen, blen;

	/* A filename cannot end in '.' or we treat it like it has none */
	alen = a->len;
	blen = b->len;
	if (ms) {
		while (alen && a->name[alen-1] == '.')
			alen--;
		while (blen && b->name[blen-1] == '.')
			blen--;
	}
	if (alen == blen) {
		if (strnicmp(a->name, b->name, alen) == 0)
			return 0;
	}
	return 1;
}

/*
 * Case sensitive compare of two isofs names.
 */
static int
isofs_dentry_cmp_common(struct dentry *dentry,struct qstr *a,struct qstr *b,int ms)
{
	int alen, blen;

	/* A filename cannot end in '.' or we treat it like it has none */
	alen = a->len;
	blen = b->len;
	if (ms) {
		while (alen && a->name[alen-1] == '.')
			alen--;
		while (blen && b->name[blen-1] == '.')
			blen--;
	}
	if (alen == blen) {
		if (strncmp(a->name, b->name, alen) == 0)
			return 0;
	}
	return 1;
}

static int
isofs_hash(struct dentry *dentry, struct qstr *qstr)
{
	return isofs_hash_common(dentry, qstr, 0);
}

static int
isofs_hashi(struct dentry *dentry, struct qstr *qstr)
{
	return isofs_hashi_common(dentry, qstr, 0);
}

static int
isofs_dentry_cmp(struct dentry *dentry,struct qstr *a,struct qstr *b)
{
	return isofs_dentry_cmp_common(dentry, a, b, 0);
}

static int
isofs_dentry_cmpi(struct dentry *dentry,struct qstr *a,struct qstr *b)
{
	return isofs_dentry_cmpi_common(dentry, a, b, 0);
}

#ifdef CONFIG_JOLIET
static int
isofs_hash_ms(struct dentry *dentry, struct qstr *qstr)
{
	return isofs_hash_common(dentry, qstr, 1);
}

static int
isofs_hashi_ms(struct dentry *dentry, struct qstr *qstr)
{
	return isofs_hashi_common(dentry, qstr, 1);
}

static int
isofs_dentry_cmp_ms(struct dentry *dentry,struct qstr *a,struct qstr *b)
{
	return isofs_dentry_cmp_common(dentry, a, b, 1);
}

static int
isofs_dentry_cmpi_ms(struct dentry *dentry,struct qstr *a,struct qstr *b)
{
	return isofs_dentry_cmpi_common(dentry, a, b, 1);
}
#endif

static int parse_options(char *options, struct iso9660_options * popt)
{
	char *this_char,*value;

	popt->map = 'n';
	popt->rock = 'y';
	popt->joliet = 'y';
	popt->cruft = 'n';
	popt->unhide = 'n';
	popt->check = 'u';		/* unset */
	popt->nocompress = 0;
	popt->blocksize = 1024;
	popt->mode = S_IRUGO | S_IXUGO; /* r-x for all.  The disc could
					   be shared with DOS machines so
					   virtually anything could be
					   a valid executable. */
	popt->gid = 0;
	popt->uid = 0;
	popt->iocharset = NULL;
	popt->utf8 = 0;
	popt->session=-1;
	popt->sbsector=-1;
	if (!options) return 1;
	for (this_char = strtok(options,","); this_char; this_char = strtok(NULL,",")) {
	        if (strncmp(this_char,"norock",6) == 0) {
		  popt->rock = 'n';
		  continue;
		}
	        if (strncmp(this_char,"nojoliet",8) == 0) {
		  popt->joliet = 'n';
		  continue;
		}
	        if (strncmp(this_char,"unhide",6) == 0) {
		  popt->unhide = 'y';
		  continue;
		}
	        if (strncmp(this_char,"cruft",5) == 0) {
		  popt->cruft = 'y';
		  continue;
		}
	        if (strncmp(this_char,"utf8",4) == 0) {
		  popt->utf8 = 1;
		  continue;
		}
	        if (strncmp(this_char,"nocompress",10) == 0) {
		  popt->nocompress = 1;
		  continue;
		}
		if ((value = strchr(this_char,'=')) != NULL)
			*value++ = 0;

#ifdef CONFIG_JOLIET
		if (!strcmp(this_char,"iocharset") && value) {
			popt->iocharset = value;
			while (*value && *value != ',')
				value++;
			if (value == popt->iocharset)
				return 0;
			*value = 0;
		} else
#endif
		if (!strcmp(this_char,"map") && value) {
			if (value[0] && !value[1] && strchr("ano",*value))
				popt->map = *value;
			else if (!strcmp(value,"off")) popt->map = 'o';
			else if (!strcmp(value,"normal")) popt->map = 'n';
			else if (!strcmp(value,"acorn")) popt->map = 'a';
			else return 0;
		}
		if (!strcmp(this_char,"session") && value) {
			char * vpnt = value;
			unsigned int ivalue = simple_strtoul(vpnt, &vpnt, 0);
			if(ivalue < 0 || ivalue >99) return 0;
			popt->session=ivalue+1;
		}
		if (!strcmp(this_char,"sbsector") && value) {
			char * vpnt = value;
			unsigned int ivalue = simple_strtoul(vpnt, &vpnt, 0);
			if(ivalue < 0 || ivalue >660*512) return 0;
			popt->sbsector=ivalue;
		}
		else if (!strcmp(this_char,"check") && value) {
			if (value[0] && !value[1] && strchr("rs",*value))
				popt->check = *value;
			else if (!strcmp(value,"relaxed")) popt->check = 'r';
			else if (!strcmp(value,"strict")) popt->check = 's';
			else return 0;
		}
		else if (!strcmp(this_char,"conv") && value) {
			/* no conversion is done anymore;
			   we still accept the same mount options,
			   but ignore them */
			if (value[0] && !value[1] && strchr("btma",*value)) ;
			else if (!strcmp(value,"binary")) ;
			else if (!strcmp(value,"text")) ;
			else if (!strcmp(value,"mtext")) ;
			else if (!strcmp(value,"auto")) ;
			else return 0;
		}
		else if (value &&
			 (!strcmp(this_char,"block") ||
			  !strcmp(this_char,"mode") ||
			  !strcmp(this_char,"uid") ||
			  !strcmp(this_char,"gid"))) {
		  char * vpnt = value;
		  unsigned int ivalue = simple_strtoul(vpnt, &vpnt, 0);
		  if (*vpnt) return 0;
		  switch(*this_char) {
		  case 'b':
		    if (   ivalue != 512
			&& ivalue != 1024
			&& ivalue != 2048) return 0;
		    popt->blocksize = ivalue;
		    break;
		  case 'u':
		    popt->uid = ivalue;
		    break;
		  case 'g':
		    popt->gid = ivalue;
		    break;
		  case 'm':
		    popt->mode = ivalue;
		    break;
		  }
		}
		else return 1;
	}
	return 1;
}

/*
 * look if the driver can tell the multi session redirection value
 *
 * don't change this if you don't know what you do, please!
 * Multisession is legal only with XA disks.
 * A non-XA disk with more than one volume descriptor may do it right, but
 * usually is written in a nowhere standardized "multi-partition" manner.
 * Multisession uses absolute addressing (solely the first frame of the whole
 * track is #0), multi-partition uses relative addressing (each first frame of
 * each track is #0), and a track is not a session.
 *
 * A broken CDwriter software or drive firmware does not set new standards,
 * at least not if conflicting with the existing ones.
 *
 * emoenke@gwdg.de
 */
#define WE_OBEY_THE_WRITTEN_STANDARDS 1

static unsigned int isofs_get_last_session(struct super_block *sb,s32 session )
{
	struct cdrom_multisession ms_info;
	unsigned int vol_desc_start;
	struct block_device *bdev = sb->s_bdev;
	int i;

	vol_desc_start=0;
	ms_info.addr_format=CDROM_LBA;
	if(session >= 0 && session <= 99) {
		struct cdrom_tocentry Te;
		Te.cdte_track=session;
		Te.cdte_format=CDROM_LBA;
		i = ioctl_by_bdev(bdev, CDROMREADTOCENTRY, (unsigned long) &Te);
		if (!i) {
			printk(KERN_DEBUG "Session %d start %d type %d\n",
			       session, Te.cdte_addr.lba,
			       Te.cdte_ctrl&CDROM_DATA_TRACK);
			if ((Te.cdte_ctrl&CDROM_DATA_TRACK) == 4)
				return Te.cdte_addr.lba;
		}
			
		printk(KERN_ERR "Invalid session number or type of track\n");
	}
	i = ioctl_by_bdev(bdev, CDROMMULTISESSION, (unsigned long) &ms_info);
	if(session > 0) printk(KERN_ERR "Invalid session number\n");
#if 0
	printk("isofs.inode: CDROMMULTISESSION: rc=%d\n",i);
	if (i==0) {
		printk("isofs.inode: XA disk: %s\n",ms_info.xa_flag?"yes":"no");
		printk("isofs.inode: vol_desc_start = %d\n", ms_info.addr.lba);
	}
#endif
	if (i==0)
#if WE_OBEY_THE_WRITTEN_STANDARDS
        if (ms_info.xa_flag) /* necessary for a valid ms_info.addr */
#endif
		vol_desc_start=ms_info.addr.lba;
	return vol_desc_start;
}

/*
 * Initialize the superblock and read the root inode.
 *
 * Note: a check_disk_change() has been done immediately prior
 * to this call, so we don't need to check again.
 */
static struct super_block *isofs_read_super(struct super_block *s, void *data,
					    int silent)
{
	kdev_t				dev = s->s_dev;
	struct buffer_head	      * bh = NULL, *pri_bh = NULL;
	struct hs_primary_descriptor  * h_pri = NULL;
	struct iso_primary_descriptor * pri = NULL;
	struct iso_supplementary_descriptor *sec = NULL;
	struct iso_directory_record   * rootp;
	int				joliet_level = 0;
	int				high_sierra;
	int				iso_blknum, block;
	int				orig_zonesize;
	int				table;
	unsigned int			blocksize, blocksize_bits;
	unsigned int			vol_desc_start;
	unsigned long			first_data_zone;
	struct inode		      * inode;
	struct iso9660_options		opt;

	if (!parse_options((char *) data, &opt))
		goto out_unlock;

#if 0
	printk("map = %c\n", opt.map);
	printk("rock = %c\n", opt.rock);
	printk("joliet = %c\n", opt.joliet);
	printk("check = %c\n", opt.check);
	printk("cruft = %c\n", opt.cruft);
	printk("unhide = %c\n", opt.unhide);
	printk("blocksize = %d\n", opt.blocksize);
	printk("gid = %d\n", opt.gid);
	printk("uid = %d\n", opt.uid);
	printk("iocharset = %s\n", opt.iocharset);
#endif

	/*
	 * First of all, get the hardware blocksize for this device.
	 * If we don't know what it is, or the hardware blocksize is
	 * larger than the blocksize the user specified, then use
	 * that value.
	 */
	blocksize = get_hardsect_size(dev);
	if(blocksize > opt.blocksize) {
	    /*
	     * Force the blocksize we are going to use to be the
	     * hardware blocksize.
	     */
	    opt.blocksize = blocksize;
	}

	blocksize_bits = 0;
	{
	  int i = opt.blocksize;
	  while (i != 1){
	    blocksize_bits++;
	    i >>=1;
	  }
	}

	set_blocksize(dev, opt.blocksize);
	s->s_blocksize = opt.blocksize;

	s->u.isofs_sb.s_high_sierra = high_sierra = 0; /* default is iso9660 */

	vol_desc_start = (opt.sbsector != -1) ?
		opt.sbsector : isofs_get_last_session(s,opt.session);

  	for (iso_blknum = vol_desc_start+16;
             iso_blknum < vol_desc_start+100; iso_blknum++)
	{
	    struct hs_volume_descriptor   * hdp;
	    struct iso_volume_descriptor  * vdp;

	    block = iso_blknum << (ISOFS_BLOCK_BITS-blocksize_bits);
	    if (!(bh = sb_bread(s, block)))
		goto out_no_read;

	    vdp = (struct iso_volume_descriptor *)bh->b_data;
	    hdp = (struct hs_volume_descriptor *)bh->b_data;
	    
	    /* Due to the overlapping physical location of the descriptors, 
	     * ISO CDs can match hdp->id==HS_STANDARD_ID as well. To ensure 
	     * proper identification in this case, we first check for ISO.
	     */
	    if (strncmp (vdp->id, ISO_STANDARD_ID, sizeof vdp->id) == 0) {
		if (isonum_711 (vdp->type) == ISO_VD_END)
		    break;
		if (isonum_711 (vdp->type) == ISO_VD_PRIMARY) {
		    if (pri == NULL) {
			pri = (struct iso_primary_descriptor *)vdp;
			/* Save the buffer in case we need it ... */
			pri_bh = bh;
			bh = NULL;
		    }
		}
#ifdef CONFIG_JOLIET
		else if (isonum_711 (vdp->type) == ISO_VD_SUPPLEMENTARY) {
		    sec = (struct iso_supplementary_descriptor *)vdp;
		    if (sec->escape[0] == 0x25 && sec->escape[1] == 0x2f) {
			if (opt.joliet == 'y') {
			    if (sec->escape[2] == 0x40) {
				joliet_level = 1;
			    } else if (sec->escape[2] == 0x43) {
				joliet_level = 2;
			    } else if (sec->escape[2] == 0x45) {
				joliet_level = 3;
			    }
			    printk(KERN_DEBUG"ISO 9660 Extensions: Microsoft Joliet Level %d\n",
				   joliet_level);
			}
			goto root_found;
		    } else {
			/* Unknown supplementary volume descriptor */
			sec = NULL;
		    }
		}
#endif
	    } else {
	        if (strncmp (hdp->id, HS_STANDARD_ID, sizeof hdp->id) == 0) {
		    if (isonum_711 (hdp->type) != ISO_VD_PRIMARY)
		        goto out_freebh;
		
		    s->u.isofs_sb.s_high_sierra = 1;
		    high_sierra = 1;
		    opt.rock = 'n';
		    h_pri = (struct hs_primary_descriptor *)vdp;
		    goto root_found;
		}
	    }

            /* Just skip any volume descriptors we don't recognize */

	    brelse(bh);
	    bh = NULL;
	}
	/*
	 * If we fall through, either no volume descriptor was found,
	 * or else we passed a primary descriptor looking for others.
	 */
	if (!pri)
		goto out_unknown_format;
	brelse(bh);
	bh = pri_bh;
	pri_bh = NULL;

root_found:

	if (joliet_level && (pri == NULL || opt.rock == 'n')) {
	    /* This is the case of Joliet with the norock mount flag.
	     * A disc with both Joliet and Rock Ridge is handled later
	     */
	    pri = (struct iso_primary_descriptor *) sec;
	}

	if(high_sierra){
	  rootp = (struct iso_directory_record *) h_pri->root_directory_record;
#ifndef IGNORE_WRONG_MULTI_VOLUME_SPECS
	  if (isonum_723 (h_pri->volume_set_size) != 1)
		goto out_no_support;
#endif /* IGNORE_WRONG_MULTI_VOLUME_SPECS */
	  s->u.isofs_sb.s_nzones = isonum_733 (h_pri->volume_space_size);
	  s->u.isofs_sb.s_log_zone_size = isonum_723 (h_pri->logical_block_size);
	  s->u.isofs_sb.s_max_size = isonum_733(h_pri->volume_space_size);
	} else {
	  rootp = (struct iso_directory_record *) pri->root_directory_record;
#ifndef IGNORE_WRONG_MULTI_VOLUME_SPECS
	  if (isonum_723 (pri->volume_set_size) != 1)
		goto out_no_support;
#endif /* IGNORE_WRONG_MULTI_VOLUME_SPECS */
	  s->u.isofs_sb.s_nzones = isonum_733 (pri->volume_space_size);
	  s->u.isofs_sb.s_log_zone_size = isonum_723 (pri->logical_block_size);
	  s->u.isofs_sb.s_max_size = isonum_733(pri->volume_space_size);
	}

	s->u.isofs_sb.s_ninodes = 0; /* No way to figure this out easily */

	orig_zonesize = s -> u.isofs_sb.s_log_zone_size;
	/*
	 * If the zone size is smaller than the hardware sector size,
	 * this is a fatal error.  This would occur if the disc drive
	 * had sectors that were 2048 bytes, but the filesystem had
	 * blocks that were 512 bytes (which should only very rarely
	 * happen.)
	 */
	if(blocksize != 0 && orig_zonesize < blocksize)
		goto out_bad_size;

	/* RDE: convert log zone size to bit shift */
	switch (s -> u.isofs_sb.s_log_zone_size)
	  { case  512: s -> u.isofs_sb.s_log_zone_size =  9; break;
	    case 1024: s -> u.isofs_sb.s_log_zone_size = 10; break;
	    case 2048: s -> u.isofs_sb.s_log_zone_size = 11; break;

	    default:
		goto out_bad_zone_size;
	  }

	s->s_magic = ISOFS_SUPER_MAGIC;

	/* The CDROM is read-only, has no nodes (devices) on it, and since
	   all of the files appear to be owned by root, we really do not want
	   to allow suid.  (suid or devices will not show up unless we have
	   Rock Ridge extensions) */

	s->s_flags |= MS_RDONLY /* | MS_NODEV | MS_NOSUID */;

	/* Set this for reference. Its not currently used except on write
	   which we don't have .. */
	   
	/* RDE: data zone now byte offset! */

	first_data_zone = ((isonum_733 (rootp->extent) +
			  isonum_711 (rootp->ext_attr_length))
			 << s -> u.isofs_sb.s_log_zone_size);
	s->u.isofs_sb.s_firstdatazone = first_data_zone;
#ifndef BEQUIET
	printk(KERN_DEBUG "Max size:%ld   Log zone size:%ld\n",
	       s->u.isofs_sb.s_max_size,
	       1UL << s->u.isofs_sb.s_log_zone_size);
	printk(KERN_DEBUG "First datazone:%ld   Root inode number:%ld\n",
	       s->u.isofs_sb.s_firstdatazone >> s -> u.isofs_sb.s_log_zone_size,
	       s->u.isofs_sb.s_firstdatazone);
	if(high_sierra)
		printk(KERN_DEBUG "Disc in High Sierra format.\n");
#endif

	/*
	 * If the Joliet level is set, we _may_ decide to use the
	 * secondary descriptor, but can't be sure until after we
	 * read the root inode. But before reading the root inode
	 * we may need to change the device blocksize, and would
	 * rather release the old buffer first. So, we cache the
	 * first_data_zone value from the secondary descriptor.
	 */
	if (joliet_level) {
		pri = (struct iso_primary_descriptor *) sec;
		rootp = (struct iso_directory_record *)
			pri->root_directory_record;
		first_data_zone = ((isonum_733 (rootp->extent) +
			  	isonum_711 (rootp->ext_attr_length))
				 << s -> u.isofs_sb.s_log_zone_size);
	}

	/*
	 * We're all done using the volume descriptor, and may need
	 * to change the device blocksize, so release the buffer now.
	 */
	brelse(pri_bh);
	brelse(bh);

	/*
	 * Force the blocksize to 512 for 512 byte sectors.  The file
	 * read primitives really get it wrong in a bad way if we don't
	 * do this.
	 *
	 * Note - we should never be setting the blocksize to something
	 * less than the hardware sector size for the device.  If we
	 * do, we would end up having to read larger buffers and split
	 * out portions to satisfy requests.
	 *
	 * Note2- the idea here is that we want to deal with the optimal
	 * zonesize in the filesystem.  If we have it set to something less,
	 * then we have horrible problems with trying to piece together
	 * bits of adjacent blocks in order to properly read directory
	 * entries.  By forcing the blocksize in this way, we ensure
	 * that we will never be required to do this.
	 */
	if ( orig_zonesize != opt.blocksize ) {
		set_blocksize(dev, orig_zonesize);
#ifndef BEQUIET
		printk(KERN_DEBUG 
			"ISOFS: Forcing new log zone size:%d\n", orig_zonesize);
#endif
	}
	s->s_blocksize = orig_zonesize;
	s->s_blocksize_bits = s -> u.isofs_sb.s_log_zone_size;

	s->u.isofs_sb.s_nls_iocharset = NULL;

#ifdef CONFIG_JOLIET
	if (joliet_level && opt.utf8 == 0) {
		char * p = opt.iocharset ? opt.iocharset : "iso8859-1";
		s->u.isofs_sb.s_nls_iocharset = load_nls(p);
		if (! s->u.isofs_sb.s_nls_iocharset) {
			/* Fail only if explicit charset specified */
			if (opt.iocharset)
				goto out_unlock;
			s->u.isofs_sb.s_nls_iocharset = load_nls_default();
		}
	}
#endif
	s->s_op = &isofs_sops;
	s->u.isofs_sb.s_mapping = opt.map;
	s->u.isofs_sb.s_rock = (opt.rock == 'y' ? 2 : 0);
	s->u.isofs_sb.s_rock_offset = -1; /* initial offset, will guess until SP is found*/
	s->u.isofs_sb.s_cruft = opt.cruft;
	s->u.isofs_sb.s_unhide = opt.unhide;
	s->u.isofs_sb.s_uid = opt.uid;
	s->u.isofs_sb.s_gid = opt.gid;
	s->u.isofs_sb.s_utf8 = opt.utf8;
	s->u.isofs_sb.s_nocompress = opt.nocompress;
	/*
	 * It would be incredibly stupid to allow people to mark every file
	 * on the disk as suid, so we merely allow them to set the default
	 * permissions.
	 */
	s->u.isofs_sb.s_mode = opt.mode & 0777;

	/*
	 * Read the root inode, which _may_ result in changing
	 * the s_rock flag. Once we have the final s_rock value,
	 * we then decide whether to use the Joliet descriptor.
	 */
	inode = iget(s, s->u.isofs_sb.s_firstdatazone);

	/*
	 * If this disk has both Rock Ridge and Joliet on it, then we
	 * want to use Rock Ridge by default.  This can be overridden
	 * by using the norock mount option.  There is still one other
	 * possibility that is not taken into account: a Rock Ridge
	 * CD with Unicode names.  Until someone sees such a beast, it
	 * will not be supported.
	 */
	if (s->u.isofs_sb.s_rock == 1) {
		joliet_level = 0;
	} else if (joliet_level) {
		s->u.isofs_sb.s_rock = 0;
		if (s->u.isofs_sb.s_firstdatazone != first_data_zone) {
			s->u.isofs_sb.s_firstdatazone = first_data_zone;
			printk(KERN_DEBUG 
				"ISOFS: changing to secondary root\n");
			iput(inode);
			inode = iget(s, s->u.isofs_sb.s_firstdatazone);
		}
	}

	if (opt.check == 'u') {
		/* Only Joliet is case insensitive by default */
		if (joliet_level) opt.check = 'r';
		else opt.check = 's';
	}
	s->u.isofs_sb.s_joliet_level = joliet_level;

	/* check the root inode */
	if (!inode)
		goto out_no_root;
	if (!inode->i_op)
		goto out_bad_root;
	/* get the root dentry */
	s->s_root = d_alloc_root(inode);
	if (!(s->s_root))
		goto out_no_root;

	table = 0;
	if (joliet_level) table += 2;
	if (opt.check == 'r') table++;
	s->s_root->d_op = &isofs_dentry_ops[table];

	return s;

	/*
	 * Display error messages and free resources.
	 */
out_bad_root:
	printk(KERN_WARNING "isofs_read_super: root inode not initialized\n");
	goto out_iput;
out_no_root:
	printk(KERN_WARNING "isofs_read_super: get root inode failed\n");
out_iput:
	iput(inode);
#ifdef CONFIG_JOLIET
	if (s->u.isofs_sb.s_nls_iocharset)
		unload_nls(s->u.isofs_sb.s_nls_iocharset);
#endif
	goto out_unlock;
out_no_read:
	printk(KERN_WARNING "isofs_read_super: "
		"bread failed, dev=%s, iso_blknum=%d, block=%d\n",
		kdevname(dev), iso_blknum, block);
	goto out_unlock;
out_bad_zone_size:
	printk(KERN_WARNING "Bad logical zone size %ld\n",
		s->u.isofs_sb.s_log_zone_size);
	goto out_freebh;
out_bad_size:
	printk(KERN_WARNING "Logical zone size(%d) < hardware blocksize(%u)\n",
		orig_zonesize, blocksize);
	goto out_freebh;
#ifndef IGNORE_WRONG_MULTI_VOLUME_SPECS
out_no_support:
	printk(KERN_WARNING "Multi-volume disks not supported.\n");
	goto out_freebh;
#endif
out_unknown_format:
	if (!silent)
		printk(KERN_WARNING "Unable to identify CD-ROM format.\n");

out_freebh:
	brelse(bh);
out_unlock:
	return NULL;
}

static int isofs_statfs (struct super_block *sb, struct statfs *buf)
{
	buf->f_type = ISOFS_SUPER_MAGIC;
	buf->f_bsize = sb->s_blocksize;
	buf->f_blocks = (sb->u.isofs_sb.s_nzones
                  << (sb->u.isofs_sb.s_log_zone_size - sb->s_blocksize_bits));
	buf->f_bfree = 0;
	buf->f_bavail = 0;
	buf->f_files = sb->u.isofs_sb.s_ninodes;
	buf->f_ffree = 0;
	buf->f_namelen = NAME_MAX;
	return 0;
}

/*
 * Get a set of blocks; filling in buffer_heads if already allocated
 * or getblk() if they are not.  Returns the number of blocks inserted
 * (0 == error.)
 */
int isofs_get_blocks(struct inode *inode, long iblock,
		     struct buffer_head **bh_result, unsigned long nblocks)
{
	unsigned long b_off;
	unsigned offset, sect_size;
	unsigned int firstext;
	unsigned long nextino;
	int section, rv;

	lock_kernel();

	rv = 0;
	if (iblock < 0) {
		printk("isofs_get_blocks: block < 0\n");
		goto abort;
	}

	b_off = iblock;
	
	offset    = 0;
	firstext  = inode->u.isofs_i.i_first_extent;
	sect_size = inode->u.isofs_i.i_section_size >> ISOFS_BUFFER_BITS(inode);
	nextino   = inode->u.isofs_i.i_next_section_ino;
	section   = 0;

	while ( nblocks ) {
		/* If we are *way* beyond the end of the file, print a message.
		 * Access beyond the end of the file up to the next page boundary
		 * is normal, however because of the way the page cache works.
		 * In this case, we just return 0 so that we can properly fill
		 * the page with useless information without generating any
		 * I/O errors.
		 */
		if (b_off > ((inode->i_size + PAGE_CACHE_SIZE - 1) >> ISOFS_BUFFER_BITS(inode))) {
			printk("isofs_get_blocks: block >= EOF (%ld, %ld)\n",
			       iblock, (unsigned long) inode->i_size);
			goto abort;
		}
		
		if (nextino) {
			while (b_off >= (offset + sect_size)) {
				struct inode *ninode;
				
				offset += sect_size;
				if (nextino == 0)
					goto abort;
				ninode = iget(inode->i_sb, nextino);
				if (!ninode)
					goto abort;
				firstext  = ninode->u.isofs_i.i_first_extent;
				sect_size = ninode->u.isofs_i.i_section_size;
				nextino   = ninode->u.isofs_i.i_next_section_ino;
				iput(ninode);
				
				if (++section > 100) {
					printk("isofs_get_blocks: More than 100 file sections ?!?, aborting...\n");
					printk("isofs_get_blocks: ino=%lu block=%ld firstext=%u sect_size=%u nextino=%lu\n",
					       inode->i_ino, iblock, firstext, (unsigned) sect_size, nextino);
					goto abort;
				}
			}
		}
		
		if ( *bh_result ) {
			(*bh_result)->b_dev      = inode->i_dev;
			(*bh_result)->b_blocknr  = firstext + b_off - offset;
			(*bh_result)->b_state   |= (1UL << BH_Mapped);
		} else {
			*bh_result = sb_getblk(inode->i_sb, firstext+b_off-offset);
			if ( !*bh_result )
				goto abort;
		}
		bh_result++;	/* Next buffer head */
		b_off++;	/* Next buffer offset */
		nblocks--;
		rv++;
	}


abort:
	unlock_kernel();
	return rv;
}

/*
 * Used by the standard interfaces.
 */
static int isofs_get_block(struct inode *inode, long iblock,
		    struct buffer_head *bh_result, int create)
{
	if ( create ) {
		printk("isofs_get_block: Kernel tries to allocate a block\n");
		return -EROFS;
	}

	return isofs_get_blocks(inode, iblock, &bh_result, 1) ? 0 : -EIO;
}

static int isofs_bmap(struct inode *inode, int block)
{
	struct buffer_head dummy;
	int error;

	dummy.b_state = 0;
	dummy.b_blocknr = -1000;
	error = isofs_get_block(inode, block, &dummy, 0);
	if (!error)
		return dummy.b_blocknr;
	return 0;
}

struct buffer_head *isofs_bread(struct inode *inode, unsigned int block)
{
	unsigned int blknr = isofs_bmap(inode, block);
	if (!blknr)
		return NULL;
	return sb_bread(inode->i_sb, blknr);
}

static int isofs_readpage(struct file *file, struct page *page)
{
	return block_read_full_page(page,isofs_get_block);
}

static int _isofs_bmap(struct address_space *mapping, long block)
{
	return generic_block_bmap(mapping,block,isofs_get_block);
}

static struct address_space_operations isofs_aops = {
	readpage: isofs_readpage,
	sync_page: block_sync_page,
	bmap: _isofs_bmap
};

static inline void test_and_set_uid(uid_t *p, uid_t value)
{
	if(value) {
		*p = value;
	}
}

static inline void test_and_set_gid(gid_t *p, gid_t value)
{
        if(value) {
                *p = value;
        }
}

static int isofs_read_level3_size(struct inode * inode)
{
	unsigned long f_pos = inode->i_ino;
	unsigned long bufsize = ISOFS_BUFFER_SIZE(inode);
	int high_sierra = inode->i_sb->u.isofs_sb.s_high_sierra;
	struct buffer_head * bh = NULL;
	unsigned long block, offset;
	int i = 0;
	int more_entries = 0;
	struct iso_directory_record * tmpde = NULL;

	inode->i_size = 0;
	inode->u.isofs_i.i_next_section_ino = 0;

	block = f_pos >> ISOFS_BUFFER_BITS(inode);
	offset = f_pos & (bufsize-1);

	do {
		struct iso_directory_record * de;
		unsigned int de_len;

		if (!bh) {
			bh = sb_bread(inode->i_sb, block);
			if (!bh)
				goto out_noread;
		}
		de = (struct iso_directory_record *) (bh->b_data + offset);
		de_len = *(unsigned char *) de;

		if (de_len == 0) {
			brelse(bh);
			bh = NULL;
			f_pos = (f_pos + ISOFS_BLOCK_SIZE) & ~(ISOFS_BLOCK_SIZE - 1);
			block = f_pos >> ISOFS_BUFFER_BITS(inode);
			offset = 0;
			continue;
		}

		offset += de_len;

		/* Make sure we have a full directory entry */
		if (offset >= bufsize) {
			int slop = bufsize - offset + de_len;
			if (!tmpde) {
				tmpde = kmalloc(256, GFP_KERNEL);
				if (!tmpde)
					goto out_nomem;
			}
			memcpy(tmpde, de, slop);
			offset &= bufsize - 1;
			block++;
			brelse(bh);
			bh = NULL;
			if (offset) {
				bh = sb_bread(inode->i_sb, block);
				if (!bh)
					goto out_noread;
				memcpy((void *) tmpde + slop, bh->b_data, offset);
			}
			de = tmpde;
		}

		inode->i_size += isonum_733(de->size);
		if (i == 1)
			inode->u.isofs_i.i_next_section_ino = f_pos;

		more_entries = de->flags[-high_sierra] & 0x80;

		f_pos += de_len;
		i++;
		if(i > 100)
			goto out_toomany;
	} while(more_entries);
out:
	if (tmpde)
		kfree(tmpde);
	if (bh)
		brelse(bh);
	return 0;

out_nomem:
	if (bh)
		brelse(bh);
	return -ENOMEM;

out_noread:
	printk(KERN_INFO "ISOFS: unable to read i-node block %lu\n", block);
	if (tmpde)
		kfree(tmpde);
	return -EIO;

out_toomany:
	printk(KERN_INFO "isofs_read_level3_size: "
		"More than 100 file sections ?!?, aborting...\n"
	  	"isofs_read_level3_size: inode=%lu ino=%lu\n",
		inode->i_ino, f_pos);
	goto out;
}

static void isofs_read_inode(struct inode * inode)
{
	struct super_block *sb = inode->i_sb;
	unsigned long bufsize = ISOFS_BUFFER_SIZE(inode);
	int block = inode->i_ino >> ISOFS_BUFFER_BITS(inode);
	int high_sierra = sb->u.isofs_sb.s_high_sierra;
	struct buffer_head * bh = NULL;
	struct iso_directory_record * de;
	struct iso_directory_record * tmpde = NULL;
	unsigned int de_len;
	unsigned long offset;
	int volume_seq_no, i;

	bh = sb_bread(inode->i_sb, block);
	if (!bh)
		goto out_badread;

	offset = (inode->i_ino & (bufsize - 1));
	de = (struct iso_directory_record *) (bh->b_data + offset);
	de_len = *(unsigned char *) de;

	if (offset + de_len > bufsize) {
		int frag1 = bufsize - offset;

		tmpde = kmalloc(de_len, GFP_KERNEL);
		if (tmpde == NULL) {
			printk(KERN_INFO "isofs_read_inode: out of memory\n");
			goto fail;
		}
		memcpy(tmpde, bh->b_data + offset, frag1);
		brelse(bh);
		bh = sb_bread(inode->i_sb, ++block);
		if (!bh)
			goto out_badread;
		memcpy((char *)tmpde+frag1, bh->b_data, de_len - frag1);
		de = tmpde;
	}

	/* Assume it is a normal-format file unless told otherwise */
	inode->u.isofs_i.i_file_format = isofs_file_normal;

	if (de->flags[-high_sierra] & 2) {
		inode->i_mode = S_IRUGO | S_IXUGO | S_IFDIR;
		inode->i_nlink = 1; /* Set to 1.  We know there are 2, but
				       the find utility tries to optimize
				       if it is 2, and it screws up.  It is
				       easier to give 1 which tells find to
				       do it the hard way. */
	} else {
 		/* Everybody gets to read the file. */
		inode->i_mode = inode->i_sb->u.isofs_sb.s_mode;
		inode->i_nlink = 1;
	        inode->i_mode |= S_IFREG;
		/* If there are no periods in the name,
		 * then set the execute permission bit
		 */
		for(i=0; i< de->name_len[0]; i++)
			if(de->name[i]=='.' || de->name[i]==';')
				break;
		if(i == de->name_len[0] || de->name[i] == ';')
			inode->i_mode |= S_IXUGO; /* execute permission */
	}
	inode->i_uid = inode->i_sb->u.isofs_sb.s_uid;
	inode->i_gid = inode->i_sb->u.isofs_sb.s_gid;
	inode->i_blocks = inode->i_blksize = 0;


	inode->u.isofs_i.i_section_size = isonum_733 (de->size);
	if(de->flags[-high_sierra] & 0x80) {
		if(isofs_read_level3_size(inode)) goto fail;
	} else {
		inode->i_size = isonum_733 (de->size);
	}

	/*
	 * The ISO-9660 filesystem only stores 32 bits for file size.
	 * mkisofs handles files up to 2GB-2 = 2147483646 = 0x7FFFFFFE bytes
	 * in size. This is according to the large file summit paper from 1996.
	 * WARNING: ISO-9660 filesystems > 1 GB and even > 2 GB are fully
	 *	    legal. Do not prevent to use DVD's schilling@fokus.gmd.de
	 */
	if ((inode->i_size < 0 || inode->i_size > 0x7FFFFFFE) &&
	    inode->i_sb->u.isofs_sb.s_cruft == 'n') {
		printk(KERN_WARNING "Warning: defective CD-ROM.  "
		       "Enabling \"cruft\" mount option.\n");
		inode->i_sb->u.isofs_sb.s_cruft = 'y';
	}

	/*
	 * Some dipshit decided to store some other bit of information
	 * in the high byte of the file length.  Catch this and holler.
	 * WARNING: this will make it impossible for a file to be > 16MB
	 * on the CDROM.
	 */

	if (inode->i_sb->u.isofs_sb.s_cruft == 'y' &&
	    inode->i_size & 0xff000000) {
		inode->i_size &= 0x00ffffff;
	}

	if (de->interleave[0]) {
		printk("Interleaved files not (yet) supported.\n");
		inode->i_size = 0;
	}

	/* I have no idea what file_unit_size is used for, so
	   we will flag it for now */
	if (de->file_unit_size[0] != 0) {
		printk("File unit size != 0 for ISO file (%ld).\n",
		       inode->i_ino);
	}

	/* I have no idea what other flag bits are used for, so
	   we will flag it for now */
#ifdef DEBUG
	if((de->flags[-high_sierra] & ~2)!= 0){
		printk("Unusual flag settings for ISO file (%ld %x).\n",
		       inode->i_ino, de->flags[-high_sierra]);
	}
#endif

	inode->i_mtime = inode->i_atime = inode->i_ctime =
		iso_date(de->date, high_sierra);

	inode->u.isofs_i.i_first_extent = (isonum_733 (de->extent) +
					   isonum_711 (de->ext_attr_length));

	/* Set the number of blocks for stat() - should be done before RR */
	inode->i_blksize = PAGE_CACHE_SIZE; /* For stat() only */
	inode->i_blocks  = (inode->i_size + 511) >> 9;

	/*
	 * Now test for possible Rock Ridge extensions which will override
	 * some of these numbers in the inode structure.
	 */

	if (!high_sierra) {
		parse_rock_ridge_inode(de, inode);
		/* if we want uid/gid set, override the rock ridge setting */
		test_and_set_uid(&inode->i_uid, inode->i_sb->u.isofs_sb.s_uid);
		test_and_set_gid(&inode->i_gid, inode->i_sb->u.isofs_sb.s_gid);
	}

	/* get the volume sequence number */
	volume_seq_no = isonum_723 (de->volume_sequence_number) ;

    /*
     * Multi volume means tagging a group of CDs with info in their headers.
     * All CDs of a group must share the same vol set name and vol set size
     * and have different vol set seq num. Deciding that data is wrong based
     * in that three fields is wrong. The fields are informative, for
     * cataloging purposes in a big jukebox, ie. Read sections 4.17, 4.18, 6.6
     * of ftp://ftp.ecma.ch/ecma-st/Ecma-119.pdf (ECMA 119 2nd Ed = ISO 9660)
     */
#ifndef IGNORE_WRONG_MULTI_VOLUME_SPECS
	/*
	 * Disable checking if we see any volume number other than 0 or 1.
	 * We could use the cruft option, but that has multiple purposes, one
	 * of which is limiting the file size to 16Mb.  Thus we silently allow
	 * volume numbers of 0 to go through without complaining.
	 */
	if (inode->i_sb->u.isofs_sb.s_cruft == 'n' &&
	    (volume_seq_no != 0) && (volume_seq_no != 1)) {
		printk(KERN_WARNING "Warning: defective CD-ROM "
		       "(volume sequence number %d). "
		       "Enabling \"cruft\" mount option.\n", volume_seq_no);
		inode->i_sb->u.isofs_sb.s_cruft = 'y';
	}
#endif /*IGNORE_WRONG_MULTI_VOLUME_SPECS */

	/* Install the inode operations vector */
#ifndef IGNORE_WRONG_MULTI_VOLUME_SPECS
	if (inode->i_sb->u.isofs_sb.s_cruft != 'y' &&
	    (volume_seq_no != 0) && (volume_seq_no != 1)) {
		printk(KERN_WARNING "Multi-volume CD somehow got mounted.\n");
	} else
#endif /*IGNORE_WRONG_MULTI_VOLUME_SPECS */
	{
		if (S_ISREG(inode->i_mode)) {
			inode->i_fop = &generic_ro_fops;
			switch ( inode->u.isofs_i.i_file_format ) {
#ifdef CONFIG_ZISOFS
			case isofs_file_compressed:
				inode->i_data.a_ops = &zisofs_aops;
				break;
#endif
			default:
				inode->i_data.a_ops = &isofs_aops;
				break;
			}
		} else if (S_ISDIR(inode->i_mode)) {
			inode->i_op = &isofs_dir_inode_operations;
			inode->i_fop = &isofs_dir_operations;
		} else if (S_ISLNK(inode->i_mode)) {
			inode->i_op = &page_symlink_inode_operations;
			inode->i_data.a_ops = &isofs_symlink_aops;
		} else
			/* XXX - parse_rock_ridge_inode() had already set i_rdev. */
			init_special_inode(inode, inode->i_mode,
					   kdev_t_to_nr(inode->i_rdev));
	}
 out:
	if (tmpde)
		kfree(tmpde);
	if (bh)
		brelse(bh);
	return;

 out_badread:
	printk(KERN_WARNING "ISOFS: unable to read i-node block\n");
 fail:
	make_bad_inode(inode);
	goto out;
}

#ifdef LEAK_CHECK
#undef malloc
#undef free_s
#undef sb_bread
#undef brelse

void * leak_check_malloc(unsigned int size){
  void * tmp;
  check_malloc++;
  tmp = kmalloc(size, GFP_KERNEL);
  return tmp;
}

void leak_check_free_s(void * obj, int size){
  check_malloc--;
  return kfree(obj);
}

struct buffer_head * leak_check_bread(struct super_block *sb, int block){
  check_bread++;
  return sb_bread(sb, block);
}

void leak_check_brelse(struct buffer_head * bh){
  check_bread--;
  return brelse(bh);
}

#endif

static DECLARE_FSTYPE_DEV(iso9660_fs_type, "iso9660", isofs_read_super);

static int __init init_iso9660_fs(void)
{
#ifdef CONFIG_ZISOFS
	int err;

	err = zisofs_init();
	if ( err )
		return err;
#endif
        return register_filesystem(&iso9660_fs_type);
}

static void __exit exit_iso9660_fs(void)
{
        unregister_filesystem(&iso9660_fs_type);
#ifdef CONFIG_ZISOFS
	zisofs_cleanup();
#endif
}

EXPORT_NO_SYMBOLS;

module_init(init_iso9660_fs)
module_exit(exit_iso9660_fs)
MODULE_LICENSE("GPL");


/* 
 * CVF extensions for fat-based filesystems
 *
 * written 1997,1998 by Frank Gockel <gockel@sent13.uni-duisburg.de>
 *
 * please do not remove the next line, dmsdos needs it for verifying patches
 * CVF-FAT-VERSION-ID: 1.2.0
 *
 */
 
#include<linux/sched.h>
#include<linux/fs.h>
#include<linux/msdos_fs.h>
#include<linux/msdos_fs_sb.h>
#include<linux/string.h>
#include<linux/fat_cvf.h>
#include<linux/config.h>
#ifdef CONFIG_KMOD
#include<linux/kmod.h>
#endif

#define MAX_CVF_FORMATS 3

struct buffer_head *default_fat_bread(struct super_block *,int);
struct buffer_head *default_fat_getblk(struct super_block *, int);
void default_fat_brelse(struct super_block *, struct buffer_head *);
void default_fat_mark_buffer_dirty (struct super_block *, struct buffer_head *);
void default_fat_set_uptodate (struct super_block *, struct buffer_head *,int);
int default_fat_is_uptodate(struct super_block *, struct buffer_head *);
int default_fat_access(struct super_block *sb,int nr,int new_value);
void default_fat_ll_rw_block (struct super_block *sb, int opr, int nbreq,
			      struct buffer_head *bh[32]);
int default_fat_bmap(struct inode *inode,int block);
ssize_t default_fat_file_write(struct file *filp, const char *buf,
			       size_t count, loff_t *ppos);

struct cvf_format default_cvf = {
	cvf_version: 		0,	/* version - who cares? */	
	cvf_version_text: 	"plain",
	flags:			0,	/* flags - who cares? */
	cvf_bread:		default_fat_bread,
	cvf_getblk:		default_fat_getblk,
	cvf_brelse:		default_fat_brelse,
	cvf_mark_buffer_dirty:	default_fat_mark_buffer_dirty,
	cvf_set_uptodate:	default_fat_set_uptodate,
	cvf_is_uptodate:	default_fat_is_uptodate,
	cvf_ll_rw_block:	default_fat_ll_rw_block,
	fat_access:		default_fat_access,
	cvf_bmap:		default_fat_bmap,
	cvf_file_read:		generic_file_read,
	cvf_file_write:		default_fat_file_write,
};

struct cvf_format *cvf_formats[MAX_CVF_FORMATS];
int cvf_format_use_count[MAX_CVF_FORMATS];

int register_cvf_format(struct cvf_format*cvf_format)
{ int i,j;

  for(i=0;i<MAX_CVF_FORMATS;++i)
  { if(cvf_formats[i]==NULL)
    { /* free slot found, now check version */
      for(j=0;j<MAX_CVF_FORMATS;++j)
      { if(cvf_formats[j])
        { if(cvf_formats[j]->cvf_version==cvf_format->cvf_version)
          { printk("register_cvf_format: version %d already registered\n",
                   cvf_format->cvf_version);
            return -1;
          }
        }
      }
      cvf_formats[i]=cvf_format;
      cvf_format_use_count[i]=0;
      printk("CVF format %s (version id %d) successfully registered.\n",
             cvf_format->cvf_version_text,cvf_format->cvf_version);
      return 0;
    }
  }
  
  printk("register_cvf_format: too many formats\n");
  return -1;
}

int unregister_cvf_format(struct cvf_format*cvf_format)
{ int i;

  for(i=0;i<MAX_CVF_FORMATS;++i)
  { if(cvf_formats[i])
    { if(cvf_formats[i]->cvf_version==cvf_format->cvf_version)
      { if(cvf_format_use_count[i])
        { printk("unregister_cvf_format: format %d in use, cannot remove!\n",
          cvf_formats[i]->cvf_version);
          return -1;
        }
      
        printk("CVF format %s (version id %d) successfully unregistered.\n",
        cvf_formats[i]->cvf_version_text,cvf_formats[i]->cvf_version);
        cvf_formats[i]=NULL;
        return 0;
      }
    }
  }
  
  printk("unregister_cvf_format: format %d is not registered\n",
         cvf_format->cvf_version);
  return -1;
}

void dec_cvf_format_use_count_by_version(int version)
{ int i;

  for(i=0;i<MAX_CVF_FORMATS;++i)
  { if(cvf_formats[i])
    { if(cvf_formats[i]->cvf_version==version)
      { --cvf_format_use_count[i];
        if(cvf_format_use_count[i]<0)
        { cvf_format_use_count[i]=0;
          printk(KERN_EMERG "FAT FS/CVF: This is a bug in cvf_version_use_count\n");
        }
        return;
      }
    }
  }
  
  printk("dec_cvf_format_use_count_by_version: version %d not found ???\n",
         version);
}

int detect_cvf(struct super_block*sb,char*force)
{ int i;
  int found=0;
  int found_i=-1;

  if(force)
    if(strcmp(force,"autoload")==0)
    {
#ifdef CONFIG_KMOD
      request_module("cvf_autoload");
      force=NULL;
#else
      printk("cannot autoload CVF modules: kmod support is not compiled into kernel\n");
      return -1;
#endif
    }
    
#ifdef CONFIG_KMOD
  if(force)
    if(*force)
      request_module(force);
#endif

  if(force)
  { if(*force)
    { for(i=0;i<MAX_CVF_FORMATS;++i)
      { if(cvf_formats[i])
        { if(!strcmp(cvf_formats[i]->cvf_version_text,force))
            return i;
        }
      }
      printk("CVF format %s unknown (module not loaded?)\n",force);
      return -1;
    }
  }

  for(i=0;i<MAX_CVF_FORMATS;++i)
  { if(cvf_formats[i])
    { if(cvf_formats[i]->detect_cvf(sb))
      { ++found;
        found_i=i;
      }
    }
  }
  
  if(found==1)return found_i;
  if(found>1)printk("CVF detection ambiguous, please use cvf_format=xxx option\n"); 
  return -1;
}

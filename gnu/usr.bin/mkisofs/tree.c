/*
 * File tree.c - scan directory  tree and build memory structures for iso9660
 * filesystem

   Written by Eric Youngdale (1993).

   Copyright 1993 Yggdrasil Computing, Incorporated

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* ADD_FILES changes made by Ross Biro biro@yggdrasil.com 2/23/95 */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#ifndef VMS
#if defined(HASSYSMACROS) && !defined(HASMKDEV)
#include <sys/sysmacros.h>
#endif
#include <unistd.h>
#ifdef HASMKDEV
#include <sys/types.h>
#include <sys/mkdev.h>
#endif
#else
#include <sys/file.h>
#include <vms/fabdef.h>
#include "vms.h"
extern char * strdup(const char *);
#endif

#include "mkisofs.h"
#include "iso9660.h"

#include <sys/stat.h>

#include "exclude.h"

#ifdef NON_UNIXFS
#define S_ISLNK(m)	(0)
#define S_ISSOCK(m)	(0)
#define S_ISFIFO(m)	(0)
#else
#ifndef S_ISLNK
#define S_ISLNK(m)	(((m) & S_IFMT) == S_IFLNK)
#endif
#ifndef S_ISSOCK
#define S_ISSOCK(m)	(((m) & S_IFMT) == S_IFSOCK)
#endif
#endif

#ifdef __svr4__
extern char * strdup(const char *);
#endif

/*  WALNUT CREEK CDROM HACK  -- rab 950126 */
static char trans_tbl[] = "00_TRANS.TBL";

static unsigned char symlink_buff[256];

extern int verbose;

struct stat fstatbuf = {0,};  /* We use this for the artificial entries we create */

struct stat root_statbuf = {0, };  /* Stat buffer for root directory */

struct directory * reloc_dir = NULL;


void FDECL1(sort_n_finish, struct directory *, this_dir)
{
  struct directory_entry  *s_entry, *s_entry1;
  time_t current_time;
  struct directory_entry * table;
  int count;
  int new_reclen;
  char *  c;
  int tablesize = 0;
  char newname[34],  rootname[34];

  /* Here we can take the opportunity to toss duplicate entries from the
     directory.  */

  table = NULL;

  if(fstatbuf.st_ctime == 0){
	  time (&current_time);
	  fstatbuf.st_uid = 0;
	  fstatbuf.st_gid = 0;
	  fstatbuf.st_ctime = current_time;
	  fstatbuf.st_mtime = current_time;
	  fstatbuf.st_atime = current_time;
  };

  flush_file_hash();
  s_entry = this_dir->contents;
  while(s_entry){

	  /* First assume no conflict, and handle this case */

	  if(!(s_entry1 = find_file_hash(s_entry->isorec.name))){
		  add_file_hash(s_entry);
		  s_entry = s_entry->next;
		  continue;
	  };

	  if(s_entry1 == s_entry){
		  fprintf(stderr,"Fatal goof\n");
		  exit(1);
	  };
	  /* OK, handle the conflicts.  Try substitute names until we come
	     up with a winner */
	  strcpy(rootname, s_entry->isorec.name);
	  if(full_iso9660_filenames) {
	    if(strlen(rootname) > 27) rootname[27] = 0;
	  }
	  c  = strchr(rootname, '.');
	  if (c) *c = 0;
	  count = 0;
	  while(count < 1000){
		  sprintf(newname,"%s.%3.3d%s", rootname,  count,
			  (s_entry->isorec.flags[0] == 2 ||
			   omit_version_number ? "" : ";1"));

#ifdef VMS
		  /* Sigh.  VAXCRTL seems to be broken here */
		  { int ijk = 0;
		    while(newname[ijk]) {
		      if(newname[ijk] == ' ') newname[ijk] = '0';
		      ijk++;
		    };
		  }
#endif

		  if(!find_file_hash(newname)) break;
		  count++;
	  };
	  if(count >= 1000){
		  fprintf(stderr,"Unable to  generate unique  name for file %s\n", s_entry->name);
		  exit(1);
	  };

	  /* OK, now we have a good replacement name.  Now decide which one
	     of these two beasts should get the name changed */

	  if(s_entry->priority < s_entry1->priority) {
		  fprintf(stderr,"Using %s for  %s%s%s (%s)\n", newname,  this_dir->whole_name, SPATH_SEPARATOR, s_entry->name, s_entry1->name);
		  s_entry->isorec.name_len[0] =  strlen(newname);
		  new_reclen =  sizeof(struct iso_directory_record) -
			  sizeof(s_entry->isorec.name) +
				  strlen(newname);
		  if(use_RockRidge) {
			  if (new_reclen & 1) new_reclen++;  /* Pad to an even byte */
			  new_reclen += s_entry->rr_attr_size;
		  };
		  if (new_reclen & 1) new_reclen++;  /* Pad to an even byte */
		  s_entry->isorec.length[0] = new_reclen;
		  strcpy(s_entry->isorec.name, newname);
	  } else {
		  delete_file_hash(s_entry1);
		  fprintf(stderr,"Using %s for  %s%s%s (%s)\n", newname,  this_dir->whole_name, SPATH_SEPARATOR, s_entry1->name, s_entry->name);
		  s_entry1->isorec.name_len[0] =  strlen(newname);
		  new_reclen =  sizeof(struct iso_directory_record) -
			  sizeof(s_entry1->isorec.name) +
				  strlen(newname);
		  if(use_RockRidge) {
			  if (new_reclen & 1) new_reclen++;  /* Pad to an even byte */
			  new_reclen += s_entry1->rr_attr_size;
		  };
		  if (new_reclen & 1) new_reclen++;  /* Pad to an even byte */
		  s_entry1->isorec.length[0] = new_reclen;
		  strcpy(s_entry1->isorec.name, newname);
		  add_file_hash(s_entry1);
	  };
	  add_file_hash(s_entry);
	  s_entry = s_entry->next;
  };

  if(generate_tables && !find_file_hash(trans_tbl) && (reloc_dir != this_dir)){
	  /* First we need to figure out how big this table is */
	  for (s_entry = this_dir->contents; s_entry; s_entry = s_entry->next){
		  if(strcmp(s_entry->name, ".") == 0  ||
		     strcmp(s_entry->name, "..") == 0) continue;
		  if(s_entry->table) tablesize += 35 + strlen(s_entry->table);
	  };
		  table = (struct directory_entry *)
		e_malloc(sizeof (struct directory_entry));
 	memset(table, 0, sizeof(struct directory_entry));
	table->table = NULL;
	table->next = this_dir->contents;
	this_dir->contents = table;

	table->filedir = root;
	table->isorec.flags[0] = 0;
	table->priority  = 32768;
	iso9660_date(table->isorec.date, fstatbuf.st_ctime);
	table->inode = TABLE_INODE;
	table->dev = (dev_t) UNCACHED_DEVICE;
	set_723(table->isorec.volume_sequence_number, 1);
	set_733(table->isorec.size, tablesize);
	table->size = tablesize;
	table->filedir = this_dir;
	table->name = strdup("<translation table>");
	table->table = (char *) e_malloc(ROUND_UP(tablesize));
	memset(table->table, 0, ROUND_UP(tablesize));
#if 1	  /* WALNUT CREEK -- 950126 */
	iso9660_file_length  (trans_tbl, table, 0);
#else
	iso9660_file_length  (trans_tbl, table, 1);
#endif

	if(use_RockRidge){
		fstatbuf.st_mode = 0444 | S_IFREG;
		fstatbuf.st_nlink = 1;
		generate_rock_ridge_attributes("",
					       trans_tbl, table,
					       &fstatbuf, &fstatbuf, 0);
	};
  };

  for(s_entry = this_dir->contents; s_entry; s_entry = s_entry->next){
	  new_reclen = strlen(s_entry->isorec.name);

	  if(s_entry->isorec.flags[0] ==  2){
		  if (strcmp(s_entry->name,".") && strcmp(s_entry->name,"..")) {
			  path_table_size += new_reclen + sizeof(struct iso_path_table) - 1;
			  if (new_reclen & 1) path_table_size++;
		  } else {
			  new_reclen = 1;
			  if (this_dir == root && strlen(s_entry->name) == 1)
				  path_table_size += sizeof(struct iso_path_table);
		  }
	  };
	  if(path_table_size & 1) path_table_size++;  /* For odd lengths we pad */
	  s_entry->isorec.name_len[0] = new_reclen;

	  new_reclen +=
		  sizeof(struct iso_directory_record) -
			  sizeof(s_entry->isorec.name);

	  if (new_reclen & 1)
		  new_reclen++;
	  if(use_RockRidge){
		  new_reclen += s_entry->rr_attr_size;

		  if (new_reclen & 1)
			  new_reclen++;
	  };
	  if(new_reclen > 0xff) {
		  fprintf(stderr,"Fatal error - RR overflow for file %s\n",
			  s_entry->name);
		  exit(1);
	  };
	  s_entry->isorec.length[0] = new_reclen;
  };

  sort_directory(&this_dir->contents);

  if(table){
	  char buffer[1024];
	  count = 0;
	  for (s_entry = this_dir->contents; s_entry; s_entry = s_entry->next){
		  if(s_entry == table) continue;
		  if(!s_entry->table) continue;
		  if(strcmp(s_entry->name, ".") == 0  ||
		     strcmp(s_entry->name, "..") == 0) continue;

		  sprintf(buffer,"%c %-34s%s",s_entry->table[0],
			  s_entry->isorec.name, s_entry->table+1);
		  memcpy(table->table + count, buffer, strlen(buffer));
		  count +=  strlen(buffer);
		  free(s_entry->table);
		  s_entry->table = NULL;
	};
	if(count !=  tablesize) {
 		fprintf(stderr,"Translation table size mismatch %d %d\n",
			count, tablesize);
		exit(1);
	};
  };

  /* Now go through the directory and figure out how large this one will be.
     Do not split a directory entry across a sector boundary */

  s_entry = this_dir->contents;
  this_dir->ce_bytes = 0;
  while(s_entry){
	  new_reclen = s_entry->isorec.length[0];
	  if ((this_dir->size & (SECTOR_SIZE - 1)) + new_reclen >= SECTOR_SIZE)
		  this_dir->size = (this_dir->size + (SECTOR_SIZE - 1)) &
			  ~(SECTOR_SIZE - 1);
	  this_dir->size += new_reclen;

	  /* See if continuation entries were used on disc */
	  if(use_RockRidge &&
	     s_entry->rr_attr_size != s_entry->total_rr_attr_size) {
	    unsigned char * pnt;
	    int len;
	    int nbytes;

	    pnt = s_entry->rr_attributes;
	    len = s_entry->total_rr_attr_size;

	    /* We make sure that each continuation entry record is not
	       split across sectors, but each file could in theory have more
	       than one CE, so we scan through and figure out what we need. */

	    while(len > 3){
	      if(pnt[0] == 'C' && pnt[1] == 'E') {
		nbytes = get_733(pnt+20);

		if((this_dir->ce_bytes & (SECTOR_SIZE - 1)) + nbytes >=
		   SECTOR_SIZE) this_dir->ce_bytes =
		     ROUND_UP(this_dir->ce_bytes);
		/* Now store the block in the ce buffer */
		this_dir->ce_bytes += nbytes;
		if(this_dir->ce_bytes & 1) this_dir->ce_bytes++;
	      };
	      len -= pnt[2];
	      pnt += pnt[2];
	    }
	  }
	  s_entry = s_entry->next;
  }
}

static void generate_reloc_directory()
{
	int new_reclen;
	time_t current_time;
	struct directory_entry  *s_entry;

	/* Create an  entry for our internal tree */
	time (&current_time);
	reloc_dir = (struct directory *)
		e_malloc(sizeof(struct directory));
	memset(reloc_dir, 0, sizeof(struct directory));
	reloc_dir->parent = root;
	reloc_dir->next = root->subdir;
	root->subdir = reloc_dir;
	reloc_dir->depth = 1;
	reloc_dir->whole_name = strdup("./rr_moved");
	reloc_dir->de_name =  strdup("rr_moved");
	reloc_dir->extent = 0;

	new_reclen  = strlen(reloc_dir->de_name);

	/* Now create an actual directory  entry */
	s_entry = (struct directory_entry *)
		e_malloc(sizeof (struct directory_entry));
	memset(s_entry, 0, sizeof(struct directory_entry));
	s_entry->next = root->contents;
	reloc_dir->self = s_entry;

	root->contents = s_entry;
	root->contents->name = strdup(reloc_dir->de_name);
	root->contents->filedir = root;
	root->contents->isorec.flags[0] = 2;
	root->contents->priority  = 32768;
	iso9660_date(root->contents->isorec.date, current_time);
	root->contents->inode = UNCACHED_INODE;
	root->contents->dev = (dev_t) UNCACHED_DEVICE;
	set_723(root->contents->isorec.volume_sequence_number, 1);
	iso9660_file_length (reloc_dir->de_name, root->contents, 1);

	if(use_RockRidge){
		fstatbuf.st_mode = 0555 | S_IFDIR;
		fstatbuf.st_nlink = 2;
		generate_rock_ridge_attributes("",
					       "rr_moved", s_entry,
					       &fstatbuf, &fstatbuf, 0);
	};

	/* Now create the . and .. entries in rr_moved */
	/* Now create an actual directory  entry */
	s_entry = (struct directory_entry *)
		e_malloc(sizeof (struct directory_entry));
	memcpy(s_entry, root->contents,
	       sizeof(struct directory_entry));
	s_entry->name = strdup(".");
	iso9660_file_length (".", s_entry, 1);

	s_entry->filedir = reloc_dir;
	reloc_dir->contents = s_entry;

	if(use_RockRidge){
		fstatbuf.st_mode = 0555 | S_IFDIR;
		fstatbuf.st_nlink = 2;
		generate_rock_ridge_attributes("",
					       ".", s_entry,
					       &fstatbuf, &fstatbuf, 0);
	};

	s_entry = (struct directory_entry *)
		e_malloc(sizeof (struct directory_entry));
	memcpy(s_entry, root->contents,
	       sizeof(struct directory_entry));
	s_entry->name = strdup("..");
	iso9660_file_length ("..", s_entry, 1);
	s_entry->filedir = root;
	reloc_dir->contents->next = s_entry;
	reloc_dir->contents->next->next = NULL;
	if(use_RockRidge){
		fstatbuf.st_mode = 0555 | S_IFDIR;
		fstatbuf.st_nlink = 2;
		generate_rock_ridge_attributes("",
					       "..", s_entry,
					       &root_statbuf, &root_statbuf, 0);
	};
}

static void FDECL1(increment_nlink, struct directory_entry *, s_entry){
  unsigned char * pnt;
  int len, nlink;

  pnt = s_entry->rr_attributes;
  len = s_entry->total_rr_attr_size;
  while(len){
    if(pnt[0] == 'P' && pnt[1] == 'X') {
      nlink =  get_733(pnt+12);
      set_733(pnt+12, nlink+1);
      break;
    };
    len -= pnt[2];
    pnt += pnt[2];
  };
}

void finish_cl_pl_entries(){
  struct directory_entry  *s_entry, *s_entry1;
  struct directory *  d_entry;

  s_entry = reloc_dir->contents;
   s_entry  = s_entry->next->next;  /* Skip past . and .. */
  for(; s_entry; s_entry = s_entry->next){
	  d_entry = reloc_dir->subdir;
	  while(d_entry){
		  if(d_entry->self == s_entry) break;
		  d_entry = d_entry->next;
	  };
	  if(!d_entry){
		  fprintf(stderr,"Unable to locate directory parent\n");
		  exit(1);
	  };

	  /* First fix the PL pointer in the directory in the rr_reloc dir */
	  s_entry1 = d_entry->contents->next;
	  set_733(s_entry1->rr_attributes +  s_entry1->total_rr_attr_size - 8,
		  s_entry->filedir->extent);

	  /* Now fix the CL pointer */
	  s_entry1 = s_entry->parent_rec;

	  set_733(s_entry1->rr_attributes +  s_entry1->total_rr_attr_size - 8,
		  d_entry->extent);

	  s_entry->filedir = reloc_dir;  /* Now we can fix this */
  }
  /* Next we need to modify the NLINK terms in the assorted root directory records
     to account for the presence of the RR_MOVED directory */

  increment_nlink(root->self);
  increment_nlink(root->self->next);
  d_entry = root->subdir;
  while(d_entry){
    increment_nlink(d_entry->contents->next);
    d_entry = d_entry->next;
  };
}

#ifdef ADD_FILES
/* This function looks up additions. */
char *
FDECL3(look_up_addition,char **, newpath, char *,path, struct dirent **,de) {
  char *dup_path;
  char *cp;
  struct file_adds *f;
  struct file_adds *tmp;

  f=root_file_adds;
  if (!f) return NULL;

  /* I don't trust strtok */
  dup_path = strdup (path);

  cp = strtok (dup_path, SPATH_SEPARATOR);
  while (cp != NULL) {
    for (tmp = f->child; tmp != NULL; tmp=tmp->next) {
      if (strcmp (tmp->name, cp) == 0) break;
    }
    if (tmp == NULL) {
      /* no match */
      free (dup_path);
      return (NULL);
    }
    f = tmp;
    cp = strtok(NULL, SPATH_SEPARATOR);
  }
  free (dup_path);

  /* looks like we found something. */
  if (tmp->used >= tmp->add_count) return (NULL);

  *newpath = tmp->adds[tmp->used].path;
  tmp->used++;
  *de = &(tmp->de);
  return (tmp->adds[tmp->used-1].name);
  
}

/* This function lets us add files from outside the standard file tree.
   It is useful if we want to duplicate a cd, but add/replace things.
   We should note that the real path will be used for exclusions. */

struct dirent *
FDECL3(readdir_add_files, char **, pathp, char *,path, DIR *, dir){
  struct dirent *de;

  char *addpath;
  char *name;

  de = readdir (dir);
  if (de) {
    return (de);
  }

  name=look_up_addition (&addpath, path, &de);

  if (!name) {
    return;
  }

  *pathp=addpath;
  
  /* Now we must create the directory entry. */
  /* fortuneately only the name seems to matter. */
  /*
  de->d_ino = -1;
  de->d_off = 0;
  de->d_reclen = strlen (name);
  */
  strncpy (de->d_name, name, NAME_MAX);
  de->d_name[NAME_MAX]=0;
  return (de);

}
#else
struct dirent *
FDECL3(readdir_add_files, char **, pathp, char *,path, DIR *, dir){
  return (readdir (dir));
}
#endif
/*
 * This function scans the directory tree, looking for files, and it makes
 * note of everything that is found.  We also begin to construct the ISO9660
 * directory entries, so that we can determine how large each directory is.
 */

int
FDECL2(scan_directory_tree,char *, path, struct directory_entry *, de){
  DIR * current_dir;
  char whole_path[1024];
  struct dirent * d_entry;
  struct directory_entry  *s_entry, *s_entry1;
  struct directory * this_dir, *next_brother, *parent;
  struct stat statbuf, lstatbuf;
  int status, dflag;
  char * cpnt;
  int new_reclen;
  int deep_flag;
  char *old_path;

  current_dir = opendir(path);
  d_entry = NULL;

  /* Apparently NFS sometimes allows you to open the directory, but
     then refuses to allow you to read the contents.  Allow for this */

  old_path = path;

  if(current_dir) d_entry = readdir_add_files(&path, old_path, current_dir);

  if(!current_dir || !d_entry) {
	  fprintf(stderr,"Unable to open directory %s\n", path);
	  de->isorec.flags[0] &= ~2; /* Mark as not a directory */
	  if(current_dir) closedir(current_dir);
	  return 0;
  };

  parent = de->filedir;
  /* Set up the struct for the current directory, and insert it into the
     tree */

#ifdef VMS
  vms_path_fixup(path);
#endif

  this_dir = (struct directory *) e_malloc(sizeof(struct directory));
  this_dir->next = NULL;
  new_reclen = 0;
  this_dir->subdir = NULL;
  this_dir->self = de;
  this_dir->contents = NULL;
  this_dir->whole_name = strdup(path);
  cpnt = strrchr(path, PATH_SEPARATOR);
  if(cpnt)
    cpnt++;
  else
    cpnt = path;
  this_dir->de_name = strdup(cpnt);
  this_dir->size = 0;
  this_dir->extent = 0;

  if(!parent || parent == root){
    if (!root) {
      root = this_dir;  /* First time through for root directory only */
      root->depth = 0;
      root->parent = root;
    } else {
      this_dir->depth = 1;
      if(!root->subdir)
	root->subdir = this_dir;
      else {
	next_brother = root->subdir;
	while(next_brother->next) next_brother = next_brother->next;
	next_brother->next = this_dir;
      };
      this_dir->parent = parent;
    };
  } else {
	  /* Come through here for  normal traversal of  tree */
#ifdef DEBUG
	  fprintf(stderr,"%s(%d) ", path, this_dir->depth);
#endif
	  if(parent->depth >  RR_relocation_depth) {
		  fprintf(stderr,"Directories too deep  %s\n", path);
		  exit(1);
	  };

	  this_dir->parent = parent;
	  this_dir->depth = parent->depth + 1;

	  if(!parent->subdir)
		  parent->subdir = this_dir;
	  else {
		  next_brother = parent->subdir;
		  while(next_brother->next) next_brother = next_brother->next;
		  next_brother->next = this_dir;
	  };
  };

/* Now we scan the directory itself, and look at what is inside of it. */

  dflag = 0;
  while(1==1){

    /* The first time through, skip this, since we already asked for
       the first entry when we opened the directory. */
    if(dflag) d_entry = readdir_add_files(&path, old_path, current_dir);
    dflag++;

    if(!d_entry) break;

    /* OK, got a valid entry */

    /* If we do not want all files, then pitch the backups. */
    if(!all_files){
	    if(strchr(d_entry->d_name,'~')) continue;
	    if(strchr(d_entry->d_name,'#')) continue;
    };

    if(strlen(path)+strlen(d_entry->d_name) + 2 > sizeof(whole_path)){
      fprintf(stderr, "Overflow of stat buffer\n");
      exit(1);
    };

    /* Generate the complete ASCII path for this file */
    strcpy(whole_path, path);
#ifndef VMS
    if(whole_path[strlen(whole_path)-1] != '/')
      strcat(whole_path, "/");
#endif
    strcat(whole_path, d_entry->d_name);

    /* Should we exclude this file? */
    if (is_excluded(whole_path)) {
      if (verbose) {
        fprintf(stderr, "Excluded: %s\n",whole_path);
      }
      continue;
    }
#if 0
    if (verbose)  fprintf(stderr, "%s\n",whole_path);
#endif
    status = stat(whole_path, &statbuf);

    lstat(whole_path, &lstatbuf);

    if(this_dir == root && strcmp(d_entry->d_name, ".") == 0)
      root_statbuf = statbuf;  /* Save this for later on */

    /* We do this to make sure that the root entries are consistent */
    if(this_dir == root && strcmp(d_entry->d_name, "..") == 0) {
      statbuf = root_statbuf;
      lstatbuf = root_statbuf;
    };

    if(S_ISLNK(lstatbuf.st_mode)){

	    /* Here we decide how to handle the symbolic links.  Here
	       we handle the general case - if we are not following
	       links or there is an error, then we must change
	       something.  If RR is in use, it is easy, we let RR
	       describe the file.  If not, then we punt the file. */

	    if((status || !follow_links)){
		    if(use_RockRidge){
			    status = 0;
			    statbuf.st_size = 0;
			    STAT_INODE(statbuf) = UNCACHED_INODE;
			    statbuf.st_dev = (dev_t) UNCACHED_DEVICE;
			    statbuf.st_mode = (statbuf.st_mode & ~S_IFMT) | S_IFREG;
		    } else {
			    if(follow_links) fprintf(stderr,
				    "Unable to stat file %s - ignoring and continuing.\n",
				    whole_path);
			    else fprintf(stderr,
				    "Symlink %s ignored - continuing.\n",
				    whole_path);
			    continue;  /* Non Rock Ridge discs - ignore all symlinks */
		    };
	    }

	    if( follow_links
	       && S_ISDIR(statbuf.st_mode) ) 
	      {
		if(   strcmp(d_entry->d_name, ".")
		   && strcmp(d_entry->d_name, "..") )
		  {
		    if(find_directory_hash(statbuf.st_dev, STAT_INODE(statbuf)))
		      {
			if(!use_RockRidge) 
			  {
			    fprintf(stderr, "Already cached directory seen (%s)\n", 
				    whole_path);
			    continue;
			  }
			statbuf.st_size = 0;
			STAT_INODE(statbuf) = UNCACHED_INODE;
			statbuf.st_dev = (dev_t) UNCACHED_DEVICE;
			statbuf.st_mode = (statbuf.st_mode & ~S_IFMT) | S_IFREG;
		    } else {
		      lstatbuf = statbuf;
		      add_directory_hash(statbuf.st_dev, STAT_INODE(statbuf));
		    }
		  }
	      }
    }

    /*
     * Add directories to the cache so that we don't waste space even
     * if we are supposed to be following symlinks.
     */
    if( follow_links
       && strcmp(d_entry->d_name, ".")
       && strcmp(d_entry->d_name, "..")
       && S_ISDIR(statbuf.st_mode) ) 
	  {
	    add_directory_hash(statbuf.st_dev, STAT_INODE(statbuf));
	  }
#ifdef VMS
    if(!S_ISDIR(lstatbuf.st_mode) && (statbuf.st_fab_rfm != FAB$C_FIX &&
				      statbuf.st_fab_rfm != FAB$C_STMLF)) {
      fprintf(stderr,"Warning - file %s has an unsupported VMS record"
	      " format (%d)\n",
	      whole_path, statbuf.st_fab_rfm);
    }
#endif

    if(S_ISREG(lstatbuf.st_mode) && (status = access(whole_path, R_OK))){
      fprintf(stderr, "File %s is not readable (errno = %d) - ignoring\n",
	      whole_path, errno);
      continue;
    }

    /* Add this so that we can detect directory loops with hard links.
     If we are set up to follow symlinks, then we skip this checking. */
    if(   !follow_links 
       && S_ISDIR(lstatbuf.st_mode) 
       && strcmp(d_entry->d_name, ".") 
       && strcmp(d_entry->d_name, "..") ) 
      {
	    if(find_directory_hash(statbuf.st_dev, STAT_INODE(statbuf))) {
		    fprintf(stderr,"Directory loop - fatal goof (%s %lx %lu).\n",
			    whole_path, (unsigned long) statbuf.st_dev,
			    (unsigned long) STAT_INODE(statbuf));
		    exit(1);
	    };
	    add_directory_hash(statbuf.st_dev, STAT_INODE(statbuf));
    };

    if (!S_ISCHR(lstatbuf.st_mode) && !S_ISBLK(lstatbuf.st_mode) &&
	!S_ISFIFO(lstatbuf.st_mode) && !S_ISSOCK(lstatbuf.st_mode)
	&& !S_ISLNK(lstatbuf.st_mode) && !S_ISREG(lstatbuf.st_mode) &&
	!S_ISDIR(lstatbuf.st_mode)) {
      fprintf(stderr,"Unknown file type %s - ignoring and continuing.\n",
	      whole_path);
      continue;
    };

    /* Who knows what trash this is - ignore and continue */

    if(status) {
	    fprintf(stderr,
		    "Unable to stat file %s - ignoring and continuing.\n",
		    whole_path);
	    continue;
    };

    s_entry = (struct directory_entry *)
      e_malloc(sizeof (struct directory_entry));
    s_entry->next = this_dir->contents;
    this_dir->contents = s_entry;
    deep_flag = 0;
    s_entry->table = NULL;

    s_entry->name = strdup(d_entry->d_name);
    s_entry->whole_name = strdup (whole_path);

    s_entry->filedir = this_dir;
    s_entry->isorec.flags[0] = 0;
    s_entry->isorec.ext_attr_length[0] = 0;
    iso9660_date(s_entry->isorec.date, statbuf.st_ctime);
    s_entry->isorec.file_unit_size[0] = 0;
    s_entry->isorec.interleave[0] = 0;
    if(parent && parent ==  reloc_dir && strcmp(d_entry->d_name,  "..") == 0){
	    s_entry->inode = UNCACHED_INODE;
	    s_entry->dev = (dev_t) UNCACHED_DEVICE;
	    deep_flag  = NEED_PL;
    } else  {
	    s_entry->inode = STAT_INODE(statbuf);
	    s_entry->dev = statbuf.st_dev;
    };
    set_723(s_entry->isorec.volume_sequence_number, 1);
    iso9660_file_length(d_entry->d_name, s_entry, S_ISDIR(statbuf.st_mode));
    s_entry->rr_attr_size = 0;
    s_entry->total_rr_attr_size = 0;
    s_entry->rr_attributes = NULL;

    /* Directories are assigned sizes later on */
    if (!S_ISDIR(statbuf.st_mode)) {
	set_733(s_entry->isorec.size, statbuf.st_size);

	if (S_ISCHR(lstatbuf.st_mode) || S_ISBLK(lstatbuf.st_mode) ||
	    S_ISFIFO(lstatbuf.st_mode) || S_ISSOCK(lstatbuf.st_mode)
	  || S_ISLNK(lstatbuf.st_mode))
	  s_entry->size = 0;
	else
	  s_entry->size = statbuf.st_size;
    } else
      s_entry->isorec.flags[0] = 2;

    if (strcmp(d_entry->d_name,".") && strcmp(d_entry->d_name,"..") &&
	S_ISDIR(statbuf.st_mode) && this_dir->depth >  RR_relocation_depth){
		  if(!reloc_dir) generate_reloc_directory();

		  s_entry1 = (struct directory_entry *)
			  e_malloc(sizeof (struct directory_entry));
		  memcpy(s_entry1, this_dir->contents,
			 sizeof(struct directory_entry));
		  s_entry1->table = NULL;
		  s_entry1->name = strdup(this_dir->contents->name);
		  s_entry1->next = reloc_dir->contents;
		  reloc_dir->contents = s_entry1;
		  s_entry1->priority  =  32768;
		  s_entry1->parent_rec = this_dir->contents;

		  deep_flag = NEED_RE;

		  if(use_RockRidge) {
			  generate_rock_ridge_attributes(whole_path,
							 d_entry->d_name, s_entry1,
							 &statbuf, &lstatbuf, deep_flag);
		  };

		  deep_flag = 0;

		  /* We need to set this temporarily so that the parent to this is correctly
		     determined. */
		  s_entry1->filedir = reloc_dir;
		  scan_directory_tree(whole_path, s_entry1);
		  s_entry1->filedir = this_dir;

		  statbuf.st_size = 0;
		  statbuf.st_mode &= 0777;
		  set_733(s_entry->isorec.size, 0);
		  s_entry->size = 0;
		  s_entry->isorec.flags[0] = 0;
		  s_entry->inode = UNCACHED_INODE;
		  deep_flag = NEED_CL;
	  };

    if(generate_tables && strcmp(s_entry->name, ".") && strcmp(s_entry->name, "..")) {
	    char  buffer[2048];
	    switch(lstatbuf.st_mode & S_IFMT){
	    case S_IFDIR:
		    sprintf(buffer,"D\t%s\n",
			    s_entry->name);
		    break;
#ifndef NON_UNIXFS
	    case S_IFBLK:
		    sprintf(buffer,"B\t%s\t%lu %lu\n",
			    s_entry->name,
			    (unsigned long) major(statbuf.st_rdev),
			    (unsigned long) minor(statbuf.st_rdev));
		    break;
	    case S_IFIFO:
		    sprintf(buffer,"P\t%s\n",
			    s_entry->name);
		    break;
	    case S_IFCHR:
		    sprintf(buffer,"C\t%s\t%lu %lu\n",
			    s_entry->name,
			    (unsigned long) major(statbuf.st_rdev),
			    (unsigned long) minor(statbuf.st_rdev));
		    break;
	    case S_IFLNK:
		    readlink(whole_path, symlink_buff, sizeof(symlink_buff));
		    sprintf(buffer,"L\t%s\t%s\n",
			    s_entry->name, symlink_buff);
		    break;
	    case S_IFSOCK:
		    sprintf(buffer,"S\t%s\n",
			    s_entry->name);
		    break;
#endif /* NON_UNIXFS */
	    case S_IFREG:
	    default:
		    sprintf(buffer,"F\t%s\n",
			    s_entry->name);
		    break;
	    };
	    s_entry->table = strdup(buffer);
    };

    if(S_ISDIR(statbuf.st_mode)){
            int dflag;
	    if (strcmp(d_entry->d_name,".") && strcmp(d_entry->d_name,"..")) {
		dflag = scan_directory_tree(whole_path, s_entry);
	    /* If unable to scan directory, mark this as a non-directory */
	        if(!dflag)
		  lstatbuf.st_mode = (lstatbuf.st_mode & ~S_IFMT) | S_IFREG;
	      }
	  };

  if(use_RockRidge && this_dir == root && strcmp(s_entry->name, ".")  == 0)
	  deep_flag |= NEED_CE | NEED_SP;  /* For extension record */

    /* Now figure out how much room this file will take in the directory */

    if(use_RockRidge) {
	    generate_rock_ridge_attributes(whole_path,
					   d_entry->d_name, s_entry,
					   &statbuf, &lstatbuf, deep_flag);

    }
  }
  closedir(current_dir);
  sort_n_finish(this_dir);
  return 1;
}


void FDECL2(generate_iso9660_directories, struct directory *, node, FILE*, outfile){
  struct directory * dpnt;

  dpnt = node;

  while (dpnt){
    generate_one_directory(dpnt, outfile);
    if(dpnt->subdir) generate_iso9660_directories(dpnt->subdir, outfile);
    dpnt = dpnt->next;
  };
}

void FDECL1(dump_tree, struct directory *, node){
  struct directory * dpnt;

  dpnt = node;

  while (dpnt){
    fprintf(stderr,"%4d %5d %s\n",dpnt->extent, dpnt->size, dpnt->de_name);
    if(dpnt->subdir) dump_tree(dpnt->subdir);
    dpnt = dpnt->next;
  };
}


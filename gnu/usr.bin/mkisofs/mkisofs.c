/*
 * Program mkisofs.c - generate iso9660 filesystem  based upon directory
 * tree on hard disk.

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

#include "mkisofs.h"

#include <assert.h>

#ifdef linux
#include <getopt.h>
#endif

#include "iso9660.h"
#include <ctype.h>

#ifndef VMS
#include <time.h>
#else
#include <sys/time.h>
#include "vms.h"
#endif

#include <stdlib.h>
#include <sys/stat.h>

#ifndef VMS
#include <unistd.h>
#endif

#include "exclude.h"

#ifdef __NetBSD__
#include <sys/time.h>
#include <sys/resource.h>
#endif

struct directory * root = NULL;

static char version_string[] = "mkisofs v1.04";

FILE * discimage;
unsigned int next_extent = 0;
unsigned int last_extent = 0;
unsigned int path_table_size = 0;
unsigned int path_table[4] = {0,};
unsigned int path_blocks = 0;
struct iso_directory_record root_record;
static int timezone_offset;
char * extension_record = NULL;
int extension_record_extent = 0;
static  int extension_record_size = 0;

/* These variables are associated with command line options */
int use_RockRidge = 0;
int verbose = 0;
int all_files  = 0;
int follow_links = 0;
int generate_tables = 0;
char * preparer = PREPARER_DEFAULT;
char * publisher = PUBLISHER_DEFAULT;
char * appid = APPID_DEFAULT;
char * copyright = COPYRIGHT_DEFAULT;
char * biblio = BIBLIO_DEFAULT;
char * abstract = ABSTRACT_DEFAULT;
char * volset_id = VOLSET_ID_DEFAULT;
char * volume_id = VOLUME_ID_DEFAULT;
char * system_id = SYSTEM_ID_DEFAULT;

int omit_period = 0;             /* Violates iso9660, but these are a pain */
int transparent_compression = 0; /* So far only works with linux */
int omit_version_number = 0;     /* May violate iso9660, but noone uses vers*/
int RR_relocation_depth = 6;     /* Violates iso9660, but most systems work */
int full_iso9660_filenames = 0;  /* Used with Amiga.  Disc will not work with
				  DOS */
int allow_leading_dots = 0;	 /* DOS cannot read names with leading dots */

struct rcopts{
  char * tag;
  char ** variable;
};

struct rcopts rcopt[] = {
  {"PREP", &preparer},
  {"PUBL", &publisher},
  {"APPI", &appid},
  {"COPY", &copyright},
  {"BIBL", &biblio},
  {"ABST", &abstract},
  {"VOLS", &volset_id},
  {"VOLI", &volume_id},
  {"SYSI", &system_id},
  {NULL, NULL}
};

#ifdef ultrix
char *strdup(s)
char *s;{char *c;if(c=(char *)malloc(strlen(s)+1))strcpy(c,s);return c;}
#endif

void FDECL1(read_rcfile, char *, appname)
{
  FILE * rcfile;
  struct rcopts * rco;
  char * pnt, *pnt1;
  char linebuffer[256];
  rcfile = fopen(".mkisofsrc","r");

  if(!rcfile) {
    if(strlen(appname)+sizeof(".mkisofsrc") > sizeof(linebuffer)) return;
    strcpy(linebuffer, appname);
    pnt = strrchr(linebuffer,'/');
    if(!pnt) return;
    pnt++;
    strcpy(pnt, ".mkisofsrc");
    rcfile = fopen(linebuffer,"r");
    fprintf(stderr, "Using %s.\n", linebuffer);
  } else {
    fprintf(stderr, "Using ./.mkisofsrc.\n");
  }

  if(!rcfile) return;

  /* OK, we got it.  Now read in the lines and parse them */
  while(!feof(rcfile))
    {
      fgets(linebuffer, sizeof(linebuffer), rcfile);
      pnt = linebuffer;
      while(1==1) {
	if(*pnt == ' ' || *pnt == '\t' || *pnt == '\n' || *pnt == 0) break;
	if(islower(*pnt)) *pnt = toupper(*pnt);
	pnt++;
      }
      /* OK, now find the '=' sign */
      while(*pnt && *pnt != '=' && *pnt != '#') pnt++;

      if(*pnt == '#') continue; /* SKip comment */
      if(*pnt != '=') continue; /* Skip to next line */
      pnt++; /* Skip past '=' sign */

      while(*pnt == ' ' || *pnt == '\t') pnt++; /* And skip past whitespace */

      /* Now get rid of trailing newline */
      pnt1 = pnt;
      while(*pnt1) {
	if(*pnt1 == '\n') *pnt1 = 0;
	else
	  pnt1++;
      };
      pnt1 = linebuffer;
      while(*pnt1 == ' ' || *pnt1 == '\t') pnt1++;
      /* OK, now figure out which option we have */
      for(rco = rcopt; rco->tag; rco++) {
	if(strncmp(rco->tag, pnt1, 4) == 0)
	  {
	    *rco->variable = strdup(pnt);
	    break;
	  };
      }
    }
  fclose(rcfile);
}

char * path_table_l = NULL;
char * path_table_m = NULL;
int goof = 0;

void usage(){
	fprintf(stderr,"Usage:\n");
	fprintf(stderr,
"mkisofs [-o outfile] [-R] [-V volid] [-v] [-a] \
[-T]\n [-l] [-d] [-V] [-D] [-L] [-p preparer] \
[-P publisher] [ -A app_id ] [-z] \
[-x path -x path ...] path\n");
	exit(1);
}

int get_iso9660_timezone_offset(){
  struct tm gm;
  struct tm * pt;
  time_t ctime;
  int local_min, gmt_min;

  time(&ctime);
  pt = gmtime(&ctime);
  gm = *pt;
  pt = localtime(&ctime);

  if(gm.tm_year < pt->tm_year)
    gm.tm_yday = -1;

  if(gm.tm_year > pt->tm_year)
    pt->tm_yday = -1;

  gmt_min = gm.tm_min + 60*(gm.tm_hour + 24*gm.tm_yday);
  local_min = pt->tm_min + 60*(pt->tm_hour + 24*pt->tm_yday);
  return (gmt_min - local_min)/15;
}


/* Fill in date in the iso9660 format */
int FDECL2(iso9660_date,char *, result, time_t, ctime){
  struct tm *local;
  local = localtime(&ctime);
  result[0] = local->tm_year;
  result[1] = local->tm_mon + 1;
  result[2] = local->tm_mday;
  result[3] = local->tm_hour;
  result[4] = local->tm_min;
  result[5] = local->tm_sec;
  result[6] = timezone_offset;
  return 0;
}

int FDECL3(iso9660_file_length,const char*, name, struct directory_entry *, sresult,
			int, dirflag){
  int seen_dot = 0;
  int seen_semic = 0;
  char * result;
  int priority = 32767;
  int tildes = 0;
  int ignore = 0;
  int extra = 0;
  int current_length = 0;
  int chars_after_dot = 0;
  int chars_before_dot = 0;
  const char * pnt;
  result = sresult->isorec.name;

  if(strcmp(name,".") == 0){
    if(result) *result = 0;
    return 1;
  };

  if(strcmp(name,"..") == 0){
    if(result) {
	    *result++ = 1;
	    *result++ = 0;
    }
    return 1;
  };

  pnt = name;
  while(*pnt){
#ifdef VMS
    if(strcmp(pnt,".DIR;1") == 0) break;
#endif
    if(*pnt == '#') {priority = 1; pnt++; continue; };
    if(*pnt == '~') {priority = 1; tildes++; pnt++; continue;};
    if(*pnt == ';') {seen_semic = 1; *result++ = *pnt++; continue; };
    if(ignore) {pnt++; continue;};
    if(seen_semic){
      if(*pnt >= '0' && *pnt <= '9') *result++ = *pnt;
      extra++;
      pnt++;
      continue;
    };
    if(full_iso9660_filenames) {
      /* Here we allow a more relaxed syntax. */
      if(*pnt == '.') {
	if (seen_dot) {ignore++; continue;}
	seen_dot++;
      }
      if(current_length < 30) *result++ = (islower(*pnt) ? toupper(*pnt) : *pnt);
    } else { /* Dos style filenames */
      if(*pnt == '.') {
        if (!chars_before_dot && !allow_leading_dots) {
	  /* DOS can't read files with dot first */
          chars_before_dot++;
          if (result) *result++ = '_'; /* Substitute underscore */
        } else {
          if (seen_dot) {ignore++; continue;}
	  if(result) *result++ = '.';
	  seen_dot++;
        }
      } else if (seen_dot) {
	if(chars_after_dot < 3) {
	  chars_after_dot++;
	  if(result) *result++ = (islower(*pnt) ? toupper(*pnt) : *pnt);
	}
      } else {
	if(chars_before_dot < 8) {
	  chars_before_dot++;
	  if(result) *result++ = (islower(*pnt) ? toupper(*pnt) : *pnt);
	};
      };
    };
    current_length++;
    pnt++;
  };

  if(tildes == 2){
    int prio1 = 0;
    pnt = name;
    while (*pnt && *pnt != '~') pnt++;
    if (*pnt) pnt++;
    while(*pnt && *pnt != '~'){
      prio1 = 10*prio1 + *pnt - '0';
      pnt++;
    };
    priority = prio1;
  };

  if (!dirflag){
    if (!seen_dot && !omit_period) {
      if (result) *result++ = '.';
      extra++;
    };
    if(!omit_version_number && !seen_semic) {
      if(result){
	*result++ = ';';
	*result++ = '1';
      };
      extra += 2;
    }
  };
  if(result) *result++ = 0;

#if 1  /* WALNUT CREEK HACKS -- rab 950126 */
{
  int i, c, len;
  char *r;

  assert(result);
  assert(omit_version_number);
  assert(omit_period);
  assert(extra == 0);
  r = sresult->isorec.name;
  len = strlen(r);
  if (r[len - 1] == '.') {
      assert(seen_dot && chars_after_dot == 0);
      r[--len] = '\0';
      seen_dot = 0;
  }
  for (i = 0; i < len; ++i) {
      c = r[i];
      if (c == '.') {
	  if (dirflag) {
	      fprintf(stderr, "changing DIR %s to ", r);
	      r[i] = '\0';
	      fprintf(stderr, "%s\n", r);
	      chars_after_dot = 0;
	      seen_dot = 0;
	      extra = 0;
	      break;
	  }
      } else if (!isalnum(c) && c != '_') {
	  fprintf(stderr, "changing %s to ", r);
	  r[i] = '_';
	  fprintf(stderr, "%s\n", r);
      }
  }
}
#endif

  sresult->priority = priority;
  return chars_before_dot + chars_after_dot + seen_dot + extra;
}

#ifdef ADD_FILES

struct file_adds *root_file_adds = NULL;

void
FDECL2(add_one_file, char *, addpath, char *, path )
{
  char *cp;
  char *name;
  struct file_adds *f;
  struct file_adds *tmp;

  f = root_file_adds;
  tmp = NULL;

  name = rindex (addpath, PATH_SEPARATOR);
  if (name == NULL) {
    name = addpath;
  } else {
    name++;
  }

  cp = strtok (addpath, SPATH_SEPARATOR);

  while (cp != NULL && strcmp (name, cp)) {
     if (f == NULL) {
        root_file_adds = e_malloc (sizeof *root_file_adds);
        f=root_file_adds;
        f->name = NULL;
        f->child = NULL;
        f->next = NULL;
        f->add_count = 0;
        f->adds = NULL;
	f->used = 0;
     }
    if (f->child) {
      for (tmp = f->child; tmp->next != NULL; tmp =tmp->next) {
         if (strcmp (tmp->name, cp) == 0) {
           f = tmp;
           goto next;
         }
      }
      if (strcmp (tmp->name, cp) == 0) {
          f=tmp;
          goto next;
      }
      /* add a new node. */
      tmp->next = e_malloc (sizeof (*tmp->next));
      f=tmp->next;
      f->name = strdup (cp);
      f->child = NULL;
      f->next = NULL;
      f->add_count = 0;
      f->adds = NULL;
      f->used = 0;
    } else {
      /* no children. */
      f->child = e_malloc (sizeof (*f->child));
      f = f->child;
      f->name = strdup (cp);
      f->child = NULL;
      f->next = NULL;
      f->add_count = 0;
      f->adds = NULL;
      f->used = 0;

    }
   next:
     cp = strtok (NULL, SPATH_SEPARATOR);
   }
  /* Now f if non-null points to where we should add things */
  if (f == NULL) {
     root_file_adds = e_malloc (sizeof *root_file_adds);
     f=root_file_adds;
     f->name = NULL;
     f->child = NULL;
     f->next = NULL;
     f->add_count = 0;
     f->adds = NULL;
   }

  /* Now f really points to where we should add this name. */
  f->add_count++;
  f->adds = realloc (f->adds, sizeof (*f->adds)*f->add_count);
  f->adds[f->add_count-1].path = strdup (path);
  f->adds[f->add_count-1].name = strdup (name);
}

void
FDECL3(add_file_list, int, argc, char **,argv, int, ind)
{
  char *ptr;
  char *dup_arg;

  while (ind < argc) {
     dup_arg = strdup (argv[ind]);
     ptr = index (dup_arg,'=');
     if (ptr == NULL) {
        free (dup_arg);
        return;
     }
     *ptr = 0;
     ptr++;
     add_one_file (dup_arg, ptr);
     free (dup_arg);
     ind++;
  }
}
void
FDECL1(add_file, char *, filename)
{
  char buff[1024];
  FILE *f;
  char *ptr;
  char *p2;
  int count=0;

  if (strcmp (filename, "-") == 0) {
    f = stdin;
  } else {
    f = fopen (filename, "r");
    if (f == NULL) {
      perror ("fopen");
      exit (1);
    }
  }
  while (fgets (buff, 1024, f)) {
    count++;
    ptr = buff;
    while (isspace (*ptr)) ptr++;
    if (*ptr==0) continue;
    if (*ptr=='#') continue;

    if (ptr[strlen(ptr)-1]== '\n') ptr[strlen(ptr)-1]=0;
    p2 = index (ptr, '=');
    if (p2 == NULL) {
      fprintf (stderr, "Error in line %d: %s\n", count, buff);
      exit (1);
    }
    *p2 = 0;
    p2++;
    add_one_file (ptr, p2);
  }
  if (f != stdin) fclose (f);
}

#endif

int FDECL2(main, int, argc, char **, argv){
  char * outfile;
  struct directory_entry de;
  unsigned int mem_start;
  struct stat statbuf;
  char * scan_tree;
  int c;
#ifdef ADD_FILES
  char *add_file_file = NULL;
#endif

  if (argc < 2)
    usage();

  /* Get the defaults from the .mkisofsrc file */
  read_rcfile(argv[0]);

  outfile = NULL;
  while ((c = getopt(argc, argv, "i:o:V:RfvaTp:P:x:dDlLNzA:")) != EOF)
    switch (c)
      {
      case 'p':
	preparer = optarg;
	if(strlen(preparer) > 128) {
		fprintf(stderr,"Preparer string too long\n");
		exit(1);
	};
	break;
      case 'P':
	publisher = optarg;
	if(strlen(publisher) > 128) {
		fprintf(stderr,"Publisher string too long\n");
		exit(1);
	};
	break;
      case 'A':
	appid = optarg;
	if(strlen(appid) > 128) {
		fprintf(stderr,"Application-id string too long\n");
		exit(1);
	};
	break;
      case 'd':
	omit_period++;
	break;
      case 'D':
	RR_relocation_depth = 32767;
	break;
      case 'l':
	full_iso9660_filenames++;
	break;
      case 'L':
        allow_leading_dots++;
        break;
      case 'N':
	omit_version_number++;
	break;
      case 'o':
	outfile = optarg;
	break;
      case 'f':
	follow_links++;
	break;
      case 'R':
	use_RockRidge++;
	break;
      case 'V':
	volume_id = optarg;
	break;
      case 'v':
	verbose++;
	break;
      case 'a':
	all_files++;
	break;
      case 'T':
	generate_tables++;
	break;
      case 'z':
#ifdef VMS
	fprintf(stderr,"Transparent compression not supported with VMS\n");
	exit(1);
#else
	transparent_compression++;
#endif
	break;
      case 'x':
        exclude(optarg);
	break;
      case 'i':
#ifdef ADD_FILES
	add_file_file = optarg;
	break;
#endif
      default:
	usage();
	exit(1);
      }
#ifdef __NetBSD__
    {
	int resource;
    struct rlimit rlp;
	if (getrlimit(RLIMIT_DATA,&rlp) == -1)
		perror("Warning: getrlimit");
	else {
		rlp.rlim_cur=33554432;
		if (setrlimit(RLIMIT_DATA,&rlp) == -1)
			perror("Warning: setrlimit");
		}
	}
#endif
  mem_start = (unsigned int) sbrk(0);

  if(verbose) fprintf(stderr,"%s\n", version_string);
  /* Now find the timezone offset */

  timezone_offset = get_iso9660_timezone_offset();

  /*  The first step is to scan the directory tree, and take some notes */

  scan_tree = argv[optind];

#ifdef ADD_FILES
  if (add_file_file) {
    add_file(add_file_file);
  }
  add_file_list (argc, argv, optind+1);
#endif

  if(!scan_tree){
	  usage();
	  exit(1);
  };

#ifndef VMS
  if(scan_tree[strlen(scan_tree)-1] != '/') {
    scan_tree = (char *) e_malloc(strlen(argv[optind])+2);
    strcpy(scan_tree, argv[optind]);
    strcat(scan_tree, "/");
  };
#endif

  if(use_RockRidge){
#if 1
	extension_record = generate_rr_extension_record("RRIP_1991A",
				       "THE ROCK RIDGE INTERCHANGE PROTOCOL PROVIDES SUPPORT FOR POSIX FILE SYSTEM SEMANTICS",
				       "PLEASE CONTACT DISC PUBLISHER FOR SPECIFICATION SOURCE.  SEE PUBLISHER IDENTIFIER IN PRIMARY VOLUME DESCRIPTOR FOR CONTACT INFORMATION.", &extension_record_size);
#else
	extension_record = generate_rr_extension_record("IEEE_P1282",
				       "THE IEEE P1282 PROTOCOL PROVIDES SUPPORT FOR POSIX FILE SYSTEM SEMANTICS",
				       "PLEASE CONTACT THE IEEE STANDARDS DEPARTMENT, PISCATAWAY, NJ, USA FOR THE P1282 SPECIFICATION.", &extension_record_size);
#endif
  };

  stat(argv[optind], &statbuf);
  add_directory_hash(statbuf.st_dev, STAT_INODE(statbuf));

  de.filedir = root;  /* We need this to bootstrap */
  scan_directory_tree(argv[optind], &de);
  root->self = root->contents;  /* Fix this up so that the path tables get done right */

  if(reloc_dir) sort_n_finish(reloc_dir);

  if (goof) exit(1);

  if (outfile){
	  discimage = fopen(outfile, "w");
	  if (!discimage){
		  fprintf(stderr,"Unable to open disc image file\n");
		  exit(1);

	  };
  } else
	  discimage =  stdout;

  /* Now assign addresses on the disc for the path table. */

  path_blocks = (path_table_size + (SECTOR_SIZE - 1)) >> 11;
  if (path_blocks & 1) path_blocks++;
  path_table[0] = 0x14;
  path_table[1] = path_table[0] + path_blocks;
  path_table[2] = path_table[1] + path_blocks;
  path_table[3] = path_table[2] + path_blocks;

  last_extent = path_table[3] + path_blocks;  /* The next free block */

  /* The next step is to go through the directory tree and assign extent
     numbers for all of the directories */

  assign_directory_addresses(root);

  if(extension_record) {
	  struct directory_entry * s_entry;
	  extension_record_extent = last_extent++;
	  s_entry = root->contents;
	  set_733(s_entry->rr_attributes + s_entry->rr_attr_size - 24,
		  extension_record_extent);
	  set_733(s_entry->rr_attributes + s_entry->rr_attr_size - 8,
		  extension_record_size);
  };

  if (use_RockRidge && reloc_dir)
	  finish_cl_pl_entries();

  /* Now we generate the path tables that are used by DOS to improve directory
     access times. */
  generate_path_tables();

  /* Generate root record for volume descriptor. */
  generate_root_record();

  dump_tree(root);

  iso_write(discimage);

  fprintf(stderr,"Max brk space used %x\n",
	  ((unsigned int)sbrk(0)) - mem_start);
  fprintf(stderr,"%d extents written (%d Mb)\n", last_extent, last_extent >> 9);
#ifdef VMS
  return 1;
#else
  return 0;
#endif
}

void *e_malloc(size_t size)
{
void* pt;
	if((pt=malloc(size))==NULL) {
		printf("Not enougth memory\n");
		exit (1);
		}
return pt;
}

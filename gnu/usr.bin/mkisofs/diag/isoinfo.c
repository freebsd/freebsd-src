/*
 * File isodump.c - dump iso9660 directory information.
 *

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

/*
 * Simple program to dump contents of iso9660 image in more usable format.
 *
 * Usage:
 * To list contents of image (with or without RR):
 *	isoinfo -l [-R] -i imagefile
 * To extract file from image:
 *	isoinfo -i imagefile -x xtractfile > outfile
 * To generate a "find" like list of files:
 *	isoinfo -f -i imagefile
 */

#include "../config.h"

#include <stdio.h>
#include <signal.h>
#include <sys/stat.h>

#ifdef __svr4__
#include <stdlib.h>
#else
extern int optind;
extern char *optarg;
/* extern int getopt (int __argc, char **__argv, char *__optstring); */
#endif

FILE * infile;
int use_rock = 0;
int do_listing = 0;
int do_find = 0;
char * xtract = 0;

struct stat fstat_buf;
char name_buf[256];
char xname[256];
unsigned char date_buf[9];

unsigned char buffer[2048];

#define PAGE sizeof(buffer)

#define ISODCL(from, to) (to - from + 1)


int
isonum_731 (char * p)
{
	return ((p[0] & 0xff)
		| ((p[1] & 0xff) << 8)
		| ((p[2] & 0xff) << 16)
		| ((p[3] & 0xff) << 24));
}


int
isonum_733 (unsigned char * p)
{
	return (isonum_731 (p));
}

struct iso_primary_descriptor {
	unsigned char type			[ISODCL (  1,   1)]; /* 711 */
	unsigned char id				[ISODCL (  2,   6)];
	unsigned char version			[ISODCL (  7,   7)]; /* 711 */
	unsigned char unused1			[ISODCL (  8,   8)];
	unsigned char system_id			[ISODCL (  9,  40)]; /* aunsigned chars */
	unsigned char volume_id			[ISODCL ( 41,  72)]; /* dunsigned chars */
	unsigned char unused2			[ISODCL ( 73,  80)];
	unsigned char volume_space_size		[ISODCL ( 81,  88)]; /* 733 */
	unsigned char unused3			[ISODCL ( 89, 120)];
	unsigned char volume_set_size		[ISODCL (121, 124)]; /* 723 */
	unsigned char volume_sequence_number	[ISODCL (125, 128)]; /* 723 */
	unsigned char logical_block_size		[ISODCL (129, 132)]; /* 723 */
	unsigned char path_table_size		[ISODCL (133, 140)]; /* 733 */
	unsigned char type_l_path_table		[ISODCL (141, 144)]; /* 731 */
	unsigned char opt_type_l_path_table	[ISODCL (145, 148)]; /* 731 */
	unsigned char type_m_path_table		[ISODCL (149, 152)]; /* 732 */
	unsigned char opt_type_m_path_table	[ISODCL (153, 156)]; /* 732 */
	unsigned char root_directory_record	[ISODCL (157, 190)]; /* 9.1 */
	unsigned char volume_set_id		[ISODCL (191, 318)]; /* dunsigned chars */
	unsigned char publisher_id		[ISODCL (319, 446)]; /* achars */
	unsigned char preparer_id		[ISODCL (447, 574)]; /* achars */
	unsigned char application_id		[ISODCL (575, 702)]; /* achars */
	unsigned char copyright_file_id		[ISODCL (703, 739)]; /* 7.5 dchars */
	unsigned char abstract_file_id		[ISODCL (740, 776)]; /* 7.5 dchars */
	unsigned char bibliographic_file_id	[ISODCL (777, 813)]; /* 7.5 dchars */
	unsigned char creation_date		[ISODCL (814, 830)]; /* 8.4.26.1 */
	unsigned char modification_date		[ISODCL (831, 847)]; /* 8.4.26.1 */
	unsigned char expiration_date		[ISODCL (848, 864)]; /* 8.4.26.1 */
	unsigned char effective_date		[ISODCL (865, 881)]; /* 8.4.26.1 */
	unsigned char file_structure_version	[ISODCL (882, 882)]; /* 711 */
	unsigned char unused4			[ISODCL (883, 883)];
	unsigned char application_data		[ISODCL (884, 1395)];
	unsigned char unused5			[ISODCL (1396, 2048)];
};

struct iso_directory_record {
	unsigned char length			[ISODCL (1, 1)]; /* 711 */
	unsigned char ext_attr_length		[ISODCL (2, 2)]; /* 711 */
	unsigned char extent			[ISODCL (3, 10)]; /* 733 */
	unsigned char size			[ISODCL (11, 18)]; /* 733 */
	unsigned char date			[ISODCL (19, 25)]; /* 7 by 711 */
	unsigned char flags			[ISODCL (26, 26)];
	unsigned char file_unit_size		[ISODCL (27, 27)]; /* 711 */
	unsigned char interleave			[ISODCL (28, 28)]; /* 711 */
	unsigned char volume_sequence_number	[ISODCL (29, 32)]; /* 723 */
	unsigned char name_len		[ISODCL (33, 33)]; /* 711 */
	unsigned char name			[1];
};


int parse_rr(unsigned char * pnt, int len, int cont_flag)
{
	int slen;
	int ncount;
	int extent;
	int cont_extent, cont_offset, cont_size;
	int flag1, flag2;
	unsigned char *pnts;
	char symlink[1024];
	int goof;

	symlink[0] = 0;

	cont_extent = cont_offset = cont_size = 0;

	ncount = 0;
	flag1 = flag2 = 0;
	while(len >= 4){
		if(pnt[3] != 1) {
		  printf("**BAD RRVERSION");
		  return;
		};
		ncount++;
		if(pnt[0] == 'R' && pnt[1] == 'R') flag1 = pnt[4] & 0xff;
		if(strncmp(pnt, "PX", 2) == 0) flag2 |= 1;
		if(strncmp(pnt, "PN", 2) == 0) flag2 |= 2;
		if(strncmp(pnt, "SL", 2) == 0) flag2 |= 4;
		if(strncmp(pnt, "NM", 2) == 0) flag2 |= 8;
		if(strncmp(pnt, "CL", 2) == 0) flag2 |= 16;
		if(strncmp(pnt, "PL", 2) == 0) flag2 |= 32;
		if(strncmp(pnt, "RE", 2) == 0) flag2 |= 64;
		if(strncmp(pnt, "TF", 2) == 0) flag2 |= 128;

		if(strncmp(pnt, "PX", 2) == 0) {
			fstat_buf.st_mode = isonum_733(pnt+4);
			fstat_buf.st_nlink = isonum_733(pnt+12);
			fstat_buf.st_uid = isonum_733(pnt+20);
			fstat_buf.st_gid = isonum_733(pnt+28);
		};

		if(strncmp(pnt, "NM", 2) == 0) {
		  strncpy(name_buf, pnt+5, pnt[2] - 5);
		  name_buf[pnt[2] - 5] = 0;
		}

		if(strncmp(pnt, "CE", 2) == 0) {
			cont_extent = isonum_733(pnt+4);
			cont_offset = isonum_733(pnt+12);
			cont_size = isonum_733(pnt+20);
		};

		if(strncmp(pnt, "PL", 2) == 0 || strncmp(pnt, "CL", 2) == 0) {
			extent = isonum_733(pnt+4);
		};

		if(strncmp(pnt, "SL", 2) == 0) {
		        int cflag;

			cflag = pnt[4];
			pnts = pnt+5;
			slen = pnt[2] - 5;
			while(slen >= 1){
				switch(pnts[0] & 0xfe){
				case 0:
					strncat(symlink, pnts+2, pnts[1]);
					break;
				case 2:
					strcat (symlink, ".");
					break;
				case 4:
					strcat (symlink, "..");
					break;
				case 8:
					if((pnts[0] & 1) == 0)strcat (symlink, "/");
					break;
				case 16:
					strcat(symlink,"/mnt");
					printf("Warning - mount point requested");
					break;
				case 32:
					strcat(symlink,"kafka");
					printf("Warning - host_name requested");
					break;
				default:
					printf("Reserved bit setting in symlink", goof++);
					break;
				};
				if((pnts[0] & 0xfe) && pnts[1] != 0) {
					printf("Incorrect length in symlink component");
				};
				if((pnts[0] & 1) == 0) strcat(symlink,"/");

				slen -= (pnts[1] + 2);
				pnts += (pnts[1] + 2);
				if(xname[0] == 0) strcpy(xname, "-> ");
				strcat(xname, symlink);
		       };
			symlink[0] = 0;
		};

		len -= pnt[2];
		pnt += pnt[2];
		if(len <= 3 && cont_extent) {
		  unsigned char sector[2048];
		  lseek(fileno(infile), cont_extent << 11, 0);
		  read(fileno(infile), sector, sizeof(sector));
		  flag2 |= parse_rr(&sector[cont_offset], cont_size, 1);
		};
	};
	return flag2;
}

int 
dump_rr(struct iso_directory_record * idr)
{
	int len;
	unsigned char * pnt;

	len = idr->length[0] & 0xff;
	len -= sizeof(struct iso_directory_record);
	len += sizeof(idr->name);
	len -= idr->name_len[0];
	pnt = (unsigned char *) idr;
	pnt += sizeof(struct iso_directory_record);
	pnt -= sizeof(idr->name);
	pnt += idr->name_len[0];
	if((idr->name_len[0] & 1) == 0){
		pnt++;
		len--;
	};
	parse_rr(pnt, len, 0);
}

struct todo
{
  struct todo * next;
  char * name;
  int extent;
  int length;
};

struct todo * todo_idr = NULL;

char * months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
		       "Aug", "Sep", "Oct", "Nov", "Dec"};

dump_stat()
{
  int i;
  char outline[80];

  memset(outline, ' ', sizeof(outline));

  if(S_ISREG(fstat_buf.st_mode))
    outline[0] = '-';
  else   if(S_ISDIR(fstat_buf.st_mode))
    outline[0] = 'd';
  else   if(S_ISLNK(fstat_buf.st_mode))
    outline[0] = 'l';
  else   if(S_ISCHR(fstat_buf.st_mode))
    outline[0] = 'c';
  else   if(S_ISBLK(fstat_buf.st_mode))
    outline[0] = 'b';
  else   if(S_ISFIFO(fstat_buf.st_mode))
    outline[0] = 'f';
  else   if(S_ISSOCK(fstat_buf.st_mode))
    outline[0] = 's';
  else
    outline[0] = '?';

  memset(outline+1, '-', 9);
  if( fstat_buf.st_mode & S_IRUSR )
    outline[1] = 'r';
  if( fstat_buf.st_mode & S_IWUSR )
    outline[2] = 'w';
  if( fstat_buf.st_mode & S_IXUSR )
    outline[3] = 'x';

  if( fstat_buf.st_mode & S_IRGRP )
    outline[4] = 'r';
  if( fstat_buf.st_mode & S_IWGRP )
    outline[5] = 'w';
  if( fstat_buf.st_mode & S_IXGRP )
    outline[6] = 'x';

  if( fstat_buf.st_mode & S_IROTH )
    outline[7] = 'r';
  if( fstat_buf.st_mode & S_IWOTH )
    outline[8] = 'w';
  if( fstat_buf.st_mode & S_IXOTH )
    outline[9] = 'x';

  sprintf(outline+11, "%3d", fstat_buf.st_nlink);
  sprintf(outline+15, "%4o", fstat_buf.st_uid);
  sprintf(outline+20, "%4o", fstat_buf.st_gid);
  sprintf(outline+33, "%8d", fstat_buf.st_size);

  memcpy(outline+42, months[date_buf[1]-1], 3);
  sprintf(outline+46, "%2d", date_buf[2]);
  sprintf(outline+49, "%4d", date_buf[0]+1900);

  for(i=0; i<54; i++)
    if(outline[i] == 0) outline[i] = ' ';
  outline[54] = 0;

  printf("%s %s %s\n", outline, name_buf, xname);
}

extract_file(struct iso_directory_record * idr)
{
  int extent, len, tlen;
  unsigned char buff[2048];

  extent = isonum_733(idr->extent);
  len = isonum_733(idr->size);

  while(len > 0)
    {
      lseek(fileno(infile), extent << 11, 0);
      tlen = (len > sizeof(buff) ? sizeof(buff) : len);
      read(fileno(infile), buff, tlen);
      len -= tlen;
      extent++;
      write(1, buff, tlen);
    }
}

parse_dir(char * rootname, int extent, int len){
  unsigned int k;
  char testname[256];
  struct todo * td;
  int i, j;
  struct iso_directory_record * idr;


  if( do_listing)
    printf("\nDirectory listing of %s\n", rootname);

  while(len > 0 )
    {
      lseek(fileno(infile), extent << 11, 0);
      read(fileno(infile), buffer, sizeof(buffer));
      len -= sizeof(buffer);
      extent++;
      i = 0;
      while(1==1){
	idr = (struct iso_directory_record *) &buffer[i];
	if(idr->length[0] == 0) break;
	memset(&fstat_buf, 0, sizeof(fstat_buf));
	name_buf[0] = xname[0] = 0;
	fstat_buf.st_size = isonum_733(idr->size);
	if( idr->flags[0] & 2)
	  fstat_buf.st_mode |= S_IFDIR;
	else
	  fstat_buf.st_mode |= S_IFREG;	
	if(idr->name_len[0] == 1 && idr->name[0] == 0)
	  strcpy(name_buf, ".");
	else if(idr->name_len[0] == 1 && idr->name[0] == 1)
	  strcpy(name_buf, "..");
	else {
	  strncpy(name_buf, idr->name, idr->name_len[0]);
	  name_buf[idr->name_len[0]] = 0;
	};
	memcpy(date_buf, idr->date, 9);
	if(use_rock) dump_rr(idr);
	if(   (idr->flags[0] & 2) != 0
	   && (idr->name_len[0] != 1
	       || (idr->name[0] != 0 && idr->name[0] != 1)))
	  {
	    /*
	     * Add this directory to the todo list.
	     */
	    td = todo_idr;
	    if( td != NULL ) 
	      {
		while(td->next != NULL) td = td->next;
		td->next = (struct todo *) malloc(sizeof(*td));
		td = td->next;
	      }
	    else
	      {
		todo_idr = td = (struct todo *) malloc(sizeof(*td));
	      }
	    td->next = NULL;
	    td->extent = isonum_733(idr->extent);
	    td->length = isonum_733(idr->size);
	    td->name = (char *) malloc(strlen(rootname) 
				       + strlen(name_buf) + 2);
	    strcpy(td->name, rootname);
	    strcat(td->name, name_buf);
	    strcat(td->name, "/");
	  }
	else
	  {
	    strcpy(testname, rootname);
	    strcat(testname, name_buf);
	    if(xtract && strcmp(xtract, testname) == 0)
	      {
		extract_file(idr);
	      }
	  }
	if(   do_find
	   && (idr->name_len[0] != 1
	       || (idr->name[0] != 0 && idr->name[0] != 1)))
	  {
	    strcpy(testname, rootname);
	    strcat(testname, name_buf);
	    printf("%s\n", testname);
	  }
	if(do_listing)
	  dump_stat();
	i += buffer[i];
	if (i > 2048 - sizeof(struct iso_directory_record)) break;
      }
    }
}

usage()
{
  fprintf(stderr, "isoinfo -i filename [-l] [-R] [-x filename] [-f]\n");
}

main(int argc, char * argv[]){
  char c;
  char buffer[2048];
  int nbyte;
  char * filename = NULL;
  int i,j;
  struct todo * td;
  struct iso_primary_descriptor ipd;
  struct iso_directory_record * idr;

  if(argc < 2) return 0;
  while ((c = getopt(argc, argv, "i:Rlx:f")) != EOF)
    switch (c)
      {
      case 'f':
	do_find++;
	break;
      case 'R':
	use_rock++;
	break;
      case 'l':
	do_listing++;
	break;
      case 'i':
	filename = optarg;
	break;
      case 'x':
	xtract = optarg;
	break;
      default:
	usage();
	exit(1);
      }
  
  if( filename == NULL )
  {
	fprintf(stderr, "Error - file not specified\n");
  	exit(1);
  }

  infile = fopen(filename,"rb");

  if( infile == NULL )
  {
	fprintf(stderr,"Unable to open file %s\n", filename);
	exit(1);
  }

  lseek(fileno(infile), 16<<11, 0);
  read(fileno(infile), &ipd, sizeof(ipd));

  idr = (struct iso_directory_record *) &ipd.root_directory_record;

  parse_dir("/", isonum_733(idr->extent), isonum_733(idr->size));
  td = todo_idr;
  while(td)
    {
      parse_dir(td->name, td->extent, td->length);
      td = td->next;
    }

  fclose(infile);
}
  




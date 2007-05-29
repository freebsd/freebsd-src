/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "test.h"
__FBSDID("$FreeBSD$");

/*
Execute the following to rebuild the data for this program:
   tail -n +5 test-read_format-isorr_bz2.c | /bin/sh

rm -rf /tmp/iso
mkdir /tmp/iso
mkdir /tmp/iso/dir
echo "hello" >/tmp/iso/file
ln /tmp/iso/file /tmp/iso/hardlink
(cd /tmp/iso; ln -s file symlink)
TZ=utc touch -afhm -t 197001010000.01 /tmp/iso /tmp/iso/file /tmp/iso/dir
TZ=utc touch -afhm -t 196912312359.58 /tmp/iso/symlink
mkhybrid -R -uid 1 -gid 2 /tmp/iso | bzip2 > data.iso.bz2
cat data.iso.bz2 | ./maketest.pl > data.c
exit 1
 */

static unsigned char archive[] = {
'B','Z','h','9','1','A','Y','&','S','Y','G',11,4,'c',0,0,199,255,221,255,
255,203,252,221,'c',251,248,'?',255,223,224,167,255,222,'&','!',234,'$',0,
'0',1,' ',0,'D',2,129,8,192,3,14,'2','3','$',19,184,'J',' ','F',168,244,201,
149,'6','Q',226,155,'S',212,209,160,'h','4','i',160,26,13,0,244,134,212,0,
218,'O',212,153,1,144,244,128,148,' ',147,13,' ',213,'=','1','\'',169,166,
128,'=','!',233,0,208,0,26,0,0,30,160,'h',0,'4','z',130,180,163,'@',0,0,4,
211,0,0,0,2,'b','`',0,0,0,0,0,8,146,133,'F',154,'y','A',163,'A',161,163,'@',
'z',134,'C','C','F',131,'F','@',0,0,0,0,6,154,26,'Q',24,234,180,'P',172,251,
'=',2,'P','H','&','Y','o',130,28,'"',229,210,247,227,248,200,'?','6',161,
'?',170,'H',172,'"','H','I',16,'2','"','&',148,'G',133,'T','z',224,1,215,
' ',0,191,184,10,160,24,248,180,183,244,156,'K',202,133,208,'U',5,'6','C',
26,144,'H',168,'H','H','(','"',151,'@','m',223,'(','P',169,'e',145,148,'6',
237,235,7,227,204,']','k','{',241,187,227,244,251,':','a','L',138,'#','R',
'"',221,'_',239,')',140,'*','*',172,'Q',16,1,16,207,166,251,233,'Z',169,'4',
'_',195,'a',14,18,231,'}',14,139,137,'e',213,185,'T',194,'D','`',25,'$',187,
208,'%','c',162,'~',181,'@',204,'2',238,'P',161,213,127,'I',169,3,' ','o',
6,161,16,128,'F',214,'S','m',6,244,11,229,'Z','y','.',176,'q',' ',248,167,
204,26,193,'q',211,241,214,133,221,212,'I','`',28,244,'N','N','f','H','9',
'w',245,209,'*',20,26,208,'h','(',194,156,192,'l',';',192,'X','T',151,177,
209,'0',156,16,'=',20,'k',184,144,'z',26,'j',133,194,'9',227,'<','[','^',
17,'w','p',225,220,248,'>',205,'>','[',19,'5',155,17,175,28,28,168,175,'n',
'\'','c','w',27,222,204,'k','n','x','I',23,237,'c',145,11,184,'A','(',1,169,
'0',180,189,134,'\\','Y','x',187,'C',151,'d','k','y','-','L',218,138,'s',
'*','(',12,'h',242,'*',17,'E','L',202,146,138,'l','0',217,160,'9','.','S',
214,198,143,'3','&',237,'=','t','P',168,214,210,'`','p','J',181,'H',138,149,
'1','B',206,22,164,'[','O','A',172,134,224,179,219,166,184,'X',185,'W',154,
219,19,161,'Y',184,220,237,147,'9',191,237,'&','i','_',226,146,205,160,'@',
'b',182,';',3,'!',183,'J','t',161,160,178,173,'S',235,':','2',159,':',245,
'{','U',174,'P',142,'G','(',')',9,168,185,'A','U',231,193,'g',213,'e',12,
'X',223,22,249,')',152,237,'G',150,156,3,201,245,212,'2',218,209,177,196,
235,'_','~',137,24,31,196,232,'B',172,'w',159,24,'n',156,150,225,'1','y',
22,'#',138,193,227,232,169,170,166,179,1,11,182,'i',')',160,180,198,175,128,
249,167,5,194,142,183,'f',134,206,180,'&','E','!','[',31,195,':',192,'s',
232,187,'N',131,'Y',137,243,15,'y',12,'J',163,'-',242,'5',197,151,130,163,
240,220,'T',161,'L',159,141,159,152,'4',18,128,'.','^',250,168,200,163,'P',
231,'Y','w','F','U',186,'x',190,16,'0',228,22,'9','F','t',168,157,'i',190,
'+',246,141,142,18,' ','M',174,197,'O',165,'m',224,27,'b',150,'|','W','H',
196,'.','*','Q','$',225,'I','-',148,169,'F',7,197,'m','-',130,153,0,158,21,
'(',221,221,226,206,'g',13,159,163,'y',176,'~',158,'k','4','q','d','s',177,
'7',14,217,'1',173,206,228,'t',250,200,170,162,'d','2','Z','$','e',168,224,
223,129,174,229,165,187,252,203,'-',28,'`',207,183,'-','/',127,196,230,131,
'B',30,237,' ',8,26,194,'O',132,'L','K','\\',144,'L','c',1,10,176,192,'c',
0,244,2,168,3,0,'+',233,186,16,17,'P',17,129,252,'2',0,2,154,247,255,166,
'.',228,138,'p',161,' ',142,22,8,198};

DEFINE_TEST(test_read_format_isorr_bz2)
{
	struct archive_entry *ae;
	struct archive *a;
	const void *p;
	size_t size;
	off_t offset;
	assert((a = archive_read_new()) != NULL);
	assert(0 == archive_read_support_compression_all(a));
	assert(0 == archive_read_support_format_all(a));
	assert(0 == archive_read_open_memory(a, archive, sizeof(archive)));

	/* First entry is '.' root directory. */
	assert(0 == archive_read_next_header(a, &ae));
	assertEqualString(".", archive_entry_pathname(ae));
	assert(S_ISDIR(archive_entry_stat(ae)->st_mode));
	assertEqualInt(2048, archive_entry_size(ae));
	assertEqualInt(1, archive_entry_mtime(ae));
	assertEqualInt(0, archive_entry_mtime_nsec(ae));
	assertEqualInt(1, archive_entry_ctime(ae));
	assertEqualInt(0, archive_entry_stat(ae)->st_nlink);
	assertEqualInt(0, archive_entry_uid(ae));

	/* A directory. */
	assert(0 == archive_read_next_header(a, &ae));
	assertEqualString("dir", archive_entry_pathname(ae));
	assert(S_ISDIR(archive_entry_stat(ae)->st_mode));
	assert(2048 == archive_entry_size(ae));
	assert(1 == archive_entry_mtime(ae));
	assert(1 == archive_entry_atime(ae));
	assert(2 == archive_entry_stat(ae)->st_nlink);
	assert(1 == archive_entry_uid(ae));
	assert(2 == archive_entry_gid(ae));

	/* A regular file. */
	assert(0 == archive_read_next_header(a, &ae));
	assertEqualString("file", archive_entry_pathname(ae));
	assert(S_ISREG(archive_entry_stat(ae)->st_mode));
	assert(6 == archive_entry_size(ae));
	assert(0 == archive_read_data_block(a, &p, &size, &offset));
	assert(6 == size);
	assert(0 == offset);
	assert(0 == memcmp(p, "hello\n", 6));
	assert(1 == archive_entry_mtime(ae));
	assert(1 == archive_entry_atime(ae));
	assert(2 == archive_entry_stat(ae)->st_nlink);
	assert(1 == archive_entry_uid(ae));
	assert(2 == archive_entry_gid(ae));

	/* A hardlink to the regular file. */
	assert(0 == archive_read_next_header(a, &ae));
	assertEqualString("hardlink", archive_entry_pathname(ae));
	assert(S_ISREG(archive_entry_stat(ae)->st_mode));
	assertEqualString("file", archive_entry_hardlink(ae));
	assert(6 == archive_entry_size(ae));
	assert(1 == archive_entry_mtime(ae));
	assert(1 == archive_entry_atime(ae));
	assert(2 == archive_entry_stat(ae)->st_nlink);
	assert(1 == archive_entry_uid(ae));
	assert(2 == archive_entry_gid(ae));

	/* A symlink to the regular file. */
	assert(0 == archive_read_next_header(a, &ae));
	assertEqualString("symlink", archive_entry_pathname(ae));
	assert(S_ISLNK(archive_entry_stat(ae)->st_mode));
	assertEqualString("file", archive_entry_symlink(ae));
	assert(0 == archive_entry_size(ae));
	assert(-2 == archive_entry_mtime(ae));
	assert(-2 == archive_entry_atime(ae));
	assert(1 == archive_entry_stat(ae)->st_nlink);
	assert(1 == archive_entry_uid(ae));
	assert(2 == archive_entry_gid(ae));

	/* End of archive. */
	assert(ARCHIVE_EOF == archive_read_next_header(a, &ae));

	/* Verify archive format. */
	assert(archive_compression(a) == ARCHIVE_COMPRESSION_BZIP2);
	assert(archive_format(a) == ARCHIVE_FORMAT_ISO9660_ROCKRIDGE);

	/* Close the archive. */
	assert(0 == archive_read_close(a));
#if ARCHIVE_API_VERSION > 1
	assert(0 == archive_read_finish(a));
#else
	archive_read_finish(a);
#endif
}



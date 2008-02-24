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
__FBSDID("$FreeBSD: src/lib/libarchive/test/test_read_format_iso_gz.c,v 1.1 2007/03/03 07:37:37 kientzle Exp $");

static unsigned char archive[] = {
31,139,8,8,201,'R','p','C',0,3,'t','e','s','t','-','r','e','a','d','_','f',
'o','r','m','a','t','.','i','s','o',0,237,219,223,'k',211,'@',28,0,240,212,
23,'K','}',20,169,143,135,15,162,224,218,180,']','W',186,183,173,'I',183,
206,254,144,'d',19,246,'$',5,';',24,'2',11,235,240,239,221,127,162,233,'f',
17,29,219,24,12,'\'',243,243,'!',185,187,220,']',146,';',8,9,223,131,'D',
17,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,'P',234,
'%','q',220,'(','E',253,211,217,'l',';','O',194,'u','z','I','6',25,']',219,
26,194,234,'z',241,'o',217,13,247,'-',182,229,30,149,203,'Q',229,178,170,
242,252,'W',243,139,'e',242,'*',170,'^',30,'U',163,242,'2','+','G',199,207,
158,'V','_',190,';',127,178,':',255,134,1,241,23,140,222,15,242,'I','?',15,
'E',26,186,27,27,'q','}',183,'8',232,15,134,'i','~',152,239,167,163,176,'}',
'0',24,'&','i',22,'^','/',159,159,180,'7',201,146,162,176,150,213,147,143,
'E','!','K',183,246,'\'','Y','x',211,'{',27,26,221,'n','+',164,181,195,201,
193,'x','\'',217,26,166,171,202,'N',216,171,'}','H',183,178,'|','2',174,239,
213,242,222,238,'`','8',28,140,'w',30,'j',186,205,'8','n','7',26,'q',167,
217,'j',174,183,';','q','|','~',165,'"',254,'C','t',165,'G',')','z',168,209,
243,'o',184,143,215,'6',220,139,239,'?',191,255,0,192,255,163,136,224,194,
'h',254,'5',140,231,223,'B',232,132,'f','k',179,185,190,217,238,'\\',132,
':',149,147,'/',199,139,249,209,'"','4','k','q',173,21,214,230,225,'l',182,
'8','[',';',157,'M','?',127,':',154,159,158,'L',207,'j','E','{',152,'>',244,
28,0,128,187,')',']',172,177,139,255,1,0,0,224,'1',187,136,252,171,22,0,0,
0,0,224,'1',187,253,31,187,'[','{','X',';',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,224,206,
'~',0,137,'#',195,182,0,128,1,0};

DEFINE_TEST(test_read_format_iso_gz)
{
	struct archive_entry *ae;
	struct archive *a;
	assert((a = archive_read_new()) != NULL);
	assert(0 == archive_read_support_compression_all(a));
	assert(0 == archive_read_support_format_all(a));
	assert(0 == archive_read_open_memory(a, archive, sizeof(archive)));
	assert(0 == archive_read_next_header(a, &ae));
	assert(archive_compression(a) == ARCHIVE_COMPRESSION_GZIP);
	assert(archive_format(a) == ARCHIVE_FORMAT_ISO9660);
	assert(0 == archive_read_close(a));
#if ARCHIVE_API_VERSION > 1
	assert(0 == archive_read_finish(a));
#else
	archive_read_finish(a);
#endif
}



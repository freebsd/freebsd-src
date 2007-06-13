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
 * Each of the following is an archive of a single sparse file
 * named 'sparse' which has:
 *   * a length of 3M exactly
 *   * a single 'a' byte at offset 100000
 *   * a single 'a' byte at offset 200000
 */

/* Old GNU tar sparse format. */
static unsigned char archive_old[] = {
31,139,8,0,150,221,'l','F',0,3,'+','.','H',',','*','N','e',160,')','0',0,
2,'3',19,19,16,'m','h','n','j',128,'L',131,128,17,16,'3',24,26,152,25,27,
27,154,154,25,25,26,'2',0,'e',205,141,140,25,20,130,'i',235,',',8,'(','-',
'.','I',',','R','P','`','(',201,204,197,171,142,144,'<',217,0,20,4,198,'f',
'&',134,160,'P',128,134,8,156,13,12,'%','#',',',226,134,'&',16,'>','L',28,
204,198,'g',1,'\\',3,213,'A','"',245,141,28,5,'#',8,140,166,159,'Q','0',10,
'F',193,'(',24,24,0,0,'}','}',226,185,0,10,0,0};

/* GNU tar "0.0" posix format, as written by GNU tar 1.15.1. */
static unsigned char archive_0_0[] = {
31,139,8,0,171,221,'l','F',0,3,237,147,193,'N',195,'0',12,134,'s',206,'S',
244,9,186,216,142,147,246,144,'3',156,'&','.','<','@',24,153,'4',193,24,'j',
':','i',227,233,'I',129,'M','E','*','L','h',21,'U',165,'|',23,'K','v','~',
203,145,253,151,139,';',127,184,13,254,'1','4',177,180,'h',140,'^',196,'W',
223,196,' ',198,'C','%',140,214,']',4,203,170,31,'?',208,10,5,'(','C',4,'l',
'4',161,'P','@',218,178,'(',14,'#',206,240,'#',251,216,250,'&',141,'r','m',
159,175,191,156,227,'L','@','[',220,',',239,203,207,157,151,'q',243,22,28,
129,'f',139,149,'D',211,'/',189,236,183,15,207,187,213,'S','t','$',177,234,
'W','v',235,'u',12,173,171,19,'d',228,247,134,157,234,216,134,232,24,'P',
'b','=',' ',131,'$',171,',',254,'Y','w',158,146,7,'u','J',162,'*','|',187,
217,6,7,'P',1,'S',205,'5','v',185,'U','?',167,13,203,169,'7','0','-',163,
155,'}',128,11,254,199,206,'.','\'',255,'#','@',242,'?','X','R',162,248,23,
19,157,252,159,206,226,215,'w',151,234,'3',245,255,181,248,169,7,200,204,
154,'|','?',153,'L','&','3',13,239,',',195,'|','B',0,14,0,0};

/* GNU tar "0.1" posix format, as written by GNU tar 1.15.XXX. */
static unsigned char archive_0_1[] = {
31,139,8,0,191,221,'l','F',0,3,237,148,193,'N',195,'0',12,134,'s',238,'S',
244,1,170,'4','v','b',167,';',244,202,'v','B','H',136,7,8,'#',135,138,'u',
'L',205,'&','M','<','=','Y',183,161,10,9,'v',216,160,'*',202,'w',177,'e',
'G',137,19,251,143,',',31,220,'~',225,221,139,239,130,180,200,204,'e',216,
184,'.','x','q',';','T',132,141,'9','X',176,164,134,182,7,9,5,'(',214,26,
136,13,145,'P',160,'I','Y',145,239,'o','X',195,183,236,194,214,'u',177,148,
'k',247,'9',221,229,211,'N',4,180,249,252,254,'I',30,'{','.','C',243,238,
'k',13,134,',','V',25,242,'0',181,222,181,207,171,183,229,'k',168,245,215,
140,'k','}','}',244,'3',130,'a',166,'u',155,'z',22,209,'\\',16,'`',1,209,
173,',',246,254,233,144,'B','e',168,'r',183,'m',226,22,0,21,144,158,'1',244,
177,229,'0','f',152,178,177,'_',234,127,'"',203,216,175,199,190,']','w',205,
202,255,202,23,'p','A',255,'x',144,203,'Y',255,8,16,245,'O',192,'$',242,'?',
17,209,'Y',255,'q',220,'~','\\','w',')','?','Q',253,'_',139,27,187,128,196,
164,'I',243,147,'H','$',18,227,240,1,'Q',127,'c',137,0,14,0,0};

/* GNU tar "1.0" posix format, as written by GNU tar 1.16.1. */
static unsigned char archive_1_0[] = {
31,139,8,0,210,221,'l','F',0,3,237,148,207,'N',195,'0',12,198,'s',206,'S',
244,9,210,216,206,159,238,208,'+',236,132,144,16,15,16,'A',14,'E',219,'@',
201,'&','M','<','=','i','G','Q',133,'4','v','`','l',170,240,239,'b',235,'s',
20,'9','q',190,168,250,'>',236,151,'1','<',199,148,149,'G',231,154,':',191,
133,148,163,'8',31,186,224,140,233,'#','x',171,167,'q',0,193,10,208,142,8,
172,'3',30,133,6,178,218,139,'j',127,198,30,142,178,203,219,144,'J','+',191,
221,231,243,',','_','q','&',' ','V',183,'w',143,234,'0','s',181,14,'/',175,
169,5,249,'M',237,'6','E',213,18,221,'T',221,132,'u','l',15,185,'$',152,'V',
'R',12,171,220,189,199,150,192,'X',143,141,'D',']',133,'m','W',150,3,'4',
'`','i',225,200,244,218,211,'T','3',206,202,'k','_',198,'?','D',213,'e','p',
15,195,220,'n',186,'U',252,147,'/',224,132,255,169,183,203,232,127,4,'(',
254,183,224,181,168,'.','b',162,209,255,229,'-',254,184,238,'T','}',166,254,
'\'',185,'(',144,147,22,'P','B','I',27,143,'C','>','z','W',179,'+',153,227,
132,'k','7',192,204,26,'~','?',12,195,'0',12,'s','y','>',0,244,'|','e',9,
0,18,0,0};

static void
test_data(const char *buff, int buff_size,
    int start_index, int data_index, const char *data)
{
	int i;

	assert(buff_size > data_index - start_index);
	for (i = 0; i < data_index - start_index; i++)
		assert(buff[i] == 0);
	assert(0 == memcmp(buff + (data_index - start_index), data, strlen(data)));
	i += strlen(data);
	for (; i < buff_size; i++)
		assert(buff[i] == 0);
}

static void
verify_archive(void *b, size_t l)
{
	struct archive_entry *ae;
	struct archive *a;
	const void *d;
	size_t s;
	off_t o;

	assert((a = archive_read_new()) != NULL);
	assert(0 == archive_read_support_compression_all(a));
	assert(0 == archive_read_support_format_all(a));
	assert(0 == archive_read_open_memory(a, b, l));
	assertEqualIntA(a, 0, archive_read_next_header(a, &ae));

	assertEqualIntA(a, 0, archive_read_data_block(a, &d, &s, &o));
	test_data(d, s, o, 1000000, "a");
	assertEqualIntA(a, 0, archive_read_data_block(a, &d, &s, &o));
	test_data(d, s, o, 2000000, "a");
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_data_block(a, &d, &s, &o));
	failure("Size returned at EOF must be zero");
	assertEqualInt(s, 0);
	failure("Offset at EOF must be same as file size");
	assertEqualInt(o, 3145728);

	assert(0 == archive_read_close(a));
#if ARCHIVE_API_VERSION > 1
	assert(0 == archive_read_finish(a));
#else
	archive_read_finish(a);
#endif
}

DEFINE_TEST(test_read_format_gtar_sparse)
{
	verify_archive(archive_old, sizeof(archive_old));
	verify_archive(archive_0_0, sizeof(archive_0_0));
	verify_archive(archive_0_1, sizeof(archive_0_1));
	verify_archive(archive_1_0, sizeof(archive_1_0));
}



/*-
SOFTWARE LICENSE AGREEMENT

IMPORTANT! READ CAREFULLY: 
THIS SOFTWARE LICENSE AGREEMENT (the ``Agreement'') is a legal 
agreement between you (either an individual or a single entity) and 
M-Systems Flash Disk Pioneers Ltd. ("M-Systems").  
This Agreement relates to the M-Systems' software accompanying this 
Agreement, which includes computer software and may include associated
media, printed materials, and ``online'' or electronic documentation 
(the ``Licensed Software'').  
By downloading, installing, copying, or otherwise using the Licensed
Software, you agree to be bound by the terms of this Agreement.
If you do not agree to the terms of this Agreement, do not install, 
copy or use the Licensed Software.

The Licensed Software is protected by copyright laws and international
copyright treaties, as well as other intellectual property laws and 
treaties.  The Licensed Software is licensed, not sold.  
The Licensed Software is being provided solely for use with M-Systems'
DiskOnChip® product lines.
1.	License Grant. 
(a)	Grant of License. Subject to the terms and conditions of this 
        Agreement, M-Systems hereby grants you a nonexclusive, 
        royalty-free, worldwide license (including the right to 
        sublicense) to use, copy and distribute the Licensed Software 
        with M-Systems DiskOnChip® products.
(b)	Restrictions on Use.  The Licensed Software is licensed solely
        for use with and to support M-Systems' DiskOnChip® products. 
        Use of this Licensed Software with, or to support, any other 
        flash disk, flash card, resident flash array or solid state 
        disk of any kind is expressly prohibited, and constitutes an 
        illegal infringement of M-Systems' patent, copyright and other 
        rights in and to the Licensed Software. 
2.	Limitations on Reverse Engineering, Decompilation, and 
        Disassembly.  You may not reverse engineer, decompile, or 
        disassemble the Licensed Software, except and only to the 
        extent that such activity is expressly permitted by applicable 
        law notwithstanding this limitation.
3.	Termination. Without prejudice to any other rights, M-Systems 
        may terminate this Agreement if you fail to comply with the 
        terms and conditions of this Agreement. In such event, you must
        destroy all copies of the Licensed Software and all of its 
        component parts.
4.	Intellectual Property Rights.  Title to the Licensed Software, 
        and all rights with respect to the Software not specifically 
        granted under this Agreement, including without limitation 
        all rights of modification, disassembly and decompilation and 
        all copyright, patent, trademark, trade secret and other 
        proprietary rights and interests are reserved to M-Systems. 
        You may not remove or alter the "README" or "COPYRIGHT" files
	or copyright notices in the Licensed Software.
5.	DISCLAIMER OF WARRANTIES. To the maximum extent permitted by 
        applicable law, M-Systems and its suppliers provide the Product
        and any (if any) support services related to the Product 
        ("Support Services") AS IS AND WITH ALL FAULTS, and hereby 
        disclaim all warranties and conditions, either express, 
        implied or statutory, including, but not limited to, any 
        (if any) implied warranties or conditions of merchantability, 
        of fitness for a particular purpose, of lack of viruses, of 
        accuracy or completeness of responses, of results, and of lack
        of negligence or lack of workmanlike effort, all with regard 
        to the Product, and the provision of or failure to provide 
        Support Services. ALSO, THERE IS NO WARRANTY OR CONDITION OF
        TITLE, QUIET ENJOYMENT, QUIET POSSESSION, CORRESPONDENCE TO 
        DESCRIPTION OR NON-INFRINGEMENT, WITH REGARD TO THE PRODUCT. 
        THE ENTIRE RISK AS TO THE QUALITY OF OR ARISING OUT OF USE OR 
        PERFORMANCE OF THE PRODUCT AND SUPPORT SERVICES, IF ANY, 
        REMAINS WITH YOU.
6.	EXCLUSION OF INCIDENTAL, CONSEQUENTIAL AND CERTAIN OTHER 
        DAMAGES. To the maximum extent permitted by applicable law, 
        in no event shall M-Systems or its suppliers be liable for 
        any special, incidental, indirect, or consequential damages 
        whatsoever (including, but not limited to, damages for loss 
        of profits or confidential or other information, for business 
        interruption, for personal injury, for loss of privacy, for 
        failure to meet any duty including of good faith or of 
        reasonable care, for negligence, and for any other pecuniary 
        or other loss whatsoever) arising out of or in any way related
        to the use of or inability to use the Product, the provision 
        of or failure to provide Support Services, or otherwise under 
        or in connection with any provision of Agreement, even in the 
        event of the fault, tort (including negligence), strict 
        liability, breach of contract or breach of warranty of 
        M-Systems or any supplier, and even if M-Systems or any 
        supplier has been advised of the possibility of such damages. 
7.	LIMITATION OF LIABILITY AND REMEDIES. Notwithstanding any 
        damages that you might incur for any reason whatsoever 
        (including, without limitation, all damages referenced above 
        and all direct or general damages), the entire liability of 
        M-Systems and any of its suppliers under any provision of this
        Agreement and your exclusive remedy for all of the foregoing 
        shall be limited to the greater of the amount actually paid by
        you for the Product or U.S.$5.00. The foregoing limitations, 
        exclusions and disclaimers shall apply to the maximum extent 
        permitted by applicable law, even if any remedy fails its 
        essential purpose.
8.	Miscellaneous
        The laws of the State of California, United States of America, 
        exclusive of conflict-of-laws provisions, shall govern this 
        Agreement in all respects. 
*/

#ifndef MSYSOSAK_H
#define MSYSOSAK_H 1

struct doc2k_stat {
	unsigned short	type;
	long		unitSize;
	long		mediaSize;
	long		chipSize;
	int		interleaving;
	unsigned long	window;

	/* Non-OSAK members Below this point */
	char		*product;
	char		model[40];
};

enum doc2k_work { DOC2K_READ, DOC2K_WRITE, DOC2K_ERASE };

int doc2k_probe(int drive, unsigned lowaddr, unsigned highaddr);
int doc2k_info(int drive, struct doc2k_stat *info);
int doc2k_size(int drive, unsigned *capacity, unsigned *ncyl, unsigned *nhead, unsigned *nsect);
int doc2k_open(int drive);
int doc2k_close(int drive);
int doc2k_rwe(int drive, enum doc2k_work what, unsigned block, unsigned len, void *ptr);
void doc2k_memcpy(void *dst, const void *src, unsigned len);
void doc2k_memset(void *dst, int c, unsigned len);
int doc2k_memcmp(const void *dst, const void *src, unsigned len);
void *doc2k_malloc(int bytes);
void doc2k_free(void *ptr);
void doc2k_delay(unsigned usec);

#endif

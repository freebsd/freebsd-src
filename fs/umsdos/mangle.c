/*
 *  linux/fs/umsdos/mangle.c
 *
 *      Written 1993 by Jacques Gelinas 
 *
 * Control the mangling of file name to fit msdos name space.
 * Many optimisations by GLU == dglaude@is1.vub.ac.be (Glaude David)
 */

#include <linux/errno.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/umsdos_fs.h>

/* (This file is used outside of the kernel) */
#ifndef __KERNEL__
#define KERN_WARNING
#endif

/*
 * Complete the mangling of the MSDOS fake name
 * based on the position of the entry in the EMD file.
 * 
 * Simply complete the job of umsdos_parse; fill the extension.
 * 
 * Beware that info->f_pos must be set.
 */
void umsdos_manglename (struct umsdos_info *info)
{
	if (info->msdos_reject) {
		/* #Specification: file name / non MSDOS conforming / mangling
		 * Each non MSDOS conforming file has a special extension
		 * build from the entry position in the EMD file.
		 * 
		 * This number is then transform in a base 32 number, where
		 * each digit is expressed like hexadecimal number, using
		 * digit and letter, except it uses 22 letters from 'a' to 'v'.
		 * The number 32 comes from 2**5. It is faster to split a binary
		 * number using a base which is a power of two. And I was 32
		 * when I started this project. Pick your answer :-) .
		 * 
		 * If the result is '0', it is replace with '_', simply
		 * to make it odd.
		 * 
		 * This is true for the first two character of the extension.
		 * The last one is taken from a list of odd character, which
		 * are:
		 * 
		 * { } ( ) ! ` ^ & @
		 * 
		 * With this scheme, we can produce 9216 ( 9* 32 * 32)
		 * different extensions which should not clash with any useful
		 * extension already popular or meaningful. Since most directory
		 * have much less than 32 * 32 files in it, the first character
		 * of the extension of any mangled name will be {.
		 * 
		 * Here are the reason to do this (this kind of mangling).
		 * 
		 * -The mangling is deterministic. Just by the extension, we
		 * are able to locate the entry in the EMD file.
		 * 
		 * -By keeping to beginning of the file name almost unchanged,
		 * we are helping the MSDOS user.
		 * 
		 * -The mangling produces names not too ugly, so an msdos user
		 * may live with it (remember it, type it, etc...).
		 * 
		 * -The mangling produces names ugly enough so no one will
		 * ever think of using such a name in real life. This is not
		 * fool proof. I don't think there is a total solution to this.
		 */
		int entry_num;
		char *pt = info->fake.fname + info->fake.len;
		/* lookup for encoding the last character of the extension 
		 * It contains valid character after the ugly one to make sure 
		 * even if someone overflows the 32 * 32 * 9 limit, it still 
		 * does something 
		 */
#define SPECIAL_MANGLING '{','}','(',')','!','`','^','&','@'
		static char lookup3[] =
		{
			SPECIAL_MANGLING,
		/* This is the start of lookup12 */
			'_', '1', '2', '3', '4', '5', '6', '7', '8', '9',
			'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
			'p', 'q', 'r', 's', 't', 'u', 'v'
		};

#define lookup12 (lookup3+9)
		entry_num = info->f_pos / UMSDOS_REC_SIZE;
		if (entry_num > (9* 32 * 32)){
			printk (KERN_WARNING "UMSDOS: more than 9216 files in a directory.\n"
				"This may break the mangling strategy.\n"
				"Not a killer problem. See doc.\n");
		}
		*pt++ = '.';
		*pt++ = lookup3 [(entry_num >> 10) & 31];
		*pt++ = lookup12[(entry_num >> 5) & 31];
		*pt++ = lookup12[entry_num & 31];
		*pt = '\0';		/* help doing printk */	
		info->fake.len += 4;
		info->msdos_reject = 0;		/* Avoid mangling twice */
	}
}

/*
 * Evaluate the record size needed to store of name of len character.
 * The value returned is a multiple of UMSDOS_REC_SIZE.
 */
int umsdos_evalrecsize (int len)
{
	struct umsdos_dirent dirent;
	int nbrec = 1 + ((len - 1 + (dirent.name - (char *) &dirent))
			 / UMSDOS_REC_SIZE);

	return nbrec * UMSDOS_REC_SIZE;
	/*
	 * GLU        This should be inlined or something to speed it up to the max.
	 * GLU        nbrec is absolutely not needed to return the value.
	 */
}
#ifdef TEST
int umsdos_evalrecsize_old (int len)
{
	struct umsdos_dirent dirent;
	int size = len + (dirent.name - (char *) &dirent);
	int nbrec = size / UMSDOS_REC_SIZE;
	int extra = size % UMSDOS_REC_SIZE;

	if (extra > 0)
		nbrec++;
	return nbrec * UMSDOS_REC_SIZE;
}
#endif


/*
 * Fill the struct info with the full and msdos name of a file
 * Return 0 if all is OK, a negative error code otherwise.
 */
int umsdos_parse (
			 const char *fname,
			 int len,
			 struct umsdos_info *info)
{
	int ret = -ENAMETOOLONG;

	/* #Specification: file name / too long
	 * If a file name exceed UMSDOS maxima, the file name is silently
	 * truncated. This makes it conformant with the other file system
	 * of Linux (minix and ext2 at least).
	 */
	if (len > UMSDOS_MAXNAME)
		len = UMSDOS_MAXNAME;
	{
		const char *firstpt = NULL;	/* First place we saw a "." in fname */

		/* #Specification: file name / non MSDOS conforming / base length 0
		 * file names beginning with a period '.' are invalid for MS-DOS.
		 * It needs absolutely a base name. So the file name is mangled
		 */
		int ivldchar = fname[0] == '.';		/* At least one invalid character */
		int msdos_len = len;
		int base_len;

		/*
		 * cardinal_per_size tells if there exists at least one
		 * DOS pseudo device on length n.  See the test below.
		 */
		static const char cardinal_per_size[9] =
		{
			0, 0, 0, 1, 1, 0, 1, 0, 1
		};

		/*
		 * lkp translate all character to acceptable character (for DOS).
		 * When lkp[n] == n, it means also it is an acceptable one.
		 * So it serves both as a flag and as a translator.
		 */
		static char lkp[256];
		static char is_init = 0;

		if (!is_init) {
			/*
			 * Initialisation of the array is easier and less error
                         * prone like this.
			 */
			int i;
			static const char *spc = "\"*+,/:;<=>?[\\]|~";

			is_init = 1;
			for (i = 0; i <= 32; i++)
				lkp[i] = '#';
			for (i = 33; i < 'A'; i++)
				lkp[i] = (char) i;
			for (i = 'A'; i <= 'Z'; i++)
				lkp[i] = (char) (i + ('a' - 'A'));
			for (i = 'Z' + 1; i < 127; i++)
				lkp[i] = (char) i;
			for (i = 128; i < 256; i++)
				lkp[i] = '#';

			lkp['.'] = '_';
			while (*spc != '\0')
				lkp[(unsigned char) (*spc++)] = '#';
		}
		/*  GLU
		 * File names longer than 8+'.'+3 are invalid for MS-DOS,
		 * so the file name is to be mangled--no further test is needed.
		 * This speeds up handling of long names.
		 * The position of the last point is no more necessary anyway.
		 */
		if (len <= (8 + 1 + 3)) {
			const char *pt = fname;
			const char *endpt = fname + len;

			while (pt < endpt) {
				if (*pt == '.') {
					if (firstpt != NULL) {
						/* 2 . in a file name. Reject */
						ivldchar = 1;
						break;
					} else {
						int extlen = (int) (endpt - pt);

						firstpt = pt;
						if (firstpt - fname > 8) {
							/* base name longer than 8: reject */
							ivldchar = 1;
							break;
						} else if (extlen > 4) {
							/* Extension longer than 4 (including .): reject */
							ivldchar = 1;
							break;
						} else if (extlen == 1) {
							/* #Specification: file name / non MSDOS conforming / last char == .
							 * If the last character of a file name is
							 * a period, mangling is applied. MS-DOS does
							 * not support those file names.
							 */
							ivldchar = 1;
							break;
						} else if (extlen == 4) {
							/* #Specification: file name / non MSDOS conforming / mangling clash
							 * To avoid clash with    the umsdos mangling, any file
							 * with a special character as the first character
							 * of the extension will be mangled. This solves the
							 * following problem:
							 * 
							 * #
							 * touch FILE
							 * # FILE is invalid for DOS, so mangling is applied
							 * # file.{_1 is created in the DOS directory
							 * touch file.{_1
							 * # To UMSDOS file point to a single DOS entry.
							 * # So file.{_1 has to be mangled.
							 * #
							 */
							static char special[] =
							{
								SPECIAL_MANGLING, '\0'
							};

							if (strchr (special, firstpt[1]) != NULL) {
								ivldchar = 1;
								break;
							}
						}
					}
				} else if (lkp[(unsigned char) (*pt)] != *pt) {
					ivldchar = 1;
					break;
				}
				pt++;
			}
		} else {
			ivldchar = 1;
		}
		if (ivldchar
		    || (firstpt == NULL && len > 8)
		    || (len == UMSDOS_EMD_NAMELEN
			&& memcmp (fname, UMSDOS_EMD_FILE, UMSDOS_EMD_NAMELEN) == 0)) {
			/* #Specification: file name / --linux-.---
			 * The name of the EMD file --linux-.--- is map to a mangled
			 * name. So UMSDOS does not restrict its use.
			 */
			/* #Specification: file name / non MSDOS conforming / mangling
			 * Non MSDOS conforming file names must use some alias to fit
			 * in the MSDOS name space.
			 * 
			 * The strategy is simple. The name is simply truncated to
			 * 8 char. points are replace with underscore and a
			 * number is given as an extension. This number correspond
			 * to the entry number in the EMD file. The EMD file
			 * only need to carry the real name.
			 * 
			 * Upper case is also converted to lower case.
			 * Control character are converted to #.
			 * Spaces are converted to #.
			 * The following characters are also converted to #.
			 * #
			 * " * + , / : ; < = > ? [ \ ] | ~
			 * #
			 * 
			 * Sometimes the problem is not in MS-DOS itself but in
			 * command.com.
			 */
			int i;
			char *pt = info->fake.fname;

			base_len = msdos_len = (msdos_len > 8) ? 8 : msdos_len;
			/*
			 * There is no '.' any more so we know for a fact that
			 * the base length is the length.
			 */
			memcpy (info->fake.fname, fname, msdos_len);
			for (i = 0; i < msdos_len; i++, pt++)
				*pt = lkp[(unsigned char) (*pt)];
			*pt = '\0';	/* GLU  We force null termination. */
			info->msdos_reject = 1;
			/*
			 * The numeric extension is added only when we know
			 * the position in the EMD file, in umsdos_newentry(),
			 * umsdos_delentry(), and umsdos_findentry().
			 * See umsdos_manglename().
			 */
		} else {
			/* Conforming MSDOS file name */
			strncpy (info->fake.fname, fname, len);
			info->msdos_reject = 0;
			base_len = firstpt != NULL ? (int) (firstpt - fname) : len;
		}
		if (cardinal_per_size[base_len]) {
			/* #Specification: file name / MSDOS devices / mangling
			 * To avoid unreachable file from MS-DOS, any MS-DOS conforming
			 * file with a basename equal to one of the MS-DOS pseudo
			 * devices will be mangled.
			 * 
			 * If a file such as "prn" was created, it would be unreachable
			 * under MS-DOS because "prn" is assumed to be the printer, even
			 * if the file does have an extension.
			 * 
			 * Since the extension is unimportant to MS-DOS, we must patch
			 * the basename also. We simply insert a minus '-'. To avoid
			 * conflict with valid file with a minus in front (such as
			 * "-prn"), we add an mangled extension like any other
			 * mangled file name.
			 * 
			 * Here is the list of DOS pseudo devices:
			 * 
			 * #
			 * "prn","con","aux","nul",
			 * "lpt1","lpt2","lpt3","lpt4",
			 * "com1","com2","com3","com4",
			 * "clock$"
			 * #
			 * 
			 * and some standard ones for common DOS programs
			 * 
			 * "emmxxxx0","xmsxxxx0","setverxx"
			 * 
			 * (Thanks to Chris Hall <cah17@phoenix.cambridge.ac.uk>
			 * for pointing these out to me).
			 * 
			 * Is there one missing?
			 */
			/* This table must be ordered by length */
			static const char *tbdev[] =
			{
				"prn", "con", "aux", "nul",
				"lpt1", "lpt2", "lpt3", "lpt4",
				"com1", "com2", "com3", "com4",
				"clock$",
				"emmxxxx0", "xmsxxxx0", "setverxx"
			};

			/* Tell where to find in tbdev[], the first name of */
			/* a certain length */
			static const char start_ind_dev[9] =
			{
				0, 0, 0, 4, 12, 12, 13, 13, 16
			};
			char basen[9];
			int i;

			for (i = start_ind_dev[base_len - 1]; i < start_ind_dev[base_len]; i++) {
				if (memcmp (info->fake.fname, tbdev[i], base_len) == 0) {
					memcpy (basen, info->fake.fname, base_len);
					basen[base_len] = '\0';		/* GLU  We force null termination. */
					/*
					 * GLU        We do that only if necessary; we try to do the
					 * GLU        simple thing in the usual circumstance. 
					 */
					info->fake.fname[0] = '-';
					strcpy (info->fake.fname + 1, basen);	/* GLU  We already guaranteed a null would be at the end. */
					msdos_len = (base_len == 8) ? 8 : base_len + 1;
					info->msdos_reject = 1;
					break;
				}
			}
		}
		info->fake.fname[msdos_len] = '\0';	/* Help doing printk */
		/* GLU      This zero should (always?) be there already. */
		info->fake.len = msdos_len;
		/* Why not use info->fake.len everywhere? Is it longer?
                 */
		memcpy (info->entry.name, fname, len);
		info->entry.name[len] = '\0';	/* for printk */
		info->entry.name_len = len;
		ret = 0;
	}
	/*
	 * Evaluate how many records are needed to store this entry.
	 */
	info->recsize = umsdos_evalrecsize (len);
	return ret;
}

#ifdef TEST

struct MANG_TEST {
	char *fname;		/* Name to validate */
	int msdos_reject;	/* Expected msdos_reject flag */
	char *msname;		/* Expected msdos name */
};

struct MANG_TEST tb[] =
{
	"hello", 0, "hello",
	"hello.1", 0, "hello.1",
	"hello.1_", 0, "hello.1_",
	"prm", 0, "prm",

#ifdef PROPOSITION
	"HELLO", 1, "hello",
	"Hello.1", 1, "hello.1",
	"Hello.c", 1, "hello.c",
#else
/*
 * I find the three examples below very unfortunate.  I propose to
 * convert them to lower case in a quick preliminary pass, then test
 * whether there are other troublesome characters.  I have not made
 * this change, because it is not easy, but I wanted to mention the 
 * principle.  Obviously something like that would increase the chance
 * of collisions, for example between "HELLO" and "Hello", but these
 * can be treated elsewhere along with the other collisions.
 */

	"HELLO", 1, "hello",
	"Hello.1", 1, "hello_1",
	"Hello.c", 1, "hello_c",
#endif

	"hello.{_1", 1, "hello_{_",
	"hello\t", 1, "hello#",
	"hello.1.1", 1, "hello_1_",
	"hel,lo", 1, "hel#lo",
	"Salut.Tu.vas.bien?", 1, "salut_tu",
	".profile", 1, "_profile",
	".xv", 1, "_xv",
	"toto.", 1, "toto_",
	"clock$.x", 1, "-clock$",
	"emmxxxx0", 1, "-emmxxxx",
	"emmxxxx0.abcd", 1, "-emmxxxx",
	"aux", 1, "-aux",
	"prn", 1, "-prn",
	"prn.abc", 1, "-prn",
	"PRN", 1, "-prn",
  /* 
   * GLU        WARNING:  the results of these are different with my version
   * GLU        of mangling compared to the original one.
   * GLU        CAUSE:  the manner of calculating the baselen variable.
   * GLU                For you they are always 3.
   * GLU                For me they are respectively 7, 8, and 8.

   */
	"PRN.abc", 1, "prn_abc",
	"Prn.abcd", 1, "prn_abcd",
	"prn.abcd", 1, "prn_abcd",
	"Prn.abcdefghij", 1, "prn_abcd"
};

int main (int argc, char *argv[])
{
	int i, rold, rnew;

	printf ("Testing the umsdos_parse.\n");
	for (i = 0; i < sizeof (tb) / sizeof (tb[0]); i++) {
		struct MANG_TEST *pttb = tb + i;
		struct umsdos_info info;
		int ok = umsdos_parse (pttb->fname, strlen (pttb->fname), &info);

		if (strcmp (info.fake.fname, pttb->msname) != 0) {
			printf ("**** %s -> ", pttb->fname);
			printf ("%s <> %s\n", info.fake.fname, pttb->msname);
		} else if (info.msdos_reject != pttb->msdos_reject) {
			printf ("**** %s -> %s ", pttb->fname, pttb->msname);
			printf ("%d <> %d\n", info.msdos_reject, pttb->msdos_reject);
		} else {
			printf ("     %s -> %s %d\n", pttb->fname, pttb->msname
				,pttb->msdos_reject);
		}
	}
	printf ("Testing the new umsdos_evalrecsize.");
	for (i = 0; i < UMSDOS_MAXNAME; i++) {
		rnew = umsdos_evalrecsize (i);
		rold = umsdos_evalrecsize_old (i);
		if (!(i % UMSDOS_REC_SIZE)) {
			printf ("\n%d:\t", i);
		}
		if (rnew != rold) {
			printf ("**** %d newres: %d != %d \n", i, rnew, rold);
		} else {
			printf (".");
		}
	}
	printf ("\nEnd of Testing.\n");

	return 0;
}

#endif

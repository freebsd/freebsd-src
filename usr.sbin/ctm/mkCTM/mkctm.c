/* Still missing:
 *
 * mkctm 
 *	-B regex	Bogus
 *	-I regex	Ignore
 *      -D int		Damage
 *	-q		decrease verbosity
 *	-v		increase verbosity
 *      -l file		logfile
 *	name		cvs-cur
 *	prefix		src/secure
 *	dir1		"Soll"
 *	dir2		"Ist"
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <dirent.h>
#include <regex.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <md5.h>
#include <err.h>
#include <signal.h>

#define DEFAULT_IGNORE	"/CVS$|/\\.#|00_TRANS\\.TBL$"
#define DEFAULT_BOGUS	"\\.core$|\\.orig$|\\.rej$|\\.o$"
regex_t reg_ignore,  reg_bogus;
int	flag_ignore, flag_bogus;

int	verbose;
int	damage, damage_limit;
int	change;

FILE	*logf;

u_long s1_ignored,	s2_ignored;
u_long s1_bogus,	s2_bogus;
u_long s1_wrong,	s2_wrong;
u_long s_new_dirs,	s_new_files,	s_new_bytes;
u_long s_del_dirs,	s_del_files,	                s_del_bytes;
u_long 			s_files_chg,	s_bytes_add,	s_bytes_del;
u_long s_same_dirs,	s_same_files,	s_same_bytes;
u_long 			s_edit_files,	s_edit_bytes,	s_edit_saves;
u_long 			s_sub_files,	s_sub_bytes;

void
Usage(void)
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "\tmkctm [-options] name number timestamp prefix");
	fprintf(stderr, " dir1 dir2");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "\t\t-B bogus_regexp\n");
	fprintf(stderr, "\t\t-D damage_limit\n");
	fprintf(stderr, "\t\t-I ignore_regexp\n");
	fprintf(stderr, "\t\t-q\n");
	fprintf(stderr, "\t\t-v\n");
}

void
print_stat(FILE *fd, char *pre)
{
    fprintf(fd, "%sNames:\n", pre);
    fprintf(fd, "%s  ignore:  %5lu ref   %5lu target\n",
	    pre, s1_ignored, s2_ignored);
    fprintf(fd, "%s  bogus:   %5lu ref   %5lu target\n",
	    pre, s1_bogus, s2_bogus);
    fprintf(fd, "%s  wrong:   %5lu ref   %5lu target\n", 
	    pre, s1_wrong, s2_wrong);
    fprintf(fd, "%sDelta:\n", pre);
    fprintf(fd, "%s  new:     %5lu dirs  %5lu files  %9lu plus\n", 
	    pre, s_new_dirs, s_new_files, s_new_bytes);
    fprintf(fd, "%s  del:     %5lu dirs  %5lu files                   %9lu minus\n", 
	    pre, s_del_dirs, s_del_files, s_del_bytes);
    fprintf(fd, "%s  chg:                 %5lu files  %9lu plus   %9lu minus\n",
	    pre, s_files_chg, s_bytes_add, s_bytes_del);
    fprintf(fd, "%s  same:    %5lu dirs  %5lu files  %9lu bytes\n", 
	    pre, s_same_dirs, s_same_files, s_same_bytes);
    fprintf(fd, "%sMethod:\n", pre);
    fprintf(fd, "%s  edit:                %5lu files  %9lu bytes  %9lu saved\n", 
	    pre, s_edit_files, s_edit_bytes, s_edit_saves);
    fprintf(fd, "%s  sub:                 %5lu files  %9lu bytes\n", 
	    pre, s_sub_files, s_sub_bytes);

}

void
stat_info(int foo)
{
	signal(SIGINFO, stat_info);
	print_stat(stderr, "INFO: ");
}

void DoDir(const char *dir1, const char *dir2, const char *name);

static struct stat st;
static __inline struct stat *
StatFile(char *name)
{
	if (lstat(name, &st) < 0) 
		err(1, "Couldn't stat %s\n", name);
	return &st;
}

int
dirselect(struct dirent *de)
{
	if (!strcmp(de->d_name, "."))	return 0;
	if (!strcmp(de->d_name, ".."))	return 0;
	return 1;
}

void
name_stat(const char *pfx, const char *dir, const char *name, struct dirent *de)
{
	char *buf = alloca(strlen(dir) + strlen(name) + 
		strlen(de->d_name) + 3);
	struct stat *st;

	strcpy(buf, dir); 
		strcat(buf, "/"); strcat(buf, name);
		strcat(buf, "/"); strcat(buf, de->d_name);
	st = StatFile(buf);
	printf("%s %s%s %u %u %o", 
	    pfx, name, de->d_name, 
	    st->st_uid, st->st_gid, st->st_mode & ~S_IFMT);
	fprintf(logf, "%s %s%s\n", pfx, name, de->d_name);
	if (verbose > 1) {
		fprintf(stderr, "%s %s%s\n", pfx, name, de->d_name);
	}
}

void
Equ(const char *dir1, const char *dir2, const char *name, struct dirent *de)
{
	if (de->d_type == DT_DIR) {
		char *p = alloca(strlen(name)+strlen(de->d_name)+2);

		strcpy(p, name);  strcat(p, de->d_name); strcat(p, "/");
		DoDir(dir1, dir2, p);
		s_same_dirs++;
	} else {
		char *buf1 = alloca(strlen(dir1) + strlen(name) + 
			strlen(de->d_name) + 3);
		char *buf2 = alloca(strlen(dir2) + strlen(name) + 
			strlen(de->d_name) + 3);
		char *m1, md5_1[33], *m2, md5_2[33];
		u_char *p1, *p2;
		int fd1, fd2;
		struct stat s1, s2;

		strcpy(buf1, dir1); 
			strcat(buf1, "/"); strcat(buf1, name);
			strcat(buf1, "/"); strcat(buf1, de->d_name);
		fd1 = open(buf1, O_RDONLY);
		if(fd1 < 0) { perror(buf1); exit(3); }
		fstat(fd1, &s1);
		strcpy(buf2, dir2); 
			strcat(buf2, "/"); strcat(buf2, name);
			strcat(buf2, "/"); strcat(buf2, de->d_name);
		fd2 = open(buf2, O_RDONLY);
		if(fd2 < 0) { perror(buf2); exit(3); }
		fstat(fd2, &s2);
#if 0
		/* XXX if we could just trust the size to change... */
		if (s1.st_size == s2.st_size) {
			s_same_files++;
			s_same_bytes += s1.st_size;
			close(fd1);
			close(fd2);
			goto finish;
		}
#endif
		p1=mmap(0, s1.st_size, PROT_READ, MAP_PRIVATE, fd1, 0);
		if ((int)p1 == -1) { perror(buf1); exit(3); }
		close(fd1);

		p2=mmap(0, s2.st_size, PROT_READ, MAP_PRIVATE, fd2, 0);
		if ((int)p2 == -1) { perror(buf2); exit(3); }
		close(fd2);

		/* If identical, we're done. */
		if((s1.st_size == s2.st_size) && !memcmp(p1, p2, s1.st_size)) {
			s_same_files++;
			s_same_bytes += s1.st_size;
			goto finish;
		}

		s_files_chg++;
		change++;
		if (s1.st_size > s2.st_size)
			s_bytes_del += (s1.st_size - s2.st_size);
		else
			s_bytes_add += (s2.st_size - s1.st_size);

		m1 = MD5Data(p1, s1.st_size, md5_1);
		m2 = MD5Data(p2, s2.st_size, md5_2);
		
		/* Just a curiosity... */
		if(!strcmp(m1, m2)) {
			if (s1.st_size != s2.st_size) 
				fprintf(stderr,
		"Notice: MD5 same for files of diffent size:\n\t%s\n\t%s\n",
					buf1, buf2);
			goto finish;
		}

		{
			u_long l = s2.st_size + 2;
			u_char *cmd = alloca(strlen(buf1)+strlen(buf2)+100);
			u_char *ob = alloca(l), *p;
			int j;
			FILE *F;
			
			if (p1[s1.st_size-1] != '\n') {
				if (verbose > 0) 
					fprintf(stderr,
					    "last char != \\n in %s\n",
					     buf1);
				goto subst;
			}

			if (p2[s2.st_size-1] != '\n') {
				if (verbose > 0) 
					fprintf(stderr,
					    "last char != \\n in %s\n",
					     buf2);
				goto subst;
			}

			for (p=p1; p<p1+s1.st_size; p++)
				if (!*p) {
					if (verbose > 0) 
						fprintf(stderr,
						    "NULL char in %s\n",
						     buf1);
					goto subst;
				}

			for (p=p2; p<p2+s2.st_size; p++)
				if (!*p) {
					if (verbose > 0) 
						fprintf(stderr,
						    "NULL char in %s\n",
						     buf2);
					goto subst;
				}

			strcpy(cmd, "diff -n ");
			strcat(cmd, buf1);
			strcat(cmd, " ");
			strcat(cmd, buf2);
			F = popen(cmd, "r");
			for (j = 1, l = 0; l < s2.st_size; ) {
				j = fread(ob+l, 1, s2.st_size - l, F);
				if (j < 1) 
					break;
				l += j;
				continue;
			}
			if (j) {
				l = 0;
				while (EOF != fgetc(F))
					continue;
			}
			pclose(F);
			
			if (l && l < s2.st_size) {
				name_stat("CTMFN", dir2, name, de);
				printf(" %s %s %d\n", m1, m2, (unsigned)l);
				fwrite(ob, 1, l, stdout);
				putchar('\n');
				s_edit_files++;
				s_edit_bytes += l;
				s_edit_saves += (s2.st_size - l);
			} else {
			subst:
				name_stat("CTMFS", dir2, name, de);
				printf(" %s %s %u\n", m1, m2, (unsigned)s2.st_size);
				fwrite(p2, 1, s2.st_size, stdout);
				putchar('\n');
				s_sub_files++;
				s_sub_bytes += s2.st_size;
			}
		}
	    finish:
		munmap(p1, s1.st_size);
		munmap(p2, s2.st_size);
	}
}

void
Add(const char *dir1, const char *dir2, const char *name, struct dirent *de)
{
	change++;
	if (de->d_type == DT_DIR) {
		char *p = alloca(strlen(name)+strlen(de->d_name)+2);
		strcpy(p, name);  strcat(p, de->d_name); strcat(p, "/");
		name_stat("CTMDM", dir2, name, de);
		putchar('\n');
		s_new_dirs++;
		DoDir(dir1, dir2, p);
	} else if (de->d_type == DT_REG) {
		char *buf2 = alloca(strlen(dir2) + strlen(name) + 
			strlen(de->d_name) + 3);
		char *m2, md5_2[33];
		u_char *p1;
		struct stat st;
		int fd1;

		strcpy(buf2, dir2); 
			strcat(buf2, "/"); strcat(buf2, name);
			strcat(buf2, "/"); strcat(buf2, de->d_name);
		fd1 = open(buf2, O_RDONLY);
		if (fd1 < 0) {perror(buf2); exit (3); }
		fstat(fd1, &st);
		p1=mmap(0, st.st_size, PROT_READ, MAP_PRIVATE, fd1, 0);
		if ((int)p1 == -1) { perror(buf2); exit(3); }
		close(fd1);
		m2 = MD5Data(p1, st.st_size, md5_2);
		name_stat("CTMFM", dir2, name, de);
		printf(" %s %u\n", m2, (unsigned)st.st_size);
		fwrite(p1, 1, st.st_size, stdout);
		putchar('\n');
		munmap(p1, st.st_size);
		s_new_files++;
		s_new_bytes += st.st_size;
	}
}

void
Del (const char *dir1, const char *dir2, const char *name, struct dirent *de)
{
	damage++;
	change++;
	if (de->d_type == DT_DIR) {
		char *p = alloca(strlen(name)+strlen(de->d_name)+2);
		strcpy(p, name);  strcat(p, de->d_name); strcat(p, "/");
		DoDir(dir1, dir2, p);
		printf("CTMDR %s%s\n", name, de->d_name);
		fprintf(logf, "CTMDR %s%s\n", name, de->d_name);
		if (verbose > 1) {
			fprintf(stderr, "CTMDR %s%s\n", name, de->d_name);
		}
		s_del_dirs++;
	} else if (de->d_type == DT_REG) {
		char *buf1 = alloca(strlen(dir1) + strlen(name) + 
			strlen(de->d_name) + 3);
		char *m1, md5_1[33];
		strcpy(buf1, dir1); 
			strcat(buf1, "/"); strcat(buf1, name);
			strcat(buf1, "/"); strcat(buf1, de->d_name);
		m1 = MD5File(buf1, md5_1);
		printf("CTMFR %s%s %s\n", name, de->d_name, m1);
		fprintf(logf, "CTMFR %s%s %s\n", name, de->d_name, m1);
		if (verbose > 1) {
			fprintf(stderr, "CTMFR %s%s\n", name, de->d_name);
		}
		s_del_files++;
		s_del_bytes += StatFile(buf1)->st_size;
	}
}

void
GetNext(int *i, int *n, struct dirent **nl, const char *dir, const char *name, u_long *ignored, u_long *bogus, u_long *wrong)
{
	char buf[BUFSIZ];
	char buf1[BUFSIZ];

	for (;;) {
		for (;;) {
			(*i)++;
			if (*i >= *n)
				return;
			strcpy(buf1, name);
			if (buf1[strlen(buf1)-1] != '/')
				strcat(buf1, "/"); 
			strcat(buf1, nl[*i]->d_name);
			if (flag_ignore && 
			    !regexec(&reg_ignore, buf1, 0, 0, 0)) {
				(*ignored)++;
				fprintf(logf, "Ignore %s\n", buf1);
				if (verbose > 2) {
					fprintf(stderr, "Ignore %s\n", buf1);
				}
			} else if (flag_bogus && 
			    !regexec(&reg_bogus, buf1, 0, 0, 0)) {
				(*bogus)++;
				fprintf(logf, "Bogus %s\n", buf1);
				fprintf(stderr, "Bogus %s\n", buf1);
				damage++;
			} else {
				*buf = 0;
				if (*dir != '/')
					strcat(buf, "/");
				strcat(buf, dir); 
				if (buf[strlen(buf)-1] != '/')
					strcat(buf, "/"); 
				strcat(buf, buf1);
				break;
			}
			free(nl[*i]); nl[*i] = 0;
		}
		/* If the filesystem didn't tell us, find type */
		if (nl[*i]->d_type == DT_UNKNOWN) 
			nl[*i]->d_type = IFTODT(StatFile(buf)->st_mode);
		if (nl[*i]->d_type == DT_REG || nl[*i]->d_type == DT_DIR)
			break;
		(*wrong)++;
		if (verbose > 0)
			fprintf(stderr, "Wrong %s\n", buf);
		free(nl[*i]); nl[*i] = 0;
	}
}

void
DoDir(const char *dir1, const char *dir2, const char *name)
{
	int i1, i2, n1, n2, i;
	struct dirent **nl1, **nl2;
	char *buf1 = alloca(strlen(dir1) + strlen(name) + 4);
	char *buf2 = alloca(strlen(dir2) + strlen(name) + 4);

	strcpy(buf1, dir1); strcat(buf1, "/"); strcat(buf1, name);
	strcpy(buf2, dir2); strcat(buf2, "/"); strcat(buf2, name);
	n1 = scandir(buf1, &nl1, dirselect, alphasort);
	n2 = scandir(buf2, &nl2, dirselect, alphasort);
	i1 = i2 = -1;
	GetNext(&i1, &n1, nl1, dir1, name, &s1_ignored, &s1_bogus, &s1_wrong);
	GetNext(&i2, &n2, nl2, dir2, name, &s2_ignored, &s2_bogus, &s2_wrong);
	for (;i1 < n1 || i2 < n2;) {

		if (damage_limit && damage > damage_limit)
			break;

		/* Get next item from list 1 */
		if (i1 < n1 && !nl1[i1]) 
			GetNext(&i1, &n1, nl1, dir1, name, 
				&s1_ignored, &s1_bogus, &s1_wrong);

		/* Get next item from list 2 */
		if (i2 < n2 && !nl2[i2]) 
			GetNext(&i2, &n2, nl2, dir2, name, 
				&s2_ignored, &s2_bogus, &s2_wrong);

		if (i1 >= n1 && i2 >= n2) {
			/* Done */
			break;
		} else if (i1 >= n1 && i2 < n2) {
			/* end of list 1, add anything left on list 2 */
			Add(dir1, dir2, name, nl2[i2]);
			free(nl2[i2]); nl2[i2] = 0;
		} else if (i1 < n1 && i2 >= n2) {
			/* end of list 2, delete anything left on list 1 */
			Del(dir1, dir2, name, nl1[i1]);
			free(nl1[i1]); nl1[i1] = 0;
		} else if (!(i = strcmp(nl1[i1]->d_name, nl2[i2]->d_name))) {
			/* Identical names */
			if (nl1[i1]->d_type == nl2[i2]->d_type) {
				/* same type */
				Equ(dir1, dir2, name, nl1[i1]);
			} else {
				/* different types */
				Del(dir1, dir2, name, nl1[i1]);
				Add(dir1, dir2, name, nl2[i2]);
			}
			free(nl1[i1]); nl1[i1] = 0;
			free(nl2[i2]); nl2[i2] = 0;
		} else if (i < 0) {
			/* Something extra in list 1, delete it */
			Del(dir1, dir2, name, nl1[i1]);
			free(nl1[i1]); nl1[i1] = 0;
		} else {
			/* Something extra in list 2, add it */
			Add(dir1, dir2, name, nl2[i2]);
			free(nl2[i2]); nl2[i2] = 0;
		}
	}
	if (n1 >= 0)
		free(nl1);
	if (n2 >= 0)
		free(nl2);
}

int
main(int argc, char **argv)
{
	int i;
	extern char *optarg;
	extern int optind;

	setbuf(stderr, NULL);

#if 0
	if (regcomp(&reg_bogus, DEFAULT_BOGUS, REG_EXTENDED | REG_NEWLINE))
		/* XXX use regerror to explain it */
		errx(1, "Default regular expression argument to -B is botched");
	flag_bogus = 1;

	if (regcomp(&reg_ignore, DEFAULT_IGNORE, REG_EXTENDED | REG_NEWLINE))
		/* XXX use regerror to explain it */
		errx(1, "Default regular expression argument to -I is botched");
	flag_ignore = 1;
#endif

	while ((i = getopt(argc, argv, "D:I:B:l:qv")) != EOF)
		switch (i) {
		case 'D':
			damage_limit = strtol(optarg, 0, 0);
			if (damage_limit < 0)
				errx(1, "Damage limit must be positive");
			break;
		case 'I':
			if (flag_ignore)
				regfree(&reg_ignore);
			flag_ignore = 0;
			if (!*optarg)
				break;
			if (regcomp(&reg_ignore, optarg,
			    REG_EXTENDED | REG_NEWLINE))
				/* XXX use regerror to explain it */
				errx(1, "Regular expression argument to -I is botched");
			flag_ignore = 1;
			break;
		case 'B':
			if (flag_bogus)
				regfree(&reg_bogus);
			flag_bogus = 0;
			if (!*optarg)
				break;
			if (regcomp(&reg_bogus, optarg,
			    REG_EXTENDED | REG_NEWLINE))
				/* XXX use regerror to explain it */
				errx(1, "Regular expression argument to -B is botched");
			flag_bogus = 1;
			break;
		case 'l':
			logf = fopen(optarg, "w");
			if (!logf)
				err(1, optarg);
			break;
		case 'q':
			verbose--;
			break;
		case 'v':
			verbose++;
			break;
		case '?':
		default:
			Usage();
			return (1);
		}
	argc -= optind;
	argv += optind;

	if (!logf)
		logf = fopen("/dev/null", "w");

	setbuf(stdout, 0);

	if (argc != 6) {
		Usage();
		return (1);
	}
	
	signal(SIGINFO, stat_info);

	fprintf(stderr, "CTM_BEGIN 2.0 %s %s %s %s\n",
		argv[0], argv[1], argv[2], argv[3]);
	fprintf(logf, "CTM_BEGIN 2.0 %s %s %s %s\n",
		argv[0], argv[1], argv[2], argv[3]);
	printf("CTM_BEGIN 2.0 %s %s %s %s\n",
		argv[0], argv[1], argv[2], argv[3]);
	DoDir(argv[4], argv[5], "");
	if (damage_limit && damage > damage_limit) {
		print_stat(stderr, "DAMAGE: ");
		errx(1, "Damage of %d would exceed %d files", 
			damage, damage_limit);
	} else if (change < 2) {
		errx(4, "No changes");
	} else {
		printf("CTM_END ");
		fprintf(logf, "CTM_END\n");
		print_stat(stderr, "END: ");
	}
	exit(0);
}

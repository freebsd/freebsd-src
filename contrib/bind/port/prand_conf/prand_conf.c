/* $Id: prand_conf.c,v 1.7 2001/03/07 06:46:33 marka Exp $
 *
 * Portions Copyright (c) 1995-1998 by TIS Labs at Network Assoociates Inc.
 * Portions Copyright (c) 1998-1998 by TIS Labs @ Network Associates Inc.
 *
 * Permission to use, copy modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND NETWORK ASSOCIATES
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL
 * TRUSTED INFORMATION SYSTEMS BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THE SOFTWARE.
 *
 * program to find where system commands reside 
 * and what directores are avialable for inspection 
 * this information is stored in the file prand_conf.h in current directory
 *
 * function my_find get variable number of arguments
 * the first argument is the name of the command 
 * all remaining arguments are list of directories to search for the command in
 * this function returns the path to the command
 */

#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define LINE_MAX 256

int 
my_find(char *cmd, char **dir)
{
	int curr = 0, c_len, i;
	char cmd_line[LINE_MAX];
	
	memset(cmd_line, 0, sizeof(cmd_line));
	c_len = strlen(cmd);
	for (i = 0; dir[i]; i++) {
		curr = strlen(dir[i]);
		if (curr + c_len < sizeof(cmd_line)-3) {
			sprintf(cmd_line, "%s%s",dir[i], cmd);
			if (access(cmd_line, X_OK) == 0) 
				return (i);
			memset(cmd_line, 0, c_len + curr + 2);
		}
	}
	return (0);
}

/* 
 * function to simulate the ` ` operator in perl  return the number
 * of bytes read from the pipe 
 */
int
pipe_run(char *cmd_line)
{
	FILE *pd;
	char scratch[LINE_MAX];
	int ex, no_bytes = 0, no = 1;

	pd = popen(cmd_line, "r");
	for (; (pd != NULL) && (no > 0); no_bytes += no) 
		no = fread(scratch, sizeof(char), sizeof(scratch), pd);
	ex = pclose(pd);
	return (no_bytes);
}

/*
 * function that executes a command with certain flags and checks that the
 * output is at least certain length
 * First parameter the command 
 * Second parameter is ther flags 
 * third parameter is the number of bytes required 
 * output is 1 if the command works 0 if not 
 * This function writes to the include file if
 */
int 
ex(FILE *fd, char *path, char *cmd, char *arg, int lower_bound)
{
	char line[LINE_MAX];

	if (strlen(path) + strlen(cmd) + strlen(arg) < sizeof(line)-7) { 
		memset(line, 0, sizeof(line));
		sprintf(line, "%s%s %s 2>&1", path, cmd, arg);
		if (pipe_run(line) > lower_bound) {
			fprintf(fd,"\t\"%s\",\n", line);
			return (1);
		}
	}
	return (0);
}

int 
main() 
{
	extern int errno;
	FILE *fd;
	int res, vm, i;
	int ps, arp, net, dig, cmd;
/*
 * set up list of directories where each command may be found in 
 */
	char *arp_path[] = {"/usr/sbin", "/sbin", "/usr/etc/", "/etc/", 
			   "/usr/bin/", NULL};
	char *ps_path[] = {"/usr/bin", "/bin/", NULL};
	char *net_path[]  = {"/usr/ucb/", "/usr/bin/", "/usr/etc/", 
			     "/usr/sbin/", "/bin/", NULL};
	char *dig_path[] = {"/usr/bin/", "/usr/local/bin/", NULL};
	char **df_path  = ps_path;
	char *uptime_path[] = {"/usr/ucb/", "/usr/bin/", "/usr/bsd/", NULL};
	char *iostat_path[] = { "/usr/bin/", "/bin/", "/usr/sbin/", NULL};
	char *vmstat_path[] = {"/usr/ucb/", "/usr/bin/", "/usr/sbin/", NULL};
	char *vm_stat_path[] = {"/usr/ucb/", "/usr/bin/", NULL};
	char **w_path = uptime_path;

/* find which directories exist  */
	char *dirs[] = {"/tmp", "/usr/tmp", "/var/tmp", ".", "/",  
			"/var/spool", "/usr/spool", 
			"/usr/adm", "/var/adm", "/dev", 
			"/usr/mail", "/var/spool/mail", "/var/mail", 
			"/home", "/usr/home", NULL};

	char *files[] = {"/proc/stat", "/proc/rtc", "/proc/meminfo", 
			 "/proc/interrupts",  "/proc/self/status", 
			 "/proc/self/maps",  "/proc/curproc/status",
			 "/proc/curproc/map",
			 "/var/log/messages", "/var/log/wtmp", 
			 "/var/log/lastlog", "/var/adm/messages", 
			 "/var/adm/wtmp", "/var/adm/lastlog", NULL};

	struct stat st;
	time_t tim;
/* main program: */

	if ((fd = fopen("prand_conf.h", "w")) == NULL) {
		perror("Failed creating file prand_conf.h");
		exit(errno);
	}

	fprintf(fd, "#ifndef _PRAND_CMD_H_\n#define _PRAND_CMD_H_\n\n");

	fprintf(fd, "static const char *cmds[] = {\n");
       
	if ((ps = my_find("ps", ps_path)) >= 0)
		res = ex(fd, ps_path[ps], "ps","-axlw", 460) || 
			ex(fd, ps_path[ps], "ps", "-ef", 300) || 
				ex(fd, ps_path[ps], "ps", "-ale", 300);

	if ((arp = my_find("arp", arp_path)) >= 0) 
	    res = ex(fd, arp_path[arp], "arp", "-n -a", 40);

	if ((net = my_find("netstat", net_path)) >= 0)
		res = ex(fd, net_path[net], "netstat", "-an", 1000);
	if ((cmd = my_find("df", df_path)) >= 0)
		res = ex(fd, df_path[cmd], "df", "", 40);

	if ((dig = my_find("dig", dig_path)) >= 0)
		res = ex(fd, dig_path[dig], "dig", "com. soa +ti=1 +retry=0", 
			 100);
	if ((cmd = my_find("uptime", uptime_path)) >= 0)
	     res = ex(fd, uptime_path[cmd], "uptime", "", 40);
	if ((cmd = my_find("printenv", uptime_path)) >= 0)
	     res = ex(fd, uptime_path[cmd], "printenv", "", 400);
	if (net >= 0)
		res = ex(fd, net_path[net], "netstat", "-s", 1000);

	if (dig >= 0)
		res = ex(fd, net_path[net], "dig", ". soa +ti=1 +retry=0",100);
	if ((cmd = my_find("iostat", iostat_path)) >= 0)
		res = ex(fd, iostat_path[cmd], "iostat", "", 100);

	vm  = 0;
	if ((cmd = my_find("vmstat", vmstat_path)))
		vm = ex(fd, vmstat_path[cmd], "vmstat", "", 200);
	if (vm ==0 && ((cmd = my_find("vm_stat", vm_stat_path)) >= 0))
	    vm = ex(fd, vm_stat_path[cmd], "vm_stat", "", 200);
	if ((cmd = my_find("w", w_path)))
		res = ex(fd, w_path[cmd], "w", "", 100);
	fprintf(fd,"\tNULL\n};\n\n");

	fprintf(fd, "static const char *dirs[] = {\n");

	for (i=0; dirs[i]; i++) { 
		if (lstat(dirs[i], &st) == 0) 
			if (S_ISDIR(st.st_mode))
				fprintf(fd,"\t\"%s\",\n", dirs[i]);
	}
	fprintf(fd,"\tNULL\n};\n\n");


	fprintf(fd, "static const char *files[] = {\n");
	tim = time(NULL);
	for (i=0; files[i]; i++) {
		if (lstat(files[i],&st) == 0)
			if (S_ISREG(st.st_mode) && 
			    (tim - st.st_mtime) < 84600) 
				fprintf(fd,"\t\"%s\",\n", files[i]);
	}
	fprintf (fd, "\tNULL\n};\n");
		
	if ((stat("/dev/random", &st) == 0))
		if (S_ISCHR(st.st_mode))
			fprintf(fd, "\n#ifndef HAVE_DEV_RANDOM\n%s%s",
				"# define HAVE_DEV_RANDOM 1\n",
				"#endif /* HAVE_DEV_RANDOM */\n\n");

	fprintf(fd, "\n#endif /* _PRAND_CMD_H_ */\n");
	fclose(fd);
	exit (0);
}

#ifndef LINT
static const char rcsid[] = "$Header: /proj/cvs/isc/bind8/src/lib/dst/prandom.c,v 1.12 2001/07/26 01:20:09 marka Exp $";
#endif
/*
 * Portions Copyright (c) 1995-1998 by Trusted Information Systems, Inc.
 *
 * Permission to use, copy modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND TRUSTED INFORMATION SYSTEMS
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL
 * TRUSTED INFORMATION SYSTEMS BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THE SOFTWARE.
 */

#include "port_before.h"

#include <assert.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <dirent.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "dst_internal.h"
#include "prand_conf.h"

#include "port_after.h"

#ifndef DST_NUM_HASHES
#define DST_NUM_HASHES 4
#endif
#ifndef DST_NUMBER_OF_COUNTERS
#define DST_NUMBER_OF_COUNTERS 5	/* 32 * 5 == 160 == SHA(1) > MD5 */
#endif

/* 
 * the constant below is a prime number to make fixed data structues like 
 * stat and time wrap over blocks. This adds certain uncertanty to what is 
 * in each digested block. 
 * The prime number 2879 has the special property that when 
 * divided by 2,4 and 6 the result is also a prime numbers
 */

#ifndef DST_RANDOM_BLOCK_SIZE
#define DST_RANDOM_BLOCK_SIZE 2879
#endif

/* 
 * This constant dictatates how many bits we shift to the right before using a 
 */
#ifndef DST_SHIFT
#define DST_SHIFT 9
#endif

/*
 * An initalizer that is as bad as any other with half the bits set 
 */
#ifndef DST_RANDOM_PATTERN
#define DST_RANDOM_PATTERN 0x8765CA93
#endif
/* 
 * things must have changed in the last 3600 seconds to be used 
 */
#define MAX_OLD 3600


/*  
 *  these two data structure are used to process input data into digests, 
 *
 *  The first structure is containts a pointer to a DST HMAC key 
 *  the variables accompanying are used for 
 *	step : select every step byte from input data for the hash
 *	block: number of data elements going into each hash
 *	digested: number of data elements digested so far
 *	curr: offset into the next input data for the first byte. 
 */
typedef struct hash {
	DST_KEY *key;
	void *ctx;
	int digested, block, step, curr;
} prand_hash;

/*
 *  This data structure controlls number of hashes and keeps track of 
 *  overall progress in generating correct number of bytes of output.
 *	output  : array to store the output data in
 *	needed  : how many bytes of output are needed
 *	filled  : number of bytes in output so far. 
 *	bytes   : total number of bytes processed by this structure
 *	file_digest : the HMAC key used to digest files.
 */
typedef struct work {
	int needed, filled, bytes;
	u_char *output;
	prand_hash *hash[DST_NUM_HASHES];
	DST_KEY *file_digest;
} dst_work;


/* 
 * forward function declarations 
 */
static int get_dev_random(u_char *output, int size);
static int do_time(dst_work *work);
static int do_ls(dst_work *work);
static int unix_cmd(dst_work *work);
static int digest_file(dst_work *work);

static void force_hash(dst_work *work, prand_hash *hash);
static int do_hash(dst_work *work, prand_hash *hash, const u_char *input,
		   int size);
static int my_digest(dst_work *tmp, const u_char *input, int size);
static prand_hash *get_hmac_key(int step, int block);

static int own_random(dst_work *work);


/* 
 * variables used in the quick random number generator 
 */
static u_int32_t ran_val = DST_RANDOM_PATTERN;
static u_int32_t ran_cnt = (DST_RANDOM_PATTERN >> 10);

/* 
 * setting the quick_random generator to particular values or if both 
 * input parameters are 0 then set it to initial vlaues
 */

void
dst_s_quick_random_set(u_int32_t val, u_int32_t cnt)
{
	ran_val = (val == 0) ? DST_RANDOM_PATTERN : val;
	ran_cnt = (cnt == 0) ? (DST_RANDOM_PATTERN >> 10) : cnt;
}

/* 
 * this is a quick and random number generator that seems to generate quite 
 * good distribution of data 
 */
u_int32_t
dst_s_quick_random(int inc)
{
	ran_val = ((ran_val >> 13) ^ (ran_val << 19)) ^
		((ran_val >> 7) ^ (ran_val << 25));
	if (inc > 0)		/* only increasing values accepted */
		ran_cnt += inc;
	ran_val += ran_cnt++;
	return (ran_val);
}

/* 
 * get_dev_random: Function to read /dev/random reliably
 * this function returns how many bytes where read from the device.
 * port_after.h should set the control variable HAVE_DEV_RANDOM 
 */
static int
get_dev_random(u_char *output, int size)
{
#ifdef HAVE_DEV_RANDOM
	struct stat st;
	int n = 0, fd = -1, s;

	s = stat("/dev/random", &st);
	if (s == 0 && S_ISCHR(st.st_mode)) {
		if ((fd = open("/dev/random", O_RDONLY | O_NONBLOCK)) != -1) {
			if ((n = read(fd, output, size)) < 0)
				n = 0;
			close(fd);
		}
		return (n);
	}
#endif
	return (0);
}

/*
 * Portable way of getting the time values if gettimeofday is missing 
 * then compile with -DMISSING_GETTIMEOFDAY  time() is POSIX compliant but
 * gettimeofday() is not.
 * Time of day is predictable, we are looking for the randomness that comes 
 * the last few bits in the microseconds in the timer are hard to predict when 
 * this is invoked at the end of other operations
 */
struct timeval *mtime;
static int
do_time(dst_work *work)
{
	int cnt = 0;
	static u_char tmp[sizeof(struct timeval) + sizeof(struct timezone)];
	struct timezone *zone;

	zone = (struct timezone *) tmp;
	mtime = (struct timeval *)(tmp + sizeof(struct timezone));
	gettimeofday(mtime, zone);
	cnt = sizeof(tmp);
	my_digest(work, tmp, sizeof(tmp));

	return (cnt);
}

/*
 * this function simulates the ls command, but it uses stat which gives more
 * information and is harder to guess 
 * Each call to this function will visit the next directory on the list of 
 * directories, in a circular manner. 
 * return value is the number of bytes added to the temp buffer
 *
 * do_ls() does not visit subdirectories
 * if attacker has access to machine it can guess most of the values seen
 * thus it is important to only visit directories that are freqently updated
 * Attacker that has access to the network can see network traffic 
 * when NFS mounted directories are accessed and know exactly the data used
 * but may not know exactly in what order data is used. 
 * Returns the number of bytes that where returned in stat structures
 */
static int
do_ls(dst_work *work)
{
	struct dir_info { 
		uid_t  uid;
		gid_t  gid;
		off_t size;
		time_t atime, mtime, ctime;
	};
	static struct dir_info dir_info;
	struct stat buf;
	struct dirent *entry;
	static int i = 0;
	static unsigned long d_round = 0;
	struct timeval tv;
	int n = 0, dir_len, tb_i = 0, out = 0;

	char file_name[1024];
	u_char tmp_buff[1024]; 
	DIR *dir = NULL;

	if (dirs[i] == NULL) 	/* if at the end of the list start over */
		i = 0;
	if (stat(dirs[i++], &buf))  /* directory does not exist */
		return (0);

	gettimeofday(&tv, NULL);
	if (d_round == 0) 
		d_round = tv.tv_sec - MAX_OLD;
	else if (i==1) /* if starting a new round cut what we accept */
		d_round += (tv.tv_sec - d_round)/2;

	if (buf.st_atime < (time_t)d_round) 
		return (0);

	EREPORT(("do_ls i %d filled %4d\n", i-1, work->filled));
	memcpy(tmp_buff, &buf, sizeof(buf)); 
	tb_i += sizeof(buf);


	if ((dir = opendir(dirs[i-1])) == NULL)/* open it for read */
		return (0);
	strcpy(file_name, dirs[i-1]);
	dir_len = strlen(file_name);
	file_name[dir_len++] = '/';
	while ((entry = readdir(dir))) {
		int len = strlen(entry->d_name);
		out += len;
		if (my_digest(work, (u_char *)entry->d_name, len))
			break;
	
		memcpy(&file_name[dir_len], entry->d_name, len);
		file_name[dir_len + len] = 0x0;
		/* for all entries in dir get the stats */
		if (stat(file_name, &buf) == 0) {
			n++;	/* count successfull stat calls */
			/* copy non static fields */
			dir_info.uid   += buf.st_uid;
			dir_info.gid   += buf.st_gid;
			dir_info.size  += buf.st_size;
			dir_info.atime += buf.st_atime;
			dir_info.mtime += buf.st_mtime;
			dir_info.ctime += buf.st_ctime;
			out += sizeof(dir_info);
			if(my_digest(work, (u_char *)&dir_info, 
				     sizeof(dir_info)))
				break; 
		}
	}
	closedir(dir);	/* done */
	out += do_time(work);	/* add a time stamp */
	return (out);
}


/* 
 * unix_cmd() 
 * this function executes the a command from the cmds[] list of unix commands 
 * configured in the prand_conf.h file
 * return value is the number of bytes added to the randomness temp buffer
 * 
 * it returns the number of bytes that where read in
 * if more data is needed at the end time is added to the data.
 * This function maintains a state to selects the next command to run
 * returns the number of bytes read in from the command 
 */
static int
unix_cmd(dst_work *work)
{
	static int cmd_index = 0;
	int cnt = 0, n;
	FILE *pipe;
	u_char buffer[4096];

	if (cmds[cmd_index] == NULL)
		cmd_index = 0;
	EREPORT(("unix_cmd() i %d filled %4d\n", cmd_index, work->filled));
	pipe = popen(cmds[cmd_index++], "r");	/* execute the command */

	while ((n = fread(buffer, sizeof(char), sizeof(buffer), pipe)) > 0) {
		cnt += n;	/* process the output */
		if (my_digest(work, buffer, n))
			break;
		/* this adds some randomness to the output */
		cnt += do_time(work);
	}
	while ((n = fread(buffer, sizeof(char), sizeof(buffer), pipe)) > 0)
		(void)NULL; /* drain the pipe */
	pclose(pipe);
	return (cnt);		/* read how many bytes where read in */
}

/* 
 * digest_file() This function will read a file and run hash over it
 * input is a file name 
 */ 
static int 
digest_file(dst_work *work) 
{
	static int f_cnt = 0;
	static unsigned long f_round = 0;
	FILE *fp; 
	void *ctx;
	const char *name;
	int no, i; 
	struct stat st;
	struct timeval tv;
	u_char buf[1024];

	if (f_round == 0 || files[f_cnt] == NULL || work->file_digest == NULL) 
		if (gettimeofday(&tv, NULL)) /* only do this if needed */
			return (0);
	if (f_round == 0)   /* first time called set to one hour ago */
		f_round = (tv.tv_sec - MAX_OLD); 
	name = files[f_cnt++]; 
	if (files[f_cnt] == NULL) {  /* end of list of files */
		if(f_cnt <= 1)       /* list is too short */
			return (0);
		f_cnt = 0;           /* start again on list */
		f_round += (tv.tv_sec - f_round)/2; /* set new cutoff */
		work->file_digest = dst_free_key(work->file_digest);
	}
	if (work->file_digest == NULL) {
		work->file_digest  = dst_buffer_to_key("", KEY_HMAC_MD5, 0, 0, 
					    (u_char *)&tv, sizeof(tv));
		if (work->file_digest == NULL)
			return (0);
	}
	if (access(name, R_OK) || stat(name, &st))
		return (0); /* no such file or not allowed to read it */
	if (strncmp(name, "/proc/", 6) && st.st_mtime < (time_t)f_round)  
		return(0); /* file has not changed recently enough */
	if (dst_sign_data(SIG_MODE_INIT, work->file_digest, &ctx, 
			  NULL, 0, NULL, 0)) {
		work->file_digest = dst_free_key(work->file_digest);
		return (0);
	}
	if ((fp = fopen(name, "r")) == NULL) 
		return (0);
	for (no = 0; (i = fread(buf, sizeof(*buf), sizeof(buf), fp)) > 0; 
	     no += i) 
		dst_sign_data(SIG_MODE_UPDATE, work->file_digest, &ctx, 
			      buf, i, NULL, 0);

	fclose(fp);
	if (no >= 64) {
		i = dst_sign_data(SIG_MODE_FINAL, work->file_digest, &ctx, 
				  NULL, 0, &work->output[work->filled], 
				  DST_HASH_SIZE);	  
		if (i > 0) 
			work->filled += i;
	}
	else if (i > 0)
		my_digest(work, buf, i);
	my_digest(work, (const u_char *)name, strlen(name));
	return (no + strlen(name));
}

/* 
 * function to perform the FINAL and INIT operation on a hash if allowed
 */
static void
force_hash(dst_work *work, prand_hash *hash)
{
	int i = 0;

	/* 
	 * if more than half a block then add data to output 
	 * otherwise adde the digest to the next hash 
	 */
	if ((hash->digested * 2) > hash->block) {
		i = dst_sign_data(SIG_MODE_FINAL, hash->key, &hash->ctx,
				  NULL, 0, &work->output[work->filled],
				  DST_HASH_SIZE);

		hash->digested = 0;
		dst_sign_data(SIG_MODE_INIT, hash->key, &hash->ctx, 
			      NULL, 0, NULL, 0);
		if (i > 0)
			work->filled += i;
	}
	return;
}

/* 
 * This function takes the input data does the selection of data specified
 * by the hash control block.
 * The step varialbe in the work sturcture determines which 1/step bytes
 * are used, 
 *
 */
static int
do_hash(dst_work *work, prand_hash *hash, const u_char *input, int size)
{
	const u_char *tmp = input;
	u_char *save = NULL, *tp;
	int i, cnt = size, n, needed, avail, dig, tmp_size = 0;

	if (cnt <= 0 || input == NULL)
		return (0);

	if (hash->step > 1) {	/* if using subset of input data */
		tmp_size = size / hash->step + 2;
		tmp = tp = save = malloc(tmp_size);
		for (cnt = 0, i = hash->curr; i < size; i += hash->step, cnt++)
			*(tp++) = input[i];
		/* calcutate the starting point in the next input set */
		hash->curr = (hash->step - (i - size)) % hash->step;
	}
	/* digest the data in block sizes */
	for (n = 0; n < cnt; n += needed) {
		avail = (cnt - n);
		needed = hash->block - hash->digested;
		dig = (avail < needed) ? avail : needed;
		dst_sign_data(SIG_MODE_UPDATE, hash->key, &hash->ctx, 
			      &tmp[n], dig, NULL, 0);
		hash->digested += dig;
		if (hash->digested >= hash->block)
			force_hash(work, hash);
		if (work->needed < work->filled) {
			if (tmp_size > 0) 
				SAFE_FREE2(save, tmp_size);
			return (1);
		}
	}
	if (tmp_size > 0)
		SAFE_FREE2(save, tmp_size);
	return (0);
}

/*
 * Copy data from INPUT for length SIZE into the work-block TMP.
 * If we fill the work-block, digest it; then,
 * if work-block needs more data, keep filling with the rest of the input.
 */
static int
my_digest(dst_work *work, const u_char *input, int size)
{

	int i, full = 0;
	static unsigned counter;
	
	counter += size;
	/* first do each one of the hashes */
	for (i = 0; i < DST_NUM_HASHES && full == 0; i++) 
		full = do_hash(work, work->hash[i], input, size) +
		       do_hash(work, work->hash[i], (u_char *) &counter, 
				sizeof(counter));
/* 
 * if enough data has be generated do final operation on all hashes 
 *  that have enough date for that 
 */
	for (i = 0; full && (i < DST_NUM_HASHES); i++)
		force_hash(work, work->hash[i]);

	return (full);
}

/*
 * this function gets some semi random data and sets that as an HMAC key
 * If we get a valid key this function returns that key initalized
 * otherwise it returns NULL;
 */
static prand_hash *
get_hmac_key(int step, int block)
{

	u_char *buff;
	int temp = 0, n = 0, size = 70;
	DST_KEY *new_key = NULL;
	prand_hash *new = NULL;

	/* use key that is larger than  digest algorithms (64) for key size */
	buff = malloc(size);
	if (buff == NULL)
		return (NULL);
	/* do not memset the allocated memory to get random bytes there */
	/* time of day is somewhat random  expecialy in the last bytes */
	gettimeofday((struct timeval *) &buff[n], NULL);
	n += sizeof(struct timeval);

/* get some semi random stuff in here stir it with micro seconds */
	if (n < size) {
		temp = dst_s_quick_random((int) buff[n - 1]);
		memcpy(&buff[n], &temp, sizeof(temp));
		n += sizeof(temp);
	}
/* get the pid of this process and its parent */
	if (n < size) {
		temp = (int) getpid();
		memcpy(&buff[n], &temp, sizeof(temp));
		n += sizeof(temp);
	}
	if (n < size) {
		temp = (int) getppid();
		memcpy(&buff[n], &temp, sizeof(temp));
		n += sizeof(temp);
	}
/* get the user ID */
	if (n < size) {
		temp = (int) getuid();
		memcpy(&buff[n], &temp, sizeof(temp));
		n += sizeof(temp);
	}
#ifndef GET_HOST_ID_MISSING
	if (n < size) {
		temp = (int) gethostid();
		memcpy(&buff[n], &temp, sizeof(temp));
		n += sizeof(temp);
	}
#endif
/* get some more random data */
	if (n < size) {
		temp = dst_s_quick_random((int) buff[n - 1]);
		memcpy(&buff[n], &temp, sizeof(temp));
		n += sizeof(temp);
	}
/* covert this into a HMAC key */
	new_key = dst_buffer_to_key("", KEY_HMAC_MD5, 0, 0, buff, size);
	SAFE_FREE(buff);

/* get the control structure */
	if ((new = malloc(sizeof(prand_hash))) == NULL)
		return (NULL);
	new->digested = new->curr = 0;
	new->step = step;
	new->block = block;
	new->key = new_key;
	if (dst_sign_data(SIG_MODE_INIT, new_key, &new->ctx, NULL, 0, NULL, 0))
		return (NULL);

	return (new);
}

/* 
 * own_random() 
 * This function goes out and from various sources tries to generate enough
 * semi random data that a hash function can generate a random data. 
 * This function will iterate between the two main random source sources, 
 *  information from programs and directores in random order. 
 * This function return the number of bytes added to the random output buffer. 
 */
static int
own_random(dst_work *work)
{
	int dir = 0, b;
	int bytes, n, cmd = 0, dig = 0;
	int start =0;
/* 
 * now get the initial seed to put into the quick random function from 
 * the address of the work structure 
 */
	bytes = (int) getpid();
/*
 * proceed while needed 
 */
	while (work->filled < work->needed) {
		EREPORT(("own_random r %08x b %6d f %6d\n",
			 ran_val, bytes, work->filled));
/* pick a random number in the range of 0..7 based on that random number
 * perform some operations that yield random data
 */
		start = work->filled;
		n = (dst_s_quick_random(bytes) >> DST_SHIFT) & 0x07;
		switch (n) {
		    case 0:
		    case 3:
			if (sizeof(cmds) > 2 *sizeof(*cmds)) {
				b = unix_cmd(work);
				cmd += b;
			}
			break;

		    case 1:
		    case 7:
			if (sizeof(dirs) > 2 *sizeof(*dirs)) {
				b = do_ls(work);
				dir += b;
			}
			break;

		    case 4:
		    case 5:
			/* retry getting data from /dev/random */
			b = get_dev_random(&work->output[work->filled], 
					   work->needed - work->filled);
			if (b > 0)
				work->filled += b;
			break;

		    case 6:
			if (sizeof(files) > 2 * sizeof(*files)) {
				b = digest_file(work);
				dig += b;
			}
			break;

		    case 2:
		    default:	/* to make sure we make some progress */
			work->output[work->filled++] = 0xff &
				dst_s_quick_random(bytes);
			b = 1;
			break;
		}
		if (b > 0) 
			bytes += b;
	}
	return (work->filled);
}


/* 
 * dst_s_random() This function will return the requested number of bytes 
 * of randomness to the caller it will use the best available sources of 
 * randomness.
 * The current order is to use /dev/random, precalculated randomness, and 
 * finaly use some system calls and programs to generate semi random data that 
 * is then digested to generate randomness. 
 * This function is thread safe as each thread uses its own context, but
 * concurrent treads will affect each other as they update shared state 
 * information.
 * It is strongly recommended that this function be called requesting a size 
 * that is not a multiple of the output of the hash function used. 
 * 
 * If /dev/random is not available this function is not suitable to generate 
 * large ammounts of data, rather it is suitable to seed a pseudo-random 
 * generator 
 * Returns the number of bytes put in the output buffer 
 */
int
dst_s_random(u_char *output, int size)
{
	int n = 0, s, i;
	static u_char old_unused[DST_HASH_SIZE * DST_NUM_HASHES];
	static int unused = 0;

	if (size <= 0 || output == NULL)
		return (0);

	if (size >= 2048)
		return (-1);
	/* 
	 * Read from /dev/random 
	 */
	n = get_dev_random(output, size);
	/* 
	 *  If old data is available and needed use it 
	 */
	if (n < size && unused > 0) {
		int need = size - n;
		if (unused <= need) {
			memcpy(output, old_unused, unused);
			n += unused;
			unused = 0;
		} else {
			memcpy(output, old_unused, need);
			n += need;
			unused -= need;
			memcpy(old_unused, &old_unused[need], unused);
		}
	}
	/*
	 * If we need more use the simulated randomness here.
	 */
	if (n < size) {
		dst_work *my_work = (dst_work *) malloc(sizeof(dst_work));
		if (my_work == NULL)
			return (n);
		my_work->needed = size - n;
		my_work->filled = 0;
		my_work->output = (u_char *) malloc(my_work->needed +
						    DST_HASH_SIZE *
						    DST_NUM_HASHES);
		my_work->file_digest = NULL;
		if (my_work->output == NULL)
			return (n);
		memset(my_work->output, 0x0, my_work->needed);
/* allocate upto 4 different HMAC hash functions out of order */
#if DST_NUM_HASHES >= 3
		my_work->hash[2] = get_hmac_key(3, DST_RANDOM_BLOCK_SIZE / 2);
#endif
#if DST_NUM_HASHES >= 2
		my_work->hash[1] = get_hmac_key(7, DST_RANDOM_BLOCK_SIZE / 6);
#endif
#if DST_NUM_HASHES >= 4
		my_work->hash[3] = get_hmac_key(5, DST_RANDOM_BLOCK_SIZE / 4);
#endif
		my_work->hash[0] = get_hmac_key(1, DST_RANDOM_BLOCK_SIZE);
		if (my_work->hash[0] == NULL)	/* if failure bail out */
			return (n);
		s = own_random(my_work);
/* if more generated than needed store it for future use */
		if (s >= my_work->needed) {
			EREPORT(("dst_s_random(): More than needed %d >= %d\n",
				 s, my_work->needed));
			memcpy(&output[n], my_work->output, my_work->needed);
			n += my_work->needed;
			/* saving unused data for next time */
			unused = s - my_work->needed;
			memcpy(old_unused, &my_work->output[my_work->needed],
			       unused);
		} else {
			/* XXXX This should not happen */
			EREPORT(("Not enough %d >= %d\n", s, my_work->needed));
			memcpy(&output[n], my_work->output, s);
			n += my_work->needed;
		}

/* delete the allocated work area */
		for (i = 0; i < DST_NUM_HASHES; i++) {
			dst_free_key(my_work->hash[i]->key);
			SAFE_FREE(my_work->hash[i]);
		}
		SAFE_FREE(my_work->output);
		SAFE_FREE(my_work);
	}
	return (n);
}

/*
 * A random number generator that is fast and strong 
 * this random number generator is based on HASHing data,
 * the input to the digest function is a collection of <NUMBER_OF_COUNTERS>
 * counters that is incremented between digest operations
 * each increment operation amortizes to 2 bits changed in that value
 * for 5 counters thus the input will amortize to have 10 bits changed 
 * The counters are initaly set using the strong random function above
 * the HMAC key is selected by the same methold as the HMAC keys for the 
 * strong random function. 
 * Each set of counters is used for 2^25 operations 
 * 
 * returns the number of bytes written to the output buffer 
 * or       negative number in case of error 
 */
int
dst_s_semi_random(u_char *output, int size)
{
	static u_int32_t counter[DST_NUMBER_OF_COUNTERS];
	static u_char semi_old[DST_HASH_SIZE];
	static int semi_loc = 0, cnt = 0, hb_size = 0;
	static DST_KEY *my_key = NULL;
	prand_hash *hash;
	int out = 0, i, n;

	if (output == NULL || size <= 0)
		return (-2);

/* check if we need a new key */
	if (my_key == NULL || cnt > (1 << 25)) {	/* get HMAC KEY */
		if (my_key)
			my_key->dk_func->destroy(my_key);
		if ((hash = get_hmac_key(1, DST_RANDOM_BLOCK_SIZE)) == NULL)
			return (0);
		my_key = hash->key;
/* check if the key works stir the new key using some old random data */
		hb_size = dst_sign_data(SIG_MODE_ALL, my_key, NULL, 
				        (u_char *) counter, sizeof(counter),
					semi_old, sizeof(semi_old));
		if (hb_size <= 0) {
			EREPORT(("dst_s_semi_random() Sign of alg %d failed %d\n",
				 my_key->dk_alg, hb_size));
			return (-1);
		}
/* new set the counters to random values */
		dst_s_random((u_char *) counter, sizeof(counter));
		cnt = 0;
	}
/* if old data around use it first */
	if (semi_loc < hb_size) {
		if (size <= hb_size - semi_loc) {	/* need less */
			memcpy(output, &semi_old[semi_loc], size);
			semi_loc += size;
			return (size);	/* DONE */
		} else {
			out = hb_size - semi_loc;
			memcpy(output, &semi_old[semi_loc], out);
			semi_loc += out;
		}
	}
/* generate more randome stuff */
	while (out < size) {
		/* 
		 * modify at least one bit by incrementing at least one counter
		 * based on the last bit of the last counter updated update
		 * the next one.
		 * minimaly this  operation will modify at least 1 bit, 
		 * amortized 2 bits
		 */
		for (n = 0; n < DST_NUMBER_OF_COUNTERS; n++)
			i = (int) counter[n]++;

		i = dst_sign_data(SIG_MODE_ALL, my_key, NULL, 
				  (u_char *) counter, hb_size,
				  semi_old, sizeof(semi_old));
#ifdef REPORT_ERRORS
		if (i != hb_size)
			EREPORT(("HMAC SIGNATURE FAILURE %d\n", i));
#endif
		cnt++;
		if (size - out < i)	/* Not all data is needed */
			semi_loc = i = size - out;
		memcpy(&output[out], semi_old, i);
		out += i;
	}
	return (out);
}

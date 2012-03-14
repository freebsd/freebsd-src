/*-
 * Copyright 2000 Hans Reiser
 * See README for licensing and copyright details
 * 
 * Ported to FreeBSD by Jean-Sébastien Pédron <jspedron@club-internet.fr>
 * 
 * $FreeBSD$
 */

#include <gnu/fs/reiserfs/reiserfs_fs.h>

#if 0
static char error_buf[1024];
static char fmt_buf[1024];
static char off_buf[80];

static char *
reiserfs_cpu_offset(struct cpu_key *key)
{

	if (cpu_key_k_type(key) == TYPE_DIRENTRY)
		sprintf(off_buf, "%Lu(%Lu)",
		    (unsigned long long)GET_HASH_VALUE(cpu_key_k_offset(key)),
		    (unsigned long long)GET_GENERATION_NUMBER(
			cpu_key_k_offset(key)));
	else
		sprintf(off_buf, "0x%Lx",
		    (unsigned long long)cpu_key_k_offset(key));

	return (off_buf);
}

static char *
le_offset(struct key *key)
{
	int version;

	version = le_key_version(key);
	if (le_key_k_type(version, key) == TYPE_DIRENTRY)
		sprintf(off_buf, "%Lu(%Lu)",
		    (unsigned long long)GET_HASH_VALUE(
			le_key_k_offset(version, key)),
		    (unsigned long long)GET_GENERATION_NUMBER(
			le_key_k_offset(version, key)));
	else
		sprintf(off_buf, "0x%Lx",
		    (unsigned long long)le_key_k_offset(version, key));

	return (off_buf);
}

static char *
cpu_type(struct cpu_key *key)
{

	if (cpu_key_k_type(key) == TYPE_STAT_DATA)
		return ("SD");
	if (cpu_key_k_type(key) == TYPE_DIRENTRY)
		return ("DIR");
	if (cpu_key_k_type(key) == TYPE_DIRECT)
		return ("DIRECT");
	if (cpu_key_k_type(key) == TYPE_INDIRECT)
		return ("IND");

	return ("UNKNOWN");
}

static char *
le_type(struct key *key)
{
	int version;

	version = le_key_version(key);

	if (le_key_k_type(version, key) == TYPE_STAT_DATA)
		return ("SD");
	if (le_key_k_type(version, key) == TYPE_DIRENTRY)
		return ("DIR");
	if (le_key_k_type(version, key) == TYPE_DIRECT)
		return ("DIRECT");
	if (le_key_k_type(version, key) == TYPE_INDIRECT)
		return ("IND");

	return ("UNKNOWN");
}

/* %k */
static void
sprintf_le_key(char *buf, struct key *key)
{

	if (key)
		sprintf(buf, "[%d %d %s %s]", le32toh(key->k_dir_id),
		    le32toh(key->k_objectid), le_offset(key), le_type(key));
	else
		sprintf(buf, "[NULL]");
}

/* %K */
static void
sprintf_cpu_key(char *buf, struct cpu_key *key)
{

	if (key)
		sprintf(buf, "[%d %d %s %s]", key->on_disk_key.k_dir_id,
		    key->on_disk_key.k_objectid, reiserfs_cpu_offset (key),
		    cpu_type (key));
	else
		sprintf(buf, "[NULL]");
}

static void sprintf_de_head(char *buf, struct reiserfs_de_head *deh)
{

	if (deh)
		sprintf(buf,
		    "[offset=%d dir_id=%d objectid=%d location=%d state=%04x]",
		    deh_offset(deh), deh_dir_id(deh),
		    deh_objectid(deh), deh_location(deh), deh_state(deh));
	else
		sprintf(buf, "[NULL]");
}

static void
sprintf_item_head(char *buf, struct item_head *ih)
{

	if (ih) {
		strcpy(buf, (ih_version(ih) == KEY_FORMAT_3_6) ?
		    "*3.6* " : "*3.5*");
		sprintf_le_key(buf + strlen(buf), &(ih->ih_key));
		sprintf(buf + strlen(buf), ", item_len %d, item_location %d, "
		    "free_space(entry_count) %d",
		    ih_item_len(ih), ih_location(ih), ih_free_space(ih));
	} else
		sprintf(buf, "[NULL]");
}

static void
sprintf_direntry(char *buf, struct reiserfs_dir_entry *de)
{
	char name[20];

	memcpy(name, de->de_name, de->de_namelen > 19 ? 19 : de->de_namelen);
	name [de->de_namelen > 19 ? 19 : de->de_namelen] = 0;
	sprintf(buf, "\"%s\" ==> [%d %d]",
	    name, de->de_dir_id, de->de_objectid);
}

static void
sprintf_block_head(char *buf, struct buf *bp)
{

	sprintf(buf, "level=%d, nr_items=%d, free_space=%d rdkey ",
	    B_LEVEL(bp), B_NR_ITEMS(bp), B_FREE_SPACE(bp));
}

static void
sprintf_disk_child(char *buf, struct disk_child *dc)
{

	sprintf (buf, "[dc_number=%d, dc_size=%u]",
	    dc_block_number(dc), dc_size(dc));
}

static char *
is_there_reiserfs_struct (char *fmt, int *what, int *skip)
{
	char *k;

	k = fmt;
	*skip = 0;

	while ((k = strchr(k, '%')) != NULL) {
		if (k[1] == 'k' || k[1] == 'K' || k[1] == 'h' || k[1] == 't' ||
		    k[1] == 'z' || k[1] == 'b' || k[1] == 'y' || k[1] == 'a' ) {
			*what = k[1];
			break;
		}
		(*skip)++;
		k++;
	}

	return (k);
}

static void
prepare_error_buf(const char *fmt, va_list args)
{
	char *fmt1, *k, *p;
	int i, j, what, skip;

	fmt1 = fmt_buf;
	p = error_buf;
	strcpy (fmt1, fmt);

	while ((k = is_there_reiserfs_struct(fmt1, &what, &skip)) != NULL) {
		*k = 0;

		p += vsprintf (p, fmt1, args);

		for (i = 0; i < skip; i ++)
			j = va_arg(args, int);

		switch (what) {
		case 'k':
			sprintf_le_key(p, va_arg(args, struct key *));
			break;
		case 'K':
			sprintf_cpu_key(p, va_arg(args, struct cpu_key *));
			break;
		case 'h':
			sprintf_item_head(p, va_arg(args, struct item_head *));
			break;
		case 't':
			sprintf_direntry(p,
			    va_arg(args, struct reiserfs_dir_entry *));
			break;
		case 'y':
			sprintf_disk_child(p,
			    va_arg(args, struct disk_child *));
			break;
		case 'z':
			sprintf_block_head(p,
			    va_arg(args, struct buffer_head *));
			break;
		case 'a':
			sprintf_de_head(p,
			    va_arg(args, struct reiserfs_de_head *));
			break;
		}

		p   += strlen(p);
		fmt1 = k + 2;
	}

	vsprintf(p, fmt1, args);
}

/*
 * In addition to usual conversion specifiers this accepts reiserfs
 * specific conversion specifiers: 
 *   %k to print little endian key, 
 *   %K to print cpu key, 
 *   %h to print item_head,
 *   %t to print directory entry,
 *   %z to print block head (arg must be struct buf *)
 */

#define do_reiserfs_warning(fmt)					\
{									\
	va_list args;							\
	va_start(args, fmt);						\
	prepare_error_buf(fmt, args);					\
	va_end(args);							\
}

void
__reiserfs_log(int level, const char * fmt, ...)
{

	do_reiserfs_warning(fmt);
	log(level, "ReiserFS/%s: %s\n", __FUNCTION__, error_buf);
}

#endif

char *
reiserfs_hashname(int code)
{

	if (code == YURA_HASH)
		return ("rupasov");
	if (code == TEA_HASH)
		return ("tea");
	if (code == R5_HASH)
		return ("r5");

	return ("unknown");
}

void
reiserfs_dump_buffer(caddr_t buf, off_t len)
{
	int i, j;

	log(LOG_DEBUG, "reiserfs: dumping a buffer of %jd bytes\n",
	    (intmax_t)len);
	for (i = 0; i < len; i += 16) {
		log(LOG_DEBUG, "%08x: ", i);
		for (j = 0; j < 16; j += 2) {
			if (i + j >= len)
				log(LOG_DEBUG, "     ");
			else
				log(LOG_DEBUG, "%02x%02x ",
				    buf[i + j] & 0xff,
				    buf[i + j + 1] & 0xff);
		}
		for (j = 0; j < 16; ++j) {
			if (i + j >= len)
				break;
			log(LOG_DEBUG, "%c",
			    isprint(buf[i + j]) ? buf[i + j] : '.');
		}
		log(LOG_DEBUG, "\n");
	}
}

/*
 * linux/net/sunrpc/sysctl.c
 *
 * Sysctl interface to sunrpc module. This is for debugging only now.
 *
 * I would prefer to register the sunrpc table below sys/net, but that's
 * impossible at the moment.
 */

#include <linux/config.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/linkage.h>
#include <linux/ctype.h>
#include <linux/fs.h>
#include <linux/sysctl.h>
#include <linux/module.h>

#include <asm/uaccess.h>
#include <linux/sunrpc/types.h>
#include <linux/sunrpc/sched.h>
#include <linux/sunrpc/stats.h>

/*
 * Declare the debug flags here
 */
unsigned int	rpc_debug;
unsigned int	nfs_debug;
unsigned int	nfsd_debug;
unsigned int	nlm_debug;

#ifdef RPC_DEBUG

static struct ctl_table_header *sunrpc_table_header;
static ctl_table		sunrpc_table[];

void
rpc_register_sysctl(void)
{
	if (!sunrpc_table_header) {
		sunrpc_table_header = register_sysctl_table(sunrpc_table, 1);
#ifdef CONFIG_PROC_FS
		if (sunrpc_table[0].de)
			sunrpc_table[0].de->owner = THIS_MODULE;
#endif
	}
			
}

void
rpc_unregister_sysctl(void)
{
	if (sunrpc_table_header) {
		unregister_sysctl_table(sunrpc_table_header);
		sunrpc_table_header = NULL;
	}
}

static int
proc_dodebug(ctl_table *table, int write, struct file *file,
				void *buffer, size_t *lenp)
{
	char		tmpbuf[20], *p, c;
	unsigned int	value;
	size_t		left, len;

	if ((file->f_pos && !write) || !*lenp) {
		*lenp = 0;
		return 0;
	}

	left = *lenp;

	if (write) {
		if (!access_ok(VERIFY_READ, buffer, left))
			return -EFAULT;
		p = (char *) buffer;
		while (left && __get_user(c, p) >= 0 && isspace(c))
			left--, p++;
		if (!left)
			goto done;

		if (left > sizeof(tmpbuf) - 1)
			return -EINVAL;
		copy_from_user(tmpbuf, p, left);
		tmpbuf[left] = '\0';

		for (p = tmpbuf, value = 0; '0' <= *p && *p <= '9'; p++, left--)
			value = 10 * value + (*p - '0');
		if (*p && !isspace(*p))
			return -EINVAL;
		while (left && isspace(*p))
			left--, p++;
		*(unsigned int *) table->data = value;
		/* Display the RPC tasks on writing to rpc_debug */
		if (table->ctl_name == CTL_RPCDEBUG) {
			rpc_show_tasks();
		}
	} else {
		if (!access_ok(VERIFY_WRITE, buffer, left))
			return -EFAULT;
		len = sprintf(tmpbuf, "%d", *(unsigned int *) table->data);
		if (len > left)
			len = left;
		copy_to_user(buffer, tmpbuf, len);
		if ((left -= len) > 0) {
			put_user('\n', (char *)buffer + len);
			left--;
		}
	}

done:
	*lenp -= left;
	file->f_pos += *lenp;
	return 0;
}

#define DIRENTRY(nam1, nam2, child)	\
	{CTL_##nam1, #nam2, NULL, 0, 0555, child }
#define DBGENTRY(nam1, nam2)	\
	{CTL_##nam1##DEBUG, #nam2 "_debug", &nam2##_debug, sizeof(int),\
	 0644, NULL, &proc_dodebug}

static ctl_table		debug_table[] = {
	DBGENTRY(RPC,  rpc),
	DBGENTRY(NFS,  nfs),
	DBGENTRY(NFSD, nfsd),
	DBGENTRY(NLM,  nlm),
	{0}
};

static ctl_table		sunrpc_table[] = {
	DIRENTRY(SUNRPC, sunrpc, debug_table),
	{0}
};

#endif

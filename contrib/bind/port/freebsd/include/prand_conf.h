#ifndef _PRAND_CMD_H_
#define _PRAND_CMD_H_


#ifndef HAVE_DEV_RANDOM
# define HAVE_DEV_RANDOM 1
#endif /* HAVE_DEV_RANDOM */

static const char *cmds[] = {
	"/bin/ps -axlw 2>&1",
	"/usr/sbin/arp -an 2>&1",
	"/usr/bin/netstat -an 2>&1",
	"/bin/df  2>&1",
	"/usr/bin/dig com. soa +ti=1 +retry=0 2>&1",
	"/usr/bin/netstat -an 2>&1",
	"/usr/bin/dig . soa +ti=1 +retry=0 2>&1",
	"/usr/sbin/iostat  2>&1",
	"/usr/bin/vmstat  2>&1",
	"/usr/bin/w  2>&1",
	NULL
};

static const char *dirs[] = {
	"/tmp",
	"/usr/tmp",
	".",
	"/",
	"/var/spool",
	"/dev",
	"/var/mail",
	"/home",
	"/usr/home",
	NULL
};

static const char *files[] = {
	"/var/log/messages",
	"/var/log/wtmp",
	"/var/log/lastlog",
	NULL
};

#endif /* _PRAND_CMD_H_ */

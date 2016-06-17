#ifndef __PRESTO_JOURNAL_H
#define __PRESTO_JOURNAL_H


#include <linux/version.h>

struct journal_prefix {
	int len;
        u32 version;
	int pid;
	int uid;
	int fsuid;
	int fsgid;
	int opcode;
        u32 ngroups;
        u32 groups[0];
};

struct journal_suffix {
	unsigned long prevrec;  /* offset of previous record for dentry */
	int recno;
	int time;
	int len;
};

#endif

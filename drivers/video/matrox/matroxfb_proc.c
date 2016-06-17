/*
 *
 * Hardware accelerated Matrox Millennium I, II, Mystique, G100, G200, G400 and G450.
 *
 * (c) 1998-2002 Petr Vandrovec <vandrove@vc.cvut.cz>
 *
 * Version: 1.64 2002/06/10
 *
 */

#include "matroxfb_base.h"
#include <linux/proc_fs.h>

static struct proc_dir_entry* mga_pde;

struct procinfo {
	struct matrox_fb_info* info;
	struct proc_dir_entry* pde;
};

static inline void remove_pde(struct proc_dir_entry* pde) {
	if (pde) {
		remove_proc_entry(pde->name, pde->parent);
	}
}

#ifndef CONFIG_PROC_FS
static int bios_read_proc(char* buffer, char** start, off_t offset,
			  int size, int *eof, void *data) {
	return 0;
}

static int pins_read_proc(char* buffer, char** start, off_t offset,
			  int size, int *eof, void *data) {
	return 0;
}
#else
/* This macro frees the machine specific function from bounds checking and
 * this like that... */
#define PRINT_PROC(fmt,args...)					\
	do {							\
		len += sprintf(buffer+len, fmt, ##args );	\
		if (begin + len > offset + size)		\
			break;					\
		if (begin + len < offset) {			\
			begin += len;				\
			len = 0;				\
		}						\
	} while(0)

static int bios_read_proc(char* buffer, char** start, off_t offset,
			  int size, int *eof, void *data) {
	int len = 0;
	off_t begin = 0;
	struct matrox_bios* bd = data;

	do {
		*eof = 0;
		if (bd->bios_valid) {
			PRINT_PROC("BIOS:   %u.%u.%u\n", bd->version.vMaj, bd->version.vMin, bd->version.vRev);
			PRINT_PROC("Output: 0x%02X\n", bd->output.state);
			PRINT_PROC("TVOut:  %s\n", bd->output.tvout?"yes":"no");
			PRINT_PROC("PINS:   %s\n", bd->pins_len ? "found" : "not found");
			PRINT_PROC("Info:   %p\n", bd);
		} else {
			PRINT_PROC("BIOS:   Invalid\n");
		}
		*eof = 1;
	} while (0);
	if (offset >= begin + len)
		return 0;
	*start = buffer + (offset - begin);
	return size < begin + len - offset ? size : begin + len - offset;
}

static int pins_read_proc(char* buffer, char** start, off_t offset,
			  int size, int *eof, void *data) {
	struct matrox_bios* bd = data;
	
	if (offset >= bd->pins_len) {
		*eof = 1;
		return 0;
	}
	if (offset + size >= bd->pins_len) {
		size = bd->pins_len - offset;
		*eof = 1;
	}
	memcpy(buffer, bd->pins + offset, size);
	*start = buffer;
	return size;
}
#endif /* CONFIG_PROC_FS */

static void* matroxfb_proc_probe(struct matrox_fb_info* minfo) {
	struct procinfo* binfo;
	char b[10];

	binfo = (struct procinfo*)kmalloc(sizeof(*binfo), GFP_KERNEL);
	if (!binfo) {
		printk(KERN_ERR "matroxfb_proc: Not enough memory for /proc control structs\n");
		return NULL;
	}
	binfo->info = minfo;
	sprintf(b, "fb%u", GET_FB_IDX(minfo->fbcon.node));
	binfo->pde = proc_mkdir(b, mga_pde);
	if (binfo->pde) {
		create_proc_read_entry("bios", 0, binfo->pde, bios_read_proc, &minfo->bios);
		if (minfo->bios.pins_len) {
			struct proc_dir_entry* p = create_proc_read_entry("pins", 0, binfo->pde, pins_read_proc, &minfo->bios);
			if (p) {
				p->size = minfo->bios.pins_len;
			}
		}
	}
	return binfo;
}

static void matroxfb_proc_remove(struct matrox_fb_info* minfo, void* binfoI) {
	struct procinfo* binfo = binfoI;

	if (binfo->pde) {
		remove_proc_entry("pins", binfo->pde);
		remove_proc_entry("bios", binfo->pde);
		remove_pde(binfo->pde);
	}
	kfree(binfo);
}

static struct matroxfb_driver procfn = {
	.name =		"Matrox /proc driver",
	.probe =	matroxfb_proc_probe,
	.remove =	matroxfb_proc_remove
};

static int matroxfb_proc_init(void) {
	mga_pde = proc_mkdir("driver/mga", NULL);
	matroxfb_register_driver(&procfn);
	return 0;
}

static void matroxfb_proc_exit(void) {
	matroxfb_unregister_driver(&procfn);
	remove_pde(mga_pde);
}

MODULE_AUTHOR("(c) 2001-2002 Petr Vandrovec <vandrove@vc.cvut.cz>");
MODULE_DESCRIPTION("Matrox /proc driver");
MODULE_LICENSE("GPL");
module_init(matroxfb_proc_init);
module_exit(matroxfb_proc_exit);
/* we do not have __setup() */

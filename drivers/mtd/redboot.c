/*
 * $Id: redboot.c,v 1.6 2001/10/25 09:16:06 dwmw2 Exp $
 *
 * Parse RedBoot-style Flash Image System (FIS) tables and
 * produce a Linux partition array to match.
 */

#include <linux/kernel.h>
#include <linux/slab.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>

struct fis_image_desc {
    unsigned char name[16];      // Null terminated name
    unsigned long flash_base;    // Address within FLASH of image
    unsigned long mem_base;      // Address in memory where it executes
    unsigned long size;          // Length of image
    unsigned long entry_point;   // Execution entry point
    unsigned long data_length;   // Length of actual data
    unsigned char _pad[256-(16+7*sizeof(unsigned long))];
    unsigned long desc_cksum;    // Checksum over image descriptor
    unsigned long file_cksum;    // Checksum over image data
};

struct fis_list {
	struct fis_image_desc *img;
	struct fis_list *next;
};

static inline int redboot_checksum(struct fis_image_desc *img)
{
	/* RedBoot doesn't actually write the desc_cksum field yet AFAICT */
	return 1;
}

int parse_redboot_partitions(struct mtd_info *master, struct mtd_partition **pparts)
{
	int nrparts = 0;
	struct fis_image_desc *buf;
	struct mtd_partition *parts;
	struct fis_list *fl = NULL, *tmp_fl;
	int ret, i;
	size_t retlen;
	char *names;
	int namelen = 0;

	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);

	if (!buf)
		return -ENOMEM;

	/* Read the start of the last erase block */
	ret = master->read(master, master->size - master->erasesize,
			   PAGE_SIZE, &retlen, (void *)buf);

	if (ret)
		goto out;

	if (retlen != PAGE_SIZE) {
		ret = -EIO;
		goto out;
	}

	/* RedBoot image could appear in any of the first three slots */
	for (i = 0; i < 3; i++) {
		if (!memcmp(buf[i].name, "RedBoot", 8))
			break;
	}
	if (i == 3) {
		/* Didn't find it */
		printk(KERN_NOTICE "No RedBoot partition table detected in %s\n",
		       master->name);
		ret = 0;
		goto out;
	}

	for (i = 0; i < PAGE_SIZE / sizeof(struct fis_image_desc); i++) {
		struct fis_list *new_fl, **prev;

		if (buf[i].name[0] == 0xff)
			break;
		if (!redboot_checksum(&buf[i]))
			break;

		new_fl = kmalloc(sizeof(struct fis_list), GFP_KERNEL);
		namelen += strlen(buf[i].name)+1;
		if (!new_fl) {
			ret = -ENOMEM;
			goto out;
		}
		new_fl->img = &buf[i];
		buf[i].flash_base &= master->size-1;

		/* I'm sure the JFFS2 code has done me permanent damage.
		 * I now think the following is _normal_
		 */
		prev = &fl;
		while(*prev && (*prev)->img->flash_base < new_fl->img->flash_base)
			prev = &(*prev)->next;
		new_fl->next = *prev;
		*prev = new_fl;

		nrparts++;
	}
	if (fl->img->flash_base)
		nrparts++;

	for (tmp_fl = fl; tmp_fl->next; tmp_fl = tmp_fl->next) {
		if (tmp_fl->img->flash_base + tmp_fl->img->size + master->erasesize < tmp_fl->next->img->flash_base)
			nrparts++;
	}
	parts = kmalloc(sizeof(*parts)*nrparts + namelen, GFP_KERNEL);

	if (!parts) {
		ret = -ENOMEM;
		goto out;
	}
	names = (char *)&parts[nrparts];
	memset(parts, 0, sizeof(*parts)*nrparts + namelen);
	i=0;

	if (fl->img->flash_base) {
	       parts[0].name = "unallocated space";
	       parts[0].size = fl->img->flash_base;
	       parts[0].offset = 0;
	}
	for ( ; i<nrparts; i++) {
		parts[i].size = fl->img->size;
		parts[i].offset = fl->img->flash_base;
		parts[i].name = names;

		strcpy(names, fl->img->name);
		names += strlen(names)+1;

		if(fl->next && fl->img->flash_base + fl->img->size + master->erasesize < fl->next->img->flash_base) {
			i++;
			parts[i].offset = parts[i-1].size + parts[i-1].offset;
			parts[i].size = fl->next->img->flash_base - parts[i].offset;
			parts[i].name = "unallocated space";
		}
		tmp_fl = fl;
		fl = fl->next;
		kfree(tmp_fl);
	}
	ret = nrparts;
	*pparts = parts;
 out:
	while (fl) {
		struct fis_list *old = fl;
		fl = fl->next;
		kfree(old);
	}
	kfree(buf);
	return ret;
}

EXPORT_SYMBOL(parse_redboot_partitions);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Red Hat, Inc. - David Woodhouse <dwmw2@cambridge.redhat.com>");
MODULE_DESCRIPTION("Parsing code for RedBoot Flash Image System (FIS) tables");

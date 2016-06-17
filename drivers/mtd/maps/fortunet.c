/* fortunet.c memory map
 *
 * $Id: fortunet.c,v 1.2 2002/10/14 12:50:22 rmk Exp $
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#define MAX_NUM_REGIONS		4
#define MAX_NUM_PARTITIONS	8

#define DEF_WINDOW_ADDR_PHY	0x00000000
#define DEF_WINDOW_SIZE		0x00800000		// 8 Mega Bytes

#define MTD_FORTUNET_PK		"MTD FortuNet: "

#define MAX_NAME_SIZE		128

struct map_region
{
	int			window_addr_phyical;
	int			altbuswidth;
	struct map_info		map_info;
	struct mtd_info		*mymtd;
	struct mtd_partition	parts[MAX_NUM_PARTITIONS];
	char			map_name[MAX_NAME_SIZE];
	char			parts_name[MAX_NUM_PARTITIONS][MAX_NAME_SIZE];
};

static struct map_region	map_regions[MAX_NUM_REGIONS];
static int			map_regions_set[MAX_NUM_REGIONS] = {0,0,0,0};
static int			map_regions_parts[MAX_NUM_REGIONS] = {0,0,0,0};


__u8 fortunet_read8(struct map_info *map, unsigned long ofs)
{
	return *(__u8 *)(map->map_priv_1 + ofs);
}

__u16 fortunet_read16(struct map_info *map, unsigned long ofs)
{
	return *(__u16 *)(map->map_priv_1 + ofs);
}

__u32 fortunet_read32(struct map_info *map, unsigned long ofs)
{
	return *(__u32 *)(map->map_priv_1 + ofs);
}

void fortunet_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy(to, (void *)(map->map_priv_1 + from), len);
}

void fortunet_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	*(__u8 *)(map->map_priv_1 + adr) = d;
}

void fortunet_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	*(__u16 *)(map->map_priv_1 + adr) = d;
}

void fortunet_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	*(__u32 *)(map->map_priv_1 + adr) = d;
}

void fortunet_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy((void *)(map->map_priv_1 + to), from, len);
}

struct map_info default_map = {
	size: DEF_WINDOW_SIZE,
	buswidth: 4,
	read8: fortunet_read8,
	read16: fortunet_read16,
	read32: fortunet_read32,
	copy_from: fortunet_copy_from,
	write8: fortunet_write8,
	write16: fortunet_write16,
	write32: fortunet_write32,
	copy_to: fortunet_copy_to
};

static char * __init get_string_option(char *dest,int dest_size,char *sor)
{
	if(!dest_size)
		return sor;
	dest_size--;
	while(*sor)
	{
		if(*sor==',')
		{
			sor++;
			break;
		}
		else if(*sor=='\"')
		{
			sor++;
			while(*sor)
			{
				if(*sor=='\"')
				{
					sor++;
					break;
				}
				*dest = *sor;
				dest++;
				sor++;
				dest_size--;
				if(!dest_size)
				{
					*dest = 0;
					return sor;
				}
			}
		}
		else
		{
			*dest = *sor;
			dest++;
			sor++;
			dest_size--;
			if(!dest_size)
			{
				*dest = 0;
				return sor;
			}
		}
	}
	*dest = 0;
	return sor;
}

static int __init MTD_New_Region(char *line)
{
	char	string[MAX_NAME_SIZE];
	int	params[6];
	get_options (get_string_option(string,sizeof(string),line),6,params);
	if(params[0]<1)
	{
		printk(MTD_FORTUNET_PK "Bad paramters for MTD Region "
			" name,region-number[,base,size,buswidth,altbuswidth]\n");
		return 1;
	}
	if((params[1]<0)||(params[1]>=MAX_NUM_REGIONS))
	{
		printk(MTD_FORTUNET_PK "Bad region index of %d only have 0..%u regions\n",
			params[1],MAX_NUM_REGIONS-1);
		return 1;
	}
	memset(&map_regions[params[1]],0,sizeof(map_regions[params[1]]));
	memcpy(&map_regions[params[1]].map_info,
		&default_map,sizeof(map_regions[params[1]].map_info));
        map_regions_set[params[1]] = 1;
        map_regions[params[1]].window_addr_phyical = DEF_WINDOW_ADDR_PHY;
        map_regions[params[1]].altbuswidth = 2;
        map_regions[params[1]].mymtd = NULL;
	map_regions[params[1]].map_info.name = map_regions[params[1]].map_name;
	strcpy(map_regions[params[1]].map_info.name,string);
	if(params[0]>1)
	{
		map_regions[params[1]].window_addr_phyical = params[2];
	}
	if(params[0]>2)
	{
		map_regions[params[1]].map_info.size = params[3];
	}
	if(params[0]>3)
	{
		map_regions[params[1]].map_info.buswidth = params[4];
	}
	if(params[0]>4)
	{
		map_regions[params[1]].altbuswidth = params[5];
	}
	return 1;
}

static int __init MTD_New_Partion(char *line)
{
	char	string[MAX_NAME_SIZE];
	int	params[4];
	get_options (get_string_option(string,sizeof(string),line),4,params);
	if(params[0]<3)
	{
		printk(MTD_FORTUNET_PK "Bad paramters for MTD Partion "
			" name,region-number,size,offset\n");
		return 1;
	}
	if((params[1]<0)||(params[1]>=MAX_NUM_REGIONS))
	{
		printk(MTD_FORTUNET_PK "Bad region index of %d only have 0..%u regions\n",
			params[1],MAX_NUM_REGIONS-1);
		return 1;
	}
	if(map_regions_parts[params[1]]>=MAX_NUM_PARTITIONS)
	{
		printk(MTD_FORTUNET_PK "Out of space for partion in this region\n");
		return 1;
	}
	map_regions[params[1]].parts[map_regions_parts[params[1]]].name =
		map_regions[params[1]].	parts_name[map_regions_parts[params[1]]];
	strcpy(map_regions[params[1]].parts[map_regions_parts[params[1]]].name,string);
	map_regions[params[1]].parts[map_regions_parts[params[1]]].size =
		params[2];
	map_regions[params[1]].parts[map_regions_parts[params[1]]].offset =
		params[3];
	map_regions[params[1]].parts[map_regions_parts[params[1]]].mask_flags = 0;
	map_regions_parts[params[1]]++;
	return 1;
}

__setup("MTD_Region=", MTD_New_Region);
__setup("MTD_Partion=", MTD_New_Partion);

int __init init_fortunet(void)
{
	int	ix,iy;
	for(iy=ix=0;ix<MAX_NUM_REGIONS;ix++)
	{
		if(map_regions_parts[ix]&&(!map_regions_set[ix]))
		{
			printk(MTD_FORTUNET_PK "Region %d is not setup (Seting to default)\n",
				ix);
			memset(&map_regions[ix],0,sizeof(map_regions[ix]));
			memcpy(&map_regions[ix].map_info,&default_map,
				sizeof(map_regions[ix].map_info));
			map_regions_set[ix] = 1;
			map_regions[ix].window_addr_phyical = DEF_WINDOW_ADDR_PHY;
			map_regions[ix].altbuswidth = 2;
			map_regions[ix].mymtd = NULL;
			map_regions[ix].map_info.name = map_regions[ix].map_name;
			strcpy(map_regions[ix].map_info.name,"FORTUNET");
		}
		if(map_regions_set[ix])
		{
			iy++;
			printk(KERN_NOTICE MTD_FORTUNET_PK "%s flash device at phyicaly "
				" address %x size %x\n",
				map_regions[ix].map_info.name,
				map_regions[ix].window_addr_phyical,
				map_regions[ix].map_info.size);
			map_regions[ix].map_info.map_priv_1 =
				(int)ioremap_nocache(
				map_regions[ix].window_addr_phyical,
				map_regions[ix].map_info.size);
			if(!map_regions[ix].map_info.map_priv_1)
			{
				printk(MTD_FORTUNET_PK "%s flash failed to ioremap!\n",
					map_regions[ix].map_info.name);
				return -ENXIO;
			}
			printk(KERN_NOTICE MTD_FORTUNET_PK "%s flash is veritualy at: %x\n",
				map_regions[ix].map_info.name,
				map_regions[ix].map_info.map_priv_1);
			map_regions[ix].mymtd = do_map_probe("cfi_probe",
				&map_regions[ix].map_info);
			if((!map_regions[ix].mymtd)&&(
				map_regions[ix].altbuswidth!=map_regions[ix].map_info.buswidth))
			{
				printk(KERN_NOTICE MTD_FORTUNET_PK "Trying alternet buswidth "
					"for %s flash.\n",
					map_regions[ix].map_info.name);
				map_regions[ix].map_info.buswidth =
					map_regions[ix].altbuswidth;
				map_regions[ix].mymtd = do_map_probe("cfi_probe",
					&map_regions[ix].map_info);
			}
			map_regions[ix].mymtd->module = THIS_MODULE;
			add_mtd_partitions(map_regions[ix].mymtd,
				map_regions[ix].parts,map_regions_parts[ix]);
		}
	}
	if(iy)
		return 0;
	return -ENXIO;
}

static void __exit cleanup_fortunet(void)
{
	int	ix;
	for(ix=0;ix<MAX_NUM_REGIONS;ix++)
	{
		if(map_regions_set[ix])
		{
			if( map_regions[ix].mymtd )
			{
				del_mtd_partitions( map_regions[ix].mymtd );
				map_destroy( map_regions[ix].mymtd );
			}
			iounmap((void *)map_regions[ix].map_info.map_priv_1);
		}
	}
}

module_init(init_fortunet);
module_exit(cleanup_fortunet);

MODULE_AUTHOR("FortuNet, Inc.");
MODULE_DESCRIPTION("MTD map driver for FortuNet boards");

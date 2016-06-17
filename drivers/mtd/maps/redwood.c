/*
 * $Id:
 *
 * redwood.c - mapper for IBM Redwood-4/5 board.
 *	
 * Copyright 2001 MontaVista Softare Inc. 
 *  
 * This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.    
 *  
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR   IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT,  INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *  
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  History: 12/17/2001 - Armin
 *  		migrated to use do_map_probe
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#include <asm/io.h>

#define WINDOW_ADDR 0xffc00000
#define WINDOW_SIZE 0x00400000

__u8 redwood_flash_read8(struct map_info *map, unsigned long ofs)
{
        return *(__u8 *)(map->map_priv_1 + ofs);
}

__u16 redwood_flash_read16(struct map_info *map, unsigned long ofs)
{
        return *(__u16 *)(map->map_priv_1 + ofs);
}

__u32 redwood_flash_read32(struct map_info *map, unsigned long ofs)
{
        return *(volatile unsigned int *)(map->map_priv_1 + ofs);
}

void redwood_flash_copy_from(struct map_info *map, void *to,
                             unsigned long from, ssize_t len)
{
        memcpy(to, (void *)(map->map_priv_1 + from), len);
}

void redwood_flash_write8(struct map_info *map, __u8 d, unsigned long adr)
{
        *(__u8 *)(map->map_priv_1 + adr) = d;
}

void redwood_flash_write16(struct map_info *map, __u16 d, unsigned long adr)
{
        *(__u16 *)(map->map_priv_1 + adr) = d;
}

void redwood_flash_write32(struct map_info *map, __u32 d, unsigned long adr)
{
        *(__u32 *)(map->map_priv_1 + adr) = d;
}

void redwood_flash_copy_to(struct map_info *map, unsigned long to,
                           const void *from, ssize_t len)
{
        memcpy((void *)(map->map_priv_1 + to), from, len);
}

struct map_info redwood_flash_map = {
        name: "IBM Redwood",
        size: WINDOW_SIZE,
        buswidth: 2,
        read8: redwood_flash_read8,
        read16: redwood_flash_read16,
        read32: redwood_flash_read32,
        copy_from: redwood_flash_copy_from,
        write8: redwood_flash_write8,
        write16: redwood_flash_write16,
        write32: redwood_flash_write32,
        copy_to: redwood_flash_copy_to
};


static struct mtd_partition redwood_flash_partitions[] = {
        {
                name: "Redwood OpenBIOS Vital Product Data",
                offset: 0,
                size:   0x10000,
                mask_flags: MTD_WRITEABLE       /* force read-only */
        },
        {
                name: "Redwood kernel",
                offset: 0x10000,
                size:   0x200000 - 0x10000
        },
        {
                name: "Redwood OpenBIOS non-volatile storage",
                offset: 0x200000,
                size:   0x10000,
                mask_flags: MTD_WRITEABLE       /* force read-only */
        },
        {
                name: "Redwood filesystem",
                offset: 0x210000,
                size:   0x200000 - (0x10000 + 0x20000)
        },
        {
                name: "Redwood OpenBIOS",
                offset: 0x3e0000,
                size:   0x20000,
                mask_flags: MTD_WRITEABLE       /* force read-only */
        }
};
#define NUM_REDWOOD_FLASH_PARTITIONS \
        (sizeof(redwood_flash_partitions)/sizeof(redwood_flash_partitions[0]))

static struct mtd_info *redwood_mtd;

int __init init_redwood_flash(void)
{
        printk(KERN_NOTICE "redwood: flash mapping: %x at %x\n",
               WINDOW_SIZE, WINDOW_ADDR);

        redwood_flash_map.map_priv_1 =
                (unsigned long)ioremap(WINDOW_ADDR, WINDOW_SIZE);

        if (!redwood_flash_map.map_priv_1) {
                printk("init_redwood_flash: failed to ioremap\n");
                return -EIO;
        }

        redwood_mtd = do_map_probe("cfi_probe",&redwood_flash_map);

        if (redwood_mtd) {
                redwood_mtd->module = THIS_MODULE;
                return add_mtd_partitions(redwood_mtd,
                                          redwood_flash_partitions,
                                          NUM_REDWOOD_FLASH_PARTITIONS);
        }

        return -ENXIO;
}

static void __exit cleanup_redwood_flash(void)
{
        if (redwood_mtd) {
                del_mtd_partitions(redwood_mtd);
                iounmap((void *)redwood_flash_map.map_priv_1);
                map_destroy(redwood_mtd);
        }
}

module_init(init_redwood_flash);
module_exit(cleanup_redwood_flash);

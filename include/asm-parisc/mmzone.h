#ifndef _PARISC_MMZONE_H
#define _PARISC_MMZONE_H

struct node_map_data {
    pg_data_t pg_data;
    struct page *adj_node_mem_map;
};

extern struct node_map_data node_data[];
extern unsigned char *chunkmap;

#define BADCHUNK                ((unsigned char)0xff)
#define CHUNKSZ                 (256*1024*1024)
#define CHUNKSHIFT              28
#define CHUNKMASK               (~(CHUNKSZ - 1))
#define CHUNKNUM(paddr)         ((paddr) >> CHUNKSHIFT)

#define NODE_DATA(nid)          (&node_data[nid].pg_data)
#define NODE_MEM_MAP(nid)       (NODE_DATA(nid)->node_mem_map)
#define ADJ_NODE_MEM_MAP(nid)   (node_data[nid].adj_node_mem_map)

#define phys_to_page(paddr) \
	(ADJ_NODE_MEM_MAP(chunkmap[CHUNKNUM((paddr))]) \
	+ ((paddr) >> PAGE_SHIFT))

#define virt_to_page(kvaddr) phys_to_page(__pa(kvaddr))

/* This is kind of bogus, need to investigate performance of doing it right */
#define VALID_PAGE(page)	((page - mem_map) < max_mapnr)

#endif /* !_PARISC_MMZONE_H */

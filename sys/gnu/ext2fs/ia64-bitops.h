/* $FreeBSD$ */

#ifndef _GNU_EXT2FS_BITOPS_H_
#define _GNU_EXT2FS_BITOPS_H_

#define	find_first_zero_bit(addr, size)		find_next_zero_bit(addr,size,0)
 
static __inline int
clear_bit(int no, void *addr)
{
	panic("ext2fs: clear_bit() unimplemented");
	return (0);
}

static __inline int
set_bit(int no, void *addr)
{
	panic("ext2fs: set_bit() unimplemented");
	return (0);
}

static __inline int
test_bit(int no, void *addr)
{
	panic("ext2fs: clear_bit() unimplemented");
	return (0);
}

static __inline size_t
find_next_zero_bit(void *addr, size_t size, size_t ofs)
{
	panic("ext2fs: find_next_zero_bit() unimplemented");
	return (0);
}

static __inline void *
memscan(void *addr, int c, size_t sz)
{
	panic("ext2fs: memscan() unimplemented");
	return (addr);
}

#endif /* _GNU_EXT2FS_BITOPS_H_ */

/*----------------------------------------------------------------
 * compatibility macros for AWE32 driver
 *----------------------------------------------------------------*/

/* redefine following macros */
#undef IOCTL_IN
#undef IOCTL_OUT
#undef OUTW
#undef COPY_FROM_USER
#undef COPY_TO_USER
#undef GET_BYTE_FROM_USER
#undef GET_SHORT_FROM_USER
#undef IOCTL_TO_USER
  
#ifdef linux

/*================================================================
 * Linux macros
 *================================================================*/

/* use inline prefix */
#define INLINE	inline

/*----------------------------------------------------------------
 * memory management for linux
 *----------------------------------------------------------------*/

#ifdef AWE_OBSOLETE_VOXWARE
/* old type linux system */

/* i/o requests; nothing */
#define awe_check_port()	0	/* always false */
#define awe_request_region()	/* nothing */
#define awe_release_region()	/* nothing */

static int _mem_start;  /* memory pointer for permanent buffers */

#define my_malloc_init(memptr)	_mem_start = (memptr)
#define my_malloc_memptr()	_mem_start
#define my_free(ptr)	/* do nothing */
#define my_realloc(buf,oldsize,size)	NULL	/* no realloc */

static void *my_malloc(int size)
{
	char *ptr;
	PERMANENT_MALLOC(ptr, char*, size, _mem_start);
	return (void*)ptr;
}

/* allocate buffer only once */
#define INIT_TABLE(buffer,index,nums,type) {\
buffer = my_malloc(sizeof(type) * (nums)); index = (nums);\
}

#else

#define AWE_DYNAMIC_BUFFER

#define my_malloc_init(ptr)	/* nothing */
#define my_malloc_memptr()	0
#define my_malloc(size)		vmalloc(size)
#define my_free(ptr)		if (ptr) {vfree(ptr);}

static void *my_realloc(void *buf, int oldsize, int size)
{
	void *ptr;
	if ((ptr = vmalloc(size)) == NULL)
		return NULL;
	memcpy(ptr, buf, ((oldsize < size) ? oldsize : size) );
	vfree(buf);
	return ptr;
}

/* do not allocate buffer at beginning */
#define INIT_TABLE(buffer,index,nums,type) {buffer=NULL; index=0;}

/* old type macro */
#define RET_ERROR(err)		-err

#endif

/*----------------------------------------------------------------
 * i/o interfaces for linux
 *----------------------------------------------------------------*/

#define OUTW(data,addr)		outw(data, addr)

#ifdef AWE_NEW_KERNEL_INTERFACE
#define COPY_FROM_USER(target,source,offs,count) \
	copy_from_user(target, (source)+(offs), count)
#define GET_BYTE_FROM_USER(target,addr,offs) \
	get_user(target, (unsigned char*)&((addr)[offs]))
#define GET_SHORT_FROM_USER(target,addr,offs) \
	get_user(target, (unsigned short*)&((addr)[offs]))
#ifdef AWE_OSS38
#define IOCTL_TO_USER(target,offs,source,count) \
	memcpy(target, (source)+(offs), count)
#define IO_WRITE_CHECK(cmd)	(_SIOC_DIR(cmd) & _IOC_WRITE)
#else
#define IOCTL_TO_USER(target,offs,source,count) \
	copy_to_user(target, (source)+(offs), count)
#define IO_WRITE_CHECK(cmd)	(_IOC_DIR(cmd) & _IOC_WRITE)
#endif /* AWE_OSS38 */
#define COPY_TO_USER	IOCTL_TO_USER
#define IOCTL_IN(arg)	(*(int*)(arg))
#define IOCTL_OUT(arg,val) (*(int*)(arg) = (val))

#else /* old type i/o */
#define COPY_FROM_USER(target,source,offs,count) \
	memcpy_fromfs(target, (source)+(offs), (count))
#define GET_BYTE_FROM_USER(target,addr,offs) \
	*((char  *)&(target)) = get_fs_byte((addr)+(offs))
#define GET_SHORT_FROM_USER(target,addr,offs) \
	*((short *)&(target)) = get_fs_word((addr)+(offs))
#define IOCTL_TO_USER(target,offs,source,count) \
	memcpy_tofs(target, (source)+(offs), (count))
#define COPY_TO_USER	IOCTL_TO_USER
#define IO_WRITE_CHECK(cmd)	(cmd & IOC_IN)
#define IOCTL_IN(arg)		get_fs_long((long *)(arg))
#define IOCTL_OUT(arg,ret)	snd_ioctl_return((int *)arg, ret)

#endif /* AWE_NEW_KERNEL_INTERFACE */

#define BZERO(target,len)	memset(target, 0, len)
#define MEMCPY(dst,src,len)	memcpy(dst, src, len)


#elif defined(__FreeBSD__)

/*================================================================
 * FreeBSD macros
 *================================================================*/

/* inline is not checked yet.. maybe it'll work */
#define INLINE	/*inline*/

/*----------------------------------------------------------------
 * memory management for freebsd
 *----------------------------------------------------------------*/

/* i/o requests; nothing */
#define awe_check_port()	0	/* always false */
#define awe_request_region()	/* nothing */
#define awe_release_region()	/* nothing */

#define AWE_DYNAMIC_BUFFER

#define my_malloc_init(ptr)	/* nothing */
#define my_malloc_memptr()	0
#define my_malloc(size)		malloc(size, M_TEMP, M_WAITOK)
#define my_free(ptr)		if (ptr) {free(ptr, M_TEMP);}

#define INIT_TABLE(buffer,index,nums,type) {buffer=NULL; index=0;}

/* it should be realloc? */
static void *my_realloc(void *buf, int oldsize, int size)
{
	void *ptr;
	if ((ptr = my_malloc(size)) == NULL)
		return NULL;
	memcpy(ptr, buf, ((oldsize < size) ? oldsize : size) );
	my_free(buf);
	return ptr;
}

/*----------------------------------------------------------------
 * i/o interfaces for freebsd
 *----------------------------------------------------------------*/

/* according to linux rule; the arguments are swapped */
#define OUTW(data,addr)		outw(addr, data)

#define COPY_FROM_USER(target,source,offs,count) \
	uiomove(((caddr_t)(target)),(count),((struct uio *)(source)))
#define COPY_TO_USER(target,source,offs,count) \
	uiomove(((caddr_t)(source)),(count),((struct uio *)(target)))
#define GET_BYTE_FROM_USER(target,addr,offs) \
	uiomove(((char*)&(target)), 1, ((struct uio *)(addr)))
#define GET_SHORT_FROM_USER(target,addr,offs) \
	uiomove(((char*)&(target)), 2, ((struct uio *)(addr)))
#define IOCTL_TO_USER(target,offs,source,count) \
	memcpy(&((target)[offs]), (source), (count))
#define IO_WRITE_CHECK(cmd)	(cmd & IOC_IN)
#define IOCTL_IN(arg)		(*(int*)(arg))
#define IOCTL_OUT(arg,val)	(*(int*)(arg) = (val))
#define BZERO(target,len)	bzero((caddr_t)target, len)
#define MEMCPY(dst,src,len)	bcopy((caddr_t)src, (caddr_t)dst, len)

#endif


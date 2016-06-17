#include <stdio.h>
#include <stdlib.h>
#include <byteswap.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

void xlate( char * inb, char * trb, unsigned len )
{
	unsigned i;
	for (  i=0; i<len; ++i )
	{
		char c = *inb++;
		char c1 = c >> 4;
		char c2 = c & 0xf;
		if ( c1 > 9 )
			c1 = c1 + 'A' - 10;
		else
			c1 = c1 + '0';
		if ( c2 > 9 )
			c2 = c2 + 'A' - 10;
		else
			c2 = c2 + '0';
		*trb++ = c1;
		*trb++ = c2;
	}
	*trb = 0;
}

#define ElfHeaderSize  (64 * 1024)
#define ElfPages  (ElfHeaderSize / 4096)

void get4k( /*istream *inf*/FILE *file, char *buf )
{
	unsigned j;
	unsigned num = fread(buf, 1, 4096, file);
	for ( j=num; j<4096; ++j )
		buf[j] = 0;
}

void put4k( /*ostream *outf*/FILE *file, char *buf )
{
	fwrite(buf, 1, 4096, file);
}

int main(int argc, char **argv)
{
	char inbuf[4096];
	FILE *sysmap = NULL;
	char* ptr_end = NULL; 
	FILE *inputVmlinux = NULL;
	FILE *outputVmlinux = NULL;
	long i = 0;
	unsigned long sysmapFileLen = 0;
	unsigned long sysmapLen = 0;
	unsigned long roundR = 0;
	unsigned long kernelLen = 0;
	unsigned long actualKernelLen = 0;
	unsigned long round = 0;
	unsigned long roundedKernelLen = 0;
	unsigned long sysmapStartOffs = 0;
	unsigned long sysmapPages = 0;
	unsigned long roundedKernelPages = 0;
	long padPages = 0;
	if ( argc < 2 )
	{
		fprintf(stderr, "Name of System Map file missing.\n");
		exit(1);
	}

	if ( argc < 3 )
	{
		fprintf(stderr, "Name of vmlinux file missing.\n");
		exit(1);
	}

	if ( argc < 4 )
	{
		fprintf(stderr, "Name of vmlinux output file missing.\n");
		exit(1);
	}

	sysmap = fopen(argv[1], "r");
	if ( ! sysmap )
	{
		fprintf(stderr, "System Map file \"%s\" failed to open.\n", argv[1]);
		exit(1);
	}
	inputVmlinux = fopen(argv[2], "r");
	if ( ! inputVmlinux )
	{
		fprintf(stderr, "vmlinux file \"%s\" failed to open.\n", argv[2]);
		exit(1);
	}
	outputVmlinux = fopen(argv[3], "w");
	if ( ! outputVmlinux )
	{
		fprintf(stderr, "output vmlinux file \"%s\" failed to open.\n", argv[3]);
		exit(1);
	}


  
	fseek(inputVmlinux, 0, SEEK_END);
	kernelLen = ftell(inputVmlinux);
	fseek(inputVmlinux, 0, SEEK_SET);
	printf("kernel file size = %ld\n", kernelLen);
	if ( kernelLen == 0 )
	{
		fprintf(stderr, "You must have a linux kernel specified as argv[2]\n");
		exit(1);
	}


	actualKernelLen = kernelLen - ElfHeaderSize;

	printf("actual kernel length (minus ELF header) = %ld/%lxx \n", actualKernelLen, actualKernelLen);

	round = actualKernelLen % 4096;
	roundedKernelLen = actualKernelLen;
	if ( round )
		roundedKernelLen += (4096 - round);

	printf("Kernel length rounded up to a 4k multiple = %ld/%lxx \n", roundedKernelLen, roundedKernelLen);
	roundedKernelPages = roundedKernelLen / 4096;
	printf("Kernel pages to copy = %ld/%lxx\n", roundedKernelPages, roundedKernelPages);



	/* Sysmap file */
	fseek(sysmap, 0, SEEK_END);
	sysmapFileLen = ftell(sysmap);
	fseek(sysmap, 0, SEEK_SET);
	printf("%s file size = %ld\n", argv[1], sysmapFileLen);

	sysmapLen = sysmapFileLen;

	roundR = 4096 - (sysmapLen % 4096);
	if (roundR)
	{
		printf("Rounding System Map file up to a multiple of 4096, adding %ld\n", roundR);
		sysmapLen += roundR;
	}
	printf("Rounded System Map size is %ld\n", sysmapLen);
  
  /* Process the Sysmap file to determine the true end of the kernel */
	sysmapPages = sysmapLen / 4096;
	printf("System map pages to copy = %ld\n", sysmapPages);
	/* read the whole file line by line, expect that it doesnt fail */
	while ( fgets(inbuf, 4096, sysmap) )  ;
	/* search for _end in the last page of the system map */
	ptr_end = strstr(inbuf, " _end");
	if (!ptr_end)
	{
		fprintf(stderr, "Unable to find _end in the sysmap file \n");
		fprintf(stderr, "inbuf: \n");
		fprintf(stderr, "%s \n", inbuf);
		exit(1);
	}
	printf("Found _end in the last page of the sysmap - backing up 10 characters it looks like %s", ptr_end-10);
	sysmapStartOffs = (unsigned int)strtol(ptr_end-10, NULL, 16);
	/* calc how many pages we need to insert between the vmlinux and the start of the sysmap */
	padPages = sysmapStartOffs/4096 - roundedKernelPages;

	/* Check and see if the vmlinux is larger than _end in System.map */
	if (padPages < 0)
	{ /* vmlinux is larger than _end - adjust the offset to start the embedded system map */ 
		sysmapStartOffs = roundedKernelLen;
		printf("vmlinux is larger than _end indicates it needs to be - sysmapStartOffs = %lx \n", sysmapStartOffs);
		padPages = 0;
		printf("will insert %lx pages between the vmlinux and the start of the sysmap \n", padPages);
	}
	else
	{ /* _end is larger than vmlinux - use the sysmapStartOffs we calculated from the system map */
		printf("vmlinux is smaller than _end indicates is needed - sysmapStartOffs = %lx \n", sysmapStartOffs);
		printf("will insert %lx pages between the vmlinux and the start of the sysmap \n", padPages);
	}




	/* Copy 64K ELF header */
	for (i=0; i<(ElfPages); ++i)
	{
		get4k( inputVmlinux, inbuf );
		put4k( outputVmlinux, inbuf );
	}

  
	/* Copy the vmlinux (as full pages). */
	fseek(inputVmlinux, ElfHeaderSize, SEEK_SET);
	for ( i=0; i<roundedKernelPages; ++i )
	{
		get4k( inputVmlinux, inbuf );
    
		/* Set the offsets (of the start and end) of the embedded sysmap so it is set in the vmlinux.sm */
		if ( i == 0 )
		{
			unsigned long * p;
			printf("Storing embedded_sysmap_start at 0x3c\n");
			p = (unsigned long *)(inbuf + 0x3c);

#if (BYTE_ORDER == __BIG_ENDIAN)
			*p = sysmapStartOffs;
#else
			*p = bswap_32(sysmapStartOffs);
#endif

			printf("Storing embedded_sysmap_end at 0x44\n");
			p = (unsigned long *)(inbuf + 0x44);

#if (BYTE_ORDER == __BIG_ENDIAN)
			*p = sysmapStartOffs + sysmapFileLen;
#else
			*p = bswap_32(sysmapStartOffs + sysmapFileLen);
#endif
		}
    
		put4k( outputVmlinux, inbuf );
	}
  
  
	/* Insert any pad pages between the end of the vmlinux and where the system map needs to be. */
	for (i=0; i<padPages; ++i)
	{
		memset(inbuf, 0, 4096);
		put4k(outputVmlinux, inbuf);
	}


	/* Copy the system map (as full pages). */
	fseek(sysmap, 0, SEEK_SET);  /* start reading from begining of the system map */
	for ( i=0; i<sysmapPages; ++i )
	{
		get4k( sysmap, inbuf );
		put4k( outputVmlinux, inbuf );
	}


	fclose(sysmap);
	fclose(inputVmlinux);
	fclose(outputVmlinux);
	/* Set permission to executable */
	chmod(argv[3], S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);

	return 0;
}


/*
 * This is a simple program which demonstrates use of mmapped DMA buffer
 * of the sound driver directly from application program.
 *
 * This sample program works (currently) only with Linux, FreeBSD and BSD/OS
 * (FreeBSD and BSD/OS require OSS version 3.8-beta16 or later.
 *
 * Note! Don't use mmapped DMA buffers (direct audio) unless you have
 * very good reasons to do it. Programs using this feature will not
 * work with all soundcards. GUS (GF1) is one of them (GUS MAX works).
 *
 * This program requires version 3.5-beta7 or later of OSS
 * (3.8-beta16 or later in FreeBSD and BSD/OS).
 */

#ifndef lint
static const char rcsid[] =
	"$Id$";
#endif /* not lint */

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <machine/soundcard.h>
#include <sys/time.h>

int
main()
{
	int fd, sz, fsz, tmp, nfrag;
        int caps;

	int sd, sl=0, sp;

	unsigned char data[500000], *dp = data;

        caddr_t buf;
	struct timeval tim;

	unsigned char *op;
	
        struct audio_buf_info info;

	int frag = 0xffff000c;	/* Max # fragments of 2^13=8k bytes */

	fd_set writeset;

	close(0);
	if ((fd=open("/dev/dsp", O_RDWR, 0))==-1)
		err(1, "/dev/dsp");
/*
 * Then setup sampling parameters. Just sampling rate in this case.
 */

	tmp = 8000;
	ioctl(fd, SNDCTL_DSP_SPEED, &tmp);

/*
 * Load some test data.
 */

  sl = sp = 0;
  if ((sd=open("smpl", O_RDONLY, 0))!=-1)
  {
	sl = read(sd, data, sizeof(data));
	printf("%d bytes read from file.\n", sl);
	close(sd);
  }
  else warn("smpl");

	if (ioctl(fd, SNDCTL_DSP_GETCAPS, &caps)==-1)
	{
		warn("sorry but your sound driver is too old");
		err(1, "/dev/dsp");
	}

/*
 * Check that the device has capability to do this. Currently just
 * CS4231 based cards will work.
 *
 * The application should also check for DSP_CAP_MMAP bit but this
 * version of driver doesn't have it yet.
 */
/*	ioctl(fd, SNDCTL_DSP_SETSYNCRO, 0); */

/*
 * You need version 3.5-beta7 or later of the sound driver before next
 * two lines compile. There is no point to modify this program to
 * compile with older driver versions since they don't have working
 * mmap() support.
 */
	if (!(caps & DSP_CAP_TRIGGER) ||
	    !(caps & DSP_CAP_MMAP))
		errx(1, "sorry but your soundcard can't do this");

/*
 * Select the fragment size. This is propably important only when
 * the program uses select(). Fragment size defines how often
 * select call returns.
 */

	ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &frag);

/*
 * Compute total size of the buffer. It's important to use this value
 * in mmap() call.
 */

	if (ioctl(fd, SNDCTL_DSP_GETOSPACE, &info)==-1)
		err(1, "GETOSPACE");

	sz = info.fragstotal * info.fragsize;
	fsz = info.fragsize;
/*
 * Call mmap().
 * 
 * IMPORTANT NOTE!!!!!!!!!!!
 *
 * Full duplex audio devices have separate input and output buffers. 
 * It is not possible to map both of them at the same mmap() call. The buffer
 * is selected based on the prot argument in the following way:
 *
 * - PROT_READ (alone) selects the input buffer.
 * - PROT_WRITE (alone) selects the output buffer.
 * - PROT_WRITE|PROT_READ together select the output buffer. This combination
 *   is required in BSD to make the buffer accessible. With just PROT_WRITE
 *   every attempt to access the returned buffer will result in segmentation/bus
 *   error. PROT_READ|PROT_WRITE is also permitted in Linux with OSS version
 *   3.8-beta16 and later (earlier versions don't accept it).
 *
 * Non duplex devices have just one buffer. When an application wants to do both
 * input and output it's recommended that the device is closed and re-opened when
 * switching between modes. PROT_READ|PROT_WRITE can be used to open the buffer
 * for both input and output (with OSS 3.8-beta16 and later) but the result may be
 * unpredictable.
 */

	if ((buf=mmap(NULL, sz, PROT_WRITE | PROT_READ, MAP_FILE|MAP_SHARED, fd, 0))==(caddr_t)-1)
		err(1, "mmap (write)");
	printf("mmap (out) returned %08x\n", buf);
	op=buf;

/*
 * op contains now a pointer to the DMA buffer
 */

/*
 * Then it's time to start the engine. The driver doesn't allow read() and/or
 * write() when the buffer is mapped. So the only way to start operation is
 * to togle device's enable bits. First set them off. Setting them on enables
 * recording and/or playback.
 */

	tmp = 0;
	ioctl(fd, SNDCTL_DSP_SETTRIGGER, &tmp);  

/*
 * It might be usefull to write some data to the buffer before starting.
 */

	tmp = PCM_ENABLE_OUTPUT;
	ioctl(fd, SNDCTL_DSP_SETTRIGGER, &tmp);

/*
 * The machine is up and running now. Use SNDCTL_DSP_GETOPTR to get the
 * buffer status.
 *
 * NOTE! The driver empties each buffer fragmen after they have been
 * played. This prevents looping sound if there are some performance problems
 * in the application side. For similar reasons it recommended that the
 * application uses some amout of play ahead. It can rewrite the unplayed
 * data later if necessary.
 */

	nfrag = 0;
	while (1)
	{
		struct count_info count;
		int extra;

		FD_ZERO(&writeset);
		FD_SET(fd, &writeset);

		tim.tv_sec = 10;
		tim.tv_usec= 0;

		select(fd+1, &writeset, &writeset, NULL, NULL);
/*
 * SNDCTL_DSP_GETOPTR (and GETIPTR as well) return three items. The
 * bytes field returns number of bytes played since start. It can be used
 * as a real time clock.
 *
 * The blocks field returns number of fragment transitions (interrupts) since
 * previous GETOPTR call. It can be used as a method to detect underrun 
 * situations.
 *
 * The ptr field is the DMA pointer inside the buffer area (in bytes from
 * the beginning of total buffer area).
 */

		if (ioctl(fd, SNDCTL_DSP_GETOPTR, &count)==-1)
			err(1, "GETOPTR");
                if (count.ptr < 0 ) count.ptr = 0;
		nfrag += count.blocks;


#ifdef VERBOSE

		printf("\rTotal: %09d, Fragment: %03d, Ptr: %06d",
			count.bytes, nfrag, count.ptr);
		fflush(stdout);
#endif

/*
 * Caution! This version doesn't check for bounds of the DMA
 * memory area. It's possible that the returned pointer value is not aligned
 * to fragment boundaries. It may be several samples behind the boundary
 * in case there was extra delay between the actual hardware interrupt and
 * the time when DSP_GETOPTR was called.
 *
 * Don't just call memcpy() with length set to 'fragment_size' without
 * first checking that the transfer really fits to the buffer area.
 * A mistake of just one byte causes seg fault. It may be easiest just
 * to align the returned pointer value to fragment boundary before using it.
 *
 * It would be very good idea to write few extra samples to next fragment
 * too. Otherwise several (uninitialized) samples from next fragment
 * will get played before your program gets chance to initialize them.
 * Take in count the fact thaat there are other processes batling about
 * the same CPU. This effect is likely to be very annoying if fragment
 * size is decreased too much.
 */

/*
 * Just a minor clarification to the above. The following line alings
 * the pointer to fragment boundaries. Note! Don't trust that fragment
 * size is always a power of 2. It may not be so in future.
 */
		count.ptr = ((count.ptr+16)/fsz )*fsz;
#ifdef VERBOSE
		printf(" memcpy(%6d, %4d)", (dp-data), fsz);
		fflush(stdout);
#endif

/*
 * Set few bytes in the beginning of next fragment too.
 */

		if ((count.ptr+fsz+16) < sz)	/* Last fragment? */
		   extra = 16;
		else
		   extra = 0;
		memcpy(op+count.ptr, dp, (fsz+extra));
		dp += fsz;
		if (dp > (data+sl-fsz))
		   dp = data;

	}

	exit(0);
}

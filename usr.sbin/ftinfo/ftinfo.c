/*
 *  Copyright (c) 1993 Steve Gerakines
 *
 *  This is freely redistributable software.  You may do anything you
 *  wish with it, so long as the above notice stays intact.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS
 *  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT,
 *  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 *  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 *  IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 *  ftinfo.c - display tape drive status
 *  10/30/93 v0.3
 *  Initial revision.
 *
 *  usage: ftinfo [ -f tape ]
 */
#include <stdio.h>
#include <sys/ftape.h>

#define	DEFQIC	"/dev/rft0"
#define	equal(s1,s2)	(strcmp(s1, s2) == 0)
QIC_HWInfo hw;
QIC_Geom g;

main(int argc, char *argv[])
{
  int ft;
  int gotgeom;
  int s;
	char *tape, *getenv();

	if (argc > 2 && (equal(argv[1], "-t") || equal(argv[1], "-f"))) {
		argc -= 2;
		tape = argv[2];
		argv += 2;
	} else
		if ((tape = getenv("TAPE")) == NULL)
			tape = DEFQIC;
  if (argc > 1) {
	fprintf(stderr, "usage: ftinfo [ -f tape ]\n");
	exit(1);
  }

  if ((ft = open(tape, 2)) < 0) {
	fprintf(stderr, "ftinfo: couldn't open tape device %s\n", tape);
	exit(2);
  }

  if (ioctl(ft, QIOSTATUS, &s) < 0) {
	fprintf(stderr, "ftinfo: couldn't get tape drive status\n");
	exit(2);
  }

  if ((s & QS_CART) && (s & QS_FMTOK)) {
	if (ioctl(ft, QIOGEOM, &g) < 0)
		fprintf(stderr, "ftinfo: warning: get tape geometry failed\n");
  }

  if (ioctl(ft, QIOHWINFO, &hw) < 0)
	fprintf(stderr, "ftinfo: warning: get hardware info failed\n");

  close(ft);

  printf("drive status:      %s\n", (s & QS_READY) ? "Ready" : "Not Ready");
  if (s & QS_CART) {
	if (s & QS_FMTOK) {
		printf("tape type:         %s %s\n",
			g.g_fmtdesc, (s & QS_RDONLY) ? "(Write-Protect)" : "");
		printf("tape length:       %s\n", g.g_lendesc);
	} else
		printf("tape type:         Unformatted %s\n",
			(s & QS_RDONLY) ? "(Write-Protect)" : "");
  } else
	printf("tape type:         No tape in drive\n");
  printf("drive make:        0x%04x\n", hw.hw_make);
  printf("drive model:       0x%02x\n", hw.hw_model);
  printf("drive rom-id:      0x%02x\n", hw.hw_romid);
  printf("beta roms:         %s\n", hw.hw_rombeta ? "Yes" : "No");

  exit(0);
}

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <net/route.h>
#include <net/if.h>
#include <sys/termios.h>
#define COMPAT_43
#include <sys/ioctl.h>

char *
ioctlname(val)
{

	if (val ==  TIOCMODG)
		return("TIOCMODG");
	if (val ==  TIOCMODS)
		return("TIOCMODS");
	if (val ==  TIOCEXCL)
		return("TIOCEXCL");
	if (val ==  TIOCNXCL)
		return("TIOCNXCL");
	if (val ==  TIOCFLUSH)
		return("TIOCFLUSH");
	if (val ==  TIOCGETA)
		return("TIOCGETA");
	if (val ==  TIOCSETA)
		return("TIOCSETA");
	if (val ==  TIOCSETAW)
		return("TIOCSETAW");
	if (val ==  TIOCSETAF)
		return("TIOCSETAF");
	if (val ==  TIOCGETD)
		return("TIOCGETD");
	if (val ==  TIOCSETD)
		return("TIOCSETD");
	if (val ==  TIOCSBRK)
		return("TIOCSBRK");
	if (val ==  TIOCCBRK)
		return("TIOCCBRK");
	if (val ==  TIOCSDTR)
		return("TIOCSDTR");
	if (val ==  TIOCCDTR)
		return("TIOCCDTR");
	if (val ==  TIOCGPGRP)
		return("TIOCGPGRP");
	if (val ==  TIOCSPGRP)
		return("TIOCSPGRP");
	if (val ==  TIOCOUTQ)
		return("TIOCOUTQ");
	if (val ==  TIOCSTI)
		return("TIOCSTI");
	if (val ==  TIOCNOTTY)
		return("TIOCNOTTY");
	if (val ==  TIOCPKT)
		return("TIOCPKT");
	if (val ==  TIOCPKT_IOCTL)
		return("TIOCPKT_IOCTL");
	if (val ==  TIOCSTOP)
		return("TIOCSTOP");
	if (val ==  TIOCSTART)
		return("TIOCSTART");
	if (val ==  TIOCMSET)
		return("TIOCMSET");
	if (val ==  TIOCMBIS)
		return("TIOCMBIS");
	if (val ==  TIOCMBIC)
		return("TIOCMBIC");
	if (val ==  TIOCMGET)
		return("TIOCMGET");
	if (val ==  TIOCREMOTE)
		return("TIOCREMOTE");
	if (val ==  TIOCGWINSZ)
		return("TIOCGWINSZ");
	if (val ==  TIOCSWINSZ)
		return("TIOCSWINSZ");
	if (val ==  TIOCUCNTL)
		return("TIOCUCNTL");
	if (val ==  TIOCCONS)
		return("TIOCCONS");
	if (val ==  TIOCSCTTY)
		return("TIOCSCTTY");
	if (val ==  TIOCEXT)
		return("TIOCEXT");
	if (val ==  TIOCSIG)
		return("TIOCSIG");
	if (val ==  TIOCDRAIN)
		return("TIOCDRAIN");
	if (val ==  FIOCLEX)
		return("FIOCLEX");
	if (val ==  FIONCLEX)
		return("FIONCLEX");
	if (val ==  FIONREAD)
		return("FIONREAD");
	if (val ==  FIONBIO)
		return("FIONBIO");
	if (val ==  FIOASYNC)
		return("FIOASYNC");
	if (val ==  FIOSETOWN)
		return("FIOSETOWN");
	if (val ==  FIOGETOWN)
		return("FIOGETOWN");
	if (val ==  SIOCSHIWAT)
		return("SIOCSHIWAT");
	if (val ==  SIOCGHIWAT)
		return("SIOCGHIWAT");
	if (val ==  SIOCSLOWAT)
		return("SIOCSLOWAT");
	if (val ==  SIOCGLOWAT)
		return("SIOCGLOWAT");
	if (val ==  SIOCATMARK)
		return("SIOCATMARK");
	if (val ==  SIOCSPGRP)
		return("SIOCSPGRP");
	if (val ==  SIOCGPGRP)
		return("SIOCGPGRP");
	if (val ==  SIOCADDRT)
		return("SIOCADDRT");
	if (val ==  SIOCDELRT)
		return("SIOCDELRT");
	if (val ==  SIOCSIFADDR)
		return("SIOCSIFADDR");
	if (val ==  OSIOCGIFADDR)
		return("OSIOCGIFADDR");
	if (val ==  SIOCGIFADDR)
		return("SIOCGIFADDR");
	if (val ==  SIOCSIFDSTADDR)
		return("SIOCSIFDSTADDR");
	if (val ==  OSIOCGIFDSTADDR)
		return("OSIOCGIFDSTADDR");
	if (val ==  SIOCGIFDSTADDR)
		return("SIOCGIFDSTADDR");
	if (val ==  SIOCSIFFLAGS)
		return("SIOCSIFFLAGS");
	if (val ==  SIOCGIFFLAGS)
		return("SIOCGIFFLAGS");
	if (val ==  OSIOCGIFBRDADDR)
		return("OSIOCGIFBRDADDR");
	if (val ==  SIOCGIFBRDADDR)
		return("SIOCGIFBRDADDR");
	if (val ==  SIOCSIFBRDADDR)
		return("SIOCSIFBRDADDR");
	if (val ==  OSIOCGIFCONF)
		return("OSIOCGIFCONF");
	if (val ==  SIOCGIFCONF)
		return("SIOCGIFCONF");
	if (val ==  OSIOCGIFNETMASK)
		return("OSIOCGIFNETMASK");
	if (val ==  SIOCGIFNETMASK)
		return("SIOCGIFNETMASK");
	if (val ==  SIOCSIFNETMASK)
		return("SIOCSIFNETMASK");
	if (val ==  SIOCGIFMETRIC)
		return("SIOCGIFMETRIC");
	if (val ==  SIOCSIFMETRIC)
		return("SIOCSIFMETRIC");
	if (val ==  SIOCDIFADDR)
		return("SIOCDIFADDR");
	if (val ==  SIOCAIFADDR)
		return("SIOCAIFADDR");
	if (val ==  SIOCSARP)
		return("SIOCSARP");
	if (val ==  OSIOCGARP)
		return("OSIOCGARP");
	if (val ==  SIOCGARP)
		return("SIOCGARP");
	if (val ==  SIOCDARP)
		return("SIOCDARP");
	if (val ==  TIOCGETD)
		return("TIOCGETD");
	if (val ==  TIOCSETD)
		return("TIOCSETD");
	if (val ==  TIOCHPCL)
		return("TIOCHPCL");
	if (val ==  TIOCGETP)
		return("TIOCGETP");
	if (val ==  TIOCSETP)
		return("TIOCSETP");
	if (val ==  TIOCSETN)
		return("TIOCSETN");
	if (val ==  TIOCSETC)
		return("TIOCSETC");
	if (val ==  TIOCGETC)
		return("TIOCGETC");
	if (val ==  TIOCLBIS)
		return("TIOCLBIS");
	if (val ==  TIOCLBIC)
		return("TIOCLBIC");
	if (val ==  TIOCLSET)
		return("TIOCLSET");
	if (val ==  TIOCLGET)
		return("TIOCLGET");
	if (val ==  TIOCSLTC)
		return("TIOCSLTC");
	if (val ==  TIOCGLTC)
		return("TIOCGLTC");

	return(NULL);
}

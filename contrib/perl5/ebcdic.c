#include "EXTERN.h"
#define PERL_IN_EBCDIC_C
#include "perl.h"

/* in ASCII order, not that it matters */
static const char controllablechars[] = "?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_";
 
int
ebcdic_control(int ch)
{
    	if (ch > 'a') {
	        char *ctlp;
 
 	       if (islower(ch))
  	              ch = toupper(ch);
 
 	       if ((ctlp = strchr(controllablechars, ch)) == 0) {
  	              Perl_die(aTHX_ "unrecognised control character '%c'\n", ch);
     	       }
 
        	if (ctlp == controllablechars)
         	       return('\177'); /* DEL */
        	else
         	       return((unsigned char)(ctlp - controllablechars - 1));
	} else { /* Want uncontrol */
        	if (ch == '\177' || ch == -1)
                	return('?');
        	else if (ch == '\157')
                	return('\177');
        	else if (ch == '\174')
                	return('\000');
        	else if (ch == '^')    /* '\137' in 1047, '\260' in 819 */
                	return('\036');
        	else if (ch == '\155')
                	return('\037');
        	else if (0 < ch && ch < (sizeof(controllablechars) - 1))
                	return(controllablechars[ch+1]);
        	else
                	Perl_die(aTHX_ "invalid control request: '\\%03o'\n", ch & 0xFF);
	}
}

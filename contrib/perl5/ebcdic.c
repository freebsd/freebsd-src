#include "EXTERN.h"
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
  	              die("unrecognised control character '%c'\n", ch);
     	       }
 
        	if (ctlp == controllablechars)
         	       return('\177'); /* DEL */
        	else
         	       return((unsigned char)(ctlp - controllablechars - 1));
	} else { /* Want uncontrol */
        	if (ch == '\177' || ch == -1)
                	return('?');
        	else if (0 < ch && ch < (sizeof(controllablechars) - 1))
                	return(controllablechars[ch+1]);
        	else
                	die("invalid control request: '\\%03o'\n", ch & 0xFF);
	}
}

/* gpib data structures */
struct gpibdata {
          char *data;  /* data string for ins and outs	*/
          unsigned char address; /* gpib address */
          int *count;
           } ;


/* IOCTL commands */
#define GPIBWRITE _IOW('g',1,struct gpibdata)
#define GPIBREAD _IOW('g',2,struct gpibdata)
#define GPIBINIT _IOW('g',3,struct gpibdata)
#define GPIBTRIGGER _IOW('g',4,struct gpibdata)
#define GPIBREMOTE _IOW('g',5,struct gpibdata)
#define GPIBLOCAL _IOW('g',6,struct gpibdata)
#define GPIBMTRIGGER _IOW('g',7,struct gpibdata)
#define GPIBMREMOTE _IOW('g',8,struct gpibdata)
#define GPIBMLOCAL _IOW('g',9,struct gpibdata)
#define GPIBSPOLL _IOW('g',10,struct gpibdata)

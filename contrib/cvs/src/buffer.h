/* Declarations concerning the buffer data structure.  */

#if defined (SERVER_SUPPORT) || defined (CLIENT_SUPPORT)

/*
 * We must read data from a child process and send it across the
 * network.  We do not want to block on writing to the network, so we
 * store the data from the child process in memory.  A BUFFER
 * structure holds the status of one communication, and uses a linked
 * list of buffer_data structures to hold data.
 */

struct buffer
{
    /* Data.  */
    struct buffer_data *data;

    /* Last buffer on data chain.  */
    struct buffer_data *last;

    /* Nonzero if the buffer is in nonblocking mode.  */
    int nonblocking;

    /* Functions must be provided to transfer data in and out of the
       buffer.  Either the input or output field must be set, but not
       both.  */

    /* Read data into the buffer DATA.  There is room for up to SIZE
       bytes.  In blocking mode, wait until some input, at least NEED
       bytes, is available (NEED may be 0 but that is the same as NEED
       == 1).  In non-blocking mode return immediately no matter how
       much input is available; NEED is ignored. Return 0 on success,
       or -1 on end of file, or an errno code.  Set the number of
       bytes read in *GOT.
       
       If there are a nonzero number of bytes available, less than NEED,
       followed by end of file, just read those bytes and return 0.  */
    int (*input) PROTO((void *closure, char *data, int need, int size,
			int *got));

    /* Write data.  This should write up to HAVE bytes from DATA.
       This should return 0 on success, or an errno code.  It should
       set the number of bytes written in *WROTE.  */
    int (*output) PROTO((void *closure, const char *data, int have,
			 int *wrote));

    /* Flush any data which may be buffered up after previous calls to
       OUTPUT.  This should return 0 on success, or an errno code.  */
    int (*flush) PROTO((void *closure));

    /* Change the blocking mode of the underlying communication
       stream.  If BLOCK is non-zero, it should be placed into
       blocking mode.  Otherwise, it should be placed into
       non-blocking mode.  This should return 0 on success, or an
       errno code.  */
    int (*block) PROTO ((void *closure, int block));

    /* Shut down the communication stream.  This does not mean that it
       should be closed.  It merely means that no more data will be
       read or written, and that any final processing that is
       appropriate should be done at this point.  This may be NULL.
       It should return 0 on success, or an errno code.  This entry
       point exists for the compression code.  */
    int (*shutdown) PROTO((struct buffer *));

    /* This field is passed to the INPUT, OUTPUT, and BLOCK functions.  */
    void *closure;

    /* Function to call if we can't allocate memory.  */
    void (*memory_error) PROTO((struct buffer *));
};

/* Data is stored in lists of these structures.  */

struct buffer_data
{
    /* Next buffer in linked list.  */
    struct buffer_data *next;

    /*
     * A pointer into the data area pointed to by the text field.  This
     * is where to find data that has not yet been written out.
     */
    char *bufp;

    /* The number of data bytes found at BUFP.  */
    int size;

    /*
     * Actual buffer.  This never changes after the structure is
     * allocated.  The buffer is BUFFER_DATA_SIZE bytes.
     */
    char *text;
};

/* The size we allocate for each buffer_data structure.  */
#define BUFFER_DATA_SIZE (4096)

/* The type of a function passed as a memory error handler.  */
typedef void (*BUFMEMERRPROC) PROTO ((struct buffer *));

extern struct buffer *buf_initialize PROTO((int (*) (void *, char *, int,
						     int, int *),
					    int (*) (void *, const char *,
						     int, int *),
					    int (*) (void *),
					    int (*) (void *, int),
					    int (*) (struct buffer *),
					    void (*) (struct buffer *),
					    void *));
extern void buf_free PROTO((struct buffer *));
extern struct buffer *buf_nonio_initialize PROTO((void (*) (struct buffer *)));
extern struct buffer *stdio_buffer_initialize
  PROTO((FILE *, int, int, void (*) (struct buffer *)));
extern FILE *stdio_buffer_get_file PROTO((struct buffer *));
extern struct buffer *compress_buffer_initialize
  PROTO((struct buffer *, int, int, void (*) (struct buffer *)));
extern struct buffer *packetizing_buffer_initialize
  PROTO((struct buffer *, int (*) (void *, const char *, char *, int),
	 int (*) (void *, const char *, char *, int, int *), void *,
	 void (*) (struct buffer *)));
extern int buf_empty_p PROTO((struct buffer *));
extern void buf_output PROTO((struct buffer *, const char *, int));
extern void buf_output0 PROTO((struct buffer *, const char *));
extern void buf_append_char PROTO((struct buffer *, int));
extern int buf_send_output PROTO((struct buffer *));
extern int buf_flush PROTO((struct buffer *, int));
extern int set_nonblock PROTO((struct buffer *));
extern int set_block PROTO((struct buffer *));
extern int buf_send_counted PROTO((struct buffer *));
extern int buf_send_special_count PROTO((struct buffer *, int));
extern void buf_append_data PROTO((struct buffer *,
				   struct buffer_data *,
				   struct buffer_data *));
extern void buf_append_buffer PROTO((struct buffer *, struct buffer *));
extern int buf_read_file PROTO((FILE *, long, struct buffer_data **,
				struct buffer_data **));
extern int buf_read_file_to_eof PROTO((FILE *, struct buffer_data **,
				       struct buffer_data **));
extern int buf_input_data PROTO((struct buffer *, int *));
extern int buf_read_line PROTO((struct buffer *, char **, int *));
extern int buf_read_data PROTO((struct buffer *, int, char **, int *));
extern void buf_copy_lines PROTO((struct buffer *, struct buffer *, int));
extern int buf_copy_counted PROTO((struct buffer *, struct buffer *, int *));
extern int buf_chain_length PROTO((struct buffer_data *));
extern int buf_length PROTO((struct buffer *));
extern int buf_shutdown PROTO((struct buffer *));

#ifdef SERVER_FLOWCONTROL
extern int buf_count_mem PROTO((struct buffer *));
#endif /* SERVER_FLOWCONTROL */

#endif /* defined (SERVER_SUPPORT) || defined (CLIENT_SUPPORT) */

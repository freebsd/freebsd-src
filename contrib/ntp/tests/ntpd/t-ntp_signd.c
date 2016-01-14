#include "config.h"

#include "ntp.h"
#include "ntp_calendar.h"
#include "ntp_stdlib.h"

#include "unity.h"

#include "test-libntp.h"


#define HAVE_NTP_SIGND

#include "ntp_signd.c"

extern int ux_socket_connect(const char *name);


//MOCKED FUNCTIONS

//this connect function overrides/mocks connect() from  <sys/socket.h>
int
connect(int socket, const struct sockaddr *address, socklen_t address_len)
{
	return 1;
}

/*
** Mocked read() and write() calls.
**
** These will only operate 4 bytes at a time.
**
** This is so write_all can be properly tested.
*/

static char rw_buf[4];

ssize_t
write(int fd, void const * buf, size_t len)
{
	REQUIRE(0 <= len);
	if (len >= 4) len = 4;	/* 4 bytes, max */
	(void)memcpy(rw_buf, buf, len);

	return len;
}

ssize_t
read(int fd, void * buf, size_t len)
{
	REQUIRE(0 <= len);
	if (len >= 4) len = 4;
	(void)memcpy(buf, rw_buf, len);
	return len;
}


//END OF MOCKED FUNCTIONS

static int
isGE(int a,int b)
{ 
	if (a >= b) {return 1;}
	else {return 0;}
}

extern void test_connect_incorrect_socket(void);
extern void test_connect_correct_socket(void);
extern void test_write_all(void);
extern void test_send_packet(void);
extern void test_recv_packet(void);
extern void test_send_via_ntp_signd(void);


void 
test_connect_incorrect_socket(void)
{
	TEST_ASSERT_EQUAL(-1, ux_socket_connect(NULL));

	return;
}

void 
test_connect_correct_socket(void)
{
	int temp = ux_socket_connect("/socket");

	//risky, what if something is listening on :123, or localhost isnt 127.0.0.1?
	//TEST_ASSERT_EQUAL(-1, ux_socket_connect("127.0.0.1:123")); 

	//printf("%d\n",temp);
	TEST_ASSERT_TRUE(isGE(temp,0));

	//write_all();
	//char *socketName = "Random_Socket_Name";
	//int length = strlen(socketName);

	return;
}


void
test_write_all(void)
{
	int fd = ux_socket_connect("/socket");

	TEST_ASSERT_TRUE(isGE(fd, 0));

	char * str = "TEST123";
	int temp = write_all(fd, str,strlen(str));
	TEST_ASSERT_EQUAL(strlen(str), temp);

	(void)close(fd);
	return;
}


void
test_send_packet(void)
{
	int fd = ux_socket_connect("/socket");

	TEST_ASSERT_TRUE(isGE(fd, 0));

	char * str2 = "PACKET12345";
	int temp = send_packet(fd, str2, strlen(str2));

	TEST_ASSERT_EQUAL(0,temp);

	(void)close(fd);
	return;
}


/*
** HMS: What's going on here?
** Looks like this needs more work.
*/
void
test_recv_packet(void)
{
	int fd = ux_socket_connect("/socket");

	TEST_ASSERT_TRUE(isGE(fd, 0));

	uint32_t size = 256;	
	char *str = NULL;
	int temp = recv_packet(fd, &str, &size);

	send_packet(fd, str, strlen(str));
	free(str);
	TEST_ASSERT_EQUAL(0,temp); //0 because nobody sent us anything (yet!)

	(void)close(fd);
	return;
}

void 
test_send_via_ntp_signd(void)
{
	struct recvbuf *rbufp = (struct recvbuf *) malloc(sizeof(struct recvbuf));
	int	xmode = 1;
	keyid_t	xkeyid = 12345; 
	int	flags = 0;
	struct pkt  *xpkt = (struct pkt *) malloc(sizeof(struct pkt)); //defined in ntp.h

	TEST_ASSERT_NOT_NULL(rbufp);
	TEST_ASSERT_NOT_NULL(xpkt);
	memset(xpkt, 0, sizeof(struct pkt));

	//send_via_ntp_signd(NULL,NULL,NULL,NULL,NULL);	//doesn't work
	/*
	** Send the xpkt to Samba, read the response back in rbufp
	*/
	send_via_ntp_signd(rbufp,xmode,xkeyid,flags,xpkt);

	free(rbufp);
	free(xpkt);

	return;
}

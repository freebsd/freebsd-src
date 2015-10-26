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
int connect(int socket, const struct sockaddr *address,
socklen_t address_len){
	return 1;
}

//mocked write will only send 4 bytes at a time. This is so write_all can be properly tested
ssize_t write(int fd, void const * buf, size_t len){
	if(len >= 4){return 4;}
	else return len;
}

ssize_t read(int fd, void * buf, size_t len){
	if(len >= 4){return 4;}
	else return len;
}


//END OF MOCKED FUNCTIONS

int isGE(int a,int b){ 
	if(a >= b) {return 1;}
	else {return 0;}
}


void 
test_connect_incorrect_socket(void){
	TEST_ASSERT_EQUAL(-1, ux_socket_connect(NULL));
}

void 
test_connect_correct_socket(void){



	int temp = ux_socket_connect("/socket");

	//risky, what if something is listening on :123, or localhost isnt 127.0.0.1?
	//TEST_ASSERT_EQUAL(-1, ux_socket_connect("127.0.0.1:123")); 

	//printf("%d\n",temp);
	TEST_ASSERT_TRUE(isGE(temp,0));

	//write_all();
	//char *socketName = "Random_Socket_Name";
	//int length = strlen(socketName);

}


void
test_write_all(void){
	int fd = ux_socket_connect("/socket");
	TEST_ASSERT_TRUE(isGE(fd,0));
	char * str = "TEST123";
	int temp = write_all(fd, str,strlen(str));
	TEST_ASSERT_EQUAL(strlen(str),temp);
}


void
test_send_packet(void){
	int fd = ux_socket_connect("/socket");
	char * str2 = "PACKET12345";
	int temp = send_packet(fd, str2, strlen(str2));
	TEST_ASSERT_EQUAL(0,temp);
}


void
test_recv_packet(void){
	int fd = ux_socket_connect("/socket");
	int size = 256;	
	char str[size];

	int temp = recv_packet(fd, &str, &size);
	send_packet(fd, str, strlen(str));
	TEST_ASSERT_EQUAL(0,temp); //0 because nobody sent us anything (yet!)
}

void 
test_send_via_ntp_signd(){

	struct recvbuf *rbufp = (struct recvbuf *) malloc(sizeof(struct recvbuf));
	int	xmode = 1;
	keyid_t	xkeyid = 12345; 
	int flags =0;
	struct pkt  *xpkt = (struct pkt *) malloc(sizeof(struct pkt)); //defined in ntp.h

	//send_via_ntp_signd(NULL,NULL,NULL,NULL,NULL);	//doesn't work
	send_via_ntp_signd(rbufp,xmode,xkeyid,flags,xpkt);


}

#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define TRUE 1
#define FALSE 0

struct rdp {  
  unsigned char flag;
  unsigned char pktseq;
  unsigned char ackseq;
  unsigned char unassigned;
  int           senderid;
  int           recvid;
  int           metadata;
  unsigned char payload[1000];
}__attribute__((packed));
// ^ pakker tett, siden denne structen skal sendes ut på verdensveven

struct rdp_connection {  
  // AKA. en slags RDP socket.
  int id;
  int sockfd;
  struct sockaddr_storage *recipient;
  //struct addrinfo *recipient;
  socklen_t recipientlen;
};

static int rdp_sockfd = 0;

int rdp_print(struct rdp *pakke);
int rdp_printc(struct rdp_connection *con);

int rdp_listen(char *port);
struct rdp_connection *rdp_accept(int sockfd, int accept);
struct rdp_connection *rdp_connect(char* vert, char *port);
int rdp_write(struct rdp_connection *token, struct rdp *pakke);
void *rdp_read(struct rdp_connection *token, void *dest_buf);
int rdp_close(struct rdp_connection *token);
int rdp_error (int rv, char *msg);

// TESTOMRÅDE
void *_get_addr(struct sockaddr_storage *ss);
const char *_get_recipient_addr(char *s, struct rdp_connection *token);
const char *_get_recipient_port(char *s, struct rdp_connection *token);

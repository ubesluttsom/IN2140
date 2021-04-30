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
  uint8_t payload[1000];
}__attribute__((packed));
// ^ pakker tett, siden denne structen skal sendes ut på verdensveven

struct rdp_connection {  
  int pktseq;   // hvor mange pakker er sendt over denne koblingen
  int ackseq;   // siste pakke som ble ACK-et (avgjør om vi kan sende neste)
  int senderid; // IDen til vår ende av koblingen
  int recvid;   // IDen til mottakers ende av koblingen
  int sockfd;   // nettverksstøpsel på lokal maskin
  struct sockaddr_storage *recipient; // addresen til mottaker
  socklen_t recipientlen;             // lengde på addresse
};

struct rdp_connection *rdp_accept(int sockfd, int accept, int assign_id);
struct rdp_connection *rdp_connect(char* vert, char *port, int assign_id);

int   rdp_listen(char *port);
int   rdp_write(struct rdp_connection *con, struct rdp *pakke);
int   rdp_ack(struct rdp_connection *con);
void *rdp_read(struct rdp_connection *con, void *dest_buf);
void *rdp_peek(int sockfd, void *dest_buf,
               struct sockaddr_storage *dest_addr, socklen_t *dest_addrlen);
int   rdp_close(struct rdp_connection *con, int close_sockfd);
int   rdp_error(int rv, char *msg);
int   rdp_print(struct rdp *pakke);
int   rdp_printc(struct rdp_connection *con);

// TESTOMRÅDE
void       *_get_addr(struct sockaddr_storage *ss);
const char *_get_recipient_addr(char *s, struct rdp_connection *con);
const char *_get_recipient_port(char *s, struct rdp_connection *con);

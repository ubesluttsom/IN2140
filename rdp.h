#include <sys/socket.h>

struct rdp {  
  unsigned char flag;
  unsigned char pktseq;
  unsigned char ackseq;
  unsigned char unassigned;
  int           senderid;
  int           recvid;
  int           metadata;
  unsigned char payload[1000];
};

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

struct rdp_connection *rdp_accept(int sockfd);
struct rdp_connection *rdp_connect(char* vert, char *port, struct sockaddr_storage *ss);
int rdp_write(struct rdp_connection *token, struct rdp *pakke);
int rdp_read(struct rdp_connection *token);
int rdp_close(struct rdp_connection *token);

// TESTOMRÅDE
void *_get_addr(struct sockaddr_storage *ss);
void _get_recipient_addr(char *s, struct rdp_connection *token);
void _get_recipient_port(char *s, struct rdp_connection *token);

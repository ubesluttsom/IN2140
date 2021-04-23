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
  int sockfd;
  struct addrinfo *addr;
};

int rdp_print(struct rdp *pakke);

struct rdp_connection * rdp_accept();
struct rdp_connection * rdp_connect(char* vert, char *port);
int rdp_write(struct rdp_connection *token, unsigned char *payload, size_t len);
int rdp_read(struct rdp_connection *token);
int rdp_close(struct rdp_connection *token);

struct rdp {  
  unsigned char       flag;
  unsigned char       pktseq;
  unsigned char       ackseq;
  unsigned char       unassigned;
  int                 senderid;
  int                 recvid;
  int                 metadata;
  unsigned char       payload[1000];
};

struct rdp_connection {  
  // AKA. en slags RDP socket.
  int sockfd;
};

int rdp_print(struct rdp *pakke);

struct rdp_connection * rdp_accept();
int                     rdp_write(struct rdp_connection *token);
int                     rdp_read(struct rdp_connection *token);
struct rdp_connection * rdp_connect(char *port);
int                     rdp_close(struct rdp_connection *token);

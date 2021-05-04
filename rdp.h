#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "send_packet.h"

#define TRUE 1
#define FALSE 0

#define RDP_REQ 0x01 // RDP_REQ(UEST): forbindelsesforespørsel.        
#define RDP_TER 0x02 // RDP_TER(MINATE): forbindelsesavslutning.         
#define RDP_DAT 0x04 // RDP_DAT(A): datapakke.                      
#define RDP_ACK 0x08 // RDP_ACK: en ACK.                            
#define RDP_ACC 0x10 // RDP_ACC(EPT) aksepterer en forbindelsesforespørsel.
#define RDP_DEN 0x20 // RDP_DEN(IED) avslår forbindelsesforespørsel.    

#define RDP_CONFULL -1 // avslår forbindelse grunnet kapasitet
#define RDP_INVALID -2 // avslår forbindelse grunnet ugyldig sender ID

struct rdp {  
  unsigned char flag;       // «Flagg som definerer forskjellige typer pakker»
  unsigned char pktseq;     // «Sekvensnummer pakke»
  unsigned char ackseq;     // «Sekvensnummer ACK-ed av pakke»
  unsigned char unassigned; // «Ubrukt, det vil si alltid 0»
  int           senderid;   // «Avsenderens forbindelses-ID i [Big-Endian]»
  int           recvid;     // «Mottakerens forbindelses-ID i [Big-Endian]»
  int           metadata;   // total datapakkelengde, eller `RDP_DEN` årsak
  uint8_t payload[1000];    // Nyttelast. TODO: «flexible array member»
}__attribute__((packed));
// ^ pakker tett, siden denne structen skal sendes ut på verdensveven

struct rdp_connection {  
  int pktseq;   // hvor mange pakker i sekvens er sendt/mottatt
  int ackseq;   // siste pakke som ble ACK-et (avgjør om vi kan sende neste)
  int senderid; // IDen til vår ende av koblingen
  int recvid;   // IDen til mottakers ende av koblingen
  int sockfd;   // nettverksstøpsel på lokal maskin
  struct sockaddr_storage *recipient; // adressen til mottaker
  socklen_t recipientlen;             // lengde på adresse
};

struct rdp_connection *rdp_accept(int sockfd,
                                  struct rdp_connection *cons[],
                                  int conslen,
                                  int accept,
                                  int assign_id);
struct rdp_connection *rdp_connect(char* vert, char *port, int assign_id);

int   rdp_listen(char *port);
int   rdp_send(struct rdp_connection *con, struct rdp *pakke);
int rdp_write(struct rdp_connection *cons[], // koblinger det skal sendes over
              int N,                         // `cons[]` lengde
              uint8_t data[],                // array med data som skal sendes
              size_t datalen);               // lengde på data array (bytes)
int rdp_terminate(struct rdp_connection *cons[],
                  int conslen,
                  struct rdp_connection *con);
int   rdp_ack(struct rdp_connection *con);
void *rdp_read(int sockfd, struct rdp_connection *cons[], int conslen, void *dest_buf);
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

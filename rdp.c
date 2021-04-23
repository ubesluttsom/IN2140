#include <stdio.h>
#include <stdlib.h>

#include "rdp.h"

#include <sys/errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <unistd.h>   // `close()`

int rdp_print(struct rdp *pakke)
{
  printf("{ ");
  printf("flag==%x, ",          pakke->flag);
  printf("pktseq==%x, ",        pakke->pktseq);
  printf("ackseq==%x, ",        pakke->ackseq);
  printf("senderid==%x, ",      pakke->senderid);
  printf("recvid==%x, ",        pakke->recvid);
  printf("metadata==%x, ",      pakke->metadata);
  printf("payload==\"%s\" }\n", pakke->payload);
  return EXIT_SUCCESS;
}

struct rdp_connection * rdp_connect(char* vert, char* port)
{
  // Binder en socket til port. Jeg har lent meg på denne guiden:
  // <https://beej.us/guide/bgnet/html/index-wide.html>

  struct addrinfo hints, *res;

  memset(&hints, '\0', sizeof hints); // sikrer oss mot tilfelige problemer
  hints.ai_family   = AF_UNSPEC;   // spesifierer ikke IP versjon
  hints.ai_socktype = SOCK_DGRAM;  // vi ønsker typen «datagram», for UDP
  hints.ai_flags    = AI_PASSIVE;  // bruker addresen til den lokale maskinen

  int error;
  if ((error = getaddrinfo(vert, port, &hints, &res)) != 0) {
    printf("GETADDRINFO(): %s\n", gai_strerror(error));
    return NULL;
  }

  int sockfd;
  if ((sockfd = socket(res->ai_family, res->ai_socktype,
                       res->ai_protocol)) != -1) {
    printf("SOCKET: %d\n", sockfd);
  } else {
    printf("SOCKET: %s\n", strerror(errno));
    return NULL;
  }

  // int true = 1;
  // setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &true, sizeof(int));
  // setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &true, sizeof(int));

  // TODO: Dette virker litt «hacky»; fins det en bedre løsning?
    if (bind(sockfd, res->ai_addr, res->ai_addrlen) == 0) {
      printf("BIND: suksess\n");
    } else {
      printf("BIND: %s.\n", strerror(errno));
      if (errno == 48) {
        printf("      Ignorerer og antar RDP allerede er bundet til porten\n");
        printf("      på denne maskinen.\n");
      } else return NULL;
    }

  struct rdp_connection *session = malloc(sizeof(struct rdp_connection));
  session->sockfd = sockfd;
  session->addr   = res;

  return session;
}

int rdp_write(struct rdp_connection *connection, unsigned char *data, size_t datalen)
{
  struct rdp pakke;
  pakke.flag       = 0x04;
  pakke.pktseq     = 0x0;
  pakke.ackseq     = 0x0;
  pakke.unassigned = 0x0;
  pakke.senderid   = 0x0;
  pakke.recvid     = 0x0;
  pakke.metadata   = 0x0;
  memset(pakke.payload, '\0', sizeof(pakke.payload));
  memcpy(pakke.payload, data, datalen);

  int sendt;
  if ((sendt = sendto(connection->sockfd, (void *) &pakke,
                      sizeof pakke, 0, connection->addr->ai_addr,
                      connection->addr->ai_addrlen)) > 0) {
    printf("SENDTO: sendte %d bytes\n", sendt);
  } else {
    printf("SENDTO: %s\n", strerror(errno));
    return EXIT_FAILURE;
  }
  return 1;
}

int rdp_read(struct rdp_connection *connection)
{
  // Lager buffer `buf` som pakke skal mottas på
  void *buf[sizeof(struct rdp) + 1];
  memset(&buf, '\0', sizeof buf);

  // Informasjon om avsender skal lagres i `from`
  struct sockaddr_storage from;
  memset(&from, '\0', sizeof from);
  unsigned int fromlen = sizeof from;

  // Mottar pakke (BLOKKERER I/O!)
  int recv;
  if ((recv = recvfrom(connection->sockfd, buf, sizeof buf, 0,
                       (struct sockaddr *) &from, &fromlen)) <= 0) {
    printf("RECV: %s\n", strerror(errno));
    return EXIT_FAILURE;
  }

  // Printer pakke
  buf[recv] = '\0';
  printf("RECV: %d bytes: ", recv);
  rdp_print(&buf);

  return EXIT_SUCCESS;
}

int rdp_close(struct rdp_connection *connection)
{
  freeaddrinfo(connection->addr);
  close(connection->sockfd);
  free(connection);
  return EXIT_SUCCESS;
}

#include <stdio.h>
#include <stdlib.h>

#include "rdp.h"

#include <sys/errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/socket.h>

int rdp_print(struct rdp *pakke)
{
  printf("{ flag==%x, pktseq==%x, ackseq==%x, senderid==%x, recvid==%x, metadata==%x, payload==\"%s\" }\n",
         pakke->flag,
         pakke->pktseq,
         pakke->ackseq,
         pakke->senderid,
         pakke->recvid,
         pakke->metadata,
         pakke->payload);
  return 1;
}

struct rdp_connection * rdp_connect(char* port)
{
  // Binder en socket til port. Jeg har lent meg på denne guiden:
  // <https://beej.us/guide/bgnet/html/index-wide.html>

  struct addrinfo hints, *res;

  memset(&hints, '\0', sizeof hints); // sikrer oss mot tilfelige problemer
  hints.ai_family   = AF_UNSPEC;   // spesifierer ikke IP versjon
  hints.ai_socktype = SOCK_DGRAM;  // vi ønsker typen «datagram», for UDP
  hints.ai_flags    = AI_PASSIVE;  // bruker addresen til den lokale maskinen

  int error;
  if ((error = getaddrinfo(NULL, port, &hints, &res)) != 0) {
    printf("GETADDRINFO(): %s\n", gai_strerror(error));
    return NULL;
  }

  int sockfd;
  if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) != -1) {
    printf("SOCKET: %d\n", sockfd);
  } else {
    printf("SOCKET: %s\n", strerror(errno));
    return NULL;
  }

  if (bind(sockfd, res->ai_addr, res->ai_addrlen) == 0) {
    printf("BIND: suksess\n");
  } else {
    printf("BIND: %s\n", strerror(errno));
    return NULL;
  }

  freeaddrinfo(res);

  struct rdp_connection *session = malloc(sizeof(struct rdp_connection));
  session->sockfd = sockfd;

  return session;
}

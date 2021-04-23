// Standardbibliotek.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

// RDP definisjon
#include "rdp.h"


/* MAIN */

int main(int argc, char* argv[])
{
  if (argc != 3) {
    printf("Feil antall argumenter: per nå kun implementert for 3:\n");
    printf("usage: %s <IP/vertsnavn til server> <port>\n", argv[0]);
    return EXIT_FAILURE;
  }

  char* vert = argv[1];
  char* port = argv[2];

  printf("Starter klient.\n");

//  // Kobler en socket til tjener-port. Jeg har lent meg på denne guiden:
//  // <https://beej.us/guide/bgnet/html/index-wide.html>
//
//  struct addrinfo hints, *res;
//
//  memset(&hints, '\0', sizeof hints); // sikrer oss mot tilfelige problemer
//  hints.ai_family   = AF_UNSPEC;      // spesifierer ikke IP versjon
//  hints.ai_socktype = SOCK_DGRAM;     // vi ønsker typen «datagram», for UDP
//  hints.ai_flags    = AI_PASSIVE;     // bruker addresen til den lokale maskinen
//
//  int error;
//  if ((error = getaddrinfo(vert, port, &hints, &res)) != 0) {
//    printf("GETADDRINFO(): %s\n", gai_strerror(error));
//    return EXIT_FAILURE;
//  }
//
//  int sokk;
//  if ((sokk = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) != -1) {
//    printf("SOCKET: %d\n", sokk);
//  } else {
//    printf("SOCKET: %s\n", strerror(errno));
//    return EXIT_FAILURE;
//  }
//
//  // if (bind(sokk, res->ai_addr, res->ai_addrlen) == 0) {
//  //   printf("BIND: suksess\n");
//  // } else {
//  //   printf("BIND: %s\n", strerror(errno));
//  //   return EXIT_FAILURE;
//  // }

  struct rdp_connection *session = rdp_connect(vert, port);
  if (session == NULL) {
    printf("RDP: klarte ikke opprette forbindelse\n");
    return EXIT_FAILURE;
  }

  // SEND RDP-PAKKE
  
  struct rdp pakke;
  pakke.flag       = 0x04;
  pakke.pktseq     = 0x0;
  pakke.ackseq     = 0x0;
  pakke.unassigned = 0x0;
  pakke.senderid   = 0x0;
  pakke.recvid     = 0x0;
  pakke.metadata   = 0x0;
  memset(pakke.payload, '\0', sizeof(pakke.payload));
  memcpy(pakke.payload, "Heisann.", 8);

  // void buf[sizeof(struct rdp)];
  struct addrinfo *to;
  to = session->addr;
  // memset(&to, '\0', sizeof to);
  // unsigned int tolen = sizeof to;

  int sendt;
  if ((sendt = sendto(session->sockfd, (void *) &pakke, sizeof pakke, 0, to->ai_addr, to->ai_addrlen)) > 0) {
    printf("SENDTO: sendte %d bytes\n", sendt);
  } else {
    printf("SENDTO: %s\n", strerror(errno));
    return EXIT_FAILURE;
  }


  // FRIGJØR MINNE

  rdp_close(session);

  return 1;
}

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

int main(int argc, char *argv[])
{
  if (argc != 2) {
    printf("Feil antall argumenter: per nå kun implementert for 2:\n");
    printf("usage: %s <port>\n", argv[0]);
    return EXIT_FAILURE;
  }

  char *port = argv[1];

  printf("Starter server.\n");

  struct rdp_connection *session = rdp_connect(NULL, port, NULL);
  if (session == NULL) {
    printf("RDP: klarte ikke opprette forbindelse\n");
    return EXIT_FAILURE;
  }
  

//  // LYTTE SOCKET
//
//  struct addrinfo hints, *res;
//
//  memset(&hints, '\0', sizeof hints); // sikrer oss mot tilfelige problemer
//  hints.ai_family   = AF_UNSPEC;   // spesifierer ikke IP versjon
//  hints.ai_socktype = SOCK_DGRAM;  // vi ønsker typen «datagram», for UDP
//  hints.ai_flags    = AI_PASSIVE;  // bruker addresen til den lokale maskinen
//
//  int error;
//  if ((error = getaddrinfo(NULL, port, &hints, &res)) != 0) {
//    printf("GETADDRINFO(): %s\n", gai_strerror(error));
//    return EXIT_FAILURE;
//  }
//
//  int sockfd;
//  if ((sockfd = socket(res->ai_family, res->ai_socktype,
//                       res->ai_protocol)) != -1) {
//    printf("SOCKET: %d\n", sockfd);
//  } else {
//    printf("SOCKET: %s\n", strerror(errno));
//    return EXIT_FAILURE;
//  }
//
//  if (bind(sockfd, res->ai_addr, res->ai_addrlen) == 0) {
//    printf("BIND: suksess\n");
//  } else {
//    printf("BIND: %s.\n", strerror(errno));
//    return EXIT_FAILURE;
//  }

  // LYTT og ACCEPT

  printf("LYTTER PÅ PORT %s\n", port);

  rdp_read(session); // blokkerer i/o

  // Prøver å svare:
  //char s[INET6_ADDRSTRLEN];
  //_get_recipient_addr(s, session2);
  struct rdp_connection *session2 = rdp_connect(NULL, NULL, session->recipient);
  if (session2 == NULL) {
    printf("RDP: klarte ikke opprette forbindelse\n");
    return EXIT_FAILURE;
  }

  rdp_write(session2, (unsigned char *) "Her er et svar.", 15);

  // FRIGJØR MINNE

  // rdp_close(session);

  return EXIT_SUCCESS;
}

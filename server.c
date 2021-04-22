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
  if (argc != 2) {
    printf("Feil antall argumenter: per nå kun implementert for 2:\n");
    printf("usage: %s <port>\n", argv[0]);
    return EXIT_FAILURE;
  }

  char* port = argv[1];

  printf("Starter server.\n");

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
    return EXIT_FAILURE;
  }

  int sokk;
  if ((sokk = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) != -1) {
    printf("SOCKET: %d\n", sokk);
  } else {
    printf("SOCKET: %s\n", strerror(errno));
    return EXIT_FAILURE;
  }

  if (bind(sokk, res->ai_addr, res->ai_addrlen) == 0) {
    printf("BIND: suksess\n");
  } else {
    printf("BIND: %s\n", strerror(errno));
    return EXIT_FAILURE;
  }

  printf("LYTTER PÅ PORT %s\n", port);
  while(1) {
    sleep(1);

    void *buf[sizeof(struct rdp) + 1];
    memset(&buf, '\0', sizeof buf);
    struct sockaddr_storage from;
    memset(&from, '\0', sizeof from);
    unsigned int fromlen = sizeof from;

    int recv;
    if ((recv = recvfrom(sokk, buf, sizeof buf, 0,(struct sockaddr *) &from, &fromlen)) > 0) {
      buf[recv] = '\0';
      printf("RECV: %d bytes: ", recv);
      printrdp(&buf);
    } else {
      printf("RECV: %s\n", strerror(errno));
      // return EXIT_FAILURE;
    }

  }

  // FRIGJØR MINNE

  freeaddrinfo(res);

  return 1;
}

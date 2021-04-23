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

  struct rdp_connection *session = rdp_connect(NULL, port);
  if (session == NULL) {
    printf("RDP: klarte ikke opprette forbindelse\n");
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
    if ((recv = recvfrom(session->sockfd, buf, sizeof buf, 0,(struct sockaddr *) &from, &fromlen)) > 0) {
      buf[recv] = '\0';
      printf("RECV: %d bytes: ", recv);
      rdp_print(&buf);
    } else {
      printf("RECV: %s\n", strerror(errno));
      // return EXIT_FAILURE;
    }
  }

  // FRIGJØR MINNE

  rdp_close(session);

  return 1;
}

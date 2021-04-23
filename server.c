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
    rdp_read(session);
  }

  // FRIGJØR MINNE

  rdp_close(session);

  return EXIT_SUCCESS;
}

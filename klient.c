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

  struct rdp_connection *con = rdp_connect(vert, port);
  if (con == NULL) {
    printf("RDP: Klarte ikke opprette forbindelse\n");
    return EXIT_FAILURE;
  }
  printf("RDP: tilkoblet!\n");
  rdp_read(con, NULL);

  // FRIGJØR MINNE

  rdp_close(con);

  return EXIT_SUCCESS;
}

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

  struct rdp_connection *session = rdp_connect(vert, port, NULL);
  if (session == NULL) {
    printf("RDP: Klarte ikke opprette forbindelse\n");
    return EXIT_FAILURE;
  }

  // SEND RDP-PAKKE
   
  struct rdp pakke;
  pakke.flag       = 0x01;
  pakke.pktseq     = 0x0;
  pakke.ackseq     = 0x0;
  pakke.unassigned = 0x0;
  pakke.senderid   = 0x0;
  pakke.recvid     = 0x0;
  pakke.metadata   = 0x0;
  memset(pakke.payload, '\0', sizeof(pakke.payload));
  char s[] = "Heisann.";
  memcpy(pakke.payload, "Hei.", sizeof s);
  rdp_write(session, &pakke);

  // VENT PÅ SVAR

  sleep(1);

  rdp_read(session); // blokkerer i/o

  // FRIGJØR MINNE

  rdp_close(session);

  return EXIT_SUCCESS;
}

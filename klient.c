// RDP definisjon
#include "rdp.h"

#include <time.h>
#include <sys/time.h>

/* MAIN */

int main(int argc, char* argv[])
{
  if (argc != 4) {
    printf("usage: %s <servers IP/vertsnavn> <port> <pakketap>\n", argv[0]);
    return EXIT_FAILURE;
  }

  srand(time(NULL));
  int rnd = rand();

  char* vert = argv[1];                // nettverksaddressen til serveren
  char* port = argv[2];                // RDPs nettverksport
  set_loss_probability(atof(argv[3])); // sett pakketapsannsynlighet

  printf("Starter klient.\n");

  struct rdp_connection *con = rdp_connect(vert, port, rnd);
  if (con == NULL) {
    printf("RDP: Klarte ikke opprette forbindelse\n");
    return EXIT_FAILURE;
  }
  printf("RDP: tilkoblet!\n");

  // MOTTAR PAKKE

  struct rdp pkt;
  bzero(&pkt, sizeof pkt);

  while (pkt.flag != 0x02) {
    rdp_read(con, &pkt);
    rdp_print(&pkt);
  }

  printf("klient: mottokk termineringsforespørsel! Avslutter\n");

  // FRIGJØR MINNE

  rdp_close(con, TRUE);

  return EXIT_SUCCESS;
}

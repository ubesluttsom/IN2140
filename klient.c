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

  char* vert = argv[1];                // nettverksaddressen til serveren
  char* port = argv[2];                // RDPs nettverksport
  set_loss_probability(atof(argv[3])); // sett pakketapsannsynlighet

  struct rdp_connection *con; // datastruktur med oppkoblingsinfo
  struct rdp pkt;             // buffer vi mottar pakker på

  printf("Starter klient.\n");

  srand(time(NULL));                     // seed pseudotilfeldighet
  con = rdp_connect(vert, port, rand()); // prøver å koble til server
  if (con == NULL) {
    printf("klient: klarte ikke opprette forbindelse til server\n");
    return EXIT_FAILURE;
  } else printf("klient: tilkoblet server!\n");


  // MOTTAR PAKKER

  bzero(&pkt, sizeof pkt); // nuller ut pakkebufferet

  // Prøver å lese pakker til vi mottar et koblingsavsluttings flagg
  while (pkt.flag != RDP_TER) {
    rdp_read(con, &pkt); // les pakke fra forbindelsen. BLOKKER I/O!
    rdp_print(&pkt);     // utskrift av pakken vi mottok
  }

  printf("klient: mottok termineringsforespørsel! Avslutter\n");

  // FRIGJØR MINNE

  rdp_close(con, TRUE);

  return EXIT_SUCCESS;
}

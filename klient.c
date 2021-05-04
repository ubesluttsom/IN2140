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
  int payloadlen;             // nyttelast lengde i bytes
  void *rv;                   // generisk returverdi

  // Seed pseudotilfeldighet
  srand(time(NULL));


  // KOBLER TIL SERVER:

  con = rdp_connect(vert, port, rand()); // prøver å koble til server
  if (con == NULL) {
    printf("klient: klarte ikke opprette forbindelse til server\n");
    rdp_close(con, TRUE); // opprydding
    return EXIT_FAILURE;
  } else printf("klient: tilkoblet server!\n");


  // OPPRETTER FIL:

  // Lager (pseudo)tilfeldig filnavn
  char file_name[sizeof("kernel-file-XXXXX")];
  snprintf(file_name, sizeof(file_name), "%s%d", "kernel-file-", rand());

  // Prøver å åpne fil for skriving
  FILE * output_file = fopen(file_name, "wx");
  if (output_file == NULL) {
    rdp_error(-1, "klient");
    fclose(output_file);  // opprydding
    rdp_close(con, TRUE); // opprydding
    exit(EXIT_FAILURE);
  }


  // MOTTAR PAKKER:

  bzero(&pkt, sizeof pkt); // nuller ut pakkebufferet

  // Prøver å lese pakker til vi mottar et koblingsavsluttingsflagg
  while (pkt.flag != RDP_TER) {
    rv = rdp_read(con->sockfd, &con, 1, &pkt); // les pakke. BLOKKER I/O.
    rdp_print(&pkt);                           // utskrift av pakken vi mottok
    
    // Filtrer ut pakkene vi ikke skal skrive til fil
    if (rv == NULL) continue;
    if (pkt.flag != RDP_DAT) continue;

    // Finner lengde på nyttelasten i pakken
    // TODO: dette er stygt. Skille ut i egen funksjon i `rdp.c`?
    payloadlen = pkt.metadata - sizeof(pkt.flag)   - sizeof(pkt.pktseq)
                              - sizeof(pkt.ackseq) - sizeof(pkt.senderid)
                              - sizeof(pkt.recvid) - sizeof(pkt.metadata);
    for (int i = 0; i < payloadlen; i++) {
      // Skriver nyttelast til fil, byte for byte
      fputc(pkt.payload[i], output_file);
    }
  }

  printf("klient: mottok termineringsforespørsel! Avslutter\n");


  // FRIGJØR MINNE:

  fclose(output_file);
  rdp_close(con, TRUE);

  return EXIT_SUCCESS;
}

// RDP definisjon
#include "rdp.h"

#include <sys/time.h>
#include <time.h>

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
  int klient_id;              // ønsket ID, argument til `rdp_connect()`
  int server_id;              // tildelt ID fra `rdp_connect()`
  uint8_t buf[1000];          // buffer vi mottar pakker på
  int rc;                     // «read count»
  struct pollfd pfds[1];      // fildesk. vi vil at `poll` skal sjekke
  int pkt_pending;            // boolsk, om en pakke venter i fildesk.

  // Seed pseudotilfeldighet
  srand(time(NULL));


  // OPPRETTER FIL:

  // Lager (pseudo)tilfeldig filnavn
  char file_name[sizeof("kernel-file-XXXXX")];
  snprintf(file_name, sizeof(file_name), "%s%d", "kernel-file-", rand());

  // Prøver å åpne fil for skriving
  FILE * output_file = fopen(file_name, "wx");
  if (output_file == NULL) {
    rdp_error(-1, "klient");
    fclose(output_file);  // opprydding
    exit(EXIT_FAILURE);
  }


  // KOBLER TIL SERVER:

  klient_id = rand();                       // velg tilfeldig ID
  con = rdp_connect(vert, port, klient_id); // prøver å koble til server

  if (con == NULL) {

    printf("NOT CONNECTED %d <?>\n", klient_id);
    rdp_close(con, TRUE); // opprydding

    return EXIT_FAILURE;

  }

  server_id = ntohl(con->recvid);
  printf("CONNECTED %d %d\n", klient_id, server_id);

  pfds->fd = con->sockfd; // vi vil at `poll` skal lytte på dette støpselet
  pfds->events = POLLIN;  // vi overvåker om vi kan *lese* (ikke skrive)


  // MOTTAR PAKKER:

  bzero(&buf, sizeof buf); // nuller ut pakkebufferet


  // Prøver å lese pakker til forbindelsen avslutter seg selv. `rdp_read()`
  // returnerer antall leste bytes, RDP_READ_TER ved forbindelsesavsluttning
  // (eller andre feil/infomeldinger).

  rc = 0; 

  while (rc != RDP_READ_TER) {

    // Vi venter på pakker på ubestemt tid
    pkt_pending = poll(pfds, 1, -1);
    if (rdp_error(pkt_pending, "klient: poll")) return EXIT_FAILURE;

    rc = rdp_read(con->sockfd, &con, 1, &buf); // les pakke. BLOKKER I/O.
    
    // Hopp over hvis pakke ikke hadde noe leselig data (dvs. nyttelast)
    if (rc < 0) continue;

    // Avslutt hvis vi har fått en tom datapakke;
    if (rc == 0) break;

    // Skriver eventuell nyttelast til fil, byte for byte
    for (int i = 0; i < rc; i++) {
      fputc(buf[i], output_file);
    }
  }

  printf("DISCONNECTED %d %d\n", klient_id, server_id);
  printf("%s\n", file_name);


  // FRIGJØR MINNE:

  fclose(output_file);
  rdp_close(con, TRUE);

  return EXIT_SUCCESS;
}

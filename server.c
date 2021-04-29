// RDP definisjon
#include "rdp.h"

struct rdp_connection *check_id(int id, struct rdp_connection *cons[])
{
  // TODO: IMPLEMENTER MEG!
}

/* MAIN */

int main(int argc, char *argv[])
{
  if (argc != 4) {
    printf("Feil antall argumenter: per nå implementert for 3:\n");
    printf("usage: %s <port> <filnavn> <N-klienter>\n", argv[0]);
    printf("(<filnavn> er ikke i bruk ennå)");
    return EXIT_FAILURE;
  }

  char *port  = argv[1];
  const int N = atoi(argv[3]); // maks klienter
  int n       = 0;             // faktisk antall klienter

  int listen_fd;                   // fildesk. til nettverksstøpsel vi avlytter
  struct rdp_connection *cons[N];  // holder koblinger til alle klienter

  // TODO: Lag funksjon som kapper opp på forhånd filen inn i pakker. Kanskje
  // samle disse inn i et array indeksert av sekvensnummer, eller no’?

  printf("Starter server.\n");

  listen_fd = rdp_listen(port);
  if (listen_fd == EXIT_FAILURE) return EXIT_FAILURE; 

  // Nuller ut arrayet som skal holde koblingene våre.
  memset(&cons, '\0', sizeof cons);

  // Venter på pakke ...
  struct rdp pkt_buf;
  struct rdp_connection *new_con;

  for (int i = 0; i < 10; i++) { // MIDLERTIDIG

    // Sniktitt på pakke
    rdp_peek(listen_fd, &pkt_buf, NULL, NULL);

    // Hvis forbindelsesforespørsel:
    if (pkt_buf.flag == 0x01) {
      // Godta forbindelse om det er plass. Bruker `n` for å telle plassene.
      new_con = rdp_accept(listen_fd, n < N ? TRUE : FALSE, n);
      if (new_con != NULL) {
        cons[n] = new_con;
        n++;
      }
    }

    // Hvis forbindelsesavsluttning:
    if (pkt_buf.flag == 0x02) {
      cons[ntohl(pkt_buf.senderid)] = NULL;
    }

    // Hvis data/nyttelast:
    if (pkt_buf.flag == 0x04) {
      // Øh ... dette bør ikke skje? TODO: «rdp_kast_pakke()», eller noe
      // sånt? Kan eventuelt modde `rdp_read()` funksjonen.
    } 

    // Hvis ACK:
    if (pkt_buf.flag == 0x08) {
      // Per nå, fikser `rdp_read()` dette. TODO: skille ut i egen funksjon?
      rdp_read(cons[ntohl(pkt_buf.senderid)], NULL);
    } 

    // Prøver nå å sende data til alle koblinger.
    for (int j = 0; j < N; j++) {
      // Hopp over ikke-eksisterende koblinger
      if (cons[j] == NULL) continue;

      // Lager pakke:
      bzero(&pkt_buf, sizeof pkt_buf);
      // memset(&pkt_buf, '\0', sizeof pkt_buf);
      pkt_buf.flag     = 0x04;
      pkt_buf.senderid = cons[j]->senderid;
      pkt_buf.recvid   = cons[j]->recvid;
      strcpy((char *)pkt_buf.payload, "... data ...");

      // Sender pakke:
      rdp_write(cons[j], &pkt_buf);
    }
  }

  // FRIGJØR MINNE

  close(listen_fd);
  rdp_close(new_con);

  return EXIT_SUCCESS;
}

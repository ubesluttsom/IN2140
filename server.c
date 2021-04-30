// RDP definisjon
#include "rdp.h"

int id_to_idx(int id, struct rdp_connection *cons[], int conslen)
{
  for (int i = 0; i < conslen; i++) {
    if (cons[i] == NULL) continue;
    // Hvis vi finner ID-en, returner indeksen i arrayet
    if (cons[i]->recvid == id) return i;
  }
  // Hvis ID-en ikke finnes i koblingene våre, returner -1;
  return -1;
}

// size_t parse_file(char *path,            // filsti til hvor filen ligget
//                   uint8_t *data_dest[])  // peker til hvor data skal lagres
// {
//   // TODO: Funksjon for å lese en fil inn i minne. Spesifikt et indeksert
//   // array `fildata`. Dette arrayet kan dermed brukes med en «stride»-faktor,
//   // alla `fildata[1000 * con->pktseq]` for å hente nyttelasten til pakke
//   // nummer `pktseq`. Dette kan gjøres på forhånd siden alle klienter skal
//   // sendes samme fil. RETUR: `sizeof data_dest[]`.
// }

int mk_next_pkt(struct rdp_connection *con, // koblingen som skal sendes over
                uint8_t data[],             // array med *alt* som skal sendes
                size_t datalen,             // lengde på data array (bytes)
                struct rdp *pkt)            // peker hvor pakke skal lagres
{
  // TODO: Funksjon som lager neste pakke som skal sendes over kobling til
  // klient.

  int wc;            // «write count»
  size_t payloadlen; // lengde på payload i RDP pakke

  // Henter nyttelastlengde.
  payloadlen = sizeof(pkt->payload);

  if (payloadlen*(con->pktseq) >= datalen) {
    // Hvis «pakker sendt» ganger lengden på nyttelasten overskrider totalt
    // antall bytes vi ønsker å sende (`datalen`) ... har noe galt skjedd
    printf("server: mk_next_pkt: Panic!\n");
    return EXIT_FAILURE;
  } else if (payloadlen*(con->pktseq) + payloadlen > datalen) {
    // Sjekker om vi står i fare for å overskride `data` arrayet. Vi
    // tilpasser «write count» deretter.
    wc = datalen - payloadlen*(con->pktseq);
  } else {
    // Ellers, fyller vi ut hele nyttelasten til pakken.
    wc = payloadlen;
  }

  // Lager pakke
  pkt->flag     = 0x04;
  pkt->senderid = con->senderid;
  pkt->recvid   = con->recvid;
  pkt->metadata = wc;

  // Her brukes `payloadlen` som «stride»-faktor og `con->pktseq` som indeks
  // i fildata arrayet.
  memcpy(pkt->payload, &data[payloadlen*(con->pktseq)], wc);

  return EXIT_SUCCESS;
}

int terminate(struct rdp_connection *con)
{
  struct rdp pkt;
  
  bzero(&pkt, sizeof pkt);
  pkt.flag     = 0x02; // <-- termflagg
  pkt.senderid = con->senderid;
  pkt.recvid   = con->recvid;

  if (rdp_write(con, &pkt)) return EXIT_FAILURE;
  // Frigjør minne i forbinnelsen, men IKKE lukk fildiskriptoren!
  if (rdp_close(con, FALSE)) return EXIT_FAILURE;
  return EXIT_SUCCESS;
}

/********/
/* MAIN */
/********/

int main(int argc, char *argv[])
{
  if (argc != 4) {
    printf("Feil antall argumenter: per nå implementert for 3:\n");
    printf("usage: %s <port> <filnavn> <N-klienter>\n", argv[0]);
    printf("(<filnavn> er ikke i bruk ennå)");
    return EXIT_FAILURE;
  }

  char *port  = argv[1];       // nettverksporten vi vil bruke
  const int N = atoi(argv[3]); // maks klienter
  int n       = 0;             // faktisk antall klienter

  // MIDLERTIDIGE DATAVERDIER: {
  uint8_t data[] = " ... data ... ";
  size_t datalen = 14;
  // }

  int listen_fd;                   // fildesk. til nettverksstøpsel vi avlytter
  struct rdp_connection *cons[N];  // holder koblinger til alle klienter
  int idx;                         // indeks til en forbindelse i `cons[]`

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
    idx = id_to_idx(pkt_buf.senderid, cons, N);

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
    else if (idx > -1 && pkt_buf.flag == 0x02) {
      printf("server: mottokk forbinnelsesavslutting. Terminerer kobling\n");
      free(cons[idx]);
      cons[idx] = NULL;
    }

    // Hvis ACK:
    else if (idx > -1 && pkt_buf.flag == 0x08) {
      // Per nå, fikser `rdp_read()` dette. TODO: skille ut i egen funksjon?
      printf("server: mottokk ACK\n");
      rdp_read(cons[idx], NULL);
      // TODO: hvis dette er ACK på siste pakke må forbindelsesavsluttning
      // sendes! Alla:
      if ( (sizeof(pkt_buf.payload))*(cons[idx]->pktseq ) >= datalen ) {
        // Altså, hvis vi med siste pakke har (i teorien) skrevet all data,
        // eller mer, terminer kobling.
        printf("server: siste pakke til %d er ACK-et. Terminerer kobling\n",
                ntohl(cons[idx]->recvid));
        terminate(cons[idx]);
        cons[idx] = NULL;
      }
    } 

    // Hvis annen pakke:
    else if (idx > -1) {
      // Øh ... dette bør ikke skje? TODO: «rdp_kast_pakke()», eller noe
      // sånt? Kan eventuelt modde `rdp_read()` funksjonen.
      int rc = recv(cons[idx]->sockfd, &pkt_buf, sizeof pkt_buf, 0);
      rdp_error(rc, "server: rdp_trash: recvfrom");
      printf("server: mottokk uventet pakke: \n");
      rdp_print(&pkt_buf);
    } 

    // Prøver nå å sende «data» til alle koblinger.
    for (int j = 0; j < N; j++) {
      // Hopp over ikke-eksisterende koblinger
      if (cons[j] == NULL) continue;

      // Lager neste pakke som vi ønsker å sende:
      bzero(&pkt_buf, sizeof pkt_buf);
      mk_next_pkt(cons[j], data, datalen, &pkt_buf);
      // Sender pakke:
      rdp_write(cons[j], &pkt_buf);
    }
  }

  // FRIGJØR MINNE

  for (int i = 0; i < N; i++) {
    if (cons[i] == NULL) continue;
    else terminate(cons[i]);
  }

  // Lukker nettverksstøpsel.
  close(listen_fd);

  return EXIT_SUCCESS;
}

// RDP definisjon
#include "rdp.h"

#include <poll.h>

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
//   // array `data_dest`. Dette arrayet kan dermed brukes med en
//   // «stride»-faktor, alla `data_dest[1000 * con->pktseq]` for å hente
//   // nyttelasten til pakke nummer `pktseq`. Dette kan gjøres på forhånd
//   // siden alle klienter skal sendes samme fil. RETUR: `sizeof data_dest[]`.  
// }

int mk_next_pkt(struct rdp_connection *con, // koblingen som skal sendes over
                uint8_t data[],             // array med *alt* som skal sendes
                size_t datalen,             // lengde på data array (bytes)
                struct rdp *pkt)            // peker hvor pakke skal lagres
{
  // TODO: Funksjon som lager neste pakke som skal sendes over kobling til
  // klient. Denne bør kanskje flyttes til transportlaget, og slås sammen
  // med `rdp_write` i `rdp.c`?

  int wc;            // «write count»
  size_t payloadlen; // lengde på payload i RDP pakke

  // Henter nyttelastlengde. TODO: endre til «flexible array member».
  payloadlen = sizeof(pkt->payload);

  // Sekvensnummeret til første pakke er 0 (`con->ackseq` initialiseres til
  // -1 i `rdp_accept`), og generelt sender vi alltid pakken etter siste ACK
  // vi mottok.
  pkt->pktseq = con->ackseq + 1;

  if (payloadlen*(pkt->pktseq) >= datalen) {
    // Hvis «pakker sendt» ganger lengden på nyttelasten overskrider totalt
    // antall bytes vi ønsker å sende (`datalen`) ... har noe galt skjedd
    printf("server: mk_next_pkt: Panic!\n");
    return EXIT_FAILURE;
  } else if (payloadlen*(pkt->pktseq) + payloadlen > datalen) {
    // Sjekker om vi står i fare for å overskride `data` arrayet. Vi
    // tilpasser «write count» deretter.
    wc = datalen - payloadlen*(pkt->pktseq);
  } else {
    // Ellers, fyller vi ut hele nyttelasten til pakken.
    wc = payloadlen;
  }

  // Lager pakke
  pkt->flag     = RDP_DAT;
  pkt->senderid = con->senderid;
  pkt->recvid   = con->recvid;
  // Finner total pakkelengde. TODO: endre til «flexible array member».
  pkt->metadata =   sizeof(pkt->flag)   + sizeof(pkt->pktseq)
                  + sizeof(pkt->ackseq) + sizeof(pkt->senderid)
                  + sizeof(pkt->recvid) + sizeof(pkt->metadata)
                  + wc;

  // Her brukes `payloadlen` som «stride»-faktor og `pkt->pktseq` som indeks
  // i fildataarrayet.
  memcpy(pkt->payload, &data[payloadlen*(pkt->pktseq)], wc);

  return EXIT_SUCCESS;
}

int terminate(struct rdp_connection *con)
{
  struct rdp pkt;
  
  bzero(&pkt, sizeof pkt);
  pkt.flag     = RDP_TER; // <-- termflagg
  pkt.senderid = con->senderid;
  pkt.recvid   = con->recvid;
  pkt.pktseq   = con->ackseq + 1;

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
  if (argc != 5) {
    printf("usage: %s <port> <filnavn> <N-klienter> <pakketap>\n", argv[0]);
    return EXIT_FAILURE;
  }

  char *port  = argv[1];               // nettverksporten vi vil bruke
  char *path  = argv[2];               // filsti til fil vi skal sende
  const int N = atoi(argv[3]);         // maks klienter
  int n       = 0;                     // faktisk antall klienter
  set_loss_probability(atof(argv[4])); // sett pakketapsannsynlighet


  // FILINNLESING:

  uint8_t *data;   // data-array med alt som skal sendes
  size_t datalen;  // hvor mye data som skal sendes i bytes
  FILE* data_file; // filpeker

  // Åpner fil
  data_file = fopen(path, "r");
  if (data_file == NULL) {
    rdp_error(-1, "server: parse_file");
    exit(EXIT_FAILURE);
  }

  // Finner lengde på fil, `datalen`
  fseek(data_file, 0, SEEK_END);
  datalen = ftell(data_file);
  fseek(data_file, 0, SEEK_SET);

  // Allokerer minne til all data
  data = malloc(datalen);
  
  // Leser igjennom hele filen og skriver hver byte til data arrayet.
  for (int i = 0; fread(&data[i], sizeof(uint8_t), 1, data_file); i++);


  // ANDRE VARIABLER:

  int listen_fd;                  // fildesk. til nettverksstøpsel vi avlytter
  struct rdp_connection *cons[N]; // holder koblinger til alle klienter
  struct rdp_connection *new_con; // peker til en potensiell ny forbindelse
  int idx;                        // indeks til en forbindelse: `cons[idx]`
  struct rdp pkt_buf;             // pakkebuffer til ymse bruk i hovedløkka
  struct pollfd pfds[1];          // fildesk. vi vil at `poll` skal sjekke
  int pkt_pending;                // boolsk, om en pakke venter i fildesk.

  // Nuller ut arrayet som skal holde alle koblingene våre.
  bzero(&cons, sizeof cons);


  // LISTEN:

  // Oppretter nettverksstøpsel som det skal lyttes på
  listen_fd = rdp_listen(port);
  if (listen_fd == EXIT_FAILURE) return EXIT_FAILURE; 
  pfds->fd = listen_fd;  // vi vil at `poll` skal lytte på dette støpselet
  pfds->events = POLLIN; // vi overvåker om vi kan *lese* (ikke skrive)


  // HOVEDLØKKE:

  for (int i = 0; i < 10000000; i++) { // MIDLERTIDIG

    // Bruker 100 ms til å sjekke om det venter noen pakker. TODO: jeg er
    // usikker på om det er ideelt å bruke `poll()` her, med hensyn på CPU-en?
    pkt_pending = poll(pfds, 1, 100);
    if (rdp_error(pkt_pending, "server: poll")) return EXIT_FAILURE;

    if (pkt_pending) {
      // SNIKTITT PÅ PAKKE:
      // TODO: dette blokkerer I/O, og bør heller implementeres med
      // `select()`, eller så må `recvfrom()` i `rdp_peek()` endes til
      // ikke-blokkerende.
      rdp_peek(listen_fd, &pkt_buf, NULL, NULL);
      idx = id_to_idx(pkt_buf.senderid, cons, N);
    }

    // TOLKER PAKKE:

    // Hvis forbindelsesforespørsel:
    if (pkt_pending && pkt_buf.flag == RDP_REQ) {
      printf("server: forbindelsesforespørsel mottatt\n");
      // Godta forbindelse om bare hvis etterspurt ID ikke er i bruk,
      // og det er plass. Bruker `n` for å telle plassene.
      if (id_to_idx(pkt_buf.senderid, cons, N) != -1) {
        printf("server: forbindelsesforespørsel avslått: ID er i bruk\n");
        new_con = rdp_accept(listen_fd, NFSP_INVALID, n);
      } else if (n == N) { 
        printf("server: forbindelsesforespørsel avslått: ingen kapasitet\n");
        new_con = rdp_accept(listen_fd, NFSP_CONFULL, n);
      } else {
        new_con = rdp_accept(listen_fd, TRUE, n);
      }
      if (new_con != NULL) {
        cons[n] = new_con;
        n++;
      }
    }

    // Hvis forbindelsesavslutting:
    else if (pkt_pending && idx > -1 && pkt_buf.flag == RDP_TER) {
      printf("server: mottok forbindelsesavslutting. Terminerer kobling\n");
      free(cons[idx]);
      cons[idx] = NULL;
    }

    // Hvis ACK:
    else if (pkt_pending && idx > -1 && pkt_buf.flag == RDP_ACK) {
      // Per nå, fikser `rdp_read()` dette. TODO: skille ut i egen funksjon?
      printf("server: mottokk ACK %d fra %d\n",
             pkt_buf.ackseq, ntohl(cons[idx]->recvid));
      rdp_read(cons[idx], &pkt_buf);

      // Hvis dette er ACK på siste pakke må forbindelsesavsluttning
      if ( (sizeof(pkt_buf.payload))*(cons[idx]->ackseq + 1) >= datalen ) {
        // Altså, hvis vi med siste pakke har (i teorien) skrevet all data,
        // eller mer, skal kobling termineres.
        printf("server: siste pakke til %d er ACK-et. Terminerer kobling\n",
                ntohl(cons[idx]->recvid));
        terminate(cons[idx]);
        cons[idx] = NULL;
      }
    } 

    // Hvis annen pakke:
    else if (pkt_pending && idx > -1) {
      // Øh ... dette bør ikke skje? TODO: «rdp_kast_pakke()», eller noe
      // sånt? Kan eventuelt modde `rdp_read()` funksjonen.
      int rc = recv(cons[idx]->sockfd, &pkt_buf, sizeof pkt_buf, 0);
      rdp_error(rc, "server: rdp_trash: recvfrom");
      printf("server: mottokk uventet pakke: \n");
      rdp_print(&pkt_buf);
    } 

    // SEND DATA:

    // Prøver nå å sende «data» til alle koblinger.
    for (int j = 0; j < N; j++) {
      // Hopp over ikke-eksisterende koblinger
      if (cons[j] == NULL) continue;

      // TODO: må implementere en tidstaker eller hvileperiode av noe slag.
      // Nå bare gjentas pakker igjen og igjen, uten å gi klienten tid til å
      // ACKe dem.

      // Lager neste pakke som vi ønsker å sende:
      bzero(&pkt_buf, sizeof pkt_buf);
      mk_next_pkt(cons[j], data, datalen, &pkt_buf);
      // Sender pakke:
      rdp_write(cons[j], &pkt_buf);
    }

    // FERDIG?

    if (n == N) {

      // Sjekker om vi har tjent `N` klienter og terminert alle forbindelser
      // ved å se om koblingsarrayet `cons[]` kun har NULL pekere.
      int j = 0;
      while ( j < N && cons[j] == NULL ) j++; 
      if (j == N) {
        
        // AVSLUTT. FRIGJØR MINNE.

        printf("server: har tjent N==%d klienter. Avslutter server.\n", N);
        for (j = 0; j < N; j++) {
          if (cons[j] == NULL) continue;
          else terminate(cons[i]);
        }

        free(data);

        // Lukker nettverksstøpsel.
        close(listen_fd);
        return EXIT_SUCCESS;
      }
    }
  }

  return EXIT_FAILURE; // <-- Bør aldri nåes.
}

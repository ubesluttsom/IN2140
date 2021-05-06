// RDP definisjon
#include "rdp.h"

#include <time.h>
#include <sys/time.h>


/********/
/* MAIN */
/********/

int main(int argc, char *argv[])
{
  if (argc != 5) {
    printf("usage: %s <port> <filnavn> <N-klienter> <pakketap>\n", argv[0]);
    return EXIT_FAILURE;
  }


  //  ARGUMENTER:

  char *port  = argv[1];               // nettverksporten vi vil bruke
  char *path  = argv[2];               // filsti til fil vi skal sende
  const int N = atoi(argv[3]);         // maks klienter
  int n       = 0;                     // faktisk antall klienter *nå*
  set_loss_probability(atof(argv[4])); // sett pakketapsannsynlighet


  // FILINNLESING:

  uint8_t *data;   // data-array med alt som skal sendes
  size_t datalen;  // hvor mye data som skal sendes i bytes
  FILE* data_file; // filpeker

  // Åpner fil
  data_file = fopen(path, "r");
  if (data_file == NULL) {
    rdp_error(-1, "server");
    exit(EXIT_FAILURE);
  }

  // Finner lengde på fil, `datalen`
  fseek(data_file, 0, SEEK_END);
  datalen = ftell(data_file);
  fseek(data_file, 0, SEEK_SET);

  // Allokerer minne til all data
  data = malloc(datalen);
  
  // Leser igjennom hele filen og skriver hver byte til dataarrayet.
  for (int i = 0; fread(&data[i], sizeof(uint8_t), 1, data_file); i++);

  // Ferdig med filen nå
  fclose(data_file);


  // ANDRE VARIABLER:

  int listen_fd;                  // fildesk. til nettverksstøpsel vi avlytter
  struct rdp_connection *cons[N]; // holder koblinger til alle klienter
  struct rdp_connection *new_con; // peker til en potensiell ny forbindelse
  int accept_flag;                // «value–return» flagg til `rdp_accept()`
  int server_id;                  // tildelt server ID for forbindelse
  int klient_id;                  // returnert klient ID for forbindelse
  struct pollfd pfds[1];          // fildesk. vi vil at `poll` skal sjekke
  int pkt_pending;                // boolsk, om en pakke venter i fildesk.
  int wc_total;                   // «write count» for `rdp_write()`

  // Nuller ut arrayet som skal holde alle koblingene våre.
  bzero(&cons, sizeof cons);


  // LISTEN:

  // Oppretter nettverksstøpsel som det skal lyttes på
  listen_fd = rdp_listen(port);
  if (listen_fd == 0) return EXIT_FAILURE; 

  pfds->fd = listen_fd;  // vi vil at `poll` skal lytte på dette støpselet
  pfds->events = POLLIN; // vi overvåker om vi kan *lese* (ikke skrive)


  // Seed pseudotilfeldighet
  srand(time(NULL));


  // HOVEDLØKKE:

  while (1) {

    // MOTTA EVENTUELLE PAKKER:

    // Bruker 100 ms til å sjekke om det venter noen pakker. 100 ms er tiden
    // oppgaven ønsker å vente på ACK, før pakker sendes på nytt.

    pkt_pending = poll(pfds, 1, 100);
    if (rdp_error(pkt_pending, "server: poll")) return EXIT_FAILURE;
    if (pkt_pending) {

      // `rdp_read()` tolker innholdet. Hvis den beslutter pakken er en
      // forbindelsesforespørsel gjøres ingenting, uten å fjerne pakken fra
      // støpselet. Ellers hvis avsender er ukjent, kastes pakken. Ellers,
      // vil den prøve å respondere riktig avhengig av flagg og avsender,
      // eksempelvis ACK eller avslutningsforespørsel.

      rdp_read(listen_fd, // les pakker herfra
               cons,      // array med koblinger
               N,         // lengde på array med koblinger
               NULL);     // er NULL siden lagring av pakke er unødvendig
    }

    // Sjekker om det fortsatt finnes en ubehandlet pakke i støpselet. Dette
    // er nødvendig for å forhindre at `rdp_accept()` potensielt blokkerer
    // i det uendelige. (Kunne også sjekket returverdien til `rdp_read()`)

    pkt_pending = poll(pfds, 1, 0);
    if (rdp_error(pkt_pending, "server: poll")) return EXIT_FAILURE;
    if (pkt_pending) {

      // Leser kun forbindelsesforespørsel, eller returneres NULL, uten å
      // fjerne pakke fra støpsel. Hvis det er kapasitet, godta forespørsler.

      accept_flag = n < N ? TRUE : RDP_CONFULL;
      server_id = rand();   // tildel tilfeldig ID
      klient_id = 0;        // argument returvariabel

      new_con = rdp_accept(listen_fd,     // forespørsler herfra
                           cons,          // array med koblinger
                           N,             // lengde på `cons[]`
                           &accept_flag,  // skal vi godta? «value–return»
                           &server_id,    // tildel denne ID-en
                           &klient_id);   // før inn klientens ID her

      if (accept_flag == RDP_NOTAREQ) {

        // Hvis pakken i støpselet ikke var en forbindelsesforespørsel,
        // bryt if-else løkke; gjør ingenting.

      } else if (accept_flag != TRUE) {

        printf("NOT CONNECTED %d %d\n", ntohl(server_id), ntohl(klient_id));

      } else if (new_con != NULL) {

        // Hvis det ble opprettet en forbindelse, sett den inn i array, og
        // øk antall forbindelser `n`

        cons[n] = new_con;
        n++;

        printf("CONNECTED %d %d\n", ntohl(server_id), ntohl(klient_id));

      }
    } 


    // SEND DATA:

    // Hvis vi ikke mottok noen pakker, ble timeouten på 100 ms utløst, så vi
    // prøver å skrive neste (eller gjenta) pakker til alle forbindelser.
    // Så lenge det finnes koblinger i `cons` vil `rdp_write()` skrive *noe*,
    // om det så er gjentatte ikke-ACK-ede pakker.

    // Teller antall bytes skrevet til koblinger i denne
    // hovedløkkeiterasjonen:

    wc_total = 0;

    // Prøver nå å sende «data» til alle koblinger.

    for (int wc, i = 0; i < N; i++) {

      if (cons[i] == NULL) continue;   // kun eksisterende koblinger

      wc = rdp_write(cons[i], data, datalen);   // prøv å skrive

      if (wc) wc_total += wc;   // sum opp bytes skrevet

      else {

        // Hvis det ikke ble skrevet noe, altså siste datapakke hadde lengde
        // 0, termineres kobling

        printf("DISCONNECTED %d %d\n",
               ntohl(cons[i]->recvid),
               ntohl(cons[i]->senderid));

        rdp_terminate(cons[i]);    // send terminering
        rdp_close(cons[i], FALSE); // lukker kobling, men IKKE fildesk.
        cons[i] = NULL;            // fjern fra koblingsarray

      }
    }


    // FERDIG?

    if (wc_total != 0) continue;   // hvis vi skrev noe, er vi ikke ferdige

    if (n == N) {

      // Altså, hvis vi ikke har skrevet noe. Og har hatt `N` koblinger.

      // AVSLUTT.

      printf("server: har tjent N==%d klienter. Avslutter server\n", N);
      free(data);       // frigjør minne
      close(listen_fd); // lukk nettverksstøpsel

      return EXIT_SUCCESS;
    }


    // ELLERS, VENT PÅ UBESTEMT TID:

    else {

      // Hvis ingen av de øvrige tilfellene har blitt utløst, ventes det på
      // nye pakker i det uendelige.
      pkt_pending = poll(pfds, 1, -1);
      if (rdp_error(pkt_pending, "server: poll")) return EXIT_FAILURE;

    }

  }   // Slutt: hovedløkke.

  return EXIT_FAILURE; // <-- Bør aldri nåes.
}

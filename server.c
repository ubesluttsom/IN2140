// RDP definisjon
#include "rdp.h"

#include <poll.h>


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

  while (1) {

    // Bruker 100 ms til å sjekke om det venter noen pakker. 100 ms er tiden
    // oppgaven ønsker å vente på ACK, før pakker sendes på nytt. Jeg sender
    // alle pakker på nytt i hver iterasjon av hovedløkka når det ikke er noen
    // pakker å lese uansett (som kanskje er litt dumt, siden dette sikkert
    // sender mer data enn nødvendig). TODO: Jeg er usikker på om det er
    // ideelt å bruke `poll()` her, med hensyn på CPU-en?
    pkt_pending = poll(pfds, 1, 100);
    if (rdp_error(pkt_pending, "server: poll")) return EXIT_FAILURE;


    // MOTTA EVENTUELLE PAKKER:

    if (pkt_pending) {

      // Hvis det er kapasitet, godta forespørsler
      new_con = rdp_accept(listen_fd, cons, N, n < N ? TRUE : RDP_CONFULL, n);

      if (new_con != NULL) {

        // Hvis det ble opprettet en forbindelse, sett den inn i array, og
        // øk antall forbindelser `n`

        cons[n] = new_con;
        n++;

      } else {

        // Ellers leses pakken, hvor `rpd_read()` tolker innholdet. Siden
        // serveren ikke trenger å lagre pakker den mottar noe sted, kan siste
        // argument settes til `NULL`
        rdp_read(listen_fd, cons, N, NULL);

      }
    } 


    // SEND DATA:

    else rdp_write(cons, N, data, datalen);


    // FERDIG?

    if (n == N) {

      // Sjekker om vi har tjent `N` klienter og terminert alle forbindelser
      // ved å se om koblingsarrayet `cons[]` kun har NULL pekere.

      int j = 0;
      while ( j < N && cons[j] == NULL ) j++; 
      if (j == N) {
        
        // AVSLUTT.

        printf("server: har tjent N==%d klienter. Avslutter server\n", N);
        free(data);       // frigjør minne
        close(listen_fd); // lukk nettverksstøpsel

        return EXIT_SUCCESS;
      }
    }
  }

  return EXIT_FAILURE; // <-- Bør aldri nåes.
}

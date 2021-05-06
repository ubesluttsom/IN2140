#include "rdp.h"

#define RED "\e[0;31m"
#define RESET "\e[0m"


/**
 * Skriver en pakke til terminalen.
 */
int rdp_print(struct rdp *pkt)
{
  printf("{ ");
  printf("flag==0x%x, ",          pkt->flag);
  printf("pktseq==0x%x, ",        pkt->pktseq);
  printf("ackseq==0x%x, ",        pkt->ackseq);
  printf("senderid==0x%x, ",      ntohl(pkt->senderid));
  printf("recvid==0x%x, ",        ntohl(pkt->recvid));
  printf("metadata==0x%x, ",      ntohl(pkt->metadata));

  // Passer på å terminere `payload` med `\0` før utskrift. Her lager jeg
  // en ny strengbuffer som jeg kopierer over til.
  char str[rdp_payloadlen(pkt) + 1];
  str[rdp_payloadlen(pkt)] = '\0';
  strncpy(str, (char *)pkt->payload, rdp_payloadlen(pkt));

  printf("payload==\"%s\" }\n", str);

  return 1;
}


/**
 * Skriver ut feilmeldinger fra `errno`.
 */
int rdp_error (int rv, char *msg)
{
  if (rv < 0) {
    fprintf(stderr, "%s%s: %s%s\n", RED, msg, strerror(errno), RESET);
    return 1;
  }

  return 0;
}


/**
 * Svar på forbindelsesforespørsel. Vil svare avhengig av flagget som sendes
 * inn `accept`; funksjonen kan også avslå basert på forespørselspakken som
 * mottas, og vil da skrive over `accept` med årsaken. Koblingen vil bli
 * tildelt verdien som sendes med `senderid`; `recvid` avgjøres av funksjonen
 * selv. `senderid` og `recvid` returneres i nettverksbyterekkefølge.
 */
struct rdp_connection *rdp_accept(int sockfd,    // støpsel som avlyttes
                                  struct rdp_connection *cons[], // koblinger
                                  int conslen,   // lengde på koblingsarray
                                  int *accept,   // «value–return» flagg
                                  int *senderid, // tildel denne sender ID-en
                                  int *recvid)   // lagre mottaker ID her
{
  int rc, wc;                        // read / write count
  void *buf[sizeof(struct rdp)];     // lest data
  struct sockaddr_storage sender;    // avsender på forespørsel
  socklen_t senderlen;               // lengde på adresse
  struct rdp pkt;                    // svar på forespørsel
  struct rdp_connection *new_con;    // den potensielle nye forbindelsen

  // Sjekker om første pakke i nettverksstøpselet er en forespørsel;
  // hvis ikke, returneres NULL
  rdp_peek(sockfd, &buf, NULL, NULL);
  if (((struct rdp *)&buf)->flag != RDP_REQ) {
    *accept = RDP_NOTAREQ;
    *recvid = 0;
    return NULL;
  }


  // MOTTAR FORESPØRSEL:
  
  // Setter buffer `buf` som forespørsel skal mottas på til \0
  memset(&buf, '\0', sizeof buf);

  // Informasjon om avsender skal lagres i `sender`; dette er altså
  // svaradressen. Fjerner informasjonen som er der fra før.
  memset(&sender, '\0', sizeof sender);
  senderlen = sizeof sender;

  // Motta pakke (blokkerer i/o!)
  rc = recvfrom(sockfd, buf, sizeof buf, 0,
                (struct sockaddr *)&sender, &senderlen);
  if (rdp_error(rc, "rdp_accept: recvfrom")) {
    *accept = FALSE;
    *recvid = 0;
    return NULL;
  }

  // Henter ut og setter ID-er, i nettverksbyterekkefølge
  *recvid   = ((struct rdp *)&buf)->senderid;
  *senderid = htonl(*senderid);

  // Hvis det ønskes å opprette forbindelse må det først sjekkes om
  // etterspurt ID er ledig.
  if (*accept == TRUE) {

    // Sjekker alle forbindelser
    for (int i = 0; i < conslen; i++) {
      if (cons[i] == NULL) continue;

      // Hvis vi finner ID-en, skal vi avslå forbindelsen med feilmelding,
      // siden vi ikke kan ha to koblinger med samme ID.
      if (cons[i]->recvid == ((struct rdp *)&buf)->senderid) {
        *accept = RDP_INVALID;
        break;
      }
    }
  }
  

  // SENDER FORESPØRSELSSVAR:

  memset(&pkt, '\0', sizeof(pkt));

  // Variabelen `accept` er et flagg med en av verdiene: `RDP_CONFULL`,
  // `RDP_INVALID` eller `TRUE`. Hvis `accept` ikke er `TRUE`, blir kobling
  // avslått og vi sender flagget/årsaken tilbake til avsender i
  // `metadata`-feltet.

  pkt.flag     = (*accept == TRUE) ? RDP_ACC : RDP_DEN;
  pkt.metadata = htonl((*accept != TRUE) ? *accept : 0);
  pkt.senderid = *senderid;
  pkt.recvid   = *recvid;

  wc = send_packet(sockfd, (void *)&pkt, sizeof pkt, 0,
                   (struct sockaddr *)&sender, senderlen);
  if (rdp_error(wc, "rdp_accept: send_packet")) {
    *accept = FALSE;
    return NULL; 
  }

  // Hvis vi aksepterer, returnerer vi en ny kobling; ellers NULL.
  if (*accept == TRUE) {

    // Lager ny oppkobling
    new_con               = malloc(sizeof(struct rdp_connection));
    new_con->recipient    = malloc(sizeof(struct sockaddr_storage));
    new_con->recipientlen = senderlen;
    new_con->sockfd       = sockfd;
    new_con->senderid     = *senderid;
    new_con->recvid       = *recvid;
    new_con->pktseq       = -1; // signal på at ingen pakker er sendt ennå
    new_con->ackseq       = -1; // signal på at ingen pakker er ACK-et ennå
    memset(new_con->recipient, '\0', sizeof(struct sockaddr_storage));
    memcpy(new_con->recipient, &sender, senderlen);

    return new_con;
  }

  return NULL;
}

int rdp_listen(char *port)
{
  struct addrinfo hints, *res;
  int true = TRUE;
  int sockfd, rv;

  // Finner IP-adressen våres. Lar `getaddrinfo` avgjøre formatet.
  memset(&hints, '\0', sizeof hints); 
  hints.ai_family   = AF_UNSPEC;   // spesifiserer ikke IP versjon
  hints.ai_socktype = SOCK_DGRAM;  // vi ønsker typen «datagram», for UDP
  hints.ai_flags    = AI_PASSIVE;  // bruker adressen til den lokale maskinen
  rv = getaddrinfo(NULL, port, &hints, &res); // NULL som hostname -> oss selv
  if (rv != 0) {
    fprintf(stderr, "%srdp_listen: getaddrinfo: %s%s\n",
            RED, gai_strerror(rv), RESET);
    return 0;
  }

  // Lager støpselfildeskriptor. Bruker argumentene vi fylte inn i `hints` over
  sockfd = socket(res->ai_family, res->ai_socktype, 0);
  if (rdp_error(sockfd, "rdp_listen: socket")) return 0;

  // Prøver å igjenbruke porten, nyttig hvis vi kjører serveren flere
  // ganger uten å lukke støpselet i en testsituasjon.
  rv = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &true, sizeof(int));
  if (rdp_error(rv, "rdp_listen: setsockopt")) return 0;

  // Binder UDP-støpselet til nettverksadressen vi har funnet.
  rv = bind(sockfd, res->ai_addr, res->ai_addrlen);
  if (rdp_error(rv, "rdp_listen: bind")) return 0;

  // Ferdig med denne lenkede listen nå
  freeaddrinfo(res);

  return sockfd;
}


/**
 * Prøver å koble til en RDP-lytter, på en bestemt adresse og port.
 */
struct rdp_connection *rdp_connect(char* vert, char* port, int assign_id)
{
  int rv, wc;                  // retur verdi; «write count»
  struct addrinfo hints, *res; // variable til `getaddrinfo()`
  struct rdp_connection *con;  // forbindelsen som skal returneres
  int sockfd;                  // nettverksstøpselfildeskriptor
  struct rdp pkt;              // RDP-pakke buffer
  struct pollfd pfds[1];       // fildesk. som `poll()` skal sjekke

  // Lager hint til `getaddrinfo()`
  memset(&hints, '\0', sizeof hints); 
  hints.ai_family   = AF_UNSPEC;   // spesifiserer ikke IP versjon
  hints.ai_socktype = SOCK_DGRAM;  // vi ønsker typen «datagram», for UDP
  hints.ai_flags    = AI_PASSIVE;  // bruker adressen til den lokale maskinen

  // Gjør DNS-oppslag
  rv = getaddrinfo(vert, port, &hints, &res);
  if (rv != 0) {
    fprintf(stderr, "%srdp_connect: getaddrinfo: %s%s\n",
            RED, gai_strerror(rv), RESET);
    return NULL;
  }

  // TODO: `res` er nå en lenket liste med svar fra DNS-oppslaget. Jeg bare
  // antar at første resultat er gyldig, men ideelt bør jeg prøve alle
  // nodene før jeg gir opp.

  sockfd = socket(res->ai_family,    // bruker IP versjon fra DNS oppslaget
                  res->ai_socktype,  // blir SOCK_DGRAM, fra `hints` over
                  res->ai_protocol); // blir UDP, fra oppslaget
  if (rdp_error(sockfd, "rdp_connect: socket")) return NULL;

  // Finner en ledig nettverksport (på den lokale maskinen) som vi kan binde
  // oss til; vi *foretrekker* `port`-en som vi sendte inn som argument --
  // samme som serveren -- men vi nøyer oss med en hvilkensomhelst ledig en.
  rv = bind(sockfd, res->ai_addr, res->ai_addrlen);
  if (rdp_error(rv, "rdp_connect: bind")) {
    if (errno == 48 /* macOS */ || errno == 98 /* IFI / Linux */) {
      printf("Ignorerer og antar RDP allerede er bundet til denne\n");
      printf("porten på den lokale maskinen. Lar OS-et tildele en\n");
      printf("tilfeldig annen port for denne forbindelsen.\n");
    } else return NULL;
  }

  // Sender forespørsel til server
  bzero(&pkt, sizeof(pkt));
  pkt.flag     = RDP_REQ;           // forespørselsflagg
  pkt.senderid = htonl(assign_id);  // ID-en vi ønsker i [Big-Endian]
  pkt.recvid   = htonl(0);          // ukjent, 0 inntil svar fra server

  wc = send_packet(sockfd, (void *)&pkt, sizeof pkt, 0,
                   (struct sockaddr *)res->ai_addr, res->ai_addrlen);
  if (rdp_error(wc, "rdp_connect: send_packet")) return NULL; 

  // Lager RDP forbindelsesstruktur
  con               = malloc(sizeof(struct rdp_connection));
  con->recipient    = malloc(sizeof(struct sockaddr_storage));
  con->recipientlen = res->ai_addrlen;
  con->sockfd       = sockfd;
  con->pktseq       = -1; // signal på at ingen pakker er mottatt ennå
  con->ackseq       = -1; // signal på at ingen pakker er ACK-et ennå
  con->senderid     = pkt.recvid;
  memset(con->recipient, '\0', sizeof(struct sockaddr_storage));
  memcpy(con->recipient, res->ai_addr, res->ai_addrlen);

  // Trenger ikke denne lenger
  freeaddrinfo(res);

  // Venter på svar med `poll()`
  pfds->fd = sockfd;        // vi vil at `poll` skal lytte på dette støpselet
  pfds->events = POLLIN;    // vi overvåker om vi kan *lese* (ikke skrive)
  rv = poll(pfds, 1, 1000); // vil at `poll()` skal vente i 1 sek == 1000 ms

  // Sjekker om timeout, eller andre feilmeldinger
  if (rv == 0 || rdp_error(rv, "rdp_connect: poll")) {
    if (rv == 0) printf("%srdp_connect: timeout%s\n", RED, RESET);
    free(con);
    return NULL;
  }

  // Leser svar fra server
  bzero(&pkt, sizeof(pkt));
  rv = recv(sockfd, &pkt, sizeof pkt, 0);
  if (rdp_error(rv, "rdp_connect: recv")) return NULL;

  // Hvis forespørselen blir avslått, frigjør vi koblingstrukturen vi
  // konstruerte og setter pekeren til NULL.
  if (pkt.flag != RDP_ACC) { 

    printf("%srdp_connect: forbindelse ikke opprettet", RED);
    if ((int) ntohl(pkt.metadata) == RDP_INVALID) printf(": ugyldig ID");
    if ((int) ntohl(pkt.metadata) == RDP_CONFULL) printf(": ingen kapasitet");
    if (pkt.senderid == con->senderid) printf(": ingen server på adresse");
    printf("%s\n", RESET);

    rdp_close(con, TRUE);
    con = NULL;

  } else {

    // Ellers, husker vi å sette ID-ene vi har fått fra serveren.
    con->recvid   = pkt.senderid;
    con->senderid = pkt.recvid;

  }

  return con; // <-- enten NULL, eller `malloc`-et
}


/**
 * Send en RDP-pakke over en RDP-forbindelse.
 */
int rdp_send(struct rdp_connection *con, struct rdp *pkt)
{
  int wc; // «write count»
  
  // Hvis vi ikke har mottatt en ACK på forrige pakke vi sendte, nekter denne
  // funksjonen å skrive neste pakke. Applikasjonen som kaller må vente på en
  // ACK på tidligere sendte pakke. (Med mindre man prøver å sende noe annet
  // enn en datapakke.)
  if (pkt->flag == RDP_DAT && pkt->pktseq % 255 != (con->ackseq + 1) % 255) {
    return 0;
  }

  // Finner ut akkurat hvor mye data vi trenger å sende
  if (pkt->flag == RDP_DAT) wc = ntohl(pkt->metadata);
  else wc = sizeof(*pkt) - sizeof(pkt->payload);

  wc = send_packet(con->sockfd,                       // Over dette støpselet,
                   (void *)pkt,                       // send denne pakken,
                   wc,                                // som er så mange bytes,
                   0,                                 // med ingen rare flagg,
                   (struct sockaddr *)con->recipient, // til denne adressen,
                   con->recipientlen);                // som er så lang.
  if (rdp_error(wc, "rdp_send: send_packet")) return 0;

  // Øker pakkesekvensnummeret i koblingen, men bare hvis pakken ikke er et
  // duplikat av en tidligere ikke-ACKet pakke
  if (pkt->pktseq % 255 == (con->pktseq + 1) % 255) {
    con->pktseq += 1;
  }

  return 1;
}


/**
 * Prøv å skrive data over en kobling. Gitt et laaangt dataarray vil
 * funksjonen selv dele opp i flere pakker, og vil, basert på informasjon i
 * forbindelsesstrukturen om tidligere pakker, avgjøre hva neste pakke bør
 * være (om det så er en helt ny pakke, eller et duplikat av en tidligere en).
 * Hvis funksjonen tror den har levert hele dataarrayet sender den en tom
 * pakke.
 */
int rdp_write(struct rdp_connection *con,    // kobling det skal sendes over
              uint8_t data[],                // array med data som skal sendes
              size_t datalen)                // lengde på data array (bytes)
{
  int wc;                // «write count»
  struct rdp pkt;        // pakke som skal sendes
  size_t max_payloadlen; // lengde på payload i RDP pakke
  int new_pktseq;        // pakkesekvensnummer på den nye pakken

  // Definerer nyttelastlengde.
  max_payloadlen = sizeof(pkt.payload);

  bzero(&pkt, sizeof pkt);

  // Sekvensnummeret til første pakke er 0 (`con->ackseq` initialiseres til
  // -1), og generelt sender vi alltid pakken etter siste ACK vi mottok. Hvis
  // vi må sende flere pakker enn 255, begynner vi ACK sekvensen fra 0 igjen.
  // `con->pktseq` er en `int` og kan dermed holde mye høyere verdier, og vi
  // utnytter dette med modulo. Med denne løsningen får vi en øvre
  // filstørrelse på 255*1000*INT_MAX bytes > 547 TB (gitt minst et 32-bit
  // system); dette er mer enn nok. Siden dette er «stop-and-wait» kunne
  // jeg egentlig bare latt `ackseq` alternert mellom 0 og 1, men jeg
  // foretrekker denne løsningen siden feilmeldinger og utskrift virker mer
  // intuitivt (for meg).

  // Hvis forrige pakke er ACKet, lag neste pakke (ellers lag forrige på nytt)
  if (con->pktseq % 255 == con->ackseq % 255) new_pktseq = con->pktseq + 1;
  else new_pktseq = con->pktseq;

  // Finner «write count» basert på hvor mye data vi vil sende, og hvor mange
  // pakker som allerede er send:

  if (max_payloadlen*new_pktseq >= datalen) {

    // Hvis «pakker sendt» ganger lengden på nyttelasten overskrider totalt
    // antall bytes vi ønsker å sende (`datalen`), bør i teorien all ønsket
    // data blitt sendt alt. Neste datapakke vil ha ha lengde 0.
    wc = 0;

  } else if (max_payloadlen*new_pktseq + max_payloadlen > datalen) {

    // Sjekker om vi står i fare for å overskride `data` arrayet. Vi
    // tilpasser «write count» deretter.
    wc = datalen - max_payloadlen*new_pktseq;

  } else {

    // Ellers, fyller vi ut hele nyttelasten til pakken.
    wc = max_payloadlen;

  }

  // Lager pakke
  pkt.flag     = RDP_DAT;
  pkt.senderid = con->senderid;
  pkt.recvid   = con->recvid;
  pkt.pktseq   = new_pktseq % 255;
  // Finner total pakkelengde.
  pkt.metadata = htonl(   sizeof(pkt.flag)     + sizeof(pkt.pktseq)
                        + sizeof(pkt.ackseq)   + sizeof(pkt.unassigned)
                        + sizeof(pkt.senderid) + sizeof(pkt.recvid)
                        + sizeof(pkt.metadata) + wc );

  // Her brukes `max_payloadlen` som «stride»-faktor og `new_pktseq` som
  // indeks i dataarrayet.
  memcpy(pkt.payload, &data[max_payloadlen*new_pktseq], wc);

  // Sender pakke:
  rdp_send(con, &pkt);

  return wc;
}


/**
 * Send en forbindelsesavslutningspakke.
 */
int rdp_terminate(struct rdp_connection *con)
{
  struct rdp pkt;

  // Send terminering
  bzero(&pkt, sizeof pkt);
  pkt.flag     = RDP_TER; // <-- termflagg
  pkt.senderid = con->senderid;
  pkt.recvid   = con->recvid;
  pkt.pktseq   = (con->pktseq + 1) % 255;
  rdp_send(con, &pkt);   // send termineringspakke

  return 1;
}


/**
 * Send en ACK. Funksjonen vil selv avgjøre innholdet i ACK-pakken basert
 * på informasjonen i koblingsstruktet.
 */
int rdp_ack(struct rdp_connection *con)
{
  int wc;         // «write count»
  struct rdp pkt; // ACK-pakken vi skal sende

  memset(&pkt, '\0', sizeof pkt);
  pkt.flag     = RDP_ACK; // ACK-flagg
  pkt.senderid = con->senderid;
  pkt.recvid   = con->recvid;
  pkt.ackseq   = con->pktseq % 255;

  wc = send_packet(con->sockfd, (void *)&pkt, sizeof pkt, 0,
                   (struct sockaddr *)con->recipient, con->recipientlen);
  if (rdp_error(wc, "rdp_ack: send_packet")) return 0;

  // Noterer oss hva som er siste pakke vi har ACKet, hvis vi skulle motta
  // et duplikat.
  con->ackseq = con->pktseq % 255;

  return 1;
}


/**
 * Leser en pakke fra et nettverksstøpsel. Hvis pakken er en ny datapakke, vil
 * nyttelasten skrives til returbufferet `dest_buf`, svare med en ACK, og
 * funksjonen returnerer hvor mange bytes i nyttelasten. Hvis pakken er et
 * duplikat av en tidligere datapakke, vil den gjenta ACK og returnere en
 * feilmelding. Funksjonen vil også håndtere mottagelse av ACK-pakker.
 * Forbindelse- og avslutningsforespørsler, eller andre pakker, vil returnere
 * feilmeldinger.
 */
size_t rdp_read(int sockfd,                    // nettverksstøpsel
                struct rdp_connection *cons[], // koblingsarray
                int conslen,                   // lengde på koblingsarray
                void *dest_buf)                // returbuffer for nyttelast
{
  int rc, wc;                      // «read/write count»
  uint8_t buf[sizeof(struct rdp)]; // hvor pakken mottas
  struct sockaddr_storage sender;  // lagring for avsenders adresse
  socklen_t senderlen;             // avsenderadresse lengde
  struct rdp_connection *con;      // peker til avsenders forbindelse

  // Sjekker om første pakke i nettverksstøpselet er en forespørsel;
  // hvis det er tilfelle, returneres RDP_READ_REQ
  rdp_peek(sockfd, &buf, NULL, NULL);
  if (((struct rdp *)&buf)->flag == RDP_REQ) return RDP_READ_REQ;


  // LES INN PAKKE:

  // Nuller buffer `buf` som pakke skal mottas på.
  bzero(&buf, sizeof buf);

  // Informasjon om avsender skal lagres i `recipient`; dette er altså
  // svaradressen. Fjerner informasjonen som er der fra før.
  bzero(&sender, sizeof sender);
  senderlen = sizeof sender;

  // Mottar pakke, *for real* denne gangen (BLOKKERER I/O!)
  rc = recvfrom(sockfd, buf, sizeof buf, 0,
                (struct sockaddr *)&sender, &senderlen);
  if (rdp_error(rc, "rdp_read: recvfrom")) return 0;


  // FINNER AVSENDERS FORBINDELSE:

  con = NULL;
  for (int i = 0; i < conslen; i++) {
    if (cons[i] == NULL) continue;   // skip ikke-eksisterende koblinger

    // Hvis vi finner ID-en, settes pekeren `*con` til denne forbindelsen
    if (cons[i]->recvid == ((struct rdp *)&buf)->senderid) con = cons[i];
  }

  // Er avsender ukjent, gjøres ingenting
  if (con == NULL) return RDP_READ_UNK;


  // TOLKER PAKKEFLAGG:

  // Om pakken inneholder nyttelast
  if (((struct rdp *)&buf)->flag == RDP_DAT) {

    if (dest_buf != NULL) {
      // Skriver data til returbufferet
      wc = rdp_payloadlen((struct rdp *)&buf);
      memcpy(dest_buf, ((struct rdp *)&buf)->payload, wc);
    }

    // Sender en ACK
    if (((struct rdp *)&buf)->pktseq != (con->ackseq + 1) % 255) {
      // Hvis pakkesekvensnummeret er lavere enn siste ACK-en vi sendte,
      // sender vi forrige ACK på nytt og kaster denne pakken.
      rdp_ack(con);
      //printf("%srdp_read: mottok duplikat. Gjentar ACK på forrige pakke.%s\n",
      //       RED, RESET);
      return RDP_READ_DUP;
    } else {
      // Ellers oppdateres `pktseq` og vi sender ACK på den nye pakken.
      con->pktseq += 1;
      rdp_ack(con);
      return wc;
    }
  }

  // Håndterer pACKe. Øh, altså en ACK.
  else if (((struct rdp *)&buf)->flag == RDP_ACK) {

    // Oppdaterer `ackseq` i denne oppkoblingen.
    con->ackseq = ((struct rdp *)&buf)->ackseq;

    return RDP_READ_ACK;
  }

  // Hvis forbindelsesavslutting:
  else if (((struct rdp *)&buf)->flag == RDP_TER) return RDP_READ_TER;
  
  return RDP_READ_UNK; // <-- Nåes hvis pakken har et ukjent flagg.
}


/**
 * Likner på `rdp_read`, men vi bare tar en titt på neste pakke, heller enn å
 * faktisk fjerne den fra nettverksstøpselet. Vi skriver også HELE pakken til
 * `dest_buf`, ikke bare nyttelasten.
 */
size_t rdp_peek(int sockfd,                         // hvor vi ser etter pkt
                void *dest_buf,                     // returbuffer for pkt
                struct sockaddr_storage *dest_addr, // returbuf. avsender
                socklen_t *dest_addrlen)            // returbuf. adresselengde
{
  int rc;                           // «read count»
  uint8_t buf[sizeof(struct rdp)];  // hvor pakken mottas
  struct sockaddr_storage sender;   // lagring for avsenders adresse
  socklen_t senderlen;              // avsenderadresse lengde

  // Nuller buffer `buf` som pakke skal mottas på
  memset(&buf, '\0', sizeof buf);

  // Informasjon om avsender skal lagres i `recipient`; dette er altså
  // svaradressen. Fjerner informasjonen som er der fra før.
  memset(&sender, '\0', sizeof sender);
  senderlen = sizeof sender;
  
  // Titter på pakke (BLOKKERER I/O!)
  rc = recvfrom(sockfd,
                buf,
                sizeof buf,
                MSG_PEEK,   // <-- vi tar bare en titt
                (struct sockaddr *)&sender,
                &senderlen);
  if (rdp_error(rc, "rdp_peek: recvfrom")) return 0;

  // Noterer ned avsenders adresse. Hvis vi ikke har noe sted å notere dette,
  // dropper vi det i stillhet.
  if (dest_addr != NULL) {
    memcpy(&sender, dest_addr, senderlen);
    *dest_addrlen = senderlen;
  }

  // Skriver data til returbufferet, hvis det eksisterer.
  if (dest_buf != NULL) {
    memcpy(dest_buf, &buf, rc);
    return rc;
  }

  return 0;
}


/**
 * Finner lengde på nyttelasten i en RDP-datapakke `pkt`.
 */
size_t rdp_payloadlen(struct rdp *pkt)
{
  if (pkt == NULL) return 0;
  if (pkt->flag != RDP_DAT) return 0;

  return ntohl(pkt->metadata) - sizeof(pkt->flag)     - sizeof(pkt->pktseq)
                              - sizeof(pkt->ackseq)   - sizeof(pkt->unassigned)
                              - sizeof(pkt->senderid) - sizeof(pkt->recvid)
                              - sizeof(pkt->metadata);

  // TODO: deler av uttrykket over kan avgjøres ved kompilering, så man kan
  //       sikkert spare et par klokkesykler med en #define makro.
}


/**
 * Lukker en forbindelse, frigjør datastrukturer. Kan også lukke
 * nettverksstøpselet, om ønskelig.
 */
int rdp_close(struct rdp_connection *con, // kobling som skal lukke
              int close_sockfd)           // om `sockfd` skal lukkes, boolsk
{
  if (con != NULL) {
    if (close_sockfd) close(con->sockfd);
    free(con->recipient);
    free(con);
    return 1;
  } else return 0;
}

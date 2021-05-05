#include "rdp.h"
#include "send_packet.h"

#define RED "\e[0;31m"
#define GRN "\e[0;32m"
#define YEL "\e[0;33m"
#define RESET "\e[0m"

int rdp_print(struct rdp *pkt)
{
  printf("{ ");
  printf("flag==0x%x, ",          pkt->flag);
  printf("pktseq==0x%x, ",        pkt->pktseq);
  printf("ackseq==0x%x, ",        pkt->ackseq);
  printf("senderid==0x%x, ",      ntohl(pkt->senderid));
  printf("recvid==0x%x, ",        ntohl(pkt->recvid));
  printf("metadata==0x%x, ",      pkt->metadata);

  // Passer på å terminere `payload` med `\0` før utskrift. Her lager jeg
  // en ny strengbuffer som jeg kopierer over til.
  char str[rdp_payloadlen(pkt) + 1];
  str[rdp_payloadlen(pkt)] = '\0';
  strncpy(str, (char *)pkt->payload, rdp_payloadlen(pkt));

  printf("payload==\"%s\" }\n", str);

  return 1;
}

int rdp_printc(struct rdp_connection* con)
{
  printf("senderid==%d, ", ntohl(con->senderid));
  printf("recvid==%d, ",   ntohl(con->recvid));
  printf("sockfd==%d, ",   con->sockfd);
  char s[INET6_ADDRSTRLEN];
  printf("recipient->addr==%s, ", _get_recipient_addr(s, con));
  printf("recipient->port==%s, ", _get_recipient_port(s, con));
  printf("recipientlen==%d }\n", con->recipientlen);
  return 1;
}

int rdp_error (int rv, char *msg) {
  if (rv < 0) {
    fprintf(stderr, "%s%s: %s%s\n", RED, msg, strerror(errno), RESET);
    // TODO: oppvask
    return 1;
  }
  return 0;
}

struct rdp_connection *rdp_accept(int sockfd,
                                  struct rdp_connection *cons[],
                                  int conslen,
                                  int accept,
                                  int assign_id)
{
  int rc, wc;                        // read / write count
  void *buf[sizeof(struct rdp)];      // lest data
  struct sockaddr_storage sender;    // avsender på forespørsel
  socklen_t senderlen;               // lengde på adresse
  struct rdp pkt;                    // svar på forespørsel
  struct rdp_connection *new_con;    // den potensielle nye forbindelsen

  // Sjekker om første pakke i nettverksstøpselet er en forespørsel;
  // hvis ikke, returneres NULL
  rdp_peek(sockfd, &buf, NULL, NULL);
  if (((struct rdp *)&buf)->flag != RDP_REQ) return NULL;
  
  // Setter buffer `buf` som forespørsel skal mottas på til \0
  memset(&buf, '\0', sizeof buf);

  // Informasjon om avsender skal lagres i `sender`; dette er altså
  // svaradressen. Fjerner informasjonen som er der fra før.
  memset(&sender, '\0', sizeof sender);
  senderlen = sizeof sender;

  // Hvis det ønskes å opprette forbindelse må det først sjekkes om
  // etterspurt ID er ledig.
  if (accept == TRUE) {
    // Sjekker alle forbindelser
    for (int i = 0; i < conslen; i++) {
      if (cons[i] == NULL) continue;
      else if (cons[i]->recvid == ((struct rdp *)&buf)->senderid) {
        // Hvis vi finner ID-en, skal vi avslå forbindelsen med feilmelding;
        // vi kan ikke ha to koblinger med samme ID.
        accept = RDP_INVALID;
        break;
      }
    }
  }
  
  // Mottar pakke. (BLOKKERER I/O!)
  rc = recvfrom(sockfd, buf, sizeof buf, 0,
                (struct sockaddr *)&sender, &senderlen);
  if (rdp_error(rc, "rdp_accept: recvfrom")) return NULL;

  // Sender forespørselssvar
  memset(&pkt, '\0', sizeof(pkt));
  pkt.senderid = htonl(assign_id);
  pkt.recvid   = ((struct rdp *)&buf)->senderid;
  // `accept` er et flagg med en av verdiene: `RDP_CONFULL`, `RDP_INVALID`
  // eller `TRUE`. Hvis `accept` ikke er `TRUE`, blir kobling avslått og vi
  // sender flagget/årsaken tilbake i `metadata`-feltet.
  pkt.flag     = (accept == TRUE) ? RDP_ACC : RDP_DEN;
  pkt.metadata = (accept != TRUE) ? accept : 0;
  wc = send_packet(sockfd, (void *)&pkt, sizeof pkt, 0,
                   (struct sockaddr *)&sender, senderlen);
  if (rdp_error(wc, "rdp_accept: send_packet")) return NULL; 

  // Hvis vi aksepterer, returnerer vi en ny kobling; ellers NULL.
  if (accept == TRUE) {

    // // Lager støpselfildeskriptor.
    // sockfd = socket(sender.ss_family, SOCK_DGRAM, 0);
    // if (rdp_error(sockfd, "rdp_accept: socket")) return NULL;

    // // Binder UDP-støpselet til nettverksadressen til avsender.
    // rv = bind(sockfd, &sender, senderlen);
    // if (rdp_error(rv, "rdp_accept: bind")) return NULL;

    // Lager ny oppkobling
    new_con               = malloc(sizeof(struct rdp_connection));
    new_con->recipient    = malloc(sizeof(struct sockaddr_storage));
    new_con->recipientlen = senderlen;
    new_con->sockfd       = sockfd;
    new_con->senderid     = htonl(assign_id);
    new_con->recvid       = ((struct rdp *)&buf)->senderid;
    new_con->pktseq       = -1; // signal på at ingen pakker er sendt ennå
    new_con->ackseq       = -1; // signal på at ingen pakker er ACK-et ennå
    memset(new_con->recipient, '\0', sizeof(struct sockaddr_storage));
    memcpy(new_con->recipient, &sender, senderlen);
    return new_con;
  } else return NULL;
}

int rdp_listen(char *port)
{
  struct addrinfo hints, *res;
  int true = 1;
  int sockfd, rv;

  // Finner IP-addresen våres. Lar `getaddrinfo` avgjøre formatet.
  memset(&hints, '\0', sizeof hints); 
  hints.ai_family   = AF_UNSPEC;   // spesifierer ikke IP versjon
  hints.ai_socktype = SOCK_DGRAM;  // vi ønsker typen «datagram», for UDP
  hints.ai_flags    = AI_PASSIVE;  // bruker addresen til den lokale maskinen
  rv = getaddrinfo(NULL, port, &hints, &res);  // NULL som hostname => oss selv
  if (rv != 0) {
    fprintf(stderr, "%srdp_listen: getaddrinfo: %s%s\n",
            RED, gai_strerror(rv), RESET);
    return 0;
  }

  // Lager støpselfildeskriptor. Bruker argumentene vi fylte inn i `hints` over
  sockfd = socket(res->ai_family, res->ai_socktype, 0);
  if (rdp_error(sockfd, "rdp_listen: socket")) return 0;

  // Prøver å igjenbruke porten, nyttig hvis vi kjører serveren flere
  // ganger uten å lukke støpselet. (MEN! skaper kluss om vi prøver å
  // kjøre flere servere på samme maskin.)
  rv = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &true, sizeof(int));
  if (rdp_error(rv, "rdp_listen: setsockopt")) return 0;

  // Binder UDP-støpselet til nettverksadressen vi har funnet.
  rv = bind(sockfd, res->ai_addr, res->ai_addrlen);
  if (rdp_error(rv, "rdp_listen: bind")) return 0;

  // Ferdig med denne lenkede listen nå
  freeaddrinfo(res);

  return sockfd;
}

struct rdp_connection *rdp_connect(char* vert, char* port, int assign_id)
{
  int rv, wc;                  // retur verdi; «write count»
  struct addrinfo hints, *res; // variable til `getaddrinfo()`
  struct rdp_connection *con;  // forbindelsen som skal returneres
  int sockfd;                  // nettverksstøpselfildeskriptor
  struct rdp pkt;              // RDP-pakke buffer
  fd_set fds;                  // sett med fildeskriptorer til `select()`
  struct timeval tv;           // timeout for svar fra server

  // Gir hint til 
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
  memset(&pkt, '\0', sizeof(pkt));
  pkt.flag     = RDP_REQ;
  pkt.senderid = htonl(assign_id);
  pkt.recvid   = htonl(0);

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
  memcpy(con->recipient, res->ai_addr, sizeof(struct sockaddr_storage));

  // Trenger ikke denne lenger
  freeaddrinfo(res);

  // Venter på svar med `select()`
  FD_ZERO(&fds);
  FD_SET(sockfd, &fds);
  tv.tv_sec = 1;  // timeout på 1 sek, som oppgaven ber om
  tv.tv_usec = 0;
  if ((rv = select(sockfd+1, &fds, NULL, NULL, &tv)) == 0) {
    printf("%srdp_connect: timeout%s\n", RED, RESET);
    free(con);
    return NULL;
  } else if (rdp_error(rv, "rdp_connect: select")) return NULL;

  // Leser svar fra server
  memset(&pkt, '\0', sizeof(pkt));
  rv = recv(sockfd, &pkt, sizeof pkt, 0);
  if (rdp_error(rv, "rdp_connect: recv")) return NULL;

  rdp_print(&pkt);
  // Hvis forespørselen blir avslått, frigjør vi koblingstrukturen vi
  // konstruerte og setter pekeren til NULL.
  if (pkt.flag != RDP_ACC) { 
    printf("%srdp_connect: forbindelse ikke opprettet", RED);
    if (pkt.metadata == RDP_INVALID) printf(": ugyldig ID");
    if (pkt.metadata == RDP_CONFULL) printf(": ingen kapasitet");
    if (pkt.senderid == con->senderid) printf(": ingen server på adresse");
    printf("%s\n", RESET);
    free(con);
    con = NULL;
  } else {
    // Ellers, husker vi å sette ID-ene vi har fått fra serveren.
    con->recvid   = pkt.senderid;
    con->senderid = pkt.recvid;
  }

  return con; // <-- enten NULL, eller `malloc`-et
}

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
  if (pkt->flag == RDP_DAT) wc = pkt->metadata;
  else wc = sizeof(*pkt) - sizeof(pkt->payload);

  wc = send_packet(con->sockfd,
                   (void *)pkt,
                   wc,
                   0,
                   (struct sockaddr *)con->recipient, 
                   con->recipientlen);
  if (rdp_error(wc, "rdp_send: send_packet")) return 0;

  // Øker pakkesekvensnummeret i koblingen, men bare hvis pakken ikke er et
  // duplikat av en tidligere ikke-ACKet pakke
  if (pkt->pktseq % 255 == (con->pktseq + 1) % 255) {
    con->pktseq += 1;
  }

  return 1;
}

int rdp_write(struct rdp_connection *cons[], // koblinger det skal sendes over
              int conslen,                   // `cons[]`’ lengde
              uint8_t data[],                // array med data som skal sendes
              size_t datalen)                // lengde på data array (bytes)
{
  int wc, null_ptrs;     // «write count»; teller antall NULL pekere i array
  struct rdp pkt;        // pakke som skal sendes
  size_t max_payloadlen; // lengde på payload i RDP pakke
  int new_pktseq;        // pakkesekvensnummer på den nye pakken

  // Definerer nyttelastlengde.
  max_payloadlen = sizeof(pkt.payload);

  // Teller antall NULL-pekere, for å avgjøre returverdi
  null_ptrs = 0;

  // Prøver nå å sende «data» til alle koblinger.
  for (int j = 0; j < conslen; j++) {
    // Hopp over ikke-eksisterende koblinger
    if (cons[j] == NULL) {
      null_ptrs++;
      continue;
    }

    bzero(&pkt, sizeof pkt);

    // Lager neste pakke som vi ønsker å sende:

    // Sekvensnummeret til første pakke er 0 (`con->ackseq` initialiseres til
    // -1 i `rdp_accept`), og generelt sender vi alltid pakken etter siste ACK
    // vi mottok. Hvis vi må sende flere pakker enn 255, begynner vi ACK
    // sekvensen fra 0 igjen. `con->pktseq` er en `int` og kan dermed holde
    // mye høyere verdier, og vi utnytter dette med modulo. Med denne
    // løsningen får vi en øvre filstørrelse på 255*1000*INT_MAX bytes > 547
    // TB (på et 32-bit system, eller mer); dette er mer enn nok. Siden dette
    // er «stop-and-wait» kunne jeg egentlig bare latt `ackseq` alternert
    // mellom 0 og 1, men jeg foretrekker dette siden feilmeldinger og
    // utskrift virker mer intuitivt (for meg).

    // Hvis forrige pakke er ACKet, lag neste pakke (ellers lag forrige på
    // nytt)
    new_pktseq = cons[j]->pktseq;
    if (cons[j]->pktseq % 255 == cons[j]->ackseq % 255) new_pktseq++;

    if (max_payloadlen*new_pktseq >= datalen) {
      // Hvis «pakker sendt» ganger lengden på nyttelasten overskrider totalt
      // antall bytes vi ønsker å sende (`datalen`) skal neste pakke
      // terminere forbindelsen
      printf("server: siste pakke til %d er ACK-et. Terminerer kobling\n",
             ntohl(cons[j]->recvid));
      rdp_terminate(cons, conslen, cons[j]);
      continue;
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
    pkt.senderid = cons[j]->senderid;
    pkt.recvid   = cons[j]->recvid;
    pkt.pktseq   = new_pktseq % 255;
    // Finner total pakkelengde.
    pkt.metadata =   sizeof(pkt.flag)     + sizeof(pkt.pktseq)
                   + sizeof(pkt.ackseq)   + sizeof(pkt.unassigned)
                   + sizeof(pkt.senderid) + sizeof(pkt.recvid)
                   + sizeof(pkt.metadata) + wc;

    // Her brukes `max_payloadlen` som «stride»-faktor og `new_pktseq` som
    // indeks i fildataarrayet.
    memcpy(pkt.payload, &data[max_payloadlen*new_pktseq], wc);

    // Sender pakke:
    rdp_send(cons[j], &pkt);
  }

  // Hvis koblingsarrayet består kun av NULL-pekere, er dette et tegn på at
  // dette funksjonskallet ikke har sendt noen pakker. Vi returnerer deretter.
  if (null_ptrs == conslen) return 0;
  else return 1;
}

int rdp_terminate(struct rdp_connection *cons[],
                  int conslen,
                  struct rdp_connection *con)
{
  struct rdp pkt;
  int i = 0;

  // Send terminering
  bzero(&pkt, sizeof pkt);
  pkt.flag     = RDP_TER; // <-- termflagg
  pkt.senderid = con->senderid;
  pkt.recvid   = con->recvid;
  pkt.pktseq   = (con->pktseq + 1) % 255;
  rdp_send(con, &pkt);   // send termineringspakke
  rdp_close(con, FALSE); // lukker kobling, men IKKE fildeskriptoren

  // Fjern denne koblingen fra lista
  while (cons[i] != con && i < conslen) i++; // finner indeks til `con`
  if (i >= conslen) return 0;                // fant ikke indeks i array
  else cons[i] = NULL;                       // ellers, fjern kobling

  return 1;
}

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
  else {
    // Noterer oss hva som er siste pakke vi har ACKet, hvis vi skulle
    // motta duplikat.
    con->ackseq = con->pktseq % 255;
    return 1;
  }
}

size_t rdp_read(int sockfd,
                struct rdp_connection *cons[],
                int conslen,
                void *dest_buf)
{
  int rc, wc;                     // «read/write count»
  uint8_t buf[sizeof(struct rdp)];   // hvor pakken mottas
  struct sockaddr_storage sender; // lagring for avsenders adresse
  socklen_t senderlen;            // avsenderadresse lengde
  struct rdp_connection *con;     // peker til avsenders forbindelse

  // Sjekker om første pakke i nettverksstøpselet er en forespørsel;
  // hvis det er tilfelle, returneres 0
  rdp_peek(sockfd, &buf, NULL, NULL);
  if (((struct rdp *)&buf)->flag == RDP_REQ) return 0;

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

  // Finner avsenders forbindelse
  con = NULL;
  for (int i = 0; i < conslen; i++) {
    if (cons[i] == NULL) continue;
    // Hvis vi finner ID-en, settes pekeren `*con` til denne forbindelsen
    if (cons[i]->recvid == ((struct rdp *)&buf)->senderid) con = cons[i];
  }

  // Er avsender ukjent, gjøres ingenting
  if (con == NULL) {
    return 0;
  } 

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
      printf("%srdp_read: mottok duplikat. Gjentar ACK på forrige pakke.%s\n",
             RED, RESET);
      return 0;
    } else {
      // Ellers oppdateres `pktseq` og vi sender ACK på den nye pakken.
      con->pktseq += 1;
      rdp_ack(con);
      return wc;
    }
  }

  // Håndterer pACKe.
  else if (((struct rdp *)&buf)->flag == RDP_ACK) {
    printf("server: mottok ACK %d fra %d\n",
           ((struct rdp *)&buf)->ackseq, ntohl(con->recvid));
    // Oppdaterer `ackseq` i denne oppkoblingen.
    con->ackseq = ((struct rdp *)&buf)->ackseq;
    return 0;
  }

  // Hvis forbindelsesavslutting:
  else if (((struct rdp *)&buf)->flag == RDP_TER) {
    printf("server: mottok forbindelsesavslutting. Terminerer kobling\n");
    rdp_terminate(cons, conslen, con);
    return 0;
  }
  
  return 0; // <-- Bør ikke nåes 
}

// *Nesten* samme funksjon som `rdp_read`, men vi bare tar en titt på neste
// pakke, heller enn å faktisk fjerne den fra nettverksstøpselet. Vi skriver
// også HELE pakken til `dest_buf`, ikke bare nyttelasten.
size_t rdp_peek(int sockfd, void *dest_buf,
                struct sockaddr_storage *dest_addr,
                socklen_t *dest_addrlen)
{
  int rc, wc;                     // «read/write count»
  uint8_t buf[sizeof(struct rdp)];  // hvor pakken mottas
  struct sockaddr_storage sender; // lagring for avsenders adresse
  socklen_t senderlen;            // avsenderadresse lengde

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

  if (dest_addr != NULL) {
    // Noterer ned avsenders adresse. Hvis vi ikke har noe sted å notere
    // dette, dropper vi det i stillhet.
    memcpy(&sender, dest_addr, senderlen);
    *dest_addrlen = senderlen;
  }

  if (dest_buf != NULL) {
    // Skriver data til returbufferet
    wc = rc;
    memcpy(dest_buf, &buf, wc);
    return wc;
  }

  return 0;
}

size_t rdp_payloadlen(struct rdp *pkt)
{
  if (pkt == NULL) return 0;
  if (pkt->flag != RDP_DAT) return 0;
  else return pkt->metadata - sizeof(pkt->flag)     - sizeof(pkt->pktseq)
                            - sizeof(pkt->ackseq)   - sizeof(pkt->unassigned)
                            - sizeof(pkt->senderid) - sizeof(pkt->recvid)
                            - sizeof(pkt->metadata);
  // TODO: deler av uttrykket over kan avgjøres ved kompilering, så man kan
  //       sikkert spare et par klokkesykler med en #define makro.
}

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


/* Hjelpefunksjoner. */

void *_get_addr(struct sockaddr_storage *ss) {
  if (ss->ss_family == AF_INET) {
    return &((struct sockaddr_in *)ss)->sin_addr;
  } else if (ss->ss_family == AF_INET6) {
    return &((struct sockaddr_in6 *)ss)->sin6_addr;
  } else {
    printf("_get_addr: feil i `ss_family` til `struct sockaddr_storage`\n");
    exit(-1);
  }
}

// Henter ut nettverksadressen til en forbindelse, som leselig streng.
const char *_get_recipient_addr(char *s, struct rdp_connection *con) {
  const char* rv = inet_ntop(con->recipient->ss_family, _get_addr(con->recipient), s, sizeof s);
  if (rv == NULL) rdp_error(-1, "_get_recipient_addr: inet_ntop");
  return rv;
}

// Henter ut nettverksporten til en forbindelse, som leselig streng.
const char *_get_recipient_port(char *s, struct rdp_connection *con) {
  u_int16_t port;
  if (con->recipient->ss_family == AF_INET) {
    port = ((struct sockaddr_in *)con->recipient)->sin_port;
  } else if (con->recipient->ss_family == AF_INET6) {
    port = ((struct sockaddr_in6 *)con->recipient)->sin6_port;
  } else {
    printf("_get_recipient_port: feil i `ss_family` til `struct sockaddr_storage`\n");
    exit(-1);
  }
  const char* rv = inet_ntop(con->recipient->ss_family, &port, s, sizeof s);
  if (rv == NULL) rdp_error(-1, "_get_recipient_port: inet_ntop");
  return rv;
}

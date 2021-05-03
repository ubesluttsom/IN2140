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
  // en ny strengbuffer som jeg kopierer over til. TODO: tilpasse dette til
  // «flexible array member».
  int payloadlen;
  if (pkt->flag == RDP_DAT) {
    payloadlen = pkt->metadata - sizeof(pkt->flag)   - sizeof(pkt->pktseq)
                               - sizeof(pkt->ackseq) - sizeof(pkt->senderid)
                               - sizeof(pkt->recvid) - sizeof(pkt->metadata);
  } else payloadlen = 0;
  char str[payloadlen + 1];
  str[payloadlen] = '\0';
  strncpy(str, (char *)pkt->payload, payloadlen);

  printf("payload==\"%s\" }\n", str);

  return EXIT_SUCCESS;
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
  return EXIT_SUCCESS;
}

int rdp_error (int rv, char *msg) {
  if (rv < 0) {
    fprintf(stderr, "%s%s: %s%s\n", RED, msg, strerror(errno), RESET);
    // TODO: oppvask
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

struct rdp_connection *rdp_accept(int sockfd, int accept, int assign_id)
{
  int rc, wc;                        // read / write count
  void *buf[sizeof(struct rdp) + 1]; // lest data (pluss nullbyte)
  struct sockaddr_storage sender;    // avsender på forespørsel
  socklen_t senderlen;               // lengde på adresse
  struct rdp pkt;                    // svar på forespørsel
  struct rdp_connection *new_con;    // den potensielle nye forbindelsen
  
  // Setter buffer `buf` som forespørsel skal mottas på til \0
  memset(&buf, '\0', sizeof buf);

  // Informasjon om avsender skal lagres i `sender`; dette er altså
  // svaradressen. Fjerner informasjonen som er der fra før.
  memset(&sender, '\0', sizeof sender);
  senderlen = sizeof sender;
  
  // Mottar pakke. (BLOKKERER I/O!)
  rc = recvfrom(sockfd,
      buf,
      sizeof buf,
      0,
      (struct sockaddr *)&sender,
      &senderlen);
  if (rdp_error(rc, "rdp_accept: recvfrom")) return NULL;

  // Dobbelsjekker pakkeflagg
  if (((struct rdp *)&buf)->flag != RDP_REQ) {
    // Dette skal ikke skje. Bør ha blitt sjekket før kallet på
    // `rdp_accept()`.
    printf("rdp_accept: ikke en forespørsel. Pakke kastes\n");
    return NULL;
  }

  // Sender forespørselssvar
  memset(&pkt, '\0', sizeof(pkt));
  pkt.senderid = htonl(assign_id);
  pkt.recvid   = ((struct rdp *)&buf)->senderid;
  // `accept` er et flagg med en av verdiene: `NFSP_CONFULL`, `NFSP_INVALID`
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
    return EXIT_FAILURE;
  }

  // Lager støpselfildeskriptor. Bruker argumentene vi fylte inn i `hints` over
  sockfd = socket(res->ai_family, res->ai_socktype, 0);
  if (rdp_error(sockfd, "rdp_listen: socket")) return EXIT_FAILURE;

  // Prøver å igjenbruke porten, nyttig hvis vi kjører serveren flere
  // ganger uten å lukke støpselet. (MEN! skaper kluss om vi prøver å
  // kjøre flere servere på samme maskin.)
  rv = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &true, sizeof(int));
  if (rdp_error(rv, "rdp_listen: setsockopt")) return EXIT_FAILURE;

  // Binder UDP-støpselet til nettverksadressen vi har funnet.
  rv = bind(sockfd, res->ai_addr, res->ai_addrlen);
  if (rdp_error(rv, "rdp_listen: bind")) return EXIT_FAILURE;

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

  // Gir hint til `getaddrinfo()`
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
    printf("errno == %d\n", errno);
    if (errno == 48 || errno == 98) {
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

  // Lager RDP forbinndelsesstruktur
  con               = malloc(sizeof(struct rdp_connection));
  con->recipient    = malloc(sizeof(struct sockaddr_storage));
  con->recipientlen = res->ai_addrlen;
  con->sockfd       = sockfd;
  con->pktseq       = -1; // signal på at ingen pakker er mottatt ennå
  con->ackseq       = -1; // signal på at ingen pakker er ACK-et ennå
  con->senderid     = pkt.senderid;
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
  if (rdp_read(con, &pkt) == NULL) return NULL;


  rdp_print(&pkt);
  // Hvis forespørselen blir avslått, frigjør vi koblingstrukturen vi
  // konstruerte og setter pekeren til NULL.
  if (pkt.flag != RDP_ACC) { 
    printf("%srdp_connect: forbindelse ikke opprettet", RED);
    if (pkt.metadata == NFSP_INVALID) printf(": ugyldig ID");
    if (pkt.metadata == NFSP_CONFULL) printf(": ingen kapasitet");
    if (pkt.senderid == con->senderid) printf(": ingen server på adresse");
    printf("%s\n", RESET);
    free(con);
    con = NULL;
  } else {
    // Ellers, husker vi å sette ID-en vi har fått fra serveren.
    con->recvid = pkt.senderid;
  }

  return con; // <-- enten NULL, eller `malloc`-et
}

int rdp_write(struct rdp_connection *con, struct rdp *pkt)
{
  int wc;
  
  // Hvis vi ikke har mottatt en ACK på forrige pakke vi sendte, nekter denne
  // funksjonen å skrive neste pakke. Applikasjonen som kaller må vente på en
  // ACK på tidligere sendte pakke. (Med mindre man prøver å sende en
  // koblingsterminering, `flag==RDP_TER`; denne slipper igjennom.)
  // Kommentar: Dette er også noe jeg sjekker i server applikasjonen min,
  //            med siden det er et formelt krav i oppgaven gjøres det her
  //            og.
  if (pkt->pktseq > con->ackseq + 1 && pkt->flag != RDP_TER) {
    return EXIT_FAILURE;
  }

  wc = send_packet(con->sockfd, (void *)pkt, sizeof *pkt, 0,
                   (struct sockaddr *)con->recipient, con->recipientlen);
  if (rdp_error(wc, "rdp_write: send_packet")) return EXIT_FAILURE;

  // TODO: vent på ACK. Send på nytt etter 100ms. Bør implementeres i egen
  // funksjon, tror jeg, som også oppdaterer `ackseq`. Kanskje gjøre dette
  // i applikasjonslaget?
  
  return EXIT_SUCCESS;
}

int rdp_ack(struct rdp_connection *con)
{
  int wc;         // «write count»
  struct rdp pkt; // ACK-pakken vi skal sende

  memset(&pkt, '\0', sizeof pkt);
  pkt.flag     = RDP_ACK; // ACK-flagg
  pkt.senderid = con->senderid;
  pkt.recvid   = con->recvid;
  pkt.ackseq   = con->pktseq;

  wc = send_packet(con->sockfd, (void *)&pkt, sizeof pkt, 0,
                   (struct sockaddr *)con->recipient, con->recipientlen);
  if (rdp_error(wc, "rdp_ack: send_packet")) return EXIT_FAILURE;
  else {
    // Noterer oss hva som er siste pakke vi har ACKet, hvis vi skulle
    // motta duplikat.
    con->ackseq = con->pktseq;
    return EXIT_SUCCESS;
  }
}

void *rdp_read(struct rdp_connection *con, void *dest_buf)
{
  int rc;                         // «read count»
  void *buf[sizeof(struct rdp)];  // hvor pakken mottas
  struct sockaddr_storage sender; // lagring for avsenders adresse
  socklen_t senderlen;            // avsenderadresse lengde

  // Nuller buffer `buf` som pakke skal mottas på
  memset(&buf, '\0', sizeof buf);

  // Informasjon om avsender skal lagres i `recipient`; dette er altså
  // svaradressen. Fjerner informasjonen som er der fra før.
  memset(&sender, '\0', sizeof sender);
  senderlen = sizeof sender;
  
  // Mottar pakke (BLOKKERER I/O!)
  rc = recvfrom(con->sockfd,
                buf,
                sizeof buf,
                0,
                (struct sockaddr *)&sender,
                &senderlen);
  if (rdp_error(rc, "rdp_read: recvfrom")) return NULL;

  // // Overskriver `recipient` med avsenders addresse
  // memcpy(&sender, con->recipient, senderlen);

  // Hvis `dest_buf` pekeren er `NULL`, velger vi heller å bare printe pakken
  if (dest_buf == NULL) {
    // Lager buffer for avsenderstreng
    char s[INET6_ADDRSTRLEN];
    // Printer pakke
    printf("rdp_read: recvfrom: %d bytes from \"%s\":\n",
           rc, _get_recipient_addr(s, con));
    rdp_print((struct rdp *)&buf);
  } else {
    // Skriver pakke til returbufferet
    memcpy(dest_buf, &buf, sizeof(struct rdp));
  }

  // Sender en ACK om pakken inneholder nyttelast
  if (((struct rdp *)&buf)->flag == RDP_DAT) {
    if (((struct rdp *)&buf)->pktseq <= con->ackseq) {
      // Hvis pakkesekvensnummeret er lavere enn siste ACK-en vi sendte,
      // sender vi forrige ACK på nytt og kaster denne pakken.
      rdp_ack(con);
      printf("%srdp_read: mottokk duplikat. Gjentar ACK på forrige pakke.%s\n",
             RED, RESET);
      return NULL;
    } else {
      // Ellers oppdateres `pktseq` og vi sender ACK på den nye pakken.
      con->pktseq = ((struct rdp *)&buf)->pktseq;
      rdp_ack(con);
    }
  }

  // Håndterer pACKe.
  if (((struct rdp *)&buf)->flag == RDP_ACK) {
    // Oppdaterer `ackseq` i denne oppkoblingen.
    con->ackseq = ((struct rdp *)&buf)->ackseq;
  }

  return dest_buf;
}

// *Nesten* samme funksjon som `rdp_read`, men vi bare tar en titt på neste
// pakke, heller enn å faktisk fjerne den fra nettverksstøpselet.
void *rdp_peek(int sockfd, void *dest_buf, struct sockaddr_storage *dest_addr,
               socklen_t *dest_addrlen)
{
  int rc;  // «read count»
  void *buf[sizeof(struct rdp)];  // hvor pakken mottas
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
  if (rdp_error(rc, "rdp_peek: recvfrom")) return NULL;

  if (dest_addr != NULL) {
    // Noterer ned avsenders adresse. Hvis vi ikke har noe sted å notere
    // dette, dropper vi det i stillhet.
    memcpy(&sender, dest_addr, senderlen);
    *dest_addrlen = senderlen;
  }

  // Hvis `dest_buf` pekeren er `NULL`, skrives pakken ut
  if (dest_buf == NULL) {
    // Printer pakke
    printf("rdp_peek: recvfrom: %d bytes:\n", rc);
    rdp_print((struct rdp *)&buf);
  } else {
    // Skriver pakke til returbufferet
    memcpy(dest_buf, &buf, sizeof(struct rdp));
  }

  return dest_buf;
}

int rdp_close(struct rdp_connection *con, // kobling som skal lukke
              int close_sockfd)           // om `sockfd` skal lukkes, boolsk
{
  if (con != NULL) {
    if (close_sockfd) close(con->sockfd);
    free(con->recipient);
    free(con);
    return EXIT_SUCCESS;
  } else return EXIT_FAILURE;
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

// Henter ut nettverksaddressen til en forbinnelse, som leselig streng.
const char *_get_recipient_addr(char *s, struct rdp_connection *con) {
  const char* rv = inet_ntop(con->recipient->ss_family, _get_addr(con->recipient), s, sizeof s);
  if (rv == NULL) rdp_error(-1, "_get_recipient_addr: inet_ntop");
  return rv;
}

// Henter ut nettverksporten til en forbinnelse, som leselig streng.
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

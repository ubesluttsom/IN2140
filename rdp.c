#include "rdp.h"

#define RED "\e[0;31m"
#define GRN "\e[0;32m"
#define YEL "\e[0;33m"
#define RESET "\e[0m"

int rdp_print(struct rdp *pakke)
{
  printf("{ ");
  printf("flag==0x%x, ",          pakke->flag);
  printf("pktseq==0x%x, ",        pakke->pktseq);
  printf("ackseq==0x%x, ",        pakke->ackseq);
  printf("senderid==0x%x, ",      ntohl(pakke->senderid));
  printf("recvid==0x%x, ",        ntohl(pakke->recvid));
  printf("metadata==0x%x, ",      pakke->metadata);
  printf("payload==\"%s\" }\n", pakke->payload);
  return EXIT_SUCCESS;
}

int rdp_printc(struct rdp_connection* con) {
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
  struct rdp pac;                    // svar på forespørsel
  struct rdp_connection *new_con;    // den potensielle nye forbinnelsen
  
  // Setter buffer `buf` som forespørsel skal mottas på til 0
  memset(&buf, '\0', sizeof buf);

  // Informasjon om avsender skal lagres i `sender`; dette er altså
  // svaradressen. Fjerner informasjonen som er der fra før.
  memset(&sender, '\0', sizeof sender);
  senderlen = sizeof sender;
  
  // Titter på pakke (BLOKKERER I/O!)
  if (rdp_peek(sockfd, buf, &sender, &senderlen) == NULL) return NULL;

  // Sjekker om pakken er en forbindelsesforespørsel
  if (((struct rdp *)&buf)->flag == 0x01) {
    // Mottar pakke, *for real* denne gangen (BLOKKERER I/O!)
    rc = recvfrom(sockfd,
                  buf,
                  sizeof buf,
                  0,
                  (struct sockaddr *)&sender,
                  &senderlen);
    if (rdp_error(rc, "rdp_accept: recvfrom")) return NULL;
    printf("rdp_accept: recvfrom: ny forbindelsesforspørsel!\n");
  } else {
    printf("rdp_accept: recvfrom: ikke en forspørsel; ignorerer\n");
    return NULL;
  }

  // Sender forespørselssvar
  memset(&pac, '\0', sizeof(pac));
  pac.flag     = accept ? 0x10 : 0x20;
  pac.senderid = htonl(assign_id);
  pac.recvid   = ((struct rdp *)&buf)->senderid;
  // TODO: metadata felt må fylles inn ved avslag på kobling
  // pac.metadata = accept ? 0x0 : <grunn til avslag>;
  wc = sendto(sockfd, (void *)&pac, sizeof pac, 0,
                  (struct sockaddr *)&sender, senderlen);
  if (rdp_error(wc, "rdp_accept: sendto")) return NULL; 

  // Hvis vi aksepterer, returnerer vi en ny kobling; ellers NULL.
  if (accept) {
    // Lager ny oppkobling
    new_con               = malloc(sizeof(struct rdp_connection));
    new_con->recipient    = malloc(sizeof(struct sockaddr_storage));
    new_con->recipientlen = senderlen;
    new_con->sockfd       = sockfd;
    new_con->senderid     = htonl(assign_id);
    new_con->recvid       = ((struct rdp *)&buf)->senderid;
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
    fprintf(stderr, "%srdp_connect: getaddrinfo: %s%s\n",
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

  // Binder UDP-støpselet til nettverksaddressen vi har funnet.
  rv = bind(sockfd, res->ai_addr, res->ai_addrlen);
  if (rdp_error(rv, "rdp_listen: bind")) return EXIT_FAILURE;

  return sockfd;
}

struct rdp_connection *rdp_connect(char* vert, char* port, int assign_id)
{
  int rv, wc;
  struct addrinfo hints, *res;
  struct rdp_connection *con;

  // Setter hints til `getaddrinfo`
  memset(&hints, '\0', sizeof hints); 
  hints.ai_family   = AF_UNSPEC;   // spesifierer ikke IP versjon
  hints.ai_socktype = SOCK_DGRAM;  // vi ønsker typen «datagram», for UDP
  hints.ai_flags    = AI_PASSIVE;  // bruker addresen til den lokale maskinen

  // Gjør DNS-oppslag
  rv = getaddrinfo(vert, port, &hints, &res);
  if (rv != 0) {
    fprintf(stderr, "%srdp_connect: getaddrinfo: %s%s\n",
            RED, gai_strerror(rv), RESET);
    return NULL;
  }

  // TODO: `res` er nå en lenket liste med svar fra DNS-oppslaget. Jeg bare
  // anntar at første resultat er gyldig, men ideellt bør jeg prøve alle
  // nodene før jeg gir opp.

  // Lager støpselfildeskriptor
  int sockfd;
  sockfd = socket(res->ai_family,    // bruker IP versjon fra DNS oppslaget
                  res->ai_socktype,  // blir SOCK_DGRAM, fra `hints` over
                  res->ai_protocol); // blir UDP, fra oppslaget
  if (rdp_error(sockfd, "rdp_connect: socket")) return NULL;

  // Finner en ledig nettverksport (på den lokale maskinen) som vi kan binde
  // oss til; vi *foretrekker* `port`-en som vi sendte inn som argument --
  // samme som serveren -- men vi nøyer oss med en hvilkensomhelst ledig en.
  rv = bind(sockfd, res->ai_addr, res->ai_addrlen);
  if (rdp_error(rv, "rdp_connect: bind")) {
    if (errno == 48) {
      printf("Ignorerer og antar RDP allerede er bundet til denne\n");
      printf("porten på den lokale maskinen. Lar OS-et tildele en\n");
      printf("tilfeldig annen port for denne forbindelsen.\n");
    } else return NULL;
  }

  // Sender forespørsel til server
  struct rdp pac;
  memset(&pac, '\0', sizeof(pac));
  pac.flag     = 0x01;
  pac.senderid = htonl(assign_id);
  pac.recvid   = htonl(0);
  wc = sendto(sockfd, (void *)&pac, sizeof pac, 0,
              (struct sockaddr *)res->ai_addr, res->ai_addrlen);
  if (rdp_error(wc, "rdp_connect: sendto")) return NULL; 

  // Lager RDP forbinndelsesstruktur
  con               = malloc(sizeof(struct rdp_connection));
  con->recipient    = malloc(sizeof(struct sockaddr_storage));
  con->recipientlen = res->ai_addrlen;
  con->sockfd       = sockfd;
  con->senderid     = pac.senderid;
  memset(con->recipient, '\0', sizeof(struct sockaddr_storage));
  memcpy(con->recipient, res->ai_addr, sizeof(struct sockaddr_storage));

  // Trenger ikke denne lenger
  freeaddrinfo(res);

  // Venter på svar fra server
  memset(&pac, '\0', sizeof(pac));
  if (rdp_read(con, &pac) == NULL) return NULL;

  rdp_print(&pac);
  // Hvis forespørselen blir avslått, frigjør vi koblingstrukturen vi
  // konstruerte og setter pekeren til NULL.
  if (pac.flag != 0x10) { 
    printf("%srdp_connect: forbindelse avslått av server%s\n", RED, RESET);
    free(con);
    con = NULL;
  } else {
    // Ellers, husker vi å sette ID-en vi har fått fra serveren.
    con->recvid = pac.senderid;
  }

  return con; // <-- enten NULL, eller `malloc`-et
}

int rdp_write(struct rdp_connection *con, struct rdp *pac)
{
  int wc;
  
  // Hvis vi ikke har mottatt en ACK på forrige pakke vi sendte, nekter denne
  // funksjonen å skrive neste pakke. Applikasjonen som kaller må vente på
  // en ACK på tidligere sendte pakke.
  if (con->ackseq < pac->pktseq) {
    return EXIT_FAILURE;
  }

  wc = sendto(con->sockfd, (void *)pac, sizeof *pac, 0,
                 (struct sockaddr *)con->recipient, con->recipientlen);
  if (rdp_error(wc, "rdp_write: sendto")) return EXIT_FAILURE;

  // TODO: vent på ACK. Send på nytt etter 100ms. Bør implementeres i egen
  // funksjon, tror jeg, som også oppdaterer `ackseq`.

  // Øker `pktseq` før vi returnerer.
  con->pktseq += 1;
  return EXIT_SUCCESS;
}

int rdp_ack(struct rdp_connection *con)
{
  int wc;
  struct rdp pkt; // ACK-pakken vi skal sende

  memset(&pkt, '\0', sizeof pkt);
  pkt.flag     = 0x08; // ACK-flagg
  pkt.senderid = con->senderid;
  pkt.recvid   = con->recvid;
  pkt.ackseq   = con->pktseq;

  wc = sendto(con->sockfd, (void *)&pkt, sizeof pkt, 0,
              (struct sockaddr *)con->recipient, con->recipientlen);
  if (rdp_error(wc, "rdp_ack: sendto")) return EXIT_FAILURE;
  else {
    // Noterer oss hva som er siste pakke vi har ACKet, hvis vi skulle
    // mota duplikat.
    con->ackseq = con->pktseq;
    return EXIT_SUCCESS;
  }
}

void *rdp_read(struct rdp_connection *con, void *dest_buf)
{
  int rc;  // «read count»
  void *buf[sizeof(struct rdp) + 1]; // hvor pakken mottas
  struct sockaddr_storage sender;    // lagring for avsenders addresse
  socklen_t senderlen;               // avsenderaddresse lengde

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
    buf[rc] = "";
    printf("rdp_read: recvfrom: %d bytes from \"%s\":\n",
           rc, _get_recipient_addr(s, con));
    rdp_print((struct rdp *)&buf);
  } else {
    // Skriver pakke til returbufferet
    memcpy(dest_buf, &buf, sizeof(struct rdp));
  }

  // Sender en ACK om pakken inneholder nyttelast
  if (((struct rdp *)&buf)->flag == 0x04) {
    if (((struct rdp *)&buf)->pktseq <= con->ackseq && con->pktseq != 0) {
      // Hvis pakkesekvendnummeret er lavere enn siste ACK-en vi sendte,
      // sender vi forrige ACK på nytt og kaster denne pakken.
      rdp_ack(con);
      printf("%srdp_read: mottokk duplikatpakke. Gjentar ACK.%s\n",
             RED, RESET);
      return NULL;
    } else {
      // Ellers setter øker vi `pktseq` og sender ACK på den nye pakken.
      con->pktseq = ((struct rdp *)&buf)->pktseq;
      rdp_ack(con);
    }
  }

  // Håndterer pACKe.
  if (((struct rdp *)&buf)->flag == 0x08) {
    // Oppdaterer `ackseq` i denne oppkoblingen.
    printf("rdp_read: dette er en ACK: `ackseq=%d`\n",
           ((struct rdp *)&buf)->ackseq);
    con->ackseq = ((struct rdp *)&buf)->ackseq;
  }

  return dest_buf;
}

// *Nesten* samme funksjon som `rdp_read`, men vi bare tar en titt på neste
// pakke, heller enn å faktisk fjerne den fra nettverskstøpselet.
void *rdp_peek(int sockfd, void *dest_buf, struct sockaddr_storage *dest_addr,
               socklen_t *dest_addrlen)
{
  int rc;  // «read count»
  void *buf[sizeof(struct rdp) + 1]; // hvor pakken mottas
  struct sockaddr_storage sender;    // lagring for avsenders addresse
  socklen_t senderlen;               // avsenderaddresse lengde

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
    // Noterer ned avsenders addresse. Hvis vi ikke har noe sted å notere
    // dette, dropper vi det i stillhet.
    memcpy(&sender, dest_addr, senderlen);
    *dest_addrlen = senderlen;
  }

  // Hvis `dest_buf` pekeren er `NULL`, velger vi heller å bare printe pakken
  if (dest_buf == NULL) {
    // Printer pakke
    buf[rc] = "";
    printf("rdp_peek: recvfrom: %d bytes:\n", rc);
    rdp_print((struct rdp *)&buf);
  } else {
    // Skriver pakke til returbufferet
    memcpy(dest_buf, &buf, sizeof(struct rdp));
  }

  return dest_buf;
}

int rdp_close(struct rdp_connection *con)
{
  if (con != NULL) {
    close(con->sockfd);
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

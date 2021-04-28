#include "rdp.h"

#define RED "\e[0;31m"
#define GRN "\e[0;32m"
#define YEL "\e[0;33m"
#define RESET "\e[0m"

int rdp_print(struct rdp *pakke)
{
  printf("{ ");
  printf("flag==%x, ",          pakke->flag);
  printf("pktseq==%x, ",        pakke->pktseq);
  printf("ackseq==%x, ",        pakke->ackseq);
  printf("senderid==%x, ",      pakke->senderid);
  printf("recvid==%x, ",        pakke->recvid);
  printf("metadata==%x, ",      pakke->metadata);
  printf("payload==\"%s\" }\n", pakke->payload);
  return EXIT_SUCCESS;
}

int rdp_printc(struct rdp_connection* con) {
  printf("id==%d, ", con->id);
  printf("sockfd==%d, ", con->sockfd);
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

struct rdp_connection *rdp_accept(int sockfd, int accept)
{
  int rc, wc;                        // read / write count
  void *buf[sizeof(struct rdp) + 1]; // lest data (pluss nullbyte)
  struct sockaddr_storage sender;    // avsender på forespørsel
  socklen_t senderlen;               // lengde på adresse
  struct rdp pac;                    // svar på forespørsel
  struct rdp_connection *new_connection;
  
  // Setter buffer `buf` som pakke skal mottas på til 0
  memset(&buf, '\0', sizeof buf);

  // Informasjon om avsender skal lagres i `sender`; dette er altså
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
  if (rdp_error(rc, "rdp_accept: recvfrom")) return NULL;

  //// Henter vertsnavn til avsender:
  //char s[INET6_ADDRSTRLEN];
  //inet_ntop(sender.ss_family, _get_addr(&sender), s, sizeof s);
  //// Printer pakke
  //buf[rc] = "";
  //printf(YEL);
  //printf("rdp_accept: recvfrom: %d bytes from \"%s\": ", rc, s);
  //rdp_print((struct rdp *) &buf);
  //printf(RESET);
  //fflush(stdout);

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

  // Lager ny oppkobling
  new_connection               = malloc(sizeof(struct rdp_connection));
  new_connection->recipient    = malloc(sizeof(struct sockaddr_storage));
  new_connection->recipientlen = senderlen;
  new_connection->sockfd       = sockfd;
  memset(new_connection->recipient, '\0', sizeof(struct sockaddr_storage));
  memcpy(new_connection->recipient, &sender, senderlen);

  // Sender forespørselssvar
  memset(&pac, '\0', sizeof(pac));
  pac.flag = accept ? 0x10 : 0x20;
  wc = rdp_write(new_connection, &pac);
  if (rdp_error(wc, "rdp_write: sendto")) return NULL; 

  // Hvis vi aksepterer, returnerer vi den nye forbinnelsen, ellers rydder vi
  // opp og returnerer NULL
  if (accept) return new_connection;
  rdp_close(new_connection);
  return NULL;
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

struct rdp_connection *rdp_connect(char* vert, char* port)
{
  int rv;
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

  // Lager RDP forbinndelsesstruktur
  con               = malloc(sizeof(struct rdp_connection));
  con->recipient    = malloc(sizeof(struct sockaddr_storage));
  con->recipientlen = res->ai_addrlen;
  con->sockfd       = sockfd;
  memset(con->recipient, '\0', sizeof(struct sockaddr_storage));
  memcpy(con->recipient, res->ai_addr, sizeof(struct sockaddr_storage));

  // Trenger ikke denne lenger
  freeaddrinfo(res);

  // Sender forespørsel til server
  struct rdp pac;
  memset(&pac, '\0', sizeof(pac));
  pac.flag = 0x01;
  rdp_write(con, &pac);

  // Venter på svar fra server
  memset(&pac, '\0', sizeof(pac));
  rdp_read(con, &pac);

  if (pac.flag == 0x10) { 
    return con;
  } else {
    rdp_close(con);
    return NULL;
  }
}

int rdp_write(struct rdp_connection *connection, struct rdp *pakke)
{
  int sendt = sendto(connection->sockfd,
                     (void *)pakke,
                     sizeof *pakke,
                     0,
                     (struct sockaddr *)connection->recipient,
                     connection->recipientlen);
  if (rdp_error(sendt, "rdp_write: sendto")) return EXIT_FAILURE;
  return EXIT_SUCCESS;
}

void *rdp_read(struct rdp_connection *connection, void *dest_buf)
{
  int rc;
  struct sockaddr_storage sender;

  // Select:
  
  // TODO

  // Lager buffer `buf` som pakke skal mottas på
  void *buf[sizeof(struct rdp) + 1];
  memset(&buf, '\0', sizeof buf);

  // Informasjon om avsender skal lagres i `recipient`; dette er altså
  // svaradressen. Fjerner informasjonen som er der fra før.
  memset(&sender, '\0', sizeof sender);
  socklen_t senderlen = sizeof sender;
  
  // Mottar pakke (BLOKKERER I/O!)
  rc = recvfrom(connection->sockfd,
                  buf,
                  sizeof buf,
                  0,
                  (struct sockaddr *)&sender,
                  &senderlen);
  if (rdp_error(rc, "rdp_read: recvfrom")) return NULL;

  if (dest_buf == NULL) {
    // Henter avsender
    char s[INET6_ADDRSTRLEN];
    inet_ntop(sender.ss_family, _get_addr(&sender), s, sizeof s);
    // Printer pakke
    buf[rc] = "";
    printf(YEL);
    printf("rdp_read: recvfrom: %d bytes from \"%s\": ", rc, s);
    rdp_print((struct rdp *) &buf);
    printf(RESET);
    fflush(stdout);
  } else {
    // Skriver pakke til returbufferet
    memcpy(dest_buf, &buf, sizeof(struct rdp));
  }

  // Overskriver `recipient` med avsenders addresse
  memcpy(&sender, connection->recipient, senderlen);

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

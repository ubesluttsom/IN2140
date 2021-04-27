#include <stdio.h>
#include <stdlib.h>

#include "rdp.h"

#include <sys/errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <arpa/inet.h>  // inet_ntop

#include <unistd.h>   // `close()`


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
  _get_recipient_addr(s, con);
  printf("recipient->addr==%s, ", s);
  _get_recipient_port(s, con);
  printf("recipient->port==%s, ", s);
  printf("recipientlen==%d }\n", con->recipientlen);
  return EXIT_SUCCESS;
}

struct rdp_connection *rdp_accept(int sockfd)
{
  
  // Lager buffer `buf` som pakke skal mottas på
  void *buf[sizeof(struct rdp) + 1];
  memset(&buf, '\0', sizeof buf);

  // Informasjon om avsender skal lagres i `sender`; dette er altså
  // svaradressen. Fjerner informasjonen som er der fra før.
  struct sockaddr_storage sender;
  memset(&sender, '\0', sizeof sender);
  socklen_t senderlen = sizeof sender;
  
  // Titter på pakke (BLOKKERER I/O!)
  int recv;
  if ((recv = recvfrom(sockfd, buf, sizeof buf, MSG_PEEK,
                       (struct sockaddr *)&sender,
                       &senderlen)) <= 0) {
    printf("RECV: %s\n", strerror(errno));
    return NULL;
  }

  // Printer pakke

  // Henter vertsnavn til avsender:
  char s[INET6_ADDRSTRLEN];
  inet_ntop(sender.ss_family, _get_addr(&sender), s, sizeof s);
  buf[recv] = "";
  printf("RECV: %d bytes from \"%s\": ", recv, s);
  rdp_print((struct rdp *) &buf);

  if (((struct rdp *)&buf)->flag == 0x01) {
    // Mottar pakke, for real denne gangen (BLOKKERER I/O!)
    if ((recv = recvfrom(sockfd, buf, sizeof buf, 0,
                         (struct sockaddr *)&sender,
                         &senderlen)) <= 0) {
      printf("RECV: %s\n", strerror(errno));
      return NULL;
    }
    printf("RECV: tolker som forbindelsesforspørsel siden `flag == 0x01`\n");
  } else {
    printf("RECV: ikke en forspørsel siden `flag != 0x01`: ignorerer\n");
    return NULL;
  }

  // Lager ny oppkobling

  struct rdp_connection *new_connection = malloc(sizeof(struct rdp_connection));

  new_connection->sockfd = sockfd;
  new_connection->recipient = malloc(sizeof(struct sockaddr_storage));

  memset(new_connection->recipient, '\0', sizeof(struct sockaddr_storage));
  memcpy(new_connection->recipient, &sender, senderlen);

  new_connection->recipientlen = senderlen;

  return new_connection;
}

struct rdp_connection *rdp_connect(char* vert, char* port, struct sockaddr_storage *ss)
{
  // Binder en socket til port. Jeg har lent meg på denne guiden:
  // <https://beej.us/guide/bgnet/html/index-wide.html>

  struct addrinfo hints, *res;

  memset(&hints, '\0', sizeof hints); 
  hints.ai_family   = AF_UNSPEC;   // spesifierer ikke IP versjon
  hints.ai_socktype = SOCK_DGRAM;  // vi ønsker typen «datagram», for UDP
  hints.ai_flags    = AI_PASSIVE;  // bruker addresen til den lokale maskinen
  hints.ai_addr     = (struct sockaddr *)ss;  /* socket-address for socket */

  int error;
  if ((error = getaddrinfo(vert, port, &hints, &res)) != 0) {
    printf("GETADDRINFO(): %s\n", gai_strerror(error));
    return NULL;
  }

  int sockfd;
  if ((sockfd = socket(res->ai_family, res->ai_socktype,
                       res->ai_protocol)) != -1) {
    printf("SOCKET: %d\n", sockfd);
  } else {
    printf("SOCKET: %s\n", strerror(errno));
    return NULL;
  }

  // int true = 1
  // setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &true, sizeof(int));
  // setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &true, sizeof(int));

  // TODO: Dette virker litt «hacky»; fins det en bedre løsning?
  if (bind(sockfd, res->ai_addr, res->ai_addrlen) == 0) {
    printf("BIND: suksess\n");
  } else {
    printf("BIND: %s.\n", strerror(errno));
    if (errno == 48) {
      printf("      Ignorerer og antar RDP allerede er bundet til denne\n");
      printf("      porten på den lokale maskinen. Lar OS-et tildele en\n");
      printf("      tilfeldig annen port for denne forbindelsen.\n");
    } else return NULL;
  }

  struct rdp_connection *session = malloc(sizeof(struct rdp_connection));

  session->sockfd = sockfd;
  session->recipient = malloc(sizeof(struct sockaddr_storage));

  memset(session->recipient, '\0', sizeof(struct sockaddr_storage));
  memcpy(session->recipient, res->ai_addr, sizeof(struct sockaddr_storage));

  session->recipientlen = res->ai_addrlen;

  freeaddrinfo(res);

  return session;
}

int rdp_write(struct rdp_connection *connection, struct rdp *pakke)
{
  int sendt;
  if ((sendt = sendto(connection->sockfd, (void *) pakke,
                      sizeof *pakke, 0,
                      (struct sockaddr *)connection->recipient,
                      connection->recipientlen)) > 0) {
    printf("SENDTO: sendte %d bytes\n", sendt);
  } else {
    printf("SENDTO: %s\n", strerror(errno));
    return EXIT_FAILURE;
  }
  return 1;
}

int rdp_read(struct rdp_connection *connection)
{
  // Select:
  
  // TODO

  // Lager buffer `buf` som pakke skal mottas på
  void *buf[sizeof(struct rdp) + 1];
  memset(&buf, '\0', sizeof buf);

  // Informasjon om avsender skal lagres i `recipient`; dette er altså
  // svaradressen. Fjerner informasjonen som er der fra før.
  struct sockaddr_storage sender;
  memset(&sender, '\0', sizeof sender);
  socklen_t senderlen = sizeof sender;
  //memset(&(connection->recipient), '\0', sizeof(connection->recipient));
  //connection->recipientlen = sizeof(connection->recipient);
  
  // Titter på pakke (BLOKKERER I/O!)
  int recv;
  if ((recv = recvfrom(connection->sockfd, buf, sizeof buf, MSG_PEEK,
                       (struct sockaddr *)&sender,
                       &senderlen)) <= 0) {
    printf("RECV: %s\n", strerror(errno));
    return EXIT_FAILURE;
  }

  //// Mottar pakke (BLOKKERER I/O!)
  //if ((recv = recvfrom(connection->sockfd, buf, sizeof buf, 0,
  //                     //(struct sockaddr *)connection->recipient,
  //                     (struct sockaddr *)&sender,
  //                     //&(connection->recipientlen),
  //                     &senderlen)) <= 0) {
  //  printf("RECV: %s\n", strerror(errno));
  //  return EXIT_FAILURE;
  //}

  // Printer pakke
  char s[INET6_ADDRSTRLEN];
  inet_ntop(sender.ss_family, _get_addr(&sender), s, sizeof s);
  buf[recv] = "";
  printf("RECV: %d bytes from \"%s\": ", recv, s);
  rdp_print((struct rdp *) &buf);

  memcpy(&sender, connection->recipient, senderlen);

  return EXIT_SUCCESS;
}

int rdp_close(struct rdp_connection *connection)
{
  close(connection->sockfd);
  free(connection->recipient);
  free(connection);
  return EXIT_SUCCESS;
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

void _get_recipient_addr(char *s, struct rdp_connection *connection) {
  inet_ntop(connection->recipient->ss_family, _get_addr(connection->recipient), s, sizeof s);
  return;
}

void _get_recipient_port(char *s, struct rdp_connection *connection) {
  u_int16_t port;
  if (connection->recipient->ss_family == AF_INET) {
    port = ((struct sockaddr_in *)connection->recipient)->sin_port;
  } else if (connection->recipient->ss_family == AF_INET6) {
    port = ((struct sockaddr_in6 *)connection->recipient)->sin6_port;
  } else {
    printf("_get_recipient_port: feil i `ss_family` til `struct sockaddr_storage`\n");
    exit(-1);
  }
  inet_ntop(connection->recipient->ss_family, &port, s, sizeof s);
  return;
}

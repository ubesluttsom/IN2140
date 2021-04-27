// Standardbibliotek.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>  // inet_ntop

// RDP definisjon
#include "rdp.h"


/* MAIN */

int main(int argc, char *argv[])
{
  if (argc != 2) {
    printf("Feil antall argumenter: per nå kun implementert for 2:\n");
    printf("usage: %s <port>\n", argv[0]);
    return EXIT_FAILURE;
  }

  char *port = argv[1];

  printf("Starter server.\n");

  struct rdp_connection *session = rdp_connect(NULL, port, NULL);
  if (session == NULL) {
    printf("RDP: klarte ikke opprette forbindelse\n");
    return EXIT_FAILURE;
  }
  

  // LYTTE SOCKET
  // 
  //   struct addrinfo hints, *res;
  // 
  //   memset(&hints, '\0', sizeof hints); // sikrer oss mot tilfelige problemer
  //   hints.ai_family   = AF_UNSPEC;   // spesifierer ikke IP versjon
  //   hints.ai_socktype = SOCK_DGRAM;  // vi ønsker typen «datagram», for UDP
  //   hints.ai_flags    = AI_PASSIVE;  // bruker addresen til den lokale maskinen
  // 
  //   int error;
  //   if ((error = getaddrinfo(NULL, port, &hints, &res)) != 0) {
  //     printf("GETADDRINFO(): %s\n", gai_strerror(error));
  //     return EXIT_FAILURE;
  //   }
  // 
  //   int sockfd;
  //   if ((sockfd = socket(res->ai_family, res->ai_socktype,
  //                        res->ai_protocol)) != -1) {
  //     printf("SOCKET: %d\n", sockfd);
  //   } else {
  //     printf("SOCKET: %s\n", strerror(errno));
  //     return EXIT_FAILURE;
  //   }
  // 
  //   if (bind(sockfd, res->ai_addr, res->ai_addrlen) == 0) {
  //     printf("BIND: suksess\n");
  //   } else {
  //     printf("BIND: %s.\n", strerror(errno));
  //     return EXIT_FAILURE;
  //   }
  // 
  //   // LYTT og ACCEPT
  // 
  //   printf("LYTTER PÅ PORT %s\n", port);
  // 
  struct rdp_connection *new_connection = rdp_accept(session->sockfd); // blokkerer i/o
  if (new_connection != NULL) {
    struct rdp pakke;
    pakke.flag       = 0x04;
    pakke.pktseq     = 0x0;
    pakke.ackseq     = 0x0;
    pakke.unassigned = 0x0;
    pakke.senderid   = 0x0;
    pakke.recvid     = 0x0;
    pakke.metadata   = 0x0;
    memset(pakke.payload, '\0', sizeof(pakke.payload));
    char s[] = "Svar.";
    memcpy(pakke.payload, s, sizeof s);
    rdp_write(new_connection, &pakke);
  } else {
    printf("RDP: error: new_connection == NULL\n");
  }
  // 
  // 
  //   // READ
  // 
  //   // Lager buffer `buf` som pakke skal mottas på
  //   void *buf[sizeof(struct rdp) + 1];
  //   memset(&buf, '\0', sizeof buf);
  // 
  //   // Informasjon om avsender skal lagres i `recipient`; dette er altså
  //   // svaradressen. Fjerner informasjonen som er der fra før.
  //   struct sockaddr_storage sender;
  //   memset(&sender, '\0', sizeof sender);
  //   socklen_t senderlen = sizeof sender;
  // 
  //   // Mottar pakke (BLOKKERER I/O!)
  //   int recv;
  //   if ((recv = recvfrom(sockfd, buf, sizeof buf, 0,
  //                        (struct sockaddr *)&sender,
  //                        &senderlen)) <= 0) {
  //     printf("RECV: %s\n", strerror(errno));
  //     return EXIT_FAILURE;
  //   }
  // 
  //   // Printer pakke
  //   char s[INET6_ADDRSTRLEN];
  //   inet_ntop(sender.ss_family, _get_addr(&sender), s, sizeof s);
  //   buf[recv] = "";
  //   printf("RECV: %d bytes from %s: ", recv, s);
  //   rdp_print((struct rdp *) &buf);
  // 
  // 
  //   // ============================
  // 
  //   struct rdp pakke;
  //   pakke.flag       = 0x04;
  //   pakke.pktseq     = 0x0;
  //   pakke.ackseq     = 0x0;
  //   pakke.unassigned = 0x0;
  //   pakke.senderid   = 0x0;
  //   pakke.recvid     = 0x0;
  //   pakke.metadata   = 0x0;
  //   memset(pakke.payload, '\0', sizeof(pakke.payload));
  //   memcpy(pakke.payload, "Her er et svar.", 15);
  // 
  //   // Lager send socket:
  // 
  //   struct addrinfo send_hints, *send_res;
  // 
  //   memset(&send_hints, '\0', sizeof send_hints); // sikrer oss mot tilfelige problemer
  //   hints.ai_family   = AF_UNSPEC;   // spesifierer ikke IP versjon
  //   hints.ai_socktype = SOCK_DGRAM;  // vi ønsker typen «datagram», for UDP
  //   hints.ai_flags    = AI_PASSIVE;  // bruker addresen til den lokale maskinen
  //   hints.ai_addr     = (struct sockaddr *)&sender;  // henter svaraddresse fra sender
  // 
  //   if ((error = getaddrinfo(NULL, port, &send_hints, &send_res)) != 0) {
  //     printf("GETADDRINFO(): %s\n", gai_strerror(error));
  //     return EXIT_FAILURE;
  //   }
  // 
  //   int send_sockfd;
  //   if ((send_sockfd = socket(send_res->ai_family, send_res->ai_socktype,
  //                        send_res->ai_protocol)) != -1) {
  //     printf("SOCKET: %d\n", send_sockfd);
  //   } else {
  //     printf("SOCKET: %s\n", strerror(errno));
  //     return EXIT_FAILURE;
  //   }
  // 
  //   int true = 1;
  //   setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &true, sizeof(int));
  // 
  //   if (bind(send_sockfd, send_res->ai_addr, send_res->ai_addrlen) == 0) {
  //     printf("BIND: suksess\n");
  //   } else {
  //     printf("BIND: %s.\n", strerror(errno));
  //     return EXIT_FAILURE;
  //   }
  // 
  //   // Sender data:
  // 
  //   int sendt;
  //   if ((sendt = sendto(send_sockfd, &pakke,
  //                       sizeof(pakke)+1, 0,
  //                       send_res->ai_addr,
  //                       send_res->ai_addrlen)) > 0) {
  //     printf("SENDTO: sendte %d bytes\n", sendt);
  //   } else {
  //     printf("SENDTO: %s\n", strerror(errno));
  //     return EXIT_FAILURE;
  //   }

  // Prøver å svare:
  //char s[INET6_ADDRSTRLEN];
  //_get_recipient_addr(s, session);
  //printf("RDP: %s\n", s);
  //struct rdp_connection *session2 = rdp_connect(s, NULL, session->recipient);
  //if (session2 == NULL) {
  //  printf("RDP: klarte ikke opprette forbindelse\n");
  //  return EXIT_FAILURE;
  //}

  //rdp_write(session2, (unsigned char *) "Her er et svar.", 15);

  // FRIGJØR MINNE

  rdp_close(session);

  return EXIT_SUCCESS;
}

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
  int listen_fd;

  printf("Starter server.\n");

  // TODO: lage en bedre, mer rettet funksjon for oppkobling av serveren?
  listen_fd = rdp_listen(port);
  if (listen_fd == EXIT_FAILURE) return EXIT_FAILURE; 

  struct rdp_connection *new_con = NULL;
  for (int i = 2; i > 0; --i) {
    new_con = rdp_accept(listen_fd, TRUE); // blokkerer i/o
    if (new_con != NULL) {
      // Sender et et svar.
      struct rdp pakke;
      memset(&pakke, '\0', sizeof pakke);
      pakke.flag = 0x04;
      strcpy(pakke.payload, "... data ...");
      rdp_write(new_con, &pakke);
    } else {
      printf("RDP: ingen nye forbinnelser, leser andre pakker\n");
      // Lager buffer `buf` som pakke skal mottas på
      void *buf[sizeof(struct rdp) + 1];
      memset(&buf, '\0', sizeof buf);
      int rc = recv(listen_fd, buf, sizeof buf, 0);
      rdp_error(rc, "RDP: recv");
      // Printer pakke
      buf[rc] = "";
      printf("rdp_read: recv: %d bytes: ", rc);
      rdp_print((struct rdp *)&buf);
    }
  //  sleep(3);
  }

  // FRIGJØR MINNE

  close(listen_fd);
  rdp_close(new_con);

  return EXIT_SUCCESS;
}

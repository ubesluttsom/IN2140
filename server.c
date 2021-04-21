// Standardbibliotek.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>


/* MAIN */

int main(int argc, char* argv[])
{
  printf("Starter server.\n");

  int socket = socket(AF_INET, SOCK_DGRAM, 17);

  while (1) {
    sleep(1);
    printf("Nå har det gått 1 sekund.\n");
  }

  return 1;
}

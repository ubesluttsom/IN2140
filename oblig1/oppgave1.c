#include <stdio.h>

int main(int argc, char **argv)
{
  if (argc != 4) {
    // Hvis antall argumenter er feil printer vi bruk, og avslutter
    printf("bruk: oppgave1 <streng> <bokstav> <erstattning>\n");
    return 1;
  }

  // Ittererer over alle karakterene i argumentet <streng> helt til vi når en
  // \0 byte, mao. enden av strengen
  for (int i = 0; argv[1][i] != 0; i++) {
    // Hvis `c` variablen sammsvarer med argumentet <bokstav>
    if (argv[1][i] == *argv[2]) {
      // Erstatt <bokstav> med <erstattning>
      argv[1][i] = *argv[3];
    }
  }

  // Print ut strengen som vi har endret på
  printf("%s\n", &argv[1][0]);

  return 0;
}

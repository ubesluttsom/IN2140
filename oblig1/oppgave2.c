#include <stdlib.h>

int stringsum (char* s)
{
  // Initialiserer en teller.
  int sum = 0;
  // Ittererer over strengen `s` til vi finner en \0.
  for (int i = 0; s[i] != 0; i++) {
    // Teller tallene basert på deres verdi i ASCII; 65–90 er majuskler, 96–122
    // er minuskler. Er tegnet utenfor noen av de spennene, returneres
    // feilverdien -1.
    if      (s[i] > 64 && s[i] < 91)  sum += s[i] - 64;
    else if (s[i] > 96 && s[i] < 123) sum += s[i] - 96;
    else return -1;
  }
  return sum;
}

int distance_between (char* s, char c)
{
  // Ittererer over strengen `s` til vi finner \0.
  for (int i = 0, start = 0; s[i] != 0; i++) {
    // Hvis vi finner et tilfellet av tegnet `c`:
    if (s[i] == c) {
      // Hvis `start` (som marker første tilfellet av `c` i strengen) ikke er
      // 0, returnerer vi differansen mellom nåværende plassering og
      // startverdien.
      if (start) return i + 1 - start;
      // Ellers, hvis `start` ikke er satt ennå setter vi start til nåværende
      // plassering (pluss 1 for å ta høyde for når s[0] == c, da vil if-testen
      // over fortsatt fungere).
      else start = i + 1;
    }
  }
  // Hvis \0 i strengen `s` blir nådd, har vi ikke funnet nok `c`-er, og
  // returnerer feilverdien -1.
  return -1;
}

char* string_between (char* s, char c)
{
  // Gjenbruker funksjonen fra forrige deloppgave for å vurdere nøyaktig hvor
  // mye minne vi vil allokere.
  int lengde = distance_between(s, c);
  // Vi utnytter at forrige deloppgave har akkurat samme feilstatus som
  // denne, og videreformidler dette.
  if (lengde == -1) return NULL;

  // Allokkerer riktig mengde minne.
  char* retur_streng = malloc(sizeof(char)*lengde);

  // Ittererer over strengen `s` til vi finner \0.
  for (int i = 0; s[i] != 0; i++) {
    // Når vi møter på første tilfellet av `c` ...
    if (s[i] == c) {
      i++;
      // ... kopier over tegn for tegn fra `s` til `retur_streng`, inntil vi
      // støter på andre forekomst av `c` ...
      for (int j = 0; s[i] != c; j++, i++) {
        retur_streng[j] = s[i];
      }
      // ... og returner denne nye strengen.
      return retur_streng;
    }
  }

  // Eksekveringen bør i teorien aldri komme hit.
  return NULL;
}

void stringsum2 (char* s, int* res)
{
  // Derefererer pekeren og setter verdien med fuksjonen som alt er skrevet.
  *res = stringsum(s);
  return;
}

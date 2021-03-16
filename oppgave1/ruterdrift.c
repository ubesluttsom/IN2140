// Inkluderer ruterstuktur- og funksjonsdeklarasjon.
#include "ruterdrift.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* MAIN */

int main(/* int argc, char* argv[] */) {
  // struct ruter * frakoblinger = malloc(sizeof(struct ruter *) * 10);
  // struct ruter * tilkoblinger = malloc(sizeof(struct ruter *) * 10);
  struct ruter ruter1 = ruter(37, 16, 5, "Zyxel", NULL, NULL, 0, 0);
  struct ruter ruter2 = ruter(42, 19, 7, "Netgear", NULL, NULL, 0, 0);
  legg_til_kobling(&ruter1, &ruter2);
  printr(ruter1);
  printr(ruter2);
  sett_flagg(&ruter1, 0, 1);
  sett_flagg(&ruter1, 1, 1);
  sett_flagg(&ruter1, 2, 1);
  sett_flagg(&ruter1, 4, 8);
  sett_flagg(&ruter1, 4, 16);
  sett_modell(&ruter1, "D-link");
  printr(ruter1);
}


/* HJELPEFUNKSJONER */

// Oppretter en ny ruterstruktur.
struct ruter ruter(
    unsigned char  ruter_id,
    unsigned char  flagg,
    int            prod_modell_lengde,
    char *         prod_modell,
    struct ruter * tilkoblinger,
    struct ruter * frakoblinger,
    unsigned short aktive_tilkoblinger,
    unsigned short aktive_frakoblinger) {
  // Lager ny ruter struktur, noen vriabler er null midlertidig.
  struct ruter ny_ruter = { ruter_id, flagg, {NULL}, {NULL}, {NULL}, 0, 0 };

  // Kopierer over prod./mod. streng, byte for byte.
  strncpy((char *) ny_ruter.prod_modell,
          prod_modell, prod_modell_lengde);

  // Kopierer over rutertilkoblinger (hvis det er noen) ...
  if (tilkoblinger != NULL) {
    memcpy(ny_ruter.tilkoblinger, tilkoblinger, 10);
    ny_ruter.aktive_tilkoblinger = aktive_tilkoblinger;
  }
  // ... og ruterfrakoblinger
  if (frakoblinger != NULL) {
    memcpy(ny_ruter.frakoblinger, frakoblinger, 10);
    ny_ruter.aktive_frakoblinger = aktive_frakoblinger;
  }

  return ny_ruter;
}

// Lager en kobling fra ruter `kilde` til ruter `dest`; oppdaterer variablene
// som «husker» hvilke koblinger som er aktive eller ikke, `aktive_frakoblinger`
// og `aktive_tilkoblinger`.
int legg_til_kobling(struct ruter * kilde, struct ruter * dest) {
  // Finner ledige «hull» i arrayene med koblinger.
  int i = 9, j = 9;
  while (kilde->aktive_frakoblinger >> i) {
    i--;
  }
  while (dest->aktive_tilkoblinger >> j) {
    j--;
  }

  // Sjekker om vi har overskridet kapasiteten på 10 plasser i arrayene.
  if (i < 0) {
    printf("Error: koblinger i ruter %d overfyldt! "
           "Ignorerer kommando.\n", kilde->ruter_id);
    return 0;
  } else if (j < 0) {
    printf("Error: koblinger i ruter %d overfyldt! "
           "Ignorerer kommando.\n", dest->ruter_id);
    return 0;
  }

  // Setter inn `til` på den første ledige plassen som vi fant.
  kilde->frakoblinger[i] = dest;
  dest->tilkoblinger[j]  = kilde;

  // Oppdaterer hvilke koblinger som er aktive med litt bitfikling.
  kilde->aktive_frakoblinger = (1 << i) | kilde->aktive_frakoblinger;
  dest->aktive_tilkoblinger  = (1 << j) | dest->aktive_frakoblinger;
  return 1;
}

/* Printer ruterstrukturer */
void printr(struct ruter ruter) {
  printf("Ruter-ID:\t%d\n",  ruter.ruter_id);

  // Printer flagg og gjør bit-oprerasjoner for å vise hva de betyr
  printf("Flagg:\t\t%#x\n", ruter.flagg);
  if (ruter.flagg & 1) printf(" - Er bruk:\tJA\n");
  else                 printf(" - Er bruk:\tNEI\n");
  if (ruter.flagg & 2) printf(" - Trådløs:\tJA\n");
  else                 printf(" - Trådløs:\tNEI\n");
  if (ruter.flagg & 4) printf(" - 5 GHz:\tJA\n");
  else                 printf(" - 5 GHz:\tNEI\n");
  printf(" - Endr.nr.:\t%d\n", ruter.flagg >> 4);

  printf("Prod./modell:\t%s\n", (char*) ruter.prod_modell);

  printf("Tilkoblinger:\n");
  for (int i = 9; i >= 0; i--) {
    if ((1 << i) & ruter.aktive_tilkoblinger) {
      printf(" <- %d\n", ruter.tilkoblinger[i]->ruter_id);
    }
  }

  printf("Frakoblinger:\n");
  for (int i = 9; i >= 0; i--) {
    if ((1 << i) & ruter.aktive_frakoblinger) {
      printf(" -> %d\n", ruter.frakoblinger[i]->ruter_id);
    }
  }
}

// Setter endrer på flagget i ruter. Gjør dette ved å sette et gitt
// `flagg_bit` til en `ny_verdi`. Sjekker også om argumentene er gyldige.
int sett_flagg(struct ruter * ruter, int flagg_bit, int ny_verdi) {
  if (flagg_bit < 0 || flagg_bit == 3 || flagg_bit > 4) {
    printf("Error: `%d` er ikke et gyldig flagg som kan endres. "
           "Ignorerer kommando.\n", flagg_bit);
    return 0;
  }
  if (flagg_bit != 4 && (ny_verdi < 0 || ny_verdi > 1) ) {
    printf("Error: `%d` er ikke en gyldig verdi for flagg `%d`: "
           "`verdi` == { 0, 1 }. Ignorerer kommando.\n", ny_verdi, flagg_bit);
    return 0;
  } else if (flagg_bit == 4 && (ny_verdi < 0 || ny_verdi >= 0xF) ) {
    printf("Error: `%d` er ikke en gyldig verdi for flagg 4 (endringsnummer): "
           "0 <= `verdi` >= 0xF. Ignorerer kommando.\n", ny_verdi);
    return 0;
  }

  // Maskerer vekk `flagg_bit` fra ruterens flagg, og ORer det med `ny_verdi`.
  ruter->flagg = (ruter->flagg & ~(1 << flagg_bit)) | (ny_verdi << flagg_bit);
  return 1;
}

// Setter et nytt navn til en ruter. Vil først sjekke at nytt navn faktisk får
// plass i ruterstrukturen vår. `strncpy` vil fylle opp `ruter->prod_modell`
// med akkurat 249 bytes. For lange strenger kuttes av. Resterende plass i
// char-arrayet med `\0`.
int sett_modell(struct ruter * ruter, char * ny_prod_modell) {
  strncpy((char *) ruter->prod_modell, ny_prod_modell, 249);
  return 0;
}

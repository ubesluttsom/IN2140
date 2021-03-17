// Inkluderer ruterstuktur- og funksjonsdeklarasjon.
#include "ruterdrift.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* MAIN */

int main(int argc, char* argv[]) {
  if (argc != 2) {
    if (argc < 2) printf("Error: Inputfil må spesifiseres. Avslutter ... \n");
    if (argc > 2) printf("Error: Tar kun et argument. Avslutter ... \n");
    return EXIT_FAILURE;
  }

  printf("Åpner fil: %s ... \n", argv[1]);

  char* filnavn = argv[1];
  FILE* fil = fopen(filnavn, "r");

  if(!fil) {
    printf("Error: Filåpning feilet!");
    return EXIT_FAILURE;
  }

  struct ruter_array * data = innlesing(fil);

  for (int i = 0; i < data->antall; ++i) {
    printr(data->rutere[i]);
  }

  free_ruter_array(data);
}


/* HJELPEFUNKSJONER */

// Oppretter en ny ruterstruktur.
struct ruter ruter(
    int            ruter_id,
    unsigned char  flagg,
    char *         prod_modell) {
  // Lager ny ruter struktur, noen variabler er null inntil videre.
  struct ruter ny_ruter = { ruter_id, flagg, {NULL}, {NULL}, {NULL}, 0, 0 };

  // Kopierer over prod./mod. streng, byte for byte. Egentlig bare et innpakket
  // `strncpy` kall, men jeg får vel gjenbruke kode hvor jeg kan.
  sett_modell(&ny_ruter, prod_modell);

  return ny_ruter;
}

// Lager en kobling fra ruter `kilde` til ruter `dest`; oppdaterer variablene
// som «husker» hvilke koblinger som er aktive eller ikke, `aktive_frakoblinger`
// og `aktive_tilkoblinger`.
int legg_til_kobling(struct ruter * kilde, struct ruter * dest) {
  // Finner ledige «hull» i arrayene med koblinger.
  int i = 0, j = 0;
  while ((kilde->aktive_frakoblinger >> i) & 1) {
    ++i;
  }
  while ((dest->aktive_tilkoblinger >> j) & 1) {
    ++j;
  }

  // Sjekker om vi har overskridet kapasiteten på 10 plasser i arrayene.
  if (i > 9) {
    printf("Error: koblinger i ruter %d overfyldt! "
           "Ignorerer kommando.\n", kilde->ruter_id);
    return 0;
  } else if (j > 9) {
    printf("Error: koblinger i ruter %d overfyldt! "
           "Ignorerer kommando.\n", dest->ruter_id);
    return 0;
  }

  // Setter inn `til` på den første ledige plassen som vi fant.
  kilde->frakoblinger[i] = dest;
  dest->tilkoblinger[j]  = kilde;

  // Oppdaterer hvilke koblinger som er aktive med litt bitfikling.
  kilde->aktive_frakoblinger = (1 << i) | kilde->aktive_frakoblinger;
  dest->aktive_tilkoblinger  = (1 << j) | dest->aktive_tilkoblinger;
  return 1;
}

// Printer ruterstrukturer
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
  for (int i = 0; i < 10; ++i) {
    if ((1 << i) & ruter.aktive_tilkoblinger) {
      printf(" <- %d\n", ruter.tilkoblinger[i]->ruter_id);
    }
  }

  printf("Frakoblinger:\n");
  for (int i = 0; i < 10; ++i) {
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


/* FILINNLESING */

struct ruter_array * innlesing(FILE* fil) {

  struct ruter_array * data = malloc(sizeof(struct ruter_array));

  // Leser inn anntall rutere fra fil, lagrer i struktur.
  fread(&data->antall, sizeof(int), 1, fil);
  printf("Antall rutere i fil: %d\n", data->antall);

  // Allokerer plass til antall rutere spesifisert i filen. Innleveringen
  // definerer ingen komando for opprettelse av nye rutere, så det er ingen
  // behov for å individuelt allokere plass for hver ruter; tenker det er
  // greiest å allokere én stor chunk, som trenger bare et kall på `free` ved
  // terminering av programmet.
  data->rutere = malloc(sizeof(struct ruter)*data->antall);

  // Leser inn «N» blokker med ruterinformasjon og oppretter en ruter for hver
  // av dem.
  for (int i = 0; i < data->antall; ++i) {
    // Leser inn info.
    int ruter_id;
    fread(&ruter_id, sizeof(int), 1, fil);
    unsigned char flagg = fgetc(fil);
    unsigned char prod_modell_len = fgetc(fil) + 1; // pluss `\0`
    char prod_modell[prod_modell_len];
    fread(prod_modell, sizeof(char), prod_modell_len, fil);
    // Lager ruter struktur.
    data->rutere[i] = ruter(ruter_id, flagg, prod_modell);
  }

  // Leser inn informasjonen om koblinger mellom rutere.  Ser på 2 og 2 int-er
  // som leses inn i `buf`, helt til vi når EOF og `fread` returnerer 0.
  int buf[2];
  while (fread(&buf, sizeof(int), 2, fil)) {
    struct ruter * ruter1 = ruterid(buf[0], data);
    struct ruter * ruter2 = ruterid(buf[1], data);
    legg_til_kobling(ruter1, ruter2);
  }

  return data;
}

// Frigjør allokert minne til datastrukruren.
int free_ruter_array(struct ruter_array * data) {
  free(data->rutere);
  free(data);
  return 0;
}

// Finner riktig ruterpeker i datastrukturen, gitt en ruter ID.
struct ruter * ruterid(int ruter_id, struct ruter_array * data) {
  for (int i = 0; i < data->antall; ++i) {
    if (data->rutere[i].ruter_id == ruter_id) {
      return &data->rutere[i];
    }
  }
  printf("Error: fant ingen ruter med id `%d`.\n", ruter_id);
  return NULL;
}

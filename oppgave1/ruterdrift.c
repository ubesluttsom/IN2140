// Inkluderer ruterstuktur- og funksjonsdeklarasjon.
#include "ruterdrift.h"
// Standardbibliotek.
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

  struct database * data = innlesing(fil);

  for (int i = 0; i < data->antall; ++i) {
    printr(data->ruter[i]);
  }

  slett_ruter(ruterid(789, data), data);
  slett_ruter(ruterid(865, data), data);
  slett_ruter(ruterid(865, data), data);

  for (int i = 0; i < data->antall; ++i) {
    printr(data->ruter[i]);
  }


  printf("Prøver å skrive til fil ...\n");
  FILE * utfil = fopen("new-topolopy.dat", "w");
  utskrift(data, utfil);

  free_database(data);
}


/* KOMMANDOER */

// Oppretter en ny ruterstruktur.
struct ruter * ruter(
    int           ruter_id,
    unsigned char flagg,
    unsigned char prod_modell_len,
    char *        prod_modell) {
  // Lager ny ruter struktur, noen variabler er null inntil videre.
  struct ruter * ny_ruter_peker = malloc(sizeof(struct ruter));
  struct ruter ny_ruter = { ruter_id, flagg, prod_modell_len, {NULL}, {NULL}, {NULL} };
  memcpy(ny_ruter_peker, &ny_ruter, sizeof(struct ruter));

  // Kopierer over prod./mod. streng, byte for byte. Egentlig bare et innpakket
  // `strncpy` kall, men jeg får vel gjenbruke kode hvor jeg kan.
  sett_modell(ny_ruter_peker, prod_modell);

  // Nuller ut til-/frakoblinger, vi kobler dem sammen senere.
  for (int i = 0; i < 9; ++i) {
    ny_ruter_peker->tilkoblinger[i] = NULL;
    ny_ruter_peker->frakoblinger[i] = NULL;
  }

  return ny_ruter_peker;
}

// Lager en kobling fra ruter `kilde` til ruter `dest`.
int legg_til_kobling(struct ruter * kilde, struct ruter * dest, struct database * data) {
  // Finner ledige «hull» i arrayene med koblinger.
  int i = 0, j = 0;
  for ( ; kilde->frakoblinger[i] != NULL; ++i) {
    // Sjekker om vi har overskridet kapasiteten på 10 plasser.
    if (i > 9) {
      printf("Error: koblinger i ruter %d overfyldt! "
             "Ignorerer kommando.\n", kilde->ruter_id);
      return 0;
    }
  }
  for ( ; dest->tilkoblinger[j] != NULL; ++j) {
    if (j > 9) {
      printf("Error: koblinger i ruter %d overfyldt! "
             "Ignorerer kommando.\n", dest->ruter_id);
      return 0;
    }
  }

  // Setter inn `til` på den første ledige plassen som vi fant.
  kilde->frakoblinger[i] = dest;
  dest->tilkoblinger[j]  = kilde;


  // LEGG TIL KOBLING I DATABASE

  for (int i = 0; i < data->nettverk->antall_max; ++i) {
    if (data->nettverk->kobling[i] == NULL) {
      data->nettverk->kobling[i] = malloc(sizeof(struct kobling));
      data->nettverk->kobling[i]->kilde = kilde;
      data->nettverk->kobling[i]->dest  = dest;
      ++data->nettverk->antall;
      printf("Legger til kobling %d -> %d i database. " 
             "Totalt antall koblinger er nå %d.\n",
             kilde->ruter_id, dest->ruter_id, data->nettverk->antall);
      return 1;
    }
  }

  printf("Error: koblinger oppfyldt. Ignorerer kommando.\n");
  return 0;
}

// Printer ruterstrukturer
void printr(struct ruter * ruter) {
  if (ruter == NULL) {
    // Ignorerer helt hvis `ruter` er NULL, for at print i `for`-løkker skal
    // fungere (og ikke spamme error beskjeder).
    // printf("Error: ruter som forsøkes å skrives ut et NULL.\n");
    return;
  }

  printf("Ruter-ID:\t%d\n",  ruter->ruter_id);

  // Printer flagg og gjør bit-oprerasjoner for å vise hva de betyr
  printf("Flagg:\t\t%#x\n", ruter->flagg);
  printf(" - Er bruk:\t%s\n", (ruter->flagg & 1) ? "JA" : "NEI");
  printf(" - Trådløs:\t%s\n", (ruter->flagg & 2) ? "JA" : "NEI");
  printf(" - 5 GHz:\t%s\n",   (ruter->flagg & 2) ? "JA" : "NEI");
  printf(" - Endr.nr.:\t%d\n", ruter->flagg >> 4);

  printf("Prod./modell:\t%s\n", (char*) ruter->prod_modell);

  printf("Tilkoblinger:\n");
  for (int i = 0; i < 9; ++i) {
    if (ruter->tilkoblinger[i] != NULL) {
      printf(" <- %d\n", ruter->tilkoblinger[i]->ruter_id);
    }
  }

  printf("Frakoblinger:\n");
  for (int i = 0; i < 9; ++i) {
    if (ruter->frakoblinger[i] != NULL) {
      printf(" -> %d\n", ruter->frakoblinger[i]->ruter_id);
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
  ruter->prod_modell_len = (unsigned char) strlen(ny_prod_modell);
  return 0;
}

// Sletter en `ruter`. 
int slett_ruter(struct ruter * ruter, struct database * data) {
  if (ruter == NULL) {
    printf("Error: ruter som skulle slettes er NULL.\n");
    return 0;
  }

  // -----
  
  for (int i = 0; i < data->nettverk->antall_max; ++i) {
    if (data->nettverk->kobling[i] == NULL)
      continue;
    else if (data->nettverk->kobling[i]->kilde == ruter
        || data->nettverk->kobling[i]->dest == ruter) {
      free(data->nettverk->kobling[i]);
      data->nettverk->kobling[i] = NULL;
      --data->nettverk->antall;
    }
  }

  // -----

  // Nuller ut til-/frakoblinger.
  for (int i = 0; i < 9; ++i) {
    if (ruter->tilkoblinger[i] != NULL) {
      for (int j = 0; j < 9; ++j) {
        if (ruter->tilkoblinger[i]->frakoblinger[j] == ruter) {
          ruter->tilkoblinger[i]->frakoblinger[j] = NULL;
        }
      }
    }

    if (ruter->frakoblinger[i] != NULL) {
      for (int j = 0; j < 9; ++j) {
        if (ruter->frakoblinger[i]->tilkoblinger[j] == ruter) {
          ruter->frakoblinger[i]->tilkoblinger[j] = NULL;
        }
      }
    }
  }

  for (int i = 0; i < data->antall; ++i) {
    // Hvis `ruter` peker til samme sted som `data->ruter[i]` ...
    if (ruter == data->ruter[i]) {
      // ... null ut denne ruteren.
      printf("AVLUS: ...\n");
      free(data->ruter[i]);
      data->ruter[i] = NULL;
      // Trenger ikke frigjøre minnet, siden det ble allokert i en stor chunk
      // med plass til ALLE ruterene som ble lest inn.
      return 1;
    }
  }
  printf("Error: fant ikke ruter som skulle slettes.\n");
  return 0;
}


/* HJELPEFUNKSJONER */

// Frigjør allokert minne til datastrukruren.
int free_database(struct database * data) {
  for (int i = 0; i < data->antall; ++i) {
    free(data->ruter[i]);
  }
  for (int i = 0; i < data->nettverk->antall_max; ++i) {
    free(data->nettverk->kobling[i]);
  }
  free(data->nettverk);
  free(data);
  return 0;
}

// Finner riktig ruterpeker i datastrukturen, gitt en ruter ID.
struct ruter * ruterid(int ruter_id, struct database * data) {
  for (int i = 0; i < data->antall; ++i) {
    if (data->ruter[i] == NULL)
      continue;
    if (data->ruter[i]->ruter_id == ruter_id)
      return data->ruter[i];
  }
  printf("Error: fant ingen ruter med id `%d`.\n", ruter_id);
  return NULL;
}


/* FILINNLESING og -UTSKRIFT */

struct database * innlesing(FILE* fil) {
  // RUTERE

  // Allokerer plass til antall rutere spesifisert i filen. Innleveringen
  // definerer ingen komando for opprettelse av nye rutere, så vi kan være
  // sikre på at vi allokerer nok plass. `data->ruter` er en «flexible array
  // member», som vi tilpasser til «N», `antall_rutere`.
  int antall_rutere;
  fread(&antall_rutere, sizeof(int), 1, fil);
  printf("Antall rutere i fil: %d\n", antall_rutere);
  struct database * data = malloc(
      sizeof(int) + 
      sizeof(struct nettverk *) + 
      sizeof(struct ruter *)*antall_rutere);
  data->antall = antall_rutere;


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
    data->ruter[i] = ruter(ruter_id, flagg, prod_modell_len, prod_modell);
  }

  // NETTVERK
  
  // Oppgaven spesifiserer at hver ruter ikke har fler enn 10 koblinger, vi
  // allokerer deretter.
  struct nettverk * nett = malloc(
      sizeof(int) + sizeof(int) + sizeof(struct kobling *)*data->antall*10);
  nett->antall_max = data->antall*10;
  nett->antall = 0;
  for (int i = 0; i < nett->antall_max; ++i) {
    nett->kobling[i] = NULL;
  }

  data->nettverk = nett;

  // Leser inn informasjonen om koblinger mellom rutere.  Ser på 2 og 2 int-er
  // som leses inn i `buf`, helt til vi når EOF og `fread` returnerer 0.
  int buf[2];
  while (fread(&buf, sizeof(int), 2, fil)) {
    struct ruter * kilde = ruterid(buf[0], data);
    struct ruter * dest  = ruterid(buf[1], data);
    legg_til_kobling(kilde, dest, data);
  }

  return data;
}

int utskrift(struct database * data, FILE * fil) {
  int faktisk_antall = 0;
  for (int i = 0; i < data->antall; ++i)
    if (data->ruter[i] != NULL)
      ++faktisk_antall;
  fwrite(&faktisk_antall, sizeof(int), 1, fil);

  for (int i = 0; i < data->antall; ++i) {
    if (data->ruter[i] == NULL)
      continue;
    
    putw(data->ruter[i]->ruter_id, fil);
    fputc(data->ruter[i]->flagg, fil);
    fputc(data->ruter[i]->prod_modell_len, fil);
    // printf("AVLUS: Skriver ruter %d til fil ...\n",
    //        data->ruter[i]->ruter_id);
    fputs((char *) &data->ruter[i]->prod_modell, fil);
    fputc('\0', fil);
  }

  for (int i = 0; i < data->nettverk->antall_max; ++i) {
    if (data->nettverk->kobling[i] == NULL)
      continue;
    putw(data->nettverk->kobling[i]->kilde->ruter_id, fil);
    putw(data->nettverk->kobling[i]->dest->ruter_id, fil);
    // printf("AVLUS: Skriver kobling %d -> %d til fil ...\n",
    //        data->nettverk->kobling[i]->kilde->ruter_id,
    //        data->nettverk->kobling[i]->dest->ruter_id);
  }

  fflush(fil);

  return 1;
}

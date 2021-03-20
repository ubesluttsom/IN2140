// Inkluderer ruterstuktur- og funksjonsdeklarasjon.
#include "ruterdrift.h"
// Standardbibliotek.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* MAIN */

int main(int argc, char* argv[])
{
  if (argc != 3) {
    if (argc < 2) printf("Error: datafil må spesifiseres.\n");
    if (argc < 3) printf("Error: kommandofil må spesifiseres.\n");
    if (argc > 3) printf("Error: Tar kun maks 2 argumenter.\n");
    printf("... Avslutter ...\n");
    return EXIT_FAILURE;
  }

  printf("... Åpner datafil: `%s` ... \n", argv[1]);

  char* filnavn = argv[1];
  FILE* datafil = fopen(filnavn, "r");

  if (!datafil) {
    printf("Error: Filåpning feilet!\n");
    return EXIT_FAILURE;
  }

  struct database * data = innlesing(datafil);

  // for (int i = 0; i < data->rom; ++i) {
  //   print(data->ruter[i], data);
  // }

  // KOMMANDOFIL

  printf("... Åpner kommandofil: `%s` ... \n", argv[2]);
 
  char* filnavn2 = argv[2];
  FILE* kommandofil = fopen(filnavn2, "r");

  if (!kommandofil) {
    printf("Error: Filåpning feilet!\n");
    return EXIT_FAILURE;
  }

  kommandoer(data, kommandofil);

  // SKRIV TIL FIL

  printf("... Skriver til fil ...\n");
  FILE * utfil = fopen("new-topolopy.dat", "w");
  utskrift(data, utfil);

  // AVSLUTT

  printf("... Frigjør minne ...\n");
  free_database(data);

  printf("... Avslutter ...\n");
  return EXIT_SUCCESS;
}


/* KOMMANDOER */

// Lager en kobling fra ruter `kilde` til ruter `dest`.
int legg_til_kobling(struct ruter * kilde, struct ruter * dest,
                     struct database * data)
{
  if (kilde == NULL || dest == NULL) {
    if (kilde == NULL) printf("Error: kan ikke opprette kobling; "
                              "koblingskilden finnes ikke.\n");
    if (dest  == NULL) printf("Error: kan ikke opprette kobling; "
                              "koblingsdestinasjon finnes ikke.\n");
    return 0;
  }

  // Ser etter et «NULL-hull» i `kobling[]` hvor vi kan sette inn.
  for (int i = 0; i < data->nettverk->rom; ++i) {
    if (data->nettverk->kobling[i] != NULL) 
      continue;
    data->nettverk->kobling[i] = malloc(sizeof(struct kobling));
    data->nettverk->kobling[i]->kilde = kilde;
    data->nettverk->kobling[i]->dest  = dest;
    ++data->nettverk->antall;
    printf("... Lager kobling %d -> %d. Antall koblinger %d ...\n",
           kilde->id, dest->id, data->nettverk->antall);
    return 1;
  }

  printf("Error: koblinger oppfyldt. Ignorerer kommando.\n");
  return 0;
}

// Printer ruterstrukturer
void print(struct ruter * ruter, struct database * data)
{
  if (ruter == NULL)
    // Ignorerer helt hvis `ruter` er NULL, for at print i `for`-løkker skal
    // fungere (og ikke spamme error beskjeder).
    // printf("Error: ruter som forsøkes å skrives ut et NULL.\n");
    return;

  printf("Ruter-ID:\t%d\n",  ruter->id);

  // Printer flagg og gjør bit-oprerasjoner for å vise hva de betyr
  printf("Flagg:\t\t%#x\n", ruter->flagg);
  printf(" - Er bruk:\t%s\n", (ruter->flagg & 1) ? "JA" : "NEI");
  printf(" - Trådløs:\t%s\n", (ruter->flagg & 2) ? "JA" : "NEI");
  printf(" - 5 GHz:\t%s\n",   (ruter->flagg & 4) ? "JA" : "NEI");
  printf(" - Endr.nr.:\t%d\n", ruter->flagg >> 4);

  printf("Prod./modell:\t%s\n", (char*) ruter->prod_modell);

  printf("Koblinger:\n");
  for (int i = 0; i < data->nettverk->rom; ++i) {
    if (data->nettverk->kobling[i] == NULL)
      continue;
    if (data->nettverk->kobling[i]->dest == ruter)
      printf(" <- %d\n", data->nettverk->kobling[i]->kilde->id);
    if (data->nettverk->kobling[i]->kilde == ruter)
      printf(" -> %d\n", data->nettverk->kobling[i]->dest->id);
  }
}

// Setter endrer på flagget i ruter. Gjør dette ved å sette et gitt
// `flagg_bit` til en `ny_verdi`. Sjekker også om argumentene er gyldige.
int sett_flagg(struct ruter * ruter, int flagg_bit, int ny_verdi)
{
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

  printf("... I ruter %d, setter flaggbit %d til verdien %d ...\n",
      ruter->id, flagg_bit, ny_verdi);
  // Maskerer vekk `flagg_bit` fra ruterens flagg, og ORer det med `ny_verdi`.
  ruter->flagg = (ruter->flagg & ~(1 << flagg_bit)) | (ny_verdi << flagg_bit);
  return 1;
}

// Setter et nytt navn til en ruter. Vil først sjekke at nytt navn faktisk får
// plass i ruterstrukturen vår. `strncpy` vil fylle opp `ruter->prod_modell`
// med akkurat 249 bytes. For lange strenger kuttes av. Resterende plass i
// char-arrayet med `\0`.
int sett_modell(struct ruter * ruter, char * ny_prod_modell)
{
  strncpy((char *) ruter->prod_modell, ny_prod_modell, 249);
  ruter->prod_modell_len = (unsigned char) strlen(ny_prod_modell);
  return 0;
}

// Sletter en `ruter`. 
int slett_ruter(struct ruter * ruter, struct database * data)
{
  if (ruter == NULL) {
    printf("Error: ruter som skulle slettes finnes ikke.\n");
    return 0;
  }

  printf("... Sletter ruter %d ...\n", ruter->id);
  
  for (int i = 0; i < data->nettverk->rom; ++i) {
    if (data->nettverk->kobling[i] == NULL)
      continue;
    else if (data->nettverk->kobling[i]->kilde == ruter
        || data->nettverk->kobling[i]->dest == ruter) {
      free(data->nettverk->kobling[i]);
      data->nettverk->kobling[i] = NULL;
      --data->nettverk->antall;
    }
  }

  for (int i = 0; i < data->rom; ++i) {
    // Hvis `ruter` peker til samme sted som `data->ruter[i]` ...
    if (ruter == data->ruter[i]) {
      // ... frigjør minne, null ut pekeren og dekrementer antall.
      free(data->ruter[i]);
      data->ruter[i] = NULL;
      --data->antall;
      return 1;
    }
  }
  printf("Error: fant ikke ruter som skulle slettes.\n");
  return 0;
}


/* HJELPEFUNKSJONER */

// Oppretter en ny ruterstruktur. Allokerer minne; returpekeren må frigjøres.
struct ruter * ruter(
    int           id,
    unsigned char flagg,
    unsigned char prod_modell_len,
    char *        prod_modell)
{
  printf("... Oppretter ruter %d ...\n", id);
  // Lager ny ruter struktur, prod./mod. strengen er null inntil videre.
  struct ruter * ny_ruter_peker = malloc(sizeof(struct ruter));
  struct ruter ny_ruter = { id, flagg, prod_modell_len, {NULL} };
  memcpy(ny_ruter_peker, &ny_ruter, sizeof(struct ruter));

  // Kopierer over prod./mod. streng, byte for byte. Egentlig bare et innpakket
  // `strncpy` kall, men jeg får vel gjenbruke kode hvor jeg kan.
  sett_modell(ny_ruter_peker, prod_modell);

  return ny_ruter_peker;
}

// Frigjør allokert minne til datastrukruren.
int free_database(struct database * data)
{
  for (int i = 0; i < data->rom; ++i) {
    free(data->ruter[i]);
  }
  for (int i = 0; i < data->nettverk->rom; ++i) {
    free(data->nettverk->kobling[i]);
  }
  free(data->nettverk);
  free(data);
  return 0;
}

// Finner riktig ruterpeker i datastrukturen, gitt en ruter ID.
struct ruter * ruterid(int id, struct database * data)
{
  for (int i = 0; i < data->rom; ++i) {
    if (data->ruter[i] == NULL)
      continue;
    if (data->ruter[i]->id == id)
      return data->ruter[i];
  }
  printf("Error: fant ingen ruter med id `%d`.\n", id);
  return NULL;
}


/* FILINNLESING og -UTSKRIFT */

struct database * innlesing(FILE* fil)
{
  // RUTERE

  // Allokerer plass til antall rutere spesifisert i filen. Innleveringen
  // definerer ingen komando for opprettelse av nye rutere, så vi kan være
  // sikre på at vi allokerer nok plass. `data->ruter` er en «flexible array
  // member», som vi tilpasser til «N», `antall_rutere`.
  int antall_rutere;
  fread(&antall_rutere, sizeof(int), 1, fil);
  struct database * data = malloc(
      sizeof(int) + 
      sizeof(int) + 
      sizeof(struct nettverk *) + 
      sizeof(struct ruter *)*antall_rutere);
  data->rom = antall_rutere;

  // Leser inn «N» blokker med ruterinformasjon og oppretter en ruter for hver
  // av dem.
  for (int i = 0; i < data->rom; ++i) {
    // Leser inn info.
    int id;
    fread(&id, sizeof(int), 1, fil);
    unsigned char flagg = fgetc(fil);
    unsigned char prod_modell_len = fgetc(fil) + 1; // pluss `\0`
    char prod_modell[prod_modell_len];
    fread(prod_modell, sizeof(char), prod_modell_len, fil);
    // Lager og allokerer plass til ruterstruktur.
    data->ruter[i] = ruter(id, flagg, prod_modell_len, prod_modell);
    ++data->antall;
  }

  // NETTVERK
  
  // Oppgaven spesifiserer at hver ruter ikke har fler enn 10 koblinger, vi
  // allokerer deretter.
  struct nettverk * nett = malloc(
      sizeof(int) +
      sizeof(int) +
      sizeof(struct kobling *)*data->rom*10);
  nett->rom = data->rom*10;
  nett->antall = 0;
  for (int i = 0; i < nett->rom; ++i) {
    // Alle koblinger er i utgangspunktet ikke-eksisterende enda.
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

// Skriver datastrukturer til fil.
int utskrift(struct database * data, FILE * fil)
{
  // int faktisk_antall = 0;
  // for (int i = 0; i < data->rom; ++i)
  //   if (data->ruter[i] != NULL)
  //     ++faktisk_antall;
  fwrite(&data->antall, sizeof(int), 1, fil);

  for (int i = 0; i < data->rom; ++i) {
    if (data->ruter[i] == NULL)
      continue;
    
    putw(data->ruter[i]->id, fil);
    fputc(data->ruter[i]->flagg, fil);
    fputc(data->ruter[i]->prod_modell_len, fil);
    // printf("AVLUS: ... Skriver ruter %d til fil ...\n",
    //        data->ruter[i]->id);
    fputs((char *) &data->ruter[i]->prod_modell, fil);
    fputc('\0', fil);
  }

  for (int i = 0; i < data->nettverk->rom; ++i) {
    if (data->nettverk->kobling[i] == NULL)
      continue;
    putw(data->nettverk->kobling[i]->kilde->id, fil);
    putw(data->nettverk->kobling[i]->dest->id, fil);
    // printf("AVLUS: ... Skriver kobling %d -> %d til fil ...\n",
    //        data->nettverk->kobling[i]->kilde->id,
    //        data->nettverk->kobling[i]->dest->id);
  }

  fflush(fil);

  return 1;
}

// Leser inn og utfører kommandoer fra kommandofil `fil`.
int kommandoer(struct database * data, FILE * fil)
{
  char linje[300];
  char * kommando;
  struct ruter * ruter;

  while (fgets(linje, 300, fil) != NULL) {
    printf("KOMMANDO: %s", linje);

    kommando = strtok(linje, " \n");
    ruter    = ruterid(atoi(strtok(NULL, " \n")), data);

    // Tolker kommando. Her bruker jeg `strtok` for å hente ut argumenter. Jeg
    // konverterer de `atoi` når jeg jeg trenger tall. Sekvensielle kall på
    // `strtok` med `NULL` som argument fortsetter nedover på samme streng.
    // Merk at jeg bruker litt forskjellige avgrensninger: « \n» vs. «\n».
    if (strcmp(kommando, "print") == 0) {
      print(ruter, data);
    } else if (strcmp(kommando, "slett_ruter") == 0) {
      slett_ruter(ruter, data);
    } else if (strcmp(kommando, "sett_modell") == 0) {
      sett_modell(
          ruter,
          strtok(NULL, "\n"));
    } else if (strcmp(kommando, "legg_til_kobling") == 0) {
      legg_til_kobling(
          ruter,
          ruterid(atoi(strtok(NULL, " \n")), data),
          data);
    } else if (strcmp(kommando, "finnes_rute") == 0) {
      printf("Error: `finnes_rute` kommando ikke implementert enda.\n");
    } else if (strcmp(kommando, "sett_flagg") == 0) {
      sett_flagg(
          ruter,
          atoi(strtok(NULL, " \n")),
          atoi(strtok(NULL, " \n")));
    } else {
      printf("Error: ukjent kommando `%s`.\n", kommando);
    }
  }

  return 1;
}

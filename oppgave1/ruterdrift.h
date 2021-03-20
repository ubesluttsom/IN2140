#include <stdio.h>

/* STRUKTURER */

// Ruterstruktur, på akkurat samme format som i datafilene.
struct ruter {
  int id;                          // Ruterens unike ID.
  unsigned char flagg;             // Flagg med bitvis informasjon.
  unsigned char prod_modell_len;   // Lengde på beskrivelse.
  char * prod_modell[250];         // Beskrivelse.
};

// Kobling. En abstraksjon for en enkelt kobling mellom to rutere.
struct kobling {
  struct ruter * kilde;   // Ruter som koblingen fører FRA.
  struct ruter * dest;    // Ruter som koblingen fører TIL.
};

// Nettverkstruktur. Holder informasjon om alle koblinger mellom rutere.
struct nettverk {
  int rom;                      // Koblinger det er plass til.
  int antall;                   // Koblinger til en hver tid.
  struct kobling * kobling[];   // Pekere til koblinger.
  // ^ flexible array member
};

// Database. En paraplystruktur som holder styr på all data under kjøring. På
// denne måten er det enkelt sende en peker til denne databasen til en
// funksjon, og funksjonen har så tilgang til all informasjon den skulle
// trenge.
struct database {
  int rom;                      // Antall rutere det er rom til.
  int antall;                   // Antall rutere til en hver tid.
  struct nettverk * nettverk;   // Informasjon om koblinger.
  struct ruter * ruter[];       // Pekere til alle rutere.
  // ^ flexible array member
};


/* FUNKSJONSDEKLARASJONER */

// Disse funksjonene er forklart i mer detalj i `ruterdrift.c`.

struct ruter * ruter(int id, unsigned char flagg,
                     unsigned char prod_modell_len, char * prod_modell);
void print(struct ruter * ruter, struct database * data);
int legg_til_kobling(struct ruter * kilde, struct ruter * dest,
                     struct database * data);
int sett_flagg(struct ruter * ruter, int flagg_bit, int ny_verdi);
int sett_modell(struct ruter * ruter, char * ny_prod_modell);
int slett_ruter(struct ruter * ruter, struct database * data);
struct database * innlesing(FILE* fil);
int utskrift(struct database * data, FILE * fil);
int kommandoer(struct database * data, FILE * fil);
int free_database(struct database * data);
struct ruter * ruterid(int id, struct database * data);

// TODO: Hjelpefunksjon som sjekker om det finnes en direkte kobling mellom to
// rutere. Noe alla:
//   `int finnes_kobling(struct ruter * kilde, struct ruter * dest)`
// Trenger uansett å lage en generell slik funksjon, som finner en «rute»
// mellom to rutere generelt, uanhengig av ledd.
//
// TODO: `utskrift(...)` kan skrives om til å skrive direkte til fil med
//   `fwrite(..., sizeof(data->ruter[i]), ...)`
// hvis til-/frakoblinger arrayene fjernes fra `struct ruter`.

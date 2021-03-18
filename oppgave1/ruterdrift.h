#include <stdio.h>

/* STRUKTURER */

struct ruter {
  int ruter_id;
  unsigned char flagg;
  char * prod_modell[250];
  // Koblinger mellom rutere. Siden «kantene» er ensrettet er konvensjonen min:
  // `tilkoblinger` peker TIL ruteren, `frakoblinger` peker FRA.
  struct ruter * tilkoblinger[10];
  struct ruter * frakoblinger[10];
  // Hvike koblinger som er i bruk; på bit-format: 0000 0000 0000 0010 betyr at
  // `tilkoblinger[1]` er aktiv, men ingen andre. Bare de første 10 bitene
  // (little endian) er i bruk. Må initialiseres til null.
  unsigned short aktive_tilkoblinger;
  unsigned short aktive_frakoblinger;
};

struct ruter_array {
  int antall;
  struct ruter * rutere;
};


/* FUNKSJONSDEKLARASJONER */

struct ruter ruter(
    int ruter_id,
    unsigned char flagg,
    char * prod_modell);
void printr(struct ruter * ruter);
int legg_til_kobling(struct ruter * kilde, struct ruter * dest);
int sett_flagg(struct ruter * ruter, int flagg_bit, int ny_verdi);
int sett_modell(struct ruter * ruter, char * ny_prod_modell);
int slett_ruter(struct ruter * ruter, struct ruter_array * data);
struct ruter_array * innlesing(FILE* fil);
int free_ruter_array(struct ruter_array * data);
struct ruter * ruterid(int ruter_id, struct ruter_array * data);

// TODO: Hjelpefunksjon som sjekker om det finnes en direkte kobling mellom to
// rutere. Noe alla:
//   `int finnes_kobling(struct ruter * kilde, struct ruter * dest)`
// Trenger uansett å lage en generell slik funksjon, som finner en «sti» mellom
// to rutere generelt, uanhengig av ledd.

#include <stdio.h>

/* STRUKTURER */

struct ruter {
  int ruter_id;
  unsigned char flagg;
  unsigned char prod_modell_len;
  char * prod_modell[250];
  // Koblinger mellom rutere. Siden «kantene» er ensrettet er konvensjonen min:
  // `tilkoblinger` peker TIL ruteren, `frakoblinger` peker FRA.
  struct ruter * tilkoblinger[10];
  struct ruter * frakoblinger[10];
};

struct kobling {
  struct ruter * kilde;
  struct ruter * dest;
};

struct nettverk {
  int antall;
  int antall_max;
  struct kobling * kobling[];
  // ^ flexible array member
};

struct database {
  int antall;
  struct nettverk * nettverk;
  struct ruter * ruter; 
  // ^ flexible array member
};


/* FUNKSJONSDEKLARASJONER */

struct ruter ruter(
    int ruter_id,
    unsigned char flagg,
    unsigned char prod_modell_len,
    char * prod_modell);
void printr(struct ruter * ruter);
int legg_til_kobling(
    struct ruter * kilde,
    struct ruter * dest,
    struct database * data);
int sett_flagg(struct ruter * ruter, int flagg_bit, int ny_verdi);
int sett_modell(struct ruter * ruter, char * ny_prod_modell);
int slett_ruter(struct ruter * ruter, struct database * data);
struct database * innlesing(FILE* fil);
int utskrift(struct database * data, FILE * fil);
int free_database(struct database * data);
struct ruter * ruterid(int ruter_id, struct database * data);

// TODO: Hjelpefunksjon som sjekker om det finnes en direkte kobling mellom to
// rutere. Noe alla:
//   `int finnes_kobling(struct ruter * kilde, struct ruter * dest)`
// Trenger uansett å lage en generell slik funksjon, som finner en «rute»
// mellom to rutere generelt, uanhengig av ledd.
//
// TODO: Skrive om `strukt database` til å ha fleksibel ruter array av typen
// `struct ruter * ruter[]`. Deretter allokere (og frigjøre minne) til hver
// ruter, i stedet for i et stort jaffs.
// - [ ] `struct database`
// - [ ] `ruter(...)`
// - [ ] `innlesing(...)`
// - [ ] `free_database(...)`
// - [ ] `printr(...)`
// - [ ] %s/data->ruter[i]\./data->ruter[i]->/gc

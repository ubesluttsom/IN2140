/* STRUKTURER */

struct ruter {
  unsigned int ruter_id;
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


/* FUNKSJONSDEKLARASJONER */

struct ruter ruter(
    unsigned char ruter_id,
    unsigned char flagg,
    int prod_modell_lengde,
    char * prod_modell,
    struct ruter * tilkoblinger,
    struct ruter * frakoblinger,
    unsigned short aktive_tilkoblinger,
    unsigned short aktive_frakoblinger);
void printr(struct ruter ruter);
int legg_til_kobling(struct ruter * kilde, struct ruter * dest);
int sett_flagg(struct ruter * ruter, int flagg_bit, int ny_verdi);
int sett_modell(struct ruter * ruter, char * ny_prod_modell);

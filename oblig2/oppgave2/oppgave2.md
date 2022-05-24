# Oppgave 2 --- Teori

## Spørsmål 1

Vi starter i spor 18, med hodet på vei utover. Ettersom C-SCAN alltid beveger
seg fra ytterpunkt til ytterpunkt, vil hodet nå spor 30 og fortsette __utover__
til 31.

## Spørsmål 2

C-LOOK er lik C-SCAN, utenom at den vil hele tiden vurdere om det er noen
ventende forespørsler i retningen den beveger seg; dersom det ikke er det, vil
den snu før den har nådd ytterste spor i disken. Ettersom det ikke er noen
forespørsler lengre ut en spor 30, vil hodet i dette tilfellet bevege seg
__innover__ umiddelbart etter.

## Spørsmål 3

FIFO
  : Rekkefølgen blir akkurat den samme som listingen i oppgaven, yngst til
  eldlst. Vi finner total distense ved summen av forskjellene mellom hver
  forespørsel. Altså, $$\lvert 18-24 \rvert + \lvert 24-7 \rvert + \lvert 7-14
  \rvert + \lvert 14-20 \rvert + \lvert 20-10 \rvert + \lvert 10-30 \rvert +
  \lvert 30-4 \rvert + \lvert 4-23 \rvert = 111.$$

SSTF
  : Vi har rekkefølgen: (18), 20, 24, 23, 30, 14, 10, 7, 4. Siden 23 kommer inn
  litt sent, må vi snu for å tjene den forespørslen, for så å snu igjen mot 30,
  blir distansene denne gangen: $$\lvert 18-24 \rvert + \lvert 24-23 \rvert +
  \lvert 23-30 \rvert + \lvert 30-4 \rvert = 40.$$

LOOK
  : Her blir rekkefølgen: (18), 20, 24, 30, 23, 14, 10, 7, 4. Vi beveger oss
  altså fra 18 til 30, så fra 30 til 4. Som blir $$\lvert 18-30 \rvert + \lvert
  30-4 \rvert = 38.$$

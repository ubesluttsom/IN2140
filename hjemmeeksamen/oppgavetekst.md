# Multipleksing og tapsgjenoppretting

## Introduksjon

En viktig oppgave for transportlaget er multipleksing. Mens nettverkslaget er ansvarlig for � levere pakker til et endesystem, er transportlaget ansvarlig for � levere disse pakkene til riktig prosess p� endesystemet. Dette er imidlertid en forenkling. I mange tilfeller m� en enkel prosess tilby mange forbindelser, og transportlaget m� tilby multipleksing mellom alle disse forbindelsene i samme prosess. Et veldig typisk eksempel p� dette er en webserver, som m� utlevere filer til flere nettlesere samtidig.

Hvis transportlaget tilbyr p�litelige tjenester til laget over, kan det ogs� v�re n�dvendig � implementere p�litelighet hvis det underliggende nettverket ikke gj�r det. I dagens Internett tilbyr TCP multipleksing og p�litelighet p� toppen av nettverkslaget IP.

I denne oppgaven skal du bruke UDP-protokollen for � etterligne en pakkeorientert, forbindelsesl�s, up�litelig nettverkslagprotokoll, og du lager ditt eget transportlag som implementerer multipleksing for flere samtidige forbindelser til samme prosess p� en enkelt server.

Du skal implementere to applikasjoner: en klient og en server. Disse applikasjonene implementerer b�de funksjonalitet for transportlag og funksjonalitet for applikasjonslag.


## Oppgaven

Denne oppgaven er inspirert av en protokoll som sjelden brukes i dag: filtjenesteprotokollen FSP. Den overf�rer store filer (for eksempel en operativsystemkjerne) over UDP og brukes noen ganger for � levere store filer til mange bladdatamaskiner i en beregningsklynge n�r klyngen sl�s p�. Disse bladdatamaskinene har ingen permanent lagring; FSP brukes for � hente operativsystemene deres under oppstart. FSP ble utviklet fordi det er en veldig lettvektig protokoll p� toppen av UDP, men dens rolle er sv�rt begrenset fordi den ikke kan gi p�litelig dataoverf�ring. Hvis pakker g�r tapt, m� FSP starte overf�ringen p� nytt.

I denne oppgaven skal du opprette et nytt transportlag, RDP, Reliable Datagram Protocol, som tilbyr multipleksing og p�litelighet, en NewFSP-klient som henter filer fra en server, og en NewFSP-server som bruker RDP for � levere store filer til flere NewFSP-klienter p�litelig.


### RDP

Dessverre kan du ikke implementere RDP direkte over IP fordi det krever rotprivilegier. I stedet skal du bruke UDP for � etterligne en forbindelsesl�s, up�litelig nettverkslagprotokoll under transportprotokollen RDP. P� grunn av begrensningene ved IFIs installasjon, m� du st�tte IPv4-adresser. Du kan ogs� st�tte IPv6-adresser.

Transportprotokollen RDP, som ligger p� toppen av denne �nettverksprotokollen�, skal tilby multipleksing og p�litelighet. RDP-implementeringen din vil bruke en enkelt UDP-port for � motta pakker fra alle klienter, og den vil ogs� svare p� dem over denne porten.

__P�litelighet__: RDP-protokollen din er en forbindelsesorientert protokoll, fordi det er lettere � gi p�litelighet med forbindelsesorienterte protokoller enn forbindelsesl�se. Applikasjonen din skal kalle funksjonen `rdp_accept()` for � sjekke om en ny forbindelsesforesp�rsel er kommet, og godta den hvis det er tilfelle. `rdp_accept()` returnerer en NULL-peker hvis det ikke er tilfelle, og en peker til en __dynamisk allokert datastruktur__ som representerer en RDP-forbindelse hvis en forbindelse aksepteres. RDP er ikke s� komplisert som TCP; den bruker en enkel stopp-og-vent-protokoll for � oppn� p�litelig dataoverf�ring. Hver pakke som sendes av en RDP-avsender, m� kvitteres av mottakeren f�r den kan sende neste pakke (stopp-og-vent). Hvis ingen bekreftelse mottas innen 100 millisekunder, sender RDP pakken p� nytt. RDP-avsenderen godtar ikke en annen pakke fra applikasjonen f�r den forrige pakken har blitt levert (det betyr at den returnerer 0 n�r applikasjonen kaller `rdp_write()`).

__Multipleksing__: P�krevet multipleksing skal implementeres av deg i RDP-protokollen din. Du skal bruke forbindelses-ID-er som identifiserer riktig forbindelse p� serveren og p� klienten. N�r en klient oppretter en forbindelse, velger den sin egen forbindelses-ID tilfeldig og inkluderer en forbindelses-ID 0 for serveren n�r den ber om en forbindelse. Hvis en annen klient med samme forbindelses-ID allerede er koblet til serveren, m� den avvise den nye forbindelsesforesp�rselen ved � sende en pakke med `flags==0x20`.

### NewFSP

Du skal implementere to programmer: en NewFSP-server og en NewFSP-klient.

NewFSP-klienten skal bruke RDP for � koble seg til en NewFSP-server og hente en fil. Klienten kjenner ikke navnet p� filen; den vil ganske enkelt opprette en RDP-forbindelse til serveren og motta pakker. NewFSP-klienten lagrer disse pakkene i en lokal fil kalt `kernel-file<tilfeldig tall>`. N�r NewFSP-serveren indikerer at filoverf�ringen er fullf�rt ved � sende en tom pakke, lukker NewFSP-klienten forbindelsen.

NewFSP-serveren skal bruke RDP for � motta forbindelsesforesp�rsler fra NewFSP-klienter og betjene dem. NewFSP-serveren er en prosess med kun �n tr�d, som kan betjene flere NewFSP-klienter samtidig. Den har en hovedl�kke, der den

1. sjekker om en ny RDP-forbindelsesforesp�rsel er mottatt,
2. pr�ver � levere neste pakke (eller den siste tomme pakken) til hver tilkoblede RDP-klient,
3. sjekker om en RDP-forbindelse er stengt.
4. Hvis ingenting har skjedd i noen av disse tre tilfellene (ingen ny forbindelse, ingen forbindelse ble stengt, ingen pakke kunne sendes til noen forbindelse), m� du vente en periode. Det er to implementasjonsalternativer:
  a. � vente i ett sekund og s� fors�ke igjen
  b. � bruke `select()` for � vente til en hendelse inntreffer

N�r en klient har koblet seg til, vil NewFSP-serveren overf�re en stor fil til klienten. N�r den har overf�rt hele filen, sender den en tom pakke for � indikere at overf�ringen er fullf�rt.

Den sender den samme store filen til alle disse klientene. Du kan velge om NewFSP-serveren leser filen fra disken flere ganger eller holder den i minnet. Den bruker RDPs multipleksing for � oppn� dette, men det betyr at den m� kunne godta nye tilkoblinger fra en NewFSP-klient innimellom mens den sender pakker til andre, allerede tilkoblede NewFSP-klienter.


### Pakkene

RDP-protokollformatet for transportlaget ditt m� v�re f�lgende:

+-----------------+--------------------+-----------------------------------------------------------------------------+
| Datatype        | Navn (forslagsvis) | Beskrivelse                                                                 |
+-----------------+--------------------+-----------------------------------------------------------------------------+
| `unsigned char` | `flag`             | Flagg som definerer forskjellige typer pakker.                              |
|                 |                    | - Hvis `flag==0x01`, s� er denne pakken en forbindelsesforesp�rsel.         |
|                 |                    | - Hvis `flag==0x02`, s� er denne pakken en forbindelsesavslutning.          |
|                 |                    | - Hvis `flag==0x04`, s� inneholder denne pakken data.                       |
|                 |                    | - Hvis `flag==0x08`, s� er denne pakken en ACK.                             |
|                 |                    | - Hvis `flag==0x10`, s� aksepterer denne pakken en forbindelsesforesp�rsel. |
|                 |                    | - Hvis `flag==0x20`, s� avsl�r denne pakken en forbindelsesforesp�rsel.     |
+-----------------+--------------------+-----------------------------------------------------------------------------+
| `usignert char` | `pktseq`           | Sekvensnummer pakke                                                         |
+-----------------+--------------------+-----------------------------------------------------------------------------+
| `usignert char` | `ackseq`           | Sekvensnummer ACK-ed av pakke                                               |
+-----------------+--------------------+-----------------------------------------------------------------------------+
| `usignert char` | `unassigned`       | Ubrukt, det vil si alltid 0                                                 |
+-----------------+--------------------+-----------------------------------------------------------------------------+
| `int`           | `senderid`         | Avsenderens forbindelses-ID i nettverksbyterekkef�lge                       |
+-----------------+--------------------+-----------------------------------------------------------------------------+
| `int`           | `recvid`           | Mottakerens forbindelses-ID i nettverksbyterekkef�lge                       |
+-----------------+--------------------+-----------------------------------------------------------------------------+
| `int`           | `metadata`         | Heltallsverdi i nettverksbyterekkef�lge der tolkningen avhenger av          |
|                 |                    | verdien av flagg                                                            |
|                 |                    | - Hvis `flag==0x04`, s� gir metadata den totale lengden p� pakken,          |
|                 |                    |   inkludert nyttelasten, i bytes.                                           |
|                 |                    | - Hvis `flag==0x20`, s� gir metadata en heltallsverdi som indikerer         |
|                 |                    |   �rsaken til � avsl� forbindelsesforesp�rselen. Du velger betydningen      |
|                 |                    |   av disse feilkodene.                                                      |
+-----------------+--------------------+-----------------------------------------------------------------------------+
| bytes           | `payload`          | Nyttelast, det vil si antall bytes som er angitt med forrige heltall,       |
|                 |                    | men aldri mer enn 1000 bytes. Nyttelast er bare inkludert i pakken          |
|                 |                    | hvis `flag==0x04`.                                                          |
+-----------------+--------------------+-----------------------------------------------------------------------------+

Merknader:
* I hver pakke kan bare �n av bitene i flags settes til 1. Hvis du mottar en pakke der dette ikke stemmer, kast den.
* En pakke kan kun ha nyttelast hvis `flags==0x04`. 
* Hvis `flags==0x01`, er serverens forbindelses-ID ukjent og m� inneholde 0.
* Den f�rste datapakken i en forbindelse har sekvensnummeret 0.
* En ACK-pakke inneholder sekvensnummeret til den siste pakken den har mottatt (ulikt TCP).

Programmet sender datagrammer som har en nyttelast mellom 0 og 1000 bytes (>=0 og <1000). Programmet er helt avhengig av RDP for � garantere at alle bytes kommer i riktig rekkef�lge og at ingen g�r tapt. NewFSP-applikasjonen tolker at en datapakke uten nyttelast indikerer slutten p� en filoverf�ring � for RDP er en tom datapakke bare en vanlig datapakke.


### Klienten

Klienten skal godta f�lgende kommandolinjeargumenter:
* IP-adressen eller vertsnavnet til maskinen der serverapplikasjonen kj�rer.
* UDP-portnummeret som brukes av serveren.
* Tapssannsynligheten prob gitt som en flytende verdi mellom 0 og 1, hvor 0 ikke er noe tap og 1 alltid er tap.

Klienten kaller funksjonen `set_loss_probability(prob)` ved starten av programmet.

Klienten pr�ver � koble seg til serveren. Hvis serveren ikke svarer p� forbindelsesforesp�rselen i l�pet av ett sekund, skriver klienten ut en feilmelding og avslutter. N�r klienten har koblet seg til, skriver den ut forbindelses-ID-ene til b�de klient og server p� skjermen.

N�r klienten har mottatt og lagret den komplette filen som er overf�rt av serveren og RDP-forbindelsen er lukket, skriver den ut navnet p� filen som den har skrevet (dvs. `kernel-file-<tilfeldig tall>`) til skjermen og avslutter.


### Serveren

Serveren skal godta f�lgende kommandolinjeargumenter:

* UDP-portnummeret som serverapplikasjonen skal motta pakker p�
* Filnavnet p� filen som den sender til alle tilkoblede NewFSP-klienter
* Antallet N filer som serveren skal servere f�r den avslutter seg selv.
* Taps-sannsynligheten gitt som en flyt-verdi mellom 0 og 1, hvor 0 ikke er noe tap og 1 alltid er tap.

Serveren kaller funksjonen `set_loss_probability(prob)` ved starten av programmet.

Hver gang en klient kobler seg til serveren, svarer serveren med en pakke med `flagg==0x10` og skriver ut forbindelses-ID-ene til b�de serveren og klienten til skjermen: `CONNECTED <klient-ID> <server-ID>`. Men hvis serveren allerede sender eller har sendt den N-te filen, godtar den ikke forbindelsen og svarer med en pakke med `flagg==0x20` og skriver ut: `NOT CONNECTED <klient-ID> <server-ID>`.

N�r en klient kobler seg fra serveren, skriver serveren ut forbindelses-ID-ene til b�de serveren og klienten til skjermen: `DISCONNECTED <klient-ID> <server-ID>`.

Serveren skal avslutte n�r den har levert N filer.


### Pakketap

Du m� ogs� h�ndtere tap p� stien fra klienten til serveren. Stien vil ha tap fordi du m� bruke funksjonen `send_packet()` som erstatning for C-funksjonen `sendto()`. Det tar de samme argumentene i samme rekkef�lge.

De medf�lgende filene `send_packet.c` og `send_packet.h` kompilerer p� p�loggingsmaskinene p� IFI uten noen `-std` kompileringsflagg. Det kompileres ogs� med `-std=gnu11` (standard), selv med `-Wall -Wextra`. Andre C-standarder, spesielt `-std=c11`, vil gi en advarsel og funksjonen vil muligens ikke funke riktig.

Pakketapssannsynligheten er gitt som et tall mellom 0 og 1. Hvis du implementerer pakkeformatet som beskrevet i denne oppgaven, blir bare pakker som inneholder data (`flags==0x04`) eller ACKs (`flags==0x08`) forkastet. Dette er for enkelhets skyld, men du kan fjerne den betingelsen hvis du ogs� vil oppleve tap av forbindelsesforesp�rsler og forbindelsesavslutninger.

B�de klienten og serveren din m� sette tapssannsynligheten ved � kalle funksjonen void `set_loss_probability(float x)` som er gitt i `send_packet.c`.


### Minneh�ndtering

Vi forventer at det ikke er minnelekkasjer under en normal kj�ring av applikasjonene. En normal kj�ring er en kj�ring der kommandolinjeargumenter og filnavn er gyldige. Alt dynamisk allokert minne m� eksplisitt deallokeres. Alle �pnede fildeskriptorer m� v�re eksplisitt lukket.

Vi forventer at Valgrind ikke viser noen advarsler under en normal kj�ring av applikasjonene. Hvis du er overbevist om at en advarsel er falsk, noe som sjelden skjer, m� du oppgi dette i koden eller i et eget dokument.

Ikke � oppfylle disse forventningene vil p�virke evalueringen av oppgaven din negativt.


## Innlevering

Du m� sende inn all koden din i et enkelt TAR-, TAR.GZ- eller ZIP-arkiv.

Hvis filen heter `<kandidatnummer>.tar` eller `<kandidatnummer>.tgz` eller `<candidatenumber>.tar.gz`, vil vi bruke kommandoen `tar` p� `login.ifi.uio.no` for � pakke den ut. Hvis filen heter `<kandidatnummer>.zip`, vil vi bruke kommandoen `unzip` p� `login.ifi.uio.no` for � pakke den ut. Forsikre deg om at dette fungerer f�r du laster opp filen. Det kan ogs� v�re smart � laste ned og teste koden etter innlevering.

Arkivet m� inneholde en `Makefile`, som skal ha minst disse alternativene:
* `make` - kompilerer begge programmene til kj�rbare bin�rfiler client og server
* `make all` - gj�r det samme som make uten parameter
* `make clean` - sletter kj�rbare filer og eventuelle midlertidige filer (f.eks. `*.o`)


## Om evalueringen

Hjemmeeksamen vil bli evaluert p� datamaskinene i `login.ifi.uio.no`-klyngen. Programmene m� kompileres og kj�res p� disse datamaskinene. Vi vil pr�ve � kj�re NewFSP-klient og server p� forskjellige datamaskiner i klyngen.

Hjemmeeksamen krever forst�else for ulike funksjoner som operativsystemer og nettverksprotokoller m� tilby applikasjoner, spesielt minneh�ndtering, filh�ndtering og nettverk.

Det er lurt � l�se deler av denne hjemmeeksamen i separate programmer f�rst og kombinere dem til en klient og en server senere.

Vi foresl�r � adressere deloppgavene som f�lger:

* Minneh�ndtering
  a. Ikke glem � frigj�re alt minnet du har allokert. I operativsystemer og nettverk er du alltid personlig ansvarlig for minneh�ndtering. � vite n�yaktig hvilket minne du bruker, hvor lenge du bruker det og n�r du kan frigj�re det, er en av de viktigste oppgavene i operativsystemer og nettverk. Vi bruker C i dette kurset for � sikre at du forst�r hvor vanskelig denne oppgaven er og hvordan du kan l�se den. � gj�re dette riktig (med hjelp fra Valgrind) er veldig viktig for hjemmeeksamen.
  b. Vi forventer ikke at serveren frigj�r alt minne f�r du implementerer nettverksdeloppgave f, men vi forventer at du husker minnet som du har allokert.
  c. Vi forventer ikke at klienten og serveren frigj�r alt minne i tilfelle krasj eller n�r du m� trykke `Ctrl-C` p� grunn av et annet problem.

* Nettverk
  a. Send UDP-pakker mellom klient og server ved hjelp av funksjonene `sendto()` og `recvfrom()`. Forsikre deg om at b�de klient og server frigj�r alt minne f�r du avslutter det.
  b. Implementere pakkeformatet som kreves av oppgaven.
  c. Bytt ut funksjonen `sendto()` med funksjonen `send_packet()` og sett tapssannsynligheten p� klientsiden til en verdi mellom 0 og 1.
  d. Pass p� at igjensending av pakker fungerer.
  e. Forsikre deg om at multipleksing fungerer etter forbindelse med flere klienter samtidig. Du m� bruke den medf�lgende funksjonen `send_packet()` med en viss sannsynlighet for pakketap. Filoverf�ring kan v�re for raskt for � starte flere samtidige NewFSP-klienter hvis du ikke gj�r det.
  f. Implementer �steng forbindelse�-funksjonaliteten og s�rg for at serveren frigj�r alt minne riktig f�r den avslutter.

* Fil p� serveren
  a. Sjekk om filen som er angitt p� kommandolinjen til NewFSP-serveren eksisterer. Avslutt med en feilmelding hvis filen ikke blir funnet.
  b. Du kan �pne filen for sending �n gang per forbindelse fra NewFSP-klienten, men du kan ogs� lese den en gang og holde den i minnet til NewFSP-serveren avsluttes.

* Fil p� klienten
  a. Sjekk at filen `kernel-file-<tilfeldig tall>` ikke eksisterer, og at du kan �pne den.
  b. Skriv pakkene som klienten mottar til filen.

* Opprette en komplett klient og server. Du m� l�se flere nettverksdeloppgaver og flere fildeloppgaver for � best� hjemmeeksamen. En slurvet l�sning for mange deloppgaver er like verdifull som en veldig god l�sning for noen f� deloppgaver.

## Nyttige og relevante funksjoner

* For _sockets_
  - `socket()`
  - `bind()`
  - `sendto()`
  - `recvfrom()`
  - `select()`
  - `perror()`
  - `getaddrinfo()`

* For filer
  - `fread()`
  - `fwrite()`
  - `fopen()`
  - `fclose()`
  - `basename()`
  - `lstat` or `fstat()`

* For mapper
  - `opendir()`
  - `readdir()`
  - `closedir()`

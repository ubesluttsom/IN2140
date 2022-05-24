# Ok, så jeg innser at denne makefila er unødvendig innviklet, men det jo en
# øvelsessak for meg, og jeg lærer mye av å gjøre det på denne måten.

.PHONY: all innlevering clean

OPPGAVER    = oppgave1 oppgave2
OPPGAVE1    = oppgave1.c
OPPGAVE2    = oppgave2.c oblig1_tests.c
BRUKERNAVN  = martimn
KILDEKODE   = $(OPPGAVE1) $(OPPGAVE2) oppgave3.md makefile
INNLEVERING = $(BRUKERNAVN).tar.gz
FLAGG       = -Wall

all: $(OPPGAVER)

oppgave1: $(OPPGAVE1)
oppgave2: $(OPPGAVE2:.c=.o)

$(OPPGAVER):
	# Kompilerer og linker `$@`
	gcc $(FLAGG) $^ -o $@

%.o: %.c
	# Kompilerer objektfil `$@`
	gcc $(FLAGG) -c $< -o $@

innlevering: $(INNLEVERING)

$(INNLEVERING): $(KILDEKODE)
	rm -f $(INNLEVERING)
	rm -f -r $(BRUKERNAVN)
	mkdir $(BRUKERNAVN)
	cp $(KILDEKODE) $(BRUKERNAVN)/
	tar czf $(INNLEVERING) $(BRUKERNAVN)/

clean:
	rm -f $(OPPGAVER) $(INNLEVERING) *.o
	rm -f -r $(BRUKERNAVN) *.dSYM

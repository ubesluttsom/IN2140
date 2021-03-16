# Ok, så jeg innser at denne makefila er unødvendig innviklet, men det jo en
# øvelsessak for meg, og jeg lærer mye av å gjøre det på denne måten.

.PHONY: all innlevering clean

OPPGAVER    = ruterdrift oppgave2
OPPGAVE1    = oppgave1/ruterdrift.c
OPPGAVE2    = oppgave2/oppgave2.md
BRUKERNAVN  = martimn
KILDEKODE   = $(OPPGAVE1) $(OPPGAVE2) Makefile
INNLEVERING = $(BRUKERNAVN).tar.gz
FLAGG       = -Wall -Wextra

run: ruterdrift
	./ruterdrift

all: $(OPPGAVER)

ruterdrift: $(OPPGAVE1:.c=.o)
	# Kompilerer og linker `$@`
	gcc $(FLAGG) $^ -o $@

oppgave2: $(OPPGAVE2:.md=.pdf)

%.o: %.c
	# Kompilerer objektfil `$@`
	gcc $(FLAGG) -c $< -o $@

%.pdf: %.md
	# Kompilerer pdf-fil fra markdown
	pandoc $< -o $@

# innlevering: $(INNLEVERING)

# $(INNLEVERING): $(KILDEKODE)
# 	rm -f $(INNLEVERING)
# 	rm -f -r $(BRUKERNAVN)
# 	mkdir $(BRUKERNAVN)
# 	cp -r $(KILDEKODE) $(BRUKERNAVN)/
# 	tar czf $(INNLEVERING) $(BRUKERNAVN)/
# 
# clean:
# 	rm -f $(OPPGAVER) $(INNLEVERING) *.o
# 	rm -f -r $(BRUKERNAVN) *.dSYM

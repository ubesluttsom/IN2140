# Ok, så jeg innser at denne makefila er unødvendig innviklet, men det jo en
# øvelsessak for meg, og jeg lærer mye av å gjøre det på denne måten.

.PHONY: run valgrind all innlevering clean oppgave2

OPPGAVER    = oppgave1 oppgave2
OPPGAVE1    = oppgave1/ruterdrift.c oppgave1/ruterdrift.h
OPPGAVE2    = oppgave2/oppgave2.pdf
MARKDOWN    = oppgave2/oppgave2-meta.md oppgave2/oppgave2.md
BRUKERNAVN  = martimn
KILDEKODE   = $(OPPGAVE1) $(OPPGAVE2) Makefile
INNLEVERING = $(BRUKERNAVN).tar.gz
FLAGG       = -Wall -Wextra -g

run: ruterdrift
	./ruterdrift testing_data/10_routers_15_edges/topology.dat testing_data/10_routers_15_edges/commands.txt

valgrind: ruterdrift
	valgrind --leak-check=full --show-leak-kinds=all ./ruterdrift testing_data/50_routers_150_edges/topology.dat testing_data/50_routers_150_edges/commands.txt

all: $(OPPGAVER)

oppgave1: $(OPPGAVE1)
	# Kompilerer og linker `$@`
	gcc $(FLAGG) $< -o ruterdrift

oppgave2: $(OPPGAVE2)

$(OPPGAVE2): $(MARKDOWN)
	# Kompilerer pdf-fil fra markdown
	pandoc -Vlang:nb $(MARKDOWN) -o $@

innlevering: $(INNLEVERING)

$(INNLEVERING): $(KILDEKODE)
	rm -f $(INNLEVERING)
	rm -f -r $(BRUKERNAVN)
	mkdir $(BRUKERNAVN)
	mkdir $(BRUKERNAVN)/oppgave1
	mkdir $(BRUKERNAVN)/oppgave2
	cp $(OPPGAVE1) $(BRUKERNAVN)/oppgave1/
	cp $(OPPGAVE2) $(BRUKERNAVN)/oppgave2/
	tar czf $(INNLEVERING) $(BRUKERNAVN)/

clean:
	rm -f $(INNLEVERING) oppgave1/*.o
	rm -f -r $(BRUKERNAVN) *.dSYM

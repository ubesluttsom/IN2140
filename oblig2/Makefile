.PHONY: run valgrind all innlevering clean oppgave1 oppgave2

OPPGAVER    = oppgave1 oppgave2
OPPGAVE1    = oppgave1/ruterdrift.c oppgave1/ruterdrift.h
OPPGAVE2    = oppgave2/oppgave2.pdf
MARKDOWN    = oppgave2/oppgave2-meta.md oppgave2/oppgave2.md
TESTDATA    = testing_data
BRUKERNAVN  = martimn
INNLEVERING = $(BRUKERNAVN).tar.gz
FLAGG       = -Wall -Wextra -g

all: oppgave1

run: oppgave1
	./ruterdrift testing_data/10_routers_15_edges/topology.dat testing_data/10_routers_15_edges/commands.txt

valgrind: oppgave1
	valgrind --leak-check=full --show-leak-kinds=all ./ruterdrift testing_data/50_routers_150_edges/topology.dat testing_data/50_routers_150_edges/commands.txt

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
	cp -r Makefile $(BRUKERNAVN)/
	cp -r $(OPPGAVE1) $(BRUKERNAVN)/oppgave1/
	cp -r $(TESTDATA) $(BRUKERNAVN)/testing_data/
	cp -r $(OPPGAVE2) $(BRUKERNAVN)/oppgave2/
	tar czf $(INNLEVERING) $(BRUKERNAVN)/

clean:
	rm -f $(INNLEVERING) ruterdrift new-topolopy.dat oppgave1/*.o
	rm -f -r $(BRUKERNAVN) *.dSYM

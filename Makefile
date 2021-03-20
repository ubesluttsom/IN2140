# Ok, så jeg innser at denne makefila er unødvendig innviklet, men det jo en
# øvelsessak for meg, og jeg lærer mye av å gjøre det på denne måten.

.PHONY: run valgrind all innlevering clean

OPPGAVER    = ruterdrift oppgave2
OPPGAVE1    = oppgave1/ruterdrift.c
OPPGAVE2    = oppgave2/oppgave2.md
BRUKERNAVN  = martimn
KILDEKODE   = $(OPPGAVE1) $(OPPGAVE2) Makefile
INNLEVERING = $(BRUKERNAVN).tar.gz
FLAGG       = -Wall -Wextra -g

run: ruterdrift
	./ruterdrift testing_data/10_routers_15_edges/topology.dat testing_data/10_routers_15_edges/commands.txt

valgrind: ruterdrift
	valgrind --leak-check=full --show-leak-kinds=all ./ruterdrift testing_data/50_routers_150_edges/topology.dat testing_data/50_routers_150_edges/commands.txt

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

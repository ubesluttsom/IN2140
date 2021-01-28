.PHONY: all clean

OPPGAVER = oppgave1 oppgave2
FLAGG    = -O0 -g -Wall

all: $(OPPGAVER)

oppgave1: oppgave1.o
	gcc $(FLAGG) $< -o $@

oppgave2: oppgave2.o oblig1_tests.o
	gcc $(FLAGG) $^ -o $@

%.o: %.c
	gcc $(FLAGG) -c $< -o $@

clean:
	rm -f $(OPPGAVER) *.o

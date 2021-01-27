.PHONY: all clean

OPPGAVER = oppgave1
FLAGG    = -O0 -g

all: $(OPPGAVER)

%: %.c
	gcc $(FLAGG) $< -o $@

clean:
	rm -f $(OPPGAVER)

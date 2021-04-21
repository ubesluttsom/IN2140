.PHONY: run valgrind all innlevering clean

PREKODE = send_packet.c send_packet.h
KLIENT = klient.c $(PREKODE)
SERVER = server.c $(PREKODE)

KANDIDATNUM  = xxxxxxx
INNLEVERING = $(KANDIDATNUM).tar.gz
FLAGG       = -Wall -Wextra -g -std=gnu11

all: klient server

run: server
	./server

# valgrind: klient server
# 	valgrind --leak-check=full --show-leak-kinds=all XXXXXXX

klient: $(KLIENT)
	# Kompilerer og linker `$@`
	gcc $(FLAGG) $< -o klient

server: $(SERVER)
	# Kompilerer og linker `$@`
	gcc $(FLAGG) $< -o server

innlevering: $(INNLEVERING)

$(INNLEVERING): $(KILDEKODE)
	rm -f $(INNLEVERING)
	rm -f -r $(KANDIDATNUM)
	mkdir $(KANDIDATNUM)
	cp -r Makefile $(KANDIDATNUM)/
	cp -r $(KLIENT) $(KANDIDATNUM)
	cp -r $(SERVER) $(KANDIDATNUM)
	tar czf $(INNLEVERING) $(KANDIDATNUM)/

clean:
	rm -f $(INNLEVERING) *.o
	rm -f -r $(KANDIDATNUM) *.dSYM

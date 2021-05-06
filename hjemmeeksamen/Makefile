.PHONY: run all innlevering clean

PREKODE = send_packet.c send_packet.h
RDP     = rdp.c rdp.h
KLIENT  = klient.c $(PREKODE) $(RDP)
SERVER  = server.c $(PREKODE) $(RDP)
BIN     = server klient

FLAGG = -Wall -Wextra -g -std=gnu11

KANDIDATNUM = 15629
INNLEVERING = $(KANDIDATNUM).tar.gz

all: server klient

klient: $(KLIENT)
server: $(SERVER)
$(BIN):
	# Kompilerer og linker `$@`
	gcc $(FLAGG) $(filter %.c, $^) -o $@

run: klient
	./klient ::1 2021

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
	rm -f server klient kernel-file-* *.gch
	# rm -f $(INNLEVERING) *.o
	# rm -f -r $(KANDIDATNUM) *.dSYM

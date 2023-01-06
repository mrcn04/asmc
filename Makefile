CC = gcc
CFLAGS = -mmacosx-version-min=10.14 -Wall -g -framework IOKit
EXEC   = asmc
PREFIX = /usr/local

build: $(EXEC)

$(EXEC) : smc.c
	$(CC) $(CFLAGS) -o $@ $?

install : $(EXEC)
	@install -v $(EXEC) $(PREFIX)/bin/$(EXEC)


clean:
	rm $(EXEC)
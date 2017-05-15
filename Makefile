
PREFIX=
ifneq ($(OS),Windows_NT)
	PREFIX=i586-mingw32msvc-
endif
CC=$(PREFIX)gcc
WINDRES=$(PREFIX)windres
CCFLAGS=-D_UNICODE -DUNICODE
#console, windows
SUBSYSTEM=console

all: main.exe

run: all
	./main.exe

util.o: util.c util.h
	$(CC) $(CCFLAGS) -ggdb -o $@ -c $<

main.o: main.c util.h
	$(CC) $(CCFLAGS) -ggdb -o $@ -c $<

main.exe: main.o util.o
	$(CC) $(CCFLAGS) -ggdb -o $@ $^ -Wl,--subsystem,$(SUBSYSTEM) -lkernel32 -luser32 -lcomctl32 -lgdi32 -ladvapi32

clean:
	rm -f *.o main.exe

.PHONY: all run clean

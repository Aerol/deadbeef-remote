CFLAGS=-O2
LDFLAGS=-shared

all : remote.so client.exe
.PHONY : all

remote.so : remote.o
	cc $(LDFLAGS) $(CFLAGS) -o $@ $^

client.exe : client.o
	cc $(CFLAGS) -o $@ $^

clean :
	rm remote.so client.exe *.o

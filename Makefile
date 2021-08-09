
all: libcalc test client server



servermain.o: servermain.c protocol.h
	$(CXX) -Wall -c servermain.c -I.

clientmain.o: clientmain.c protocol.h
	$(CXX) -Wall -c clientmain.c -I.

main.o: main.cpp protocol.h
	$(CXX) -Wall -c main.cpp -I.


test: main.o calcLib.o
	$(CXX) -L./ -Wall -o test main.o -lcalc


client: clientmain.o calcLib.o
	$(CXX) -L./ -Wall -o client clientmain.o -lcalc

server: servermain.o calcLib.o
	$(CXX) -L./ -Wall -o server servermain.o -lcalc -pthread




calcLib.o: calcLib.c calcLib.h
	gcc -Wall -fPIC -c calcLib.c

libcalc: calcLib.o
	ar -rc libcalc.a -o calcLib.o

clean:
	rm *.o *.a test server client

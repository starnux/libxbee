options = -pedantic-errors -D_DEBUG_
makes = serveur

all:$(makes)

test001:test001.c
	gcc -o test001 test001.c
testAPI:testAPI.c
	gcc -o testAPI testAPI.c
testSend:testSend.c
	gcc -o testSend testSend.c
test002:test002.c
	gcc -o test002 test002.c
test002API:test002API.c libxbee.o
	gcc -o test002API test002API.c libxbee.o
sendAPI:sendAPI.c libxbee.o
	gcc -o sendAPI sendAPI.c libxbee.o
recvAPI:recvAPI.c libxbee.o
	gcc -o recvAPI recvAPI.c libxbee.o
libxbee.o:libxbee.c libxbee.h
	gcc -o libxbee.o -c libxbee.c -pedantic $(options)
serveur:serveur.c libxbee.o
	gcc -o serveur libxbee.o serveur.c $(options)

clean:
	rm $(makes) *.o

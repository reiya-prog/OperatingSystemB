UNAME = $(shell whoami)
port = 21000

server: server.c DieWithError.c HandleTCPClient.c
	gcc server.c DieWithError.c HandleTCPClient.c -lpthread -o server
run: server
	./server ${port} ${UNAME}

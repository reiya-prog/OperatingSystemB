UNAME = $(shell whoami)
port = 21000

client: client.c DieWithError.c
	gcc client.c DieWithError.c -o client
run: client
	./client ${port} ${UNAME}

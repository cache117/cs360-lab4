all: client

client: client.cpp
	g++ -o client client.cpp
fork: fork.c
	g++ -o fork fork.c
clean:
	rm client fork

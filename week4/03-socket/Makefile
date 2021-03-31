all: hclient hserver

CC=gcc
hclient: ./client/client.c
	${CC} -g $^ -o ./client/hclient

hserver: ./server/server.c
	${CC} -g $^ -lpthread -o ./server/hserver

run: hclient hserver
	sudo python topo.py 3

run1: hclient hserver
	sudo python topo.py 1

run2: hclient hserver
	sudo python topo.py 2

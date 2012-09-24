
CC = gcc

LIBS = -lpthread\
	/home/users/cse533/Stevens/unpv13e/libunp.a
	
FLAGS = -g -O2

CFLAGS = ${FLAGS} -I /home/users/cse533/Stevens/unpv13e/lib\

all: lin_tour arp_lin

lin_tour: lin_tour.o get_hw_addrs.o
	${CC} ${FLAGS} -o lin_tour lin_tour.o get_hw_addrs.o ${LIBS}
	
lin_tour.o: lin_tour.c ass4version1.h hw_addrs.h ping.h
	${CC} ${CFLAGS} -c lin_tour.c
	
arp_lin: arp.o common.o
	${CC} ${FLAGS} -o arp_lin arp.o common.o ${LIBS}

arp.o: arp.c common.h
	${CC} ${CFLAGS} -c arp.c	
	
common.o: common.c common.h
	${CC} ${CFLAGS} -c common.c
	
get_hw_addrs.o: get_hw_addrs.c hw_addrs.h
	${CC} ${CFLAGS} -c get_hw_addrs.c
	
clean:
	rm lin_tour arp_lin lin_tour.o get_hw_addrs.o arp.o common.o

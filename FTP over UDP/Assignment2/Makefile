
CC = gcc

LIBS =  -lsocket  -lm  -lresolv -lsocket -lnsl -lpthread -lposix4\
	/home/courses/cse533/Stevens/unpv13e_solaris2.10/libunp.a

FLAGS =  -g -O2
CFLAGS = ${FLAGS} -I/home/courses/cse533/Stevens/unpv13e_solaris2.10/lib

PROGS=		prifinfo_plus client server
CLEANFILES= rtt.o client.o server.o prifinfo_plus.o get_ifi_info_plus.o reliable_server_send.o reliable_client_send.o

all:	${PROGS}

client: client.o rtt.o
	${CC} ${FLAGS} -o client client.o get_ifi_info_plus.o rtt.o ${LIBS}

client.o: client.c
	${CC} ${CFLAGS} -c client.c

server: server.o
	${CC} ${FLAGS} -o server server.o get_ifi_info_plus.o ${LIBS}

server.o: server.c
	${CC} ${CFLAGS} -c server.c

rtt.o: rtt.c
	${CC} ${CFLAGS} -c rtt.c

reliable_server_send.o: reliable_server_send.c
		${CC} ${CFLAGS} -c reliable_server_send.c

reliable_client_send.o: reliable_client_send.c
		${CC} ${CFLAGS} -c reliable_client_send.c

get_ifi_info_plus.o: get_ifi_info_plus.c
	${CC} ${CFLAGS} -c get_ifi_info_plus.c

prifinfo_plus: get_ifi_info_plus.o prifinfo_plus.o
	${CC} -o prifinfo_plus get_ifi_info_plus.o prifinfo_plus.o ${LIBS}

prifinfo_plus.o: prifinfo_plus.c
	${CC} ${CFLAGS} -c prifinfo_plus.c


clean:
		rm -f ${PROGS} ${CLEANFILES}

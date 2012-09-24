This is a README File for the Assignment A1:

System Documentation:

This is a client  server programme where in th eEcho & Daytime service has been implemented. 
The Client is made up of 2 modules namely :Echo Client  & Daytime client. 
At any given point of time one could have multiple clients connected to a server.

Assumptions:

The Daytime service is hardwired to the Port# 8888
The Echo service is hardwired to the Port# 9999
This code has been developed on a windows machine hence the path used for xterm  in the execlp command is "xterm" with the 
absolute path mentioned in the X-Win32 2010 under Connection field in the general tab (/usr/openwin/bin/xterm -ls) . 
Note on solaris the absolute path might have to be givenThough we take in the server IP address/Domain name. 
I have hardcoded the server to the local host to run the client server  application.

Implementation:

CLIENT:

1.Command line arguments procesing: This has been implemented using the function gethostbyname(), gethostbyaddress() 
where in if the user enters the IP address the domain name is displayed & vice -a-versa.

2.The Client Parent:  Here the client is continously quering the user for I/P  and on recieving the same 2 process are forked off( through the fork call).

3.The Multiplexing  is done by the select() call  for the processes. Status messages form the child to the client are passed through the half duplex pipe.

4.The client child:  The client children are  are handled by generarting an xterm window through the command  execlp().

5.Service requests termination & programm robustness:The Daytime client is terminated through ^C  which has be handled in the program by notifying user 
the terminated child ,Id No & other details through the SIGCHILD() signal. The ^D termination of the Echo client has been partially handled.

6.IPC Pipe: A half duplex pipe has been used for th epurpose of communication between the parent & child client

7.More Robustness: Handled partially. SIGCHILD Implemented.

SERVER:

1.Multithreading services: 2 threads have been used & a select call, to  dispay daytime on the xterm after every 5secs.( i.e. sleep for 5 secs).

2.Relation to inetd superserver: Used the idea behind it to build the server.

3.Listening to multiple services: This was implemented through select.

4.Robustness:Implemented partially.

5.Time server Implementation: Implemented

6.Proper status message: Implemented

7.SO_REUSEADDR socket option: Implemented

8.Non-Blocking concept: Not implemented.


User & Testing Documentation:

1.Compiling the programme: Run the make file uploaded , name : Makefile

2.Post compilation the object & executable file should be seen.On doing an ls the following files are displayed:

3.Makefile, readline.o, tcpechotimecli.o,client*,server*,time_cli*,echo_cli*,
tcpechotimecli.c,time_cli.c,echo_cli.c,tcpechotimesrv.o,time_cli.o,echo_cli.o,
tchechotimesrv.c

Note : 
echo_cli.c : Echo client file
time_cli.c:  Time client
tcpechotimecli.c: Client 
tcpechotimesrv.c: Server

4.Run the executable server* by executing the command ./server

5.Run the execuatbel client* followed by an IP addres/Domain Name by executing the command ./client <IP address/Domain name> (e.g. ./client 127.0.0.1)

6.On running the above programme the user would be given an option to choose between the 3 services

1.Echo client
2.Time service
3.Quit 

choose accordingly & then :

for: 1.Type data in th exterm generated window & see the sam eis reflected back on the window
for: 2.See wether one is able to see the time being reflected after every 5secs.
for: 3.Enter quit should exit the programme.
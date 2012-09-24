/* Given the hostname, populate the struct hostent get the ip address */


#include "unp.h"
#include  <signal.h>
#include  <setjmp.h>

int sig_flg;
volatile sig_atomic_t eflag = 0;

jmp_buf  JumpBuffer; 

void
sig_chld(int signo)
{
    pid_t   pid;
    int             stat;

    Signal(SIGCHLD, sig_chld);   /* Register the signal for the next run */ 
    eflag = 1;
    printf ("eflag is %d \n", eflag);
    pid = wait(&stat);
    printf("child %d terminated\n", pid);
    return ;
}

int main(int argc, char **argv)
{
int opt;
int n;
struct in_addr  addr ;
struct hostent *hp;
pid_t childpid;
int echo_pfd[2], time_pfd[2];
char buf[MAXLINE + 1];
char **pp;
char *inet_addr; 


/* invoke the program with server name or server's IP address
   Display server name and IP address of the server */
   if( argc != 2)
	err_quit("usage: myclient <hostname>");


   if (inet_aton(argv[1], &addr) != 0)  {
	hp = gethostbyaddr((const char *)&addr, sizeof(addr), AF_INET); 
	    if(hp == NULL) {
	        err_quit("gethostbyaddr error for host: %s: %s",argv[1], hstrerror(h_errno));
	}
    }
    else  {                              
	hp = gethostbyname(argv[1]);
    if (hp == NULL){
   	err_quit("gethostbyname failed for host: %s: %s",argv[1], hstrerror(h_errno));
	}
}


 /*********************************************** */
/* Now we have valid pointer for hostent stucture. 
	Print the Host name and IPaddress from this structure */


/* *************************************** */

    printf("official hostname: %s\n", hp->h_name);

    for (pp = hp->h_aliases; *pp != NULL; pp++)
	printf("alias: %s\n", *pp);

    for (pp = hp->h_addr_list; *pp != NULL; pp++) {
	addr.s_addr = ((struct in_addr *)*pp)->s_addr;
	printf("IPaddress: %s\n", inet_ntoa(addr));
	inet_addr =  inet_ntoa(addr);
    }
    Signal(SIGCHLD, sig_chld);    
/*  get service type from the uyser
    Echo service --- 1
    Time service --- 2
    Quit application --- 3
*/
    while(1){
    printf("Enter the service option\n\r");
    printf("Select 1 for ECHO    2 for TIME    3 to Quit \n\r");
    scanf("%d", &opt);
    eflag = 0;
    if(opt == 1){
	printf("ECHO service\n\r");
        if(pipe(echo_pfd) < 0)
	    err_quit("pipe error ");
	childpid = fork();
	if ( childpid == 0){
	    close(echo_pfd[0]); /*  Close the read end. Child writes in to the pipe */
	    execlp("xterm", "xterm", "-e", "./echo_cli", inet_addr, &echo_pfd[1], NULL);
	}
	else { /* code for parent of Echo Client */
             close(echo_pfd[1]); /* close write descriptor of the pipe */
	     while (1){
		n = read(echo_pfd[0], buf, MAXLINE);
		if(n == 0){
		    printf("Pipe: Read failed \n \r"); 
                    break;
                }
		write(1, buf, n);
	     }
             close(echo_pfd[0]); /* before next iteration close the read des as well */	
	}/* end parent */
    }/* end opt */
else if(opt == 2){
	printf("Time service\n\r");
	
	if(pipe(time_pfd) < 0)
		err_quit("pipe error ");
	childpid = fork();
	if ( childpid == 0){
	    printf(" I am Time_client Child \n\r ");
	    close(time_pfd[0]); /* close the read descriptor on the child */ 
	    execlp("xterm", "xterm", "-e", "./time_cli", "127.0.0.1", &time_pfd[1], NULL);
	}
	else { 
	    printf("I am Time_client Parent \n\r ");
	    close(time_pfd[1]); /* close the write descriptor on the parent */ 
	    while(1){
		n = read(time_pfd[0], buf, MAXLINE);
		if(n == 0){
		    printf("Pipe: Read failed \n\r");
                    break;
                } 
		write(1, buf, n);
	    }	
            close(time_pfd[0]); /* before next iteration close the read des as well */	
	}
    }
    else if(opt == 3){
	printf("Quit application\n\r");
	break;
    }
    else 
	printf("Error: select either 1 - Echo_client, 2 - Time client 3 - Quit only \n\r"); 
}/* end while */

}/* end main */

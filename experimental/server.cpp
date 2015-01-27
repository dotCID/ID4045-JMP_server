/* A combination of 
   http://www.linuxhowtos.org/C_C++/socket.htm
   and the Arduino Serial library
   
   MCW - 19/01/2015
*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
namespace local{ 					// to make the calling of these functions easier
#include <arduino-serial-lib.c>
}
#include <cstring>

int ardfd, sockfd, newsockfd;
bool alive = true;
const char* serialport = "/dev/ttyACM0";
void readTCP(int);

/*
	Function to properly close all connections before exiting
	@return void
	@param message The error message code provided by the system
*/
void exit_program(int message){
	printf("Received Exit call\n");
	if(ardfd>0) local::serialport_close(ardfd);
	close(sockfd);
	close(newsockfd);
	exit(message);
}

/*
	Function to handle errors
	@return void
	@param msg The error message code provided by the system
*/
void error(const char *msg)
{
    perror(msg);
    //exit_program(1);
}

/*
	Initialise the serial connection to the Arduino
	@return void
*/
void initArduinoSerial(){
	int baud = 9600;
	ardfd = local::serialport_init(serialport, baud);
	if( ardfd==-1 ) printf("ERROR: couldn't open port to Arduino\n");
	else printf("opened port %s\n",serialport);
	local::serialport_flush(ardfd);
}

/*
	Read serial messages from the Arduino
	@return void
	@param buffer The buffer the messages are stored in
*/
void readArduinoSerial(char* buffer){
	local::serialport_read_until(ardfd, buffer, '\n', 256, 5000);
}

/*
	The main program
	@return int A status code to return
	@param argc The amount of commandline arguments
	@param argv The commandline arguments: [server port](required), [Arduino serial port](optional)
*/
int main(int argc, char *argv[])
{
     int portno, pid;
     socklen_t clilen;
     struct sockaddr_in serv_addr, cli_addr;

     if (argc < 2) {
         fprintf(stderr,"ERROR, not enough arguments\n");
         exit_program(1);
     }
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) 
        error("ERROR opening socket");
     bzero((char *) &serv_addr, sizeof(serv_addr));
     portno = atoi(argv[1]);
    if(argc > 2) serialport = argv[2];
     
	 initArduinoSerial();
     
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              error("ERROR on binding address");
     listen(sockfd,5);
     printf("Listening to incoming signals.\n");
     clilen = sizeof(cli_addr);
		
		// Infinite loop to create a new process for each connection, keeps the server alive indefinitely
	 while (alive) {
         newsockfd = accept(sockfd, 
               (struct sockaddr *) &cli_addr, &clilen);
         if (newsockfd < 0) 
             error("ERROR accepting connection");
         pid = fork();
         if (pid < 0)
             error("ERROR on forking");
         if (pid == 0)  {
             close(sockfd);
             readTCP(newsockfd);
             exit(0);
         }else close(newsockfd);
     }
     close(sockfd);
     return 0;
}


/*
 Reads the incoming TCP messages and responds if needed. 
 A separate instance is created by the loop above for each connection.
 @return void
 @param sock The socket used for the connection to be read
 */
void readTCP (int sock)
{
   int n;
   char buffer[256];
   char buf2[256];
      
   bzero(buffer,256);
   n = read(sock,buffer,255);
   if (n < 0) error("ERROR reading from socket");
   printf("Received message: %s\n",buffer);
   local::serialport_flush(ardfd);
   if(ardfd>0) local::serialport_write(ardfd, buffer);
   bzero(buffer, 256);
   readArduinoSerial(buffer);
   printf("Response: %s", buffer);
   
   if(strcmp(buffer, (char *)"echo\n")==0) n = write(sock, buffer, strlen(buffer));
   if(strcmp(buffer, (char *)"aAddr\n")==0) n = write(sock, serialport, strlen(serialport));
   if(strcmp(buffer, (char *)"getPos\n")==0) {
	    local::serialport_write(ardfd, buffer);
   		bzero(buffer, 256);
		readArduinoSerial(buffer);
		//char * testMessage = (char *)"aExt(34.54) aRot(55.03) hPos(05.00) vPos(18.25)";
   		n = write(sock, buffer, strlen(buffer));
   }
   
   //n = write(sock,"I got your message",18);
   if (n < 0) error("ERROR writing to socket");

}

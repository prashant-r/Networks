/*
* audiolisten.c
* Created on: Nov 15, 2016
*      Author: prashantravi
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <wait.h>
#include <termios.h>
#include <stdbool.h>
#include <stdio_ext.h>
#include <semaphore.h>
#include <strings.h>
#include <unistd.h>
#include <linux/soundcard.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <time.h>
#define BUFSIZE 1024
#define AUDIODEVICE "/dev/audio"
#define AUDIOMODE O_WRONLY
#define SHARED 0



// Example execution :
// Here server-ip server-tcp-port client-udp-port payload-size playback-del gamma buf-sz target-buf logfile-c filename(short or long)
//./audiolisten.o localhost 6554 5645 250 2500 30 40000 20000 logfile-c filedeposit/pp.au
	// server-ip = localhost
	// server-tcp-port = 6554
	// client-udp-port = 5645
	// payload-size = 250B
	// playback-del = 2500 ms = 2.5 seconds
	// lambda = 30msec
	// buf-sz = 40,000
	// target-buf = 20,000
	// logfile-c
	// filename = filedeposit/pp.au





int create_socket_to_listen_on(char *rand_port);
size_t send_socket_data(int in_fd, char * message, struct sockaddr * server_addr);
size_t recv_socket_data(int in_fd, char * buffer);
void packet_handler();
int  open_audio();
void report_statistics(int Q_star, int Q_t, int tau);
void close_audio(int fd);
long getTimeDifference(struct timeval *t1 , struct timeval *t2);
void playback_handler();
int nsleep(long miliseconds);
void do_sleep_alarm(long time);
void play_it_back();

int plot_time;
// TODO: Check arguments are correct.
int pyld_sz;
int pl_del;
int sfd;
int max_buf_sz;
int audio_fd;
int target_buf_sz;
int gamm_r;
FILE * logFile;
FILE * plotFile;
struct sockaddr_in * server_to_transact_with;
volatile int current_buffer_level;
volatile bool transmitting = false;
struct timeval startWatch;
sem_t full;
char * shared_buffer;
int how_many_for_a_second;
/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}


int nsleep(long miliseconds)
{
   struct timespec req, rem;
   if(miliseconds > 999)
   {
        req.tv_sec = (int)(miliseconds / 1000);                            /* Must be Non-Negative */
        req.tv_nsec = (miliseconds - ((long)req.tv_sec * 1000)) * 1000000; /* Must be in range of 0 to 999999999 */
   }
   else
   {
        req.tv_sec = 0;                         /* Must be Non-Negative */
        req.tv_nsec = miliseconds * 1000000;    /* Must be in range of 0 to 999999999 */
   }
   for(;;){
	   if( nanosleep(&req , &req) == -1) continue;
	   else break;
   }

   return 0;
}

int open_audio()
{
    int fd=-1;
    fd= open(AUDIODEVICE, AUDIOMODE);
    if (fd < 0)
    {
        printf("error opening audio device\n");
        exit(1);
    }
    return fd;
}

void close_audio(int fd)
{
    close(fd);
}

void playback_handler() {

    if(transmitting)
    {
        if(sem_trywait(&full) != 0){
            return;
        }
        play_it_back();
    }
    else if(current_buffer_level && audio_fd)
    	play_it_back();
}
int last_sec = -1;
void play_it_back()
{

	if(current_buffer_level < pyld_sz)
	{
		// we can write off the buffer_level amount of stuff
	    write(audio_fd, shared_buffer, current_buffer_level);
	    current_buffer_level = 0;
	}
	else
	{
		// we need to move what we can't write
		write(audio_fd, shared_buffer, pyld_sz);
	    memmove(shared_buffer, shared_buffer+pyld_sz, current_buffer_level - pyld_sz);
	    current_buffer_level = current_buffer_level - pyld_sz;
	 }

	 struct timeval tz2;
	 if (-1 == gettimeofday(&tz2, NULL)) {
		 perror("resettimeofday: gettimeofday");
	     exit(-1);
	 }
	 long ms = getTimeDifference(&startWatch,&tz2);
	 double time_duration = ms/1000.0;

	 int curr_sec = (int) time_duration;
	 if(curr_sec > last_sec)
	 {
	    fprintf(plotFile, "%d		%d\n", curr_sec, current_buffer_level);
	    last_sec = curr_sec;
	 }

	 fprintf(logFile, "PB Handler | Time: %lf sec | current_buffer_level: %d  \n", time_duration, current_buffer_level);
}

void packet_handler()
{

    if(transmitting)
    {
        char buffer[pyld_sz];
        int n = 0;
        n = recv_socket_data(sfd, buffer);
        if( n > 0)
        {
            if(n == 3)
            {
                char god[3] = {'G', 'O', 'D'};
                int ret;
                if(god[0] == buffer[0] && god[1] == buffer[1] && god[2] == buffer[2]){
                    // End of transmission 
                    fprintf(logFile, "End of transmission current_buffer_level %d \n", current_buffer_level);
                    transmitting = false;
                    return;
                }
            }
            // Some data came my way. Proceed to write to user buffer.
        }
        // Report the current statistics 
        int tau = gamm_r;
        report_statistics(target_buf_sz, current_buffer_level, tau);
        //printf("current_buffer_level %d \n ", current_buffer_level);
        memcpy(&(shared_buffer[current_buffer_level]), buffer, n);
        current_buffer_level += n;
        sem_post(&full);
        struct timeval tz2;
        if (-1 == gettimeofday(&tz2, NULL)) {
        	perror("resettimeofday: gettimeofday");
            exit(-1);
        }
        long ms = getTimeDifference(&startWatch,&tz2);
        double time_duration = ms/1000.0;

        int curr_sec = (int) time_duration;

        if(curr_sec > last_sec)
        {
        	fprintf(plotFile, "%d		%d\n", curr_sec, current_buffer_level);
        	last_sec = curr_sec;
        }
        fprintf(logFile, "PACK HANDLER | Time: %lf sec | current_buffer_level: %d  \n", time_duration, current_buffer_level);
    }
}


void report_statistics(int Q_star, int Q_t, int tau)
{
    // convert first argument to char array
    int i;
    char first[11]; 
    sprintf(first,"%d", Q_star);

    // convert second argument to char array
    char second[11]; 
    sprintf(second,"%d", Q_t);

    // convert third argument to char array
    char third[11];
    sprintf(third, "%d", tau);
    // construct the statistic message
    char statistic[50];
    bzero(statistic, 50);
    strcat(statistic, " ");
    strcat(statistic, "Q");
    strcat(statistic, " ");
    strcat(statistic, first);
    strcat(statistic, " ");
    strcat(statistic, second);
    strcat(statistic, " ");
    strcat(statistic, third);
    strcat(statistic, " ");

    // send the message over
    send_socket_data(sfd, statistic, server_to_transact_with);
}

int main(int argc, char **argv) {
    int sockfd, portno, n;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char * hostname;
    char * client_udp_port;
    char * payload_size;
    char * playback_del;
    char * buf_sz;
    char * target_buf;
    char * logfile_c;
    char * plotfile_c;
    char * filename; 

    char buf[BUFSIZE];

    /* check command line arguments */
    if (argc != 11) {
     fprintf(stderr,"usage: %s server-ip server-tcp-port client-udp-port payload-size playback-del gamma buf-sz target-buf logfile-c filename(short or long)\n", argv[0]);
     exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);
    client_udp_port = argv[3];
    payload_size = argv[4];
    pyld_sz = atoi(payload_size);
    playback_del = argv[5];
    pl_del = atoi(playback_del);
    gamm_r = atoi(argv[6]);
    buf_sz = argv[7];
    max_buf_sz = atoi(buf_sz);
    target_buf = argv[8]; // Q*
    target_buf_sz = atoi(target_buf);
    logfile_c = argv[9];
    filename = argv[10];
    plotfile_c = "client.dat";
    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
     (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    /* connect: create a connection with the server */
    if (connect(sockfd, &serveraddr, sizeof(serveraddr)) < 0) 
      error("ERROR connecting");


    // sigio or sigpoll
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = &packet_handler;
    sa.sa_flags = SA_RESTART;
    if (!((sigaction(SIGIO, &sa, NULL ) == 0) && (sigaction(SIGPOLL, &sa, NULL) == 0)))
    {
        perror("Can't create io signal action.");
        exit(EXIT_FAILURE);
    }

    // create socket to listen on 
    sfd = create_socket_to_listen_on(client_udp_port);
    /* construct the message for the user */
    bzero(buf, BUFSIZE);
    char port_path[50];
    bzero(port_path, 50);

    strcat(port_path, client_udp_port);
    strcat(port_path, " ");
    strcat(port_path, filename);
    strcpy(buf, port_path);


    /* send the message line to the server */
    n = write(sockfd, buf, strlen(buf));
    if (n < 0) 
        error("ERROR writing to socket");

    /* print the server's reply */
    bzero(buf, BUFSIZE);
    n = read(sockfd, buf, BUFSIZE);
    if (n < 0) 
        error("ERROR reading from socket");
    close(sockfd);

    // decode the message
    // Split the received buf by whitespace
    char * pch;
    char * server_status_r = strtok (buf," ");
    char * server_port_r = strtok (NULL, " ");

    if(strcmp(server_status_r, "OK") == 0)
    {
        // server to transact with
        struct  sockaddr_in server;
        server.sin_family = AF_INET;
        struct hostent *hp, *gethostbyname();
        hp = gethostbyname(hostname);
        bcopy ( hp->h_addr, &(server.sin_addr.s_addr), hp->h_length);
        server.sin_port = htons(atoi(server_port_r));
        server_to_transact_with = &server;


        /* open the audio device driver */
        audio_fd = open_audio();

        if (-1 == gettimeofday(&startWatch, NULL)) {
            perror("resettimeofday: gettimeofday");
            exit(-1);
        }
        shared_buffer = malloc( sizeof(char) * ( max_buf_sz + 1 ) );

        // semaphore to protect the shared_buffer
        sem_init(&full, SHARED, 0);   /* sem full = 0 , shared between threads of same process */

        /* open the log file */
        logFile = fopen(logfile_c, "wb");


        // open the plot file
        plotFile = fopen(plotfile_c, "wb");


        if (logFile == NULL) {
            printf("I couldn't open logfile_c for appending.\n");
            exit(0);
        }

        transmitting = true;
        current_buffer_level = 0;
        printf("Buffering..  \n");
        nsleep((long) pl_del-1);
        printf("Now playing %s \n", filename);

        plot_time = -1;
        do_sleep_alarm(1);
        while(transmitting)
        {
        	do_sleep_alarm(gamm_r);
        }
        while(!transmitting && current_buffer_level > 0)
        {
        	// Finish it up
        	do_sleep_alarm(gamm_r);
        }
        fclose(logFile);
        fclose(plotFile);
        close_audio(audio_fd);
        audio_fd = -1;
        sem_close(&full);
        sem_destroy(&full);
}
return 0;
}


int create_socket_to_listen_on(char *rand_port)
{
    int  sd;
    struct sockaddr_in server;
    struct sockaddr_in foo;
    int len = sizeof(struct sockaddr);
    char buf[512];
    int rc;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(atoi(rand_port));
    sd = socket (AF_INET, SOCK_DGRAM, 0);
    bind ( sd, (struct sockaddr *) &server, sizeof(server));
    getsockname(sd, (struct sockaddr *) &foo, &len);
    int sockfd = sd;
    int flags = 0;
    if (-1 == (flags = fcntl(sockfd, F_GETFL, 0)))
        flags = 0;
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        perror("Cant' make socket non blocking");
        exit(EXIT_FAILURE);
    }
    if (-1 == (flags = fcntl(sockfd, F_GETFL, 0)))
        flags = 0;
    if (fcntl(sockfd, F_SETFL, flags | O_ASYNC) < 0)
    {
        perror("Can't make socket asynchronous");
        exit(EXIT_FAILURE);
    }
    if (fcntl(sockfd, F_SETOWN, getpid()))
    {
        perror("Can't own the socket");
        exit(EXIT_FAILURE);
    }
    return sd;
}

size_t send_socket_data(int in_fd, char * message, struct sockaddr * server_addr)
{
    size_t numSent;
    struct sockaddr recv_sock;
    socklen_t addr_len = sizeof(recv_sock);
    struct sockaddr * who_to_send_addr = server_addr;
    numSent = sendto(in_fd, message, strlen(message) , 0, who_to_send_addr, addr_len);
    return numSent;
}
size_t recv_socket_data(int in_fd, char * buffer)
{
    struct sockaddr_in from;
    size_t length=sizeof(struct sockaddr_in);
    int n = recvfrom(in_fd,buffer,pyld_sz,0,(struct sockaddr *)&from, &length);
    if (n < 0) error("recvfrom");
    return n;
}

long getTimeDifference(struct timeval *t1 , struct timeval *t2)
{
    struct timeval tv1 = *t1;
    struct timeval tv2 = *t2;
    long milliseconds;
    milliseconds = (tv2.tv_usec - tv1.tv_usec) / 1000;
    milliseconds += (tv2.tv_sec - tv1.tv_sec) *1000;
    return milliseconds;
}

void do_sleep_alarm(long time)
{
	nsleep(time);
	playback_handler();
}


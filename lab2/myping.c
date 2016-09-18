/*
 * mypingd.c
 *
 *  Created on: Sep 17, 2016
 *      Author: prashantravi
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

typedef int bool;
#define true 1
#define false 0

#define MAX_BUF 1000
#define EXPECTED_RESPONSE "terve"


bool received = false;

pid_t current_pid = -1 ;

void make_alphanumeric_string(char *s, const int len);
int sendPingRequest(char* hostname, char* hostUDPport, char* secretKey);
void validateCommandLineArguments(int argc, char ** argv);
void printTimeDifference(struct timeval *t1 , struct timeval *t2);
void kill_current_process(int sig);

void kill_current_process(int sig)
{
	int saved_errno = errno;
	if(received == false){
		printf( "no response from ping server\n" );
		kill(current_pid,SIGKILL);
	}
	errno = saved_errno;
}


void make_alphanumeric_string(char *s, const int len) {
	static const char alphanum[] =
			"0123456789"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz";

	for (int i = 0; i < len; ++i) {
		s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
	}
	s[len] = 0;
}

void printTimeDifference(struct timeval *t1 , struct timeval *t2)
{
	struct timeval tv1 = *t1;
	struct timeval tv2 = *t2;
	long milliseconds;
	milliseconds = tv2.tv_usec - tv1.tv_usec / 1000;
	milliseconds += (tv2.tv_sec - tv1.tv_sec) *1000;
	printf("Round trip time was : %3ld\n",milliseconds);
}

void validateCommandLineArguments(int argc, char ** argv)
{
	// Write this validation properly TODO
	if(argc != 4)
	{
		printf("\nERROR! usage: %s receiver_hostname receiver_port secret_key\n\n", argv[0]);
		exit(1);
	}
}


int sendPingRequest(char* hostname, char* hostUDPport, char* secretKey)
{
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	if ((rv = getaddrinfo(hostname, hostUDPport, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 2;
	}

	struct addrinfo *availableServerSockets = servinfo;
	bool connectionSuccessful = false;
	while(availableServerSockets != NULL)
	{
		bool error = false;

		if ((sockfd = socket(availableServerSockets->ai_family, availableServerSockets->ai_socktype,availableServerSockets->ai_protocol)) == -1) {//If it fails...
			error = true;
		}

		if (connect(sockfd, availableServerSockets->ai_addr, availableServerSockets->ai_addrlen) == -1) {
			error = true;
		}
		if(error)
			availableServerSockets = availableServerSockets->ai_next;
		else
		{
			connectionSuccessful = true;
			break;
		}
	}

	if(!connectionSuccessful)
	{
		printf("Could not connect to host \n");
		return 2;
	}
	else
	{
		// Successfully connected to server
	}
	int tr = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tr, sizeof(int));

	// Construct message with format $secretKey$pad to be 1000 bytes
	char str[MAX_BUF];
	//Get length of secret key.
	int dlen = (strlen(secretKey)) * sizeof(char);
	sprintf(str, "$%s$", secretKey);
	dlen = dlen + 2*sizeof(char);
	//Find how many alphanumeric characters we need to pad
	int dpad = MAX_BUF - dlen;
	//Our final sending buffer
	char fSendBuffer[MAX_BUF] = {0};
	memcpy(fSendBuffer, str , dlen);
	make_alphanumeric_string(fSendBuffer+dlen, dpad );

	char recv_response[MAX_BUF];
	struct timeval tv1;
	struct timezone tz1;
	struct timeval tv2;
	struct timezone tz2;
	signal(SIGALRM,(void (*)(int))kill_current_process);
	ualarm (2.55*1000000, 0);
	if (-1 == gettimeofday(&tv1, &tz1)) {
		perror("resettimeofday: gettimeofday");
		exit(-1);
	}

	struct sockaddr_in to;
	struct sockaddr from;
	memset(&to, 0, sizeof(to));
	to.sin_family = AF_INET;
	to.sin_addr.s_addr   = inet_addr(hostname);

	struct sockaddr_storage their_addr;
	socklen_t addr_len = sizeof their_addr;
	socklen_t sin_size = sizeof(struct sockaddr);
	sendto(sockfd, fSendBuffer, 1000*sizeof(char), 0, (struct sockaddr*)&to, sizeof(to));
	socklen_t addrlen = sizeof(from); /* must be initialized */
	recvfrom(sockfd, recv_response, sizeof(recv_response), 0, &from, &addrlen);
	received = true;
	if (-1 == gettimeofday(&tv2, &tz2)) {
		perror("resettimeofday: gettimeofday");
		exit(-1);
	}
	if(strcmp(EXPECTED_RESPONSE, recv_response ) == 0)
		printTimeDifference(&tv1, &tv2);
	else
		printf("Packet was corrupt : %s\n", recv_response);
	close(sockfd);
	return 0;
}

int main(int argc, char** argv)
{
	validateCommandLineArguments(argc, argv);
	current_pid = getpid();
	return sendPingRequest(argv[1], argv[2], argv[3]);
}
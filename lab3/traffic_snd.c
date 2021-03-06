/*
 * mypingd.c
 *
 *  Created on: Sep 17, 2016
 *      Author: prashantravi
 *
 */


#define _DEFAULT_SOURCE
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
#define TIME_OUT 550000
#define INTERVAL 50000

volatile bool received = false;
pid_t current_pid = -1 ;

int sendTrafficGenerationRequest(char* hostname, char* hostUDPport, char* payload_size, char * packet_count, char * packet_spacing);
void validateCommandLineArguments(int argc, char ** argv);
long getTimeDifference(struct timeval *t1 , struct timeval *t2);

void validateCommandLineArguments(int argc, char ** argv)
{
	if(argc != 6)
	{
		printf("\nERROR! (expected 5 , got %d) usage: %s receiver_hostname receiver_port payload_size packet_count packet_spacing\n\n", argc, argv[0]);
		exit(1);
	}
}
/*
	Send the traffic using the designated arguments. 


*/


int sendTrafficGenerationRequest(char* hostname, char* hostUDPport, char* payload_size, char * packet_count, char * packet_spacing)
{

	int payloadSize = atoi(payload_size);
	int packetCount = atoi(packet_count);
	unsigned int packetSpacing = atoi(packet_spacing);

	int sockfd;
	struct addrinfo hints, *servinfo;
	int rv;
	int numbytes;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	if ((rv = getaddrinfo(hostname, hostUDPport, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 2;
	}
	char payload[payloadSize];


	// Fill payload with first letter of last name
	memset(&payload, 'R', payloadSize);

	// Setup socket. 
	struct addrinfo *availableServerSockets = servinfo;
	bool connectionSuccessful = false;
	while(availableServerSockets != NULL)
	{
		bool error = false;

		if ((sockfd = socket(availableServerSockets->ai_family, availableServerSockets->ai_socktype,availableServerSockets->ai_protocol)) == -1) {//If it fails...
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
		// Connection was not successful.
		printf("Could not connect to host \n");
		return 2;
	}
	else
	{
		// Successfully connected to server
	}
	int tr = 1;
	char recv_response[6];
	struct timeval tv1;
	struct timezone tz1;
	struct timeval tv2;
	struct timezone tz2;
	struct sockaddr_in from;
	socklen_t addr_len = sizeof from;
	socklen_t sin_size = sizeof(struct sockaddr);
	// Start the timer before sending
	if (-1 == gettimeofday(&tv1, NULL)) {
		perror("resettimeofday: gettimeofday");
		exit(-1);
	}

	int total_bytes_sent = 0;
	int total_packets_sent = 0;
	int packets_sent = 0;
	int packet_counter = 0;

	// Send the packetCount number of packets of designated size
	for (packet_counter = 0; packet_counter < packetCount; packet_counter++)
	{

		if(packet_counter > 0)
		{
			// if not the first packet then sleep.
			usleep(packetSpacing);
		}
		if((packets_sent = sendto(sockfd, payload, payloadSize , 0, availableServerSockets->ai_addr, availableServerSockets->ai_addrlen)) == -1)
		{
			
			perror("sendto: failed\n");
			exit(EXIT_FAILURE);
		}
		total_bytes_sent += packets_sent;
	}
	// Account for the bytes in UDP header 
	total_bytes_sent +=  ( 46 * packet_counter)	;

	char lastThreeBytes[] = {'G', 'O', 'D'};
	int g =0;
	// Send the last 3 3-byte packets without stopping.
	for (g = 0; g< 3; g++)
	{

		if((packets_sent = sendto(sockfd, lastThreeBytes, 3, 0, availableServerSockets->ai_addr, availableServerSockets->ai_addrlen))== -1)
		{
			perror("sendto: failed\n");
			exit(EXIT_FAILURE);
		}
	}
	// Stop the timer
	if (-1 == gettimeofday(&tv2, NULL)) {
		perror("resettimeofday: gettimeofday");
		exit(-1);
	}
	long long int total_num_bits = (8*total_bytes_sent);
	long ms = getTimeDifference(&tv1,&tv2);
	double time_duration = ms/1000.0;
	printf("Completion time: %lf seconds\n", time_duration);
	printf("Application bitrate: %lf pps\n", (double) packetCount/time_duration);
    printf("Application bitrate: %lf bps\n", (double)(total_num_bits/time_duration));
	close(sockfd);
	return EXIT_SUCCESS;
}
// Helper function to compute the time difference between two timevals 
// to the order of milliseonds.
long getTimeDifference(struct timeval *t1 , struct timeval *t2)
{
	struct timeval tv1 = *t1;
	struct timeval tv2 = *t2;
	long milliseconds;
	milliseconds = (tv2.tv_usec - tv1.tv_usec) / 1000;
	milliseconds += (tv2.tv_sec - tv1.tv_sec) *1000;
	return milliseconds;
}



int main(int argc, char** argv)
{
	// valdidate that the command line arguments are correct.
	validateCommandLineArguments(argc, argv);
	current_pid = getpid();
	return sendTrafficGenerationRequest(argv[1], argv[2], argv[3], argv[4], argv[5]);
}
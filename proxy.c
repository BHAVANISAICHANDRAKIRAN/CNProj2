#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#include "proxy_MyLib2.h"

#define N 10
int url_block_len = (N - 6);
int word_block_len = (N - 4);

char *url_block[] = { "facebook.com", "hulu.com", "youtube.com", "virus.com" };	//BLOCK LIST URL'S

char *word_block[] = { "bad", "alcoholic", "ape", "cheater", "stupid" };	//BLOCK LIST WORDS


int main(int argc, char **argv)	//MAIN BEGINS
{
	struct hostent *hostent;
	struct sockaddr_in addr_in, client_addr, server_addr;
	int sockfd, newsockfd;
	pid_t pid;
	int client_len = sizeof(client_addr);
	struct stat st = { 0 };


	if (argc != 2)	//CHECK THE PORT NUMBER FROM THE USER INPUT
	{

		printf("Please enter the port number\n\n");
		return -1;
	}

	printf("Server running...\n");


	if (stat("./cache/", &st) == -1) {	//CREATE CACHE DIRECTORY
		mkdir("./cache/", 0700);
	}

	bzero((char*)&server_addr, sizeof(server_addr));
	bzero((char*)&client_addr, sizeof(client_addr));

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(argv[1]));
	server_addr.sin_addr.s_addr = INADDR_ANY;


	sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);	//LISTEN SOCKET CREATED
	if (sockfd < 0)
	{
		perror("Initialize the socket fail");
	}


	if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)	//SOCKET BINDING
	{
		perror("Bind the socket fail");
	}


	listen(sockfd, 50);	//SUPPORT 50 CLIENTS

start:
	newsockfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);

	if ((pid = fork()) == 0)
	{
		struct sockaddr_in host_addr;


		int cfd;			//CACHE fd
		int i, n;

		int port = 80;		//PORT USED AS DEFAULT ONE
		int rsockfd;
		char proto[256];	//HTTP PROTOCOL VERSION
		char url[4096];
		char type[256];		//REQUEST TYPE



		char datetime[256];


		char url_host[256], url_path[256];	//SEPERATE URL AND PATH

		char encryptedURL[4096];
		char filepath[256];

		char *dateptr;
		char buffer[4096];
		int response_code;	//HTTP RESPONSE

		bzero((char*)buffer, 4096);


		recv(newsockfd, buffer, 4096, 0);	//RECEIVE THE REQUEST

		sscanf(buffer, "%s %s %s", type, url, proto);	//ANALYSE THE HTTP REQUEST


		if (url[0] == '/')
		{
			strcpy(buffer, &url[1]);
			strcpy(url, buffer);
		}




		if ((strncmp(type, "GET", 3) != 0) || ((strncmp(proto, "HTTP/1.1", 8) != 0) && (strncmp(proto, "HTTP/1.0", 8) != 0)))
		{


			sprintf(buffer, "400 : BAD REQUEST\nSEND GET REQUESTS ");
			send(newsockfd, buffer, strlen(buffer), 0);


			goto end;
		}


		parseURL(url, url_host, &port, url_path);	//READ HOST AND PATH


		encryptURL(url, encryptedURL);


		printf("\t-> Host URL: %s\n", url_host);
		printf("\t-> Host Port Number: %d\n", port);




		for (i = 0; i < url_block_len; i++)
		{

			if (NULL != strstr(url, url_block[i]))	//CHECK IF THE REQUESTED URL IS IN BLOCK LIST
			{

				printf("\t-> Blocked URL: %s\n", url_block[i]);


				sprintf(buffer, "400 : BAD REQUEST\nSORRY!!! THIS URL IS BLOCKED BY OUR SERVER\n  %s  ", url_block[i]);
				send(newsockfd, buffer, strlen(buffer), 0);


				goto end;
			}
		}

		if ((hostent = gethostbyname(url_host)) == NULL)	//RESOLVE IP OF HOST
		{
			fprintf(stderr, "Could not resolve %s: %s\n", url_host, strerror(errno));
			goto end;
		}

		bzero((char*)&host_addr, sizeof(host_addr));
		host_addr.sin_port = htons(port);
		host_addr.sin_family = AF_INET;
		bcopy((char*)hostent->h_addr, (char*)&host_addr.sin_addr.s_addr, hostent->h_length);


		rsockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);	//SOCKET FOR REMOTE HOST CONNECTION

		if (rsockfd < 0)
		{
			perror("Remote socket not created");
			goto end;
		}


		if (connect(rsockfd, (struct sockaddr*)&host_addr, sizeof(struct sockaddr)) < 0)
		{
			perror("Remote server could not be connected");
			goto end;
		}


		printf("\t-> Now serving the host: %s w/ ip: %s\n", url_host, inet_ntoa(host_addr.sin_addr));




		sprintf(filepath, "./cache/%s", encryptedURL);	//CHECK IF CACHE HAS THE CONTENTS OF REQUESTED URL
		if (0 != access(filepath, 0)) {

			sprintf(buffer, "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", url_path, url_host);

			printf("\t-> Accessed from server and not cache\n");


			goto request;
		}


		sprintf(filepath, "./cache/%s", encryptedURL);	//FILE IN CACAHE
		cfd = open(filepath, O_RDWR);
		bzero((char*)buffer, 4096);

		read(cfd, buffer, 4096);
		close(cfd);


		dateptr = strstr(buffer, "Date:");
		if (NULL != dateptr)
		{


			bzero((char*)datetime, 256);

			strncpy(datetime, &dateptr[6], 29);


			sprintf(buffer, "GET %s HTTP/1.0\r\nHost: %s\r\nIf-Modified-Since: %s\r\nConnection: close\r\n\r\n", url_path, url_host, datetime);

			printf("\t-> Sending a conditional GET request to server to check if modified since %s...\n", datetime);

		}
		else {

			sprintf(buffer, "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", url_path, url_host);

			printf("\t-> Could not check modified since\n");

		}

	request:

		n = send(rsockfd, buffer, strlen(buffer), 0);	//SEND TO HOST

		if (n < 0)
		{
			perror("Could not write to remote socket");
			goto end;
		}



		cfd = -1;	//CACHE FOR FUTURE USE


		do
		{
			bzero((char*)buffer, 4096);


			n = recv(rsockfd, buffer, 4096, 0);

			if (n > 0)
			{

				if (cfd == -1)
				{
					float ver;

					sscanf(buffer, "HTTP/%f %d", &ver, &response_code);	//CHECK RESPONSE CODE


					printf("\t-> Response: %d\n", response_code);


					if (response_code != 304)
					{

						sprintf(filepath, "./cache/%s", encryptedURL);	//FILE CREATED IN CACHE DIRECTORY
						if ((cfd = open(filepath, O_RDWR | O_TRUNC | O_CREAT, S_IRWXU)) < 0)
						{
							perror("Cache file creation failed");
							goto end;
						}

						printf("\t-> from the remote host...\n");

					}
					else {


						printf("\t-> Content is not modified\n");
						printf("\t-> Fetch from local cache...\n");


						goto from_cache;
					}
				}


				for (i = 0; i < word_block_len; i++)
				{

					if (NULL != strstr(buffer, word_block[i]))
					{

						printf("\t-> The word in blacklist: %s\n", word_block[i]);


						close(cfd);


						sprintf(filepath, "./cache/%s", encryptedURL);
						remove(filepath);


						sprintf(buffer, "400 : BAD REQUEST\nSORRY OUR SERVER HAS BLOCKED THIS WEBSITE AS IT HAS WORDS FROM BLACKLIST\n  %s  ", word_block[i]);
						send(newsockfd, buffer, strlen(buffer), 0);
						goto end;
					}
				}


				write(cfd, buffer, n);
			}
		} while (n > 0);
		close(cfd);



	from_cache:


		sprintf(filepath, "./cache/%s", encryptedURL);	//READ FROM THE CACHE FILE
		if ((cfd = open(filepath, O_RDONLY)) < 0)
		{
			perror("failed to open cache file");
			goto end;
		}
		do
		{
			bzero((char*)buffer, 4096);
			n = read(cfd, buffer, 4096);
			if (n > 0)
			{

				send(newsockfd, buffer, n, 0);
			}
		} while (n > 0);
		close(cfd);

	end:

		close(rsockfd);
		close(newsockfd);
		close(sockfd);
		return 0;
	}
	else {

		close(newsockfd);


		goto start;
	}

	close(sockfd);
	return 0;

}

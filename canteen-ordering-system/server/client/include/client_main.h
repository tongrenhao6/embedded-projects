#ifndef CLIENT_MAIN_H
#define CLIENT_MAIN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

int connect_to_server(void);
int receive_from_server(int menu_sockfd, char *buffer, int len);
void show_menu(int menu_sockfd);

#endif



#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <time.h>

#define PORT 3491 // порт соединения

#define MAXDATASIZE 100 // максимальный объем пакета

int main(int argc, char *argv[])
{
    int sockfd, numbytes;  
    struct hostent *he;
    struct sockaddr_in their_addr; // адрес сервера

    if ((he=gethostbyname(argv[1])) == NULL) {  // получить адрес сервера
        herror("gethostbyname");
        exit(1);
    }

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        perror("socket");
        exit(1);
    }

    their_addr.sin_family = AF_INET;    // системный порядок
    their_addr.sin_port = htons(PORT);  // сетевой порядок
    their_addr.sin_addr = *((struct in_addr *)he->h_addr);
    memset(&(their_addr.sin_zero), '\0', 8);  // обнуляем остаток структуры

    char buf[MAXDATASIZE];

    while(1) {

        if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
            perror("socket");
            exit(1);
        }
        if (connect(sockfd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr)) == -1) {
            perror("connect");
            exit(1);
        }

        if ((numbytes=recv(sockfd, buf, MAXDATASIZE, 0)) == -1) {
            perror("recv");
            exit(1);
        }

        buf[numbytes] = '\0';

        printf("Принято байт: %d\n", numbytes);
        printf("Принято сообщение: %s\n", buf);

        if (strcmp(buf, "FINISH") == 0) {
            close(sockfd);
            break;
        }

        close(sockfd);
    }


    return 0;
}
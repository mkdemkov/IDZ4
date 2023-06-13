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

#define PORT 3490 // порт соединения

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

    int M = atoi(argv[2]);
    int N = atoi(argv[3]);
    int K = atoi(argv[4]);

    int buf[K+2];
    for(int i = 0; i < K+1; i++) {
        buf[i] = -1;
    }

    buf[K+1] = 1; // id

    int try = 1;
    while(1) {

        if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
            perror("socket");
            exit(1);
        }
        if (connect(sockfd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr)) == -1) {
            perror("connect");
            exit(1);
        }


        printf("Try: %d\n", try);

        int len, bytes_sent;

        len = sizeof(buf);
        bytes_sent = send(sockfd, buf, len, 0);

        printf("Длина сообщения: %d\n", len);
        printf("Отправлено байт: %d\n", bytes_sent);

        if (buf[K]+1 >= M * N) {
            printf("%d > %d\n", buf[K], M*N);
            break;
        }

        if ((numbytes=recv(sockfd, buf, sizeof(buf), 0)) == -1) {
            perror("recv");
            exit(1);
        }

        buf[numbytes] = '\0';

        printf("Принято байт: %d\n", numbytes);

        printf("Принято от библиотекаря:");
        for(int i = 0; i < K; i++) {
            printf(" %d", buf[i]);
        }
        printf("\n");
        printf("Номер полки: %d\n", buf[K]);

        try++;
        close(sockfd);

        sleep(3);
    }


    return 0;
}
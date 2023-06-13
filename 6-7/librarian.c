#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <time.h>

#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>


#define MAXDATASIZE 100 // максимальный объем пакета

#define SHM_SIZE 1024

int MYPORT = 3495;    // порт
int LOG_PORT = 3491;    // порт

#define BACKLOG 10     // количество входящих соединений

sem_t *sem_librarian;
sem_t *sem_break;
sem_t *sem_finish;
int shm_fd;
void *shm_ptr;
int shm_fd_finish;
void *shm_ptr_finish;

char name[] = "fifo3.fifo";

void sigchld_handler(int s) {
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

void sigint(int s) {
    sem_close(sem_librarian);
    sem_close(sem_break);
    sem_unlink("/libsem2");
    sem_unlink("/breaksem2");
    shm_unlink("/shm");
    munmap(shm_ptr, SHM_SIZE);
    close(shm_fd);
}

void *logThread(void *vargp) {
    int sockfd, new_fd;  // слушаем на sock_fd, соединение в new_fd
    struct sockaddr_in my_addr;    // локальный адрес
    struct sockaddr_in their_addr; // удаленный адрес
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        perror("socket");
        exit(1);
    }
    
    my_addr.sin_family = AF_INET;         // системный порядок байт
    my_addr.sin_port = htons(LOG_PORT);     // сетевой порядок байт
    my_addr.sin_addr.s_addr = INADDR_ANY; // автоматически указать локальный IP
    memset(&(my_addr.sin_zero), '\0', 8); // обнулить остаток

    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1) {
        perror("bind");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // закончить процессы
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    // create the pipe
    remove(name);
    (void)umask(0);
    mknod(name, S_IFIFO | 0666, 0);

    sem_finish = sem_open("/finishsem2", O_CREAT, 0666, 1);

    shm_fd_finish = shm_open("/shm_finish", O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd_finish, SHM_SIZE);
    shm_ptr_finish = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_finish, 0);
    if (shm_ptr_finish == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    int* shm_arr_finish = (int*) shm_ptr_finish;


    while(1) {
        sem_wait(sem_finish);
        if (shm_arr_finish[0] == 1) {
            sem_post(sem_finish);
            break;
        }
        sem_post(sem_finish);

        sin_size = sizeof(struct sockaddr_in);
        if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size)) == -1) {
            perror("accept");
            continue;
        }
        printf("logger: соединение с %s\n", inet_ntoa(their_addr.sin_addr));
        pid_t pid = fork();
        if (pid == 0) { // это дочерний процесс
            close(sockfd); // которому не нужно слушать

            // read from pipe
            int fd = open(name, O_RDONLY);
            char buf[100] = {0};
            read(fd, buf, sizeof(buf));

            // // check if buf == "FINISH"
            if (strcmp(buf, "FINISH") == 0) {
                printf("logger: FINISH\n");
                sem_wait(sem_finish);
                shm_arr_finish[0] = 1;
                sem_post(sem_finish);
                close(new_fd);
                close(fd);
                exit(0);
            }

            // send to client

            if (send(new_fd, buf, sizeof(buf), 0) == -1) {
                perror("send");
            }
            close(new_fd);
            close(fd);
            exit(0);
        } else if (pid == -1) {
            perror("fork");
            //exit(EXIT_FAILURE);
        }
        close(new_fd);  // основному процессу не нужно соединение
    }

    close(sockfd);

    sem_close(sem_finish);
    sem_unlink("/finishsem2");
    shm_unlink("/shm_finish");
    munmap(shm_ptr_finish, SHM_SIZE);
}


int main(int argc, char *argv[]) {

    int main_port = atoi(argv[4]);
    int logger_port = atoi(argv[5]);

    MYPORT = main_port;
    LOG_PORT = logger_port;

    pthread_t tid;
    pthread_create(&tid, NULL, logThread, NULL);

    srand(time(0));

    // create shared memory
    shm_fd = shm_open("/shm", O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, SHM_SIZE);
    shm_ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    sem_librarian = sem_open("/libsem2", O_CREAT, 0666, 1);
    sem_break = sem_open("/breaksem2", O_CREAT, 0666, 1);

    if (sem_librarian == SEM_FAILED || sem_break == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }


    int sockfd, new_fd;  // слушаем на sock_fd, соединение в new_fd
    struct sockaddr_in my_addr;    // локальный адрес
    struct sockaddr_in their_addr; // удаленный адрес
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;

    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
        perror("setsockopt");
        exit(1);
    }
    
    my_addr.sin_family = AF_INET;         // системный порядок байт
    my_addr.sin_port = htons(MYPORT);     // сетевой порядок байт
    my_addr.sin_addr.s_addr = INADDR_ANY; // автоматически указать локальный IP
    memset(&(my_addr.sin_zero), '\0', 8); // обнулить остаток

    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1) {
        perror("bind");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // закончить процессы
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    struct sigaction act;
    act.sa_handler = sigint;
    // if (sigaction(SIGINT, &act, NULL) == -1) {
    //     perror("sigaction");
    //     exit(1);
    // }

    int M = atoi(argv[1]);
    int N = atoi(argv[2]);
    int K = atoi(argv[3]);

    int* array = malloc(M * N * K * sizeof(int));
    for (int i = 0; i < M * N * K; i++) {
        array[i] = i;
    }
    for (int i = 0; i < M * N * K; i++) {
        int j = rand() % (M * N * K);
        int tmp = array[i];
        array[i] = array[j];
        array[j] = tmp;
    }

    printf("Array: ");
    for (int i = 0; i < M * N * K; i++) {
        printf("%d ", array[i]);
    }
    printf("\n");

    int last_bookshelf_index = 0;
    int result[M * N * K];

    sem_wait(sem_librarian);
    int* shm_arr = (int*) shm_ptr;
    shm_arr[0] = 0;
    sem_post(sem_librarian);


    while(1) {  // основной цикл accept()
        usleep(100);
        sem_wait(sem_break);
        if (shm_arr[0] > M*N) {
            sem_post(sem_break);
            break;
        }
        sem_post(sem_break);
        sin_size = sizeof(struct sockaddr_in);
        if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size)) == -1) {
            perror("accept");
            continue;
        }
        printf("сервер: соединение с %s\n", inet_ntoa(their_addr.sin_addr));
        pid_t pid = fork();
        if (pid == 0) { // это дочерний процесс
            close(sockfd); // которому не нужно слушать
            sem_wait(sem_librarian);
            sem_wait(sem_break);
            

            // read from client
            int numbytes;
            int buf[K+2];
            if ((numbytes=recv(new_fd, buf, sizeof(buf), 0)) == -1) {
                perror("recv");
                exit(1);
            }
            printf("Принято байт %d\n", numbytes);

            printf("Принято от студента %d:", buf[K+1]);
            for(int i = 0; i < K; i++) {
                printf(" %d", buf[i]);
            }
            printf("\n");

            // do work
            int* shm_array = (int*) shm_ptr;
            if (buf[0] != -1) {
                for(int i = 0; i < K; i++) {
                    shm_array[buf[K]*K + i + 1] = buf[i];
                }
            }

            // send to client
            int msg[K+2];
            msg[K] = shm_array[0];
            msg[K+1] = buf[K+1];
            for(int i = 0; i < K; i++) {
                msg[i] = array[shm_array[0]*K + i];
            }

            // write to pipe
            int fd;
            if ((fd = open(name, O_WRONLY)) == -1) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            char str[100] = {0};
            sprintf(str, "Принято от студента %d: %d %d %d %d\nОтправлено студенту %d: %d %d %d %d",  
                buf[K+1], buf[0], buf[1], buf[2], buf[3],
                buf[K+1], msg[0], msg[1], msg[2], msg[3]);

            if (write(fd, str, sizeof(str)) == -1) {
                perror("write");
                exit(EXIT_FAILURE);
            }
            close(fd);


            if (send(new_fd, msg, sizeof(msg), 0) == -1) {
                perror("send");
            }

            close(new_fd);

            shm_array[0]++;
            sem_post(sem_librarian);
            sem_post(sem_break);
            exit(0);
        } else if (pid == -1) {
            perror("fork");
            //exit(EXIT_FAILURE);
        }
        usleep(100);
        close(new_fd);  // основному процессу не нужно соединение
    }

    // sort result
    int* shm_array = (int*) shm_ptr;
    for (int i = 1; i <= M * N * K; i++) {
        for (int j = i + 1; j <= M * N * K; j++) {
            if (shm_array[i] > shm_array[j]) {
                int tmp = shm_array[i];
                shm_array[i] = shm_array[j];
                shm_array[j] = tmp;
            }
        }
    }

    printf("Result: ");
    for (int i = 1; i <= M * N * K; i++) {
        printf("%d ", shm_array[i]);
    }
    printf("\n");

    int fd;
    if ((fd = open(name, O_WRONLY)) == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }
    char str[100] = "FINISH";
    if (write(fd, str, sizeof(str)) == -1) {
        perror("write");
        exit(EXIT_FAILURE);
    }
    close(fd);

    close(sockfd);

    sem_close(sem_librarian);
    sem_close(sem_break);
    sem_unlink("/libsem2");
    sem_unlink("/breaksem2");
    shm_unlink("/shm");
    munmap(shm_ptr, SHM_SIZE);
    close(shm_fd);

    pthread_exit(NULL);

    return 0;
} 
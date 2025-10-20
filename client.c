#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define LENGTH 2048

volatile sig_atomic_t flag = 0;
int sockfd = 0;
char name[32];

pthread_mutex_t stdout_mutex = PTHREAD_MUTEX_INITIALIZER;

void str_trim_lf(char* arr, int length) {
    for (int i = 0; i < length; i++) {
        if (arr[i] == '\n') {
            arr[i] = '\0';
            break;
        }
    }
}

void catch_ctrl_c_and_exit(int sig) {
    flag = 1;
}

void print_prompt() {
    printf("\r> ");
    fflush(stdout);
}

void clear_line() {
    printf("\r\033[K");
    fflush(stdout);
}

void send_msg_handler() {
    char message[LENGTH] = {};
    char buffer[LENGTH] = {};

    while(1) {
        pthread_mutex_lock(&stdout_mutex);
        print_prompt();
        pthread_mutex_unlock(&stdout_mutex);
        
        if (fgets(message, LENGTH, stdin) == NULL) {
            break;
        }
        
        str_trim_lf(message, LENGTH);

        if (strcmp(message, "exit") == 0) {
            break;
        } else if (strlen(message) > 0) {
            // Send plain message: "name: message"
            sprintf(buffer, "%s: %s", name, message);
            
            pthread_mutex_lock(&stdout_mutex);
            clear_line();
            pthread_mutex_unlock(&stdout_mutex);
            
            send(sockfd, buffer, strlen(buffer), 0);
        }

        memset(message, 0, LENGTH);
        memset(buffer, 0, LENGTH);
    }
    catch_ctrl_c_and_exit(2);
}

void recv_msg_handler() {
    char message[LENGTH] = {};
    while (1) {
        int receive = recv(sockfd, message, LENGTH, 0);
        if (receive > 0) {
            pthread_mutex_lock(&stdout_mutex);
            clear_line();
            printf("%s\n", message);  // Print the colored message from server
            print_prompt();
            pthread_mutex_unlock(&stdout_mutex);
        } 
        else if (receive == 0) {
            break;
        }
        memset(message, 0, LENGTH);
    }
}

int main(int argc, char **argv){
    if (argc != 3) {
        printf("Usage: %s <ip> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *ip = argv[1];
    int port = atoi(argv[2]);

    signal(SIGINT, catch_ctrl_c_and_exit);

    printf("Please enter your name: ");
    fgets(name, 32, stdin);
    str_trim_lf(name, strlen(name));

    if (strlen(name) < 2 || strlen(name) >= 32) {
        printf("Name must be 2-30 characters.\n");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        printf("ERROR: connect\n");
        return EXIT_FAILURE;
    }

    send(sockfd, name, 32, 0);

    printf("=== WELCOME TO THE CHATROOM ===\n");
    print_prompt();

    pthread_t send_msg_thread;
    pthread_t recv_msg_thread;

    pthread_create(&send_msg_thread, NULL, (void *) send_msg_handler, NULL);
    pthread_create(&recv_msg_thread, NULL, (void *) recv_msg_handler, NULL);

    while (1) {
        if(flag) {
            printf("\nBye\n");
            break;
        }
    }

    close(sockfd);
    return EXIT_SUCCESS;
}

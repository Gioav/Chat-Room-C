#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>

#define MAX_CLIENTS 100
#define BUFFER_SZ 2048

static _Atomic unsigned int cli_count = 0;
static int uid = 10;

/* Client structure */
typedef struct {
    struct sockaddr_in address;
    int sockfd;
    int uid;
    char name[32];
    char color_code[10];
} client_t;

client_t *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Color codes
const char* colors[] = {
    "\033[31m", // Red
    "\033[32m", // Green  
    "\033[33m", // Yellow
    "\033[34m", // Blue
    "\033[35m", // Magenta
    "\033[36m", // Cyan
    "\033[91m", // Light Red
    "\033[92m", // Light Green
    "\033[93m", // Light Yellow
    "\033[94m", // Light Blue
    "\033[95m", // Light Magenta
    "\033[96m", // Light Cyan
};

void str_trim_lf(char* arr, int length) {
    for (int i = 0; i < length; i++) {
        if (arr[i] == '\n') {
            arr[i] = '\0';
            break;
        }
    }
}

/* Add client to queue */
void queue_add(client_t *cl) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (!clients[i]) {
            clients[i] = cl;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

/* Remove client from queue */
void queue_remove(int uid) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] && clients[i]->uid == uid) {
            clients[i] = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

/* Send message to all clients except sender */
void send_message(char *s, int uid) {
    pthread_mutex_lock(&clients_mutex);
    
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] && clients[i]->uid != uid) {
            if (write(clients[i]->sockfd, s, strlen(s)) < 0) {
                perror("ERROR: write to descriptor failed");
            }
        }
    }
    
    pthread_mutex_unlock(&clients_mutex);
}

/* Handle all communication with the client */
void *handle_client(void *arg) {
    char buffer[BUFFER_SZ];
    char name[32];
    int leave_flag = 0;

    cli_count++;
    client_t *cli = (client_t *)arg;

    // Assign color based on UID
    strcpy(cli->color_code, colors[cli->uid % 12]);

    // Get client name
    if (recv(cli->sockfd, name, 32, 0) <= 0 || strlen(name) < 2) {
        printf("Client didn't enter a valid name.\n");
        leave_flag = 1;
    } else {
        str_trim_lf(name, strlen(name));
        strcpy(cli->name, name);
        
        // Send join message WITH COLOR
        sprintf(buffer, "%s%s has joined\033[0m\n", cli->color_code, cli->name);
        printf("%s has joined\n", cli->name);
        send_message(buffer, cli->uid);
    }

    bzero(buffer, BUFFER_SZ);

    while (!leave_flag) {
        int receive = recv(cli->sockfd, buffer, BUFFER_SZ, 0);
        
        if (receive > 0) {
            if (strlen(buffer) > 0) {
                // Create colored message
                char colored_msg[BUFFER_SZ];
                sprintf(colored_msg, "%s%s\033[0m\n", cli->color_code, buffer);
                
                // Send colored message to all clients
                send_message(colored_msg, cli->uid);
                
                // Print plain message on server
                printf("%s\n", buffer);
            }
        } 
        else if (receive == 0) {
            // Client left
            sprintf(buffer, "%s%s has left\033[0m\n", cli->color_code, cli->name);
            printf("%s has left\n", cli->name);
            send_message(buffer, cli->uid);
            leave_flag = 1;
        } 
        else {
            perror("ERROR: recv failed");
            leave_flag = 1;
        }

        bzero(buffer, BUFFER_SZ);
    }

    // Cleanup
    close(cli->sockfd);
    queue_remove(cli->uid);
    free(cli);
    cli_count--;
    pthread_detach(pthread_self());
    
    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage: %s <ip> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *ip = argv[1];
    int port = atoi(argv[2]);
    int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr, cli_addr;
    pthread_t tid;

    // Socket settings
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(port);

    signal(SIGPIPE, SIG_IGN);

    int option = 1;
    setsockopt(listenfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char*)&option, sizeof(option));

    // Bind
    if (bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR: Socket binding failed");
        return EXIT_FAILURE;
    }

    // Listen
    if (listen(listenfd, 10) < 0) {
        perror("ERROR: Socket listening failed");
        return EXIT_FAILURE;
    }

    printf("=== WELCOME TO THE CHATROOM ===\n");
    printf("Server: %s:%d\n\n", ip, port);

    while (1) {
        socklen_t clilen = sizeof(cli_addr);
        connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

        // Check max clients
        if (cli_count + 1 == MAX_CLIENTS) {
            printf("Max clients reached. Rejected connection.\n");
            close(connfd);
            continue;
        }

        // Create client
        client_t *cli = (client_t *)malloc(sizeof(client_t));
        cli->address = cli_addr;
        cli->sockfd = connfd;
        cli->uid = uid++;

        // Add to queue and create thread
        queue_add(cli);
        pthread_create(&tid, NULL, &handle_client, (void*)cli);

        sleep(1);
    }

    return EXIT_SUCCESS;
}

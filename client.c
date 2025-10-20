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

// Global variables
volatile sig_atomic_t flag = 0;
int sockfd = 0;
char name[32];
char color_code[10] = "\033[0m"; // Default color

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

void str_overwrite_stdout() {
    printf("\r> ");
    fflush(stdout);
}

void str_trim_lf (char* arr, int length) {
    int i;
    for (i = 0; i < length; i++) {
        if (arr[i] == '\n') {
            arr[i] = '\0';
            break;
        }
    }
}

void catch_ctrl_c_and_exit(int sig) {
    flag = 1;
}

void send_msg_handler() {
    char message[LENGTH] = {};
    char buffer[LENGTH + 42] = {};

    while(1) {
        str_overwrite_stdout();
        if (fgets(message, LENGTH, stdin) == NULL) {
            break;
        }
        str_trim_lf(message, LENGTH);

        if (strcmp(message, "exit") == 0) {
            break;
        } else if (strlen(message) > 0) {
            // Format: COLOR + NAME + ": " + MESSAGE + RESET
            sprintf(buffer, "%s%s: %s\033[0m\n", color_code, name, message);
            send(sockfd, buffer, strlen(buffer), 0);
        }

        bzero(message, LENGTH);
        bzero(buffer, LENGTH + 42);
    }
    catch_ctrl_c_and_exit(2);
}

void recv_msg_handler() {
    char message[LENGTH] = {};
    while (1) {
        int receive = recv(sockfd, message, LENGTH, 0);
        if (receive > 0) {
            // Save current cursor position
            printf("\033[s");
            // Move to beginning of line and clear it
            printf("\r\033[K");
            // Print the received message
            printf("%s", message);
            // Restore cursor position and re-print the prompt
            printf("\033[u");
            str_overwrite_stdout();
            fflush(stdout);
        } 
        else if (receive == 0) {
            break;
        } 
        memset(message, 0, sizeof(message));
    }
}

void choose_color() {
    printf("\nChoose a color for your name:\n");
    printf("1. %sRed\033[0m\n", colors[0]);
    printf("2. %sGreen\033[0m\n", colors[1]);
    printf("3. %sYellow\033[0m\n", colors[2]);
    printf("4. %sBlue\033[0m\n", colors[3]);
    printf("5. %sMagenta\033[0m\n", colors[4]);
    printf("6. %sCyan\033[0m\n", colors[5]);
    printf("7. %sLight Red\033[0m\n", colors[6]);
    printf("8. %sLight Green\033[0m\n", colors[7]);
    printf("9. %sLight Yellow\033[0m\n", colors[8]);
    printf("10. %sLight Blue\033[0m\n", colors[9]);
    printf("11. %sLight Magenta\033[0m\n", colors[10]);
    printf("12. %sLight Cyan\033[0m\n", colors[11]);
    printf("Enter choice (1-12): ");
    
    char choice[4];
    fgets(choice, 4, stdin);
    int color_choice = atoi(choice);
    
    if (color_choice >= 1 && color_choice <= 12) {
        strcpy(color_code, colors[color_choice - 1]);
    } else {
        printf("Invalid choice. Using default color.\n");
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

    if (strlen(name) > 32 || strlen(name) < 2){
        printf("Name must be less than 30 and more than 2 characters.\n");
        return EXIT_FAILURE;
    }

    choose_color();

    struct sockaddr_in server_addr;

    /* Socket settings */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    // Connect to Server
    int err = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (err == -1) {
        printf("ERROR: connect\n");
        return EXIT_FAILURE;
    }

    // Send only name (color will be sent with each message)
    send(sockfd, name, 32, 0);

    printf("=== WELCOME TO THE CHATROOM ===\n");

    pthread_t send_msg_thread;
    if(pthread_create(&send_msg_thread, NULL, (void *) send_msg_handler, NULL) != 0){
        printf("ERROR: pthread\n");
        return EXIT_FAILURE;
    }

    pthread_t recv_msg_thread;
    if(pthread_create(&recv_msg_thread, NULL, (void *) recv_msg_handler, NULL) != 0){
        printf("ERROR: pthread\n");
        return EXIT_FAILURE;
    }

    while (1){
        if(flag){
            printf("\nBye\n");
            break;
        }
    }

    close(sockfd);
    return EXIT_SUCCESS;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define REQUEST_TYPE 1
#define RESPONSE_TYPE 2
#define PRIMARY_SERVER_MSG_TYPE 3
#define SECONDARY_SERVER_1_MSG_TYPE 4
#define SECONDARY_SERVER_2_MSG_TYPE 5

#define MAX_MSG_SIZE 256
#define MSG_KEY 1234

struct msg_buffer {
    long msg_type;
    char msg_text[MAX_MSG_SIZE];
};

void sendToPrimaryServer(int msg_id, struct msg_buffer message) {
    message.msg_type = PRIMARY_SERVER_MSG_TYPE;
    if(msgsnd(msg_id, &message, sizeof(message.msg_text), 0) == -1) {
        perror("msgsnd");
        exit(EXIT_FAILURE);
    }

    printf("Load Balancer: Message (%s) forwarded to Primary Server\n", message.msg_text);
}

void sendToSecondaryServer(int msg_id, struct msg_buffer message, int secServerId) {
    message.msg_type = secServerId;

    if (msgsnd(msg_id, &message, sizeof(message.msg_text), 0) == -1) {
        perror("msgsnd");
        exit(EXIT_FAILURE);
    }
    int secServerName;
    secServerName = (secServerId==4) ? 1 : 2;
    printf("Load Balancer: Message forwarded to Secondary Server %d\n", secServerName);
}

int main() {
    key_t key;
    int msg_id;
    struct msg_buffer message;

    // Create a unique key for the message queue
    key = ftok("/tmp", MSG_KEY);
    if (key == -1) {
        perror("ftok");
        exit(EXIT_FAILURE);
    }

    // Create a message queue
    msg_id = msgget(key, 0666 | IPC_CREAT);
    if (msg_id == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }

    printf("Load Balancer: Message Queue created with key %d\n", key);

    // Infinite loop to receive messages
    while (1) {
        // Receive the message
        if (msgrcv(msg_id, &message, sizeof(message.msg_text), REQUEST_TYPE, 0) == -1) {
            perror("msgrcv");
            exit(EXIT_FAILURE);
        }

        int op_no, seq_no;
        char filename[10];

        sscanf(message.msg_text, "%d %d %s", &seq_no, &op_no, filename);
        printf("\nOption no: %d\n", op_no);
        printf("\nSeq no: %d\n", seq_no);
        printf("\nFilename: %s\n", filename);

        // Print the received message
        printf("Load Balancer: Received message: %s\n", message.msg_text);

        if(op_no == 1 || op_no == 2) {
            sendToPrimaryServer(msg_id, message);
        } else if(op_no == 3 || op_no == 4) {
            int secServerId = (seq_no % 2 == 1) ? SECONDARY_SERVER_1_MSG_TYPE : SECONDARY_SERVER_2_MSG_TYPE;
            sendToSecondaryServer(msg_id, message, secServerId);
        } else if(strcmp(message.msg_text, "exit") == 0) {
            break;
        } else {
            printf("Wrong request number\n");
        }
    }

    // Close and remove the message queue when done
    if (msgctl(msg_id, IPC_RMID, NULL) == -1) {
        perror("msgctl");
        exit(EXIT_FAILURE);
    }

    printf("Load Balancer: Message Queue destroyed\n");
    printf("Load Balancer: Exiting\n");

    return 0;
}
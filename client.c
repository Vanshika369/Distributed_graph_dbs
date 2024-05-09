#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>

#define REQUEST_TYPE 1
#define RESPONSE_TYPE 2

#define MAX_MSG_SIZE 256
#define MSG_KEY 1234
#define SHM_KEY 5678

struct msg_buffer {
    long msg_type;
    char msg_text[MAX_MSG_SIZE];
};

void* createSharedMemory(int nodes, int adj[][31]) {
    int shmid;
    key_t shmkey = ftok("/tmp", SHM_KEY);

    size_t str_size = snprintf(NULL, 0, "%d", nodes);
    for (int i = 1; i <= nodes; ++i) {
        for (int j = 1; j <= nodes; ++j) {
            str_size += snprintf(NULL, 0, " %d", adj[i][j]);
        }
    }

    // Create share memory segment
    
    shmid = shmget(shmkey, str_size + 1, IPC_CREAT | 0666);    
    if(shmid == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    // Attach shared memory segment

    char* shared_memory = (char*)shmat(shmid, NULL, 0);
    if (shared_memory == (void*)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    // Copy data to shared memory

    sprintf(shared_memory, "%d", nodes);
    for (int i = 1; i <= nodes; ++i) {
        for (int j = 1; j <= nodes; ++j) {
            sprintf(shared_memory + strlen(shared_memory), " %d", adj[i][j]);
        }
    }

    // Detach shared memory segment
    if(shmdt(shared_memory) == -1) {
        perror("shmdt");
        exit(EXIT_FAILURE);
    }

    printf("Client: Shared memory segment created with key %d\n", shmkey);
    return NULL;
}

void* createSharedMemory2(int startNode) {
    int shmid;
    key_t shmkey = ftok("/tmp", SHM_KEY);

    size_t str_size = snprintf(NULL, 0, "%d", startNode);

    shmid = shmget(shmkey, str_size + 1, IPC_CREAT | 0666);    
    if(shmid == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    char* shared_memory = (char*)shmat(shmid, NULL, 0);
    if (shared_memory == (void*)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    sprintf(shared_memory, "%d", startNode);

    if(shmdt(shared_memory) == -1) {
        perror("shmdt");
        exit(EXIT_FAILURE);
    }

    printf("Client: Shared memory segment created with key %d\n", shmkey);
    return NULL;
}

int main() {
    key_t key;
    int msg_id;
    struct msg_buffer message;

    // Create a unique key for the message queue
    key = ftok("/tmp", MSG_KEY);

    // Get the message queue ID
    msg_id = msgget(key, 0666);
    if (msg_id == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }

    printf("Client: Connected to Message Queue with key %d\n", key);

    while (1) {
        int option;
        printf("\nOption Menu:\n");
        printf("1. Add a new graph to the database\n");
        printf("2. Modify an existing graph of the database\n");
        printf("3. Perform DFS on an existing graph in the database\n");
        printf("4. Perform BFS on an existing graph in the database\n");
        printf("5. Destroy mqueue and exit.\n");

        int seq_no, op_no;
        char filename[10];

        printf("\nEnter sequence number: ");
        scanf("%d", &seq_no);

        printf("\nEnter Operation Number: ");
        scanf("%d", &op_no);

        printf("\nEnter Graph File Name: ");
        scanf("%s", filename);

        if(op_no == 5) {
            strcpy(message.msg_text, "exit");
        } else if(op_no == 1 || op_no == 2) {
            int nodes;
            int adj[31][31];
            printf("\nEnter number of nodes of graph: ");
            scanf("%d", &nodes);

            printf("Enter adjacency matrix, each row on a separate line and elements of a single row separated by whitespace characters:\n");
            for(int i=1; i<=nodes; i++) {
                for(int j=1; j<=nodes; j++) {
                    scanf("%d", &adj[i][j]);
                }
            }
            createSharedMemory(nodes,adj);
            snprintf(message.msg_text, MAX_MSG_SIZE, "%d %d %s", seq_no, op_no, filename);
        } else if(op_no == 3 || op_no == 4) {
            int vertex;
            printf("\nEnter starting vertex: ");
            scanf("%d", &vertex);
            createSharedMemory2(vertex);
            snprintf(message.msg_text, MAX_MSG_SIZE, "%d %d %s", seq_no, op_no, filename);
        } else {
            printf("Wrong option chosen\n");
            continue;
        }

        // Set the message type to 1
        message.msg_type = REQUEST_TYPE;

        // Send the message
        if (msgsnd(msg_id, &message, sizeof(message.msg_text), 0) == -1) {
            perror("msgsnd");
            exit(EXIT_FAILURE);
        }

        printf("Client: Message sent to Load Balancer\n");
    }
    return 0;
}

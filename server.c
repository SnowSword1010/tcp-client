#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <limits.h>
#include <pthread.h>
#include "myqueue.h"

#define SERVERPORT 8989
#define BUFFSIZE 4096
#define SOCKETERROR (-1)
#define SERVER_BACKLOG 100
#define THREAD_POOL_SIZE 20

// creating a thread pool
pthread_t thread_pool[THREAD_POOL_SIZE];
// initialising mutex lock
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct sockaddr_in SA_IN; // datatype sockaddr_in is given the name SA_IN
typedef struct sockaddr SA;       // datatype sockaddr is given the name SA
void *handle_connection(void *p_client_socket);
int check(int exp, const char *msg);
void *thread_function(void *arg);

int main(int argc, char **argv)
{
    int server_socket, client_socket, addr_size;
    SA_IN server_addr, client_addr;

    //first off create a bunch of threads to handle future connections
    for (int i = 0; i < THREAD_POOL_SIZE; i++)
    {
        pthread_create(&thread_pool[i], NULL, thread_function, NULL);
    }

    check((server_socket = socket(AF_INET, SOCK_STREAM, 0)), "Failed to create socket");

    //Initialize the address struct
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVERPORT);
    check(bind(server_socket, (SA *)&server_addr, sizeof(server_addr)), "Bind failed");
    check(listen(server_socket, SERVER_BACKLOG), "Listen failed");
    while (true)
    {
        printf("Waiting for connections.....\n");
        //wait for, and eventually accept an incoming connection
        addr_size = sizeof(SA_IN);
        check(client_socket = accept(server_socket, (SA *)&client_addr, (socklen_t *)&addr_size), "acceptfailed");
        printf("connection!\n");

        //do whatever we do with connections

        //put the connection somewhere so that an available thraed can find it

        int *pclient = malloc(sizeof(int));
        *pclient = client_socket;

        //make sure only thread messes with the queue at a time
        pthread_mutex_lock(&mutex);
        enqueue(pclient);
        pthread_mutex_unlock(&mutex);
        // pthread_create(&t,NULL,handle_connection,pclient);
        handle_connection(pclient);
    }
    return 0;
}


int check(int exp, const char *msg)
{
    if (exp == SOCKETERROR)
    {
        perror(msg);
        exit(1);
    }
    return exp;
}

void *thread_function(void *arg)
{
    while (true)
    {
        int *pclient;
        pthread_mutex_lock(&mutex);
        pclient = dequeue();
        pthread_mutex_unlock(&mutex);
        if (pclient != NULL)
        {
            //we have a connection
            handle_connection(pclient);
        }
    }
}

void *handle_connection(void *p_client_socket){
    printf("I'm called");
    int client_socket = *((int *)p_client_socket);
    free(p_client_socket);
    char buffer[BUFFSIZE];
    size_t bytes_read;
    int msgsize = 0;
    char actualpath[1000];

    //read the client's message -- the name of the file to read
    while ((bytes_read = read(client_socket, buffer + msgsize, sizeof(buffer) - msgsize)))
    {
        msgsize += bytes_read;
        if (msgsize > BUFFSIZE - 1 || buffer[msgsize - 1] == '\n')
            break;
    }
    check(bytes_read, "recv error");
    buffer[msgsize - 1] = 0; //null terminate the message and remove the \n
    printf("Request: %s\n", buffer);
    fflush(stdout);

    //Validity check
    if (realpath(buffer, actualpath) == NULL)
    {
        printf("Error (Bad Path): %s\n", buffer);
        close(client_socket);
        return NULL;
    }

    //read file and send its contents to client
    FILE *fp = fopen(actualpath, "r");
    if (fp = NULL)
    {
        printf("ERROR(open): %s\n", buffer);
        close(client_socket);
        return NULL;
    }
    sleep(1); //just do nothing
    //read file contents and send them to client
    //note this is a fine example program, but rather insecure
    //a real program would probably limit the client to certain files
    while ((bytes_read = fread(buffer, 1, BUFFSIZE, fp)) > 0)
    {
        printf("Sending %zu bytes\n", bytes_read);
        write(client_socket, buffer, bytes_read);
    }
    close(client_socket);
    fclose(fp);
    printf("closing connection\n");
    return NULL;
}

/**********************************
tcp_server.c: the source file of the server in tcp transmission
1. Receive file in small data units
3. Send ACK for each data unit received
2. Send final ACK
***********************************/

#include "headsock.h"

#define BACKLOG 10

// receiving and transmitting function
void receiver_and_transmission(int sockfd);

int main(void)
{
    int sockfd, con_fd, bind_status, connection_status; // ?
    struct sockaddr_in client_address;
    struct sockaddr_in host_address;
    int sin_size;

    pid_t pid;

    // create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        printf("error in socket!");
        exit(1);
    }

    host_address.sin_family = AF_INET;
    host_address.sin_port = htons(MYTCP_PORT);
    host_address.sin_addr.s_addr = htonl(INADDR_ANY);
    bzero(&(host_address.sin_zero), 8);

    // bind socket
    bind_status = bind(sockfd, (struct sockaddr *)&host_address, sizeof(struct sockaddr));
    if (bind_status < 0)
    {
        printf("error in binding socket");
        exit(1);
    }
    // start listening
    connection_status = listen(sockfd, BACKLOG);
    if (connection_status < 0)
    {
        printf("Error in listening");
        exit(1);
    }

    while (1)
    {
        printf("waiting for data\n");
        sin_size = sizeof(struct sockaddr_in);
        // accept new packet
        con_fd = accept(sockfd, (struct sockaddr *)&client_address, &sin_size);
        if (con_fd < 0)
        {
            printf("error in accepting new packet\n");
            exit(1);
        }
        // create acception process
        pid = fork();
        if (pid == 0)
        {
            close(sockfd);
            receiver_and_transmission(con_fd);
            close(con_fd);
            exit(0);
        }
        else
        {
            close(con_fd);
        }
    }
    close(sockfd);
    exit(0);
}

void receiver_and_transmission(int sockfd)
{
    char buffer[BUFSIZE];
    FILE *file;
    char recvs[DATALEN];
    struct ack_so ack;
    int end, curr_data_length = 0, send_status;
    long lseek = 0;
    end = 0;

    printf("Receiving data\n");

    // keep receiving data until \0 is received
    while (!end)
    {
        curr_data_length = recv(sockfd, &ack, 2, 0);
        if (curr_data_length == -1)
        {
            printf("send error!");
            exit(1);
        }
        if (recvs[curr_data_length - 1] == '\0')
        {
            end = 1;
            // subtract to get the correct data size
            // since the last one is end char \0
            curr_data_length--;
        }
        memcpy((buffer + lseek), recvs, curr_data_length);
        lseek += curr_data_length;
    }

    ack.num = 1;
    ack.len = 0;
    // send ack to client
    send_status = send(sockfd, &ack, 2, 0);
    if (send_status == -1)
    {
        printf("send ack error!");
        exit(1);
    }

    file = fopen("receivedTCPFile.txt", "wt");
    if (file == NULL)
    {
        printf("File doesn't exist\n");
        exit(0);
    }
    // write data into file
    fwrite(buffer, 1, lseek, file);
    fclose(file);
    printf("A file has been successfully received!\n");
    printf("The total data received is %d bytes\n", (int)lseek);
}
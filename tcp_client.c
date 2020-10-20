/*******************************
tcp_client.c: the source file of the client in tcp transmission 
1. Transfer a large message (from a file)
2. Split into short data units
3. Send using stop-and-wait flow control
5. Check for ACK, handle retransmission with negative ACK
6. Measure message transfer time and throughput for various size of data units
gcc tcp_client.c -o tcp_client
./tcp_client 127.0.0.1
********************************/

#include "headsock.h"

float transmission(FILE *file, int socketfd, long *len);
void time_interval(struct timeval *out, struct timeval *in);

int main(int argc, char **argv)
{
    int socketfd, connection_status;
    float transmission_time, data_rate;
    long len;
    struct sockaddr_in server_address;
    char **pptr;
    struct hostent *host;
    struct in_addr **address;
    FILE *file;

    // check for the no of correct params
    if (argc != 2)
    {
        printf("parameters do not match");
    }

    // get host information
    host = gethostbyname(argv[1]);
    if (host == NULL)
    {
        printf("error when getting host by name");
        exit(0);
    }

    // print host name
    printf("canonical name: %s\n", host->h_name);
    // print host aliases names
    for (pptr = host->h_aliases; *pptr != NULL; pptr++)
    {
        printf("the aliases name is %s\n", *pptr);
    }
    // check for address type
    switch (host->h_addrtype)
    {
    case AF_INET:
        printf("address type: AF_INET\n");
        break;
    default:
        printf("unknown address type\n");
        break;
    }

    address = (struct in_addr **)host->h_addr_list;

    // create socket
    socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketfd < 0)
    {
        printf("error in creating socket");
        exit(1);
    }
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(MYTCP_PORT);
    memcpy(&(server_address.sin_addr.s_addr), *address, sizeof(struct in_addr));
    bzero(&(server_address.sin_zero), 8);

    // connect the socket with the host / test connection
    connection_status = connect(socketfd, (struct sockaddr *)&server_address, sizeof(struct sockaddr));
    if (connection_status != 0)
    {
        printf("connection failed\n");
        close(socketfd);
        exit(1);
    }

    // check for file
    file = fopen("myfile.txt", "r+t");
    if (file == NULL)
    {
        printf("File does not exit\n");
        exit(0);
    }

    // start transmitting
    transmission_time = transmission(file, socketfd, &len);

    // calculate message transfer time and data rates
    data_rate = (len / (float)transmission_time);
    printf("Time(ms) taken: %.3f, Data sent(byte): %d\n", transmission_time, (int)len);
    printf("Data rate: %f (Kbytes/s)\n", data_rate);

    close(socketfd);
    fclose(file);
    exit(0);
}

float transmission(FILE *file, int socketfd, long *len)
{
    char *buffer;
    long file_size, curr_index = 0;
    char sends[DATALEN];
    struct ack_so ack;
    int bytes_sent, bytes_read, bytes_to_send;
    float time_inv = 0.0;
    struct timeval send_time, receive_time;
    // to measure throughput
    long packets_sent = 0;
    float total_bytes_sent = 0;

    // initialise ack value
    ack.num = -1;
    ack.len = 0;

    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    rewind(file); // rewind to beginning of file
    printf("The file length is %d bytes\n", (int)file_size);
    printf("The packet length is %d bytes\n", DATALEN);

    //allocate memory to contain the whole file
    buffer = (char *)malloc(file_size);
    if (buffer == NULL)
    {
        exit(2);
    }

    // copy the file into the buffer -> load file in the buffer
    fread(buffer, 1, file_size, file);

    // append the end byte for checking at the server side
    buffer[file_size] = '\0';

    // get current time or start time
    gettimeofday(&send_time, NULL);

    // keep sending and receiving ACK
    while (curr_index <= file_size)
    {
        // get remaining file size?
        if ((file_size + 1 - curr_index) <= DATALEN)
            bytes_to_send = file_size + 1 - curr_index;
        else
            bytes_to_send = DATALEN;

        memcpy(sends, (buffer + curr_index), bytes_to_send);

        // if incorrect NACK received, resend
        // send the packet and check for sent status
        while (ack.num != 1)
        {
            // sending the packet and check for status
            bytes_sent = send(socketfd, &sends, bytes_to_send, 0);
            packets_sent++;
            total_bytes_sent += bytes_sent;
            if (bytes_sent == -1)
            {
                printf("Packet sending error!\n");
                exit(1);
            }

            // check if the ACK is received
            // recv (for TCP SOCK_STREAM sockets) is equivalent to recvfrom but normally used on a connected socket only
            // recvfrom places the received message into the buffer (for UDP SOCK_DGRAM sockets)
            // Both functions take the socket descriptor s, a pointer to the buffer buf, the size (in bytes) of the buffer len, and a set of flags that control how the functions work.
            bytes_read = recv(socketfd, &ack, 2, 0);
            if (bytes_read == -1)
            {
                printf("error when receiving\n");
                exit(1);
            }

            // check the status of the ack
            if (ack.num == 1 && ack.len == 0)
            {
                curr_index += bytes_to_send;
                printf("Packet received\n");
            }
            else if (ack.num == 2)
            {
                // NACK received - need to retransmit
                printf("Packet sent failure - NACK received. Retransmitting...\n");
            }
            else
            {
                printf("error in transmission\n");
                exit(1);
            }
        }

        // reassign ack to default
        ack.num = -1;
        ack.len = 0;
    }

    // get received time, and calculate time interval
    gettimeofday(&receive_time, NULL);
    *len = curr_index;
    time_interval(&receive_time, &send_time);
    time_inv += (receive_time.tv_sec) * 1000.0 + (receive_time.tv_usec) / 1000.0;

    printf("The total data sent(byte) with retransmission: %d\n", (int)total_bytes_sent);

    return (time_inv);
}

void time_interval(struct timeval *receive_time, struct timeval *send_time)
{
    // check for microseconds differences
    if ((receive_time->tv_usec -= send_time->tv_usec) < 0)
    {
        // subtract 1 second difference to add the equavalent microseconds
        --receive_time->tv_sec;
        receive_time->tv_usec += 1000000;
    }
    // calculate the time interval / differences between start and end
    receive_time->tv_sec -= send_time->tv_sec;
}
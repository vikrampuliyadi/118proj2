#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h> // Include for gettimeofday function

#include "utils.h"

#define INITIAL_WINDOW_SIZE 1
#define MAX_WINDOW_SIZE 10
#define DUPLICATE_ACK_THRESHOLD 3   // Example threshold for duplicate ACKs
#define PACKET_SEND_DELAY_US 10000 // Example delay between sending each packet in microseconds

int main(int argc, char *argv[])
{
    int listen_sockfd, send_sockfd;
    struct sockaddr_in client_addr, server_addr_to, server_addr_from;
    socklen_t addr_size = sizeof(server_addr_to);
    struct timeval tv;
    struct packet pkt;
    struct packet ack_pkt;
    char buffer[PAYLOAD_SIZE];
    unsigned short seq_num = 0;
    unsigned short ack_num = 0;
    char last = 0;
    char ack = 0;

    // read filename from command line argument
    if (argc != 2)
    {
        printf("Usage: ./client <filename>\n");
        return 1;
    }
    char *filename = argv[1];

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0)
    {
        perror("Could not create listen socket");
        return 1;
    }

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0)
    {
        perror("Could not create send socket");
        return 1;
    }

    // Configure the server address structure to which we will send data
    memset(&server_addr_to, 0, sizeof(server_addr_to));
    server_addr_to.sin_family = AF_INET;
    server_addr_to.sin_port = htons(SERVER_PORT_TO);
    server_addr_to.sin_addr.s_addr = inet_addr(SERVER_IP);

    // Configure the client address structure
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(CLIENT_PORT);
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the listen socket to the client address
    if (bind(listen_sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    // Open file for reading
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("Error opening file");
        close(listen_sockfd);
        close(send_sockfd);
        return 1;
    }

    // Initialize variables for AIMD scheme
    int window_size = MAX_WINDOW_SIZE;
    int base = 0;
    int nextseqnum = 0;
    int expected_ack = 0;
    int duplicate_acks = 0;
    int recv_len;
    ssize_t bytes_read;
    int all_window_acks_recv;
    char acks[MAX_WINDOW_SIZE] = {0}; // Track acks for each packet in the window
    long *file_positions = malloc(MAX_WINDOW_SIZE * sizeof(long)); // stores fp positions to resend
    int smallest_unacked = 0;
    int attempts = 0;
    while (1)
    {
        // Send packets up to the current window size
        // only send packets that haven't received acks yet
        while (nextseqnum < base + window_size && !feof(fp))
        {
            file_positions[nextseqnum % MAX_WINDOW_SIZE] = ftell(fp); // Remember the current file position
            // Construct and send packet with sequence number nextseqnum
            bytes_read = fread(buffer, 1, PAYLOAD_SIZE, fp);
            build_packet(&pkt, nextseqnum, 0, feof(fp) ? 1 : 0, 0, bytes_read, buffer);
            sendto(send_sockfd, &pkt, sizeof(struct packet), 0, (struct sockaddr *)&server_addr_to, sizeof(struct sockaddr_in));
            printSend(&pkt, 0);
            nextseqnum++;

            // Introduce delay between sending packets
            usleep(PACKET_SEND_DELAY_US);
        }


        if ((feof(fp) && base == nextseqnum) || attempts > MAX_WINDOW_SIZE) {
            break;
        }
    

        // Set timeout for receiving ACKs
        tv.tv_sec = 0;
        tv.tv_usec = 300000;
        setsockopt(listen_sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
        all_window_acks_recv = 1;

        // Wait for ACKs and handle them
        while (1) {
            // Receive ACKs
            recv_len = recvfrom(listen_sockfd, &ack_pkt, sizeof(struct packet), 0, NULL, NULL);

            if (recv_len < 0) {
                // Handle timeout
                // Resend the packet that timed out and any unacked packets in the window
                for (int i = base; i < nextseqnum; i++) {
                    if (acks[i % MAX_WINDOW_SIZE] > 0) {
                        bytes_read = fread(buffer, 1, PAYLOAD_SIZE, fp);
                        continue; //don't resend packets that were acked
                    }
                    fseek(fp, file_positions[i % MAX_WINDOW_SIZE], SEEK_SET); // Reset file pointer to the correct position
                    bytes_read = fread(buffer, 1, PAYLOAD_SIZE, fp);
                    build_packet(&pkt, i, 0, feof(fp) ? 1 : 0, 0, bytes_read, buffer);
                    sendto(send_sockfd, &pkt, sizeof(struct packet), 0, (struct sockaddr *)&server_addr_to, sizeof(struct sockaddr_in));
                    printSend(&pkt, 1); // Indicate resend
                    attempts++;
                }
                break;
            }
            else {
                // set ack received
                acks[ack_pkt.acknum % MAX_WINDOW_SIZE] = 1;
                for (int i = smallest_unacked; i < smallest_unacked + window_size; i++) {                    
                    if (acks[i % MAX_WINDOW_SIZE] == 0) {
                        smallest_unacked = i;
                        break;
                    }
                    acks[i % MAX_WINDOW_SIZE] = 0;
                }
                base = smallest_unacked;
                attempts = 0;

                // Log received ACK
                printf("Received ACK: %d\n", ack_pkt.acknum);
            }
        }


    }

    free(file_positions);
    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}


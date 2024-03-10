#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "utils.h"

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
    if (bind(listen_sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0)
    {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    // Open file for reading
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL)
    {
        perror("Error opening file");
        close(listen_sockfd);
        close(send_sockfd);
        return 1;
    }

    // TODO: Read from file, and initiate reliable data transfer to the server
    size_t read_bytes;
    int window_base = 0;
    int window_end = WINDOW_SIZE - 1;
    fd_set read_fds;
    int ack_received = 0;

    while (!feof(fp) || window_base <= seq_num)
    {
        while (seq_num <= window_end && (read_bytes = fread(buffer, 1, PAYLOAD_SIZE, fp)) > 0)
        {
            build_packet(&pkt, seq_num, 0, feof(fp) ? 1 : 0, 0, read_bytes, buffer);
            sendto(send_sockfd, &pkt, sizeof(struct packet), 0, (struct sockaddr *)&server_addr_to, sizeof(struct sockaddr_in));
            printSend(&pkt, 0);
            seq_num++;
        }

        int expected_ack = window_base; // Initialize expected_ack to the base of the window

        // Wait for ACK
        FD_ZERO(&read_fds);
        FD_SET(send_sockfd, &read_fds);
        tv.tv_sec = TIMEOUT;
        tv.tv_usec = 0;

        if (select(send_sockfd + 1, &read_fds, NULL, NULL, &tv) > 0)
        {
            recvfrom(send_sockfd, &pkt, sizeof(struct packet), 0, NULL, NULL);
            printRecv(&pkt);

            // Check if received ACK is within the window
            if (pkt.ack == 1 && pkt.acknum >= window_base && pkt.acknum <= window_end)
            {
                if (pkt.acknum >= expected_ack)
                {
                    // Update window base to the next expected sequence number
                    window_base = pkt.acknum + 1;
                    window_end = window_base + WINDOW_SIZE - 1;
                    expected_ack = window_base; // Update expected_ack to the new base
                }
            }
        }
        else if (!ack_received)
        {
            // Timeout, resend window
            fseek(fp, window_base * PAYLOAD_SIZE, SEEK_SET);
            seq_num = window_base;
        }
    }

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}

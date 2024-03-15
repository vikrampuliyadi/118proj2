#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"

#define MAX_WINDOW_SIZE 100

int main()
{
    int listen_sockfd, send_sockfd;
    struct sockaddr_in server_addr, client_addr_from, client_addr_to;
    struct packet buffer;
    socklen_t addr_size = sizeof(client_addr_from);
    int expected_seq_num = 0;
    int recv_len;
    struct packet ack_pkt;
    int received[MAX_WINDOW_SIZE] = {0};
    struct packet packetsBuffer[MAX_WINDOW_SIZE]; // Buffer for out-of-order packets
    int last_recvd = 0;

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0)
    {
        perror("Could not create send socket");
        return 1;
    }

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0)
    {
        perror("Could not create listen socket");
        return 1;
    }

    // Configure the server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the listen socket to the server address
    if (bind(listen_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    // Configure the client address structure to which we will send ACKs
    memset(&client_addr_to, 0, sizeof(client_addr_to));
    client_addr_to.sin_family = AF_INET;
    client_addr_to.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    client_addr_to.sin_port = htons(CLIENT_PORT_TO);

    // Open the target file for writing (always write to output.txt)
    FILE *fp = fopen("output.txt", "wb");

    // TODO: Receive file from the client and save it as output.txt

    // if null file path
    if (fp == NULL)
    {
        perror("Error opening output file");
        close(listen_sockfd);
        close(send_sockfd);
        return 1;
    }

    int all_recv;
    while (1)
    {
        struct packet pkt, ack_pkt;
        all_recv = 1;
        // Receive packets
        if (recvfrom(listen_sockfd, &pkt, sizeof(struct packet), 0, (struct sockaddr *)&client_addr_from, &addr_size) < 0)
        {
            perror("Error receiving packet");
            continue;
        }
        printRecv(&pkt);

        // Send ACK regardless of expected packet num
        build_packet(&ack_pkt, 0, pkt.seqnum, 0, 1, 0, NULL); 
        sendto(send_sockfd, &ack_pkt, sizeof(struct packet), 0, (struct sockaddr *)&client_addr_to, addr_size);
        printSend(&ack_pkt, 0); 

        if (received[pkt.seqnum % MAX_WINDOW_SIZE] == 0 && (pkt.seqnum >= expected_seq_num && pkt.seqnum < expected_seq_num + MAX_WINDOW_SIZE)) {
            received[pkt.seqnum % MAX_WINDOW_SIZE] = 1;
            if (pkt.last) {
                last_recvd = 1;
            }
            packetsBuffer[pkt.seqnum % MAX_WINDOW_SIZE] = pkt;

            while (received[expected_seq_num % MAX_WINDOW_SIZE] == 1) {
                // Write payload to file and advance next_expected_seq
                fwrite(packetsBuffer[expected_seq_num % MAX_WINDOW_SIZE].payload, 1, packetsBuffer[expected_seq_num % MAX_WINDOW_SIZE].length, fp);
                received[expected_seq_num % MAX_WINDOW_SIZE] = 0; 
                expected_seq_num++; 
            }
        }

        // Break the loop and finish executionn when the last packet is received
        for (int i = 0; i < MAX_WINDOW_SIZE; i++) {
            if (received[i] != 0) {
                all_recv = 0;
            }
        }
        if (last_recvd && all_recv)
        {
            break;
        }
    }

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}

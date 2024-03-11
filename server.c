#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"

int main()
{
    int listen_sockfd, send_sockfd;
    struct sockaddr_in server_addr, client_addr_from, client_addr_to;
    struct packet buffer;
    socklen_t addr_size = sizeof(client_addr_from);
    int expected_seq_num = 0;
    int recv_len;
    struct packet ack_pkt;

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

    while (1)
    {
        struct packet pkt, ack_pkt;
        // Receive packets
        if (recvfrom(listen_sockfd, &pkt, sizeof(struct packet), 0, (struct sockaddr *)&client_addr_from, &addr_size) < 0)
        {
            perror("Error receiving packet");
            continue; // Or break, depending on how you want to handle receive errors
        }

        // Check if the received packet is the expected sequence number
        if (pkt.seqnum == expected_seq_num)
        {
            printf("current:, %d , expected: %d \n", pkt.seqnum, expected_seq_num);

            // Write payload to file
            fwrite(pkt.payload, 1, pkt.length, fp);
            expected_seq_num++;

            // Send ACK for received packet
            build_packet(&ack_pkt, 0, pkt.seqnum, 0, 1, 0, NULL); // Correctly initialize the ACK packet
            sendto(send_sockfd, &ack_pkt, sizeof(struct packet), 0, (struct sockaddr *)&client_addr_to, addr_size);
            printSend(&ack_pkt, 0); // Assuming printRecv function logs received packets
        } 
        else if (pkt.seqnum < expected_seq_num) {
            build_packet(&ack_pkt, 0, pkt.seqnum, 0, 1, 0, NULL);
            sendto(send_sockfd, &ack_pkt, sizeof(struct packet), 0, (struct sockaddr *)&client_addr_to, addr_size);
            printSend(&ack_pkt, 0); // Assuming printRecv function logs received packets
        }
        else
        {
            
            printf("not equal: current:, %d , expected: %d \n", pkt.seqnum, expected_seq_num);
            // If a duplicate packet is received, resend the ACK for the last correctly received packet
            build_packet(&ack_pkt, 0, expected_seq_num - 1, 0, 1, 0, NULL);
            sendto(send_sockfd, &ack_pkt, sizeof(struct packet), 0, (struct sockaddr *)&client_addr_to, addr_size);
            printSend(&ack_pkt, 0); // Assuming printRecv function logs received packets
        }

        // Break the loop and finish execution when the last packet is received
        // if (pkt.last)
        // {
        //     break;
        // }
    }

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}

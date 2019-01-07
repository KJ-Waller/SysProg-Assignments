#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <arpa/inet.h>

int fd;
static int size = 64;

void createSocket() {
    fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (fd < 0) {
        fprintf(stderr, "ERROR: Socket could not be acquired");
        exit(1);
    }
}

// void listen() {
//     char msg[size];
//     int err = 0;
//     socklen_t fromlen;
//     struct sockaddr_in from;

//     err = recvfrom(fd, msg, size, 0, (struct sockaddr*) &from, &fromlen);

//     if (err < 0) {
//         fprintf(stderr, "Error: Failed to receive message from client");
//     }

//     // TODO: read message and send it back
// }

int main(int argc, char ** argv) {
    createSocket();
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int err = bind(fd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));
    if (err < 0) {
        fprintf(stderr, "ERROR: Could not bind socket");
    }

    int alternator = 0;

    while (1) {
        char msg[64];
        int err = 0;
        socklen_t fromlen;
        struct sockaddr_in from;
        fromlen = sizeof(struct sockaddr_in);

        err = recvfrom(fd, msg, 64, 0, (struct sockaddr*) &from, &fromlen);
        if (err < 0) {
            fprintf(stderr, "ERROR: Something went wrong when receiving message from client");
        }

        printf("Received %d bytes from host %s port %d: %s", err, inet_ntoa(from.sin_addr), ntohs(from.sin_port), msg);

        if (alternator == 0) {
            int errsend;

            // TODO: Check if casting is necessary
            errsend = sendto(fd, msg, 64, 0, (struct sockaddr*) &from, sizeof(struct sockaddr_in));

            if (errsend < 0) {
                fprintf(stderr, "ERROR: Message was not sent");
                exit(1);
            }
            alternator++;
        } else {
            alternator--;
        }

    }

    return 0;
}
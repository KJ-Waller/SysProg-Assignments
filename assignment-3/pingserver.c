#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>

static int PORT = 1234;
static int SIZE = 64;

int createSocket() {
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (fd < 0) {
        fprintf(stderr, "ERROR: Socket could not be acquired\n");
        exit(1);
    }
    return fd;
}

void bindSocket(int fd) {
    struct sockaddr_in addr;
    int err;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    err = bind(fd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));
    if (err < 0) {
        fprintf(stderr, "ERROR: Could not bind socket\n");
        exit(1);
    }
}

int main(int argc, char ** argv) {
    int fd, errrcv, errsend;
    char msg[64];
    struct sockaddr_in addr, from;
    socklen_t fromlen;
    
    // Create socket and bind
    fd = createSocket();
    bindSocket(fd);

    // Keep receiving messages and replying back with the message received.
    while (1) {
        errrcv = 0;
        fromlen = sizeof(struct sockaddr_in);

        errrcv = recvfrom(fd, msg, SIZE, 0, (struct sockaddr*) &from, &fromlen);
        if (errrcv < 0) {
            fprintf(stderr, "ERROR: Something went wrong when receiving message from client");
            exit(1);
        }

        errsend = sendto(fd, msg, SIZE, 0, (struct sockaddr*) &from, sizeof(struct sockaddr_in));

        if (errsend < 0) {
            fprintf(stderr, "ERROR: Message was not sent");
            exit(1);
        }

    }

    return 0;
}
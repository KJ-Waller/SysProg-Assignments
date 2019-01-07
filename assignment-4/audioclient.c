#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include "audio.h"

#define BUFSIZE 1024
#define SIZE 64

static int PORT_SERVER = 1234;

// Basic errorhandler that takes error code and message
void errorHandler(int error, char * msg) {
    if (error < 0) {
        fprintf(stderr, "ERROR: %s\n", msg);
        exit(1);
    }
}

// Takes in a name as string, and returns corresponding in_addr if address is found
struct in_addr * resolveHostName(const char *name) {
    struct hostent *resolv;
    struct in_addr *addrp;

    resolv = gethostbyname(name);
    if (resolv == NULL) {
        fprintf(stderr, "Address not found for %s\n", name);
        exit(1);
    }

    addrp = (struct in_addr*) resolv->h_addr_list[0];
    return addrp;
}

// Creates socket and returns file descriptor
int createSocket() {
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    errorHandler(fd, "Socket could not be acquired");
    
    return fd;
}

// Sends a string to a specified sockaddr_in destination through the provided socket file descriptor
void sendString(int fd, char * msg, struct sockaddr_in dest) {
    int errsend;

    errsend = sendto(fd, msg, SIZE, 0, (struct sockaddr*) &dest, sizeof(struct sockaddr_in));
    errorHandler(errsend, "Message was not sent");
}

// Receives the audio header information, and stores it in the provided pointers
void recvAudioHeader(int fd, int * sample_rate, int * sample_size, int * channels) {
    int header[3], nb, err;
    struct sockaddr_in from;
    fd_set read_set;
    socklen_t fromlen;
    struct timeval timeout = {
        tv_sec: 6,
        tv_usec: 0
    };

    // Wait no more than 6 seconds for the header. 
    FD_ZERO(&read_set);
    FD_SET(fd, &read_set);
    nb = select(fd+1, &read_set, NULL, NULL, &timeout);
    errorHandler(nb, "Something went wrong when waiting for audio header packet");

    if (nb == 0) {
        errorHandler(-1, "No message received from server. Maybe it's not started yet?");
    }

    if (FD_ISSET(fd, &read_set)) {
        err = recvfrom(fd, &header, 3*sizeof(int), 0, (struct sockaddr*) &from, &fromlen);
        errorHandler(err, "Something went wrong when receiving header");

        // Send header acknowledgement
        sendString(fd, "ACK", from);
    }

    *sample_rate = header[0];
    *sample_size = header[1];
    *channels = header[2];
}

// Takes a string host and pointer to a sockaddr_in, and changes it with server info
void setServerSockaddr(struct sockaddr_in * from, char * host) {
    struct in_addr *addrp;
    addrp = resolveHostName(host);

    from->sin_family = AF_INET;
    from->sin_port = htons(PORT_SERVER);
    from->sin_addr.s_addr = addrp->s_addr;
}

// Closes connection to audio and socket file descriptors
void closeConnection(int aud_fd, int sock_fd) {
    int err;
    err = close(sock_fd);
    errorHandler(err, "Something went wrong when closing socket file descriptor");
    err = close(aud_fd);
    errorHandler(err, "Something went wrong when closing audio device file descriptor");
}

int main(int argc, char ** argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: audioclient <hostname> <filename>\n");
        return 1;
    }

    // Initialise variables for main
    int sock_fd, aud_fd, sock_p, sample_rate, sample_size, channels, nb, err;
    struct sockaddr_in from;
    char buffer[BUFSIZE];
    fd_set read_set;
    struct timeval timeout = {
        tv_sec: 6,
        tv_usec: 0
    };

    // DNS (resolving hostname)
    setServerSockaddr(&from, argv[1]);

    // Create socket
    sock_fd = createSocket();

    // Send filename to server
    sendString(sock_fd, argv[2], from);

    recvAudioHeader(sock_fd, &sample_rate, &sample_size, &channels);

    // Get audio device file descriptor
    aud_fd = aud_writeinit(sample_rate, sample_size, channels);
    errorHandler(aud_fd, "Couldn't connect to audio device\n");

    FD_ZERO(&read_set);

    // Listen for incoming packets, and write buffer immediately to audio file descriptor
    do {
        // Wait max 6 seconds to receive next packet, otherwise exit
        timeout.tv_sec = 6;
        timeout.tv_usec = 0;
        FD_SET(sock_fd, &read_set);
        nb = select(sock_fd+1, &read_set, NULL, NULL, &timeout);
        errorHandler(nb, "Something went wrong with select function timeout");
        if (nb == 0) {
            err = printf("Haven't received a packet from the server for more than 6 seconds.\nClosing connection\n");
            errorHandler(err, "Something went wrong printing to screen");
            closeConnection(aud_fd, sock_fd);
            return 0;
        }

        // When we receive something, either check if it is a FIN, or send ACK back to server & play audio buffer
        if (FD_ISSET(sock_fd, &read_set)) {
            sock_p = read(sock_fd, buffer, BUFSIZE);
            errorHandler(sock_p, "Something went wrong when receiving packet from server");

            if (strcmp(buffer, "FIN") == 0) {
                err = printf("EOF\n");
                errorHandler(err, "Something went wrong when printing to stdout");
                closeConnection(aud_fd, sock_fd);
                return 0;
            }

            sendString(sock_fd, "ACK", from);

            err = write(aud_fd, buffer, BUFSIZE);
            errorHandler(err, "Something went wrong writing to the audio device");
        }

    } while(sock_p > 0);

    // Close socket and audio file descriptors when finished
    closeConnection(aud_fd, sock_fd);

    return 0;
}
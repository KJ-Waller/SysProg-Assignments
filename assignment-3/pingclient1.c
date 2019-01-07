#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <netdb.h>
#include <unistd.h>

static int PORT_SERVER = 1234;
static int SIZE = 64;

int createSocket() {
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (fd < 0) {
        fprintf(stderr, "ERROR: Socket could not be acquired\n");
        exit(1);
    }
    return fd;
}

void sendMessage(int fd, in_addr_t s_addr) {
    int errsend;
    char msg[64] = "Ping test message";

    struct sockaddr_in dest;

    dest.sin_family = AF_INET;
    dest.sin_port = htons(PORT_SERVER);
    dest.sin_addr.s_addr = s_addr;

    errsend = sendto(fd, msg, SIZE, 0, (struct sockaddr*) &dest, sizeof(struct sockaddr_in));

    if (errsend < 0) {
        fprintf(stderr, "ERROR: Message was not sent\n");
        exit(1);
    }
}



void recvMessage(int fd) {
    char resmsg[64];
    int errrecv;
    socklen_t fromlen;
    struct sockaddr_in from;
    fromlen = sizeof(struct sockaddr_in);

    errrecv = recvfrom(fd, resmsg, SIZE, 0, (struct sockaddr*) &from, &fromlen);

    if (errrecv < 0) {
        fprintf(stderr, "ERROR: Something went wrong when receiving the message\n");
        exit(1);
    }
}

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

struct timespec getCurrentTime() {
    struct timespec starttime;
    int starterr = clock_gettime(CLOCK_REALTIME, &starttime);
    if (starterr < 0) {
        fprintf(stderr, "ERROR: Something went wrong when reading the system clock\n");
        exit(1);
    }
    return starttime;
}

double calcRTT(struct timespec starttime, struct timespec endtime) {
    // Subtract starttime from endtime to get RTT
    long sec = endtime.tv_sec - starttime.tv_sec;
    long ns = endtime.tv_nsec - starttime.tv_nsec;

    return ((double) sec) + ((double) ns*1E-9);
}

int main(int argc, char ** argv) {
    // Check if there is an argument given
    if (argc != 2) {
        fprintf(stderr, "Usage: pingclient1 <domain-name-to-ping>\n");
        exit(1);
    }

    int fd, err;
    struct in_addr *addrp;
    double rtt;
    struct timespec starttime, endtime;
    
    // DNS (resolving hostname)
    addrp = resolveHostName(argv[1]);

    // Create socket
    fd = createSocket();

    // Send message to the server & get start time for transmission
    sendMessage(fd, addrp->s_addr);
    starttime = getCurrentTime();

    // Wait to receive message from server, then get the receiving/end time
    recvMessage(fd);
    endtime = getCurrentTime();

    // Cast sec and nsec to a single double for printing
    rtt = calcRTT(starttime, endtime);

    err = printf("The RTT was: %f seconds.\n", rtt);
    if (err < 0) {
        exit(1);
    }

    err = close(fd);
    if (err < 0) {
        fprintf(stderr, "ERROR: Socket couldn't be closed");
        exit(1);
    }

    return 0;
}
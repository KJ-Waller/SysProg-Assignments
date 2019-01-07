#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/select.h>
#include <netdb.h>
#include <errno.h>

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

void sendMessage(int fd, int message, in_addr_t s_addr) {
    int errsend, errprint;
    struct sockaddr_in dest;
    char msg[64];

    // Convert package number to string
    errprint = snprintf(msg, 10, "%d", message);
    if (errprint < 0) {
        fprintf(stderr, "ERROR: Something went wrong with converting message to string\n");
        exit(1);
    }

    dest.sin_family = AF_INET;
    dest.sin_port = htons(PORT_SERVER);
    dest.sin_addr.s_addr = s_addr;

    errsend = sendto(fd, msg, SIZE, 0, (struct sockaddr*) &dest, sizeof(struct sockaddr_in));

    if (errsend < 0) {
        fprintf(stderr, "ERROR: Message was not sent\n");
        exit(1);
    }
}

int recvMessage(int fd) {
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
    errno = 0;
    int packetnum = 0;
    packetnum = strtol(resmsg, NULL, 10);

    if (errno != 0) {
        fprintf(stderr, "ERROR: Something went wrong when reading the packet number");
        exit(1);
    }

    return packetnum;
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
        return 1;
    }

    int fd, nb, packetnumber, nsleeperrno, printerr, count = 1;
    double rtt;
    struct in_addr *addrp;
    struct timeval timeout;
    struct timespec extratimeout, starttime, endtime;
    fd_set read_set;

    // DNS (resolving hostname)
    addrp = resolveHostName(argv[1]);

    // Create socket
    fd = createSocket();

    FD_ZERO(&read_set);

    // Send a packet every second to the server
    while(1) {
        // Set our socket to be monitored when it's ready to be read
        FD_SET(fd, &read_set);
        // Set our timeout to 1 second
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        // Send message with the count as the content to check for packet order, and get start time for transmission
        sendMessage(fd, count, addrp->s_addr);
        starttime = getCurrentTime();

        
        // Block program until message arrives
        nb = select(fd+1, &read_set, NULL, NULL, &timeout);

        if (nb < 0) {
            fprintf(stderr, "ERROR: Something went wrong with the timeout\n");
            return 1;
        }
        // Packet did not arrive within 1 second
        if (nb == 0) {
            printerr = printf("Packet %u: lost.\n", count);
            if (printerr < 0) {
                fprintf(stderr, "ERROR: Something went wrong when printing to stdout\n");
                return 1;
            }
        }
        // A packet arrived
        if (FD_ISSET(fd, &read_set)) {
            // Get the content of the packet as an int
            packetnumber = recvMessage(fd);
            endtime = getCurrentTime();
            rtt = calcRTT(starttime, endtime);

            // If packet number is different from expected packet number
            if (packetnumber != count) {
                printerr = printf("Packet %u: wrong counter! Received %u instead of %u.\n", count, packetnumber, count);
            }
            // If packet number is the same as expected packet number
            if (packetnumber == count) {
                printerr = printf("Packet %u: %f seconds.\n", count, rtt);
            }

            // In case either print statements fail
            if (printerr < 0) {
                fprintf(stderr, "Something went wrong when printing to stdout\n");
                return 1;
            }
        }
        // Increment packet counter for the next message
        count++;

        // Sleep until the full second has passed
        // timeout value is adjusted to how much time the program DID NOT sleep
        // timeval struct uses microseconds instead of nanoseconds, hence *1E3
        extratimeout.tv_sec = timeout.tv_sec;
        extratimeout.tv_nsec = (timeout.tv_usec)* 1E3;

        // Use nanosleep from time.h to sleep for the remainder of the second left
        nsleeperrno = nanosleep(&extratimeout, NULL);

        if (nsleeperrno < 0) {
            fprintf(stderr, "ERROR: Something went wrong whith nanosleep\n");
            return 1;
        }
    }


    return 0;
}
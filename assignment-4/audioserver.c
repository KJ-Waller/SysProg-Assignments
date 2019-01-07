#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include "audio.h"

#define BUFSIZE 1024
#define SIZE 64

static int PORT = 1234;

// Basic errorhandler that takes error code and message
void errorHandler(int error, char * msg) {
    if (error < 0) {
        fprintf(stderr, "ERROR: %s\n", msg);
        exit(1);
    }
}

// Creates socket and returns file descriptor
int createSocket() {
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    errorHandler(fd, "Socket could not be acquired");
    return fd;
}

// Binds a given file descriptor to a socket
void bindSocket(int fd) {
    struct sockaddr_in addr;
    int err;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    err = bind(fd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));
    errorHandler(err, "Could not bind socket");
}

// Gets a timespec of the current time
struct timespec getCurrentTime() {
    struct timespec starttime;
    int starterr = clock_gettime(CLOCK_REALTIME, &starttime);
    if (starterr < 0) {
        fprintf(stderr, "ERROR: Something went wrong when reading the system clock\n");
        exit(1);
    }
    return starttime;
}

// Gets the time difference between two timespecs in a double, for use with timeouts
double getTimediff(struct timespec starttime, struct timespec endtime) {
    // Subtract starttime from endtime to get RTT
    long sec = endtime.tv_sec - starttime.tv_sec;
    long ns = endtime.tv_nsec - starttime.tv_nsec;

    return ((double) sec) + ((double) ns*1E-9);
}

// Sends a string to a specified sockaddr_in destination through the provided socket file descriptor
void sendString(int fd, char * msg, struct sockaddr_in dest) {
    int errsend;

    errsend = sendto(fd, msg, SIZE, 0, (struct sockaddr*) &dest, sizeof(struct sockaddr_in));
    errorHandler(errsend, "Message was not sent");
}

// Sends packet with specialised format containing audio header information to client
int sendAudioHeader(int client_fd, struct sockaddr_in client, int sample_rate, int sample_size, int channels, struct timeval timeout) {
    int err, nb, header[3] = {sample_rate, sample_size, channels};
    double timediff;
    struct timespec starttime;
    char ack[SIZE];
    fd_set read_set;
    
    // Get starttime for timeout
    starttime = getCurrentTime();
    FD_ZERO(&read_set);
    // Send header information out for 6 seconds max, until an acknowledgement arrives
    do {
        err = sendto(client_fd, header, sizeof(int[3]), 0, (struct sockaddr*) &client, sizeof(struct sockaddr_in));
        errorHandler(err, "Something went wrong sending header to client");

        FD_SET(client_fd, &read_set);
        nb = select(client_fd+1, &read_set, NULL, NULL, &timeout);
        errorHandler(nb, "Something went wrong when waiting for audio header packet");

        // Receive and confirm acknowledgement content
        if (FD_ISSET(client_fd, &read_set)) {
            err = read(client_fd, ack, SIZE);
            errorHandler(err, "Something went wrong when receiving ack");
            
            // Go back to listen for requests if we don't receive an acknowledgement back from the client
            if (strcmp(ack, "ACK") != 0) {
                err = printf("Received something other than an acknowledgement after sending audio header\nClosing Connection");
                errorHandler(err, "Something went wrong when printing to stdout");
                return 1;
            }
        }
        
        // If we've been sending audio headers out for more than 6 seconds, go back to listen for other requests
        timediff = getTimediff(starttime, getCurrentTime());
        if (timediff > 6) {
            err = printf("Waited for more than 6 seconds for audio header acknowledgement to arrive from client.\nClosing Connection");
            errorHandler(err, "Something went wrong when printing to stdout");
            return 1;
        }
    } while (nb == 0);

    return 0;
}

// Streams a given filename to a given client
// First reads and sends audio header information to client in first packet
// Then iterates through the audio file, sending each chunk as it goes,
//      Every time packet is sent, it waits for an acknowledgement before sending next packet
// Finally, sending FIN packet when done transmitting audio file
void streamAudio(int client_fd, char * filename, struct sockaddr_in client){
    int wav_fd, wav_pointer, err, sample_rate, sample_size, channels, byterate, nb;
    double pack_per_sec, timediff;
    struct timespec extratimeout, starttime;
    char buffer[BUFSIZE], ack[SIZE];
    fd_set read_set;
    struct timeval timeout;

    // Initialise the read of the audio file requested by client
    wav_fd = aud_readinit(filename, &sample_rate, &sample_size, &channels);
    errorHandler(wav_fd, "Couldn't read audio file");

    // Calculate bitrate necessary for transmission
    byterate = sample_rate * (sample_size/8) * channels;
    
    // Calculate packets/second, based on our BUFSIZE, and set timeout accordingly
    pack_per_sec = (double)byterate/BUFSIZE;
    // *0.94 to decrease sleeping time a tiny bit to account for network delay
    timeout.tv_usec = (((double) 1/pack_per_sec)*1E6)*0.94; 

    // Send audio file header information to client
    err = sendAudioHeader(client_fd, client, sample_rate, sample_size, channels, timeout);
    // Return in case of timeout
    if (err) {return;}
    
    // Start reading the audio file, and send packets
    FD_ZERO(&read_set);
    do {
        // Read audio chunk
        wav_pointer = read(wav_fd, buffer, BUFSIZE);
        errorHandler(wav_pointer, "Something went wrong when reading the audio file");
        
        // Get starttime for transmission of each packet
        starttime = getCurrentTime();
        do {
            // Set timeout to sleep for amount of time to maintain bitrate
            FD_SET(client_fd, &read_set);
            timeout.tv_sec = 0;
            timeout.tv_usec = (((double) 1/pack_per_sec)*1E6);

            // Send audio packet
            err = sendto(client_fd, buffer, wav_pointer, 0, (struct sockaddr*) &client, sizeof(struct sockaddr_in));
            errorHandler(err, "Something went wrong sending packet to client");
            
            // Wait until acknowledgement arrives
            nb = select(client_fd+1, &read_set, NULL, NULL, &timeout);
            errorHandler(nb, "Something went wrong when waiting for acknowledgement");

            // If time it took since trying to send this particular audio packet is more than 6 seconds,
            // stop transmitting audio files and return to listening to requests
            timediff = getTimediff(starttime, getCurrentTime());
            if (timediff > 6 && nb == 0) {
                err = printf("Waited for more than 6 seconds for acknowledgement. Closing connection\n");
                errorHandler(err, "Something went wrong printing to stdout");
                return;
            }
        } while (nb == 0);

        // When ack has arrived, read it
        if (FD_ISSET(client_fd, &read_set)) {
            err = read(client_fd, ack, SIZE);
            errorHandler(err, "Something went wrong when receiving ack");
        }

        // timeout was set to the amount of time between between each packet transmission to maintain bitrate
        // The select function changes timeout to the amount of time not slept
        // Here we convert timeout of struct timeval to to struct timespec to use with nanosleep, to sleep 
        // the extra amount of time to maintain bitrate
        extratimeout.tv_sec = timeout.tv_sec;
        extratimeout.tv_nsec = (timeout.tv_usec)*1E3;
        err = nanosleep(&extratimeout, NULL);
        errorHandler(err, "Something went wrong when pacing transmission");

    } while (wav_pointer > 0);

    // When audio file has finished transmitting, send FIN to client
    sendString(client_fd, "FIN", client);

    // Close file descriptor for wav file transmitted
    err = close(wav_fd);
    errorHandler(err, "Something went wrong when closing wav file descriptor");

    // Audio has been streamed successfully
    err = printf("Audio has been streamed\n");
    errorHandler(err, "Something went wrong when printing to stdout");
}

int main(int argc, char ** argv) {
    int fd, err;
    char filename[SIZE];
    struct sockaddr_in from;
    socklen_t fromlen;
    
    fromlen = sizeof(struct sockaddr_in);

    // Create socket and bind
    fd = createSocket();
    bindSocket(fd);

    // Listen for requests
    while (1) {
        err = printf("Listening for requests on port %d\n", PORT);

        // Read filename
        err = recvfrom(fd, filename, SIZE, 0, (struct sockaddr*) &from, &fromlen);
        errorHandler(err, "Something went wrong when receiving message from client");
        
        if (strcmp(filename, "ACK") != 0) { // Don't do anything when rogue ACKs come in
            err = printf("Received request for filename: %s\n", filename);
            errorHandler(err, "Something went wrong when printing to stdout");

            // Stream audio to client
            streamAudio(fd, filename, from);
        }
        
    }

    // Close server socket
    err = close(fd);
    errorHandler(err, "Something went wrong when closing network file descriptor");
    return 0;
}
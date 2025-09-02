#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>
#include <time.h>

#define ADDRESS "10.0.3.1"
#define PACKET_SIZE 1400

#define NSEC_PER_SEC 1000000000L
#define timerdiff(a,b) (((a)->tv_sec - (b)->tv_sec) * NSEC_PER_SEC + (((a)->tv_nsec - (b)->tv_nsec)))

volatile sig_atomic_t stop = 0;

int wait_nanoseconds(long nsec)
{
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    while(1){
        clock_gettime(CLOCK_MONOTONIC, &now);
        if(timerdiff(&now, &start) >= nsec){
            break;
        }
    }
}

void intHandler(int dummy) {
    stop = 1;
}

int main(int argc, char *argv[]) {
    signal(SIGINT, intHandler);
    if (argc != 4) {
        fprintf(stderr, "Usage: ./send.o num_targets wait_time_nanos report_interval\n");
        return 1;
    }

    int num_targets = atoi(argv[1]);
    if (num_targets < 1) {
        fprintf(stderr, "num_targets must be greater than 0\n");
        return 1;
    }
    printf("num_targets: %d\n", num_targets);

    int wait_time_nanos = atoi(argv[2]);
    if(wait_time_nanos < 0) {
        fprintf(stderr, "wait_time_nanos can not be smaller than 0\n");
        return 1;
    }
    printf("wait_time_nanos: %d\n", wait_time_nanos);

    int report_interval = atoi(argv[3]);
    if(report_interval<0){
        fprintf(stderr, "report_interval can not be smaller than 0");
    }
    printf("report_interval: %d\n", report_interval);

    struct sockaddr_in target_addrs[num_targets];
    uint64_t counters[num_targets];
    uint8_t packet[PACKET_SIZE];

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket failed");
        return 1;
    }

    for (int i = 0; i < num_targets; i++) {
        memset(&target_addrs[i], 0, sizeof(target_addrs[i]));
        target_addrs[i].sin_family = AF_INET;
        target_addrs[i].sin_port = htons(i + 1);
        inet_pton(AF_INET, ADDRESS, &target_addrs[i].sin_addr);
        counters[i] = 1;
    }

    socklen_t addrlen = sizeof(struct sockaddr_in);
    memset(packet, 0, PACKET_SIZE);

    struct timespec tstart={0,0}, tend={0,0};
    clock_gettime(CLOCK_MONOTONIC, &tstart);
    uint64_t packets_send = 0;

    while (!stop) {
        if (report_interval!=0 && (packets_send%report_interval == 0) && packets_send!=0) {
            clock_gettime(CLOCK_MONOTONIC, &tend);
            double timediff = timerdiff(&tend, &tstart);
            double speed = ((double)(PACKET_SIZE * report_interval)) / (timediff / NSEC_PER_SEC);           
            printf("packets_send: %ld | Speed [Byte/s]: %f\n", packets_send, speed);
            clock_gettime(CLOCK_MONOTONIC, &tstart);
        }
        packets_send++;
        for (int i = 0; i < num_targets; i++) {
            // Place the counter in the last 8 bytes of the packet
            memcpy(&packet[PACKET_SIZE - sizeof(counters[i])], &counters[i], sizeof(counters[i]));

            if (sendto(sock, packet, PACKET_SIZE, 0, (struct sockaddr*) &target_addrs[i], addrlen) < 0) {
                perror("sendto failed");
                return -1;
            }
            counters[i]++;
        }
        if (wait_time_nanos>0) {
            wait_nanoseconds(wait_time_nanos);
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &tend);
    double timediff = timerdiff(&tend, &tstart);
    double speed = ((double)(PACKET_SIZE * packets_send)) / (timediff / NSEC_PER_SEC);
    printf("packets_send: %ld | Speed [Byte/s]: %f\n", packets_send, speed);
    close(sock);
    return 0;
}

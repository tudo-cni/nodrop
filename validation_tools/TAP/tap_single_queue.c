#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#include <inttypes.h>
#include <signal.h>

#define RECEIVED_PACKET_SIZE 1450 //Should be PACKET_SIZE + 32 + 18

#define NSEC_PER_SEC 1000000000L
#define timerdiff(a,b) (((a)->tv_sec - (b)->tv_sec) * NSEC_PER_SEC + (((a)->tv_nsec - (b)->tv_nsec)))

struct timespec tstart={0,0}, tend={0,0};
uint64_t last_counter_received = 0;
volatile sig_atomic_t stop = 0;
uint64_t packets_received = 0;
uint64_t packets_lost = 0;
uint64_t run_time_ns;
int wait_time_nanos;
int fd;

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

int tap_open(char* devname)
{
    struct ifreq ifr;
    int fd, err;

    if ((fd = open("/dev/net/tun", O_RDWR)) == -1) {
        exit(1);
    }
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;

    strncpy(ifr.ifr_name, devname, IFNAMSIZ);

    if ((err = ioctl(fd, TUNSETIFF, (void*)&ifr)) == -1) {
        close(fd);
        exit(1);
    }
    return fd;
}

void intHandler(int dummy) {
    stop = 1;
}

int main(int argc, char* argv[])
{
    if(argc != 3) {
        fprintf(stderr, "Usage: ./tap_single_queue.o <wait_time_nanos> <run_time_ns>\n");
        return 1;
    }

    wait_time_nanos = atoi(argv[1]);
    if(wait_time_nanos<0){
        fprintf(stderr, "wait_time_nanos can not be smaller than 0");
        return 1;
    }
    printf("wait_time_nanos: %d\n", wait_time_nanos);

    run_time_ns = atoll(argv[2]);
    if(run_time_ns<0){
        fprintf(stderr, "run_time_ns can not be smaller than 0");
        return 1;
    }
    printf("run_time_ns: %ld\n", run_time_ns);

    printf("RECEIVED_PACKET_SIZE: %d\n", RECEIVED_PACKET_SIZE);

    int nbytes;
    char buf[1600];

    printf("Trying to open tap0...\n");
    fd = tap_open("tap0");
    signal(SIGINT, intHandler);
    printf("Device tap0 opened\n");

    while (!stop) {
        if(packets_received==1){
            clock_gettime(CLOCK_MONOTONIC, &tstart);
        }
        if(packets_received>0){
            clock_gettime(CLOCK_MONOTONIC, &tend);
            double timediff = timerdiff(&tend, &tstart);
            if(timediff > run_time_ns){
                break;
            }
        }

        nbytes = read(fd, buf, sizeof(buf));
        if (nbytes >= 800) { // Minimum Ethernet frame size
            packets_received++;
            // Extract counter from the last 8 bytes of the frame
            int i;
            uint64_t received_counter = 0;
            for (i = 7; i >= 0; --i) {
                received_counter <<= 8;
                uint8_t temp = (uint8_t) buf[nbytes-8+i];
                received_counter |= (uint64_t) temp;
            }
            if (received_counter != last_counter_received+1 && last_counter_received != 0) {
                packets_lost += received_counter - last_counter_received - 1;
            }
            last_counter_received = received_counter;
            if(wait_time_nanos!=0) {
                wait_nanoseconds((long)wait_time_nanos);
            }
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &tend);
    double timediff = timerdiff(&tend, &tstart);
    double speed = ((double)(RECEIVED_PACKET_SIZE * packets_received)) / (timediff / NSEC_PER_SEC);
    printf("packets_received: %ld | packets_lost: %ld | Speed [Byte/s]: %f\n", packets_received, packets_lost, speed);
    close(fd);
    return 0;
}

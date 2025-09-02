#define _GNU_SOURCE
#include <pthread.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <inttypes.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#define IFF_BACKPRESSURE 0x0080
#define RECEIVED_PACKET_SIZE 1432 //Should be the senders PACKET_SIZE + 32

#define NSEC_PER_SEC 1000000000L
#define timerdiff(a,b) (((a)->tv_sec - (b)->tv_sec) * NSEC_PER_SEC + (((a)->tv_nsec - (b)->tv_nsec)))

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int wait_time_nanos, num_targets;
volatile sig_atomic_t stop = 0;
pthread_t threads[256];
uint64_t run_time_ns;
int thread_args[256];
int fds[256];

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

int tun_alloc_mq(char *dev, int queues, int *fds)
{
    struct ifreq ifr;
    int fd, err, i;

    if (!dev){
        return -1;
    }
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_MULTI_QUEUE;

    strncpy(ifr.ifr_name, dev, IFNAMSIZ);
    for (i = 0; i < queues; i++) {
        if ((fd = open("/dev/net/tun", O_RDWR)) < 0) {
            goto err;
        }
        if (err = ioctl(fd, TUNSETIFF, (void *)&ifr)) {
            close(fd);
            goto err;
        }
        fds[i] = fd;
    }
    return 0;
err:
    for (--i; i >= 0; i--)
        close(fds[i]);
    return err;
}

void* worker_thread(void* arg) {
    int thread_number = *(int*)arg;
    int fd = fds[thread_number];
    char buf[1600];
    int nbytes;
    uint64_t last_counter_received = 0;
    uint64_t packets_received = 0;
    uint64_t packets_lost = 0;
    struct timespec tstart={0,0}, tend={0,0};
    clock_gettime(CLOCK_MONOTONIC, &tstart);

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
        if (nbytes == RECEIVED_PACKET_SIZE) {
            packets_received++;
            int i;
            uint64_t received_counter = 0;
            for( i = 7; i >= 0; --i )
            {
                received_counter <<= 8;
                uint8_t temp = (uint8_t) buf[nbytes-8+i];
                received_counter |= (uint64_t) temp;
            }
            if(received_counter != last_counter_received+1) {
                packets_lost+=received_counter-last_counter_received-1;
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
    printf("thread_number: %d | packets_received: %ld | packets_lost: %ld | Speed [Byte/s]: %f\n", thread_number, packets_received, packets_lost, speed);
    return NULL;
}

void intHandler(int dummy) {
    stop = 1;
}

int main(int argc, char* argv[])
{   
    if(argc != 4) {
        fprintf(stderr, "Usage: ./tun_multi_queue.o <num_targets> <wait_time_nanos> <run_time_ns>\n");
        return 1;
    }

    num_targets = atoi(argv[1]);
    if ((num_targets < 1) || (num_targets > 256)) {
        fprintf(stderr, "Number of targets must be greater than 0 and smaller or equal 256\n");
        return 1;
    }
    printf("num_targets: %d\n", num_targets);

    wait_time_nanos = atoi(argv[2]);
    if(wait_time_nanos<0){
        fprintf(stderr, "wait_time_nanos can not be smaller than 0");
        return 1;
    }
    printf("wait_time_nanos: %d\n", wait_time_nanos);

    run_time_ns = atoll(argv[3]);
    if(run_time_ns<0){
        fprintf(stderr, "run_time_ns can not be smaller than 0");
        return 1;
    }
    printf("run_time_ns: %ld\n", run_time_ns);

    int init = tun_alloc_mq("tun0", num_targets, fds);
    if (init) {
        printf("Error while init tun0: %d\n", init);
        return init;
    }
    signal(SIGINT, intHandler);
    printf("Device tun0 opened\n");

    for (int i = 0; i < num_targets; ++i) {
        thread_args[i] = i;
        pthread_create(&threads[i], NULL, worker_thread, &thread_args[i]);
    }

    for (int i = 0; i < num_targets; ++i) {
        pthread_join(threads[i], NULL);
    }
    for (int i = 0; i < num_targets; ++i) {
        close(fds[i]);
    }
    return 0;
}

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <time.h>
#include <signal.h>

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
    if (argc != 5) {
        fprintf(stderr, "Usage: ./send_tap.o <interface> num_targets wait_time_nanos report_interval\n");
        return 1;
    }

    char *ifname = argv[1];
    int num_targets = atoi(argv[2]);
    int wait_time_nanos = atoi(argv[3]);
    int report_interval = atoi(argv[4]);

    if (num_targets < 1) {
        fprintf(stderr, "num_targets must be greater than 0\n");
        return 1;
    }
    printf("interface: %s\n", ifname);
    printf("num_targets: %d\n", num_targets);
    printf("wait_time_nanos: %d\n", wait_time_nanos);
    printf("report_interval: %d\n", report_interval);

    int sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    struct ifreq if_idx;
    memset(&if_idx, 0, sizeof(struct ifreq));
    strncpy(if_idx.ifr_name, ifname, IFNAMSIZ-1);
    if (ioctl(sockfd, SIOCGIFINDEX, &if_idx) < 0) {
        perror("SIOCGIFINDEX");
        return 1;
    }

    struct sockaddr_ll socket_address;
    memset(&socket_address, 0, sizeof(struct sockaddr_ll));
    socket_address.sll_ifindex = if_idx.ifr_ifindex;
    socket_address.sll_halen = ETH_ALEN;
    memset(socket_address.sll_addr, 0xff, ETH_ALEN); // broadcast

    uint64_t counters[num_targets];
    for (int i = 0; i < num_targets; i++) counters[i] = 1;

    unsigned char frame[PACKET_SIZE];
    memset(frame, 0, PACKET_SIZE);
    // Set destination MAC (first 6 bytes)
    memset(frame, 0xff, 6);
    // Set source MAC (next 6 bytes)
    memset(frame+6, 0x11, 6);
    // Set EtherType (next 2 bytes, 0x0800 for IPv4)
    frame[12] = 0x08;
    frame[13] = 0x00;

    // IPv4 header (20 bytes)
    struct iphdr {
        unsigned char ihl:4, version:4;
        unsigned char tos;
        unsigned short tot_len;
        unsigned short id;
        unsigned short frag_off;
        unsigned char ttl;
        unsigned char protocol;
        unsigned short check;
        unsigned int saddr;
        unsigned int daddr;
    } __attribute__((packed));

    // UDP header (8 bytes)
    struct udphdr {
        unsigned short source;
        unsigned short dest;
        unsigned short len;
        unsigned short check;
    } __attribute__((packed));

    // Pointers to headers in frame
    struct iphdr *ip = (struct iphdr *)(frame + 14);
    struct udphdr *udp = (struct udphdr *)(frame + 14 + 20);
    unsigned char *payload = frame + 14 + 20 + 8;
    int payload_len = PACKET_SIZE - 14 - 20 - 8;

    // Fill IP header
    ip->version = 4;
    ip->ihl = 5;
    ip->tos = 0;
    ip->tot_len = htons(20 + 8 + payload_len);
    ip->id = htons(0);
    ip->frag_off = 0;
    ip->ttl = 64;
    ip->protocol = 17; // UDP
    ip->check = 0; // Kernel will not check this for TAP
    ip->saddr = htonl(0xc0a80001); // 192.168.0.1
    ip->daddr = htonl(0x0a000301); // 10.0.3.1

    // Fill UDP header (dest port will be set in loop)
    udp->source = htons(12345);
    udp->len = htons(8 + payload_len);
    udp->check = 0;

    // Fill payload with zeros (or any data)
    memset(payload, 0, payload_len);

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
            // Set UDP dest port for this target (must match tc filter)
            udp->dest = htons(i + 1); // tc filter expects dport 1,2,3,...
            // Place the counter in the last 8 bytes of the payload
            memcpy(payload + payload_len - sizeof(counters[i]), &counters[i], sizeof(counters[i]));
            // sendto may return ENOBUFS, therefore try until it does not fail.
            while (sendto(sockfd, frame, PACKET_SIZE, 0, (struct sockaddr*)&socket_address, sizeof(socket_address)) < 0);
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
    close(sockfd);
    return 0;
}

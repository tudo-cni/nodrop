sudo ip tuntap add mode tun dev tun0
sudo ip addr add 10.0.3.2/24 dev tun0
sudo ip link set dev tun0 up
# A default qdisc like fq_codel could drop packets. We DONT want that!
# A pfifo of 20000 packets showed to be sufficient. Use 25000 to be safe.
sudo tc qdisc replace dev tun0 root pfifo limit 25000 #noqueue

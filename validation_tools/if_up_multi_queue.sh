sudo ip tuntap add dev tun0 mode tun multi_queue
sudo ip addr add 10.0.3.0/24 dev tun0
sudo ip link set dev tun0 up
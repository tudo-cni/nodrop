sudo ip tuntap add mode tap dev tap0
sudo ip addr add 10.0.3.2/24 dev tap0
sudo ip link set dev tap0 up
# A default qdisc like fq_codel could drop packets. We DONT want that!
# A pfifo of 20000 packets showed to be sufficient. Use 25000 to be safe.
sudo tc qdisc replace dev tap0 root pfifo limit 25000 #noqueue

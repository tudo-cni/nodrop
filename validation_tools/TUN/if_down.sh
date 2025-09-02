sudo tc qdisc replace dev tun0 root noqueue
sudo ip link delete tun0
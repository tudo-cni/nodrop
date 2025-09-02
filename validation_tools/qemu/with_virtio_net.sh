sudo qemu-system-x86_64 -hda debian.qcow2   -netdev tap,id=mynet0,ifname=tap0,script=no,downscript=no,vhost=on   -device virtio-net-pci,netdev=mynet0 -m 1024 -enable-kvm

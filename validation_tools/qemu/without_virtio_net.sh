sudo qemu-system-x86_64 -m 1024 -hda debian.qcow2   -netdev tap,id=mynet0,ifname=tap0,script=no,downscript=no   -device e1000,netdev=mynet0 -enable-kvm

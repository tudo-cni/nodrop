# The NODROP Patch

## Disclaimer

This repository is currently under construction. More detailed information will follow shortly. Please expect changes to the README and file structure.
We are also further working on a DKMS Module to load the patch dynamically.

## Publications using the NODROP Patch

- T. Gebauer, S. Schippers, C. Wietfeld, "The NODROP Patch: Hardening Secure Networking for Real-time Teleoperation by Preventing Packet Drops in the Linux TUN Driver," in IEEE 102nd Vehicular Technology Conference (VTC-Fall), Chengdu, China, October 2025. (Accepted for Presentation)

If you use this code or results in your publications, please cite our work as mentioned in Citation. Also, if you do not find your work in this list, please open a merge request.

## Detailed Changes

Please visit [Visual NODROP Diff](https://cni.tu-dortmund.de) (coming soon.) for a detailed side-by-side diff of the patch.


## Usage

Currently the patch has to be applied manually to your distributions kernel-tree. Please use git to apply the patch series.

## TUN Validation Tools

Our proposed validation tools can be used to verify the packet drop behavior of the default TUN driver for single and multi queue TUN interfaces.

With the NODROP patch the tool should show zero packet drops.
 

<details>
  <summary>Compile the validation tools</summary>

```
gcc -o send.o send.c && gcc -o tun_single_queue.o tun_single_queue.c && gcc -o tun_multi_queue.o tun_multi_queue.c
```

</details>

<details>
  <summary>Run single queue validation</summary>

Setup the interface:

```
sudo sh if_up_single_queue.sh
```

Run the TUN client:

```
Usage: ./tun_single_queue.o backpressure_flag wait_time_nanos report_interval
```

Typical usage: Backpressure enabled, limited speed due to wait_time_nanos and report not too often:

```
sudo ./tun_single_queue.o 1 100000 10000
```

Optional: Set the TUN queue size:

```
sudo ip link set dev tun0 txqueuelen 1
```

Feed the TUN client with packets using a different terminal window:

```
Usage: ./send.o num_targets wait_time_nanos report_interval
```

Here we only have one target and want sending as fast as possible:

```
./send.o 1 0 10000
```

Shut the interface down again:

```
sudo ip link delete tun0
```

</details>

<details>
  <summary>Run multi queue validation</summary>

Setup the interface:

```
sudo sh if_down.sh
sudo sh if_up_multi_queue.sh
```

Run the TUN client:

```
Usage: ./tun_multi_queue.o num_targets wait_time_nanos run_time_ns
```

Typical usage: 4 queues, limited speed due to wait_time_nanos and 10s runtime

```
sudo ./tun_multi_queue.o 1 100000 10000000000
```

Optional: Set the TUN queue size:

```
sudo ip link set dev tun0 txqueuelen 1
```

Setup the filtering for the multi queue using a different terminal window:

```
sudo sh qdiscs_and_filter_multi_queue.sh
```

Feed the TUN client with packets:

```
Usage: ./send.o num_targets wait_time_nanos report_interval
```

Here we only have 4 targets and want sending as fast as possible:

```
./send.o 4 0 10000
```

</details>

## TAP Validation

Description coming soon. Benchmarks for theoretical tests and for VM setup with TAP or TAP+vhost_net are in the respective directories already.

## Acknowledgements

This work has been funded by the German Federal Ministry of Research, Technology and Space (BMFTR) via the 6GEM research hub under funding reference 16KISK038 and is further supported by the project DRZ (Establishment of the German Rescue Robotics Center) under funding reference 13N16476.

## Citation

If you use this code or results in your paper, please cite our work as:

```
@InProceedings{gebauer2025,
 Author = {Tim Gebauer and Simon Schippers and Christian Wietfeld},
 Title = {{The NODROP Patch: Hardening Secure Networking for Real-time Teleoperation by Preventing Packet Drops in the Linux TUN Driver}},
 Booktitle = {IEEE 102nd Vehicular Technology Conference (VTC-Fall)},
 Address = {Chengdu, China},
 Month = {oct},
 Year = {2025},
 Keywords = {Rescue Robotics; Multi-Link; 5G},
 Project = {6GEM, DRZ},
}
```

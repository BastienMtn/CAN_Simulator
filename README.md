To compile the simulator :

Driver version: 
```gcc main.c opel_omega_2001.c 64bit/libLinuxCAN_API.a -lm `pkg-config --cflags --libs gtk4` -lpthread -o simulator```

SocketCAN version:
```gcc main.c opel_omega_2001.c socketCAN.c -lm `pkg-config --cflags --libs gtk4` -lpthread -DSOCKET -o simulator```

To run the simulator 

```./simulator```

To use the SocketCAN version:
1. Install can-utils : ``sudo apt install can-utils``
2. (Optional) Install cantools to decode the data in real-time (use pipx if possible): ```pip install cantools```
3. Open a new terminal B
4. In terminal B, run:
   1. VCAN:
      ````bash
      sudo modprobe vcan
      sudo modprobe can
      sudo ip link add dev vcan0 type vcan
      sudo ip link set up vcan0
      echo Virtual CAN Bus has been opened!
      ````
   2. SLCAN (for speed options, [see here](https://elinux.org/Bringing_CAN_interface_up#SLCAN_based_Interfaces)):
      ```bash
      sudo modprobe vcan
      sudo modprobe slcan
      sudo modprobe can
      sudo slcand -o -c -s5 -S3000000 /dev/ttyUSB0 slcan0
      ip link set can0 txqueuelen 1000
      sudo ip link set slcan0 up
      echo Serial CAN Bus has been opened!
      ```
5. Verify that the interface is up: `ip a`
6. In terminal B, run : `candump vcan0` (Add `| cantools decode opel_omega_2001.dbc` for decoding and choose the right interface!)
7. Modify the interface in the code at the line 296 (between `slcan0` and `vcan0`)
8. In terminal A, compile and run the simulator
To compile the simulator :

```gcc main.c opel_omega_2001.c 64bit/libLinuxCAN_API.a -lm `pkg-config --cflags --libs gtk4` -lpthread -o simulator```

To run the simulator 

```./simulator```

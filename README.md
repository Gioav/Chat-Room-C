# Chat-Room-C
## Server
Compile the server:
```bash
gcc server.c -o server -lpthread
```
Execute the server:
> To host the server on all the network interfaces, use `0.0.0.0` as IP.
```bash
./server <ip_addr> <port>
```
## Client
Compile the client:
```bash
gcc client.c -o client -lpthread 
```
Execute the client: 
```bash
./client <ip_addr> <port>
```

# Filesystem for the DC assignment
The filesystem we have sued for this assignment looks like the below:  
```
vbhadra@vbhadra-DQ77MK:~/chatroom$ tree
.
├── client
│   ├── ClientMain.cpp
│   ├── DME.cpp
│   ├── DME.hpp
│   └── Makefile
├── common
│   ├── Logger.cpp
│   ├── Logger.hpp
│   ├── NetUtils.cpp
│   └── NetUtils.hpp
├── create_structure.sh
├── Makefile
├── README.md
└── server
    ├── Makefile
    └── ServerMain.cpp

4 directories, 13 files
```

---

# Chatroom Distributed System – Setup and Test Guide

## Overview

This guide explains how to deploy and test the Distributed Chatroom Application using three AWS EC2 instances — one server and two clients.
The project is written in **C++**, uses the **Ricart–Agrawala Distributed Mutual Exclusion (DME)** algorithm, and allows multiple users to exchange messages safely through a shared file managed by the server.

Everything is automated through simple shell scripts. Just follow these steps exactly.

---

## 1. Create 3 EC2 Instances

Launch three **Ubuntu 24.04 LTS (t3.micro)** instances in the same AWS region (for example, `eu-west-2`).
Name them:

* `chatroom-server`
* `chatroom-client1`
* `chatroom-client2`  

Here is a screenshot from the three instances that I created for this purpose:

<img width="1572" height="130" alt="image" src="https://github.com/user-attachments/assets/aca6a727-4576-4de2-b0e0-4f4405620a53" />

All three must be in the same **VPC** and **subnet** so they can communicate with each other.

---

## 2. Create Security Group

1. Go to your **AWS EC2 Dashboard**.

2. In the left sidebar, click **Security Groups**.

3. Click **Create security group**.

   * Name: `chatroom-sg`
   * Description: *Chatroom project security group*

4. In **Inbound rules**, add the following:

| Type       | Port | Source    | Purpose                         |
| ---------- | ---- | --------- | ------------------------------- |
| SSH        | 22   | 0.0.0.0/0 | Allows SSH access               |
| Custom TCP | 7000 | 0.0.0.0/0 | Chatroom view and post commands |
| Custom TCP | 8001 | 0.0.0.0/0 | DME messages                    |
| Custom TCP | 8002 | 0.0.0.0/0 | DME coordination                |

You don’t need to edit outbound rules — AWS allows all outbound traffic by default.
Here is a screenshot from the security group that I created for this:  
<img width="1591" height="574" alt="image" src="https://github.com/user-attachments/assets/12e5cfd0-392f-4301-9d6d-9a2c51d1b771" />

After creating the group:

* Go to **Instances** in EC2 dashboard.
* Select each instance (server and clients).
* Choose **Actions → Security → Change security groups**.
* Tick **chatroom-sg** and click **Save**.

Or alternatively, you can choose an exisitng security group at the time of EC2 instance creation as well.  

---

## 3. Connect to Each Instance

Open three terminals — one for each node.

Note:  
I had created this /home/vbhadra/Downloads/CUDA-Assignment-Key-Pair.pem key-pair in the past and resued it for this project. 
You can create a new one or use any existing you might already have.  

Example:

```bash
ssh -i <your-key.pem> ubuntu@<public-ip>
```

Keep all three terminals open:

**Server**

```bash
ssh -i /home/vbhadra/Downloads/CUDA-Assignment-Key-Pair.pem ubuntu@18.170.221.136
```

**Client 1**

```bash
ssh -i /home/vbhadra/Downloads/CUDA-Assignment-Key-Pair.pem ubuntu@35.179.154.246
```

**Client 2**

```bash
ssh -i /home/vbhadra/Downloads/CUDA-Assignment-Key-Pair.pem ubuntu@18.171.207.58
```
---

## 4. Clone the Repository and Set Up Environment

Run on each node (Server, Client1, Client2):

```bash
git clone https://github.com/vivekbhadra/chatroom.git
```  
This will create a new folde in your present directory called chatroom.  
Go inside chatroom and run the following script:  
```
cd chatroom
./setup.sh
```
This script will:

* Update your package list
* Install **make**, **build-essential**, and **tree**
* Prepare the environment for compilation

When complete, you’ll see:

```
Setup complete — You can now run:
cd chatroom
make clean && make
```

---

## 5. Compile the Code

Inside the `chatroom` directory on each instance:

```bash
make clean && make
```

Executables will appear in the `bin/` directory.

**Note:**   
**You have to clone the repository and compile the code in each of the three EC2 instances.**

---

## 6. Start the Server

On the server terminal:

```bash
./start_server.sh
```

Example output:

```
ubuntu@ip-172-31-23-9:~/chatroom$ ./start_server.sh 
Executing: ./bin/server --bind 0.0.0.0:7000 --file ./chat.txt
[2025-10-26 16:09:05] [SERVER] Starting on 0.0.0.0:7000 using file: ./chat.txt
[2025-10-26 16:09:05] [NET] TcpListen() called with hostPort=0.0.0.0:7000
[2025-10-26 16:09:05] [NET] SplitHostPort(): host=0.0.0.0, port=7000
[2025-10-26 16:09:05] [NET] Creating socket: family=2, socktype=1, protocol=6
[2025-10-26 16:09:05] [NET] Attempting bind() and listen() on socket fd=3
[2025-10-26 16:09:05] [NET] TcpListen(): Successfully bound and listening on fd=3
[2025-10-26 16:09:05] [SERVER] Listening for connections...
```

Leave this running.

---

## 7. Start the Clients

### Client 1

```bash
./start_client1.sh
```

Example:

```
ubuntu@ip-172-31-22-222:~/chatroom$ ./start_client1.sh 
Executing: ./bin/client --user Lucy --self-id 1 --peer-id 2 --listen 0.0.0.0:8001 --peer 172.31.27.84:8002 --server 172.31.23.9:7000
[2025-10-26 16:09:27] [NET] TcpListen() called with hostPort=0.0.0.0:8001
[2025-10-26 16:09:27] [NET] SplitHostPort(): host=0.0.0.0, port=8001
[2025-10-26 16:09:27] [NET] Creating socket: family=2, socktype=1, protocol=6
[2025-10-26 16:09:27] [NET] Attempting bind() and listen() on socket fd=3
[2025-10-26 16:09:27] [NET] TcpListen(): Successfully bound and listening on fd=3
[2025-10-26 16:09:27] [NET] Attempting connect()
[2025-10-26 16:09:27] [CLIENT] Peer not ready, retrying in 2s... (attempt 1)
[2025-10-26 16:09:29] [NET] Attempting connect()
[2025-10-26 16:09:29] [CLIENT] Peer not ready, retrying in 2s... (attempt 2)
[2025-10-26 16:09:31] [NET] Attempting connect()
[2025-10-26 16:09:31] [CLIENT] Peer not ready, retrying in 2s... (attempt 3)
```

### Client 2

```bash
./start_client2.sh
```

Example:

```
Executing: ./bin/client --user "Joel" --self-id 2 --peer-id 1 --listen 0.0.0.0:8002 --peer 172.31.22.222:8001 --server 172.31.23.9:7000
[2025-10-26 16:09:58] [NET] TcpListen() called with hostPort=0.0.0.0:8002
[2025-10-26 16:09:58] [NET] SplitHostPort(): host=0.0.0.0, port=8002
[2025-10-26 16:09:58] [NET] Creating socket: family=2, socktype=1, protocol=6
[2025-10-26 16:09:58] [NET] Attempting bind() and listen() on socket fd=3
[2025-10-26 16:09:58] [NET] TcpListen(): Successfully bound and listening on fd=3
[2025-10-26 16:09:58] [NET] Attempting connect()
[2025-10-26 16:09:58] [NET] TcpConnect(): successfully connected
[2025-10-26 16:09:58] [CLIENT] Connected to peer 172.31.22.222:8001 after 0 attempts.
[2025-10-26 16:09:58] [CLIENT] Chat Room — DC Assignment II
[2025-10-26 16:09:58] [CLIENT] User: Joel (self=2, peer=1)
[2025-10-26 16:09:58] [CLIENT] Commands: view | post "text" | quit
> 
```

---

## 8. Test the Application

### From Client 1

**View messages**

```
> view
[2025-10-26 16:10:36] [NET] TcpConnectHostPort() input: 172.31.23.9:7000
[2025-10-26 16:10:36] [NET] SplitHostPort(): host=172.31.23.9, port=7000
[2025-10-26 16:10:36] [NET] Attempting connect()
[2025-10-26 16:10:36] [NET] TcpConnect(): successfully connected
[2025-10-26 16:10:36] [NET][SEND] VIEW

[2025-10-26 16:10:36] [NET] SendLine(): sent 5 bytes, result=0
[2025-10-26 16:10:36] [CLIENT] 26 Oct 03:14 PM Joel: "HELLO"
[2025-10-26 16:10:36] [CLIENT] 
[2025-10-26 16:10:36] [CLIENT] 26 Oct 04:08 PM Joel: "Hi there"
```

Displays the current chat log from the server.

**Post a message**

```
> post "Hello from Client 1 - testing DME"
```

Expected log:

```
> post "Hello from Client 1 – testing DME"
[2025-10-26 16:11:15] [NET][SEND] REQUEST 1 1

[2025-10-26 16:11:15] [DME][RA] Sent message: REQUEST 1 1

[2025-10-26 16:11:15] [DME][RA] REQUEST sent to peer ID: 2 request ID:1
[2025-10-26 16:11:15] [CLIENT 1] peer->me: REPLY 2
[2025-10-26 16:11:15] [DME] Message Received : REPLY 2

[2025-10-26 16:11:15] [DME] Extracted Type: REPLY
[2025-10-26 16:11:15] [DME][RA] Received REPLY (permission granted) from peer 2
[2025-10-26 16:11:15] [DME][RA] ENTER critical section (permission received)
[2025-10-26 16:11:15] [NET] TcpConnectHostPort() input: 172.31.23.9:7000
[2025-10-26 16:11:15] [NET] SplitHostPort(): host=172.31.23.9, port=7000
[2025-10-26 16:11:15] [NET] Attempting connect()
[2025-10-26 16:11:15] [NET] TcpConnect(): successfully connected
[2025-10-26 16:11:15] [NET][SEND] POST 26 Oct 04:11 PM Lucy: "Hello from Client 1 – testing DME"

[2025-10-26 16:11:15] [NET] SendLine(): sent 65 bytes, result=0
[2025-10-26 16:11:15] [CLIENT] (posted)
```

### From Client 2

**View messages**

```
> view
```

You should now see:

```
> view
[2025-10-26 16:12:08] [NET] TcpConnectHostPort() input: 172.31.23.9:7000
[2025-10-26 16:12:08] [NET] SplitHostPort(): host=172.31.23.9, port=7000
[2025-10-26 16:12:08] [NET] Attempting connect()
[2025-10-26 16:12:08] [NET] TcpConnect(): successfully connected
[2025-10-26 16:12:08] [NET][SEND] VIEW

[2025-10-26 16:12:08] [NET] SendLine(): sent 5 bytes, result=0
[2025-10-26 16:12:08] [CLIENT] 26 Oct 03:14 PM Joel: "HELLO"
[2025-10-26 16:12:08] [CLIENT] 
[2025-10-26 16:12:08] [CLIENT] 26 Oct 04:08 PM Joel: "Hi there"
[2025-10-26 16:12:08] [CLIENT] 
[2025-10-26 16:12:08] [CLIENT] 26 Oct 04:11 PM Lucy: "Hello from Client 1 – testing DME"
[2025-10-26 16:12:08] [CLIENT] 
```

---

## 9. Check Server Logs

On the server:

```
./start_server.sh
```

You’ll observe all `VIEW` and `POST` requests, file updates, and connection logs.
```
ubuntu@ip-172-31-23-9:~/chatroom$ ./start_server.sh 
Executing: ./bin/server --bind 0.0.0.0:7000 --file ./chat.txt
[2025-10-26 16:09:05] [SERVER] Starting on 0.0.0.0:7000 using file: ./chat.txt
[2025-10-26 16:09:05] [NET] TcpListen() called with hostPort=0.0.0.0:7000
[2025-10-26 16:09:05] [NET] SplitHostPort(): host=0.0.0.0, port=7000
[2025-10-26 16:09:05] [NET] Creating socket: family=2, socktype=1, protocol=6
[2025-10-26 16:09:05] [NET] Attempting bind() and listen() on socket fd=3
[2025-10-26 16:09:05] [NET] TcpListen(): Successfully bound and listening on fd=3
[2025-10-26 16:09:05] [SERVER] Listening for connections...
[2025-10-26 16:10:36] [SERVER] Received line: "VIEW
"
[2025-10-26 16:10:36] [SERVER] VIEW request served. File size: 65 bytes
[2025-10-26 16:10:36] [SERVER] Connection closed
[2025-10-26 16:11:15] [SERVER] Received line: "POST 26 Oct 04:11 PM Lucy: "Hello from Client 1 – testing DME"
"
[2025-10-26 16:11:15] [SERVER] POST appended: 26 Oct 04:11 PM Lucy: "Hello from Client 1 – testing DME"

[2025-10-26 16:11:15] [SERVER] Connection closed
[2025-10-26 16:12:08] [SERVER] Received line: "VIEW
"
[2025-10-26 16:12:08] [SERVER] VIEW request served. File size: 126 bytes
[2025-10-26 16:12:08] [SERVER] Connection closed
```

At the end, both messages appear timestamped and ordered correctly in the shared file.

---

---

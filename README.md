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
cd chatroom
./setup.sh
```

The script will:

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

---

## 6. Start the Server

On the server terminal:

```bash
./start_server.sh
```

Example output:

```
Executing: ./bin/server --bind 0.0.0.0:7000 --file ./chat.txt
[2025-10-26 14:44:08] [SERVER] Starting on 0.0.0.0:7000 using file: ./chat.txt
[SERVER] Listening for connections...
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
Executing: ./bin/client --user Lucy --self-id 1 --peer-id 2 --listen 0.0.0.0:8001 --peer 172.31.27.84:8002 --server 172.31.23.9:7000
[CLIENT] Peer not ready, retrying...
```

### Client 2

```bash
./start_client2.sh
```

Example:

```
Executing: ./bin/client --user "Joel" --self-id 2 --peer-id 1 --listen 0.0.0.0:8002 --peer 172.31.22.222:8001 --server 172.31.23.9:7000
[CLIENT] Connected to peer 172.31.22.222:8001
[CLIENT] Chat Room -- DC Assignment II
[CLIENT] Commands: view | post "text" | quit
>
```

---

## 8. Test the Application

### From Client 1

**View messages**

```
> view
```

Displays the current chat log from the server.

**Post a message**

```
> post "Hello from Client 1 - testing DME"
```

Expected log:

```
[DME][RA] REQUEST sent to peer ID: 2
[DME][RA] ENTER critical section
[NET][SEND] POST 26 Oct 04:11 PM Lucy: "Hello from Client 1 - testing DME"
```

### From Client 2

**View messages**

```
> view
```

You should now see:

```
26 Oct 04:11 PM Lucy: "Hello from Client 1 - testing DME"
```

While **Client 1** is posting, try posting simultaneously from **Client 2**:

```
> post "Client 2 posting at the same time"
```

---

## 9. Check Server Logs

On the server:

```
./start_server.sh
```

You’ll observe all `VIEW` and `POST` requests, file updates, and connection logs.

At the end, both messages appear timestamped and ordered correctly in the shared file.

---

## Summary

**Chatroom Distributed System – Setup and Test Guide**

1. Create 3 EC2 Instances
2. Create Security Group
3. Connect to Each Instance
4. Clone the Repository and Set Up Environment
5. Compile the Code
6. Start the Server
7. Start the Clients
8. Test the Application

---

Would you like me to save this as a downloadable `Instructions.md` file formatted for your GitHub Wiki (with proper link anchors and code block syntax)?

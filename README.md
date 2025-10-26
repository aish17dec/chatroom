chmod +x create_structure.sh
./create_structure.sh
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

./bin/server --bind 0.0.0.0:7000 --file ./chat.txt
./bin/client   --user Lucy   --self-id 1   --peer-id 2   --listen 0.0.0.0:8001   --peer 172:   --server 172.31.23.9:7000

<pre>
         ______
        / ____ \
   ____/ /    \ \
  / ____/   o  \ \
 / /     __/   / / ECHOS
/ /  o__/  \  / /  Data Replicator
\ \     \__/  \ \  Copyright (C) 2019, Tom Oleson, All Rights Reserved.
 \ \____   \   \ \
  \____ \   o  / /
       \ \____/ /
        \______/
</pre>

# Data Replicator

Requires libcm_64.so from project common

Features:
1. Non-blocking I/O
2. Service multiple client connections on TCP/IP
3. Event driven (epoll)
4. Uses Linux system resources/libraries; no third-party libraries
5. Event loop in its own thread
6. Single thread listener and thread-pool dispatcher
7. Written in C++ to be Fast!


When data is received from one connection, a copy of the data is
sent to all other connections.


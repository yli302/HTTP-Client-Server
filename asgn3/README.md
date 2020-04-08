# adding key-value store to multithreaded HTTP server
Author: Yujia Li
CruzID: yli302@ucsc.edu
- This is a program implement basic HTTP client-server PUT and GET.(HTTP/1.1)
- has key-value store to multithreaded HTTP server

### To run HTTP client-server
- use make to compile source file.
```sh
$ make
```
- run server.
```sh
$ ./httpserver [-N num] [-c num] [-f filename] address:port
```
- run client. after second arguemnt, all arguemnt must in r:httpname:filename or s:filename:httpname format.
```sh
$ ./httpclient address:port r:httpname:filename s:filename:httpname
```

### Files
- server.cpp client.cpp kvstore.h city.cpp city.h config.h Makefile DESIGN.pdf WRITEUP.pdf README.md are required.

### test
- see test directory.
- run shell scripts to test.

### Resource:
cityhash:
https://github.com/google/cityhash

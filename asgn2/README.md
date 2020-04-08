# multi-threaded HTTP server with in-memory caching
Author: Yujia Li
CruzID: yli302@ucsc.edu
- This is a program implement basic HTTP client-server PUT and GET.(HTTP/1.1)
- multi-thread server
- in-memory caching(still work on)

### To run HTTP client-server
- use make to compile source file.
```sh
$ make
```
- run server.
```sh
$ ./httpserver [-N num] [-c num] address:port
```
- run client. after second arguemnt, all arguemnt must in r:httpname:filename or s:filename:httpname format.
```sh
$ ./httpclient address:port r:httpname:filename s:filename:httpname
```

### Files
- server.cpp client.cpp lruCache.h lruCache.cpp Makefile DESIGN.pdf WRITEUP.pdf README.md are required.

### Resource:
LRUCache:
https://medium.com/@krishankantsinghal/my-first-blog-on-medium-583159139237

### Bugs:
- When N < client process, server may crash down
- When run lots of client process as the same time, need to end client by ^c.
```sh
$ ./httpclient 0.0.0.0:8000 r:1234567890123456789012345678901234567890:file1 &
$ ./httpclient 0.0.0.0:8000 r:1234567890123456789012345678901234567890:file2 &
$ ./httpclient 0.0.0.0:8000 r:1234567890123456789012345678901234567890:file3 &
$ ./httpclient 0.0.0.0:8000 s:file1:1234567890123456789012345678901234567891 &
$ ./httpclient 0.0.0.0:8000 r:1234567890123456789012345678901234567890:file3
```
will cause this error.

### Not finished
- cache write
- cache sync

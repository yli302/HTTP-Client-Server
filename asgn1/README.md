# HTTP client-server
Author: Yujia Li
CruzID: yli302@ucsc.edu
This is a program implement basic HTTP client-server PUT and GET.(HTTP/1.1)

### To run HTTP client-server
- use make to compile source file.
```sh
$ make
```
- run server.
```sh
$ ./httpserver address:port 
```
- run client. after second arguemnt, all arguemnt must in r:httpname:filename or s:filename:httpname format.
```sh
$ ./httpclient address:port r:httpname:filename s:filename:httpname
```

### Files
- server.cpp client.cpp Makefile DESIGN.pdf WRITEUP.pdf README.md are required.
- bigfile smallfile zerofile are files for test.
- test is directory for test.

### Resource:
How to check valid-filename:
http://www.csharp411.com/check-valid-file-path-in-c/

### Bugs:
- Request PUT or GET(must include at least one request which will be responded other than 200 ok or 201 create) many times(at least 5 times) in very short time(1~2 seconds) in different ./httpclient process will lead to write incorrect data to file in PUT request.

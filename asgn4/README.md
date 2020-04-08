# adding aliases to the HTTP server
Author: Yujia Li
CruzID: yli302@ucsc.edu
- This is a program implement basic HTTP client-server PUT and GET.(HTTP/1.1)
- has key-value store to multithreaded HTTP server
- has aliases function to the HTTP server

### implementations in asg4
- mainly in kvs_nameing.h
- client.cpp: add PATCH: ALIAS, delete limite of name send by PUT and GET (needn't be 40 hex char).
- server.cpp: add PATCH: ALIAS, delete limite of name receive by PUT and GET (needn't be 40 hex char).

### To run HTTP client-server
- use make to compile source file.
```sh
$ make
```
- run server.
```sh
$ ./httpserver [-N num] [-c num] [-f filename.fvs] [-m name_mapping.fvs] address:port
```
- run client. after second arguemnt, all arguemnt must in r:httpname:filename or s:filename:httpname or a:existing_name:new_name format.
```sh
$ ./httpclient address:port r:httpname:filename s:filename:httpname a:existing_name:new_name
```


### Files
- [ server.cpp client.cpp kvstore.h kvs_nameing.h city.cpp city.h config.h Makefile DESIGN.pdf WRITEUP.pdf README.md ]

### test
- see test directory.
- run shell scripts to test.

### Resource:
cityhash:
https://github.com/google/cityhash

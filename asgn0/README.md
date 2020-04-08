# mycat
Author: Yujia Li
CruzID: yli302@ucsc.edu
mycat is a program implement basic cat program, without support for any flags.

### To run mycat
- use make to compile source file.
```sh
$ make
```
- for copy standard input to standard output.
```sh
$ ./mycat 
```
- for copy file to standard output. No file number limit. In example, there are two files.
```sh
$ ./mycat filename1 filename2
```

### Files
- cat.cpp Makefile DESIGN.pdf WRITEUP.pdf README.md are required.
- bigfile smallfile zerofile are files for test.
- test is directory for test.
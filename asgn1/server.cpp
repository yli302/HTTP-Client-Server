#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define NUL '\0'

bool isValidPort(char *port) {
  for (int i = 0; port[i] != NUL; i++) {
    if (!isdigit(port[i]))
      return 0;
  }
  return 1;
}

int32_t parseContentLength(char *cLength) {
  if (cLength == NULL)
    return 1;
  else {
    cLength = cLength + 16;
    for (int i = 0; cLength[i] != NUL; i++) {
      if (!isdigit(cLength[i]))
        return 1;
    }
    return atoi(cLength);
  }
}

bool isValidHeader(char **header) {
  uint32_t headerBool = 1;
  size_t headerSize = 0;
  for (int i = 0; header[i] != NULL; i++)
    headerSize++;
  // header must be three parts: "action Code" "httpname" "http version"
  if (headerSize != 3) {
    headerBool = 0;
  } else {
    // check action code
    if ((strcmp(header[0], "PUT") != 0) && (strcmp(header[0], "GET") != 0))
      headerBool = 0;
    // check HTTP version
    if ((strcmp(header[2], "HTTP/1.1") != 0))
      headerBool = 0;
    // check httpname
    if (!(strlen(header[1]) == 40) &&
        !((strlen(header[1]) == 41) && (header[1][0] == '/')))
      headerBool = 0;
  }
  return headerBool;
}

int32_t main(int32_t argc, char **argv) {
  char fileBuf[4096];
  char headerBuf[4096];
  ssize_t count;
  char *SERVER_NAME_STRING;
  int32_t PORT_NUMBER = 80;

  // parsing argv[1] to SERVER_NAME_STRING and PORT_NUMBER
  if (argc != 2) {
    fprintf(stderr, "Error: invalid arguemnt\n");
    exit(1);
  } else {
    const char split[2] = ":";
    char *token;
    // parse address
    SERVER_NAME_STRING = (token = strtok(argv[1], split));
    if (token != NULL) {
      // parse port
      if ((token = strtok(NULL, split)) != NULL) {
        if (!isValidPort(token)) {
          fprintf(stderr, "Error: invalid port number\n");
          exit(1);
        }
        PORT_NUMBER = atoi(token);
      }
    }
  }

  // create a socket
  struct hostent *hent;
  if ((hent = gethostbyname(SERVER_NAME_STRING)) == NULL) {
    fprintf(stderr, "Error: invalid IPv4 address: %s\n", strerror(h_errno));
    exit(1);
  }
  // print address and port info to stdout
  fprintf(stdout, "HTTPServer address:%s port:%d\n", SERVER_NAME_STRING,
          PORT_NUMBER);

  struct sockaddr_in addr;
  memcpy(&addr.sin_addr.s_addr, hent->h_addr, hent->h_length);
  addr.sin_port = htons(PORT_NUMBER);
  addr.sin_family = AF_INET;

  int sock;
  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    fprintf(stderr, "Error: fail create socket: %s\n", strerror(errno));
    exit(1);
  }
  // Socket Setup for Server
  int enable = 1;
  if ((setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable))) ==
      -1) {
    fprintf(stderr, "Error: fail set socket option: %s\n", strerror(errno));
    exit(1);
  }
  if ((bind(sock, (struct sockaddr *)&addr, sizeof(addr))) == -1) {
    fprintf(stderr, "Error: fail bind socket: %s\n", strerror(errno));
    exit(1);
  }
  // run server all the time, until crtl+c
  while (true) {
    fprintf(stdout,
            "-------------------------------------------------------\n");
    memset(headerBuf, 0, 4096);
    if ((listen(sock, 0)) == -1) {
      fprintf(stderr, "Error: fail listen: %s\n", strerror(errno));
      exit(1);
    }
    int cl;
    if ((cl = accept(sock, NULL, NULL)) == -1) {
      fprintf(stderr, "Error: fail accept: %s\n", strerror(errno));
      exit(1);
    }
    /* receive request header and parse header to *request_header[]
     * e.g.: request_header[0] = PUT,
     *		 request_header[1] = 40byte-string,
     *		 request_header[2] = HTTP/1.1
     * option: Content-Length: int.
     */
    count = recv(cl, headerBuf, sizeof(headerBuf) - 1, 0);
    // *request is a copy of HTTP reqeust header
    char request[4096];
    strncpy(request, headerBuf, strlen(headerBuf));
    char *token = strtok(headerBuf, "\r\n:");
    // print Request Header to stdout
    if (token == NULL) {
      fprintf(stdout, "No request received \n");

    } else {
      fprintf(stdout, "\"%s\" \n", token);
    }
    char *request_header[256];
    request_header[0] = strtok(token, " ");
    for (int i = 0; request_header[i] != NULL;) {
      request_header[++i] = strtok(NULL, " ");
    }
    if (!isValidHeader(request_header)) {
      send(cl, "HTTP/1.1 400 Bad Request\r\n\r\n", 28, 0);
      fprintf(stderr, "Error: 400 Bad Request: Unsupported request format\n");
      close(cl);
      continue;
    } else {
      // delete the '/' before httpname, if this '/' exist.
      char *httpname = request_header[1];
      if (httpname[0] == '/') {
        request_header[1] = httpname + 1;
      }
    }
    // parse Content-Length: number to a int32_t
    token = strstr(request, "Content-Length: ");
    token = strtok(token, "\r\n");
    int32_t content_Length = parseContentLength(token);

    /*
     * handle requests
     */
    int32_t fd = 0;
    int32_t bufferRead = 0;
    bool isNewfile = 0;
    /*
     * PUT request
     */
    if (strcmp(request_header[0], "PUT") == 0) {
      if ((fd = open(request_header[1], O_WRONLY | O_TRUNC)) == -1) {
        // create new file
        if ((fd = open(request_header[1], O_CREAT | O_WRONLY | O_TRUNC,
                       S_IRWXU)) == -1) {
          send(cl, "HTTP/1.1 403 Forbidden\r\n\r\n", 26, 0);
          fprintf(stderr, "Error: 403 Forbidden: fail open: %s\n",
                  strerror(errno));
          close(cl);
          close(fd);
          continue;
        }
        isNewfile = 1;
      }
      // is a directory
      if (write(fd, 0, 0) == -1) {
        send(cl, "HTTP/1.1 403 Forbidden\r\n\r\n", 26, 0);
        fprintf(stderr, "Error: 403 Forbidden: fail open: %s\n",
                strerror(errno));
        close(cl);
        close(fd);
        continue;
      }
      // send response
      if (isNewfile) {
        send(cl, "HTTP/1.1 201 Created\r\n\r\n", 24, 0);
        fprintf(stdout, "201 Created\n");
      } else {
        send(cl, "HTTP/1.1 200 OK\r\n\r\n", 19, 0);
        fprintf(stdout, "200 OK\n");
      }
      // write into file
      memset(fileBuf, 0, 4096);
      count = recv(cl, fileBuf, sizeof(fileBuf) - 1, 0);
      while (count != 0) {
        if (write(fd, fileBuf, count) == -1) {
          send(cl, "HTTP/1.1 403 Forbidden\r\n\r\n", 26, 0);
          fprintf(stderr, "Error: 403 Forbidden: fail open: %s\n",
                  strerror(errno));
          close(cl);
          close(fd);
          continue;
        }
        memset(fileBuf, 0, 4096);
        bufferRead = bufferRead + count;
        if (bufferRead == content_Length)
          break;
        count = recv(cl, fileBuf, sizeof(fileBuf) - 1, 0);
      }
      close(cl);
      close(fd);
      continue;
    }
    /*
     * GET request
     */
    if (strcmp(request_header[0], "GET") == 0) {
      if ((fd = open(request_header[1], O_RDONLY)) == -1) {
        send(cl, "HTTP/1.1 404 Not Found\r\n\r\n", 26, 0);
        fprintf(stderr, "Error: 404 Not Found: fail open: %s\n",
                strerror(errno));
        close(cl);
        close(fd);
        continue;
      }
      if ((count = read(fd, fileBuf, sizeof(fileBuf))) == -1) {
        send(cl, "HTTP/1.1 403 Forbidden\r\n\r\n", 26, 0);
        fprintf(stderr, "Error: 403 Forbidden: fail read: %s\n",
                strerror(errno));
        close(cl);
        close(fd);
        continue;
      }
      // get Content-Length of file: "Content-Length: number\r\n\r\n"
      struct stat st;
      fstat(fd, &st);
      off_t file_sz = st.st_size;
      char target_string[256];
      sprintf(target_string, "%ld", file_sz);
      char cLength[4096];
      memset(cLength, 0, 4096);
      strcat(cLength, "Content-Length: ");
      strcat(cLength, target_string);
      strcat(cLength, "\r\n\r\n");
      // send header(response and content-length)
      send(cl, "HTTP/1.1 200 OK\r\n", 17, 0);
      fprintf(stdout, "200 OK\n");
      send(cl, cLength, strlen(cLength), 0);
      // send data(if exist)
      while (count != 0) {
        send(cl, fileBuf, count, 0);
        count = read(fd, fileBuf, sizeof(fileBuf));
      }

      close(cl);
      close(fd);
      continue;
    }
    close(cl);
  }
}
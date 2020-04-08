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

bool isValidRequest(char **request) {
  uint32_t requestBool = 1;
  size_t requestSize = 0;
  for (int i = 0; request[i] != NULL; i++) {
    requestSize++;
  }
  /* request argument must be three part:   s:filename:httpname
   *                                     or r:httpname:filename
   */
  if (requestSize != 3) {
    requestBool = 0;
  } else {
    if (!(strcmp(request[0], "s"))) {
      // check httpname
      requestBool = (strlen(request[2]) == 40);
    } else if (!(strcmp(request[0], "r"))) {
      // check httpname
      requestBool = (strlen(request[1]) == 40);
    } else
      requestBool = 0;
  }
  return requestBool;
}

int32_t main(int32_t argc, char **argv) {
  char fileBuf[4096];
  char headerBuf[4096];
  ssize_t count;
  char *SERVER_NAME_STRING;
  int32_t PORT_NUMBER = 80;

  // parsing argv[1] to SERVER_NAME_STRING and PORT_NUMBER
  if (argc < 2) {
    fprintf(stderr, "Error: invalid arguemnt\n");
    exit(1);
  } else {
    char *token;
    const char split[2] = ":";
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
  struct sockaddr_in addr;
  memcpy(&addr.sin_addr.s_addr, hent->h_addr, hent->h_length);
  addr.sin_port = htons(PORT_NUMBER);
  addr.sin_family = AF_INET;

  for (int i = 2; i < argc; i++) {
    fprintf(stdout,
            "-------------------------------------------------------\n");
    // Socket Setup for Client
    int sock;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
      fprintf(stderr, "Error: fail create socket: %s\n", strerror(errno));
      continue;
    }
    if ((connect(sock, (struct sockaddr *)&addr, sizeof(addr))) == -1) {
      fprintf(stderr, "Error: fail connect: %s\n", strerror(errno));
      continue;
    }

    /*
     * parse request argument to HTTP Header.
     * s:filename:httpname -> PUT httpname HTTP/1.1\r\nContent-Length:
     * number\r\n\r\n r:httpname:filename -> GET httpname HTTP/1.1\r\n\r\n
     * *curr_argv[] store three elements which are splited by ':'
     */
    char *curr_argv[256];
    char *token = strtok(argv[i], ":");
    curr_argv[0] = token;
    for (int j = 0; curr_argv[j] != NULL;) {
      curr_argv[++j] = strtok(NULL, ":");
    }
    if (!isValidRequest(curr_argv)) {
      fprintf(stderr, "Error: request argument is wrong\n");
      close(sock);
      continue;
    }

    /*
     * Send HTTP request Header to server
     */
    int32_t fd = 0;
    int32_t bufferRead = 0;
    memset(fileBuf, 0, 4096);
    /*
     * PUT request
     */
    if (strcmp(curr_argv[0], "s") == 0) {
      // For PUT, need to check the Content-Length.
      // filename is curr_argv[1]
      memset(headerBuf, 0, 4096);
      strcat(headerBuf, "PUT ");
      strcat(headerBuf, curr_argv[2]);
      strcat(headerBuf, " HTTP/1.1\r\n");
      strcat(headerBuf, "Content-Length: ");

      // check file condition with open and read
      if ((fd = open(curr_argv[1], O_RDONLY)) == -1) {
        fprintf(stderr, "Error: fail open: %s\n", strerror(errno));
        close(sock);
        close(fd);
        continue;
      }
      if ((count = read(fd, fileBuf, sizeof(fileBuf))) == -1) {
        fprintf(stderr, "Error: fail read: %s\n", strerror(errno));
        close(sock);
        close(fd);
        continue;
      }

      // get Content-Length of file, and send PUT request header to server
      struct stat st;
      fstat(fd, &st);
      off_t file_sz = st.st_size;
      char target_string[256];
      sprintf(target_string, "%ld", file_sz);
      strcat(headerBuf, target_string);
      strcat(headerBuf, "\r\n\r\n");
      send(sock, headerBuf, strlen(headerBuf), 0);

      // send data to server(if exist)
      while (count != 0) {
        send(sock, fileBuf, count, 0);
        count = read(fd, fileBuf, sizeof(fileBuf));
      }
      // receive response from server
      memset(headerBuf, 0, 4096);
      count = recv(sock, headerBuf, sizeof(headerBuf), MSG_WAITALL);
      fprintf(stdout, "%s", headerBuf);
      close(sock);
      close(fd);
      continue;
    }

    /*
     * GET request
     */
    if (strcmp(curr_argv[0], "r") == 0) {
      // send GET request header to server
      // filename is curr_argv[2]
      strcat(headerBuf, "GET ");
      strcat(headerBuf, curr_argv[1]);
      strcat(headerBuf, " HTTP/1.1\r\n\r\n");
      send(sock, headerBuf, strlen(headerBuf), 0);

      if ((fd = open(curr_argv[2], O_CREAT | O_WRONLY | O_TRUNC)) == -1) {
        fprintf(stderr, "Error: fail open: %s\n", strerror(errno));
        while ((count = recv(sock, fileBuf, sizeof(fileBuf) - 1, 0)) != 0) {
          memset(fileBuf, 0, 4096);
          count = recv(sock, fileBuf, sizeof(fileBuf) - 1, 0);
        }
        close(sock);
        close(fd);
        continue;
      }

      // receive GET response header from server
      memset(headerBuf, 0, 4096);
      if ((count = recv(sock, headerBuf, sizeof(headerBuf) - 1, 0)) == 0) {
        fprintf(stderr, "Error: no response header\n");
      }
      char temp[4096]; // temp string to store something.
      memset(temp, 0, 4096);
      // handle status code
      char *start_ptr;
      char *end_ptr;
      start_ptr = strstr(headerBuf, "HTTP/1.1");
      if (start_ptr == NULL) {
        fprintf(stderr, "Error: invalid response header\n");
        close(sock);
        close(fd);
        continue;
      }
      end_ptr = strstr(start_ptr, "\r\n");
      strncpy(temp, start_ptr, end_ptr - start_ptr);
      fprintf(stdout, "%s\r\n\r\n", temp);
      if (strcmp(temp, "HTTP/1.1 200 OK")) {
        close(sock);
        close(fd);
        continue;
      }
      // handle Content-Length (if exist). cLength is the int type of the
      // Content-Length
      int32_t cLength = 1;
      start_ptr = strstr(headerBuf, "Content-Length: ");
      if (start_ptr != NULL) {
        end_ptr = strstr(start_ptr, "\r\n");
        memset(temp, 0, 4096);
        strncpy(temp, start_ptr + 16, end_ptr - (start_ptr + 16));
        cLength = atoi(temp);
      }
      // handle the data received with header (if exist).
      start_ptr = strstr(headerBuf, "\r\n\r\n");
      if (start_ptr == NULL) {
        fprintf(stderr, "Error: invalid response header\n");
        close(sock);
        close(fd);
        continue;
      }
      strncpy(fileBuf, start_ptr + 4, strlen(start_ptr + 4));
      // receive data from server
      while (count != 0) {
        if (write(fd, fileBuf, strlen(fileBuf)) == -1) {
          fprintf(stderr, "Error: fail write: %s\n", strerror(errno));
          close(sock);
          close(fd);
          continue;
        }
        memset(fileBuf, 0, 4096);
        bufferRead = bufferRead + count;
        if (bufferRead == cLength)
          break;
        count = recv(sock, fileBuf, sizeof(fileBuf) - 1, 0);
      }
      close(sock);
      close(fd);
      continue;
    }
    close(sock);
  }
}
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define NUL '\0'

using namespace std;
int fd;
uint32_t curr_fvs_end;

#include "kvstore.h"

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
  bool headerBool = 1;
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

/* implementation of assignemnt 2 */
/* multi-thread related */
pthread_mutex_t lock_dispatch; // for dispatch descriptor to worker threads
pthread_mutex_t lock_rw;       // for write and read operation
sem_t nworkers;   // To know whether at least a not busy work thread is exist.
int32_t nreaders; // how many worker threads are operating read now
sem_t is_writing; // is writing now

/* use to store thread related information and share between threads.
 * id is as same as the index of the array of thread_info
 * have_work use to tell thread work or wait.
 * cl is the file descriptor, and when it is -1, this work thread is not busy.
 */
struct thread_info {
  int32_t id = 0;
  pthread_cond_t have_work;
  int cl = -1;
};

void *client_connection(void *arg) {
  struct thread_info *ti = (thread_info *)arg;
  for (;;) {
    pthread_mutex_lock(&lock_dispatch);
    // sleep if no work hands off from dispatch thread
    if (ti->cl == -1) {
      pthread_cond_wait(&ti->have_work, &lock_dispatch);
    }
    if (ti->cl != -1) {
    }
    pthread_mutex_unlock(&lock_dispatch);
    /* handle HTTP header,
     * recv, send, read and write here.
     */
    char fileBuf[4096];
    char headerBuf[4096];
    ssize_t count = 0;
    memset(headerBuf, 0, 4096);
    memset(fileBuf, 0, 4096);
    // handle HTTP header
    /* receive request header and parse header to *request_header[]
     * e.g.: request_header[0] = PUT,
     *		 request_header[1] = 40byte-string,
     *		 request_header[2] = HTTP/1.1
     * option: Content-Length: int.
     */
    count = recv(ti->cl, headerBuf, sizeof(headerBuf) - 1, 0);
    // *request is a copy of HTTP reqeust header
    char request[4096];
    strncpy(request, headerBuf, strlen(headerBuf));
    char *token = strtok(headerBuf, "\r\n:");
    // print Request Header to stdout
    if (token == NULL) {
      fprintf(stderr, "No request received \n");
    } else {
      // fprintf(stdout, "\"%s\" \n", token);
    }
    char *request_header[256];
    request_header[0] = strtok(token, " ");
    for (int i = 0; request_header[i] != NULL;) {
      request_header[++i] = strtok(NULL, " ");
    }
    if (!isValidHeader(request_header)) {
      send(ti->cl, "HTTP/1.1 400 Bad Request\r\n\r\n", 28, 0);
      fprintf(stderr, "Error: 400 Bad Request: Unsupported request format\n");
      close(ti->cl);
      // set this worker thread as not busy
      ti->cl = -1;
      sem_post(&nworkers);
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
    ssize_t content_Length = parseContentLength(token);
    /*
     * handle requests
     */
    size_t offset = 0;
    bool isNewfile = 0;
    uint8_t object_name[20];
    hex_to_ascii(object_name, request_header[1]);
    /*
     * PUT request
     */
    if (strcmp(request_header[0], "PUT") == 0) {
      // set content length to entry object
      //  if entry object already exists, kvinfo returun 0
      //  if created an entry object, kvinfo return 1
      sem_wait(&is_writing);
      isNewfile = kvinfo(object_name, content_Length);
      // for notice client send data
      send(ti->cl, "continue", 8, 0);
      // receive data from client and write to kvs file
      memset(fileBuf, 0, 4096);
      count = recv(ti->cl, fileBuf, sizeof(fileBuf), 0);
      while (true) {
        kvwrite(object_name, count, offset, (uint8_t *)fileBuf);
        memset(fileBuf, 0, 4096);
        offset += count;
        if (offset == (size_t)content_Length) {
          break;
        }
        count = recv(ti->cl, fileBuf, sizeof(fileBuf), 0);
      }
      sem_post(&is_writing);
      // send response
      if (isNewfile) {
        send(ti->cl, "HTTP/1.1 201 Created\r\n\r\n", 24, 0);
        // fprintf(stdout, "201 Created\n");
      } else {
        send(ti->cl, "HTTP/1.1 200 OK\r\n\r\n", 19, 0);
        // fprintf(stdout, "200 OK\n");
      }
      close(ti->cl);
      // set this worker thread as not busy
      // pthread_mutex_lock(&lock_nworkers);
      ti->cl = -1;
      sem_post(&nworkers);
      // pthread_mutex_unlock(&lock_nworkers);
      continue;
    }
    /*
     * GET request
     */
    if (strcmp(request_header[0], "GET") == 0) {
      pthread_mutex_lock(&lock_rw);
      nreaders += 1;
      if (nreaders == 1) {
        sem_wait(&is_writing);
      }
      pthread_mutex_unlock(&lock_rw);
      if ((content_Length = kvinfo(object_name, -1)) == -2) {
        send(ti->cl, "HTTP/1.1 404 Not Found\r\n\r\n", 26, 0);
        fprintf(stderr, "Error: 404 Not Found: no entry object\n");
        pthread_mutex_lock(&lock_rw);
        nreaders -= 1;
        if (nreaders == 0) {
          sem_post(&is_writing);
        }
        pthread_mutex_unlock(&lock_rw);
        close(ti->cl);
        // set this worker thread as not busy
        ti->cl = -1;
        sem_post(&nworkers);
        continue;
      }
      // send GET Content-Length: "Content-Length: number\r\n\r\n"
      char target_string[256];
      sprintf(target_string, "%ld", content_Length);
      char cLength[4096];
      memset(cLength, 0, 4096);
      strcat(cLength, "Content-Length: ");
      strcat(cLength, target_string);
      strcat(cLength, "\r\n\r\n");
      // send header(response and content-length)
      send(ti->cl, "HTTP/1.1 200 OK\r\n", 17, 0);
      // fprintf(stdout, "200 OK\n");
      send(ti->cl, cLength, strlen(cLength), 0);
      // read and send data(if exist)
      count = kvread(object_name, sizeof(fileBuf), offset, (uint8_t *)fileBuf);
      while (true) {
        send(ti->cl, fileBuf, count, 0);
        memset(fileBuf, 0, 4096);
        offset += count;
        if (offset == (size_t)content_Length) {
          break;
        }
        count =
            kvread(object_name, sizeof(fileBuf), offset, (uint8_t *)fileBuf);
      }
      pthread_mutex_lock(&lock_rw);
      nreaders -= 1;
      if (nreaders == 0) {
        sem_post(&is_writing);
      }
      pthread_mutex_unlock(&lock_rw);

      close(ti->cl);
      // set this worker thread as not busy
      ti->cl = -1;
      sem_post(&nworkers);
      continue;
    }
    close(ti->cl);
    ti->cl = -1;
    sem_post(&nworkers);
  }
}

int32_t main(int32_t argc, char **argv) {
  char *SERVER_NAME_STRING;
  uint32_t PORT_NUMBER = 80;
  uint32_t NUM_THREADS = 4;
  size_t NUM_BLOCK = 40;
  char fvs_filename[40];
  memset(fvs_filename, 0, 40);

  int opt;
  while ((opt = getopt(argc, argv, "N:c:f:")) != -1) {
    switch (opt) {
    case 'N':
      NUM_THREADS = atoi(optarg);
      break;
    case 'c':
      NUM_BLOCK = atoi(optarg);
      break;
    case 'f':
      strncpy(fvs_filename, optarg, strlen(optarg));
      break;
    default:
      fprintf(stderr,
              "Usage: %s [-N nthreads] [-c cachesize] [-f filename] "
              "[IPaddress:portNumber]",
              argv[0]);
      exit(1);
    }
  }

  // did not get fvs filename,
  if (strcmp(fvs_filename, "") == 0) {
    fprintf(stderr, "Usage: must specify fvs filename by [-f filename]\n");
    exit(1);
  }
  // initial kvs file
  // if kvs file exist, keep track of the pointer to file end.
  // if kvs file doesn't exist, create new one.
  if ((fd = open(fvs_filename, O_RDWR | O_CREAT,
                 S_IRWXU | S_IRWXG | S_IRWXO)) == -1) {
    fprintf(stderr, "Error: open fail: %s\n", strerror(errno));
  }
  if (init_fvs_file() == 0) {
    fprintf(stdout, "Success open fvsfile: %s\n", fvs_filename);
  } else {
    fprintf(stdout, "Success create fvsfile: %s\n", fvs_filename);
  }

  // parsing argv[optind] to SERVER_NAME_STRING and PORT_NUMBER
  if (argc > 6) {
    fprintf(stderr, "Error: invalid arguemnt\n");
    exit(1);
  } else {
    const char split[2] = ":";
    char *token;
    // parse address
    SERVER_NAME_STRING = (token = strtok(argv[optind], split));
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

  /*
   * server socket setup
   */
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
  if ((listen(sock, 0)) == -1) {
    fprintf(stderr, "Error: fail listen: %s\n", strerror(errno));
    exit(1);
  }
  int cl;

  /*
   * pthread spawn and create thread
   */
  pthread_mutex_init(&lock_dispatch, NULL);
  pthread_mutex_init(&lock_rw, NULL);
  sem_init(&nworkers, 0, NUM_THREADS);
  sem_init(&is_writing, 0, 1);
  nreaders = 0;
  pthread_t *thread = (pthread_t *)malloc(sizeof(pthread_t) * NUM_THREADS);
  struct thread_info *tinfo =
      (thread_info *)malloc(sizeof(tinfo) * NUM_THREADS);
  for (uint32_t i = 0; i < NUM_THREADS; i++) {
    tinfo[i].id = i;
    pthread_cond_init(&tinfo[i].have_work, NULL);
    tinfo[i].cl = -1;
    pthread_create(&thread[i], NULL, client_connection, &tinfo[i]);
  }

  // dispatch thread
  while ((cl = accept(sock, NULL, NULL))) {
    if (cl == -1) {
      fprintf(stderr, "Error: fail accept: %s\n", strerror(errno));
      exit(1);
    }
    // sleep if all work threads are busy
    sem_wait(&nworkers);
    // check which work thread is not busy.
    pthread_mutex_lock(&lock_dispatch);
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
      if (tinfo[i].cl == -1) {
        tinfo[i].cl = cl;
        pthread_cond_signal(&tinfo[i].have_work);
        break;
      }
    }
    pthread_mutex_unlock(&lock_dispatch);
  }
}

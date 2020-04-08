#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_SIZE 65536
#define NO_FILE 1
#define FALSE -1
#define FILE_END 0

/* a function to print error.
 * output should in that format: "cat: fileName: errorMsg\n"
 */
void printErr(char *fileName, char *data, char *errorMsg) {
  memset(data, 0, BUFFER_SIZE * sizeof(char));
  strcat(data, "cat: ");
  strcat(data, fileName);
  strcat(data, ": ");
  strcat(data, errorMsg);
  strcat(data, "\n");
  write(STDERR_FILENO, data, BUFFER_SIZE);
}

int main(int argc, char **argv) {
  char data[BUFFER_SIZE];
  /* fd is file descriptor.
   * count stores the return value from read.
   */
  int32_t fd, count;

  if (argc == NO_FILE) {
    // when no fileName in argument, input is standard input.
    while (true) {
      count = read(STDIN_FILENO, data, sizeof(data));
      if (count == FALSE) {
        // error when read file: "Is a directory"
        char noFile[2] = "-";
        printErr(noFile, data, strerror(errno));
        break;
      }
      if (count == FILE_END)
        break;
      write(STDOUT_FILENO, data, count);
    }
  } else {
    // when arguments are fileNames, input is file.
    for (int i = 1; i < argc; i++) {
      fd = open(argv[i], O_RDONLY);
      if (fd == FALSE) {
        // error when open file: "No such file or directory" or "Permission
        // denied"
        printErr(argv[i], data, strerror(errno));
      } else {
        // read fd and write result to standard output
        while (true) {
          count = read(fd, data, sizeof(data));
          if (count == FALSE) {
            // error when read file: "Is a directory"
            printErr(argv[i], data, strerror(errno));
            break;
          }
          if (count == FILE_END)
            break;
          write(STDOUT_FILENO, data, count);
        }
        close(fd);
      }
    }
  }
  return 0;
}
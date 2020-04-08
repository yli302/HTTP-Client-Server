#ifndef KVS_H_
#define KVS_H_

#include "city.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;

typedef struct kvs_entry {
  uint32_t block_nr;
  int32_t length;
  uint32_t datapointer;
  uint8_t httpname[20];
} kvs_entry;

typedef struct key {
  uint8_t httpname[20];
  uint32_t block_nr;
} key;

// initial kvs file
// if kvs file exists, keep track of pointer to file end, return 0
// if kvs file doesn't exist, create new one, return 1
int32_t init_fvs_file() {
  struct kvs_entry init_entry;
  memset(&init_entry, 0, 32);
  size_t offset = 0;
  struct stat st;
  fstat(fd, &st);
  off_t file_sz = st.st_size;
  curr_fvs_end = ((((file_sz - 25600000) / 4096) + 1) * 4096) + 25600000;
  if (file_sz == 0) {
    for (int i = 0; i < 800000; ++i) {
      if (pwrite(fd, &init_entry, 32, offset) == -1) {
        fprintf(stderr, "Error: write fail: %s\n", strerror(errno));
      }
      offset += 32;
    }
    fstat(fd, &st);
    file_sz = st.st_size;
    curr_fvs_end = file_sz;
    return 1;
  }
  return 0;
}

// key is a 24 byte string, httpname + block_nr.
uint32_t nameHash(const uint8_t *object_name, uint32_t block_nr) {
  struct key key;
  strncpy((char *)key.httpname, (const char *)object_name, 20);
  key.block_nr = block_nr;
  return CityHash32((const char *)&key, 24) % 800000;
}

// convert 40 byte hex to 20 byte ascii
void hex_to_ascii(uint8_t *object_name, const char *httpname) {
  for (uint32_t i = 0; i < 20; ++i) {
    char temp[3];
    temp[0] = httpname[i * 2];
    temp[1] = httpname[i * 2 + 1];
    temp[2] = '\0';
    uint8_t ascii_value = (uint8_t)strtol(temp, 0, 16);
    object_name[i] = ascii_value;
  }
}

ssize_t kvwrite(const uint8_t *object_name, size_t length, size_t offset,
                const uint8_t *data) {
  struct kvs_entry entry;
  uint32_t entry_ptr;
  uint32_t block_nr = offset / 4096;
  uint32_t data_offset = offset % 4096;
  entry_ptr = nameHash(object_name, block_nr);
  // loop for linear probing, iterate entil empty or find object.
  for (;;) {
    memset(&entry, 0, 32);
    if (pread(fd, &entry, 32, entry_ptr * 32) == -1) {
      fprintf(stderr, "Error: kvwrite read fail: %s\n", strerror(errno));
      return -1;
    }
    // if object doesn't exist, create entry object and update data.
    if (strcmp((char *)entry.httpname, "") == 0) {
      memcpy(entry.httpname, object_name, 20);
      entry.block_nr = block_nr;
      entry.length = length;
      entry.datapointer = curr_fvs_end;
      curr_fvs_end += 4096;
      if (pwrite(fd, &entry, 32, entry_ptr * 32) == -1) {
        fprintf(stderr, "Error: kvwrite write fail: %s\n", strerror(errno));
        return -1;
      }
      return pwrite(fd, data, length, entry.datapointer + data_offset);
    }
    // if this entry contains correct object, write data
    if (memcmp(entry.httpname, object_name, 20) == 0 &&
        entry.block_nr == block_nr) {
      // if this is not block 0, update length.
      //  if write a new data, overwrite the Length
      //  if continue write data in a block, plus this length
      if (block_nr != 0) {
        if (data_offset == 0) {
          entry.length = length;
        } else {
          entry.length = data_offset + length;
        }
      }
      if (pwrite(fd, &entry, 32, entry_ptr * 32) == -1) {
        fprintf(stderr, "Error: kvwrite write fail: %s\n", strerror(errno));
        return -1;
      }
      return pwrite(fd, data, length, entry.datapointer + data_offset);
    }
    // if this entry is not the correct object or empty, find next one
    entry_ptr += 1 * 32;
  }
  return -1;
}

ssize_t kvread(const uint8_t *object_name, size_t length, size_t offset,
               uint8_t *data) {
  struct kvs_entry entry;
  memset(&entry, 0, 32);
  uint32_t entry_ptr;
  uint32_t block_nr = offset / 4096;
  entry_ptr = nameHash(object_name, block_nr);
  // loop for linear probing, iterate entil empty or find object.
  for (;;) {
    if (pread(fd, &entry, 32, entry_ptr * 32) == -1) {
      fprintf(stderr, "Error: kvread read fail: %s\n", strerror(errno));
      return -1;
    }
    // if object doesn't exists, return -2.
    if (strcmp((char *)entry.httpname, "") == 0) {
      return -2;
    }
    // if this entry contains correct object, read data
    if (memcmp(entry.httpname, object_name, 20) == 0 &&
        entry.block_nr == block_nr) {
      // if this is block 0, and length greater than 4096, return 4096
      if (block_nr == 0 && entry.length > 4096) {
        pread(fd, data, 4096, entry.datapointer);
        // cout << "length " << 4096 << endl;
        return length;
      } else {
        pread(fd, data, entry.length, entry.datapointer);
        return entry.length;
      }
    }
    // if this entry is not the correct object or empty, find next one
    entry_ptr += 1 * 32;
  }
  return -2;
}

// before process PUT request, use kvinfo to set length
//  if entry object exists, return 0
//  if entry object doesn't exist, return 1 and create it
// before process GET reqeust, use kvinfo to get length
//  if entry object exists, return its Length
//  if entry object doesn't exist, return -2
ssize_t kvinfo(const uint8_t *object_name, ssize_t length) {
  struct kvs_entry entry;
  uint32_t entry_ptr = nameHash(object_name, 0);
  // get length
  if (length == -1) {
    // loop for linear probing, iterate entil empty or find object.
    for (;;) {
      if (pread(fd, &entry, 32, entry_ptr * 32) == -1) {
        fprintf(stderr, "Error: kvinfo read fail: %s\n", strerror(errno));
        return -1;
      }
      // if object doesn't exist, return -2;
      if (strcmp((char *)entry.httpname, "") == 0) {
        return -2;
      }
      // if this entry contains correct object, return its length
      if (memcmp(entry.httpname, object_name, 20) == 0) {
        return entry.length;
      }
      // if this entry is not the correct object, find next one
      entry_ptr += 1 * 32;
    }
  }
  // set length
  if (length >= 0) {
    // loop for linear probing, iterate entil empty or find object.
    for (;;) {
      if (pread(fd, &entry, 32, entry_ptr * 32) == -1) {
        fprintf(stderr, "Error: kvinfo read fail: %s\n", strerror(errno));
        return -1;
      }
      // if object doesn't exist, create it and return 1.
      if (strcmp((char *)entry.httpname, "") == 0) {
        memset(&entry, 0, 32);
        memcpy(entry.httpname, object_name, 20);
        entry.block_nr = 0;
        entry.length = length;
        entry.datapointer = curr_fvs_end;
        curr_fvs_end += 4096;
        if (pwrite(fd, &entry, 32, entry_ptr * 32) == -1) {
          fprintf(stderr, "Error: kvinfo write fail: %s\n", strerror(errno));
          return -1;
        }
        return 1;
      }
      // if this entry contain this object, update length and return 0
      if (memcmp(entry.httpname, object_name, 20) == 0) {
        entry.length = length;
        if (pwrite(fd, &entry, 32, entry_ptr * 32) == -1) {
          fprintf(stderr, "Error: kvinfo write fail: %s\n", strerror(errno));
          return -1;
        }
        return 0;
      }
      // if this entry is not the correct object, find next one
      entry_ptr += 1 * 32;
    }
  }
  return -2;
}

#endif

#ifndef KVS_NAMEMAP_H_
#define KVS_NAMEMAP_H_

#include "city.h"
#include <bits/stdc++.h>
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
#include <vector>

using namespace std;

// initial name map file
// if name map file exists, return 0
// if name map file doesn't exist, create new one, return 1
int32_t init_name_mapping() {
  uint8_t init_entry[128];
  memset(&init_entry, 0, 128);
  size_t offset = 0;
  struct stat st;
  fstat(fd_name_map, &st);
  off_t file_sz = st.st_size;
  if (file_sz == 0) {
    for (int i = 0; i < 10000; ++i) {
      if (pwrite(fd_name_map, &init_entry, 128, offset) == -1) {
        fprintf(stderr, "Error: write fail: %s\n", strerror(errno));
      }
      offset += 128;
    }
    return 1;
  }
  return 0;
}

// Check whether name is 40 hex characters
bool is_40_hex_httpname(const char *name) {
  bool httpname_Bool = 1;
  if (strlen(name) == 40) {
    for (size_t i = 0; i < strlen(name); ++i) {
      if ((name[i] >= '0' && name[i] <= '9') ||
          (name[i] >= 'a' && name[i] <= 'f') ||
          (name[i] >= 'A' && name[i] <= 'F')) {
        httpname_Bool = 1;
      } else {
        httpname_Bool = 0;
      }
    }
  } else {
    httpname_Bool = 0;
  }
  return httpname_Bool;
}

// look up name mapping
// if name mapping exist, return length of existing_name.
// if not exist, return -2.
ssize_t name_lookup(const char *new_name, char *existing_name) {
  uint32_t entry_ptr;
  char names[128];
  memset(names, 0, 128);
  entry_ptr = CityHash32(new_name, strlen(new_name)) % 10000;
  // loop for linear probing
  for (;;) {
    if (pread(fd_name_map, names, 128, entry_ptr * 128) == -1) {
      fprintf(stderr, "Error: kvread read fail: %s\n", strerror(errno));
      return -1;
    }
    char name1[128];
    char name2[128];
    memset(name1, 0, 128);
    memset(name2, 0, 128);
    strcpy(name1, names);
    strcpy(name2, names + strlen(names) + 1);
    if (strcmp(name1, "") == 0) {
      return -2;
    }
    if (strcmp(name1, new_name) == 0) {
      memset(existing_name, 0, 128);
      strcpy(existing_name, name2);
      return strlen(name2);
    }
    entry_ptr += 1 * 128;
    return -2;
  }
}

// add new name map from a new_name to an existing_name
// new_name + ‘\0’ + existing_name + ‘\0’ <= 128 byte
// if add successfully, return size of content stored in
// entry.
// if add names greater than 128 byte, return -1.
// if existing name doesn’t already exist on the server, return -2.
ssize_t name_add(const char *new_name, const char *existing_name) {
  uint32_t entry_ptr;
  char names[128];
  char temp[128];
  memset(names, 0, 128);
  if ((strlen(new_name) + strlen(existing_name) + 2) <= 128) {
    if (name_lookup(existing_name, temp) >= 0 ||
        is_40_hex_httpname(existing_name)) {
      entry_ptr = CityHash32(new_name, strlen(new_name)) % 10000;
      // loop for linear probing
      for (;;) {
        if (pread(fd_name_map, names, 128, entry_ptr * 128) == -1) {
          fprintf(stderr, "Error: kvread read fail: %s\n", strerror(errno));
          return -1;
        }
        char name1[128];
        memset(name1, 0, 128);
        strcpy(name1, names);
        // if this entry doesn't exist, create entry.
        if (strcmp(name1, "") == 0) {
          memset(names, 0, 128);
          strcat(names, new_name);
          strcat(names + strlen(new_name) + 1, existing_name);
          return pwrite(fd_name_map, names, 128, entry_ptr * 128);
        }
        // if this entry exists, update entry.
        if ((strcmp(name1, new_name) == 0)) {
          memset(names, 0, 128);
          strcat(names, new_name);
          strcat(names + strlen(new_name) + 1, existing_name);
          return pwrite(fd_name_map, names, 128, entry_ptr * 128);
        }
        // if this entry is not the correct object or empty, find next.
        entry_ptr += 1 * 128;
      }
    } else {
      return -2;
    }
  } else {
    return -1;
  }
}

// look up name mapping until find valid httpname or empty entry.
// A alias chain can occur. In This situation alias will
//    always map to another alias and never map to a valid httpname.
// Use a vector<string> to check alias chain.
// if find alias chain, return -1.
// if find valid httpname, return its length.
// if find empty entry, return -2.
ssize_t name_find_httpname(const char *new_name, char *existing_name,
                           vector<string> nameList) {
  // new_name is a valid httpname
  if (is_40_hex_httpname(new_name)) {
    strcpy(existing_name, new_name);
    return strlen(existing_name);
  }
  // entry is empty
  if (name_lookup(new_name, existing_name) == -2) {
    return -2;
  }
  // existing_name is a valid httpname
  if (is_40_hex_httpname(existing_name)) {
    return strlen(existing_name);
  }
  // existing_name is alias.
  string s(existing_name);
  // an alias chain is detected
  if (find(nameList.begin(), nameList.end(), s) != nameList.end()) {
    return -1;
  }
  nameList.push_back(s);
  char nextName[128];
  strcpy(nextName, existing_name);
  return name_find_httpname(nextName, existing_name, nameList);
}

#endif

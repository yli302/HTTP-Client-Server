#ifndef __lruCache_h__
#define __lruCache_h__
#include <stdlib.h>
#include <list>
#include <map>
#include <stdio.h>
#include <string.h>

class LRUcache{
  struct block{
    ssize_t sz;
    char data[4096];
    char httpname[4096];
  };

  // set a pair as key
  using key = std::pair<char[4096], ssize_t>;
  using hashmap = std::map<key, block *>;

  hashmap cacheMap;
  std::list<block> cacheList;
  size_t cacheSize;
  block *blocks;

  public:
    /* constructor
     * argument: size of cache
     */
    LRUcache(size_t size){
      cacheSize = size;
      blocks = (block*)malloc(sizeof(block) * size);
      cacheList.assign(blocks, blocks+40);
    }
    void set_block_string(char *a){
      strncpy(blocks[1].httpname, a, 4096);
    }
    char *get_block_stirng(){
      return blocks[1].httpname;
    }
    void freeblocks(){
      free(blocks);
    }
};

#endif

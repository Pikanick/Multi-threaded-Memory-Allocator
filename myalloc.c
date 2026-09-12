#include "myalloc.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct LLnode
{
  size_t size;
  void *header_addr;
  struct LLnode *next;

} LLnode;

LLnode *headfree = NULL;
LLnode *headused= NULL;

struct Myalloc
{
  enum allocation_algorithm aalgorithm;
  int size;
  void *memory;

  // Some other data members you want,
  // such as lists to record allocated/free memory
  pthread_mutex_t mutex;
  struct LLnode *first_block;
  int total_free;
  int total_used;
};

struct Myalloc myalloc;

void initialize_allocator(int _size, enum allocation_algorithm _aalgorithm)
{
  assert(_size > 0);
  myalloc.aalgorithm = _aalgorithm;
  myalloc.size = _size;
  // at the beginning, all free
  myalloc.total_free = myalloc.size; // - 8
  myalloc.total_used = 0;
  myalloc.memory = (void*)malloc(myalloc.size);
  memset(myalloc.memory, 0, _size);
  //// set size in first 8 byte
  //*(__uint8_t*)myalloc.memory = _size - 8;

  // Add some other initialization
  // 0-initialize all
  headfree = (LLnode *)malloc(sizeof(LLnode));
  headfree->size = myalloc.size; // size of the whole memory + 8 bits
  headfree->header_addr = myalloc.memory; // start of the memory
  headfree->next = NULL; 

  headused = (LLnode *)malloc(sizeof(LLnode));
  headused->size = 0; // no used memory
  // very start of memory
  headused->header_addr = myalloc.memory;
  headused->next = NULL;
}

void destroy_allocator()
{
  free(myalloc.memory);

  // Free the linked-list metadata too -- the previous version only freed
  // the memory pool itself and leaked every LLnode ever allocated for
  // the free/used lists.
  LLnode *cur = headfree;
  while (cur != NULL) {
    LLnode *next = cur->next;
    free(cur);
    cur = next;
  }
  headfree = NULL;

  cur = headused;
  while (cur != NULL) {
    LLnode *next = cur->next;
    free(cur);
    cur = next;
  }
  headused = NULL;
}

// These return the fit *node itself* -- not some "previous node"
// encoding of it. (The previous version returned a "prev" pointer using a
// convention that could not distinguish "the fit is headfree itself" from
// "the fit is headfree->next" once the free list had more than one node,
// which is why e.g. allocate() would sometimes hand back a *different*
// free block than the one it had just found: find_firstfit_prev returned
// `headfree` in both cases, and remove_freenode always read that as
// "the fit is headfree->next" whenever headfree->next was non-NULL.)

LLnode *find_firstfit(size_t new_size) {
  size_t real_new_size = new_size + 8;
  for (LLnode *cur = headfree; cur != NULL; cur = cur->next) {
    if (cur->size >= real_new_size) {
      return cur;
    }
  }
  return NULL;
}

LLnode *find_bestfit(size_t new_size) {
  size_t real_new_size = new_size + 8;
  LLnode *best_fit = NULL;
  for (LLnode *cur = headfree; cur != NULL; cur = cur->next) {
    if (cur->size >= real_new_size && (best_fit == NULL || cur->size < best_fit->size)) {
      best_fit = cur;
    }
  }
  return best_fit;
}

LLnode *find_worstfit(size_t new_size) {
  size_t real_new_size = new_size + 8;
  LLnode *worst_fit = NULL;
  for (LLnode *cur = headfree; cur != NULL; cur = cur->next) {
    if (cur->size >= real_new_size && (worst_fit == NULL || cur->size > worst_fit->size)) {
      worst_fit = cur;
    }
  }
  return worst_fit;
}

/*
 * Finds a free node in the free linked list
 * according to the allocation algorithm and
 * makes space for the new node.
 * Returns the memory address of the start of
 * the corresponding block of memory, and (via out_consumed_size) how many
 * bytes -- header included -- were actually taken from the free list for
 * it (normally new_size + 8, but see MIN_USABLE_FREE_CHUNK below).
 */
// A free chunk left with less than this many bytes can never itself hold
// another allocation (it couldn't even fit the 8-byte header), so rather
// than strand it as permanently-unusable, dead space, we hand the whole
// block to the current allocation instead of just splitting off exactly
// what was asked for.
#define MIN_USABLE_FREE_CHUNK 9

void *remove_freenode(size_t new_size, size_t *out_consumed_size) {
  LLnode *fit_node = NULL;
  switch (myalloc.aalgorithm)
  {
  case FIRST_FIT:
    fit_node = find_firstfit(new_size);
    break;
  case BEST_FIT: // satisfies the allocation request from the available memory block that at least as large as the requested size and that results in the smallest remainder fragment.
    fit_node = find_bestfit(new_size);
    break;
  case WORST_FIT: // results in the largest remainder fragment
    fit_node = find_worstfit(new_size);
    break;
  }

  // no fit anywhere in the free list
  if (fit_node == NULL) {
    return NULL;
  }

  void *avail_addr = fit_node->header_addr;
  size_t real_new_size = new_size + 8;
  size_t leftover = fit_node->size - real_new_size; // guaranteed >= 0: fit_node->size >= real_new_size
  size_t consumed_size;

  if (leftover < MIN_USABLE_FREE_CHUNK) {
    // Give the whole block to this allocation instead of leaving an
    // unusable sliver behind.
    consumed_size = fit_node->size;
    if (fit_node == headfree) {
      headfree = headfree->next;
    } else {
      LLnode *prev = headfree;
      while (prev->next != fit_node) {
        prev = prev->next;
      }
      prev->next = fit_node->next;
    }
    free(fit_node);
  } else {
    // Split: shrink the free block down to just the leftover.
    consumed_size = real_new_size;
    fit_node->header_addr += real_new_size;
    fit_node->size -= real_new_size;
  }

  if (out_consumed_size != NULL) {
    *out_consumed_size = consumed_size;
  }
  return avail_addr;
}

// Given a pointer to the found fit block and size requested by user,
// create and insert a corresponding node into the used list.
LLnode* insert_used_node(void* new_addr, size_t new_size) {
  struct LLnode *cur = headused;
  while (cur != NULL)
  {
    // if cur is node to insert after (its block ends exactly where the
    // new block begins)
    if ((cur->header_addr + cur->size) == new_addr)
    {
      // found the node to insert it after
      // need to repoint prev's next to new node
      LLnode *new_node = (LLnode *)malloc(sizeof(LLnode));
      new_node->header_addr = new_addr;
      new_node->size = new_size+8;
      new_node->next = cur->next;
      cur->next = new_node;
      return new_node;
    }
    cur = cur->next;
  }
  // No contiguous predecessor found (e.g. this is the first allocation,
  // or BEST_FIT/WORST_FIT handed back a block that isn't adjacent to any
  // existing used block). List order doesn't matter for correctness here
  // -- only the *free* list needs to stay address-ordered for merging --
  // so just insert it right after the (size-0) sentinel head.
  LLnode *new_node = (LLnode *)malloc(sizeof(LLnode));
  new_node->header_addr = new_addr;
  new_node->size = new_size + 8;
  new_node->next = headused->next;
  headused->next = new_node;
  return new_node;
}

void *allocate(int _size) {
  // The linked lists (headfree/headused) are shared, mutable state, so
  // every function that walks or edits them needs to hold the mutex for
  // its whole critical section -- not just the read-only statistics
  // functions, which is all the previous version protected. Without this,
  // two threads calling allocate()/deallocate() concurrently (see
  // test_threading() in main.c) can interleave their linked-list edits
  // and corrupt them.
  pthread_mutex_lock(&mutex);

  if (myalloc.total_free < (_size + 8))
  {
    printf("Error, allocate(): size requested greater than total free space left in entire available memory space.\n");
    pthread_mutex_unlock(&mutex);
    return NULL;
  }
  // Find a free chunk. consumed_size is normally _size + 8, but
  // remove_freenode may hand over a slightly larger block if splitting
  // off exactly _size+8 would leave an unusably small sliver behind.
  size_t consumed_size = 0;
  void* header_addr = remove_freenode(_size, &consumed_size);
  // if no fit, return NULL
  if (header_addr == NULL) {
    pthread_mutex_unlock(&mutex);
    return NULL;
  }
  // Grab the chunk and make a used node for it
  // REMEMBER to free the node in deallocate after use
  LLnode* new_node = insert_used_node(header_addr, consumed_size - 8);

  myalloc.total_free -= new_node->size;
  myalloc.total_used += new_node->size;

  // store a literal int at the start of the memory chunk
  *((unsigned long*)header_addr) = (unsigned long)new_node->size;

  void *result = new_node->header_addr + 8;
  pthread_mutex_unlock(&mutex);
  return result;
}

/*
 * Finds the node in the used linked list
 * for the block of the memory address given,
 * removes it from the used list and
 * returns it.
 */
LLnode* remove_usednode(void* block_addr) {
  struct LLnode *prev = headused;
  struct LLnode *cur = headused->next;
  // always >= 1 node, size 0 headused
  while (cur != NULL)
  {
    // user access no header
    if (cur->header_addr == (block_addr - 8))
    {
      // remove node from linked list
      prev->next = cur->next;
      // REMEMBER to link into free list
      cur->next = NULL;
      return cur;
    }
    prev = prev->next;
    cur = cur->next;
  }
  // block_addr doesn't match any used block (double free / bad pointer) --
  // let the caller (deallocate) handle this instead of returning garbage.
  return NULL;
}

// Merges any free nodes that are contiguous
void merge_free_contigs() {
  LLnode* cur = headfree->next;
  LLnode* prev = headfree;
  while (cur != NULL)
  {
    // for fragmented but contiguous memory spaces in the free list
    // no nodes before touching, a node touching after, and not first node
    if (prev->header_addr + prev->size == cur->header_addr)
    {
      // merge all the way to the end
      prev->size += cur->size;
      // remove cur from the free list...
      LLnode *to_free = cur;
      prev->next = cur->next;
      cur = cur->next;
      // ...only *after* we've read everything we need from it. The
      // previous version freed cur and then still did `prev = cur;
      // cur = cur->next;`, reading a freed node (use-after-free --
      // this is what was actually crashing allocate()/deallocate()).
      free(to_free);
      // prev stays put: it just absorbed cur's block, so it's still the
      // right node to compare the *new* cur against on the next iteration.
    }
    else
    {
      prev = cur;
      cur = cur->next;
    }
  }
}

// Given a pointer to the LLnode removed from the used list
// for the block the user wants to free, insert it into the free list.
// Calls merge_free_contigs() to merge any contiguous free nodes after the fact.
void insert_free_node(LLnode* prevly_used_node) {
  void* block_addr = prevly_used_node->header_addr;

  // special case no free nodes
  if (headfree == NULL)
  {
    headfree = prevly_used_node;
    return;
  }

  struct LLnode* cur = headfree;
  while (cur != NULL)
  {
    // there are only nodes before
    if (cur->header_addr <= block_addr && cur->next == NULL)
    {
      prevly_used_node->next = cur->next;
      cur->next = prevly_used_node;
      break;
    }
    // general case: right spot is sandwich addresses
    else if (cur->header_addr <= block_addr && cur->next->header_addr >= block_addr) //before & after
    {
      prevly_used_node->next = cur->next;
      cur->next = prevly_used_node;
      break;
    }
    // the new block's address is before the current head's: it becomes
    // the new head, with the *old* head as its ->next.
    else if (headfree->header_addr >= block_addr)
    {
      // (The previous version set this to headfree->next, which skipped
      // the old head entirely -- silently dropping it from the free
      // list for good, without freeing it either. In this allocator,
      // headfree's own address moves forward every time its block gets
      // split from the front, so this branch is very much reachable in
      // practice -- not just a first-node edge case -- and the dropped
      // node showed up as a memory leak.)
      prevly_used_node->next = headfree;
      headfree = prevly_used_node;
      break;
    }
    cur = cur->next;
  }
  merge_free_contigs();
}

void deallocate(void *_ptr)
{
  pthread_mutex_lock(&mutex);

  // myalloc.size includes first size header, but total_free doesn't
  if (myalloc.total_free == myalloc.size)
  {
    printf("Error, deallocate(): no space left to deallocate in entire memory space.\n");
    pthread_mutex_unlock(&mutex);
    return;
  }
  // Find the used node for the chunk
  LLnode* used_node = remove_usednode(_ptr);
  // if none found, done (double free or invalid pointer)
  if (used_node == NULL) {
    pthread_mutex_unlock(&mutex);
    return;
  }
  size_t new_size = used_node->size;
  // Move node back to free list
  // REMEMBER to free the node in allocate after use
  insert_free_node(used_node);

  myalloc.total_free += new_size;
  myalloc.total_used -= new_size;

  pthread_mutex_unlock(&mutex);
}

int compact_allocation(void **_before, void **_after)
{
  // Slide every used chunk down to the front of the pool (in address
  // order, so a chunk is never overwritten before it's been moved),
  // leaving one single contiguous free chunk behind -- instead of the
  // previous version, which ignored the actual allocator state
  // entirely: it treated *_before/*_after as *input* addresses (they're
  // uninitialized output arrays), malloc'd unrelated memory, and called
  // free() on a pointer that was never returned by malloc (undefined
  // behavior), all while never touching headfree/headused at all.
  pthread_mutex_lock(&mutex);

  int count = 0;
  for (LLnode *cur = headused->next; cur != NULL; cur = cur->next) {
    count++;
  }
  if (count == 0) {
    pthread_mutex_unlock(&mutex);
    return 0;
  }

  LLnode **chunks = (LLnode **)malloc(sizeof(LLnode *) * count);
  int idx = 0;
  for (LLnode *cur = headused->next; cur != NULL; cur = cur->next) {
    chunks[idx++] = cur;
  }
  // Insertion sort by address -- count is the number of live allocations,
  // never large enough for this to matter.
  for (int i = 1; i < count; i++) {
    LLnode *key = chunks[i];
    int j = i - 1;
    while (j >= 0 && chunks[j]->header_addr > key->header_addr) {
      chunks[j + 1] = chunks[j];
      j--;
    }
    chunks[j + 1] = key;
  }

  void *cursor = myalloc.memory;
  for (int i = 0; i < count; i++) {
    LLnode *chunk = chunks[i];
    if (_before != NULL) {
      _before[i] = chunk->header_addr + 8; // report user pointers, not raw headers
    }
    if (chunk->header_addr != cursor) {
      // memmove (not memcpy): the source and destination chunks can
      // overlap once several chunks have already been slid down.
      memmove(cursor, chunk->header_addr, chunk->size);
      chunk->header_addr = cursor;
    }
    if (_after != NULL) {
      _after[i] = chunk->header_addr + 8;
    }
    cursor += chunk->size;
  }
  free(chunks);

  // Replace the (now scattered/stale) free list with a single node
  // covering whatever's left after the packed used chunks.
  for (LLnode *cur = headfree; cur != NULL; ) {
    LLnode *next = cur->next;
    free(cur);
    cur = next;
  }
  headfree = NULL;

  size_t remaining = (char *)myalloc.memory + myalloc.size - (char *)cursor;
  if (remaining > 0) {
    headfree = (LLnode *)malloc(sizeof(LLnode));
    headfree->size = remaining;
    headfree->header_addr = cursor;
    headfree->next = NULL;
  }

  pthread_mutex_unlock(&mutex);
  return count;
}

int available_memory()
{
  pthread_mutex_lock(&mutex);
  // total_free counts each free chunk's *raw* size, header included; the
  // largest single allocation that space could actually satisfy is 8
  // bytes (one header) less than that. Clamp at 0 rather than going
  // negative once every last byte is allocated (total_free == 0).
  int result = myalloc.total_free >= 8 ? myalloc.total_free - 8 : 0;
  pthread_mutex_unlock(&mutex);
  return result;
}

void get_statistics(struct Stats* _stat)
{
  pthread_mutex_lock(&mutex);

  _stat->allocated_size = 0;
  _stat->allocated_chunks = 0;
  _stat->free_size = 0;
  _stat->free_chunks = 0;
  _stat->smallest_free_chunk_size = 0;
  _stat->largest_free_chunk_size = 0;

  // Every chunk's raw ->size includes its own 8-byte header; every size
  // reported here is meant to be the *usable* size (raw - 8), which is
  // why this needs to happen per chunk rather than once at the end.

  // headfree has no dummy node -- if it's non-NULL it's itself a real
  // free chunk.
  int min = -1;
  int max = 0;
  for (LLnode *cur = headfree; cur != NULL; cur = cur->next)
  {
    _stat->free_chunks += 1;
    _stat->free_size += (int)cur->size - 8;
    if (min < 0 || (int)cur->size < min) {
      min = (int)cur->size;
    }
    if ((int)cur->size > max) {
      max = (int)cur->size;
    }
  }
  if (min >= 0)
  {
    _stat->smallest_free_chunk_size = min - 8;
    _stat->largest_free_chunk_size = max - 8;
  }

  // headused *is* a dummy size-0 sentinel, so start from ->next.
  for (LLnode *cur = headused->next; cur != NULL; cur = cur->next)
  {
    _stat->allocated_chunks += 1;
    _stat->allocated_size += (int)cur->size - 8;
  }

  pthread_mutex_unlock(&mutex);
}
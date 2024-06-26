#include "myalloc.h"
#include <assert.h>
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

  // free other dynamic allocated memory to avoid memory leak
}

// Finds the previous node of the first fit in the free list.
// Returns NULL if no fit is found or only one node.
void *find_firstfit_prev(size_t new_size) {
  size_t real_new_size = new_size + 8;
  LLnode *cur_node = headfree;
  // default return value if only 1 node
  LLnode *prev_node = headfree;
  // > 1 node in list
  if (headfree->next != NULL)
  {
    while (cur_node->size < real_new_size && cur_node->next != NULL)
    {
      prev_node = cur_node;
      cur_node = cur_node->next;
    }
  }
  // if after traversing, no fit, return NULL
  if (cur_node->next == NULL && cur_node->size < real_new_size)
  {
    return NULL;
  }

  return prev_node;
}

// Finds the previous node of the best fit in the free list.
// Returns NULL if no fit is found or only one node.
void *find_bestfit_prev(size_t new_size) {
  size_t real_new_size = new_size + 8;
  LLnode *cur_node = headfree;
  LLnode *cur_prev = NULL;
  LLnode *best_fit = NULL;
  // default return value if only 1 node
  LLnode *best_prev = headfree;
  if (headfree == NULL){
    return NULL;
  }
   while (cur_node != NULL) // operating on free list to find the smallest block that can accomodate the requested size
  {
    // find the smallest free block that can accommodate the requested size
    // check each memory block and compare the size to the requested size
        if (cur_node->size >= real_new_size &&
        (best_fit == NULL || cur_node->size < best_fit->size))
    {
      best_fit = cur_node;
      if(cur_prev != NULL)
      {
        best_prev = cur_prev;
      }
      
    }
    cur_prev = cur_node;
    cur_node = cur_node->next;
  }

  // if only 1 node, after 1 iteration,
  // cur_prev = headfree

  // if after traversing, no fit, return NULL
  if (cur_node == NULL)
  {
    if (cur_prev->size < real_new_size) {
      return NULL;
    }
  }

  return best_prev;
}

void *find_worstfit_prev(size_t new_size) {
  size_t real_new_size = new_size + 8;
  LLnode *cur_node = headfree;
  LLnode *cur_prev = NULL;
  LLnode *worst_fit = NULL;
  // default return value if only 1 node
  LLnode *worst_prev = headfree;
  while (cur_node != NULL) // operating on free list to find the smallest block that can accomodate the requested size
  {
    // find the smallest free block that can accommodate the requested size
    // check each memory block and compare the size to the requested size
    if (cur_node->size >= real_new_size && (worst_fit == NULL || cur_node->size > worst_fit->size)) // checks it at least fits
    {
      worst_fit = cur_node;        
      worst_prev = cur_prev;
      
    }
    cur_prev = cur_node;
    cur_node = cur_node->next;
  }

  if(worst_fit == NULL && cur_node != NULL && cur_node->size >= real_new_size)
  {
    worst_fit = cur_node;
    worst_prev = cur_prev;
  }
  // if after traversing, no fit, return NULL
  if (cur_node == NULL)
  {
    if (cur_prev->size < real_new_size) {
      return NULL;
    }
  }

  return worst_prev;
}

/*
 * Finds a free node in the free linked list
 * according to the allocation algorithm and
 * makes space for the new node.
 * Returns the memory address of the start of
 * the corresponding block of memory.
 */
void *remove_freenode(size_t new_size) {
  void* avail_addr = NULL;
  LLnode* fit_prev = NULL;
  LLnode* fit_node = NULL;
  switch (myalloc.aalgorithm)
  {
  case FIRST_FIT:
    fit_prev = find_firstfit_prev(new_size);
    break;
  case BEST_FIT: // satisfies the allocation request from the available memory block that at least as large as the requested size and that results in the smallest remainder fragment.
    fit_prev = find_bestfit_prev(new_size);
    break;
  case WORST_FIT: // results in the largest remainder fragment
                  //  find the smallest free block that can accommodate the requested size
    // check each memory block and compare the size to the requested size
    fit_prev = find_worstfit_prev(new_size);
    break;
  }

  // copy out the address of the free block
  // only 1 node
  //
  if (fit_prev == headfree && fit_prev->next == NULL) {
    fit_node = fit_prev;
    avail_addr = fit_prev->header_addr;
  }
  else if (fit_prev == headfree && fit_prev->next != NULL) {
    fit_node = fit_prev->next;
    avail_addr = fit_node->header_addr;
  }
  // no fit
  else if (fit_prev == NULL) {
    return NULL;
  }
  else {
    fit_node = fit_prev->next;
    avail_addr = fit_node->header_addr;
  }

  // to allocate the memory block in the worst fit block and split the block if necessary
  // there is a fit
  if (fit_prev != NULL)
  {
    size_t real_new_size = new_size + 8;
    // Shrink in 2 cases: only headfree, or
    // requested size < fit_node->size

    // when only 1 node, just returning headfree as prev
    // even though it's the fit node itself
    if (fit_prev == headfree && fit_prev->next == NULL) {
      // shrink from very (right) end of chunk
      headfree->header_addr += real_new_size;
      headfree->size -= real_new_size;
    }
    else if (fit_node->size > real_new_size)
    {
      // shrink from very (left) beginning of chunk
      fit_node->header_addr += real_new_size;
      fit_node->size -= real_new_size;
    }
    else
    {
      // Just remove node
      fit_prev->next = fit_node->next;
      free(fit_node);
    }
  }
  // return the actual memory address of the free block
  return avail_addr;
}

// Given a pointer to the found fit block and size requested by user,
// create and insert a corresponding node into the used list.
LLnode* insert_used_node(void* new_addr, size_t new_size) {
  struct LLnode *cur = headused;
  while (cur != NULL)
  {
    // if cur is node to insert after
    // 0 indexed so actually already 1 past
    //printf("cur->header_addr: %p, new_addr: %p\n", cur->header_addr, new_addr);
    //printf("*(cur->header_addr): %c, *new_addr: %c\n", *((char*)cur->header_addr), *((char*)new_addr));
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
}

void *allocate(int _size) {   
  if (myalloc.total_free < (_size + 8))
  {
    printf("Error, allocate(): size requested greater than total free space left in entire available memory space.\n");
    return NULL;
  }
  // Find a free chunk
  void* header_addr = remove_freenode(_size);
  // if no fit, return NULL
  if (header_addr == NULL) {
    return NULL;
  }
  // Grab the chunk and make a used node for it
  // REMEMBER to free the node in deallocate after use
  LLnode* new_node = insert_used_node(header_addr, _size);

  myalloc.total_free -= new_node->size;
  myalloc.total_used += new_node->size;

  // // check if after allocating, less than enough for another single chunk
  // if (myalloc.total_free < 9) {
  //   printf("Error, allocate(): size requested greater than total free space left in entire available memory space.\n");
  //   return NULL;
  // }

  // store a literal int at the start of the memory chunk
  *((unsigned long*)header_addr) = (unsigned long)new_node->size;

  // check real_new_size
  return new_node->header_addr + 8;
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
    // 0 indexed so actually already 1 past
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
      // remove cur->next from free list
      prev->next = cur->next;
      free(cur);
    }
    prev = cur;
    cur = cur->next;
  }
}

// Given a pointer to the LLnode removed from the used list
// for the block the user wants to free, insert it into the free list.
// Calls merge_free_contigs() to merge any contiguous free nodes after the fact.
void insert_free_node(LLnode* prevly_used_node) {
  void* block_addr = prevly_used_node->header_addr;
  size_t block_size = prevly_used_node->size;

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
    // there are only nodes after
    else if (headfree->header_addr >= block_addr) 
    {
      prevly_used_node->next = headfree->next;
      headfree = prevly_used_node;
      break;
    }
    cur = cur->next;
  }
  merge_free_contigs();
}

void deallocate(void *_ptr)
{
  // myalloc.size includes first size header, but total_free doesn't
  if (myalloc.total_free == myalloc.size)
  {
    printf("Error, deallocate(): no space left to deallocate in entire memory space.\n");
    return;
  }
  // Find the used node for the chunk
  LLnode* used_node = remove_usednode(_ptr);
  // if none found, done
  if (used_node == NULL) {
    return;
  }
  size_t new_size = used_node->size;
  // Move node back to free list
  // REMEMBER to free the node in allocate after use
  insert_free_node(used_node);

  myalloc.total_free += new_size;
  myalloc.total_used -= new_size;
}

int compact_allocation(void **_before, void **_after)
{ 
//   int i=0;
//   struct LLnode* cur1 = headused;
//   while (cur1->next != NULL)
//   {
//     _before[i] = cur1->header_addr;
//     cur1 = cur1->next;
//     i++;
//   }
//   cur1 = headused;
//   struct LLnode* cur2 = headfree;
//   while (cur2->next != NULL & cur1->next != NULL)
//   {
//     _before[i] = cur2->header_addr;
//     cur1 = cur1->next;
//     cur2 = cur2->next;
//     i++;
//   }

  int compacted_size = 0;
  // Calculate the total size of the allocation
    size_t allocation_size = (char *)*_after - (char *)*_before;

    // Allocate a new memory block to hold the compacted allocation
    void *new_allocation = malloc(allocation_size);
    if (new_allocation == NULL) {
        // Allocation failed
        return -1;
    }
  
    // Copy the used memory to the beginning of the new allocation
    void *current_ptr = *_before;
    void *new_ptr = new_allocation;
    while (current_ptr < *_after) {
        memcpy(new_ptr, current_ptr, sizeof(void *));
        current_ptr += sizeof(void *);
        new_ptr += sizeof(void *);
    }

    // Free the original allocation
    free(*_before);

    // Update the _before and _after pointers to point to the new allocation
    *_before = new_allocation;
    *_after = (char *)new_allocation + allocation_size;

    return 0;
  // compact allocated memory
  // update _before, _after and compacted_size

  return compacted_size;
}

int available_memory()
{
  pthread_mutex_lock(&mutex);
  // int available_memory_size = 0;
  // // Calculate available memory size
  // available_memory_size = myalloc.size - 8;

  // struct LLnode* cur1 = headfree;
  // while (cur1 != NULL)
  // {
  //   available_memory_size += cur1->size;
  //   cur1 = cur1->next;
  // }
  pthread_mutex_unlock(&mutex);
  // return available_memory_size;
  return myalloc.total_free - 8;
}

void get_statistics(struct Stats* _stat)
{
  
  pthread_mutex_lock(&mutex);
  // Populate struct Stats with the statistics
  _stat->allocated_size = 0; // IS THIS WITHOUT NODES OR USED LIST?
  _stat->allocated_chunks = 0; // used list
  _stat->free_size = 0;
  _stat->free_chunks = 0;
  _stat->smallest_free_chunk_size = myalloc.size-8;
  _stat->largest_free_chunk_size = myalloc.size-8;

  // special case no free nodes
  if (headfree == NULL)
  {
    _stat->free_size = myalloc.total_free-8;
    _stat->free_chunks = 1;
  }

  // special case no used nodes
  if (headused->next == NULL)
  {
    _stat->allocated_chunks = 0;
    _stat->allocated_size=0;
  }

  struct LLnode* cur1 = headfree;
  while (cur1 != NULL)
  {
    _stat->free_chunks += 1;
    _stat->free_size += cur1->size;
    cur1 = cur1->next;
  }

  // first node in used list is dummy (size 0)
  struct LLnode* cur2 = headused;
  while (cur2->next != NULL)
  {
    _stat->allocated_chunks += 1;
    _stat->allocated_size += cur2->size;
    cur2 = cur2->next;
  }

  cur1 = headfree;
  LLnode* cur_prev = NULL;
  LLnode* largest_free_chunk_size = NULL;
  LLnode* smallest_free_chunk_size = NULL;
  // default return value if only 1 node
  LLnode* best_prev = headfree;
  int min = myalloc.size;
  int max = 0;
  while (cur1 != NULL)
  {

    // If min is greater than head->data then
    // assign value of head->data to min
    // otherwise node point to next node.
    if (min > cur1->size)
    {
      min = cur1->size;
    }

    if (max < cur1->size)
    {
      max = cur1->size;
    }
    cur1 = cur1->next;
  }
  _stat->smallest_free_chunk_size = min-8;
  _stat->largest_free_chunk_size = max-8;
  pthread_mutex_unlock(&mutex);
}
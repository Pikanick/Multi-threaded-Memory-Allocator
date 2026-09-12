# Multi-threaded-Memory-Allocator
This project demonstrates custom memory allocation strategies in C. It includes the implementation of a simple memory allocator, `myalloc`, and a program to test its functionality. The project illustrates how memory allocation can be managed in a constrained environment and visualizes the memory blocks through a series of images. The allocator supports First Fit, Best Fit, and Worst Fit algorithms for memory allocation.

## Files

- `main.c`: Contains the main function to test the memory allocation functions.
- `myalloc.c`: Implements the custom memory allocator.
- `myalloc.h`: Header file for the custom memory allocator.
- `Makefile`: Makefile to compile the project.
- `README.md`: This readme file.
- `memory1.png`, `memory2.png`, `memory3.png`: Images demonstrating memory allocation states.

## Memory Allocation Images

The provided images illustrate the memory allocation process:

1. **memory1.png**: Initial state of memory blocks.
2. **memory2.png**: Intermediate state showing memory after some allocations and deallocations.
3. **memory3.png**: Final state after merging free blocks.
## Features
- Initialization of the memory allocator.
- Allocation and deallocation interfaces.
- Metadata management.
- Compaction support.
- Statistics reporting.
- Multi-threading support.
- Uninitialization.

## Compilation and Execution

To compile and run the project, use the provided Makefile:

```bash
make
./myalloc
```

`./myalloc` runs the whole test suite in `main.c` and prints
`----TEST_RESULT_Success: N/N` at the end. Pass two arguments to run a
single case: `./myalloc <algorithm> <test>`, where `<algorithm>` is
0=FIRST_FIT, 1=BEST_FIT, 2=WORST_FIT, and `<test>` is 0=basic space
allocation, 1=algorithm-specific behavior, 2=compaction, 3=threading.

## Testing

```bash
./run_tests.sh          # build + run the test suite, fail if any test fails
./run_tests.sh --all    # also run under ASan+UBSan and ThreadSanitizer
```

## Implementation Details

### myalloc.c

This file contains the implementation of the custom memory allocator. The allocator manages a fixed-size memory pool and provides functions for memory allocation and deallocation.

### myalloc.h

This header file declares the functions and structures used by the custom memory allocator.

### main.c

This file contains the main function that tests the memory allocator. It includes various test cases to ensure the allocator functions correctly under different scenarios.

## Custom Memory Allocation Strategy

The custom allocator uses a simple first-fit strategy to allocate memory blocks. It maintains a list of free and allocated blocks and merges adjacent free blocks during deallocation to reduce fragmentation.

## Usage

1. Include `myalloc.h` in your project.
2. Call `initialize_allocator()` once with a pool size and a fit
   algorithm, then use `allocate()`/`deallocate()` against that pool.
   (The actual function names are `allocate`/`deallocate`, not
   `myalloc`/`myfree`.)

## Example

Here is a simple example of how to use the custom memory allocator:

```c
#include "myalloc.h"

int main() {
    initialize_allocator(100, FIRST_FIT);

    void *ptr1 = allocate(20);
    void *ptr2 = allocate(10);
    deallocate(ptr1);
    void *ptr3 = allocate(5);
    deallocate(ptr2);
    deallocate(ptr3);

    destroy_allocator();
    return 0;
}
```

## Thread safety

`allocate()`/`deallocate()`/`compact_allocation()` hold a single mutex for
their entire critical section, so concurrent calls from multiple threads
(see `test_threading()` in `main.c`) don't corrupt the free/used lists.
Verified with ThreadSanitizer via `./run_tests.sh --tsan`.

## Conclusion

This project demonstrates a basic custom memory allocator in C and provides a visual representation of the memory allocation process. It serves as a useful learning tool for understanding memory management at a lower level.


## License

This project is licensed under the MIT License

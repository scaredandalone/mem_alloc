### Memory Allocator Interpretation

This is my memory allocator interpretation, based on the Memory APIs section of Operating Systems: Three Easy Pieces (malloc).  

It implements a singly linked list to track all allocations and free segments within the heap.  

It uses the Windows system call VirtualAlloc() to request additional memory from the operating system when needed.  

A header block (struct) is appended to every memory segment to store the size of the segment, whether it is free or not, and a pointer to the next segment.  

Each free block also contains a next_free pointer so that all free blocks can be tracked separately in a free list.  

Free blocks are coalesced both forwards and backwards to form larger free segments and reduce fragmentation.  

Block splitting is supported so that allocations do not take up more space than required.  

There is also basic safety such as double-free detection and 16-byte alignment.  







#### SWITCH TO MASTER BRANCH TO VIEW THE SOURCE!

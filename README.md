# LockfreeSPSCQueue

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/p-id/spscq/master/LICENSE)

This is header only, C implementation of a lockfree single producer single consumer bounded queue for concurrent programming.

This implementation is best suited for heavy data movement between single producer and single consumer, typical application scenarios are video streaming pipeline and for sharing data across CPUs. The implementation ensures there are no additional memory copies while sharing the data between threads.

The code has been battle tested in production for quite sometime, but do report issues if observed.
# Example

```c
#define ELEMENT_TYPE payload_t*
#define SPSC_QUEUE_SIZE 8192

spsc_queue_t queue;

// producer thread
payload_t* payload_in = NULL;
payload_in = malloc(sizeof(payload_t)); // alternatively get payload blob from object-pool
load_data(payload_in);
enqueue(&queue, payload_in);

// consumer thread
payload_t* payload_out = NULL;
dequeue(&queue, &payload_out);
consume_func(payload_out);
free(payload_out); // alternatively return payload_out back to object-pool
```

Only a single writer thread can perform enqueue operations and only a single reader thread can perform dequeue operations. Any other usage is invalid.

However, one can bundle the SPSC queue implementation with provided spin-lock implementation.
As a note of caution the current spin-lock implementation uses a lot of assembly code which might need a bit of porting effort [Ran in production handling ~100k requests per second on Intel x86_64 Dell hardware]

# Implementation

The underlying implementation uses [ring buffer](https://en.wikipedia.org/wiki/Circular_buffer).

Care has been taken to make sure to avoid any issues with [false sharing](https://en.wikipedia.org/wiki/False_sharing).

The algorithm is optimized further to use an adaptive [B-Tree style] lookahead for queue head/tail indexes to avoid cache-line faults.

References:

- *Intel*. [Avoiding and Identifying False Sharing Among Threads](https://software.intel.com/en-us/articles/avoiding-and-identifying-false-sharing-among-threads).
- *B-Queue*. [B-Queue: Efficient and Practical Queuing for Fast Core-to-Core Communication](http://staff.ustc.edu.cn/~bhua/publications/IJPP_draft.pdf)

## See Also
* [Folly MPMC Queue](https://github.com/facebook/folly/blob/master/folly/MPMCQueue.h) Open source by Facebook
* [Elle](https://github.com/infinit/elle) C++14 asynchronous/coroutine framework used by Infiniti distributed-encrypted filesystem
* [SeaStar](https://github.com/scylladb/seastar) non-blocking asynchronous event-driven C++14 framework used by NOSQL ScyllaDB (see [tutorial](https://github.com/scylladb/seastar/blob/master/doc/tutorial.md))
* [Boson](https://github.com/duckie/boson) C++14 asynchronous/coroutine framework similar to Go channels
* [MC FastFlow](http://sourceforge.net/projects/mc-fastflow/) **M**ulti-**C**ore friendly framework in old C++98 (published in 2009) and still maintained

# About

This project was created by [Piyush Dewnani]
<[piyush@dewnani.net](mailto:piyush@dewnani.net)>.

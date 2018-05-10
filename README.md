##LockfreeSPSCQueue

This is header only, c implementation of a lockfree single producer single consumer bounded queue for concurrent programming.

This implementation is best suited for heavy data movement between single producer and single consumer, typical application scenarios are video streaming pipeline and for sharing data across CPUs. The implementaiton ensures there are no additional memory copies while sharing the data between threads.

The code has been battle tested in production for quite sometime, but do report issues if observed. The current uses a lot of
assembly low-level stuff which might need a bit of porting effort as the current supported platform is 64-bit only.

The code uses a few tricks like special handling of CPU cache-line and user-space implementation of spin-locks.
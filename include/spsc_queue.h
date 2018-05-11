// SPSC-Queue -- An efficient and practical queueing for fast core-to-core communication
// Bounded buffer circular queue, to be gaurded with external locks [no dynamic memory allocation involved]
#ifndef spsc_queue_h_
#define spsc_queue_h_

#include <stdint.h> // for uint32_t
#include <string.h> // for memset(...)

#ifndef SPSC_QUEUE_SIZE
    #define SPSC_QUEUE_SIZE (1024 * 8)
#endif

#ifndef ELEMENT_TYPE
    #define ELEMENT_TYPE uint32_t
#endif
static ELEMENT_TYPE SPSC_QUEUE_ELEMENT_ZERO = 0;

#define SPSC_BATCH_SIZE (SPSC_QUEUE_SIZE/16)
#define SPSC_BATCH_INCREAMENT (SPSC_BATCH_SIZE/2)
#define SPSC_CONGESTION_PENALTY (1000) // spin-cycles

#define SPSC_OP_SUCCESS 0
#define SPSC_Q_FULL -1
#define SPSC_Q_EMPTY -2


typedef struct spsc_queue {
    volatile        uint32_t        head;                               // Mostly accessed by producer.
    volatile        uint32_t        batch_head;
    volatile        uint32_t        tail __attribute__ ((aligned(64))); // Mostly accessed by consumer.
    volatile        uint32_t        batch_tail;
    unsigned long   batch_history;

#ifdef SPSC_Q_UNDER_TEST
    uint64_t        start_c __attribute__ ((aligned(64)));
    uint64_t        stop_c;
#endif

    ELEMENT_TYPE data[SPSC_QUEUE_SIZE] __attribute__ ((aligned(64))); // accessed by both producer and comsumer
} __attribute__ ((aligned(64))) spsc_queue_t;

static inline uint64_t _read_tsc() {
        uint64_t        time;
        uint32_t        msw   , lsw;
        __asm__         __volatile__("rdtsc\n\t"
                        "movl %%edx, %0\n\t"
                        "movl %%eax, %1\n\t"
                        :         "=r"         (msw), "=r"(lsw)
                        :
                        :         "%edx"      , "%eax");
        time = ((uint64_t) msw << 32) | lsw;
        return time;
}

static inline void _wait_ticks(uint64_t ticks) {
        uint64_t        current_time;
        uint64_t        time = _read_tsc();
        time += ticks;
        do {
                current_time = _read_tsc();
        } while (current_time < time);
}

static void queue_init(spsc_queue_t* this) {
	memset(this, 0, sizeof(spsc_queue_t));
    this->batch_history = SPSC_BATCH_SIZE;
}

static inline int front_peek(spsc_queue_t* this, ELEMENT_TYPE* value) {
	if ( this->data[this->tail] ) {
    	*value = this->data[this->tail];
        return SPSC_OP_SUCCESS;
    }
    return SPSC_Q_EMPTY;
}

static inline int dequeue(spsc_queue_t* this, ELEMENT_TYPE* pValue) {
    uint32_t tmp_tail;
    unsigned long batch_size = this->batch_history;
    *pValue = SPSC_QUEUE_ELEMENT_ZERO;

    // try to zero-in on next batch tail
    if( this->tail == this->batch_tail ) {
        tmp_tail = this->tail + SPSC_BATCH_SIZE;
        if ( tmp_tail >= SPSC_QUEUE_SIZE ) {
                tmp_tail = 0;
                if (this->batch_history < SPSC_BATCH_SIZE) {
                    this->batch_history =
                        (SPSC_BATCH_SIZE < (this->batch_history + SPSC_BATCH_INCREAMENT))?
                        SPSC_BATCH_SIZE : (this->batch_history + SPSC_BATCH_INCREAMENT);
                }
        }

        batch_size = this->batch_history;
        while (!(this->data[tmp_tail])) {
            _wait_ticks(SPSC_CONGESTION_PENALTY);

            batch_size = batch_size >> 1;
            if( batch_size >= 0 ) {
                    tmp_tail = this->tail + batch_size;
                    if (tmp_tail >= SPSC_QUEUE_SIZE) {
                        tmp_tail = 0;
                    }
            } else {
                return SPSC_Q_EMPTY;
            }
        }
        this->batch_history = batch_size;

        if ( tmp_tail == this->tail ) {
            tmp_tail = (tmp_tail + 1) >= SPSC_QUEUE_SIZE ?
                    0 : tmp_tail + 1;
        }
        this->batch_tail = tmp_tail;
    }

    // actually pull-out data-element
    *pValue = this->data[this->tail];
    this->data[this->tail] = SPSC_QUEUE_ELEMENT_ZERO;
    this->tail ++;
    if ( this->tail >= SPSC_QUEUE_SIZE ) {
            this->tail = 0;
    }

    return SPSC_OP_SUCCESS;
}

static inline int enqueue(spsc_queue_t* this, ELEMENT_TYPE value) {
    uint32_t tmp_head;

    // try to zero-in on next batch head
    if( this->head == this->batch_head ) {
        tmp_head = this->head + SPSC_BATCH_SIZE;
        if ( tmp_head >= SPSC_QUEUE_SIZE )
                tmp_head = 0;
        if ( this->data[tmp_head] ) {
            // run spin cycle penality
            _wait_ticks(SPSC_CONGESTION_PENALTY);
            return SPSC_Q_FULL;
        } else {
            this->batch_head = tmp_head;
        }
    }

    this->data[this->head] = value;
    this->head ++;
    if ( this->head >= SPSC_QUEUE_SIZE ) {
            this->head = 0;
    }

    return SPSC_OP_SUCCESS;
}

#endif // cs_queue_h_

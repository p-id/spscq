// CS-Queue -- An efficient and practical queueing for fast core-to-core communication
// Bounded buffer circular queue, to be gaurded with external locks [no dynamic memory allocation involved]
#ifndef cs_queue_h_
#define cs_queue_h_

#define ELEMENT_TYPE void
#define CS_QUEUE_SIZE (16384 * 8)

static ELEMENT_TYPE* CS_QUEUE_ELEMENT_ZERO = NULL;

typedef struct cs_queue {
    volatile	uint32_t	volatile head;                                 // Mostly accessed by producer.
	volatile 	uint32_t	volatile tail __attribute__ ((aligned(64)));  // Mostly accessed by consumer.
    ELEMENT_TYPE* data[CS_QUEUE_SIZE]; // accessed by both producer and comsumer
} cs_queue_t;

static void queue_init(cs_queue_t* q) {
	memset(q, 0, sizeof(cs_queue_t));
}

static inline ELEMENT_TYPE* front_peek(cs_queue_t* q) {
    ELEMENT_TYPE* value = CS_QUEUE_ELEMENT_ZERO;

	if ( q->data[q->tail] ) {
    	value = q->data[q->tail];
    }

	return value;
}

static inline ELEMENT_TYPE* dequeue(cs_queue_t* q) {
    ELEMENT_TYPE* value = CS_QUEUE_ELEMENT_ZERO;

	if ( q->data[q->tail] ) {
    	value = q->data[q->tail];
    	q->data[q->tail] = CS_QUEUE_ELEMENT_ZERO;
    	q->tail ++;
    	if ( q->tail >= CS_QUEUE_SIZE )
    		q->tail = 0;
    }

	return value;
}

static inline int enqueue(cs_queue_t* q, ELEMENT_TYPE* value) {
    int r_value = 0;

	if (q->data[q->head] == CS_QUEUE_ELEMENT_ZERO) {
    	q->data[q->head] = value;
    	q->head ++;
    	if ( q->head >= CS_QUEUE_SIZE ) {
    		q->head = 0;
    	}
    } else {
        r_value = 1;
    }

	return r_value;
}

#endif // cs_queue_h_

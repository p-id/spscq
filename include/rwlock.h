// Read-write spin locks ported from Linux kernel
#ifndef rwlock_h_
#define rwlock_h_

#include <immintrin.h>
#include <string.h>

#include "atomic.h"

#define __TICKET_LOCK_INC		1
#define TICKET_LOCK_INC			((__ticket_t)__TICKET_LOCK_INC)
#define TICKET_SHIFT			(sizeof(__ticket_t) * 8)

typedef unsigned char  __ticket_t;
typedef unsigned short __ticketpair_t;

struct __raw_tickets {
	volatile __ticket_t head, tail;
};

typedef struct arch_spinlock {
	union {
		__ticketpair_t head_tail;
		struct __raw_tickets tickets;
	};
} arch_spinlock_t;

static inline void arch_spin_lock(arch_spinlock_t *lock) {
	register struct __raw_tickets inc;

	inc.head = 0;
	inc.tail = TICKET_LOCK_INC;

	inc = xadd(&lock->tickets, inc);
	if (likely(inc.head == inc.tail))
		goto out;

	for (;;) {
		inc.head = lock->tickets.head;
		if (inc.head == inc.tail)
			goto out;
		_mm_pause();
	}
out:
	barrier();	/* make sure nothing creeps before the lock is taken */
}

static inline void arch_spin_unlock(arch_spinlock_t *lock) {
	__add(&lock->tickets.head, TICKET_LOCK_INC);
}

typedef struct qrwlock {
	atomic_t		cnts;
	arch_spinlock_t		lock;
} rwlock_t;

/*
 * Writer states & reader shift and bias
 */
#define	_QW_WAITING	1		/* A writer is waiting	   */
#define	_QW_LOCKED	0xff		/* A writer holds the lock */
#define	_QW_WMASK	0xff		/* Writer mask		   */
#define	_QR_SHIFT	8		/* Reader count shift	   */
#define _QR_BIAS	(1U << _QR_SHIFT)

static inline void
rwlock_init(rwlock_t *lock)
{
	memset(lock, 0, sizeof(*lock));
}

static inline void
rspin_until_writer_unlock(struct qrwlock *lock, unsigned int cnts)
{
	while ((cnts & _QW_WMASK) == _QW_LOCKED) {
		_mm_pause();
		cnts = atomic_read(&lock->cnts);
	}
}

static inline void queue_write_lock_slowpath(struct qrwlock *lock) {
	int cnts;

	/* Put the writer into the wait queue */
	arch_spin_lock(&lock->lock);

	/* Try to acquire the lock directly if no reader is present */
	if (!atomic_read(&lock->cnts) &&
	    (atomic_cmpxchg(&lock->cnts, 0, _QW_LOCKED) == 0))
		goto unlock;

	/*
	 * Set the waiting flag to notify readers that a writer is pending,
	 * or wait for a previous writer to go away.
	 */
	for (;;) {
		cnts = atomic_read(&lock->cnts);
		if (!(cnts & _QW_WMASK) &&
		    (atomic_cmpxchg(&lock->cnts, cnts, cnts | _QW_WAITING) == cnts))
			break;

		_mm_pause();
	}

	/* When no more readers, set the locked flag */
	for (;;) {
		cnts = atomic_read(&lock->cnts);
		if ((cnts == _QW_WAITING) &&
		    (atomic_cmpxchg(&lock->cnts, _QW_WAITING,
				    _QW_LOCKED) == _QW_WAITING))
			break;

		_mm_pause();
	}
unlock:
	arch_spin_unlock(&lock->lock);
}

static inline void queue_read_lock_slowpath(struct qrwlock *lock) {
	unsigned int cnts;

	atomic_sub(_QR_BIAS, &lock->cnts);

	/*
	 * Put the reader into the wait queue
	 */
	arch_spin_lock(&lock->lock);

	/*
	 * At the head of the wait queue now, wait until the writer state
	 * goes to 0 and then try to increment the reader count and get
	 * the lock. It is possible that an incoming writer may steal the
	 * lock in the interim, so it is necessary to check the writer byte
	 * to make sure that the write lock isn't taken.
	 */
	while (atomic_read(&lock->cnts) & _QW_WMASK)
		_mm_pause();

	cnts = atomic_add_return(_QR_BIAS, &lock->cnts) - _QR_BIAS;
	rspin_until_writer_unlock(lock, cnts);

	/*
	 * Signal the next one in queue to become queue head
	 */
	arch_spin_unlock(&lock->lock);
}

static inline void write_lock(rwlock_t *lock) {
	/* Optimize for the unfair lock case where the fair flag is 0. */
	if (atomic_cmpxchg(&lock->cnts, 0, _QW_LOCKED) == 0)
		return;

	queue_write_lock_slowpath(lock);
}

static inline void write_unlock(rwlock_t *lock) {
	/*
	 * If the writer field is atomic, it can be cleared directly.
	 * Otherwise, an atomic subtraction will be used to clear it.
	 */
	atomic_sub(_QW_LOCKED, &lock->cnts);
}

static inline void read_lock(rwlock_t *lock) {
	unsigned int cnts;

	cnts = atomic_add_return(_QR_BIAS, &lock->cnts);
	if (likely(!(cnts & _QW_WMASK)))
		return;

	/* The slowpath will decrement the reader count, if necessary. */
	queue_read_lock_slowpath(lock);
}

static inline void read_unlock(rwlock_t *lock) {
	atomic_sub(_QR_BIAS, &lock->cnts);
}

#define write_lock_bh(l)	write_lock(l)
#define write_unlock_bh(l)	write_unlock(l)
#define read_lock_bh(l)		read_lock(l)
#define read_unlock_bh(l)	read_unlock(l)

#endif // rwlock_h_

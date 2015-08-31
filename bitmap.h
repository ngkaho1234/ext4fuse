#ifndef BITMAP_H
#define BITMAP_H

#include "types/ext4_basic.h"

#define BITS_PER_LONG __WORDSIZE

#define BIT_MASK(nr)        (1UL << ((nr) % BITS_PER_LONG))
#define BIT_WORD(nr)        ((nr) / BITS_PER_LONG)
#define BITOP_WORD(nr)      ((nr) / BITS_PER_LONG)

#define ext4_test_and_set_bit        test_and_set_bit
#define ext4_set_bit            set_bit
#define ext4_test_and_clear_bit        test_and_clear_bit
#define ext4_clear_bit            clear_bit
#define ext4_test_bit            test_bit
#define ext4_find_next_zero_bit        find_next_zero_bit
#define ext4_find_next_bit        find_next_bit

/**
 * __ffs - find first bit in word.
 * @word: The word to search
 *
 * Undefined if no bit exists, so code should check against 0 first.
 */
static inline unsigned long __ffs(unsigned long word)
{
    int num = 0;

#if BITS_PER_LONG == 64
    if ((word & 0xffffffff) == 0) {
        num += 32;
        word >>= 32;
    }
#endif
    if ((word & 0xffff) == 0) {
        num += 16;
        word >>= 16;
    }
    if ((word & 0xff) == 0) {
        num += 8;
        word >>= 8;
    }
    if ((word & 0xf) == 0) {
        num += 4;
        word >>= 4;
    }
    if ((word & 0x3) == 0) {
        num += 2;
        word >>= 2;
    }
    if ((word & 0x1) == 0)
        num += 1;
    return num;
}

/*
 * ffz - find first zero in word.
 * @word: The word to search
 *
 * Undefined if no zero exists, so code should check against ~0UL first.
 */
static inline unsigned long ffz(unsigned long x)
{
	return __ffs(~(x));
}

/**
 * set_bit - Atomically set a bit in memory
 * @nr: the bit to set
 * @addr: the address to start counting from
 *
 * This function is atomic and may not be reordered.  See __set_bit()
 * if you do not require the atomic guarantees.
 *
 * Note: there are no guarantees that this function will not be reordered
 * on non x86 architectures, so if you are writing portable code,
 * make sure not to rely on its reordering guarantees.
 *
 * Note that @nr may be almost arbitrarily large; this function is not
 * restricted to acting on a single-word quantity.
 */
static inline void set_bit(int nr, volatile unsigned long *addr)
{
    unsigned long mask = BIT_MASK(nr);
    unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);

    *p  |= mask;
}

/**
 * clear_bit - Clears a bit in memory
 * @nr: Bit to clear
 * @addr: Address to start counting from
 *
 * clear_bit() is atomic and may not be reordered.  However, it does
 * not contain a memory barrier, so if it is used for locking purposes,
 * you should call smp_mb__before_atomic() and/or smp_mb__after_atomic()
 * in order to ensure changes are visible on other processors.
 */
static inline void clear_bit(int nr, volatile unsigned long *addr)
{
    unsigned long mask = BIT_MASK(nr);
    unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);

    *p &= ~mask;
}

/**
 * change_bit - Toggle a bit in memory
 * @nr: Bit to change
 * @addr: Address to start counting from
 *
 * change_bit() is atomic and may not be reordered. It may be
 * reordered on other architectures than x86.
 * Note that @nr may be almost arbitrarily large; this function is not
 * restricted to acting on a single-word quantity.
 */
static inline void change_bit(int nr, volatile unsigned long *addr)
{
    unsigned long mask = BIT_MASK(nr);
    unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);

    *p ^= mask;
}

/**
 * test_and_set_bit - Set a bit and return its old value
 * @nr: Bit to set
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.
 * It may be reordered on other architectures than x86.
 * It also implies a memory barrier.
 */
static inline int test_and_set_bit(int nr, volatile unsigned long *addr)
{
    unsigned long mask = BIT_MASK(nr);
    unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);
    unsigned long old;

    old = *p;
    *p = old | mask;

    return (old & mask) != 0;
}

/**
 * test_and_clear_bit - Clear a bit and return its old value
 * @nr: Bit to clear
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.
 * It can be reorderdered on other architectures other than x86.
 * It also implies a memory barrier.
 */
static inline int test_and_clear_bit(int nr, volatile unsigned long *addr)
{
    unsigned long mask = BIT_MASK(nr);
    unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);
    unsigned long old;

    old = *p;
    *p = old & ~mask;

    return (old & mask) != 0;
}

/**
 * test_and_change_bit - Change a bit and return its old value
 * @nr: Bit to change
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.
 * It also implies a memory barrier.
 */
static inline int test_and_change_bit(int nr, volatile unsigned long *addr)
{
    unsigned long mask = BIT_MASK(nr);
    unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);
    unsigned long old;

    old = *p;
    *p = old ^ mask;

    return (old & mask) != 0;
}

/**
 * test_bit - Determine whether a bit is set
 * @nr: bit number to test
 * @addr: Address to start counting from
 */
static inline int test_bit(int nr, const volatile unsigned long *addr)
{
    return 1UL & (addr[BIT_WORD(nr)] >> (nr & (BITS_PER_LONG-1)));
}


static inline unsigned long
find_next_bit(unsigned long *addr, unsigned int size, unsigned long offset)
{
	unsigned long *p;
	unsigned long result;
	unsigned long tmp;

	if (offset >= size)
		return size;

	p = addr + BITOP_WORD(offset);
	result = offset & ~(BITS_PER_LONG-1);
	size -= result;
	offset %= BITS_PER_LONG;
	if (offset) {
		tmp = *(p++);
		tmp &= (~0UL << offset);
		if (size < BITS_PER_LONG)
			goto found_first;
		if (tmp)
			goto found_middle;
		size -= BITS_PER_LONG;
		result += BITS_PER_LONG;
	}
	while (size & ~(BITS_PER_LONG-1)) {
		if ((tmp = *(p++)))
			goto found_middle;
		result += BITS_PER_LONG;
		size -= BITS_PER_LONG;
	}
	if (!size)
		return result;
	tmp = *p;

found_first:
	tmp &= (~0UL >> (BITS_PER_LONG - size));
	if (tmp == 0UL)		/* Are any bits set? */
		return result + size;	/* Nope. */
found_middle:
	return result + __ffs(tmp);
}

/*
 * This implementation of find_{first,next}_zero_bit was stolen from
 * Linus' asm-alpha/bitops.h.
 */
static inline unsigned long
find_next_zero_bit(unsigned long *addr, unsigned long size,
                 unsigned long offset)
{
    unsigned long *p = addr + BITOP_WORD(offset);
    unsigned long result = offset & ~(BITS_PER_LONG-1);
    unsigned long tmp;

    if (offset >= size)
        return size;
    size -= result;
    offset %= BITS_PER_LONG;
    if (offset) {
        tmp = *(p++);
        tmp |= ~0UL >> (BITS_PER_LONG - offset);
        if (size < BITS_PER_LONG)
            goto found_first;
        if (~tmp)
            goto found_middle;
        size -= BITS_PER_LONG;
        result += BITS_PER_LONG;
    }
    while (size & ~(BITS_PER_LONG-1)) {
        if (~(tmp = *(p++)))
            goto found_middle;
        result += BITS_PER_LONG;
        size -= BITS_PER_LONG;
    }
    if (!size)
        return result;
    tmp = *p;

found_first:
    tmp |= ~0UL << size;
    if (tmp == ~0UL)    /* Are any bits zero? */
        return result + size;   /* Nope. */
found_middle:
    return result + ffz(tmp);
}

int mb_test_bit(int bit, void *addr);
int mb_find_next_zero_bit(void *addr, int max, int start);
int mb_find_next_bit(void *addr, int max, int start);
void mb_clear_bits(void *bm, int cur, int len);
void mb_set_bits(void *bm, int cur, int len);
int mb_find_zero_run_len(void *addr, int max, int start);

#endif

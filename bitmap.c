#include "bitmap.h"


static inline void *mb_correct_addr_and_bit(int *bit, void *addr)
{
#if BITS_PER_LONG == 64
	*bit += ((unsigned long) addr & 7UL) << 3;
	addr = (void *) ((unsigned long) addr & ~7UL);
#else
	*bit += ((unsigned long) addr & 3UL) << 3;
	addr = (void *) ((unsigned long) addr & ~3UL);
#endif
	return addr;
}

int mb_test_bit(int bit, void *addr)
{
	/*
	 * ext4_test_bit on architecture like powerpc
	 * needs unsigned long aligned address
	 */
	addr = mb_correct_addr_and_bit(&bit, addr);
	return ext4_test_bit(bit, addr);
}

static inline void mb_set_bit(int bit, void *addr)
{
	addr = mb_correct_addr_and_bit(&bit, addr);
	ext4_set_bit(bit, addr);
}

static inline void mb_clear_bit(int bit, void *addr)
{
	addr = mb_correct_addr_and_bit(&bit, addr);
	ext4_clear_bit(bit, addr);
}

static inline int mb_test_and_clear_bit(int bit, void *addr)
{
	addr = mb_correct_addr_and_bit(&bit, addr);
	return ext4_test_and_clear_bit(bit, addr);
}

int mb_find_next_zero_bit(void *addr, int max, int start)
{
	int fix = 0, ret, tmpmax;
	addr = mb_correct_addr_and_bit(&fix, addr);
	tmpmax = max + fix;
	start += fix;

	ret = ext4_find_next_zero_bit(addr, tmpmax, start) - fix;
	if (ret > max)
		return max;
	return ret;
}

int mb_find_next_bit(void *addr, int max, int start)
{
	int fix = 0, ret, tmpmax;
	addr = mb_correct_addr_and_bit(&fix, addr);
	tmpmax = max + fix;
	start += fix;

	ret = ext4_find_next_bit(addr, tmpmax, start) - fix;
	if (ret > max)
		return max;
	return ret;
}

void mb_clear_bits(void *bm, int cur, int len)
{
	uint32_t *addr;

	len = cur + len;
	while (cur < len) {
		if ((cur & 31) == 0 && (len - cur) >= 32) {
			/* fast path: clear whole word at once */
			addr = bm + (cur >> 3);
			*addr = 0;
			cur += 32;
			continue;
		}
		mb_clear_bit(cur, bm);
		cur++;
	}
}

/* clear bits in given range
 * will return first found zero bit if any, -1 otherwise
 */
static int mb_test_and_clear_bits(void *bm, int cur, int len)
{
	uint32_t *addr;
	int zero_bit = -1;

	len = cur + len;
	while (cur < len) {
		if ((cur & 31) == 0 && (len - cur) >= 32) {
			/* fast path: clear whole word at once */
			addr = bm + (cur >> 3);
			if (*addr != (uint32_t)(-1) && zero_bit == -1)
				zero_bit = cur + mb_find_next_zero_bit(addr, 32, 0);
			*addr = 0;
			cur += 32;
			continue;
		}
		if (!mb_test_and_clear_bit(cur, bm) && zero_bit == -1)
			zero_bit = cur;
		cur++;
	}

	return zero_bit;
}

void mb_set_bits(void *bm, int cur, int len)
{
	uint32_t *addr;

	len = cur + len;
	while (cur < len) {
		if ((cur & 31) == 0 && (len - cur) >= 32) {
			/* fast path: set whole word at once */
			addr = bm + (cur >> 3);
			*addr = 0xffffffff;
			cur += 32;
			continue;
		}
		mb_set_bit(cur, bm);
		cur++;
	}
}

int mb_find_zero_run_len(void *addr, int max, int start)
{
	return start + 1 - mb_find_next_zero_bit(addr, max, start);
}

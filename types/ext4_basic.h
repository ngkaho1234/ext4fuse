#ifndef EXT4_BASIC_H
#define EXT4_BASIC_H

#include <stdint.h>

/* TODO: This is a good place to address endianness.  The defined types should
 * not leak outside the types directory anyway. */

#define __le64      uint64_t
#define __le32      uint32_t
#define __le16      uint16_t
#define __u64       uint64_t
#define __u32       uint32_t
#define __u16       uint16_t
#define __u8        uint8_t

#define le16_to_cpu(v) (v)
#define le32_to_cpu(v) (v)
#define le64_to_cpu(v) (v)

#define cpu_to_le16(v) (v)
#define cpu_to_le32(v) (v)
#define cpu_to_le64(v) (v)

#define le16_add_cpu(v, a) (*(v) += (a))
#define le32_add_cpu(v, a) (*(v) += (a))
#define le64_add_cpu(v, a) (*(v) += (a))

typedef uint32_t ext4_lblk_t;
typedef uint64_t ext4_fsblk_t;

typedef uint32_t ext4_group_t;

#endif

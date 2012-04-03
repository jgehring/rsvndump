#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/uio.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned u32;
typedef unsigned long long u64;

#define BUG_ON(x) assert(!(x))

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define __LITTLE_ENDIAN__ 1
#endif

#define get_unaligned(x) (*(x))
#define put_unaligned(v,x) (*(x) = (v))
#ifdef __LITTLE_ENDIAN__
/* Little endian */
#	define get_unaligned_le32(x) ((*(u32 *)(x)))
#	define put_unaligned_le16(v,x) (*(u16 *)(x) = (v))
#else
/* Big endian */
#	define mybswap16(x) ((((x) & 0xFF00) >> 8) | (((x) & 0x00FF) << 8))
#	define mybswap32(x) ((((x) & 0xFF000000) >> 24) | \
                         (((x) & 0x00FF0000) >> 8)  | \
                         (((x) & 0x0000FF00) << 8)  | \
                         (((x) & 0x000000FF) << 24))
#	define get_unaligned_le32(x) (mybswap32(*(u32 *)(x)))
#	define put_unaligned_le16(v,x) (*(u16 *)(x) = mybswap16((u16)v))
#endif

#define vmalloc(x) malloc(x)
#define vfree(x) free(x)

#define EXPORT_SYMBOL(x)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))

#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)

#define min_t(t,x,y) ((x) < (y) ? (x) : (y))
#define max_t(t,x,y) ((x) > (y) ? (x) : (y))


#define BITS_PER_LONG (__SIZEOF_LONG__ * 8)

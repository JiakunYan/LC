#ifndef BITOPS_H_
#define BITOPS_H_

#include "mv/macro.h"

// we need lock so it is atomic and will not be reorder.
// but this is not guarantee that it will be available to other processor.
#define LOCKPREFIX "lock;"

#define ADDR (*(volatile long*)addr)

MV_INLINE void sync_set_bit(long nr, volatile unsigned long* addr)
{
  asm volatile(LOCKPREFIX "bts %1,%0" : "+m"(ADDR) : "Ir"(nr) : "memory");
}

MV_INLINE void sync_clear_bit(long nr, volatile unsigned long* addr)
{
  asm volatile(LOCKPREFIX "btr %1,%0" : "+m"(ADDR) : "Ir"(nr) : "memory");
}

MV_INLINE unsigned long find_first_set(unsigned long word)
{
  asm("rep; bsf %1,%0" : "=r"(word) : "rm"(word));
  return word;
}

MV_INLINE unsigned long find_last_set(unsigned long word)
{
  asm("bsr %1,%0" : "=r"(word) : "rm"(word));
  return word;
}

MV_INLINE unsigned long exchange(unsigned long word,
                                 volatile unsigned long* addr)
{
  asm("xchgq %0,%1" : "=r"(word) : "m"(ADDR), "0"(word) : "memory");
  return word;
}

MV_INLINE long bit_flip(unsigned long word, int bit)
{
  return word ^ ((long)1 << bit);
}

#endif
/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * GCC may emit memcpy for structure copies even with -fno-builtin. */
#include <Base.h>
void *memcpy(void *dst, const void *src, UINTN count)
{
  volatile UINT8 *d = dst;
  const volatile UINT8 *s = src;
  UINTN i;
  for (i = 0; i < count; ++i) d[i] = s[i];
  return dst;
}

void *memset(void *dst, int value, UINTN count)
{
  volatile UINT8 *d = dst;
  UINTN i;
  for (i = 0; i < count; ++i) d[i] = (UINT8)value;
  return dst;
}

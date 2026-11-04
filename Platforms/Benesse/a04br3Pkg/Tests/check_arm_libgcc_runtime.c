/* Linux ARM user-mode probe of the actual selected libgcc integer helpers.
 * This is not firmware, and does not establish wchar/enum ABI compatibility.
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
typedef unsigned long long U64;
typedef long long S64;
extern S64 __divdi3(S64, S64);
extern S64 __moddi3(S64, S64);
extern U64 __muldi3(U64, U64);
extern int __ucmpdi2(U64, U64);
extern U64 __udivdi3(U64, U64);
extern U64 __umoddi3(U64, U64);
extern U64 __udivmoddi4(U64, U64, U64 *);

/* Bit-at-a-time reference without multiplication, division or remainder. */
static U64 reference_div(U64 n, U64 d, U64 *remainder)
{
  U64 q = 0, r = 0;
  int i;
  for (i = 63; i >= 0; --i) {
    U64 carry = r >> 63;
    r = (r << 1) | ((n >> i) & 1);
    if (carry || r >= d) { r -= d; q |= (U64)1 << i; }
  }
  *remainder = r;
  return q;
}

static U64 reference_mul(U64 a, U64 b)
{
  U64 result = 0;
  while (b) { if (b & 1) result += a; a <<= 1; b >>= 1; }
  return result;
}

static int check(void)
{
  volatile U64 n = 0xfedcba9876543210ULL, d = 0x100000003ULL, r = 0;
  volatile S64 sn = -123456789012345LL, sd = 12345;
  U64 state = 0x2135ebd721ae438bULL;
  int i;
  if (__udivdi3(n,d) != 0xfedcba95ULL || __umoddi3(n,d) != 0x79be0251ULL) return 1;
  if (__udivmoddi4(n,d,(U64 *)&r) != 0xfedcba95ULL || r != 0x79be0251ULL) return 2;
  if (__divdi3(sn,sd) != -10000549940LL || __moddi3(sn,sd) != -3045LL) return 3;
  if (__muldi3(0x100000001ULL, 0x100000003ULL) != 0x400000003ULL) return 4;
  if (__ucmpdi2(1,2) != 0 || __ucmpdi2(2,2) != 1 || __ucmpdi2(3,2) != 2) return 5;
  for (i = 0; i < 4096; ++i) {
    U64 expected, rem, combined;
    state ^= state << 13; state ^= state >> 7; state ^= state << 17; n = state;
    state ^= state << 13; state ^= state >> 7; state ^= state << 17; d = (state >> (i & 63)) | 1;
    expected = reference_div(n, d, &rem);
    combined = __udivmoddi4(n, d, (U64 *)&r);
    if (__udivdi3(n, d) != expected || __umoddi3(n, d) != rem ||
        combined != expected || r != rem || __muldi3(n, d) != reference_mul(n, d)) return 6;
    if (__ucmpdi2(n,d) != (n < d ? 0 : n == d ? 1 : 2)) return 7;
    /* Positive magnitudes avoid signed overflow, including INT64_MIN / -1. */
    n >>= 1; d = (d >> 1) | 1;
    expected = reference_div(n, d, &rem);
    if (__divdi3(-(S64)n, (S64)d) != -(S64)expected ||
        __moddi3(-(S64)n, (S64)d) != -(S64)rem) return 8;
  }
  return 0;
}
__attribute__((noreturn)) void _start(void)
{
  register int code __asm__("r0") = check();
  register int call __asm__("r7") = 1;
  __asm__ volatile ("svc 0" : : "r"(code), "r"(call) : "memory");
  __builtin_unreachable();
}

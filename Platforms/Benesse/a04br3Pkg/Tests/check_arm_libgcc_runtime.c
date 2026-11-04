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

static int check(void)
{
  volatile U64 n = 0xfedcba9876543210ULL, d = 0x100000003ULL, r = 0;
  volatile S64 sn = -123456789012345LL, sd = 12345;
  if (__udivdi3(n,d) != 0xfedcba95ULL || __umoddi3(n,d) != 0x79be0251ULL) return 1;
  if (__udivmoddi4(n,d,(U64 *)&r) != 0xfedcba95ULL || r != 0x79be0251ULL) return 2;
  if (__divdi3(sn,sd) != -10000549940LL || __moddi3(sn,sd) != -3045LL) return 3;
  if (__muldi3(0x100000001ULL, 0x100000003ULL) != 0x400000003ULL) return 4;
  if (__ucmpdi2(1,2) != 0 || __ucmpdi2(2,2) != 1 || __ucmpdi2(3,2) != 2) return 5;
  return 0;
}
__attribute__((noreturn)) void _start(void)
{
  register int code __asm__("r0") = check();
  register int call __asm__("r7") = 1;
  __asm__ volatile ("svc 0" : : "r"(code), "r"(call) : "memory");
  __builtin_unreachable();
}

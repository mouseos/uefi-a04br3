/* Test actual early recovery code against a simple PWRAP/RTC MMIO model. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define A04_RECOVERY_MMIO 1
static uint32_t pending, valid, pdn, mode, writes, fail_init, fail_readback;
static char uart[128]; static unsigned uart_len; static int uart_ready=1, dlab;
uint32_t rd(uintptr_t a) {
  if(a==0x1100500cU) return dlab?0x80:0;
  if(a==0x11005014U) return uart_ready?0x20:0;
  if(a==0x1000f0a4U) return fail_init?0:((1U<<21)|(valid?(6U<<16)|pending:0));
  if(a==0x10007000U) return mode;
  return 0;
}
void wr(uintptr_t a,uint32_t v) {
  if(a==0x11005000U) {if(uart_len<sizeof(uart)-1) uart[uart_len++]=(char)v;return;}
  if(a==0x1000f0a8U) {valid=0;return;}
  if(a==0x1000f0a0U) {
    uint32_t addr=((v>>16)&0x7fffU)*2U;
    if(v>>31) { if(addr==0x802c) pdn=v&0xffff; }
    else { pending=addr==0x802c?(fail_readback?0:pdn):0;valid=1; }
    return;
  }
  if(a>=0x10007000U && a<=0x10007008U) {writes++;if(a==0x10007000U)mode=v&0xffffffU;}
}
#include "../WindowsArm32/BootShim/EarlyRecovery.c"
int main(void) {
  A04FdtReport(0x10); A04RecoveryReport(0);
  if(strcmp(uart,"\nA04FDT FAIL=0x10\nA04FDT RECOVERY=FAILED_HALT\n")) return 4;
  uart_len=0; uart[0]=0; uart_ready=0; A04FdtReport(6); if(uart_len) return 5;
  uart_ready=1; dlab=1; A04FdtReport(6); if(uart_len) return 6; dlab=0;
  fail_init=1; if(A04PrepareFastbootRecovery() || writes) return 1;
  fail_init=0; fail_readback=1; if(A04PrepareFastbootRecovery() || writes) return 2;
  pending=valid=pdn=mode=writes=fail_readback=0;
  if(!A04PrepareFastbootRecovery() || !(pdn&0x2000) || !(mode&1) || writes!=3) return 3;
  puts("RTC fastboot readback gates watchdog setup; failure leaves it untouched (MMIO mock)");
  return 0;
}

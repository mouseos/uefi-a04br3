/* Experimental pre-copy FDT validator, no MMIO and no global state.
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
#include <stdint.h>
#include <stddef.h>

static uint32_t be32(const unsigned char *p) {
  /* FDT properties have only four-byte alignment. Do not let the compiler
   * combine these into an unaligned 64-bit load while LK's SCTLR is in force. */
  const volatile unsigned char *b = p;
  return ((uint32_t)b[0]<<24)|((uint32_t)b[1]<<16)|((uint32_t)b[2]<<8)|b[3];
}
static uint64_t be64(const unsigned char *p) { return ((uint64_t)be32(p)<<32)|be32(p+4); }
static int eq(const char *a,const char *b) { while (*a && *a==*b) { ++a; ++b; } return *a==*b; }
static int overlap(uint64_t base,uint64_t size,uint64_t lo,uint64_t hi) {
  return size && base<hi && (size>UINT64_MAX-base || base+size>lo);
}
static int conflict(uint64_t base,uint64_t size) {
  return overlap(base,size,0x40080000,0x41380000) || overlap(base,size,0x4e240000,0x52000000);
}
/* capacity is the amount of mapped input, never an untrusted FDT field. */
int A04FdtGuard(const void *input,size_t capacity) {
  const unsigned char *f=input,*st,*strings; uint32_t total,so,sz,stro,strsz,ro,pos=0;
  uint64_t init_start=0,init_end=0; int have_start=0,have_end=0,depth=-1,resdepth=-1;
  int chosen=-1,mem=-1,resnode=-1,resreg=0,memok=0,rootaddr=2,rootsize=1,resaddr=0,ressize=0,seenres=0,resranges=0;
  if (!f || capacity<40 || be32(f)!=0xd00dfeed) return 1;
  total=be32(f+4); so=be32(f+8); stro=be32(f+12); ro=be32(f+16); strsz=be32(f+32); sz=be32(f+36);
  if (total<40 || total>capacity || total>0x100000 || (so&3) || (ro&7) ||
      so>total || sz>total-so || stro>total || strsz>total-stro || ro<40 || ro>so ||
      (so<stro+strsz && stro<so+sz)) return 2;
  while (1) { uint64_t a,n; if (ro>so || so-ro<16) return 3; a=be64(f+ro);n=be64(f+ro+8);ro+=16;
    if (!a&&!n) break; if (!n || n>UINT64_MAX-a || conflict(a,n)) return 4; }
  st=f+so; strings=f+stro;
  while (pos<sz) {
    uint32_t token; if (sz-pos<4) return 5; token=be32(st+pos);pos+=4;
    if (token==1) {
      uint32_t start=pos; while(pos<sz && st[pos]) pos++; if(pos==sz) return 5;
      depth++; if(depth>32) return 5;
      if(depth==1 && eq((const char*)st+start,"reserved-memory")) { if(seenres) return 10; seenres=1; resdepth=depth; }
      if(depth==1 && (eq((const char*)st+start,"chosen") || eq((const char*)st+start,"chosen@0"))) chosen=depth;
      if(depth==1 && (eq((const char*)st+start,"memory") || eq((const char*)st+start,"memory@00000000"))) mem=depth;
      if(resdepth>=0 && depth==resdepth+1) { resnode=depth; resreg=0; }
      pos=(pos+4)&~3U; if(pos>sz) return 5;
    } else if(token==2) {
      if(depth<0) return 5;
      if(depth==resnode) { if(!resreg) return 11; resnode=-1; }
      if(depth==resdepth) resdepth=-1; if(depth==chosen) chosen=-1; if(depth==mem) mem=-1; depth--;
    } else if(token==3) {
      uint32_t len,noff; const char *name; const unsigned char *v; uint32_t n;
      if(sz-pos<8 || depth<0) return 5; len=be32(st+pos);noff=be32(st+pos+4);pos+=8;
      if(len>sz-pos || noff>=strsz) return 5; name=(const char*)strings+noff;
      for(n=noff;n<strsz && strings[n];n++) {} if(n==strsz) return 5;
      v=st+pos; if(len>UINT32_MAX-3) return 5; n=(len+3)&~3U; if(n>sz-pos) return 5;pos+=n;
      if(depth==0 && eq(name,"#address-cells") && len==4) rootaddr=be32(v);
      if(depth==0 && eq(name,"#size-cells") && len==4) rootsize=be32(v);
      if(depth==resdepth && eq(name,"#address-cells") && len==4) resaddr=be32(v);
      if(depth==resdepth && eq(name,"#size-cells") && len==4) ressize=be32(v);
      if(depth==resdepth && eq(name,"ranges")) { if(len) return 12; resranges=1; }
      if(depth==chosen && eq(name,"linux,initrd-start")) { if(len!=4 && len!=8) return 7; init_start=len==4?be32(v):be64(v);have_start=1; }
      if(depth==chosen && eq(name,"linux,initrd-end")) { if(len!=4 && len!=8) return 7; init_end=len==4?be32(v):be64(v);have_end=1; }
      if(depth==resnode && eq(name,"reg")) {
        if(resaddr!=2 || ressize!=2 || !len || len%16) return 13;
        for(n=0;n<len;n+=16) { uint64_t a=be64(v+n),s=be64(v+n+8); if(!s||s>UINT64_MAX-a||conflict(a,s)) return 14; }
        resreg=1;
      }
      if(depth==mem && eq(name,"reg")) {
        if(rootaddr!=2 || rootsize!=2 || !len || len%16) return 8;
        for(n=0;n<len;n+=16) { uint64_t a=be64(v+n),s=be64(v+n+8); if(s>UINT64_MAX-a) return 8;
          if(a<=0x40080000 && a+s>=0x41380000) memok|=1;
          if(a<=0x4e240000 && a+s>=0x52000000) memok|=2; }
      }
    } else if(token==4) { continue; }
    else if(token==9) {
      if(depth!=-1 || !seenres || !resranges || resaddr!=2 || ressize!=2 || memok!=3 || !have_start || !have_end || init_start>=init_end ||
         init_start!=0x44000000 || init_end>0x44400000 || conflict(init_start,init_end-init_start)) return 9;
      return 0;
    } else return 5;
  }
  return 5;
}

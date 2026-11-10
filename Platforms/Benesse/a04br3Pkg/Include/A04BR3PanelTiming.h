/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04BR3_PANEL_TIMING_H
#define A04BR3_PANEL_TIMING_H
#define A04BR3_PANEL_TIMING_GUID {0x72a415d8,0x3187,0x4c2b,{0xa1,0x28,0x96,0xd6,0x25,0xf3,0x51,0x70}}
#define A04BR3_PANEL_TIMING_MAGIC 0x314d5444U
/* DTM0: immutable little-endian DWORD[16], version1, fps in 0.01 Hz. */
typedef struct {
 UINT32 Magic, Version, Bytes, Flags, PanelId;
 UINT32 Width, Height, TotalWidth, TotalHeight, Fps100, FpsDenominator;
 UINT32 Reserved[5];
} A04BR3_PANEL_TIMING;
/* Parser is used only by SEC and the portable host contract test. */
#ifdef A04BR3_PANEL_TIMING_PARSER
STATIC UINT32 A04PanelLe32(CONST UINT8 *P) {
 return (UINT32)P[0]|((UINT32)P[1]<<8)|((UINT32)P[2]<<16)|((UINT32)P[3]<<24);
}
STATIC BOOLEAN A04PanelParse(CONST UINT8 *P, UINTN Length, A04BR3_PANEL_TIMING *R) {
 CONST CHAR8 Name[]="auo_wxga_dsi"; UINTN I; UINT32 Fps,Vram;
 UINT32 *Words=(UINT32 *)R;
 for(I=0;I<sizeof(*R)/sizeof(UINT32);I++) Words[I]=0;
 R->Magic=A04BR3_PANEL_TIMING_MAGIC;R->Version=1;R->Bytes=sizeof(*R);
 /* CT3 tag_videolfb: u64 fb_base,u32 found,fps,vram,char lcmname[]. */
 if(P==NULL||Length<20+sizeof(Name)||Length>256) return FALSE;
 for(I=0;I<sizeof(Name);I++) if(P[20+I]!=(UINT8)Name[I]) return FALSE;
 Fps=A04PanelLe32(P+12);Vram=A04PanelLe32(P+16);
 if(A04PanelLe32(P)!=0xbe9c0000U||A04PanelLe32(P+4)!=0||
    A04PanelLe32(P+8)!=1||Fps<4500||Fps>7500||Vram<4096000U||Vram>0x02000000U) return FALSE;
 /* Geometry/porches are board specification from CT3 auo_wxga_dsi.c.
  * Refresh is the real LK boot measurement, never a nominal fallback. */
 R->Flags=1;R->PanelId=1;R->Width=800;R->Height=1280;
 R->TotalWidth=960;R->TotalHeight=1300;R->Fps100=Fps;R->FpsDenominator=100;
 return TRUE;
}
#endif /* A04BR3_PANEL_TIMING_PARSER */
#endif

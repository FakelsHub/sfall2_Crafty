/*
 *    sfall
 *    Copyright (C) 2008, 2009, 2010, 2012  The sfall team
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "main.h"

#include <math.h>
#include <stdio.h>
#include "Define.h"
#include "FalloutEngine.h"
#include "FileSystem.h"
#include "Tiles.h"

struct sArt {
 DWORD flags;
 char path[16];
 char* names;
 int d18;
 int total;

 sArt(char* str) {
  flags=0;
  strncpy_s(path, str, 16);
  names=0;
  d18=0;
  total=0;
 }
};
struct OverrideEntry {
 //DWORD id;
 DWORD xtiles;
 DWORD ytiles;
 DWORD replacementid;

 OverrideEntry(DWORD _xtiles, DWORD _ytiles, DWORD _repid) {
  xtiles=_xtiles;
  ytiles=_ytiles;
  replacementid=_repid;
 }
};
#pragma pack(push, 1)
struct frm {
 DWORD _id;   //0x00
 DWORD unused;  //0x04
 WORD frames;  //0x08
 WORD xshift[6];  //0x0a
 WORD yshift[6];  //0x16
 DWORD framestart[6];//0x22
 DWORD size;   //0x3a
 WORD width;   //0x3e
 WORD height;  //0x40
 DWORD frmsize;  //0x42
 WORD xoffset;  //0x46
 WORD yoffset;  //0x48
 BYTE pixels[80*36]; //0x4a
};
#pragma pack(pop)

static OverrideEntry** overrides;
static DWORD origTileCount=0;

static const DWORD Tiles_0E[] = {
 0x484255, 0x48429D, 0x484377, 0x484385, 0x48A897, 0x48A89A, 0x4B2231,
 0x4B2374, 0x4B2381, 0x4B2480, 0x4B248D, 0x4B2A7C, 0x4B2BDA,
};

static const DWORD Tiles_3F[] = {
 0x41875D, 0x4839E6, 0x483A2F, 0x484380, 0x48A803, 0x48A9F2, 0x48C96C,
 0x48C99F, 0x48C9D2, 0x4B2247, 0x4B2334, 0x4B2440, 0x4B2AB9, 0x4B2B8F,
 0x4B2BF4,
};

static const DWORD Tiles_40[] = {
 0x48C941, 0x48C955, 0x48CA5F, 0x48CA71, 0x48CA9B, 0x48CAAD, 0x48CADB,
 0x48CAEB, 0x48CAF7, 0x48CB20, 0x48CB32, 0x48CB61,
};

static const DWORD Tiles_C0[] = {
 0x48424E, 0x484296, 0x484372, 0x48A88D, 0x48A892, 0x4B222C, 0x4B236F,
 0x4B247B, 0x4B2A77, 0x4B2BD5,
};

static DWORD db_fopen(const char* path, const char* mode) {
 DWORD result;
 __asm {
  mov eax, path;
  mov edx, mode;
  call db_fopen_
  mov result, eax;
 }
 return result;
}
static char* db_fgets(char* buf, int max_count, DWORD file) {
 char* result;
 __asm {
  mov eax, buf;
  mov edx, max_count;
  mov ebx, file;
  call db_fgets_
  mov result, eax;
 }
 return result;
}
static void db_fclose(DWORD file) {
 __asm {
  mov eax, file
  call db_fclose_
 }
}
static short db_freadShort(DWORD file) {
 short rout=0;
 __asm {
  mov eax, file;
  lea edx, rout;
  call db_freadShort_
 }
 return rout;
}
static void db_freadByteCount(DWORD file, void* cptr, int count) {
 __asm {
  mov eax, file;
  mov edx, cptr;
  mov ebx, count;
  call db_freadByteCount_
 }
}
static void db_fwriteByteCount(DWORD file, void* cptr, int count) {
 __asm {
  mov eax, file;
  mov edx, cptr;
  mov ebx, count;
  call db_fwriteByteCount_
 }
}
static void db_fseek(DWORD file, long pos/*, int origin*/) {
 __asm {
  mov eax, file;
  mov edx, pos;
  xor ebx, ebx;
  call db_fseek_
 }
}
static void* mem_realloc(void* lpmem, DWORD msize) {
 void* result;
 __asm {
  mov eax, lpmem;
  mov edx, msize;
  call mem_realloc_
  mov result, eax;
 }
 return result;
}

typedef int (_stdcall *functype)();
static const functype _art_init=(functype)art_init_;
static BYTE* mask;
static void CreateMask() {
 mask = new BYTE[80*36];
 DWORD file = db_fopen("art\\tiles\\grid000.frm", "r");
 db_fseek(file, 0x4a);
 db_freadByteCount(file, (BYTE*)mask, 80*36);
 db_fclose(file);
}
static WORD ByteSwapW(WORD w) { return ((w&0xff) << 8) | ((w&0xff00) >> 8); }
static DWORD ByteSwapD(DWORD w) { return ((w&0xff) << 24) | ((w&0xff00) << 8) | ((w&0xff0000) >> 8) | ((w&0xff000000) >> 24); }
static int ProcessTile(sArt* tiles, int tile, int listpos) {
 char buf[32];
 //sprintf_s(buf, "art\\tiles\\%s", &tiles->names[13*tile]);
 strcpy_s(buf, "art\\tiles\\");
 strcat_s(buf, &tiles->names[13*tile]);
 
 DWORD art = db_fopen(buf, "r");
 if(!art) return 0;
 db_fseek(art, 0x3e);
 int width = db_freadShort(art);  //80;
 if(width == 80) {
  db_fclose(art);
  return 0;
 }
 int height = db_freadShort(art); //36
 db_fseek(art, 0x4A);
 BYTE* pixeldata = new BYTE[width*height];
 db_freadByteCount(art, (BYTE*)pixeldata, width*height);
 DWORD listid = listpos-tiles->total;
 float newwidth = (float)(width - width%8);
 float newheight = (float)(height - height%12);
 int xsize = (int)floor(newwidth/32.0f - newheight/24.0f);
 int ysize = (int)floor(newheight/16.0f - newwidth/64.0f);
 for(int y=0;y<ysize;y++) {
  for(int x=0;x<xsize;x++) {
   frm frame;
   db_fseek(art, 0);
   db_freadByteCount(art, (BYTE*)&frame, 0x4a);
   frame.height=ByteSwapW(36);
   frame.width=ByteSwapW(80);
   frame.frmsize=ByteSwapD(80*36);
   frame.size=ByteSwapD(80*36 + 12);
   int xoffset=x*48 + (ysize-(y+1))*32;
   int yoffset=height - (36 + x*12 + y*24);
   for(int y2=0;y2<36;y2++) {
    for(int x2=0;x2<80;x2++) {
     if(mask[y2*80+x2]) {
      frame.pixels[y2*80+x2]=pixeldata[(yoffset+y2)*width+xoffset+x2];
     } else frame.pixels[y2*80+x2]=0;
    }
   }
   
   sprintf_s(buf, 32, "art\\tiles\\zzz%04d.frm", listid++);
   //FScreateFromData(buf, &frame, sizeof(frame));
   DWORD file=db_fopen(buf, "w");
   db_fwriteByteCount(file, &frame, sizeof(frame));
   db_fclose(file);
  }
 }
 overrides[tile]=new OverrideEntry(xsize, ysize, listpos);
 db_fclose(art);
 delete[] pixeldata;
 return xsize*ysize;
}
static DWORD tileMode;
static int _stdcall ArtInitHook2() {
 if(_art_init()) return 1;

 CreateMask();

 sArt* tiles=&((sArt*)_art)[4];
 char buf[32];
 DWORD listpos=tiles->total;
 origTileCount=listpos;
 overrides=new OverrideEntry*[listpos];
 ZeroMemory(overrides, 4*(listpos-1));

 if(tileMode==2) {
  DWORD file=db_fopen("art\\tiles\\xltiles.lst", "rt");
  if(!file) return 0;
  DWORD id;
  char* comment;
  while(db_fgets(buf, 31, file)>0) {
   if(comment=strchr(buf, ';')) *comment=0;
   id=atoi(buf);
   if(id>1) listpos+=ProcessTile(tiles, id, listpos);
  }
  db_fclose(file);
 } else {
  for(int i=2;i<tiles->total;i++) listpos+=ProcessTile(tiles, i, listpos);
 }
 if(listpos!=tiles->total) {
  tiles->names=(char*)mem_realloc(tiles->names, listpos*13);
  for(DWORD i=tiles->total;i<listpos;i++) {
   sprintf_s(&tiles->names[i*13], 12, "zzz%04d.frm", i-tiles->total);
  }
  tiles->total=listpos;
 }

 delete[] mask;
 return 0;
}
static void __declspec(naked) ArtInitHook() {
 __asm {
  pushad
  mov  eax, ds:[_read_callback]
  push eax
  xor  eax, eax
  mov  ds:[_read_callback], eax
  call ArtInitHook2
  pop  eax
  mov  ds:[_read_callback], eax
  popad
  xor  eax, eax
  retn
 }
}

struct tilestruct { short tile[2]; };
static void _stdcall SquareLoadCheck(tilestruct* data) {
 for(DWORD y=0;y<100;y++) {
  for(DWORD x=0;x<100;x++) {
   for(DWORD z=0;z<2;z++) {
    DWORD tile=data[y*100+x].tile[z];
    if(tile>1&&tile<origTileCount&&overrides[tile]) {
     DWORD newtile=overrides[tile]->replacementid - 1;
     for(DWORD y2=0;y2<overrides[tile]->ytiles;y2++) {
      for(DWORD x2=0;x2<overrides[tile]->xtiles;x2++) {
       newtile++;
       if(x-x2<0||y-y2<0) continue; 
       data[(y-y2)*100+x-x2].tile[z]=(short)newtile;
      }
     }
    }
   }
  }
 }
}

static void __declspec(naked) SquareLoadHook() {
 __asm {
  mov  edi, edx
  call db_freadIntCount_
  test eax, eax
  jnz  end
  pushad
  push edi
  call SquareLoadCheck
  popad
end:
  retn
 }
}

static void __declspec(naked) art_id_hook() {
 __asm {
  cmp  esi, 0x4000000                       // Tile?
  jne  end                                  // ���
  and  eax, 0x3FFF
  retn
end:
  and  eax, 0xFFF
  retn
 }
}

static void __declspec(naked) art_get_name_hook() {
 __asm {
  sar  eax, 0x18
  cmp  eax, ObjType_Tile
  jne  end
  mov  esi, edx
  mov  ebp, edx
  and  esi, 0x3FFF
  and  ebp, 0xC000
  sar  ebp, 0xE
end:
  test esi, esi
  retn
 }
}

static void __declspec(naked) map_save_file_hook() {
 __asm {
  push edx
nextSquare:
  and  dword ptr [edx], 0xFFF0FFF
  add  edx, 4
  loop nextSquare
  pop  edx
  mov  ecx, ebx
  jmp  db_fwriteLongCount_
 }
}

/*static void __declspec(naked) square_load_hook() {
 __asm {
  mov  ecx, ebx
  mov  edi, edx
  call db_freadIntCount_
  test eax, eax
  jnz  end
  pushad
  mov  eax, 0xC000C000
  mov  ebx, 0x3F0E
loopSquares:
  test [edi], eax
  jnz  skip
  add  edi, 4
  loop loopSquares
  mov  eax, 0xF000F000
  mov  ebx, 0x0F0C
skip:
  xor  ax, ax
  shr  eax, 0x10
  push eax
  push 0x484371
  call SafeWrite32
  xor  eax, eax
  mov  al, bl
  push eax
  push eax
  push 0x484377
  call SafeWrite8
  push 0x484385
  call SafeWrite8
  xor  eax, eax
  mov  al, bh
  push eax
  push 0x484380
  call SafeWrite8
  popad
end:
  retn
 }
}*/

void TilesInit() {
 tileMode = GetPrivateProfileIntA("Misc", "AllowLargeTiles", 0, ini);
 if (tileMode == 1 || tileMode == 2) {
  HookCall(0x481D72, &ArtInitHook);
  HookCall(0x48434C, SquareLoadHook);
 }

 if (GetPrivateProfileIntA("Misc", "MoreTiles", 0, ini)) {
  MakeCall(0x419D46, &art_id_hook, false);
  MakeCall(0x419479, &art_get_name_hook, false);
  for (int i = 0; i < sizeof(Tiles_0E)/4; i++) SafeWrite8(Tiles_0E[i], 0xE);
  for (int i = 0; i < sizeof(Tiles_3F)/4; i++) SafeWrite8(Tiles_3F[i], 0x3F);
  for (int i = 0; i < sizeof(Tiles_40)/4; i++) SafeWrite8(Tiles_40[i], 0x40);
  for (int i = 0; i < sizeof(Tiles_C0)/4; i++) SafeWrite8(Tiles_C0[i], 0xC0);
  if (*(DWORD*)0x1000E1BF == 0x1000 && *(DWORD*)0x1000E1D9 == 0x0FFF) { // HRP 4.1.8
   SafeWrite8(0x1000E1C0, 0x40);
   SafeWrite8(0x1000E1DA, 0x3F);
  }
 } else HookCall(0x483BCD, &map_save_file_hook);
// HookCall(0x48434C, &square_load_hook);

}

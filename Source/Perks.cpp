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

#include "Define.h"
#include "FalloutEngine.h"
#include "Perks.h"
#include "Vector9x.cpp"

//static const BYTE PerksUsed=121;

static char Name[64*PERK_count];
static char Desc[1024*PERK_count];
static char tName[64*TRAIT_count];
static char tDesc[1024*TRAIT_count];
static char perksFile[260] = ".\\";
static BYTE disableTraits[TRAIT_count];
static DWORD* pc_trait=(DWORD*)_pc_trait;

#define check_trait(a) !disableTraits[a] && (pc_trait[0] == a || pc_trait[1] == a)

static DWORD addPerkMode = 2;

struct TraitStruct {
 char* Name;
 char* Desc;
 DWORD Image;
};

struct PerkStruct {
 char* Name;
 char* Desc;
 int Image;
 int Ranks;
 int Level;
 int Stat;
 int StatMag;
 int Skill1;
 int Skill1Mag;
 int Type;
 int Skill2;
 int Skill2Mag;
 int Str;
 int Per;
 int End;
 int Chr;
 int Int;
 int Agl;
 int Lck;
};
//static const PerkStruct BlankPerk={ &Name[PERK_count*64], &Desc[PERK_count*1024], 0x48, 1, 1, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 };
static PerkStruct Perks[PERK_count];
static TraitStruct Traits[TRAIT_count];

struct FakePerk {
 int Level;
 int Image;
 char Name[64];
 char Desc[1024];
};

vector<FakePerk> fakeTraits;
vector<FakePerk> fakePerks;
vector<FakePerk> fakeSelectablePerks;

static DWORD RemoveTraitID;
static DWORD RemovePerkID;
static DWORD RemoveSelectableID;

static int TraitStatBonuses[TRAIT_count][STAT_max_derived+1];
static int TraitSkillBonuses[TRAIT_count][SKILL_count];

static DWORD IgnoringDefaultPerks=0;
static char PerkBoxTitle[64];

static DWORD PerkFreqOverride;

static const DWORD GainPerks[7][2] = {
 {0x4AF122, 0xC9}, {0x4AF184, 0xC9}, {0x4AF1C0, 0xC9}, {0x4AF217, 0xC9}, // mov  ecx, ecx
 {0x4AF19F, 0x90}, {0x4AF232, 0x90}, {0x4AF24D, 0x90}                    // nop
};

void _stdcall SetPerkFreq(int i) {
 PerkFreqOverride = i;
}

static DWORD _stdcall IsTraitDisabled(int id) {
 return disableTraits[id];
}

static void __declspec(naked) LevelUpHook() {
 __asm {
  push ecx;
  push ebx;
  mov ecx, PerkFreqOverride;
  test ecx, ecx;
  jnz afterskilled;
  mov eax, TRAIT_skilled
  call trait_level_ //Check if the player has the skilled trait
  test eax, eax;
  jz notskilled;
  push TRAIT_skilled
  call IsTraitDisabled;
  test eax, eax;
  jnz notskilled;
  mov ecx, 4;
  jmp afterskilled;
notskilled:
  mov ecx, 3;
afterskilled:
  mov eax, ds:[_Level_] //Get players level
  inc eax;
  xor edx, edx;
  div ecx;
  test edx, edx;
  jnz end;
  inc byte ptr ds:[_free_perk]; //Increment the number of perks owed
end:
  pop ebx;
  pop ecx;
  mov edx, ds:[_Level_]
  retn;
 }
}

static void __declspec(naked) GetPerkBoxTitleHook() {
 __asm {
  lea eax, PerkBoxTitle;
  ret;
 }
}
void _stdcall IgnoreDefaultPerks() { IgnoringDefaultPerks=1; }
void _stdcall RestoreDefaultPerks() { IgnoringDefaultPerks=0; }
void _stdcall SetPerkboxTitle(char* name) {
 if(name[0]=='\0') {
  SafeWrite32(0x0043C77D, 0x488cb);
 } else {
  strcpy_s(PerkBoxTitle, name);
  HookCall(0x0043C77C, GetPerkBoxTitleHook);
 }
}
void _stdcall SetSelectablePerk(char* name, int level, int image, char* desc) {
 if(level<0||level>1) return;
 if(level==0) {
  for(DWORD i=0;i<fakeSelectablePerks.size();i++) {
   if(!strcmp(name,fakeSelectablePerks[i].Name)) {
    fakeSelectablePerks.remove_at(i);
    return;
   }
  }
 } else {
  for(DWORD i=0;i<fakeSelectablePerks.size();i++) {
   if(!strcmp(name,fakeSelectablePerks[i].Name)) {
    fakeSelectablePerks[i].Level=level;
    fakeSelectablePerks[i].Image=image;
    strcpy_s(fakeSelectablePerks[i].Desc, desc);
    return;
   }
  }
  FakePerk fp;
  memset(&fp, 0, sizeof(FakePerk));
  fp.Level=level;
  fp.Image=image;
  strcpy_s(fp.Name, name);
  strcpy_s(fp.Desc, desc);
  fakeSelectablePerks.push_back(fp);
 }
}
void _stdcall SetFakePerk(char* name, int level, int image, char* desc) {
 if(level<0||level>100) return;
 if(level==0) {
  for(DWORD i=0;i<fakePerks.size();i++) {
   if(!strcmp(name,fakePerks[i].Name)) {
    fakePerks.remove_at(i);
    return;
   }
  }
 } else {
  for(DWORD i=0;i<fakePerks.size();i++) {
   if(!strcmp(name,fakePerks[i].Name)) {
    fakePerks[i].Level=level;
    fakePerks[i].Image=image;
    strcpy_s(fakePerks[i].Desc, desc);
    return;
   }
  }
  FakePerk fp;
  memset(&fp, 0, sizeof(FakePerk));
  fp.Level=level;
  fp.Image=image;
  strcpy_s(fp.Name, name);
  strcpy_s(fp.Desc, desc);
  fakePerks.push_back(fp);
 }
}
void _stdcall SetFakeTrait(char* name, int level, int image, char* desc) {
 if(level<0||level>1) return;
 if(level==0) {
  for(DWORD i=0;i<fakeTraits.size();i++) {
   if(!strcmp(name,fakeTraits[i].Name)) {
    fakeTraits.remove_at(i);
    return;
   }
  }
 } else {
  for(DWORD i=0;i<fakeTraits.size();i++) {
   if(!strcmp(name,fakeTraits[i].Name)) {
    fakeTraits[i].Level=level;
    fakeTraits[i].Image=image;
    strcpy_s(fakeTraits[i].Desc, desc);
    return;
   }
  }
  FakePerk fp;
  memset(&fp, 0, sizeof(FakePerk));
  fp.Level=level;
  fp.Image=image;
  strcpy_s(fp.Name, name);
  strcpy_s(fp.Desc, desc);
  fakeTraits.push_back(fp);
 }
}

static DWORD _stdcall HaveFakeTraits2() {
 return fakeTraits.size();
}
static void __declspec(naked) HaveFakeTraits() {
 __asm {
  push ebx;
  push ecx;
  push edx;
  call HaveFakeTraits2;
  pop edx;
  pop ecx;
  pop ebx;
  ret;
 }
}
static DWORD _stdcall HaveFakePerks2() {
 return fakePerks.size();
}
static void __declspec(naked) HaveFakePerks() {
 __asm {
  push ebx;
  push ecx;
  push edx;
  call HaveFakePerks2;
  pop edx;
  pop ecx;
  pop ebx;
  ret;
 }
}
static FakePerk* _stdcall GetFakePerk2(int id) {
 return &fakePerks[id-PERK_count];
}

static void __declspec(naked) GetFakePerk() {
 __asm {
  mov eax, [esp+4];
  push ebx;
  push ecx;
  push edx;
  push eax;
  call GetFakePerk2;
  pop edx;
  pop ecx;
  pop ebx;
  ret 4;
 }
}
static FakePerk* _stdcall GetFakeSPerk2(int id) {
 return &fakeSelectablePerks[id-PERK_count];
}

static void __declspec(naked) GetFakeSPerk() {
 __asm {
  mov eax, [esp+4];
  push ebx;
  push ecx;
  push edx;
  push eax;
  call GetFakeSPerk2;
  pop edx;
  pop ecx;
  pop ebx;
  ret 4;
 }
}
static DWORD _stdcall GetFakeSPerkLevel2(int id) {
 char* c=fakeSelectablePerks[id-PERK_count].Name;
 for(DWORD i=0;i<fakePerks.size();i++) {
  if(!strcmp(c,fakePerks[i].Name)) return fakePerks[i].Level;
 }
 return 0;
}
static void __declspec(naked) GetFakeSPerkLevel() {
 __asm {
  mov eax, [esp+4];
  push ebx;
  push ecx;
  push edx;
  push eax;
  call GetFakeSPerkLevel2;
  pop edx;
  pop ecx;
  pop ebx;
  ret 4;
 }
}

static DWORD _stdcall HandleFakeTraits(int i2) {
 for(DWORD i=0;i<fakeTraits.size();i++) {
  DWORD a=(DWORD)fakeTraits[i].Name;
  __asm {
   mov eax, a;
   call folder_print_line_
   mov a, eax;
  }
  if(a&&!i2) {
   i2=1;
   *(DWORD*)_folder_card_fid = fakeTraits[i].Image;
   *(DWORD*)_folder_card_title = (DWORD)fakeTraits[i].Name;
   *(DWORD*)_folder_card_title2 = 0;
   *(DWORD*)_folder_card_desc = (DWORD)fakeTraits[i].Desc;
  }
 }
 return i2;
}

static void __declspec(naked) PlayerHasPerkHook() {
 __asm {
  push ecx;
  call HandleFakeTraits;
  mov ecx, eax;
  xor ebx, ebx;
oloop:
  mov eax, ds:[_obj_dude]
  mov edx, ebx;
  call perk_level_
  test eax, eax;
  jnz win;
  inc ebx;
  cmp ebx, PERK_count;
  jl oloop;
  call HaveFakePerks;
  test eax, eax;
  jnz win;
  push 0x434446
  retn
win:
  push 0x43438A
  retn
 }
}
static void __declspec(naked) PlayerHasTraitHook() {
 __asm {
  call HaveFakeTraits;
  test eax, eax;
  jz end;
  push 0x43425B
  retn
end:
  jmp PlayerHasPerkHook;
 }
}

static void __declspec(naked) GetPerkLevelHook() {
 __asm {
  cmp edx, PERK_count;
  jl end;
  push edx;
  call GetFakePerk;
  mov eax, ds:[eax];
  ret;
end:
  jmp perk_level_
 }
}

static void __declspec(naked) GetPerkImageHook() {
 __asm {
  cmp eax, PERK_count;
  jl end;
  push eax;
  call GetFakePerk;
  mov eax, ds:[eax+4];
  ret;
end:
  jmp perk_skilldex_fid_
 }
}

static void __declspec(naked) GetPerkNameHook() {
 __asm {
  cmp eax, PERK_count;
  jl end;
  push eax;
  call GetFakePerk;
  lea eax, ds:[eax+8];
  ret;
end:
  jmp perk_name_
 }
}

static void __declspec(naked) GetPerkDescHook() {
 __asm {
  cmp eax, PERK_count;
  jl end;
  push eax;
  call GetFakePerk;
  lea eax, ds:[eax+72];
  ret;
end:
  jmp perk_description_
 }
}

static void __declspec(naked) EndPerkLoopHook() {
 __asm {
  call HaveFakePerks;
  add eax, PERK_count;
  cmp ebx, eax;
  jl end;
  push 0x434446
  retn
end:
  push 0x4343A5
  retn
 }
}

static DWORD _stdcall HandleExtraSelectablePerks(DWORD offset, DWORD* data) {
 for(DWORD i=0;i<fakeSelectablePerks.size();i++) {
  //*(WORD*)(_name_sort_list + (offset+i)*8)=(WORD)(PERK_count+i);
  data[offset+i]=PERK_count+i;
 }
 return offset+fakeSelectablePerks.size();
}
static void __declspec(naked) GetAvailablePerksHook() {
 __asm {
  push ecx;
  push ebx;
  push edx;
  mov ebx, IgnoringDefaultPerks;
  test ebx, ebx;
  jnz skipdefaults;
  call perk_make_list_
  jmp next;
skipdefaults:
  xor eax, eax;
next:
  push eax;
  call HandleExtraSelectablePerks;
  pop ebx;
  pop ecx;
  ret;
 }
}
static void __declspec(naked) GetPerkSLevelHook() {
 __asm {
  cmp edx, PERK_count;
  jl end;
  push edx;
  call GetFakeSPerkLevel;
  ret;
end:
  jmp perk_level_
 }
}
static void __declspec(naked) GetPerkSImageHook() {
 __asm {
  cmp eax, PERK_count;
  jl end;
  push eax;
  call GetFakeSPerk;
  mov eax, ds:[eax+4];
  ret;
end:
  jmp perk_skilldex_fid_
 }
}

static void __declspec(naked) GetPerkSNameHook() {
 __asm {
  cmp eax, PERK_count;
  jl end;
  push eax;
  call GetFakeSPerk;
  lea eax, ds:[eax+8];
  ret;
end:
  jmp perk_name_
 }
}

static void __declspec(naked) GetPerkSDescHook() {
 __asm {
  cmp eax, PERK_count;
  jl end;
  push eax;
  call GetFakeSPerk;
  lea eax, ds:[eax+72];
  ret;
end:
  jmp perk_description_
 }
}

static void _stdcall AddFakePerk(DWORD perkID) {
 perkID-=PERK_count;
 if(addPerkMode&1) {
  bool matched=false;
  for(DWORD d=0;d<fakeTraits.size();d++) {
   if(!strcmp(fakeTraits[d].Name, fakeSelectablePerks[perkID].Name)) {
    matched=true;
   }
  }
  if(!matched) {
   RemoveTraitID=fakeTraits.size();
   fakeTraits.push_back(fakeSelectablePerks[perkID]);
  }
 }
 if(addPerkMode&2) {
  bool matched=false;
  for(DWORD d=0;d<fakePerks.size();d++) {
   if(!strcmp(fakePerks[d].Name, fakeSelectablePerks[perkID].Name)) {
    RemovePerkID=d;
    fakePerks[d].Level++;
    matched=true;
   }
  }
  if(!matched) {
   RemovePerkID=fakePerks.size();
   fakePerks.push_back(fakeSelectablePerks[perkID]);
  }
 }
 if(addPerkMode&4) {
  RemoveSelectableID=perkID;
  //fakeSelectablePerks.remove_at(perkID);

 }
}

static void __declspec(naked) perk_add_hook() {
 __asm {
  pushad
  push edx
  call AddFakePerk
  popad
  xor  eax, eax
  retn
 }
}

static void __declspec(naked) HeaveHoHook() {
 __asm {
  xor edx, edx;
  mov eax, ecx;
  call stat_level_
  lea ebx, [0+eax*4];
  sub ebx, eax;
  cmp ebx, esi;
  jle lower;
  mov ebx, esi;
lower:
  mov eax, ecx;
  mov edx, PERK_heave_ho
  call perk_level_
  lea ecx, [0+eax*8];
  sub ecx, eax;
  sub ecx, eax;
  mov eax, ecx;
  add eax, ebx;
  push 0x478AFC
  retn
 }
}
void _stdcall ApplyHeaveHoFix() {
 SafeWrite8(0x478AC4, 0xe9);
 HookCall(0x478AC4, HeaveHoHook);
 Perks[PERK_heave_ho].Str=0;
}

static DWORD Educated, Lifegiver, Tag_, Mutate_;
static void __declspec(naked) editor_design_hook() {
 __asm {
  call SavePlayer_
  mov  eax, ds:[_Educated]
  mov  Educated, eax
  mov  eax, ds:[_Lifegiver]
  mov  Lifegiver, eax
  mov  eax, ds:[_Tag_]
  mov  Tag_, eax
  mov  eax, ds:[_Mutate_]
  mov  Mutate_, eax
  retn
 }
}

static void __declspec(naked) editor_design_hook2() {
 __asm {
  mov  eax, Educated
  mov  ds:[_Educated], eax
  mov  eax, Lifegiver
  mov  ds:[_Lifegiver], eax
  mov  eax, Tag_
  mov  ds:[_Tag_], eax
  mov  eax, Mutate_
  mov  ds:[_Mutate_], eax
  jmp  RestorePlayer_
 }
}

static void __declspec(naked) perks_dialog_hook() {
 __asm {
  call ListSkills_
  mov  ebx, ds:[_obj_dude]
  mov  eax, ebx
  mov  edx, PERK_educated
  call perk_level_
  mov  ds:[_Educated], eax
  mov  eax, ebx
  mov  edx, PERK_lifegiver
  call perk_level_
  mov  ds:[_Lifegiver], eax
  mov  eax, ebx
  mov  edx, PERK_tag
  call perk_level_
  mov  ds:[_Tag_], eax
  mov  eax, ebx
  mov  edx, PERK_mutate
  call perk_level_
  mov  ds:[_Mutate_], eax
  retn
 }
}

static void __declspec(naked) perk_can_add_hook() {
 __asm {
  mov  esi, ds:[_perkLevelDataList]
  mov  esi, [esi+edx*4]
  imul esi, esi, 3
  add  esi, [ecx+0x10]                      // Perk.ReqLevel
  cmp  eax, esi                             // ������� ������ ������ ���������� ������?
  jl   end                                  // ��
  pop  esi                                  // ���������� ����� ��������
  push 0x496891
end:
  retn
 }
}

static void PerkSetup() {
 //Character screen
 HookCall(0x00434256, PlayerHasTraitHook);
 MakeCall(0x0043436B, PlayerHasPerkHook, true);
 HookCall(0x004343AC, GetPerkLevelHook);
 HookCall(0x004343C1, GetPerkNameHook);
 HookCall(0x004343DF, GetPerkNameHook);
 HookCall(0x0043440D, GetPerkImageHook);
 HookCall(0x0043441B, GetPerkNameHook);
 HookCall(0x00434432, GetPerkDescHook);
 MakeCall(0x0043443D, EndPerkLoopHook, true);

 //GetPlayerAvailablePerks
 HookCall(0x43D127, GetAvailablePerksHook);
 HookCall(0x43D17D, GetPerkSNameHook);
 HookCall(0x43D25E, GetPerkSLevelHook);
 HookCall(0x43D275, GetPerkSLevelHook);
 //ShowPerkBox
 HookCall(0x0043C82E, GetPerkSLevelHook);
 HookCall(0x0043C85B, GetPerkSLevelHook);
 HookCall(0x43C888, GetPerkSDescHook);
 HookCall(0x43C8A6, GetPerkSNameHook);
 HookCall(0x43C8D1, GetPerkSDescHook);
 HookCall(0x43C8EF, GetPerkSNameHook);
 HookCall(0x43C90F, GetPerkSImageHook);
 SafeWrite8(0x496A65, 0x16);
 MakeCall(0x496A6B, &perk_add_hook, false);
 //PerkboxSwitchPerk
 HookCall(0x43C3F1, GetPerkSLevelHook);
 HookCall(0x43C41E, GetPerkSLevelHook);
 HookCall(0x43C44B, GetPerkSDescHook);
 HookCall(0x43C469, GetPerkSNameHook);
 HookCall(0x43C494, GetPerkSDescHook);
 HookCall(0x43C4B2, GetPerkSNameHook);
 HookCall(0x43C4D2, GetPerkSImageHook);

 memset(Name, 0, sizeof(Name));
 memset(Desc, 0, sizeof(Desc));
 memcpy(Perks, (void*)_perk_data, sizeof(PerkStruct)*PERK_count);

 SafeWrite32(0x00496669, (DWORD)Perks);
 SafeWrite32(0x00496837, (DWORD)Perks);
 SafeWrite32(0x00496BAD, (DWORD)Perks);
 SafeWrite32(0x00496C41, (DWORD)Perks);
 SafeWrite32(0x00496D25, (DWORD)Perks);
 SafeWrite32(0x00496696, (DWORD)Perks + 4);
 SafeWrite32(0x00496BD1, (DWORD)Perks + 4);
 SafeWrite32(0x00496BF5, (DWORD)Perks + 8);
 SafeWrite32(0x00496AD4, (DWORD)Perks + 12);

 if (strlen(perksFile)) {
  char num[4];
  for(int i=0;i<PERK_count;i++) {
   _itoa_s(i, num, 10);
   if(GetPrivateProfileString(num, "Name", "", &Name[i*64], 63, perksFile)) Perks[i].Name=&Name[i*64];
   if(GetPrivateProfileString(num, "Desc", "", &Desc[i*1024], 1023, perksFile)) {
    Perks[i].Desc=&Desc[i*1024];
   }
   int value;
   value=GetPrivateProfileInt(num, "Image", -99999, perksFile);
   if(value!=-99999) Perks[i].Image=value;
   value=GetPrivateProfileInt(num, "Ranks", -99999, perksFile);
   if(value!=-99999) Perks[i].Ranks=value;
   value=GetPrivateProfileInt(num, "Level", -99999, perksFile);
   if(value!=-99999) Perks[i].Level=value;
   value=GetPrivateProfileInt(num, "Stat", -99999, perksFile);
   if(value!=-99999) Perks[i].Stat=value;
   value=GetPrivateProfileInt(num, "StatMag", -99999, perksFile);
   if(value!=-99999) Perks[i].StatMag=value;
   value=GetPrivateProfileInt(num, "Skill1", -99999, perksFile);
   if(value!=-99999) Perks[i].Skill1=value;
   value=GetPrivateProfileInt(num, "Skill1Mag", -99999, perksFile);
   if(value!=-99999) Perks[i].Skill1Mag=value;
   value=GetPrivateProfileInt(num, "Type", -99999, perksFile);
   if(value!=-99999) Perks[i].Type=value;
   value=GetPrivateProfileInt(num, "Skill2", -99999, perksFile);
   if(value!=-99999) Perks[i].Skill2=value;
   value=GetPrivateProfileInt(num, "Skill2Mag", -99999, perksFile);
   if(value!=-99999) Perks[i].Skill2Mag=value;
   value=GetPrivateProfileInt(num, "STR", -99999, perksFile);
   if(value!=-99999) Perks[i].Str=value;
   value=GetPrivateProfileInt(num, "PER", -99999, perksFile);
   if(value!=-99999) Perks[i].Per=value;
   value=GetPrivateProfileInt(num, "END", -99999, perksFile);
   if(value!=-99999) Perks[i].End=value;
   value=GetPrivateProfileInt(num, "CHR", -99999, perksFile);
   if(value!=-99999) Perks[i].Chr=value;
   value=GetPrivateProfileInt(num, "INT", -99999, perksFile);
   if(value!=-99999) Perks[i].Int=value;
   value=GetPrivateProfileInt(num, "AGL", -99999, perksFile);
   if(value!=-99999) Perks[i].Agl=value;
   value=GetPrivateProfileInt(num, "LCK", -99999, perksFile);
   if(value!=-99999) Perks[i].Lck=value;
  }
 }

 for(int i=0;i<PERK_count;i++) {
  if(Perks[i].Name!=&Name[64*i]) {
   strcpy_s(&Name[64*i], 64, Perks[i].Name);
   Perks[i].Name=&Name[64*i];
  }
  if(Perks[i].Desc&&Perks[i].Desc!=&Desc[1024*i]) {
   strcpy_s(&Desc[1024*i], 1024, Perks[i].Desc);
   Perks[i].Desc=&Desc[1024*i];
  }
 }

 //perk_owed hooks
 MakeCall(0x4AFB2F, LevelUpHook, false);//replaces 'mov edx, ds:[PlayerLevel]
 SafeWrite8(0x4AFB34, 0x90);

 SafeWrite8(0x43C2EC, 0xEB); //skip the block of code which checks if the player has gained a perk (now handled in level up code)

// ���������� ���������� ����������������� �����
 SafeWrite16(0x43C369, 0x0DFE);             // dec  byte ptr ds:_free_perk
 SafeWrite8(0x43C370, 0xB1);                // jmp  0x43C322
 HookCall(0x431E2B, &editor_design_hook);
 HookCall(0x4329B2, &editor_design_hook2);
 HookCall(0x43CA77, &perks_dialog_hook);
 MakeCall(0x496884, &perk_can_add_hook, false);

// ����������� ���� ������
 SafeWrite8(0x43C577, 31);                  // 91-60=31
 SafeWrite8(0x43CB69, 74);                  // 134-60=74

 int RiflescopePenalty = GetPrivateProfileIntA("Misc", "RiflescopePenalty", -1, ini);
 if (RiflescopePenalty >= 0) SafeWrite32(0x42448E, RiflescopePenalty);

}

static int _stdcall stat_get_base_direct(DWORD statID) {
 DWORD result;
 __asm {
  mov edx, statID;
  mov eax, ds:[_obj_dude]
  call stat_get_base_direct_
  mov result, eax;
 }
 return result;
}

static int _stdcall trait_adjust_stat_override(DWORD statID) {
 int result = 0;
 if (statID >= STAT_st && statID <= STAT_max_derived) {
  if (pc_trait[0] != -1) result+=TraitStatBonuses[pc_trait[0]][statID];
  if (pc_trait[1] != -1) result+=TraitStatBonuses[pc_trait[1]][statID];
  switch (statID) {
   case STAT_st:
    if(check_trait(TRAIT_gifted)) result++;
    if(check_trait(TRAIT_bruiser)) result+=2;
    break;
   case STAT_pe:
    if(check_trait(TRAIT_gifted)) result++;
    break;
   case STAT_en:
    if(check_trait(TRAIT_gifted)) result++;
    break;
   case STAT_ch:
    if(check_trait(TRAIT_gifted)) result++;
    break;
   case STAT_iq:
    if(check_trait(TRAIT_gifted)) result++;
    break;
   case STAT_ag:
    if(check_trait(TRAIT_gifted)) result++;
    if(check_trait(TRAIT_small_frame)) result++;
    break;
   case STAT_lu:
    if(check_trait(TRAIT_gifted)) result++;
    break;
   case STAT_max_move_points:
    if(check_trait(TRAIT_bruiser)) result-=2;
    break;
   case STAT_ac:
    if(check_trait(TRAIT_kamikaze)) return -stat_get_base_direct(STAT_ac);
    break;
   case STAT_melee_dmg:
    if(check_trait(TRAIT_heavy_handed)) result+=4;
    break;
   case STAT_carry_amt:
    if(check_trait(TRAIT_small_frame)) {
     int str=stat_get_base_direct(STAT_st);
     result-=str*10;
    }
    break;
   case STAT_sequence:
    if(check_trait(TRAIT_kamikaze)) result+=5;
    break;
   case STAT_heal_rate:
    if(check_trait(TRAIT_fast_metabolism)) result+=2;
    break;
   case STAT_crit_chance:
    if(check_trait(TRAIT_finesse)) result+=10;
    break;
   case STAT_better_crit:
    if(check_trait(TRAIT_heavy_handed)) result-=30;
    break;
   case STAT_rad_resist:
    if(check_trait(TRAIT_fast_metabolism)) return -stat_get_base_direct(STAT_rad_resist);
    break;
   case STAT_poison_resist:
    if(check_trait(TRAIT_fast_metabolism)) return -stat_get_base_direct(STAT_poison_resist);
    break;
  }
 }
 return result;
}

static void __declspec(naked) TraitAdjustStatHook() {
 __asm {
  xor  ebx, ebx
check:
  cmp  eax, STAT_max_derived
  jbe  skip
  retn
skip:
  push eax
  call trait_adjust_stat_override
  mov  ebx, STAT_real_max_stat
  xchg ebx, eax
  jmp  check
 }
}

static int _stdcall trait_adjust_skill_override(DWORD skillID) {
 int result = 0;
 if (pc_trait[0] != -1) result+=TraitSkillBonuses[pc_trait[0]][skillID];
 if (pc_trait[1] != -1) result+=TraitSkillBonuses[pc_trait[1]][skillID];

 if (check_trait(TRAIT_gifted)) result-=10;
 if (check_trait(TRAIT_good_natured)) {
  if (skillID <= SKILL_THROWING) result-=10;
  else if (skillID == SKILL_FIRST_AID || skillID == SKILL_DOCTOR || skillID == SKILL_CONVERSANT || skillID == SKILL_BARTER) result+=15;
 }
 return result;
}

static void __declspec(naked) TraitAdjustSkillHook() {
 __asm {
  push edx;
  push ecx;
  push eax;
  call trait_adjust_skill_override;
  pop ecx;
  pop edx;
  retn;
 }
}

static void __declspec(naked) BlockedTrait() {
 __asm {
  xor eax, eax;
  retn;
 }
}

static void TraitSetup() {
 MakeCall(0x4B3C84, &TraitAdjustStatHook, false);
 MakeCall(0x4B40FC, &TraitAdjustSkillHook, true);

 memset(tName, 0, sizeof(tName));
 memset(tDesc, 0, sizeof(tDesc));
 memcpy(Traits, (void*)_trait_data, sizeof(TraitStruct)*TRAIT_count);
 memset(TraitStatBonuses, 0, sizeof(TraitStatBonuses));
 memset(TraitSkillBonuses, 0, sizeof(TraitSkillBonuses));

 SafeWrite32(0x4B3A81, (DWORD)Traits);
 SafeWrite32(0x4B3B80, (DWORD)Traits);
 SafeWrite32(0x4B3AAE, (DWORD)Traits + 4);
 SafeWrite32(0x4B3BA0, (DWORD)Traits + 4);
 SafeWrite32(0x4B3BC0, (DWORD)Traits + 8);

 if (strlen(perksFile)) {
  char num[5], buf[512];
  num[0] = 't';
  char* num2 = &num[1];
  for (int TRAIT_ = 0; TRAIT_ < TRAIT_count; TRAIT_++) {
   _itoa_s(TRAIT_, num2, 4, 10);
   if (GetPrivateProfileString(num, "Name", "", &tName[TRAIT_*64], 63, perksFile)) Traits[TRAIT_].Name = &tName[TRAIT_*64];
   if (GetPrivateProfileString(num, "Desc", "", &tDesc[TRAIT_*1024], 1023, perksFile)) Traits[TRAIT_].Desc = &tDesc[TRAIT_*1024];
   int value;
   value = GetPrivateProfileInt(num, "Image", -99999, perksFile);
   if (value != -99999) Traits[TRAIT_].Image = value;

   if (GetPrivateProfileStringA(num, "StatMod", "", buf, 512, perksFile) > 0) {
    char *stat, *mod;
    stat = strtok(buf, "|");
    mod = strtok(0, "|");
    while (stat && mod) {
     int STAT_ = atoi(stat);
     if (STAT_ >= STAT_st && STAT_ <= STAT_max_derived) TraitStatBonuses[TRAIT_][STAT_] = atoi(mod);
     stat = strtok(0, "|");
     mod = strtok(0, "|");
    }
   }

   if (GetPrivateProfileStringA(num, "SkillMod", "", buf, 512, perksFile) > 0) {
    char *skill, *mod;
    skill = strtok(buf, "|");
    mod = strtok(0, "|");
    while (skill && mod) {
     int SKILL_ = atoi(skill);
     if (SKILL_ >= SKILL_SMALL_GUNS && SKILL_ < SKILL_count) TraitSkillBonuses[TRAIT_][SKILL_] = atoi(mod);
     skill = strtok(0, "|");
     mod = strtok(0, "|");
    }
   }

   if (GetPrivateProfileInt(num, "NoHardcode", 0, perksFile)) {
    disableTraits[TRAIT_] = 1;
    switch(TRAIT_) {
     case TRAIT_one_hander:
      HookCall(0x4245E0, &BlockedTrait);
      break;
     case TRAIT_finesse:
      HookCall(0x4248F9, &BlockedTrait);
      break;
     case TRAIT_fast_shot:
      HookCall(0x478C8A, &BlockedTrait); //fast shot
      HookCall(0x478E70, &BlockedTrait);
      break;
     case TRAIT_bloody_mess:
      HookCall(0x410707, &BlockedTrait);
      break;
     case TRAIT_jinxed:
      HookCall(0x42389F, &BlockedTrait);
      break;
     case TRAIT_drug_addict:
      HookCall(0x47A0CD, &BlockedTrait);
      HookCall(0x47A51A, &BlockedTrait);
      break;
     case TRAIT_drug_resistant:
      HookCall(0x479BE1, &BlockedTrait);
      HookCall(0x47A0DD, &BlockedTrait);
      break;
     case TRAIT_skilled:
      HookCall(0x43C295, &BlockedTrait);
      HookCall(0x43C2F3, &BlockedTrait);
      break;
     case TRAIT_gifted:
      HookCall(0x43C2A4, &BlockedTrait);
      break;
    }
   }
  }
 }

 for (int i = 0; i < TRAIT_count; i++) {
  if (Traits[i].Name != &tName[64*i]) {
   strcpy_s(&tName[64*i], 64, Traits[i].Name);
   Traits[i].Name = &tName[64*i];
  }
  if (Traits[i].Desc && Traits[i].Desc != &tDesc[1024*i]) {
   strcpy_s(&tDesc[1024*i], 1024, Traits[i].Desc);
   Traits[i].Desc = &tDesc[1024*i];
  }
 }
}

static __declspec(naked) void PerkInitWrapper() {
 __asm {
  call perk_init_
  pushad
  call PerkSetup
  popad
  retn
 }
}

static void __declspec(naked) game_init_hook() {
 __asm {
  call trait_init_
  pushad
  call TraitSetup
  popad
  retn
 }
}

static void __declspec(naked) is_supper_bonus_hook() {
 __asm {
  add  eax, ecx
  test eax, eax
  jle  skip
  cmp  eax, 10
  jle  end
skip:
  pop  eax                                  // ���������� ����� ��������
  xor  eax, eax
  inc  eax
  pop  edx
  pop  ecx
  pop  ebx
end:
  retn
 }
}

static void __declspec(naked) PrintBasicStat_hook() {
 __asm {
  test eax, eax
  jle  skip
  cmp  eax, 10
  jg   end
  pop  ebx                                  // ���������� ����� ��������
  push 0x434C21
  retn
skip:
  xor  eax, eax
end:
  retn
 }
}

static void __declspec(naked) StatButton_hook() {
 __asm {
  call inc_stat_
  test eax, eax
  jl   end
  test ebx, ebx
  jge  end
  sub  ds:[_character_points], esi
  dec  esi
  mov  [esp+0xC+0x4], esi
end:
  retn
 }
}

static void __declspec(naked) StatButton_hook1() {
 __asm {
  call stat_level_
  cmp  eax, 1
  jg   end
  pop  eax                                  // ���������� ����� ��������
  xor  eax, eax
  inc  eax
  mov  [esp+0xC], eax
  push 0x437B41
end:
  retn
 }
}

static DWORD drugExploit = 0;
static void __declspec(naked) protinst_use_item_hook() {
 __asm {
  inc  dword ptr drugExploit
  call obj_use_book_
  dec  dword ptr drugExploit
  retn
 }
}

static void __declspec(naked) UpdateLevel_hook() {
 __asm {
  inc  dword ptr drugExploit
  call perks_dialog_
  dec  dword ptr drugExploit
  retn
 }
}

static void __declspec(naked) skill_level_hook() {
 __asm {
  inc  dword ptr drugExploit
  call skill_level_
  dec  dword ptr drugExploit
  retn
 }
}

static void __declspec(naked) SliderBtn_hook() {
 __asm {
  inc  dword ptr drugExploit
  call skill_inc_point_
  dec  dword ptr drugExploit
  retn
 }
}

static void __declspec(naked) SliderBtn_hook1() {
 __asm {
  inc  dword ptr drugExploit
  call skill_dec_point_
  dec  dword ptr drugExploit
  retn
 }
}

static void __declspec(naked) checkPerk() {
 __asm {
  inc  eax                                  // ���� ����?
  jz   end                                  // ���
  dec  eax
  imul edx, eax, 0x4C
  mov  eax, offset Perks                    // _perk_data
  lea  eax, [eax+edx]
  cmp  esi, [eax+0x14]                      // Perk.Stat
  jne  skip
  sub  ebp, [eax+0x18]                      // Perk.StatMod
skip:
  cmp  dword ptr [eax+0xC], -1              // Perk.Ranks
  jne  end
  sub  ebp, [eax+esi*4+0x30]                // Perk.Str
end:
  retn
 }
}

static void __declspec(naked) stat_get_real_bonus() {
 __asm {
  push ecx
  xchg ebp, eax
  mov  eax, ebx
  call inven_worn_
  test eax, eax
  jz   noArmor
  call item_ar_perk_
  call checkPerk
noArmor:
  mov  eax, ebx
  call inven_right_hand_
  test eax, eax
  jz   noRightWeapon
  mov  edx, eax
  call item_get_type_
  cmp  eax, item_type_weapon
  jne  noRightWeapon
  xchg edx, eax                             // eax = weapon
  call item_w_perk_
  call checkPerk
noRightWeapon:
  mov  eax, ebx
  call inven_left_hand_
  test eax, eax
  jz   noLeftWeapon
  mov  edx, eax
  call item_get_type_
  cmp  eax, item_type_weapon
  jne  noLeftWeapon
  xchg edx, eax                             // eax = weapon
  call item_w_perk_
  call checkPerk
noLeftWeapon:
  mov  ecx, ds:[_queue]
loopQueue:
  jecxz end                                 // ������ ��� � �������
  cmp  ebx, [ecx+0x8]                       // source == queue.object?
  jne  nextQueue                            // ���
  mov  edx, [ecx+0x4]                       // edx = queue.type
  mov  edi, [ecx+0xC]                       // edi = queue.data
  test edx, edx                             // ��������?
  jz   checkDrug                            // ��
  cmp  edx, 2                               // �����������?
  je   checkAddict                          // ��
  cmp  edx, 6                               // ��������?
  jne  nextQueue                            // ���
  cmp  esi, edx                             // STAT_lu?
  je   nextQueue                            // ��
  mov  eax, [edi]                           // eax = queue_rads.rad_level
  dec  eax
  shl  eax, 5
  sub  ebp, ds:[_rad_bonus][eax+esi*4]
  jmp  nextQueue
checkDrug:
  cmp  dword ptr [edi+0x4], -2              // ������ ��������?
  jne  loopStats                            // ���
  inc  edx
  inc  edx
loopStats:
  cmp  esi, [edi+edx*4+0x4]                 // stat == queue_drug.stat#?
  jne  nextStat                             // ���
  add  ebp, [edi+edx*4+0x10]
nextStat:  
  inc  edx
  cmp  edx, 3
  jl   loopStats
  jmp  nextQueue
checkAddict:
  dec  edx
  cmp  [edi], edx                           // ���� �������?
  je   nextQueue                            // ���
  mov  eax, [edi+0x8]                       // eax = queue_addict.perk
  call checkPerk
nextQueue:
  mov  ecx, [ecx+0x10]                      // ecx = queue.next
  jmp  loopQueue
end:
  xchg ebp, eax
  pop  ecx
  retn
 }
}

static void __declspec(naked) stat_level_hook() {
// ebx = source, ecx = base, esi = stat
 __asm {
  call stat_get_bonus_
  cmp  esi, STAT_lu
  ja   end                                  // ��������� ������ ����-�����
  cmp  dword ptr drugExploit, 0             // ����� �� ������ ����?
  je   end                                  // ���
  call stat_get_real_bonus                  // �� ��������� ��������� �������
end:
  retn
 }
}

void _stdcall SetPerkValue(int id, int value, DWORD offset) {
 if(id<0||id>=PERK_count) return;
 *(DWORD*)((DWORD)(&Perks[id])+offset)=value;
}
void _stdcall SetPerkName(int id, char* value) {
 if(id<0||id>=PERK_count) return;
 strcpy_s(&Name[id*64], 64, value);
}
void _stdcall SetPerkDesc(int id, char* value) {
 if(id<0||id>=PERK_count) return;
 strcpy_s(&Desc[id*1024], 1024, value);
 Perks[id].Desc=&Desc[1024*id];
}

void PerksInit() {

 for (int STAT_ = STAT_st; STAT_ <= STAT_lu; STAT_++) {
  SafeWrite32(_perk_data + 0x14 + (PERK_gain_strength + STAT_) * 0x4C, STAT_);
  SafeWrite32(_perk_data + 0x18 + (PERK_gain_strength + STAT_) * 0x4C, 1);
  SafeWrite8(GainPerks[STAT_][0], (BYTE)GainPerks[STAT_][1]);
 }

 HookCall(0x442729, &PerkInitWrapper);

 if (GetPrivateProfileString("Misc", "PerksFile", "", &perksFile[2], 257, ini)) HookCall(0x44272E, &game_init_hook);
 else perksFile[0] = 0;

 MakeCall(0x43DF6F, &is_supper_bonus_hook, false);
 MakeCall(0x434BFF, &PrintBasicStat_hook, false);
 HookCall(0x437AB4, &StatButton_hook);
 HookCall(0x437B26, &StatButton_hook1);

 if (GetPrivateProfileIntA("Misc", "DrugExploitFix", 0, ini)) {
  HookCall(0x49BF5B, &protinst_use_item_hook);
  HookCall(0x43C342, &UpdateLevel_hook);
  HookCall(0x43A8A1, &skill_level_hook);    // SavePlayer_
  HookCall(0x43B3FC, &SliderBtn_hook);
  HookCall(0x43B463, &skill_level_hook);    // SliderBtn_
  HookCall(0x43B47C, &SliderBtn_hook1);
  HookCall(0x4AEFC3, &stat_level_hook);
 }

}

void PerksReset() {
 fakeTraits.clear();
 fakePerks.clear();
 fakeSelectablePerks.clear();
 SafeWrite32(0x0043C77D, 0x488cb);
 IgnoringDefaultPerks=0;
 addPerkMode=2;
 SafeWrite8(0x478AC4, 0xba);
 SafeWrite32(0x478AC5, 0x23);
 PerkFreqOverride=0;
}
void PerksSave(HANDLE file) {
 DWORD count;
 DWORD unused;
 count=fakeTraits.size();
 WriteFile(file, &count, 4, &unused, 0);
 for(DWORD i=0;i<count;i++) WriteFile(file, &fakeTraits[i], sizeof(FakePerk), &unused, 0);
 count=fakePerks.size();
 WriteFile(file, &count, 4, &unused, 0);
 for(DWORD i=0;i<count;i++) WriteFile(file, &fakePerks[i], sizeof(FakePerk), &unused, 0);
 count=fakeSelectablePerks.size();
 WriteFile(file, &count, 4, &unused, 0);
 for(DWORD i=0;i<count;i++) WriteFile(file, &fakeSelectablePerks[i], sizeof(FakePerk), &unused, 0);
}
bool PerksLoad(HANDLE file) {
 DWORD count;
 DWORD size;
 ReadFile(file, &count, 4, &size, 0);
 if(size!=4) return false;
 for(DWORD i=0;i<count;i++) {
  FakePerk fp;
  ReadFile(file,&fp,sizeof(FakePerk),&size,0);
  fakeTraits.push_back(fp);
 }
 ReadFile(file, &count, 4, &size, 0);
 if(size!=4) return false;
 for(DWORD i=0;i<count;i++) {
  FakePerk fp;
  ReadFile(file,&fp,sizeof(FakePerk),&size,0);
  fakePerks.push_back(fp);
 }
 ReadFile(file, &count, 4, &size, 0);
 if(size!=4) return false;
 for(DWORD i=0;i<count;i++) {
  FakePerk fp;
  ReadFile(file,&fp,sizeof(FakePerk),&size,0);
  fakeSelectablePerks.push_back(fp);
 }
 return true;
}

void _stdcall AddPerkMode(DWORD mode) {
 addPerkMode=mode;
}
DWORD _stdcall HasFakePerk(char* name) {
 for(DWORD i=0;i<fakePerks.size();i++) {
  if(!strcmp(name,fakePerks[i].Name)) {
   return fakePerks[i].Level;
  }
 }
 return 0;
}
DWORD _stdcall HasFakeTrait(char* name) {
 for(DWORD i=0;i<fakeTraits.size();i++) {
  if(!strcmp(name,fakeTraits[i].Name)) {
   return 1;
  }
 }
 return 0;
}
void _stdcall ClearSelectablePerks() {
 fakeSelectablePerks.clear();
 addPerkMode=2;
 SafeWrite32(0x0043C77D, 0x488cb);
 IgnoringDefaultPerks=0;
}
void PerksEnterCharScreen() {
 RemoveTraitID=-1;
 RemovePerkID=-1;
 RemoveSelectableID=-1;
}
void PerksCancelCharScreen() {
 if(RemoveTraitID!=-1) {
  fakeTraits.remove_at(RemoveTraitID); 
 }
 if(RemovePerkID!=-1) {
  if(!--fakePerks[RemovePerkID].Level) fakePerks.remove_at(RemovePerkID);
 }
}
void PerksAcceptCharScreen() {
 if(RemoveSelectableID!=-1) fakeSelectablePerks.remove_at(RemoveSelectableID);
}

#pragma once
#include <Windows.h>

#include "game_structures.h"
#include "memory.h"

namespace Zeal {
namespace GameUI {
static HANDLE *Heap = (HANDLE *)0x0080b420;

struct BaseVTable  // Equivalent to CXWnd's VTable in client.
{
  /*0000*/ LPVOID IsValid;
  /*0004*/ LPVOID Deconstructor;
  /*0008*/ LPVOID DrawNC;
  /*000C*/ LPVOID Draw;
  /*0010*/ LPVOID PostDraw;
  /*0014*/ LPVOID DrawCursor;
  /*0018*/ LPVOID DrawChildItem;
  /*001C*/ LPVOID DrawCaret;
  /*0020*/ LPVOID DrawBackground;
  /*0024*/ LPVOID DrawTooltip;
  /*0028*/ LPVOID GetMinimizedRect;
  /*002C*/ LPVOID DrawTitleBar;
  /*0030*/ LPVOID GetCursorToDisplay;
  /*0034*/ LPVOID HandleLButtonDown;
  /*0038*/ LPVOID HandleLButtonUp;
  /*003C*/ LPVOID HandleLButtonHeld;
  /*0040*/ LPVOID HandleLButtonUpAfterHeld;
  /*0044*/ LPVOID HandleRButtonDown;
  /*0048*/ LPVOID HandleRButtonUp;
  /*004C*/ LPVOID HandleRButtonHeld;
  /*0050*/ LPVOID HandleRButtonUpAfterHeld;
  /*0054*/ LPVOID HandleWheelButtonDown;
  /*0058*/ LPVOID HandleWheelButtonUp;
  /*005C*/ LPVOID HandleMouseMove;
  /*0060*/ LPVOID HandleWheelMove;
  /*0064*/ LPVOID TranslateKeyboardMsg;
  /*0068*/ LPVOID HandleKeyboardMsg;
  /*006C*/ LPVOID OnDragDrop;
  /*0070*/ LPVOID GetDragDropCursor;
  /*0074*/ LPVOID QueryDropOK;
  /*0078*/ LPVOID OnClickStick;
  /*007C*/ LPVOID OnClickStickCursor;
  /*0080*/ LPVOID QueryClickStickDropOK;
  /*0084*/ LPVOID WndNotification;
  /*0088*/ LPVOID Deactivate;
  /*008C*/ LPVOID OnShow;
  /*0090*/ LPVOID OnMove;
  /*0094*/ LPVOID OnResize;
  /*0098*/ LPVOID OnMinimizeBox;
  /*009C*/ LPVOID OnTileBox;
  /*00A0*/ LPVOID OnTile;
  /*00A4*/ LPVOID OnSetFocus;
  /*00A8*/ LPVOID OnKillFocus;
  /*00AC*/ LPVOID OnProcessFrame;
  /*00B0*/ LPVOID OnVScroll;
  /*00B4*/ LPVOID OnHScroll;
  /*00B8*/ LPVOID OnBroughtToTop;
  /*00BC*/ LPVOID OnActivate;
  /*00C0*/ LPVOID AboutToDeleteWnd;
  /*00C4*/ LPVOID RequestDockInfo;
  /*00C8*/ LPVOID GetTooltip;
  /*00CC*/ LPVOID HitTest;
  /*00D0*/ LPVOID GetHitTestRect;
  /*00D4*/ LPVOID GetInnerRect;
  /*00D8*/ LPVOID GetClientRect;
  /*00DC*/ LPVOID GetMinSize;
  /*00E0*/ LPVOID GetUntileSize;
  /*00E4*/ LPVOID IsPointTransparent;
  /*00E8*/ LPVOID SetDrawTemplate;
  /*00EC*/ LPVOID SetWindowTextA;
  /*00F0*/ LPVOID SetVScrollPos;
  /*00F4*/ LPVOID SetAttributesFromSidl;
  /*00F8*/ LPVOID OnReloadSidl;
};

struct CXPoint {
  int x;
  int y;
  CXPoint(int _x, int _y) : x(_x), y(_y){};
};

struct CXRect {
  int Left = 0;
  int Top = 0;
  int Right = 0;
  int Bottom = 0;
  // CXRect() {}
  // CXRect(int l, int t, int r, int b) { Left = l; Top = t; Right = r; Bottom = b; }
};

struct SidlScreenWndVTable : BaseVTable  // VTable for SidlWnd
{
  /*00FC*/ LPVOID LoadIniInfo;
  /*0100*/ LPVOID StoreIniInfo;
};

struct ButtonWndVTable : BaseVTable {
  /*0x0FC*/ LPVOID SetRadioGroup;
};

struct ItemDisplayVTable : SidlScreenWndVTable {
  /*0x104*/ LPVOID Activate;
};

struct ContextMenuVTable : BaseVTable  // Not a SidlScreenWnd derived class.  CListWnd has the same layout.
{
  /*0x0FC*/ LPVOID OnHeaderClick;
  /*0x100*/ LPVOID DrawColumnSeparators;
  /*0x104*/ LPVOID DrawSeparator;
  /*0x108*/ LPVOID DrawLine;
  /*0x10c*/ LPVOID DrawHeader;
  /*0x110*/ LPVOID DrawItem;
  /*0x114*/ LPVOID DeleteAll;
  /*0x118*/ LPVOID Compare;
  /*0x11c*/ LPVOID Sort;
};

struct pCXSTR {
  /*0x00*/ DWORD ReferenceCount;
  /*0x04*/ DWORD MaxLength;
  /*0x08*/ DWORD Length;
  /*0x0c*/ BOOL Encoding;  // 0: ASCII, 1: Unicode
  /*0x10*/ PCRITICAL_SECTION pLock;
  /*0x14*/ CHAR Text[1];  // Stub (in use is Length, allocated is MaxLength)
};

struct CXSTR {
  CXSTR() { Data = nullptr; };

  // These constructors are explicit since currently the destructor is commented out so
  // that any implicit construction will likely cause a reference count leak.
  explicit CXSTR(const char *data) {
    reinterpret_cast<void(__thiscall *)(const CXSTR *, const char *)>(0x575F30)(this, data);
  }

  explicit CXSTR(std::string data) {
    reinterpret_cast<void(__thiscall *)(const CXSTR *, const char *)>(0x575F30)(this, data.c_str());
  }

  void Assure(int length, int encoding) {
    reinterpret_cast<void(__thiscall *)(const CXSTR *, int, int)>(0x575A60)(this, length, encoding);
  }

  void FreeRep() {
    if (Data) reinterpret_cast<void(__thiscall *)(const CXSTR *, pCXSTR *)>(0x575DC0)(this, Data);
    Data = nullptr;
  }

  char *CastToCharPtr() const { return reinterpret_cast<char *(__thiscall *)(const CXSTR *)>(0x577E80)(this); }

  void Append(const std::string data) {
    reinterpret_cast<void(__thiscall *)(const CXSTR *, const char *)>(0x577310)(this, data.c_str());
  }

  void Set(const std::string data) {
    reinterpret_cast<void(__thiscall *)(const CXSTR *, const char *)>(0x576190)(this, data.c_str());
  }

  void Set(const char *data) {
    reinterpret_cast<void(__thiscall *)(const CXSTR *, const char *)>(0x576190)(this, data);
  }

  CXSTR &operator=(const CXSTR &other) {
    // Client operator= handles nullptrs, reference counting, and FreeRep() of old value.
    reinterpret_cast<pCXSTR *(__thiscall *)(CXSTR *, const CXSTR *)>(0x576140)(this, &other);
    return *this;
  }

  operator std::string() const {
    const char *result = CastToCharPtr();
    if (result)
      return std::string(result);
    else
      return "";
  }

  // To enable the destructor we likely need to delete the copy constructors and rely on
  // move operations to properly track the reference count (like across call by values).
  //~CXSTR()
  //{
  //	FreeRep();
  //}
  pCXSTR *Data;
};

struct GAMEFONT {
  /* 0x0000 */ DWORD Unknown0000;
  /* 0x0004 */ DWORD Size;
};

struct ARGBCOLOR {
  union {
    struct {
      BYTE B;
      BYTE G;
      BYTE R;
      BYTE A;
    };

    DWORD ARGB;
  };

  ARGBCOLOR(BYTE _R, BYTE _G, BYTE _B, BYTE _A) : A(_A), R(_R), G(_G), B(_B){};
  ARGBCOLOR(DWORD _ARGB) : ARGB(_ARGB){};
  ARGBCOLOR() : A{}, R{}, G{}, B{} {};
};

struct ControlTemplate {
  char Unknown0x0[0x20];
  pCXSTR *Item;
};

struct ListWndColInfo  // CListWnd::SetItemText strides by 7 * 4 bytes for each row.
{
  DWORD Unknown0x00;
  int ColCount;          // Used in SetItemText as the number of columns.
  DWORD Unknown0x08[5];  // Total size of 0x1c.
};

struct BasicWnd  // Equivalent to CXWnd in client.
{
  // BasicWnd() {};
  void Deconstruct(bool delete_me) {
    reinterpret_cast<void(__thiscall *)(const BasicWnd *, bool delete_me)>(vtbl->Deconstructor)(this, delete_me);
  }

  void SetFocus() { reinterpret_cast<void(__thiscall *)(const BasicWnd *)>(0x572290)(this); }

  void BringToFront() { reinterpret_cast<void(__thiscall *)(const BasicWnd *, int)>(0x573a80)(this, 1); }

  // The client BasicWnd constructor (CXWnd::CXWnd()) sets the vtbl to point at a default
  // BaseVTable located at 0x005eaa94. The constructors of other BasicWnd derived classes
  // can have different default tables (contents and a few extra entries), so make a new
  // copy of the initial table to allow modifications.
  void SetupCustomVTable(size_t size = sizeof(BaseVTable)) {
    BYTE *newtbl = new BYTE[size];
    mem::copy((int)newtbl, (int)vtbl, size);
    mem::unprotect_memory(newtbl, size);
    vtbl = reinterpret_cast<BaseVTable *>(newtbl);
  }

  void DeleteCustomVTable(BaseVTable *original = nullptr) {
    delete[] reinterpret_cast<BYTE *>(vtbl);
    vtbl = original ? original : reinterpret_cast<BaseVTable *>(0x005eaa94);  // ~CXWnd().
  }

  void show(int make_visible_flag, bool bring_to_top) {
    reinterpret_cast<void(__thiscall *)(const BasicWnd *, int, bool)>(0x572310)(this, make_visible_flag, bring_to_top);
  }

 protected:  // Block external use of the CXSTR argument version to "encourage" std::string.
  // Note: This is actually CSidlScreenWnd::GetChildItem() but that internally calls CXWnd::GetChildItem().
  BasicWnd *GetChildItem(CXSTR name, bool log_error = true) {
    if (!log_error) mem::write<BYTE>(0x570378, 0xEB);  // jump passed the ui error logging
    BasicWnd *wnd = reinterpret_cast<BasicWnd *(__thiscall *)(const BasicWnd *, CXSTR)>(0x570320)(this, name);
    if (!log_error) mem::write<BYTE>(0x570378, 0x75);  // restore the ui error logging
    return wnd;
  }

 public:
  BasicWnd *GetChildItem(const std::string &name, bool log_error = true) {
    CXSTR name_cxstr(name);  // GetChildItem calls FreeRep() internally.
    auto result = GetChildItem(name_cxstr, log_error);
    return result;
  }

  void CreateChildren() { reinterpret_cast<BasicWnd *(__thiscall *)(const BasicWnd *)>(0x56f4f0)(this); }

  int WndNotification(int sender, int event, int userdata) {
    return reinterpret_cast<int(__thiscall *)(const BasicWnd *, int, int, int)>(vtbl->WndNotification)(this, sender,
                                                                                                       event, userdata);
  }

  void MinimizeToggle() { reinterpret_cast<void(__thiscall *)(const BasicWnd *)>(vtbl->OnMinimizeBox)(this); }

  CXRect GetScreenRect() { return reinterpret_cast<CXRect(__thiscall *)(const BasicWnd *)>(0x005751C0)(this); }

  void DrawTooltipAtPoint(int left, int top) {
    reinterpret_cast<void(__thiscall *)(const BasicWnd *, int, int)>(0x00574800)(this, left, top);
  }

  void LeftClickDown(int mouse_x, int mouse_y, unsigned int flags = 0) {
    reinterpret_cast<int(__thiscall *)(const BasicWnd *, int x, int y, uint32_t)>(vtbl->HandleLButtonDown)(
        this, mouse_x, mouse_y, flags);
  }

  void LeftClickUp(int mouse_x, int mouse_y, unsigned int flags = 0) {
    reinterpret_cast<int(__thiscall *)(const BasicWnd *, int x, int y, unsigned)>(vtbl->HandleLButtonUp)(
        this, mouse_x, mouse_y, flags);
  }

  /* 0x0000 */ BaseVTable *vtbl;
  /* 0x0004 */ DWORD MouseHoverTimer;
  /* 0x0008 */ DWORD FadeDelay;
  /* 0x000C */ DWORD FadeDuration;
  /* 0x0010 */ BYTE FadedAlpha;  // Alpha transparency value when faded
  /* 0x0011 */ BYTE IsNotFaded;  // Set to 0 when faded, 1 when not faded
  /* 0x0012 */ BYTE IsLocked;
  /* 0x0013 */ BYTE LockEnable;         // Enable Lock option in CContextMenuManager::WarnDefaultMenu.
  /* 0x0014 */ BYTE DisableRightClick;  // Set to 1 to ignore RMB down (like in CChatWindow() for CW_ChatOutput).
  /* 0x0015 */ BYTE Unknown0015[0x3];
  /* 0x0018 */ DWORD Unknown0018;
  /* 0x001C */ struct SidlWnd *ParentWnd;
  /* 0x0020 */ struct SidlWnd *FirstChildWnd;
  /* 0x0024 */ struct SidlWnd *NextSiblingWnd;

  /* 0x0028 */ BYTE HasChildren;
  /* 0x0029 */ BYTE HasSiblings;
  /* 0x002A */ BYTE Unknown0030[2];
  /* 0x002C */ DWORD Flags;
  /* 0x0030 */ CXRect Location;
  /* 0x0040 */ CXRect LocationPlaceholder;  // used when minimizing the window
  /* 0x0050 */ BYTE IsVisible;              // show
  /* 0x0051 */ BYTE IsEnabled;
  /* 0x0052 */ BYTE IsMinimized;
  /* 0x0053 */ BYTE Unknown0053;
  /* 0x0054 */ BYTE IsOpen;
  /* 0x0055 */ BYTE Unknown0055;
  /* 0x0056 */ BYTE IsMouseOver;  // mouse is hovering over
  /* 0x0057 */ BYTE Unknown0057;
  /* 0x0058 */ DWORD WindowStyleFlags;
  /* 0x005C */ GAMEFONT *FontPointer;
  /* 0x0060 */ CXSTR Text;
  /* 0x0064 */ CXSTR ToolTipText;
  /* 0x0068 */ ARGBCOLOR TextColor;
  /* 0x006C */ ARGBCOLOR ToolTipTextColor;
  /* 0x006C */ BYTE Unknown0068[20];
  /* 0x0084 */ CXSTR XmlToolTipText;
  /* 0x0088 */ BYTE Unknown0088[20];
  /* 0x009C */ BYTE UnfadedAlpha;  // Alpha transparency value when active
  /* 0x009D */ BYTE Unknown009d;
  /* 0x009E */ BYTE AlphaTransparency;  // Current Alpha transparency
  /* 0x009F */ BYTE Unknown009F;
  /* 0x00A0 */ BYTE ZLayer;
  /* 0x00A1 */ BYTE Unknown00A1[7];
  /* 0x00A8 */ DWORD DrawTemplate;
  /*0x0ac*/ BYTE Unknown0x0ac[0x4];
  /*0x0b0*/ DWORD ZLayer2;
  /*0x0b4*/ BYTE Unknown0x0b4[0x28];
  /*0x0dc*/ DWORD FadeTickCount;
  /*0x0e0*/ BYTE Unknown0x0e0; /* CXWnd::StartFade */
  /*0x0e1*/ BYTE Unknown0x0e1; /* CXWnd::StartFade */
  /*0x0e2*/ BYTE Unknown0x0e2;
  /*0x0e3*/ BYTE Unknown0x0e3;
  /*0x0e4*/ DWORD Unknown0x0e4; /* CXWnd::StartFade, CXWnd::Minimize */
  /*0x0e8*/ DWORD VScrollMax;
  /*0x0ec*/ DWORD VScrollPos;
  /*0x0f0*/ DWORD HScrollMax;
  /*0x0f4*/ DWORD HScrollPos;
  /*0x0f8*/ BYTE ValidCXWnd;
  /*0x0f9*/ BYTE Unused0x0f9[0x3];
  /*0x0fc*/ union {
    struct ListWndColInfo *ColInfoArray;  // Row-length array of info structures.
    struct pCXSTR *SidlText;
    DWORD Items;
    struct ListWnd *CmbListWnd;
  };

  /*0x100*/ union {
    struct _CXSTR *SidlScreen;
    DWORD SlotID;
    DWORD Caret_Start;
    DWORD ItemCount;
  };

  union {
    /*0x104*/ LPVOID SidlPiece; /* CScreenPieceTemplate (important) */
    DWORD Caret_End;
  };

  /*0x108*/ BYTE Checked;
  /*0x109*/ BYTE Highlighted;
  /*0x10a*/ BYTE Unused0x10a[0x2];
  /*0x10c*/ DWORD TextureAnim; /* used in CSidlScreenWnd::AddButtonToRadioGroup */
  /*0x110*/ CXSTR InputText;
};

struct SidlManager {
  /*0x000*/ BYTE Unknown0x0000[0x7c];
  /*0x07c*/ CXSTR ErrorMsg;
  // Unknown size.
};

struct SidlWnd : BasicWnd {
  ~SidlWnd(){};
  SidlWnd(){};

  // The client wnd constructor (CSidlScreenWnd()) sets the vtbl to point at a default
  // SidlScreenWndVTable located at 0x005ea98c. The constructors of other derived classes
  // may have different default tables, so make a new specific copy to allow modification.
  // Note that if the derived class has an extended vtable size, use the BasicWnd versions
  // of these that support different sizes.
  void SetupCustomVTable() { BasicWnd::SetupCustomVTable(sizeof(SidlScreenWndVTable)); }

  static SidlScreenWndVTable *GetDefaultVTable() {
    auto *sidl_default_vtbl = reinterpret_cast<SidlScreenWndVTable *>(0x005ea98c);
    return sidl_default_vtbl;
  }

  // This should only be called immediately before destruction of custom SidlWnd classes
  // created with SetupCustomVTable().
  void DeleteCustomVTable() { BasicWnd::DeleteCustomVTable(GetDefaultVTable()); }

  /*0x114*/ DWORD Selector;
  /*0x118*/ DWORD PushToSelector;
  union {
    /*0x11c*/ DWORD EnableINIStorage;
    /*0x11c*/ int SelectedIndex;
  };

  /*0x120*/ union {
    CXSTR INIStorageName;
    struct _GAMEINVSLOT *pGameInvSlot;
  };
  /*0x124*/ struct CTextureAnimation *BackgroundTexture;
  /*0x128*/ DWORD Unknown0x128; /* CTextureAnimation */
  /*0x12c*/ int ContextMenu;    /* CTextureAnimation its an id for the menu*/
  /*0x130*/ BYTE Unknown0x130;  /* CTextureAnimation */
  /*0x131*/ BYTE Unknown0x131;  /* CTextureAnimation */
  /*0x132*/ BYTE Unknown0x132;  /* CTextureAnimation */
  /*0x133*/ BYTE Unknown0x133;  /* CTextureAnimation */
};

struct BuffWindowButton : SidlWnd {
  /* 0x0134 */ int Unknown1;
  /* 0x0138 */ struct CTextureAnimation *BuffIcon;  // Background stored in 0x124
};

struct BuffWindow : SidlWnd {
  /* 0x0134 */ BYTE Unknown00134;  // Initialization flag
  /* 0x0135 */ BYTE Unknown00135;  // Initialization flag
  /* 0x0136 */ BYTE Unknown00136;
  /* 0x0137 */ BYTE Unknown00137;
  /* 0x0138 */ struct CTextureAnimation *BlueIconBackground;
  /* 0x013C */ struct CTextureAnimation *RedIconBackground;
  /* 0x0140 */ struct CTextureAnimation *CTextureAnimations[GAME_NUM_BUFFS];
  /* 0x017C */ BuffWindowButton *BuffButtonWnd[GAME_NUM_BUFFS];
  /* 0x01B8 */ DWORD NextRefreshTime;
  /* 0x01BC */ DWORD Width;
  /* 0x01C0 */ DWORD Height;
  /* End    */
};

struct CharSelect : SidlWnd {
  /*0x134*/ BYTE Activated;  // Set to 1 when activated and 0 in Deactivate().
  /*0x135*/ BYTE Unknown0x135[0x3];
  /*0x138*/ SidlWnd *ExploreButton;
  /*0x13C*/ SidlWnd *RotateButton;
  /*0x140*/ SidlWnd *CharButtons[8];
  /*0x160*/ SidlWnd *EnterWorldButton;
  /*0x164*/ SidlWnd *DeleteButton;
  /*0x168*/ SidlWnd *QuitButton;
  /*0x16C*/ SidlWnd *ExploreModeWnd;
  /*0x170*/ BYTE Rotate;
  /*0x171*/ BYTE Explore;
  /*0x172*/ BYTE SetLocationByClass;
  /*0x173*/ BYTE Unknown0x173;
  /*0x174*/ DWORD SelectIndex;       // Set to 0xffffffff (-1) at ActivateStart().
  /*0x178*/ BYTE Unknown0x178[0x8];  // 0x180 allocated.
};

struct RaidWnd : SidlWnd {
  /*0x134*/ BYTE ToggleState;
  /*0x135*/ BYTE Unknown0x135[0x3b];
  /*0x170*/ DWORD ClassColors[15];  // D3DCOLOR colors (not in class_id order).
};

struct HotButton : SidlWnd {
  int GetPage() { return *(int *)0x7f69f6; }

  BYTE GetType(int button_index) { return *(BYTE *)(0x7f6862 + (button_index + (GetPage() * 0xA))); }
};

struct ComboWnd : BasicWnd  // Uses a BaseVTable.
{
  void DeleteAll() { reinterpret_cast<int(__thiscall *)(const ComboWnd *)>(0x5a18e0)(this); }

  void InsertChoice(const std::string &text) {
    CXSTR data(text);  // InsertChoice calls FreeRep() internally.
    reinterpret_cast<void(__thiscall *)(const ComboWnd *, CXSTR)>(0x5A1750)(this, data);
  }

  void SetChoice(int index) { reinterpret_cast<void(__thiscall *)(const ComboWnd *, int)>(0x5A1860)(this, index); }
};

struct ListWnd : BasicWnd  // Note ListWnd has same vtable structure as ContextMenu, so this declaration is incomplete.
{
  ListWnd(){};

  int AddString(std::string str) {
    CXSTR str_cxstr(str);  // AddString calls FreeRep() internally.
    return reinterpret_cast<int(__thiscall *)(const ListWnd *, CXSTR, UINT, UINT, UINT)>(0x5797A0)(this, str_cxstr,
                                                                                                   0xffffffff, 0, 0);
  }

  void SetItemText(std::string str, int row, int column) {
    CXSTR str_cxstr(str);  // SetItemText calls FreeRep() internally.
    reinterpret_cast<void(__thiscall *)(const ListWnd *, int, int, CXSTR)>(0x579DC0)(this, row, column, str_cxstr);
  }

  void SetItemData(int row)  // not sure why this is needed
  {
    reinterpret_cast<void(__thiscall *)(const ListWnd *, int, int)>(0x579D70)(this, row, row);
  }

  void Sort(int col) { reinterpret_cast<void(__thiscall *)(const ListWnd *, int)>(0x57cb00)(this, col); }

  void DeleteAll() { reinterpret_cast<void(__thiscall *)(const ListWnd *)>(0x579530)(this); }

  int GetItemData(int row) { return reinterpret_cast<int(__thiscall *)(const ListWnd *, int)>(0x578E80)(this, row); }

  std::string GetItemText(int row, int col) {
    Zeal::GameUI::CXSTR text;
    GetItemText(&text, row, col);
    std::string result = std::string(text);
    text.FreeRep();
    return result;
  }

 protected:  // Allow only internal use of the CXSTR* parameter verison.
  ListWnd *GetItemText(CXSTR *buffer, int row, int col) {
    return reinterpret_cast<ListWnd *(__thiscall *)(const ListWnd *, CXSTR *, int, int)>(0x578ed0)(this, buffer, row,
                                                                                                   col);
  }

 public:
  /* 0x0114 */ BYTE Unknown0114[0x08];
  /* 0x011c */ int SelectedIndex;
  // Incomplete list.
};

struct ItemDisplayWnd : SidlWnd {
 private:
  ItemDisplayWnd(){};  // Either use Create() or assign a pointer to an existing class.

 public:
  static constexpr uint32_t kDefaultVTableAddr = 0x005e5a98;

  // Inspired by ui_manager::create_sidl to use the same heap for destructor deletion support.
  static ItemDisplayWnd *Create(BasicWnd *parent_wnd = nullptr) {
    ItemDisplayWnd *wnd = reinterpret_cast<ItemDisplayWnd *>(HeapAlloc(*Zeal::GameUI::Heap, 0, sizeof(ItemDisplayWnd)));
    mem::set((int)wnd, 0, sizeof(ItemDisplayWnd));
    reinterpret_cast<void(__fastcall *)(ItemDisplayWnd *, int, BasicWnd *)>(0x00423331)(wnd, 0, parent_wnd);
    return wnd;
  }

  void Destroy() {
    reinterpret_cast<void(__thiscall *)(const ItemDisplayWnd *, bool)>(this->vtbl->Deconstructor)(this, true);
  }

  // The client ItemDisplayWnd constructor (CItemDisplayWnd::CItemDisplayWnd()) sets the vtbl to point at a
  // default ItemDisplayVTable located at 0x005e5a98. Copy that table over in order to modify it.
  void SetupCustomVTable() { BasicWnd::SetupCustomVTable(sizeof(ItemDisplayVTable)); }

  // This should only be called immediately before destruction of custom ItemDisplayWnd classes
  // created with SetupCustomVTable().
  void DeleteCustomVTable() {
    auto *item_display_default_vtbl = reinterpret_cast<ItemDisplayVTable *>(kDefaultVTableAddr);
    BasicWnd::DeleteCustomVTable(item_display_default_vtbl);
  }

  void Activate() {
    auto *vtable = static_cast<ItemDisplayVTable *>(vtbl);
    reinterpret_cast<void(__thiscall *)(const ItemDisplayWnd *)>(vtable->Activate)(this);
  }

  void Deactivate() { reinterpret_cast<void(__thiscall *)(const ItemDisplayWnd *)>(vtbl->Deactivate)(this); }

  /* 0x0134 */ struct SidlWnd *ItemDescription;  // the item stats text window
  /* 0x0138 */ BYTE Unknown0138[4];
  /* 0x013C */ struct SidlWnd *IconBtn;  // the item icon window
  /* 0x0140 */ BYTE IsActivated;         // 1 if activated, 0 if deactivated
  /* 0x0141 */ BYTE Unknown0140[3];
  /* 0x0144 */ CXSTR DisplayText;                            // the item name is the title text
  /* 0x0148 */ CXSTR WindowTitle;                            // the item stats text
  /* 0x014C */ Zeal::GameStructures::GAMEITEMINFOBASE Item;  // Copied into in SetItem() if flag set.
  /* 0x0230 */ BYTE ItemValid;                               // Set to 1 if Item was populated in SetItem() else 0.
  /* 0x0231 */ BYTE unknown0231[11];                         // Operator new has a total size of 0x23c bytes.
};

struct InvSlotWnd : BasicWnd  // Operator new of 0x12C bytes.
{
  static constexpr BaseVTable *default_vtable = reinterpret_cast<BaseVTable *>(0x005eb774);

  int HandleLButtonUp(int mouse_x, int mouse_y, unsigned int flags) {
    return reinterpret_cast<int(__thiscall *)(InvSlotWnd *, int, int, unsigned int)>(0x005a7b10)(this, mouse_x, mouse_y,
                                                                                                 flags);
  }

  int HandleRButtonUp(int mouse_x, int mouse_y, unsigned int flags) {
    return reinterpret_cast<int(__thiscall *)(InvSlotWnd *, int, int, unsigned int)>(0x005a7e80)(this, mouse_x, mouse_y,
                                                                                                 flags);
  }
  /* 0x0114 */ BYTE Unknown0114;
  /* 0x0115 */ BYTE Unknown0015;  // Passed in as param_3 to InvSlot::HandleLButtonUp().
  /* 0x0116 */ BYTE Unknown0116;
  /* 0x0117 */ BYTE Unknown0117;
  /* 0x0118 */ struct InvSlot *invSlot;
  /* 0x011C */ DWORD Unknown011c;
  /* 0x0120 */ DWORD IsActive;  // Set to 1 when mouse has triggered an interaction, 0 when released.
  /* 0x0124 */ DWORD Unknown0124;
  /* 0x0128 */ DWORD Unknown0128;
};

struct InvSlot  // Operator new of 0x14 bytes.
{
  int HandleLButtonUp() {
    int flag = this->invSlotWnd ? this->invSlotWnd->Unknown0015 : 0;
    return reinterpret_cast<int(__thiscall *)(InvSlot *, int unused_x, int unused_y, BYTE flag)>(0x421E48)(this, 0, 0,
                                                                                                           flag);
  }

  /* 0x0000 */ void *destructor;        // Pointer to pointer to CInvSlot::~CInvSlot.
  /* 0x0004 */ InvSlotWnd *invSlotWnd;  // Points back to "parent".
  /* 0x0008 */ void *textureAnimation;  // Also copied to invSlotWnd->TextureAnimation
  /* 0x000C */ DWORD Index;  // Index to CInvSlotMgr->invSlots[i]. This changes as containers are opened/closed.
  /* 0x0010 */ Zeal::GameStructures::GAMEITEMINFOBASE *Item;
};

struct MerchantWnd : SidlWnd  // operator_new(0x3e4)
{
  /*0x134*/ BYTE Activated;  // 1 = activated, 0 = deactivated
  /*0x135*/ BYTE Unknown0x135[0xb];
  /*0x140*/ DWORD Unknown0x140[0x50];  // Likely ItemInfo*. Deleted when deactivated.
  /*0x280*/ float MerchantGreed;
  /*0x284*/ DWORD InventoryItemSlot;
  /*0x288*/ Zeal::GameStructures::GAMEITEMINFO **ItemInfo;  // Item used in selling.
  /*0x28C*/ SidlWnd *MerchantName;                          // MW_MerchantName
  /*0x290*/ SidlWnd *SelectedItem;                          // MW_SelectedItem
  /*0x294*/ SidlWnd *BuyButton;                             // MW_Buy_Button
  /*0x298*/ SidlWnd *SellButton;                            // MW_Sell_Button
  /*0x29c*/ SidlWnd *InvSlotWnd[0x50];                      // Unsure if InvSlot vs InvSlotWnd
  /*0x3dc*/ SidlWnd *DoneButton;
  /*0x3e0*/ SidlWnd *MerchantSlotsWnd;  // MerchantSlotsWnd
};

struct SliderWnd : BasicWnd  // Uses a BaseVTable.
{
  SliderWnd(){};
  /* 0x0114 */ BYTE Unknown0114[0x20];
  /* 0x0134 */ int current_val;
  /* 0x0138 */ BYTE Unknown0138[4];
  /* 0x013C */ int max_val;
  /* 0x0140 */ int val3;
  /* 0x0144 */ BYTE Unknown0144[0xC];  // the item name is the title text. Operator new has a total size of 0x150 bytes.
};

struct TradeWnd : public SidlWnd {
  /* 0x0134 */ BYTE Activated;
  /* 0x0135 */ BYTE Unknown0135[0xb];
  /* 0x0140 */ SidlWnd *HisMoneyButtons[4];
  /* 0x0150 */ SidlWnd *MyMoneyButtons[4];
  /* 0x0160 */ SidlWnd *TradeButton;
  /* 0x0164 */ SidlWnd *CancelButton;
  /* 0x0168 */ SidlWnd *HisNameLabel;
  /* 0x016C */ SidlWnd *MyNameLabel;
  /* 0x0170 */ InvSlotWnd *TradeSlots[0x10];
  /* 0x01B0 */ DWORD Unknown01b0[0x4];  // Probably Their Money slots.
  /* 0x01C0 */ int MyPlatinum;
  /* 0x01C4 */ int MyGold;
  /* 0x01C8 */ int MySilver;
  /* 0x01CC */ int MyCopper;
  /* 0x01D0 */ Zeal::GameStructures::_GAMEITEMINFO *GiveItems[8];     // My item giving array.
  /* 0x01F0 */ Zeal::GameStructures::_GAMEITEMINFO *ReceiveItems[8];  // Probably their item array.
  /* 0x0210 */ BYTE Unknown0210;     // Set to 0 in constructor. Possibly accept status.
  /* 0x0211 */ BYTE Unknown0211;     // Set to 0 in constructor. Possibly accept status.
  /* 0x0212 */ BYTE Unknown0212[2];  // Operator new of 0x214 bytes.
};

struct LootWnd : public SidlWnd {
  void RequestLootSlot(UINT slot, BYTE inventory) {
    reinterpret_cast<void(__thiscall *)(const LootWnd *, UINT, bool)>(0x426b02)(this, slot, inventory);
  }

  /* 0x0134 */ DWORD Unk1;
  /* 0x0138 */ DWORD ItemSlotIndex[GAME_NUM_LOOT_WINDOW_ITEMS];
  /* 0x01B0 */ DWORD Timer;
  /* 0x01B4 */ PVOID Unknown01B4;
  /* 0x01B8 */ Zeal::GameStructures::_GAMEITEMINFO *Item[GAME_NUM_LOOT_WINDOW_ITEMS];
  /* 0x0230 */ BYTE Unknown0230[0x84];  // Operator new of 0x2b4 bytes.
};

struct EditWnd : public BasicWnd  // Note: this definition has a truncated vtbl.
{
  // void ReplaceSelection(const char* data, int length)
  //{
  //	reinterpret_cast<void(__thiscall*)(const EditWnd*, const char*, int)>(0x5a41b0)(this, data, length);
  // }
  void ReplaceSelection(const char *data, bool filter_input) {
    CXSTR data_cxstr(data);  // ReplaceSelection calls FreeRep() internally.
    reinterpret_cast<void(__thiscall *)(const EditWnd *, CXSTR, int)>(0x5a3f00)(this, data_cxstr, filter_input);
  }

  int GetInputLength() { return this->InputText.Data->Length - (this->item_link_count * 9); }

  void SetText(const std::string &text) {
    CXSTR data(text);  // SetText calls FreeRep() internally.
    reinterpret_cast<void(__thiscall *)(const EditWnd *, CXSTR)>(0x5a3d00)(this, data);
  }

  void AddItemTag(int item_id, const char *name) {
    reinterpret_cast<void(__thiscall *)(const EditWnd *, int, const char *)>(0x5a2920)(this, item_id, name);
  }

  /* 0x0114 */ DWORD LinkStartIndex[10];
  /* 0x013C */ DWORD LinkEndIndex[10];
  /* 0x0164 */ BYTE UnknownBytes[0xA0];
  /* 0x0204 */ DWORD item_link_count;
  /* 0x0208 */ BYTE uk1[3];
  /* 0x020B */ BYTE SomeFlag;
  /* 0x020C */ DWORD uk2;
  /* 0x0210 */ DWORD AlphaNumericOnly;
};

struct ChatWnd : public SidlWnd {
  /*0x134*/ class CChatManager *ChatManager;  // Points back so Deactivate can release itself.
  /*0x138*/ EditWnd *edit;                    // CW_ChatInput
  /*0x13C*/ EditWnd *ChatOutput;              // CW_ChatOutput
  /*0x140*/ BYTE Uknown0x140[0x240 - 0x140];
};

// onetimehero 09-17-03
// CContainerWnd
// Actual Size 0x17C in other client
class ContainerWnd : public SidlWnd {
 public:
  /*0x134*/ DWORD something;                                     // dont know maybe type or a counter/ID?;
  /*0x138*/ Zeal::GameStructures::GAMEITEMINFO *pContainerInfo;  // Type, capacity, etc.
  /*0x13c*/ InvSlotWnd *pSlotWnds[0x0a];
  /*0x164*/ SidlWnd *pCombine;
  /*0x168*/ SidlWnd *pDone;
  /*0x16c*/ SidlWnd *pIcon;
  /*0x170*/ SidlWnd *A_DragItem;
  /*0x174*/ SidlWnd *pLabel;
  /*0x178*/ BYTE Unknown0x178[4];
  /*0x17c*/
};

// Actual Size 0x54 in game.
class ContainerMgr {
 public:
  /*0x000*/ DWORD pvfTable;                     // NOT based on CXWnd.  Contains only destructor
  /*0x004*/ ContainerWnd *pPCContainers[0x11];  // All open containers, including World, in order of opening...
  /*0x048**/ Zeal::GameStructures::GAMEITEMINFO *pWorldItems;  // Only GAMEITEMCONTAINERINFO section is valid. Pointer
                                                               // to world bags, crafting stations. Null if none open.
  /*0x04c*/ DWORD Unknown0x04c;  // in the future this is ID of container in zone, starts at one (zero?) and goes up.
  /*0x050*/ DWORD dwTimeSpentWithWorldContainerOpen;  // Cumulative counter?
                                                      /*0x054*/

  void OpenContainer(Zeal::GameStructures::GAMEITEMINFO *container, int index) {
    reinterpret_cast<void(__thiscall *)(ContainerMgr *, Zeal::GameStructures::GAMEITEMINFO *, int)>(0x004168bd)(
        this, container, index);
  }

  void CloseContainer(Zeal::GameStructures::GAMEITEMINFO *container, bool close_window) {
    reinterpret_cast<void(__thiscall *)(ContainerMgr *, Zeal::GameStructures::GAMEITEMINFO *, bool)>(0x004169e5)(
        this, container, close_window);
  }

  int CloseAllContainers() { return reinterpret_cast<int(__thiscall *)(ContainerMgr *)>(0x00416a43)(this); }
};

struct CInvSlotMgr {
 public:
  InvSlot *FindInvSlot(int slot_id) {
    return reinterpret_cast<InvSlot *(__thiscall *)(CInvSlotMgr *, int)>(0x423010)(this, slot_id);
  }

  /* 0x000 */ DWORD *Unknonwn0000;
  /* 0x004 */ InvSlot *InvSlots[450];
  /* 0x70C */ DWORD NumInvSlots;
};

struct ColorPickerWnd : public SidlWnd {
  void Activate(BasicWnd *wnd, DWORD color) {
    reinterpret_cast<void(__thiscall *)(const ColorPickerWnd *, BasicWnd *, DWORD)>(0x414F2A)(this, wnd, color);
  };

  void SetCurrentcolor(DWORD color) {
    reinterpret_cast<void(__thiscall *)(const ColorPickerWnd *, DWORD)>(0x414F87)(this, color);
  };
};

struct GameKey {
  /* 0x0000 */ UINT32 code;
  /* 0x0004 */ int timestamp;
  /* 0x0008 */ int unk;
  /* 0x000C */ int isDown;
  /* 0x0010 */ int whoknows;
};

struct CTextureFont {
  // Internal use only method for passing CXSTR through.
  int DrawWrappedText(CXSTR str, CXRect rect1, CXRect rect2, unsigned long color, unsigned short unk1, int unk2) const {
    return reinterpret_cast<int(__thiscall *)(const CTextureFont *, CXSTR, CXRect, CXRect, unsigned long,
                                              unsigned short, int)>(0x005a4a30)(this, str, rect1, rect2, color, unk1,
                                                                                unk2);
  }

  // Convenience function with const char* to CXSTR handling.
  int DrawWrappedText(const char *text, CXRect rect1, CXRect rect2, unsigned long color, unsigned short unk1,
                      int unk2) const {
    CXSTR str(text);  // DrawWrappedText calls FreeRep() internally.
    return this->DrawWrappedText(str, rect1, rect2, color, unk1, unk2);
  }

  int GetHeight() { return reinterpret_cast<int(__thiscall *)(const CTextureFont *)>(0x5A4930)(this); }
};

struct CXWndManager {
  // get font
  CTextureFont *GetFont(int index) {
    return reinterpret_cast<CTextureFont *(__thiscall *)(const CXWndManager *, int)>(0x538EAA)(this, index);
  }

  /* 0x0000 */ DWORD Unknown0x0;
  /* 0x0004 */ DWORD Unknown0x4;
  /* 0x0008 */ DWORD Unknown0x8;
  /* 0x000C */ DWORD Unknown0xC;
  /* 0x0010 */ DWORD Unknown0x10;
  /* 0x0014 */ GameKey LastKey;
  /* 0x0028 */ EditWnd *ActiveEdit;
  /* 0x002C */ int unknown1;
  /* 0x0030 */ SidlWnd *Focused;
  /* 0x0034 */ DWORD Unknown34;
  /* 0x0038 */ DWORD Unknown38;
  /* 0x003C */ SidlWnd *Hovered;
  /* 0x0040 */ DWORD Unknown40[3];
  /* 0x004C */ int AbsMouseX;
  /* 0x0050 */ int AbsMouseY;
  /* 0x0054 */ BYTE CapsLockState;
  /* 0x0055 */ BYTE ShiftKeyState;    // GetKeyboardFlags() bit 0x01
  /* 0x0056 */ BYTE ControlKeyState;  // GetKeyboardFlags() bit 0x02
  /* 0x0057 */ BYTE AltKeyState;      // GetKeyboardFlags() bit 0x04
  /* 0x0058 */ BYTE unknown58;        // GetKeyboardFlags() bit 0x08
  /* 0x0059 */ BYTE unknown59[3];
  /* 0x005C */ DWORD unknown5C;            // Accessed in ShowCursor
  /* 0x0060 */ DWORD unknown60[0x50 / 4];  // Accessed in ShowCursor
  /* 0x00B0 */ DWORD unknownB0;            // Accessed in ShowCursor
  /* 0x00B4 */ DWORD unknownB4[2];
  /* 0x00BC */ DWORD ScreenWidth;
  /* 0x00C0 */ DWORD ScreenHeight;
  /* 0x00C4 */ DWORD unknownC4;  // Pointer.
  /* 0x00C8 */ DWORD unknownC8;
};

class ContextMenu {
 private:
  ContextMenu() {}  // Either use Create() or assign a pointer to an existing class.

  static void __fastcall CustomDestructor(void *menu, int unusedEDX, bool delete_me) {
    BasicWnd *wnd = reinterpret_cast<BasicWnd *>(menu);  // ContextMenu inherits from BasicWnd.
    delete reinterpret_cast<ContextMenuVTable *>(wnd->vtbl);
    wnd->vtbl = reinterpret_cast<BaseVTable *>(0x005e4a24);  // Set back to default ContextMenu::vtable.
    reinterpret_cast<void(__thiscall *)(const BasicWnd *, bool)>(wnd->vtbl->Deconstructor)(wnd, delete_me);
  };

 public:
  // Inspired by ui_manager::create_sidl to use the same heap for destructor deletion support.
  // Useful for classes that require custom vtables but not custom destructors in that table.
  static ContextMenu *Create(int cxwnd, int a1, CXRect r) {
    ContextMenu *menu = reinterpret_cast<ContextMenu *>(HeapAlloc(*Zeal::GameUI::Heap, 0, sizeof(ContextMenu)));
    if (!menu) return nullptr;
    mem::set((int)menu, 0, sizeof(ContextMenu));
    reinterpret_cast<void(__fastcall *)(ContextMenu *, int, int, int, CXRect)>(0x417785)(menu, 0, cxwnd, a1, r);
    ContextMenuVTable *newtbl = new ContextMenuVTable();
    if (!newtbl) return nullptr;
    mem::copy((int)newtbl, (int)menu->fnTable, sizeof(ContextMenuVTable));
    mem::unprotect_memory(newtbl, sizeof(ItemDisplayVTable));
    menu->fnTable = newtbl;
    menu->fnTable->Deconstructor = CustomDestructor;
    return menu;
  }

  void Destroy() {
    reinterpret_cast<void(__thiscall *)(const ContextMenu *, bool)>(this->fnTable->Deconstructor)(this, true);
  }

  void AddSeparator() const { reinterpret_cast<void(__thiscall *)(const ContextMenu *)>(0x417A41)(this); }

  void EnableMenuItem(int index, bool toggle) {
    reinterpret_cast<void(__thiscall *)(const ContextMenu *, int, bool)>(0x417a93)(this, index, toggle);
  }

  void EnableLine(int index, bool toggle)  // this one doesn't forcibly change the color
  {
    reinterpret_cast<void(__thiscall *)(const ContextMenu *, int, bool)>(0x579f90)(this, index, toggle);
  }

  int AddMenuItem(const std::string &data, int index, bool disp_activated = false, bool has_children = false) const {
    CXSTR data_cxstr(data);  // AddMenuItem calls FreeRep() internally.
    int result = reinterpret_cast<int(__thiscall *)(const ContextMenu *, CXSTR, int, bool)>(0x417910)(
        this, data_cxstr, has_children ? index | 0x80000000 : index, disp_activated);
    return result;
  }

  void SetItemColor(int index, ARGBCOLOR color) {
    reinterpret_cast<void(__thiscall *)(const ContextMenu *, int, int, ARGBCOLOR)>(0x579eb0)(this, index, 0x1, color);
    reinterpret_cast<void(__thiscall *)(const ContextMenu *, int, int, ARGBCOLOR)>(0x579eb0)(this, index, 0x2, color);
  }

  void RemoveAllMenuItems() { reinterpret_cast<void(__thiscall *)(const ContextMenu *)>(0x417a7f)(this); }

  void CheckMenuItem(int row, bool check, bool uncheck_other_rows = false) {
    reinterpret_cast<void(__thiscall *)(const ContextMenu *, int, bool, bool)>(0x417ae8)(this, row, check,
                                                                                         uncheck_other_rows);
  }

  /*0x000*/ ContextMenuVTable *fnTable;
  /*0x004*/ DWORD Unknown0x004;  /* set to 0 in CXWnd::Refade*/
  /*0x008*/ DWORD TimeMouseOver; /* "Delay" in ini*/
  /*0x00c*/ DWORD FadeDuration;  /* "Duration" in ini*/
  /*0x010*/ BYTE FadeToAlpha;    /* set to 1 in CXWnd::Refade */
  /*0x011*/ BYTE Unknown0x011;   /* Faded? */
  /*0x012*/ BYTE Locked;
  /*0x013*/ BYTE Unknown0x013;
  /*0x014*/ BYTE Clickable;
  /*0x015*/ BYTE Unknown0x015;
  /*0x016*/ BYTE Unknown0x016;
  /*0x017*/ BYTE Unknown0x017;
  /*0x018*/ BYTE Unknown0x018[0x04];
  /*0x01c*/ struct _CSIDLWND *pParentWindow; /* If this is NULL, coordinates are absolute...*/
  /*0x020*/ struct _CSIDLWND *pChildren;
  /*0x024*/ struct _CSIDLWND *pSiblings; /* its a tree.*/
  /*0x028*/ BYTE HasChildren;            /*CXWnd__GetFirstChildWnd*/
  /*0x029*/ BYTE HasSiblings;            /*CXWnd__GetNextSib*/
  /*0x02a*/ BYTE Unknown0x02a[0x2];
  /*0x02c*/ DWORD XMLIndex;
  /*0x030*/ RECT Location;
  /*0x040*/ RECT OldLocation;
  /*0x050*/ BYTE dShow;
  /*0x051*/ BYTE Enabled;
  /*0x052*/ BYTE Minimized;
  /*0x053*/ BYTE Unknown0x053; /*ontilebox*/
  /*0x054*/ BYTE Unknown0x054;
  /*0x055*/ BYTE Unknown0x055;
  /*0x056*/ BYTE MouseOver;
  /*0x057*/ BYTE Unknown0x057;
  /*0x058*/ DWORD
  WindowStyle; /* bit 1 - vertical scroll, bit 2 - horizontal scroll, bit 4 - title bar?, bit 8 - border*/
  /*0x05c*/ GAMEFONT *TextureFont; /*its a CTextureFont* */
  /*0x060*/ struct _CXSTR *WindowText;
  /*0x064*/ struct _CXSTR *Tooltip;
  /*0x068*/ DWORD UnknownCW;   /* CXWnd::SetLookLikeParent*/
  /*0x06c*/ ARGBCOLOR BGColor; /* "BGTint.Red", green, blue*/
  /*0x070*/ DWORD Unknown0x070;
  /*0x074*/ BYTE Unknown0x074[0x4];
  /*0x078*/ FLOAT Unknown0x078;
  /*0x07C*/ BYTE Unknown0x07C[0x4];
  /*0x080*/ DWORD BGType; /* "BGType" in ini */
  /*0x084*/ struct _CXSTR *XMLToolTip;

  /*0x088*/ BYTE Unknown0x088[0x14];
  /*0x09c*/ BYTE Alpha; /* "Alpha" in ini */
  /*0x09d*/ BYTE Fades; /* "Fades" in ini */
  /*0x09e*/ BYTE Unknown0x0aa;
  /*0x09f*/ BYTE Unknown0x0ab;
  /*0x0a0*/ BYTE Unknown0x0a0[0x8];
  /*0x0a8*/ LPVOID DrawTemplate;
  /*0x0ac*/ BYTE Unknown0x0ac[0x4];
  /*0x0b0*/ DWORD ZLayer;
  /*0x0b4*/ BYTE Unknown0x0b4[0x28];
  /*0x0dc*/ DWORD FadeTickCount;
  /*0x0e0*/ BYTE Unknown0x0f8; /* CXWnd::StartFade */
  /*0x0e1*/ BYTE Unknown0x0f9; /* CXWnd::StartFade */
  /*0x0e2*/ BYTE Unknown0x0fa;
  /*0x0e3*/ BYTE Unknown0x0fb;
  /*0x0e4*/ DWORD Unknown0x0fc; /* CXWnd::StartFade, CXWnd::Minimize */
  /*0x0e8*/ DWORD VScrollMax;
  /*0x0ec*/ DWORD VScrollPos;
  /*0x0f0*/ DWORD HScrollMax;
  /*0x0f4*/ DWORD HScrollPos;
  /*0x0f8*/ BYTE ValidCXWnd;
  /*0x0f9*/ BYTE Unused0x0f9[0x3];
  /*0x0fc*/ union {
    struct _CXSTR *SidlText;
    DWORD Items;
  };

  /*0x100*/ union {
    struct _CXSTR *SidlScreen;
    DWORD SlotID;
  };
  /*0x104*/ LPVOID SidlPiece; /* CScreenPieceTemplate (important) */
  /*0x108*/ BYTE Checked;
  /*0x109*/ BYTE Highlighted;
  /*0x10a*/ BYTE Unused0x10a[0x2];
  /*0x10c*/ DWORD TextureAnim; /* used in CSidlScreenWnd::AddButtonToRadioGroup */
  /*0x110*/ struct _CXSTR *InputText;

  /*0x114*/ DWORD Selector;
  /*0x118*/ DWORD PushToSelector;
  /*0x11c*/ int SelectedIndex;
  /*0x120*/ union {
    struct _CXSTR *INIStorageName;
    struct _GAMEINVSLOT *pGameInvSlot;
  };

  /*0x124*/ DWORD Unknown0x124; /* CTextureAnimation */
  /*0x128*/ DWORD Unknown0x128; /* CTextureAnimation */
  /*0x12c*/ DWORD ContextMenu1; /* CTextureAnimation its an id for the menu*/
  /*0x130*/ int *Unknown0x130;  /* CTextureAnimation */
  /*0x134*/ BYTE Unknown0x134;
  /*0x135*/ BYTE Unknown0x135;
  /*0x136*/ BYTE Unknown0x136;
  /*0x137*/ BYTE Unknown0x137;
  /*0x138*/ BYTE Unknown0x138;
  /*0x139*/ BYTE Unknown0x139;
  /*0x13a*/ BYTE Unknown0x13a;
  /*0x13b*/ BYTE Unknown0x13b;
  /*0x13c*/ int *Unknown0x13c;
  /*0x140*/ int *Unknown0x140;
  /*0x144*/ BYTE Unknown0x144;
  /*0x145*/ BYTE Unknown0x145;
  /*0x146*/ BYTE Unknown0x146;
  /*0x147*/ BYTE Unknown0x147;
  /*0x148*/ int *Unknown0x148;
  /*0x14c*/ DWORD Unknown0x14c;
  /*0x150*/ int *Unknown0x150;
  /*0x154*/ DWORD Unknown0x154;
  /*0x158*/ int *Unknown0x158;
  /*0x15c*/ DWORD Unknown0x15c;
  /*0x160*/ int *Unknown0x160;
  /*0x164*/ DWORD Unknown0x164;
  /*0x168*/ int *Unknown0x168;
  /*0x16c*/ DWORD Unknown0x16c;
  /*0x170*/ int *Unknown0x170;
  /*0x174*/ DWORD Unknown0x174;
  /*0x178*/ int *Unknown0x178;
  /*0x17c*/ DWORD Unknown0x17c;
};

class CChatManager  // : public SidlWnd
{
 public:  // this class is a complete hack lol
  struct ChatWnd *GetActiveChatWindow() const {
    return reinterpret_cast<struct ChatWnd *(__thiscall *)(const CChatManager *)>(0x41114A)(this);
  }

  void CreateChatWindow() const { reinterpret_cast<void(__thiscall *)(const CChatManager *)>(0x410C5A)(this); }

  void FreeChatWindow(struct ChatWnd *wnd) const {
    reinterpret_cast<void(__thiscall *)(const CChatManager *, struct ChatWnd *)>(0x41110C)(this, wnd);
  }

  void CreateChatWindow(const char *name, int language, int default_channel, int chat_channel, const char *tell_target,
                        int font) const {
    reinterpret_cast<void(__thiscall *)(const CChatManager *, const char *name, int language, int default_channel,
                                        int chat_channel, const char *tell_target, int font)>(0x410e84)(
        this, name, language, default_channel, chat_channel, tell_target, font);
  }

  /*0x000*/ ChatWnd *ChatWindows[32];
  /*0x080*/ DWORD MaxChatWindows;
  /*0x084*/ int ActiveChatWnd;
  /*0x088*/ int AlwaysChatHereIndex;
  /*0x08C*/ int unknown;
  /*0x090*/ ChatWnd *ChannelMapWnd[0x29];
  /*0x134*/ BYTE unknown0x134[0xa4];
  /*0x1d8*/ int MyHitsMode;
  /*0x1dc*/ BYTE unknown0x1dc[0xc];
  /*0x1e8*/ int OthersHitsMode;
  /*0x1ec*/ BYTE unknown0x1ec[0xc];
  /*0x1f8*/  // Operator new of 0x1f8 in client.
};

class CContextMenuManager {
 public:  // this class is a complete hack lol
  int GetDefaultMenuIndex() const {
    return reinterpret_cast<int(__thiscall *)(const CContextMenuManager *)>(0x4137C2)(this);
  }

  int AddMenu(ContextMenu *context_menu) const {
    return reinterpret_cast<int(__thiscall *)(const CContextMenuManager *, ContextMenu *)>(0x417ED4)(this,
                                                                                                     context_menu);
  }

  int PopupMenu(int index, CXPoint pt, ContextMenu *menu) {
    return reinterpret_cast<int(__thiscall *)(const CContextMenuManager *, int, CXPoint, ContextMenu *)>(0x41822D)(
        this, index, pt, menu);
  }

  int RemoveMenu(int menu_index, bool remove_children) {
    return reinterpret_cast<int(__thiscall *)(const CContextMenuManager *, int, bool)>(0x417E1B)(this, menu_index,
                                                                                                 remove_children);
  }

  /*0x0000*/ BYTE Unknown0x0000[0x128];  // yeah i know its a window...
  /*0x0128*/ void *Menus[0x400];
  /*0x1128*/ DWORD MenuCount;
};

class OptionsWnd : public SidlWnd {
 public:
  static constexpr SidlScreenWndVTable *default_vtable = reinterpret_cast<SidlScreenWndVTable *>(0x005e60f0);

  struct KeyMapPair {
    CXSTR name;    // Descriptive name show in UI.
    int category;  // Bitfield with key category (can be multiple).
  };

  // Maps the KeyMapPairs to keyboard page lists of rows and columns per category.
  void UpdateKeyboardAssignmentList() { reinterpret_cast<void(__thiscall *)(const OptionsWnd *)>(0x0042b07b)(this); }

  /*0x134*/ void *Subwindows;
  /*0x138*/ void *GeneralPage;
  /*0x13C*/ void *FastItemDestroyCheckbox;
  /*0x140*/ void *GuildInvitesCheckbox;
  /*0x144*/ void *LootAutoSplitCheckbox;
  /*0x148*/ void *AnonymousCheckbox;
  /*0x14C*/ void *RolePlayingCheckbox;
  /*0x150*/ void *PlayerTradeCombobox;
  /*0x154*/ void *ItemDroppingCombobox;
  /*0x158*/ void *SoundRealismSlider;
  /*0x15C*/ void *SoundRealismValueLabel;
  /*0x160*/ void *MusicVolumeSlider;
  /*0x164*/ void *MusicVolumeValueLabel;
  /*0x168*/ void *SoundVolumeSlider;
  /*0x16C*/ void *SoundVolumeValueLabel;
  /*0x170*/ void *LoadSkinButton;
  /*0x174*/ void *DisplayPage;
  /*0x178*/ void *PCNamesCheckbox;
  /*0x17C*/ void *NPCNamesCheckbox;
  /*0x180*/ void *LevelOfDetailCheckbox;
  /*0x184*/ void *GammaSlider;
  /*0x188*/ void *GammaValueLabel;
  /*0x18C*/ void *ClipPlaneSlider;
  /*0x190*/ void *ClipPlaneValueLabel;
  /*0x194*/ void *VideoModesButton;
  /*0x198*/ void *ParticleDensityCombobox;
  /*0x19C*/ void *SkyCombobox;
  /*0x1A0*/ void *FadeDelaySlider;
  /*0x1A4*/ void *FadeDelayValueLabel;
  /*0x1A8*/ void *FadeDurationSlider;
  /*0x1AC*/ void *FadeDurationValueLabel;
  /*0x1B0*/ void *WindowAlphaSlider;
  /*0x1B4*/ void *WindowAlphaValueLabel;
  /*0x1B8*/ void *FadeToAlphaSlider;
  /*0x1BC*/ void *FadeToAlphaValueLabel;
  /*0x1C0*/ void *SpellParticleDensityCombobox;
  /*0x1C4*/ void *SpellParticleNearClipCombobox;
  /*0x1C8*/ void *SpellParticleOpacitySlider;
  /*0x1CC*/ void *SpellParticleOpacityValueLabel;
  /*0x1D0*/ int GlobalAlpha;
  /*0x1D4*/ int GlobalFadeToAlpha;
  /*0x1D8*/ int GlobalFadeDelay;
  /*0x1DC*/ int GlobalFadeDuration;
  /*0x1E0*/ int Unknown_0x1e0;  // Set to 0 in constructor.
  /*0x1E4*/ void *MousePage;
  /*0x1E8*/ void *InvertYAxisCheckbox;
  /*0x1EC*/ void *LookSpringCheckbox;
  /*0x1F0*/ void *MouseLookCheckbox;
  /*0x1F4*/ void *MouseSensitivitySlider;
  /*0x1F8*/ void *MouseSensitivityValueLabel;
  /*0x1FC*/ void *KeyboardPage;
  /*0x200*/ void *KeyboardFilterCombobox;
  /*0x204*/ void *KeyboardAssignmentList;
  /*0x208*/ void *DefaultKeysButton;
  /*0x20C*/ KeyMapPair KeyMaps[256];  // Allocates 256 pairs (through 0xa0c).
  /*0xA0C*/ int KeyAssignmentRow;     // Set to -1 in constructor. List row.
  /*0xA10*/ int KeyAssignmentColumn;  // Set to -1 in constructor. List col: 1 = primary, 2 = alt key.
  /*0xA14*/ void *ChatPage;
  /*0xA18*/ void *ChatArray[10];  // Chat0 to Chat9.
  /*0xA40*/ void *DamageShields;
  /*0xA44*/ void *NPCSpells;
  /*0xA48*/ void *PCSpellsComboBo;
  /*0xA4C*/ void *BardSongsComboBox;
  /*0xA50*/ void *CriticalSpellsComboBox;
  /*0xA54*/ void *CriticalMeleeComboBox;
  /*0xA58*/ void *ColorPage;
  /*0xA5C*/ int Unknown0xA5c;
  /*0xA60*/ void *UserColorDefault;
  /*0xA64*/ void *UserColorArray[0x48];  // UserColor0 to UserColor71.
  /*0xB84*/ int Unknown0xb84;            // Possibly activated?
  /*0xB88*/ int Timestamp;               // Set to diplay tick time in constructor.
  /*0xB8C*/                              // Size of new allocation.
};

class SpellBookWnd : public SidlWnd {
 public:
  static constexpr SidlScreenWndVTable *default_vtable = reinterpret_cast<SidlScreenWndVTable *>(0x005e6e48);

  void BeginMemorize(int book_index, int gem_index, bool unsure) const {
    reinterpret_cast<void(__thiscall *)(const SidlWnd *, int, int, bool)>(0x434a05)(this, book_index, gem_index,
                                                                                    unsure);
  }

  void StopSpellBookAction() const  // Aborts memorization or scribing.
  {
    reinterpret_cast<void(__thiscall *)(const SidlWnd *)>(0x00435531)(this);
  }

  void Activate() const { reinterpret_cast<void(__thiscall *)(const SidlWnd *)>(0x0043441f)(this); }

  void Deactivate() const { reinterpret_cast<void(__thiscall *)(const SidlWnd *)>(0x0043450a)(this); }

  int WndNotification(const BasicWnd *src_wnd, int param_2, void *param_3) {
    return reinterpret_cast<int(__thiscall *)(SpellBookWnd *, const BasicWnd *, int, void *)>(0x004345cb)(
        this, src_wnd, param_2, param_3);
  }

  void DisplaySpellInfo(const BasicWnd *src_wnd) {
    reinterpret_cast<int(__thiscall *)(SpellBookWnd *, const BasicWnd *)>(0x00435234)(this, src_wnd);
  }

  /*0x134*/ BYTE Activated;  // Set to 1 when activated and 0 in Deactivate().
  /*0x135*/ BYTE Unknown0x135[3];
  /*0x138*/ DWORD SpellBookIndex;
  /*0x13C*/ DWORD Unknown0x13C;
  /*0x140*/ DWORD MemorizingSpellIndex;
  /*0x144*/ DWORD MemorizingSpellId;
  /*0x148*/ DWORD MemTicksLeft;
  /*0x14C*/ DWORD ScribeIndex;  // Unsure.
  /*0x150*/ DWORD ScribeTicksLeft;
  /*0x154*/ DWORD CachedSittingState;
  /*0x158*/ DWORD Timestamp;  // Used in process frame to trigger StopSpellBookAction.
};

// TODO: The current BasicWnd definition is not the true base class and should probably be
// truncated down to 0xfc bytes. Instead of refactoring all of the classes, just repeating
// the necessary fields and methods in SpellGemWnd to enable recast time tip functionality.
struct SpellGemWnd {  // Total allocated size of 0x188 bytes.

  CXRect GetScreenRect() { return reinterpret_cast<CXRect(__thiscall *)(const SpellGemWnd *)>(0x005751C0)(this); }

  void DrawTooltipAtPoint(int left, int top) {
    reinterpret_cast<void(__thiscall *)(const SpellGemWnd *, int, int)>(0x00574800)(this, left, top);
  }

  /*0x000*/ BaseVTable *vtbl;
  /*0x004*/ BYTE unknown0004[0x60];  // Likely BasicWnd up to 0x0fc.
  /*0x064*/ CXSTR ToolTipText;
  /*0x068*/ BYTE unknown0068[0x94];      // Likely BasicWnd up to 0x0fc.
  /*0x0fc*/ DWORD unknown00fc;           // Set to 0 in Init().
  /*0x100*/ DWORD unknown0100;           // Set to 0 in Init(). Enables mouse over brightness boost.
  /*0x104*/ DWORD unknown0104;           // Set to 0 in Init().
  /*0x108*/ DWORD fadeBackground[0x0b];  // Calculated fading background tint values from SetSpellGemTint().
  /*0x134*/ DWORD unknown0134;           // Set to 0xff. Possibly initial fadevalues2.
  /*0x138*/ DWORD fadeIcon[0x0a];        // Linear ramp down from 0xea to 0x18 by -0x15. Alpha values for icon.
  /*0x160*/ DWORD spellIconOffsetX;      // Set by template parameter in constructor and SetAttributesFromSidl.
  /*0x164*/ DWORD spellIconOffsetY;      // Set by template parameter in constructor amd SetAttributesFromSidl.
  /*0x168*/ DWORD spellicon;   // same as in lucys db if this is equal to FFFFFFFF there is no spell memmed in this
                               // slot...
  /*0x16c*/ DWORD spellstate;  // 1 = cast in progress or refreshtime not met or gem is empty 2 means we ducked or
                               // aborted cast, 0 means its ok to cast
  /*0x170*/ DWORD
  gemTintStage;  // Index into fade values. Counts up to 10 in state 1, else down to 0 and resets state to 0 at 0.
  /*0x174*/ void *textureIcon;  // TextureAnimation for spell icon.
  /*0x178*/ DWORD unknown0178;  // Set to 0 in constructor.
  /*0x17c*/ void
      *textureBackground;  // TextureAnimation set by template parameter in constructor. Also SetAttributesFromSidl.
  /*0x180*/ void *textureHolder;  // Empty gem texture animation. Set in SetAttributesFromSidl.
  /*0x184*/ void
      *textureHighlight;  // Set by template parameter in constructor. Also SetAttributesFromSidl. Possibly Highlight.
};

class CastSpellWnd : public SidlWnd  // Aka CastSpellWnd in client.
{
 public:
  static constexpr SidlScreenWndVTable *default_vtable = reinterpret_cast<SidlScreenWndVTable *>(0x005e41ac);

  void Forget(int index) const {
    reinterpret_cast<void(__thiscall *)(const CastSpellWnd *, int)>(0x40a662)(this, index);
  }

  void UpdateSpellGems(int index) const {
    reinterpret_cast<void(__thiscall *)(const CastSpellWnd *, int)>(0x40a8b7)(this, index);
  }

  int WndNotification(const BasicWnd *src_wnd, int param_2, void *param_3) {
    return reinterpret_cast<int(__thiscall *)(CastSpellWnd *, const BasicWnd *, int, void *)>(0x0040a32a)(
        this, src_wnd, param_2, param_3);
  }

  void HandleSpellInfoDisplay(const BasicWnd *src_wnd) {
    reinterpret_cast<int(__thiscall *)(CastSpellWnd *, const BasicWnd *)>(0x0040a480)(this, src_wnd);
  }

  /*0x134*/ BYTE Unknown0x134[0x08];
  /*0x13C*/ SpellGemWnd *SpellSlots[0x8];
  /*0x15C*/ SidlWnd *SpellBook;
};

class QuantityWnd : public SidlWnd {
 public:
  /*0x134*/ BYTE Activated;  // Set to 1 when activated and 0 in Deactivate().
  /*0x135*/ BYTE Unknown0x135[3];
  /*0x138*/ DWORD Unknown0x138;
  /*0x13C*/ DWORD Unknown0x13C;
  /*0x140*/ DWORD Unknown0x140;  // Set to 0 in constructor.
  /*0x144*/ DWORD Unknown0x144;  // Quantity slider.
  /*0x148*/ DWORD Unknown0x148;  // Quantity slider input.
  /*0x14C*/ DWORD Unknown0x14c;  // Quantity accept button.
};

class BazaarSearchWnd : public SidlWnd {
 public:
  void doQuery() { reinterpret_cast<void(__thiscall *)(BazaarSearchWnd *)>(0x0040614c)(this); }

  /*0x0134*/ BYTE Activated;  // Set to 1 when activated and 0 in Deactivate().
  /*0x0135*/ BYTE Unknown0x135[3];
  /*0x0138*/ BYTE Unknown0x138[0x3854];
  /*0x398c*/ DWORD ItemList;               // BZR_ItemList.
  /*0x3990*/ DWORD QueryButton;            // BZR_QueryButton.
  /*0x3994*/ DWORD WelcomeButton;          // BZR_WelcomeButton.
  /*0x3998*/ DWORD UpdatePlayerButton;     // BZR_UpdatePlayerButton.
  /*0x399c*/ DWORD RequestItemButton;      // BZR_RequestItemButton.
  /*0x39a0*/ DWORD Default;                // BZR_Default.
  /*0x39a4*/ DWORD ItemNameLabel;          // BZR_ItemNameLabel.
  /*0x39a8*/ DWORD PlayersLabel;           // BZR_PlayersLabel.
  /*0x39ac*/ DWORD ItemSlotLabel;          // BZR_ItemSlotLabel.
  /*0x39b0*/ DWORD StatSlotLabel;          // BZR_StatSlotLabel.
  /*0x39b4*/ DWORD RaceSlotLabel;          // BZR_RaceSlotLabel.
  /*0x39b8*/ DWORD CLassSlotLabel;         // BZR_CLassSlotLabel.
  /*0x39bc*/ DWORD ItemTypeLabel;          // BZR_ItemTypeLabel.
  /*0x39c0*/ DWORD SearchResultLabel;      // BZR_SearchResultLabel.
  /*0x39c4*/ DWORD MaxPriceLabel;          // BZR_MaxPriceLabel.
  /*0x39c8*/ DWORD MinPriceLabel;          // BZR_MinPriceLabel.
  /*0x39cc*/ ComboWnd *ItemSlotCombobox;   // BZR_ItemSlotCombobox.
  /*0x39d0*/ ComboWnd *StatSlotCombobox;   // BZR_StatSlotCombobox.
  /*0x39d4*/ ComboWnd *RaceSlotCombobox;   // BZR_RaceSlotCombobox.
  /*0x39d8*/ ComboWnd *ClassSlotCombobox;  // BZR_ClassSlotCombobox.
  /*0x39dc*/ ComboWnd *ItemTypeCombobox;   // BZR_ItemTypeCombobox.
  /*0x39e0*/ ComboWnd *PlayersCombobox;    // BZR_PlayersCombobox.
  /*0x39e4*/ EditWnd *ItemNameInput;       // BZR_ItemNameInput.
  /*0x39e8*/ EditWnd *MaxPriceInput;       // BZR_MaxPriceInput.
  /*0x39ec*/ EditWnd *MinPriceInput;       // BZR_MinPriceInput.
};

class CConfirmationDialog : public SidlWnd {
  enum Categories {
    GuildInvite = 0x0,       // Message 0x4118.
    Pvp = 0x1,               // Unsupported message 0x41cf.
    RezzAnswer = 0x2,        // Message 0x419b.
    DestroyItem = 0x5,       // Message 0x412d.
    GuildWar = 0x8,          // Unsupported message 0x4193.
    GuildPeace = 0x9,        // Unsupported message 0x4194.
    Surname = 0xa,           // Message 0x41c4.
    GmKill = 0xd,            // Message 0x406c.
    Sacrifice = 0x14,        // Message 0x41ea.
    DropItem = 0x15,         // Message 0x402c.
    LootItem = 0x16,         // Message 0x40a0.
    GuildDelete = 0x17,      // Message 0x411a.
    Translocate = 0x19,      // Message 0x4206.
    DropMoney = 0x1a,        // Message 0x4007.
    DeleteCharacter = 0x1b,  // Message 0x405a.
    ResetKeys = 0x1c,        // Resets keyboard ini settings.
    OkPrompt = 0x1d,         // Informational dialog with an ok button.
    OldVideoMode = 0x1f,     // Accepts or restores video mode.
    Msg0x408c = 0x20,        // Unsupported message 0x408c.
    DefaultColors = 0x21,    // Restores to default UI colors.
    DeleteSpell = 0x22,      // Message 0x424a.
  };

 public:
  /*0x134*/ BYTE Activated;  // Set to 1 when activated and 0 in Deactivate().
  /*0x135*/ BYTE Unknown0x135[3];
  /*0x138*/ DWORD YesButton;
  /*0x13C*/ DWORD NoButton;
  /*0x140*/ DWORD OkButton;
  /*0x144*/ DWORD TextOutput;
  /*0x148*/ DWORD Category;      // Dialog type (category). 0x14 = sacrifice, 0x1d = Ok, 0x20 = no process.
  /*0x14C*/ DWORD Timeout;       // If non-zero, set to current time + timeout duration in Activate().
  /*0x150*/ DWORD Unknown0x14c;  // Set to 0 in constructor.
  /*0x154*/ BYTE Unknown0x154[0x200 - 0x154];
  /*0x200*/ DWORD FocusWnd;                    //  Set in Activate().
  /*0x204*/ BYTE Unknown0x204[0x390 - 0x204];  // 0x390 bytes allocated
};

struct pInstWindows {
  CContextMenuManager *ContextMenuManager;  // 0x63D5CC
  CChatManager *ChatManager;                // 0x63D5D0
  CConfirmationDialog *ConfirmationDialog;  // 0x63D5D4
  CharSelect *CharacterSelect;              // 0x63D5D8
  SidlWnd *FacePick;                        // 0x63D5DC
  ItemDisplayWnd *ItemWnd;                  // 0x63D5E0
  SidlWnd *Note;                            // 0x63D5E4
  SidlWnd *Help;                            // 0x63D5E8
  SidlWnd *Book;                            // 0x63D5EC
  SidlWnd *PetInfo;                         // 0x63D5F0
  SidlWnd *Train;                           // 0x63D5F4
  SidlWnd *Skills;                          // 0x63D5F8
  SidlWnd *SkillsSelect;                    // 0x63D5FC
  SidlWnd *Friends;                         // 0x63D600
  SidlWnd *AA;                              // 0x63D604
  SidlWnd *Group;                           // 0x63D608
  SidlWnd *Loadskin;                        // 0x63D60C
  SidlWnd *Alarm;                           // 0x63D610
  SidlWnd *MusicPlayer;                     // 0x63D614
  RaidWnd *Raid;                            // 0x63D618
  SidlWnd *RaidOptions;                     // 0x63D61C
  SidlWnd *Breath;                          // 0x63D620
  SidlWnd *Target;                          // 0x63D624
  HotButton *HotButton;                     // 0x63D628
  ColorPickerWnd *ColorPicker;              // 0x63D62C
  SidlWnd *Player;                          // 0x63D630
  OptionsWnd *Options;                      // 0x63D634
  SidlWnd *BuffWindowNORMAL;                // 0x63D638
  SidlWnd *CharacterCreation;               // 0x63D63C
  SidlWnd *CursorAttachment;                // 0x63D640
  SidlWnd *Casting;                         // 0x63D644
  CastSpellWnd *SpellGems;                  // 0x63D648
  SpellBookWnd *SpellBook;                  // 0x63D64C
  SidlWnd *Inventory;                       // 0x63D650
  SidlWnd *Bank;                            // 0x63D654
  QuantityWnd *Quantity;                    // 0x63D658
  LootWnd *Loot;                            // 0x63D65C
  SidlWnd *Actions;                         // 0x63D660
  MerchantWnd *Merchant;                    // 0x63D664
  TradeWnd *Trade;                          // 0x63D668
  SidlWnd *Selector;                        // 0x63D66C
  SidlWnd *Bazaar;                          // 0x63D670
  BazaarSearchWnd *BazaarSearch;            // 0x63D674
  SidlWnd *Give;                            // 0x63D678
  SidlWnd *Tracking;                        // 0x63D67C
  SidlWnd *Inspect;                         // 0x63D680
  SidlWnd *SocialEdit;                      // 0x63D684
  SidlWnd *Feedback;                        // 0x63D688
  SidlWnd *BugReport;                       // 0x63D68C
  SidlWnd *VideoModes;                      // 0x63D690
  SidlWnd *TextEntry;                       // 0x63D694
  SidlWnd *FileSelection;                   // 0x63D698
  SidlWnd *Compass;                         // 0x63D69C
  SidlWnd *PlayerNotes;                     // 0x63D6A0
  SidlWnd *GemsGame;                        // 0x63D6A4
  SidlWnd *TimeLeft;                        // 0x63D6A8
  SidlWnd *PetitionQ;                       // 0x63D6AC
  SidlWnd *Soulmark;                        // 0x63D6B0
  CInvSlotMgr *InvSlotMgr;                  // 0x63D6B4
  ContainerMgr *ContainerMgr;               // 0x63D6B8
};

}  // namespace GameUI
}  // namespace Zeal
#include "std.h"

#include "emul.h"
#include "vars.h"
#include "snapshot.h"
#include "init.h"

#include "util.h"

static void cpuid(unsigned CpuInfo[4], unsigned _eax)
{
#ifdef _MSC_VER
   __cpuid((int *)CpuInfo, int(_eax));
#endif

#ifdef __GNUC__
#ifdef __clang__
   __cpuid((int *)CpuInfo, _eax);
#else
   __cpuid(_eax, CpuInfo[0], CpuInfo[1], CpuInfo[2], CpuInfo[3]);
#endif // __clang__
#endif
}

void fillCpuString(char dst[49])
{
   dst[0] = dst[12] = dst[48] = 0;
   unsigned CpuInfo[4];
   unsigned *d = (unsigned *)dst;

   cpuid(CpuInfo, 0x80000000);
   if(CpuInfo[0] < 0x80000004)
   {
       cpuid(CpuInfo, 0);
       d[0] = CpuInfo[1];
       d[1] = CpuInfo[3];
       d[2] = CpuInfo[2];
       return;
   }

   cpuid(CpuInfo, 0x80000002);
   d[0] = CpuInfo[0];
   d[1] = CpuInfo[1];
   d[2] = CpuInfo[2];
   d[3] = CpuInfo[3];

   cpuid(CpuInfo, 0x80000003);
   d[4] = CpuInfo[0];
   d[5] = CpuInfo[1];
   d[6] = CpuInfo[2];
   d[7] = CpuInfo[3];

   cpuid(CpuInfo, 0x80000004);
   d[ 8] = CpuInfo[0];
   d[ 9] = CpuInfo[1];
   d[10] = CpuInfo[2];
   d[11] = CpuInfo[3];
}

unsigned cpuid(unsigned _eax, int ext)
{
   unsigned CpuInfo[4];

   cpuid(CpuInfo, _eax);

   return ext ? CpuInfo[3] : CpuInfo[0];
}

unsigned __int64 GetCPUFrequency()
{
   LARGE_INTEGER Frequency;
   LARGE_INTEGER Start;
   LARGE_INTEGER Stop;
   unsigned long long c1, c2;

   QueryPerformanceFrequency(&Frequency);
   LONGLONG t = Frequency.QuadPart >> 3;
   c1 = rdtsc();
   QueryPerformanceCounter(&Start);
   do
   {
       QueryPerformanceCounter(&Stop);
   } while((Stop.QuadPart - Start.QuadPart) < t);
   c2 = rdtsc();

   return (c2 - c1) << 3;
}

void trim(char *dst)
{
   size_t i = strlen(dst);
   // trim right spaces
   while (i && isspace(u8(dst[i-1]))) i--;
   dst[i] = 0;
   // trim left spaces
   for (i = 0; isspace(u8(dst[i])); i++);
   strcpy(dst, dst+i);
}


const char nop = 0;
const char * const nil = &nop;

int ishex(char c)
{
   return (isdigit(c) || (tolower(c) >= 'a' && tolower(c) <= 'f'));
}

unsigned char hex(char p)
{
   p = char(tolower(p));
   return u8((p < 'a') ? p-'0' : p-'a'+10);
}

unsigned char hex(const char *p)
{
   return 0x10*hex(p[0]) + hex(p[1]);
}

unsigned process_msgs()
{
   MSG msg;
   unsigned key = 0;

   while(PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
   {
/*
      if (msg.message == WM_NCLBUTTONDOWN)
          continue;
*/
      // mouse messages must be processed further
      if (msg.message == WM_LBUTTONDOWN)
      {
          mousepos = DWORD(msg.lParam);
          key = VK_LMB;
      }
      if (msg.message == WM_RBUTTONDOWN)
      {
          mousepos = DWORD(msg.lParam | 0x80000000);
          key = VK_RMB;
      }
      if (msg.message == WM_MBUTTONDOWN)
      {
          mousepos = DWORD(msg.lParam | 0x80000000);
          key = VK_MMB;
      }

      if (msg.message == WM_MOUSEWHEEL) // [vv] Process mouse whell only in client area
      {
          POINT Pos = { GET_X_LPARAM(msg.lParam), GET_Y_LPARAM(msg.lParam) };

          RECT ClientRect;
          GetClientRect(msg.hwnd, &ClientRect);
          MapWindowPoints(msg.hwnd, HWND_DESKTOP, (LPPOINT)&ClientRect, 2);

          if(PtInRect(&ClientRect, Pos))
              key = GET_WHEEL_DELTA_WPARAM(msg.wParam) < 0 ? VK_MWD : VK_MWU;
      }

      // WM_KEYDOWN and WM_SYSKEYDOWN must not be dispatched,
      // bcoz window will be closed on alt-f4
      if (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN)
      {
         if (conf.atm.xt_kbd)
             input.atm51.setkey(unsigned(msg.lParam >> 16), 1);
         switch (( msg.lParam>>16)&0x1FF)
         {
            case 0x02a: kbdpcEX[0]=(kbdpcEX[0]^0x01)|0x80; break;
            case 0x036: kbdpcEX[1]=(kbdpcEX[1]^0x01)|0x80; break;
            case 0x01d: kbdpcEX[2]=(kbdpcEX[2]^0x01)|0x80; break;
            case 0x11d: kbdpcEX[3]=(kbdpcEX[3]^0x01)|0x80; break;
            case 0x038: kbdpcEX[4]=(kbdpcEX[4]^0x01)|0x80; break;
            case 0x138: kbdpcEX[5]=(kbdpcEX[5]^0x01)|0x80; break;
         } //Dexus
//         printf("%s, WM_KEYDOWN, WM_SYSKEYDOWN\n", __FUNCTION__);
         key = unsigned(msg.wParam);
      }
      else if (msg.message == WM_KEYUP || msg.message == WM_SYSKEYUP)
      {
         if (conf.atm.xt_kbd) input.atm51.setkey(unsigned(msg.lParam >> 16), 0);
         switch (( msg.lParam>>16)&0x1FF)
         {
            case 0x02a: kbdpcEX[0]&=0x01; kbdpcEX[1]&=0x01; break;
            case 0x036: kbdpcEX[0]&=0x01; kbdpcEX[1]&=0x01; break;
            case 0x01d: kbdpcEX[2]&=0x01; break;
            case 0x11d: kbdpcEX[3]&=0x01; break;
            case 0x038: kbdpcEX[4]&=0x01; break;
            case 0x138: kbdpcEX[5]&=0x01; break;
         } //Dexus

//         printf("%s, WM_KEYUP, WM_SYSKEYUP\n", __FUNCTION__);
//         DispatchMessage(&msg); //Dexus
      }

      if(!((WM_KEYFIRST <= msg.message) && (msg.message <= WM_KEYLAST)) ||
          ((WM_MOUSEFIRST <= msg.message) && (msg.message <= WM_MOUSELAST)))
         DispatchMessage(&msg);
   }
   return key;
}

void eat() // eat messages
{
   Sleep(20);
   while (process_msgs()) Sleep(10);
}

bool dispatch_more(action *table)
{
   if (!table)
       return false;

   u8 tmp = kbdpc[0];
   kbdpc[0] = 0x80; // nil button is always pressed

//   __debugbreak();
   while (table->name)
   {
//      printf("%02X|%02X|%02X|%02X\n", table->k1, table->k2, table->k3, table->k4);
      unsigned k[4] = { table->k1, table->k2, table->k3, table->k4 };
      unsigned b[4];

      for(unsigned i =0; i< 4; i++)
      {
          switch(k[i])
          {
          case DIK_MENU:
              b[i] = kbdpc[DIK_LMENU] | kbdpc[DIK_RMENU]; // Alt
          break;
          case DIK_CONTROL:
              b[i] = kbdpc[DIK_LCONTROL] | kbdpc[DIK_RCONTROL];
          break;
          case DIK_SHIFT:
              b[i] = kbdpc[DIK_LSHIFT] | kbdpc[DIK_RSHIFT];
          break;
          default:
              b[i] = kbdpc[k[i]];
          }
      }

      if (b[0] & b[1] & b[2] & b[3] & 0x80)
      {
         table->func();
         kbdpc[0] = tmp;
         return true;
      }
      table++;
   }

   kbdpc[0] = tmp;
   return false;
}

bool dispatch(action *table)
{
   if (*droppedFile)
   {
       trd_toload = DefaultDrive;
       loadsnap(droppedFile);
       *droppedFile = 0;
   }
   if(!input.readdevices())
       return false;

   dispatch_more(table);
   return true;
}

bool wcmatch(char *string, char *wc)
{
   for (;;wc++, string++) {
      if (!*string && !*wc) return 1;
      if (*wc == '?') { if (*string) continue; else return 0; }
      if (*wc == '*') {
         for (wc++; *string; string++)
            if (wcmatch(string, wc)) return 1;
         return 0;
      }
      if (tolower(*string) != tolower(*wc)) return 0;
   }
}

void dump1(BYTE *p, unsigned sz)
{
   while (sz) {
      printf("\t");
      unsigned chunk = (sz > 16)? 16 : sz;
      unsigned i; //Alone Coder 0.36.7
      for (/*unsigned*/ i = 0; i < chunk; i++) printf("%02X ", p[i]);
      for (; i < 16; i++) printf("   ");
      for (i = 0; i < chunk; i++) printf("%c", (p[i] < 0x20)? '.' : p[i]);
      printf("\n");
      sz -= chunk; p += chunk;
   }
   printf("\n");
}

void color(int ink)
{
   SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), WORD(ink));
}

void err_win32(DWORD errcode)
{
   if (errcode == 0xFFFFFFFF) errcode = GetLastError();

   char msg[512];
   if (!FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, errcode,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  msg, sizeof(msg), nullptr)) *msg = 0;

   trim(msg); CharToOem(msg, msg);

   color();
   printf((*msg)? "win32 error message: " : "win32 error code: ");
   color(CONSCLR_ERRCODE);
   if (*msg) printf("%s\n", msg); else printf("%04lX\n", errcode);
   color();
}

void errmsg(const char *err, const char *str)
{
   color();
   printf("error: ");
   color(CONSCLR_ERROR);
   printf(err, str);
   color();
   printf("\n");

#ifdef _DEBUG
   OutputDebugString(err);
   OutputDebugString(str);
   OutputDebugString("\n");
#endif
}

void err_printf(const char *format, ...)
{
   va_list args;
   va_start(args, format);
   color();
   printf("error: ");
   color(CONSCLR_ERROR);
   vprintf(format, args);
   color();
   printf("\n");
   va_end(args);
}

void __declspec(noreturn) errexit(const char *err, const char *str)
{
   errmsg(err, str);
   exit();
}


extern "C" void * _ReturnAddress(void);

#ifndef __INTEL_COMPILER
#pragma intrinsic(_ReturnAddress)
#pragma intrinsic(_byteswap_ulong)
#pragma intrinsic(_byteswap_ushort)
#endif

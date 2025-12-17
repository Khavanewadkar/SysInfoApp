/*
    SysInfo Pro - Dashboard Edition
    Single-screen, high-density hardware status display.
*/

#include <conio.h>
#include <iphlpapi.h>
#include <math.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <winioctl.h>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "winmm.lib")

// --- Console Graphics Helpers ---

void SetColor(int fg, int bg) {
  SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), (bg << 4) | fg);
}

void GotoXY(int x, int y) {
  COORD c = {(SHORT)x, (SHORT)y};
  SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), c);
}

void DrawBox(int x, int y, int w, int h, const char *title) {
  // Colors: Cyan border (3/11), Black bg (0)
  SetColor(11, 0);

  // Corners and sides
  char tl = 201, tr = 187, bl = 200, br = 188;
  char hor = 205, ver = 186;

  // Top
  GotoXY(x, y);
  printf("%c", tl);
  for (int i = 0; i < w - 2; i++)
    printf("%c", hor);
  printf("%c", tr);

  // Sides
  for (int i = 1; i < h - 1; i++) {
    GotoXY(x, y + i);
    printf("%c", ver);
    GotoXY(x + w - 1, y + i);
    printf("%c", ver);
  }

  // Bottom
  GotoXY(x, y + h - 1);
  printf("%c", bl);
  for (int i = 0; i < w - 2; i++)
    printf("%c", hor);
  printf("%c", br);

  // Title
  if (title) {
    GotoXY(x + 2, y);
    SetColor(14, 0); // Yellow title
    printf(" %s ", title);
  }
  SetColor(7, 0); // Reset
}

// --- Status Fetchers with Position Injection ---

void PrintLabelVal(int x, int y, const char *label, const char *val) {
  GotoXY(x, y);
  SetColor(8, 0);
  printf("%-14s: ", label); // Grey label
  SetColor(15, 0);
  printf("%-20s", val); // White value
}

// OS Info
void Dashboard_OS(int x, int y) {
  char buf[256];
  DWORD size = sizeof(buf);
  HKEY hKey;

  if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                    "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0,
                    KEY_READ, &hKey) == ERROR_SUCCESS) {
    if (RegQueryValueExA(hKey, "ProductName", NULL, NULL, (LPBYTE)buf, &size) ==
        ERROR_SUCCESS)
      PrintLabelVal(x, y, "OS", buf);

    size = sizeof(buf);
    if (RegQueryValueExA(hKey, "DisplayVersion", NULL, NULL, (LPBYTE)buf,
                         &size) == ERROR_SUCCESS)
      PrintLabelVal(x, y + 1, "Version", buf);

    RegCloseKey(hKey);
  }

  // Uptime
  ULONGLONG ticks = GetTickCount64();
  long long hours = ticks / (1000 * 60 * 60);
  long long mins = (ticks / (1000 * 60)) % 60;
  sprintf(buf, "%lldh %lldm", hours, mins);
  PrintLabelVal(x, y + 2, "Uptime", buf);
}

// memory
void Dashboard_RAM(int x, int y) {
  MEMORYSTATUSEX statex;
  statex.dwLength = sizeof(statex);
  GlobalMemoryStatusEx(&statex);

  char val[64];
  sprintf(val, "%.1f GB", (float)statex.ullTotalPhys / (1024 * 1024 * 1024));
  PrintLabelVal(x, y, "Total RAM", val);

  sprintf(val, "%.1f GB", (float)statex.ullAvailPhys / (1024 * 1024 * 1024));
  PrintLabelVal(x, y + 1, "Available", val);

  sprintf(val, "%ld%%", statex.dwMemoryLoad);
  PrintLabelVal(x, y + 2, "Load", val);

  // Simple bar
  GotoXY(x, y + 3);
  SetColor(8, 0);
  printf("[");
  int bars = statex.dwMemoryLoad / 10;
  SetColor(statex.dwMemoryLoad > 80 ? 12 : 10, 0);
  for (int i = 0; i < 10; i++)
    printf("%c", i < bars ? 254 : ' ');
  SetColor(8, 0);
  printf("]");
}

// CPU
void Dashboard_CPU(int x, int y) {
  HKEY hKey;
  char buffer[256];
  DWORD size = sizeof(buffer);

  if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                    "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0,
                    KEY_READ, &hKey) == ERROR_SUCCESS) {
    if (RegQueryValueExA(hKey, "ProcessorNameString", NULL, NULL,
                         (LPBYTE)buffer, &size) == ERROR_SUCCESS) {
      // Simplify CPU name
      char *p = strstr(buffer, " @");
      if (p)
        *p = 0;
      PrintLabelVal(x, y, "CPU", buffer);
    }
    RegCloseKey(hKey);
  }

  SYSTEM_INFO sysInfo;
  GetNativeSystemInfo(&sysInfo);
  sprintf(buffer, "%d", sysInfo.dwNumberOfProcessors);
  PrintLabelVal(x, y + 1, "Threads", buffer);
}

void Dashboard_Power(int x, int y) {
  SYSTEM_POWER_STATUS sps;
  if (GetSystemPowerStatus(&sps)) {
    PrintLabelVal(x, y, "Power Src",
                  sps.ACLineStatus == 1 ? "AC (Plugged)" : "Battery");
    char p[32];
    sprintf(p, "%d%%", sps.BatteryLifePercent);
    PrintLabelVal(x, y + 1, "Level", p);
  }
}

void Dashboard_Network(int x, int y) {
  PIP_ADAPTER_INFO pAdapterInfo =
      (PIP_ADAPTER_INFO)malloc(sizeof(IP_ADAPTER_INFO));
  ULONG len = sizeof(IP_ADAPTER_INFO);

  if (GetAdaptersInfo(pAdapterInfo, &len) == ERROR_BUFFER_OVERFLOW) {
    free(pAdapterInfo);
    pAdapterInfo = (PIP_ADAPTER_INFO)malloc(len);
  }

  if (GetAdaptersInfo(pAdapterInfo, &len) == NO_ERROR) {
    PIP_ADAPTER_INFO p = pAdapterInfo;
    int count = 0;
    while (p && count < 2) {
      // Only show actual IPs (skip 0.0.0.0)
      if (strcmp(p->IpAddressList.IpAddress.String, "0.0.0.0") != 0) {
        PrintLabelVal(x, y + (count * 2), "Net IF", "Connected");
        PrintLabelVal(x, y + (count * 2) + 1, "IP Addr",
                      p->IpAddressList.IpAddress.String);
        count++;
      }
      p = p->Next;
    }
    if (count == 0)
      PrintLabelVal(x, y, "Network", "No Connection");
  }
  if (pAdapterInfo)
    free(pAdapterInfo);
}

void Dashboard_Storage(int x, int y) {
  char drvStr[MAX_PATH];
  GetLogicalDriveStringsA(MAX_PATH, drvStr);
  char *s = drvStr;
  int count = 0;
  while (*s && count < 3) {
    ULARGE_INTEGER free, total, totalFree;
    if (GetDiskFreeSpaceExA(s, &free, &total, &totalFree)) {
      float totGB = (float)total.QuadPart / (1024 * 1024 * 1024);
      float freeGB = (float)free.QuadPart / (1024 * 1024 * 1024);
      char label[10];
      sprintf(label, "Drive %s", s);
      char val[64];
      sprintf(val, "%.0fGB Free / %.0fGB", freeGB, totGB);
      PrintLabelVal(x, y + count, label, val);
      count++;
    }
    s += strlen(s) + 1;
  }
}

void Dashboard_IDs(int x, int y) {
  static char hostname[64] = "N/A";
  static char serial[64] = "N/A";
  static char uuid[64] = "N/A";
  static int fetched = 0;

  if (!fetched) {
    // Hostname
    DWORD size = sizeof(hostname);
    GetComputerNameA(hostname, &size);

    // Serial
    FILE *pipe = _popen("wmic bios get serialnumber", "r");
    if (pipe) {
      char line[128];
      int lc = 0;
      while (fgets(line, sizeof(line), pipe)) {
        if (lc == 1 && strlen(line) > 1) {
          char *p = line;
          while (*p && *p <= ' ')
            p++;
          char *end = p + strlen(p) - 1;
          while (end > p && *end <= ' ')
            *end-- = 0;
          strncpy(serial, p, sizeof(serial) - 1);
        }
        if (strlen(line) > 1)
          lc++;
      }
      _pclose(pipe);
    }

    // UUID
    pipe = _popen("wmic csproduct get uuid", "r");
    if (pipe) {
      char line[128];
      int lc = 0;
      while (fgets(line, sizeof(line), pipe)) {
        if (lc == 1 && strlen(line) > 1) {
          char *p = line;
          while (*p && *p <= ' ')
            p++;
          char *end = p + strlen(p) - 1;
          while (end > p && *end <= ' ')
            *end-- = 0;
          strncpy(uuid, p, sizeof(uuid) - 1);
        }
        if (strlen(line) > 1)
          lc++;
      }
      _pclose(pipe);
    }
    fetched = 1;
  }

  PrintLabelVal(x, y, "Hostname", hostname);
  PrintLabelVal(x, y + 1, "Serial Num", serial);
  PrintLabelVal(x, y + 2, "UUID", uuid);
}

int main() {
  // Hide Cursor
  CONSOLE_CURSOR_INFO cursorInfo;
  GetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursorInfo);
  cursorInfo.bVisible = FALSE;
  SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursorInfo);

  system("cls");
  system("title Hardware Dashboard");

  // Get Console Width for dynamic layout
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
  int scrW = csbi.dwSize.X;
  if (scrW < 80)
    scrW = 80; // Minimum safe width

  // Layout Calculations
  // We want 2 columns with a small gap.
  // Margins: Left (2), Center (2), Right (2) -> Total 6 reserved
  int boxW = (scrW - 6) / 2;

  int col1X = 2;
  int col2X = col1X + boxW + 2;

  // Text content offsets (indent inside box)
  int text1X = col1X + 2;
  int text2X = col2X + 2;

  // Draw Layout Frames
  //        X, Y, W, H
  // Draw Layout Frames
  //        X, Y, W, H
  DrawBox(col1X, 1, boxW, 6, "SYSTEM");
  DrawBox(col2X, 1, boxW, 6, "CPU");

  DrawBox(col1X, 8, boxW, 7, "MEMORY");
  DrawBox(col2X, 8, boxW, 7, "NETWORK");

  // Bottom section: Split right column into POWER and IDs
  // We'll increase height to 10 to fit 3 lines of IDs comfortably + borders +
  // gap STORAGE spans 16 to 25 (height 10)

  DrawBox(col1X, 16, boxW, 10, "STORAGE");

  // Power: 2 lines content -> needs 4 lines height (16-19)
  DrawBox(col2X, 16, boxW, 4, "POWER");

  // Gap at 20 is empty space

  // IDs: 3 lines content -> needs 5 lines height (21-25)
  DrawBox(col2X, 21, boxW, 5, "SYSTEM IDS");

  // Bottom controls
  // Previous Y=26, now Y=27
  DrawBox(col1X, 27, (col2X + boxW) - col1X, 3, "CONTROLS");
  PrintLabelVal(text1X, 28, "Status", "Monitoring active. Press 'X' to exit.");

  // Refresh Loop
  while (1) {
    Dashboard_OS(text1X, 3);
    Dashboard_CPU(text2X, 3);

    Dashboard_RAM(text1X, 10);
    Dashboard_Network(text2X, 10);

    // Storage (16..25) content at 17
    Dashboard_Storage(text1X, 17);

    // Power (16..19) content at 17
    Dashboard_Power(text2X, 17);

    // IDs (21..25) content at 22
    Dashboard_IDs(text2X, 22);

    if (_kbhit()) {
      char c = _getch();
      if (c == 'x' || c == 'X')
        break;
    }
    Sleep(1000);
  }

  // Restore Cursor
  cursorInfo.bVisible = TRUE;
  SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursorInfo);
  system("cls");
  return 0;
}

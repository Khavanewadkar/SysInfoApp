# SysInfo Pro - Hardware Dashboard

A modern, terminal-based system information dashboard for Windows that displays real-time hardware and system status in a clean, organized interface.

## Features

- **System Information**: OS version, uptime
- **CPU Details**: Processor name and thread count
- **Memory Status**: Total RAM, available memory, and usage percentage with visual bar
- **Network Information**: Active network interfaces and IP addresses
- **Storage**: Drive information with free/total space
- **Power Status**: AC/Battery status and battery level
- **System IDs**: Hostname, BIOS serial number, and system UUID

## Screenshots

The dashboard features a responsive layout with:
- Dynamic box sizing based on terminal width
- Color-coded sections with cyan borders
- Real-time monitoring with 1-second refresh rate
- Clean separation between different information categories

## Requirements

- Windows 10 or later
- GCC (MinGW) or MSVC compiler
- Terminal with support for extended ASCII characters

## Building

### Using the build script:
```bash
build.bat
```

### Manual compilation with GCC:
```bash
windres sysinfo.rc -O coff -o sysinfo_res.o
gcc sysinfo.c sysinfo_res.o -o sysinfo.exe -lws2_32 -lwinmm -liphlpapi
```

## Usage

Simply run the executable:
```bash
sysinfo.exe
```

Press 'X' to exit the application.

## Tips

- Maximize your terminal window before running for the best experience
- The layout automatically adjusts to your terminal width
- All information updates in real-time every second

## License

This project is open source and available for personal and educational use.

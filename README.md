# Windows Service & Keylogger

This project provides a Windows service and keylogger developed in C. The service manages and controls the keylogger, which monitors and logs keystrokes and active foreground processes.

## Overview

- **Windows Service (`svc.exe`)**
  - Manages service installation, start, stop, and deletion.
  - Launches the keylogger in the background.
  - Ensures single instance execution of the keylogger.

- **Keylogger (`winkey.exe`)**
  - Captures keystrokes and associated foreground process details.
  - Uses low-level hooks to monitor keyboard input.
  - Logs data in a readable format with timestamps.

## Prerequisites

- **Compiler**: CL
- **Build Tool**: NMAKE
- **OS**: Windows 11 but should work on Windows 10

## Quick Start

1. **Compile the Project**: Open a Developer Command Prompt and run:
   ```powershell
   nmake
   ```

3. **Install the Service**:
   ```powershell
   .\svc.exe install
   ```

4. **Start the Service**:
   ```powershell
   .\svc.exe start
   ```

5. **Check Keylogger Status**:
   ```powershell
   tasklist | Select-String "winkey"
   ```

6. **Stop the Service**:
   ```powershell
   .\svc.exe stop
   ```

7. **Remove the Service**:
   ```powershell
   .\svc.exe delete
   ```

## Important Notes

- Disable Windows Defender if necessary for proper functionality.
- Keylogger logs are saved in `keystrokes.log`.

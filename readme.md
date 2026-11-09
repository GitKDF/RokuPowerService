# RokuPowerService (v5.0)

**RokuPowerService** is a lightweight Windows background service that automatically controls the power state of a Roku TV being used as a PC monitor. It monitors Windows display power state notifications (VESA DPMS / Console Display State) and sends corresponding **PowerOn** (via Wake-on-LAN and Roku ECP HTTP) or **PowerOff** commands to the Roku TV.

---

## Features

- **Automated Display Power Control:** Powers on your Roku TV when Windows turns the display on and powers it down when Windows turns the display off.
- **Wake-on-LAN Support:** Enables fast startup even when *Fast TV Start* is disabled or *Turn off after 4 hours* is active on the Roku TV.
- **Configurable Power-Off Delay:** Custom off-timer (0 to 99 minutes, or -1 to disable auto-off) matching Windows display sleep settings.
- **Interactive Setup Assistant (v5.0):** Double-clicking `RokuPowerService.exe` guides you through setup, validates your network configuration, auto-copies the executable to `Program Files`, and registers the service automatically.
- **Standalone Binary:** Zero external runtime dependencies (statically linked C runtime).

---

## Quick Start / Installation

### Method 1: Interactive Setup (Recommended)
1. Download `RokuPowerService.exe` from the latest [Release](https://github.com/your-username/RokuPowerService/releases).
2. Right-click `RokuPowerService.exe` and select **Run as Administrator** (or approve the UAC prompt).
3. Follow the interactive console prompts:
   - Enter your Roku's **IPv4 Address** (e.g., `192.168.1.50`).
   - Enter your network's **Subnet Mask** (e.g., `255.255.255.0`).
   - Enter your Roku's **MAC Address** (e.g., `1c:1e:e3:f8:b7:3d`).
   - Enter an optional **Power-Off Delay** in minutes (default is 14 minutes).

The setup wizard will validate your network input format, copy the executable to `%ProgramFiles%\RokuPowerService`, and install and start the service automatically.

---

### Method 2: Command Line Installation

Open Command Prompt as **Administrator** and run:

```cmd
RokuPowerService.exe -install <RokuIP> <SubnetMask> <MACAddress> [DelayTime]
```

#### Example:
```cmd
RokuPowerService.exe -install 192.168.1.50 255.255.255.0 1c:1e:e3:f8:b7:3d 14
```

#### Parameters:
| Parameter | Description |
| :--- | :--- |
| `<RokuIP>` | IPv4 Address of the Roku TV (Must be fixed in your router's DHCP settings). |
| `<Subnet Mask>` | Local Subnet Mask (typically `255.255.255.0` for 192.168.x.x networks). |
| `<MAC Address>` | Hardware MAC Address of the Roku TV (`XX:XX:XX:XX:XX:XX`). |
| `[Delay Time]` | Optional off delay in minutes. Range `-1` to `99`. Default is `14` min. (`0` = immediate off, `-1` = off disabled). |

---

## Service Operations

From an elevated Command Prompt or PowerShell terminal:

- **Pause Signaling (e.g. for non-PC TV use):**
  ```cmd
  sc pause RokuPowerService
  ```
- **Resume Signaling:**
  ```cmd
  sc continue RokuPowerService
  ```
- **Restart Service:**
  ```cmd
  RokuPowerService.exe -restart
  ```
- **Uninstall Service:**
  ```cmd
  RokuPowerService.exe -remove
  ```

---

## Prerequisites & Router Setup

1. **Static IP Assignment:** Ensure your Roku TV has a static/reserved IP address assigned via your Wi-Fi router's DHCP settings.
2. **Network Control Allowed:** On your Roku TV, navigate to:
   - `Settings` -> `System` -> `Advanced system settings` -> `Control by mobile apps` -> Set to **Default** or **Permissive**.
3. **MAC Address Identification:**
   - Go to `Settings` -> `System` -> `About` on your TV. Note the MAC address for whichever network connection (Wired or Wireless) your TV uses.

---

## Building from Source

### Visual Studio (2017 or newer)
1. Open `RokuPowerService.vcxproj` in Visual Studio.
2. Select **Release** and **x64** (or **Win32**).
3. Build Solution (`Ctrl+Shift+B`).

*Note: The project target platform is set to Windows 10 and standard C++17 (`/std:c++17`).*

---

## License

This project is a derivative work derived from Microsoft sample code (`CppWindowsService`) and is licensed under the **Microsoft Limited Public License (Ms-LPL v1.1)**. See the `LICENSE` file for full terms.
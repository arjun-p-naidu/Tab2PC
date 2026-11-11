# Network Pen Receiver (Windows)

A small Windows program that receives stylus input over the network and turns it into **real Windows pen input**. Pressure, hover and pen-down/up all work, so drawing apps that support Windows Ink treat it like a physical pen.

It's the receiving half of a two-PC setup:

- **Sender:** a Linux (Fedora) machine with a tablet/stylus. It reads the pen with Linux input APIs and sends packets over UDP.
- **Receiver (this repo):** a Windows PC. It listens on UDP port `9000` and injects each packet as pen input using the Windows synthetic pointer API.

```
[Tablet] → Linux PC (sender) ──UDP:9000──▶ Windows PC (this program) → Windows Ink pen input
```

## Requirements

- Windows 10 version 1809 or later, or Windows 11 (needed for `CreateSyntheticPointerDevice`)
- A C++ compiler: MSVC (Visual Studio) or MinGW-w64
- Both PCs on the same network, with UDP port `9000` reachable on the Windows PC

## Building

### Visual Studio / MSVC

From a **Developer Command Prompt**:

```bat
cl /EHsc /O2 Main.cpp
```

The `#pragma comment(lib, ...)` lines link `user32.lib` and `ws2_32.lib` automatically.

You can also create an empty Console App project in Visual Studio, add `Main.cpp`, and build.

### MinGW-w64

```bash
g++ -O2 Main.cpp -o Main.exe -lws2_32 -luser32
```

## Usage

1. **Open the port in Windows Firewall.** Add an inbound rule for **UDP 9000**. Either use *Windows Defender Firewall with Advanced Security* (`wf.msc`), or run this in an admin PowerShell:

   ```powershell
   New-NetFirewallRule -DisplayName "Network Pen Receiver (UDP 9000)" -Direction Inbound -Protocol UDP -LocalPort 9000 -Action Allow
   ```

2. **Run the receiver** on the Windows PC:

   ```bat
   Main.exe
   ```

   You should see:

   ```
   Listening for pen on port 9000
   ```

3. **Start the sender** on the Linux PC and point it at the Windows PC's IP address, port `9000`.

4. Move the pen. The console shows live coordinates, pressure and contact state:

   ```
   X: 960 | Y: 540 | Pres: 512 | Touch: DOWN
   ```

Press `Ctrl+C` to stop.

## Packet format

Each UDP datagram is one 16-byte struct of four 32-bit signed integers in **little-endian** byte order (native on both x86 Linux and Windows):

| Offset | Field      | Type      | Description                                      |
|-------:|------------|-----------|--------------------------------------------------|
| 0      | `x`        | `int32_t` | Raw tablet X (0 – 29376)                          |
| 4      | `y`        | `int32_t` | Raw tablet Y (0 – 16524)                          |
| 8      | `pressure` | `int32_t` | Pen pressure, clamped to 0 – 1024 on the receiver |
| 12     | `touch`    | `int32_t` | `1` = pen touching the surface, `0` = hovering    |

```cpp
struct TabletPacket {
    int32_t x;
    int32_t y;
    int32_t pressure;
    int32_t touch;
};
```

Any sender that produces this layout will work, not just the Linux one.

## How it works

1. Binds a UDP socket to `0.0.0.0:9000`.
2. Creates a virtual pen device with `CreateSyntheticPointerDevice(PT_PEN, ...)`.
3. For each packet:
   - Scales tablet coordinates to screen pixels:
     `screenX = x * 1920 / 29376`, `screenY = y * 1080 / 16524`
   - Builds a `POINTER_TYPE_INFO` pen frame with position and pressure.
   - Sets pointer flags from the touch state: `DOWN` when contact starts, `INCONTACT` while drawing, `UP` when the pen lifts, `INRANGE` while hovering.
   - Injects it with `InjectSyntheticPointerInput`.

## Configuration

These values are currently hard-coded in `Main.cpp`. Change them to match your setup:

| What               | Where in code                     | Default         |
|--------------------|-----------------------------------|-----------------|
| UDP port           | `htons(9000)`                     | `9000`          |
| Screen resolution  | `1920` / `1080` in the main loop  | 1920 × 1080     |
| Tablet range       | `29376` / `16524` in the main loop| 29376 × 16524   |
| Max pressure       | `min(max(pressure, 0), 1024)`     | 1024            |

On multi-monitor setups the coordinates map onto the primary display starting at (0, 0).

## Troubleshooting

- **`Bind failed, port 9000 might be in use.`** Another program is using the port. Find it with:
  ```powershell
  Get-Process -Id (Get-NetUDPEndpoint -LocalPort 9000).OwningProcess
  ```
- **`Failed to create synthetic pen.`** Your Windows version is too old (needs 1809+).
- **Nothing happens when drawing.**
  - Check the firewall rule allows inbound **UDP** 9000.
  - Confirm the sender is targeting the right IP.
  - Watch the console: if the numbers update, packets are arriving and the problem is injection or app-side.
- **Cursor lands in the wrong place.** Update the screen resolution and tablet range values to match your hardware, and check Windows display scaling.
- **No pressure in your drawing app.** Make sure the app has Windows Ink / pointer input enabled. Photoshop and Krita have a setting for this.

## Security note

The receiver accepts packets from **anyone** who can reach UDP port 9000 and turns them into pen input on your desktop. Only run it on a trusted network, and consider limiting the firewall rule to the sender's IP:

```powershell
New-NetFirewallRule -DisplayName "Network Pen Receiver (UDP 9000)" -Direction Inbound -Protocol UDP -LocalPort 9000 -RemoteAddress 192.168.1.50 -Action Allow
```

## License

Add your license of choice here (e.g. MIT).

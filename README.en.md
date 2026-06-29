<div align="center">

<p>
  <a href="README.md"><strong>🇷🇺 Русский</strong></a> &nbsp;|&nbsp;
  <strong>🇬🇧 English</strong>
</p>

# 🧪 The Powder Toy — MCP

### _AI-controlled physics sandbox_

[![Build](https://github.com/Hamazzis/The-Powder-Toy-MCP/actions/workflows/build.yaml/badge.svg)](https://github.com/Hamazzis/The-Powder-Toy-MCP/actions/workflows/build.yaml)
[![Release](https://img.shields.io/github/v/release/Hamazzis/The-Powder-Toy-MCP?label=release)](https://github.com/Hamazzis/The-Powder-Toy-MCP/releases)
[![Platform](https://img.shields.io/badge/platform-Android-brightgreen?logo=android)](https://github.com/Hamazzis/The-Powder-Toy-MCP/releases)
[![License](https://img.shields.io/badge/license-GPLv2-blue)](LICENSE)
[![MCP](https://img.shields.io/badge/MCP-ready-8A2BE2)](https://modelcontextprotocol.io)

---

**The Powder Toy** is a legendary physics sandbox simulating hundreds of materials: sand, water, fire, metals, electronics, explosives, and more.

**This fork** adds a built-in **MCP server** that lets AI directly control the game: build schematics, run simulation, analyze physics, take screenshots, and iterate until the design works.

</div>

---

## ✨ Features

| Capability | How it works |
|---|---|
| 🧱 **Build** | Place any elements: `create`, `create_line`, `create_box`, `create_wall` |
| 🔬 **Analyze** | Read temperature, pressure, particle types anywhere: `read_state` |
| 📸 **See** | Base64 screenshots — AI sees what it built: `screenshot` |
| ⏱️ **Simulate** | Run simulation for N ticks: `run` |
| ↩️ **Undo** | Snapshots — didn't work? Rollback and retry: `snapshot` / `restore` |
| 🌡️ **Control physics** | Change temperature, pressure, gravity: `heat`, `pressure` |
| 📂 **Load** | `.stm` / `.cps` save files: `load_save` |
| 🧪 **Experiment** | Loop: build → check → fix → done |

---

## 🛠️ All Tools

| Tool | Description | Parameters |
|---|---|---|
| `create` | Create element at position | `type`, `x`, `y`, `[width]`, `[height]` |
| `create_line` | Line of elements | `type`, `x1`, `y1`, `x2`, `y2` |
| `create_box` | Fill rectangle | `type`, `x1`, `y1`, `x2`, `y2` |
| `create_wall` | Place walls | `wall_type`, `x`, `y`, `[width]`, `[height]` |
| `run` | Run simulation | `[ticks]` (max: 10000) |
| `read_state` | Read state | `[x]`, `[y]`, `[width]`, `[height]` |
| `screenshot` | Screenshot (base64 PNG) | `[format]` |
| `snapshot` | Save snapshot | — |
| `restore` | Restore snapshot | — |
| `clear` | Clear simulation | — |
| `pause` | Pause | `[paused]` |
| `list_elements` | List all elements | — |
| `get_sim_info` | Simulation info | — |
| `delete` | Delete particles | `x`, `y`, `[width]`, `[height]` |
| `heat` | Set temperature | `x`, `y`, `temperature` |
| `pressure` | Set pressure | `x`, `y`, `pressure` |
| `load_save` | Load save file | `file` |

---

## 🚀 Installation

### 📥 Download APK

[![Download](https://img.shields.io/badge/⬇_Download_APK-aarch64-brightgreen?style=for-the-badge)](https://github.com/Hamazzis/The-Powder-Toy-MCP/releases/latest)
[![All releases](https://img.shields.io/badge/📦_All_releases-gray?style=for-the-badge)](https://github.com/Hamazzis/The-Powder-Toy-MCP/releases)

| Architecture | File |
|---|---|
| **ARM64** (modern phones) | `*-aarch64-android-bionic.apk` |
| ARM (older phones) | `*-arm-android-bionic.apk` |
| x86_64 (emulator) | `*-x86_64-android-bionic.apk` |
| x86 (emulator 32bit) | `*-x86-android-bionic.apk` |

### 🔌 Connection

```bash
# Via Hermes (recommended)
hermes mcp add the-powder-toy \
  --url http://127.0.0.1:8123/mcp \
  --transport http

# Or via curl for testing
curl -X POST http://127.0.0.1:8123/api \
  -H "Content-Type: application/json" \
  -d '{"method":"list_elements"}'
```

---

## 🧪 AI Workflow Example

```
1.  clear()
    → cleared the scene

2.  create_box("METL", 100, 200, 50, 10)
    → built a wall

3.  create("WATR", 125, 100)
    → poured water

4.  run(200)
    → ran simulation

5.  screenshot()
    → AI saw the result

6.  read_state(0, 0, 300, 300)
    → checked temperature / pressure

7.  restore()
    → didn't work, rolled back

8.  create("FIRE", 125, 150)
    → corrected

9.  run(500)
    → rechecked

10. snapshot()
    → works, saved
```

---

## 🏗️ Architecture

```
┌──────────────┐     MCP (HTTP/SSE)     ┌────────────────────────┐
│  AI / Agent   │ ◄─────────────────────►│  The Powder Toy (APK)  │
│  (Hermes,     │    localhost:8123      │                        │
│   Claude)     │                       │  ┌──────────────────┐  │
│              │                       │  │ MCP Server       │  │
│              │                       │  │ • HTTP/SSE       │  │
│              │                       │  │ • JSON-RPC       │  │
│              │                       │  │ • 17 tools       │  │
└──────────────┘                       │  └──────────────────┘  │
                                       │  ┌──────────────────┐  │
                                       │  │ Simulation Core  │  │
                                       │  │ • Particles      │  │
                                       │  │ • Physics        │  │
                                       │  │ • Chemistry      │  │
                                       │  └──────────────────┘  │
                                       └────────────────────────┘
```

Server written in **C++**, built directly into the game engine:
- **`src/mcp/mcp_server.cpp`** — HTTP + JSON-RPC + MCP SSE server
- **Zero dependencies** — POSIX sockets + jsoncpp (already in TPT)
- **Background thread** for TCP, **direct access** to Simulation on main thread

### Endpoints

| Method | Path | Purpose |
|---|---|---|
| `POST` | `/api` | JSON-RPC (direct) |
| `POST` | `/mcp` | MCP JSON-RPC |
| `GET` | `/sse` | MCP SSE transport |
| `POST` | `/message` | MCP message endpoint |
| `GET` | `/screenshot` | Screenshot (JSON) |
| `GET` | `/health` | Health check |

---

## 📜 License

**GNU General Public License v2.**  
Original project: [The Powder Toy](https://github.com/The-Powder-Toy/The-Powder-Toy)

---

<div align="center">

**Made with ❤️ by [Hamazzis](https://github.com/Hamazzis)**  

</div>
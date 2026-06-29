<div align="center">

<p>
  <strong>🇷🇺</strong>
  <a href="#en"><strong>🇬🇧</strong></a>
</p>

# 🧪 The Powder Toy — MCP

### _AI-управляемая физическая песочница_

[![Build](https://github.com/Hamazzis/The-Powder-Toy-MCP/actions/workflows/build.yaml/badge.svg)](https://github.com/Hamazzis/The-Powder-Toy-MCP/actions/workflows/build.yaml)
[![Release](https://img.shields.io/github/v/release/Hamazzis/The-Powder-Toy-MCP?label=release)](https://github.com/Hamazzis/The-Powder-Toy-MCP/releases)
[![Platform](https://img.shields.io/badge/platform-Android-brightgreen?logo=android)](https://github.com/Hamazzis/The-Powder-Toy-MCP/releases)
[![License](https://img.shields.io/badge/license-GPLv2-blue)](LICENSE)
[![MCP](https://img.shields.io/badge/MCP-ready-8A2BE2)](https://modelcontextprotocol.io)

---

**The Powder Toy** — культовая физическая песочница, симулирующая взаимодействие сотен материалов: песок, вода, огонь, металлы, электроника, взрывчатка и многое другое.

**Этот форк** добавляет встроенный **MCP-сервер**, который позволяет AI напрямую управлять игрой: строить схемы, запускать симуляцию, анализировать физику, делать скриншоты и итерировать, пока конструкция не заработает.

</div>

---

## ✨ Что умеет

| Возможность | Как работает |
|---|---|
| 🧱 **Строить** | Размещать любые элементы: `create`, `create_line`, `create_box`, `create_wall` |
| 🔬 **Анализировать** | Читать температуру, давление, типы частиц: `read_state` |
| 📸 **Видеть** | Скриншоты в base64 — AI видит что построил: `screenshot` |
| ⏱️ **Симулировать** | Запускать симуляцию на N тиков: `run` |
| ↩️ **Откатывать** | Снэпшоты — не сработало? Откат: `snapshot` / `restore` |
| 🌡️ **Управлять физикой** | Температура, давление: `heat`, `pressure` |
| 📂 **Загружать** | Сохранения `.stm` / `.cps`: `load_save` |
| 🧪 **Экспериментировать** | Цикл: построил → проверил → исправил |

---

## 🛠️ Инструменты

| Инструмент | Описание |
|---|---|
| `create` | Создать элемент |
| `create_line` | Линия элементов |
| `create_box` | Заливка прямоугольника |
| `create_wall` | Стены |
| `run` | Симуляция N тиков |
| `read_state` | Прочитать состояние |
| `screenshot` | Скриншот (base64 PNG) |
| `snapshot` / `restore` | Снэпшот / откат |
| `clear` | Очистить |
| `pause` | Пауза |
| `list_elements` | Список элементов |
| `get_sim_info` | Инфо о симуляции |
| `delete` | Стереть |
| `heat` / `pressure` | Температура / давление |
| `load_save` | Загрузить файл |

---

## 🚀 Установка

```bash
hermes mcp add the-powder-toy \
  --url http://127.0.0.1:8123/mcp \
  --transport http
```

---

<details>
<summary><h2 style="display:inline">🇬🇧 English version</h2></summary>

<br>

> Click on 🇷🇺 at the top to go back

<div align="center">

# 🧪 The Powder Toy — MCP

### _AI-controlled physics sandbox_

</div>

**The Powder Toy** is a legendary physics sandbox simulating hundreds of materials: sand, water, fire, metals, electronics, explosives, and more.

**This fork** adds a built-in **MCP server** that lets AI directly control the game: build schematics, run simulation, analyze physics, take screenshots, and iterate until the design works.

## ✨ Features

| Capability | How it works |
|---|---|
| 🧱 **Build** | Place any elements |
| 🔬 **Analyze** | Read temperature, pressure, particle types |
| 📸 **See** | Base64 screenshots |
| ⏱️ **Simulate** | Run N simulation ticks |
| ↩️ **Undo** | Snapshots — rollback and retry |
| 🌡️ **Physics** | Control temperature, pressure |
| 📂 **Load** | `.stm` / `.cps` save files |

## 🛠️ Tools

| Tool | Description |
|---|---|
| `create` | Create element |
| `create_line` | Line of elements |
| `create_box` | Fill rectangle |
| `create_wall` | Place walls |
| `run` | Run simulation |
| `read_state` | Read state |
| `screenshot` | Screenshot |
| `snapshot` / `restore` | Undo / redo |
| `clear` | Clear simulation |
| `pause` | Pause |
| `list_elements` | List all elements |
| `get_sim_info` | Simulation info |
| `delete` | Delete particles |
| `heat` / `pressure` | Temperature / pressure |
| `load_save` | Load save file |

## 🚀 Installation

```bash
hermes mcp add the-powder-toy \
  --url http://127.0.0.1:8123/mcp \
  --transport http
```

</details>

---

## 📜 License

**GNU General Public License v2.**  
Оригинальный проект: [The Powder Toy](https://github.com/The-Powder-Toy/The-Powder-Toy)

---

<div align="center">
  <sub>Made with ❤️ by <a href="https://github.com/Hamazzis">Hamazzis</a></sub>
</div>
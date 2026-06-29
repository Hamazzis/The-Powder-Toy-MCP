<div align="center">

<p>
  <strong>🇷🇺 Русский</strong> &nbsp;|&nbsp;
  <a href="README.en.md"><strong>🇬🇧 English</strong></a>
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
| 🔬 **Анализировать** | Читать температуру, давление, типы частиц в любой области: `read_state` |
| 📸 **Видеть** | Скриншоты в base64 — AI видит что построил: `screenshot` |
| ⏱️ **Симулировать** | Запускать симуляцию на N тиков: `run` |
| ↩️ **Откатывать** | Снэпшоты — не сработало? Откат и новая попытка: `snapshot` / `restore` |
| 🌡️ **Управлять физикой** | Менять температуру, давление, гравитацию: `heat`, `pressure` |
| 📂 **Загружать** | Сохранения `.stm` / `.cps`: `load_save` |
| 🧪 **Экспериментировать** | Цикл: построил → проверил → исправил → готово |

---

## 🛠️ Все инструменты

| Инструмент | Описание | Параметры |
|---|---|---|
| `create` | Создать элемент в области | `type`, `x`, `y`, `[width]`, `[height]` |
| `create_line` | Линия элементов | `type`, `x1`, `y1`, `x2`, `y2` |
| `create_box` | Заливка прямоугольника | `type`, `x1`, `y1`, `x2`, `y2` |
| `create_wall` | Стены | `wall_type`, `x`, `y`, `[width]`, `[height]` |
| `run` | Запустить симуляцию | `[ticks]` (макс: 10000) |
| `read_state` | Прочитать состояние | `[x]`, `[y]`, `[width]`, `[height]` |
| `screenshot` | Скриншот (base64 PNG) | `[format]` |
| `snapshot` | Сохранить снэпшот | — |
| `restore` | Откатить к снэпшоту | — |
| `clear` | Очистить симуляцию | — |
| `pause` | Пауза | `[paused]` |
| `list_elements` | Список всех элементов | — |
| `get_sim_info` | Инфо о симуляции | — |
| `delete` | Стереть частицы | `x`, `y`, `[width]`, `[height]` |
| `heat` | Задать температуру | `x`, `y`, `temperature` |
| `pressure` | Задать давление | `x`, `y`, `pressure` |
| `load_save` | Загрузить сохранение | `file` |

---

## 🚀 Установка

### 📥 Скачать APK

[![Download](https://img.shields.io/badge/⬇_Скачать_APK-aarch64-brightgreen?style=for-the-badge)](https://github.com/Hamazzis/The-Powder-Toy-MCP/releases/latest)
[![All releases](https://img.shields.io/badge/📦_Все_релизы-gray?style=for-the-badge)](https://github.com/Hamazzis/The-Powder-Toy-MCP/releases)

| Архитектура | Файл |
|---|---|
| **ARM64** (современные телефоны) | `*-aarch64-android-bionic.apk` |
| ARM (старые телефоны) | `*-arm-android-bionic.apk` |
| x86_64 (эмулятор) | `*-x86_64-android-bionic.apk` |
| x86 (эмулятор 32bit) | `*-x86-android-bionic.apk` |

### 🔌 Подключение

```bash
# Через Hermes (рекомендовано)
hermes mcp add the-powder-toy \
  --url http://127.0.0.1:8123/mcp \
  --transport http

# Или через curl для проверки
curl -X POST http://127.0.0.1:8123/api \
  -H "Content-Type: application/json" \
  -d '{"method":"list_elements"}'
```

---

## 🧪 Пример рабочего цикла AI

```
1.  clear()
    → очистили сцену

2.  create_box("METL", 100, 200, 50, 10)
    → построили стенку

3.  create("WATR", 125, 100)
    → налили воду

4.  run(200)
    → симулировали

5.  screenshot()
    → AI увидел что получилось

6.  read_state(0, 0, 300, 300)
    → проверил температуру / давление

7.  restore()
    → не сработало, откатили

8.  create("FIRE", 125, 150)
    → исправили

9.  run(500)
    → проверили снова

10. snapshot()
    → работает, сохранили
```

---

## 🏗️ Архитектура

```
┌──────────────┐     MCP (HTTP/SSE)     ┌────────────────────────┐
│  AI / Агент   │ ◄─────────────────────►│  The Powder Toy (APK)  │
│  (Hermes,     │    localhost:8123      │                        │
│   Claude)     │                       │  ┌──────────────────┐  │
│              │                       │  │ MCP Server       │  │
│              │                       │  │ • HTTP/SSE       │  │
│              │                       │  │ • JSON-RPC       │  │
│              │                       │  │ • 17 инструментов│  │
└──────────────┘                       │  └──────────────────┘  │
                                       │  ┌──────────────────┐  │
                                       │  │ Simulation Core  │  │
                                       │  │ • Частицы        │  │
                                       │  │ • Физика         │  │
                                       │  │ • Химия          │  │
                                       │  └──────────────────┘  │
                                       └────────────────────────┘
```

Сервер написан на **C++** и встроен прямо в игровой движок:
- **`src/mcp/mcp_server.cpp`** — HTTP + JSON-RPC + MCP SSE сервер
- **Без зависимостей** — только POSIX sockets + jsoncpp (уже есть в TPT)
- **Фоновый поток** для TCP, **прямой доступ** к Simulation на главном потоке

### Эндпоинты

| Метод | Путь | Назначение |
|---|---|---|
| `POST` | `/api` | JSON-RPC (прямой) |
| `POST` | `/mcp` | MCP JSON-RPC |
| `GET` | `/sse` | MCP SSE transport |
| `POST` | `/message` | MCP message endpoint |
| `GET` | `/screenshot` | Скриншот (JSON) |
| `GET` | `/health` | Health check |

---

## 📜 Лицензия

**GNU General Public License v2.**  
Оригинальный проект: [The Powder Toy](https://github.com/The-Powder-Toy/The-Powder-Toy)

---

<div align="center">

**Made with ❤️ by [Hamazzis](https://github.com/Hamazzis)**  

</div>
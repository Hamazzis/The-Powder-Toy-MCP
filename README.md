# 🔬 The Powder Toy — MCP (AI-Controlled)

> **Форк The Powder Toy с встроенным MCP-сервером для управления игрой через ИИ**

[![Build](https://github.com/Hamazzis/The-Powder-Toy-MCP/actions/workflows/build.yaml/badge.svg)](https://github.com/Hamazzis/The-Powder-Toy-MCP/actions/workflows/build.yaml)

The Powder Toy — это физическая песочница, симулирующая взаимодействие сотен материалов: песок, вода, огонь, металлы, взрывчатка, электроника и многое другое.

Этот форк добавляет **встроенный MCP-сервер** (Model Context Protocol), который позволяет AI (через Claude, Hermes, Copilot и любые MCP-совместимые агенты) **напрямую управлять игрой**: строить схемы, запускать симуляцию, анализировать результаты и итерировать, пока конструкция не заработает.

---

## ✨ Возможности

| Что умеет ИИ | Как это работает |
|---|---|
| 🧱 **Строить** | Размещать любые элементы, линии, прямоугольники, стены |
| 🔬 **Анализировать** | Читать температуру, давление, типы частиц в любой области |
| 📸 **Видеть** | Делать скриншоты (base64 PNG) и анализировать визуально |
| ⏱️ **Симулировать** | Запускать симуляцию на N тиков и проверять результат |
| ↩️ **Откатывать** | Снэпшоты → если не сработало, откат и новая попытка |
| 📋 **Загружать** | Сохранения `.stm` / `.cps` |
| 🧪 **Экспериментировать** | Итеративный цикл: построил → проверил → исправил |

---

## 🚀 Как это работает

```
┌──────────────┐    MCP (HTTP/SSE)    ┌────────────────────────┐
│  ИИ / Агент   │ ◄──────────────────► │  The Powder Toy (APK)  │
│  (Hermes,     │    localhost:8123    │                        │
│   Claude,     │                      │  ┌──────────────────┐  │
│   Copilot)    │                      │  │ MCP Server       │  │
│              │                      │  │ • HTTP/SSE       │  │
│              │                      │  │ • JSON-RPC       │  │
│              │                      │  │ • 17 инструментов│  │
└──────────────┘                      │  └──────────────────┘  │
                                      │  ┌──────────────────┐  │
                                      │  │ Simulation Core  │  │
                                      │  │ • Частицы        │  │
                                      │  │ • Физика         │  │
                                      │  │ • Химия          │  │
                                      │  └──────────────────┘  │
                                      └────────────────────────┘
```

Сервер запускается автоматически при старте игры на **порту 8123** (только localhost).
Поддерживает **два протокола**:
1. **MCP HTTP** (рекомендуемый) — `POST /mcp` с JSON-RPC 2.0
2. **Прямой REST API** — `POST /api` для curl / отладки

---

## 🛠️ Инструменты (MCP Tools)

| Инструмент | Описание | Параметры |
|---|---|---|
| `create` | Создать элемент в области | `type`, `x`, `y`, `[width]`, `[height]` |
| `create_line` | Линия элементов | `type`, `x1`, `y1`, `x2`, `y2` |
| `create_box` | Заливка прямоугольника | `type`, `x1`, `y1`, `x2`, `y2` |
| `create_wall` | Стены (wall, fan, vacuum, heat...) | `wall_type`, `x`, `y`, `[width]`, `[height]` |
| `run` | Запустить симуляцию | `[ticks]` (1–10000, по умолч. 100) |
| `read_state` | Прочитать состояние | `[x]`, `[y]`, `[width]`, `[height]`, `[detailed]` |
| `screenshot` | Скриншот (base64 PNG) | `[format]` — "base64" или "file" |
| `snapshot` | Сохранить снэпшот (откат) | — |
| `restore` | Откатить к последнему снэпшоту | — |
| `clear` | Очистить симуляцию | — |
| `pause` | Пауза (get/set) | `[paused]` |
| `list_elements` | Список всех элементов | — |
| `get_sim_info` | Полная информация о симуляции | — |
| `delete` | Стереть частицы | `x`, `y`, `[width]`, `[height]` |
| `heat` | Задать температуру | `x`, `y`, `temperature`, `[width]`, `[height]` |
| `pressure` | Задать давление | `x`, `y`, `pressure`, `[width]`, `[height]` |
| `load_save` | Загрузить файл сохранения | `file` (путь к `.stm`/`.cps`) |

### Ресурсы (MCP Resources)

| URI | Описание |
|---|---|
| `game://state` | Полное состояние симуляции (JSON) |
| `game://screenshot` | Скриншот (base64 PNG blob) |

---

## 📥 Установка

### Предварительно собранные APK

1. Открой [Actions](https://github.com/Hamazzis/The-Powder-Toy-MCP/actions)
2. Выбери последний успешный билд
3. В разделе **Artifacts** скачай APK для своей архитектуры
4. Установи на телефон

### Сборка из исходников

```bash
# Форкнуть и клонировать
git clone https://github.com/Hamazzis/The-Powder-Toy-MCP.git
cd The-Powder-Toy-MCP

# GitHub Actions соберёт автоматически после пуша
git push
```

---

## 🔌 Подключение

### Через Hermes (рекомендовано)

```bash
hermes mcp add the-powder-toy \
  --url http://127.0.0.1:8123/mcp \
  --transport http
```

### Через curl (для отладки)

```bash
# Проверка здоровья
curl http://127.0.0.1:8123/health

# Список доступных элементов
curl -X POST http://127.0.0.1:8123/api \
  -H "Content-Type: application/json" \
  -d '{"method":"list_elements"}'

# Насыпать песка
curl -X POST http://127.0.0.1:8123/api \
  -H "Content-Type: application/json" \
  -d '{"method":"create","params":{"type":"SAND","x":150,"y":100,"width":100,"height":50}}'

# Налить воду
curl -X POST http://127.0.0.1:8123/api \
  -H "Content-Type: application/json" \
  -d '{"method":"create","params":{"type":"WATR","x":150,"y":50,"width":100,"height":10}}'

# Запустить симуляцию
curl -X POST http://127.0.0.1:8123/api \
  -H "Content-Type: application/json" \
  -d '{"method":"run","params":{"ticks":500}}'

# Сделать скриншот
curl -X POST http://127.0.0.1:8123/api \
  -H "Content-Type: application/json" \
  -d '{"method":"screenshot"}' | python3 -c "import sys,json; d=json.load(sys.stdin)['result']['data']; open('/tmp/tpt.png','wb').write(__import__('base64').b64decode(d))"

# Прочитать состояние
curl -X POST http://127.0.0.1:8123/api \
  -H "Content-Type: application/json" \
  -d '{"method":"read_state","params":{"x":0,"y":0,"width":300,"height":300}}'
```

### MCP SSE Transport

```bash
# SSE (потоковое подключение)
curl -N http://127.0.0.1:8123/sse

# В другом терминале — отправка команд через SSE-сессию
curl -X POST http://127.0.0.1:8123/message?sessionId=<ID> \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"get_sim_info","arguments":{}}}'
```

---

## 🏗️ Пример рабочего цикла ИИ

```
1.  clear()                          → очистили сцену
2.  create_box("METL", 100, 200, 50, 10)   → построили стенку
3.  create("WATR", 125, 100)         → налили воду
4.  run(200)                         → симулировали
5.  screenshot()                     → ИИ увидел что получилось
6.  read_state(0, 0, 300, 300)       → проверил температуру / давление
7.  restore()                        → не сработало, откатили
8.  create("FIRE", 125, 150)         → исправили
9.  run(500)                         → проверили снова
10. snapshot()                       → работает, сохранили
```

---

## 🏗️ Архитектура (для разработчиков)

MCP-сервер написан на C++ и встроен прямо в игровой движок:

- **`src/mcp/mcp_server.h`** — публичное API (Start, Stop, SetGamePointers)
- **`src/mcp/mcp_server.cpp`** — HTTP + JSON-RPC + MCP SSE сервер
- **Фоновый поток** — обрабатывает TCP-соединения
- **Прямой доступ к Simulation** — все команды выполняются на главном потоке игры
- **Без зависимостей** — только POSIX sockets + jsoncpp (уже есть в TPT)

### Эндпоинты

| Метод | Путь | Описание |
|---|---|---|
| `POST` | `/api` | JSON-RPC (direct) |
| `POST` | `/mcp` | MCP JSON-RPC |
| `GET` | `/sse` | MCP SSE transport |
| `POST` | `/message` | MCP message endpoint |
| `GET` | `/screenshot` | Скриншот (JSON) |
| `GET` | `/health` | Health check |

---

## 📜 Лицензия

Исходный код распространяется под лицензией **GNU General Public License v2**.

Оригинальный проект: [The Powder Toy](https://github.com/The-Powder-Toy/The-Powder-Toy)

---

## 🤝 Контрибьюция

PR и идеи приветствуются! Что можно улучшить:
- WebSocket транспорт вместо SSE
- Потоковый рендеринг видео
- Ещё больше инструментов для электроники
- GUI панель управления
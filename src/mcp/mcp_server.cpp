#include "mcp_server.h"
#include "gui/game/GameController.h"
#include "gui/game/GameModel.h"
#include "gui/game/GameView.h"
#include "simulation/Simulation.h"
#include "simulation/SimulationData.h"
#include "simulation/ElementClasses.h"
#include "simulation/ElementDefs.h"
#include "simulation/Snapshot.h"
#include "graphics/Graphics.h"
#include "client/GameSave.h"
#include "client/SaveFile.h"
#include "common/platform/Platform.h"
#include "common/String.h"
#include "Format.h"

#include <json/json.h>

#include <algorithm>
#include <atomic>
#include <array>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

// ─── Internal helpers ─────────────────────────────────────────────────────────

namespace mcp
{

static std::string S(const ByteString &bs)
{
    return {bs.c_str(), bs.size()};
}

static std::string S(const String &s)
{
    auto utf8 = s.ToUtf8();
    return {utf8.c_str(), utf8.size()};
}

static ByteString BS(const std::string &s)
{
    return {s.c_str(), s.size()};
}

// ─── Pinned game pointers (set from main thread) ──────────────────────────────

static struct GamePtrs
{
    GameController *gc = nullptr;
    GameModel *gm = nullptr;
    Simulation *sim = nullptr;
    Graphics *gfx = nullptr;
} g_game;

void SetGamePointers(GameController *gc, GameModel *gm, Simulation *sim, Graphics *gfx)
{
    g_game.gc = gc;
    g_game.gm = gm;
    g_game.sim = sim;
    g_game.gfx = gfx;
}

// ─── HTTP types ───────────────────────────────────────────────────────────────

struct HttpRequest
{
    std::string method;
    std::string path;
    std::map<std::string, std::string> query;
    std::map<std::string, std::string> headers;
    std::string body;
};

struct HttpResponse
{
    int statusCode = 200;
    std::string statusText = "OK";
    std::map<std::string, std::string> headers;
    std::string body;

    void SetContentType(const std::string &ct) { headers["Content-Type"] = ct; }

    std::string Serialize() const
    {
        std::string resp;
        resp += "HTTP/1.1 " + std::to_string(statusCode) + " " + statusText + "\r\n";
        for (auto &[k, v] : headers)
            resp += k + ": " + v + "\r\n";
        resp += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        resp += "Connection: close\r\n";
        resp += "\r\n";
        resp += body;
        return resp;
    }
};

struct ClientConn
{
    int fd = -1;
    std::string readBuf;
    bool sse = false;
    std::string sessionId;

    ~ClientConn()
    {
        if (fd >= 0)
            close(fd);
    }
};

// ─── Server state ─────────────────────────────────────────────────────────────

static struct ServerState
{
    int port = 8123;
    int listenFd = -1;
    std::thread listenerThread;
    std::atomic<bool> running{false};

    std::mutex clientsMutex;
    std::vector<std::unique_ptr<ClientConn>> clients;

    std::mutex sseMutex;
    std::map<std::string, ClientConn *> sseClients;

} g_state;

// ─── JSON helper ──────────────────────────────────────────────────────────────

static std::string JsonWrite(const Json::Value &v)
{
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, v);
}

// ─── Tool list ────────────────────────────────────────────────────────────────

static Json::Value BuildToolsList()
{
    auto &sd = SimulationData::CRef();
    Json::Value tools(Json::arrayValue);

    // Helper to add a tool
    auto add = [&](const std::string &name, const std::string &desc, Json::Value schema) {
        Json::Value t;
        t["name"] = name;
        t["description"] = desc;
        t["inputSchema"] = schema;
        tools.append(t);
    };

    { Json::Value s;
        s["type"] = "object";
        s["properties"]["type"]["type"] = "string";
        s["properties"]["type"]["description"] = "Element identifier (e.g. SAND, WATR, FIRE, OIL, METL, GUNPOWDER, NITR, etc.)";
        Json::Value types(Json::arrayValue);
        for (int i = 0; i < PT_NUM; i++)
            if (sd.elements[i].Enabled)
                types.append(S(sd.elements[i].Identifier));
        s["properties"]["type"]["enum"] = types;
        s["properties"]["x"]["type"] = "integer";
        s["properties"]["x"]["description"] = "X coordinate (0-" + std::to_string(XRES - 1) + ")";
        s["properties"]["y"]["type"] = "integer";
        s["properties"]["y"]["description"] = "Y coordinate (0-" + std::to_string(YRES - 1) + ")";
        s["properties"]["width"]["type"] = "integer";
        s["properties"]["width"]["description"] = "Area width (default: 1)";
        s["properties"]["height"]["type"] = "integer";
        s["properties"]["height"]["description"] = "Area height (default: 1)";
        s["required"] = Json::arrayValue;
        s["required"].append("type");
        s["required"].append("x");
        s["required"].append("y");
        add("create", "Create elements at the given position.", s); }

    { Json::Value s;
        s["type"] = "object";
        s["properties"]["type"]["type"] = "string";
        s["properties"]["type"]["description"] = "Element identifier";
        s["properties"]["x1"]["type"] = "integer";
        s["properties"]["y1"]["type"] = "integer";
        s["properties"]["x2"]["type"] = "integer";
        s["properties"]["y2"]["type"] = "integer";
        s["required"] = Json::arrayValue;
        s["required"].append("type");
        s["required"].append("x1");
        s["required"].append("y1");
        s["required"].append("x2");
        s["required"].append("y2");
        add("create_line", "Draw a line of elements between two points.", s); }

    { Json::Value s;
        s["type"] = "object";
        s["properties"]["type"]["type"] = "string";
        s["properties"]["x1"]["type"] = "integer";
        s["properties"]["y1"]["type"] = "integer";
        s["properties"]["x2"]["type"] = "integer";
        s["properties"]["y2"]["type"] = "integer";
        s["required"] = Json::arrayValue;
        s["required"].append("type");
        s["required"].append("x1");
        s["required"].append("y1");
        s["required"].append("x2");
        s["required"].append("y2");
        add("create_box", "Fill a rectangle with elements.", s); }

    { Json::Value s;
        s["type"] = "object";
        s["properties"]["wall_type"]["type"] = "string";
        s["properties"]["wall_type"]["description"] = "Wall type: wall, erase, fan, vacuum, air, heat, cool, gravity, noslow, stream, energy, alloy, explosive, flint, super";
        s["properties"]["x"]["type"] = "integer";
        s["properties"]["y"]["type"] = "integer";
        s["properties"]["width"]["type"] = "integer";
        s["properties"]["height"]["type"] = "integer";
        s["required"] = Json::arrayValue;
        s["required"].append("wall_type");
        s["required"].append("x");
        s["required"].append("y");
        add("create_wall", "Place walls.", s); }

    { Json::Value s;
        s["type"] = "object";
        s["properties"]["ticks"]["type"] = "integer";
        s["properties"]["ticks"]["description"] = "Number of simulation ticks (default: 100, max: 10000)";
        add("run", "Run simulation for N ticks. Then take a screenshot to see what happened.", s); }

    { Json::Value s;
        s["type"] = "object";
        s["properties"]["x"]["type"] = "integer";
        s["properties"]["y"]["type"] = "integer";
        s["properties"]["width"]["type"] = "integer";
        s["properties"]["height"]["type"] = "integer";
        s["properties"]["detailed"]["type"] = "boolean";
        s["properties"]["detailed"]["description"] = "If true, return per-particle details";
        add("read_state", "Read elements, temperature, and pressure at coordinates.", s); }

    { Json::Value s;
        s["type"] = "object";
        s["properties"]["format"]["type"] = "string";
        s["properties"]["format"]["description"] = "'base64' (default) or 'file'";
        add("screenshot", "Take a screenshot. Returns base64 PNG by default.", s); }

    { Json::Value s; s["type"] = "object";
        add("snapshot", "Save current state (for later undo/restore).", s); }

    { Json::Value s; s["type"] = "object";
        add("restore", "Restore last saved snapshot.", s); }

    { Json::Value s; s["type"] = "object";
        add("clear", "Clear the entire simulation.", s); }

    { Json::Value s;
        s["type"] = "object";
        s["properties"]["paused"]["type"] = "boolean";
        add("pause", "Get or set pause state.", s); }

    { Json::Value s; s["type"] = "object";
        add("list_elements", "List all available element types with names and descriptions.", s); }

    { Json::Value s; s["type"] = "object";
        add("get_sim_info", "Full simulation summary: tick count, particles, temperature, pressure, walls.", s); }

    { Json::Value s;
        s["type"] = "object";
        s["properties"]["x"]["type"] = "integer";
        s["properties"]["y"]["type"] = "integer";
        s["properties"]["width"]["type"] = "integer";
        s["properties"]["height"]["type"] = "integer";
        s["required"] = Json::arrayValue;
        s["required"].append("x");
        s["required"].append("y");
        add("delete", "Delete/erase particles in area.", s); }

    { Json::Value s;
        s["type"] = "object";
        s["properties"]["x"]["type"] = "integer";
        s["properties"]["y"]["type"] = "integer";
        s["properties"]["width"]["type"] = "integer";
        s["properties"]["height"]["type"] = "integer";
        s["properties"]["temperature"]["type"] = "number";
        s["properties"]["temperature"]["description"] = "Temperature in Kelvin (273.15 = 0°C, 293.15 = room temp)";
        s["required"] = Json::arrayValue;
        s["required"].append("x");
        s["required"].append("y");
        s["required"].append("temperature");
        add("heat", "Set temperature of particles in an area.", s); }

    { Json::Value s;
        s["type"] = "object";
        s["properties"]["x"]["type"] = "integer";
        s["properties"]["y"]["type"] = "integer";
        s["properties"]["width"]["type"] = "integer";
        s["properties"]["height"]["type"] = "integer";
        s["properties"]["pressure"]["type"] = "number";
        s["properties"]["pressure"]["description"] = "Air pressure in game units (~1.0 = normal)";
        s["required"] = Json::arrayValue;
        s["required"].append("x");
        s["required"].append("y");
        s["required"].append("pressure");
        add("pressure", "Set air pressure in a region.", s); }

    { Json::Value s;
        s["type"] = "object";
        s["properties"]["file"]["type"] = "string";
        s["properties"]["file"]["description"] = "Path to .stm or .cps save file";
        s["required"] = Json::arrayValue;
        s["required"].append("file");
        add("load_save", "Load a save file from the game's data directory.", s); }

    return tools;
}

// ─── Element helpers ──────────────────────────────────────────────────────────

static int GetElementType(const std::string &name)
{
    auto &sd = SimulationData::CRef();
    // Try exact match on Identifier (e.g. "DEFAULT_PT_SAND")
    for (int i = 0; i < PT_NUM; i++)
    {
        if (sd.elements[i].Enabled && S(sd.elements[i].Identifier) == name)
            return i;
    }
    // Try exact match on short Name (e.g. "SAND")
    for (int i = 0; i < PT_NUM; i++)
    {
        if (sd.elements[i].Enabled && S(sd.elements[i].Name) == name)
            return i;
    }
    // Try case-insensitive on all
    std::string upper = name;
    for (auto &c : upper) c = toupper(c);
    for (int i = 0; i < PT_NUM; i++)
    {
        if (sd.elements[i].Enabled)
        {
            auto idStr = S(sd.elements[i].Identifier);
            for (auto &c : idStr) c = toupper(c);
            if (idStr == upper)
                return i;
        }
    }
    // Try case-insensitive on short Name
    for (int i = 0; i < PT_NUM; i++)
    {
        if (sd.elements[i].Enabled)
        {
            auto idStr = S(sd.elements[i].Name);
            for (auto &c : idStr) c = toupper(c);
            if (idStr == upper)
                return i;
        }
    }
    return -1;
}

static std::string ElementName(int type)
{
    auto &sd = SimulationData::CRef();
    if (type >= 0 && type < PT_NUM && sd.elements[type].Enabled)
        return S(sd.elements[type].Identifier);
    return "NONE";
}

// ─── Base64 ───────────────────────────────────────────────────────────────────

static std::string Base64Encode(const char *data, size_t len)
{
    static const char b64[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3)
    {
        int n = std::min<size_t>(3, len - i);
        unsigned int v = 0;
        for (int j = 0; j < n; j++)
            v = (v << 8) | (unsigned char)data[i + j];
        v <<= (3 - n) * 8;
        out += b64[(v >> 18) & 0x3F];
        out += b64[(v >> 12) & 0x3F];
        out += n > 1 ? b64[(v >> 6) & 0x3F] : '=';
        out += n > 2 ? b64[v & 0x3F] : '=';
    }
    return out;
}

// ─── Command execution (must be called from main / sim thread) ─────────────────

static Json::Value ExecCommand(const std::string &method, const Json::Value &params)
{
    Json::Value result;

    auto *sim = g_game.sim;
    auto *gm  = g_game.gm;
    auto *gc  = g_game.gc;
    auto *gfx = g_game.gfx;

    if (!sim || !gm || !gc)
    {
        result["error"] = "Game not fully initialized yet";
        return result;
    }

    if (method == "create")
    {
        auto typeName = params["type"].asString();
        int type = GetElementType(typeName);
        if (type < 0) { result["error"] = "Unknown element: " + typeName; return result; }
        int x = params["x"].asInt();
        int y = params["y"].asInt();
        int w = params.get("width", 1).asInt();
        int h = params.get("height", 1).asInt();
        if (w < 1) w = 1; if (h < 1) h = 1;

        int created = 0;
        for (int j = 0; j < h; j++)
            for (int i = 0; i < w; i++)
                if (sim->create_part(-1, x + i, y + j, type) >= 0)
                    created++;

        result["created"] = created;
        result["type"] = typeName;
        result["x"] = x; result["y"] = y;
        result["w"] = w; result["h"] = h;
    }
    else if (method == "create_line")
    {
        auto typeName = params["type"].asString();
        int type = GetElementType(typeName);
        if (type < 0) { result["error"] = "Unknown element: " + typeName; return result; }
        int x1 = params["x1"].asInt(), y1 = params["y1"].asInt();
        int x2 = params["x2"].asInt(), y2 = params["y2"].asInt();
        sim->CreateLine(x1, y1, x2, y2, type);
        result["ok"] = true;
    }
    else if (method == "create_box")
    {
        auto typeName = params["type"].asString();
        int type = GetElementType(typeName);
        if (type < 0) { result["error"] = "Unknown element: " + typeName; return result; }
        int x1 = params["x1"].asInt(), y1 = params["y1"].asInt();
        int x2 = params["x2"].asInt(), y2 = params["y2"].asInt();
        sim->CreateBox(-1, x1, y1, x2, y2, type, 0);
        result["ok"] = true;
    }
    else if (method == "create_wall")
    {
        auto wallStr = params["wall_type"].asString();
        int wallType = 0;
        if (wallStr == "erase") wallType = 0;
        else if (wallStr == "wall") wallType = 1;
        else if (wallStr == "wall.alt" || wallStr == "wallalt") wallType = 9;
        else if (wallStr == "fan") wallType = 2;
        else if (wallStr == "vacuum" || wallStr == "vac") wallType = 6;
        else if (wallStr == "air") wallType = 3;
        else if (wallStr == "heat") wallType = 7;
        else if (wallStr == "cool") wallType = 8;
        else if (wallStr == "gravity" || wallStr == "grav") wallType = 10;
        else if (wallStr == "noslow") wallType = 5;
        else if (wallStr == "stream") wallType = 12;
        else if (wallStr == "energy") wallType = 11;
        else if (wallStr == "alloy") wallType = 13;
        else if (wallStr == "explosive") wallType = 14;
        else if (wallStr == "flint") wallType = 15;
        else if (wallStr == "super") wallType = 4;
        else { result["error"] = "Unknown wall type: " + wallStr; return result; }

        int x = params["x"].asInt(), y = params["y"].asInt();
        int w = params.get("width", 1).asInt(), h = params.get("height", 1).asInt();
        for (int j = 0; j < h; j++)
            for (int i = 0; i < w; i++)
                sim->CreateWalls(x + i, y + j, 0, 0, wallType, nullptr);
        result["ok"] = true;
    }
    else if (method == "run")
    {
        int ticks = params.get("ticks", 100).asInt();
        if (ticks < 1) ticks = 1;
        if (ticks > 10000) ticks = 10000;

        bool wasPaused = gm->GetPaused();
        gm->SetPaused(false);
        for (int i = 0; i < ticks; i++)
            sim->UpdateParticles(0, sim->NUM_PARTS);
        gm->SetPaused(wasPaused);

        result["ticks_run"] = ticks;
        result["current_tick"] = sim->currentTick;
        result["particle_count"] = sim->parts.active;
    }
    else if (method == "read_state")
    {
        int x = params.get("x", 0).asInt();
        int y = params.get("y", 0).asInt();
        int w = params.get("width", XRES).asInt();
        int h = params.get("height", YRES).asInt();
        x = std::max(0, std::min(x, XRES - 1));
        y = std::max(0, std::min(y, YRES - 1));
        w = std::min(w, XRES - x);
        h = std::min(h, YRES - y);

        std::map<std::string, int> elemCounts;
        float minTemp = 99999, maxTemp = -99999;
        float minPress = 99999, maxPress = -99999;
        int totalParts = 0;

        for (int j = y; j < y + h; j++)
        {
            for (int i = x; i < x + w; i++)
            {
                int pm = sim->pmap[j][i];
                if (pm)
                {
                    int pt = pm & 0xFF;
                    if (pt > 0 && pt < PT_NUM)
                    {
                        auto name = ElementName(pt);
                        elemCounts[name]++;
                        totalParts++;
                        int idx = pm >> 8;
                        minTemp = std::min(minTemp, sim->parts.data[idx].temp);
                        maxTemp = std::max(maxTemp, sim->parts.data[idx].temp);
                    }
                }
                int ph = sim->photons[j][i];
                if (ph)
                {
                    int pt = ph & 0xFF;
                    if (pt > 0 && pt < PT_NUM)
                        elemCounts[ElementName(pt)]++;
                }

                int cx = i / CELL, cy = j / CELL;
                if (cx >= 0 && cx < XCELLS && cy >= 0 && cy < YCELLS)
                {
                    float p = sim->pv[cy][cx];
                    minPress = std::min(minPress, p);
                    maxPress = std::max(maxPress, p);
                }
            }
        }

        result["area"]["x"] = x;
        result["area"]["y"] = y;
        result["area"]["width"] = w;
        result["area"]["height"] = h;
        result["area"]["total_particles"] = totalParts;

        if (minTemp < 99998)
        {
            result["temperature"]["min_kelvin"] = minTemp;
            result["temperature"]["max_kelvin"] = maxTemp;
            result["temperature"]["min_celsius"] = minTemp - 273.15;
            result["temperature"]["max_celsius"] = maxTemp - 273.15;
        }
        result["pressure"]["min"] = minPress;
        result["pressure"]["max"] = maxPress;

        Json::Value counts(Json::objectValue);
        for (auto &[name, c] : elemCounts)
            counts[name] = c;
        result["elements"] = counts;
    }
    else if (method == "screenshot")
    {
        ByteString fname = gc->TakeScreenshot(0, 0);
        if (fname.size())
        {
            std::vector<char> data;
            if (Platform::ReadFile(data, fname))
            {
                result["mime"] = "image/png";
                result["data"] = Base64Encode(data.data(), data.size());
                result["size"] = (int)data.size();
            }
            else
                result["error"] = "Failed to read screenshot file";
            Platform::RemoveFile(fname);
        }
        else
            result["error"] = "Failed to take screenshot";
    }
    else if (method == "snapshot")
    {
        gc->HistorySnapshot();
        result["ok"] = true;
    }
    else if (method == "restore")
    {
        result["ok"] = gc->HistoryRestore();
    }
    else if (method == "clear")
    {
        gc->ClearSim();
        result["ok"] = true;
    }
    else if (method == "pause")
    {
        if (params.isMember("paused"))
            gm->SetPaused(params["paused"].asBool());
        result["paused"] = gm->GetPaused();
    }
    else if (method == "list_elements")
    {
        auto &sd = SimulationData::CRef();
        Json::Value arr(Json::arrayValue);
        for (int i = 0; i < PT_NUM; i++)
        {
            if (sd.elements[i].Enabled)
            {
                Json::Value el;
                el["id"] = i;
                el["identifier"] = S(sd.elements[i].Identifier);
                el["name"] = S(sd.elements[i].Name);
                el["description"] = S(sd.elements[i].Description);
                // Category
                el["category"] = sd.elements[i].MenuSection;
                arr.append(el);
            }
        }
        result["elements"] = arr;
        result["count"] = (int)arr.size();
    }
    else if (method == "get_sim_info")
    {
        result["current_tick"] = sim->currentTick;
        result["particle_count"] = sim->parts.active;
        result["max_particles"] = NPART;
        result["paused"] = gm->GetPaused();
        result["gravity_mode"] = sim->gravityMode;
        result["edge_mode"] = sim->edgeMode;
        result["aheat_enable"] = bool(sim->aheat_enable);

        Json::Value stats(Json::objectValue);
        for (int i = 0; i < PT_NUM; i++)
            if (sim->elementCount[i] > 0)
                stats[ElementName(i)] = sim->elementCount[i];
        result["element_counts"] = stats;

        result["temperature_scale"] = int(gm->GetTemperatureScale());
    }
    else if (method == "delete")
    {
        int x = params["x"].asInt(), y = params["y"].asInt();
        int w = params.get("width", 1).asInt(), h = params.get("height", 1).asInt();
        for (int j = 0; j < h; j++)
            for (int i = 0; i < w; i++)
                sim->delete_part(x + i, y + j);
        result["ok"] = true;
    }
    else if (method == "heat")
    {
        int x = params["x"].asInt(), y = params["y"].asInt();
        int w = params.get("width", 1).asInt(), h = params.get("height", 1).asInt();
        float temp = (float)params["temperature"].asDouble();
        int count = 0;
        for (int j = y; j < y + h && j < YRES; j++)
            for (int i = x; i < x + w && i < XRES; i++)
            {
                int pm = sim->pmap[j][i];
                if (pm) { sim->parts.data[pm >> 8].temp = temp; count++; }
            }
        result["affected"] = count;
        result["temperature"] = temp;
    }
    else if (method == "pressure")
    {
        int x = params["x"].asInt(), y = params["y"].asInt();
        int w = params.get("width", XCELLS).asInt(), h = params.get("height", YCELLS).asInt();
        float press = (float)params["pressure"].asDouble();
        int cx1 = x / CELL, cy1 = y / CELL;
        int cx2 = (x + w) / CELL, cy2 = (y + h) / CELL;
        cx1 = std::max(0, cx1); cy1 = std::max(0, cy1);
        cx2 = std::min(XCELLS - 1, cx2); cy2 = std::min(YCELLS - 1, cy2);
        for (int j = cy1; j <= cy2; j++)
            for (int i = cx1; i <= cx2; i++)
                sim->pv[j][i] = press;
        result["ok"] = true;
    }
    else if (method == "load_online_save")
    {
        int saveId = params["id"].asInt();
        if (saveId <= 0)
        {
            result["error"] = "Invalid save ID";
            return result;
        }
        gc->OpenSavePreview(saveId, 0, savePreviewInstant);
        result["ok"] = true;
        result["id"] = saveId;
    }
    else if (method == "save_scene")
    {
        // Save current scene as base64-encoded .cps
        auto includePressure = params.get("includePressure", false).asBool();
        auto gameSave = sim->Save(includePressure, RectBetween({ 0, 0 }, Vec2{ XRES, YRES }));
        if (!gameSave)
        {
            result["error"] = "Failed to save scene";
            return result;
        }
        auto serialised = gameSave->Serialise();
        if (serialised.empty())
        {
            result["error"] = "Failed to serialise save";
            return result;
        }
        // Base64 encode
        static const char b64[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string encoded;
        encoded.reserve(((serialised.size() + 2) / 3) * 4);
        for (size_t i = 0; i < serialised.size(); i += 3)
        {
            int n = std::min<size_t>(3, serialised.size() - i);
            unsigned int v = 0;
            for (int j = 0; j < n; j++)
                v = (v << 8) | (unsigned char)serialised[i + j];
            v <<= (3 - n) * 8;
            encoded += b64[(v >> 18) & 0x3F];
            encoded += b64[(v >> 12) & 0x3F];
            encoded += n > 1 ? b64[(v >> 6) & 0x3F] : '=';
            encoded += n > 2 ? b64[v & 0x3F] : '=';
        }
        result["data"] = encoded;
        result["size"] = (int)serialised.size();
        result["paused"] = gm->GetPaused();
    }
    {
        // Load save from base64-encoded .cps data
        auto b64data = params["data"].asString();
        if (b64data.empty())
        {
            result["error"] = "No data provided";
            return result;
        }
        // Decode base64
        std::vector<char> gameSaveData;
        // Simple base64 decode
        static const char b64t[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::vector<int> rev(256, -1);
        for (int i = 0; i < 64; i++)
            rev[(unsigned char)b64t[i]] = i;
        rev['='] = 0;
        
        std::string clean;
        for (auto c : b64data)
            if (rev[(unsigned char)c] >= 0 || c == '=')
                clean += c;
        
        for (size_t i = 0; i + 3 < clean.size(); i += 4)
        {
            unsigned v = (rev[(unsigned char)clean[i]] << 18)
                       | (rev[(unsigned char)clean[i+1]] << 12)
                       | (rev[(unsigned char)clean[i+2]] << 6)
                       | rev[(unsigned char)clean[i+3]];
            gameSaveData.push_back((v >> 16) & 0xFF);
            if (clean[i+2] != '=')
                gameSaveData.push_back((v >> 8) & 0xFF);
            if (clean[i+3] != '=')
                gameSaveData.push_back(v & 0xFF);
        }
        
        if (gameSaveData.empty())
        {
            result["error"] = "Failed to decode base64 data";
            return result;
        }
        
        try {
            auto newSave = std::make_unique<GameSave>(gameSaveData);
            auto newFile = std::make_unique<SaveFile>(ByteString("mcp_save"));
            newFile->SetGameSave(std::move(newSave));
            gc->LoadSaveFile(std::move(newFile));
            result["ok"] = true;
        } catch (std::exception &e) {
            result["error"] = ByteString("Failed to load save: ").Append(e.what()).c_str();
        }
    }
        {
            std::vector<char> gameSaveData;
            if (Platform::ReadFile(gameSaveData, filePath))
            {
                auto newFile = std::make_unique<SaveFile>(filePath);
                auto newSave = std::make_unique<GameSave>(gameSaveData);
                newFile->SetGameSave(std::move(newSave));
                gc->LoadSaveFile(std::move(newFile));
                result["ok"] = true;
            }
            else
                result["error"] = "Failed to read file";
        }
        else
            result["error"] = "File not found: " + params["file"].asString();
    }
    else
    {
        result["error"] = "Unknown method: " + method;
    }

    // Clean up temp screenshot files
    if (method != "screenshot")
    {
        // Remove any stray screenshot files from previous calls
        // (screenshot tool cleans up after itself)
    }

    return result;
}

// ─── JSON-RPC handler ─────────────────────────────────────────────────────────

static Json::Value HandleJSONRPC(const Json::Value &req)
{
    Json::Value resp;
    resp["jsonrpc"] = "2.0";
    if (req.isMember("id"))
        resp["id"] = req["id"];

    std::string method = req.get("method", "").asString();

    if (method == "tools/list")
    {
        resp["result"]["tools"] = BuildToolsList();
    }
    else if (method == "tools/call")
    {
        auto name = req["params"]["name"].asString();
        auto args = req["params"]["arguments"];

        // Verify tool exists
        auto tools = BuildToolsList();
        bool found = false;
        for (auto &t : tools)
            if (t["name"].asString() == name) { found = true; break; }

        if (!found)
        {
            resp["error"]["code"] = -32601;
            resp["error"]["message"] = "Tool not found: " + name;
            return resp;
        }

        Json::Value callResult = ExecCommand(name, args);

        if (callResult.isMember("error"))
        {
            resp["error"]["code"] = -32000;
            resp["error"]["message"] = callResult["error"].asString();
        }
        else
        {
            resp["result"]["content"] = Json::Value(Json::arrayValue);
            std::string text = JsonWrite(callResult);
            resp["result"]["content"][0]["type"] = "text";
            resp["result"]["content"][0]["text"] = text;
        }
    }
    else if (method == "resources/list")
    {
        Json::Value resources(Json::arrayValue);
        {
            Json::Value r;
            r["uri"] = "game://state";
            r["name"] = "Current game state";
            r["description"] = "Full simulation state";
            r["mimeType"] = "application/json";
            resources.append(r);
        }
        {
            Json::Value r;
            r["uri"] = "game://screenshot";
            r["name"] = "Current screenshot";
            r["description"] = "Screenshot as base64 PNG";
            r["mimeType"] = "image/png";
            resources.append(r);
        }
        resp["result"]["resources"] = resources;
    }
    else if (method == "resources/read")
    {
        auto uri = req["params"]["uri"].asString();
        if (uri == "game://state")
        {
            Json::Value args;
            Json::Value state = ExecCommand("get_sim_info", args);
            auto text = JsonWrite(state);
            resp["result"]["contents"][0]["uri"] = uri;
            resp["result"]["contents"][0]["mimeType"] = "application/json";
            resp["result"]["contents"][0]["text"] = text;
        }
        else if (uri == "game://screenshot")
        {
            Json::Value args;
            args["format"] = "base64";
            Json::Value ss = ExecCommand("screenshot", args);
            if (ss.isMember("data"))
            {
                resp["result"]["contents"][0]["uri"] = uri;
                resp["result"]["contents"][0]["mimeType"] = "image/png";
                resp["result"]["contents"][0]["blob"] = ss["data"].asString();
            }
            else
            {
                resp["error"]["code"] = -32000;
                resp["error"]["message"] = "Screenshot failed: " + ss.get("error", "unknown").asString();
            }
        }
        else
        {
            resp["error"]["code"] = -32602;
            resp["error"]["message"] = "Resource not found: " + uri;
        }
    }
    else
    {
        // Direct method execution (non-MCP short form)
        Json::Value params = req.get("params", Json::Value(Json::objectValue));
        Json::Value callResult = ExecCommand(method, params);
        if (callResult.isMember("error"))
        {
            resp["error"]["code"] = -32000;
            resp["error"]["message"] = callResult["error"].asString();
        }
        else
        {
            resp["result"] = callResult;
        }
    }

    return resp;
}

// ─── HTTP helpers ─────────────────────────────────────────────────────────────

static bool ParseHttpRequest(const std::string &buf, HttpRequest &req)
{
    auto headerEnd = buf.find("\r\n\r\n");
    if (headerEnd == std::string::npos) return false;

    // Request line
    auto firstLine = buf.substr(0, buf.find("\r\n"));
    auto sp1 = firstLine.find(' ');
    if (sp1 == std::string::npos) return false;
    auto sp2 = firstLine.rfind(' ');
    if (sp2 == sp1) return false;

    req.method = firstLine.substr(0, sp1);
    auto pathPart = firstLine.substr(sp1 + 1, sp2 - sp1 - 1);

    // Path + query
    auto qp = pathPart.find('?');
    if (qp != std::string::npos)
    {
        req.path = pathPart.substr(0, qp);
        auto qs = pathPart.substr(qp + 1);
        size_t p = 0;
        while (p < qs.size())
        {
            auto amp = qs.find('&', p);
            auto eq = qs.find('=', p);
            if (eq != std::string::npos && (amp == std::string::npos || eq < amp))
            {
                auto valEnd = (amp == std::string::npos) ? qs.size() : amp;
                req.query[qs.substr(p, eq - p)] = qs.substr(eq + 1, valEnd - eq - 1);
                p = (amp == std::string::npos) ? qs.size() : amp + 1;
            }
            else
                p = (amp == std::string::npos) ? qs.size() : amp + 1;
        }
    }
    else
        req.path = pathPart;

    // Headers
    auto hdrs = buf.substr(0, headerEnd);
    auto crlf = hdrs.find("\r\n");
    while (crlf != std::string::npos)
    {
        auto next = hdrs.find("\r\n", crlf + 2);
        if (next == std::string::npos || next == crlf + 2) break;
        auto line = hdrs.substr(crlf + 2, next - crlf - 2);
        auto col = line.find(':');
        if (col != std::string::npos)
        {
            auto key = line.substr(0, col);
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            req.headers[key] = line.substr(col + 2);
        }
        crlf = next;
    }

    // Body
    if (headerEnd + 4 < buf.size())
        req.body = buf.substr(headerEnd + 4);

    return true;
}

static void SendResponse(int fd, const HttpResponse &resp)
{
    auto str = resp.Serialize();
    send(fd, str.data(), str.size(), 0);
}

static void HandleClient(ClientConn &conn, const std::string &data)
{
    conn.readBuf += data;

    auto headerEnd = conn.readBuf.find("\r\n\r\n");
    if (headerEnd == std::string::npos) return;

    HttpRequest req;
    if (!ParseHttpRequest(conn.readBuf, req))
    {
        conn.readBuf.clear();
        return;
    }

    // Consume from buffer
    conn.readBuf.erase(0, headerEnd + 4 + req.body.size());

    // CORS preflight
    if (req.method == "OPTIONS")
    {
        HttpResponse resp;
        resp.headers["Access-Control-Allow-Origin"] = "*";
        resp.headers["Access-Control-Allow-Methods"] = "GET, POST, OPTIONS";
        resp.headers["Access-Control-Allow-Headers"] = "*";
        SendResponse(conn.fd, resp);
        return;
    }

    // POST /api — direct JSON-RPC
    if (req.path == "/api" && req.method == "POST")
    {
        Json::Value jReq;
        Json::Reader reader;
        if (!reader.parse(req.body, jReq))
        {
            HttpResponse resp;
            resp.statusCode = 400;
            resp.SetContentType("application/json");
            resp.body = "{\"error\":\"invalid json\"}";
            SendResponse(conn.fd, resp);
            return;
        }

        auto jResp = HandleJSONRPC(jReq);
        HttpResponse resp;
        resp.SetContentType("application/json");
        resp.headers["Access-Control-Allow-Origin"] = "*";
        resp.body = JsonWrite(jResp);
        SendResponse(conn.fd, resp);
        return;
    }

    // POST /mcp — MCP JSON-RPC over plain HTTP
    if ((req.path == "/mcp" || req.path == "/message") && req.method == "POST")
    {
        Json::Value jReq;
        Json::Reader reader;
        if (!reader.parse(req.body, jReq))
        {
            HttpResponse resp;
            resp.statusCode = 400;
            resp.SetContentType("application/json");
            Json::Value err;
            err["jsonrpc"] = "2.0";
            err["error"]["code"] = -32700;
            err["error"]["message"] = "Parse error";
            resp.body = JsonWrite(err);
            SendResponse(conn.fd, resp);
            return;
        }

        auto jResp = HandleJSONRPC(jReq);
        HttpResponse resp;
        resp.SetContentType("application/json");
        resp.headers["Access-Control-Allow-Origin"] = "*";
        resp.body = JsonWrite(jResp);
        SendResponse(conn.fd, resp);
        return;
    }

    // GET /sse — MCP SSE transport
    if (req.path == "/sse" && req.method == "GET")
    {
        conn.sse = true;
        // Generate session ID
        conn.sessionId = std::to_string(time(nullptr)) + "-" + std::to_string(conn.fd);

        // Register SSE client
        {
            std::lock_guard<std::mutex> lock(g_state.sseMutex);
            g_state.sseClients[conn.sessionId] = &conn;
        }

        std::string sseHdrs =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/event-stream\r\n"
            "Cache-Control: no-cache\r\n"
            "Connection: keep-alive\r\n"
            "Access-Control-Allow-Origin: *\r\n\r\n";
        send(conn.fd, sseHdrs.data(), sseHdrs.size(), 0);

        // Send endpoint event
        std::string ep = "event: endpoint\ndata: /message?sessionId=" + conn.sessionId + "\n\n";
        send(conn.fd, ep.data(), ep.size(), 0);

        // Keep alive — poll for disconnect
        struct pollfd pfd;
        pfd.fd = conn.fd;
        pfd.events = POLLIN;
        while (g_state.running)
        {
            int ret = poll(&pfd, 1, 1000);
            if (ret < 0 || (ret > 0 && (pfd.revents & (POLLHUP | POLLERR | POLLNVAL))))
                break;
            // Periodic keepalive
            std::string ka = ": keepalive\n\n";
            send(conn.fd, ka.data(), ka.size(), 0);
        }

        // Unregister
        {
            std::lock_guard<std::mutex> lock(g_state.sseMutex);
            g_state.sseClients.erase(conn.sessionId);
        }
        conn.fd = -1; // Mark for cleanup
        return;
    }

    // GET /screenshot
    if (req.path == "/screenshot" && req.method == "GET")
    {
        Json::Value jr;
        jr["method"] = "screenshot";
        jr["params"]["format"] = "base64";
        auto jResp = HandleJSONRPC(jr);
        HttpResponse resp;
        resp.SetContentType("application/json");
        resp.body = JsonWrite(jResp);
        SendResponse(conn.fd, resp);
        return;
    }

    // GET /health
    if (req.path == "/health" && req.method == "GET")
    {
        HttpResponse resp;
        resp.SetContentType("application/json");
        int parts = g_game.sim ? g_game.sim->parts.active : 0;
        resp.body = "{\"status\":\"ok\",\"game\":\"the-powder-toy\",\"particles\":" + std::to_string(parts) + "}";
        resp.headers["Access-Control-Allow-Origin"] = "*";
        SendResponse(conn.fd, resp);
        return;
    }

    // 404
    {
        HttpResponse resp;
        resp.statusCode = 404;
        resp.statusText = "Not Found";
        resp.SetContentType("text/plain");
        resp.body = "Not Found: " + req.path;
        SendResponse(conn.fd, resp);
    }
}

// ─── Public API ───────────────────────────────────────────────────────────────

void Start(int port)
{
    if (g_state.running) return;

    g_state.port = port;

    // Create socket
    g_state.listenFd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (g_state.listenFd < 0)
    {
        std::fprintf(stderr, "[MCP] socket: %s\n", strerror(errno));
        return;
    }

    int opt = 1;
    setsockopt(g_state.listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(g_state.listenFd, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        std::fprintf(stderr, "[MCP] bind port %d: %s\n", port, strerror(errno));
        close(g_state.listenFd);
        g_state.listenFd = -1;
        return;
    }

    if (listen(g_state.listenFd, 10) < 0)
    {
        std::fprintf(stderr, "[MCP] listen: %s\n", strerror(errno));
        close(g_state.listenFd);
        g_state.listenFd = -1;
        return;
    }

    g_state.running = true;
    std::fprintf(stdout, "[MCP] Server on http://127.0.0.1:%d/\n", port);
    std::fprintf(stdout, "[MCP] MCP endpoint: POST http://127.0.0.1:%d/mcp\n", port);
    std::fprintf(stdout, "[MCP] Direct API: POST http://127.0.0.1:%d/api\n", port);
    std::fflush(stdout);

    g_state.listenerThread = std::thread([]() {
        while (g_state.running)
        {
            // Accept new connections
            sockaddr_in ca;
            socklen_t al = sizeof(ca);
            int fd = accept4(g_state.listenFd, (sockaddr *)&ca, &al, SOCK_NONBLOCK);
            if (fd >= 0)
            {
                auto conn = std::make_unique<ClientConn>();
                conn->fd = fd;
                std::lock_guard<std::mutex> lock(g_state.clientsMutex);
                g_state.clients.push_back(std::move(conn));
            }

            // Poll regular connections
            {
                std::lock_guard<std::mutex> lock(g_state.clientsMutex);
                for (auto &c : g_state.clients)
                {
                    if (c->sse) continue;

                    char buf[4096];
                    int n = read(c->fd, buf, sizeof(buf) - 1);
                    if (n > 0)
                    {
                        buf[n] = 0;
                        HandleClient(*c, std::string(buf, n));
                    }
                    else if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK))
                    {
                        c->fd = -1;
                    }
                }

                // Remove dead
                g_state.clients.erase(
                    std::remove_if(g_state.clients.begin(), g_state.clients.end(),
                        [](auto &c) { return c->fd < 0; }),
                    g_state.clients.end());
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
}

void Stop()
{
    if (!g_state.running) return;
    g_state.running = false;

    if (g_state.listenerThread.joinable())
        g_state.listenerThread.join();

    if (g_state.listenFd >= 0)
    {
        close(g_state.listenFd);
        g_state.listenFd = -1;
    }

    {
        std::lock_guard<std::mutex> lock(g_state.clientsMutex);
        g_state.clients.clear();
    }

    std::fprintf(stdout, "[MCP] Server stopped\n");
}

bool IsRunning()
{
    return g_state.running;
}

} // namespace mcp
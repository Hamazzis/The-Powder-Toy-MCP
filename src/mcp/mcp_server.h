#pragma once

namespace mcp
{

void Start(int port = 8123);
void Stop();
bool IsRunning();

// Call from main thread after game is initialized
class GameController;
class GameModel;
class Simulation;
class Graphics;
void SetGamePointers(GameController *gc, GameModel *gm, Simulation *sim, Graphics *gfx);

} // namespace mcp
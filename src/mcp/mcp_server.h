#pragma once

// Real classes from game — forward declared at global scope
class GameController;
class GameModel;
class Simulation;
class Graphics;

namespace mcp
{

void Start(int port = 8123);
void Stop();
bool IsRunning();

// Call from main thread after game is initialized.
// Accepts pointers to the real game classes.
void SetGamePointers(GameController *gc, GameModel *gm, Simulation *sim, Graphics *gfx);

} // namespace mcp
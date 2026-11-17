#pragma once

#include "Player.h"
#include "World.h"

class Game
{
public:
    void Run();

private:
    Player player_;
    World world_;

    void DrawInterface() const;
};

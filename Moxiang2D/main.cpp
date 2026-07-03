#include "Game.h"

#include "raylib.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#define MOXIANG_WEB 1
#else
#define MOXIANG_WEB 0
#endif

#ifndef MOXIANG_USE_IMGUI
#define MOXIANG_USE_IMGUI 0
#endif

#if MOXIANG_USE_IMGUI
#include "rlImGui.h"
#endif

static Game gGame;

static void UpdateDrawFrame()
{
    float dt = GetFrameTime();

    gGame.Update(dt);

    BeginDrawing();
    ClearBackground(BLACK);

    gGame.Draw();

#if MOXIANG_USE_IMGUI
    rlImGuiBegin();
    gGame.DrawEditorUi();
    rlImGuiEnd();
#endif

    EndDrawing();
}

#if MOXIANG_WEB
static void WebMainLoop()
{
    UpdateDrawFrame();
}
#endif

int main()
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);

#if MOXIANG_WEB
    InitWindow(960, 540, "Moxiang2D Web");
#else
    InitWindow(1280, 720, "Moxiang2D Editor");
#endif

    SetTargetFPS(60);

    gGame.Init();

#if MOXIANG_USE_IMGUI
    rlImGuiSetup(true);
#endif

#if MOXIANG_WEB
    emscripten_set_main_loop(WebMainLoop, 0, 1);
#else
    while (!WindowShouldClose())
    {
        UpdateDrawFrame();
    }

#if MOXIANG_USE_IMGUI
    rlImGuiShutdown();
#endif

    CloseWindow();
#endif

    return 0;
}   
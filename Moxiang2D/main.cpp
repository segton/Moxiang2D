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
    double frameStart = GetTime();

    float dt = GetFrameTime();

    double updateStart = GetTime();
    gGame.Update(dt);
    double updateEnd = GetTime();

    BeginDrawing();
    ClearBackground(BLACK);

    double drawStart = GetTime();

    gGame.Draw();

#if MOXIANG_USE_IMGUI
    rlImGuiBegin();
    gGame.DrawEditorUi();
    rlImGuiEnd();
#endif

    double drawEnd = GetTime();

    double endDrawingStart = GetTime();
    EndDrawing();
    double endDrawingEnd = GetTime();

    float updateMs = static_cast<float>((updateEnd - updateStart) * 1000.0);
    float drawMs = static_cast<float>((drawEnd - drawStart) * 1000.0);
    float endDrawingMs = static_cast<float>((endDrawingEnd - endDrawingStart) * 1000.0);
    float fullFrameMs = static_cast<float>((endDrawingEnd - frameStart) * 1000.0);

    gGame.SubmitFrameTiming(
        fullFrameMs,
        updateMs,
        drawMs,
        endDrawingMs
    );
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
    InitWindow(960, 540, "Moxiang2D Hybrid 3D Web");
#else
    InitWindow(1920, 1080, "Moxiang2D Hybrid 3D Editor");
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

    gGame.Shutdown();
    CloseWindow();
#endif

    return 0;
}
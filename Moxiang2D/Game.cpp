#include "Game.h"

#include "raymath.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

#ifndef MOXIANG_USE_IMGUI
#define MOXIANG_USE_IMGUI 0
#endif

#if MOXIANG_USE_IMGUI
#include "imgui.h"
#endif

static float DistanceSquared(Vector2 a, Vector2 b)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;

    return dx * dx + dy * dy;
}

static Vector2 MoveTowards(Vector2 current, Vector2 target, float maxDistance)
{
    Vector2 toTarget = Vector2Subtract(target, current);
    float distance = Vector2Length(toTarget);

    if (distance <= maxDistance || distance <= 0.001f)
    {
        return target;
    }

    Vector2 direction = Vector2Scale(toTarget, 1.0f / distance);
    return Vector2Add(current, Vector2Scale(direction, maxDistance));
}

static bool PointInsideCircle(Vector2 point, Vector2 center, float radius)
{
    return Vector2Distance(point, center) <= radius;
}

static void DrawTextureInRect(Texture2D texture, Rectangle dest, bool autoFit)
{
    if (texture.id == 0)
    {
        return;
    }

    Rectangle source{
        0.0f,
        0.0f,
        static_cast<float>(texture.width),
        static_cast<float>(texture.height)
    };

    if (autoFit)
    {
        DrawTexturePro(texture, source, dest, { 0.0f, 0.0f }, 0.0f, WHITE);
    }
    else
    {
        Rectangle centered{
            dest.x + dest.width * 0.5f - texture.width * 0.5f,
            dest.y + dest.height * 0.5f - texture.height * 0.5f,
            static_cast<float>(texture.width),
            static_cast<float>(texture.height)
        };

        DrawTexturePro(texture, source, centered, { 0.0f, 0.0f }, 0.0f, WHITE);
    }
}

void Game::Init()
{
    InitTileBrushes();

    camera.target = { 0.0f, 0.0f };
    camera.offset = { 640.0f, 360.0f };
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    if (!LoadLevel(DefaultLevelPath))
    {
        LoadDefaultLevel();
    }

    playerPosition = CellToWorld(MapWidth / 2, MapHeight / 2);
    camera.target = playerPosition;
}

void Game::InitTileBrushes()
{
    tileBrushes.clear();

    tileBrushes.push_back(TileBrush{ "", Texture2D{}, false, true, Color{70, 115, 72, 255} });
    tileBrushes.push_back(TileBrush{ "", Texture2D{}, false, true, Color{116, 91, 55, 255} });
    tileBrushes.push_back(TileBrush{ "", Texture2D{}, false, true, Color{104, 104, 100, 255} });
    tileBrushes.push_back(TileBrush{ "", Texture2D{}, false, true, Color{60, 100, 150, 255} });
}

void Game::LoadDefaultLevel()
{
    tiles.assign(MapWidth * MapHeight, static_cast<int>(TileType::Grass));

    for (int y = 0; y < MapHeight; ++y)
    {
        for (int x = MapWidth / 2 - 3; x <= MapWidth / 2 + 3; ++x)
        {
            tiles[CellIndex(x, y)] = static_cast<int>(TileType::Dirt);
        }
    }

    for (int x = 4; x < 11; ++x)
    {
        tiles[CellIndex(x, 6)] = static_cast<int>(TileType::Stone);
    }

    for (int x = 25; x < 33; ++x)
    {
        tiles[CellIndex(x, 20)] = static_cast<int>(TileType::Water);
    }

    obstacles.clear();

    Obstacle treeA;
    treeA.position = { -420.0f, -260.0f };
    treeA.size = { 90.0f, 90.0f };
    treeA.type = 0;
    treeA.collisionEnabled = true;
    treeA.colliderAuto = true;
    treeA.collisionShape = CollisionShape::Box;
    UpdateObstacleAutoCollider(treeA);
    obstacles.push_back(treeA);

    Obstacle treeB;
    treeB.position = { -280.0f, 160.0f };
    treeB.size = { 76.0f, 76.0f };
    treeB.type = 0;
    treeB.collisionEnabled = true;
    treeB.colliderAuto = true;
    treeB.collisionShape = CollisionShape::Box;
    UpdateObstacleAutoCollider(treeB);
    obstacles.push_back(treeB);

    Obstacle rockA;
    rockA.position = { 360.0f, -110.0f };
    rockA.size = { 60.0f, 60.0f };
    rockA.type = 1;
    rockA.collisionEnabled = true;
    rockA.colliderAuto = true;
    rockA.collisionShape = CollisionShape::Box;
    UpdateObstacleAutoCollider(rockA);
    obstacles.push_back(rockA);

    npcs = {
        {
            {340.0f, -260.0f},
            30.0f,
            "Old Swordsman",
            "Train your footwork before entering the mountain path."
        },
        {
            {-360.0f, 280.0f},
            30.0f,
            "Village Scout",
            "Monsters are gathering ahead. Choose your martial skill wisely."
        }
    };
}

bool Game::SaveLevel(const char* path) const
{
    std::filesystem::create_directories("levels");

    std::ofstream out(path);

    if (!out.is_open())
    {
        return false;
    }

    out << "MOXIANG_LEVEL 1\n";

    out << "TILES " << MapWidth << " " << MapHeight << "\n";

    for (int y = 0; y < MapHeight; ++y)
    {
        for (int x = 0; x < MapWidth; ++x)
        {
            out << tiles[CellIndex(x, y)] << " ";
        }

        out << "\n";
    }

    out << "TILE_BRUSHES " << tileBrushes.size() << "\n";

    for (int i = 0; i < static_cast<int>(tileBrushes.size()); ++i)
    {
        const TileBrush& brush = tileBrushes[i];

        out
            << i << " "
            << std::quoted(brush.imagePath) << " "
            << brush.autoFitToTile << " "
            << static_cast<int>(brush.fallbackColor.r) << " "
            << static_cast<int>(brush.fallbackColor.g) << " "
            << static_cast<int>(brush.fallbackColor.b) << " "
            << static_cast<int>(brush.fallbackColor.a) << "\n";
    }

    out << "OBSTACLES " << obstacles.size() << "\n";

    for (const Obstacle& obstacle : obstacles)
    {
        out
            << obstacle.position.x << " "
            << obstacle.position.y << " "
            << obstacle.size.x << " "
            << obstacle.size.y << " "
            << obstacle.type << " "
            << std::quoted(obstacle.imagePath) << " "
            << obstacle.collisionEnabled << " "
            << obstacle.colliderAuto << " "
            << static_cast<int>(obstacle.collisionShape) << " "
            << obstacle.colliderSize.x << " "
            << obstacle.colliderSize.y << " "
            << obstacle.colliderRadius << "\n";
    }

    out << "NPCS " << npcs.size() << "\n";

    for (const NPC& npc : npcs)
    {
        out
            << npc.position.x << " "
            << npc.position.y << " "
            << npc.radius << " "
            << std::quoted(npc.name) << " "
            << std::quoted(npc.dialogue) << "\n";
    }

    return true;
}

bool Game::LoadLevel(const char* path)
{
    std::ifstream in(path);

    if (!in.is_open())
    {
        return false;
    }

    std::string tag;
    int version = 0;

    in >> tag >> version;

    if (tag != "MOXIANG_LEVEL")
    {
        return false;
    }

    InitTileBrushes();

    while (in >> tag)
    {
        if (tag == "TILES")
        {
            int width = 0;
            int height = 0;

            in >> width >> height;

            tiles.assign(MapWidth * MapHeight, static_cast<int>(TileType::Grass));

            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    int tile = 0;
                    in >> tile;

                    if (x < MapWidth && y < MapHeight)
                    {
                        tiles[CellIndex(x, y)] = tile;
                    }
                }
            }
        }
        else if (tag == "TILE_BRUSHES")
        {
            int count = 0;
            in >> count;

            for (int i = 0; i < count; ++i)
            {
                int index = 0;
                std::string imagePath;
                int autoFit = 1;
                int r = 255;
                int g = 255;
                int b = 255;
                int a = 255;

                in
                    >> index
                    >> std::quoted(imagePath)
                    >> autoFit
                    >> r
                    >> g
                    >> b
                    >> a;

                if (index >= 0 && index < static_cast<int>(tileBrushes.size()))
                {
                    tileBrushes[index].imagePath = imagePath;
                    tileBrushes[index].autoFitToTile = autoFit != 0;
                    tileBrushes[index].fallbackColor = Color{
                        static_cast<unsigned char>(r),
                        static_cast<unsigned char>(g),
                        static_cast<unsigned char>(b),
                        static_cast<unsigned char>(a)
                    };

                    if (!imagePath.empty())
                    {
                        LoadTileBrushTexture(index, imagePath);
                    }
                }
            }
        }
        else if (tag == "OBSTACLES")
        {
            int count = 0;
            in >> count;

            obstacles.clear();

            for (int i = 0; i < count; ++i)
            {
                Obstacle obstacle;
                std::string imagePath;
                int collisionEnabled = 1;
                int colliderAuto = 1;
                int collisionShape = 0;

                in
                    >> obstacle.position.x
                    >> obstacle.position.y
                    >> obstacle.size.x
                    >> obstacle.size.y
                    >> obstacle.type
                    >> std::quoted(imagePath)
                    >> collisionEnabled
                    >> colliderAuto
                    >> collisionShape
                    >> obstacle.colliderSize.x
                    >> obstacle.colliderSize.y
                    >> obstacle.colliderRadius;

                obstacle.imagePath = imagePath;
                obstacle.collisionEnabled = collisionEnabled != 0;
                obstacle.colliderAuto = colliderAuto != 0;
                obstacle.collisionShape = static_cast<CollisionShape>(collisionShape);

                if (!imagePath.empty())
                {
                    LoadObstacleTexture(obstacle, imagePath);
                }

                UpdateObstacleAutoCollider(obstacle);
                obstacles.push_back(obstacle);
            }
        }
        else if (tag == "NPCS")
        {
            int count = 0;
            in >> count;

            npcs.clear();

            for (int i = 0; i < count; ++i)
            {
                NPC npc;

                in
                    >> npc.position.x
                    >> npc.position.y
                    >> npc.radius
                    >> std::quoted(npc.name)
                    >> std::quoted(npc.dialogue);

                npcs.push_back(npc);
            }
        }
    }

    return true;
}

bool Game::LoadTileBrushTexture(int tileIndex, const std::string& path)
{
    if (tileIndex < 0 || tileIndex >= static_cast<int>(tileBrushes.size()))
    {
        return false;
    }

    if (path.empty())
    {
        return false;
    }

    Texture2D texture = LoadTexture(path.c_str());

    if (texture.id == 0)
    {
        return false;
    }

    if (tileBrushes[tileIndex].hasTexture)
    {
        UnloadTexture(tileBrushes[tileIndex].texture);
    }

    tileBrushes[tileIndex].texture = texture;
    tileBrushes[tileIndex].hasTexture = true;
    tileBrushes[tileIndex].imagePath = path;

    return true;
}

bool Game::LoadCurrentObstacleTexture(const std::string& path)
{
    if (path.empty())
    {
        return false;
    }

    Texture2D texture = LoadTexture(path.c_str());

    if (texture.id == 0)
    {
        return false;
    }

    if (currentObstacleHasTexture)
    {
        UnloadTexture(currentObstacleTexture);
    }

    currentObstacleTexture = texture;
    currentObstacleHasTexture = true;
    currentObstacleImagePath = path;

    if (obstacleAutoFitToGrid)
    {
        newObstacleSize = { TileSize, TileSize };
    }
    else
    {
        newObstacleSize = {
            static_cast<float>(texture.width),
            static_cast<float>(texture.height)
        };
    }

    newObstacleColliderSize = newObstacleSize;
    newObstacleColliderRadius = std::max(newObstacleSize.x, newObstacleSize.y) * 0.5f;

    return true;
}

bool Game::LoadObstacleTexture(Obstacle& obstacle, const std::string& path)
{
    if (path.empty())
    {
        return false;
    }

    Texture2D texture = LoadTexture(path.c_str());

    if (texture.id == 0)
    {
        return false;
    }

    obstacle.texture = texture;
    obstacle.hasTexture = true;
    obstacle.imagePath = path;

    return true;
}

void Game::Update(float dt)
{
#if MOXIANG_USE_IMGUI
    if (IsKeyPressed(KEY_B))
    {
        buildMode = !buildMode;
    }

    if (buildMode)
    {
        UpdateEditorCameraControls();
        HandleDroppedFiles();
    }
#endif

    UpdateInput(dt);
    UpdatePlayer(dt);
    UpdateCamera(dt);
}

void Game::Draw()
{
    BeginMode2D(camera);
    DrawGround();
    DrawObstacles();
    DrawNpcs();
    DrawPlayer();
    DrawEditorWorldOverlay();
    EndMode2D();

    DrawUi();
    DrawDialogue();
}

void Game::UpdateEditorCameraControls()
{
#if MOXIANG_USE_IMGUI
    if (ImGui::GetIO().WantCaptureMouse)
    {
        return;
    }
#endif

    Vector2 mousePosition = GetMousePosition();
    float wheel = GetMouseWheelMove();

    if (wheel != 0.0f)
    {
        Vector2 worldBeforeZoom = GetScreenToWorld2D(mousePosition, camera);

        camera.zoom += wheel * 0.10f;
        camera.zoom = Clamp(camera.zoom, 0.25f, 4.0f);

        Vector2 worldAfterZoom = GetScreenToWorld2D(mousePosition, camera);
        Vector2 delta = Vector2Subtract(worldBeforeZoom, worldAfterZoom);

        camera.target = Vector2Add(camera.target, delta);
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) || IsMouseButtonDown(MOUSE_BUTTON_MIDDLE))
    {
        Vector2 mouseDelta = GetMouseDelta();
        mouseDelta = Vector2Scale(mouseDelta, -1.0f / camera.zoom);
        camera.target = Vector2Add(camera.target, mouseDelta);
    }
}

void Game::HandleDroppedFiles()
{
#if MOXIANG_USE_IMGUI
    if (!IsFileDropped())
    {
        return;
    }

    FilePathList droppedFiles = LoadDroppedFiles();

    if (droppedFiles.count > 0)
    {
        std::string path = droppedFiles.paths[0];

        if (tileImageDropHovered)
        {
            if (LoadTileBrushTexture(selectedTile, path))
            {
                std::snprintf(tileImagePathInput, sizeof(tileImagePathInput), "%s", path.c_str());
            }
        }
        else if (obstacleImageDropHovered)
        {
            if (LoadCurrentObstacleTexture(path))
            {
                std::snprintf(obstacleImagePathInput, sizeof(obstacleImagePathInput), "%s", path.c_str());
            }
        }
        else
        {
            // Fallback behaviour if user drops image somewhere else.
            if (editorTool == static_cast<int>(EditorTool::PaintTile))
            {
                if (LoadTileBrushTexture(selectedTile, path))
                {
                    std::snprintf(tileImagePathInput, sizeof(tileImagePathInput), "%s", path.c_str());
                }
            }
            else
            {
                if (LoadCurrentObstacleTexture(path))
                {
                    std::snprintf(obstacleImagePathInput, sizeof(obstacleImagePathInput), "%s", path.c_str());
                }
            }
        }
    }

    UnloadDroppedFiles(droppedFiles);
#endif
}

#if MOXIANG_USE_IMGUI
static bool DrawImageDropZone(const char* label, const char* currentPath)
{
    ImGui::Text("%s", label);

    ImVec2 size = ImVec2(-1.0f, 52.0f);

    ImGui::Button(
        (std::string("Drop image here##") + label).c_str(),
        size
    );

    bool hovered = ImGui::IsItemHovered();

    if (hovered)
    {
        ImGui::SetTooltip("Drop image file here");
    }

    if (currentPath != nullptr && currentPath[0] != '\0')
    {
        ImGui::TextWrapped("Current: %s", currentPath);
    }
    else
    {
        ImGui::TextDisabled("No image selected");
    }

    return hovered;
}
#endif

void Game::UpdateInput(float dt)
{
    Vector2 clickScreenPosition{};
    bool pressed = false;
    bool down = false;
    bool released = false;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        clickScreenPosition = GetMousePosition();
        pressed = true;
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
    {
        clickScreenPosition = GetMousePosition();
        down = true;
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
    {
        clickScreenPosition = GetMousePosition();
        released = true;
    }

    int touchCount = GetTouchPointCount();
    bool touchingNow = touchCount > 0;

    if (touchingNow && !wasTouching)
    {
        clickScreenPosition = GetTouchPosition(0);
        pressed = true;
        down = true;
    }

    wasTouching = touchingNow;

    if (dialogueCooldown > 0.0f)
    {
        dialogueCooldown -= dt;
    }

#if MOXIANG_USE_IMGUI
    if (buildMode)
    {
        HandleEditorWorldInput(clickScreenPosition, pressed, down, released);
        return;
    }

    if (ImGui::GetIO().WantCaptureMouse)
    {
        return;
    }
#endif

    if (!pressed)
    {
        return;
    }

    if (activeDialogueNpc != -1)
    {
        if (dialogueCooldown <= 0.0f)
        {
            activeDialogueNpc = -1;
        }

        return;
    }

    Vector2 worldPosition = GetScreenToWorld2D(clickScreenPosition, camera);
    int clickedNpc = GetClickedNpc(worldPosition);

    if (clickedNpc != -1)
    {
        float distanceToNpc = Vector2Distance(playerPosition, npcs[clickedNpc].position);

        if (distanceToNpc <= interactDistance)
        {
            StartDialogue(clickedNpc);
        }
        else
        {
            SetMoveDestinationNearNpc(clickedNpc);
        }
    }
    else
    {
        SetMoveDestination(worldPosition);
    }
}

void Game::HandleEditorWorldInput(Vector2 screenPosition, bool pressed, bool down, bool released)
{
#if MOXIANG_USE_IMGUI
    if (ImGui::GetIO().WantCaptureMouse)
    {
        return;
    }
#endif

    if (released)
    {
        draggingObstacle = false;
        selectedObstacleIndex = -1;
        return;
    }

    if (!pressed && !down)
    {
        return;
    }

    Vector2 worldPosition = GetScreenToWorld2D(screenPosition, camera);

    if (editorTool == static_cast<int>(EditorTool::PaintTile))
    {
        int cellX = 0;
        int cellY = 0;

        if (WorldToCell(worldPosition, cellX, cellY))
        {
            tiles[CellIndex(cellX, cellY)] = selectedTile;
        }
    }
    else if (editorTool == static_cast<int>(EditorTool::PlaceObstacle))
    {
        if (!pressed)
        {
            return;
        }

        Vector2 placePosition = worldPosition;

        if (obstacleSnapToGrid)
        {
            int cellX = 0;
            int cellY = 0;

            if (WorldToCell(worldPosition, cellX, cellY))
            {
                placePosition = CellToWorld(cellX, cellY);
            }
        }

        Obstacle obstacle;
        obstacle.position = placePosition;
        obstacle.size = obstacleAutoFitToGrid ? Vector2{ TileSize, TileSize } : newObstacleSize;
        obstacle.type = selectedObstacleType;
        obstacle.collisionEnabled = newObstacleCollision;
        obstacle.colliderAuto = newObstacleColliderAuto;
        obstacle.collisionShape = static_cast<CollisionShape>(newObstacleCollisionShape);
        obstacle.colliderSize = newObstacleColliderSize;
        obstacle.colliderRadius = newObstacleColliderRadius;

        if (currentObstacleHasTexture)
        {
            LoadObstacleTexture(obstacle, currentObstacleImagePath);
        }

        UpdateObstacleAutoCollider(obstacle);

        obstacles.push_back(obstacle);

        currentPath.clear();
        hasPath = false;
    }
    else if (editorTool == static_cast<int>(EditorTool::EraseObstacle))
    {
        if (!pressed)
        {
            return;
        }

        int obstacleIndex = GetObstacleAt(worldPosition);

        if (obstacleIndex != -1)
        {
            if (obstacles[obstacleIndex].hasTexture)
            {
                UnloadTexture(obstacles[obstacleIndex].texture);
            }

            obstacles.erase(obstacles.begin() + obstacleIndex);

            currentPath.clear();
            hasPath = false;
        }
    }
    else if (editorTool == static_cast<int>(EditorTool::MoveObstacle))
    {
        if (pressed)
        {
            selectedObstacleIndex = GetObstacleAt(worldPosition);

            if (selectedObstacleIndex != -1)
            {
                draggingObstacle = true;
                dragOffset = Vector2Subtract(obstacles[selectedObstacleIndex].position, worldPosition);
            }
        }

        if (down && draggingObstacle && selectedObstacleIndex >= 0 && selectedObstacleIndex < static_cast<int>(obstacles.size()))
        {
            Vector2 newPosition = Vector2Add(worldPosition, dragOffset);

            if (obstacleSnapToGrid)
            {
                int cellX = 0;
                int cellY = 0;

                if (WorldToCell(newPosition, cellX, cellY))
                {
                    newPosition = CellToWorld(cellX, cellY);
                }
            }

            obstacles[selectedObstacleIndex].position = newPosition;

            currentPath.clear();
            hasPath = false;
        }
    }
}

void Game::UpdatePlayer(float dt)
{
    if (activeDialogueNpc != -1)
    {
        return;
    }

    if (pendingNpc != -1)
    {
        float distanceToNpc = Vector2Distance(playerPosition, npcs[pendingNpc].position);

        if (distanceToNpc <= interactDistance)
        {
            StartDialogue(pendingNpc);
            return;
        }
    }

    if (!hasPath || pathIndex >= static_cast<int>(currentPath.size()))
    {
        hasPath = false;
        return;
    }

    Vector2 target = currentPath[pathIndex];
    float step = playerSpeed * dt;

    playerPosition = MoveTowards(playerPosition, target, step);

    if (Vector2Distance(playerPosition, target) < 2.0f)
    {
        pathIndex++;

        if (pathIndex >= static_cast<int>(currentPath.size()))
        {
            hasPath = false;
        }
    }
}

void Game::UpdateCamera(float dt)
{
    camera.offset = {
        static_cast<float>(GetScreenWidth()) * 0.5f,
        static_cast<float>(GetScreenHeight()) * 0.5f
    };

#if MOXIANG_USE_IMGUI
    if (buildMode)
    {
        return;
    }
#endif

    camera.target = Vector2Lerp(camera.target, playerPosition, 8.0f * dt);
}

int Game::GetClickedNpc(Vector2 worldPosition) const
{
    for (int i = 0; i < static_cast<int>(npcs.size()); ++i)
    {
        const NPC& npc = npcs[i];

        if (PointInsideCircle(worldPosition, npc.position, npc.radius + 16.0f))
        {
            return i;
        }
    }

    return -1;
}

int Game::GetObstacleAt(Vector2 worldPosition) const
{
    for (int i = static_cast<int>(obstacles.size()) - 1; i >= 0; --i)
    {
        const Obstacle& obstacle = obstacles[i];

        if (CheckCollisionPointRec(worldPosition, GetObstacleVisualRect(obstacle)))
        {
            return i;
        }
    }

    return -1;
}

void Game::StartDialogue(int npcIndex)
{
    activeDialogueNpc = npcIndex;
    pendingNpc = -1;
    currentPath.clear();
    hasPath = false;
    dialogueCooldown = 0.20f;
}

int Game::CellIndex(int x, int y) const
{
    return y * MapWidth + x;
}

bool Game::IsCellInside(int x, int y) const
{
    return x >= 0 && x < MapWidth && y >= 0 && y < MapHeight;
}

bool Game::WorldToCell(Vector2 worldPosition, int& cellX, int& cellY) const
{
    float originX = -static_cast<float>(MapWidth) * TileSize * 0.5f;
    float originY = -static_cast<float>(MapHeight) * TileSize * 0.5f;

    cellX = static_cast<int>(std::floor((worldPosition.x - originX) / TileSize));
    cellY = static_cast<int>(std::floor((worldPosition.y - originY) / TileSize));

    return IsCellInside(cellX, cellY);
}

Vector2 Game::CellToWorld(int cellX, int cellY) const
{
    float originX = -static_cast<float>(MapWidth) * TileSize * 0.5f;
    float originY = -static_cast<float>(MapHeight) * TileSize * 0.5f;

    return {
        originX + static_cast<float>(cellX) * TileSize + TileSize * 0.5f,
        originY + static_cast<float>(cellY) * TileSize + TileSize * 0.5f
    };
}

bool Game::IsTileWalkable(int tileType) const
{
    TileType type = static_cast<TileType>(tileType);

    return type != TileType::Water;
}

bool Game::IsCellBlocked(int cellX, int cellY) const
{
    if (!IsCellInside(cellX, cellY))
    {
        return true;
    }

    int tileType = tiles[CellIndex(cellX, cellY)];

    if (!IsTileWalkable(tileType))
    {
        return true;
    }

    Vector2 cellCenter = CellToWorld(cellX, cellY);

    for (const Obstacle& obstacle : obstacles)
    {
        if (!obstacle.collisionEnabled)
        {
            continue;
        }

        if (obstacle.collisionShape == CollisionShape::Box)
        {
            Rectangle collider = GetObstacleCollisionRect(obstacle);

            collider.x -= playerRadius;
            collider.y -= playerRadius;
            collider.width += playerRadius * 2.0f;
            collider.height += playerRadius * 2.0f;

            if (CheckCollisionPointRec(cellCenter, collider))
            {
                return true;
            }
        }
        else
        {
            float radius = obstacle.colliderRadius + playerRadius + 4.0f;

            if (Vector2Distance(cellCenter, obstacle.position) <= radius)
            {
                return true;
            }
        }
    }

    return false;
}

bool Game::FindNearestWalkableCell(Vector2 worldPosition, int& outX, int& outY) const
{
    int baseX = 0;
    int baseY = 0;

    if (!WorldToCell(worldPosition, baseX, baseY))
    {
        baseX = std::max(0, std::min(MapWidth - 1, baseX));
        baseY = std::max(0, std::min(MapHeight - 1, baseY));
    }

    if (!IsCellBlocked(baseX, baseY))
    {
        outX = baseX;
        outY = baseY;
        return true;
    }

    for (int radius = 1; radius < 12; ++radius)
    {
        for (int y = baseY - radius; y <= baseY + radius; ++y)
        {
            for (int x = baseX - radius; x <= baseX + radius; ++x)
            {
                if (!IsCellInside(x, y))
                {
                    continue;
                }

                if (IsCellBlocked(x, y))
                {
                    continue;
                }

                outX = x;
                outY = y;
                return true;
            }
        }
    }

    return false;
}

bool Game::FindPath(Vector2 startWorld, Vector2 targetWorld, std::vector<Vector2>& outPath) const
{
    outPath.clear();

    int startX = 0;
    int startY = 0;
    int targetX = 0;
    int targetY = 0;

    if (!FindNearestWalkableCell(startWorld, startX, startY))
    {
        return false;
    }

    if (!FindNearestWalkableCell(targetWorld, targetX, targetY))
    {
        return false;
    }

    struct Node
    {
        float g = std::numeric_limits<float>::infinity();
        float f = std::numeric_limits<float>::infinity();
        int parent = -1;
        bool opened = false;
        bool closed = false;
    };

    std::vector<Node> nodes(MapWidth * MapHeight);
    std::vector<int> openList;

    int startIndex = CellIndex(startX, startY);
    int targetIndex = CellIndex(targetX, targetY);

    nodes[startIndex].g = 0.0f;
    nodes[startIndex].f = 0.0f;
    nodes[startIndex].opened = true;
    openList.push_back(startIndex);

    const int directions[8][2] = {
        { 1,  0},
        {-1,  0},
        { 0,  1},
        { 0, -1},
        { 1,  1},
        {-1,  1},
        { 1, -1},
        {-1, -1}
    };

    while (!openList.empty())
    {
        int bestOpenPosition = 0;
        int currentIndex = openList[0];

        for (int i = 1; i < static_cast<int>(openList.size()); ++i)
        {
            int candidateIndex = openList[i];

            if (nodes[candidateIndex].f < nodes[currentIndex].f)
            {
                currentIndex = candidateIndex;
                bestOpenPosition = i;
            }
        }

        openList.erase(openList.begin() + bestOpenPosition);
        nodes[currentIndex].closed = true;

        if (currentIndex == targetIndex)
        {
            break;
        }

        int currentX = currentIndex % MapWidth;
        int currentY = currentIndex / MapWidth;

        for (const auto& direction : directions)
        {
            int nextX = currentX + direction[0];
            int nextY = currentY + direction[1];

            if (!IsCellInside(nextX, nextY))
            {
                continue;
            }

            if (IsCellBlocked(nextX, nextY))
            {
                continue;
            }

            bool diagonal = direction[0] != 0 && direction[1] != 0;

            if (diagonal)
            {
                if (IsCellBlocked(currentX + direction[0], currentY) ||
                    IsCellBlocked(currentX, currentY + direction[1]))
                {
                    continue;
                }
            }

            int nextIndex = CellIndex(nextX, nextY);

            if (nodes[nextIndex].closed)
            {
                continue;
            }

            float stepCost = diagonal ? 1.4142f : 1.0f;
            float newG = nodes[currentIndex].g + stepCost;

            if (!nodes[nextIndex].opened || newG < nodes[nextIndex].g)
            {
                float heuristic =
                    static_cast<float>(std::abs(targetX - nextX) + std::abs(targetY - nextY));

                nodes[nextIndex].g = newG;
                nodes[nextIndex].f = newG + heuristic;
                nodes[nextIndex].parent = currentIndex;

                if (!nodes[nextIndex].opened)
                {
                    nodes[nextIndex].opened = true;
                    openList.push_back(nextIndex);
                }
            }
        }
    }

    if (!nodes[targetIndex].closed)
    {
        return false;
    }

    std::vector<Vector2> reversedPath;

    int current = targetIndex;

    while (current != -1 && current != startIndex)
    {
        int x = current % MapWidth;
        int y = current / MapWidth;

        reversedPath.push_back(CellToWorld(x, y));
        current = nodes[current].parent;
    }

    std::reverse(reversedPath.begin(), reversedPath.end());

    outPath = reversedPath;
    return !outPath.empty();
}

void Game::SetMoveDestination(Vector2 worldTarget)
{
    pendingNpc = -1;

    if (FindPath(playerPosition, worldTarget, currentPath))
    {
        pathIndex = 0;
        hasPath = true;
    }
    else
    {
        currentPath.clear();
        hasPath = false;
    }
}

void Game::SetMoveDestinationNearNpc(int npcIndex)
{
    pendingNpc = npcIndex;

    Vector2 npcPosition = npcs[npcIndex].position;

    if (FindPath(playerPosition, npcPosition, currentPath))
    {
        pathIndex = 0;
        hasPath = true;
    }
    else
    {
        currentPath.clear();
        hasPath = false;
    }
}

Rectangle Game::GetObstacleVisualRect(const Obstacle& obstacle) const
{
    return Rectangle{
        obstacle.position.x - obstacle.size.x * 0.5f,
        obstacle.position.y - obstacle.size.y * 0.5f,
        obstacle.size.x,
        obstacle.size.y
    };
}

Rectangle Game::GetObstacleCollisionRect(const Obstacle& obstacle) const
{
    return Rectangle{
        obstacle.position.x - obstacle.colliderSize.x * 0.5f,
        obstacle.position.y - obstacle.colliderSize.y * 0.5f,
        obstacle.colliderSize.x,
        obstacle.colliderSize.y
    };
}

void Game::UpdateObstacleAutoCollider(Obstacle& obstacle) const
{
    if (!obstacle.colliderAuto)
    {
        return;
    }

    obstacle.colliderSize = obstacle.size;
    obstacle.colliderRadius = std::max(obstacle.size.x, obstacle.size.y) * 0.5f;
}

void Game::DrawGround()
{
    float originX = -static_cast<float>(MapWidth) * TileSize * 0.5f;
    float originY = -static_cast<float>(MapHeight) * TileSize * 0.5f;

    for (int y = 0; y < MapHeight; ++y)
    {
        for (int x = 0; x < MapWidth; ++x)
        {
            int tile = tiles[CellIndex(x, y)];

            if (tile < 0 || tile >= static_cast<int>(tileBrushes.size()))
            {
                tile = 0;
            }

            const TileBrush& brush = tileBrushes[tile];

            Rectangle dest{
                originX + x * TileSize,
                originY + y * TileSize,
                TileSize,
                TileSize
            };

            if (brush.hasTexture)
            {
                DrawTextureInRect(brush.texture, dest, brush.autoFitToTile);
            }
            else
            {
                DrawRectangleRec(dest, brush.fallbackColor);
            }
        }
    }
}

void Game::DrawObstacles()
{
    for (const Obstacle& obstacle : obstacles)
    {
        Rectangle visualRect = GetObstacleVisualRect(obstacle);

        if (obstacle.hasTexture)
        {
            DrawTextureInRect(obstacle.texture, visualRect, true);
        }
        else
        {
            float radius = std::max(obstacle.size.x, obstacle.size.y) * 0.5f;

            if (obstacle.type == 0)
            {
                DrawCircleV(Vector2Add(obstacle.position, { 5.0f, 8.0f }), radius + 10.0f, Color{ 0, 0, 0, 65 });
                DrawCircleV(obstacle.position, radius + 10.0f, Color{ 35, 78, 38, 255 });
                DrawCircleV(obstacle.position, radius, Color{ 48, 126, 54, 255 });
                DrawCircleV(Vector2Add(obstacle.position, { -10.0f, -8.0f }), radius * 0.55f, Color{ 76, 153, 73, 255 });
            }
            else
            {
                DrawCircleV(Vector2Add(obstacle.position, { 5.0f, 7.0f }), radius, Color{ 0, 0, 0, 60 });
                DrawCircleV(obstacle.position, radius, Color{ 94, 93, 88, 255 });
                DrawCircleV(Vector2Add(obstacle.position, { -9.0f, -7.0f }), radius * 0.45f, Color{ 126, 125, 116, 255 });
            }
        }

        if (buildMode && obstacle.collisionEnabled)
        {
            if (obstacle.collisionShape == CollisionShape::Box)
            {
                Rectangle collider = GetObstacleCollisionRect(obstacle);
                DrawRectangleLinesEx(collider, 2.0f, Color{ 255, 70, 70, 180 });
            }
            else
            {
                DrawCircleLines(
                    static_cast<int>(obstacle.position.x),
                    static_cast<int>(obstacle.position.y),
                    obstacle.colliderRadius,
                    Color{ 255, 70, 70, 180 }
                );
            }
        }
    }
}

void Game::DrawNpcs()
{
    for (const NPC& npc : npcs)
    {
        DrawCircleLines(
            static_cast<int>(npc.position.x),
            static_cast<int>(npc.position.y),
            interactDistance,
            Color{ 255, 255, 255, 60 }
        );

        DrawCircleV(Vector2Add(npc.position, { 5.0f, 8.0f }), npc.radius + 6.0f, Color{ 0, 0, 0, 70 });
        DrawCircleV(npc.position, npc.radius + 6.0f, Color{ 28, 28, 34, 255 });
        DrawCircleV(npc.position, npc.radius, Color{ 218, 184, 92, 255 });

        DrawText(
            npc.name.c_str(),
            static_cast<int>(npc.position.x - 66.0f),
            static_cast<int>(npc.position.y - 62.0f),
            18,
            WHITE
        );
    }
}

void Game::DrawPlayer()
{
    if (hasPath && pathIndex < static_cast<int>(currentPath.size()))
    {
        for (int i = pathIndex; i < static_cast<int>(currentPath.size()) - 1; ++i)
        {
            DrawLineEx(currentPath[i], currentPath[i + 1], 3.0f, Color{ 255, 255, 255, 100 });
        }

        DrawCircleLines(
            static_cast<int>(currentPath.back().x),
            static_cast<int>(currentPath.back().y),
            18.0f,
            Color{ 255, 255, 255, 180 }
        );
    }

    DrawCircleV(Vector2Add(playerPosition, { 5.0f, 8.0f }), playerRadius + 4.0f, Color{ 0, 0, 0, 85 });
    DrawCircleV(playerPosition, playerRadius + 4.0f, Color{ 36, 36, 44, 255 });
    DrawCircleV(playerPosition, playerRadius, Color{ 78, 148, 255, 255 });
}

void Game::DrawDialogue()
{
    if (activeDialogueNpc == -1)
    {
        return;
    }

    const NPC& npc = npcs[activeDialogueNpc];

    int boxX = 50;
    int boxY = GetScreenHeight() - 155;
    int boxW = GetScreenWidth() - 100;
    int boxH = 115;

    DrawRectangle(boxX, boxY, boxW, boxH, Color{ 20, 20, 24, 235 });
    DrawRectangleLines(boxX, boxY, boxW, boxH, Color{ 235, 210, 140, 255 });

    DrawText(npc.name.c_str(), boxX + 20, boxY + 15, 24, Color{ 235, 210, 140, 255 });
    DrawText(npc.dialogue.c_str(), boxX + 20, boxY + 52, 20, WHITE);
    DrawText("Click / tap to close", boxX + 20, boxY + 84, 16, Color{ 190, 190, 190, 255 });
}

void Game::DrawUi()
{
    DrawRectangle(0, 0, GetScreenWidth(), 62, Color{ 0, 0, 0, 130 });

    DrawText("Moxiang2D - Top-down Prototype", 18, 10, 22, WHITE);

#if MOXIANG_USE_IMGUI
    DrawText("Click ground to move. Click NPC to talk. Press B for Build Mode.", 18, 36, 16, Color{ 225, 225, 225, 255 });
#else
    DrawText("Click/tap ground to move. Click/tap NPC to walk over and talk.", 18, 36, 16, Color{ 225, 225, 225, 255 });
#endif
}

void Game::DrawEditorWorldOverlay()
{
    if (!buildMode)
    {
        return;
    }

    float originX = -static_cast<float>(MapWidth) * TileSize * 0.5f;
    float originY = -static_cast<float>(MapHeight) * TileSize * 0.5f;

    if (showGrid)
    {
        for (int x = 0; x <= MapWidth; ++x)
        {
            float worldX = originX + x * TileSize;

            DrawLineV(
                { worldX, originY },
                { worldX, originY + MapHeight * TileSize },
                Color{ 255, 255, 255, 45 }
            );
        }

        for (int y = 0; y <= MapHeight; ++y)
        {
            float worldY = originY + y * TileSize;

            DrawLineV(
                { originX, worldY },
                { originX + MapWidth * TileSize, worldY },
                Color{ 255, 255, 255, 45 }
            );
        }
    }

    Vector2 mouseWorld = GetScreenToWorld2D(GetMousePosition(), camera);

    int cellX = 0;
    int cellY = 0;

    if (WorldToCell(mouseWorld, cellX, cellY))
    {
        Vector2 center = CellToWorld(cellX, cellY);

        DrawRectangleLines(
            static_cast<int>(center.x - TileSize * 0.5f),
            static_cast<int>(center.y - TileSize * 0.5f),
            static_cast<int>(TileSize),
            static_cast<int>(TileSize),
            YELLOW
        );
    }
}

void Game::DrawEditorUi()
{
#if MOXIANG_USE_IMGUI

    tileImageDropHovered = false;
    obstacleImageDropHovered = false;

    ImGui::Begin("Moxiang2D Builder");

    ImGui::Checkbox("Build Mode (B)", &buildMode);
    ImGui::Checkbox("Show Grid", &showGrid);

    ImGui::Separator();

    if (ImGui::Button("Save Level"))
    {
        SaveLevel(DefaultLevelPath);
    }

    ImGui::SameLine();

    if (ImGui::Button("Load Level"))
    {
        LoadLevel(DefaultLevelPath);
    }

    ImGui::Text("Level file: %s", DefaultLevelPath);

    ImGui::Separator();

    const char* tools[] = {
        "Paint Tile",
        "Place Obstacle",
        "Erase Obstacle",
        "Move Obstacle"
    };

    ImGui::Combo("Tool", &editorTool, tools, 4);

    ImGui::Separator();

    const char* tileNames[] = {
        "Grass",
        "Dirt",
        "Stone",
        "Water / Blocked"
    };

    ImGui::Combo("Tile", &selectedTile, tileNames, 4);

    if (selectedTile >= 0 && selectedTile < static_cast<int>(tileBrushes.size()))
    {
        ImGui::Checkbox("Auto Fit Tile Image", &tileBrushes[selectedTile].autoFitToTile);

        tileImageDropHovered = DrawImageDropZone("Tile Image Slot", tileImagePathInput);

        ImGui::InputText("Tile Image Path", tileImagePathInput, sizeof(tileImagePathInput));

        if (ImGui::Button("Load Tile Image"))
        {
            LoadTileBrushTexture(selectedTile, tileImagePathInput);
        }
    }

    ImGui::Separator();

    const char* obstacleNames[] = {
        "Tree Placeholder",
        "Rock Placeholder"
    };

    ImGui::Combo("Obstacle Type", &selectedObstacleType, obstacleNames, 2);

    ImGui::Checkbox("Snap Obstacle To Grid", &obstacleSnapToGrid);
    ImGui::Checkbox("Auto Fit Obstacle To Grid", &obstacleAutoFitToGrid);

    if (!obstacleAutoFitToGrid)
    {
        ImGui::DragFloat2("Obstacle Size", &newObstacleSize.x, 1.0f, 8.0f, 512.0f);
    }

    obstacleImageDropHovered = DrawImageDropZone("Obstacle Image Slot", obstacleImagePathInput);

    ImGui::InputText("Obstacle Image Path", obstacleImagePathInput, sizeof(obstacleImagePathInput));

    if (ImGui::Button("Load Obstacle Image"))
    {
        LoadCurrentObstacleTexture(obstacleImagePathInput);
    }

    ImGui::Checkbox("Obstacle Collision", &newObstacleCollision);
    ImGui::Checkbox("Auto Collider = Object Size", &newObstacleColliderAuto);

    const char* colliderTypes[] = {
        "Box",
        "Circle"
    };

    ImGui::Combo("Collider Shape", &newObstacleCollisionShape, colliderTypes, 2);

    if (!newObstacleColliderAuto)
    {
        if (newObstacleCollisionShape == static_cast<int>(CollisionShape::Box))
        {
            ImGui::DragFloat2("Collider Size", &newObstacleColliderSize.x, 1.0f, 8.0f, 512.0f);
        }
        else
        {
            ImGui::DragFloat("Collider Radius", &newObstacleColliderRadius, 1.0f, 8.0f, 256.0f);
        }
    }

    ImGui::Separator();

    ImGui::Text("Camera:");
    ImGui::BulletText("Mouse wheel: zoom");
    ImGui::BulletText("Right mouse / middle mouse drag: pan");

    ImGui::Text("Editor:");
    ImGui::BulletText("Paint Tile: hold left mouse");
    ImGui::BulletText("Place Obstacle: left click");
    ImGui::BulletText("Move Obstacle: drag obstacle");
    ImGui::BulletText("Erase Obstacle: click obstacle");

    ImGui::Separator();

    ImGui::Text("Obstacles: %d", static_cast<int>(obstacles.size()));
    ImGui::Text("NPCs: %d", static_cast<int>(npcs.size()));
    ImGui::Text("Camera zoom: %.2f", camera.zoom);

    ImGui::End();
#endif
}

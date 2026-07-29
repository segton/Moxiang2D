#include "Game.h"

#include "raymath.h"
#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

#ifndef MOXIANG_USE_IMGUI
#define MOXIANG_USE_IMGUI 0
#endif

#if MOXIANG_USE_IMGUI
#include "imgui.h"
#endif


/*
static void DrawOrientedEllipse(
    Vector2 center,
    Vector2 direction,
    float halfLength,
    float halfWidth,
    Color color,
    int segments = 24
)
{
    if (
        halfLength <= 0.0f ||
        halfWidth <= 0.0f ||
        segments < 3
        )
    {
        return;
    }

    if (Vector2Length(direction) <= 0.001f)
    {
        direction = {
            1.0f,
            0.0f
        };
    }
    else
    {
        direction =
            Vector2Normalize(
                direction
            );
    }

    Vector2 perpendicular{
        -direction.y,
        direction.x
    };

    for (int i = 0; i < segments; ++i)
    {
        float angle0 =
            static_cast<float>(i) /
            static_cast<float>(segments) *
            PI *
            2.0f;

        float angle1 =
            static_cast<float>(i + 1) /
            static_cast<float>(segments) *
            PI *
            2.0f;

        Vector2 point0{
            center.x +
                direction.x *
                cosf(angle0) *
                halfLength +
                perpendicular.x *
                sinf(angle0) *
                halfWidth,

            center.y +
                direction.y *
                cosf(angle0) *
                halfLength +
                perpendicular.y *
                sinf(angle0) *
                halfWidth
        };

        Vector2 point1{
            center.x +
                direction.x *
                cosf(angle1) *
                halfLength +
                perpendicular.x *
                sinf(angle1) *
                halfWidth,

            center.y +
                direction.y *
                cosf(angle1) *
                halfLength +
                perpendicular.y *
                sinf(angle1) *
                halfWidth
        };

        DrawTriangle(
            center,
            point1,
            point0,
            color
        );
    }
}
*/


static void DrawWrappedText(
    const std::string& text,
    Rectangle bounds,
    int fontSize,
    float lineGap,
    Color color,
    int maxLines
)
{
    if (bounds.width <= 0.0f ||
        bounds.height <= 0.0f ||
        fontSize <= 0 ||
        maxLines <= 0)
    {
        return;
    }

    std::istringstream stream(text);

    std::string word;
    std::string line;

    float y = bounds.y;
    int linesDrawn = 0;

    while (stream >> word)
    {
        std::string testLine =
            line.empty()
            ? word
            : line + " " + word;

        bool exceedsWidth =
            MeasureText(
                testLine.c_str(),
                fontSize
            ) > bounds.width;

        if (exceedsWidth && !line.empty())
        {
            DrawText(
                line.c_str(),
                static_cast<int>(bounds.x),
                static_cast<int>(y),
                fontSize,
                color
            );

            linesDrawn++;

            if (linesDrawn >= maxLines)
            {
                return;
            }

            y += static_cast<float>(fontSize) + lineGap;

            if (y + fontSize > bounds.y + bounds.height)
            {
                return;
            }

            line = word;
        }
        else
        {
            line = testLine;
        }
    }

    if (!line.empty() &&
        linesDrawn < maxLines &&
        y + fontSize <= bounds.y + bounds.height)
    {
        DrawText(
            line.c_str(),
            static_cast<int>(bounds.x),
            static_cast<int>(y),
            fontSize,
            color
        );
    }
}

static std::string ToPortableAssetPath(
    const std::string& rawPath
)
{
    if (rawPath.empty())
    {
        return {};
    }

    std::string path = rawPath;

    // Convert Windows separators to portable separators.
    std::replace(
        path.begin(),
        path.end(),
        '\\',
        '/'
    );

    // Remove a leading "./".
    while (path.rfind("./", 0) == 0)
    {
        path.erase(0, 2);
    }

    // Already portable.
    if (path.rfind("Assets/", 0) == 0)
    {
        return path;
    }

    // Convert:
    // D:/Dev/Moxiang2D/Moxiang2D/Assets/trees1.png
    //
    // Into:
    // Assets/trees1.png
    const std::string assetsMarker =
        "/Assets/";

    std::size_t assetsPosition =
        path.find(assetsMarker);

    if (assetsPosition != std::string::npos)
    {
        // Add 1 to remove the slash before "Assets".
        return path.substr(
            assetsPosition + 1
        );
    }

    // The path is outside Assets.
    // Keep it unchanged so desktop loading can still attempt it.
    return path;
}

static std::string ResolveAssetPathForLoad(
    const std::string& savedPath
)
{
    if (savedPath.empty())
    {
        return {};
    }

    std::string portablePath =
        ToPortableAssetPath(savedPath);

    // Preferred path for both desktop and web.
    if (FileExists(portablePath.c_str()))
    {
        return portablePath;
    }

    // Backward compatibility for old desktop level files.
    // This allows the original absolute path to work once,
    // even before the level is resaved.
    if (FileExists(savedPath.c_str()))
    {
        TraceLog(
            LOG_WARNING,
            "[ASSET PATH] Using legacy absolute path: %s",
            savedPath.c_str()
        );

        return savedPath;
    }

    TraceLog(
        LOG_ERROR,
        "[ASSET PATH] File not found | saved='%s' portable='%s'",
        savedPath.c_str(),
        portablePath.c_str()
    );

    // Return the portable path so raylib's own error log
    // also shows the expected browser path.
    return portablePath;
}

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

static float DistancePointToSegmentSquared(Vector2 point, Vector2 start, Vector2 end, float& outT)
{
    Vector2 segment = Vector2Subtract(end, start);
    Vector2 toPoint = Vector2Subtract(point, start);

    float segmentLengthSq = segment.x * segment.x + segment.y * segment.y;

    if (segmentLengthSq <= 0.0001f)
    {
        outT = 0.0f;
        return DistanceSquared(point, start);
    }

    float t = (toPoint.x * segment.x + toPoint.y * segment.y) / segmentLengthSq;

    if (t < 0.0f)
    {
        t = 0.0f;
    }

    if (t > 1.0f)
    {
        t = 1.0f;
    }

    outT = t;

    Vector2 closest{
        start.x + segment.x * t,
        start.y + segment.y * t
    };

    return DistanceSquared(point, closest);
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

static void DrawTextureFrameOnQuad2D(
    Texture2D texture,
    Rectangle source,
    Vector2 topLeft,
    Vector2 topRight,
    Vector2 bottomRight,
    Vector2 bottomLeft,
    Color tint
)
{
    if (
        texture.id == 0 ||
        texture.width <= 0 ||
        texture.height <= 0
        )
    {
        return;
    }

    const float inverseWidth =
        1.0f /
        static_cast<float>(
            texture.width
            );

    const float inverseHeight =
        1.0f /
        static_cast<float>(
            texture.height
            );

    const float u0 =
        source.x *
        inverseWidth;

    const float v0 =
        source.y *
        inverseHeight;

    const float u1 =
        (
            source.x +
            source.width
            ) *
        inverseWidth;

    const float v1 =
        (
            source.y +
            source.height
            ) *
        inverseHeight;

    rlSetTexture(
        texture.id
    );

    rlBegin(
        RL_QUADS
    );

    rlColor4ub(
        tint.r,
        tint.g,
        tint.b,
        tint.a
    );

    rlTexCoord2f(u0, v0);
    rlVertex2f(
        topLeft.x,
        topLeft.y
    );

    rlTexCoord2f(u0, v1);
    rlVertex2f(
        bottomLeft.x,
        bottomLeft.y
    );

    rlTexCoord2f(u1, v1);
    rlVertex2f(
        bottomRight.x,
        bottomRight.y
    );

    rlTexCoord2f(u1, v0);
    rlVertex2f(
        topRight.x,
        topRight.y
    );

    rlEnd();

    rlSetTexture(0);
}

static void DrawTextureFrameOnWallQuad2D(
    Texture2D texture,
    Rectangle source,
    Vector2 upperStart,
    Vector2 upperEnd,
    Vector2 lowerEnd,
    Vector2 lowerStart,
    Color tint,
    bool flipHorizontal
)
{
    if (
        texture.id == 0 ||
        texture.width <= 0 ||
        texture.height <= 0
        )
    {
        return;
    }

    const float inverseWidth =
        1.0f /
        static_cast<float>(
            texture.width
            );

    const float inverseHeight =
        1.0f /
        static_cast<float>(
            texture.height
            );

    float uStart =
        source.x *
        inverseWidth;

    float uEnd =
        (
            source.x +
            source.width
            ) *
        inverseWidth;

    const float vTop =
        source.y *
        inverseHeight;

    const float vBottom =
        (
            source.y +
            source.height
            ) *
        inverseHeight;

    if (flipHorizontal)
    {
        std::swap(
            uStart,
            uEnd
        );
    }

    rlSetTexture(
        texture.id
    );

    rlBegin(
        RL_QUADS
    );

    rlColor4ub(
        tint.r,
        tint.g,
        tint.b,
        tint.a
    );

    // Upper start.
    rlTexCoord2f(
        uStart,
        vTop
    );

    rlVertex2f(
        upperStart.x,
        upperStart.y
    );

    // Lower start.
    rlTexCoord2f(
        uStart,
        vBottom
    );

    rlVertex2f(
        lowerStart.x,
        lowerStart.y
    );

    // Lower end.
    rlTexCoord2f(
        uEnd,
        vBottom
    );

    rlVertex2f(
        lowerEnd.x,
        lowerEnd.y
    );

    // Upper end.
    rlTexCoord2f(
        uEnd,
        vTop
    );

    rlVertex2f(
        upperEnd.x,
        upperEnd.y
    );

    rlEnd();

    rlSetTexture(0);
}

static void DrawTextureOnQuad(
    Texture2D texture,
    Vector2 topLeft,
    Vector2 topRight,
    Vector2 bottomRight,
    Vector2 bottomLeft,
    Color tint
)
{
    if (texture.id == 0)
    {
        return;
    }

    rlSetTexture(texture.id);
    rlBegin(RL_QUADS);

    rlColor4ub(
        tint.r,
        tint.g,
        tint.b,
        tint.a
    );

    // Use the same winding direction as raylib's
    // normal 2D texture rendering:
    //
    // top-left -> bottom-left ->
    // bottom-right -> top-right

    rlTexCoord2f(0.0f, 0.0f);
    rlVertex2f(
        topLeft.x,
        topLeft.y
    );

    rlTexCoord2f(0.0f, 1.0f);
    rlVertex2f(
        bottomLeft.x,
        bottomLeft.y
    );

    rlTexCoord2f(1.0f, 1.0f);
    rlVertex2f(
        bottomRight.x,
        bottomRight.y
    );

    rlTexCoord2f(1.0f, 0.0f);
    rlVertex2f(
        topRight.x,
        topRight.y
    );

    rlEnd();
    rlSetTexture(0);
}

static Texture2D CreateTilePlaceholderTexture(
    Color baseColor,
    Color alternateColor
)
{
    constexpr int placeholderSize = 64;
    constexpr int checkerSize = 8;

    Image image =
        GenImageChecked(
            placeholderSize,
            placeholderSize,
            checkerSize,
            checkerSize,
            baseColor,
            alternateColor
        );

    Texture2D texture =
        LoadTextureFromImage(
            image
        );

    UnloadImage(
        image
    );

    if (texture.id != 0)
    {
        SetTextureFilter(
            texture,
            TEXTURE_FILTER_POINT
        );
    }

    return texture;
}

static bool GetRampDirectionOffset(
    RampDirection direction,
    int& outX,
    int& outY
)
{
    outX = 0;
    outY = 0;

    switch (direction)
    {
    case RampDirection::North:
        outY = -1;
        return true;

    case RampDirection::East:
        outX = 1;
        return true;

    case RampDirection::South:
        outY = 1;
        return true;

    case RampDirection::West:
        outX = -1;
        return true;

    case RampDirection::None:
    default:
        return false;
    }
}



static Color GetChamberDebugColor(
    int chamberId,
    unsigned char alpha
)
{
    const Color colors[] = {
        Color{ 75, 170, 255, 255 },
        Color{ 255, 155, 70, 255 },
        Color{ 130, 220, 120, 255 },
        Color{ 210, 110, 255, 255 },
        Color{ 255, 220, 80, 255 },
        Color{ 80, 220, 210, 255 },
        Color{ 255, 110, 145, 255 },
        Color{ 175, 175, 255, 255 }
    };

    constexpr int colorCount =
        static_cast<int>(
            sizeof(colors) /
            sizeof(colors[0])
            );

    const int safeIndex =
        chamberId >= 0
        ? chamberId % colorCount
        : 0;

    Color result =
        colors[safeIndex];

    result.a = alpha;
    return result;
}

namespace
{
    struct HybridMeshBuilder
    {
        std::vector<float> vertices;
        std::vector<float> texcoords;
        std::vector<float> normals;
        std::vector<unsigned char> colors;

        void AddVertex(
            Vector3 position,
            Vector2 uv,
            Vector3 normal,
            Color color
        )
        {
            vertices.push_back(position.x);
            vertices.push_back(position.y);
            vertices.push_back(position.z);

            texcoords.push_back(uv.x);
            texcoords.push_back(uv.y);

            normals.push_back(normal.x);
            normals.push_back(normal.y);
            normals.push_back(normal.z);

            colors.push_back(color.r);
            colors.push_back(color.g);
            colors.push_back(color.b);
            colors.push_back(color.a);
        }

        void AddTriangle(
            Vector3 a,
            Vector3 b,
            Vector3 c,
            Vector2 uvA,
            Vector2 uvB,
            Vector2 uvC,
            Vector3 normal,
            Color color
        )
        {
            AddVertex(a, uvA, normal, color);
            AddVertex(b, uvB, normal, color);
            AddVertex(c, uvC, normal, color);
        }

        void AddQuad(
            Vector3 p0,
            Vector3 p1,
            Vector3 p2,
            Vector3 p3,
            Vector3 normal,
            Color color
        )
        {
            AddTriangle(
                p0,
                p1,
                p2,
                { 0.0f, 0.0f },
                { 0.0f, 1.0f },
                { 1.0f, 1.0f },
                normal,
                color
            );

            AddTriangle(
                p0,
                p2,
                p3,
                { 0.0f, 0.0f },
                { 1.0f, 1.0f },
                { 1.0f, 0.0f },
                normal,
                color
            );
        }

        void RemapUv(
            float u0,
            float v0,
            float u1,
            float v1
        )
        {
            for (
                size_t index = 0;
                index + 1 < texcoords.size();
                index += 2
                )
            {
                const float originalU =
                    texcoords[index];

                const float originalV =
                    texcoords[index + 1];

                texcoords[index] =
                    u0 +
                    originalU *
                    (
                        u1 -
                        u0
                        );

                texcoords[index + 1] =
                    v0 +
                    originalV *
                    (
                        v1 -
                        v0
                        );
            }
        }

        Mesh BuildMesh() const
        {
            Mesh mesh{};

            if (vertices.empty())
            {
                return mesh;
            }

            mesh.vertexCount =
                static_cast<int>(vertices.size() / 3);

            mesh.triangleCount =
                mesh.vertexCount / 3;

            mesh.vertices = static_cast<float*>(
                MemAlloc(vertices.size() * sizeof(float))
                );

            mesh.texcoords = static_cast<float*>(
                MemAlloc(texcoords.size() * sizeof(float))
                );

            mesh.normals = static_cast<float*>(
                MemAlloc(normals.size() * sizeof(float))
                );

            mesh.colors = static_cast<unsigned char*>(
                MemAlloc(colors.size() * sizeof(unsigned char))
                );

            if (
                mesh.vertices == nullptr ||
                mesh.texcoords == nullptr ||
                mesh.normals == nullptr ||
                mesh.colors == nullptr
                )
            {
                if (mesh.vertices != nullptr) MemFree(mesh.vertices);
                if (mesh.texcoords != nullptr) MemFree(mesh.texcoords);
                if (mesh.normals != nullptr) MemFree(mesh.normals);
                if (mesh.colors != nullptr) MemFree(mesh.colors);

                return Mesh{};
            }

            std::memcpy(
                mesh.vertices,
                vertices.data(),
                vertices.size() * sizeof(float)
            );

            std::memcpy(
                mesh.texcoords,
                texcoords.data(),
                texcoords.size() * sizeof(float)
            );

            std::memcpy(
                mesh.normals,
                normals.data(),
                normals.size() * sizeof(float)
            );

            std::memcpy(
                mesh.colors,
                colors.data(),
                colors.size() * sizeof(unsigned char)
            );

            UploadMesh(&mesh, false);
            return mesh;
        }
    };

    static Color ScaleHybridColor(Color color, float scale)
    {
        return Color{
            static_cast<unsigned char>(Clamp(color.r * scale, 0.0f, 255.0f)),
            static_cast<unsigned char>(Clamp(color.g * scale, 0.0f, 255.0f)),
            static_cast<unsigned char>(Clamp(color.b * scale, 0.0f, 255.0f)),
            color.a
        };
    }

    static Texture2D CreateHybridCircleTexture(bool softShadow)
    {
        constexpr int size = 64;

        Image image = GenImageColor(size, size, BLANK);

        for (int y = 0; y < size; ++y)
        {
            for (int x = 0; x < size; ++x)
            {
                float dx =
                    (static_cast<float>(x) + 0.5f) /
                    static_cast<float>(size) * 2.0f - 1.0f;

                float dy =
                    (static_cast<float>(y) + 0.5f) /
                    static_cast<float>(size) * 2.0f - 1.0f;

                float distance = sqrtf(dx * dx + dy * dy);

                if (distance > 1.0f)
                {
                    continue;
                }

                unsigned char alpha = 255;

                if (softShadow)
                {
                    float fade =
                        1.0f -
                        Clamp(
                            (distance - 0.15f) /
                            0.85f,
                            0.0f,
                            1.0f
                        );

                    fade *= fade;

                    alpha =
                        static_cast<unsigned char>(
                            190.0f * fade
                            );
                }

                ImageDrawPixel(
                    &image,
                    x,
                    y,
                    Color{ 255, 255, 255, alpha }
                );
            }
        }

        Texture2D texture = LoadTextureFromImage(image);
        UnloadImage(image);

        if (texture.id != 0)
        {
            SetTextureFilter(
                texture,
                softShadow
                ? TEXTURE_FILTER_BILINEAR
                : TEXTURE_FILTER_POINT
            );
        }

        return texture;
    }
}

#define PERF_TIME_BLOCK(field, codeBlock)        \
    do                                           \
    {                                            \
        double perfStartTime = GetTime();        \
        codeBlock                                \
        field = static_cast<float>(              \
            (GetTime() - perfStartTime) * 1000.0 \
        );                                       \
    } while (false)

#ifndef MOXIANG_PROFILE_DRAW_FLUSH
#define MOXIANG_PROFILE_DRAW_FLUSH 1
#endif

#if MOXIANG_PROFILE_DRAW_FLUSH
#define PERF_DRAW_BLOCK(field, codeBlock)         \
    do                                            \
    {                                             \
        double perfStartTime = GetTime();         \
        codeBlock                                 \
        rlDrawRenderBatchActive();                \
        field = static_cast<float>(               \
            (GetTime() - perfStartTime) * 1000.0  \
        );                                        \
    } while (false)
#else
#define PERF_DRAW_BLOCK(field, codeBlock) PERF_TIME_BLOCK(field, codeBlock)
#endif

void Game::Init()
{
    InitTileBrushes();

    camera.target = { 0.0f, 0.0f };
    camera.offset = { 640.0f, 360.0f };
    camera.rotation = 0.0f;
    camera.zoom = 1.5f;

    if (!LoadLevel(DefaultLevelPath))
    {
        LoadDefaultLevel();
    }

    if (
        terrainTileSheet.id == 0 &&
        FileExists(
            terrainTileSheetPath.c_str()
        )
        )
    {
        LoadTerrainTileSheet(
            terrainTileSheetPath,
            4,
            4
        );
    }


    // Older/default maps may not contain chamber information yet.
    if (chambers.empty())
    {
        InitializeTestChambers();
    }
    else
    {
        RebuildChamberBounds();
    }

    Vector2 requestedSpawn =
        CellToWorld(
            MapWidth / 2,
            MapHeight / 2
        );

    int spawnCellX = MapWidth / 2;
    int spawnCellY = MapHeight / 2;

    if (
        FindNearestWalkableCell(
            requestedSpawn,
            spawnCellX,
            spawnCellY
        )
        )
    {
        playerPosition =
            CellToWorld(
                spawnCellX,
                spawnCellY
            );
    }
    else
    {
        playerPosition =
            requestedSpawn;
    }

    // Resolve the initial chamber immediately.
    UpdateActiveChamber(true);

    if (rendererMode == WorldRendererMode::Legacy2D)
    {
        camera.target =
            WorldToViewElevated(playerPosition);
    }
    else
    {
        hybridCameraTargetWorld =
            playerPosition;
    }

    LoadSmallEnemySpriteSheet();
    LoadShooterSpriteSheets();
    LoadBossSpriteSheets();


    LoadHuashanImpactSpriteSheet();

    LoadEnemySpawnEffectSpriteSheet();

    InitCombat();
    LoadPlayerSpriteSheet();

    // The window/OpenGL context already exists when Game::Init() runs.
    LoadIsoGroundShader();
    LoadDashAfterimageShader();

    // Initialise full-screen scene and lighting render targets.
    InitLighting();

    // Initialise the depth-tested hybrid renderer. The old 2D renderer
    // remains available as a fallback through the editor/F6 toggle.
    InitHybrid3D();

    if (
        rendererMode ==
        WorldRendererMode::Legacy2D
        )
    {
        BuildGroundCache();
    }

    TraceLog(
        LOG_WARNING,
        "[GROUND INIT] cacheReady=%d cacheTexture=%u shaderLoaded=%d shaderId=%u",
        groundCacheReady ? 1 : 0,
        groundCache.texture.id,
        isoGroundShaderLoaded ? 1 : 0,
        isoGroundShader.id
    );
}

void Game::Shutdown()
{
    ShutdownHybrid3D();

    if (groundCache.id != 0)
    {
        UnloadRenderTexture(groundCache);
        groundCache = {};
    }

    if (sceneTarget.id != 0)
    {
        UnloadRenderTexture(sceneTarget);
        sceneTarget = {};
    }

    if (lightTarget.id != 0)
    {
        UnloadRenderTexture(lightTarget);
        lightTarget = {};
    }

    if (lightingShader.id != 0)
    {
        UnloadShader(lightingShader);
        lightingShader = {};
    }

    if (isoGroundShader.id != 0)
    {
        UnloadShader(isoGroundShader);
        isoGroundShader = {};
    }

    if (dashAfterimageShader.id != 0)
    {
        UnloadShader(dashAfterimageShader);
        dashAfterimageShader = {};
    }

    if (radialLightTexture.id != 0)
    {
        UnloadTexture(radialLightTexture);
        radialLightTexture = {};
    }

    if (playerSpriteSheet.id != 0)
    {
        UnloadTexture(playerSpriteSheet);
        playerSpriteSheet = {};
    }

    if (playerAttackSpriteSheet.id != 0)
    {
        UnloadTexture(playerAttackSpriteSheet);
        playerAttackSpriteSheet = {};
    }

    if (playerIdleSpriteSheet.id != 0)
    {
        UnloadTexture(playerIdleSpriteSheet);
        playerIdleSpriteSheet = {};
    }

    if (smallEnemySpriteSheet.id != 0)
    {
        UnloadTexture(smallEnemySpriteSheet);
        smallEnemySpriteSheet = {};
    }

    if (currentObstacleTexture.id != 0)
    {
        UnloadTexture(currentObstacleTexture);
        currentObstacleTexture = {};
    }

    if (terrainTileSheet.id != 0)
    {
        UnloadTexture(
            terrainTileSheet
        );

        terrainTileSheet = {};
    }

    if (huashanImpactSpriteSheet.id != 0)
    {
        UnloadTexture(huashanImpactSpriteSheet);
        huashanImpactSpriteSheet = {};
        huashanImpactSpriteLoaded = false;
    }

    if (
        enemySpawnEffectSpriteSheet.id !=
        0
        )
    {
        UnloadTexture(
            enemySpawnEffectSpriteSheet
        );

        enemySpawnEffectSpriteSheet = {};
    }

    enemySpawnEffectSpriteLoaded =
        false;

    if (shooterIdleSpriteSheet.id != 0)
    {
        UnloadTexture(
            shooterIdleSpriteSheet
        );

        shooterIdleSpriteSheet = {};
    }

    if (shooterWalkSpriteSheet.id != 0)
    {
        UnloadTexture(
            shooterWalkSpriteSheet
        );

        shooterWalkSpriteSheet = {};
    }

    if (shooterAttackSpriteSheet.id != 0)
    {
        UnloadTexture(
            shooterAttackSpriteSheet
        );

        shooterAttackSpriteSheet = {};
    }

    shooterIdleSpriteLoaded = false;
    shooterWalkSpriteLoaded = false;
    shooterAttackSpriteLoaded = false;

    if (bossIdleSpriteSheet.id != 0)
    {
        UnloadTexture(
            bossIdleSpriteSheet
        );

        bossIdleSpriteSheet = {};
    }

    if (bossWalkSpriteSheet.id != 0)
    {
        UnloadTexture(
            bossWalkSpriteSheet
        );

        bossWalkSpriteSheet = {};
    }

    bossIdleSpriteLoaded = false;
    bossWalkSpriteLoaded = false;

    if (bossRoarSpriteSheet.id != 0)
    {
        UnloadTexture(
            bossRoarSpriteSheet
        );

        bossRoarSpriteSheet = {};
    }

    if (bossStunnedSpriteSheet.id != 0)
    {
        UnloadTexture(
            bossStunnedSpriteSheet
        );

        bossStunnedSpriteSheet = {};
    }

    if (bossPreChargeSpriteSheet.id != 0)
    {
        UnloadTexture(
            bossPreChargeSpriteSheet
        );

        bossPreChargeSpriteSheet = {};
    }

    if (bossChargeSpriteSheet.id != 0)
    {
        UnloadTexture(
            bossChargeSpriteSheet
        );

        bossChargeSpriteSheet = {};
    }

    bossRoarSpriteLoaded = false;
    bossStunnedSpriteLoaded = false;
    bossPreChargeSpriteLoaded = false;
    bossChargeSpriteLoaded = false;

    if (bossAttackSpriteSheet.id != 0)
    {
        UnloadTexture(
            bossAttackSpriteSheet
        );

        bossAttackSpriteSheet = {};
    }
    if (
        bossPreHealSpriteSheet.id !=
        0
        )
    {
        UnloadTexture(
            bossPreHealSpriteSheet
        );

        bossPreHealSpriteSheet = {};
    }

    if (
        bossHealingLoopSpriteSheet.id !=
        0
        )
    {
        UnloadTexture(
            bossHealingLoopSpriteSheet
        );

        bossHealingLoopSpriteSheet = {};
    }

    bossPreHealSpriteLoaded =
        false;

    bossHealingLoopSpriteLoaded =
        false;

    bossAttackSpriteLoaded = false;
    for (Obstacle& obstacle : obstacles)
    {
        if (obstacle.texture.id != 0)
        {
            UnloadTexture(obstacle.texture);
            obstacle.texture = {};
            obstacle.hasTexture = false;
        }
    }

    for (TileBrush& brush : tileBrushes)
    {
        if (
            !brush.usesAtlas &&
            brush.texture.id != 0
            )
        {
            UnloadTexture(
                brush.texture
            );
        }

        brush.texture = {};
        brush.hasTexture = false;
    }
}

void Game::InitTileBrushes()
{
    // Release textures from the previous brush list.
    for (TileBrush& brush : tileBrushes)
    {
        if (
            brush.hasTexture &&
            brush.texture.id != 0
            )
        {
            UnloadTexture(
                brush.texture
            );
        }
    }

    tileBrushes.clear();

    const Color baseColors[4] = {
        // Grass
        Color{ 70, 115, 72, 255 },

        // Dirt
        Color{ 116, 91, 55, 255 },

        // Stone
        Color{ 104, 104, 100, 255 },

        // Water
        Color{ 60, 100, 150, 255 }
    };

    const Color alternateColors[4] = {
        // Grass
        Color{ 82, 132, 82, 255 },

        // Dirt
        Color{ 137, 107, 67, 255 },

        // Stone
        Color{ 126, 126, 122, 255 },

        // Water
        Color{ 72, 122, 180, 255 }
    };

    // Always create four valid brushes first.
    // Real image files replace these generated placeholders.
    for (int tileIndex = 0; tileIndex < 4; ++tileIndex)
    {
        TileBrush brush;

        brush.imagePath.clear();
        brush.autoFitToTile = true;
        brush.fallbackColor =
            baseColors[tileIndex];

        brush.texture =
            CreateTilePlaceholderTexture(
                baseColors[tileIndex],
                alternateColors[tileIndex]
            );

        brush.hasTexture =
            brush.texture.id != 0;

        tileBrushes.push_back(
            brush
        );
    }

    // Add or change paths here to match your real filenames.
    const char* defaultTextureCandidates[4][7] = {
        // Grass
        {
            "Assets/grass.jpg",
            "Assets/grass.png",
            "Assets/tiles/grass.jpg",
            "Assets/tiles/grass.png",
            "Assets/tiles/grass_tile.png",
            nullptr,
            nullptr
        },

        // Dirt
        {
            "Assets/dirt.jpg",
            "Assets/dirt.png",
            "Assets/tiles/dirt.jpg",
            "Assets/tiles/dirt.png",
            "Assets/tiles/dirt_tile.png",
            nullptr,
            nullptr
        },

        // Stone
        {
            "Assets/stone.jpg",
            "Assets/stone.png",
            "Assets/tiles/stone.jpg",
            "Assets/tiles/stone.png",
            "Assets/tiles/stone_tile.png",
            nullptr,
            nullptr
        },

        // Water
        {
            "Assets/water.jpg",
            "Assets/water.png",
            "Assets/tiles/water.jpg",
            "Assets/tiles/water.png",
            "Assets/tiles/water_tile.png",
            nullptr,
            nullptr
        }
    };

    for (int tileIndex = 0; tileIndex < 4; ++tileIndex)
    {
        bool loadedRealTexture = false;

        for (
            int candidateIndex = 0;
            candidateIndex < 7;
            ++candidateIndex
            )
        {
            const char* candidate =
                defaultTextureCandidates
                [tileIndex]
                [candidateIndex];

            if (candidate == nullptr)
            {
                break;
            }

            if (!FileExists(candidate))
            {
                continue;
            }

            if (
                LoadTileBrushTexture(
                    tileIndex,
                    candidate
                )
                )
            {
                loadedRealTexture = true;
                break;
            }
        }

        if (!loadedRealTexture)
        {
            TraceLog(
                LOG_WARNING,
                "[TILE TEXTURE] Brush %d has no image. "
                "Using generated placeholder.",
                tileIndex
            );
        }
    }

    InvalidateGroundCache();
}

void Game::LoadDefaultLevel()
{
    tiles.assign(MapWidth * MapHeight, static_cast<int>(TileType::Grass));
    terrainCells.assign(MapWidth * MapHeight, TerrainCell{});

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

Rectangle Game::MakeChamberWorldBounds(
    int minCellX,
    int minCellY,
    int maxCellX,
    int maxCellY
) const
{
    minCellX = std::max(0, std::min(MapWidth - 1, minCellX));
    minCellY = std::max(0, std::min(MapHeight - 1, minCellY));
    maxCellX = std::max(0, std::min(MapWidth - 1, maxCellX));
    maxCellY = std::max(0, std::min(MapHeight - 1, maxCellY));

    if (maxCellX < minCellX)
    {
        std::swap(minCellX, maxCellX);
    }

    if (maxCellY < minCellY)
    {
        std::swap(minCellY, maxCellY);
    }

    const float mapOriginX =
        -static_cast<float>(MapWidth) *
        TileSize *
        0.5f;

    const float mapOriginY =
        -static_cast<float>(MapHeight) *
        TileSize *
        0.5f;

    return Rectangle{
        mapOriginX +
            static_cast<float>(minCellX) *
            TileSize,

        mapOriginY +
            static_cast<float>(minCellY) *
            TileSize,

        static_cast<float>(
            maxCellX -
            minCellX +
            1
        ) *
            TileSize,

        static_cast<float>(
            maxCellY -
            minCellY +
            1
        ) *
            TileSize
    };
}

void Game::InitializeTestChambers()
{
    const size_t expectedCellCount =
        static_cast<size_t>(
            MapWidth *
            MapHeight
            );

    if (
        terrainCells.size() !=
        expectedCellCount
        )
    {
        terrainCells.assign(
            expectedCellCount,
            TerrainCell{}
        );
    }

    chambers.clear();

    EnsureChamberExists(0);
    EnsureChamberExists(1);

    DungeonChamber* westChamber =
        FindChamberById(0);

    DungeonChamber* eastChamber =
        FindChamberById(1);

    if (westChamber != nullptr)
    {
        westChamber->name =
            "West Chamber";
    }

    if (eastChamber != nullptr)
    {
        eastChamber->name =
            "East Chamber";
    }

    const int splitX =
        MapWidth /
        2;

    for (int y = 0; y < MapHeight; ++y)
    {
        for (int x = 0; x < MapWidth; ++x)
        {
            TerrainCell& cell =
                terrainCells[
                    CellIndex(x, y)
                ];

            cell.enabled = true;
            cell.chamberId =
                x < splitX
                ? 0
                : 1;
        }
    }

    RebuildChamberBounds();

    activeChamberId = -1;
    previousChamberId = -1;

    TraceLog(
        LOG_INFO,
        "[CHAMBER] Initialized %d test chambers.",
        static_cast<int>(
            chambers.size()
            )
    );
}

int Game::FindChamberAtWorld(
    Vector2 worldPosition
) const
{
    int cellX = 0;
    int cellY = 0;

    if (
        !WorldToCell(
            worldPosition,
            cellX,
            cellY
        ) ||
        !IsCellEnabled(
            cellX,
            cellY
        )
        )
    {
        return -1;
    }

    return
        terrainCells[
            CellIndex(
                cellX,
                cellY
            )
        ].chamberId;
}

bool Game::IsCellEnabled(
    int cellX,
    int cellY
) const
{
    if (
        !IsCellInside(
            cellX,
            cellY
        )
        )
    {
        return false;
    }

    const int index =
        CellIndex(
            cellX,
            cellY
        );

    if (
        index < 0 ||
        index >=
        static_cast<int>(
            terrainCells.size()
            )
        )
    {
        return false;
    }

    return terrainCells[index].enabled;
}

void Game::EnsureChamberExists(
    int chamberId
)
{
    if (chamberId < 0)
    {
        return;
    }

    if (
        FindChamberById(
            chamberId
        ) != nullptr
        )
    {
        return;
    }

    DungeonChamber chamber;

    chamber.id = chamberId;
    chamber.name =
        "Chamber " +
        std::to_string(
            chamberId
        );

    chamber.minCellX = MapWidth;
    chamber.minCellY = MapHeight;
    chamber.maxCellX = -1;
    chamber.maxCellY = -1;
    chamber.hasCells = false;

    chambers.push_back(
        chamber
    );
}

void Game::RebuildChamberBounds()
{
    for (DungeonChamber& chamber : chambers)
    {
        chamber.minCellX = MapWidth;
        chamber.minCellY = MapHeight;
        chamber.maxCellX = -1;
        chamber.maxCellY = -1;
        chamber.hasCells = false;
        chamber.worldBounds = {};
    }

    for (int y = 0; y < MapHeight; ++y)
    {
        for (int x = 0; x < MapWidth; ++x)
        {
            if (!IsCellEnabled(x, y))
            {
                continue;
            }

            const int chamberId =
                terrainCells[
                    CellIndex(x, y)
                ].chamberId;

            if (chamberId < 0)
            {
                continue;
            }

            EnsureChamberExists(
                chamberId
            );

            DungeonChamber* chamber =
                FindChamberById(
                    chamberId
                );

            if (chamber == nullptr)
            {
                continue;
            }

            chamber->hasCells = true;

            chamber->minCellX =
                std::min(
                    chamber->minCellX,
                    x
                );

            chamber->minCellY =
                std::min(
                    chamber->minCellY,
                    y
                );

            chamber->maxCellX =
                std::max(
                    chamber->maxCellX,
                    x
                );

            chamber->maxCellY =
                std::max(
                    chamber->maxCellY,
                    y
                );
        }
    }

    for (DungeonChamber& chamber : chambers)
    {
        if (!chamber.hasCells)
        {
            chamber.worldBounds = {};
            continue;
        }

        chamber.worldBounds =
            MakeChamberWorldBounds(
                chamber.minCellX,
                chamber.minCellY,
                chamber.maxCellX,
                chamber.maxCellY
            );
    }
}

void Game::CreateNewMap(
    int width,
    int height,
    bool startEmpty
)
{
    width =
        std::max(
            4,
            std::min(
                MaximumMapDimension,
                width
            )
        );

    height =
        std::max(
            4,
            std::min(
                MaximumMapDimension,
                height
            )
        );

    MapWidth = width;
    MapHeight = height;

    editorNewMapWidth = MapWidth;
    editorNewMapHeight = MapHeight;

    tiles.assign(
        MapWidth * MapHeight,
        static_cast<int>(
            TileType::Grass
            )
    );

    terrainCells.assign(
        MapWidth * MapHeight,
        TerrainCell{}
    );

    for (TerrainCell& cell : terrainCells)
    {
        cell.enabled =
            !startEmpty;

        cell.chamberId =
            startEmpty
            ? -1
            : 0;

        cell.elevation = 0;
        cell.rampDirection =
            RampDirection::None;
    }

    chambers.clear();
    EnsureChamberExists(0);

    DungeonChamber* firstChamber =
        FindChamberById(0);

    if (firstChamber != nullptr)
    {
        firstChamber->name =
            "Chamber 0";
    }

    // Keep a small starting platform on an otherwise empty map.
    if (startEmpty)
    {
        const int centerX = MapWidth / 2;
        const int centerY = MapHeight / 2;

        for (int offsetY = -1; offsetY <= 1; ++offsetY)
        {
            for (int offsetX = -1; offsetX <= 1; ++offsetX)
            {
                const int cellX = centerX + offsetX;
                const int cellY = centerY + offsetY;

                if (!IsCellInside(cellX, cellY))
                {
                    continue;
                }

                TerrainCell& cell =
                    terrainCells[
                        CellIndex(
                            cellX,
                            cellY
                        )
                    ];

                cell.enabled = true;
                cell.chamberId = 0;
            }
        }
    }

    for (Obstacle& obstacle : obstacles)
    {
        if (obstacle.texture.id != 0)
        {
            UnloadTexture(
                obstacle.texture
            );

            obstacle.texture = {};
        }
    }

    obstacles.clear();
    npcs.clear();

    currentPath.clear();
    pathIndex = 0;
    hasPath = false;

    pendingNpc = -1;
    activeDialogueNpc = -1;

    playerPosition =
        CellToWorld(
            MapWidth / 2,
            MapHeight / 2
        );

    player.pos = playerPosition;
    player.moveTarget = playerPosition;
    player.hasMoveTarget = false;

    RebuildChamberBounds();
    UpdateActiveChamber(true);

    InitCombat();

    // Keep the construction canvas quiet until gameplay is restarted.
    waveSpawningPaused = true;

    if (
        rendererMode ==
        WorldRendererMode::Hybrid3D
        )
    {
        hybridCameraTargetWorld =
            playerPosition;

        UpdateHybridCamera(0.0f);
    }
    else
    {
        camera.target =
            WorldToViewElevated(
                playerPosition
            );
    }

    InvalidateGroundCache();

    TraceLog(
        LOG_INFO,
        "[MAP] Created new map: %dx%d, empty=%d.",
        MapWidth,
        MapHeight,
        startEmpty ? 1 : 0
    );
}

DungeonChamber* Game::FindChamberById(
    int chamberId
)
{
    for (DungeonChamber& chamber : chambers)
    {
        if (chamber.id == chamberId)
        {
            return &chamber;
        }
    }

    return nullptr;
}

const DungeonChamber* Game::FindChamberById(
    int chamberId
) const
{
    for (const DungeonChamber& chamber : chambers)
    {
        if (chamber.id == chamberId)
        {
            return &chamber;
        }
    }

    return nullptr;
}

void Game::UpdateActiveChamber(
    bool forceUpdate
)
{
    const int detectedChamberId =
        FindChamberAtWorld(
            playerPosition
        );

    // Chamber -1 is reserved for shared transition floor/corridors.
    // Walking across one keeps the last real chamber active until the
    // player reaches another painted chamber.
    if (detectedChamberId < 0)
    {
        return;
    }

    if (
        !forceUpdate &&
        detectedChamberId == activeChamberId
        )
    {
        return;
    }

    const int oldChamberId =
        activeChamberId;

    previousChamberId =
        oldChamberId;

    activeChamberId =
        detectedChamberId;

    for (DungeonChamber& chamber : chambers)
    {
        const bool isActive =
            chamber.id == activeChamberId;

        chamber.active =
            isActive;

        chamber.targetVisibility =
            isActive
            ? 1.0f
            : 0.0f;

        if (isActive)
        {
            chamber.discovered = true;
        }

        // Initial placement and the legacy cached renderer switch
        // immediately. Hybrid 3D uses UpdateChamberVisibility().
        if (
            forceUpdate ||
            rendererMode == WorldRendererMode::Legacy2D
            )
        {
            chamber.visibility =
                chamber.targetVisibility;
        }
    }

    // All combat objects from the cleared room disappear when the next
    // room becomes active. Persistent player skills remain untouched.
    if (
        oldChamberId >= 0 &&
        oldChamberId != activeChamberId
        )
    {
        projectiles.clear();
        pendingEnemySpawns.clear();
        vfxParticles.clear();
        bossFallingRocks.clear();
        huashanGroundMarks.clear();

        dongfengCasting = false;
        dongfengWaveActive = false;
    }

    InvalidateGroundCache();

    DungeonChamber* newChamber =
        FindChamberById(
            activeChamberId
        );

    if (
        newChamber != nullptr &&
        !newChamber->cleared &&
        !newChamber->encounterStarted &&
        !buildMode &&
        gameState == GameState::Playing
        )
    {
        StartChamberEncounter(
            activeChamberId
        );
    }
    else if (
        newChamber != nullptr &&
        newChamber->cleared
        )
    {
        wave.chamberId =
            newChamber->id;

        wave.chamberWave =
            newChamber->wavesRequired;

        wave.chamberWavesRequired =
            newChamber->wavesRequired;

        wave.enemiesSpawned = 0;
        wave.enemiesToSpawn = 0;
        wave.waveActive = false;
        wave.waitingForNextWave = false;
    }

    TraceLog(
        LOG_INFO,
        "[CHAMBER] Player moved from %d to %d (%s).",
        oldChamberId,
        activeChamberId,
        newChamber != nullptr
        ? newChamber->name.c_str()
        : "no chamber"
    );
}

void Game::UpdateChamberVisibility(
    float dt
)
{
    if (rendererMode == WorldRendererMode::Legacy2D)
    {
        return;
    }

    const float maximumChange =
        std::max(
            0.0f,
            chamberFadeSpeed * dt
        );

    for (DungeonChamber& chamber : chambers)
    {
        const float difference =
            chamber.targetVisibility -
            chamber.visibility;

        if (std::fabs(difference) <= maximumChange)
        {
            chamber.visibility =
                chamber.targetVisibility;
        }
        else
        {
            chamber.visibility +=
                difference > 0.0f
                ? maximumChange
                : -maximumChange;
        }

        chamber.visibility =
            Clamp(
                chamber.visibility,
                0.0f,
                1.0f
            );
    }
}

float Game::GetChamberVisibility(
    int chamberId
) const
{
    if (buildMode || chamberId < 0)
    {
        return 1.0f;
    }

    const DungeonChamber* chamber =
        FindChamberById(
            chamberId
        );

    return
        chamber != nullptr
        ? Clamp(
            chamber->visibility,
            0.0f,
            1.0f
        )
        : 0.0f;
}

bool Game::IsChamberVisible(
    int chamberId
) const
{
    return
        GetChamberVisibility(
            chamberId
        ) > 0.01f;
}

bool Game::IsWorldPositionInVisibleChamber(
    Vector2 worldPosition
) const
{
    int cellX = 0;
    int cellY = 0;

    if (
        !WorldToCell(
            worldPosition,
            cellX,
            cellY
        ) ||
        !IsCellEnabled(
            cellX,
            cellY
        )
        )
    {
        return false;
    }

    const int chamberId =
        terrainCells[
            CellIndex(
                cellX,
                cellY
            )
        ].chamberId;

    return
        chamberId < 0 ||
        IsChamberVisible(
            chamberId
        );
}

void Game::ResetChamberEncounterProgress()
{
    for (DungeonChamber& chamber : chambers)
    {
        chamber.cleared = false;
        chamber.encounterStarted = false;
        chamber.wavesRequired = 2;
        chamber.wavesCompleted = 0;
        chamber.currentWave = 0;

        chamber.active =
            chamber.id == activeChamberId;

        chamber.discovered =
            chamber.active;

        chamber.targetVisibility =
            chamber.active
            ? 1.0f
            : 0.0f;

        chamber.visibility =
            chamber.targetVisibility;
    }

    wave = {};
    wave.chamberWavesRequired = 2;

    previousChamberId = -1;
    chamberClearedMessageTimer = 0.0f;

    InvalidateGroundCache();
}

void Game::StartChamberEncounter(
    int chamberId
)
{
    DungeonChamber* chamber =
        FindChamberById(
            chamberId
        );

    if (
        chamber == nullptr ||
        !chamber->hasCells ||
        chamber->cleared
        )
    {
        return;
    }

    chamber->encounterStarted = true;
    chamber->wavesRequired = 2;
    chamber->wavesCompleted = 0;
    chamber->currentWave = 1;

    StartChamberWave(
        chamberId,
        1
    );
}

bool Game::HasLivingEnemiesInChamber(
    int chamberId
) const
{
    for (const Enemy& enemy : enemies)
    {
        if (
            enemy.active &&
            enemy.chamberId == chamberId
            )
        {
            return true;
        }
    }

    return false;
}

bool Game::HasPendingEnemiesInChamber(
    int chamberId
) const
{
    for (
        const PendingEnemySpawn& pending :
        pendingEnemySpawns
        )
    {
        if (pending.chamberId == chamberId)
        {
            return true;
        }
    }

    return false;
}

void Game::LoadEnemySpawnEffectSpriteSheet()
{
    enemySpawnEffectSpriteSheet =
        LoadTexture(
            "Assets/vfx/enemy_spawn_circle.png"
        );

    if (
        enemySpawnEffectSpriteSheet.id ==
        0
        )
    {
        enemySpawnEffectSpriteLoaded =
            false;

        TraceLog(
            LOG_WARNING,
            "[ENEMY SPAWN VFX] Failed to load "
            "Assets/vfx/enemy_spawn_circle.png"
        );

        return;
    }

    enemySpawnEffectSpriteLoaded =
        true;

    SetTextureFilter(
        enemySpawnEffectSpriteSheet,
        TEXTURE_FILTER_BILINEAR
    );

    const int safeColumns =
        std::max(
            1,
            enemySpawnEffectColumns
        );

    const int safeRows =
        std::max(
            1,
            enemySpawnEffectRows
        );

    enemySpawnEffectFrameWidth =
        enemySpawnEffectSpriteSheet.width /
        safeColumns;

    enemySpawnEffectFrameHeight =
        enemySpawnEffectSpriteSheet.height /
        safeRows;

    enemySpawnEffectFrameCount =
        safeColumns *
        safeRows;

    TraceLog(
        LOG_INFO,
        "[ENEMY SPAWN VFX] Loaded: %dx%d | "
        "frame=%dx%d | frames=%d",
        enemySpawnEffectSpriteSheet.width,
        enemySpawnEffectSpriteSheet.height,
        enemySpawnEffectFrameWidth,
        enemySpawnEffectFrameHeight,
        enemySpawnEffectFrameCount
    );
}

void Game::LoadHuashanImpactSpriteSheet()
{
    huashanImpactSpriteSheet =
        LoadTexture(
            "Assets/vfx/huashan_impact.png"
        );

    if (huashanImpactSpriteSheet.id == 0)
    {
        huashanImpactSpriteLoaded = false;

        TraceLog(
            LOG_ERROR,
            "[HUASHAN VFX] Failed to load "
            "Assets/vfx/huashan_impact.png"
        );

        return;
    }

    huashanImpactSpriteLoaded = true;

    SetTextureFilter(
        huashanImpactSpriteSheet,
        TEXTURE_FILTER_BILINEAR
    );

    huashanImpactFrameWidth =
        huashanImpactSpriteSheet.width /
        huashanImpactColumns;

    huashanImpactFrameHeight =
        huashanImpactSpriteSheet.height /
        huashanImpactRows;

    huashanImpactFrameCount =
        huashanImpactColumns *
        huashanImpactRows;

    TraceLog(
        LOG_INFO,
        "[HUASHAN VFX] Loaded: %dx%d | frame=%dx%d | frames=%d",
        huashanImpactSpriteSheet.width,
        huashanImpactSpriteSheet.height,
        huashanImpactFrameWidth,
        huashanImpactFrameHeight,
        huashanImpactFrameCount
    );
}

void Game::StartHuashanImpactAnimation(
    Vector2 worldPosition
)
{
    if (
        !huashanImpactSpriteLoaded ||
        huashanImpactSpriteSheet.id == 0
        )
    {
        TraceLog(
            LOG_ERROR,
            "[HUASHAN VFX] Cannot start: texture unavailable."
        );

        return;
    }

    huashanImpactAnimationPosition =
        worldPosition;

    huashanImpactAnimationActive =
        true;

    huashanImpactFrame = 0;
    huashanImpactFrameTimer = 0.0f;

    TraceLog(
        LOG_INFO,
        "[HUASHAN VFX] START position=(%.1f, %.1f) texture=%u",
        worldPosition.x,
        worldPosition.y,
        huashanImpactSpriteSheet.id
    );
}

void Game::UpdateHuashanImpactAnimation(
    float dt
)
{
    // ----------------------------------------------
    // Update old crater marks.
    // ----------------------------------------------

    for (
        HuashanGroundMark& mark :
        huashanGroundMarks
        )
    {
        mark.life -= dt;

        if (mark.life < 0.0f)
        {
            mark.life = 0.0f;
        }
    }

    huashanGroundMarks.erase(
        std::remove_if(
            huashanGroundMarks.begin(),
            huashanGroundMarks.end(),
            [](
                const HuashanGroundMark& mark
                )
            {
                return mark.life <= 0.0f;
            }
        ),
        huashanGroundMarks.end()
    );

    // ----------------------------------------------
    // Update the currently playing impact animation.
    // ----------------------------------------------

    if (
        !huashanImpactAnimationActive ||
        huashanImpactFrameDuration <= 0.0f
        )
    {
        return;
    }

    huashanImpactFrameTimer += dt;

    while (
        huashanImpactFrameTimer >=
        huashanImpactFrameDuration
        )
    {
        huashanImpactFrameTimer -=
            huashanImpactFrameDuration;

        if (
            huashanImpactFrame <
            huashanImpactFrameCount - 1
            )
        {
            huashanImpactFrame++;
            continue;
        }

        // The last sprite-sheet frame has finished playing.
        // Convert it into a persistent fading ground mark.
        HuashanGroundMark mark;

        mark.position =
            huashanImpactAnimationPosition;

        mark.life =
            huashanGroundMarkFadeDuration;

        mark.maxLife =
            huashanGroundMarkFadeDuration;

        huashanGroundMarks.push_back(
            mark
        );

        // Prevent unlimited persistent marks.
        constexpr int maximumMarks = 8;

        if (
            static_cast<int>(
                huashanGroundMarks.size()
                ) >
            maximumMarks
            )
        {
            huashanGroundMarks.erase(
                huashanGroundMarks.begin()
            );
        }

        huashanImpactAnimationActive =
            false;

        huashanImpactFrame =
            huashanImpactFrameCount - 1;

        huashanImpactFrameTimer =
            0.0f;

        break;
    }
}

void Game::DrawHuashanImpactAnimation()
{
    if (
        !huashanImpactAnimationActive ||
        !huashanImpactSpriteLoaded ||
        huashanImpactSpriteSheet.id == 0
        )
    {
        return;
    }

    int column =
        huashanImpactFrame %
        huashanImpactColumns;

    int row =
        huashanImpactFrame /
        huashanImpactColumns;

    Rectangle source{
        static_cast<float>(
            column *
            huashanImpactFrameWidth
        ),

        static_cast<float>(
            row *
            huashanImpactFrameHeight
        ),

        static_cast<float>(
            huashanImpactFrameWidth
        ),

        static_cast<float>(
            huashanImpactFrameHeight
        )
    };

    Vector2 drawPosition =
        WorldToViewElevated(
            huashanImpactAnimationPosition,
            4.0f
        );

    Rectangle destination{
        drawPosition.x,
        drawPosition.y,
        huashanImpactVisualSize,
        huashanImpactVisualSize
    };

    Vector2 origin{
        huashanImpactVisualSize * 0.5f,
        huashanImpactVisualSize * 0.5f
    };

    DrawTexturePro(
        huashanImpactSpriteSheet,
        source,
        destination,
        origin,
        0.0f,
        WHITE
    );
}

void Game::LoadPlayerSpriteSheet()
{
    // Walking / idle sheet.
    playerSpriteSheet =
        LoadTexture(
            "Assets/player/spritesheet (13).png"
        );

    if (playerSpriteSheet.id != 0)
    {
        playerSpriteLoaded = true;

        SetTextureFilter(
            playerSpriteSheet,
            TEXTURE_FILTER_POINT
        );

        TraceLog(
            LOG_INFO,
            "[PLAYER SPRITE] Walking sheet loaded: %dx%d",
            playerSpriteSheet.width,
            playerSpriteSheet.height
        );
    }
    else
    {
        playerSpriteLoaded = false;

        TraceLog(
            LOG_ERROR,
            "[PLAYER SPRITE] Failed to load walking sheet."
        );
    }

    // Separate attack sheet.
    playerAttackSpriteSheet =
        LoadTexture(
            "Assets/player/player_attack.png"
        );

    if (playerAttackSpriteSheet.id != 0)
    {
        playerAttackSpriteLoaded = true;

        SetTextureFilter(
            playerAttackSpriteSheet,
            TEXTURE_FILTER_POINT
        );

        TraceLog(
            LOG_INFO,
            "[PLAYER SPRITE] Attack sheet loaded: %dx%d",
            playerAttackSpriteSheet.width,
            playerAttackSpriteSheet.height
        );

        int expectedWidth =
            playerAttackFrameWidth *
            playerAttackFramesPerRow;

        int expectedHeight =
            playerAttackFrameHeight *
            playerDirectionRows;

        if (
            playerAttackSpriteSheet.width <
            expectedWidth ||
            playerAttackSpriteSheet.height <
            expectedHeight
            )
        {
            TraceLog(
                LOG_WARNING,
                "[PLAYER SPRITE] Attack sheet is smaller than expected. "
                "Expected at least %dx%d, received %dx%d.",
                expectedWidth,
                expectedHeight,
                playerAttackSpriteSheet.width,
                playerAttackSpriteSheet.height
            );
        }
    }
    else
    {
        playerAttackSpriteLoaded = false;

        TraceLog(
            LOG_ERROR,
            "[PLAYER SPRITE] Failed to load: "
            "Assets/player/player_attack.png"
        );
    }
    // Separate idle sheet.
    playerIdleSpriteSheet =
        LoadTexture(
            "Assets/player/player_idle.png"
        );

    if (playerIdleSpriteSheet.id != 0)
    {
        playerIdleSpriteLoaded = true;

        SetTextureFilter(
            playerIdleSpriteSheet,
            TEXTURE_FILTER_POINT
        );

        // Detect the number of idle frames from the sheet width.
        int detectedIdleFrames =
            playerIdleSpriteSheet.width /
            playerIdleFrameWidth;

        if (detectedIdleFrames > 0)
        {
            playerIdleFramesPerRow =
                detectedIdleFrames;
        }

        TraceLog(
            LOG_INFO,
            "[PLAYER SPRITE] Idle sheet loaded: %dx%d | frames=%d",
            playerIdleSpriteSheet.width,
            playerIdleSpriteSheet.height,
            playerIdleFramesPerRow
        );

        int expectedIdleHeight =
            playerIdleFrameHeight *
            playerDirectionRows;

        if (
            playerIdleSpriteSheet.height <
            expectedIdleHeight
            )
        {
            TraceLog(
                LOG_WARNING,
                "[PLAYER SPRITE] Idle sheet is shorter than expected. "
                "Expected at least %d pixels for 8 directions, received %d.",
                expectedIdleHeight,
                playerIdleSpriteSheet.height
            );
        }
    }
    else
    {
        playerIdleSpriteLoaded = false;

        TraceLog(
            LOG_WARNING,
            "[PLAYER SPRITE] Idle sheet not found. "
            "Falling back to walking frame 0: "
            "Assets/player/player_idle.png"
        );
    }

}

bool Game::SaveLevel(const char* path) const
{
    std::filesystem::create_directories("levels");

    std::ofstream out(path);

    if (!out.is_open())
    {
        return false;
    }

    out << "MOXIANG_LEVEL 7\n";

    out << "TILES " << MapWidth << " " << MapHeight << "\n";

    for (int y = 0; y < MapHeight; ++y)
    {
        for (int x = 0; x < MapWidth; ++x)
        {
            out << tiles[CellIndex(x, y)] << " ";
        }

        out << "\n";
    }

    out << "TERRAIN_ELEVATIONS "
        << MapWidth << " "
        << MapHeight << " "
        << terrainElevationStep << "\n";

    for (int y = 0; y < MapHeight; ++y)
    {
        for (int x = 0; x < MapWidth; ++x)
        {
            out
                << GetTerrainElevation(x, y)
                << " ";
        }

        out << "\n";
    }

    out
        << "TERRAIN_RAMPS "
        << MapWidth << " "
        << MapHeight << "\n";

    for (int y = 0; y < MapHeight; ++y)
    {
        for (int x = 0; x < MapWidth; ++x)
        {
            out
                << static_cast<int>(
                    terrainCells[
                        CellIndex(
                            x,
                            y
                        )
                    ].rampDirection
                    )
                << " ";
        }

        out << "\n";
    }

    out
        << "WALL_TILES "
        << MapWidth
        << " "
        << MapHeight
        << "\n";

    for (int y = 0; y < MapHeight; ++y)
    {
        for (int x = 0; x < MapWidth; ++x)
        {
            const TerrainCell& cell =
                terrainCells[
                    CellIndex(
                        x,
                        y
                    )
                ];

            out
                << cell.northWallTile
                << " "
                << cell.eastWallTile
                << " "
                << cell.southWallTile
                << " "
                << cell.westWallTile
                << " ";
        }

        out << "\n";
    }

    out
        << "CHAMBERS "
        << chambers.size()
        << "\n";

    for (
        const DungeonChamber& chamber :
        chambers
        )
    {
        out
            << chamber.id
            << " "
            << std::quoted(
                chamber.name
            )
            << "\n";
    }

    out
        << "CELL_LAYOUT "
        << MapWidth
        << " "
        << MapHeight
        << "\n";

    for (int y = 0; y < MapHeight; ++y)
    {
        for (int x = 0; x < MapWidth; ++x)
        {
            const TerrainCell& cell =
                terrainCells[
                    CellIndex(x, y)
                ];

            out
                << (cell.enabled ? 1 : 0)
                << " "
                << cell.chamberId
                << " ";
        }

        out << "\n";
    }

    out
        << "TERRAIN_ATLAS "
        << std::quoted(
            ToPortableAssetPath(
                terrainTileSheetPath
            )
        )
        << " "
        << terrainTileSheetColumns
        << " "
        << terrainTileSheetRows
        << "\n";

    out
        << "TILE_WALKABILITY "
        << tileBrushes.size()
        << "\n";

    for (
        int index = 0;
        index <
        static_cast<int>(
            tileBrushes.size()
            );
        ++index
        )
    {
        out
            << index
            << " "
            << (
                tileBrushes[index].walkable
                ? 1
                : 0
                )
            << "\n";
    }

    out << "TILE_BRUSHES " << tileBrushes.size() << "\n";

    for (int i = 0; i < static_cast<int>(tileBrushes.size()); ++i)
    {
        const TileBrush& brush = tileBrushes[i];

        out
            << i << " "
            << std::quoted(
                ToPortableAssetPath(
                    brush.imagePath
                )
            ) << " "
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
            << obstacle.heightLevel << " "
            << obstacle.castsShadow << " "
            << obstacle.blocksLight << " "
            << obstacle.foreground << " "
            << std::quoted(
                ToPortableAssetPath(
                    obstacle.imagePath
                )
            ) << " "
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

/*

*/

void Game::StartPlayerAttackAnimation(
    const Enemy* target
)
{
    // Face the closest acquired enemy when one exists.
    // A null target means the player attacks in their
    // current facing direction.
    if (target != nullptr)
    {
        Vector2 worldDirection =
            Vector2Subtract(
                target->pos,
                playerPosition
            );

        if (
            Vector2Length(
                worldDirection
            ) >
            0.01f
            )
        {
            Vector2 viewDirection =
                WorldVectorToView(
                    worldDirection
                );

            if (
                Vector2Length(
                    viewDirection
                ) >
                0.01f
                )
            {
                viewDirection =
                    Vector2Normalize(
                        viewDirection
                    );

                playerDirection =
                    GetPlayerDirectionFromVector(
                        viewDirection
                    );

                lastPlayerDirection =
                    playerDirection;
            }
        }
    }
    else
    {
        // Attack in the last known direction when
        // there is no nearby enemy.
        playerDirection =
            lastPlayerDirection;
    }

    playerAnimationState =
        PlayerAnimationState::Attacking;

    playerAnimFrame = 0;
    playerAnimTimer = 0.0f;

    playerAttackImpactTriggered = false;
    playerAttackImpactPending = false;

    playerAttackTargetId =
        target != nullptr
        ? target->id
        : 0;

    // Do not clear:
    //
    // currentPath
    // hasPath
    // pendingNpc
    //
    // Attacking no longer replaces movement.
}

void Game::CancelPlayerAttackAnimation()
{
    if (
        playerAnimationState !=
        PlayerAnimationState::Attacking
        )
    {
        return;
    }

    // Cancel the animation immediately.
    playerAnimationState =
        PlayerAnimationState::Idle;

    playerAnimFrame = 0;
    playerAnimTimer = 0.0f;

    // Prevent a cancelled swing from applying damage
    // after the dash has already started.
    playerAttackImpactTriggered = false;
    playerAttackImpactPending = false;
    playerAttackTargetId = 0;

    // Do not reset player.attackTimer.
    // Cancelling an attack does not refund its cooldown.
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

    chambers.clear();

    bool loadedCellLayout =
        false;

    while (in >> tag)
    {
        if (tag == "TILES")
        {
            int savedWidth = 0;
            int savedHeight = 0;

            in
                >> savedWidth
                >> savedHeight;

            MapWidth =
                std::max(
                    1,
                    std::min(
                        MaximumMapDimension,
                        savedWidth
                    )
                );

            MapHeight =
                std::max(
                    1,
                    std::min(
                        MaximumMapDimension,
                        savedHeight
                    )
                );

            editorNewMapWidth = MapWidth;
            editorNewMapHeight = MapHeight;

            tiles.assign(
                MapWidth * MapHeight,
                static_cast<int>(
                    TileType::Grass
                    )
            );

            terrainCells.assign(
                MapWidth * MapHeight,
                TerrainCell{}
            );

            for (int y = 0; y < savedHeight; ++y)
            {
                for (int x = 0; x < savedWidth; ++x)
                {
                    int tile = 0;
                    in >> tile;

                    if (
                        x < MapWidth &&
                        y < MapHeight
                        )
                    {
                        tiles[
                            CellIndex(x, y)
                        ] = tile;
                    }
                }
            }
        }
        else if (tag == "TERRAIN_ELEVATIONS")
        {
            int width = 0;
            int height = 0;
            float savedElevationStep = terrainElevationStep;

            in
                >> width
                >> height;

            if (version >= 3)
            {
                in >> savedElevationStep;
                terrainElevationStep = savedElevationStep;
            }

            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    int elevation = 0;
                    in >> elevation;

                    if (x < MapWidth && y < MapHeight)
                    {
                        terrainCells[CellIndex(x, y)].elevation =
                            std::max(
                                0,
                                std::min(
                                    maxTerrainElevation,
                                    elevation
                                )
                            );
                    }
                }
            }
        }
        else if (tag == "TERRAIN_RAMPS")
        {
            int width = 0;
            int height = 0;

            in
                >> width
                >> height;

            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    int directionValue = 0;

                    in >> directionValue;

                    if (
                        x >= MapWidth ||
                        y >= MapHeight
                        )
                    {
                        continue;
                    }

                    directionValue =
                        std::max(
                            static_cast<int>(
                                RampDirection::None
                                ),
                            std::min(
                                static_cast<int>(
                                    RampDirection::West
                                    ),
                                directionValue
                            )
                        );

                    terrainCells[
                        CellIndex(
                            x,
                            y
                        )
                    ].rampDirection =
                        static_cast<RampDirection>(
                            directionValue
                            );
                }
            }
        }
        else if (tag == "WALL_TILES")
        {
            int width = 0;
            int height = 0;

            in
                >> width
                >> height;

            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    int northTile = -1;
                    int eastTile = -1;
                    int southTile = -1;
                    int westTile = -1;

                    in
                        >> northTile
                        >> eastTile
                        >> southTile
                        >> westTile;

                    if (
                        x >= MapWidth ||
                        y >= MapHeight
                        )
                    {
                        continue;
                    }

                    TerrainCell& cell =
                        terrainCells[
                            CellIndex(
                                x,
                                y
                            )
                        ];

                    cell.northWallTile =
                        northTile;

                    cell.eastWallTile =
                        eastTile;

                    cell.southWallTile =
                        southTile;

                    cell.westWallTile =
                        westTile;
                }
            }
            }

        else if (tag == "CHAMBERS")
        {
            int count = 0;
            in >> count;

            chambers.clear();

            for (int i = 0; i < count; ++i)
            {
                DungeonChamber chamber;

                in
                    >> chamber.id
                    >> std::quoted(
                        chamber.name
                    );

                chambers.push_back(
                    chamber
                );
            }
        }
        else if (tag == "CELL_LAYOUT")
        {
            int savedWidth = 0;
            int savedHeight = 0;

            in
                >> savedWidth
                >> savedHeight;

            for (int y = 0; y < savedHeight; ++y)
            {
                for (int x = 0; x < savedWidth; ++x)
                {
                    int enabledValue = 1;
                    int chamberId = 0;

                    in
                        >> enabledValue
                        >> chamberId;

                    if (
                        x >= MapWidth ||
                        y >= MapHeight
                        )
                    {
                        continue;
                    }

                    TerrainCell& cell =
                        terrainCells[
                            CellIndex(x, y)
                        ];

                    cell.enabled =
                        enabledValue != 0;

                    cell.chamberId =
                        chamberId;
                }
            }

            loadedCellLayout = true;
        }
        else if (tag == "TERRAIN_ATLAS")
        {
            std::string savedPath;

            int columns = 4;
            int rows = 4;

            in
                >> std::quoted(
                    savedPath
                )
                >> columns
                >> rows;

            const std::string portablePath =
                ToPortableAssetPath(
                    savedPath
                );

            const std::string loadPath =
                ResolveAssetPathForLoad(
                    savedPath
                );

            if (
                !loadPath.empty() &&
                LoadTerrainTileSheet(
                    loadPath,
                    columns,
                    rows
                )
                )
            {
                terrainTileSheetPath =
                    portablePath;

                std::snprintf(
                    terrainTileSheetPathInput,
                    sizeof(
                        terrainTileSheetPathInput
                        ),
                    "%s",
                    terrainTileSheetPath.c_str()
                );
            }
            else
            {
                TraceLog(
                    LOG_WARNING,
                    "[TERRAIN ATLAS] Could not load saved atlas: %s",
                    savedPath.c_str()
                );
            }
            }
        else if (tag == "TILE_WALKABILITY")
        {
            int count = 0;

            in >> count;

            for (
                int itemIndex = 0;
                itemIndex < count;
                ++itemIndex
                )
            {
                int tileIndex = -1;
                int walkableValue = 1;

                in
                    >> tileIndex
                    >> walkableValue;

                if (
                    tileIndex < 0 ||
                    tileIndex >=
                    static_cast<int>(
                        tileBrushes.size()
                        )
                    )
                {
                    continue;
                }

                tileBrushes[
                    tileIndex
                ].walkable =
                    walkableValue != 0;
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
                    if (
                        index >= 0 &&
                        index <
                        static_cast<int>(
                            tileBrushes.size()
                            )
                        )
                    {
                        const std::string portablePath =
                            ToPortableAssetPath(
                                imagePath
                            );

                        const std::string loadPath =
                            ResolveAssetPathForLoad(
                                imagePath
                            );

                        tileBrushes[index].imagePath =
                            portablePath;

                        tileBrushes[index].autoFitToTile =
                            autoFit != 0;

                        tileBrushes[index].fallbackColor =
                            Color{
                                static_cast<unsigned char>(r),
                                static_cast<unsigned char>(g),
                                static_cast<unsigned char>(b),
                                static_cast<unsigned char>(a)
                        };

                        if (!loadPath.empty())
                        {
                            if (LoadTileBrushTexture(
                                index,
                                loadPath
                            ))
                            {
                                // LoadTileBrushTexture stores the path it received.
                                // Replace it with the portable version afterward.
                                tileBrushes[index].imagePath =
                                    portablePath;
                            }
                        }
                    }
                    tileBrushes[index].fallbackColor = Color{
                        static_cast<unsigned char>(r),
                        static_cast<unsigned char>(g),
                        static_cast<unsigned char>(b),
                        static_cast<unsigned char>(a)
                    };

                }
            }
        }
        else if (tag == "OBSTACLES")
        {
            int count = 0;

            int castsShadow = 1;
            int blocksLight = 0;
            int foreground = 0;

            in >> count;

            obstacles.clear();

            for (int i = 0; i < count; ++i)
            {
                Obstacle obstacle;
                std::string imagePath;

                int collisionEnabled = 1;
                int colliderAuto = 1;
                int collisionShape = 0;
                if (version >= 2)
                {
                    in
                        >> obstacle.position.x
                        >> obstacle.position.y
                        >> obstacle.size.x
                        >> obstacle.size.y
                        >> obstacle.type
                        >> obstacle.heightLevel
                        >> castsShadow
                        >> blocksLight
                        >> foreground
                        >> std::quoted(imagePath)
                        >> collisionEnabled
                        >> colliderAuto
                        >> collisionShape
                        >> obstacle.colliderSize.x
                        >> obstacle.colliderSize.y
                        >> obstacle.colliderRadius;
                }
                else
                {
                    // Backward compatibility with older maps.
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

                    obstacle.heightLevel = 0;
                }
                const std::string portablePath =
                    ToPortableAssetPath(
                        imagePath
                    );

                const std::string loadPath =
                    ResolveAssetPathForLoad(
                        imagePath
                    );

                obstacle.imagePath =
                    portablePath;

                obstacle.collisionEnabled =
                    collisionEnabled != 0;

                obstacle.colliderAuto =
                    colliderAuto != 0;

                obstacle.castsShadow =
                    castsShadow != 0;

                obstacle.blocksLight =
                    blocksLight != 0;

                obstacle.foreground =
                    foreground != 0;

                obstacle.collisionShape =
                    static_cast<CollisionShape>(
                        collisionShape
                        );

                if (!loadPath.empty())
                {
                    if (LoadObstacleTexture(
                        obstacle,
                        loadPath
                    ))
                    {
                        obstacle.imagePath =
                            portablePath;
                    }
                }

                UpdateObstacleAutoCollider(
                    obstacle
                );

                obstacles.push_back(
                    obstacle
                );
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

    if (!loadedCellLayout)
    {
        // Version 4 and earlier receive the temporary west/east layout.
        InitializeTestChambers();
    }
    else
    {
        for (
            const TerrainCell& cell :
            terrainCells
            )
        {
            if (
                cell.enabled &&
                cell.chamberId >= 0
                )
            {
                EnsureChamberExists(
                    cell.chamberId
                );
            }
        }

        RebuildChamberBounds();
    }

    editorNewMapWidth = MapWidth;
    editorNewMapHeight = MapHeight;

    InvalidateGroundCache();

    return true;
}

bool Game::LoadTileBrushTexture(
    int tileIndex,
    const std::string& path
)
{
    if (
        tileIndex < 0 ||
        tileIndex >=
        static_cast<int>(
            tileBrushes.size()
            )
        )
    {
        return false;
    }

    if (
        path.empty() ||
        !FileExists(
            path.c_str()
        )
        )
    {
        TraceLog(
            LOG_WARNING,
            "[TILE TEXTURE] Missing brush=%d path='%s'. "
            "Keeping placeholder texture.",
            tileIndex,
            path.c_str()
        );

        return false;
    }

    Texture2D texture =
        LoadTexture(
            path.c_str()
        );

    if (texture.id == 0)
    {
        TraceLog(
            LOG_WARNING,
            "[TILE TEXTURE] Failed to load brush=%d path='%s'. "
            "Keeping placeholder texture.",
            tileIndex,
            path.c_str()
        );

        return false;
    }

    // Only unload the old texture after the replacement
    // has loaded successfully. This preserves placeholders
    // when an invalid file is selected.
    if (
        tileBrushes[tileIndex].hasTexture &&
        tileBrushes[tileIndex].texture.id != 0
        )
    {
        UnloadTexture(
            tileBrushes[tileIndex].texture
        );
    }

    tileBrushes[tileIndex].texture =
        texture;

    tileBrushes[tileIndex].hasTexture =
        true;

    tileBrushes[tileIndex].imagePath =
        ToPortableAssetPath(
            path
        );

    SetTextureFilter(
        tileBrushes[tileIndex].texture,
        TEXTURE_FILTER_POINT
    );

    InvalidateGroundCache();

    TraceLog(
        LOG_INFO,
        "[TILE TEXTURE] Loaded brush=%d "
        "texture=%u size=%dx%d path='%s'",
        tileIndex,
        texture.id,
        texture.width,
        texture.height,
        path.c_str()
    );

    return true;
}

bool Game::LoadTerrainTileSheet(
    const std::string& path,
    int columns,
    int rows
)
{
    if (
        path.empty() ||
        columns <= 0 ||
        rows <= 0
        )
    {
        return false;
    }

    const std::string loadPath =
        ResolveAssetPathForLoad(
            path
        );

    if (
        loadPath.empty() ||
        !FileExists(
            loadPath.c_str()
        )
        )
    {
        TraceLog(
            LOG_WARNING,
            "[TERRAIN ATLAS] File not found: %s",
            path.c_str()
        );

        return false;
    }

    Texture2D newTexture =
        LoadTexture(
            loadPath.c_str()
        );

    if (newTexture.id == 0)
    {
        TraceLog(
            LOG_WARNING,
            "[TERRAIN ATLAS] Failed to load: %s",
            loadPath.c_str()
        );

        return false;
    }

    if (
        newTexture.width < columns ||
        newTexture.height < rows
        )
    {
        UnloadTexture(
            newTexture
        );

        return false;
    }

    if (terrainTileSheet.id != 0)
    {
        UnloadTexture(
            terrainTileSheet
        );
    }

    terrainTileSheet =
        newTexture;

    terrainTileSheetPath =
        ToPortableAssetPath(
            path
        );

    terrainTileSheetColumns =
        std::max(
            1,
            columns
        );

    terrainTileSheetRows =
        std::max(
            1,
            rows
        );

    // This atlas is painted rather than pixel art.
    SetTextureFilter(
        terrainTileSheet,
        TEXTURE_FILTER_BILINEAR
    );

    ConfigureTileBrushesFromSheet();

    selectedTile =
        std::max(
            0,
            std::min(
                selectedTile,
                static_cast<int>(
                    tileBrushes.size()
                    ) - 1
            )
        );

    InvalidateGroundCache();
    MarkHybridTerrainDirty();

    TraceLog(
        LOG_INFO,
        "[TERRAIN ATLAS] Loaded %s | %dx%d | grid=%dx%d | tiles=%d",
        terrainTileSheetPath.c_str(),
        terrainTileSheet.width,
        terrainTileSheet.height,
        terrainTileSheetColumns,
        terrainTileSheetRows,
        terrainTileSheetColumns *
        terrainTileSheetRows
    );

    return true;
}

void Game::ConfigureTileBrushesFromSheet()
{
    if (
        terrainTileSheet.id == 0 ||
        terrainTileSheetColumns <= 0 ||
        terrainTileSheetRows <= 0
        )
    {
        return;
    }

    // Remove old standalone brush textures.
    for (TileBrush& brush : tileBrushes)
    {
        if (
            !brush.usesAtlas &&
            brush.hasTexture &&
            brush.texture.id != 0
            )
        {
            UnloadTexture(
                brush.texture
            );
        }
    }

    tileBrushes.clear();

    const float sourceWidth =
        static_cast<float>(
            terrainTileSheet.width
            ) /
        static_cast<float>(
            terrainTileSheetColumns
            );

    const float sourceHeight =
        static_cast<float>(
            terrainTileSheet.height
            ) /
        static_cast<float>(
            terrainTileSheetRows
            );

    const int tileCount =
        terrainTileSheetColumns *
        terrainTileSheetRows;

    tileBrushes.reserve(
        tileCount
    );

    for (int index = 0; index < tileCount; ++index)
    {
        const int column =
            index %
            terrainTileSheetColumns;

        const int row =
            index /
            terrainTileSheetColumns;

        TileBrush brush;

        brush.imagePath.clear();
        brush.texture = {};
        brush.hasTexture = false;
        brush.autoFitToTile = true;

        brush.usesAtlas = true;

        brush.source = {
            static_cast<float>(
                column
            ) *
                sourceWidth,

            static_cast<float>(
                row
            ) *
                sourceHeight,

            sourceWidth,
            sourceHeight
        };

        brush.walkable = true;

        brush.fallbackColor = {
            90,
            90,
            94,
            255
        };

        tileBrushes.push_back(
            brush
        );
    }
}

Texture2D Game::GetTileBrushTexture(
    int tileIndex
) const
{
    if (
        tileIndex < 0 ||
        tileIndex >=
        static_cast<int>(
            tileBrushes.size()
            )
        )
    {
        return {};
    }

    const TileBrush& brush =
        tileBrushes[
            tileIndex
        ];

    if (brush.usesAtlas)
    {
        return
            terrainTileSheet;
    }

    return
        brush.texture;
}

int Game::GetTerrainWallTileIndex(
    int cellX,
    int cellY,
    TerrainWallFace face
) const
{
    if (
        !IsCellInside(
            cellX,
            cellY
        )
        )
    {
        return 0;
    }

    const int cellIndex =
        CellIndex(
            cellX,
            cellY
        );

    const TerrainCell& cell =
        terrainCells[
            cellIndex
        ];

    int wallTileIndex = -1;

    switch (face)
    {
    case TerrainWallFace::North:
        wallTileIndex =
            cell.northWallTile;
        break;

    case TerrainWallFace::East:
        wallTileIndex =
            cell.eastWallTile;
        break;

    case TerrainWallFace::South:
        wallTileIndex =
            cell.southWallTile;
        break;

    case TerrainWallFace::West:
        wallTileIndex =
            cell.westWallTile;
        break;

    case TerrainWallFace::None:
    default:
        break;
    }

    if (
        wallTileIndex >= 0 &&
        wallTileIndex <
        static_cast<int>(
            tileBrushes.size()
            )
        )
    {
        return wallTileIndex;
    }

    int floorTileIndex =
        tiles[
            cellIndex
        ];

    if (
        floorTileIndex < 0 ||
        floorTileIndex >=
        static_cast<int>(
            tileBrushes.size()
            )
        )
    {
        floorTileIndex = 0;
    }

    return floorTileIndex;
}

Rectangle Game::GetTileBrushSourceRect(
    int tileIndex
) const
{
    if (
        tileIndex < 0 ||
        tileIndex >=
        static_cast<int>(
            tileBrushes.size()
            )
        )
    {
        return {};
    }

    const TileBrush& brush =
        tileBrushes[
            tileIndex
        ];

    if (brush.usesAtlas)
    {
        // Half-pixel inset prevents neighbouring atlas tiles
        // from bleeding into one another during filtering.
        constexpr float inset =
            0.5f;

        return Rectangle{
            brush.source.x +
                inset,

            brush.source.y +
                inset,

            std::max(
                1.0f,
                brush.source.width -
                inset *
                2.0f
            ),

            std::max(
                1.0f,
                brush.source.height -
                inset *
                2.0f
            )
        };
    }

    if (brush.texture.id == 0)
    {
        return {};
    }

    return Rectangle{
        0.0f,
        0.0f,
        static_cast<float>(
            brush.texture.width
        ),
        static_cast<float>(
            brush.texture.height
        )
    };
}

bool Game::LoadCurrentObstacleTexture(
    const std::string& path
)
{
    if (path.empty())
    {
        return false;
    }

    Texture2D texture =
        LoadTexture(path.c_str());

    if (texture.id == 0)
    {
        return false;
    }

    if (currentObstacleHasTexture)
    {
        UnloadTexture(
            currentObstacleTexture
        );
    }

    currentObstacleTexture = texture;
    currentObstacleHasTexture = true;
    currentObstacleImagePath = path;

    currentObstacleNativeSize = {
        static_cast<float>(texture.width),
        static_cast<float>(texture.height)
    };

    if (obstacleFitWidthToTiles)
    {
        float targetWidth =
            TileSize *
            obstacleVisualWidthTiles;

        float aspectRatio =
            static_cast<float>(texture.height) /
            static_cast<float>(texture.width);

        newObstacleSize = {
            targetWidth,
            targetWidth * aspectRatio
        };
    }
    else
    {
        newObstacleSize =
            currentObstacleNativeSize;
    }

    // Auto collider represents only the base/footprint,
    // not the full visible tree.
    if (newObstacleColliderAuto)
    {
        newObstacleColliderSize = {
            TileSize * 0.70f,
            TileSize * 0.50f
        };

        newObstacleColliderRadius =
            std::max(
                newObstacleColliderSize.x,
                newObstacleColliderSize.y
            ) * 0.5f;
    }

    TraceLog(
        LOG_INFO,
        "Loaded obstacle texture: %s | native=%dx%d visual=%.0fx%.0f",
        path.c_str(),
        texture.width,
        texture.height,
        newObstacleSize.x,
        newObstacleSize.y
    );

    return true;
}

std::string Game::ImportDroppedAssetToProject(
    const std::string& sourcePath,
    const std::string& assetCategory
)
{
    if (sourcePath.empty())
    {
        return {};
    }

#if defined(__EMSCRIPTEN__)

    // Browser-dropped files only exist temporarily in the browser's
    // virtual filesystem. They are not copied into the compiled Assets
    // directory and will disappear after a page refresh.
    TraceLog(
        LOG_WARNING,
        "[ASSET IMPORT] Browser drop is temporary: %s",
        sourcePath.c_str()
    );

    return sourcePath;

#else

    namespace fs = std::filesystem;

    std::error_code error;

    fs::path source = fs::path(sourcePath);

    if (!fs::exists(source, error) ||
        !fs::is_regular_file(source, error))
    {
        TraceLog(
            LOG_ERROR,
            "[ASSET IMPORT] Source file not found: %s",
            sourcePath.c_str()
        );

        return {};
    }

    fs::path targetDirectory =
        fs::path("Assets") /
        assetCategory;

    fs::create_directories(
        targetDirectory,
        error
    );

    if (error)
    {
        TraceLog(
            LOG_ERROR,
            "[ASSET IMPORT] Could not create folder: %s",
            targetDirectory.generic_string().c_str()
        );

        return {};
    }

    fs::path targetPath =
        targetDirectory /
        source.filename();

    fs::path sourceAbsolute =
        fs::absolute(source).lexically_normal();

    fs::path targetAbsolute =
        fs::absolute(targetPath).lexically_normal();

    // Do not copy when the file is already inside the destination.
    if (sourceAbsolute != targetAbsolute)
    {
        fs::copy_file(
            source,
            targetPath,
            fs::copy_options::overwrite_existing,
            error
        );

        if (error)
        {
            TraceLog(
                LOG_ERROR,
                "[ASSET IMPORT] Copy failed: %s -> %s",
                sourcePath.c_str(),
                targetPath.generic_string().c_str()
            );

            return {};
        }
    }

    // generic_string() uses forward slashes, which is suitable for web.
    std::string relativePath =
        targetPath.generic_string();

    TraceLog(
        LOG_INFO,
        "[ASSET IMPORT] Imported: %s -> %s",
        sourcePath.c_str(),
        relativePath.c_str()
    );

    return relativePath;

#endif
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

void Game::UpdatePerformanceStats(float dt)
{
    perfFrameMs = dt * 1000.0f;

    if (perfAverageFrameMs <= 0.0f)
    {
        perfAverageFrameMs = perfFrameMs;
    }
    else
    {
        perfAverageFrameMs = perfAverageFrameMs * 0.95f + perfFrameMs * 0.05f;
    }

    if (perfFrameMs > perfWorstFrameMs)
    {
        perfWorstFrameMs = perfFrameMs;
        perfWorstFrameTimer = 3.0f;
    }

    bool frameSpike = perfFrameMs >= 24.0f;
    bool updateSpike = perfUpdateTotalMs >= 8.0f;
    bool drawSpike = perfDrawTotalMs >= 8.0f;
    bool combatSpike = perfCombatMs >= 6.0f;

    if (frameSpike || updateSpike || drawSpike || combatSpike)
    {
        perfSpikeCount++;

        perfLastSpikeFrameMs = perfFrameMs;
        perfLastSpikeUpdateMs = perfUpdateTotalMs;
        perfLastSpikeDrawMs = perfDrawTotalMs;
        perfLastSpikeCombatMs = perfCombatMs;

        perfLastSpikeCDProjectilesMs = perfDrawCombatProjectilesMs;
        perfLastSpikeCDEnemiesMs = perfDrawCombatEnemiesMs;
        perfLastSpikeCDBladesMs = perfDrawCombatBladesMs;
        perfLastSpikeCDDongfengTelegraphMs = perfDrawCombatDongfengTelegraphMs;
        perfLastSpikeCDDongfengWaveMs = perfDrawCombatDongfengWaveMs;
        perfLastSpikeCDVfxMs = perfDrawCombatVfxMs;

        perfSpikeHoldTimer = 2.0f;

        perfLastSpikeReason = "Frame pacing / browser";

        if (perfDrawTotalMs >= perfUpdateTotalMs &&
            perfDrawTotalMs >= perfCombatMs &&
            perfDrawTotalMs >= 8.0f)
        {
            perfLastSpikeReason = "Draw";

            if (perfLastSpikeReason == std::string("Draw"))
            {
                perfLastSpikeDrawSection = "Unknown";

                float best = perfDrawGroundMs;

                perfLastSpikeDrawSection = "Ground";

                if (perfDrawObstaclesMs > best)
                {
                    best = perfDrawObstaclesMs;
                    perfLastSpikeDrawSection = "Obstacles";
                }

                if (perfDrawNpcsMs > best)
                {
                    best = perfDrawNpcsMs;
                    perfLastSpikeDrawSection = "NPCs";
                }

                if (perfDrawCombatWorldMs > best)
                {
                    best = perfDrawCombatWorldMs;
                    perfLastSpikeDrawSection = "CombatWorld";

                    if (perfLastSpikeDrawSection == std::string("CombatWorld"))
                    {
                        float bestCombatDraw = perfDrawCombatProjectilesMs;
                        perfLastSpikeCombatDrawSection = "Projectiles";

                        if (perfDrawCombatEnemiesMs > bestCombatDraw)
                        {
                            bestCombatDraw = perfDrawCombatEnemiesMs;
                            perfLastSpikeCombatDrawSection = "Enemies";
                        }

                        if (perfDrawCombatBladesMs > bestCombatDraw)
                        {
                            bestCombatDraw = perfDrawCombatBladesMs;
                            perfLastSpikeCombatDrawSection = "Blades";
                        }

                        if (perfDrawCombatDongfengTelegraphMs > bestCombatDraw)
                        {
                            bestCombatDraw = perfDrawCombatDongfengTelegraphMs;
                            perfLastSpikeCombatDrawSection = "DongfengTelegraph";
                        }

                        if (perfDrawCombatDongfengWaveMs > bestCombatDraw)
                        {
                            bestCombatDraw = perfDrawCombatDongfengWaveMs;
                            perfLastSpikeCombatDrawSection = "DongfengWave";
                        }

                        if (perfDrawCombatVfxMs > bestCombatDraw)
                        {
                            bestCombatDraw = perfDrawCombatVfxMs;
                            perfLastSpikeCombatDrawSection = "VFX";
                        }

                    }
                }

                if (perfRecordSpikes && perfSpikeRecordCooldown <= 0.0f)
                {
                    RecordPerformanceSpike();
                    perfSpikeRecordCooldown = 0.35f;
                }

                if (perfDrawPlayerMs > best)
                {
                    best = perfDrawPlayerMs;
                    perfLastSpikeDrawSection = "Player";
                }

                if (perfDrawUiMs > best)
                {
                    best = perfDrawUiMs;
                    perfLastSpikeDrawSection = "UI";
                }

                if (perfDrawHudMs > best)
                {
                    best = perfDrawHudMs;
                    perfLastSpikeDrawSection = "HUD";
                }

                if (perfDrawSkillUiMs > best)
                {
                    best = perfDrawSkillUiMs;
                    perfLastSpikeDrawSection = "SkillUI";
                }

                if (perfDrawControlsMs > best)
                {
                    best = perfDrawControlsMs;
                    perfLastSpikeDrawSection = "Controls";
                }

                if (perfDrawOverlayMs > best)
                {
                    best = perfDrawOverlayMs;
                    perfLastSpikeDrawSection = "PerfOverlay";
                }
            }

        }
        else if (perfCombatMs >= perfDrawTotalMs &&
            perfCombatMs >= perfUpdateTotalMs &&
            perfCombatMs >= 6.0f)
        {
            perfLastSpikeReason = "Combat";
        }
        else if (perfUpdateTotalMs >= 8.0f)
        {
            perfLastSpikeReason = "Update";
        }
    }

    if (perfWorstFrameTimer > 0.0f)
    {
        perfWorstFrameTimer -= dt;
    }
    else
    {
        perfWorstFrameMs *= 0.995f;

        if (perfWorstFrameMs < perfAverageFrameMs)
        {
            perfWorstFrameMs = perfAverageFrameMs;
        }
    }

    if (perfSpikeHoldTimer > 0.0f)
    {
        perfSpikeHoldTimer -= dt;
    }

    if (perfSpikeRecordCooldown > 0.0f)
    {
        perfSpikeRecordCooldown -= dt;
    }

}

void Game::Update(float dt)
{
    perfUpdateStartTime = GetTime();

    UpdatePerformanceStats(dt);

    if (playerDamageFlashTimer > 0.0f)
    {
        playerDamageFlashTimer -= dt;

        if (playerDamageFlashTimer < 0.0f)
        {
            playerDamageFlashTimer = 0.0f;
        }
    }

    UpdateHuashanImpactFeedback(dt);
    UpdateHuashanImpactAnimation(dt);

    // Keep the debug button responsive to window resizing.
    UpdateDebugUpgradeButtonRect();

    // Keyboard test controls.
    UpdateDebugControls();

    if (IsKeyPressed(KEY_F4))
    {
        showPerformanceOverlay =
            !showPerformanceOverlay;
    }

    if (IsKeyPressed(KEY_F5))
    {
        DumpPerformanceSpikes();
    }

    if (IsKeyPressed(KEY_F6))
    {
        rendererMode =
            rendererMode == WorldRendererMode::Hybrid3D
            ? WorldRendererMode::Legacy2D
            : WorldRendererMode::Hybrid3D;

        if (rendererMode == WorldRendererMode::Hybrid3D)
        {
            hybridCameraTargetWorld = playerPosition;
            MarkHybridTerrainDirty();
            UpdateHybridCamera(0.0f);
        }
        else
        {
            useIsometricView = true;
            InvalidateGroundCache();
            camera.target = WorldToViewElevated(playerPosition);
        }

        for (DungeonChamber& chamber : chambers)
        {
            chamber.visibility =
                chamber.targetVisibility;
        }

        TraceLog(
            LOG_WARNING,
            "[RENDERER] mode=%s",
            rendererMode == WorldRendererMode::Hybrid3D
            ? "Hybrid3D"
            : "Legacy2D"
        );
    }

    if (IsKeyPressed(KEY_F7))
    {
        CreateTestLights();

        TraceLog(
            LOG_INFO,
            "[LIGHTING] Random test lights regenerated."
        );
    }
#if MOXIANG_USE_IMGUI
    if (IsKeyPressed(KEY_R))
    {

        RestartGameplay();
    }

    if (IsKeyPressed(KEY_E))
    {
        GenerateSkillChoices();
    }
#endif

    if (rendererMode == WorldRendererMode::Hybrid3D)
    {
        if (hybridTerrainDirty)
        {
            RebuildHybridTerrain();
        }
    }
    else if (!groundCacheReady || groundCacheDirty)
    {
        BuildGroundCache();
    }

#if MOXIANG_USE_IMGUI
    if (IsKeyPressed(KEY_F2))
    {
        buildMode =
            !buildMode;
    }
#endif

    // B creates a Boss for animation and behaviour testing.
    if (
        !buildMode &&
        gameState ==
        GameState::Playing &&
        IsKeyPressed(KEY_B)
        )
    {
        SpawnTestBoss();
    }

#if MOXIANG_USE_IMGUI
    if (buildMode)
    {
        UpdateEditorCameraControls();
        HandleDroppedFiles();

        UpdateInput(dt);
        UpdatePlayer(dt);
        UpdateCamera(dt);

        return;
    }
#endif

    UpdateAttackButtonRect();
    UpdateDashButtonRect();
    UpdateSkillButtonRects();

    if (gameState == GameState::GameOver)
    {
        UpdateVfx(dt);
        UpdateCamera(dt);
        return;
    }

    if (gameState == GameState::ChoosingUpgrade)
    {
        // Recalculate the cards every frame so window resizing,
        // browser resizing and mobile orientation changes are handled.
        UpdateUpgradeChoiceLayout();

        UpdateInput(dt);
        UpdateVfx(dt);
        UpdateCamera(dt);
        return;
    }

    UpdateJoystick(dt);
    UpdateAttackButton();
    UpdateDashButton();

    UpdateInput(dt);
    UpdateDash(dt);
    UpdatePlayerKnockback(dt);

    if (playerKnockbackActive)
    {
        // Knockback owns player movement until it ends.
        currentPath.clear();
        pathIndex = 0;
        hasPath = false;
        pendingNpc = -1;

        joystickActive = false;

        joystickDirection = {
            0.0f,
            0.0f
        };
    }
    else if (dashActive)
    {
        // Dash movement is handled by UpdateDash().
        currentPath.clear();
        hasPath = false;
        pendingNpc = -1;
    }
    else if (
        IsPlayerMovementLocked()
        )
    {
        // Existing code continues...
        // Hard skill locks such as Huashan and Dongfeng
        // still prevent ordinary movement.
        currentPath.clear();
        hasPath = false;
        pendingNpc = -1;

        joystickActive = false;

        joystickDirection = {
            0.0f,
            0.0f
        };
    }
    else if (joystickActive)
    {
        // Movement remains available during normal attacks.
        MovePlayerWithJoystick(
            dt
        );
    }
    else
    {
        // Existing click path also remains active.
        UpdatePlayer(
            dt
        );
    }

    UpdatePlayerAnimation(dt);

    // Chamber membership is updated after every movement source:
    // path movement, joystick movement, dash and knockback.
    UpdateActiveChamber();
    UpdateChamberVisibility(dt);

    if (chamberClearedMessageTimer > 0.0f)
    {
        chamberClearedMessageTimer =
            std::max(
                0.0f,
                chamberClearedMessageTimer - dt
            );
    }

    player.pos = playerPosition;

    for (Light2D& light : lights)
    {
        if (light.followsPlayer)
        {
            light.position =
                playerPosition;
        }
    }

    UpdateCombat(dt);
    UpdateCamera(dt);


    perfUpdateTotalMs = static_cast<float>((GetTime() - perfUpdateStartTime) * 1000.0);
}

void Game::Draw()
{
    perfDrawStartTime =
        GetTime();

    perfDrawObstaclesMs = 0.0f;
    perfDrawNpcsMs = 0.0f;
    perfDrawPlayerMs = 0.0f;

    // Browser and desktop windows may be resized.
    EnsureLightingTargets();

    const bool useLighting =
        lightingReady &&
        sceneTarget.id != 0 &&
        lightTarget.id != 0 &&
        lightingShader.id != 0 &&
        lightingLightMapLocation >= 0;

    // --------------------------------------------------
    // PASS 1:
    // Draw the world.
    //
    // With lighting enabled, the world is drawn into
    // sceneTarget first.
    //
    // Without lighting, it is drawn directly to screen.
    // --------------------------------------------------

    if (useLighting)
    {
        BeginTextureMode(
            sceneTarget
        );

        ClearBackground(
            BLACK
        );
    }

    if (
        rendererMode ==
        WorldRendererMode::Hybrid3D
        )
    {
        perfBeginMode2DMs = 0.0f;
        perfEndMode2DMs = 0.0f;
        perfDrawWorldOverlayMs = 0.0f;

        PERF_DRAW_BLOCK(
            perfDrawGroundMs,
            {
                DrawHybridWorld3D();
            }
        );

        PERF_DRAW_BLOCK(
            perfDrawCombatWorldMs,
            {
                DrawHybridScreenOverlays2D();
            }
        );
    }
    else
    {
        const double beginModeStart =
            GetTime();

        BeginMode2D(
            camera
        );

        perfBeginMode2DMs =
            static_cast<float>(
                (
                    GetTime() -
                    beginModeStart
                    ) *
                1000.0
                );

        PERF_DRAW_BLOCK(
            perfDrawGroundMs,
            {
                DrawGround();
            }
        );

        PERF_DRAW_BLOCK(
            perfDrawCombatWorldMs,
            {
                DrawWorldGroundEffects();
                DrawWorldDepthSorted();
                DrawWorldForegroundEffects();
            }
        );

        PERF_DRAW_BLOCK(
            perfDrawWorldOverlayMs,
            {
                DrawEditorWorldOverlay();
            }
        );

        const double endModeStart =
            GetTime();

        EndMode2D();

        perfEndMode2DMs =
            static_cast<float>(
                (
                    GetTime() -
                    endModeStart
                    ) *
                1000.0
                );
    }

    if (useLighting)
    {
        EndTextureMode();
    }

    // --------------------------------------------------
    // PASS 2:
    // Generate the light map.
    // --------------------------------------------------

    if (useLighting)
    {
        DrawLightMap();

        // --------------------------------------------------
        // PASS 3:
        // Draw sceneTarget back to the actual screen and
        // multiply it by the generated light map.
        // --------------------------------------------------

        BeginShaderMode(
            lightingShader
        );

        SetShaderValueTexture(
            lightingShader,
            lightingLightMapLocation,
            lightTarget.texture
        );

        // Render textures are vertically inverted.
        Rectangle sceneSource{
            0.0f,
            0.0f,

            static_cast<float>(
                sceneTarget.texture.width
            ),

            -static_cast<float>(
                sceneTarget.texture.height
            )
        };

        DrawTextureRec(
            sceneTarget.texture,
            sceneSource,
            {
                0.0f,
                0.0f
            },
            WHITE
        );

        EndShaderMode();
    }

    // --------------------------------------------------
    // PASS 4:
    // Unlit Boss effects.
    //
    // This must happen after sceneTarget is composited,
    // otherwise the white rings will be affected by lighting
    // or covered by the scene texture.
    // --------------------------------------------------

    DrawBossUnlitEffects();

    // Full-screen Huashan flash is also unlit.
    DrawHuashanImpactFlash();

    // --------------------------------------------------
    // PASS 5:
    // UI and screen-space elements.
    // --------------------------------------------------

    PERF_DRAW_BLOCK(
        perfDrawUiMs,
        {
            DrawUi();
        }
    );

    PERF_DRAW_BLOCK(
        perfDrawHudMs,
        {
            DrawCombatHud();
        }
    );

    PERF_DRAW_BLOCK(
        perfDrawSkillUiMs,
        {
            DrawSkillUi();
        }
    );

    PERF_DRAW_BLOCK(
        perfDrawControlsMs,
        {
            DrawJoystick();
            DrawDashButton();
            DrawAttackButton();

            DrawDebugUpgradeButton();

        }
    );

    PERF_DRAW_BLOCK(
        perfDrawUpgradeChoicesMs,
        {
            if (
                gameState ==
                GameState::ChoosingUpgrade
            )
            {
                DrawUpgradeChoices();
            }
        }
    );

    PERF_DRAW_BLOCK(
        perfDrawDialogueMs,
        {
            DrawDialogue();
        }
    );

    PERF_DRAW_BLOCK(
        perfDrawGameOverMs,
        {
            if (
                gameState ==
                GameState::GameOver
            )
            {
                DrawRectangle(
                    0,
                    0,
                    GetScreenWidth(),
                    GetScreenHeight(),
                    Color{
                        0,
                        0,
                        0,
                        190
                    }
                );

                DrawText(
                    "GAME OVER",
                    GetScreenWidth() / 2 - 120,
                    GetScreenHeight() / 2 - 30,
                    48,
                    RED
                );
            }
        }
    );

    PERF_DRAW_BLOCK(
        perfDrawOverlayMs,
        {
            DrawPerformanceOverlay();
        }
    );

    perfDrawTotalMs =
        static_cast<float>(
            (
                GetTime() -
                perfDrawStartTime
                ) *
            1000.0
            );

    const float accounted =
        perfBeginMode2DMs +
        perfDrawGroundMs +
        perfDrawCombatWorldMs +
        perfDrawWorldOverlayMs +
        perfEndMode2DMs +
        perfDrawUiMs +
        perfDrawHudMs +
        perfDrawSkillUiMs +
        perfDrawControlsMs +
        perfDrawUpgradeChoicesMs +
        perfDrawDialogueMs +
        perfDrawGameOverMs +
        perfDrawOverlayMs;

    perfDrawUnaccountedMs =
        perfDrawTotalMs -
        accounted;

    if (
        perfDrawUnaccountedMs <
        0.0f
        )
    {
        perfDrawUnaccountedMs =
            0.0f;
    }
}

void Game::UpdateEditorCameraControls()
{
    if (rendererMode == WorldRendererMode::Hybrid3D)
    {
        UpdateHybridEditorCameraControls();
        return;
    }

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

    FilePathList droppedFiles =
        LoadDroppedFiles();

    if (droppedFiles.count == 0)
    {
        UnloadDroppedFiles(droppedFiles);
        return;
    }

    std::string sourcePath =
        droppedFiles.paths[0];

    auto importTile =
        [this](
            const std::string& path
            )
        {
            const std::string importedPath =
                ImportDroppedAssetToProject(
                    path,
                    "tiles"
                );

            if (importedPath.empty())
            {
                return false;
            }

            if (
                !LoadTerrainTileSheet(
                    importedPath,
                    terrainTileSheetColumns,
                    terrainTileSheetRows
                )
                )
            {
                return false;
            }

            std::snprintf(
                terrainTileSheetPathInput,
                sizeof(
                    terrainTileSheetPathInput
                    ),
                "%s",
                importedPath.c_str()
            );

            terrainTileSheetPath =
                importedPath;

            return true;
        };

    auto importObstacle =
        [this](
            const std::string& path
            )
        {
            std::string importedPath =
                ImportDroppedAssetToProject(
                    path,
                    "obstacles"
                );

            if (importedPath.empty())
            {
                return false;
            }

            if (!LoadCurrentObstacleTexture(
                importedPath
            ))
            {
                return false;
            }

            std::snprintf(
                obstacleImagePathInput,
                sizeof(obstacleImagePathInput),
                "%s",
                importedPath.c_str()
            );

            return true;
        };

    if (tileImageDropHovered)
    {
        importTile(sourcePath);
    }
    else if (obstacleImageDropHovered)
    {
        importObstacle(sourcePath);
    }
    else if (
        editorTool ==
        static_cast<int>(
            EditorTool::PaintTile
            )
        )
    {
        importTile(sourcePath);
    }
    else
    {
        importObstacle(sourcePath);
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

    if (gameState == GameState::ChoosingUpgrade)
    {
        TryChooseUpgradeAtScreen(clickScreenPosition);
        return;
    }

    if (
        CheckCollisionPointRec(
            clickScreenPosition,
            debugUpgradeButtonRect
        )
        )
    {
        OpenManualUpgradeMenu();
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

    if (TryActivateSkillAtScreen(clickScreenPosition))
    {
        return;
    }

    if (IsScreenPointOnCombatUi(clickScreenPosition))
    {
        return;
    }

    Vector2 worldPosition{};

    if (rendererMode == WorldRendererMode::Hybrid3D)
    {
        int terrainCellX = 0;
        int terrainCellY = 0;

        if (
            !ScreenToTerrainWorld3D(
                clickScreenPosition,
                worldPosition,
                &terrainCellX,
                &terrainCellY
            )
            )
        {
            return;
        }

        worldPosition =
            CellToWorld(
                terrainCellX,
                terrainCellY
            );
    }
    else
    {
        Vector2 viewPosition =
            GetScreenToWorld2D(
                clickScreenPosition,
                camera
            );

        worldPosition =
            ViewToWorld(viewPosition);

        int terrainCellX = 0;
        int terrainCellY = 0;

        if (
            ViewToTerrainCell(
                viewPosition,
                terrainCellX,
                terrainCellY
            )
            )
        {
            worldPosition =
                CellToWorld(
                    terrainCellX,
                    terrainCellY
                );
        }
    }

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

    const bool usingWallTool =
        editorTool ==
        static_cast<int>(
            EditorTool::PaintWall
            ) ||
        editorTool ==
        static_cast<int>(
            EditorTool::ClearWall
            );

    if (usingWallTool)
    {
        if (
            rendererMode !=
            WorldRendererMode::Hybrid3D
            )
        {
            return;
        }

        int wallCellX = -1;
        int wallCellY = -1;

        TerrainWallFace wallFace =
            TerrainWallFace::None;

        if (
            !ScreenToTerrainWall3D(
                screenPosition,
                wallCellX,
                wallCellY,
                wallFace
            )
            )
        {
            return;
        }

        if (
            !IsCellEnabled(
                wallCellX,
                wallCellY
            )
            )
        {
            return;
        }

        TerrainCell& wallCell =
            terrainCells[
                CellIndex(
                    wallCellX,
                    wallCellY
                )
            ];

        int* wallTileOverride =
            nullptr;

        switch (wallFace)
        {
        case TerrainWallFace::North:
            wallTileOverride =
                &wallCell.northWallTile;
            break;

        case TerrainWallFace::East:
            wallTileOverride =
                &wallCell.eastWallTile;
            break;

        case TerrainWallFace::South:
            wallTileOverride =
                &wallCell.southWallTile;
            break;

        case TerrainWallFace::West:
            wallTileOverride =
                &wallCell.westWallTile;
            break;

        case TerrainWallFace::None:
        default:
            break;
        }

        if (wallTileOverride == nullptr)
        {
            return;
        }

        if (
            editorTool ==
            static_cast<int>(
                EditorTool::PaintWall
                )
            )
        {
            if (
                selectedTile >= 0 &&
                selectedTile <
                static_cast<int>(
                    tileBrushes.size()
                    )
                )
            {
                *wallTileOverride =
                    selectedTile;
            }
        }
        else
        {
            // Return to inheriting the floor tile.
            *wallTileOverride =
                -1;
        }

        InvalidateGroundCache();

        return;
    }

    int pickedCellX = 0;
    int pickedCellY = 0;
    Vector2 worldPosition{};

    if (rendererMode == WorldRendererMode::Hybrid3D)
    {
        if (
            !ScreenToTerrainWorld3D(
                screenPosition,
                worldPosition,
                &pickedCellX,
                &pickedCellY
            )
            )
        {
            return;
        }
    }
    else
    {
        Vector2 viewPosition =
            GetScreenToWorld2D(
                screenPosition,
                camera
            );

        worldPosition =
            ViewToWorld(viewPosition);

        if (
            ViewToTerrainCell(
                viewPosition,
                pickedCellX,
                pickedCellY
            )
            )
        {
            worldPosition =
                CellToWorld(
                    pickedCellX,
                    pickedCellY
                );
        }
    }

    if (
        editorTool ==
        static_cast<int>(EditorTool::PaintTile)
        )
    {
        int cellX = 0;
        int cellY = 0;

        if (
            WorldToCell(
                worldPosition,
                cellX,
                cellY
            )
            )
        {
            const int index =
                CellIndex(
                    cellX,
                    cellY
                );

            TerrainCell& cell =
                terrainCells[index];

            bool changed = false;

            if (!cell.enabled)
            {
                cell.enabled = true;
                cell.chamberId =
                    editorSelectedChamberId;

                EnsureChamberExists(
                    editorSelectedChamberId
                );

                changed = true;
            }

            if (tiles[index] != selectedTile)
            {
                tiles[index] = selectedTile;
                changed = true;
            }

            if (changed)
            {
                RebuildChamberBounds();
                InvalidateGroundCache();
            }
        }
    }
    else if (
        editorTool ==
        static_cast<int>(EditorTool::PaintChamber)
        )
    {
        int cellX = 0;
        int cellY = 0;

        if (
            WorldToCell(
                worldPosition,
                cellX,
                cellY
            )
            )
        {
            TerrainCell& cell =
                terrainCells[
                    CellIndex(
                        cellX,
                        cellY
                    )
                ];

            const bool changed =
                !cell.enabled ||
                cell.chamberId !=
                editorSelectedChamberId;

            if (changed)
            {
                cell.enabled = true;
                cell.chamberId =
                    editorSelectedChamberId;

                EnsureChamberExists(
                    editorSelectedChamberId
                );

                RebuildChamberBounds();
                InvalidateGroundCache();
            }
        }
    }
    else if (
        editorTool ==
        static_cast<int>(EditorTool::EraseMapCell)
        )
    {
        int cellX = 0;
        int cellY = 0;

        if (
            WorldToCell(
                worldPosition,
                cellX,
                cellY
            )
            )
        {
            TerrainCell& cell =
                terrainCells[
                    CellIndex(
                        cellX,
                        cellY
                    )
                ];

            if (cell.enabled)
            {
                cell.enabled = false;
                cell.chamberId = -1;
                cell.northWallTile = -1;
                cell.eastWallTile = -1;
                cell.southWallTile = -1;
                cell.westWallTile = -1;

                cell.elevation = 0;
                cell.rampDirection =
                    RampDirection::None;

                RebuildChamberBounds();
                InvalidateGroundCache();

                currentPath.clear();
                pathIndex = 0;
                hasPath = false;
            }
        }
    }
    else if (
        editorTool ==
        static_cast<int>(EditorTool::RaiseTerrain)
        )
    {
        if (!pressed)
        {
            return;
        }

        int cellX = 0;
        int cellY = 0;

        if (
            WorldToCell(
                worldPosition,
                cellX,
                cellY
            ) &&
            IsCellEnabled(
                cellX,
                cellY
            )
            )
        {
            TerrainCell& cell =
                terrainCells[CellIndex(cellX, cellY)];

            cell.elevation =
                std::min(
                    maxTerrainElevation,
                    cell.elevation + 1
                );

            InvalidateGroundCache();
            currentPath.clear();
            hasPath = false;
        }
    }
    else if (
        editorTool ==
        static_cast<int>(EditorTool::LowerTerrain)
        )
    {
        if (!pressed)
        {
            return;
        }

        int cellX = 0;
        int cellY = 0;

        if (
            WorldToCell(
                worldPosition,
                cellX,
                cellY
            ) &&
            IsCellEnabled(
                cellX,
                cellY
            )
            )
        {
            TerrainCell& cell =
                terrainCells[CellIndex(cellX, cellY)];

            cell.elevation =
                std::max(
                    0,
                    cell.elevation - 1
                );

            InvalidateGroundCache();
            currentPath.clear();
            hasPath = false;
        }
    }
    else if (
        editorTool ==
        static_cast<int>(EditorTool::FlattenTerrain)
        )
    {
        if (!pressed)
        {
            return;
        }

        int cellX = 0;
        int cellY = 0;

        if (
            WorldToCell(
                worldPosition,
                cellX,
                cellY
            ) &&
            IsCellEnabled(
                cellX,
                cellY
            )
            )
        {
            TerrainCell& cell =
                terrainCells[CellIndex(cellX, cellY)];

            cell.elevation =
                std::max(
                    0,
                    std::min(
                        maxTerrainElevation,
                        editorFlattenElevation
                    )
                );

            InvalidateGroundCache();
            currentPath.clear();
            hasPath = false;
        }
    }
    else if (
        editorTool ==
        static_cast<int>(
            EditorTool::PlaceRamp
            )
        )
    {
        if (!pressed)
        {
            return;
        }

        int cellX = 0;
        int cellY = 0;

        if (
            !WorldToCell(
                worldPosition,
                cellX,
                cellY
            ) ||
            !IsCellEnabled(
                cellX,
                cellY
            )
            )
        {
            return;
        }

        RampDirection direction =
            static_cast<RampDirection>(
                editorRampDirection
                );

        int offsetX = 0;
        int offsetY = 0;

        if (
            !GetRampDirectionOffset(
                direction,
                offsetX,
                offsetY
            )
            )
        {
            TraceLog(
                LOG_WARNING,
                "[RAMP] Select a direction."
            );

            return;
        }

        int higherX =
            cellX +
            offsetX;

        int higherY =
            cellY +
            offsetY;

        if (
            !IsCellInside(
                higherX,
                higherY
            ) ||
            !IsCellEnabled(
                higherX,
                higherY
            )
            )
        {
            TraceLog(
                LOG_WARNING,
                "[RAMP] Destination is outside the map."
            );

            return;
        }

        int lowerElevation =
            GetTerrainElevation(
                cellX,
                cellY
            );

        int higherElevation =
            GetTerrainElevation(
                higherX,
                higherY
            );

        if (
            higherElevation !=
            lowerElevation + 1
            )
        {
            TraceLog(
                LOG_WARNING,
                "[RAMP] Invalid ramp: lower=%d higher=%d. "
                "The destination must be exactly one level higher.",
                lowerElevation,
                higherElevation
            );

            return;
        }

        terrainCells[
            CellIndex(
                cellX,
                cellY
            )
        ].rampDirection =
            direction;

            InvalidateGroundCache();

            currentPath.clear();
            hasPath = false;
    }
    else if (
        editorTool ==
        static_cast<int>(
            EditorTool::RemoveRamp
            )
        )
    {
        if (!pressed)
        {
            return;
        }

        int cellX = 0;
        int cellY = 0;

        if (
            WorldToCell(
                worldPosition,
                cellX,
                cellY
            ) &&
            IsCellEnabled(
                cellX,
                cellY
            )
            )
        {
            terrainCells[
                CellIndex(
                    cellX,
                    cellY
                )
            ].rampDirection =
                RampDirection::None;

                InvalidateGroundCache();

                currentPath.clear();
                hasPath = false;
        }
    }

    else if (editorTool == static_cast<int>(EditorTool::PlaceObstacle))
    {
        if (!pressed)
        {
            return;
        }

        Vector2 placePosition = worldPosition;

        int placementCellX = 0;
        int placementCellY = 0;

        if (
            !WorldToCell(
                worldPosition,
                placementCellX,
                placementCellY
            ) ||
            !IsCellEnabled(
                placementCellX,
                placementCellY
            )
            )
        {
            return;
        }

        if (obstacleSnapToGrid)
        {
            placePosition =
                CellToWorld(
                    placementCellX,
                    placementCellY
                );
        }

        Obstacle obstacle;

        obstacle.position =
            placePosition;

        obstacle.size =
            newObstacleSize;

        obstacle.type =
            selectedObstacleType;

        obstacle.heightLevel =
            editorHeightLevel;

        obstacle.castsShadow =
            newObstacleCastsShadow;

        obstacle.blocksLight =
            newObstacleBlocksLight;

        obstacle.foreground =
            newObstacleForeground;

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

        int obstacleIndex = GetObstacleAt(
            worldPosition,
            editorSelectActiveLevelOnly
            ? editorHeightLevel
            : -1
        );

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
            selectedObstacleIndex = GetObstacleAt(
                worldPosition,
                editorSelectActiveLevelOnly
                ? editorHeightLevel
                : -1
            );

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
        float distanceToNpc =
            Vector2Distance(
                playerPosition,
                npcs[
                    pendingNpc
                ].position
            );

        if (
            distanceToNpc <=
            interactDistance
            )
        {
            StartDialogue(
                pendingNpc
            );

            return;
        }
    }

    if (
        !hasPath ||
        pathIndex >=
        static_cast<int>(
            currentPath.size()
            )
        )
    {
        hasPath = false;
        return;
    }

    Vector2 target =
        currentPath[
            pathIndex
        ];

    const float step =
        GetCurrentPlayerMoveSpeed() *
        dt;

    Vector2 candidatePosition =
        MoveTowards(
            playerPosition,
            target,
            step
        );

    if (
        !CanPlayerStandAt(
            candidatePosition
        )
        )
    {
        currentPath.clear();
        pathIndex = 0;
        hasPath = false;
        return;
    }

    playerPosition =
        candidatePosition;

    if (
        Vector2Distance(
            playerPosition,
            target
        ) <
        2.0f
        )
    {
        pathIndex++;

        if (
            pathIndex >=
            static_cast<int>(
                currentPath.size()
                )
            )
        {
            hasPath = false;
        }
    }
}

void Game::UpdateCamera(
    float dt
)
{
    if (
        rendererMode ==
        WorldRendererMode::Hybrid3D
        )
    {
        UpdateHybridCamera(dt);
        return;
    }

    camera.offset = {
        static_cast<float>(
            GetScreenWidth()
        ) * 0.5f +
            huashanScreenShakeOffset.x,

        static_cast<float>(
            GetScreenHeight()
        ) * 0.5f +
            huashanScreenShakeOffset.y
    };

#if MOXIANG_USE_IMGUI
    if (buildMode)
    {
        return;
    }
#endif

    Vector2 desiredTarget =
        WorldToViewElevated(
            playerPosition
        );

    camera.target =
        Vector2Lerp(
            camera.target,
            desiredTarget,
            Clamp(
                11.0f * dt,
                0.0f,
                1.0f
            )
        );
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

int Game::GetObstacleAt(
    Vector2 worldPosition,
    int requiredHeightLevel
) const
{
    for (
        int i =
        static_cast<int>(
            obstacles.size()
            ) - 1;
            i >= 0;
            --i
        )
    {
        const Obstacle& obstacle =
            obstacles[i];

        if (
            requiredHeightLevel >= 0 &&
            obstacle.heightLevel !=
            requiredHeightLevel
            )
        {
            continue;
        }

        Rectangle footprint{
            obstacle.position.x -
                obstacle.colliderSize.x * 0.5f,

            obstacle.position.y -
                obstacle.colliderSize.y * 0.5f,

            obstacle.colliderSize.x,
            obstacle.colliderSize.y
        };

        if (
            CheckCollisionPointRec(
                worldPosition,
                footprint
            )
            )
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

int Game::GetTerrainElevation(
    int cellX,
    int cellY
) const
{
    if (
        !IsCellInside(
            cellX,
            cellY
        ) ||
        !IsCellEnabled(
            cellX,
            cellY
        )
        )
    {
        return 0;
    }

    const int index =
        CellIndex(
            cellX,
            cellY
        );

    if (
        index < 0 ||
        index >=
        static_cast<int>(
            terrainCells.size()
            )
        )
    {
        return 0;
    }

    return terrainCells[index].elevation;
}

int Game::GetTerrainElevationAtWorld(
    Vector2 worldPosition
) const
{
    int cellX = 0;
    int cellY = 0;

    if (!WorldToCell(worldPosition, cellX, cellY))
    {
        return 0;
    }

    return GetTerrainElevation(cellX, cellY);
}

float Game::GetTerrainSurfaceLevelAtWorld(
    Vector2 worldPosition
) const
{
    int cellX = 0;
    int cellY = 0;

    if (
        !WorldToCell(
            worldPosition,
            cellX,
            cellY
        )
        )
    {
        return 0.0f;
    }

    const int baseElevation =
        GetTerrainElevation(
            cellX,
            cellY
        );

    const TerrainCell& cell =
        terrainCells[
            CellIndex(
                cellX,
                cellY
            )
        ];

    int offsetX = 0;
    int offsetY = 0;

    if (
        !GetRampDirectionOffset(
            cell.rampDirection,
            offsetX,
            offsetY
        )
        )
    {
        return static_cast<float>(
            baseElevation
            );
    }

    const int higherX =
        cellX +
        offsetX;

    const int higherY =
        cellY +
        offsetY;

    if (
        !IsCellInside(
            higherX,
            higherY
        ) ||
        GetTerrainElevation(
            higherX,
            higherY
        ) !=
        baseElevation + 1
        )
    {
        return static_cast<float>(
            baseElevation
            );
    }

    const float originX =
        -static_cast<float>(
            MapWidth
            ) *
        TileSize *
        0.5f;

    const float originY =
        -static_cast<float>(
            MapHeight
            ) *
        TileSize *
        0.5f;

    const float cellMinX =
        originX +
        static_cast<float>(
            cellX
            ) *
        TileSize;

    const float cellMinY =
        originY +
        static_cast<float>(
            cellY
            ) *
        TileSize;

    const float localX =
        Clamp(
            (
                worldPosition.x -
                cellMinX
                ) /
            TileSize,
            0.0f,
            1.0f
        );

    const float localY =
        Clamp(
            (
                worldPosition.y -
                cellMinY
                ) /
            TileSize,
            0.0f,
            1.0f
        );

    float rampProgress = 0.0f;

    switch (cell.rampDirection)
    {
    case RampDirection::North:
        // North edge is the high edge.
        rampProgress =
            1.0f -
            localY;
        break;

    case RampDirection::East:
        // East edge is the high edge.
        rampProgress =
            localX;
        break;

    case RampDirection::South:
        // South edge is the high edge.
        rampProgress =
            localY;
        break;

    case RampDirection::West:
        // West edge is the high edge.
        rampProgress =
            1.0f -
            localX;
        break;

    case RampDirection::None:
    default:
        rampProgress = 0.0f;
        break;
    }

    return
        static_cast<float>(
            baseElevation
            ) +
        Clamp(
            rampProgress,
            0.0f,
            1.0f
        );
}

bool Game::CanTraverseTerrainEdge(
    int fromX,
    int fromY,
    int toX,
    int toY
) const
{
    if (
        !IsCellInside(
            fromX,
            fromY
        ) ||
        !IsCellInside(
            toX,
            toY
        ) ||
        !IsCellEnabled(
            fromX,
            fromY
        ) ||
        !IsCellEnabled(
            toX,
            toY
        )
        )
    {
        return false;
    }

    if (!buildMode)
    {
        const int fromChamberId =
            terrainCells[
                CellIndex(
                    fromX,
                    fromY
                )
            ].chamberId;

        const int toChamberId =
            terrainCells[
                CellIndex(
                    toX,
                    toY
                )
            ].chamberId;

        if (fromChamberId != toChamberId)
        {
            const DungeonChamber* activeChamber =
                FindChamberById(
                    activeChamberId
                );

            // The current chamber behaves as though its exits are sealed
            // until both encounter waves have been defeated.
            if (
                activeChamber != nullptr &&
                !activeChamber->cleared
                )
            {
                return false;
            }
        }
    }

    int deltaX =
        toX -
        fromX;

    int deltaY =
        toY -
        fromY;

    // Movement inside the same cell is always valid.
    if (
        deltaX == 0 &&
        deltaY == 0
        )
    {
        return true;
    }

    // Prevent large movement steps from skipping cells.
    if (
        std::abs(deltaX) > 1 ||
        std::abs(deltaY) > 1
        )
    {
        return false;
    }

    int fromElevation =
        GetTerrainElevation(
            fromX,
            fromY
        );

    int toElevation =
        GetTerrainElevation(
            toX,
            toY
        );

    // Normal movement across one flat terrain level.
    if (fromElevation == toElevation)
    {
        return true;
    }

    // Elevation changes must be cardinal, not diagonal.
    bool cardinalNeighbour =
        std::abs(deltaX) +
        std::abs(deltaY) ==
        1;

    if (!cardinalNeighbour)
    {
        return false;
    }

    // One ramp only connects adjacent elevations.
    if (
        std::abs(
            toElevation -
            fromElevation
        ) != 1
        )
    {
        return false;
    }

    int lowerX = fromX;
    int lowerY = fromY;

    int higherX = toX;
    int higherY = toY;

    if (fromElevation > toElevation)
    {
        lowerX = toX;
        lowerY = toY;

        higherX = fromX;
        higherY = fromY;
    }

    const TerrainCell& lowerCell =
        terrainCells[
            CellIndex(
                lowerX,
                lowerY
            )
        ];

    int offsetX = 0;
    int offsetY = 0;

    if (
        !GetRampDirectionOffset(
            lowerCell.rampDirection,
            offsetX,
            offsetY
        )
        )
    {
        return false;
    }

    return
        lowerX + offsetX == higherX &&
        lowerY + offsetY == higherY;
}

bool Game::IsTerrainCircleBlocked(
    Vector2 fromPosition,
    Vector2 candidatePosition,
    float radius
) const
{
    radius =
        std::max(
            1.0f,
            radius
        );

    int fromX = 0;
    int fromY = 0;

    int targetX = 0;
    int targetY = 0;

    if (
        !WorldToCell(
            fromPosition,
            fromX,
            fromY
        ) ||
        !WorldToCell(
            candidatePosition,
            targetX,
            targetY
        )
        )
    {
        return true;
    }

    auto IsTerrainCellWalkable =
        [this](
            int cellX,
            int cellY
            )
        {
            if (
                !IsCellInside(
                    cellX,
                    cellY
                ) ||
                !IsCellEnabled(
                    cellX,
                    cellY
                )
                )
            {
                return false;
            }

            return
                IsTileWalkable(
                    tiles[
                        CellIndex(
                            cellX,
                            cellY
                        )
                    ]
                );
        };

    auto IsValidRampFromCell =
        [this](
            int cellX,
            int cellY
            )
        {
            if (
                !IsCellInside(
                    cellX,
                    cellY
                )
                )
            {
                return false;
            }

            const TerrainCell& cell =
                terrainCells[
                    CellIndex(
                        cellX,
                        cellY
                    )
                ];

            int offsetX = 0;
            int offsetY = 0;

            if (
                !GetRampDirectionOffset(
                    cell.rampDirection,
                    offsetX,
                    offsetY
                )
                )
            {
                return false;
            }

            const int higherX =
                cellX +
                offsetX;

            const int higherY =
                cellY +
                offsetY;

            return
                IsCellInside(
                    higherX,
                    higherY
                ) &&
                GetTerrainElevation(
                    higherX,
                    higherY
                ) ==
                GetTerrainElevation(
                    cellX,
                    cellY
                ) +
                1;
        };

    auto IsRampNetworkCell =
        [&](
            int cellX,
            int cellY
            )
        {
            if (
                !IsCellInside(
                    cellX,
                    cellY
                )
                )
            {
                return false;
            }

            // This cell is the lower part of a ramp.
            if (
                IsValidRampFromCell(
                    cellX,
                    cellY
                )
                )
            {
                return true;
            }

            // Check whether this cell is the higher destination
            // of a neighbouring ramp.
            constexpr int directions[4][2]{
                { 1, 0 },
                { -1, 0 },
                { 0, 1 },
                { 0, -1 }
            };

            for (
                const auto& direction :
                directions
                )
            {
                const int neighbourX =
                    cellX +
                    direction[0];

                const int neighbourY =
                    cellY +
                    direction[1];

                if (
                    !IsValidRampFromCell(
                        neighbourX,
                        neighbourY
                    )
                    )
                {
                    continue;
                }

                int offsetX = 0;
                int offsetY = 0;

                GetRampDirectionOffset(
                    terrainCells[
                        CellIndex(
                            neighbourX,
                            neighbourY
                        )
                    ].rampDirection,
                    offsetX,
                            offsetY
                            );

                if (
                    neighbourX +
                    offsetX ==
                    cellX &&
                    neighbourY +
                    offsetY ==
                    cellY
                    )
                {
                    return true;
                }
            }

            return false;
        };

    // --------------------------------------------------
    // Check the movement centre in small increments.
    //
    // This prevents low frame rates from skipping over
    // a narrow ramp connection or terrain edge.
    // --------------------------------------------------

    const float travelDistance =
        Vector2Distance(
            fromPosition,
            candidatePosition
        );

    const float maximumSampleDistance =
        std::max(
            2.0f,
            std::min(
                playerCollisionSubstep,
                radius * 0.35f
            )
        );

    const int sampleCount =
        std::max(
            1,
            static_cast<int>(
                std::ceil(
                    travelDistance /
                    maximumSampleDistance
                )
                )
        );

    Vector2 previousSample =
        fromPosition;

    int previousCellX =
        fromX;

    int previousCellY =
        fromY;

    for (
        int sampleIndex = 1;
        sampleIndex <= sampleCount;
        ++sampleIndex
        )
    {
        const float t =
            static_cast<float>(
                sampleIndex
                ) /
            static_cast<float>(
                sampleCount
                );

        const Vector2 samplePosition =
            Vector2Lerp(
                fromPosition,
                candidatePosition,
                t
            );

        int sampleCellX = 0;
        int sampleCellY = 0;

        if (
            !WorldToCell(
                samplePosition,
                sampleCellX,
                sampleCellY
            )
            )
        {
            return true;
        }

        if (
            !IsTerrainCellWalkable(
                sampleCellX,
                sampleCellY
            )
            )
        {
            return true;
        }

        if (
            sampleCellX !=
            previousCellX ||
            sampleCellY !=
            previousCellY
            )
        {
            if (
                !CanTraverseTerrainEdge(
                    previousCellX,
                    previousCellY,
                    sampleCellX,
                    sampleCellY
                )
                )
            {
                const float previousSurface =
                    GetTerrainSurfaceLevelAtWorld(
                        previousSample
                    );

                const float nextSurface =
                    GetTerrainSurfaceLevelAtWorld(
                        samplePosition
                    );

                const float centreTolerance =
                    std::max(
                        0.12f,
                        maximumSampleDistance /
                        TileSize +
                        0.08f
                    );

                const bool belongsToRampNetwork =
                    IsRampNetworkCell(
                        previousCellX,
                        previousCellY
                    ) ||
                    IsRampNetworkCell(
                        sampleCellX,
                        sampleCellY
                    );

                // Adjacent ramps may cross a cell boundary that
                // is not a direct lower-to-upper ramp edge.
                // Allow it only when both actual surfaces meet.
                if (
                    !belongsToRampNetwork ||
                    std::fabs(
                        previousSurface -
                        nextSurface
                    ) >
                    centreTolerance
                    )
                {
                    return true;
                }
            }
        }

        previousSample =
            samplePosition;

        previousCellX =
            sampleCellX;

        previousCellY =
            sampleCellY;
    }

    // --------------------------------------------------
    // Map boundary
    // --------------------------------------------------

    const float originX =
        -static_cast<float>(
            MapWidth
            ) *
        TileSize *
        0.5f;

    const float originY =
        -static_cast<float>(
            MapHeight
            ) *
        TileSize *
        0.5f;

    const float mapMaxX =
        originX +
        static_cast<float>(
            MapWidth
            ) *
        TileSize;

    const float mapMaxY =
        originY +
        static_cast<float>(
            MapHeight
            ) *
        TileSize;

    if (
        candidatePosition.x -
        radius <
        originX ||
        candidatePosition.y -
        radius <
        originY ||
        candidatePosition.x +
        radius >
        mapMaxX ||
        candidatePosition.y +
        radius >
        mapMaxY
        )
    {
        return true;
    }

    // --------------------------------------------------
    // Check every terrain cell touched by the player circle.
    // --------------------------------------------------

    int minCellX =
        static_cast<int>(
            std::floor(
                (
                    candidatePosition.x -
                    radius -
                    originX
                    ) /
                TileSize
            )
            );

    int maxCellX =
        static_cast<int>(
            std::floor(
                (
                    candidatePosition.x +
                    radius -
                    originX
                    ) /
                TileSize
            )
            );

    int minCellY =
        static_cast<int>(
            std::floor(
                (
                    candidatePosition.y -
                    radius -
                    originY
                    ) /
                TileSize
            )
            );

    int maxCellY =
        static_cast<int>(
            std::floor(
                (
                    candidatePosition.y +
                    radius -
                    originY
                    ) /
                TileSize
            )
            );

    minCellX =
        std::max(
            0,
            minCellX
        );

    minCellY =
        std::max(
            0,
            minCellY
        );

    maxCellX =
        std::min(
            MapWidth - 1,
            maxCellX
        );

    maxCellY =
        std::min(
            MapHeight - 1,
            maxCellY
        );

    const float radiusSquared =
        radius *
        radius;

    const float candidateSurface =
        GetTerrainSurfaceLevelAtWorld(
            candidatePosition
        );

    // A circle standing on a ramp spans a small height range.
    // The allowed difference therefore depends on its radius.
    const float surfaceTolerance =
        Clamp(
            radius /
            TileSize +
            0.08f,
            0.18f,
            0.42f
        );

    constexpr float sampleInset =
        0.5f;

    for (
        int cellY = minCellY;
        cellY <= maxCellY;
        ++cellY
        )
    {
        for (
            int cellX = minCellX;
            cellX <= maxCellX;
            ++cellX
            )
        {
            if (
                cellX == targetX &&
                cellY == targetY
                )
            {
                continue;
            }

            Rectangle cellRectangle{
                originX +
                    static_cast<float>(
                        cellX
                    ) *
                    TileSize,

                originY +
                    static_cast<float>(
                        cellY
                    ) *
                    TileSize,

                TileSize,
                TileSize
            };

            const float closestX =
                Clamp(
                    candidatePosition.x,
                    cellRectangle.x,
                    cellRectangle.x +
                    cellRectangle.width
                );

            const float closestY =
                Clamp(
                    candidatePosition.y,
                    cellRectangle.y,
                    cellRectangle.y +
                    cellRectangle.height
                );

            const float differenceX =
                candidatePosition.x -
                closestX;

            const float differenceY =
                candidatePosition.y -
                closestY;

            const float distanceSquared =
                differenceX *
                differenceX +
                differenceY *
                differenceY;

            if (
                distanceSquared >=
                radiusSquared
                )
            {
                continue;
            }

            if (
                !IsTerrainCellWalkable(
                    cellX,
                    cellY
                )
                )
            {
                return true;
            }

            // Sample slightly inside the neighbouring cell.
            // This avoids WorldToCell selecting the wrong cell
            // when the closest point lies exactly on an edge.
            Vector2 neighbouringSample{
                Clamp(
                    candidatePosition.x,
                    cellRectangle.x +
                        sampleInset,
                    cellRectangle.x +
                        cellRectangle.width -
                        sampleInset
                ),

                Clamp(
                    candidatePosition.y,
                    cellRectangle.y +
                        sampleInset,
                    cellRectangle.y +
                        cellRectangle.height -
                        sampleInset
                )
            };

            const float neighbouringSurface =
                GetTerrainSurfaceLevelAtWorld(
                    neighbouringSample
                );

            const float surfaceDifference =
                std::fabs(
                    neighbouringSurface -
                    candidateSurface
                );

            if (
                surfaceDifference <=
                surfaceTolerance
                )
            {
                // Flat ground, a normal ramp endpoint,
                // or another ramp that continues the slope.
                continue;
            }

            const bool rampNetworkOverlap =
                IsRampNetworkCell(
                    targetX,
                    targetY
                ) &&
                IsRampNetworkCell(
                    cellX,
                    cellY
                );

            if (
                rampNetworkOverlap &&
                surfaceDifference <=
                surfaceTolerance +
                0.08f
                )
            {
                // Slightly more tolerance between two ramps
                // placed side by side.
                continue;
            }

            return true;
        }
    }

    return false;
}

float Game::GetTerrainHeightAtWorld(
    Vector2 worldPosition
) const
{
    return
        GetTerrainSurfaceLevelAtWorld(
            worldPosition
        ) *
        terrainElevationStep;
}

Vector2 Game::WorldToViewElevated(
    Vector2 worldPosition,
    float additionalHeight
) const
{
    if (rendererMode == WorldRendererMode::Hybrid3D)
    {
        return GetWorldToScreen(
            WorldToHybrid3D(
                worldPosition,
                additionalHeight
            ),
            hybridCamera
        );
    }

    Vector2 viewPosition =
        WorldToView(worldPosition);

    // The F6 top-down view is a flat debugging view.
    if (!useIsometricView)
    {
        return viewPosition;
    }

    viewPosition.y -=
        GetTerrainHeightAtWorld(worldPosition) +
        additionalHeight;

    return viewPosition;
}

bool Game::ViewToTerrainCell(
    Vector2 viewPosition,
    int& cellX,
    int& cellY
) const
{
    if (!useIsometricView)
    {
        return
            WorldToCell(
                ViewToWorld(viewPosition),
                cellX,
                cellY
            );
    }

    float originX =
        -static_cast<float>(MapWidth) *
        TileSize *
        0.5f;

    float originY =
        -static_cast<float>(MapHeight) *
        TileSize *
        0.5f;

    // Search front-to-back so the visible elevated tile wins.
    for (
        int depth =
        MapWidth + MapHeight - 2;
        depth >= 0;
        --depth
        )
    {
        for (int y = 0; y < MapHeight; ++y)
        {
            int x = depth - y;

            if (!IsCellInside(x, y))
            {
                continue;
            }

            float x0 = originX + x * TileSize;
            float y0 = originY + y * TileSize;
            float x1 = x0 + TileSize;
            float y1 = y0 + TileSize;

            float height =
                static_cast<float>(
                    GetTerrainElevation(x, y)
                    ) *
                terrainElevationStep;

            Vector2 top = WorldToView({ x0, y0 });
            Vector2 right = WorldToView({ x1, y0 });
            Vector2 bottom = WorldToView({ x1, y1 });
            Vector2 left = WorldToView({ x0, y1 });

            top.y -= height;
            right.y -= height;
            bottom.y -= height;
            left.y -= height;

            bool inside =
                CheckCollisionPointTriangle(
                    viewPosition,
                    top,
                    right,
                    bottom
                ) ||
                CheckCollisionPointTriangle(
                    viewPosition,
                    top,
                    bottom,
                    left
                );

            if (inside)
            {
                cellX = x;
                cellY = y;
                return true;
            }
        }
    }

    return false;
}

void Game::DrawTerrainCellOutline(
    int cellX,
    int cellY,
    Color color,
    float thickness
) const
{
    if (!IsCellInside(cellX, cellY))
    {
        return;
    }

    float originX =
        -static_cast<float>(MapWidth) *
        TileSize *
        0.5f;

    float originY =
        -static_cast<float>(MapHeight) *
        TileSize *
        0.5f;

    float x0 = originX + cellX * TileSize;
    float y0 = originY + cellY * TileSize;
    float x1 = x0 + TileSize;
    float y1 = y0 + TileSize;

    float height =
        static_cast<float>(
            GetTerrainElevation(cellX, cellY)
            ) *
        terrainElevationStep;

    Vector2 p0 = WorldToView({ x0, y0 });
    Vector2 p1 = WorldToView({ x1, y0 });
    Vector2 p2 = WorldToView({ x1, y1 });
    Vector2 p3 = WorldToView({ x0, y1 });

    p0.y -= height;
    p1.y -= height;
    p2.y -= height;
    p3.y -= height;

    DrawLineEx(p0, p1, thickness, color);
    DrawLineEx(p1, p2, thickness, color);
    DrawLineEx(p2, p3, thickness, color);
    DrawLineEx(p3, p0, thickness, color);
}

void Game::GetTerrainSurfacePointsView(
    int cellX,
    int cellY,
    int elevation,
    Vector2& top,
    Vector2& right,
    Vector2& bottom,
    Vector2& left
) const
{
    float originX =
        -static_cast<float>(
            MapWidth
            ) *
        TileSize *
        0.5f;

    float originY =
        -static_cast<float>(
            MapHeight
            ) *
        TileSize *
        0.5f;

    float x0 =
        originX +
        static_cast<float>(
            cellX
            ) *
        TileSize;

    float y0 =
        originY +
        static_cast<float>(
            cellY
            ) *
        TileSize;

    float x1 =
        x0 +
        TileSize;

    float y1 =
        y0 +
        TileSize;

    top =
        WorldToView({
            x0,
            y0
            });

    right =
        WorldToView({
            x1,
            y0
            });

    bottom =
        WorldToView({
            x1,
            y1
            });

    left =
        WorldToView({
            x0,
            y1
            });

    float elevationOffset =
        static_cast<float>(
            elevation
            ) *
        terrainElevationStep;

    top.y -=
        elevationOffset;

    right.y -=
        elevationOffset;

    bottom.y -=
        elevationOffset;

    left.y -=
        elevationOffset;
}

bool Game::IsRampConnectionBetweenCells(
    int cellAX,
    int cellAY,
    int cellBX,
    int cellBY
) const
{
    if (
        !IsCellInside(cellAX, cellAY) ||
        !IsCellInside(cellBX, cellBY) ||
        !IsCellEnabled(cellAX, cellAY) ||
        !IsCellEnabled(cellBX, cellBY)
        )
    {
        return false;
    }

    int elevationA =
        GetTerrainElevation(
            cellAX,
            cellAY
        );

    int elevationB =
        GetTerrainElevation(
            cellBX,
            cellBY
        );

    if (
        std::abs(
            elevationA -
            elevationB
        ) != 1
        )
    {
        return false;
    }

    int lowerX = cellAX;
    int lowerY = cellAY;

    int higherX = cellBX;
    int higherY = cellBY;

    if (elevationA > elevationB)
    {
        lowerX = cellBX;
        lowerY = cellBY;

        higherX = cellAX;
        higherY = cellAY;
    }

    const TerrainCell& lowerCell =
        terrainCells[
            CellIndex(
                lowerX,
                lowerY
            )
        ];

    int offsetX = 0;
    int offsetY = 0;

    if (
        !GetRampDirectionOffset(
            lowerCell.rampDirection,
            offsetX,
            offsetY
        )
        )
    {
        return false;
    }

    return
        lowerX + offsetX == higherX &&
        lowerY + offsetY == higherY;
}

float Game::GetTerrainCliffDepth(
    int cellX,
    int cellY,
    TerrainCliffFace face
) const
{
    if (
        !IsCellInside(
            cellX,
            cellY
        )
        )
    {
        return
            -std::numeric_limits<float>::
            infinity();
    }

    Vector2 edgePosition =
        CellToWorld(
            cellX,
            cellY
        );

    switch (face)
    {
    case TerrainCliffFace::North:
        edgePosition.y -=
            TileSize *
            0.5f;
        break;

    case TerrainCliffFace::East:
        edgePosition.x +=
            TileSize *
            0.5f;
        break;

    case TerrainCliffFace::South:
        edgePosition.y +=
            TileSize *
            0.5f;
        break;

    case TerrainCliffFace::West:
        edgePosition.x -=
            TileSize *
            0.5f;
        break;
    }

    return
        GetWorldDepth(
            edgePosition
        );
}

void Game::DrawTerrainCliffSegment(
    int cellX,
    int cellY,
    TerrainCliffFace face,
    int segmentLevel
) const
{
    if (
        !IsCellInside(
            cellX,
            cellY
        ) ||
        !IsCellEnabled(
            cellX,
            cellY
        )
        )
    {
        return;
    }

    const int cellElevation =
        GetTerrainElevation(
            cellX,
            cellY
        );

    int neighbourX =
        cellX;

    int neighbourY =
        cellY;

    TerrainWallFace wallFace =
        TerrainWallFace::None;

    Color wallTint{
        205,
        205,
        205,
        255
    };

    switch (face)
    {
    case TerrainCliffFace::North:
        neighbourY -= 1;

        wallFace =
            TerrainWallFace::North;

        wallTint = {
            164,
            164,
            170,
            255
        };
        break;

    case TerrainCliffFace::East:
        neighbourX += 1;

        wallFace =
            TerrainWallFace::East;

        wallTint = {
            178,
            178,
            184,
            255
        };
        break;

    case TerrainCliffFace::South:
        neighbourY += 1;

        wallFace =
            TerrainWallFace::South;

        wallTint = {
            216,
            216,
            222,
            255
        };
        break;

    case TerrainCliffFace::West:
        neighbourX -= 1;

        wallFace =
            TerrainWallFace::West;

        wallTint = {
            192,
            192,
            198,
            255
        };
        break;
    }

    const int neighbourElevation =
        GetTerrainElevation(
            neighbourX,
            neighbourY
        );

    if (
        segmentLevel <
        neighbourElevation ||
        segmentLevel >=
        cellElevation
        )
    {
        return;
    }

    if (
        IsRampConnectionBetweenCells(
            cellX,
            cellY,
            neighbourX,
            neighbourY
        )
        )
    {
        return;
    }

    Vector2 upperTop{};
    Vector2 upperRight{};
    Vector2 upperBottom{};
    Vector2 upperLeft{};

    Vector2 lowerTop{};
    Vector2 lowerRight{};
    Vector2 lowerBottom{};
    Vector2 lowerLeft{};

    GetTerrainSurfacePointsView(
        cellX,
        cellY,
        segmentLevel + 1,
        upperTop,
        upperRight,
        upperBottom,
        upperLeft
    );

    GetTerrainSurfacePointsView(
        cellX,
        cellY,
        segmentLevel,
        lowerTop,
        lowerRight,
        lowerBottom,
        lowerLeft
    );

    Vector2 upperStart{};
    Vector2 upperEnd{};
    Vector2 lowerEnd{};
    Vector2 lowerStart{};

    switch (face)
    {
    case TerrainCliffFace::North:
        upperStart =
            upperTop;

        upperEnd =
            upperRight;

        lowerEnd =
            lowerRight;

        lowerStart =
            lowerTop;
        break;

    case TerrainCliffFace::East:
        upperStart =
            upperRight;

        upperEnd =
            upperBottom;

        lowerEnd =
            lowerBottom;

        lowerStart =
            lowerRight;
        break;

    case TerrainCliffFace::South:
        upperStart =
            upperLeft;

        upperEnd =
            upperBottom;

        lowerEnd =
            lowerBottom;

        lowerStart =
            lowerLeft;
        break;

    case TerrainCliffFace::West:
        upperStart =
            upperTop;

        upperEnd =
            upperLeft;

        lowerEnd =
            lowerLeft;

        lowerStart =
            lowerTop;
        break;
    }

    const int wallTileIndex =
        GetTerrainWallTileIndex(
            cellX,
            cellY,
            wallFace
        );

    Texture2D wallTexture =
        GetTileBrushTexture(
            wallTileIndex
        );

    const Rectangle source =
        GetTileBrushSourceRect(
            wallTileIndex
        );

    if (wallTexture.id != 0)
    {
        // Keep the atlas facing consistently from the
        // screen-left side toward the screen-right side.
        const bool flipHorizontal =
            upperEnd.x <
            upperStart.x;

        DrawTextureFrameOnWallQuad2D(
            wallTexture,
            source,
            upperStart,
            upperEnd,
            lowerEnd,
            lowerStart,
            wallTint,
            flipHorizontal
        );

        return;
    }

    Color fallback{
        80,
        80,
        84,
        255
    };

    if (
        wallTileIndex >= 0 &&
        wallTileIndex <
        static_cast<int>(
            tileBrushes.size()
            )
        )
    {
        fallback =
            tileBrushes[
                wallTileIndex
            ].fallbackColor;
    }

    fallback.r =
        static_cast<unsigned char>(
            static_cast<float>(
                fallback.r
                ) *
            (
                static_cast<float>(
                    wallTint.r
                    ) /
                255.0f
                )
            );

    fallback.g =
        static_cast<unsigned char>(
            static_cast<float>(
                fallback.g
                ) *
            (
                static_cast<float>(
                    wallTint.g
                    ) /
                255.0f
                )
            );

    fallback.b =
        static_cast<unsigned char>(
            static_cast<float>(
                fallback.b
                ) *
            (
                static_cast<float>(
                    wallTint.b
                    ) /
                255.0f
                )
            );

    DrawTriangle(
        upperStart,
        lowerStart,
        lowerEnd,
        fallback
    );

    DrawTriangle(
        upperStart,
        lowerEnd,
        upperEnd,
        fallback
    );
}

void Game::DrawTerrainCliffFace(
    int cellX,
    int cellY,
    TerrainCliffFace face
) const
{
    if (
        !IsCellInside(
            cellX,
            cellY
        ) ||
        !IsCellEnabled(
            cellX,
            cellY
        )
        )
    {
        return;
    }

    int elevation =
        GetTerrainElevation(
            cellX,
            cellY
        );

    if (elevation <= 0)
    {
        return;
    }

    Vector2 top{};
    Vector2 right{};
    Vector2 bottom{};
    Vector2 left{};

    GetTerrainSurfacePointsView(
        cellX,
        cellY,
        elevation,
        top,
        right,
        bottom,
        left
    );

    int neighbourX =
        cellX;

    int neighbourY =
        cellY;

    if (
        face ==
        TerrainCliffFace::East
        )
    {
        neighbourX += 1;
    }
    else
    {
        neighbourY += 1;
    }

    int neighbourElevation =
        GetTerrainElevation(
            neighbourX,
            neighbourY
        );

    if (
        elevation <=
        neighbourElevation
        )
    {
        return;
    }

    if (
        IsRampConnectionBetweenCells(
            cellX,
            cellY,
            neighbourX,
            neighbourY
        )
        )
    {
        return;
    }

    float wallHeight =
        static_cast<float>(
            elevation -
            neighbourElevation
            ) *
        terrainElevationStep;

    int tileIndex =
        tiles[
            CellIndex(
                cellX,
                cellY
            )
        ];

    Color southColor{
        105,
        78,
        50,
        255
    };

    Color eastColor{
        70,
        61,
        48,
        255
    };

    if (
        tileIndex ==
        static_cast<int>(
            TileType::Stone
            )
        )
    {
        southColor =
            Color{
                110,
                112,
                118,
                255
        };

        eastColor =
            Color{
                76,
                80,
                90,
                255
        };
    }
    else if (
        tileIndex ==
        static_cast<int>(
            TileType::Water
            )
        )
    {
        southColor =
            Color{
                46,
                76,
                112,
                255
        };

        eastColor =
            Color{
                32,
                54,
                88,
                255
        };
    }

    if (
        face ==
        TerrainCliffFace::East
        )
    {
        Vector2 lowerRight{
            right.x,
            right.y +
                wallHeight
        };

        Vector2 lowerBottom{
            bottom.x,
            bottom.y +
                wallHeight
        };

        DrawTriangle(
            bottom,
            lowerBottom,
            lowerRight,
            eastColor
        );

        DrawTriangle(
            bottom,
            lowerRight,
            right,
            eastColor
        );

        // Top lip helps clip the character cleanly.

    }
    else
    {
        Vector2 lowerLeft{
            left.x,
            left.y +
                wallHeight
        };

        Vector2 lowerBottom{
            bottom.x,
            bottom.y +
                wallHeight
        };

        DrawTriangle(
            left,
            lowerLeft,
            lowerBottom,
            southColor
        );

        DrawTriangle(
            left,
            lowerBottom,
            bottom,
            southColor
        );


    }
}

void Game::DrawTerrainTopSurface(
    int cellX,
    int cellY
) const
{
    if (
        !IsCellInside(
            cellX,
            cellY
        ) ||
        !IsCellEnabled(
            cellX,
            cellY
        ) ||
        tileBrushes.empty()
        )
    {
        return;
    }

    int elevation =
        GetTerrainElevation(
            cellX,
            cellY
        );

    if (elevation <= 0)
    {
        return;
    }

    Vector2 top{};
    Vector2 right{};
    Vector2 bottom{};
    Vector2 left{};

    GetTerrainSurfacePointsView(
        cellX,
        cellY,
        elevation,
        top,
        right,
        bottom,
        left
    );

    int tileIndex =
        tiles[
            CellIndex(
                cellX,
                cellY
            )
        ];

    if (
        tileIndex < 0 ||
        tileIndex >=
        static_cast<int>(
            tileBrushes.size()
            )
        )
    {
        tileIndex =
            static_cast<int>(
                TileType::Grass
                );
    }

    const TileBrush& brush =
        tileBrushes[
            tileIndex
        ];

    Texture2D brushTexture =
        GetTileBrushTexture(
            tileIndex
        );

    if (brushTexture.id != 0)
    {
        DrawTextureFrameOnQuad2D(
            brushTexture,
            GetTileBrushSourceRect(
                tileIndex
            ),
            top,
            right,
            bottom,
            left,
            WHITE
        );
    }
    else
    {
        Color fallback =
            brush.fallbackColor;

        fallback.a = 255;

        DrawTriangle(
            top,
            right,
            bottom,
            fallback
        );

        DrawTriangle(
            top,
            bottom,
            left,
            fallback
        );
    }

    // Keep only subtle north/west contour lines.
    // These help same-material elevations remain readable
    // without producing heavy black lines on cliff faces.
    Color contourColor{
        10,
        10,
        10,
        38
    };

    constexpr float contourThickness =
        1.0f;

    int northElevation =
        GetTerrainElevation(
            cellX,
            cellY - 1
        );

    if (
        northElevation < elevation &&
        !IsRampConnectionBetweenCells(
            cellX,
            cellY,
            cellX,
            cellY - 1
        )
        )
    {
        DrawLineEx(
            top,
            right,
            contourThickness,
            contourColor
        );
    }

    int westElevation =
        GetTerrainElevation(
            cellX - 1,
            cellY
        );

    if (
        westElevation < elevation &&
        !IsRampConnectionBetweenCells(
            cellX,
            cellY,
            cellX - 1,
            cellY
        )
        )
    {
        DrawLineEx(
            left,
            top,
            contourThickness,
            contourColor
        );
    }
}

void Game::DrawTerrainRampSurface(
    int cellX,
    int cellY
) const
{
    if (
        !IsCellInside(
            cellX,
            cellY
        ) ||
        !IsCellEnabled(
            cellX,
            cellY
        ) ||
        tileBrushes.empty()
        )
    {
        return;
    }

    const TerrainCell& cell =
        terrainCells[
            CellIndex(
                cellX,
                cellY
            )
        ];

    int offsetX = 0;
    int offsetY = 0;

    if (
        !GetRampDirectionOffset(
            cell.rampDirection,
            offsetX,
            offsetY
        )
        )
    {
        return;
    }

    int targetX =
        cellX +
        offsetX;

    int targetY =
        cellY +
        offsetY;

    if (
        !IsCellInside(
            targetX,
            targetY
        )
        )
    {
        return;
    }

    int baseElevation =
        GetTerrainElevation(
            cellX,
            cellY
        );

    int targetElevation =
        GetTerrainElevation(
            targetX,
            targetY
        );

    if (
        targetElevation !=
        baseElevation + 1
        )
    {
        return;
    }

    Vector2 lowTop{};
    Vector2 lowRight{};
    Vector2 lowBottom{};
    Vector2 lowLeft{};

    Vector2 highTop{};
    Vector2 highRight{};
    Vector2 highBottom{};
    Vector2 highLeft{};

    GetTerrainSurfacePointsView(
        cellX,
        cellY,
        baseElevation,
        lowTop,
        lowRight,
        lowBottom,
        lowLeft
    );

    GetTerrainSurfacePointsView(
        cellX,
        cellY,
        baseElevation + 1,
        highTop,
        highRight,
        highBottom,
        highLeft
    );

    Vector2 rampTop{};
    Vector2 rampRight{};
    Vector2 rampBottom{};
    Vector2 rampLeft{};

    switch (cell.rampDirection)
    {
    case RampDirection::North:
        rampTop = highTop;
        rampRight = highRight;
        rampBottom = lowBottom;
        rampLeft = lowLeft;
        break;

    case RampDirection::East:
        rampTop = lowTop;
        rampRight = highRight;
        rampBottom = highBottom;
        rampLeft = lowLeft;
        break;

    case RampDirection::South:
        rampTop = lowTop;
        rampRight = lowRight;
        rampBottom = highBottom;
        rampLeft = highLeft;
        break;

    case RampDirection::West:
        rampTop = highTop;
        rampRight = lowRight;
        rampBottom = lowBottom;
        rampLeft = highLeft;
        break;

    case RampDirection::None:
    default:
        return;
    }

    int tileIndex =
        tiles[
            CellIndex(
                cellX,
                cellY
            )
        ];

    if (
        tileIndex < 0 ||
        tileIndex >=
        static_cast<int>(
            tileBrushes.size()
            )
        )
    {
        tileIndex =
            static_cast<int>(
                TileType::Grass
                );
    }

    const TileBrush& brush =
        tileBrushes[
            tileIndex
        ];

    Texture2D brushTexture =
        GetTileBrushTexture(
            tileIndex
        );

    if (brushTexture.id != 0)
    {
        DrawTextureFrameOnQuad2D(
            brushTexture,
            GetTileBrushSourceRect(
                tileIndex
            ),
            rampTop,
            rampRight,
            rampBottom,
            rampLeft,
            WHITE
        );
    }
    else
    {
        Color fallback =
            brush.fallbackColor;

        fallback.a = 255;

        DrawTriangle(
            rampTop,
            rampRight,
            rampBottom,
            fallback
        );

        DrawTriangle(
            rampTop,
            rampBottom,
            rampLeft,
            fallback
        );
    }

    // Do not draw four black borders here.
    // They were responsible for some of the strong
    // diagonal lines around the ramp and walls.
}



Vector2 Game::GetObstacleViewPosition(
    const Obstacle& obstacle
) const
{
    Vector2 viewPosition =
        WorldToView(
            obstacle.position
        );

    // Terrain elevation raises the entire object.
    // heightLevel remains a local offset for roofs and stacked props.
    viewPosition.y -=
        GetTerrainHeightAtWorld(
            obstacle.position
        ) +
        static_cast<float>(
            obstacle.heightLevel
            ) *
        obstacleHeightStep;

    return viewPosition;
}

Vector2 Game::WorldToView(Vector2 worldPosition) const
{
    if (rendererMode == WorldRendererMode::Hybrid3D)
    {
        return GetWorldToScreen(
            {
                worldPosition.x * hybridUnitsPerPixel,
                0.0f,
                worldPosition.y * hybridUnitsPerPixel
            },
            hybridCamera
        );
    }

    if (!useIsometricView)
    {
        return worldPosition;
    }

    constexpr float diagonalScale = 0.70710678118f;

    return {
        (worldPosition.x - worldPosition.y) * diagonalScale,
        (worldPosition.x + worldPosition.y) * diagonalScale * isoVerticalScale
    };
}

Vector2 Game::ViewToWorld(Vector2 viewPosition) const
{
    if (rendererMode == WorldRendererMode::Hybrid3D)
    {
        Vector2 world{};

        if (ScreenToTerrainWorld3D(viewPosition, world))
        {
            return world;
        }

        return {};
    }

    if (!useIsometricView)
    {
        return viewPosition;
    }

    constexpr float diagonalScale = 0.70710678118f;

    float difference = viewPosition.x / diagonalScale;
    float sum = viewPosition.y / (diagonalScale * isoVerticalScale);

    return {
        (difference + sum) * 0.5f,
        (sum - difference) * 0.5f
    };
}

Vector2 Game::WorldVectorToView(Vector2 worldVector) const
{
    if (rendererMode == WorldRendererMode::Hybrid3D)
    {
        const Vector2 origin =
            GetWorldToScreen(
                { 0.0f, 0.0f, 0.0f },
                hybridCamera
            );

        const Vector2 endpoint =
            GetWorldToScreen(
                {
                    worldVector.x * hybridUnitsPerPixel,
                    0.0f,
                    worldVector.y * hybridUnitsPerPixel
                },
                hybridCamera
            );

        return Vector2Subtract(endpoint, origin);
    }

    if (!useIsometricView)
    {
        return worldVector;
    }

    constexpr float diagonalScale = 0.70710678118f;

    return {
        (worldVector.x - worldVector.y) * diagonalScale,
        (worldVector.x + worldVector.y) * diagonalScale * isoVerticalScale
    };
}

void Game::DrawGroundCircle(Vector2 worldCenter, float worldRadius, Color color) const
{
    Vector2 center =
        WorldToViewElevated(worldCenter);

    if (useIsometricView)
    {
        DrawEllipse(
            static_cast<int>(center.x),
            static_cast<int>(center.y),
            worldRadius,
            worldRadius * isoVerticalScale,
            color
        );
    }
    else
    {
        DrawCircleV(center, worldRadius, color);
    }
}

void Game::DrawGroundCircleLines(Vector2 worldCenter, float worldRadius, Color color) const
{
    Vector2 center =
        WorldToViewElevated(worldCenter);

    if (useIsometricView)
    {
        DrawEllipseLines(
            static_cast<int>(center.x),
            static_cast<int>(center.y),
            worldRadius,
            worldRadius * isoVerticalScale,
            color
        );
    }
    else
    {
        DrawCircleLines(
            static_cast<int>(center.x),
            static_cast<int>(center.y),
            worldRadius,
            color
        );
    }
}

void Game::DrawGroundCellOutline(int cellX, int cellY, Color color, float thickness) const
{
    float originX = -static_cast<float>(MapWidth) * TileSize * 0.5f;
    float originY = -static_cast<float>(MapHeight) * TileSize * 0.5f;

    float x0 = originX + static_cast<float>(cellX) * TileSize;
    float y0 = originY + static_cast<float>(cellY) * TileSize;
    float x1 = x0 + TileSize;
    float y1 = y0 + TileSize;

    Vector2 p0 = WorldToView({ x0, y0 });
    Vector2 p1 = WorldToView({ x1, y0 });
    Vector2 p2 = WorldToView({ x1, y1 });
    Vector2 p3 = WorldToView({ x0, y1 });

    DrawLineEx(p0, p1, thickness, color);
    DrawLineEx(p1, p2, thickness, color);
    DrawLineEx(p2, p3, thickness, color);
    DrawLineEx(p3, p0, thickness, color);
}

bool Game::IsTileWalkable(
    int tileType
) const
{
    if (
        tileType < 0 ||
        tileType >=
        static_cast<int>(
            tileBrushes.size()
            )
        )
    {
        return false;
    }

    return
        tileBrushes[
            tileType
        ].walkable;
}

bool Game::IsCellBlocked(int cellX, int cellY) const
{
    if (
        !IsCellInside(
            cellX,
            cellY
        ) ||
        !IsCellEnabled(
            cellX,
            cellY
        )
        )
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

        // Upper obstacle-local pieces do not block ground navigation.
        if (obstacle.heightLevel > 0)
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

            int currentElevation =
                GetTerrainElevation(
                    currentX,
                    currentY
                );

            int nextElevation =
                GetTerrainElevation(
                    nextX,
                    nextY
                );

            // Cliffs are impassable until ramps/stairs are added.
            if (
                !CanTraverseTerrainEdge(
                    currentX,
                    currentY,
                    nextX,
                    nextY
                )
                )
            {
                continue;
            }

            bool diagonal =
                direction[0] != 0 &&
                direction[1] != 0;

            if (diagonal)
            {
                // Elevation changes can only happen directly
                // through the cardinal ramp connection.
                if (
                    nextElevation !=
                    currentElevation
                    )
                {
                    continue;
                }

                int sideAX =
                    currentX +
                    direction[0];

                int sideAY =
                    currentY;

                int sideBX =
                    currentX;

                int sideBY =
                    currentY +
                    direction[1];

                if (
                    !IsCellInside(
                        sideAX,
                        sideAY
                    ) ||
                    !IsCellInside(
                        sideBX,
                        sideBY
                    ) ||
                    IsCellBlocked(
                        sideAX,
                        sideAY
                    ) ||
                    IsCellBlocked(
                        sideBX,
                        sideBY
                    ) ||
                    GetTerrainElevation(
                        sideAX,
                        sideAY
                    ) !=
                    currentElevation ||
                    GetTerrainElevation(
                        sideBX,
                        sideBY
                    ) !=
                    currentElevation
                    )
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

void Game::DrawObstacleVisual(const Obstacle& obstacle)
{
    Vector2 basePosition =
        GetObstacleViewPosition(
            obstacle
        );

    if (obstacle.hasTexture &&
        obstacle.texture.id != 0)
    {
        Rectangle source{
            0.0f,
            0.0f,
            static_cast<float>(obstacle.texture.width),
            static_cast<float>(obstacle.texture.height)
        };

        Rectangle destination{
            basePosition.x,
            basePosition.y,
            obstacle.size.x,
            obstacle.size.y
        };

        // The obstacle position represents the bottom centre/feet.
        Vector2 origin{
            obstacle.size.x * 0.5f,
            obstacle.size.y
        };

        DrawTexturePro(
            obstacle.texture,
            source,
            destination,
            origin,
            0.0f,
            WHITE
        );

        return;
    }

    // Temporary placeholder.
    float radius =
        std::max(
            obstacle.size.x,
            obstacle.size.y
        ) * 0.25f;

    DrawCircleV(
        {
            basePosition.x,
            basePosition.y - radius
        },
        radius,
        obstacle.type == 0
        ? Color{ 48, 126, 54, 255 }
        : Color{ 94, 93, 88, 255 }
    );
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

void Game::UpdateObstacleAutoCollider(
    Obstacle& obstacle
) const
{
    if (!obstacle.colliderAuto)
    {
        return;
    }

    switch (obstacle.type)
    {
    case 0: // Tree
        obstacle.colliderSize = {
            TileSize * 0.70f,
            TileSize * 0.50f
        };
        break;

    case 1: // Rock
        obstacle.colliderSize = {
            TileSize * 0.85f,
            TileSize * 0.70f
        };
        break;

    case 2: // Wall
        obstacle.colliderSize = {
            TileSize,
            TileSize
        };
        break;

    case 3: // Fence
        obstacle.colliderSize = {
            TileSize,
            TileSize * 0.30f
        };
        break;

    case 4: // Building
        obstacle.colliderSize = {
            TileSize * 2.0f,
            TileSize * 1.5f
        };
        break;

    case 5: // Bush
        obstacle.colliderSize = {
            TileSize * 0.75f,
            TileSize * 0.55f
        };
        break;

    case 6: // Statue
        obstacle.colliderSize = {
            TileSize * 0.60f,
            TileSize * 0.60f
        };
        break;

    default: // Custom
        obstacle.colliderSize = {
            TileSize,
            TileSize
        };
        break;
    }

    obstacle.colliderRadius =
        std::max(
            obstacle.colliderSize.x,
            obstacle.colliderSize.y
        ) * 0.5f;
}

void Game::DrawGround()
{
    if (
        !groundCacheReady ||
        groundCacheDirty
        )
    {
        BuildGroundCache();
    }

    if (
        !groundCacheReady ||
        groundCache.texture.id == 0
        )
    {
        TraceLog(
            LOG_ERROR,
            "[GROUND DRAW] Ground cache is unavailable."
        );

        return;
    }

    Rectangle source{
        0.0f,
        0.0f,
        static_cast<float>(
            groundCache.texture.width
        ),
        -static_cast<float>(
            groundCache.texture.height
        )
    };

    DrawTextureRec(
        groundCache.texture,
        source,
        groundCacheDrawPosition,
        WHITE
    );
}

void Game::DrawObstacles()
{
    for (const Obstacle& obstacle : obstacles)
    {
        Vector2 viewPosition = WorldToView(obstacle.position);

        Rectangle visualRect{
            viewPosition.x - obstacle.size.x * 0.5f,
            viewPosition.y - obstacle.size.y * 0.5f,
            obstacle.size.x,
            obstacle.size.y
        };

        if (obstacle.hasTexture)
        {
            // Props stay upright like billboards; only their ground position is projected.
            DrawTextureInRect(obstacle.texture, visualRect, true);
        }
        else
        {
            float radius = std::max(obstacle.size.x, obstacle.size.y) * 0.5f;

            if (obstacle.type == 0)
            {
                DrawCircleV(Vector2Add(viewPosition, { 5.0f, 8.0f }), radius + 10.0f, Color{ 0, 0, 0, 65 });
                DrawCircleV(viewPosition, radius + 10.0f, Color{ 35, 78, 38, 255 });
                DrawCircleV(viewPosition, radius, Color{ 48, 126, 54, 255 });
                DrawCircleV(Vector2Add(viewPosition, { -10.0f, -8.0f }), radius * 0.55f, Color{ 76, 153, 73, 255 });
            }
            else
            {
                DrawCircleV(Vector2Add(viewPosition, { 5.0f, 7.0f }), radius, Color{ 0, 0, 0, 60 });
                DrawCircleV(viewPosition, radius, Color{ 94, 93, 88, 255 });
                DrawCircleV(Vector2Add(viewPosition, { -9.0f, -7.0f }), radius * 0.45f, Color{ 126, 125, 116, 255 });
            }
        }

        if (buildMode && obstacle.collisionEnabled)
        {
            if (obstacle.collisionShape == CollisionShape::Box)
            {
                Rectangle collider = GetObstacleCollisionRect(obstacle);

                Vector2 p0 = WorldToView({ collider.x, collider.y });
                Vector2 p1 = WorldToView({ collider.x + collider.width, collider.y });
                Vector2 p2 = WorldToView({ collider.x + collider.width, collider.y + collider.height });
                Vector2 p3 = WorldToView({ collider.x, collider.y + collider.height });

                Color c{ 255, 70, 70, 180 };
                DrawLineEx(p0, p1, 2.0f, c);
                DrawLineEx(p1, p2, 2.0f, c);
                DrawLineEx(p2, p3, 2.0f, c);
                DrawLineEx(p3, p0, 2.0f, c);
            }
            else
            {
                DrawGroundCircleLines(
                    obstacle.position,
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
        Vector2 viewPosition = WorldToView(npc.position);

        DrawGroundCircleLines(
            npc.position,
            interactDistance,
            Color{ 255, 255, 255, 60 }
        );

        DrawCircleV(Vector2Add(viewPosition, { 5.0f, 8.0f }), npc.radius + 6.0f, Color{ 0, 0, 0, 70 });
        DrawCircleV(viewPosition, npc.radius + 6.0f, Color{ 28, 28, 34, 255 });
        DrawCircleV(viewPosition, npc.radius, Color{ 218, 184, 92, 255 });

        DrawText(
            npc.name.c_str(),
            static_cast<int>(viewPosition.x - 66.0f),
            static_cast<int>(viewPosition.y - 62.0f),
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
            DrawLineEx(
                WorldToView(currentPath[i]),
                WorldToView(currentPath[i + 1]),
                3.0f,
                Color{ 255, 255, 255, 100 }
            );
        }

        DrawGroundCircleLines(
            currentPath.back(),
            18.0f,
            Color{ 255, 255, 255, 180 }
        );
    }

    float jumpHeight = GetHuashanJumpHeight();
    Vector2 groundPosition =
        WorldToViewElevated(
            playerPosition
        );
    Vector2 drawPosition = { groundPosition.x, groundPosition.y - jumpHeight };

    float shadowScale = 1.0f - jumpHeight / 140.0f;

    if (shadowScale < 0.45f)
    {
        shadowScale = 0.45f;
    }

    DrawEllipse(
        static_cast<int>(groundPosition.x + 5.0f),
        static_cast<int>(groundPosition.y + 10.0f),
        playerRadius * 1.05f * shadowScale,
        playerRadius * 0.45f * shadowScale,
        Color{ 0, 0, 0, 95 }
    );

    if (huashanJumpActive)
    {
        DrawLineEx(
            WorldToView(huashanJumpStart),
            WorldToView(huashanJumpEnd),
            3.0f,
            Color{ 255, 80, 55, 130 }
        );
    }

    DrawPlayerSprite(drawPosition);

    if (huashanJumpActive)
    {
        DrawCircleLines(
            static_cast<int>(drawPosition.x),
            static_cast<int>(drawPosition.y),
            playerRadius + 8.0f,
            Color{ 255, 80, 55, 220 }
        );
    }
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

void Game::UpdateDebugUpgradeButtonRect()
{
    const float screenWidth =
        static_cast<float>(
            GetScreenWidth()
            );

    const float buttonWidth =
        Clamp(
            screenWidth * 0.15f,
            124.0f,
            170.0f
        );

    debugUpgradeButtonRect = {
        screenWidth -
            buttonWidth -
            18.0f,

        72.0f,

        buttonWidth,
        42.0f
    };
}

void Game::OpenManualUpgradeMenu()
{
    if (
        gameState !=
        GameState::Playing ||
        buildMode
        )
    {
        return;
    }

    upgradeMenuOpenedManually =
        true;

    GenerateSkillChoices();

    gameState =
        GameState::ChoosingUpgrade;

    // Release gameplay controls so they do not remain
    // held when the player returns from the menu.
    attackButtonDown =
        false;

    dashButtonDown =
        false;

    dashButtonPressed =
        false;

    joystickActive =
        false;

    joystickDirection = {
        0.0f,
        0.0f
    };
}

void Game::UpdateDebugControls()
{
    if (
        buildMode ||
        gameState !=
        GameState::Playing
        )
    {
        return;
    }

    // --------------------------------------------------
    // Pause/resume automatic wave spawning
    // --------------------------------------------------

    if (IsKeyPressed(KEY_P))
    {
        waveSpawningPaused =
            !waveSpawningPaused;

        TraceLog(
            LOG_INFO,
            "[DEBUG] Automatic wave spawning: %s",
            waveSpawningPaused
            ? "PAUSED"
            : "RUNNING"
        );
    }

    // --------------------------------------------------
    // Manual enemy spawning
    //
    // These do not change wave.enemiesSpawned, so they
    // do not corrupt normal wave progression.
    // --------------------------------------------------

    if (IsKeyPressed(KEY_ONE))
    {
        SpawnEnemy(
            EnemyType::Grunt
        );

        TraceLog(
            LOG_INFO,
            "[DEBUG] Spawned Grunt."
        );
    }

    if (IsKeyPressed(KEY_TWO))
    {
        SpawnEnemy(
            EnemyType::Runner
        );

        TraceLog(
            LOG_INFO,
            "[DEBUG] Spawned Runner."
        );
    }

    if (IsKeyPressed(KEY_THREE))
    {
        SpawnEnemy(
            EnemyType::Tank
        );

        TraceLog(
            LOG_INFO,
            "[DEBUG] Spawned Tank."
        );
    }

    if (IsKeyPressed(KEY_FOUR))
    {
        SpawnEnemy(
            EnemyType::Shooter
        );

        TraceLog(
            LOG_INFO,
            "[DEBUG] Spawned Shooter."
        );
    }

    if (IsKeyPressed(KEY_FIVE))
    {
        // Uses your existing special test function,
        // which positions the Boss close to the player.
        SpawnTestBoss();
    }

    // --------------------------------------------------
    // Manual upgrade selection
    // --------------------------------------------------

    if (IsKeyPressed(KEY_U))
    {
        OpenManualUpgradeMenu();
    }

}

void Game::DrawDebugUpgradeButton() const
{
    if (
        gameState !=
        GameState::Playing ||
        buildMode
        )
    {
        return;
    }

    const bool hovered =
        CheckCollisionPointRec(
            GetMousePosition(),
            debugUpgradeButtonRect
        );

    Color fillColor =
        hovered
        ? Color{
            150,
            105,
            35,
            235
    }
        : Color{
            95,
            68,
            30,
            220
    };

    Color outlineColor =
        hovered
        ? GOLD
        : Color{
            225,
            190,
            105,
            255
    };

    DrawRectangleRounded(
        debugUpgradeButtonRect,
        0.20f,
        8,
        fillColor
    );

    DrawRectangleRoundedLinesEx(
        debugUpgradeButtonRect,
        0.20f,
        8,
        2.0f,
        outlineColor
    );

    const char* label =
        "UPGRADE [U]";

    constexpr int fontSize =
        18;

    const int textWidth =
        MeasureText(
            label,
            fontSize
        );

    DrawText(
        label,

        static_cast<int>(
            debugUpgradeButtonRect.x +
            debugUpgradeButtonRect.width *
            0.5f -
            static_cast<float>(
                textWidth
                ) *
            0.5f
            ),

        static_cast<int>(
            debugUpgradeButtonRect.y +
            debugUpgradeButtonRect.height *
            0.5f -
            static_cast<float>(
                fontSize
                ) *
            0.5f
            ),

        fontSize,
        WHITE
    );

}
void Game::DrawUi()
{
    DrawRectangle(0, 0, GetScreenWidth(), 62, Color{ 0, 0, 0, 130 });

    const char* rendererTitle =
        rendererMode == WorldRendererMode::Hybrid3D
        ? "Moxiang2D - Hybrid 3D Isometric"
        : "Moxiang2D - Legacy 2D Isometric";

    DrawText(
        rendererTitle,
        18,
        10,
        22,
        WHITE
    );

#if MOXIANG_USE_IMGUI
    DrawText("F6: toggle renderer | Click ground to move | B: Build Mode", 18, 36, 16, Color{ 225, 225, 225, 255 });
#else
    DrawText("F6: toggle Hybrid 3D / Legacy 2D | Click/tap ground to move", 18, 36, 16, Color{ 225, 225, 225, 255 });
#endif

    std::string chamberLabel = "CHAMBER: NONE";

    const DungeonChamber* activeChamber =
        FindChamberById(activeChamberId);

    if (activeChamber != nullptr)
    {
        chamberLabel =
            "CHAMBER: " +
            std::to_string(activeChamber->id) +
            " - " +
            activeChamber->name;
    }

    constexpr int chamberFontSize = 18;

    const int chamberTextWidth =
        MeasureText(
            chamberLabel.c_str(),
            chamberFontSize
        );

    DrawText(
        chamberLabel.c_str(),
        GetScreenWidth() - chamberTextWidth - 18,
        20,
        chamberFontSize,
        Color{ 235, 210, 140, 255 }
    );
}
void Game::DrawEditorWorldOverlay()
{
    if (rendererMode == WorldRendererMode::Hybrid3D)
    {
        return;
    }

    if (!buildMode)
    {
        return;
    }

    if (
        showGrid ||
        showChamberOverlay
        )
    {
        for (int y = 0; y < MapHeight; ++y)
        {
            for (int x = 0; x < MapWidth; ++x)
            {
                Color cellColor{
                    255,
                    255,
                    255,
                    34
                };

                float thickness = 1.0f;

                if (!IsCellEnabled(x, y))
                {
                    cellColor =
                        Color{
                            255,
                            70,
                            70,
                            70
                    };
                }
                else if (showChamberOverlay)
                {
                    const int chamberId =
                        terrainCells[
                            CellIndex(x, y)
                        ].chamberId;

                    cellColor =
                        GetChamberDebugColor(
                            chamberId,
                            150
                        );

                    thickness = 2.0f;
                }

                DrawTerrainCellOutline(
                    x,
                    y,
                    cellColor,
                    thickness
                );
            }
        }
    }

    // Display logical ramp connections in Build Mode.
    for (int y = 0; y < MapHeight; ++y)
    {
        for (int x = 0; x < MapWidth; ++x)
        {
            if (!IsCellEnabled(x, y))
            {
                continue;
            }

            const TerrainCell& cell =
                terrainCells[
                    CellIndex(
                        x,
                        y
                    )
                ];

            int offsetX = 0;
            int offsetY = 0;

            if (
                !GetRampDirectionOffset(
                    cell.rampDirection,
                    offsetX,
                    offsetY
                )
                )
            {
                continue;
            }

            int destinationX =
                x +
                offsetX;

            int destinationY =
                y +
                offsetY;

            if (
                !IsCellInside(
                    destinationX,
                    destinationY
                )
                )
            {
                continue;
            }

            Vector2 start =
                WorldToViewElevated(
                    CellToWorld(
                        x,
                        y
                    )
                );

            Vector2 end =
                WorldToViewElevated(
                    CellToWorld(
                        destinationX,
                        destinationY
                    )
                );

            DrawLineEx(
                start,
                end,
                5.0f,
                MAGENTA
            );

            DrawCircleV(
                start,
                5.0f,
                BLUE
            );

            DrawCircleV(
                end,
                7.0f,
                YELLOW
            );
        }
    }

    Vector2 mouseView =
        GetScreenToWorld2D(
            GetMousePosition(),
            camera
        );

    int cellX = 0;
    int cellY = 0;

    if (
        ViewToTerrainCell(
            mouseView,
            cellX,
            cellY
        )
        )
    {
        DrawTerrainCellOutline(
            cellX,
            cellY,
            YELLOW,
            2.0f
        );

        Vector2 labelPosition =
            WorldToViewElevated(
                CellToWorld(cellX, cellY)
            );

        DrawText(
            TextFormat(
                "H%d  C%d  %s",
                GetTerrainElevation(
                    cellX,
                    cellY
                ),
                terrainCells[
                    CellIndex(
                        cellX,
                        cellY
                    )
                ].chamberId,
                IsCellEnabled(
                    cellX,
                    cellY
                )
                        ? "ACTIVE"
                        : "VOID"
                        ),
            static_cast<int>(labelPosition.x - 10.0f),
            static_cast<int>(labelPosition.y - 24.0f),
            16,
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

    int rendererIndex =
        static_cast<int>(rendererMode);

    const char* rendererNames[] = {
        "Legacy 2D",
        "Hybrid 3D"
    };

    if (
        ImGui::Combo(
            "Renderer",
            &rendererIndex,
            rendererNames,
            2
        )
        )
    {
        rendererMode =
            static_cast<WorldRendererMode>(
                rendererIndex
                );

        if (rendererMode == WorldRendererMode::Hybrid3D)
        {
            hybridCameraTargetWorld =
                playerPosition;

            MarkHybridTerrainDirty();
            UpdateHybridCamera(0.0f);
        }
        else
        {
            InvalidateGroundCache();
            camera.target =
                WorldToViewElevated(
                    playerPosition
                );
        }
    }

    if (buildMode)
    {
        ImGui::Separator();

        ImGui::Text("Enemy Preview");

        ImGui::Checkbox(
            "Use Blob For Missing Enemy Sprites",
            &useBlobForMissingEnemySprites
        );

        if (useBlobForMissingEnemySprites)
        {
            ImGui::TextDisabled(
                "Tank, Shooter and Boss use the blob as a temporary placeholder."
            );
        }
    }

    ImGui::Checkbox("Show Grid", &showGrid);

    ImGui::Separator();

    if (ImGui::Button("Save Level"))
    {
        SaveLevel(DefaultLevelPath);
    }

    ImGui::SameLine();

    if (ImGui::Button("Load Level"))
    {
        if (!LoadLevel(DefaultLevelPath))
        {
            LoadDefaultLevel();
            InitializeTestChambers();
        }

        Vector2 requestedSpawn =
            CellToWorld(
                MapWidth / 2,
                MapHeight / 2
            );

        int spawnCellX = MapWidth / 2;
        int spawnCellY = MapHeight / 2;

        if (
            FindNearestWalkableCell(
                requestedSpawn,
                spawnCellX,
                spawnCellY
            )
            )
        {
            playerPosition =
                CellToWorld(
                    spawnCellX,
                    spawnCellY
                );
        }
        else
        {
            playerPosition = requestedSpawn;
        }

        player.pos = playerPosition;
        player.moveTarget = playerPosition;
        player.hasMoveTarget = false;

        currentPath.clear();
        pathIndex = 0;
        hasPath = false;

        UpdateActiveChamber(true);
        InvalidateGroundCache();

        if (
            rendererMode ==
            WorldRendererMode::Hybrid3D
            )
        {
            hybridCameraTargetWorld =
                playerPosition;

            UpdateHybridCamera(0.0f);
        }
        else
        {
            camera.target =
                WorldToViewElevated(
                    playerPosition
                );
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Restart Test"))
    {
        RestartGameplay();
    }

    ImGui::Text("Level file: %s", DefaultLevelPath);

    ImGui::Separator();

    ImGui::Text("Map Canvas");

    ImGui::InputInt(
        "Map Width",
        &editorNewMapWidth
    );

    ImGui::InputInt(
        "Map Height",
        &editorNewMapHeight
    );

    editorNewMapWidth =
        std::max(
            4,
            std::min(
                MaximumMapDimension,
                editorNewMapWidth
            )
        );

    editorNewMapHeight =
        std::max(
            4,
            std::min(
                MaximumMapDimension,
                editorNewMapHeight
            )
        );

    ImGui::Checkbox(
        "Start With Empty Map",
        &editorNewMapStartsEmpty
    );

    if (ImGui::Button("Create New Map"))
    {
        CreateNewMap(
            editorNewMapWidth,
            editorNewMapHeight,
            editorNewMapStartsEmpty
        );
    }

    ImGui::Text(
        "Current map: %d x %d",
        MapWidth,
        MapHeight
    );

    ImGui::TextDisabled(
        "Void cells define the irregular outer shape."
    );

    ImGui::Separator();

    ImGui::Text("Chambers");

    ImGui::InputInt(
        "Selected Chamber ID",
        &editorSelectedChamberId
    );

    editorSelectedChamberId =
        std::max(
            0,
            editorSelectedChamberId
        );

    ImGui::InputText(
        "Chamber Name",
        editorChamberNameInput,
        sizeof(editorChamberNameInput)
    );

    if (ImGui::Button("Add / Update Chamber"))
    {
        EnsureChamberExists(
            editorSelectedChamberId
        );

        DungeonChamber* chamber =
            FindChamberById(
                editorSelectedChamberId
            );

        if (chamber != nullptr)
        {
            chamber->name =
                editorChamberNameInput;
        }

        RebuildChamberBounds();
    }

    ImGui::Checkbox(
        "Show Chamber Overlay",
        &showChamberOverlay
    );

    for (const DungeonChamber& chamber : chambers)
    {
        ImGui::Text(
            "ID %d: %s%s",
            chamber.id,
            chamber.name.c_str(),
            chamber.hasCells
            ? ""
            : " (unused)"
        );
    }

    ImGui::Separator();

    const char* tools[] = {
        "Paint Tile",
        "Raise Terrain",
        "Lower Terrain",
        "Flatten Terrain",

        "Paint Wall",
        "Clear Wall",

        "Paint Chamber",
        "Erase Map Cell",

        "Place Ramp",
        "Remove Ramp",

        "Place Obstacle",
        "Erase Obstacle",
        "Move Obstacle"
    };

    ImGui::Combo(
        "Tool",
        &editorTool,
        tools,
        13
    );

    if (
        editorTool ==
        static_cast<int>(
            EditorTool::PaintWall
            )
        )
    {
        ImGui::TextDisabled(
            "Hybrid 3D: click directly on a visible side wall."
        );
    }
    else if (
        editorTool ==
        static_cast<int>(
            EditorTool::ClearWall
            )
        )
    {
        ImGui::TextDisabled(
            "Click a wall to restore its inherited floor material."
        );
    }

    ImGui::Separator();

    ImGui::Text("Terrain Elevation");

    if (
        ImGui::DragFloat(
            "Terrain Height Per Level",
            &terrainElevationStep,
            1.0f,
            16.0f,
            96.0f,
            "%.0f px"
        )
        )
    {
        InvalidateGroundCache();
    }

    if (
        ImGui::SliderInt(
            "Maximum Terrain Level",
            &maxTerrainElevation,
            1,
            12
        )
        )
    {
        editorFlattenElevation =
            std::min(
                editorFlattenElevation,
                maxTerrainElevation
            );
    }

    ImGui::SliderInt(
        "Flatten Target Level",
        &editorFlattenElevation,
        0,
        maxTerrainElevation
    );

    ImGui::TextDisabled(
        "Cliffs block movement unless connected by a ramp."
    );

    ImGui::Separator();

    ImGui::Text("Ramp / Stair Connection");

    const char* rampDirections[] = {
        "None",
        "North (-Y)",
        "East (+X)",
        "South (+Y)",
        "West (-X)"
    };

    ImGui::Combo(
        "Ramp Direction",
        &editorRampDirection,
        rampDirections,
        5
    );

    ImGui::TextDisabled(
        "Click the lower cell and point toward the adjacent level +1 cell."
    );

    ImGui::Separator();

    ImGui::Text("Obstacle Local Height");

    ImGui::SliderInt(
        "Active Height Level",
        &editorHeightLevel,
        0,
        12
    );

    ImGui::DragFloat(
        "Height Per Level",
        &obstacleHeightStep,
        1.0f,
        16.0f,
        96.0f,
        "%.0f px"
    );

    ImGui::Checkbox(
        "Select Active Level Only",
        &editorSelectActiveLevelOnly
    );

    if (ImGui::Button("Level Down"))
    {
        editorHeightLevel =
            std::max(
                0,
                editorHeightLevel - 1
            );
    }

    ImGui::SameLine();

    if (ImGui::Button("Level Up"))
    {
        editorHeightLevel =
            std::min(
                12,
                editorHeightLevel + 1
            );
    }

    ImGui::Text(
        "Editing level: %d",
        editorHeightLevel
    );

    ImGui::Separator();

    ImGui::Text("Terrain Tilesheet");

    ImGui::InputText(
        "Tilesheet Path",
        terrainTileSheetPathInput,
        sizeof(
            terrainTileSheetPathInput
            )
    );

    ImGui::InputInt(
        "Tilesheet Columns",
        &terrainTileSheetColumns
    );

    ImGui::InputInt(
        "Tilesheet Rows",
        &terrainTileSheetRows
    );

    terrainTileSheetColumns =
        std::max(
            1,
            terrainTileSheetColumns
        );

    terrainTileSheetRows =
        std::max(
            1,
            terrainTileSheetRows
        );

    tileImageDropHovered =
        DrawImageDropZone(
            "Terrain Tilesheet Drop",
            terrainTileSheetPathInput
        );

    if (
        ImGui::Button(
            "Load Terrain Tilesheet"
        )
        )
    {
        LoadTerrainTileSheet(
            terrainTileSheetPathInput,
            terrainTileSheetColumns,
            terrainTileSheetRows
        );
    }

    if (
        terrainTileSheet.id != 0 &&
        !tileBrushes.empty()
        )
    {
        ImGui::Separator();

        selectedTile =
            std::max(
                0,
                std::min(
                    selectedTile,
                    static_cast<int>(
                        tileBrushes.size()
                        ) - 1
                )
            );

        ImGui::SliderInt(
            "Selected Tile",
            &selectedTile,
            0,
            static_cast<int>(
                tileBrushes.size()
                ) - 1
        );

        const int selectedColumn =
            selectedTile %
            terrainTileSheetColumns;

        const int selectedRow =
            selectedTile /
            terrainTileSheetColumns;

        ImGui::Text(
            "Tile index: %d",
            selectedTile
        );

        ImGui::Text(
            "Atlas position: column %d, row %d",
            selectedColumn,
            selectedRow
        );

        ImGui::Checkbox(
            "Selected Tile Walkable",
            &tileBrushes[
                selectedTile
            ].walkable
        );

        const Rectangle source =
            GetTileBrushSourceRect(
                selectedTile
            );

        const ImVec2 uv0{
            source.x /
                static_cast<float>(
                    terrainTileSheet.width
                ),

            source.y /
                static_cast<float>(
                    terrainTileSheet.height
                )
        };

        const ImVec2 uv1{
            (
                source.x +
                source.width
            ) /
                static_cast<float>(
                    terrainTileSheet.width
                ),

            (
                source.y +
                source.height
            ) /
                static_cast<float>(
                    terrainTileSheet.height
                )
        };

        const ImTextureID previewTextureId =
            (ImTextureID)(
                intptr_t
                )terrainTileSheet.id;

        ImGui::Text("Selected Tile Preview");

        ImGui::Image(
            previewTextureId,
            ImVec2{
                128.0f,
                128.0f
            },
            uv0,
            uv1
        );
    }
    else
    {
        ImGui::TextDisabled(
            "Load a terrain tilesheet to select atlas tiles."
        );
    }

    ImGui::Separator();

    const char* obstacleNames[] = {
        "Tree",
        "Rock",
        "Wall",
        "Fence",
        "Building",
        "Bush",
        "Statue",
        "Custom Image"
    };

    ImGui::Combo(
        "Obstacle Type",
        &selectedObstacleType,
        obstacleNames,
        8
    );

    ImGui::Checkbox(
        "Snap Base To Grid",
        &obstacleSnapToGrid
    );

    ImGui::Checkbox(
        "Size Width In Tiles",
        &obstacleFitWidthToTiles
    );

    if (obstacleFitWidthToTiles)
    {
        float oldWidthTiles =
            obstacleVisualWidthTiles;

        ImGui::DragFloat(
            "Visual Width In Tiles",
            &obstacleVisualWidthTiles,
            0.05f,
            0.25f,
            8.0f,
            "%.2f tiles"
        );

        if (obstacleVisualWidthTiles !=
            oldWidthTiles)
        {
            float targetWidth =
                TileSize *
                obstacleVisualWidthTiles;

            float aspectRatio = 1.0f;

            if (currentObstacleNativeSize.x >
                0.0f)
            {
                aspectRatio =
                    currentObstacleNativeSize.y /
                    currentObstacleNativeSize.x;
            }

            newObstacleSize = {
                targetWidth,
                targetWidth * aspectRatio
            };
        }

        ImGui::Text(
            "Visual size: %.0f x %.0f",
            newObstacleSize.x,
            newObstacleSize.y
        );
    }
    else
    {
        ImGui::Checkbox(
            "Keep Aspect Ratio",
            &obstacleKeepAspectRatio
        );

        float previousWidth =
            newObstacleSize.x;

        ImGui::DragFloat(
            "Visual Width",
            &newObstacleSize.x,
            1.0f,
            8.0f,
            1024.0f
        );

        if (obstacleKeepAspectRatio &&
            previousWidth != newObstacleSize.x &&
            currentObstacleNativeSize.x > 0.0f)
        {
            float aspectRatio =
                currentObstacleNativeSize.y /
                currentObstacleNativeSize.x;

            newObstacleSize.y =
                newObstacleSize.x *
                aspectRatio;
        }

        if (!obstacleKeepAspectRatio)
        {
            ImGui::DragFloat(
                "Visual Height",
                &newObstacleSize.y,
                1.0f,
                8.0f,
                1024.0f
            );
        }
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

    ImGui::Text("Obstacle Rendering");

    ImGui::Checkbox(
        "Casts Shadow",
        &newObstacleCastsShadow
    );

    ImGui::Checkbox(
        "Blocks Light",
        &newObstacleBlocksLight
    );

    ImGui::Checkbox(
        "Foreground Object",
        &newObstacleForeground
    );

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

void Game::InitCombat()
{
    player.pos = playerPosition;
    player.moveTarget = playerPosition;
    player.hasMoveTarget = false;

    player.hp = 120;
    player.maxHp = 120;

    player.attackDamage = 12;
    player.attackInterval = 0.42f;
    player.meleeRange = 78.0f;
    player.attackAssistRange = 360.0f;

    attackButtonDown = false;
    attackAssistPathTimer = 0.0f;

    waveSpawningPaused =
        false;

    upgradeMenuOpenedManually =
        false;

    UpdateDebugUpgradeButtonRect();

    dashButtonDown = false;
    dashButtonWasDown = false;
    dashButtonPressed = false;
    dashActive = false;
    dashTimer = 0.0f;
    dashCooldownRemaining = 0.0f;
    dashInvulnerabilityTimer = 0.0f;
    dashAfterimageTimer = 0.0f;
    dashAfterimages.clear();
    playerKnockbackActive = false;

    playerKnockbackVelocity = {
        0.0f,
        0.0f
    };

    playerKnockbackTimer = 0.0f;
    playerKnockbackMaxTimer = 0.0f;
    playerKnockbackPeakHeight = 145.0f;

    UpdateAttackButtonRect();
    UpdateDashButtonRect();
    UpdateSkillButtonRects();

    player.hp = player.maxHp;
    // Allow the first button press to attack immediately.
    player.attackTimer = player.attackInterval;

    enemies.clear();
    pendingEnemySpawns.clear();

    projectiles.clear();
    vfxParticles.clear();
    orbitalBlades.clear();

    huashanJumpActive = false;
    huashanJumpTimer = 0.0f;
    huashanJumpDuration = 0.24f;

    huashanImpactAnimationActive =
        false;

    huashanImpactFrame =
        0;

    huashanImpactFrameTimer =
        0.0f;

    huashanGroundMarks.clear();

    huashanScreenShakeTimer =
        0.0f;

    huashanScreenShakeOffset = {
        0.0f,
        0.0f
    };

    huashanImpactFlashTimer =
        0.0f;

    dongfengCasting = false;
    dongfengCastTimer = 0.0f;
    dongfengCastDuration = 0.45f;
    dongfengWaveSpeed = 700.0f;
    dongfengLockedTargetId = 0;

    dongfengWaveActive = false;
    dongfengWaveId = 0;
    dongfengWaveTravelled = 0.0f;

    dongfengCastDirection = { 1.0f, 0.0f };
    dongfengWaveDirection = { 1.0f, 0.0f };

    nextEnemyId = 1;
    nextDongfengWaveId = 1;

    skills[0].type = SkillType::SpinningBlade; // Wind Blades
    skills[0].cooldown = 3.0f;
    skills[0].cooldownRemaining = 0.0f;
    skills[0].unlocked = true;
    skills[0].level = 1;

    skills[1].type = SkillType::Huashan;
    skills[1].cooldown = 5.0f;
    skills[1].cooldownRemaining = 0.0f;
    skills[1].unlocked = false;
    skills[1].level = 1;

    skills[2].type = SkillType::Dongfeng;
    skills[2].cooldown = 8.0f;
    skills[2].cooldownRemaining = 0.0f;
    skills[2].unlocked = false;
    skills[2].level = 1;

    UpdateSkillButtonRects();

    gameState = GameState::Playing;

    bossFallingRocks.clear();

    ResetChamberEncounterProgress();
    StartChamberEncounter(
        activeChamberId
    );
}

void Game::RestartGameplay()
{
    // --------------------------------------------------
    // Clear temporary movement and interaction state
    // --------------------------------------------------
    bossFallingRocks.clear();
    currentPath.clear();
    pathIndex = 0;
    hasPath = false;

    pendingNpc = -1;
    activeDialogueNpc = -1;
    dialogueCooldown = 0.0f;

    selectedObstacleIndex = -1;
    draggingObstacle = false;

    joystickActive = false;
    joystickWasTouching = false;
    joystickTouchId = -1;

    joystickDirection = {
        0.0f,
        0.0f
    };

    wasTouching = false;

    // --------------------------------------------------
    // Find a safe restart cell near the map centre
    // --------------------------------------------------

    Vector2 requestedSpawn =
        CellToWorld(
            MapWidth / 2,
            MapHeight / 2
        );

    int spawnCellX =
        MapWidth / 2;

    int spawnCellY =
        MapHeight / 2;

    if (
        FindNearestWalkableCell(
            requestedSpawn,
            spawnCellX,
            spawnCellY
        )
        )
    {
        playerPosition =
            CellToWorld(
                spawnCellX,
                spawnCellY
            );
    }
    else
    {
        playerPosition =
            requestedSpawn;
    }

    // --------------------------------------------------
    // Reset animation state
    // --------------------------------------------------

    playerAnimationState =
        PlayerAnimationState::Idle;

    playerAnimFrame = 0;
    playerAnimTimer = 0.0f;

    playerAttackImpactTriggered = false;
    playerAttackImpactPending = false;
    playerAttackTargetId = 0;

    // --------------------------------------------------
    // Reset combat, chamber ownership, enemies, projectiles and skills
    // --------------------------------------------------

    UpdateActiveChamber(true);
    InitCombat();

    player.pos =
        playerPosition;

    player.moveTarget =
        playerPosition;

    player.hasMoveTarget =
        false;

    currentUpgradeChoices.clear();

    // --------------------------------------------------
    // Move the camera immediately to the restart point
    // --------------------------------------------------

    if (rendererMode == WorldRendererMode::Hybrid3D)
    {
        hybridCameraTargetWorld =
            playerPosition;

        UpdateHybridCamera(0.0f);
    }
    else
    {
        camera.target =
            WorldToViewElevated(
                playerPosition
            );
    }

    TraceLog(
        LOG_INFO,
        "[RESTART] Gameplay restarted at cell %d, %d.",
        spawnCellX,
        spawnCellY
    );
}

void Game::UpdateCombat(float dt)
{

    vfxSpawnedThisFrame = 0;

    double combatStartTime = GetTime();

    PERF_TIME_BLOCK(perfSkillsMs,
        {
            UpdateSkills(dt);
        });

    PERF_TIME_BLOCK(
        perfHuashanMs,
        {
            UpdateHuashan(dt);

    // Must continue updating after huashanJumpActive becomes false.
    //UpdateHuashanImpactAnimation(dt);
        }
    );

    PERF_TIME_BLOCK(perfDongfengMs,
        {
            UpdateDongfeng(dt);
        });

    PERF_TIME_BLOCK(perfWaveMs,
        {
            UpdateWave(dt);
        });

    PERF_TIME_BLOCK(perfEnemiesMs,
        {
            UpdateEnemies(dt);
            UpdateBossFallingRocks(
                dt
            );
        });

    PERF_TIME_BLOCK(perfMeleeMs,
        {
            UpdateMeleeAttack(dt);
        });

    PERF_TIME_BLOCK(perfBladesMs,
        {
            UpdateOrbitalBlades(dt);
        });

    PERF_TIME_BLOCK(perfProjectilesMs,
        {
            UpdateProjectiles(dt);
        });

    PERF_TIME_BLOCK(perfCollisionMs,
        {
            CheckProjectileEnemyCollisions();
        });

    PERF_TIME_BLOCK(perfVfxMs,
        {
            UpdateVfx(dt);
        });

    PERF_TIME_BLOCK(perfCleanupMs,
        {
            CleanupCombatObjects();
        });

    perfCombatMs = static_cast<float>((GetTime() - combatStartTime) * 1000.0);

    if (player.hp <= 0)
    {
        player.hp = 0;
        gameState = GameState::GameOver;
    }
}

void Game::DrawPerformanceOverlay() const
{
    if (!showPerformanceOverlay)
    {
        return;
    }

    int x = 16;
    int y = 74;
    int line = 18;

    Color bg{ 0, 0, 0, 170 };
    DrawRectangle(x - 8, y - 8, 500, 800, bg);

    Color frameColor = GREEN;

    if (perfFrameMs > 22.0f)
    {
        frameColor = YELLOW;
    }

    if (perfFrameMs > 33.0f)
    {
        frameColor = RED;
    }
    DrawText("F4 overlay | F5 dump spike log", x, y, 15, LIGHTGRAY);
    y += line + 6;

    DrawText(TextFormat("FPS: %d", GetFPS()), x, y, 18, frameColor);
    y += line;

    DrawText(TextFormat("Full Frame: %.2f ms", perfFullFrameMs), x, y, 15, WHITE);
    y += line;

    DrawText(TextFormat("Main Update: %.2f ms", perfExternalUpdateMs), x, y, 15, WHITE);
    y += line;

    DrawText(TextFormat("Main Draw: %.2f ms", perfExternalDrawMs), x, y, 15, WHITE);
    y += line;

    DrawText(TextFormat("EndDrawing: %.2f ms", perfEndDrawingMs), x, y, 15, WHITE);
    y += line + 6;

    DrawText(TextFormat("Frame: %.2f ms", perfFrameMs), x, y, 16, frameColor);
    y += line;

    DrawText(TextFormat("Avg: %.2f ms", perfAverageFrameMs), x, y, 16, WHITE);
    y += line;

    DrawText(TextFormat("Worst: %.2f ms", perfWorstFrameMs), x, y, 16, ORANGE);
    y += line + 6;

    DrawText(TextFormat("Update: %.2f ms", perfUpdateTotalMs), x, y, 16, WHITE);
    y += line;

    DrawText(TextFormat("Draw: %.2f ms", perfDrawTotalMs), x, y, 16, WHITE);
    y += line;

    Color spikeColor = perfSpikeHoldTimer > 0.0f ? RED : LIGHTGRAY;

    DrawText(TextFormat("Spike Reason: %s", perfLastSpikeReason), x, y, 15, spikeColor);
    y += line;

    DrawText(TextFormat("Spike Frame: %.2f ms", perfLastSpikeFrameMs), x, y, 15, spikeColor);
    y += line;

    DrawText(TextFormat("Spike Update: %.2f ms", perfLastSpikeUpdateMs), x, y, 15, spikeColor);
    y += line;

    DrawText(TextFormat("Spike Draw: %.2f ms", perfLastSpikeDrawMs), x, y, 15, spikeColor);
    y += line;

    DrawText(TextFormat("Spike Combat: %.2f ms", perfLastSpikeCombatMs), x, y, 15, spikeColor);
    y += line + 6;

    DrawText(TextFormat("Draw Ground: %.2f", perfDrawGroundMs), x, y, 15, LIGHTGRAY);
    y += line;

    DrawText(TextFormat("Draw Combat: %.2f", perfDrawCombatWorldMs), x, y, 15, LIGHTGRAY);
    y += line;

    DrawText(TextFormat("Draw Player: %.2f", perfDrawPlayerMs), x, y, 15, LIGHTGRAY);
    y += line;

    DrawText(TextFormat("Draw UI: %.2f", perfDrawUiMs + perfDrawHudMs + perfDrawSkillUiMs + perfDrawControlsMs), x, y, 15, LIGHTGRAY);
    y += line;

    DrawText(TextFormat("Spike Draw Section: %s", perfLastSpikeDrawSection), x, y, 15, spikeColor);
    y += line + 6;

    DrawText(TextFormat("Combat Draw Spike: %s", perfLastSpikeCombatDrawSection), x, y, 15, spikeColor);
    y += line;

    DrawText(TextFormat("Spike CD Projectiles: %.2f", perfLastSpikeCDProjectilesMs), x, y, 15, LIGHTGRAY);
    y += line;

    DrawText(TextFormat("Spike CD Enemies: %.2f", perfLastSpikeCDEnemiesMs), x, y, 15, LIGHTGRAY);
    y += line;

    DrawText(TextFormat("Spike CD Blades: %.2f", perfLastSpikeCDBladesMs), x, y, 15, LIGHTGRAY);
    y += line;

    DrawText(TextFormat("Spike CD DongfengTele: %.2f", perfLastSpikeCDDongfengTelegraphMs), x, y, 15, LIGHTGRAY);
    y += line;

    DrawText(TextFormat("Spike CD DongfengWave: %.2f", perfLastSpikeCDDongfengWaveMs), x, y, 15, LIGHTGRAY);
    y += line;

    DrawText(TextFormat("Spike CD VFX: %.2f", perfLastSpikeCDVfxMs), x, y, 15, LIGHTGRAY);
    y += line + 6;
    y += line + 6;

    DrawText(TextFormat("Enemies: %d", static_cast<int>(enemies.size())), x, y, 16, WHITE);
    y += line;

    DrawText(TextFormat("Projectiles: %d", static_cast<int>(projectiles.size())), x, y, 16, WHITE);
    y += line;

    DrawText(TextFormat("VFX: %d", static_cast<int>(vfxParticles.size())), x, y, 16, WHITE);
    y += line + 6;

    DrawText(TextFormat("Combat: %.2f ms", perfCombatMs), x, y, 16, WHITE);
    y += line;

    DrawText(TextFormat("Skills: %.2f", perfSkillsMs), x, y, 15, LIGHTGRAY);
    y += line;

    DrawText(TextFormat("Huashan: %.2f", perfHuashanMs), x, y, 15, LIGHTGRAY);
    y += line;

    DrawText(TextFormat("Dongfeng: %.2f", perfDongfengMs), x, y, 15, LIGHTGRAY);
    y += line;

    DrawText(TextFormat("Wave: %.2f", perfWaveMs), x, y, 15, LIGHTGRAY);
    y += line;

    DrawText(TextFormat("Enemies: %.2f", perfEnemiesMs), x, y, 15, LIGHTGRAY);
    y += line;

    DrawText(TextFormat("Melee: %.2f", perfMeleeMs), x, y, 15, LIGHTGRAY);
    y += line;

    DrawText(TextFormat("Blades: %.2f", perfBladesMs), x, y, 15, LIGHTGRAY);
    y += line;

    DrawText(TextFormat("Projectiles: %.2f", perfProjectilesMs), x, y, 15, LIGHTGRAY);
    y += line;

    DrawText(TextFormat("Collisions: %.2f", perfCollisionMs), x, y, 15, LIGHTGRAY);
    y += line;

    DrawText(TextFormat("VFX: %.2f", perfVfxMs), x, y, 15, LIGHTGRAY);
    y += line;

    DrawText(TextFormat("Cleanup: %.2f", perfCleanupMs), x, y, 15, LIGHTGRAY);
}


void Game::StartWave(int waveNumber)
{
    // Compatibility entry point used by the existing upgrade code.
    // Chamber progression owns the actual encounter state.
    StartChamberWave(
        activeChamberId,
        waveNumber
    );
}

void Game::StartChamberWave(
    int chamberId,
    int chamberWave
)
{
    DungeonChamber* chamber =
        FindChamberById(
            chamberId
        );

    if (
        chamber == nullptr ||
        chamber->cleared ||
        chamberWave < 1 ||
        chamberWave > chamber->wavesRequired
        )
    {
        return;
    }

    int chamberOrder = 0;

    for (const DungeonChamber& candidate : chambers)
    {
        if (!candidate.hasCells)
        {
            continue;
        }

        if (candidate.id == chamberId)
        {
            break;
        }

        chamberOrder++;
    }

    chamber->encounterStarted = true;
    chamber->currentWave = chamberWave;

    wave.chamberId = chamberId;
    wave.chamberWave = chamberWave;
    wave.chamberWavesRequired = chamber->wavesRequired;

    // Retain the original wave-based stat scaling without making a
    // chamber ID such as 100 create absurd enemy health.
    wave.wave =
        1 +
        chamberOrder * chamber->wavesRequired +
        (chamberWave - 1);

    wave.enemiesSpawned = 0;
    wave.enemiesToSpawn =
        5 +
        std::min(
            chamberOrder,
            4
        ) +
        (chamberWave - 1) * 2;

    wave.spawnTimer = 0.0f;
    wave.spawnInterval =
        std::max(
            0.32f,
            0.72f -
            static_cast<float>(chamberOrder) * 0.035f -
            static_cast<float>(chamberWave - 1) * 0.06f
        );

    wave.waveActive = true;
    wave.waitingForNextWave = true;
    wave.nextWaveTimer =
        chamberWave == 1
        ? chamberWaveStartDelay
        : 1.35f;

    gameState = GameState::Playing;

    TraceLog(
        LOG_INFO,
        "[CHAMBER] Chamber %d starting wave %d/%d with %d enemies.",
        chamberId,
        chamberWave,
        chamber->wavesRequired,
        wave.enemiesToSpawn
    );
}

void Game::UpdateWave(float dt)
{
    if (!wave.waveActive)
    {
        return;
    }

    DungeonChamber* chamber =
        FindChamberById(
            wave.chamberId
        );

    if (
        chamber == nullptr ||
        chamber->cleared ||
        wave.chamberId != activeChamberId
        )
    {
        wave.waveActive = false;
        wave.waitingForNextWave = false;
        return;
    }

    if (wave.waitingForNextWave)
    {
        wave.nextWaveTimer -= dt;

        if (wave.nextWaveTimer > 0.0f)
        {
            return;
        }

        wave.nextWaveTimer = 0.0f;
        wave.waitingForNextWave = false;
    }

    if (
        !waveSpawningPaused &&
        wave.enemiesSpawned < wave.enemiesToSpawn
        )
    {
        wave.spawnTimer += dt;

        if (wave.spawnTimer >= wave.spawnInterval)
        {
            wave.spawnTimer = 0.0f;

            const int roll =
                GetRandomValue(
                    1,
                    100
                );

            EnemyType type =
                EnemyType::Grunt;

            if (roll <= 45)
            {
                type = EnemyType::Grunt;
            }
            else if (roll <= 68)
            {
                type = EnemyType::Runner;
            }
            else if (roll <= 88)
            {
                type = EnemyType::Shooter;
            }
            else
            {
                type = EnemyType::Tank;
            }

            SpawnEnemyInChamber(
                type,
                chamber->id
            );

            wave.enemiesSpawned++;
        }
    }

    const bool finishedSpawning =
        wave.enemiesSpawned >=
        wave.enemiesToSpawn;

    if (!finishedSpawning)
    {
        return;
    }

    if (
        HasLivingEnemiesInChamber(
            chamber->id
        ) ||
        HasPendingEnemiesInChamber(
            chamber->id
        )
        )
    {
        return;
    }

    chamber->wavesCompleted =
        std::max(
            chamber->wavesCompleted,
            chamber->currentWave
        );

    if (
        chamber->wavesCompleted <
        chamber->wavesRequired
        )
    {
        StartChamberWave(
            chamber->id,
            chamber->wavesCompleted + 1
        );

        return;
    }

    chamber->cleared = true;
    chamber->currentWave =
        chamber->wavesRequired;

    wave.waveActive = false;
    wave.waitingForNextWave = false;

    chamberClearedMessageTimer = 3.5f;

    currentPath.clear();
    pathIndex = 0;
    hasPath = false;

    TraceLog(
        LOG_INFO,
        "[CHAMBER] Chamber %d (%s) cleared. Exit unlocked.",
        chamber->id,
        chamber->name.c_str()
    );
}

Vector2 Game::GetRandomSpawnPosition() const
{
    return
        GetRandomSpawnPositionInChamber(
            activeChamberId
        );
}

Vector2 Game::GetRandomSpawnPositionInChamber(
    int chamberId
) const
{
    for (int attempt = 0; attempt < 48; ++attempt)
    {
        const float distance =
            static_cast<float>(
                GetRandomValue(
                    360,
                    720
                )
                );

        const float angle =
            static_cast<float>(
                GetRandomValue(
                    0,
                    359
                )
                ) *
            DEG2RAD;

        const Vector2 candidate{
            playerPosition.x +
                cosf(angle) * distance,

            playerPosition.y +
                sinf(angle) * distance
        };

        int cellX = 0;
        int cellY = 0;

        if (
            !WorldToCell(
                candidate,
                cellX,
                cellY
            ) ||
            IsCellBlocked(
                cellX,
                cellY
            ) ||
            terrainCells[
                CellIndex(
                    cellX,
                    cellY
                )
            ].chamberId != chamberId
            )
        {
            continue;
        }

        std::vector<Vector2> connectionPath;

        if (
            Vector2Distance(
                candidate,
                playerPosition
            ) >= TileSize * 2.5f &&
            FindPath(
                candidate,
                playerPosition,
                connectionPath
            )
            )
        {
            return candidate;
        }
    }

    Vector2 bestPosition =
        playerPosition;

    float bestDistanceSquared =
        -1.0f;

    for (int y = 0; y < MapHeight; ++y)
    {
        for (int x = 0; x < MapWidth; ++x)
        {
            if (
                !IsCellEnabled(
                    x,
                    y
                ) ||
                IsCellBlocked(
                    x,
                    y
                ) ||
                terrainCells[
                    CellIndex(
                        x,
                        y
                    )
                ].chamberId != chamberId
                )
            {
                continue;
            }

            const Vector2 candidate =
                CellToWorld(
                    x,
                    y
                );

            std::vector<Vector2> connectionPath;

            if (
                chamberId == activeChamberId &&
                !FindPath(
                    candidate,
                    playerPosition,
                    connectionPath
                )
                )
            {
                continue;
            }

            const float distanceSquared =
                DistanceSquared(
                    candidate,
                    playerPosition
                );

            if (
                distanceSquared >
                bestDistanceSquared
                )
            {
                bestDistanceSquared =
                    distanceSquared;

                bestPosition =
                    candidate;
            }
        }
    }

    return bestPosition;
}

void Game::SpawnTestBoss()
{
    const std::size_t previousEnemyCount =
        enemies.size();

    SpawnEnemy(
        EnemyType::Boss
    );

    if (
        enemies.size() <=
        previousEnemyCount
        )
    {
        TraceLog(
            LOG_WARNING,
            "[BOSS TEST] Boss could not be spawned."
        );

        return;
    }

    Enemy& boss =
        enemies.back();

    const Vector2 testOffsets[] = {
        { 300.0f, 0.0f },
        { -300.0f, 0.0f },
        { 0.0f, 300.0f },
        { 0.0f, -300.0f },

        { 220.0f, 220.0f },
        { -220.0f, 220.0f },
        { 220.0f, -220.0f },
        { -220.0f, -220.0f }
    };

    bool placedNearPlayer =
        false;

    for (
        const Vector2 offset :
    testOffsets
        )
    {
        Vector2 requestedPosition =
            Vector2Add(
                playerPosition,
                offset
            );

        int cellX = 0;
        int cellY = 0;

        if (
            !FindNearestWalkableCell(
                requestedPosition,
                cellX,
                cellY
            )
            )
        {
            continue;
        }

        Vector2 candidatePosition =
            CellToWorld(
                cellX,
                cellY
            );

        float collisionRadius =
            std::max(
                6.0f,
                boss.radius * 0.55f
            );

        if (
            !CanEnemyStandAt(
                candidatePosition,
                candidatePosition,
                collisionRadius
            )
            )
        {
            continue;
        }

        std::vector<Vector2>
            connectionPath;

        if (
            !FindPath(
                candidatePosition,
                playerPosition,
                connectionPath
            )
            )
        {
            continue;
        }

        boss.pos =
            candidatePosition;

        boss.previousAnimationPosition =
            candidatePosition;

        boss.animationPositionInitialized =
            true;

        boss.bossPreviousAnimationPosition =
            candidatePosition;

        boss.bossAnimationPositionInitialized =
            true;

        boss.animationState =
            EnemyAnimationState::Idle;

        boss.bossAnimationFrame = 0;
        boss.bossAnimationTimer = 0.0f;

        SetEnemyFacingFromWorldDirection(
            boss,
            Vector2Subtract(
                playerPosition,
                boss.pos
            )
        );

        RefreshEnemyPath(
            boss
        );

        placedNearPlayer =
            true;

        break;
    }

    TraceLog(
        LOG_INFO,
        "[BOSS TEST] Spawned Boss id=%d nearPlayer=%d "
        "position=(%.1f, %.1f)",
        boss.id,
        placedNearPlayer ? 1 : 0,
        boss.pos.x,
        boss.pos.y
    );
}

bool Game::IsEnemySpawnProtected(
    const Enemy& enemy
) const
{
    if (!enemy.active)
    {
        return false;
    }

    // The enemy is protected only before it has
    // completely emerged from the ground.
    return
        enemy.spawnState ==
        EnemySpawnState::GroundEffect ||
        enemy.spawnState ==
        EnemySpawnState::Emerging;
}

float Game::GetEnemySpawnDepth(
    const Enemy& enemy
) const
{
    float spriteVisualHeight =
        enemy.radius *
        2.0f;

    if (
        enemy.type ==
        EnemyType::Shooter
        )
    {
        spriteVisualHeight =
            static_cast<float>(
                shooterFrameHeight
                ) *
            shooterVisualScale;
    }
    else if (
        ShouldUseBlobEnemySprite(
            enemy
        )
        )
    {
        spriteVisualHeight =
            GetBlobEnemyVisualSize(
                enemy
            );
    }

    // Upright Hybrid billboards are physically enlarged to
    // compensate for the angled camera projection.
    //
    // The previous 0.78/0.88 multipliers did not place the
    // complete displayed sprite below the terrain.
    const float projectionCompensation =
        1.75f;

    // Extra depth also accounts for transparent padding
    // surrounding the character inside its sprite frame.
    const float extraBurialDepth =
        28.0f;

    return
        spriteVisualHeight *
        projectionCompensation +
        extraBurialDepth;
}

float Game::GetEnemySpawnVisualOffset(
    const Enemy& enemy
) const
{
    if (
        enemy.spawnState !=
        EnemySpawnState::Emerging
        )
    {
        return 0.0f;
    }

    const float progress =
        Clamp(
            enemy.spawnStateTimer /
            std::max(
                0.01f,
                enemySpawnEmergenceDuration
            ),
            0.0f,
            1.0f
        );

    // Smooth start and finish, but unlike the previous
    // cubic ease-out it does not reveal most of the enemy
    // immediately.
    const float smoothProgress =
        progress *
        progress *
        (
            3.0f -
            2.0f *
            progress
            );

    const float buriedDepth =
        GetEnemySpawnDepth(
            enemy
        );

    // At progress 0:
    //     offset = -buriedDepth
    //
    // At progress 1:
    //     offset = 0
    return
        -buriedDepth *
        (
            1.0f -
            smoothProgress
            );
}

float Game::GetEnemySpawnFlashAmount(
    const Enemy& enemy
) const
{
    // Keep the enemy completely tinted for the
    // entire underground rising animation.
    if (
        enemy.spawnState ==
        EnemySpawnState::Emerging
        )
    {
        return 1.0f;
    }

    // Once fully emerged and active, gradually
    // return to the enemy's original colours.
    if (
        enemy.spawnState ==
        EnemySpawnState::FadeOut
        )
    {
        const float progress =
            Clamp(
                enemy.spawnStateTimer /
                std::max(
                    0.01f,
                    enemySpawnFadeDuration
                ),
                0.0f,
                1.0f
            );

        const float smoothProgress =
            progress *
            progress *
            (
                3.0f -
                2.0f *
                progress
                );

        return
            1.0f -
            smoothProgress;
    }

    return 0.0f;
}

Rectangle Game::GetEnemySpawnEffectSourceRect(
    int frame
) const
{
    const int safeColumns =
        std::max(
            1,
            enemySpawnEffectColumns
        );

    const int safeFrameCount =
        std::max(
            1,
            enemySpawnEffectFrameCount
        );

    frame =
        std::max(
            0,
            std::min(
                frame,
                safeFrameCount - 1
            )
        );

    const int column =
        frame %
        safeColumns;

    const int row =
        frame /
        safeColumns;

    return Rectangle{
        static_cast<float>(
            column *
            enemySpawnEffectFrameWidth
        ),

        static_cast<float>(
            row *
            enemySpawnEffectFrameHeight
        ),

        static_cast<float>(
            enemySpawnEffectFrameWidth
        ),

        static_cast<float>(
            enemySpawnEffectFrameHeight
        )
    };
}

void Game::UpdateEnemySpawnState(
    Enemy& enemy,
    float dt
)
{
    if (
        enemy.spawnState ==
        EnemySpawnState::Ready
        )
    {
        return;
    }

    enemy.spawnStateTimer +=
        dt;

    // --------------------------------------------------
    // GroundEffect and Emerging are protected phases.
    //
    // Enemy remains stationary, immune and unable to act.
    // --------------------------------------------------

    if (
        enemy.spawnState ==
        EnemySpawnState::GroundEffect ||
        enemy.spawnState ==
        EnemySpawnState::Emerging
        )
    {
        enemy.path.clear();
        enemy.pathIndex = 0;
        enemy.pathRefreshTimer = 0.0f;

        enemy.knockbackVelocity = {
            0.0f,
            0.0f
        };

        enemy.animationState =
            EnemyAnimationState::Idle;
    }

    // --------------------------------------------------
    // Circle animation completed.
    // Hold its final frame and begin rising.
    // --------------------------------------------------

    if (
        enemy.spawnState ==
        EnemySpawnState::GroundEffect &&
        enemy.spawnStateTimer >=
        enemySpawnGroundEffectDuration
        )
    {
        enemy.spawnStateTimer -=
            enemySpawnGroundEffectDuration;

        enemy.spawnState =
            EnemySpawnState::Emerging;

        enemy.animationState =
            EnemyAnimationState::Idle;

        enemy.shooterAnimationFrame =
            0;

        enemy.shooterAnimationTimer =
            0.0f;
    }

    // --------------------------------------------------
    // Enemy has fully emerged.
    //
    // It becomes active immediately, while the circle
    // and bright tint begin fading.
    // --------------------------------------------------

    if (
        enemy.spawnState ==
        EnemySpawnState::Emerging &&
        enemy.spawnStateTimer >=
        enemySpawnEmergenceDuration
        )
    {
        enemy.spawnStateTimer -=
            enemySpawnEmergenceDuration;

        enemy.spawnState =
            EnemySpawnState::FadeOut;

        enemy.previousAnimationPosition =
            enemy.pos;

        enemy.animationPositionInitialized =
            true;

        enemy.path.clear();
        enemy.pathIndex = 0;
        enemy.pathRefreshTimer = 0.0f;

        RefreshEnemyPath(
            enemy
        );
    }

    // --------------------------------------------------
    // Visual fade has completed.
    // --------------------------------------------------

    if (
        enemy.spawnState ==
        EnemySpawnState::FadeOut &&
        enemy.spawnStateTimer >=
        enemySpawnFadeDuration
        )
    {
        enemy.spawnState =
            EnemySpawnState::Ready;

        enemy.spawnStateTimer =
            0.0f;
    }
}


void Game::SpawnEnemy(EnemyType type)
{
    SpawnEnemyInChamber(
        type,
        activeChamberId
    );
}

void Game::SpawnEnemyInChamber(
    EnemyType type,
    int chamberId
)
{
    if (enemies.size() >= 80)
    {
        return;
    }

    Enemy enemy;
    enemy.id = nextEnemyId++;
    enemy.chamberId = chamberId;
    enemy.type = type;
    enemy.pos =
        GetRandomSpawnPositionInChamber(
            chamberId
        );
    enemy.active = true;

    if (type == EnemyType::Grunt)
    {
        enemy.radius = 24.0f;
        enemy.speed = 95.0f;
        enemy.hp = 24 + wave.wave * 5;
        enemy.maxHp = enemy.hp;
        enemy.contactDamage = 5 + wave.wave;
        enemy.attackInterval = 0.95f;
        enemy.weight = 1.0f;
        enemy.knockbackResistance = 1.0f;
    }
    else if (type == EnemyType::Runner)
    {
        enemy.radius = 20.0f;
        enemy.speed = 145.0f;
        enemy.hp = 16 + wave.wave * 4;
        enemy.maxHp = enemy.hp;
        enemy.contactDamage = 6 + wave.wave;
        enemy.attackInterval = 0.75f;
        enemy.pathRefreshInterval = 0.22f;
        enemy.weight = 0.70f;
        enemy.knockbackResistance = 0.85f;
    }
    else if (type == EnemyType::Tank)
    {
        enemy.radius = 36.0f;
        enemy.speed = 65.0f;
        enemy.hp = 60 + wave.wave * 12;
        enemy.maxHp = enemy.hp;
        enemy.contactDamage = 10 + wave.wave * 2;
        enemy.attackInterval = 1.20f;
        enemy.pathRefreshInterval = 0.50f;
        enemy.weight = 1.80f;
        enemy.knockbackResistance = 1.50f;
    }
    else if (type == EnemyType::Shooter)
    {

        enemy.shooterShotsPerAttack = 2;

        enemy.radius = 23.0f;
        enemy.speed = 85.0f;
        enemy.hp = 28 + wave.wave * 6;
        enemy.maxHp = enemy.hp;
        enemy.contactDamage = 5 + wave.wave;
        enemy.attackInterval = 1.10f;

        enemy.shootRange = 620.0f;
        enemy.shootInterval = 1.65f;
        enemy.bulletDamage = 6 + wave.wave;
        enemy.bulletSpeed = 360.0f + (30 * wave.wave);
        enemy.weight = 0.90f;
        enemy.knockbackResistance = 1.0f;
    }
    else if (type == EnemyType::Boss)
    {
        enemy.radius = 62.0f;
        enemy.speed = 72.0f;

        enemy.hp =
            800 +
            wave.wave * 80;

        enemy.maxHp =
            enemy.hp;

        // Used by the circular slam.
        enemy.contactDamage =
            22 +
            wave.wave * 3;

        enemy.attackInterval =
            1.10f;

        // Used by the laser and phase projectiles.
        enemy.bulletDamage =
            12 +
            wave.wave * 2;

        enemy.bulletSpeed =
            390.0f;

        enemy.weight = 5.0f;
        enemy.knockbackResistance = 4.0f;

        enemy.bossActionState =
            BossActionState::None;

        enemy.bossPhase75Triggered =
            false;

        enemy.bossPhase25Triggered =
            false;

        // Prevent an immediate laser when the Boss
        // first crosses below 50%.
        enemy.bossLaserCooldownTimer =
            2.0f;
    }

    if (smallEnemyFramesPerRow > 0)
    {
        enemy.spriteFrame =
            GetRandomValue(
                0,
                smallEnemyFramesPerRow - 1
            );
    }
    else
    {
        enemy.spriteFrame = 0;
    }

    enemy.spriteAnimTimer =
        static_cast<float>(
            GetRandomValue(0, 1000)
            ) /
        1000.0f *
        smallEnemyFrameDuration;

    enemy.spriteDirection =
        PlayerDirection::Down;

    enemy.previousAnimationPosition =
        enemy.pos;

    enemy.animationPositionInitialized =
        true;

    if (
        type ==
        EnemyType::Boss
        )
    {
        enemy.animationState =
            EnemyAnimationState::Idle;

        if (bossIdleFramesPerRow > 0)
        {
            enemy.bossAnimationFrame =
                GetRandomValue(
                    0,
                    bossIdleFramesPerRow - 1
                );
        }
        else
        {
            enemy.bossAnimationFrame = 0;
        }

        enemy.bossAnimationTimer =
            static_cast<float>(
                GetRandomValue(
                    0,
                    1000
                )
                ) /
            1000.0f *
            bossIdleFrameDuration;

        enemy.bossPreviousAnimationPosition =
            enemy.pos;

        enemy.bossAnimationPositionInitialized =
            true;
    }

    // Normal enemies use the underground spawning sequence.
    // Bosses enter combat immediately.
    if (
        type !=
        EnemyType::Boss
        )
    {
        enemy.spawnState =
            EnemySpawnState::GroundEffect;

        enemy.spawnStateTimer =
            0.0f;

        enemy.animationState =
            EnemyAnimationState::Idle;

        enemy.path.clear();
        enemy.pathIndex = 0;
    }
    else
    {
        enemy.spawnState =
            EnemySpawnState::Ready;

        enemy.spawnStateTimer =
            0.0f;

        RefreshEnemyPath(
            enemy
        );
    }

    enemies.push_back(
        enemy
    );
}

void Game::UpdateEnemies(float dt)
{
    for (Enemy& enemy : enemies)
    {
        if (!enemy.active)
        {
            continue;
        }

        if (
            enemy.chamberId >= 0 &&
            enemy.chamberId != activeChamberId
            )
        {
            continue;
        }

        // --------------------------------------------------
        // Normal enemy spawning sequence
        //
        // Spawning enemies cannot move, attack, receive physics,
        // or interact with the player.
        // --------------------------------------------------

// Update all visual spawn phases, including FadeOut.
        if (
            enemy.spawnState !=
            EnemySpawnState::Ready
            )
        {
            UpdateEnemySpawnState(
                enemy,
                dt
            );
        }

        // Protected enemies cannot move, attack or receive damage.
        if (
            IsEnemySpawnProtected(
                enemy
            )
            )
        {
            UpdateSmallEnemyAnimation(
                enemy,
                dt
            );

            continue;
        }

        // FadeOut reaches this point, so the fully emerged enemy
        // can move, attack and receive damage while its spawn
        // visuals fade away.

        UpdateEnemyReactionTimers(
            enemy,
            dt
        );

        ApplyEnemyPhysics(
            enemy,
            dt
        );

        UpdateSmallEnemyAnimation(
            enemy,
            dt
        );

        const int enemyTerrainElevation =
            GetTerrainElevationAtWorld(
                enemy.pos
            );

        const int playerTerrainElevation =
            GetTerrainElevationAtWorld(
                playerPosition
            );

        const bool sameTerrainLevel =
            enemyTerrainElevation ==
            playerTerrainElevation;

        const float distanceToPlayer =
            Vector2Distance(
                enemy.pos,
                playerPosition
            );

        // --------------------------------------------------
        // Boss has its own complete behaviour controller.
        // It handles:
        // - idle/walking animation
        // - ground slam
        // - continuous laser
        // - 75% and 25% phase jumps
        // --------------------------------------------------

        if (
            enemy.type ==
            EnemyType::Boss
            )
        {
            UpdateBossBehavior(
                enemy,
                dt,
                distanceToPlayer,
                sameTerrainLevel
            );

            continue;
        }

        // --------------------------------------------------
        // Other enemies
        // --------------------------------------------------

        if (
            IsEnemyCrowdControlled(
                enemy
            )
            )
        {
            continue;
        }

        if (
            enemy.type ==
            EnemyType::Shooter
            )
        {
            UpdateEnemyShooter(
                enemy,
                dt,
                distanceToPlayer
            );

            continue;
        }

        // --------------------------------------------------
        // Normal contact attack for Grunt, Runner and Tank.
        // The Boss must not use this section.
        // --------------------------------------------------

        const float enemyAttackRange =
            enemy.radius +
            player.radius +
            8.0f;

        if (
            sameTerrainLevel &&
            distanceToPlayer <=
            enemyAttackRange
            )
        {
            enemy.attackTimer +=
                dt;

            if (
                enemy.attackTimer >=
                enemy.attackInterval
                )
            {
                enemy.attackTimer =
                    0.0f;

                if (
                    !IsPlayerAirborne() &&
                    !IsPlayerInvulnerable()
                    )
                {
                    DamagePlayer(
                        enemy.contactDamage
                    );
                }
            }

            continue;
        }

        // --------------------------------------------------
        // Normal path movement
        // --------------------------------------------------

        enemy.pathRefreshTimer +=
            dt;

        const bool pathFinished =
            !enemy.path.empty() &&
            enemy.pathIndex >=
            static_cast<int>(
                enemy.path.size()
                );

        if (
            enemy.pathRefreshTimer >=
            enemy.pathRefreshInterval ||
            pathFinished
            )
        {
            RefreshEnemyPath(
                enemy
            );
        }

        MoveEnemyAlongPath(
            enemy,
            dt
        );
    }

    // Boss healing minions were queued while the enemy
    // vector was being iterated. Add them only after the
    // loop has finished.
    ProcessPendingEnemySpawns();

    ResolveEnemySeparation();
}

void Game::ResolveEnemySeparation()
{
    constexpr float separationScale =
        0.62f;


    auto IsSeparationMovementLocked =
        [this](
            const Enemy& enemy
            )
        {
            const bool shootingLocked =
                enemy.type ==
                EnemyType::Shooter &&
                enemy.animationState ==
                EnemyAnimationState::Attacking;

            const bool crowdControlLocked =
                enemy.stunTimer > 0.0f ||
                enemy.landingStunTimer > 0.0f ||
                enemy.frozenTimer > 0.0f;

            // Prevent enemy separation from moving the Boss
            // while it performs a slam, laser, or phase jump.
            const bool bossLocked =
                enemy.type ==
                EnemyType::Boss &&
                (
                    enemy.bossActionState !=
                    BossActionState::None ||

                    enemy.bossDashCharging ||

                    enemy.bossDashTimer >
                    0.0f
                    );
            const bool spawnLocked =
                IsEnemySpawnProtected(
                    enemy
                );

            return
                spawnLocked ||
                shootingLocked ||
                bossLocked ||
                crowdControlLocked;
        };


    for (
        int firstIndex = 0;
        firstIndex <
        static_cast<int>(
            enemies.size()
            );
        ++firstIndex
        )
    {
        Enemy& first =
            enemies[firstIndex];

        if (
            !first.active ||
            first.chamberId != activeChamberId
            )
        {
            continue;
        }

        for (
            int secondIndex =
            firstIndex + 1;
            secondIndex <
            static_cast<int>(
                enemies.size()
                );
            ++secondIndex
            )
        {
            Enemy& second =
                enemies[secondIndex];

            if (
                !second.active ||
                second.chamberId != activeChamberId
                )
            {
                continue;
            }

            if (
                GetTerrainElevationAtWorld(
                    first.pos
                ) !=
                GetTerrainElevationAtWorld(
                    second.pos
                )
                )
            {
                continue;
            }

            Vector2 difference =
                Vector2Subtract(
                    second.pos,
                    first.pos
                );

            float distance =
                Vector2Length(
                    difference
                );

            const float minimumDistance =
                (
                    first.radius +
                    second.radius
                    ) *
                separationScale;

            if (
                distance >=
                minimumDistance
                )
            {
                continue;
            }

            Vector2 direction{};

            if (distance <= 0.001f)
            {
                const float angle =
                    static_cast<float>(
                        (
                            first.id * 37 +
                            second.id * 53
                            ) %
                        360
                        ) *
                    DEG2RAD;

                direction = {
                    cosf(angle),
                    sinf(angle)
                };

                distance = 0.0f;
            }
            else
            {
                direction =
                    Vector2Scale(
                        difference,
                        1.0f /
                        distance
                    );
            }

            const bool firstMovementLocked =
                IsSeparationMovementLocked(
                    first
                );

            const bool secondMovementLocked =
                IsSeparationMovementLocked(
                    second
                );

            // Do not reposition either enemy when both
            // are intentionally locked in place.
            if (
                firstMovementLocked &&
                secondMovementLocked
                )
            {
                continue;
            }

            const float overlap =
                minimumDistance -
                distance;

            float firstCorrectionAmount =
                overlap *
                0.5f;

            float secondCorrectionAmount =
                overlap *
                0.5f;

            // The unlocked enemy receives the complete
            // separation correction.
            if (firstMovementLocked)
            {
                firstCorrectionAmount =
                    0.0f;

                secondCorrectionAmount =
                    overlap;
            }
            else if (secondMovementLocked)
            {
                firstCorrectionAmount =
                    overlap;

                secondCorrectionAmount =
                    0.0f;
            }

            const float firstRadius =
                std::max(
                    6.0f,
                    first.radius *
                    0.55f
                );

            const float secondRadius =
                std::max(
                    6.0f,
                    second.radius *
                    0.55f
                );

            if (
                firstCorrectionAmount >
                0.0001f
                )
            {
                const Vector2 firstCandidate =
                    Vector2Subtract(
                        first.pos,
                        Vector2Scale(
                            direction,
                            firstCorrectionAmount
                        )
                    );

                if (
                    CanEnemyStandAt(
                        first.pos,
                        firstCandidate,
                        firstRadius
                    )
                    )
                {
                    first.pos =
                        firstCandidate;
                }
            }

            if (
                secondCorrectionAmount >
                0.0001f
                )
            {
                const Vector2 secondCandidate =
                    Vector2Add(
                        second.pos,
                        Vector2Scale(
                            direction,
                            secondCorrectionAmount
                        )
                    );

                if (
                    CanEnemyStandAt(
                        second.pos,
                        secondCandidate,
                        secondRadius
                    )
                    )
                {
                    second.pos =
                        secondCandidate;
                }
            }
        }
    }
}

bool Game::CanEnemyStandAt(
    Vector2 fromPosition,
    Vector2 worldPosition,
    float collisionRadius
) const
{
    int targetX = 0;
    int targetY = 0;

    if (
        !WorldToCell(
            worldPosition,
            targetX,
            targetY
        )
        )
    {
        return false;
    }

    if (
        IsCellBlocked(
            targetX,
            targetY
        )
        )
    {
        return false;
    }

    return
        !IsTerrainCircleBlocked(
            fromPosition,
            worldPosition,
            collisionRadius
        );
}

void Game::SpawnEnemyProjectile(
    Vector2 startPos,
    Vector2 targetPos,
    int damage,
    float speed,
    float radius,
    float visualHeight
)
{
    if (projectiles.size() >= 240)
    {
        return;
    }

    Vector2 direction = Vector2Subtract(targetPos, startPos);

    if (Vector2Length(direction) <= 0.01f)
    {
        return;
    }

    direction = Vector2Normalize(direction);

    Projectile projectile;
    projectile.owner = ProjectileOwner::Enemy;
    projectile.pos = startPos;
    projectile.velocity =
        Vector2Scale(
            direction,
            speed
        );

    projectile.terrainElevation =
        GetTerrainElevationAtWorld(
            startPos
        );
    projectile.visualHeight =
        std::max(
            0.0f,
            visualHeight
        );
    projectile.radius =
        radius;
    projectile.damage = damage;
    projectile.life = 3.2f;
    projectile.active = true;

    projectiles.push_back(projectile);
}

void Game::SpawnRadialEnemyProjectiles(Vector2 center, int count, int damage, float speed, float radius)
{
    if (count <= 0)
    {
        return;
    }

    for (int i = 0; i < count; ++i)
    {
        float angle = (static_cast<float>(i) / static_cast<float>(count)) * PI * 2.0f;

        Vector2 target = {
            center.x + cosf(angle) * 100.0f,
            center.y + sinf(angle) * 100.0f
        };

        SpawnEnemyProjectile(center, target, damage, speed, radius);
    }
}


void Game::StartBossDash(
    Enemy& enemy
)
{
    if (
        enemy.type !=
        EnemyType::Boss
        )
    {
        return;
    }

    if (
        enemy.bossDashCharging ||
        enemy.bossDashTimer > 0.0f
        )
    {
        return;
    }

    enemy.bossDashIsForward = false;

    enemy.bossDashPurpose =
        BossDashPurpose::PhaseEscape;

    // Interrupt the current attack.
    enemy.bossActionState =
        BossActionState::None;

    enemy.bossLaserWindupTimer =
        0.0f;

    enemy.bossLaserActiveTimer =
        0.0f;

    enemy.bossLaserDamageTimer =
        0.0f;

    enemy.animationState =
        EnemyAnimationState::Idle;

    enemy.bossAnimationFrame = 0;
    enemy.bossAnimationTimer = 0.0f;

    enemy.bossDashCharging = true;

    const float attackSpeedMultiplier =
        GetBossAttackSpeedMultiplier(
            enemy
        );

    enemy.bossDashChargeDuration =
        0.90f /
        attackSpeedMultiplier;

    enemy.bossDashChargeTimer =
        enemy.bossDashChargeDuration;

    enemy.path.clear();
    enemy.pathIndex = 0;

    VfxParticle warningCircle;

    warningCircle.type =
        VfxType::SkillCircle;

    warningCircle.pos =
        enemy.pos;

    warningCircle.radius =
        enemy.radius +
        42.0f;

    warningCircle.life =
        enemy.bossDashChargeDuration;

    warningCircle.maxLife =
        enemy.bossDashChargeDuration;

    warningCircle.color = {
        255,
        30,
        30,
        130
    };

    warningCircle.active = true;

    vfxParticles.push_back(
        warningCircle
    );
}


void Game::ShootProjectile(Vector2 startPos, Vector2 targetPos, int damage, float speed)
{
    if (projectiles.size() >= 200)
    {
        return;
    }

    Vector2 direction = Vector2Subtract(targetPos, startPos);

    if (Vector2Length(direction) <= 0.01f)
    {
        return;
    }

    direction = Vector2Normalize(direction);

    Projectile projectile;
    projectile.owner = ProjectileOwner::Player;
    projectile.pos = startPos;
    projectile.velocity =
        Vector2Scale(
            direction,
            speed
        );

    projectile.terrainElevation =
        GetTerrainElevationAtWorld(
            startPos
        );

    projectile.radius =
        7.0f;
    projectile.damage = damage;
    projectile.life = 2.0f;
    projectile.active = true;

    projectiles.push_back(projectile);
}

void Game::UpdateAutoAttack(float dt)
{
    player.attackTimer += dt;

    if (player.attackTimer < player.attackInterval)
    {
        return;
    }

    Enemy* target = FindNearestEnemy(playerPosition, player.attackAssistRange);

    if (target == nullptr)
    {
        return;
    }

    player.attackTimer = 0.0f;

    ShootProjectile(playerPosition, target->pos, player.attackDamage, 680.0f);
}

void Game::UpdateProjectiles(float dt)
{
    for (
        Projectile& projectile :
        projectiles
        )
    {
        if (!projectile.active)
        {
            continue;
        }

        Vector2 previousPosition =
            projectile.pos;

        Vector2 nextPosition =
            Vector2Add(
                projectile.pos,
                Vector2Scale(
                    projectile.velocity,
                    dt
                )
            );

        if (
            IsProjectileBlockedByWorld(
                previousPosition,
                nextPosition,
                projectile.terrainElevation
            )
            )
        {
            projectile.pos =
                nextPosition;

            projectile.active =
                false;

            SpawnHitSpark(
                nextPosition
            );

            continue;
        }

        projectile.pos =
            nextPosition;

        projectile.life -=
            dt;

        if (projectile.life <= 0.0f)
        {
            projectile.active =
                false;
        }
    }
}

void Game::CheckProjectileEnemyCollisions()
{
    for (
        Projectile& projectile :
        projectiles
        )
    {
        if (!projectile.active)
        {
            continue;
        }

        if (
            projectile.owner ==
            ProjectileOwner::Player
            )
        {
            for (Enemy& enemy : enemies)
            {
                if (
                    !enemy.active ||
                    IsEnemySpawnProtected(
                        enemy
                    )
                    )
                {
                    continue;
                }

                // A projectile cannot hit an actor
                // on another terrain level.
                if (
                    GetTerrainElevationAtWorld(
                        enemy.pos
                    ) !=
                    projectile.terrainElevation
                    )
                {
                    continue;
                }

                if (
                    CheckCollisionCircles(
                        projectile.pos,
                        projectile.radius,
                        enemy.pos,
                        enemy.radius
                    )
                    )
                {
                    projectile.active =
                        false;

                    ApplyDamageToEnemy(
                        enemy,
                        projectile.damage,
                        enemy.pos
                    );

                    break;
                }
            }
        }
        else if (
            projectile.owner ==
            ProjectileOwner::Enemy
            )
        {
            if (
                GetTerrainElevationAtWorld(
                    playerPosition
                ) !=
                projectile.terrainElevation
                )
            {
                continue;
            }

            if (IsPlayerAirborne())
            {
                continue;
            }

            if (
                CheckCollisionCircles(
                    projectile.pos,
                    projectile.radius,
                    playerPosition,
                    player.radius
                )
                )
            {
                projectile.active =
                    false;

                if (IsPlayerInvulnerable())
                {
                    SpawnHitSpark(
                        projectile.pos
                    );

                    continue;
                }

                DamagePlayer(
                    projectile.damage
                );
            }
        }
    }
}

void Game::CleanupCombatObjects()
{
    enemies.erase(
        std::remove_if(
            enemies.begin(),
            enemies.end(),
            [](const Enemy& enemy)
            {
                return !enemy.active;
            }
        ),
        enemies.end()
    );

    bossFallingRocks.erase(
        std::remove_if(
            bossFallingRocks.begin(),
            bossFallingRocks.end(),
            [](
                const BossFallingRock& rock
                )
            {
                return !rock.active;
            }
        ),
        bossFallingRocks.end()
    );

    projectiles.erase(
        std::remove_if(
            projectiles.begin(),
            projectiles.end(),
            [](const Projectile& projectile)
            {
                return !projectile.active;
            }
        ),
        projectiles.end()
    );

    vfxParticles.erase(
        std::remove_if(
            vfxParticles.begin(),
            vfxParticles.end(),
            [](const VfxParticle& particle)
            {
                return !particle.active;
            }
        ),
        vfxParticles.end()
    );
}

void Game::SpawnHitSpark(Vector2 pos)
{
    const int particleCount = 3;

    if (!CanSpawnVfx(particleCount))
    {
        return;
    }

    for (int i = 0; i < particleCount; ++i)
    {
        float angle = static_cast<float>(GetRandomValue(0, 360)) * DEG2RAD;
        float speed = static_cast<float>(GetRandomValue(80, 180));

        VfxParticle p;
        p.type = VfxType::HitSpark;
        p.pos = pos;
        p.velocity = { cosf(angle) * speed, sinf(angle) * speed };
        p.radius = static_cast<float>(GetRandomValue(3, 6));
        p.life = 0.18f;
        p.maxLife = 0.18f;
        p.color = { 255, 230, 80, 220 };
        p.active = true;

        vfxParticles.push_back(p);
    }
}

void Game::SpawnDamageNumber(Vector2 pos, int value)
{
    if (!CanSpawnVfx(1))
    {
        return;
    }

    VfxParticle p;
    p.type = VfxType::FloatingDamage;
    p.pos = Vector2Add(pos, { -8.0f, -18.0f });
    p.velocity = { 0.0f, -42.0f };
    p.radius = 0.0f;
    p.life = 0.55f;
    p.maxLife = 0.55f;
    p.value = value;
    p.color = { 255, 245, 170, 255 };
    p.active = true;

    vfxParticles.push_back(p);
}

void Game::SpawnDeathBurst(Vector2 pos)
{
    const int particleCount = 6;

    if (!CanSpawnVfx(particleCount))
    {
        return;
    }

    for (int i = 0; i < particleCount; ++i)
    {
        float angle = static_cast<float>(GetRandomValue(0, 360)) * DEG2RAD;
        float speed = static_cast<float>(GetRandomValue(120, 260));

        VfxParticle p;
        p.type = VfxType::DeathBurst;
        p.pos = pos;
        p.velocity = { cosf(angle) * speed, sinf(angle) * speed };
        p.radius = static_cast<float>(GetRandomValue(5, 10));
        p.life = 0.28f;
        p.maxLife = 0.28f;
        p.color = { 255, 90, 45, 210 };
        p.active = true;

        vfxParticles.push_back(p);
    }
}

void Game::SpawnLightningLine(Vector2 start, Vector2 end)
{
    if (!CanSpawnVfx(1))
    {
        return;
    }

    VfxParticle p;
    p.type = VfxType::LightningLine;
    p.pos = start;
    p.endPos = end;
    p.radius = 5.0f;
    p.life = 0.14f;
    p.maxLife = 0.14f;
    p.color = { 190, 235, 255, 230 };
    p.active = true;

    vfxParticles.push_back(p);
}

void Game::UpdateVfx(float dt)
{
    for (VfxParticle& p : vfxParticles)
    {
        if (!p.active)
        {
            continue;
        }

        p.life -= dt;

        if (p.life <= 0.0f)
        {
            p.active = false;
            continue;
        }

        p.pos = Vector2Add(p.pos, Vector2Scale(p.velocity, dt));
        p.velocity = Vector2Scale(p.velocity, 0.92f);
    }
}

void Game::UpdateSkillButtonRects()
{
    float size = 56.0f;
    float gap = 12.0f;

    float screenW = static_cast<float>(GetScreenWidth());
    float screenH = static_cast<float>(GetScreenHeight());

    float totalW = size * 3.0f + gap * 2.0f;
    float startX = screenW * 0.5f - totalW * 0.5f;
    float y = screenH - 92.0f;

    for (int i = 0; i < 3; ++i)
    {
        skills[i].buttonRect = {
            startX + i * (size + gap),
            y,
            size,
            size
        };
    }
}

void Game::UpdateSkills(float dt)
{
    for (SkillSlot& skill : skills)
    {
        if (skill.cooldownRemaining > 0.0f)
        {
            skill.cooldownRemaining -= dt;

            if (skill.cooldownRemaining < 0.0f)
            {
                skill.cooldownRemaining = 0.0f;
            }
        }
    }
}

bool Game::TryActivateSkillAtScreen(Vector2 screenPos)
{
    for (SkillSlot& skill : skills)
    {
        if (!skill.unlocked)
        {
            continue;
        }

        if (CheckCollisionPointRec(screenPos, skill.buttonRect) &&
            skill.cooldownRemaining <= 0.0f)
        {
            ActivateSkill(skill.type);
            skill.cooldownRemaining = skill.cooldown;
            return true;
        }
    }

    return false;
}

void Game::ActivateHuashan()
{
    if (huashanJumpActive)
    {
        return;
    }

    int slotIndex = FindSkillSlotIndex(SkillType::Huashan);
    int level = (slotIndex >= 0) ? skills[slotIndex].level : 1;

    Enemy* target = FindNearestEnemy(playerPosition, 620.0f);

    if (target == nullptr)
    {
        return;
    }

    Vector2 landingPosition = playerPosition;

    if (!FindHuashanLandingPosition(*target, landingPosition))
    {
        // No valid landing spot around the target.
        // Still cast at the target, but land back at current position.
        landingPosition = playerPosition;
    }

    huashanImpactCenter = target->pos;

    float fullOldDamageRadius = 165.0f + static_cast<float>(level - 1) * 6.0f;

    if (fullOldDamageRadius > 240.0f)
    {
        fullOldDamageRadius = 240.0f;
    }

    // Damage radius stays reduced.
    huashanDamageRadius = fullOldDamageRadius * 0.50f;

    // Knockback radius remains large.
    huashanKnockbackRadius = fullOldDamageRadius + 95.0f;

    huashanMainDamage = 105 + level * 20;
    huashanSplashDamage = 48 + level * 10;

    huashanPrimaryKnockback = 2150.0f + level * 55.0f;
    huashanOuterKnockback = 1850.0f + level * 45.0f;

    huashanAirborneDuration = 0.72f + level * 0.015f;

    if (huashanAirborneDuration > 1.05f)
    {
        huashanAirborneDuration = 1.05f;
    }

    huashanLandingStunDuration = 1.50f;

    currentPath.clear();
    hasPath = false;
    pendingNpc = -1;

    huashanJumpActive = true;
    huashanJumpTimer = 0.0f;

    // Fixed airtime regardless of distance.
    huashanJumpDuration = 0.36f;

    huashanJumpStart = playerPosition;
    huashanJumpEnd = landingPosition;

    VfxParticle leapLine;
    leapLine.type = VfxType::SlashLine;
    leapLine.pos = playerPosition;
    leapLine.endPos = landingPosition;
    leapLine.radius = 22.0f;
    leapLine.life = 0.30f;
    leapLine.maxLife = 0.30f;
    leapLine.color = { 255, 45, 30, 255 };
    leapLine.active = true;
    vfxParticles.push_back(leapLine);
}

void Game::UpdateHuashan(float dt)
{
    if (!huashanJumpActive)
    {
        return;
    }

    huashanJumpTimer += dt;

    float t = huashanJumpTimer / huashanJumpDuration;

    if (t > 1.0f)
    {
        t = 1.0f;
    }

    float easedT = t * t * (3.0f - 2.0f * t);

    // Air movement ignores obstacles. The player is jumping over them.
    playerPosition = Vector2Lerp(huashanJumpStart, huashanJumpEnd, easedT);
    player.pos = playerPosition;

    if (huashanJumpTimer >= huashanJumpDuration)
    {
        huashanJumpActive = false;

        playerPosition = huashanJumpEnd;
        player.pos = playerPosition;

        ResolveHuashanImpact();
    }
}

bool Game::IsPlayerAirborne() const
{
    return
        huashanJumpActive ||
        playerKnockbackActive;
}

bool Game::IsPlayerMovementLocked() const
{
    if (playerKnockbackActive)
    {
        return true;
    }

    return
        huashanJumpActive ||
        dongfengCasting;
}

float Game::GetHuashanJumpHeight() const
{
    if (!huashanJumpActive || huashanJumpDuration <= 0.0f)
    {
        return 0.0f;
    }

    float t = huashanJumpTimer / huashanJumpDuration;

    if (t < 0.0f)
    {
        t = 0.0f;
    }

    if (t > 1.0f)
    {
        t = 1.0f;
    }

    return sinf(t * PI) * 125.0f;
}

float Game::GetPlayerVisualHeight() const
{
    float visualHeight =
        GetHuashanJumpHeight();

    if (
        playerKnockbackActive &&
        playerKnockbackMaxTimer > 0.0f
        )
    {
        const float progress =
            1.0f -
            Clamp(
                playerKnockbackTimer /
                playerKnockbackMaxTimer,
                0.0f,
                1.0f
            );

        const float knockbackHeight =
            sinf(
                progress *
                PI
            ) *
            playerKnockbackPeakHeight;

        visualHeight =
            std::max(
                visualHeight,
                knockbackHeight
            );
    }

    return visualHeight;
}

void Game::ResolveHuashanImpact()
{
    StartHuashanImpactAnimation(
        huashanImpactCenter
    );

    StartHuashanImpactFeedback();

    TraceLog(
        LOG_INFO,
        "[HUASHAN VFX] Impact started at %.1f, %.1f",
        huashanImpactCenter.x,
        huashanImpactCenter.y
    );

    // Existing placeholder VFX can remain below for now.
    VfxParticle outerCircle;
    outerCircle.type = VfxType::SkillCircle;
    outerCircle.pos = huashanImpactCenter;
    outerCircle.radius = huashanKnockbackRadius;
    outerCircle.life = 0.75f;
    outerCircle.maxLife = 0.75f;
    outerCircle.color = { 255, 70, 45, 85 };
    outerCircle.active = true;
    vfxParticles.push_back(outerCircle);

    VfxParticle damageCircle;
    damageCircle.type = VfxType::SkillCircle;
    damageCircle.pos = huashanImpactCenter;
    damageCircle.radius = huashanDamageRadius;
    damageCircle.life = 0.80f;
    damageCircle.maxLife = 0.80f;
    damageCircle.color = { 255, 20, 10, 180 };
    damageCircle.active = true;
    vfxParticles.push_back(damageCircle);

    //SpawnHitSpark(huashanImpactCenter);
    //SpawnDeathBurst(huashanImpactCenter);

    Enemy* mainTarget = FindNearestEnemy(huashanImpactCenter, huashanDamageRadius + 32.0f);

    for (Enemy& enemy : enemies)
    {
        if (!enemy.active)
        {
            continue;
        }

        float distance = Vector2Distance(enemy.pos, huashanImpactCenter);

        if (distance > huashanKnockbackRadius + enemy.radius)
        {
            continue;
        }

        bool insideDamageRadius = distance <= huashanDamageRadius + enemy.radius;
        bool isMainTarget = (&enemy == mainTarget);

        if (insideDamageRadius)
        {
            int damage = isMainTarget ? huashanMainDamage : huashanSplashDamage;
            ApplySkillDamageToEnemy(enemy, SkillType::Huashan, damage, enemy.pos);
        }

        if (!enemy.active)
        {
            continue;
        }

        // Inner radius: damage + stronger launch.
        // Outer radius: no damage, only launch + landing stun.
        float force = insideDamageRadius ? huashanPrimaryKnockback : huashanOuterKnockback;
        float airborne = insideDamageRadius ? huashanAirborneDuration : huashanAirborneDuration * 0.90f;

        SpawnHitSpark(enemy.pos);

        if (IsBossEnemy(enemy))
        {
            enemy.stunTimer = std::max(enemy.stunTimer, 0.35f);
            ApplyKnockbackToEnemy(enemy, huashanImpactCenter, force * 0.20f, 0.10f, 0.25f);
        }
        else
        {
            ApplyKnockbackToEnemy(
                enemy,
                huashanImpactCenter,
                force,
                airborne,
                huashanLandingStunDuration
            );
        }
    }
}

void Game::ActivateDongfeng()
{
    if (dongfengCasting || dongfengWaveActive)
    {
        return;
    }

    int slotIndex = FindSkillSlotIndex(SkillType::Dongfeng);
    int level = (slotIndex >= 0) ? skills[slotIndex].level : 1;

    Enemy* target = FindNearestEnemy(playerPosition, 950.0f);

    if (target == nullptr)
    {
        return;
    }

    Vector2 direction = Vector2Subtract(target->pos, playerPosition);

    if (Vector2Length(direction) <= 0.01f)
    {
        direction = { 1.0f, 0.0f };
    }

    direction = Vector2Normalize(direction);

    dongfengCasting = true;
    dongfengCastTimer = 0.0f;

    // Upgrade scaling:
    // Lv1 = 0.50s
    // Each level reduces cast by 0.025s
    // Minimum = 0.22s
    dongfengCastDuration = 0.50f - static_cast<float>(level - 1) * 0.1f;

    if (dongfengCastDuration < 0.18f)
    {
        dongfengCastDuration = 0.18f;
    }

    dongfengLockedTargetId = target->id;

    dongfengCastStart = playerPosition;
    dongfengCastDirection = direction;

    dongfengRange = 1250.0f + static_cast<float>(level - 1) * 28.0f;

    if (dongfengRange > 1650.0f)
    {
        dongfengRange = 1650.0f;
    }

    // Projectile speed scales by skill level.
    // Lower levels are intentionally slower.
    // Higher levels launch a faster crescent.
    dongfengWaveSpeed = 700.0f + static_cast<float>(level - 1) * 85.0f;

    if (dongfengWaveSpeed > 1700.0f)
    {
        dongfengWaveSpeed = 1700.0f;
    }

    // Upgrade scaling:
    // Initial crescent gets bigger with each upgrade.
    dongfengStartRadius = 44.0f + static_cast<float>(level - 1) * 5.0f;

    if (dongfengStartRadius > 115.0f)
    {
        dongfengStartRadius = 115.0f;
    }

    // Still grows as it travels.
    dongfengEndRadius = dongfengStartRadius + 96.0f + static_cast<float>(level - 1) * 3.0f;

    if (dongfengEndRadius > 220.0f)
    {
        dongfengEndRadius = 220.0f;
    }

    dongfengMainDamage = 240 + level * 42;
    dongfengLineDamage = 135 + level * 24;
    dongfengKnockback = 3200.0f + level * 85.0f;

    currentPath.clear();
    hasPath = false;
    pendingNpc = -1;

    SpawnHitSpark(playerPosition);

    VfxParticle castCircle;
    castCircle.type = VfxType::SkillCircle;
    castCircle.pos = playerPosition;
    castCircle.radius = dongfengStartRadius + 18.0f;
    castCircle.life = dongfengCastDuration;
    castCircle.maxLife = dongfengCastDuration;
    castCircle.color = { 255, 220, 40, 135 };
    castCircle.active = true;
    vfxParticles.push_back(castCircle);
}

float Game::GetDongfengWaveRadius() const
{
    if (dongfengRange <= 0.0f)
    {
        return dongfengStartRadius;
    }

    float t = dongfengWaveTravelled / dongfengRange;

    if (t < 0.0f)
    {
        t = 0.0f;
    }

    if (t > 1.0f)
    {
        t = 1.0f;
    }

    return dongfengStartRadius + (dongfengEndRadius - dongfengStartRadius) * t;
}

void Game::UpdateDongfeng(float dt)
{
    if (dongfengCasting)
    {
        dongfengCastTimer += dt;

        if (dongfengCastTimer >= dongfengCastDuration)
        {
            dongfengCasting = false;
            dongfengCastTimer = 0.0f;

            LaunchDongfengWave();
        }
    }

    if (!dongfengWaveActive)
    {
        return;
    }

    dongfengWavePrevPos = dongfengWavePos;

    float step = dongfengWaveSpeed * dt;

    if (dongfengWaveTravelled + step > dongfengRange)
    {
        step = dongfengRange - dongfengWaveTravelled;
    }

    dongfengWaveTravelled += step;

    dongfengWavePos = Vector2Add(
        dongfengWavePos,
        Vector2Scale(dongfengWaveDirection, step)
    );

    float radius = GetDongfengWaveRadius();

    DestroyEnemyProjectilesInDongfengPath(
        dongfengWavePrevPos,
        dongfengWavePos,
        radius
    );

    ResolveDongfengWaveHits(
        dongfengWavePrevPos,
        dongfengWavePos,
        radius
    );

    if (dongfengWaveTravelled >= dongfengRange)
    {
        dongfengWaveActive = false;
        dongfengLockedTargetId = 0;

        SpawnHitSpark(dongfengWavePos);
    }
}

void Game::LaunchDongfengWave()
{
    dongfengWaveActive = true;
    dongfengWaveId = nextDongfengWaveId++;

    dongfengWaveStart = dongfengCastStart;
    dongfengWavePos = dongfengCastStart;
    dongfengWavePrevPos = dongfengCastStart;
    dongfengWaveDirection = dongfengCastDirection;
    dongfengWaveTravelled = 0.0f;

    SpawnHitSpark(dongfengWaveStart);

    VfxParticle launchLine;
    launchLine.type = VfxType::SlashLine;
    launchLine.pos = dongfengWaveStart;
    launchLine.endPos = Vector2Add(
        dongfengWaveStart,
        Vector2Scale(dongfengWaveDirection, 160.0f)
    );
    launchLine.radius = dongfengStartRadius;
    launchLine.life = 0.16f;
    launchLine.maxLife = 0.16f;
    launchLine.color = { 255, 220, 40, 235 };
    launchLine.active = true;
    vfxParticles.push_back(launchLine);
}

void Game::DestroyEnemyProjectilesInDongfengPath(
    Vector2 previousPos,
    Vector2 currentPos,
    float radius
)
{
    for (Projectile& projectile : projectiles)
    {
        if (!projectile.active)
        {
            continue;
        }

        if (projectile.owner != ProjectileOwner::Enemy)
        {
            continue;
        }

        float t = 0.0f;
        float distanceSq = DistancePointToSegmentSquared(
            projectile.pos,
            previousPos,
            currentPos,
            t
        );

        float hitRadius = radius + projectile.radius;

        if (distanceSq <= hitRadius * hitRadius)
        {
            projectile.active = false;
            SpawnHitSpark(projectile.pos);
        }
    }
}

void Game::DrawDongfengTelegraph()
{
    if (!dongfengCasting)
    {
        return;
    }

    float t = 0.0f;

    if (dongfengCastDuration > 0.0f)
    {
        t = dongfengCastTimer / dongfengCastDuration;
    }

    t = Clamp(t, 0.0f, 1.0f);

    Vector2 startWorld = playerPosition;
    Vector2 endWorld = Vector2Add(
        startWorld,
        Vector2Scale(dongfengCastDirection, 260.0f + 120.0f * t)
    );

    DrawLineEx(
        WorldToViewElevated(startWorld),
        WorldToViewElevated(endWorld),
        5.0f + 12.0f * t,
        Color{ 255, 220, 45, static_cast<unsigned char>(100 + 130 * t) }
    );

    DrawGroundCircleLines(
        playerPosition,
        dongfengStartRadius + 12.0f * t,
        Color{ 255, 235, 90, static_cast<unsigned char>(155 + 80 * t) }
    );

    DrawGroundCircleLines(
        playerPosition,
        dongfengStartRadius * 0.55f + 18.0f * t,
        Color{ 255, 255, 210, static_cast<unsigned char>(120 + 90 * t) }
    );
}
void Game::DrawDongfengWave()
{
    if (!dongfengWaveActive)
    {
        return;
    }

    float radius = GetDongfengWaveRadius();
    Vector2 viewPosition = WorldToViewElevated(dongfengWavePos);
    Vector2 viewDirection = WorldVectorToView(dongfengWaveDirection);

    if (Vector2Length(viewDirection) > 0.001f)
    {
        viewDirection = Vector2Normalize(viewDirection);
    }
    else
    {
        viewDirection = { 1.0f, 0.0f };
    }

    float angleDeg = atan2f(viewDirection.y, viewDirection.x) * RAD2DEG;

    float progress = 0.0f;

    if (dongfengRange > 0.0f)
    {
        progress = dongfengWaveTravelled / dongfengRange;
    }

    progress = Clamp(progress, 0.0f, 1.0f);

    unsigned char alpha = static_cast<unsigned char>(215 + 30 * progress);

    Color outerColor{ 255, 205, 35, alpha };
    Color innerColor{ 255, 255, 220, 235 };

    DrawRing(
        viewPosition,
        radius * 0.62f,
        radius,
        angleDeg - 70.0f,
        angleDeg + 70.0f,
        18,
        outerColor
    );

    DrawRing(
        viewPosition,
        radius * 0.82f,
        radius * 0.94f,
        angleDeg - 48.0f,
        angleDeg + 48.0f,
        12,
        innerColor
    );

    Vector2 tail = Vector2Subtract(
        viewPosition,
        Vector2Scale(viewDirection, radius * 1.10f)
    );

    DrawLineEx(
        tail,
        viewPosition,
        radius * 0.14f,
        Color{ 255, 210, 45, 115 }
    );
}
void Game::ResolveDongfengWaveHits(
    Vector2 previousPos,
    Vector2 currentPos,
    float radius
)
{
    for (Enemy& enemy : enemies)
    {
        if (
            !enemy.active ||
            IsEnemySpawnProtected(
                enemy
            )
            )
        {
            continue;
        }

        if (enemy.dongfengHitId == dongfengWaveId)
        {
            continue;
        }

        float t = 0.0f;
        float distanceSq = DistancePointToSegmentSquared(
            enemy.pos,
            previousPos,
            currentPos,
            t
        );

        float hitRadius = radius + enemy.radius;

        if (distanceSq > hitRadius * hitRadius)
        {
            continue;
        }

        enemy.dongfengHitId = dongfengWaveId;

        bool isMainTarget = enemy.id == dongfengLockedTargetId;

        int damage = isMainTarget ? dongfengMainDamage : dongfengLineDamage;

        ApplySkillDamageToEnemy(
            enemy,
            SkillType::Dongfeng,
            damage,
            enemy.pos
        );

        if (!enemy.active)
        {
            continue;
        }

        SpawnHitSpark(enemy.pos);

        Vector2 knockbackOrigin = Vector2Subtract(
            enemy.pos,
            Vector2Scale(dongfengWaveDirection, 80.0f)
        );

        if (IsBossEnemy(enemy))
        {
            enemy.stunTimer = std::max(enemy.stunTimer, 0.25f);

            ApplyKnockbackToEnemy(
                enemy,
                knockbackOrigin,
                dongfengKnockback * 0.85f,
                0.08f,
                0.20f
            );
        }
        else
        {
            ApplyKnockbackToEnemy(
                enemy,
                knockbackOrigin,
                dongfengKnockback,
                0.38f,
                0.80f
            );
        }
    }
}



void Game::ActivateSkill(SkillType type)
{
    if (type == SkillType::SpinningBlade)
    {
        ActivateFireball(); // Current real Wind Blades implementation.
    }
    else if (type == SkillType::Huashan)
    {
        ActivateHuashan();
    }
    else if (type == SkillType::Dongfeng)
    {
        ActivateDongfeng();
    }

    // Legacy placeholder skills, remove later.
    else if (type == SkillType::Fireball)
    {
        ActivateFireball();
    }
    else if (type == SkillType::Lightning)
    {
        ActivateLightning();
    }
    else if (type == SkillType::IceField)
    {
        ActivateIceField();
    }
}



void Game::ActivateFireball()
{
    SpawnWhirlwindBlades();
}

void Game::SpawnWhirlwindBlades()
{
    int slotIndex = FindSkillSlotIndex(SkillType::SpinningBlade);
    int level = (slotIndex >= 0) ? skills[slotIndex].level : 1;

    int bladeCount = GetWhirlwindBladeCount(level);

    int damage = 20 + level * 6;
    float duration = 5.0f;

    if (level >= 5)
    {
        duration += 1.0f;
    }

    if (level >= 10)
    {
        duration += 1.0f;
    }

    if (level >= 15)
    {
        duration += 1.0f;
    }

    orbitalBlades.clear();

    for (int i = 0; i < bladeCount; ++i)
    {
        OrbitalBlade blade;

        blade.angle = (static_cast<float>(i) / static_cast<float>(bladeCount)) * PI * 2.0f;
        blade.orbitRadius = 105.0f;
        blade.angularSpeed = 6.5f + level * 0.05f;
        blade.bladeRadius = 30.0f;
        blade.damage = damage;
        blade.life = duration;
        blade.hitTimer = 0.0f;
        blade.hitInterval = 0.12f;
        blade.active = true;

        orbitalBlades.push_back(blade);
    }

    SpawnHitSpark(playerPosition);
}
void Game::ActivateLightning()
{
    int slotIndex = FindSkillSlotIndex(SkillType::Lightning);
    int level = (slotIndex >= 0) ? skills[slotIndex].level : 1;

    int baseDamage = 50 + level * 12;

    int branchCount = GetLightningBranchCount(level);
    int chainCount = GetLightningChainCount(level);

    float firstTargetRange = 720.0f;
    float chainRange = 300.0f;

    std::vector<Enemy*> globallyHitEnemies;

    for (int branch = 0; branch < branchCount; ++branch)
    {
        Enemy* currentTarget = FindNearestEnemyExcluding(
            playerPosition,
            firstTargetRange,
            globallyHitEnemies
        );

        if (currentTarget == nullptr)
        {
            break;
        }

        Vector2 chainStart = playerPosition;
        int currentDamage = baseDamage;

        std::vector<Enemy*> branchHitEnemies;

        for (int chain = 0; chain <= chainCount && currentTarget != nullptr; ++chain)
        {
            branchHitEnemies.push_back(currentTarget);
            globallyHitEnemies.push_back(currentTarget);

            SpawnLightningLine(chainStart, currentTarget->pos);

            currentTarget->stunTimer = 0.20f;

            ApplyDamageToEnemy(
                *currentTarget,
                currentDamage,
                currentTarget->pos
            );

            chainStart = currentTarget->pos;

            currentDamage /= 2;

            if (currentDamage < 1)
            {
                break;
            }

            currentTarget = FindNearestEnemyExcluding(
                chainStart,
                chainRange,
                globallyHitEnemies
            );
        }
    }
}

void Game::ActivateIceField()
{
    int slotIndex = FindSkillSlotIndex(SkillType::IceField);
    int level = (slotIndex >= 0) ? skills[slotIndex].level : 1;

    float slowRadius = 360.0f;

    float freezeRadius = 120.0f + static_cast<float>(level - 1) * 16.0f;

    if (freezeRadius > slowRadius)
    {
        freezeRadius = slowRadius;
    }

    float freezeDuration = 1.40f + level * 0.08f;

    if (level >= 5)
    {
        freezeDuration += 0.35f;
    }

    if (level >= 10)
    {
        freezeDuration += 0.35f;
    }

    if (level >= 15)
    {
        freezeDuration += 0.35f;
    }

    if (level >= 20)
    {
        freezeDuration += 0.35f;
    }

    float visualDuration = 1.00f + level * 0.04f;

    if (visualDuration > 2.0f)
    {
        visualDuration = 2.0f;
    }

    float postFreezeSlowDuration = 2.0f + level * 0.06f;
    float outerSlowDuration = 3.0f + level * 0.08f;

    int freezeDamage = 22 + level * 6;
    int slowDamage = 10 + level * 3;

    VfxParticle slowCircle;
    slowCircle.type = VfxType::SkillCircle;
    slowCircle.pos = playerPosition;
    slowCircle.radius = slowRadius;
    slowCircle.life = visualDuration;
    slowCircle.maxLife = visualDuration;
    slowCircle.color = { 80, 160, 255, 70 };
    slowCircle.active = true;
    vfxParticles.push_back(slowCircle);

    VfxParticle freezeCircle;
    freezeCircle.type = VfxType::SkillCircle;
    freezeCircle.pos = playerPosition;
    freezeCircle.radius = freezeRadius;
    freezeCircle.life = visualDuration;
    freezeCircle.maxLife = visualDuration;
    freezeCircle.color = { 170, 230, 255, 150 };
    freezeCircle.active = true;
    vfxParticles.push_back(freezeCircle);

    for (Enemy& enemy : enemies)
    {
        if (!enemy.active)
        {
            continue;
        }

        float distance = Vector2Distance(playerPosition, enemy.pos);

        if (distance <= freezeRadius)
        {
            enemy.frozenTimer = freezeDuration;
            enemy.slowTimer = freezeDuration + postFreezeSlowDuration;
            enemy.slowMultiplier = 0.35f;

            ApplyDamageToEnemy(enemy, freezeDamage, enemy.pos);
        }
        else if (distance <= slowRadius)
        {
            enemy.slowTimer = outerSlowDuration;
            enemy.slowMultiplier = 0.42f;

            ApplyDamageToEnemy(enemy, slowDamage, enemy.pos);
        }
    }
}

void Game::DrawCombat()
{
    PERF_DRAW_BLOCK(perfDrawCombatProjectilesMs,
        {
            DrawProjectiles();
        });

    PERF_DRAW_BLOCK(perfDrawCombatEnemiesMs,
        {
            DrawEnemies();
        });

    PERF_DRAW_BLOCK(perfDrawCombatBladesMs,
        {
            DrawOrbitalBlades();
        });

    PERF_DRAW_BLOCK(perfDrawCombatDongfengTelegraphMs,
        {
            DrawDongfengTelegraph();
        });

    PERF_DRAW_BLOCK(perfDrawCombatDongfengWaveMs,
        {
            DrawDongfengWave();
        });

    PERF_DRAW_BLOCK(perfDrawCombatVfxMs,
        {
            DrawVfx();
        });
}



void Game::DrawEnemies()
{
    for (const Enemy& enemy : enemies)
    {
        if (!enemy.active)
        {
            continue;
        }

        float visibilityRadius = enemy.radius + 80.0f + enemy.visualHeight;

        if (!IsWorldCircleVisible(enemy.pos, visibilityRadius))
        {
            continue;
        }

        Color color = GREEN;

        if (enemy.type == EnemyType::Runner) color = LIME;
        else if (enemy.type == EnemyType::Tank) color = DARKGREEN;
        else if (enemy.type == EnemyType::Shooter) color = ORANGE;
        else if (enemy.type == EnemyType::Boss) color = PURPLE;

        if (enemy.frozenTimer > 0.0f)
        {
            color = { 150, 230, 255, 255 };
        }
        else if (enemy.stunTimer > 0.0f || enemy.landingStunTimer > 0.0f)
        {
            color = { 255, 255, 120, 255 };
        }
        else if (enemy.slowTimer > 0.0f)
        {
            color = { 90, 170, 255, 255 };
        }

        if (enemy.type == EnemyType::Boss && enemy.bossDashCharging)
        {
            float elapsed = enemy.bossDashChargeDuration - enemy.bossDashChargeTimer;
            float phaseLength = enemy.bossDashChargeDuration / 6.0f;
            int phase = static_cast<int>(elapsed / phaseLength);
            color = (phase % 2 == 0) ? WHITE : Color{ 255, 40, 40, 255 };
        }

        Vector2 groundPos = WorldToView(enemy.pos);
        Vector2 drawPos = { groundPos.x, groundPos.y - enemy.visualHeight };

        if (enemy.visualHeight > 2.0f)
        {
            float shadowScale = 1.0f - enemy.visualHeight / 130.0f;
            if (shadowScale < 0.38f) shadowScale = 0.38f;

            DrawEllipse(
                static_cast<int>(groundPos.x + 5.0f),
                static_cast<int>(groundPos.y + 9.0f),
                enemy.radius * shadowScale,
                enemy.radius * 0.38f * shadowScale,
                Color{ 0, 0, 0, 90 }
            );
        }

        DrawCircleV(drawPos, enemy.radius, color);

        if (enemy.hp < enemy.maxHp || enemy.type == EnemyType::Boss)
        {
            float hpRatio = Clamp(
                static_cast<float>(enemy.hp) / static_cast<float>(enemy.maxHp),
                0.0f,
                1.0f
            );

            Rectangle backBar{
                drawPos.x - enemy.radius,
                drawPos.y - enemy.radius - 18.0f,
                enemy.radius * 2.0f,
                6.0f
            };

            Rectangle hpBar = backBar;
            hpBar.width *= hpRatio;

            DrawRectangleRec(backBar, DARKGRAY);
            DrawRectangleRec(hpBar, RED);
        }
    }
}
void Game::DrawProjectiles()
{
    for (const Projectile& projectile : projectiles)
    {
        if (!projectile.active)
        {
            continue;
        }

        Vector2 viewPosition = WorldToView(projectile.pos);

        if (projectile.owner == ProjectileOwner::Enemy)
        {
            DrawCircleV(viewPosition, projectile.radius + 5.0f, { 255, 80, 80, 100 });
            DrawCircleV(viewPosition, projectile.radius, { 255, 80, 60, 255 });
        }
        else
        {
            DrawCircleV(viewPosition, projectile.radius + 5.0f, { 255, 220, 80, 100 });
            DrawCircleV(viewPosition, projectile.radius, YELLOW);
        }
    }
}
void Game::DrawVfx()
{
    for (const VfxParticle& p : vfxParticles)
    {
        if (!p.active)
        {
            continue;
        }

        float t = (p.maxLife > 0.0f) ? p.life / p.maxLife : 1.0f;
        t = Clamp(t, 0.0f, 1.0f);

        Color c = p.color;
        c.a = static_cast<unsigned char>(static_cast<float>(p.color.a) * t);

        Vector2 viewPos = WorldToView(p.pos);

        if (p.type == VfxType::HitSpark || p.type == VfxType::DeathBurst)
        {
            DrawCircleV(viewPos, p.radius * t, c);
        }
        else if (p.type == VfxType::FloatingDamage)
        {
            DrawText(
                TextFormat("%d", p.value),
                static_cast<int>(viewPos.x),
                static_cast<int>(viewPos.y),
                18,
                c
            );
        }
        else if (p.type == VfxType::LightningLine)
        {
            float width = std::max(2.0f, p.radius * t);
            DrawLineEx(viewPos, WorldToView(p.endPos), width, c);
        }
        else if (p.type == VfxType::SlashLine)
        {
            float width = std::max(3.0f, p.radius * 0.35f * t);
            DrawLineEx(viewPos, WorldToView(p.endPos), width, c);
        }
        else if (p.type == VfxType::SkillCircle)
        {
            Color outline = p.color;
            outline.a = static_cast<unsigned char>(180.0f * t);

            Color inner = p.color;
            inner.a = static_cast<unsigned char>(90.0f * t);

            DrawGroundCircleLines(p.pos, p.radius, outline);
            DrawGroundCircleLines(p.pos, p.radius * 0.55f, inner);
        }
        else if (p.type == VfxType::Explosion)
        {
            DrawGroundCircleLines(p.pos, p.radius * t, c);
        }
    }
}
void Game::DrawCombatHud()
{
    const int y = 68;

    const DungeonChamber* chamber =
        FindChamberById(
            activeChamberId
        );

    DrawRectangle(
        0,
        y,
        430,
        110,
        Color{
            0,
            0,
            0,
            140
        }
    );

    DrawText(
        TextFormat(
            "HP: %d/%d",
            player.hp,
            player.maxHp
        ),
        18,
        y + 8,
        20,
        WHITE
    );

    DrawText(
        TextFormat(
            "Chamber: %s",
            chamber != nullptr
            ? chamber->name.c_str()
            : "None"
        ),
        18,
        y + 32,
        18,
        WHITE
    );

    const int displayedWave =
        chamber != nullptr
        ? std::max(
            1,
            chamber->currentWave
        )
        : 0;

    const int requiredWaves =
        chamber != nullptr
        ? chamber->wavesRequired
        : 2;

    DrawText(
        TextFormat(
            "Encounter wave: %d/%d   Spawned: %d/%d",
            displayedWave,
            requiredWaves,
            wave.enemiesSpawned,
            wave.enemiesToSpawn
        ),
        18,
        y + 56,
        17,
        WHITE
    );

    const bool chamberCleared =
        chamber != nullptr &&
        chamber->cleared;

    const Color exitColor =
        chamberCleared
        ? Color{
            120,
            255,
            145,
            255
    }
        : Color{
            255,
            170,
            80,
            255
    };

    const char* encounterStatus =
        chamberCleared
        ? "EXIT UNLOCKED - enter the next chamber"
        : (
            wave.waitingForNextWave
            ? "CHAMBER LOCKED - next wave incoming"
            : "CHAMBER LOCKED - defeat all enemies"
            );

    DrawText(
        encounterStatus,
        18,
        y + 80,
        17,
        exitColor
    );

    if (chamberClearedMessageTimer > 0.0f)
    {
        const char* message =
            "CHAMBER CLEARED - EXIT UNLOCKED";

        const int fontSize = 30;
        const int textWidth =
            MeasureText(
                message,
                fontSize
            );

        const int boxWidth =
            textWidth + 48;

        const int boxX =
            GetScreenWidth() / 2 -
            boxWidth / 2;

        const int boxY = 86;

        DrawRectangle(
            boxX,
            boxY,
            boxWidth,
            56,
            Color{
                0,
                0,
                0,
                205
            }
        );

        DrawRectangleLines(
            boxX,
            boxY,
            boxWidth,
            56,
            GOLD
        );

        DrawText(
            message,
            boxX + 24,
            boxY + 13,
            fontSize,
            GOLD
        );
    }
}

void Game::DrawSkillUi()
{
    for (const SkillSlot& skill : skills)
    {
        DrawRectangleRec(skill.buttonRect, DARKGRAY);
        DrawRectangleLinesEx(skill.buttonRect, 2.0f, WHITE);

        const char* label = "SKL";

        if (skill.type == SkillType::SpinningBlade)
        {
            label = "BLADE";
        }
        else if (skill.type == SkillType::Huashan)
        {
            label = "HUA";
        }
        else if (skill.type == SkillType::Dongfeng)
        {
            label = "DONG";
        }
        else if (skill.type == SkillType::Fireball)
        {
            label = "FIRE";
        }
        else if (skill.type == SkillType::Lightning)
        {
            label = "LITE";
        }
        else if (skill.type == SkillType::IceField)
        {
            label = "ICE";
        }

        DrawText(
            label,
            static_cast<int>(skill.buttonRect.x + 8),
            static_cast<int>(skill.buttonRect.y + 20),
            18,
            WHITE
        );

        if (!skill.unlocked)
        {
            DrawRectangleRec(skill.buttonRect, { 35, 35, 35, 210 });
            DrawRectangleLinesEx(skill.buttonRect, 2.0f, GRAY);

            DrawText(
                "LOCK",
                static_cast<int>(skill.buttonRect.x + 8),
                static_cast<int>(skill.buttonRect.y + 22),
                18,
                GRAY
            );

            continue;
        }

        if (skill.cooldownRemaining > 0.0f)
        {
            float ratio = skill.cooldownRemaining / skill.cooldown;

            Rectangle overlay = skill.buttonRect;
            overlay.y += overlay.height * (1.0f - ratio);
            overlay.height *= ratio;

            DrawRectangleRec(overlay, { 0, 0, 0, 170 });

            DrawText(
                TextFormat("%.1f", skill.cooldownRemaining),
                static_cast<int>(skill.buttonRect.x + 13),
                static_cast<int>(skill.buttonRect.y + 40),
                18,
                YELLOW
            );
        }
    }
}

void Game::DrawUpgradeChoices()
{
    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();

    DrawRectangle(
        0,
        0,
        screenW,
        screenH,
        { 0, 0, 0, 190 }
    );

    int titleSize = static_cast<int>(
        Clamp(
            static_cast<float>(screenH) * 0.060f,
            28.0f,
            42.0f
        )
        );

    int subtitleSize = static_cast<int>(
        Clamp(
            static_cast<float>(screenH) * 0.034f,
            15.0f,
            22.0f
        )
        );

    const char* heading = "Choose a Skill";

    int headingWidth =
        MeasureText(heading, titleSize);

    int titleY = static_cast<int>(
        Clamp(
            static_cast<float>(screenH) * 0.045f,
            20.0f,
            50.0f
        )
        );

    DrawText(
        heading,
        screenW / 2 - headingWidth / 2,
        titleY,
        titleSize,
        WHITE
    );

    const char* instruction =
        "Select one martial skill to strengthen your build";

    int instructionWidth =
        MeasureText(
            instruction,
            subtitleSize
        );

    DrawText(
        instruction,
        screenW / 2 - instructionWidth / 2,
        titleY + titleSize + 8,
        subtitleSize,
        { 210, 210, 210, 255 }
    );

    bool usePortraitRow =
        static_cast<float>(screenW) >= 620.0f;

    for (
        size_t i = 0;
        i < currentUpgradeChoices.size();
        ++i
        )
    {
        const UpgradeChoice& choice =
            currentUpgradeChoices[i];

        Rectangle card = choice.cardRect;

        bool hovered =
            CheckCollisionPointRec(
                GetMousePosition(),
                card
            );

        Color fill =
            hovered
            ? Color{ 68, 68, 86, 248 }
        : Color{ 48, 48, 60, 240 };

        Color outline =
            hovered
            ? GOLD
            : Color{ 210, 210, 210, 255 };

        DrawRectangleRounded(
            card,
            0.10f,
            8,
            fill
        );

        DrawRectangleRoundedLinesEx(
            card,
            0.10f,
            8,
            3.0f,
            outline
        );

        float padding = Clamp(
            card.width * 0.055f,
            10.0f,
            18.0f
        );

        std::string status;

        if (choice.unlocksSkill)
        {
            status = "Unlock";
        }
        else
        {
            status =
                "Lv." +
                std::to_string(choice.targetLevel);
        }

        if (usePortraitRow)
        {
            float imageSize = std::min(
                card.width - padding * 2.0f,
                Clamp(
                    card.height * 0.34f,
                    86.0f,
                    150.0f
                )
            );

            Rectangle imageRect{
                card.x +
                    card.width * 0.5f -
                    imageSize * 0.5f,
                card.y + padding,
                imageSize,
                imageSize
            };

            DrawRectangleRounded(
                imageRect,
                0.10f,
                6,
                Color{ 80, 80, 95, 255 }
            );

            DrawRectangleRoundedLinesEx(
                imageRect,
                0.10f,
                6,
                2.0f,
                Color{ 180, 180, 190, 255 }
            );

            int imageTextSize = static_cast<int>(
                Clamp(
                    imageSize * 0.18f,
                    15.0f,
                    22.0f
                )
                );

            int imageTextWidth =
                MeasureText(
                    "IMAGE",
                    imageTextSize
                );

            DrawText(
                "IMAGE",
                static_cast<int>(
                    imageRect.x +
                    imageRect.width * 0.5f -
                    imageTextWidth * 0.5f
                    ),
                static_cast<int>(
                    imageRect.y +
                    imageRect.height * 0.5f -
                    imageTextSize * 0.5f
                    ),
                imageTextSize,
                LIGHTGRAY
            );

            float textY =
                imageRect.y +
                imageRect.height +
                padding * 0.75f;

            int cardTitleSize =
                static_cast<int>(
                    Clamp(
                        card.width * 0.090f,
                        18.0f,
                        27.0f
                    )
                    );

            int cardSubtitleSize =
                static_cast<int>(
                    Clamp(
                        card.width * 0.064f,
                        14.0f,
                        19.0f
                    )
                    );

            int descriptionSize =
                static_cast<int>(
                    Clamp(
                        card.width * 0.058f,
                        13.0f,
                        17.0f
                    )
                    );

            DrawText(
                choice.title.c_str(),
                static_cast<int>(card.x + padding),
                static_cast<int>(textY),
                cardTitleSize,
                WHITE
            );

            textY +=
                static_cast<float>(cardTitleSize) +
                5.0f;

            DrawText(
                choice.subtitle.c_str(),
                static_cast<int>(card.x + padding),
                static_cast<int>(textY),
                cardSubtitleSize,
                Color{ 255, 210, 90, 255 }
            );

            textY +=
                static_cast<float>(cardSubtitleSize) +
                10.0f;

            float statusAreaHeight = 34.0f;

            Rectangle descriptionBounds{
                card.x + padding,
                textY,
                card.width - padding * 2.0f,
                card.y +
                    card.height -
                    padding -
                    statusAreaHeight -
                    textY
            };

            DrawWrappedText(
                choice.description,
                descriptionBounds,
                descriptionSize,
                4.0f,
                Color{ 220, 220, 220, 255 },
                5
            );

            int statusSize =
                static_cast<int>(
                    Clamp(
                        card.width * 0.075f,
                        16.0f,
                        22.0f
                    )
                    );

            int statusWidth =
                MeasureText(
                    status.c_str(),
                    statusSize
                );

            DrawText(
                status.c_str(),
                static_cast<int>(
                    card.x +
                    card.width -
                    padding -
                    statusWidth
                    ),
                static_cast<int>(
                    card.y +
                    card.height -
                    padding -
                    statusSize
                    ),
                statusSize,
                YELLOW
            );
        }
        else
        {
            // Compact mobile layout.
            float imageSize = std::min(
                card.height - padding * 2.0f,
                Clamp(
                    card.width * 0.24f,
                    58.0f,
                    96.0f
                )
            );

            Rectangle imageRect{
                card.x + padding,
                card.y +
                    card.height * 0.5f -
                    imageSize * 0.5f,
                imageSize,
                imageSize
            };

            DrawRectangleRounded(
                imageRect,
                0.10f,
                6,
                Color{ 80, 80, 95, 255 }
            );

            DrawRectangleRoundedLinesEx(
                imageRect,
                0.10f,
                6,
                2.0f,
                Color{ 180, 180, 190, 255 }
            );

            int imageTextSize = static_cast<int>(
                Clamp(
                    imageSize * 0.17f,
                    12.0f,
                    18.0f
                )
                );

            int imageTextWidth =
                MeasureText(
                    "IMAGE",
                    imageTextSize
                );

            DrawText(
                "IMAGE",
                static_cast<int>(
                    imageRect.x +
                    imageRect.width * 0.5f -
                    imageTextWidth * 0.5f
                    ),
                static_cast<int>(
                    imageRect.y +
                    imageRect.height * 0.5f -
                    imageTextSize * 0.5f
                    ),
                imageTextSize,
                LIGHTGRAY
            );

            float textX =
                imageRect.x +
                imageRect.width +
                padding;

            float textWidth =
                card.x +
                card.width -
                padding -
                textX;

            int cardTitleSize =
                static_cast<int>(
                    Clamp(
                        card.height * 0.17f,
                        17.0f,
                        24.0f
                    )
                    );

            int cardSubtitleSize =
                static_cast<int>(
                    Clamp(
                        card.height * 0.125f,
                        13.0f,
                        17.0f
                    )
                    );

            int descriptionSize =
                static_cast<int>(
                    Clamp(
                        card.height * 0.115f,
                        12.0f,
                        16.0f
                    )
                    );

            float textY = card.y + padding;

            DrawText(
                choice.title.c_str(),
                static_cast<int>(textX),
                static_cast<int>(textY),
                cardTitleSize,
                WHITE
            );

            textY +=
                static_cast<float>(cardTitleSize) +
                3.0f;

            DrawText(
                choice.subtitle.c_str(),
                static_cast<int>(textX),
                static_cast<int>(textY),
                cardSubtitleSize,
                Color{ 255, 210, 90, 255 }
            );

            textY +=
                static_cast<float>(cardSubtitleSize) +
                5.0f;

            Rectangle descriptionBounds{
                textX,
                textY,
                textWidth,
                card.y +
                    card.height -
                    padding -
                    descriptionSize -
                    textY
            };

            DrawWrappedText(
                choice.description,
                descriptionBounds,
                descriptionSize,
                3.0f,
                Color{ 220, 220, 220, 255 },
                2
            );

            int statusSize =
                static_cast<int>(
                    Clamp(
                        card.height * 0.14f,
                        14.0f,
                        20.0f
                    )
                    );

            int statusWidth =
                MeasureText(
                    status.c_str(),
                    statusSize
                );

            DrawText(
                status.c_str(),
                static_cast<int>(
                    card.x +
                    card.width -
                    padding -
                    statusWidth
                    ),
                static_cast<int>(
                    card.y +
                    card.height -
                    padding -
                    statusSize
                    ),
                statusSize,
                YELLOW
            );
        }
    }
}

bool Game::TryChooseUpgradeAtScreen(Vector2 screenPos)
{
    for (size_t i = 0; i < currentUpgradeChoices.size(); ++i)
    {
        if (CheckCollisionPointRec(screenPos, currentUpgradeChoices[i].cardRect))
        {
            ApplyUpgradeChoice(static_cast<int>(i));
            return true;
        }
    }

    return false;
}

void Game::ApplyUpgradeChoice(int choiceIndex)
{
    if (choiceIndex < 0 || choiceIndex >= static_cast<int>(currentUpgradeChoices.size()))
    {
        return;
    }

    const UpgradeChoice& choice = currentUpgradeChoices[choiceIndex];

    int slotIndex = FindSkillSlotIndex(choice.skillType);

    if (slotIndex >= 0)
    {
        if (!skills[slotIndex].unlocked)
        {
            skills[slotIndex].unlocked = true;
            skills[slotIndex].level = 1;
        }
        else
        {
            skills[slotIndex].level++;

            if (skills[slotIndex].level > 20)
            {
                skills[slotIndex].level = 20;
            }
        }

        skills[slotIndex].cooldownRemaining = 0.0f;
    }

    const bool returnToCurrentWave =
        upgradeMenuOpenedManually;

    upgradeMenuOpenedManually =
        false;

    currentUpgradeChoices.clear();

    if (returnToCurrentWave)
    {
        // Manual upgrade:
        // resume the same wave without changing its counters.
        gameState =
            GameState::Playing;

        attackButtonDown =
            false;

        dashButtonDown =
            false;

        dashButtonPressed =
            false;

        return;
    }

    // Chamber encounters start their own next wave. A non-manual
    // upgrade therefore only returns to gameplay.
    gameState =
        GameState::Playing;

    attackButtonDown = false;
    dashButtonDown = false;
    dashButtonPressed = false;
}

void Game::UnlockSkill(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= 3)
    {
        return;
    }

    skills[slotIndex].unlocked = true;
    skills[slotIndex].cooldownRemaining = 0.0f;
}

void Game::LevelUpSkill(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= 3)
    {
        return;
    }

    if (!skills[slotIndex].unlocked)
    {
        skills[slotIndex].unlocked = true;
        skills[slotIndex].level = 1;
        return;
    }

    skills[slotIndex].level++;

    if (skills[slotIndex].level > 20)
    {
        skills[slotIndex].level = 20;
    }
}
void Game::UpdateJoystick(float dt)
{
    (void)dt;

    Vector2 pointer{};
    bool hasJoystickTouch = false;

    int touchCount = GetTouchPointCount();

    // Mobile / touch input
    if (touchCount > 0)
    {
        if (joystickTouchId != -1)
        {
            for (int i = 0; i < touchCount; ++i)
            {
                int id = GetTouchPointId(i);

                if (id == joystickTouchId)
                {
                    pointer = GetTouchPosition(i);
                    hasJoystickTouch = true;
                    break;
                }
            }

            if (!hasJoystickTouch)
            {
                joystickTouchId = -1;
                joystickActive = false;
                joystickDirection = { 0.0f, 0.0f };
                return;
            }
        }
        else
        {
            for (int i = 0; i < touchCount; ++i)
            {
                Vector2 touch = GetTouchPosition(i);

                if (IsScreenPointOnButtonUi(touch))
                {
                    continue;
                }

                if (IsScreenPointInsideJoystick(touch))
                {
                    joystickTouchId = GetTouchPointId(i);
                    pointer = touch;
                    hasJoystickTouch = true;
                    joystickActive = true;
                    break;
                }
            }
        }

        if (hasJoystickTouch)
        {
            Vector2 base = GetJoystickBaseScreen();
            Vector2 delta = Vector2Subtract(pointer, base);
            float distance = Vector2Length(delta);

            if (distance > joystickRadius)
            {
                delta = Vector2Scale(Vector2Normalize(delta), joystickRadius);
                distance = joystickRadius;
            }

            if (distance < 8.0f)
            {
                joystickDirection = { 0.0f, 0.0f };
            }
            else
            {
                joystickDirection = Vector2Scale(delta, 1.0f / joystickRadius);
            }

            return;
        }

        if (joystickTouchId == -1)
        {
            joystickActive = false;
            joystickDirection = { 0.0f, 0.0f };
        }

        return;
    }

    // No touch input, reset joystick touch ownership.
    joystickTouchId = -1;

    // Desktop mouse fallback only when there are no touches.
    bool mouseDown = false;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        Vector2 mouse = GetMousePosition();

        if (!IsScreenPointOnButtonUi(mouse) && IsScreenPointInsideJoystick(mouse))
        {
            pointer = mouse;
            mouseDown = true;
            joystickActive = true;
        }
    }
    else if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && joystickActive)
    {
        Vector2 mouse = GetMousePosition();

        if (!IsScreenPointOnButtonUi(mouse))
        {
            pointer = mouse;
            mouseDown = true;
        }
    }

    if (mouseDown)
    {
        Vector2 base = GetJoystickBaseScreen();
        Vector2 delta = Vector2Subtract(pointer, base);
        float distance = Vector2Length(delta);

        if (distance > joystickRadius)
        {
            delta = Vector2Scale(Vector2Normalize(delta), joystickRadius);
            distance = joystickRadius;
        }

        if (distance < 8.0f)
        {
            joystickDirection = { 0.0f, 0.0f };
        }
        else
        {
            joystickDirection = Vector2Scale(delta, 1.0f / joystickRadius);
        }

        return;
    }

    Vector2 keyboardDirection{ 0.0f, 0.0f };

    if (IsKeyDown(KEY_A)) keyboardDirection.x -= 1.0f;
    if (IsKeyDown(KEY_D)) keyboardDirection.x += 1.0f;
    if (IsKeyDown(KEY_W)) keyboardDirection.y -= 1.0f;
    if (IsKeyDown(KEY_S)) keyboardDirection.y += 1.0f;

    if (Vector2Length(keyboardDirection) > 0.01f)
    {
        joystickDirection = Vector2Normalize(keyboardDirection);
        joystickActive = true;
        return;
    }

    joystickActive = false;
    joystickDirection = { 0.0f, 0.0f };
}

void Game::DrawJoystick()
{
    if (!useJoystickMovement)
    {
        return;
    }

    Vector2 base = GetJoystickBaseScreen();

    Vector2 knob = Vector2Add(
        base,
        Vector2Scale(joystickDirection, joystickRadius)
    );

    DrawCircleV(base, joystickRadius, { 0, 0, 0, 90 });
    DrawCircleLines(
        static_cast<int>(base.x),
        static_cast<int>(base.y),
        joystickRadius,
        { 255, 255, 255, 110 }
    );

    DrawCircleV(knob, joystickKnobRadius, { 255, 255, 255, 140 });
    DrawCircleLines(
        static_cast<int>(knob.x),
        static_cast<int>(knob.y),
        joystickKnobRadius,
        WHITE
    );
}

int Game::FindSkillSlotIndex(SkillType type) const
{
    for (int i = 0; i < 3; ++i)
    {
        if (skills[i].type == type)
        {
            return i;
        }
    }

    return -1;
}

const char* Game::GetSkillDisplayName(SkillType type) const
{
    switch (type)
    {
    case SkillType::SpinningBlade: return "Spinning Blades";
    case SkillType::Huashan:       return "Huashan";
    case SkillType::Dongfeng:      return "Dongfeng";

    case SkillType::Fireball:      return "Legacy Fireball";
    case SkillType::Lightning:     return "Legacy Lightning";
    case SkillType::IceField:      return "Legacy Ice Field";

    default:                       return "Unknown Skill";
    }
}

const char* Game::GetSkillSubtitle(SkillType type) const
{
    switch (type)
    {
    case SkillType::SpinningBlade: return "Orbit Skill";
    case SkillType::Huashan:       return "Impact Skill";
    case SkillType::Dongfeng:      return "Piercing Skill";

    case SkillType::Fireball:      return "Legacy Skill";
    case SkillType::Lightning:     return "Legacy Skill";
    case SkillType::IceField:      return "Legacy Skill";

    default:                       return "Skill";
    }
}

const char* Game::GetSkillDescription(SkillType type) const
{
    switch (type)
    {
    case SkillType::SpinningBlade:
        return "Summon spinning blades that orbit around the player.";
    case SkillType::Huashan:
        return "Leap to the nearest enemy and crash down with a heavy area strike.";
    case SkillType::Dongfeng:
        return "After a short cast, release a long piercing wind strike.";

    case SkillType::Fireball:
        return "Legacy placeholder skill. Remove later.";
    case SkillType::Lightning:
        return "Legacy placeholder skill. Remove later.";
    case SkillType::IceField:
        return "Legacy placeholder skill. Remove later.";

    default:
        return "A martial skill.";
    }
}

Rectangle Game::GetUpgradeCardRect(int index) const
{
    constexpr int choiceCount = 3;

    float screenW =
        static_cast<float>(GetScreenWidth());

    float screenH =
        static_cast<float>(GetScreenHeight());

    float topY = Clamp(
        screenH * 0.22f,
        112.0f,
        160.0f
    );

    float bottomMargin = Clamp(
        screenH * 0.035f,
        12.0f,
        28.0f
    );

    float availableH =
        screenH - topY - bottomMargin;

    if (availableH < 100.0f)
    {
        availableH = 100.0f;
    }

    // Normal landscape layout:
    // three vertical cards in one horizontal row.
    bool usePortraitRow = screenW >= 620.0f;

    if (usePortraitRow)
    {
        float marginX = Clamp(
            screenW * 0.035f,
            16.0f,
            56.0f
        );

        float gap = Clamp(
            screenW * 0.018f,
            10.0f,
            22.0f
        );

        float cardW =
            (
                screenW -
                marginX * 2.0f -
                gap * static_cast<float>(choiceCount - 1)
                ) /
            static_cast<float>(choiceCount);

        cardW = std::min(cardW, 340.0f);

        float cardH = std::min(
            availableH,
            std::min(
                460.0f,
                cardW * 1.85f
            )
        );

        float totalW =
            cardW * choiceCount +
            gap * static_cast<float>(choiceCount - 1);

        float startX =
            screenW * 0.5f -
            totalW * 0.5f;

        float y =
            topY +
            std::max(
                0.0f,
                (availableH - cardH) * 0.5f
            );

        return {
            startX +
                static_cast<float>(index) *
                (cardW + gap),
            y,
            cardW,
            cardH
        };
    }

    // Narrow mobile portrait fallback.
    // The cards become stacked so the text remains readable.
    float marginX = Clamp(
        screenW * 0.04f,
        12.0f,
        24.0f
    );

    float gap = Clamp(
        screenH * 0.015f,
        8.0f,
        14.0f
    );

    float cardW =
        screenW - marginX * 2.0f;

    float cardH =
        (
            availableH -
            gap * static_cast<float>(choiceCount - 1)
            ) /
        static_cast<float>(choiceCount);

    return {
        marginX,
        topY +
            static_cast<float>(index) *
            (cardH + gap),
        cardW,
        cardH
    };
}

void Game::UpdateUpgradeChoiceLayout()
{
    for (
        int i = 0;
        i < static_cast<int>(currentUpgradeChoices.size());
        ++i
        )
    {
        currentUpgradeChoices[i].cardRect =
            GetUpgradeCardRect(i);
    }
}


void Game::GenerateSkillChoices()
{
    currentUpgradeChoices.clear();

    SkillType pool[3] =
    {
        SkillType::SpinningBlade,
        SkillType::Huashan,
        SkillType::Dongfeng
    };

    for (int i = 0; i < 3; ++i)
    {
        SkillType type = pool[i];

        UpgradeChoice choice;
        choice.skillType = type;
        choice.title = GetSkillDisplayName(type);
        choice.subtitle = GetSkillSubtitle(type);
        choice.description = GetSkillDescription(type);
        choice.cardRect = {};

        int slotIndex = FindSkillSlotIndex(type);

        if (slotIndex >= 0 && skills[slotIndex].unlocked)
        {
            choice.unlocksSkill = false;
            choice.targetLevel = skills[slotIndex].level + 1;
        }
        else
        {
            choice.unlocksSkill = true;
            choice.targetLevel = 1;
        }

        currentUpgradeChoices.push_back(choice);
    }
    UpdateUpgradeChoiceLayout();
}

Vector2 Game::GetJoystickBaseScreen() const
{
    return {
        95.0f,
        static_cast<float>(GetScreenHeight()) - 95.0f
    };
}

bool Game::IsScreenPointInsideJoystick(Vector2 screenPos) const
{
    Vector2 base = GetJoystickBaseScreen();

    return Vector2Distance(screenPos, base) <= joystickRadius + 35.0f;
}

bool Game::CanPlayerStandAt(
    Vector2 worldPosition
) const
{
    int targetX = 0;
    int targetY = 0;

    if (
        !WorldToCell(
            worldPosition,
            targetX,
            targetY
        )
        )
    {
        return false;
    }

    if (
        IsCellBlocked(
            targetX,
            targetY
        )
        )
    {
        return false;
    }

    const float terrainCollisionRadius =
        Clamp(
            playerTerrainCollisionRadius,
            4.0f,
            TileSize * 0.45f
        );

    return
        !IsTerrainCircleBlocked(
            playerPosition,
            worldPosition,
            terrainCollisionRadius
        );
}

float Game::GetCurrentPlayerMoveSpeed() const
{
    const bool attackMovementSlowActive =
        attackButtonDown ||
        playerAnimationState ==
        PlayerAnimationState::Attacking;

    if (
        !attackMovementSlowActive ||
        dashActive
        )
    {
        return
            playerSpeed;
    }

    return
        playerSpeed *
        Clamp(
            playerAttackMovementSpeedMultiplier,
            0.0f,
            1.0f
        );
}

void Game::MovePlayerWithJoystick(
    float dt
)
{
    if (
        !useJoystickMovement ||
        !joystickActive ||
        activeDialogueNpc != -1
        )
    {
        return;
    }

    if (
        Vector2Length(
            joystickDirection
        ) <= 0.05f
        )
    {
        return;
    }

    currentPath.clear();
    hasPath = false;
    pendingNpc = -1;

    Vector2 movementDirection =
        ViewDirectionToWorldDirection(
            joystickDirection
        );

    if (
        Vector2Length(
            movementDirection
        ) <= 0.001f
        )
    {
        return;
    }

    movementDirection =
        Vector2Normalize(
            movementDirection
        );

    const float totalDistance =
        GetCurrentPlayerMoveSpeed() *
        dt;

    const float maximumStep =
        std::max(
            1.0f,
            playerCollisionSubstep
        );

    const int stepCount =
        std::max(
            1,
            static_cast<int>(
                std::ceil(
                    totalDistance /
                    maximumStep
                )
                )
        );

    const Vector2 stepMovement =
        Vector2Scale(
            movementDirection,
            totalDistance /
            static_cast<float>(
                stepCount
                )
        );

    const bool diagonalInput =
        std::fabs(
            joystickDirection.x
        ) >
        0.20f &&
        std::fabs(
            joystickDirection.y
        ) >
        0.20f;

    // Diagonal input should slide at full speed.
    // Direct wall input receives only gentle automatic gliding.
    const float slideScale =
        diagonalInput
        ? 1.0f
        : Clamp(
            playerDirectWallGlideStrength,
            0.0f,
            1.0f
        );

    for (
        int stepIndex = 0;
        stepIndex < stepCount;
        ++stepIndex
        )
    {
        const Vector2 fullCandidate =
            Vector2Add(
                playerPosition,
                stepMovement
            );

        if (
            CanPlayerStandAt(
                fullCandidate
            )
            )
        {
            playerPosition =
                fullCandidate;

            continue;
        }

        const Vector2 xMovement{
            stepMovement.x *
                slideScale,
            0.0f
        };

        const Vector2 yMovement{
            0.0f,
            stepMovement.y *
                slideScale
        };

        const bool hasXMovement =
            std::fabs(
                xMovement.x
            ) >
            0.0001f;

        const bool hasYMovement =
            std::fabs(
                yMovement.y
            ) >
            0.0001f;

        const Vector2 xCandidate =
            Vector2Add(
                playerPosition,
                xMovement
            );

        const Vector2 yCandidate =
            Vector2Add(
                playerPosition,
                yMovement
            );

        const bool canMoveX =
            hasXMovement &&
            CanPlayerStandAt(
                xCandidate
            );

        const bool canMoveY =
            hasYMovement &&
            CanPlayerStandAt(
                yCandidate
            );

        bool movedAlongWall = false;

        if (
            canMoveX ||
            canMoveY
            )
        {
            // Prefer the larger component instead of always
            // preferring world X.
            const bool moveXFirst =
                canMoveX &&
                (
                    !canMoveY ||
                    std::fabs(
                        stepMovement.x
                    ) >=
                    std::fabs(
                        stepMovement.y
                    )
                    );

            if (moveXFirst)
            {
                playerPosition =
                    xCandidate;

                movedAlongWall = true;

                // Try to retain a small amount of the second axis.
                const Vector2 secondCandidate =
                    Vector2Add(
                        playerPosition,
                        yMovement
                    );

                if (
                    hasYMovement &&
                    CanPlayerStandAt(
                        secondCandidate
                    )
                    )
                {
                    playerPosition =
                        secondCandidate;
                }
            }
            else
            {
                playerPosition =
                    yCandidate;

                movedAlongWall = true;

                const Vector2 secondCandidate =
                    Vector2Add(
                        playerPosition,
                        xMovement
                    );

                if (
                    hasXMovement &&
                    CanPlayerStandAt(
                        secondCandidate
                    )
                    )
                {
                    playerPosition =
                        secondCandidate;
                }
            }
        }

        if (movedAlongWall)
        {
            continue;
        }

        // --------------------------------------------------
        // Gentle angular wall assist
        //
        // This is mainly for direct input into a wall.
        // It tries increasingly sideways directions at a
        // reduced speed, preventing the player from feeling
        // completely glued to the terrain edge.
        // --------------------------------------------------

        const float glideDistance =
            Vector2Length(
                stepMovement
            ) *
            Clamp(
                playerDirectWallGlideStrength,
                0.0f,
                1.0f
            );

        if (glideDistance <= 0.001f)
        {
            break;
        }

        constexpr float probeAngles[]{
            15.0f,
            30.0f,
            45.0f,
            60.0f,
            75.0f,
            90.0f
        };

        // Horizontal input determines which side is preferred.
        // With pure north input, positive is chosen consistently
        // rather than alternating every frame.
        const float preferredSign =
            joystickDirection.x < 0.0f
            ? -1.0f
            : 1.0f;

        bool foundGlide =
            false;

        for (
            float angleDegrees :
        probeAngles
            )
        {
            for (
                int sideIndex = 0;
                sideIndex < 2;
                ++sideIndex
                )
            {
                const float sign =
                    sideIndex == 0
                    ? preferredSign
                    : -preferredSign;

                const float angle =
                    angleDegrees *
                    DEG2RAD *
                    sign;

                const float cosine =
                    cosf(
                        angle
                    );

                const float sine =
                    sinf(
                        angle
                    );

                Vector2 probeDirection{
                    movementDirection.x *
                        cosine -
                        movementDirection.y *
                        sine,

                    movementDirection.x *
                        sine +
                        movementDirection.y *
                        cosine
                };

                const Vector2 probeCandidate =
                    Vector2Add(
                        playerPosition,
                        Vector2Scale(
                            probeDirection,
                            glideDistance
                        )
                    );

                if (
                    CanPlayerStandAt(
                        probeCandidate
                    )
                    )
                {
                    playerPosition =
                        probeCandidate;

                    foundGlide =
                        true;

                    break;
                }
            }

            if (foundGlide)
            {
                break;
            }
        }

        if (!foundGlide)
        {
            // The player is in a genuine enclosed corner.
            break;
        }
    }
}

Vector2 Game::GetViewDirectionFromPlayerDirection(
    PlayerDirection direction
) const
{
    constexpr float diagonal = 0.70710678118f;

    switch (direction)
    {
    case PlayerDirection::Down:      return { 0.0f, 1.0f };
    case PlayerDirection::DownRight: return { diagonal, diagonal };
    case PlayerDirection::Right:     return { 1.0f, 0.0f };
    case PlayerDirection::UpRight:   return { diagonal, -diagonal };
    case PlayerDirection::Up:        return { 0.0f, -1.0f };
    case PlayerDirection::UpLeft:    return { -diagonal, -diagonal };
    case PlayerDirection::Left:      return { -1.0f, 0.0f };
    case PlayerDirection::DownLeft:  return { -diagonal, diagonal };
    default:                         return { 0.0f, 1.0f };
    }
}

Vector2 Game::GetDashDirectionWorld() const
{
    Vector2 viewDirection =
        GetPlayerAnimationMoveDirection();

    if (Vector2Length(viewDirection) <= 0.05f)
    {
        viewDirection =
            GetViewDirectionFromPlayerDirection(
                lastPlayerDirection
            );
    }

    Vector2 worldDirection =
        ViewDirectionToWorldDirection(
            viewDirection
        );

    if (Vector2Length(worldDirection) <= 0.001f)
    {
        worldDirection = { 0.0f, 1.0f };
    }

    return Vector2Normalize(worldDirection);
}

void Game::UpdateDashButtonRect()
{
    float screenW =
        static_cast<float>(GetScreenWidth());

    float screenH =
        static_cast<float>(GetScreenHeight());

    constexpr float size = 72.0f;
    constexpr float gap = 16.0f;

    float attackCenterX =
        attackButtonRect.x +
        attackButtonRect.width * 0.5f;

    dashButtonRect = {
        attackCenterX - size * 0.5f,
        attackButtonRect.y - gap - size,
        size,
        size
    };

    // Keep the button visible on very short browser viewports.
    if (dashButtonRect.y < 70.0f)
    {
        dashButtonRect.y = 70.0f;
    }

    (void)screenW;
    (void)screenH;
}

void Game::UpdateDashButton()
{
    bool pointerDownNow = false;

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
    {
        pointerDownNow =
            CheckCollisionPointRec(
                GetMousePosition(),
                dashButtonRect
            );
    }

    int touchCount = GetTouchPointCount();

    for (int i = 0; i < touchCount; ++i)
    {
        if (CheckCollisionPointRec(
            GetTouchPosition(i),
            dashButtonRect
        ))
        {
            pointerDownNow = true;
            break;
        }
    }

    bool keyboardPressed =
        IsKeyPressed(KEY_LEFT_SHIFT) ||
        IsKeyPressed(KEY_RIGHT_SHIFT);

    bool keyboardDown =
        IsKeyDown(KEY_LEFT_SHIFT) ||
        IsKeyDown(KEY_RIGHT_SHIFT);

    dashButtonPressed =
        keyboardPressed ||
        (pointerDownNow && !dashButtonWasDown);

    dashButtonDown =
        pointerDownNow ||
        keyboardDown;

    dashButtonWasDown =
        pointerDownNow;
}

void Game::TryStartDash()
{
    if (
        dashActive ||
        dashCooldownRemaining > 0.0f ||
        gameState != GameState::Playing ||
        activeDialogueNpc != -1 ||
        huashanJumpActive ||
        dongfengCasting
        )
    {
        return;
    }

    CancelPlayerAttackAnimation();

    dashDirectionWorld =
        GetDashDirectionWorld();

    Vector2 dashDirectionView =
        WorldVectorToView(dashDirectionWorld);

    if (Vector2Length(dashDirectionView) > 0.01f)
    {
        dashDirectionView =
            Vector2Normalize(dashDirectionView);

        playerDirection =
            GetPlayerDirectionFromVector(
                dashDirectionView
            );

        lastPlayerDirection =
            playerDirection;
    }

    currentPath.clear();
    hasPath = false;
    pendingNpc = -1;

    dashActive = true;
    dashTimer = dashDuration;
    dashCooldownRemaining = dashCooldown;
    dashInvulnerabilityTimer = 0.0f;
    dashAfterimageTimer = 0.0f;

    // Keep the running pose active during the burst.
    playerAnimationState =
        PlayerAnimationState::Walking;

    SpawnDashAfterimage();
}

void Game::EndDash()
{
    if (!dashActive)
    {
        return;
    }

    dashActive = false;
    dashTimer = 0.0f;
    dashInvulnerabilityTimer =
        dashInvulnerabilityGrace;

    SpawnDashAfterimage();
}

void Game::UpdateDashAfterimages(float dt)
{
    for (DashAfterimage& afterimage : dashAfterimages)
    {
        afterimage.life -= dt;
    }

    dashAfterimages.erase(
        std::remove_if(
            dashAfterimages.begin(),
            dashAfterimages.end(),
            [](const DashAfterimage& afterimage)
            {
                return afterimage.life <= 0.0f;
            }
        ),
        dashAfterimages.end()
    );
}

void Game::SpawnDashAfterimage()
{
    if (
        !playerSpriteLoaded ||
        playerSpriteSheet.id == 0
        )
    {
        return;
    }

    if (
        static_cast<int>(
            dashAfterimages.size()
            ) >=
        maxDashAfterimages
        )
    {
        dashAfterimages.erase(
            dashAfterimages.begin()
        );
    }

    DashAfterimage afterimage;

    afterimage.worldPosition =
        playerPosition;

    afterimage.direction =
        playerDirection;

    afterimage.frame =
        std::max(
            0,
            std::min(
                playerAnimFrame,
                std::max(
                    1,
                    playerFramesPerRow
                ) - 1
            )
        );

    afterimage.useBossSprite =
        false;

    afterimage.life =
        dashAfterimageLifetime;

    afterimage.maxLife =
        dashAfterimageLifetime;

    dashAfterimages.push_back(
        afterimage
    );
}

void Game::UpdateDash(float dt)
{
    UpdateDashAfterimages(dt);

    if (dashInvulnerabilityTimer > 0.0f)
    {
        dashInvulnerabilityTimer -= dt;

        if (dashInvulnerabilityTimer < 0.0f)
        {
            dashInvulnerabilityTimer = 0.0f;
        }
    }

    if (dashCooldownRemaining > 0.0f)
    {
        dashCooldownRemaining -= dt;

        if (dashCooldownRemaining < 0.0f)
        {
            dashCooldownRemaining = 0.0f;
        }
    }

    if (dashButtonPressed)
    {
        TryStartDash();
    }

    // Consume the edge-triggered press.
    dashButtonPressed = false;

    if (!dashActive)
    {
        return;
    }

    float movementTime =
        std::min(dt, dashTimer);

    dashTimer -= dt;

    dashAfterimageTimer -= movementTime;

    while (dashAfterimageTimer <= 0.0f)
    {
        SpawnDashAfterimage();
        dashAfterimageTimer +=
            dashAfterimageInterval;
    }

    float remainingDistance =
        dashSpeed * movementTime;

    // Small collision steps prevent a fast dash from tunnelling
    // through blocked tiles or obstacle footprints.
    constexpr float collisionStep = 10.0f;

    bool hitObstacle = false;

    while (remainingDistance > 0.001f)
    {
        float step =
            std::min(
                remainingDistance,
                collisionStep
            );

        Vector2 nextPosition =
            Vector2Add(
                playerPosition,
                Vector2Scale(
                    dashDirectionWorld,
                    step
                )
            );

        if (!CanPlayerStandAt(nextPosition))
        {
            hitObstacle = true;
            break;
        }

        playerPosition = nextPosition;
        remainingDistance -= step;
    }

    player.pos = playerPosition;

    if (hitObstacle || dashTimer <= 0.0f)
    {
        EndDash();
    }
}

bool Game::IsPlayerInvulnerable() const
{
    return
        dashActive ||
        dashInvulnerabilityTimer > 0.0f;
}

void Game::DrawDashButton()
{
    Vector2 center{
        dashButtonRect.x +
            dashButtonRect.width * 0.5f,
        dashButtonRect.y +
            dashButtonRect.height * 0.5f
    };

    bool ready =
        dashCooldownRemaining <= 0.0f;

    Color fill;

    if (!ready)
    {
        fill = { 35, 55, 75, 210 };
    }
    else if (dashButtonDown || dashActive)
    {
        fill = { 70, 205, 255, 240 };
    }
    else
    {
        fill = { 35, 125, 180, 220 };
    }

    DrawCircleV(
        center,
        dashButtonRect.width * 0.5f,
        fill
    );

    DrawCircleLines(
        static_cast<int>(center.x),
        static_cast<int>(center.y),
        dashButtonRect.width * 0.5f,
        ready ? WHITE : GRAY
    );

    DrawText(
        "DASH",
        static_cast<int>(center.x - 25.0f),
        static_cast<int>(center.y - 9.0f),
        18,
        ready ? WHITE : LIGHTGRAY
    );

    if (!ready)
    {
        DrawText(
            TextFormat("%.1f", dashCooldownRemaining),
            static_cast<int>(center.x - 14.0f),
            static_cast<int>(center.y + 13.0f),
            16,
            YELLOW
        );
    }
}

void Game::DrawDashAfterimage(
    const DashAfterimage& afterimage
)
{
    if (!playerSpriteLoaded ||
        playerSpriteSheet.id == 0 ||
        afterimage.maxLife <= 0.0f)
    {
        return;
    }

    float lifeRatio =
        Clamp(
            afterimage.life /
            afterimage.maxLife,
            0.0f,
            1.0f
        );

    int frame =
        std::max(
            0,
            std::min(
                afterimage.frame,
                playerFramesPerRow - 1
            )
        );

    int row =
        GetPlayerDirectionRow(
            afterimage.direction
        );

    Rectangle source{
        static_cast<float>(
            frame * playerFrameWidth
        ),
        static_cast<float>(
            row * playerFrameHeight
        ),
        static_cast<float>(playerFrameWidth),
        static_cast<float>(playerFrameHeight)
    };

    const float drawScale =
        playerSpriteDrawScale;

    Vector2 viewPosition =
        WorldToViewElevated(
            afterimage.worldPosition
        );

    Rectangle destination{
        viewPosition.x,
        viewPosition.y,
        static_cast<float>(playerFrameWidth) *
            drawScale,
        static_cast<float>(playerFrameHeight) *
            drawScale
    };

    Vector2 origin{
        static_cast<float>(playerFrameWidth) *
            drawScale * 0.5f,
        static_cast<float>(playerFrameHeight) *
            drawScale * 0.84f
    };

    float alpha =
        lifeRatio * lifeRatio * 0.95f;

    if (dashAfterimageShaderLoaded &&
        dashAfterimageShader.id != 0 &&
        dashAfterimageColorLocation >= 0)
    {
        float shaderColor[4]{
            0.25f + 0.55f * lifeRatio,
            0.78f + 0.22f * lifeRatio,
            1.0f,
            alpha
        };

        SetShaderValue(
            dashAfterimageShader,
            dashAfterimageColorLocation,
            shaderColor,
            SHADER_UNIFORM_VEC4
        );

        BeginBlendMode(BLEND_ADDITIVE);
        BeginShaderMode(dashAfterimageShader);

        DrawTexturePro(
            playerSpriteSheet,
            source,
            destination,
            origin,
            0.0f,
            WHITE
        );

        EndShaderMode();
        EndBlendMode();
    }
    else
    {
        Color fallbackTint{
            90,
            220,
            255,
            static_cast<unsigned char>(
                255.0f * alpha
            )
        };

        DrawTexturePro(
            playerSpriteSheet,
            source,
            destination,
            origin,
            0.0f,
            fallbackTint
        );
    }
}

void Game::UpdateAttackButtonRect()
{
    float screenW = static_cast<float>(GetScreenWidth());
    float screenH = static_cast<float>(GetScreenHeight());

    attackButtonRect = {
        screenW - 112.0f,
        screenH - 112.0f,
        88.0f,
        88.0f
    };
}

void Game::UpdateAttackButton()
{
    attackButtonDown = false;

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
    {
        Vector2 mouse = GetMousePosition();

        if (CheckCollisionPointRec(mouse, attackButtonRect))
        {
            attackButtonDown = true;
        }
    }

    int touchCount = GetTouchPointCount();

    for (int i = 0; i < touchCount; ++i)
    {
        Vector2 touch = GetTouchPosition(i);

        if (CheckCollisionPointRec(touch, attackButtonRect))
        {
            attackButtonDown = true;
            return;
        }
    }
}

Vector2 Game::GetMeleeApproachPoint(const Enemy& enemy) const
{
    Vector2 fromEnemyToPlayer = Vector2Subtract(playerPosition, enemy.pos);

    if (Vector2Length(fromEnemyToPlayer) <= 0.01f)
    {
        fromEnemyToPlayer = { 1.0f, 0.0f };
    }

    fromEnemyToPlayer = Vector2Normalize(fromEnemyToPlayer);

    float desiredDistance = enemy.radius + player.radius + 12.0f;

    return Vector2Add(enemy.pos, Vector2Scale(fromEnemyToPlayer, desiredDistance));
}

void Game::UpdateMeleeAttack(
    float dt
)
{
    // Cooldown continues progressing regardless of whether
    // the button is being held.
    player.attackTimer +=
        dt;

    if (
        player.attackTimer >
        player.attackInterval
        )
    {
        player.attackTimer =
            player.attackInterval;
    }

    // --------------------------------------------------
    // Apply the hit at the configured animation frame
    // --------------------------------------------------

    if (playerAttackImpactPending)
    {
        playerAttackImpactPending =
            false;

        Enemy* impactTarget =
            FindEnemyById(
                playerAttackTargetId
            );

        if (
            impactTarget != nullptr &&
            impactTarget->active
            )
        {
            const float distance =
                Vector2Distance(
                    playerPosition,
                    impactTarget->pos
                );

            const float hitRange =
                player.meleeRange +
                impactTarget->radius +
                35.0f;

            if (
                distance <=
                hitRange
                )
            {
                DealMeleeHit(
                    *impactTarget
                );
            }
        }

        playerAttackTargetId = 0;
    }

    // Dash movement owns the player while active.
    // TryStartDash() already cancels any current attack.
    if (
        dashActive ||
        IsPlayerMovementLocked()
        )
    {
        return;
    }

    // Wait for the current attack animation to finish.
    if (
        playerAnimationState ==
        PlayerAnimationState::Attacking
        )
    {
        return;
    }

    // Releasing the button stops automatic repeat.
    if (!attackButtonDown)
    {
        return;
    }

    // Respect the normal melee cooldown.
    if (
        player.attackTimer <
        player.attackInterval
        )
    {
        return;
    }

    // Find an enemy only for facing and possible damage.
    // The player never moves toward this enemy.
    Enemy* target =
        FindNearestEnemy(
            playerPosition,
            playerAttackAutoFaceRange
        );

    player.attackTimer =
        0.0f;

    // A null target is valid and produces an air attack.
    StartPlayerAttackAnimation(
        target
    );
}

void Game::DealMeleeHit(Enemy& targetEnemy)
{
    int mainDamage = player.attackDamage;
    int splashDamage = static_cast<int>(player.attackDamage * 0.55f);

    if (splashDamage < 1)
    {
        splashDamage = 1;
    }

    float splashRadius = 120.0f;

    Vector2 hitCenter = targetEnemy.pos;

    SpawnSlashEffect(playerPosition, targetEnemy.pos);

    for (Enemy& enemy : enemies)
    {
        if (
            !enemy.active ||
            IsEnemySpawnProtected(
                enemy
            )
            )
        {
            continue;
        }

        float distance = Vector2Distance(enemy.pos, hitCenter);

        if (distance > splashRadius + enemy.radius)
        {
            continue;
        }

        int damage = (&enemy == &targetEnemy) ? mainDamage : splashDamage;

        ApplyDamageToEnemy(enemy, damage, enemy.pos);

        if (!enemy.active)
        {
            continue;
        }

        // Bosses take damage and flash, but basic melee
// attacks do not physically move them.
        if (
            enemy.type ==
            EnemyType::Boss
            )
        {
            continue;
        }

        Vector2 knockbackDirection = Vector2Subtract(enemy.pos, playerPosition);

        if (Vector2Length(knockbackDirection) > 0.01f)
        {
            knockbackDirection = Vector2Normalize(knockbackDirection);
            Vector2 knockedPos = Vector2Add(enemy.pos, Vector2Scale(knockbackDirection, 18.0f));

            int cellX = 0;
            int cellY = 0;

            if (WorldToCell(knockedPos, cellX, cellY) && !IsCellBlocked(cellX, cellY))
            {
                enemy.pos = knockedPos;
            }
        }
    }
}

void Game::SpawnSlashEffect(Vector2 start, Vector2 end)
{
    if (vfxParticles.size() >= 400)
    {
        return;
    }

    VfxParticle p;
    p.type = VfxType::SlashLine;
    p.pos = start;
    p.endPos = end;
    p.radius = 18.0f;
    p.life = 0.14f;
    p.maxLife = 0.14f;
    p.color = { 180, 230, 255, 255 };
    p.active = true;

    vfxParticles.push_back(p);
}

void Game::DrawAttackButton()
{
    Vector2 center = {
        attackButtonRect.x + attackButtonRect.width * 0.5f,
        attackButtonRect.y + attackButtonRect.height * 0.5f
    };

    Color fill = attackButtonDown
        ? Color{ 190, 70, 70, 230 }
    : Color{ 120, 50, 50, 210 };

    DrawCircleV(center, attackButtonRect.width * 0.5f, fill);
    DrawCircleLines(
        static_cast<int>(center.x),
        static_cast<int>(center.y),
        attackButtonRect.width * 0.5f,
        WHITE
    );

    DrawText(
        "ATK",
        static_cast<int>(center.x - 24.0f),
        static_cast<int>(center.y - 12.0f),
        24,
        WHITE
    );
}

void Game::RefreshEnemyPath(
    Enemy& enemy
)
{
    enemy.path.clear();
    enemy.pathIndex = 0;
    enemy.pathRefreshTimer = 0.0f;

    std::vector<Vector2> newPath;

    if (
        FindPath(
            enemy.pos,
            playerPosition,
            newPath
        )
        )
    {
        enemy.path =
            newPath;

        enemy.pathIndex =
            0;

        return;
    }

    // Failed path searches wait before trying again.
    enemy.pathRefreshTimer =
        -0.75f;
}

void Game::MoveEnemyAlongPath(
    Enemy& enemy,
    float dt
)
{
    if (
        enemy.path.empty() ||
        enemy.pathIndex >=
        static_cast<int>(
            enemy.path.size()
            )
        )
    {
        return;
    }

    Vector2 target =
        enemy.path[
            enemy.pathIndex
        ];

    float speedMultiplier =
        1.0f;

    if (
        enemy.slowTimer >
        0.0f
        )
    {
        speedMultiplier =
            enemy.slowMultiplier;
    }

    float step =
        enemy.speed *
        speedMultiplier *
        dt;

    Vector2 candidatePosition =
        MoveTowards(
            enemy.pos,
            target,
            step
        );

    float collisionRadius =
        std::max(
            6.0f,
            enemy.radius *
            0.55f
        );

    if (
        !CanEnemyStandAt(
            enemy.pos,
            candidatePosition,
            collisionRadius
        )
        )
    {
        enemy.path.clear();
        enemy.pathIndex = 0;

        // Request another path during the next update.
        enemy.pathRefreshTimer =
            enemy.pathRefreshInterval;

        return;
    }

    enemy.pos =
        candidatePosition;

    if (
        Vector2Distance(
            enemy.pos,
            target
        ) <
        3.0f
        )
    {
        enemy.pathIndex++;
    }
}

bool Game::IsScreenPointOnCombatUi(Vector2 screenPos) const
{

    if (
        CheckCollisionPointRec(
            screenPos,
            debugUpgradeButtonRect
        )
        )
    {
        return true;
    }

    if (useJoystickMovement && IsScreenPointInsideJoystick(screenPos))
    {
        return true;
    }

    if (CheckCollisionPointRec(screenPos, attackButtonRect) ||
        CheckCollisionPointRec(screenPos, dashButtonRect))
    {
        return true;
    }

    for (const SkillSlot& skill : skills)
    {
        if (CheckCollisionPointRec(screenPos, skill.buttonRect))
        {
            return true;
        }
    }

    return false;
}

void Game::UpdateOrbitalBlades(float dt)
{
    for (OrbitalBlade& blade : orbitalBlades)
    {
        if (!blade.active)
        {
            continue;
        }

        blade.life -= dt;

        if (blade.life <= 0.0f)
        {
            blade.active = false;
            continue;
        }

        blade.angle += blade.angularSpeed * dt;

        Vector2 bladePos = {
            playerPosition.x + cosf(blade.angle) * blade.orbitRadius,
            playerPosition.y + sinf(blade.angle) * blade.orbitRadius
        };

        blade.hitTimer += dt;

        if (blade.hitTimer < blade.hitInterval)
        {
            continue;
        }

        blade.hitTimer = 0.0f;

        for (Enemy& enemy : enemies)
        {
            if (!enemy.active)
            {
                continue;
            }

            if (CheckCollisionCircles(bladePos, blade.bladeRadius, enemy.pos, enemy.radius))
            {
                ApplyDamageToEnemy(enemy, blade.damage, enemy.pos);

                Vector2 knockbackDirection = Vector2Subtract(enemy.pos, playerPosition);

                if (Vector2Length(knockbackDirection) > 0.01f)
                {
                    knockbackDirection = Vector2Normalize(knockbackDirection);

                    Vector2 knockedPos = Vector2Add(
                        enemy.pos,
                        Vector2Scale(knockbackDirection, 10.0f)
                    );

                    int cellX = 0;
                    int cellY = 0;

                    if (WorldToCell(knockedPos, cellX, cellY) && !IsCellBlocked(cellX, cellY))
                    {
                        enemy.pos = knockedPos;
                    }
                }
            }
        }
    }

    orbitalBlades.erase(
        std::remove_if(
            orbitalBlades.begin(),
            orbitalBlades.end(),
            [](const OrbitalBlade& blade)
            {
                return !blade.active;
            }
        ),
        orbitalBlades.end()
    );
}

bool Game::IsBossEnemy(const Enemy& enemy) const
{
    return enemy.type == EnemyType::Boss;
}

bool Game::IsEnemyCrowdControlled(const Enemy& enemy) const
{
    return enemy.frozenTimer > 0.0f ||
        enemy.stunTimer > 0.0f ||
        enemy.airborneTimer > 0.0f ||
        enemy.landingStunTimer > 0.0f;
}

float Game::GetEnemyKnockbackScale(const Enemy& enemy) const
{
    float resistance = enemy.weight * enemy.knockbackResistance;

    if (resistance < 0.10f)
    {
        resistance = 0.10f;
    }

    return 1.0f / resistance;
}

float Game::GetBossSkillDamageMultiplier(
    SkillType skillType,
    const Enemy& enemy
) const
{
    if (!IsBossEnemy(enemy))
    {
        return 1.0f;
    }

    if (skillType == SkillType::Huashan)
    {
        return 0.40f;
    }

    if (skillType == SkillType::Dongfeng)
    {
        return 0.65f;
    }

    return 1.0f;
}

int Game::GetWhirlwindBladeCount(int level) const
{
    // Lv.1  = 2 blades
    // Lv.4  = 3 blades
    // Lv.7  = 4 blades
    // Lv.10 = 5 blades
    // Lv.13 = 6 blades
    // Lv.16 = 7 blades
    // Lv.19 = 8 blades

    int bladeCount = 2 + (level - 1) / 3;

    if (bladeCount > 8)
    {
        bladeCount = 8;
    }

    return bladeCount;
}

void Game::DrawOrbitalBlades()
{
    for (const OrbitalBlade& blade : orbitalBlades)
    {
        if (!blade.active)
        {
            continue;
        }

        Vector2 bladeWorldPos = {
            playerPosition.x + cosf(blade.angle) * blade.orbitRadius,
            playerPosition.y + sinf(blade.angle) * blade.orbitRadius
        };

        Vector2 bladePos = WorldToView(bladeWorldPos);

        DrawCircleV(bladePos, blade.bladeRadius + 8.0f, { 120, 210, 255, 80 });
        DrawCircleV(bladePos, blade.bladeRadius, { 180, 235, 255, 230 });
        DrawCircleLines(
            static_cast<int>(bladePos.x),
            static_cast<int>(bladePos.y),
            blade.bladeRadius,
            WHITE
        );
    }
}
void Game::UpdateEnemyReactionTimers(Enemy& enemy, float dt)
{

    if (enemy.damageFlashTimer > 0.0f)
    {
        enemy.damageFlashTimer -= dt;

        if (enemy.damageFlashTimer < 0.0f)
        {
            enemy.damageFlashTimer = 0.0f;
        }
    }

    if (enemy.stunTimer > 0.0f)
    {
        enemy.stunTimer -= dt;
        if (enemy.stunTimer < 0.0f)
        {
            enemy.stunTimer = 0.0f;
        }
    }

    if (enemy.frozenTimer > 0.0f)
    {
        enemy.frozenTimer -= dt;
        if (enemy.frozenTimer < 0.0f)
        {
            enemy.frozenTimer = 0.0f;
        }
    }

    if (enemy.slowTimer > 0.0f)
    {
        enemy.slowTimer -= dt;
        if (enemy.slowTimer < 0.0f)
        {
            enemy.slowTimer = 0.0f;
            enemy.slowMultiplier = 1.0f;
        }
    }
    else
    {
        enemy.slowMultiplier = 1.0f;
    }

    if (enemy.airborneTimer > 0.0f)
    {
        enemy.airborneTimer -= dt;

        if (enemy.airborneMaxTimer > 0.0f)
        {
            float t = 1.0f - enemy.airborneTimer / enemy.airborneMaxTimer;

            if (t < 0.0f)
            {
                t = 0.0f;
            }

            if (t > 1.0f)
            {
                t = 1.0f;
            }

            float maxVisualHeight = IsBossEnemy(enemy) ? 28.0f : 125.0f;
            enemy.visualHeight = sinf(t * PI) * maxVisualHeight;
        }

        if (enemy.airborneTimer <= 0.0f)
        {
            enemy.airborneTimer = 0.0f;
            enemy.airborneMaxTimer = 0.0f;
            enemy.visualHeight = 0.0f;

            if (enemy.landingStunOnLand > 0.0f)
            {
                enemy.landingStunTimer = enemy.landingStunOnLand;
                enemy.landingStunOnLand = 0.0f;
            }
        }
    }
    else
    {
        enemy.visualHeight = 0.0f;
    }

    if (enemy.landingStunTimer > 0.0f)
    {
        enemy.landingStunTimer -= dt;

        if (enemy.landingStunTimer < 0.0f)
        {
            enemy.landingStunTimer = 0.0f;
        }
    }
}

void Game::ApplyEnemyPhysics(Enemy& enemy, float dt)
{
    if (Vector2Length(enemy.knockbackVelocity) <= 1.0f)
    {
        enemy.knockbackVelocity = { 0.0f, 0.0f };
        return;
    }

    Vector2 movement = Vector2Scale(enemy.knockbackVelocity, dt);
    Vector2 nextPos = Vector2Add(enemy.pos, movement);

    if (CanEnemyStandAt(
        enemy.pos,
        nextPos,
        std::max(
            6.0f,
            enemy.radius *
            0.55f
        )
    ))
    {
        enemy.pos = nextPos;
    }
    else
    {
        enemy.knockbackVelocity = { 0.0f, 0.0f };
        return;
    }

    enemy.knockbackVelocity = Vector2Scale(enemy.knockbackVelocity, 0.93f);
}

void Game::ApplyKnockbackToEnemy(
    Enemy& enemy,
    Vector2 origin,
    float force,
    float airborneDuration,
    float landingStunDuration
)
{
    if (
        !enemy.active ||
        IsEnemySpawnProtected(
            enemy
        )
        )
    {
        return;
    }

    // The healing Boss cannot be moved, launched or
// interrupted by knockback skills.
    if (
        enemy.type ==
        EnemyType::Boss &&
        (
            enemy.bossActionState ==
            BossActionState::HealingWindup ||
            enemy.bossActionState ==
            BossActionState::HealingActive
            )
        )
    {
        return;
    }

    Vector2 direction = Vector2Subtract(enemy.pos, origin);

    if (Vector2Length(direction) <= 0.01f)
    {
        direction = { 1.0f, 0.0f };
    }

    direction = Vector2Normalize(direction);

    float finalForce = force * GetEnemyKnockbackScale(enemy);

    if (IsBossEnemy(enemy))
    {
        finalForce *= 0.20f;
        airborneDuration *= 0.20f;
        landingStunDuration *= 0.35f;
    }

    enemy.knockbackVelocity = Vector2Add(
        enemy.knockbackVelocity,
        Vector2Scale(direction, finalForce)
    );

    if (airborneDuration > enemy.airborneTimer)
    {
        enemy.airborneTimer = airborneDuration;
        enemy.airborneMaxTimer = airborneDuration;
        enemy.visualHeight = 1.0f;
    }

    if (landingStunDuration > enemy.landingStunOnLand)
    {
        enemy.landingStunOnLand = landingStunDuration;
    }

    enemy.path.clear();
    enemy.pathIndex = 0;
}

void Game::ApplySkillDamageToEnemy(
    Enemy& enemy,
    SkillType skillType,
    int baseDamage,
    Vector2 hitPos
)
{
    float multiplier = GetBossSkillDamageMultiplier(skillType, enemy);

    int adjustedDamage = static_cast<int>(baseDamage * multiplier);

    if (adjustedDamage < 1)
    {
        adjustedDamage = 1;
    }

    ApplyDamageToEnemy(enemy, adjustedDamage, hitPos);
}

int Game::GetModifiedDamageToEnemy(
    const Enemy& enemy,
    int baseDamage
) const
{
    float multiplier =
        1.0f;

    // Frozen enemies normally take additional damage.
    if (enemy.frozenTimer > 0.0f)
    {
        multiplier *=
            1.50f;
    }

    // Boss remains damageable and still flashes, but
    // receives heavy protection throughout both healing
    // animations.
    if (
        enemy.type ==
        EnemyType::Boss &&
        (
            enemy.bossActionState ==
            BossActionState::HealingWindup ||
            enemy.bossActionState ==
            BossActionState::HealingActive
            )
        )
    {
        multiplier *=
            bossHealingDamageTakenMultiplier;
    }

    int finalDamage =
        static_cast<int>(
            static_cast<float>(
                baseDamage
                ) *
            multiplier
            );

    if (finalDamage < 1)
    {
        finalDamage =
            1;
    }

    return finalDamage;
}

void Game::DamagePlayer(
    int damage
)
{
    if (damage <= 0)
    {
        return;
    }

    player.hp -=
        damage;

    if (player.hp < 0)
    {
        player.hp = 0;
    }

    playerDamageFlashTimer =
        damageFlashDuration;

    SpawnHitSpark(
        playerPosition
    );

    SpawnDamageNumber(
        playerPosition,
        damage
    );
}

void Game::ApplyDamageToEnemy(
    Enemy& enemy,
    int baseDamage,
    Vector2 hitPos
)
{
    if (
        !enemy.active ||
        enemy.chamberId != activeChamberId ||
        IsEnemySpawnProtected(
            enemy
        )
        )
    {
        return;
    }

    const int finalDamage =
        GetModifiedDamageToEnemy(
            enemy,
            baseDamage
        );

    enemy.hp -=
        finalDamage;

    enemy.damageFlashTimer =
        damageFlashDuration;

    SpawnHitSpark(
        hitPos
    );

    SpawnDamageNumber(
        enemy.pos,
        finalDamage
    );

    if (
        enemy.hp <= 0
        )
    {
        enemy.hp = 0;
        enemy.active = false;

        SpawnDeathBurst(
            enemy.pos
        );

        return;
    }

    // --------------------------------------------------
 // Ordinary enemy hit-stun
 //
 // Bosses do not receive ordinary hit-stun from player
 // attacks. Their dedicated interruption system below
 // can still stun them during laser or bombardment.
 // --------------------------------------------------

    if (
        enemy.type !=
        EnemyType::Boss
        )
    {
        float hitStunDuration =
            normalEnemyHitStunDuration;

        if (
            enemy.type ==
            EnemyType::Shooter
            )
        {
            hitStunDuration =
                shooterHitStunDuration;
        }

        enemy.stunTimer =
            std::max(
                enemy.stunTimer,
                hitStunDuration
            );

        enemy.path.clear();
        enemy.pathIndex = 0;
        enemy.pathRefreshTimer = 0.0f;
    }

    // --------------------------------------------------
// Boss channelled-attack interruption
// --------------------------------------------------

    if (
        enemy.type ==
        EnemyType::Boss
        )
    {
        const bool breakableBossAttack =
            enemy.bossActionState ==
            BossActionState::LaserWindup ||
            enemy.bossActionState ==
            BossActionState::LaserActive ||
            enemy.bossActionState ==
            BossActionState::BombardmentRoar;

        if (
            breakableBossAttack &&
            GetBossPowerTier(enemy) < 3
            )
        {
            enemy.bossInterruptDamage +=
                static_cast<float>(
                    finalDamage
                    );

            if (
                enemy.bossInterruptDamage >=
                GetBossInterruptThreshold(
                    enemy
                )
                )
            {
                const float stunDuration =
                    GetBossSpecialStunDuration(
                        enemy
                    );

                if (
                    StartBossStunned(
                        enemy,
                        stunDuration,
                        false
                    )
                    )
                {
                    return;
                }
            }
        }
    }

    // --------------------------------------------------
    // Damage interrupts shooter attacks
    // --------------------------------------------------

    if (
        enemy.type ==
        EnemyType::Shooter
        )
    {
        enemy.animationState =
            EnemyAnimationState::Idle;

        enemy.shooterAnimationFrame =
            0;

        enemy.shooterAnimationTimer =
            0.0f;

        enemy.shooterAttackImpactProcessed =
            false;

        enemy.shooterProjectileFired =
            false;

        enemy.shooterRetreatTimer =
            0.0f;

        enemy.shooterShotsFiredThisAttack =
            0;

        enemy.shooterAttackLoopedOnce =
            false;

        enemy.shooterExtraShotTimer =
            0.0f;

        enemy.shootTimer =
            0.0f;
    }
    /*
        if (
        enemy.type ==
        EnemyType::Boss
        )
    {
        enemy.bossHitCounter++;

        if (
            enemy.bossHitCounter >=
            enemy.bossDashHitThreshold &&
            enemy.bossDashTimer <=
            0.0f &&
            !enemy.bossDashCharging
            )
        {
            StartBossDash(
                enemy
            );
        }
    }
    */
    // Preserve current boss reaction logic.

}

Enemy* Game::FindEnemyById(
    int enemyId
)
{
    if (enemyId == 0)
    {
        return nullptr;
    }

    for (Enemy& enemy : enemies)
    {
        if (
            enemy.active &&
            enemy.id == enemyId
            )
        {
            return &enemy;
        }
    }

    return nullptr;
}

Enemy* Game::FindNearestEnemy(Vector2 fromPos, float range)
{
    Enemy* nearest = nullptr;
    float nearestDistSq = range * range;

    for (Enemy& enemy : enemies)
    {
        if (
            !enemy.active ||
            enemy.chamberId != activeChamberId ||
            IsEnemySpawnProtected(
                enemy
            )
            )
        {
            continue;
        }

        float distSq = DistanceSquared(fromPos, enemy.pos);

        if (distSq < nearestDistSq)
        {
            nearestDistSq = distSq;
            nearest = &enemy;
        }
    }

    return nearest;
}

int Game::GetLightningBranchCount(int level) const
{
    // Lv.1  = 3 initial branches
    // Lv.5  = 4 branches
    // Lv.9  = 5 branches
    // Lv.13 = 6 branches
    // Lv.17 = 7 branches
    // Lv.20 = 8 branches

    int branchCount = 3 + (level - 1) / 4;

    if (level >= 20)
    {
        branchCount = 8;
    }

    if (branchCount > 8)
    {
        branchCount = 8;
    }

    return branchCount;
}

int Game::GetLightningChainCount(int level) const
{
    // Lv.1 chains to 2 extra enemies.
    // Scales up to 10 extra chain jumps.

    int chainCount = 2 + (level - 1) / 2;

    if (chainCount > 10)
    {
        chainCount = 10;
    }

    return chainCount;
}

Enemy* Game::FindNearestEnemyExcluding(
    Vector2 fromPos,
    float range,
    const std::vector<Enemy*>& excluded
)
{
    Enemy* nearest = nullptr;
    float nearestDistSq = range * range;

    for (Enemy& enemy : enemies)
    {
        if (
            !enemy.active ||
            enemy.chamberId != activeChamberId ||
            IsEnemySpawnProtected(
                enemy
            )
            )
        {
            continue;
        }

        bool alreadyExcluded = false;

        for (Enemy* excludedEnemy : excluded)
        {
            if (&enemy == excludedEnemy)
            {
                alreadyExcluded = true;
                break;
            }
        }

        if (alreadyExcluded)
        {
            continue;
        }

        float distSq = DistanceSquared(fromPos, enemy.pos);

        if (distSq < nearestDistSq)
        {
            nearestDistSq = distSq;
            nearest = &enemy;
        }
    }

    return nearest;
}

bool Game::IsScreenPointOnButtonUi(Vector2 screenPos) const
{
    if (
        CheckCollisionPointRec(
            screenPos,
            debugUpgradeButtonRect
        )
        )
    {
        return true;
    }

    if (CheckCollisionPointRec(screenPos, attackButtonRect) ||
        CheckCollisionPointRec(screenPos, dashButtonRect))
    {
        return true;
    }

    for (const SkillSlot& skill : skills)
    {
        if (CheckCollisionPointRec(screenPos, skill.buttonRect))
        {
            return true;
        }
    }

    return false;
}
bool Game::FindHuashanLandingPosition(
    const Enemy& target,
    Vector2& outLandingPosition
) const
{
    Vector2 targetToPlayer =
        Vector2Subtract(
            playerPosition,
            target.pos
        );

    if (
        Vector2Length(
            targetToPlayer
        ) <= 0.01f
        )
    {
        targetToPlayer = {
            1.0f,
            0.0f
        };
    }

    targetToPlayer =
        Vector2Normalize(
            targetToPlayer
        );

    float baseAngle =
        atan2f(
            targetToPlayer.y,
            targetToPlayer.x
        );

    float minimumDistance =
        target.radius +
        player.radius +
        12.0f;

    constexpr int angleSteps = 24;
    constexpr int distanceSteps = 5;

    for (
        int distanceStep = 0;
        distanceStep < distanceSteps;
        ++distanceStep
        )
    {
        float landingDistance =
            minimumDistance +
            static_cast<float>(
                distanceStep
                ) *
            18.0f;

        for (
            int angleIndex = 0;
            angleIndex < angleSteps;
            ++angleIndex
            )
        {
            int sideStep =
                (angleIndex + 1) /
                2;

            float sideSign =
                angleIndex % 2 == 0
                ? 1.0f
                : -1.0f;

            float angleOffset =
                static_cast<float>(
                    sideStep
                    ) *
                sideSign *
                PI /
                12.0f;

            float angle =
                baseAngle +
                angleOffset;

            Vector2 candidate{
                target.pos.x +
                    cosf(angle) *
                    landingDistance,

                target.pos.y +
                    sinf(angle) *
                    landingDistance
            };

            int cellX = 0;
            int cellY = 0;

            if (
                !WorldToCell(
                    candidate,
                    cellX,
                    cellY
                )
                )
            {
                continue;
            }

            if (
                IsCellBlocked(
                    cellX,
                    cellY
                )
                )
            {
                continue;
            }

            bool collidesWithObstacle =
                false;

            for (
                const Obstacle& obstacle :
                obstacles
                )
            {
                if (
                    !obstacle.collisionEnabled ||
                    obstacle.heightLevel > 0
                    )
                {
                    continue;
                }

                if (
                    obstacle.collisionShape ==
                    CollisionShape::Box
                    )
                {
                    Rectangle collider =
                        GetObstacleCollisionRect(
                            obstacle
                        );

                    collider.x -=
                        playerRadius;

                    collider.y -=
                        playerRadius;

                    collider.width +=
                        playerRadius * 2.0f;

                    collider.height +=
                        playerRadius * 2.0f;

                    if (
                        CheckCollisionPointRec(
                            candidate,
                            collider
                        )
                        )
                    {
                        collidesWithObstacle =
                            true;

                        break;
                    }
                }
                else
                {
                    float combinedRadius =
                        obstacle.colliderRadius +
                        playerRadius;

                    if (
                        Vector2Distance(
                            candidate,
                            obstacle.position
                        ) <
                        combinedRadius
                        )
                    {
                        collidesWithObstacle =
                            true;

                        break;
                    }
                }
            }

            if (collidesWithObstacle)
            {
                continue;
            }

            outLandingPosition =
                candidate;

            return true;
        }
    }

    return false;
}

bool Game::IsProjectileBlockedByWorld(
    Vector2 previousPosition,
    Vector2 nextPosition,
    int projectileElevation
) const
{
    float travelDistance =
        Vector2Distance(
            previousPosition,
            nextPosition
        );

    // Sample the whole movement segment so fast
    // projectiles cannot skip through thin cliffs.
    int sampleCount =
        std::max(
            1,
            static_cast<int>(
                std::ceil(
                    travelDistance /
                    std::max(
                        8.0f,
                        TileSize * 0.20f
                    )
                )
                )
        );

    for (
        int sampleIndex = 1;
        sampleIndex <= sampleCount;
        ++sampleIndex
        )
    {
        float t =
            static_cast<float>(
                sampleIndex
                ) /
            static_cast<float>(
                sampleCount
                );

        Vector2 samplePosition =
            Vector2Lerp(
                previousPosition,
                nextPosition,
                t
            );

        int cellX = 0;
        int cellY = 0;

        if (
            !WorldToCell(
                samplePosition,
                cellX,
                cellY
            )
            )
        {
            return true;
        }

        // Entering another elevation means the
        // projectile hit a vertical cliff.
        if (
            GetTerrainElevation(
                cellX,
                cellY
            ) !=
            projectileElevation
            )
        {
            return true;
        }

        int tileType =
            tiles[
                CellIndex(
                    cellX,
                    cellY
                )
            ];

        if (!IsTileWalkable(tileType))
        {
            return true;
        }

        for (
            const Obstacle& obstacle :
            obstacles
            )
        {
            if (!obstacle.collisionEnabled)
            {
                continue;
            }

            // Upper decorative pieces do not block
            // ground-plane projectiles.
            if (
                obstacle.heightLevel > 0 ||
                GetTerrainElevationAtWorld(
                    obstacle.position
                ) !=
                projectileElevation
                )
            {
                continue;
            }

            if (
                obstacle.collisionShape ==
                CollisionShape::Box
                )
            {
                Rectangle collider =
                    GetObstacleCollisionRect(
                        obstacle
                    );

                if (
                    CheckCollisionPointRec(
                        samplePosition,
                        collider
                    )
                    )
                {
                    return true;
                }
            }
            else if (
                Vector2Distance(
                    samplePosition,
                    obstacle.position
                ) <=
                obstacle.colliderRadius
                )
            {
                return true;
            }
        }
    }

    return false;
}
void Game::ExecuteBossDash(
    Enemy& enemy
)
{
    if (
        enemy.type !=
        EnemyType::Boss
        )
    {
        return;
    }

    Vector2 away =
        Vector2Subtract(
            enemy.pos,
            playerPosition
        );

    if (
        Vector2Length(away) <=
        0.01f
        )
    {
        away = {
            1.0f,
            0.0f
        };
    }

    away =
        Vector2Normalize(
            away
        );

    const float dashPowerMultiplier =
        GetBossDashPowerMultiplier(
            enemy
        );

    const float dashDurationMultiplier =
        GetBossDashDurationMultiplier(
            enemy
        );

    enemy.bossDashIsForward =
        false;

    enemy.bossDashTimer =
        0.32f *
        dashDurationMultiplier;

    enemy.bossDashVelocity =
        Vector2Scale(
            away,
            780.0f *
            dashPowerMultiplier
        );

    SpawnHitSpark(
        enemy.pos
    );

    // Projectile speed also becomes stronger by phase.
    // Damage and projectile count remain unchanged.
    SpawnRadialEnemyProjectiles(
        enemy.pos,
        28,
        enemy.bulletDamage * 2,
        340.0f *
        dashPowerMultiplier,
        11.0f
    );
}

Vector2 Game::GetPlayerAnimationMoveDirection() const
{
    if (dashActive && Vector2Length(dashDirectionWorld) > 0.01f)
    {
        Vector2 viewDirection = WorldVectorToView(dashDirectionWorld);

        if (Vector2Length(viewDirection) > 0.01f)
        {
            return Vector2Normalize(viewDirection);
        }
    }

    // During Huashan jump, face the jump direction.
    if (huashanJumpActive)
    {
        Vector2 jumpDirection = Vector2Subtract(huashanJumpEnd, huashanJumpStart);

        if (Vector2Length(jumpDirection) > 0.01f)
        {
            Vector2 viewDirection = WorldVectorToView(jumpDirection);
            return Vector2Normalize(viewDirection);
        }
    }

    // Joystick movement.
    if (joystickActive && Vector2Length(joystickDirection) > 0.05f)
    {
        return joystickDirection;
    }

    // Tap-to-move path movement.
    if (hasPath && pathIndex < static_cast<int>(currentPath.size()))
    {
        Vector2 pathDirection = Vector2Subtract(currentPath[pathIndex], playerPosition);

        if (Vector2Length(pathDirection) > 0.05f)
        {
            Vector2 viewDirection = WorldVectorToView(pathDirection);
            return Vector2Normalize(viewDirection);
        }
    }

    return { 0.0f, 0.0f };
}

PlayerDirection Game::GetPlayerDirectionFromVector(Vector2 direction) const
{
    if (Vector2Length(direction) <= 0.05f)
    {
        return lastPlayerDirection;
    }

    float angle = atan2f(direction.y, direction.x);

    // Convert angle to 0-360 degrees.
    float degrees = angle * RAD2DEG;

    if (degrees < 0.0f)
    {
        degrees += 360.0f;
    }

    // Screen/world Y goes downward in this game.
    // 0 deg = right, 90 deg = down, 180 deg = left, 270 deg = up.

    if (degrees >= 337.5f || degrees < 22.5f)
    {
        return PlayerDirection::Right;
    }
    else if (degrees < 67.5f)
    {
        return PlayerDirection::DownRight;
    }
    else if (degrees < 112.5f)
    {
        return PlayerDirection::Down;
    }
    else if (degrees < 157.5f)
    {
        return PlayerDirection::DownLeft;
    }
    else if (degrees < 202.5f)
    {
        return PlayerDirection::Left;
    }
    else if (degrees < 247.5f)
    {
        return PlayerDirection::UpLeft;
    }
    else if (degrees < 292.5f)
    {
        return PlayerDirection::Up;
    }
    else
    {
        return PlayerDirection::UpRight;
    }
}

int Game::GetPlayerDirectionRow(PlayerDirection direction) const
{
    switch (direction)
    {
    case PlayerDirection::Down:      return 0;
    case PlayerDirection::DownRight: return 1;
    case PlayerDirection::Right:     return 2;
    case PlayerDirection::UpRight:   return 3;
    case PlayerDirection::Up:        return 4;
    case PlayerDirection::UpLeft:    return 5;
    case PlayerDirection::Left:      return 6;
    case PlayerDirection::DownLeft:  return 7;
    default:                         return 0;
    }
}

void Game::UpdatePlayerAnimation(float dt)
{
    // Attack animation has priority over walking and idle.
    if (
        playerAnimationState ==
        PlayerAnimationState::Attacking
        )
    {
        playerAnimTimer += dt;

        // Use while instead of if so animation timing remains
        // correct during occasional frame-rate drops.
        while (
            playerAnimTimer >=
            playerAttackFrameDuration &&
            playerAnimationState ==
            PlayerAnimationState::Attacking
            )
        {
            playerAnimTimer -=
                playerAttackFrameDuration;

            playerAnimFrame++;

            // Trigger melee damage when the attack animation
            // reaches its configured impact frame.
            if (
                !playerAttackImpactTriggered &&
                playerAnimFrame >=
                playerAttackImpactFrame
                )
            {
                playerAttackImpactTriggered = true;
                playerAttackImpactPending = true;
            }

            // Return to idle after the final attack frame.
            if (
                playerAnimFrame >=
                playerAttackFramesPerRow
                )
            {
                playerAnimationState =
                    PlayerAnimationState::Idle;

                playerDirection =
                    lastPlayerDirection;

                playerAnimFrame = 0;
                playerAnimTimer = 0.0f;

                break;
            }
        }

        return;
    }

    Vector2 moveDirection =
        GetPlayerAnimationMoveDirection();

    bool isMoving =
        Vector2Length(moveDirection) > 0.05f;

    if (isMoving)
    {
        // Reset only when entering the walking state.
        if (
            playerAnimationState !=
            PlayerAnimationState::Walking
            )
        {
            playerAnimFrame = 0;
            playerAnimTimer = 0.0f;
        }

        playerAnimationState =
            PlayerAnimationState::Walking;

        playerDirection =
            GetPlayerDirectionFromVector(
                moveDirection
            );

        lastPlayerDirection =
            playerDirection;

        playerAnimTimer += dt;

        while (
            playerAnimTimer >=
            playerAnimFrameDuration
            )
        {
            playerAnimTimer -=
                playerAnimFrameDuration;

            playerAnimFrame++;

            if (
                playerAnimFrame >=
                playerFramesPerRow
                )
            {
                playerAnimFrame = 0;
            }
        }

        return;
    }

    // No movement and no attack means idle becomes
    // the default animation state.
    if (
        playerAnimationState !=
        PlayerAnimationState::Idle
        )
    {
        playerAnimationState =
            PlayerAnimationState::Idle;

        playerAnimFrame = 0;
        playerAnimTimer = 0.0f;
    }

    // Idle keeps the player's last facing direction.
    playerDirection =
        lastPlayerDirection;

    if (
        playerIdleSpriteLoaded &&
        playerIdleSpriteSheet.id != 0 &&
        playerIdleFramesPerRow > 1
        )
    {
        playerAnimTimer += dt;

        while (
            playerAnimTimer >=
            playerIdleFrameDuration
            )
        {
            playerAnimTimer -=
                playerIdleFrameDuration;

            playerAnimFrame++;

            if (
                playerAnimFrame >=
                playerIdleFramesPerRow
                )
            {
                playerAnimFrame = 0;
            }
        }
    }
    else
    {
        // Safe fallback when the idle sheet is unavailable.
        playerAnimFrame = 0;
        playerAnimTimer = 0.0f;
    }
}

void Game::DrawPlayerSprite(
    Vector2 drawPosition
)
{
    Texture2D activeTexture{};

    int activeFrameWidth =
        playerFrameWidth;

    int activeFrameHeight =
        playerFrameHeight;

    int activeFramesPerRow =
        playerFramesPerRow;

    int activeFrame =
        playerAnimFrame;

    bool isDrawingAttack =
        playerAnimationState ==
        PlayerAnimationState::Attacking &&
        playerAttackSpriteLoaded &&
        playerAttackSpriteSheet.id != 0;

    bool isDrawingIdle =
        playerAnimationState ==
        PlayerAnimationState::Idle &&
        playerIdleSpriteLoaded &&
        playerIdleSpriteSheet.id != 0;

    if (isDrawingAttack)
    {
        activeTexture =
            playerAttackSpriteSheet;

        activeFrameWidth =
            playerAttackFrameWidth;

        activeFrameHeight =
            playerAttackFrameHeight;

        activeFramesPerRow =
            playerAttackFramesPerRow;
    }
    else if (isDrawingIdle)
    {
        activeTexture =
            playerIdleSpriteSheet;

        activeFrameWidth =
            playerIdleFrameWidth;

        activeFrameHeight =
            playerIdleFrameHeight;

        activeFramesPerRow =
            playerIdleFramesPerRow;
    }
    else if (
        playerSpriteLoaded &&
        playerSpriteSheet.id != 0
        )
    {
        activeTexture =
            playerSpriteSheet;

        activeFrameWidth =
            playerFrameWidth;

        activeFrameHeight =
            playerFrameHeight;

        activeFramesPerRow =
            playerFramesPerRow;

        // Missing idle or attack sheets fall back safely
        // to frame 0 of the walking sheet.
        if (
            playerAnimationState ==
            PlayerAnimationState::Attacking ||
            playerAnimationState ==
            PlayerAnimationState::Idle
            )
        {
            activeFrame = 0;
        }
    }
    else
    {
        DrawCircleV(
            drawPosition,
            playerRadius + 4.0f,
            Color{ 36, 36, 44, 255 }
        );

        DrawCircleV(
            drawPosition,
            playerRadius,
            Color{ 78, 148, 255, 255 }
        );

        return;
    }

    activeFrame =
        std::max(
            0,
            std::min(
                activeFrame,
                activeFramesPerRow - 1
            )
        );

    int row =
        GetPlayerDirectionRow(
            playerDirection
        );

    Rectangle source{
        static_cast<float>(
            activeFrame *
            activeFrameWidth
        ),
        static_cast<float>(
            row *
            activeFrameHeight
        ),
        static_cast<float>(
            activeFrameWidth
        ),
        static_cast<float>(
            activeFrameHeight
        )
    };

    const float drawScale =
        playerSpriteDrawScale;

    Rectangle destination{
        drawPosition.x,
        drawPosition.y,
        static_cast<float>(
            activeFrameWidth
        ) * drawScale,
        static_cast<float>(
            activeFrameHeight
        ) * drawScale
    };

    // Walking, idle and attack sheets should use
    // the same character foot position.
    Vector2 origin{
        static_cast<float>(
            activeFrameWidth
        ) * drawScale * 0.5f,

        static_cast<float>(
            activeFrameHeight
        ) * drawScale * 0.84f
    };

    DrawTexturePro(
        activeTexture,
        source,
        destination,
        origin,
        0.0f,
        WHITE
    );
}

bool Game::IsWorldCircleVisible(Vector2 worldPos, float radius) const
{
    Vector2 viewPos =
        WorldToViewElevated(worldPos);

    if (rendererMode == WorldRendererMode::Hybrid3D)
    {
        return !(
            viewPos.x + radius < 0.0f ||
            viewPos.x - radius > static_cast<float>(GetScreenWidth()) ||
            viewPos.y + radius < 0.0f ||
            viewPos.y - radius > static_cast<float>(GetScreenHeight())
            );
    }

    Vector2 topLeft = GetScreenToWorld2D({ 0.0f, 0.0f }, camera);
    Vector2 bottomRight = GetScreenToWorld2D(
        { static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight()) },
        camera
    );

    return !(viewPos.x + radius < topLeft.x ||
        viewPos.x - radius > bottomRight.x ||
        viewPos.y + radius < topLeft.y ||
        viewPos.y - radius > bottomRight.y);
}
void Game::InvalidateGroundCache()
{
    groundCacheDirty = true;
    MarkHybridTerrainDirty();
}

void Game::BuildGroundCache()
{
    const size_t expectedCellCount =
        static_cast<size_t>(
            MapWidth *
            MapHeight
            );

    // --------------------------------------------------
    // Validate terrain data
    // --------------------------------------------------

    if (tiles.size() != expectedCellCount)
    {
        TraceLog(
            LOG_ERROR,
            "[GROUND CACHE] Invalid tile count: %d, expected %d",
            static_cast<int>(
                tiles.size()
                ),
            MapWidth * MapHeight
        );

        groundCacheReady = false;
        return;
    }

    if (terrainCells.size() != expectedCellCount)
    {
        terrainCells.assign(
            expectedCellCount,
            TerrainCell{}
        );
    }

    if (tileBrushes.empty())
    {
        TraceLog(
            LOG_ERROR,
            "[GROUND CACHE] No tile brushes available."
        );

        groundCacheReady = false;
        return;
    }

    // --------------------------------------------------
    // Map dimensions
    // --------------------------------------------------

    const float originX =
        -static_cast<float>(
            MapWidth
            ) *
        TileSize *
        0.5f;

    const float originY =
        -static_cast<float>(
            MapHeight
            ) *
        TileSize *
        0.5f;

    const float mapWorldWidth =
        static_cast<float>(
            MapWidth
            ) *
        TileSize;

    const float mapWorldHeight =
        static_cast<float>(
            MapHeight
            ) *
        TileSize;

    constexpr float cachePadding =
        16.0f;

    int textureWidth = 1;
    int textureHeight = 1;

    // --------------------------------------------------
    // Calculate cache bounds
    //
    // Raised terrain is no longer included here.
    // It is drawn later through DrawWorldDepthSorted().
    // --------------------------------------------------

    if (!useIsometricView)
    {
        groundCacheDrawPosition = {
            originX,
            originY
        };

        textureWidth =
            std::max(
                1,
                static_cast<int>(
                    std::ceil(
                        mapWorldWidth
                    )
                    )
            );

        textureHeight =
            std::max(
                1,
                static_cast<int>(
                    std::ceil(
                        mapWorldHeight
                    )
                    )
            );
    }
    else
    {
        Vector2 mapCorners[4] = {
            WorldToView({
                originX,
                originY
            }),

            WorldToView({
                originX +
                    mapWorldWidth,
                originY
            }),

            WorldToView({
                originX +
                    mapWorldWidth,
                originY +
                    mapWorldHeight
            }),

            WorldToView({
                originX,
                originY +
                    mapWorldHeight
            })
        };

        float minX =
            mapCorners[0].x;

        float maxX =
            mapCorners[0].x;

        float minY =
            mapCorners[0].y;

        float maxY =
            mapCorners[0].y;

        for (int i = 1; i < 4; ++i)
        {
            minX =
                std::min(
                    minX,
                    mapCorners[i].x
                );

            maxX =
                std::max(
                    maxX,
                    mapCorners[i].x
                );

            minY =
                std::min(
                    minY,
                    mapCorners[i].y
                );

            maxY =
                std::max(
                    maxY,
                    mapCorners[i].y
                );
        }

        groundCacheDrawPosition = {
            std::floor(
                minX -
                cachePadding
            ),

            std::floor(
                minY -
                cachePadding
            )
        };

        textureWidth =
            std::max(
                1,
                static_cast<int>(
                    std::ceil(
                        maxX +
                        cachePadding -
                        groundCacheDrawPosition.x
                    )
                    )
            );

        textureHeight =
            std::max(
                1,
                static_cast<int>(
                    std::ceil(
                        maxY +
                        cachePadding -
                        groundCacheDrawPosition.y
                    )
                    )
            );
    }

    // --------------------------------------------------
    // Create or resize render texture
    // --------------------------------------------------

    bool mustCreateTexture =
        !groundCacheReady ||
        groundCache.id == 0 ||
        groundCache.texture.id == 0 ||
        groundCache.texture.width !=
        textureWidth ||
        groundCache.texture.height !=
        textureHeight;

    if (mustCreateTexture)
    {
        if (groundCache.id != 0)
        {
            UnloadRenderTexture(
                groundCache
            );

            groundCache = {};
        }

        groundCache =
            LoadRenderTexture(
                textureWidth,
                textureHeight
            );

        if (
            groundCache.id == 0 ||
            groundCache.texture.id == 0
            )
        {
            TraceLog(
                LOG_ERROR,
                "[GROUND CACHE] Could not create %dx%d render texture.",
                textureWidth,
                textureHeight
            );

            groundCacheReady = false;
            return;
        }

        SetTextureFilter(
            groundCache.texture,
            TEXTURE_FILTER_POINT
        );

        groundCacheReady = true;
    }

    // --------------------------------------------------
    // Begin cache rendering
    // --------------------------------------------------

    BeginTextureMode(
        groundCache
    );

    ClearBackground(
        BLANK
    );

    auto GetValidTileIndex =
        [this](int cellX, int cellY)
        {
            if (
                !IsCellInside(
                    cellX,
                    cellY
                )
                )
            {
                return static_cast<int>(
                    TileType::Grass
                    );
            }

            int tileIndex =
                tiles[
                    CellIndex(
                        cellX,
                        cellY
                    )
                ];

            if (
                tileIndex < 0 ||
                tileIndex >=
                static_cast<int>(
                    tileBrushes.size()
                    )
                )
            {
                tileIndex =
                    static_cast<int>(
                        TileType::Grass
                        );
            }

            return tileIndex;
        };

    // --------------------------------------------------
    // Flat top-down debugging view
    // --------------------------------------------------

    if (!useIsometricView)
    {
        for (int y = 0; y < MapHeight; ++y)
        {
            for (int x = 0; x < MapWidth; ++x)
            {
                if (
                    !IsCellEnabled(x, y) ||
                    !IsChamberVisible(
                        terrainCells[
                            CellIndex(x, y)
                        ].chamberId
                    )
                    )
                {
                    continue;
                }

                int tileIndex =
                    GetValidTileIndex(
                        x,
                        y
                    );

                const TileBrush& brush =
                    tileBrushes[
                        tileIndex
                    ];

                Rectangle destination{
                    static_cast<float>(
                        x
                    ) *
                        TileSize,

                    static_cast<float>(
                        y
                    ) *
                        TileSize,

                    TileSize,
                    TileSize
                };

                Texture2D brushTexture =
                    GetTileBrushTexture(
                        tileIndex
                    );

                if (brushTexture.id != 0)
                {
                    DrawTexturePro(
                        brushTexture,
                        GetTileBrushSourceRect(
                            tileIndex
                        ),
                        destination,
                        {
                            0.0f,
                            0.0f
                        },
                        0.0f,
                        WHITE
                    );
                }
                else
                {
                    Color fallback =
                        brush.fallbackColor;

                    fallback.a = 255;

                    DrawRectangleRec(
                        destination,
                        fallback
                    );
                }
            }
        }
    }
    else
    {
        // --------------------------------------------------
        // Base isometric terrain only
        // --------------------------------------------------

        auto ToCache =
            [this](Vector2 viewPosition)
            {
                return Vector2Subtract(
                    viewPosition,
                    groundCacheDrawPosition
                );
            };

        auto GetBaseSurfacePoints =
            [&](
                int cellX,
                int cellY,
                Vector2& top,
                Vector2& right,
                Vector2& bottom,
                Vector2& left
                )
            {
                const float x0 =
                    originX +
                    static_cast<float>(
                        cellX
                        ) *
                    TileSize;

                const float y0 =
                    originY +
                    static_cast<float>(
                        cellY
                        ) *
                    TileSize;

                const float x1 =
                    x0 +
                    TileSize;

                const float y1 =
                    y0 +
                    TileSize;

                top =
                    ToCache(
                        WorldToView({
                            x0,
                            y0
                            })
                    );

                right =
                    ToCache(
                        WorldToView({
                            x1,
                            y0
                            })
                    );

                bottom =
                    ToCache(
                        WorldToView({
                            x1,
                            y1
                            })
                    );

                left =
                    ToCache(
                        WorldToView({
                            x0,
                            y1
                            })
                    );
            };

        auto DrawBaseTileSurface =
            [&](
                int cellX,
                int cellY
                )
            {
                Vector2 top{};
                Vector2 right{};
                Vector2 bottom{};
                Vector2 left{};

                GetBaseSurfacePoints(
                    cellX,
                    cellY,
                    top,
                    right,
                    bottom,
                    left
                );

                int tileIndex =
                    GetValidTileIndex(
                        cellX,
                        cellY
                    );

                const TileBrush& brush =
                    tileBrushes[
                        tileIndex
                    ];

                Texture2D brushTexture =
                    GetTileBrushTexture(
                        tileIndex
                    );

                if (brushTexture.id != 0)
                {
                    DrawTextureFrameOnQuad2D(
                        brushTexture,
                        GetTileBrushSourceRect(
                            tileIndex
                        ),
                        top,
                        right,
                        bottom,
                        left,
                        WHITE
                    );
                }
                else
                {
                    Color fallback =
                        brush.fallbackColor;

                    fallback.a = 255;

                    DrawTriangle(
                        top,
                        right,
                        bottom,
                        fallback
                    );

                    DrawTriangle(
                        top,
                        bottom,
                        left,
                        fallback
                    );
                }
            };

        // Draw back-to-front so adjacent base tiles overlap
        // consistently in the isometric projection.
        for (
            int depth = 0;
            depth <=
            MapWidth +
            MapHeight -
            2;
            ++depth
            )
        {
            for (int y = 0; y < MapHeight; ++y)
            {
                int x =
                    depth -
                    y;

                if (
                    !IsCellInside(
                        x,
                        y
                    ) ||
                    !IsCellEnabled(
                        x,
                        y
                    ) ||
                    !IsChamberVisible(
                        terrainCells[
                            CellIndex(
                                x,
                                y
                            )
                        ].chamberId
                    )
                    )
                {
                    continue;
                }

                DrawBaseTileSurface(
                    x,
                    y
                );
            }
        }
    }

    EndTextureMode();

    groundCacheDirty = false;
    groundCacheReady = true;

    TraceLog(
        LOG_INFO,
        "[GROUND CACHE] Built base terrain | "
        "fbo=%u texture=%u size=%dx%d "
        "draw=(%.1f, %.1f)",
        groundCache.id,
        groundCache.texture.id,
        groundCache.texture.width,
        groundCache.texture.height,
        groundCacheDrawPosition.x,
        groundCacheDrawPosition.y
    );
}

void Game::RecordPerformanceSpike()
{
    PerfSpikeRecord record;

    record.index = ++perfSpikeRecordCounter;

    record.frameMs = perfLastSpikeFrameMs;
    record.updateMs = perfLastSpikeUpdateMs;
    record.drawMs = perfLastSpikeDrawMs;
    record.combatMs = perfLastSpikeCombatMs;

    record.reason = perfLastSpikeReason;
    record.drawSection = perfLastSpikeDrawSection;
    record.combatDrawSection = perfLastSpikeCombatDrawSection;

    record.drawGroundMs = perfDrawGroundMs;
    record.drawCombatMs = perfDrawCombatWorldMs;
    record.drawPlayerMs = perfDrawPlayerMs;

    record.drawUiMs =
        perfDrawUiMs +
        perfDrawHudMs +
        perfDrawSkillUiMs +
        perfDrawControlsMs +
        perfDrawOverlayMs;

    record.cdProjectilesMs = perfDrawCombatProjectilesMs;
    record.cdEnemiesMs = perfDrawCombatEnemiesMs;
    record.cdBladesMs = perfDrawCombatBladesMs;
    record.cdDongfengTeleMs = perfDrawCombatDongfengTelegraphMs;
    record.cdDongfengWaveMs = perfDrawCombatDongfengWaveMs;
    record.cdVfxMs = perfDrawCombatVfxMs;

    record.enemies = static_cast<int>(enemies.size());
    record.projectiles = static_cast<int>(projectiles.size());
    record.vfx = static_cast<int>(vfxParticles.size());

    perfSpikeRecords.push_back(record);

    while (static_cast<int>(perfSpikeRecords.size()) > perfMaxSpikeRecords)
    {
        perfSpikeRecords.pop_front();
    }

    TraceLog(
        LOG_WARNING,
        "PERF SPIKE #%d | reason=%s | frame=%.2f update=%.2f draw=%.2f combatUpdate=%.2f | "
        "beginMode=%.2f endMode=%.2f | "
        "drawSection=%s | ground=%.2f obstacles=%.2f npcs=%.2f combatWorld=%.2f player=%.2f worldOverlay=%.2f ui=%.2f overlay=%.2f | "
        "upgrade=%.2f dialogue=%.2f gameOver=%.2f unaccounted=%.2f | "
        "combatDraw=%s | cdProj=%.2f cdEnemies=%.2f cdBlades=%.2f cdDongfengTele=%.2f cdDongfengWave=%.2f cdVfx=%.2f | "
        "enemies=%d proj=%d vfx=%d",
        record.index,
        record.reason.c_str(),
        record.frameMs,
        record.updateMs,
        record.drawMs,
        record.combatMs,

        perfBeginMode2DMs,
        perfEndMode2DMs,

        record.drawSection.c_str(),
        perfDrawGroundMs,
        perfDrawObstaclesMs,
        perfDrawNpcsMs,
        perfDrawCombatWorldMs,
        perfDrawPlayerMs,
        perfDrawWorldOverlayMs,
        perfDrawUiMs + perfDrawHudMs + perfDrawSkillUiMs + perfDrawControlsMs,
        perfDrawOverlayMs,

        perfDrawUpgradeChoicesMs,
        perfDrawDialogueMs,
        perfDrawGameOverMs,
        perfDrawUnaccountedMs,

        record.combatDrawSection.c_str(),
        perfDrawCombatProjectilesMs,
        perfDrawCombatEnemiesMs,
        perfDrawCombatBladesMs,
        perfDrawCombatDongfengTelegraphMs,
        perfDrawCombatDongfengWaveMs,
        perfDrawCombatVfxMs,

        record.enemies,
        record.projectiles,
        record.vfx
    );
}

void Game::DumpPerformanceSpikes()
{
#if defined(__EMSCRIPTEN__)
    TraceLog(LOG_WARNING, "===== PERF SPIKE LOG BEGIN =====");

    for (const PerfSpikeRecord& r : perfSpikeRecords)
    {
        TraceLog(
            LOG_WARNING,
            "#%d,%s,frame=%.2f,update=%.2f,draw=%.2f,combat=%.2f,drawSection=%s,combatDraw=%s,ground=%.2f,combatDrawMs=%.2f,player=%.2f,ui=%.2f,cdProj=%.2f,cdEnemies=%.2f,cdBlades=%.2f,cdDongfengTele=%.2f,cdDongfengWave=%.2f,cdVfx=%.2f,enemies=%d,projectiles=%d,vfx=%d",
            r.index,
            r.reason.c_str(),
            r.frameMs,
            r.updateMs,
            r.drawMs,
            r.combatMs,
            r.drawSection.c_str(),
            r.combatDrawSection.c_str(),
            r.drawGroundMs,
            r.drawCombatMs,
            r.drawPlayerMs,
            r.drawUiMs,
            r.cdProjectilesMs,
            r.cdEnemiesMs,
            r.cdBladesMs,
            r.cdDongfengTeleMs,
            r.cdDongfengWaveMs,
            r.cdVfxMs,
            r.enemies,
            r.projectiles,
            r.vfx
        );
    }

    TraceLog(LOG_WARNING, "===== PERF SPIKE LOG END =====");
#else
    std::ofstream out("perf_spikes.csv");

    if (!out.is_open())
    {
        TraceLog(LOG_WARNING, "Could not write perf_spikes.csv");
        return;
    }

    out
        << "index,reason,frame_ms,update_ms,draw_ms,combat_ms,"
        << "draw_section,combat_draw_section,"
        << "draw_ground_ms,draw_combat_ms,draw_player_ms,draw_ui_ms,"
        << "cd_projectiles_ms,cd_enemies_ms,cd_blades_ms,"
        << "cd_dongfeng_tele_ms,cd_dongfeng_wave_ms,cd_vfx_ms,"
        << "enemies,projectiles,vfx\n";

    for (const PerfSpikeRecord& r : perfSpikeRecords)
    {
        out
            << r.index << ","
            << r.reason << ","
            << r.frameMs << ","
            << r.updateMs << ","
            << r.drawMs << ","
            << r.combatMs << ","
            << r.drawSection << ","
            << r.combatDrawSection << ","
            << r.drawGroundMs << ","
            << r.drawCombatMs << ","
            << r.drawPlayerMs << ","
            << r.drawUiMs << ","
            << r.cdProjectilesMs << ","
            << r.cdEnemiesMs << ","
            << r.cdBladesMs << ","
            << r.cdDongfengTeleMs << ","
            << r.cdDongfengWaveMs << ","
            << r.cdVfxMs << ","
            << r.enemies << ","
            << r.projectiles << ","
            << r.vfx
            << "\n";
    }

    out.close();

    TraceLog(
        LOG_WARNING,
        "Wrote %d perf spike records to perf_spikes.csv",
        static_cast<int>(perfSpikeRecords.size())
    );
#endif
}


void Game::SubmitFrameTiming(
    float fullFrameMs,
    float gameUpdateMs,
    float gameDrawMs,
    float endDrawingMs
)
{
    perfFullFrameMs = fullFrameMs;
    perfExternalUpdateMs = gameUpdateMs;
    perfExternalDrawMs = gameDrawMs;
    perfEndDrawingMs = endDrawingMs;

    // At 60 FPS, normal frame time is about 16.67 ms.
    // Only log actual bad frames, not normal EndDrawing/vsync waiting.
    bool frameSpike = fullFrameMs >= 45.0f || gameDrawMs >= 15.0f || gameUpdateMs >= 15.0f;

    if (!frameSpike)
    {
        return;
    }

    const char* likelyCause = "Frame pacing / vsync / browser";

    if (gameUpdateMs >= 8.0f && gameUpdateMs >= gameDrawMs)
    {
        likelyCause = "Game Update";
    }
    else if (gameDrawMs >= 8.0f && gameDrawMs >= gameUpdateMs)
    {
        likelyCause = "Game Draw";
    }
    else if (endDrawingMs >= 20.0f)
    {
        likelyCause = "EndDrawing / present wait";
    }

    TraceLog(
        LOG_WARNING,
        "FULL FRAME SPIKE | cause=%s | full=%.2f update=%.2f draw=%.2f endDrawing=%.2f | enemies=%d proj=%d vfx=%d",
        likelyCause,
        fullFrameMs,
        gameUpdateMs,
        gameDrawMs,
        endDrawingMs,
        static_cast<int>(enemies.size()),
        static_cast<int>(projectiles.size()),
        static_cast<int>(vfxParticles.size())
    );
}

bool Game::CanSpawnVfx(int count)
{
    if (static_cast<int>(vfxParticles.size()) + count > MaxVfxParticles)
    {
        return false;
    }

    if (vfxSpawnedThisFrame + count > maxVfxSpawnPerFrame)
    {
        return false;
    }

    vfxSpawnedThisFrame += count;
    return true;
}

void Game::LoadIsoGroundShader()
{
#if defined(PLATFORM_WEB) || defined(__EMSCRIPTEN__)

    const char* vertexPath =
        "Assets/shaders/iso_ground_100.vs";

    const char* fragmentPath =
        "Assets/shaders/iso_ground_100.fs";

#else

    const char* vertexPath =
        "Assets/shaders/iso_ground_330.vs";

    const char* fragmentPath =
        "Assets/shaders/iso_ground_330.fs";

#endif

    TraceLog(
        LOG_INFO,
        "[ISO SHADER] Loading vertex='%s' fragment='%s'",
        vertexPath,
        fragmentPath
    );

    if (!FileExists(vertexPath))
    {
        TraceLog(
            LOG_ERROR,
            "[ISO SHADER] Vertex file not found: %s",
            vertexPath
        );

        return;
    }

    if (!FileExists(fragmentPath))
    {
        TraceLog(
            LOG_ERROR,
            "[ISO SHADER] Fragment file not found: %s",
            fragmentPath
        );

        return;
    }

    isoGroundShader = LoadShader(
        vertexPath,
        fragmentPath
    );

    if (isoGroundShader.id == 0)
    {
        TraceLog(
            LOG_ERROR,
            "[ISO SHADER] LoadShader failed"
        );

        return;
    }

    isoVerticalScaleLocation =
        GetShaderLocation(
            isoGroundShader,
            "isoVerticalScale"
        );

    if (isoVerticalScaleLocation < 0)
    {
        TraceLog(
            LOG_ERROR,
            "[ISO SHADER] Uniform not found: isoVerticalScale"
        );

        UnloadShader(isoGroundShader);
        isoGroundShader = {};

        return;
    }

    isoGroundShaderLoaded = true;

    TraceLog(
        LOG_INFO,
        "[ISO SHADER] Loaded successfully | id=%u uniform=%d",
        isoGroundShader.id,
        isoVerticalScaleLocation
    );
}


void Game::LoadDashAfterimageShader()
{
#if defined(PLATFORM_WEB) || defined(__EMSCRIPTEN__)

    const char* fragmentPath =
        "Assets/shaders/dash_afterimage_100.fs";

#else

    const char* fragmentPath =
        "Assets/shaders/dash_afterimage_330.fs";

#endif

    if (!FileExists(fragmentPath))
    {
        TraceLog(
            LOG_WARNING,
            "[DASH SHADER] File not found: %s",
            fragmentPath
        );

        return;
    }

    // Use raylib's default sprite vertex shader.
    dashAfterimageShader =
        LoadShader(nullptr, fragmentPath);

    if (dashAfterimageShader.id == 0)
    {
        TraceLog(
            LOG_WARNING,
            "[DASH SHADER] LoadShader failed"
        );

        return;
    }

    dashAfterimageColorLocation =
        GetShaderLocation(
            dashAfterimageShader,
            "afterimageColor"
        );

    if (dashAfterimageColorLocation < 0)
    {
        TraceLog(
            LOG_WARNING,
            "[DASH SHADER] Uniform not found: afterimageColor"
        );

        UnloadShader(dashAfterimageShader);
        dashAfterimageShader = {};
        return;
    }

    dashAfterimageShaderLoaded = true;
}


float Game::GetWorldDepth(
    Vector2 worldPosition,
    float bias
) const
{
    // Works in both top-down and isometric modes.
    //
    // In isometric mode, WorldToView().y is based on:
    // worldX + worldY.
    return WorldToView(worldPosition).y + bias;
}


void Game::DrawNpcVisual(const NPC& npc)
{
    Vector2 viewPosition =
        WorldToViewElevated(npc.position);

    DrawCircleV(
        Vector2Add(
            viewPosition,
            { 5.0f, 8.0f }
        ),
        npc.radius + 6.0f,
        Color{ 0, 0, 0, 70 }
    );

    DrawCircleV(
        viewPosition,
        npc.radius + 6.0f,
        Color{ 28, 28, 34, 255 }
    );

    DrawCircleV(
        viewPosition,
        npc.radius,
        Color{ 218, 184, 92, 255 }
    );

    DrawText(
        npc.name.c_str(),
        static_cast<int>(
            viewPosition.x - 66.0f
            ),
        static_cast<int>(
            viewPosition.y - 62.0f
            ),
        18,
        WHITE
    );
}

void Game::DrawEnemyVisual(
    const Enemy& enemy
)
{
    if (
        !enemy.active ||
        enemy.spawnState ==
        EnemySpawnState::GroundEffect
        )
    {
        return;
    }

    const float enemySpawnVisualOffset =
        GetEnemySpawnVisualOffset(
            enemy
        );

    const float finalEnemyVisualHeight =
        enemy.visualHeight +
        enemySpawnVisualOffset;

    const float enemySpawnFlashAmount =
        GetEnemySpawnFlashAmount(
            enemy
        );

    // --------------------------------------------------
    // Choose the correct enemy rendering system
    // --------------------------------------------------

    const bool usesShooterEnemySprite =
        enemy.type ==
        EnemyType::Shooter &&
        EnemyHasDedicatedSprite(
            EnemyType::Shooter
        );

    const bool usesBossEnemySprite =
        enemy.type ==
        EnemyType::Boss &&
        EnemyHasDedicatedSprite(
            EnemyType::Boss
        );

    const bool usesBlobEnemySprite =
        !usesShooterEnemySprite &&
        !usesBossEnemySprite &&
        ShouldUseBlobEnemySprite(
            enemy
        );

    // --------------------------------------------------
    // Sprite dimensions
    // --------------------------------------------------

    const float shooterVisualWidth =
        static_cast<float>(
            shooterFrameWidth
            ) *
        shooterVisualScale;

    const float shooterVisualHeight =
        static_cast<float>(
            shooterFrameHeight
            ) *
        shooterVisualScale;

    const float bossVisualWidth =
        static_cast<float>(
            bossFrameWidth
            ) *
        bossVisualScale;

    const float bossVisualHeight =
        static_cast<float>(
            bossFrameHeight
            ) *
        bossVisualScale;

    float visualSize =
        enemy.radius *
        2.0f;

    if (usesShooterEnemySprite)
    {
        visualSize =
            std::max(
                shooterVisualWidth,
                shooterVisualHeight
            );
    }
    else if (usesBossEnemySprite)
    {
        visualSize =
            std::max(
                bossVisualWidth,
                bossVisualHeight
            );
    }
    else if (usesBlobEnemySprite)
    {
        visualSize =
            GetBlobEnemyVisualSize(
                enemy
            );
    }

    const float visibilityRadius =
        visualSize +
        80.0f +
        enemy.visualHeight;

    if (
        !IsWorldCircleVisible(
            enemy.pos,
            visibilityRadius
        )
        )
    {
        return;
    }


    // --------------------------------------------------
    // Colours and status effects
    // --------------------------------------------------

    Color fallbackColor =
        GREEN;

    Color spriteTint =
        WHITE;

    if (
        enemy.type ==
        EnemyType::Runner
        )
    {
        fallbackColor =
            LIME;
    }
    else if (
        enemy.type ==
        EnemyType::Tank
        )
    {
        fallbackColor =
            DARKGREEN;
    }
    else if (
        enemy.type ==
        EnemyType::Shooter
        )
    {
        fallbackColor =
            ORANGE;
    }
    else if (
        enemy.type ==
        EnemyType::Boss
        )
    {
        fallbackColor =
            GetBossTierTint(
                enemy
            );
    }

    if (
        enemy.type ==
        EnemyType::Boss
        )
    {
        spriteTint =
            GetBossTierTint(
                enemy
            );

        fallbackColor =
            spriteTint;
    }

    // --------------------------------------------------
// Charge warning blink
//
// The charge animation remains on frame zero.
// The Boss alternates between its current tier tint
// and bright white before charging.
// --------------------------------------------------

    if (
        enemy.type ==
        EnemyType::Boss &&
        enemy.bossActionState ==
        BossActionState::ChargeWindup
        )
    {
        const int blinkPhase =
            static_cast<int>(
                GetTime() *
                12.0
                );

        if (
            blinkPhase %
            2 ==
            0
            )
        {
            spriteTint =
                WHITE;

            fallbackColor =
                WHITE;
        }
    }


    if (enemy.frozenTimer > 0.0f)
    {
        fallbackColor = {
            150,
            230,
            255,
            255
        };

        spriteTint = {
            150,
            230,
            255,
            255
        };
    }
    else if (
        enemy.stunTimer > 0.0f ||
        enemy.landingStunTimer > 0.0f
        )
    {
        fallbackColor = {
            255,
            255,
            120,
            255
        };

        spriteTint = {
            255,
            255,
            150,
            255
        };
    }
    else if (enemy.slowTimer > 0.0f)
    {
        fallbackColor = {
            90,
            170,
            255,
            255
        };

        spriteTint = {
            150,
            205,
            255,
            255
        };
    }

    // Boss flashes while preparing its dash.
    if (
        enemy.type ==
        EnemyType::Boss &&
        enemy.bossDashCharging
        )
    {
        const float elapsed =
            enemy.bossDashChargeDuration -
            enemy.bossDashChargeTimer;

        const float phaseLength =
            enemy.bossDashChargeDuration /
            6.0f;

        const int phase =
            static_cast<int>(
                elapsed /
                std::max(
                    0.001f,
                    phaseLength
                )
                );

        const Color tierTint =
            GetBossTierTint(
                enemy
            );

        spriteTint =
            phase % 2 == 0
            ? tierTint
            : WHITE;

        fallbackColor =
            spriteTint;
    }


    // --------------------------------------------------
    // Screen positions
    // --------------------------------------------------

    const Vector2 groundPosition =
        WorldToViewElevated(
            enemy.pos
        );

    // Shooter and Boss use a feet/bottom anchor.
    const Vector2 shooterDrawPosition{
        groundPosition.x,
        groundPosition.y -
            finalEnemyVisualHeight
    };

    const Vector2 bossDrawPosition{
        groundPosition.x,
        groundPosition.y -
            finalEnemyVisualHeight
    };

    // Blob and circle enemies use a centre anchor.
    float centreVisualLift =
        enemy.radius;

    if (usesBlobEnemySprite)
    {
        centreVisualLift =
            visualSize *
            0.42f;
    }

    const Vector2 centredDrawPosition{
        groundPosition.x,

        groundPosition.y -
            finalEnemyVisualHeight -
            centreVisualLift
    };

    float shadowScale =
        1.0f -
        enemy.visualHeight /
        130.0f;

    shadowScale =
        std::max(
            0.38f,
            shadowScale
        );

    // --------------------------------------------------
    // Ground shadow
    // --------------------------------------------------

    if (usesShooterEnemySprite)
    {
        DrawEllipse(
            static_cast<int>(
                groundPosition.x +
                2.0f
                ),

            static_cast<int>(
                groundPosition.y +
                6.0f
                ),

            shooterVisualWidth *
            0.18f *
            shadowScale,

            shooterVisualHeight *
            0.045f *
            shadowScale,

            Color{
                0,
                0,
                0,
                80
            }
        );
    }
    else if (usesBossEnemySprite)
    {
        DrawEllipse(
            static_cast<int>(
                groundPosition.x +
                3.0f
                ),

            static_cast<int>(
                groundPosition.y +
                8.0f
                ),

            bossVisualWidth *
            0.22f *
            shadowScale,

            bossVisualHeight *
            0.055f *
            shadowScale,

            Color{
                0,
                0,
                0,
                85
            }
        );
    }
    else if (usesBlobEnemySprite)
    {
        const float shadowWidth =
            visualSize *
            0.24f *
            shadowScale;

        const float shadowHeight =
            visualSize *
            0.09f *
            shadowScale;

        const float shadowOffsetX =
            1.5f;

        const float shadowOffsetY =
            visualSize *
            0.17f;

        DrawEllipse(
            static_cast<int>(
                groundPosition.x +
                shadowOffsetX
                ),

            static_cast<int>(
                groundPosition.y +
                shadowOffsetY
                ),

            shadowWidth,
            shadowHeight,

            Color{
                0,
                0,
                0,
                70
            }
        );
    }
    else
    {
        DrawEllipse(
            static_cast<int>(
                groundPosition.x +
                5.0f
                ),

            static_cast<int>(
                groundPosition.y +
                9.0f
                ),

            enemy.radius *
            1.15f *
            shadowScale,

            enemy.radius *
            0.42f *
            shadowScale,

            Color{
                0,
                0,
                0,
                90
            }
        );
    }

    // --------------------------------------------------
    // Enemy sprite
    // --------------------------------------------------

    auto DrawCurrentEnemyBody =
        [&](
            Color bodyTint,
            Color circleTint
            )
        {
            if (usesShooterEnemySprite)
            {
                DrawShooterEnemySprite(
                    enemy,
                    shooterDrawPosition,
                    bodyTint
                );
            }
            else if (usesBossEnemySprite)
            {
                DrawBossEnemySprite(
                    enemy,
                    bossDrawPosition,
                    bodyTint
                );
            }
            else if (usesBlobEnemySprite)
            {
                DrawSmallEnemySprite(
                    enemy,
                    centredDrawPosition,
                    bodyTint
                );
            }
            else
            {
                DrawCircleV(
                    centredDrawPosition,
                    enemy.radius,
                    circleTint
                );
            }
        };

    // Draw the enemy using its normal colours.
    DrawCurrentEnemyBody(
        spriteTint,
        fallbackColor
    );

    // Draw a second bright layer while the enemy emerges.
    if (
        enemySpawnFlashAmount >
        0.001f
        )
    {
        const unsigned char glowAlpha =
            static_cast<unsigned char>(
                255.0f *
                enemySpawnFlashAmount
                );

        const Color emergenceGlow{
            255,
            245,
            190,
            glowAlpha
        };

        BeginBlendMode(
            BLEND_ADDITIVE
        );

        DrawCurrentEnemyBody(
            emergenceGlow,
            emergenceGlow
        );

        EndBlendMode();
    }




    // --------------------------------------------------
    // Health bar
    // --------------------------------------------------

    if (
        !IsEnemySpawnProtected(
            enemy
        ) &&
        (
            enemy.hp <
            enemy.maxHp ||
            enemy.type ==
            EnemyType::Boss
            )
        )
    {
        const float hpRatio =
            Clamp(
                static_cast<float>(
                    enemy.hp
                    ) /
                static_cast<float>(
                    std::max(
                        1,
                        enemy.maxHp
                    )
                    ),
                0.0f,
                1.0f
            );

        float hpBarWidth =
            enemy.radius *
            2.0f;

        float visualTop =
            centredDrawPosition.y -
            enemy.radius;

        if (usesShooterEnemySprite)
        {
            hpBarWidth =
                shooterVisualWidth *
                0.68f;

            visualTop =
                shooterDrawPosition.y -
                shooterVisualHeight *
                0.98f;
        }
        else if (usesBossEnemySprite)
        {
            hpBarWidth =
                bossVisualWidth *
                0.72f;

            visualTop =
                bossDrawPosition.y -
                bossVisualHeight *
                0.98f;
        }
        else if (usesBlobEnemySprite)
        {
            hpBarWidth =
                visualSize *
                0.70f;

            visualTop =
                centredDrawPosition.y -
                visualSize *
                0.5f;
        }

        Rectangle backBar{
            groundPosition.x -
                hpBarWidth *
                0.5f,

            visualTop -
                14.0f,

            hpBarWidth,
            6.0f
        };

        Rectangle hpBar =
            backBar;

        hpBar.width *=
            hpRatio;

        DrawRectangleRec(
            backBar,
            DARKGRAY
        );

        DrawRectangleRec(
            hpBar,
            RED
        );
    }
}

void Game::DrawPlayerVisual()
{
    float jumpHeight =
        GetPlayerVisualHeight();

    Vector2 groundPosition =
        WorldToViewElevated(playerPosition);

    Vector2 drawPosition{
        groundPosition.x,
        groundPosition.y -
            jumpHeight
    };

    float shadowScale =
        1.0f -
        jumpHeight /
        140.0f;

    if (shadowScale < 0.45f)
    {
        shadowScale = 0.45f;
    }

    DrawEllipse(
        static_cast<int>(
            groundPosition.x + 5.0f
            ),
        static_cast<int>(
            groundPosition.y + 10.0f
            ),
        playerRadius *
        1.05f *
        shadowScale,
        playerRadius *
        0.45f *
        shadowScale,
        Color{ 0, 0, 0, 95 }
    );

    DrawPlayerSprite(drawPosition);

    if (huashanJumpActive)
    {
        DrawCircleLines(
            static_cast<int>(
                drawPosition.x
                ),
            static_cast<int>(
                drawPosition.y
                ),
            playerRadius + 8.0f,
            Color{
                255,
                80,
                55,
                220
            }
        );
    }
}

void Game::DrawProjectileVisual(
    const Projectile& projectile
)
{
    if (!projectile.active)
    {
        return;
    }

    // Do not use WorldToViewElevated() here because
    // it samples whatever terrain is beneath the projectile.
    // The projectile must stay on its original terrain plane.
    Vector2 viewPosition =
        WorldToView(
            projectile.pos
        );

    viewPosition.y -=
        projectile.visualHeight;

    if (useIsometricView)
    {
        viewPosition.y -=
            static_cast<float>(
                projectile.terrainElevation
                ) *
            terrainElevationStep;
    }

    if (
        projectile.owner ==
        ProjectileOwner::Enemy
        )
    {
        DrawCircleV(
            viewPosition,
            projectile.radius + 5.0f,
            Color{
                255,
                80,
                80,
                100
            }
        );

        DrawCircleV(
            viewPosition,
            projectile.radius,
            Color{
                255,
                80,
                60,
                255
            }
        );
    }
    else
    {
        DrawCircleV(
            viewPosition,
            projectile.radius + 5.0f,
            Color{
                255,
                220,
                80,
                100
            }
        );

        DrawCircleV(
            viewPosition,
            projectile.radius,
            YELLOW
        );
    }
}


void Game::DrawOrbitalBladeVisual(
    const OrbitalBlade& blade
)
{
    if (!blade.active)
    {
        return;
    }

    Vector2 bladeWorldPosition{
        playerPosition.x +
            cosf(blade.angle) *
            blade.orbitRadius,

        playerPosition.y +
            sinf(blade.angle) *
            blade.orbitRadius
    };

    Vector2 bladeViewPosition =
        WorldToViewElevated(
            bladeWorldPosition
        );

    DrawCircleV(
        bladeViewPosition,
        blade.bladeRadius + 8.0f,
        { 120, 210, 255, 80 }
    );

    DrawCircleV(
        bladeViewPosition,
        blade.bladeRadius,
        { 180, 235, 255, 230 }
    );

    DrawCircleLines(
        static_cast<int>(
            bladeViewPosition.x
            ),
        static_cast<int>(
            bladeViewPosition.y
            ),
        blade.bladeRadius,
        WHITE
    );
}

bool Game::IsGroundVfx(
    VfxType type
) const
{
    return
        type == VfxType::SkillCircle ||
        type == VfxType::Explosion;
}

bool Game::IsForegroundVfx(
    VfxType type
) const
{
    return
        type ==
        VfxType::FloatingDamage;
}

void Game::DrawVfxParticleVisual(
    const VfxParticle& particle
)
{
    if (!particle.active)
    {
        return;
    }

    float t = 1.0f;

    if (particle.maxLife > 0.0f)
    {
        t =
            particle.life /
            particle.maxLife;
    }

    t = Clamp(
        t,
        0.0f,
        1.0f
    );

    Color color =
        particle.color;

    color.a =
        static_cast<unsigned char>(
            static_cast<float>(
                particle.color.a
                ) * t
            );

    Vector2 viewPosition =
        WorldToViewElevated(particle.pos);

    if (
        particle.type ==
        VfxType::HitSpark ||
        particle.type ==
        VfxType::DeathBurst
        )
    {
        DrawCircleV(
            viewPosition,
            particle.radius * t,
            color
        );
    }
    else if (
        particle.type ==
        VfxType::FloatingDamage
        )
    {
        DrawText(
            TextFormat(
                "%d",
                particle.value
            ),
            static_cast<int>(
                viewPosition.x
                ),
            static_cast<int>(
                viewPosition.y
                ),
            18,
            color
        );
    }
    else if (
        particle.type ==
        VfxType::LightningLine
        )
    {
        DrawLineEx(
            viewPosition,
            WorldToViewElevated(
                particle.endPos
            ),
            std::max(
                2.0f,
                particle.radius * t
            ),
            color
        );
    }
    else if (
        particle.type ==
        VfxType::SlashLine
        )
    {
        DrawLineEx(
            viewPosition,
            WorldToViewElevated(
                particle.endPos
            ),
            std::max(
                3.0f,
                particle.radius *
                0.35f *
                t
            ),
            color
        );
    }
    else if (
        particle.type ==
        VfxType::SkillCircle
        )
    {
        Color outline =
            particle.color;

        outline.a =
            static_cast<unsigned char>(
                180.0f * t
                );

        DrawGroundCircleLines(
            particle.pos,
            particle.radius,
            outline
        );

        DrawGroundCircleLines(
            particle.pos,
            particle.radius * 0.55f,
            Color{
                particle.color.r,
                particle.color.g,
                particle.color.b,
                static_cast<unsigned char>(
                    90.0f * t
                )
            }
        );
    }
    else if (
        particle.type ==
        VfxType::Explosion
        )
    {
        DrawGroundCircleLines(
            particle.pos,
            particle.radius * t,
            color
        );
    }
}

void Game::DrawWorldDepthSorted()
{
    worldDrawItems.clear();

    worldDrawItems.reserve(
        obstacles.size() +
        npcs.size() +
        enemies.size() +
        projectiles.size() +
        orbitalBlades.size() +
        dashAfterimages.size() +
        vfxParticles.size() +
        static_cast<size_t>(
            MapWidth *
            MapHeight *
            (
                maxTerrainElevation *
                2 +
                2
                )
            ) +
        2
    );

    auto AddWorldObject =
        [this](
            WorldDrawKind kind,
            int index,
            Vector2 worldPosition,
            int elevationBand,
            int tiePriority,
            int stableOrder
            )
        {
            if (
                kind != WorldDrawKind::Player &&
                !IsWorldPositionInVisibleChamber(
                    worldPosition
                )
                )
            {
                return;
            }

            WorldDrawItem item;

            item.kind =
                kind;

            item.index =
                index;

            item.elevationBand =
                std::max(
                    0,
                    elevationBand
                );

            // Sorting uses the ground footprint.
            // Visual elevation is handled separately by
            // WorldToViewElevated() inside drawing functions.
            item.depth =
                GetWorldDepth(
                    worldPosition
                );

            item.tiePriority =
                tiePriority;

            item.secondaryDepth =
                WorldToView(
                    worldPosition
                ).x;

            item.stableOrder =
                stableOrder;

            worldDrawItems.push_back(
                item
            );
        };

    // --------------------------------------------------
    // Raised terrain
    //
    // Elevation-band model:
    //
    // top at H1       -> band H0
    // actor on H1     -> band H1
    // wall H1 to H0   -> band H0
    // ramp H0 to H1   -> band H0
    //
    // This lets raised terrain obstruct lower actors,
    // while actors standing on top render afterward.
    // --------------------------------------------------

    if (useIsometricView)
    {
        auto GetTerrainBackDepth =
            [this](
                int cellX,
                int cellY
                )
            {
                Vector2 center =
                    CellToWorld(
                        cellX,
                        cellY
                    );

                Vector2 backCorner{
                    center.x -
                        TileSize * 0.5f,

                    center.y -
                        TileSize * 0.5f
                };

                return
                    GetWorldDepth(
                        backCorner
                    );
            };

        for (int y = 0; y < MapHeight; ++y)
        {
            for (int x = 0; x < MapWidth; ++x)
            {
                if (
                    !IsCellEnabled(x, y) ||
                    !IsChamberVisible(
                        terrainCells[
                            CellIndex(x, y)
                        ].chamberId
                    )
                    )
                {
                    continue;
                }

                const int cellIndex =
                    CellIndex(
                        x,
                        y
                    );

                const int elevation =
                    GetTerrainElevation(
                        x,
                        y
                    );

                const TerrainCell& cell =
                    terrainCells[
                        cellIndex
                    ];

                const Vector2 cellCenter =
                    CellToWorld(
                        x,
                        y
                    );

                // ------------------------------------------
                // Raised top
                //
                // Top H1 is sorted with H0 so it can cover
                // lower actors behind it. An actor standing
                // on H1 belongs to band H1 and draws later.
                // ------------------------------------------

                if (elevation > 0)
                {
                    WorldDrawItem item;

                    item.kind =
                        WorldDrawKind::TerrainTop;

                    item.cellX = x;
                    item.cellY = y;

                    item.elevationBand =
                        elevation - 1;

                    item.depth =
                        GetTerrainBackDepth(
                            x,
                            y
                        );

                    item.tiePriority = 4;

                    item.secondaryDepth =
                        WorldToView(
                            cellCenter
                        ).x;

                    item.stableOrder =
                        cellIndex *
                        (
                            maxTerrainElevation *
                            2 +
                            4
                            );

                    worldDrawItems.push_back(
                        item
                    );
                }

                // ------------------------------------------
                // Ramp
                // ------------------------------------------

                int rampOffsetX = 0;
                int rampOffsetY = 0;

                const bool hasRampDirection =
                    GetRampDirectionOffset(
                        cell.rampDirection,
                        rampOffsetX,
                        rampOffsetY
                    );

                if (hasRampDirection)
                {
                    const int targetX =
                        x +
                        rampOffsetX;

                    const int targetY =
                        y +
                        rampOffsetY;

                    const bool validRamp =
                        IsCellInside(
                            targetX,
                            targetY
                        ) &&
                        GetTerrainElevation(
                            targetX,
                            targetY
                        ) ==
                        elevation + 1;

                    if (validRamp)
                    {
                        WorldDrawItem item;

                        item.kind =
                            WorldDrawKind::TerrainRamp;

                        item.cellX = x;
                        item.cellY = y;

                        item.elevationBand =
                            elevation;

                        item.depth =
                            GetTerrainBackDepth(
                                x,
                                y
                            );

                        item.tiePriority = 14;

                        item.secondaryDepth =
                            WorldToView(
                                cellCenter
                            ).x;

                        item.stableOrder =
                            cellIndex *
                            (
                                maxTerrainElevation *
                                2 +
                                4
                                ) +
                            1;

                        worldDrawItems.push_back(
                            item
                        );
                    }
                }

                if (elevation <= 0)
                {
                    continue;
                }

                // ------------------------------------------
 // All four terrain wall directions
 // ------------------------------------------

                const int faceStride =
                    maxTerrainElevation +
                    1;

                const int terrainStableStride =
                    faceStride *
                    4 +
                    8;

                auto AddTerrainWallSegments =
                    [&](
                        TerrainCliffFace face,
                        WorldDrawKind drawKind,
                        int neighbourX,
                        int neighbourY,
                        Vector2 edgePosition,
                        int tiePriority,
                        int faceOrder
                        )
                    {
                        const int neighbourElevation =
                            GetTerrainElevation(
                                neighbourX,
                                neighbourY
                            );

                        if (
                            IsRampConnectionBetweenCells(
                                x,
                                y,
                                neighbourX,
                                neighbourY
                            )
                            )
                        {
                            return;
                        }

                        for (
                            int segmentLevel =
                            neighbourElevation;
                            segmentLevel <
                            elevation;
                            ++segmentLevel
                            )
                        {
                            WorldDrawItem item;

                            item.kind =
                                drawKind;

                            item.cellX =
                                x;

                            item.cellY =
                                y;

                            item.segmentLevel =
                                segmentLevel;

                            item.elevationBand =
                                segmentLevel;

                            item.depth =
                                GetWorldDepth(
                                    edgePosition
                                );

                            item.tiePriority =
                                tiePriority;

                            item.secondaryDepth =
                                WorldToView(
                                    edgePosition
                                ).x;

                            item.stableOrder =
                                cellIndex *
                                terrainStableStride +
                                faceOrder *
                                faceStride +
                                segmentLevel;

                            worldDrawItems.push_back(
                                item
                            );
                        }
                    };

                // North and west are the rear-facing walls.
                // Draw them before the raised top surface.
                Vector2 northEdge =
                    cellCenter;

                northEdge.y -=
                    TileSize *
                    0.5f;

                AddTerrainWallSegments(
                    TerrainCliffFace::North,
                    WorldDrawKind::TerrainNorthCliff,
                    x,
                    y - 1,
                    northEdge,
                    2,
                    0
                );

                Vector2 westEdge =
                    cellCenter;

                westEdge.x -=
                    TileSize *
                    0.5f;

                AddTerrainWallSegments(
                    TerrainCliffFace::West,
                    WorldDrawKind::TerrainWestCliff,
                    x - 1,
                    y,
                    westEdge,
                    3,
                    1
                );

                // East and south are the foreground-facing walls.
                // Draw them after the raised top.
                Vector2 eastEdge =
                    cellCenter;

                eastEdge.x +=
                    TileSize *
                    0.5f;

                AddTerrainWallSegments(
                    TerrainCliffFace::East,
                    WorldDrawKind::TerrainEastCliff,
                    x + 1,
                    y,
                    eastEdge,
                    12,
                    2
                );

                Vector2 southEdge =
                    cellCenter;

                southEdge.y +=
                    TileSize *
                    0.5f;

                AddTerrainWallSegments(
                    TerrainCliffFace::South,
                    WorldDrawKind::TerrainSouthCliff,
                    x,
                    y + 1,
                    southEdge,
                    13,
                    3
                );
            }
        }
    }

    // --------------------------------------------------
    // Obstacles
    // --------------------------------------------------

    for (
        int i = 0;
        i <
        static_cast<int>(
            obstacles.size()
            );
        ++i
        )
    {
        const Obstacle& obstacle =
            obstacles[i];

        const int terrainElevation =
            GetTerrainElevationAtWorld(
                obstacle.position
            );

        AddWorldObject(
            WorldDrawKind::Obstacle,
            i,
            obstacle.position,

            terrainElevation +
            std::max(
                0,
                obstacle.heightLevel
            ),

            30 +
            std::max(
                0,
                obstacle.heightLevel
            ),

            100000 +
            i
        );
    }

    // --------------------------------------------------
    // NPCs
    // --------------------------------------------------

    for (
        int i = 0;
        i <
        static_cast<int>(
            npcs.size()
            );
        ++i
        )
    {
        const NPC& npc =
            npcs[i];

        AddWorldObject(
            WorldDrawKind::NPC,
            i,
            npc.position,
            GetTerrainElevationAtWorld(
                npc.position
            ),
            20,
            200000 +
            i
        );
    }

    // --------------------------------------------------
    // Enemies
    // --------------------------------------------------

    for (
        int i = 0;
        i <
        static_cast<int>(
            enemies.size()
            );
        ++i
        )
    {
        const Enemy& enemy =
            enemies[i];

        if (!enemy.active)
        {
            continue;
        }

        AddWorldObject(
            WorldDrawKind::Enemy,
            i,
            enemy.pos,
            GetTerrainElevationAtWorld(
                enemy.pos
            ),
            20,
            300000 +
            enemy.id
        );
    }

    // --------------------------------------------------
    // Dash afterimages
    // --------------------------------------------------

    for (
        int i = 0;
        i <
        static_cast<int>(
            dashAfterimages.size()
            );
        ++i
        )
    {
        const DashAfterimage& afterimage =
            dashAfterimages[i];

        AddWorldObject(
            WorldDrawKind::DashAfterimage,
            i,
            afterimage.worldPosition,
            GetTerrainElevationAtWorld(
                afterimage.worldPosition
            ),
            18,
            400000 +
            i
        );
    }

    // --------------------------------------------------
    // Player
    // --------------------------------------------------

    AddWorldObject(
        WorldDrawKind::Player,
        -1,
        playerPosition,
        GetTerrainElevationAtWorld(
            playerPosition
        ),
        20,
        500000
    );

    // --------------------------------------------------
    // Projectiles
    // --------------------------------------------------

    for (
        int i = 0;
        i <
        static_cast<int>(
            projectiles.size()
            );
        ++i
        )
    {
        const Projectile& projectile =
            projectiles[i];

        if (!projectile.active)
        {
            continue;
        }

        AddWorldObject(
            WorldDrawKind::Projectile,
            i,
            projectile.pos,
            projectile.terrainElevation,
            25,
            600000 +
            i
        );
    }

    // --------------------------------------------------
    // Orbiting blades
    // --------------------------------------------------

    const int playerElevation =
        GetTerrainElevationAtWorld(
            playerPosition
        );

    for (
        int i = 0;
        i <
        static_cast<int>(
            orbitalBlades.size()
            );
        ++i
        )
    {
        const OrbitalBlade& blade =
            orbitalBlades[i];

        if (!blade.active)
        {
            continue;
        }

        Vector2 bladeWorldPosition{
            playerPosition.x +
                cosf(
                    blade.angle
                ) *
                blade.orbitRadius,

            playerPosition.y +
                sinf(
                    blade.angle
                ) *
                blade.orbitRadius
        };

        AddWorldObject(
            WorldDrawKind::OrbitalBlade,
            i,
            bladeWorldPosition,
            playerElevation,
            25,
            700000 +
            i
        );
    }

    // --------------------------------------------------
    // Dongfeng wave
    // --------------------------------------------------

    if (dongfengWaveActive)
    {
        AddWorldObject(
            WorldDrawKind::DongfengWave,
            -1,
            dongfengWavePos,
            GetTerrainElevationAtWorld(
                dongfengWavePos
            ),
            25,
            800000
        );
    }

    // --------------------------------------------------
    // Depth-sorted VFX
    // --------------------------------------------------

    for (
        int i = 0;
        i <
        static_cast<int>(
            vfxParticles.size()
            );
        ++i
        )
    {
        const VfxParticle& particle =
            vfxParticles[i];

        if (!particle.active)
        {
            continue;
        }

        if (
            IsGroundVfx(
                particle.type
            ) ||
            IsForegroundVfx(
                particle.type
            )
            )
        {
            continue;
        }

        Vector2 depthPosition =
            particle.pos;

        if (
            particle.type ==
            VfxType::LightningLine ||
            particle.type ==
            VfxType::SlashLine
            )
        {
            depthPosition = {
                (
                    particle.pos.x +
                    particle.endPos.x
                ) *
                    0.5f,

                (
                    particle.pos.y +
                    particle.endPos.y
                ) *
                    0.5f
            };
        }

        AddWorldObject(
            WorldDrawKind::Vfx,
            i,
            depthPosition,
            GetTerrainElevationAtWorld(
                depthPosition
            ),
            26,
            900000 +
            i
        );
    }

    // --------------------------------------------------
    // Stable elevation-aware painter sorting
    //
    // Elevation band is the primary grouping key.
    // All items belonging to one elevation are therefore
    // rendered together before moving to the next level.
    // --------------------------------------------------

    std::stable_sort(
        worldDrawItems.begin(),
        worldDrawItems.end(),
        [](
            const WorldDrawItem& a,
            const WorldDrawItem& b
            )
        {
            if (
                a.elevationBand !=
                b.elevationBand
                )
            {
                return
                    a.elevationBand <
                    b.elevationBand;
            }

            constexpr double precision =
                16.0;

            const long long depthA =
                static_cast<long long>(
                    std::llround(
                        static_cast<double>(
                            a.depth
                            ) *
                        precision
                    )
                    );

            const long long depthB =
                static_cast<long long>(
                    std::llround(
                        static_cast<double>(
                            b.depth
                            ) *
                        precision
                    )
                    );

            if (depthA != depthB)
            {
                return
                    depthA <
                    depthB;
            }

            if (
                a.tiePriority !=
                b.tiePriority
                )
            {
                return
                    a.tiePriority <
                    b.tiePriority;
            }

            const long long secondaryA =
                static_cast<long long>(
                    std::llround(
                        static_cast<double>(
                            a.secondaryDepth
                            ) *
                        precision
                    )
                    );

            const long long secondaryB =
                static_cast<long long>(
                    std::llround(
                        static_cast<double>(
                            b.secondaryDepth
                            ) *
                        precision
                    )
                    );

            if (
                secondaryA !=
                secondaryB
                )
            {
                return
                    secondaryA <
                    secondaryB;
            }

            if (
                a.stableOrder !=
                b.stableOrder
                )
            {
                return
                    a.stableOrder <
                    b.stableOrder;
            }

            return
                static_cast<int>(
                    a.kind
                    ) <
                static_cast<int>(
                    b.kind
                    );
        }
    );


    // --------------------------------------------------
    // Draw sorted items
    // --------------------------------------------------

    for (
        const WorldDrawItem& item :
        worldDrawItems
        )
    {
        switch (item.kind)
        {
        case WorldDrawKind::TerrainTop:
        {
            DrawTerrainTopSurface(
                item.cellX,
                item.cellY
            );

            break;
        }

        case WorldDrawKind::TerrainRamp:
        {
            DrawTerrainRampSurface(
                item.cellX,
                item.cellY
            );

            break;
        }
        case WorldDrawKind::TerrainNorthCliff:
        {
            DrawTerrainCliffSegment(
                item.cellX,
                item.cellY,
                TerrainCliffFace::North,
                item.segmentLevel
            );

            break;
        }

        case WorldDrawKind::TerrainWestCliff:
        {
            DrawTerrainCliffSegment(
                item.cellX,
                item.cellY,
                TerrainCliffFace::West,
                item.segmentLevel
            );

            break;
        }

        case WorldDrawKind::TerrainEastCliff:
        {
            DrawTerrainCliffSegment(
                item.cellX,
                item.cellY,
                TerrainCliffFace::East,
                item.segmentLevel
            );

            break;
        }

        case WorldDrawKind::TerrainSouthCliff:
        {
            DrawTerrainCliffSegment(
                item.cellX,
                item.cellY,
                TerrainCliffFace::South,
                item.segmentLevel
            );

            break;
        }

        case WorldDrawKind::Obstacle:
        {
            if (
                item.index >= 0 &&
                item.index <
                static_cast<int>(
                    obstacles.size()
                    )
                )
            {
                DrawObstacleVisual(
                    obstacles[
                        item.index
                    ]
                );
            }

            break;
        }

        case WorldDrawKind::NPC:
        {
            if (
                item.index >= 0 &&
                item.index <
                static_cast<int>(
                    npcs.size()
                    )
                )
            {
                DrawNpcVisual(
                    npcs[
                        item.index
                    ]
                );
            }

            break;
        }

        case WorldDrawKind::Enemy:
        {
            if (
                item.index >= 0 &&
                item.index <
                static_cast<int>(
                    enemies.size()
                    )
                )
            {
                DrawEnemyVisual(
                    enemies[
                        item.index
                    ]
                );
            }

            break;
        }

        case WorldDrawKind::DashAfterimage:
        {
            if (
                item.index >= 0 &&
                item.index <
                static_cast<int>(
                    dashAfterimages.size()
                    )
                )
            {
                DrawDashAfterimage(
                    dashAfterimages[
                        item.index
                    ]
                );
            }

            break;
        }

        case WorldDrawKind::Player:
        {
            DrawPlayerVisual();
            break;
        }

        case WorldDrawKind::Projectile:
        {
            if (
                item.index >= 0 &&
                item.index <
                static_cast<int>(
                    projectiles.size()
                    )
                )
            {
                DrawProjectileVisual(
                    projectiles[
                        item.index
                    ]
                );
            }

            break;
        }

        case WorldDrawKind::OrbitalBlade:
        {
            if (
                item.index >= 0 &&
                item.index <
                static_cast<int>(
                    orbitalBlades.size()
                    )
                )
            {
                DrawOrbitalBladeVisual(
                    orbitalBlades[
                        item.index
                    ]
                );
            }

            break;
        }

        case WorldDrawKind::DongfengWave:
        {
            DrawDongfengWave();
            break;
        }

        case WorldDrawKind::Vfx:
        {
            if (
                item.index >= 0 &&
                item.index <
                static_cast<int>(
                    vfxParticles.size()
                    )
                )
            {
                DrawVfxParticleVisual(
                    vfxParticles[
                        item.index
                    ]
                );
            }

            break;
        }
        }
    }
}

void Game::DrawEnemySpawnGroundEffects3D() const
{
    if (
        !enemySpawnEffectSpriteLoaded ||
        enemySpawnEffectSpriteSheet.id ==
        0
        )
    {
        return;
    }

    for (
        const Enemy& enemy :
        enemies
        )
    {
        if (
            !enemy.active ||
            !IsChamberVisible(
                enemy.chamberId
            ) ||
            enemy.type ==
            EnemyType::Boss ||
            enemy.spawnState ==
            EnemySpawnState::Ready
            )
        {
            continue;
        }

        // GroundEffect animates through the sheet.
        // Emerging and FadeOut use the final frame.
        int frame =
            enemySpawnEffectFrameCount -
            1;

        unsigned char alpha =
            255;

        if (
            enemy.spawnState ==
            EnemySpawnState::GroundEffect
            )
        {
            const float progress =
                Clamp(
                    enemy.spawnStateTimer /
                    std::max(
                        0.01f,
                        enemySpawnGroundEffectDuration
                    ),
                    0.0f,
                    1.0f
                );

            frame =
                std::min(
                    enemySpawnEffectFrameCount -
                    1,

                    static_cast<int>(
                        progress *
                        static_cast<float>(
                            enemySpawnEffectFrameCount
                            )
                        )
                );
        }
        else if (
            enemy.spawnState ==
            EnemySpawnState::Emerging
            )
        {
            // Hold the final frame at full opacity while
            // the enemy is still rising.
            frame =
                enemySpawnEffectFrameCount -
                1;

            alpha =
                255;
        }
        else if (
            enemy.spawnState ==
            EnemySpawnState::FadeOut
            )
        {
            frame =
                enemySpawnEffectFrameCount -
                1;

            const float fadeProgress =
                Clamp(
                    enemy.spawnStateTimer /
                    std::max(
                        0.01f,
                        enemySpawnFadeDuration
                    ),
                    0.0f,
                    1.0f
                );

            const float smoothFade =
                fadeProgress *
                fadeProgress *
                (
                    3.0f -
                    2.0f *
                    fadeProgress
                    );

            alpha =
                static_cast<unsigned char>(
                    255.0f *
                    (
                        1.0f -
                        smoothFade
                        )
                    );
        }

        const float visualSize =
            std::max(
                enemySpawnEffectVisualSize,
                enemy.radius *
                4.50f
            );

        DrawHybridGroundTextureFrame(
            enemySpawnEffectSpriteSheet,
            GetEnemySpawnEffectSourceRect(
                frame
            ),
            enemy.pos,
            visualSize,
            2.4f,
            Color{
                255,
                255,
                255,
                alpha
            }
        );
    }
}

void Game::DrawWorldGroundEffects()
{

    DrawWorldShadows();

    DrawEnemySpawnGroundEffects2D();

    DrawBossFallingRockTelegraphs2D();
    DrawBossLasers2D();

    // Player movement path
    if (
        hasPath &&
        pathIndex <
        static_cast<int>(
            currentPath.size()
            )
        )
    {
        for (
            int i = pathIndex;
            i <
            static_cast<int>(
                currentPath.size()
                ) - 1;
                ++i
            )
        {
            DrawLineEx(
                WorldToViewElevated(
                    currentPath[i]
                ),
                WorldToViewElevated(
                    currentPath[i + 1]
                ),
                3.0f,
                Color{
                    255,
                    255,
                    255,
                    100
                }
            );
        }

        DrawGroundCircleLines(
            currentPath.back(),
            18.0f,
            Color{
                255,
                255,
                255,
                180
            }
        );
    }

    // NPC interaction areas
    for (const NPC& npc : npcs)
    {
        DrawGroundCircleLines(
            npc.position,
            interactDistance,
            Color{
                255,
                255,
                255,
                60
            }
        );
    }

    // Huashan ground trajectory
    if (huashanJumpActive)
    {
        DrawLineEx(
            WorldToViewElevated(
                huashanJumpStart
            ),
            WorldToViewElevated(
                huashanJumpEnd
            ),
            3.0f,
            Color{
                255,
                80,
                55,
                130
            }
        );
    }

    // Dongfeng aiming line
    DrawDongfengTelegraph();

    // Ground-based VFX
    for (
        const VfxParticle& particle :
        vfxParticles
        )
    {
        if (
            particle.active &&
            IsGroundVfx(particle.type)
            )
        {
            DrawVfxParticleVisual(
                particle
            );
        }
    }

    DrawHuashanImpactGround2D();
}

void Game::DrawWorldForegroundEffects()
{

    DrawBossFallingRocks2D();
    for (
        const VfxParticle& particle :
        vfxParticles
        )
    {
        if (
            particle.active &&
            IsForegroundVfx(
                particle.type
            )
            )
        {
            DrawVfxParticleVisual(
                particle
            );
        }
    }
}

void Game::EnsureLightingTargets()
{
    int width =
        GetScreenWidth();

    int height =
        GetScreenHeight();

    if (
        width <= 0 ||
        height <= 0
        )
    {
        return;
    }

    bool alreadyCorrectSize =
        lightingWidth == width &&
        lightingHeight == height &&
        sceneTarget.id != 0 &&
        lightTarget.id != 0;

    if (alreadyCorrectSize)
    {
        return;
    }

    if (sceneTarget.id != 0)
    {
        UnloadRenderTexture(
            sceneTarget
        );

        sceneTarget = {};
    }

    if (lightTarget.id != 0)
    {
        UnloadRenderTexture(
            lightTarget
        );

        lightTarget = {};
    }

    sceneTarget =
        LoadRenderTexture(
            width,
            height
        );

    lightTarget =
        LoadRenderTexture(
            width,
            height
        );

    if (
        sceneTarget.id == 0 ||
        lightTarget.id == 0
        )
    {
        TraceLog(
            LOG_ERROR,
            "[LIGHTING] Render target creation failed at %dx%d",
            width,
            height
        );

        lightingWidth = 0;
        lightingHeight = 0;

        return;
    }

    // Keep the pixel-art world sharp.
    SetTextureFilter(
        sceneTarget.texture,
        TEXTURE_FILTER_POINT
    );

    // The light map should remain soft.
    SetTextureFilter(
        lightTarget.texture,
        TEXTURE_FILTER_BILINEAR
    );

    lightingWidth = width;
    lightingHeight = height;

    TraceLog(
        LOG_INFO,
        "[LIGHTING] Render targets created: %dx%d",
        width,
        height
    );
}

void Game::DrawLightMap()
{
    if (
        lightTarget.id == 0 ||
        radialLightTexture.id == 0
        )
    {
        return;
    }

    BeginTextureMode(
        lightTarget
    );

    // Every part of the map starts with this base lighting.
    // Lower RGB values create darker shadows.
    ClearBackground(
        ambientLight
    );

    const bool useLegacyCamera =
        rendererMode == WorldRendererMode::Legacy2D;

    if (useLegacyCamera)
    {
        BeginMode2D(camera);
    }

    // Each radial light is added on top of the ambient colour.
    BeginBlendMode(
        BLEND_ADDITIVE
    );

    for (const Light2D& light : lights)
    {
        if (!light.enabled)
        {
            continue;
        }

        if (
            !IsWorldPositionInVisibleChamber(
                light.position
            )
            )
        {
            continue;
        }

        const int lightChamberId =
            FindChamberAtWorld(
                light.position
            );

        const float chamberVisibility =
            GetChamberVisibility(
                lightChamberId
            );

        if (chamberVisibility <= 0.01f)
        {
            continue;
        }

        Vector2 viewPosition =
            WorldToViewElevated(
                light.position
            );

        viewPosition =
            Vector2Add(
                viewPosition,
                light.viewOffset
            );

        float screenScale = 1.0f;

        if (rendererMode == WorldRendererMode::Hybrid3D)
        {
            screenScale =
                static_cast<float>(GetScreenHeight()) /
                std::max(1.0f, hybridCamera.fovy) *
                hybridUnitsPerPixel;
        }

        float width =
            light.radius * 2.0f *
            light.scale.x *
            screenScale;

        float height =
            light.radius * 2.0f *
            light.scale.y *
            screenScale;

        Rectangle source{
            0.0f,
            0.0f,
            static_cast<float>(
                radialLightTexture.width
            ),
            static_cast<float>(
                radialLightTexture.height
            )
        };

        Rectangle destination{
            viewPosition.x,
            viewPosition.y,
            width,
            height
        };

        Vector2 origin{
          width * 0.5f,
          height * 0.5f
        };

        float strength =
            Clamp(
                light.intensity *
                chamberVisibility,
                0.0f,
                1.0f
            );

        Color tint =
            light.color;

        tint.a =
            static_cast<unsigned char>(
                255.0f * strength
                );

        DrawTexturePro(
            radialLightTexture,
            source,
            destination,
            origin,
            light.rotationDegrees,
            tint
        );
    }

    // --------------------------------------------------
// Hybrid dash glow
//
// Maximum cost is maxDashAfterimages textured quads.
// With the current setting, that is at most five.
// --------------------------------------------------

    if (
        rendererMode ==
        WorldRendererMode::Hybrid3D &&
        !dashAfterimages.empty()
        )
    {
        const float dashScreenScale =
            static_cast<float>(
                GetScreenHeight()
                ) /
            std::max(
                1.0f,
                hybridCamera.fovy
            ) *
            hybridUnitsPerPixel;

        Rectangle source{
            0.0f,
            0.0f,
            static_cast<float>(
                radialLightTexture.width
            ),
            static_cast<float>(
                radialLightTexture.height
            )
        };

        for (
            const DashAfterimage& afterimage :
            dashAfterimages
            )
        {
            if (afterimage.maxLife <= 0.0f)
            {
                continue;
            }

            const float lifeRatio =
                Clamp(
                    afterimage.life /
                    afterimage.maxLife,
                    0.0f,
                    1.0f
                );

            Vector2 screenPosition =
                WorldToViewElevated(
                    afterimage.worldPosition,
                    34.0f
                );

            const float glowSize =
                150.0f *
                dashScreenScale *
                (
                    0.72f +
                    lifeRatio *
                    0.28f
                    );

            Rectangle destination{
                screenPosition.x,
                screenPosition.y,
                glowSize,
                glowSize
            };

            Vector2 origin{
                glowSize * 0.5f,
                glowSize * 0.5f
            };

            DrawTexturePro(
                radialLightTexture,
                source,
                destination,
                origin,
                0.0f,
                Color{
                    65,
                    190,
                    255,
                    static_cast<unsigned char>(
                        125.0f *
                        lifeRatio *
                        lifeRatio
                    )
                }
            );
        }
    }

    EndBlendMode();

    if (useLegacyCamera)
    {
        EndMode2D();
    }

    EndTextureMode();
}

void Game::InitLighting()
{
    lightingReady = false;

    // ------------------------------------------
    // Load radial light texture
    // ------------------------------------------

    const char* radialLightPath =
        "Assets/effects/radial_light.png";

    if (!FileExists(radialLightPath))
    {
        TraceLog(
            LOG_ERROR,
            "[LIGHTING] Radial light texture not found: %s",
            radialLightPath
        );

        return;
    }

    radialLightTexture =
        LoadTexture(
            radialLightPath
        );

    if (radialLightTexture.id == 0)
    {
        TraceLog(
            LOG_ERROR,
            "[LIGHTING] Could not load radial light texture."
        );

        return;
    }

    // Light gradients should be smooth.
    // Do not use POINT filtering for the radial texture.
    SetTextureFilter(
        radialLightTexture,
        TEXTURE_FILTER_BILINEAR
    );

    // ------------------------------------------
    // Select desktop or web shader
    // ------------------------------------------

#if defined(PLATFORM_WEB) || defined(__EMSCRIPTEN__)

    const char* fragmentPath =
        "Assets/shaders/lighting_100.fs";

#else

    const char* fragmentPath =
        "Assets/shaders/lighting_330.fs";

#endif

    if (!FileExists(fragmentPath))
    {
        TraceLog(
            LOG_ERROR,
            "[LIGHTING] Shader not found: %s",
            fragmentPath
        );

        return;
    }

    // nullptr tells raylib to use its default vertex shader.
    lightingShader =
        LoadShader(
            nullptr,
            fragmentPath
        );

    if (lightingShader.id == 0)
    {
        TraceLog(
            LOG_ERROR,
            "[LIGHTING] Failed to load lighting shader."
        );

        return;
    }

    lightingLightMapLocation =
        GetShaderLocation(
            lightingShader,
            "lightMap"
        );

    if (lightingLightMapLocation < 0)
    {
        TraceLog(
            LOG_ERROR,
            "[LIGHTING] Uniform not found: lightMap"
        );

        return;
    }

    // Create render textures at the current window size.
    EnsureLightingTargets();

    if (
        sceneTarget.id == 0 ||
        lightTarget.id == 0
        )
    {
        TraceLog(
            LOG_ERROR,
            "[LIGHTING] Failed to create render targets."
        );

        return;
    }

    // Create permanent player light and
    // random environmental test lights.
    CreateTestLights();

    lightingReady = true;

    TraceLog(
        LOG_INFO,
        "[LIGHTING] Ready | radial=%u shader=%u scene=%u light=%u",
        radialLightTexture.id,
        lightingShader.id,
        sceneTarget.id,
        lightTarget.id
    );
}

void Game::DrawObstacleShadow(
    const Obstacle& obstacle
) const
{

    if (!obstacle.castsShadow)
    {
        return;
    }

    // For the first stacking implementation,
    // only the ground-level block creates the
    // soft contact shadow.
    //
    // Otherwise a six-block wall would create
    // six overlapping circular shadows.
    if (obstacle.heightLevel > 0)
    {
        return;
    }

    if (!worldShadowsEnabled)
    {
        return;
    }

    if (radialLightTexture.id == 0)
    {
        return;
    }

    Vector2 basePosition =
        WorldToViewElevated(
            obstacle.position
        );

    Vector2 shadowDirection =
        worldShadowDirectionView;

    if (
        Vector2Length(
            shadowDirection
        ) <= 0.001f
        )
    {
        shadowDirection = {
            -1.0f,
            0.35f
        };
    }

    shadowDirection =
        Vector2Normalize(
            shadowDirection
        );

    Rectangle source{
        0.0f,
        0.0f,
        static_cast<float>(
            radialLightTexture.width
        ),
        static_cast<float>(
            radialLightTexture.height
        )
    };

    // --------------------------------------------------
    // Main soft canopy shadow
    // --------------------------------------------------

    float canopyWidth =
        obstacle.size.x * 0.92f;

    float canopyHeight =
        obstacle.size.y * 0.38f;

    if (obstacle.type == 1)
    {
        // Smaller shadow for rocks.
        canopyWidth *= 0.65f;
        canopyHeight *= 0.65f;
    }

    Vector2 canopyCenter =
        Vector2Add(
            basePosition,
            Vector2Scale(
                shadowDirection,
                obstacle.size.y * 0.06f
            )
        );

    Rectangle canopyDestination{
        canopyCenter.x,
        canopyCenter.y + 3.0f,
        canopyWidth,
        canopyHeight
    };

    unsigned char canopyAlpha =
        static_cast<unsigned char>(
            Clamp(
                static_cast<float>(
                    worldShadowOpacity
                    ),
                0.0f,
                255.0f
            )
            );

    DrawTexturePro(
        radialLightTexture,
        source,
        canopyDestination,
        {
            canopyDestination.width * 0.5f,
            canopyDestination.height * 0.5f
        },
        0.0f,
        Color{
            8,
            10,
            14,
            canopyAlpha
        }
    );

    // --------------------------------------------------
    // Small contact shadow directly below the trunk
    // --------------------------------------------------

    float contactWidth =
        std::max(
            18.0f,
            obstacle.colliderSize.x * 0.82f
        );

    float contactHeight =
        std::max(
            8.0f,
            obstacle.colliderSize.y * 0.42f
        );

    Rectangle contactDestination{
        basePosition.x,
        basePosition.y + 3.0f,
        contactWidth,
        contactHeight
    };

    unsigned char contactAlpha =
        static_cast<unsigned char>(
            Clamp(
                static_cast<float>(
                    worldShadowOpacity + 18
                    ),
                0.0f,
                255.0f
            )
            );

    DrawTexturePro(
        radialLightTexture,
        source,
        contactDestination,
        {
            contactDestination.width * 0.5f,
            contactDestination.height * 0.5f
        },
        0.0f,
        Color{
            4,
            6,
            8,
            contactAlpha
        }
    );
}

void Game::DrawWorldShadows() const
{
    if (!worldShadowsEnabled)
    {
        return;
    }

    BeginBlendMode(
        BLEND_ALPHA
    );

    for (const Obstacle& obstacle : obstacles)
    {
        if (
            !IsWorldPositionInVisibleChamber(
                obstacle.position
            )
            )
        {
            continue;
        }

        DrawObstacleShadow(
            obstacle
        );
    }

    EndBlendMode();
}

void Game::CreateTestLights()
{
    lights.clear();

    // --------------------------------------------------
    // Permanent player light
    // --------------------------------------------------

    Light2D playerLight;

    playerLight.position =
        playerPosition;

    // Small readability light rather than
    // the main environmental light.
    playerLight.radius =
        125.0f;

    playerLight.intensity =
        0.52f;

    playerLight.color = {
        255,
        242,
        220,
        255
    };

    playerLight.enabled = true;
    playerLight.followsPlayer = true;

    // The player's world position is at the feet.
    // Move the light upward toward the torso.
    playerLight.viewOffset = {
        0.0f,
        -48.0f
    };

    playerLight.scale = {
        1.0f,
        1.0f
    };

    playerLight.rotationDegrees =
        0.0f;

    lights.push_back(
        playerLight
    );

    // --------------------------------------------------
    // Random test environment lights
    // --------------------------------------------------

    constexpr int testLightCount = 10;

    float halfMapWidth =
        static_cast<float>(
            MapWidth
            ) *
        TileSize *
        0.5f;

    float halfMapHeight =
        static_cast<float>(
            MapHeight
            ) *
        TileSize *
        0.5f;

    const Color testColors[] = {
        // Warm torch
        Color{ 255, 170, 90, 255 },

        // Soft sunlight
        Color{ 255, 225, 175, 255 },

        // Cool magic
        Color{ 120, 175, 255, 255 },

        // Pale neutral light
        Color{ 225, 235, 255, 255 },

        // Soft green magical light
        Color{ 145, 255, 185, 255 }
    };

    constexpr int colorCount =
        sizeof(testColors) /
        sizeof(testColors[0]);

    for (int i = 0; i < testLightCount; ++i)
    {
        Light2D testLight;

        testLight.position = {
            static_cast<float>(
                GetRandomValue(
                    static_cast<int>(
                        -halfMapWidth + 160.0f
                    ),
                    static_cast<int>(
                        halfMapWidth - 160.0f
                    )
                )
            ),

            static_cast<float>(
                GetRandomValue(
                    static_cast<int>(
                        -halfMapHeight + 160.0f
                    ),
                    static_cast<int>(
                        halfMapHeight - 160.0f
                    )
                )
            )
        };

        testLight.radius =
            static_cast<float>(
                GetRandomValue(
                    180,
                    430
                )
                );

        // 0.18 to 0.62
        testLight.intensity =
            static_cast<float>(
                GetRandomValue(
                    18,
                    62
                )
                ) /
            100.0f;

        int colorIndex =
            GetRandomValue(
                0,
                colorCount - 1
            );

        testLight.color =
            testColors[colorIndex];

        testLight.enabled = true;
        testLight.followsPlayer = false;

        lights.push_back(
            testLight
        );
    }

    TraceLog(
        LOG_INFO,
        "[LIGHTING] Created %d lights, including player light.",
        static_cast<int>(
            lights.size()
            )
    );
}

void Game::LoadShooterSpriteSheets()
{
    auto LoadShooterSheet =
        [this](
            const char* path,
            Texture2D& texture,
            bool& loaded,
            int& framesPerRow
            )
        {
            if (texture.id != 0)
            {
                UnloadTexture(
                    texture
                );

                texture = {};
            }

            loaded = false;

            texture =
                LoadTexture(
                    path
                );

            if (texture.id == 0)
            {
                TraceLog(
                    LOG_WARNING,
                    "[SHOOTER SPRITE] Failed to load: %s",
                    path
                );

                return;
            }

            SetTextureFilter(
                texture,
                TEXTURE_FILTER_POINT
            );

            framesPerRow =
                std::max(
                    1,
                    texture.width /
                    std::max(
                        1,
                        shooterFrameWidth
                    )
                );

            const int expectedHeight =
                shooterFrameHeight *
                shooterDirectionRows;

            if (
                texture.height <
                expectedHeight
                )
            {
                TraceLog(
                    LOG_WARNING,
                    "[SHOOTER SPRITE] Sheet too short: %s | "
                    "expected height=%d actual=%d",
                    path,
                    expectedHeight,
                    texture.height
                );
            }

            loaded = true;

            TraceLog(
                LOG_INFO,
                "[SHOOTER SPRITE] Loaded: %s | "
                "size=%dx%d frames=%d",
                path,
                texture.width,
                texture.height,
                framesPerRow
            );
        };

    LoadShooterSheet(
        "Assets/enemies/shooter_idle.png",
        shooterIdleSpriteSheet,
        shooterIdleSpriteLoaded,
        shooterIdleFramesPerRow
    );

    LoadShooterSheet(
        "Assets/enemies/shooter_walk.png",
        shooterWalkSpriteSheet,
        shooterWalkSpriteLoaded,
        shooterWalkFramesPerRow
    );

    LoadShooterSheet(
        "Assets/enemies/shooter_attack.png",
        shooterAttackSpriteSheet,
        shooterAttackSpriteLoaded,
        shooterAttackFramesPerRow
    );
}

void Game::SetEnemyFacingFromWorldDirection(
    Enemy& enemy,
    Vector2 worldDirection
)
{
    if (
        Vector2Length(
            worldDirection
        ) <=
        0.001f
        )
    {
        return;
    }

    Vector2 viewDirection =
        WorldVectorToView(
            worldDirection
        );

    if (
        Vector2Length(
            viewDirection
        ) <=
        0.001f
        )
    {
        return;
    }

    viewDirection =
        Vector2Normalize(
            viewDirection
        );

    enemy.spriteDirection =
        GetPlayerDirectionFromVector(
            viewDirection
        );
}

bool Game::GetShooterAnimationFrame(
    const Enemy& enemy,
    Texture2D& outTexture,
    Rectangle& outSource,
    float& outWidthPixels,
    float& outHeightPixels,
    float& outAnchorY
) const
{
    if (
        enemy.type !=
        EnemyType::Shooter
        )
    {
        return false;
    }

    int framesPerRow = 1;
    bool selected = false;

    auto SelectSheet =
        [&](
            Texture2D texture,
            bool loaded,
            int frameCount
            )
        {
            if (
                selected ||
                !loaded ||
                texture.id == 0
                )
            {
                return;
            }

            outTexture = texture;

            framesPerRow =
                std::max(
                    1,
                    frameCount
                );

            selected = true;
        };

    switch (enemy.animationState)
    {
    case EnemyAnimationState::Attacking:
        SelectSheet(
            shooterAttackSpriteSheet,
            shooterAttackSpriteLoaded,
            shooterAttackFramesPerRow
        );
        break;

    case EnemyAnimationState::Walking:
        SelectSheet(
            shooterWalkSpriteSheet,
            shooterWalkSpriteLoaded,
            shooterWalkFramesPerRow
        );
        break;

    case EnemyAnimationState::Idle:
    default:
        SelectSheet(
            shooterIdleSpriteSheet,
            shooterIdleSpriteLoaded,
            shooterIdleFramesPerRow
        );
        break;
    }

    // Safe fallbacks when a particular sheet is missing.
    SelectSheet(
        shooterIdleSpriteSheet,
        shooterIdleSpriteLoaded,
        shooterIdleFramesPerRow
    );

    SelectSheet(
        shooterWalkSpriteSheet,
        shooterWalkSpriteLoaded,
        shooterWalkFramesPerRow
    );

    SelectSheet(
        shooterAttackSpriteSheet,
        shooterAttackSpriteLoaded,
        shooterAttackFramesPerRow
    );

    if (!selected)
    {
        return false;
    }

    const int frame =
        std::max(
            0,
            std::min(
                enemy.shooterAnimationFrame,
                framesPerRow - 1
            )
        );

    int directionRow =
        GetPlayerDirectionRow(
            enemy.spriteDirection
        );

    directionRow =
        std::max(
            0,
            std::min(
                directionRow,
                shooterDirectionRows - 1
            )
        );

    outSource = {
        static_cast<float>(
            frame *
            shooterFrameWidth
        ),

        static_cast<float>(
            directionRow *
            shooterFrameHeight
        ),

        static_cast<float>(
            shooterFrameWidth
        ),

        static_cast<float>(
            shooterFrameHeight
        )
    };

    outWidthPixels =
        static_cast<float>(
            shooterFrameWidth
            ) *
        shooterVisualScale;

    outHeightPixels =
        static_cast<float>(
            shooterFrameHeight
            ) *
        shooterVisualScale;

    outAnchorY =
        0.98f;
    return true;
}

bool Game::MoveShooterAwayFromPlayer(
    Enemy& enemy,
    float dt
)
{
    Vector2 awayDirection =
        Vector2Subtract(
            enemy.pos,
            playerPosition
        );

    if (
        Vector2Length(
            awayDirection
        ) <=
        0.001f
        )
    {
        awayDirection = {
            1.0f,
            0.0f
        };
    }
    else
    {
        awayDirection =
            Vector2Normalize(
                awayDirection
            );
    }

    float movementSpeed =
        enemy.speed *
        shooterRetreatSpeedMultiplier;

    if (enemy.slowTimer > 0.0f)
    {
        movementSpeed *=
            enemy.slowMultiplier;
    }

    const float movementDistance =
        movementSpeed *
        dt;

    constexpr float probeAngles[]{
        0.0f,
        25.0f,
        -25.0f,
        50.0f,
        -50.0f,
        75.0f,
        -75.0f,
        100.0f,
        -100.0f
    };

    const float collisionRadius =
        std::max(
            6.0f,
            enemy.radius *
            0.65f
        );

    for (
        float angleDegrees :
    probeAngles
        )
    {
        const float angle =
            angleDegrees *
            DEG2RAD;

        const float cosine =
            cosf(
                angle
            );

        const float sine =
            sinf(
                angle
            );

        Vector2 probeDirection{
            awayDirection.x *
                cosine -
                awayDirection.y *
                sine,

            awayDirection.x *
                sine +
                awayDirection.y *
                cosine
        };

        Vector2 candidatePosition =
            Vector2Add(
                enemy.pos,
                Vector2Scale(
                    probeDirection,
                    movementDistance
                )
            );

        if (
            !CanEnemyStandAt(
                enemy.pos,
                candidatePosition,
                collisionRadius
            )
            )
        {
            continue;
        }

        enemy.pos =
            candidatePosition;

        SetEnemyFacingFromWorldDirection(
            enemy,
            probeDirection
        );

        enemy.path.clear();
        enemy.pathIndex = 0;

        return true;
    }

    return false;
}

void Game::UpdateEnemyShooter(
    Enemy& enemy,
    float dt,
    float distanceToPlayer
)
{
    // --------------------------------------------------
    // Preserve the existing boss projectile behavior.
    // Boss animations can be added separately later.
    // --------------------------------------------------

    /*\
        if (
        enemy.type ==
        EnemyType::Boss
        )
    {
        if (
            distanceToPlayer >
            enemy.shootRange
            )
        {
            return;
        }

        enemy.shootTimer += dt;

        if (
            enemy.shootTimer <
            enemy.shootInterval
            )
        {
            return;
        }

        enemy.shootTimer = 0.0f;

        Vector2 direction =
            Vector2Subtract(
                playerPosition,
                enemy.pos
            );

        if (
            Vector2Length(
                direction
            ) <=
            0.01f
            )
        {
            return;
        }

        // Boss remains on idle for shooting,
        // but turns to face the player.
        SetEnemyFacingFromWorldDirection(
            enemy,
            direction
        );

        direction =
            Vector2Normalize(
                direction
            );

        const float baseAngle =
            atan2f(
                direction.y,
                direction.x
            );

        for (
            int projectileIndex = -1;
            projectileIndex <= 1;
            ++projectileIndex
            )
        {
            const float angle =
                baseAngle +
                static_cast<float>(
                    projectileIndex
                    ) *
                0.18f;

            Vector2 target{
                enemy.pos.x +
                    cosf(angle) *
                    100.0f,

                enemy.pos.y +
                    sinf(angle) *
                    100.0f
            };

            SpawnEnemyProjectile(
                enemy.pos,
                target,
                enemy.bulletDamage,
                enemy.bulletSpeed,
                9.0f
            );
        }

        return;
    }
    */


    if (
        enemy.type !=
        EnemyType::Shooter
        )
    {
        return;
    }

    enemy.shootTimer =
        std::min(
            enemy.shootInterval,
            enemy.shootTimer +
            dt
        );

    auto SetAnimationState =
        [&](
            EnemyAnimationState state
            )
        {
            if (
                enemy.animationState ==
                state
                )
            {
                return;
            }

            enemy.animationState =
                state;

            enemy.shooterAnimationFrame = 0;
            enemy.shooterAnimationTimer = 0.0f;
        };

    auto ReleaseShooterArrow =
        [this, &enemy]()
        {
            const int requestedShotCount =
                std::max(
                    1,
                    enemy.shooterShotsPerAttack
                );

            if (
                enemy.shooterShotsFiredThisAttack >=
                requestedShotCount
                )
            {
                return;
            }

            enemy.shooterShotsFiredThisAttack++;

            const bool sameTerrainLevel =
                GetTerrainElevationAtWorld(
                    enemy.pos
                ) ==
                GetTerrainElevationAtWorld(
                    playerPosition
                );

            const float currentDistance =
                Vector2Distance(
                    enemy.pos,
                    playerPosition
                );

            if (
                !sameTerrainLevel ||
                currentDistance >
                enemy.shootRange *
                1.10f
                )
            {
                return;
            }

            Vector2 shotDirection =
                Vector2Subtract(
                    playerPosition,
                    enemy.pos
                );

            if (
                Vector2Length(
                    shotDirection
                ) <=
                0.001f
                )
            {
                return;
            }

            shotDirection =
                Vector2Normalize(
                    shotDirection
                );

            // Move the gameplay starting position slightly
            // forward from the archer's feet.
            const Vector2 projectileStart =
                Vector2Add(
                    enemy.pos,
                    Vector2Scale(
                        shotDirection,
                        shooterProjectileForwardOffset
                    )
                );

            SpawnEnemyProjectile(
                projectileStart,
                playerPosition,
                enemy.bulletDamage,
                enemy.bulletSpeed,
                7.0f,
                shooterProjectileVisualHeight
            );

            enemy.shooterProjectileFired =
                true;
        };

    auto AdvanceLoopingAnimation =
        [&](
            int frameCount,
            float frameDuration
            )
        {
            frameCount =
                std::max(
                    1,
                    frameCount
                );

            frameDuration =
                std::max(
                    0.01f,
                    frameDuration
                );

            enemy.shooterAnimationTimer +=
                dt;

            while (
                enemy.shooterAnimationTimer >=
                frameDuration
                )
            {
                enemy.shooterAnimationTimer -=
                    frameDuration;

                enemy.shooterAnimationFrame =
                    (
                        enemy.shooterAnimationFrame +
                        1
                        ) %
                    frameCount;
            }
        };

    const int enemyTerrainElevation =
        GetTerrainElevationAtWorld(
            enemy.pos
        );

    const int playerTerrainElevation =
        GetTerrainElevationAtWorld(
            playerPosition
        );

    const bool sameTerrainLevel =
        enemyTerrainElevation ==
        playerTerrainElevation;

    // --------------------------------------------------
    // Shooting animation
    //
    // Do not perform any movement in this state.
    // --------------------------------------------------

    if (
        enemy.animationState ==
        EnemyAnimationState::Attacking
        )
    {
        // The shooter remains stationary for the entire attack.
        enemy.path.clear();
        enemy.pathIndex = 0;

        SetEnemyFacingFromWorldDirection(
            enemy,
            Vector2Subtract(
                playerPosition,
                enemy.pos
            )
        );

        const int attackFrameCount =
            std::max(
                1,
                shooterAttackFramesPerRow
            );

        // Seven frames means:
        //
        // visible frame 1 = index 0
        // visible frame 7 = index 6
        const int lastFrameIndex =
            attackFrameCount -
            1;

        const int requestedShotCount =
            std::max(
                1,
                enemy.shooterShotsPerAttack
            );

        const float frameDuration =
            std::max(
                0.01f,
                shooterAttackFrameDuration
            );

        auto TryReleaseFinalFrameShot =
            [&]()
            {
                if (
                    enemy.shooterAnimationFrame !=
                    lastFrameIndex ||
                    enemy.shooterAttackImpactProcessed
                    )
                {
                    return;
                }

                enemy.shooterAttackImpactProcessed =
                    true;

                ReleaseShooterArrow();
            };

        // Handles the case where the final frame was entered
        // during the previous update.
        TryReleaseFinalFrameShot();

        // --------------------------------------------------
        // Third and later shots
        //
        // The complete animation may loop only once.
        // After the second pass reaches the final frame,
        // hold that final frame while releasing any remaining
        // burst arrows.
        // --------------------------------------------------

        if (
            enemy.shooterAttackLoopedOnce &&
            enemy.shooterAnimationFrame ==
            lastFrameIndex &&
            enemy.shooterShotsFiredThisAttack <
            requestedShotCount
            )
        {
            enemy.shooterExtraShotTimer +=
                dt;

            const float extraShotInterval =
                std::max(
                    0.01f,
                    shooterExtraShotInterval
                );

            while (
                enemy.shooterExtraShotTimer >=
                extraShotInterval &&
                enemy.shooterShotsFiredThisAttack <
                requestedShotCount
                )
            {
                enemy.shooterExtraShotTimer -=
                    extraShotInterval;

                ReleaseShooterArrow();
            }

            // Keep showing the final release frame until all
            // extra arrows in the burst have been fired.
            return;
        }

        enemy.shooterAnimationTimer +=
            dt;

        while (
            enemy.shooterAnimationTimer >=
            frameDuration
            )
        {
            enemy.shooterAnimationTimer -=
                frameDuration;

            // Advance toward the final frame.
            if (
                enemy.shooterAnimationFrame <
                lastFrameIndex
                )
            {
                enemy.shooterAnimationFrame++;

                // The projectile is released immediately when
                // the final frame becomes active.
                TryReleaseFinalFrameShot();

                continue;
            }

            // --------------------------------------------------
            // Current animation pass has finished.
            // --------------------------------------------------

            if (
                enemy.shooterShotsFiredThisAttack <
                requestedShotCount &&
                !enemy.shooterAttackLoopedOnce
                )
            {
                // Return to frame 0 only once.
                enemy.shooterAttackLoopedOnce =
                    true;

                enemy.shooterAnimationFrame =
                    0;

                enemy.shooterAnimationTimer =
                    0.0f;

                enemy.shooterExtraShotTimer =
                    0.0f;

                // Allow the final frame of the second pass
                // to release the next arrow.
                enemy.shooterAttackImpactProcessed =
                    false;

                return;
            }

            if (
                enemy.shooterShotsFiredThisAttack <
                requestedShotCount
                )
            {
                // The second pass is complete, but more arrows
                // remain. Hold the final frame; the block above
                // releases them without another animation loop.
                enemy.shooterAnimationFrame =
                    lastFrameIndex;

                enemy.shooterAnimationTimer =
                    0.0f;

                return;
            }

            // --------------------------------------------------
            // Complete attack finished.
            // --------------------------------------------------

            const bool firedAtLeastOneProjectile =
                enemy.shooterProjectileFired;

            SetAnimationState(
                EnemyAnimationState::Idle
            );

            enemy.shooterShotsFiredThisAttack =
                0;

            enemy.shooterAttackLoopedOnce =
                false;

            enemy.shooterExtraShotTimer =
                0.0f;

            enemy.shooterProjectileFired =
                false;

            enemy.shooterAttackImpactProcessed =
                false;

            if (firedAtLeastOneProjectile)
            {
                enemy.shooterRetreatTimer =
                    shooterRetreatDuration;
            }

            return;
        }

        return;
    }

    // --------------------------------------------------
    // Retreat after firing
    // --------------------------------------------------

    if (
        enemy.shooterRetreatTimer >
        0.0f
        )
    {
        enemy.shooterRetreatTimer -=
            dt;

        if (
            enemy.shooterRetreatTimer <
            0.0f
            )
        {
            enemy.shooterRetreatTimer =
                0.0f;
        }

        SetAnimationState(
            EnemyAnimationState::Walking
        );

        const bool moved =
            MoveShooterAwayFromPlayer(
                enemy,
                dt
            );

        if (!moved)
        {
            // The shooter reached a wall or enclosed corner.
            enemy.shooterRetreatTimer =
                0.0f;
        }

        AdvanceLoopingAnimation(
            shooterWalkFramesPerRow,
            shooterWalkFrameDuration
        );

        return;
    }

    // --------------------------------------------------
    // Move toward shooting range
    // --------------------------------------------------

    if (
        !sameTerrainLevel ||
        distanceToPlayer >
        enemy.shootRange
        )
    {
        enemy.pathRefreshTimer +=
            dt;

        const bool pathFinished =
            !enemy.path.empty() &&
            enemy.pathIndex >=
            static_cast<int>(
                enemy.path.size()
                );

        if (
            enemy.pathRefreshTimer >=
            enemy.pathRefreshInterval ||
            pathFinished
            )
        {
            RefreshEnemyPath(
                enemy
            );
        }

        const Vector2 previousPosition =
            enemy.pos;

        MoveEnemyAlongPath(
            enemy,
            dt
        );

        const Vector2 movement =
            Vector2Subtract(
                enemy.pos,
                previousPosition
            );

        if (
            Vector2Length(
                movement
            ) >
            0.01f
            )
        {
            SetAnimationState(
                EnemyAnimationState::Walking
            );

            SetEnemyFacingFromWorldDirection(
                enemy,
                movement
            );

            AdvanceLoopingAnimation(
                shooterWalkFramesPerRow,
                shooterWalkFrameDuration
            );
        }
        else
        {
            SetAnimationState(
                EnemyAnimationState::Idle
            );

            AdvanceLoopingAnimation(
                shooterIdleFramesPerRow,
                shooterIdleFrameDuration
            );
        }

        return;
    }

    // --------------------------------------------------
    // Player is in shooting range
    // --------------------------------------------------

    enemy.path.clear();
    enemy.pathIndex = 0;

    SetEnemyFacingFromWorldDirection(
        enemy,
        Vector2Subtract(
            playerPosition,
            enemy.pos
        )
    );

    if (
        enemy.shootTimer >=
        enemy.shootInterval
        )
    {
        enemy.shootTimer =
            0.0f;

        SetAnimationState(
            EnemyAnimationState::Attacking
        );

        enemy.shooterAnimationFrame =
            0;

        enemy.shooterAnimationTimer =
            0.0f;

        enemy.shooterShotsFiredThisAttack =
            0;

        enemy.shooterAttackLoopedOnce =
            false;

        enemy.shooterExtraShotTimer =
            0.0f;

        enemy.shooterProjectileFired =
            false;

        enemy.shooterAttackImpactProcessed =
            false;


        return;
    }

    SetAnimationState(
        EnemyAnimationState::Idle
    );

    AdvanceLoopingAnimation(
        shooterIdleFramesPerRow,
        shooterIdleFrameDuration
    );
}

void Game::LoadSmallEnemySpriteSheet()
{
    smallEnemySpriteSheet =
        LoadTexture(
            "Assets/enemies/small_blob.png"
        );

    if (smallEnemySpriteSheet.id == 0)
    {
        smallEnemySpriteLoaded = false;

        TraceLog(
            LOG_WARNING,
            "[ENEMY SPRITE] Could not load: "
            "Assets/enemies/small_blob.png"
        );

        return;
    }

    smallEnemySpriteLoaded = true;

    SetTextureFilter(
        smallEnemySpriteSheet,
        TEXTURE_FILTER_POINT
    );

    int detectedFrames =
        smallEnemySpriteSheet.width /
        smallEnemyFrameWidth;

    if (detectedFrames > 0)
    {
        smallEnemyFramesPerRow =
            detectedFrames;
    }

    int expectedHeight =
        smallEnemyFrameHeight *
        smallEnemyDirectionRows;

    if (
        smallEnemySpriteSheet.height <
        expectedHeight
        )
    {
        TraceLog(
            LOG_WARNING,
            "[ENEMY SPRITE] Directional sheet is too short. "
            "Expected at least %d pixels, received %d.",
            expectedHeight,
            smallEnemySpriteSheet.height
        );
    }

    TraceLog(
        LOG_INFO,
        "[ENEMY SPRITE] Small blob loaded: "
        "%dx%d | frames=%d | directions=%d",
        smallEnemySpriteSheet.width,
        smallEnemySpriteSheet.height,
        smallEnemyFramesPerRow,
        smallEnemyDirectionRows
    );
}

bool Game::IsSmallAnimatedEnemy(
    EnemyType type
) const
{
    return
        type == EnemyType::Grunt ||
        type == EnemyType::Runner;
}

void Game::UpdateSmallEnemyAnimation(
    Enemy& enemy,
    float dt
)
{
    bool usesBlobSprite =
        ShouldUseBlobEnemySprite(
            enemy
        );

    if (!usesBlobSprite)
    {
        // Keep this synchronized so enabling the toggle later
        // does not produce a false direction from an old position.
        enemy.previousAnimationPosition =
            enemy.pos;

        enemy.animationPositionInitialized =
            true;

        return;
    }

    // ---------------------------------------------
    // Update direction from movement.
    // ---------------------------------------------

    if (!enemy.animationPositionInitialized)
    {
        enemy.previousAnimationPosition =
            enemy.pos;

        enemy.animationPositionInitialized =
            true;
    }
    else
    {
        Vector2 worldMovement =
            Vector2Subtract(
                enemy.pos,
                enemy.previousAnimationPosition
            );

        enemy.previousAnimationPosition =
            enemy.pos;

        if (
            Vector2Length(worldMovement) >
            0.01f
            )
        {
            Vector2 viewMovement =
                WorldVectorToView(
                    worldMovement
                );

            if (
                Vector2Length(viewMovement) >
                0.001f
                )
            {
                viewMovement =
                    Vector2Normalize(
                        viewMovement
                    );

                enemy.spriteDirection =
                    GetPlayerDirectionFromVector(
                        viewMovement
                    );
            }
        }
    }

    // ---------------------------------------------
    // Update frames.
    // ---------------------------------------------

    if (smallEnemyFramesPerRow <= 1)
    {
        enemy.spriteFrame = 0;
        enemy.spriteAnimTimer = 0.0f;
        return;
    }

    if (enemy.frozenTimer > 0.0f)
    {
        return;
    }

    enemy.spriteAnimTimer += dt;

    while (
        enemy.spriteAnimTimer >=
        smallEnemyFrameDuration
        )
    {
        enemy.spriteAnimTimer -=
            smallEnemyFrameDuration;

        enemy.spriteFrame++;

        if (
            enemy.spriteFrame >=
            smallEnemyFramesPerRow
            )
        {
            enemy.spriteFrame = 0;
        }
    }
}

void Game::DrawSmallEnemySprite(
    const Enemy& enemy,
    Vector2 drawPosition,
    Color tint
)
{
    if (
        !smallEnemySpriteLoaded ||
        smallEnemySpriteSheet.id == 0
        )
    {
        return;
    }

    int frame = std::max(
        0,
        std::min(
            enemy.spriteFrame,
            smallEnemyFramesPerRow - 1
        )
    );

    int directionRow =
        GetPlayerDirectionRow(
            enemy.spriteDirection
        );

    directionRow = std::max(
        0,
        std::min(
            directionRow,
            smallEnemyDirectionRows - 1
        )
    );

    Rectangle source{
        static_cast<float>(
            frame *
            smallEnemyFrameWidth
        ),

        static_cast<float>(
            directionRow *
            smallEnemyFrameHeight
        ),

        static_cast<float>(
            smallEnemyFrameWidth
        ),

        static_cast<float>(
            smallEnemyFrameHeight
        )
    };

    float visualSize =
        GetBlobEnemyVisualSize(
            enemy
        );

    Rectangle destination{
        drawPosition.x,
        drawPosition.y,
        visualSize,
        visualSize
    };

    Vector2 origin{
        visualSize * 0.5f,
        visualSize * 0.5f
    };

    DrawTexturePro(
        smallEnemySpriteSheet,
        source,
        destination,
        origin,
        0.0f,
        tint
    );
}

void Game::DrawShooterEnemySprite(
    const Enemy& enemy,
    Vector2 drawPosition,
    Color tint
)
{
    Texture2D texture{};
    Rectangle source{};

    float widthPixels = 0.0f;
    float heightPixels = 0.0f;
    float anchorY = 1.0f;

    if (
        !GetShooterAnimationFrame(
            enemy,
            texture,
            source,
            widthPixels,
            heightPixels,
            anchorY
        )
        )
    {
        return;
    }

    Rectangle destination{
        drawPosition.x,
        drawPosition.y,
        widthPixels,
        heightPixels
    };

    Vector2 origin{
        widthPixels * 0.5f,
        heightPixels * anchorY
    };

    DrawTexturePro(
        texture,
        source,
        destination,
        origin,
        0.0f,
        tint
    );
}

bool Game::EnemyHasDedicatedSprite(
    EnemyType type
) const
{
    switch (type)
    {
    case EnemyType::Grunt:
    case EnemyType::Runner:
        return
            smallEnemySpriteLoaded &&
            smallEnemySpriteSheet.id != 0;

    case EnemyType::Tank:
        return false;

    case EnemyType::Shooter:
        return
            (
                shooterIdleSpriteLoaded &&
                shooterIdleSpriteSheet.id != 0
                ) ||
            (
                shooterWalkSpriteLoaded &&
                shooterWalkSpriteSheet.id != 0
                ) ||
            (
                shooterAttackSpriteLoaded &&
                shooterAttackSpriteSheet.id != 0
                );

    case EnemyType::Boss:
        return
            (
                bossIdleSpriteLoaded &&
                bossIdleSpriteSheet.id != 0
                ) ||
            (
                bossWalkSpriteLoaded &&
                bossWalkSpriteSheet.id != 0
                ) ||
            (
                bossAttackSpriteLoaded &&
                bossAttackSpriteSheet.id != 0
                );
    }

    return false;
}
bool Game::ShouldUseBlobEnemySprite(
    const Enemy& enemy
) const
{
    if (
        !smallEnemySpriteLoaded ||
        smallEnemySpriteSheet.id == 0
        )
    {
        return false;
    }

    // Grunt and Runner use the blob as their real sprite.
    if (IsSmallAnimatedEnemy(enemy.type))
    {
        return true;
    }

    // Other enemy types use it only as an editor placeholder.
    return
        buildMode &&
        useBlobForMissingEnemySprites &&
        !EnemyHasDedicatedSprite(enemy.type);
}

float Game::GetBlobEnemyVisualSize(
    const Enemy& enemy
) const
{
    float visualSize =
        enemy.radius *
        smallEnemyVisualScale;

    // Grunt and Runner use their normal calculated size.
    if (IsSmallAnimatedEnemy(enemy.type))
    {
        return visualSize;
    }

    // Placeholder enemies represent their relative size,
    // but are clamped so the blob is not tiny or enormous.
    return Clamp(
        visualSize,
        96.0f,
        180.0f
    );
}

// ============================================================================
// Hybrid 2.5D renderer
// ============================================================================

float Game::PixelsToHybridUnits(float pixels) const
{
    return pixels * hybridUnitsPerPixel;
}

float Game::GetHybridTerrainHeightUnits(
    Vector2 worldPosition
) const
{
    return PixelsToHybridUnits(
        GetTerrainHeightAtWorld(worldPosition)
    );
}

Vector3 Game::WorldToHybrid3D(
    Vector2 worldPosition,
    float additionalHeightPixels
) const
{
    return Vector3{
        worldPosition.x * hybridUnitsPerPixel,
        GetHybridTerrainHeightUnits(worldPosition) +
            PixelsToHybridUnits(additionalHeightPixels),
        worldPosition.y * hybridUnitsPerPixel
    };
}

void Game::LoadHybridShaders()
{
#if defined(PLATFORM_WEB) || defined(__EMSCRIPTEN__)
    const char* terrainVertexPath =
        "Assets/shaders/terrain_100.vs";

    const char* terrainFragmentPath =
        "Assets/shaders/terrain_100.fs";

    const char* billboardVertexPath =
        "Assets/shaders/billboard_100.vs";

    const char* billboardFragmentPath =
        "Assets/shaders/billboard_100.fs";
#else
    const char* terrainVertexPath =
        "Assets/shaders/terrain_330.vs";

    const char* terrainFragmentPath =
        "Assets/shaders/terrain_330.fs";

    const char* billboardVertexPath =
        "Assets/shaders/billboard_330.vs";

    const char* billboardFragmentPath =
        "Assets/shaders/billboard_330.fs";
#endif

    if (
        FileExists(terrainVertexPath) &&
        FileExists(terrainFragmentPath)
        )
    {
        hybridTerrainShader =
            LoadShader(
                terrainVertexPath,
                terrainFragmentPath
            );

        hybridTerrainShaderLoaded =
            hybridTerrainShader.id != 0;
    }

    if (!hybridTerrainShaderLoaded)
    {
        TraceLog(
            LOG_WARNING,
            "[HYBRID3D] Terrain shader unavailable; using raylib default shader."
        );
    }

    if (
        FileExists(billboardVertexPath) &&
        FileExists(billboardFragmentPath)
        )
    {
        hybridBillboardShader =
            LoadShader(
                billboardVertexPath,
                billboardFragmentPath
            );

        hybridBillboardShaderLoaded =
            hybridBillboardShader.id != 0;

        if (hybridBillboardShaderLoaded)
        {
            hybridBillboardFlashLocation =
                GetShaderLocation(
                    hybridBillboardShader,
                    "flashAmount"
                );

            TraceLog(
                LOG_INFO,
                "[HYBRID3D] Billboard flash uniform location=%d",
                hybridBillboardFlashLocation
            );
        }

    }

    if (!hybridBillboardShaderLoaded)
    {
        TraceLog(
            LOG_WARNING,
            "[HYBRID3D] Billboard alpha-discard shader unavailable."
        );
    }
}

void Game::InitHybrid3D()
{
    hybridCameraTargetWorld =
        playerPosition;

    hybridCamera.position = {
        24.0f,
        30.0f,
        24.0f
    };

    hybridCamera.target = {
        playerPosition.x * hybridUnitsPerPixel,
        GetHybridTerrainHeightUnits(playerPosition),
        playerPosition.y * hybridUnitsPerPixel
    };

    hybridCamera.up = {
        0.0f,
        1.0f,
        0.0f
    };

    // camera.zoom is the master zoom for both Legacy2D and Hybrid3D.
    hybridCameraOrthoSize =
        GetHybridOrthoSizeFromSharedZoom();

    hybridCamera.fovy =
        hybridCameraOrthoSize;

    hybridCamera.projection =
        CAMERA_ORTHOGRAPHIC;

    LoadHybridShaders();

    hybridCircleTexture =
        CreateHybridCircleTexture(false);

    hybridShadowTexture =
        CreateHybridCircleTexture(true);

    hybridTerrainBatches.resize(
        std::max(
            static_cast<size_t>(4),
            tileBrushes.size()
        )
    );

    MarkHybridTerrainDirty();
    RebuildHybridTerrain();
    UpdateHybridCamera(0.0f);

    TraceLog(
        LOG_INFO,
        "[HYBRID3D] Initialised | terrain=%d billboardShader=%d",
        hybridTerrainReady ? 1 : 0,
        hybridBillboardShaderLoaded ? 1 : 0
    );
}

void Game::ShutdownHybrid3D()
{
    for (HybridTerrainBatch& batch : hybridTerrainBatches)
    {
        if (batch.ready)
        {
            UnloadModel(batch.model);
            batch = {};
        }
    }

    hybridTerrainBatches.clear();
    hybridTerrainReady = false;

    if (hybridTerrainShader.id != 0)
    {
        UnloadShader(hybridTerrainShader);
        hybridTerrainShader = {};
    }

    if (hybridBillboardShader.id != 0)
    {
        UnloadShader(hybridBillboardShader);
        hybridBillboardShader = {};
    }

    hybridTerrainShaderLoaded = false;
    hybridBillboardShaderLoaded = false;

    if (hybridCircleTexture.id != 0)
    {
        UnloadTexture(hybridCircleTexture);
        hybridCircleTexture = {};
    }

    if (hybridShadowTexture.id != 0)
    {
        UnloadTexture(hybridShadowTexture);
        hybridShadowTexture = {};
    }
}

void Game::MarkHybridTerrainDirty()
{
    hybridTerrainDirty = true;
}

void Game::RebuildHybridTerrain()
{
    if (!hybridTerrainDirty)
    {
        return;
    }

    const size_t expectedCellCount =
        static_cast<size_t>(MapWidth * MapHeight);

    if (
        tiles.size() != expectedCellCount ||
        terrainCells.size() != expectedCellCount ||
        tileBrushes.empty()
        )
    {
        hybridTerrainReady = false;
        return;
    }

    for (HybridTerrainBatch& batch : hybridTerrainBatches)
    {
        if (batch.ready)
        {
            UnloadModel(batch.model);
            batch = {};
        }
    }

    hybridTerrainBatches.clear();

    struct ChamberTerrainBuilder
    {
        int chamberId = -1;
        int tileIndex = -1;
        HybridMeshBuilder meshBuilder;
    };

    std::vector<ChamberTerrainBuilder>
        builders;

    auto GetBuilder =
        [&builders](
            int chamberId,
            int tileIndex
            ) -> HybridMeshBuilder&
        {
            for (
                ChamberTerrainBuilder& entry :
                builders
                )
            {
                if (
                    entry.chamberId == chamberId &&
                    entry.tileIndex == tileIndex
                    )
                {
                    return entry.meshBuilder;
                }
            }

            ChamberTerrainBuilder entry;
            entry.chamberId = chamberId;
            entry.tileIndex = tileIndex;

            builders.push_back(
                std::move(entry)
            );

            return
                builders.back().meshBuilder;
        };

    const float originX =
        -static_cast<float>(MapWidth) *
        TileSize *
        0.5f;

    const float originY =
        -static_cast<float>(MapHeight) *
        TileSize *
        0.5f;

    const float levelHeight =
        PixelsToHybridUnits(
            terrainElevationStep
        );

    auto GetValidTileIndex =
        [this](int cellX, int cellY)
        {
            if (!IsCellInside(cellX, cellY))
            {
                return static_cast<int>(TileType::Grass);
            }

            int tileIndex =
                tiles[CellIndex(cellX, cellY)];

            if (
                tileIndex < 0 ||
                tileIndex >=
                static_cast<int>(tileBrushes.size())
                )
            {
                tileIndex =
                    static_cast<int>(TileType::Grass);
            }

            return tileIndex;
        };

    struct CornerHeights
    {
        float nw = 0.0f;
        float ne = 0.0f;
        float se = 0.0f;
        float sw = 0.0f;
    };

    auto GetCornerHeights =
        [this, levelHeight](
            int cellX,
            int cellY
            )
        {
            CornerHeights heights{};

            if (
                !IsCellInside(
                    cellX,
                    cellY
                ) ||
                !IsCellEnabled(
                    cellX,
                    cellY
                )
                )
            {
                return heights;
            }

            const TerrainCell& cell =
                terrainCells[
                    CellIndex(cellX, cellY)
                ];

            const int elevation =
                GetTerrainElevation(
                    cellX,
                    cellY
                );

            const float baseHeight =
                static_cast<float>(elevation) *
                levelHeight;

            heights.nw = baseHeight;
            heights.ne = baseHeight;
            heights.se = baseHeight;
            heights.sw = baseHeight;

            int offsetX = 0;
            int offsetY = 0;

            if (
                !GetRampDirectionOffset(
                    cell.rampDirection,
                    offsetX,
                    offsetY
                )
                )
            {
                return heights;
            }

            const int targetX =
                cellX + offsetX;

            const int targetY =
                cellY + offsetY;

            if (
                !IsCellInside(targetX, targetY) ||
                GetTerrainElevation(targetX, targetY) !=
                elevation + 1
                )
            {
                return heights;
            }

            const float highHeight =
                baseHeight + levelHeight;

            switch (cell.rampDirection)
            {
            case RampDirection::North:
                heights.nw = highHeight;
                heights.ne = highHeight;
                break;

            case RampDirection::East:
                heights.ne = highHeight;
                heights.se = highHeight;
                break;

            case RampDirection::South:
                heights.sw = highHeight;
                heights.se = highHeight;
                break;

            case RampDirection::West:
                heights.nw = highHeight;
                heights.sw = highHeight;
                break;

            case RampDirection::None:
            default:
                break;
            }

            return heights;
        };

    constexpr float wallEpsilon = 0.0001f;

    for (int y = 0; y < MapHeight; ++y)
    {
        for (int x = 0; x < MapWidth; ++x)
        {
            if (!IsCellEnabled(x, y))
            {
                continue;
            }

            const int tileIndex =
                GetValidTileIndex(x, y);

            const int chamberId =
                terrainCells[
                    CellIndex(
                        x,
                        y
                    )
                ].chamberId;

            HybridMeshBuilder& topBuilder =
                GetBuilder(
                    chamberId,
                    tileIndex
                );

            const CornerHeights current =
                GetCornerHeights(x, y);

            const float x0 =
                (originX +
                    static_cast<float>(x) * TileSize) *
                hybridUnitsPerPixel;

            const float x1 =
                (originX +
                    static_cast<float>(x + 1) * TileSize) *
                hybridUnitsPerPixel;

            const float z0 =
                (originY +
                    static_cast<float>(y) * TileSize) *
                hybridUnitsPerPixel;

            const float z1 =
                (originY +
                    static_cast<float>(y + 1) * TileSize) *
                hybridUnitsPerPixel;

            const Vector3 nw{
                x0,
                current.nw,
                z0
            };

            const Vector3 ne{
                x1,
                current.ne,
                z0
            };

            const Vector3 se{
                x1,
                current.se,
                z1
            };

            const Vector3 sw{
                x0,
                current.sw,
                z1
            };

            topBuilder.AddQuad(
                nw,
                sw,
                se,
                ne,
                { 0.0f, 1.0f, 0.0f },
                WHITE
            );

            // East edge.
            if (
                !IsRampConnectionBetweenCells(
                    x,
                    y,
                    x + 1,
                    y
                )
                )
            {
                const CornerHeights neighbour =
                    GetCornerHeights(x + 1, y);

                const float lowerNe =
                    std::min(current.ne, neighbour.nw);

                const float lowerSe =
                    std::min(current.se, neighbour.sw);

                if (
                    current.ne - lowerNe > wallEpsilon ||
                    current.se - lowerSe > wallEpsilon
                    )
                {
                    const int eastWallTile =
                        GetTerrainWallTileIndex(
                            x,
                            y,
                            TerrainWallFace::East
                        );

                    HybridMeshBuilder& eastWallBuilder =
                        GetBuilder(
                            chamberId,
                            eastWallTile
                        );

                    eastWallBuilder.AddQuad(
                        ne,
                        se,
                        {
                            x1,
                            lowerSe,
                            z1
                        },
                        {
                            x1,
                            lowerNe,
                            z0
                        },
                        {
                            1.0f,
                            0.0f,
                            0.0f
                        },
                        ScaleHybridColor(
                            WHITE,
                            0.70f
                        )
                    );
                }
            }

            // West edge.
            if (
                !IsRampConnectionBetweenCells(
                    x,
                    y,
                    x - 1,
                    y
                )
                )
            {
                const CornerHeights neighbour =
                    GetCornerHeights(x - 1, y);

                const float lowerNw =
                    std::min(current.nw, neighbour.ne);

                const float lowerSw =
                    std::min(current.sw, neighbour.se);

                if (
                    current.nw - lowerNw > wallEpsilon ||
                    current.sw - lowerSw > wallEpsilon
                    )
                {
                    const int westWallTile =
                        GetTerrainWallTileIndex(
                            x,
                            y,
                            TerrainWallFace::West
                        );

                    HybridMeshBuilder& westWallBuilder =
                        GetBuilder(
                            chamberId,
                            westWallTile
                        );

                    westWallBuilder.AddQuad(
                        sw,
                        nw,
                        {
                            x0,
                            lowerNw,
                            z0
                        },
                        {
                            x0,
                            lowerSw,
                            z1
                        },
                        {
                            -1.0f,
                            0.0f,
                            0.0f
                        },
                        ScaleHybridColor(
                            WHITE,
                            0.62f
                        )
                    );
                }
            }

            // South edge.
            if (
                !IsRampConnectionBetweenCells(
                    x,
                    y,
                    x,
                    y + 1
                )
                )
            {
                const CornerHeights neighbour =
                    GetCornerHeights(x, y + 1);

                const float lowerSw =
                    std::min(current.sw, neighbour.nw);

                const float lowerSe =
                    std::min(current.se, neighbour.ne);

                if (
                    current.sw - lowerSw > wallEpsilon ||
                    current.se - lowerSe > wallEpsilon
                    )
                {
                    const int southWallTile =
                        GetTerrainWallTileIndex(
                            x,
                            y,
                            TerrainWallFace::South
                        );

                    HybridMeshBuilder& southWallBuilder =
                        GetBuilder(
                            chamberId,
                            southWallTile
                        );

                    southWallBuilder.AddQuad(
                        se,
                        sw,
                        {
                            x0,
                            lowerSw,
                            z1
                        },
                        {
                            x1,
                            lowerSe,
                            z1
                        },
                        {
                            0.0f,
                            0.0f,
                            1.0f
                        },
                        ScaleHybridColor(
                            WHITE,
                            0.82f
                        )
                    );
                }
            }

            // North edge.
            if (
                !IsRampConnectionBetweenCells(
                    x,
                    y,
                    x,
                    y - 1
                )
                )
            {
                const CornerHeights neighbour =
                    GetCornerHeights(x, y - 1);

                const float lowerNw =
                    std::min(current.nw, neighbour.sw);

                const float lowerNe =
                    std::min(current.ne, neighbour.se);

                if (
                    current.nw - lowerNw > wallEpsilon ||
                    current.ne - lowerNe > wallEpsilon
                    )
                {
                    const int northWallTile =
                        GetTerrainWallTileIndex(
                            x,
                            y,
                            TerrainWallFace::North
                        );

                    HybridMeshBuilder& northWallBuilder =
                        GetBuilder(
                            chamberId,
                            northWallTile
                        );

                    northWallBuilder.AddQuad(
                        nw,
                        ne,
                        {
                            x1,
                            lowerNe,
                            z0
                        },
                        {
                            x0,
                            lowerNw,
                            z0
                        },
                        {
                            0.0f,
                            0.0f,
                            -1.0f
                        },
                        ScaleHybridColor(
                            WHITE,
                            0.74f
                        )
                    );
                }
            }
        }
    }

    bool builtAnyBatch = false;

    for (
        ChamberTerrainBuilder& builderEntry :
        builders
        )
    {
        const int tileIndex =
            builderEntry.tileIndex;

        Texture2D brushTexture =
            GetTileBrushTexture(
                tileIndex
            );

        if (
            brushTexture.id != 0 &&
            brushTexture.width > 0 &&
            brushTexture.height > 0
            )
        {
            const Rectangle source =
                GetTileBrushSourceRect(
                    tileIndex
                );

            const float u0 =
                source.x /
                static_cast<float>(
                    brushTexture.width
                    );

            const float v0 =
                source.y /
                static_cast<float>(
                    brushTexture.height
                    );

            const float u1 =
                (
                    source.x +
                    source.width
                    ) /
                static_cast<float>(
                    brushTexture.width
                    );

            const float v1 =
                (
                    source.y +
                    source.height
                    ) /
                static_cast<float>(
                    brushTexture.height
                    );

            builderEntry.meshBuilder.RemapUv(
                u0,
                v0,
                u1,
                v1
            );
        }

        Mesh mesh =
            builderEntry.meshBuilder.BuildMesh();

        if (mesh.vertexCount <= 0)
        {
            continue;
        }

        HybridTerrainBatch batch;

        batch.mesh =
            mesh;

        batch.model =
            LoadModelFromMesh(
                mesh
            );

        batch.chamberId =
            builderEntry.chamberId;

        batch.tileIndex =
            tileIndex;

        batch.ready =
            batch.model.meshCount > 0;

        if (!batch.ready)
        {
            continue;
        }

        if (
            hybridTerrainShaderLoaded &&
            batch.model.materialCount > 0
            )
        {
            batch.model.materials[0].shader =
                hybridTerrainShader;
        }

        Texture2D batchTexture =
            GetTileBrushTexture(
                tileIndex
            );

        if (
            batchTexture.id != 0 &&
            batch.model.materialCount > 0
            )
        {
            batch.model.materials[0]
                .maps[MATERIAL_MAP_ALBEDO]
                .texture =
                batchTexture;
        }

        hybridTerrainBatches.push_back(
            std::move(
                batch
            )
        );

        builtAnyBatch =
            true;
    }

    hybridTerrainDirty = false;
    hybridTerrainReady = builtAnyBatch;

    TraceLog(
        LOG_INFO,
        "[HYBRID3D] Terrain rebuilt | ready=%d batches=%d",
        hybridTerrainReady ? 1 : 0,
        static_cast<int>(hybridTerrainBatches.size())
    );
}

float Game::GetHybridOrthoSizeFromSharedZoom() const
{
    // In the legacy renderer, one 64 px tile is scaled by camera.zoom.
    // In an orthographic Camera3D, fovy is the visible vertical size in
    // 3D world units. Since one tile equals one hybrid unit, this mapping
    // keeps the on-screen tile scale approximately identical in both modes.
    const float safeZoom =
        std::max(
            0.05f,
            camera.zoom
        );

    const float viewportHeight =
        std::max(
            1.0f,
            static_cast<float>(
                GetScreenHeight()
                )
        );

    const float orthoSize =
        viewportHeight /
        (
            TileSize *
            safeZoom
            );

    return
        Clamp(
            orthoSize,
            2.0f,
            80.0f
        );
}

Vector2 Game::ViewDirectionToWorldDirection(
    Vector2 viewDirection
) const
{
    if (Vector2Length(viewDirection) <= 0.001f)
    {
        return {
            0.0f,
            0.0f
        };
    }

    if (rendererMode == WorldRendererMode::Hybrid3D)
    {
        // Convert a screen-relative direction into a direction along the
        // horizontal X/Z gameplay plane. This is direction conversion only;
        // unlike ViewToWorld(), it does not perform a mouse ray cast.
        Vector3 cameraForward =
            Vector3Normalize(
                Vector3Subtract(
                    hybridCamera.target,
                    hybridCamera.position
                )
            );

        Vector3 flatForward{
            cameraForward.x,
            0.0f,
            cameraForward.z
        };

        if (Vector3Length(flatForward) <= 0.001f)
        {
            flatForward = {
                -1.0f,
                0.0f,
                -1.0f
            };
        }
        else
        {
            flatForward =
                Vector3Normalize(
                    flatForward
                );
        }

        Vector3 screenRight =
            Vector3CrossProduct(
                cameraForward,
                hybridCamera.up
            );

        screenRight.y = 0.0f;

        if (Vector3Length(screenRight) <= 0.001f)
        {
            screenRight = {
                1.0f,
                0.0f,
                -1.0f
            };
        }
        else
        {
            screenRight =
                Vector3Normalize(
                    screenRight
                );
        }

        // Screen Y increases downward, while flatForward points toward the
        // top of the screen / farther into the isometric scene.
        Vector3 movement3D =
            Vector3Add(
                Vector3Scale(
                    screenRight,
                    viewDirection.x
                ),
                Vector3Scale(
                    flatForward,
                    -viewDirection.y
                )
            );

        Vector2 worldDirection{
            movement3D.x,
            movement3D.z
        };

        if (Vector2Length(worldDirection) <= 0.001f)
        {
            return {
                0.0f,
                0.0f
            };
        }

        return
            Vector2Normalize(
                worldDirection
            );
    }

    if (!useIsometricView)
    {
        return
            Vector2Normalize(
                viewDirection
            );
    }

    // The legacy isometric projection is linear, so its inverse can be used
    // directly for a direction vector.
    constexpr float diagonalScale = 0.70710678118f;

    const float difference =
        viewDirection.x /
        diagonalScale;

    const float sum =
        viewDirection.y /
        (
            diagonalScale *
            isoVerticalScale
            );

    Vector2 worldDirection{
        (
            difference +
            sum
        ) *
            0.5f,

        (
            sum -
            difference
        ) *
            0.5f
    };

    if (Vector2Length(worldDirection) <= 0.001f)
    {
        return {
            0.0f,
            0.0f
        };
    }

    return
        Vector2Normalize(
            worldDirection
        );
}

void Game::UpdateHybridCamera(float dt)
{
#if MOXIANG_USE_IMGUI
    const bool editorOwnsCamera =
        buildMode;
#else
    const bool editorOwnsCamera = false;
#endif

    if (!editorOwnsCamera)
    {
        float amount =
            dt <= 0.0f
            ? 1.0f
            : Clamp(
                hybridCameraFollowSpeed * dt,
                0.0f,
                1.0f
            );

        hybridCameraTargetWorld =
            Vector2Lerp(
                hybridCameraTargetWorld,
                playerPosition,
                amount
            );
    }

    Vector3 target{
        hybridCameraTargetWorld.x *
            hybridUnitsPerPixel,

        GetHybridTerrainHeightUnits(
            hybridCameraTargetWorld
        ) * 0.45f,

        hybridCameraTargetWorld.y *
            hybridUnitsPerPixel
    };

    // Match the Hybrid3D camera pitch to the vertical compression used by
    // the legacy isometric projection. With equal X/Z camera components,
    // this produces nearly the same diamond shape in both renderers.
    const float verticalScale =
        Clamp(
            isoVerticalScale,
            0.10f,
            0.95f
        );

    const float cameraHeightRatio =
        sqrtf(
            (
                2.0f *
                verticalScale *
                verticalScale
                ) /
            std::max(
                0.001f,
                1.0f -
                verticalScale *
                verticalScale
            )
        );

    Vector3 direction =
        Vector3Normalize(
            Vector3{
                1.0f,
                cameraHeightRatio,
                1.0f
            }
        );

    hybridCamera.target = target;
    hybridCamera.position =
        Vector3Add(
            target,
            Vector3Scale(
                direction,
                hybridCameraDistance
            )
        );

    hybridCamera.up = {
        0.0f,
        1.0f,
        0.0f
    };

    hybridCamera.fovy =
        hybridCameraOrthoSize;

    hybridCamera.projection =
        CAMERA_ORTHOGRAPHIC;

#if MOXIANG_USE_IMGUI
    const bool allowScreenShake =
        !buildMode;
#else
    const bool allowScreenShake =
        true;
#endif

    if (
        allowScreenShake &&
        (
            fabsf(
                huashanScreenShakeOffset.x
            ) >
            0.001f ||
            fabsf(
                huashanScreenShakeOffset.y
            ) >
            0.001f
            )
        )
    {
        Vector3 forward =
            Vector3Normalize(
                Vector3Subtract(
                    hybridCamera.target,
                    hybridCamera.position
                )
            );

        Vector3 screenRight =
            Vector3CrossProduct(
                forward,
                hybridCamera.up
            );

        if (
            Vector3Length(
                screenRight
            ) >
            0.001f
            )
        {
            screenRight =
                Vector3Normalize(
                    screenRight
                );
        }

        Vector3 screenUp =
            Vector3CrossProduct(
                screenRight,
                forward
            );

        if (
            Vector3Length(
                screenUp
            ) >
            0.001f
            )
        {
            screenUp =
                Vector3Normalize(
                    screenUp
                );
        }

        // For an orthographic camera, fovy represents
        // the visible vertical size in world units.
        float worldUnitsPerPixel =
            hybridCamera.fovy /
            std::max(
                1.0f,
                static_cast<float>(
                    GetScreenHeight()
                    )
            );

        Vector3 shakeOffset =
            Vector3Add(
                Vector3Scale(
                    screenRight,
                    -huashanScreenShakeOffset.x *
                    worldUnitsPerPixel
                ),
                Vector3Scale(
                    screenUp,
                    huashanScreenShakeOffset.y *
                    worldUnitsPerPixel
                )
            );

        // Move both so camera direction remains unchanged.
        hybridCamera.position =
            Vector3Add(
                hybridCamera.position,
                shakeOffset
            );

        hybridCamera.target =
            Vector3Add(
                hybridCamera.target,
                shakeOffset
            );
    }

}

void Game::UpdateHybridEditorCameraControls()
{
#if MOXIANG_USE_IMGUI
    if (ImGui::GetIO().WantCaptureMouse)
    {
        return;
    }
#endif

    const float wheel =
        GetMouseWheelMove();

    if (wheel != 0.0f)
    {
        const Vector2 mousePosition =
            GetMousePosition();

        Vector2 worldBeforeZoom{};

        const bool hadWorldBeforeZoom =
            ScreenToTerrainWorld3D(
                mousePosition,
                worldBeforeZoom
            );

        // Use exactly the same zoom value and increment as the legacy
        // Camera2D editor controls. UpdateHybridCamera() derives the
        // orthographic size from this shared value.
        camera.zoom +=
            wheel *
            0.10f;

        camera.zoom =
            Clamp(
                camera.zoom,
                0.25f,
                4.0f
            );

        UpdateHybridCamera(0.0f);

        Vector2 worldAfterZoom{};

        if (
            hadWorldBeforeZoom &&
            ScreenToTerrainWorld3D(
                mousePosition,
                worldAfterZoom
            )
            )
        {
            // Keep the terrain point below the cursor stationary while
            // zooming, matching the legacy editor camera behaviour.
            const Vector2 correction =
                Vector2Subtract(
                    worldBeforeZoom,
                    worldAfterZoom
                );

            hybridCameraTargetWorld =
                Vector2Add(
                    hybridCameraTargetWorld,
                    correction
                );

            UpdateHybridCamera(0.0f);
        }
    }

    if (
        IsMouseButtonDown(MOUSE_BUTTON_RIGHT) ||
        IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)
        )
    {
        const Vector2 mouseDelta =
            GetMouseDelta();

        Vector3 forward =
            Vector3Normalize(
                Vector3Subtract(
                    hybridCamera.target,
                    hybridCamera.position
                )
            );

        Vector3 right =
            Vector3Normalize(
                Vector3CrossProduct(
                    forward,
                    hybridCamera.up
                )
            );

        Vector3 flatForward{
            forward.x,
            0.0f,
            forward.z
        };

        if (Vector3Length(flatForward) > 0.001f)
        {
            flatForward =
                Vector3Normalize(flatForward);
        }

        const float unitsPerPixel =
            hybridCameraOrthoSize /
            std::max(
                1.0f,
                static_cast<float>(
                    GetScreenHeight()
                    )
            );

        Vector3 movement =
            Vector3Add(
                Vector3Scale(
                    right,
                    -mouseDelta.x *
                    unitsPerPixel
                ),
                Vector3Scale(
                    flatForward,
                    mouseDelta.y *
                    unitsPerPixel
                )
            );

        hybridCameraTargetWorld.x +=
            movement.x /
            hybridUnitsPerPixel;

        hybridCameraTargetWorld.y +=
            movement.z /
            hybridUnitsPerPixel;
    }

    UpdateHybridCamera(0.0f);
}

bool Game::ScreenToTerrainWall3D(
    Vector2 screenPosition,
    int& outCellX,
    int& outCellY,
    TerrainWallFace& outFace
) const
{
    outCellX = -1;
    outCellY = -1;
    outFace =
        TerrainWallFace::None;

    if (
        rendererMode !=
        WorldRendererMode::Hybrid3D
        )
    {
        return false;
    }

    const Ray ray =
        GetScreenToWorldRay(
            screenPosition,
            hybridCamera
        );

    const float originX =
        -static_cast<float>(
            MapWidth
            ) *
        TileSize *
        0.5f;

    const float originY =
        -static_cast<float>(
            MapHeight
            ) *
        TileSize *
        0.5f;

    const float levelHeight =
        PixelsToHybridUnits(
            terrainElevationStep
        );

    struct WallCornerHeights
    {
        float nw = 0.0f;
        float ne = 0.0f;
        float se = 0.0f;
        float sw = 0.0f;
    };

    auto GetCornerHeights =
        [
            this,
            levelHeight
        ](
            int cellX,
            int cellY
            )
        {
            WallCornerHeights heights{};

            if (
                !IsCellInside(
                    cellX,
                    cellY
                ) ||
                !IsCellEnabled(
                    cellX,
                    cellY
                )
                )
            {
                return heights;
            }

            const TerrainCell& cell =
                terrainCells[
                    CellIndex(
                        cellX,
                        cellY
                    )
                ];

            const int elevation =
                GetTerrainElevation(
                    cellX,
                    cellY
                );

            const float baseHeight =
                static_cast<float>(
                    elevation
                    ) *
                levelHeight;

            heights.nw = baseHeight;
            heights.ne = baseHeight;
            heights.se = baseHeight;
            heights.sw = baseHeight;

            int offsetX = 0;
            int offsetY = 0;

            if (
                !GetRampDirectionOffset(
                    cell.rampDirection,
                    offsetX,
                    offsetY
                )
                )
            {
                return heights;
            }

            const int targetX =
                cellX +
                offsetX;

            const int targetY =
                cellY +
                offsetY;

            if (
                !IsCellInside(
                    targetX,
                    targetY
                ) ||
                !IsCellEnabled(
                    targetX,
                    targetY
                ) ||
                GetTerrainElevation(
                    targetX,
                    targetY
                ) !=
                elevation + 1
                )
            {
                return heights;
            }

            const float highHeight =
                baseHeight +
                levelHeight;

            switch (cell.rampDirection)
            {
            case RampDirection::North:
                heights.nw = highHeight;
                heights.ne = highHeight;
                break;

            case RampDirection::East:
                heights.ne = highHeight;
                heights.se = highHeight;
                break;

            case RampDirection::South:
                heights.sw = highHeight;
                heights.se = highHeight;
                break;

            case RampDirection::West:
                heights.nw = highHeight;
                heights.sw = highHeight;
                break;

            case RampDirection::None:
            default:
                break;
            }

            return heights;
        };

    float nearestDistance =
        std::numeric_limits<float>::max();

    int nearestCellX = -1;
    int nearestCellY = -1;

    TerrainWallFace nearestFace =
        TerrainWallFace::None;

    auto TestTriangle =
        [&](
            Vector3 pointA,
            Vector3 pointB,
            Vector3 pointC,
            int cellX,
            int cellY,
            TerrainWallFace face
            )
        {
            RayCollision collision =
                GetRayCollisionTriangle(
                    ray,
                    pointA,
                    pointB,
                    pointC
                );

            // Test the reverse winding as well so wall picking
            // works from either side.
            if (!collision.hit)
            {
                collision =
                    GetRayCollisionTriangle(
                        ray,
                        pointA,
                        pointC,
                        pointB
                    );
            }

            if (
                !collision.hit ||
                collision.distance >=
                nearestDistance
                )
            {
                return;
            }

            nearestDistance =
                collision.distance;

            nearestCellX =
                cellX;

            nearestCellY =
                cellY;

            nearestFace =
                face;
        };

    auto TestQuad =
        [&](
            Vector3 point0,
            Vector3 point1,
            Vector3 point2,
            Vector3 point3,
            int cellX,
            int cellY,
            TerrainWallFace face
            )
        {
            TestTriangle(
                point0,
                point1,
                point2,
                cellX,
                cellY,
                face
            );

            TestTriangle(
                point0,
                point2,
                point3,
                cellX,
                cellY,
                face
            );
        };

    constexpr float wallEpsilon =
        0.0001f;

    for (int y = 0; y < MapHeight; ++y)
    {
        for (int x = 0; x < MapWidth; ++x)
        {
            if (
                !IsCellEnabled(
                    x,
                    y
                )
                )
            {
                continue;
            }

            const WallCornerHeights current =
                GetCornerHeights(
                    x,
                    y
                );

            const float x0 =
                (
                    originX +
                    static_cast<float>(
                        x
                        ) *
                    TileSize
                    ) *
                hybridUnitsPerPixel;

            const float x1 =
                (
                    originX +
                    static_cast<float>(
                        x + 1
                        ) *
                    TileSize
                    ) *
                hybridUnitsPerPixel;

            const float z0 =
                (
                    originY +
                    static_cast<float>(
                        y
                        ) *
                    TileSize
                    ) *
                hybridUnitsPerPixel;

            const float z1 =
                (
                    originY +
                    static_cast<float>(
                        y + 1
                        ) *
                    TileSize
                    ) *
                hybridUnitsPerPixel;

            const Vector3 nw{
                x0,
                current.nw,
                z0
            };

            const Vector3 ne{
                x1,
                current.ne,
                z0
            };

            const Vector3 se{
                x1,
                current.se,
                z1
            };

            const Vector3 sw{
                x0,
                current.sw,
                z1
            };

            // East
            if (
                !IsRampConnectionBetweenCells(
                    x,
                    y,
                    x + 1,
                    y
                )
                )
            {
                const WallCornerHeights neighbour =
                    GetCornerHeights(
                        x + 1,
                        y
                    );

                const float lowerNe =
                    std::min(
                        current.ne,
                        neighbour.nw
                    );

                const float lowerSe =
                    std::min(
                        current.se,
                        neighbour.sw
                    );

                if (
                    current.ne - lowerNe >
                    wallEpsilon ||
                    current.se - lowerSe >
                    wallEpsilon
                    )
                {
                    TestQuad(
                        ne,
                        se,
                        {
                            x1,
                            lowerSe,
                            z1
                        },
                        {
                            x1,
                            lowerNe,
                            z0
                        },
                        x,
                        y,
                        TerrainWallFace::East
                    );
                }
            }

            // West
            if (
                !IsRampConnectionBetweenCells(
                    x,
                    y,
                    x - 1,
                    y
                )
                )
            {
                const WallCornerHeights neighbour =
                    GetCornerHeights(
                        x - 1,
                        y
                    );

                const float lowerNw =
                    std::min(
                        current.nw,
                        neighbour.ne
                    );

                const float lowerSw =
                    std::min(
                        current.sw,
                        neighbour.se
                    );

                if (
                    current.nw - lowerNw >
                    wallEpsilon ||
                    current.sw - lowerSw >
                    wallEpsilon
                    )
                {
                    TestQuad(
                        sw,
                        nw,
                        {
                            x0,
                            lowerNw,
                            z0
                        },
                        {
                            x0,
                            lowerSw,
                            z1
                        },
                        x,
                        y,
                        TerrainWallFace::West
                    );
                }
            }

            // South
            if (
                !IsRampConnectionBetweenCells(
                    x,
                    y,
                    x,
                    y + 1
                )
                )
            {
                const WallCornerHeights neighbour =
                    GetCornerHeights(
                        x,
                        y + 1
                    );

                const float lowerSw =
                    std::min(
                        current.sw,
                        neighbour.nw
                    );

                const float lowerSe =
                    std::min(
                        current.se,
                        neighbour.ne
                    );

                if (
                    current.sw - lowerSw >
                    wallEpsilon ||
                    current.se - lowerSe >
                    wallEpsilon
                    )
                {
                    TestQuad(
                        se,
                        sw,
                        {
                            x0,
                            lowerSw,
                            z1
                        },
                        {
                            x1,
                            lowerSe,
                            z1
                        },
                        x,
                        y,
                        TerrainWallFace::South
                    );
                }
            }

            // North
            if (
                !IsRampConnectionBetweenCells(
                    x,
                    y,
                    x,
                    y - 1
                )
                )
            {
                const WallCornerHeights neighbour =
                    GetCornerHeights(
                        x,
                        y - 1
                    );

                const float lowerNw =
                    std::min(
                        current.nw,
                        neighbour.sw
                    );

                const float lowerNe =
                    std::min(
                        current.ne,
                        neighbour.se
                    );

                if (
                    current.nw - lowerNw >
                    wallEpsilon ||
                    current.ne - lowerNe >
                    wallEpsilon
                    )
                {
                    TestQuad(
                        nw,
                        ne,
                        {
                            x1,
                            lowerNe,
                            z0
                        },
                        {
                            x0,
                            lowerNw,
                            z0
                        },
                        x,
                        y,
                        TerrainWallFace::North
                    );
                }
            }
        }
    }

    if (
        nearestCellX < 0 ||
        nearestFace ==
        TerrainWallFace::None
        )
    {
        return false;
    }

    outCellX =
        nearestCellX;

    outCellY =
        nearestCellY;

    outFace =
        nearestFace;

    return true;
}

bool Game::ScreenToTerrainWorld3D(
    Vector2 screenPosition,
    Vector2& outWorldPosition,
    int* outCellX,
    int* outCellY
) const
{
    Ray ray =
        GetScreenToWorldRay(
            screenPosition,
            hybridCamera
        );

    const float originX =
        -static_cast<float>(MapWidth) *
        TileSize *
        0.5f;

    const float originY =
        -static_cast<float>(MapHeight) *
        TileSize *
        0.5f;

    const float levelHeight =
        PixelsToHybridUnits(
            terrainElevationStep
        );

    auto GetCornerHeights =
        [this, levelHeight](
            int cellX,
            int cellY,
            float& nw,
            float& ne,
            float& se,
            float& sw
            )
        {
            const int elevation =
                GetTerrainElevation(
                    cellX,
                    cellY
                );

            const float base =
                static_cast<float>(elevation) *
                levelHeight;

            nw = base;
            ne = base;
            se = base;
            sw = base;

            if (!IsCellInside(cellX, cellY))
            {
                return;
            }

            const TerrainCell& cell =
                terrainCells[
                    CellIndex(cellX, cellY)
                ];

            int offsetX = 0;
            int offsetY = 0;

            if (
                !GetRampDirectionOffset(
                    cell.rampDirection,
                    offsetX,
                    offsetY
                )
                )
            {
                return;
            }

            const int targetX = cellX + offsetX;
            const int targetY = cellY + offsetY;

            if (
                !IsCellInside(targetX, targetY) ||
                GetTerrainElevation(targetX, targetY) !=
                elevation + 1
                )
            {
                return;
            }

            const float high =
                base + levelHeight;

            switch (cell.rampDirection)
            {
            case RampDirection::North:
                nw = high;
                ne = high;
                break;

            case RampDirection::East:
                ne = high;
                se = high;
                break;

            case RampDirection::South:
                sw = high;
                se = high;
                break;

            case RampDirection::West:
                nw = high;
                sw = high;
                break;

            case RampDirection::None:
            default:
                break;
            }
        };

    float bestDistance =
        std::numeric_limits<float>::max();

    RayCollision bestCollision{};
    int bestCellX = -1;
    int bestCellY = -1;

    for (int y = 0; y < MapHeight; ++y)
    {
        for (int x = 0; x < MapWidth; ++x)
        {
            float hNw = 0.0f;
            float hNe = 0.0f;
            float hSe = 0.0f;
            float hSw = 0.0f;

            GetCornerHeights(
                x,
                y,
                hNw,
                hNe,
                hSe,
                hSw
            );

            const float x0 =
                (originX +
                    static_cast<float>(x) * TileSize) *
                hybridUnitsPerPixel;

            const float x1 =
                (originX +
                    static_cast<float>(x + 1) * TileSize) *
                hybridUnitsPerPixel;

            const float z0 =
                (originY +
                    static_cast<float>(y) * TileSize) *
                hybridUnitsPerPixel;

            const float z1 =
                (originY +
                    static_cast<float>(y + 1) * TileSize) *
                hybridUnitsPerPixel;

            const Vector3 nw{ x0, hNw, z0 };
            const Vector3 ne{ x1, hNe, z0 };
            const Vector3 se{ x1, hSe, z1 };
            const Vector3 sw{ x0, hSw, z1 };

            RayCollision collisionA =
                GetRayCollisionTriangle(
                    ray,
                    nw,
                    sw,
                    se
                );

            RayCollision collisionB =
                GetRayCollisionTriangle(
                    ray,
                    nw,
                    se,
                    ne
                );

            const RayCollision* candidate = nullptr;

            if (
                collisionA.hit &&
                collisionB.hit
                )
            {
                candidate =
                    collisionA.distance < collisionB.distance
                    ? &collisionA
                    : &collisionB;
            }
            else if (collisionA.hit)
            {
                candidate = &collisionA;
            }
            else if (collisionB.hit)
            {
                candidate = &collisionB;
            }

            if (
                candidate != nullptr &&
                candidate->distance < bestDistance
                )
            {
                bestDistance =
                    candidate->distance;

                bestCollision =
                    *candidate;

                bestCellX = x;
                bestCellY = y;
            }
        }
    }

    if (bestCellX >= 0)
    {
        outWorldPosition = {
            bestCollision.point.x /
                hybridUnitsPerPixel,
            bestCollision.point.z /
                hybridUnitsPerPixel
        };

        if (outCellX != nullptr)
        {
            *outCellX = bestCellX;
        }

        if (outCellY != nullptr)
        {
            *outCellY = bestCellY;
        }

        return true;
    }

    if (fabsf(ray.direction.y) <= 0.0001f)
    {
        return false;
    }

    const float t =
        -ray.position.y /
        ray.direction.y;

    if (t < 0.0f)
    {
        return false;
    }

    Vector3 point =
        Vector3Add(
            ray.position,
            Vector3Scale(
                ray.direction,
                t
            )
        );

    outWorldPosition = {
        point.x /
            hybridUnitsPerPixel,
        point.z /
            hybridUnitsPerPixel
    };

    int cellX = 0;
    int cellY = 0;

    if (
        WorldToCell(
            outWorldPosition,
            cellX,
            cellY
        )
        )
    {
        if (outCellX != nullptr) *outCellX = cellX;
        if (outCellY != nullptr) *outCellY = cellY;
        return true;
    }

    return false;
}
void Game::DrawHybridGroundEffects3D()
{
    // --------------------------------------------------
    // Lightweight contact shadows
    //
    // Depth testing remains active so terrain can hide shadows.
    // Depth writing is disabled so translucent shadows cannot
    // block the player, enemies, or other transparent effects.
    // --------------------------------------------------

    rlDisableDepthMask();

    BeginBlendMode(
        BLEND_ALPHA
    );

    // --------------------------------------------------
 // Player contact shadow
 //
 // The player is grounded, so the shadow should remain
 // tight beneath the visible feet.
 // --------------------------------------------------

    DrawHybridGroundShadow(
        playerPosition,
        playerRadius * 2.20f,
        playerRadius * 0.55f,
        Color{
            0,
            0,
            0,
            105
        }
    );

    // --------------------------------------------------
    // Enemy shadows
    //
    // Grunt and Runner use floating blob sprites, so they
    // keep the wider, softer floating shadow.
    //
    // Shooter, Tank and Boss use tighter contact shadows.
    // --------------------------------------------------

    constexpr bool drawEnemyShadows =
        true;

    constexpr int maxEnemyShadows =
        24;

    if (drawEnemyShadows)
    {
        int drawnEnemyShadows =
            0;

        for (
            const Enemy& enemy :
            enemies
            )
        {
            if (
                !enemy.active ||
                !IsChamberVisible(
                    enemy.chamberId
                ) ||
                IsEnemySpawnProtected(
                    enemy
                ) ||
                drawnEnemyShadows >=
                maxEnemyShadows
                )
            {
                continue;
            }

            const Vector2 screenPosition =
                GetWorldToScreen(
                    WorldToHybrid3D(
                        enemy.pos
                    ),
                    hybridCamera
                );

            constexpr float screenMargin =
                100.0f;

            if (
                screenPosition.x <
                -screenMargin ||
                screenPosition.y <
                -screenMargin ||
                screenPosition.x >
                static_cast<float>(
                    GetScreenWidth()
                    ) +
                screenMargin ||
                screenPosition.y >
                static_cast<float>(
                    GetScreenHeight()
                    ) +
                screenMargin
                )
            {
                continue;
            }

            const bool usesFloatingBlobShadow =
                ShouldUseBlobEnemySprite(
                    enemy
                );

            float shadowWidth =
                enemy.radius *
                1.80f;

            float shadowDepth =
                enemy.radius *
                0.48f;

            unsigned char shadowAlpha =
                92;

            if (usesFloatingBlobShadow)
            {
                // Wide, soft shadow communicates that the blob
                // is hovering above the terrain.
                shadowWidth =
                    enemy.radius *
                    2.40f;

                shadowDepth =
                    enemy.radius *
                    0.90f;

                shadowAlpha =
                    68;
            }
            else if (
                enemy.type ==
                EnemyType::Shooter
                )
            {
                // Tight contact shadow directly beneath the
                // archer's feet.
                shadowWidth =
                    enemy.radius *
                    1.75f;

                shadowDepth =
                    enemy.radius *
                    0.42f;

                shadowAlpha =
                    96;
            }
            else if (
                enemy.type ==
                EnemyType::Boss
                )
            {
                // Larger than ordinary grounded enemies, but
                // still a contact shadow rather than a floating one.
                shadowWidth =
                    enemy.radius *
                    2.05f;

                shadowDepth =
                    enemy.radius *
                    0.55f;

                shadowAlpha =
                    112;
            }
            else if (
                enemy.type ==
                EnemyType::Tank
                )
            {
                shadowWidth =
                    enemy.radius *
                    1.90f;

                shadowDepth =
                    enemy.radius *
                    0.55f;

                shadowAlpha =
                    100;
            }

            DrawHybridGroundShadow(
                enemy.pos,
                shadowWidth,
                shadowDepth,
                Color{
                    0,
                    0,
                    0,
                    shadowAlpha
                }
            );

            drawnEnemyShadows++;
        }
    }


    // Normal enemy spawning circles.
    DrawEnemySpawnGroundEffects3D();

    // Falling-rock danger circles must always draw,
    // regardless of whether Huashan is active.
    DrawBossFallingRockTelegraphsHybrid3D();

    if (
        huashanImpactSpriteLoaded &&
        (
            huashanImpactAnimationActive ||
            !huashanGroundMarks.empty()
            )
        )
    {
        rlDrawRenderBatchActive();

        rlDisableBackfaceCulling();
        rlDisableDepthMask();

        BeginBlendMode(
            BLEND_ALPHA
        );

        // Do not use the billboard shader here.
        // Its alpha-discard threshold can make the final
        // portion of the slow fade disappear suddenly.

        DrawHuashanImpactGround3D();

        EndBlendMode();

        rlDrawRenderBatchActive();

        rlEnableDepthMask();
        rlEnableBackfaceCulling();
    }


    // Obstacle shadows are disabled here by default.
    // Trees and large props can later use baked artwork shadows
    // or a separately capped shadow pass.
    constexpr bool drawObstacleShadows = false;

    if (drawObstacleShadows)
    {
        constexpr int maxObstacleShadows = 20;

        int drawnObstacleShadows = 0;

        for (const Obstacle& obstacle : obstacles)
        {
            if (
                !obstacle.castsShadow ||
                obstacle.heightLevel > 0 ||
                drawnObstacleShadows >=
                maxObstacleShadows
                )
            {
                continue;
            }

            DrawHybridGroundShadow(
                obstacle.position,
                obstacle.colliderSize.x * 1.10f,
                obstacle.colliderSize.y * 0.55f,
                Color{
                    0,
                    0,
                    0,
                    58
                }
            );

            drawnObstacleShadows++;
        }
    }

    EndBlendMode();

    rlEnableDepthMask();

    // --------------------------------------------------
    // Existing path and ground indicators
    // --------------------------------------------------

    if (
        hasPath &&
        pathIndex <
        static_cast<int>(
            currentPath.size()
            )
        )
    {
        for (
            int i = pathIndex;
            i <
            static_cast<int>(
                currentPath.size()
                ) -
            1;
            ++i
            )
        {
            DrawLine3D(
                WorldToHybrid3D(
                    currentPath[i],
                    2.0f
                ),
                WorldToHybrid3D(
                    currentPath[i + 1],
                    2.0f
                ),
                Color{
                    255,
                    255,
                    255,
                    110
                }
            );
        }
    }

    for (const NPC& npc : npcs)
    {
        DrawCircle3D(
            WorldToHybrid3D(
                npc.position,
                2.0f
            ),
            PixelsToHybridUnits(
                interactDistance
            ),
            {
                1.0f,
                0.0f,
                0.0f
            },
            90.0f,
            Color{
                255,
                255,
                255,
                60
            }
        );
    }

    if (huashanJumpActive)
    {
        DrawLine3D(
            WorldToHybrid3D(
                huashanJumpStart,
                3.0f
            ),
            WorldToHybrid3D(
                huashanJumpEnd,
                3.0f
            ),
            Color{
                255,
                80,
                55,
                150
            }
        );
    }

    if (dongfengCasting)
    {
        Vector2 end =
            Vector2Add(
                dongfengCastStart,
                Vector2Scale(
                    dongfengCastDirection,
                    dongfengRange
                )
            );

        DrawLine3D(
            WorldToHybrid3D(
                dongfengCastStart,
                4.0f
            ),
            WorldToHybrid3D(
                end,
                4.0f
            ),
            Color{
                120,
                220,
                255,
                130
            }
        );
    }

    for (const VfxParticle& particle : vfxParticles)
    {
        if (
            particle.active &&
            IsGroundVfx(
                particle.type
            )
            )
        {
            DrawHybridVfx3D(
                particle
            );
        }
    }
}

void Game::DrawHybridTerrain3D()
{
    if (hybridTerrainDirty)
    {
        RebuildHybridTerrain();
    }

    if (!hybridTerrainReady)
    {
        return;
    }

    for (const HybridTerrainBatch& batch : hybridTerrainBatches)
    {
        if (!batch.ready)
        {
            continue;
        }

        const float visibility =
            GetChamberVisibility(
                batch.chamberId
            );

        if (visibility <= 0.01f)
        {
            continue;
        }

        Color tint = WHITE;
        tint.a =
            static_cast<unsigned char>(
                255.0f * visibility
                );

        DrawModel(
            batch.model,
            { 0.0f, 0.0f, 0.0f },
            1.0f,
            tint
        );
    }
}

void Game::DrawHybridGroundDisc(
    Vector2 worldPosition,
    float radiusPixels,
    Color color,
    float additionalHeightPixels,
    int segments
) const
{
    if (
        radiusPixels <= 0.0f ||
        segments < 3
        )
    {
        return;
    }

    const Vector3 center =
        WorldToHybrid3D(
            worldPosition,
            additionalHeightPixels + 0.65f
        );

    const float radius =
        PixelsToHybridUnits(radiusPixels);

    if (hybridShadowTexture.id != 0)
    {
        const Vector3 nw{
            center.x - radius,
            center.y,
            center.z - radius
        };

        const Vector3 ne{
            center.x + radius,
            center.y,
            center.z - radius
        };

        const Vector3 se{
            center.x + radius,
            center.y,
            center.z + radius
        };

        const Vector3 sw{
            center.x - radius,
            center.y,
            center.z + radius
        };

        rlSetTexture(hybridShadowTexture.id);
        rlBegin(RL_QUADS);
        rlColor4ub(color.r, color.g, color.b, color.a);

        rlTexCoord2f(0.0f, 0.0f);
        rlVertex3f(nw.x, nw.y, nw.z);

        rlTexCoord2f(0.0f, 1.0f);
        rlVertex3f(sw.x, sw.y, sw.z);

        rlTexCoord2f(1.0f, 1.0f);
        rlVertex3f(se.x, se.y, se.z);

        rlTexCoord2f(1.0f, 0.0f);
        rlVertex3f(ne.x, ne.y, ne.z);

        rlEnd();
        rlSetTexture(0);
        return;
    }

    rlSetTexture(0);
    rlBegin(RL_TRIANGLES);
    rlColor4ub(color.r, color.g, color.b, color.a);

    for (int i = 0; i < segments; ++i)
    {
        const float angleA =
            static_cast<float>(i) /
            static_cast<float>(segments) *
            PI * 2.0f;

        const float angleB =
            static_cast<float>(i + 1) /
            static_cast<float>(segments) *
            PI * 2.0f;

        rlVertex3f(center.x, center.y, center.z);
        rlVertex3f(
            center.x + cosf(angleA) * radius,
            center.y,
            center.z + sinf(angleA) * radius
        );
        rlVertex3f(
            center.x + cosf(angleB) * radius,
            center.y,
            center.z + sinf(angleB) * radius
        );
    }

    rlEnd();
}


void Game::DrawHybridGroundShadow(
    Vector2 worldPosition,
    float widthPixels,
    float depthPixels,
    Color color,
    float additionalHeightPixels
) const
{
    if (
        widthPixels <= 0.0f ||
        depthPixels <= 0.0f
        )
    {
        return;
    }

    // Fallback if the soft radial texture was not created.
    if (hybridShadowTexture.id == 0)
    {
        DrawHybridGroundDisc(
            worldPosition,
            std::max(
                widthPixels,
                depthPixels
            ) * 0.5f,
            color,
            additionalHeightPixels
        );

        return;
    }

    // Slight lift prevents z-fighting with the terrain.
    const Vector3 center =
        WorldToHybrid3D(
            worldPosition,
            additionalHeightPixels + 0.85f
        );

    const float halfWidth =
        PixelsToHybridUnits(
            widthPixels * 0.5f
        );

    const float halfDepth =
        PixelsToHybridUnits(
            depthPixels * 0.5f
        );

    // Align the long axis with the camera's horizontal direction.
    // This makes the contact shadow look like a normal horizontal
    // sprite shadow instead of a diagonal world-space oval.
    Vector3 cameraForward =
        Vector3Normalize(
            Vector3Subtract(
                hybridCamera.target,
                hybridCamera.position
            )
        );

    Vector3 groundRight =
        Vector3CrossProduct(
            cameraForward,
            Vector3{
                0.0f,
                1.0f,
                0.0f
            }
        );

    if (
        Vector3Length(
            groundRight
        ) <= 0.0001f
        )
    {
        groundRight = {
            1.0f,
            0.0f,
            0.0f
        };
    }
    else
    {
        groundRight =
            Vector3Normalize(
                groundRight
            );
    }

    Vector3 groundForward{
        -groundRight.z,
        0.0f,
        groundRight.x
    };

    const Vector3 rightOffset =
        Vector3Scale(
            groundRight,
            halfWidth
        );

    const Vector3 depthOffset =
        Vector3Scale(
            groundForward,
            halfDepth
        );

    const Vector3 northWest =
        Vector3Subtract(
            Vector3Subtract(
                center,
                rightOffset
            ),
            depthOffset
        );

    const Vector3 southWest =
        Vector3Add(
            Vector3Subtract(
                center,
                rightOffset
            ),
            depthOffset
        );

    const Vector3 southEast =
        Vector3Add(
            Vector3Add(
                center,
                rightOffset
            ),
            depthOffset
        );

    const Vector3 northEast =
        Vector3Subtract(
            Vector3Add(
                center,
                rightOffset
            ),
            depthOffset
        );

    rlSetTexture(
        hybridShadowTexture.id
    );

    rlBegin(
        RL_QUADS
    );

    rlColor4ub(
        color.r,
        color.g,
        color.b,
        color.a
    );

    rlTexCoord2f(
        0.0f,
        0.0f
    );

    rlVertex3f(
        northWest.x,
        northWest.y,
        northWest.z
    );

    rlTexCoord2f(
        0.0f,
        1.0f
    );

    rlVertex3f(
        southWest.x,
        southWest.y,
        southWest.z
    );

    rlTexCoord2f(
        1.0f,
        1.0f
    );

    rlVertex3f(
        southEast.x,
        southEast.y,
        southEast.z
    );

    rlTexCoord2f(
        1.0f,
        0.0f
    );

    rlVertex3f(
        northEast.x,
        northEast.y,
        northEast.z
    );

    rlEnd();

    rlSetTexture(0);
}

void Game::DrawHybridBillboardFrame(
    Texture2D texture,
    Rectangle source,
    Vector2 worldPosition,
    float widthPixels,
    float heightPixels,
    float anchorY,
    float additionalHeightPixels,
    Color tint,
    HybridBillboardOrientation orientation,
    float whiteFlashAmount
) const
{
    if (
        texture.id == 0 ||
        texture.width <= 0 ||
        texture.height <= 0 ||
        widthPixels <= 0.0f ||
        heightPixels <= 0.0f
        )
    {
        return;
    }

    const float safeFlashAmount =
        Clamp(
            whiteFlashAmount,
            0.0f,
            1.0f
        );

    const bool isolateWhiteFlash =
        hybridBillboardShaderLoaded &&
        hybridBillboardFlashLocation >= 0 &&
        safeFlashAmount > 0.001f;

    if (isolateWhiteFlash)
    {
        // Render every previously queued normal sprite before
        // changing the uniform for this specific actor.
        rlDrawRenderBatchActive();

        SetShaderValue(
            hybridBillboardShader,
            hybridBillboardFlashLocation,
            &safeFlashAmount,
            SHADER_UNIFORM_FLOAT
        );
    }

    anchorY =
        Clamp(
            anchorY,
            0.0f,
            1.0f
        );

    const float width =
        PixelsToHybridUnits(
            widthPixels
        );

    float height =
        PixelsToHybridUnits(
            heightPixels
        );

    // Camera forward points from the camera toward its target.
    Vector3 cameraForward =
        Vector3Normalize(
            Vector3Subtract(
                hybridCamera.target,
                hybridCamera.position
            )
        );

    Vector3 billboardRight{};
    Vector3 billboardUp{};

    if (
        orientation ==
        HybridBillboardOrientation::FaceCamera
        )
    {
        // A full camera-facing billboard uses the camera's real screen
        // basis. This avoids the vertical foreshortening produced by an
        // upright/cylindrical billboard under a steep isometric camera.
        billboardRight =
            Vector3Normalize(
                Vector3CrossProduct(
                    cameraForward,
                    hybridCamera.up
                )
            );

        if (
            Vector3Length(
                billboardRight
            ) <= 0.0001f
            )
        {
            billboardRight = {
                1.0f,
                0.0f,
                0.0f
            };
        }

        billboardUp =
            Vector3Normalize(
                Vector3CrossProduct(
                    billboardRight,
                    cameraForward
                )
            );
    }
    else
    {
        // Cylindrical billboard:
        // stays vertically upright while rotating horizontally
        // to face the camera.
        billboardUp = {
            0.0f,
            1.0f,
            0.0f
        };

        Vector3 horizontalForward{
            cameraForward.x,
            0.0f,
            cameraForward.z
        };

        if (
            Vector3Length(
                horizontalForward
            ) <= 0.0001f
            )
        {
            horizontalForward = {
                0.0f,
                0.0f,
                -1.0f
            };
        }
        else
        {
            horizontalForward =
                Vector3Normalize(
                    horizontalForward
                );
        }

        billboardRight =
            Vector3Normalize(
                Vector3CrossProduct(
                    horizontalForward,
                    billboardUp
                )
            );

        // A world-upright sprite normally appears shorter when
        // viewed through the angled isometric camera.
        //
        // Increase its physical height to preserve the original
        // sprite-sheet proportions on screen.
        const float verticalProjectionScale =
            sqrtf(
                std::max(
                    0.0001f,
                    1.0f -
                    cameraForward.y *
                    cameraForward.y
                )
            );

        height /=
            std::max(
                0.35f,
                verticalProjectionScale
            );
    }

    // WorldToHybrid3D() returns the terrain contact point. anchorY is the
    // vertical location inside the sprite that should sit on that point.
    Vector3 center =
        WorldToHybrid3D(
            worldPosition,
            additionalHeightPixels
        );

    center =
        Vector3Add(
            center,
            Vector3Scale(
                billboardUp,
                (
                    anchorY -
                    0.5f
                    ) *
                height
            )
        );

    const Vector3 halfRight =
        Vector3Scale(
            billboardRight,
            width * 0.5f
        );

    const Vector3 halfUp =
        Vector3Scale(
            billboardUp,
            height * 0.5f
        );

    const Vector3 topLeft =
        Vector3Add(
            Vector3Subtract(
                center,
                halfRight
            ),
            halfUp
        );

    const Vector3 topRight =
        Vector3Add(
            Vector3Add(
                center,
                halfRight
            ),
            halfUp
        );

    const Vector3 bottomLeft =
        Vector3Subtract(
            Vector3Subtract(
                center,
                halfRight
            ),
            halfUp
        );

    const Vector3 bottomRight =
        Vector3Subtract(
            Vector3Add(
                center,
                halfRight
            ),
            halfUp
        );

    const float invTextureWidth =
        1.0f /
        static_cast<float>(
            texture.width
            );

    const float invTextureHeight =
        1.0f /
        static_cast<float>(
            texture.height
            );

    const float u0 =
        source.x *
        invTextureWidth;

    const float v0 =
        source.y *
        invTextureHeight;

    const float u1 =
        (
            source.x +
            source.width
            ) *
        invTextureWidth;

    const float v1 =
        (
            source.y +
            source.height
            ) *
        invTextureHeight;

    rlSetTexture(
        texture.id
    );

    rlBegin(
        RL_TRIANGLES
    );

    rlColor4ub(
        tint.r,
        tint.g,
        tint.b,
        tint.a
    );

    // Front face points toward the camera. Two explicit triangles are
    // used for reliable WebGL 1 support.
    rlTexCoord2f(u0, v0);
    rlVertex3f(
        topLeft.x,
        topLeft.y,
        topLeft.z
    );

    rlTexCoord2f(u0, v1);
    rlVertex3f(
        bottomLeft.x,
        bottomLeft.y,
        bottomLeft.z
    );

    rlTexCoord2f(u1, v1);
    rlVertex3f(
        bottomRight.x,
        bottomRight.y,
        bottomRight.z
    );

    rlTexCoord2f(u0, v0);
    rlVertex3f(
        topLeft.x,
        topLeft.y,
        topLeft.z
    );

    rlTexCoord2f(u1, v1);
    rlVertex3f(
        bottomRight.x,
        bottomRight.y,
        bottomRight.z
    );

    rlTexCoord2f(u1, v0);
    rlVertex3f(
        topRight.x,
        topRight.y,
        topRight.z
    );

    rlEnd();
    rlSetTexture(0);

    if (isolateWhiteFlash)
    {
        // Render this actor immediately while its own flash
        // value is still active.
        rlDrawRenderBatchActive();

        // Return the shader to normal before another sprite
        // is queued.
        const float noFlash =
            0.0f;

        SetShaderValue(
            hybridBillboardShader,
            hybridBillboardFlashLocation,
            &noFlash,
            SHADER_UNIFORM_FLOAT
        );
    }
}

bool Game::GetActivePlayerFrame(
    Texture2D& outTexture,
    Rectangle& outSource,
    float& outWidthPixels,
    float& outHeightPixels,
    float& outAnchorY
) const
{
    outTexture = {};
    outSource = {};
    outWidthPixels = 0.0f;
    outHeightPixels = 0.0f;

    // Keep the billboard above the terrain.
    outAnchorY =
        0.98f;

    int frameWidth =
        playerFrameWidth;

    int frameHeight =
        playerFrameHeight;

    int framesPerRow =
        playerFramesPerRow;

    int frame =
        playerAnimFrame;

    const bool drawAttack =
        playerAnimationState ==
        PlayerAnimationState::Attacking &&
        playerAttackSpriteLoaded &&
        playerAttackSpriteSheet.id !=
        0;

    const bool drawIdle =
        playerAnimationState ==
        PlayerAnimationState::Idle &&
        playerIdleSpriteLoaded &&
        playerIdleSpriteSheet.id !=
        0;

    if (drawAttack)
    {
        outTexture =
            playerAttackSpriteSheet;

        frameWidth =
            playerAttackFrameWidth;

        frameHeight =
            playerAttackFrameHeight;

        framesPerRow =
            playerAttackFramesPerRow;
    }
    else if (drawIdle)
    {
        outTexture =
            playerIdleSpriteSheet;

        frameWidth =
            playerIdleFrameWidth;

        frameHeight =
            playerIdleFrameHeight;

        framesPerRow =
            playerIdleFramesPerRow;
    }
    else if (
        playerSpriteLoaded &&
        playerSpriteSheet.id !=
        0
        )
    {
        outTexture =
            playerSpriteSheet;

        if (
            playerAnimationState ==
            PlayerAnimationState::Idle ||
            playerAnimationState ==
            PlayerAnimationState::Attacking
            )
        {
            frame =
                0;
        }
    }
    else
    {
        return false;
    }

    frame =
        std::max(
            0,
            std::min(
                frame,
                framesPerRow - 1
            )
        );

    const int row =
        GetPlayerDirectionRow(
            playerDirection
        );

    // Remove transparent space below the player's feet.
    const float bottomTrim =
        Clamp(
            playerSpriteBottomTrim,
            0.0f,
            static_cast<float>(
                frameHeight - 1
                )
        );

    outSource = {
        static_cast<float>(
            frame *
            frameWidth
        ),

        static_cast<float>(
            row *
            frameHeight
        ),

        static_cast<float>(
            frameWidth
        ),

        static_cast<float>(
            frameHeight
        ) -
        bottomTrim
    };

    const float drawScale =
        playerSpriteDrawScale;

    outWidthPixels =
        static_cast<float>(
            frameWidth
            ) *
        drawScale;

    // Use the cropped source height so the sprite does not
    // become vertically stretched.
    outHeightPixels =
        outSource.height *
        drawScale;

    return true;
}

void Game::DrawHybridPlayer3D()
{
    Texture2D texture{};
    Rectangle source{};
    float width = 0.0f;
    float height = 0.0f;
    float anchorY = 0.84f;

    const float jumpHeight =
        GetPlayerVisualHeight();

    if (
        GetActivePlayerFrame(
            texture,
            source,
            width,
            height,
            anchorY
        )
        )
    {

        const float playerWhiteFlash =
            playerDamageFlashTimer > 0.0f
            ? 1.0f
            : 0.0f;

        DrawHybridBillboardFrame(
            texture,
            source,
            playerPosition,
            width,
            height,
            anchorY,
            jumpHeight,
            WHITE,
            HybridBillboardOrientation::UprightWorld,
            playerWhiteFlash
        );
    }
    else if (hybridCircleTexture.id != 0)
    {
        const float playerWhiteFlash =
            playerDamageFlashTimer > 0.0f
            ? 1.0f
            : 0.0f;

        DrawHybridBillboardFrame(
            hybridCircleTexture,
            {
                0.0f,
                0.0f,
                static_cast<float>(
                    hybridCircleTexture.width
                ),
                static_cast<float>(
                    hybridCircleTexture.height
                )
            },
            playerPosition,
            playerRadius * 2.0f,
            playerRadius * 2.0f,
            1.0f,
            jumpHeight,
            Color{
                78,
                148,
                255,
                255
            },
            HybridBillboardOrientation::FaceCamera,
            playerWhiteFlash
        );
    }
}

void Game::DrawHybridDashAfterimage3D(
    const DashAfterimage& afterimage
)
{
    if (afterimage.maxLife <= 0.0f)
    {
        return;
    }

    Texture2D activeTexture{};

    int frameWidth = 0;
    int frameHeight = 0;
    int framesPerRow = 1;
    int directionRows = 8;

    float drawScale = 1.0f;
    float anchorY =
        0.98f;
    // --------------------------------------------------
    // Choose Player or Boss sprite
    // --------------------------------------------------

    if (afterimage.useBossSprite)
    {
        if (
            bossWalkSpriteLoaded &&
            bossWalkSpriteSheet.id != 0
            )
        {
            activeTexture =
                bossWalkSpriteSheet;

            framesPerRow =
                bossWalkFramesPerRow;
        }
        else if (
            bossIdleSpriteLoaded &&
            bossIdleSpriteSheet.id != 0
            )
        {
            activeTexture =
                bossIdleSpriteSheet;

            framesPerRow =
                bossIdleFramesPerRow;
        }
        else
        {
            return;
        }

        frameWidth =
            bossFrameWidth;

        frameHeight =
            bossFrameHeight;

        directionRows =
            bossDirectionRows;

        drawScale =
            bossVisualScale;

        anchorY =
            0.98f;
    }
    else
    {
        if (
            !playerSpriteLoaded ||
            playerSpriteSheet.id == 0
            )
        {
            return;
        }

        activeTexture =
            playerSpriteSheet;

        frameWidth =
            playerFrameWidth;

        frameHeight =
            playerFrameHeight;

        framesPerRow =
            playerFramesPerRow;

        directionRows =
            playerDirectionRows;

        drawScale =
            playerSpriteDrawScale;

        anchorY =
            0.98f;
    }

    framesPerRow =
        std::max(
            1,
            framesPerRow
        );

    directionRows =
        std::max(
            1,
            directionRows
        );

    const float lifeRatio =
        Clamp(
            afterimage.life /
            afterimage.maxLife,
            0.0f,
            1.0f
        );

    const int frame =
        std::max(
            0,
            std::min(
                afterimage.frame,
                framesPerRow - 1
            )
        );

    int row =
        GetPlayerDirectionRow(
            afterimage.direction
        );

    row =
        std::max(
            0,
            std::min(
                row,
                directionRows - 1
            )
        );

    Rectangle source{
        static_cast<float>(
            frame *
            frameWidth
        ),

        static_cast<float>(
            row *
            frameHeight
        ),

        static_cast<float>(
            frameWidth
        ),

        static_cast<float>(
            frameHeight
        )
    };

    const float afterimageWidthPixels =
        static_cast<float>(
            frameWidth
            ) *
        drawScale;

    const float afterimageHeightPixels =
        static_cast<float>(
            frameHeight
            ) *
        drawScale;

    const float alpha =
        lifeRatio *
        lifeRatio *
        0.98f;

    if (
        dashAfterimageShaderLoaded &&
        dashAfterimageShader.id != 0 &&
        dashAfterimageColorLocation >= 0
        )
    {
        float outerColor[4]{
            0.25f +
                0.55f *
                lifeRatio,

            0.78f +
                0.22f *
                lifeRatio,

            1.0f,
            alpha
        };

        SetShaderValue(
            dashAfterimageShader,
            dashAfterimageColorLocation,
            outerColor,
            SHADER_UNIFORM_VEC4
        );

        DrawHybridBillboardFrame(
            activeTexture,
            source,
            afterimage.worldPosition,
            afterimageWidthPixels,
            afterimageHeightPixels,
            anchorY,
            0.0f,
            WHITE,
            HybridBillboardOrientation::UprightWorld
        );

        // Bright inner silhouette.
        float coreColor[4]{
            0.78f,
            0.96f,
            1.0f,
            alpha * 0.38f
        };

        SetShaderValue(
            dashAfterimageShader,
            dashAfterimageColorLocation,
            coreColor,
            SHADER_UNIFORM_VEC4
        );

        DrawHybridBillboardFrame(
            activeTexture,
            source,
            afterimage.worldPosition,
            afterimageWidthPixels,
            afterimageHeightPixels,
            anchorY,
            0.0f,
            WHITE,
            HybridBillboardOrientation::UprightWorld
        );
    }
    else
    {
        DrawHybridBillboardFrame(
            activeTexture,
            source,
            afterimage.worldPosition,
            afterimageWidthPixels,
            afterimageHeightPixels,
            anchorY,
            0.0f,
            Color{
                125,
                235,
                255,
                static_cast<unsigned char>(
                    245.0f *
                    alpha
                )
            },
            HybridBillboardOrientation::UprightWorld
        );
    }
}

void Game::DrawHybridEnemy3D(
    const Enemy& enemy
)
{
    if (
        !enemy.active ||
        enemy.spawnState ==
        EnemySpawnState::GroundEffect
        )
    {
        return;
    }

    const float enemySpawnVisualOffset =
        GetEnemySpawnVisualOffset(
            enemy
        );

    const float finalEnemyVisualHeight =
        enemy.visualHeight +
        enemySpawnVisualOffset;

    const float damageWhiteFlash =
        enemy.damageFlashTimer > 0.0f
        ? 1.0f
        : 0.0f;

    const float spawnWhiteFlash =
        GetEnemySpawnFlashAmount(
            enemy
        );

    const float enemyWhiteFlash =
        std::max(
            damageWhiteFlash,
            spawnWhiteFlash
        );

    // Textured enemies keep their authored colours.
    Color spriteTint =
        WHITE;

    Color fallbackTint =
        GREEN;

    if (enemy.type == EnemyType::Runner)
    {
        fallbackTint = LIME;
    }
    else if (enemy.type == EnemyType::Tank)
    {
        fallbackTint = DARKGREEN;
    }
    else if (enemy.type == EnemyType::Shooter)
    {
        fallbackTint = ORANGE;
    }
    else if (enemy.type == EnemyType::Boss)
    {
        fallbackTint =
            GetBossTierTint(
                enemy
            );
    }

    // Apply the persistent Boss phase tint before
   // temporary status-effect colours.
    if (
        enemy.type ==
        EnemyType::Boss
        )
    {
        spriteTint =
            GetBossTierTint(
                enemy
            );

        fallbackTint =
            spriteTint;
    }

    // --------------------------------------------------
    // Charge warning blink
    // --------------------------------------------------

    if (
        enemy.type ==
        EnemyType::Boss &&
        enemy.bossActionState ==
        BossActionState::ChargeWindup
        )
    {
        const int blinkPhase =
            static_cast<int>(
                GetTime() *
                12.0
                );

        if (
            blinkPhase %
            2 ==
            0
            )
        {
            spriteTint =
                WHITE;

            fallbackTint =
                WHITE;
        }
    }

    // Temporary status effects take priority over
    // the charge warning and HP-tier tint.
    if (enemy.frozenTimer > 0.0f)
    {
        spriteTint =
            Color{
                150,
                230,
                255,
                255
        };

        fallbackTint =
            spriteTint;
    }
    else if (
        enemy.stunTimer > 0.0f ||
        enemy.landingStunTimer > 0.0f
        )
    {
        spriteTint =
            Color{
                255,
                255,
                150,
                255
        };

        fallbackTint =
            spriteTint;
    }
    else if (
        enemy.slowTimer >
        0.0f
        )
    {
        spriteTint =
            Color{
                150,
                205,
                255,
                255
        };

        fallbackTint =
            spriteTint;
    }

    // --------------------------------------------------
   // Dedicated shooter sprite
   // --------------------------------------------------

    if (
        enemy.type ==
        EnemyType::Shooter
        )
    {
        Texture2D shooterTexture{};
        Rectangle shooterSource{};

        float shooterWidthPixels =
            0.0f;

        float shooterHeightPixels =
            0.0f;

        float shooterAnchorY =
            1.0f;

        if (
            GetShooterAnimationFrame(
                enemy,
                shooterTexture,
                shooterSource,
                shooterWidthPixels,
                shooterHeightPixels,
                shooterAnchorY
            )
            )
        {
            DrawHybridBillboardFrame(
                shooterTexture,
                shooterSource,
                enemy.pos,
                shooterWidthPixels,
                shooterHeightPixels,
                shooterAnchorY,

                // Use the spawning height instead of
                // the normal enemy height.
                finalEnemyVisualHeight,

                spriteTint,
                HybridBillboardOrientation::UprightWorld,
                enemyWhiteFlash
            );

            return;
        }
    }

    // --------------------------------------------------
    // Dedicated Boss sprite
    // --------------------------------------------------

    if (
        enemy.type ==
        EnemyType::Boss
        )
    {
        Texture2D bossTexture{};
        Rectangle bossSource{};

        float bossWidthPixels =
            0.0f;

        float bossHeightPixels =
            0.0f;

        float bossAnchorY =
            1.0f;

        if (
            GetBossAnimationFrame(
                enemy,
                bossTexture,
                bossSource,
                bossWidthPixels,
                bossHeightPixels,
                bossAnchorY
            )
            )
        {
            DrawHybridBillboardFrame(
                bossTexture,
                bossSource,
                enemy.pos,
                bossWidthPixels,
                bossHeightPixels,
                bossAnchorY,
                finalEnemyVisualHeight,
                spriteTint,
                HybridBillboardOrientation::UprightWorld,
                enemyWhiteFlash
            );

            return;
        }
    }

    // --------------------------------------------------
    // Small animated enemy sprite
    // --------------------------------------------------

    const bool useBlob =
        ShouldUseBlobEnemySprite(
            enemy
        );

    if (
        useBlob &&
        smallEnemySpriteLoaded &&
        smallEnemySpriteSheet.id !=
        0
        )
    {
        const int frame =
            std::max(
                0,
                std::min(
                    enemy.spriteFrame,
                    smallEnemyFramesPerRow -
                    1
                )
            );

        int row =
            GetPlayerDirectionRow(
                enemy.spriteDirection
            );

        row =
            std::max(
                0,
                std::min(
                    row,
                    smallEnemyDirectionRows -
                    1
                )
            );

        Rectangle source{
            static_cast<float>(
                frame *
                smallEnemyFrameWidth
            ),

            static_cast<float>(
                row *
                smallEnemyFrameHeight
            ),

            static_cast<float>(
                smallEnemyFrameWidth
            ),

            static_cast<float>(
                smallEnemyFrameHeight
            )
        };

        const float size =
            GetBlobEnemyVisualSize(
                enemy
            );

        DrawHybridBillboardFrame(
            smallEnemySpriteSheet,
            source,
            enemy.pos,
            size,
            size,
            0.98f,

            // This is the important replacement for
            // Grunt, Runner and other blob enemies.
            finalEnemyVisualHeight,

            spriteTint,
            HybridBillboardOrientation::FaceCamera,
            enemyWhiteFlash
        );
    }

    // --------------------------------------------------
    // Fallback enemy circle
    // --------------------------------------------------

    else if (
        hybridCircleTexture.id !=
        0
        )
    {
        DrawHybridBillboardFrame(
            hybridCircleTexture,
            {
                0.0f,
                0.0f,

                static_cast<float>(
                    hybridCircleTexture.width
                ),

                static_cast<float>(
                    hybridCircleTexture.height
                )
            },
            enemy.pos,
            enemy.radius *
            2.0f,
            enemy.radius *
            2.0f,
            1.0f,
            finalEnemyVisualHeight,
            fallbackTint,
            HybridBillboardOrientation::FaceCamera,
            enemyWhiteFlash
        );
    }
}

void Game::DrawHybridObstacle3D(
    const Obstacle& obstacle
)
{
    const float localHeight =
        static_cast<float>(
            std::max(
                0,
                obstacle.heightLevel
            )
            ) *
        obstacleHeightStep;

    // --------------------------------------------------
    // Textured obstacle
    // --------------------------------------------------

    if (
        obstacle.hasTexture &&
        obstacle.texture.id != 0
        )
    {
        DrawHybridBillboardFrame(
            obstacle.texture,
            {
                0.0f,
                0.0f,

                static_cast<float>(
                    obstacle.texture.width
                ),

                static_cast<float>(
                    obstacle.texture.height
                )
            },
            obstacle.position,
            obstacle.size.x,
            obstacle.size.y,
            1.0f,
            localHeight,
            WHITE
        );

        return;
    }

    // --------------------------------------------------
    // Fallback obstacle placeholder
    // --------------------------------------------------

    if (
        hybridCircleTexture.id ==
        0
        )
    {
        return;
    }

    const float size =
        std::max(
            obstacle.size.x,
            obstacle.size.y
        ) *
        0.5f;

    const Color fallbackColor =
        obstacle.type == 0
        ? Color{
            48,
            126,
            54,
            255
    }
        : Color{
            94,
            93,
            88,
            255
    };

    DrawHybridBillboardFrame(
        hybridCircleTexture,
        {
            0.0f,
            0.0f,

            static_cast<float>(
                hybridCircleTexture.width
            ),

            static_cast<float>(
                hybridCircleTexture.height
            )
        },
        obstacle.position,
        size,
        size,
        1.0f,
        localHeight,
        fallbackColor
    );
}

void Game::DrawHybridNpc3D(
    const NPC& npc
)
{
    if (hybridCircleTexture.id == 0)
    {
        return;
    }

    DrawHybridBillboardFrame(
        hybridCircleTexture,
        {
            0.0f,
            0.0f,
            static_cast<float>(hybridCircleTexture.width),
            static_cast<float>(hybridCircleTexture.height)
        },
        npc.position,
        npc.radius * 2.0f,
        npc.radius * 2.0f,
        1.0f,
        0.0f,
        Color{ 218, 184, 92, 255 }
    );
}

void Game::DrawHybridProjectile3D(
    const Projectile& projectile
)
{
    if (!projectile.active)
    {
        return;
    }

    Vector3 position{
        projectile.pos.x *
            hybridUnitsPerPixel,

        static_cast<float>(
            projectile.terrainElevation
        ) *
            PixelsToHybridUnits(
                terrainElevationStep
            ) +
            PixelsToHybridUnits(
                projectile.visualHeight
            ),

        projectile.pos.y *
            hybridUnitsPerPixel
    };

    DrawSphere(
        position,
        PixelsToHybridUnits(projectile.radius),
        projectile.owner == ProjectileOwner::Enemy
        ? Color{ 255, 80, 60, 255 }
        : YELLOW
    );
}

void Game::DrawHybridOrbitalBlade3D(
    const OrbitalBlade& blade
)
{
    if (!blade.active)
    {
        return;
    }

    Vector2 worldPosition{
        playerPosition.x +
            cosf(blade.angle) *
            blade.orbitRadius,
        playerPosition.y +
            sinf(blade.angle) *
            blade.orbitRadius
    };

    Vector3 position =
        WorldToHybrid3D(
            worldPosition,
            26.0f
        );

    DrawSphere(
        position,
        PixelsToHybridUnits(
            blade.bladeRadius * 0.38f
        ),
        Color{ 180, 235, 255, 230 }
    );
}

void Game::DrawHybridVfx3D(
    const VfxParticle& particle
)
{
    if (!particle.active)
    {
        return;
    }

    float lifeRatio = 1.0f;

    if (particle.maxLife > 0.0f)
    {
        lifeRatio =
            Clamp(
                particle.life /
                particle.maxLife,
                0.0f,
                1.0f
            );
    }

    Color color = particle.color;
    color.a =
        static_cast<unsigned char>(
            static_cast<float>(color.a) *
            lifeRatio
            );

    if (
        particle.type == VfxType::LightningLine ||
        particle.type == VfxType::SlashLine
        )
    {
        DrawLine3D(
            WorldToHybrid3D(
                particle.pos,
                28.0f
            ),
            WorldToHybrid3D(
                particle.endPos,
                28.0f
            ),
            color
        );

        return;
    }

    if (particle.type == VfxType::FloatingDamage)
    {
        return;
    }

    Vector3 position =
        WorldToHybrid3D(
            particle.pos,
            particle.type == VfxType::SkillCircle
            ? 1.0f
            : 18.0f
        );

    if (particle.type == VfxType::SkillCircle)
    {
        DrawCircle3D(
            position,
            PixelsToHybridUnits(particle.radius),
            { 1.0f, 0.0f, 0.0f },
            90.0f,
            color
        );
    }
    else
    {
        DrawSphere(
            position,
            PixelsToHybridUnits(
                std::max(2.0f, particle.radius)
            ),
            color
        );
    }
}

void Game::DrawHybridDongfeng3D()
{
    if (!dongfengWaveActive)
    {
        return;
    }

    const float radius =
        GetDongfengWaveRadius();

    Vector3 position =
        WorldToHybrid3D(
            dongfengWavePos,
            20.0f
        );

    DrawSphereWires(
        position,
        PixelsToHybridUnits(radius * 0.22f),
        10,
        10,
        Color{ 140, 220, 255, 220 }
    );
}



void Game::DrawHybridActors3D()
{
    // --------------------------------------------------
    // Opaque / alpha-tested billboard pass
    // --------------------------------------------------

    if (hybridBillboardShaderLoaded)
    {
        BeginShaderMode(
            hybridBillboardShader
        );

        // Always begin the actor pass with no damage flash.
        if (hybridBillboardFlashLocation >= 0)
        {
            const float noFlash =
                0.0f;

            SetShaderValue(
                hybridBillboardShader,
                hybridBillboardFlashLocation,
                &noFlash,
                SHADER_UNIFORM_FLOAT
            );
        }
    }

    for (const Obstacle& obstacle : obstacles)
    {
        if (
            !IsWorldPositionInVisibleChamber(
                obstacle.position
            )
            )
        {
            continue;
        }

        DrawHybridObstacle3D(
            obstacle
        );
    }

    for (const NPC& npc : npcs)
    {
        if (
            !IsWorldPositionInVisibleChamber(
                npc.position
            )
            )
        {
            continue;
        }

        DrawHybridNpc3D(
            npc
        );
    }

    for (const Enemy& enemy : enemies)
    {
        if (
            !IsChamberVisible(
                enemy.chamberId
            )
            )
        {
            continue;
        }

        DrawHybridEnemy3D(
            enemy
        );
    }

    DrawHybridPlayer3D();

    if (hybridBillboardShaderLoaded)
    {
        EndShaderMode();
    }
    DrawBossFallingRocksHybrid3D();
    DrawBossLasersHybrid3D();

    // --------------------------------------------------
    // Bright transparent dash pass
    //
    // Depth testing stays enabled so terrain and actors can
    // correctly cover afterimages.
    //
    // Depth writing is disabled so multiple transparent
    // afterimages can accumulate without blocking each other.
    // --------------------------------------------------

    if (!dashAfterimages.empty())
    {
        rlDisableDepthMask();

        BeginBlendMode(
            BLEND_ADDITIVE
        );

        const bool useDashShader =
            dashAfterimageShaderLoaded &&
            dashAfterimageShader.id != 0 &&
            dashAfterimageColorLocation >= 0;

        if (useDashShader)
        {
            BeginShaderMode(
                dashAfterimageShader
            );
        }
        else if (hybridBillboardShaderLoaded)
        {
            BeginShaderMode(
                hybridBillboardShader
            );
        }

        for (
            const DashAfterimage& afterimage :
            dashAfterimages
            )
        {
            if (
                !IsWorldPositionInVisibleChamber(
                    afterimage.worldPosition
                )
                )
            {
                continue;
            }

            DrawHybridDashAfterimage3D(
                afterimage
            );
        }

        if (useDashShader)
        {
            EndShaderMode();
        }
        else if (hybridBillboardShaderLoaded)
        {
            EndShaderMode();
        }

        EndBlendMode();

        rlEnableDepthMask();
    }

    // --------------------------------------------------
    // 3D combat objects and effects
    // --------------------------------------------------

    for (const Projectile& projectile : projectiles)
    {
        if (
            !IsWorldPositionInVisibleChamber(
                projectile.pos
            )
            )
        {
            continue;
        }

        DrawHybridProjectile3D(
            projectile
        );
    }

    for (const OrbitalBlade& blade : orbitalBlades)
    {
        DrawHybridOrbitalBlade3D(
            blade
        );
    }

    DrawHybridDongfeng3D();

    for (const VfxParticle& particle : vfxParticles)
    {
        if (
            particle.active &&
            !IsGroundVfx(
                particle.type
            ) &&
            !IsForegroundVfx(
                particle.type
            )
            )
        {
            if (
                !IsWorldPositionInVisibleChamber(
                    particle.pos
                )
                )
            {
                continue;
            }

            DrawHybridVfx3D(
                particle
            );
        }
    }
}

void Game::DrawHybridEditorOverlay3D()
{
#if MOXIANG_USE_IMGUI
    if (!buildMode)
    {
        return;
    }
#else
    return;
#endif

    Vector2 pickedWorld{};
    int pickedX = -1;
    int pickedY = -1;

    if (
        ScreenToTerrainWorld3D(
            GetMousePosition(),
            pickedWorld,
            &pickedX,
            &pickedY
        )
        )
    {
        hybridHoveredCellX = pickedX;
        hybridHoveredCellY = pickedY;
    }
    else
    {
        hybridHoveredCellX = -1;
        hybridHoveredCellY = -1;
    }

    const float originX =
        -static_cast<float>(MapWidth) *
        TileSize *
        0.5f;

    const float originY =
        -static_cast<float>(MapHeight) *
        TileSize *
        0.5f;

    auto DrawCellOutline =
        [this, originX, originY](
            int cellX,
            int cellY,
            Color color
            )
        {
            if (!IsCellInside(cellX, cellY))
            {
                return;
            }

            const float x0 =
                originX +
                static_cast<float>(cellX) *
                TileSize;

            const float x1 =
                x0 + TileSize;

            const float y0 =
                originY +
                static_cast<float>(cellY) *
                TileSize;

            const float y1 =
                y0 + TileSize;

            Vector2 corners2D[4] = {
                { x0, y0 },
                { x1, y0 },
                { x1, y1 },
                { x0, y1 }
            };

            Vector3 corners3D[4]{};

            for (int i = 0; i < 4; ++i)
            {
                corners3D[i] =
                    WorldToHybrid3D(
                        corners2D[i],
                        2.0f
                    );
            }

            DrawLine3D(corners3D[0], corners3D[1], color);
            DrawLine3D(corners3D[1], corners3D[2], color);
            DrawLine3D(corners3D[2], corners3D[3], color);
            DrawLine3D(corners3D[3], corners3D[0], color);
        };

    if (
        showGrid ||
        showChamberOverlay
        )
    {
        for (int y = 0; y < MapHeight; ++y)
        {
            for (int x = 0; x < MapWidth; ++x)
            {
                Color cellColor{
                    255,
                    255,
                    255,
                    38
                };

                if (!IsCellEnabled(x, y))
                {
                    cellColor =
                        Color{
                            255,
                            70,
                            70,
                            80
                    };
                }
                else if (showChamberOverlay)
                {
                    const int chamberId =
                        terrainCells[
                            CellIndex(x, y)
                        ].chamberId;

                    cellColor =
                        GetChamberDebugColor(
                            chamberId,
                            190
                        );
                }

                DrawCellOutline(
                    x,
                    y,
                    cellColor
                );
            }
        }
    }

    if (
        hybridHoveredCellX >= 0 &&
        hybridHoveredCellY >= 0
        )
    {
        DrawCellOutline(
            hybridHoveredCellX,
            hybridHoveredCellY,
            YELLOW
        );
    }

    for (int y = 0; y < MapHeight; ++y)
    {
        for (int x = 0; x < MapWidth; ++x)
        {
            if (!IsCellEnabled(x, y))
            {
                continue;
            }

            const TerrainCell& cell =
                terrainCells[
                    CellIndex(x, y)
                ];

            int offsetX = 0;
            int offsetY = 0;

            if (
                !GetRampDirectionOffset(
                    cell.rampDirection,
                    offsetX,
                    offsetY
                )
                )
            {
                continue;
            }

            const int targetX = x + offsetX;
            const int targetY = y + offsetY;

            if (!IsCellInside(targetX, targetY))
            {
                continue;
            }

            DrawLine3D(
                WorldToHybrid3D(
                    CellToWorld(x, y),
                    8.0f
                ),
                WorldToHybrid3D(
                    CellToWorld(targetX, targetY),
                    8.0f
                ),
                MAGENTA
            );
        }
    }
}

void Game::DrawHybridWorld3D()
{
    UpdateHybridCamera(0.0f);

    BeginMode3D(hybridCamera);

    DrawHybridTerrain3D();
    DrawHybridGroundEffects3D();
    DrawHybridActors3D();
    DrawHybridEditorOverlay3D();

    EndMode3D();
}

void Game::DrawHybridScreenOverlays2D()
{



    for (const NPC& npc : npcs)
    {
        if (
            !IsWorldPositionInVisibleChamber(
                npc.position
            )
            )
        {
            continue;
        }

        Vector2 screen =
            GetWorldToScreen(
                WorldToHybrid3D(
                    npc.position,
                    npc.radius * 2.3f
                ),
                hybridCamera
            );

        DrawText(
            npc.name.c_str(),
            static_cast<int>(screen.x - 66.0f),
            static_cast<int>(screen.y),
            18,
            WHITE
        );
    }

    for (
        const Enemy& enemy :
        enemies
        )
    {
        if (
            !enemy.active ||
            !IsChamberVisible(
                enemy.chamberId
            ) ||
            (
                enemy.hp >= enemy.maxHp &&
                enemy.type != EnemyType::Boss
                )
            )
        {
            continue;
        }

        // --------------------------------------------------
        // Detect the actual active shooter sprite dimensions
        // --------------------------------------------------

        bool usesShooterSprite =
            false;

        float shooterWidthPixels =
            0.0f;

        float shooterHeightPixels =
            0.0f;

        float shooterAnchorY =
            0.98f;

        if (
            enemy.type ==
            EnemyType::Shooter
            )
        {
            Texture2D unusedTexture{};
            Rectangle unusedSource{};

            usesShooterSprite =
                GetShooterAnimationFrame(
                    enemy,
                    unusedTexture,
                    unusedSource,
                    shooterWidthPixels,
                    shooterHeightPixels,
                    shooterAnchorY
                );
        }

        const bool usesBlobSprite =
            !usesShooterSprite &&
            ShouldUseBlobEnemySprite(
                enemy
            );

        float healthBarWidth =
            enemy.radius *
            2.0f;

        float healthBarHeightAboveGround =
            enemy.visualHeight +
            enemy.radius *
            2.0f +
            12.0f;

        // --------------------------------------------------
        // Archer health bar
        // --------------------------------------------------

        if (usesShooterSprite)
        {
            // UprightWorld billboards compensate their physical
            // height to avoid looking vertically compressed.
            // Apply the same compensation to the health bar.
            Vector3 cameraForward =
                Vector3Normalize(
                    Vector3Subtract(
                        hybridCamera.target,
                        hybridCamera.position
                    )
                );

            const float verticalProjectionScale =
                sqrtf(
                    std::max(
                        0.0001f,
                        1.0f -
                        cameraForward.y *
                        cameraForward.y
                    )
                );

            const float compensatedShooterHeight =
                shooterHeightPixels /
                std::max(
                    0.35f,
                    verticalProjectionScale
                );

            constexpr float shooterHealthBarGap =
                18.0f;

            healthBarHeightAboveGround =
                enemy.visualHeight +
                compensatedShooterHeight *
                shooterAnchorY +
                shooterHealthBarGap;

            healthBarWidth =
                shooterWidthPixels *
                0.68f;
        }
        else if (usesBlobSprite)
        {
            const float visualSize =
                GetBlobEnemyVisualSize(
                    enemy
                );

            healthBarHeightAboveGround =
                enemy.visualHeight +
                visualSize *
                0.92f +
                12.0f;

            healthBarWidth =
                visualSize *
                0.70f;
        }

        const Vector2 screen =
            GetWorldToScreen(
                WorldToHybrid3D(
                    enemy.pos,
                    healthBarHeightAboveGround
                ),
                hybridCamera
            );

        const float hpRatio =
            Clamp(
                static_cast<float>(
                    enemy.hp
                    ) /
                static_cast<float>(
                    std::max(
                        1,
                        enemy.maxHp
                    )
                    ),
                0.0f,
                1.0f
            );

        Rectangle backBar{
            screen.x -
                healthBarWidth *
                0.5f,

            screen.y,

            healthBarWidth,
            6.0f
        };

        Rectangle frontBar =
            backBar;

        frontBar.width *=
            hpRatio;

        DrawRectangleRec(
            backBar,
            DARKGRAY
        );

        DrawRectangleRec(
            frontBar,
            RED
        );
    }

    DrawWorldForegroundEffects();
}

Rectangle Game::GetHuashanImpactSourceRect(
    int frame
) const
{
    const int safeColumns =
        std::max(
            1,
            huashanImpactColumns
        );

    const int safeRows =
        std::max(
            1,
            huashanImpactRows
        );

    const int maximumFrames =
        safeColumns *
        safeRows;

    frame =
        std::max(
            0,
            std::min(
                frame,
                maximumFrames - 1
            )
        );

    const int column =
        frame %
        safeColumns;

    const int row =
        frame /
        safeColumns;

    return Rectangle{
        static_cast<float>(
            column *
            huashanImpactFrameWidth
        ),

        static_cast<float>(
            row *
            huashanImpactFrameHeight
        ),

        static_cast<float>(
            huashanImpactFrameWidth
        ),

        static_cast<float>(
            huashanImpactFrameHeight
        )
    };
}

void Game::DrawHuashanImpactGround2D()
{
    if (
        !huashanImpactSpriteLoaded ||
        huashanImpactSpriteSheet.id == 0
        )
    {
        return;
    }

    auto drawFrame =
        [this](
            Vector2 worldPosition,
            int frame,
            unsigned char alpha
            )
        {
            const float halfSize =
                huashanImpactVisualSize *
                0.5f;

            const Vector2 northWestWorld{
                worldPosition.x - halfSize,
                worldPosition.y - halfSize
            };

            const Vector2 northEastWorld{
                worldPosition.x + halfSize,
                worldPosition.y - halfSize
            };

            const Vector2 southEastWorld{
                worldPosition.x + halfSize,
                worldPosition.y + halfSize
            };

            const Vector2 southWestWorld{
                worldPosition.x - halfSize,
                worldPosition.y + halfSize
            };

            const Vector2 topLeft =
                WorldToViewElevated(
                    northWestWorld,
                    0.75f
                );

            const Vector2 topRight =
                WorldToViewElevated(
                    northEastWorld,
                    0.75f
                );

            const Vector2 bottomRight =
                WorldToViewElevated(
                    southEastWorld,
                    0.75f
                );

            const Vector2 bottomLeft =
                WorldToViewElevated(
                    southWestWorld,
                    0.75f
                );

            DrawTextureFrameOnQuad2D(
                huashanImpactSpriteSheet,
                GetHuashanImpactSourceRect(
                    frame
                ),
                topLeft,
                topRight,
                bottomRight,
                bottomLeft,
                Color{
                    255,
                    255,
                    255,
                    alpha
                }
            );
        };

    BeginBlendMode(
        BLEND_ALPHA
    );

    // Draw lingering final frames first.
    for (
        const HuashanGroundMark& mark :
        huashanGroundMarks
        )
    {
        float opacity =
            mark.maxLife > 0.0f
            ? mark.life / mark.maxLife
            : 0.0f;

        opacity =
            Clamp(
                opacity,
                0.0f,
                1.0f
            );

        // Ease the fade so it remains visible longer,
        // then fades more noticeably near the end.
        opacity =
            opacity *
            opacity;

        unsigned char alpha =
            static_cast<unsigned char>(
                opacity *
                255.0f
                );

        drawFrame(
            mark.position,
            huashanImpactFrameCount - 1,
            alpha
        );
    }

    // Draw the active sprite animation.
    if (huashanImpactAnimationActive)
    {
        drawFrame(
            huashanImpactAnimationPosition,
            huashanImpactFrame,
            255
        );
    }

    EndBlendMode();
}

void Game::DrawHybridGroundTextureFrame(
    Texture2D texture,
    Rectangle source,
    Vector2 worldPosition,
    float sizePixels,
    float heightBiasPixels,
    Color tint
) const
{
    if (
        texture.id == 0 ||
        texture.width <= 0 ||
        texture.height <= 0 ||
        sizePixels <= 0.0f
        )
    {
        return;
    }

    const Vector3 center =
        WorldToHybrid3D(
            worldPosition,
            heightBiasPixels
        );

    const float halfSize =
        PixelsToHybridUnits(
            sizePixels *
            0.5f
        );

    // Horizontal XZ quad lying directly on the terrain plane.
    const Vector3 northWest{
        center.x - halfSize,
        center.y,
        center.z - halfSize
    };

    const Vector3 northEast{
        center.x + halfSize,
        center.y,
        center.z - halfSize
    };

    const Vector3 southEast{
        center.x + halfSize,
        center.y,
        center.z + halfSize
    };

    const Vector3 southWest{
        center.x - halfSize,
        center.y,
        center.z + halfSize
    };

    const float inverseWidth =
        1.0f /
        static_cast<float>(
            texture.width
            );

    const float inverseHeight =
        1.0f /
        static_cast<float>(
            texture.height
            );

    const float u0 =
        source.x *
        inverseWidth;

    const float v0 =
        source.y *
        inverseHeight;

    const float u1 =
        (
            source.x +
            source.width
            ) *
        inverseWidth;

    const float v1 =
        (
            source.y +
            source.height
            ) *
        inverseHeight;

    rlSetTexture(
        texture.id
    );

    rlBegin(
        RL_QUADS
    );

    rlColor4ub(
        tint.r,
        tint.g,
        tint.b,
        tint.a
    );

    rlTexCoord2f(u0, v0);
    rlVertex3f(
        northWest.x,
        northWest.y,
        northWest.z
    );

    rlTexCoord2f(u0, v1);
    rlVertex3f(
        southWest.x,
        southWest.y,
        southWest.z
    );

    rlTexCoord2f(u1, v1);
    rlVertex3f(
        southEast.x,
        southEast.y,
        southEast.z
    );

    rlTexCoord2f(u1, v0);
    rlVertex3f(
        northEast.x,
        northEast.y,
        northEast.z
    );

    rlEnd();

    rlSetTexture(0);
}

void Game::DrawHuashanImpactGround3D()
{
    if (
        !huashanImpactSpriteLoaded ||
        huashanImpactSpriteSheet.id == 0
        )
    {
        return;
    }

    // Lingering crater marks.
    for (
        const HuashanGroundMark& mark :
        huashanGroundMarks
        )
    {
        float opacity =
            mark.maxLife > 0.0f
            ? mark.life / mark.maxLife
            : 0.0f;

        opacity =
            Clamp(
                opacity,
                0.0f,
                1.0f
            );

        opacity =
            opacity *
            opacity;

        unsigned char alpha =
            static_cast<unsigned char>(
                opacity *
                255.0f
                );

        DrawHybridGroundTextureFrame(
            huashanImpactSpriteSheet,
            GetHuashanImpactSourceRect(
                huashanImpactFrameCount - 1
            ),
            mark.position,
            huashanImpactVisualSize,
            3.0f,
            Color{
                255,
                255,
                255,
                alpha
            }
        );
    }

    // Current impact animation.
    if (huashanImpactAnimationActive)
    {
        DrawHybridGroundTextureFrame(
            huashanImpactSpriteSheet,
            GetHuashanImpactSourceRect(
                huashanImpactFrame
            ),
            huashanImpactAnimationPosition,
            huashanImpactVisualSize,
            3.0f,
            WHITE
        );
    }
}

void Game::StartHuashanImpactFeedback()
{
    huashanScreenShakeTimer =
        huashanScreenShakeDuration;

    huashanImpactFlashTimer =
        huashanImpactFlashDuration;

    // Give the first impact frame an immediate offset.
    float randomX =
        static_cast<float>(
            GetRandomValue(
                -1000,
                1000
            )
            ) /
        1000.0f;

    float randomY =
        static_cast<float>(
            GetRandomValue(
                -1000,
                1000
            )
            ) /
        1000.0f;

    huashanScreenShakeOffset = {
        randomX *
            huashanScreenShakeStrength,

        randomY *
            huashanScreenShakeStrength *
            0.65f
    };
}

void Game::UpdateHuashanImpactFeedback(
    float dt
)
{
    // ----------------------------------------------
    // Flash
    // ----------------------------------------------

    if (huashanImpactFlashTimer > 0.0f)
    {
        huashanImpactFlashTimer -= dt;

        if (huashanImpactFlashTimer < 0.0f)
        {
            huashanImpactFlashTimer =
                0.0f;
        }
    }

    // ----------------------------------------------
    // Screen shake
    // ----------------------------------------------

    if (huashanScreenShakeTimer <= 0.0f)
    {
        huashanScreenShakeTimer =
            0.0f;

        huashanScreenShakeOffset = {
            0.0f,
            0.0f
        };

        return;
    }

    huashanScreenShakeTimer -= dt;

    if (huashanScreenShakeTimer < 0.0f)
    {
        huashanScreenShakeTimer =
            0.0f;
    }

    float ratio =
        huashanScreenShakeDuration > 0.0f
        ? huashanScreenShakeTimer /
        huashanScreenShakeDuration
        : 0.0f;

    ratio =
        Clamp(
            ratio,
            0.0f,
            1.0f
        );

    // Strong initial shake with fast falloff.
    float currentStrength =
        huashanScreenShakeStrength *
        ratio *
        ratio;

    float randomX =
        static_cast<float>(
            GetRandomValue(
                -1000,
                1000
            )
            ) /
        1000.0f;

    float randomY =
        static_cast<float>(
            GetRandomValue(
                -1000,
                1000
            )
            ) /
        1000.0f;

    huashanScreenShakeOffset = {
        randomX *
            currentStrength,

        randomY *
            currentStrength *
            0.65f
    };
}

void Game::DrawHuashanImpactFlash() const
{
    if (
        huashanImpactFlashTimer <= 0.0f ||
        huashanImpactFlashDuration <= 0.0f
        )
    {
        return;
    }

    float ratio =
        huashanImpactFlashTimer /
        huashanImpactFlashDuration;

    ratio =
        Clamp(
            ratio,
            0.0f,
            1.0f
        );

    // The flash begins bright and disappears quickly.
    float flashStrength =
        ratio *
        ratio;

    unsigned char alpha =
        static_cast<unsigned char>(
            static_cast<float>(
                huashanImpactFlashMaxAlpha
                ) *
            flashStrength
            );

    DrawRectangle(
        0,
        0,
        GetScreenWidth(),
        GetScreenHeight(),
        Color{
            255,
            246,
            215,
            alpha
        }
    );
}
void Game::LoadBossSpriteSheets()
{
    auto LoadBossSheet =
        [this](
            const char* path,
            Texture2D& texture,
            bool& loaded,
            int& framesPerRow
            )
        {
            if (texture.id != 0)
            {
                UnloadTexture(
                    texture
                );

                texture = {};
            }

            loaded = false;

            texture =
                LoadTexture(
                    path
                );

            if (texture.id == 0)
            {
                TraceLog(
                    LOG_WARNING,
                    "[BOSS SPRITE] Failed to load: %s",
                    path
                );

                return;
            }

            SetTextureFilter(
                texture,
                TEXTURE_FILTER_POINT
            );

            framesPerRow =
                std::max(
                    1,
                    texture.width /
                    std::max(
                        1,
                        bossFrameWidth
                    )
                );

            const int expectedHeight =
                bossFrameHeight *
                bossDirectionRows;

            if (
                texture.height <
                expectedHeight
                )
            {
                TraceLog(
                    LOG_WARNING,
                    "[BOSS SPRITE] Sheet too short: %s | "
                    "expected height=%d actual=%d",
                    path,
                    expectedHeight,
                    texture.height
                );
            }

            loaded = true;

            TraceLog(
                LOG_INFO,
                "[BOSS SPRITE] Loaded: %s | "
                "size=%dx%d frames=%d",
                path,
                texture.width,
                texture.height,
                framesPerRow
            );
        };

    LoadBossSheet(
        "Assets/enemies/boss_idle.png",
        bossIdleSpriteSheet,
        bossIdleSpriteLoaded,
        bossIdleFramesPerRow
    );

    LoadBossSheet(
        "Assets/enemies/boss_walk.png",
        bossWalkSpriteSheet,
        bossWalkSpriteLoaded,
        bossWalkFramesPerRow
    );

    bossAttackSpriteSheet =
        LoadTexture(
            "Assets/enemies/boss_attack.png"
        );

    if (bossAttackSpriteSheet.id != 0)
    {
        bossAttackSpriteLoaded = true;

        SetTextureFilter(
            bossAttackSpriteSheet,
            TEXTURE_FILTER_POINT
        );

        bossAttackFramesPerRow =
            std::max(
                1,
                bossAttackSpriteSheet.width /
                bossFrameWidth
            );

        bossAttackDirectionRows =
            std::max(
                1,
                bossAttackSpriteSheet.height /
                bossFrameHeight
            );

        TraceLog(
            LOG_INFO,
            "[BOSS SPRITE] Attack loaded: %dx%d | "
            "frames=%d rows=%d",
            bossAttackSpriteSheet.width,
            bossAttackSpriteSheet.height,
            bossAttackFramesPerRow,
            bossAttackDirectionRows
        );
    }
    else
    {
        bossAttackSpriteLoaded = false;

        TraceLog(
            LOG_WARNING,
            "[BOSS SPRITE] Failed to load: "
            "Assets/enemies/boss_attack.png"
        );
    }
    auto LoadBossActionSheet =
        [this](
            const char* path,
            Texture2D& texture,
            bool& loaded,
            int& framesPerRow,
            int& directionRows
            )
        {
            if (texture.id != 0)
            {
                UnloadTexture(
                    texture
                );

                texture = {};
            }

            loaded = false;

            texture =
                LoadTexture(
                    path
                );

            if (texture.id == 0)
            {
                TraceLog(
                    LOG_WARNING,
                    "[BOSS SPRITE] Failed to load: %s",
                    path
                );

                return;
            }

            SetTextureFilter(
                texture,
                TEXTURE_FILTER_POINT
            );

            framesPerRow =
                std::max(
                    1,
                    texture.width /
                    std::max(
                        1,
                        bossFrameWidth
                    )
                );

            directionRows =
                std::max(
                    1,
                    texture.height /
                    std::max(
                        1,
                        bossFrameHeight
                    )
                );

            loaded = true;

            TraceLog(
                LOG_INFO,
                "[BOSS SPRITE] Loaded action: %s | "
                "frames=%d rows=%d",
                path,
                framesPerRow,
                directionRows
            );
        };

    LoadBossActionSheet(
        "Assets/enemies/boss_roar.png",
        bossRoarSpriteSheet,
        bossRoarSpriteLoaded,
        bossRoarFramesPerRow,
        bossRoarDirectionRows
    );

    LoadBossActionSheet(
        "Assets/enemies/boss_stunned.png",
        bossStunnedSpriteSheet,
        bossStunnedSpriteLoaded,
        bossStunnedFramesPerRow,
        bossStunnedDirectionRows
    );

    LoadBossActionSheet(
        "Assets/enemies/boss_preheal.png",
        bossPreHealSpriteSheet,
        bossPreHealSpriteLoaded,
        bossPreHealFramesPerRow,
        bossPreHealDirectionRows
    );

    LoadBossActionSheet(
        "Assets/enemies/boss_heal_loop.png",
        bossHealingLoopSpriteSheet,
        bossHealingLoopSpriteLoaded,
        bossHealingLoopFramesPerRow,
        bossHealingLoopDirectionRows
    );

    LoadBossActionSheet(
        "Assets/enemies/boss_precharge.png",
        bossPreChargeSpriteSheet,
        bossPreChargeSpriteLoaded,
        bossPreChargeFramesPerRow,
        bossPreChargeDirectionRows
    );

    LoadBossActionSheet(
        "Assets/enemies/boss_charge.png",
        bossChargeSpriteSheet,
        bossChargeSpriteLoaded,
        bossChargeFramesPerRow,
        bossChargeDirectionRows
    );

}

void Game::UpdateBossAnimation(
    Enemy& enemy,
    float dt
)
{
    if (
        enemy.type !=
        EnemyType::Boss
        )
    {
        return;
    }

    auto AdvanceBossLoop =
        [&enemy, dt](
            int frameCount,
            float frameDuration,
            float playbackSpeed,
            int firstFrame
            )
        {
            frameCount =
                std::max(
                    1,
                    frameCount
                );

            firstFrame =
                std::max(
                    0,
                    std::min(
                        firstFrame,
                        frameCount - 1
                    )
                );

            if (
                enemy.bossAnimationFrame <
                firstFrame
                )
            {
                enemy.bossAnimationFrame =
                    firstFrame;
            }

            enemy.bossAnimationTimer +=
                dt *
                playbackSpeed;

            frameDuration =
                std::max(
                    0.01f,
                    frameDuration
                );

            while (
                enemy.bossAnimationTimer >=
                frameDuration
                )
            {
                enemy.bossAnimationTimer -=
                    frameDuration;

                enemy.bossAnimationFrame++;

                if (
                    enemy.bossAnimationFrame >=
                    frameCount
                    )
                {
                    enemy.bossAnimationFrame =
                        firstFrame;
                }
            }
        };


    // --------------------------------------------------
// Prehealing animation
//
// Plays once, then enters the 20-second healing loop.
// --------------------------------------------------

    if (
        enemy.bossActionState ==
        BossActionState::HealingWindup
        )
    {
        enemy.animationState =
            EnemyAnimationState::Attacking;

        const int frameCount =
            std::max(
                1,
                bossPreHealFramesPerRow
            );

        const float frameDuration =
            std::max(
                0.01f,
                bossPreHealFrameDuration
            );

        enemy.bossAnimationTimer +=
            dt;

        while (
            enemy.bossAnimationTimer >=
            frameDuration
            )
        {
            enemy.bossAnimationTimer -=
                frameDuration;

            enemy.bossAnimationFrame++;

            if (
                enemy.bossAnimationFrame >=
                frameCount
                )
            {
                enemy.bossActionState =
                    BossActionState::HealingActive;

                enemy.bossAnimationFrame =
                    0;

                enemy.bossAnimationTimer =
                    0.0f;

                enemy.bossHealingTimer =
                    0.0f;

                enemy.bossHealingWaveTimer =
                    0.0f;

                enemy.bossHealingWavesSpawned =
                    0;

                break;
            }
        }

        return;
    }

    // --------------------------------------------------
    // Healing loop animation
    // --------------------------------------------------

    if (
        enemy.bossActionState ==
        BossActionState::HealingActive
        )
    {
        enemy.animationState =
            EnemyAnimationState::Attacking;

        AdvanceBossLoop(
            bossHealingLoopFramesPerRow,
            bossHealingLoopFrameDuration,
            1.0f,
            0
        );

        return;
    }

    if (
        enemy.bossActionState ==
        BossActionState::BombardmentRoar
        )
    {
        enemy.animationState =
            EnemyAnimationState::Attacking;

        AdvanceBossLoop(
            bossRoarFramesPerRow,
            bossRoarFrameDuration,
            GetBossAttackSpeedMultiplier(
                enemy
            ),
            0
        );

        return;
    }

    if (
        enemy.bossActionState ==
        BossActionState::Stunned
        )
    {
        enemy.animationState =
            EnemyAnimationState::Attacking;

        AdvanceBossLoop(
            bossStunnedFramesPerRow,
            bossStunnedFrameDuration,
            1.0f,
            0
        );

        return;
    }

    if (
        enemy.bossActionState ==
        BossActionState::ChargeWindup
        )
    {
        enemy.animationState =
            EnemyAnimationState::Attacking;

        const int frameCount =
            std::max(
                1,
                bossPreChargeFramesPerRow
            );

        const float frameDuration =
            std::max(
                0.01f,
                bossPreChargeFrameDuration
            );

        const float playbackSpeed =
            GetBossAttackSpeedMultiplier(
                enemy
            );

        enemy.bossAnimationTimer +=
            dt *
            playbackSpeed;

        while (
            enemy.bossAnimationTimer >=
            frameDuration
            )
        {
            enemy.bossAnimationTimer -=
                frameDuration;

            enemy.bossAnimationFrame++;

            // One complete pre-charge animation has finished.
            if (
                enemy.bossAnimationFrame >=
                frameCount
                )
            {
                enemy.bossAnimationFrame =
                    0;

                enemy.bossPreChargeLoopsCompleted++;

                // After two complete loops, begin charging.
                if (
                    enemy.bossPreChargeLoopsCompleted >=
                    bossPreChargeLoopCount
                    )
                {
                    enemy.bossPreChargeLoopsCompleted =
                        bossPreChargeLoopCount;

                    enemy.bossActionState =
                        BossActionState::ChargeActive;

                    // The moving charge sheet starts from frame zero.
                    enemy.bossAnimationFrame =
                        0;

                    enemy.bossAnimationTimer =
                        0.0f;

                    SpawnBossDashAfterimage(
                        enemy
                    );

                    break;
                }
            }
        }

        return;
    }

    if (
        enemy.bossActionState ==
        BossActionState::ChargeActive
        )
    {
        enemy.animationState =
            EnemyAnimationState::Attacking;

        AdvanceBossLoop(
            bossChargeFramesPerRow,
            bossChargeFrameDuration,
            GetBossDashPowerMultiplier(
                enemy
            ),

            // Pre-charge now uses a separate sheet,
            // so the charge sheet uses every frame.
            0
        );

        return;
    }

    // --------------------------------------------------
    // Slam animation
    // --------------------------------------------------

    if (
        enemy.bossActionState ==
        BossActionState::Slam
        )
    {
        enemy.animationState =
            EnemyAnimationState::Attacking;

        if (enemy.frozenTimer > 0.0f)
        {
            return;
        }

        const float attackAnimationSpeed =
            GetBossAttackSpeedMultiplier(
                enemy
            );

        enemy.bossAnimationTimer +=
            dt *
            attackAnimationSpeed;

        while (
            enemy.bossAnimationTimer >=
            bossAttackFrameDuration
            )
        {
            enemy.bossAnimationTimer -=
                bossAttackFrameDuration;

            enemy.bossAnimationFrame++;

            if (
                enemy.bossAnimationFrame >=
                bossAttackImpactFrame &&
                !enemy.bossSlamImpactProcessed
                )
            {
                ResolveBossSlamImpact(
                    enemy
                );
            }

            if (
                enemy.bossAnimationFrame >=
                bossAttackFramesPerRow
                )
            {
                enemy.bossActionState =
                    BossActionState::None;

                enemy.animationState =
                    EnemyAnimationState::Idle;

                enemy.bossAnimationFrame =
                    0;

                enemy.bossAnimationTimer =
                    0.0f;

                break;
            }
        }

        return;
    }

    // --------------------------------------------------
    // Idle and walking animation
    // --------------------------------------------------

    Vector2 movement =
        Vector2Subtract(
            enemy.pos,
            enemy.bossPreviousAnimationPosition
        );

    enemy.bossPreviousAnimationPosition =
        enemy.pos;

    bool isMoving =
        Vector2Length(movement) >
        0.05f;

    if (
        enemy.bossDashCharging ||
        enemy.bossActionState ==
        BossActionState::SlamWindup ||
        enemy.bossActionState ==
        BossActionState::LaserWindup ||
        enemy.bossActionState ==
        BossActionState::LaserActive
        )
    {
        isMoving = false;
    }

    if (enemy.bossDashTimer > 0.0f)
    {
        isMoving = true;

        movement =
            enemy.bossDashVelocity;
    }

    const EnemyAnimationState desiredState =
        isMoving
        ? EnemyAnimationState::Walking
        : EnemyAnimationState::Idle;

    if (
        enemy.animationState !=
        desiredState
        )
    {
        enemy.animationState =
            desiredState;

        enemy.bossAnimationFrame = 0;
        enemy.bossAnimationTimer = 0.0f;
    }

    if (isMoving)
    {
        SetEnemyFacingFromWorldDirection(
            enemy,
            movement
        );
    }

    if (enemy.frozenTimer > 0.0f)
    {
        return;
    }

    int frameCount =
        bossIdleFramesPerRow;

    float frameDuration =
        bossIdleFrameDuration;

    if (
        enemy.animationState ==
        EnemyAnimationState::Walking
        )
    {
        frameCount =
            bossWalkFramesPerRow;

        frameDuration =
            bossWalkFrameDuration;

        float walkingAnimationSpeed =
            GetBossMovementSpeedMultiplier(
                enemy
            );

        // During a dash, animate even faster so the sprite
        // matches its much higher physical movement speed.
        if (enemy.bossDashTimer > 0.0f)
        {
            walkingAnimationSpeed *=
                GetBossDashPowerMultiplier(
                    enemy
                );
        }

        frameDuration =
            std::max(
                0.025f,
                frameDuration /
                walkingAnimationSpeed
            );
    }

    frameCount =
        std::max(
            1,
            frameCount
        );

    frameDuration =
        std::max(
            0.01f,
            frameDuration
        );

    enemy.bossAnimationTimer +=
        dt;

    while (
        enemy.bossAnimationTimer >=
        frameDuration
        )
    {
        enemy.bossAnimationTimer -=
            frameDuration;

        enemy.bossAnimationFrame =
            (
                enemy.bossAnimationFrame +
                1
                ) %
            frameCount;
    }
}

bool Game::GetBossAnimationFrame(
    const Enemy& enemy,
    Texture2D& outTexture,
    Rectangle& outSource,
    float& outWidthPixels,
    float& outHeightPixels,
    float& outAnchorY
) const
{
    if (
        enemy.type !=
        EnemyType::Boss
        )
    {
        return false;
    }

    int framesPerRow = 1;
    int directionRows = bossDirectionRows;

    bool selected = false;

    auto SelectSheet =
        [&](
            Texture2D texture,
            bool loaded,
            int frameCount,
            int rowCount
            )
        {
            if (
                selected ||
                !loaded ||
                texture.id == 0
                )
            {
                return;
            }

            outTexture =
                texture;

            framesPerRow =
                std::max(
                    1,
                    frameCount
                );

            directionRows =
                std::max(
                    1,
                    rowCount
                );

            selected = true;
        };

    if (
        enemy.bossActionState ==
        BossActionState::HealingWindup
        )
    {
        SelectSheet(
            bossPreHealSpriteSheet,
            bossPreHealSpriteLoaded,
            bossPreHealFramesPerRow,
            bossPreHealDirectionRows
        );
    }
    else if (
        enemy.bossActionState ==
        BossActionState::HealingActive
        )
    {
        SelectSheet(
            bossHealingLoopSpriteSheet,
            bossHealingLoopSpriteLoaded,
            bossHealingLoopFramesPerRow,
            bossHealingLoopDirectionRows
        );
    }
    else if (
        enemy.bossActionState ==
        BossActionState::BombardmentRoar
        )
    {
        SelectSheet(
            bossRoarSpriteSheet,
            bossRoarSpriteLoaded,
            bossRoarFramesPerRow,
            bossRoarDirectionRows
        );
    }
    else if (
        enemy.bossActionState ==
        BossActionState::Stunned
        )
    {
        SelectSheet(
            bossStunnedSpriteSheet,
            bossStunnedSpriteLoaded,
            bossStunnedFramesPerRow,
            bossStunnedDirectionRows
        );
    }
    else if (
        enemy.bossActionState ==
        BossActionState::ChargeWindup
        )
    {
        SelectSheet(
            bossPreChargeSpriteSheet,
            bossPreChargeSpriteLoaded,
            bossPreChargeFramesPerRow,
            bossPreChargeDirectionRows
        );
    }
    else if (
        enemy.bossActionState ==
        BossActionState::ChargeActive
        )
    {
        SelectSheet(
            bossChargeSpriteSheet,
            bossChargeSpriteLoaded,
            bossChargeFramesPerRow,
            bossChargeDirectionRows
        );
    }
    else if (
        enemy.bossActionState ==
        BossActionState::Slam
        )
    {
        SelectSheet(
            bossAttackSpriteSheet,
            bossAttackSpriteLoaded,
            bossAttackFramesPerRow,
            bossAttackDirectionRows
        );
    }

    if (
        enemy.animationState ==
        EnemyAnimationState::Attacking
        )
    {
        SelectSheet(
            bossAttackSpriteSheet,
            bossAttackSpriteLoaded,
            bossAttackFramesPerRow,
            bossAttackDirectionRows
        );
    }
    else if (
        enemy.animationState ==
        EnemyAnimationState::Walking
        )
    {
        SelectSheet(
            bossWalkSpriteSheet,
            bossWalkSpriteLoaded,
            bossWalkFramesPerRow,
            bossDirectionRows
        );
    }
    else
    {
        SelectSheet(
            bossIdleSpriteSheet,
            bossIdleSpriteLoaded,
            bossIdleFramesPerRow,
            bossDirectionRows
        );
    }

    // Fallbacks.
    SelectSheet(
        bossIdleSpriteSheet,
        bossIdleSpriteLoaded,
        bossIdleFramesPerRow,
        bossDirectionRows
    );

    SelectSheet(
        bossWalkSpriteSheet,
        bossWalkSpriteLoaded,
        bossWalkFramesPerRow,
        bossDirectionRows
    );

    SelectSheet(
        bossAttackSpriteSheet,
        bossAttackSpriteLoaded,
        bossAttackFramesPerRow,
        bossAttackDirectionRows
    );

    if (!selected)
    {
        return false;
    }

    const int frame =
        std::max(
            0,
            std::min(
                enemy.bossAnimationFrame,
                framesPerRow - 1
            )
        );

    int directionRow = 0;

    if (directionRows > 1)
    {
        directionRow =
            GetPlayerDirectionRow(
                enemy.spriteDirection
            );

        directionRow =
            std::max(
                0,
                std::min(
                    directionRow,
                    directionRows - 1
                )
            );
    }

    outSource = {
        static_cast<float>(
            frame *
            bossFrameWidth
        ),

        static_cast<float>(
            directionRow *
            bossFrameHeight
        ),

        static_cast<float>(
            bossFrameWidth
        ),

        static_cast<float>(
            bossFrameHeight
        )
    };

    const float bottomTrim =
        Clamp(
            bossSpriteBottomTrim,
            0.0f,
            static_cast<float>(
                bossFrameHeight - 1
                )
        );

    outSource.height =
        static_cast<float>(
            bossFrameHeight
            ) -
        bottomTrim;

    outWidthPixels =
        static_cast<float>(
            bossFrameWidth
            ) *
        bossVisualScale;

    outHeightPixels =
        outSource.height *
        bossVisualScale;

    // Keep the billboard above the terrain.
    // The transparent margin is handled through cropping.
    outAnchorY =
        0.98f;
    return true;
}

void Game::DrawBossEnemySprite(
    const Enemy& enemy,
    Vector2 drawPosition,
    Color tint
)
{
    Texture2D texture{};
    Rectangle source{};

    float widthPixels = 0.0f;
    float heightPixels = 0.0f;
    float anchorY = 1.0f;

    if (
        !GetBossAnimationFrame(
            enemy,
            texture,
            source,
            widthPixels,
            heightPixels,
            anchorY
        )
        )
    {
        return;
    }

    Rectangle destination{
        drawPosition.x,
        drawPosition.y,
        widthPixels,
        heightPixels
    };

    Vector2 origin{
        widthPixels * 0.5f,
        heightPixels * anchorY
    };

    DrawTexturePro(
        texture,
        source,
        destination,
        origin,
        0.0f,
        tint
    );
}

void Game::StartBossSlam(
    Enemy& enemy
)
{
    if (
        enemy.type !=
        EnemyType::Boss ||
        enemy.bossActionState !=
        BossActionState::None
        )
    {
        return;
    }

    // Do not start the attack animation yet.
    enemy.bossActionState =
        BossActionState::SlamWindup;

    enemy.bossSlamWindupTimer =
        bossSlamWindupDuration;

    enemy.animationState =
        EnemyAnimationState::Idle;

    enemy.bossAnimationFrame = 0;
    enemy.bossAnimationTimer = 0.0f;

    enemy.bossSlamImpactProcessed =
        false;

    enemy.attackTimer =
        0.0f;

    enemy.path.clear();
    enemy.pathIndex = 0;

    SetEnemyFacingFromWorldDirection(
        enemy,
        Vector2Subtract(
            playerPosition,
            enemy.pos
        )
    );
}

void Game::ResolveBossSlamImpact(
    Enemy& enemy
)
{
    if (
        enemy.bossSlamImpactProcessed
        )
    {
        return;
    }

    enemy.bossSlamImpactProcessed =
        true;

    // White unlit impact ring.
    enemy.bossSlamImpactVisualTimer =
        bossSlamImpactVisualDuration;

    const bool sameTerrainLevel =
        GetTerrainElevationAtWorld(
            enemy.pos
        ) ==
        GetTerrainElevationAtWorld(
            playerPosition
        );

    const float hitDistance =
        Vector2Distance(
            enemy.pos,
            playerPosition
        );

    if (
        sameTerrainLevel &&
        hitDistance <=
        bossSlamRadius +
        player.radius &&
        !IsPlayerAirborne() &&
        !IsPlayerInvulnerable()
        )
    {
        // Apply launch first because DamagePlayer() may begin
 // the player's invulnerability period.
        ApplyKnockbackToPlayer(
            enemy.pos,
            bossSlamKnockbackForce,
            bossSlamKnockbackDuration
        );

        DamagePlayer(
            enemy.contactDamage
        );
    }
}

void Game::StartBossLaser(
    Enemy& enemy
)
{
    if (
        enemy.type !=
        EnemyType::Boss ||
        enemy.bossActionState !=
        BossActionState::None
        )
    {
        return;
    }

    enemy.bossActionState =
        BossActionState::LaserWindup;

    enemy.bossLaserWindupTimer =
        bossLaserWindupDuration;

    enemy.bossLaserActiveTimer =
        bossLaserDuration;

    enemy.bossLaserDamageTimer =
        0.0f;

    // The beam initially aims at the player's current
    // location, but causes no damage during wind-up.
    enemy.bossLaserAimPosition =
        playerPosition;

    enemy.bossLaserEndPosition =
        playerPosition;

    enemy.path.clear();
    enemy.pathIndex = 0;

    enemy.animationState =
        EnemyAnimationState::Idle;

    enemy.bossAnimationFrame = 0;
    enemy.bossAnimationTimer = 0.0f;

    SetEnemyFacingFromWorldDirection(
        enemy,
        Vector2Subtract(
            playerPosition,
            enemy.pos
        )
    );
}

Vector2 Game::GetBossLaserBlockedEnd(
    Vector2 start,
    Vector2 desiredEnd
) const
{
    const float distance =
        Vector2Distance(
            start,
            desiredEnd
        );

    const int sampleCount =
        std::max(
            1,
            static_cast<int>(
                std::ceil(
                    distance /
                    8.0f
                )
                )
        );

    const int laserElevation =
        GetTerrainElevationAtWorld(
            start
        );

    Vector2 previousSample =
        start;

    for (
        int sampleIndex = 1;
        sampleIndex <= sampleCount;
        ++sampleIndex
        )
    {
        const float t =
            static_cast<float>(
                sampleIndex
                ) /
            static_cast<float>(
                sampleCount
                );

        const Vector2 samplePosition =
            Vector2Lerp(
                start,
                desiredEnd,
                t
            );

        if (
            IsProjectileBlockedByWorld(
                previousSample,
                samplePosition,
                laserElevation
            )
            )
        {
            return previousSample;
        }

        previousSample =
            samplePosition;
    }

    return desiredEnd;
}

void Game::UpdateBossLaser(
    Enemy& enemy,
    float dt
)
{
    if (
        enemy.bossActionState !=
        BossActionState::LaserWindup &&
        enemy.bossActionState !=
        BossActionState::LaserActive
        )
    {
        return;
    }
    const float attackSpeedMultiplier =
        GetBossAttackSpeedMultiplier(
            enemy
        );

    // The target follows the player slowly instead of
    // snapping directly onto their current position.
    enemy.bossLaserAimPosition =
        Vector2Lerp(
            enemy.bossLaserAimPosition,
            playerPosition,
            Clamp(
                bossLaserFollowSpeed *
                attackSpeedMultiplier *
                dt,
                0.0f,
                1.0f
            )
        );

    Vector2 direction =
        Vector2Subtract(
            enemy.bossLaserAimPosition,
            enemy.pos
        );

    if (
        Vector2Length(direction) <=
        0.001f
        )
    {
        direction = {
            1.0f,
            0.0f
        };
    }
    else
    {
        direction =
            Vector2Normalize(
                direction
            );
    }

    SetEnemyFacingFromWorldDirection(
        enemy,
        direction
    );

    // Start slightly outside the Boss body.
    const Vector2 laserStart =
        Vector2Add(
            enemy.pos,
            Vector2Scale(
                direction,
                bossLaserForwardOffset
            )
        );

    enemy.bossLaserStartPosition =
        laserStart;

    const Vector2 desiredEnd =
        Vector2Add(
            enemy.pos,
            Vector2Scale(
                direction,
                bossLaserRange
            )
        );

    enemy.bossLaserEndPosition =
        GetBossLaserBlockedEnd(
            laserStart,
            desiredEnd
        );

    if (
        enemy.bossActionState ==
        BossActionState::LaserWindup
        )
    {
        enemy.bossLaserWindupTimer -=
            dt *
            attackSpeedMultiplier;

        if (
            enemy.bossLaserWindupTimer <=
            0.0f
            )
        {
            enemy.bossLaserWindupTimer =
                0.0f;

            enemy.bossActionState =
                BossActionState::LaserActive;

            enemy.bossLaserActiveTimer =
                bossLaserDuration;

            enemy.bossLaserDamageTimer =
                0.0f;
        }

        return;
    }

    enemy.bossLaserActiveTimer -=
        dt;

    enemy.bossLaserDamageTimer -=
        dt;

    if (
        enemy.bossLaserDamageTimer <=
        0.0f
        )
    {
        enemy.bossLaserDamageTimer =
            bossLaserDamageInterval;

        const bool sameTerrainLevel =
            GetTerrainElevationAtWorld(
                enemy.pos
            ) ==
            GetTerrainElevationAtWorld(
                playerPosition
            );

        float segmentT = 0.0f;

        const float distanceSquared =
            DistancePointToSegmentSquared(
                playerPosition,
                laserStart,
                enemy.bossLaserEndPosition,
                segmentT
            );

        const float damageRadius =
            player.radius +
            bossLaserWidth *
            0.5f;

        if (
            sameTerrainLevel &&
            distanceSquared <=
            damageRadius *
            damageRadius &&
            !IsPlayerAirborne() &&
            !IsPlayerInvulnerable()
            )
        {
            DamagePlayer(
                enemy.bulletDamage
            );

            SpawnHitSpark(
                playerPosition
            );
        }
    }

    if (
        enemy.bossLaserActiveTimer <=
        0.0f
        )
    {
        enemy.bossActionState =
            BossActionState::None;

        enemy.bossLaserActiveTimer =
            0.0f;

        enemy.bossLaserCooldownTimer =
            bossLaserCooldown;

        enemy.animationState =
            EnemyAnimationState::Idle;

        enemy.bossAnimationFrame = 0;
        enemy.bossAnimationTimer = 0.0f;

        RefreshEnemyPath(
            enemy
        );
    }
}

void Game::UpdateBossBehavior(
    Enemy& enemy,
    float dt,
    float distanceToPlayer,
    bool sameTerrainLevel
)
{
    const float healthRatio =
        static_cast<float>(
            enemy.hp
            ) /
        static_cast<float>(
            std::max(
                1,
                enemy.maxHp
            )
            );

    const bool bossBelowHalfHealth =
        healthRatio <=
        0.50f;

    const float lowHealthAbilityFrequency =
        bossBelowHalfHealth
        ? bossLowHealthAbilityFrequencyMultiplier
        : 1.0f;

    const float movementSpeedMultiplier =
        GetBossMovementSpeedMultiplier(
            enemy
        );

    // Below 50% HP, this extra multiplier affects:
    //
    // - normal slam frequency
    // - slam wind-up speed
    // - laser cooldown
    // - roar cooldown
    // - charge cooldown
    const float attackSpeedMultiplier =
        GetBossAttackSpeedMultiplier(
            enemy
        ) *
        lowHealthAbilityFrequency;

    const float dashPowerMultiplier =
        GetBossDashPowerMultiplier(
            enemy
        );

    enemy.bossBombardmentCooldownTimer =
        std::max(
            0.0f,
            enemy.bossBombardmentCooldownTimer -
            dt *
            attackSpeedMultiplier
        );

    enemy.bossChargeCooldownTimer =
        std::max(
            0.0f,
            enemy.bossChargeCooldownTimer -
            dt *
            attackSpeedMultiplier
        );

    enemy.bossDecisionTimer =
        std::max(
            0.0f,
            enemy.bossDecisionTimer -
            dt
        );

    enemy.bossForwardDashCooldownTimer =
        std::max(
            0.0f,
            enemy.bossForwardDashCooldownTimer -
            dt *
            dashPowerMultiplier *
            lowHealthAbilityFrequency
        );

    enemy.bossSlamImpactVisualTimer =
        std::max(
            0.0f,
            enemy.bossSlamImpactVisualTimer -
            dt
        );

    enemy.bossLaserCooldownTimer =
        std::max(
            0.0f,
            enemy.bossLaserCooldownTimer -
            dt *
            attackSpeedMultiplier
        );

    // --------------------------------------------------
// Dedicated Boss stunned state
// --------------------------------------------------

    if (
        enemy.bossActionState ==
        BossActionState::Stunned
        )
    {
        enemy.bossStunnedTimer -=
            dt;

        if (
            enemy.bossStunnedTimer <=
            0.0f
            )
        {
            enemy.bossStunnedTimer =
                0.0f;

            enemy.bossActionState =
                BossActionState::None;

            enemy.animationState =
                EnemyAnimationState::Idle;

            enemy.bossAnimationFrame = 0;
            enemy.bossAnimationTimer = 0.0f;

            RefreshEnemyPath(
                enemy
            );
        }

        UpdateBossAnimation(
            enemy,
            dt
        );

        return;
    }

    // --------------------------------------------------
// Boss healing states
//
// These states completely own the Boss until healing
// ends and the forced stun begins.
// --------------------------------------------------

    if (
        enemy.bossActionState ==
        BossActionState::HealingWindup ||
        enemy.bossActionState ==
        BossActionState::HealingActive
        )
    {
        UpdateBossHealing(
            enemy,
            dt
        );

        UpdateBossAnimation(
            enemy,
            dt
        );

        return;
    }

    // --------------------------------------------------
    // Straight-line charge
    // --------------------------------------------------

    if (
        enemy.bossActionState ==
        BossActionState::ChargeWindup ||
        enemy.bossActionState ==
        BossActionState::ChargeActive
        )
    {
        UpdateBossCharge(
            enemy,
            dt
        );

        UpdateBossAnimation(
            enemy,
            dt
        );

        return;
    }

    // --------------------------------------------------
    // Bombardment roar
    // --------------------------------------------------

    if (
        enemy.bossActionState ==
        BossActionState::BombardmentRoar
        )
    {
        UpdateBossBombardment(
            enemy,
            dt
        );

        UpdateBossAnimation(
            enemy,
            dt
        );

        return;
    }

    // --------------------------------------------------
// Healing trigger
//
// The Boss must be free before beginning healing.
// Crossing below 50% during another attack causes
// healing to begin after that attack finishes.
// --------------------------------------------------

    const bool bossCanBeginHealing =
        enemy.bossActionState ==
        BossActionState::None &&
        !enemy.bossDashCharging &&
        enemy.bossDashTimer <=
        0.0f;

    if (
        bossCanBeginHealing &&
        healthRatio <=
        bossHealingTriggerHealthRatio &&
        enemy.bossHealingUseCount <
        bossHealingMaximumUses &&
        enemy.bossHealingCooldownTimer <=
        0.0f
        )
    {
        StartBossHealing(
            enemy
        );

        UpdateBossAnimation(
            enemy,
            dt
        );

        return;
    }

    // --------------------------------------------------
    // One-time health phases have highest priority.
    // --------------------------------------------------

    if (
        !enemy.bossDashCharging &&
        enemy.bossDashTimer <= 0.0f
        )
    {
        if (
            healthRatio <= 0.75f &&
            !enemy.bossPhase75Triggered
            )
        {
            enemy.bossPhase75Triggered =
                true;

            StartBossDash(
                enemy
            );
        }
        else if (
            healthRatio <= 0.25f &&
            !enemy.bossPhase25Triggered
            )
        {
            enemy.bossPhase25Triggered =
                true;

            StartBossDash(
                enemy
            );
        }
    }

    // --------------------------------------------------
    // Phase jump charge.
    // --------------------------------------------------

    if (enemy.bossDashCharging)
    {
        enemy.bossDashChargeTimer -=
            dt;

        if (
            enemy.bossDashChargeTimer <=
            0.0f
            )
        {
            enemy.bossDashCharging =
                false;

            enemy.bossDashChargeTimer =
                0.0f;

            ExecuteBossDash(
                enemy
            );
        }

        UpdateBossAnimation(
            enemy,
            dt
        );

        return;
    }

    // --------------------------------------------------
    // Phase jump movement.
    // --------------------------------------------------

    if (enemy.bossDashTimer > 0.0f)
    {
        enemy.bossDashTimer -=
            dt;

        Vector2 totalMovement =
            Vector2Scale(
                enemy.bossDashVelocity,
                dt
            );

        const float movementLength =
            Vector2Length(
                totalMovement
            );

        const int stepCount =
            std::max(
                1,
                static_cast<int>(
                    std::ceil(
                        movementLength /
                        8.0f
                    )
                    )
            );

        const Vector2 movementStep =
            Vector2Scale(
                totalMovement,
                1.0f /
                static_cast<float>(
                    stepCount
                    )
            );

        for (
            int stepIndex = 0;
            stepIndex < stepCount;
            ++stepIndex
            )
        {
            if (
                enemy.bossDashIsForward &&
                Vector2Distance(
                    enemy.pos,
                    playerPosition
                ) <=
                bossForwardDashStopDistance
                )
            {
                enemy.bossDashTimer =
                    0.0f;

                break;
            }

            const Vector2 nextPosition =
                Vector2Add(
                    enemy.pos,
                    movementStep
                );

            if (
                !CanEnemyStandAt(
                    enemy.pos,
                    nextPosition,
                    std::max(
                        6.0f,
                        enemy.radius *
                        0.55f
                    )
                )
                )
            {
                enemy.bossDashTimer =
                    0.0f;

                break;
            }

            enemy.pos =
                nextPosition;
        }

        if (enemy.bossDashIsForward)
        {
            enemy.bossDashAfterimageTimer -=
                dt *
                dashPowerMultiplier;

            while (
                enemy.bossDashAfterimageTimer <=
                0.0f
                )
            {
                SpawnBossDashAfterimage(
                    enemy
                );

                enemy.bossDashAfterimageTimer +=
                    bossDashAfterimageInterval;
            }
        }

        if (
            enemy.bossDashTimer <=
            0.0f
            )
        {
            const BossDashPurpose completedPurpose =
                enemy.bossDashPurpose;

            if (enemy.bossDashIsForward)
            {
                SpawnBossDashAfterimage(
                    enemy
                );
            }

            enemy.bossDashTimer =
                0.0f;

            enemy.bossDashIsForward =
                false;

            enemy.bossDashVelocity = {
                0.0f,
                0.0f
            };

            enemy.bossDashPurpose =
                BossDashPurpose::None;

            // ----------------------------------------------
            // Chase dash ends in a delayed ground slam.
            // ----------------------------------------------

            if (
                completedPurpose ==
                BossDashPurpose::ChaseSlam
                )
            {
                StartBossSlam(
                    enemy
                );
            }

            // ----------------------------------------------
            // Bombardment retreat ends in the roaring state.
            // ----------------------------------------------

            else if (
                completedPurpose ==
                BossDashPurpose::BombardmentEscape
                )
            {
                StartBossBombardmentRoar(
                    enemy
                );
            }

            // ----------------------------------------------
            // Health-phase escape simply resumes movement.
            // ----------------------------------------------

            else
            {
                RefreshEnemyPath(
                    enemy
                );
            }
        }

        UpdateBossAnimation(
            enemy,
            dt
        );

        return;
    }

    if (
        IsEnemyCrowdControlled(
            enemy
        )
        )
    {
        UpdateBossAnimation(
            enemy,
            dt
        );

        return;
    }

    // --------------------------------------------------
// Continue an existing special attack.
//
// Existing actions must always update before the Boss
// is allowed to select a new attack.
// --------------------------------------------------

    if (
        enemy.bossActionState ==
        BossActionState::LaserWindup ||
        enemy.bossActionState ==
        BossActionState::LaserActive
        )
    {
        UpdateBossLaser(
            enemy,
            dt
        );

        UpdateBossAnimation(
            enemy,
            dt
        );

        return;
    }

    // --------------------------------------------------
    // Continue the slam warning.
    //
    // This must happen before bombardment, charge, chase
    // dash, or laser selection.
    // --------------------------------------------------

    if (
        enemy.bossActionState ==
        BossActionState::SlamWindup
        )
    {
        enemy.bossSlamWindupTimer -=
            dt *
            attackSpeedMultiplier;

        if (
            enemy.bossSlamWindupTimer <=
            0.0f
            )
        {
            enemy.bossSlamWindupTimer =
                0.0f;

            enemy.bossActionState =
                BossActionState::Slam;

            enemy.animationState =
                EnemyAnimationState::Attacking;

            enemy.bossAnimationFrame =
                0;

            enemy.bossAnimationTimer =
                0.0f;
        }

        UpdateBossAnimation(
            enemy,
            dt
        );

        return;
    }

    // --------------------------------------------------
    // Continue the slam animation.
    // --------------------------------------------------

    if (
        enemy.bossActionState ==
        BossActionState::Slam
        )
    {
        UpdateBossAnimation(
            enemy,
            dt
        );

        return;
    }

    // --------------------------------------------------
    // The Boss may select a new action only when it is
    // completely free.
    // --------------------------------------------------

    const bool bossCanStartNewAction =
        enemy.bossActionState ==
        BossActionState::None &&
        !enemy.bossDashCharging &&
        enemy.bossDashTimer <=
        0.0f;

    enemy.attackTimer =
        std::min(
            enemy.attackInterval,
            enemy.attackTimer +
            dt *
            attackSpeedMultiplier
        );

    const bool chargeRangeValid =
        distanceToPlayer >
        bossSlamRadius *
        1.45f &&
        distanceToPlayer <
        950.0f;

    // Roar and charge become more likely below 50% HP.
    const int roarRollEnd =
        bossBelowHalfHealth
        ? 35
        : 22;

    const int chargeRollEnd =
        roarRollEnd +
        (
            bossBelowHalfHealth
            ? 40
            : 30
            );

    // --------------------------------------------------
    // Random special-action selection.
    //
    // Roar is now a normal base ability and may occur
    // from 100% HP onward.
    // --------------------------------------------------

    if (
        bossCanStartNewAction &&
        enemy.bossDecisionTimer <=
        0.0f
        )
    {
        enemy.bossDecisionTimer =
            bossBelowHalfHealth
            ? bossLowHealthDecisionInterval
            : bossDecisionInterval;

        const int attackRoll =
            GetRandomValue(
                0,
                99
            );

        // --------------------------------------------------
        // Roar / bombardment
        //
        // Plays directly without first performing the retreat
        // dash. Available at every Boss health level.
        // --------------------------------------------------

        if (
            attackRoll <
            roarRollEnd &&
            sameTerrainLevel &&
            enemy.bossBombardmentCooldownTimer <=
            0.0f
            )
        {
            StartBossBombardmentRoar(
                enemy
            );

            UpdateBossAnimation(
                enemy,
                dt
            );

            return;
        }

        // --------------------------------------------------
        // Straight-line charge
        // --------------------------------------------------

        if (
            attackRoll >=
            roarRollEnd &&
            attackRoll <
            chargeRollEnd &&
            chargeRangeValid &&
            sameTerrainLevel &&
            enemy.bossChargeCooldownTimer <=
            0.0f
            )
        {
            StartBossCharge(
                enemy
            );

            UpdateBossAnimation(
                enemy,
                dt
            );

            return;
        }
    }

    // --------------------------------------------------
    // Far-distance chase dash.
    //
    // This now runs only when no other Boss action is active.
    // --------------------------------------------------

    if (
        bossCanStartNewAction &&
        sameTerrainLevel &&
        distanceToPlayer >
        bossForwardDashTriggerDistance &&
        enemy.bossForwardDashCooldownTimer <=
        0.0f
        )
    {
        StartBossForwardDash(
            enemy
        );

        UpdateBossAnimation(
            enemy,
            dt
        );

        return;
    }

    // --------------------------------------------------
    // Below 50% HP: ranged laser when the player is not
    // within slam range.
    // --------------------------------------------------

    const bool laserUnlocked =
        healthRatio <=
        0.50f;

    const float slamStartRange =
        bossSlamRadius *
        0.82f;

    if (
        bossCanStartNewAction &&
        laserUnlocked &&
        sameTerrainLevel &&
        distanceToPlayer >
        slamStartRange &&
        enemy.bossLaserCooldownTimer <=
        0.0f
        )
    {
        StartBossLaser(
            enemy
        );

        UpdateBossAnimation(
            enemy,
            dt
        );

        return;
    }

    // --------------------------------------------------
    // Basic circular axe slam.
    // --------------------------------------------------

    if (
        bossCanStartNewAction &&
        sameTerrainLevel &&
        distanceToPlayer <=
        slamStartRange &&
        enemy.attackTimer >=
        enemy.attackInterval
        )
    {
        StartBossSlam(
            enemy
        );

        UpdateBossAnimation(
            enemy,
            dt
        );

        return;
    }

    // --------------------------------------------------
    // Ordinary Boss movement.
    // --------------------------------------------------

    enemy.pathRefreshTimer +=
        dt;

    const bool pathMissing =
        enemy.path.empty();

    const bool pathFinished =
        !enemy.path.empty() &&
        enemy.pathIndex >=
        static_cast<int>(
            enemy.path.size()
            );

    // Several Boss actions clear the path. Refresh it
    // immediately instead of waiting for the path timer.
    if (
        pathMissing ||
        pathFinished ||
        enemy.pathRefreshTimer >=
        enemy.pathRefreshInterval
        )
    {
        RefreshEnemyPath(
            enemy
        );
    }

    // Temporarily scale movement without permanently
    // changing the Boss's base movement speed.
    const float baseMovementSpeed =
        enemy.speed;

    enemy.speed =
        baseMovementSpeed *
        movementSpeedMultiplier;

    MoveEnemyAlongPath(
        enemy,
        dt
    );

    enemy.speed =
        baseMovementSpeed;

    UpdateBossAnimation(
        enemy,
        dt
    );
}

void Game::DrawBossLasers2D() const
{
    for (const Enemy& enemy : enemies)
    {
        if (
            !enemy.active ||
            enemy.type !=
            EnemyType::Boss
            )
        {
            continue;
        }

        const bool windup =
            enemy.bossActionState ==
            BossActionState::LaserWindup;

        const bool active =
            enemy.bossActionState ==
            BossActionState::LaserActive;

        if (!windup && !active)
        {
            continue;
        }

        const Vector2 start =
            WorldToViewElevated(
                enemy.bossLaserStartPosition,
                bossLaserVisualHeight
            );

        const Vector2 end =
            WorldToViewElevated(
                enemy.bossLaserEndPosition,
                bossLaserVisualHeight
            );

        if (windup)
        {
            DrawLineEx(
                start,
                end,
                5.0f,
                Color{
                    255,
                    70,
                    40,
                    115
                }
            );

            continue;
        }

        DrawLineEx(
            start,
            end,
            bossLaserWidth + 18.0f,
            Color{
                100,
                220,
                255,
                75
            }
        );

        DrawLineEx(
            start,
            end,
            bossLaserWidth,
            Color{
                150,
                235,
                255,
                225
            }
        );

        DrawLineEx(
            start,
            end,
            bossLaserWidth * 0.30f,
            WHITE
        );
    }
}

void Game::DrawBossLasersHybrid3D() const
{
    rlDisableDepthMask();

    BeginBlendMode(
        BLEND_ADDITIVE
    );

    for (const Enemy& enemy : enemies)
    {
        if (
            !enemy.active ||
            enemy.type !=
            EnemyType::Boss
            )
        {
            continue;
        }

        const bool windup =
            enemy.bossActionState ==
            BossActionState::LaserWindup;

        const bool active =
            enemy.bossActionState ==
            BossActionState::LaserActive;

        if (!windup && !active)
        {
            continue;
        }

        const Vector3 start =
            WorldToHybrid3D(
                enemy.bossLaserStartPosition,
                bossLaserVisualHeight
            );

        const Vector3 end =
            WorldToHybrid3D(
                enemy.bossLaserEndPosition,
                bossLaserVisualHeight
            );

        // --------------------------------------------------
        // Blue laser warning
        // --------------------------------------------------

        if (windup)
        {
            DrawCylinderEx(
                start,
                end,
                PixelsToHybridUnits(
                    3.0f
                ),
                PixelsToHybridUnits(
                    3.0f
                ),
                8,
                Color{
                    80,
                    190,
                    255,
                    150
                }
            );

            DrawCylinderEx(
                start,
                end,
                PixelsToHybridUnits(
                    1.2f
                ),
                PixelsToHybridUnits(
                    1.2f
                ),
                8,
                Color{
                    220,
                    250,
                    255,
                    240
                }
            );

            continue;
        }

        // --------------------------------------------------
        // Blue continuous laser
        // --------------------------------------------------

        // Wide outer blue glow.
        DrawCylinderEx(
            start,
            end,
            PixelsToHybridUnits(
                bossLaserWidth *
                0.70f
            ),
            PixelsToHybridUnits(
                bossLaserWidth *
                0.70f
            ),
            12,
            Color{
                45,
                125,
                255,
                75
            }
        );

        // Main beam body.
        DrawCylinderEx(
            start,
            end,
            PixelsToHybridUnits(
                bossLaserWidth *
                0.48f
            ),
            PixelsToHybridUnits(
                bossLaserWidth *
                0.48f
            ),
            12,
            Color{
                65,
                185,
                255,
                185
            }
        );

        // Bright cyan-white core.
        DrawCylinderEx(
            start,
            end,
            PixelsToHybridUnits(
                bossLaserWidth *
                0.15f
            ),
            PixelsToHybridUnits(
                bossLaserWidth *
                0.15f
            ),
            10,
            Color{
                215,
                248,
                255,
                255
            }
        );
    }

    EndBlendMode();

    rlEnableDepthMask();
}

void Game::ApplyKnockbackToPlayer(
    Vector2 origin,
    float force,
    float duration
)
{
    // Do not interrupt the player's own Huashan jump.
    if (huashanJumpActive)
    {
        return;
    }

    Vector2 direction =
        Vector2Subtract(
            playerPosition,
            origin
        );

    if (
        Vector2Length(direction) <=
        0.01f
        )
    {
        direction = {
            1.0f,
            0.0f
        };
    }
    else
    {
        direction =
            Vector2Normalize(
                direction
            );
    }

    playerKnockbackVelocity =
        Vector2Scale(
            direction,
            force
        );

    playerKnockbackTimer =
        std::max(
            0.01f,
            duration
        );

    playerKnockbackMaxTimer =
        playerKnockbackTimer;

    // Stronger attacks launch the player higher.
    playerKnockbackPeakHeight =
        Clamp(
            force *
            0.13f,
            105.0f,
            180.0f
        );

    playerKnockbackActive =
        true;

    currentPath.clear();
    pathIndex = 0;
    hasPath = false;
    pendingNpc = -1;

    joystickActive = false;

    joystickDirection = {
        0.0f,
        0.0f
    };
}

void Game::UpdatePlayerKnockback(
    float dt
)
{
    if (!playerKnockbackActive)
    {
        return;
    }

    playerKnockbackTimer -=
        dt;

    Vector2 totalMovement =
        Vector2Scale(
            playerKnockbackVelocity,
            dt
        );

    const float movementLength =
        Vector2Length(
            totalMovement
        );

    const int stepCount =
        std::max(
            1,
            static_cast<int>(
                std::ceil(
                    movementLength /
                    std::max(
                        1.0f,
                        playerCollisionSubstep
                    )
                )
                )
        );

    const Vector2 movementStep =
        Vector2Scale(
            totalMovement,
            1.0f /
            static_cast<float>(
                stepCount
                )
        );

    for (
        int stepIndex = 0;
        stepIndex < stepCount;
        ++stepIndex
        )
    {
        const Vector2 candidatePosition =
            Vector2Add(
                playerPosition,
                movementStep
            );

        if (
            !CanPlayerStandAt(
                candidatePosition
            )
            )
        {
            playerKnockbackVelocity = {
                0.0f,
                0.0f
            };

            playerKnockbackActive =
                false;

            break;
        }

        playerPosition =
            candidatePosition;
    }

    const float damping =
        expf(
            -playerKnockbackDamping *
            dt
        );

    playerKnockbackVelocity =
        Vector2Scale(
            playerKnockbackVelocity,
            damping
        );

    if (
        playerKnockbackTimer <= 0.0f ||
        Vector2Length(
            playerKnockbackVelocity
        ) < 20.0f
        )
    {
        playerKnockbackActive =
            false;

        playerKnockbackTimer =
            0.0f;

        playerKnockbackVelocity = {
            0.0f,
            0.0f
        };
    }
    if (
        playerKnockbackTimer <= 0.0f ||
        Vector2Length(
            playerKnockbackVelocity
        ) < 20.0f
        )
    {
        playerKnockbackActive =
            false;

        playerKnockbackTimer =
            0.0f;

        playerKnockbackMaxTimer =
            0.0f;

        playerKnockbackVelocity = {
            0.0f,
            0.0f
        };
    }

    player.pos =
        playerPosition;
}

void Game::StartBossForwardDash(
    Enemy& enemy
)
{
    if (
        enemy.type !=
        EnemyType::Boss ||
        enemy.bossActionState !=
        BossActionState::None ||
        enemy.bossDashCharging ||
        enemy.bossDashTimer > 0.0f ||
        enemy.bossForwardDashCooldownTimer > 0.0f
        )
    {
        return;
    }

    Vector2 direction =
        Vector2Subtract(
            playerPosition,
            enemy.pos
        );

    if (
        Vector2Length(direction) <=
        0.01f
        )
    {
        return;
    }

    direction =
        Vector2Normalize(
            direction
        );

    const float dashPowerMultiplier =
        GetBossDashPowerMultiplier(
            enemy
        );

    const float dashDurationMultiplier =
        GetBossDashDurationMultiplier(
            enemy
        );

    enemy.bossDashIsForward =
        true;

    // Lower-health phases dash for longer.
    enemy.bossDashTimer =
        bossForwardDashDuration *
        dashDurationMultiplier;

    // Lower-health phases dash faster.
    enemy.bossDashVelocity =
        Vector2Scale(
            direction,
            bossForwardDashSpeed *
            dashPowerMultiplier
        );

    // Store the normal cooldown. UpdateBossBehavior()
    // will count it down faster at higher phases.
    enemy.bossForwardDashCooldownTimer =
        bossForwardDashCooldown;

    enemy.bossDashAfterimageTimer =
        0.0f;

    enemy.path.clear();
    enemy.pathIndex = 0;

    enemy.animationState =
        EnemyAnimationState::Walking;

    SetEnemyFacingFromWorldDirection(
        enemy,
        direction
    );
    enemy.bossDashPurpose =
        BossDashPurpose::ChaseSlam;

    SpawnBossDashAfterimage(
        enemy
    );
}

void Game::SpawnBossDashAfterimage(
    const Enemy& enemy
)
{
    const bool hasWalkingSheet =
        bossWalkSpriteLoaded &&
        bossWalkSpriteSheet.id != 0;

    const bool hasIdleSheet =
        bossIdleSpriteLoaded &&
        bossIdleSpriteSheet.id != 0;

    if (
        !hasWalkingSheet &&
        !hasIdleSheet
        )
    {
        return;
    }

    if (
        static_cast<int>(
            dashAfterimages.size()
            ) >=
        maxDashAfterimages
        )
    {
        dashAfterimages.erase(
            dashAfterimages.begin()
        );
    }

    const int frameCount =
        hasWalkingSheet
        ? bossWalkFramesPerRow
        : bossIdleFramesPerRow;

    DashAfterimage afterimage;

    afterimage.worldPosition =
        enemy.pos;

    afterimage.direction =
        enemy.spriteDirection;

    afterimage.frame =
        std::max(
            0,
            std::min(
                enemy.bossAnimationFrame,
                std::max(
                    1,
                    frameCount
                ) - 1
            )
        );

    afterimage.useBossSprite =
        true;

    afterimage.life =
        dashAfterimageLifetime;

    afterimage.maxLife =
        dashAfterimageLifetime;

    dashAfterimages.push_back(
        afterimage
    );
}

void Game::DrawBossUnlitEffects() const
{

    if (
        rendererMode ==
        WorldRendererMode::Hybrid3D
        )
    {
        DrawBossSlamDomeHybrid3D();
        return;
    }

    // --------------------------------------------------
    // Legacy 2D renderer
    // --------------------------------------------------

    if (
        rendererMode ==
        WorldRendererMode::Legacy2D
        )
    {
        BeginMode2D(
            camera
        );

        BeginBlendMode(
            BLEND_ADDITIVE
        );

        for (const Enemy& enemy : enemies)
        {
            if (
                !enemy.active ||
                !IsChamberVisible(
                    enemy.chamberId
                ) ||
                enemy.type !=
                EnemyType::Boss
                )
            {
                continue;
            }

            // ------------------------------------------
            // White translucent slam warning dome
            // ------------------------------------------

            if (
                enemy.bossActionState ==
                BossActionState::SlamWindup
                )
            {
                const float progress =
                    1.0f -
                    Clamp(
                        enemy.bossSlamWindupTimer /
                        std::max(
                            0.01f,
                            bossSlamWindupDuration
                        ),
                        0.0f,
                        1.0f
                    );

                const float pulse =
                    0.5f +
                    0.5f *
                    sinf(
                        progress *
                        PI *
                        5.0f
                    );

                const float domeRadius =
                    bossSlamRadius *
                    (
                        0.92f +
                        progress *
                        0.08f
                        );

                // Large translucent dome body.
                DrawGroundCircle(
                    enemy.pos,
                    domeRadius,
                    Color{
                        255,
                        255,
                        255,
                        static_cast<unsigned char>(
                            26.0f +
                            progress *
                            22.0f
                        )
                    }
                );

                // Brighter inner layer gives the effect
                // more volume instead of looking flat.
                DrawGroundCircle(
                    enemy.pos,
                    domeRadius *
                    0.72f,
                    Color{
                        255,
                        255,
                        255,
                        static_cast<unsigned char>(
                            13.0f +
                            pulse *
                            17.0f
                        )
                    }
                );

                // Bright outer boundary.
                DrawGroundCircleLines(
                    enemy.pos,
                    domeRadius,
                    Color{
                        255,
                        255,
                        255,
                        static_cast<unsigned char>(
                            190.0f +
                            progress *
                            65.0f
                        )
                    }
                );

                DrawGroundCircleLines(
                    enemy.pos,
                    domeRadius - 3.0f,
                    Color{
                        255,
                        255,
                        255,
                        static_cast<unsigned char>(
                            90.0f +
                            progress *
                            100.0f
                        )
                    }
                );

                // Closing circle indicates when the
                // slam is about to begin.
                const float closingRadius =
                    std::max(
                        4.0f,
                        bossSlamRadius *
                        (
                            1.0f -
                            progress
                            )
                    );

                DrawGroundCircleLines(
                    enemy.pos,
                    closingRadius,
                    Color{
                        255,
                        255,
                        255,
                        225
                    }
                );
            }

            // ------------------------------------------
            // White impact burst
            // ------------------------------------------

            if (
                enemy.bossSlamImpactVisualTimer >
                0.0f
                )
            {
                const float lifeRatio =
                    Clamp(
                        enemy.bossSlamImpactVisualTimer /
                        std::max(
                            0.01f,
                            bossSlamImpactVisualDuration
                        ),
                        0.0f,
                        1.0f
                    );

                const float impactProgress =
                    1.0f -
                    lifeRatio;

                const float impactRadius =
                    bossSlamRadius *
                    (
                        1.0f +
                        impactProgress *
                        0.32f
                        );

                DrawGroundCircle(
                    enemy.pos,
                    impactRadius,
                    Color{
                        255,
                        255,
                        255,
                        static_cast<unsigned char>(
                            70.0f *
                            lifeRatio
                        )
                    }
                );

                DrawGroundCircleLines(
                    enemy.pos,
                    impactRadius,
                    Color{
                        255,
                        255,
                        255,
                        static_cast<unsigned char>(
                            255.0f *
                            lifeRatio
                        )
                    }
                );

                DrawGroundCircleLines(
                    enemy.pos,
                    impactRadius - 5.0f,
                    Color{
                        255,
                        255,
                        255,
                        static_cast<unsigned char>(
                            190.0f *
                            lifeRatio
                        )
                    }
                );
            }
        }

        EndBlendMode();
        EndMode2D();

        return;
    }

    // --------------------------------------------------
    // Hybrid 3D renderer
    //
    // This is rendered in screen space after lighting.
    // The projected filled ellipses recreate the previous
    // translucent dome appearance.
    // --------------------------------------------------

    auto GetProjectedEllipse =
        [this](
            Vector2 worldPosition,
            float worldRadius,
            Vector2& outCenter,
            float& outRadiusX,
            float& outRadiusY
            )
        {
            outCenter =
                GetWorldToScreen(
                    WorldToHybrid3D(
                        worldPosition,
                        4.0f
                    ),
                    hybridCamera
                );

            const Vector2 horizontalEdge =
                GetWorldToScreen(
                    WorldToHybrid3D(
                        {
                            worldPosition.x +
                                worldRadius,

                            worldPosition.y
                        },
                        4.0f
                    ),
                    hybridCamera
                );

            const Vector2 verticalEdge =
                GetWorldToScreen(
                    WorldToHybrid3D(
                        {
                            worldPosition.x,

                            worldPosition.y +
                                worldRadius
                        },
                        4.0f
                    ),
                    hybridCamera
                );

            outRadiusX =
                std::max(
                    1.0f,
                    Vector2Distance(
                        outCenter,
                        horizontalEdge
                    )
                );

            outRadiusY =
                std::max(
                    1.0f,
                    Vector2Distance(
                        outCenter,
                        verticalEdge
                    )
                );
        };

    BeginBlendMode(
        BLEND_ADDITIVE
    );

    for (const Enemy& enemy : enemies)
    {
        if (
            !enemy.active ||
            enemy.type !=
            EnemyType::Boss
            )
        {
            continue;
        }

        // ----------------------------------------------
        // White translucent warning dome
        // ----------------------------------------------

        if (
            enemy.bossActionState ==
            BossActionState::SlamWindup
            )
        {
            const float progress =
                1.0f -
                Clamp(
                    enemy.bossSlamWindupTimer /
                    std::max(
                        0.01f,
                        bossSlamWindupDuration
                    ),
                    0.0f,
                    1.0f
                );

            const float pulse =
                0.5f +
                0.5f *
                sinf(
                    progress *
                    PI *
                    5.0f
                );

            const float domeRadius =
                bossSlamRadius *
                (
                    0.92f +
                    progress *
                    0.08f
                    );

            Vector2 center{};
            float radiusX = 0.0f;
            float radiusY = 0.0f;

            GetProjectedEllipse(
                enemy.pos,
                domeRadius,
                center,
                radiusX,
                radiusY
            );

            // Main translucent dome body.
            DrawEllipse(
                static_cast<int>(
                    center.x
                    ),
                static_cast<int>(
                    center.y
                    ),
                radiusX,
                radiusY,
                Color{
                    255,
                    255,
                    255,
                    static_cast<unsigned char>(
                        28.0f +
                        progress *
                        24.0f
                    )
                }
            );

            // Inner glow layer.
            DrawEllipse(
                static_cast<int>(
                    center.x
                    ),
                static_cast<int>(
                    center.y
                    ),
                radiusX *
                0.72f,
                radiusY *
                0.72f,
                Color{
                    255,
                    255,
                    255,
                    static_cast<unsigned char>(
                        12.0f +
                        pulse *
                        18.0f
                    )
                }
            );

            // Outer glowing border.
            DrawEllipseLines(
                static_cast<int>(
                    center.x
                    ),
                static_cast<int>(
                    center.y
                    ),
                radiusX,
                radiusY,
                Color{
                    255,
                    255,
                    255,
                    static_cast<unsigned char>(
                        190.0f +
                        progress *
                        65.0f
                    )
                }
            );

            DrawEllipseLines(
                static_cast<int>(
                    center.x
                    ),
                static_cast<int>(
                    center.y
                    ),
                std::max(
                    1.0f,
                    radiusX - 3.0f
                ),
                std::max(
                    1.0f,
                    radiusY - 2.0f
                ),
                Color{
                    255,
                    255,
                    255,
                    static_cast<unsigned char>(
                        90.0f +
                        progress *
                        100.0f
                    )
                }
            );

            // Closing timing indicator.
            const float closingScale =
                std::max(
                    0.04f,
                    1.0f -
                    progress
                );

            DrawEllipseLines(
                static_cast<int>(
                    center.x
                    ),
                static_cast<int>(
                    center.y
                    ),
                radiusX *
                closingScale,
                radiusY *
                closingScale,
                Color{
                    255,
                    255,
                    255,
                    230
                }
            );
        }

        // ----------------------------------------------
        // White impact burst
        // ----------------------------------------------

        if (
            enemy.bossSlamImpactVisualTimer >
            0.0f
            )
        {
            const float lifeRatio =
                Clamp(
                    enemy.bossSlamImpactVisualTimer /
                    std::max(
                        0.01f,
                        bossSlamImpactVisualDuration
                    ),
                    0.0f,
                    1.0f
                );

            const float impactProgress =
                1.0f -
                lifeRatio;

            const float impactRadius =
                bossSlamRadius *
                (
                    1.0f +
                    impactProgress *
                    0.32f
                    );

            Vector2 center{};
            float radiusX = 0.0f;
            float radiusY = 0.0f;

            GetProjectedEllipse(
                enemy.pos,
                impactRadius,
                center,
                radiusX,
                radiusY
            );

            DrawEllipse(
                static_cast<int>(
                    center.x
                    ),
                static_cast<int>(
                    center.y
                    ),
                radiusX,
                radiusY,
                Color{
                    255,
                    255,
                    255,
                    static_cast<unsigned char>(
                        75.0f *
                        lifeRatio
                    )
                }
            );

            DrawEllipseLines(
                static_cast<int>(
                    center.x
                    ),
                static_cast<int>(
                    center.y
                    ),
                radiusX,
                radiusY,
                Color{
                    255,
                    255,
                    255,
                    static_cast<unsigned char>(
                        255.0f *
                        lifeRatio
                    )
                }
            );

            DrawEllipseLines(
                static_cast<int>(
                    center.x
                    ),
                static_cast<int>(
                    center.y
                    ),
                std::max(
                    1.0f,
                    radiusX - 5.0f
                ),
                std::max(
                    1.0f,
                    radiusY - 3.0f
                ),
                Color{
                    255,
                    255,
                    255,
                    static_cast<unsigned char>(
                        190.0f *
                        lifeRatio
                    )
                }
            );
        }
    }

    EndBlendMode();
}
void Game::DrawBossSlamDomeHybrid3D() const
{
    if (
        rendererMode !=
        WorldRendererMode::Hybrid3D
        )
    {
        return;
    }

    // Only draw when an actual slam impact is active.
    bool hasVisibleImpactDome = false;

    for (const Enemy& enemy : enemies)
    {
        if (
            enemy.active &&
            IsChamberVisible(
                enemy.chamberId
            ) &&
            enemy.type ==
            EnemyType::Boss &&
            enemy.bossSlamImpactVisualTimer >
            0.0f
            )
        {
            hasVisibleImpactDome = true;
            break;
        }
    }

    if (!hasVisibleImpactDome)
    {
        return;
    }

    BeginMode3D(
        hybridCamera
    );

    // This effect is drawn after the lighting composite.
    // Keep it unlit and visible as an overlay.
    rlDisableDepthTest();
    rlDisableDepthMask();
    rlDisableBackfaceCulling();

    BeginBlendMode(
        BLEND_ALPHA
    );

    auto DrawSolidHemisphere =
        [](
            Vector3 center,
            float radius,
            float heightScale,
            Color color
            )
        {
            constexpr int longitudeSegments =
                40;

            constexpr int latitudeSegments =
                14;

            // Only filled triangles.
            // There is intentionally no RL_LINES pass.
            rlBegin(
                RL_TRIANGLES
            );

            rlColor4ub(
                color.r,
                color.g,
                color.b,
                color.a
            );

            for (
                int latitude = 0;
                latitude <
                latitudeSegments;
                ++latitude
                )
            {
                const float latitude0 =
                    static_cast<float>(
                        latitude
                        ) /
                    static_cast<float>(
                        latitudeSegments
                        ) *
                    PI *
                    0.5f;

                const float latitude1 =
                    static_cast<float>(
                        latitude + 1
                        ) /
                    static_cast<float>(
                        latitudeSegments
                        ) *
                    PI *
                    0.5f;

                const float ringRadius0 =
                    cosf(latitude0) *
                    radius;

                const float ringRadius1 =
                    cosf(latitude1) *
                    radius;

                const float height0 =
                    sinf(latitude0) *
                    radius *
                    heightScale;

                const float height1 =
                    sinf(latitude1) *
                    radius *
                    heightScale;

                for (
                    int longitude = 0;
                    longitude <
                    longitudeSegments;
                    ++longitude
                    )
                {
                    const float angle0 =
                        static_cast<float>(
                            longitude
                            ) /
                        static_cast<float>(
                            longitudeSegments
                            ) *
                        PI *
                        2.0f;

                    const float angle1 =
                        static_cast<float>(
                            longitude + 1
                            ) /
                        static_cast<float>(
                            longitudeSegments
                            ) *
                        PI *
                        2.0f;

                    const Vector3 point00{
                        center.x +
                            cosf(angle0) *
                            ringRadius0,

                        center.y +
                            height0,

                        center.z +
                            sinf(angle0) *
                            ringRadius0
                    };

                    const Vector3 point01{
                        center.x +
                            cosf(angle1) *
                            ringRadius0,

                        center.y +
                            height0,

                        center.z +
                            sinf(angle1) *
                            ringRadius0
                    };

                    const Vector3 point10{
                        center.x +
                            cosf(angle0) *
                            ringRadius1,

                        center.y +
                            height1,

                        center.z +
                            sinf(angle0) *
                            ringRadius1
                    };

                    const Vector3 point11{
                        center.x +
                            cosf(angle1) *
                            ringRadius1,

                        center.y +
                            height1,

                        center.z +
                            sinf(angle1) *
                            ringRadius1
                    };

                    // First triangle.
                    rlVertex3f(
                        point00.x,
                        point00.y,
                        point00.z
                    );

                    rlVertex3f(
                        point10.x,
                        point10.y,
                        point10.z
                    );

                    rlVertex3f(
                        point11.x,
                        point11.y,
                        point11.z
                    );

                    // Second triangle.
                    rlVertex3f(
                        point00.x,
                        point00.y,
                        point00.z
                    );

                    rlVertex3f(
                        point11.x,
                        point11.y,
                        point11.z
                    );

                    rlVertex3f(
                        point01.x,
                        point01.y,
                        point01.z
                    );
                }
            }

            rlEnd();
        };

    for (const Enemy& enemy : enemies)
    {
        if (
            !enemy.active ||
            !IsChamberVisible(
                enemy.chamberId
            ) ||
            enemy.type !=
            EnemyType::Boss ||
            enemy.bossSlamImpactVisualTimer <=
            0.0f
            )
        {
            continue;
        }

        const float lifeRatio =
            Clamp(
                enemy.bossSlamImpactVisualTimer /
                std::max(
                    0.01f,
                    bossSlamImpactVisualDuration
                ),
                0.0f,
                1.0f
            );

        const float impactProgress =
            1.0f -
            lifeRatio;

        // Starts near the attack radius and expands
        // slightly after the slam.
        const float radiusPixels =
            bossSlamRadius *
            (
                0.94f +
                impactProgress *
                0.24f
                );

        const float radiusUnits =
            PixelsToHybridUnits(
                radiusPixels
            );

        const Vector3 domeCenter =
            WorldToHybrid3D(
                enemy.pos,

                // Keep the base slightly above the terrain
                // to avoid visual z-fighting.
                3.0f
            );

        // Strong at impact, then fades out.
        const unsigned char alpha =
            static_cast<unsigned char>(
                125.0f *
                lifeRatio *
                lifeRatio
                );

        DrawSolidHemisphere(
            domeCenter,
            radiusUnits,

            // Controls the dome height.
            // 1.0 = round hemisphere.
            // Lower = flatter dome.
            0.72f,

            Color{
                255,
                255,
                255,
                alpha
            }
        );
    }

    EndBlendMode();

    rlEnableBackfaceCulling();
    rlEnableDepthMask();
    rlEnableDepthTest();

    EndMode3D();
}

float Game::GetBossInterruptThreshold(
    const Enemy& enemy
) const
{
    return
        static_cast<float>(
            std::max(
                1,
                enemy.maxHp
            )
            ) *
        bossInterruptHealthRatio;
}

float Game::GetBossSpecialStunDuration(
    const Enemy& enemy
) const
{
    switch (
        GetBossPowerTier(
            enemy
        )
        )
    {
    case 0:
        return 4.50f;

    case 1:
        return 3.50f;

    case 2:
        return 2.20f;

        // At 25% HP and below, damage can no longer
        // stagger the Boss.
    case 3:
    default:
        return 0.0f;
    }
}

bool Game::StartBossStunned(
    Enemy& enemy,
    float requestedDuration,
    bool forcedStun
)
{
    if (
        enemy.type !=
        EnemyType::Boss ||
        !enemy.active
        )
    {
        return false;
    }

    // Ordinary damage-based stagger is disabled
    // at the final 25% HP tier.
    if (
        !forcedStun &&
        GetBossPowerTier(enemy) >= 3
        )
    {
        enemy.bossInterruptDamage =
            0.0f;

        return false;
    }

    const float duration =
        std::max(
            0.10f,
            requestedDuration
        );

    enemy.bossActionState =
        BossActionState::Stunned;

    enemy.bossStunnedTimer =
        duration;

    enemy.bossInterruptDamage =
        0.0f;

    // Cancel all attacks and movement.
    enemy.bossDashCharging = false;
    enemy.bossDashChargeTimer = 0.0f;
    enemy.bossDashTimer = 0.0f;
    enemy.bossDashVelocity = {
        0.0f,
        0.0f
    };

    enemy.bossDashPurpose =
        BossDashPurpose::None;

    enemy.bossDashIsForward =
        false;

    enemy.bossLaserWindupTimer =
        0.0f;

    enemy.bossLaserActiveTimer =
        0.0f;

    enemy.bossLaserDamageTimer =
        0.0f;

    enemy.bossBombardmentTimer =
        0.0f;

    enemy.bossBombardmentSpawnTimer =
        0.0f;

    enemy.bossChargeWindupTimer =
        0.0f;

    enemy.bossChargeRemainingTimer =
        0.0f;

    enemy.bossChargeHitPlayer =
        false;

    enemy.attackTimer =
        0.0f;

    enemy.stunTimer =
        0.0f;

    enemy.path.clear();
    enemy.pathIndex = 0;

    enemy.animationState =
        EnemyAnimationState::Attacking;

    enemy.bossAnimationFrame = 0;
    enemy.bossAnimationTimer = 0.0f;

    // Interrupting the roar also removes rocks that
    // have not reached the ground yet.
    for (
        BossFallingRock& rock :
        bossFallingRocks
        )
    {
        if (
            rock.active &&
            rock.ownerBossId ==
            enemy.id
            )
        {
            rock.active = false;
        }
    }

    SpawnHitSpark(
        enemy.pos
    );

    return true;
}

int Game::GetBossPowerTier(
    const Enemy& enemy
) const
{
    if (
        enemy.type !=
        EnemyType::Boss ||
        enemy.maxHp <= 0
        )
    {
        return 0;
    }

    const float healthRatio =
        Clamp(
            static_cast<float>(
                enemy.hp
                ) /
            static_cast<float>(
                enemy.maxHp
                ),
            0.0f,
            1.0f
        );

    if (healthRatio <= 0.25f)
    {
        return 3;
    }

    if (healthRatio <= 0.50f)
    {
        return 2;
    }

    if (healthRatio <= 0.75f)
    {
        return 1;
    }

    return 0;
}

float Game::GetBossMovementSpeedMultiplier(
    const Enemy& enemy
) const
{
    return
        1.0f +
        static_cast<float>(
            GetBossPowerTier(enemy)
            ) *
        bossMovementSpeedPerTier;
}

float Game::GetBossAttackSpeedMultiplier(
    const Enemy& enemy
) const
{
    return
        1.0f +
        static_cast<float>(
            GetBossPowerTier(enemy)
            ) *
        bossAttackSpeedPerTier;
}

float Game::GetBossDashPowerMultiplier(
    const Enemy& enemy
) const
{
    return
        1.0f +
        static_cast<float>(
            GetBossPowerTier(enemy)
            ) *
        bossDashPowerPerTier;
}

float Game::GetBossDashDurationMultiplier(
    const Enemy& enemy
) const
{
    return
        1.0f +
        static_cast<float>(
            GetBossPowerTier(enemy)
            ) *
        bossDashDurationPerTier;
}
Color Game::GetBossTierTint(
    const Enemy& enemy
) const
{
    if (
        enemy.type !=
        EnemyType::Boss ||
        enemy.maxHp <= 0
        )
    {
        return WHITE;
    }

    const float healthRatio =
        Clamp(
            static_cast<float>(
                enemy.hp
                ) /
            static_cast<float>(
                enemy.maxHp
                ),
            0.0f,
            1.0f
        );

    // 25% HP or lower: yellow.
    if (healthRatio <= 0.25f)
    {
        return Color{
            255,
            235,
            115,
            255
        };
    }

    // 50% HP or lower: orange.
    if (healthRatio <= 0.50f)
    {
        return Color{
            255,
            180,
            105,
            255
        };
    }

    // 75% HP or lower: red.
    if (healthRatio <= 0.75f)
    {
        return Color{
            255,
            125,
            125,
            255
        };
    }

    // Above 75%: original sprite colors.
    return WHITE;
}

void Game::StartBossBombardment(
    Enemy& enemy
)
{
    if (
        enemy.type !=
        EnemyType::Boss ||
        enemy.bossActionState !=
        BossActionState::None ||
        enemy.bossDashCharging ||
        enemy.bossDashTimer > 0.0f ||
        enemy.bossBombardmentCooldownTimer >
        0.0f
        )
    {
        return;
    }

    Vector2 awayDirection =
        Vector2Subtract(
            enemy.pos,
            playerPosition
        );

    if (
        Vector2Length(
            awayDirection
        ) <= 0.01f
        )
    {
        awayDirection = {
            1.0f,
            0.0f
        };
    }
    else
    {
        awayDirection =
            Vector2Normalize(
                awayDirection
            );
    }

    enemy.bossDashPurpose =
        BossDashPurpose::BombardmentEscape;

    // Keep this true so your existing Boss dash
    // afterimage system remains active.
    enemy.bossDashIsForward =
        true;

    enemy.bossDashTimer =
        bossBombardmentRetreatDuration *
        GetBossDashDurationMultiplier(
            enemy
        );

    enemy.bossDashVelocity =
        Vector2Scale(
            awayDirection,
            bossBombardmentRetreatSpeed *
            GetBossDashPowerMultiplier(
                enemy
            )
        );

    enemy.bossBombardmentCooldownTimer =
        bossBombardmentCooldown;

    enemy.bossDashAfterimageTimer =
        0.0f;

    enemy.path.clear();
    enemy.pathIndex = 0;

    enemy.animationState =
        EnemyAnimationState::Walking;

    SetEnemyFacingFromWorldDirection(
        enemy,
        awayDirection
    );

    SpawnBossDashAfterimage(
        enemy
    );
}

void Game::StartBossBombardmentRoar(
    Enemy& enemy
)
{
    if (
        enemy.type !=
        EnemyType::Boss ||
        !enemy.active ||
        enemy.bossActionState !=
        BossActionState::None ||
        enemy.bossDashCharging ||
        enemy.bossDashTimer >
        0.0f
        )
    {
        return;
    }

    enemy.bossBombardmentCooldownTimer =
        bossBombardmentCooldown;

    enemy.bossActionState =
        BossActionState::BombardmentRoar;

    enemy.bossBombardmentTimer =
        bossBombardmentDuration;

    enemy.bossBombardmentSpawnTimer =
        0.15f;

    enemy.bossInterruptDamage =
        0.0f;

    enemy.animationState =
        EnemyAnimationState::Attacking;

    enemy.bossAnimationFrame = 0;
    enemy.bossAnimationTimer = 0.0f;

    enemy.path.clear();
    enemy.pathIndex = 0;

    SetEnemyFacingFromWorldDirection(
        enemy,
        Vector2Subtract(
            playerPosition,
            enemy.pos
        )
    );
}

void Game::UpdateBossBombardment(
    Enemy& enemy,
    float dt
)
{
    if (
        enemy.bossActionState !=
        BossActionState::BombardmentRoar
        )
    {
        return;
    }

    const float attackSpeed =
        GetBossAttackSpeedMultiplier(
            enemy
        );

    enemy.bossBombardmentTimer -=
        dt;

    enemy.bossBombardmentSpawnTimer -=
        dt *
        attackSpeed;

    while (
        enemy.bossBombardmentSpawnTimer <=
        0.0f &&
        enemy.bossBombardmentTimer >
        0.0f
        )
    {
        SpawnBossFallingRock(
            enemy
        );

        // At tier 2 and above, occasionally create
        // a second simultaneous rock.
        if (
            GetBossPowerTier(enemy) >= 2 &&
            GetRandomValue(0, 99) < 35
            )
        {
            SpawnBossFallingRock(
                enemy
            );
        }

        enemy.bossBombardmentSpawnTimer +=
            bossBombardmentRockInterval;
    }

    if (
        enemy.bossBombardmentTimer <=
        0.0f
        )
    {
        enemy.bossBombardmentTimer =
            0.0f;

        enemy.bossActionState =
            BossActionState::None;

        enemy.animationState =
            EnemyAnimationState::Idle;

        enemy.bossAnimationFrame = 0;
        enemy.bossAnimationTimer = 0.0f;

        enemy.bossInterruptDamage =
            0.0f;

        RefreshEnemyPath(
            enemy
        );
    }
}

void Game::SpawnBossFallingRock(
    const Enemy& enemy
)
{
    const float randomAngle =
        static_cast<float>(
            GetRandomValue(
                0,
                359
            )
            ) *
        DEG2RAD;

    // Square root gives a more even distribution
    // across the target area.
    const float randomRatio =
        sqrtf(
            static_cast<float>(
                GetRandomValue(
                    0,
                    1000
                )
                ) /
            1000.0f
        );

    const float randomDistance =
        randomRatio *
        bossBombardmentTargetSpread;

    Vector2 requestedPosition{
        playerPosition.x +
            cosf(randomAngle) *
            randomDistance,

        playerPosition.y +
            sinf(randomAngle) *
            randomDistance
    };

    int cellX = 0;
    int cellY = 0;

    if (
        FindNearestWalkableCell(
            requestedPosition,
            cellX,
            cellY
        )
        )
    {
        requestedPosition =
            CellToWorld(
                cellX,
                cellY
            );
    }

    BossFallingRock rock;

    rock.ownerBossId =
        enemy.id;

    rock.position =
        requestedPosition;

    rock.terrainElevation =
        GetTerrainElevationAtWorld(
            requestedPosition
        );

    const float randomFallDuration =
        static_cast<float>(
            GetRandomValue(
                static_cast<int>(
                    bossRockFallDurationMin *
                    1000.0f
                    ),
                static_cast<int>(
                    bossRockFallDurationMax *
                    1000.0f
                    )
            )
            ) /
        1000.0f;

    rock.fallDuration =
        randomFallDuration;

    rock.timer =
        randomFallDuration;

    rock.startHeight =
        bossRockStartHeight;

    rock.visualRadius =
        bossRockVisualRadius +
        static_cast<float>(
            GetRandomValue(
                -7,
                9
            )
            );

    rock.damageRadius =
        static_cast<float>(
            GetRandomValue(
                static_cast<int>(
                    bossRockDamageRadiusMin
                    ),
                static_cast<int>(
                    bossRockDamageRadiusMax
                    )
            )
            );

    rock.damage =
        std::max(
            1,
            enemy.bulletDamage *
            2
        );

    rock.active =
        true;

    bossFallingRocks.push_back(
        rock
    );
}

void Game::UpdateBossFallingRocks(
    float dt
)
{
    for (
        BossFallingRock& rock :
        bossFallingRocks
        )
    {
        if (!rock.active)
        {
            continue;
        }

        rock.timer -=
            dt;

        if (rock.timer > 0.0f)
        {
            continue;
        }

        rock.timer =
            0.0f;

        const bool sameTerrainLevel =
            GetTerrainElevationAtWorld(
                playerPosition
            ) ==
            rock.terrainElevation;

        const float hitDistance =
            Vector2Distance(
                playerPosition,
                rock.position
            );

        if (
            sameTerrainLevel &&
            hitDistance <=
            rock.damageRadius +
            player.radius &&
            !IsPlayerAirborne() &&
            !IsPlayerInvulnerable()
            )
        {
            ApplyKnockbackToPlayer(
                rock.position,
                850.0f,
                0.30f
            );

            DamagePlayer(
                rock.damage
            );
        }

        VfxParticle impact;

        impact.type =
            VfxType::Explosion;

        impact.pos =
            rock.position;

        impact.radius =
            rock.damageRadius;

        impact.life =
            0.32f;

        impact.maxLife =
            impact.life;

        impact.color = {
            150,
            125,
            95,
            220
        };

        impact.active =
            true;

        vfxParticles.push_back(
            impact
        );

        rock.active =
            false;
    }
}

void Game::DrawBossFallingRockTelegraphsHybrid3D() const
{
    // Draw a solid circular ground area using hybridCircleTexture.
    auto DrawDamageArea =
        [this](
            Vector2 worldPosition,
            float radiusPixels,
            Color color,
            float additionalHeightPixels
            )
        {
            if (
                radiusPixels <= 0.0f ||
                hybridCircleTexture.id == 0
                )
            {
                return;
            }

            const Vector3 center =
                WorldToHybrid3D(
                    worldPosition,
                    additionalHeightPixels +
                    0.65f
                );

            const float radius =
                PixelsToHybridUnits(
                    radiusPixels
                );

            const Vector3 northWest{
                center.x - radius,
                center.y,
                center.z - radius
            };

            const Vector3 northEast{
                center.x + radius,
                center.y,
                center.z - radius
            };

            const Vector3 southEast{
                center.x + radius,
                center.y,
                center.z + radius
            };

            const Vector3 southWest{
                center.x - radius,
                center.y,
                center.z + radius
            };

            rlSetTexture(
                hybridCircleTexture.id
            );

            rlBegin(
                RL_QUADS
            );

            rlColor4ub(
                color.r,
                color.g,
                color.b,
                color.a
            );

            rlTexCoord2f(
                0.0f,
                0.0f
            );

            rlVertex3f(
                northWest.x,
                northWest.y,
                northWest.z
            );

            rlTexCoord2f(
                0.0f,
                1.0f
            );

            rlVertex3f(
                southWest.x,
                southWest.y,
                southWest.z
            );

            rlTexCoord2f(
                1.0f,
                1.0f
            );

            rlVertex3f(
                southEast.x,
                southEast.y,
                southEast.z
            );

            rlTexCoord2f(
                1.0f,
                0.0f
            );

            rlVertex3f(
                northEast.x,
                northEast.y,
                northEast.z
            );

            rlEnd();

            rlSetTexture(
                0
            );
        };

    for (
        const BossFallingRock& rock :
        bossFallingRocks
        )
    {
        if (!rock.active)
        {
            continue;
        }

        const float progress =
            1.0f -
            Clamp(
                rock.timer /
                std::max(
                    0.01f,
                    rock.fallDuration
                ),
                0.0f,
                1.0f
            );

        // --------------------------------------------------
        // Full damage radius
        //
        // Uses the hard circle texture so the entire affected
        // area remains visible instead of fading away like a
        // normal soft shadow.
        // --------------------------------------------------

        DrawDamageArea(
            rock.position,
            rock.damageRadius,
            Color{
                35,
                12,
                12,
                static_cast<unsigned char>(
                    105.0f +
                    progress *
                        35.0f
                )
            },
            1.0f
        );

        // Slightly smaller secondary layer helps the dangerous
        // area remain readable over bright terrain.
        DrawDamageArea(
            rock.position,
            rock.damageRadius *
            0.92f,
            Color{
                65,
                15,
                12,
                static_cast<unsigned char>(
                    35.0f +
                    progress *
                        30.0f
                )
            },
            1.2f
        );

        // --------------------------------------------------
        // Falling-rock shadow
        //
        // This remains soft and grows as the rock approaches.
        // --------------------------------------------------

        const float shadowRadius =
            rock.visualRadius *
            (
                0.45f +
                progress *
                1.15f
                );

        DrawHybridGroundDisc(
            rock.position,
            shadowRadius,
            Color{
                0,
                0,
                0,
                static_cast<unsigned char>(
                    110.0f +
                    progress *
                        125.0f
                )
            },
            1.8f,
            24
        );
    }
}

void Game::DrawBossFallingRockTelegraphs2D() const
{
    for (
        const BossFallingRock& rock :
        bossFallingRocks
        )
    {
        if (!rock.active)
        {
            continue;
        }

        const float progress =
            1.0f -
            Clamp(
                rock.timer /
                std::max(
                    0.01f,
                    rock.fallDuration
                ),
                0.0f,
                1.0f
            );

        DrawGroundCircle(
            rock.position,
            rock.damageRadius,
            Color{
                20,
                15,
                15,
                static_cast<unsigned char>(
                    65.0f +
                    progress *
                    55.0f
                )
            }
        );

        DrawGroundCircle(
            rock.position,
            rock.visualRadius *
            (
                0.45f +
                progress *
                1.15f
                ),
            Color{
                0,
                0,
                0,
                static_cast<unsigned char>(
                    100.0f +
                    progress *
                    120.0f
                )
            }
        );
    }
}
void Game::DrawBossFallingRocksHybrid3D() const
{
    for (
        const BossFallingRock& rock :
        bossFallingRocks
        )
    {
        if (!rock.active)
        {
            continue;
        }

        const float heightRatio =
            Clamp(
                rock.timer /
                std::max(
                    0.01f,
                    rock.fallDuration
                ),
                0.0f,
                1.0f
            );

        // Accelerating fall.
        const float curvedHeightRatio =
            heightRatio *
            heightRatio;

        const float height =
            rock.startHeight *
            curvedHeightRatio +
            rock.visualRadius;

        const Vector3 rockPosition =
            WorldToHybrid3D(
                rock.position,
                height
            );

        DrawSphere(
            rockPosition,
            PixelsToHybridUnits(
                rock.visualRadius
            ),
            Color{
                95,
                82,
                70,
                255
            }
        );

        // Smaller offset sphere makes the rock less perfectly round.
        DrawSphere(
            Vector3{
                rockPosition.x +
                    PixelsToHybridUnits(
                        rock.visualRadius *
                        0.28f
                    ),

                rockPosition.y +
                    PixelsToHybridUnits(
                        rock.visualRadius *
                        0.14f
                    ),

                rockPosition.z -
                    PixelsToHybridUnits(
                        rock.visualRadius *
                        0.18f
                    )
            },
            PixelsToHybridUnits(
                rock.visualRadius *
                0.62f
            ),
            Color{
                72,
                62,
                54,
                255
            }
        );
    }
}

void Game::DrawBossFallingRocks2D() const
{
    for (
        const BossFallingRock& rock :
        bossFallingRocks
        )
    {
        if (!rock.active)
        {
            continue;
        }

        const float heightRatio =
            Clamp(
                rock.timer /
                std::max(
                    0.01f,
                    rock.fallDuration
                ),
                0.0f,
                1.0f
            );

        const float height =
            rock.startHeight *
            heightRatio *
            heightRatio;

        const Vector2 drawPosition =
            WorldToViewElevated(
                rock.position,
                height
            );

        DrawCircleV(
            drawPosition,
            rock.visualRadius,
            Color{
                95,
                82,
                70,
                255
            }
        );

        DrawCircleV(
            Vector2Add(
                drawPosition,
                {
                    rock.visualRadius *
                        0.25f,

                    -rock.visualRadius *
                        0.18f
                }
            ),
            rock.visualRadius *
            0.55f,
            Color{
                70,
                60,
                52,
                255
            }
        );
    }
}

void Game::StartBossCharge(
    Enemy& enemy
)
{
    if (
        enemy.type !=
        EnemyType::Boss ||
        enemy.bossActionState !=
        BossActionState::None ||
        enemy.bossDashCharging ||
        enemy.bossDashTimer > 0.0f ||
        enemy.bossChargeCooldownTimer >
        0.0f
        )
    {
        return;
    }

    Vector2 direction =
        Vector2Subtract(
            playerPosition,
            enemy.pos
        );

    if (
        Vector2Length(
            direction
        ) <= 0.01f
        )
    {
        direction = {
            1.0f,
            0.0f
        };
    }
    else
    {
        direction =
            Vector2Normalize(
                direction
            );
    }

    enemy.bossChargeDirection =
        direction;

    enemy.bossActionState =
        BossActionState::ChargeWindup;

    // The pre-charge duration is now controlled by
    // completing the animation exactly twice.
    enemy.bossChargeWindupTimer =
        0.0f;

    enemy.bossPreChargeLoopsCompleted =
        0;

    enemy.bossChargeRemainingTimer =
        bossChargeMaximumDuration;

    enemy.bossChargeCooldownTimer =
        bossChargeCooldown;

    enemy.bossChargeHitPlayer =
        false;


    enemy.animationState =
        EnemyAnimationState::Attacking;

    // Begin the directional pre-charge animation.
    enemy.bossAnimationFrame = 0;
    enemy.bossAnimationTimer = 0.0f;

    enemy.path.clear();
    enemy.pathIndex = 0;

    SetEnemyFacingFromWorldDirection(
        enemy,
        direction
    );
}

void Game::UpdateBossCharge(
    Enemy& enemy,
    float dt
)
{
    if (
        enemy.bossActionState ==
        BossActionState::ChargeWindup
        )
    {
        // UpdateBossAnimation() controls the pre-charge
        // frames and changes the state after two loops.
        return;
    }

    if (
        enemy.bossActionState !=
        BossActionState::ChargeActive
        )
    {
        return;
    }

    enemy.bossChargeRemainingTimer -=
        dt;

    const float chargeSpeed =
        bossChargeSpeed *
        GetBossDashPowerMultiplier(
            enemy
        );

    const Vector2 totalMovement =
        Vector2Scale(
            enemy.bossChargeDirection,
            chargeSpeed *
            dt
        );

    const int stepCount =
        std::max(
            1,
            static_cast<int>(
                std::ceil(
                    Vector2Length(
                        totalMovement
                    ) /
                    7.0f
                )
                )
        );

    const Vector2 movementStep =
        Vector2Scale(
            totalMovement,
            1.0f /
            static_cast<float>(
                stepCount
                )
        );

    bool hitWall = false;

    for (
        int stepIndex = 0;
        stepIndex < stepCount;
        ++stepIndex
        )
    {
        const Vector2 candidatePosition =
            Vector2Add(
                enemy.pos,
                movementStep
            );

        if (
            !CanEnemyStandAt(
                enemy.pos,
                candidatePosition,
                std::max(
                    8.0f,
                    enemy.radius *
                    0.60f
                )
            )
            )
        {
            hitWall = true;
            break;
        }

        enemy.pos =
            candidatePosition;

        const bool sameTerrainLevel =
            GetTerrainElevationAtWorld(
                enemy.pos
            ) ==
            GetTerrainElevationAtWorld(
                playerPosition
            );

        if (
            !enemy.bossChargeHitPlayer &&
            sameTerrainLevel &&
            CheckCollisionCircles(
                enemy.pos,
                enemy.radius *
                0.72f,
                playerPosition,
                player.radius
            ) &&
            !IsPlayerInvulnerable()
            )
        {
            ApplyKnockbackToPlayer(
                enemy.pos,
                bossChargePlayerKnockbackForce,
                bossChargePlayerKnockbackDuration
            );

            DamagePlayer(
                std::max(
                    1,
                    enemy.contactDamage *
                    2
                )
            );

            enemy.bossChargeHitPlayer =
                true;
        }
    }
    /*
        enemy.bossDashAfterimageTimer -=
        dt *
        GetBossDashPowerMultiplier(
            enemy
        );

    while (
        enemy.bossDashAfterimageTimer <=
        0.0f
        )
    {
        SpawnBossDashAfterimage(
            enemy
        );

        enemy.bossDashAfterimageTimer +=
            bossDashAfterimageInterval;
    }
    */


    // Normally this triggers because the collision test
    // found an obstacle or terrain wall.
    //
    // The timer is an emergency guard against the Boss
    // travelling indefinitely on a malformed map.
    if (
        hitWall ||
        enemy.bossChargeRemainingTimer <=
        0.0f
        )
    {
        SpawnHitSpark(
            enemy.pos
        );

        StartBossStunned(
            enemy,
            bossChargeWallStunDuration,
            true
        );
    }
}

void Game::DrawEnemySpawnGroundEffects2D() const
{
    if (
        !enemySpawnEffectSpriteLoaded ||
        enemySpawnEffectSpriteSheet.id ==
        0
        )
    {
        return;
    }

    BeginBlendMode(
        BLEND_ALPHA
    );

    for (
        const Enemy& enemy :
        enemies
        )
    {
        if (
            !enemy.active ||
            !IsChamberVisible(
                enemy.chamberId
            ) ||
            enemy.type ==
            EnemyType::Boss ||
            enemy.spawnState ==
            EnemySpawnState::Ready
            )
        {
            continue;
        }

        // GroundEffect animates through the sheet.
 // Emerging and FadeOut use the final frame.
        int frame =
            enemySpawnEffectFrameCount -
            1;

        unsigned char alpha =
            255;

        if (
            enemy.spawnState ==
            EnemySpawnState::GroundEffect
            )
        {
            const float progress =
                Clamp(
                    enemy.spawnStateTimer /
                    std::max(
                        0.01f,
                        enemySpawnGroundEffectDuration
                    ),
                    0.0f,
                    1.0f
                );

            frame =
                std::min(
                    enemySpawnEffectFrameCount -
                    1,

                    static_cast<int>(
                        progress *
                        static_cast<float>(
                            enemySpawnEffectFrameCount
                            )
                        )
                );
        }
        else if (
            enemy.spawnState ==
            EnemySpawnState::Emerging
            )
        {
            // The final frame remains completely visible until
            // the enemy is fully above the ground.
            frame =
                enemySpawnEffectFrameCount -
                1;

            alpha =
                255;
        }
        else if (
            enemy.spawnState ==
            EnemySpawnState::FadeOut
            )
        {
            frame =
                enemySpawnEffectFrameCount -
                1;

            const float fadeProgress =
                Clamp(
                    enemy.spawnStateTimer /
                    std::max(
                        0.01f,
                        enemySpawnFadeDuration
                    ),
                    0.0f,
                    1.0f
                );

            const float smoothFade =
                fadeProgress *
                fadeProgress *
                (
                    3.0f -
                    2.0f *
                    fadeProgress
                    );

            alpha =
                static_cast<unsigned char>(
                    255.0f *
                    (
                        1.0f -
                        smoothFade
                        )
                    );
        }

        const float visualSize =
            std::max(
                enemySpawnEffectVisualSize,
                enemy.radius *
                4.50f
            );

        const float halfSize =
            visualSize *
            0.5f;

        const Vector2 northWest{
            enemy.pos.x - halfSize,
            enemy.pos.y - halfSize
        };

        const Vector2 northEast{
            enemy.pos.x + halfSize,
            enemy.pos.y - halfSize
        };

        const Vector2 southEast{
            enemy.pos.x + halfSize,
            enemy.pos.y + halfSize
        };

        const Vector2 southWest{
            enemy.pos.x - halfSize,
            enemy.pos.y + halfSize
        };

        DrawTextureFrameOnQuad2D(
            enemySpawnEffectSpriteSheet,
            GetEnemySpawnEffectSourceRect(
                frame
            ),
            WorldToViewElevated(
                northWest,
                1.2f
            ),
            WorldToViewElevated(
                northEast,
                1.2f
            ),
            WorldToViewElevated(
                southEast,
                1.2f
            ),
            WorldToViewElevated(
                southWest,
                1.2f
            ),
            Color{
                255,
                255,
                255,
                alpha
            }
        );
    }

    EndBlendMode();
}

void Game::StartBossHealing(
    Enemy& enemy
)
{
    if (
        enemy.type !=
        EnemyType::Boss ||
        !enemy.active ||
        enemy.bossActionState !=
        BossActionState::None ||
        enemy.bossHealingUseCount >=
        bossHealingMaximumUses
        )
    {
        return;
    }

    enemy.bossHealingUseCount++;

    enemy.bossActionState =
        BossActionState::HealingWindup;

    enemy.animationState =
        EnemyAnimationState::Attacking;

    enemy.bossAnimationFrame =
        0;

    enemy.bossAnimationTimer =
        0.0f;

    enemy.bossHealingTimer =
        0.0f;

    enemy.bossHealingWaveTimer =
        0.0f;

    enemy.bossHealingWavesSpawned =
        0;

    enemy.bossHealingAmountApplied =
        0;

    enemy.bossHealingTargetAmount =
        std::max(
            1,
            static_cast<int>(
                static_cast<float>(
                    enemy.maxHp
                    ) *
                bossHealingAmountRatio +
                0.5f
                )
        );

    // Completely stop all movement.
    enemy.path.clear();
    enemy.pathIndex = 0;
    enemy.pathRefreshTimer = 0.0f;

    enemy.velocity = {
        0.0f,
        0.0f
    };

    enemy.knockbackVelocity = {
        0.0f,
        0.0f
    };

    enemy.bossDashCharging =
        false;

    enemy.bossDashChargeTimer =
        0.0f;

    enemy.bossDashTimer =
        0.0f;

    enemy.bossDashVelocity = {
        0.0f,
        0.0f
    };

    enemy.bossDashPurpose =
        BossDashPurpose::None;

    enemy.bossDashIsForward =
        false;

    // Healing cannot be stopped by ordinary crowd control.
    enemy.stunTimer =
        0.0f;

    enemy.frozenTimer =
        0.0f;

    enemy.slowTimer =
        0.0f;

    enemy.airborneTimer =
        0.0f;

    enemy.airborneMaxTimer =
        0.0f;

    enemy.landingStunTimer =
        0.0f;

    enemy.landingStunOnLand =
        0.0f;

    enemy.visualHeight =
        0.0f;

    enemy.bossInterruptDamage =
        0.0f;
}

void Game::UpdateBossHealing(
    Enemy& enemy,
    float dt
)
{
    if (
        enemy.bossActionState !=
        BossActionState::HealingWindup &&
        enemy.bossActionState !=
        BossActionState::HealingActive
        )
    {
        return;
    }

    // Keep the Boss completely stationary throughout
    // both healing states.
    enemy.path.clear();
    enemy.pathIndex = 0;
    enemy.pathRefreshTimer = 0.0f;

    enemy.velocity = {
        0.0f,
        0.0f
    };

    enemy.knockbackVelocity = {
        0.0f,
        0.0f
    };

    enemy.stunTimer = 0.0f;
    enemy.frozenTimer = 0.0f;
    enemy.slowTimer = 0.0f;

    enemy.airborneTimer = 0.0f;
    enemy.airborneMaxTimer = 0.0f;
    enemy.landingStunTimer = 0.0f;
    enemy.landingStunOnLand = 0.0f;
    enemy.visualHeight = 0.0f;

    // Prehealing is controlled by UpdateBossAnimation().
    if (
        enemy.bossActionState ==
        BossActionState::HealingWindup
        )
    {
        return;
    }

    const float safeDuration =
        std::max(
            0.10f,
            bossHealingDuration
        );

    enemy.bossHealingTimer =
        std::min(
            safeDuration,
            enemy.bossHealingTimer +
            dt
        );

    // --------------------------------------------------
    // Gradual healing
    //
    // The amount that should have been restored by this
    // point in the 20-second period is calculated, then
    // only the missing difference is applied.
    // --------------------------------------------------

    const float healingProgress =
        Clamp(
            enemy.bossHealingTimer /
            safeDuration,
            0.0f,
            1.0f
        );

    const int desiredHealingAmount =
        std::min(
            enemy.bossHealingTargetAmount,

            static_cast<int>(
                static_cast<float>(
                    enemy.bossHealingTargetAmount
                    ) *
                healingProgress
                )
        );

    const int healingThisFrame =
        desiredHealingAmount -
        enemy.bossHealingAmountApplied;

    if (healingThisFrame > 0)
    {
        enemy.hp =
            std::min(
                enemy.maxHp,
                enemy.hp +
                healingThisFrame
            );

        enemy.bossHealingAmountApplied =
            desiredHealingAmount;
    }

    // --------------------------------------------------
 // Sequential healing minion waves
 //
 // Wave 1 begins immediately.
 //
 // Each later wave may begin only after every minion
 // belonging to the previous wave has been defeated.
 // --------------------------------------------------

    if (
        enemy.bossHealingWavesSpawned <=
        0
        )
    {
        const int firstWaveNumber =
            1;

        QueueBossHealingMinionWave(
            enemy,
            firstWaveNumber
        );

        enemy.bossHealingWavesSpawned =
            firstWaveNumber;
    }
    else if (
        enemy.bossHealingWavesSpawned <
        bossHealingMinionWaveCount
        )
    {
        const int currentWaveNumber =
            enemy.bossHealingWavesSpawned;

        const bool currentWaveDefeated =
            !HasLivingBossHealingMinions(
                enemy,
                currentWaveNumber
            );

        if (currentWaveDefeated)
        {
            const int nextWaveNumber =
                currentWaveNumber +
                1;

            QueueBossHealingMinionWave(
                enemy,
                nextWaveNumber
            );

            enemy.bossHealingWavesSpawned =
                nextWaveNumber;
        }
    }

    const bool healingTimeCompleted =
        enemy.bossHealingTimer >=
        safeDuration;

    const bool allWavesSpawned =
        enemy.bossHealingWavesSpawned >=
        bossHealingMinionWaveCount;

    bool finalWaveDefeated =
        false;

    if (allWavesSpawned)
    {
        finalWaveDefeated =
            !HasLivingBossHealingMinions(
                enemy,
                enemy.bossHealingWavesSpawned
            );
    }

    // The Boss remains in its healing loop until both:
    //
    // 1. The intended healing duration has completed.
    // 2. All three minion waves have been defeated.
    if (
        !healingTimeCompleted ||
        !allWavesSpawned ||
        !finalWaveDefeated
        )
    {
        return;
    }

    // Ensure the complete 20% healing amount is applied.
    const int remainingHealing =
        enemy.bossHealingTargetAmount -
        enemy.bossHealingAmountApplied;

    if (remainingHealing > 0)
    {
        enemy.hp =
            std::min(
                enemy.maxHp,
                enemy.hp +
                remainingHealing
            );

        enemy.bossHealingAmountApplied =
            enemy.bossHealingTargetAmount;
    }

    // Use double the ordinary special stun duration.
    float normalStunDuration =
        GetBossSpecialStunDuration(
            enemy
        );

    // The final health tier normally cannot be staggered,
    // so use the original high-tier stun as the fallback.
    if (normalStunDuration <= 0.0f)
    {
        normalStunDuration =
            4.50f;
    }

    const float healingEndStunDuration =
        std::max(
            bossHealingMinimumEndStunDuration,
            normalStunDuration *
            bossHealingEndStunMultiplier
        );

    // Cooldown continues counting during the stun, so add
    // the stun time to preserve the intended post-stun delay.
    enemy.bossHealingCooldownTimer =
        bossHealingCooldown +
        healingEndStunDuration;

    StartBossStunned(
        enemy,
        healingEndStunDuration,
        true
    );
}

bool Game::HasLivingBossHealingMinions(
    const Enemy& boss,
    int waveNumber
) const
{
    if (
        boss.id <= 0 ||
        waveNumber <= 0
        )
    {
        return false;
    }

    // Check enemies already added to the game.
    for (
        const Enemy& enemy :
        enemies
        )
    {
        if (
            !enemy.active
            )
        {
            continue;
        }

        if (
            enemy.bossHealingSummonerId ==
            boss.id &&
            enemy.bossHealingSummonWave ==
            waveNumber
            )
        {
            return true;
        }
    }

    // Also check queued enemies that have not yet been
    // inserted into the enemies vector.
    for (
        const PendingEnemySpawn& pending :
        pendingEnemySpawns
        )
    {
        if (
            pending.bossHealingSummonerId ==
            boss.id &&
            pending.bossHealingSummonWave ==
            waveNumber
            )
        {
            return true;
        }
    }

    return false;
}

void Game::QueueBossHealingMinionWave(
    const Enemy& boss,
    int waveNumber
)
{
    if (
        boss.type !=
        EnemyType::Boss ||
        !boss.active ||
        waveNumber <= 0
        )
    {
        return;
    }

    const int minionCount =
        std::max(
            1,
            bossHealingMinionsPerWave
        );

    const float randomRotation =
        static_cast<float>(
            GetRandomValue(
                0,
                359
            )
            ) *
        DEG2RAD;

    const int bossElevation =
        GetTerrainElevationAtWorld(
            boss.pos
        );

    for (
        int minionIndex = 0;
        minionIndex < minionCount;
        ++minionIndex
        )
    {
        bool foundPosition =
            false;

        Vector2 spawnPosition =
            boss.pos;

        for (
            int attempt = 0;
            attempt < 12;
            ++attempt
            )
        {
            const float baseAngle =
                randomRotation +
                (
                    static_cast<float>(
                        minionIndex
                        ) /
                    static_cast<float>(
                        minionCount
                        )
                    ) *
                PI *
                2.0f;

            const float angle =
                baseAngle +
                static_cast<float>(
                    attempt
                    ) *
                0.31f;

            const float radiusVariation =
                static_cast<float>(
                    (
                        attempt %
                        3
                        ) -
                    1
                    ) *
                24.0f;

            const float spawnRadius =
                bossHealingMinionSpawnRadius +
                radiusVariation;

            const Vector2 candidate{
                boss.pos.x +
                    cosf(angle) *
                    spawnRadius,

                boss.pos.y +
                    sinf(angle) *
                    spawnRadius
            };

            if (
                GetTerrainElevationAtWorld(
                    candidate
                ) !=
                bossElevation
                )
            {
                continue;
            }

            if (
                !CanEnemyStandAt(
                    candidate,
                    candidate,
                    20.0f
                )
                )
            {
                continue;
            }

            spawnPosition =
                candidate;

            foundPosition =
                true;

            break;
        }

        if (!foundPosition)
        {
            continue;
        }

        const int typeRoll =
            GetRandomValue(
                0,
                99
            );

        EnemyType minionType =
            EnemyType::Grunt;

        if (typeRoll < 45)
        {
            minionType =
                EnemyType::Grunt;
        }
        else if (typeRoll < 70)
        {
            minionType =
                EnemyType::Runner;
        }
        else if (typeRoll < 90)
        {
            minionType =
                EnemyType::Shooter;
        }
        else
        {
            minionType =
                EnemyType::Tank;
        }

        PendingEnemySpawn pendingSpawn;

        pendingSpawn.type =
            minionType;

        pendingSpawn.chamberId =
            boss.chamberId;

        pendingSpawn.position =
            spawnPosition;

        pendingSpawn.bossHealingSummonerId =
            boss.id;

        pendingSpawn.bossHealingSummonWave =
            waveNumber;

        pendingEnemySpawns.push_back(
            pendingSpawn
        );
    }
}

void Game::ProcessPendingEnemySpawns()
{
    if (pendingEnemySpawns.empty())
    {
        return;
    }

    std::vector<PendingEnemySpawn>
        spawnsToProcess;

    spawnsToProcess.swap(
        pendingEnemySpawns
    );

    for (
        const PendingEnemySpawn& request :
        spawnsToProcess
        )
    {
        const std::size_t previousCount =
            enemies.size();

        SpawnEnemyInChamber(
            request.type,
            request.chamberId
        );

        if (
            enemies.size() <=
            previousCount
            )
        {
            continue;
        }

        Enemy& spawnedEnemy =
            enemies.back();

        spawnedEnemy.bossHealingSummonerId =
            request.bossHealingSummonerId;

        spawnedEnemy.bossHealingSummonWave =
            request.bossHealingSummonWave;

        spawnedEnemy.pos =
            request.position;

        spawnedEnemy.previousAnimationPosition =
            request.position;

        spawnedEnemy.animationPositionInitialized =
            true;

        spawnedEnemy.path.clear();
        spawnedEnemy.pathIndex = 0;
        spawnedEnemy.pathRefreshTimer = 0.0f;

        spawnedEnemy.spawnState =
            EnemySpawnState::GroundEffect;

        spawnedEnemy.spawnStateTimer =
            0.0f;

        spawnedEnemy.animationState =
            EnemyAnimationState::Idle;
    }
}

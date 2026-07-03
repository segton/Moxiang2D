#pragma once

#include "raylib.h"

#include <string>
#include <vector>

enum class GameState
{
    Playing,
    ChoosingUpgrade,
    GameOver
};

enum class EnemyType
{
    Grunt,
    Runner,
    Tank,
    Shooter,
    Boss
};

enum class ProjectileOwner
{
    Player,
    Enemy
};

enum class SkillType
{
    Fireball,
    Lightning,
    IceField,
    SpinningBlade
};

enum class VfxType
{
    HitSpark,
    Explosion,
    FloatingDamage,
    LightningLine,
    SkillCircle,
    DeathBurst
};

enum class TileType
{
    Grass = 0,
    Dirt,
    Stone,
    Water
};

enum class EditorTool
{
    PaintTile = 0,
    PlaceObstacle,
    EraseObstacle,
    MoveObstacle
};

enum class CollisionShape
{
    Box = 0,
    Circle
};

struct TileBrush
{
    std::string imagePath;
    Texture2D texture{};
    bool hasTexture = false;
    bool autoFitToTile = true;
    Color fallbackColor = WHITE;
};

struct Obstacle
{
    Vector2 position{};
    Vector2 size{ 64.0f, 64.0f };

    int type = 0; // 0 = tree, 1 = rock

    std::string imagePath;
    Texture2D texture{};
    bool hasTexture = false;

    bool collisionEnabled = true;
    bool colliderAuto = true;

    CollisionShape collisionShape = CollisionShape::Box;
    Vector2 colliderSize{ 64.0f, 64.0f };
    float colliderRadius = 32.0f;
};

struct Player
{
    Vector2 pos{ 360.0f, 760.0f };
    Vector2 moveTarget{ 360.0f, 760.0f };

    bool hasMoveTarget = false;

    float radius = 28.0f;
    float speed = 420.0f;

    int hp = 20;
    int maxHp = 20;

    float attackRange = 420.0f;
    float attackTimer = 0.0f;
    float attackInterval = 0.45f;

    int attackDamage = 1;
    int projectileCount = 1;
};

struct Enemy
{
    EnemyType type = EnemyType::Grunt;

    Vector2 pos{};
    Vector2 velocity{};

    float radius = 24.0f;
    float speed = 110.0f;

    int hp = 3;
    int maxHp = 3;
    int contactDamage = 1;

    float attackTimer = 0.0f;
    float attackInterval = 1.0f;

    bool active = false;
};

struct Projectile
{
    ProjectileOwner owner = ProjectileOwner::Player;

    Vector2 pos{};
    Vector2 velocity{};

    float radius = 7.0f;
    int damage = 1;

    float life = 2.0f;
    bool active = false;
};

struct SkillSlot
{
    SkillType type = SkillType::Fireball;

    float cooldown = 3.0f;
    float cooldownRemaining = 0.0f;

    Rectangle buttonRect{};
};

struct VfxParticle
{
    VfxType type = VfxType::HitSpark;

    Vector2 pos{};
    Vector2 velocity{};

    float radius = 4.0f;
    float life = 0.0f;
    float maxLife = 1.0f;

    int value = 0;          // For damage numbers
    Vector2 endPos{};       // For lightning lines

    Color color = WHITE;
    bool active = false;
};

struct WaveManager
{
    int wave = 1;

    int enemiesToSpawn = 10;
    int enemiesSpawned = 0;

    float spawnTimer = 0.0f;
    float spawnInterval = 0.8f;

    bool waveActive = true;
};

struct NPC
{
    Vector2 position{};
    float radius = 32.0f;
    std::string name;
    std::string dialogue;
};

class Game
{
public:
    void Init();
    void Update(float dt);
    void Draw();
    void DrawEditorUi();

private:
    static constexpr int MapWidth = 40;
    static constexpr int MapHeight = 30;
    static constexpr float TileSize = 64.0f;

    static constexpr const char* DefaultLevelPath = "levels/level01.mox";

private:
    void InitTileBrushes();
    void LoadDefaultLevel();

    bool SaveLevel(const char* path) const;
    bool LoadLevel(const char* path);

    bool LoadTileBrushTexture(int tileIndex, const std::string& path);
    bool LoadCurrentObstacleTexture(const std::string& path);
    bool LoadObstacleTexture(Obstacle& obstacle, const std::string& path);

    void UpdateInput(float dt);
    void UpdatePlayer(float dt);
    void UpdateCamera(float dt);

    void UpdateEditorCameraControls();
    void HandleDroppedFiles();

    void DrawGround();
    void DrawObstacles();
    void DrawNpcs();
    void DrawPlayer();
    void DrawDialogue();
    void DrawUi();
    void DrawEditorWorldOverlay();

    void HandleEditorWorldInput(Vector2 screenPosition, bool pressed, bool down, bool released);

    int GetClickedNpc(Vector2 worldPosition) const;
    int GetObstacleAt(Vector2 worldPosition) const;

    void StartDialogue(int npcIndex);

    int CellIndex(int x, int y) const;
    bool IsCellInside(int x, int y) const;
    bool WorldToCell(Vector2 worldPosition, int& cellX, int& cellY) const;
    Vector2 CellToWorld(int cellX, int cellY) const;

    bool IsTileWalkable(int tileType) const;
    bool IsCellBlocked(int cellX, int cellY) const;

    bool FindNearestWalkableCell(Vector2 worldPosition, int& outX, int& outY) const;
    bool FindPath(Vector2 startWorld, Vector2 targetWorld, std::vector<Vector2>& outPath) const;

    void SetMoveDestination(Vector2 worldTarget);
    void SetMoveDestinationNearNpc(int npcIndex);

    Rectangle GetObstacleVisualRect(const Obstacle& obstacle) const;
    Rectangle GetObstacleCollisionRect(const Obstacle& obstacle) const;
    void UpdateObstacleAutoCollider(Obstacle& obstacle) const;


    void InitCombat();

    void UpdateCombat(float dt);
    void UpdateWave(float dt);
    void UpdateEnemies(float dt);
    void UpdateAutoAttack(float dt);
    void UpdateProjectiles(float dt);
    void UpdateSkills(float dt);
    void UpdateVfx(float dt);

    void SpawnEnemy(EnemyType type);
    Vector2 GetRandomSpawnPosition() const;

    Enemy* FindNearestEnemy(Vector2 fromPos, float range);
    void ShootProjectile(Vector2 startPos, Vector2 targetPos, int damage, float speed);

    void CheckProjectileEnemyCollisions();
    void CleanupCombatObjects();

    void StartWave(int waveNumber);

    void UpdateSkillButtonRects();
    bool TryActivateSkillAtScreen(Vector2 screenPos);
    void ActivateSkill(SkillType type);
    void ActivateFireball();
    void ActivateLightning();
    void ActivateIceField();

    void SpawnHitSpark(Vector2 pos);
    void SpawnDamageNumber(Vector2 pos, int value);
    void SpawnDeathBurst(Vector2 pos);
    void SpawnLightningLine(Vector2 start, Vector2 end);

    void DrawCombat();
    void DrawEnemies();
    void DrawProjectiles();
    void DrawVfx();
    void DrawSkillUi();
    void DrawCombatHud();

    bool TryChooseUpgradeAtScreen(Vector2 screenPos);
    void ApplyUpgradeChoice(int choiceIndex);
    void DrawUpgradeChoices();

private:
    std::vector<int> tiles;
    std::vector<TileBrush> tileBrushes;

    Vector2 playerPosition{ 0.0f, 0.0f };

    float playerRadius = 20.0f;
    float playerSpeed = 230.0f;

    Camera2D camera{};

    std::vector<Vector2> currentPath;
    int pathIndex = 0;
    bool hasPath = false;

    std::vector<Obstacle> obstacles;
    std::vector<NPC> npcs;

    int pendingNpc = -1;
    int activeDialogueNpc = -1;

    float interactDistance = 95.0f;
    float dialogueCooldown = 0.0f;

    bool wasTouching = false;

    bool buildMode = false;
    bool showGrid = true;

    int editorTool = static_cast<int>(EditorTool::PaintTile);

    int selectedTile = 0;

    int selectedObstacleType = 0;
    bool obstacleSnapToGrid = true;
    bool obstacleAutoFitToGrid = true;

    Vector2 newObstacleSize{ 64.0f, 64.0f };
    bool newObstacleCollision = true;
    bool newObstacleColliderAuto = true;
    int newObstacleCollisionShape = static_cast<int>(CollisionShape::Box);
    Vector2 newObstacleColliderSize{ 64.0f, 64.0f };
    float newObstacleColliderRadius = 32.0f;

    std::string currentObstacleImagePath;
    Texture2D currentObstacleTexture{};
    bool currentObstacleHasTexture = false;

    char tileImagePathInput[512]{};
    char obstacleImagePathInput[512]{};

    int selectedObstacleIndex = -1;
    bool draggingObstacle = false;
    Vector2 dragOffset{};
    bool tileImageDropHovered = false;
    bool obstacleImageDropHovered = false;

    Player player;

    std::vector<Enemy> enemies;
    std::vector<Projectile> projectiles;
    std::vector<VfxParticle> vfxParticles;

    SkillSlot skills[3];

    WaveManager wave;

    GameState gameState = GameState::Playing;
};
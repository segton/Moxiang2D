#pragma once

#include "raylib.h"

#include <string>
#include <vector>
#include <deque>


enum class PlayerDirection
{
    Down = 0,
    DownRight = 1,
    Right = 2,
    UpRight = 3,
    Up = 4,
    UpLeft = 5,
    Left = 6,
    DownLeft = 7
};

enum class PlayerAnimationState
{
    Idle,
    Walking,
    Attacking
};

enum class GameState
{
    Playing,
    ChoosingUpgrade,
    GameOver
};

enum class VfxType
{
    HitSpark,
    Explosion,
    FloatingDamage,
    LightningLine,
    SkillCircle,
    DeathBurst,
    SlashLine
};

enum class TerrainCliffFace
{
    North = 0,
    East,
    South,
    West
};

enum class TerrainWallFace
{
    None = -1,

    North = 0,
    East,
    South,
    West
};

struct Light2D
{
    Vector2 position{
        0.0f,
        0.0f
    };

    float radius = 240.0f;
    float intensity = 1.0f;

    Color color{
        255,
        190,
        100,
        255
    };

    bool enabled = true;
    bool followsPlayer = false;

    // Visual offset in view/screen space after WorldToView().
    Vector2 viewOffset{
        0.0f,
        0.0f
    };

    // Optional scaling if we want elliptical lights later.
    Vector2 scale{
        1.0f,
        1.0f
    };

    float rotationDegrees = 0.0f;
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

struct HuashanGroundMark
{
    Vector2 position{
        0.0f,
        0.0f
    };

    float life = 10.0f;
    float maxLife = 10.0f;
};

struct DashAfterimage
{
    Vector2 worldPosition{};

    PlayerDirection direction =
        PlayerDirection::Down;

    int frame = 0;

    // False = player walking sheet.
    // True  = Boss walking sheet.
    bool useBossSprite = false;

    float life = 0.0f;
    float maxLife = 0.28f;
};

struct OrbitalBlade
{
    float angle = 0.0f;
    float orbitRadius = 110.0f;
    float angularSpeed = 6.5f;

    float bladeRadius = 42.0f;

    int damage = 16;

    float life = 5.0f;
    float hitTimer = 0.0f;
    float hitInterval = 0.6f;

    bool active = false;
};

enum class SkillType
{
    SpinningBlade,   // Real skill: Wind Blades
    Huashan,         // Real skill: 怒劈华山
    Dongfeng,        // Real skill: 东风浩荡
    Fireball,
    Lightning,
    IceField,

};

struct UpgradeChoice
{
    SkillType skillType = SkillType::SpinningBlade;

    std::string title;
    std::string subtitle;
    std::string description;

    bool unlocksSkill = true;
    int targetLevel = 1;

    Rectangle cardRect{};
};

enum class EnemyType
{
    Grunt,
    Runner,
    Tank,
    Shooter,
    Boss
};

enum class EnemyAnimationState
{
    Idle = 0,
    Walking,
    Attacking
};

enum class EnemySpawnState
{
    GroundEffect = 0,

    // Enemy rises from underground.
    // Spawn circle and enemy tint remain fully visible.
    Emerging,

    // Enemy is fully above ground and active.
    // Spawn circle and enemy tint now fade away.
    FadeOut,

    Ready
};

enum class BossActionState
{
    None = 0,

    SlamWindup,
    Slam,

    LaserWindup,
    LaserActive,

    BombardmentRoar,

    HealingWindup,
    HealingActive,

    ChargeWindup,
    ChargeActive,

    Stunned
};

enum class BossDashPurpose
{
    None = 0,

    // 75% and 25% escape dash with radial bullets.
    PhaseEscape,

    // Forward dash toward player, followed by slam.
    ChaseSlam,

    // Retreat dash, followed by bombardment roar.
    BombardmentEscape
};

enum class ProjectileOwner
{
    Player,
    Enemy
};





enum class TileType
{
    Grass = 0,
    Dirt,
    Stone,
    Water
};

enum class RampDirection
{
    None = 0,

    // Grid directions.
    North, // y - 1
    East,  // x + 1
    South, // y + 1
    West   // x - 1
};

enum class EditorTool
{
    PaintTile = 0,
    RaiseTerrain,
    LowerTerrain,
    FlattenTerrain,

    PaintWall,
    ClearWall,

    PaintChamber,
    EraseMapCell,

    PlaceRamp,
    RemoveRamp,

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
    // Used only by ordinary standalone tile textures.
    std::string imagePath;

    Texture2D texture{};
    bool hasTexture = false;
    bool autoFitToTile = true;

    // Atlas configuration.
    bool usesAtlas = false;

    // Pixel rectangle inside terrainTileSheet.
    Rectangle source{
        0.0f,
        0.0f,
        1.0f,
        1.0f
    };

    // Controls pathfinding and movement.
    bool walkable = true;

    Color fallbackColor = WHITE;
};

struct TerrainCell
{
    // False means this cell is outside the playable map shape.
    // Void cells are neither rendered nor walkable.
    bool enabled = true;

    // Exact chamber membership. -1 means no chamber.
    int chamberId = 0;

    // -1 means inherit the floor tile of this cell.
    int northWallTile = -1;
    int eastWallTile = -1;
    int southWallTile = -1;
    int westWallTile = -1;

    // Walkable terrain height.
    int elevation = 0;

    // A ramp is stored on the lower cell and points
    // toward one adjacent cell at elevation + 1.
    RampDirection rampDirection =
        RampDirection::None;
};

// The legacy renderer is preserved as a fallback while the new renderer
// uses a real depth buffer for terrain, sprites, ramps and obstacles.
enum class WorldRendererMode
{
    Legacy2D = 0,
    Hybrid3D
};

// Default billboards face the camera on both axes so sprite proportions
// remain unchanged. UprightWorld can be used for special objects that
// should rotate only around the vertical world axis.
enum class HybridBillboardOrientation
{
    FaceCamera = 0,
    UprightWorld
};

struct HybridTerrainBatch
{
    Mesh mesh{};
    Model model{};

    // Terrain is split by chamber and tile texture so hidden
    // chambers do not submit any terrain draw calls.
    int chamberId = -1;
    int tileIndex = -1;

    bool ready = false;
};

// --------------------------------------------------
// Dungeon chamber foundation
//
// Step 1 only tracks which chamber contains the player.
// Rendering, encounter waves and gates will use this data later.
// --------------------------------------------------
struct DungeonChamber
{
    int id = -1;
    std::string name;

    // Inclusive terrain-cell bounds.
    int minCellX = 0;
    int minCellY = 0;
    int maxCellX = 0;
    int maxCellY = 0;

    // Cached world-space rectangle for rendering/culling later.
    Rectangle worldBounds{};

    // True when at least one enabled map cell belongs to this chamber.
    bool hasCells = false;

    bool discovered = false;
    bool active = false;
    bool cleared = false;

    // Every chamber contains two encounter waves. The exit remains
    // locked until both waves have spawned and all owned enemies die.
    bool encounterStarted = false;
    int wavesRequired = 2;
    int wavesCompleted = 0;
    int currentWave = 0;

    // Hybrid terrain fades between chambers. Legacy rendering swaps
    // chambers immediately because its ground is stored in one cache.
    float visibility = 0.0f;
    float targetVisibility = 0.0f;
};

struct Obstacle
{
    // Ground footprint position.
    Vector2 position{};

    // Visual sprite size.
    Vector2 size{
        64.0f,
        64.0f
    };

    int type = 0;

    // Local height offset above the terrain beneath this object.
    // Ordinary trees and props should normally remain at 0.
    int heightLevel = 0;

    std::string imagePath;
    Texture2D texture{};
    bool hasTexture = false;

    bool collisionEnabled = true;
    bool colliderAuto = true;

    CollisionShape collisionShape =
        CollisionShape::Box;

    Vector2 colliderSize{
        64.0f,
        64.0f
    };

    float colliderRadius =
        32.0f;

    // Rendering and future lighting controls.
    bool castsShadow = true;

    // Used later by walls and buildings
    // to block environmental lights.
    bool blocksLight = false;

    // Draw after the normal world pass.
    bool foreground = false;
};


struct Player
{
    Vector2 pos{ 360.0f, 760.0f };
    Vector2 moveTarget{ 360.0f, 760.0f };

    bool hasMoveTarget = false;

    float radius = 28.0f;
    float speed = 420.0f;

    int hp = 120;
    int maxHp = 120;

    // Melee combat
    float meleeRange = 78.0f;
    float attackAssistRange = 360.0f;

    float attackTimer = 0.0f;
    float attackInterval = 0.42f;

    int attackDamage = 12;

    int projectileCount = 1; // keep for skills later if needed
};

struct PerfSpikeRecord
{
    int index = 0;

    float frameMs = 0.0f;
    float updateMs = 0.0f;
    float drawMs = 0.0f;
    float combatMs = 0.0f;

    std::string reason;
    std::string drawSection;
    std::string combatDrawSection;

    float drawGroundMs = 0.0f;
    float drawCombatMs = 0.0f;
    float drawPlayerMs = 0.0f;
    float drawUiMs = 0.0f;

    float cdProjectilesMs = 0.0f;
    float cdEnemiesMs = 0.0f;
    float cdBladesMs = 0.0f;
    float cdDongfengTeleMs = 0.0f;
    float cdDongfengWaveMs = 0.0f;
    float cdVfxMs = 0.0f;

    int enemies = 0;
    int projectiles = 0;
    int vfx = 0;
};

struct Enemy
{
    int id = 0;

    // Chamber encounter that owns this enemy. Debug and boss-healing
    // spawns inherit the currently active/owning chamber.
    int chamberId = -1;

    EnemyType type = EnemyType::Grunt;

    Vector2 pos{};
    Vector2 velocity{};

    float radius = 24.0f;
    float speed = 110.0f;

    // Small-enemy animation.
    int spriteFrame = 0;
    float spriteAnimTimer = 0.0f;

    // Brief white flash after receiving damage.
    float damageFlashTimer = 0.0f;
    // Number of arrows fired by one complete attack.
    int shooterShotsPerAttack = 1;

    // Runtime state for the current attack.
    int shooterShotsFiredThisAttack = 0;

    // The animation may return from the final frame to frame 0
    // only one time during the complete attack.
    bool shooterAttackLoopedOnce = false;

    // Used for the third and later arrows while frame 0 is held.
    float shooterExtraShotTimer = 0.0f;

    // Direction currently displayed by the sprite.
    PlayerDirection spriteDirection =
        PlayerDirection::Down;

    // Used to determine which direction the enemy moved.
    Vector2 previousAnimationPosition{
        0.0f,
        0.0f
    };

    bool animationPositionInitialized = false;

    int hp = 3;
    int maxHp = 3;
    int contactDamage = 1;

    float attackTimer = 0.0f;
    float attackInterval = 1.0f;

    std::vector<Vector2> path;
    int pathIndex = 0;
    float pathRefreshTimer = 0.0f;
    float pathRefreshInterval = 0.35f;

    float frozenTimer = 0.0f;
    float slowTimer = 0.0f;
    float slowMultiplier = 1.0f;
    float stunTimer = 0.0f;

    // Physical reaction system for heavy skills such as Huashan.
    float weight = 1.0f;
    float knockbackResistance = 1.0f;
    Vector2 knockbackVelocity{ 0.0f, 0.0f };
    float airborneTimer = 0.0f;
    float airborneMaxTimer = 0.0f;
    float landingStunTimer = 0.0f;
    float landingStunOnLand = 0.0f;
    float visualHeight = 0.0f;

    int dongfengHitId = 0;

    float shootTimer = 0.0f;
    float shootInterval = 1.8f;
    float shootRange = 560.0f;
    int bulletDamage = 6;
    float bulletSpeed = 360.0f;

    int bossHitCounter = 0;
    int bossDashHitThreshold = 6;

    bool bossDashCharging = false;
    float bossDashChargeTimer = 0.0f;
    float bossDashChargeDuration = 0.90f;

    float bossDashTimer = 0.0f;
    Vector2 bossDashVelocity{};

    bool active = false;

    EnemyAnimationState animationState =
        EnemyAnimationState::Idle;

    // --------------------------------------------------
// Boss healing-wave ownership
//
// Zero means this is an ordinary enemy.
// --------------------------------------------------

    int bossHealingSummonerId =
        0;

    int bossHealingSummonWave =
        0;

    // --------------------------------------------------
// Enemy spawning sequence
// --------------------------------------------------

    EnemySpawnState spawnState =
        EnemySpawnState::Ready;

    float spawnStateTimer =
        0.0f;

    int shooterAnimationFrame = 0;
    float shooterAnimationTimer = 0.0f;

    // Prevents the attack impact from triggering more than once.
    bool shooterAttackImpactProcessed = false;

    // Used to decide whether retreat should begin.
    bool shooterProjectileFired = false;

    // Shooter runs away while this is greater than zero.
    float shooterRetreatTimer = 0.0f;


    // Boss idle and walking animation.
    int bossAnimationFrame = 0;
    float bossAnimationTimer = 0.0f;

    // Boss uses a separate previous-position value.
    // Do not share this with the blob placeholder animation.
    Vector2 bossPreviousAnimationPosition{
        0.0f,
        0.0f
    };

    bool bossAnimationPositionInitialized = false;


    BossActionState bossActionState =
        BossActionState::None;

    // One-time health phases.
    bool bossPhase75Triggered = false;
    bool bossPhase25Triggered = false;

    // Slam attack.
    bool bossSlamImpactProcessed = false;

    // Laser attack.
    float bossLaserWindupTimer = 0.0f;
    float bossLaserActiveTimer = 0.0f;
    float bossLaserCooldownTimer = 0.0f;
    float bossLaserDamageTimer = 0.0f;

    Vector2 bossLaserAimPosition{
        0.0f,
        0.0f
    };

    Vector2 bossLaserEndPosition{
        0.0f,
        0.0f
    };

    // True only during the normal forward chase dash.
// The 75% and 25% escape jump keeps this false.
    bool bossDashIsForward = false;

    float bossForwardDashCooldownTimer = 0.0f;
    float bossDashAfterimageTimer = 0.0f;

    // Delay before the slam animation starts.
    float bossSlamWindupTimer = 0.0f;

    // White, unlit ring shown briefly after impact.
    float bossSlamImpactVisualTimer = 0.0f;

    // Actual world-space start of the laser.
    Vector2 bossLaserStartPosition{
        0.0f,
        0.0f
    };


    BossDashPurpose bossDashPurpose =
        BossDashPurpose::None;

    // --------------------------------------------------
    // Bombardment
    // --------------------------------------------------

    float bossBombardmentTimer = 0.0f;
    float bossBombardmentSpawnTimer = 0.0f;
    float bossBombardmentCooldownTimer = 0.0f;


    // --------------------------------------------------
// Boss healing phase
// --------------------------------------------------

    int bossHealingUseCount = 0;

    float bossHealingTimer = 0.0f;
    float bossHealingWaveTimer = 0.0f;
    float bossHealingCooldownTimer = 0.0f;

    int bossHealingWavesSpawned = 0;

    int bossHealingAmountApplied = 0;
    int bossHealingTargetAmount = 0;

    // --------------------------------------------------
    // Break / special stun
    // --------------------------------------------------

    // Damage accumulated while the Boss is using
    // a breakable channelled attack.
    float bossInterruptDamage = 0.0f;

    float bossStunnedTimer = 0.0f;

    // --------------------------------------------------
    // Straight-line charge
    // --------------------------------------------------

    float bossChargeWindupTimer = 0.0f;
    float bossChargeRemainingTimer = 0.0f;
    float bossChargeCooldownTimer = 0.0f;

    // Number of completed pre-charge animation loops.
    int bossPreChargeLoopsCompleted = 0;

    Vector2 bossChargeDirection{
        1.0f,
        0.0f
    };

    bool bossChargeHitPlayer = false;

    // Prevents random attack selection every frame.
    float bossDecisionTimer = 0.0f;


};

struct BossFallingRock
{
    int ownerBossId = 0;

    Vector2 position{
        0.0f,
        0.0f
    };

    int terrainElevation = 0;

    float timer = 1.0f;
    float fallDuration = 1.0f;

    // Height at the beginning of the fall.
    float startHeight = 700.0f;

    // Visual rock size.
    float visualRadius = 38.0f;

    // Actual ground damage radius.
    float damageRadius = 145.0f;

    int damage = 12;

    bool active = true;
};

struct Projectile
{
    ProjectileOwner owner =
        ProjectileOwner::Player;

    Vector2 pos{};
    Vector2 velocity{};

    int terrainElevation = 0;

    // Visual height above the projectile's terrain plane.
    // This does not affect its 2D gameplay collision.
    float visualHeight = 18.0f;

    float radius = 7.0f;
    int damage = 1;

    float life = 2.0f;
    bool active = false;
};

struct SkillSlot
{
    SkillType type = SkillType::SpinningBlade;

    float cooldown = 3.0f;
    float cooldownRemaining = 0.0f;

    Rectangle buttonRect{};

    bool unlocked = false;
    int level = 1;
};


struct PendingEnemySpawn
{
    EnemyType type =
        EnemyType::Grunt;

    int chamberId = -1;

    Vector2 position{
        0.0f,
        0.0f
    };

    // Boss-healing summon information.
    // Both remain zero for ordinary queued spawns.
    int bossHealingSummonerId =
        0;

    int bossHealingSummonWave =
        0;
};

struct WaveManager
{
    // Combat scaling index retained for the existing enemy stats.
    int wave = 1;

    int chamberId = -1;
    int chamberWave = 0;
    int chamberWavesRequired = 2;

    int enemiesToSpawn = 0;
    int enemiesSpawned = 0;

    float spawnTimer = 0.0f;
    float spawnInterval = 0.8f;

    bool waveActive = false;
    bool waitingForNextWave = false;
    float nextWaveTimer = 0.0f;
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
    void Shutdown();
    void Update(float dt);
    void Draw();
    void DrawEditorUi();
    void SubmitFrameTiming(
        float fullFrameMs,
        float gameUpdateMs,
        float gameDrawMs,
        float endDrawingMs
    );
private:
    // Runtime dimensions: every saved level may use a different canvas size.
    // Irregular level shapes are represented by disabled/void TerrainCells.
    int MapWidth = 40;
    int MapHeight = 30;

    static constexpr int MaximumMapDimension = 512;
    static constexpr float TileSize = 64.0f;

    static constexpr const char* DefaultLevelPath = "levels/level01.mox";

private:
    void InitTileBrushes();
    void LoadDefaultLevel();

    // Dungeon chamber foundation.
    void InitializeTestChambers();
    void UpdateActiveChamber(bool forceUpdate = false);
    void UpdateChamberVisibility(float dt);
    void ResetChamberEncounterProgress();
    void StartChamberEncounter(int chamberId);
    void StartChamberWave(int chamberId, int chamberWave);

    int FindChamberAtWorld(Vector2 worldPosition) const;
    float GetChamberVisibility(int chamberId) const;
    bool IsChamberVisible(int chamberId) const;
    bool IsWorldPositionInVisibleChamber(Vector2 worldPosition) const;

    bool HasLivingEnemiesInChamber(int chamberId) const;
    bool HasPendingEnemiesInChamber(int chamberId) const;

    void CreateNewMap(
        int width,
        int height,
        bool startEmpty
    );

    bool IsCellEnabled(
        int cellX,
        int cellY
    ) const;

    void EnsureChamberExists(
        int chamberId
    );

    void RebuildChamberBounds();

    DungeonChamber* FindChamberById(int chamberId);
    const DungeonChamber* FindChamberById(int chamberId) const;

    Rectangle MakeChamberWorldBounds(
        int minCellX,
        int minCellY,
        int maxCellX,
        int maxCellY
    ) const;

    bool SaveLevel(const char* path) const;
    bool LoadLevel(const char* path);

    bool LoadTileBrushTexture(int tileIndex, const std::string& path);

    bool LoadTerrainTileSheet(
        const std::string& path,
        int columns,
        int rows
    );

    void ConfigureTileBrushesFromSheet();

    Texture2D GetTileBrushTexture(
        int tileIndex
    ) const;

    Rectangle GetTileBrushSourceRect(
        int tileIndex
    ) const;

    bool LoadCurrentObstacleTexture(const std::string& path);
    bool LoadObstacleTexture(Obstacle& obstacle, const std::string& path);

    std::string ImportDroppedAssetToProject(
        const std::string& sourcePath,
        const std::string& assetCategory
    );


    void UpdateInput(float dt);
    void UpdatePlayer(float dt);
    void UpdateCamera(float dt);

    void UpdateEditorCameraControls();
    void HandleDroppedFiles();

    void DrawGround();
    void BuildGroundCache();
    void InvalidateGroundCache();

    void DrawObstacles();
    void DrawNpcs();
    void DrawPlayer();
    void DrawDialogue();
    void DrawUi();
    void DrawEditorWorldOverlay();

    bool IsWorldCircleVisible(Vector2 worldPos, float radius) const;

    void LoadPlayerSpriteSheet();

    void StartPlayerAttackAnimation(
        const Enemy* target
    );

    void CancelPlayerAttackAnimation();

    float GetCurrentPlayerMoveSpeed() const;

    void UpdatePlayerAnimation(float dt);
    void DrawPlayerSprite(Vector2 drawPosition);

    Vector2 GetPlayerAnimationMoveDirection() const;

    PlayerDirection GetPlayerDirectionFromVector(
        Vector2 direction
    ) const;

    int GetPlayerDirectionRow(
        PlayerDirection direction
    ) const;

    void HandleEditorWorldInput(Vector2 screenPosition, bool pressed, bool down, bool released);

    int GetClickedNpc(Vector2 worldPosition) const;
    int GetObstacleAt(
        Vector2 worldPosition,
        int requiredHeightLevel = -1
    ) const;

    void StartDialogue(int npcIndex);

    int CellIndex(int x, int y) const;
    bool IsCellInside(int x, int y) const;
    bool WorldToCell(Vector2 worldPosition, int& cellX, int& cellY) const;
    Vector2 CellToWorld(int cellX, int cellY) const;


    bool CanTraverseTerrainEdge(
        int fromX,
        int fromY,
        int toX,
        int toY
    ) const;

    bool IsTerrainCircleBlocked(
        Vector2 fromPosition,
        Vector2 candidatePosition,
        float radius
    ) const;

    bool ViewToTerrainCell(
        Vector2 viewPosition,
        int& cellX,
        int& cellY
    ) const;

    Vector2 WorldToViewElevated(
        Vector2 worldPosition,
        float additionalHeight = 0.0f
    ) const;

    void DrawTerrainCellOutline(
        int cellX,
        int cellY,
        Color color,
        float thickness = 1.0f
    ) const;

    void GetTerrainSurfacePointsView(
        int cellX,
        int cellY,
        int elevation,
        Vector2& top,
        Vector2& right,
        Vector2& bottom,
        Vector2& left
    ) const;

    void DrawTerrainCliffFace(
        int cellX,
        int cellY,
        TerrainCliffFace face
    ) const;

    float GetTerrainCliffDepth(
        int cellX,
        int cellY,
        TerrainCliffFace face
    ) const;

    bool IsRampConnectionBetweenCells(
        int cellAX,
        int cellAY,
        int cellBX,
        int cellBY
    ) const;
    // Lightweight 2.5D projection. Gameplay remains in normal 2D world space.

    Vector2 GetObstacleViewPosition(
        const Obstacle& obstacle
    ) const;

    Vector2 WorldToView(Vector2 worldPosition) const;
    Vector2 ViewToWorld(Vector2 viewPosition) const;
    Vector2 WorldVectorToView(Vector2 worldVector) const;
    void DrawGroundCircle(Vector2 worldCenter, float worldRadius, Color color) const;
    void DrawGroundCircleLines(Vector2 worldCenter, float worldRadius, Color color) const;
    void DrawGroundCellOutline(int cellX, int cellY, Color color, float thickness = 1.0f) const;

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
    void DamagePlayer(
        int damage
    );

    void RestartGameplay();

    void UpdateCombat(float dt);
    void UpdateWave(float dt);
    void UpdateEnemies(float dt);
    void ResolveEnemySeparation();

    void UpdateAutoAttack(float dt);
    void UpdateProjectiles(float dt);
    void UpdateSkills(float dt);
    void UpdateVfx(float dt);

    void SpawnEnemy(EnemyType type);
    void SpawnEnemyInChamber(
        EnemyType type,
        int chamberId
    );

    Vector2 GetRandomSpawnPosition() const;
    Vector2 GetRandomSpawnPositionInChamber(
        int chamberId
    ) const;

    void UpdateEnemySpawnState(
        Enemy& enemy,
        float dt
    );

    bool IsEnemySpawnProtected(
        const Enemy& enemy
    ) const;

    float GetEnemySpawnDepth(
        const Enemy& enemy
    ) const;

    float GetEnemySpawnVisualOffset(
        const Enemy& enemy
    ) const;

    float GetEnemySpawnFlashAmount(
        const Enemy& enemy
    ) const;

    void LoadEnemySpawnEffectSpriteSheet();

    Rectangle GetEnemySpawnEffectSourceRect(
        int frame
    ) const;

    void DrawEnemySpawnGroundEffects2D() const;
    void DrawEnemySpawnGroundEffects3D() const;

    Enemy* FindNearestEnemy(Vector2 fromPos, float range);
    Enemy* FindEnemyById(int enemyId);

    void ShootProjectile(Vector2 startPos, Vector2 targetPos, int damage, float speed);

    void CheckProjectileEnemyCollisions();
    void CleanupCombatObjects();

    void StartWave(int waveNumber);

    void UpdateSkillButtonRects();
    bool TryActivateSkillAtScreen(Vector2 screenPos);
    void ActivateSkill(SkillType type);

    void ActivateHuashan();
    void UpdateHuashan(float dt);
    void ResolveHuashanImpact();
    float GetHuashanJumpHeight() const;
    bool IsPlayerAirborne() const;

    bool FindHuashanLandingPosition(const Enemy& target, Vector2& outLandingPosition) const;

    bool IsProjectileBlockedByWorld(
        Vector2 previousPosition,
        Vector2 nextPosition,
        int projectileElevation
    ) const;

    void ExecuteBossDash(Enemy& enemy);

    void ActivateDongfeng();
    void UpdateDongfeng(float dt);
    void LaunchDongfengWave();
    void ResolveDongfengWaveHits(Vector2 previousPos, Vector2 currentPos, float radius);
    void DestroyEnemyProjectilesInDongfengPath(Vector2 previousPos, Vector2 currentPos, float radius);
    void DrawDongfengTelegraph();
    void DrawDongfengWave();
    float GetDongfengWaveRadius() const;

    bool IsPlayerMovementLocked() const;

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
    void UpdatePerformanceStats(float dt);
    void DrawPerformanceOverlay() const;

    bool TryChooseUpgradeAtScreen(Vector2 screenPos);
    void ApplyUpgradeChoice(int choiceIndex);
    void DrawUpgradeChoices();

    void UnlockSkill(int slotIndex);
    void LevelUpSkill(int slotIndex);

    bool CanPlayerStandAt(Vector2 worldPosition) const;
    void MovePlayerWithJoystick(float dt);

    void GenerateSkillChoices();
    void UpdateUpgradeChoiceLayout();

    Rectangle GetUpgradeCardRect(int index) const;
    const char* GetSkillDisplayName(SkillType type) const;
    const char* GetSkillDescription(SkillType type) const;
    const char* GetSkillSubtitle(SkillType type) const;
    int FindSkillSlotIndex(SkillType type) const;

    void UpdateJoystick(float dt);
    void DrawJoystick();

    Vector2 GetJoystickBaseScreen() const;
    bool IsScreenPointInsideJoystick(Vector2 screenPos) const;

    void UpdateAttackButton();
    void UpdateAttackButtonRect();
    void DrawAttackButton();

    // --------------------------------------------------
// Debug/test controls
// --------------------------------------------------

    void UpdateDebugControls();

    void OpenManualUpgradeMenu();

    void UpdateDebugUpgradeButtonRect();

    void DrawDebugUpgradeButton() const;

    void UpdateDashButton();
    void UpdateDashButtonRect();
    void DrawDashButton();

    void UpdateDash(float dt);
    void UpdateDashAfterimages(float dt);
    void TryStartDash();
    void EndDash();
    void SpawnDashAfterimage();
    void DrawDashAfterimage(const DashAfterimage& afterimage);

    Vector2 GetDashDirectionWorld() const;
    Vector2 GetViewDirectionFromPlayerDirection(PlayerDirection direction) const;

    bool IsPlayerInvulnerable() const;

    bool IsScreenPointOnCombatUi(Vector2 screenPos) const;

    void UpdateMeleeAttack(float dt);
    void DealMeleeHit(Enemy& enemy);
    Vector2 GetMeleeApproachPoint(const Enemy& enemy) const;

    void SpawnSlashEffect(Vector2 start, Vector2 end);

    void RefreshEnemyPath(Enemy& enemy);
    void MoveEnemyAlongPath(Enemy& enemy, float dt);

    void SpawnWhirlwindBlades();
    void UpdateOrbitalBlades(float dt);
    void DrawOrbitalBlades();
    int GetWhirlwindBladeCount(int level) const;

    int GetModifiedDamageToEnemy(const Enemy& enemy, int baseDamage) const;
    void ApplyDamageToEnemy(Enemy& enemy, int baseDamage, Vector2 hitPos);

    bool IsBossEnemy(const Enemy& enemy) const;
    bool IsEnemyCrowdControlled(const Enemy& enemy) const;
    float GetEnemyKnockbackScale(const Enemy& enemy) const;
    float GetBossSkillDamageMultiplier(
        SkillType skillType,
        const Enemy& enemy
    ) const;

    void UpdateEnemyReactionTimers(Enemy& enemy, float dt);
    void ApplyEnemyPhysics(Enemy& enemy, float dt);

    void ApplyKnockbackToEnemy(
        Enemy& enemy,
        Vector2 origin,
        float force,
        float airborneDuration,
        float landingStunDuration
    );

    void ApplySkillDamageToEnemy(
        Enemy& enemy,
        SkillType skillType,
        int baseDamage,
        Vector2 hitPos
    );

    bool CanEnemyStandAt(
        Vector2 fromPosition,
        Vector2 worldPosition,
        float collisionRadius
    ) const;

    void SpawnEnemyProjectile(
        Vector2 startPos,
        Vector2 targetPos,
        int damage,
        float speed,
        float radius,
        float visualHeight = 18.0f
    );
    void SpawnRadialEnemyProjectiles(Vector2 center, int count, int damage, float speed, float radius);

    void UpdateEnemyShooter(Enemy& enemy, float dt, float distanceToPlayer);
    void StartBossDash(Enemy& enemy);

    int GetLightningBranchCount(int level) const;
    int GetLightningChainCount(int level) const;

    Enemy* FindNearestEnemyExcluding(
        Vector2 fromPos,
        float range,
        const std::vector<Enemy*>& excluded
    );

    bool IsScreenPointOnButtonUi(Vector2 screenPos) const;

    void RecordPerformanceSpike();
    void DumpPerformanceSpikes();

    enum class WorldDrawKind
    {
        TerrainTop,
        TerrainRamp,

        TerrainNorthCliff,
        TerrainEastCliff,
        TerrainSouthCliff,
        TerrainWestCliff,

        Obstacle,
        NPC,
        Enemy,
        DashAfterimage,
        Player,
        Projectile,
        OrbitalBlade,
        DongfengWave,
        Vfx
    };

    struct WorldDrawItem
    {
        WorldDrawKind kind =
            WorldDrawKind::Player;

        int index = -1;

        int cellX = -1;
        int cellY = -1;

        int elevationBand = 0;

        // Used by individual cliff segments.
        int segmentLevel = -1;

        float depth = 0.0f;
        float secondaryDepth = 0.0f;

        int tiePriority = 0;
        int stableOrder = 0;
    };

    float GetWorldDepth(
        Vector2 worldPosition,
        float bias = 0.0f
    ) const;

    void DrawWorldGroundEffects();

    void DrawWorldShadows() const;

    void DrawObstacleShadow(
        const Obstacle& obstacle
    ) const;

    void DrawWorldDepthSorted();
    void DrawWorldForegroundEffects();

    bool IsGroundVfx(VfxType type) const;
    bool IsForegroundVfx(VfxType type) const;

    void DrawVfxParticleVisual(
        const VfxParticle& particle
    );

    void DrawObstacleVisual(
        const Obstacle& obstacle
    );

    void DrawNpcVisual(
        const NPC& npc
    );

    void DrawEnemyVisual(
        const Enemy& enemy
    );

    void DrawPlayerVisual();

    void DrawProjectileVisual(
        const Projectile& projectile
    );

    void DrawOrbitalBladeVisual(
        const OrbitalBlade& blade
    );

    void CreateTestLights();


    void LoadSmallEnemySpriteSheet();

    bool IsSmallAnimatedEnemy(
        EnemyType type
    ) const;

    void UpdateSmallEnemyAnimation(
        Enemy& enemy,
        float dt
    );

    void DrawSmallEnemySprite(
        const Enemy& enemy,
        Vector2 drawPosition,
        Color tint
    );

    bool EnemyHasDedicatedSprite(
        EnemyType type
    ) const;

    bool ShouldUseBlobEnemySprite(
        const Enemy& enemy
    ) const;

    float GetBlobEnemyVisualSize(
        const Enemy& enemy
    ) const;

    int GetTerrainElevation(int cellX, int cellY) const;
    int GetTerrainElevationAtWorld(Vector2 worldPosition) const;
    float GetTerrainHeightAtWorld(Vector2 worldPosition) const;

    float GetTerrainSurfaceLevelAtWorld(
        Vector2 worldPosition
    ) const;

    void DrawTerrainCliffSegment(
        int cellX,
        int cellY,
        TerrainCliffFace face,
        int segmentLevel
    ) const;

    void DrawTerrainTopSurface(
        int cellX,
        int cellY
    ) const;

    void DrawTerrainRampSurface(
        int cellX,
        int cellY
    ) const;

    // --------------------------------------------------
    // Hybrid 2.5D renderer: 3D terrain + 2D sprite billboards
    // --------------------------------------------------
    void InitHybrid3D();
    void ShutdownHybrid3D();
    void LoadHybridShaders();
    void RebuildHybridTerrain();
    void MarkHybridTerrainDirty();
    void UpdateHybridCamera(float dt);
    void UpdateHybridEditorCameraControls();


    int hybridBillboardFlashLocation = -1;

    // Shared camera scale and camera-relative movement helpers.
    // camera.zoom remains the single zoom value used by both renderers.
    float GetHybridOrthoSizeFromSharedZoom() const;
    Vector2 ViewDirectionToWorldDirection(
        Vector2 viewDirection
    ) const;

    Vector3 WorldToHybrid3D(
        Vector2 worldPosition,
        float additionalHeightPixels = 0.0f
    ) const;

    float PixelsToHybridUnits(float pixels) const;
    float GetHybridTerrainHeightUnits(Vector2 worldPosition) const;

    bool ScreenToTerrainWorld3D(
        Vector2 screenPosition,
        Vector2& outWorldPosition,
        int* outCellX = nullptr,
        int* outCellY = nullptr
    ) const;

    bool ScreenToTerrainWall3D(
        Vector2 screenPosition,
        int& outCellX,
        int& outCellY,
        TerrainWallFace& outFace
    ) const;

    int GetTerrainWallTileIndex(
        int cellX,
        int cellY,
        TerrainWallFace face
    ) const;

    void DrawHybridWorld3D();
    void DrawHybridTerrain3D();
    void DrawHybridGroundEffects3D();
    void DrawHybridActors3D();
    void DrawHybridEditorOverlay3D();
    void DrawHybridScreenOverlays2D();

    void DrawHybridBillboardFrame(
        Texture2D texture,
        Rectangle source,
        Vector2 worldPosition,
        float widthPixels,
        float heightPixels,
        float anchorY,
        float additionalHeightPixels,
        Color tint,
        HybridBillboardOrientation orientation =
        HybridBillboardOrientation::FaceCamera,
        float whiteFlashAmount = 0.0f
    ) const;

    void DrawHybridGroundDisc(
        Vector2 worldPosition,
        float radiusPixels,
        Color color,
        float additionalHeightPixels = 0.0f,
        int segments = 24
    ) const;


    void DrawHuashanImpactGround2D();
    void DrawHuashanImpactGround3D();

    void DrawHybridGroundTextureFrame(
        Texture2D texture,
        Rectangle source,
        Vector2 worldPosition,
        float sizePixels,
        float heightBiasPixels,
        Color tint
    ) const;

    void DrawHybridGroundShadow(
        Vector2 worldPosition,
        float widthPixels,
        float depthPixels,
        Color color,
        float additionalHeightPixels = 0.0f
    ) const;

    void DrawHybridPlayer3D();
    void DrawHybridEnemy3D(const Enemy& enemy);
    void DrawHybridObstacle3D(const Obstacle& obstacle);
    void DrawHybridNpc3D(const NPC& npc);
    void DrawHybridProjectile3D(const Projectile& projectile);
    void DrawHybridOrbitalBlade3D(const OrbitalBlade& blade);
    void DrawHybridDashAfterimage3D(const DashAfterimage& afterimage);
    void DrawHybridVfx3D(const VfxParticle& particle);
    void DrawHybridDongfeng3D();

    bool GetActivePlayerFrame(
        Texture2D& outTexture,
        Rectangle& outSource,
        float& outWidthPixels,
        float& outHeightPixels,
        float& outAnchorY
    ) const;


    void LoadShooterSpriteSheets();

    void SetEnemyFacingFromWorldDirection(
        Enemy& enemy,
        Vector2 worldDirection
    );

    bool MoveShooterAwayFromPlayer(
        Enemy& enemy,
        float dt
    );

    bool GetShooterAnimationFrame(
        const Enemy& enemy,
        Texture2D& outTexture,
        Rectangle& outSource,
        float& outWidthPixels,
        float& outHeightPixels,
        float& outAnchorY
    ) const;

    void DrawShooterEnemySprite(
        const Enemy& enemy,
        Vector2 drawPosition,
        Color tint
    );

private:
    std::vector<int> tiles;
    std::vector<TerrainCell> terrainCells;
    std::vector<TileBrush> tileBrushes;

    Texture2D terrainTileSheet{};

    std::string terrainTileSheetPath =
        "Assets/tiles/dungeon_tilesheet.png";

    int terrainTileSheetColumns = 4;
    int terrainTileSheetRows = 4;

    char terrainTileSheetPathInput[512] =
        "Assets/tiles/dungeon_tilesheet.png";

    // Dungeon chamber runtime state.
    std::vector<DungeonChamber> chambers;
    int activeChamberId = -1;
    int previousChamberId = -1;

    float chamberFadeSpeed = 2.6f;
    float chamberWaveStartDelay = 1.10f;
    float chamberClearedMessageTimer = 0.0f;


    // Reused every frame for painter-style depth sorting.
    std::vector<WorldDrawItem> worldDrawItems;
    Vector2 playerPosition{ 0.0f, 0.0f };

    bool playerKnockbackActive = false;

    float playerKnockbackMaxTimer = 0.0f;
    float playerKnockbackPeakHeight = 145.0f;

    Vector2 playerKnockbackVelocity{
        0.0f,
        0.0f
    };

    float playerKnockbackTimer = 0.0f;
    float playerKnockbackDamping = 8.0f;

    // Gameplay/combat body radius.
    float playerRadius = 20.0f;

    // Additional terrain-edge clearance.
    // Used to prevent the visual sprite entering cliff walls.
    float playerTerrainCollisionRadius = 16.0f;

    // Maximum movement distance checked in one collision step.
// Smaller values improve ramps and corners but require more checks.
    float playerCollisionSubstep = 4.0f;

    // Amount of automatic sideways movement when the player pushes
    // directly into a wall. 0 disables it; 1 gives full sliding speed.
    float playerDirectWallGlideStrength = 0.60f;

    // Shared visual scale for walking, idle, attack and dash sprites.
    // 0.65f was the previous size.
    float playerSpriteDrawScale = 0.52f;
    // Transparent pixels underneath the visible feet


    float playerSpeed = 320.0f;

    // Movement speed while the attack button is held
// or an attack animation is still playing.
    float playerAttackMovementSpeedMultiplier =
        0.45f;

    // Maximum distance at which an enemy affects
    // the player's attack-facing direction.
    //
    // This does not cause the player to move.
    float playerAttackAutoFaceRange =
        480.0f;

    Camera2D camera{};

    // Hybrid renderer state. Gameplay remains in the existing 2D world
    // coordinates; only the presentation uses 3D geometry and depth.
    WorldRendererMode rendererMode =
        WorldRendererMode::Hybrid3D;

    Camera3D hybridCamera{};
    Vector2 hybridCameraTargetWorld{ 0.0f, 0.0f };
    float hybridCameraOrthoSize = 22.0f;
    float hybridCameraDistance = 42.0f;
    float hybridCameraFollowSpeed = 8.0f;

    // One gameplay tile equals one 3D world unit.
    float hybridUnitsPerPixel = 1.0f / TileSize;

    std::vector<HybridTerrainBatch> hybridTerrainBatches;
    bool hybridTerrainDirty = true;
    bool hybridTerrainReady = false;

    Shader hybridTerrainShader{};
    Shader hybridBillboardShader{};
    bool hybridTerrainShaderLoaded = false;
    bool hybridBillboardShaderLoaded = false;

    Texture2D hybridCircleTexture{};
    Texture2D hybridShadowTexture{};

    int hybridHoveredCellX = -1;
    int hybridHoveredCellY = -1;

    // F6 toggles the old top-down/isometric view only in legacy mode.
    bool useIsometricView = true;
    float isoVerticalScale = 0.62f;

    RenderTexture2D groundCache{};
    Vector2 groundCacheDrawPosition{};
    bool groundCacheReady = false;
    bool groundCacheDirty = true;




    // Delay between the third and later burst arrows.
//
// The second arrow happens immediately when the animation
// returns to frame 0.
    float shooterExtraShotInterval = 0.11f;

    Texture2D shooterIdleSpriteSheet{};
    Texture2D shooterWalkSpriteSheet{};
    Texture2D shooterAttackSpriteSheet{};

    bool shooterIdleSpriteLoaded = false;
    bool shooterWalkSpriteLoaded = false;
    bool shooterAttackSpriteLoaded = false;

    // All three sheets use the same frame dimensions.
    int shooterFrameWidth = 256;
    int shooterFrameHeight = 256;
    int shooterDirectionRows = 8;

    int shooterIdleFramesPerRow = 6;
    int shooterWalkFramesPerRow = 6;
    int shooterAttackFramesPerRow = 6;

    float shooterIdleFrameDuration = 0.14f;
    float shooterWalkFrameDuration = 0.10f;
    float shooterAttackFrameDuration = 0.085f;

    // Zero-based frame on which the projectile is created.
    int shooterAttackImpactFrame = 3;

    float shooterVisualScale = 0.52f;

    // Height of the bow/hand above the archer's feet.
    float shooterProjectileVisualHeight =
        72.0f;

    // Pushes the projectile slightly forward so it appears
    // outside the archer instead of inside their body.
    float shooterProjectileForwardOffset =
        16.0f;

    // Retreat after successfully firing.
    float shooterRetreatDuration = 0.60f;
    float shooterRetreatSpeedMultiplier = 1.35f;

    // Hit-stun settings.
    float shooterHitStunDuration = 0.28f;
    float normalEnemyHitStunDuration = 0.12f;
    float bossHitStunDuration = 0.05f;

    // Grunt and Runner share this floating blob animation.
    Texture2D smallEnemySpriteSheet{};
    bool smallEnemySpriteLoaded = false;

    // One horizontal row of square frames.
    int smallEnemyFrameWidth = 256;
    int smallEnemyFrameHeight = 256;
    int smallEnemyFramesPerRow = 5;

    int smallEnemyDirectionRows = 8;

    float smallEnemyFrameDuration = 0.12f;
    float smallEnemyVisualScale = 4.8f;

    // Player damage feedback.
    float playerDamageFlashTimer = 0.0f;

    // Duration of the white flash.
    float damageFlashDuration = 0.10f;

    // Walking / idle sprite sheet.
    Texture2D playerSpriteSheet{};
    bool playerSpriteLoaded = false;

    // Attack sprite sheet.
    Texture2D playerAttackSpriteSheet{};
    bool playerAttackSpriteLoaded = false;

    Texture2D playerIdleSpriteSheet{};
    bool playerIdleSpriteLoaded = false;

    int playerIdleFrameWidth = 256;
    int playerIdleFrameHeight = 256;
    int playerIdleFramesPerRow = 7;

    float playerIdleFrameDuration = 0.14f;

    // Walking sheet settings.
    int playerFrameWidth = 256;
    int playerFrameHeight = 256;
    int playerFramesPerRow = 7;

    // Attack sheet settings.
    int playerAttackFrameWidth = 256;
    int playerAttackFrameHeight = 256;
    int playerAttackFramesPerRow = 7;

    // Both sheets use eight directional rows.
    int playerDirectionRows = 8;

    // Shared animation playback values.
    int playerAnimFrame = 0;
    float playerAnimTimer = 0.0f;

    // Walking speed.
    float playerAnimFrameDuration = 0.075f;

    // Six attack frames × 0.07 seconds = 0.42 seconds,
    // matching the current player.attackInterval.
    float playerAttackFrameDuration = 0.07f;

    // Zero-based frame index.
    // Frame 3 means the fourth attack frame causes damage.
    int playerAttackImpactFrame = 3;

    // Attack state.
    PlayerAnimationState playerAnimationState =
        PlayerAnimationState::Idle;

    bool playerAttackImpactTriggered = false;
    bool playerAttackImpactPending = false;

    // Store the enemy ID instead of an Enemy pointer.
    // The enemies vector may be changed or cleaned up.
    int playerAttackTargetId = 0;

    PlayerDirection playerDirection =
        PlayerDirection::Down;

    PlayerDirection lastPlayerDirection =
        PlayerDirection::Down;

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
    bool showChamberOverlay = true;

    int editorTool = static_cast<int>(EditorTool::PaintTile);

    int editorNewMapWidth = 40;
    int editorNewMapHeight = 30;
    bool editorNewMapStartsEmpty = true;

    int editorSelectedChamberId = 0;
    char editorChamberNameInput[64] = "Chamber 0";

    // Terrain elevation is independent from obstacle-local stacking.
    float terrainElevationStep = 48.0f;
    int maxTerrainElevation = 6;
    int editorFlattenElevation = 0;

    int editorRampDirection =
        static_cast<int>(
            RampDirection::East
            );

    // Active obstacle-local stacking level selected in the editor.
    int editorHeightLevel = 0;

    // Screen/view-space height between stacked levels.
    float obstacleHeightStep = 44.0f;

    // Defaults for newly placed obstacles.
    bool newObstacleCastsShadow = true;
    bool newObstacleBlocksLight = false;
    bool newObstacleForeground = false;

    // When enabled, editor operations only select
    // obstacles on the active level.
    bool editorSelectActiveLevelOnly = true;

    int selectedTile = 0;

    int selectedObstacleType = 0;
    bool obstacleSnapToGrid = true;
    bool obstacleFitWidthToTiles = true;
    bool obstacleKeepAspectRatio = true;

    float obstacleVisualWidthTiles = 1.5f;

    Vector2 currentObstacleNativeSize{
        64.0f,
        64.0f
    };

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


    bool useBlobForMissingEnemySprites = true;

    // Transparent padding at the bottom of each sprite frame.
//
// This padding is removed before drawing so the visible feet
// can touch the ground without putting the billboard below it.
    float playerSpriteBottomTrim =
        10.0f;

    float shooterSpriteBottomTrim =
        14.0f;

    float bossSpriteBottomTrim =
        18.0f;

    Player player;

    std::vector<Enemy> enemies;
    std::vector<Projectile> projectiles;

    std::vector<PendingEnemySpawn>
        pendingEnemySpawns;

    std::vector<VfxParticle> vfxParticles;

    std::vector<BossFallingRock>
        bossFallingRocks;

    SkillSlot skills[3];

    WaveManager wave;

    // Stops only automatic wave spawning.
// Existing enemies continue updating normally.
    bool waveSpawningPaused = false;

    // True when the upgrade menu was opened through
    // the debug key or the on-screen button.
    bool upgradeMenuOpenedManually = false;

    Rectangle debugUpgradeButtonRect{};

    GameState gameState = GameState::Playing;

    bool useJoystickMovement = true;

    bool joystickActive = false;
    bool joystickWasTouching = false;
    int joystickTouchId = -1;

    Vector2 joystickDirection{ 0.0f, 0.0f };

    float joystickRadius = 68.0f;
    float joystickKnobRadius = 28.0f;

    std::vector<UpgradeChoice> currentUpgradeChoices;

    Rectangle attackButtonRect{};
    bool attackButtonDown = false;

    Rectangle dashButtonRect{};
    bool dashButtonDown = false;
    bool dashButtonWasDown = false;
    bool dashButtonPressed = false;

    bool dashActive = false;
    float dashTimer = 0.0f;
    float dashDuration = 0.25f;
    float dashSpeed = 980.0f;

    float dashCooldown = 0.75f;
    float dashCooldownRemaining = 0.0f;

    // Small grace window prevents damage on the exact frame
    // the dash movement finishes.
    float dashInvulnerabilityTimer = 0.0f;
    float dashInvulnerabilityGrace = 0.06f;

    Vector2 dashDirectionWorld{ 0.0f, 1.0f };

    float dashAfterimageTimer = 0.0f;
    float dashAfterimageInterval = 0.045f;
    float dashAfterimageLifetime = 0.15f;
    int maxDashAfterimages = 6;

    std::vector<DashAfterimage> dashAfterimages;

    float attackAssistPathTimer = 0.0f;

    std::vector<OrbitalBlade> orbitalBlades;

    bool huashanJumpActive = false;
    float huashanJumpTimer = 0.0f;
    float huashanJumpDuration = 0.24f;
    Vector2 huashanJumpStart{ 0.0f, 0.0f };
    Vector2 huashanJumpEnd{ 0.0f, 0.0f };
    Vector2 huashanImpactCenter{ 0.0f, 0.0f };
    float huashanDamageRadius = 170.0f;
    float huashanKnockbackRadius = 260.0f;
    int huashanMainDamage = 120;
    int huashanSplashDamage = 55;
    float huashanPrimaryKnockback = 1250.0f;
    float huashanOuterKnockback = 950.0f;
    float huashanAirborneDuration = 0.55f;
    float huashanLandingStunDuration = 1.50f;

    std::vector<HuashanGroundMark>
        huashanGroundMarks;

    float huashanGroundMarkFadeDuration =
        10.0f;

    // Screen shake
    float huashanScreenShakeTimer =
        0.0f;

    float huashanScreenShakeDuration =
        1.2f;

    float huashanScreenShakeStrength =
        60.0f;

    Vector2 huashanScreenShakeOffset{
        0.0f,
        0.0f
    };

    // Full-screen impact flash
    float huashanImpactFlashTimer =
        0.0f;

    float huashanImpactFlashDuration =
        0.4f;

    int huashanImpactFlashMaxAlpha =
        150;


    // --------------------------------------------------
// Normal enemy spawning effect
// --------------------------------------------------

    Texture2D enemySpawnEffectSpriteSheet{};

    bool enemySpawnEffectSpriteLoaded =
        false;

    // Change these to match the final sprite-sheet layout.
    int enemySpawnEffectColumns = 4;
    int enemySpawnEffectRows = 4;
    int enemySpawnEffectFrameCount = 16;

    int enemySpawnEffectFrameWidth = 512;
    int enemySpawnEffectFrameHeight = 512;

    // Circle animation before the enemy starts emerging.
    float enemySpawnGroundEffectDuration =
        0.70f;

    // Time taken for the enemy to rise fully from underground.
    float enemySpawnEmergenceDuration =
        1.40f;

    // After the enemy is fully above ground and active,
    // fade both the circle and bright enemy tint.
    float enemySpawnFadeDuration =
        0.45f;

    // Ground-effect diameter in world pixels.
    float enemySpawnEffectVisualSize =
        130.0f;

    bool dongfengCasting = false;
    float dongfengCastTimer = 0.0f;
    float dongfengCastDuration = 0.50f;



    int dongfengLockedTargetId = 0;

    Vector2 dongfengCastStart{ 0.0f, 0.0f };
    Vector2 dongfengCastDirection{ 1.0f, 0.0f };

    bool dongfengWaveActive = false;
    int dongfengWaveId = 0;

    Vector2 dongfengWaveStart{ 0.0f, 0.0f };
    Vector2 dongfengWavePos{ 0.0f, 0.0f };
    Vector2 dongfengWavePrevPos{ 0.0f, 0.0f };
    Vector2 dongfengWaveDirection{ 1.0f, 0.0f };

    float dongfengWaveTravelled = 0.0f;
    float dongfengRange = 1250.0f;
    float dongfengWaveSpeed = 700.0f;

    float dongfengStartRadius = 60.0f;
    float dongfengEndRadius = 140.0f;

    int dongfengMainDamage = 240;
    int dongfengLineDamage = 135;

    float dongfengKnockback = 3200.0f;

    int nextEnemyId = 1;
    int nextDongfengWaveId = 1;

    bool showPerformanceOverlay = true;

    float perfFrameMs = 0.0f;
    float perfAverageFrameMs = 0.0f;
    float perfWorstFrameMs = 0.0f;
    float perfWorstFrameTimer = 0.0f;

    float perfCombatMs = 0.0f;
    float perfSkillsMs = 0.0f;
    float perfHuashanMs = 0.0f;
    float perfDongfengMs = 0.0f;
    float perfWaveMs = 0.0f;
    float perfEnemiesMs = 0.0f;
    float perfMeleeMs = 0.0f;
    float perfBladesMs = 0.0f;
    float perfProjectilesMs = 0.0f;
    float perfCollisionMs = 0.0f;
    float perfVfxMs = 0.0f;
    float perfCleanupMs = 0.0f;


    float perfLastSpikeMs = 0.0f;
    float perfSpikeHoldTimer = 0.0f;
    int perfSpikeCount = 0;

    float perfFrameSpikeThresholdMs = 22.0f;
    float perfWorkSpikeThresholdMs = 10.0f;

    float perfUpdateTotalMs = 0.0f;
    float perfDrawTotalMs = 0.0f;

    float perfLastSpikeFrameMs = 0.0f;
    float perfLastSpikeUpdateMs = 0.0f;
    float perfLastSpikeDrawMs = 0.0f;
    float perfLastSpikeCombatMs = 0.0f;

    const char* perfLastSpikeReason = "None";

    double perfUpdateStartTime = 0.0;
    double perfDrawStartTime = 0.0;

    float perfDrawGroundMs = 0.0f;
    float perfDrawObstaclesMs = 0.0f;
    float perfDrawNpcsMs = 0.0f;
    float perfDrawCombatWorldMs = 0.0f;
    float perfDrawPlayerMs = 0.0f;
    float perfDrawWorldOverlayMs = 0.0f;
    float perfDrawUiMs = 0.0f;
    float perfDrawHudMs = 0.0f;
    float perfDrawSkillUiMs = 0.0f;
    float perfDrawControlsMs = 0.0f;
    float perfDrawOverlayMs = 0.0f;

    const char* perfLastSpikeDrawSection = "Unknown";


    float perfDrawCombatProjectilesMs = 0.0f;
    float perfDrawCombatEnemiesMs = 0.0f;
    float perfDrawCombatBladesMs = 0.0f;
    float perfDrawCombatDongfengTelegraphMs = 0.0f;
    float perfDrawCombatDongfengWaveMs = 0.0f;
    float perfDrawCombatVfxMs = 0.0f;

    const char* perfLastSpikeCombatDrawSection = "Unknown";

    float perfLastSpikeCDProjectilesMs = 0.0f;
    float perfLastSpikeCDEnemiesMs = 0.0f;
    float perfLastSpikeCDBladesMs = 0.0f;
    float perfLastSpikeCDDongfengTelegraphMs = 0.0f;
    float perfLastSpikeCDDongfengWaveMs = 0.0f;
    float perfLastSpikeCDVfxMs = 0.0f;

    std::deque<PerfSpikeRecord> perfSpikeRecords;

    int perfSpikeRecordCounter = 0;
    int perfMaxSpikeRecords = 80;

    bool perfRecordSpikes = true;
    float perfSpikeRecordCooldown = 0.0f;

    float perfFullFrameMs = 0.0f;
    float perfExternalUpdateMs = 0.0f;
    float perfExternalDrawMs = 0.0f;
    float perfEndDrawingMs = 0.0f;

    static constexpr int MaxVfxParticles = 120;

    int vfxSpawnedThisFrame = 0;
    int maxVfxSpawnPerFrame = 24;

    bool CanSpawnVfx(int count = 1);

    float perfBeginMode2DMs = 0.0f;
    float perfEndMode2DMs = 0.0f;
    float perfDrawUpgradeChoicesMs = 0.0f;
    float perfDrawDialogueMs = 0.0f;
    float perfDrawGameOverMs = 0.0f;
    float perfDrawUnaccountedMs = 0.0f;

    Shader isoGroundShader{};
    bool isoGroundShaderLoaded = false;
    int isoVerticalScaleLocation = -1;

    Shader dashAfterimageShader{};
    bool dashAfterimageShaderLoaded = false;
    int dashAfterimageColorLocation = -1;

    void LoadIsoGroundShader();
    void LoadDashAfterimageShader();

    void InitLighting();
    void EnsureLightingTargets();
    void DrawLightMap();

    void LoadHuashanImpactSpriteSheet();
    void StartHuashanImpactAnimation(Vector2 worldPosition);
    void UpdateHuashanImpactAnimation(float dt);
    void DrawHuashanImpactAnimation();

    void StartHuashanImpactFeedback();
    void UpdateHuashanImpactFeedback(float dt);
    void DrawHuashanImpactFlash() const;

    Rectangle GetHuashanImpactSourceRect(
        int frame
    ) const;

    Texture2D huashanImpactSpriteSheet{};
    bool huashanImpactSpriteLoaded = false;

    bool huashanImpactAnimationActive = false;
    Vector2 huashanImpactAnimationPosition{ 0.0f, 0.0f };

    int huashanImpactFrame = 0;
    float huashanImpactFrameTimer = 0.0f;

    int huashanImpactColumns = 4;
    int huashanImpactRows = 4;
    int huashanImpactFrameCount = 16;

    int huashanImpactFrameWidth = 256;
    int huashanImpactFrameHeight = 256;

    float huashanImpactFrameDuration = 0.03f;

    // Visual diameter in world/view pixels.
    float huashanImpactVisualSize = 800.0f;

    // --------------------------------------------------
 // 2D lighting
 // --------------------------------------------------

    std::vector<Light2D> lights;

    Texture2D radialLightTexture{};

    RenderTexture2D sceneTarget{};
    RenderTexture2D lightTarget{};

    Shader lightingShader{};

    int lightingLightMapLocation = -1;

    bool lightingReady = false;

    int lightingWidth = 0;
    int lightingHeight = 0;

    // This controls the darkness and colour of areas
    // that are not reached by a point light.
    //
    // Increase these RGB values for a brighter daytime scene.
    // Reduce them for a darker nighttime scene.
    Color ambientLight{
        105,
        112,
        132,
        255
    };

    // --------------------------------------------------
// Directional world shadows
// --------------------------------------------------

    // Direction that shadows extend on the displayed map.
    // Negative X = left.
    // Positive X = right.
    // Negative Y = upward.
    // Positive Y = downward.
    bool worldShadowsEnabled = true;

    Vector2 worldShadowDirectionView{
        -0.90f,
        0.34f
    };

    float worldShadowLengthScale = 0.0f;
    float worldShadowWidthScale = 0.0f;
    int worldShadowOpacity = 50;

    void LoadBossSpriteSheets();

    void UpdateBossAnimation(
        Enemy& enemy,
        float dt
    );

    bool GetBossAnimationFrame(
        const Enemy& enemy,
        Texture2D& outTexture,
        Rectangle& outSource,
        float& outWidthPixels,
        float& outHeightPixels,
        float& outAnchorY
    ) const;

    void DrawBossEnemySprite(
        const Enemy& enemy,
        Vector2 drawPosition,
        Color tint
    );

    void SpawnTestBoss();

    Texture2D bossIdleSpriteSheet{};
    Texture2D bossWalkSpriteSheet{};

    bool bossIdleSpriteLoaded = false;
    bool bossWalkSpriteLoaded = false;

    int bossFrameWidth = 256;
    int bossFrameHeight = 256;
    int bossDirectionRows = 8;

    int bossIdleFramesPerRow = 9;
    int bossWalkFramesPerRow = 9;

    float bossIdleFrameDuration = 0.14f;
    float bossWalkFrameDuration = 0.14f;

    // 256 × 0.90 = approximately 230 pixels tall.
    float bossVisualScale = 1.4f;

    void UpdateBossBehavior(
        Enemy& enemy,
        float dt,
        float distanceToPlayer,
        bool sameTerrainLevel
    );

    void StartBossSlam(
        Enemy& enemy
    );

    void ResolveBossSlamImpact(
        Enemy& enemy
    );

    void StartBossLaser(
        Enemy& enemy
    );

    void UpdateBossLaser(
        Enemy& enemy,
        float dt
    );

    Vector2 GetBossLaserBlockedEnd(
        Vector2 start,
        Vector2 desiredEnd
    ) const;

    void DrawBossLasers2D() const;
    void DrawBossLasersHybrid3D() const;

    void StartBossBombardment(
        Enemy& enemy
    );

    void StartBossBombardmentRoar(
        Enemy& enemy
    );

    void UpdateBossBombardment(
        Enemy& enemy,
        float dt
    );

    void SpawnBossFallingRock(
        const Enemy& enemy
    );

    void UpdateBossFallingRocks(
        float dt
    );

    void DrawBossFallingRockTelegraphs2D() const;
    void DrawBossFallingRockTelegraphsHybrid3D() const;

    void DrawBossFallingRocks2D() const;
    void DrawBossFallingRocksHybrid3D() const;

    float GetBossInterruptThreshold(
        const Enemy& enemy
    ) const;

    float GetBossSpecialStunDuration(
        const Enemy& enemy
    ) const;

    bool StartBossStunned(
        Enemy& enemy,
        float requestedDuration,
        bool forcedStun
    );

    void StartBossCharge(
        Enemy& enemy
    );

    void UpdateBossCharge(
        Enemy& enemy,
        float dt
    );

    void StartBossHealing(
        Enemy& enemy
    );

    void UpdateBossHealing(
        Enemy& enemy,
        float dt
    );

    void QueueBossHealingMinionWave(
        const Enemy& boss,
        int waveNumber
    );

    bool HasLivingBossHealingMinions(
        const Enemy& boss,
        int waveNumber
    ) const;

    void ProcessPendingEnemySpawns();


    // --------------------------------------------------
// Boss bombardment
// --------------------------------------------------

    float bossBombardmentDuration = 10.0f;
    float bossBombardmentCooldown = 16.0f;

    float bossBombardmentRetreatDuration = 0.42f;
    float bossBombardmentRetreatSpeed = 850.0f;

    // Base interval. Later HP tiers reduce this interval.
    float bossBombardmentRockInterval = 0.34f;

    // Rocks primarily target the area around the player.
    float bossBombardmentTargetSpread = 430.0f;

    float bossRockFallDurationMin = 0.90f;
    float bossRockFallDurationMax = 1.40f;

    float bossRockDamageRadiusMin = 70.0f;
    float bossRockDamageRadiusMax = 120.0f;

    float bossRockStartHeight = 700.0f;
    float bossRockVisualRadius = 38.0f;

    // --------------------------------------------------
    // Boss stagger
    // --------------------------------------------------

    // Percentage of maximum HP that must be dealt during
    // one laser or roar to interrupt the Boss.
    float bossInterruptHealthRatio = 0.12f;

    // --------------------------------------------------
    // Boss straight-line charge
    // --------------------------------------------------

    float bossChargeWindupDuration = 0.90f;
    float bossChargeSpeed = 1050.0f;

    // Safety limit. Normally the charge ends by hitting a wall.
    float bossChargeMaximumDuration = 3.50f;

    float bossChargeCooldown = 8.0f;

    float bossChargeWallStunDuration = 2.60f;

    float bossChargePlayerKnockbackForce = 1500.0f;
    float bossChargePlayerKnockbackDuration = 0.45f;

    Texture2D bossAttackSpriteSheet{};

    bool bossAttackSpriteLoaded = false;

    int bossAttackFramesPerRow = 9;

    // The supplied attack strip contains one directional row.
    int bossAttackDirectionRows = 1;

    float bossAttackFrameDuration = 0.10f;

    // Zero-based: frame 4 is the fifth frame.
    int bossAttackImpactFrame = 4;

    // Circular axe-slam attack.
    float bossSlamRadius = 150.0f;

    // Laser attack.
    float bossLaserWindupDuration = 3.f;
    float bossLaserDuration = 3.0f;
    float bossLaserCooldown = 7.0f;

    // Lower value creates more noticeable tracking delay.
    float bossLaserFollowSpeed = 0.8f;

    float bossLaserRange = 900.0f;


    float bossLaserDamageInterval = 0.5f;


    // --------------------------------------------------
// Boss forward chase dash
// --------------------------------------------------

    float bossForwardDashTriggerDistance = 520.0f;
    float bossForwardDashStopDistance = 320.0f;

    float bossForwardDashDuration = 0.4f;
    float bossForwardDashSpeed = 1050.0f;
    float bossForwardDashCooldown = 3.5f;

    float bossDashAfterimageInterval = 0.08f;

    // --------------------------------------------------
    // Boss slam
    // --------------------------------------------------

    float bossSlamWindupDuration = 0.72f;
    float bossSlamImpactVisualDuration = 0.55f;

    float bossSlamKnockbackForce = 3150.0f;
    float bossSlamKnockbackDuration = 0.5f;

    // Each tier represents another 25% HP lost,
// beginning at 75% HP.
    float bossMovementSpeedPerTier = 1.25f;
    float bossAttackSpeedPerTier = 0.32f;
    float bossDashPowerPerTier = 0.2f;
    float bossDashDurationPerTier = 0.09f;

    // Additional ability-frequency increase once the Boss
    // reaches 50% HP or lower.
    float bossLowHealthAbilityFrequencyMultiplier =
        1.35f;

    // How frequently the Boss rolls for roar and charge.
    float bossDecisionInterval =
        0.80f;

    float bossLowHealthDecisionInterval =
        0.45f;

    // --------------------------------------------------
    // Laser
    // --------------------------------------------------

    // Boss is approximately 358 pixels tall at scale 1.4.
    // This places the laser around the upper torso/chest.
    float bossLaserVisualHeight = 200.0f;

    float bossLaserForwardOffset = 42.0f;

    Texture2D bossRoarSpriteSheet{};
    Texture2D bossStunnedSpriteSheet{};

    Texture2D bossPreHealSpriteSheet{};
    Texture2D bossHealingLoopSpriteSheet{};

    bool bossPreHealSpriteLoaded = false;
    bool bossHealingLoopSpriteLoaded = false;

    int bossPreHealFramesPerRow = 9;
    int bossHealingLoopFramesPerRow = 9;

    int bossPreHealDirectionRows = 1;
    int bossHealingLoopDirectionRows = 1;

    float bossPreHealFrameDuration = 0.10f;
    float bossHealingLoopFrameDuration = 0.11f;

    // Stationary preparation animation.
    Texture2D bossPreChargeSpriteSheet{};

    // Moving charge animation.
    Texture2D bossChargeSpriteSheet{};

    bool bossRoarSpriteLoaded = false;
    bool bossStunnedSpriteLoaded = false;
    bool bossPreChargeSpriteLoaded = false;
    bool bossChargeSpriteLoaded = false;

    int bossRoarFramesPerRow = 9;
    int bossStunnedFramesPerRow = 9;

    // Uploaded animation currently has 9 frames.
    int bossPreChargeFramesPerRow = 9;

    int bossChargeFramesPerRow = 9;

    int bossRoarDirectionRows = 1;
    int bossStunnedDirectionRows = 1;

    // Both charge-related sheets are directional.
    int bossPreChargeDirectionRows = 8;
    int bossChargeDirectionRows = 8;

    float bossRoarFrameDuration = 0.11f;
    float bossStunnedFrameDuration = 0.13f;

    // Two full pre-charge loops before movement starts.
    int bossPreChargeLoopCount = 2;

    float bossPreChargeFrameDuration = 0.09f;
    float bossChargeFrameDuration = 0.075f;


    // --------------------------------------------------
// Boss healing ability
// --------------------------------------------------

// May activate only below this health percentage.
    float bossHealingTriggerHealthRatio =
        0.50f;

    // Maximum number of healing phases per Boss.
    int bossHealingMaximumUses =
        3;

    // Restores 20% of maximum HP during each phase.
    float bossHealingAmountRatio =
        0.20f;

    // Active healing duration.
    float bossHealingDuration =
        20.0f;

    // Boss takes only 20% of incoming damage while
    // preparing or actively healing.
    float bossHealingDamageTakenMultiplier =
        0.20f;

    // Prevents repeated healing phases immediately after
    // the ending stun.
    float bossHealingCooldown =
        8.0f;

    // Three waves, five enemies per wave.
    int bossHealingMinionWaveCount =
        3;

    int bossHealingMinionsPerWave =
        5;

    // Wave 1 occurs immediately, then waves at
    // approximately 2.5 and 5 seconds.
    float bossHealingMinionWaveInterval =
        2.50f;

    float bossHealingMinionSpawnRadius =
        185.0f;

    // Ending stun is double the Boss's usual special stun.
    float bossHealingEndStunMultiplier =
        2.0f;

    // Ensures the healing stun is always substantial.
    float bossHealingMinimumEndStunDuration =
        6.0f;

    // Previously 20.
    float bossLaserWidth = 170.0f;
    int GetBossPowerTier(
        const Enemy& enemy
    ) const;

    float GetBossMovementSpeedMultiplier(
        const Enemy& enemy
    ) const;

    float GetBossAttackSpeedMultiplier(
        const Enemy& enemy
    ) const;

    float GetBossDashPowerMultiplier(
        const Enemy& enemy
    ) const;

    float GetBossDashDurationMultiplier(
        const Enemy& enemy
    ) const;

    void StartBossForwardDash(
        Enemy& enemy
    );

    void SpawnBossDashAfterimage(
        const Enemy& enemy
    );

    void ApplyKnockbackToPlayer(
        Vector2 origin,
        float force,
        float duration
    );

    void UpdatePlayerKnockback(
        float dt
    );

    Color GetBossTierTint(
        const Enemy& enemy
    ) const;

    void DrawBossUnlitEffects() const;

    void DrawBossSlamDomeHybrid3D() const;

    float GetPlayerVisualHeight() const;


};
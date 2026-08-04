#!/usr/bin/env python3
"""Apply the fast two-chamber progression and Boss presentation patch.

Run from the inner Moxiang2D project folder:
    python tools/apply_fast_two_chamber_gameplay_patch.py

Or pass the folder containing Game.cpp and Game.h:
    python apply_fast_two_chamber_gameplay_patch.py --root D:/Dev/Moxiang2D/Moxiang2D

The script creates one-time .bak copies before editing.
"""

from pathlib import Path
import argparse
import shutil


def find_source_root(requested: Path) -> Path:
    requested = requested.resolve()
    candidates = [requested, requested / "Moxiang2D"]

    for candidate in candidates:
        if (candidate / "Game.h").is_file() and (candidate / "Game.cpp").is_file():
            return candidate

    raise SystemExit(
        "Could not find Game.h and Game.cpp. Run from the inner Moxiang2D folder "
        "or pass --root D:/Dev/Moxiang2D/Moxiang2D"
    )


parser = argparse.ArgumentParser()
parser.add_argument("--root", type=Path, default=Path.cwd())
args = parser.parse_args()
source_root = find_source_root(args.root)
hpath = source_root / "Game.h"
cpppath = source_root / "Game.cpp"

h = hpath.read_text(encoding='utf-8')
cpp = cpppath.read_text(encoding='utf-8')

if 'static constexpr int BossChamberId = 1;' in h:
    raise SystemExit('Fast two-chamber gameplay patch already appears to be applied.')

header_backup = hpath.with_suffix(hpath.suffix + '.before_fast_two_chamber.bak')
source_backup = cpppath.with_suffix(cpppath.suffix + '.before_fast_two_chamber.bak')

if not header_backup.exists():
    shutil.copy2(hpath, header_backup)
if not source_backup.exists():
    shutil.copy2(cpppath, source_backup)

print(f'Backup: {header_backup}')
print(f'Backup: {source_backup}')

def rep(text, old, new, label, count=1):
    found=text.count(old)
    if found < count:
        raise RuntimeError(f'{label}: expected at least {count}, found {found}')
    return text.replace(old,new,count)

# ---------------- Header ----------------
h=rep(h,
'''    // Retreat dash, followed by bombardment roar.\n    BombardmentEscape\n};''',
'''    // Retreat dash, followed by bombardment roar.\n    BombardmentEscape,\n\n    // Scripted dash to the exact chamber centre before a special.\n    CenterRoar,\n    CenterLaser\n};''','boss purpose')

h=rep(h,
'''    float bossDashTimer = 0.0f;\n    Vector2 bossDashVelocity{};\n\n    bool active = false;''',
'''    float bossDashTimer = 0.0f;\n    Vector2 bossDashVelocity{};\n\n    // Used by scripted centre dashes before roar and laser.\n    Vector2 bossDashTargetPosition{};\n    bool bossDashUsesTargetPosition = false;\n\n    bool active = false;''','boss dash target')

h=rep(h,
'''    void DrawSkillUi();\n    void DrawCombatHud();\n    void UpdatePerformanceStats(float dt);''',
'''    void DrawSkillUi();\n    void DrawCombatHud();\n    const Enemy* FindActiveBoss() const;\n    void DrawBossHealthBar(const Enemy& boss) const;\n    void UpdatePerformanceStats(float dt);''','boss hud declarations')

h=rep(h,
'''    void UnlockSkill(int slotIndex);\n    void LevelUpSkill(int slotIndex);\n\n    bool CanPlayerStandAt(Vector2 worldPosition) const;''',
'''    void UnlockSkill(int slotIndex);\n    void LevelUpSkill(int slotIndex);\n\n    void HandleEnemyDefeated(const Enemy& enemy);\n    void TryOpenPendingSkillChoice();\n    void ApplyAutomaticSkillUpgrade();\n    bool AreAllCoreSkillsUnlocked() const;\n    int GetNextSkillProgressionTarget() const;\n\n    bool CanPlayerStandAt(Vector2 worldPosition) const;''','progress declarations')

h=rep(h,
'''    void StartBossLaser(\n        Enemy& enemy\n    );\n\n    void UpdateBossLaser(''',
'''    void StartBossLaser(\n        Enemy& enemy\n    );\n\n    void StartBossCenterDash(\n        Enemy& enemy,\n        BossDashPurpose purpose\n    );\n\n    void UpdateBossLaser(''','center dash declaration')

h=rep(h,
'''    SkillSlot skills[3];\n\n    WaveManager wave;''',
'''    SkillSlot skills[3];\n\n    // Fast vertical-slice progression. Chamber 0 contains normal enemies;\n    // chamber 1 is the dedicated boss encounter.\n    static constexpr int BossChamberId = 1;\n\n    int enemiesDefeatedTotal = 0;\n\n    std::array<int, 3> skillUnlockKillThresholds{\n        2,\n        5,\n        9\n    };\n\n    int nextSkillUnlockThresholdIndex = 0;\n    int pendingSkillChoiceCount = 0;\n\n    int nextAutomaticUpgradeKillCount = 12;\n    int automaticUpgradeKillInterval = 3;\n    int automaticUpgradeRoundRobinSlot = 0;\n\n    float automaticUpgradeMessageTimer = 0.0f;\n    int automaticUpgradeMessageSlot = -1;\n    int automaticUpgradeMessageLevel = 0;\n\n    WaveManager wave;''','progress members')

# ---------------- CPP ----------------
cpp=rep(cpp,
'''        chamber.wavesRequired = 2;\n        chamber.wavesCompleted = 0;''',
'''        chamber.wavesRequired =\n            chamber.id == BossChamberId\n            ? 1\n            : 2;\n\n        chamber.wavesCompleted = 0;''','reset wave count')

cpp=rep(cpp,
'''    chamber->encounterStarted = true;\n    chamber->wavesRequired = 2;\n    chamber->wavesCompleted = 0;''',
'''    chamber->encounterStarted = true;\n    chamber->wavesRequired =\n        chamber->id == BossChamberId\n        ? 1\n        : 2;\n\n    chamber->wavesCompleted = 0;''','start encounter wave count')

old_skills='''    skills[0].type = SkillType::SpinningBlade; // Wind Blades\n    skills[0].cooldown = 3.0f;\n    skills[0].cooldownRemaining = 0.0f;\n    skills[0].unlocked = true;\n    skills[0].level = 1;\n\n    skills[1].type = SkillType::Huashan;\n    skills[1].cooldown = 5.0f;\n    skills[1].cooldownRemaining = 0.0f;\n    skills[1].unlocked = false;\n    skills[1].level = 1;\n\n    skills[2].type = SkillType::Dongfeng;\n    skills[2].cooldown = 8.0f;\n    skills[2].cooldownRemaining = 0.0f;\n    skills[2].unlocked = false;\n    skills[2].level = 1;'''
new_skills='''    enemiesDefeatedTotal = 0;\n    nextSkillUnlockThresholdIndex = 0;\n    pendingSkillChoiceCount = 0;\n\n    nextAutomaticUpgradeKillCount = 12;\n    automaticUpgradeRoundRobinSlot = 0;\n    automaticUpgradeMessageTimer = 0.0f;\n    automaticUpgradeMessageSlot = -1;\n    automaticUpgradeMessageLevel = 0;\n\n    // The player begins with no active skills. Skill-choice screens\n    // unlock them at 2, 5 and 9 defeated normal enemies.\n    skills[0].type = SkillType::SpinningBlade;\n    skills[0].cooldown = 2.0f;\n    skills[0].cooldownRemaining = 0.0f;\n    skills[0].unlocked = false;\n    skills[0].level = 1;\n\n    skills[1].type = SkillType::Huashan;\n    skills[1].cooldown = 3.5f;\n    skills[1].cooldownRemaining = 0.0f;\n    skills[1].unlocked = false;\n    skills[1].level = 1;\n\n    skills[2].type = SkillType::Dongfeng;\n    skills[2].cooldown = 5.5f;\n    skills[2].cooldownRemaining = 0.0f;\n    skills[2].unlocked = false;\n    skills[2].level = 1;'''
cpp=rep(cpp,old_skills,new_skills,'init skills')

cpp=rep(cpp,
'''    vfxSpawnedThisFrame = 0;\n\n    double combatStartTime = GetTime();''',
'''    vfxSpawnedThisFrame = 0;\n\n    automaticUpgradeMessageTimer =\n        std::max(\n            0.0f,\n            automaticUpgradeMessageTimer - dt\n        );\n\n    double combatStartTime = GetTime();''','update toast timer')

cpp=rep(cpp,
'''    if (player.hp <= 0)\n    {\n        player.hp = 0;\n        gameState = GameState::GameOver;\n    }\n}''',
'''    if (player.hp <= 0)\n    {\n        player.hp = 0;\n        gameState = GameState::GameOver;\n        return;\n    }\n\n    // Open progression menus only after all combat loops for this frame\n    // have finished. This avoids invalidating an enemy iteration when an\n    // area attack defeats several enemies at once.\n    TryOpenPendingSkillChoice();\n}''','update combat end')

cpp=rep(cpp,
'''    wave.enemiesSpawned = 0;\n    wave.enemiesToSpawn =\n        5 +\n        std::min(\n            chamberOrder,\n            4\n        ) +\n        (chamberWave - 1) * 2;\n\n    wave.spawnTimer = 0.0f;\n    wave.spawnInterval =\n        std::max(\n            0.32f,\n            0.72f -\n            static_cast<float>(chamberOrder) * 0.035f -\n            static_cast<float>(chamberWave - 1) * 0.06f\n        );\n\n    wave.waveActive = true;\n    wave.waitingForNextWave = true;\n    wave.nextWaveTimer =\n        chamberWave == 1\n        ? chamberWaveStartDelay\n        : 1.35f;''',
'''    const bool bossChamber =\n        chamber->id == BossChamberId;\n\n    wave.enemiesSpawned = 0;\n\n    // Chamber 0 is a fast 18-enemy progression arena. Chamber 1\n    // contains one Boss and no ordinary waves.\n    wave.enemiesToSpawn =\n        bossChamber\n        ? 1\n        : (\n            chamberWave == 1\n            ? 8\n            : 10\n            );\n\n    wave.spawnTimer = 0.0f;\n    wave.spawnInterval =\n        bossChamber\n        ? 0.20f\n        : 0.30f;\n\n    wave.waveActive = true;\n    wave.waitingForNextWave = true;\n    wave.nextWaveTimer =\n        bossChamber\n        ? 0.45f\n        : (\n            chamberWave == 1\n            ? 0.35f\n            : 0.45f\n            );''','wave setup')

old_type='''            const int roll =\n                GetRandomValue(\n                    1,\n                    100\n                );\n\n            EnemyType type =\n                EnemyType::Grunt;\n\n            if (roll <= 45)\n            {\n                type = EnemyType::Grunt;\n            }\n            else if (roll <= 68)\n            {\n                type = EnemyType::Runner;\n            }\n            else if (roll <= 88)\n            {\n                type = EnemyType::Shooter;\n            }\n            else\n            {\n                type = EnemyType::Tank;\n            }'''
new_type='''            EnemyType type =\n                EnemyType::Grunt;\n\n            if (chamber->id == BossChamberId)\n            {\n                type = EnemyType::Boss;\n            }\n            else\n            {\n                const int roll =\n                    GetRandomValue(\n                        1,\n                        100\n                    );\n\n                if (roll <= 42)\n                {\n                    type = EnemyType::Grunt;\n                }\n                else if (roll <= 67)\n                {\n                    type = EnemyType::Runner;\n                }\n                else if (roll <= 88)\n                {\n                    type = EnemyType::Shooter;\n                }\n                else\n                {\n                    type = EnemyType::Tank;\n                }\n            }'''
cpp=rep(cpp,old_type,new_type,'wave spawn type')

cpp=rep(cpp,
'''    enemy.pos =\n        GetRandomSpawnPositionInChamber(\n            chamberId\n        );\n    enemy.active = true;''',
'''    enemy.pos =\n        GetRandomSpawnPositionInChamber(\n            chamberId\n        );\n\n    // The Boss enters from the northern side of chamber 1. The centre\n    // remains reserved for its scripted roar/laser dash destination.\n    if (\n        type == EnemyType::Boss &&\n        chamberId == BossChamberId\n        )\n    {\n        const DungeonChamber* bossChamber =\n            FindChamberById(\n                chamberId\n            );\n\n        if (bossChamber != nullptr)\n        {\n            const Vector2 preferredPosition{\n                bossChamber->worldBounds.x +\n                    bossChamber->worldBounds.width * 0.5f,\n\n                bossChamber->worldBounds.y +\n                    bossChamber->worldBounds.height * 0.24f\n            };\n\n            int spawnCellX = 0;\n            int spawnCellY = 0;\n\n            if (\n                FindNearestWalkableCell(\n                    preferredPosition,\n                    spawnCellX,\n                    spawnCellY\n                )\n                )\n            {\n                enemy.pos =\n                    CellToWorld(\n                        spawnCellX,\n                        spawnCellY\n                    );\n            }\n        }\n    }\n\n    enemy.active = true;''','boss spawn position')

cpp=rep(cpp,
'''    GenerateSkillChoices();\n\n    gameState =\n        GameState::ChoosingUpgrade;''',
'''    GenerateSkillChoices();\n\n    if (currentUpgradeChoices.empty())\n    {\n        upgradeMenuOpenedManually = false;\n        return;\n    }\n\n    gameState =\n        GameState::ChoosingUpgrade;''','manual upgrade empty')

# Replace full ApplyUpgradeChoice function up to UnlockSkill.
start=cpp.index('void Game::ApplyUpgradeChoice(int choiceIndex)')
end=cpp.index('void Game::UnlockSkill(int slotIndex)', start)
new_apply='''void Game::ApplyUpgradeChoice(int choiceIndex)\n{\n    if (\n        choiceIndex < 0 ||\n        choiceIndex >=\n        static_cast<int>(\n            currentUpgradeChoices.size()\n        )\n        )\n    {\n        return;\n    }\n\n    const UpgradeChoice choice =\n        currentUpgradeChoices[choiceIndex];\n\n    const int slotIndex =\n        FindSkillSlotIndex(\n            choice.skillType\n        );\n\n    // The choice screen is now exclusively an unlock screen. Skill\n    // levels are awarded automatically by kill milestones.\n    if (\n        slotIndex >= 0 &&\n        !skills[slotIndex].unlocked\n        )\n    {\n        UnlockSkill(\n            slotIndex\n        );\n\n        skills[slotIndex].level = 1;\n    }\n\n    upgradeMenuOpenedManually = false;\n    currentUpgradeChoices.clear();\n\n    gameState =\n        GameState::Playing;\n\n    attackButtonDown = false;\n    dashButtonDown = false;\n    dashButtonPressed = false;\n    joystickActive = false;\n    joystickDirection = { 0.0f, 0.0f };\n\n    // Area attacks can cross more than one threshold in the same frame.\n    // Open the next queued unlock immediately after the previous choice.\n    TryOpenPendingSkillChoice();\n\n    // If all skills became unlocked after kills had already passed an\n    // automatic milestone, catch those upgrades up now.\n    if (AreAllCoreSkillsUnlocked())\n    {\n        while (\n            enemiesDefeatedTotal >=\n            nextAutomaticUpgradeKillCount\n            )\n        {\n            ApplyAutomaticSkillUpgrade();\n\n            nextAutomaticUpgradeKillCount +=\n                automaticUpgradeKillInterval;\n        }\n    }\n}\n\n'''
cpp=cpp[:start]+new_apply+cpp[end:]

# Insert progression function definitions after LevelUpSkill.
level_start=cpp.index('void Game::LevelUpSkill(int slotIndex)')
level_end=cpp.index('void Game::UpdateJoystick(float dt)', level_start)
level_func=cpp[level_start:level_end]
progress_defs='''bool Game::AreAllCoreSkillsUnlocked() const\n{\n    for (const SkillSlot& skill : skills)\n    {\n        if (!skill.unlocked)\n        {\n            return false;\n        }\n    }\n\n    return true;\n}\n\nint Game::GetNextSkillProgressionTarget() const\n{\n    if (\n        !AreAllCoreSkillsUnlocked() &&\n        nextSkillUnlockThresholdIndex <\n        static_cast<int>(\n            skillUnlockKillThresholds.size()\n        )\n        )\n    {\n        return\n            skillUnlockKillThresholds[\n                nextSkillUnlockThresholdIndex\n            ];\n    }\n\n    return nextAutomaticUpgradeKillCount;\n}\n\nvoid Game::ApplyAutomaticSkillUpgrade()\n{\n    int minimumLevel =\n        std::numeric_limits<int>::max();\n\n    for (const SkillSlot& skill : skills)\n    {\n        if (skill.unlocked)\n        {\n            minimumLevel =\n                std::min(\n                    minimumLevel,\n                    skill.level\n                );\n        }\n    }\n\n    if (minimumLevel == std::numeric_limits<int>::max())\n    {\n        return;\n    }\n\n    int selectedSlot = -1;\n\n    for (int offset = 0; offset < 3; ++offset)\n    {\n        const int slot =\n            (\n                automaticUpgradeRoundRobinSlot +\n                offset\n                ) %\n            3;\n\n        if (\n            skills[slot].unlocked &&\n            skills[slot].level == minimumLevel\n            )\n        {\n            selectedSlot = slot;\n            break;\n        }\n    }\n\n    if (selectedSlot < 0)\n    {\n        return;\n    }\n\n    LevelUpSkill(\n        selectedSlot\n    );\n\n    automaticUpgradeRoundRobinSlot =\n        (selectedSlot + 1) % 3;\n\n    automaticUpgradeMessageSlot =\n        selectedSlot;\n\n    automaticUpgradeMessageLevel =\n        skills[selectedSlot].level;\n\n    automaticUpgradeMessageTimer =\n        2.2f;\n\n    TraceLog(\n        LOG_INFO,\n        "[PROGRESSION] Auto-upgraded %s to level %d at %d defeats.",\n        GetSkillDisplayName(\n            skills[selectedSlot].type\n        ),\n        skills[selectedSlot].level,\n        enemiesDefeatedTotal\n    );\n}\n\nvoid Game::HandleEnemyDefeated(\n    const Enemy& enemy\n)\n{\n    // The final Boss does not grant a post-fight upgrade.\n    if (enemy.type == EnemyType::Boss)\n    {\n        return;\n    }\n\n    enemiesDefeatedTotal++;\n\n    while (\n        nextSkillUnlockThresholdIndex <\n            static_cast<int>(\n                skillUnlockKillThresholds.size()\n            ) &&\n        enemiesDefeatedTotal >=\n            skillUnlockKillThresholds[\n                nextSkillUnlockThresholdIndex\n            ]\n        )\n    {\n        pendingSkillChoiceCount++;\n        nextSkillUnlockThresholdIndex++;\n    }\n\n    if (AreAllCoreSkillsUnlocked())\n    {\n        while (\n            enemiesDefeatedTotal >=\n            nextAutomaticUpgradeKillCount\n            )\n        {\n            ApplyAutomaticSkillUpgrade();\n\n            nextAutomaticUpgradeKillCount +=\n                automaticUpgradeKillInterval;\n        }\n    }\n}\n\nvoid Game::TryOpenPendingSkillChoice()\n{\n    if (\n        gameState != GameState::Playing ||\n        buildMode ||\n        pendingSkillChoiceCount <= 0\n        )\n    {\n        return;\n    }\n\n    GenerateSkillChoices();\n\n    if (currentUpgradeChoices.empty())\n    {\n        pendingSkillChoiceCount = 0;\n        return;\n    }\n\n    pendingSkillChoiceCount--;\n    upgradeMenuOpenedManually = false;\n    gameState = GameState::ChoosingUpgrade;\n\n    attackButtonDown = false;\n    dashButtonDown = false;\n    dashButtonPressed = false;\n    joystickActive = false;\n    joystickDirection = { 0.0f, 0.0f };\n}\n\n'''
cpp=cpp[:level_end]+progress_defs+cpp[level_end:]

cpp=rep(cpp,
'''Rectangle Game::GetUpgradeCardRect(int index) const\n{\n    constexpr int choiceCount = 3;''',
'''Rectangle Game::GetUpgradeCardRect(int index) const\n{\n    const int choiceCount =\n        std::max(\n            1,\n            static_cast<int>(\n                currentUpgradeChoices.size()\n            )\n        );''','dynamic choice count')

# Replace GenerateSkillChoices function.
gen_start=cpp.index('void Game::GenerateSkillChoices()')
gen_end=cpp.index('Vector2 Game::GetJoystickBaseScreen() const', gen_start)
new_gen='''void Game::GenerateSkillChoices()\n{\n    currentUpgradeChoices.clear();\n\n    const SkillType pool[3] =\n    {\n        SkillType::SpinningBlade,\n        SkillType::Huashan,\n        SkillType::Dongfeng\n    };\n\n    for (SkillType type : pool)\n    {\n        const int slotIndex =\n            FindSkillSlotIndex(\n                type\n            );\n\n        // Already-unlocked skills are never presented as manual upgrades.\n        if (\n            slotIndex < 0 ||\n            skills[slotIndex].unlocked\n            )\n        {\n            continue;\n        }\n\n        UpgradeChoice choice;\n        choice.skillType = type;\n        choice.title = GetSkillDisplayName(type);\n        choice.subtitle = GetSkillSubtitle(type);\n        choice.description = GetSkillDescription(type);\n        choice.unlocksSkill = true;\n        choice.targetLevel = 1;\n        choice.cardRect = {};\n\n        currentUpgradeChoices.push_back(\n            choice\n        );\n    }\n\n    UpdateUpgradeChoiceLayout();\n}\n\n'''
cpp=cpp[:gen_start]+new_gen+cpp[gen_end:]

cpp=rep(cpp,
'''        SpawnDeathBurst(\n            enemy.pos\n        );\n\n        return;''',
'''        SpawnDeathBurst(\n            enemy.pos\n        );\n\n        HandleEnemyDefeated(\n            enemy\n        );\n\n        return;''','death progression')

# Remove world-space boss health bars in Legacy2D.
cpp=rep(cpp,
'''        if (enemy.hp < enemy.maxHp || enemy.type == EnemyType::Boss)''',
'''        if (\n            enemy.type != EnemyType::Boss &&\n            enemy.hp < enemy.maxHp\n            )''','legacy boss bar')

# Remove world-space boss health bars in Hybrid3D.
cpp=rep(cpp,
'''            !enemy.active ||\n            !IsChamberVisible(\n                enemy.chamberId\n            ) ||\n            (\n                enemy.hp >= enemy.maxHp &&\n                enemy.type != EnemyType::Boss\n                )''',
'''            !enemy.active ||\n            !IsChamberVisible(\n                enemy.chamberId\n            ) ||\n            enemy.type == EnemyType::Boss ||\n            enemy.hp >= enemy.maxHp''','hybrid boss bar')

# Insert global boss health bar functions and alter HUD origin.
cpp=rep(cpp,
'''void Game::DrawCombatHud()\n{\n    const int y = 68;''',
'''const Enemy* Game::FindActiveBoss() const\n{\n    for (const Enemy& enemy : enemies)\n    {\n        if (\n            enemy.active &&\n            enemy.type == EnemyType::Boss &&\n            enemy.chamberId == activeChamberId\n            )\n        {\n            return &enemy;\n        }\n    }\n\n    return nullptr;\n}\n\nvoid Game::DrawBossHealthBar(\n    const Enemy& boss\n) const\n{\n    const float screenWidth =\n        static_cast<float>(\n            GetScreenWidth()\n        );\n\n    const float barWidth =\n        Clamp(\n            screenWidth * 0.58f,\n            420.0f,\n            900.0f\n        );\n\n    const float barHeight = 24.0f;\n    const float barX =\n        screenWidth * 0.5f -\n        barWidth * 0.5f;\n\n    const float barY = 78.0f;\n\n    const float hpRatio =\n        Clamp(\n            static_cast<float>(boss.hp) /\n            static_cast<float>(\n                std::max(1, boss.maxHp)\n            ),\n            0.0f,\n            1.0f\n        );\n\n    const char* bossName =\n        "AZURE WARDEN";\n\n    const int nameFontSize = 22;\n    const int nameWidth =\n        MeasureText(\n            bossName,\n            nameFontSize\n        );\n\n    DrawText(\n        bossName,\n        static_cast<int>(\n            screenWidth * 0.5f -\n            static_cast<float>(nameWidth) * 0.5f\n        ),\n        50,\n        nameFontSize,\n        Color{ 242, 220, 170, 255 }\n    );\n\n    Rectangle background{\n        barX - 4.0f,\n        barY - 4.0f,\n        barWidth + 8.0f,\n        barHeight + 8.0f\n    };\n\n    DrawRectangleRounded(\n        background,\n        0.18f,\n        8,\n        Color{ 8, 10, 16, 230 }\n    );\n\n    DrawRectangleRoundedLinesEx(\n        background,\n        0.18f,\n        8,\n        2.0f,\n        Color{ 225, 190, 105, 255 }\n    );\n\n    Rectangle emptyBar{\n        barX,\n        barY,\n        barWidth,\n        barHeight\n    };\n\n    Rectangle filledBar =\n        emptyBar;\n\n    filledBar.width *=\n        hpRatio;\n\n    DrawRectangleRounded(\n        emptyBar,\n        0.20f,\n        8,\n        Color{ 45, 20, 28, 255 }\n    );\n\n    if (filledBar.width > 1.0f)\n    {\n        DrawRectangleRounded(\n            filledBar,\n            0.20f,\n            8,\n            Color{ 190, 42, 52, 255 }\n        );\n    }\n\n    const char* hpText =\n        TextFormat(\n            "%d / %d",\n            boss.hp,\n            boss.maxHp\n        );\n\n    const int hpFontSize = 17;\n    const int hpTextWidth =\n        MeasureText(\n            hpText,\n            hpFontSize\n        );\n\n    DrawText(\n        hpText,\n        static_cast<int>(\n            screenWidth * 0.5f -\n            static_cast<float>(hpTextWidth) * 0.5f\n        ),\n        static_cast<int>(\n            barY + 3.0f\n        ),\n        hpFontSize,\n        WHITE\n    );\n}\n\nvoid Game::DrawCombatHud()\n{\n    const Enemy* activeBoss =\n        FindActiveBoss();\n\n    if (activeBoss != nullptr)\n    {\n        DrawBossHealthBar(\n            *activeBoss\n        );\n    }\n\n    const int y =\n        activeBoss != nullptr\n        ? 126\n        : 68;''','boss hud functions')

# Expand HUD panel and add progression line.
cpp=rep(cpp,
'''        430,\n        110,''',
'''        430,\n        134,''','hud height')

cpp=rep(cpp,
'''    DrawText(\n        encounterStatus,\n        18,\n        y + 80,\n        17,\n        exitColor\n    );\n\n    if (chamberClearedMessageTimer > 0.0f)''',
'''    DrawText(\n        encounterStatus,\n        18,\n        y + 80,\n        17,\n        exitColor\n    );\n\n    const int nextProgressTarget =\n        GetNextSkillProgressionTarget();\n\n    const char* progressionLabel =\n        AreAllCoreSkillsUnlocked()\n        ? "Next automatic skill upgrade"\n        : "Next skill choice";\n\n    DrawText(\n        TextFormat(\n            "Defeats: %d   %s: %d",\n            enemiesDefeatedTotal,\n            progressionLabel,\n            nextProgressTarget\n        ),\n        18,\n        y + 104,\n        16,\n        Color{ 190, 220, 255, 255 }\n    );\n\n    if (\n        automaticUpgradeMessageTimer > 0.0f &&\n        automaticUpgradeMessageSlot >= 0 &&\n        automaticUpgradeMessageSlot < 3\n        )\n    {\n        const char* upgradeText =\n            TextFormat(\n                "%s AUTOMATICALLY UPGRADED TO LV.%d",\n                GetSkillDisplayName(\n                    skills[automaticUpgradeMessageSlot].type\n                ),\n                automaticUpgradeMessageLevel\n            );\n\n        const int upgradeFontSize = 24;\n        const int upgradeTextWidth =\n            MeasureText(\n                upgradeText,\n                upgradeFontSize\n            );\n\n        const int upgradeBoxWidth =\n            upgradeTextWidth + 44;\n\n        const int upgradeBoxX =\n            GetScreenWidth() / 2 -\n            upgradeBoxWidth / 2;\n\n        const int upgradeBoxY =\n            activeBoss != nullptr\n            ? 116\n            : 72;\n\n        DrawRectangle(\n            upgradeBoxX,\n            upgradeBoxY,\n            upgradeBoxWidth,\n            46,\n            Color{ 0, 18, 28, 220 }\n        );\n\n        DrawRectangleLines(\n            upgradeBoxX,\n            upgradeBoxY,\n            upgradeBoxWidth,\n            46,\n            Color{ 80, 220, 255, 255 }\n        );\n\n        DrawText(\n            upgradeText,\n            upgradeBoxX + 22,\n            upgradeBoxY + 10,\n            upgradeFontSize,\n            Color{ 130, 235, 255, 255 }\n        );\n    }\n\n    if (chamberClearedMessageTimer > 0.0f)''','hud progression')

# Insert centre dash implementation before StartBossLaser.
cpp=rep(cpp,
'''void Game::StartBossLaser(\n    Enemy& enemy\n)''',
'''void Game::StartBossCenterDash(\n    Enemy& enemy,\n    BossDashPurpose purpose\n)\n{\n    if (\n        enemy.type != EnemyType::Boss ||\n        !enemy.active ||\n        enemy.bossActionState != BossActionState::None ||\n        enemy.bossDashCharging ||\n        enemy.bossDashTimer > 0.0f ||\n        (\n            purpose != BossDashPurpose::CenterRoar &&\n            purpose != BossDashPurpose::CenterLaser\n            )\n        )\n    {\n        return;\n    }\n\n    const DungeonChamber* chamber =\n        FindChamberById(\n            enemy.chamberId\n        );\n\n    if (chamber == nullptr)\n    {\n        return;\n    }\n\n    Vector2 centre{\n        chamber->worldBounds.x +\n            chamber->worldBounds.width * 0.5f,\n\n        chamber->worldBounds.y +\n            chamber->worldBounds.height * 0.5f\n    };\n\n    // Resolve the exact centre to the closest valid floor cell. The updated\n    // level generator intentionally leaves this cell and its approach clear.\n    int centreCellX = 0;\n    int centreCellY = 0;\n\n    if (\n        FindNearestWalkableCell(\n            centre,\n            centreCellX,\n            centreCellY\n        )\n        )\n    {\n        centre =\n            CellToWorld(\n                centreCellX,\n                centreCellY\n            );\n    }\n\n    const Vector2 displacement =\n        Vector2Subtract(\n            centre,\n            enemy.pos\n        );\n\n    const float distance =\n        Vector2Length(\n            displacement\n        );\n\n    if (distance <= 4.0f)\n    {\n        enemy.pos = centre;\n\n        if (purpose == BossDashPurpose::CenterRoar)\n        {\n            StartBossBombardmentRoar(\n                enemy\n            );\n        }\n        else\n        {\n            StartBossLaser(\n                enemy\n            );\n        }\n\n        return;\n    }\n\n    const float dashSpeed =\n        1150.0f *\n        GetBossDashPowerMultiplier(\n            enemy\n        );\n\n    const float duration =\n        Clamp(\n            distance /\n            std::max(\n                1.0f,\n                dashSpeed\n            ),\n            0.16f,\n            0.72f\n        );\n\n    enemy.bossDashPurpose = purpose;\n    enemy.bossDashTargetPosition = centre;\n    enemy.bossDashUsesTargetPosition = true;\n    enemy.bossDashIsForward = false;\n    enemy.bossDashTimer = duration;\n    enemy.bossDashVelocity =\n        Vector2Scale(\n            displacement,\n            1.0f / duration\n        );\n\n    enemy.bossDashAfterimageTimer = 0.0f;\n    enemy.path.clear();\n    enemy.pathIndex = 0;\n    enemy.animationState = EnemyAnimationState::Walking;\n\n    SetEnemyFacingFromWorldDirection(\n        enemy,\n        displacement\n    );\n\n    SpawnBossDashAfterimage(\n        enemy\n    );\n}\n\nvoid Game::StartBossLaser(\n    Enemy& enemy\n)''','insert center dash')

# Dash afterimages should also display for scripted center movement.
cpp=rep(cpp,
'''        if (enemy.bossDashIsForward)\n        {\n            enemy.bossDashAfterimageTimer -=''',
'''        if (\n            enemy.bossDashIsForward ||\n            enemy.bossDashUsesTargetPosition\n            )\n        {\n            enemy.bossDashAfterimageTimer -=''','center dash afterimage')

cpp=rep(cpp,
'''            if (enemy.bossDashIsForward)\n            {\n                SpawnBossDashAfterimage(\n                    enemy\n                );\n            }\n\n            enemy.bossDashTimer =''',
'''            const bool completedTargetDash =\n                enemy.bossDashUsesTargetPosition;\n\n            if (\n                enemy.bossDashIsForward ||\n                completedTargetDash\n                )\n            {\n                SpawnBossDashAfterimage(\n                    enemy\n                );\n            }\n\n            if (completedTargetDash)\n            {\n                // Finish exactly on the authored arena centre even when the\n                // final fixed-timestep movement would otherwise stop short.\n                enemy.pos =\n                    enemy.bossDashTargetPosition;\n            }\n\n            enemy.bossDashTimer =''','center dash completion start')

cpp=rep(cpp,
'''            enemy.bossDashIsForward =\n                false;\n\n            enemy.bossDashVelocity = {''',
'''            enemy.bossDashIsForward =\n                false;\n\n            enemy.bossDashUsesTargetPosition =\n                false;\n\n            enemy.bossDashVelocity = {''','center dash reset')

cpp=rep(cpp,
'''            else if (\n                completedPurpose ==\n                BossDashPurpose::BombardmentEscape\n                )\n            {\n                StartBossBombardmentRoar(\n                    enemy\n                );\n            }\n\n            // ----------------------------------------------\n            // Health-phase escape simply resumes movement.\n            // ----------------------------------------------\n\n            else''',
'''            else if (\n                completedPurpose ==\n                BossDashPurpose::BombardmentEscape ||\n                completedPurpose ==\n                BossDashPurpose::CenterRoar\n                )\n            {\n                StartBossBombardmentRoar(\n                    enemy\n                );\n            }\n\n            else if (\n                completedPurpose ==\n                BossDashPurpose::CenterLaser\n                )\n            {\n                StartBossLaser(\n                    enemy\n                );\n            }\n\n            // ----------------------------------------------\n            // Health-phase escape simply resumes movement.\n            // ----------------------------------------------\n\n            else''','center dash dispatch')

# Change random special triggers to center dash.
cpp=rep(cpp,
'''            StartBossBombardmentRoar(\n                enemy\n            );\n\n            UpdateBossAnimation(''',
'''            StartBossCenterDash(\n                enemy,\n                BossDashPurpose::CenterRoar\n            );\n\n            UpdateBossAnimation(''','roar center trigger', count=1)

# Replace the direct laser selection near the decision block, not StartBossCenterDash internals.
needle='''        StartBossLaser(\n            enemy\n        );\n\n        UpdateBossAnimation(\n            enemy,\n            dt\n        );\n\n        return;\n    }\n\n    // --------------------------------------------------\n    // Basic circular axe slam.'''
replacement='''        StartBossCenterDash(\n            enemy,\n            BossDashPurpose::CenterLaser\n        );\n\n        UpdateBossAnimation(\n            enemy,\n            dt\n        );\n\n        return;\n    }\n\n    // --------------------------------------------------\n    // Basic circular axe slam.'''
cpp=rep(cpp,needle,replacement,'laser center trigger')

hpath.write_text(h,encoding='utf-8')
cpppath.write_text(cpp,encoding='utf-8')
print(f'Patched: {hpath}')
print(f'Patched: {cpppath}')

#include "raylib.h"
#include <vector>
#include <cmath>
#include <string>
#include <algorithm>

// Paleta de cores profissional
#define COLOR_BG         CLITERAL(Color){ 245, 245, 250, 255 }
#define COLOR_GRID       CLITERAL(Color){ 220, 220, 230, 120 }
#define COLOR_PATH       CLITERAL(Color){ 120, 180, 255, 180 }
#define COLOR_ENEMY      CLITERAL(Color){ 220, 80, 80, 255 }
#define COLOR_ENEMY_HIT  CLITERAL(Color){ 255, 180, 180, 255 }
#define COLOR_TOWER      CLITERAL(Color){ 60, 120, 220, 255 }
#define COLOR_PROJECTILE CLITERAL(Color){ 255, 180, 60, 255 }
#define COLOR_HUD_BG     CLITERAL(Color){ 30, 30, 40, 180 }
#define COLOR_HUD_TEXT   CLITERAL(Color){ 240, 240, 255, 255 }

// --- Declaração antecipada da função de sombra ---
static void DrawCircleShadow(Vector2 pos, float radius, Color color);

// --- Funções auxiliares de vetor ---
static Vector2 Vector2Add(Vector2 a, Vector2 b) {
    return { a.x + b.x, a.y + b.y };
}
static Vector2 Vector2Subtract(Vector2 a, Vector2 b) {
    return { a.x - b.x, a.y - b.y };
}
static Vector2 Vector2Scale(Vector2 v, float scale) {
    return { v.x * scale, v.y * scale };
}
static float Vector2Length(Vector2 v) {
    return sqrtf(v.x * v.x + v.y * v.y);
}
static Vector2 Vector2Normalize(Vector2 v) {
    float len = Vector2Length(v);
    if (len == 0.0f) return { 0.0f, 0.0f };
    return { v.x / len, v.y / len };
}

// --- Configurações do jogo ---
const int screenWidth = 960;
const int screenHeight = 540;
// Quadriculado menor: aumente o número de células
const int mapWidth = 24;
const int mapHeight = 15;
const int maxLives = 10;

// O grid agora é dinâmico para preencher a tela
float gridSizeX = (float)screenWidth / (float)mapWidth;
float gridSizeY = (float)screenHeight / (float)mapHeight;

// --- Estruturas ---
struct Enemy {
    Vector2 pos;
    int pathIndex;
    float speed;
    float hp, maxHp;
    bool alive;
};

struct Tower {
    Vector2 pos;
    float range;
    float fireRate;
    float fireCooldown;
};

struct Projectile {
    Vector2 pos, target;
    float speed;
    bool active;
};

struct PathNode {
    Vector2 pos;
};

// --- Variáveis globais ---
std::vector<PathNode> path;
std::vector<Enemy> enemies;
std::vector<Tower> towers;
std::vector<Projectile> projectiles;
int money = 100;
int lives = maxLives;
int wave = 1;
float spawnTimer = 0.0f;
int enemiesToSpawn = 0;
bool placingTower = false;
bool gameOver = false;
float waveDelay = 1.5f; // Delay entre as waves
float waveDelayTimer = 0.0f;

// --- Funções auxiliares ---
static float Distance(Vector2 a, Vector2 b) {
    return sqrtf((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}

// Caminho em L, mas agora usando grid dinâmico
static void InitPath() {
    path.clear();
    path.push_back({ Vector2{0.0f, gridSizeY * 4.0f + gridSizeY / 2.0f} });
    path.push_back({ Vector2{gridSizeX * 10.0f + gridSizeX / 2.0f, gridSizeY * 4.0f + gridSizeY / 2.0f} });
    path.push_back({ Vector2{gridSizeX * 10.0f + gridSizeX / 2.0f, gridSizeY * 12.0f + gridSizeY / 2.0f} });
    path.push_back({ Vector2{gridSizeX * 22.0f + gridSizeX / 2.0f, gridSizeY * 12.0f + gridSizeY / 2.0f} });
}

static void StartWave() {
    enemiesToSpawn = 5 + wave * 2 + wave * wave / 4; // Progressão mais interessante
    spawnTimer = 0.0f;
    waveDelayTimer = 0.0f;
}

static void SpawnEnemy() {
    Enemy e = {};
    e.pos = path[0].pos;
    e.pathIndex = 0;
    e.speed = 60.0f + wave * 2.0f + (wave > 5 ? wave * 1.5f : 0.0f);
    e.maxHp = 40.0f + wave * 10.0f + (wave > 5 ? wave * 8.0f : 0.0f);
    e.hp = e.maxHp;
    e.alive = true;
    enemies.push_back(e);
}

static void UpdateEnemies(float dt) {
    for (auto& e : enemies) {
        if (!e.alive) continue;
        if (e.pathIndex < (int)path.size() - 1) {
            Vector2 target = path[e.pathIndex + 1].pos;
            Vector2 dir = Vector2Normalize(Vector2Subtract(target, e.pos));
            float moveDist = e.speed * dt;
            float distToTarget = Distance(e.pos, target);
            if (moveDist >= distToTarget) {
                e.pos = target;
                e.pathIndex++;
            }
            else {
                e.pos = Vector2Add(e.pos, Vector2Scale(dir, moveDist));
            }
        }
        else {
            e.alive = false;
            lives--;
            if (lives <= 0) gameOver = true;
        }
    }
}

static void UpdateTowers(float dt) {
    for (auto& t : towers) {
        t.fireCooldown -= dt;
        if (t.fireCooldown <= 0.0f) {
            // Atira no inimigo mais próximo do final
            Enemy* best = nullptr;
            float bestProgress = -1.0f;
            for (auto& e : enemies) {
                if (e.alive && Distance(t.pos, e.pos) < t.range) {
                    size_t nextIndex = (e.pathIndex + 1 < path.size()) ? e.pathIndex + 1 : e.pathIndex;
                    float progress = (float)e.pathIndex + 1.0f - Distance(e.pos, path[nextIndex].pos) / (gridSizeX + gridSizeY);
                    if (progress > bestProgress) {
                        bestProgress = progress;
                        best = &e;
                    }
                }
            }
            if (best) {
                Projectile p = {};
                p.pos = t.pos;
                p.target = best->pos;
                p.speed = 320.0f;
                p.active = true;
                projectiles.push_back(p);
                t.fireCooldown = t.fireRate;
            }
        }
    }
}

static void UpdateProjectiles(float dt) {
    for (auto& p : projectiles) {
        if (!p.active) continue;
        Vector2 dir = Vector2Normalize(Vector2Subtract(p.target, p.pos));
        float step = p.speed * dt;
        float dist = Distance(p.pos, p.target);
        if (step >= dist) {
            p.pos = p.target;
            p.active = false;
        }
        else {
            p.pos = Vector2Add(p.pos, Vector2Scale(dir, step));
        }
        for (auto& e : enemies) {
            if (e.alive && Distance(p.pos, e.pos) < gridSizeY * 0.28f) {
                e.hp -= 30.0f;
                p.active = false;
                if (e.hp <= 0) {
                    e.alive = false;
                    money += 10 + wave * 2;
                }
                break;
            }
        }
    }
    // Limpa projéteis mortos
    projectiles.erase(std::remove_if(projectiles.begin(), projectiles.end(), [](const Projectile& p) { return !p.active; }), projectiles.end());
}

// Linha fina preta entre os nós do caminho
static void DrawPath() {
    float thickness = fminf(gridSizeX, gridSizeY) * 0.22f;
    for (size_t i = 0; i < path.size() - 1; i++) {
        Color grad = Fade(COLOR_PATH, 0.7f - 0.2f * (float)i / (path.size()-2));
        DrawLineEx(path[i].pos, path[i + 1].pos, thickness, grad);
    }
}

static void DrawMap() {
    // Grid
    for (int y = 0; y < mapHeight; y++)
        for (int x = 0; x < mapWidth; x++)
            DrawRectangleLines((int)((float)x * gridSizeX), (int)((float)y * gridSizeY), (int)gridSizeX, (int)gridSizeY, COLOR_GRID);

    DrawPath();
}

static void DrawEnemies() {
    for (auto& e : enemies) {
        if (!e.alive) continue;
        Color c = (e.hp < e.maxHp * 0.7f) ? COLOR_ENEMY_HIT : COLOR_ENEMY;
        DrawCircleShadow(e.pos, gridSizeY * 0.38f, c);
        float ratio = e.hp / e.maxHp;
        Rectangle rect1;
        rect1.x = e.pos.x - gridSizeX * 0.42f;
        rect1.y = e.pos.y - gridSizeY * 0.58f;
        rect1.width = gridSizeX * 0.84f;
        rect1.height = 8.0f;
        DrawRectangleRounded(rect1, 0.5f, 8, DARKGRAY);

        Rectangle rect2;
        rect2.x = e.pos.x - gridSizeX * 0.42f;
        rect2.y = e.pos.y - gridSizeY * 0.58f;
        rect2.width = gridSizeX * 0.84f * ratio;
        rect2.height = 8.0f;
        DrawRectangleRounded(rect2, 0.5f, 8, GREEN);
    }
}

// Desenha o range da torre com linha mais grossa
static void DrawTowerRange(Vector2 center, float radius, Color color, int thickness) {
    for (int i = 0; i < thickness; i++) {
        DrawCircleLines((int)center.x, (int)center.y, (int)(radius - i), color);
    }
}

static void DrawTowers() {
    for (auto& t : towers) {
        DrawCircleShadow(t.pos, gridSizeY * 0.42f, COLOR_TOWER);
        DrawTowerRange(t.pos, t.range, Fade(COLOR_TOWER, 0.18f), 8);
        DrawCircleLines((int)t.pos.x, (int)t.pos.y, (int)(gridSizeY * 0.42f), Fade(BLACK, 0.25f));
    }
}

static void DrawProjectiles() {
    for (auto& p : projectiles)
        if (p.active) {
            DrawCircleV(p.pos, gridSizeY * 0.22f, Fade(COLOR_PROJECTILE, 0.7f));
            DrawCircleV(p.pos, gridSizeY * 0.12f, COLOR_PROJECTILE);
        }
}

static void DrawHUD() {
    DrawRectangle(0, 0, 260, 140, COLOR_HUD_BG);
    DrawText(TextFormat("Money: $%d", money), 18, 14, 26, COLOR_HUD_TEXT);
    DrawText(TextFormat("Lives: %d", lives), 18, 44, 26, COLOR_HUD_TEXT);
    DrawText(TextFormat("Wave: %d", wave), 18, 74, 26, COLOR_HUD_TEXT);
    DrawText("T: construir torre", 18, 104, 20, Fade(COLOR_HUD_TEXT, 0.7f));
    if (placingTower)
        DrawText("Clique para posicionar (direito cancela)", 18, 128, 18, Fade(BLUE, 0.7f));
    if (gameOver)
        DrawText("GAME OVER! R para reiniciar", 300, 250, 36, RED);
    if (waveDelayTimer > 0.0f)
        DrawText("Próxima onda em breve...", screenWidth / 2 - 120, 10, 24, DARKGRAY);
}

// Verifica se a célula está na trilha (usando distância do centro da célula para cada segmento da trilha)
static bool IsOnPath(Vector2 cellCenter) {
    float pathWidth = gridSizeY * 0.8f;
    for (size_t i = 0; i < path.size() - 1; i++) {
        Vector2 a = path[i].pos;
        Vector2 b = path[i + 1].pos;
        Vector2 ab = Vector2Subtract(b, a);
        Vector2 ac = Vector2Subtract(cellCenter, a);
        float abLen = Vector2Length(ab);
        float proj = (ab.x * ac.x + ab.y * ac.y) / (abLen * abLen);
        proj = fmaxf(0.0f, fminf(1.0f, proj));
        Vector2 closest = Vector2Add(a, Vector2Scale(ab, proj));
        if (Distance(cellCenter, closest) < pathWidth / 2.0f)
            return true;
    }
    return false;
}

// Verifica se já existe torre na célula
static bool IsTowerAtCell(int gx, int gy) {
    Vector2 pos = { gx * gridSizeX + gridSizeX / 2.0f, gy * gridSizeY + gridSizeY / 2.0f };
    for (const auto& t : towers)
        if (Distance(t.pos, pos) < gridSizeX * 0.2f)
            return true;
    return false;
}

// Exemplo de sombra para círculos:
static void DrawCircleShadow(Vector2 pos, float radius, Color color) {
    Vector2 shadowPos;
    shadowPos.x = pos.x + radius * 0.15f;
    shadowPos.y = pos.y + radius * 0.18f;
    DrawCircleV(shadowPos, radius * 1.08f, Fade(BLACK, 0.18f));
    DrawCircleV(pos, radius, color);
}

// --- Função principal ---
int main() {
    InitWindow(screenWidth, screenHeight, "Tower Defense - Raylib");
    SetTargetFPS(60);

    gridSizeX = (float)screenWidth / (float)mapWidth;
    gridSizeY = (float)screenHeight / (float)mapHeight;

    InitPath();
    StartWave();

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (!gameOver) {
            if (enemiesToSpawn > 0 && waveDelayTimer <= 0.0f) {
                spawnTimer -= dt;
                if (spawnTimer <= 0.0f) {
                    SpawnEnemy();
                    enemiesToSpawn--;
                    spawnTimer = 1.0f;
                }
            }
            else if (enemiesToSpawn == 0 && std::all_of(enemies.begin(), enemies.end(), [](const Enemy& e) {return !e.alive; })) {
                wave++;
                waveDelayTimer = waveDelay;
                StartWave();
            }

            if (waveDelayTimer > 0.0f) {
                waveDelayTimer -= dt;
                if (waveDelayTimer < 0.0f) waveDelayTimer = 0.0f;
            }

            UpdateEnemies(dt);
            UpdateTowers(dt);
            UpdateProjectiles(dt);

            // Colocação de torre
            Vector2 m = GetMousePosition();
            int gx = (int)(m.x / gridSizeX), gy = (int)(m.y / gridSizeY);
            Vector2 cellCenter = { gx * gridSizeX + gridSizeX / 2.0f, gy * gridSizeY + gridSizeY / 2.0f };

            if (placingTower) {
                if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) placingTower = false;
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && money >= 50) {
                    if (gx >= 0 && gx < mapWidth && gy >= 0 && gy < mapHeight) {
                        if (!IsOnPath(cellCenter) && !IsTowerAtCell(gx, gy)) {
                            Tower t = {};
                            t.pos = cellCenter;
                            t.range = gridSizeY * 2.5f;
                            t.fireRate = 0.7f;
                            t.fireCooldown = 0.0f;
                            towers.push_back(t);
                            money -= 50;
                        }
                    }
                    placingTower = false;
                }
            }
            if (IsKeyPressed(KEY_T)) placingTower = true;
        }
        else {
            if (IsKeyPressed(KEY_R)) {
                enemies.clear();
                towers.clear();
                projectiles.clear();
                money = 100;
                lives = maxLives;
                wave = 1;
                gameOver = false;
                waveDelayTimer = 0.0f;
                StartWave();
            }
        }

        // --- Desenho ---
        BeginDrawing();
        ClearBackground(COLOR_BG);

        DrawMap();
        DrawEnemies();
        DrawTowers();
        DrawProjectiles();
        DrawHUD();

        // Realce de célula e range ao posicionar torre
        if (placingTower) {
            Vector2 m = GetMousePosition();
            int gx = (int)(m.x / gridSizeX), gy = (int)(m.y / gridSizeY);
            if (gx >= 0 && gx < mapWidth && gy >= 0 && gy < mapHeight) {
                Vector2 cellCenter = { gx * gridSizeX + gridSizeX / 2.0f, gy * gridSizeY + gridSizeY / 2.0f };
                Color c = (IsOnPath(cellCenter) || IsTowerAtCell(gx, gy)) ? Fade(RED, 0.4f) : Fade(GREEN, 0.3f);
                DrawRectangle((int)((float)gx * gridSizeX), (int)((float)gy * gridSizeY), (int)gridSizeX, (int)gridSizeY, c);
                // Range mais grosso na pré-visualização
                DrawTowerRange(cellCenter, gridSizeY * 2.5f, Fade(BLUE, 0.3f), 6);
            }
        }

        // Feedback de derrota
        if (gameOver) {
            DrawRectangle(0, 0, screenWidth, screenHeight, Fade(RED, 0.2f));
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
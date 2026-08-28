#include "chai3d.h"
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>


#include <GLFW/glfw3.h>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <mutex>


using namespace chai3d;
using namespace std;


// SIGSEGV Signal Handler for C++ Crash Stack Trace Diagnosis
void segsegv_handler(int sig) {
    void *array[50];
    size_t size = backtrace(array, 50);
    fprintf(stderr, "\n[FATAL] CRITICAL ERROR: Segmentation Fault (Signal %d) detected!\n", sig);
    fprintf(stderr, "[FATAL] Printing C++ stack trace for diagnosis:\n");
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    fprintf(stderr, "[FATAL] Stack trace print completed. Exiting.\n");
    exit(1);
}

// =============================================================================
// SAFE MATERIAL & COLOR HELPERS (Segfault Protection)
// =============================================================================
template<typename T>
inline void safeSetColor(T* obj, const cColorf& color) {
    if (obj) {
        if (!obj->m_material) {
            obj->m_material = cMaterial::create();
        }
        cColorf tempColor = color;
        obj->m_material->setColor(tempColor);
    }
}

inline cColorf safeGetDiffuse(cGenericObject* obj) {
    if (obj) {
        if (!obj->m_material) {
            obj->m_material = cMaterial::create();
        }
        return obj->m_material->m_diffuse;
    }
    return cColorf(1.0f, 1.0f, 1.0f, 1.0f);
}

inline void safeSetDiffuse(cGenericObject* obj, const cColorf& color) {
    if (obj) {
        if (!obj->m_material) {
            obj->m_material = cMaterial::create();
        }
        obj->m_material->m_diffuse = color;
    }
}


// Define C_RAD_TO_DEG to ensure compatibility across all CHAI3D versions
#ifndef C_RAD_TO_DEG
#define C_RAD_TO_DEG 57.29577951308232
#endif

// =============================================================================
// GLOBAL CONFIGURATIONS & GAME STATE
// =============================================================================
int playerLives = 5;
int playerGold = 150;
int playerScore = 0;
int currentWave = 0; // Fix: Initialize to 0 so the first call to triggerNextWave makes it Wave 1!
const int TOWER_COST = 50;
const double TOWER_RADIUS = 0.5;
const double TOWER_HEIGHT = 1.5;

bool gameOver = false;
bool gameWin = false;

// Auto-wave transition state variables
bool waveInTransition = false;
double waveTransitionTimer = 0.0;
const double WAVE_TRANSITION_DELAY = 5.0; // 5 seconds of celebration/celebratory delay

// Screen dimensions
int windowWidth = 1024;
int windowHeight = 768;

// CHAI3D core elements
cWorld* world;
cCamera* camera;
cDirectionalLight* light;
cMesh* terrain;

// UI HUD elements
cFontPtr font;
cLabel* statusLabel = nullptr;
cLabel* statsLabel = nullptr;
cLabel* instructionsLabel = nullptr;

// =============================================================================
// THREADING & MUTEX SYNCHRONIZATION
// =============================================================================
std::recursive_mutex game_mutex;
bool hapticsLoopRunning = false;

// =============================================================================
// HAPTIC DEVICE & TOOL MANAGEMENT
// =============================================================================
cHapticDeviceHandler* handler = nullptr;
std::shared_ptr<cGenericHapticDevice> hapticDevice = nullptr;
bool hapticDeviceReady = false; // Flag to track if physical hardware was actually opened
cShapeSphere* hapticCursor = nullptr;

// Haptic feedback physical parameters (Spring-Damper Model)
const double GROUND_STIFFNESS = 450.0; // N/m (Hooke's Law spring coefficient)
const double GROUND_DAMPING = 8.0;     // N-s/m (viscous damping)
const double TOWER_STIFFNESS = 300.0;  // Force to feel towers as solid cylinders
const double CURSOR_RADIUS = 0.15;


// Shared thread flags
cVector3d sharedCursorPos(0, 0, 0);
cVector3d sharedCursorVel(0, 0, 0);
bool placeTowerRequested = false;
cVector3d towerPlacementLocation(0, 0, 0);

// Safe main-thread spawning variables
int enemiesLeftToSpawn = 0;
double enemySpawnTimer = 0.0;
const double ENEMY_SPAWN_COOLDOWN = 1.5; // seconds


// =============================================================================
// GAME ENTITIES DEFINITIONS
// =============================================================================

// Waypoint pathing configuration
vector<cVector3d> waypoints;

// Enemy Structure (using cMultiMesh for multi-mesh obj loading capability)
struct Enemy {
    cMultiMesh* mesh = nullptr;
    cVector3d position;
    double maxHealth = 100.0;
    double health = 100.0;
    double speed = 1.2;
    int currentWaypoint = 0;
    bool active = true;
    bool isAir = false; // Ground tank vs Air plane
    double actionTimer = 0.0; // Cooldown for tank attacks or plane bomb drops
};

// Tower Structure
struct Tower {
    cMesh* baseMesh = nullptr;
    cMesh* turretMesh = nullptr;
    cVector3d position;
    double range = 4.0;
    double fireRate = 1.2; // shots per second
    double cooldownTimer = 0.0;
    double damage = 35.0;
    double maxHealth = 100.0;
    double health = 100.0;
};

// Projectile Structure (Object-pooled)
struct Projectile {
    cShapeSphere* mesh = nullptr;
    cVector3d position;
    cVector3d velocity;
    Enemy* target = nullptr;
    double speed = 8.0;
    double damage = 35.0;
    bool active = false;
};

// Explosion Effect Structure
struct Explosion {
    cShapeSphere* mesh = nullptr;
    double lifetime = 0.0;
    double maxLifetime = 0.3; // seconds
    double currentRadius = 0.1;
    double maxRadius = 0.8;
    bool active = true;
};

// Bomb entity structure for air plane attacks
struct Bomb {
    cShapeSphere* mesh = nullptr;
    cVector3d position;
    double speed = 6.0; // Falling speed
    bool active = false;
};

// Tank Shell structure for tank counter-attacks
struct TankShell {
    cShapeSphere* mesh = nullptr;
    cVector3d position;
    cVector3d targetPos;
    Tower* targetTower = nullptr;
    double speed = 12.0; // Shell velocity
    double damage = 15.0; // Attack damage
    bool active = false;
};

// Entity lists
vector<Enemy*> activeEnemies;
vector<Tower*> activeTowers;
vector<Projectile> projectilePool;
vector<Explosion> activeExplosions;
const int PROJECTILE_POOL_SIZE = 40;

const int BOMB_POOL_SIZE = 30;
vector<Bomb> bombPool;

const int TANK_SHELL_POOL_SIZE = 30;
vector<TankShell> tankShellPool;

// Initialize Bomb Pool
void initBombPool() {
    bombPool.clear();
    for (int i = 0; i < BOMB_POOL_SIZE; ++i) {
        Bomb b;
        b.mesh = new cShapeSphere(0.12); // Bomb sphere mesh
        cColorf bombColor(0.2f, 0.2f, 0.2f, 1.0f); // Dark tactical grey bomb
        safeSetColor(b.mesh, bombColor);
        b.mesh->setEnabled(false);
        world->addChild(b.mesh);
        bombPool.push_back(b);
    }
}

// Request and spawn a falling bomb from a plane
void spawnBomb(const cVector3d& startPos) {
    for (auto& b : bombPool) {
        if (!b.active) {
            b.active = true;
            b.position = startPos;
            b.mesh->setEnabled(true);
            b.mesh->setLocalPos(startPos);
            std::cout << "[BOMB_DROP] Plane dropped a bomb at " << startPos << std::endl; std::cout.flush();
            return;
        }
    }
}

// Initialize Tank Shell Pool
void initTankShellPool() {
    tankShellPool.clear();
    for (int i = 0; i < TANK_SHELL_POOL_SIZE; ++i) {
        TankShell ts;
        ts.mesh = new cShapeSphere(0.08); // Small fiery tank shell
        cColorf shellColor(1.0f, 0.45f, 0.0f, 1.0f); // Bright fiery orange
        safeSetColor(ts.mesh, shellColor);
        ts.mesh->setEnabled(false);
        world->addChild(ts.mesh);
        tankShellPool.push_back(ts);
    }
}

// Spawn a tank shell traveling towards a target tower
void spawnTankShell(const cVector3d& startPos, Tower* targetTower) {
    for (auto& ts : tankShellPool) {
        if (!ts.active) {
            ts.active = true;
            ts.position = startPos;
            ts.targetTower = targetTower;
            ts.targetPos = targetTower->position; // Save target position in case tower dies mid-flight
            ts.mesh->setEnabled(true);
            ts.mesh->setLocalPos(startPos);
            std::cout << "[TANK_SHELL_FIRE] Tank fired shell at tower! Start: " << startPos << ", Target: " << ts.targetPos << std::endl; std::cout.flush();
            return;
        }
    }
}

// =============================================================================
// WAYPOINT & TERRAIN INITIALIZATION
// =============================================================================
void initWaypoints() {
    waypoints.clear();
    // Path starts at (15.0, -8.0, 0.1) and snakes across the field to (-12.0, 8.0, 0.1)
    waypoints.push_back(cVector3d(15.0, -8.0, 0.1));
    waypoints.push_back(cVector3d(5.0, -8.0, 0.1));
    waypoints.push_back(cVector3d(5.0, 0.0, 0.1));
    waypoints.push_back(cVector3d(-5.0, 0.0, 0.1));
    waypoints.push_back(cVector3d(-5.0, 8.0, 0.1));
    waypoints.push_back(cVector3d(-15.0, 8.0, 0.1));
}

// Draw a path marker mesh for waypoints
void drawPath() {
    for (size_t i = 0; i < waypoints.size() - 1; ++i) {
        cMesh* segment = new cMesh();
        cCreateBox(segment, 0.4, 0.4, 0.05);
        cColorf roadColor(0.25f, 0.22f, 0.20f, 1.0f); // dusty path color
        safeSetColor(segment, roadColor);
        segment->setLocalPos(waypoints[i]);
        world->addChild(segment);

        // Fill path connection markers
        cVector3d start = waypoints[i];
        cVector3d end = waypoints[i+1];
        cVector3d dir = end - start;
        double dist = dir.length();
        dir.normalize();
        
        for (double d = 0.8; d < dist; d += 0.8) {
            cMesh* pathDot = new cMesh();
            cCreateBox(pathDot, 0.3, 0.3, 0.03);
            safeSetColor(pathDot, roadColor);
            pathDot->setLocalPos(start + dir * d);
            world->addChild(pathDot);
        }
    }
}

// =============================================================================
// OBJECT POOL & HELPER FUNCTIONS
// =============================================================================
void initProjectilePool() {
    projectilePool.clear();
    for (int i = 0; i < PROJECTILE_POOL_SIZE; ++i) {
        Projectile p;
        p.mesh = new cShapeSphere(0.08);
        cColorf projectileColor(1.0f, 0.8f, 0.0f, 1.0f); // Bright Gold
        safeSetColor(p.mesh, projectileColor);
        p.mesh->setEnabled(false); // Dormant initially
        world->addChild(p.mesh);
        projectilePool.push_back(p);
    }
}

Projectile* spawnProjectile(const cVector3d& startPos, Enemy* target, double dmg) {
    for (auto& p : projectilePool) {
        if (!p.active) {
            p.active = true;
            p.position = startPos;
            p.target = target;
            p.damage = dmg;
            p.mesh->setEnabled(true);
            p.mesh->setLocalPos(startPos);
            return &p;
        }
    }
    // Dynamic expansion of pool if exhausted
    Projectile p;
    p.mesh = new cShapeSphere(0.08);
    cColorf projectileColor(1.0f, 0.8f, 0.0f, 1.0f);
    safeSetColor(p.mesh, projectileColor);
    p.active = true;
    p.position = startPos;
    p.target = target;
    p.damage = dmg;
    p.mesh->setEnabled(true);
    world->addChild(p.mesh);
    projectilePool.push_back(p);
    return &projectilePool.back();
}

void spawnExplosion(const cVector3d& pos) {
    Explosion exp;
    exp.mesh = new cShapeSphere(exp.currentRadius);
    exp.mesh->setLocalPos(pos);
    
    // Transparent red-orange expanding ring
    cColorf fireColor(1.0f, 0.4f, 0.1f, 1.0f);
    safeSetColor(exp.mesh, fireColor);
    exp.mesh->setUseTransparency(true);
    
    world->addChild(exp.mesh);
    activeExplosions.push_back(exp);
}

void updateExplosions(double dt) {
    for (auto& exp : activeExplosions) {
        if (!exp.active) continue;
        exp.lifetime += dt;

        if (exp.lifetime >= exp.maxLifetime) {
            exp.active = false;
            world->removeChild(exp.mesh);
            delete exp.mesh;
        } else {
            double ratio = exp.lifetime / exp.maxLifetime;
            exp.currentRadius = exp.maxRadius * ratio;
            exp.mesh->setRadius(exp.currentRadius);

            // Fix: Access public member m_diffuse directly as cMaterial has no getColor()
            cColorf color = safeGetDiffuse(exp.mesh);
            color.setA(1.0f - ratio); // Fade alpha out
            safeSetDiffuse(exp.mesh, color);
        }
    }
    activeExplosions.erase(
        remove_if(activeExplosions.begin(), activeExplosions.end(), 
                  [](const Explosion& e) { return !e.active; }), 
        activeExplosions.end()
    );
}

// =============================================================================
// GAME PLAY SYSTEMS & LOGIC
// =============================================================================
void spawnEnemy() {
    std::cout << "[DEBUG_SPAWN] spawnEnemy() triggered." << std::endl; std::cout.flush();
    Enemy* e = new Enemy();
    e->isAir = (rand() % 3 == 0); // 33% air, 67% ground
    e->speed = (1.0 + (rand() % 100) / 200.0) * (1.0 + currentWave * 0.15);
    e->maxHealth = 80.0 + currentWave * 30.0;
    e->health = e->maxHealth;
    e->currentWaypoint = 0;
    e->position = waypoints[0];

    e->actionTimer = 1.0 + (rand() % 100) / 50.0; // Randomize initial attack/bomb timing
    std::cout << "[DEBUG_SPAWN] Initializing cMultiMesh..." << std::endl; std::cout.flush();
    e->mesh = new cMultiMesh();
    // SỬA LỖI CHÍ MẠNG: Khởi tạo material cho đối tượng cMultiMesh cha để tránh Null Pointer dereference khi gọi renderView()
    if (!e->mesh->m_material) {
        e->mesh->m_material = cMaterial::create();
    }
    
    bool success = false;
    if (e->isAir) {
        std::vector<std::string> paths = {
            "resources/models/plane_enemy.obj",
            "../resources/models/plane_enemy.obj",
            "../../resources/models/plane_enemy.obj",
            "models/plane_enemy.obj",
            "../models/plane_enemy.obj",
            "plane_enemy.obj",
            "../plane_enemy.obj"
        };
        for (const auto& path : paths) {
            std::cout << "[DEBUG_SPAWN] Attempting to load plane model from: " << path << std::endl; std::cout.flush();
            success = e->mesh->loadFromFile(path);
            if (success) {
                std::cout << "[DEBUG_SPAWN] Successfully loaded plane model from: " << path << std::endl; std::cout.flush();
                break;
            }
        }
        if (success) {
            cColorf planeColor(0.2f, 0.7f, 1.0f, 1.0f);
            safeSetColor(e->mesh, planeColor);
            
            // CRITICAL GL FIXES: Disable textures & display lists to prevent driver crashes
            e->mesh->setUseTexture(false, true);
            e->mesh->setUseDisplayList(false, true);
            
            std::cout << "[DEBUG_SPAWN] Plane meshes loaded: " << e->mesh->getNumMeshes() << std::endl; std::cout.flush();
            // Ensure all child meshes loaded from OBJ have valid materials to prevent Segfault in renderView()
            for (unsigned int i = 0; i < e->mesh->getNumMeshes(); ++i) {
                cMesh* child = e->mesh->getMesh(i);
                if (child) {
                    safeSetColor(child, planeColor);
                    child->setUseTexture(false, false);
                    child->setUseDisplayList(false, false);
                }
            }
        } else {
            std::cout << "[DEBUG_SPAWN] Failed to load plane model. Re-instantiating clean cMultiMesh and generating fallback cMesh Sphere..." << std::endl; std::cout.flush();
            delete e->mesh;
            e->mesh = new cMultiMesh();
            if (!e->mesh->m_material) {
                e->mesh->m_material = cMaterial::create();
            }
            cMesh* fallbackMesh = new cMesh();
            cCreateSphere(fallbackMesh, 0.4);
            cColorf planeColor(0.3f, 0.6f, 0.9f, 1.0f);
            safeSetColor(fallbackMesh, planeColor);
            e->mesh->addMesh(fallbackMesh);
        }
        e->position.z(2.2); // Airplane Z-height
    } else {
        std::vector<std::string> paths = {
            "resources/models/tank_enemy.obj",
            "../resources/models/tank_enemy.obj",
            "../../resources/models/tank_enemy.obj",
            "models/tank_enemy.obj",
            "../models/tank_enemy.obj",
            "tank_enemy.obj",
            "../tank_enemy.obj"
        };
        for (const auto& path : paths) {
            std::cout << "[DEBUG_SPAWN] Attempting to load tank model from: " << path << std::endl; std::cout.flush();
            success = e->mesh->loadFromFile(path);
            if (success) {
                std::cout << "[DEBUG_SPAWN] Successfully loaded tank model from: " << path << std::endl; std::cout.flush();
                break;
            }
        }
        if (success) {
            cColorf tankColor(0.9f, 0.3f, 0.2f, 1.0f);
            safeSetColor(e->mesh, tankColor);
            
            // CRITICAL GL FIXES: Disable textures & display lists to prevent driver crashes
            e->mesh->setUseTexture(false, true);
            e->mesh->setUseDisplayList(false, true);
            
            std::cout << "[DEBUG_SPAWN] Tank meshes loaded: " << e->mesh->getNumMeshes() << std::endl; std::cout.flush();
            // Ensure all child meshes loaded from OBJ have valid materials to prevent Segfault in renderView()
            for (unsigned int i = 0; i < e->mesh->getNumMeshes(); ++i) {
                cMesh* child = e->mesh->getMesh(i);
                if (child) {
                    safeSetColor(child, tankColor);
                    child->setUseTexture(false, false);
                    child->setUseDisplayList(false, false);
                }
            }
        } else {
            std::cout << "[DEBUG_SPAWN] Failed to load tank model. Re-instantiating clean cMultiMesh and generating fallback cMesh Box..." << std::endl; std::cout.flush();
            delete e->mesh;
            e->mesh = new cMultiMesh();
            if (!e->mesh->m_material) {
                e->mesh->m_material = cMaterial::create();
            }
            cMesh* fallbackMesh = new cMesh();
            cCreateBox(fallbackMesh, 0.7, 0.6, 0.4);
            cColorf tankColor(0.8f, 0.3f, 0.3f, 1.0f);
            safeSetColor(fallbackMesh, tankColor);
            e->mesh->addMesh(fallbackMesh);
        }
    }

    std::cout << "[DEBUG_SPAWN] Creating AABB collision detector..." << std::endl; std::cout.flush();
    e->mesh->createAABBCollisionDetector(0.01);
    
    std::cout << "[DEBUG_SPAWN] Setting local position..." << std::endl; std::cout.flush();
    e->mesh->setLocalPos(e->position);
    
    std::cout << "[DEBUG_SPAWN] Adding mesh to world..." << std::endl; std::cout.flush();
    if (world == nullptr) {
        std::cout << "[DEBUG_SPAWN] CRITICAL: world is NULL!" << std::endl; std::cout.flush();
    }
    world->addChild(e->mesh);

    std::cout << "[DEBUG_SPAWN] Locking mutex and pushing enemy to active list..." << std::endl; std::cout.flush();
    std::lock_guard<std::recursive_mutex> lock(game_mutex);
    activeEnemies.push_back(e);
    std::cout << "[DEBUG_SPAWN] spawnEnemy() completed successfully." << std::endl; std::cout.flush();
}

void triggerNextWave() {
    if (gameOver || gameWin) return;
    currentWave++;
    int waveSize = 4 + currentWave * 2;
    std::cout << "[GAME] Wave " << currentWave << " starting! Spawning " << waveSize << " enemies." << std::endl;
    
    std::lock_guard<std::recursive_mutex> lock(game_mutex);
    enemiesLeftToSpawn = waveSize;
    enemySpawnTimer = 0.0; // spawn first enemy immediately on next update

    // --- "TẠO RA MAP MỚI" (DYNAMIC TERRAIN THEME BY LEVEL/WAVE) ---
    if (world && terrain) {
        cColorf groundColor;
        cColorf skyColor;
        std::string themeName;

        if (currentWave <= 1) {
            // Theme 1: Grassland (Olive Green)
            groundColor.set(0.24f, 0.28f, 0.24f, 1.0f);
            skyColor.set(0.12f, 0.14f, 0.18f, 1.0f);
            themeName = "TACTICAL GRASSLAND";
        } else if (currentWave <= 3) {
            // Theme 2: Desert (Sandy Yellow)
            groundColor.set(0.65f, 0.55f, 0.35f, 1.0f);
            skyColor.set(0.22f, 0.18f, 0.14f, 1.0f);
            themeName = "SANDSTORM DESERT";
        } else if (currentWave <= 5) {
            // Theme 3: Snowy Tundra (Ice Blue/White)
            groundColor.set(0.70f, 0.80f, 0.85f, 1.0f);
            skyColor.set(0.08f, 0.10f, 0.15f, 1.0f);
            themeName = "FROZEN TUNDRA";
        } else {
            // Theme 4: Volcanic Area (Ashen/Dark Red)
            groundColor.set(0.15f, 0.05f, 0.05f, 1.0f);
            skyColor.set(0.05f, 0.01f, 0.01f, 1.0f);
            themeName = "VOLCANIC WASTELAND";
        }

        safeSetColor(terrain, groundColor);
        world->setBackgroundColor(skyColor);
        std::cout << "[MAP_GENERATION] Transitioned to new map theme: " << themeName << std::endl; std::cout.flush();
    }
}

// Create a tower at specified position
bool createTowerAt(const cVector3d& pos) {
    if (playerGold < TOWER_COST) return false;

    // Check if the coordinate is within terrain bounds and not too close to other towers
    if (abs(pos.x()) > 18.0 || abs(pos.y()) > 14.0) return false;

    for (const auto& t : activeTowers) {
        if (cDistance(t->position, pos) < TOWER_RADIUS * 2.5) {
            std::cout << "[PLACEMENT_FAIL] Area already occupied by another tower." << std::endl;
            return false;
        }
    }

    Tower* t = new Tower();
    t->position = pos;
    t->position.z(0.0); // Ground-aligned

    t->baseMesh = new cMesh();
    t->turretMesh = new cMesh();

    // Tower structural modeling using parent-child hierarchies
    cCreateCylinder(t->baseMesh, TOWER_HEIGHT, TOWER_RADIUS);
    cColorf baseColor(0.4f, 0.45f, 0.5f, 1.0f); // Slate Gray Tower Base
    safeSetColor(t->baseMesh, baseColor);
    t->baseMesh->setLocalPos(t->position);

    cCreateBox(t->turretMesh, 0.6, 0.3, 0.3);
    cColorf turretColor(0.8f, 0.7f, 0.1f, 1.0f); // Gold Gun Turret
    safeSetColor(t->turretMesh, turretColor);
    t->turretMesh->setLocalPos(0, 0, TOWER_HEIGHT); // Attach vertically atop cylinder base

    t->baseMesh->addChild(t->turretMesh);
    world->addChild(t->baseMesh);

    // Build hierarchical AABB trees for physics proxy detection
    t->baseMesh->createAABBCollisionDetector(0.01);

    std::lock_guard<std::recursive_mutex> lock(game_mutex);
    activeTowers.push_back(t);
    playerGold -= TOWER_COST;
    playerScore += 10;
    std::cout << "[PLACEMENT_SUCCESS] Built Defensive Tower at " << pos << ". Gold remaining: " << playerGold << std::endl;

    return true;
}

// =============================================================================
// GRAPHICS RENDER LOOP & LOGIC (Approx 60Hz)
// =============================================================================
void updateGame(double dt) {
    if (gameOver || gameWin) return;

    std::lock_guard<std::recursive_mutex> lock(game_mutex);

    // Handle wave auto-transition timer
    if (waveInTransition) {
        waveTransitionTimer -= dt;
        if (waveTransitionTimer <= 0.0) {
            waveInTransition = false;
            triggerNextWave();
        }
    }

    // 0. Handle staggered enemy spawning safely on the main thread (thread-safe with OpenGL)
    if (!waveInTransition && enemiesLeftToSpawn > 0) {
        enemySpawnTimer -= dt;
        if (enemySpawnTimer <= 0.0) {
            spawnEnemy();
            enemiesLeftToSpawn--;
            enemySpawnTimer = ENEMY_SPAWN_COOLDOWN;
        }
    }

    // 1. Process asynchronous tower placement requests from the haptic thread
    if (placeTowerRequested) {
        placeTowerRequested = false;
        createTowerAt(towerPlacementLocation);
    }

    // 2. Update Enemy Pathfinding & Movements (Waypoint Navigation)
    for (auto& e : activeEnemies) {
        if (!e->active) continue;

        cVector3d targetDest = waypoints[e->currentWaypoint];
        if (e->isAir) targetDest.z(2.2); // Maintain flight altitude

        cVector3d toWaypoint = targetDest - e->position;
        double dist = toWaypoint.length();

        if (dist < 0.25) {
            e->currentWaypoint++;
            if (e->currentWaypoint >= (int)waypoints.size()) {
                // Enemy breached defenses!
                e->active = false;
                world->removeChild(e->mesh);
                playerLives--;
                std::cout << "[BREACH] Enemy escaped! Lives left: " << playerLives << std::endl;
                
                if (playerLives <= 0) {
                    gameOver = true;
                    std::cout << "======================================================" << std::endl;
                    std::cout << "         GAME OVER! YOUR VILLAGE HAS FALLEN!          " << std::endl;
                    std::cout << "======================================================" << std::endl;
                }
                continue;
            }
        }

        toWaypoint.normalize();
        e->position += toWaypoint * e->speed * dt;
        e->mesh->setLocalPos(e->position);

        // --- NEW COMBAT LOGIC: Tanks attack towers & Planes drop bombs ---
        e->actionTimer -= dt;
        if (e->actionTimer <= 0.0) {
            if (e->isAir) {
                // Plane drops a bomb straight down
                spawnBomb(e->position);
                e->actionTimer = 3.0 + (rand() % 200) / 100.0; // Cooldown for next bomb (3-5 seconds)
            } else {
                // Tank looks for nearest tower within 4.0 meters to fire at
                Tower* targetTower = nullptr;
                double nearestDist = 4.0;
                for (auto* t : activeTowers) {
                    double d = cDistance(e->position, t->position);
                    if (d < nearestDist) {
                        nearestDist = d;
                        targetTower = t;
                    }
                }
                if (targetTower) {
                    // Attack! Spawn a tank shell traveling towards the tower instead of instant-damage!
                    cVector3d nozzlePos = e->position;
                    nozzlePos.z(nozzlePos.z() + 0.3); // Raise slightly to align with tank height
                    spawnTankShell(nozzlePos, targetTower);
                    e->actionTimer = 1.5 + (rand() % 100) / 100.0; // Attack cooldown (1.5 - 2.5 seconds)
                } else {
                    e->actionTimer = 0.5; // No towers nearby, scan again soon
                }
            }
        }

        // Align mesh orientation to face current heading direction (Fix: C_RAD_TO_DEG compatibility)
        cMatrix3d headingRot;
        double yaw = atan2(toWaypoint.y(), toWaypoint.x()) * C_RAD_TO_DEG;
        if (!e->isAir) {
            yaw += 180.0; // Rotate tank 180 degrees so it faces forward instead of backward!
        }
        headingRot.setExtrinsicEulerRotationDeg(0, 0, yaw, C_EULER_ORDER_XYZ);
        e->mesh->setLocalRot(headingRot);
    }

    // 3. Update Tower Cooldowns, Automated Targeting & Shooting
    for (auto& t : activeTowers) {
        if (t->cooldownTimer > 0.0) {
            t->cooldownTimer -= dt;
        }

        // Search for nearest active enemy in range
        Enemy* nearestTarget = nullptr;
        double nearestDist = t->range;

        for (auto& e : activeEnemies) {
            if (!e->active) continue;
            double dist = cDistance(t->position, e->position);
            if (dist < nearestDist) {
                nearestDist = dist;
                nearestTarget = e;
            }
        }

        if (nearestTarget) {
            // Track & rotate turret heading towards the enemy
            cVector3d toEnemy = nearestTarget->position - t->position;
            double angleRad = atan2(toEnemy.y(), toEnemy.x());
            cMatrix3d rotTurret;
            rotTurret.setExtrinsicEulerRotationDeg(0, 0, angleRad * C_RAD_TO_DEG, C_EULER_ORDER_XYZ);
            t->turretMesh->setLocalRot(rotTurret);

            // Execute automated shooting
            if (t->cooldownTimer <= 0.0) {
                cVector3d nozzlePos = t->position + cVector3d(0, 0, TOWER_HEIGHT) + cVector3d(cos(angleRad)*0.4, sin(angleRad)*0.4, 0);
                spawnProjectile(nozzlePos, nearestTarget, t->damage);
                t->cooldownTimer = 1.0 / t->fireRate;
            }
        }
    }

    // --- UPDATE ACTIVE BOMBS ---
    for (auto& b : bombPool) {
        if (!b.active) continue;

        // Falling physics
        b.position.z(b.position.z() - b.speed * dt);
        b.mesh->setLocalPos(b.position);

        // Check ground impact (Z <= 0.0)
        if (b.position.z() <= 0.0) {
            b.active = false;
            b.mesh->setEnabled(false);
            spawnExplosion(b.position); // Explode on impact!

            // Deal AoE damage to all towers within 3.0 meters
            for (auto* t : activeTowers) {
                double dist = cDistance(b.position, t->position);
                if (dist < 3.0) {
                    t->health -= 35.0; // Bomb AoE damage
                    spawnExplosion(t->position); // Small explosion on tower to show impact
                    std::cout << "[BOMB_IMPACT] Tower at " << t->position << " damaged by plane bomb! Tower Health: " << t->health << std::endl; std::cout.flush();
                }
            }
        }
    }

    // --- UPDATE ACTIVE TANK SHELLS ---
    for (auto& ts : tankShellPool) {
        if (!ts.active) continue;

        // Move towards target position (aiming for the center of the tower height)
        cVector3d targetAimPos = ts.targetPos;
        targetAimPos.z(targetAimPos.z() + TOWER_HEIGHT * 0.5);

        cVector3d dir = targetAimPos - ts.position;
        double dist = dir.length();

        if (dist < 0.3) {
            // Impact!
            ts.active = false;
            ts.mesh->setEnabled(false);
            spawnExplosion(ts.position); // Explosion on impact

            // Deal damage only if tower is still active (alive)
            bool towerStillAlive = false;
            for (auto* t : activeTowers) {
                if (t == ts.targetTower) {
                    t->health -= ts.damage;
                    towerStillAlive = true;
                    std::cout << "[TANK_SHELL_IMPACT] Tank shell hit Tower! Damage: " << ts.damage << ", Tower Health: " << t->health << std::endl; std::cout.flush();
                    break;
                }
            }
            if (!towerStillAlive) {
                std::cout << "[TANK_SHELL_IMPACT] Tank shell hit empty ground (Tower already destroyed)." << std::endl; std::cout.flush();
            }
            continue;
        }

        dir.normalize();
        ts.position += dir * ts.speed * dt;
        ts.mesh->setLocalPos(ts.position);
    }

    // --- CHECK FOR DESTROYED TOWERS ---
    for (auto it = activeTowers.begin(); it != activeTowers.end(); ) {
        if ((*it)->health <= 0.0) {
            std::cout << "[TOWER_DESTROYED] Tower at " << (*it)->position << " destroyed by enemy forces!" << std::endl; std::cout.flush();
            spawnExplosion((*it)->position);
            
            // Unparent and clean memory of tower meshes from the world
            world->removeChild((*it)->baseMesh);
            delete (*it)->baseMesh; // Deletes baseMesh and child turretMesh recursively
            delete (*it);
            it = activeTowers.erase(it);
        } else {
            ++it;
        }
    }

    // 4. Update Projectile Ballistics & Intersections
    for (auto& p : projectilePool) {
        if (!p.active) continue;

        // Redirect/adjust projectile trajectory towards its designated moving target
        if (p.target && p.target->active) {
            cVector3d dir = p.target->position - p.position;
            if (p.target->isAir) dir.z(dir.z() + 2.2); // Aim for airplane height

            double dist = dir.length();
            if (dist < 0.3) {
                // Impact! Apply damage
                p.target->health -= p.damage;
                spawnExplosion(p.position);
                p.active = false;
                p.mesh->setEnabled(false);

                if (p.target->health <= 0.0) {
                    p.target->active = false;
                    world->removeChild(p.target->mesh);
                    playerGold += 20; // Reward gold
                    playerScore += 50;
                    std::cout << "[KILLED] Destroyed Enemy! +20 Gold, Total: " << playerGold << std::endl;
                }
                continue;
            }
            dir.normalize();
            p.position += dir * p.speed * dt;
            p.mesh->setLocalPos(p.position);
        } else {
            // Target is dead or missing, dissipate projectile
            p.active = false;
            p.mesh->setEnabled(false);
        }
    }

    // 5. Cleanup destroyed memory allocations
    for (auto it = activeEnemies.begin(); it != activeEnemies.end(); ) {
        if (!(*it)->active) {
            delete (*it)->mesh;
            delete (*it);
            it = activeEnemies.erase(it);
        } else {
            ++it;
        }
    }

    // --- AUTO WAVE TRANSITION LOGIC ("TIÊU DIỆT ĐỦ SỐ ENEMY THÌ WIN/CHUYỂN MAP") ---
    if (!waveInTransition && enemiesLeftToSpawn == 0 && activeEnemies.empty() && playerLives > 0) {
        waveInTransition = true;
        waveTransitionTimer = WAVE_TRANSITION_DELAY;
        playerGold += 100; // 100 Gold completion bonus!
        playerScore += 100; // 100 Score bonus for leveling up!

        // Clear active projectiles, bombs and tank shells to start clean in the new map
        for (auto& p : projectilePool) {
            p.active = false;
            if (p.mesh) p.mesh->setEnabled(false);
        }
        for (auto& b : bombPool) {
            b.active = false;
            if (b.mesh) b.mesh->setEnabled(false);
        }
        for (auto& ts : tankShellPool) {
            ts.active = false;
            if (ts.mesh) ts.mesh->setEnabled(false);
        }
        std::cout << "[WAVE_CLEAR] Wave " << currentWave << " cleared! Level completed successfully. +100 Gold bonus. Loading new map in 5 seconds..." << std::endl; std::cout.flush();
    }

    // 6. Check Victory Criteria (Keep endless but survival celebration)
    if (currentWave >= 6 && !gameWin) {
        gameWin = true;
        std::cout << "======================================================" << std::endl;
        std::cout << "     SUPREME VICTORY! YOU DEFENDED ALL 5 MAP THEMES!  " << std::endl;
        std::cout << "======================================================" << std::endl;
    }

    // 7. Process particle expansions
    updateExplosions(dt);
}

// Update HUD texts smoothly
void updateHUD() {
    if (statusLabel && statsLabel) {
        cColorf green(0.2f, 0.9f, 0.2f, 1.0f);
        cColorf red(0.9f, 0.2f, 0.2f, 1.0f);
        cColorf white(1.0f, 1.0f, 1.0f, 1.0f);

                if (gameOver) {
            statusLabel->setText("VILLAGE DESTROYED - GAME OVER!");
            // Fix: Direct assignment since cColorf has no setColor()
            statusLabel->m_fontColor = red;
        } else if (gameWin) {
            statusLabel->setText("SUPREME VICTORY! ALL MAPS SURVIVED!");
            statusLabel->m_fontColor = green;
        } else if (waveInTransition) {
            char transitionBuffer[128];
            sprintf(transitionBuffer, "WAVE %d CLEARED! LOADING NEW MAP IN %.1fs...", currentWave, waveTransitionTimer);
            statusLabel->setText(transitionBuffer);
            statusLabel->m_fontColor = green;
        } else {
            statusLabel->setText("TOWER DEFENSE COMBAT RUNNING");
            statusLabel->m_fontColor = white;
        }

        char statsBuffer[256];
        sprintf(statsBuffer, "Lives: %d | Gold: %d / %d | Score: %d / 500 | Wave: %d", 
                playerLives, playerGold, TOWER_COST, playerScore, currentWave);
        statsLabel->setText(statsBuffer);

        // Position Labels to fit viewport resize
        statusLabel->setLocalPos(20, windowHeight - 40, 0);
        statsLabel->setLocalPos(20, windowHeight - 70, 0);
        instructionsLabel->setLocalPos(20, 20, 0);
    }
}

// =============================================================================
// SUB-MILLISECOND HAPTIC UPDATE LOOP (Strict 1000Hz Thread)
// =============================================================================
void updateHaptics() {
    hapticsLoopRunning = true;
    cPrecisionClock clock;
    clock.start();

    // Haptic interaction variables
    cVector3d localCursorPos(0, 0, 1.0);
    cVector3d localCursorVel(0, 0, 0);
    bool buttonWasPressed = false;

    std::cout << "[HAPTICS] High frequency 1000Hz force loop thread started." << std::endl;

    while (hapticsLoopRunning) {
        double dt = clock.stop();
        clock.start();

        // Cap timestep to avoid numerical instabilities on system lag
        if (dt > 0.005) dt = 0.005;

        // Read physical device data
        cVector3d devicePos(0, 0, 0);
        cVector3d deviceVel(0, 0, 0);
        bool buttonPressedNow = false;

        if (hapticDeviceReady) {
            hapticDevice->getPosition(devicePos);
            hapticDevice->getLinearVelocity(deviceVel);
            
            unsigned int buttonStatus = 0;
            hapticDevice->getUserSwitches(buttonStatus);
            buttonPressedNow = (buttonStatus & 1) != 0; // Read first physical button
        }

                // Scale physical workspace motions into virtual coordinate boundaries if haptic device is active
        if (hapticDeviceReady) {
            localCursorPos = devicePos * 6.5; // Workspace scaling multiplier
            localCursorVel = deviceVel * 6.5;

            // Thread-safe update of current cursor coordinates
            std::lock_guard<std::recursive_mutex> lock(game_mutex);
            sharedCursorPos = localCursorPos;
            sharedCursorVel = localCursorVel;
        }

        // Calculate haptic forces (F = Force spring-damper feedback)
        cVector3d hapticForce(0.0, 0.0, 0.0);

        // 1. Solid Ground Simulation (Hooke's Law Spring-Damper constraint)
        double groundZ = 0.0;
        if (localCursorPos.z() < (groundZ + CURSOR_RADIUS)) {
            double penetrationDepth = (groundZ + CURSOR_RADIUS) - localCursorPos.z();
            
            // F_z = k * x - b * v (Stiffness Spring force + viscous damping)
            double fSpring = GROUND_STIFFNESS * penetrationDepth;
            double fDamping = -GROUND_DAMPING * localCursorVel.z();
            
            hapticForce.z(fSpring + fDamping);
        }

        // 2. Solid Tower Cylindrical Obstacles Simulation (feel the towers)
        {
            std::lock_guard<std::recursive_mutex> lock(game_mutex);
            for (const auto& t : activeTowers) {
                cVector3d towerToCursor = localCursorPos - t->position;
                towerToCursor.z(0.0); // Project onto 2D plane
                double dist2D = towerToCursor.length();

                if (dist2D < (TOWER_RADIUS + CURSOR_RADIUS) && localCursorPos.z() < TOWER_HEIGHT) {
                    double overlap = (TOWER_RADIUS + CURSOR_RADIUS) - dist2D;
                    towerToCursor.normalize();

                    // Spring repulsion in radial direction
                    cVector3d radialForce = towerToCursor * TOWER_STIFFNESS * overlap;
                    // Apply viscous damping along collision axis
                    double radialVel = localCursorVel.dot(towerToCursor);
                    cVector3d radialDamping = towerToCursor * (-GROUND_DAMPING * radialVel);

                    hapticForce += (radialForce + radialDamping);
                }
            }
        }

        // 3. Process Haptic Button Press to Trigger Tower Placement
        if (buttonPressedNow && !buttonWasPressed) {
            std::lock_guard<std::recursive_mutex> lock(game_mutex);
            if (playerGold >= TOWER_COST && !gameOver && !gameWin) {
                placeTowerRequested = true;
                towerPlacementLocation = localCursorPos;
                towerPlacementLocation.z(0.0); // Project onto ground
            }
        }
        buttonWasPressed = buttonPressedNow;

        // Apply safely clamped forces back to the haptic hardware device
        if (hapticDeviceReady) {
            // Protect device motors from overloading
            double maxForce = 6.5; // Newtons limit
            if (hapticForce.length() > maxForce) {
                hapticForce.normalize();
                hapticForce *= maxForce;
            }
            hapticDevice->setForce(hapticForce);
        }
        
        // SỬA LỖI CPU WATCHDOG: Ngủ 1ms mỗi chu kỳ để tránh quá tải nhân CPU gây lỗi sập 'Killed'!
        cSleepMs(1);
    }

    // Stop haptic forces before exiting thread
    if (hapticDeviceReady) {
        hapticDevice->setForce(cVector3d(0, 0, 0));
    }
    std::cout << "[HAPTICS] Haptics loop thread exited cleanly." << std::endl;
}

// =============================================================================
// WINDOW KEY CALLBACK
// =============================================================================
void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    if (hapticDeviceReady) return; // Prevent overwriting if hardware is active

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    if (width == 0 || height == 0) return;

    // Convert mouse pixels to [-1.0, 1.0] normalized device coordinates (NDC)
    double normX = (xpos / width) * 2.0 - 1.0;
    double normY = 1.0 - (ypos / height) * 2.0; // standard OpenGL: bottom=-1, top=1

    // Camera parameters from initGame()
    cVector3d C(18.0, 0.0, 15.0); // Camera Position
    cVector3d T(0.0, 0.0, 0.0);   // Target / Look-at point
    cVector3d U(0.0, 0.0, 1.0);   // Up vector

    // Derive camera coordinate axes
    cVector3d F = T - C;
    F.normalize(); // Forward vector

    cVector3d R = cCross(F, U);
    R.normalize(); // Right vector

    cVector3d U_cam = cCross(R, F);
    U_cam.normalize(); // Up vector in camera space

    // Compute aspect ratio and field of view half-angle (standard CHAI3D is 45.0 degrees)
    double aspect = (double)width / (double)height;
    double fovH = tan(45.0 * M_PI / 360.0); // tan(22.5 degrees)

    // Direction of the ray in camera coordinates
    cVector3d ray_cam(normX * fovH * aspect, normY * fovH, -1.0);

    // Convert ray to world space
    cVector3d ray_world = R * ray_cam.x() + U_cam * ray_cam.y() - F * ray_cam.z();
    ray_world.normalize();

    std::lock_guard<std::recursive_mutex> lock(game_mutex);
    
    // Find intersection of ray P(t) = C + t*ray_world with ground plane Z = 0
    if (abs(ray_world.z()) > 0.0001) {
        double t = -C.z() / ray_world.z();
        if (t > 0.0) {
            cVector3d intersection = C + ray_world * t;
            
            // Clamp coordinates to terrain boundaries (36.0 x 28.0)
            if (intersection.x() < -18.0) intersection.x(-18.0);
            if (intersection.x() > 18.0) intersection.x(18.0);
            if (intersection.y() < -14.0) intersection.y(-14.0);
            if (intersection.y() > 14.0) intersection.y(14.0);
            
            sharedCursorPos = intersection;
            sharedCursorPos.z(0.15); // Hover cursor slightly above terrain
        }
    }
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        std::lock_guard<std::recursive_mutex> lock(game_mutex);
        if (playerGold >= TOWER_COST && !gameOver && !gameWin) {
            placeTowerRequested = true;
            towerPlacementLocation = sharedCursorPos;
            towerPlacementLocation.z(0.0); // Align to ground
            std::cout << "[MOUSE] Click registered! Requesting tower placement at " << sharedCursorPos << std::endl; std::cout.flush();
        }
    }
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

    if (key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    if (key == GLFW_KEY_N) {
        triggerNextWave();
    }

    // Keyboard support for moving haptic cursor and placing towers without haptic device
    if (!hapticDeviceReady) {
        double step = 0.5;
        std::lock_guard<std::recursive_mutex> lock(game_mutex);
        if (key == GLFW_KEY_LEFT || key == GLFW_KEY_A) {
            sharedCursorPos.y(sharedCursorPos.y() + step);
        }
        if (key == GLFW_KEY_RIGHT || key == GLFW_KEY_D) {
            sharedCursorPos.y(sharedCursorPos.y() - step);
        }
        if (key == GLFW_KEY_UP || key == GLFW_KEY_W) {
            sharedCursorPos.x(sharedCursorPos.x() - step);
        }
        if (key == GLFW_KEY_DOWN || key == GLFW_KEY_S) {
            sharedCursorPos.x(sharedCursorPos.x() + step);
        }
        if (key == GLFW_KEY_SPACE) {
            if (playerGold >= TOWER_COST && !gameOver && !gameWin) {
                placeTowerRequested = true;
                towerPlacementLocation = sharedCursorPos;
                towerPlacementLocation.z(0.0);
                std::cout << "[KEYBOARD] Space pressed! Requesting tower placement at " << sharedCursorPos << std::endl; std::cout.flush();
            }
        }
    }
}

// =============================================================================
// COMPILATION FIXES & SCENE CONFIGURATION
// =============================================================================
void initGame() {
    srand(static_cast<unsigned int>(time(NULL)));

    // Initialize core World
    world = new cWorld();
    world->setBackgroundColor(cColorf(0.12f, 0.14f, 0.18f)); // Dark tactical backdrop

    // Camera setup
    camera = new cCamera(world);
    world->addChild(camera);
    camera->set(cVector3d(18.0, 0.0, 15.0),  // Positioned diagonally overlooking the field
                cVector3d(0.0, 0.0, 0.0),    // Pointing directly at grid center
                cVector3d(0.0, 0.0, 1.0));   // Z-Axis is Up
    camera->setClippingPlanes(0.1, 80.0);

    // Directional Lighting
    light = new cDirectionalLight(world);
    world->addChild(light);
    light->setEnabled(true);
    light->setDir(cVector3d(-0.6, -0.4, -0.8)); // Diagonal lighting
    
    // Render Terrain Surface
    terrain = new cMesh();
    world->addChild(terrain);
    cCreatePlane(terrain, 36.0, 28.0);
    terrain->setLocalPos(0, 0, 0);
    cColorf sandColor(0.24f, 0.28f, 0.24f, 1.0f); // tactical olive green sand
    safeSetColor(terrain, sandColor);

    // Path & waypoints
    initWaypoints();
    drawPath();

    // Initial project pool setup
    initProjectilePool();
    
    // Initialize Bomb Pool
    initBombPool();
    
    // Initialize Tank Shell Pool
    initTankShellPool();

    // Spawn virtual 3D haptic cursor orb
    hapticCursor = new cShapeSphere(CURSOR_RADIUS);
    cColorf cursorColor(0.0f, 0.9f, 0.9f, 0.8f); // Cyan semi-transparent
    safeSetColor(hapticCursor, cursorColor);
    hapticCursor->setUseTransparency(true);
    world->addChild(hapticCursor);

    // HUD Design Setup (Fix: Create font pointer using standard static constructor cFont::create())
    font = cFont::create();
    
    // Dynamic Font Loader: Scan multiple relative paths for the user's actual fonts
    bool fontLoaded = false;
    std::vector<std::string> fontPaths = {
        "resources/fonts/calibri-24.fnt",
        "../resources/fonts/calibri-24.fnt",
        "../../resources/fonts/calibri-24.fnt",
        "resources/fonts/consolas-24.fnt",
        "../resources/fonts/consolas-24.fnt",
        "../../resources/fonts/consolas-24.fnt",
        "calibri-24.fnt",
        "consolas-24.fnt"
    };

    for (const auto& path : fontPaths) {
        if (font->loadFromFile(path)) {
            // Verify that the companion texture image was also successfully loaded into graphics memory
            if (font->m_texture && font->m_texture->m_image && font->m_texture->m_image->getWidth() > 0) {
                fontLoaded = true;
                std::cout << "[UI] Successfully loaded font and companion texture from: " << path << std::endl; std::cout.flush();
                break;
            } else {
                std::cout << "[UI_WARNING] Font file found at " << path << " but companion PNG texture is missing, corrupt, or CHAI3D has no PNG support!" << std::endl; std::cout.flush();
            }
        }
    }

    if (!fontLoaded) {
        std::cout << "[FONT_WARNING] Failed to load any font files from resources! HUD labels will be disabled to prevent crash." << std::endl; std::cout.flush();
    }

    // Only create labels if a font is successfully loaded to avoid Segmentation Fault during rendering
    if (fontLoaded) {
        statusLabel = new cLabel(font);
        statusLabel->setFontScale(1.1);
        camera->m_frontLayer->addChild(statusLabel);

        statsLabel = new cLabel(font);
        statsLabel->setFontScale(0.9);
        camera->m_frontLayer->addChild(statsLabel);

        instructionsLabel = new cLabel(font);
        instructionsLabel->setFontScale(0.75);
        cColorf instrColor(0.8f, 0.8f, 0.8f, 1.0f);
        instructionsLabel->m_fontColor = instrColor;
        instructionsLabel->setText("Haptic Controls: Feel terrain, press Device Button to buy/place towers.\nKeyboard: [N] - Launch Next Wave | [Esc] - Quit");
        camera->m_frontLayer->addChild(instructionsLabel);
    } else {
        statusLabel = nullptr;
        statsLabel = nullptr;
        instructionsLabel = nullptr;
    }

    // Spawning 1st wave staggered enemies automatically
    triggerNextWave();
}

// =============================================================================
// MAIN ENTRY POINT
// =============================================================================
int main(int argc, char** argv) {
    // Register SIGSEGV signal handler to catch and diagnose Segmentation Faults
    signal(SIGSEGV, segsegv_handler);

    // 1. Initialize GLFW Rendering Engine
    if (!glfwInit()) {
        std::cerr << "CRITICAL ERROR: Failed to initialize GLFW." << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "Tactical Haptic Tower Defense - CHAI3D", NULL, NULL);
    
    if (!window) {
        std::cerr << "CRITICAL ERROR: Failed to create GLFW window." << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    // Initialize GLEW (OpenGL Extension Wrangler)
    // Bind OpenGL driver extensions via GLEW (Texture, ...)
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "CRITICAL ERROR: Failed to initialize GLEW library." << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSwapInterval(1); // Enable V-Sync

    // 2. Initialize Game Objects & Scene
    initGame();

    // 3. Initialize Haptic Hardware Handler & Device
    handler = new cHapticDeviceHandler();
    if (handler->getDevice(hapticDevice, 0)) {
        if (hapticDevice->open()) {
            std::cout << "[HAPTICS] Connected to hardware device successfully: " 
                      << hapticDevice->getSpecifications().m_modelName << std::endl;
            hapticDeviceReady = true; // Set flag to true!
        } else {
            std::cout << "[HAPTICS_WARNING] Failed to open haptic device. Running in mouse/procedural fallback mode." << std::endl;
        }
    } else {
        std::cout << "[HAPTICS_WARNING] No haptic hardware detected. Game running in visual-only mode." << std::endl;
    }

    // 4. Start asynchronous 1000Hz haptic thread
    std::thread hapticThread(updateHaptics);

    double lastTime = glfwGetTime();

    // 5. Main Graphics Render Loop
    while (!glfwWindowShouldClose(window)) {
        double currentTime = glfwGetTime();
        double deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        if (deltaTime > 0.1) deltaTime = 0.1;

        // Process game mechanics and collision updates
        std::cout << "[DEBUG_LOOP] Calling updateGame()..." << std::endl; std::cout.flush();
        updateGame(deltaTime);
        std::cout << "[DEBUG_LOOP] Finished updateGame()." << std::endl; std::cout.flush();

        // Update virtual 3D cursor mesh position matching shared haptic tool coordinates
        {
            std::cout << "[DEBUG_LOOP] Locking mutex and updating cursor pos..." << std::endl; std::cout.flush();
            std::lock_guard<std::recursive_mutex> lock(game_mutex);
            hapticCursor->setLocalPos(sharedCursorPos);
            std::cout << "[DEBUG_LOOP] Cursor pos updated." << std::endl; std::cout.flush();
        }

        // Draw and update HUD HUD Overlay texts
        std::cout << "[DEBUG_LOOP] Updating HUD..." << std::endl; std::cout.flush();
        glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
        updateHUD();

        // Setup rendering canvas
        std::cout << "[DEBUG_LOOP] Rendering view..." << std::endl; std::cout.flush();
        glViewport(0, 0, windowWidth, windowHeight);
        camera->renderView(windowWidth, windowHeight);
        std::cout << "[DEBUG_LOOP] Rendering complete." << std::endl; std::cout.flush();

        // Swap draw buffers & poll keyboard inputs
        std::cout << "[DEBUG_LOOP] Swapping buffers..." << std::endl; std::cout.flush();
        glfwSwapBuffers(window);
        std::cout << "[DEBUG_LOOP] Buffers swapped." << std::endl; std::cout.flush();
        glfwPollEvents();
    }

    // 6. Program Shutdown & Safe thread closure
    std::cout << "[SHUTDOWN] Terminating haptics thread..." << std::endl;
    hapticsLoopRunning = false;
    if (hapticThread.joinable()) {
        hapticThread.join();
    }

    std::cout << "[SHUTDOWN] Releasing graphics and hardware handlers..." << std::endl;
    delete handler;
    delete world;
    
    glfwTerminate();
    std::cout << "[SHUTDOWN] Clean exit." << std::endl;
    return 0;
}

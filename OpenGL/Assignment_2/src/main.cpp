#include "chai3d.h"
#include <GLFW/glfw3.h>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <ctime>

using namespace chai3d;
using namespace std;

// =============================================================================
// GLOBAL CONFIGURATIONS & GAME STATE
// =============================================================================
int score = 0;
int escapedEnemies = 0;
const int WIN_SCORE = 50;
const int MAX_ESCAPED = 20;
bool gameOver = false;
bool gameWin = false;

// Screen dimensions
int windowWidth = 1024;
int windowHeight = 768;

// CHAI3D core elements
cWorld* world;
cCamera* camera;
cDirectionalLight* light;
cMesh* ground;

// =============================================================================
// UI (HUD) ELEMENTS
// =============================================================================
cFontPtr font;
cLabel* scoreLabel = nullptr;
cLabel* escapeLabel = nullptr;
cLabel* stateLabel = nullptr;
int lastScore = -1;
int lastEscaped = -1;

// =============================================================================
// AUDIO (SPATIALIZED 3D SOUND) ELEMENTS
// =============================================================================
cAudioDevice* audioDevice = nullptr;
cAudioBuffer* explosionBuffer = nullptr;
cAudioSource* explosionSource = nullptr;

// Cannon components
cMultiMesh* cannonBase = nullptr;
cMultiMesh* cannonBarrel = nullptr;
double barrelPitch = 20.0; // Elevation angle (Up/Down) in degrees
double barrelYaw = 0.0;    // Rotation angle (Left/Right) in degrees

// Difficulty configurations
double spawnInterval = 3.0; // Seconds between spawns
double lastSpawnTime = 0.0;
double speedBonus = 0.0;

// =============================================================================
// GAME ENTITIES DEFINITIONS
// =============================================================================

// Bullet Structure (utilizing Object Pooling)
struct Bullet {
    cShapeSphere* mesh = nullptr;
    cVector3d position;
    cVector3d velocity;
    bool active = false;
};

// Enemy Structure (utilizing Mesh loading and AABB Tree)
struct Enemy {
    cMultiMesh* mesh = nullptr;
    cVector3d position;
    double speed;
    bool active = true;
    bool isAir = false; // Tank vs Airplane
};

// Explosion Effect Structure (no const members so it is assignable in std::vector)
struct Explosion {
    cShapeSphere* mesh = nullptr;
    double lifetime = 0.0;
    double maxLifetime = 0.4; // 0.4 seconds duration
    double currentRadius = 0.1;
    double maxRadius = 1.0;   // Expansion limit
    bool active = true;
};

// Entity Pools & Active Lists
const int BULLET_POOL_SIZE = 50;
vector<Bullet> bulletPool;
vector<Enemy> activeEnemies;
vector<Explosion> activeExplosions;

// =============================================================================
// OBJECT POOL & UTILITY FUNCTIONS
// =============================================================================

// Initialize Bullet Pool at startup to avoid runtime heap allocation overheads
void initBulletPool() {
    bulletPool.clear();
    for (int i = 0; i < BULLET_POOL_SIZE; ++i) {
        Bullet b;
        b.mesh = new cShapeSphere(0.08);
        b.mesh->m_material->setYellowGold();
        b.mesh->setEnabled(false); // Hide and disable initially (Fix: b->mesh to b.mesh)
        world->addChild(b.mesh);
        bulletPool.push_back(b);
    }
}

// Request an available bullet from the pool (O(1) average lookup)
Bullet* getAvailableBullet() {
    for (auto& b : bulletPool) {
        if (!b.active) {
            b.active = true;
            b.mesh->setEnabled(true);
            return &b;
        }
    }
    // Fallback: Expand pool dynamically if completely exhausted
    Bullet b;
    b.mesh = new cShapeSphere(0.08);
    b.mesh->m_material->setYellowGold();
    b.mesh->setEnabled(true);
    b.active = true;
    world->addChild(b.mesh);
    bulletPool.push_back(b);
    std::cout << "[POOL_EXPANSION] Bullet pool exhausted. Added 1 more bullet mesh." << std::endl;
    return &bulletPool.back();
}

// Return bullet to pool (Deactivate and hide mesh)
void recycleBullet(Bullet& b) {
    b.active = false;
    b.mesh->setEnabled(false);
}

// Spawn an explosion effect using expanding transparent sphere
void spawnExplosion(const cVector3d& pos) {
    Explosion exp;
    exp.mesh = new cShapeSphere(exp.currentRadius);
    exp.mesh->setLocalPos(pos);
    
    // Glowing fire color with transparency enabled (Fix: setColor takes lvalue ref)
    cColorf orangeRed(1.0f, 0.35f, 0.05f, 1.0f);
    exp.mesh->m_material->setColor(orangeRed);
    exp.mesh->setUseTransparency(true); // Fix: setUseTransparency belongs to cGenericObject
    
    // Play explosion sound at the specified 3D position
    if (explosionSource) {
        explosionSource->setSourcePos(pos);
        explosionSource->play();
    }
    
    world->addChild(exp.mesh);
    activeExplosions.push_back(exp);
}

// Update all active explosions (expansion and alpha fading)
void updateExplosions(double deltaTime) {
    for (auto& exp : activeExplosions) {
        if (!exp.active) continue;
        exp.lifetime += deltaTime;

        if (exp.lifetime >= exp.maxLifetime) {
            exp.active = false;
            world->removeChild(exp.mesh);
            delete exp.mesh;
        } else {
            double progress = exp.lifetime / exp.maxLifetime; // normalized 0.0 -> 1.0
            
            // 1. Expand radius
            exp.currentRadius = exp.maxRadius * progress;
            exp.mesh->setRadius(exp.currentRadius);

            // 2. Fade out alpha value directly in diffuse color (Fix: m_material->getColor/setA)
            exp.mesh->m_material->m_diffuse.setA(1.0f - static_cast<float>(progress));
        }
    }
    // Remove inactive explosions from vector
    activeExplosions.erase(
        remove_if(activeExplosions.begin(), activeExplosions.end(), 
                  [](const Explosion& e) { return !e.active; }), 
        activeExplosions.end()
    );
}

// Dynamic difficulty adjustments based on current score
void adjustDifficulty() {
    int currentLevel = (score / 10) + 1; // Level increases every 10 points

    // Level up decreases spawn cooldown (faster incoming waves)
    spawnInterval = max(0.8, 3.0 - (currentLevel - 1) * 0.4);

    // Speed bonus increases to make enemies move progressively faster
    speedBonus = (currentLevel - 1) * 0.35;
}

// =============================================================================
// GAME PLAY SYSTEMS & LOGIC
// =============================================================================

// Spawn enemies with meshes and construct AABB trees for collision
void spawnEnemy(double currentTime) {
    if (gameOver || gameWin) return;

    if (currentTime - lastSpawnTime < spawnInterval) return;
    lastSpawnTime = currentTime;

    Enemy e;
    e.isAir = (rand() % 2 == 0); // 50% tank, 50% plane
    e.speed = (1.5 + (rand() % 100) / 100.0) + speedBonus; // Speed randomized & scaled by level

    double spawnY = -6.0 + (rand() % 120) / 10.0; // Random lane between Y = [-6.0, 6.0]
    
    e.mesh = new cMultiMesh(); // Fix: cMesh to cMultiMesh
    bool success = false;

    if (e.isAir) {
        // Load airplane .obj model
        success = e.mesh->loadFromFile("resources/models/plane_enemy.obj");
        if (success) {
            // Randomize blue, cyan and indigo shades for airplanes
            float r = 0.1f + (rand() % 25) / 100.0f; // 0.10 - 0.35
            float g = 0.4f + (rand() % 40) / 100.0f; // 0.40 - 0.80
            float b = 0.7f + (rand() % 30) / 100.0f; // 0.70 - 1.00
            cColorf colorCyan(r, g, b, 1.0f);
            for (unsigned int i = 0; i < e.mesh->getNumMeshes(); ++i) {
                e.mesh->getMesh(i)->m_material->setColor(colorCyan);
            }
        }
        e.position = cVector3d(25.0, spawnY, 2.0); // Flight altitude Z = 2.0
    } else {
        // Load tank .obj model
        success = e.mesh->loadFromFile("resources/models/tank_enemy.obj");
        if (success) {
            // Randomize red, orange, and crimson shades for tanks
            float r = 0.65f + (rand() % 35) / 100.0f; // 0.65 - 1.00
            float g = 0.05f + (rand() % 30) / 100.0f; // 0.05 - 0.35
            float b = 0.05f + (rand() % 30) / 100.0f; // 0.05 - 0.35
            cColorf colorRed(r, g, b, 1.0f);
            for (unsigned int i = 0; i < e.mesh->getNumMeshes(); ++i) {
                e.mesh->getMesh(i)->m_material->setColor(colorRed);
            }
        }
        e.position = cVector3d(25.0, spawnY, 0.25); // Ground clearance Z = 0.25
    }

    if (!success) {
        // Fallback procedural geometry if model loading fails
        cMesh* fallbackMesh = new cMesh();
        if (e.isAir) {
            cCreateSphere(fallbackMesh, 0.4);
            float r = 0.1f + (rand() % 25) / 100.0f;
            float g = 0.4f + (rand() % 40) / 100.0f;
            float b = 0.7f + (rand() % 30) / 100.0f;
            cColorf colorBlue(r, g, b, 1.0f);
            fallbackMesh->m_material->setColor(colorBlue);
        } else {
            cCreateBox(fallbackMesh, 0.8, 0.6, 0.4);
            float r = 0.65f + (rand() % 35) / 100.0f;
            float g = 0.05f + (rand() % 30) / 100.0f;
            float b = 0.05f + (rand() % 30) / 100.0f;
            cColorf colorLightRed(r, g, b, 1.0f);
            fallbackMesh->m_material->setColor(colorLightRed);
        }
        e.mesh->addMesh(fallbackMesh);
        std::cout << "[MODEL_WARNING] Failed to load .obj file, used fallback basic geometry." << std::endl;
    }

    // MANDATORY: Setup the AABB Tree structure for precise polygon collision detection
    e.mesh->createAABBCollisionDetector(0.01);

    world->addChild(e.mesh);
    e.mesh->setLocalPos(e.position);
    activeEnemies.push_back(e);
}

// Initialize environment, materials, lights, and load base artillery meshes
void initGame() {
    srand(static_cast<unsigned int>(time(NULL)));

    // Create World
    world = new cWorld();
    world->setBackgroundColor(cColorf(0.12f, 0.16f, 0.22f)); // Soft dark sky background

    // Create Camera and setup Viewport
    camera = new cCamera(world);
    world->addChild(camera);
    camera->set(cVector3d(-10.0, 0.0, 7.0),  // Positioned behind the defense cannon
                cVector3d(12.0, 0.0, 0.5),   // Pointing forward down the battlefield
                cVector3d(0.0, 0.0, 1.0));   // Z-Axis is Up
    camera->setClippingPlanes(0.1, 50.0);

    // Dynamic Directional Lighting
    light = new cDirectionalLight(world);
    world->addChild(light);
    light->setEnabled(true);
    light->setDir(cVector3d(1.0, 0.4, -1.0)); // Shines diagonally downwards

    // Render Battlefield Terrain Ground Plane
    ground = new cMesh();
    world->addChild(ground);
    cCreatePlane(ground, 40.0, 30.0); // 40m long, 30m wide field
    ground->setLocalPos(10.0, 0, 0); // Position ahead of the cannon base

    // Add a coordinate grid / visual guide lines to the ground for depth perspective
    cMesh* gridGuide = new cMesh();
    cCreatePlane(gridGuide, 40.0, 30.0);
    gridGuide->setLocalPos(10.0, 0.0, 0.001); // Position slightly above grass plane to prevent Z-fighting
    gridGuide->setWireMode(true); // Draw only gridlines/wireframe
    cColorf gridColor(0.3f, 0.5f, 0.3f, 0.4f);
    gridGuide->m_material->setColor(gridColor);
    world->addChild(gridGuide);
    
    cColorf colorGrass(0.2f, 0.35f, 0.2f, 1.0f);
    ground->m_material->setColor(colorGrass); // Grass green
    ground->m_material->m_specular.set(0.0f, 0.0f, 0.0f, 0.0f); // No shiny plastic grass (Fix: m_specular.set)

    // Initialize Artillery Cannon Base
    cannonBase = new cMultiMesh(); // Fix: cMesh to cMultiMesh
    if (cannonBase->loadFromFile("resources/models/cannon_base.obj")) {
        cColorf colorGray(0.25f, 0.25f, 0.3f, 1.0f);
        // cannonBase->m_material->setColor(colorGray); // Removed unsafe parent material access
        for (unsigned int i = 0; i < cannonBase->getNumMeshes(); ++i) {
            cannonBase->getMesh(i)->m_material->setColor(colorGray);
            cannonBase->getMesh(i)->m_material->m_specular.set(0.5f, 0.5f, 0.5f, 1.0f);
            cannonBase->getMesh(i)->m_material->setShininess(80);
        }
    } else {
        // Fallback base
        cMesh* fallbackMesh = new cMesh();
        cCreateBox(fallbackMesh, 0.8, 0.8, 0.6);
        cColorf colorGray(0.25f, 0.25f, 0.3f, 1.0f);
        fallbackMesh->m_material->setColor(colorGray);
        cannonBase->addMesh(fallbackMesh);
        std::cout << "[MODEL_WARNING] Failed to load cannon_base.obj, using box fallback." << std::endl;
    }
    world->addChild(cannonBase);
    cannonBase->setLocalPos(0, 0, 0);

    // Initialize Artillery Cannon Barrel
    cannonBarrel = new cMultiMesh(); // Fix: cMesh to cMultiMesh
    if (cannonBarrel->loadFromFile("resources/models/cannon_barrel.obj")) {
        cColorf colorSteelBlue(0.15f, 0.15f, 0.2f, 1.0f);
        // cannonBarrel->m_material->setColor(colorSteelBlue); // Removed unsafe parent material access
        for (unsigned int i = 0; i < cannonBarrel->getNumMeshes(); ++i) {
            cannonBarrel->getMesh(i)->m_material->setColor(colorSteelBlue);
            cannonBarrel->getMesh(i)->m_material->m_specular.set(0.6f, 0.6f, 0.6f, 1.0f);
            cannonBarrel->getMesh(i)->m_material->setShininess(90);
        }
    } else {
        // Fallback barrel
        cMesh* fallbackMesh = new cMesh();
        cCreateCylinder(fallbackMesh, 1.2, 0.12);
        cColorf colorSteelBlue(0.15f, 0.15f, 0.2f, 1.0f);
        fallbackMesh->m_material->setColor(colorSteelBlue);
        cannonBarrel->addMesh(fallbackMesh);
        std::cout << "[MODEL_WARNING] Failed to load cannon_barrel.obj, using cylinder fallback." << std::endl;
    }
    // MANDATORY Scene Graph parenting: Connecting barrel directly to base
    cannonBase->addChild(cannonBarrel);
    cannonBarrel->setLocalPos(0, 0, 0.3); // Offset vertically above base rotation pivot

    // =============================================================================
    // LASER SIGHT (AIMING ASSIST VISUAL BEAM)
    // =============================================================================
    cMesh* laserSight = new cMesh();
    cCreateCylinder(laserSight, 40.0, 0.005); // Create a 40m long, very thin laser beam cylinder
    laserSight->setLocalPos(20.0, 0.0, 0.0); // Offset by half-length so it shoots forward from the muzzle
    
    // Rotate 90 degrees around Y-axis to align cylinder (default Z-axis) with barrel X-forward axis
    cMatrix3d rotLaser;
    rotLaser.setExtrinsicEulerRotationDeg(0.0, 90.0, 0.0, C_EULER_ORDER_XYZ);
    laserSight->setLocalRot(rotLaser);
    
    // Set glowing semi-transparent neon green color
    cColorf laserColor(0.0f, 1.0f, 0.2f, 0.35f);
    laserSight->m_material->setColor(laserColor);
    laserSight->m_material->m_emission.set(0.0f, 1.0f, 0.2f, 1.0f); // Green emission glow
    laserSight->setUseTransparency(true);
    
    cannonBarrel->addChild(laserSight);

    // =============================================================================
    // UI (HUD) INITIALIZATION
    // =============================================================================
    font = cFont::create();

    // Try to load font from various common directories dynamically
    bool fontLoaded = false;
    vector<string> fontPaths = {
        "resources/fonts/calibri-24.fnt",
        "../resources/fonts/calibri-24.fnt",
        "../../resources/fonts/calibri-24.fnt",
        "calibri-24.fnt",
        "../../Tool/chai3d/bin/resources/fonts/calibri-24.fnt",
        "../../../Tool/chai3d/bin/resources/fonts/calibri-24.fnt",
        "resources/fonts/consolas-24.fnt",
        "../resources/fonts/consolas-24.fnt",
        "../../resources/fonts/consolas-24.fnt",
        "consolas-24.fnt",
        "../../Tool/chai3d/bin/resources/fonts/consolas-24.fnt",
        "../../../Tool/chai3d/bin/resources/fonts/consolas-24.fnt"
    };
    for (const auto& path : fontPaths) {
        if (font->loadFromFile(path)) {
            // Verify that the companion PNG texture image was also successfully loaded
            if (font->m_texture && font->m_texture->m_image && font->m_texture->m_image->getWidth() > 0) {
                fontLoaded = true;
                std::cout << "[UI] Loaded font and texture successfully from: " << path << std::endl;
                break;
            } else {
                std::cout << "[UI_WARNING] Font file found at " << path << " but companion PNG texture image is missing or corrupt!" << std::endl;
            }
        }
    }
    if (!fontLoaded) {
        std::cout << "[UI_WARNING] Could not load calibri-24.fnt. HUD text might be blank!" << std::endl;
    }

    if (fontLoaded) {
        std::cout << "[DEBUG_UI] Creating scoreLabel..." << std::endl;
        scoreLabel = new cLabel(font);
        cColorf colorWhite(1.0f, 1.0f, 1.0f, 1.0f);
        scoreLabel->m_fontColor = colorWhite;
        
        std::cout << "[DEBUG_UI] Checking camera->m_frontLayer..." << std::endl;
        if (camera == nullptr) {
            std::cout << "[DEBUG_UI] CRITICAL: camera is NULL!" << std::endl;
        } else if (camera->m_frontLayer == nullptr) {
            std::cout << "[DEBUG_UI] camera->m_frontLayer is NULL! Creating a new one..." << std::endl;
            camera->m_frontLayer = new cWorld();
        } else {
            std::cout << "[DEBUG_UI] camera->m_frontLayer is valid: " << camera->m_frontLayer << std::endl;
        }
        
        std::cout << "[DEBUG_UI] Adding scoreLabel to m_frontLayer..." << std::endl;
        camera->m_frontLayer->addChild(scoreLabel);

        std::cout << "[DEBUG_UI] Creating escapeLabel..." << std::endl;
        escapeLabel = new cLabel(font);
        cColorf colorYellow(1.0f, 1.0f, 0.0f, 1.0f);
        escapeLabel->m_fontColor = colorYellow;
        std::cout << "[DEBUG_UI] Adding escapeLabel to m_frontLayer..." << std::endl;
        camera->m_frontLayer->addChild(escapeLabel);

        std::cout << "[DEBUG_UI] Creating stateLabel..." << std::endl;
        stateLabel = new cLabel(font);
        stateLabel->m_fontColor = colorWhite;
        stateLabel->setEnabled(false); // Hidden by default
        std::cout << "[DEBUG_UI] Adding stateLabel to m_frontLayer..." << std::endl;
        camera->m_frontLayer->addChild(stateLabel);
        std::cout << "[DEBUG_UI] HUD Widgets created successfully." << std::endl;
    } else {
        std::cout << "[UI_INFO] Font was not loaded. Skipping HUD widgets initialization to prevent crash." << std::endl;
    }

    // =============================================================================
    // AUDIO (SPATIALIZED 3D SOUND) INITIALIZATION
    // =============================================================================
    audioDevice = new cAudioDevice();
    camera->attachAudioDevice(audioDevice);

    bool soundLoaded = false;
    vector<string> soundPaths = {
        "resources/sounds/explosion.wav",
        "../resources/sounds/explosion.wav",
        "../../resources/sounds/explosion.wav",
        "explosion.wav",
        "../Tool/chai3d/bin/resources/sounds/explosion.wav",
        "../../Tool/chai3d/bin/resources/sounds/explosion.wav"
    };

    // Instantiate buffer from device factory
    explosionBuffer = new cAudioBuffer();

    for (const auto& path : soundPaths) {
        if (explosionBuffer->loadFromFile(path)) {
            soundLoaded = true;
            std::cout << "[AUDIO] Loaded explosion sound successfully from: " << path << std::endl;
            break;
        }
    }

    if (soundLoaded) {
        explosionSource = new cAudioSource();
        explosionSource->setAudioBuffer(explosionBuffer);
        
        // TĂNG ÂM LƯỢNG NGUỒN PHÁT ĐỂ NGHE RÕ Ở KHOẢNG CÁCH XA (X = 25m)
        explosionSource->setGain(8.0); 
        
        std::cout << "[AUDIO] Spatialized 3D explosion source initialized successfully." << std::endl;
    } else {
        std::cout << "[AUDIO_WARNING] Could not load explosion.wav. Sound will be disabled!" << std::endl;
    }

    // Initalize object pool
    initBulletPool();

    std::cout << "======================================================" << std::endl;
    std::cout << " ARTILLERY TOWN DEFENSE (CHAI3D / OpenGL)" << std::endl;
    std::cout << " Controls: A/D (Left/Right), W/S (Up/Down), Space (Fire)" << std::endl;
    std::cout << " Goal: Kill 50 enemies to Win. If 20 escape, Game Over." << std::endl;
    std::cout << "======================================================" << std::endl;
}

// Update loop (running at ~60Hz) containing collision & game state evaluations
void updateGame(double deltaTime) {
    // =============================================================================
    // UI (HUD) UPDATES
    // =============================================================================
    if (scoreLabel && escapeLabel && stateLabel) {
        std::cout << "[DEBUG_UI_UPDATE] Setting scoreLabel position..." << std::endl;
        scoreLabel->setLocalPos(30, windowHeight - 45, 0);
        std::cout << "[DEBUG_UI_UPDATE] Setting escapeLabel position..." << std::endl;
        escapeLabel->setLocalPos(30, windowHeight - 80, 0);

        if (score != lastScore || escapedEnemies != lastEscaped) {
            lastScore = score;
            lastEscaped = escapedEnemies;

            std::cout << "[DEBUG_UI_UPDATE] Setting scoreLabel text..." << std::endl;
            scoreLabel->setText("Score: " + to_string(score) + " / " + to_string(WIN_SCORE));
            std::cout << "[DEBUG_UI_UPDATE] Setting escapeLabel text..." << std::endl;
            escapeLabel->setText("Breached: " + to_string(escapedEnemies) + " / " + to_string(MAX_ESCAPED));

            if (escapedEnemies >= MAX_ESCAPED * 0.7) {
                cColorf colorRed(1.0f, 0.2f, 0.2f, 1.0f);
                escapeLabel->m_fontColor = colorRed; // Red alert
            } else {
                cColorf colorYellow(1.0f, 1.0f, 0.0f, 1.0f);
                escapeLabel->m_fontColor = colorYellow; // Yellow standard
            }
        }

        if (gameWin) {
            stateLabel->setEnabled(true);
            stateLabel->setText("VICTORY! YOU PROTECTED THE TOWN!");
            cColorf greenColor(0.2f, 1.0f, 0.2f, 1.0f);
            stateLabel->m_fontColor = greenColor;
            stateLabel->setLocalPos((windowWidth - stateLabel->getWidth()) / 2.0, windowHeight / 2.0, 0);
        } else if (gameOver) {
            stateLabel->setEnabled(true);
            stateLabel->setText("GAME OVER! THE TOWN HAS FALLEN!");
            cColorf redColor(1.0f, 0.1f, 0.1f, 1.0f);
            stateLabel->m_fontColor = redColor;
            stateLabel->setLocalPos((windowWidth - stateLabel->getWidth()) / 2.0, windowHeight / 2.0, 0);
        }
    }

    if (gameOver || gameWin) return;

    // 1. Update Cannon Yaw (Base rotation on Z-Axis) and Pitch (Barrel elevation on Y-Axis)
    cMatrix3d rotBase, rotBarrel;
    rotBase.setExtrinsicEulerRotationDeg(0, 0, barrelYaw, C_EULER_ORDER_XYZ);
    cannonBase->setLocalRot(rotBase);

    rotBarrel.setExtrinsicEulerRotationDeg(0, -barrelPitch, 0, C_EULER_ORDER_XYZ);
    cannonBarrel->setLocalRot(rotBarrel);

    // 2. Update Bullets Flight & Collision Detection (using Segment-based AABB Trees)
    for (auto& b : bulletPool) {
        if (!b.active) continue;

        cVector3d posOld = b.position;
        cVector3d posNew = b.position + b.velocity * deltaTime;
        b.position = posNew;
        b.mesh->setLocalPos(posNew);

        // Discard bullet if it flies too far or sinks below terrain
        if (b.position.x() > 30.0 || b.position.z() < -0.1) {
            recycleBullet(b);
            continue;
        }

        // Check collision against all active enemies using AABB Tree raycasting segments
        for (auto& e : activeEnemies) {
            if (!e.active) continue;

            // Prepare recorder and settings for CHAI3D physics collision query
            cCollisionRecorder recorder;
            cCollisionSettings settings;
            settings.m_checkForNearestCollisionOnly = true; // Fix: use m_checkForNearestCollisionOnly

            // Execute segment-mesh intersection through hierarchical AABB tree
            bool isHit = e.mesh->computeCollisionDetection(posOld, posNew, recorder, settings);

            // Aim Assist (Magnetic Bullet fallback): if segment missed, check distance to enemy center
            if (!isHit) {
                double dist = (posNew - e.position).length();
                double threshold = e.isAir ? 0.95 : 0.75; // More generous hit box for fast-flying planes
                if (dist < threshold) {
                    isHit = true;
                }
            }

            if (isHit) {

                cVector3d collisionPos = e.position;
                
                // Spawn explosion effect at exact contact coordinates
                spawnExplosion(collisionPos);

                // Deactivate entities
                recycleBullet(b);
                e.active = false;
                world->removeChild(e.mesh);
                delete e.mesh;

                // Update score and scale difficulty
                score++;
                std::cout << "-> HIT! Target destroyed. Score: " << score << "/" << WIN_SCORE << std::endl;
                adjustDifficulty();

                if (score >= WIN_SCORE) {
                    gameWin = true;
                    std::cout << "======================================================" << std::endl;
                    std::cout << "               CONGRATULATIONS! YOU WIN!              " << std::endl;
                    std::cout << "======================================================" << std::endl;
                }
                break; // Break activeEnemies loop for this bullet as it has already exploded
            }
        }
    }

    // 3. Update Enemy Movements
    for (auto& e : activeEnemies) {
        if (!e.active) continue;

        // Move towards cannon baseline at X = 0
        e.position.x(e.position.x() - e.speed * deltaTime);
        e.mesh->setLocalPos(e.position);

        // Breach detection (reaches target base defense line)
        if (e.position.x() <= 0.0) {
            e.active = false;
            world->removeChild(e.mesh);
            delete e.mesh;
            escapedEnemies++;

            std::cout << "-> WARNING: Enemy breached! Escaped count: " << escapedEnemies << "/" << MAX_ESCAPED << std::endl;

            if (escapedEnemies >= MAX_ESCAPED) {
                gameOver = true;
                std::cout << "======================================================" << std::endl;
                std::cout << "             GAME OVER! THE TOWN HAS FALLEN!          " << std::endl;
                std::cout << "======================================================" << std::endl;
            }
        }
    }

    // 4. Update Fading Explosion Spheres
    updateExplosions(deltaTime);

    // 5. Clean inactive elements from activeEnemies vector
    activeEnemies.erase(
        remove_if(activeEnemies.begin(), activeEnemies.end(), 
                  [](const Enemy& e) { return !e.active; }), 
        activeEnemies.end()
    );
}

// =============================================================================
// INPUT HANDLERS CALLBACK
// =============================================================================
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

    // Handle restart key R (can be pressed at any time, even when game is over or won)
    if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        std::cout << "[GAME] Restarting game..." << std::endl;
        
        // Reset game state variables
        score = 0;
        escapedEnemies = 0;
        gameOver = false;
        gameWin = false;
        lastScore = -1;
        lastEscaped = -1;
        barrelPitch = 20.0;
        barrelYaw = 0.0;
        spawnInterval = 3.0;
        lastSpawnTime = glfwGetTime();
        speedBonus = 0.0;

        // Reset HUD labels
        if (stateLabel) {
            stateLabel->setEnabled(false);
        }

        // Clean up active enemies
        for (auto& e : activeEnemies) {
            world->removeChild(e.mesh);
            delete e.mesh;
        }
        activeEnemies.clear();

        // Clean up active explosions
        for (auto& exp : activeExplosions) {
            world->removeChild(exp.mesh);
            delete exp.mesh;
        }
        activeExplosions.clear();

        // Recycle all bullets
        for (auto& b : bulletPool) {
            recycleBullet(b);
        }

        return;
    }

    if (gameOver || gameWin) return;

    double controlSpeed = 3.5; // Degrees of rotation per button event

    switch (key) {
        case GLFW_KEY_A:
        case GLFW_KEY_LEFT:
            barrelYaw += controlSpeed; // Rotate Cannon left
            break;
        case GLFW_KEY_D:
        case GLFW_KEY_RIGHT:
            barrelYaw -= controlSpeed; // Rotate Cannon right
            break;
        case GLFW_KEY_W:
        case GLFW_KEY_UP:
            if (barrelPitch < 65.0) barrelPitch += controlSpeed; // Cap elevation (Up limit)
            break;
        case GLFW_KEY_S:
        case GLFW_KEY_DOWN:
            if (barrelPitch > 5.0) barrelPitch -= controlSpeed;  // Cap depression (Down limit)
            break;
        case GLFW_KEY_SPACE:
            if (action == GLFW_PRESS) {
                // Request a dormant bullet from our pre-allocated Pool
                Bullet* b = getAvailableBullet();
                if (b) {
                    cMatrix3d rot;
                    rot.setExtrinsicEulerRotationDeg(0, -barrelPitch, barrelYaw, C_EULER_ORDER_XYZ);
                    cVector3d fireDirection = rot.getCol0(); // Lấy vector trục X tiến tới
                    
                    // Tọa độ gốc: Thân pháo (0,0,0) + độ cao nòng (0.3) + khoảng cách tiến tới (1.2)
                    b->position = cVector3d(0, 0, 0.3) + fireDirection * 1.2;
                    b->velocity = fireDirection * 18.0; 
                    b->mesh->setLocalPos(b->position);
                }
            }
            break;
    }
}

// =============================================================================
// MAIN ENTRY POINT
// =============================================================================
int main(int argc, char** argv) {
    // 1. Initialize GLFW library
    if (!glfwInit()) {
        std::cerr << "CRITICAL ERROR: Failed to initialize GLFW." << std::endl;
        return -1;
    }

    // 2. Setup GLFW Rendering Context and Window Properties
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "Artillery Town Defense - CHAI3D", NULL, NULL);
    
    if (!window) {
        std::cerr << "CRITICAL ERROR: Failed to create GLFW window." << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    // Initialize GLEW (OpenGL Extension Wrangler)
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "CRITICAL ERROR: Failed to initialize GLEW library." << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwSetKeyCallback(window, keyCallback);

    // Swap buffers immediately on frame draw to sync monitor refresh rates (V-Sync)
    glfwSwapInterval(1); 

    // 3. Initialize CHAI3D Game World and Geometry
    initGame();

    double lastTime = glfwGetTime();

    // 4. MAIN GAME & GRAPHICS LOOP (Approx 60Hz)
    while (!glfwWindowShouldClose(window)) {
        double currentTime = glfwGetTime();
        double deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        // Prevent teleporting issues or physics instability during massive frame lags
        if (deltaTime > 0.1) deltaTime = 0.1;

        // Spawn Enemy Generator
        spawnEnemy(currentTime);

        // Core Game Physics and Collision Updates
        updateGame(deltaTime);

        // Setup OpenGL Viewport
        glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
        glViewport(0, 0, windowWidth, windowHeight);

        // Draw and Render 3D World Scene through Camera
        std::cout << "[DEBUG_RENDER] Rendering view..." << std::endl;
        camera->renderView(windowWidth, windowHeight);
        std::cout << "[DEBUG_RENDER] View rendered successfully." << std::endl;

        // Swap backbuffer and poll keyboard inputs
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // 5. Cleanup memory allocations upon exit
    std::cout << "[SHUTDOWN] Releasing allocated game world entities..." << std::endl;
    delete world;
    glfwTerminate();
    return 0;
}

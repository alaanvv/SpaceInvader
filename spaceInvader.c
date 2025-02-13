#include "raylib.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

#define MIN(x, y) (x < y ? x : y)
#define MAX(x, y) (x > y ? x : y)
#define CLAMP(x, y, z) (MAX(MIN(z, y), x))
#define CIRCULAR_CLAMP(x, y, z) ((y < x) ? z : ((y > z) ? x : y))
#define WINDOW_WIDTH  800
#define WINDOW_HEIGHT 600
#define BULLET_WIDTH  10
#define BULLET_HEIGHT 15
#define SHIP_WIDTH    32
#define SHIP_HEIGHT   32
#define SAVE_PATH "./save.txt"
#define RANK_PATH "./rank.txt"
#define NAME_SIZE 3
#define NUM_STARS  50
#define STAR_SPEED 0.3
#define STAR_SIZE  2
#define VOLUME 0.5
#define BACKGROUND_COLOR (Color) { 10, 0, 10 }
#define PLAYER_BULLET_COLOR WHITE
#define ENEMY_BULLET_COLOR  GREEN
#define DAMAGE_REDNESS 8
#define PLAYER_SPEED 4

// ---

typedef enum {
  START_SCREEN, MODE_SCREEN, GAME_SCREEN, END_SCREEN
} Stage;

typedef enum {
  NORMAL, HARD, HARDCORE
} Mode;

typedef struct {
  Rectangle pos, bullet;
  int hp, shooting;
  float last_shoot, shoot_timer, speed, bullet_speed;
} Ship;

typedef struct {
  Texture2D player, enemy;
  Sound s_key, s_undo, s_enter, s_hit;
  Sound s_nop, s_death, s_shoot[4], s_e_shoot;
  Music music;
} Assets;

typedef struct {
  Mode mode;
  Rectangle borders[4];
  Ship player, enemy;
  Assets assets;
  Stage stage;
  char nick[3];
  int winner, pts;
} Game;

typedef struct {
  int running;
  float start, duration;
} Animation;

// ---

void  InitGame();
void  InitMatch();
void  ReadFiles();
void  WriteRank();
void  StageStart();
void  StageMode();
void  StageEnd();
void  StageGame();
void  DrawEnemies();
void  DrawPlayer();
void  DrawBullets();
void  DrawStars();
void  PlayerMovement();
void  EnemiesMovement();
void  EnemyShoot();
void  PlayerShoot();
void  EnemiesBulletCollision();
void  PlayerBulletCollision();
int   StageInEvent();
void  SetStage(Stage stage);
void  LoadAssets();
void  UnloadAssets();
void  StartAnimation(Animation* anim);
float AnimationKeyFrame(Animation* anim);
void  StartTransition(Stage to);
void  DrawTransition();
float Shake(float x, float speed, float intensity);
float TimeSince(float x);
void  DrawCenteredText(char* str, int size, int x, int y, Color color);

// ---

Game g;

char saves[5][16] = { 0 };
char  rank[5][16] = { 0 };

Animation a_player_out = { 0, 0, 2 };
Animation a_player_inn = { 0, 0, 1 };
Animation transition = { 0, 0, 0.5 };
Stage transition_to;
int transitioned = 0;

Color background_color = BACKGROUND_COLOR;
float stars[NUM_STARS][2];
float star_speed = STAR_SPEED;

int stage_in_event = 0;
int enemy_direction = 1;
int player_immune = 0;

// ---

int main() {
  SetRandomSeed((long) &g);
  InitGame();
  SetStage(START_SCREEN);

  while (!WindowShouldClose()) {
    UpdateMusicStream(g.assets.music);

    BeginDrawing();
    ClearBackground(background_color);
    DrawStars();
    if      (g.stage == START_SCREEN) StageStart();
    else if (g.stage == MODE_SCREEN)  StageMode();
    else if (g.stage == END_SCREEN)   StageEnd();
    else if (g.stage == GAME_SCREEN)  StageGame();
    DrawTransition();
    EndDrawing();
  }

  UnloadAssets();
  CloseWindow();
  return 0;
}

// Initialization

void InitGame() {
  InitAudioDevice();
  InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Space Invaders");
  SetTargetFPS(60);
  LoadAssets();
  SetMusicVolume(g.assets.music, VOLUME * 0.3);
  SetMasterVolume(VOLUME);
  PlayMusicStream(g.assets.music);

  g.borders[0] = (Rectangle) { 0, -10, WINDOW_WIDTH, 10 };           // Up
  g.borders[1] = (Rectangle) { 0, WINDOW_HEIGHT, WINDOW_WIDTH, 10 }; // Bottom
  g.borders[2] = (Rectangle) { -10, 0, 10, WINDOW_HEIGHT };          // Left
  g.borders[3] = (Rectangle) { WINDOW_WIDTH, 0, 10, WINDOW_HEIGHT }; // Right

  g.player.speed  = PLAYER_SPEED;
  g.player.bullet_speed = 10;
  g.enemy.speed  = 3;
  g.enemy.bullet_speed = 5;

  for (int i = 0; i < NUM_STARS; i++) {
    stars[i][0] = GetRandomValue(0, WINDOW_WIDTH);
    stars[i][1] = GetRandomValue(0, WINDOW_HEIGHT);
  }
}

void InitMatch() {
  background_color = BACKGROUND_COLOR;
  star_speed = STAR_SPEED;
  g.winner = 0;
  g.pts = 0;
  g.player.pos = (Rectangle) { WINDOW_WIDTH / 2.0 - SHIP_WIDTH / 2.0, WINDOW_HEIGHT - SHIP_HEIGHT - 10, SHIP_WIDTH, SHIP_HEIGHT };

  ReadFiles();
}

// Rank

void ReadFiles() {
  // Create file if dont exist
  FILE* f_save = fopen(SAVE_PATH, "a");
  FILE* f_rank = fopen(RANK_PATH, "a");

  // Read from file
  f_save = fopen(SAVE_PATH, "r");
  f_rank = fopen(RANK_PATH, "r");

  int i = 0;
  while (fgets(saves[i], 32, f_save) && i < 5) i++;
  i = 0;
  while (fgets(rank[i], 32, f_rank) && i < 5) i++;

  fclose(f_save);
  fclose(f_rank);
}

void WriteRank() {
  char new_save[32];
  sprintf(new_save, "%s %d\n", g.nick, g.pts);

  for (int i = 4; i >= 0; i--)
    strcpy(saves[i], i ? saves[i - 1] : new_save);

  for (int i = 4; i >= 0; i--) {
    int pts;
    sscanf(rank[i], "%*s %d", &pts);

    if (!*rank[i] || g.pts > pts) {
      if (i != 4) strcpy(rank[i + 1], rank[i]);
      strcpy(rank[i], new_save);
    }
  }

  FILE* f_save = fopen(SAVE_PATH, "w");
  FILE* f_rank = fopen(RANK_PATH, "w");

  for (int i = 0; i < 5; i++) {
    if (*saves[i]) fprintf(f_save, "%s", saves[i]);
    if (*rank[i])  fprintf(f_rank, "%s", rank[i]);
  }

  fclose(f_save);
  fclose(f_rank);
}

// Stage

void StageStart() {
  // if-block that only runs once per stage
  if (StageInEvent()) {
    g.nick[0] = '\0';
    InitMatch();
  }

  // How many letters remaining to complete name
  int remaining = NAME_SIZE - strlen(g.nick);

  // Letter from a-Z
  int key = toupper(GetCharPressed());
  if (key >= 65 && key <= 90) {
    if (!remaining) PlaySound(g.assets.s_nop); // Play NOP when nothing happens
    else {
      g.nick[strlen(g.nick) + 1] = '\0';
      g.nick[strlen(g.nick)] = key;
      PlaySound(g.assets.s_key);
    }
  }

  // Swap between showing the rank or last games
  static int rank_toggle = 0;
  if (IsKeyPressed(KEY_TAB)) {
    rank_toggle = !rank_toggle;
    PlaySound(g.assets.s_key);
  }

  if (IsKeyPressed(KEY_BACKSPACE)) {
    if (!strlen(g.nick)) PlaySound(g.assets.s_nop);
    else {
      g.nick[strlen(g.nick) - 1] = '\0';
      PlaySound(g.assets.s_undo);
    }
  }

  if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
    if (remaining) PlaySound(g.assets.s_nop);
    else {
      StartTransition(MODE_SCREEN);
      PlaySound(g.assets.s_enter);
    }
  }

  // Label Buffer
  char label_buf[64];
  char empty[remaining + 1];
  memset(empty, ' ', remaining);
  empty[remaining] = '\0';
  sprintf(label_buf, ">%sNICKNAME%s<", empty, empty);

  // Nickname Buffer
  char nick_buf[64];
  strcpy(nick_buf, g.nick);
  if (strlen(g.nick) < NAME_SIZE) strcat(nick_buf, "_");

  DrawCenteredText("SPACE INVADERS", 69, 0, 40, DARKBROWN);
  DrawCenteredText("SPACE INVADERS", 70, 0, 30, YELLOW);
  DrawCenteredText(label_buf, 40, 0, 170, remaining ? WHITE : PURPLE);
  if (!remaining) DrawCenteredText(nick_buf,  40, Shake(-0.2, 13, 3), 220 + Shake(-0.2, 5, 4), DARKPURPLE);
  DrawCenteredText(nick_buf,  40, Shake(0, 13, 3), 220 + Shake(0, 5, 4), remaining ? WHITE : PURPLE);
  for (int i = 0; i < 5; i++) DrawCenteredText(rank_toggle ? rank[i] : saves[i], 50, 0, 300 + 55 * i, PURPLE);
}

void StageMode() {
  if (StageInEvent()) InitMatch();

  static Mode selected = NORMAL;

  if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) {
    if (selected == HARDCORE) PlaySound(g.assets.s_nop);
    else {
      selected++;
      PlaySound(g.assets.s_key);
    }
  }

  if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP)) {
    if (selected == 0) PlaySound(g.assets.s_nop);
    else {
      selected--;
      PlaySound(g.assets.s_key);
    }
  }

  if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
    StartTransition(GAME_SCREEN);
    g.mode = selected;
    PlaySound(g.assets.s_enter);
  }

  DrawCenteredText("SPACE INVADERS", 69, 0, 40, DARKBROWN);
  DrawCenteredText("SPACE INVADERS", 70, 0, 30, YELLOW);

  char buffer[32];
  for (int i = 0; i <= HARDCORE; i++) {
    Color color[]   = { WHITE,    PURPLE,     RED       };
    Color color_d[] = { GRAY,     DARKPURPLE, DARKBROWN };
    char mode[][32] = { "NORMAL", "HARD",     "HARDCORE"};

    sprintf(buffer, selected == i ? "> %s <" : "%s", mode[i]);
    if (selected == i) {
      DrawCenteredText(buffer, 55, Shake(-0.2, 9 * (i + 1), 4), 250 + 100 * i + Shake(-0.2, 5 * (i + 1), 3), color_d[i]);
      DrawCenteredText(buffer, 55, Shake(0,    9 * (i + 1), 4), 250 + 100 * i + Shake(0,    5 * (i + 1), 3), color[i]);
    }
    else DrawCenteredText(buffer, 50, 0, 250 + 100 * i, color[i]);
  }
}

void StageEnd() {
  if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
    StartTransition(g.winner ? GAME_SCREEN : START_SCREEN);
    PlaySound(g.assets.s_enter);
  }

  if (!g.winner) EnemiesMovement();

  if (a_player_out.running) {
    float x = AnimationKeyFrame(&a_player_out);
    g.player.pos.y = WINDOW_HEIGHT - SHIP_HEIGHT - 10 - pow(x / a_player_out.duration, 2) * WINDOW_HEIGHT;
  }

  char* message = g.winner ? "YOU WON" : "YOU DIED";
  Color color   = g.winner ? GREEN     : RED;
  Color color_d = g.winner ? DARKGREEN : DARKBROWN;

  DrawCenteredText(message, 80, Shake(-0.2, 13, 4), 250 + Shake(-0.2, 5, 5), color_d);
  DrawCenteredText(message, 80, Shake(0,    13, 4), 250 + Shake(0,    5, 5), color);
  DrawCenteredText("- Hit Enter -", 40, 0, WINDOW_HEIGHT - 50, GRAY);
  if (g.winner) DrawPlayer();
  else          DrawEnemies();
}

void StageGame() {
  if (StageInEvent()) {
    background_color = BACKGROUND_COLOR;
    star_speed = STAR_SPEED;

    g.player.pos.y  = WINDOW_HEIGHT - SHIP_HEIGHT - 10;
    g.player.bullet = (Rectangle) { 0, 0, BULLET_WIDTH, BULLET_HEIGHT };
    g.player.shooting = 0;
    player_immune     = 0;

    g.enemy.pos    = (Rectangle) { 0, 15, SHIP_WIDTH, SHIP_HEIGHT };
    g.enemy.bullet = (Rectangle) { 0, 0, BULLET_WIDTH, BULLET_HEIGHT };
    g.enemy.shooting = 0;
    enemy_direction  = 1;

    a_player_out.running = 0;    StartAnimation(&a_player_inn);

    g.enemy.last_shoot = GetTime();

    // Adjusts based on mode
    g.player.hp          = g.mode == NORMAL ? 3 : g.mode == HARD ? 2   : 1;
    g.enemy.speed        = g.mode == NORMAL ? 3 : g.mode == HARD ? 4.5 : 6;
    g.enemy.bullet_speed = g.mode == NORMAL ? 5 : g.mode == HARD ? 6   : 7;
    g.enemy.shoot_timer  = g.mode == NORMAL ? 3 : g.mode == HARD ? 1   : 0.1;
    background_color.r += g.mode * DAMAGE_REDNESS;
    star_speed         *= g.mode + 1;
  }

  if (player_immune) player_immune--;

  EnemiesMovement();
  PlayerMovement();
  EnemyShoot();
  PlayerShoot();
  DrawBullets();
  DrawEnemies();
  DrawPlayer();
}

// Draw

void DrawEnemies() {
  Vector2 frame_size = { 32, 32 };

  static int frame = 0;
  static int last_swap = 0;

  if (TimeSince(last_swap) > 1) {
    frame = !frame;
    last_swap = GetTime();
  }

  Rectangle frame_rec = { frame * frame_size.x, 0, frame_size.x, frame_size.y };
  Rectangle pos_rec   = { g.enemy.pos.x, g.enemy.pos.y, 32, 32 };
  DrawTexturePro(g.assets.enemy, frame_rec, pos_rec, (Vector2) { 0, 0 }, 0, WHITE);
}

void DrawPlayer() {
  Rectangle frame_rec = { 0, 0, 32, 32 };
  Rectangle pos_rec   = { g.player.pos.x, g.player.pos.y, 32, 32 };

  if (a_player_inn.running) {
    float x = AnimationKeyFrame(&a_player_inn);
    pos_rec.y += pow(x - 1, 2) * 50;
  }

  Color color = { 255, 255, 255, player_immune ? 127 : 255 };
  DrawTexturePro(g.assets.player, frame_rec, pos_rec, (Vector2) { 0, 0 }, 0, color);
}

void DrawBullets() {
  if (g.player.shooting) DrawRectangleRec(g.player.bullet, PLAYER_BULLET_COLOR);
  if (g.enemy.shooting)  DrawRectangleRec(g.enemy.bullet,  ENEMY_BULLET_COLOR);
}

void DrawStars() {
  for (int i = 0; i < NUM_STARS; i++) {
    stars[i][1] = CIRCULAR_CLAMP(-STAR_SIZE, stars[i][1] + star_speed, WINDOW_HEIGHT);
    DrawRectangle(stars[i][0], stars[i][1], STAR_SIZE, STAR_SIZE, WHITE);
  }
}

// Game functions

void PlayerMovement() {
  if ((IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) && !CheckCollisionRecs(g.player.pos, g.borders[3])) g.player.pos.x += g.player.speed;
  if ((IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  && !CheckCollisionRecs(g.player.pos, g.borders[2])) g.player.pos.x -= g.player.speed;
}

void EnemiesMovement() {
  if      (CheckCollisionRecs(g.enemy.pos, g.borders[2])) enemy_direction =  1;
  else if (CheckCollisionRecs(g.enemy.pos, g.borders[3])) enemy_direction = -1;

  g.enemy.pos.x += g.enemy.speed * enemy_direction;
}

void EnemyShoot() {
  if (g.enemy.shooting) {
    EnemiesBulletCollision();
    g.enemy.bullet.y += g.enemy.bullet_speed;
    return;
  }

  if (TimeSince(g.enemy.last_shoot) < g.enemy.shoot_timer) return;
  g.enemy.bullet.x = g.enemy.pos.x + g.enemy.pos.width  / 2;
  g.enemy.bullet.y = g.enemy.pos.y + g.enemy.pos.height / 2;
  g.enemy.shooting = 1;
  g.enemy.last_shoot = GetTime();
  PlaySound(g.assets.s_e_shoot);
}

void PlayerShoot() {
  if (g.player.shooting) {
    PlayerBulletCollision();
    g.player.bullet.y -= g.player.bullet_speed;
    return;
  }

  if (!IsKeyDown(32)) return;
  g.player.bullet.x = g.player.pos.x + g.player.pos.width  / 2 - BULLET_WIDTH  / 2.0;
  g.player.bullet.y = g.player.pos.y + g.player.pos.height / 2 - BULLET_HEIGHT / 2.0;
  g.player.shooting = 1;
  PlaySound(g.assets.s_shoot[GetRandomValue(0, 3)]);
}

void EnemiesBulletCollision() {
  if (!player_immune && CheckCollisionRecs(g.player.pos, g.enemy.bullet)) {
    g.enemy.shooting = 0;
    player_immune = 30;
    background_color.r += DAMAGE_REDNESS;
    if (--g.player.hp)
      PlaySound(g.assets.s_hit);
    else {
      // TODO Make death a function
      g.winner = 0;
      SetStage(END_SCREEN);
      PlaySound(g.assets.s_death);
      WriteRank();
      g.pts = 0;
    }
  }

  if (CheckCollisionRecs(g.enemy.bullet, g.borders[1]))
    g.enemy.shooting = 0;
}

void PlayerBulletCollision() {
  if (CheckCollisionRecs(g.enemy.pos, g.player.bullet)) {
    g.winner = 1;
    StartAnimation(&a_player_out);
    SetStage(END_SCREEN);
    PlaySound(g.assets.s_hit);
    g.pts += 100 * (g.mode + 1);
    g.player.shooting = 0;
  }

  if (CheckCollisionRecs(g.player.bullet, g.borders[0]))
    g.player.shooting = 0;
}

// Stage

int StageInEvent() {
  int tmp = stage_in_event;
  stage_in_event = 1;
  return !tmp;
}

void SetStage(Stage stage) {
  g.stage = stage;
  stage_in_event = 0;
}

// Assets

void LoadAssets() {
  g.assets.music      = LoadMusicStream("assets/soundtrack.mp3");
  g.assets.enemy      = LoadTexture("assets/enemy.png");
  g.assets.player     = LoadTexture("assets/player.png");
  g.assets.s_key      = LoadSound("assets/key.wav");
  g.assets.s_undo     = LoadSound("assets/undo.wav");
  g.assets.s_enter    = LoadSound("assets/enter.wav");
  g.assets.s_hit      = LoadSound("assets/hit.wav");
  g.assets.s_nop      = LoadSound("assets/nop.wav");
  g.assets.s_death    = LoadSound("assets/death.wav");
  g.assets.s_shoot[0] = LoadSound("assets/shoot_1.wav");
  g.assets.s_shoot[1] = LoadSound("assets/shoot_2.wav");
  g.assets.s_shoot[2] = LoadSound("assets/shoot_3.wav");
  g.assets.s_shoot[3] = LoadSound("assets/shoot_4.wav");
  g.assets.s_e_shoot  = LoadSound("assets/shoot.wav");
}

void UnloadAssets() {
  UnloadMusicStream(g.assets.music);
  UnloadTexture(g.assets.player);
  UnloadTexture(g.assets.enemy);
  UnloadSound(g.assets.s_key);
  UnloadSound(g.assets.s_undo);
  UnloadSound(g.assets.s_enter);
  UnloadSound(g.assets.s_hit);
  UnloadSound(g.assets.s_nop);
  UnloadSound(g.assets.s_death);
  UnloadSound(g.assets.s_shoot[0]);
  UnloadSound(g.assets.s_shoot[1]);
  UnloadSound(g.assets.s_shoot[2]);
  UnloadSound(g.assets.s_shoot[3]);
  UnloadSound(g.assets.s_e_shoot);
}

// Animation

void StartAnimation(Animation* anim) {
  anim->start   = GetTime();
  anim->running = 1;
}

float AnimationKeyFrame(Animation* anim) {
  float x = (TimeSince(anim->start));
  if (x >= anim->duration) anim->running = 0;
  return x;
}

// Transition

void StartTransition(Stage to) {
  StartAnimation(&transition);
  transition_to = to;
  transitioned = 0;
}

void DrawTransition() {
  if (!transition.running) return;
  float x = AnimationKeyFrame(&transition);
  if (x >= transition.duration / 2 && !transitioned) {
    SetStage(transition_to);
    transitioned = 1;
  }
  // TODO Transition varieties
  DrawRectangle(-WINDOW_WIDTH + x / transition.duration * 2 * WINDOW_WIDTH, 0, WINDOW_WIDTH, WINDOW_HEIGHT, BLACK);
}

// Utils

float Shake(float offset, float speed, float intensity) {
  return sin((GetTime() + offset) * speed) * intensity;
}

float TimeSince(float x) {
  return GetTime() - x;
}

void DrawCenteredText(char* str, int size, int x, int y, Color color) {
  DrawText(str, WINDOW_WIDTH / 2 - MeasureText(str, size) / 2 + x, y, size, color);
}

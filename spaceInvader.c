#include "raylib.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

#define MIN(x, y) (x < y ? x : y)
#define MAX(x, y) (x > y ? x : y)
#define CLAMP(x, y, z) (MAX(MIN(z, y), x))
#define RAND(min, max) (random() % (max - min) + min)

#define WINDOW_WIDTH  800
#define WINDOW_HEIGHT 600
#define BULLET_WIDTH  10
#define BULLET_HEIGHT 15
#define STD_WIDTH     32
#define STD_HEIGHT    32

#define SAVE_PATH "./save.txt"
#define RANK_PATH "./rank.txt"
#define NAME_SIZE 3
#define VOLUME 0.5
#define NUM_STARS 50
#define STAR_SPEED 0.3
#define STAR_SIZE 2

#define BACKGROUND_COLOR    (Color) { 10, 0, 10 }
#define PLAYER_BULLET_COLOR WHITE
#define ENEMY_BULLET_COLOR  GREEN

#define PLAYER_SPEED 4

typedef enum {
  START_SCREEN, DIFFICULTY_SCREEN, GAME_SCREEN, END_SCREEN
} Stage;

typedef enum {
  NORMAL, HARD, HARDCORE
} Difficulty;

typedef struct {
  int active, timer, speed;
  Rectangle pos;
  Color color;
  Sound sound;
} Bullet;

typedef struct {
  int speed, direction;
  Rectangle pos;
  Color  color;
  Bullet bullet;
} Enemy;

typedef struct {
  Rectangle pos;
  Color  color;
  Bullet bullet;
  int speed;
} Player;

typedef struct {
  Texture2D player, enemy;
  Sound s_key, s_undo, s_enter, s_hit, s_nop, s_death, s_shoot[4];
} Assets;

typedef struct {
  Enemy enemy;
  Player player;
  Rectangle borders[4];
  Assets assets;
  Music music;
  Stage stage;
  Difficulty difficulty;
  int winner;
  char nick[11];
  int pts;
} Game;

typedef struct {
  int running;
  float start;
} Animation;

void InitGame(Game *g);
void InitGameWindow(Game *g);
void InitShips(Game *g);
void FinishGame(Game *g);
void FinishGameWindow(Game *g);
void UpdateGame(Game *g);
void UpdateStartScreen(Game *g);
void UpdateDifficultyScreen(Game *g);
void UpdateEndScreen(Game *g);
void DrawGame(Game *g);
void DrawEnemies(Game *g);
void DrawPlayer(Game *g);
void DrawBullets(Game *g);
void DrawStars(Game *g);
void MoveStars(Game *g);
void PlayerMovement(Game *g);
void EnemiesMovement(Game *g);
void ShootEnemiesBullets(Game *g);
void ShootPlayerBullets(Game *g);
int  EnemiesBulletCollision(Game *g);
int  PlayerBulletCollision(Game *g);
void DrawCenteredText(char* str, int font_size, int x, int y, Color color);
void LoadAssets(Game *g);
void UnloadAssets(Game *g);

char  saves[5][32] = { 0 };
char  rank[5][32] = { 0 };
float stars[NUM_STARS][2];
int   rank_toggle = 0;

Animation anim_player_end   = { 0, 0 };
Animation anim_player_start = { 0, 0 };

int main() {
  Game g;

  srand(time(0));
  InitGame(&g);
  InitGameWindow(&g);

  while (!WindowShouldClose()) {
    UpdateMusicStream(g.music);

    if      (g.stage == START_SCREEN)      UpdateStartScreen(&g);
    else if (g.stage == DIFFICULTY_SCREEN) UpdateDifficultyScreen(&g);
    else if (g.stage == END_SCREEN)        UpdateEndScreen(&g);
    else                                   UpdateGame(&g);
  }

  FinishGameWindow(&g);
  return 0;
}

void InitGame(Game *g) {
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

  // ---

  g->stage = START_SCREEN;
  g->winner = 0;
  g->nick[0] = '\0';
  g->pts = 0;
  g->difficulty = NORMAL;

  g->borders[0] = (Rectangle) { 0, -10, WINDOW_WIDTH, 10 };           // Up
  g->borders[1] = (Rectangle) { 0, WINDOW_HEIGHT, WINDOW_WIDTH, 10 }; // Bottom
  g->borders[2] = (Rectangle) { -10, 0, 10, WINDOW_HEIGHT };          // Left
  g->borders[3] = (Rectangle) { WINDOW_WIDTH, 0, 10, WINDOW_HEIGHT }; // Right

  for (int i = 0; i < NUM_STARS; i++) {
    stars[i][0] = RAND(0, WINDOW_WIDTH);
    stars[i][1] = RAND(0, WINDOW_HEIGHT);
  }

  InitShips(g);
}

void InitGameWindow(Game *g) {
  InitAudioDevice();
  InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Space Invaders");
  SetTargetFPS(60);
  LoadAssets(g);
  g->music = LoadMusicStream("assets/soundtrack.mp3");
  SetMusicVolume(g->music, VOLUME / 5);
  SetMasterVolume(VOLUME);
  PlayMusicStream(g->music);
}

void InitShips(Game *g) {
  g->player.pos = (Rectangle) { WINDOW_WIDTH / 2.0 - STD_WIDTH / 2.0, WINDOW_HEIGHT - STD_HEIGHT - 10, STD_WIDTH, STD_HEIGHT };
  g->player.color = WHITE;
  g->player.speed = PLAYER_SPEED;
  g->player.bullet.active = 0;
  g->player.bullet.speed = 10;

  g->enemy.pos = (Rectangle) { 0, 15, STD_WIDTH, STD_HEIGHT };
  g->enemy.color = RED;
  g->enemy.speed = 3;
  g->enemy.direction = 1; // 0 - Left; 1 - Right
  g->enemy.bullet.active = 0;
  g->enemy.bullet.timer = GetTime();
  g->enemy.bullet.speed = 5;
}

void FinishGame(Game *g) {
  char new_save[32];
  sprintf(new_save, "%s %d\n", g->nick, g->pts);

  strcpy(saves[4], saves[3]);
  strcpy(saves[3], saves[2]);
  strcpy(saves[2], saves[1]);
  strcpy(saves[1], saves[0]);
  strcpy(saves[0], new_save);

  for (int i = 4; i >= 0; i--) {
    int _pts;
    sscanf(rank[i], "%*s %d", &_pts);

    if (!*rank[i] || g->pts > _pts) {
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

void FinishGameWindow(Game *g) {
  UnloadMusicStream(g->music);
  UnloadAssets(g);
  CloseWindow();
}

void UpdateGame(Game *g) {
  EnemiesMovement(g);
  PlayerMovement(g);
  ShootEnemiesBullets(g);
  ShootPlayerBullets(g);
  DrawGame(g);
}

void UpdateStartScreen(Game *g) {
  int remaining = NAME_SIZE - strlen(g->nick);
  int key = toupper(GetCharPressed());

  if (key > 31 && key < 126) {
    if (!remaining) PlaySound(g->assets.s_nop);
    else {
      g->nick[strlen(g->nick) + 1] = '\0';
      g->nick[strlen(g->nick)] = key;
      PlaySound(g->assets.s_key);
    }
  }

  if (IsKeyPressed(KEY_TAB)) {
    rank_toggle = !rank_toggle;
    PlaySound(g->assets.s_key);
  }

  if (IsKeyPressed(KEY_BACKSPACE)) {
    if (!strlen(g->nick)) PlaySound(g->assets.s_nop);
    else {
      g->nick[strlen(g->nick) - 1] = '\0';
      PlaySound(g->assets.s_undo);
    }
  }

  if (IsKeyPressed(KEY_ENTER)) {
    if (remaining) PlaySound(g->assets.s_nop);
    else g->stage = DIFFICULTY_SCREEN;
  }

  // Label Buffer
  char label_buf[64];
  char empty[remaining + 1];
  memset(empty, ' ', remaining);
  empty[remaining] = '\0';
  sprintf(label_buf, ">%sNICKNAME%s<", empty, empty);

  // Nickname Buffer
  char nick_buf[64];
  strcpy(nick_buf, g->nick);
  if (strlen(g->nick) < NAME_SIZE) strcat(nick_buf, "_");

  BeginDrawing();
  ClearBackground(BACKGROUND_COLOR);
  DrawStars(g);
  DrawCenteredText("SPACE INVADERS", 69, 0, 40, DARKBROWN);
  DrawCenteredText("SPACE INVADERS", 70, 0, 30, YELLOW);
  DrawCenteredText(label_buf, 40, 0, 170, remaining ? WHITE : PURPLE);
  if (!remaining) DrawCenteredText(nick_buf,  40, sin((GetTime() - 0.2) * 13) * 3, 220 + cos((GetTime() - 0.2) * 5) * 4, DARKPURPLE);
  DrawCenteredText(nick_buf,  40, sin(GetTime() * 13) * 3, 220 + cos(GetTime() * 5) * 4, remaining ? WHITE : PURPLE);
  for (int i = 0; i < 5; i++) DrawCenteredText(rank_toggle ? rank[i] : saves[i], 50, 0, 300 + 55 * i, PURPLE);
  EndDrawing();
}

void UpdateDifficultyScreen(Game *g) {
  static Difficulty selected = NORMAL;

  if (IsKeyPressed(264) || IsKeyPressed(83)) {
    if (selected == HARDCORE) PlaySound(g->assets.s_nop);
    else {
      selected++;
      PlaySound(g->assets.s_key);
    }
  }

  if (IsKeyPressed(265) || IsKeyPressed(87)) {
    if (selected == 0) PlaySound(g->assets.s_nop);
    else {
      selected--;
      PlaySound(g->assets.s_key);
    }
  }

  if (IsKeyPressed(KEY_ENTER)) {
    g->stage = GAME_SCREEN;
    g->difficulty = selected;
    anim_player_start.running = 1;
    anim_player_start.start = GetTime();
    PlaySound(g->assets.s_enter);
  }

  char _buffer[64], buffer[64];

  BeginDrawing();
  ClearBackground(BACKGROUND_COLOR);
  DrawStars(g);
  DrawCenteredText("SPACE INVADERS", 69, 0, 40, DARKBROWN);
  DrawCenteredText("SPACE INVADERS", 70, 0, 30, YELLOW);

  for (int i = 0; i <= HARDCORE; i++) {
    Color color, color_d;
    buffer[0]  = '\0';
    _buffer[0] = '\0';

    switch (i) {
      case NORMAL:
        strcat(_buffer, "NORMAL");
        color   = WHITE;
        color_d = GRAY;
        break;
      case HARD:
        strcat(_buffer, "HARD");
        color   = PURPLE;
        color_d = DARKPURPLE;
        break;
      case HARDCORE:
        strcat(_buffer, "HARDCORE");
        color   = RED;
        color_d = DARKBROWN;
        break;
    }

    sprintf(buffer, selected == i ? "> %s <" : "%s", _buffer);
    if (selected == i) {
      DrawCenteredText(buffer, 55, sin((GetTime() - 0.2) * 9 * (i + 1)) * 4, 200 + 100 * i + cos((GetTime() - 0.2) * 5 * (i + 1)) * 3, color_d);
      DrawCenteredText(buffer, 55, sin((GetTime())       * 9 * (i + 1)) * 4, 200 + 100 * i + cos((GetTime())       * 5 * (i + 1)) * 3, color);
    }
    else DrawCenteredText(buffer, selected == i ? 55 : 50, 0, 200 + 100 * i, color);
  }

  EndDrawing();
}

void UpdateEndScreen(Game *g) {
  if (IsKeyPressed(KEY_ENTER)) {
    InitGame(g);
    PlaySound(g->assets.s_enter);
    return;
  }

  if (anim_player_end.running) {
    float x = (GetTime() - anim_player_end.start) / 2;
    g->player.pos.y = WINDOW_HEIGHT - STD_HEIGHT - 10 - x * x * WINDOW_HEIGHT;
    if (x >= 1) anim_player_end.running = 0;
  }

  if (!g->winner) {
    EnemiesMovement(g);
  }

  char* message = g->winner ? "YOU WON" : "YOU DIED";
  Color color   = g->winner ? GREEN     : RED;
  Color color_d = g->winner ? DARKGREEN : DARKBROWN;

  BeginDrawing();
  if (g->winner) DrawPlayer(g);
  else           DrawEnemies(g);
  ClearBackground(BACKGROUND_COLOR);
  DrawStars(g);
  DrawCenteredText(message, 80, sin((GetTime() - 0.2) * 13) * 4, 250 + cos((GetTime() - 0.2) * 5) * 5, color_d);
  DrawCenteredText(message, 80, sin(GetTime() * 13) * 3, 250 + cos(GetTime() * 5) * 4, color);
  DrawCenteredText("- Hit Enter -", 40, 0, WINDOW_HEIGHT - 50, GRAY);
  EndDrawing();
}

void DrawGame(Game *g) {
  BeginDrawing();
  ClearBackground(BACKGROUND_COLOR);
  DrawStars(g);
  DrawBullets(g);
  DrawEnemies(g);
  DrawPlayer(g);
  EndDrawing();
}

void DrawEnemies(Game *g) {
  Vector2 frame_size = { 32, 32 };

  static int frame = 0;
  static int last_swap = 0;

  if (GetTime() - last_swap > 1) {
    frame = !frame;
    last_swap = GetTime();
  }

  Rectangle frame_rec = { frame * frame_size.x, 0, frame_size.x, frame_size.y };
  Rectangle pos_rec   = { g->enemy.pos.x, g->enemy.pos.y, 32, 32 };
  DrawTexturePro(g->assets.enemy, frame_rec, pos_rec, (Vector2) { 0, 0 }, 0, WHITE);
}

void DrawPlayer(Game *g) {
  int y = g->player.pos.y;

  if (anim_player_start.running) {
    float x = MIN(1, (GetTime() - anim_player_start.start));
    y += (x - 1) * (x - 1) * 50;
    if (x >= 1) anim_player_start.running = 0;
  }

  Rectangle frame_rec = { 0, 0, 32, 32 };
  Rectangle pos_rec   = { g->player.pos.x, y, 32, 32 };
  DrawTexturePro(g->assets.player, frame_rec, pos_rec, (Vector2) { 0, 0 }, 0, WHITE);
}

void DrawBullets(Game *g) {
  if (g->player.bullet.active) DrawRectangleRec(g->player.bullet.pos, PLAYER_BULLET_COLOR);
  if (g->enemy.bullet.active)  DrawRectangleRec(g->enemy.bullet.pos,  ENEMY_BULLET_COLOR);
}

void DrawStars(Game *g) {
  for (int i = 0; i < NUM_STARS; i++)
    DrawRectangle(stars[i][0], stars[i][1], STAR_SIZE, STAR_SIZE, WHITE);
  MoveStars(g);
}

void MoveStars(Game *g) {
  for (int i = 0; i < NUM_STARS; i++) {
    stars[i][1] += STAR_SPEED;
    if (stars[i][1] > WINDOW_HEIGHT)
      stars[i][1] = -3;
  }
}
void PlayerMovement(Game *g) {
  if ((IsKeyDown(262) || IsKeyDown(68)) && !CheckCollisionRecs(g->player.pos, g->borders[3])) g->player.pos.x += g->player.speed;
  if ((IsKeyDown(263) || IsKeyDown(65)) && !CheckCollisionRecs(g->player.pos, g->borders[2])) g->player.pos.x -= g->player.speed;
}

void EnemiesMovement(Game *g) {
  if      (CheckCollisionRecs(g->enemy.pos, g->borders[2])) g->enemy.direction = 1;
  else if (CheckCollisionRecs(g->enemy.pos, g->borders[3])) g->enemy.direction = 0;

  if (g->enemy.direction) g->enemy.pos.x += g->enemy.speed;
  else                    g->enemy.pos.x -= g->enemy.speed;
}

void ShootEnemiesBullets(Game *g) {
  if (!g->enemy.bullet.active && GetTime() - g->enemy.bullet.timer > 3) {
    g->enemy.bullet.pos = (Rectangle) { g->enemy.pos.x + g->enemy.pos.width/2, g->enemy.pos.y + g->enemy.pos.height / 2, BULLET_WIDTH, BULLET_HEIGHT };
    g->enemy.bullet.active = 1;
    g->enemy.bullet.timer = GetTime();
    PlaySound(g->enemy.bullet.sound);
  }

  if (g->enemy.bullet.active) {
    if (EnemiesBulletCollision(g)) g->enemy.bullet.active = 0;
    g->enemy.bullet.pos.y += g->enemy.bullet.speed;
  }
}

void ShootPlayerBullets(Game *g) {
  if (IsKeyDown(32) && !g->player.bullet.active) {
    g->player.bullet.pos = (Rectangle) { g->player.pos.x + g->player.pos.width / 2, g->player.pos.y + g->player.pos.height / 2, BULLET_WIDTH, BULLET_HEIGHT };
    g->player.bullet.active = 1;
    PlaySound(g->assets.s_shoot[RAND(0, 3)]);
  }

  if (g->player.bullet.active) {
    if (PlayerBulletCollision(g)) g->player.bullet.active = 0;
    g->player.bullet.pos.y -= g->player.bullet.speed;
  }
}

int EnemiesBulletCollision(Game *g) {
  if (CheckCollisionRecs(g->player.pos, g->enemy.bullet.pos)) {
    g->stage = END_SCREEN;
    PlaySound(g->assets.s_death);
    g->pts = 0;
    FinishGame(g);
    return 1;
  }

  if (CheckCollisionRecs(g->enemy.bullet.pos, g->borders[1]))
    return 1;

  return 0;
}

int PlayerBulletCollision(Game *g) {
  if (CheckCollisionRecs(g->enemy.pos, g->player.bullet.pos)) {
    g->winner = 1;
    anim_player_end.running = 1;
    anim_player_end.start = GetTime();
    g->stage = END_SCREEN;
    PlaySound(g->assets.s_hit);
    g->pts = 100 * (g->difficulty + 1);
    FinishGame(g);
    return 1;
  }

  if (CheckCollisionRecs(g->player.bullet.pos, g->borders[0]))
    return 1;

  return 0;
}

void DrawCenteredText(char* str, int font_size, int x, int y, Color color) {
  DrawText(str, WINDOW_WIDTH / 2 - MeasureText(str, font_size) / 2 + x, y, font_size, color);
}

void LoadAssets(Game *g) {
  g->assets.enemy       = LoadTexture("assets/enemy.png");
  g->assets.player      = LoadTexture("assets/player.png");
  g->assets.s_key       = LoadSound("assets/key.wav");
  g->assets.s_undo      = LoadSound("assets/undo.wav");
  g->assets.s_enter     = LoadSound("assets/enter.wav");
  g->assets.s_hit       = LoadSound("assets/hit.wav");
  g->assets.s_nop       = LoadSound("assets/nop.wav");
  g->assets.s_death     = LoadSound("assets/death.wav");
  g->assets.s_shoot[0]  = LoadSound("assets/shoot_1.wav");
  g->assets.s_shoot[1]  = LoadSound("assets/shoot_2.wav");
  g->assets.s_shoot[2]  = LoadSound("assets/shoot_3.wav");
  g->assets.s_shoot[3]  = LoadSound("assets/shoot_4.wav");
  g->enemy.bullet.sound = LoadSound("assets/shoot.wav");
}

void UnloadAssets(Game *g) {
  UnloadTexture(g->assets.player);
  UnloadTexture(g->assets.enemy);
  UnloadSound(g->assets.s_key);
  UnloadSound(g->assets.s_undo);
  UnloadSound(g->assets.s_enter);
  UnloadSound(g->assets.s_hit);
  UnloadSound(g->assets.s_nop);
  UnloadSound(g->assets.s_death);
  UnloadSound(g->assets.s_shoot[0]);
  UnloadSound(g->assets.s_shoot[1]);
  UnloadSound(g->assets.s_shoot[2]);
  UnloadSound(g->assets.s_shoot[3]);
  UnloadSound(g->enemy.bullet.sound);
}

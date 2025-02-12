#include "raylib.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

#define MIN(x, y) (x < y ? x : y)
#define MAX(x, y) (x > y ? x : y)
#define CLAMP(x, y, z) (MAX(MIN(z, y), x))
#define RAND(min, max) (random() % (max - min) + min)

#define WINDOW_WIDTH  800
#define WINDOW_HEIGHT 600
#define BULLET_WIDTH  10
#define BULLET_HEIGHT 15
#define SHIP_WIDTH    32
#define SHIP_HEIGHT   32
#define SAVE_PATH "./save.txt"
#define RANK_PATH "./rank.txt"
#define NAME_SIZE 3
#define VOLUME 0.5
#define NUM_STARS 50
#define STAR_SPEED 0.3
#define STAR_SIZE 2
#define BACKGROUND_COLOR (Color) { 10, 0, 10 }
#define PLAYER_BULLET_COLOR WHITE
#define ENEMY_BULLET_COLOR  GREEN
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

void  InitGame(Game *g);
void  InitGameWindow(Game *g);
void  WriteRank(Game *g);
void  UpdateGame(Game *g);
void  UpdateStartScreen(Game *g);
void  UpdateModeScreen(Game *g);
void  UpdateEndScreen(Game *g);
void  DrawGame(Game *g);
void  DrawEnemies(Game *g);
void  DrawPlayer(Game *g);
void  DrawBullets(Game *g);
void  DrawStars(Game *g);
void  MoveStars(Game *g);
void  PlayerMovement(Game *g);
void  EnemiesMovement(Game *g);
void  EnemyShoot(Game *g);
void  PlayerShoot(Game *g);
void  EnemiesBulletCollision(Game *g);
void  PlayerBulletCollision(Game *g);
int   StageInEvent();
void  SetStage(Game *g, Stage stage);
float Shake(float x, float speed, float intensity);
float TimeSince(float x);
void  DrawCenteredText(char* str, int font_size, int x, int y, Color color);
void  LoadAssets(Game *g);
void  UnloadAssets(Game *g);
void  StartAnimation(Animation* anim);
float AnimationKeyFrame(Animation* anim);
void  StartTransition(Stage to);
void  DrawTransition(Game *g);

// ---

char saves[5][16] = { 0 };
char  rank[5][16] = { 0 };
float stars[NUM_STARS][2];

Animation a_player_out = { 0, 0, 2 };
Animation a_player_inn = { 0, 0, 1 };
Animation transition = { 0, 0, 0.5 };
Stage transition_to;

Color background_color = BACKGROUND_COLOR;
float star_speed = STAR_SPEED;

int stage_in_event = 0;
int enemy_direction = 1;
int player_immune = 0;

// ---

int main() {
  Game g;

  srand((long) &g);
  InitGameWindow(&g);
  SetStage(&g, START_SCREEN);

  while (!WindowShouldClose()) {
    UpdateMusicStream(g.assets.music);

    if      (g.stage == START_SCREEN) UpdateStartScreen(&g);
    else if (g.stage == MODE_SCREEN)  UpdateModeScreen(&g);
    else if (g.stage == END_SCREEN)   UpdateEndScreen(&g);
    else if (g.stage == GAME_SCREEN)  UpdateGame(&g);
  }

  UnloadAssets(&g);
  CloseWindow();
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

  background_color = BACKGROUND_COLOR;
  star_speed = STAR_SPEED;
  g->nick[0] = '\0';
  g->winner = 0;
  g->pts = 0;

  g->borders[0] = (Rectangle) { 0, -10, WINDOW_WIDTH, 10 };           // Up
  g->borders[1] = (Rectangle) { 0, WINDOW_HEIGHT, WINDOW_WIDTH, 10 }; // Bottom
  g->borders[2] = (Rectangle) { -10, 0, 10, WINDOW_HEIGHT };          // Left
  g->borders[3] = (Rectangle) { WINDOW_WIDTH, 0, 10, WINDOW_HEIGHT }; // Right

  for (int i = 0; i < NUM_STARS; i++) {
    stars[i][0] = RAND(0, WINDOW_WIDTH);
    stars[i][1] = RAND(0, WINDOW_HEIGHT);
  }

  g->player.pos = (Rectangle) { WINDOW_WIDTH / 2.0 - SHIP_WIDTH / 2.0, WINDOW_HEIGHT - SHIP_HEIGHT - 10, SHIP_WIDTH, SHIP_HEIGHT };
  g->player.bullet = (Rectangle) { 0, 0, BULLET_WIDTH, BULLET_HEIGHT };
  g->player.speed = PLAYER_SPEED;
  g->player.shooting = 0;
  g->player.bullet_speed = 10;
  player_immune = 0;

  g->enemy.pos    = (Rectangle) { 0, 15, SHIP_WIDTH, SHIP_HEIGHT };
  g->enemy.bullet = (Rectangle) { 0, 0, BULLET_WIDTH, BULLET_HEIGHT };
  g->enemy.speed = 3;
  g->enemy.shooting = 0;
  g->enemy.bullet_speed = 5;
  enemy_direction = 1;
}

void InitGameWindow(Game *g) {
  InitAudioDevice();
  InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Space Invaders");
  SetTargetFPS(60);
  LoadAssets(g);
  SetMusicVolume(g->assets.music, VOLUME * 0.3);
  SetMasterVolume(VOLUME);
  PlayMusicStream(g->assets.music);
}

void WriteRank(Game *g) {
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

void UpdateGame(Game *g) {
  if (StageInEvent()) {
    StartAnimation(&a_player_inn);
    g->enemy.last_shoot = GetTime();

    // Adjusts based on mode
    g->player.hp = g->mode == NORMAL ? 3 : g->mode == HARD ? 2 : 1;
    background_color.r += g->mode * 3;
    g->enemy.speed *= 1 + g->mode * 0.3;
    g->enemy.bullet_speed *= 1 + g->mode * 0.5;
    g->enemy.shoot_timer = g->mode == NORMAL ? 3 : g->mode == HARD ? 1 : 0.1;
    star_speed *= 1 + g->mode;
  }

  if (player_immune) player_immune--;

  EnemiesMovement(g);
  PlayerMovement(g);
  EnemyShoot(g);
  PlayerShoot(g);
  DrawGame(g);
}

void UpdateStartScreen(Game *g) {
  if (StageInEvent()) InitGame(g);

  static int rank_toggle = 0;
  int remaining = NAME_SIZE - strlen(g->nick);
  int key = toupper(GetCharPressed());

  if (key >= 65 && key <= 90 || key >= 97 && key <= 122) {
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
    else StartTransition(MODE_SCREEN);
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
  ClearBackground(background_color);
  DrawStars(g);
  DrawCenteredText("SPACE INVADERS", 69, 0, 40, DARKBROWN);
  DrawCenteredText("SPACE INVADERS", 70, 0, 30, YELLOW);
  DrawCenteredText(label_buf, 40, 0, 170, remaining ? WHITE : PURPLE);
  if (!remaining) DrawCenteredText(nick_buf,  40, Shake(-0.2, 13, 3), 220 + Shake(-0.2, 5, 4), DARKPURPLE);
  DrawCenteredText(nick_buf,  40, Shake(0, 13, 3), 220 + Shake(0, 5, 4), remaining ? WHITE : PURPLE);
  for (int i = 0; i < 5; i++) DrawCenteredText(rank_toggle ? rank[i] : saves[i], 50, 0, 300 + 55 * i, PURPLE);
  DrawTransition(g);
  EndDrawing();
}

void UpdateModeScreen(Game *g) {
  static Mode selected = NORMAL;

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
    StartTransition(GAME_SCREEN);
    g->mode = selected;
    PlaySound(g->assets.s_enter);
  }

  char _buffer[32], buffer[32];

  BeginDrawing();
  ClearBackground(background_color);
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
      DrawCenteredText(buffer, 55, Shake(-0.2, 9 * (i + 1), 4), 250 + 100 * i + Shake(-0.2, 5 * (i + 1), 3), color_d);
      DrawCenteredText(buffer, 55, Shake(0,    9 * (i + 1), 4), 250 + 100 * i + Shake(0,    5 * (i + 1), 3), color);
    }
    else DrawCenteredText(buffer, 50, 0, 250 + 100 * i, color);
  }

  DrawTransition(g);
  EndDrawing();
}

void UpdateEndScreen(Game *g) {
  if (StageInEvent()) WriteRank(g);

  if (IsKeyPressed(KEY_ENTER)) {
    StartTransition(START_SCREEN);
    PlaySound(g->assets.s_enter);
  }

  if (a_player_out.running) {
    float x = AnimationKeyFrame(&a_player_out);
    g->player.pos.y = WINDOW_HEIGHT - SHIP_HEIGHT - 10 - x / a_player_out.duration * x / a_player_out.duration * WINDOW_HEIGHT;
  }

  if (!g->winner) EnemiesMovement(g);

  char* message = g->winner ? "YOU WON" : "YOU DIED";
  Color color   = g->winner ? GREEN     : RED;
  Color color_d = g->winner ? DARKGREEN : DARKBROWN;

  BeginDrawing();
  ClearBackground(background_color);
  DrawStars(g);
  DrawCenteredText(message, 80, Shake(-0.2, 13, 4), 250 + Shake(-0.2, 5, 5), color_d);
  DrawCenteredText(message, 80, Shake(0,    13, 4), 250 + Shake(0,    5, 5), color);
  DrawCenteredText("- Hit Enter -", 40, 0, WINDOW_HEIGHT - 50, GRAY);
  if (g->winner) DrawPlayer(g);
  else           DrawEnemies(g);
  DrawTransition(g);
  EndDrawing();
}

void DrawGame(Game *g) {
  BeginDrawing();
  ClearBackground(background_color);
  DrawStars(g);
  DrawBullets(g);
  DrawEnemies(g);
  DrawPlayer(g);
  DrawTransition(g);
  EndDrawing();
}

void DrawEnemies(Game *g) {
  Vector2 frame_size = { 32, 32 };

  static int frame = 0;
  static int last_swap = 0;

  if (TimeSince(last_swap) > 1) {
    frame = !frame;
    last_swap = GetTime();
  }

  Rectangle frame_rec = { frame * frame_size.x, 0, frame_size.x, frame_size.y };
  Rectangle pos_rec   = { g->enemy.pos.x, g->enemy.pos.y, 32, 32 };
  DrawTexturePro(g->assets.enemy, frame_rec, pos_rec, (Vector2) { 0, 0 }, 0, WHITE);
}

void DrawPlayer(Game *g) {
  int y = g->player.pos.y;

  if (a_player_inn.running) {
    float x = AnimationKeyFrame(&a_player_inn);
    y += pow(x - 1, 2) * 50;
  }

  Rectangle frame_rec = { 0, 0, 32, 32 };
  Rectangle pos_rec   = { g->player.pos.x, y, 32, 32 };
  Color color = { 255, 255, 255, player_immune ? 127 : 255 };
  DrawTexturePro(g->assets.player, frame_rec, pos_rec, (Vector2) { 0, 0 }, 0, color);
}

void DrawBullets(Game *g) {
  if (g->player.shooting) DrawRectangleRec(g->player.bullet, PLAYER_BULLET_COLOR);
  if (g->enemy.shooting)  DrawRectangleRec(g->enemy.bullet,  ENEMY_BULLET_COLOR);
}

void DrawStars(Game *g) {
  for (int i = 0; i < NUM_STARS; i++)
    DrawRectangle(stars[i][0], stars[i][1], STAR_SIZE, STAR_SIZE, WHITE);
  MoveStars(g);
}

void MoveStars(Game *g) {
  for (int i = 0; i < NUM_STARS; i++) {
    stars[i][1] += star_speed;
    if (stars[i][1] > WINDOW_HEIGHT)
      stars[i][1] = -STAR_SIZE;
  }
}

void PlayerMovement(Game *g) {
  if ((IsKeyDown(262) || IsKeyDown(68)) && !CheckCollisionRecs(g->player.pos, g->borders[3])) g->player.pos.x += g->player.speed;
  if ((IsKeyDown(263) || IsKeyDown(65)) && !CheckCollisionRecs(g->player.pos, g->borders[2])) g->player.pos.x -= g->player.speed;
}

void EnemiesMovement(Game *g) {
  if      (CheckCollisionRecs(g->enemy.pos, g->borders[2])) enemy_direction =  1;
  else if (CheckCollisionRecs(g->enemy.pos, g->borders[3])) enemy_direction = -1;

  g->enemy.pos.x += g->enemy.speed * enemy_direction;
}

void EnemyShoot(Game *g) {
  if (g->enemy.shooting) {
    EnemiesBulletCollision(g);
    g->enemy.bullet.y += g->enemy.bullet_speed;
    return;
  }

  if (TimeSince(g->enemy.last_shoot) < g->enemy.shoot_timer) return;
  g->enemy.bullet.x = g->enemy.pos.x + g->enemy.pos.width  / 2;
  g->enemy.bullet.y = g->enemy.pos.y + g->enemy.pos.height / 2;
  g->enemy.shooting = 1;
  g->enemy.last_shoot = GetTime();
  PlaySound(g->assets.s_e_shoot);
}

void PlayerShoot(Game *g) {
  if (g->player.shooting) {
    PlayerBulletCollision(g);
    g->player.bullet.y -= g->player.bullet_speed;
    return;
  }

  if (!IsKeyDown(32)) return;
  g->player.bullet.x = g->player.pos.x + g->player.pos.width  / 2;
  g->player.bullet.y = g->player.pos.y + g->player.pos.height / 2;
  g->player.shooting = 1;
  PlaySound(g->assets.s_shoot[RAND(0, 3)]);
}

void EnemiesBulletCollision(Game *g) {
  if (!player_immune && CheckCollisionRecs(g->player.pos, g->enemy.bullet)) {
    g->enemy.shooting = 0;
    player_immune = 30;
    background_color.r += 3;
    if (--g->player.hp)
      PlaySound(g->assets.s_hit);
    else {
      SetStage(g, END_SCREEN);
      PlaySound(g->assets.s_death);
      g->pts = 0;
    }
  }

  if (CheckCollisionRecs(g->enemy.bullet, g->borders[1]))
    g->enemy.shooting = 0;
}

void PlayerBulletCollision(Game *g) {
  if (CheckCollisionRecs(g->enemy.pos, g->player.bullet)) {
    g->winner = 1;
    StartAnimation(&a_player_out);
    SetStage(g, END_SCREEN);
    PlaySound(g->assets.s_hit);
    g->pts = 100 * (g->mode + 1);
    g->player.shooting = 0;
  }

  if (CheckCollisionRecs(g->player.bullet, g->borders[0]))
    g->player.shooting = 0;
}

// Stage

int  StageInEvent() {
  int tmp = stage_in_event;
  if (!tmp) stage_in_event = 1;
  return !tmp;
}

void SetStage(Game *g, Stage stage) {
  g->stage = stage;
  stage_in_event = 0;
}

// Utils

float Shake(float offset, float speed, float intensity) {
  return sin((GetTime() + offset) * speed) * intensity;
}

float TimeSince(float x) {
  return GetTime() - x;
}

void DrawCenteredText(char* str, int font_size, int x, int y, Color color) {
  DrawText(str, WINDOW_WIDTH / 2 - MeasureText(str, font_size) / 2 + x, y, font_size, color);
}

// Assets

void LoadAssets(Game *g) {
  g->assets.music      = LoadMusicStream("assets/soundtrack.mp3");
  g->assets.enemy      = LoadTexture("assets/enemy.png");
  g->assets.player     = LoadTexture("assets/player.png");
  g->assets.s_key      = LoadSound("assets/key.wav");
  g->assets.s_undo     = LoadSound("assets/undo.wav");
  g->assets.s_enter    = LoadSound("assets/enter.wav");
  g->assets.s_hit      = LoadSound("assets/hit.wav");
  g->assets.s_nop      = LoadSound("assets/nop.wav");
  g->assets.s_death    = LoadSound("assets/death.wav");
  g->assets.s_shoot[0] = LoadSound("assets/shoot_1.wav");
  g->assets.s_shoot[1] = LoadSound("assets/shoot_2.wav");
  g->assets.s_shoot[2] = LoadSound("assets/shoot_3.wav");
  g->assets.s_shoot[3] = LoadSound("assets/shoot_4.wav");
  g->assets.s_e_shoot  = LoadSound("assets/shoot.wav");
}

void UnloadAssets(Game *g) {
  UnloadMusicStream(g->assets.music);
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
  UnloadSound(g->assets.s_e_shoot);
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
}

void DrawTransition(Game *g) {
  if (!transition.running) return;
  float x = AnimationKeyFrame(&transition);
  if (x >= transition.duration / 2 && g->stage != transition_to) SetStage(g, transition_to);
  DrawRectangle(-WINDOW_WIDTH + x / transition.duration * 2 * WINDOW_WIDTH, 0, WINDOW_WIDTH, WINDOW_HEIGHT, BLACK);
}

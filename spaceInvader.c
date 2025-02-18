#include "raylib.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

#define CIRCULAR_CLAMP(x, y, z) ((y < x) ? z : ((y > z) ? x : y))
#define MIN(x, y) (x < y ? x : y)
#define MAX(x, y) (x < y ? y : x)
#define LEN(x)    (sizeof(x) / sizeof(x[0]))

#define WINDOW_WIDTH  800
#define WINDOW_HEIGHT 600
#define BULLET_WIDTH  10
#define BULLET_HEIGHT 10
#define SHIP_WIDTH    32
#define SHIP_HEIGHT   32

#define SAVE_PATH "./save.txt"
#define RANK_PATH "./rank.txt"
#define NAME_SIZE 3

#define NUM_STARS  60
#define STAR_SPEED 0.25
#define BACKGROUND_COLOR (Color) { 10, 0, 10 }
#define DAMAGE_REDNESS 6
#define VOLUME 0.5

#define ALL_ENEMIES_SHOOT 0
#define SPICY_MODE        1
#define SHOW_HP           1

// ---

typedef enum {
  START_SCREEN, MODE_SCREEN, GAME_SCREEN, END_SCREEN
} Stage;

typedef enum {
  NORMAL, HARD, HARDCORE
} Mode;

typedef enum {
  T_LTR, T_RTL, T_BTT, T_TTB
} TransitionType;

typedef struct {
  Rectangle pos, bullet;
  int hp, shooting;
  float last_shoot, shoot_timer, speed, bullet_speed;
} Ship;

typedef struct {
  Rectangle pos;
  int max_hp, hp;
} Barrier;

typedef struct {
  Texture2D player, enemy, barrier[4];
  Sound s_key, s_undo, s_enter, s_hit;
  Sound s_nop, s_death, s_shoot[4], s_e_shoot;
  Sound s_damage, s_shield, s_break;
  Music music;
} Assets;

typedef struct {
  Mode mode;
  Rectangle borders[4];
  Ship player, enemies[12][6];
  Barrier barriers[4];
  Stage stage;
  char nick[3];
  int winner, pts, timer, level, enemy_columns, enemy_lines;
} Game;

typedef struct {
  int running;
  float start, duration;
} Animation;

// ---

void  InitGame();
void  ReadRank();
void  WriteRank();
void  StageStart();
void  StageMode();
void  StageEnd();
void  StageGame();
void  DrawHUD();
void  DrawEnemies();
void  DrawPlayer();
void  DrawBullets();
void  DrawStars();
void  DrawBarriers();
void  GenerateMap();
void  PlayerMovement();
void  EnemiesMovement();
void  EnemyShoot();
void  PlayerShoot();
void  EnemiesBulletCollision();
void  PlayerBulletCollision();
void  TakeDamage();
void  WinGame();
void  LoseGame();
int   StageInEvent();
void  SetStage(Stage stage);
void  LoadAssets();
void  UnloadAssets();
void  StartAnimation(Animation* anim);
float AnimationKeyFrame(Animation* anim);
void  StartTransition(Stage to, TransitionType type);
void  DrawTransition();
float Shake(float x, float speed, float intensity);
float TimeSince(float x);
void  DrawCenteredText(char* str, int size, int x, int y, Color color);

// --- Variáveis Globais

Game g = { .nick = "\0" };
Assets assets;

char saves[5][16] = { 0 };
char  rank[5][16] = { 0 };

Animation a_player_out = { 0, 0, 2 };
Animation a_player_inn = { 0, 0, 1 };
Animation transition = { 0, 0, 0.5 };
TransitionType transition_type;
Stage transition_to;
int transitioned = 0;

Color background_color;
float stars[NUM_STARS][2];
float star_speed;

int stage_in_event;
int enemy_direction;
int player_immune;

// ---

int main() {
  SetRandomSeed((long) &g);
  InitGame();
  SetStage(START_SCREEN);

  while (!WindowShouldClose()) {
    UpdateMusicStream(assets.music);

    BeginDrawing();
    ClearBackground(background_color);
    DrawStars();
    // Onde os estados do jogo são loopados até o usuário sair
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

// ---

// Inicializar o jogo; Roda apenas uma vez
void InitGame() {
  InitAudioDevice();
  InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Space Invaders");
  SetTargetFPS(60);
  LoadAssets();
  SetMusicVolume(assets.music, VOLUME * 0.3);
  SetSoundVolume(assets.s_e_shoot, 0.2);
  SetMasterVolume(VOLUME);
  PlayMusicStream(assets.music);

  g.borders[0] = (Rectangle) { 0, -10, WINDOW_WIDTH, 10 };           // Up
  g.borders[1] = (Rectangle) { 0, WINDOW_HEIGHT, WINDOW_WIDTH, 10 }; // Bottom
  g.borders[2] = (Rectangle) { -10, 0, 10, WINDOW_HEIGHT };          // Left
  g.borders[3] = (Rectangle) { WINDOW_WIDTH, 0, 10, WINDOW_HEIGHT }; // Right

  g.player.speed  = 5;
  g.player.bullet_speed = 15;

  for (int i = 0; i < NUM_STARS; i++) {
    stars[i][0] = GetRandomValue(0, WINDOW_WIDTH);
    stars[i][1] = GetRandomValue(0, WINDOW_HEIGHT);
  }
}

// Le os arquivos de pontuacao
void ReadRank() {
  // Cria o arquivo se ele nao existir
  FILE* f_save = fopen(SAVE_PATH, "a");
  FILE* f_rank = fopen(RANK_PATH, "a");

  // Le o arquivo
  f_save = fopen(SAVE_PATH, "r");
  f_rank = fopen(RANK_PATH, "r");

  int i = 0;
  while (fgets(saves[i], 32, f_save) && i < 5) i++;
  i = 0;
  while (fgets(rank[i], 32, f_rank) && i < 5) i++;

  fclose(f_save);
  fclose(f_rank);
}

// Escreve a pontuacao atual nos arquivos
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

// --- Funcoes de cada tela que rodam todo frame

// Menu
void StageStart() {
  // "if (StageInEvent())" cria um bloco de código que só executa no primeiro frame do estágio
  if (StageInEvent()) {
    // Inicializa valores toda vez que se entra no menu principal
    background_color = BACKGROUND_COLOR;
    star_speed = STAR_SPEED;
    g.winner = 0;
    g.pts = 0;
    g.level = 0;
    g.player.pos = (Rectangle) { WINDOW_WIDTH / 2.0 - SHIP_WIDTH / 2.0, WINDOW_HEIGHT - SHIP_HEIGHT - 10, SHIP_WIDTH, SHIP_HEIGHT };
    ReadRank();
  }

  int remaining = NAME_SIZE - strlen(g.nick);

  int key = toupper(GetCharPressed());
  if (key >= 65 && key <= 90) {
    if (!remaining) PlaySound(assets.s_nop);
    else {
      g.nick[strlen(g.nick) + 1] = '\0';
      g.nick[strlen(g.nick)] = key;
      PlaySound(assets.s_key);
    }
  }

  // Alterna em mostrar o ranking ou os jogos anteriores
  static int rank_toggle = 0;
  if (IsKeyPressed(KEY_TAB)) {
    rank_toggle = !rank_toggle;
    PlaySound(assets.s_key);
  }

  if (IsKeyPressed(KEY_BACKSPACE)) {
    if (!strlen(g.nick)) PlaySound(assets.s_nop);
    else {
      g.nick[strlen(g.nick) - 1] = '\0';
      PlaySound(assets.s_undo);
    }
  }

  if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
    if (remaining) PlaySound(assets.s_nop);
    else {
      StartTransition(MODE_SCREEN, T_LTR);
      PlaySound(assets.s_enter);
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

  DrawCenteredText(SPICY_MODE ? "SPICY INVADERS" : "SPACE INVADERS", 69, 0, 40, DARKBROWN);
  DrawCenteredText(SPICY_MODE ? "SPICY INVADERS" : "SPACE INVADERS", 70, 0, 30, YELLOW);
  DrawCenteredText(label_buf, 40, 0, 170, remaining ? WHITE : PURPLE);
  if (!remaining) DrawCenteredText(nick_buf,  40, Shake(-0.2, 13, 3), 220 + Shake(-0.2, 5, 4), DARKPURPLE);
  DrawCenteredText(nick_buf,  40, Shake(0, 13, 3), 220 + Shake(0, 5, 4), remaining ? WHITE : PURPLE);
  for (int i = 0; i < 5; i++) DrawCenteredText(rank_toggle ? rank[i] : saves[i], 50, 0, 300 + 55 * i, PURPLE);
}

// Selecao de modo
void StageMode() {
  static Mode selected = NORMAL;

  if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) {
    if (selected == HARDCORE) PlaySound(assets.s_nop);
    else {
      selected++;
      PlaySound(assets.s_key);
    }
  }

  if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP)) {
    if (!selected) PlaySound(assets.s_nop);
    else {
      selected--;
      PlaySound(assets.s_key);
    }
  }

  if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
    StartTransition(GAME_SCREEN, T_BTT);
    g.mode = selected;
    PlaySound(assets.s_enter);
  }

  DrawCenteredText(SPICY_MODE ? "SPICY INVADERS" : "SPACE INVADERS", 69, 0, 40, DARKBROWN);
  DrawCenteredText(SPICY_MODE ? "SPICY INVADERS" : "SPACE INVADERS", 70, 0, 30, YELLOW);

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

// Tela Final
void StageEnd() {
  if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
    StartTransition(g.winner ? GAME_SCREEN : START_SCREEN, g.winner ? T_BTT : T_RTL);
    PlaySound(assets.s_enter);
  }

  if (!g.winner) EnemiesMovement();

  if (a_player_out.running) {
    float x = AnimationKeyFrame(&a_player_out);
    g.player.pos.y = WINDOW_HEIGHT - SHIP_HEIGHT - 10 - pow(x / a_player_out.duration, 2) * WINDOW_HEIGHT;
  }

  char* message = g.winner ? "YOU WON" : "YOU DIED";
  Color color   = g.winner ? GREEN     : RED;
  Color color_d = g.winner ? DARKGREEN : DARKBROWN;

  if (!g.winner) DrawEnemies();
  DrawCenteredText(message, 80, Shake(-0.2, 13, 4), 250 + Shake(-0.2, 5, 5), color_d);
  DrawCenteredText(message, 80, Shake(0,    13, 4), 250 + Shake(0,    5, 5), color);
   DrawCenteredText("- Hit Enter -", 38, 0, WINDOW_HEIGHT - 50, GRAY);
  if (g.winner) DrawPlayer();
}

// Jogo
void StageGame() {
  // Bloco que roda no inicio de cada round
  if (StageInEvent()) {
    g.level++;
    g.player.pos.y  = WINDOW_HEIGHT - SHIP_HEIGHT - 10;
    g.player.bullet = (Rectangle) { 0, 0, BULLET_WIDTH, BULLET_HEIGHT };
    g.player.shooting = 0;
    player_immune = 0;
    enemy_direction = 1;
    a_player_out.running = 0;
    StartAnimation(&a_player_inn);
    g.player.hp = 3 - g.mode;

    GenerateMap();
  }
  if ((int)(g.timer - GetTime()) <= 0) LoseGame(); // Faz o player perder o jogo caso alcance o tempo limite
  if (IsKeyPressed(KEY_F2)) WinGame();             // Atalho pro jogador ganhar caso aperte F2
  if (IsKeyPressed(KEY_F3)) LoseGame();            // Atalho pro jogador perder caso aperte F3

  // Decrementa os frames de imunidade se o player estiver imune
  if (player_immune) player_immune--;

  EnemiesMovement();
  PlayerMovement();
  EnemyShoot();
  PlayerShoot();
  DrawBullets();
  DrawEnemies();
  DrawPlayer();
  DrawBarriers();
  DrawHUD();
}

// --- Funcoes responsaveis por desenhar o jogo

void DrawHUD() {
  char buffer[32];
  if (SHOW_HP) {
    sprintf(buffer, "%d HP", g.player.hp);
    DrawText(buffer, 10, 10, 30, WHITE);
  }

  sprintf(buffer, "%d", (int) (g.timer - GetTime()));
  DrawText(buffer, WINDOW_WIDTH - MeasureText(buffer, 30) - 10, 10, 30, WHITE);
}

void DrawEnemies() {
  Vector2 frame_size = { 32, 32 };

  static int frame = 0;
  static int last_swap = 0;

  if (TimeSince(last_swap) > 1) {
    frame = !frame;
    last_swap = GetTime();
  }

  Rectangle frame_rec = { frame * frame_size.x, 0, frame_size.x, frame_size.y };

  for (int i = 0; i < g.enemy_columns; i++) {
    for (int j = 0; j < g.enemy_lines; j++) {
      // So desenha se o inimigo estiver vivo
      if(g.enemies[i][j].hp) {
        Rectangle pos_rec   = { g.enemies[i][j].pos.x, g.enemies[i][j].pos.y, 32, 32 };
        DrawTexturePro(assets.enemy, frame_rec, pos_rec, (Vector2) { 0, 0 }, 0, WHITE);
      }
    }
  }
}

void DrawPlayer() {
  Rectangle frame_rec = { 0, 0, 32, 32 };
  Rectangle pos_rec   = { g.player.pos.x, g.player.pos.y, 32, 32 };

  // Animacao do player entrar na tela
  if (a_player_inn.running) {
    float x = AnimationKeyFrame(&a_player_inn);
    pos_rec.y += pow(x - 1, 2) * 50;
  }

  Color color = { 255, 255, 255, player_immune ? 127 : 255 };
  DrawTexturePro(assets.player, frame_rec, pos_rec, (Vector2) { 0, 0 }, 0, color);
}

void DrawBullets() {
  if (g.player.shooting) DrawRectangleRec(g.player.bullet, PURPLE);

  for (int i = 0; i < g.enemy_columns; i++)
    for (int j = 0; j < g.enemy_lines; j++)
      if (g.enemies[i][j].shooting)
        DrawRectangleRec(g.enemies[i][j].bullet,  GREEN);
}

void DrawStars() {
  // Desenha e move as estrelas
  for (int i = 0; i < NUM_STARS; i++) {
    stars[i][1] = CIRCULAR_CLAMP(-2, stars[i][1] + star_speed, WINDOW_HEIGHT);
    DrawRectangle(stars[i][0], stars[i][1], 2, 2, WHITE);
  }
}

void DrawBarriers() {
  Rectangle frame_rec = { 0, 0, 32, 32 };
  for (int i = 0; i < LEN(g.barriers); i++) {
    if (!g.barriers[i].hp) continue;
    Rectangle pos_rec = { g.barriers[i].pos.x, g.barriers[i].pos.y, 32, 32 };
    int spr_i = 4 - (float) g.barriers[i].hp / g.barriers[i].max_hp * 4;
    DrawTexturePro(assets.barrier[spr_i], frame_rec, pos_rec, (Vector2) { 0, 0 }, 0, WHITE);
  }
}

// --- Funcoes do jogo

void GenerateMap() {
  background_color = BACKGROUND_COLOR;
  background_color.r = g.mode * DAMAGE_REDNESS;
  star_speed = STAR_SPEED + ((g.level-1)/4.0) * (g.mode + 1);
  g.timer = MAX(80, GetTime() + 120 - (g.level * 10));
  g.enemy_columns = MIN(7 + ((g.level - 1) / 2), LEN(g.enemies));
  g.enemy_lines = MIN(3 + (g.level < 5 ? 0 : (((g.level - 6) / 2))), LEN(g.enemies[0]));

  // Inicializa a matriz dos inimigos
  for (int i = 0; i < LEN(g.enemies); i++) {
    for (int j = 0; j < LEN(g.enemies[0]); j++) {
      g.enemies[i][j].hp = 1;
      if (i >= g.enemy_columns || j >= g.enemy_lines) g.enemies[i][j].hp = 0;
      g.enemies[i][j].shooting = 0;
      g.enemies[i][j].last_shoot = GetTime() - GetRandomValue(0, 4);
      g.enemies[i][j].pos = (Rectangle) { i * 60, 15 + j * 60, SHIP_WIDTH, SHIP_HEIGHT };
      g.enemies[i][j].bullet = (Rectangle) { 0, 0, BULLET_WIDTH, BULLET_HEIGHT };
      g.enemies[i][j].bullet_speed = g.mode == NORMAL ? 5 : g.mode == HARD ? 6   : 7;
      g.enemies[i][j].bullet_speed *= 1 + MIN(0.2, g.level/10.0);
      g.enemies[i][j].shoot_timer  = g.mode == NORMAL ? 4 : g.mode == HARD ? 3   : 2;
      g.enemies[i][j].shoot_timer *= 1 - MIN(0.2, g.level/10.0);
      g.enemies[i][j].speed        = g.mode == NORMAL ? 3 : g.mode == HARD ? 4.5 : 6;
      g.enemies[i][j].speed *= 1 + MIN(0.2, g.level/10.0);
    }
  }

  // Inicializa as barreiras
  for (int i = 0; i < LEN(g.barriers); i++) {
    g.barriers[i].max_hp = g.mode == NORMAL ? 5 : g.mode == HARD ? 8   : 10;
    g.barriers[i].hp = g.barriers[i].max_hp;
    g.barriers[i].pos = (Rectangle) { (i + 1) * ((float) WINDOW_WIDTH / ((int)LEN(g.barriers) + 1)), 400, SHIP_WIDTH, SHIP_HEIGHT };
  }
}

void PlayerMovement() {
  if ((IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) && !CheckCollisionRecs(g.player.pos, g.borders[3])) g.player.pos.x += g.player.speed;
  if ((IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  && !CheckCollisionRecs(g.player.pos, g.borders[2])) g.player.pos.x -= g.player.speed;
}

void EnemiesMovement() {
  for (int i = 0; i < g.enemy_columns; i++) {
    for (int j = 0; j < g.enemy_lines; j++) {
      if      (CheckCollisionRecs(g.enemies[i][j].pos, g.borders[2])) enemy_direction =  1;
      else if (CheckCollisionRecs(g.enemies[i][j].pos, g.borders[3])) enemy_direction = -1;
    }
  }

  for (int i = 0; i < g.enemy_columns; i++)
    for (int j = 0; j < g.enemy_lines; j++)
      g.enemies[i][j].pos.x += g.enemies[i][j].speed * enemy_direction;
}

void EnemyShoot() {
  for (int i = 0; i < g.enemy_columns; i++) {
    for (int j = 0; j < g.enemy_lines; j++) {
      if (g.enemies[i][j].shooting) {
        EnemiesBulletCollision();
        g.enemies[i][j].bullet.y += g.enemies[i][j].bullet_speed;
        continue;
      }

      // Atira se o inimigo estiver vivo e no tempo
      if (TimeSince(g.enemies[i][j].last_shoot) < g.enemies[i][j].shoot_timer || !g.enemies[i][j].hp || (g.enemies[i][j+1].hp && j+1 < LEN(g.enemies[0]))) continue;
      g.enemies[i][j].bullet.x = g.enemies[i][j].pos.x + g.enemies[i][j].pos.width  / 2;
      g.enemies[i][j].bullet.y = g.enemies[i][j].pos.y + g.enemies[i][j].pos.height / 2;
      g.enemies[i][j].shooting = 1;
      g.enemies[i][j].last_shoot = GetTime();
      PlaySound(assets.s_e_shoot);
    }
  }
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
  PlaySound(assets.s_shoot[GetRandomValue(0, 3)]);
}

void EnemiesBulletCollision() {
  // Loopa todos inimigos
  for (int i = 0; i < g.enemy_columns; i++) {
    for (int j = 0; j < g.enemy_lines; j++) {
      if (!g.enemies[i][j].shooting) continue;
      // Colisao com o player
      if (CheckCollisionRecs(g.player.pos, g.enemies[i][j].bullet)) {
        g.enemies[i][j].shooting = 0;
        TakeDamage();
      }

      // Colisao com alguma barreira
      for (int a = 0; a < LEN(g.barriers); a++) {
        if (CheckCollisionRecs(g.barriers[a].pos,g.enemies[i][j].bullet) && g.barriers[a].hp) {
          g.barriers[a].hp -= 1;
          PlaySound(g.barriers[a].hp ? assets.s_shield : assets.s_break);
          g.enemies[i][j].shooting = 0;
        }
      }

      // Colisao com a borda
      if (CheckCollisionRecs(g.enemies[i][j].bullet, g.borders[1]))
        g.enemies[i][j].shooting = 0;
    }
  }
}

void PlayerBulletCollision() {
  // Loopa todos inimigos
  for (int i = 0; i < g.enemy_columns; i++) {
    for (int j = 0; j < g.enemy_lines; j++) {
      // Colisao com inimigo
      if (CheckCollisionRecs(g.enemies[i][j].pos, g.player.bullet) && g.enemies[i][j]. hp) {
        g.enemies[i][j].hp = 0;
        g.player.shooting = 0;
        PlaySound(assets.s_hit);

        for (int a = 0; a < g.enemy_columns; a++)
          for (int c = 0; c < g.enemy_lines; c++)
            if (g.enemies[a][c].hp) return;

        WinGame();
      }

      // Colisao com alguma barreira
      for (int k = 0; k < LEN(g.barriers); k++)
        if (CheckCollisionRecs(g.barriers[k].pos, g.player.bullet) && g.barriers[k].hp)
          g.player.shooting = 0;

      // Colisao com a borda
      if (CheckCollisionRecs(g.player.bullet, g.borders[0]))
        g.player.shooting = 0;
    }
  }
}

// Reduz o HP e finaliza a partida se chegar em 0
void TakeDamage() {
  if (player_immune) return;
  PlaySound(assets.s_damage);
  player_immune = 30;
  background_color.r += DAMAGE_REDNESS;
  if (--g.player.hp) PlaySound(assets.s_hit);
  else LoseGame();
}

// Finaliza o round com vitoria
void WinGame() {
  StartAnimation(&a_player_out);
  SetStage(END_SCREEN);
  PlaySound(assets.s_hit);
  g.winner = 1;
  g.pts += 100 * (g.mode + 1);
  g.player.shooting = 0;
}

// Finaliza o round com derrota
void LoseGame() {
  SetStage(END_SCREEN);
  PlaySound(assets.s_death);
  g.winner = 0;
  WriteRank();
}

// Funcao pra checar se e o primeiro frame e desligar a flag
int StageInEvent() {
  int tmp = stage_in_event;
  stage_in_event = 1;
  return !tmp;
}

// Entra em um estagio e desativa a flag de primeiro frame
void SetStage(Stage stage) {
  g.stage = stage;
  stage_in_event = 0;
}

// --- Assets

void LoadAssets() {
  assets.music      = LoadMusicStream("assets/soundtrack.mp3");
  assets.enemy      = LoadTexture("assets/enemy.png");
  assets.player     = LoadTexture("assets/player.png");
  assets.barrier[0] = LoadTexture("assets/barrier0.png");
  assets.barrier[1] = LoadTexture("assets/barrier1.png");
  assets.barrier[2] = LoadTexture("assets/barrier2.png");
  assets.barrier[3] = LoadTexture("assets/barrier3.png");
  assets.s_key      = LoadSound("assets/key.wav");
  assets.s_undo     = LoadSound("assets/undo.wav");
  assets.s_enter    = LoadSound("assets/enter.wav");
  assets.s_hit      = LoadSound("assets/hit.wav");
  assets.s_nop      = LoadSound("assets/nop.wav");
  assets.s_death    = LoadSound("assets/death.wav");
  assets.s_shoot[0] = LoadSound("assets/shoot_1.wav");
  assets.s_shoot[1] = LoadSound("assets/shoot_2.wav");
  assets.s_shoot[2] = LoadSound("assets/shoot_3.wav");
  assets.s_shoot[3] = LoadSound("assets/shoot_4.wav");
  assets.s_e_shoot  = LoadSound("assets/shoot.wav");
  assets.s_damage  = LoadSound("assets/damage.wav");
  assets.s_shield  = LoadSound("assets/shield.wav");
  assets.s_break  = LoadSound("assets/break.wav");
}

void UnloadAssets() {
  UnloadMusicStream(assets.music);
  UnloadTexture(assets.player);
  UnloadTexture(assets.enemy);
  UnloadTexture(assets.barrier[0]);
  UnloadTexture(assets.barrier[1]);
  UnloadTexture(assets.barrier[2]);
  UnloadTexture(assets.barrier[3]);
  UnloadSound(assets.s_key);
  UnloadSound(assets.s_undo);
  UnloadSound(assets.s_enter);
  UnloadSound(assets.s_hit);
  UnloadSound(assets.s_nop);
  UnloadSound(assets.s_death);
  UnloadSound(assets.s_shoot[0]);
  UnloadSound(assets.s_shoot[1]);
  UnloadSound(assets.s_shoot[2]);
  UnloadSound(assets.s_shoot[3]);
  UnloadSound(assets.s_e_shoot);
  UnloadSound(assets.s_damage);
  UnloadSound(assets.s_shield);
  UnloadSound(assets.s_break);
}

// --- Animacoes

void StartAnimation(Animation* anim) {
  anim->start   = GetTime();
  anim->running = 1;
}

float AnimationKeyFrame(Animation* anim) {
  float x = (TimeSince(anim->start));
  if (x >= anim->duration) anim->running = 0;
  return x;
}

// Transicoes

void StartTransition(Stage to, TransitionType type) {
  StartAnimation(&transition);
  transition_type = type;
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

  if (transition_type == T_LTR)
    DrawRectangle(-WINDOW_WIDTH + x / transition.duration * 2 * WINDOW_WIDTH, 0, WINDOW_WIDTH, WINDOW_HEIGHT, BLACK);
  if (transition_type == T_RTL)
    DrawRectangle(+WINDOW_WIDTH - x / transition.duration * 2 * WINDOW_WIDTH, 0, WINDOW_WIDTH, WINDOW_HEIGHT, BLACK);
  if (transition_type == T_TTB)
    DrawRectangle(0, -WINDOW_HEIGHT + x / transition.duration * 2 * WINDOW_HEIGHT, WINDOW_WIDTH, WINDOW_HEIGHT, BLACK);
  if (transition_type == T_BTT)
    DrawRectangle(0, +WINDOW_HEIGHT - x / transition.duration * 2 * WINDOW_HEIGHT, WINDOW_WIDTH, WINDOW_HEIGHT, BLACK);
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

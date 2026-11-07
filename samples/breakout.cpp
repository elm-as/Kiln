#include "kiln.h"
#include "synth.h"
#include <cstdio>

const float W = 480, H = 270;
const float HUD_H = 20;

enum { COLS = 12, ROWS = 6, MAX_PARTICLES = 512 };
const float BRICK_W = 36, BRICK_H = 10, BRICK_GAP = 2;
const float PADDLE_Y = H - 20, PADDLE_H = 6, PADDLE_SPEED = 280;
const float BALL = 6;

struct Brick {
    Rect r;
    int hp;
    Color color;
};

struct Particle {
    vec2 pos, vel;
    float life;
    Color color;
};

enum State { SERVE, PLAY, GAME_OVER };

static Brick bricks[COLS * ROWS];
static int bricks_left;
static Particle particles[MAX_PARTICLES];
static int particle_count;

static float paddle_x, paddle_prev_x, paddle_w;
static vec2 ball, ball_prev, ball_vel;
static float ball_speed;
static float last_mouse_x = -1;
static float shake;

static State state;
static bool paused;
static int score, best, lives, level;

static Font font, big_font;
static Texture ball_tex;
static Sound snd_paddle, snd_brick, snd_hard, snd_wall, snd_lost, snd_level;

// same little square wave blips as before, through the synth now
static Sound blip(float f0, float f1, float seconds, float volume = 0.25f)
{
    SfxParams p;
    p.freq = f0;
    p.freq_end = f1;
    p.attack = 0.003f;
    p.sustain = 0;
    p.decay = seconds;
    p.volume = volume;
    auto s = std::make_shared<SoundData>();
    s->rate = SYNTH_RATE;
    s->samples = sfx_render(p);
    return s;
}

static const uint32_t row_colors[ROWS] = { 0xe0474c, 0xf08c3a, 0xf2c94c, 0x6fcf6a, 0x4aa3df, 0x9b6ae0 };

static const char ball_pixels[] =
    ".####."
    "######"
    "######"
    "######"
    "######"
    ".####.";

static void build_level()
{
    float left = (W - (COLS * BRICK_W + (COLS - 1) * BRICK_GAP)) / 2;
    bricks_left = 0;
    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
            Brick& b = bricks[y * COLS + x];
            b.r = { left + x * (BRICK_W + BRICK_GAP), 32 + y * (BRICK_H + BRICK_GAP), BRICK_W, BRICK_H };
            b.color = rgb(row_colors[y]);
            b.hp = y < level ? 2 : 1;
            // every other level gets holes so they don't all look the same
            if (level % 2 == 0 && (x + y) % 5 == 0)
                b.hp = 0;
            if (b.hp > 0)
                bricks_left++;
        }
    }
}

static void serve()
{
    state = SERVE;
    ball_speed = 160.f + level * 15;
}

static void new_game()
{
    score = 0;
    lives = 3;
    level = 1;
    paddle_w = 48;
    paddle_x = paddle_prev_x = (W - paddle_w) / 2;
    particle_count = 0;
    paused = false;
    build_level();
    serve();
}

static void launch()
{
    float a = randf(-0.4f, 0.4f);
    ball_vel = { sinf(a) * ball_speed, -cosf(a) * ball_speed };
    state = PLAY;
}

static void burst(Rect r, Color c, int n)
{
    for (int i = 0; i < n && particle_count < MAX_PARTICLES; i++) {
        Particle& p = particles[particle_count++];
        p.pos = { r.x + randf(0, r.w), r.y + randf(0, r.h) };
        p.vel = { randf(-70, 70), randf(-90, 10) };
        p.life = randf(0.4f, 0.9f);
        p.color = c;
    }
}

static void hit_brick(Brick& b)
{
    b.hp--;
    if (b.hp > 0) {
        burst(b.r, WHITE, 4);
        audio_play(snd_hard);
        return;
    }
    bricks_left--;
    score += 10 * level;
    if (score > best)
        best = score;
    ball_speed = fminf(ball_speed + 3, 360);
    burst(b.r, b.color, 14);
    shake = 0.08f;
    audio_play(snd_brick);
}

static Brick* touching_brick()
{
    Rect br = { ball.x, ball.y, BALL, BALL };
    for (Brick& b : bricks)
        if (b.hp > 0 && overlaps(br, b.r))
            return &b;
    return 0;
}

// x and y are resolved one after the other. crude, but the ball never moves
// more than a few pixels per tick so it doesn't tunnel through anything
static void move_ball(float dt)
{
    ball.x += ball_vel.x * dt;
    if (ball.x < 0) {
        ball.x = 0;
        ball_vel.x = fabsf(ball_vel.x);
        audio_play(snd_wall);
    } else if (ball.x + BALL > W) {
        ball.x = W - BALL;
        ball_vel.x = -fabsf(ball_vel.x);
        audio_play(snd_wall);
    }
    if (Brick* b = touching_brick()) {
        ball.x = ball_vel.x > 0 ? b->r.x - BALL : b->r.x + b->r.w;
        ball_vel.x = -ball_vel.x;
        hit_brick(*b);
    }

    ball.y += ball_vel.y * dt;
    if (ball.y < HUD_H) {
        ball.y = HUD_H;
        ball_vel.y = fabsf(ball_vel.y);
        audio_play(snd_wall);
    }
    if (Brick* b = touching_brick()) {
        ball.y = ball_vel.y > 0 ? b->r.y - BALL : b->r.y + b->r.h;
        ball_vel.y = -ball_vel.y;
        hit_brick(*b);
    }

    Rect paddle = { paddle_x, PADDLE_Y, paddle_w, PADDLE_H };
    if (ball_vel.y > 0 && overlaps({ ball.x, ball.y, BALL, BALL }, paddle)) {
        // -1 on the left edge, 1 on the right. up to ~60 degrees off vertical
        float t = (ball.x + BALL / 2 - (paddle_x + paddle_w / 2)) / (paddle_w / 2);
        float a = clampf(t, -1, 1) * 1.05f;
        ball_vel = { sinf(a) * ball_speed, -cosf(a) * ball_speed };
        ball.y = PADDLE_Y - BALL;
        audio_play(snd_paddle);
    }

    if (ball.y > H) {
        lives--;
        shake = 0.3f;
        audio_play(snd_lost);
        if (lives > 0)
            serve();
        else
            state = GAME_OVER;
    }
}

static void update_particles(float dt)
{
    for (int i = 0; i < particle_count;) {
        Particle& p = particles[i];
        p.life -= dt;
        if (p.life <= 0) {
            p = particles[--particle_count];
            continue;
        }
        p.vel.y += 300 * dt;
        p.pos += p.vel * dt;
        i++;
    }
}

static void init()
{
    font = font_create("Consolas", 12);
    big_font = font_create("Consolas", 28);

    uint8_t px[6 * 6 * 4];
    for (int i = 0; i < 36; i++) {
        uint8_t a = ball_pixels[i] == '#' ? 255 : 0;
        px[i * 4 + 0] = px[i * 4 + 1] = px[i * 4 + 2] = 255;
        px[i * 4 + 3] = a;
    }
    ball_tex = texture_create(6, 6, px);

    snd_paddle = blip(520, 480, 0.05f);
    snd_wall   = blip(300, 300, 0.03f, 0.15f);
    snd_brick  = blip(780, 1100, 0.06f);
    snd_hard   = blip(220, 200, 0.05f);
    snd_lost   = blip(400, 90, 0.45f);
    snd_level  = blip(500, 1500, 0.35f);

    new_game();
}

static void update(const Input& in, float dt)
{
    if (in.pressed[KEY_ESCAPE]) {
        engine_quit();
        return;
    }

    if (state == GAME_OVER) {
        update_particles(dt);
        if (in.pressed[KEY_SPACE] || in.pressed[KEY_ENTER] || in.mouse_pressed[MOUSE_LEFT])
            new_game();
        return;
    }

    if (in.pressed[KEY_P])
        paused = !paused;
    if (paused)
        return;

    paddle_prev_x = paddle_x;
    ball_prev = ball;

    float dir = 0;
    if (in.down[KEY_LEFT] || in.down[KEY_A]) dir -= 1;
    if (in.down[KEY_RIGHT] || in.down[KEY_D]) dir += 1;
    paddle_x += dir * PADDLE_SPEED * dt;

    // the mouse takes over whenever it moves inside the playfield
    if (in.mouse_x != last_mouse_x && contains({ 0, 0, W, H }, { in.mouse_x, in.mouse_y })) {
        paddle_x = in.mouse_x - paddle_w / 2;
        last_mouse_x = in.mouse_x;
    }
    paddle_x = clampf(paddle_x, 0, W - paddle_w);

    if (state == SERVE) {
        ball = { paddle_x + paddle_w / 2 - BALL / 2, PADDLE_Y - BALL };
        ball_prev = ball;
        if (in.pressed[KEY_SPACE] || in.mouse_pressed[MOUSE_LEFT])
            launch();
    } else {
        move_ball(dt);
        if (bricks_left == 0) {
            level++;
            audio_play(snd_level);
            build_level();
            serve();
        }
    }

    update_particles(dt);
    shake = fmaxf(0, shake - dt);
}

static void text_centered(const Font& f, const char* s, float y, Color c)
{
    draw_text(f, s, floorf((W - text_width(f, s)) / 2), y, c);
}

static void draw(float alpha)
{
    if (paused)
        alpha = 1;

    vec2 off = {};
    if (shake > 0)
        off = { randf(-2, 2), randf(-2, 2) };
    render_set_view(off);

    draw_rect({ 0, HUD_H - 2, W, 1 }, rgb(0x2a2f45));

    for (const Brick& b : bricks) {
        if (b.hp <= 0)
            continue;
        draw_rect(b.r, b.color);
        draw_rect({ b.r.x, b.r.y, b.r.w, 2 }, rgb(0xffffff, 70));
        if (b.hp > 1)
            draw_rect_lines(b.r, 1, rgb(0xffffff, 200));
    }

    float px = lerp(paddle_prev_x, paddle_x, alpha);
    draw_rect({ px, PADDLE_Y, paddle_w, PADDLE_H }, rgb(0xe8e8f0));
    draw_rect({ px, PADDLE_Y + PADDLE_H - 2, paddle_w, 2 }, rgb(0x9aa0b8));

    if (state != GAME_OVER) {
        vec2 bp = { lerp(ball_prev.x, ball.x, alpha), lerp(ball_prev.y, ball.y, alpha) };
        draw_sprite(ball_tex, {}, { bp.x, bp.y, BALL, BALL });
    }

    for (int i = 0; i < particle_count; i++) {
        const Particle& p = particles[i];
        draw_rect({ p.pos.x, p.pos.y, 2, 2 }, fade(p.color, p.life * 2));
    }

    render_set_view({ 0, 0 });

    char buf[64];
    snprintf(buf, sizeof(buf), "SCORE %d", score);
    draw_text(font, buf, 6, 2);
    snprintf(buf, sizeof(buf), "LEVEL %d", level);
    text_centered(font, buf, 2, rgb(0x9aa0b8));
    for (int i = 0; i < lives; i++)
        draw_rect({ W - 20 - i * 16, 7, 12, 4 }, rgb(0xe8e8f0));

    bool blink = fmod(platform_time(), 1.0) < 0.6;

    if (state == SERVE && !paused && blink)
        text_centered(font, "SPACE OR CLICK TO LAUNCH", H * 0.62f, WHITE);

    if (paused) {
        draw_rect({ 0, 0, W, H }, rgb(0x000000, 140));
        text_centered(big_font, "PAUSED", H / 2 - 20, WHITE);
        text_centered(font, "P TO RESUME", H / 2 + 14, rgb(0x9aa0b8));
    }

    if (state == GAME_OVER) {
        draw_rect({ 0, 0, W, H }, rgb(0x000000, 160));
        text_centered(big_font, "GAME OVER", H / 2 - 36, rgb(0xe0474c));
        snprintf(buf, sizeof(buf), "SCORE %d   BEST %d", score, best);
        text_centered(font, buf, H / 2, WHITE);
        if (blink)
            text_centered(font, "SPACE TO PLAY AGAIN", H / 2 + 24, rgb(0x9aa0b8));
    }
}

static void shutdown()
{
    audio_stop_all();
    font_free(&font);
    font_free(&big_font);
    texture_free(&ball_tex);
}

int main()
{
    GameDesc g = {};
    g.title = "Breakout";
    g.width = (int)W;
    g.height = (int)H;
    g.scale = 3;
    g.tick_rate = 120;
    g.clear = rgb(0x14161f);
    g.init = init;
    g.update = update;
    g.draw = draw;
    g.shutdown = shutdown;
    return engine_run(g);
}

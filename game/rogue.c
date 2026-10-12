/* rogue.c - a very small Rogue-like for a bare PDP-11 (no operating
 * system), on the console DL11 and an ANSI terminal.
 *
 * Nine rooms joined by corridors on each level.  Find the stairs (>) to go
 * down; on level 5 the Amulet (&) is in place of the stairs, and taking it
 * wins.  Monsters are letters; walk into one to fight it.  Gold (*) and
 * potions (!) lie about.
 *
 * Keys: h j k l y u b n (or the arrow keys) move, > goes down the stairs,
 * . or s rests a turn, q quits.  At the title, a digit 1-9 picks a fixed
 * dungeon (the same one every time); any other key a random one.
 *
 * Everything is turn-based: the game waits for a key, moves the
 * monsters, and redraws only what changed.  It needs no clock or
 * interrupts, which the emulators don't have. */
#include "console.h"

#define ROWS 24
#define COLS 80
#define MAP_TOP 1                 /* row 0 is for messages */
#define MAP_H 21                  /* map rows 1-21 */
#define STATUS_ROW 23
#define GRID_W 26                 /* rooms sit in a 3x3 grid of cells */
#define GRID_H 7
#define NROOMS 9
#define MAX_MONSTERS 12
#define LAST_LEVEL 5

/* terrain */
#define ROCK ' '
#define FLOOR '.'
#define HWALL '-'
#define VWALL '|'
#define DOOR '+'
#define PASSAGE '#'
#define STAIRS '>'
/* items */
#define GOLD '*'
#define POTION '!'
#define AMULET '&'

static char map[MAP_H][COLS];      /* terrain */
static char items[MAP_H][COLS];    /* an item, or 0 */
static char seen[MAP_H][COLS];     /* has the player seen it */
static char roomof[MAP_H][COLS];   /* room number + 1 for room cells, else 0 */
static char screen[ROWS][COLS];    /* what the terminal shows now */
static char want[MAP_H][COLS];     /* what the map part should show */
static int cur_row = -1, cur_col = -1;  /* where the terminal's cursor is */

struct room {
    int x, y, w, h;               /* the floor; walls are one cell outside */
};
static struct room rooms[NROOMS];

struct kind {
    char ch;
    const char *name;
    int hp, damage, xp, first_level;
};
static const struct kind kinds[] = {
    {'b', "bat", 3, 2, 1, 1},
    {'k', "kobold", 4, 3, 1, 1},
    {'j', "jackal", 4, 2, 1, 1},
    {'S', "snake", 6, 4, 2, 2},
    {'o', "orc", 9, 6, 3, 2},
    {'Z', "zombie", 13, 6, 4, 3},
    {'T', "troll", 20, 9, 7, 4},
    {'D', "dragon", 30, 12, 12, 5},
};
#define NKINDS (int)(sizeof kinds / sizeof kinds[0])

struct monster {
    int x, y, hp;
    const struct kind *kind;      /* 0: this slot is empty */
};
static struct monster monsters[MAX_MONSTERS];

static struct {
    int x, y, hp, max_hp, level, xp, gold, depth, turns;
} you;

static char message[COLS + 1];
static int message_len;
static int playing;
static int won;

/* ------------------------------------------------------------ random */

static unsigned rng_state = 1;

/* xorshift on 16 bits (shifts 7, 9, 8): every nonzero state, period 65535 */
static unsigned rng(void)
{
    /* the masks matter only where unsigned is wider than 16 bits */
    rng_state = (rng_state ^ rng_state << 7) & 0177777;
    rng_state ^= rng_state >> 9;
    rng_state = (rng_state ^ rng_state << 8) & 0177777;
    return rng_state;
}

/* 0 .. n-1 */
static int rnd(int n)
{
    return n > 0 ? (int)(rng() % (unsigned)n) : 0;
}

static int sign(int v) { return (v > 0) - (v < 0); }
static int absv(int v) { return v < 0 ? -v : v; }
static int max(int a, int b) { return a > b ? a : b; }

/* ---------------------------------------------------------- messages */

/* Adds to the message line, with a space between words but not before
 * punctuation. */
static void say(const char *s)
{
    if (message_len > 0 && message_len < COLS - 1 && *s != '.' && *s != '!')
        message[message_len++] = ' ';
    while (*s && message_len < COLS - 1)
        message[message_len++] = *s++;
    message[message_len] = 0;
}

static void say_num(int n)
{
    char digits[6];
    int i = 0;
    if (n < 0) {
        say("-");
        n = -n;
    }
    do {
        digits[i++] = '0' + n % 10;
        n /= 10;
    } while (n);
    while (i && message_len < COLS - 1)
        message[message_len++] = digits[--i];
    message[message_len] = 0;
}

/* "the orc" */
static void say_the(const struct kind *k)
{
    say("the");
    say(k->name);
}

/* ---------------------------------------------------------- the level */

static int in_map(int x, int y)
{
    return x >= 0 && x < COLS && y >= 0 && y < MAP_H;
}

static int walkable(int x, int y)
{
    char c;
    if (!in_map(x, y))
        return 0;
    c = map[y][x];
    return c == FLOOR || c == PASSAGE || c == DOOR || c == STAIRS;
}

static struct monster *monster_at(int x, int y)
{
    int i;
    for (i = 0; i < MAX_MONSTERS; i++)
        if (monsters[i].kind && monsters[i].x == x && monsters[i].y == y)
            return &monsters[i];
    return 0;
}

static void dig(int x, int y)
{
    if (map[y][x] == ROCK)
        map[y][x] = PASSAGE;
}

/* A corridor from a door in the right wall of room a to a door in the
 * left wall of room b, turning once in the gap between them. */
static void join_across(struct room *a, struct room *b)
{
    int ax = a->x + a->w, ay = a->y + rnd(a->h);
    int bx = b->x - 1, by = b->y + rnd(b->h);
    int mx = ax + 1 + rnd(bx - ax - 1), x, y;
    map[ay][ax] = DOOR;
    map[by][bx] = DOOR;
    for (x = ax + 1; x <= mx; x++)
        dig(x, ay);
    for (y = ay; y != by; y += sign(by - ay))
        dig(mx, y);
    for (x = mx; x < bx; x++)
        dig(x, by);
}

/* A corridor from a door in the bottom wall of room a to a door in the
 * top wall of room b. */
static void join_down(struct room *a, struct room *b)
{
    int ax = a->x + rnd(a->w), ay = a->y + a->h;
    int bx = b->x + rnd(b->w), by = b->y - 1;
    int my = ay + 1 + rnd(by - ay - 1), x, y;
    map[ay][ax] = DOOR;
    map[by][bx] = DOOR;
    for (y = ay + 1; y <= my; y++)
        dig(ax, y);
    for (x = ax; x != bx; x += sign(bx - ax))
        dig(x, my);
    for (y = my; y < by; y++)
        dig(bx, y);
}

/* A random empty floor cell in room r. */
static void spot_in(struct room *r, int *x, int *y)
{
    int tries = 0;
    do {
        *x = r->x + rnd(r->w);
        *y = r->y + rnd(r->h);
    } while ((map[*y][*x] != FLOOR || items[*y][*x] || monster_at(*x, *y) ||
              (*x == you.x && *y == you.y)) && ++tries < 100);
}

static void make_level(void)
{
    int i, x, y, cx, cy, n;
    struct room *r;

    memset(map, ROCK, sizeof map);
    memset(items, 0, sizeof items);
    memset(seen, 0, sizeof seen);
    memset(roomof, 0, sizeof roomof);
    memset(monsters, 0, sizeof monsters);

    /* Each room's walls stay one column and one row inside its grid cell,
     * so the corridors have at least two cells of rock to run through. */
    for (i = 0; i < NROOMS; i++) {
        r = &rooms[i];
        cx = i % 3 * GRID_W;
        cy = i / 3 * GRID_H;
        r->w = 4 + rnd(16);
        r->h = 2 + rnd(2);
        r->x = cx + 2 + rnd(GRID_W - 3 - r->w);
        r->y = cy + 2 + rnd(GRID_H - 3 - r->h);
        for (y = r->y - 1; y <= r->y + r->h; y++)
            for (x = r->x - 1; x <= r->x + r->w; x++) {
                if (y == r->y - 1 || y == r->y + r->h)
                    map[y][x] = HWALL;
                else if (x == r->x - 1 || x == r->x + r->w)
                    map[y][x] = VWALL;
                else
                    map[y][x] = FLOOR;
                roomof[y][x] = i + 1;
            }
    }
    for (i = 0; i < NROOMS; i++) {
        if (i % 3 < 2)
            join_across(&rooms[i], &rooms[i + 1]);
        if (i < 6)
            join_down(&rooms[i], &rooms[i + 3]);
    }

    /* you in one room, the way down in another */
    i = rnd(NROOMS);
    you.x = -1;
    spot_in(&rooms[i], &you.x, &you.y);
    n = (i + 1 + rnd(NROOMS - 1)) % NROOMS;
    spot_in(&rooms[n], &x, &y);
    if (you.depth == LAST_LEVEL)
        items[y][x] = AMULET;
    else
        map[y][x] = STAIRS;

    for (i = 0; i < 4 + rnd(4); i++) {
        spot_in(&rooms[rnd(NROOMS)], &x, &y);
        items[y][x] = rnd(3) ? GOLD : POTION;
    }

    n = 2 + you.depth + rnd(3);
    for (i = 0; i < n && i < MAX_MONSTERS; i++) {
        const struct kind *k;
        do
            k = &kinds[rnd(NKINDS)];
        while (k->first_level > you.depth);
        r = &rooms[rnd(NROOMS)];
        if (roomof[you.y][you.x] == r - rooms + 1)
            continue;   /* not in the starting room */
        spot_in(r, &x, &y);
        monsters[i].x = x;
        monsters[i].y = y;
        monsters[i].hp = k->hp + rnd(you.depth * 2);
        monsters[i].kind = k;
    }
}

/* ------------------------------------------------------------ seeing */

/* The room the player stands in, lit; else just the cells around. */
static void look(void)
{
    int x, y, r = roomof[you.y][you.x];
    if (r) {
        struct room *rm = &rooms[r - 1];
        for (y = rm->y - 1; y <= rm->y + rm->h; y++)
            for (x = rm->x - 1; x <= rm->x + rm->w; x++)
                seen[y][x] = 1;
    }
    for (y = you.y - 1; y <= you.y + 1; y++)
        for (x = you.x - 1; x <= you.x + 1; x++)
            if (in_map(x, y))
                seen[y][x] = 1;
}

static int can_see(int x, int y)
{
    int r = roomof[you.y][you.x];
    return (r && roomof[y][x] == r) || (absv(x - you.x) <= 1 && absv(y - you.y) <= 1);
}

/* ----------------------------------------------------------- drawing */

static void put_at(int row, int col, char c)
{
    if (screen[row][col] == c)
        return;
    if (row != cur_row || col != cur_col)
        gotoxy(row, col);
    putch(c);
    screen[row][col] = c;
    cur_row = row;
    cur_col = col + 1;
}

static void put_text(int row, const char *s)
{
    int col = 0;
    while (*s && col < COLS - 1)
        put_at(row, col++, *s++);
    while (col < COLS - 1)
        put_at(row, col++, ' ');
}

/* Appends text or a number to a line being built. */
static int add_text(char *line, int at, const char *s)
{
    while (*s && at < COLS - 1)
        line[at++] = *s++;
    line[at] = 0;
    return at;
}

static int add_num(char *line, int at, int n)
{
    char digits[6];
    int i = 0;
    do {
        digits[i++] = '0' + n % 10;
        n /= 10;
    } while (n);
    while (i && at < COLS - 1)
        line[at++] = digits[--i];
    line[at] = 0;
    return at;
}

/* Builds the map as it should look, then sends only the cells that
 * differ from what the terminal shows. */
static void draw(void)
{
    int x, y, i;
    char line[COLS + 1];
    int at = 0;

    for (y = 0; y < MAP_H; y++)
        for (x = 0; x < COLS - 1; x++)
            want[y][x] = !seen[y][x] ? ' ' : items[y][x] ? items[y][x] : map[y][x];
    for (i = 0; i < MAX_MONSTERS; i++)
        if (monsters[i].kind && can_see(monsters[i].x, monsters[i].y))
            want[monsters[i].y][monsters[i].x] = monsters[i].kind->ch;
    want[you.y][you.x] = '@';
    for (y = 0; y < MAP_H; y++)
        for (x = 0; x < COLS - 1; x++)
            put_at(MAP_TOP + y, x, want[y][x]);
    put_text(0, message);
    at = add_text(line, at, "Level:");
    at = add_num(line, at, you.depth);
    at = add_text(line, at, "  Gold:");
    at = add_num(line, at, you.gold);
    at = add_text(line, at, "  Hp:");
    at = add_num(line, at, you.hp);
    at = add_text(line, at, "(");
    at = add_num(line, at, you.max_hp);
    at = add_text(line, at, ")  Exp:");
    at = add_num(line, at, you.level);
    at = add_text(line, at, "/");
    add_num(line, at, you.xp);
    put_text(STATUS_ROW, line);
    gotoxy(MAP_TOP + you.y, you.x);
    cur_row = MAP_TOP + you.y;
    cur_col = you.x;
}

/* Clears the terminal and the record of what it shows. */
static void fresh_screen(void)
{
    clear_screen();
    memset(screen, ' ', sizeof screen);
    cur_row = 0;
    cur_col = 0;
}

/* ----------------------------------------------------------- fighting */

static void gain_xp(int xp)
{
    you.xp += xp;
    while (you.xp >= 6 * you.level * you.level) {
        you.level++;
        you.max_hp += 4;
        you.hp += 4;
        say("Welcome to level");
        say_num(you.level);
        say(".");
    }
}

static void attack(struct monster *m)
{
    if (rnd(10) < 2) {
        say("You miss");
        say_the(m->kind);
        say(".");
        return;
    }
    m->hp -= 1 + rnd(5 + you.level * 2);
    if (m->hp <= 0) {
        say("You defeat");
        say_the(m->kind);
        say(".");
        gain_xp(m->kind->xp);
        m->kind = 0;
    } else {
        say("You hit");
        say_the(m->kind);
        say(".");
    }
}

static void monster_attacks(struct monster *m)
{
    if (rnd(10) < 3) {
        say("The");
        say(m->kind->name);
        say("misses.");
        return;
    }
    you.hp -= 1 + rnd(m->kind->damage);
    say("The");
    say(m->kind->name);
    say("hits.");
    if (you.hp <= 0) {
        you.hp = 0;
        playing = 0;
    }
}

/* ---------------------------------------------------------- moving */

static int free_cell(int x, int y)
{
    return walkable(x, y) && !monster_at(x, y) && !(x == you.x && y == you.y);
}

static void move_monsters(void)
{
    int i;
    for (i = 0; i < MAX_MONSTERS && playing; i++) {
        struct monster *m = &monsters[i];
        int dx, dy;
        if (!m->kind)
            continue;
        dx = you.x - m->x;
        dy = you.y - m->y;
        if (absv(dx) <= 1 && absv(dy) <= 1) {
            monster_attacks(m);
            continue;
        }
        /* chase what it can see; bats flutter about */
        if (m->kind->ch == 'b' ? rnd(2) == 0 : !can_see(m->x, m->y) && max(absv(dx), absv(dy)) > 6) {
            if (rnd(3))
                continue;
            dx = rnd(3) - 1;
            dy = rnd(3) - 1;
        } else {
            dx = sign(dx);
            dy = sign(dy);
        }
        if (free_cell(m->x + dx, m->y + dy)) {
            m->x += dx;
            m->y += dy;
        } else if (dx && free_cell(m->x + dx, m->y)) {
            m->x += dx;
        } else if (dy && free_cell(m->x, m->y + dy)) {
            m->y += dy;
        }
    }
}

static void pick_up(void)
{
    char *it = &items[you.y][you.x];
    if (*it == GOLD) {
        int g = 2 + rnd(10 * you.depth + 10);
        you.gold += g;
        say("You find");
        say_num(g);
        say("gold pieces.");
    } else if (*it == POTION) {
        if (you.hp == you.max_hp)
            you.max_hp++;
        you.hp = you.max_hp;
        say("You drink a potion and feel better.");
    } else if (*it == AMULET) {
        say("You have the Amulet of Yendor!");
        won = 1;
        playing = 0;
    }
    *it = 0;
}

/* One player action; returns nonzero if it took a turn. */
static int act(int key)
{
    int dx = 0, dy = 0, x, y;
    struct monster *m;

    switch (key) {
    case 'h': dx = -1; break;
    case 'l': dx = 1; break;
    case 'k': dy = -1; break;
    case 'j': dy = 1; break;
    case 'y': dx = -1; dy = -1; break;
    case 'u': dx = 1; dy = -1; break;
    case 'b': dx = -1; dy = 1; break;
    case 'n': dx = 1; dy = 1; break;
    case '.':
    case 's':
        if (you.hp < you.max_hp && rnd(3) == 0)
            you.hp++;
        return 1;
    case '>':
        if (map[you.y][you.x] != STAIRS) {
            say("There is no way down here.");
            return 0;
        }
        you.depth++;
        make_level();
        fresh_screen();
        say("You go down to level");
        say_num(you.depth);
        say(".");
        if (you.depth == LAST_LEVEL)
            say("You sense the Amulet nearby.");
        return 0;
    case 'q':
        say("You quit.");
        playing = 0;
        return 0;
    default:
        return 0;
    }
    x = you.x + dx;
    y = you.y + dy;
    if ((m = monster_at(x, y)) != 0) {
        attack(m);
        return 1;
    }
    if (!walkable(x, y))
        return 0;
    you.x = x;
    you.y = y;
    pick_up();
    return 1;
}

/* Arrow keys arrive as ESC [ A-D; they become h j k l. */
static int read_key(void)
{
    int c = getch();
    if (c != 033)
        return c;
    if (getch() != '[')
        return 0;
    switch (getch()) {
    case 'A': return 'k';
    case 'B': return 'j';
    case 'C': return 'l';
    case 'D': return 'h';
    }
    return 0;
}

/* ----------------------------------------------------------- the game */

static unsigned title(void)
{
    unsigned count = 0;
    int key;
    fresh_screen();
    gotoxy(8, 30);
    cputs("R O G U E - 1 1");
    gotoxy(10, 22);
    cputs("a very small dungeon for a PDP-11");
    gotoxy(13, 18);
    cputs("move: h j k l y u b n or arrows   down: >");
    gotoxy(14, 18);
    cputs("rest: .   quit: q   find the Amulet (&)");
    gotoxy(17, 18);
    cputs("press a key (1-9 for a fixed dungeon)");
    /* No clock: how long the player takes to press a key is the seed. */
    while (!key_ready())
        count++;
    key = getch();
    if (key >= '1' && key <= '9')
        return (key - '0') * 4999u;
    return count ? count : 1;
}

static void finish(void)
{
    int row = 10;
    fresh_screen();
    gotoxy(row++, 10);
    cputs(message);
    gotoxy(++row, 10);
    if (won)
        cputs("You won on level ");
    else if (you.hp <= 0)
        cputs("You died on level ");
    else
        cputs("You quit on level ");
    putnum(you.depth);
    cputs(" with ");
    putnum(you.gold);
    cputs(" gold, at experience level ");
    putnum(you.level);
    cputs(".");
    gotoxy(ROWS - 1, 0);
    show_cursor(1);
    putch('\r');
    putch('\n');
}

int main(void)
{
    rng_state = title();
    show_cursor(0);
    you.hp = you.max_hp = 16;
    you.level = 1;
    you.depth = 1;
    make_level();
    fresh_screen();
    message_len = 0;
    say("Welcome to the dungeon. Find the Amulet on level 5.");
    playing = 1;
    while (playing) {
        look();
        draw();
        message_len = 0;
        message[0] = 0;
        if (act(read_key()) && playing) {
            /* wounds heal slowly */
            if (++you.turns % 6 == 0 && you.hp < you.max_hp)
                you.hp++;
            move_monsters();
        }
    }
    look();
    draw();
    finish();
    return 0;
}

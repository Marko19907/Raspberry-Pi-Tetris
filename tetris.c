#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <linux/input.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <poll.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

// The following defines are used to communicate with the sense hat
// #define FILEPATH "/dev/fb1" // Chose to find the display device dynamically instead
#define DISPLAY_NAME "RPi-Sense FB"
#define NAME_LENGTH 256
#define NUM_WORDS 64
#define FILESIZE (NUM_WORDS * sizeof(uint16_t))

int display_fd;
uint16_t *map; // The display memory map

// #define JOYSTICK_DEV "/dev/input/event4" // Chose to find the joystick device dynamically instead
#define JOYSTICK_NAME "Raspberry Pi Sense HAT Joystick"
int joystick_fd;

// The colors used in the game
#define COLOR_COUNT 8
uint16_t colors[COLOR_COUNT] = {
  0xF800, // Red
  0x07E0, // Green
  0x001F, // Blue
  0xFFE0, // Yellow
  0xFD20, // Orange
  0xF81F, // Magenta
  0x07FF, // Cyan
  0xFFFF, // White
};

void renderConsole(bool const playfieldChanged); // To silence the compiler warning

// The game state can be used to detect what happens on the playfield
#define GAMEOVER   0
#define ACTIVE     (1 << 0)
#define ROW_CLEAR  (1 << 1)
#define TILE_ADDED (1 << 2)

typedef struct {
  bool occupied;
  uint16_t color; // 16 bit color
} tile;

typedef struct {
  unsigned int x;
  unsigned int y;
} coord;

typedef struct {
  coord const grid;                     // playfield bounds
  unsigned long const uSecTickTime;     // tick rate
  unsigned long const rowsPerLevel;     // speed up after clearing rows
  unsigned long const initNextGameTick; // initial value of nextGameTick

  unsigned int tiles; // number of tiles played
  unsigned int rows;  // number of rows cleared
  unsigned int score; // game score
  unsigned int level; // game level

  tile *rawPlayfield; // pointer to raw memory of the playfield
  tile **playfield;   // This is the play field array
  unsigned int state;
  coord activeTile;                       // current tile

  unsigned long tick;         // incremeted at tickrate, wraps at nextGameTick
                              // when reached 0, next game state calculated
  unsigned long nextGameTick; // sets when tick is wrapping back to zero
                              // lowers with increasing level, never reaches 0
} gameConfig;



gameConfig game = {
                   .grid = {8, 8},
                   .uSecTickTime = 10000,
                   .rowsPerLevel = 2,
                   .initNextGameTick = 50,
};

int openDevice(const char *dir_path, const char *file_prefix, int ioctl_cmd, const char *device_name) {
  DIR *dir;
  struct dirent *ent;
  char name[256] = "Unknown";

  if ((dir = opendir(dir_path)) != NULL) {
    /* print all the files and directories within directory */
    while ((ent = readdir(dir)) != NULL) {
      if (strncmp(ent->d_name, file_prefix, strlen(file_prefix)) == 0) {

        // snprintf(dev_path, sizeof(dev_path), "%s/%s", dir_path, ent->d_name);
        // The above line is not used because it gives a warning about the size of dev_path so we're doing it with malloc at runtime instead
        size_t total_length = strlen(dir_path) + strlen(ent->d_name) + 2; // +1 for the slash and +1 for the null-terminating character
        char *dev_path = malloc(total_length);
        if (dev_path == NULL) {
          fprintf(stderr, "Error: Out of memory\n");
          return -1;
        }
        snprintf(dev_path, total_length, "%s/%s", dir_path, ent->d_name);

        int fd = open(dev_path, O_RDWR | O_NONBLOCK);
        if (fd == -1) {
          perror("Error (call to 'open')");
          free(dev_path);
          continue;
        }

        if (ioctl(fd, ioctl_cmd, &name) == -1) {
          perror("Error (call to 'ioctl')");
          close(fd);
          free(dev_path);
          continue;
        }

        if (strcmp(name, device_name) == 0) {
          closedir(dir);
          free(dev_path);
          return fd; // return the file descriptor of the device
        } else {
          close(fd);
        }
        free(dev_path);
      }
    }
    closedir(dir);
  } else {
    perror("Error (call to 'opendir')");
    return -1;
  }

  fprintf(stderr, "Error: %s not found\n", device_name);
  return -1;
}

// Sleep for a given number of milliseconds
inline void delay(unsigned int milliseconds) {
  usleep(milliseconds * 1000);
}

// Flash the LED matrix and turn it off
void flashSenseHatMatrix(uint16_t const color) {
  if (color == 0) { // If color is black, just turn off the LED matrix
    memset(map, 0, FILESIZE);
    return;
  }

  for (int i = 0; i < NUM_WORDS; i++) {
    map[i] = color;
  }

  delay(500);

  memset(map, 0, FILESIZE);
}

// This function is called to initialize the required devices
// returns false if something fails, else true
bool initializeSenseHat() {
  /* open the led frame buffer device */
  display_fd = openDevice("/dev", "fb", FBIOGET_FSCREENINFO, DISPLAY_NAME);
  if (display_fd == -1) {
    return false;
  }

  /* map the led frame buffer device into memory */
  map = mmap(NULL, FILESIZE, PROT_READ | PROT_WRITE, MAP_SHARED, display_fd, 0);
  if (map == MAP_FAILED) {
    close(display_fd);
    perror("Error memory mapping the file");
    exit(EXIT_FAILURE);
  }

  /* test the leds */
  flashSenseHatMatrix(0xFFFF); // Flash white

  /* open the joystick device */
  joystick_fd = openDevice("/dev/input", "event", EVIOCGNAME(NAME_LENGTH), JOYSTICK_NAME);
  if (joystick_fd == -1) {
    return false;
  }

  return true;
}

// This function is called when the application exits
// Used to free up any resources that were allocated
void freeSenseHat() {
  /* unmap the memory before exiting */
  if (munmap(map, FILESIZE) == -1) {
    perror("Error un-mmapping the display");
  }

  /* close the display device */
  close(display_fd);

  /* close the joystick device */
  if (close(joystick_fd) == -1) {
    perror("Error closing joystick");
  }
}

// This function should return the key that corresponds to the joystick press
// KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, with the respective direction
// and KEY_ENTER, when the the joystick is pressed
// !!! when nothing was pressed one MUST return 0 !!!
int readSenseHatJoystick() {
  struct input_event ev;
  ssize_t n = read(joystick_fd, &ev, sizeof(struct input_event));

  /* if no event was read or the read was non-blocking, return 0 so the game logic knows that no key was pressed */
  if (n <= 0) {
    return 0;
  }

  /* check if the event is a key event */
  if (ev.type == EV_KEY && (ev.value == 1 || ev.value == 2)) {
    // Check if the event is a key press (ev.value == 1) or autorepeat (hold) (ev.value == 2)
    return ev.code;
  }

  return 0;
}


// This function should render the gamefield on the LED matrix. It is called
// every game tick. The parameter playfieldChanged signals whether the game logic
// has changed the playfield
void renderSenseHatMatrix(bool const playfieldChanged) {
  if (!playfieldChanged) {
    return;
  }

  for (unsigned int y = 0; y < game.grid.y; y++) {
    for (unsigned int x = 0; x < game.grid.x; x++) {
      if (game.playfield[y][x].occupied) {
        map[y * game.grid.x + x] = game.playfield[y][x].color;
      } else {
        map[y * game.grid.x + x] = 0; // Black
      }
    }
  }
}


// The game logic uses only the following functions to interact with the playfield.
// Note to self: if you choose to change the playfield or the tile structure, you might need to
// adjust this game logic <> playfield interface

static inline void newTile(coord const target) {
  game.playfield[target.y][target.x].occupied = true;
  game.playfield[target.y][target.x].color = colors[rand() % COLOR_COUNT]; // random color from the color array
}

static inline void copyTile(coord const to, coord const from) {
  memcpy((void *) &game.playfield[to.y][to.x], (void *) &game.playfield[from.y][from.x], sizeof(tile));
}

static inline void copyRow(unsigned int const to, unsigned int const from) {
  memcpy((void *) &game.playfield[to][0], (void *) &game.playfield[from][0], sizeof(tile) * game.grid.x);

}

static inline void resetTile(coord const target) {
  memset((void *) &game.playfield[target.y][target.x], 0, sizeof(tile));
}

static inline void resetRow(unsigned int const target) {
  memset((void *) &game.playfield[target][0], 0, sizeof(tile) * game.grid.x);
}

static inline bool tileOccupied(coord const target) {
  return game.playfield[target.y][target.x].occupied;
}

static inline bool rowOccupied(unsigned int const target) {
  for (unsigned int x = 0; x < game.grid.x; x++) {
    coord const checkTile = {x, target};
    if (!tileOccupied(checkTile)) {
      return false;
    }
  }
  return true;
}


static inline void resetPlayfield() {
  for (unsigned int y = 0; y < game.grid.y; y++) {
    resetRow(y);
  }
}


// Below here comes the game logic.


bool addNewTile() {
  game.activeTile.y = 0;
  game.activeTile.x = (game.grid.x - 1) / 2;
  if (tileOccupied(game.activeTile))
    return false;
  newTile(game.activeTile);
  return true;
}

bool moveRight() {
  coord const newTile = {game.activeTile.x + 1, game.activeTile.y};
  if (game.activeTile.x < (game.grid.x - 1) && !tileOccupied(newTile)) {
    copyTile(newTile, game.activeTile);
    resetTile(game.activeTile);
    game.activeTile = newTile;
    return true;
  }
  return false;
}

bool moveLeft() {
  coord const newTile = {game.activeTile.x - 1, game.activeTile.y};
  if (game.activeTile.x > 0 && !tileOccupied(newTile)) {
    copyTile(newTile, game.activeTile);
    resetTile(game.activeTile);
    game.activeTile = newTile;
    return true;
  }
  return false;
}


bool moveDown() {
  coord const newTile = {game.activeTile.x, game.activeTile.y + 1};
  if (game.activeTile.y < (game.grid.y - 1) && !tileOccupied(newTile)) {
    copyTile(newTile, game.activeTile);
    resetTile(game.activeTile);
    game.activeTile = newTile;
    return true;
  }
  return false;
}


// Clear the row with animation and shift the rows down
bool clearRow() {
  if (rowOccupied(game.grid.y - 1)) {
    // Gradual removal animation
    for (int step = 0; step < game.grid.x; step++) {
      bool rowCleared = true;

      for (int x = 0; x <= step; x++) {
        coord currentTile = {x, game.grid.y - 1};
        if (tileOccupied(currentTile)) {
          resetTile(currentTile);
          rowCleared = false;
        }
      }

      if (!rowCleared) {
        renderConsole(true);
        renderSenseHatMatrix(true);
        delay(60);
      } else {
        break;
      }
    }

    // Shift rows down after clearing with animation
    for (int y = game.grid.y - 1; y > 0; y--) {
      for (int x = 0; x < game.grid.x; x++) {
        coord from = {x, y - 1};
        coord to = {x, y};

        if (tileOccupied(from)) {
            // Copy the tile from the higher position to the lower position
            copyTile(to, from);
            // Clear the tile at the higher position
            resetTile(from);

            // Render the playfield to update the display after each block's movement
            renderConsole(true);
            renderSenseHatMatrix(true);
            delay(40);
        }
      }
    }

    // Clear the top row
    resetRow(0);

    return true;
  }

  return false;
}

void advanceLevel() {
  game.level++;
  switch(game.nextGameTick) {
  case 1:
    break;
  case 2 ... 10:
    game.nextGameTick--;
    break;
  case 11 ... 20:
    game.nextGameTick -= 2;
    break;
  default:
    game.nextGameTick -= 10;
  }
}

void newGame() {
  game.state = ACTIVE;
  game.tiles = 0;
  game.rows = 0;
  game.score = 0;
  game.tick = 0;
  game.level = 0;
  resetPlayfield();
}

void gameOver() {
  game.state = GAMEOVER;
  game.nextGameTick = game.initNextGameTick;
}


bool sTetris(int const key) {
  bool playfieldChanged = false;

  if (game.state & ACTIVE) {
    // Move the current tile
    if (key) {
      playfieldChanged = true;
      switch (key) {
        case KEY_LEFT:
          moveLeft();
          break;
        case KEY_RIGHT:
          moveRight();
          break;
        case KEY_DOWN:
          // Loop for smooth downward animation
          while (true) {
            // Check if the tile can move down
            coord const newTile = {game.activeTile.x, game.activeTile.y + 1};
            if (game.activeTile.y < (game.grid.y - 1) && !tileOccupied(newTile)) {
                copyTile(newTile, game.activeTile);
                resetTile(game.activeTile);
                game.activeTile = newTile;
            } else {
                break; // Tile cannot move further down
            }

            renderConsole(true);
            renderSenseHatMatrix(true);
            delay(45); // Animation speed, 45ms is a good value
          }
          game.tick = 0;
          break;
        default:
          playfieldChanged = false;
      }
    }

    // If we have reached a tick to update the game
    if (game.tick == 0) {
      // We communicate the row clear and tile add over the game state
      // clear these bits if they were set before
      game.state &= ~(ROW_CLEAR | TILE_ADDED);

      playfieldChanged = true;
      // Clear row if possible
      if (clearRow()) {
        game.state |= ROW_CLEAR;
        game.rows++;
        game.score += game.level + 1;
        if ((game.rows % game.rowsPerLevel) == 0) {
          advanceLevel();
        }
      }

      // if there is no current tile or we cannot move it down,
      // add a new one. If not possible, game over.
      if (!tileOccupied(game.activeTile) || !moveDown()) {
        if (addNewTile()) {
          game.state |= TILE_ADDED;
          game.tiles++;
        } else {
          gameOver();
          flashSenseHatMatrix(0xF800); // Flash red
        }
      }
    }
  }

  // Press any key to start a new game
  if ((game.state == GAMEOVER) && key) {
    playfieldChanged = true;
    newGame();
    addNewTile();
    game.state |= TILE_ADDED;
    game.tiles++;
  }

  return playfieldChanged;
}

int readKeyboard() {
  struct pollfd pollStdin = {
       .fd = STDIN_FILENO,
       .events = POLLIN
  };
  int lkey = 0;

  if (poll(&pollStdin, 1, 0)) {
    lkey = fgetc(stdin);
    if (lkey != 27)
      goto exit;
    lkey = fgetc(stdin);
    if (lkey != 91)
      goto exit;
    lkey = fgetc(stdin);
  }
 exit:
    switch (lkey) {
      case 10: return KEY_ENTER;
      case 65: return KEY_UP;
      case 66: return KEY_DOWN;
      case 67: return KEY_RIGHT;
      case 68: return KEY_LEFT;
    }
  return 0;
}

void renderConsole(bool const playfieldChanged) {
  if (!playfieldChanged)
    return;

  // Goto beginning of console
  fprintf(stdout, "\033[%d;%dH", 0, 0);
  for (unsigned int x = 0; x < game.grid.x + 2; x ++) {
    fprintf(stdout, "-");
  }
  fprintf(stdout, "\n");
  for (unsigned int y = 0; y < game.grid.y; y++) {
    fprintf(stdout, "|");
    for (unsigned int x = 0; x < game.grid.x; x++) {
      coord const checkTile = {x, y};
      fprintf(stdout, "%c", (tileOccupied(checkTile)) ? '#' : ' ');
    }
    switch (y) {
      case 0:
        fprintf(stdout, "| Tiles: %10u\n", game.tiles);
        break;
      case 1:
        fprintf(stdout, "| Rows:  %10u\n", game.rows);
        break;
      case 2:
        fprintf(stdout, "| Score: %10u\n", game.score);
        break;
      case 4:
        fprintf(stdout, "| Level: %10u\n", game.level);
        break;
      case 7:
        fprintf(stdout, "| %17s\n", (game.state == GAMEOVER) ? "Game Over" : "");
        break;
    default:
        fprintf(stdout, "|\n");
    }
  }
  for (unsigned int x = 0; x < game.grid.x + 2; x++) {
    fprintf(stdout, "-");
  }
  fflush(stdout);
}


inline unsigned long uSecFromTimespec(struct timespec const ts) {
  return ((ts.tv_sec * 1000000) + (ts.tv_nsec / 1000));
}

int main(int argc, char **argv) {
  (void) argc;
  (void) argv;
  // This sets the stdin in a special state where each
  // keyboard press is directly flushed to the stdin and additionally
  // not outputted to the stdout
  {
    struct termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate);
    ttystate.c_lflag &= ~(ICANON | ECHO);
    ttystate.c_cc[VMIN] = 1;
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
  }

  // Allocate the playing field structure
  game.rawPlayfield = (tile *) malloc(game.grid.x * game.grid.y * sizeof(tile));
  game.playfield = (tile**) malloc(game.grid.y * sizeof(tile *));
  if (!game.playfield || !game.rawPlayfield) {
    fprintf(stderr, "ERROR: could not allocate playfield\n");
    return 1;
  }
  for (unsigned int y = 0; y < game.grid.y; y++) {
    game.playfield[y] = &(game.rawPlayfield[y * game.grid.x]);
  }

  // Reset playfield to make it empty
  resetPlayfield();
  // Start with gameOver
  gameOver();

  if (!initializeSenseHat()) {
    fprintf(stderr, "ERROR: could not initilize sense hat\n");
    return 1;
  };

  // Clear console, render first time
  fprintf(stdout, "\033[H\033[J");
  renderConsole(true);
  renderSenseHatMatrix(true);

  while (true) {
    struct timeval sTv, eTv;
    gettimeofday(&sTv, NULL);

    int key = readSenseHatJoystick();
    if (!key)
      key = readKeyboard();
    if (key == KEY_ENTER) {
      printf("\nExiting...\n");
      printf("Clearing LED matrix...\n");
      flashSenseHatMatrix(0xFFFF); // Flash white
      delay(500);
      break;
    }

    bool playfieldChanged = sTetris(key);
    renderConsole(playfieldChanged);
    renderSenseHatMatrix(playfieldChanged);

    // Wait for next tick
    gettimeofday(&eTv, NULL);
    unsigned long const uSecProcessTime = ((eTv.tv_sec * 1000000) + eTv.tv_usec) - ((sTv.tv_sec * 1000000 + sTv.tv_usec));
    if (uSecProcessTime < game.uSecTickTime) {
      usleep(game.uSecTickTime - uSecProcessTime);
    }
    game.tick = (game.tick + 1) % game.nextGameTick;
  }

  printf("Freeing resources...");

  freeSenseHat();
  free(game.playfield);
  free(game.rawPlayfield);

  printf(" done!\n");

  return 0;
}

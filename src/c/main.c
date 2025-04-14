#include <pebble.h>

// Structures for animated elements
typedef struct {
    GPoint pos;
    int direction;  // 1 for right, -1 for left
    int speed;
    bool active;    // Whether the fish is alive/visible
    int size;       // Size of the fish (1 = small, 2 = big)
    int grid_cell;  // Cell in the spatial grid for faster collision detection
} Fish;

typedef struct {
    GPoint base;
    int offset;
    int speed;
} Seaweed;

typedef struct {
    GPoint pos;
    int size;
    int speed;
    bool active;
} Bubble;

typedef struct {
    GPoint pos;
    int direction;
    int speed;
    bool active;
} Plankton;

typedef struct {
    GPoint pos;
    int direction;   // 1 for right, -1 for left
    int tentacle_offset;
    int speed;
} Octopus;

typedef struct {
    GPoint pos;
    int direction;   // 1 for right, -1 for left
    int animation_offset;
    int speed;
} Turtle;

typedef struct {
    GPoint pos;
    int tentacle_offset;
    int pulse_state;
    int speed;
} Jellyfish;

typedef struct {
    GPoint pos;
    int direction;      // 1 for right, -1 for left
    int jaw_state;      // Animation state for opening/closing mouth
    int speed;
    bool active;        // Only appears occasionally
    int timer;          // Countdown for appearance
} Shark;

typedef struct {
    GPoint pos;
    int curve_state;    // Animation for curved body
    bool active;        // Only appears occasionally
    int timer;          // Countdown for appearance/disappearance
} Seahorse;

typedef struct {
    GPoint pos;
    int direction;   // 1 for right, -1 for left
    int claw_state;  // For animating claws
    int speed;
} Crab;

typedef struct {
    GPoint pos;
    int open_state;  // For occasional opening/closing
} Clam;

// UI Elements
static Window *s_main_window;
static Layer *s_canvas_layer;
static TextLayer *s_time_layer;
static TextLayer *s_date_layer;
static Layer *s_battery_layer;
static AppTimer *s_animation_timer; // Add persistent timer handle

// Battery state
static int s_battery_level = 100;
static bool s_is_charging = false;

// Pre-allocated paths for drawing
static GPath *s_fish_tail_path = NULL;
static GPath *s_turtle_front_flipper_path = NULL;
static GPath *s_turtle_back_flipper_path = NULL;
static GPath *s_shark_body_path = NULL;
static GPath *s_shark_tail_path = NULL;
static GPath *s_shark_fin_path = NULL;

// Points arrays for path objects
static GPoint s_fish_tail_points[3];
static GPoint s_turtle_front_flipper_points[3];
static GPoint s_turtle_back_flipper_points[3];
static GPoint s_shark_body_points[5];
static GPoint s_shark_tail_points[3];
static GPoint s_shark_fin_points[3];

// Animation elements
#define MAX_FISH 5         // Increased for more fish
#define MAX_BIG_FISH 2     // Big fish that eat small fish
#define MAX_SEAWEED 4
#define MAX_BUBBLES 8
#define MAX_PLANKTON 6
#define MAX_TURTLES 1
#define MAX_JELLYFISH 1    // Reduced to one jellyfish
static Fish s_fish[MAX_FISH + MAX_BIG_FISH];  // Combined array for all fish
static Seaweed s_seaweed[MAX_SEAWEED];
static Bubble s_bubbles[MAX_BUBBLES];
static Plankton s_plankton[MAX_PLANKTON];
static Octopus s_octopus;
static Turtle s_turtles[MAX_TURTLES];
static Jellyfish s_jellyfish[MAX_JELLYFISH];
static Shark s_shark;      // One shark is enough!
static Seahorse s_seahorse; // One seahorse
static Crab s_crab;        // Tiny crab at the bottom
static Clam s_clam;        // Small clam

// Update frequency
#define ANIMATION_INTERVAL 50
#define ANIMATION_INTERVAL_LOW_POWER 100  // Slower updates when battery is low
#define LOW_BATTERY_THRESHOLD 20  // Consider battery low at 20%

// Spatial grid for collision detection optimization
#define GRID_WIDTH 3
#define GRID_HEIGHT 3
#define GRID_CELL_WIDTH 144 / GRID_WIDTH
#define GRID_CELL_HEIGHT 168 / GRID_HEIGHT
#define GRID_CELL_COUNT (GRID_WIDTH * GRID_HEIGHT)

// Track which fish are in which grid cells to optimize collision detection
static int s_fish_in_grid[GRID_CELL_COUNT][MAX_FISH + MAX_BIG_FISH];
static int s_fish_grid_counts[GRID_CELL_COUNT];

// Helper function for safer random number generation within a range
static int random_in_range(int min, int max) {
    // Ensure max > min
    if (max <= min) return min;
    
    // Calculate safe range and ensure it won't overflow
    int range = max - min + 1;
    if (range <= 0) return min; // Overflow protection
    
    // Use modulo for smaller range
    return min + (rand() % range);
}

// Initialize a fish with random position and speed
static void init_fish(Fish *fish, int size) {
    fish->pos.y = random_in_range(20, 119); // Between 20 and 119
    fish->direction = (random_in_range(0, 1) * 2) - 1;  // Either 1 or -1
    fish->speed = size == 1 ? 
                 random_in_range(2, 4) : 
                 random_in_range(1, 2);  // Big fish are slower
    fish->pos.x = (fish->direction == 1) ? -10 : 144;  // Use screen width constant
    fish->active = true;
    fish->size = size;
}

// Initialize seaweed with base position
static void init_seaweed(Seaweed *seaweed, int x) {
    seaweed->base.x = x;
    seaweed->base.y = 168;  // Screen height
    seaweed->offset = 0;
    seaweed->speed = random_in_range(1, 2);
}

// Initialize bubble
static void init_bubble(Bubble *bubble) {
    bubble->pos.x = random_in_range(0, 143);  // Random x position
    bubble->pos.y = 168;           // Start at bottom
    bubble->size = random_in_range(1, 3);
    bubble->speed = random_in_range(1, 3);
    bubble->active = true;
}

// Initialize plankton
static void init_plankton(Plankton *plankton) {
    plankton->pos.x = random_in_range(0, 143);
    plankton->pos.y = random_in_range(20, 139);
    plankton->direction = (random_in_range(0, 1) * 2) - 1;  // Either 1 or -1
    plankton->speed = random_in_range(1, 2);
    plankton->active = true;
}

// Initialize octopus
static void init_octopus(Octopus *octopus) {
    octopus->pos.x = random_in_range(37, 106);  // Somewhere in the middle
    octopus->pos.y = 25;                // Near the top (moved from bottom)
    octopus->direction = (random_in_range(0, 1) * 2) - 1;
    octopus->tentacle_offset = 0;
    octopus->speed = 1;
}

// Initialize turtle
static void init_turtle(Turtle *turtle) {
    turtle->pos.y = random_in_range(60, 119);  // Middle to bottom area
    turtle->direction = (random_in_range(0, 1) * 2) - 1;  // Either 1 or -1
    turtle->pos.x = (turtle->direction == 1) ? -15 : 144;  // Start offscreen
    turtle->animation_offset = 0;
    turtle->speed = 1;  // Turtles are slow
}

// Initialize jellyfish
static void init_jellyfish(Jellyfish *jellyfish) {
    jellyfish->pos.x = 72;  // Center horizontally
    jellyfish->pos.y = 120;  // Lower part but not too low
    jellyfish->tentacle_offset = 0;
    jellyfish->pulse_state = 0;
    jellyfish->speed = random_in_range(1, 2);
}

// Initialize shark
static void init_shark(Shark *shark) {
    shark->direction = (random_in_range(0, 1) * 2) - 1;  // Either 1 or -1
    shark->pos.x = (shark->direction == 1) ? -30 : 174;  // Start further offscreen
    shark->pos.y = random_in_range(50, 99);  // Middle area of screen
    shark->jaw_state = 0;  // Mouth closed
    shark->speed = 3;  // Sharks are fast!
    shark->active = false;  // Start inactive
    shark->timer = random_in_range(150, 299);  // Reduced timer - appear more often (2.5-5 seconds)
}

// Initialize seahorse
static void init_seahorse(Seahorse *seahorse) {
    seahorse->pos.x = 20;  // Fixed position at left side
    seahorse->pos.y = 140; // Bottom left corner
    seahorse->curve_state = 0;
    seahorse->active = true;
    seahorse->timer = 0;   // Not used for disappearing anymore
}

// Initialize crab
static void init_crab(Crab *crab) {
    crab->pos.x = 100;  // Start around the middle-right
    crab->pos.y = 160;  // Very close to bottom
    crab->direction = -1;  // Start moving left
    crab->claw_state = 0;
    crab->speed = 1;
}

// Initialize clam
static void init_clam(Clam *clam) {
    clam->pos.x = 120;  // Right side of bottom
    clam->pos.y = 165;  // Very bottom
    clam->open_state = 0;
}

// Forward declarations for update functions
static void update_seahorse(Seahorse *seahorse);
static void update_crab(Crab *crab);
static void update_clam(Clam *clam);
static void update_turtle(Turtle *turtle);
static void update_jellyfish(Jellyfish *jellyfish);
static void update_octopus(Octopus *octopus);

// Update seahorse
static void update_seahorse(Seahorse *seahorse) {
    // Only animate the seahorse's body with gentle swaying
    seahorse->curve_state = (seahorse->curve_state + 1) % TRIG_MAX_ANGLE;
    
    // Always keep active
    seahorse->active = true;
}

// Draw fish
static void draw_fish(GContext *ctx, const Fish *fish) {
    if (!fish->active) return;
    
    // Set fill color (white for B&W displays)
    graphics_context_set_fill_color(ctx, GColorWhite);
    
    int size = fish->size == 1 ? 4 : 7;  // Size difference for big fish
    
    // Fish body - using GPoint directly as required by Diorite
    graphics_fill_circle(ctx, fish->pos, size);
    
    // Tail
    s_fish_tail_points[0].x = fish->pos.x - (fish->direction * size);
    s_fish_tail_points[0].y = fish->pos.y;
    s_fish_tail_points[1].x = fish->pos.x - (fish->direction * (size * 2));
    s_fish_tail_points[1].y = fish->pos.y - size;
    s_fish_tail_points[2].x = fish->pos.x - (fish->direction * (size * 2));
    s_fish_tail_points[2].y = fish->pos.y + size;
    
    // Update path points - no recreation needed
    if (s_fish_tail_path) {
        gpath_move_to(s_fish_tail_path, GPoint(0, 0));
        gpath_draw_filled(ctx, s_fish_tail_path);
    }
    
    // Add eye for big fish
    if (fish->size > 1) {
        graphics_context_set_fill_color(ctx, GColorBlack);
        GPoint eye_pos = (GPoint){
            fish->pos.x + (fish->direction * 3),
            fish->pos.y - 2
        };
        graphics_fill_circle(ctx, eye_pos, 1);
    }
}

// Draw seaweed
static void draw_seaweed(GContext *ctx, const Seaweed *seaweed) {
    // Set stroke color (white for B&W displays)
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_context_set_stroke_width(ctx, 2);
    
    GPoint current = seaweed->base;
    GPoint next;
    
    for (int i = 0; i < 6; i++) {
        int32_t angle = (seaweed->offset + (i * 1000)) % TRIG_MAX_ANGLE;
        int16_t offset = (sin_lookup(angle) * seaweed->speed) / TRIG_MAX_RATIO;
        
        // Properly initialize next point
        next.x = current.x + offset;
        next.y = current.y - 10;
        
        graphics_draw_line(ctx, current, next);
        current = next;
    }
}

// Draw bubble
static void draw_bubble(GContext *ctx, const Bubble *bubble) {
    if (!bubble->active) return;
    
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_draw_circle(ctx, bubble->pos, bubble->size);
}

// Draw plankton
static void draw_plankton(GContext *ctx, const Plankton *plankton) {
    if (!plankton->active) return;
    
    graphics_context_set_fill_color(ctx, GColorWhite);
    
    // Draw as a tiny dot/small shape
    graphics_fill_circle(ctx, plankton->pos, 1);
}

// Draw octopus
static void draw_octopus(GContext *ctx, const Octopus *octopus) {
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_context_set_stroke_color(ctx, GColorWhite);
    
    // Draw head
    graphics_fill_circle(ctx, octopus->pos, 6);
    
    // Draw eyes
    graphics_context_set_fill_color(ctx, GColorBlack);
    GPoint left_eye = (GPoint){octopus->pos.x - 2, octopus->pos.y - 2};
    GPoint right_eye = (GPoint){octopus->pos.x + 2, octopus->pos.y - 2};
    graphics_fill_circle(ctx, left_eye, 1);
    graphics_fill_circle(ctx, right_eye, 1);
    
    // Draw tentacles
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_context_set_fill_color(ctx, GColorWhite);
    
    for (int i = 0; i < 8; i++) {
        int32_t angle = (octopus->tentacle_offset + (i * TRIG_MAX_ANGLE / 8)) % TRIG_MAX_ANGLE;
        int distance = 8;
        
        GPoint start = octopus->pos;
        GPoint end;
        
        for (int j = 0; j < 3; j++) {
            int32_t wave_angle = (octopus->tentacle_offset * 3 + (i * 500) + (j * 2000)) % TRIG_MAX_ANGLE;
            int16_t wave_offset = (sin_lookup(wave_angle) * 3) / TRIG_MAX_RATIO;
            
            int32_t segment_angle = angle + (wave_offset * TRIG_MAX_ANGLE / 360);
            
            end.x = start.x + (sin_lookup(segment_angle) * distance) / TRIG_MAX_RATIO;
            end.y = start.y + (cos_lookup(segment_angle) * distance) / TRIG_MAX_RATIO;
            
            graphics_draw_line(ctx, start, end);
            start = end;
            distance = j < 2 ? 6 : 4;  // Get shorter toward the end
        }
    }
}

// Draw turtle
static void draw_turtle(GContext *ctx, const Turtle *turtle) {
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_context_set_stroke_color(ctx, GColorWhite);
    
    // Animation offset for swimming motion
    int32_t flipper_angle = turtle->animation_offset % TRIG_MAX_ANGLE;
    int flipper_offset = (sin_lookup(flipper_angle) * 2) / TRIG_MAX_RATIO;
    
    // Draw shell with pattern (oval with details)
    GRect shell_rect = (GRect){
        .origin = {turtle->pos.x - 8, turtle->pos.y - 5},
        .size = {16, 10}
    };
    graphics_fill_rect(ctx, shell_rect, 4, GCornersAll);
    
    // Shell pattern - draw shell segments
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_stroke_width(ctx, 1);
    
    // Vertical line down the middle
    graphics_draw_line(ctx, 
                      (GPoint){turtle->pos.x, turtle->pos.y - 5},
                      (GPoint){turtle->pos.x, turtle->pos.y + 5});
    
    // Horizontal segments
    graphics_draw_line(ctx, 
                      (GPoint){turtle->pos.x - 7, turtle->pos.y - 2},
                      (GPoint){turtle->pos.x + 7, turtle->pos.y - 2});
    graphics_draw_line(ctx, 
                      (GPoint){turtle->pos.x - 7, turtle->pos.y + 2},
                      (GPoint){turtle->pos.x + 7, turtle->pos.y + 2});
    
    // Draw head
    graphics_context_set_fill_color(ctx, GColorWhite);
    GPoint head_pos = (GPoint){
        turtle->pos.x + (turtle->direction * 9),
        turtle->pos.y
    };
    graphics_fill_circle(ctx, head_pos, 4);
    
    // Draw eye
    graphics_context_set_fill_color(ctx, GColorBlack);
    GPoint eye_pos = (GPoint){
        head_pos.x + (turtle->direction * 1),
        head_pos.y - 1
    };
    graphics_fill_circle(ctx, eye_pos, 1);
    
    // Draw flippers
    graphics_context_set_fill_color(ctx, GColorWhite);
    
    // Front flipper - update points
    s_turtle_front_flipper_points[0].x = turtle->pos.x + (turtle->direction * 5);
    s_turtle_front_flipper_points[0].y = turtle->pos.y - 2;
    s_turtle_front_flipper_points[1].x = turtle->pos.x + (turtle->direction * 5);
    s_turtle_front_flipper_points[1].y = turtle->pos.y + 6;
    s_turtle_front_flipper_points[2].x = turtle->pos.x + (turtle->direction * (10 + flipper_offset));
    s_turtle_front_flipper_points[2].y = turtle->pos.y + 5;
    
    // Back flipper - update points
    s_turtle_back_flipper_points[0].x = turtle->pos.x - (turtle->direction * 5);
    s_turtle_back_flipper_points[0].y = turtle->pos.y - 2;
    s_turtle_back_flipper_points[1].x = turtle->pos.x - (turtle->direction * 5);
    s_turtle_back_flipper_points[1].y = turtle->pos.y + 6;
    s_turtle_back_flipper_points[2].x = turtle->pos.x - (turtle->direction * (10 - flipper_offset));
    s_turtle_back_flipper_points[2].y = turtle->pos.y + 5;
    
    // Draw flippers using existing paths
    if (s_turtle_front_flipper_path) {
        gpath_move_to(s_turtle_front_flipper_path, GPoint(0, 0));
        gpath_draw_filled(ctx, s_turtle_front_flipper_path);
    }
    
    if (s_turtle_back_flipper_path) {
        gpath_move_to(s_turtle_back_flipper_path, GPoint(0, 0));
        gpath_draw_filled(ctx, s_turtle_back_flipper_path);
    }
}

// Draw jellyfish
static void draw_jellyfish(GContext *ctx, const Jellyfish *jellyfish) {
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_context_set_stroke_color(ctx, GColorWhite);
    
    // Pulsing animation for the bell
    int bell_size = 7 + ((jellyfish->pulse_state < 50) ? jellyfish->pulse_state / 10 : (100 - jellyfish->pulse_state) / 10);
    
    // Draw bell (semi-circle)
    int bell_width = bell_size * 2;
    GRect bell_rect = (GRect){
        .origin = {jellyfish->pos.x - bell_size, jellyfish->pos.y - bell_size},
        .size = {bell_width, bell_size}
    };
    graphics_fill_rect(ctx, bell_rect, 0, GCornerNone);
    graphics_fill_circle(ctx, (GPoint){jellyfish->pos.x, jellyfish->pos.y - bell_size}, bell_size);
    
    // Draw tentacles
    for (int i = 0; i < 5; i++) {
        int x_pos = jellyfish->pos.x - bell_size + (i * bell_width / 4);
        GPoint start = (GPoint){x_pos, jellyfish->pos.y};
        GPoint end = start;
        
        for (int j = 0; j < 3; j++) {
            int32_t wave_angle = (jellyfish->tentacle_offset + (i * 1000) + (j * 1500)) % TRIG_MAX_ANGLE;
            int16_t wave_offset = (sin_lookup(wave_angle) * 3) / TRIG_MAX_RATIO;
            
            end.x = start.x + wave_offset;
            end.y = start.y + 5;
            
            graphics_draw_line(ctx, start, end);
            start = end;
        }
    }
}

// Draw crab
static void draw_crab(GContext *ctx, const Crab *crab) {
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_context_set_stroke_color(ctx, GColorWhite);
    
    // Draw tiny body (small circle)
    graphics_fill_circle(ctx, crab->pos, 3);
    
    // Animate claws
    int claw_offset = (crab->claw_state % 20 < 10) ? 0 : 1;
    
    // Draw legs (3 on each side)
    for (int i = 0; i < 3; i++) {
        // Left legs
        GPoint leg_start_l = (GPoint){crab->pos.x - 2, crab->pos.y - 1 + i};
        GPoint leg_end_l = (GPoint){crab->pos.x - 5, crab->pos.y + 1 + i};
        graphics_draw_line(ctx, leg_start_l, leg_end_l);
        
        // Right legs
        GPoint leg_start_r = (GPoint){crab->pos.x + 2, crab->pos.y - 1 + i};
        GPoint leg_end_r = (GPoint){crab->pos.x + 5, crab->pos.y + 1 + i};
        graphics_draw_line(ctx, leg_start_r, leg_end_r);
    }
    
    // Draw claws
    GPoint claw_left_start = (GPoint){crab->pos.x - 3, crab->pos.y - 2};
    GPoint claw_left_mid = (GPoint){crab->pos.x - 5, crab->pos.y - 3};
    GPoint claw_left_end = (GPoint){crab->pos.x - 6, crab->pos.y - 4 + claw_offset};
    
    GPoint claw_right_start = (GPoint){crab->pos.x + 3, crab->pos.y - 2};
    GPoint claw_right_mid = (GPoint){crab->pos.x + 5, crab->pos.y - 3};
    GPoint claw_right_end = (GPoint){crab->pos.x + 6, crab->pos.y - 4 + claw_offset};
    
    graphics_draw_line(ctx, claw_left_start, claw_left_mid);
    graphics_draw_line(ctx, claw_left_mid, claw_left_end);
    
    graphics_draw_line(ctx, claw_right_start, claw_right_mid);
    graphics_draw_line(ctx, claw_right_mid, claw_right_end);
    
    // Draw eyes (tiny dots on top)
    graphics_context_set_fill_color(ctx, GColorBlack);
    GPoint eye_left = (GPoint){crab->pos.x - 1, crab->pos.y - 2};
    GPoint eye_right = (GPoint){crab->pos.x + 1, crab->pos.y - 2};
    graphics_fill_circle(ctx, eye_left, 1);
    graphics_fill_circle(ctx, eye_right, 1);
}

// Draw clam
static void draw_clam(GContext *ctx, const Clam *clam) {
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_context_set_stroke_color(ctx, GColorWhite);
    
    // Draw clam shell
    int open_amount = (clam->open_state > 0) ? clam->open_state / 10 : 0;
    
    // Bottom half (static)
    GRect bottom_rect = (GRect){
        .origin = {clam->pos.x - 5, clam->pos.y - 2},
        .size = {10, 4}
    };
    graphics_fill_rect(ctx, bottom_rect, 3, GCornersBottom);
    
    // Top half (moves slightly when opening)
    GRect top_rect = (GRect){
        .origin = {clam->pos.x - 5, clam->pos.y - 4 - open_amount},
        .size = {10, 4}
    };
    graphics_fill_rect(ctx, top_rect, 3, GCornersTop);
    
    // If open, show a tiny pearl inside
    if (open_amount > 0) {
        graphics_context_set_fill_color(ctx, GColorBlack);
        GPoint pearl_pos = (GPoint){clam->pos.x, clam->pos.y - 2};
        graphics_fill_circle(ctx, pearl_pos, 1);
    }
}

// Draw shark
static void draw_shark(GContext *ctx, const Shark *shark) {
    if (!shark->active) return;
    
    // Simple, classic shark design
    graphics_context_set_fill_color(ctx, GColorWhite);
    
    // Update shark body points
    s_shark_body_points[0].x = shark->pos.x + (shark->direction * 15);
    s_shark_body_points[0].y = shark->pos.y;        // nose
    s_shark_body_points[1].x = shark->pos.x;
    s_shark_body_points[1].y = shark->pos.y - 8;    // top of body
    s_shark_body_points[2].x = shark->pos.x - (shark->direction * 15);
    s_shark_body_points[2].y = shark->pos.y - 5;    // back top
    s_shark_body_points[3].x = shark->pos.x - (shark->direction * 15);
    s_shark_body_points[3].y = shark->pos.y + 5;    // back bottom
    s_shark_body_points[4].x = shark->pos.x;
    s_shark_body_points[4].y = shark->pos.y + 8;    // bottom of body
    
    // Draw using existing path
    if (s_shark_body_path) {
        gpath_move_to(s_shark_body_path, GPoint(0, 0));
        gpath_draw_filled(ctx, s_shark_body_path);
    }
    
    // Update tail points
    s_shark_tail_points[0].x = shark->pos.x - (shark->direction * 15);
    s_shark_tail_points[0].y = shark->pos.y - 5;
    s_shark_tail_points[1].x = shark->pos.x - (shark->direction * 15);
    s_shark_tail_points[1].y = shark->pos.y + 5;
    s_shark_tail_points[2].x = shark->pos.x - (shark->direction * 25);
    s_shark_tail_points[2].y = shark->pos.y;
    
    // Draw using existing path
    if (s_shark_tail_path) {
        gpath_move_to(s_shark_tail_path, GPoint(0, 0));
        gpath_draw_filled(ctx, s_shark_tail_path);
    }
    
    // Update fin points
    s_shark_fin_points[0].x = shark->pos.x - (shark->direction * 5);
    s_shark_fin_points[0].y = shark->pos.y - 8;
    s_shark_fin_points[1].x = shark->pos.x - (shark->direction * 5);
    s_shark_fin_points[1].y = shark->pos.y - 16;
    s_shark_fin_points[2].x = shark->pos.x + (shark->direction * 3);
    s_shark_fin_points[2].y = shark->pos.y - 8;
    
    // Draw using existing path
    if (s_shark_fin_path) {
        gpath_move_to(s_shark_fin_path, GPoint(0, 0));
        gpath_draw_filled(ctx, s_shark_fin_path);
    }
    
    // Draw eye
    graphics_context_set_fill_color(ctx, GColorBlack);
    GPoint eye_pos = (GPoint){
        shark->pos.x + (shark->direction * 8),
        shark->pos.y - 2
    };
    graphics_fill_circle(ctx, eye_pos, 1);
    
    // Simple mouth line
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_line(ctx, 
                       (GPoint){shark->pos.x + (shark->direction * 14), shark->pos.y + 2},
                       (GPoint){shark->pos.x + (shark->direction * 6), shark->pos.y + 3});
}

// Draw seahorse
static void draw_seahorse(GContext *ctx, const Seahorse *seahorse) {
    if (!seahorse->active) return;
    
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_context_set_stroke_color(ctx, GColorWhite);
    
    // Animate curve state for gentle swaying
    int32_t curve_angle = seahorse->curve_state % TRIG_MAX_ANGLE;
    int curve_offset = (sin_lookup(curve_angle) * 2) / TRIG_MAX_RATIO;
    
    // Draw the head - positioned upright like a real seahorse
    GPoint head_pos = seahorse->pos;
    graphics_fill_circle(ctx, head_pos, 5); // Clear seahorse head
    
    // Draw the snout - characteristic downward-facing seahorse snout
    GPoint snout_start = (GPoint){head_pos.x, head_pos.y - 2};
    GPoint snout_mid = (GPoint){head_pos.x + 3, head_pos.y + 1};
    GPoint snout_end = (GPoint){head_pos.x + 6, head_pos.y + 3};
    
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_line(ctx, snout_start, snout_mid);
    graphics_draw_line(ctx, snout_mid, snout_end);
    
    // Draw characteristic coronet/crest on top of head
    GPoint crest[3] = {
        {head_pos.x - 2, head_pos.y - 5},
        {head_pos.x, head_pos.y - 8},
        {head_pos.x + 2, head_pos.y - 5}
    };
    graphics_context_set_stroke_width(ctx, 1);
    for (int i = 0; i < 2; i++) {
        graphics_draw_line(ctx, crest[i], crest[i+1]);
    }
    
    // Draw eye
    graphics_context_set_fill_color(ctx, GColorBlack);
    GPoint eye_pos = (GPoint){head_pos.x + 2, head_pos.y - 1};
    graphics_fill_circle(ctx, eye_pos, 1);
    
    // Draw the main body - more pronounced curve with segments
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_context_set_stroke_width(ctx, 3);
    
    // Set up body segments for a better seahorse curve
    // Seahorses have a distinctive curved body that arches forward
    GPoint body_segments[7]; // More points for better definition
    body_segments[0] = head_pos;
    
    // Create a more accurate seahorse profile - arching forward with belly outward
    body_segments[1].x = head_pos.x - 2;
    body_segments[1].y = head_pos.y + 6;
    
    body_segments[2].x = head_pos.x - 4 + curve_offset;
    body_segments[2].y = head_pos.y + 12;
    
    body_segments[3].x = head_pos.x - 2 + curve_offset;
    body_segments[3].y = head_pos.y + 18;
    
    body_segments[4].x = head_pos.x;
    body_segments[4].y = head_pos.y + 24;
    
    body_segments[5].x = head_pos.x + 2;
    body_segments[5].y = head_pos.y + 30;
    
    body_segments[6].x = head_pos.x + 1 - curve_offset;
    body_segments[6].y = head_pos.y + 35;
    
    // Draw the body segments
    for (int i = 1; i < 7; i++) {
        graphics_draw_line(ctx, body_segments[i-1], body_segments[i]);
    }
    
    // Draw the characteristic segmented appearance
    graphics_context_set_stroke_width(ctx, 1);
    for (int i = 1; i < 6; i++) {
        // Draw little ridges/bumps along the outer edge
        GPoint bump1 = {
            body_segments[i].x + 2,
            body_segments[i].y - 1
        };
        GPoint bump2 = {
            body_segments[i].x + 3,
            body_segments[i].y
        };
        graphics_draw_line(ctx, body_segments[i], bump1);
        graphics_draw_line(ctx, bump1, bump2);
    }
    
    // Draw the curled tail - tightly curled at the end
    GPoint tail_points[4];
    tail_points[0] = body_segments[6];
    tail_points[1].x = body_segments[6].x - 2;
    tail_points[1].y = body_segments[6].y + 3;
    tail_points[2].x = body_segments[6].x - 4;
    tail_points[2].y = body_segments[6].y + 2;
    tail_points[3].x = body_segments[6].x - 5;
    tail_points[3].y = body_segments[6].y - 1;
    
    graphics_context_set_stroke_width(ctx, 2);
    for (int i = 1; i < 4; i++) {
        graphics_draw_line(ctx, tail_points[i-1], tail_points[i]);
    }
    
    // Draw the characteristic bulging belly - seahorses have a distinct pouch
    graphics_context_set_fill_color(ctx, GColorWhite);
    GPoint belly_center = (GPoint){
        body_segments[3].x - 4,
        body_segments[3].y
    };
    graphics_fill_circle(ctx, belly_center, 3);
    
    // Draw dorsal fin - on the back
    graphics_context_set_stroke_width(ctx, 1);
    GPoint dorsal_fin[3] = {
        {body_segments[2].x, body_segments[2].y},
        {body_segments[2].x - 4, body_segments[2].y - 5},
        {body_segments[2].x + 2, body_segments[2].y - 2}
    };
    
    for (int i = 0; i < 2; i++) {
        graphics_draw_line(ctx, dorsal_fin[i], dorsal_fin[i+1]);
    }
    
    // Draw pectoral fin - small fin behind head
    GPoint pectoral_fin[3] = {
        {body_segments[1].x, body_segments[1].y},
        {body_segments[1].x - 3, body_segments[1].y - 2},
        {body_segments[1].x - 1, body_segments[1].y + 2}
    };
    
    for (int i = 0; i < 2; i++) {
        graphics_draw_line(ctx, pectoral_fin[i], pectoral_fin[i+1]);
    }
}

// Check if two elements collide (basic circle collision)
static bool check_collision(GPoint pos1, int radius1, GPoint pos2, int radius2) {
    int dx = pos1.x - pos2.x;
    int dy = pos1.y - pos2.y;
    int distance_squared = (dx * dx) + (dy * dy);
    int radius_sum = radius1 + radius2;
    return distance_squared <= (radius_sum * radius_sum);
}

// Calculate grid cell for a point
static int get_grid_cell(GPoint point) {
    int grid_x = point.x / GRID_CELL_WIDTH;
    int grid_y = point.y / GRID_CELL_HEIGHT;
    
    // Clamp to grid bounds
    if (grid_x < 0) grid_x = 0;
    if (grid_x >= GRID_WIDTH) grid_x = GRID_WIDTH - 1;
    if (grid_y < 0) grid_y = 0;
    if (grid_y >= GRID_HEIGHT) grid_y = GRID_HEIGHT - 1;
    
    return grid_y * GRID_WIDTH + grid_x;
}

// Update fish spatial grid positions
static void update_spatial_grid() {
    // Reset grid
    for (int i = 0; i < GRID_CELL_COUNT; i++) {
        s_fish_grid_counts[i] = 0;
    }
    
    // Place fish in grid cells
    for (int i = 0; i < MAX_FISH + MAX_BIG_FISH; i++) {
        if (s_fish[i].active) {
            int cell = get_grid_cell(s_fish[i].pos);
            s_fish[i].grid_cell = cell;
            
            // Add to grid if there's space
            if (s_fish_grid_counts[cell] < MAX_FISH + MAX_BIG_FISH) {
                s_fish_in_grid[cell][s_fish_grid_counts[cell]] = i;
                s_fish_grid_counts[cell]++;
            }
        }
    }
}

// Update canvas layer
static void canvas_update_proc(Layer *layer, GContext *ctx) {
    // Clear the screen (black for B&W displays)
    graphics_context_set_fill_color(ctx, GColorBlack);
    GRect bounds = layer_get_bounds(layer);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);
    
    // Draw seaweed first (background)
    for (int i = 0; i < MAX_SEAWEED; i++) {
        draw_seaweed(ctx, &s_seaweed[i]);
    }
    
    // Draw clam at the very bottom
    draw_clam(ctx, &s_clam);
    
    // Draw crab at the bottom
    draw_crab(ctx, &s_crab);
    
    // Draw plankton
    for (int i = 0; i < MAX_PLANKTON; i++) {
        draw_plankton(ctx, &s_plankton[i]);
    }
    
    // Draw turtle
    for (int i = 0; i < MAX_TURTLES; i++) {
        draw_turtle(ctx, &s_turtles[i]);
    }
    
    // Draw jellyfish
    for (int i = 0; i < MAX_JELLYFISH; i++) {
        draw_jellyfish(ctx, &s_jellyfish[i]);
    }
    
    // Draw seahorse
    draw_seahorse(ctx, &s_seahorse);
    
    // Draw fish (foreground)
    for (int i = 0; i < MAX_FISH + MAX_BIG_FISH; i++) {
        draw_fish(ctx, &s_fish[i]);
    }
    
    // Draw bubbles
    for (int i = 0; i < MAX_BUBBLES; i++) {
        draw_bubble(ctx, &s_bubbles[i]);
    }
    
    // Draw octopus
    draw_octopus(ctx, &s_octopus);
    
    // Draw shark on top of everything (it's the apex predator!)
    draw_shark(ctx, &s_shark);
}

// Battery layer update proc
static void battery_update_proc(Layer *layer, GContext *ctx) {
    BatteryChargeState charge_state = battery_state_service_peek();
    
    const int WIDTH = 20;
    const int HEIGHT = 8;
    GRect battery_rect;
    battery_rect.origin.x = 0;
    battery_rect.origin.y = 0;
    battery_rect.size.w = WIDTH;
    battery_rect.size.h = HEIGHT;
    
    // Draw battery outline
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_draw_rect(ctx, battery_rect);
    
    // Draw battery level
    graphics_context_set_fill_color(ctx, GColorWhite);
    int fill_width = (charge_state.charge_percent * WIDTH) / 100;
    
    GRect fill_rect;
    fill_rect.origin.x = 0;
    fill_rect.origin.y = 0;
    fill_rect.size.w = fill_width;
    fill_rect.size.h = HEIGHT;
    
    graphics_fill_rect(ctx, fill_rect, 0, GCornerNone);
}

// Animation update
static void animation_update(void) {
    // Update fish positions and check for fish being eaten
    for (int i = 0; i < MAX_FISH + MAX_BIG_FISH; i++) {
        if (!s_fish[i].active) continue;
        
        s_fish[i].pos.x += s_fish[i].direction * s_fish[i].speed;
        
        // Reset fish if it swims off screen
        if ((s_fish[i].direction == 1 && s_fish[i].pos.x > 144) ||  // Use screen width
            (s_fish[i].direction == -1 && s_fish[i].pos.x < -10)) {
            if (s_fish[i].size == 1) {
                init_fish(&s_fish[i], 1);  // Reinitialize small fish
            } else {
                init_fish(&s_fish[i], 2);  // Reinitialize big fish
            }
        }
    }
    
    // Update spatial grid
    update_spatial_grid();
    
    // Check for fish collisions using grid for optimization
    for (int i = 0; i < MAX_FISH + MAX_BIG_FISH; i++) {
        // Only check big fish as predators
        if (!s_fish[i].active || s_fish[i].size <= 1) continue;
        
        int cell = s_fish[i].grid_cell;
        
        // Check fish in same cell and adjacent cells
        for (int cell_y = -1; cell_y <= 1; cell_y++) {
            for (int cell_x = -1; cell_x <= 1; cell_x++) {
                int target_cell_x = (cell % GRID_WIDTH) + cell_x;
                int target_cell_y = (cell / GRID_WIDTH) + cell_y;
                
                // Skip if outside grid
                if (target_cell_x < 0 || target_cell_x >= GRID_WIDTH ||
                    target_cell_y < 0 || target_cell_y >= GRID_HEIGHT) {
                    continue;
                }
                
                int target_cell = target_cell_y * GRID_WIDTH + target_cell_x;
                
                // Check all small fish in this cell
                for (int k = 0; k < s_fish_grid_counts[target_cell]; k++) {
                    int j = s_fish_in_grid[target_cell][k];
                    
                    // Only check small fish that are active
                    if (j < MAX_FISH && s_fish[j].active && s_fish[j].size == 1) {
                        if (check_collision(s_fish[i].pos, 7, s_fish[j].pos, 4)) {
                            s_fish[j].active = false;  // Small fish gets eaten
                            
                            // Create bubbles for eating event - limit to available bubbles
                            int bubbles_created = 0;
                            for (int b = 0; b < MAX_BUBBLES && bubbles_created < 3; b++) {
                                if (!s_bubbles[b].active) {
                                    s_bubbles[b].pos = s_fish[j].pos;
                                    s_bubbles[b].size = random_in_range(1, 2);
                                    s_bubbles[b].speed = random_in_range(1, 2);
                                    s_bubbles[b].active = true;
                                    bubbles_created++;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Check for respawning fish - limit checks to reduce CPU usage
    for (int i = 0; i < MAX_FISH; i++) {
        if (!s_fish[i].active && (random_in_range(0, 99) < 2)) { // 2% chance instead of complex comparison
            init_fish(&s_fish[i], 1);  // Reinitialize with size 1 (small fish)
        }
    }
    
    // Update seaweed animation
    for (int i = 0; i < MAX_SEAWEED; i++) {
        s_seaweed[i].offset += s_seaweed[i].speed * 100;
        // Protect against integer overflow
        s_seaweed[i].offset %= TRIG_MAX_ANGLE;
    }
    
    // Update bubbles
    for (int i = 0; i < MAX_BUBBLES; i++) {
        if (s_bubbles[i].active) {
            s_bubbles[i].pos.y -= s_bubbles[i].speed;
            
            // Slight x wobble
            if (random_in_range(0, 2) == 0) {
                s_bubbles[i].pos.x += random_in_range(-1, 1);
            }
            
            // Remove bubble when it reaches the top
            if (s_bubbles[i].pos.y < 0) {
                s_bubbles[i].active = false;
            }
        } else if (random_in_range(0, 199) < 2) {  // Reduced chance to 1% from 2%
            init_bubble(&s_bubbles[i]);
        }
    }
    
    // Update plankton
    for (int i = 0; i < MAX_PLANKTON; i++) {
        if (s_plankton[i].active) {
            // Random movement for plankton
            if (random_in_range(0, 3) == 0) {
                s_plankton[i].pos.x += random_in_range(-1, 1);
                s_plankton[i].pos.y += random_in_range(-1, 1);
            }
            
            // Keep plankton in bounds
            if (s_plankton[i].pos.x < 0) s_plankton[i].pos.x = 0;
            if (s_plankton[i].pos.x > 144) s_plankton[i].pos.x = 144;
            if (s_plankton[i].pos.y < 0) s_plankton[i].pos.y = 0;
            if (s_plankton[i].pos.y > 168) s_plankton[i].pos.y = 168;
        } else if (random_in_range(0, 199) < 3) {  // Reduced chance to 1.5%
            init_plankton(&s_plankton[i]);
        }
    }
    
    // Update turtle
    for (int i = 0; i < MAX_TURTLES; i++) {
        update_turtle(&s_turtles[i]);
    }
    
    // Update jellyfish
    for (int i = 0; i < MAX_JELLYFISH; i++) {
        update_jellyfish(&s_jellyfish[i]);
    }
    
    // Update octopus
    update_octopus(&s_octopus);
    
    // Update shark
    if (s_shark.active) {
        // Move shark
        s_shark.pos.x += s_shark.direction * s_shark.speed;
        
        // Check for shark eating fish - limit checks per frame
        int fish_eaten = 0;
        for (int i = 0; i < MAX_FISH + MAX_BIG_FISH && fish_eaten < 2; i++) {
            if (s_fish[i].active) {
                if (abs(s_shark.pos.x - s_fish[i].pos.x) < 20 && 
                    abs(s_shark.pos.y - s_fish[i].pos.y) < 12) {
                    s_fish[i].active = false;  // Fish gets eaten
                    fish_eaten++;
                    
                    // Create bubbles to mark the eating event - limit bubbles
                    int bubbles_created = 0;
                    for (int b = 0; b < MAX_BUBBLES && bubbles_created < 3; b++) {
                        if (!s_bubbles[b].active) {
                            s_bubbles[b].pos = s_fish[i].pos;
                            s_bubbles[b].size = random_in_range(1, 3);
                            s_bubbles[b].speed = random_in_range(1, 3);
                            s_bubbles[b].active = true;
                            bubbles_created++;
                        }
                    }
                }
            }
        }
        
        // Remove shark if it swims off screen
        if ((s_shark.direction == 1 && s_shark.pos.x > 174) ||
            (s_shark.direction == -1 && s_shark.pos.x < -30)) {
            s_shark.active = false;
            s_shark.timer = random_in_range(200, 500);  // Reduced cooldown before next appearance
        }
    } else {
        // Countdown to shark appearance
        s_shark.timer--;
        if (s_shark.timer <= 0) {
            // Time for shark to appear!
            init_shark(&s_shark);
            s_shark.active = true;
        }
    }
    
    // Update seahorse - only animate, never disappear
    update_seahorse(&s_seahorse);
    
    // Update crab
    update_crab(&s_crab);
    
    // Update clam
    update_clam(&s_clam);
    
    if (s_canvas_layer) {
        layer_mark_dirty(s_canvas_layer);
    }
}

// Update time display
static void update_time(void) {
    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);
    
    if (!tick_time) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to get local time");
        return;
    }
    
    if (!s_time_layer || !s_date_layer) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Time or date layer not initialized");
        return;
    }
    
    static char s_time_buffer[8];
    strftime(s_time_buffer, sizeof(s_time_buffer), "%I:%M", tick_time);  // Changed to 12-hour format
    text_layer_set_text(s_time_layer, s_time_buffer);
    
    static char s_date_buffer[24];
    strftime(s_date_buffer, sizeof(s_date_buffer), "%a, %b %d", tick_time);  // Added day of week
    text_layer_set_text(s_date_layer, s_date_buffer);
}

// Time tick handler
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
    update_time();
}

// Animation timer callback
static void animation_timer_callback(void *data) {
    animation_update();
    
    // Register next timer and store the handle
    // Use slower animation interval if battery is low
    uint32_t next_interval = (s_battery_level <= LOW_BATTERY_THRESHOLD && !s_is_charging) ? 
                             ANIMATION_INTERVAL_LOW_POWER : ANIMATION_INTERVAL;
    
    // Important: null check before registering new timer
    AppTimer *new_timer = app_timer_register(next_interval, animation_timer_callback, NULL);
    if (new_timer) {
        s_animation_timer = new_timer;
    } else {
        // Timer creation failed - attempt recovery
        s_animation_timer = app_timer_register(next_interval * 2, animation_timer_callback, NULL);
    }
}

// Update crab animation
static void update_crab(Crab *crab) {
    // Move side to side
    crab->pos.x += crab->direction * crab->speed;
    
    // Animate claws
    crab->claw_state = (crab->claw_state + 1) % 20;
    
    // Reverse direction at screen edges
    if (crab->pos.x <= 15 || crab->pos.x >= 130) {
        crab->direction *= -1;
    }
}

// Update clam animation
static void update_clam(Clam *clam) {
    // Occasionally open and close
    if (clam->open_state > 0) {
        clam->open_state--;
    } else if (random_in_range(0, 399) == 0) {  // Rare opening (every ~20 seconds)
        clam->open_state = 40;  // Stay open for 2 seconds
    }
}

// Battery state handler
static void battery_callback(BatteryChargeState charge_state) {
    s_battery_level = charge_state.charge_percent;
    s_is_charging = charge_state.is_charging;
    
    // Request redraw of battery indicator
    if (s_battery_layer) {
        layer_mark_dirty(s_battery_layer);
    }
}

// Update turtle
static void update_turtle(Turtle *turtle) {
    turtle->pos.x += turtle->direction * turtle->speed;
    turtle->animation_offset += turtle->speed * 200;
    // Protect against integer overflow
    turtle->animation_offset %= TRIG_MAX_ANGLE;
    
    // Reset turtle if it swims off screen
    if ((turtle->direction == 1 && turtle->pos.x > 144) ||
        (turtle->direction == -1 && turtle->pos.x < -15)) {
        init_turtle(turtle);
    }
}

// Update jellyfish
static void update_jellyfish(Jellyfish *jellyfish) {
    jellyfish->tentacle_offset += jellyfish->speed * 100;
    // Protect against integer overflow
    jellyfish->tentacle_offset %= TRIG_MAX_ANGLE;
    
    // Update pulse animation
    jellyfish->pulse_state = (jellyfish->pulse_state + 1) % 100;
    
    // Move jellyfish slightly up when pulsing (middle of animation)
    if (jellyfish->pulse_state == 50) {
        jellyfish->pos.y -= 2;
        if (jellyfish->pos.y < 60) jellyfish->pos.y = 60;
    }
    
    // Random side movement
    if (random_in_range(0, 19) == 0) {
        jellyfish->pos.x += random_in_range(-1, 1);
        
        // Keep in bounds
        if (jellyfish->pos.x < 10) jellyfish->pos.x = 10;
        if (jellyfish->pos.x > 134) jellyfish->pos.x = 134;
    }
}

// Update octopus
static void update_octopus(Octopus *octopus) {
    octopus->tentacle_offset += octopus->speed * 50;
    // Protect against integer overflow
    octopus->tentacle_offset %= TRIG_MAX_ANGLE;
    
    // Slow movement for octopus
    if (random_in_range(0, 9) == 0) {
        octopus->pos.x += random_in_range(-1, 1);
        
        // Keep octopus in bounds
        if (octopus->pos.x < 10) octopus->pos.x = 10;
        if (octopus->pos.x > 134) octopus->pos.x = 134;
    }
}

static void main_window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);
    
    // Create canvas layer
    s_canvas_layer = layer_create(bounds);
    if (!s_canvas_layer) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create canvas layer");
        return;
    }
    layer_set_update_proc(s_canvas_layer, canvas_update_proc);
    layer_add_child(window_layer, s_canvas_layer);
    
    // Create time layer
    GRect time_frame;
    time_frame.origin.x = 0;
    time_frame.origin.y = 40;
    time_frame.size.w = bounds.size.w;
    time_frame.size.h = 34;
    s_time_layer = text_layer_create(time_frame);
    if (!s_time_layer) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create time layer");
        return;
    }
    
    text_layer_set_text_color(s_time_layer, GColorWhite);
    text_layer_set_background_color(s_time_layer, GColorClear);
    text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
    text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
    layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
    
    // Create date layer
    GRect date_frame;
    date_frame.origin.x = 0;
    date_frame.origin.y = 74;
    date_frame.size.w = bounds.size.w;
    date_frame.size.h = 20;
    s_date_layer = text_layer_create(date_frame);
    if (!s_date_layer) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create date layer");
        return;
    }
    
    text_layer_set_text_color(s_date_layer, GColorWhite);
    text_layer_set_background_color(s_date_layer, GColorClear);
    text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
    text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
    layer_add_child(window_layer, text_layer_get_layer(s_date_layer));
    
    // Create battery layer
    GRect battery_frame;
    battery_frame.origin.x = bounds.size.w - 25;
    battery_frame.origin.y = 5;
    battery_frame.size.w = 20;
    battery_frame.size.h = 8;
    s_battery_layer = layer_create(battery_frame);
    if (!s_battery_layer) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create battery layer");
        return;
    }
    
    layer_set_update_proc(s_battery_layer, battery_update_proc);
    layer_add_child(window_layer, s_battery_layer);
    
    // Initialize small fish
    for (int i = 0; i < MAX_FISH; i++) {
        init_fish(&s_fish[i], 1);  // Small fish
    }
    
    // Initialize big fish
    for (int i = MAX_FISH; i < MAX_FISH + MAX_BIG_FISH; i++) {
        init_fish(&s_fish[i], 2);  // Big fish
    }
    
    // Initialize seaweed
    for (int i = 0; i < MAX_SEAWEED; i++) {
        init_seaweed(&s_seaweed[i], 20 + (i * 35));
    }
    
    // Initialize bubbles
    for (int i = 0; i < MAX_BUBBLES; i++) {
        s_bubbles[i].active = false;  // Start with no bubbles
    }
    
    // Initialize plankton
    for (int i = 0; i < MAX_PLANKTON; i++) {
        if (random_in_range(0, 2) == 0) {  // Start with some plankton
            init_plankton(&s_plankton[i]);
        } else {
            s_plankton[i].active = false;
        }
    }
    
    // Initialize octopus
    init_octopus(&s_octopus);
    
    // Initialize turtle
    for (int i = 0; i < MAX_TURTLES; i++) {
        init_turtle(&s_turtles[i]);
    }
    
    // Initialize jellyfish
    for (int i = 0; i < MAX_JELLYFISH; i++) {
        init_jellyfish(&s_jellyfish[i]);
    }
    
    // Initialize shark
    init_shark(&s_shark);
    
    // Initialize seahorse
    init_seahorse(&s_seahorse);
    
    // Initialize crab
    init_crab(&s_crab);
    
    // Initialize clam
    init_clam(&s_clam);
    
    // Create and initialize paths once
    // Fish tail path
    GPathInfo fish_tail_info = {
        .num_points = 3,
        .points = s_fish_tail_points,
    };
    s_fish_tail_path = gpath_create(&fish_tail_info);
    
    // Turtle flipper paths
    GPathInfo turtle_front_flipper_info = {
        .num_points = 3,
        .points = s_turtle_front_flipper_points,
    };
    s_turtle_front_flipper_path = gpath_create(&turtle_front_flipper_info);
    
    GPathInfo turtle_back_flipper_info = {
        .num_points = 3,
        .points = s_turtle_back_flipper_points,
    };
    s_turtle_back_flipper_path = gpath_create(&turtle_back_flipper_info);
    
    // Shark paths
    GPathInfo shark_body_info = {
        .num_points = 5,
        .points = s_shark_body_points,
    };
    s_shark_body_path = gpath_create(&shark_body_info);
    
    GPathInfo shark_tail_info = {
        .num_points = 3,
        .points = s_shark_tail_points,
    };
    s_shark_tail_path = gpath_create(&shark_tail_info);
    
    GPathInfo shark_fin_info = {
        .num_points = 3,
        .points = s_shark_fin_points,
    };
    s_shark_fin_path = gpath_create(&shark_fin_info);
    
    // Start animation timer with error checking
    s_animation_timer = app_timer_register(ANIMATION_INTERVAL, animation_timer_callback, NULL);
    if (!s_animation_timer) {
        // If timer creation fails, try again with longer interval
        s_animation_timer = app_timer_register(ANIMATION_INTERVAL * 2, animation_timer_callback, NULL);
    }
    
    // Update time immediately
    update_time();
}

static void main_window_unload(Window *window) {
    // Cancel the animation timer if it exists
    if (s_animation_timer) {
        app_timer_cancel(s_animation_timer);
        s_animation_timer = NULL;
    }
    
    // Clean up path resources
    if (s_fish_tail_path) {
        gpath_destroy(s_fish_tail_path);
        s_fish_tail_path = NULL;
    }
    
    if (s_turtle_front_flipper_path) {
        gpath_destroy(s_turtle_front_flipper_path);
        s_turtle_front_flipper_path = NULL;
    }
    
    if (s_turtle_back_flipper_path) {
        gpath_destroy(s_turtle_back_flipper_path);
        s_turtle_back_flipper_path = NULL;
    }
    
    if (s_shark_body_path) {
        gpath_destroy(s_shark_body_path);
        s_shark_body_path = NULL;
    }
    
    if (s_shark_tail_path) {
        gpath_destroy(s_shark_tail_path);
        s_shark_tail_path = NULL;
    }
    
    if (s_shark_fin_path) {
        gpath_destroy(s_shark_fin_path);
        s_shark_fin_path = NULL;
    }
    
    if (s_canvas_layer) {
        layer_destroy(s_canvas_layer);
        s_canvas_layer = NULL;
    }
    
    if (s_time_layer) {
        text_layer_destroy(s_time_layer);
        s_time_layer = NULL;
    }
    
    if (s_date_layer) {
        text_layer_destroy(s_date_layer);
        s_date_layer = NULL;
    }
    
    if (s_battery_layer) {
        layer_destroy(s_battery_layer);
        s_battery_layer = NULL;
    }
}

static void init(void) {
    srand(time(NULL));  // Initialize random seed
    
    // Initialize timer handle to NULL
    s_animation_timer = NULL;
    
    // Create main window
    s_main_window = window_create();
    if (!s_main_window) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create main window");
        return;
    }
    
    // Set window handlers
    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load = main_window_load,
        .unload = main_window_unload
    });
    
    // Show the window
    window_stack_push(s_main_window, true);
    
    // Register services
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
    battery_state_service_subscribe(battery_callback);  // Use callback function
    
    // Get initial battery state
    s_battery_level = battery_state_service_peek().charge_percent;
    s_is_charging = battery_state_service_peek().is_charging;
}

static void deinit(void) {
    // Cancel the animation timer if it exists
    if (s_animation_timer) {
        app_timer_cancel(s_animation_timer);
        s_animation_timer = NULL;
    }
    
    // Clean up path resources
    if (s_fish_tail_path) {
        gpath_destroy(s_fish_tail_path);
        s_fish_tail_path = NULL;
    }
    
    if (s_turtle_front_flipper_path) {
        gpath_destroy(s_turtle_front_flipper_path);
        s_turtle_front_flipper_path = NULL;
    }
    
    if (s_turtle_back_flipper_path) {
        gpath_destroy(s_turtle_back_flipper_path);
        s_turtle_back_flipper_path = NULL;
    }
    
    if (s_shark_body_path) {
        gpath_destroy(s_shark_body_path);
        s_shark_body_path = NULL;
    }
    
    if (s_shark_tail_path) {
        gpath_destroy(s_shark_tail_path);
        s_shark_tail_path = NULL;
    }
    
    if (s_shark_fin_path) {
        gpath_destroy(s_shark_fin_path);
        s_shark_fin_path = NULL;
    }
    
    if (s_main_window) {
        window_destroy(s_main_window);
        s_main_window = NULL;
    }
    
    tick_timer_service_unsubscribe();
    battery_state_service_unsubscribe();  // Unsubscribe from battery service
}

int main(void) {
    init();
    app_event_loop();
    deinit();
    return 0;
}

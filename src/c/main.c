#include <pebble.h>

// Structures for animated elements
typedef struct {
    GPoint pos;
    int direction;  // 1 for right, -1 for left
    int speed;
    bool active;    // Whether the fish is alive/visible
    int size;       // Size of the fish (1 = small, 2 = big)
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

// UI Elements
static Window *s_main_window;
static Layer *s_canvas_layer;
static TextLayer *s_time_layer;
static TextLayer *s_date_layer;
static Layer *s_battery_layer;

// Animation elements
#define MAX_FISH 5         // Increased for more fish
#define MAX_BIG_FISH 2     // Big fish that eat small fish
#define MAX_SEAWEED 4
#define MAX_BUBBLES 8
#define MAX_PLANKTON 6
static Fish s_fish[MAX_FISH + MAX_BIG_FISH];  // Combined array for all fish
static Seaweed s_seaweed[MAX_SEAWEED];
static Bubble s_bubbles[MAX_BUBBLES];
static Plankton s_plankton[MAX_PLANKTON];
static Octopus s_octopus;

// Update frequency
#define ANIMATION_INTERVAL 50

// Initialize a fish with random position and speed
static void init_fish(Fish *fish, int size) {
    fish->pos.y = 20 + (rand() % 100);
    fish->direction = (rand() % 2) * 2 - 1;  // Either 1 or -1
    fish->speed = size == 1 ? (2 + (rand() % 3)) : (1 + (rand() % 2));  // Big fish are slower
    fish->pos.x = (fish->direction == 1) ? -10 : 144;  // Use screen width constant
    fish->active = true;
    fish->size = size;
}

// Initialize seaweed with base position
static void init_seaweed(Seaweed *seaweed, int x) {
    seaweed->base.x = x;
    seaweed->base.y = 168;  // Screen height
    seaweed->offset = 0;
    seaweed->speed = 1 + (rand() % 2);
}

// Initialize bubble
static void init_bubble(Bubble *bubble) {
    bubble->pos.x = rand() % 144;  // Random x position
    bubble->pos.y = 168;           // Start at bottom
    bubble->size = 1 + (rand() % 3);
    bubble->speed = 1 + (rand() % 3);
    bubble->active = true;
}

// Initialize plankton
static void init_plankton(Plankton *plankton) {
    plankton->pos.x = rand() % 144;
    plankton->pos.y = 20 + (rand() % 120);
    plankton->direction = (rand() % 2) * 2 - 1;  // Either 1 or -1
    plankton->speed = 1 + (rand() % 2);
    plankton->active = true;
}

// Initialize octopus
static void init_octopus(Octopus *octopus) {
    octopus->pos.x = rand() % 70 + 37;  // Somewhere in the middle
    octopus->pos.y = 140;               // Near the bottom
    octopus->direction = (rand() % 2) * 2 - 1;
    octopus->tentacle_offset = 0;
    octopus->speed = 1;
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
    GPoint tail_points[3];
    tail_points[0].x = fish->pos.x - (fish->direction * size);
    tail_points[0].y = fish->pos.y;
    tail_points[1].x = fish->pos.x - (fish->direction * (size * 2));
    tail_points[1].y = fish->pos.y - size;
    tail_points[2].x = fish->pos.x - (fish->direction * (size * 2));
    tail_points[2].y = fish->pos.y + size;
    
    // Create path for tail
    GPathInfo path_info;
    path_info.num_points = 3;
    path_info.points = tail_points;
    
    GPath *path = gpath_create(&path_info);
    gpath_draw_filled(ctx, path);
    gpath_destroy(path);
    
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

// Check if two elements collide (basic circle collision)
static bool check_collision(GPoint pos1, int radius1, GPoint pos2, int radius2) {
    int dx = pos1.x - pos2.x;
    int dy = pos1.y - pos2.y;
    int distance_squared = (dx * dx) + (dy * dy);
    int radius_sum = radius1 + radius2;
    return distance_squared <= (radius_sum * radius_sum);
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
    
    // Draw plankton
    for (int i = 0; i < MAX_PLANKTON; i++) {
        draw_plankton(ctx, &s_plankton[i]);
    }
    
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
        
        // Big fish eat small fish
        if (s_fish[i].size > 1) {  // If this is a big fish
            for (int j = 0; j < MAX_FISH; j++) {  // Check all small fish
                if (s_fish[j].active && s_fish[j].size == 1) {
                    if (check_collision(s_fish[i].pos, 7, s_fish[j].pos, 4)) {
                        s_fish[j].active = false;  // Small fish gets eaten
                        
                        // Create bubbles to mark the eating event
                        for (int b = 0; b < 3; b++) {
                            for (int k = 0; k < MAX_BUBBLES; k++) {
                                if (!s_bubbles[k].active) {
                                    s_bubbles[k].pos = s_fish[j].pos;
                                    s_bubbles[k].size = 1 + (rand() % 2);
                                    s_bubbles[k].speed = 1 + (rand() % 2);
                                    s_bubbles[k].active = true;
                                    break;
                                }
                            }
                        }
                        
                        // Respawn the eaten fish after a short delay (handled elsewhere)
                        // We'll use the inactive state as a marker for respawn logic
                    }
                }
            }
        }
    }
    
    // Check for respawning fish
    for (int i = 0; i < MAX_FISH; i++) {
        if (!s_fish[i].active && (rand() % 50) == 0) {
            init_fish(&s_fish[i], 1);  // Reinitialize with size 1 (small fish)
        }
    }
    
    // Update seaweed animation
    for (int i = 0; i < MAX_SEAWEED; i++) {
        s_seaweed[i].offset += s_seaweed[i].speed * 100;
        if (s_seaweed[i].offset >= TRIG_MAX_ANGLE) {
            s_seaweed[i].offset -= TRIG_MAX_ANGLE;
        }
    }
    
    // Update bubbles
    for (int i = 0; i < MAX_BUBBLES; i++) {
        if (s_bubbles[i].active) {
            s_bubbles[i].pos.y -= s_bubbles[i].speed;
            
            // Slight x wobble
            if (rand() % 3 == 0) {
                s_bubbles[i].pos.x += (rand() % 3) - 1;
            }
            
            // Remove bubble when it reaches the top
            if (s_bubbles[i].pos.y < 0) {
                s_bubbles[i].active = false;
            }
        } else if (rand() % 100 < 2) {  // 2% chance to create a new bubble
            init_bubble(&s_bubbles[i]);
        }
    }
    
    // Update plankton
    for (int i = 0; i < MAX_PLANKTON; i++) {
        if (s_plankton[i].active) {
            // Random movement for plankton
            if (rand() % 4 == 0) {
                s_plankton[i].pos.x += (rand() % 3) - 1;
                s_plankton[i].pos.y += (rand() % 3) - 1;
            }
            
            // Keep plankton in bounds
            if (s_plankton[i].pos.x < 0) s_plankton[i].pos.x = 0;
            if (s_plankton[i].pos.x > 144) s_plankton[i].pos.x = 144;
            if (s_plankton[i].pos.y < 0) s_plankton[i].pos.y = 0;
            if (s_plankton[i].pos.y > 168) s_plankton[i].pos.y = 168;
        } else if (rand() % 100 < 3) {  // 3% chance to create new plankton
            init_plankton(&s_plankton[i]);
        }
    }
    
    // Update octopus
    s_octopus.tentacle_offset += s_octopus.speed * 50;
    if (s_octopus.tentacle_offset >= TRIG_MAX_ANGLE) {
        s_octopus.tentacle_offset -= TRIG_MAX_ANGLE;
    }
    
    // Slow movement for octopus
    if (rand() % 10 == 0) {
        s_octopus.pos.x += (rand() % 3) - 1;
        
        // Keep octopus in bounds
        if (s_octopus.pos.x < 10) s_octopus.pos.x = 10;
        if (s_octopus.pos.x > 134) s_octopus.pos.x = 134;
    }
    
    layer_mark_dirty(s_canvas_layer);
}

// Update time display
static void update_time(void) {
    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);
    
    static char s_time_buffer[8];
    strftime(s_time_buffer, sizeof(s_time_buffer), "%H:%M", tick_time);
    text_layer_set_text(s_time_layer, s_time_buffer);
    
    static char s_date_buffer[16];
    strftime(s_date_buffer, sizeof(s_date_buffer), "%b %d", tick_time);
    text_layer_set_text(s_date_layer, s_date_buffer);
}

// Time tick handler
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
    update_time();
}

// Animation timer callback
static void animation_timer_callback(void *data) {
    animation_update();
    app_timer_register(ANIMATION_INTERVAL, animation_timer_callback, NULL);
}

static void main_window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);
    
    // Create canvas layer
    s_canvas_layer = layer_create(bounds);
    layer_set_update_proc(s_canvas_layer, canvas_update_proc);
    layer_add_child(window_layer, s_canvas_layer);
    
    // Create time layer
    GRect time_frame;
    time_frame.origin.x = 0;
    time_frame.origin.y = 40;
    time_frame.size.w = bounds.size.w;
    time_frame.size.h = 34;
    s_time_layer = text_layer_create(time_frame);
    
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
        if (rand() % 3 == 0) {  // Start with some plankton
            init_plankton(&s_plankton[i]);
        } else {
            s_plankton[i].active = false;
        }
    }
    
    // Initialize octopus
    init_octopus(&s_octopus);
    
    // Start animation timer
    app_timer_register(ANIMATION_INTERVAL, animation_timer_callback, NULL);
    
    // Update time immediately
    update_time();
}

static void main_window_unload(Window *window) {
    layer_destroy(s_canvas_layer);
    text_layer_destroy(s_time_layer);
    text_layer_destroy(s_date_layer);
    layer_destroy(s_battery_layer);
}

static void init(void) {
    srand(time(NULL));  // Initialize random seed
    
    // Create main window
    s_main_window = window_create();
    
    // Set window handlers
    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load = main_window_load,
        .unload = main_window_unload
    });
    
    // Show the window
    window_stack_push(s_main_window, true);
    
    // Register services
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
    battery_state_service_subscribe(NULL);  // Subscribe to battery service
}

static void deinit(void) {
    window_destroy(s_main_window);
    tick_timer_service_unsubscribe();
    battery_state_service_unsubscribe();  // Unsubscribe from battery service
}

int main(void) {
    init();
    app_event_loop();
    deinit();
    return 0;
}

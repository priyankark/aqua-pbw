#include <pebble.h>

// Structures for animated elements
typedef struct {
    GPoint pos;
    int direction;  // 1 for right, -1 for left
    int speed;
} Fish;

typedef struct {
    GPoint base;
    int offset;
    int speed;
} Seaweed;

// UI Elements
static Window *s_main_window;
static Layer *s_canvas_layer;
static TextLayer *s_time_layer;
static TextLayer *s_date_layer;
static Layer *s_battery_layer;

// Animation elements
#define MAX_FISH 3
#define MAX_SEAWEED 4
static Fish s_fish[MAX_FISH];
static Seaweed s_seaweed[MAX_SEAWEED];

// Update frequency
#define ANIMATION_INTERVAL 50

// Initialize a fish with random position and speed
static void init_fish(Fish *fish) {
    fish->pos.y = 20 + (rand() % 100);
    fish->direction = (rand() % 2) * 2 - 1;  // Either 1 or -1
    fish->speed = 2 + (rand() % 3);
    fish->pos.x = (fish->direction == 1) ? -10 : 144;  // Use screen width constant
}

// Initialize seaweed with base position
static void init_seaweed(Seaweed *seaweed, int x) {
    seaweed->base.x = x;
    seaweed->base.y = 168;  // Screen height
    seaweed->offset = 0;
    seaweed->speed = 1 + (rand() % 2);
}

// Draw fish
static void draw_fish(GContext *ctx, const Fish *fish) {
    // Set fill color (white for B&W displays)
    graphics_context_set_fill_color(ctx, GColorWhite);
    
    // Fish body - using GPoint directly as required by Diorite
    graphics_fill_circle(ctx, fish->pos, 4);
    
    // Tail
    GPoint tail_points[3];
    tail_points[0].x = fish->pos.x - (fish->direction * 4);
    tail_points[0].y = fish->pos.y;
    tail_points[1].x = fish->pos.x - (fish->direction * 8);
    tail_points[1].y = fish->pos.y - 3;
    tail_points[2].x = fish->pos.x - (fish->direction * 8);
    tail_points[2].y = fish->pos.y + 3;
    
    // Create path for tail
    GPathInfo path_info;
    path_info.num_points = 3;
    path_info.points = tail_points;
    
    GPath *path = gpath_create(&path_info);
    gpath_draw_filled(ctx, path);
    gpath_destroy(path);
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
    
    // Draw fish (foreground)
    for (int i = 0; i < MAX_FISH; i++) {
        draw_fish(ctx, &s_fish[i]);
    }
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
    // Update fish positions
    for (int i = 0; i < MAX_FISH; i++) {
        s_fish[i].pos.x += s_fish[i].direction * s_fish[i].speed;
        
        // Reset fish if it swims off screen
        if ((s_fish[i].direction == 1 && s_fish[i].pos.x > 144) ||  // Use screen width
            (s_fish[i].direction == -1 && s_fish[i].pos.x < -10)) {
            init_fish(&s_fish[i]);
        }
    }
    
    // Update seaweed animation
    for (int i = 0; i < MAX_SEAWEED; i++) {
        s_seaweed[i].offset += s_seaweed[i].speed * 100;
        if (s_seaweed[i].offset >= TRIG_MAX_ANGLE) {
            s_seaweed[i].offset -= TRIG_MAX_ANGLE;
        }
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
    
    // Initialize fish
    for (int i = 0; i < MAX_FISH; i++) {
        init_fish(&s_fish[i]);
    }
    
    // Initialize seaweed
    for (int i = 0; i < MAX_SEAWEED; i++) {
        init_seaweed(&s_seaweed[i], 20 + (i * 35));
    }
    
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

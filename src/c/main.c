#include <pebble.h>

// Define constants
#define NUM_SPIRALS 12           // More concentric spiral circles
#define SPIRAL_THICKNESS 1       // Thin lines for elegant look
#define POINTS_PER_CIRCLE 120    // Points per circle for smoothness
#define ANIMATION_DURATION 20000 // 20 seconds for a full rotation
#define SPIRAL_DISTORTION 3      // Amount of distortion for spirals (higher = more disturbing)
#define DISTURB_FREQUENCY 8      // How often to apply distortion (higher = less frequent)

// Window and layers
static Window *s_main_window;
static Layer *s_canvas_layer;
static TextLayer *s_time_layer;
static TextLayer *s_date_layer;
static Layer *s_battery_layer;
static BatteryChargeState s_battery_state;

// Animation
static Animation *s_animation;
static AnimationImplementation s_animation_impl;
static int s_animation_percent = 0;

// Update the animation percentage
static void animation_update(Animation *animation, const AnimationProgress progress) {
  s_animation_percent = ((progress * 100) / ANIMATION_NORMALIZED_MAX);
  layer_mark_dirty(s_canvas_layer);
}

// Draw battery indicator in Uzumaki style
static void battery_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  
  // Draw a spiral-themed battery indicator
  int width = bounds.size.w * s_battery_state.charge_percent / 100;
  
  // Battery outline
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_round_rect(ctx, GRect(0, 0, bounds.size.w, bounds.size.h), 3);
  
  // Battery fill - use spiral pattern for fill
  if (width > 0) {
    // Draw small spiral segments as battery fill
    graphics_context_set_fill_color(ctx, GColorWhite);
    
    for (int i = 0; i < width; i += 3) {
      int height = bounds.size.h;
      graphics_fill_rect(ctx, GRect(i, 0, 2, height), 0, GCornerNone);
    }
    
    // Add spiral icon inside battery
    if (s_battery_state.charge_percent > 20) {
      int center_x = width / 2;
      int center_y = bounds.size.h / 2;
      int max_radius = (bounds.size.h / 2) - 2;
      
      // Draw a mini spiral
      for (int r = 1; r <= max_radius; r += 2) {
        graphics_context_set_stroke_color(ctx, GColorBlack);
        graphics_context_set_stroke_width(ctx, 1);
        graphics_draw_circle(ctx, GPoint(center_x, center_y), r);
      }
    }
  }
  
  // Display percentage next to battery
  static char s_battery_buffer[6];
  snprintf(s_battery_buffer, sizeof(s_battery_buffer), "%d%%", s_battery_state.charge_percent);
  
  // Draw percentage text
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, 
                    s_battery_buffer, 
                    fonts_get_system_font(FONT_KEY_GOTHIC_14),
                    GRect(-45, 0, 40, bounds.size.h),
                    GTextOverflowModeTrailingEllipsis,
                    GTextAlignmentRight,
                    NULL);
}

// Battery state change callback
static void battery_callback(BatteryChargeState state) {
  s_battery_state = state;
  layer_mark_dirty(s_battery_layer);
}

// Draw a disturbing concentric circle with spiral effect
static void draw_spiral_circle(GContext *ctx, int center_x, int center_y, int radius, 
                              int rotation_offset, GColor color, int line_width,
                              int spiral_index) {
  // Set drawing properties
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, line_width);
  
  // Calculate the points around the circle (more points = smoother)
  GPoint points[POINTS_PER_CIRCLE + 1];
  
  for (int i = 0; i <= POINTS_PER_CIRCLE; i++) {
    // Calculate the angle with rotation offset
    int32_t angle = (i * TRIG_MAX_ANGLE / POINTS_PER_CIRCLE) + rotation_offset;
    
    // Keep angle in valid range
    angle = angle % TRIG_MAX_ANGLE;
    
    // Apply Uzumaki-style distortion
    // More distortion for inner circles (more disturbing in center)
    int distortion_factor = SPIRAL_DISTORTION * (NUM_SPIRALS - spiral_index) / NUM_SPIRALS;
    int distortion = 0;
    
    // Only distort at certain points for more unsettling effect
    if (i % DISTURB_FREQUENCY == 0) {
      // Create glitch/distortion based on animation progress
      int anim_phase = (s_animation_percent + spiral_index * 10) % 100;
      if (anim_phase < 50) {
        distortion = distortion_factor;
      } else {
        distortion = -distortion_factor;
      }
    }
    
    // Calculate point position on circle with distortion
    int mod_radius = radius + distortion;
    int x = center_x + (mod_radius * cos_lookup(angle) / TRIG_MAX_RATIO);
    int y = center_y + (mod_radius * sin_lookup(angle) / TRIG_MAX_RATIO);
    
    // Store the point
    points[i] = GPoint(x, y);
  }
  
  // Draw the circle as a series of connected line segments
  for (int i = 1; i <= POINTS_PER_CIRCLE; i++) {
    graphics_draw_line(ctx, points[i-1], points[i]);
  }
}

// Draw a pulsing shadow effect behind the time
static void draw_time_shadow(GContext *ctx, GRect bounds) {
  int center_x = bounds.size.w / 2;
  int center_y = bounds.size.h / 2;
  
  // Pulsing shadow effect
  int pulse = (s_animation_percent * 10) % 100;
  int radius = 50 + (pulse < 50 ? pulse : 100 - pulse) / 2; // Makes it pulse between 50-75
  
  // Draw a semi-transparent shadow disc
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  for (int r = radius; r > 0; r -= 10) {
    graphics_fill_circle(ctx, GPoint(center_x, center_y), r);
  }
  graphics_context_set_compositing_mode(ctx, GCompOpAssign);
}

// Update procedure for the canvas layer
static void canvas_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  
  // Clear the background to black
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  
  // Center point
  int center_x = bounds.size.w / 2;
  int center_y = bounds.size.h / 2;
  int min_dimension = bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h;
  
  // Calculate the animation rotation offset
  int32_t rotation_base = (s_animation_percent * TRIG_MAX_ANGLE) / 100;
  
  // Draw time shadow for better visibility
  draw_time_shadow(ctx, bounds);
  
  // Draw concentric spiral circles
  for (int i = 0; i < NUM_SPIRALS; i++) {
    // Calculate radius for this circle
    int radius = 10 + (i * ((min_dimension / 2 - 15) / NUM_SPIRALS));
    
    // Create more interesting contrast with varying shades
    GColor color;
    if (i % 3 == 0) {
      color = GColorWhite;
    } else if (i % 3 == 1) {
      color = GColorLightGray;
    } else {
      color = GColorDarkGray;
    }
    
    // Each circle rotates at a different speed and direction
    int32_t rotation = rotation_base;
    
    // Every other circle rotates opposite direction
    if (i % 2 == 1) {
      rotation = TRIG_MAX_ANGLE - rotation;
    }
    
    // Add phase offset to create spiral effect between circles
    rotation += (i * TRIG_MAX_ANGLE / (NUM_SPIRALS * 2));
    
    // Every third circle rotates faster
    if (i % 3 == 0) {
      rotation = (rotation * 3) % TRIG_MAX_ANGLE;
    }
    
    // Draw this circular spiral
    draw_spiral_circle(ctx, center_x, center_y, radius, rotation, color, SPIRAL_THICKNESS, i);
  }
  
  // Add dramatic spiral corner accents 
  int corner_size = min_dimension / 6;
  
  // Top left spiral
  draw_spiral_circle(ctx, corner_size/2, corner_size/2, corner_size, 
                   rotation_base, GColorWhite, SPIRAL_THICKNESS, 0);
  
  // Bottom right spiral
  draw_spiral_circle(ctx, bounds.size.w - corner_size/2, bounds.size.h - corner_size/2, 
                   corner_size, -rotation_base, GColorWhite, SPIRAL_THICKNESS, 0);
}

// Update the time display
static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  
  // Create buffers
  static char time_buffer[8];
  static char date_buffer[16];
  
  // Write the current time into the buffer
  if (clock_is_24h_style() == true) {
    strftime(time_buffer, sizeof(time_buffer), "%H:%M", tick_time);
  } else {
    strftime(time_buffer, sizeof(time_buffer), "%I:%M", tick_time);
  }
  
  // Write the current date into the buffer with proper formatting
  strftime(date_buffer, sizeof(date_buffer), "%a %d %b", tick_time);
  
  // Display on the TextLayers
  text_layer_set_text(s_time_layer, time_buffer);
  text_layer_set_text(s_date_layer, date_buffer);
}

// Handler for time tick events
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

// Animation completed callback
static void animation_stopped(Animation *animation, bool finished, void *context) {
  // Schedule next animation for infinite looping
  if (finished) {
    animation_schedule(s_animation);
  }
}

// Initialize the animation
static void init_animation() {
  // Create the animation
  s_animation = animation_create();
  animation_set_delay(s_animation, 0);
  animation_set_duration(s_animation, ANIMATION_DURATION);
  
  // Configure the animation implementation
  s_animation_impl.update = animation_update;
  animation_set_implementation(s_animation, &s_animation_impl);
  
  // Set up the animation to repeat
  animation_set_handlers(s_animation, (AnimationHandlers) {
    .stopped = animation_stopped
  }, NULL);
  
  // Schedule the animation
  animation_schedule(s_animation);
}

// Initialize the main window
static void main_window_load(Window *window) {
  // Get information about the Window
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  // Create the canvas layer
  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);
  layer_add_child(window_layer, s_canvas_layer);
  
  // Create the time TextLayer with larger font
  s_time_layer = text_layer_create(GRect(0, bounds.size.h / 2 - 30, bounds.size.w, 60));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
  
  // Create the date TextLayer
  GRect date_frame = GRect(0, bounds.size.h / 2 + 30, bounds.size.w, 25);
  s_date_layer = text_layer_create(date_frame);
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorWhite);
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));
  
  // Create battery meter with spiral theme
  s_battery_layer = layer_create(GRect(bounds.size.w - 50, 10, 40, 14));
  layer_set_update_proc(s_battery_layer, battery_update_proc);
  layer_add_child(window_layer, s_battery_layer);
  
  // Get the initial battery state
  s_battery_state = battery_state_service_peek();
  
  // Initialize and start the animation
  init_animation();
  
  // Make sure the time is displayed from the start
  update_time();
}

// Deinitialize the main window
static void main_window_unload(Window *window) {
  // Destroy TextLayers
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_date_layer);
  
  // Destroy Layers
  layer_destroy(s_canvas_layer);
  layer_destroy(s_battery_layer);
  
  // Destroy animation
  animation_destroy(s_animation);
}

// Initialize the app
static void init() {
  // Create main Window element and assign to pointer
  s_main_window = window_create();
  
  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  
  // Register for battery level updates
  battery_state_service_subscribe(battery_callback);
  
  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);
  
  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

// Deinitialize the app
static void deinit() {
  // Unsubscribe from services
  battery_state_service_unsubscribe();
  tick_timer_service_unsubscribe();
  
  // Destroy Window
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
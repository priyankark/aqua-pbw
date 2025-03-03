# Gears of Time

A clean and elegant watchface featuring smooth rotating spiral animations. The watchface displays time, date, and battery level with a unique spiral theme that creates an engaging visual experience while maintaining perfect clarity.

## Features

- Concentric spiral animations with varying rotation speeds
- Clear time and date display
- Battery indicator with spiral theme
- Memory-efficient animation system

## Project Structure

```
.
├── src/
│   └── c/
│       └── main.c         # Main watchface implementation
├── package.json          # Project metadata and Pebble configuration
└── wscript              # Build system configuration
```

## Build Instructions

The project uses the Rebble tool for building and running. To build the project:

```bash
rebble build
```

This will generate a `build/gears-of-time.pbw` file.

## Running in Emulator

To run the watchface in the emulator:

```bash
rebble install --emulator basalt build/gears-of-time.pbw
```

To interact with the emulator:

```bash
rebble emu-control
```

This will provide a web interface accessible at a local URL (displayed in the terminal) where you can:
- View the watchface
- Test different time formats (12h/24h)
- Simulate different times of day
- Test the watchface on different platforms

## Implementation Details

### Main Components

1. **Spiral System**
   - Concentric spiral circles with smooth animations
   - Variable rotation speeds and directions
   - Contrast-enhancing color variations
   - Corner accent spirals

2. **Time Display**
   - Clear, centered time presentation
   - Date display with day and month
   - Pulsing shadow effect for better readability

3. **Battery Indicator**
   - Spiral-themed battery meter
   - Percentage display
   - Visual battery level representation

### Platform Support

The watchface supports multiple Pebble platforms:
- aplite: Original Pebble and Pebble Steel
- chalk: Pebble Time Round
- diorite: Pebble 2

### Memory Usage
- Efficient animation management
- Clean cleanup in window unload
- Proper timer handling
- Optimized drawing routines

## Development Notes

- Built using the Rebble tool (modern replacement for Pebble SDK)
- Uses platform-specific optimizations
- Implements smooth animation system
- Supports various Pebble displays

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- Built with Rebble development tools
- Thanks to the Pebble development community

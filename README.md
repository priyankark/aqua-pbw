# Aqua

A dynamic and engaging watchface featuring an animated underwater aquarium. The watchface displays time, date, and battery level while presenting a lively aquatic ecosystem with various sea creatures.

## Features

- Animated fish, seahorse, shark, turtle, and other sea creatures
- Interactive ecosystem where big fish eat small fish
- Animated seaweed, bubbles, and plankton
- Bottom-dwelling crab and clam
- Clear time and date display
- Battery indicator
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

This will generate a `build/aqua.pbw` file.

## Running in Emulator

To run the watchface in the emulator:

```bash
rebble install --emulator basalt build/aqua.pbw
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

1. **Aquarium System**
   - Multiple types of animated sea creatures
   - Animated environment elements (seaweed, bubbles)
   - Interactive predator-prey relationships
   - Bottom-dwelling creatures (crab, clam, seahorse)

2. **Time Display**
   - Clear, centered time presentation
   - Date display with day and month
   - Designed to be visible against the aquarium background

3. **Battery Indicator**
   - Simple and effective battery meter
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

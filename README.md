# KMI MIDI Device Manager

A cross-platform C++/Qt MIDI library for Muse Kinetics / Keith McMillen Instruments (KMI) devices.

## Overview

The KMI MIDI Device Manager provides a comprehensive framework for managing MIDI communication with KMI hardware devices. It helps facilitate device detection, connection management, firmware updates, calibration, and real-time MIDI I/O operations across multiple KMI product lines.

## Features

### Core Functionality
- **Device Management**: Automatic detection and connection to KMI MIDI devices
- **MIDI I/O**: Robust MIDI input/output handling using RtMidi
- **Port Monitoring**: Real-time monitoring of MIDI port changes and device connections
- **SysEx Communication**: Complete SysEx message handling for device configuration
- **Multi-Device Support**: Simultaneous management of multiple KMI devices

### Advanced Features
- **Firmware Updates**: Complete firmware update workflow with progress tracking
- **Calibration Tools**: Device calibration interfaces for pedals and sensors
- **Bootloader Support**: Install and manage device bootloaders
- **Update Checking**: Automatic firmware update availability checking
- **UI Components**: Pre-built Qt UI widgets for firmware updates and device management

### Supported KMI Devices
- QuNeo
- QuNexus
- 12 Step
- SoftStep (1, 2, 3)
- K-Mix (in development as of 12/25/2025)

## Architecture

### Main Components

**KMI_Ports** (`kmi_ports.h/cpp`)
- MIDI port detection and monitoring
- Cross-platform port name handling
- Port change notifications
- RtMidi interface wrapper

**KMI_mdm** (`KMI_mdm.h/cpp`)
- Core MIDI device manager class
- Handles device lifecycle (connect, disconnect, communication)
- Manages firmware update state machine
- Processes incoming MIDI messages
- Supports RPN/NRPN parameter control

**Device Data** (`KMI_DevData.h`)
- Device identification (USB PIDs, SysEx IDs)
- OS-specific port name definitions
- Product-specific configuration data

**Firmware Management** (`KMI_FwVersions.h`, `fwupdate/`)
- Firmware version tracking
- Update UI components
- Bootloader installation
- Progress monitoring

**SysEx Messages** (`KMI_SysexMessages.h/c`)
- SysEx message construction and parsing
- Device-specific message protocols
- Configuration data handling

**Calibration** (`cvCal/`, `pedalCal/`)
- Sensor calibration interfaces
- Pedal calibration utilities
- Device-specific calibration workflows

## Dependencies

### Required
- **Qt 5/6**: Core, Widgets, GUI modules
- **RtMidi**: Cross-platform MIDI I/O library

### Optional
- **Network**: For firmware update checking (KMI_updates.h/cpp)

## Integration

### Basic Usage

```cpp
#include "KMI_mdm.h"
#include "KMI_ports.h"

// Create port monitor
KMI_Ports *ports = new KMI_Ports();

// Create device manager for QuNeo
MidiDeviceManager *quneo = new MidiDeviceManager(
    nullptr,          // parent widget
    PID_QUNEO,       // device PID
    "QuNeo",         // object name
    ports            // port monitor
);

// Connect signals
connect(quneo, &MidiDeviceManager::midiRxSignal, 
        this, &MyClass::handleMidiMessage);
```

### Adding to Qt Project

Add to your `.pro` file:
```qmake
SOURCES += \
    KMI_mdm.cpp \
    kmi_ports.cpp \
    KMI_SysexMessages.c

HEADERS += \
    KMI_mdm.h \
    kmi_ports.h \
    KMI_DevData.h \
    KMI_FwVersions.h \
    KMI_SysexMessages.h \
    midi.h

# Include RtMidi
INCLUDEPATH += path/to/rtmidi
SOURCES += path/to/rtmidi/RtMidi.cpp
```

### Using as Submodule

```bash
git submodule add git@github.com:Muse-Kinetics/kmi_midi_device_manager.git
git submodule update --init --recursive
```

## Platform Support

### Tested Platforms
- macOS (10.13+)
- Windows (7, 10, 11)
- Linux (Ubuntu, Debian), firmware updates not currently supported

### Platform-Specific Notes
- **Windows**: USB MIDI port names include port numbers (automatically handled)
- **macOS**: Uses CoreMIDI backend via RtMidi
- **Linux**: Uses ALSA backend via RtMidi

## Development

### Project Structure
```
kmi_midi_device_manager/
├── KMI_mdm.h/cpp           # Main device manager
├── kmi_ports.h/cpp         # Port monitoring
├── KMI_DevData.h           # Device definitions
├── KMI_FwVersions.h        # Firmware versions
├── KMI_SysexMessages.h/c   # SysEx handling
├── KMI_updates.h/cpp       # Update checking
├── midi.h                  # MIDI definitions
├── fwupdate/               # Firmware update UI
├── cvCal/                  # CV calibration
├── pedalCal/               # Pedal calibration
├── qt_ui/                  # UI components
├── kmiSysEx/               # SysEx utilities
├── stylesheets/            # Qt stylesheets
├── troubleshoot/           # Diagnostic tools
└── images/                 # UI resources
```

### Building

The library is designed to be integrated into Qt projects. No standalone build is required.

## License

This project is licensed under the Mozilla Public License 2.0 (MPL-2.0).

Copyright © 2025 KMI Music, Inc.

See [LICENSE](LICENSE) for full license text.

## Authors

Written by Eric Bateman, August 2021 - present.

## Related Projects

- [QuNeo Editor](https://github.com/Muse-Kinetics/quneo)
- [QuNexus Editor](https://github.com/Muse-Kinetics/qunexus-qt6)
- [12 Step Editor](https://github.com/Muse-Kinetics/12-step-editor-qt6)
- [SoftStep Editor](https://github.com/Muse-Kinetics/softstep_editor)
- [RtMidi](https://github.com/Muse-Kinetics/rtmidi) (Muse Kinetics fork)
- [RtMidi Original](https://github.com/thestk/rtmidi)

## Support

KMI products are currently available for sale through Muse Kinetics with a one year warranty that includes customer support, however this source code is provided as-is with no warranty. For product support please visit https://support.musekinetics.com. For feature requests, or bug reports, please create an issue in this repository and encourage other open source developers to assist you.
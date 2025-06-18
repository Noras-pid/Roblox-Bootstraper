# Roblox-Bootstraper

A modern and user-friendly application for managing Roblox installations, enabling users to install, launch, and manage multiple versions of Roblox with ease. Designed with performance and simplicity in mind, Roblox-Bootstraper provides a seamless experience for both casual and advanced users.

## Features

- **Version Management**: Install and manage multiple Roblox versions, including the latest releases or custom version hashes.
- **Custom Installation Paths**: Select installation directories with a built-in folder browser.
- **System Tray Integration**: Minimize the application to the system tray with a configurable hotkey for quick access.
- **Settings Persistence**: Save and load user preferences, including installation paths, hotkeys, and startup behavior.
- **Shortcut Creation**: Generate desktop and Start Menu shortcuts, and register a 'ruststrap' command for easy access.
- **Automatic Updates**: Fetch and display the latest Roblox version from the official CDN.
- **Robust Error Handling**: Clear status messages and progress indicators for downloads and installations.

## Configuration

The application stores its configuration in `C:\RustStrap\ruststrap_settings.json`. This file includes settings such as:
- Installation path
- Hotkey for toggling the system tray
- Minimize on launch/startup preferences
- Auto-startup with Windows

To reset the configuration, use the "Reset Configuration" button in the settings menu.

## Dependencies

Roblox-Bootstraper is built using the following tools and libraries:
- [ImGui-External-Base](https://github.com/Guardln/ImGui-External-Base): A lightweight ImGui-based framework for creating modern user interfaces.
- [Better-Roblox-Installer](https://github.com/sixpennyfox4/Better-Roblox-Installer): Inspiration and reference for Roblox version management and installation logic.
- [vcpkg](https://github.com/microsoft/vcpkg): A C++ library manager used to manage dependencies.
- [libzip](https://libzip.org/): A library for reading, creating, and modifying zip archives, used for handling Roblox installation files.

## Building from Source

To build Roblox-Bootstraper from source, you will need:
- Visual Studio 2022 or later with C++ desktop development workload
- vcpkg for dependency management
- DirectX SDK

### Steps

1. **Install vcpkg**:
   - Clone the vcpkg repository:
     ```bash
     git clone https://github.com/microsoft/vcpkg.git
     ```
   - Run the bootstrap script:
     ```bash
     cd vcpkg
     .\bootstrap-vcpkg.bat
     ```
   - Integrate vcpkg with Visual Studio:
     ```bash
     .\vcpkg integrate install
     ```

2. **Install Dependencies**:
   - Install libzip using vcpkg:
     ```bash
     .\vcpkg install libzip
     ```

3. **Clone the Repository**:
   ```bash
   git clone https://github.com/yourusername/Roblox-Bootstraper.git
   ```
4. **Build the Project**

5. **Run the Application**:
   - The executable will be located in `Build`.

## Contributing

Contributions are welcome! Please follow these steps:
1. Fork the repository.
2. Create a new branch for your feature or bug fix.
3. Submit a pull request with a clear description of your changes.

Ensure your code adheres to the existing style and includes appropriate documentation.

## Acknowledgments

- Thanks to [Guardln](https://github.com/Guardln) for the ImGui-External-Base framework.
- Thanks to [sixpennyfox4](https://github.com/sixpennyfox4) for the Better-Roblox-Installer project, which served as a valuable reference.
- Thanks to the [Microsoft vcpkg team](https://github.com/microsoft/vcpkg) for providing a robust dependency manager.
- Thanks to the [libzip contributors](https://libzip.org/) for their zip archive library.
- The Roblox community for their continuous feedback and support.
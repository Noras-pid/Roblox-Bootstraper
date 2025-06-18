Roblox-Bootstraper
A modern and user-friendly application for managing Roblox installations, enabling users to install, launch, and manage multiple versions of Roblox with ease. Designed with performance and simplicity in mind, Roblox-Bootstraper provides a seamless experience for both casual and advanced users.
Features

Version Management: Install and manage multiple Roblox versions, including the latest releases or custom version hashes.
Custom Installation Paths: Select installation directories with a built-in folder browser.
System Tray Integration: Minimize the application to the system tray with a configurable hotkey for quick access.
Settings Persistence: Save and load user preferences, including installation paths, hotkeys, and startup behavior.
Shortcut Creation: Generate desktop and Start Menu shortcuts, and register a 'ruststrap' command for easy access.
Automatic Updates: Fetch and display the latest Roblox version from the official CDN.
Robust Error Handling: Clear status messages and progress indicators for downloads and installations.



Configuration
The application stores its configuration in C:\RustStrap\ruststrap_settings.json. This file includes settings such as:

Installation path
Hotkey for toggling the system tray
Minimize on launch/startup preferences
Auto-startup with Windows

To reset the configuration, use the "Reset Configuration" button in the settings menu.
Dependencies
Roblox-Bootstraper is built using the following tools and libraries:

ImGui-External-Base: A lightweight ImGui-based framework for creating modern user interfaces.
Better-Roblox-Installer: Inspiration and reference for Roblox version management and installation logic.
vcpkg: A C++ library manager used to manage dependencies.
libzip: A library for reading, creating, and modifying zip archives, used for handling Roblox installation files.

Building from Source
To build Roblox-Bootstraper from source, you will need:

Visual Studio 2022 or later with C++ desktop development workload
vcpkg for dependency management
DirectX SDK

Steps

Install vcpkg:

Clone the vcpkg repository:git clone https://github.com/microsoft/vcpkg.git


Run the bootstrap script:cd vcpkg
.\bootstrap-vcpkg.bat


Integrate vcpkg with Visual Studio:.\vcpkg integrate install




Install Dependencies:

Install libzip using vcpkg:.\vcpkg install libzip








Contributing
Contributions are welcome! Please follow these steps:

Fork the repository.
Create a new branch for your feature or bug fix.
Submit a pull request with a clear description of your changes.

Ensure your code adheres to the existing style and includes appropriate documentation.
License
This project is licensed under the MIT License. See the LICENSE file for details.
Acknowledgments

Thanks to Guardln for the ImGui-External-Base framework.
Thanks to sixpennyfox4 for the Better-Roblox-Installer project, which served as a valuable reference.
Thanks to the Microsoft vcpkg team for providing a robust dependency manager.
Thanks to the libzip contributors for their zip archive library.
The Roblox community for their continuous feedback and support.

Contact
For issues, suggestions, or questions, please open an issue on the GitHub repository.

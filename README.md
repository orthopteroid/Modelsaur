# Modelsaur
Clay Deformation on a Simple Sphere

Multiplatform C++:
- Linux OGL1 / OGLES2
- Android NDK & JNI (currently unbuildable due to NDK issues)

Features:
- Multitouch pinch to zoom (requires a tablet on Linux)
- Proper Quaternion Transforms
- 3 different deformation tools and sizes
- Triangle coloring from custom palette
- Pleasant animated rotation feature
- Export to PNG, GIF, STL
- Hidden features to generate alternate non-sphere geometries

Build Dependencies:
- libmt-dev for multitouch support
- glm-0.9.7.6 (included) for matrix math
- giflib-5.1.4 (included) to export gif
- libpng-1.6.26 (included) to export png on linux
- libpng-1.6.34 (included) to export png on android

Interesting Things to note in the code:
- ./src/Android and ./src/linux are the platform specific c++ components
- ./android is the java compnents needed for Android
- the main app loop is app_main() in ./src/App.cpp
- png graphical assests are baked into linkable object code files using `/usr/bin/ld` and extracted via the `ACCESS_RESOURCE` macro and AppTexture::LoadResource(...)
- platform independent event handling is via a functor, passed into AppEvent(...)
- As a convienience during development AppKeyboard held much of the app and controller state. This allowed a physical keyboard to control the app but required translation logic (implemented in AppEvent(...)) for Android. However, AppEvent(...) also handles some non-keyboard messages on Linux (like window adornment events). Not a perfect world.
- hashed ID based state serializer class AppML. Presently only used for storing the tutorial mode's completion state.
- text is rendered using the OGL1 friendly (but eye unfriendly) triangle shapes from https://github.com/orthopteroid/gewellt-eight

Start screen on linux

<img width="610" height="430" alt="Screenshot_2025-11-09_16-40-10" src="https://github.com/user-attachments/assets/27f196e5-a82c-460b-95ed-12352871774d" />

Menu with a completed model

<img width="600" height="400" alt="GNHO8178-menu" src="https://github.com/user-attachments/assets/70b812be-6975-455a-9e0f-73bd9100bb90" />

<img width="600" height="400" alt="GNHO8178-savedialog" src="https://github.com/user-attachments/assets/75a19571-5edc-42a6-8f31-2e846285206a" />

Exported as a rotating animated gif

![GNHO8178](https://github.com/user-attachments/assets/92b8cffc-a7cd-4707-ac60-4db7c941bb41)

It can also be exported as STL.

Copyright 2025 orthopteroid@gmail.com except for external dependencies or code where noted.

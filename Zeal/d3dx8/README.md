# D3DX8 notes:

* The D3DX8 library includes directx helper and utility functions like projecting to the screen
  or loading textures from files

* The latest files were sourced from dx81sdk_full.exe downloaded from the [internet archive](https://archive.org/details/dx81sdk_full).
  - The header files are from the `DXF/DXSDK/include` directory
    - Only header files actively included by Zeal were copied over
    - These 8.1 header files have the `D3D_SDK_VERSION 220` that matches the game exe create argument
    - The originally checked in header files were `D3D_SDK_VERSION 120`
  - The `dx3d8.lib` is from the `DXF/DXSDK/lib` directory
    - The archive date from tis new lib is October 2001
    - The originally checked in lib was October 2000 (probably 8.0)
    - This newer lib tries to link in `libci.lib` which is a depreated single-thread CRT library
      - Had to disable that fron the linker input to make things compile
      - Need to avoid any functions that might invoke that old pathway (loading textures seemed okay)
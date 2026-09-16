# Building

## Windows (MSVC) — the primary target

```bat
cmake -S . -B build
cmake --build build --config Release
build\Release\mdview.exe --version
build\Release\mdview.exe .
```

The generator is deliberately **not** hard-coded: omitting `-G` makes CMake choose
the newest Visual Studio generator that is actually installed, and the script
echoes back the one it ended up with (`Configured with "Visual Studio 18 2026"`).
If you want to pin a specific generator:

```bat
cmake -S . -B build -G "Visual Studio 18 2026"
cmake --build build --config Release
```

The convenience script does all of this — locating `vcvars64.bat` through
`vswhere` so it works from any shell without a Developer Command Prompt, and
reconfiguring automatically if you switch generators in an existing build tree:

```bat
scripts\build.bat                       :: newest VS generator, Release
scripts\build.bat --ninja               :: Ninja instead (build-ninja\mdview.exe)
scripts\build.bat --generator "NAME"    :: pin a specific CMake generator
scripts\build.bat --test                :: build, then run the test suite
scripts\build.bat --debug               :: Debug configuration
scripts\build.bat --clean               :: wipe the build directory first
scripts\build.bat --help                :: all options
scripts\check-toolchain.bat             :: report what compilers CMake can see
```

## Linux and macOS

```sh
cmake -S . -B build
cmake --build build --config Release
./build/mdview --version
./build/mdview .
```

With a single-configuration generator the executable lands directly in
`build/`; with a multi-configuration generator (Visual Studio, Xcode) it lands
in `build/<Config>/`, which is why the commands above differ only in the path.

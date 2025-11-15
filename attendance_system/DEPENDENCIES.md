Optional native dependencies for face_recognition
===============================================

This project optionally uses the Python package `face_recognition` to compute
face embeddings. The `face_recognition` package depends on `dlib`, which is a
native library that must be built with a C++ toolchain on Windows. The steps
below outline how to prepare a Windows environment to install `face_recognition`.

Quick steps (Windows PowerShell)
--------------------------------

1. Install Visual C++ Build Tools (Visual Studio Build Tools) and include the
   "Desktop development with C++" workload. This provides the MSVC compiler
   needed to build `dlib`.
2. Install CMake: https://cmake.org/download/ and ensure `cmake` is on PATH.
3. From a PowerShell prompt inside the repository root run:

```powershell
python -m pip install --upgrade pip setuptools wheel
python -m pip install cmake
python -m pip install -r requirements.txt
```

Notes and troubleshooting
------------------------
- Building `dlib` from source can be slow and sometimes fails on Windows due
  to missing toolchain components. If you encounter build errors, consider
  these alternatives:
  - Use a pre-built wheel for `dlib` from a trusted source (for example,
    Christoph Gohlke's unofficial wheels) matching your Python version and
    platform.
  - Use a Docker container (Linux) where `dlib` is easier to install.
  - If you don't need face embeddings in your environment, you can omit
    `face_recognition` from your environment and the app will continue to run
    (the embedding generation code is guarded to avoid import-time failures).

Security / versioning
---------------------
- Pin versions in `requirements.txt` for reproducible environments before
  deploying or sharing with others.

If you'd like, I can:
- add pinned versions for these packages in `requirements.txt`;
- add a short Windows-specific troubleshooting section with common build
  errors and fixes; or
- add a small script/PowerShell helper that attempts to install the native
  prerequisites automatically.

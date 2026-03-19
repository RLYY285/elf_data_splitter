#!/bin/bash

# 创建项目目录
mkdir -p elf-data-splitter/{src,include,tests,build}

# 创建 .gitignore
cat > elf-data-splitter/.gitignore << 'EOF'
# Build directories
build/
cmake_build/
out/

# Compiled objects
*.o
*.obj
*.a
*.lib
*.dll
*.so
*.dylib

# Executables
elf-splitter
elf-splitter.exe
test_splitter
test_splitter.exe

# CMake
CMakeFiles/
CMakeCache.txt
cmake_install.cmake
Makefile
*.cmake

# IDEs
.vscode/
.idea/
*.swp
*.swo
*.user
.DS_Store
EOF

echo "✓ Project structure created successfully!"
echo "✓ Navigate to: cd elf-data-splitter"
echo "✓ Build: mkdir build && cd build && cmake .. && make"
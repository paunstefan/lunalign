# LunAlign


## Build instructions

C++23 is needed so you need a recent version of Clang or GCC.

```bash
sudo apt install clang-19
sudo apt install libc++-19-dev libc++abi-19-dev
```

```
mkdir build
cd build

cmake .. -D CMAKE_CXX_COMPILER=clang++-19
```

Building `librtprocess`:
```
cmake -DCMAKE_CXX_COMPILER=clang++-19 \
      -DCMAKE_CXX_FLAGS="-stdlib=libc++" \
      -DCMAKE_SHARED_LINKER_FLAGS="-stdlib=libc++" \
      -DBUILD_SHARED_LIBS=OFF \
      -DOPTION_OMP=OFF \
      ..

make -j6
```
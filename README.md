# LunAlign

TODO: refactoring to C++, logging, fits class

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

make
```

Building `opencv2`:

```
cmake -DBUILD_LIST=core,imgproc,imgcodecs,highgui -DCMAKE_CXX_COMPILER=clang++-19 -DCMAKE_CXX_FLAGS="-stdlib=libc++" -DCMAKE_SHARED_LINKER_FLAGS="-stdlib=libc++" -DBUILD_SHARED_LIBS=OFF -DWITH_ITT=OFF  ..

make
make install
```

You should now copy the header files from `/usr/local/include` into the include directory from the repo.
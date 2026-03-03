# LunAlign


## Build instructions

C++23 is needed so you need a recent version of Clang or GCC.

```bash
sudo apt install clang-19
sudo apt install libc++-19-dev libc++abi-19-dev
```

Also the program uses OpenMP, so you need to install the library to compile it:

```bash
sudo apt install libomp-19-dev
```

```
mkdir build
cd build

cmake .. -D CMAKE_CXX_COMPILER=clang++-19
make
```

The commands to build the dependencies separately:
(This will now be done automatically by the main CMake file.)

Building `cfitsio`:
```
cmake -G "Unix Makefiles" -DBUILD_SHARED_LIBS=OFF -DUSE_PTHREADS=ON ..

make
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

## Third-Party Code

* **Siril**: Portions of this software (specifically demosaicing code) were adapted from [Siril](https://free-astro.org/index.php/Siril).
    * *Copyright:* Francois Meyer and team free-astro.
    * *License:* GPLv3.
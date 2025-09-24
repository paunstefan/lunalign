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
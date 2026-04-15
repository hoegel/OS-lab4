# OS LAB-04

## Building
```
cd  build
cmake .. -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ -DCMAKE_SYSTEM_NAME=Windows
make -j4
```

## Running
```
wine ./lab4_sync.exe     # for running main
wine ./tests.exe            # for running tests
```
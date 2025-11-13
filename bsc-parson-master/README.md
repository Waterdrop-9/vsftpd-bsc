# The BSC-PARSON project

This project is a json parser library written in BiShengC language. We provide the C souce code of this project and its corresponding code refactored in BiShengC. The entire refactoring process has been organized according to Git commits.

## Build

Before compiling bsc-parson, the BiShengC compiler and libstdcbs should be prepared. You can find them at [here](https://gitee.com/bisheng_c_language_dep/llvm-project). Then please compile bsc-parson according to the following command:

```bash
clang -c ./bsc-parson/bsc-parson/parson.cbs -o parson.o -I/path-to-libstdcbs-head
ar rcs libparson.a parson.o
```

After that, we can use `libparson.a`. Because it depends on `libstdcbs.a`, so we should link with it.

```bash
clang main.cbs -I/path-to-libstdcbs-head -I/path-to-parson-head -L. -lstdcbs -lparson
```

Then, it will work.
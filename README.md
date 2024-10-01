# Pagan

Pagan is a dynamic parser and editor library for binary files.
Many file formats can be specified through a yaml based description language, closely mirroring the ksy format from kaitai struct.
Format specifications are interpreted at runtime to produce a parser, meaning that no coding or compilation is required to iterate on the specification

## Development status

Prototype state

My intended use-case for this project has mostly disappeared so I now work on this very infrequently.

## Language support

- C++ (native)
- javascript bindings

## Relation to Kaitai Struct

Kaitai Struct (https://kaitai.io) is a parser generator for binary files or streams using a readable declaration format.

Pagan is an independent project to Kaitai Struct, supporting (a subset of) the same declaration language (ksy).

### Implementation

The main difference is that Kaitai Struct generates source code in various languages to be included in your project directly. Pagan is a native library that creates the compiler at runtime.
The main advantage is that users don't need to set up a compiler environment to develop and test formats.

Also, adding support for different programming languages should be easier since only bindinggs for the library frontend need to be developed rather than a compiler and support library. This should also make it easier to maintain feature parity (e.g. compression/encryption libraries don't need to be available in each language individually)

The drawback is performance and convenience though. There is less opportunity for optimization and since everything is dynamic, there is no code completion in the IDE, and no static type checking.

### Feature comparison

- Pagan currently only supports a subset of the declaration and thus only a small number of formats
- Pagan has limited edit support. The syntax extension for this will probably not be compatible with the write support planned for Kaitai, if it ever comes
- Pagan currently only supports parsing files, not streams

## ToDos

- More tests are required. An automated way to test pagan parsing results against those of kaitai to ensure compatibility might be more useful than unit tests
  It would also be prudent to have a look at the kaitai test repo (https://github.com/kaitai-io/kaitai_struct_tests)
- Potentially in the course of testing, benchmark comparisons could be introduced
- There is quite a bit of code duplication that should be cleaned up
- main.cpp contains a list of technical todos (memory management), index_format.md contains todos regarding the inefficiencies and inconsistencies of the index format
- Right now pagan will not work on files larger than 4GB because of limitations of the index format (see index_format.md)
- introduce code quality verification and auto formatting (clang_tidy, clang_format)
- Linux/MacOS builds!

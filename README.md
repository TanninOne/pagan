# Pagan

Pagan is a dynamic parser and editor library for binary files.
Many file formats can be specified through a yaml based description language, closely mirroring the ksy format from kaitai struct.
Format specifications are interpreted at runtime to produce a parser, meaning that no coding or compilation is required to iterate on the specification

## Development status

Prototype state

My intended use case for this project has mostly disappeared so I now work on this very unfrequently.

## Language support

- C++ (native)
- javascript bindings

## Relation to Kaitai Struct

Kaitai Struct (https://kaitai.io) is a parser generator for binary files or streams using a readable declaration format.

Pagan is an independent project to Kaitai Struct, supporting (a subset of) the same declaration language (ksy).

### Implementation

The main difference is that Kaitai Struct generates source code in various languages to be included in your project directly. Pagan creates the compiler at runtime.
The main advantage is that users don't need to set up a compiler environment to develop and test formats.

Also, adding support for different programming languages should be easier since only bindinggs for the library frontend need to be developed rather than a compiler and support library. This should also make it easier to maintain feature parity.

The drawback is performance and convenience though. There is less opportunity for optimization and since everything is dynamic, there is no code completion in the IDE, and no static type checking.

### Feature comparison

- Pagan currently only supports a subset of the declaration and thus only a small number of formats
- Pagan has limited edit support. The syntax extension for this will probably not be compatible with the write support planned for Kaitai, if it ever comes
- Pagan currently only supports parsing files, not streams

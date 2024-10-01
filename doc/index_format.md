## Purpose

pagan does not parse the file in its entirety into a memory structure, instead it creates an index that contains copies of small PODs but references to any larger data structures.
Nested data structures are only indexed on demand or if they have to be parsed to determine their size.
On the first read of the file pagan will try to index the file to the end but for any object it already knows the size (either because it has a size field or only non-optional properties with known size) it will skip past it.
The object is then only actually parsed at runtime as the objects are accessed. As a result, the index doesn't necessarily contain objects in the order in which they appear on disk.

## Data types

pagan only knows PODs, custom types and lists thereof. However, one special type is "runtime" which will be resolved at runtime when the field is actually parsed.
The index will reserve space for the runtime type so it only has to be resolved once.

Lists can either 
* have fixed size (as in: set by the specification)
* have calculated size (as in: read as data or calculated from data in the file, available before the list is indexed)
* extend to the end of the "stream". "End of stream" means: either the end of the parent object (which needs to be of known size) or the end of the file.
* repeat until a condition is met

## Data stream

In the simplest case there will be a single data stream: the file itself.
If the file has compressed or encrypted chunks though, each of these chunks can be represented as a separate datastream that transparently decrypts/deflates the data.

## Indices

There are 3 separate buffers:

### Object index

The object index contains an unordered list (or rather: ordered by the time they were first accessed in code) of variable size entries.
One can assume that the "root" object of the file is always the first entry in the index, other objects wouldn't be accessed directly anyway as the order is unknown

``` C
struct ObjectIndex
{
  // offset into the data stream where the data for this object can be found
  uint64_t dataOffset;
  // reference to the index of the object properties
  uint8_t *properties;
  // specifies the type of object
  uint32_t typeId;
  // specifies which data stream this object is found in
  uint16_t dataStream;
  // total size of this index (including this size field)
  uint8_t size;
  // variable length bitmask specifying which properties are set
  uint8_t bitmask[1];
};
```

Implementation detail:
If the file format doesn't provide size fields, pagan may have to recursively parse the properties of the object before the index of the parent object is complete.
During this recursive parsing, the properties pointer will point to a temporary buffer containing all properties parsed so far. This is necessary so that calculated values can access those properties.

### Properties index

Once an object is fully indexed, its "properties" pointer points to this index. It will contain either the data (for PODs of size <= the size of a pointer), offsets into the data stream to where the actual data for a property is or a pointer back into the object index for nested objects.

While an object is being parsed, properties are stored in a temporary buffer, the "properties" pointer points to that. This allows dynamic functions to access previously read properties to determine size, runtime types and so on.

The representation of each property depends on the type:
* numerical values (all (unsigned) int, 8, 16, 32, 64 bits, float) are read directly into the index and take as much space as in the base file
* strings (zero terminated or with size field), data blobs: 2x 32bit fields, first one contain the offset into the data stream (1), second the size of the string
* object: 1xsigned 64bit offset. If the object hasn't been parsed yet, this is the offset into the data stream. If it has been parsed, offset into the object index * -1
* bitmasks ?
* arrays: 2x 32bit field (count, offset), see below

#### Array

A fully indexed array will contain the evaluated item count and the offset into the array index.

It may also contain -2 (COUNT_EOS - End Of Stream) or -3 (COUNT_MORE - repeat until condition is met) for the count. This means the array hasn't been indexed yet and it's length will only be known after indexing. The offset still references the array index where we store 2x64 bit values, the index into the data stream and the length to the end of the stream/parent.

#### "runtime" type

A runtime type is one where a switch/case determines the type based on other fields read from the file. These are stored as a 32bit field containing the id of the actual type followed by the index representation of that type as described above.

### Array index

The array index represents items in the same way as the properties index.
Example 1: if the item type is an array of 16 bit integers and at runtime the count was determined to be 15 items then the properties index will contain the number 15 in its first 32bit field, the offset of this array in the array index. The array index it this position will contain 15 times 16bit fields with the actual values.
Example 2: if the item type is an array of strings and again we found 15 items, the properties index contains the same fields: (15, array index offset)
The array index contains 15 * (data stream offset, string length)

## Known issues

* (1) objects can have a 64bit offset into the data stream, strings and data blobs are also stored using an index into the stream but that is limited to 32bit because we have to store it together with the blob size into a 64bit value. This could be worked around by treating the string/data offset as an offset to the owning object and/or storing blob offset and size as variable size integers.
Or splitting the file into multiple data streams if 32bit offsets don't suffice.
* (2) data blob fields are stored as (offset, length) while arrays are stored as (length, offset). Inconsistency makes me sad
* (3) we currently use 32 bit to store the actual type for "runtime" fields, that seems pretty excessive, I want to see the ksy declaring more than, say, 65k custom types...
* (4) when storing a list of "runtime" type, we currently store the concrete type with each individual item rather than with the list, despite the fact that the way types are declared, the all have to have the same type. This is probably extremely harmful to performance
* (5) code uses the words "array" and "list" interchangeably, sanitize that
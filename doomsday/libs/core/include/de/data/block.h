/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBDENG2_BLOCK_H
#define LIBDENG2_BLOCK_H

#include "../IByteArray"
#include "../IBlock"
#include "../ISerializable"
#include "../Writer"

#include <QByteArray>

namespace de {

class String;
class IIStream;

/**
 * Data buffer that implements the byte array interface.
 *
 * Note that Block is based on QByteArray, and thus automatically always ensures
 * that the data is followed by a terminating \0 character (even if one is not
 * part of the actual Block contents). Therefore it is safe to use it in functions
 * that assume zero-terminated strings.
 *
 * @ingroup data
 */
class DENG2_PUBLIC Block : public QByteArray, public IByteArray, public IBlock,
                           public ISerializable
{
public:
    Block(Size initialSize = 0);
    Block(IByteArray const &array);
    Block(Block const &other);
    Block(Block &&moved);
    Block(QByteArray const &byteArray);
    Block(char const *nullTerminatedCStr);
    Block(void const *data, Size length);

    /**
     * Constructs a block by reading the contents of an input stream. The block
     * will contain all the data that is available immediately; will not wait
     * for additional data to become available later.
     *
     * @param stream  Stream to read from.
     */
    Block(IIStream &stream);

    /**
     * Constructs a block by reading the contents of an input stream in const
     * mode. The block will contain all the data that is available immediately;
     * will not wait for additional data to become available later.
     *
     * @param stream  Stream to read from.
     */
    Block(IIStream const &stream);

    /**
     * Construct a new block and copy its contents from the specified
     * location at another array.
     *
     * @param array  Source data.
     * @param at     Offset within source data.
     * @param count  Number of bytes to copy. Also the size of the new block.
     */
    Block(IByteArray const &array, Offset at, Size count);

    Byte *data();
    Byte const *dataConst() const;

    Block &append(Byte b);
    Block &append(char const *str, int len);

    inline explicit operator bool() const { return !isEmpty(); }

    Block &operator += (char const *nullTerminatedCStr);
    Block &operator += (QByteArray const &bytes);

    /// Appends a block after this one.
    Block &operator += (Block const &other);

    /// Appends a byte array after this one.
    Block &operator += (IByteArray const &byteArray);

    /// Copies the contents of another block.
    Block &operator = (Block const &other);

    Block &operator = (IByteArray const &byteArray);

    Block compressed(int level = -1) const;
    Block decompressed() const;
    Block md5Hash() const;
    String asHexadecimalText() const;

    // Implements IByteArray.
    Size size() const;
    void get(Offset at, Byte *values, Size count) const;
    void set(Offset at, Byte const *values, Size count);

    // Implements IBlock.
    void clear();
    void copyFrom(IByteArray const &array, Offset at, Size count);
    void resize(Size size);
    Byte const *data() const;

    // Implements ISerializable.
    /**
     * Writes @a block into the destination buffer. Writes the size of the
     * block in addition to its contents, so a Reader will not need to know
     * beforehand how large the block is.
     *
     * @param block  Block to write.
     *
     * @return  Reference to the Writer.
     */
    void operator >> (Writer &to) const;

    void operator << (Reader &from);

public:
    static Block join(QList<Block> const &blocks, Block const &sep = Block());
};

template <typename... Args>
Block md5Hash(Args... args) {
    Block data;
    Writer writer(data);
    writer.writeMultiple(args...);
    return data.md5Hash();
}

} // namespace de

#endif // LIBDENG2_BLOCK_H

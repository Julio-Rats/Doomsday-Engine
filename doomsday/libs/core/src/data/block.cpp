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

#include "de/Block"
#include "de/File"

#include <QCryptographicHash>
#include <cstring>

namespace de {

Block::Block(Size initialSize)
{
    resize(initialSize);
}

Block::Block(IByteArray const &other)
{
    // Read the other's data directly into our data buffer.
    resize(other.size());
    other.get(0, (dbyte *) data(), other.size());
}

Block::Block(Block const &other)
    : QByteArray(other), IByteArray(), IBlock()
{}

Block::Block(Block &&moved)
    : QByteArray(moved)
{}

Block::Block(QByteArray const &byteArray)
    : QByteArray(byteArray)
{}

Block::Block(char const *nullTerminatedCStr)
    : QByteArray(nullTerminatedCStr)
{}

Block::Block(void const *data, Size length)
    : QByteArray(reinterpret_cast<char const *>(data), int(length)), IByteArray(), IBlock()
{}

Block::Block(IIStream &stream)
{
    stream >> *this;
}

Block::Block(IIStream const &stream)
{
    stream >> *this;
}

Block::Block(IByteArray const &other, Offset at, Size count) : IByteArray()
{
    copyFrom(other, at, count);
}

Block::Size Block::size() const
{
    return QByteArray::size();
}

void Block::get(Offset atPos, Byte *values, Size count) const
{
    if (atPos + count > size())
    {
        /// @throw OffsetError The accessed region of the block was out of range.
        throw OffsetError("Block::get", "Out of range " +
                          String("(%1[+%2] > %3)").arg(atPos).arg(count).arg(size()));
    }

    std::memcpy(values, constData() + atPos, count);
}

void Block::set(Offset at, Byte const *values, Size count)
{
    if (at > size())
    {
        /// @throw OffsetError The accessed region of the block was out of range.
        throw OffsetError("Block::set", "Out of range");
    }
    replace(at, count, QByteArray(reinterpret_cast<char const *>(values), count));
}

void Block::copyFrom(IByteArray const &array, Offset at, Size count)
{
    // Read the other's data directly into our data buffer.
    resize(count);
    array.get(at, data(), count);
}

void Block::resize(Size size)
{
    QByteArray::resize(size);
}

IByteArray::Byte const *Block::dataConst() const
{
    return reinterpret_cast<Byte const *>(QByteArray::constData());
}

Block &Block::append(IByteArray::Byte b)
{
    QByteArray::append(char(b));
    return *this;
}

Block &Block::append(char const *str, int len)
{
    QByteArray::append(str, len);
    return *this;
}

Block &Block::operator += (char const *nullTerminatedCStr)
{
    QByteArray::append(nullTerminatedCStr);
    return *this;
}

Block &Block::operator += (QByteArray const &bytes)
{
    QByteArray::append(bytes);
    return *this;
}

Block::Byte *Block::data()
{
    return reinterpret_cast<Byte *>(QByteArray::data());
}

Block::Byte const *Block::data() const
{
    return reinterpret_cast<Byte const *>(QByteArray::data());
}

void Block::operator >> (Writer &to) const
{
    // First write the length of the block.
    to << duint32(size());

    to.writeBytes(size(), *this);
}

void Block::operator << (Reader &from)
{
    duint32 size = 0;
    from >> size;

    resize(size);
    from.readBytes(size, *this);
}

Block &Block::operator += (Block const &other)
{
    QByteArray::append(other);
    return *this;
}

Block &Block::operator += (IByteArray const &byteArray)
{
    Offset pos = size();
    resize(size() + byteArray.size());
    byteArray.get(0, data() + pos, byteArray.size());
    return *this;
}

Block &Block::operator = (Block const &other)
{
    *static_cast<QByteArray *>(this) = static_cast<QByteArray const &>(other);
    return *this;
}

Block &Block::operator = (IByteArray const &byteArray)
{
    copyFrom(byteArray, 0, byteArray.size());
    return *this;
}

Block Block::compressed(int level) const
{
    return qCompress(*this, level);
}

Block Block::decompressed() const
{
    return qUncompress(*this);
}

Block Block::md5Hash() const
{
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(*this);
    return hash.result();
}

static char asciiHexNumber(char n)
{
    if (n < 0 || n > 15) return ' ';
    return n <= 9? ('0' + n) : ('a' + n - 10);
}

String Block::asHexadecimalText() const
{
    int const count = QByteArray::size();
    String hex(count * 2, QChar('0'));
    for (int i = 0; i < count; i++)
    {
        auto const ch = duint8(at(i));
        hex[i*2]     = QChar(asciiHexNumber(ch >> 4));
        hex[i*2 + 1] = QChar(asciiHexNumber(ch & 0xf));
    }
    return hex;
}

Block Block::join(QList<Block> const &blocks, Block const &sep) // static
{
    if (blocks.isEmpty()) return Block();

    Block joined = blocks.at(0);
    for (int i = 1; i < blocks.size(); ++i)
    {
        joined += sep;
        joined += blocks.at(i);
    }
    return joined;
}

void Block::clear()
{
    QByteArray::clear();
}

} // namespace de

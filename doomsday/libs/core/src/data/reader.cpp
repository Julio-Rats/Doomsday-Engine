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

#include "de/Reader"
#include "de/String"
#include "de/Block"
#include "de/ISerializable"
#include "de/IIStream"
#include "de/FixedByteArray"
#include "de/ByteRefArray"
#include "de/data/byteorder.h"

#include <QTextStream>
#include <cstring>

namespace de {

DENG2_PIMPL_NOREF(Reader)
{
    ByteOrder const &convert;
    duint version;

    // Random access source:
    IByteArray const *source;
    IByteArray::Offset offset;
    IByteArray::Offset markOffset;

    // Stream source:
    IIStream *stream;
    IIStream const *constStream;
    dsize numReceivedBytes;
    Block incoming;     ///< Buffer for bytes received so far from the stream.
    bool marking;       ///< @c true, if marking is occurring (mark() called).
    Block markedData;   ///< All read data since the mark was set.

    Impl(ByteOrder const &order, IByteArray const *src, IByteArray::Offset off)
        : convert(order), version(DENG2_PROTOCOL_LATEST),
          source(src), offset(off), markOffset(off),
          stream(0), constStream(0), numReceivedBytes(0), marking(false)
    {}

    Impl(ByteOrder const &order, IIStream *str)
        : convert(order), version(DENG2_PROTOCOL_LATEST),
          source(0), offset(0), markOffset(0),
          stream(str), constStream(0), numReceivedBytes(0), marking(false)
    {
        upgradeToByteArray();
    }

    Impl(ByteOrder const &order, IIStream const *str)
        : convert(order), version(DENG2_PROTOCOL_LATEST),
          source(0), offset(0), markOffset(0),
          stream(0), constStream(str), numReceivedBytes(0), marking(false)
    {
        upgradeToByteArray();
    }

    /**
     * Byte arrays provide more freedom with reading. If the streaming object
     * happens to support the byte array interface, Reader will use it instead.
     */
    void upgradeToByteArray()
    {
        if (stream)
        {
            if ((source = dynamic_cast<IByteArray const *>(stream)) != 0)
            {
                stream = 0;
            }
        }

        if (constStream)
        {
            if ((source = dynamic_cast<IByteArray const *>(constStream)) != 0)
            {
                constStream = 0;
            }
        }
    }

    /**
     * Reads bytes from the stream and adds them to the incoming buffer.
     *
     * @param expectedSize  Number of bytes that the reader is expecting to read.
     */
    void update(dsize expectedSize = 0)
    {
        if (incoming.size() >= expectedSize)
        {
            // No need to update yet.
            return;
        }

        if (stream)
        {
            // Modifiable stream: read new bytes, append accumulation.
            Block b;
            *stream >> b;
            incoming += b;
        }
        else if (constStream)
        {
            Block b;
            *constStream >> b;
            // Immutable stream: append only bytes we haven't seen yet.
            b.remove(0, numReceivedBytes);
            incoming += b;
            numReceivedBytes += b.size();
        }
    }

    void readBytes(IByteArray::Byte *ptr, dsize size)
    {
        if (source)
        {
            source->get(offset, ptr, size);
            offset += size;
        }
        else if (stream || constStream)
        {
            update(size);
            if (incoming.size() >= size)
            {
                std::memcpy(ptr, incoming.constData(), size);
                if (marking)
                {
                    // We'll need this for rewinding a bit later.
                    markedData += incoming.left(size);
                }
                incoming.remove(0, size);
            }
            else
            {
                throw IIStream::InputError("Reader::readBytes",
                        QString("Attempted to read %1 bytes from stream while only %2 "
                                "bytes are available").arg(size).arg(incoming.size()));
            }
        }
    }

    void mark()
    {
        if (source)
        {
            markOffset = offset;
        }
        else
        {
            markedData.clear();
            marking = true;
        }
    }

    void rewind()
    {
        if (source)
        {
            offset = markOffset;
        }
        else
        {
            incoming.prepend(markedData);
            markedData.clear();
            marking = false;
        }
    }

    bool atEnd()
    {
        return remaining() == 0;
    }

    IByteArray::Size remaining()
    {
        if (source)
        {
            return source->size() - offset;
        }
        if (stream || constStream)
        {
            update();
            return incoming.size();
        }
        return 0;
    }
};

Reader::Reader(Reader const &other) : d(new Impl(*other.d))
{}

Reader::Reader(IByteArray const &source, ByteOrder const &byteOrder, IByteArray::Offset offset)
    : d(new Impl(byteOrder, &source, offset))
{}

Reader::Reader(IIStream &stream, ByteOrder const &byteOrder)
    : d(new Impl(byteOrder, &stream))
{}

Reader::Reader(IIStream const &stream, ByteOrder const &byteOrder)
    : d(new Impl(byteOrder, &stream))
{}

Reader &Reader::withHeader()
{
    duint32 header = 0;
    *this >> header;

    d->version = header;

    // We can't read future (or invalid) versions.
    if (d->version > DENG2_PROTOCOL_LATEST)
    {
        throw VersionError("Reader::withHeader",
                           QString("Version %1 is unknown").arg(d->version));
    }

    return *this;
}

duint Reader::version() const
{
    return d->version;
}

void Reader::setVersion(duint version)
{
    d->version = version;
}

Reader &Reader::operator >> (char &byte)
{
    return *this >> reinterpret_cast<duchar &>(byte);
}

Reader &Reader::operator >> (dchar &byte)
{
    return *this >> reinterpret_cast<duchar &>(byte);
}

Reader &Reader::operator >> (duchar &byte)
{
    d->readBytes(&byte, 1);
    return *this;
}

Reader &Reader::operator >> (dint16 &word)
{
    return *this >> reinterpret_cast<duint16 &>(word);
}

Reader &Reader::operator >> (duint16 &word)
{
    d->readBytes(reinterpret_cast<IByteArray::Byte *>(&word), 2);
    d->convert.networkToHost(word, word);
    return *this;
}

Reader &Reader::operator >> (dint32 &dword)
{
    return *this >> reinterpret_cast<duint32 &>(dword);
}

Reader &Reader::operator >> (duint32 &dword)
{
    d->readBytes(reinterpret_cast<IByteArray::Byte *>(&dword), 4);
    d->convert.networkToHost(dword, dword);
    return *this;
}

Reader &Reader::operator >> (dint64 &qword)
{
    return *this >> reinterpret_cast<duint64 &>(qword);
}

Reader &Reader::operator >> (duint64 &qword)
{
    d->readBytes(reinterpret_cast<IByteArray::Byte *>(&qword), 8);
    d->convert.networkToHost(qword, qword);
    return *this;
}

Reader &Reader::operator >> (dfloat &value)
{
    return *this >> *reinterpret_cast<duint32 *>(&value);
}

Reader &Reader::operator >> (ddouble &value)
{
    return *this >> *reinterpret_cast<duint64 *>(&value);
}

Reader &Reader::operator >> (String &text)
{
    duint size = 0;
    *this >> size;

    Block bytes;
    for (duint i = 0; i < size; ++i)
    {
        IByteArray::Byte ch = 0;
        *this >> ch;
        bytes.append(ch);
    }
    text = String::fromUtf8(bytes);

    return *this;
}

Reader &Reader::operator >> (Block &block)
{
    return *this >> static_cast<IReadable &>(block);
}

Reader &Reader::operator >> (IByteArray &byteArray)
{
    duint32 size = 0;
    *this >> size;

    /**
     * @note  A temporary copy of the contents of the array is made
     * because the destination byte array is not guaranteed to be
     * a memory buffer where you can copy the contents directly.
     */
    Block data(size);
    d->readBytes(data.data(), size);
    byteArray.set(0, data.dataConst(), size);
    return *this;
}

Reader &Reader::operator >> (FixedByteArray &fixedByteArray)
{
    /**
     * @note  A temporary copy of the contents of the array is made
     * because the destination byte array is not guaranteed to be
     * a memory buffer where you can copy the contents directly.
     */
    dsize const size = fixedByteArray.size();
    Block data(size);
    d->readBytes(data.data(), size);
    fixedByteArray.set(0, data.dataConst(), size);
    return *this;
}

Reader &Reader::readBytes(dsize count, IByteArray &destination)
{
    FixedByteArray dest(destination, 0, count);
    return *this >> dest;
}

Reader &Reader::readBytesFixedSize(IByteArray &destination)
{
    FixedByteArray dest(destination);
    return *this >> dest;
}

Reader &Reader::operator >> (IReadable &readable)
{
    readable << *this;
    return *this;
}

Reader &Reader::readUntil(IByteArray &byteArray, IByteArray::Byte delimiter)
{
    dsize pos = 0;
    IByteArray::Byte b = 0;
    do {
        if (atEnd()) break;
        *this >> b;
        byteArray.set(pos++, &b, 1);
    } while (b != delimiter);
    return *this;
}

Reader &Reader::readLine(String &string)
{
    string.clear();

    Block utf;
    readUntil(utf, '\n');

    string = String::fromUtf8(utf);
    string.replace("\r", ""); // strip carriage returns

    return *this;
}

String Reader::readLine()
{
    String s;
    readLine(s);
    return s;
}

IByteArray const *Reader::source() const
{
    return d->source;
}

bool Reader::atEnd() const
{
    return d->atEnd();
}

IByteArray::Offset Reader::offset() const
{
    return d->offset;
}

IByteArray::Size Reader::remainingSize() const
{
    return d->remaining();
}

void Reader::setOffset(IByteArray::Offset offset)
{
    d->offset = offset;
}

void Reader::seek(IByteArray::Delta count)
{
    if (!d->source)
    {
        throw SeekError("Reader::seek", "Cannot seek when reading from a stream");
    }

    auto const seeked = IByteArray::Offset(IByteArray::Delta(d->offset) + count);
    if (seeked >= d->source->size())
    {
        throw IByteArray::OffsetError("Reader::seek", "Seek past bounds of source data");
    }
    d->offset = seeked;
}

void Reader::mark()
{
    d->mark();
}

void Reader::rewind()
{
    d->rewind();
}

ByteOrder const &Reader::byteOrder() const
{
    return d->convert;
}

} // namespace de

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

#ifndef LIBDENG2_LEX_H
#define LIBDENG2_LEX_H

#include "../libcore.h"
#include "../String"

#include <QFlags>

namespace de {

class TokenBuffer;

/**
 * Base class for lexical analyzers. Provides the basic service of reading
 * characters one by one from an input text. It also classifies characters.
 *
 * @ingroup script
 */
class DENG2_PUBLIC Lex
{
public:
    /// Attempt to read characters when there are none left. @ingroup errors
    DENG2_ERROR(OutOfInputError);

    enum ModeFlag {
        DoubleCharComment = 0x1, // Comment start char must be used twice to begin comment.
        RetainComments    = 0x2,
        NegativeNumbers   = 0x4, // If set, '-' preceding a number is included in the literal.
        DefaultMode       = 0
    };
    Q_DECLARE_FLAGS(ModeFlags, ModeFlag)

    /**
     * Utility for setting flags in a Lex instance. The flags specified
     * in the mode span are in effect during the lifetime of the ModeSpan instance.
     * When the ModeSpan goes out of scope, the original flags are restored
     * (the ones that were in use when the ModeSpan was constructed).
     */
    class ModeSpan
    {
    public:
        ModeSpan(Lex &lex, ModeFlags const &m) : _lex(lex), _originalMode(lex._mode) {
            applyFlagOperation(_lex._mode, m, true);
        }

        ~ModeSpan() {
            _lex._mode = _originalMode;
        }

    private:
        Lex &_lex;
        ModeFlags _originalMode;
    };

    // Constants.
    static String const T_PARENTHESIS_OPEN;
    static String const T_PARENTHESIS_CLOSE;
    static String const T_BRACKET_OPEN;
    static String const T_BRACKET_CLOSE;
    static String const T_CURLY_OPEN;
    static String const T_CURLY_CLOSE;

public:
    Lex(String const &input = "",
        QChar lineCommentChar  = QChar('#'),
        QChar multiCommentChar = QChar('\0'),
        ModeFlags initialMode  = DefaultMode);

    /// Returns the input string in its entirety.
    String const &input() const;

    /// Determines if the input string has been entirely read.
    bool atEnd() const;

    /// Returns the current position of the analyzer.
    duint pos() const;

    /// Returns the next character, according to the position.
    /// Characters past the end of the input string are returned
    /// as zero.
    QChar peek() const;

    /// Returns the next character and increments the position.
    /// Returns zero if the end of the input is reached.
    QChar get();

    /// Skips until a non-whitespace character is found.
    void skipWhite();

    /// Skips until a non-whitespace character, or newline, is found.
    void skipWhiteExceptNewline();

    /// Skips until a new line begins.
    void skipToNextLine();

    QChar peekComment() const;

    /// Returns the current line of the reading position. The character
    /// returned from get() will be on this line.
    duint lineNumber() const {
        return _state.lineNumber;
    }

    /// Determines whether there is only whitespace (or nothing)
    /// remaining on the current line.
    bool onlyWhiteOnLine();

    bool atCommentStart() const;

    /// Counts the number of whitespace characters in the beginning of
    /// the current line.
    duint countLineStartSpace() const;

    /// Attempts to parse the current reading position as a C-style number
    /// literal (integer, float, or hexadecimal). It is assumed that a new
    /// token has been started in the @a output buffer, and @a c has already
    /// been added as the token's first character.
    /// @param c Character that begins the number (from get()).
    /// @param output Token buffer.
    /// @return @c true, if a number token was parsed; otherwise @c false.
    bool parseLiteralNumber(QChar c, TokenBuffer &output);

public:
    /// Determines whether a character is whitespace.
    /// @param c Character to check.
    static bool isWhite(QChar c);

    /// Determine whether a character is alphabetic.
    /// @param c Character to check.
    static bool isAlpha(QChar c);

    /// Determine whether a character is numeric.
    /// @param c Character to check.
    static bool isNumeric(QChar c);

    /// Determine whether a character is hexadecimal numeric.
    /// @param c Character to check.
    static bool isHexNumeric(QChar c);

    /// Determine whether a character is alphanumeric.
    /// @param c Character to check.
    static bool isAlphaNumeric(QChar c);

private:
    /// Input text being analyzed.
    String const *_input;

    mutable duint _nextPos;

    struct State {
        duint pos;          ///< Current reading position.
        duint lineNumber;   ///< Keeps track of the line number on which the current position is.
        duint lineStartPos; ///< Position which begins the current line.

        State() : pos(0), lineNumber(1), lineStartPos(0) {}
    };

    State _state;

    /// Character that begins a line comment.
    QChar _lineCommentChar;

    /// Character that begins a multiline comment, if it follows _lineCommentChar.
    /// In reversed order, the characters end a multiline comment.
    QChar _multiCommentChar;

    ModeFlags _mode;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Lex::ModeFlags)

} // namespace de

#endif /* LIBDENG2_LEX_H */

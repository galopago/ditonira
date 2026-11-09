#pragma once

#include <QString>
#include <QVector>
#include <cstdint>

/**
 * ITU Morse encoding (PARIS timing units).
 * Unknown characters are omitted (see docs/ARCHITECTURE.md).
 */
class MorseEncoder
{
public:
    enum class Element : std::uint8_t {
        DitMark,   // key down, 1 unit
        DahMark,   // key down, 3 units
        ElemGap,   // key up, 1 unit (between dit/dah inside a character)
        CharGap,   // key up, 3 units
        WordGap    // key up, 7 units
    };

    struct TimedElement {
        Element kind;
        int units;
        bool keyDown;
    };

    /// Convert text to a timed element sequence. WPM is applied by the caller.
    static QVector<TimedElement> encode(const QString &text);

    /// Marks (dit/dah + intra-char gaps) for one character; empty if unknown/space.
    static QVector<TimedElement> characterMarks(QChar ch);

    /// True if ch maps to an ITU pattern after uppercasing.
    static bool isMappedCharacter(QChar ch);

    /// PARIS unit duration in milliseconds for the given WPM.
    static double unitMs(int wpm);

private:
    static const char *patternFor(QChar ch);
    static void appendPattern(QVector<TimedElement> &out, const char *pattern);
};

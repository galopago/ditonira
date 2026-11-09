#include "MorseEncoder.h"

#include <QChar>

namespace {

void push(QVector<MorseEncoder::TimedElement> &out,
          MorseEncoder::Element kind,
          int units,
          bool keyDown)
{
    out.push_back({kind, units, keyDown});
}

} // namespace

double MorseEncoder::unitMs(int wpm)
{
    if (wpm < 1) {
        wpm = 1;
    }
    return 1200.0 / static_cast<double>(wpm);
}

const char *MorseEncoder::patternFor(QChar ch)
{
    // ITU International Morse; patterns use '.' = dit, '-' = dah
    switch (ch.unicode()) {
    case 'A': return ".-";
    case 'B': return "-...";
    case 'C': return "-.-.";
    case 'D': return "-..";
    case 'E': return ".";
    case 'F': return "..-.";
    case 'G': return "--.";
    case 'H': return "....";
    case 'I': return "..";
    case 'J': return ".---";
    case 'K': return "-.-";
    case 'L': return ".-..";
    case 'M': return "--";
    case 'N': return "-.";
    case 'O': return "---";
    case 'P': return ".--.";
    case 'Q': return "--.-";
    case 'R': return ".-.";
    case 'S': return "...";
    case 'T': return "-";
    case 'U': return "..-";
    case 'V': return "...-";
    case 'W': return ".--";
    case 'X': return "-..-";
    case 'Y': return "-.--";
    case 'Z': return "--..";
    case '0': return "-----";
    case '1': return ".----";
    case '2': return "..---";
    case '3': return "...--";
    case '4': return "....-";
    case '5': return ".....";
    case '6': return "-....";
    case '7': return "--...";
    case '8': return "---..";
    case '9': return "----.";
    case '=': return "-...-";
    case '/': return "-..-.";
    case '?': return "..--..";
    case '.': return ".-.-.-";
    case ',': return "--..--";
    default:  return nullptr;
    }
}

void MorseEncoder::appendPattern(QVector<TimedElement> &out, const char *pattern)
{
    if (!pattern || !*pattern) {
        return;
    }

    bool first = true;
    for (const char *p = pattern; *p; ++p) {
        if (!first) {
            push(out, Element::ElemGap, 1, false);
        }
        first = false;
        if (*p == '.') {
            push(out, Element::DitMark, 1, true);
        } else if (*p == '-') {
            push(out, Element::DahMark, 3, true);
        }
    }
}

bool MorseEncoder::isMappedCharacter(QChar ch)
{
    return patternFor(ch.toUpper()) != nullptr;
}

QVector<MorseEncoder::TimedElement> MorseEncoder::characterMarks(QChar ch)
{
    QVector<TimedElement> out;
    const char *pat = patternFor(ch.toUpper());
    if (pat) {
        appendPattern(out, pat);
    }
    return out;
}

QVector<MorseEncoder::TimedElement> MorseEncoder::encode(const QString &text)
{
    QVector<TimedElement> out;
    const QString normalized = text.toUpper();

    bool needCharGap = false;
    bool pendingWordGap = false;

    for (int i = 0; i < normalized.size(); ++i) {
        const QChar ch = normalized.at(i);

        if (ch.isSpace()) {
            // Collapse whitespace runs into a single word gap later.
            if (needCharGap || !out.isEmpty()) {
                pendingWordGap = true;
                needCharGap = false;
            }
            continue;
        }

        const char *pat = patternFor(ch);
        if (!pat) {
            // Unknown characters are omitted (see docs/ARCHITECTURE.md).
            continue;
        }

        if (pendingWordGap) {
            push(out, Element::WordGap, 7, false);
            pendingWordGap = false;
            needCharGap = false;
        } else if (needCharGap) {
            push(out, Element::CharGap, 3, false);
        }

        appendPattern(out, pat);
        needCharGap = true;
    }

    return out;
}

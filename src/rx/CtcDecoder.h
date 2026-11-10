#pragma once

#include <string>
#include <vector>

namespace deepcw {

struct WordSpaceSpan {
    int startFrame = 0;
    int endFrame = 0;
};

struct CharacterSpan {
    char ch = '\0';
    int startFrame = 0;
    int endFrame = 0;
};

struct CtcDecodeResult {
    std::string plainText;
    std::string displayText;
    std::vector<WordSpaceSpan> wordSpaceSpans;
    std::vector<CharacterSpan> characterSpans;
};

/**
 * Greedy CTC decode for EN Conformer (blank=41, 42 classes).
 * logProbs: row-major [T_out * 42] for batch 0.
 */
CtcDecodeResult decodeEnLogProbs(const float *logProbs, int timeSteps, int numClasses = 42);

} // namespace deepcw

#include "rx/CtcDecoder.h"

#include "rx/DeepCwConstants.h"

#include <limits>

namespace deepcw {
namespace {

std::vector<int> argmaxFrames(const float *logProbs, int timeSteps, int numClasses)
{
    std::vector<int> indices(static_cast<size_t>(timeSteps));
    for (int t = 0; t < timeSteps; ++t) {
        float maxProb = -std::numeric_limits<float>::infinity();
        int maxIndex = 0;
        const int offset = t * numClasses;
        for (int c = 0; c < numClasses; ++c) {
            const float v = logProbs[offset + c];
            if (v > maxProb) {
                maxProb = v;
                maxIndex = c;
            }
        }
        indices[static_cast<size_t>(t)] = maxIndex;
    }
    return indices;
}

std::string decodeCtcPlain(const std::vector<int> &predIndices)
{
    std::string out;
    int previousIndex = -1;
    for (int index : predIndices) {
        if (index == kEnBlankIndex) {
            previousIndex = -1;
            continue;
        }
        if (index == previousIndex) {
            continue;
        }
        previousIndex = index;
        if (index >= 0 && index < kEnVocabSize) {
            out += kEnVocab[index];
        }
    }
    return out;
}

std::string decodeCtcDisplay(const std::vector<int> &predIndices)
{
    std::string out;
    int previousIndex = -1;
    for (int index : predIndices) {
        if (index == kEnBlankIndex) {
            out.push_back(' ');
            previousIndex = -1;
            continue;
        }
        if (index == previousIndex) {
            out.push_back(' ');
            continue;
        }
        previousIndex = index;
        if (index >= 0 && index < kEnVocabSize) {
            out += kEnVocab[index];
        } else {
            out.push_back(' ');
        }
    }
    return out;
}

std::vector<WordSpaceSpan> getWordSpaceSpans(const std::vector<int> &predIndices)
{
    // Vocab space is last symbol before blank (index 40).
    constexpr int spaceIndex = 40;
    std::vector<WordSpaceSpan> spans;
    int currentStart = -1;
    for (int frame = 0; frame < static_cast<int>(predIndices.size()); ++frame) {
        if (predIndices[static_cast<size_t>(frame)] == spaceIndex) {
            if (currentStart < 0) {
                currentStart = frame;
            }
            continue;
        }
        if (currentStart >= 0) {
            spans.push_back({currentStart, frame - 1});
            currentStart = -1;
        }
    }
    if (currentStart >= 0) {
        spans.push_back({currentStart, static_cast<int>(predIndices.size()) - 1});
    }
    return spans;
}

std::vector<CharacterSpan> getCharacterSpans(const std::vector<int> &predIndices)
{
    std::vector<CharacterSpan> spans;
    int previousIndex = -1;
    int active = -1;
    for (int frame = 0; frame < static_cast<int>(predIndices.size()); ++frame) {
        const int index = predIndices[static_cast<size_t>(frame)];
        if (index == kEnBlankIndex) {
            previousIndex = -1;
            active = -1;
            continue;
        }
        if (index == previousIndex) {
            if (active >= 0) {
                spans[static_cast<size_t>(active)].endFrame = frame;
            }
            continue;
        }
        previousIndex = index;
        if (index < 0 || index >= kEnVocabSize) {
            active = -1;
            continue;
        }
        const char *sym = kEnVocab[index];
        if (!sym || !sym[0]) {
            active = -1;
            continue;
        }
        spans.push_back({sym[0], frame, frame});
        active = static_cast<int>(spans.size()) - 1;
    }
    return spans;
}

} // namespace

CtcDecodeResult decodeEnLogProbs(const float *logProbs, int timeSteps, int numClasses)
{
    const auto indices = argmaxFrames(logProbs, timeSteps, numClasses);
    CtcDecodeResult result;
    result.plainText = decodeCtcPlain(indices);
    result.displayText = decodeCtcDisplay(indices);
    result.wordSpaceSpans = getWordSpaceSpans(indices);
    result.characterSpans = getCharacterSpans(indices);
    return result;
}

} // namespace deepcw

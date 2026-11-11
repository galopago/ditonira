#pragma once

namespace deepcw {

/**
 * Process-wide PortAudio init ownership (ARCHITECTURE_RX_DEEPCW.md §10.1).
 * Refcounted: Initialize once, Terminate when last user releases.
 */
class PortAudioHost
{
public:
    /// Ensure Pa_Initialize has succeeded; bump refcount. Returns false on failure.
    static bool acquire();
    /// Drop refcount; Pa_Terminate only at zero.
    static void release();
    static bool isReady();

private:
    PortAudioHost() = delete;
};

} // namespace deepcw

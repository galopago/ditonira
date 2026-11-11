#include "rx/PortAudioHost.h"

#include <portaudio.h>

#include <mutex>

namespace deepcw {
namespace {

std::mutex g_mutex;
int g_refcount = 0;
bool g_ready = false;

} // namespace

bool PortAudioHost::acquire()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_ready) {
        ++g_refcount;
        return true;
    }
    const PaError err = Pa_Initialize();
    if (err != paNoError) {
        g_ready = false;
        g_refcount = 0;
        return false;
    }
    g_ready = true;
    g_refcount = 1;
    return true;
}

void PortAudioHost::release()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_ready || g_refcount <= 0) {
        return;
    }
    --g_refcount;
    if (g_refcount == 0) {
        Pa_Terminate();
        g_ready = false;
    }
}

bool PortAudioHost::isReady()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_ready;
}

} // namespace deepcw

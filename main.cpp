
#include <stdint.h>

#include "third_party/nugget/common/syscalls/syscalls.h"
#include "third_party/nugget/psyqo/advancedpad.hh"
#include "third_party/nugget/psyqo/application.hh"
#include "third_party/nugget/psyqo/font.hh"
#include "third_party/nugget/psyqo/gpu.hh"
#include "third_party/nugget/psyqo/scene.hh"

#include "third_party/nugget/psyqo/hardware/hwregs.hh"
#include "third_party/nugget/psyqo/kernel.hh"

#include "third_party/nugget/psyqo/hardware/cpu.hh"
#include "third_party/nugget/psyqo/hardware/sio.hh"

namespace
{

    // Our application. The PadTest class will be created statically, and run from `main`.
    class PadTest final : public psyqo::Application
    {
        // We will need both methods to properly set up the application.
        void prepare() override;
        void createScene() override;

    public:
        // We will store the font here. We're not going to use chained DMA to display
        // anything, so we don't need multiple fragments. Only a single one will suffice.
        psyqo::Font<1> m_font;

        // Our pad reader.
        psyqo::AdvancedPad m_input;
    };

    // We only have a single since to display the status of the pads.
    class PadTestScene final : public psyqo::Scene
    {
        // Since there's only a single scene, we won't need to override the `start`
        // or `teardown` methods. We will do all the initialization in the application.
        int framecount = 1;
        int padindex = 0;

        void frame() override;

        // Couple of small helpers.
        void print(int x, int y, bool enabled, const char *text);
        void printPadStatus(psyqo::AdvancedPad::Pad pad, int column, const char *name);
        void printPadConnectionStatus(psyqo::AdvancedPad::Pad pad, int row, const char *name);
    };

    PadTest padTest;
    PadTestScene padTestScene;

} // namespace

// The application's `prepare` method is the location to initialize and activate the GPU.
// We shouldn't do anything else that touches the hardware here however, because interrupts
// aren't initialized yet. We could also create more objects here, but we don't have any.
void PadTest::prepare()
{
    psyqo::GPU::Configuration config;
    config.set(psyqo::GPU::Resolution::W320)
        .set(psyqo::GPU::VideoMode::AUTO)
        .set(psyqo::GPU::ColorMode::C15BITS)
        .set(psyqo::GPU::Interlace::PROGRESSIVE);
    gpu().initialize(config);
}

// The `createScene` method will be called automatically to create the first scene. It will
// never exit, so there's no need to cater for the reentrancy case here, but technically,
// this method _can_ be called multiple times, if the last scene got popped out. This would
// be bad in this case because it'd mean we're initializing the pads, and uploading the system
// font multiple times.
void PadTest::createScene()
{
    // We don't have a specific font, so let's just use the built-in system one.
    m_font.uploadSystemFont(gpu());
    // During `createScene`, interrupts are enabled, so it's okay to call `SimplePad::initialize`.
    m_input.initialize();
    // And finally we call `pushScene` with the address of our one and only scene. This last
    // call is mandatory for everything to function properly.
    pushScene(&padTestScene);
}

// Using the system font, our display is roughly 40 columns by 15 lines of text. Although the
// font system can draw text at arbitrary positions on the screen, it's a bit easier to consider
// a matrix of characters instead.
void PadTestScene::print(int x, int y, bool enabled, const char *text)
{
    y += 2;
    psyqo::Vertex pos = {{.x = int16_t(x * 8), .y = int16_t(y * 16)}};
    // Doing these lazy initializations is great for readability and encapsulation,
    // but due to the way C++ works, it'll call into the C++ guard functions every
    // time the function gets called, which may be slower than it could if those
    // were in fact globals.
    static const auto WHITE = psyqo::Color{{.r = 255, .g = 255, .b = 255}};
    static const auto GRAY = psyqo::Color{{.r = 48, .g = 48, .b = 48}};
    psyqo::Color c = enabled ? WHITE : GRAY;
    padTest.m_font.print(padTest.gpu(), text, pos, c);
}
void PadTestScene::printPadConnectionStatus(psyqo::AdvancedPad::Pad pad, int row, const char *name)
{
    auto &input = padTest.m_input;
    print(8, row, input.isPadConnected(pad), name);
}

void PadTestScene::printPadStatus(psyqo::AdvancedPad::Pad pad, int column, const char *name)
{
    auto &input = padTest.m_input;
    print(column + 0, 0, input.isButtonPressed(pad, psyqo::AdvancedPad::Button::Start), "Start");
    print(column + 0, 1, input.isButtonPressed(pad, psyqo::AdvancedPad::Button::Select), "Select");

    print(column + 0, 3, input.isButtonPressed(pad, psyqo::AdvancedPad::Button::L1), "L1");
    print(column + 0, 4, input.isButtonPressed(pad, psyqo::AdvancedPad::Button::R1), "R1");
    print(column + 0, 5, input.isButtonPressed(pad, psyqo::AdvancedPad::Button::L2), "L2");
    print(column + 0, 6, input.isButtonPressed(pad, psyqo::AdvancedPad::Button::R2), "R2");
    print(column + 0, 7, input.isButtonPressed(pad, psyqo::AdvancedPad::Button::L3), "L3");
    print(column + 0, 8, input.isButtonPressed(pad, psyqo::AdvancedPad::Button::R3), "R3");

    print(column + 10, 0, input.isButtonPressed(pad, psyqo::AdvancedPad::Button::Up), "Up");
    print(column + 10, 1, input.isButtonPressed(pad, psyqo::AdvancedPad::Button::Down), "Down");
    print(column + 10, 2, input.isButtonPressed(pad, psyqo::AdvancedPad::Button::Left), "Left");
    print(column + 10, 3, input.isButtonPressed(pad, psyqo::AdvancedPad::Button::Right), "Right");

    print(column + 10, 5, input.isButtonPressed(pad, psyqo::AdvancedPad::Button::Cross), "Cross");
    print(column + 10, 6, input.isButtonPressed(pad, psyqo::AdvancedPad::Button::Circle), "Circle");
    print(column + 10, 7, input.isButtonPressed(pad, psyqo::AdvancedPad::Button::Square), "Square");
    print(column + 10, 8, input.isButtonPressed(pad, psyqo::AdvancedPad::Button::Triangle), "Triangle");
}

// Our rendering function that'll be called periodically.
void PadTestScene::frame()
{
    padTest.gpu().clear();

    if (framecount++ % (60 * 5) == 0)
    {
        // Iterate through the pads, stop on next connected pad
        while (!(padTest.m_input.isPadConnected(static_cast<psyqo::AdvancedPad::Pad>(++padindex))) && padindex < 8)
            ;
        if (padindex > 7)
        {
            padindex = 0;
        }
    }

    print(7, padindex, true, ">");

    printPadConnectionStatus(psyqo::AdvancedPad::Pad1a, 0, "Pad 1a");
    printPadConnectionStatus(psyqo::AdvancedPad::Pad1b, 1, "Pad 1b");
    printPadConnectionStatus(psyqo::AdvancedPad::Pad1c, 2, "Pad 1c");
    printPadConnectionStatus(psyqo::AdvancedPad::Pad1d, 3, "Pad 1d");
    printPadConnectionStatus(psyqo::AdvancedPad::Pad2a, 4, "Pad 2a");
    printPadConnectionStatus(psyqo::AdvancedPad::Pad2b, 5, "Pad 2b");
    printPadConnectionStatus(psyqo::AdvancedPad::Pad2c, 6, "Pad 2c");
    printPadConnectionStatus(psyqo::AdvancedPad::Pad2d, 7, "Pad 2d");

    printPadStatus(static_cast<psyqo::AdvancedPad::Pad>(padindex), 20, "Pad Status");
}

int main() { return padTest.run(); }

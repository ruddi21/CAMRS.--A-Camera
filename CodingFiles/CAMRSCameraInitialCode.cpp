#include <iostream>
#include <cstdlib>
#include <string>
#include <chrono>
#include <thread>
#include <ctime>
#include <gpiod.h>

using namespace std;

// ================= GPIO =================
#define BTN_UP     27
#define BTN_DOWN   17
#define BTN_OK     22
#define BTN_POWER  23

// ================= MODES =================
enum Mode {
    MODE_LIVE,
    MODE_MENU,
    MODE_GALLERY,
    MODE_SETTINGS
};

Mode mode = MODE_LIVE;
bool liveRunning = false;

// ================= UTIL =================
string timestamp() {
    time_t now = time(0);
    tm *ltm = localtime(&now);

    char buf[64];
    strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", ltm);
    return string(buf);
}

// ================= CAMERA =================
void startLive() {
    if (!liveRunning) {
        system("libcamera-vid -t 0 &");
        liveRunning = true;
    }
}

void stopLive() {
    if (liveRunning) {
        system("pkill libcamera-vid");
        liveRunning = false;
    }
}

void capturePhoto() {
    string file = "img_" + timestamp() + ".jpg";

    stopLive();
    this_thread::sleep_for(chrono::milliseconds(200));

    system(("libcamera-still -o " + file).c_str());

    if (mode == MODE_LIVE) startLive();
}

// ================= UI SYSTEM =================

// FULL SCREEN UI DRAW (FRAMEBUFFER STYLE)
void drawUI() {

    // Clear screen buffer (simple approach)
    system("clear");

    cout << "\033[2J\033[H"; // ANSI clear (works in framebuffer console)

    cout << "=================================\n";
    cout << "         CAMERA OS v2            \n";
    cout << "=================================\n\n";

    switch (mode) {

        case MODE_LIVE:
            cout << "[LIVE VIEW]\n";
            cout << "Preview running...\n";
            cout << "OK = capture photo\n";
            cout << "UP = menu\n";
            break;

        case MODE_MENU:
            cout << "[MENU]\n";
            cout << "> 1. Take Photo\n";
            cout << "  2. Gallery\n";
            cout << "  3. Settings\n";
            break;

        case MODE_GALLERY:
            cout << "[GALLERY]\n";
            cout << "Stored images:\n";
            system("ls *.jpg 2>/dev/null | head -5");
            break;

        case MODE_SETTINGS:
            cout << "[SETTINGS]\n";
            cout << "No settings yet\n";
            break;
    }

    cout << "\n=================================\n";
}

// ================= GPIO =================
gpiod_line* getLine(gpiod_chip* chip, int pin) {
    gpiod_line* line = gpiod_chip_get_line(chip, pin);
    gpiod_line_request_input(line, "camera_v2");
    return line;
}

// ================= MAIN =================
int main() {

    cout << "Booting Camera OS v2...\n";

    gpiod_chip* chip = gpiod_chip_open_by_name("gpiochip0");

    auto up     = getLine(chip, BTN_UP);
    auto down   = getLine(chip, BTN_DOWN);
    auto ok     = getLine(chip, BTN_OK);
    auto power  = getLine(chip, BTN_POWER);

    startLive();
    drawUI();

    int menuIndex = 0;

    while (true) {

        int u = gpiod_line_get_value(up);
        int d = gpiod_line_get_value(down);
        int o = gpiod_line_get_value(ok);
        int p = gpiod_line_get_value(power);

        // ================= POWER =================
        if (p == 0) {
            stopLive();
            system("sudo shutdown now");
        }

        // ================= MODE SWITCH =================
        if (u == 0) {
            mode = static_cast<Mode>((mode + 1) % 4);

            if (mode == MODE_LIVE) startLive();
            else stopLive();

            drawUI();
            this_thread::sleep_for(chrono::milliseconds(200));
        }

        // ================= MENU NAV =================
        if (mode == MODE_MENU) {

            if (d == 0) {
                menuIndex = (menuIndex + 1) % 3;
                drawUI();
                this_thread::sleep_for(150ms);
            }

            if (o == 0) {
                if (menuIndex == 0) capturePhoto();
                if (menuIndex == 1) mode = MODE_GALLERY;
                if (menuIndex == 2) mode = MODE_SETTINGS;

                drawUI();
                this_thread::sleep_for(200ms);
            }
        }

        // ================= LIVE MODE =================
        if (mode == MODE_LIVE && o == 0) {
            capturePhoto();
            drawUI();
            this_thread::sleep_for(300ms);
        }

        this_thread::sleep_for(50ms);
    }

    gpiod_chip_close(chip);
    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// learn bin sh zsh bash toggle-osx-clipboard
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <conio.h> // For _kbhit() and _getch() on Windows
DWORD original_input_mode;
DWORD original_output_mode;
HANDLE hInputStream = INVALID_HANDLE_VALUE;
HANDLE hOutputStream = INVALID_HANDLE_VALUE;
#else
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#endif
//==================
// ENUMS AND STRUCTS
//==================
enum EDITOR_KEY {
    ARROW_UP = 1000,
    ARROW_DOWN,
    ARROW_LEFT,
    ARROW_RIGHT,
    PAGE_UP,
    PAGE_DOWN,
    HOME,
    END,
} EDITOR_KEY;

enum CONTROL_CHAR {
    NUL = 0,               // Null
    START_HEADING = 1,     // Start of Heading
    START_TEXT = 2,        // Start of Text
    END_TEXT = 3,          // End of Text
    END_TRANSMISSION = 4,  // End of Transmission
    ENQUIRY = 5,           // Enquiry
    ACKNOWLEDGE = 6,       // Acknowledge
    BELL = 7,              // Bell
    BACK_SPACE = 8,        // Backspace
    HORIZONTAL_TAB = 9,    // Horizontal Tab
    LINE_FEED = 10,        // Line Feed
    VERTICAL_TAB = 11,     // Vertical Tab
    FORM_FEED = 12,        // Form Feed
    CARRIAGE_RETURN = 13,  // Carriage Return
    SHIFT_OUT = 14,        // Shift Out
    SHITF_IN = 15,         // Shift In
    DATA_LINK_SCAPE = 16,  // Data Link Escape
    DC1 = 17,              // Device Control 1 (XON)
    DC2 = 18,              // Device, Control 2
    DC3 = 19,              // Device Control 3 (XOFF)
    DC4 = 20,              // Device Control 4
    NAK = 21,              // Negative Acknowledge
    SYN = 22,              // Synchronous Idle
    ETB = 23,              // End of Transmission Block
    CANCEL = 24,           // Cancel
    EM = 25,               // End of Medium
    SUB = 26,              // Substitute
    ESC = 27,              // Escape
    FILE_SEPARATOR = 28,   // File Separator
    GROUP_SEPARATOR = 29,  // Group Separator
    RECORD_SEPARATOR = 30, // Record Separator
    US = 31,               // Unit Separator
    DEL = 127              // Delete
} CONTROL_CHAR;

#if !defined(_WIN32) && !defined(_WIN64)
struct termios orig_termios;

void raw_mode_off() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void raw_mode_on() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(raw_mode_off);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON); // turn off echoing and canonical mode
    // VMIN = 0, VTIME = 1 means read() times out after 100ms if no input is ready
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}
#else
// windows bullshit for raw mode

void enable_windows_raw_mode() {
    // 1. Get handles to the standard console streams
    hInputStream = GetStdHandle(STD_INPUT_HANDLE);
    hOutputStream = GetStdHandle(STD_OUTPUT_HANDLE);

    if (hInputStream == INVALID_HANDLE_VALUE || hOutputStream == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Failed to get console handles.\n");
        exit(1);
    }

    // 2. Save original modes so we can restore them later
    GetConsoleMode(hInputStream, &original_input_mode);
    GetConsoleMode(hOutputStream, &original_output_mode);

    // Register the automatic cleanup macro
    atexit(disable_windows_raw_mode);

    // 3. Configure Input Mode (Disable line buffering, echoing, and processing)
    // Stripping ENABLE_PROCESSED_INPUT prevents Ctrl+C from instantly killing the app
    DWORD raw_input = original_input_mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
    SetConsoleMode(hInputStream, raw_input);

    // 4. Configure Output Mode (Enable ANSI/VT Escape sequence support)
    // ENABLE_VIRTUAL_TERMINAL_PROCESSING tells Windows to parse \x1b codes
    // DISABLE_NEWLINE_AUTO_RETURN prevents accidental side-scroll skips at the screen edge
    DWORD raw_output = original_output_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
#ifndef DISABLE_NEWLINE_AUTO_RETURN
#define DISABLE_NEWLINE_AUTO_RETURN 0x0008
#endif
    raw_output |= DISABLE_NEWLINE_AUTO_RETURN;

    SetConsoleMode(hOutputStream, raw_output);

    // Set console output code page to UTF-8 so icons/borders look crisp
    SetConsoleOutputCP(CP_UTF8);
}
void disable_windows_raw_mode() {
    if (hInputStream != INVALID_HANDLE_VALUE) {
        SetConsoleMode(hInputStream, original_input_mode);
    }
    if (hOutputStream != INVALID_HANDLE_VALUE) {
        SetConsoleMode(hOutputStream, original_output_mode);
    }
}
#endif

//===============
// GETTING KEYS
//==============
int read_raw_byte(void) {
#if defined(_WIN32) || defined(_WIN64)
    // Windows implementation
    if (_kbhit()) {
        return _getch();
    }
    return -1; // No key pressed
#else
    char c;
    int nread = read(STDIN_FILENO, &c, 1);
    if (nread == 1) return c;
    return -1;
#endif
}



void print_raw_key_stream(void) {
    int c;

    // Wait for the FIRST byte to kick off the stream
    while ((c = read_raw_byte()) == -1) {
#if defined(_WIN32) || defined(_WIN64)
        Sleep(10);
#endif
    }

    printf("\r\n--- RAW BYTE STREAM DETECTED ---\r\n");
    printf("| Byte # | Decimal | Hex  | Character representation |\r\n");
    printf("|--------|---------|------|--------------------------|\r\n");

    int byte_count = 1;

    // Loop through the first byte and ANY subsequent bytes packed closely behind it
    while (c != -1) {
        printf("| %-6d | %-7d | 0x%-2X | ", byte_count++, c, c);

        // Make control characters readable
        if (c == 27) {
            printf("[ESC]                    |\r\n");
        } else if (c == '\r') {
            printf("[CR] (Carriage Return)   |\r\n");
        } else if (c == '\n') {
            printf("[LF] (Line Feed)         |\r\n");
        } else if (c == '\t') {
            printf("[TAB]                    |\r\n");
        } else if (c >= 32 && c <= 126) {
            printf("'%c'                      |\r\n", c);
        } else {
            printf("[Control/Special Key]    |\r\n");
        }
// Check if another byte is immediately waiting behind it
#if defined(_WIN32) || defined(_WIN64)
        // Give Windows a microscopic window to populate its buffer if a combo key was hit
        Sleep(10);
#endif
        c = read_raw_byte();
    }
    printf("--------------------------------\r\n");
}

void print_extra_raw_key_stream(void) {
    int c;

    // Wait for the FIRST byte to kick off the stream
    while ((c = read_raw_byte()) == -1) {
#if defined(_WIN32) || defined(_WIN64)
        Sleep(10);
#endif
    }

    printf("\r\n--- RAW BYTE STREAM DETECTED ---\r\n");
    printf("| Byte # | Decimal | Hex  | Character representation |\r\n");
    printf("|--------|---------|------|--------------------------|\r\n");

    int byte_count = 1;

    // Loop through the first byte and ANY subsequent bytes packed closely behind it
    while (c != -1) {
        printf("| %-6d | %-7d | 0x%-2X | ", byte_count++, c, c);
       
        if (c >= 32 && c <= 126) {
            printf("'%c'                      |\r\n", c);
        } else {
            switch (c) {
            case NUL: printf("NUL                 |\r\n"); break;
            case START_HEADING: printf("START_HEADING    |\r\n"); break;
            case START_TEXT: printf("START_TEXT          |\r\n"); break;
            case END_TEXT: printf("END_TEXT              |\r\n"); break;
            case END_TRANSMISSION: printf("END_TRANSMISSION             |\r\n"); break;
            case ENQUIRY: printf("ENQUIRY                 |\r\n"); break;
            case ACKNOWLEDGE: printf("ACKNOWLEDGE             |\r\n"); break;
            case BELL: printf("BELL               |\r\n"); break;
            case BACK_SPACE: printf("BACK_SPACE                   |\r\n"); break;
            case HORIZONTAL_TAB: printf("HORIZONTAL_TAB           |\r\n"); break;
            case LINE_FEED: printf("LINE_FEED                     |\r\n"); break;
            case VERTICAL_TAB: printf("VERTICAL_TAB               |\r\n"); break;
            case FORM_FEED: printf("FORM_FEED                     |\r\n"); break;
            case CARRIAGE_RETURN: printf("CARRIAGE_RETURN         |\r\n"); break;
            case SHIFT_OUT: printf("SHIFT_OUT                     |\r\n"); break;
            case SHITF_IN: printf("SHITF_IN                       |\r\n"); break;
            case DATA_LINK_SCAPE: printf("DATA_LINK_SCAPE         |\r\n"); break;
            case DC1: printf("DC1                         |\r\n"); break;
            case DC2: printf("DC2                         |\r\n"); break;
            case DC3: printf("DC3                         |\r\n"); break;
            case DC4: printf("DC4                         |\r\n"); break;
            case NAK: printf("NAK                         |\r\n"); break;
            case SYN: printf("SYN                         |\r\n"); break;
            case ETB: printf("ETB                         |\r\n"); break;
            case CANCEL: printf("CANCEL                   |\r\n"); break;
            case EM: printf("EM                      |\r\n"); break;
            case SUB: printf("SUB                    |\r\n"); break;
            case ESC: printf("ESC                    |\r\n"); break;
            case FILE_SEPARATOR: printf("FILE_SEPARATOR               |\r\n"); break;
            case GROUP_SEPARATOR: printf("GROUP_SEPARATOR              |\r\n"); break;
            case RECORD_SEPARATOR: printf(" RECORD_SEPARATOR            |\r\n "); break;
            case US: printf("US                      |\r\n"); break;
            case DEL: printf("DEL                    |\r\n"); break;
            }
        }
// Check if another byte is immediately waiting behind it
#if defined(_WIN32) || defined(_WIN64)
        // Give Windows a microscopic window to populate its buffer if a combo key was hit
        Sleep(10);
#endif
        c = read_raw_byte();
    }
    printf("--------------------------------\r\n");
}
int main() {

#if !defined(_WIN32) || !defined(_WIN64)

    printf("ON MAC-LINUX");
    raw_mode_on();
#else

    printf("ON WINDOWS");
    enable_windows_raw_mode(); // Windows pathway
#endif
    printf("RAW KEY SNIFFER ACTIVE.\r\n");
    printf("Press keys to analyze their raw bytes. Press Ctrl+C or ESC to close.\r\n");

    while (1) {
        print_extra_raw_key_stream();
    }

    return 0;
    
}

#ifndef _H_KEYCODES_
#define _H_KEYCODES_

#include "types.h"

// Platform-independent key codes
// Values match X11 keysyms for direct use with X11
namespace Key {
    // Special keys
    constexpr i32 BACKSPACE = 0xFF08;   // XK_BackSpace
    constexpr i32 TAB = 0xFF09;         // XK_Tab
    constexpr i32 RETURN = 0xFF0D;      // XK_Return
    constexpr i32 ESCAPE = 0xFF1B;      // XK_Escape
    constexpr i32 DELETE = 0xFFFF;      // XK_Delete

    // Navigation keys
    constexpr i32 HOME = 0xFF50;        // XK_Home
    constexpr i32 LEFT = 0xFF51;        // XK_Left
    constexpr i32 UP = 0xFF52;          // XK_Up
    constexpr i32 RIGHT = 0xFF53;       // XK_Right
    constexpr i32 DOWN = 0xFF54;        // XK_Down
    constexpr i32 PAGE_UP = 0xFF55;     // XK_Page_Up
    constexpr i32 PAGE_DOWN = 0xFF56;   // XK_Page_Down
    constexpr i32 END = 0xFF57;         // XK_End

    // Punctuation and symbols (ASCII values)
    constexpr i32 SPACE = 0x0020;
    constexpr i32 PLUS = 0x002B;
    constexpr i32 MINUS = 0x002D;
    constexpr i32 EQUALS = 0x003D;
    constexpr i32 COMMA = 0x002C;
    constexpr i32 PERIOD = 0x002E;
    constexpr i32 SEMICOLON = 0x003B;
    constexpr i32 SLASH = 0x002F;
    constexpr i32 BACKSLASH = 0x005C;
    constexpr i32 LEFTBRACKET = 0x005B;
    constexpr i32 RIGHTBRACKET = 0x005D;
    constexpr i32 QUOTE = 0x0027;
    constexpr i32 BACKQUOTE = 0x0060;

    // Number keys (ASCII values)
    constexpr i32 KEY_0 = 0x0030;
    constexpr i32 KEY_1 = 0x0031;
    constexpr i32 KEY_2 = 0x0032;
    constexpr i32 KEY_3 = 0x0033;
    constexpr i32 KEY_4 = 0x0034;
    constexpr i32 KEY_5 = 0x0035;
    constexpr i32 KEY_6 = 0x0036;
    constexpr i32 KEY_7 = 0x0037;
    constexpr i32 KEY_8 = 0x0038;
    constexpr i32 KEY_9 = 0x0039;

    // Letter keys (lowercase ASCII values)
    constexpr i32 A = 0x0061;
    constexpr i32 B = 0x0062;
    constexpr i32 C = 0x0063;
    constexpr i32 D = 0x0064;
    constexpr i32 E = 0x0065;
    constexpr i32 F = 0x0066;
    constexpr i32 G = 0x0067;
    constexpr i32 H = 0x0068;
    constexpr i32 I = 0x0069;
    constexpr i32 J = 0x006A;
    constexpr i32 K = 0x006B;
    constexpr i32 L = 0x006C;
    constexpr i32 M = 0x006D;
    constexpr i32 N = 0x006E;
    constexpr i32 O = 0x006F;
    constexpr i32 P = 0x0070;
    constexpr i32 Q = 0x0071;
    constexpr i32 R = 0x0072;
    constexpr i32 S = 0x0073;
    constexpr i32 T = 0x0074;
    constexpr i32 U = 0x0075;
    constexpr i32 V = 0x0076;
    constexpr i32 W = 0x0077;
    constexpr i32 X = 0x0078;
    constexpr i32 Y = 0x0079;
    constexpr i32 Z = 0x007A;

    // Function keys
    constexpr i32 F1 = 0xFFBE;          // XK_F1
    constexpr i32 F2 = 0xFFBF;
    constexpr i32 F3 = 0xFFC0;
    constexpr i32 F4 = 0xFFC1;
    constexpr i32 F5 = 0xFFC2;
    constexpr i32 F6 = 0xFFC3;
    constexpr i32 F7 = 0xFFC4;
    constexpr i32 F8 = 0xFFC5;
    constexpr i32 F9 = 0xFFC6;
    constexpr i32 F10 = 0xFFC7;
    constexpr i32 F11 = 0xFFC8;
    constexpr i32 F12 = 0xFFC9;

    // Modifier keys (for reference, usually detected via event state)
    constexpr i32 SHIFT_L = 0xFFE1;     // XK_Shift_L
    constexpr i32 SHIFT_R = 0xFFE2;     // XK_Shift_R
    constexpr i32 CONTROL_L = 0xFFE3;   // XK_Control_L
    constexpr i32 CONTROL_R = 0xFFE4;   // XK_Control_R
    constexpr i32 ALT_L = 0xFFE9;       // XK_Alt_L
    constexpr i32 ALT_R = 0xFFEA;       // XK_Alt_R
}

#endif

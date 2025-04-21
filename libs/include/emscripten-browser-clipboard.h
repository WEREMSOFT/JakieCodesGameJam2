#ifndef IMGUI_CLIPBOARD_EMSCRIPTEN_H
#define IMGUI_CLIPBOARD_EMSCRIPTEN_H

#ifdef __EMSCRIPTEN__

#include <string.h>
#include <stdlib.h>
#include <emscripten.h>
#include "cimgui.h"

static char g_clipboard_buffer[2048] = "";

// Clipboard Get Function
static const char* imgui_clipboard_get_text_ctx(ImGuiContext* ctx) {
    (void)ctx; // unused
	printf("imgui_clipboard_get_text_ctx: %s\n", g_clipboard_buffer);
    return g_clipboard_buffer;
}

// Clipboard Set Function
static void imgui_clipboard_set_text_ctx(ImGuiContext* ctx, const char* text) {
    (void)ctx; // unused
	printf("imgui_clipboard_set_text_ctx: %s\n", text);
    strncpy(g_clipboard_buffer, text, sizeof(g_clipboard_buffer) - 1);
    g_clipboard_buffer[sizeof(g_clipboard_buffer) - 1] = '\0';
}

// Function to be called from JS paste event
EMSCRIPTEN_KEEPALIVE
void imgui_clipboard_set_from_js(const char* text) {
    imgui_clipboard_set_text_ctx(NULL, text);
	printf("paste called from javscript\n");
}

// Register JS event listener
static inline void imgui_clipboard_setup_js_hook() {
    EM_ASM({
        document.addEventListener('paste', function(e) {
            const text = e.clipboardData.getData('text/plain');
            const len = lengthBytesUTF8(text) + 1;
            const ptr = _malloc(len);
            stringToUTF8(text, ptr, len);
            Module._imgui_clipboard_set_from_js(ptr);
            _free(ptr);
        });
    });
}

// Init function (call after ImGui initialization)
static inline void imgui_clipboard_init() {
    ImGuiIO* io = igGetIO();
    io->Ctx->PlatformIO.Platform_GetClipboardTextFn = imgui_clipboard_get_text_ctx;
    io->Ctx->PlatformIO.Platform_SetClipboardTextFn = imgui_clipboard_set_text_ctx;
    imgui_clipboard_setup_js_hook();
}

#endif // __EMSCRIPTEN__
#endif // IMGUI_CLIPBOARD_EMSCRIPTEN_H

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <string.h>

#define MAX_INTERCHANGE_BUFFER 8000

const char* get_browser_url() {
    static char url_buffer[MAX_INTERCHANGE_BUFFER];
    const char* url = emscripten_run_script_string("window.location.href");
    strncpy(url_buffer, url, sizeof(url_buffer));
    return url_buffer;
}

void set_browser_url(const char* new_url) {
    // Usa replaceState para cambiar la URL sin recargar la página
    char script[MAX_INTERCHANGE_BUFFER];
    snprintf(script, sizeof(script),
             "window.parent.history.replaceState(null, '', '%s');", new_url);
    emscripten_run_script(script);
}

const char* get_console_text(char *buffer) {
    buffer =  emscripten_run_script_string("document.getElementById('output').value");
	return buffer;
}

void clear_output_console() {
    emscripten_run_script("document.getElementById('output').value = '';");
}

char url[MAX_INTERCHANGE_BUFFER];

void save_level_to_url(const char* level_data)
{
	char js[MAX_INTERCHANGE_BUFFER];
     snprintf(js, sizeof(js),
        "window.location.hash = '#'+btoa(`%s`)",
        level_data);
    emscripten_run_script(js);
	printf("%s\n", get_browser_url());
}

void load_level_from_url(char* out_buffer, int buffer_size)
{
    const char* js_result = emscripten_run_script_string(
        "(() => {"
        " if (!window.location.hash) return '';"
        " try {"
        "   return atob(window.location.hash.slice(1));"
        " } catch(e) { return ''; }"
        "})()"
    );

    strncpy(out_buffer, js_result, buffer_size);
    out_buffer[buffer_size - 1] = '\0'; // Null-terminate
}

#endif
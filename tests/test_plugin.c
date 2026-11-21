/*
 * Automated syntax and behavior test suite for zsh-dropdown.plugin.zsh.
 *
 * Build & run:
 *   cc -Wall -Wextra -Werror -std=c11 -O2 -o tests/test_plugin tests/test_plugin.c
 *   ./tests/test_plugin
 */

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define OUT_CAP 16384
#define SCRIPT_CAP (PATH_MAX + 512)

static char g_plugin_path[PATH_MAX];

typedef struct {
    const char *key;
    const char *val; /* NULL unsets the variable */
} EnvVar;

static void resolve_plugin_path(const char *argv0) {
    char exe_dir[PATH_MAX - 64];
    const char *slash = strrchr(argv0, '/');
    if (slash != NULL) {
        size_t len = (size_t)(slash - argv0);
        if (len >= sizeof(exe_dir)) {
            fprintf(stderr, "Path too long\n");
            exit(1);
        }
        memcpy(exe_dir, argv0, len);
        exe_dir[len] = '\0';
        snprintf(g_plugin_path, sizeof(g_plugin_path), "%s/../zsh-dropdown.plugin.zsh", exe_dir);
    } else {
        snprintf(g_plugin_path, sizeof(g_plugin_path), "zsh-dropdown.plugin.zsh");
    }

    if (access(g_plugin_path, R_OK) != 0) {
        /* Fallback when invoked from repo root */
        snprintf(g_plugin_path, sizeof(g_plugin_path), "zsh-dropdown.plugin.zsh");
        if (access(g_plugin_path, R_OK) != 0) {
            fprintf(stderr, "Cannot locate zsh-dropdown.plugin.zsh\n");
            exit(1);
        }
    }
}

static int run_zsh(const char *const argv[], const EnvVar *envs, size_t n_envs,
                   char *out_buf, size_t out_cap) {
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        perror("pipe");
        exit(1);
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        for (size_t i = 0; i < n_envs; i++) {
            if (envs[i].val == NULL) {
                unsetenv(envs[i].key);
            } else {
                setenv(envs[i].key, envs[i].val, 1);
            }
        }

        execvp("zsh", (char *const *)argv);
        perror("execvp zsh");
        _exit(127);
    }

    close(pipefd[1]);
    size_t total = 0;
    while (total + 1 < out_cap) {
        ssize_t n = read(pipefd[0], out_buf + total, out_cap - 1 - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (n == 0) break;
        total += (size_t)n;
    }
    out_buf[total] = '\0';
    close(pipefd[0]);

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) break;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

static void assert_contains(const char *test_name, const char *haystack, const char *needle) {
    if (strstr(haystack, needle) == NULL) {
        fprintf(stderr, "[FAIL] %s: expected substring '%s'\nActual output:\n%s\n",
                test_name, needle, haystack);
        exit(1);
    }
}

static void trim_trailing_ws(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ' || s[n - 1] == '\t')) {
        s[--n] = '\0';
    }
}

static void test_syntax(void) {
    char out[OUT_CAP];
    const char *argv[] = {"zsh", "-n", g_plugin_path, NULL};
    int rc = run_zsh(argv, NULL, 0, out, sizeof(out));
    if (rc != 0) {
        fprintf(stderr, "[FAIL] test_syntax (rc=%d):\n%s\n", rc, out);
        exit(1);
    }
    printf("[pass] zsh -n syntax check\n");
}

static void test_prefix_defaults(void) {
    char script[SCRIPT_CAP];
    char out[OUT_CAP];
    snprintf(script, sizeof(script),
             "source \"%s\"\n"
             "zstyle -L ':completion:*' completer\n"
             "zstyle -L ':completion:*' matcher-list\n"
             "zstyle -L ':autocomplete:*' min-input\n"
             "zstyle -L ':autocomplete:*' delay\n"
             "zstyle -L ':autocomplete:*' list-lines\n",
             g_plugin_path);

    const char *argv[] = {"zsh", "-f", "-c", script, NULL};
    int rc = run_zsh(argv, NULL, 0, out, sizeof(out));
    if (rc != 0) {
        fprintf(stderr, "[FAIL] test_prefix_defaults (rc=%d):\n%s\n", rc, out);
        exit(1);
    }

    assert_contains("test_prefix_defaults", out, "_expand _complete _prefix _ignored");
    assert_contains("test_prefix_defaults", out, "m:{[:lower:]}={[:upper:]}");
    assert_contains("test_prefix_defaults", out, "min-input 1");
    assert_contains("test_prefix_defaults", out, "delay 0.05");
    assert_contains("test_prefix_defaults", out, "list-lines 10");
    printf("[pass] default prefix & timing zstyles\n");
}

static void test_themes(void) {
    const struct {
        const char *name;
        const char *expected;
    } cases[] = {
        {"github-dark", "ma=48;2;38;79;120;1;97"},
        {"catppuccin",  "ma=48;2;69;71;90;1;38;2;205;214;244"},
        {"tokyonight",  "ma=48;2;40;52;87;1;38;2;192;202;245"},
        {"none",        ""},
    };

    char script[SCRIPT_CAP];
    char out[OUT_CAP];
    snprintf(script, sizeof(script),
             "source \"%s\"\n"
             "zstyle -L ':completion:*:default' list-colors || true\n",
             g_plugin_path);

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        EnvVar envs[] = {
            {"ZSH_DROPDOWN_THEME", cases[i].name},
            {"COLORTERM", "truecolor"},
            {"TERM_PROGRAM", "ghostty"},
        };
        const char *argv[] = {"zsh", "-f", "-c", script, NULL};
        int rc = run_zsh(argv, envs, 3, out, sizeof(out));
        if (rc != 0) {
            fprintf(stderr, "[FAIL] test_themes(%s) rc=%d:\n%s\n", cases[i].name, rc, out);
            exit(1);
        }
        if (cases[i].expected[0] != '\0') {
            assert_contains(cases[i].name, out, cases[i].expected);
        } else {
            trim_trailing_ws(out);
            if (out[0] != '\0') {
                fprintf(stderr, "[FAIL] theme 'none' should not set list-colors, got:\n%s\n", out);
                exit(1);
            }
        }
    }

    /* Verify automatic 256-color fallback on macOS Terminal.app (Apple_Terminal) */
    EnvVar env_apple[] = {
        {"ZSH_DROPDOWN_THEME", "github-dark"},
        {"TERM", "xterm-256color"},
        {"TERM_PROGRAM", "Apple_Terminal"},
        {"COLORTERM", ""},
    };
    const char *argv_apple[] = {"zsh", "-f", "-c", script, NULL};
    run_zsh(argv_apple, env_apple, 4, out, sizeof(out));
    assert_contains("Apple_Terminal 256-color fallback", out, "ma=48;5;24;1;97");
    printf("[pass] all theme presets + macOS Terminal.app 256-color fallback\n");
}

static void test_sync_detection(void) {
    char script[SCRIPT_CAP];
    char out[OUT_CAP];
    snprintf(script, sizeof(script),
             "source \"%s\"\n"
             "_zsh_dropdown_supports_sync && echo YES || echo NO\n",
             g_plugin_path);
    const char *argv[] = {"zsh", "-f", "-c", script, NULL};

    /* 1. Ghostty via TERM */
    EnvVar env_ghostty[] = {
        {"TERM", "xterm-ghostty"}, {"TERM_PROGRAM", ""}, {"GHOSTTY_RESOURCES_DIR", ""},
        {"KITTY_WINDOW_ID", ""}, {"WT_SESSION", ""}, {"KONSOLE_VERSION", ""},
        {"VTE_VERSION", ""}, {"TMUX", ""}, {"STY", ""},
        {"ZSH_DROPDOWN_SYNC_REDRAW", "auto"},
    };
    run_zsh(argv, env_ghostty, 10, out, sizeof(out));
    trim_trailing_ws(out);
    if (strcmp(out, "YES") != 0) {
        fprintf(stderr, "[FAIL] ghostty sync detection expected YES, got '%s'\n", out);
        exit(1);
    }

    /* 2. Warp & Konsole detection */
    EnvVar env_warp[] = {
        {"TERM", "xterm-256color"}, {"TERM_PROGRAM", "WarpTerminal"}, {"GHOSTTY_RESOURCES_DIR", ""},
        {"KITTY_WINDOW_ID", ""}, {"WT_SESSION", ""}, {"KONSOLE_VERSION", ""},
        {"VTE_VERSION", ""}, {"TMUX", ""}, {"STY", ""},
        {"ZSH_DROPDOWN_SYNC_REDRAW", "auto"},
    };
    run_zsh(argv, env_warp, 10, out, sizeof(out));
    trim_trailing_ws(out);
    if (strcmp(out, "YES") != 0) {
        fprintf(stderr, "[FAIL] Warp sync detection expected YES, got '%s'\n", out);
        exit(1);
    }

    EnvVar env_konsole[] = {
        {"TERM", "xterm-256color"}, {"TERM_PROGRAM", ""}, {"GHOSTTY_RESOURCES_DIR", ""},
        {"KITTY_WINDOW_ID", ""}, {"WT_SESSION", ""}, {"KONSOLE_VERSION", "240200"},
        {"VTE_VERSION", ""}, {"TMUX", ""}, {"STY", ""},
        {"ZSH_DROPDOWN_SYNC_REDRAW", "auto"},
    };
    run_zsh(argv, env_konsole, 10, out, sizeof(out));
    trim_trailing_ws(out);
    if (strcmp(out, "YES") != 0) {
        fprintf(stderr, "[FAIL] Konsole sync detection expected YES, got '%s'\n", out);
        exit(1);
    }

    /* 3. tmux & GNU screen auto guards skip Mode 2026 */
    EnvVar env_tmux[] = {
        {"TERM", "xterm-ghostty"}, {"TERM_PROGRAM", "ghostty"}, {"GHOSTTY_RESOURCES_DIR", "/app"},
        {"KITTY_WINDOW_ID", ""}, {"WT_SESSION", ""}, {"KONSOLE_VERSION", ""},
        {"VTE_VERSION", ""}, {"TMUX", "/tmp/tmux-501/default,123,0"}, {"STY", ""},
        {"ZSH_DROPDOWN_SYNC_REDRAW", "auto"},
    };
    run_zsh(argv, env_tmux, 10, out, sizeof(out));
    trim_trailing_ws(out);
    if (strcmp(out, "NO") != 0) {
        fprintf(stderr, "[FAIL] tmux auto guard expected NO, got '%s'\n", out);
        exit(1);
    }

    EnvVar env_screen[] = {
        {"TERM", "xterm-ghostty"}, {"TERM_PROGRAM", "ghostty"}, {"GHOSTTY_RESOURCES_DIR", "/app"},
        {"KITTY_WINDOW_ID", ""}, {"WT_SESSION", ""}, {"KONSOLE_VERSION", ""},
        {"VTE_VERSION", ""}, {"TMUX", ""}, {"STY", "1234.pts-0.host"},
        {"ZSH_DROPDOWN_SYNC_REDRAW", "auto"},
    };
    run_zsh(argv, env_screen, 10, out, sizeof(out));
    trim_trailing_ws(out);
    if (strcmp(out, "NO") != 0) {
        fprintf(stderr, "[FAIL] screen auto guard expected NO, got '%s'\n", out);
        exit(1);
    }

    /* 4. Legacy dumb terminal */
    EnvVar env_dumb[] = {
        {"TERM", "dumb"}, {"TERM_PROGRAM", ""}, {"GHOSTTY_RESOURCES_DIR", ""},
        {"KITTY_WINDOW_ID", ""}, {"WT_SESSION", ""}, {"KONSOLE_VERSION", ""},
        {"VTE_VERSION", ""}, {"TMUX", ""}, {"STY", ""},
        {"ZSH_DROPDOWN_SYNC_REDRAW", "auto"},
    };
    run_zsh(argv, env_dumb, 10, out, sizeof(out));
    trim_trailing_ws(out);
    if (strcmp(out, "NO") != 0) {
        fprintf(stderr, "[FAIL] dumb terminal sync detection expected NO, got '%s'\n", out);
        exit(1);
    }

    /* 5. Explicit override always (even inside tmux) */
    EnvVar env_always[] = {
        {"TERM", "dumb"}, {"TERM_PROGRAM", ""}, {"TMUX", "/tmp/tmux-501/default,123,0"},
        {"ZSH_DROPDOWN_SYNC_REDRAW", "always"},
    };
    run_zsh(argv, env_always, 4, out, sizeof(out));
    trim_trailing_ws(out);
    if (strcmp(out, "YES") != 0) {
        fprintf(stderr, "[FAIL] sync=always expected YES, got '%s'\n", out);
        exit(1);
    }

    printf("[pass] Mode 2026 detection (Ghostty / Warp / Konsole / tmux / screen / dumb / always)\n");
}

static void test_fuzzy_mode_opt_out(void) {
    char script[SCRIPT_CAP];
    char out[OUT_CAP];
    snprintf(script, sizeof(script),
             "source \"%s\"\n"
             "zstyle -L ':completion:*' completer || true\n",
             g_plugin_path);

    EnvVar envs[] = {{"ZSH_DROPDOWN_MODE", "fuzzy"}};
    const char *argv[] = {"zsh", "-f", "-c", script, NULL};
    int rc = run_zsh(argv, envs, 1, out, sizeof(out));
    if (rc != 0) {
        fprintf(stderr, "[FAIL] test_fuzzy_mode_opt_out rc=%d:\n%s\n", rc, out);
        exit(1);
    }
    trim_trailing_ws(out);
    if (out[0] != '\0') {
        fprintf(stderr, "[FAIL] fuzzy mode should not override completer, got:\n%s\n", out);
        exit(1);
    }
    printf("[pass] fuzzy mode leaves completer untouched\n");
}

int main(int argc, char **argv) {
    (void)argc;
    resolve_plugin_path(argv[0]);

    test_syntax();
    test_prefix_defaults();
    test_themes();
    test_sync_detection();
    test_fuzzy_mode_opt_out();

    printf("\nAll tests passed!\n");
    return 0;
}

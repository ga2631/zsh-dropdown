# zsh-dropdown.plugin.zsh
# Fast, flicker-free, themed live completion dropdown for Zsh / Oh My Zsh.
# Built on top of `zsh-autocomplete` with DEC Mode 2026 synchronized redraws,
# Zsh 5.9 zasync autoload self-healing, automatic 24-bit/256-color adaptation,
# and curated terminal color themes.
#
# Repository: https://github.com/ga2631/zsh-dropdown
# License: MIT

# Helper: detect 24-bit True Color terminals (falls back to 256-color on macOS Terminal.app, etc.)
_zsh_dropdown_supports_truecolor() {
  [[ "${TERM_PROGRAM:-}" == "Apple_Terminal" ]] && return 1
  case "${COLORTERM:-}" in
    truecolor|24bit) return 0 ;;
  esac
  case "${TERM:-}" in
    *direct*|*ghostty*|*kitty*|*wezterm*|*alacritty*|*foot*|*contour*|*rio*) return 0 ;;
  esac
  case "${TERM_PROGRAM:-}" in
    ghostty|WezTerm|iTerm.app|kitty|Alacritty|Rio|vscode|WarpTerminal|Tabby|Hyper) return 0 ;;
  esac
  [[ -n "${GHOSTTY_RESOURCES_DIR:-}" || -n "${KITTY_WINDOW_ID:-}" || -n "${WT_SESSION:-}" || -n "${KONSOLE_VERSION:-}" || -n "${VTE_VERSION:-}" ]]
}

# Helper: detect terminals supporting DEC Mode 2026 (Synchronized Output)
_zsh_dropdown_supports_sync() {
  case "${ZSH_DROPDOWN_SYNC_REDRAW:-auto}" in
    always) return 0 ;;
    never)  return 1 ;;
  esac

  # Skip Mode 2026 inside tmux or GNU screen in auto mode
  [[ -n "${TMUX:-}" || -n "${STY:-}" ]] && return 1

  case "${TERM:-}" in
    *ghostty*|*kitty*|*wezterm*|*alacritty*|*foot*|*contour*|*rio*) return 0 ;;
  esac
  case "${TERM_PROGRAM:-}" in
    ghostty|WezTerm|iTerm.app|kitty|Alacritty|Rio|vscode|WarpTerminal|Tabby|Hyper) return 0 ;;
  esac
  if [[ -n "${GHOSTTY_RESOURCES_DIR:-}" || -n "${KITTY_WINDOW_ID:-}" || -n "${WT_SESSION:-}" ]]; then
    return 0
  fi
  if [[ "${KONSOLE_VERSION:-0}" == <-> ]] && (( KONSOLE_VERSION >= 230800 )); then
    return 0
  fi
  if [[ "${VTE_VERSION:-0}" == <-> ]] && (( VTE_VERSION >= 7600 )); then
    return 0
  fi
  return 1
}

() {
  # Defaults (override in ~/.zshrc before sourcing Oh My Zsh / this plugin)
  : "${ZSH_DROPDOWN_THEME:=github-dark}"      # github-dark | catppuccin | tokyonight | none
  : "${ZSH_DROPDOWN_MODE:=prefix}"            # prefix | fuzzy
  : "${ZSH_DROPDOWN_MIN_INPUT:=1}"            # minimum chars before showing suggestions
  : "${ZSH_DROPDOWN_DELAY:=0.05}"             # seconds to wait after last keystroke
  : "${ZSH_DROPDOWN_LIST_LINES:=10}"          # max dropdown height in lines
  : "${ZSH_DROPDOWN_SYNC_REDRAW:=auto}"       # auto | always | never
  : "${ZSH_DROPDOWN_TAB_WIDGET:=menu-select}" # menu-select | complete-word | none

  # 1. Ensure zasync is on fpath (prevents Zsh 5.9 silent autoload failure)
  typeset -gU fpath
  local zasync_dir="${XDG_CACHE_HOME:-$HOME/.cache}/zsh/zasync"
  if [[ -d "$zasync_dir" ]]; then
    fpath=("$zasync_dir" $fpath)
  fi

  # 2. Completion matching & responsiveness
  if [[ "$ZSH_DROPDOWN_MODE" == "prefix" ]]; then
    zstyle ':completion:*' completer _expand _complete _prefix _ignored
    zstyle ':completion:*' matcher-list 'm:{[:lower:]}={[:upper:]}'
  fi

  zstyle ':autocomplete:*' min-input "$ZSH_DROPDOWN_MIN_INPUT"
  zstyle ':autocomplete:*' delay "$ZSH_DROPDOWN_DELAY"
  zstyle ':autocomplete:*' list-lines "$ZSH_DROPDOWN_LIST_LINES"

  # 3. Built-in color themes (24-bit True Color with automatic 256-color fallback)
  local use_rgb=0
  _zsh_dropdown_supports_truecolor && use_rgb=1

  case "$ZSH_DROPDOWN_THEME" in
    github-dark)
      zstyle ':completion:*:descriptions' format '%F{242}── %F{111}%d %F{242}──%f'
      if (( use_rgb )); then
        zstyle ':completion:*:default' list-colors \
          'ma=48;2;38;79;120;1;97' \
          'di=1;38;2;88;166;255' \
          'ln=38;2;121;192;255' \
          'ex=1;38;2;63;185;80' \
          'fi=38;2;230;237;243'
      else
        zstyle ':completion:*:default' list-colors \
          'ma=48;5;24;1;97' \
          'di=1;38;5;75' \
          'ln=38;5;117' \
          'ex=1;38;5;77' \
          'fi=38;5;254'
      fi
      zstyle ':completion:*' list-separator '  '
      ;;
    catppuccin)
      zstyle ':completion:*:descriptions' format '%F{245}── %F{147}%d %F{245}──%f'
      if (( use_rgb )); then
        zstyle ':completion:*:default' list-colors \
          'ma=48;2;69;71;90;1;38;2;205;214;244' \
          'di=1;38;2;137;180;250' \
          'ln=38;2;148;226;213' \
          'ex=1;38;2;166;227;161' \
          'fi=38;2;205;214;244'
      else
        zstyle ':completion:*:default' list-colors \
          'ma=48;5;239;1;38;5;189' \
          'di=1;38;5;111' \
          'ln=38;5;116' \
          'ex=1;38;5;151' \
          'fi=38;5;189'
      fi
      zstyle ':completion:*' list-separator '  '
      ;;
    tokyonight)
      zstyle ':completion:*:descriptions' format '%F{242}── %F{117}%d %F{242}──%f'
      if (( use_rgb )); then
        zstyle ':completion:*:default' list-colors \
          'ma=48;2;40;52;87;1;38;2;192;202;245' \
          'di=1;38;2;122;162;247' \
          'ln=38;2;125;207;255' \
          'ex=1;38;2;158;206;106' \
          'fi=38;2;192;202;245'
      else
        zstyle ':completion:*:default' list-colors \
          'ma=48;5;60;1;38;5;189' \
          'di=1;38;5;111' \
          'ln=38;5;117' \
          'ex=1;38;5;149' \
          'fi=38;5;189'
      fi
      zstyle ':completion:*' list-separator '  '
      ;;
    none)
      ;;
  esac
}

# Deferred initialization (runs once on first precmd, right after zsh-autocomplete)
_zsh_dropdown_init() {
  precmd_functions=(${precmd_functions:#_zsh_dropdown_init})

  # Self-heal zasync if zsh-autocomplete left an undefined pathless stub on Zsh 5.9
  local zasync_dir="${XDG_CACHE_HOME:-$HOME/.cache}/zsh/zasync"
  if [[ -f "$zasync_dir/zasync" ]]; then
    if [[ "${functions[zasync]:-}" == *"builtin autoload"* ]]; then
      unfunction zasync 2>/dev/null || true
      fpath=("$zasync_dir" $fpath)
      autoload -Uz zasync
    fi
  fi

  # Wrap zsh-autocomplete's redraw callback in Mode 2026 synchronized output
  # using a persistent O_CLOEXEC /dev/tty fd (zero open/close syscalls per redraw)
  if _zsh_dropdown_supports_sync && (( $+functions[.autocomplete:async:complete:callback] )); then
    typeset -gi _zsh_dropdown_tty_fd=0
    {
      zmodload zsh/system 2>/dev/null && (( $+builtins[sysopen] )) &&
        sysopen -o cloexec -wu _zsh_dropdown_tty_fd -- /dev/tty 2>/dev/null
    } || _zsh_dropdown_tty_fd=0

    functions[_zsh_dropdown_orig_cb]=$functions[.autocomplete:async:complete:callback]
    if (( _zsh_dropdown_tty_fd > 0 )); then
      .autocomplete:async:complete:callback() {
        builtin print -nu $_zsh_dropdown_tty_fd '\e[?2026h' 2>/dev/null
        { _zsh_dropdown_orig_cb "$@" } always {
          builtin print -nu $_zsh_dropdown_tty_fd '\e[?2026l' 2>/dev/null
        }
      }
    else
      .autocomplete:async:complete:callback() {
        builtin print -n '\e[?2026h' > /dev/tty 2>/dev/null
        { _zsh_dropdown_orig_cb "$@" } always {
          builtin print -n '\e[?2026l' > /dev/tty 2>/dev/null
        }
      }
    fi
  fi

  # Reclaim Tab across main/emacs/viins after tools like `fzf --zsh` while keeping Ctrl+R intact
  if [[ "${ZSH_DROPDOWN_TAB_WIDGET:-menu-select}" != "none" ]]; then
    local km
    for km in main emacs viins; do
      bindkey -M "$km" '^I' "$ZSH_DROPDOWN_TAB_WIDGET" 2>/dev/null || true
    done
  fi
}

precmd_functions+=(_zsh_dropdown_init)

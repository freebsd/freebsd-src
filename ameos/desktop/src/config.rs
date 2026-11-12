// SPDX-License-Identifier: BSD-2-Clause
//! Persistent configuration for AmeDE.
//!
//! Theme preference is stored as plain text in
//! `$XDG_CONFIG_HOME/amede/theme` (or `~/.config/amede/theme`).

use crate::scene::Theme;
use std::{fs, path::PathBuf};

fn home_dir() -> Option<PathBuf> {
    std::env::var_os("HOME")
        .or_else(|| std::env::var_os("USERPROFILE"))
        .map(PathBuf::from)
}

fn config_dir() -> Option<PathBuf> {
    let base = if let Some(dir) = std::env::var_os("XDG_CONFIG_HOME") {
        PathBuf::from(dir)
    } else {
        home_dir()?.join(".config")
    };
    Some(base.join("amede"))
}

/// Reads the saved theme. Returns `None` if the file is missing or unreadable.
pub fn load_theme() -> Option<Theme> {
    let path = config_dir()?.join("theme");
    let content = fs::read_to_string(path).ok()?;
    Theme::parse(content.trim())
}

/// Saves the current theme selection. Silently ignores failures (best-effort).
pub fn save_theme(theme: Theme) {
    let Some(dir) = config_dir() else { return };
    let _ = fs::create_dir_all(&dir);
    let _ = fs::write(dir.join("theme"), theme.name());
}

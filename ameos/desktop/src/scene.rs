// SPDX-License-Identifier: BSD-2-Clause
use ame_gfx::{Canvas, Color, Rect};
use fontdue::{Font, FontSettings};
use std::{error::Error, path::PathBuf};

pub fn pixel_count(width: u32, height: u32) -> Result<usize, &'static str> {
    width
        .checked_mul(height)
        .filter(|&size| (1..=16_777_216).contains(&size))
        .map(|size| size as usize)
        .ok_or("unsupported frame dimensions")
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Theme {
    Amaterasu,
    Sakura,
    Oled,
}

impl Theme {
    pub const ALL: [Self; 3] = [Self::Amaterasu, Self::Sakura, Self::Oled];

    pub fn parse(name: &str) -> Option<Self> {
        Self::ALL
            .into_iter()
            .find(|theme| theme.name().eq_ignore_ascii_case(name))
    }

    fn name(self) -> &'static str {
        match self {
            Self::Amaterasu => "Amaterasu",
            Self::Sakura => "Sakura",
            Self::Oled => "OLED",
        }
    }

    pub fn accent(self) -> Color {
        hex(match self {
            Self::Amaterasu => 0xbea5ff,
            Self::Sakura => 0xffb1c8,
            Self::Oled => 0xc5cfdd,
        })
    }
}

pub const fn hex(value: u32) -> Color {
    Color::rgb((value >> 16) as u8, (value >> 8) as u8, value as u8)
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Action {
    Theme(Theme),
    Launcher,
    Terminal,
}

#[derive(Clone, Copy)]
pub struct Layout {
    scale: f64,
    width: i32,
    height: i32,
}

impl Layout {
    pub fn new(width: u32, height: u32) -> Self {
        let scale = (f64::from(width) / 1100.0).min(f64::from(height) / 700.0);
        Self {
            scale,
            width: (f64::from(width) / scale) as i32,
            height: (f64::from(height) / scale) as i32,
        }
    }

    fn pixel(self, value: i32) -> i32 {
        (f64::from(value) * self.scale).round() as i32
    }

    fn rect(self, rect: Rect) -> Rect {
        let x = self.pixel(rect.x);
        let y = self.pixel(rect.y);
        Rect::new(
            x,
            y,
            (self.pixel(rect.x + rect.width as i32) - x) as u32,
            (self.pixel(rect.y + rect.height as i32) - y) as u32,
        )
    }

    fn theme_button(self, index: usize) -> Rect {
        Rect::new(self.width - 322 + 97 * index as i32, 24, 91, 34)
    }

    fn dock_button(self, index: i32) -> Rect {
        Rect::new(
            self.width / 2 - 159 + index * 158,
            self.height - 82,
            152,
            52,
        )
    }

    fn launcher(self) -> Rect {
        Rect::new(self.width / 2 - 166, self.height - 205, 332, 100)
    }

    fn launcher_terminal(self) -> Rect {
        let r = self.launcher();
        Rect::new(r.x + 12, r.y + 40, r.width - 24, 46)
    }

    #[cfg(feature = "window")]
    pub fn hit(self, x: f64, y: f64, launcher: bool) -> Option<Action> {
        let x = x.floor() as i32;
        let y = y.floor() as i32;
        for (index, theme) in Theme::ALL.into_iter().enumerate() {
            if self.rect(self.theme_button(index)).contains(x, y) {
                return Some(Action::Theme(theme));
            }
        }
        if self.rect(self.dock_button(0)).contains(x, y) {
            Some(Action::Launcher)
        } else if self.rect(self.dock_button(1)).contains(x, y)
            || launcher && self.rect(self.launcher_terminal()).contains(x, y)
        {
            Some(Action::Terminal)
        } else {
            None
        }
    }
}

pub struct Scene {
    pub theme: Theme,
    pub launcher: bool,
    pub launch_failed: bool,
    font: Font,
    background: Vec<u32>,
    background_key: Option<(u32, u32, Theme)>,
}

impl Scene {
    pub fn new(theme: Theme) -> Result<Self, Box<dyn Error>> {
        let font = Font::from_bytes(load_font()?, FontSettings::default())?;
        Ok(Self {
            theme,
            launcher: false,
            launch_failed: false,
            font,
            background: Vec::new(),
            background_key: None,
        })
    }

    pub fn draw(
        &mut self,
        width: u32,
        height: u32,
        pixels: &mut [u32],
        hover: Option<Action>,
    ) -> Result<(), Box<dyn Error>> {
        let len = pixel_count(width, height)?;
        let pixels = pixels.get_mut(..len).ok_or("short window buffer")?;
        let key = (width, height, self.theme);
        if self.background_key != Some(key) {
            self.background.resize(len, 0);
            crate::wallpaper::draw(self.theme, width, height, &mut self.background);
            self.background_key = Some(key);
        }
        pixels.copy_from_slice(&self.background);
        let layout = Layout::new(width, height);
        let mut frame = Frame {
            canvas: Canvas::new(pixels, width, height, width as usize)?,
            layout,
            font: &self.font,
        };
        let fg = hex(0xf8f4ff);
        let muted = hex(0xc7bed5);
        let accent = self.theme.accent();
        let panel = Color::rgba(18, 14, 28, 224);
        frame.rounded(Rect::new(22, 16, (layout.width - 44) as u32, 50), 14, panel);
        frame.rounded(Rect::new(36, 28, 26, 26), 8, accent);
        frame.rounded(Rect::new(42, 35, 14, 12), 4, hex(0x272036));
        frame.rect(Rect::new(47, 37, 4, 8), accent);
        frame.text(74, 47, 20.0, fg, "AmeDE");

        for (index, theme) in Theme::ALL.into_iter().enumerate() {
            let r = layout.theme_button(index);
            let selected = theme == self.theme;
            if selected || hover == Some(Action::Theme(theme)) {
                frame.rounded(
                    r,
                    8,
                    Color {
                        a: if selected { 53 } else { 28 },
                        ..accent
                    },
                );
            }
            frame.rounded(Rect::new(r.x + 9, r.y + 13, 8, 8), 4, theme.accent());
            frame.text(
                r.x + 22,
                r.y + 22,
                12.0,
                if selected { fg } else { muted },
                theme.name(),
            );
        }

        let left = layout.dock_button(0);
        frame.rounded(Rect::new(left.x - 7, left.y - 7, 324, 66), 17, panel);
        for (index, action, label) in [
            (0, Action::Launcher, "Приложения"),
            (1, Action::Terminal, "Терминал"),
        ] {
            let r = layout.dock_button(index);
            if hover == Some(action) || index == 0 && self.launcher {
                frame.rounded(r, 11, Color { a: 44, ..accent });
            }
            frame.rounded(Rect::new(r.x + 10, r.y + 10, 32, 32), 9, hex(0x382b4c));
            if index == 0 {
                for y in [0, 7] {
                    for x in [0, 7] {
                        frame.rounded(Rect::new(r.x + 20 + x, r.y + 20 + y, 5, 5), 1, accent);
                    }
                }
            } else {
                frame.text(r.x + 16, r.y + 32, 15.0, accent, ">_");
            }
            frame.text(r.x + 51, r.y + 32, 15.0, fg, label);
        }

        if self.launcher {
            let r = layout.launcher();
            frame.rounded(r, 16, panel);
            frame.text(r.x + 19, r.y + 29, 16.0, fg, "Приложения");
            let item = layout.launcher_terminal();
            frame.rounded(item, 9, Color { a: 44, ..accent });
            frame.text(item.x + 12, item.y + 29, 16.0, fg, "Открыть терминал");
            frame.text(item.x + 282, item.y + 29, 14.0, accent, "T");
        }
        if self.launch_failed {
            let r = Rect::new(
                layout.width / 2 - 166,
                self.height_for_notice(layout),
                332,
                38,
            );
            frame.rounded(r, 9, hex(0x54283d));
            frame.text(
                r.x + 14,
                r.y + 25,
                15.0,
                fg,
                "Не удалось запустить терминал",
            );
        }
        Ok(())
    }

    fn height_for_notice(&self, layout: Layout) -> i32 {
        layout.height - if self.launcher { 251 } else { 137 }
    }
}

struct Frame<'a> {
    canvas: Canvas<'a>,
    layout: Layout,
    font: &'a Font,
}

impl Frame<'_> {
    fn rect(&mut self, rect: Rect, color: Color) {
        self.canvas.rect(self.layout.rect(rect), color);
    }

    fn rounded(&mut self, rect: Rect, radius: i32, color: Color) {
        let r = self.layout.rect(rect);
        let radius = self
            .layout
            .pixel(radius)
            .min(r.width as i32 / 2)
            .min(r.height as i32 / 2);
        let height = r.height as i32;
        self.canvas.rect(
            Rect::new(r.x, r.y + radius, r.width, (height - radius * 2) as u32),
            color,
        );
        for row in 0..radius {
            let dy = f64::from(radius - row) - 0.5;
            let inset = radius - (f64::from(radius).powi(2) - dy * dy).sqrt().round() as i32;
            let width = r.width - 2 * inset as u32;
            self.canvas
                .rect(Rect::new(r.x + inset, r.y + row, width, 1), color);
            self.canvas.rect(
                Rect::new(r.x + inset, r.y + height - 1 - row, width, 1),
                color,
            );
        }
    }

    fn text(&mut self, x: i32, baseline: i32, size: f32, color: Color, value: &str) {
        let mut cursor = self.layout.pixel(x) as f32;
        let baseline = self.layout.pixel(baseline);
        for glyph in value.chars() {
            let (metrics, mask) = self.font.rasterize(glyph, size * self.layout.scale as f32);
            let r = Rect::new(
                cursor.round() as i32 + metrics.xmin,
                baseline - metrics.ymin - metrics.height as i32,
                metrics.width as u32,
                metrics.height as u32,
            );
            self.canvas
                .mask(r, color, &mask, metrics.width)
                .expect("font coverage dimensions");
            cursor += metrics.advance_width;
        }
    }
}

fn load_font() -> Result<Vec<u8>, Box<dyn Error>> {
    if let Some(path) = std::env::var_os("AMEDE_FONT") {
        return Ok(std::fs::read(PathBuf::from(path))?);
    }
    for path in [
        "C:/Windows/Fonts/segoeui.ttf",
        "/usr/local/share/fonts/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
    ] {
        if let Ok(bytes) = std::fs::read(path) {
            return Ok(bytes);
        }
    }
    Err("no UI font found; set AMEDE_FONT to a TrueType font with Cyrillic glyphs".into())
}

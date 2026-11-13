// SPDX-License-Identifier: BSD-2-Clause
use crate::scene::{hex, Theme};
use ame_gfx::Color;

pub fn draw(theme: Theme, width: u32, height: u32, buffer: &mut [u32]) {
    let (sky, horizon, sun, ridges) = match theme {
        Theme::Amaterasu => (
            0x100f23,
            0x56366d,
            0xf9c9b9,
            [0x50386c, 0x2f254b, 0x1d1b35, 0x121425],
        ),
        Theme::Sakura => (
            0x211328,
            0x824965,
            0xffd9bb,
            [0x744660, 0x56344f, 0x36233d, 0x211728],
        ),
        Theme::Oled => (
            0x000000,
            0x151820,
            0x676d82,
            [0x11131c, 0x0b0d14, 0x06070b, 0x000000],
        ),
    };
    let width_px = width as f32;
    let height_px = height as f32;
    let aspect = width_px / height_px;
    let hills: Vec<_> = (0..width)
        .map(|x| {
            let t = x as f32 / width_px;
            [
                0.63 + 0.055 * (10.0 * t + 0.9).sin() + 0.021 * (25.0 * t).sin(),
                0.72 + 0.060 * (9.0 * t + 2.3).sin() + 0.016 * (31.0 * t).sin(),
                0.81 + 0.066 * (7.0 * t + 1.2).sin() + 0.023 * (20.0 * t + 1.0).sin(),
                0.93 + 0.051 * (8.0 * t + 3.0).sin() + 0.013 * (39.0 * t).sin(),
            ]
        })
        .collect();
    for (y, row) in buffer.chunks_exact_mut(width as usize).enumerate() {
        let t = y as f32 / height_px;
        let base = mix(hex(sky), hex(horizon), (t * 1.4).clamp(0.0, 1.0));
        let dy = t - 0.425;
        for (x, pixel) in row.iter_mut().enumerate() {
            let dx = (x as f32 / width_px - 0.69) * aspect;
            let distance = (dx * dx + dy * dy).sqrt();
            let glow = 0.12 / (1.0 + 19.0 * distance * distance);
            let mut color = mix(base, hex(sun), glow);
            let disc = ((0.116 - distance) * height_px + 0.5).clamp(0.0, 1.0);
            color = mix(color, mix(hex(sun), hex(horizon), (dy + 0.116) * 1.8), disc);
            let ring = (1.0 - (distance - 0.154).abs() * height_px).max(0.0) * 0.22;
            color = mix(color, theme.accent(), ring);
            let seed = (x as u32).wrapping_mul(0x9e37_79b9) ^ (y as u32).wrapping_mul(0x85eb_ca6b);
            if t < 0.53 && distance > 0.16 && seed % 2473 == 0 {
                color = mix(color, theme.accent(), 0.42);
            }
            for (edge, ridge) in hills[x].into_iter().zip(ridges) {
                let coverage = ((t - edge) * height_px + 0.5).clamp(0.0, 1.0);
                color = mix(color, hex(ridge), coverage);
            }
            *pixel = color.xrgb();
        }
    }
}

fn mix(left: Color, right: Color, amount: f32) -> Color {
    let channel = |a: u8, b: u8| (f32::from(a) + (f32::from(b) - f32::from(a)) * amount) as u8;
    Color::rgb(
        channel(left.r, right.r),
        channel(left.g, right.g),
        channel(left.b, right.b),
    )
}

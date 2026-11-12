// SPDX-License-Identifier: BSD-2-Clause
//! Software drawing into a borrowed XRGB8888 buffer.

mod blend;

/// A color with straight (not premultiplied) alpha.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Color {
    pub r: u8,
    pub g: u8,
    pub b: u8,
    pub a: u8,
}

impl Color {
    pub const fn rgb(r: u8, g: u8, b: u8) -> Self {
        Self::rgba(r, g, b, 255)
    }

    pub const fn rgba(r: u8, g: u8, b: u8, a: u8) -> Self {
        Self { r, g, b, a }
    }

    pub const fn xrgb(self) -> u32 {
        (self.r as u32) << 16 | (self.g as u32) << 8 | self.b as u32
    }
}

/// Half-open rectangle in pixels. Origins may be outside the canvas.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Rect {
    pub x: i32,
    pub y: i32,
    pub width: u32,
    pub height: u32,
}

impl Rect {
    pub const fn new(x: i32, y: i32, width: u32, height: u32) -> Self {
        Self {
            x,
            y,
            width,
            height,
        }
    }

    pub fn contains(self, x: i32, y: i32) -> bool {
        let dx = i64::from(x) - i64::from(self.x);
        let dy = i64::from(y) - i64::from(self.y);
        (0..i64::from(self.width)).contains(&dx) && (0..i64::from(self.height)).contains(&dy)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct InvalidBuffer;

impl std::fmt::Display for InvalidBuffer {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("buffer size or stride does not match its dimensions")
    }
}

impl std::error::Error for InvalidBuffer {}

/// An opaque pixel canvas. Stride is measured in pixels; row padding is preserved.
pub struct Canvas<'a> {
    pixels: &'a mut [u32],
    width: u32,
    height: u32,
    stride: usize,
}

impl<'a> Canvas<'a> {
    pub fn new(
        pixels: &'a mut [u32],
        width: u32,
        height: u32,
        stride: usize,
    ) -> Result<Self, InvalidBuffer> {
        let needed = buffer_len(width, height, stride).ok_or(InvalidBuffer)?;
        let pixels = pixels.get_mut(..needed).ok_or(InvalidBuffer)?;
        Ok(Self {
            pixels,
            width,
            height,
            stride,
        })
    }

    pub fn size(&self) -> (u32, u32) {
        (self.width, self.height)
    }

    /// Composites a solid color over the rectangle with clipping.
    pub fn rect(&mut self, rect: Rect, color: Color) {
        if color.a == 0 {
            return;
        }
        let Some((left, top, right, bottom)) = self.clip(rect) else {
            return;
        };
        if color.a == 255 {
            for y in top..bottom {
                self.pixels[y * self.stride + left..y * self.stride + right].fill(color.xrgb());
            }
        } else {
            let blender = blend::span_blender(right - left);
            for y in top..bottom {
                blender(
                    &mut self.pixels[y * self.stride + left..y * self.stride + right],
                    color,
                );
            }
        }
    }

    /// Composites an 8-bit coverage mask, such as a rasterized font glyph.
    /// The mask's dimensions and position are given by `rect`.
    pub fn mask(
        &mut self,
        rect: Rect,
        color: Color,
        coverage: &[u8],
        mask_stride: usize,
    ) -> Result<(), InvalidBuffer> {
        let needed = buffer_len(rect.width, rect.height, mask_stride).ok_or(InvalidBuffer)?;
        if coverage.len() < needed {
            return Err(InvalidBuffer);
        }
        let Some((left, top, right, bottom)) = self.clip(rect) else {
            return Ok(());
        };
        let mask_x = (left as i64 - i64::from(rect.x)) as usize;
        let mask_y = (top as i64 - i64::from(rect.y)) as usize;
        for (row, y) in (top..bottom).enumerate() {
            let start = (mask_y + row) * mask_stride + mask_x;
            let samples = &coverage[start..start + right - left];
            let pixels = &mut self.pixels[y * self.stride + left..y * self.stride + right];
            for (pixel, &sample) in pixels.iter_mut().zip(samples) {
                let alpha = blend::div255(u16::from(sample) * u16::from(color.a)) as u8;
                if alpha != 0 {
                    *pixel = blend::pixel(*pixel, Color { a: alpha, ..color });
                }
            }
        }
        Ok(())
    }

    fn clip(&self, r: Rect) -> Option<(usize, usize, usize, usize)> {
        let x = i64::from(r.x);
        let y = i64::from(r.y);
        let left = x.max(0);
        let top = y.max(0);
        let right = (x + i64::from(r.width)).min(i64::from(self.width));
        let bottom = (y + i64::from(r.height)).min(i64::from(self.height));
        (left < right && top < bottom)
            .then(|| (left as usize, top as usize, right as usize, bottom as usize))
    }
}

fn buffer_len(width: u32, height: u32, stride: usize) -> Option<usize> {
    let width = width as usize;
    if stride < width {
        return None;
    }
    if width == 0 || height == 0 {
        return Some(0);
    }
    ((height - 1) as usize)
        .checked_mul(stride)?
        .checked_add(width)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn clips_rectangles_and_preserves_padding() {
        let mut pixels = [0x123456; 10];
        let mut canvas = Canvas::new(&mut pixels, 3, 2, 5).unwrap();
        canvas.rect(Rect::new(-1, -1, 3, 3), Color::rgb(255, 0, 0));
        canvas.rect(
            Rect::new(i32::MIN, 0, 1, u32::MAX),
            Color::rgb(255, 255, 255),
        );
        assert_eq!(
            pixels,
            [
                0xff0000, 0xff0000, 0x123456, 0x123456, 0x123456, 0xff0000, 0xff0000, 0x123456,
                0x123456, 0x123456
            ]
        );
    }

    #[test]
    fn clips_masks_with_source_offset_and_rejects_truncation() {
        let mut pixels = [0; 2];
        let mut canvas = Canvas::new(&mut pixels, 2, 1, 2).unwrap();
        let r = Rect::new(-1, 0, 3, 1);
        assert!(canvas
            .mask(r, Color::rgb(255, 255, 255), &[0, 128], 3)
            .is_err());
        canvas
            .mask(r, Color::rgb(255, 255, 255), &[0, 128, 255], 3)
            .unwrap();
        assert_eq!(pixels, [0x808080, 0xffffff]);
        assert!(Canvas::new(&mut pixels, 2, 2, usize::MAX).is_err());
    }
}

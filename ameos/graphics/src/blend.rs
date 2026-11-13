// SPDX-License-Identifier: BSD-2-Clause
use crate::Color;

#[inline]
pub(crate) fn div255(value: u16) -> u16 {
    // Exact rounded division over 0..=65025.
    let biased = value + 128;
    (biased + (biased >> 8)) >> 8
}

#[inline]
pub(crate) fn pixel(dst: u32, color: Color) -> u32 {
    let alpha = u16::from(color.a);
    let inverse = 255 - alpha;
    let channel = |shift, src| {
        let value = ((dst >> shift) & 255u32) as u16;
        u32::from(div255(value * inverse + u16::from(src) * alpha))
    };
    (channel(16, color.r) << 16) | (channel(8, color.g) << 8) | channel(0, color.b)
}

fn rust_span(dst: &mut [u32], color: Color) {
    for value in dst {
        *value = pixel(*value, color);
    }
}

pub(crate) fn span_blender(width: usize) -> fn(&mut [u32], Color) {
    #[cfg(target_arch = "x86_64")]
    if width >= 64 {
        return sse2_span;
    }
    let _ = width;
    rust_span
}

#[cfg(target_arch = "x86_64")]
fn sse2_span(dst: &mut [u32], color: Color) {
    let vector_len = dst.len() / 4 * 4;
    let (vectors, tail) = dst.split_at_mut(vector_len);
    if !vectors.is_empty() {
        let alpha = u16::from(color.a);
        let inverse = u32::from(255 - alpha) * 0x0001_0001;
        let premultiplied = [
            u16::from(color.b) * alpha,
            u16::from(color.g) * alpha,
            u16::from(color.r) * alpha,
            0,
        ];
        let bias = 0x0080_0080u32;
        let mask = 0x00ff_ffffu32;
        // SAFETY: SSE2 is part of the x86-64 baseline. The loop reads/writes exactly
        // vectors.len() pixels using unaligned 16-byte accesses. The constants
        // live through the asm block and the mutable slice is exclusive.
        unsafe {
            std::arch::asm!(
                include_str!("blend_sse2.asm"),
                dst = inout(reg) vectors.as_mut_ptr() => _,
                blocks = inout(reg) vectors.len() / 4 => _,
                src = in(reg) premultiplied.as_ptr(),
                inv = in(reg) &inverse,
                bias = in(reg) &bias,
                mask = in(reg) &mask,
                out("xmm0") _, out("xmm1") _, out("xmm2") _, out("xmm3") _,
                out("xmm4") _, out("xmm5") _, out("xmm6") _, out("xmm7") _,
                options(nostack),
            );
        }
    }
    rust_span(tail, color);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn rounding_matches_integer_reference() {
        for x in 0..=65025u16 {
            assert_eq!(div255(x), (x + 127) / 255);
        }
    }

    #[test]
    fn dispatch_matches_rust_for_unaligned_rows_and_tails() {
        for len in [1, 7, 8, 63, 64, 65, 1025] {
            for alpha in [1, 17, 127, 128, 254] {
                let mut got: Vec<_> = (0..len + 2)
                    .map(|i| (i as u32).wrapping_mul(0x91d28b))
                    .collect();
                let mut expected = got.clone();
                let color = Color::rgba(25, 200, 145, alpha);
                span_blender(len)(&mut got[1..=len], color);
                rust_span(&mut expected[1..=len], color);
                assert_eq!(got, expected);
            }
        }
    }

    #[test]
    #[ignore = "opt-in performance measurement; run in release mode"]
    fn benchmark_translucent_span() {
        use std::{hint::black_box, time::Instant};
        let count = 1920 * 1080;
        let color = Color::rgba(77, 43, 120, 143);
        for (name, blend) in [
            ("Rust", rust_span as fn(&mut [u32], Color)),
            ("auto", span_blender(count)),
        ] {
            let mut dst = vec![0x183460; count];
            let start = Instant::now();
            for _ in 0..100 {
                blend(black_box(&mut dst), black_box(color));
            }
            println!(
                "{name}: {:.3} ms/frame",
                start.elapsed().as_secs_f64() * 10.0
            );
        }
    }
}

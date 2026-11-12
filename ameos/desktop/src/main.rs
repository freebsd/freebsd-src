// SPDX-License-Identifier: BSD-2-Clause
mod config;
mod scene;
mod wallpaper;
#[cfg(feature = "window")]
mod window;

use scene::{Scene, Theme};
use std::{
    error::Error,
    fs::File,
    io::BufWriter,
    path::{Path, PathBuf},
};

fn main() {
    if let Err(error) = run() {
        eprintln!("AmeDE: {error}");
        std::process::exit(1);
    }
}

fn run() -> Result<(), Box<dyn Error>> {
    let mut theme = config::load_theme().unwrap_or(Theme::Amaterasu);
    let mut preview = None;
    let mut args = std::env::args_os().skip(1);
    while let Some(arg) = args.next() {
        match arg.to_str() {
            Some("--theme") => {
                let value = args.next().ok_or("--theme requires a name")?;
                theme = Theme::parse(value.to_str().unwrap_or_default())
                    .ok_or("expected amaterasu, sakura or oled")?;
            }
            Some("--preview") => {
                preview = Some(PathBuf::from(
                    args.next().ok_or("--preview requires a PNG path")?,
                ));
            }
            Some("--help" | "-h") => {
                println!("Usage: amede [--theme amaterasu|sakura|oled] [--preview PATH.png]");
                return Ok(());
            }
            _ => return Err(format!("unknown argument {}", arg.to_string_lossy()).into()),
        }
    }
    let mut scene = Scene::new(theme)?;
    if let Some(path) = preview {
        return save_preview(&mut scene, &path);
    }
    #[cfg(feature = "window")]
    return window::run(scene);
    #[cfg(not(feature = "window"))]
    Err("this build requires --preview PATH.png".into())
}

fn save_preview(scene: &mut Scene, path: &Path) -> Result<(), Box<dyn Error>> {
    let (width, height) = (1100, 700);
    let mut pixels = vec![0; scene::pixel_count(width, height)?];
    scene.draw(width, height, &mut pixels, None)?;
    let file = BufWriter::new(File::create(path)?);
    let mut encoder = png::Encoder::new(file, width, height);
    encoder.set_color(png::ColorType::Rgb);
    encoder.set_depth(png::BitDepth::Eight);
    let rgb: Vec<_> = pixels
        .iter()
        .flat_map(|&p| [(p >> 16) as u8, (p >> 8) as u8, p as u8])
        .collect();
    encoder.write_header()?.write_image_data(&rgb)?;
    Ok(())
}

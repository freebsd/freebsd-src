// SPDX-License-Identifier: BSD-2-Clause
use crate::scene::{pixel_count, Action, Layout, Scene, Theme};
use std::{error::Error, num::NonZeroU32, process::Command, rc::Rc};
use winit::{
    application::ApplicationHandler,
    dpi::{LogicalSize, PhysicalPosition},
    event::{ElementState, MouseButton, WindowEvent},
    event_loop::{ActiveEventLoop, EventLoop},
    keyboard::{Key, NamedKey},
    window::{Window, WindowId},
};

pub fn run(scene: Scene) -> Result<(), Box<dyn Error>> {
    let event_loop = EventLoop::new()?;
    let mut app = Desktop {
        scene,
        view: None,
        pointer: None,
        error: None,
    };
    event_loop.run_app(&mut app)?;
    match app.error {
        Some(error) => Err(error),
        None => Ok(()),
    }
}

struct Desktop {
    scene: Scene,
    view: Option<View>,
    pointer: Option<PhysicalPosition<f64>>,
    error: Option<Box<dyn Error>>,
}

struct View {
    window: Rc<Window>,
    surface: softbuffer::Surface<Rc<Window>, Rc<Window>>,
}

impl View {
    fn new(event_loop: &ActiveEventLoop) -> Result<Self, Box<dyn Error>> {
        let window = Rc::new(
            event_loop.create_window(
                Window::default_attributes()
                    .with_title("AmeDE")
                    .with_inner_size(LogicalSize::new(1100.0, 700.0))
                    .with_min_inner_size(LogicalSize::new(640.0, 420.0)),
            )?,
        );
        let context = softbuffer::Context::new(Rc::clone(&window))?;
        let surface = softbuffer::Surface::new(&context, Rc::clone(&window))?;
        window.request_redraw();
        Ok(Self { window, surface })
    }

    fn action_at(&self, pointer: Option<PhysicalPosition<f64>>, launcher: bool) -> Option<Action> {
        let point = pointer?;
        let size = self.window.inner_size();
        pixel_count(size.width, size.height).ok()?;
        Layout::new(size.width, size.height).hit(point.x, point.y, launcher)
    }

    fn redraw(
        &mut self,
        scene: &mut Scene,
        pointer: Option<PhysicalPosition<f64>>,
    ) -> Result<(), Box<dyn Error>> {
        let size = self.window.inner_size();
        let (Some(width), Some(height)) =
            (NonZeroU32::new(size.width), NonZeroU32::new(size.height))
        else {
            return Ok(());
        };
        pixel_count(size.width, size.height)?;
        let hover = self.action_at(pointer, scene.launcher);
        self.surface.resize(width, height)?;
        let mut buffer = self.surface.buffer_mut()?;
        scene.draw(size.width, size.height, &mut buffer, hover)?;
        self.window.pre_present_notify();
        buffer.present()?;
        Ok(())
    }
}

impl Desktop {
    fn activate(&mut self, action: Option<Action>) {
        self.scene.launch_failed = false;
        match action {
            Some(Action::Theme(theme)) => self.scene.theme = theme,
            Some(Action::Launcher) => self.scene.launcher = !self.scene.launcher,
            Some(Action::Terminal) => {
                self.scene.launcher = false;
                if let Err(error) = launch_terminal() {
                    eprintln!("AmeDE: terminal: {error}");
                    self.scene.launch_failed = true;
                }
            }
            None => self.scene.launcher = false,
        }
        if let Some(view) = &self.view {
            view.window.request_redraw();
        }
    }
}

impl ApplicationHandler for Desktop {
    fn resumed(&mut self, event_loop: &ActiveEventLoop) {
        if self.view.is_none() {
            match View::new(event_loop) {
                Ok(view) => self.view = Some(view),
                Err(error) => {
                    self.error = Some(error);
                    event_loop.exit();
                }
            }
        }
    }

    fn window_event(&mut self, event_loop: &ActiveEventLoop, id: WindowId, event: WindowEvent) {
        let Some(view) = &mut self.view else { return };
        if view.window.id() != id {
            return;
        }
        match event {
            WindowEvent::CloseRequested => event_loop.exit(),
            WindowEvent::Resized(_) | WindowEvent::ScaleFactorChanged { .. } => {
                view.window.request_redraw()
            }
            WindowEvent::RedrawRequested => {
                if let Err(error) = view.redraw(&mut self.scene, self.pointer) {
                    self.error = Some(error);
                    event_loop.exit();
                }
            }
            WindowEvent::CursorMoved { position, .. } => {
                let previous = view.action_at(self.pointer, self.scene.launcher);
                self.pointer = Some(position);
                if previous != view.action_at(self.pointer, self.scene.launcher) {
                    view.window.request_redraw();
                }
            }
            WindowEvent::CursorLeft { .. } => {
                self.pointer = None;
                view.window.request_redraw();
            }
            WindowEvent::MouseInput {
                state: ElementState::Pressed,
                button: MouseButton::Left,
                ..
            } => {
                let action = view.action_at(self.pointer, self.scene.launcher);
                self.activate(action);
            }
            WindowEvent::KeyboardInput { event, .. }
                if event.state == ElementState::Pressed && !event.repeat =>
            {
                match event.logical_key.as_ref() {
                    Key::Named(NamedKey::Escape) => self.activate(None),
                    Key::Named(NamedKey::F2) => self.activate(Some(Action::Launcher)),
                    Key::Character("1") => self.activate(Some(Action::Theme(Theme::Amaterasu))),
                    Key::Character("2") => self.activate(Some(Action::Theme(Theme::Sakura))),
                    Key::Character("3") => self.activate(Some(Action::Theme(Theme::Oled))),
                    Key::Character("t" | "T") => self.activate(Some(Action::Terminal)),
                    _ => {}
                }
            }
            _ => {}
        }
    }
}

fn launch_terminal() -> std::io::Result<()> {
    let default = if cfg!(windows) {
        "cmd.exe"
    } else if std::env::var_os("WAYLAND_DISPLAY").is_some() {
        "foot"
    } else {
        "xterm"
    };
    let terminal = std::env::var_os("AMEDE_TERMINAL").unwrap_or_else(|| default.into());
    let mut command = Command::new(terminal);
    #[cfg(windows)]
    {
        use std::os::windows::process::CommandExt;
        command.creation_flags(0x10); // CREATE_NEW_CONSOLE
    }
    let mut child = command.spawn()?;
    std::thread::spawn(move || {
        let _ = child.wait();
    });
    Ok(())
}

mod backend;
mod menu;
mod screen;

use backend::{BootBackend, StubBackend};
use crossterm::{
    cursor::{Hide, Show},
    event::{self, Event, KeyEvent, KeyEventKind},
    execute,
    terminal::{self, EnterAlternateScreen, LeaveAlternateScreen},
};
use menu::Menu;
use screen::{Action, Screen};
use std::io;

struct TerminalGuard;

impl TerminalGuard {
    fn enter() -> io::Result<Self> {
        terminal::enable_raw_mode()?;
        execute!(io::stdout(), EnterAlternateScreen, Hide)?;
        Ok(Self)
    }
}

impl Drop for TerminalGuard {
    fn drop(&mut self) {
        let _ = terminal::disable_raw_mode();
        let _ = execute!(io::stdout(), Show, LeaveAlternateScreen);
    }
}

fn run_screen(screen: &mut dyn Screen) -> io::Result<Action> {
    screen.draw()?;
    loop {
        let Event::Key(KeyEvent {
            code,
            kind: KeyEventKind::Press,
            ..
        }) = event::read()?
        else {
            continue;
        };
        match screen.handle_key(code) {
            Action::Continue => screen.draw()?,
            action => return Ok(action),
        }
    }
}

fn main() -> io::Result<()> {
    let _terminal = TerminalGuard::enter()?;
    let backend = StubBackend;
    let main_menu = Menu::new(
        "AmeOS Loader",
        vec![
            "Boot AmeOS".to_owned(),
            "Boot profiles".to_owned(),
            "Boot recovery environment".to_owned(),
            "Hardware diagnostics".to_owned(),
            "Power off".to_owned(),
        ],
    );

    let mut stack: Vec<Box<dyn Screen>> = vec![Box::new(main_menu)];

    'main: while let Some(mut current) = stack.pop() {
        let action = run_screen(&mut *current)?;
        let title = current.title();
        let selected = current.selected_index().unwrap_or(0);

        match action {
            Action::Continue => unreachable!(),
            Action::Exit => break,
            Action::Back => {
                if title == "AmeOS Loader" {
                    match selected {
                        0 => {
                            drop(_terminal);
                            backend.boot_default()?;
                            break 'main;
                        }
                        1 => {
                            stack.push(current);
                            stack.push(Box::new(Menu::new(
                                "Boot Profiles",
                                vec![
                                    "Default".to_owned(),
                                    "Single user mode".to_owned(),
                                    "Safe mode".to_owned(),
                                    "Back to main menu".to_owned(),
                                ],
                            )));
                        }
                        2 => {
                            stack.push(current);
                            stack.push(Box::new(Menu::new(
                                "Recovery Environment",
                                vec![
                                    "Mount filesystems".to_owned(),
                                    "Network configuration".to_owned(),
                                    "Shell".to_owned(),
                                    "Back to main menu".to_owned(),
                                ],
                            )));
                        }
                        3 => {
                            stack.push(current);
                            stack.push(Box::new(Menu::new(
                                "Hardware Diagnostics",
                                vec![
                                    "Memory test".to_owned(),
                                    "Disk test".to_owned(),
                                    "CPU information".to_owned(),
                                    "Back to main menu".to_owned(),
                                ],
                            )));
                        }
                        4 => break 'main,
                        _ => {}
                    }
                } else if title == "Boot Profiles" {
                    match selected {
                        0 => {
                            drop(_terminal);
                            backend.boot_default()?;
                            break 'main;
                        }
                        1 => {
                            drop(_terminal);
                            backend.boot_single_user()?;
                            break 'main;
                        }
                        2 => {
                            drop(_terminal);
                            backend.boot_safe_mode()?;
                            break 'main;
                        }
                        3 => {}
                        _ => {}
                    }
                } else if title == "Recovery Environment" {
                    match selected {
                        0 => {
                            drop(_terminal);
                            backend.mount_filesystems()?;
                            break 'main;
                        }
                        1 => {
                            drop(_terminal);
                            backend.configure_network()?;
                            break 'main;
                        }
                        2 => {
                            drop(_terminal);
                            backend.spawn_shell()?;
                            break 'main;
                        }
                        3 => {}
                        _ => {}
                    }
                } else if title == "Hardware Diagnostics" {
                    match selected {
                        0 => {
                            drop(_terminal);
                            backend.run_memory_test()?;
                            break 'main;
                        }
                        1 => {
                            drop(_terminal);
                            backend.run_disk_test()?;
                            break 'main;
                        }
                        2 => {
                            drop(_terminal);
                            backend.show_cpu_info()?;
                            break 'main;
                        }
                        3 => {}
                        _ => {}
                    }
                } else if stack.is_empty() {
                    break;
                }
            }
        }
    }

    Ok(())
}

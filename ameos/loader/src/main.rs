mod backend;
mod menu;
mod screen;
mod snapshot;
mod theme;
mod zfs;

use backend::BootBackend;
use crossterm::{
    cursor::{Hide, Show},
    event::{self, Event, KeyEvent, KeyEventKind},
    execute,
    terminal::{self, EnterAlternateScreen, LeaveAlternateScreen},
};
use menu::Menu;
use screen::{Action, Screen};
use std::io;

#[cfg(target_os = "freebsd")]
use backend::FreeBSDBackend;
#[cfg(not(target_os = "freebsd"))]
use backend::StubBackend;

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

    #[cfg(target_os = "freebsd")]
    let backend = FreeBSDBackend::new();
    #[cfg(not(target_os = "freebsd"))]
    let backend = StubBackend;

    let main_menu = Menu::new(
        "AmeOS Loader",
        vec![
            "Boot AmeOS".to_owned(),
            "Boot profiles".to_owned(),
            "Boot environments (ZFS)".to_owned(),
            "Boot recovery environment".to_owned(),
            "Hardware diagnostics".to_owned(),
            "Snapshot info".to_owned(),
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
                            let environments =
                                zfs::list_boot_environments().unwrap_or_else(|_| vec![]);
                            let mut items: Vec<String> = environments
                                .iter()
                                .map(|be| {
                                    if be.active {
                                        format!("{} (active)", be.name)
                                    } else {
                                        be.name.clone()
                                    }
                                })
                                .collect();
                            items.push("Back to main menu".to_owned());
                            stack.push(Box::new(Menu::new("Boot Environments (ZFS)", items)));
                        }
                        3 => {
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
                        4 => {
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
                        5 => {
                            drop(_terminal);
                            match snapshot::SnapshotInfo::current() {
                                Ok(info) => {
                                    println!("{}", info.display());
                                    println!("\nPress Enter to continue...");
                                    let mut input = String::new();
                                    std::io::stdin().read_line(&mut input)?;
                                }
                                Err(e) => {
                                    eprintln!("Failed to get snapshot info: {}", e);
                                }
                            }
                            break 'main;
                        }
                        6 => break 'main,
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
                } else if title == "Boot Environments (ZFS)" {
                    let environments = zfs::list_boot_environments().unwrap_or_else(|_| vec![]);
                    if selected < environments.len() {
                        let be = &environments[selected];
                        drop(_terminal);
                        if let Err(e) = zfs::activate_boot_environment(&be.name) {
                            eprintln!("Failed to activate boot environment: {}", e);
                        } else {
                            println!("Boot environment '{}' activated", be.name);
                            println!("System will boot into this environment on next reboot");
                        }
                        break 'main;
                    }
                } else if stack.is_empty() {
                    break;
                }
            }
        }
    }

    Ok(())
}
